#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"

namespace patchcraft
{
    /**
        Studio addon: exports the current PatchCraft project as a
        standalone, installable VST3 plugin.

        How it works
        ------------
        The export is a "stamp" of the shipped PatchCraft Player VST3
        template. Synth/sample/multi projects use the instrument template;
        FX projects use the Player FX template. We copy the bundle, rename it to the user's plugin name,
        embed the user's .patchcraft pack into the bundle's
        Contents/Resources/EmbeddedPack/ folder, and (optionally) install
        the bundle to the system VST3 directory so any DAW picks it up
        on the next plugin scan.

        The Player plugin, when loaded from a host, looks for an
        EmbeddedPack folder inside its own bundle (see Player/PluginProcessor.cpp,
        findEmbeddedPackFolder) and loads that pack instead of the demo.

        Licensing
        ---------
        The base Studio installer does not include PluginTemplate/ and will
        report this module as locked. The separate PatchCraft VST Expansion
        installer adds the templates and can be license-gated by Plugin.club
        or another entitlement service.
    */
    class VstExportModule
    {
    public:
        struct ExportOptions
        {
            juce::String pluginName;            // "My Synth"
            juce::String fileSafeName;          // "My_Synth"  (no spaces, slashes)
            juce::String manufacturerName;      // "Acme Audio"
            juce::String version       { "1.0.0" };
            juce::File   outputFolder;          // where the .vst3 bundle is written
            bool         installToSystemVst3 = false;
            bool         exportAsMidiEffect = false;
        };

        struct ExportResult
        {
            bool         success = false;
            juce::String message;               // human-readable summary
            juce::File   bundlePath;            // the .vst3 bundle that was written
            juce::File   installedPath;         // system VST3 dir copy, if any
        };

        // Entry point. Async; safe to call from any UI handler. The
        // dialog handles licensing, options, the export, the system
        // install, and the final "Exported to..." confirmation.
        static void showExportDialog (juce::Component* parent,
                                      const PatchCraftProject& project);

        // Lower-level: do the export without UI. Returns immediately.
        static ExportResult exportPlugin (const PatchCraftProject& project,
                                          const ExportOptions& options);

        // License management ------------------------------------------------
        static bool isModuleActivated();
        static bool validateLicenseKey (const juce::String& key);
        static bool storeLicenseKey (const juce::String& key);
        static juce::String getMachineId();
        static bool isVstExpansionInstalled();
        static juce::String vstExpansionInstallMessage();

        // Returns the directory hosts scan for VST3 plugins on this OS,
        // or an empty File if it cannot be determined.
        static juce::File getSystemVst3Folder();

        // Locates the PatchCraft Player VST3 bundle that the exporter
        // stamps. Searches: next to the Studio executable
        // (PluginTemplate/), the system VST3 dir, and a few build-tree
        // fallbacks for development runs. Empty File if not found.
        static juce::File findPlayerTemplateBundle();
    };

} // namespace patchcraft
