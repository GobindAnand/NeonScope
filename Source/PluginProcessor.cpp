#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>
#include <vector>

namespace
{
    constexpr float inverseSqrt2 = 0.70710678f;
    constexpr float meterFloorDb = -60.0f;
    constexpr float meterCeilingDb = 0.0f;
    constexpr float peakCeilingDb = 6.0f;
    constexpr float epsilon = 1.0e-6f;
    constexpr float rmsReleaseTime = 0.05f;
    constexpr float peakHoldTime = 0.3f;
    constexpr float autoGainSmoothTime = 0.08f;
    constexpr float limiterReleaseTime = 0.05f;

    inline float tanhSat (float sample, float drive)
    {
        return std::tanh (sample * drive);
    }

    inline float arctanSat (float sample, float drive)
    {
        const float normaliser = std::atan (drive);
        return normaliser > 0.0f ? std::atan (sample * drive) / normaliser : sample;
    }

    inline float hardClipSat (float sample, float drive)
    {
        return juce::jlimit (-1.0f, 1.0f, sample * drive);
    }

    inline float foldbackSat (float sample, float drive)
    {
        const float threshold = 1.0f;
        float x = sample * drive;

        if (x < -threshold || x > threshold)
        {
            x = std::fabs (std::fmod (x - threshold, threshold * 4.0f));
            x = (threshold - std::fabs (x - threshold * 2.0f)) - threshold;
        }

        return juce::jlimit (-threshold, threshold, x);
    }

    inline float softSat (float sample, float drive)
    {
        const float scaledDrive = std::pow (drive, 0.65f);
        const float x = sample * scaledDrive;
        return x / (1.0f + std::abs (x));
    }

    inline float tubeSat (float sample, float drive)
    {
        const float scaledDrive = std::pow (drive, 0.7f);
        const float x = sample * scaledDrive;

        if (x > 0.0f)
            return std::tanh (x * 0.7f);
        else
            return std::tanh (x * 1.0f) * 0.9f;
    }

    inline float computeBufferRms (const juce::AudioBuffer<float>& buffer, int channels, int numSamples)
    {
        double sum = 0.0;
        int totalSamples = 0;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float sample = data[i];
                sum += static_cast<double> (sample) * sample;
            }

