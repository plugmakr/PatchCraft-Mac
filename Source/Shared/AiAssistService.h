#pragma once

#include <juce_core/juce_core.h>

#include "PatchCraftProject.h"

#include <vector>

namespace patchcraft
{
    /**
        Provider-agnostic copilot shell.

        This first implementation is intentionally local and deterministic:
        it builds a project context pack, then returns preview-first guidance
        without making network calls or mutating the project.
    */
    class AiAssistService
    {
    public:
        enum class TaskType
        {
            BackgroundPrompt,
            SuggestLayout,
            SuggestControls,
            SuggestMacroAssignments,
            GeneratePresetNames,
            GenerateProductDescription,
            DesignCritique,
            SoundRecipe,
            WavetableRecipe,
            EqChain,
            ModulationPlan,
            BuildAssetGuidance,
            ExportChecklist,
            GenerateFaustDsp,
            GenerateMidiJson,
            GeneratePresetBank,
            GeneratePScript
        };

        struct ProjectContextPack
        {
            juce::String instrumentName;
            juce::String creator;
            juce::String engine;
            juce::String category;
            juce::String description;
            juce::String canvasSummary;
            juce::String layoutSummary;
            juce::String parameterSummary;
            juce::String dspSummary;
            juce::String sampleSummary;
            juce::String contentSummary;
            juce::String validationSummary;
            juce::String userRequest;

            juce::String toSummaryText() const;
        };

        struct Suggestion
        {
            TaskType task = TaskType::SuggestLayout;
            juce::String title;
            juce::String summary;
            juce::String contextSummary;
            juce::String details;
            bool applyable = false;
        };

        enum class ProviderMode
        {
            BuiltInTemplates,
            LocalLlamaServer
        };

        enum class ImageProviderMode
        {
            BuiltInRenderer,
            OpenAIImages
        };

        enum class TextProviderMode
        {
            BuiltInTemplates,
            LocalLlamaServer,
            CloudOpenAICompatible
        };

        struct LocalLlmConfig
        {
            ProviderMode provider = ProviderMode::BuiltInTemplates;
            juce::String endpoint { "http://127.0.0.1:8080/v1/chat/completions" };
            juce::String model { "qwen3-4b-instruct-q4" };
            int timeoutMs = 4500;
            int maxTokens = 700;
            float temperature = 0.35f;
            bool includeProjectContext = true;

            juce::var toVar() const;
            static LocalLlmConfig fromVar (const juce::var&);
        };

        struct CloudIntegrationConfig
        {
            ImageProviderMode imageProvider = ImageProviderMode::BuiltInRenderer;
            juce::String imageEndpoint { "https://api.openai.com/v1/images/generations" };
            juce::String imageModel { "gpt-image-1" };
            juce::String imageApiKey;
            juce::String murekaEndpoint { "https://api.mureka.ai" };
            juce::String murekaApiKey;
            juce::String pluginClubEndpoint { "https://plugin.club/functions/v1" };
            juce::String pluginClubApiKey;          // optional static/env key
            juce::String pluginClubAccessToken;     // device-auth Bearer token (preferred for publish)
            juce::String licenseEndpoint { "https://plugin.club/functions/v1/activateLicense" };
            juce::String licensePublicKey;
            
            TextProviderMode textProvider = TextProviderMode::CloudOpenAICompatible;
            juce::String textEndpoint { "https://api.deepseek.com/v1/chat/completions" };
            juce::String textModel { "deepseek-chat" };
            juce::String textApiKey;
            
            int cloudTimeoutMs = 60000;

            juce::var toVar() const;
            static CloudIntegrationConfig fromVar (const juce::var&);
        };

        juce::String run (TaskType, const juce::String& instrumentName) const;
        juce::String runWithPrompt (TaskType, const juce::String& userPrompt) const;
        Suggestion runWithPrompt (TaskType, ProjectContextPack, const juce::String& userPrompt) const;
        Suggestion run (TaskType, const PatchCraftProject&) const;
        Suggestion run (TaskType, const ProjectContextPack&) const;
        static juce::String displayName (TaskType);
        static std::vector<TaskType> defaultTasks();

        static ProjectContextPack buildContextPack (const PatchCraftProject&);
        static LocalLlmConfig loadLocalLlmConfig();
        static void saveLocalLlmConfig (const LocalLlmConfig&);
        static juce::File localLlmConfigFile();
        static CloudIntegrationConfig loadCloudIntegrationConfig();
        static void saveCloudIntegrationConfig (const CloudIntegrationConfig&);
        static juce::File cloudIntegrationConfigFile();
        static juce::String providerModeToString (ProviderMode);
        static ProviderMode providerModeFromString (const juce::String&);
        static juce::String imageProviderModeToString (ImageProviderMode);
        static ImageProviderMode imageProviderModeFromString (const juce::String&);
        static juce::String textProviderModeToString (TextProviderMode);
        static TextProviderMode textProviderModeFromString (const juce::String&);
        static juce::String providerStatusText();
    };

} // namespace patchcraft
