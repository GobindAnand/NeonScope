#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr float meterDbFloor   = -60.0f;
    constexpr float meterDbCeiling = 0.0f;
    constexpr float peakDbCeiling  = 6.0f;

    static juce::String formatHz (float v)
    {
        if (v >= 1000.0f)
            return juce::String (v / 1000.0f, 2) + " kHz";
        return juce::String (v, 0) + " Hz";
    }

    static juce::String formatQ (float v)       { return juce::String (v, 2) + " Q"; }
    static juce::String formatPercent (float v)  { return juce::String (v * 100.0f, 1) + "%"; }
    static juce::String formatDrive (float v)    { return juce::String (v, 1) + "x"; }

    static juce::String formatDb (float v)
    {
        auto sign = v >= 0.0f ? "+" : "";
        return sign + juce::String (v, 1) + " dB";
    }

    static juce::String formatSensitivity (float v)
    {
        return formatDb (juce::Decibels::gainToDecibels (v, -60.0f));
    }

    static float dbToNorm (float db, float minDb, float maxDb)
    {
        return juce::jlimit (0.0f, 1.0f, juce::jmap (db, minDb, maxDb, 0.0f, 1.0f));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  ScopeLookAndFeel
// ═══════════════════════════════════════════════════════════════════════════════

ScopeLookAndFeel::ScopeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Theme::textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, Theme::textPrimary);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha (0.15f));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::textPrimary);
    setColour (juce::PopupMenu::textColourId, Theme::textSecondary);
}

void ScopeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (side, side);

    const auto centre = bounds.getCentre();
    const auto radius = side * 0.5f;
    const bool hover = slider.isMouseOverOrDragging();

    // Face
    g.setColour (Theme::knobFace);
    g.fillEllipse (bounds);

    // Outer border
    g.setColour (hover ? Theme::borderLight : Theme::border);
    g.drawEllipse (bounds, 1.2f);

    // Track arc (background)
    const auto arcRadius = radius - 5.0f;
    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                         0.0f, startAngle, endAngle, true);
    g.setColour (Theme::border);
    g.strokePath (bgArc, juce::PathStrokeType (2.5f));

    // Value arc (accent)
    const auto angle = startAngle + (endAngle - startAngle) * pos;
    if (pos > 0.001f)
    {
        juce::Path valArc;
        valArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                              0.0f, startAngle, angle, true);
        g.setColour (hover ? Theme::accent : Theme::accentDim);
        g.strokePath (valArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    // Indicator line
    const auto lineLen = radius - 8.0f;
    const auto lineStart = 0.35f * lineLen;
    const juce::Point<float> p1 {
        centre.x + std::cos (angle) * lineStart,
        centre.y + std::sin (angle) * lineStart
    };
    const juce::Point<float> p2 {
        centre.x + std::cos (angle) * lineLen,
        centre.y + std::sin (angle) * lineLen
    };
    g.setColour (Theme::textPrimary.withAlpha (hover ? 0.95f : 0.8f));
    g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.6f);
}

void ScopeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float /*min*/, float /*max*/,
                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (! slider.isHorizontal())
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    auto track = bounds.withHeight (4.0f).withCentre ({ bounds.getCentreX(), bounds.getCentreY() });

    g.setColour (Theme::border);
    g.fillRoundedRectangle (track, 2.0f);

    const auto thumbX = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    auto fill = track.withRight (thumbX);
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (fill, 2.0f);

    g.setColour (Theme::textPrimary);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ thumbX, track.getCentreY() }));
}

void ScopeLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool /*isDown*/,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
    const bool hover = box.isMouseOver (true);

    g.setColour (hover ? Theme::panelHover : Theme::panel);
    g.fillRoundedRectangle (bounds, Theme::cornerRadius);
    g.setColour (hover ? Theme::borderLight : Theme::border);
    g.drawRoundedRectangle (bounds, Theme::cornerRadius, 1.0f);

    auto arrowArea = juce::Rectangle<float> ((float) buttonX, (float) buttonY,
                                             (float) buttonW, (float) buttonH);
    auto ac = arrowArea.getCentre();
    juce::Path arrow;
    arrow.addTriangle (ac.x - 4.0f, ac.y - 1.5f, ac.x + 4.0f, ac.y - 1.5f, ac.x, ac.y + 3.0f);
    g.setColour (Theme::textSecondary);
    g.fillPath (arrow);
}

void ScopeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool highlighted, bool /*down*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = button.getToggleState();
    const bool hover = highlighted || button.isMouseOver();

    g.setColour (on ? Theme::accent.withAlpha (0.12f) : Theme::panel);
    g.fillRoundedRectangle (bounds, Theme::cornerRadius);

    g.setColour (on ? Theme::accent.withAlpha (0.7f) : (hover ? Theme::borderLight : Theme::border));
    g.drawRoundedRectangle (bounds, Theme::cornerRadius, 1.0f);

    g.setColour (on ? Theme::accent : Theme::textSecondary);
    g.setFont (juce::Font (Theme::labelSize, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Editor — Construction
// ═══════════════════════════════════════════════════════════════════════════════

void NeonScopeAudioProcessorEditor::configureKnob (juce::Slider& slider,
                                                     juce::Label& label,
                                                     const juce::String& name)
{
    slider.setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setNumDecimalPlacesToDisplay (1);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Theme::textSecondary);
    label.setFont (juce::Font (Theme::labelSize));
}

void NeonScopeAudioProcessorEditor::refreshKnobLabels()
{
    auto set = [] (juce::Label& l, const juce::String& title, const juce::String& value)
    {
        l.setText (title + "\n" + value, juce::dontSendNotification);
    };
    set (cutoffLabel, "Cutoff", formatHz ((float) cutoffSlider.getValue()));
    set (resonanceLabel, "Q", formatQ ((float) resonanceSlider.getValue()));
    set (driveLabel, "Drive", formatDrive ((float) driveSlider.getValue()));
    set (mixLabel, "Mix", formatPercent ((float) mixSlider.getValue()));
    set (outputLabel, "Output", formatDb ((float) outputSlider.getValue()));
    set (sensitivityLabel, "Sensitivity", formatSensitivity ((float) sensitivitySlider.getValue()));
}

NeonScopeAudioProcessorEditor::NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&scopeLnf);

    configureKnob (cutoffSlider, cutoffLabel, "Cutoff");
    configureKnob (resonanceSlider, resonanceLabel, "Q");
    configureKnob (driveSlider, driveLabel, "Drive");
    configureKnob (mixSlider, mixLabel, "Mix");
    configureKnob (outputSlider, outputLabel, "Output");
    configureKnob (sensitivitySlider, sensitivityLabel, "Sensitivity");

    cutoffSlider.textFromValueFunction = [] (double v) { return formatHz ((float) v); };
    resonanceSlider.textFromValueFunction = [] (double v) { return formatQ ((float) v); };
    driveSlider.textFromValueFunction = [] (double v) { return formatDrive ((float) v); };
    mixSlider.textFromValueFunction = [] (double v) { return formatPercent ((float) v); };
    outputSlider.textFromValueFunction = [] (double v) { return formatDb ((float) v); };
    sensitivitySlider.textFromValueFunction = [] (double v) { return formatSensitivity ((float) v); };

    auto configureCombo = [] (juce::ComboBox& box)
    {
        box.setJustificationType (juce::Justification::centredLeft);
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

    oversamplingBox.addItem ("1x", 1);
    oversamplingBox.addItem ("1.3x", 2);
    oversamplingBox.addItem ("1.7x", 3);
    oversamplingBox.addItem ("2x", 4);
    oversamplingBox.addItem ("4x", 5);

    monitorModeBox.addItem ("Stereo", 1);
    monitorModeBox.addItem ("Mono", 2);
    monitorModeBox.addItem ("Left", 3);
    monitorModeBox.addItem ("Right", 4);
    monitorModeBox.addItem ("Mid", 5);
    monitorModeBox.addItem ("Side", 6);

    for (auto* c : std::initializer_list<juce::Component*> {
             &modeBox, &filterTypeBox, &satModeBox, &oversamplingBox, &monitorModeBox,
             &cutoffSlider, &cutoffLabel, &resonanceSlider, &resonanceLabel,
             &driveSlider, &driveLabel, &mixSlider, &mixLabel,
             &outputSlider, &outputLabel, &sensitivitySlider, &sensitivityLabel,
             &autoGainButton, &limiterButton, &bandListenButton,
             &autoGainValueLabel, &monitorModeLabel })
        addAndMakeVisible (c);

    auto& vts = processor.getValueTreeState();
    modeAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "mode", modeBox);
    filterTypeAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "filterType", filterTypeBox);
    satModeAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "satMode", satModeBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "oversampling", oversamplingBox);
    monitorModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "monitorMode", monitorModeBox);
    cutoffAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "cutoff", cutoffSlider);
    driveAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "drive", driveSlider);
    resonanceAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "resonance", resonanceSlider);
    mixAttachment         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "mix", mixSlider);
    outputTrimAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "outputTrim", outputSlider);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "sensitivity", sensitivitySlider);
    autoGainAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "AUTO_GAIN", autoGainButton);
    limiterAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "SAFETY_LIMITER", limiterButton);
    bandListenAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "bandListen", bandListenButton);

    autoGainValueLabel.setJustificationType (juce::Justification::centred);
    autoGainValueLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    autoGainValueLabel.setFont (juce::Font (Theme::valueSize, juce::Font::bold));
    autoGainValueLabel.setInterceptsMouseClicks (false, false);
    autoGainValueLabel.setText ("AG: +0.0 dB", juce::dontSendNotification);

    monitorModeLabel.setText ("Monitor", juce::dontSendNotification);
    monitorModeLabel.setJustificationType (juce::Justification::centredLeft);
    monitorModeLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    monitorModeLabel.setFont (juce::Font (Theme::labelSize));
    monitorModeLabel.setInterceptsMouseClicks (false, false);

    setSize (760, 540);
    startTimerHz (60);
    refreshKnobLabels();
    updateVisualState();
}

NeonScopeAudioProcessorEditor::~NeonScopeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Editor — Drawing Helpers
// ═══════════════════════════════════════════════════════════════════════════════

void NeonScopeAudioProcessorEditor::drawPanel (juce::Graphics& g,
                                                 juce::Rectangle<float> area,
                                                 const juce::String& title)
{
    if (area.isEmpty()) return;

    g.setColour (Theme::panel);
    g.fillRoundedRectangle (area, Theme::cornerRadius);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (area, Theme::cornerRadius, 1.0f);

    if (title.isNotEmpty())
    {
        auto header = area.reduced (14.0f, 0.0f).removeFromTop (32.0f);
        g.setColour (Theme::textSecondary);
        g.setFont (juce::Font (Theme::sectionSize, juce::Font::bold));
        g.drawText (title.toUpperCase(), header, juce::Justification::centredLeft);

        g.setColour (Theme::border);
        g.drawLine (area.getX() + 14.0f, area.getY() + 32.0f,
                    area.getRight() - 14.0f, area.getY() + 32.0f, 1.0f);
    }
}

void NeonScopeAudioProcessorEditor::drawSpectrum (juce::Graphics& g)
{
    if (spectrumBounds.isEmpty()) return;

    g.setColour (Theme::panel);
    g.fillRoundedRectangle (spectrumBounds, Theme::cornerRadius);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (spectrumBounds, Theme::cornerRadius, 1.0f);

    auto area = spectrumBounds.reduced (12.0f, 8.0f);
    const float barW = area.getWidth() / (float) NeonScopeAudioProcessor::numBands;
    const float gap = 4.0f;

    for (int i = 0; i < NeonScopeAudioProcessor::numBands; ++i)
    {
        const float val = juce::jlimit (0.0f, 1.0f, bandCache[(size_t) i]);
        const float h = juce::jmax (2.0f, area.getHeight() * val);

        juce::Rectangle<float> bar {
            area.getX() + i * barW + gap * 0.5f,
            area.getBottom() - h,
            juce::jmax (1.0f, barW - gap),
            h
        };

        g.setColour (Theme::accent.withAlpha (0.15f + val * 0.55f));
        g.fillRoundedRectangle (bar, 2.0f);
    }
}

void NeonScopeAudioProcessorEditor::drawSingleMeter (juce::Graphics& g,
                                                       juce::Rectangle<float> area,
                                                       float rmsDb, float peakDb,
                                                       float holdNorm,
                                                       const juce::String& label)
{
    if (area.isEmpty()) return;

    // Label
    auto labelArea = area.removeFromTop (18.0f);
    g.setColour (Theme::textSecondary);
    g.setFont (juce::Font (Theme::labelSize));
    g.drawText (label, labelArea, juce::Justification::centred);

    // Value readout
    auto readout = area.removeFromBottom (16.0f);
    g.setFont (juce::Font (10.0f));
    g.setColour (Theme::textSecondary);
    g.drawText (juce::String (rmsDb, 1) + " dB", readout, juce::Justification::centred);

    auto meterArea = area.reduced (0.0f, 4.0f);
    auto inner = meterArea.withSizeKeepingCentre (juce::jmin (14.0f, meterArea.getWidth()), meterArea.getHeight());

    // Background track
    g.setColour (Theme::knobFace);
    g.fillRoundedRectangle (inner, 3.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (inner, 3.0f, 0.5f);

    // RMS fill
    const float rmsNorm = dbToNorm (rmsDb, meterDbFloor, meterDbCeiling);
    const float fillH = inner.getHeight() * rmsNorm;
    auto fillRect = inner.withTop (inner.getBottom() - fillH);
    g.setColour (Theme::accent.withAlpha (0.8f));
    g.fillRoundedRectangle (fillRect, 2.0f);

    // Peak line
    const float peakNorm = dbToNorm (peakDb, meterDbFloor, peakDbCeiling);
    const float peakY = inner.getBottom() - inner.getHeight() * peakNorm;
    g.setColour (Theme::textPrimary.withAlpha (0.7f));
    g.drawLine (inner.getX(), peakY, inner.getRight(), peakY, 1.0f);

    // Peak hold dot
    const float holdY = inner.getBottom() - inner.getHeight() * juce::jlimit (0.0f, 1.0f, holdNorm);
    g.setColour (Theme::accent);
    g.fillEllipse ({ inner.getCentreX() - 3.0f, holdY - 3.0f, 6.0f, 6.0f });

    // Tick marks
    g.setFont (juce::Font (9.0f));
    for (auto db : { 0.0f, -6.0f, -12.0f, -30.0f, -60.0f })
    {
        const float n = dbToNorm (db, meterDbFloor, meterDbCeiling);
        const float ty = inner.getBottom() - inner.getHeight() * n;
        g.setColour (Theme::border);
        g.drawLine (inner.getRight() + 2.0f, ty, inner.getRight() + 6.0f, ty, 0.5f);
    }
}

void NeonScopeAudioProcessorEditor::drawCorrelation (juce::Graphics& g,
                                                       juce::Rectangle<float> area)
{
    if (area.isEmpty()) return;

    auto labelRow = area.removeFromTop (18.0f);
    g.setColour (Theme::textSecondary);
    g.setFont (juce::Font (Theme::labelSize));
    g.drawText ("Correlation", labelRow.removeFromLeft (labelRow.getWidth() * 0.6f),
                juce::Justification::left);
    g.drawText (juce::String (correlationValue, 2), labelRow, juce::Justification::right);

    area.removeFromTop (4.0f);

    // Track
    auto track = area.removeFromTop (8.0f);
    g.setColour (Theme::knobFace);
    g.fillRoundedRectangle (track, 4.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (track, 4.0f, 0.5f);

    // Center line
    g.setColour (Theme::border);
    g.drawLine (track.getCentreX(), track.getY(), track.getCentreX(), track.getBottom(), 1.0f);

    // Fill from center
    const float corrNorm = juce::jlimit (0.0f, 1.0f, (correlationValue + 1.0f) * 0.5f);
    const float indicatorX = track.getX() + track.getWidth() * corrNorm;

    juce::Colour corrColour = correlationValue > 0.0f
        ? Theme::accent.withAlpha (0.8f)
        : Theme::danger.withAlpha (0.8f);

    auto fillBar = corrNorm >= 0.5f
        ? juce::Rectangle<float> (track.getCentreX(), track.getY(),
                                   indicatorX - track.getCentreX(), track.getHeight())
        : juce::Rectangle<float> (indicatorX, track.getY(),
                                   track.getCentreX() - indicatorX, track.getHeight());
    g.setColour (corrColour);
    g.fillRect (fillBar);

    g.fillEllipse ({ indicatorX - 5.0f, track.getCentreY() - 5.0f, 10.0f, 10.0f });

    area.removeFromTop (8.0f);

    // Width
    auto widthLabel = area.removeFromTop (16.0f);
    g.setColour (Theme::textSecondary);
    g.setFont (juce::Font (Theme::labelSize));
    g.drawText ("Width", widthLabel.removeFromLeft (50.0f), juce::Justification::left);
    g.drawText (juce::String (widthValue, 2), widthLabel, juce::Justification::right);

    auto widthTrack = area.removeFromTop (6.0f).reduced (0.0f, 1.0f);
    g.setColour (Theme::knobFace);
    g.fillRoundedRectangle (widthTrack, 3.0f);
    auto widthFill = widthTrack.withWidth (widthTrack.getWidth() * juce::jlimit (0.0f, 1.0f, widthValue));
    g.setColour (Theme::accent.withAlpha (0.65f));
    g.fillRoundedRectangle (widthFill, 3.0f);
}

void NeonScopeAudioProcessorEditor::drawMeters (juce::Graphics& g,
                                                  juce::Rectangle<float> area)
{
    if (area.isEmpty()) return;

    drawPanel (g, area, "Meters");
    auto content = area.reduced (14.0f).withTrimmedTop (36.0f);

    auto corrArea = content.removeFromBottom (70.0f);
    content.removeFromBottom (6.0f);

    auto leftArea = content.removeFromLeft (content.getWidth() * 0.5f);
    auto rightArea = content;

    drawSingleMeter (g, leftArea, leftRmsDb, leftPeakDb, leftPeakHold, "L");
    drawSingleMeter (g, rightArea, rightRmsDb, rightPeakDb, rightPeakHold, "R");
    drawCorrelation (g, corrArea.reduced (6.0f, 0.0f));

    // Limiter activity indicator
    if (limiterFlash > 0.02f)
    {
        auto flashBar = area.reduced (14.0f, 0.0f).removeFromTop (2.0f).translated (0.0f, 33.0f);
        g.setColour (Theme::danger.withAlpha (limiterFlash * 0.8f));
        g.fillRect (flashBar);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Editor — Paint
// ═══════════════════════════════════════════════════════════════════════════════

void NeonScopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    // Title
    g.setColour (Theme::textPrimary);
    g.setFont (juce::Font (Theme::titleSize, juce::Font::bold));
    g.drawText ("NeonScope", titleBounds.reduced (14.0f, 0.0f), juce::Justification::centredLeft);

    g.setColour (Theme::textSecondary);
    g.setFont (juce::Font (Theme::labelSize));
    g.drawText ("v2.0", titleBounds.reduced (14.0f, 0.0f), juce::Justification::centredRight);

    drawSpectrum (g);
    drawPanel (g, distortionBounds, "Distortion");
    drawPanel (g, settingsBounds, "Settings");
    drawMeters (g, metersBounds);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Editor — Layout
// ═══════════════════════════════════════════════════════════════════════════════

void NeonScopeAudioProcessorEditor::resized()
{
    const auto M = (int) Theme::margin;
    auto bounds = getLocalBounds().reduced (M);

    titleBounds = bounds.removeFromTop (40).toFloat();
    spectrumBounds = bounds.removeFromTop (100).toFloat();
    bounds.removeFromTop (M);

    // Controls row
    auto topRow = bounds.removeFromTop (210);
    auto distortionArea = topRow.removeFromLeft (juce::roundToInt (topRow.getWidth() * 0.55f));
    topRow.removeFromLeft (M);
    distortionBounds = distortionArea.toFloat();
    settingsBounds = topRow.toFloat();

    bounds.removeFromTop (M);
    metersBounds = bounds.toFloat();

    // ── Distortion panel internals ──
    auto dContent = distortionArea.reduced (12);
    dContent.removeFromTop (36);  // header
    auto toggleRow = dContent.removeFromBottom (34);

    const int dialW = dContent.getWidth() / 4;
    auto placeDial = [&] (int i, juce::Slider& s, juce::Label& l)
    {
        auto cell = juce::Rectangle<int> (
            dContent.getX() + i * dialW, dContent.getY(),
            (i == 3) ? dContent.getRight() - (dContent.getX() + 3 * dialW) : dialW,
            dContent.getHeight());
        auto knobArea = cell.removeFromTop (cell.getHeight() - 32).reduced (4, 2);
        s.setBounds (knobArea);
        l.setBounds (cell);
    };
    placeDial (0, driveSlider, driveLabel);
    placeDial (1, mixSlider, mixLabel);
    placeDial (2, outputSlider, outputLabel);
    placeDial (3, sensitivitySlider, sensitivityLabel);

    const int tw = toggleRow.getWidth() / 3;
    autoGainButton.setBounds (toggleRow.removeFromLeft (tw).reduced (2));
    limiterButton.setBounds (toggleRow.removeFromLeft (tw).reduced (2));
    autoGainValueLabel.setBounds (toggleRow.reduced (2));

    // ── Settings panel internals ──
    auto sContent = settingsBounds.reduced (12.0f).toNearestInt();
    sContent.removeFromTop (36);  // header
    constexpr int rowH = 30;
    constexpr int gap = 6;

    auto layoutRow = [] (juce::Rectangle<int> row, juce::ComboBox& a, juce::ComboBox& b)
    {
        auto left = row.removeFromLeft (row.getWidth() / 2);
        a.setBounds (left.reduced (2, 0));
        b.setBounds (row.reduced (2, 0));
    };
    layoutRow (sContent.removeFromTop (rowH), modeBox, filterTypeBox);
    sContent.removeFromTop (gap);
    layoutRow (sContent.removeFromTop (rowH), satModeBox, oversamplingBox);
    sContent.removeFromTop (gap);

    auto monRow = sContent.removeFromTop (rowH);
    monitorModeLabel.setBounds (monRow.removeFromLeft (60));
    monitorModeBox.setBounds (monRow.removeFromLeft (monRow.getWidth() / 2).reduced (2, 0));
    bandListenButton.setBounds (monRow.reduced (2, 0));
    sContent.removeFromTop (gap);

    // Filter knobs
    auto filterArea = sContent;
    const int fKnobW = filterArea.getWidth() / 2;
    auto placeFKnob = [&] (int i, juce::Slider& s, juce::Label& l)
    {
        auto cell = juce::Rectangle<int> (
            filterArea.getX() + i * fKnobW, filterArea.getY(),
            (i == 1) ? filterArea.getRight() - (filterArea.getX() + fKnobW) : fKnobW,
            filterArea.getHeight());
        auto knobArea = cell.removeFromTop (cell.getHeight() - 28).reduced (8, 2);
        s.setBounds (knobArea);
        l.setBounds (cell);
    };
    placeFKnob (0, cutoffSlider, cutoffLabel);
    placeFKnob (1, resonanceSlider, resonanceLabel);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Editor — Timer / State
// ═══════════════════════════════════════════════════════════════════════════════

void NeonScopeAudioProcessorEditor::timerCallback()
{
    updateVisualState();
    refreshKnobLabels();
    repaint();
}

void NeonScopeAudioProcessorEditor::updateVisualState()
{
    bandCache        = processor.getBands();
    leftLevel        = processor.getLeftLevel();
    rightLevel       = processor.getRightLevel();
    leftPeakDb       = processor.getLeftPeakDb();
    rightPeakDb      = processor.getRightPeakDb();
    leftRmsDb        = processor.getLeftRmsDb();
    rightRmsDb       = processor.getRightRmsDb();
    correlationValue = processor.getCorrelationValue();
    widthValue       = processor.getWidthValue();
    autoGainDb       = processor.getAutoGainDb();
    limiterReduction = processor.getLimiterReductionDb();
    globalRmsPulse   = processor.getGlobalRmsLevel();

    // Peak hold (~300ms at 60fps)
    auto updateHold = [] (float db, float& hold, int& timer)
    {
        const float incoming = juce::jlimit (0.0f, 1.0f,
            juce::jmap (db, meterDbFloor, peakDbCeiling, 0.0f, 1.0f));
        if (incoming >= hold) { hold = incoming; timer = 18; return; }
        if (timer > 0) { --timer; return; }
        hold = juce::jmax (0.0f, hold - 0.01f);
    };
    updateHold (leftPeakDb, leftPeakHold, leftPeakHoldTimer);
    updateHold (rightPeakDb, rightPeakHold, rightPeakHoldTimer);

    // Limiter flash
    const float flashTarget = limiterReduction < -0.1f
        ? juce::jlimit (0.0f, 1.0f, -limiterReduction / 6.0f) : 0.0f;
    limiterFlash = limiterFlash * 0.6f + flashTarget * 0.4f;

    autoGainValueLabel.setText ("AG: " + formatDb (autoGainDb), juce::dontSendNotification);

    // Mode-driven enable/disable
    const auto* modeParam = processor.getValueTreeState().getRawParameterValue ("mode");
    const int modeVal = modeParam ? juce::roundToInt (modeParam->load()) : 0;
    const bool processing = modeVal != 0;
    const bool filterOn   = modeVal == 1 || modeVal == 3;
    const bool distOn     = modeVal == 2 || modeVal == 3;

    auto setActive = [] (juce::Component& c, bool on)
    {
        c.setEnabled (on);
        c.setAlpha (on ? 1.0f : 0.35f);
    };
    auto setKnob = [] (juce::Slider& s, juce::Label& l, bool on)
    {
        s.setEnabled (on); l.setEnabled (on);
        s.setAlpha (on ? 1.0f : 0.3f);
        l.setAlpha (on ? 1.0f : 0.3f);
    };

    setActive (filterTypeBox, filterOn);
    setKnob (cutoffSlider, cutoffLabel, filterOn);
    setKnob (resonanceSlider, resonanceLabel, filterOn);
    setKnob (driveSlider, driveLabel, distOn);
    setActive (satModeBox, distOn);
    setActive (oversamplingBox, distOn);
    setKnob (mixSlider, mixLabel, distOn);
    setKnob (outputSlider, outputLabel, processing);
    setKnob (sensitivitySlider, sensitivityLabel, true);

    autoGainButton.setAlpha (distOn ? 1.0f : 0.4f);
    limiterButton.setAlpha (processing ? 1.0f : 0.6f);
    bandListenButton.setAlpha (filterOn ? 1.0f : 0.35f);
    autoGainValueLabel.setAlpha (distOn ? 1.0f : 0.4f);
}
