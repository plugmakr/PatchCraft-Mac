#include "PcexpManager.h"
#include "LicenseValidator.h"

#include <memory>

namespace patchcraft
{
    namespace
    {
        static juce::String trimmedStringProperty (const juce::DynamicObject& object,
                                                  const juce::Identifier& key)
        {
            return object.getProperty (key).toString().trim();
        }

        static juce::var parseJsonFile (const juce::File& file, juce::String& error)
        {
            if (! file.existsAsFile())
            {
                error = "Missing " + file.getFullPathName();
                return {};
            }

            auto parsed = juce::JSON::parse (file);
            if (parsed.isVoid() || parsed.getDynamicObject() == nullptr)
                error = "Invalid JSON in " + file.getFullPathName();
            return parsed;
        }

        static juce::var parseJsonText (const juce::String& text, const juce::String& source, juce::String& error)
        {
            auto parsed = juce::JSON::parse (text);
            if (parsed.isVoid() || parsed.getDynamicObject() == nullptr)
                error = "Invalid JSON in " + source;
            return parsed;
        }

        static bool extensionLooksLikePcexp (const juce::File& file)
        {
            return file.getFileExtension().equalsIgnoreCase (".pcexp");
        }
    }

    bool PcexpManager::Manifest::requiresLicense() const noexcept
    {
        return licenseMode == LicenseMode::LocalKey || licenseMode == LicenseMode::External;
    }

