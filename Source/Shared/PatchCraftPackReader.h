#pragma once

#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    class PatchCraftPackReader
    {
    public:
        // Reads pack metadata + JSON files. Sample WAV data is loaded later by
        // SampleSynthEngine on the message thread (never on the audio thread).
        bool read (const juce::File& packFolder, PatchCraftPack& outPack, juce::String& error);

    private:
        static juce::var loadJson (const juce::File& f, juce::String& error);
    };

} // namespace patchcraft
