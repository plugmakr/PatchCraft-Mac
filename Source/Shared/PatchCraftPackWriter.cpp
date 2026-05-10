#include "PatchCraftPackWriter.h"
#include "PatchCraftProject.h"
#include "AssetManager.h"

#include <map>

namespace patchcraft
{
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
        auto layoutForPack = project.getLayout();
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
                if (! copyAssetToPack (e.asset, "assets/images", {}))
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
                if (! copyAssetToPack (e.filmstripAsset, sub, {}))
                {
                    error = "Failed to copy filmstrip asset: " + e.filmstripAsset;
                    return false;
                }
            }
        }

        const auto validationIssues = project.getParameters().validateReferences (
            layoutForPack.getAll(), project.getDspGraph(), exportPresets);
        juce::StringArray exportErrors;
        for (const auto& issue : validationIssues)
            if (issue.severity == "error")
                exportErrors.add (issue.toString());
        for (const auto& issue : project.getDspGraph().validateTypedGraph (project.getManifest().engine))
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

        const auto hostSlots = project.getParameters().buildHostParameterSlots();
        std::map<juce::String, int> parameterSlots;
        for (const auto& slot : hostSlots)
            if (! slot.overflow && slot.slotIndex >= 0)
                parameterSlots[slot.parameterId] = slot.slotIndex;

        for (const auto& element : layoutForPack.getAll())
        {
            if (! isPlayerRuntimeElementSupported (element.type))
                exportErrors.add ("ERROR: " + element.id + " - Player runtime does not support "
                    + elementTypeDisplayName (element.type) + " elements yet.");

            if (isRuntimeControlElement (element.type))
            {
                if (element.type == ElementType::Dropdown && element.id == "presets")
                    continue;

                if (element.parameterId.isEmpty())
                {
                    exportErrors.add ("ERROR: " + element.id + " - Runtime control is not mapped to a parameter.");
                    continue;
                }

                const auto* def = project.getParameters().find (element.parameterId);
                if (def == nullptr)
                    continue;

                if (! def->hostAutomatable)
                    exportErrors.add ("ERROR: " + element.id + " - Parameter '" + element.parameterId
                        + "' is internal and cannot be exposed as a Player host control.");
                else if (parameterSlots.find (element.parameterId) == parameterSlots.end())
                    exportErrors.add ("ERROR: " + element.id + " - Parameter '" + element.parameterId
                        + "' is outside the Player host parameter slot budget.");
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

        // -- Manifest ----------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("manifest.json"),
                         manifestForPack.toVar(), error))
            return false;

        // -- Layout ------------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("layout.json"),
                         layoutForPack.toVar (project.getCanvasSize()), error))
            return false;

        // -- Parameters --------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("parameters.json"),
                         project.getParameters().toVar(), error))
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
            "First 128 host-automatable registry parameters are exposed as fixed Player APVTS slots p0-p127; overflow parameters remain internal and cannot drive exported runtime UI controls.");
        hostMapObj->setProperty ("maxSlots", kPatchCraftHostParameterSlots);
        hostMapObj->setProperty ("overflowCount", overflowCount);
        hostMapObj->setProperty ("slots", hostSlotArr);
        if (! writeJson (packFolder.getChildFile ("hostParameterMap.json"),
                         juce::var (hostMapObj), error))
            return false;

        // -- DSP graph ---------------------------------------------------------
        if (! writeJson (packFolder.getChildFile ("dspGraph.json"),
                         project.getDspGraph().toVar(), error))
            return false;

        // -- Mappings (after copying sample files) -----------------------------
        SampleMap copiedMap;
        juce::StringArray usedSampleNames;
        std::map<juce::String, juce::String> samplePathRewrites;
        for (auto z : project.getSampleMap().getZones())
        {
            const auto originalSamplePath = z.samplePath;
            if (z.lowNote < 0 || z.highNote > 127 || z.lowNote > z.highNote
                || z.lowVelocity < 1 || z.highVelocity > 127 || z.lowVelocity > z.highVelocity)
            {
                error = "Pack export blocked by invalid sample zone range: " + z.samplePath;
                return false;
            }

            juce::File src;
            if (juce::File::isAbsolutePath (z.samplePath))
                src = juce::File (z.samplePath);
            else if (project.getProjectFolder().isDirectory())
                src = project.getProjectFolder().getChildFile (z.samplePath);

            if (! src.existsAsFile())
            {
                error = "Missing sample during pack export: " + z.samplePath;
                return false;
            }

            const auto base = src.getFileNameWithoutExtension();
            const auto ext = src.getFileExtension();
            auto dstName = src.getFileName();
            int suffix = 2;
            while (usedSampleNames.contains (dstName, true))
                dstName = base + "_" + juce::String (suffix++) + ext;
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
