#include "PromptToPluginComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{

    PromptToPluginComponent::PromptToPluginComponent (AiAssistService& service)
        : aiService (service)
    {
        addAndMakeVisible (promptEditor);
        promptEditor.setMultiLine (true);
        promptEditor.setReturnKeyStartsNewLine (false);
        promptEditor.addListener (this);
        promptEditor.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::bg().darker (0.2f));
        promptEditor.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
        promptEditor.setTextToShowWhenEmpty ("Describe your instrument (e.g. 'A warm tape saturation with wow and flutter')...", PatchCraftLookAndFeel::textDim());

        addAndMakeVisible (logViewer);
        logViewer.setMultiLine (true);
        logViewer.setReadOnly (true);
        logViewer.setScrollbarsShown (true);
        logViewer.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::bg().darker (0.5f));
        logViewer.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        logViewer.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::textDim());

        addAndMakeVisible (generateButton);
        generateButton.onClick = [this] { sendPrompt(); };

        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centredRight);
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
    }

    PromptToPluginComponent::~PromptToPluginComponent() = default;

    void PromptToPluginComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void PromptToPluginComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (8);

        auto inputArea = bounds.removeFromBottom (80);
        auto buttonArea = inputArea.removeFromRight (120);
        
        generateButton.setBounds (buttonArea.reduced (0, 10));
        
        auto statusArea = buttonArea.withY (buttonArea.getBottom() - 10).withHeight (20);
        statusLabel.setBounds (statusArea);

        promptEditor.setBounds (inputArea.reduced (0, 10));

        bounds.removeFromBottom (8);
        logViewer.setBounds (bounds);
    }

    void PromptToPluginComponent::textEditorReturnKeyPressed (juce::TextEditor& te)
    {
        if (&te == &promptEditor)
        {
            sendPrompt();
        }
    }

    void PromptToPluginComponent::sendPrompt()
    {
        auto promptText = promptEditor.getText().trim();
        if (promptText.isEmpty())
            return;

        generateButton.setEnabled (false);
        statusLabel.setText ("Generating...", juce::dontSendNotification);
        
        logViewer.moveCaretToEnd();
        logViewer.insertTextAtCaret ("\n> " + promptText + "\n");

        promptEditor.clear();

        // Run async via a thread to avoid blocking the message thread
        juce::Thread::launch ([this, promptText, safeThis = juce::Component::SafePointer<PromptToPluginComponent>(this)] () 
        {
            juce::String response = aiService.runWithPrompt (AiAssistService::TaskType::GenerateFaustDsp, promptText);
            juce::MessageManager::callAsync ([safeThis, response] 
            {
                if (safeThis != nullptr)
                    safeThis->handleAiResponse (response);
            });
        });
    }

    void PromptToPluginComponent::handleAiResponse (const juce::String& response)
    {
        generateButton.setEnabled (true);
        statusLabel.setText ("Ready", juce::dontSendNotification);

        logViewer.moveCaretToEnd();
        
        if (response.isEmpty())
        {
            logViewer.insertTextAtCaret ("[Error: No response or connection failed]\n");
            return;
        }

        // Clean up markdown tags just in case
        juce::String cleanCode = response;
        if (cleanCode.startsWith ("```faust"))
            cleanCode = cleanCode.substring (8);
        else if (cleanCode.startsWith ("```"))
            cleanCode = cleanCode.substring (3);
            
        if (cleanCode.endsWith ("```"))
            cleanCode = cleanCode.dropLastCharacters (3);

        cleanCode = cleanCode.trim();

        logViewer.insertTextAtCaret ("[Success] Received " + juce::String (cleanCode.length()) + " bytes of DSP.\n");
        
        if (onDspGenerated)
            onDspGenerated (cleanCode);
    }

} // namespace patchcraft