            totalSamples += numSamples;
        }

        if (totalSamples == 0)
            return 0.0f;

        return static_cast<float> (std::sqrt (sum / static_cast<double> (totalSamples)));
    }

    inline float normaliseDb (float dbValue, float minDb = meterFloorDb, float maxDb = meterCeilingDb)
    {
        const float clipped = juce::jlimit (minDb, maxDb, dbValue);
        return juce::jlimit (0.0f, 1.0f, (clipped - minDb) / (maxDb - minDb));
    }

    struct BlockRamp
    {
        void initialise (float currentValue, float targetValue, int totalSamplesInBlock)
        {
            start = currentValue;
            target = targetValue;
            totalSamples = totalSamplesInBlock;

            if (totalSamples <= 1)
            {
                increment = 0.0f;
                return;
            }

            increment = (target - start) / static_cast<float> (totalSamples - 1);
        }

        float valueAt (int sampleIndex) const noexcept
        {
            if (totalSamples <= 1)
                return target;

            const int clamped = juce::jlimit (0, totalSamples - 1, sampleIndex);
            return start + increment * static_cast<float> (clamped);
        }

        float valueForOversampledIndex (int oversampledIndex, float oversamplingFactor) const noexcept
        {
            if (totalSamples <= 1)
                return target;

            if (oversamplingFactor <= 1.0f)
                return valueAt (oversampledIndex);

            const float scaledIndex = static_cast<float> (oversampledIndex) / oversamplingFactor;
            const int originalIndex = juce::jlimit (0, totalSamples - 1, static_cast<int> (std::floor (scaledIndex)));
            return valueAt (originalIndex);
        }

        float target = 0.0f;

    private:
        float start = 0.0f;
        float increment = 0.0f;
        int totalSamples = 0;
    };

    inline float applyBallistics (float current, float target, float attack, float release)
    {
        if (target >= current)
            return current + (target - current) * attack;

        return current + (target - current) * release;
    }

    inline float roundToDecimals (float value, int decimals)
    {
        const float scale = std::pow (10.0f, static_cast<float> (decimals));
        if (scale <= 0.0f)
            return value;

        return std::round (value * scale) / scale;
    }

    inline void pushSamplesIntoFifo (std::vector<float>& fifo, int& index, bool& ready, const float* samples, int numSamples, int fifoSize)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fifo[static_cast<size_t> (index)] = samples[i];
            ++index;

            if (index >= fifoSize)
            {
                index = 0;
                ready = true;
            }
        }
    }

    inline void mapFFTBinsToLogBands (const std::vector<float>& fftData, std::array<std::atomic<float>, 16>& bandLevels,
                                      int fftSize, double sampleRate, float smoothingFactor)
    {
        constexpr float minFreq = 20.0f;
        constexpr float maxFreq = 20000.0f;
        constexpr int numBands = 16;
        constexpr float spectrumFloor = -80.0f;
        constexpr float spectrumCeiling = -10.0f;

        for (int band = 0; band < numBands; ++band)
        {
            const float lowFreq = minFreq * std::pow (maxFreq / minFreq, static_cast<float> (band) / numBands);
            const float highFreq = minFreq * std::pow (maxFreq / minFreq, static_cast<float> (band + 1) / numBands);

            const int lowBin = juce::jmax (1, static_cast<int> (lowFreq * fftSize / sampleRate));
            const int highBin = juce::jmin (fftSize / 2, static_cast<int> (highFreq * fftSize / sampleRate));

            float sum = 0.0f;
            int count = 0;

            for (int bin = lowBin; bin < highBin; ++bin)
            {
                const float real = fftData[static_cast<size_t> (bin * 2)];
                const float imag = fftData[static_cast<size_t> (bin * 2 + 1)];
                const float magnitude = std::sqrt (real * real + imag * imag);
                sum += magnitude;
                ++count;
            }

            const float avgMagnitude = count > 0 ? sum / count : 0.0f;
            const float scaledMagnitude = avgMagnitude / static_cast<float> (fftSize);
            const float dbValue = juce::Decibels::gainToDecibels (scaledMagnitude + epsilon, -120.0f);
            const float normalised = juce::jlimit (0.0f, 1.0f, juce::jmap (dbValue, spectrumFloor, spectrumCeiling, 0.0f, 1.0f));
            const float currentValue = bandLevels[band].load();
            const float smoothed = currentValue * smoothingFactor + normalised * (1.0f - smoothingFactor);
            bandLevels[band].store (juce::jlimit (0.0f, 1.0f, smoothed));
        }
    }
}

NeonScopeAudioProcessor::NeonScopeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& band : bandLevels)
        band.store (0.0f);
}

void NeonScopeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    const juce::uint32 blockSize = static_cast<juce::uint32> (samplesPerBlock > 0 ? samplesPerBlock : 512);
    const int channelCount = juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { currentSampleRate, blockSize, static_cast<juce::uint32> (channelCount) };

    filterL.reset();
    filterR.reset();
    filterL.prepare (spec);
    filterR.prepare (spec);

    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>> (
        static_cast<size_t> (channelCount),
        1,
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR);

    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>> (
        static_cast<size_t> (channelCount),
        2,
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR);

    const auto initOversampler = [blockSize] (std::unique_ptr<juce::dsp::Oversampling<float>>& os)
    {
        if (os != nullptr)
        {
            os->reset();
            os->initProcessing (static_cast<size_t> (blockSize));
        }
    };

    initOversampler (oversampler2x);
    initOversampler (oversampler4x);
    fractionalUpsamplers.resize (static_cast<size_t> (channelCount));
    fractionalDownsamplers.resize (static_cast<size_t> (channelCount));

    for (auto& interp : fractionalUpsamplers)
        interp.reset();
    for (auto& interp : fractionalDownsamplers)
        interp.reset();

    const int maxOversampledSamples = static_cast<int> (blockSize * 4u);
    oversamplingBuffer.setSize (channelCount,
                                juce::jmax (1, maxOversampledSamples),
                                false,
                                false,
                                true);
    bandListenBuffer.setSize (channelCount,
                              static_cast<int> (blockSize),
                              false,
                              false,
                              true);

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    fftData.resize (fftSize * 2, 0.0f);
    fifoBuffer.resize (fftSize, 0.0f);
    fifoIndex = 0;
    nextFFTBlockReady = false;

    const auto getParamValue = [this] (const juce::String& paramID, float defaultValue)
    {
        if (const auto* parameter = parameters.getRawParameterValue (paramID))
            return parameter->load();
        return defaultValue;
    };

    driveState = getParamValue ("drive", 2.0f);
    mixState = getParamValue ("mix", 1.0f);
    outputTrimState = getParamValue ("outputTrim", 0.0f);
    widthState = getParamValue ("width", 1.0f);
    autoGainCompensation = 1.0f;
    rmsLeftState = 0.0f;
    rmsRightState = 0.0f;
    limiterGain = 1.0f;

    const double sr = juce::jmax (1.0, currentSampleRate);
    rmsReleasePerSample = std::exp (-1.0 / (juce::jmax (1.0, sr * rmsReleaseTime)));
    autoGainSmoothingPerSample = std::exp (-1.0 / (juce::jmax (1.0, sr * autoGainSmoothTime)));
    limiterReleasePerSample = std::exp (-1.0 / (juce::jmax (1.0, sr * limiterReleaseTime)));

    autoGainDisplayDb.store (0.0f);
    limiterReductionDb.store (0.0f);
    globalRmsLevel.store (0.0f);
}

void NeonScopeAudioProcessor::releaseResources()
{
    filterL.reset();
    filterR.reset();
    if (oversampler2x != nullptr)
        oversampler2x->reset();
    if (oversampler4x != nullptr)
        oversampler4x->reset();
    fractionalUpsamplers.clear();
    fractionalDownsamplers.clear();
    oversamplingBuffer.setSize (0, 0);
    bandListenBuffer.setSize (0, 0);
    autoGainCompensation = 1.0f;
    limiterGain = 1.0f;
}

bool NeonScopeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainInLayout = layouts.getChannelSet (true, 0);
    const auto mainOutLayout = layouts.getChannelSet (false, 0);

    if (mainInLayout != mainOutLayout)
        return false;

    return mainOutLayout == juce::AudioChannelSet::mono()
        || mainOutLayout == juce::AudioChannelSet::stereo();
}

void NeonScopeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int activeChannels = juce::jmax (1, juce::jmin (totalNumInputChannels, totalNumOutputChannels));

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        const float* source = buffer.getReadPointer (ch);
        float* destination = buffer.getWritePointer (ch);

        if (source != destination)
            juce::FloatVectorOperations::copy (destination, source, numSamples);
    }

    const auto getParam = [this] (const juce::String& paramID, float defaultValue) -> float
    {
        if (const auto* parameter = parameters.getRawParameterValue (paramID))
            return parameter->load();
        return defaultValue;
    };

    const int mode = juce::roundToInt (getParam ("mode", 0.0f));
    const int filterChoice = juce::roundToInt (getParam ("filterType", 0.0f));
    const float cutoff = getParam ("cutoff", 8000.0f);
    const float resonance = getParam ("resonance", 0.7f);
    const float drive = getParam ("drive", 2.0f);
    const int satChoice = juce::roundToInt (getParam ("satMode", 0.0f));
    const float width = getParam ("width", 1.0f);
    const float mix = getParam ("mix", 1.0f);
    const float outputTrim = getParam ("outputTrim", 0.0f);
    const int oversamplingChoice = juce::roundToInt (getParam ("oversampling", 0.0f));
    const float sensitivity = juce::jlimit (0.1f, 4.0f, getParam ("sensitivity", 1.0f));
    const float smoothing = juce::jlimit (0.0f, 0.95f, getParam ("smoothing", 0.7f));
    const bool autoGainEnabled = getParam ("AUTO_GAIN", 1.0f) >= 0.5f;
    const bool limiterEnabled = getParam ("SAFETY_LIMITER", 1.0f) >= 0.5f;
    const bool bandListenEnabled = getParam ("bandListen", 0.0f) >= 0.5f;
    const int monitorModeChoice = juce::jlimit (0, 5, juce::roundToInt (getParam ("monitorMode", 0.0f)));

    const bool processingActive = mode != 0;
    const bool filterActive = mode == 1 || mode == 3;
    const bool distortionActive = mode == 2 || mode == 3;
    const bool widthActive = processingActive && activeChannels == 2;

    static constexpr std::array<float, 5> oversamplingFactors { 1.0f, 1.3f, 1.7f, 2.0f, 4.0f };
    const int oversamplingIndex = juce::jlimit (0, static_cast<int> (oversamplingFactors.size() - 1), oversamplingChoice);
    const float oversamplingFactor = oversamplingFactors[oversamplingIndex];

    juce::dsp::Oversampling<float>* selectedOversampler = nullptr;
    if (oversamplingFactor >= 4.0f)
        selectedOversampler = oversampler4x.get();
    else if (oversamplingFactor >= 2.0f)
        selectedOversampler = oversampler2x.get();

    BlockRamp driveRamp;
    BlockRamp mixRamp;
    BlockRamp outputRamp;
    BlockRamp widthRamp;

    const float driveTarget = juce::jlimit (1.0f, 3.0f, drive);
    const float mixTarget = juce::jlimit (0.0f, 1.0f, mix);
    const float outputTarget = outputTrim;
    const float widthTarget = juce::jlimit (0.0f, 2.0f, width);

    driveRamp.initialise (driveState, driveTarget, numSamples);
    mixRamp.initialise (mixState, mixTarget, numSamples);
    outputRamp.initialise (outputTrimState, outputTarget, numSamples);
    widthRamp.initialise (widthState, widthTarget, numSamples);

    driveState = driveRamp.target;
    mixState = mixRamp.target;
    outputTrimState = outputRamp.target;
    widthState = widthRamp.target;

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf (buffer);

    bool capturedBandBuffer = false;

    if (processingActive)
    {
        if (filterActive)
        {
            auto type = juce::dsp::StateVariableTPTFilterType::lowpass;
            if (filterChoice == 1)
                type = juce::dsp::StateVariableTPTFilterType::highpass;
            else if (filterChoice == 2)
                type = juce::dsp::StateVariableTPTFilterType::bandpass;

            filterL.setType (type);
            filterR.setType (type);
            const float limitedCutoff = juce::jlimit (80.0f, 18000.0f, cutoff);
            filterL.setCutoffFrequency (limitedCutoff);
            filterR.setCutoffFrequency (limitedCutoff);
            const float limitedResonance = juce::jlimit (0.2f, 1.5f, resonance);
            filterL.setResonance (limitedResonance);
            filterR.setResonance (limitedResonance);

            for (int channel = 0; channel < activeChannels; ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                auto& filter = (channel == 0 ? filterL : filterR);

                for (int i = 0; i < numSamples; ++i)
                    data[i] = filter.processSample (0, data[i]);
            }

            if (bandListenEnabled)
            {
                if (bandListenBuffer.getNumChannels() != activeChannels
                    || bandListenBuffer.getNumSamples() != numSamples)
                {
                    bandListenBuffer.setSize (activeChannels,
                                              numSamples,
                                              false,
                                              false,
                                              true);
                }

                for (int channel = 0; channel < activeChannels; ++channel)
                    bandListenBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

                capturedBandBuffer = true;
            }
        }
        else
        {
            capturedBandBuffer = false;
        }

        if (distortionActive)
        {
            const auto saturate = [satChoice] (float sample, float driveValue)
            {
                switch (satChoice)
                {
                    case 1: return softSat (sample, driveValue);
                    case 2: return tubeSat (sample, driveValue);
                    case 3: return arctanSat (sample, driveValue);
                    case 4: return hardClipSat (sample, driveValue);
                    case 5: return foldbackSat (sample, driveValue);
                    default: return tanhSat (sample, driveValue);
                }
            };

            auto processNonLinear = [&driveRamp, &saturate] (juce::dsp::AudioBlock<float>& block,
                                                             float osFactor)
            {
                const int totalSamples = static_cast<int> (block.getNumSamples());
                for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
                {
                    auto* data = block.getChannelPointer (channel);
                    for (int i = 0; i < totalSamples; ++i)
                    {
                        const float driveValue = driveRamp.valueForOversampledIndex (i, osFactor);
                        data[i] = saturate (data[i], driveValue);
                    }
                }
            };

            if (oversamplingFactor > 1.0f && selectedOversampler != nullptr)
            {
                selectedOversampler->initProcessing (static_cast<size_t> (numSamples));
                juce::dsp::AudioBlock<float> block (buffer);
                auto oversampledBlock = selectedOversampler->processSamplesUp (block);
                processNonLinear (oversampledBlock, oversamplingFactor);
                selectedOversampler->processSamplesDown (block);
            }
            else if (oversamplingFactor > 1.0f)
            {
                const int oversampledSamples = juce::jmax (1, static_cast<int> (std::ceil (numSamples * oversamplingFactor)));
                if (oversamplingBuffer.getNumChannels() != activeChannels
                    || oversamplingBuffer.getNumSamples() != oversampledSamples)
                {
                    oversamplingBuffer.setSize (activeChannels,
                                                oversampledSamples,
                                                false,
                                                false,
                                                true);
                }

                if (static_cast<int> (fractionalUpsamplers.size()) < activeChannels)
                {
                    fractionalUpsamplers.resize (static_cast<size_t> (activeChannels));
                    fractionalDownsamplers.resize (static_cast<size_t> (activeChannels));
                    for (auto& interp : fractionalUpsamplers)
                        interp.reset();
                    for (auto& interp : fractionalDownsamplers)
                        interp.reset();
                }

                for (int channel = 0; channel < activeChannels; ++channel)
                {
                    const auto* src = buffer.getReadPointer (channel);
                    auto* dest = oversamplingBuffer.getWritePointer (channel);
                    fractionalUpsamplers[(size_t) channel].process (1.0 / oversamplingFactor,
                                                                    src,
                                                                    dest,
                                                                    oversampledSamples);
                }

                juce::dsp::AudioBlock<float> oversampledBlock (oversamplingBuffer);
                processNonLinear (oversampledBlock, oversamplingFactor);

                for (int channel = 0; channel < activeChannels; ++channel)
                {
                    const auto* src = oversamplingBuffer.getReadPointer (channel);
                    auto* dest = buffer.getWritePointer (channel);
                    fractionalDownsamplers[(size_t) channel].process (oversamplingFactor,
                                                                      src,
                                                                      dest,
                                                                      numSamples);
                }
            }
            else
            {
                juce::dsp::AudioBlock<float> block (buffer);
                processNonLinear (block, 1.0f);
            }
        }

        if (widthActive)
        {
            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                const float widthValue = widthRamp.valueAt (i);
                const float L = left[i];
                const float R = right[i];
                const float mid = (L + R) * inverseSqrt2;
                float side = (L - R) * inverseSqrt2;
                side *= widthValue;
                left[i] = (mid + side) * inverseSqrt2;
                right[i] = (mid - side) * inverseSqrt2;
            }
        }
    }

    const bool shouldBlendDistortion = processingActive && distortionActive;
    const float wetMixTarget = shouldBlendDistortion ? mixRamp.target : 0.0f;
    const bool wetBlendActive = shouldBlendDistortion && wetMixTarget > 0.0f;

    const float autoGainCoeffBlock = std::pow (autoGainSmoothingPerSample, numSamples);

    if (autoGainEnabled && distortionActive && wetMixTarget > 0.0f)
    {
        const float dryRms = computeBufferRms (dryBuffer, activeChannels, numSamples);
        const float wetRms = computeBufferRms (buffer, activeChannels, numSamples);
        float targetGain = 1.0f;

        if (dryRms > epsilon && wetRms > epsilon)
            targetGain = juce::jlimit (0.125f, 8.0f, dryRms / juce::jmax (wetRms, epsilon));

        autoGainCompensation = autoGainCompensation * autoGainCoeffBlock + targetGain * (1.0f - autoGainCoeffBlock);
        buffer.applyGain (autoGainCompensation);
    }
    else
    {
        autoGainCompensation = autoGainCompensation * autoGainCoeffBlock + 1.0f * (1.0f - autoGainCoeffBlock);
    }

    bool replacedWithDry = false;

    if (shouldBlendDistortion)
    {
        if (wetMixTarget <= 0.0f)
        {
            buffer.makeCopyOf (dryBuffer);
            replacedWithDry = true;
        }
        else if (wetMixTarget < 1.0f)
        {
            for (int channel = 0; channel < activeChannels; ++channel)
            {
                const auto* dryData = dryBuffer.getReadPointer (channel);
                auto* wetData = buffer.getWritePointer (channel);

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const float wetAmount = mixRamp.valueAt (sample);
                    const float dryAmount = 1.0f - wetAmount;
                    wetData[sample] = dryAmount * dryData[sample] + wetAmount * wetData[sample];
                }
            }
        }
    }

    const bool applyOutputGain = processingActive && (! shouldBlendDistortion || wetMixTarget > 0.0f);
    if (applyOutputGain)
    {
        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float gain = juce::Decibels::decibelsToGain (outputRamp.valueAt (sample));
                data[sample] *= gain;
            }
        }
    }
    else if (! replacedWithDry)
    {
        buffer.makeCopyOf (dryBuffer);
    }

    if (bandListenEnabled && capturedBandBuffer)
    {
        for (int channel = 0; channel < activeChannels; ++channel)
            buffer.copyFrom (channel, 0, bandListenBuffer, channel, 0, numSamples);
    }

    auto applyMonitorMode = [&] (int selection)
    {
        if (selection == 0 || activeChannels == 0)
            return;

        auto* left = buffer.getWritePointer (0);
        auto* right = activeChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        switch (selection)
        {
            case 1: // Mono
                for (int i = 0; i < numSamples; ++i)
                {
                    const float R = right != nullptr ? right[i] : left[i];
                    const float mono = 0.5f * (left[i] + R);
                    left[i] = mono;
                    if (right != nullptr)
                        right[i] = mono;
                }
                break;
            case 2: // Left
                if (right != nullptr)
                    juce::FloatVectorOperations::copy (right, left, numSamples);
                break;
            case 3: // Right
                if (right != nullptr)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float val = right[i];
                        left[i] = val;
                        right[i] = val;
                    }
                }
                break;
            case 4: // Mid
                if (right != nullptr)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float mid = 0.5f * (left[i] + right[i]);
                        left[i] = mid;
                        right[i] = mid;
                    }
                }
                break;
            case 5: // Side
                if (right != nullptr)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float side = 0.5f * (left[i] - right[i]);
                        left[i] = side;
                        right[i] = -side;
                    }
                }
                break;
            default:
                break;
        }
    };

    applyMonitorMode (monitorModeChoice);

    for (int channel = 0; channel < activeChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = juce::jlimit (-1.0f, 1.0f, data[sample]);
    }

    const float threshold = juce::Decibels::decibelsToGain (-0.3f);
    float minLimiterGain = 1.0f;

    if (limiterEnabled)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float framePeak = 0.0f;
            for (int channel = 0; channel < activeChannels; ++channel)
                framePeak = juce::jmax (framePeak, std::abs (buffer.getSample (channel, sample)));

            const float target = framePeak > threshold ? threshold / (framePeak + epsilon) : 1.0f;

            if (target < limiterGain)
                limiterGain = target;
            else
                limiterGain = limiterGain + (1.0f - limiterGain) * (1.0f - limiterReleasePerSample);

            for (int channel = 0; channel < activeChannels; ++channel)
            {
                float sampleValue = buffer.getSample (channel, sample) * limiterGain;
                buffer.setSample (channel, sample, sampleValue);
            }

            minLimiterGain = juce::jmin (minLimiterGain, limiterGain);
        }

        limiterReductionDb.store (juce::Decibels::gainToDecibels (minLimiterGain, -120.0f));
    }
    else
    {
        limiterGain = 1.0f;
        limiterReductionDb.store (0.0f);
    }

    autoGainDisplayDb.store (juce::Decibels::gainToDecibels (autoGainCompensation, -120.0f));

    // Metering
    const float sensitivityDbOffset = juce::Decibels::gainToDecibels (sensitivity, -120.0f);
    const float meterAttack = juce::jmap (smoothing, 0.0f, 0.95f, 0.45f, 0.2f);
    const float meterRelease = juce::jmap (smoothing, 0.0f, 0.95f, 0.08f, 0.03f);
    const float bandAttack = juce::jmap (smoothing, 0.0f, 0.95f, 0.35f, 0.15f);
    const float bandRelease = juce::jmap (smoothing, 0.0f, 0.95f, 0.12f, 0.05f);
    const float rmsReleaseBlock = std::pow (rmsReleasePerSample, numSamples);

    const float* leftData = buffer.getReadPointer (0);
    const float* rightData = activeChannels > 1 ? buffer.getReadPointer (1) : nullptr;

    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    double sumLeft = 0.0;
    double sumRight = 0.0;
    double sumLR = 0.0;
    double sumMid = 0.0;
    double sumSide = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float L = leftData != nullptr ? leftData[i] : 0.0f;
        const float R = rightData != nullptr ? rightData[i] : L;

        peakLeft = juce::jmax (peakLeft, std::abs (L));
        peakRight = juce::jmax (peakRight, std::abs (R));

        sumLeft += static_cast<double> (L) * L;
        sumRight += static_cast<double> (R) * R;
        sumLR += static_cast<double> (L) * R;

        const float mid = 0.5f * (L + R);
        const float side = 0.5f * (L - R);
        sumMid += static_cast<double> (mid) * mid;
        sumSide += static_cast<double> (side) * side;
    }

    const float leftRmsInstant = std::sqrt (sumLeft / juce::jmax (1, numSamples));
    const float rightRmsInstant = std::sqrt (sumRight / juce::jmax (1, numSamples));

    auto smoothRms = [rmsReleaseBlock] (float& state, float target)
    {
        if (target >= state)
        {
            state = target;
        }
        else
        {
            state = target + (state - target) * rmsReleaseBlock;
        }

        return state;
    };

    const float smoothedLeft = smoothRms (rmsLeftState, leftRmsInstant);
    const float smoothedRight = smoothRms (rmsRightState, rightRmsInstant);

    const float leftRmsDb = juce::jlimit (meterFloorDb,
                                          meterCeilingDb,
                                          juce::Decibels::gainToDecibels (smoothedLeft + epsilon, -120.0f) + sensitivityDbOffset);
    const float rightRmsDb = juce::jlimit (meterFloorDb,
                                           meterCeilingDb,
                                           juce::Decibels::gainToDecibels (smoothedRight + epsilon, -120.0f) + sensitivityDbOffset);
    const float leftPeakDb = juce::jlimit (meterFloorDb,
                                           peakCeilingDb,
                                           juce::Decibels::gainToDecibels (peakLeft + epsilon, -120.0f) + sensitivityDbOffset);
    const float rightPeakDb = juce::jlimit (meterFloorDb,
                                            peakCeilingDb,
                                            juce::Decibels::gainToDecibels (peakRight + epsilon, -120.0f) + sensitivityDbOffset);

    const float leftNorm = normaliseDb (leftRmsDb, meterFloorDb, meterCeilingDb);
    const float rightNorm = normaliseDb (rightRmsDb, meterFloorDb, meterCeilingDb);

    const float leftSmoothedNorm = applyBallistics (currentLeftLevel.load(), leftNorm, meterAttack, meterRelease);
    const float rightSmoothedNorm = applyBallistics (currentRightLevel.load(), rightNorm, meterAttack, meterRelease);

    currentLeftLevel.store (leftSmoothedNorm);
    currentRightLevel.store (rightSmoothedNorm);
    currentLeftPeakDb.store (roundToDecimals (leftPeakDb, 1));
    currentRightPeakDb.store (roundToDecimals (rightPeakDb, 1));
    currentLeftRmsDb.store (roundToDecimals (leftRmsDb, 1));
    currentRightRmsDb.store (roundToDecimals (rightRmsDb, 1));

    const double denom = std::sqrt (juce::jmax (static_cast<double> (epsilon), sumLeft * sumRight));
    const float correlation = denom > 0.0 ? static_cast<float> (sumLR / denom) : 0.0f;
    correlationValue.store (juce::jlimit (-1.0f, 1.0f, correlation));

    const float widthMetric = sumMid > 0.0 ? juce::jlimit (0.0f, 1.0f, static_cast<float> (sumSide / sumMid)) : 0.0f;
    widthValue.store (widthMetric);
    globalRmsLevel.store (juce::jlimit (0.0f, 1.0f, 0.5f * (leftSmoothedNorm + rightSmoothedNorm)));

    if (numSamples > 0 && activeChannels > 0)
    {
        const float* monoData = buffer.getReadPointer (0);
        std::vector<float> monoMix (numSamples);

        if (activeChannels == 1)
        {
            std::copy (monoData, monoData + numSamples, monoMix.begin());
        }
        else
        {
            const float* rightData = buffer.getReadPointer (1);
            for (int i = 0; i < numSamples; ++i)
                monoMix[i] = (monoData[i] + rightData[i]) * 0.5f;
        }

        pushSamplesIntoFifo (fifoBuffer, fifoIndex, nextFFTBlockReady, monoMix.data(), numSamples, fftSize);

        if (nextFFTBlockReady)
        {
            nextFFTBlockReady = false;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());

            juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);
            window.multiplyWithWindowingTable (fftData.data(), fftSize);

            fft->performFrequencyOnlyForwardTransform (fftData.data());

            const float fftSmoothing = juce::jmap (smoothing, 0.0f, 0.95f, 0.75f, 0.92f);
            mapFFTBinsToLogBands (fftData, bandLevels, fftSize, currentSampleRate, fftSmoothing);
        }
    }
}

void NeonScopeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NeonScopeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

std::array<float, NeonScopeAudioProcessor::numBands> NeonScopeAudioProcessor::getBands() const noexcept
{
    std::array<float, numBands> values {};
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = bandLevels[i].load();

    return values;
}

juce::AudioProcessorEditor* NeonScopeAudioProcessor::createEditor()
{
    return new NeonScopeAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout NeonScopeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "mode",
        "Mode",
        juce::StringArray { "Visualize Only", "Tone Filter", "Soft Distortion", "Hybrid" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "filterType",
        "Filter Type",
        juce::StringArray { "Low-pass", "High-pass", "Band-pass" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "cutoff",
        "Cutoff",
        juce::NormalisableRange<float> { 80.0f, 18000.0f, 0.0f, 0.4f },
        8000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "resonance",
        "Resonance",
        juce::NormalisableRange<float> { 0.2f, 1.5f, 0.0f, 0.7f },
        0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "drive",
        "Drive",
        juce::NormalisableRange<float> { 1.0f, 3.0f, 0.0f, 0.6f },
        1.5f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "satMode",
        "Saturation Mode",
        juce::StringArray { "Tanh", "Soft", "Tube", "Arctan", "Hard Clip", "Foldback" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "width",
        "Stereo Width",
        juce::NormalisableRange<float> { 0.0f, 2.0f, 0.0f, 1.0f },
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix",
        "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f, 1.0f },
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "outputTrim",
        "Output Trim (dB)",
        juce::NormalisableRange<float> { -12.0f, 6.0f, 0.1f, 1.0f },
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "AUTO_GAIN",
        "Auto Gain",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SAFETY_LIMITER",
        "Limiter",
        true));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "oversampling",
        "Oversampling",
        juce::StringArray { "1x", "1.3x", "1.7x", "2x", "4x" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "bandListen",
        "Band Listen",
        false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "monitorMode",
        "Monitor Mode",
        juce::StringArray { "Stereo", "Mono", "Left", "Right", "Mid", "Side" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "sensitivity",
        "Sensitivity",
        juce::NormalisableRange<float> { 0.1f, 4.0f, 0.0f, 0.35f },
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "smoothing",
        "Smoothing",
        juce::NormalisableRange<float> { 0.0f, 0.95f, 0.0f, 0.5f },
        0.7f));

    return { params.begin(), params.end() };
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeonScopeAudioProcessor();
}
