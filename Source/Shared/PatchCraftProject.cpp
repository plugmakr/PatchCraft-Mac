#include "PatchCraftProject.h"
#include "InstrumentTemplates.h"
#include "PatchCraftPackReader.h"

#include <algorithm>

namespace patchcraft
{
    namespace
    {
        static float sanitiseSafeOscillatorTypeValue (float value)
        {
            return (float) juce::jlimit (0, 3, juce::roundToInt (value));
        }

        static float sanitiseSafeOscillatorTypeNormalised (float value)
        {
            if (value < 0.0f || value > 1.0f)
                value = sanitiseSafeOscillatorTypeValue (value) / 3.0f;

            const int safeStep = juce::jlimit (0, 3, juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * 3.0f));
            return safeStep / 3.0f;
        }

        static void ensureGraphMacroParameters (ParameterModel& parameters,
                                                LiveValueStore& liveValues,
                                                const DspGraph& graph)
        {
            for (const auto& block : graph.blocks)
            {
                if (! block.type.containsIgnoreCase ("macro"))
                    continue;
                if (parameters.find (block.id) != nullptr)
                    continue;
                ParameterDef def;
                def.id = block.id;
                def.name = block.name.isNotEmpty() ? block.name : block.id;
                def.min = 0.0f;
                def.max = 1.0f;
                def.defaultValue = 0.5f;
                def.category = "Macro";
                def.section = "mod";
                parameters.add (def);
                liveValues.getOrAddRaw (def.id, def.defaultValue);
            }
        }

        static void ensurePerformanceParameters (ParameterModel& parameters,
                                                 LiveValueStore& liveValues)
        {
            auto addIfMissing = [&] (const juce::String& id, const juce::String& name, float defaultValue)
            {
                if (parameters.find (id) == nullptr)
                {
                    ParameterDef def;
                    def.id = id;
                    def.name = name;
                    def.min = 0.0f;
                    def.max = 1.0f;
                    def.defaultValue = defaultValue;
                    def.step = 1.0f;
                    def.category = "Performance";
                    def.section = "out";
                    def.displayMode = "toggle";
                    parameters.add (def);
                }
                liveValues.getOrAddRaw (id, defaultValue);
            };

            if (parameters.find ("projectBpm") == nullptr)
            {
                ParameterDef def;
                def.id = "projectBpm";
                def.name = "Project BPM";
                def.min = 40.0f;
                def.max = 220.0f;
                def.defaultValue = 120.0f;
                def.step = 1.0f;
                def.unit = "BPM";
                def.category = "Performance";
                def.section = "out";
                def.displayMode = "continuous";
                def.hostAutomatable = false;
                def.modulatable = false;
                parameters.add (def);
            }
            liveValues.getOrAddRaw ("projectBpm", 120.0f);
            addIfMissing ("bpmSync", "BPM Sync", 1.0f);
            addIfMissing ("retrigger", "Retrigger", 1.0f);
        }

        static float sanitisePresetPlaybackValue (const juce::String& id, float value)
        {
            if (id == "noiseBlend")
                return 0.0f;

            if (id == "oscType" || id == "osc2Type")
                return sanitiseSafeOscillatorTypeValue (value);

            if (id == "oscBlend")
                return juce::jlimit (0.0f, 0.24f, value);

            if (id == "filterResonance")
                return juce::jlimit (0.0f, 0.32f, value);

            if (id == "delayFeedback")
                return juce::jlimit (0.0f, 0.45f, value);

            if (id == "reverbMix")
                return juce::jlimit (0.0f, 0.40f, value);

            if (id == "detune")
                return juce::jlimit (-8.0f, 8.0f, value);

            if (id == "osc2Detune")
                return juce::jlimit (-6.0f, 6.0f, value);

            if (id == "wtWarp")
                return juce::jlimit (0.0f, 0.35f, value);

            if (id == "wtFold")
                return juce::jlimit (0.0f, 0.20f, value);

            if (id == "wtLevel")
                return juce::jlimit (0.0f, 0.78f, value);

            return value;
        }

        static float sanitiseGraphPlaybackValue (const juce::String& id, float value)
        {
            if (id == "noiseBlend")
                return 0.0f;

            if (id == "oscType" || id == "osc2Type")
                return sanitiseSafeOscillatorTypeNormalised (value);

            if (id == "oscBlend")
                return juce::jlimit (0.0f, 0.24f, value);

            if (id == "filterResonance")
                return juce::jlimit (0.0f, 0.32f, value);

            if (id == "delayFeedback")
                return juce::jlimit (0.0f, 0.45f, value);

            if (id == "detune" || id == "osc2Detune")
            {
                const float cents = id == "detune"
                    ? juce::jlimit (-8.0f, 8.0f, value)
                    : juce::jlimit (-6.0f, 6.0f, value);
                if (value < 0.0f || value > 1.0f)
                    return juce::jmap (cents, -50.0f, 50.0f, 0.0f, 1.0f);

                const float safeMin = id == "detune" ? 0.42f : 0.44f;
                const float safeMax = id == "detune" ? 0.58f : 0.56f;
                return juce::jlimit (safeMin, safeMax, value);
            }

            if (id == "wtWarp")
                return juce::jlimit (0.0f, 0.35f, value);

            if (id == "wtFold")
                return juce::jlimit (0.0f, 0.20f, value);

            if (id == "wtLevel")
                return juce::jlimit (0.0f, 0.78f, value);

            return value;
        }

