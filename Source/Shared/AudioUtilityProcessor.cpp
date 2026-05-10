#include "AudioUtilityProcessor.h"

#include <cmath>

namespace patchcraft
{
    bool AudioUtilityProcessor::setParameter (const juce::String& id, float value)
    {
        if      (id == "inputTrimDb")      { inputTrimDb = value; return true; }
        else if (id == "phaseInvert")      { phaseInvert = value; return true; }
        else if (id == "stereoWidth")      { stereoWidth = value; return true; }
        else if (id == "monoMaker")        { monoMaker = value; return true; }
        else if (id == "outputGainDb")     { outputGainDb = value; return true; }
        else if (id == "outputLimiter")    { limiterEnabled = value; return true; }
        else if (id == "outputCeilingDb")  { limiterCeilingDb = value; return true; }
        return false;
    }

    void AudioUtilityProcessor::processInput (juce::AudioBuffer<float>& buffer,
                                              int startSample,
                                              int numSamples) const
    {
        if (numSamples <= 0)
            return;

        const auto gain = juce::Decibels::decibelsToGain (juce::jlimit (-48.0f, 24.0f, inputTrimDb.load()));
        const auto polarity = phaseInvert.load() >= 0.5f ? -1.0f : 1.0f;
        const auto totalGain = gain * polarity;
        if (std::abs (totalGain - 1.0f) < 0.00001f)
            return;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.applyGain (channel, startSample, numSamples, totalGain);
    }

    void AudioUtilityProcessor::processOutput (juce::AudioBuffer<float>& buffer,
                                               int startSample,
                                               int numSamples)
    {
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        const int channels = buffer.getNumChannels();
        const float mono = juce::jlimit (0.0f, 1.0f, monoMaker.load());
        const float width = juce::jlimit (0.0f, 2.0f, stereoWidth.load());
        if (channels > 1 && (mono > 0.0001f || std::abs (width - 1.0f) > 0.0001f))
        {
            auto* left = buffer.getWritePointer (0, startSample);
            auto* right = buffer.getWritePointer (1, startSample);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float mid = (left[sample] + right[sample]) * 0.5f;
                float side = (left[sample] - right[sample]) * 0.5f;
                side *= width * (1.0f - mono);
                left[sample] = mid + side;
                right[sample] = mid - side;
            }
        }

        const float gain = juce::Decibels::decibelsToGain (juce::jlimit (-48.0f, 24.0f, outputGainDb.load()));
        const bool limiterOn = limiterEnabled.load() >= 0.5f;
        const float ceiling = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, limiterCeilingDb.load()));
        float peakL = 0.0f;
        float peakR = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel, startSample);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                auto value = samples[sample] * gain;
                if (limiterOn)
                    value = juce::jlimit (-ceiling, ceiling, value);
                samples[sample] = value;

                const auto absValue = std::abs (value);
                if (channel == 0)
                    peakL = juce::jmax (peakL, absValue);
                else if (channel == 1)
                    peakR = juce::jmax (peakR, absValue);
            }
        }

        peakLeft.store (peakL);
        peakRight.store (channels > 1 ? peakR : peakL);
    }
}
