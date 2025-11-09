#include "PluginEditor.h"

namespace
{
    const juce::Colour backgroundColour (0xff050505);
    const juce::Colour accentPrimary   = juce::Colour::fromRGB (0, 255, 191);
    const juce::Colour accentSecondary = juce::Colour::fromRGB (255, 0, 128);
    const juce::Colour accentTertiary  = juce::Colour::fromRGB (0, 153, 255);

    constexpr float meterSectionHeight   = 70.0f;
    constexpr float controlSectionHeight = 200.0f;
}






NeonScopeAudioProcessorEditor::NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    auto configureSlider = [] (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::thumbColourId, accentPrimary);
        s.setColour (juce::Slider::trackColourId, accentTertiary.withAlpha (0.6f));
        s.setColour (juce::Slider::backgroundColourId, juce::Colours::white.withAlpha (0.07f));
        s.setNumDecimalPlacesToDisplay (2);
    };

    auto configurePercentageDisplay = [] (juce::Slider& slider)
    {
        const auto suffix = slider.getTextValueSuffix();
        const auto suffixToken = suffix.trim();

        slider.textFromValueFunction = [suffix] (double value)
        {
            return juce::String (value * 100.0, 2) + suffix;
        };

        slider.valueFromTextFunction = [suffixToken] (const juce::String& text)
        {
            auto numericText = suffixToken.isEmpty()
                                   ? text
                                   : text.upToFirstOccurrenceOf (suffixToken, false, false);

            return numericText.trim().getDoubleValue() / 100.0;
        };
    };

    configureSlider (sensitivitySlider, "Sensitivity");
    configureSlider (smoothingSlider, "Smoothing");
    configureSlider (cutoffSlider, "Cutoff");
    configureSlider (resonanceSlider, "Resonance");
    configureSlider (driveSlider, "Drive");
    configureSlider (widthSlider, "Stereo Width");
    configureSlider (mixSlider, "Mix");
    configureSlider (outputTrimSlider, "Output Trim");

    sensitivitySlider.setTextValueSuffix ("x");
    smoothingSlider.setTextValueSuffix (" %");
    cutoffSlider.setTextValueSuffix (" Hz");
    resonanceSlider.setTextValueSuffix (" Q");
    driveSlider.setTextValueSuffix ("x");
    widthSlider.setTextValueSuffix (" %");
    mixSlider.setTextValueSuffix (" %");
    outputTrimSlider.setTextValueSuffix (" dB");

    configurePercentageDisplay (smoothingSlider);
    configurePercentageDisplay (widthSlider);
    configurePercentageDisplay (mixSlider);

    auto configureCombo = [] (juce::ComboBox& box)
    {
        box.setJustificationType (juce::Justification::centredLeft);
        box.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white.withAlpha (0.07f));
    };

    configureCombo (modeBox);
    configureCombo (filterTypeBox);
    configureCombo (satModeBox);
    configureCombo (oversamplingBox);

    modeBox.addItem ("Visualize Only", 1);
    modeBox.addItem ("Tone Filter", 2);
    modeBox.addItem ("Soft Distortion", 3);
    modeBox.addItem ("Hybrid", 4);

    filterTypeBox.addItem ("Low-pass", 1);
    filterTypeBox.addItem ("High-pass", 2);
    filterTypeBox.addItem ("Band-pass", 3);

    satModeBox.addItem ("Tanh", 1);
    satModeBox.addItem ("Arctan", 2);
    satModeBox.addItem ("Hard Clip", 3);
    satModeBox.addItem ("Foldback", 4);

    oversamplingBox.addItem ("Off", 1);
    oversamplingBox.addItem ("2x", 2);

    const auto addControl = [this] (auto& control) { addAndMakeVisible (control); };

    addControl (modeBox);
    addControl (filterTypeBox);
    addControl (satModeBox);
    addControl (oversamplingBox);
    addControl (sensitivitySlider);
    addControl (smoothingSlider);
    addControl (cutoffSlider);
    addControl (resonanceSlider);
    addControl (driveSlider);
    addControl (widthSlider);
    addControl (mixSlider);
    addControl (outputTrimSlider);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "mode", modeBox);
    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "filterType", filterTypeBox);
    satModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "satMode", satModeBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "oversampling", oversamplingBox);

    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "sensitivity", sensitivitySlider);

    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "smoothing", smoothingSlider);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "cutoff", cutoffSlider);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "drive", driveSlider);

    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "resonance", resonanceSlider);

    widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "width", widthSlider);

    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "mix", mixSlider);

    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "outputTrim", outputTrimSlider);

    setSize (680, 460);
    startTimerHz (60);
    updateVisualState();
}

void NeonScopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto bounds = getLocalBounds().toFloat();

    auto meterArea = bounds.removeFromBottom (meterSectionHeight);
    auto controlArea = bounds.removeFromBottom (controlSectionHeight);
    auto titleArea = bounds.removeFromTop (50.0f);
    auto spectrumArea = bounds.reduced (20.0f, 10.0f);

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (32.0f, juce::Font::bold));
    g.drawText ("NeonScope", titleArea, juce::Justification::centred);

    const float barWidth = spectrumArea.getWidth() / static_cast<float> (NeonScopeAudioProcessor::numBands);
    const float spacing = 6.0f;
    for (int band = 0; band < NeonScopeAudioProcessor::numBands; ++band)
    {
        const float value = juce::jlimit (0.0f, 1.0f, bandCache[static_cast<size_t> (band)]);
        const float height = spectrumArea.getHeight() * value;

        juce::Rectangle<float> bar {
            spectrumArea.getX() + band * barWidth + spacing * 0.5f,
            spectrumArea.getBottom() - height,
            juce::jmax (2.0f, barWidth - spacing),
            height
        };

        juce::ColourGradient gradient (accentPrimary, bar.getCentreX(), bar.getBottom(),
                                       accentSecondary, bar.getCentreX(), bar.getY(), false);
        gradient.addColour (0.5, accentTertiary);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bar.withY (spectrumArea.getY()).withHeight (spectrumArea.getHeight()), 6.0f);

        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bar, 6.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.fillRoundedRectangle (controlArea.reduced (20.0f, 10.0f), 10.0f);

    auto drawMeter = [&g] (juce::Rectangle<float> area, float value, const juce::String& label)
    {
        const auto constrained = juce::jlimit (0.0f, 1.0f, value);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (area, 8.0f);

        auto fill = area.withWidth (area.getWidth() * constrained);
        juce::ColourGradient gradient (accentPrimary, fill.getX(), fill.getBottom(),
                                       accentSecondary, fill.getRight(), fill.getY(), false);
        gradient.addColour (0.4, accentTertiary);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (fill, 8.0f);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (label, area.reduced (8.0f, 0.0f), juce::Justification::centredLeft, false);
    };

    auto leftMeter = meterArea.removeFromTop (meterArea.getHeight() * 0.5f).reduced (20.0f, 6.0f);
    auto rightMeter = meterArea.reduced (20.0f, 6.0f);
    drawMeter (leftMeter, leftLevel, "Left");
    drawMeter (rightMeter, rightLevel, "Right");
}

void NeonScopeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto meterArea = bounds.removeFromBottom (static_cast<int> (meterSectionHeight));
    auto controlArea = bounds.removeFromBottom (static_cast<int> (controlSectionHeight)).reduced (30, 12);

    const int rowHeight = 34;
    const int spacing = 8;
    auto takeRow = [&controlArea, rowHeight, spacing]()
    {
        auto row = controlArea.removeFromTop (rowHeight);
        controlArea.removeFromTop (spacing);
        return row;
    };

    auto row1 = takeRow();
    auto row2 = takeRow();
    auto row3 = takeRow();
    auto row4 = takeRow();
    auto row5 = takeRow();

    auto modeArea = row1.removeFromLeft (row1.proportionOfWidth (0.65f));
    modeBox.setBounds (modeArea.reduced (4, 0));
    oversamplingBox.setBounds (row1.reduced (4, 0));

    auto filterTypeArea = row2.removeFromLeft (row2.proportionOfWidth (0.33f));
    filterTypeBox.setBounds (filterTypeArea.reduced (4, 0));
    cutoffSlider.setBounds (row2.reduced (4, 0));

    const int thirdWidth = row3.getWidth() / 3;
    auto resonanceArea = row3.removeFromLeft (thirdWidth);
    auto driveArea = row3.removeFromLeft (thirdWidth);
    auto satArea = row3;

    resonanceSlider.setBounds (resonanceArea.reduced (4, 0));
    driveSlider.setBounds (driveArea.reduced (4, 0));
    satModeBox.setBounds (satArea.reduced (4, 0));

    auto widthArea = row4.removeFromLeft (row4.getWidth() / 3);
    auto mixArea = row4.removeFromLeft (row4.getWidth() / 2);
    auto trimArea = row4;

    widthSlider.setBounds (widthArea.reduced (4, 0));
    mixSlider.setBounds (mixArea.reduced (4, 0));
    outputTrimSlider.setBounds (trimArea.reduced (4, 0));

    auto sensitivityArea = row5.removeFromLeft (row5.proportionOfWidth (0.5f));
    sensitivitySlider.setBounds (sensitivityArea.reduced (4, 0));
    smoothingSlider.setBounds (row5.reduced (4, 0));

    juce::ignoreUnused (meterArea);
}

void NeonScopeAudioProcessorEditor::timerCallback()
{
    updateVisualState();
    repaint();
}

void NeonScopeAudioProcessorEditor::updateVisualState()
{
    bandCache = processor.getBands();
    leftLevel = processor.getLeftLevel();
    rightLevel = processor.getRightLevel();

    const auto* modeParam = processor.getValueTreeState().getRawParameterValue ("mode");
    const int modeValue = modeParam != nullptr ? juce::roundToInt (modeParam->load()) : 0;
    const bool processingActive = modeValue != 0;
    const bool filterActive = modeValue == 1 || modeValue == 3;
    const bool distortionActive = modeValue == 2 || modeValue == 3;

    auto setControlState = [] (juce::Component& comp, bool active)
    {
        comp.setEnabled (active);
        comp.setAlpha (active ? 1.0f : 0.4f);
    };

    setControlState (filterTypeBox, filterActive);
    setControlState (cutoffSlider, filterActive);
    setControlState (resonanceSlider, filterActive);

    setControlState (driveSlider, distortionActive);
    setControlState (satModeBox, distortionActive);
    setControlState (oversamplingBox, distortionActive);

    setControlState (widthSlider, processingActive);
    setControlState (mixSlider, processingActive);
    setControlState (outputTrimSlider, processingActive);
}
