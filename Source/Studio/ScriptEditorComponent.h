#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/PatchCraftProject.h"
#include "../Shared/LiveValueStore.h"
#include "../Shared/AiAssistService.h"
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace patchcraft
{
    class StudioMainComponent;

    /** Docked pScript workspace: code editor + console. Live preview lives in the center canvas. */
    class ScriptEditorComponent : public juce::Component,
                                  public PatchCraftProject::Listener,
                                  public LiveValueStore::Listener,
                                  private juce::Timer
    {
    public:
        std::function<void()> onPopOut;
        std::function<void()> onClose;

        explicit ScriptEditorComponent (StudioMainComponent& owner);
        ~ScriptEditorComponent() override;

        void refresh();
        void insertSnippetAndCompile (const juce::String& snippet);
        void insertTemplateByName (const juce::String& name);
        void triggerTestEvent (const juce::String& eventName);
        void refreshInspectorHelpers();

        juce::String getVariablesText() const;
        juce::String getReferenceText() const;
        juce::StringArray getControlParameterIds() const;

        void paint (juce::Graphics&) override;
        void resized() override;

        void projectChanged() override;
        void liveValueChanged (const juce::String& parameterId, float newValue) override;

    private:
        struct ScriptTemplate
        {
            juce::String category;
            juce::String name;
            juce::String description;
            juce::String insertText;
        };

        void compileScript (bool live = false);
        void generateWithAi();
        void applyAiResult (const juce::String& raw);
        void validateAndShowStatus();
        void refreshVariablesView();
        void setupTemplatePresets();
        void insertSelectedTemplate();
        void triggerPreviewScriptEvent (const juce::String& eventName,
                                        const std::map<juce::String, float>& args,
                                        const juce::String& targetId = {});
        juce::String pscriptTargetForParameter (const juce::String& parameterId) const;
        static juce::String buildReferenceDocumentation();

        StudioMainComponent& studioOwner;
        PatchCraftProject& project;

        juce::TextEditor codeEditor;
        juce::TextButton compileButton;
        juce::TextButton liveButton;
        juce::TextButton clearLogButton;
        juce::TextButton popOutButton;
        juce::TextButton closeButton;
        juce::TextButton aiButton;
        juce::TextButton consoleToggleButton;
        juce::ComboBox templatePresetBox;
        juce::TextEditor statusLog;
        int hotReloadCountdown = -1;

        bool consoleVisible = false;
        bool runPresetOnCompile = true;

        std::vector<ScriptTemplate> scriptTemplates;
        std::map<juce::String, float> currentVariables;

        void timerCallback() override;
    };
}
