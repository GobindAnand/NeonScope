#include "PluginEditor.h"

#include <cmath>

namespace
{
    const juce::Colour backgroundColour (0xff050505);
    const juce::Colour accentPrimary   = juce::Colour::fromRGB (0, 255, 191);
    const juce::Colour accentSecondary = juce::Colour::fromRGB (255, 0, 128);
    const juce::Colour accentTertiary  = juce::Colour::fromRGB (0, 153, 255);

    constexpr float meterSectionHeight   = 70.0f;
    constexpr float controlSectionHeight = 270.0f;
}

NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, backgroundColour.darker());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, neonMagenta.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void NeonLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, slider);

    if (! slider.isHorizontal())
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    auto track = bounds.withHeight (4.0f).withCentre ({ bounds.getCentreX(), bounds.getCentreY() });

    g.setColour (controlSurface);
    g.fillRoundedRectangle (track, 2.0f);

    const auto thumbX = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    auto fill = track.withWidth (thumbX - track.getX());

    g.setColour (neonCyan);
    g.fillRoundedRectangle (fill, 2.0f);

    g.setColour (outlineColour);
    g.drawRoundedRectangle (track, 2.0f, 1.0f);

    auto thumb = juce::Rectangle<float> (10.0f, 18.0f).withCentre ({ thumbX, track.getCentreY() });
    g.setColour (neonMagenta);
    g.fillRoundedRectangle (thumb, 4.0f);
}

void NeonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();

    g.setColour (controlSurface);
    g.fillEllipse (bounds);

    g.setColour (outlineColour);
    g.drawEllipse (bounds, 1.0f);

    const auto angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;

    juce::Path arc;
    arc.addArc (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                rotaryStartAngle, angle, true);
    g.setColour (neonCyan);
    g.strokePath (arc, juce::PathStrokeType (2.5f));

    const auto indicatorRadius = radius - 5.0f;
    juce::Point<float> indicator (centre.x + indicatorRadius * std::cos (angle),
                                  centre.y + indicatorRadius * std::sin (angle));

    g.setColour (neonMagenta);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (indicator));
}

void NeonLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (controlSurface);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (outlineColour);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Rectangle<float> arrowArea ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    auto center = arrowArea.getCentre();

    juce::Path arrow;
    arrow.addTriangle (center.x - 5.0f, center.y - 1.5f,
                       center.x + 5.0f, center.y - 1.5f,
                       center.x, center.y + 3.5f);

    g.setColour (neonCyan);
    g.fillPath (arrow);

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (neonMagenta.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.5f);
    }
}
NeonScopeAudioProcessorEditor::NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&neonLookAndFeel);

    auto configureLinearSlider = [] (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 88, 20);
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setNumDecimalPlacesToDisplay (2);
    };

    auto configureRotarySlider = [] (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setNumDecimalPlacesToDisplay (2);
    };

    auto applyUnitTextFormatting = [] (juce::Slider& slider, double multiplier, int decimals)
    {
        const auto suffix = slider.getTextValueSuffix();
        const auto suffixToken = suffix.trim();

        slider.textFromValueFunction = [suffix, multiplier, decimals] (double value)
        {
            return juce::String (value * multiplier, decimals) + suffix;
        };

        slider.valueFromTextFunction = [suffixToken, multiplier] (const juce::String& text)
        {
            const auto numericText = suffixToken.isEmpty()
                                         ? text
                                         : text.upToFirstOccurrenceOf (suffixToken, false, false);

            return numericText.trim().getDoubleValue() / multiplier;
        };
    };

    auto formatWithMaxOneDecimal = [] (double value)
    {
        auto text = juce::String (value, 1);
        text = text.trimCharactersAtEnd ("0");
        text = text.trimCharactersAtEnd (".");
        return text.isEmpty() ? juce::String ("0") : text;
    };

    configureLinearSlider (sensitivitySlider, "Sensitivity");
    configureLinearSlider (smoothingSlider, "Smoothing");
    configureLinearSlider (cutoffSlider, "Cutoff");
    configureRotarySlider (resonanceSlider, "Resonance");
    configureRotarySlider (driveSlider, "Drive");
    configureLinearSlider (widthSlider, "Stereo Width");
    configureRotarySlider (mixSlider, "Mix");
    configureRotarySlider (outputTrimSlider, "Output Trim");

    sensitivitySlider.setTextValueSuffix ("x");
    smoothingSlider.setTextValueSuffix (" %");
    resonanceSlider.setTextValueSuffix (" Q");
    driveSlider.setTextValueSuffix ("x");
    widthSlider.setTextValueSuffix (" %");
    mixSlider.setTextValueSuffix (" %");
    outputTrimSlider.setTextValueSuffix (" dB");

    applyUnitTextFormatting (sensitivitySlider, 1.0, 2);
    applyUnitTextFormatting (smoothingSlider, 100.0, 1);
    applyUnitTextFormatting (widthSlider, 100.0, 1);
    applyUnitTextFormatting (mixSlider, 100.0, 1);
    applyUnitTextFormatting (resonanceSlider, 1.0, 2);
    applyUnitTextFormatting (driveSlider, 1.0, 2);
    applyUnitTextFormatting (outputTrimSlider, 1.0, 1);

    cutoffSlider.textFromValueFunction = [formatWithMaxOneDecimal] (double value)
    {
        if (value >= 1000.0)
            return formatWithMaxOneDecimal (value / 1000.0) + " kHz";

        return formatWithMaxOneDecimal (value) + " Hz";
    };

    cutoffSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto trimmed = text.trim();
        const auto value = trimmed.getDoubleValue();
        const auto lowered = trimmed.toLowerCase();

        return lowered.contains ("khz") ? value * 1000.0 : value;
    };

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

