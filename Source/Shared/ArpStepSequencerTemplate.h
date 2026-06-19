#pragma once

#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    constexpr int kArpStepPatternCount = 6;

    /** Seeds a midiPlayground / step arp block with a musical 16-step pattern. */
    void seedArpStepPattern (DspBlock& block, int patternIndex);

    /** Full Studio template: aligned step-sequencer UI, musical DSP graph, and presets. */
    PatchCraftPack buildArpStepSequencerPack();
}
