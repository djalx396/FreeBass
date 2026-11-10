// FreeBass engine: a structured model of Waves Renaissance Bass, fitted to captures (model v0).
// No JUCE dependency. All filters are bilinear-prewarped (TPT) sections defined relative to Freq,
// so they can move smoothly and behave the same at every sample rate.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace freebass
{
static constexpr double kPi = 3.14159265358979323846;

// ---- model v0 coefficients (see freebass_model_v0.json) ----------------------------------
namespace model
{
    // generator input
    static constexpr double inLpRatio = 1.212;                 // Butterworth LP3 at 1.212 x Freq
    // upward compressor (peak follower on |u|)
    static constexpr double attackMs = 12.22, releaseMs = 12.5;
    static constexpr double a1 = -0.611, a2 = -0.076, l0 = -1.73;   // log10 gain law, knee at l0
    // fundamental branch: gain * HP2 * HP2 * HP1 * LP2
    static constexpr double cGain = -11.740716568029987;
    static constexpr double c1w = 0.8714678594024661,  c1q = 1.3703418038704849;
    static constexpr double c2w = 0.41699516223981997, c2q = 1.551702012048711;
    static constexpr double c3w = 27.574048086248276;
    static constexpr double c4w = 7.178056242513452,   c4q = 0.2388569815256099;
    // harmonic branch: v = k1 c^2 + k2 c|c| + k3 |c|^3 + k4 c^3, then HP2 * LP2 * LP2
    static constexpr double k1 = 0.18538382325726338, k2 = 0.05678846189603427,
                            k3 = -0.022254386508740228, k4 = 0.0016632875283814257;
    static constexpr double n1w = 1.7267580205833082, n1q = 1.5303286712736714;
    static constexpr double n2w = 1.0097222767964396, n2q = 2.6803022958660976;
    static constexpr double n3w = 3.92923314798861,   n3q = 3.8362286667415924;
    // path limiter
    static constexpr double limCeilDb = -3.0, limReleaseMs = 45.0;
}

// ---- building blocks -----------------------------------------------------------------
inline double prewarp (double hz, double fs) { return std::tan (kPi * std::min (hz, 0.45 * fs) / fs); }

struct OnePole            // TPT one-pole: lp and hp outputs
{
    double G = 0, s = 0;
    void setG (double g) { G = g / (1.0 + g); }
    void reset() { s = 0; }
    inline double lp (double x) { const double v = (x - s) * G; const double y = v + s; s = y + v; return y; }
    inline double hp (double x) { return x - lp (x); }
};

struct Svf                // Simper SVF (trapezoidal), lp/hp outputs
{
    double a1 = 1, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
    void set (double g, double q) { k = 1.0 / q; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    void reset() { ic1 = ic2 = 0; }
    inline void tick (double v0, double& lp, double& hp)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; hp = v0 - k * v1 - v2;
    }
    inline double lp (double x) { double l, h; tick (x, l, h); return l; }
    inline double hp (double x) { double l, h; tick (x, l, h); return h; }
};

struct Butter3            // Butterworth 3rd order = one-pole + SVF (Q = 1), same cutoff
{
    OnePole p; Svf s;
    void set (double g) { p.setG (g); s.set (g, 1.0); }
    void reset() { p.reset(); s.reset(); }
    inline double lp (double x) { return s.lp (p.lp (x)); }
    inline double hp (double x) { return s.hp (p.hp (x)); }
};

// ---- telemetry for the editor ----------------------------------------------------------
struct Telemetry
{
    std::atomic<float> harmPeak { 0.f };   // linear peak of the processed bass (after Intensity + limiter)
    std::atomic<float> limGrDb  { 0.f };   // current path-limiter gain reduction (<= 0)
    std::atomic<float> outPeak  { 0.f };
    std::atomic<bool>  clipped  { false };
};

struct Settings
{
    double freqHz = 80.0;      // 32 .. 256
    double intensityDb = 0.0;  // -24 .. +24
    double gainDb = 0.0;       // -30 .. 0
    bool   origIn = false;
};

// ---- the engine ----------------------------------------------------------------------------
class Engine
{
public:
    static constexpr int kMaxCh = 2;
    static constexpr int kCoefBlock = 16;     // coefficients follow Freq every 16 samples

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aAtt = 1.0 - std::exp (-1.0 / (model::attackMs * 1e-3 * fs));
        aRel = 1.0 - std::exp (-1.0 / (model::releaseMs * 1e-3 * fs));
        aLim = 1.0 - std::exp (-1.0 / (model::limReleaseMs * 1e-3 * fs));
        aSmooth = 1.0 - std::exp (-1.0 / (0.03 * fs));            // 30 ms glide for Freq
        aGain = 1.0 - std::exp (-1.0 / (0.02 * fs));              // 20 ms for gains
        ceil = std::pow (10.0, model::limCeilDb / 20.0);
        reset();
        primed = false;
    }

    void reset()
    {
        for (int c = 0; c < kMaxCh; ++c) { hp[c].reset(); lp[c].reset(); }
        inLp.reset(); c1.reset(); c2.reset(); c3.reset(); c4.reset(); n1.reset(); n2.reset(); n3.reset();
        env = 0; limG = 1;
    }

    // In-place processing. ch = 1 or 2.
    void process (float* const* io, int ch, int n, const Settings& s, Telemetry* t = nullptr)
    {
        ch = std::clamp (ch, 1, kMaxCh);
        const double targetF = std::clamp (s.freqHz, 20.0, 400.0);
        const double tInt = std::pow (10.0, s.intensityDb / 20.0), tOut = std::pow (10.0, s.gainDb / 20.0);
        if (! primed) { curF = targetF; curInt = tInt; curOut = tOut; setCoefficients (curF); primed = true; }

        float harm = 0.f, outPk = 0.f; bool clip = false;
        for (int i = 0; i < n; ++i)
        {
            if (cnt-- <= 0)
            {
                cnt = kCoefBlock - 1;
                if (std::abs (curF - targetF) > 1e-9)
                {
                    curF = std::abs (curF - targetF) < 1e-4 ? targetF
                                                             : std::exp (std::log (curF) + aSmoothBlock() * (std::log (targetF) - std::log (curF)));
                    setCoefficients (curF);
                }
            }
            curInt += aGain * (tInt - curInt);
            curOut += aGain * (tOut - curOut);

            // generator input: mono sum
            double m = 0;
            for (int c = 0; c < ch; ++c) m += io[c][i];
            m /= ch;

            const double p = curInt * generator (m);
            const double pl = limiter (p);
            harm = std::max (harm, (float) std::abs (pl));

            for (int c = 0; c < ch; ++c)
            {
                const double x = io[c][i];
                double y = hp[c].hp (x);
                const double lo = lp[c].lp (x);             // always run, so toggling Orig is clean
                if (s.origIn) y += lo;
                y = (y + pl) * curOut;
                if (y > 1.0) { y = 1.0; clip = true; } else if (y < -1.0) { y = -1.0; clip = true; }
                io[c][i] = (float) y;
                outPk = std::max (outPk, (float) std::abs (y));
            }
        }
        if (t != nullptr)
        {
            if (harm > t->harmPeak.load()) t->harmPeak.store (harm);
            if (outPk > t->outPeak.load()) t->outPeak.store (outPk);
            t->limGrDb.store ((float) (20.0 * std::log10 (std::max (limG, 1e-6))));
            if (clip) t->clipped.store (true);
        }
    }

    // The processed-bass path on its own (mono in, path out, before Intensity). For tests.
    double generator (double m)
    {
        const double u = inLp.lp (m);
        const double a = std::abs (u);
        env += (a > env ? aAtt : aRel) * (a - env);
        const double l = std::max (std::log10 (env + 1e-6), model::l0);
        const double c = u * std::pow (10.0, model::a1 * l + model::a2 * l * l);

        // fundamental branch
        double yc = c2.hp (c1.hp (c));
        yc = c4.lp (c3.hp (yc));
        // harmonic branch
        const double ac = std::abs (c);
        const double v = model::k1 * c * c + model::k2 * c * ac + model::k3 * ac * ac * ac + model::k4 * c * c * c;
        const double yn = n3.lp (n2.lp (n1.hp (v)));
        return model::cGain * yc + yn;
    }

private:
    double aSmoothBlock() const { return 1.0 - std::pow (1.0 - aSmooth, (double) kCoefBlock); }

    double limiter (double x)
    {
        const double a = std::abs (x);
        if (a * limG > ceil) limG = ceil / a;
        else limG += aLim * (1.0 - limG);
        if (a * limG > ceil) limG = ceil / a;
        return x * limG;
    }

    void setCoefficients (double F)
    {
        const double g = prewarp (F, fs);
        for (int c = 0; c < kMaxCh; ++c) { hp[c].set (g); lp[c].set (g); }
        inLp.set (prewarp (model::inLpRatio * F, fs));
        c1.set (prewarp (model::c1w * F, fs), model::c1q);
        c2.set (prewarp (model::c2w * F, fs), model::c2q);
        c3.setG (prewarp (model::c3w * F, fs));
        c4.set (prewarp (model::c4w * F, fs), model::c4q);
        n1.set (prewarp (model::n1w * F, fs), model::n1q);
        n2.set (prewarp (model::n2w * F, fs), model::n2q);
        n3.set (prewarp (model::n3w * F, fs), model::n3q);
    }

    double fs = 48000, aAtt = 0, aRel = 0, aLim = 0, aSmooth = 0, aGain = 0, ceil = 0.7079;
    double curF = 80, curInt = 1, curOut = 1, env = 0, limG = 1;
    int cnt = 0; bool primed = false;
    Butter3 hp[kMaxCh], lp[kMaxCh], inLp;
    Svf c1, c2, c4, n1, n2, n3;
    OnePole c3;
};
} // namespace freebass
