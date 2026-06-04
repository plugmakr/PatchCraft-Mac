#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/PatchCraftProject.h"

namespace patchcraft
{
    /**
        ScriptEditorComponent provides a multi-line code editor for pScript code,
        complete with an inline compiler trigger and status logging.
    */
    class ScriptEditorComponent : public juce::Component,
                                  public PatchCraftProject::Listener
    {
    public:
        ScriptEditorComponent (PatchCraftProject& p)
            : project (p)
        {
            project.addListener (this);

            // Code Editor
            codeEditor.setMultiLine (true);
            codeEditor.setReturnKeyStartsNewLine (true);
            codeEditor.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            codeEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.4f));
            codeEditor.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke);
            codeEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparent);
            codeEditor.setColour (juce::TextEditor::highlightColourId, juce::Colours::teal.withAlpha (0.4f));
            codeEditor.setColour (juce::TextEditor::caretColourId, juce::Colours::orange);
            addAndMakeVisible (codeEditor);

            // Status Log Panel
            statusLog.setMultiLine (true);
            statusLog.setReadOnly (true);
            statusLog.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
            statusLog.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.6f));
            statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible (statusLog);

            // Compile Button
            compileButton.setButtonText ("Compile pScript");
            compileButton.onClick = [this] { compileScript(); };
            addAndMakeVisible (compileButton);

            refresh();
        }

        ~ScriptEditorComponent() override
        {
            project.removeListener (this);
        }

        void refresh()
        {
            codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
            validateAndShowStatus();
        }

        void projectChanged() override
        {
            if (codeEditor.getText() != project.getPscriptSource())
                codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
            validateAndShowStatus();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::transparent);
        }

        void resized() override
        {
            auto r = getLocalBounds();

            auto bottomArea = r.removeFromBottom (140);
            auto btnArea = bottomArea.removeFromTop (32).reduced (4);
            compileButton.setBounds (btnArea);
            statusLog.setBounds (bottomArea.reduced (4));

            codeEditor.setBounds (r.reduced (4));
        }

    private:
        void compileScript()
        {
            auto src = codeEditor.getText();
            auto error = project.getScriptEngine().compile (src);
            project.setPscriptSource (src);

            if (error.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
                statusLog.setText ("Compilation successful!\nReady.", false);
            }
            else
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
                statusLog.setText (error, false);
            }
        }

        void validateAndShowStatus()
        {
            auto src = project.getPscriptSource();
            if (src.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgrey);
                statusLog.setText ("No script loaded. Write pScript here.", false);
                return;
            }

            auto error = project.getScriptEngine().compile (src);
            if (error.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
                statusLog.setText ("Compilation successful!", false);
            }
            else
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
                statusLog.setText (error, false);
            }
        }

        PatchCraftProject& project;
        juce::TextEditor codeEditor;
        juce::TextEditor statusLog;
        juce::TextButton compileButton;
    };
}
