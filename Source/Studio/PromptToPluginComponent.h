#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AiAssistService.h"

namespace patchcraft
{

    class PromptToPluginComponent : public juce::Component,
                                    private juce::TextEditor::Listener
    {
    public:
        PromptToPluginComponent (AiAssistService& service);
        ~PromptToPluginComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        
        // Define a callback so the parent page can handle the generated DSP
        std::function<void(const juce::String& generatedDsp)> onDspGenerated;

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void sendPrompt();
        void handleAiResponse (const juce::String& response);

        AiAssistService& aiService;

        juce::TextEditor promptEditor;
        juce::TextEditor logViewer;
        juce::TextButton generateButton { "Generate DSP" };
        juce::Label statusLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PromptToPluginComponent)
    };

} // namespace patchcraft
