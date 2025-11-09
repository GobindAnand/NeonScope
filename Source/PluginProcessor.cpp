#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

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
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    filterStates.assign (static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels())), 0.0f);
}

void NeonScopeAudioProcessor::releaseResources()
{
    filterStates.clear();
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

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int activeChannels = juce::jmax (1, juce::jmin (totalNumInputChannels, totalNumOutputChannels));

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        const auto* source = buffer.getReadPointer (ch);
        auto* destination = buffer.getWritePointer (ch);

        if (source != destination)
            juce::FloatVectorOperations::copy (destination, source, numSamples);
    }

    const auto* sensitivityParam = parameters.getRawParameterValue ("sensitivity");
    const auto* smoothingParam = parameters.getRawParameterValue ("smoothing");
    const auto* modeParam = parameters.getRawParameterValue ("mode");
    const auto* cutoffParam = parameters.getRawParameterValue ("cutoff");
    const auto* driveParam = parameters.getRawParameterValue ("drive");

    const float sensitivity = sensitivityParam != nullptr ? sensitivityParam->load() : 1.0f;
    const float smoothing = smoothingParam != nullptr ? smoothingParam->load() : 0.7f;
    const int mode = modeParam != nullptr ? juce::roundToInt (modeParam->load()) : 0;
    const float cutoff = cutoffParam != nullptr ? cutoffParam->load() : 8000.0f;
    const float drive = driveParam != nullptr ? driveParam->load() : 1.5f;

    if (static_cast<int> (filterStates.size()) < activeChannels)
        filterStates.resize (static_cast<size_t> (activeChannels), 0.0f);

    auto syncFilterStateToSignal = [&]()
    {
        if (numSamples <= 0)
            return;

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);
            if (data != nullptr)
                filterStates[static_cast<size_t> (channel)] = data[numSamples - 1];
        }
    };

    if (mode == 1)
    {
        const float sr = static_cast<float> (currentSampleRate > 0.0 ? currentSampleRate : 44100.0);
        const float cutoffHz = juce::jlimit (200.0f, 16000.0f, cutoff);
        const float alpha = 1.0f - static_cast<float> (std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / sr));

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            float state = filterStates[static_cast<size_t> (channel)];

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                state += alpha * (x - state);
                data[i] = state;
            }

            filterStates[static_cast<size_t> (channel)] = state;
        }
    }
    else if (mode == 2)
    {
        const float driveAmount = juce::jlimit (0.5f, 5.0f, drive);

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
                data[i] = static_cast<float> (std::tanh (data[i] * driveAmount));
        }

        syncFilterStateToSignal();
    }
    else
    {
        syncFilterStateToSignal();
    }

    auto smoothValue = [smoothing] (float incoming, float current)
    {
        if (incoming >= current)
            return incoming;

        return current * smoothing + incoming * (1.0f - smoothing);
    };

    auto computeRMS = [numSamples] (const float* channelData)
    {
        if (channelData == nullptr || numSamples == 0)
            return 0.0f;

        double sumSquares = 0.0;
        for (int i = 0; i < numSamples; ++i)
            sumSquares += static_cast<double> (channelData[i]) * channelData[i];

        return static_cast<float> (std::sqrt (sumSquares / juce::jmax (1, numSamples)));
    };

    const float leftRms = activeChannels > 0 ? computeRMS (buffer.getReadPointer (0)) : 0.0f;
    const float rightRms = activeChannels > 1 ? computeRMS (buffer.getReadPointer (1)) : leftRms;

    const float leftTarget = juce::jlimit (0.0f, 1.0f, leftRms * sensitivity);
    const float rightTarget = juce::jlimit (0.0f, 1.0f, rightRms * sensitivity);

    const float leftStored = currentLeftLevel.load();
    const float rightStored = currentRightLevel.load();

    currentLeftLevel.store (smoothValue (leftTarget, leftStored));
    currentRightLevel.store (smoothValue (rightTarget, rightStored));

    const int samplesPerBand = juce::jmax (1, numSamples / numBands);

    for (int band = 0; band < numBands; ++band)
    {
        const int startSample = band * samplesPerBand;
        int endSample = (band == numBands - 1) ? numSamples : startSample + samplesPerBand;
        endSample = juce::jlimit (startSample, numSamples, endSample);

        if (startSample >= endSample)
            continue;

        double accumulator = 0.0;
        int countedSamples = 0;

        for (int channel = 0; channel < activeChannels; ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);
            if (data == nullptr)
                continue;

            for (int i = startSample; i < endSample; ++i)
            {
                accumulator += std::abs (static_cast<double> (data[i])) * sensitivity;
                ++countedSamples;
            }
        }

        const float bandTarget = countedSamples > 0
                                   ? juce::jlimit (0.0f, 1.0f, static_cast<float> (accumulator / countedSamples))
                                   : 0.0f;

        const float stored = bandLevels[band].load();
        bandLevels[band].store (smoothValue (bandTarget, stored));
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
        juce::StringArray { "Visualize Only", "Low-Pass Filter", "Soft Distortion" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "cutoff",
        "Cutoff",
        juce::NormalisableRange<float> { 200.0f, 16000.0f, 1.0f, 0.4f },
        8000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "drive",
        "Drive",
        juce::NormalisableRange<float> { 0.5f, 5.0f, 0.0f, 0.4f },
        1.5f));

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
