#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NeonScopeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NeonScopeAudioProcessorEditor (NeonScopeAudioProcessor&);
    ~NeonScopeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateVisualState();

    NeonScopeAudioProcessor& processor;

    juce::ComboBox modeBox;
    juce::Slider sensitivitySlider;
    juce::Slider smoothingSlider;
    juce::Slider cutoffSlider;
    juce::Slider driveSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;

    std::array<float, NeonScopeAudioProcessor::numBands> bandCache {};
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessorEditor)
};
