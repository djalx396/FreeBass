// Offline test: run the FreeBass engine over a 32-bit float WAV.
//   run_engine in.wav out.wav freqHz intensityDb gainDb origIn(0/1) [blockSize]
#include "../Source/FreeBassEngine.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static bool readWav (const char* path, std::vector<float>& data, int& ch, int& sr)
{
    FILE* f = std::fopen (path, "rb"); if (! f) return false;
    std::vector<unsigned char> b; unsigned char tmp[65536]; size_t n;
    while ((n = std::fread (tmp, 1, sizeof tmp, f)) > 0) b.insert (b.end(), tmp, tmp + n);
    std::fclose (f);
    if (b.size() < 12 || std::memcmp (b.data(), "RIFF", 4) || std::memcmp (b.data() + 8, "WAVE", 4)) return false;
    size_t pos = 12; int fmt = 0, bits = 0;
    while (pos + 8 <= b.size())
    {
        uint32_t sz; std::memcpy (&sz, b.data() + pos + 4, 4);
        if (! std::memcmp (b.data() + pos, "fmt ", 4))
        {
            uint16_t t, c, bb; uint32_t r;
            std::memcpy (&t, b.data() + pos + 8, 2); std::memcpy (&c, b.data() + pos + 10, 2);
            std::memcpy (&r, b.data() + pos + 12, 4); std::memcpy (&bb, b.data() + pos + 22, 2);
            fmt = t; ch = c; sr = (int) r; bits = bb;
            if (t == 0xFFFE) { uint16_t sub; std::memcpy (&sub, b.data() + pos + 32, 2); fmt = sub; }
        }
        else if (! std::memcmp (b.data() + pos, "data", 4))
        {
            if (fmt != 3 || bits != 32) return false;
            data.resize (sz / 4); std::memcpy (data.data(), b.data() + pos + 8, sz);
            return true;
        }
        pos += 8 + sz + (sz & 1);
    }
    return false;
}

static void writeWav (const char* path, const std::vector<float>& d, int ch, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t data = (uint32_t) (d.size() * 4), riff = 36 + data, fmtSz = 16, rate = (uint32_t) sr, br = rate * ch * 4;
    const uint16_t tag = 3, c = (uint16_t) ch, ba = (uint16_t) (ch * 4), bits = 32;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&tag, 2, 1, f); std::fwrite (&c, 2, 1, f); std::fwrite (&rate, 4, 1, f);
    std::fwrite (&br, 4, 1, f); std::fwrite (&ba, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&data, 4, 1, f); std::fwrite (d.data(), 4, d.size(), f);
    std::fclose (f);
}

int main (int argc, char** argv)
{
    if (argc < 7) { std::fprintf (stderr, "usage: run_engine in.wav out.wav freq int gain orig [block]\n"); return 1; }
    std::vector<float> d; int ch = 0, sr = 0;
    if (! readWav (argv[1], d, ch, sr)) { std::fprintf (stderr, "cannot read %s (need 32-bit float WAV)\n", argv[1]); return 1; }
    freebass::Settings s; s.freqHz = std::atof (argv[3]); s.intensityDb = std::atof (argv[4]);
    s.gainDb = std::atof (argv[5]); s.origIn = std::atoi (argv[6]) != 0;
    const int block = argc > 7 ? std::atoi (argv[7]) : 512;
    const int use = ch >= 2 ? 2 : 1;
    const size_t frames = d.size() / ch;
    freebass::Engine e; e.prepare (sr);
    std::vector<float> L (block), R (block);
    std::vector<float> out (frames * use);
    for (size_t off = 0; off < frames; off += block)
    {
        const int m = (int) std::min<size_t> (block, frames - off);
        for (int i = 0; i < m; ++i) { L[i] = d[(off + i) * ch]; R[i] = use > 1 ? d[(off + i) * ch + 1] : 0.f; }
        float* io[2] = { L.data(), R.data() };
        e.process (io, use, m, s);
        for (int i = 0; i < m; ++i) { out[(off + i) * use] = L[i]; if (use > 1) out[(off + i) * use + 1] = R[i]; }
    }
    writeWav (argv[2], out, use, sr);
    return 0;
}
