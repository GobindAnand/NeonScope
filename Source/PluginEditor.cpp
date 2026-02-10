#include "PluginEditor.h"

#include <cmath>

namespace
{
    const juce::Colour backgroundColour (0xff1a1a1a);
    const juce::Colour accentPrimary   = juce::Colour::fromRGB (80, 200, 255);
    const juce::Colour accentSecondary = juce::Colour::fromRGB (100, 180, 255);
    const juce::Colour accentTertiary  = juce::Colour::fromRGB (120, 160, 255);

    constexpr float meterDbFloor   = -60.0f;
    constexpr float meterDbCeiling = 0.0f;
    constexpr float peakDbCeiling  = 6.0f;

    static juce::String formatHz (float v)
    {
        if (v >= 1000.0f)
            return juce::String (v / 1000.0f, 2) + " kHz";
        return juce::String (v, 0) + " Hz";
    }

    static juce::String formatQ (float v)
    {
        return juce::String (v, 2) + " Q";
    }

    static juce::String formatPercent (float v)
    {
        return juce::String (v * 100.0f, 1) + " %";
    }

    static juce::String formatDrive (float v)
    {
        return juce::String (v, 1) + "x";
    }

    static juce::String formatDb (float v)
    {
        auto sign = v >= 0.0f ? "+" : "";
        return sign + juce::String (v, 1) + " dB";
    }

    static juce::String formatSensitivity (float v)
    {
        const float dbValue = juce::Decibels::gainToDecibels (v, -60.0f);
        return formatDb (dbValue);
    }
}

NeonDialLook::NeonDialLook()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, panelBase.darker());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, neonMagenta.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void NeonDialLook::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float minSliderPos, float maxSliderPos,
                                     const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style);

    if (! slider.isHorizontal())
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    auto track = bounds.withHeight (6.0f).withCentre ({ bounds.getCentreX(), bounds.getCentreY() });

    g.setColour (panelBase.withAlpha (0.9f));
    g.fillRoundedRectangle (track, 3.0f);

    const auto thumbX = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    auto fill = juce::Rectangle<float> (track.getX(), track.getY(), thumbX - track.getX(), track.getHeight());
    g.setColour (neonCyan.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawRoundedRectangle (track, 3.0f, 1.0f);

    auto thumbBounds = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ thumbX, track.getCentreY() });
    juce::Path glow;
    glow.addEllipse (thumbBounds);
    juce::DropShadow (neonMagenta.withAlpha (0.45f), 10, {}).drawForPath (g, glow);

    g.setColour (juce::Colours::white);
    g.fillEllipse (thumbBounds);
}

void NeonDialLook::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPosProportional,
                                     float rotaryStartAngle, float rotaryEndAngle,
                                     juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const bool hover = slider.isMouseOverOrDragging();

    g.setColour (dialBase.brighter (0.05f));
    g.fillEllipse (bounds);

    const auto trackRadius = radius - 6.0f;
    const auto trackThickness = 3.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackRadius, trackRadius,
                         0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.strokePath (track, juce::PathStrokeType (trackThickness));

    const auto angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;
    juce::Path valuePath;
    valuePath.addCentredArc (centre.x, centre.y, trackRadius, trackRadius,
                             0.0f, rotaryStartAngle, angle, true);
    g.setColour (neonCyan.withAlpha (hover ? 0.95f : 0.85f));
    g.strokePath (valuePath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (bounds, 1.0f);

    const auto pointerLength = radius - 8.0f;
    const juce::Point<float> pointerPos {
        centre.x + std::cos (angle) * pointerLength,
        centre.y + std::sin (angle) * pointerLength
    };

    g.setColour (juce::Colours::white.withAlpha (hover ? 0.95f : 0.85f));
    g.drawLine (centre.x, centre.y, pointerPos.x, pointerPos.y, 1.8f);

    g.setColour (dialBase.brighter (0.3f));
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre), 1.0f);
}

