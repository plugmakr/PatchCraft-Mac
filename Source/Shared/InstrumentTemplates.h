#pragma once

#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    /**
        Build a fully-populated demo PatchCraftPack matching the engine type.
        Used by:
        - PatchCraftProject::resetToDefaultInstrument() / setEngineType()
        - PlayerProcessor's auto-loaded "factory" pack so the Player is never
          blank when it first opens in a DAW.

        The sampler default is a Cinematic Evolve Pad with full controls in
        every tab (Main / Amp / Filter / Mod / FX / Space / Arp).
    */
    PatchCraftPack buildDemoPack (const juce::String& engineId);
    void           buildDemoLayout (LayoutModel& layout, CanvasSize& canvas,
                                    const juce::String& engineId);

} // namespace patchcraft
