#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace patchcraft
{
    class HarmonyEngine
    {
    public:
        struct ScaleDefinition
        {
            juce::String id;
            juce::String name;
            std::vector<int> intervals;
        };

        struct ChordDefinition
        {
            juce::String id;
            juce::String symbol;
            juce::String name;
            std::vector<int> intervals;
        };

        struct ChordMatch
        {
            int rootPitchClass = 0;
            int bassPitchClass = 0;
            int inversion = 0;
            int chordIndex = 0;
            juce::String chordId;
            juce::String displayName;
            float confidence = 0.0f;
            bool exact = false;
        };

        struct ScaleMatch
        {
            int rootPitchClass = 0;
            int scaleIndex = 0;
            juce::String scaleId;
            juce::String displayName;
            float confidence = 0.0f;
        };

        struct ChordSuggestion
        {
            int degree = 0;
            int rootPitchClass = 0;
            int chordIndex = 0;
            juce::String chordId;
            juce::String romanNumeral;
            juce::String displayName;
            juce::String reason;
            float score = 0.0f;
        };

        struct VoicingOptions
        {
            int lowNote = 36;
            int highNote = 96;
            int preferredCenter = 60;
            int voices = 4;
            int inversion = -1;
            float spread = 0.35f;
        };

        static const std::vector<ScaleDefinition>& scales();
        static const std::vector<ChordDefinition>& chords();
        static const ScaleDefinition& scaleAt (int index);
        static const ChordDefinition& chordAt (int index);
        static int scaleIndexForId (const juce::String& id);
        static int chordIndexForId (const juce::String& id);

        static juce::String pitchClassName (int pitchClass, bool preferFlats = false);
        static int quantizeToScale (int midiNote, int rootPitchClass, int scaleIndex);

        static std::vector<ChordMatch> detectChords (const std::vector<int>& midiNotes,
                                                     int maximumResults = 8);
        static std::vector<ScaleMatch> detectScales (const std::vector<int>& midiNotes,
                                                     int maximumResults = 8);

        static ChordSuggestion buildDiatonicChord (int scaleRootPitchClass,
                                                    int scaleIndex,
                                                    int degree,
                                                    int chordSize = 4);
        static std::vector<ChordSuggestion> diatonicChords (int scaleRootPitchClass,
                                                            int scaleIndex,
                                                            int chordSize = 4);
        static std::vector<ChordSuggestion> suggestNextChords (int scaleRootPitchClass,
                                                               int scaleIndex,
                                                               int currentDegree,
                                                               int maximumResults = 6);

        static std::vector<int> voiceChord (int rootPitchClass,
                                            int chordIndex,
                                            const VoicingOptions& options,
                                            const std::vector<int>& previousVoicing = {});
    };
}
