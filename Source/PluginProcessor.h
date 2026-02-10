#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
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
    float getLeftPeakDb() const noexcept { return currentLeftPeakDb.load(); }
    float getRightPeakDb() const noexcept { return currentRightPeakDb.load(); }
    float getLeftRmsDb() const noexcept { return currentLeftRmsDb.load(); }
    float getRightRmsDb() const noexcept { return currentRightRmsDb.load(); }
    float getCorrelationValue() const noexcept { return correlationValue.load(); }
    float getWidthValue() const noexcept { return widthValue.load(); }
    float getAutoGainDb() const noexcept { return autoGainDisplayDb.load(); }
    float getLimiterReductionDb() const noexcept { return limiterReductionDb.load(); }
    float getGlobalRmsLevel() const noexcept { return globalRmsLevel.load(); }
    const std::array<float, 5>& getMeterTicks() const noexcept { return meterTicksDb; }
    std::array<float, numBands> getBands() const noexcept;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

private:
    static constexpr std::array<float, 5> meterTicksDb { -60.0f, -30.0f, -12.0f, -6.0f, 0.0f };
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> currentLeftLevel { 0.0f };
    std::atomic<float> currentRightLevel { 0.0f };
    std::atomic<float> currentLeftPeakDb { -100.0f };
    std::atomic<float> currentRightPeakDb { -100.0f };
    std::atomic<float> currentLeftRmsDb { -100.0f };
    std::atomic<float> currentRightRmsDb { -100.0f };
    std::atomic<float> correlationValue { 0.0f };
    std::atomic<float> widthValue { 0.0f };
    std::atomic<float> autoGainDisplayDb { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };
    std::atomic<float> globalRmsLevel { 0.0f };
    std::array<std::atomic<float>, numBands> bandLevels {};
    juce::dsp::StateVariableTPTFilter<float> filterL;
    juce::dsp::StateVariableTPTFilter<float> filterR;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    double currentSampleRate = 44100.0;
    float driveState = 2.0f;
    float mixState = 1.0f;
    float outputTrimState = 0.0f;
    float widthState = 1.0f;
    float autoGainCompensation = 1.0f;
    float rmsLeftState = 0.0f;
    float rmsRightState = 0.0f;
    float rmsReleasePerSample = 0.0f;
    float autoGainSmoothingPerSample = 0.0f;
    float limiterReleasePerSample = 0.0f;
    float limiterGain = 1.0f;
    std::vector<juce::LagrangeInterpolator> fractionalUpsamplers;
    std::vector<juce::LagrangeInterpolator> fractionalDownsamplers;
    juce::AudioBuffer<float> oversamplingBuffer;
    juce::AudioBuffer<float> bandListenBuffer;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> fifoBuffer;
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonScopeAudioProcessor)
};
