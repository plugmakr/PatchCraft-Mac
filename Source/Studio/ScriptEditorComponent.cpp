#include "ScriptEditorComponent.h"
#include "StudioMainComponent.h"
#include "PackRuntimeHost.h"
#include "InspectorPanel.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        static void styleToolButton (juce::TextButton& b, juce::Colour bg = juce::Colour (0xff242424))
        {
            b.setColour (juce::TextButton::buttonColourId, bg);
            b.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        }

        static void styleToggleToolButton (juce::TextButton& b)
        {
            b.setClickingTogglesState (true);
            styleToolButton (b);
            b.setColour (juce::TextButton::buttonOnColourId, juce::Colours::teal.darker (0.2f));
            b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }
    }

    ScriptEditorComponent::ScriptEditorComponent (StudioMainComponent& owner)
        : studioOwner (owner),
          project (owner.getProject())
    {
        project.addListener (this);
        project.getLiveValues().addListener (this);

        codeEditor.setMultiLine (true);
        codeEditor.setReturnKeyStartsNewLine (true);
        codeEditor.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.5f, juce::Font::plain));
        codeEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.45f));
        codeEditor.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke);
        codeEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::teal.withAlpha (0.5f));
        codeEditor.setTextToShowWhenEmpty ("Write pScript here, drop in a template, or use AI Generate...",
                                           PatchCraftLookAndFeel::textDim().withAlpha (0.75f));
        addAndMakeVisible (codeEditor);

        compileButton.setButtonText ("Compile & Test");
        compileButton.setColour (juce::TextButton::buttonColourId, juce::Colours::teal.darker (0.2f));
        compileButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        compileButton.onClick = [this] { compileScript(); };
        addAndMakeVisible (compileButton);

        liveButton.setButtonText ("Live");
        liveButton.setClickingTogglesState (true);
        liveButton.setToggleState (true, juce::dontSendNotification);
        liveButton.setTooltip ("Hot-reload while typing.");
        styleToggleToolButton (liveButton);
        addAndMakeVisible (liveButton);

        templatePresetBox.setTextWhenNothingSelected ("Insert template...");
        templatePresetBox.onChange = [this] { insertSelectedTemplate(); };
        addAndMakeVisible (templatePresetBox);
        setupTemplatePresets();

        consoleToggleButton.setButtonText ("Console");
        styleToggleToolButton (consoleToggleButton);
        consoleToggleButton.setToggleState (false, juce::dontSendNotification);
        consoleToggleButton.onClick = [this] { consoleVisible = consoleToggleButton.getToggleState(); resized(); };
        addAndMakeVisible (consoleToggleButton);

        clearLogButton.setButtonText ("Clear Log");
        styleToolButton (clearLogButton);
        clearLogButton.onClick = [this] { statusLog.clear(); };
        addAndMakeVisible (clearLogButton);

        popOutButton.setButtonText ("Pop Out");
        styleToolButton (popOutButton);
        popOutButton.onClick = [this] { if (onPopOut) onPopOut(); };
        addAndMakeVisible (popOutButton);

        closeButton.setButtonText ("Close");
        styleToolButton (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);

        aiButton.setButtonText ("AI Generate");
        aiButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a3a8c));
        aiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        aiButton.onClick = [this] { generateWithAi(); };
        addAndMakeVisible (aiButton);

        codeEditor.onTextChange = [this]
        {
            if (liveButton.getToggleState())
                hotReloadCountdown = 12;
        };

        statusLog.setMultiLine (true);
        statusLog.setReadOnly (true);
        statusLog.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        statusLog.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.65f));
        statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke);
        addAndMakeVisible (statusLog);

        refresh();
        startTimer (33);
    }

    ScriptEditorComponent::~ScriptEditorComponent()
    {
        stopTimer();
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void ScriptEditorComponent::triggerPreviewScriptEvent (const juce::String& eventName,
                                                           const std::map<juce::String, float>& args,
                                                           const juce::String& targetId)
    {
        project.getScriptEngine().triggerEvent (eventName, args, targetId);
        refreshInspectorHelpers();
    }

    void ScriptEditorComponent::triggerTestEvent (const juce::String& eventName)
    {
        if (eventName == "note starts")
            triggerPreviewScriptEvent ("note starts", { { "velocity", 100.0f }, { "note", 60.0f } });
        else if (eventName == "note ends")
            triggerPreviewScriptEvent ("note ends", { { "velocity", 0.0f }, { "note", 60.0f } });
        else if (eventName == "modwheel")
            triggerPreviewScriptEvent ("modwheel moves", { { "modwheel", 90.0f } });
    }

    juce::String ScriptEditorComponent::pscriptTargetForParameter (const juce::String& parameterId) const
    {
        return parameterId;
    }

    void ScriptEditorComponent::liveValueChanged (const juce::String& parameterId, float newValue)
    {
        if (! isVisible() || ! project.getScriptEngine().isCompiled())
            return;

        std::map<juce::String, float> args;
        args["value"] = newValue;
        project.getScriptEngine().triggerEvent ("knob moves", args, pscriptTargetForParameter (parameterId));
    }

    juce::String ScriptEditorComponent::getVariablesText() const
    {
        juce::String text = "// Active variables\n\n";
        if (currentVariables.empty())
            return text + "// (Run events from the preview to populate variables)\n";
        for (const auto& pair : currentVariables)
            text += "let " + pair.first + " = " + juce::String (pair.second, 4) + "\n";
        return text;
    }

    juce::String ScriptEditorComponent::getReferenceText() const
    {
        return buildReferenceDocumentation();
    }

    juce::StringArray ScriptEditorComponent::getControlParameterIds() const
    {
        juce::StringArray ids;
        for (const auto& p : project.getParameters().getAll())
            ids.add (p.id);
        return ids;
    }

    void ScriptEditorComponent::refreshInspectorHelpers()
    {
        if (auto* inspector = studioOwner.getInspectorPanel())
            inspector->refreshPscriptHelpers();
    }

    void ScriptEditorComponent::refresh()
    {
        codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
        validateAndShowStatus();
        if (auto* runtime = studioOwner.getPackRuntime())
        {
            runtime->requestReloadImmediate();
            runtime->ensurePlaybackReady();
        }
        refreshInspectorHelpers();
    }

    void ScriptEditorComponent::insertSnippetAndCompile (const juce::String& snippet)
    {
        auto src = codeEditor.getText().trimEnd();
        if (src.isNotEmpty()) src += "\n\n";
        src += snippet.trimEnd() + "\n";
        codeEditor.setText (src, juce::dontSendNotification);
        compileScript();
    }

    void ScriptEditorComponent::insertTemplateByName (const juce::String& name)
    {
        for (const auto& t : scriptTemplates)
        {
            if (t.name == name)
            {
                insertSnippetAndCompile (t.insertText);
                return;
            }
        }
    }

    void ScriptEditorComponent::projectChanged()
    {
        if (codeEditor.getText() != project.getPscriptSource())
            codeEditor.setText (project.getPscriptSource(), juce::dontSendNotification);
        validateAndShowStatus();
    }

    void ScriptEditorComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel().darker (0.12f));
    }

    void ScriptEditorComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (4);

        const bool narrowToolbar = bounds.getWidth() < 720;
        const int toolbarH = narrowToolbar ? 64 : 32;
        auto toolbar = bounds.removeFromTop (toolbarH);

        if (narrowToolbar)
        {
            auto row1 = toolbar.removeFromTop (32);
            compileButton.setBounds (row1.removeFromLeft (118).reduced (1));
            liveButton.setBounds (row1.removeFromLeft (52).reduced (1));
            templatePresetBox.setBounds (row1.removeFromLeft (juce::jmax (160, row1.getWidth() / 2)).reduced (1));
            consoleToggleButton.setBounds (row1.removeFromLeft (72).reduced (1));

            auto row2 = toolbar;
            clearLogButton.setBounds (row2.removeFromLeft (78).reduced (1));
            aiButton.setBounds (row2.removeFromRight (96).reduced (1));
            closeButton.setBounds (row2.removeFromRight (62).reduced (1));
            popOutButton.setBounds (row2.removeFromRight (72).reduced (1));
        }
        else
        {
            compileButton.setBounds (toolbar.removeFromLeft (118).reduced (1));
            liveButton.setBounds (toolbar.removeFromLeft (52).reduced (1));
            templatePresetBox.setBounds (toolbar.removeFromLeft (juce::jmax (160, toolbar.getWidth() / 3)).reduced (1));
            consoleToggleButton.setBounds (toolbar.removeFromLeft (72).reduced (1));
            closeButton.setBounds (toolbar.removeFromRight (62).reduced (1));
            popOutButton.setBounds (toolbar.removeFromRight (72).reduced (1));
            aiButton.setBounds (toolbar.removeFromRight (96).reduced (1));
            clearLogButton.setBounds (toolbar.removeFromRight (78).reduced (1));
        }

        const int consoleH = consoleVisible ? juce::jmin (96, juce::jmax (56, bounds.getHeight() / 5)) : 0;
        if (consoleVisible)
            statusLog.setBounds (bounds.removeFromBottom (consoleH).reduced (2));
        statusLog.setVisible (consoleVisible);

        codeEditor.setBounds (bounds.reduced (2));
    }

    void ScriptEditorComponent::timerCallback()
    {
        if (hotReloadCountdown > 0 && --hotReloadCountdown == 0)
            compileScript (true);

        bool varsChanged = false;
        for (const auto& log : project.getScriptEngine().getPendingLogs())
        {
            auto text = statusLog.getText();
            text += "[" + log.timestamp.formatted ("%H:%M:%S") + "] " + log.text + "\n";
            statusLog.setText (text, false);
        }

        for (const auto& u : project.getScriptEngine().getPendingVariableUpdates())
        {
            currentVariables[u.name] = u.value;
            varsChanged = true;
        }

        if (varsChanged)
            refreshInspectorHelpers();
    }

    juce::String ScriptEditorComponent::buildReferenceDocumentation()
    {
        return R"(Events: preset loads | note starts | note ends | modwheel moves
         | knob "parameterId" moves | timer 250 ms

Note context: note (0-127), velocity (0-127)
Knob context: value (parameter units)
Modwheel context: modwheel (0-127)

Statements: set, let, print, randomize, repeat, if/else
Mappings: velocity mapped 0..127 -> 800 Hz..9000 Hz

These templates show behaviour you cannot get from a single binding:
multi-target macros, velocity branching, key tracking, and timed motion.

Sharing: drop a .pscript file onto a mapped control to attach it.
See docs/PSCRIPT_USER_GUIDE.md for the full workflow.
)";
    }

    void ScriptEditorComponent::compileScript (bool live)
    {
        hotReloadCountdown = -1;
        auto error = project.setPscriptSource (codeEditor.getText());

        if (! live)
        {
            currentVariables.clear();
            refreshInspectorHelpers();
        }

        if (error.isEmpty())
        {
            if (runPresetOnCompile)
                project.getScriptEngine().triggerEvent ("preset loads", {});

            statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightgreen);
            statusLog.setText (live ? "Live reload OK.\n" : "Compilation successful!\n", false);
        }
        else
        {
            statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::coral);
            statusLog.setText (error + "\n", false);
        }
    }

    void ScriptEditorComponent::generateWithAi()
    {
        auto* window = new juce::AlertWindow ("Generate pScript with AI", "Describe the behaviour you want.",
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

                auto* promptEditor = window->getTextEditor ("Prompt");
                if (promptEditor == nullptr)
                    return;

                const auto promptCopy = promptEditor->getText().trim();
                if (promptCopy.isEmpty())
                    return;

                AiAssistService::ProjectContextPack ctx;
                ctx.instrumentName = project.getManifest().instrumentName;
                juce::StringArray paramLines;
                for (const auto& p : project.getParameters().getAll())
                    paramLines.add (p.id);
                ctx.parameterSummary = paramLines.joinIntoString ("\n");

                juce::Component::SafePointer<ScriptEditorComponent> safe (this);
                juce::Thread::launch ([safe, ctx, promptCopy]()
                {
                    AiAssistService service;
                    auto suggestion = service.runWithPrompt (AiAssistService::TaskType::GeneratePScript, ctx, promptCopy);
                    juce::MessageManager::callAsync ([safe, suggestion]()
                    {
                        if (safe != nullptr)
                            safe->applyAiResult (suggestion.details);
                    });
                });
            }));
    }

    void ScriptEditorComponent::applyAiResult (const juce::String& raw)
    {
        auto script = raw.trim();
        if (script.startsWith ("```"))
        {
            script = script.fromFirstOccurrenceOf ("\n", false, false);
            script = script.upToLastOccurrenceOf ("```", false, false).trim();
        }
        if (script.isEmpty())
            return;
        auto combined = codeEditor.getText().trimEnd();
        if (combined.isNotEmpty()) combined += "\n\n";
        combined += script;
        codeEditor.setText (combined, juce::dontSendNotification);
        compileScript();
    }

    void ScriptEditorComponent::validateAndShowStatus()
    {
        if (project.getPscriptSource().isEmpty() && project.getMergedPscriptSource().isEmpty())
        {
            statusLog.clear();
            return;
        }
        auto error = project.recompileMergedScript();
        statusLog.setColour (juce::TextEditor::textColourId, error.isEmpty() ? juce::Colours::lightgreen : juce::Colours::coral);
        statusLog.setText (error.isEmpty() ? "Compilation successful!" : error, false);
    }

    void ScriptEditorComponent::setupTemplatePresets()
    {
        scriptTemplates = {
            // --- Getting Started ---
            { "Getting Started", "Cinematic Pad Init",
              "Long release + dark filter + wet reverb on load",
              R"(# Cinematic pad starting point — runs once when the preset loads.
when preset loads:
    set attack to 0.35
    set release to 2.8
    set filterCutoff to 1800 Hz
    set reverbMix to 0.42
    set delayMix to 0.12
    set volume to 0.78
)" },
            { "Getting Started", "Pluck Init",
              "Short envelope + bright filter for plucks",
              R"(# Plucky starting point.
when preset loads:
    set attack to 0.005
    set release to 0.22
    set filterCutoff to 7800 Hz
    set reverbMix to 0.08
    set delayMix to 0.05
    set volume to 0.88
)" },

            // --- Macros (can't do with one binding) ---
            { "Macros", "Tone Macro (3 targets)",
              "One knob fans out to filter, delay, and reverb with different curves",
              R"(# Attach this to a Tone / Macro knob mapped to filterCutoff,
# OR keep filterCutoff as the driver and let it move the FX sends.
# One control → three different musical ranges (not a 1:1 binding).
when knob "filterCutoff" moves:
    let tone = value mapped 20 Hz..20000 Hz -> 0.0..1.0
    set delayMix to tone mapped 0.0..1.0 -> 0.0..0.40
    set reverbMix to tone mapped 0.0..1.0 -> 0.05..0.55
    set attack to tone mapped 0.0..1.0 -> 0.005..0.45
)" },
            { "Macros", "Inverse FX Balance",
              "Raise delay and automatically pull reverb down (and vice versa)",
              R"(# Useful "send balance" you cannot get from a single binding.
when knob "delayMix" moves:
    set reverbMix to value mapped 0.0..1.0 -> 0.45..0.02
)" },
            { "Macros", "Delay Safety Clamp",
              "When delay gets wet, pull feedback down so it never runs away",
              R"(# Performance safety: wet delay + high feedback is dangerous.
when knob "delayMix" moves:
    if value > 0.55:
        set delayFeedback to 0.25
    else:
        set delayFeedback to value mapped 0.0..0.55 -> 0.15..0.55
)" },

            // --- MIDI / performance ---
            { "MIDI", "Velocity Dynamics",
              "Soft notes dark/dry, hard notes bright/wet",
              R"(# Play soft vs hard — the instrument responds differently.
# Impossible with a static filter binding alone.
when note starts:
    if velocity > 100:
        set filterCutoff to 11000 Hz
        set reverbMix to 0.28
        set delayMix to 0.18
    else:
        if velocity > 60:
            set filterCutoff to 4200 Hz
            set reverbMix to 0.16
            set delayMix to 0.08
        else:
            set filterCutoff to 900 Hz
            set reverbMix to 0.06
            set delayMix to 0.02
)" },
            { "MIDI", "Ghost Note Mute",
              "Very soft notes duck volume so ghosts stay in the background",
              R"(# Ghost notes stay quiet without a separate sample layer.
when note starts:
    if velocity < 35:
        set volume to 0.25
        set filterCutoff to 700 Hz
    else:
        set volume to velocity mapped 35..127 -> 0.55..1.0
        set filterCutoff to velocity mapped 35..127 -> 1800 Hz..9000 Hz
)" },
            { "MIDI", "Key Track Filter",
              "Higher notes open the filter — classic synth behaviour",
              R"(# Filter follows MIDI note number (C2 dark → C6 bright).
when note starts:
    set filterCutoff to note mapped 36..96 -> 600 Hz..12000 Hz
    set volume to velocity mapped 0..127 -> 0.45..1.0
)" },
            { "MIDI", "Velocity Envelope",
              "Harder hits get faster attack and longer release",
              R"(# Envelope shape follows how hard you play.
when note starts:
    set attack to velocity mapped 0..127 -> 0.12..0.004
    set release to velocity mapped 0..127 -> 0.18..1.4
    set filterCutoff to velocity mapped 0..127 -> 1200 Hz..9000 Hz
)" },
            { "MIDI", "Mod Wheel Performance",
              "Mod wheel opens filter and adds delay together",
              R"(# One expression control → brightness + space.
when modwheel moves:
    set filterCutoff to modwheel mapped 0..127 -> 800 Hz..14000 Hz
    set delayMix to modwheel mapped 0..127 -> 0.0..0.40
    set reverbMix to modwheel mapped 0..127 -> 0.05..0.35
)" },
            { "MIDI", "Note-Off Tail Bloom",
              "Reverb swells when you release; next note resets it",
              R"(# FX send reacts to note-off — not possible with envelope alone.
when note ends:
    set reverbMix to 0.52
when note starts:
    set reverbMix to 0.14
)" },

            // --- Motion / timers ---
            { "Motion", "Living Pad Drift",
              "Slow random filter motion for evolving pads",
              R"(# Organic movement with no LFO module required.
when timer 500 ms:
    randomize filterCutoff between 900 Hz and 3200 Hz
)" },
            { "Motion", "Breathing Space",
              "Timer gently pulses reverb for ambient beds",
              R"(# Subtle ambient motion.
when timer 750 ms:
    randomize reverbMix between 0.18 and 0.42
)" },

            // --- Controls (simple but still useful) ---
            { "Controls", "Knob Macro (simple)",
              "Map one control onto another range",
              R"(when knob "filterCutoff" moves:
    set delayMix to value mapped 20 Hz..20000 Hz -> 0.0..0.35
)" },
            { "Debug", "Print Note + Velocity",
              "Log note/velocity to the pScript console while you play",
              R"(when note starts:
    print note
    print velocity
)" }
        };

        templatePresetBox.clear();
        int id = 1;
        juce::String lastCategory;
        for (const auto& t : scriptTemplates)
        {
            if (t.category != lastCategory)
            {
                templatePresetBox.addSectionHeading (t.category);
                lastCategory = t.category;
            }
            templatePresetBox.addItem (t.name, id++);
        }
    }

    void ScriptEditorComponent::insertSelectedTemplate()
    {
        const int id = templatePresetBox.getSelectedId();
        if (id <= 0 || id > (int) scriptTemplates.size())
            return;
        const auto& t = scriptTemplates[(size_t) (id - 1)];
        insertSnippetAndCompile (t.insertText);
        statusLog.setColour (juce::TextEditor::textColourId, juce::Colours::lightblue);
        statusLog.setText ("Inserted \"" + t.name + "\" — " + t.description + "\n", false);
        templatePresetBox.setSelectedId (0, juce::dontSendNotification);
    }

} // namespace patchcraft
