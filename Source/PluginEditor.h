#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace ui
{
    // Ecosystem palette (shared with Free Rider)
    const juce::Colour bg     { 0xff15171b };
    const juce::Colour panel  { 0xff1e2127 };
    const juce::Colour edge   { 0xff2c3038 };
    const juce::Colour text   { 0xffc9ced6 };
    const juce::Colour dim    { 0xff6c737e };
    const juce::Colour bass   { 0xff3ec7a5 };   // processed bass / harmonics
    const juce::Colour accent { 0xffe8a33d };   // Freq
    const juce::Colour clip   { 0xffe5484d };
}

class FreeBassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FreeBassLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

// Row of mutually exclusive buttons bound to a choice/bool parameter (index = value).
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter&, juce::StringArray labels, juce::Colour onColour);
    void resized() override;
private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

// Vertical peak meter in dBFS with optional gain-reduction marker.
class Meter : public juce::Component
{
public:
    Meter (juce::Colour c) : colour (c) {}
    void setLevel (float linPeak);
    void setGr (float db) { if (std::abs (db - grDb) > 0.05f) { grDb = db; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    juce::Colour colour;
    float dispDb = -100.f, holdDb = -100.f, grDb = 0.f;
    int holdTicks = 0;
};

class FreeBassEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreeBassEditor (FreeBassProcessor&);
    ~FreeBassEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;

    FreeBassProcessor& proc;
    FreeBassLookAndFeel lnf;
    juce::Slider freq, intensity, gain;
    juce::AudioProcessorValueTreeState::SliderAttachment freqAtt, intensityAtt, gainAtt;
    ChoiceButtons orig;
    Meter harmMeter { ui::bass }, outMeter { ui::text };
    bool clipShown = false;
    juce::Rectangle<int> freqPanel, harmPanel, outPanel, clipArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeBassEditor)
};
