#include "PatchCraftPackReader.h"

namespace patchcraft
{
    namespace
    {
        static bool tryAddReferencedRegistryParameter (ParameterModel& parameters,
                                                       const juce::String& parameterId,
                                                       const juce::String& engineId)
        {
            if (parameterId.isEmpty() || parameters.contains (parameterId))
                return true;

            if (parameterId == "mpActiveBank"
                || parameterId == "mpMultiLane"
                || parameterId == "mpRetrigger"
                || parameterId.startsWith ("mpBank"))
            {
                ParameterDef def;
                def.id = parameterId;
                def.name = parameterId;
                def.min = 0.0f;
                def.max = parameterId.containsIgnoreCase ("Bank") || parameterId.containsIgnoreCase ("Step") ? 128.0f : 1.0f;
                def.defaultValue = parameterId.endsWithIgnoreCase ("Retrigger") || parameterId == "mpRetrigger" ? 1.0f : 0.0f;
                def.category = "Arp Lane";
                def.section = "mod";
                def.hostAutomatable = false;
                def.midiLearnable = false;
                def.modulatable = false;
                def.visible = false;
                parameters.add (def);
                return true;
            }

            if (parameters.addFromRegistry (parameterId, engineId))
                return true;

            for (const auto& fallbackEngine : { juce::String ("synth"),
                                                juce::String ("sample"),
                                                juce::String ("fx") })
            {
                if (fallbackEngine == engineId)
                    continue;
                if (parameters.addFromRegistry (parameterId, fallbackEngine))
                    return true;
            }

            return false;
        }

        static void repairReferencedParameters (ParameterModel& parameters,
                                                const juce::String& engineId,
                                                const LayoutModel& layout,
                                                const DspGraph& graph,
                                                const std::vector<Preset>& presets)
        {
            parameters.ensureRegistryMetadata (engineId);

            for (const auto& element : layout.getAll())
                tryAddReferencedRegistryParameter (parameters, element.parameterId, engineId);

            for (const auto& block : graph.blocks)
                tryAddReferencedRegistryParameter (parameters, block.targetId, engineId);

            for (const auto& macro : graph.macros)
            {
                tryAddReferencedRegistryParameter (parameters, macro.macroId, engineId);
                tryAddReferencedRegistryParameter (parameters, macro.targetId, engineId);
            }

            for (const auto& route : graph.modulation)
            {
                tryAddReferencedRegistryParameter (parameters, route.sourceId, engineId);
                tryAddReferencedRegistryParameter (parameters, route.targetId, engineId);
            }

            for (const auto& lane : graph.automation)
                tryAddReferencedRegistryParameter (parameters, lane.targetId, engineId);

            for (const auto& preset : presets)
                for (const auto& value : preset.values)
                    tryAddReferencedRegistryParameter (parameters, value.first, engineId);
        }
    }

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
        const bool hasExplicitHostMap = hostMapFile.existsAsFile();
        if (hasExplicitHostMap)
        {
            auto hostMapVar = juce::JSON::parse (hostMapFile);
            if (auto* hostMapObj = hostMapVar.getDynamicObject())
                if (auto* arr = hostMapObj->getProperty ("slots").getArray())
                    for (const auto& item : *arr)
                        out.hostParameterSlots.push_back (HostParameterSlot::fromVar (item));
        }

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

        const bool looksLikeFactoryDemo = packFolder.getFullPathName().containsIgnoreCase ("FactoryDemos")
            || out.manifest.creator.equalsIgnoreCase ("PatchCraft")
            || out.manifest.category.containsIgnoreCase ("Demo");
        if (looksLikeFactoryDemo)
            ensurePresetBackedPatches (out, false);

        out.backgroundImageRelative = out.manifest.backgroundImage.isNotEmpty()
            ? out.manifest.backgroundImage : juce::String ("assets/background.png");

        repairReferencedParameters (out.parameters,
                                    out.manifest.engine.isNotEmpty() ? out.manifest.engine : juce::String ("synth"),
                                    out.layout,
                                    out.dspGraph,
                                    out.presets);

        if (! hasExplicitHostMap || out.hostParameterSlots.empty())
            out.hostParameterSlots = out.parameters.buildHostParameterSlots();

        error.clear();
        return true;
    }

} // namespace patchcraft
