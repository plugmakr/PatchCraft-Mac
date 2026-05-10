#pragma once

#include "AdvancedEqProcessor.h"
#include "AdvancedFxProcessor.h"
#include "AudioUtilityProcessor.h"
#include "IInstrumentEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace patchcraft
{
    /**
        Audio-in / audio-out effect engine. Reads input audio from the buffer,
        applies a small fx chain (drive -> filter -> delay -> reverb -> output),
        overwrites the buffer with processed output.

        Parameters:
            drive          0..1   pre-distortion gain (1 = +24dB)
            mix            0..1   wet/dry of the entire chain
            filterCutoff   20..20000 Hz  (post-drive lowpass)
            filterResonance 0..1
            delayTime      0..2 s
            delayFeedback  0..0.95
            delayMix       0..1
            reverbMix      0..1
            volume         0..1.5
            pan           -1..+1
    */
    class EffectEngine : public IInstrumentEngine
    {
    public:
        EffectEngine();

        const char* engineId() const override        { return "fx"; }
        bool needsAudioInput() const override        { return true; }

        void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
        void reset() override;
        void setRenderContext (const RenderContext& context) override;

        // Effect engines ignore MIDI.
        void noteOn  (int, float) override     {}
        void noteOff (int) override            {}
        void allNotesOff() override            {}

        void setParameter (const juce::String& id, float value) override;

        void loadFromPack (const juce::File&, const SampleMap&) override {}

        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

        int getActiveVoiceCount() const noexcept override   { return 0; }

    private:
        struct Atomics
        {
            std::atomic<float> drive          { 0.0f };
            std::atomic<float> mix            { 1.0f };
            std::atomic<float> cutoff         { 12000.0f };
            std::atomic<float> resonance      { 0.10f };
            std::atomic<float> delayTime      { 0.30f };
            std::atomic<float> delayFeedback  { 0.40f };
            std::atomic<float> delayMix       { 0.0f };
            std::atomic<float> reverbMix      { 0.0f };
            std::atomic<float> volume         { 1.0f };
            std::atomic<float> expression     { 1.0f };
            std::atomic<float> pan            { 0.0f };
        } atomics;

        double sampleRate = 44100.0;
        int    blockSize  = 512;
        int    preparedChannels = 2;
        int    preparedMaxSamples = 512;
        RenderContext renderContext;

        juce::dsp::StateVariableTPTFilter<float> filter;
        AdvancedEqProcessor eq;
        AdvancedFxProcessor advancedFx;
        AudioUtilityProcessor utility;
        juce::dsp::DelayLine<float> delay { 192000 };
        juce::dsp::Reverb           reverb;

        juce::AudioBuffer<float> dryBuffer;
        std::vector<float> delayScratch;
    };

} // namespace patchcraft
