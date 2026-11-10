#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

//==============================================================================
FreeBassLookAndFeel::FreeBassLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, ui::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, ui::bg);
    setColour (Label::textColourId, ui::text);
    setColour (TextButton::buttonColourId, ui::panel.brighter (0.05f));
    setColour (TextButton::textColourOffId, ui::dim);
    setColour (TextButton::textColourOnId, Colours::black);
}

void FreeBassLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6);
    const float d = jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);
    const auto c = r.getCentre();
    const float rad = d * 0.5f, ang = a0 + pos * (a1 - a0);

    Path bgArc; bgArc.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, a1, true);
    g.setColour (ui::edge); g.strokePath (bgArc, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
    Path valArc; valArc.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, ang, true);
    g.setColour (ui::accent); g.strokePath (valArc, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (ui::panel.brighter (0.12f)); g.fillEllipse (r.reduced (11));
    g.setColour (ui::edge); g.drawEllipse (r.reduced (11), 1.f);
    Path ptr; ptr.addRoundedRectangle (-2.f, -(rad - 14), 4.f, rad * 0.42f, 2.f);
    g.setColour (ui::text); g.fillPath (ptr, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void FreeBassLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                            Slider::SliderStyle, Slider& s)
{
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (ui::bg); g.fillRoundedRectangle (cx - 3, (float) y, 6, (float) h, 3);
    g.setColour (ui::edge); g.drawRoundedRectangle (cx - 3, (float) y, 6, (float) h, 3, 1);
    // 0 dB tick
    const float zeroY = (float) y + (float) h * (1.f - (float) s.valueToProportionOfLength (0.0));
    g.setColour (ui::dim); g.fillRect (cx - 10, zeroY - 0.5f, 20.f, 1.f);
    auto cap = Rectangle<float> (cx - 18, pos - 9, 36, 18);
    g.setColour (ui::panel.brighter (0.25f)); g.fillRoundedRectangle (cap, 3);
    g.setColour (ui::text); g.fillRect (cap.getX() + 4, pos - 0.75f, cap.getWidth() - 8, 1.5f);
}

void FreeBassLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const bool on = b.getToggleState();
    Colour c = on ? b.findColour (TextButton::buttonOnColourId) : ui::panel.brighter (0.06f);
    if (over && ! on) c = c.brighter (0.08f);
    if (down) c = c.darker (0.1f);
    g.setColour (c); g.fillRoundedRectangle (r, 4);
    g.setColour (on ? c.brighter (0.2f) : ui::edge); g.drawRoundedRectangle (r, 4, 1);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (RangedAudioParameter& p, StringArray labels, Colour onColour)
    : attachment (p, [this] (float v)
      {
          const int sel = roundToInt (v);
          for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == sel, dontSendNotification);
      })
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->setColour (TextButton::buttonOnColourId, onColour);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / jmax (1, buttons.size());
    for (auto* b : buttons) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

//==============================================================================
static constexpr float kMeterLo = -48.f;

void Meter::setLevel (float lin)
{
    const float db = lin > 1e-6f ? 20.f * std::log10 (lin) : -100.f;
    dispDb = db > dispDb ? db : jmax (db, dispDb - 1.2f);
    if (db >= holdDb) { holdDb = db; holdTicks = 45; }
    else if (--holdTicks <= 0) holdDb = jmax (dispDb, holdDb - 0.8f);
    repaint();
}

void Meter::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (ui::bg); g.fillRoundedRectangle (b, 3);
    auto inner = b.reduced (3);
    auto yOf = [&] (float db) { return inner.getBottom() - (jlimit (kMeterLo, 0.f, db) - kMeterLo) / -kMeterLo * inner.getHeight(); };
    if (dispDb > kMeterLo)
    {
        auto bar = inner.withTop (yOf (dispDb));
        g.setColour (dispDb > -0.1f ? ui::clip : colour); g.fillRoundedRectangle (bar, 2);
    }
    if (holdDb > kMeterLo) { g.setColour (ui::text); g.fillRect (inner.getX(), yOf (holdDb) - 1.f, inner.getWidth(), 2.f); }
    if (grDb < -0.1f)
    {
        // limiter gain reduction, drawn from the top
        const float h = jmin (1.f, -grDb / 12.f) * inner.getHeight();
        g.setColour (ui::accent.withAlpha (0.85f)); g.fillRect (inner.getRight() - 3.f, inner.getY(), 3.f, h);
    }
    g.setColour (ui::edge);
    for (int db = 0; db >= (int) kMeterLo; db -= 12) g.fillRect (inner.getX(), yOf ((float) db), 4.f, 1.f);
}

