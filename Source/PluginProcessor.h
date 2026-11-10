#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PitchShifter.h"

namespace ParamIDs
{
    inline constexpr auto drop   = "drop";    // semitones down, 0..12
    inline constexpr auto fine   = "fine";    // cents, -50..+50
    inline constexpr auto mix    = "mix";     // 0..100 % wet
    inline constexpr auto output = "output";  // dB
}

class DropItAudioProcessor : public juce::AudioProcessor
{
public:
    DropItAudioProcessor();
    ~DropItAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    PitchShifter shifter;
    juce::dsp::DryWetMixer<float> dryWet { 1 << 15 };
    juce::dsp::Gain<float> outputGain;

    std::atomic<float>* dropParam   = nullptr;
    std::atomic<float>* fineParam   = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropItAudioProcessor)
};
