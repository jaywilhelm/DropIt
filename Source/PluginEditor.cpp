#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Name of the resulting tuning when a guitar in E standard / drop D
    // is shifted down by N semitones.
    struct TuningName { const char* standard; const char* dropped; };

    const TuningName tunings[] =
    {
        { "E Standard",        "Drop D"  },   // 0
        { "Eb Standard",       "Drop C#" },   // 1
        { "D Standard",        "Drop C"  },   // 2
        { "C# Standard",       "Drop B"  },   // 3
        { "C Standard",        "Drop A#" },   // 4
        { "B Standard",        "Drop A"  },   // 5
        { "Bb Standard",       "Drop G#" },   // 6
        { "A Standard",        "Drop G"  },   // 7
        { "Ab Standard",       "Drop F#" },   // 8
        { "G Standard",        "Drop F"  },   // 9
        { "F# Standard",       "Drop E"  },   // 10
        { "F Standard",        "Drop D#" },   // 11
        { "E Standard (-1 oct)", "Drop D (-1 oct)" } // 12
    };

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Open strings of a 6-string guitar in E standard (MIDI note numbers), low to high.
    const int standardStrings[] = { 40, 45, 50, 55, 59, 64 };

    juce::String noteName (int midi) { return noteNames[((midi % 12) + 12) % 12]; }
}

//==============================================================================
const juce::Colour DropItLookAndFeel::background { 0xff15171c };
const juce::Colour DropItLookAndFeel::panel      { 0xff1f232b };
const juce::Colour DropItLookAndFeel::accent     { 0xffff7a1a };
const juce::Colour DropItLookAndFeel::text       { 0xffe8eaee };
const juce::Colour DropItLookAndFeel::dim        { 0xff7d8594 };

DropItLookAndFeel::DropItLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               text);
    setColour (juce::ComboBox::backgroundColourId,      panel.brighter (0.08f));
    setColour (juce::ComboBox::outlineColourId,         dim.withAlpha (0.4f));
    setColour (juce::ComboBox::textColourId,            text);
    setColour (juce::ComboBox::arrowColourId,           accent);
    setColour (juce::PopupMenu::backgroundColourId,     panel);
    setColour (juce::PopupMenu::textColourId,           text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

void DropItLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const auto track   = juce::jmax (3.0f, radius * 0.12f);
    const auto arcR    = radius - track * 0.5f;
    const auto angle   = startAngle + sliderPos * (endAngle - startAngle);

    // Background track
    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (background.brighter (0.15f));
    g.strokePath (bg, { track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Value arc (bipolar sliders draw from their zero point)
    float fromAngle = startAngle;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        fromAngle = startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle);

    if (std::abs (angle - fromAngle) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                           juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        g.setColour (slider.isEnabled() ? accent : dim);
        g.strokePath (arc, { track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    // Knob body
    const auto bodyR = arcR - track * 1.4f;
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.25f), centre.x, centre.y - bodyR,
                                             panel.darker (0.4f),    centre.x, centre.y + bodyR, false));
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre), 1.0f);

    // Pointer
    juce::Path pointer;
    const auto pw = juce::jmax (2.0f, bodyR * 0.08f);
    pointer.addRoundedRectangle (-pw * 0.5f, -bodyR * 0.9f, pw, bodyR * 0.45f, pw * 0.5f);
    g.setColour (text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
DropItAudioProcessorEditor::DropItAudioProcessorEditor (DropItAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lnf);

    setupKnob (dropKnob,   dropLabel,   "DROP");
    setupKnob (fineKnob,   fineLabel,   "FINE");
    setupKnob (mixKnob,    mixLabel,    "MIX (DRY/WET)");
    setupKnob (outputKnob, outputLabel, "OUTPUT");

    dropKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 24);

    tuningTitle.setText ("TUNING", juce::dontSendNotification);
    tuningTitle.setJustificationType (juce::Justification::centredLeft);
    tuningTitle.setColour (juce::Label::textColourId, DropItLookAndFeel::dim);
    tuningTitle.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (tuningTitle);

    for (int i = 0; i <= 12; ++i)
    {
        const auto label = juce::String (i == 0 ? "0" : "-" + juce::String (i)) + " st   "
                         + tunings[i].standard + "  /  " + tunings[i].dropped;
        tuningBox.addItem (label, i + 1);
    }
    addAndMakeVisible (tuningBox);

    auto& s = processorRef.apvts;
    dropAtt   = std::make_unique<SliderAttachment> (s, ParamIDs::drop,   dropKnob);
    fineAtt   = std::make_unique<SliderAttachment> (s, ParamIDs::fine,   fineKnob);
    mixAtt    = std::make_unique<SliderAttachment> (s, ParamIDs::mix,    mixKnob);
    outputAtt = std::make_unique<SliderAttachment> (s, ParamIDs::output, outputKnob);

    // Keep knob and preset box in sync (the knob owns the parameter attachment).
    dropKnob.onValueChange = [this] { updateTuningDisplay(); };
    tuningBox.onChange = [this]
    {
        const auto id = tuningBox.getSelectedId();
        if (id > 0)
            dropKnob.setValue (id - 1, juce::sendNotificationSync);
    };

    fineKnob.setDoubleClickReturnValue (true, 0.0);
    mixKnob.setDoubleClickReturnValue (true, 100.0);
    outputKnob.setDoubleClickReturnValue (true, 0.0);
    dropKnob.setDoubleClickReturnValue (true, 2.0);

    updateTuningDisplay();

    setResizable (true, true);
    setResizeLimits (560, 380, 1120, 760);
    getConstrainer()->setFixedAspectRatio (620.0 / 420.0);
    setSize (620, 420);
}

