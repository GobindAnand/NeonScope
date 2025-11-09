#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

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

private:
    const juce::Colour backgroundColour { juce::Colour::fromRGB (9, 9, 14) };
    const juce::Colour controlSurface   { juce::Colour::fromRGB (18, 18, 24) };
    const juce::Colour neonCyan         { juce::Colour::fromRGB (0, 255, 191) };
    const juce::Colour neonMagenta      { juce::Colour::fromRGB (255, 0, 128) };
    const juce::Colour outlineColour    { juce::Colours::white.withAlpha (0.12f) };
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

    NeonScopeAudioProcessor& processor;
    NeonLookAndFeel neonLookAndFeel;

    juce::ComboBox modeBox;
    juce::ComboBox filterTypeBox;
    juce::ComboBox satModeBox;
    juce::ComboBox oversamplingBox;
    juce::Slider sensitivitySlider;
    juce::Slider smoothingSlider;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider driveSlider;
    juce::Slider widthSlider;
    juce::Slider mixSlider;
    juce::Slider outputTrimSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;

    std::array<float, NeonScopeAudioProcessor::numBands> bandCache {};
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessorEditor)
};
