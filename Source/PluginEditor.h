#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DropItLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DropItLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    static const juce::Colour background, panel, accent, text, dim;
};

class DropItAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DropItAudioProcessorEditor (DropItAudioProcessor&);
    ~DropItAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name);
    void updateTuningDisplay();

    DropItAudioProcessor& processorRef;
    DropItLookAndFeel lnf;   // declared first so it outlives the components

    juce::Slider dropKnob, fineKnob, mixKnob, outputKnob;
    juce::Label  dropLabel, fineLabel, mixLabel, outputLabel;
    juce::ComboBox tuningBox;
    juce::Label  tuningTitle;

    std::unique_ptr<SliderAttachment> dropAtt, fineAtt, mixAtt, outputAtt;

    juce::Rectangle<int> stringsArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropItAudioProcessorEditor)
};
