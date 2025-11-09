#include "PluginEditor.h"

namespace
{
    const juce::Colour backgroundColour (0xff050505);
    const juce::Colour accentPrimary    = juce::Colour::fromRGB (0, 255, 191);
    const juce::Colour accentSecondary  = juce::Colour::fromRGB (255, 0, 128);
    const juce::Colour accentTertiary   = juce::Colour::fromRGB (0, 153, 255);
    constexpr float meterSectionHeight  = 70.0f;
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
    };

    configureSlider (sensitivitySlider, "Sensitivity");
    configureSlider (smoothingSlider, "Smoothing");
    configureSlider (cutoffSlider, "Cutoff");
    configureSlider (driveSlider, "Drive");

    modeBox.setJustificationType (juce::Justification::centredLeft);
    modeBox.addItem ("Visualize Only", 1);
    modeBox.addItem ("Low-Pass Filter", 2);
    modeBox.addItem ("Soft Distortion", 3);
    modeBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    modeBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    modeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::white.withAlpha (0.07f));

    addAndMakeVisible (modeBox);
    addAndMakeVisible (sensitivitySlider);
    addAndMakeVisible (smoothingSlider);
    addAndMakeVisible (cutoffSlider);
    addAndMakeVisible (driveSlider);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "mode", modeBox);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "sensitivity", sensitivitySlider);

    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "smoothing", smoothingSlider);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "cutoff", cutoffSlider);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "drive", driveSlider);

    setSize (620, 420);
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

    const int rowHeight = 32;
    const int spacing = 8;

    auto modeArea = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (spacing);
    auto cutoffArea = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (spacing);
    auto driveArea = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (spacing);
    auto sensitivityArea = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (spacing);
    auto smoothingArea = controlArea.removeFromTop (rowHeight);

    modeBox.setBounds (modeArea);
    cutoffSlider.setBounds (cutoffArea);
    driveSlider.setBounds (driveArea);
    sensitivitySlider.setBounds (sensitivityArea);
    smoothingSlider.setBounds (smoothingArea);

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
    const bool lowPassActive = modeValue == 1;
    const bool distortionActive = modeValue == 2;

    cutoffSlider.setEnabled (lowPassActive);
    cutoffSlider.setAlpha (lowPassActive ? 1.0f : 0.4f);
    driveSlider.setEnabled (distortionActive);
    driveSlider.setAlpha (distortionActive ? 1.0f : 0.4f);
}
