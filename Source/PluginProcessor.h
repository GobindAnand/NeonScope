#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class NeonScopeAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 16;

    NeonScopeAudioProcessor();
    ~NeonScopeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NeonScope"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getLeftLevel() const noexcept { return currentLeftLevel.load(); }
    float getRightLevel() const noexcept { return currentRightLevel.load(); }
    std::array<float, numBands> getBands() const noexcept;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> currentLeftLevel { 0.0f };
    std::atomic<float> currentRightLevel { 0.0f };
    std::array<std::atomic<float>, numBands> bandLevels {};
    std::vector<float> filterStates;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessor)
};
