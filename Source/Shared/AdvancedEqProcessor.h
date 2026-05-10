#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <vector>

namespace patchcraft
{
    class AdvancedEqProcessor
    {
    public:
        static constexpr int kMaxBands = 8;

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();

        bool setParameter (const juce::String& id, float value);
        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

        struct Coefficients
        {
            float b0 = 1.0f;
            float b1 = 0.0f;
            float b2 = 0.0f;
            float a1 = 0.0f;
            float a2 = 0.0f;
        };

    private:
        struct Band
        {
            std::atomic<float> enabled { 0.0f };
            std::atomic<float> type    { 0.0f };
            std::atomic<float> mode    { 0.0f };
            std::atomic<float> freq    { 1000.0f };
            std::atomic<float> gainDb  { 0.0f };
            std::atomic<float> q       { 1.0f };
            std::atomic<float> solo    { 0.0f };
            std::atomic<float> dynMode { 0.0f };
            std::atomic<float> dynThresholdDb { -24.0f };
            std::atomic<float> dynRangeDb { 0.0f };
            std::atomic<float> dynAttackMs { 10.0f };
            std::atomic<float> dynReleaseMs { 120.0f };
        };

        struct State
        {
            float z1 = 0.0f;
            float z2 = 0.0f;
        };

        std::atomic<float> enabled { 0.0f };
        std::atomic<float> mix { 1.0f };
        std::atomic<float> outputTrimDb { 0.0f };
        std::array<Band, kMaxBands> bands;
        std::array<float, kMaxBands> detectorEnvelope {};
        std::vector<State> states;

        double sampleRate = 44100.0;
        int channelCount = 2;

        static Coefficients makeCoefficients (int type, float freq, float gainDb, float q, double sampleRate);
        State& stateFor (int bandIndex, int channel);
    };
}