DropItAudioProcessorEditor::~DropItAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void DropItAudioProcessorEditor::setupKnob (juce::Slider& knob, juce::Label& label, const juce::String& name)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    knob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (knob);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, DropItLookAndFeel::dim);
    label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (label);
}

void DropItAudioProcessorEditor::updateTuningDisplay()
{
    const int drop = juce::jlimit (0, 12, juce::roundToInt (dropKnob.getValue()));
    tuningBox.setSelectedId (drop + 1, juce::dontSendNotification);
    repaint();
}

void DropItAudioProcessorEditor::paint (juce::Graphics& g)
{
    using LF = DropItLookAndFeel;
    const float scale = (float) getWidth() / 620.0f;

    g.fillAll (LF::background);

    // Header
    auto header = getLocalBounds().removeFromTop (juce::roundToInt (56 * scale));
    g.setColour (LF::panel);
    g.fillRect (header);
    g.setColour (LF::accent);
    g.fillRect (header.removeFromBottom (2));

    auto titleArea = header.reduced (juce::roundToInt (20 * scale), 0);
    g.setFont (juce::Font (juce::FontOptions (26.0f * scale, juce::Font::bold)));
    g.setColour (LF::text);
    g.drawText ("DROP", titleArea, juce::Justification::centredLeft);
    const auto dropWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "DROP"));
    g.setColour (LF::accent);
    g.drawText ("IT", titleArea.withTrimmedLeft (dropWidth), juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (12.0f * scale)));
    g.setColour (LF::dim);
    g.drawText ("drop tuning pitch shifter", titleArea, juce::Justification::centredRight);

    // Strings display: shows the open-string notes after the drop.
    if (! stringsArea.isEmpty())
    {
        g.setColour (LF::panel);
        g.fillRoundedRectangle (stringsArea.toFloat(), 8.0f * scale);

        const int drop = juce::jlimit (0, 12, juce::roundToInt (dropKnob.getValue()));
        auto area = stringsArea.reduced (juce::roundToInt (10 * scale));

        g.setColour (LF::dim);
        g.setFont (juce::Font (juce::FontOptions (11.0f * scale, juce::Font::bold)));
        g.drawText ("OPEN STRINGS  (standard  |  drop)", area.removeFromTop (juce::roundToInt (16 * scale)),
                    juce::Justification::centredLeft);

        const int n = 6;
        const int cellW = area.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int> (area.getX() + i * cellW, area.getY(), cellW, area.getHeight())
                            .reduced (juce::roundToInt (3 * scale));

            g.setColour (LF::background);
            g.fillRoundedRectangle (cell.toFloat(), 5.0f * scale);

            const int midiStd  = standardStrings[i] - drop;
            const int midiDrop = (i == 0 ? standardStrings[i] - 2 : standardStrings[i]) - drop;

            auto top = cell.removeFromTop (cell.getHeight() / 2);
            g.setColour (LF::text);
            g.setFont (juce::Font (juce::FontOptions (18.0f * scale, juce::Font::bold)));
            g.drawText (noteName (midiStd), top, juce::Justification::centredBottom);

            g.setColour (i == 0 ? LF::accent : LF::dim);
            g.setFont (juce::Font (juce::FontOptions (14.0f * scale)));
            g.drawText (noteName (midiDrop), cell, juce::Justification::centredTop);
        }
    }
}

void DropItAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / 620.0f;
    auto s = [scale] (int v) { return juce::roundToInt ((float) v * scale); };

    auto area = getLocalBounds();
    area.removeFromTop (s (56));
    area.reduce (s (20), s (14));

    // Left: big drop knob
    auto left = area.removeFromLeft (s (230));
    dropLabel.setFont (juce::Font (juce::FontOptions (14.0f * scale, juce::Font::bold)));
    dropLabel.setBounds (left.removeFromTop (s (22)));
    dropKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, s (100), s (24));
    dropKnob.setBounds (left.reduced (s (4)));

    area.removeFromLeft (s (16));

    // Right top: tuning selector
    auto right = area;
    tuningTitle.setBounds (right.removeFromTop (s (18)));
    tuningBox.setBounds (right.removeFromTop (s (30)));
    right.removeFromTop (s (10));

    stringsArea = right.removeFromTop (s (86));
    right.removeFromTop (s (10));

    // Right bottom: three small knobs
    const int w = right.getWidth() / 3;
    juce::Slider* knobs[] = { &fineKnob, &mixKnob, &outputKnob };
    juce::Label*  labels[] = { &fineLabel, &mixLabel, &outputLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = right.removeFromLeft (w);
        labels[i]->setFont (juce::Font (juce::FontOptions (12.0f * scale, juce::Font::bold)));
        labels[i]->setBounds (cell.removeFromTop (s (18)));
        knobs[i]->setTextBoxStyle (juce::Slider::TextBoxBelow, false, s (90), s (20));
        knobs[i]->setBounds (cell);
    }
}