        static void sanitisePlaybackGraph (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
            {
                for (auto& value : block.values)
                    value.second = sanitiseGraphPlaybackValue (value.first, value.second);

                if (block.targetId == "noiseBlend" || block.type.containsIgnoreCase ("noise"))
                {
                    block.values["noiseBlend"] = 0.0f;
                    block.values["amount"] = 0.0f;
                    block.values["value"] = 0.0f;
                    block.enabled = false;
                }
            }
        }

        static void sanitiseMusicalLiveValues (const ParameterModel& parameters,
                                               LiveValueStore& liveValues)
        {
            for (const auto& def : parameters.getAll())
            {
                auto value = liveValues.getValue (def.id, def.defaultValue);
                value = sanitisePresetPlaybackValue (def.id, value);
                liveValues.setValue (def.id, value);
            }
        }

        static void syncPlaybackGraphFromLiveValues (DspGraph& graph,
                                                     const ParameterModel& parameters,
                                                     const LiveValueStore& liveValues)
        {
            for (auto& block : graph.blocks)
            {
                for (auto& entry : block.values)
                {
                    if (entry.first == "bank")
                        continue;
                    if (const auto* def = parameters.find (entry.first))
                        entry.second = sanitiseGraphPlaybackValue (entry.first,
                            liveValues.getValue (entry.first, def->defaultValue));
                }
            }

            sanitisePlaybackGraph (graph);
        }
    }

    PatchCraftProject::PatchCraftProject()
    {
        scriptEngine.bindStore (&liveValues);
        resetToDefaultInstrument();
    }

    void PatchCraftProject::resetToDefaultInstrument()
    {
        // Synth is the default new-project engine: it's audible immediately
        // (no samples needed) so the user always hears something on first run.
        auto pack = buildDemoPack ("synth");

        manifest = pack.manifest;
        manifest.description = "A demo cinematic pad for PatchCraft.";
        canvasSize  = pack.canvasSize;
        parameters  = pack.parameters;
        parameters.ensureRegistryMetadata (manifest.engine);
        layout      = pack.layout;
        sampleMap.clear();
        presets     = pack.presets;
        patches.clear();
        sectionPresets.clear();
        expansions.clear();
        midiMappings = pack.midiMappings;
        backgroundImageRelative = pack.backgroundImageRelative;
        dspGraph.resetForEngine (manifest.engine);
        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);

        resetLiveValuesToDefaults();
        auto defaultPatch = captureCurrentPatch (manifest.defaultPreset.isNotEmpty()
            ? manifest.defaultPreset : manifest.instrumentName + " Patch");
        defaultPatch.isDefault = true;
        patches.push_back (defaultPatch);
        // Don't auto-link the first preset's patchId to the default patch:
        // doing so makes that preset's own values get overridden by the
        // captured-defaults patch on selection, which masked the trance
        // preset bank's tonal differences.
        if (! presets.empty())
            presets.front().isDefault = true;
        dirty = false;
    }

    void PatchCraftProject::resetCanvasToBlank()
    {
        layout.clear();
        backgroundImageRelative.clear();
        manifest.backgroundImage.clear();
        notifyChanged();
    }

    // -------------------------------------------------------------------------
    // Engine switching
    // -------------------------------------------------------------------------
    void PatchCraftProject::setEngineType (const juce::String& engineId)
    {
        const auto previousGraph = dspGraph;
        const bool preserveUserGraph = previousGraph.userConfigured;

        // Build a fully-populated demo pack for the new engine and copy its
        // contents in. This gives every engine type the same level of polish:
        // all tabs filled, full preset list, parameter defaults tuned to be
        // audible.
        auto pack = buildDemoPack (engineId);

        // "drum" is a Sampler-engine variant whose layout uses a PadGrid.
        // Persist the manifest engine as "sample" so every UI that gates on
        // sampler behaviour keeps working; the "drumness" is implied by the
        // layout content (presence of an ElementType::PadGrid element).
        manifest.engine = engineId == "drum" ? juce::String ("sample") : engineId;
        manifest.instrumentName = pack.manifest.instrumentName;
        manifest.defaultPreset  = pack.manifest.defaultPreset;
        canvasSize  = pack.canvasSize;
        parameters  = pack.parameters;
        parameters.ensureRegistryMetadata (engineId);
        layout      = pack.layout;
        presets     = pack.presets;
        patches.clear();
        sectionPresets.clear();
        midiMappings = pack.midiMappings;
        backgroundImageRelative = pack.backgroundImageRelative;
        if (preserveUserGraph)
            dspGraph = previousGraph;
        else
            dspGraph.resetForEngine (engineId);
        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);

        resetLiveValuesToDefaults();
        auto defaultPatch = captureCurrentPatch (manifest.defaultPreset.isNotEmpty()
            ? manifest.defaultPreset : manifest.instrumentName + " Patch");
        defaultPatch.isDefault = true;
        patches.push_back (defaultPatch);
        // See note in resetToDefaultInstrument: do not auto-link the first
        // preset's patchId to this captured-defaults patch.
        notifyChanged();
    }

    bool PatchCraftProject::loadRuntimePackAsProject (const juce::File& packFolder, juce::String& error)
    {
        PatchCraftPack pack;
        PatchCraftPackReader reader;
        if (! reader.read (packFolder, pack, error))
            return false;

        projectFolder = packFolder;
        manifest = pack.manifest;
        canvasSize = pack.canvasSize;
        parameters = pack.parameters;
        parameters.ensureRegistryMetadata (manifest.engine);
        layout = pack.layout;
        sampleMap = pack.sampleMap;
        presets = pack.presets;
        patches = pack.patches;
        sectionPresets = pack.sectionPresets;
        expansions = pack.expansions;
        midiMappings = pack.midiMappings;
        dspGraph = pack.dspGraph;
        sanitisePlaybackGraph (dspGraph);
        backgroundImageRelative = pack.backgroundImageRelative;

        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);
        resetLiveValuesToDefaults();

        const auto defaultName = manifest.defaultPreset;
        auto presetToApply = std::find_if (presets.begin(), presets.end(),
            [&] (const Preset& preset)
            {
                return (defaultName.isNotEmpty() && preset.name == defaultName) || preset.isDefault;
            });

        if (presetToApply != presets.end())
        {
            for (const auto& value : presetToApply->values)
                liveValues.setValue (value.first, sanitisePresetPlaybackValue (value.first, value.second));
            sanitiseMusicalLiveValues (parameters, liveValues);
            syncPlaybackGraphFromLiveValues (dspGraph, parameters, liveValues);
            manifest.defaultPreset = presetToApply->name;
            for (auto& preset : presets)
                preset.isDefault = preset.name == presetToApply->name;
        }

        dirty = false;
        notifyChanged();
        error.clear();
        return true;
    }

    void PatchCraftProject::resetLiveValuesToDefaults()
    {
        liveValues.clear();
        for (auto& def : parameters.getAll())
            liveValues.getOrAddRaw (def.id, def.defaultValue);
    }

    namespace
    {
        static juce::String makeStableContentId (juce::String name)
        {
            name = name.trim().toLowerCase();
            if (name.isEmpty())
                name = "untitled";
            juce::String out;
            for (int i = 0; i < name.length(); ++i)
            {
                const auto c = name[i];
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    out += c;
                else if (c == ' ' || c == '-' || c == '_')
                    out += '_';
            }
            while (out.contains ("__"))
                out = out.replace ("__", "_");
            out = out.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
            return out.isNotEmpty() ? out : "untitled";
        }

        static void addAssetReference (juce::StringArray& refs, const juce::String& ref)
        {
            if (ref.isNotEmpty())
                refs.addIfNotAlreadyThere (ref);
        }
    }

    InstrumentPatch PatchCraftProject::captureCurrentPatch (const juce::String& name) const
    {
        InstrumentPatch patch;
        patch.name = name.trim().isNotEmpty() ? name.trim() : manifest.instrumentName + " Patch";
        patch.id = makeStableContentId (patch.name);
        patch.description = "Full playable PatchCraft sound state.";
        patch.engine = manifest.engine;
        patch.category = manifest.category;
        patch.author = manifest.creator;
        patch.version = manifest.version;
        patch.packId = manifest.instrumentName;
        patch.dspGraph = dspGraph;
        patch.sampleZones = sampleMap.getZones();
        patch.midiMappings = midiMappings;
        patch.tags = manifest.tags;
        patch.tags.addIfNotAlreadyThere (manifest.engine);

        addAssetReference (patch.includedAssets, backgroundImageRelative);
        addAssetReference (patch.includedAssets, manifest.backgroundImage);
        addAssetReference (patch.includedAssets, manifest.libraryThumbnail);
        addAssetReference (patch.includedAssets, manifest.playerLogoImage);
        addAssetReference (patch.includedAssets, manifest.playerTitleBannerImage);
        for (const auto& element : layout.getAll())
        {
            addAssetReference (patch.includedAssets, element.asset);
            addAssetReference (patch.includedAssets, element.filmstripAsset);
        }
        for (const auto& zone : sampleMap.getZones())
        {
            addAssetReference (patch.includedAssets, zone.samplePath);
            addAssetReference (patch.includedAssets, zone.midiPath);
        }
        patch.libraryReferences = patch.includedAssets;

        for (const auto& def : parameters.getAll())
            patch.parameterValues[def.id] = liveValues.getValue (def.id, def.defaultValue);
        return patch;
    }

    bool PatchCraftProject::applyPatch (const InstrumentPatch& patch)
    {
        if (patch.id.isEmpty() && patch.name.isEmpty())
            return false;

        const auto patchName = patch.name.trim().isNotEmpty() ? patch.name.trim() : manifest.defaultPreset;
        if (patchName.isNotEmpty())
            manifest.defaultPreset = patchName;

        if (patch.engine.isNotEmpty() && patch.engine != manifest.engine)
        {
            manifest.engine = patch.engine;
            parameters.ensureRegistryMetadata (manifest.engine);
        }

        const bool hasGraphState = ! patch.dspGraph.blocks.empty()
            || ! patch.dspGraph.edges.empty()
            || ! patch.dspGraph.macros.empty()
            || ! patch.dspGraph.modulation.empty()
            || ! patch.dspGraph.automation.empty()
            || patch.dspGraph.userConfigured;
        if (hasGraphState)
        {
            dspGraph = patch.dspGraph;
            sanitisePlaybackGraph (dspGraph);
        }
        else if (dspGraph.blocks.empty())
            dspGraph.resetForEngine (manifest.engine);

        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);

        sampleMap.clear();
        for (const auto& zone : patch.sampleZones)
            sampleMap.add (zone);

        midiMappings = patch.midiMappings;

        resetLiveValuesToDefaults();
        for (const auto& value : patch.parameterValues)
            liveValues.setValue (value.first, sanitisePresetPlaybackValue (value.first, value.second));
        sanitiseMusicalLiveValues (parameters, liveValues);
        syncPlaybackGraphFromLiveValues (dspGraph, parameters, liveValues);

        for (auto& existing : patches)
            existing.isDefault = (existing.id.isNotEmpty() && existing.id == patch.id)
                || (existing.name.isNotEmpty() && existing.name == patch.name);
        for (auto& existing : presets)
            existing.isDefault = (existing.patchId.isNotEmpty() && existing.patchId == patch.id)
                || (existing.name.isNotEmpty() && existing.name == patch.name);

        notifyChanged();
        return true;
    }

    bool PatchCraftProject::applyPreset (const Preset& preset)
    {
        const InstrumentPatch* patchToApply = nullptr;
        for (const auto& patch : patches)
        {
            if ((preset.patchId.isNotEmpty() && patch.id == preset.patchId)
                || (preset.name.isNotEmpty() && patch.name == preset.name))
            {
                patchToApply = &patch;
                break;
            }
        }

        if (patchToApply != nullptr)
        {
            // Apply the patch first (which restores DSP graph, sample zones,
            // MIDI mappings, and parameter snapshot), then overlay the
            // preset's own .values so the preset is always authoritative.
            // Previously a preset that matched a patch would only play the
            // patch's recorded state and silently discard the preset's
            // parameter table, so e.g. all 50 trance presets sounded like
            // the default factory patch.
            applyPatch (*patchToApply);
            for (const auto& value : preset.values)
                liveValues.setValue (value.first, sanitisePresetPlaybackValue (value.first, value.second));
            sanitiseMusicalLiveValues (parameters, liveValues);
            syncPlaybackGraphFromLiveValues (dspGraph, parameters, liveValues);
            if (preset.name.isNotEmpty())
                manifest.defaultPreset = preset.name;
            for (auto& existing : presets)
                existing.isDefault = existing.name == preset.name;
            notifyChanged();
            return true;
        }

        if (preset.name.isNotEmpty())
            manifest.defaultPreset = preset.name;
        resetLiveValuesToDefaults();
        for (const auto& value : preset.values)
            liveValues.setValue (value.first, sanitisePresetPlaybackValue (value.first, value.second));
        sanitiseMusicalLiveValues (parameters, liveValues);
        syncPlaybackGraphFromLiveValues (dspGraph, parameters, liveValues);

        for (auto& existing : presets)
            existing.isDefault = existing.name == preset.name;

        notifyChanged();
        return true;
    }

    SectionPreset PatchCraftProject::captureSectionPreset (const juce::String& section,
                                                           const juce::String& name) const
    {
        SectionPreset preset;
        preset.section = section.toLowerCase();
        preset.name = name.trim().isNotEmpty() ? name.trim()
            : preset.section.toUpperCase() + " Section Preset";
        preset.id = makeStableContentId (preset.section + "_" + preset.name);
        preset.description = "Reusable " + preset.section + " DSP section preset.";
        preset.engine = manifest.engine;
        preset.category = preset.section;
        preset.packId = manifest.instrumentName;
        preset.tags.addIfNotAlreadyThere (preset.section);
        preset.tags.addIfNotAlreadyThere (manifest.engine);

        juce::StringArray blockIds;
        for (const auto& block : dspGraph.blocks)
        {
            if (block.section.equalsIgnoreCase (preset.section))
            {
                preset.blocks.push_back (block);
                blockIds.addIfNotAlreadyThere (block.id);
            }
        }

        for (const auto& edge : dspGraph.edges)
            if (blockIds.contains (edge.sourceNodeId, false) || blockIds.contains (edge.targetNodeId, false))
                preset.edges.push_back (edge);
        for (const auto& macro : dspGraph.macros)
            if (blockIds.contains (macro.macroId, false) || blockIds.contains (macro.targetId, false))
                preset.macros.push_back (macro);
        for (const auto& route : dspGraph.modulation)
            if (blockIds.contains (route.sourceId, false) || blockIds.contains (route.targetId, false))
                preset.modulation.push_back (route);
        for (const auto& lane : dspGraph.automation)
            if (blockIds.contains (lane.targetId, false))
                preset.automation.push_back (lane);

        for (const auto& def : parameters.getAll())
            if (def.section.equalsIgnoreCase (preset.section))
                preset.parameterValues[def.id] = liveValues.getValue (def.id, def.defaultValue);
        return preset;
    }

    bool PatchCraftProject::applySectionPreset (const SectionPreset& preset,
                                                bool replaceCurrentSection,
                                                juce::String& error)
    {
        if (preset.section.isEmpty())
        {
            error = "Section preset has no target section.";
            return false;
        }

        if (replaceCurrentSection)
        {
            juce::StringArray removedIds;
            for (const auto& block : dspGraph.blocks)
                if (block.section.equalsIgnoreCase (preset.section))
                    removedIds.addIfNotAlreadyThere (block.id);

            dspGraph.blocks.erase (std::remove_if (dspGraph.blocks.begin(), dspGraph.blocks.end(),
                [&] (const DspBlock& block) { return block.section.equalsIgnoreCase (preset.section); }),
                dspGraph.blocks.end());
            dspGraph.edges.erase (std::remove_if (dspGraph.edges.begin(), dspGraph.edges.end(),
                [&] (const DspGraphEdge& edge)
                {
                    return removedIds.contains (edge.sourceNodeId, false)
                        || removedIds.contains (edge.targetNodeId, false);
                }), dspGraph.edges.end());
        }

        auto makeUniqueBlockId = [&] (const juce::String& preferred)
        {
            auto candidate = preferred.isNotEmpty() ? preferred : makeStableContentId (preset.name);
            auto exists = [&] (const juce::String& id)
            {
                for (const auto& block : dspGraph.blocks)
                    if (block.id == id)
                        return true;
                return false;
            };
            int suffix = 2;
            while (exists (candidate))
                candidate = preferred + "_" + juce::String (suffix++);
            return candidate;
        };

        std::map<juce::String, juce::String> remappedIds;
        for (auto block : preset.blocks)
        {
            const auto oldId = block.id;
            block.section = preset.section;
            block.id = makeUniqueBlockId (block.id);
            remappedIds[oldId] = block.id;
            dspGraph.blocks.push_back (std::move (block));
        }

        auto remap = [&] (const juce::String& id)
        {
            const auto it = remappedIds.find (id);
            return it == remappedIds.end() ? id : it->second;
        };
        for (auto edge : preset.edges)
        {
            edge.sourceNodeId = remap (edge.sourceNodeId);
            edge.targetNodeId = remap (edge.targetNodeId);
            edge.id = edge.sourceNodeId + "_to_" + edge.targetNodeId;
            dspGraph.edges.push_back (std::move (edge));
        }
        for (auto macro : preset.macros)
        {
            macro.id = makeStableContentId (preset.id + "_" + macro.id);
            macro.macroId = remap (macro.macroId);
            macro.targetId = remap (macro.targetId);
            dspGraph.macros.push_back (std::move (macro));
        }
        for (auto route : preset.modulation)
        {
            route.id = makeStableContentId (preset.id + "_" + route.id);
            route.sourceId = remap (route.sourceId);
            route.targetId = remap (route.targetId);
            dspGraph.modulation.push_back (std::move (route));
        }
        for (auto lane : preset.automation)
        {
            lane.id = makeStableContentId (preset.id + "_" + lane.id);
            lane.targetId = remap (lane.targetId);
            dspGraph.automation.push_back (std::move (lane));
        }
        for (const auto& value : preset.parameterValues)
            liveValues.setValue (value.first, value.second);

        dspGraph.userConfigured = true;
        notifyChanged();
        return true;
    }

    ExpansionMetadata& PatchCraftProject::ensureExpansion (const juce::String& idOrName)
    {
        const auto id = makeStableContentId (idOrName.isNotEmpty() ? idOrName : manifest.instrumentName + " Expansion");
        for (auto& expansion : expansions)
            if (expansion.id == id || expansion.name == idOrName)
                return expansion;

        ExpansionMetadata metadata;
        metadata.id = id;
        metadata.name = idOrName.isNotEmpty() ? idOrName : manifest.instrumentName + " Expansion";
        metadata.description = manifest.description;
        metadata.author = manifest.creator;
        metadata.brand = manifest.playerDisplayName.isNotEmpty() ? manifest.playerDisplayName : manifest.creator;
        metadata.category = manifest.category;
        metadata.version = manifest.version;
        metadata.tags = manifest.tags;
        expansions.push_back (std::move (metadata));
        return expansions.back();
    }

    // -------------------------------------------------------------------------
    // Undo / redo
    // -------------------------------------------------------------------------
    namespace
    {
        // Captures the entire layout vector before/after a mutation. Plenty
        // fast for hand-built instrument layouts of a few hundred elements.
        class LayoutSnapshotAction : public juce::UndoableAction
        {
        public:
            LayoutSnapshotAction (PatchCraftProject& projIn,
                                  std::vector<LayoutElement> beforeIn,
                                  std::vector<LayoutElement> afterIn,
                                  juce::String selectionBeforeIn,
                                  juce::String selectionAfterIn)
                : project (projIn),
                  before (std::move (beforeIn)),
                  after  (std::move (afterIn)),
                  selectionBefore (std::move (selectionBeforeIn)),
                  selectionAfter  (std::move (selectionAfterIn))
            {}

            bool perform() override
            {
                project.getLayout().getAll() = after;
                lastSelectionApplied = selectionAfter;
                project.notifyChanged();
                return true;
            }

            bool undo() override
            {
                project.getLayout().getAll() = before;
                lastSelectionApplied = selectionBefore;
                project.notifyChanged();
                return true;
            }

            int getSizeInUnits() override
            {
                return (int) ((before.size() + after.size()) * sizeof (LayoutElement));
            }

            juce::String lastSelectionApplied;

        private:
            PatchCraftProject& project;
            std::vector<LayoutElement> before, after;
            juce::String selectionBefore, selectionAfter;
        };

        class SampleMapSnapshotAction : public juce::UndoableAction
        {
        public:
            SampleMapSnapshotAction (PatchCraftProject& projIn,
                                     std::vector<SampleZoneDef> beforeIn,
                                     std::vector<SampleZoneDef> afterIn)
                : project (projIn),
                  before (std::move (beforeIn)),
                  after  (std::move (afterIn))
            {}

            bool perform() override
            {
                project.getSampleMap().getZones() = after;
                project.notifyChanged();
                return true;
            }

            bool undo() override
            {
                project.getSampleMap().getZones() = before;
                project.notifyChanged();
                return true;
            }

            int getSizeInUnits() override
            {
                return (int) ((before.size() + after.size()) * sizeof (SampleZoneDef));
            }

        private:
            PatchCraftProject& project;
            std::vector<SampleZoneDef> before, after;
        };
    }

    void PatchCraftProject::performLayoutEdit (const juce::String& actionName,
                                               std::function<void (LayoutModel&)> mutator)
    {
        if (! mutator) return;
        const auto before = layout.getAll();
        mutator (layout);
        const auto after = layout.getAll();

        undoManager.beginNewTransaction (actionName);
        // Build the action with state already applied; perform() is a no-op
        // re-apply for redo-after-undo.
        undoManager.perform (new LayoutSnapshotAction (
            *this, before, after, juce::String(), juce::String()));
        notifyChanged();
    }

    void PatchCraftProject::performSampleMapEdit (const juce::String& actionName,
                                                  std::function<void (SampleMap&)> mutator)
    {
        if (! mutator) return;
        const auto before = sampleMap.getZones();
        mutator (sampleMap);
        const auto after = sampleMap.getZones();

        undoManager.beginNewTransaction (actionName);
        undoManager.perform (new SampleMapSnapshotAction (*this, before, after));
        notifyChanged();
    }

    void PatchCraftProject::undo()
    {
        if (undoManager.canUndo()) undoManager.undo();
    }

    void PatchCraftProject::redo()
    {
        if (undoManager.canRedo()) undoManager.redo();
    }

    juce::File PatchCraftProject::getAssetsFolder() const
    {
        return projectFolder.getChildFile ("assets");
    }

    juce::File PatchCraftProject::getSamplesFolder() const
    {
        return projectFolder.getChildFile ("samples");
    }

    juce::File PatchCraftProject::getKnobAssetsFolder() const
    {
        return projectFolder.getChildFile ("assets/knobs");
    }

    void PatchCraftProject::notifyChanged (ChangeScope scope)
    {
        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);
        markDirty();
        listeners.call ([scope] (Listener& l) { l.projectChanged (scope); });
    }

    bool PatchCraftProject::save (const juce::File& folder, juce::String& error)
    {
        if (! folder.exists() && ! folder.createDirectory())
        {
            error = "Could not create project folder: " + folder.getFullPathName();
            return false;
        }
        projectFolder = folder;
        getAssetsFolder().createDirectory();
        getSamplesFolder().createDirectory();
        getKnobAssetsFolder().createDirectory();
        folder.getChildFile ("assets/sliders").createDirectory();
        folder.getChildFile ("assets/meters").createDirectory();
        folder.getChildFile ("assets/images").createDirectory();
        folder.getChildFile ("exports").createDirectory();

        auto copyAssetIntoProject = [&] (juce::String& path, const juce::String& subFolder,
                                         const juce::String& fallbackName) -> bool
        {
            if (path.isEmpty())
                return true;

            juce::File src = juce::File::isAbsolutePath (path)
                ? juce::File (path)
                : folder.getChildFile (path);

            if (! src.existsAsFile())
                return true;

            if (! juce::File::isAbsolutePath (path) && src.isAChildOf (folder))
                return true;

            auto dstDir = folder.getChildFile (subFolder);
            dstDir.createDirectory();
            auto dstName = fallbackName.isNotEmpty() ? fallbackName : src.getFileName();
            auto dst = dstDir.getChildFile (dstName);
            if (src != dst && ! src.copyFileTo (dst))
                return false;
            path = dst.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
            return true;
        };

        juce::StringArray usedSampleNames;
        auto copySampleIntoProject = [&] (SampleZoneDef& zone) -> bool
        {
            if (zone.samplePath.isEmpty())
                return true;

            juce::File src = juce::File::isAbsolutePath (zone.samplePath)
                ? juce::File (zone.samplePath)
                : folder.getChildFile (zone.samplePath);

            if (! src.existsAsFile())
            {
                error = "Missing sample during project save: " + zone.samplePath;
                return false;
            }

            if (! juce::File::isAbsolutePath (zone.samplePath) && src.isAChildOf (folder))
            {
                usedSampleNames.addIfNotAlreadyThere (src.getFileName(), true);
                zone.samplePath = src.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
                return true;
            }

            auto dstDir = folder.getChildFile ("samples");
            dstDir.createDirectory();

            const auto base = src.getFileNameWithoutExtension();
            const auto ext = src.getFileExtension();
            auto dstName = src.getFileName();
            int suffix = 2;
            while (usedSampleNames.contains (dstName, true))
                dstName = base + "_" + juce::String (suffix++) + ext;
            usedSampleNames.add (dstName);

            auto dst = dstDir.getChildFile (dstName);
            if (src != dst)
            {
                if (dst.existsAsFile() && ! dst.deleteFile())
                {
                    error = "Could not replace project sample: " + dst.getFullPathName();
                    return false;
                }
                if (! src.copyFileTo (dst))
                {
                    error = "Failed to copy sample into project: " + src.getFullPathName();
                    return false;
                }
            }

            zone.samplePath = dst.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
            return true;
        };

        if (! copyAssetIntoProject (backgroundImageRelative, "assets", "background" + juce::File (backgroundImageRelative).getFileExtension()))
        {
            error = "Failed to copy background artwork into project assets.";
            return false;
        }
        if (backgroundImageRelative.isEmpty())
            backgroundImageRelative = "assets/background.png";
        manifest.backgroundImage = backgroundImageRelative;

        for (auto& e : layout.getAll())
        {
            if (e.type == ElementType::Image)
            {
                if (! copyAssetIntoProject (e.asset, "assets/images", {}))
                {
                    error = "Failed to copy image asset: " + e.asset;
                    return false;
                }
            }
            if (e.filmstripAsset.isNotEmpty())
            {
                auto sub = e.type == ElementType::Slider ? "assets/sliders"
                         : e.type == ElementType::Meter  ? "assets/meters"
                                                         : "assets/knobs";
                if (! copyAssetIntoProject (e.filmstripAsset, sub, {}))
                {
                    error = "Failed to copy filmstrip asset: " + e.filmstripAsset;
                    return false;
                }
            }
        }

        for (auto& zone : sampleMap.getZones())
            if (! copySampleIntoProject (zone))
                return false;

        auto currentPatchName = manifest.defaultPreset.isNotEmpty()
            ? manifest.defaultPreset
            : manifest.instrumentName + " Patch";
        int defaultPatchIndex = -1;
        for (int i = 0; i < (int) patches.size(); ++i)
        {
            if (patches[(size_t) i].isDefault)
            {
                defaultPatchIndex = i;
                if (patches[(size_t) i].name.isNotEmpty())
                    currentPatchName = patches[(size_t) i].name;
                break;
            }
        }

        auto currentPatch = captureCurrentPatch (currentPatchName);
        currentPatch.isDefault = true;
        if (defaultPatchIndex >= 0)
            patches[(size_t) defaultPatchIndex] = currentPatch;
        else
        {
            patches.push_back (currentPatch);
            defaultPatchIndex = (int) patches.size() - 1;
        }
        for (int i = 0; i < (int) patches.size(); ++i)
            if (i != defaultPatchIndex && patches[(size_t) i].id == currentPatch.id)
                patches[(size_t) i].isDefault = false;

        int defaultPresetIndex = -1;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].isDefault || presets[(size_t) i].name == manifest.defaultPreset)
            {
                defaultPresetIndex = i;
                break;
            }

        auto currentPreset = currentPatch.toPreset();
        currentPreset.isDefault = true;
        if (defaultPresetIndex >= 0)
        {
            const auto existingName = presets[(size_t) defaultPresetIndex].name;
            presets[(size_t) defaultPresetIndex] = currentPreset;
            if (existingName.isNotEmpty())
                presets[(size_t) defaultPresetIndex].name = existingName;
        }
        else
        {
            presets.push_back (currentPreset);
            defaultPresetIndex = (int) presets.size() - 1;
        }
        for (int i = 0; i < (int) presets.size(); ++i)
            presets[(size_t) i].isDefault = (i == defaultPresetIndex);
        manifest.defaultPreset = presets[(size_t) defaultPresetIndex].name;

        auto* root = new juce::DynamicObject();
        root->setProperty ("formatVersion", kFormatVersion);
        root->setProperty ("manifest",   manifest.toVar());
        root->setProperty ("layout",     layout.toVar (canvasSize));
        root->setProperty ("parameters", parameters.toVar());
        root->setProperty ("mappings",   sampleMap.toVar());
        root->setProperty ("background", backgroundImageRelative);
        root->setProperty ("dspGraph",   dspGraph.toVar());

        juce::Array<juce::var> presetArr;
        for (auto& p : presets) presetArr.add (p.toVar());
        auto* presetsObj = new juce::DynamicObject();
        presetsObj->setProperty ("presets", presetArr);
        root->setProperty ("presetData", juce::var (presetsObj));

        juce::Array<juce::var> patchArr;
        for (auto& patch : patches) patchArr.add (patch.toVar());
        auto* patchObj = new juce::DynamicObject();
        patchObj->setProperty ("patches", patchArr);
        root->setProperty ("patchData", juce::var (patchObj));

        juce::Array<juce::var> sectionPresetArr;
        for (auto& preset : sectionPresets) sectionPresetArr.add (preset.toVar());
        auto* sectionPresetObj = new juce::DynamicObject();
        sectionPresetObj->setProperty ("sectionPresets", sectionPresetArr);
        root->setProperty ("sectionPresetData", juce::var (sectionPresetObj));

        juce::Array<juce::var> expansionArr;
        for (auto& expansion : expansions) expansionArr.add (expansion.toVar());
        auto* expansionObj = new juce::DynamicObject();
        expansionObj->setProperty ("expansions", expansionArr);
        root->setProperty ("expansionData", juce::var (expansionObj));

        juce::Array<juce::var> midiArr;
        for (const auto& mapping : midiMappings)
            midiArr.add (mapping.toVar());
        auto* midiObj = new juce::DynamicObject();
        midiObj->setProperty ("mappings", midiArr);
        root->setProperty ("midiMappings", juce::var (midiObj));

        auto json = juce::JSON::toString (juce::var (root), true);
        if (! folder.getChildFile ("project.json").replaceWithText (json))
        {
            error = "Failed to write project.json";
            return false;
        }
        folder.getChildFile ("pscript.txt").replaceWithText (pscriptSource);
        markClean();
        return true;
    }

    bool PatchCraftProject::load (const juce::File& folder, juce::String& error)
    {
        auto pj = folder.getChildFile ("project.json");
        if (! pj.existsAsFile())
        {
            error = "project.json not found in " + folder.getFullPathName();
            return false;
        }
        auto parsed = juce::JSON::parse (pj);
        if (! parsed.isObject())
        {
            error = "project.json is not valid JSON.";
            return false;
        }
        auto* root = parsed.getDynamicObject();
        manifest = Manifest::fromVar (root->getProperty ("manifest"));
        layout.fromVar (root->getProperty ("layout"), canvasSize);
        parameters.fromVar (root->getProperty ("parameters"));
        parameters.ensureRegistryMetadata (manifest.engine);
        sampleMap.fromVar (root->getProperty ("mappings"));
        backgroundImageRelative = root->getProperty ("background").toString();
        if (backgroundImageRelative.isEmpty())
            backgroundImageRelative = manifest.backgroundImage.isNotEmpty()
                ? manifest.backgroundImage : juce::String ("assets/background.png");
        manifest.backgroundImage = backgroundImageRelative;

        const auto dspGraphVar = root->getProperty ("dspGraph");
        dspGraph.fromVar (dspGraphVar);
        if (! dspGraphVar.isObject() && dspGraph.blocks.empty())
            dspGraph.resetForEngine (manifest.engine);
        ensurePerformanceParameters (parameters, liveValues);
        ensureGraphMacroParameters (parameters, liveValues, dspGraph);

        presets.clear();
        if (auto* pd = root->getProperty ("presetData").getDynamicObject())
            if (auto* arr = pd->getProperty ("presets").getArray())
                for (auto& item : *arr)
                    presets.push_back (Preset::fromVar (item));

        patches.clear();
        if (auto* pd = root->getProperty ("patchData").getDynamicObject())
            if (auto* arr = pd->getProperty ("patches").getArray())
                for (auto& item : *arr)
                    patches.push_back (InstrumentPatch::fromVar (item));

        sectionPresets.clear();
        if (auto* spd = root->getProperty ("sectionPresetData").getDynamicObject())
            if (auto* arr = spd->getProperty ("sectionPresets").getArray())
                for (auto& item : *arr)
                    sectionPresets.push_back (SectionPreset::fromVar (item));

        expansions.clear();
        if (auto* ed = root->getProperty ("expansionData").getDynamicObject())
            if (auto* arr = ed->getProperty ("expansions").getArray())
                for (auto& item : *arr)
                    expansions.push_back (ExpansionMetadata::fromVar (item));

        midiMappings.clear();
        if (auto* md = root->getProperty ("midiMappings").getDynamicObject())
            if (auto* arr = md->getProperty ("mappings").getArray())
                for (auto& item : *arr)
                    midiMappings.push_back (MidiMapping::fromVar (item));

        projectFolder = folder;
        resetLiveValuesToDefaults();

        const InstrumentPatch* patchToApply = nullptr;
        for (const auto& patch : patches)
            if (patch.isDefault)
            {
                patchToApply = &patch;
                break;
            }
        if (patchToApply == nullptr)
        {
            for (const auto& preset : presets)
            {
                if (! preset.isDefault && preset.name != manifest.defaultPreset)
                    continue;
                for (const auto& patch : patches)
                    if (patch.id == preset.patchId || patch.name == preset.name)
                    {
                        patchToApply = &patch;
                        break;
                    }
                if (patchToApply != nullptr)
                    break;
            }
        }
        if (patchToApply == nullptr && ! patches.empty())
            patchToApply = &patches.front();

        if (patchToApply != nullptr)
            applyPatch (*patchToApply);
        else
        {
            for (const auto& preset : presets)
            {
                if (! preset.isDefault && preset.name != manifest.defaultPreset)
                    continue;
                for (const auto& value : preset.values)
                    liveValues.setValue (value.first, value.second);
                break;
            }
        }

        if (patches.empty())
        {
            auto patch = captureCurrentPatch (manifest.defaultPreset.isNotEmpty()
                ? manifest.defaultPreset : manifest.instrumentName + " Patch");
            patch.isDefault = true;
            patches.push_back (std::move (patch));
        }

        auto scriptFile = folder.getChildFile ("pscript.txt");
        if (scriptFile.existsAsFile())
        {
            pscriptSource = scriptFile.loadFileAsString();
            scriptEngine.compile (pscriptSource);
        }
        else
        {
            pscriptSource = "";
            scriptEngine.compile ("");
        }
        markClean();
        return true;
    }

} // namespace patchcraft