//==============================================================================
FreeBassEditor::FreeBassEditor (FreeBassProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      freqAtt (p.apvts, ID::freq, freq), intensityAtt (p.apvts, ID::intensity, intensity), gainAtt (p.apvts, ID::gain, gain),
      orig (*p.apvts.getParameter (ID::orig), { "OUT", "IN" }, ui::bass)
{
    setLookAndFeel (&lnf);

    freq.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    freq.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 22);
    freq.setTextValueSuffix (" Hz");
    freq.setDoubleClickReturnValue (true, 80.0);
    addAndMakeVisible (freq);

    for (auto* s : { &intensity, &gain })
    {
        s->setSliderStyle (Slider::LinearVertical);
        s->setTextBoxStyle (Slider::TextBoxBelow, false, 70, 22);
        s->setTextValueSuffix (" dB");
        s->setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (*s);
    }
    for (auto* s : { &freq, &intensity, &gain })
        s->setColour (Slider::textBoxOutlineColourId, ui::edge);
    addAndMakeVisible (orig);
    addAndMakeVisible (harmMeter);
    addAndMakeVisible (outMeter);

    setSize (560, 340);
    startTimerHz (30);
}

FreeBassEditor::~FreeBassEditor() { setLookAndFeel (nullptr); }

void FreeBassEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);
    auto top = getLocalBounds().removeFromTop (40).reduced (16, 0);
    g.setFont (uiFont (20.f, true)); g.setColour (ui::text);
    g.drawText ("FreeBass", top, Justification::centredLeft);
    g.setFont (uiFont (11.f)); g.setColour (ui::dim);
    g.drawText ("THE ECOSYSTEM  |  v0.1  |  model v0", top, Justification::centredRight);

    for (auto r : { freqPanel, harmPanel, outPanel })
    {
        g.setColour (ui::panel); g.fillRoundedRectangle (r.toFloat(), 6);
    }
    g.setFont (uiFont (11.f, true)); g.setColour (ui::dim);
    g.drawText ("FREQUENCY", freqPanel.withHeight (24).translated (0, 6), Justification::centred);
    g.drawText ("ORIGINAL BASS", Rectangle<int> (freqPanel.getX(), freqPanel.getBottom() - 62, freqPanel.getWidth(), 16), Justification::centred);
    g.drawText ("INTENSITY", harmPanel.withHeight (24).translated (0, 6), Justification::centred);
    g.drawText ("GAIN", outPanel.withHeight (24).translated (0, 6), Justification::centred);

    g.setFont (uiFont (10.f)); g.setColour (ui::dim);
    g.drawText ("HARMONICS", Rectangle<int> (harmMeter.getX() - 40, harmMeter.getBottom() + 4, harmMeter.getWidth() + 80, 14), Justification::centred);
    g.drawText ("OUTPUT", Rectangle<int> (outMeter.getX() - 40, outMeter.getBottom() + 4, outMeter.getWidth() + 80, 14), Justification::centred);

    // clip LED (click to reset)
    g.setColour (clipShown ? ui::clip : ui::edge);
    g.fillEllipse (clipArea.toFloat().reduced (2));
    g.setColour (ui::dim);
    g.drawText ("CLIP", clipArea.translated (-34, 0).withWidth (32), Justification::centredRight);
}

void FreeBassEditor::resized()
{
    auto r = getLocalBounds().withTrimmedTop (40).reduced (12, 0).withTrimmedBottom (12);
    freqPanel = r.removeFromLeft (220);
    r.removeFromLeft (10);
    harmPanel = r.removeFromLeft ((r.getWidth() - 10) / 2);
    r.removeFromLeft (10);
    outPanel = r;

    auto f = freqPanel.reduced (12).withTrimmedTop (22);
    orig.setBounds (f.removeFromBottom (30).reduced (20, 0));
    f.removeFromBottom (24);
    freq.setBounds (f);

    auto layoutSliderMeter = [] (Rectangle<int> panel, Slider& s, Meter& m)
    {
        auto a = panel.reduced (12).withTrimmedTop (22);
        auto meterCol = a.removeFromRight (40);
        s.setBounds (a);
        m.setBounds (meterCol.withTrimmedBottom (40).withTrimmedTop (4).withSizeKeepingCentre (14, meterCol.getHeight() - 44));
    };
    layoutSliderMeter (harmPanel, intensity, harmMeter);
    layoutSliderMeter (outPanel, gain, outMeter);
    clipArea = Rectangle<int> (outPanel.getRight() - 30, outPanel.getY() + 8, 14, 14);
}

void FreeBassEditor::mouseDown (const MouseEvent& e)
{
    if (clipArea.expanded (30, 4).contains (e.getPosition()))
    {
        proc.telemetry.clipped.store (false);
        clipShown = false;
        repaint (clipArea.expanded (40, 4));
    }
}

void FreeBassEditor::timerCallback()
{
    harmMeter.setLevel (proc.telemetry.harmPeak.exchange (0.f));
    harmMeter.setGr (proc.telemetry.limGrDb.load());
    outMeter.setLevel (proc.telemetry.outPeak.exchange (0.f));
    const bool c = proc.telemetry.clipped.load();
    if (c != clipShown) { clipShown = c; repaint (clipArea.expanded (40, 4)); }
}
