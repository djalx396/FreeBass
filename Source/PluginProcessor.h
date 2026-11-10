#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FreeBassEngine.h"

namespace ID
{
    static const juce::String freq { "freq" }, intensity { "intensity" }, gain { "gain" }, orig { "orig" };
}

class FreeBassProcessor : public juce::AudioProcessor
{
public:
    FreeBassProcessor();
    ~FreeBassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FreeBass"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    freebass::Telemetry telemetry;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    freebass::Engine engine;
    std::atomic<float>* freqParam = nullptr;
    std::atomic<float>* intensityParam = nullptr;
    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* origParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeBassProcessor)
};
