#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/PatchCraftProject.h"
#include <map>
#include <vector>

namespace patchcraft
{
    /**
        A premium, interactive pScript workspace with a side-by-side layout:
        - Left: Code Editor & Console Output Terminal
        - Right: Tabbed Reference, Variable Watcher, and Canvas Parameter Explorer
    */
    class ScriptEditorComponent : public juce::Component,
                                  public PatchCraftProject::Listener,
                                  public juce::Timer
    {
    public:
        ScriptEditorComponent (PatchCraftProject& p)
            : project (p),
              activeTab (0)
        {
            project.addListener (this);

            // Left Side: Code Editor
            codeEditor.setMultiLine (true);
            codeEditor.setReturnKeyStartsNewLine (true);
            codeEditor.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            codeEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.4f));
            codeEditor.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke);
            codeEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::teal.withAlpha (0.5f));
            codeEditor.setColour (juce::TextEditor::highlightColourId, juce::Colours::teal.withAlpha (0.3f));
            codeEditor.setColour (juce::CaretComponent::caretColourId, juce::Colours::orange);
            addAndMakeVisible (codeEditor);

            // Left Side: Compile & Clear Controls Bar
            compileButton.setButtonText ("Compile & Test");
            compileButton.setColour (juce::TextButton::buttonColourId, juce::Colours::teal.darker (0.2f));
            compileButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            compileButton.onClick = [this] { compileScript(); };
            addAndMakeVisible (compileButton);

            clearLogButton.setButtonText ("Clear Log");
            clearLogButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
            clearLogButton.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            clearLogButton.onClick = [this] { statusLog.clear(); };
            addAndMakeVisible (clearLogButton);

            // Left Side: Debug Output Console Terminal
            statusLog.setMultiLine (true);
            statusLog.setReadOnly (true);
            statusLog.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
            statusLog.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.6f));
            statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke);
            statusLog.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (statusLog);

            // Right Side: Tab Buttons Bar
            tabReferenceBtn.setButtonText ("Reference");
            tabReferenceBtn.setToggleable (true);
            tabReferenceBtn.onClick = [this] { switchTab (0); };
            addAndMakeVisible (tabReferenceBtn);

            tabVariablesBtn.setButtonText ("Variables");
            tabVariablesBtn.setToggleable (true);
            tabVariablesBtn.onClick = [this] { switchTab (1); };
            addAndMakeVisible (tabVariablesBtn);

            tabRegistryBtn.setButtonText ("Controls");
            tabRegistryBtn.setToggleable (true);
            tabRegistryBtn.onClick = [this] { switchTab (2); };
            addAndMakeVisible (tabRegistryBtn);

            // Right Side Tab 1: Cheatsheet Panel (Viewport & Container)
            addAndMakeVisible (cheatsheetViewport);
            cheatsheetViewport.setViewedComponent (&cheatsheetContainer, false);
            setupCheatsheetItems();

            // Right Side Tab 2: Variable Debug Watcher
            addAndMakeVisible (variablesWatcher);
            variablesWatcher.setMultiLine (true);
            variablesWatcher.setReadOnly (true);
            variablesWatcher.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            variablesWatcher.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.5f));
            variablesWatcher.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
            variablesWatcher.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            refreshVariablesView();

            // Right Side Tab 3: Controls Parameter Registry Panel
            addAndMakeVisible (registryViewport);
            registryViewport.setViewedComponent (&registryContainer, false);

            switchTab (0);
            refresh();
            
            // Poll log & variables updates at 30 FPS
            startTimer (33);
        }

        ~ScriptEditorComponent() override
        {
            stopTimer();
            project.removeListener (this);
        }

        void refresh()
        {
            codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
            validateAndShowStatus();
            rebuildRegistryPanel();
        }

        void insertSnippetAndCompile (const juce::String& snippet)
        {
            auto src = codeEditor.getText().trimEnd();
            if (src.isNotEmpty())
                src += "\n\n";
            src += snippet.trimEnd();
            src += "\n";

            codeEditor.setText (src, juce::dontSendNotification);
            codeEditor.setCaretPosition (src.length());
            compileScript();
            codeEditor.grabKeyboardFocus();
        }

        void projectChanged() override
        {
            if (codeEditor.getText() != project.getPscriptSource())
                codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
            validateAndShowStatus();
            rebuildRegistryPanel();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::transparentBlack);
        }

        void resized() override
        {
            auto r = getLocalBounds();

            // Split Left / Right
            auto rightWidth = juce::jlimit (200, 450, (int) (r.getWidth() * 0.32f));
            auto leftArea = r.removeFromLeft (r.getWidth() - rightWidth);
            leftArea.reduce (4, 4);
            rightArea = r.reduced (4, 4);

            // Left Area Layout: Editor on top, Controls Bar in middle, Terminal Console on bottom
            auto consoleHeight = 160;
            auto consoleArea = leftArea.removeFromBottom (consoleHeight);
            auto ctrlBarArea = consoleArea.removeFromTop (32);
            
            // Controls Bar buttons
            auto compileBtnArea = ctrlBarArea.removeFromLeft (130);
            compileButton.setBounds (compileBtnArea.reduced (2));
            auto clearBtnArea = ctrlBarArea.removeFromLeft (100);
            clearLogButton.setBounds (clearBtnArea.reduced (2));

            statusLog.setBounds (consoleArea.reduced (2));
            codeEditor.setBounds (leftArea.reduced (2));

            // Right Area Layout: Tab Bar at top, Content Panel at bottom
            auto tabHeight = 32;
            auto tabBar = rightArea.removeFromTop (tabHeight);
            
            auto btnW = tabBar.getWidth() / 3;
            tabReferenceBtn.setBounds (tabBar.removeFromLeft (btnW).reduced (1));
            tabVariablesBtn.setBounds (tabBar.removeFromLeft (btnW).reduced (1));
            tabRegistryBtn.setBounds (tabBar.reduced (1));

            cheatsheetViewport.setBounds (rightArea);
            variablesWatcher.setBounds (rightArea);
            registryViewport.setBounds (rightArea);

            cheatsheetContainer.setBounds (0, 0, cheatsheetViewport.getWidth() - 16, cheatsheetItems.size() * 54);
        }

        // Timer callback pulls telemetry from audio thread
        void timerCallback() override
        {
            // 1. Pull print logs
            auto logs = project.getScriptEngine().getPendingLogs();
            if (! logs.empty())
            {
                juce::String currentLogText = statusLog.getText();
                for (const auto& log : logs)
                {
                    auto timeStr = log.timestamp.formatted ("%H:%M:%S");
                    currentLogText += "[" + timeStr + "] " + log.text + "\n";
                }
                statusLog.setText (currentLogText, false);
                statusLog.setCaretPosition (currentLogText.length());
            }

            // 2. Pull variable updates
            auto updates = project.getScriptEngine().getPendingVariableUpdates();
            if (! updates.empty())
            {
                for (const auto& u : updates)
                    currentVariables[u.name] = u.value;
                refreshVariablesView();
            }
        }

    private:
        void switchTab (int tabIndex)
        {
            activeTab = tabIndex;

            tabReferenceBtn.setToggleState (tabIndex == 0, juce::dontSendNotification);
            tabVariablesBtn.setToggleState (tabIndex == 1, juce::dontSendNotification);
            tabRegistryBtn.setToggleState (tabIndex == 2, juce::dontSendNotification);

            tabReferenceBtn.setColour (juce::TextButton::buttonColourId, tabIndex == 0 ? juce::Colours::teal.darker (0.3f) : juce::Colour (0xff242424));
            tabVariablesBtn.setColour (juce::TextButton::buttonColourId, tabIndex == 1 ? juce::Colours::teal.darker (0.3f) : juce::Colour (0xff242424));
            tabRegistryBtn.setColour (juce::TextButton::buttonColourId, tabIndex == 2 ? juce::Colours::teal.darker (0.3f) : juce::Colour (0xff242424));

            cheatsheetViewport.setVisible (tabIndex == 0);
            variablesWatcher.setVisible (tabIndex == 1);
            registryViewport.setVisible (tabIndex == 2);
        }

        void compileScript()
        {
            auto src = codeEditor.getText();
            auto error = project.getScriptEngine().compile (src);
            project.setPscriptSource (src);

            currentVariables.clear();
            refreshVariablesView();

            if (error.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
                statusLog.setText ("Compilation successful!\nReady.\n", false);
            }
            else
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
                statusLog.setText (error + "\n", false);
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

        void refreshVariablesView()
        {
            juce::String text = "// Active Script Variables Watcher\n\n";
            if (currentVariables.empty())
            {
                text += "// (No variables registered yet. Execute note or control events to populate variables)\n";
            }
            else
            {
                for (const auto& pair : currentVariables)
                {
                    text += "let " + pair.first + " = " + juce::String (pair.second, 4) + "\n";
                }
            }
            variablesWatcher.setText (text, false);
        }

        struct CheatsheetItem
        {
            juce::String title;
            juce::String description;
            juce::String insertText;
        };

        std::vector<CheatsheetItem> cheatsheetItems;

        void setupCheatsheetItems()
        {
            cheatsheetItems = {
                { "Control Macro", "One mapped control drives another parameter", "when knob \"filterCutoff\" moves:\n    let amount = value mapped 20 Hz..20000 Hz -> 0.0..1.0\n    set delayMix to amount * 0.35\n" },
                { "Note Velocity", "MIDI velocity opens the filter musically", "when note starts:\n    set filterCutoff to velocity mapped 0..127 -> 800 Hz..9000 Hz\n" },
                { "Note Release", "Reset a value when notes stop", "when note ends:\n    set delayMix to 0.0\n" },
                { "Mod Wheel", "Hardware mod wheel controls an effect amount", "when modwheel moves:\n    set reverbMix to modwheel mapped 0..127 -> 0.0..0.45\n" },
                { "Timer Motion", "Creates a periodic script event", "when timer 250 ms:\n    print value\n" },
                { "Variable", "Create a readable intermediate value", "let amount = value mapped 0.0..1.0 -> 0.0..1.0\n" },
                { "Set Parameter", "Write directly to a real parameter ID", "set filterCutoff to 1200 Hz\n" },
                { "Randomize Safe Range", "Randomize inside a musical range", "randomize filterCutoff between 600 Hz and 1800 Hz\n" },
                { "Conditional", "Choose behavior from an input value", "if velocity > 100:\n    set delayMix to 0.25\nelse:\n    set delayMix to 0.05\n" },
                { "Print Debug", "Show a value in the pScript log", "print value\n" }
            };

            // Setup buttons in cheatsheet viewport container
            cheatsheetContainer.deleteAllChildren();
            for (size_t i = 0; i < cheatsheetItems.size(); ++i)
            {
                auto& item = cheatsheetItems[i];
                auto* btn = new juce::TextButton();
                btn->setButtonText (item.title);
                btn->setTooltip (item.description);
                btn->setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker (0.2f));
                btn->onClick = [this, item] {
                    codeEditor.insertTextAtCaret (item.insertText);
                    codeEditor.grabKeyboardFocus();
                };
                cheatsheetContainer.addAndMakeVisible (btn);
                btn->setBounds (4, (int) (i * 54) + 4, cheatsheetViewport.getWidth() - 24, 46);
            }
        }

        // Visual explorer of canvas parameter mappings
        void rebuildRegistryPanel()
        {
            registryContainer.deleteAllChildren();
            auto allParams = project.getParameters().getAll();
            
            int yPos = 4;
            int idx = 0;
            for (const auto& p : allParams)
            {
                auto friendlyName = p.name.isNotEmpty() ? p.name : p.id;
                auto desc = "ID: " + p.id + " | Range: " + juce::String (p.min) + ".." + juce::String (p.max) + " " + p.unit;

                auto* btn = new juce::TextButton();
                btn->setButtonText (friendlyName + "  (" + p.id + ")");
                btn->setTooltip (desc);
                btn->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c1c));
                btn->onClick = [this, parameterId = p.id] {
                    codeEditor.insertTextAtCaret (parameterId);
                    codeEditor.grabKeyboardFocus();
                };
                registryContainer.addAndMakeVisible (btn);
                btn->setBounds (4, yPos, registryViewport.getWidth() - 24, 46);
                
                yPos += 54;
                idx++;
            }
            registryContainer.setBounds (0, 0, registryViewport.getWidth() - 16, yPos + 10);
        }

        static juce::String getFriendlyParameterName (const juce::String& id)
        {
            juce::String res;
            for (int i = 0; i < id.length(); ++i)
            {
                auto c = id[i];
                if (juce::CharacterFunctions::isUpperCase (c) && i > 0)
                {
                    res += "." + juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
                }
                else
                {
                    res += juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
                }
            }
            return res;
        }

        PatchCraftProject& project;
        juce::TextEditor codeEditor;
        juce::TextButton compileButton;
        juce::TextButton clearLogButton;
        juce::TextEditor statusLog;

        // Custom Tab System
        int activeTab;
        juce::TextButton tabReferenceBtn;
        juce::TextButton tabVariablesBtn;
        juce::TextButton tabRegistryBtn;
        juce::Rectangle<int> rightArea;

        // Cheatsheet components
        juce::Viewport cheatsheetViewport;
        class CheatsheetContainer : public juce::Component
        {
        public:
            ~CheatsheetContainer() override { deleteAllChildren(); }
        } cheatsheetContainer;

        // Watcher components
        juce::TextEditor variablesWatcher;
        std::map<juce::String, float> currentVariables;

        // Registry components
        juce::Viewport registryViewport;
        class RegistryContainer : public juce::Component
        {
        public:
            ~RegistryContainer() override { deleteAllChildren(); }
        } registryContainer;
    };
}
