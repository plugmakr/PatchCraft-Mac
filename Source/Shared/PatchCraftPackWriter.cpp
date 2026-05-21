#include "PatchCraftPackWriter.h"
#include "PatchCraftProject.h"
#include "AssetManager.h"
#include "LicenseValidator.h"

#include <algorithm>
#include <map>

namespace patchcraft
{
    namespace
    {
        static bool tryAddRegistryParameter (ParameterModel& parameters,
                                             const juce::String& parameterId,
                                             const juce::String& engineId)
        {
            if (parameterId.isEmpty() || parameters.contains (parameterId))
                return true;

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

        static bool graphHasBlock (const DspGraph& graph, const juce::String& id)
        {
            for (const auto& block : graph.blocks)
                if (block.id == id)
                    return true;
            return false;
        }

        static void ensureMacroBlockParameters (ParameterModel& parameters,
                                                const DspGraph& graph)
        {
            for (const auto& block : graph.blocks)
            {
                if (! block.type.containsIgnoreCase ("macro") || parameters.contains (block.id))
                    continue;

                ParameterDef def;
                def.id = block.id;
                def.name = block.name.isNotEmpty() ? block.name : block.id;
                def.min = 0.0f;
                def.max = 1.0f;
                def.defaultValue = 0.5f;
                def.category = "Macro";
                def.section = "mod";
                def.displayMode = "continuous";
                def.hostAutomatable = true;
                def.midiLearnable = true;
                def.modulatable = true;
                parameters.add (def);
            }
        }

        static juce::String inferredRuntimeParameterForElement (const LayoutElement& element)
        {
            const auto id = element.id.toLowerCase();
            const auto label = element.label.toLowerCase();

            if (id == "modwheel" || id == "mod_wheel" || id == "mod-wheel"
                || label == "mod" || label.contains ("mod wheel"))
                return "modWheel";

            if (id == "expression" || label == "expr" || label.contains ("expression"))
                return "expression";

            if (id == "pitchwheel" || id == "pitch_wheel" || id == "pitch-wheel"
                || label.contains ("pitch wheel"))
                return "pitchWheel";

            if (id == "sustain" || id == "sustainpedal" || label.contains ("sustain"))
                return "sustainPedal";

            return {};
        }

        static void bindObviousRuntimeControls (LayoutModel& layout)
        {
            for (auto& element : layout.getAll())
            {
                if (! isRuntimeControlElement (element.type) || element.parameterId.isNotEmpty())
                    continue;

                const auto inferred = inferredRuntimeParameterForElement (element);
                if (inferred.isNotEmpty())
                    element.parameterId = inferred;
            }
        }

        static void ensureReferencedParameters (ParameterModel& parameters,
                                                const LayoutModel& layout,
                                                const DspGraph& graph,
                                                const std::vector<Preset>& presets,
                                                const juce::String& engineId)
        {
            ensureMacroBlockParameters (parameters, graph);

            for (const auto& element : layout.getAll())
                tryAddRegistryParameter (parameters, element.parameterId, engineId);

            for (const auto& block : graph.blocks)
                tryAddRegistryParameter (parameters, block.targetId, engineId);

            for (const auto& macro : graph.macros)
            {
                tryAddRegistryParameter (parameters, macro.macroId, engineId);
                tryAddRegistryParameter (parameters, macro.targetId, engineId);
            }

            for (const auto& route : graph.modulation)
            {
                if (! graphHasBlock (graph, route.sourceId))
                    tryAddRegistryParameter (parameters, route.sourceId, engineId);
                tryAddRegistryParameter (parameters, route.targetId, engineId);
            }

            for (const auto& lane : graph.automation)
                tryAddRegistryParameter (parameters, lane.targetId, engineId);

            for (const auto& preset : presets)
                for (const auto& value : preset.values)
                    tryAddRegistryParameter (parameters, value.first, engineId);
        }

        static int prunePresetValuesMissingParameters (std::vector<Preset>& presets,
                                                       const ParameterModel& parameters)
        {
            int removed = 0;
            for (auto& preset : presets)
            {
                for (auto it = preset.values.begin(); it != preset.values.end();)
                {
                    if (parameters.find (it->first) == nullptr)
                    {
                        it = preset.values.erase (it);
                        ++removed;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            return removed;
        }

        static void pruneInvalidGraphReferences (DspGraph& graph,
                                                 ParameterModel& parameters,
                                                 const juce::String& engineId)
        {
            ensureMacroBlockParameters (parameters, graph);

            for (auto& block : graph.blocks)
                if (block.targetId.isNotEmpty()
                    && ! tryAddRegistryParameter (parameters, block.targetId, engineId)
                    && parameters.find (block.targetId) == nullptr)
                    block.targetId.clear();

            graph.macros.erase (std::remove_if (graph.macros.begin(), graph.macros.end(),
                [&] (const MacroAssignment& macro)
                {
                    return (macro.macroId.isNotEmpty()
                            && ! tryAddRegistryParameter (parameters, macro.macroId, engineId)
                            && parameters.find (macro.macroId) == nullptr)
                        || (macro.targetId.isNotEmpty()
                            && ! tryAddRegistryParameter (parameters, macro.targetId, engineId)
                            && parameters.find (macro.targetId) == nullptr);
                }),
                graph.macros.end());

            graph.modulation.erase (std::remove_if (graph.modulation.begin(), graph.modulation.end(),
                [&] (const ModRoute& route)
                {
                    const bool sourceOk = route.sourceId.isEmpty()
                        || graphHasBlock (graph, route.sourceId)
                        || tryAddRegistryParameter (parameters, route.sourceId, engineId)
                        || parameters.find (route.sourceId) != nullptr;

                    const bool targetOk = route.targetId.isNotEmpty()
                        && (tryAddRegistryParameter (parameters, route.targetId, engineId)
                            || parameters.find (route.targetId) != nullptr);

                    return ! sourceOk || ! targetOk;
                }),
                graph.modulation.end());

            graph.automation.erase (std::remove_if (graph.automation.begin(), graph.automation.end(),
                [&] (const AutomationLane& lane)
                {
                    return lane.targetId.isEmpty()
                        || (! tryAddRegistryParameter (parameters, lane.targetId, engineId)
                            && parameters.find (lane.targetId) == nullptr);
                }),
                graph.automation.end());

            auto nodes = graph.buildTypedNodes (engineId);
            std::map<juce::String, DspNodeKind> nodeKinds;
            for (const auto& node : nodes)
                nodeKinds[node.id] = node.kind;

            graph.edges.erase (std::remove_if (graph.edges.begin(), graph.edges.end(),
                [&] (const DspGraphEdge& edge)
                {
                    if (! edge.enabled)
                        return false;

                    const auto source = nodeKinds.find (edge.sourceNodeId);
                    const auto target = nodeKinds.find (edge.targetNodeId);
                    if (edge.sourceNodeId.isEmpty() || edge.targetNodeId.isEmpty()
                        || edge.sourceNodeId == edge.targetNodeId
                        || source == nodeKinds.end()
                        || target == nodeKinds.end())
                        return true;

                    return edge.signalType == DspSignalType::audio
                        && (source->second == DspNodeKind::modulation
                            || target->second == DspNodeKind::source);
                }),
                graph.edges.end());

            bool routingStillInvalid = false;
            for (const auto& issue : graph.validateTypedGraph (engineId))
            {
                if (issue.severity != "error")
                    continue;

                if (issue.message.containsIgnoreCase ("edge")
                    || issue.message.containsIgnoreCase ("reachable"))
                {
                    routingStillInvalid = true;
                    break;
                }
            }

            if (routingStillInvalid)
                graph.edges.clear();
        }
    }

    static bool writeJson (const juce::File& f, const juce::var& v, juce::String& error)
    {
        auto json = juce::JSON::toString (v, true);
        if (! f.replaceWithText (json))
        {
            error = "Failed to write " + f.getFileName();
            return false;
        }
        return true;
    }

    bool PatchCraftPackWriter::write (const PatchCraftProject& project,
                                      const juce::File& packFolder,
                                      juce::String& error)
    {
        if (! packFolder.exists())
        {
            auto res = packFolder.createDirectory();
            if (res.failed())
            {
                error = "Could not create pack folder: " + res.getErrorMessage();
                return false;
            }
        }
        else if (! packFolder.isDirectory())
        {
            error = "Target is not a directory.";
            return false;
        }

        // Sub-folders
        auto assets   = packFolder.getChildFile ("assets");
        auto samples  = packFolder.getChildFile ("samples");
        assets.createDirectory();
        samples.createDirectory();
        assets.getChildFile ("knobs").createDirectory();
        assets.getChildFile ("sliders").createDirectory();
        assets.getChildFile ("meters").createDirectory();
        assets.getChildFile ("images").createDirectory();

        auto manifestForPack = project.getManifest();
        if (manifestForPack.licenseProductId.isEmpty()
            && (manifestForPack.licenseRequired || manifestForPack.licenseServerUrl.isNotEmpty()))
            manifestForPack.licenseProductId = LicenseValidator::hashInstrumentId (manifestForPack.instrumentName,
                                                                                   manifestForPack.creator);
        auto layoutForPack = project.getLayout();
        auto parametersForPack = project.getParameters();
        parametersForPack.ensureRegistryMetadata (manifestForPack.engine);
        auto dspGraphForPack = project.getDspGraph();
        auto backgroundForPack = project.backgroundImageRelative.isNotEmpty()
            ? project.backgroundImageRelative : manifestForPack.backgroundImage;
        auto exportPatches = project.getPatches();
        if (exportPatches.empty())
            exportPatches.push_back (project.captureCurrentPatch (
                manifestForPack.defaultPreset.isNotEmpty()
                    ? manifestForPack.defaultPreset
                    : manifestForPack.instrumentName + " Patch"));
        auto exportSectionPresets = project.getSectionPresets();
        auto exportExpansions = project.getExpansions();
        auto exportPresets = project.getPresets();

        auto ensureExportExpansion = [&] (const juce::String& idOrName) -> ExpansionMetadata&
        {
            const auto wanted = idOrName.isNotEmpty() ? idOrName : manifestForPack.instrumentName + " Core";
            for (auto& expansion : exportExpansions)
                if (expansion.id == wanted || expansion.name == wanted)
                    return expansion;

            ExpansionMetadata metadata;
            metadata.id = wanted.toLowerCase().replaceCharacters (" \\/:*?\"<>|", "__________");
            metadata.name = wanted;
            metadata.description = manifestForPack.description;
            metadata.author = manifestForPack.creator;
            metadata.brand = manifestForPack.playerDisplayName.isNotEmpty()
                ? manifestForPack.playerDisplayName : manifestForPack.creator;
            metadata.category = manifestForPack.category;
            metadata.version = manifestForPack.version;
            metadata.tags = manifestForPack.tags;
            exportExpansions.push_back (std::move (metadata));
            return exportExpansions.back();
        };

        auto& defaultExpansion = ensureExportExpansion (packFolder.getParentDirectory().getFileName().isNotEmpty()
            ? packFolder.getParentDirectory().getFileName()
            : manifestForPack.instrumentName + " Core");
        for (auto& patch : exportPatches)
        {
            if (patch.id.isEmpty())
                patch.id = patch.name.toLowerCase().replaceCharacters (" \\/:*?\"<>|", "__________");
            if (patch.packId.isEmpty())
                patch.packId = manifestForPack.instrumentName;
            if (patch.expansionId.isEmpty())
                patch.expansionId = defaultExpansion.id;
            defaultExpansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
            for (const auto& asset : patch.includedAssets)
                defaultExpansion.includedAssets.addIfNotAlreadyThere (asset);

            bool hasPreset = false;
            for (const auto& preset : exportPresets)
                if (preset.patchId == patch.id || preset.name == patch.name)
                    hasPreset = true;
            if (! hasPreset)
                exportPresets.push_back (patch.toPreset());
        }
        for (auto& preset : exportPresets)
        {
            if (preset.packId.isEmpty())
                preset.packId = manifestForPack.instrumentName;
            if (preset.expansionId.isEmpty())
                preset.expansionId = defaultExpansion.id;
            defaultExpansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
            for (const auto& ref : preset.libraryReferences)
                defaultExpansion.includedAssets.addIfNotAlreadyThere (ref);
        }
        for (auto& preset : exportSectionPresets)
        {
            if (preset.packId.isEmpty())
                preset.packId = manifestForPack.instrumentName;
            if (preset.expansionId.isEmpty())
                preset.expansionId = defaultExpansion.id;
            defaultExpansion.includedSectionPresetIds.addIfNotAlreadyThere (preset.id);
            for (const auto& ref : preset.libraryReferences)
                defaultExpansion.includedAssets.addIfNotAlreadyThere (ref);
        }

        auto copyAssetToPack = [&] (juce::String& path, const juce::String& subFolder,
                                    const juce::String& fallbackName) -> bool
        {
            if (path.isEmpty())
                return true;

            juce::File src = juce::File::isAbsolutePath (path)
                ? juce::File (path)
                : project.getProjectFolder().isDirectory()
                    ? project.getProjectFolder().getChildFile (path)
                    : juce::File (path);

            if (! src.existsAsFile())
            {
                error = "Missing asset during pack export: " + path;
                return false;
            }

            auto dstDir = packFolder.getChildFile (subFolder);
            dstDir.createDirectory();
            auto dstName = fallbackName.isNotEmpty() ? fallbackName : src.getFileName();
            auto dst = dstDir.getChildFile (dstName);
            if (src != dst && ! src.copyFileTo (dst))
                return false;
            path = dst.getRelativePathFrom (packFolder).replaceCharacter ('\\', '/');
            return true;
        };

        // Resolve the background source. If it's missing on disk (common for
        // projects that haven't been saved yet, or templates that point at
        // assets/background.png without ever importing one), synthesise a
        // default background so the export still succeeds — mirrors how the
        // library thumbnail handles a missing thumbnail.png.
        auto resolveProjectAsset = [&] (const juce::String& path) -> juce::File
        {
            if (path.isEmpty()) return {};
            if (juce::File::isAbsolutePath (path)) return juce::File (path);
            if (project.getProjectFolder().isDirectory())
                return project.getProjectFolder().getChildFile (path);
            return juce::File (path);
        };

        const auto bgSource = resolveProjectAsset (backgroundForPack);
        if (backgroundForPack.isEmpty() || ! bgSource.existsAsFile())
        {
            auto bgDst = packFolder.getChildFile ("assets/background.png");
            bgDst.getParentDirectory().createDirectory();
            auto img = AssetManager::renderDefaultHeroImage (1280, 800);
            juce::PNGImageFormat png;
            std::unique_ptr<juce::FileOutputStream> out (bgDst.createOutputStream());
            if (out == nullptr || ! png.writeImageToStream (img, *out))
            {
                error = "Failed to generate default background artwork.";
                return false;
            }
            backgroundForPack = "assets/background.png";
        }
        else if (! copyAssetToPack (backgroundForPack, "assets",
                                     "background" + juce::File (backgroundForPack).getFileExtension()))
        {
            if (error.isEmpty())
                error = "Failed to copy background artwork into exported pack.";
            return false;
        }
        manifestForPack.backgroundImage = backgroundForPack;

        if (manifestForPack.libraryThumbnail.isNotEmpty())
        {
            auto thumbnailPath = manifestForPack.libraryThumbnail;
            auto thumbnailSource = juce::File::isAbsolutePath (thumbnailPath)
                ? juce::File (thumbnailPath)
                : project.getProjectFolder().isDirectory()
                    ? project.getProjectFolder().getChildFile (thumbnailPath)
                    : juce::File (thumbnailPath);

            if (! thumbnailSource.existsAsFile()
                && juce::File (thumbnailPath).getFileName().equalsIgnoreCase ("thumbnail.png"))
            {
                auto thumbDst = packFolder.getChildFile ("assets/thumbnail.png");
                thumbDst.getParentDirectory().createDirectory();
                auto image = AssetManager::renderDefaultHeroImage (640, 360);
                juce::PNGImageFormat png;
                std::unique_ptr<juce::FileOutputStream> out (thumbDst.createOutputStream());
                if (out == nullptr || ! png.writeImageToStream (image, *out))
                {
                    error = "Failed to generate default library thumbnail.";
                    return false;
                }
                manifestForPack.libraryThumbnail = "assets/thumbnail.png";
            }
            else if (! copyAssetToPack (thumbnailPath, "assets", "thumbnail" + juce::File (thumbnailPath).getFileExtension()))
            {
                error = "Failed to copy library thumbnail into exported pack.";
                return false;
            }
            else
            {
                manifestForPack.libraryThumbnail = thumbnailPath;
            }
        }

        if (manifestForPack.playerLogoImage.isNotEmpty())
        {
            auto logoPath = manifestForPack.playerLogoImage;
            if (! copyAssetToPack (logoPath, "assets/images", {}))
            {
                error = "Failed to copy Player logo artwork into exported pack.";
                return false;
            }
            manifestForPack.playerLogoImage = logoPath;
        }

        for (auto& expansion : exportExpansions)
        {
            if (expansion.artworkPath.isNotEmpty())
            {
                auto artworkPath = expansion.artworkPath;
                if (! copyAssetToPack (artworkPath, "assets/expansions", {}))
                {
                    error = "Failed to copy expansion artwork: " + expansion.artworkPath;
                    return false;
                }
                expansion.artworkPath = artworkPath;
            }
            if (expansion.licensePath.isNotEmpty())
            {
                auto licensePath = expansion.licensePath;
                if (! copyAssetToPack (licensePath, "assets/licenses", {}))
                {
                    error = "Failed to copy expansion license file: " + expansion.licensePath;
                    return false;
                }
                expansion.licensePath = licensePath;
            }
        }

        for (auto& e : layoutForPack.getAll())
        {
            if (e.type == ElementType::Image)
            {
                // Image elements that point at a missing asset (typical of a
                // freshly-created or never-saved project, or a template that
                // references assets/hero.png without ever importing one) used
                // to abort the entire export. Drop the asset reference instead
                // - the Player renders an empty image gracefully and the user
                // can re-import later. Background art is handled separately
                // above with a synthesised default.
                if (e.asset.isNotEmpty() && ! copyAssetToPack (e.asset, "assets/images", {}))
                {
                    error.clear();
                    e.asset.clear();
                }
            }

            if (e.filmstripAsset.isNotEmpty())
            {
                auto sub = e.type == ElementType::Slider ? "assets/sliders"
                         : e.type == ElementType::Meter  ? "assets/meters"
                                                        : "assets/knobs";
                if (! copyAssetToPack (e.filmstripAsset, sub, {}))
                {
                    // Same resilience as for image elements: clear the missing
                    // filmstrip rather than blocking the export.
                    error.clear();
                    e.filmstripAsset.clear();
                }
            }
        }

        bindObviousRuntimeControls (layoutForPack);
        ensureReferencedParameters (parametersForPack, layoutForPack, dspGraphForPack,
                                    exportPresets, manifestForPack.engine);
        pruneInvalidGraphReferences (dspGraphForPack, parametersForPack, manifestForPack.engine);
        for (auto& patch : exportPatches)
        {
            const auto patchEngine = patch.engine.isNotEmpty() ? patch.engine : manifestForPack.engine;
            ensureReferencedParameters (parametersForPack, layoutForPack, patch.dspGraph,
                                        exportPresets, patchEngine);
            pruneInvalidGraphReferences (patch.dspGraph, parametersForPack, patchEngine);
        }
        prunePresetValuesMissingParameters (exportPresets, parametersForPack);

        const auto validationIssues = parametersForPack.validateReferences (
            layoutForPack.getAll(), dspGraphForPack, exportPresets);
        juce::StringArray exportErrors;
        for (const auto& issue : validationIssues)
            if (issue.severity == "error")
                exportErrors.add (issue.toString());
        for (const auto& issue : dspGraphForPack.validateTypedGraph (manifestForPack.engine))
            if (issue.severity == "error")
                exportErrors.add (issue.toString());

        auto projectFileExists = [&] (const juce::String& path)
        {
            if (path.isEmpty())
                return true;
            const auto file = juce::File::isAbsolutePath (path)
                ? juce::File (path)
                : project.getProjectFolder().isDirectory()
                    ? project.getProjectFolder().getChildFile (path)
                    : juce::File (path);
            if (file.existsAsFile())
                return true;
            return packFolder.getChildFile (path).existsAsFile();
        };
        auto patchExists = [&] (const juce::String& id)
        {
            for (const auto& patch : exportPatches)
                if (patch.id == id)
                    return true;
            return false;
        };
        auto presetExists = [&] (const juce::String& name)
        {
            for (const auto& preset : exportPresets)
                if (preset.name == name)
                    return true;
            return false;
        };
        auto sectionPresetExists = [&] (const juce::String& id)
        {
            for (const auto& preset : exportSectionPresets)
                if (preset.id == id)
                    return true;
            return false;
        };

        for (const auto& patch : exportPatches)
        {
            if (patch.id.isEmpty() || patch.name.isEmpty())
                exportErrors.add ("ERROR: patch - Playable patches require both id and name.");
            if (patch.dspGraph.blocks.empty())
                exportErrors.add ("ERROR: " + patch.name + " - Playable patch has no DSP graph blocks.");
            for (const auto& asset : patch.includedAssets)
                if (! projectFileExists (asset))
                    exportErrors.add ("ERROR: " + patch.name + " - Patch references missing asset: " + asset);
            for (const auto& zone : patch.sampleZones)
                if (! projectFileExists (zone.samplePath))
                    exportErrors.add ("ERROR: " + patch.name + " - Patch references missing sample: " + zone.samplePath);
        }

        for (const auto& preset : exportPresets)
        {
            if (preset.name.isEmpty())
                exportErrors.add ("ERROR: preset - Preset name is required.");
            if (preset.patchId.isNotEmpty() && ! patchExists (preset.patchId))
                exportErrors.add ("ERROR: " + preset.name + " - Preset references missing patch id: " + preset.patchId);
            for (const auto& ref : preset.libraryReferences)
                if (! projectFileExists (ref))
                    exportErrors.add ("ERROR: " + preset.name + " - Preset references missing library asset: " + ref);
        }

        for (const auto& preset : exportSectionPresets)
        {
            if (preset.id.isEmpty() || preset.section.isEmpty())
                exportErrors.add ("ERROR: sectionPreset - Section presets require id and section.");
            for (const auto& ref : preset.libraryReferences)
                if (! projectFileExists (ref))
                    exportErrors.add ("ERROR: " + preset.name + " - Section preset references missing library asset: " + ref);
        }

        for (const auto& expansion : exportExpansions)
        {
            if (expansion.id.isEmpty() || expansion.name.isEmpty())
                exportErrors.add ("ERROR: expansion - Expansion metadata requires id and name.");
            if (expansion.artworkPath.isNotEmpty() && ! projectFileExists (expansion.artworkPath))
                exportErrors.add ("ERROR: " + expansion.name + " - Expansion artwork is missing: " + expansion.artworkPath);
            if (expansion.licensePath.isNotEmpty() && ! projectFileExists (expansion.licensePath))
                exportErrors.add ("ERROR: " + expansion.name + " - Expansion license file is missing: " + expansion.licensePath);
            for (const auto& id : expansion.includedPatchIds)
                if (! patchExists (id))
                    exportErrors.add ("ERROR: " + expansion.name + " - Expansion references missing patch id: " + id);
            for (const auto& name : expansion.includedPresetNames)
                if (! presetExists (name))
                    exportErrors.add ("ERROR: " + expansion.name + " - Expansion references missing preset: " + name);
            for (const auto& id : expansion.includedSectionPresetIds)
                if (! sectionPresetExists (id))
                    exportErrors.add ("ERROR: " + expansion.name + " - Expansion references missing section preset id: " + id);
            for (const auto& asset : expansion.includedAssets)
                if (! projectFileExists (asset))
                    exportErrors.add ("ERROR: " + expansion.name + " - Expansion references missing asset: " + asset);
        }

        const auto hostSlots = parametersForPack.buildHostParameterSlots();

        for (const auto& element : layoutForPack.getAll())
        {
            if (! isPlayerRuntimeElementSupported (element.type))
                exportErrors.add ("ERROR: " + element.id + " - Player runtime does not support "
                    + elementTypeDisplayName (element.type) + " elements yet.");

            if (isRuntimeControlElement (element.type))
            {
                if (element.type == ElementType::Dropdown && element.id == "presets")
                    continue;
                if (element.action.isNotEmpty())
                    continue;

                if (element.parameterId.isEmpty())
                {
                    exportErrors.add ("ERROR: " + element.id + " - Runtime control is not mapped to a parameter.");
                    continue;
                }

                const auto* def = parametersForPack.find (element.parameterId);
                if (def == nullptr)
                    exportErrors.add ("ERROR: " + element.id + " - Runtime control maps to missing parameter '"
                        + element.parameterId + "'.");
            }
        }

        if (! exportErrors.isEmpty())
        {
            error = "Pack export blocked by invalid project references:\n"
                  + exportErrors.joinIntoString ("\n");
            return false;
        }

        if (project.getManifest().engine.equalsIgnoreCase ("sample")
            && project.getSampleMap().getZones().empty())
        {
            error = "Pack export blocked: sample instruments need at least one mapped sample zone.";
            return false;
        }

        juce::StringArray usedSampleNames;
        std::map<juce::String, juce::String> samplePathRewrites;

        auto safeFileStem = [] (juce::String text)
        {
            text = text.trim();
            if (text.isEmpty())
                text = "layer";
            text = text.replaceCharacters (" \\/:*?\"<>|#.", "____________");
            while (text.contains ("__"))
                text = text.replace ("__", "_");
            return text.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
        };

        auto resolveSampleFile = [&] (const juce::String& path, const juce::File& mappingBase)
        {
            if (path.isEmpty())
                return juce::File();
            if (juce::File::isAbsolutePath (path))
                return juce::File (path);
            if (mappingBase.isDirectory())
            {
                const auto fromMapping = mappingBase.getChildFile (path);
                if (fromMapping.existsAsFile())
                    return fromMapping;
            }
            if (project.getProjectFolder().isDirectory())
            {
                const auto fromProject = project.getProjectFolder().getChildFile (path);
                if (fromProject.existsAsFile())
                    return fromProject;
            }
            return mappingBase.isDirectory() ? mappingBase.getChildFile (path) : juce::File (path);
        };

        auto copyZoneSampleToPack = [&] (SampleZoneDef& z,
                                         const juce::File& mappingBase,
                                         const juce::String& filePrefix) -> bool
        {
            const auto originalSamplePath = z.samplePath;
            if (z.lowNote < 0 || z.highNote > 127 || z.lowNote > z.highNote
                || z.lowVelocity < 1 || z.highVelocity > 127 || z.lowVelocity > z.highVelocity)
            {
                error = "Pack export blocked by invalid sample zone range: " + z.samplePath;
                return false;
            }

            auto src = resolveSampleFile (z.samplePath, mappingBase);
            if (! src.existsAsFile())
            {
                error = "Missing sample during pack export: " + z.samplePath;
                return false;
            }

            const auto prefix = safeFileStem (filePrefix);
            const auto base = src.getFileNameWithoutExtension();
            const auto ext = src.getFileExtension();
            auto dstName = (filePrefix.isNotEmpty() ? prefix + "_" : juce::String()) + src.getFileName();
            int suffix = 2;
            while (usedSampleNames.contains (dstName, true))
                dstName = (filePrefix.isNotEmpty() ? prefix + "_" : juce::String())
                        + base + "_" + juce::String (suffix++) + ext;
            usedSampleNames.add (dstName);

            auto dst = samples.getChildFile (dstName);
            if (src != dst)
            {
                if (dst.existsAsFile() && ! dst.deleteFile())
                {
                    error = "Could not replace exported sample: " + dst.getFullPathName();
                    return false;
                }
                if (! src.copyFileTo (dst))
                {
                    error = "Failed to copy sample during pack export: " + src.getFullPathName();
                    return false;
                }
            }

            z.samplePath = "samples/" + dst.getFileName();
            samplePathRewrites[originalSamplePath] = z.samplePath;
            return true;
        };

        auto resolveProjectJson = [&] (const juce::String& path)
        {
            if (path.isEmpty())
                return juce::File();
            if (juce::File::isAbsolutePath (path))
                return juce::File (path);
            if (project.getProjectFolder().isDirectory())
                return project.getProjectFolder().getChildFile (path);
            return juce::File (path);
        };

        auto exportMultiInstrumentLayers = [&]() -> bool
        {
            const bool wantsMulti = manifestForPack.multiInstrumentMode
                || manifestForPack.engine.equalsIgnoreCase ("multi");
            if (! wantsMulti)
                return true;

            auto instruments = packFolder.getChildFile ("instruments");
            if (! instruments.createDirectory())
            {
                error = "Could not create multi-instrument folder: " + instruments.getFullPathName();
                return false;
            }

            const int layerCount = juce::jmax (manifestForPack.instrumentIds.size(),
                                    juce::jmax (manifestForPack.instrumentFiles.size(),
                                    juce::jmax (manifestForPack.instrumentNames.size(),
                                               project.getSampleMap().getZones().empty() ? 0 : 1)));
            if (layerCount <= 0)
            {
                error = "Pack export blocked: multi-instrument packs need at least one layer mapping.";
                return false;
            }

            juce::StringArray nextIds, nextNames, nextFiles;
            for (int i = 0; i < layerCount; ++i)
            {
                auto layerId = i < manifestForPack.instrumentIds.size()
                    ? manifestForPack.instrumentIds[i].trim()
                    : "layer_" + juce::String (i + 1);
                if (layerId.isEmpty())
                    layerId = "layer_" + juce::String (i + 1);
                layerId = safeFileStem (layerId);

                auto layerName = i < manifestForPack.instrumentNames.size()
                    ? manifestForPack.instrumentNames[i].trim()
                    : "Layer " + juce::String (i + 1);
                if (layerName.isEmpty())
                    layerName = "Layer " + juce::String (i + 1);

                const auto fileRef = i < manifestForPack.instrumentFiles.size()
                    ? manifestForPack.instrumentFiles[i].trim()
                    : juce::String();
                const auto sourceMapFile = resolveProjectJson (fileRef);

                SampleMap layerMap;
                juce::File mappingBase = project.getProjectFolder();
                if (sourceMapFile.existsAsFile())
                {
                    auto mapVar = juce::JSON::parse (sourceMapFile);
                    if (! mapVar.isObject())
                    {
                        error = "Multi-instrument layer map is not valid JSON: " + sourceMapFile.getFullPathName();
                        return false;
                    }
                    layerMap.fromVar (mapVar);
                    mappingBase = sourceMapFile.getParentDirectory();
                }
                else if (fileRef.isNotEmpty())
                {
                    error = "Missing multi-instrument layer map: " + fileRef;
                    return false;
                }
                else if (i == 0 && ! project.getSampleMap().getZones().empty())
                {
                    layerMap = project.getSampleMap();
                }
                else
                {
                    error = "Multi-instrument layer " + layerName + " has no sample map.";
                    return false;
                }

                if (layerMap.getZones().empty())
                {
                    error = "Multi-instrument layer " + layerName + " has no mapped sample zones.";
                    return false;
                }

                SampleMap copiedLayerMap;
                for (auto zone : layerMap.getZones())
                {
                    if (! copyZoneSampleToPack (zone, mappingBase, layerId))
                        return false;
                    copiedLayerMap.add (zone);
                }

                const auto layerFile = instruments.getChildFile (layerId + ".json");
                if (! writeJson (layerFile, copiedLayerMap.toVar(), error))
                    return false;

                nextIds.add (layerId);
                nextNames.add (layerName);
                nextFiles.add (layerFile.getRelativePathFrom (packFolder).replaceCharacter ('\\', '/'));
            }

            manifestForPack.engine = "multi";
            manifestForPack.multiInstrumentMode = true;
            manifestForPack.instrumentIds = nextIds;
            manifestForPack.instrumentNames = nextNames;
            manifestForPack.instrumentFiles = nextFiles;
            return true;
        };

        if (! exportMultiInstrumentLayers())
            return false;

        // -- Manifest ----------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("manifest.json"),
                         manifestForPack.toVar(), error))
            return false;

        if (manifestForPack.licenseRequired
            || manifestForPack.licenseServerUrl.isNotEmpty()
            || manifestForPack.licenseProductId.isNotEmpty())
        {
            LicenseValidator::LicenseInfo licenseInfo;
            licenseInfo.licenseKey = manifestForPack.licenseKey;
            licenseInfo.instrumentName = manifestForPack.instrumentName;
            licenseInfo.creator = manifestForPack.creator;
            licenseInfo.productId = manifestForPack.licenseProductId;
            licenseInfo.licenseServerUrl = manifestForPack.licenseServerUrl;
            licenseInfo.policy = manifestForPack.licensePolicy;
            licenseInfo.trialDays = manifestForPack.trialDays;
            licenseInfo.isTrial = manifestForPack.isTrial;
            licenseInfo.expiryDate = manifestForPack.trialExpiryDate;
            licenseInfo.offlineGraceDays = manifestForPack.licenseOfflineGraceDays;
            licenseInfo.bindToMachine = manifestForPack.licenseBindToMachine;

            auto* license = new juce::DynamicObject();
            license->setProperty ("schema", "patchcraft.license.v1");
            license->setProperty ("required", manifestForPack.licenseRequired);
            license->setProperty ("productId", manifestForPack.licenseProductId);
            license->setProperty ("serverUrl", manifestForPack.licenseServerUrl);
            license->setProperty ("publicKey", manifestForPack.licensePublicKey);
            license->setProperty ("policy", manifestForPack.licensePolicy);
            license->setProperty ("trialDays", manifestForPack.trialDays);
            license->setProperty ("offlineGraceDays", manifestForPack.licenseOfflineGraceDays);
            license->setProperty ("bindToMachine", manifestForPack.licenseBindToMachine);
            license->setProperty ("allowTrialConversion", manifestForPack.licenseAllowTrialConversion);
            license->setProperty ("activationTemplate", LicenseValidator::buildActivationRequest (licenseInfo));
            if (! writeJson (packFolder.getChildFile ("license.json"), juce::var (license), error))
                return false;
        }

        // -- Layout ------------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("layout.json"),
                         layoutForPack.toVar (project.getCanvasSize()), error))
            return false;

