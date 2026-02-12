#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─── Design Tokens ──────────────────────────────────────────────────────────
namespace Theme
{
    inline const juce::Colour background   { 0xff0E0F12 };
    inline const juce::Colour panel        { 0xff171A20 };
    inline const juce::Colour panelHover   { 0xff1D2028 };
    inline const juce::Colour border       { 0xff2A2F3A };
    inline const juce::Colour borderLight  { 0xff3A4050 };
    inline const juce::Colour textPrimary  { 0xffE6EDF3 };
    inline const juce::Colour textSecondary{ 0xff8A93A2 };
    inline const juce::Colour accent       { 0xff00E0B8 };
    inline const juce::Colour accentDim    { 0xff00A888 };
    inline const juce::Colour danger       { 0xffFF4D4D };
    inline const juce::Colour knobFace     { 0xff1C1F28 };

    constexpr float titleSize   = 20.0f;
    constexpr float sectionSize = 13.0f;
    constexpr float labelSize   = 11.0f;
    constexpr float valueSize   = 11.0f;
    constexpr float margin      = 10.0f;
    constexpr float cornerRadius = 6.0f;
}

// ─── Look and Feel ──────────────────────────────────────────────────────────
class ScopeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ScopeLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float min, float max,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool isDown,
                       int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;
};

// ─── Editor ─────────────────────────────────────────────────────────────────
class NeonScopeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor&);
    ~NeonScopeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ── Timer / state ───────────────────────────────────────────────────
    void timerCallback() override;
    void updateVisualState();

    // ── Helpers ─────────────────────────────────────────────────────────
    void configureKnob (juce::Slider&, juce::Label&, const juce::String& name);
    void refreshKnobLabels();
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title);
    void drawSpectrum (juce::Graphics&);
    void drawMeters (juce::Graphics&, juce::Rectangle<float> area);
    void drawSingleMeter (juce::Graphics&, juce::Rectangle<float> area,
                          float rmsDb, float peakDb, float holdNorm,
                          const juce::String& label);
    void drawCorrelation (juce::Graphics&, juce::Rectangle<float> area);

    // ── Core ────────────────────────────────────────────────────────────
    NeonScopeAudioProcessor& processor;
    ScopeLookAndFeel scopeLnf;

    // ── Controls ────────────────────────────────────────────────────────
    juce::ComboBox modeBox, filterTypeBox, satModeBox, oversamplingBox, monitorModeBox;
    juce::Slider cutoffSlider, resonanceSlider, driveSlider;
    juce::Slider mixSlider, outputSlider, sensitivitySlider;
    juce::ToggleButton autoGainButton { "Auto Gain" };
    juce::ToggleButton limiterButton  { "Limiter" };
    juce::ToggleButton bandListenButton { "Band Listen" };
    juce::Label cutoffLabel, resonanceLabel, driveLabel;
    juce::Label mixLabel, outputLabel, sensitivityLabel;
    juce::Label autoGainValueLabel, monitorModeLabel;

    // ── Attachments ─────────────────────────────────────────────────────
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> monitorModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandListenAttachment;

    // ── Visual state ────────────────────────────────────────────────────
    std::array<float, NeonScopeAudioProcessor::numBands> bandCache {};
    float leftLevel = 0.0f, rightLevel = 0.0f;
    float leftPeakDb = -100.0f, rightPeakDb = -100.0f;
    float leftRmsDb = -100.0f, rightRmsDb = -100.0f;
    float correlationValue = 0.0f, widthValue = 0.0f;
    float autoGainDb = 0.0f, limiterReduction = 0.0f;
    float globalRmsPulse = 0.0f, limiterFlash = 0.0f;
    float leftPeakHold = 0.0f, rightPeakHold = 0.0f;
    int leftPeakHoldTimer = 0, rightPeakHoldTimer = 0;

    // ── Layout rects ────────────────────────────────────────────────────
    juce::Rectangle<float> titleBounds, spectrumBounds;
    juce::Rectangle<float> distortionBounds, settingsBounds, metersBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessorEditor)
};