    juce::var PcexpManager::Manifest::toVar() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("format", format);
        object->setProperty ("formatVersion", formatVersion);
        object->setProperty ("id", id);
        object->setProperty ("name", name);
        object->setProperty ("version", version);
        object->setProperty ("kind", kind);
        object->setProperty ("author", author);
        object->setProperty ("description", description);
        object->setProperty ("minPatchCraftVersion", minPatchCraftVersion);
        object->setProperty ("productId", productId);
        object->setProperty ("licenseEndpoint", licenseEndpoint);
        object->setProperty ("licenseMode", PcexpManager::licenseModeToString (licenseMode));
        object->setProperty ("capabilities", PcexpManager::arrayToVar (capabilities));
        object->setProperty ("dependencies", PcexpManager::arrayToVar (dependencies));
        object->setProperty ("tags", PcexpManager::arrayToVar (tags));
        object->setProperty ("builtIn", builtIn);
        return juce::var (object);
    }

    PcexpManager::Manifest PcexpManager::Manifest::fromVar (const juce::var& value)
    {
        Manifest manifest;
        manifest.rawManifest = value;

        if (auto* object = value.getDynamicObject())
        {
            manifest.format = trimmedStringProperty (*object, "format");
            if (manifest.format.isEmpty())
                manifest.format = "PatchCraft Extension";

            manifest.formatVersion = object->hasProperty ("formatVersion")
                ? (int) object->getProperty ("formatVersion")
                : 1;
            manifest.id = trimmedStringProperty (*object, "id");
            manifest.name = trimmedStringProperty (*object, "name");
            manifest.version = trimmedStringProperty (*object, "version");
            if (manifest.version.isEmpty())
                manifest.version = "1.0.0";
            manifest.kind = trimmedStringProperty (*object, "kind");
            manifest.author = trimmedStringProperty (*object, "author");
            manifest.description = trimmedStringProperty (*object, "description");
            manifest.minPatchCraftVersion = trimmedStringProperty (*object, "minPatchCraftVersion");
            if (manifest.minPatchCraftVersion.isEmpty())
                manifest.minPatchCraftVersion = "0.1.0";

            manifest.productId = trimmedStringProperty (*object, "productId");
            manifest.licenseEndpoint = trimmedStringProperty (*object, "licenseEndpoint");
            manifest.licenseMode = PcexpManager::licenseModeFromString (trimmedStringProperty (*object, "licenseMode"));

            if (auto* license = object->getProperty ("license").getDynamicObject())
            {
                const auto mode = trimmedStringProperty (*license, "mode");
                if (mode.isNotEmpty())
                    manifest.licenseMode = PcexpManager::licenseModeFromString (mode);

                const auto product = trimmedStringProperty (*license, "productId");
                if (product.isNotEmpty())
                    manifest.productId = product;

                const auto endpoint = trimmedStringProperty (*license, "endpoint");
                if (endpoint.isNotEmpty())
                    manifest.licenseEndpoint = endpoint;
            }

            manifest.capabilities = PcexpManager::arrayFromVar (object->getProperty ("capabilities"));
            manifest.dependencies = PcexpManager::arrayFromVar (object->getProperty ("dependencies"));
            manifest.tags = PcexpManager::arrayFromVar (object->getProperty ("tags"));
            manifest.builtIn = object->hasProperty ("builtIn")
                ? (bool) object->getProperty ("builtIn")
                : false;
        }

        return manifest;
    }

    bool PcexpManager::InstalledExpansion::isUsable() const noexcept
    {
        return enabled && (licenseState == LicenseState::BuiltIn
                          || licenseState == LicenseState::NotRequired
                          || licenseState == LicenseState::Licensed);
    }

    juce::String PcexpManager::InstalledExpansion::statusText() const
    {
        if (! enabled)
            return "Disabled";
        return PcexpManager::licenseStateToString (licenseState);
    }

    PcexpManager::PcexpManager (juce::File userRoot, juce::File bundledRoot)
        : userExpansionRoot (std::move (userRoot)),
          bundledExpansionRoot (std::move (bundledRoot))
    {
    }

    juce::File PcexpManager::defaultUserExpansionRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
                .getChildFile ("Extensions");
    }

    juce::File PcexpManager::defaultBundledExpansionRoot()
    {
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
            .getParentDirectory()
            .getChildFile ("Extensions");
    }

    PcexpManager::Manifest PcexpManager::builtInPscriptManifest()
    {
        Manifest manifest;
        manifest.id = "com.patchcraft.pscript.core";
        manifest.name = "pScript Core";
        manifest.version = "1.0.0";
        manifest.kind = "scripting-language";
        manifest.author = "PatchCraft";
        manifest.description = "Built-in safe PatchCraft scripting language for MIDI, macros, UI logic, automation, validation, and preset recipes.";
        manifest.licenseMode = LicenseMode::BuiltIn;
        manifest.builtIn = true;
        manifest.capabilities.addArray ({ "script.pscript",
                                          "script.events",
                                          "script.macros",
                                          "script.midi",
                                          "script.ui",
                                          "script.samplemap",
                                          "script.validation" });
        manifest.tags.addArray ({ "built-in", "scripting", "pscript" });
        return manifest;
    }

    const std::vector<PcexpManager::InstalledExpansion>& PcexpManager::scanInstalled()
    {
        installed.clear();

        InstalledExpansion pscript;
        pscript.manifest = builtInPscriptManifest();
        pscript.enabled = true;
        pscript.bundled = true;
        pscript.licenseState = LicenseState::BuiltIn;
        installed.push_back (std::move (pscript));

        scanRoot (bundledExpansionRoot, true);
        scanRoot (userExpansionRoot, false);
        return installed;
    }

    PcexpManager::PackageValidation PcexpManager::validatePackage (const juce::File& package) const
    {
        if (package.isDirectory())
            return validateDirectoryPackage (package);

        if (package.existsAsFile() && extensionLooksLikePcexp (package))
            return validateArchivePackage (package);

        PackageValidation validation;
        validation.error = "Expected a .pcexp archive or expansion folder.";
        return validation;
    }

    juce::Result PcexpManager::installPackage (const juce::File& package, bool overwriteExisting)
    {
        const auto validation = validatePackage (package);
        if (! validation.valid)
            return juce::Result::fail (validation.error);

        auto rootResult = userExpansionRoot.createDirectory();
        if (rootResult.failed())
            return rootResult;

        const auto destination = userExpansionRoot.getChildFile (installFolderNameFor (validation.manifest));
        if (destination.exists())
        {
            if (! overwriteExisting)
                return juce::Result::fail ("Extension is already installed: " + validation.manifest.id);
            if (! destination.deleteRecursively())
                return juce::Result::fail ("Could not replace existing expansion folder: " + destination.getFullPathName());
        }

        if (validation.isDirectory)
        {
            auto result = copyDirectoryRecursively (package, destination);
            if (result.failed())
                return result;
        }
        else
        {
            juce::ZipFile zip (package);
            auto pathResult = validateZipEntryPaths (zip);
            if (pathResult.failed())
                return pathResult;

            auto unzipResult = zip.uncompressTo (destination, true);
            if (unzipResult.failed())
                return unzipResult;
        }

        PersistentState state;
        state.enabled = true;
        state.installedAt = juce::Time::getCurrentTime().toISO8601 (true);
        auto stateResult = writeState (destination, state);
        if (stateResult.failed())
            return stateResult;

        scanInstalled();
        return juce::Result::ok();
    }

    juce::Result PcexpManager::uninstall (const juce::String& expansionId)
    {
        scanInstalled();
        const auto* expansion = findInstalled (expansionId);
        if (expansion == nullptr)
            return juce::Result::fail ("Extension is not installed: " + expansionId);
        if (expansion->manifest.builtIn || expansion->bundled)
            return juce::Result::fail ("Built-in or bundled expansions cannot be removed from Settings.");
        if (! expansion->installRoot.isDirectory())
            return juce::Result::fail ("Extension has no writable install folder: " + expansionId);
        if (! expansion->installRoot.deleteRecursively())
            return juce::Result::fail ("Could not remove " + expansion->installRoot.getFullPathName());
        scanInstalled();
        return juce::Result::ok();
    }

    juce::Result PcexpManager::setEnabled (const juce::String& expansionId, bool shouldBeEnabled)
    {
        scanInstalled();
        const auto* expansion = findInstalled (expansionId);
        if (expansion == nullptr)
            return juce::Result::fail ("Extension is not installed: " + expansionId);
        if (expansion->manifest.builtIn)
            return juce::Result::fail ("pScript Core is built in and cannot be disabled.");
        if (! expansion->installRoot.isDirectory())
            return juce::Result::fail ("Extension has no writable install folder: " + expansionId);

        auto state = readState (expansion->installRoot);
        state.enabled = shouldBeEnabled;
        auto result = writeState (expansion->installRoot, state);
        scanInstalled();
        return result;
    }

    juce::Result PcexpManager::storeLicenseKey (const juce::String& expansionId, const juce::String& key)
    {
        scanInstalled();
        const auto* expansion = findInstalled (expansionId);
        if (expansion == nullptr)
            return juce::Result::fail ("Extension is not installed: " + expansionId);
        if (expansion->manifest.builtIn)
            return juce::Result::fail ("Built-in expansions do not need license keys.");
        if (! expansion->installRoot.isDirectory())
            return juce::Result::fail ("Extension has no writable install folder: " + expansionId);

        const auto state = evaluateLicense (expansion->manifest, key);
        if (state == LicenseState::Invalid)
            return juce::Result::fail ("License key is not valid for " + expansion->manifest.name);

        auto persistent = readState (expansion->installRoot);
        persistent.licenseKey = key.trim();
        auto result = writeState (expansion->installRoot, persistent);
        scanInstalled();
        return result;
    }

    bool PcexpManager::hasCapability (const juce::String& capability)
    {
        scanInstalled();
        for (const auto& expansion : installed)
            if (expansion.isUsable() && expansion.manifest.capabilities.contains (capability))
                return true;
        return false;
    }

    const PcexpManager::InstalledExpansion* PcexpManager::findInstalled (const juce::String& expansionId)
    {
        for (const auto& expansion : installed)
            if (expansion.manifest.id == expansionId)
                return &expansion;
        return nullptr;
    }

    juce::String PcexpManager::licenseModeToString (LicenseMode mode)
    {
        switch (mode)
        {
            case LicenseMode::BuiltIn:  return "built-in";
            case LicenseMode::None:     return "none";
            case LicenseMode::LocalKey: return "local-key";
            case LicenseMode::External: return "external";
        }
        return "none";
    }

    PcexpManager::LicenseMode PcexpManager::licenseModeFromString (const juce::String& value)
    {
        const auto v = value.trim().toLowerCase();
        if (v == "built-in" || v == "builtin") return LicenseMode::BuiltIn;
        if (v == "local-key" || v == "local" || v == "license-key") return LicenseMode::LocalKey;
        if (v == "external" || v == "server" || v == "online") return LicenseMode::External;
        return LicenseMode::None;
    }

    juce::String PcexpManager::licenseStateToString (LicenseState state)
    {
        switch (state)
        {
            case LicenseState::BuiltIn:     return "Built in";
            case LicenseState::NotRequired: return "Ready";
            case LicenseState::Licensed:    return "Licensed";
            case LicenseState::Unlicensed:  return "License required";
            case LicenseState::Invalid:     return "Invalid license";
        }
        return "Unknown";
    }

    juce::StringArray PcexpManager::arrayFromVar (const juce::var& value)
    {
        juce::StringArray result;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                if (item.toString().trim().isNotEmpty())
                    result.addIfNotAlreadyThere (item.toString().trim());
        return result;
    }

    juce::var PcexpManager::arrayToVar (const juce::StringArray& values)
    {
        juce::Array<juce::var> array;
        for (const auto& value : values)
            array.add (value);
        return juce::var (array);
    }

    juce::String PcexpManager::installFolderNameFor (const Manifest& manifest)
    {
        auto id = manifest.id.replaceCharacter ('.', '-')
                             .replaceCharacter (':', '-')
                             .replaceCharacter ('/', '-')
                             .replaceCharacter ('\\', '-');
        auto version = manifest.version.replaceCharacter ('.', '-')
                                       .replaceCharacter (':', '-')
                                       .replaceCharacter ('/', '-')
                                       .replaceCharacter ('\\', '-');
        return juce::File::createLegalFileName (id + "-" + version);
    }

    juce::Result PcexpManager::copyDirectoryRecursively (const juce::File& source,
                                                         const juce::File& destination)
    {
        auto create = destination.createDirectory();
        if (create.failed())
            return create;

        for (auto& entry : juce::RangedDirectoryIterator (source, false, "*", juce::File::findFilesAndDirectories))
        {
            const auto child = entry.getFile();
            const auto target = destination.getChildFile (child.getFileName());
            if (child.isDirectory())
            {
                auto result = copyDirectoryRecursively (child, target);
                if (result.failed())
                    return result;
            }
            else if (! child.copyFileTo (target))
            {
                return juce::Result::fail ("Could not copy " + child.getFullPathName());
            }
        }

        return juce::Result::ok();
    }

    bool PcexpManager::zipEntryPathIsSafe (const juce::String& path)
    {
        auto normalized = path.replaceCharacter ('\\', '/').trim();
        if (normalized.isEmpty())
            return false;
        if (normalized.startsWithChar ('/') || normalized.startsWith ("~") || normalized.contains (":"))
            return false;

        juce::StringArray parts;
        parts.addTokens (normalized, "/", {});
        for (const auto& part : parts)
            if (part == "..")
                return false;
        return true;
    }

    juce::Result PcexpManager::validateZipEntryPaths (juce::ZipFile& zip)
    {
        for (int i = 0; i < zip.getNumEntries(); ++i)
            if (const auto* entry = zip.getEntry (i))
                if (! zipEntryPathIsSafe (entry->filename))
                    return juce::Result::fail ("Unsafe path inside .pcexp: " + entry->filename);
        return juce::Result::ok();
    }

    PcexpManager::PackageValidation PcexpManager::validateDirectoryPackage (const juce::File& package) const
    {
        juce::String error;
        const auto manifest = parseJsonFile (package.getChildFile (manifestFileName()), error);
        if (error.isNotEmpty())
        {
            PackageValidation validation;
            validation.isDirectory = true;
            validation.error = error;
            return validation;
        }

        return validateManifestObject (manifest, true, false);
    }

    PcexpManager::PackageValidation PcexpManager::validateArchivePackage (const juce::File& package) const
    {
        PackageValidation validation;
        validation.isArchive = true;

        juce::ZipFile zip (package);
        if (zip.getNumEntries() <= 0)
        {
            validation.error = "Could not read .pcexp archive: " + package.getFullPathName();
            return validation;
        }

        auto pathResult = validateZipEntryPaths (zip);
        if (pathResult.failed())
        {
            validation.error = pathResult.getErrorMessage();
            return validation;
        }

        const auto* entry = zip.getEntry (manifestFileName(), true);
        if (entry == nullptr)
        {
            validation.error = ".pcexp archive must contain a top-level manifest.json.";
            return validation;
        }

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (*entry));
        if (stream == nullptr)
        {
            validation.error = "Could not read manifest.json from .pcexp archive.";
            return validation;
        }

        juce::String error;
        const auto manifest = parseJsonText (stream->readEntireStreamAsString(), package.getFullPathName(), error);
        if (error.isNotEmpty())
        {
            validation.error = error;
            return validation;
        }

        return validateManifestObject (manifest, false, true);
    }

    PcexpManager::PackageValidation PcexpManager::validateManifestObject (const juce::var& manifestVar,
                                                                         bool isDirectory,
                                                                         bool isArchive) const
    {
        PackageValidation validation;
        validation.isDirectory = isDirectory;
        validation.isArchive = isArchive;
        validation.manifest = Manifest::fromVar (manifestVar);

        if (validation.manifest.format != "PatchCraft Extension"
            && validation.manifest.format != "PatchCraft Expansion")
        {
            validation.error = "manifest.json format must be \"PatchCraft Extension\".";
            return validation;
        }
        if (validation.manifest.formatVersion < 1)
        {
            validation.error = "manifest.json formatVersion must be 1 or greater.";
            return validation;
        }
        if (validation.manifest.id.isEmpty())
        {
            validation.error = "manifest.json is missing id.";
            return validation;
        }
        if (validation.manifest.name.isEmpty())
        {
            validation.error = "manifest.json is missing name.";
            return validation;
        }
        if (validation.manifest.kind.isEmpty())
        {
            validation.error = "manifest.json is missing kind.";
            return validation;
        }
        if (validation.manifest.capabilities.isEmpty())
        {
            validation.error = "manifest.json must declare at least one capability.";
            return validation;
        }

        validation.valid = true;
        return validation;
    }

    void PcexpManager::scanRoot (const juce::File& root, bool bundled)
    {
        if (! root.isDirectory())
            return;

        for (auto& entry : juce::RangedDirectoryIterator (root, false, "*", juce::File::findFilesAndDirectories))
        {
            const auto package = entry.getFile();
            PackageValidation validation;
            if (package.isDirectory())
                validation = validateDirectoryPackage (package);
            else if (extensionLooksLikePcexp (package))
                validation = validateArchivePackage (package);
            else
                continue;

            if (! validation.valid)
                continue;

            InstalledExpansion expansion;
            expansion.manifest = validation.manifest;
            expansion.bundled = bundled;
            if (package.isDirectory())
                expansion.installRoot = package;
            else
                expansion.packageFile = package;

            const auto state = package.isDirectory() ? readState (package) : PersistentState {};
            expansion.enabled = state.enabled;
            expansion.installedAt = state.installedAt;
            expansion.licenseKey = state.licenseKey;
            expansion.licenseState = evaluateLicense (expansion.manifest, expansion.licenseKey);
            upsertInstalled (std::move (expansion));
        }
    }

    void PcexpManager::upsertInstalled (InstalledExpansion expansion)
    {
        for (auto& existing : installed)
        {
            if (existing.manifest.id == expansion.manifest.id)
            {
                if (! existing.manifest.builtIn)
                    existing = std::move (expansion);
                return;
            }
        }

        installed.push_back (std::move (expansion));
    }

    PcexpManager::PersistentState PcexpManager::readState (const juce::File& installRoot)
    {
        PersistentState state;
        juce::String error;
        const auto parsed = parseJsonFile (installRoot.getChildFile (stateFileName()), error);
        if (error.isNotEmpty())
            return state;

        if (auto* object = parsed.getDynamicObject())
        {
            state.enabled = object->hasProperty ("enabled")
                ? (bool) object->getProperty ("enabled")
                : true;
            state.installedAt = trimmedStringProperty (*object, "installedAt");
            state.licenseKey = trimmedStringProperty (*object, "licenseKey");
        }
        return state;
    }

    juce::Result PcexpManager::writeState (const juce::File& installRoot, const PersistentState& state)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("enabled", state.enabled);
        object->setProperty ("installedAt", state.installedAt.isNotEmpty()
                                           ? state.installedAt
                                           : juce::Time::getCurrentTime().toISO8601 (true));
        object->setProperty ("licenseKey", state.licenseKey);
        const auto json = juce::JSON::toString (juce::var (object), true);
        if (! installRoot.getChildFile (stateFileName()).replaceWithText (json))
            return juce::Result::fail ("Could not write " + installRoot.getChildFile (stateFileName()).getFullPathName());
        return juce::Result::ok();
    }

    PcexpManager::LicenseState PcexpManager::evaluateLicense (const Manifest& manifest,
                                                              const juce::String& licenseKey)
    {
        if (manifest.licenseMode == LicenseMode::BuiltIn || manifest.builtIn)
            return LicenseState::BuiltIn;

        if (! manifest.requiresLicense())
            return LicenseState::NotRequired;

        if (licenseKey.trim().isEmpty())
            return LicenseState::Unlicensed;

        if (manifest.licenseMode == LicenseMode::External)
            return LicenseState::Licensed;

        const auto product = manifest.productId.isNotEmpty() ? manifest.productId : manifest.id;
        return LicenseValidator::validateKey (licenseKey, product) ? LicenseState::Licensed
                                                                   : LicenseState::Invalid;
    }

} // namespace patchcraft
