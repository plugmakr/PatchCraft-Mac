#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "AdvancedEqProcessor.h"
#include "AdvancedFxProcessor.h"
#include "AudioUtilityProcessor.h"
#include "PatchCraftPackFormat.h"
#include "IInstrumentEngine.h"
#include "SampleVoice.h"
#include "SampleSliceUtils.h"

#include <array>
#include <memory>

namespace patchcraft
{
    /**
        Polyphonic sample-playback engine driven by a SampleMap.
        - Loads WAVs on the message thread.
        - process() is RT-safe: no allocations, no file I/O.
    */
    class SampleSynthEngine : public IInstrumentEngine
    {
    public:
        static constexpr int kMaxVoices = 32;

        SampleSynthEngine();

        const char* engineId() const override        { return "sample"; }
        bool needsAudioInput() const override        { return false; }

        void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
        void reset() override;
        void setRenderContext (const RenderContext& context) override;

        void loadFromPack (const juce::File& packFolder, const SampleMap& map) override;
        // Studio convenience: same as loadFromPack but resolves relative paths
        // against the project folder.
        void loadFromMap  (const juce::File& projectFolder, const SampleMap& map);

        /** Chop-lab audition: load one zone from an in-memory buffer (no disk I/O). */
        void loadSingleZoneFromBuffer (const SampleZoneDef& zone,
                                       const juce::AudioBuffer<float>& buffer,
                                       double sourceSampleRate);

        /** Sample position of the loudest active voice for a MIDI note, or -1. */
        double getActivePlayheadSample (int midiNote) const noexcept;

        void noteOn  (int midiNote, float velocity) override;
        void noteOff (int midiNote) override;
        void allNotesOff() override;

        void setParameter (const juce::String& id, float value) override;

        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

        int getActiveVoiceCount() const noexcept override;
        int getLoadedSampleCount() const noexcept override;
        juce::String getDiagnosticStatus() const override;

    private:
        struct AtomicParams
        {
            std::atomic<float> attack    { 0.01f };
            std::atomic<float> decay     { 0.20f };
            std::atomic<float> sustain   { 0.80f };
            std::atomic<float> release   { 0.50f };
            std::atomic<float> sampleStart { 0.0f };
            std::atomic<float> sampleLength { 1.0f };
            std::atomic<float> sampleSlice { 0.0f };
            std::atomic<float> sampleSliceCount { 1.0f };
            std::atomic<float> samplePitch { 0.0f };
            std::atomic<float> sampleReverse { 0.0f };
            std::atomic<float> sampleGlitch { 0.0f };
            std::atomic<float> sampleGlitchGrid { 16.0f };
            std::atomic<float> granularOn { 0.0f };
            std::atomic<float> granularDensity { 24.0f };
            std::atomic<float> granularSizeMs { 90.0f };
            std::atomic<float> granularSizeRandom { 0.25f };
            std::atomic<float> granularSpread { 0.18f };
            std::atomic<float> granularScan { 0.0f };
            std::atomic<float> granularPitchSpread { 0.0f };
            std::atomic<float> granularPanSpread { 0.45f };
            std::atomic<float> granularReverse { 0.0f };
            std::atomic<float> granularTexture { 0.20f };
            std::atomic<float> granularMaxGrains { 16.0f };
            std::atomic<float> granularDirection { 3.0f };
            std::atomic<float> granularWindow { 0.0f };
            std::atomic<float> granularFreeze { 0.0f };
            std::atomic<float> cutoff    { 4200.0f };
            std::atomic<float> resonance { 0.20f };
            std::atomic<float> reverbMix { 0.30f };
            std::atomic<float> reverbEnabled { 1.0f };
            std::atomic<float> reverbType { 2.0f };
            std::atomic<float> delayMix  { 0.20f };
            std::atomic<float> delayTime { 0.30f };
            std::atomic<float> delayFb   { 0.40f };
            std::atomic<float> delayEnabled { 1.0f };
            std::atomic<float> delayType { 3.0f };
            std::atomic<float> volume    { 1.0f };
            std::atomic<float> expression { 1.0f };
            std::atomic<float> pan       { 0.0f };
            std::atomic<float> retrigger { 1.0f };
            std::atomic<float> bpmSync   { 1.0f };
            // 0.5 == linear (legacy). <0.5 compresses dynamics (louder soft
            // notes), >0.5 expands. Shapes velocity -> gain via a power curve.
            std::atomic<float> velocitySensitivity { 0.5f };
            std::array<std::atomic<float>, kMaxChopPads> padVolume {};
            std::array<std::atomic<float>, kMaxChopPads> padPitch {};
            std::array<std::atomic<float>, kMaxChopPads> padPan {};

