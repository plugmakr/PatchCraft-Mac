#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace patchcraft
{
    class AdvancedFxProcessor
    {
    public:
        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();

        bool setParameter (const juce::String& id, float value);
        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    private:
        struct Atomics
        {
            std::atomic<float> dynamicsThresholdDb { -18.0f };
            std::atomic<float> dynamicsRatio       { 2.0f };
            std::atomic<float> dynamicsAttackMs    { 10.0f };
            std::atomic<float> dynamicsReleaseMs   { 120.0f };
            std::atomic<float> dynamicsMakeupDb    { 0.0f };
            std::atomic<float> dynamicsMix         { 0.0f };

            std::atomic<float> chorusRate          { 0.35f };
            std::atomic<float> chorusDepth         { 0.35f };
            std::atomic<float> chorusFeedback      { 0.0f };
            std::atomic<float> chorusMix           { 0.0f };

            std::atomic<float> phaserRate          { 0.25f };
            std::atomic<float> phaserDepth         { 0.45f };
            std::atomic<float> phaserFeedback      { 0.0f };
            std::atomic<float> phaserMix           { 0.0f };

            std::atomic<float> combFreq            { 220.0f };
            std::atomic<float> combFeedback        { 0.35f };
            std::atomic<float> combMix             { 0.0f };

            std::atomic<float> resonatorFreq       { 440.0f };
            std::atomic<float> resonatorQ          { 4.0f };
            std::atomic<float> resonatorMix        { 0.0f };

            std::atomic<float> convolutionSize     { 3.0f };
            std::atomic<float> convolutionMix      { 0.0f };

            std::atomic<float> spectralTilt        { 0.0f };
            std::atomic<float> spectralMix         { 0.0f };
        } atomics;

        double sampleRate = 44100.0;
        int blockSize = 512;
        int channels = 2;
        int maxCombDelaySamples = 44100;

        juce::dsp::Chorus<float> chorus;
        juce::dsp::Phaser<float> phaser;
        juce::dsp::StateVariableTPTFilter<float> resonator;

        juce::AudioBuffer<float> scratchBuffer;
        std::vector<float> dynamicsEnvelope;
        std::vector<float> spectralLowState;
        std::vector<std::vector<float>> combLines;
        std::vector<int> combWriteIndices;
        std::vector<std::array<float, 8>> convolutionState;
        std::vector<int> convolutionWriteIndices;
    };
}
