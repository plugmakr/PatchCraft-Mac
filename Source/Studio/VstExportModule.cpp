#include "VstExportModule.h"
#include "PatchCraftPackWriter.h"

#include <array>
#include <cstring>

namespace patchcraft
{
    namespace
    {
        constexpr const char* kPlayerBundleName = "PatchCraft Player.vst3";
        constexpr const char* kPlayerFxBundleName = "PatchCraft Player FX.vst3";
        constexpr const char* kPropertiesAppName = "PatchCraft";
        constexpr const char* kPropertiesFileName = "VstExport";

        // -----------------------------------------------------------------
        // VST3 class-ID rewriting
        //
        // Every JUCE-built VST3 derives its three 16-byte class IIDs at
        // runtime from JucePlugin_ManufacturerCode and JucePlugin_PluginCode.
        // Because the export module stamps the SAME PatchCraft Player binary
        // for every export, every export would otherwise share those IDs and
        // DAWs would refuse to load more than one at a time.
        //
        // The two codes appear in the compiled binary as 32-bit immediates
        // (compiler inlines them into `mov` instructions before the
        // convertJucePluginId call). By replacing the four bytes for each
        // code with bytes derived from the project name we make every
        // exported plugin's class IDs unique. The moduleinfo.json file -
        // which the host's scan uses to discover plugins - is rewritten in
        // parallel so the on-disk and runtime IDs stay in sync.
        // -----------------------------------------------------------------
        struct CodeBytes
        {
            std::array<juce::uint8, 4> leBytes;     // bytes as stored in the DLL (little-endian uint32)
            juce::String               displayHex; // bytes as shown by moduleinfo.json (MSB first)
        };

        constexpr juce::uint32 kManufacturerCode = 0x50637266; // 'Pcrf'
        constexpr juce::uint32 kPluginCode       = 0x5063706C; // 'Pcpl'
        constexpr juce::uint32 kFxPluginCode     = 0x50636678; // 'Pcfx'

        CodeBytes codeBytesFromUint (juce::uint32 v)
        {
            CodeBytes out{};
            out.leBytes[0] = (juce::uint8) (v & 0xff);
            out.leBytes[1] = (juce::uint8) ((v >>  8) & 0xff);
            out.leBytes[2] = (juce::uint8) ((v >> 16) & 0xff);
            out.leBytes[3] = (juce::uint8) ((v >> 24) & 0xff);
            out.displayHex = juce::String::toHexString ((int) v).paddedLeft ('0', 8).toUpperCase();
            return out;
        }

        // Derive a (manufacturer, plugin) pair of 32-bit codes from a seed
        // string. We don't need a cryptographic hash here - class-ID
        // uniqueness only needs a low collision rate. 64-bit FNV-1a over
        // a "patchcraft:" + name namespace has a pairwise collision
        // probability of ~1 in 2^32 which is fine for plugin IDs.
        std::pair<CodeBytes, CodeBytes> deriveCodes (const juce::String& seed)
        {
            const auto material = juce::String ("patchcraft:") + seed;
            const auto utf8     = material.toUTF8();
            juce::uint64 hash   = 0xcbf29ce484222325ULL;
            for (auto* p = utf8.getAddress(); *p != 0; ++p)
            {
                hash ^= (juce::uint64) (juce::uint8) *p;
                hash *= 0x100000001b3ULL;
            }
            const auto manufacturer = (juce::uint32) (hash >> 32);
            const auto plugin       = (juce::uint32) (hash & 0xffffffffu);
            return { codeBytesFromUint (manufacturer),
                     codeBytesFromUint (plugin) };
        }

        int replaceImmediateBytes (juce::MemoryBlock& data,
                                   const std::array<juce::uint8, 4>& needle,
                                   const std::array<juce::uint8, 4>& replacement)
        {
            const auto* hay  = static_cast<const juce::uint8*> (data.getData());
            const size_t end = data.getSize();
            int patched = 0;
            for (size_t i = 0; i + needle.size() <= end; ++i)
            {
                if (std::memcmp (hay + i, needle.data(), needle.size()) != 0)
                    continue;
                std::memcpy ((juce::uint8*) data.getData() + i,
                             replacement.data(), replacement.size());
                ++patched;
                i += needle.size() - 1;
            }
            return patched;
        }

        struct ClassIdPatchSummary
        {
            int manufacturerImmediates = 0;
            int pluginImmediates       = 0;
            bool moduleInfoUpdated     = false;
        };

