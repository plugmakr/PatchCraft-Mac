#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    class MidiPlaygroundPattern
    {
    public:
        static constexpr int kStepCount = 128;
        static constexpr int kPhraseBankCount = 5;

        static int getActiveBank (const DspBlock&);
        static void setActiveBank (DspBlock&, int bank);
        static bool bankHasData (const DspBlock&, int bank);
        static void storeActiveBank (DspBlock&, int bank);
        static void loadBank (DspBlock&, int bank, bool seedFromActiveIfEmpty);
        static void copyBank (DspBlock&, int sourceBank, int destinationBank);
        static juce::StringArray getProgressionNames();
        static void applyProgressionPreset (DspBlock&, int progressionIndex, int bank);

        static bool writeMidiClip (const DspBlock&, const juce::File& targetFile,
                                   double bpm, int rootNote, juce::String& error);
    };
}
