#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "AiAssistService.h"
#include "PatchCraftProject.h"

namespace patchcraft
{
    class AiImageService
    {
    public:
        enum class ImageKind
        {
            Background,
            Asset,
            TemplateArtwork
        };

        struct Request
        {
            ImageKind kind = ImageKind::Background;
            juce::String prompt;
            int width = 1280;
            int height = 800;
            bool transparent = false;
            juce::File outputFile;
        };

        struct Result
        {
            bool success = false;
            bool usedFallback = false;
            juce::String message;
            juce::File outputFile;
        };

        static juce::String buildPrompt (ImageKind kind,
                                         const PatchCraftProject& project,
                                         const juce::String& userDirection);

        static Result generate (const Request& request,
                                const AiAssistService::CloudIntegrationConfig& config);
    };
}
