#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/PatchCraftProject.h"
#include "../Shared/AiAssistService.h"
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
        // Invoked when the user clicks "Pop Out"; host wires this to float the panel.
        std::function<void()> onPopOut;
        // Invoked when the user closes the docked editor; host returns to Elements tab.
        std::function<void()> onClose;

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

            liveButton.setButtonText ("Live");
            liveButton.setClickingTogglesState (true);
            liveButton.setToggleState (true, juce::dontSendNotification);
            liveButton.setTooltip ("Hot-reload: recompile automatically as you type.");
            liveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
            liveButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::teal.darker (0.2f));
            liveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            liveButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible (liveButton);

            clearLogButton.setButtonText ("Clear Log");
            clearLogButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
            clearLogButton.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            clearLogButton.onClick = [this] { statusLog.clear(); };
            addAndMakeVisible (clearLogButton);

            popOutButton.setButtonText ("Pop Out");
            popOutButton.setTooltip ("Open the pScript editor in its own resizable window.");
            popOutButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
            popOutButton.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            popOutButton.onClick = [this] { if (onPopOut) onPopOut(); };
            addAndMakeVisible (popOutButton);

            closeButton.setButtonText ("Close");
            closeButton.setTooltip ("Close the pScript editor and return to Elements.");
            closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
            closeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
            closeButton.onClick = [this] { if (onClose) onClose(); };
            addAndMakeVisible (closeButton);

            aiButton.setButtonText ("AI Generate");
            aiButton.setTooltip ("Generate pScript with the global AI (DeepSeek). Describe what you want.");
            aiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a3a8c));
            aiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            aiButton.onClick = [this] { generateWithAi(); };
            addAndMakeVisible (aiButton);

            // Hot reload: debounce recompiles while typing.
            codeEditor.onTextChange = [this]
            {
                if (liveButton.getToggleState())
                    hotReloadCountdown = 12;   // ~400ms at 33ms tick
            };

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

            // Right Side Tab 1: Language reference + snippet shortcuts
            referenceDoc.setMultiLine (true);
            referenceDoc.setReadOnly (true);
            referenceDoc.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
            referenceDoc.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
            referenceDoc.setColour (juce::TextEditor::textColourId, juce::Colours::lightgrey);
            referenceDoc.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            referenceDoc.setText (buildReferenceDocumentation());
            addAndMakeVisible (referenceDoc);

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
            compileButton.setBounds (ctrlBarArea.removeFromLeft (120).reduced (2));
            liveButton.setBounds (ctrlBarArea.removeFromLeft (54).reduced (2));
            clearLogButton.setBounds (ctrlBarArea.removeFromLeft (84).reduced (2));
            closeButton.setBounds (ctrlBarArea.removeFromRight (68).reduced (2));
            popOutButton.setBounds (ctrlBarArea.removeFromRight (84).reduced (2));
            aiButton.setBounds (ctrlBarArea.removeFromRight (100).reduced (2));

            statusLog.setBounds (consoleArea.reduced (2));
            codeEditor.setBounds (leftArea.reduced (2));

            // Right Area Layout: Tab Bar at top, Content Panel at bottom
            auto tabHeight = 32;
            auto tabBar = rightArea.removeFromTop (tabHeight);
            
            auto btnW = tabBar.getWidth() / 3;
            tabReferenceBtn.setBounds (tabBar.removeFromLeft (btnW).reduced (1));
            tabVariablesBtn.setBounds (tabBar.removeFromLeft (btnW).reduced (1));
            tabRegistryBtn.setBounds (tabBar.reduced (1));

            auto tabContent = rightArea;

            if (activeTab == 0)
            {
                const int refDocH = juce::jmax (140, (int) (tabContent.getHeight() * 0.58f));
                auto refTop = tabContent.removeFromTop (refDocH);
                referenceDoc.setBounds (refTop.reduced (1));
                cheatsheetViewport.setBounds (tabContent);
                layoutCheatsheetButtons();
            }
            else if (activeTab == 1)
            {
                variablesWatcher.setBounds (tabContent);
            }
            else
            {
                registryViewport.setBounds (tabContent);
            }
        }

        void layoutCheatsheetButtons()
        {
            const int width = juce::jmax (160, cheatsheetViewport.getWidth() - 24);
            int y = 4;
            for (auto* child : cheatsheetContainer.getChildren())
            {
                if (auto* btn = dynamic_cast<juce::TextButton*> (child))
                {
                    btn->setBounds (4, y, width, 42);
                    y += 46;
                }
            }
            cheatsheetContainer.setSize (cheatsheetViewport.getWidth() - 16, y + 8);
        }

        // Timer callback pulls telemetry from audio thread
        void timerCallback() override
        {
            // 0. Hot-reload debounce: recompile shortly after the user stops typing.
            if (hotReloadCountdown > 0)
            {
                if (--hotReloadCountdown == 0)
                    compileScript (true);
            }

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
            referenceDoc.setVisible (tabIndex == 0);
            variablesWatcher.setVisible (tabIndex == 1);
            registryViewport.setVisible (tabIndex == 2);
            resized();
        }

        static juce::String buildReferenceDocumentation()
        {
            return R"(pScript Language Reference
==========================

Structure
---------
- Root-level event blocks only. Indent the body with spaces or tabs.
- Optional header: script "My Instrument Logic"
- Each handler: when <event>:

Events
------
when preset loads:
when note starts:          // value: velocity (0..127)
when note ends:
when modwheel moves:       // value: modwheel (0..127)
when knob "Cutoff" moves:  // value: control 0..1 or Hz depending on mapping
when pad "Kick" held:
when pad "Kick" released:
when timer 250 ms:         // fires every 250 ms while script is compiled

Statements
----------
let amount = value mapped 0.0..1.0 -> 0.0..1.0
set filterCutoff to velocity mapped 0..127 -> 800 Hz..9000 Hz
print value
randomize filterCutoff between 600 Hz and 1800 Hz
play layer "Main"
turn on effect "Delay"
turn off effect "Delay"
smooth 120 ms              // applies to the next set/randomize
repeat 4: set delayMix to 0.2
if velocity > 100:
    set delayMix to 0.25
else:
    set delayMix to 0.05

Expressions
-----------
- Numbers with units: 1200 Hz, 0.35, 250 ms, 50%
- Identifiers: value, velocity, modwheel, parameter IDs from Controls tab
- Mapped ranges: <source> mapped <min>..<max> -> <min>..<max>
- Math: + - * / with standard precedence; unary minus supported

Parameters
----------
Use real parameter IDs from the Controls tab (e.g. filterCutoff, delayMix).
Friendly dotted names (filter.cutoff) are resolved automatically.

Tips
----
- Enable Live for hot-reload while editing.
- Use print value to debug knob and note events in the log below.
- Attach handlers from the Inspector or select multiple controls for bulk attach.
)";
        }

        void compileScript (bool live = false)
        {
            hotReloadCountdown = -1;
            auto src = codeEditor.getText();
            auto error = project.getScriptEngine().compile (src);
            project.setPscriptSource (src);

            if (! live)
            {
                currentVariables.clear();
                refreshVariablesView();
            }

            const auto stamp = juce::Time::getCurrentTime().formatted ("%H:%M:%S");
            if (error.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
                statusLog.setText ((live ? "[" + stamp + "] Live reload OK.\n"
                                         : "Compilation successful!\nReady.\n"), false);
            }
            else
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
                statusLog.setText ((live ? "[" + stamp + "] " : juce::String()) + error + "\n", false);
            }
        }

        void generateWithAi()
        {
            auto* window = new juce::AlertWindow ("Generate pScript with AI",
                "Describe the behaviour you want (e.g. 'open the filter with velocity and add a slow timed tremolo on delayMix').",
                juce::MessageBoxIconType::QuestionIcon);
            window->addTextEditor ("Prompt", "", "Prompt:");
            window->addButton ("Generate", 1);
            window->addButton ("Cancel", 0);
            window->enterModalState (true, juce::ModalCallbackFunction::create (
                [this, window] (int result)
                {
                    std::unique_ptr<juce::AlertWindow> owned (window);
                    if (result != 1)
                        return;
                    auto* text = window->getTextEditor ("Prompt");
                    if (text == nullptr)
                        return;
                    const auto prompt = text->getText().trim();
                    if (prompt.isEmpty())
                        return;

                    statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgrey);
                    statusLog.setText ("Contacting AI...\n", false);

                    // Build a context pack so the model only uses real parameter IDs.
                    AiAssistService::ProjectContextPack ctx;
                    ctx.instrumentName = project.getManifest().instrumentName;
                    juce::StringArray paramLines;
                    for (const auto& p : project.getParameters().getAll())
                        paramLines.add (p.id + " (" + (p.name.isNotEmpty() ? p.name : p.id) + ", "
                                        + juce::String (p.min) + ".." + juce::String (p.max) + " " + p.unit + ")");
                    ctx.parameterSummary = paramLines.joinIntoString ("\n");

                    juce::Component::SafePointer<ScriptEditorComponent> safe (this);
                    juce::Thread::launch ([safe, ctx, prompt]()
                    {
                        AiAssistService service;
                        auto suggestion = service.runWithPrompt (AiAssistService::TaskType::GeneratePScript, ctx, prompt);
                        juce::MessageManager::callAsync ([safe, suggestion]()
                        {
                            if (safe == nullptr)
                                return;
                            safe->applyAiResult (suggestion.details);
                        });
                    });
                }));
        }

        void applyAiResult (const juce::String& raw)
        {
            auto script = raw.trim();
            // Strip markdown code fences if the model added them.
            if (script.startsWith ("```"))
            {
                script = script.fromFirstOccurrenceOf ("\n", false, false);
                script = script.upToLastOccurrenceOf ("```", false, false).trim();
            }
            if (script.isEmpty())
            {
                statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
                statusLog.setText ("AI returned no script. Check the API key in Settings.\n", false);
                return;
            }
            auto existing = codeEditor.getText().trimEnd();
            auto combined = existing.isNotEmpty() ? existing + "\n\n" + script : script;
            codeEditor.setText (combined, juce::dontSendNotification);
            codeEditor.setCaretPosition (combined.length());
            compileScript();
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
                { "Preset Load", "Initialize values when a preset loads", "when preset loads:\n    set filterCutoff to 1200 Hz\n    set delayMix to 0.12\n" },
                { "Pad Held", "Respond while a drum pad is held", "when pad \"Kick\" held:\n    set driveAmount to 0.65\n" },
                { "Pad Release", "Reset when a pad is released", "when pad \"Kick\" released:\n    set driveAmount to 0.0\n" },
                { "Timer Motion", "Creates a periodic script event", "when timer 250 ms:\n    print value\n" },
                { "Play Layer", "Trigger a sample layer by name", "when note starts:\n    play layer \"Main\"\n" },
                { "Effect Toggle", "Turn an effect on or off from script", "when knob \"fxEnable\" moves:\n    turn on effect \"Delay\"\n" },
                { "Variable", "Create a readable intermediate value", "let amount = value mapped 0.0..1.0 -> 0.0..1.0\n" },
                { "Set Parameter", "Write directly to a real parameter ID", "set filterCutoff to 1200 Hz\n" },
                { "Randomize Safe Range", "Randomize inside a musical range", "randomize filterCutoff between 600 Hz and 1800 Hz\n" },
                { "Smooth Glide", "Glide the next parameter write over time", "when knob \"macro1\" moves:\n    smooth 180 ms\n    set filterCutoff to value mapped 0.0..1.0 -> 400 Hz..8000 Hz\n" },
                { "Repeat Loop", "Run a statement multiple times", "when note starts:\n    repeat 3:\n        randomize pan between -0.4 and 0.4\n" },
                { "Conditional", "Choose behavior from an input value", "if velocity > 100:\n    set delayMix to 0.25\nelse:\n    set delayMix to 0.05\n" },
                { "Print Debug", "Show a value in the pScript log", "print value\n" }
            };

            cheatsheetContainer.deleteAllChildren();
            for (const auto& item : cheatsheetItems)
            {
                auto* btn = new juce::TextButton();
                btn->setButtonText (item.title);
                btn->setTooltip (item.description);
                btn->setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker (0.2f));
                btn->onClick = [this, item] {
                    codeEditor.insertTextAtCaret (item.insertText);
                    codeEditor.grabKeyboardFocus();
                };
                cheatsheetContainer.addAndMakeVisible (btn);
            }
            layoutCheatsheetButtons();
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
        juce::TextButton liveButton;
        juce::TextButton clearLogButton;
        juce::TextButton popOutButton;
        juce::TextButton closeButton;
        juce::TextButton aiButton;
        juce::TextEditor statusLog;
        juce::TextEditor referenceDoc;
        int hotReloadCountdown = -1;

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
