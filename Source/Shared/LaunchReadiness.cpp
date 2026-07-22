#include "LaunchReadiness.h"

#include "AiAssistService.h"
#include "PluginClubPublisher.h"
#include "SampleMap.h"

namespace patchcraft
{
    namespace
    {
        static void addBlocking (LaunchReadiness::Report& report, const juce::String& title, const juce::String& detail)
        {
            ++report.errorCount;
            report.blockingErrors.add (title + ": " + detail);
        }

        static bool isDrumProject (const PatchCraftProject& project)
        {
            if (project.getEngineType().equalsIgnoreCase ("drum"))
                return true;

            for (const auto& element : project.getLayout().getAll())
                if (element.type == ElementType::PadGrid || element.type == ElementType::DrumGrid)
                    return true;

            for (const auto& block : project.getDspGraph().blocks)
                if (block.type.containsIgnoreCase ("drum"))
                    return true;

            return false;
        }

        static int countElementType (const PatchCraftProject& project, ElementType type)
        {
            int count = 0;
            for (const auto& element : project.getLayout().getAll())
                if (element.type == type)
                    ++count;
            return count;
        }

        static juce::File resolveAssetPath (const PatchCraftProject& project, const juce::String& path)
        {
            if (path.isEmpty())
                return {};

            juce::File file (path);
            if (juce::File::isAbsolutePath (path))
                return file;

            if (project.getProjectFolder().isDirectory())
                return project.getProjectFolder().getChildFile (path);

            return {};
        }

        static bool assetExists (const PatchCraftProject& project, const juce::String& path)
        {
            const auto file = resolveAssetPath (project, path);
            return file != juce::File() && file.existsAsFile();
        }

        static juce::File runtimeFolder()
        {
            const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            return app.isDirectory() ? app : app.getParentDirectory();
        }

        static int countRuntimeFactoryDemos()
        {
            const auto demos = runtimeFolder().getChildFile ("FactoryDemos");
            if (! demos.isDirectory())
                return 0;

            int count = 0;
            for (auto& entry : juce::RangedDirectoryIterator (demos, false, "*.patchcraft", juce::File::findDirectories))
                if (entry.getFile().getChildFile ("manifest.json").existsAsFile())
                    ++count;
            return count;
        }

        static juce::StringArray missingRuntimeDistributionItems()
        {
            constexpr int kApprovedFactoryDemoCount = 10;
            const auto appDir = runtimeFolder();
            juce::StringArray missing;

            if (! appDir.getChildFile ("PatchCraftStudio.exe").existsAsFile()
                && ! appDir.getChildFile ("PatchCraft Studio.exe").existsAsFile()
                && ! appDir.getChildFile ("PatchCraftStudio").existsAsFile())
                missing.add ("PatchCraft Studio executable");
            if (countRuntimeFactoryDemos() < kApprovedFactoryDemoCount)
                missing.add ("FactoryDemos with the approved factory demo set");
            if (! appDir.getChildFile ("Library").isDirectory())
                missing.add ("Library folder");
            if (! appDir.getChildFile ("Library").getChildFile ("Assets").isDirectory())
                missing.add ("Library/Assets");
            if (! appDir.getChildFile ("PlayerPlugins").getChildFile ("PatchCraft Player.vst3").exists())
                missing.add ("PlayerPlugins/PatchCraft Player.vst3");
            if (! appDir.getChildFile ("PlayerPlugins").getChildFile ("PatchCraft Player FX.vst3").exists())
                missing.add ("PlayerPlugins/PatchCraft Player FX.vst3");

            return missing;
        }
    }