NeonScopeAudioProcessorEditor::~NeonScopeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
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

    constexpr int rowHeight = 30;
    constexpr int rowSpacing = 6;
    constexpr int rotaryAreaHeight = 132;
    constexpr int rotarySpacing = 10;

    auto rotaryArea = controlArea.removeFromBottom (rotaryAreaHeight);
    controlArea.removeFromBottom (rotarySpacing);

    auto row1 = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (rowSpacing);
    auto row2 = controlArea.removeFromTop (rowHeight);
    controlArea.removeFromTop (rowSpacing);
    auto row3 = controlArea.removeFromTop (rowHeight);

    auto modeArea = row1.removeFromLeft (row1.proportionOfWidth (0.65f));
    modeBox.setBounds (modeArea.reduced (4, 0));
    oversamplingBox.setBounds (row1.reduced (4, 0));

    auto filterTypeArea = row2.removeFromLeft (row2.proportionOfWidth (0.3f));
    filterTypeBox.setBounds (filterTypeArea.reduced (4, 0));
    cutoffSlider.setBounds (row2.reduced (4, 4));

    auto row3Bounds = row3;
    const int totalRow3Width = row3Bounds.getWidth();
    const int satWidth = static_cast<int> (totalRow3Width * 0.22f);
    const int widthWidth = static_cast<int> (totalRow3Width * 0.4f);
    const int remaining = juce::jmax (0, totalRow3Width - satWidth - widthWidth);
    const int smallWidth = remaining / 2;

    auto satArea = row3Bounds.removeFromLeft (satWidth);
    auto widthArea = row3Bounds.removeFromLeft (widthWidth);
    auto sensitivityArea = row3Bounds.removeFromLeft (smallWidth);
    auto smoothingArea = row3Bounds;

    satModeBox.setBounds (satArea.reduced (4, 0));
    widthSlider.setBounds (widthArea.reduced (4, 4));
    sensitivitySlider.setBounds (sensitivityArea.reduced (4, 4));
    smoothingSlider.setBounds (smoothingArea.reduced (4, 4));

    auto rotaryTop = rotaryArea.removeFromTop (rotaryArea.getHeight() / 2);
    auto rotaryBottom = rotaryArea;

    const int rotaryPadding = 10;
    auto resonanceArea = rotaryTop.removeFromLeft (rotaryTop.getWidth() / 2).reduced (rotaryPadding);
    auto driveArea = rotaryTop.reduced (rotaryPadding);
    auto mixArea = rotaryBottom.removeFromLeft (rotaryBottom.getWidth() / 2).reduced (rotaryPadding);
    auto trimArea = rotaryBottom.reduced (rotaryPadding);

    resonanceSlider.setBounds (resonanceArea);
    driveSlider.setBounds (driveArea);
    mixSlider.setBounds (mixArea);
    outputTrimSlider.setBounds (trimArea);

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
