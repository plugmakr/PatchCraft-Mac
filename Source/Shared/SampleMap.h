#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    struct SampleMapHealthStatus
    {
        int totalZones = 0;
        int playableZones = 0;
        int missingFiles = 0;
        int missingMidiFiles = 0;
        int invalidRanges = 0;
        int rootOutsideRange = 0;
        int coveredNotes = 0;
        int firstCoveredNote = -1;
        int lastCoveredNote = -1;
        bool engineIsSample = false;
        bool exportReady = false;
        juce::String primaryIssue;
        juce::StringArray issues;

        bool blocksExport() const;
        juce::String exportMessage() const;
    };

    /**
        Authoring-side editable list of sample zones.
    */
    class SampleMap
    {
    public:
        SampleMap() = default;

        std::vector<SampleZoneDef>&       getZones()       { return zones; }
        const std::vector<SampleZoneDef>& getZones() const { return zones; }

        void add (const SampleZoneDef& z)         { zones.push_back (z); }
        void removeAt (int index);
        void clear()                              { zones.clear(); }

        // Choose a zone for a given note/velocity. Returns nullptr if none match.
        const SampleZoneDef* findZoneFor (int note, int velocity) const;

        // Auto-distribute zones across the keyboard given existing list of samples.
        void autoMapAcrossKeyboard();
        void autoMapByRootNotes();
        void autoMapDrumPads (int startNote = 36, int padCount = 16, bool stackSamples = false);

        static int noteNumberFromName (const juce::String& noteName);
        static SampleZoneDef inferZoneFromFile (const juce::File& file,
                                                int fallbackRootNote,
                                                int fallbackLowNote,
                                                int fallbackHighNote,
                                                bool* usedVelocityRange = nullptr);
        static SampleZoneDef inferZoneFromFileWithAudio (const juce::File& file,
                                                         int fallbackRootNote,
                                                         int fallbackLowNote,
                                                         int fallbackHighNote,
                                                         bool* usedNamePitch = nullptr,
                                                         bool* usedAudioPitch = nullptr,
                                                         bool* usedVelocityRange = nullptr);
        static int detectRootNoteFromAudioFile (const juce::File& file);

        // --- Beatmaker analysis (Serato-style) --------------------------------
        // Result of analysing an audio clip for tempo + musical key.
        struct ClipAnalysis
        {
            double bpm = 0.0;          // 0 == not detected
            int    keyPitchClass = -1; // 0..11 (C..B), -1 == not detected
            bool   keyIsMinor = false;
            float  confidence = 0.0f;  // 0..1 rough confidence in the key result
            juce::String keyName() const; // e.g. "F# min", or "" if undetected
        };

        // Estimate tempo (BPM) from an onset-strength envelope via autocorrelation.
        // Returns 0.0 when no stable tempo is found. searchMin/Max bound the result.
        static double detectTempoBpm (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                      double searchMinBpm = 70.0, double searchMaxBpm = 180.0);

        // Estimate musical key using a chroma profile + Krumhansl-Schmuckler
        // key-finding. keyIsMinor is set true for minor keys.
        static int detectMusicalKey (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                     bool& keyIsMinor, float* confidenceOut = nullptr);

        // Convenience: load a file and run both tempo + key detection.
        static ClipAnalysis analyseClipFile (const juce::File& file);

        // Analyse an in-memory region (Chop Lab / offline preview).
        static ClipAnalysis analyseClipBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                               int startSample = 0, int endSample = -1);

        // Onset detection: returns sample positions of detected attacks within
        // [startSample, endSample), capped at maxOnsets. Used for transient chop.
        static std::vector<int> detectOnsets (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                              int startSample, int endSample, int maxOnsets);

        // Beat-grid slice points across [startSample, endSample) for a given BPM.
        // Produces slicesPerBeat divisions per quarter note (e.g. 1 = beats,
        // 4 = sixteenths). Returns boundary positions (count = slices + 1).
        static std::vector<int> sliceByBeatGrid (int startSample, int endSample, double sampleRate,
                                                 double bpm, int slicesPerBeat);
        static SampleMapHealthStatus evaluateHealth (const SampleMap& map,
                                                     const juce::File& projectFolder,
                                                     const juce::String& engineId);
        static juce::File resolveSamplePath (const juce::File& projectFolder,
                                             const juce::String& samplePath);

        juce::var toVar() const;
        void fromVar (const juce::var&);

    private:
        std::vector<SampleZoneDef> zones;
    };

} // namespace patchcraft
