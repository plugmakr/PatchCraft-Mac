#pragma once

#include <juce_core/juce_core.h>

#include "AiAssistService.h"
#include "PatchCraftProject.h"

namespace patchcraft
{
    class PluginClubPublisher
    {
    public:
        enum class ArtifactKind
        {
            PatchCraftInstrumentPack,
            StandaloneVst3Plugin,
            OneShotPack,
            LoopPack,
            ControlAssetPack,
            GenericArchive
        };

        struct PublishOptions
        {
            juce::String endpoint;
            juce::String apiKey;
            juce::File stagingRoot;
            int timeoutMs = 60000;
        };

        struct PublishArtifact
        {
            ArtifactKind kind = ArtifactKind::PatchCraftInstrumentPack;
            juce::String title;
            juce::String description;
            juce::String creator;
            juce::String category;
            juce::String version { "1.0.0" };
            juce::String status { "draft" };
            double price = 0.0;
            juce::StringArray tags;
            juce::StringArray formats;
            juce::StringArray operatingSystems;
            juce::StringArray daws;
            juce::File sourcePath;
            juce::File artworkFile;
            juce::var extraMetadata;
        };

        struct PublishResult
        {
            bool success = false;
            bool uploaded = false;
            juce::String message;
            ArtifactKind artifactKind = ArtifactKind::PatchCraftInstrumentPack;
            juce::String endpointUsed;
            int statusCode = 0;
            juce::String draftId;
            juce::String sellerProductId;
            juce::String editUrl;
            juce::String errorCode;
            juce::String serverResponse;
            juce::File packFolder;
            juce::File payloadFile;
            juce::File metadataFile;
            juce::File archiveFile;
        };

        static PublishOptions optionsFromCloudConfig (const AiAssistService::CloudIntegrationConfig&);
        static PublishResult publishDraft (const PatchCraftProject&, const PublishOptions&);
        static PublishResult publishArtifact (const PublishArtifact&, const PublishOptions&);

        static juce::String normaliseSellerImportEndpoint (const juce::String& configuredEndpoint);
        static juce::String artifactKindToString (ArtifactKind);
        static juce::String productTypeForArtifactKind (ArtifactKind);
    };
}
