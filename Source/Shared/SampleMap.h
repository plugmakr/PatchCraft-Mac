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
