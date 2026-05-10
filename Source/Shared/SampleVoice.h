#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
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

        void start (const LoadedSamplePtr& sample, int midiNote, float velocity,
                    const juce::ADSR::Parameters& adsr,
                    bool legato,
                    float sampleStart01 = 0.0f,
                    float sampleLength01 = 1.0f,
                    int sampleSlice = 0,
                    int sampleSliceCount = 1,
                    float pitchOffset = 0.0f,
                    bool reverseOverride = false);

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
        double currentSampleRate = 44100.0;

        juce::ADSR env;
        juce::ADSR::Parameters envParams;
    };

} // namespace patchcraft
