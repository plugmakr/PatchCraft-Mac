// VST3 export smoke test.
//
// Builds a default PatchCraftProject in-process, asks VstExportModule to
// stamp out a renamed .vst3 bundle, and verifies the resulting bundle has
// the structure a host expects (Contents/<arch>/<name>.vst3 binary,
// Contents/Resources/EmbeddedPack/manifest.json, etc).
//
// The test points the exporter at the just-built PatchCraft Player VST3 via
// the PATCHCRAFT_PLAYER_TEMPLATE environment variable so it doesn't rely on
// the Studio's runtime PluginTemplate/ staging directory.
//
// Exit code 0 = pass, 1 = fail. Diagnostics go to stdout.

#include "PatchCraftProject.h"
#include "VstExportModule.h"

#include <juce_core/juce_core.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void setEnv (const char* name, const juce::String& value)
    {
       #if JUCE_WINDOWS
        _putenv_s (name, value.toRawUTF8());
       #else
        setenv (name, value.toRawUTF8(), 1);
       #endif
    }
}

namespace
{
    int failures = 0;

    void check (bool condition, const std::string& message)
    {
        if (! condition)
        {
            std::cout << "  FAIL: " << message << std::endl;
            ++failures;
        }
        else
        {
            std::cout << "  ok:   " << message << std::endl;
        }
    }

    juce::File pickArchSubfolder (const juce::File& contents)
    {
        // VST3 bundle architecture sub-folder varies by platform. We accept
        // whatever sub-folder is present so the test doesn't have to know
        // the host's architecture string.
        if (! contents.isDirectory())
            return {};
        for (auto& entry : juce::RangedDirectoryIterator (contents, false, "*",
                              juce::File::findDirectories))
        {
            const auto name = entry.getFile().getFileName();
            if (name == "x86_64-win" || name == "x86-win"
                || name == "arm64-win" || name == "arm64ec-win"
                || name.endsWith ("-linux") || name == "MacOS")
                return entry.getFile();
        }
        return {};
    }

    juce::String locateTemplate (const char* envVar,
                                 const juce::String& artefactFolder,
                                 const juce::String& bundleName)
    {
        auto templatePath = juce::SystemStats::getEnvironmentVariable (envVar, {});
        if (templatePath.isEmpty())
        {
            const auto self  = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const auto guess = self.getParentDirectory().getParentDirectory().getParentDirectory()
                                   .getChildFile (artefactFolder)
                                   .getChildFile (self.getParentDirectory().getFileName())
                                   .getChildFile ("VST3")
                                   .getChildFile (bundleName);
            if (guess.exists())
                templatePath = guess.getFullPathName();
        }
        return templatePath;
    }

    void stripProjectForExportSmoke (patchcraft::PatchCraftProject& project)
    {
        project.getLayout().clear();
        project.getPatches().clear();
        project.getExpansions().clear();
        project.getPresets().clear();
        project.getManifest().backgroundImage.clear();
        project.getManifest().libraryThumbnail.clear();
        project.getManifest().playerLogoImage.clear();
        project.backgroundImageRelative.clear();
        project.getDspGraph().resetForEngine (project.getManifest().engine);
    }

    void checkModuleInfoMetadata (const juce::File& resources,
                                  const patchcraft::VstExportModule::ExportOptions& options,
                                  const juce::String& oldName,
                                  const juce::String& originalCidTail,
                                  const juce::String& expectedCategory)
    {
        const auto moduleInfo = resources.getChildFile ("moduleinfo.json");
        check (moduleInfo.existsAsFile(), "moduleinfo.json was written");
        if (! moduleInfo.existsAsFile())
            return;

        const auto text = moduleInfo.loadFileAsString();
        check (text.contains ("\"Name\": \"" + options.pluginName + "\""),
               "moduleinfo.json reports the exported plugin name");
        check (! text.contains ("\"Name\": \"" + oldName + "\""),
               "moduleinfo.json no longer reports the template plugin name");
        check (text.contains ("\"Vendor\": \"" + options.manufacturerName + "\""),
               "moduleinfo.json reports the exported manufacturer");
        check (text.contains ("\"Version\": \"" + options.version + "\""),
               "moduleinfo.json reports the exported version");
        check (! text.contains (originalCidTail),
               "moduleinfo.json no longer contains the template class ID tail");
        check (expectedCategory.isEmpty() || text.contains ("\"" + expectedCategory + "\""),
               "moduleinfo.json keeps the expected VST3 category");
    }
}

