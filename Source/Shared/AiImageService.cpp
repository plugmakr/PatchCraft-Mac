#include "AiImageService.h"

#include "AssetManager.h"

namespace patchcraft
{
    namespace
    {
        static juce::String kindLabel (AiImageService::ImageKind kind)
        {
            switch (kind)
            {
                case AiImageService::ImageKind::Background:      return "plugin background";
                case AiImageService::ImageKind::Asset:           return "transparent UI asset";
                case AiImageService::ImageKind::TemplateArtwork: return "instrument template artwork";
            }
            return "plugin artwork";
        }

        static juce::String openAiSizeFor (int width, int height)
        {
            if (width > height)
                return "1536x1024";
            if (height > width)
                return "1024x1536";
            return "1024x1024";
        }

        static bool writePng (const juce::Image& image, const juce::File& file, juce::String& error)
        {
            if (! file.getParentDirectory().createDirectory())
            {
                error = "Could not create image output folder: " + file.getParentDirectory().getFullPathName();
                return false;
            }

            juce::PNGImageFormat png;
            std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());
            if (out == nullptr || ! png.writeImageToStream (image, *out))
            {
                error = "Could not write generated PNG: " + file.getFullPathName();
                return false;
            }

            return true;
        }

        static juce::Image fitImage (const juce::Image& source, int width, int height, bool transparent)
        {
            if (! source.isValid() || (source.getWidth() == width && source.getHeight() == height))
                return source;

            juce::Image target (transparent ? juce::Image::ARGB : juce::Image::RGB,
                                juce::jmax (1, width), juce::jmax (1, height), true);
            juce::Graphics g (target);
            g.fillAll (transparent ? juce::Colours::transparentBlack : juce::Colour (0xff0b0d10));
            g.drawImage (source, target.getBounds().toFloat(), juce::RectanglePlacement::fillDestination);
            return target;
        }

        static AiImageService::Result renderFallback (const AiImageService::Request& request,
                                                      const juce::String& reason)
        {
            AiImageService::Result result;
            result.usedFallback = true;
            result.outputFile = request.outputFile;

            juce::Image image;
            if (request.kind == AiImageService::ImageKind::Background
                || request.kind == AiImageService::ImageKind::TemplateArtwork)
            {
                image = AssetManager::renderDefaultHeroImage (juce::jmax (1, request.width),
                                                              juce::jmax (1, request.height));
            }
            else
            {
                image = juce::Image (juce::Image::ARGB, juce::jmax (1, request.width),
                                     juce::jmax (1, request.height), true);
                juce::Graphics g (image);
                auto area = image.getBounds().toFloat().reduced ((float) image.getWidth() * 0.16f,
                                                                 (float) image.getHeight() * 0.16f);
                g.setColour (juce::Colour (0xccf5a623));
                g.fillRoundedRectangle (area, 28.0f);
                g.setColour (juce::Colour (0xffffffff).withAlpha (0.22f));
                g.drawRoundedRectangle (area.reduced (3.0f), 28.0f, 3.0f);
            }

            juce::String error;
            result.success = writePng (image, request.outputFile, error);
            result.message = result.success
                ? "Generated local fallback " + kindLabel (request.kind) + ". " + reason
                : error;
            return result;
        }

        static juce::String extractImagePayload (const juce::var& parsed, bool& isUrl)
        {
            isUrl = false;
            if (auto* root = parsed.getDynamicObject())
            {
                if (auto* data = root->getProperty ("data").getArray())
                {
                    if (data->isEmpty())
                        return {};

                    if (auto* first = data->getReference (0).getDynamicObject())
                    {
                        const auto b64 = first->getProperty ("b64_json").toString();
                        if (b64.isNotEmpty())
                            return b64;

                        const auto url = first->getProperty ("url").toString();
                        if (url.isNotEmpty())
                        {
                            isUrl = true;
                            return url;
                        }
                    }
                }
            }
            return {};
        }

