#include "PluginProcessor.h"
#include "PluginEditor.h"

DropItAudioProcessor::DropItAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DropItState", createParameterLayout())
{
    dropParam   = apvts.getRawParameterValue (ParamIDs::drop);
    fineParam   = apvts.getRawParameterValue (ParamIDs::fine);
    mixParam    = apvts.getRawParameterValue (ParamIDs::mix);
    outputParam = apvts.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout DropItAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamIDs::drop, 1 }, "Drop", 0, 12, 2,
        AudioParameterIntAttributes()
            .withLabel ("st")
            .withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("0 st") : "-" + String (v) + " st"; })
            .withValueFromStringFunction ([] (const String& s) { return std::abs (s.retainCharacters ("0123456789").getIntValue()); })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fine, 1 }, "Fine",
        NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("ct")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " ct"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })));

    return layout;
}

bool DropItAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void DropItAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    shifter.prepare (sampleRate, (int) numChannels);

    // Dry path is delayed by the shifter's latency so dry & wet stay aligned.
    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::sin3dB);
    dryWet.setWetLatency ((float) shifter.getLatencySamples());

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.05);

    setLatencySamples (shifter.getLatencySamples());
}

void DropItAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float semitones = -(float) dropParam->load() + fineParam->load() / 100.0f;
    shifter.setPitchRatio (std::pow (2.0f, semitones / 12.0f));

    dryWet.setWetMixProportion (mixParam->load() / 100.0f);
    outputGain.setGainDecibels (outputParam->load());

    juce::dsp::AudioBlock<float> block (buffer);

    dryWet.pushDrySamples (block);
    shifter.process (block);
    dryWet.mixWetSamples (block);

    juce::dsp::ProcessContextReplacing<float> ctx (block);
    outputGain.process (ctx);
}

juce::AudioProcessorEditor* DropItAudioProcessor::createEditor()
{
    return new DropItAudioProcessorEditor (*this);
}

void DropItAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DropItAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DropItAudioProcessor();
}
