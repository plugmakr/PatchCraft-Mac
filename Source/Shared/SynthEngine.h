#pragma once

#include "AdvancedEqProcessor.h"
#include "AdvancedFxProcessor.h"
#include "AudioUtilityProcessor.h"
#include "IInstrumentEngine.h"
#include "DspRack.h"
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstdint>

namespace patchcraft
{
    /**
        Polyphonic oscillator-based synth.

        Per-voice: oscillator (sine/saw/square/triangle) + optional noise blend + ADSR.
        Global: low-pass filter (cutoff/resonance) + LFO (mod cutoff or pitch)
        + delay + reverb + master volume/pan.

        Parameters (id -> behaviour):
            oscType       0..3   primary sine / saw / square / triangle
            osc2Type      0..3   secondary sine / saw / square / triangle
            oscBlend      0..1   crossfade from primary to secondary oscillator
            octave       -2..+2  pitch shift in octaves
            detune     -100..+100 cents
            osc2Detune  -100..+100 cents applied to secondary oscillator
            subBlend      0..1   adds a lower-octave square sub layer
            noiseBlend    0..1   adds broadband noise texture
            attack/decay/sustain/release  ADSR (sec / 0..1)
            filterCutoff  20..20000 Hz
            filterResonance 0..1
            lfoRate       0.1..20  Hz
            lfoAmount     0..1     (modulates cutoff)
            delayTime     0..2 s
            delayFeedback 0..0.95
            delayMix      0..1
            reverbMix     0..1
            volume        0..1.5
            pan          -1..+1
    */
    class SynthEngine : public IInstrumentEngine
    {
    public:
        static constexpr int kMaxVoices = 16;

        SynthEngine();

        const char* engineId() const override         { return "synth"; }
        bool needsAudioInput() const override         { return false; }

        void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
        void reset() override;
        void setRenderContext (const RenderContext& context) override;

        void noteOn  (int midiNote, float velocity) override;
        void noteOff (int midiNote) override;
        void allNotesOff() override;

        void setParameter (const juce::String& id, float value) override;

        void loadFromPack (const juce::File&, const SampleMap&) override {}

        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

        int getActiveVoiceCount() const noexcept override;

    private:
        static constexpr int kCustomWavetablePoints = 32;
        static constexpr int kCustomWavetableFrames = 4;

        struct Voice
        {
            bool     active = false;
            int      note = 60;
            float    velocity = 1.0f;
            uint32_t age = 0;
            double   phase = 0.0;
            double   phase2 = 0.0;
            double   subPhase = 0.0;
            std::array<double, 8> wavetablePhase {};
            std::array<double, 8> wavetablePhaseInc {};
            double   phaseInc = 0.0;
            double   phaseInc2 = 0.0;
            double   subPhaseInc = 0.0;
            juce::ADSR env;
        };

        struct Atomics
        {
            std::atomic<float> oscType        { 1.0f };   // saw
            std::atomic<float> osc2Type       { 3.0f };   // triangle
            std::atomic<float> oscBlend       { 0.0f };
            std::atomic<float> octave         { 0.0f };
            std::atomic<float> detuneCents    { 0.0f };
            std::atomic<float> osc2DetuneCents { 7.0f };
            std::atomic<float> subBlend       { 0.0f };
            std::atomic<float> noiseBlend     { 0.0f };
            std::atomic<float> wtEnabled      { 0.0f };
            std::atomic<float> wtTable        { 0.0f };
            std::atomic<float> wtPosition     { 0.0f };
            std::atomic<float> wtMorph        { 0.0f };
            std::atomic<float> wtWarp         { 0.0f };
            std::atomic<float> wtFold         { 0.0f };
            std::atomic<float> wtUnison       { 1.0f };
            std::atomic<float> wtDetune       { 12.0f };
            std::atomic<float> wtSpread       { 0.0f };
            std::atomic<float> wtLevel        { 0.0f };
            std::atomic<float> wtBend         { 0.0f };
            std::atomic<float> wtSyncRatio    { 1.0f };
            std::atomic<float> wtSpectralTilt { 0.0f };
            std::atomic<float> wtPhaseMode    { 0.0f };
            std::atomic<float> wtFramePosition { 0.0f };
            std::atomic<float> wtFrameCount   { 1.0f };
            std::array<std::atomic<float>, kCustomWavetablePoints> wtShape {};
            std::array<std::array<std::atomic<float>, kCustomWavetablePoints>, kCustomWavetableFrames> wtFrameShape {};
            std::atomic<float> attack         { 0.01f };
            std::atomic<float> decay          { 0.20f };
            std::atomic<float> sustain        { 0.80f };
            std::atomic<float> release        { 0.40f };
            std::atomic<float> cutoff         { 4200.0f };
            std::atomic<float> resonance      { 0.20f };
            std::atomic<float> lfoRate        { 4.0f };
            std::atomic<float> lfoAmount      { 0.0f };
            std::atomic<float> delayTime      { 0.30f };
            std::atomic<float> delayFeedback  { 0.40f };
            std::atomic<float> delayMix       { 0.0f };
            std::atomic<float> reverbMix      { 0.0f };
            std::atomic<float> volume         { 0.8f };
            std::atomic<float> expression     { 1.0f };
            std::atomic<float> pan            { 0.0f };
            std::atomic<float> retrigger      { 1.0f };
        } atomics;

        double sampleRate = 44100.0;
        int    blockSize  = 512;
        int    preparedChannels = 2;
        int    preparedMaxSamples = 512;
        uint32_t voiceAgeCounter = 0;
        RenderContext renderContext;

        std::array<Voice, kMaxVoices> voices;
        Voice* findFreeVoice();

        double lfoPhase = 0.0;
        juce::Random rng { 0x1234abcd };

        juce::dsp::StateVariableTPTFilter<float> filter;
        AdvancedEqProcessor eq;
        AdvancedFxProcessor advancedFx;
        AudioUtilityProcessor utility;
        juce::dsp::DelayLine<float> delayL { 96000 };
        juce::dsp::DelayLine<float> delayR { 96000 };
        juce::dsp::Reverb           reverb;
        DspRack                     dspRack;

        juce::AudioBuffer<float> tempBuffer;

        juce::ADSR::Parameters currentAdsr() const;
        double frequencyForVoice (int note, float extraCents = 0.0f, float extraOctave = 0.0f) const;
        float  oscSample (int oscType, double phase, juce::Random& rng) const;
        float  wavetableSample (int table, double phase, float position, float morph,
                                float warp, float fold, float bend, int syncRatio,
                                float spectralTilt, double frequency) const;
    };

} // namespace patchcraft