void NeonDialLook::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                 int buttonX, int buttonY, int buttonW, int buttonH,
                                 juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    const bool hover = box.isMouseOver (true);
    g.setColour (dialBase.brighter (hover ? 0.08f : 0.0f));
    g.fillRoundedRectangle (bounds, 4.0f);

    const float outlineAlpha = hover ? 0.35f : 0.15f;
    g.setColour (juce::Colours::white.withAlpha (outlineAlpha));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Rectangle<float> arrowArea ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH);
    auto center = arrowArea.getCentre();

    juce::Path arrow;
    arrow.addTriangle (center.x - 5.0f, center.y - 1.5f,
                       center.x + 5.0f, center.y - 1.5f,
                       center.x, center.y + 3.5f);

    g.setColour (neonCyan.withAlpha (0.9f));
    g.fillPath (arrow);

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (neonMagenta.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.3f);
    }
}

void NeonDialLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
    const bool isOn = button.getToggleState();
    const bool hover = shouldDrawButtonAsHighlighted || button.isMouseOver();
    const bool down = shouldDrawButtonAsDown;

    auto baseColour = dialBase.withAlpha (0.9f);
    if (isOn)
        baseColour = baseColour.brighter (0.08f);

    g.setColour (baseColour);
    g.fillRoundedRectangle (bounds, 6.0f);

    const float glowAlpha = isOn ? 0.9f : (hover ? 0.45f : 0.25f);
    g.setColour (neonCyan.withAlpha (glowAlpha));
    g.drawRoundedRectangle (bounds, 6.0f, 1.6f);

    if (down && hover)
    {
        g.setColour (neonMagenta.withAlpha (0.25f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (isOn ? 0.95f : 0.75f));
    g.setFont (juce::Font (14.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);

    if (isOn)
    {
        juce::Rectangle<float> indicator (bounds.getRight() - 18.0f, bounds.getCentreY() - 6.0f, 12.0f, 12.0f);
        juce::Colour indicatorColour = neonCyan.withAlpha (hover ? 0.95f : 0.8f);
        g.setColour (indicatorColour);
        g.fillEllipse (indicator);
    }
}

void NeonScopeAudioProcessorEditor::configureNeonKnob (juce::Slider& slider,
                                                       juce::Label& label,
                                                       const juce::String& name)
{
    slider.setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentPrimary);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.25f));
    slider.setColour (juce::Slider::thumbColourId, accentSecondary);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (juce::Font (14.0f, juce::Font::bold));
}