    LaunchReadiness::Report LaunchReadiness::evaluate (const PatchCraftProject& project, Scope scope)
    {
        Report report;
        const auto& manifest = project.getManifest();
        const auto engine = project.getEngineType();

        const bool sampleBased = engine.equalsIgnoreCase ("sample") || engine.equalsIgnoreCase ("drum")
            || (isDrumProject (project)
                && ! engine.equalsIgnoreCase ("synth")
                && ! engine.equalsIgnoreCase ("fx"));
        if (sampleBased)
        {
            const auto health = SampleMap::evaluateHealth (project.getSampleMap(), project.getProjectFolder(), engine);
            if (health.blocksExport())
                addBlocking (report, "Sample map blocks export", health.exportMessage());
            else if (health.totalZones == 0)
                addBlocking (report, "No playable sample zones",
                             "Sample and drum instruments need mapped zones/pads before they can be sold or exported.");
        }

        const auto graphIssues = project.getDspGraph().validateTypedGraph (engine);
        int graphErrors = 0;
        juce::StringArray graphDetails;
        for (const auto& issue : graphIssues)
        {
            if (issue.severity == "error")
                ++graphErrors;
            if (graphDetails.size() < 4)
                graphDetails.add (issue.toString());
        }

        if (graphErrors > 0)
            addBlocking (report, "DSP graph has blocking errors", graphDetails.joinIntoString ("  |  "));

        const auto paramIssues = project.getParameters().validateReferences (project.getLayout().getAll(),
                                                                             project.getDspGraph(),
                                                                             project.getPresets());
        int paramErrors = 0;
        juce::StringArray paramDetails;
        for (const auto& issue : paramIssues)
        {
            if (issue.severity == "error")
                ++paramErrors;
            if (paramDetails.size() < 4)
                paramDetails.add (issue.toString());
        }

        int unboundControls = 0;
        for (const auto& element : project.getLayout().getAll())
        {
            if (! isRuntimeControlElement (element.type) || element.type == ElementType::Dropdown)
                continue;
            if (element.action.isNotEmpty())
                continue;
            if (element.parameterId.isEmpty())
                ++unboundControls;
        }

        if (project.getLayout().getAll().empty())
            addBlocking (report, "Missing Player UI",
                         "The exported Player needs a real customer-facing interface.");
        else if (paramErrors > 0)
            addBlocking (report, "Missing control bindings", paramDetails.joinIntoString ("  |  "));
        else if (unboundControls > 0 && project.getManifest().quickBuildMode)
            addBlocking (report, "Missing control bindings",
                         juce::String (unboundControls) + " runtime controls are not bound to parameters.");

        const int orbitElements = countElementType (project, ElementType::ArpLane);
        if (orbitElements > 0)
        {
            const bool hasMidiPlayground = std::any_of (project.getDspGraph().blocks.begin(),
                                                        project.getDspGraph().blocks.end(),
                                                        [] (const DspBlock& block)
                                                        {
                                                            return block.type.containsIgnoreCase ("midiPlayground")
                                                                || block.type.containsIgnoreCase ("phrase generator")
                                                                || block.type.containsIgnoreCase ("midi generator");
                                                        });
            if (! hasMidiPlayground)
            {
                addBlocking (report,
                             "CircleSEQ surface is not connected to a performance engine",
                             "The Design canvas has CircleSEQ/Arp Lane elements, but no MIDI Playground engine is present.");
            }
        }

        if (project.getPatches().empty() || project.getPresets().empty())
        {
            addBlocking (report,
                         "No sellable playable presets yet",
                         "A shipped product needs at least one full Patch-backed preset.");
        }

        if ((manifest.backgroundImage.isNotEmpty() && ! assetExists (project, manifest.backgroundImage))
         || (manifest.libraryThumbnail.isNotEmpty() && ! assetExists (project, manifest.libraryThumbnail))
         || (manifest.playerLogoImage.isNotEmpty() && ! assetExists (project, manifest.playerLogoImage))
         || (manifest.playerTitleBannerImage.isNotEmpty() && ! assetExists (project, manifest.playerTitleBannerImage)))
        {
            addBlocking (report,
                         "Branding references missing files",
                         "One or more artwork paths are set but the file cannot be found.");
        }

        if (manifest.licenseRequired
            && (manifest.licenseProductId.trim().isEmpty() || manifest.licenseServerUrl.trim().isEmpty()))
        {
            addBlocking (report,
                         "Licensing is enabled but incomplete",
                         "Protected instruments need a product ID and licensing server URL before publishing.");
        }

        if (scope == Scope::Publish)
        {
            const auto cloud = AiAssistService::loadCloudIntegrationConfig();
            if (cloud.pluginClubEndpoint.trim().isNotEmpty()
                && PluginClubPublisher::normaliseSellerImportEndpoint (cloud.pluginClubEndpoint).isEmpty())
            {
                addBlocking (report,
                             "Plugin.club endpoint is invalid",
                             "Use https://plugin.club/functions/v1 or https://plugin.club/functions/v1/sellerImport in Settings.");
            }
        }

        if (scope == Scope::LaunchBundle)
        {
            const auto missingDistribution = missingRuntimeDistributionItems();
            if (! missingDistribution.isEmpty())
            {
                addBlocking (report,
                             "Studio distribution package is incomplete",
                             "Missing: " + missingDistribution.joinIntoString (", "));
            }
        }

        return report;
    }
}
