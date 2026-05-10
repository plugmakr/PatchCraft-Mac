#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    const Preset* PatchCraftPack::findPreset (const juce::String& name) const
    {
        for (auto& p : presets)
            if (p.name == name) return &p;
        return nullptr;
    }

    const Preset* PatchCraftPack::findDefaultPreset() const
    {
        for (auto& p : presets)
            if (p.isDefault) return &p;
        if (! manifest.defaultPreset.isEmpty())
            if (auto* p = findPreset (manifest.defaultPreset)) return p;
        return presets.empty() ? nullptr : &presets.front();
    }

    const InstrumentPatch* PatchCraftPack::findPatch (const juce::String& id) const
    {
        for (const auto& patch : patches)
            if (patch.id == id)
                return &patch;
        return nullptr;
    }

    const InstrumentPatch* PatchCraftPack::findPatchForPreset (const Preset& preset) const
    {
        if (preset.patchId.isNotEmpty())
            if (auto* patch = findPatch (preset.patchId))
                return patch;
        for (const auto& patch : patches)
            if (patch.name == preset.name)
                return &patch;
        return nullptr;
    }

    const InstrumentPatch* PatchCraftPack::findDefaultPatch() const
    {
        if (const auto* preset = findDefaultPreset())
            if (auto* patch = findPatchForPreset (*preset))
                return patch;
        for (const auto& patch : patches)
            if (patch.isDefault)
                return &patch;
        return patches.empty() ? nullptr : &patches.front();
    }

} // namespace patchcraft
