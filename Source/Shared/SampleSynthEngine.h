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
            std::atomic<float> cutoff    { 4200.0f };
            std::atomic<float> resonance { 0.20f };
            std::atomic<float> reverbMix { 0.30f };
            std::atomic<float> delayMix  { 0.20f };
            std::atomic<float> delayTime { 0.30f };
            std::atomic<float> delayFb   { 0.40f };
            std::atomic<float> volume    { 1.0f };
            std::atomic<float> expression { 1.0f };
            std::atomic<float> pan       { 0.0f };
            std::atomic<float> retrigger { 1.0f };
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
        int nextVoiceIndex = 0;
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

        juce::AudioBuffer<float> tempBuffer;

        SampleVoice* findFreeVoice();
        LoadedSamplePtr selectSample (int note, int velocity);
        std::array<int, 128> nextRoundRobinIndex {};

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
            float  velocity = 0.0f;
            juce::ADSR env;
        };
        std::array<SineVoice, kMaxVoices> sineVoices;
        SineVoice* findFreeSineVoice();
        bool hasUsableSamples() const noexcept
        {
            auto list = getSamples();
            return list != nullptr && ! list->empty();
        }
    };

} // namespace patchcraft