void NeonScopeAudioProcessorEditor::refreshKnobLabels()
{
    auto setLabel = [] (juce::Label& label, const juce::String& title, const juce::String& value)
    {
        label.setText (title + "\n" + value, juce::dontSendNotification);
    };

    setLabel (cutoffLabel, "Filter Cutoff", formatHz (static_cast<float> (cutoffSlider.getValue())));
    setLabel (resonanceLabel, "Filter Q", formatQ (static_cast<float> (resonanceSlider.getValue())));
    setLabel (driveLabel, "Drive", formatDrive (static_cast<float> (driveSlider.getValue())));
    setLabel (mixLabel, "Mix", formatPercent (static_cast<float> (mixSlider.getValue())));
    setLabel (outputLabel, "Output", formatDb (static_cast<float> (outputSlider.getValue())));
    setLabel (sensitivityLabel, "Sensitivity", formatSensitivity (static_cast<float> (sensitivitySlider.getValue())));
}
NeonScopeAudioProcessorEditor::NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&neonDialLook);

    configureNeonKnob (cutoffSlider, cutoffLabel, "Filter Cutoff");
    configureNeonKnob (resonanceSlider, resonanceLabel, "Filter Q");
    configureNeonKnob (driveSlider, driveLabel, "Drive");
    configureNeonKnob (mixSlider, mixLabel, "Mix");
    configureNeonKnob (outputSlider, outputLabel, "Output");
    configureNeonKnob (sensitivitySlider, sensitivityLabel, "Sensitivity");

    auto styleDial = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setNumDecimalPlacesToDisplay (2);
    };

    styleDial (cutoffSlider);
    styleDial (resonanceSlider);
    styleDial (driveSlider);
    styleDial (mixSlider);
    styleDial (outputSlider);
    styleDial (sensitivitySlider);
    mixSlider.setNumDecimalPlacesToDisplay (1);
    outputSlider.setNumDecimalPlacesToDisplay (1);
    sensitivitySlider.setNumDecimalPlacesToDisplay (1);

    cutoffSlider.textFromValueFunction = [] (double value) { return formatHz (static_cast<float> (value)); };
    resonanceSlider.textFromValueFunction = [] (double value) { return formatQ (static_cast<float> (value)); };
    driveSlider.textFromValueFunction = [] (double value) { return formatDrive (static_cast<float> (value)); };
    mixSlider.textFromValueFunction = [] (double value) { return formatPercent (static_cast<float> (value)); };
    outputSlider.textFromValueFunction = [] (double value) { return formatDb (static_cast<float> (value)); };
    sensitivitySlider.textFromValueFunction = [] (double value) { return formatSensitivity (static_cast<float> (value)); };

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
    configureCombo (monitorModeBox);

    modeBox.addItem ("Visualize Only", 1);
    modeBox.addItem ("Tone Filter", 2);
    modeBox.addItem ("Soft Distortion", 3);
    modeBox.addItem ("Hybrid", 4);

    filterTypeBox.addItem ("Low-pass", 1);
    filterTypeBox.addItem ("High-pass", 2);
    filterTypeBox.addItem ("Band-pass", 3);

    satModeBox.addItem ("Tanh", 1);
    satModeBox.addItem ("Soft", 2);
    satModeBox.addItem ("Tube", 3);
    satModeBox.addItem ("Arctan", 4);
    satModeBox.addItem ("Hard Clip", 5);
    satModeBox.addItem ("Foldback", 6);

    oversamplingBox.clear();
    oversamplingBox.addItem ("1x", 1);
    oversamplingBox.addItem ("1.3x", 2);
    oversamplingBox.addItem ("1.7x", 3);
    oversamplingBox.addItem ("2x", 4);
    oversamplingBox.addItem ("4x", 5);

    monitorModeBox.clear();
    monitorModeBox.addItem ("Stereo", 1);
    monitorModeBox.addItem ("Mono", 2);
    monitorModeBox.addItem ("Left", 3);
    monitorModeBox.addItem ("Right", 4);
    monitorModeBox.addItem ("Mid", 5);
    monitorModeBox.addItem ("Side", 6);

    addAndMakeVisible (modeBox);
    addAndMakeVisible (filterTypeBox);
    addAndMakeVisible (satModeBox);
    addAndMakeVisible (oversamplingBox);
    addAndMakeVisible (monitorModeBox);

    addAndMakeVisible (cutoffSlider);
    addAndMakeVisible (cutoffLabel);
    addAndMakeVisible (resonanceSlider);
    addAndMakeVisible (resonanceLabel);
    addAndMakeVisible (driveSlider);
    addAndMakeVisible (driveLabel);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (outputSlider);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (sensitivitySlider);
    addAndMakeVisible (sensitivityLabel);
    addAndMakeVisible (autoGainButton);
    addAndMakeVisible (limiterButton);
    addAndMakeVisible (bandListenButton);
    addAndMakeVisible (autoGainValueLabel);
    addAndMakeVisible (monitorModeLabel);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "mode", modeBox);
    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "filterType", filterTypeBox);
    satModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "satMode", satModeBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "oversampling", oversamplingBox);
    monitorModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getValueTreeState(), "monitorMode", monitorModeBox);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "cutoff", cutoffSlider);

    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "drive", driveSlider);

    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "resonance", resonanceSlider);

    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "mix", mixSlider);

    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "outputTrim", outputSlider);

    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), "sensitivity", sensitivitySlider);

    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getValueTreeState(), "AUTO_GAIN", autoGainButton);

    limiterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getValueTreeState(), "SAFETY_LIMITER", limiterButton);

    bandListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getValueTreeState(), "bandListen", bandListenButton);

    autoGainValueLabel.setJustificationType (juce::Justification::centred);
    autoGainValueLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    autoGainValueLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    autoGainValueLabel.setInterceptsMouseClicks (false, false);
    autoGainValueLabel.setText ("AG: +0.0 dB", juce::dontSendNotification);

    monitorModeLabel.setText ("Monitor Mode", juce::dontSendNotification);
    monitorModeLabel.setJustificationType (juce::Justification::centredLeft);
    monitorModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    monitorModeLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    monitorModeLabel.setInterceptsMouseClicks (false, false);

    setSize (760, 560);
    startTimerHz (60);
    refreshKnobLabels();
    updateVisualState();
}