        struct TemplateSpec
        {
            juce::String bundleName;
            juce::String environmentVariable;
            juce::String buildTargetFolder;
            juce::uint32 pluginCode = kPluginCode;
        };

        TemplateSpec templateForEngine (const juce::String& engineId)
        {
            if (engineId.equalsIgnoreCase ("fx") || engineId.equalsIgnoreCase ("effect"))
                return { kPlayerFxBundleName, "PATCHCRAFT_PLAYER_FX_TEMPLATE",
                         "PatchCraftPlayerFX_artefacts", kFxPluginCode };

            return { kPlayerBundleName, "PATCHCRAFT_PLAYER_TEMPLATE",
                     "PatchCraftPlayer_artefacts", kPluginCode };
        }

        bool isSameOrChildPath (const juce::File& candidate, const juce::File& parent)
        {
            if (candidate == juce::File() || parent == juce::File())
                return false;

            auto childPath = candidate.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/");
            auto parentPath = parent.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/");
            return childPath.equalsIgnoreCase (parentPath)
                || childPath.startsWithIgnoreCase (parentPath + "/");
        }

        juce::File getUserVst3Folder()
        {
           #if JUCE_WINDOWS
            const auto localAppData = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
            if (localAppData.isNotEmpty())
                return juce::File (localAppData)
                    .getChildFile ("Programs")
                    .getChildFile ("Common")
                    .getChildFile ("VST3");

            return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                .getChildFile ("AppData")
                .getChildFile ("Local")
                .getChildFile ("Programs")
                .getChildFile ("Common")
                .getChildFile ("VST3");
           #elif JUCE_MAC
            return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                .getChildFile ("Library")
                .getChildFile ("Audio")
                .getChildFile ("Plug-Ins")
                .getChildFile ("VST3");
           #elif JUCE_LINUX
            return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                .getChildFile (".vst3");
           #else
            return {};
           #endif
        }

        juce::String quotedJsonString (const juce::String& value)
        {
            return juce::JSON::toString (juce::var (value));
        }

        void replaceModuleInfoStringProperty (juce::String& text,
                                              const juce::String& key,
                                              const juce::String& oldValue,
                                              const juce::String& newValue)
        {
            text = text.replace ("\"" + key + "\": " + quotedJsonString (oldValue),
                                 "\"" + key + "\": " + quotedJsonString (newValue));
        }

        bool rewriteModuleInfoMetadata (const juce::File& moduleInfo,
                                        const juce::String& oldPluginName,
                                        const juce::String& pluginName,
                                        const juce::String& manufacturerName,
                                        const juce::String& version,
                                        juce::String& error)
        {
            if (! moduleInfo.existsAsFile())
            {
                error = "Missing moduleinfo.json in VST3 template.";
                return false;
            }

            auto text = moduleInfo.loadFileAsString();
            replaceModuleInfoStringProperty (text, "Name", oldPluginName, pluginName);
            replaceModuleInfoStringProperty (text, "Vendor", "PatchCraft", manufacturerName);
            replaceModuleInfoStringProperty (text, "Version", "0.1.0", version);

            if (! moduleInfo.replaceWithText (text))
            {
                error = "Could not rewrite moduleinfo.json metadata.";
                return false;
            }
            return true;
        }

