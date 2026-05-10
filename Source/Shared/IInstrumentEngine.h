#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PatchCraftTypes.h"
#include "SampleMap.h"
#include "RenderContext.h"

namespace patchcraft
{
    /**
        Common interface for the three instrument engines:
        - "sample" : SampleSynthEngine - plays mapped WAV zones
        - "synth"  : SynthEngine        - oscillator-based polyphonic synth
        - "fx"     : EffectEngine       - audio-in / audio-out effect chain

        For Sample/Synth: process() ADDS the engine output to the buffer (host
        clears the buffer first).
        For FX:           process() READS the buffer as input audio and
                         OVERWRITES it with processed output.
    */
    class IInstrumentEngine
    {
    public:
        virtual ~IInstrumentEngine() = default;

        virtual const char* engineId() const = 0;          // "sample" / "synth" / "fx"
        virtual bool needsAudioInput() const = 0;          // true only for "fx"

        virtual void prepare (double sampleRate, int maxBlockSize, int numChannels) = 0;
        virtual void reset() = 0;
        virtual void setRenderContext (const RenderContext&) {}

        virtual void noteOn  (int midiNote, float velocity) = 0;
        virtual void noteOff (int midiNote) = 0;
        virtual void allNotesOff() = 0;

        virtual void setParameter (const juce::String& parameterId, float value) = 0;

        virtual void loadFromPack (const juce::File& packFolder,
                                   const SampleMap& map) = 0;

        virtual void process (juce::AudioBuffer<float>& buffer,
                              int startSample, int numSamples) = 0;

        virtual int getActiveVoiceCount() const noexcept = 0;
        virtual int getLoadedSampleCount() const noexcept { return 0; }
        virtual juce::String getDiagnosticStatus() const { return {}; }
    };

} // namespace patchcraft
