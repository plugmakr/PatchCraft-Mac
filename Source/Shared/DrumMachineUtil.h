#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    namespace DrumMachineUtil
    {
        juce::String defaultTrackLabel (int track);
        int defaultTrackNote (int track);

        juce::String cellPrefix (int pattern, int track, int step);

        void setCell (DspBlock& block,
                      int pattern,
                      int track,
                      int step,
                      bool active,
                      float velocity,
                      float gate,
                      float probability = 1.0f,
                      int divisions = 1);

        void ensureBlockDefaults (DspBlock& block);
        void seedFactoryPatterns (DspBlock& block);
        void seedEmptyPatterns (DspBlock& block);
        void clearPattern (DspBlock& block, int pattern, int tracks, int steps);
    }
}
