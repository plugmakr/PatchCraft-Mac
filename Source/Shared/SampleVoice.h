#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>
#include <atomic>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Holds an immutable, pre-loaded sample buffer used by SampleVoice.
        Created on the message thread, read from on the audio thread.
        Use shared_ptr so voices can safely hold references.
    */
    struct LoadedSample
    {
        juce::String  path;
        juce::AudioBuffer<float> buffer;
        double        sourceSampleRate = 44100.0;
        SampleZoneDef zone;
    };
    using LoadedSamplePtr = std::shared_ptr<LoadedSample>;

    /**
        Lightweight polyphonic voice. Plays a LoadedSample at a pitch derived
        from rootNote / midiNote with linear interpolation. ADSR envelope is
        applied per-sample. No allocations, no file access in process().

        Thread safety: start/kill/render/release are ONLY called from the
        audio thread, so no mutex is needed. isActive() uses an atomic
        so it can be safely read from any thread.
    */
    class SampleVoice
    {
    public:
        SampleVoice() = default;

        void prepare (double sampleRate);

        bool isActive() const noexcept                  { return active.load (std::memory_order_acquire); }
        int  getNote()  const noexcept                  { return note; }
        bool isOneShot() const noexcept                 { return oneShot; }
        int  getChokeGroup() const noexcept             { return chokeGroup; }

        // Voice-stealing heuristics (audio-thread only).
        int    getPriority() const noexcept             { return priority; }
        juce::uint32 getStartOrder() const noexcept     { return startOrder; }
        bool   isReleasing() const noexcept             { return releasing; }
        float  getEnvelopeLevel() const noexcept        { return lastEnvLevel; }

        void start (const LoadedSamplePtr& sample, int midiNote, float velocity,
                    const juce::ADSR::Parameters& adsr,
                    bool legato,
                    float sampleStart01 = 0.0f,
                    float sampleLength01 = 1.0f,
                    int sampleSlice = 0,
                    int sampleSliceCount = 1,
                    float pitchOffset = 0.0f,
                    bool reverseOverride = false,
                    float tempoRatio = 1.0f,
                    float padGain = 1.0f,
                    float padPanOffset = 0.0f,
                    float extraGain = 1.0f,
                    juce::uint32 voiceStartOrder = 0);

        // Begin release stage; voice will keep mixing until envelope finishes.
        void release();

        // Hard stop (steal).
        void kill();

        // Add this voice's audio into the destination buffer.
        void render (juce::AudioBuffer<float>& dest, int startSample, int numSamples);

    private:
        LoadedSamplePtr sample;  // shared_ptr keeps sample alive while voice uses it
        std::atomic<bool> active { false };
        int    note = 60;
        double position = 0.0;
        double pitchRatio = 1.0;
        int    playStart = 0;
        int    playEnd = 0;
        bool   reversePlayback = false;
        bool   oneShot = false;
        int    chokeGroup = 0;
        float  leftGain = 1.0f, rightGain = 1.0f;
        float  velocityGain = 1.0f;
        // Per-trigger multiplier carrying velocity curve + velocity-layer
        // crossfade gain computed by the engine. 1.0 == legacy behaviour.
        float  extraGain = 1.0f;
        double currentSampleRate = 44100.0;

        // Voice-stealing metadata.
        int          priority = 0;
        juce::uint32 startOrder = 0;
        bool         releasing = false;
        float        lastEnvLevel = 0.0f;

        // Equal-power loop crossfade length (samples), derived per-trigger.
        int    loopCrossfade = 0;
        bool   loopForced = false; // playMode == Loop forces full-region looping

        juce::ADSR env;
        juce::ADSR::Parameters envParams;
    };

    class GranularVoice
    {
    public:
        struct Params
        {
            float sampleStart = 0.0f;
            float sampleLength = 1.0f;
            int sampleSlice = 0;
            int sampleSliceCount = 1;
            float pitchOffset = 0.0f;
            float tempoRatio = 1.0f;
            float density = 24.0f;
            float sizeMs = 90.0f;
            float sizeRandom = 0.25f;
            float positionSpread = 0.18f;
            float scanRate = 0.0f;
            float pitchSpread = 0.0f;
            float panSpread = 0.45f;
            float reverseProbability = 0.0f;
            float texture = 0.2f;
            int maxGrains = 16;
            int directionMode = 3;
            int windowShape = 0;
            bool freeze = false;
            float padGain = 1.0f;
            float padPanOffset = 0.0f;
        };

        GranularVoice() = default;

        void prepare (double sampleRate);

        bool isActive() const noexcept                  { return active.load (std::memory_order_acquire); }
        int  getNote()  const noexcept                  { return note; }
        bool isOneShot() const noexcept                 { return false; }
        int  getChokeGroup() const noexcept             { return chokeGroup; }

        void start (const LoadedSamplePtr& sample, int midiNote, float velocity,
                    const juce::ADSR::Parameters& adsr,
                    const Params& params,
                    juce::uint32 seed);

        void release();
        void kill();
        void render (juce::AudioBuffer<float>& dest, int startSample, int numSamples, const Params& params);

    private:
        static constexpr int kMaxGrains = 32;

        struct Grain
        {
            bool active = false;
            double position = 0.0;
            double step = 1.0;
            int age = 0;
            int length = 1;
            float leftGain = 0.707f;
            float rightGain = 0.707f;
            float gain = 1.0f;
        };

        LoadedSamplePtr sample;
        std::atomic<bool> active { false };
        int note = 60;
        int chokeGroup = 0;
        float velocityGain = 1.0f;
        double currentSampleRate = 44100.0;
        double samplesUntilNextGrain = 0.0;
        double scanPosition = 0.0;
        int alternatingDirection = 1;
        juce::uint32 rngState = 0x12345678u;
        juce::ADSR env;
        juce::ADSR::Parameters envParams;
        std::array<Grain, kMaxGrains> grains;

        juce::uint32 nextRandom() noexcept;
        float random01() noexcept;
        float randomSigned() noexcept;
        int activeGrainCount() const noexcept;
        void spawnGrain (const Params& params, float envelopeValue);
        float grainWindow (const Grain& grain, int shape) const noexcept;
    };

} // namespace patchcraft
