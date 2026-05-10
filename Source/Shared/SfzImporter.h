#pragma once

#include "PatchCraftTypes.h"
#include "SampleMap.h"

namespace patchcraft
{
    /**
        Parses an SFZ v1/v2 text file and converts zones into PatchCraft's
        SampleZoneDef format.  Supports the most common opcodes needed for
        realistic instrument migration.
    */
    class SfzImporter
    {
    public:
        struct Result
        {
            juce::Array<SampleZoneDef> zones;
            juce::StringArray warnings;
            bool success = false;
        };

        static Result parseFile (const juce::File& sfzFile);

    private:
        struct RegionState
        {
            juce::String samplePath;
            int  lokey = 0, hikey = 127, key = -1;
            int  lovel = 1, hivel = 127;
            int  pitch_keycenter = -1;
            float volume = 0.0f;
            float pan = 0.0f;
            int  offset = 0;
            int  end = 0;
            bool loop_mode = false;
            int  loop_start = 0, loop_end = 0;
            int  group = 0;
            int  seq_position = 0, seq_length = 0;
            bool triggerRelease = false;
            bool oneShot = false;
        };

        static void applyOpcode (RegionState& state, const juce::String& opcode,
                                 const juce::String& value);
        static int  noteFromString (const juce::String& s);
        static SampleZoneDef stateToZone (const RegionState&, const juce::File& sfzDir);
    };

} // namespace patchcraft
