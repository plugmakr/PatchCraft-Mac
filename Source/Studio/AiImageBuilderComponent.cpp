#include "AiImageBuilderComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "AiImageService.h"

namespace patchcraft
{
    AiImageBuilderComponent::AiImageBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        promptLabel.setFont (juce::FontOptions (14.0f));
        promptLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (promptLabel);

        promptBox.setMultiLine (true);
        promptBox.setReturnKeyStartsNewLine (true);
        promptBox.setFont (juce::FontOptions (15.0f));
        promptBox.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::panel());
        promptBox.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::borderSoft());
        promptBox.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::textDim());
        promptBox.setText ("A cinematic metallic knob with blue glowing led, transparent background");
        addAndMakeVisible (promptBox);

        generateButton.getProperties().set ("accent", true);
        generateButton.onClick = [this] { triggerGeneration(); };
        addAndMakeVisible (generateButton);

        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (statusLabel);

        addAndMakeVisible (previewImage);
    }

    AiImageBuilderComponent::~AiImageBuilderComponent() = default;

    void AiImageBuilderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());
        if (isGenerating)
        {
            g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.5f));
            g.drawText ("Generating Image...", getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void AiImageBuilderComponent::resized()
    {
        auto r = getLocalBounds().reduced (12);

        auto topBar = r.removeFromTop (120);
        promptLabel.setBounds (topBar.removeFromTop (24));
        
        auto btnArea = topBar.removeFromBottom (32);
        generateButton.setBounds (btnArea.removeFromLeft (140));
        statusLabel.setBounds (btnArea.withTrimmedLeft (12));

        topBar.removeFromBottom (8);
        promptBox.setBounds (topBar);

        r.removeFromTop (12);
        previewImage.setBounds (r);
    }

    void AiImageBuilderComponent::triggerGeneration()
    {
        if (isGenerating) return;

        const auto userPrompt = promptBox.getText();
        if (userPrompt.isEmpty()) return;

        isGenerating = true;
        statusLabel.setText ("Requesting Image...", juce::dontSendNotification);
        generateButton.setEnabled (false);
        previewImage.setImage (juce::Image());
        repaint();

        juce::Thread::launch ([this, userPrompt]
        {
            AiImageService::Request req;
            req.kind = AiImageService::ImageKind::Asset;
            req.prompt = userPrompt;
            req.transparent = true;
            req.width = 512;
            req.height = 512;

            const auto config = AiAssistService::loadCloudIntegrationConfig();

            lastResult = AiImageService::generate (req, config);

            juce::MessageManager::callAsync ([this]
            {
                generationFinished();
            });
        });
    }

    void AiImageBuilderComponent::generationFinished()
    {
        isGenerating = false;
        generateButton.setEnabled (true);

        if (lastResult.success && lastResult.outputFile.existsAsFile())
        {
            lastGeneratedFile = lastResult.outputFile;
            statusLabel.setText ("Generated successfully.", juce::dontSendNotification);
            previewImage.setImage (juce::ImageFileFormat::loadFrom (lastGeneratedFile));
        }
        else
        {
            statusLabel.setText ("Failed: " + lastResult.message, juce::dontSendNotification);
        }

        repaint();
    }

    void AiImageBuilderComponent::addGeneratedAssetToLibrary()
    {
        if (! lastGeneratedFile.existsAsFile()) return;

        // Copy the generated image to the user's project library/Images
        auto destDir = owner.getProject().getProjectFolder().getChildFile ("Images");
        destDir.createDirectory();
        auto destFile = destDir.getChildFile ("AI_Asset_" + juce::String (juce::Time::currentTimeMillis()) + ".png");
        lastGeneratedFile.copyFileTo (destFile);

        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, "Asset Exported", "The AI generated asset was saved to the project Images folder!");
    }

} // namespace patchcraft