NeonScopeAudioProcessorEditor::~NeonScopeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NeonScopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto drawPanel = [&] (juce::Rectangle<float> area, const juce::String& title) -> juce::Rectangle<float>
    {
        if (area.isEmpty())
            return {};

        auto panel = area;
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (panel, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (panel, 8.0f, 1.0f);

        auto content = panel.reduced (12.0f);
        auto header = content.removeFromTop (24.0f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (title, header, juce::Justification::left);
        return content;
    };

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (24.0f, juce::Font::bold));
    g.drawText ("NeonScope", titleBounds, juce::Justification::centredLeft);

    if (! spectrumBounds.isEmpty())
    {
        auto spectrumPanel = spectrumBounds;
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (spectrumPanel, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (spectrumPanel, 8.0f, 1.0f);

        auto spectrumArea = spectrumPanel.reduced (14.0f, 10.0f);
        const float barWidth = spectrumArea.getWidth() / static_cast<float> (NeonScopeAudioProcessor::numBands);
        const float spacing = 6.0f;

        for (int band = 0; band < NeonScopeAudioProcessor::numBands; ++band)
        {
            const float value = juce::jlimit (0.0f, 1.0f, bandCache[static_cast<size_t> (band)]);
            const float height = juce::jmax (3.0f, spectrumArea.getHeight() * value);

            juce::Rectangle<float> bar {
                spectrumArea.getX() + band * barWidth + spacing * 0.5f,
                spectrumArea.getBottom() - height,
                juce::jmax (2.0f, barWidth - spacing),
                height
            };

            g.setColour (accentPrimary.withAlpha (0.7f));
            g.fillRoundedRectangle (bar, 3.0f);

            g.setColour (accentPrimary.withAlpha (0.3f));
            g.drawRoundedRectangle (bar, 3.0f, 0.5f);
        }
    }

    juce::ignoreUnused (drawPanel (distortionBounds, "Distortion"));
    juce::ignoreUnused (drawPanel (settingsBounds, "Settings"));
    auto metersContent = drawPanel (metersBounds, "Meters");

    if (! metersContent.isEmpty())
    {
        auto legendArea = metersContent.removeFromTop (20.0f);
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        auto legendCell = legendArea.removeFromLeft (legendArea.getWidth() / 3);
        g.drawText ("Peak", legendCell, juce::Justification::centred);
        legendCell = legendArea.removeFromLeft (legendArea.getWidth() / 2);
        g.drawText ("RMS", legendCell, juce::Justification::centred);
        g.drawText ("Corr", legendArea, juce::Justification::centred);

        auto correlationArea = metersContent.removeFromBottom (80.0f);
        auto stereoArea = metersContent;

        auto dbToNorm = [] (float db, float minDb, float maxDb)
        {
            return juce::jlimit (0.0f, 1.0f, juce::jmap (db, minDb, maxDb, 0.0f, 1.0f));
        };

        auto drawMeter = [&] (juce::Rectangle<float> panel,
                              float rmsDb,
                              float peakDb,
                              float peakHoldNorm,
                              const juce::String& label,
                              bool showTicks)
        {
            if (panel.isEmpty())
                return;

            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (panel, 10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (panel, 10.0f, 1.0f);

            auto inner = panel.reduced (26.0f, 34.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (inner, 8.0f);

            const float fillNorm = dbToNorm (rmsDb, meterDbFloor, meterDbCeiling);
            const float peakNorm = dbToNorm (peakDb, meterDbFloor, peakDbCeiling);

            const float fillHeight = inner.getHeight() * fillNorm;
            auto fillRect = inner.withY (inner.getBottom() - fillHeight).withHeight (fillHeight);
            juce::ColourGradient gradient (accentPrimary, fillRect.getCentreX(), fillRect.getBottom(),
                                           accentSecondary, fillRect.getCentreX(), fillRect.getY(), false);
            gradient.addColour (0.5, accentTertiary);
            g.setGradientFill (gradient);
            g.fillRect (fillRect);

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            const float peakY = inner.getBottom() - inner.getHeight() * peakNorm;
            g.drawLine (inner.getX(), peakY, inner.getRight(), peakY, 1.0f);

            const float holdY = inner.getBottom() - inner.getHeight() * juce::jlimit (0.0f, 1.0f, peakHoldNorm);
            g.setColour (accentSecondary.brighter (0.15f));
            g.fillEllipse ({ inner.getCentreX() - 4.0f, holdY - 4.0f, 8.0f, 8.0f });

            if (showTicks)
            {
                g.setFont (juce::Font (12.0f));
                const auto& ticks = processor.getMeterTicks();
                for (auto db : ticks)
                {
                    const float tickNorm = dbToNorm (db, meterDbFloor, meterDbCeiling);
                    const float tickY = inner.getBottom() - inner.getHeight() * tickNorm;
                    g.setColour (juce::Colours::white.withAlpha (0.25f));
                    g.drawLine (inner.getX() - 8.0f, tickY, inner.getX(), tickY, 1.0f);
                    g.drawText (juce::String (db, 0) + " dB",
                                juce::Rectangle<float> (inner.getX() - 70.0f, tickY - 8.0f, 60.0f, 16.0f),
                                juce::Justification::right);
                }
            }

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font (16.0f, juce::Font::bold));
            g.drawText (label,
                        juce::Rectangle<float> (panel.getX(), panel.getY() + 6.0f, panel.getWidth(), 24.0f),
                        juce::Justification::centred);

            g.setFont (juce::Font (13.0f));
            const auto rmsText = formatDb (rmsDb);
            const auto peakText = formatDb (peakDb);
            g.drawText (rmsText + " RMS | " + peakText + " PK",
                        juce::Rectangle<float> (panel.getX(), panel.getBottom() - 24.0f, panel.getWidth(), 20.0f),
                        juce::Justification::centred);
        };

        auto leftArea = stereoArea.removeFromLeft (stereoArea.getWidth() * 0.5f).reduced (16.0f, 6.0f);
        auto rightArea = stereoArea.reduced (16.0f, 6.0f);

        drawMeter (leftArea, leftRmsDb, leftPeakDb, leftPeakHold, "Left", true);
        drawMeter (rightArea, rightRmsDb, rightPeakDb, rightPeakHold, "Right", false);

        if (! correlationArea.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (correlationArea, 10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (correlationArea, 10.0f, 1.0f);

            auto corrInner = correlationArea.reduced (16.0f, 12.0f);
            const float correlationNorm = juce::jlimit (0.0f, 1.0f, (correlationValue + 1.0f) * 0.5f);

            auto track = corrInner.withHeight (corrInner.getHeight() * 0.35f).withCentre (corrInner.getCentre());
            juce::Colour corrColour {};
            if (correlationNorm < 0.5f)
            {
                const float t = correlationNorm / 0.5f;
                corrColour = juce::Colours::red.interpolatedWith (accentPrimary, t).withAlpha (0.9f);
            }
            else
            {
                const float t = (correlationNorm - 0.5f) / 0.5f;
                corrColour = accentPrimary.interpolatedWith (accentTertiary, t).withAlpha (0.9f);
            }

            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawLine (track.getCentreX(), corrInner.getY(), track.getCentreX(), corrInner.getBottom(), 1.0f);

            auto positiveTrack = track.withTrimmedRight (track.getWidth() * (1.0f - correlationNorm));
            g.setColour (corrColour);
            g.fillRoundedRectangle (positiveTrack, 6.0f);

            const float indicatorX = track.getX() + track.getWidth() * correlationNorm;
            g.setColour (corrColour.brighter (0.2f));
            g.fillEllipse ({ indicatorX - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f });
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse ({ indicatorX - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f }, 1.5f);

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font (16.0f, juce::Font::bold));
            g.drawText ("Correlation", corrInner.removeFromTop (22.0f), juce::Justification::left);
            g.setFont (juce::Font (14.0f));
            g.drawText (juce::String (correlationValue, 1),
                        juce::Rectangle<float> (corrInner.getRight() - 60.0f, corrInner.getY() - 2.0f, 60.0f, 24.0f),
                        juce::Justification::right);

            auto widthRow = corrInner.removeFromBottom (20.0f);
            auto widthLabel = widthRow.removeFromLeft (70.0f);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("Width", widthLabel, juce::Justification::left);
            auto widthBar = widthRow.reduced (4.0f, 4.0f);
            auto widthFill = widthBar.withWidth (widthBar.getWidth() * juce::jlimit (0.0f, 1.0f, widthValue));
            g.setColour (accentTertiary.withAlpha (0.8f));
            g.drawRoundedRectangle (widthBar, 5.0f, 1.0f);
            g.fillRoundedRectangle (widthFill, 5.0f);
        }

        if (limiterFlash > 0.02f && ! metersBounds.isEmpty())
        {
            auto flashArea = metersBounds.reduced (20.0f, 4.0f);
            g.setColour (accentSecondary.withAlpha (0.1f + limiterFlash * 0.35f));
            g.fillRoundedRectangle (flashArea.removeFromTop (4.0f), 2.0f);
        }
    }
}

void NeonScopeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    titleBounds = bounds.removeFromTop (50).toFloat();
    spectrumBounds = bounds.removeFromTop (110).toFloat().reduced (12.0f, 6.0f);

    auto content = bounds.reduced (12);
    auto topRow = content.removeFromTop (200);
    auto distortionArea = topRow.removeFromLeft (juce::roundToInt (topRow.getWidth() * 0.55f));
    distortionBounds = distortionArea.toFloat();
    settingsBounds = topRow.toFloat();

    content.removeFromTop (6);
    metersBounds = content.toFloat();

    constexpr int panelHeaderOffset = 28;
    auto distortionContent = distortionArea.reduced (8);
    distortionContent.removeFromTop (panelHeaderOffset);
    constexpr int toggleRowHeight = 38;
    auto toggleRow = distortionContent.removeFromBottom (toggleRowHeight);
    const int dialCount = 4;
    const int dialWidth = juce::jmax (1, distortionContent.getWidth() / dialCount);

    auto layoutDial = [] (juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label)
    {
        constexpr int labelHeight = 40;
        auto sliderArea = area.removeFromTop (juce::jmax (20, area.getHeight() - labelHeight)).reduced (4, 4);
        slider.setBounds (sliderArea);
        label.setBounds (area);
    };

    auto placeDial = [&] (int index, juce::Slider& slider, juce::Label& label)
    {
        auto cell = juce::Rectangle<int> (distortionContent.getX() + index * dialWidth,
                                          distortionContent.getY(),
                                          (index == dialCount - 1)
                                              ? distortionContent.getRight() - (distortionContent.getX() + index * dialWidth)
                                              : dialWidth,
                                          distortionContent.getHeight());
        layoutDial (cell, slider, label);
    };

    placeDial (0, driveSlider, driveLabel);
    placeDial (1, mixSlider, mixLabel);
    placeDial (2, outputSlider, outputLabel);
    placeDial (3, sensitivitySlider, sensitivityLabel);

    auto settingsContent = settingsBounds.reduced (12.0f).toNearestInt();
    settingsContent.removeFromTop (panelHeaderOffset);
    constexpr int comboRowHeight = 32;
    constexpr int comboSpacing = 8;

    auto layoutComboRow = [] (juce::Rectangle<int> area, juce::ComboBox& left, juce::ComboBox& right)
    {
        auto padded = area.reduced (6, 0);
        auto leftArea = padded.removeFromLeft (padded.getWidth() / 2);
        left.setBounds (leftArea.reduced (3, 0));
        right.setBounds (padded.reduced (3, 0));
    };

    auto row1 = settingsContent.removeFromTop (comboRowHeight);
    layoutComboRow (row1, modeBox, filterTypeBox);
    settingsContent.removeFromTop (comboSpacing);

    auto row2 = settingsContent.removeFromTop (comboRowHeight);
    layoutComboRow (row2, satModeBox, oversamplingBox);
    settingsContent.removeFromTop (comboSpacing);

    auto row3 = settingsContent.removeFromTop (comboRowHeight);
    auto monitorLabelArea = row3.removeFromLeft (90);
    monitorModeLabel.setBounds (monitorLabelArea);
    auto monitorComboArea = row3.removeFromLeft (juce::roundToInt (row3.getWidth() * 0.5f));
    monitorModeBox.setBounds (monitorComboArea.reduced (3, 0));
    bandListenButton.setBounds (row3.reduced (3, 0));
    settingsContent.removeFromTop (comboSpacing);

    auto layoutKnob = [] (juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label)
    {
        constexpr int labelHeight = 34;
        auto sliderArea = area.removeFromTop (juce::jmax (18, area.getHeight() - labelHeight)).reduced (8, 3);
        slider.setBounds (sliderArea);
        label.setBounds (area);
    };

    auto filterArea = settingsContent;
    const int filterCellWidth = juce::jmax (1, filterArea.getWidth() / 2);

    auto placeFilterKnob = [&] (int index, juce::Slider& slider, juce::Label& label)
    {
        auto cell = juce::Rectangle<int> (filterArea.getX() + index * filterCellWidth,
                                          filterArea.getY(),
                                          (index == 1)
                                              ? filterArea.getRight() - (filterArea.getX() + index * filterCellWidth)
                                              : filterCellWidth,
                                          filterArea.getHeight());
        layoutKnob (cell, slider, label);
    };

    placeFilterKnob (0, cutoffSlider, cutoffLabel);
    placeFilterKnob (1, resonanceSlider, resonanceLabel);

    const int toggleCellWidth = toggleRow.getWidth() / 3;
    auto autoGainArea = toggleRow.removeFromLeft (toggleCellWidth).reduced (3, 3);
    auto autoGainValueArea = toggleRow.removeFromRight (toggleCellWidth).reduced (3, 3);
    auto limiterArea = toggleRow.reduced (3, 3);
    autoGainButton.setBounds (autoGainArea);
    limiterButton.setBounds (limiterArea);
    autoGainValueLabel.setBounds (autoGainValueArea);
}

void NeonScopeAudioProcessorEditor::timerCallback()
{
    updateVisualState();
    refreshKnobLabels();
    repaint();
}

void NeonScopeAudioProcessorEditor::updateVisualState()
{
    bandCache = processor.getBands();
    leftLevel = processor.getLeftLevel();
    rightLevel = processor.getRightLevel();
    leftPeakDb = processor.getLeftPeakDb();
    rightPeakDb = processor.getRightPeakDb();
    leftRmsDb = processor.getLeftRmsDb();
    rightRmsDb = processor.getRightRmsDb();
    correlationValue = processor.getCorrelationValue();
    widthValue = processor.getWidthValue();
    autoGainDb = processor.getAutoGainDb();
    limiterReduction = processor.getLimiterReductionDb();
    globalRmsPulse = processor.getGlobalRmsLevel();

    constexpr int peakHoldFrames = 18;
    auto normalisePeak = [] (float db)
    {
        return juce::jlimit (0.0f, 1.0f, juce::jmap (db, meterDbFloor, peakDbCeiling, 0.0f, 1.0f));
    };

    auto updateHold = [normalisePeak, peakHoldFrames] (float db, float& holdValue, int& timer)
    {
        const float incoming = normalisePeak (db);
        if (incoming >= holdValue)
        {
            holdValue = incoming;
            timer = peakHoldFrames;
            return;
        }

        if (timer > 0)
        {
            --timer;
            return;
        }

        holdValue = juce::jmax (0.0f, holdValue - 0.01f);
    };

    updateHold (leftPeakDb, leftPeakHold, leftPeakHoldTimer);
    updateHold (rightPeakDb, rightPeakHold, rightPeakHoldTimer);

    const float flashTarget = limiterReduction < -0.1f ? juce::jlimit (0.0f, 1.0f, -limiterReduction / 6.0f) : 0.0f;
    limiterFlash = limiterFlash * 0.6f + flashTarget * 0.4f;
    autoGainValueLabel.setText ("AG: " + formatDb (autoGainDb), juce::dontSendNotification);

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

    auto setKnobState = [] (juce::Slider& slider, juce::Label& label, bool active)
    {
        slider.setEnabled (active);
        label.setEnabled (active);
        const float alpha = active ? 1.0f : 0.35f;
        slider.setAlpha (alpha);
        label.setAlpha (alpha);
    };

    setControlState (filterTypeBox, filterActive);
    setKnobState (cutoffSlider, cutoffLabel, filterActive);
    setKnobState (resonanceSlider, resonanceLabel, filterActive);

    setKnobState (driveSlider, driveLabel, distortionActive);
    setControlState (satModeBox, distortionActive);
    setControlState (oversamplingBox, distortionActive);
    setKnobState (mixSlider, mixLabel, distortionActive);
    setKnobState (outputSlider, outputLabel, processingActive);
    setKnobState (sensitivitySlider, sensitivityLabel, true);

    autoGainButton.setAlpha (distortionActive ? 1.0f : 0.5f);
    limiterButton.setAlpha (processingActive ? 1.0f : 0.7f);
    bandListenButton.setAlpha (filterActive ? 1.0f : 0.4f);
    autoGainValueLabel.setAlpha (distortionActive ? 1.0f : 0.5f);
}
