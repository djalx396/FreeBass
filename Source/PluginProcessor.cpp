#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;

juce::AudioProcessorValueTreeState::ParameterLayout FreeBassProcessor::createLayout()
{
    auto dbAttr = juce::AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); });
    auto hzAttr = juce::AudioParameterFloatAttributes().withLabel ("Hz")
                    .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); });

    // Freq: 32..256 Hz in 1 Hz steps, log-shaped knob (centre ~ 90 Hz), like R Bass.
    juce::NormalisableRange<float> freqRange (32.f, 256.f, 1.f);
    freqRange.setSkewForCentre (90.5f);

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<APF> (juce::ParameterID { ID::freq, 1 },      "Frequency", freqRange, 80.f, hzAttr));
    p.push_back (std::make_unique<APF> (juce::ParameterID { ID::intensity, 1 }, "Intensity", juce::NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f, dbAttr));
    p.push_back (std::make_unique<APF> (juce::ParameterID { ID::gain, 1 },      "Out Gain",  juce::NormalisableRange<float> (-30.f, 0.f, 0.1f), 0.f, dbAttr));
    p.push_back (std::make_unique<APB> (juce::ParameterID { ID::orig, 1 },      "Original Bass", false,
                                        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? juce::String ("In") : juce::String ("Out"); })));
    return { p.begin(), p.end() };
}

FreeBassProcessor::FreeBassProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FREEBASS", createLayout())
{
    freqParam      = apvts.getRawParameterValue (ID::freq);
    intensityParam = apvts.getRawParameterValue (ID::intensity);
    gainParam      = apvts.getRawParameterValue (ID::gain);
    origParam      = apvts.getRawParameterValue (ID::orig);
}

bool FreeBassProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void FreeBassProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

void FreeBassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);

    freebass::Settings s;
    s.freqHz      = freqParam->load();
    s.intensityDb = intensityParam->load();
    s.gainDb      = gainParam->load();
    s.origIn      = origParam->load() > 0.5f;

    const int ch = juce::jlimit (1, 2, juce::jmin (numIn, numOut));
    float* io[2] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, ch, n, s, &telemetry);
}

void FreeBassProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("modelVersion", "v0", nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void FreeBassProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FreeBassProcessor::createEditor() { return new FreeBassEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FreeBassProcessor(); }
