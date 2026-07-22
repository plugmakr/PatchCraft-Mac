#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
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
            std::atomic<float> chorusEnabled       { 1.0f };
            std::atomic<float> chorusType          { 2.0f };

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

            std::atomic<float> tapeDrive           { 0.25f };
            std::atomic<float> tapeTone            { 0.55f };
            std::atomic<float> tapeFlutter         { 0.12f };
            std::atomic<float> tapeMix             { 0.0f };

            std::atomic<float> vinylAge            { 0.35f };
            std::atomic<float> vinylDust           { 0.08f };
            std::atomic<float> vinylWarp           { 0.12f };
            std::atomic<float> vinylMix            { 0.0f };

            std::atomic<float> lofiBits            { 12.0f };
            std::atomic<float> lofiRate            { 0.20f };
            std::atomic<float> lofiMix             { 0.0f };

            std::atomic<float> vocalFormant        { 0.40f };
            std::atomic<float> vocalBody           { 0.35f };
            std::atomic<float> vocalMix            { 0.0f };

            std::atomic<float> multiTapTime        { 0.375f };
            std::atomic<float> multiTapFeedback    { 0.35f };
            std::atomic<float> multiTapSpread      { 0.45f };
            std::atomic<float> multiTapMix         { 0.0f };
        } atomics;

        double sampleRate = 44100.0;
        int blockSize = 512;
        int channels = 2;
        int maxCombDelaySamples = 44100;
        int maxMultiTapDelaySamples = 88200;

        juce::dsp::Chorus<float> chorus;
        juce::dsp::Phaser<float> phaser;
        juce::dsp::StateVariableTPTFilter<float> resonator;
        juce::dsp::StateVariableTPTFilter<float> vocalFormantA;
        juce::dsp::StateVariableTPTFilter<float> vocalFormantB;

        juce::AudioBuffer<float> scratchBuffer;
        juce::AudioBuffer<float> formantScratchBuffer;
        std::vector<float> dynamicsEnvelope;
        std::vector<float> spectralLowState;
        std::vector<float> tapeToneState;
        std::vector<float> vinylToneState;
        std::vector<float> lofiHeld;
        std::vector<int> lofiCounters;
        std::vector<std::vector<float>> combLines;
        std::vector<int> combWriteIndices;
        std::vector<std::vector<float>> multiTapLines;
        std::vector<int> multiTapWriteIndices;
        std::vector<std::array<float, 8>> convolutionState;
        std::vector<int> convolutionWriteIndices;
        std::uint32_t randomState = 0x1234abcd;
        double modulationPhase = 0.0;
    };
}