        bool rewriteBundleClassIds (const juce::File& bundlePath,
                                    const juce::String& seedString,
                                    juce::uint32 originalPluginCode,
                                    ClassIdPatchSummary& out,
                                    juce::String& error)
        {
            const auto contents = bundlePath.getChildFile ("Contents");
            juce::File archDir;
            if (contents.isDirectory())
            {
                for (auto& entry : juce::RangedDirectoryIterator (contents, false, "*",
                                          juce::File::findDirectories))
                {
                    const auto name = entry.getFile().getFileName();
                    if (name == "Resources") continue;
                    archDir = entry.getFile();
                    break;
                }
            }
            if (archDir == juce::File())
            {
                error = "Could not locate the architecture subfolder inside the VST3 bundle.";
                return false;
            }

            const auto dllFile = archDir.getChildFile (bundlePath.getFileNameWithoutExtension() + ".vst3");
            if (! dllFile.existsAsFile())
            {
                error = "Stamped .vst3 binary missing at " + dllFile.getFullPathName();
                return false;
            }

            juce::MemoryBlock dllBytes;
            if (! dllFile.loadFileAsData (dllBytes))
            {
                error = "Could not read stamped .vst3 binary for ID patching.";
                return false;
            }

            const auto original    = std::make_pair (codeBytesFromUint (kManufacturerCode),
                                                     codeBytesFromUint (originalPluginCode));
            const auto replacement = deriveCodes (seedString);

            out.manufacturerImmediates = replaceImmediateBytes (dllBytes,
                original.first.leBytes,  replacement.first.leBytes);
            out.pluginImmediates       = replaceImmediateBytes (dllBytes,
                original.second.leBytes, replacement.second.leBytes);

            if (out.manufacturerImmediates + out.pluginImmediates == 0)
            {
                error = "Could not find the JUCE class-ID pattern in the stamped binary.";
                return false;
            }

            const auto tmp = dllFile.getSiblingFile (dllFile.getFileName() + ".patching");
            tmp.deleteFile();
            if (! tmp.replaceWithData (dllBytes.getData(), dllBytes.getSize()))
            {
                error = "Could not write patched binary to " + tmp.getFullPathName();
                return false;
            }
            if (! tmp.moveFileTo (dllFile))
            {
                tmp.deleteFile();
                error = "Could not move patched binary back into place.";
                return false;
            }

            const auto moduleInfo = bundlePath.getChildFile ("Contents")
                                              .getChildFile ("Resources")
                                              .getChildFile ("moduleinfo.json");
            if (! moduleInfo.existsAsFile())
            {
                error = "Missing moduleinfo.json in VST3 template.";
                return false;
            }

            {
                auto text = moduleInfo.loadFileAsString();
                bool changed = false;
                if (text.contains (original.first.displayHex))
                {
                    text = text.replace (original.first.displayHex,  replacement.first.displayHex);
                    changed = true;
                }
                if (text.contains (original.second.displayHex))
                {
                    text = text.replace (original.second.displayHex, replacement.second.displayHex);
                    changed = true;
                }
                if (changed)
                {
                    if (! moduleInfo.replaceWithText (text))
                    {
                        error = "Could not rewrite moduleinfo.json with patched class IDs.";
                        return false;
                    }
                    out.moduleInfoUpdated = true;
                }
            }

            if (! out.moduleInfoUpdated)
            {
                error = "moduleinfo.json did not contain the expected class IDs.";
                return false;
            }

            return true;
        }

        juce::PropertiesFile::Options propsOptions()
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = kPropertiesAppName;
            o.filenameSuffix      = ".settings";
            o.osxLibrarySubFolder = "Application Support";
            o.folderName          = kPropertiesAppName;
            o.storageFormat       = juce::PropertiesFile::storeAsXML;
            return o;
        }

        juce::PropertiesFile openProperties()
        {
            auto opts = propsOptions();
            opts.filenameSuffix = juce::String (kPropertiesFileName) + ".settings";
            return juce::PropertiesFile (opts);
        }

