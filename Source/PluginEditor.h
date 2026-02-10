#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NeonDialLook : public juce::LookAndFeel_V4
{
public:
    NeonDialLook();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    const juce::Colour panelBase   { juce::Colour::fromRGB (15, 16, 23) };
    const juce::Colour dialBase    { juce::Colour::fromRGB (22, 24, 34) };
    const juce::Colour neonCyan    { juce::Colour::fromRGB (0, 255, 191) };
    const juce::Colour neonMagenta { juce::Colour::fromRGB (255, 96, 182) };
};

class NeonScopeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor&);
    ~NeonScopeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateVisualState();
    void configureNeonKnob (juce::Slider& slider, juce::Label& label, const juce::String& name);
    void refreshKnobLabels();

    NeonScopeAudioProcessor& processor;
    NeonDialLook neonDialLook;

    juce::ComboBox modeBox;
    juce::ComboBox filterTypeBox;
    juce::ComboBox satModeBox;
    juce::ComboBox oversamplingBox;
    juce::ComboBox monitorModeBox;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider driveSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;
    juce::Slider sensitivitySlider;
    juce::ToggleButton autoGainButton { "Auto Gain" };
    juce::ToggleButton limiterButton { "Limiter" };
    juce::ToggleButton bandListenButton { "Band Listen" };
    juce::Label cutoffLabel;
    juce::Label resonanceLabel;
    juce::Label driveLabel;
    juce::Label mixLabel;
    juce::Label outputLabel;
    juce::Label sensitivityLabel;
    juce::Label autoGainValueLabel;
    juce::Label monitorModeLabel;

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

    std::array<float, NeonScopeAudioProcessor::numBands> bandCache {};
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;
    float leftPeakDb = -100.0f;
    float rightPeakDb = -100.0f;
    float leftRmsDb = -100.0f;
    float rightRmsDb = -100.0f;
    float correlationValue = 0.0f;
    float widthValue = 0.0f;
    float autoGainDb = 0.0f;
    float limiterReduction = 0.0f;
    float globalRmsPulse = 0.0f;
    float limiterFlash = 0.0f;
    float leftPeakHold = 0.0f;
    float rightPeakHold = 0.0f;
    int leftPeakHoldTimer = 0;
    int rightPeakHoldTimer = 0;
    juce::Rectangle<float> titleBounds;
    juce::Rectangle<float> spectrumBounds;
    juce::Rectangle<float> distortionBounds;
    juce::Rectangle<float> settingsBounds;
    juce::Rectangle<float> metersBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessorEditor)
};
