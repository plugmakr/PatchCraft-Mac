#pragma once

#include "PatchCraftTypes.h"
#include "ParameterModel.h"
#include "LayoutModel.h"
#include "SampleMap.h"

namespace patchcraft
{
    /**
        Runtime in-memory representation of a loaded .patchcraft pack.
        Read by PatchCraftPackReader, written by PatchCraftPackWriter.
    */
    struct PatchCraftPack
    {
        juce::File   rootFolder;     // path of the pack folder on disk
        Manifest     manifest;
        CanvasSize   canvasSize;
        ParameterModel parameters;
        LayoutModel    layout;
        SampleMap      sampleMap;
        DspGraph       dspGraph;
        std::vector<HostParameterSlot> hostParameterSlots;
        std::vector<Preset> presets;
        std::vector<InstrumentPatch> patches;
        std::vector<SectionPreset> sectionPresets;
        std::vector<ExpansionMetadata> expansions;
        std::vector<MidiMapping> midiMappings;
        juce::String backgroundImageRelative { "assets/background.png" };

        const Preset* findPreset (const juce::String& name) const;
        const Preset* findDefaultPreset() const;
        const InstrumentPatch* findPatch (const juce::String& id) const;
        const InstrumentPatch* findPatchForPreset (const Preset& preset) const;
        const InstrumentPatch* findDefaultPatch() const;
    };

    void ensurePresetBackedPatches (PatchCraftPack& pack, bool replaceExisting = false);

} // namespace patchcraft
