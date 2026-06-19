#pragma once

#include "IInstrumentEngine.h"
#include "PatchCraftTypes.h"
#include "RenderContext.h"

#include <vector>

namespace patchcraft
{
    /**
        Runtime engine for the PianoRoll element.

        A piano roll stores a list of notes on a step grid (start step, length in
        steps, pitch, velocity). At runtime this class reads that note list from a
        dedicated "pianoRoll" DSP block and, driven by the host (or internal)
        transport, triggers the instrument engine's noteOn/noteOff so the pattern
        plays back in time. It is the runtime counterpart of the MIDI Playground /
        Composer runtimes but exposes a free-form, editable MIDI clip.
    */
    class PianoRollRuntime
    {
    public:
        struct Note
        {
            int startStep = 0;
            int lengthSteps = 1;
            int pitch = 60;
            float velocity = 0.8f;
        };

        static bool isPianoRollBlock (const DspBlock&);

        // Encode / decode the compact note list stored in block.metadata["notes"].
        static juce::String encodeNotes (const std::vector<Note>&);
        static std::vector<Note> decodeNotes (const juce::String&);

        // Convert a standard MIDI file into piano-roll step notes.
        static std::vector<Note> notesFromMidiFile (const juce::MidiFile& midiFile,
                                                    int steps,
                                                    int defaultLengthSteps = 1);

        void bind (const DspGraph&);
        bool isEnabled() const noexcept { return enabled; }

        void reset();
        void allNotesOff (IInstrumentEngine&);
        void process (IInstrumentEngine&, const RenderContext&);

        int totalSteps() const noexcept { return steps; }
        int stepsPerBeat() const noexcept { return perBeat; }

        // 0..1 position of the playhead within the loop, or -1 when stopped.
        double getPlaybackPosition01() const noexcept { return playback01; }

    private:
        struct SoundingNote
        {
            int pitch = 60;
            double endAbsStep = 0.0;
        };

        static float valueFor (const DspBlock&, const juce::String& key, float fallback);

        void stopAll (IInstrumentEngine&);

        bool enabled = false;
        std::vector<Note> notes;
        int steps = 16;
        int perBeat = 4;
        float rate = 1.0f;     // grid speed multiplier
        float gate = 0.9f;     // note length scaler
        float velocityScale = 1.0f;
        bool sync = true;
        bool loop = true;

        std::vector<SoundingNote> sounding;
        double lastAbsStep = -1.0;
        double playback01 = -1.0;
        bool wasPlaying = false;
    };
}
