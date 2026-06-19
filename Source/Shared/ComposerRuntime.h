#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "HarmonyEngine.h"
#include "PatchCraftTypes.h"
#include "RenderContext.h"

#include <array>
#include <vector>

namespace patchcraft
{
    class ComposerRuntime
    {
    public:
        static constexpr int kMaxChords = 16;

        void bind (const DspGraph& graph);
        void reset();
        void setParameter (const juce::String& parameterId, float value);
        void process (const RenderContext& context, juce::MidiBuffer& output);
        void releaseAll (juce::MidiBuffer& output, int sampleOffset = 0);

        bool isEnabled() const noexcept                 { return enabled; }
        bool passesInputMidi() const noexcept           { return settings.midiThru; }
        int getCurrentChordIndex() const noexcept       { return currentChordIndex; }
        double getPlaybackPosition01() const noexcept   { return playbackPosition01; }
        const std::vector<int>& getActiveNotes() const  { return activeNotes; }

    private:
        struct Settings
        {
            int rootPitchClass = 0;
            int scaleIndex = 1;
            int chordCount = 4;
            float beatsPerChord = 1.0f;
            float gate = 0.82f;
            float velocity = 0.82f;
            int voices = 4;
            int octave = 4;
            int outputChannel = 1;
            float spread = 0.38f;
            bool midiThru = true;
            std::array<int, kMaxChords> degrees {{ 0, 5, 3, 4 }};
            std::array<int, kMaxChords> inversions {};
            std::array<bool, kMaxChords> active {};
        };

        bool enabled = false;
        Settings settings;
        std::vector<int> activeNotes;
        std::vector<int> previousVoicing;
        int currentChordIndex = -1;
        double playbackPosition01 = -1.0;
        double previousPpq = -1.0;
        bool gateOpen = false;

        static bool isComposerBlock (const DspBlock& block);
        static float valueFor (const DspBlock& block, const juce::String& key, float fallback);
        void loadBlock (const DspBlock& block);
        void startChord (int chordIndex, juce::MidiBuffer& output, int sampleOffset);
    };
}