int main (int argc, char** argv)
{
    juce::ignoreUnused (argc, argv);

    std::cout << "PatchCraft VST3 export smoke test" << std::endl;

    // ------------------------------------------------------------------
    // Locate the Player VST3 template. CMake passes the artefact dir via
    // the PATCHCRAFT_PLAYER_TEMPLATE env var; if it's unset we fall back
    // to the standard build-tree layout next to the test exe.
    // ------------------------------------------------------------------
    auto templateEnv = locateTemplate ("PATCHCRAFT_PLAYER_TEMPLATE",
                                       "PatchCraftPlayer_artefacts",
                                       "PatchCraft Player.vst3");

    if (templateEnv.isEmpty() || ! juce::File (templateEnv).exists())
    {
        std::cout << "  SKIP: PATCHCRAFT_PLAYER_TEMPLATE not set and no template "
                     "could be auto-located; build PatchCraftPlayer_VST3 first."
                  << std::endl;
        return 0; // skip = pass; the build target normally produces it.
    }

    setEnv ("PATCHCRAFT_PLAYER_TEMPLATE", templateEnv);
    std::cout << "  template: " << templateEnv.toStdString() << std::endl;

    const auto fxTemplateEnv = locateTemplate ("PATCHCRAFT_PLAYER_FX_TEMPLATE",
                                               "PatchCraftPlayerFX_artefacts",
                                               "PatchCraft Player FX.vst3");
    if (fxTemplateEnv.isNotEmpty() && juce::File (fxTemplateEnv).exists())
    {
        setEnv ("PATCHCRAFT_PLAYER_FX_TEMPLATE", fxTemplateEnv);
        std::cout << "  fx template: " << fxTemplateEnv.toStdString() << std::endl;
    }

    const auto composerTemplateEnv = locateTemplate ("PATCHCRAFT_COMPOSER_TEMPLATE",
                                                     "PatchCraftComposer_artefacts",
                                                     "PatchCraft Composer.vst3");
    if (composerTemplateEnv.isNotEmpty() && juce::File (composerTemplateEnv).exists())
    {
        setEnv ("PATCHCRAFT_COMPOSER_TEMPLATE", composerTemplateEnv);
        std::cout << "  composer template: " << composerTemplateEnv.toStdString() << std::endl;
    }

    // ------------------------------------------------------------------
    // Build a project with just enough state to exercise the export path
    // (manifest + a minimum layout). The default demo pack pulls in a lot
    // of parameter references that touch unrelated pre-existing validation
    // issues; this smoke test is focused on the VST3 bundle stamping, not
    // on the PackWriter's full validation surface.
    // ------------------------------------------------------------------
    patchcraft::PatchCraftProject project;
    project.getManifest().instrumentName = "Smoke Test Synth";
    project.getManifest().creator        = "PatchCraft QA";
    stripProjectForExportSmoke (project);
    project.getManifest().playerDisplayName = "Smoke Test Player";
    project.getManifest().playerClientName = "QA Client";
    project.getManifest().whiteLabelPackageName = "QA Smoke Label";
    project.getManifest().whiteLabelPublisher = "PatchCraft QA";
    project.getManifest().whiteLabelProductCode = "QA-SMOKE-001";
    project.getManifest().licenseRequired = true;
    project.getManifest().licenseProductId = "qa-smoke-synth";
    project.getManifest().licenseServerUrl = "https://plugin.club/functions/deviceAuth";
    project.getManifest().playerSupportUrl = "https://plugin.club/support";

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("PatchCraftVstExportSmoke");
    tempRoot.deleteRecursively();
    tempRoot.createDirectory();

    patchcraft::VstExportModule::ExportOptions options;
    options.pluginName          = "Smoke Test Plugin";
    options.fileSafeName        = "Smoke_Test_Plugin";
    options.manufacturerName    = "PatchCraft QA";
    options.version             = "0.0.1";
    options.outputFolder        = tempRoot.getChildFile ("out");
    options.installToSystemVst3 = false;  // never touch the user's plug-in folder

    std::cout << "  output:   " << options.outputFolder.getFullPathName().toStdString()
              << std::endl;

    const auto result = patchcraft::VstExportModule::exportPlugin (project, options);

    check (result.success, "exportPlugin reports success");
    if (! result.success)
        std::cout << "    message: " << result.message.toStdString() << std::endl;

    // ------------------------------------------------------------------
    // Inspect the produced bundle. Expect:
    //   Smoke_Test_Plugin.vst3/
    //     Contents/
    //       <arch>/Smoke_Test_Plugin.vst3      (the DLL itself)
    //       Resources/moduleinfo.json
    //       Resources/EmbeddedPack/manifest.json
    //       Resources/export_info.json
    // ------------------------------------------------------------------
    const auto bundle   = result.bundlePath;
    const auto contents = bundle.getChildFile ("Contents");
    check (bundle.exists(),        "bundle directory exists");
    check (contents.isDirectory(), "Contents/ exists");

    const auto arch = pickArchSubfolder (contents);
    check (arch != juce::File(), "Contents/<arch>/ exists");
    if (arch != juce::File())
    {
       #if JUCE_MAC
        const auto dll = arch.getChildFile ("Smoke_Test_Plugin");
        check (dll.existsAsFile(), "Contents/<arch>/Smoke_Test_Plugin binary exists");
       #else
        const auto dll = arch.getChildFile ("Smoke_Test_Plugin.vst3");
        check (dll.existsAsFile(), "Contents/<arch>/Smoke_Test_Plugin.vst3 binary exists");
       #endif
        check (dll.getSize() > 100 * 1024, "binary is at least 100 KB (template was copied)");
    }

    const auto resources = contents.getChildFile ("Resources");
    check (resources.isDirectory(), "Contents/Resources/ exists");

    const auto embedded = resources.getChildFile ("EmbeddedPack");
    check (embedded.isDirectory(), "Contents/Resources/EmbeddedPack/ exists");
    const auto embeddedManifestFile = embedded.getChildFile ("manifest.json");
    check (embeddedManifestFile.existsAsFile(), "EmbeddedPack/manifest.json was written");
    if (embeddedManifestFile.existsAsFile())
    {
        const auto parsed = juce::JSON::parse (embeddedManifestFile);
        if (auto* obj = parsed.getDynamicObject())
        {
            check (! (bool) obj->getProperty ("playerShowPackMenu"),
                   "embedded standalone pack hides the generic pack menu");
            check (! (bool) obj->getProperty ("playerAllowPackLoading"),
                   "embedded standalone pack disables external pack loading");
            check (! (bool) obj->getProperty ("playerShowLibraryBrowser"),
                   "embedded standalone pack disables the library browser");
        }
        else
        {
            check (false, "EmbeddedPack/manifest.json is valid JSON");
        }
    }

    const auto exportInfo = resources.getChildFile ("export_info.json");
    check (exportInfo.existsAsFile(), "export_info.json was written");
    if (exportInfo.existsAsFile())
    {
        const auto parsed = juce::JSON::parse (exportInfo);
        if (auto* obj = parsed.getDynamicObject())
        {
            check (obj->getProperty ("pluginName").toString() == options.pluginName,
                   "export_info.json reports the right plugin name");
        }
        else
        {
            check (false, "export_info.json is valid JSON");
        }
    }

    const auto whiteLabelInfo = resources.getChildFile ("white_label_product.json");
    check (whiteLabelInfo.existsAsFile(), "white_label_product.json was written");
    if (whiteLabelInfo.existsAsFile())
    {
        const auto parsed = juce::JSON::parse (whiteLabelInfo);
        if (auto* obj = parsed.getDynamicObject())
        {
            check (obj->getProperty ("product_name").toString() == "QA Smoke Label",
                   "white_label_product.json reports the branded package name");
            check (obj->getProperty ("publisher").toString() == "PatchCraft QA",
                   "white_label_product.json reports the publisher");
            check ((bool) obj->getProperty ("license_required"),
                   "white_label_product.json preserves license requirement");
            check (obj->getProperty ("support_url").toString() == "https://plugin.club/support",
                   "white_label_product.json preserves support URL");
        }
        else
        {
            check (false, "white_label_product.json is valid JSON");
        }
    }

    checkModuleInfoMetadata (resources, options, "PatchCraft Player",
                             "506372665063706C", "Instrument");

    if (result.success)
    {
        const auto replaceResult = patchcraft::VstExportModule::exportPlugin (project, options);
        check (replaceResult.success, "re-export over an existing VST3 bundle succeeds");
        if (! replaceResult.success)
            std::cout << "    message: " << replaceResult.message.toStdString() << std::endl;
        if (replaceResult.success)
        {
            check (replaceResult.bundlePath.exists(), "re-exported bundle still exists");
            check (replaceResult.bundlePath.getFileName() == "Smoke_Test_Plugin.vst3",
                   "re-export keeps the expected bundle name when the old bundle is replaceable");
        }
    }

    // Real authoring projects can contain stale DSP-builder graph edges or
    // older template controls whose IDs imply performance parameters but do
    // not have parameterId set. Export should repair the pack copy rather
    // than blocking the VST build.
    {
        patchcraft::PatchCraftProject dirtyProject;
        dirtyProject.getManifest().instrumentName = "Stale Graph Synth";
        dirtyProject.getManifest().creator        = "PatchCraft QA";
        stripProjectForExportSmoke (dirtyProject);
        dirtyProject.getParameters().remove ("oscBlend");
        dirtyProject.getParameters().remove ("modWheel");
        dirtyProject.getParameters().remove ("expression");

        patchcraft::LayoutElement expression;
        expression.type = patchcraft::ElementType::Slider;
        expression.id = "expression";
        expression.label = "Expr";
        dirtyProject.getLayout().add (expression);

        patchcraft::LayoutElement modwheel;
        modwheel.type = patchcraft::ElementType::Slider;
        modwheel.id = "modwheel";
        modwheel.label = "Mod";
        dirtyProject.getLayout().add (modwheel);

        patchcraft::DspBlock staleSource;
        staleSource.id = "source_12";
        staleSource.section = "source";
        staleSource.type = "oscillator";
        staleSource.name = "Legacy Source";
        staleSource.targetId = "oscBlend";
        staleSource.values["volume"] = 0.7f;
        dirtyProject.getDspGraph().blocks.push_back (staleSource);

        patchcraft::DspGraphEdge staleEdge;
        staleEdge.id = "delay_1_to_missing_reverb";
        staleEdge.sourceNodeId = "delay_1";
        staleEdge.targetNodeId = "missing_reverb";
        dirtyProject.getDspGraph().edges.push_back (staleEdge);

        patchcraft::VstExportModule::ExportOptions dirtyOptions = options;
        dirtyOptions.pluginName   = "Stale Graph Export";
        dirtyOptions.fileSafeName = "Stale_Graph_Export";
        dirtyOptions.outputFolder = tempRoot.getChildFile ("stale-out");
        dirtyOptions.installToSystemVst3 = false;

        const auto dirtyResult = patchcraft::VstExportModule::exportPlugin (dirtyProject, dirtyOptions);
        check (dirtyResult.success, "export sanitizes stale graph and legacy performance controls");
        if (! dirtyResult.success)
            std::cout << "    message: " << dirtyResult.message.toStdString() << std::endl;
        if (dirtyResult.success)
        {
            const auto dirtyParams = juce::JSON::parse (dirtyResult.bundlePath
                .getChildFile ("Contents")
                .getChildFile ("Resources")
                .getChildFile ("EmbeddedPack")
                .getChildFile ("parameters.json"));
            const auto dirtyParamText = juce::JSON::toString (dirtyParams, false);
            check (dirtyParamText.contains ("\"oscBlend\"")
                   && dirtyParamText.contains ("\"modWheel\"")
                   && dirtyParamText.contains ("\"expression\""),
                   "sanitized embedded pack preserves repaired parameters");
        }
    }

    // The license workflow is independent of the export and should be a
    // pure check of the validator; cover it here so a regression breaks
    // the build rather than the user's first export attempt.
    check (patchcraft::VstExportModule::validateLicenseKey ("PATCHCRAFT55"),
           "license validator accepts the trial key");
    check (! patchcraft::VstExportModule::validateLicenseKey ("short"),
           "license validator rejects sub-8-char keys");

    // Two exports with different plugin names must produce different
    // binaries: the unique-class-ID step rewrites bytes in the .vst3 DLL,
    // so a byte-for-byte comparison is a cheap proxy for "the host will
    // see them as distinct plugins".
    if (result.success)
    {
        patchcraft::VstExportModule::ExportOptions second = options;
        second.pluginName   = "Smoke Test Plugin Two";
        second.fileSafeName = "Smoke_Test_Plugin_Two";
        second.outputFolder = tempRoot.getChildFile ("out2");
        const auto r2 = patchcraft::VstExportModule::exportPlugin (project, second);
        check (r2.success, "second export with a different name succeeds");
        if (r2.success)
        {
            // Locate both DLLs and compare contents.
            const auto firstArch  = pickArchSubfolder (result.bundlePath.getChildFile ("Contents"));
            const auto secondArch = pickArchSubfolder (r2.bundlePath.getChildFile ("Contents"));
            if (firstArch != juce::File() && secondArch != juce::File())
            {
                juce::MemoryBlock a, b;
               #if JUCE_MAC
                firstArch.getChildFile ("Smoke_Test_Plugin").loadFileAsData (a);
                secondArch.getChildFile ("Smoke_Test_Plugin_Two").loadFileAsData (b);
               #else
                firstArch.getChildFile ("Smoke_Test_Plugin.vst3").loadFileAsData (a);
                secondArch.getChildFile ("Smoke_Test_Plugin_Two.vst3").loadFileAsData (b);
               #endif
                check (a.getSize() == b.getSize(),
                       "the two exports have the same binary size (template-copy reused)");
                check (a != b,
                       "the two exports have different binaries (class-ID patching diverged them)");
            }
        }
    }

    if (fxTemplateEnv.isNotEmpty() && juce::File (fxTemplateEnv).exists())
    {
        patchcraft::PatchCraftProject fxProject;
        fxProject.setEngineType ("fx");
        fxProject.getManifest().instrumentName = "Smoke Test FX";
        fxProject.getManifest().creator        = "PatchCraft QA";
        stripProjectForExportSmoke (fxProject);

        patchcraft::VstExportModule::ExportOptions fxOptions;
        fxOptions.pluginName          = "Smoke Test FX";
        fxOptions.fileSafeName        = "Smoke_Test_FX";
        fxOptions.manufacturerName    = "PatchCraft QA";
        fxOptions.version             = "0.0.2";
        fxOptions.outputFolder        = tempRoot.getChildFile ("fx-out");
        fxOptions.installToSystemVst3 = false;

        const auto fxResult = patchcraft::VstExportModule::exportPlugin (fxProject, fxOptions);
        check (fxResult.success, "FX export reports success");
        if (! fxResult.success)
            std::cout << "    message: " << fxResult.message.toStdString() << std::endl;

        const auto fxContents = fxResult.bundlePath.getChildFile ("Contents");
        const auto fxArch = pickArchSubfolder (fxContents);
        check (fxArch != juce::File(), "FX export Contents/<arch>/ exists");
        if (fxArch != juce::File())
        {
           #if JUCE_MAC
            check (fxArch.getChildFile ("Smoke_Test_FX").existsAsFile(),
                   "FX export renamed the inner VST3 binary");
           #else
            check (fxArch.getChildFile ("Smoke_Test_FX.vst3").existsAsFile(),
                   "FX export renamed the inner VST3 binary");
           #endif
        }

        const auto fxResources = fxContents.getChildFile ("Resources");
        checkModuleInfoMetadata (fxResources, fxOptions, "PatchCraft Player FX",
                                 "5063726650636678", "Fx");
    }

    if (composerTemplateEnv.isNotEmpty() && juce::File (composerTemplateEnv).exists())
    {
        patchcraft::PatchCraftProject composerProject;
        composerProject.getManifest().instrumentName = "Smoke Test Composer";
        composerProject.getManifest().creator = "PatchCraft QA";
        stripProjectForExportSmoke (composerProject);

        patchcraft::DspBlock composer;
        composer.id = "smoke_harmony_composer";
        composer.section = "mod";
        composer.type = "harmonyComposer";
        composer.name = "Harmony Composer";
        composer.targetId = "composerRoot";
        composer.enabled = true;
        composer.values["composerRoot"] = 0.0f;
        composer.values["composerScale"] = 1.0f;
        composer.values["composerChordCount"] = 4.0f;
        composerProject.getDspGraph().blocks.push_back (composer);

        patchcraft::VstExportModule::ExportOptions composerOptions;
        composerOptions.pluginName = "Smoke Test Composer";
        composerOptions.fileSafeName = "Smoke_Test_Composer";
        composerOptions.manufacturerName = "PatchCraft QA";
        composerOptions.version = "0.0.3";
        composerOptions.outputFolder = tempRoot.getChildFile ("composer-out");
        composerOptions.exportAsMidiEffect = true;

        const auto composerResult = patchcraft::VstExportModule::exportPlugin (composerProject, composerOptions);
        check (composerResult.success, "Composer MIDI export reports success");
        if (! composerResult.success)
            std::cout << "    message: " << composerResult.message.toStdString() << std::endl;

        const auto composerContents = composerResult.bundlePath.getChildFile ("Contents");
        const auto composerArch = pickArchSubfolder (composerContents);
        check (composerArch != juce::File(), "Composer export Contents/<arch>/ exists");
        if (composerArch != juce::File())
        {
           #if JUCE_MAC
            check (composerArch.getChildFile ("Smoke_Test_Composer").existsAsFile(),
                   "Composer export renamed the inner VST3 binary");
           #else
            check (composerArch.getChildFile ("Smoke_Test_Composer.vst3").existsAsFile(),
                   "Composer export renamed the inner VST3 binary");
           #endif
        }

        checkModuleInfoMetadata (composerContents.getChildFile ("Resources"), composerOptions,
                                 "PatchCraft Composer", "5063726650636D70", "Fx");
    }

    if (failures == 0)
    {
        std::cout << "PatchCraft VST3 export smoke test passed." << std::endl;
        return 0;
    }

    std::cout << "PatchCraft VST3 export smoke test FAILED with "
              << failures << " issue(s)." << std::endl;
    return 1;
}