        // -- Parameters --------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("parameters.json"),
                         parametersForPack.toVar(), error))
            return false;

        juce::Array<juce::var> hostSlotArr;
        int overflowCount = 0;
        for (const auto& slot : hostSlots)
        {
            hostSlotArr.add (slot.toVar());
            if (slot.overflow)
                ++overflowCount;
        }
        auto* hostMapObj = new juce::DynamicObject();
        hostMapObj->setProperty ("strategy",
            "First 128 host-automatable registry parameters are exposed as fixed Player APVTS slots p0-p127; internal and overflow parameters remain Player-runtime controllable but are not host automation slots.");
        hostMapObj->setProperty ("maxSlots", kPatchCraftHostParameterSlots);
        hostMapObj->setProperty ("overflowCount", overflowCount);
        hostMapObj->setProperty ("slots", hostSlotArr);
        if (! writeJson (packFolder.getChildFile ("hostParameterMap.json"),
                         juce::var (hostMapObj), error))
            return false;

        // -- DSP graph ---------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("dspGraph.json"),
                         dspGraphForPack.toVar(), error))
            return false;

        // -- Mappings (after copying sample files) -----------------------------
        SampleMap copiedMap;
        for (auto z : project.getSampleMap().getZones())
        {
            if (! copyZoneSampleToPack (z, project.getProjectFolder(), {}))
                return false;
            copiedMap.add (z);
        }
        if (! writeJson (packFolder.getChildFile ("mappings.json"),
                         copiedMap.toVar(), error))
            return false;

        for (auto& patch : exportPatches)
        {
            for (auto& zone : patch.sampleZones)
                if (auto it = samplePathRewrites.find (zone.samplePath); it != samplePathRewrites.end())
                    zone.samplePath = it->second;
            for (auto& asset : patch.includedAssets)
                if (auto it = samplePathRewrites.find (asset); it != samplePathRewrites.end())
                    asset = it->second;
        }

        for (auto& expansion : exportExpansions)
            for (auto& asset : expansion.includedAssets)
                if (auto it = samplePathRewrites.find (asset); it != samplePathRewrites.end())
                    asset = it->second;

        juce::Array<juce::var> patchArr;
        for (const auto& patch : exportPatches)
            patchArr.add (patch.toVar());
        auto* patchesObj = new juce::DynamicObject();
        patchesObj->setProperty ("patches", patchArr);
        if (! writeJson (packFolder.getChildFile ("patches.json"),
                         juce::var (patchesObj), error))
            return false;

        juce::Array<juce::var> sectionPresetArr;
        for (const auto& preset : exportSectionPresets)
            sectionPresetArr.add (preset.toVar());
        auto* sectionPresetObj = new juce::DynamicObject();
        sectionPresetObj->setProperty ("sectionPresets", sectionPresetArr);
        if (! writeJson (packFolder.getChildFile ("sectionPresets.json"),
                         juce::var (sectionPresetObj), error))
            return false;

        juce::Array<juce::var> expansionArr;
        for (const auto& expansion : exportExpansions)
            expansionArr.add (expansion.toVar());
        auto* expansionObj = new juce::DynamicObject();
        expansionObj->setProperty ("expansions", expansionArr);
        if (! writeJson (packFolder.getChildFile ("expansions.json"),
                         juce::var (expansionObj), error))
            return false;

        // -- Presets -----------------------------------------------------------
        juce::Array<juce::var> presetArr;
        for (auto p : exportPresets)
        {
            if (p.packId.isEmpty())
                p.packId = manifestForPack.instrumentName;
            if (p.expansionId.isEmpty())
                p.expansionId = packFolder.getParentDirectory().getFileName();
            presetArr.add (p.toVar());
        }
        auto* presetsObj = new juce::DynamicObject();
        presetsObj->setProperty ("presets", presetArr);
        if (! writeJson (packFolder.getChildFile ("presets.json"),
                         juce::var (presetsObj), error))
            return false;

        // -- MIDI mappings -----------------------------------------------------
        juce::Array<juce::var> midiArr;
        for (const auto& mapping : project.getMidiMappings())
            midiArr.add (mapping.toVar());
        auto* midiObj = new juce::DynamicObject();
        midiObj->setProperty ("mappings", midiArr);
        if (! writeJson (packFolder.getChildFile ("midiMappings.json"),
                         juce::var (midiObj), error))
            return false;

        // -- Background image --------------------------------------------------
        if (project.getProjectFolder().isDirectory())
        {
            auto srcBg = project.getProjectFolder()
                            .getChildFile (project.backgroundImageRelative);
            if (srcBg.existsAsFile())
            {
                auto dstBg = packFolder.getChildFile (project.backgroundImageRelative);
                dstBg.getParentDirectory().createDirectory();
                srcBg.copyFileTo (dstBg);
            }

            // Copy any other knob/slider/meter assets if present
            for (auto sub : { "assets/knobs", "assets/sliders",
                              "assets/meters", "assets/images" })
            {
                auto srcDir = project.getProjectFolder().getChildFile (sub);
                if (srcDir.isDirectory())
                {
                    auto files = srcDir.findChildFiles (juce::File::findFiles, false);
                    for (auto& f : files)
                        f.copyFileTo (packFolder.getChildFile (sub).getChildFile (f.getFileName()));
                }
            }
        }

        // -- Generate default hero image if not present ------------------------
        auto heroDst = packFolder.getChildFile ("assets/hero.png");
        if (! heroDst.existsAsFile())
        {
            auto heroImg = AssetManager::renderDefaultHeroImage (1200, 360);
            juce::PNGImageFormat png;
            heroDst.getParentDirectory().createDirectory();
            std::unique_ptr<juce::FileOutputStream> out (heroDst.createOutputStream());
            if (out != nullptr)
                png.writeImageToStream (heroImg, *out);
        }

        return true;
    }

} // namespace patchcraft