            AtomicParams()
            {
                for (auto& value : padVolume)
                    value.store (1.0f, std::memory_order_relaxed);
                for (auto& value : padPitch)
                    value.store (0.0f, std::memory_order_relaxed);
                for (auto& value : padPan)
                    value.store (0.0f, std::memory_order_relaxed);
            }
        } atomics;

        juce::ADSR::Parameters currentAdsr() const;

        // Sample storage. Message thread builds a new immutable list and
        // atomically swaps the shared_ptr. Audio thread takes its own shared
        // copy before reading, so old lists stay alive until readers finish.
        using SampleList = std::vector<LoadedSamplePtr>;
        std::shared_ptr<const SampleList> currentSamples;

        std::shared_ptr<const SampleList> getSamples() const noexcept
        {
            return std::atomic_load_explicit (&currentSamples, std::memory_order_acquire);
        }

        mutable juce::CriticalSection diagnosticsLock;
        juce::String firstMissingSample;
        juce::String firstFailedSample;
        std::atomic<int> requestedZoneCount { 0 };
        std::atomic<int> loadedSampleCount { 0 };
        std::atomic<int> missingSampleCount { 0 };
        std::atomic<int> failedSampleCount { 0 };
        std::atomic<int> lastMissedNote { -1 };
        std::atomic<int> lastMissedVelocity { 0 };
        std::atomic<int> missedNoteCount { 0 };

        std::array<SampleVoice, kMaxVoices> voices;
        std::array<GranularVoice, kMaxVoices> granularVoices;
        int nextVoiceIndex = 0;
        int nextGranularVoiceIndex = 0;
        std::atomic<juce::uint32> triggerCounter { 0 };

        double sampleRate = 44100.0;
        int    blockSize  = 512;
        int    preparedChannels = 2;
        int    preparedMaxSamples = 512;
        RenderContext renderContext;

        // DSP
        juce::dsp::StateVariableTPTFilter<float> filter;
        AdvancedEqProcessor eq;
        AdvancedFxProcessor advancedFx;
        AudioUtilityProcessor utility;
        juce::dsp::DelayLine<float> delayL { 96000 };
        juce::dsp::DelayLine<float> delayR { 96000 };
        juce::dsp::Reverb           reverb;

        double tapeLfoPhase = 0.0;
        float lastFbL = 0.0f;
        float lastFbR = 0.0f;

        juce::AudioBuffer<float> tempBuffer;

        SampleVoice* findFreeVoice();
        GranularVoice* findFreeGranularVoice();
        GranularVoice::Params currentGranularParams (float tempoRatio = 1.0f) const;
        LoadedSamplePtr selectSample (int note, int velocity);

        // A zone selected for a note, paired with its velocity-crossfade gain.
        struct LayeredSample
        {
            LoadedSamplePtr sample;
            float velocityGain = 1.0f;
        };
        // Gather every zone that should sound for note+velocity (respecting
        // round-robin within a group), each with an equal-power velocity
        // crossfade gain. Single-match packs behave exactly as before.
        int selectSamplesLayered (int note, int velocity, LayeredSample* out, int maxLayers);
        float velocityCurveGain (float velocity01) const noexcept;

        static constexpr int kMaxLayersPerNote = 8;
        std::array<int, 128> nextRoundRobinIndex {};
        std::atomic<juce::uint32> voiceStartCounter { 0 };

        juce::AudioFormatManager formatManager;

        // ---- Sine fallback ---------------------------------------------------
        // When the loaded pack has no playable samples we synthesize a simple
        // sine voice per held note so the user still hears audio while
        // designing the instrument. Phase B's synth engine replaces this.
        struct SineVoice
        {
            bool   active = false;
            int    note = 60;
            double phase = 0.0;
            double phase2 = 0.0;
            float  velocity = 0.0f;
            juce::ADSR env;
            // When a note lands in the GM percussion range and no samples are
            // loaded, we synthesize a drum hit (kick/snare/hat/...) instead of a
            // sine so drum grids and pads actually sound like drums. -1 = melodic.
            int    drumType = -1;
            double drumAgeSamples = 0.0;
            float  prevNoise = 0.0f;
            juce::uint32 noiseState = 0x9e3779b9u;
        };
        std::array<SineVoice, kMaxVoices> sineVoices;
        SineVoice* findFreeSineVoice();
        static int drumTypeForNote (int note) noexcept;
        bool hasUsableSamples() const noexcept
        {
            auto list = getSamples();
            return list != nullptr && ! list->empty();
        }
    };

} // namespace patchcraft
