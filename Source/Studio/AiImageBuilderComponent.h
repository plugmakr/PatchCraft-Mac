#pragma once

#include <JuceHeader.h>
#include "AiImageService.h"

namespace patchcraft
{
    class StudioMainComponent;

    class AiImageBuilderComponent : public juce::Component
    {
    public:
        AiImageBuilderComponent (StudioMainComponent& owner);
        ~AiImageBuilderComponent() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        
        void addGeneratedAssetToLibrary();

    private:
        void triggerGeneration();
        void generationFinished();

        StudioMainComponent& owner;

        juce::Label promptLabel { {}, "Prompt:" };
        juce::TextEditor promptBox;
        juce::TextButton generateButton { "Generate Asset" };
        juce::Label statusLabel;
        juce::ImageComponent previewImage;

        juce::File lastGeneratedFile;
        AiImageService::Result lastResult;
        bool isGenerating = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiImageBuilderComponent)
    };

} // namespace patchcraft
