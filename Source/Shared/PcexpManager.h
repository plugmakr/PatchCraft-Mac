#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

namespace patchcraft
{
    /**
        Manages PatchCraft application extensions packaged as .pcexp files.

        This is intentionally separate from project ExpansionMetadata, which is
        sellable instrument/preset content. A .pcexp extends PatchCraft itself:
        exporters, script runtimes, synth modules, validators, templates, etc.
    */
    class PcexpManager
    {
    public:
        enum class LicenseMode
        {
            BuiltIn,
            None,
            LocalKey,
            External
        };

        enum class LicenseState
        {
            BuiltIn,
            NotRequired,
            Licensed,
            Unlicensed,
            Invalid
        };

        struct Manifest
        {
            juce::String format { "PatchCraft Extension" };
            int formatVersion = 1;
            juce::String id;
            juce::String name;
            juce::String version { "1.0.0" };
            juce::String kind;
            juce::String author;
            juce::String description;
            juce::String minPatchCraftVersion { "0.1.0" };
            juce::String productId;
            juce::String licenseEndpoint;
            LicenseMode licenseMode = LicenseMode::None;
            juce::StringArray capabilities;
            juce::StringArray dependencies;
            juce::StringArray tags;
            juce::var rawManifest;

            bool builtIn = false;

            bool requiresLicense() const noexcept;
            juce::var toVar() const;
            static Manifest fromVar (const juce::var&);
        };

        struct InstalledExpansion
        {
            Manifest manifest;
            juce::File installRoot;
            juce::File packageFile;
            bool enabled = true;
            bool bundled = false;
            juce::String installedAt;
            juce::String licenseKey;
            LicenseState licenseState = LicenseState::NotRequired;

            bool isUsable() const noexcept;
            juce::String statusText() const;
        };

        struct PackageValidation
        {
            bool valid = false;
            bool isDirectory = false;
            bool isArchive = false;
            juce::String error;
            Manifest manifest;
        };

        explicit PcexpManager (juce::File userRoot = defaultUserExpansionRoot(),
                               juce::File bundledRoot = defaultBundledExpansionRoot());

        static juce::File defaultUserExpansionRoot();
        static juce::File defaultBundledExpansionRoot();
        static Manifest builtInPscriptManifest();

        const std::vector<InstalledExpansion>& scanInstalled();
        const std::vector<InstalledExpansion>& getInstalled() const noexcept { return installed; }

        PackageValidation validatePackage (const juce::File& package) const;
        juce::Result installPackage (const juce::File& package, bool overwriteExisting);
        juce::Result uninstall (const juce::String& expansionId);
        juce::Result setEnabled (const juce::String& expansionId, bool shouldBeEnabled);
        juce::Result storeLicenseKey (const juce::String& expansionId, const juce::String& key);

        bool hasCapability (const juce::String& capability);
        const InstalledExpansion* findInstalled (const juce::String& expansionId);

        juce::File getUserRoot() const noexcept { return userExpansionRoot; }
        juce::File getBundledRoot() const noexcept { return bundledExpansionRoot; }

        static juce::String licenseModeToString (LicenseMode);
        static LicenseMode licenseModeFromString (const juce::String&);
        static juce::String licenseStateToString (LicenseState);

    private:
        struct PersistentState
        {
            bool enabled = true;
            juce::String installedAt;
            juce::String licenseKey;
        };

        juce::File userExpansionRoot;
        juce::File bundledExpansionRoot;
        std::vector<InstalledExpansion> installed;

        static juce::String manifestFileName() { return "manifest.json"; }
        static juce::String stateFileName() { return "pcexp-state.json"; }
        static juce::String installFolderNameFor (const Manifest&);

        static juce::StringArray arrayFromVar (const juce::var&);
        static juce::var arrayToVar (const juce::StringArray&);
        static juce::Result copyDirectoryRecursively (const juce::File& source,
                                                      const juce::File& destination);
        static bool zipEntryPathIsSafe (const juce::String& path);
        static juce::Result validateZipEntryPaths (juce::ZipFile&);

        PackageValidation validateDirectoryPackage (const juce::File& package) const;
        PackageValidation validateArchivePackage (const juce::File& package) const;
        PackageValidation validateManifestObject (const juce::var& manifestVar,
                                                  bool isDirectory,
                                                  bool isArchive) const;

        void scanRoot (const juce::File& root, bool bundled);
        void upsertInstalled (InstalledExpansion expansion);
        static PersistentState readState (const juce::File& installRoot);
        static juce::Result writeState (const juce::File& installRoot, const PersistentState& state);
        static LicenseState evaluateLicense (const Manifest&, const juce::String& licenseKey);
    };

} // namespace patchcraft
