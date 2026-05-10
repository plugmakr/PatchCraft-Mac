#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace patchcraft
{
    class AudioUtilityProcessor
    {
    public:
        bool setParameter (const juce::String& id, float value);

        void processInput (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) const;
        void processOutput (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

        float getPeakLeft() const noexcept  { return peakLeft.load(); }
        float getPeakRight() const noexcept { return peakRight.load(); }

    private:
        std::atomic<float> inputTrimDb { 0.0f };
        std::atomic<float> phaseInvert { 0.0f };
        std::atomic<float> stereoWidth { 1.0f };
        std::atomic<float> monoMaker { 0.0f };
        std::atomic<float> outputGainDb { 0.0f };
        std::atomic<float> limiterEnabled { 1.0f };
        std::atomic<float> limiterCeilingDb { -0.5f };
        std::atomic<float> peakLeft { 0.0f };
        std::atomic<float> peakRight { 0.0f };
    };
}
