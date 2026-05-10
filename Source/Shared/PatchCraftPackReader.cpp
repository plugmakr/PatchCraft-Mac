#include "PatchCraftPackReader.h"

namespace patchcraft
{
    juce::var PatchCraftPackReader::loadJson (const juce::File& f, juce::String& error)
    {
        if (! f.existsAsFile())
        {
            error = f.getFileName() + " missing.";
            return {};
        }
        auto v = juce::JSON::parse (f);
        if (! v.isObject())
        {
            error = f.getFileName() + " is not valid JSON.";
            return {};
        }
        return v;
    }

    bool PatchCraftPackReader::read (const juce::File& packFolder,
                                     PatchCraftPack& out, juce::String& error)
    {
        if (! packFolder.isDirectory())
        {
            error = "Pack folder does not exist: " + packFolder.getFullPathName();
            return false;
        }

        out.rootFolder = packFolder;

        auto manifestVar = loadJson (packFolder.getChildFile ("manifest.json"), error);
        if (! manifestVar.isObject()) return false;
        out.manifest = Manifest::fromVar (manifestVar);

        auto layoutVar = loadJson (packFolder.getChildFile ("layout.json"), error);
        if (! layoutVar.isObject()) return false;
        out.layout.fromVar (layoutVar, out.canvasSize);

        auto paramsVar = loadJson (packFolder.getChildFile ("parameters.json"), error);
        if (paramsVar.isObject())
            out.parameters.fromVar (paramsVar);

        out.hostParameterSlots.clear();
        auto hostMapFile = packFolder.getChildFile ("hostParameterMap.json");
        if (hostMapFile.existsAsFile())
        {
            auto hostMapVar = juce::JSON::parse (hostMapFile);
            if (auto* hostMapObj = hostMapVar.getDynamicObject())
                if (auto* arr = hostMapObj->getProperty ("slots").getArray())
                    for (const auto& item : *arr)
                        out.hostParameterSlots.push_back (HostParameterSlot::fromVar (item));
        }
        if (out.hostParameterSlots.empty())
            out.hostParameterSlots = out.parameters.buildHostParameterSlots();

        auto mappingsVar = loadJson (packFolder.getChildFile ("mappings.json"), error);
        if (mappingsVar.isObject())
            out.sampleMap.fromVar (mappingsVar);

        auto dspGraphFile = packFolder.getChildFile ("dspGraph.json");
        const bool hasDspGraphFile = dspGraphFile.existsAsFile();
        if (hasDspGraphFile)
            out.dspGraph.fromVar (juce::JSON::parse (dspGraphFile));
        if (! hasDspGraphFile && out.dspGraph.blocks.empty())
            out.dspGraph.resetForEngine (out.manifest.engine);

        out.presets.clear();
        auto presetsVar = juce::JSON::parse (packFolder.getChildFile ("presets.json"));
        if (auto* pd = presetsVar.getDynamicObject())
            if (auto* arr = pd->getProperty ("presets").getArray())
                for (auto& item : *arr)
                    out.presets.push_back (Preset::fromVar (item));

        out.patches.clear();
        auto patchesVar = juce::JSON::parse (packFolder.getChildFile ("patches.json"));
        if (auto* pd = patchesVar.getDynamicObject())
            if (auto* arr = pd->getProperty ("patches").getArray())
                for (auto& item : *arr)
                    out.patches.push_back (InstrumentPatch::fromVar (item));

        out.sectionPresets.clear();
        auto sectionPresetsVar = juce::JSON::parse (packFolder.getChildFile ("sectionPresets.json"));
        if (auto* spd = sectionPresetsVar.getDynamicObject())
            if (auto* arr = spd->getProperty ("sectionPresets").getArray())
                for (auto& item : *arr)
                    out.sectionPresets.push_back (SectionPreset::fromVar (item));

        out.expansions.clear();
        auto expansionsVar = juce::JSON::parse (packFolder.getChildFile ("expansions.json"));
        if (auto* ed = expansionsVar.getDynamicObject())
            if (auto* arr = ed->getProperty ("expansions").getArray())
                for (auto& item : *arr)
                    out.expansions.push_back (ExpansionMetadata::fromVar (item));

        out.midiMappings.clear();
        auto midiVar = juce::JSON::parse (packFolder.getChildFile ("midiMappings.json"));
        if (auto* md = midiVar.getDynamicObject())
            if (auto* arr = md->getProperty ("mappings").getArray())
                for (auto& item : *arr)
                    out.midiMappings.push_back (MidiMapping::fromVar (item));

        out.backgroundImageRelative = out.manifest.backgroundImage.isNotEmpty()
            ? out.manifest.backgroundImage : juce::String ("assets/background.png");

        error.clear();
        return true;
    }

} // namespace patchcraft