        static AiImageService::Result generateOpenAi (const AiImageService::Request& request,
                                                      const AiAssistService::CloudIntegrationConfig& config)
        {
            AiImageService::Result result;
            result.outputFile = request.outputFile;

            if (config.imageApiKey.trim().isEmpty())
                return renderFallback (request, "OpenAI image API key is not configured.");

            auto* root = new juce::DynamicObject();
            root->setProperty ("model", config.imageModel.trim().isNotEmpty()
                                        ? config.imageModel.trim() : juce::String ("gpt-image-1"));
            root->setProperty ("prompt", request.prompt);
            root->setProperty ("size", openAiSizeFor (request.width, request.height));
            if (request.transparent)
                root->setProperty ("background", "transparent");

            const auto body = juce::JSON::toString (juce::var (root), false);
            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto stream = juce::URL (config.imageEndpoint.trim().isNotEmpty()
                                     ? config.imageEndpoint.trim()
                                     : juce::String ("https://api.openai.com/v1/images/generations"))
                .withPOSTData (body)
                .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (juce::jlimit (3000, 120000, config.cloudTimeoutMs))
                    .withExtraHeaders ("Content-Type: application/json\r\nAuthorization: Bearer "
                                       + config.imageApiKey.trim() + "\r\n")
                    .withResponseHeaders (&responseHeaders)
                    .withStatusCode (&statusCode)
                    .withHttpRequestCmd ("POST")
                    .withNumRedirectsToFollow (2));

            if (stream == nullptr)
                return renderFallback (request, "Could not connect to the image provider.");

            const auto response = stream->readEntireStreamAsString();
            if (statusCode < 200 || statusCode >= 300)
                return renderFallback (request, "Image provider returned HTTP " + juce::String (statusCode)
                                                + ": " + response.substring (0, 220));

            bool payloadIsUrl = false;
            const auto payload = extractImagePayload (juce::JSON::parse (response), payloadIsUrl);
            if (payload.isEmpty())
                return renderFallback (request, "Image provider response did not contain image data.");

            juce::MemoryBlock imageBytes;
            if (payloadIsUrl)
            {
                auto imageStream = juce::URL (payload)
                    .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (juce::jlimit (3000, 120000, config.cloudTimeoutMs))
                        .withNumRedirectsToFollow (3));
                if (imageStream == nullptr)
                    return renderFallback (request, "Image provider returned a URL that could not be downloaded.");
                imageStream->readIntoMemoryBlock (imageBytes);
            }
            else
            {
                juce::MemoryOutputStream decoded (imageBytes, false);
                if (! juce::Base64::convertFromBase64 (decoded, payload))
                    return renderFallback (request, "Image provider returned invalid base64 image data.");
            }

            juce::MemoryInputStream input (imageBytes, false);
            auto image = juce::ImageFileFormat::loadFrom (input);
            if (! image.isValid())
                return renderFallback (request, "Decoded image data was not a readable bitmap.");

            juce::String error;
            result.success = writePng (fitImage (image, request.width, request.height, request.transparent),
                                       request.outputFile, error);
            result.message = result.success ? "Generated " + kindLabel (request.kind) + "." : error;
            return result;
        }
    }

    juce::String AiImageService::buildPrompt (ImageKind kind,
                                              const PatchCraftProject& project,
                                              const juce::String& userDirection)
    {
        const auto& manifest = project.getManifest();
        juce::String prompt;
        prompt << "Create a premium " << kindLabel (kind) << " for a music software instrument named '"
               << (manifest.instrumentName.isNotEmpty() ? manifest.instrumentName : juce::String ("Untitled Instrument"))
               << "'. Engine: " << project.getEngineType() << ". Category: " << manifest.category << ". ";

        if (kind == ImageKind::Background)
        {
            prompt << "Use a refined dark studio-grade visual style, high contrast, warm amber accent energy, "
                   << "clear section zones for controls, and enough empty space for knobs, labels, tabs, meters, and preset browsing. "
                   << "Do not include text, logos, knobs, sliders, meters, buttons, UI labels, or brand names. ";
        }
        else if (kind == ImageKind::Asset)
        {
            prompt << "Create a single reusable transparent-background visual asset suitable for a plugin GUI. "
                   << "No text, no copyrighted marks, no baked-in controls unless explicitly requested. ";
        }
        else
        {
            prompt << "Create full template artwork with visually distinct panels/sections but no text, no knobs, no sliders, and no logos. ";
        }

        if (userDirection.trim().isNotEmpty())
            prompt << "Developer direction: " << userDirection.trim();

        return prompt.trim();
    }

    AiImageService::Result AiImageService::generate (const Request& request,
                                                     const AiAssistService::CloudIntegrationConfig& config)
    {
        if (request.outputFile == juce::File())
        {
            Result result;
            result.message = "No output file was provided for image generation.";
            return result;
        }

        if (config.imageProvider == AiAssistService::ImageProviderMode::OpenAIImages)
            return generateOpenAi (request, config);

        return renderFallback (request, "Cloud image generation is not enabled.");
    }
}