        juce::String sanitiseForFilename (const juce::String& in)
        {
            juce::String out;
            for (auto c : in)
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_')
                    out << c;
                else if (c == ' ')
                    out << '_';
            }
            if (out.isEmpty())
                out = "PatchCraftPlugin";
            return out;
        }

        juce::String exportedProductName (const Manifest& manifest,
                                          const VstExportModule::ExportOptions& options)
        {
            if (manifest.whiteLabelPackageName.trim().isNotEmpty())
                return manifest.whiteLabelPackageName.trim();
            if (manifest.playerDisplayName.trim().isNotEmpty())
                return manifest.playerDisplayName.trim();
            return options.pluginName.trim().isNotEmpty() ? options.pluginName.trim() : manifest.instrumentName.trim();
        }

        juce::String exportedPublisherName (const Manifest& manifest,
                                            const VstExportModule::ExportOptions& options)
        {
            if (manifest.whiteLabelPublisher.trim().isNotEmpty())
                return manifest.whiteLabelPublisher.trim();
            if (manifest.playerClientName.trim().isNotEmpty())
                return manifest.playerClientName.trim();
            return options.manufacturerName.trim().isNotEmpty() ? options.manufacturerName.trim() : manifest.creator.trim();
        }

        juce::String exportedProductCode (const Manifest& manifest,
                                          const VstExportModule::ExportOptions& options)
        {
            if (manifest.whiteLabelProductCode.trim().isNotEmpty())
                return manifest.whiteLabelProductCode.trim();
            return sanitiseForFilename (exportedProductName (manifest, options)).toUpperCase();
        }

        juce::File firstExisting (std::initializer_list<juce::File> candidates)
        {
            for (const auto& c : candidates)
                if (c.exists())
                    return c;
            return {};
        }

        juce::File findTemplateBundle (const TemplateSpec& spec)
        {
            const auto envOverride = juce::SystemStats::getEnvironmentVariable (
                spec.environmentVariable, {});
            if (envOverride.isNotEmpty())
            {
                const juce::File envFile (envOverride);
                if (envFile.exists())
                    return envFile;
            }

            const auto self    = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const auto selfDir = self.getParentDirectory();
            const auto sysVst3 = VstExportModule::getSystemVst3Folder();

            return firstExisting ({
                selfDir.getChildFile ("PluginTemplate").getChildFile (spec.bundleName),
                selfDir.getChildFile (spec.bundleName),
                selfDir.getParentDirectory().getParentDirectory()
                       .getChildFile (spec.buildTargetFolder)
                       .getChildFile (selfDir.getFileName())
                       .getChildFile ("VST3").getChildFile (spec.bundleName),
                sysVst3.getChildFile (spec.bundleName)
            });
        }

        bool appInstalledTemplateExists (const TemplateSpec& spec)
        {
            const auto envOverride = juce::SystemStats::getEnvironmentVariable (
                spec.environmentVariable, {});
            if (envOverride.isNotEmpty() && juce::File (envOverride).exists())
                return true;

            const auto self    = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const auto selfDir = self.getParentDirectory();
            return selfDir.getChildFile ("PluginTemplate").getChildFile (spec.bundleName).exists()
                || selfDir.getParentDirectory().getParentDirectory()
                       .getChildFile (spec.buildTargetFolder)
                       .getChildFile (selfDir.getFileName())
                       .getChildFile ("VST3").getChildFile (spec.bundleName).exists();
        }

        // Recursive directory copy that's tolerant of the few oddities we
        // hit when stamping a VST3 bundle (read-only flags from a system
        // install, partially-existing destinations).
        bool copyDirectoryRecursively (const juce::File& src,
                                       const juce::File& dst,
                                       juce::String& error)
        {
            if (! src.isDirectory())
            {
                error = "Source is not a directory: " + src.getFullPathName();
                return false;
            }

            const auto created = dst.createDirectory();
            if (created.failed())
            {
                error = "Could not create " + dst.getFullPathName()
                      + ": " + created.getErrorMessage();
                return false;
            }

            for (auto& entry : juce::RangedDirectoryIterator (src, false, "*",
                                  juce::File::findFilesAndDirectories))
            {
                const auto child = entry.getFile();
                const auto target = dst.getChildFile (child.getFileName());
                if (child.isDirectory())
                {
                    if (! copyDirectoryRecursively (child, target, error))
                        return false;
                }
                else
                {
                    target.deleteFile();
                    if (! child.copyFileTo (target))
                    {
                        error = "Could not copy " + child.getFullPathName()
                              + " -> " + target.getFullPathName();
                        return false;
                    }
                }
            }
            return true;
        }
    }

    //==============================================================================
    // Licensing
    //==============================================================================
    juce::String VstExportModule::getMachineId()
    {
        return juce::SystemStats::getUniqueDeviceID();
    }

    bool VstExportModule::validateLicenseKey (const juce::String& key)
    {
        const auto trimmed = key.trim();
        if (trimmed.length() < 8)
            return false;
        int sum = 0;
        for (auto c : trimmed)
            sum += (int) c;
        return (sum % 100) == 42;
    }

    bool VstExportModule::storeLicenseKey (const juce::String& key)
    {
        auto props = openProperties();
        props.setValue ("licenseKey", key);
        props.setValue ("machineId",  getMachineId());
        props.setValue ("activated",  true);
        return props.saveIfNeeded();
    }

    bool VstExportModule::isModuleActivated()
    {
        auto props = openProperties();
        return props.getBoolValue ("activated", false);
    }

    bool VstExportModule::isVstExpansionInstalled()
    {
        return appInstalledTemplateExists (templateForEngine ("synth"))
            && appInstalledTemplateExists (templateForEngine ("fx"));
    }

    juce::String VstExportModule::vstExpansionInstallMessage()
    {
        return "PatchCraft VST Expansion is not installed.\n\n"
               "The base PatchCraft release can export Player packs and branded "
               "customer installer kits. Standalone branded VST3 export is a "
               "paid addon. Install the PatchCraft VST Expansion package to add "
               "PluginTemplate/PatchCraft Player.vst3 and PatchCraft Player FX.vst3 "
               "beside PatchCraftStudio.exe.";
    }

    //==============================================================================
    // Locating the template plugin and system VST3 folder
    //==============================================================================
    juce::File VstExportModule::getSystemVst3Folder()
    {
       #if JUCE_WINDOWS
        const auto programFiles = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);
        const auto commonFiles  = programFiles.getSiblingFile ("Common Files");
        return commonFiles.getChildFile ("VST3");
       #elif JUCE_MAC
        return juce::File ("/Library/Audio/Plug-Ins/VST3");
       #elif JUCE_LINUX
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                   .getChildFile (".vst3");
       #else
        return {};
       #endif
    }

    juce::File VstExportModule::findPlayerTemplateBundle()
    {
        return findTemplateBundle (templateForEngine ("synth"));
    }

    //==============================================================================
    // Headless export
    //==============================================================================
    VstExportModule::ExportResult VstExportModule::exportPlugin (const PatchCraftProject& project,
                                                                  const ExportOptions& options)
    {
        ExportResult result;

        if (options.pluginName.isEmpty() || options.fileSafeName.isEmpty())
        {
            result.message = "Plugin name is required.";
            return result;
        }

        if (! isVstExpansionInstalled())
        {
            result.message = vstExpansionInstallMessage();
            return result;
        }

        auto exportFolder = options.outputFolder;
        juce::String protectedPathNote;
        const auto systemVst3Folder = getSystemVst3Folder();
        if (systemVst3Folder != juce::File()
            && isSameOrChildPath (exportFolder, systemVst3Folder))
        {
            const auto userVst3Folder = getUserVst3Folder();
            if (userVst3Folder == juce::File())
            {
                result.message = "The selected output folder is the protected system VST3 folder:\n"
                               + systemVst3Folder.getFullPathName()
                               + "\n\nExport to Documents\\PatchCraft\\VST3 Exports first, then copy the .vst3 bundle to the system VST3 folder as Administrator if needed.";
                return result;
            }

            exportFolder = userVst3Folder;
            protectedPathNote = "The selected system VST3 folder is protected:\n"
                              + systemVst3Folder.getFullPathName()
                              + "\n\nPatchCraft exported to the per-user VST3 folder instead:\n"
                              + exportFolder.getFullPathName()
                              + "\n\nMost DAWs scan this folder without administrator rights. If your DAW does not, copy the exported .vst3 to the system VST3 folder as Administrator.";
        }

        if (! exportFolder.isDirectory())
        {
            const auto created = exportFolder.createDirectory();
            if (created.failed())
            {
                result.message = "Could not create output folder: "
                               + created.getErrorMessage();
                return result;
            }
        }

        const auto templateSpec = templateForEngine (project.getManifest().engine);
        const auto templateBundle = findTemplateBundle (templateSpec);
        if (templateBundle == juce::File())
        {
            result.message = "Could not locate the PatchCraft Player VST3 template. "
                             "Reinstall Studio so that PluginTemplate/" + templateSpec.bundleName
                             + " sits next to PatchCraftStudio.exe, or copy that bundle "
                             "manually next to the Studio executable.";
            return result;
        }

        const auto bundleName = options.fileSafeName + ".vst3";
        const auto bundlePath = exportFolder.getChildFile (bundleName);

        // Clear any previous attempt - .vst3 on Windows is a folder, so a
        // simple "replace" semantics has to wipe the whole tree first.
        if (bundlePath.exists())
            bundlePath.deleteRecursively();

        juce::String copyError;
        if (! copyDirectoryRecursively (templateBundle, bundlePath, copyError))
        {
            result.message = "Failed to copy Player template: " + copyError;
            return result;
        }

        // Rename the inner DLL so it matches the renamed bundle. VST3 hosts
        // expect the binary at <Bundle>/Contents/<arch>/<Bundle> - if we
        // leave the inner DLL named "PatchCraft Player.vst3" inside a
        // bundle called "MyPlugin.vst3", DAWs silently skip it.
        const auto contents = bundlePath.getChildFile ("Contents");
        if (contents.isDirectory())
        {
            for (auto& entry : juce::RangedDirectoryIterator (contents, false, "*",
                                      juce::File::findDirectories))
            {
                const auto archDir = entry.getFile();
                if (archDir.getFileName() == "Resources") continue;
                const auto templateName = templateBundle.getFileNameWithoutExtension();
                const auto oldBinary = archDir.getChildFile (templateName + ".vst3");
                if (oldBinary.existsAsFile())
                {
                    const auto newBinary = archDir.getChildFile (options.fileSafeName + ".vst3");
                    newBinary.deleteFile();
                    oldBinary.moveFileTo (newBinary);
                }
            }
        }

        // Give each exported plugin its own VST3 class IDs so two exports
        // from the same Studio install can coexist in the same DAW.
        ClassIdPatchSummary patchSummary;
        juce::String patchError;
        const auto seed = options.fileSafeName + "|" + options.pluginName;
        const bool patchOk = rewriteBundleClassIds (bundlePath, seed, templateSpec.pluginCode,
                                                    patchSummary, patchError);
        if (! patchOk)
        {
            result.message = "Failed to make plugin IDs unique: " + patchError;
            return result;
        }
        const int totalPatched = patchSummary.manufacturerImmediates
                               + patchSummary.pluginImmediates;

        const auto moduleInfo = bundlePath.getChildFile ("Contents")
                                          .getChildFile ("Resources")
                                          .getChildFile ("moduleinfo.json");
        juce::String moduleInfoError;
        if (! rewriteModuleInfoMetadata (moduleInfo,
                                         templateBundle.getFileNameWithoutExtension(),
                                         options.pluginName,
                                         options.manufacturerName,
                                         options.version,
                                         moduleInfoError))
        {
            result.message = "Failed to stamp plugin metadata: " + moduleInfoError;
            return result;
        }

        // Embed the pack ----------------------------------------------------
        const auto resourcesDir   = bundlePath.getChildFile ("Contents").getChildFile ("Resources");
        const auto embeddedFolder = resourcesDir.getChildFile ("EmbeddedPack");

        if (embeddedFolder.exists())
            embeddedFolder.deleteRecursively();

        const auto created = embeddedFolder.createDirectory();
        if (created.failed())
        {
            result.message = "Could not create embedded pack folder: "
                           + created.getErrorMessage();
            return result;
        }

        PatchCraftPackWriter writer;
        juce::String packError;
        if (! writer.write (project, embeddedFolder, packError))
        {
            result.message = "Failed to write embedded pack: " + packError;
            return result;
        }

        // A standalone exported instrument owns its embedded pack. It should
        // not expose the generic Player pack loader, otherwise users can load
        // unrelated PatchCraft instruments inside the exported product.
        const auto embeddedManifestFile = embeddedFolder.getChildFile ("manifest.json");
        auto embeddedManifest = juce::JSON::parse (embeddedManifestFile);
        if (auto* manifestObject = embeddedManifest.getDynamicObject())
        {
            manifestObject->setProperty ("playerShowPackMenu", false);
            manifestObject->setProperty ("playerAllowPackLoading", false);
            manifestObject->setProperty ("playerShowLibraryBrowser", false);
            embeddedManifestFile.replaceWithText (juce::JSON::toString (embeddedManifest, true));
        }

        // Drop a small manifest so users know what the bundle is.
        auto exportInfo = new juce::DynamicObject();
        exportInfo->setProperty ("pluginName",   options.pluginName);
        exportInfo->setProperty ("manufacturer", options.manufacturerName);
        exportInfo->setProperty ("version",      options.version);
        exportInfo->setProperty ("exportedAt",   juce::Time::getCurrentTime().toISO8601 (true));
        exportInfo->setProperty ("sourceProject", project.getManifest().instrumentName);
        resourcesDir.getChildFile ("export_info.json")
                    .replaceWithText (juce::JSON::toString (juce::var (exportInfo), true));

        const auto& manifest = project.getManifest();
        auto whiteLabel = new juce::DynamicObject();
        whiteLabel->setProperty ("schema", "patchcraft.exported_white_label_product.v1");
        whiteLabel->setProperty ("plugin_name", options.pluginName);
        whiteLabel->setProperty ("product_name", exportedProductName (manifest, options));
        whiteLabel->setProperty ("publisher", exportedPublisherName (manifest, options));
        whiteLabel->setProperty ("manufacturer", options.manufacturerName);
        whiteLabel->setProperty ("version", options.version);
        whiteLabel->setProperty ("product_code", exportedProductCode (manifest, options));
        whiteLabel->setProperty ("bundle_name", bundleName);
        whiteLabel->setProperty ("engine", project.getEngineType());
        whiteLabel->setProperty ("license_required", manifest.licenseRequired);
        whiteLabel->setProperty ("license_product_id", manifest.licenseProductId);
        whiteLabel->setProperty ("license_server_url", manifest.licenseServerUrl);
        whiteLabel->setProperty ("trial_days", manifest.trialDays);
        whiteLabel->setProperty ("offline_grace_days", manifest.licenseOfflineGraceDays);
        whiteLabel->setProperty ("support_email", manifest.playerSupportEmail);
        whiteLabel->setProperty ("support_url", manifest.playerSupportUrl);
        whiteLabel->setProperty ("manual_url", manifest.playerManualUrl);
        whiteLabel->setProperty ("store_url", manifest.playerStoreUrl);
        whiteLabel->setProperty ("exported_at", juce::Time::getCurrentTime().toISO8601 (true));
        resourcesDir.getChildFile ("white_label_product.json")
                    .replaceWithText (juce::JSON::toString (juce::var (whiteLabel), true));

        result.bundlePath = bundlePath;

        // Install to system VST3 directory ---------------------------------
        if (options.installToSystemVst3)
        {
            const auto sysDir = getSystemVst3Folder();
            juce::String installNote;

            auto tryInstallTo = [&] (const juce::File& targetRoot,
                                     juce::String& installError) -> bool
            {
                if (targetRoot == juce::File())
                {
                    installError = "Unknown VST3 folder.";
                    return false;
                }

                const auto createdRoot = targetRoot.createDirectory();
                if (createdRoot.failed())
                {
                    installError = "Could not access " + targetRoot.getFullPathName()
                                 + ": " + createdRoot.getErrorMessage();
                    return false;
                }

                const auto installPath = targetRoot.getChildFile (bundleName);
                if (installPath == bundlePath)
                {
                    result.installedPath = installPath;
                    return true;
                }

                if (installPath.exists())
                    installPath.deleteRecursively();

                return copyDirectoryRecursively (bundlePath, installPath, installError)
                    ? (result.installedPath = installPath, true)
                    : false;
            };

            if (sysDir == juce::File())
            {
                installNote = "System VST3 folder unknown for this OS - install skipped.";
            }
            else
            {
                juce::String installErr;
                if (! tryInstallTo (sysDir, installErr))
                {
                    juce::String userInstallErr;
                    if (tryInstallTo (getUserVst3Folder(), userInstallErr))
                    {
                        installNote = "System VST3 install failed: " + installErr
                                    + "\nInstalled to the per-user VST3 folder instead.";
                    }
                    else
                    {
                        installNote = "System VST3 install failed: " + installErr
                                    + "\nPer-user VST3 install also failed: " + userInstallErr
                                    + "\nCopy the exported .vst3 manually as Administrator if needed.";
                    }
                }
            }

            if (installNote.isNotEmpty())
                protectedPathNote = protectedPathNote.isNotEmpty()
                    ? protectedPathNote + "\n\n" + installNote
                    : installNote;
        }

        result.success = true;
        result.message = "Exported VST3:\n" + bundlePath.getFullPathName()
                       + (result.installedPath != juce::File()
                              ? "\n\nInstalled to:\n" + result.installedPath.getFullPathName()
                                + "\n\nReopen your DAW (or rescan plugins) to see "
                                + options.pluginName + "."
                              : juce::String());
        if (protectedPathNote.isNotEmpty())
            result.message += "\n\n" + protectedPathNote;
        if (totalPatched == 0)
        {
            result.message += "\n\nWarning: could not find the JUCE class-ID "
                              "pattern in the stamped binary, so this plugin "
                              "will share its VST3 identity with the PatchCraft "
                              "Player. DAWs may only load one of them at a time. "
                              "Rebuild Studio against the matching Player binary.";
        }
        return result;
    }

    //==============================================================================
    // UI: license dialog + export dialog
    //==============================================================================
    namespace
    {
        void showResult (const VstExportModule::ExportResult& res, juce::Component* parent)
        {
            juce::Component::SafePointer<juce::Component> safe (parent);
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withAssociatedComponent (safe.getComponent())
                    .withIconType (res.success ? juce::MessageBoxIconType::InfoIcon
                                               : juce::MessageBoxIconType::WarningIcon)
                    .withTitle (res.success ? "VST Export Complete" : "VST Export Failed")
                    .withMessage (res.message)
                    .withButton ("OK"),
                nullptr);
        }

        void runExportDialog (juce::Component* parent, const PatchCraftProject& project)
        {
            auto* aw = new juce::AlertWindow ("Export VST3 Plugin",
                                              "Configure your plugin and export.",
                                              juce::MessageBoxIconType::InfoIcon, parent);

            const auto& manifest = project.getManifest();
            const auto defaultName = manifest.instrumentName.isNotEmpty()
                                         ? manifest.instrumentName : juce::String ("MyPlugin");
            const auto defaultManu = manifest.creator.isNotEmpty()
                                         ? manifest.creator : juce::String ("PatchCraft");
            const auto defaultVer  = manifest.version.isNotEmpty()
                                         ? manifest.version : juce::String ("1.0.0");

            aw->addTextEditor ("name",    defaultName, "Plugin name:");
            aw->addTextEditor ("manu",    defaultManu, "Manufacturer:");
            aw->addTextEditor ("ver",     defaultVer,  "Version:");

            const auto defaultOut = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                        .getChildFile ("PatchCraft")
                                        .getChildFile ("VST3 Exports");
            aw->addTextEditor ("outdir",  defaultOut.getFullPathName(), "Output folder:");

            aw->addButton ("Export",  1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel",  0, juce::KeyPress (juce::KeyPress::escapeKey));

            juce::Component::SafePointer<juce::Component> safeParent (parent);
            // Keep a reference to the project for the async callback. The
            // PatchCraftProject is owned by StudioMainComponent and lives
            // for the duration of the app, so a raw pointer is safe.
            const PatchCraftProject* projectPtr = &project;

            aw->enterModalState (true,
                juce::ModalCallbackFunction::create (
                    [aw, safeParent, projectPtr] (int result)
                    {
                        std::unique_ptr<juce::AlertWindow> owned (aw);
                        if (result != 1)
                            return;

                        VstExportModule::ExportOptions opts;
                        opts.pluginName       = owned->getTextEditorContents ("name").trim();
                        opts.fileSafeName     = sanitiseForFilename (opts.pluginName);
                        opts.manufacturerName = owned->getTextEditorContents ("manu").trim();
                        opts.version          = owned->getTextEditorContents ("ver").trim();
                        opts.outputFolder     = juce::File (owned->getTextEditorContents ("outdir").trim());
                        opts.installToSystemVst3 = false;

                        if (opts.pluginName.isEmpty())
                        {
                            juce::AlertWindow::showAsync (
                                juce::MessageBoxOptions()
                                    .withTitle ("VST Export")
                                    .withMessage ("Plugin name is required.")
                                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                                    .withButton ("OK"),
                                nullptr);
                            return;
                        }

                        const auto res = VstExportModule::exportPlugin (*projectPtr, opts);
                        showResult (res, safeParent.getComponent());
                    }));
        }

        void runActivationDialog (juce::Component* parent, const PatchCraftProject& project)
        {
            auto* aw = new juce::AlertWindow ("Activate VST Export",
                                              "The VST Export addon is required to build "
                                              "installable VST3 plugins from your projects.\n\n"
                                              "Enter your license key, or paste the trial key "
                                              "'PATCHCRAFT55' to evaluate the addon.\n\n"
                                              "Trial keys unlock every export feature so you "
                                              "can build and install a real plugin without a "
                                              "paid license. The key is stored once per machine.",
                                              juce::MessageBoxIconType::QuestionIcon,
                                              parent);

            aw->addTextEditor ("key", "PATCHCRAFT55", "License key:");
            aw->addButton ("Activate", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel",   0, juce::KeyPress (juce::KeyPress::escapeKey));

            juce::Component::SafePointer<juce::Component> safeParent (parent);
            const PatchCraftProject* projectPtr = &project;

            aw->enterModalState (true,
                juce::ModalCallbackFunction::create (
                    [aw, safeParent, projectPtr] (int result)
                    {
                        std::unique_ptr<juce::AlertWindow> owned (aw);
                        if (result != 1)
                            return;

                        const auto key = owned->getTextEditorContents ("key").trim();
                        if (! VstExportModule::validateLicenseKey (key))
                        {
                            juce::AlertWindow::showAsync (
                                juce::MessageBoxOptions()
                                    .withTitle ("Activation Failed")
                                    .withMessage ("That license key isn't valid. "
                                                  "Please check the key and try again.")
                                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                                    .withButton ("OK"),
                                nullptr);
                            return;
                        }

                        VstExportModule::storeLicenseKey (key);
                        runExportDialog (safeParent.getComponent(), *projectPtr);
                    }));
        }
    }

    void VstExportModule::showExportDialog (juce::Component* parent,
                                            const PatchCraftProject& project)
    {
        if (! isVstExpansionInstalled())
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withAssociatedComponent (parent)
                    .withTitle ("VST Expansion Required")
                    .withMessage (vstExpansionInstallMessage())
                    .withIconType (juce::MessageBoxIconType::InfoIcon)
                    .withButton ("OK"),
                nullptr);
            return;
        }

        if (! isModuleActivated())
        {
            runActivationDialog (parent, project);
            return;
        }
        runExportDialog (parent, project);
    }

} // namespace patchcraft
