#include "InspectorPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "CanvasEditor.h"

#include <algorithm>
#include <memory>

namespace patchcraft
{
    static void styleLabel (juce::Label& l, juce::String text,
                            juce::Justification j = juce::Justification::centredLeft,
                            float fontSize = 12.0f, bool bold = false)
    {
        l.setText (std::move (text), juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        l.setFont (juce::Font (fontSize, bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
    }

    static juce::String colourToHex (juce::Colour colour)
    {
        return "#" + colour.toDisplayString (true);
    }

    static juce::Colour colourFromHex (juce::String text, juce::Colour fallback)
    {
        text = text.trim().removeCharacters ("#");
        if (text.length() == 6)
            text = "ff" + text;
        if (text.length() != 8)
            return fallback;
        return juce::Colour ((juce::uint32) text.getHexValue64());
    }

    static bool isContainerElement (ElementType type)
    {
        return type == ElementType::Panel || type == ElementType::Group || type == ElementType::TabPanel;
    }

    static juce::String scopedTabGroupId (const LayoutElement& tabPanel, const juce::String& label)
    {
        if (tabPanel.id == "tabs")
            return LayoutElement::tabLabelToGroupId (label);
        return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
    }

    static juce::String extractIdFromComboText (const juce::String& text)
    {
        const auto open = text.lastIndexOfChar ('(');
        const auto close = text.lastIndexOfChar (')');
        return open >= 0 && close > open ? text.substring (open + 1, close).trim() : text.trim();
    }

    static juce::StringArray linesPreservingChannelSlots (juce::String text)
    {
        juce::StringArray lines;
        text = text.replace ("\r\n", "\n").replace ("\r", "\n");
        int start = 0;
        while (start <= text.length())
        {
            const int next = text.indexOf (start, "\n");
            if (next < 0)
            {
                lines.add (text.substring (start).trim());
                break;
            }
            lines.add (text.substring (start, next).trim());
            start = next + 1;
        }

        while (! lines.isEmpty() && lines[lines.size() - 1].isEmpty())
            lines.remove (lines.size() - 1);
        return lines;
    }

    static bool isElementChildOfContainer (const LayoutElement& child, const LayoutElement& container)
    {
        if (child.id == container.id)
            return false;

        if (container.type == ElementType::TabPanel)
        {
            for (const auto& tab : container.tabs)
                if (child.groupId == scopedTabGroupId (container, tab))
                    return true;
            return false;
        }

        return child.containerId == container.id || child.groupId == container.id;
    }

    static bool parameterIsEnabled (const PatchCraftProject& project, const ParameterDef& parameter)
    {
        if (parameter.enabledBy.isEmpty())
            return true;

        const auto* gate = project.getParameters().find (parameter.enabledBy);
        const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
        const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
        return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
    }

    static juce::String parameterTooltip (const PatchCraftProject& project, const ParameterDef& parameter)
    {
        juce::String text = parameter.name + " (" + parameter.id + ")";
        text += "\nSection: " + parameter.section + " / " + parameter.category;
        text += "\nRange: " + juce::String (parameter.min, 3) + " to " + juce::String (parameter.max, 3);
        if (parameter.unit.isNotEmpty())
            text += " " + parameter.unit;
        if (! parameter.hostAutomatable)
            text += "\nNot exposed for host automation.";
        if (! parameter.midiLearnable)
            text += "\nMIDI learn is disabled for this parameter.";
        if (! parameter.modulatable)
            text += "\nCannot be used as a modulation target.";
        if (! parameterIsEnabled (project, parameter))
            text += "\nDisabled: " + (parameter.enableHint.isNotEmpty()
                ? parameter.enableHint
                : ("Enable " + parameter.enabledBy + " first."));
        return text;
    }

    static void setComponentTooltip (juce::Component& component, const juce::String& text)
    {
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (&component))
            tip->setTooltip (text);
    }

    static bool isDrumMachineBlock (const DspBlock& block)
    {
        return block.type.containsIgnoreCase ("drum")
            || block.values.find ("dmTracks") != block.values.end()
            || block.values.find ("dmSteps") != block.values.end();
    }

    static float valueForBlockKey (const DspBlock& block, const juce::String& key, float fallback)
    {
        const auto it = block.values.find (key);
        return it != block.values.end() ? it->second : fallback;
    }

    static juce::String defaultDrumTrackLabel (int track)
    {
        static const char* labels[] =
        {
            "Kick", "Snare", "Closed Hat", "Open Hat",
            "Clap", "Low Tom", "Perc", "Crash",
            "Ride", "Rim", "Shaker", "FX",
            "Pad 13", "Pad 14", "Pad 15", "Pad 16"
        };
        return track >= 0 && track < 16 ? juce::String (labels[track])
                                        : "Track " + juce::String (track + 1);
    }

    static int defaultDrumTrackNote (int track)
    {
        static const int notes[] =
        {
            36, 38, 42, 46, 39, 45, 48, 49,
            51, 37, 44, 52, 53, 54, 55, 56
        };
        return track >= 0 && track < 16 ? notes[track] : 36 + track;
    }

    InspectorPanel::InspectorPanel (StudioMainComponent& o) : owner (o)
    {
        styleLabel (header, "INSPECTOR", juce::Justification::centredLeft, 12.0f, true);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (header);

        auto styleSection = [] (juce::Label& label, const juce::String& text)
        {
            styleLabel (label, "v  " + text, juce::Justification::centredLeft, 11.0f, true);
            label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
            label.setColour (juce::Label::backgroundColourId, PatchCraftLookAndFeel::raised().withAlpha (0.65f));
            label.setColour (juce::Label::outlineColourId, PatchCraftLookAndFeel::border().withAlpha (0.75f));
            label.setInterceptsMouseClicks (false, false);
        };

        styleSection (lblLayoutSection, "LAYOUT");
        styleSection (lblParameterSection, "PARAMETER BINDING");
        styleSection (lblStyleSection, "VISUAL STYLE");
        styleSection (lblSpecialSection, "ADVANCED ELEMENT TOOLS");

        styleLabel (lblType,        "Type");
        styleLabel (lblId,          "ID");
        styleLabel (lblPos,         "Position");
        styleLabel (lblSize,        "Size");
        styleLabel (lblParam,       "Parameter");
        styleLabel (lblLabel,       "Label");
        styleLabel (lblValFmt,      "Value Format");
        styleLabel (lblStyle,       "Style");
        styleLabel (lblKnobStyle,   "Knob Style");
        styleLabel (lblOpacity,     "Opacity");
        styleLabel (lblState,       "State");
        styleLabel (lblShapeKind,   "Shape");
        styleLabel (lblCorner,      "Corner");
        styleLabel (lblStroke,      "Stroke");
        styleLabel (lblShadow,      "Shadow");
        styleLabel (lblGlow,        "Glow");
        styleLabel (lblBlur,        "Blur");
        styleLabel (lblAudioReactive, "Audio");
        styleLabel (lblAudioReactiveMode, "React Mode");
        styleLabel (lblAudioReactiveAmount, "React Amt");
        styleLabel (lblAnimationMode, "Anim");
        styleLabel (lblAnimationRate, "Anim Rate");
        styleLabel (lblLabelPosition, "Label Pos");
        styleLabel (lblLabelOffsetX,  "Label X");
        styleLabel (lblLabelOffsetY,  "Label Y");
        styleLabel (lblLabelSpacing,  "Spacing");
        styleLabel (lblLabelSize,     "Text Size");
        styleLabel (lblBgColour,      "BG Color");
        styleLabel (lblBorderColour,  "Border");
        styleLabel (lblAccentColour,  "Accent");
        styleLabel (lblMin,         "Min");
        styleLabel (lblMax,         "Max");
        styleLabel (lblDefault,     "Default");
        styleLabel (lblStep,        "Step");
        styleLabel (lblValType,     "Value Type");
        styleLabel (lblSmoothing,   "Smoothing");
        styleLabel (lblPosX,        "X", juce::Justification::centred, 11.0f);
        styleLabel (lblPosY,        "Y", juce::Justification::centred, 11.0f);
        styleLabel (lblSizeW,       "W", juce::Justification::centred, 11.0f);
        styleLabel (lblSizeH,       "H", juce::Justification::centred, 11.0f);
        styleLabel (lblAction,      "Action");
        styleLabel (lblActions,     "ACTIONS", juce::Justification::centredLeft, 11.0f, true);
        lblActions.setInterceptsMouseClicks (false, false);
        styleLabel (lblAsset,       "Asset");
        styleLabel (lblGroup,       "Container");
        styleLabel (lblTabs,        "Tabs");
        styleLabel (lblContainerManager, "CONTAINER MANAGER", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblContainerChildren, "Children");
        styleLabel (lblFilmstripPath,   "Knob Image");
        styleLabel (lblFilmstripFrames, "Frames");
        styleLabel (lblDrumGrid,        "DRUM GRID", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblDrumPattern,     "Pattern");
        styleLabel (lblDrumTracks,      "Tracks");
        styleLabel (lblDrumSteps,       "Steps");
        styleLabel (lblDrumCell,        "Cell");
        styleLabel (lblDrumVelocity,    "Velocity");
        styleLabel (lblDrumGate,        "Gate");
        styleLabel (lblDrumProbability, "Chance");
        styleLabel (lblDrumDivision,    "Division");
        styleLabel (lblDrumPadFxTarget, "Pad FX");
        styleLabel (lblDrumPadFxAmount, "Pad FX Amt");
        styleLabel (lblDrumCellFxTarget, "Cell FX");
        styleLabel (lblDrumCellFxAmount, "Cell FX Amt");
        styleLabel (lblMixer,           "MIXER", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblMixerMode,       "Mode");
        styleLabel (lblMixerChannels,   "Channels");
        styleLabel (lblMixerLabels,     "Labels");
        styleLabel (lblMixerVolumes,    "Volume Params");
        styleLabel (lblMixerPans,       "Pan Params");
        styleLabel (lblMixerMutes,      "Mute Params");
        styleLabel (lblMixerSolos,      "Solo Params");
        styleLabel (lblMacroEditor,     "MACRO ROUTING", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblMacroTargets,    "Targets");
        styleLabel (lblModMatrixEditor, "MOD MATRIX", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblModRoutes,       "Routes");
        styleLabel (lblGranularEditor,  "GRANULAR ENGINE", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblGranularDirection, "Direction");
        styleLabel (lblGranularDensity, "Density");
        styleLabel (lblGranularSize,    "Size");
        styleLabel (lblGranularRandom,  "Random");
        styleLabel (lblGranularSpread,  "Spread");
        styleLabel (lblGranularScan,    "Scan");
        styleLabel (lblGranularPitch,   "Pitch");
        styleLabel (lblGranularPan,     "Pan");
        styleLabel (lblGranularTexture, "Texture");

        for (auto* l : { &lblType, &lblId, &lblPos, &lblSize, &lblParam, &lblLabel, &lblAction,
                         &lblValFmt, &lblStyle, &lblKnobStyle, &lblMin, &lblMax,
                         &lblLayoutSection, &lblParameterSection, &lblStyleSection, &lblSpecialSection,
                         &lblOpacity, &lblState, &lblShapeKind, &lblCorner, &lblStroke,
                         &lblShadow, &lblGlow, &lblBlur, &lblAudioReactive,
                         &lblAudioReactiveMode, &lblAudioReactiveAmount,
                         &lblAnimationMode, &lblAnimationRate,
                         &lblLabelPosition, &lblLabelOffsetX, &lblLabelOffsetY,
                         &lblLabelSpacing, &lblLabelSize, &lblBgColour, &lblBorderColour, &lblAccentColour,
                         &lblDefault, &lblStep, &lblValType, &lblSmoothing,
                         &lblActions, &lblPosX, &lblPosY, &lblSizeW, &lblSizeH,
                         &lblAsset, &lblGroup, &lblTabs, &lblContainerManager, &lblContainerChildren,
                         &lblFilmstripPath, &lblFilmstripFrames,
                         &lblDrumGrid, &lblDrumPattern, &lblDrumTracks, &lblDrumSteps,
                         &lblDrumCell, &lblDrumVelocity, &lblDrumGate, &lblDrumProbability,
                          &lblDrumDivision, &lblDrumPadFxTarget, &lblDrumPadFxAmount,
                          &lblDrumCellFxTarget, &lblDrumCellFxAmount,
                          &lblMixer, &lblMixerMode, &lblMixerChannels, &lblMixerLabels,
                          &lblMixerVolumes, &lblMixerPans, &lblMixerMutes, &lblMixerSolos,
                          &lblMacroEditor, &lblMacroTargets, &lblModMatrixEditor, &lblModRoutes,
                          &lblGranularEditor, &lblGranularDirection, &lblGranularDensity,
                          &lblGranularSize, &lblGranularRandom, &lblGranularSpread,
                          &lblGranularScan, &lblGranularPitch, &lblGranularPan,
                          &lblGranularTexture })
        {
            addAndMakeVisible (*l);
            if (l == &lblDrumGrid || l == &lblMixer || l == &lblMacroEditor
                || l == &lblModMatrixEditor || l == &lblGranularEditor || l == &lblContainerManager)
                l->setInterceptsMouseClicks (false, false);
        }

        // Type combo
        const char* types[] = { "Image", "Knob", "Slider", "Button", "Toggle",
                                "Dropdown", "Label", "Value Display", "Meter",
                                "Waveform", "Keyboard", "Panel", "Shape", "XY Pad",
                                "Granular Field",
                                "Tab Panel", "Scroll Panel", "Group", "Separator",
                                "Drum Pad", "Pad Grid", "Drum Grid", "Mixer",
                                "Macro Control", "Mod Matrix" };
        int id = 1;
        for (auto* t : types) typeBox.addItem (t, id++);
        addAndMakeVisible (typeBox);

        addAndMakeVisible (idEdit);
        addAndMakeVisible (xEdit);
        addAndMakeVisible (yEdit);
        addAndMakeVisible (wEdit);
        addAndMakeVisible (hEdit);

        for (auto* e : { &idEdit, &xEdit, &yEdit, &wEdit, &hEdit })
            e->setIndents (6, 4);

        addAndMakeVisible (parameterBox);
        midiLearnButton.getProperties().set ("smallButton", true);
        midiLearnButton.setTooltip ("Start MIDI Learn for the selected parameter. Switch to Test and move a hardware control.");
        midiLearnButton.onClick = [this]
        {
            if (auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId()))
                if (el->parameterId.isNotEmpty())
                    owner.beginMidiLearn (el->parameterId);
        };
        addAndMakeVisible (midiLearnButton);
        addAndMakeVisible (labelEdit);
        labelEdit.setIndents (6, 4);
        addAndMakeVisible (actionEdit);
        actionEdit.setIndents (6, 4);
        actionEdit.setTooltip ("Optional runtime action, e.g. showContainer:layers, toggleContainer:help. Works on Button, Toggle, Label, and Image elements.");

        addAndMakeVisible (assetEdit);
        assetEdit.setIndents (6, 4);
        addAndMakeVisible (browseAssetBtn);
        browseAssetBtn.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Choose image", juce::File(), "*.png;*.jpg;*.jpeg");
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File()) return;
                    assetEdit.setText (f.getFullPathName(), true);
                    writeFromUi();
                });
        };
        assetEdit.onTextChange = [this] { writeFromUi(); };

        addAndMakeVisible (containerBox);
        containerBox.onChange = [this] { writeFromUi(); };

        addAndMakeVisible (containerChildrenBox);
        for (auto* button : { &containerAddSelectedBtn, &containerRemoveChildBtn, &containerSelectChildBtn,
                              &containerAddTabBtn, &containerRemoveTabBtn })
        {
            button->getProperties().set ("fontSize", 10.5);
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }

        containerAddSelectedBtn.setTooltip ("Add the current multi-selection to this container. For Tab Panels, assets are placed on the active tab.");
        containerRemoveChildBtn.setTooltip ("Remove the selected child from this container without deleting it.");
        containerSelectChildBtn.setTooltip ("Select the chosen child on the canvas and in Layers.");
        containerAddTabBtn.setTooltip ("Add a new page to this Tab Panel.");
        containerRemoveTabBtn.setTooltip ("Remove the last tab from this Tab Panel. Assets on that tab move back to Main.");

        containerAddSelectedBtn.onClick = [this]
        {
            auto* container = owner.getProject().getLayout().find (owner.getSelectedElementId());
            if (container == nullptr || ! isContainerElement (container->type))
                return;

            const auto selectedIds = owner.getSelectedElementIds();
            auto targetTabGroup = container->tabs.isEmpty()
                ? juce::String ("main")
                : scopedTabGroupId (*container, container->tabs[0]);
            if (container->type == ElementType::TabPanel)
                if (auto* canvas = owner.getCanvasEditor())
                    if (auto active = canvas->getActiveTabGroupsByPanel().find (container->id);
                        active != canvas->getActiveTabGroupsByPanel().end())
                        targetTabGroup = active->second;

            for (const auto& id : selectedIds)
            {
                if (id == container->id)
                    continue;

                if (auto* child = owner.getProject().getLayout().find (id))
                {
                    if (container->type == ElementType::TabPanel)
                    {
                        child->groupId = targetTabGroup;
                        child->containerId.clear();
                    }
                    else
                    {
                        child->containerId = container->id;
                    }
                }
            }

            owner.getProject().notifyChanged();
            refresh();
        };

        containerRemoveChildBtn.onClick = [this]
        {
            const auto childId = extractIdFromComboText (containerChildrenBox.getText());
            if (auto* child = owner.getProject().getLayout().find (childId))
            {
                child->containerId.clear();
                child->groupId.clear();
                owner.getProject().notifyChanged();
                refresh();
            }
        };

        containerSelectChildBtn.onClick = [this]
        {
            const auto childId = extractIdFromComboText (containerChildrenBox.getText());
            if (owner.getProject().getLayout().find (childId) != nullptr)
                owner.setSelectedElementId (childId);
        };

        containerAddTabBtn.onClick = [this]
        {
            auto* tabPanel = owner.getProject().getLayout().find (owner.getSelectedElementId());
            if (tabPanel == nullptr || tabPanel->type != ElementType::TabPanel)
                return;

            auto* alert = new juce::AlertWindow ("Add Tab", "Tab name:", juce::MessageBoxIconType::NoIcon);
            alert->addTextEditor ("name", "New Tab", "Name");
            alert->addButton ("Add", 1);
            alert->addButton ("Cancel", 0);
            alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert, id = tabPanel->id] (int result)
            {
                const juce::String name = alert->getTextEditorContents ("name").trim();
                std::unique_ptr<juce::AlertWindow> owned (alert);
                if (result != 1 || name.isEmpty())
                    return;

                if (auto* item = owner.getProject().getLayout().find (id))
                {
                    item->tabs.addIfNotAlreadyThere (name);
                    owner.getProject().notifyChanged();
                    refresh();
                }
            }), true);
        };

        containerRemoveTabBtn.onClick = [this]
        {
            auto* tabPanel = owner.getProject().getLayout().find (owner.getSelectedElementId());
            if (tabPanel == nullptr || tabPanel->type != ElementType::TabPanel || tabPanel->tabs.size() <= 1)
                return;

            const auto removedGroup = scopedTabGroupId (*tabPanel, tabPanel->tabs[tabPanel->tabs.size() - 1]);
            tabPanel->tabs.remove (tabPanel->tabs.size() - 1);
            for (auto& item : owner.getProject().getLayout().getAll())
                if (item.groupId == removedGroup)
                    item.groupId = "main";

            owner.getProject().notifyChanged();
            refresh();
        };

        tabsEdit.setMultiLine (true, false);
        tabsEdit.setReturnKeyStartsNewLine (true);
        tabsEdit.setIndents (6, 4);
        tabsEdit.onTextChange = [this] { writeFromUi(); };
        addAndMakeVisible (tabsEdit);

        filmstripPathEdit.setIndents (6, 4);
        filmstripPathEdit.onTextChange = [this] { writeFromUi(); };
        addAndMakeVisible (filmstripPathEdit);

        filmstripBrowseBtn.onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Choose filmstrip PNG", juce::File(), "*.png");
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File()) return;
                    filmstripPathEdit.setText (f.getFullPathName(), true);

                    // Auto-fill frame count if user hasn't entered one.
                    if (filmstripFramesEdit.getText().getIntValue() <= 0)
                    {
                        if (auto img = juce::ImageFileFormat::loadFrom (f); img.isValid())
                        {
                            const int n = PatchCraftLookAndFeel::detectFilmstripFrames (img, true);
                            filmstripFramesEdit.setText (juce::String (n), true);
                        }
                    }
                    writeFromUi();
                });
        };
        addAndMakeVisible (filmstripBrowseBtn);

        filmstripFramesEdit.setIndents (6, 4);
        filmstripFramesEdit.setInputRestrictions (4, "0123456789");
        filmstripFramesEdit.onTextChange = [this] { writeFromUi(); };
        addAndMakeVisible (filmstripFramesEdit);

        filmstripAutoBtn.onClick = [this]
        {
            // Detect frame count from current filmstrip image.
            const auto path = filmstripPathEdit.getText();
            if (path.isEmpty()) return;
            juce::File f = juce::File::isAbsolutePath (path)
                ? juce::File (path)
                : owner.getProject().getProjectFolder().getChildFile (path);
            if (auto img = juce::ImageFileFormat::loadFrom (f); img.isValid())
            {
                const int n = PatchCraftLookAndFeel::detectFilmstripFrames (img, true);
                filmstripFramesEdit.setText (juce::String (n), true);
                writeFromUi();
            }
        };
        addAndMakeVisible (filmstripAutoBtn);

        for (int pattern = 1; pattern <= 8; ++pattern)
            drumPatternBox.addItem ("Pattern " + juce::String (pattern), pattern);
        for (int track = 0; track < 16; ++track)
            drumTrackBox.addItem (juce::String (track + 1) + "  " + defaultDrumTrackLabel (track), track + 1);
        for (int step = 1; step <= 64; ++step)
            drumStepBox.addItem ("Step " + juce::String (step), step);
        drumDivisionBox.addItem ("Single x1", 1);
        drumDivisionBox.addItem ("Double x2", 2);
        drumDivisionBox.addItem ("Triple x3", 3);
        drumDivisionBox.addItem ("Quad x4", 4);
        auto addDrumFxTargets = [] (juce::ComboBox& combo)
        {
            combo.addItem ("None", 1);
            combo.addItem ("Filter Throw", 2);
            combo.addItem ("Resonance Ping", 3);
            combo.addItem ("Drive Hit", 4);
            combo.addItem ("Delay Throw", 5);
            combo.addItem ("Reverb Throw", 6);
            combo.addItem ("Tape Smear", 7);
            combo.addItem ("Lo-Fi Crush", 8);
            combo.addItem ("Granular Texture", 9);
        };
        addDrumFxTargets (drumPadFxTargetBox);
        addDrumFxTargets (drumCellFxTargetBox);
        drumPadFxTargetBox.setTooltip ("Effect fired when this drum track is triggered from pads or MIDI notes.");
        drumCellFxTargetBox.setTooltip ("Effect fired only when the selected grid cell plays.");
        for (auto* combo : { &drumPatternBox, &drumTrackBox, &drumStepBox, &drumDivisionBox,
                             &drumPadFxTargetBox, &drumCellFxTargetBox })
        {
            combo->setTooltip ("Edits the selected Drum Grid and its connected drum-machine pattern.");
            addAndMakeVisible (*combo);
        }
        drumPadFxTargetBox.setTooltip ("Effect fired when this drum track is triggered from pads or MIDI notes.");
        drumCellFxTargetBox.setTooltip ("Effect fired only when the selected grid cell plays.");

        for (auto* slider : { &drumTracksSlider, &drumStepsSlider, &drumVelocitySlider,
                              &drumGateSlider, &drumProbabilitySlider,
                              &drumPadFxAmountSlider, &drumCellFxAmountSlider })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, true, 56, 22);
            addAndMakeVisible (*slider);
        }
        drumTracksSlider.setRange (1.0, 16.0, 1.0);
        drumStepsSlider.setRange (1.0, 64.0, 1.0);
        drumVelocitySlider.setRange (0.0, 100.0, 1.0);
        drumGateSlider.setRange (5.0, 100.0, 1.0);
        drumProbabilitySlider.setRange (0.0, 100.0, 1.0);
        drumPadFxAmountSlider.setRange (0.0, 100.0, 1.0);
        drumCellFxAmountSlider.setRange (0.0, 100.0, 1.0);
        drumVelocitySlider.setTextValueSuffix (" %");
        drumGateSlider.setTextValueSuffix (" %");
        drumProbabilitySlider.setTextValueSuffix (" %");
        drumPadFxAmountSlider.setTextValueSuffix (" %");
        drumCellFxAmountSlider.setTextValueSuffix (" %");

        drumCellEnabledToggle.setTooltip ("Turns the selected pattern cell on or off.");
        addAndMakeVisible (drumCellEnabledToggle);
        for (auto* button : { &drumApplyTrapRollBtn, &drumClearPatternBtn, &drumCopyPatternBtn,
                              &drumPastePatternBtn, &drumDuplicatePatternBtn, &drumOpenPerformanceBtn })
        {
            button->getProperties().set ("fontSize", 10.5);
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }
        drumApplyTrapRollBtn.setTooltip ("Adds musical x2/x3/x4 roll divisions to hats and percussion in the current pattern.");
        drumClearPatternBtn.setTooltip ("Clears all hits in the selected pattern.");
        drumCopyPatternBtn.setTooltip ("Copies the selected pattern's hits, velocities, gates, probabilities, and divisions.");
        drumPastePatternBtn.setTooltip ("Pastes the copied pattern into the current pattern.");
        drumDuplicatePatternBtn.setTooltip ("Copies the current pattern to the next pattern slot.");
        drumOpenPerformanceBtn.setTooltip ("Opens MIDI Playground for full drum-machine editing and playback.");

        valueFormatBox.addItem ("Auto",     1);
        valueFormatBox.addItem ("0.00",     2);
        valueFormatBox.addItem ("4.2 kHz",  3);
        valueFormatBox.addItem ("dB",       4);
        valueFormatBox.addItem ("%",        5);
        valueFormatBox.addItem ("ms",       6);
        addAndMakeVisible (valueFormatBox);

        for (auto* sName : { "Modern Dark", "Vintage Gold", "Gold Dark",
                             "Space Blue", "Minimal Flat", "Glass" })
            styleBox.addItem (sName, styleBox.getNumItems() + 1);
        addAndMakeVisible (styleBox);

        for (int i = 1; i <= 8; ++i)
            knobStyleBox.addItem ("Vintage 0" + juce::String (i), i);
        addAndMakeVisible (knobStyleBox);

        opacitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
        opacitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 48, 22);
        opacitySlider.setRange (0.0, 100.0, 1.0);
        opacitySlider.setTextValueSuffix (" %");
        addAndMakeVisible (opacitySlider);
        addAndMakeVisible (visibleToggle);
        addAndMakeVisible (lockedToggle);

        shapeKindBox.addItem ("Rounded Rect", 1);
        shapeKindBox.addItem ("Ellipse", 2);
        shapeKindBox.addItem ("Triangle", 3);
        shapeKindBox.addItem ("Diamond", 4);
        shapeKindBox.addItem ("Line", 5);
        addAndMakeVisible (shapeKindBox);
        for (auto* slider : { &cornerSlider, &strokeSlider, &shadowSlider, &glowSlider, &blurSlider })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, true, 48, 22);
            addAndMakeVisible (*slider);
        }
        cornerSlider.setRange (0.0, 80.0, 1.0);
        strokeSlider.setRange (0.0, 16.0, 0.5);
        shadowSlider.setRange (0.0, 100.0, 1.0);
        glowSlider.setRange (0.0, 100.0, 1.0);
        blurSlider.setRange (0.0, 100.0, 1.0);
        addAndMakeVisible (audioReactiveToggle);

        audioReactiveModeBox.addItem ("Level", 1);
        audioReactiveModeBox.addItem ("Peak", 2);
        audioReactiveModeBox.addItem ("Low Band", 3);
        audioReactiveModeBox.addItem ("Mid Band", 4);
        audioReactiveModeBox.addItem ("High Band", 5);
        audioReactiveModeBox.addItem ("Transient", 6);
        addAndMakeVisible (audioReactiveModeBox);

        audioReactiveAmountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        audioReactiveAmountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 48, 22);
        audioReactiveAmountSlider.setRange (0.0, 100.0, 1.0);
        audioReactiveAmountSlider.setTextValueSuffix (" %");
        addAndMakeVisible (audioReactiveAmountSlider);

        animationModeBox.addItem ("None", 1);
        animationModeBox.addItem ("Pulse", 2);
        animationModeBox.addItem ("Breathe", 3);
        animationModeBox.addItem ("Shake", 4);
        animationModeBox.addItem ("Glow", 5);
        addAndMakeVisible (animationModeBox);

        animationRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        animationRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 54, 22);
        animationRateSlider.setRange (0.05, 16.0, 0.05);
        animationRateSlider.setTextValueSuffix (" Hz");
        addAndMakeVisible (animationRateSlider);

        labelPositionBox.addItem ("Bottom", 1);
        labelPositionBox.addItem ("Top", 2);
        labelPositionBox.addItem ("Left", 3);
        labelPositionBox.addItem ("Right", 4);
        labelPositionBox.addItem ("Hidden", 5);
        addAndMakeVisible (labelPositionBox);
        for (auto* slider : { &labelOffsetXSlider, &labelOffsetYSlider, &labelSpacingSlider, &labelSizeSlider })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, true, 52, 22);
            addAndMakeVisible (*slider);
        }
        labelOffsetXSlider.setRange (-120.0, 120.0, 1.0);
        labelOffsetYSlider.setRange (-120.0, 120.0, 1.0);
        labelSpacingSlider.setRange (-40.0, 80.0, 1.0);
        labelSizeSlider.setRange (0.0, 48.0, 1.0);

        for (auto* edit : { &backgroundColourEdit, &borderColourEdit, &accentColourEdit })
        {
            edit->setIndents (6, 4);
            addAndMakeVisible (*edit);
        }
        for (auto* button : { &backgroundColourButton, &borderColourButton, &accentColourButton })
        {
            button->getProperties().set ("smallButton", true);
            button->getProperties().set ("fontSize", 10.5);
            addAndMakeVisible (*button);
        }
        backgroundColourButton.onClick = [this] { showColourPicker ("Background Color", backgroundColourEdit, colourFromHex (backgroundColourEdit.getText(), juce::Colours::transparentBlack)); };
        borderColourButton.onClick = [this] { showColourPicker ("Border Color", borderColourEdit, colourFromHex (borderColourEdit.getText(), juce::Colours::transparentBlack)); };
        accentColourButton.onClick = [this] { showColourPicker ("Accent Color", accentColourEdit, colourFromHex (accentColourEdit.getText(), juce::Colours::transparentBlack)); };

        addAndMakeVisible (minEdit);
        addAndMakeVisible (maxEdit);
        addAndMakeVisible (defaultEdit);
        addAndMakeVisible (stepEdit);
        for (auto* e : { &minEdit, &maxEdit, &defaultEdit, &stepEdit })
            e->setIndents (6, 4);

        valueTypeBox.addItem ("Audio", 1);
        valueTypeBox.addItem ("MIDI",  2);
        valueTypeBox.addItem ("Custom",3);
        addAndMakeVisible (valueTypeBox);

        smoothingSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        smoothingSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 50, 22);
        smoothingSlider.setRange (0.0, 200.0, 1.0);
        smoothingSlider.setTextValueSuffix (" ms");
        addAndMakeVisible (smoothingSlider);

        for (auto* b : { &btnDuplicate, &btnCopy, &btnCopyNoParams, &btnPaste,
                          &btnAllTabs, &btnDelete, &btnForward, &btnBackward })
        {
            b->getProperties().set ("fontSize", 10.5);
            b->getProperties().set ("toolbarIcon", true);
            b->getProperties().set ("corner", 5.0);
            addAndMakeVisible (*b);
        }
        btnDuplicate.setTooltip ("Duplicate");
        btnCopy.setTooltip      ("Copy selected elements with parameter bindings.");
        btnCopyNoParams.setTooltip ("Copy selected elements but clear parameter bindings on paste.");
        btnPaste.setTooltip     ("Paste copied elements.");
        btnAllTabs.setTooltip   ("Copy selected tab-scoped elements to every tab in the same Tab Panel.");
        btnDelete.setTooltip    ("Delete");
        btnForward.setTooltip   ("Bring Forward");
        btnBackward.setTooltip  ("Send Backward");
        copyTabsAsReferenceToggle.setTooltip ("When enabled, Copy Selection To All Tabs creates linked copies. Moving or editing one linked copy updates the matching copies on the other tabs.");
        addAndMakeVisible (copyTabsAsReferenceToggle);

        btnDuplicate.onClick = [this]
        {
            owner.duplicateSelected();
        };
        btnCopy.onClick = [this] { owner.copySelectedElements (true); };
        btnCopyNoParams.onClick = [this] { owner.copySelectedElements (false); };
        btnPaste.onClick = [this] { owner.pasteCopiedElements(); };
        btnAllTabs.onClick = [this] { owner.copySelectedToAllTabs(); };
        copyTabsAsReferenceToggle.onClick = [this]
        {
            owner.setCopySelectionToTabsAsReference (copyTabsAsReferenceToggle.getToggleState());
        };
        btnDelete.onClick = [this]
        {
            owner.deleteSelected();
        };
        btnForward.onClick = [this]
        {
            owner.orderSelected ("forward");
        };
        btnBackward.onClick = [this]
        {
            owner.orderSelected ("backward");
        };

        // Wire change callbacks
        auto rewrite = [this] { writeFromUi(); };
        idEdit.onTextChange      = rewrite;
        xEdit.onTextChange       = rewrite;
        yEdit.onTextChange       = rewrite;
        wEdit.onTextChange       = rewrite;
        hEdit.onTextChange       = rewrite;
        labelEdit.onTextChange   = rewrite;
        actionEdit.onTextChange  = rewrite;
        minEdit.onTextChange     = rewrite;
        maxEdit.onTextChange     = rewrite;
        defaultEdit.onTextChange = rewrite;
        stepEdit.onTextChange    = rewrite;
        typeBox.onChange         = rewrite;
        parameterBox.onChange    = rewrite;
        valueFormatBox.onChange  = rewrite;
        styleBox.onChange        = rewrite;
        knobStyleBox.onChange    = rewrite;
        opacitySlider.onValueChange = rewrite;
        visibleToggle.onClick    = rewrite;
        lockedToggle.onClick     = rewrite;
        shapeKindBox.onChange    = rewrite;
        cornerSlider.onValueChange = rewrite;
        strokeSlider.onValueChange = rewrite;
        shadowSlider.onValueChange = rewrite;
        glowSlider.onValueChange = rewrite;
        blurSlider.onValueChange = rewrite;
        audioReactiveToggle.onClick = rewrite;
        audioReactiveModeBox.onChange = rewrite;
        audioReactiveAmountSlider.onValueChange = rewrite;
        animationModeBox.onChange = rewrite;
        animationRateSlider.onValueChange = rewrite;
        labelPositionBox.onChange = rewrite;
        labelOffsetXSlider.onValueChange = rewrite;
        labelOffsetYSlider.onValueChange = rewrite;
        labelSpacingSlider.onValueChange = rewrite;
        labelSizeSlider.onValueChange = rewrite;
        backgroundColourEdit.onTextChange = rewrite;
        borderColourEdit.onTextChange = rewrite;
        accentColourEdit.onTextChange = rewrite;
        valueTypeBox.onChange    = rewrite;
        smoothingSlider.onValueChange = rewrite;

        drumPatternBox.onChange = [this]
        {
            if (inhibitCallbacks) return;
            writeDrumGridElementFromUi();
            refreshDrumControls();
        };
        drumTracksSlider.onValueChange = [this]
        {
            if (inhibitCallbacks) return;
            writeDrumGridElementFromUi();
            refreshDrumControls();
        };
        drumStepsSlider.onValueChange = [this]
        {
            if (inhibitCallbacks) return;
            writeDrumGridElementFromUi();
            refreshDrumControls();
        };
        drumTrackBox.onChange = [this] { if (! inhibitCallbacks) refreshDrumControls(); };
        drumStepBox.onChange = [this] { if (! inhibitCallbacks) refreshDrumControls(); };
        drumDivisionBox.onChange = [this] { writeDrumCellFromUi(); };
        drumPadFxTargetBox.onChange = [this] { writeDrumTrackFxFromUi(); };
        drumPadFxAmountSlider.onValueChange = [this] { writeDrumTrackFxFromUi(); };
        drumCellFxTargetBox.onChange = [this] { writeDrumCellFromUi(); };
        drumCellFxAmountSlider.onValueChange = [this] { writeDrumCellFromUi(); };
        drumVelocitySlider.onValueChange = [this] { writeDrumCellFromUi(); };
        drumGateSlider.onValueChange = [this] { writeDrumCellFromUi(); };
        drumProbabilitySlider.onValueChange = [this] { writeDrumCellFromUi(); };
        drumCellEnabledToggle.onClick = [this] { writeDrumCellFromUi(); };
        drumApplyTrapRollBtn.onClick = [this] { applyTrapRollToCurrentPattern(); };
        drumClearPatternBtn.onClick = [this] { clearCurrentDrumPattern(); };
        drumCopyPatternBtn.onClick = [this] { copyCurrentDrumPattern(); };
        drumPastePatternBtn.onClick = [this] { pasteCurrentDrumPattern(); };
        drumDuplicatePatternBtn.onClick = [this] { duplicateCurrentDrumPatternToNext(); };
        drumOpenPerformanceBtn.onClick = [this] { owner.setBottomTab (BottomPanel::Page::MidiPlayground); };

        mixerModeBox.addItem ("Auto - layers when available", 1);
        mixerModeBox.addItem ("Layers only", 2);
        mixerModeBox.addItem ("Parameter channels", 3);
        mixerModeBox.setTooltip ("Auto uses loaded Player layers when this is a multi-instrument pack; Parameter channels use the explicit parameter lists below.");
        addAndMakeVisible (mixerModeBox);

        mixerHelpLabel.setText ("Parameter mixer setup:\n"
                                "1. Set Mode to Parameter channels and choose Channels.\n"
                                "2. Enter one parameter id per line, e.g. volume, delayMix, reverbMix.\n"
                                "3. Use right-click > Find Parameter for valid ids, or Break Selected Mixer Into Channels for separate strips.",
                                juce::dontSendNotification);
        mixerHelpLabel.setFont (juce::Font (10.5f));
        mixerHelpLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        mixerHelpLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (mixerHelpLabel);

        mixerChannelsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        mixerChannelsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 48, 22);
        mixerChannelsSlider.setRange (1.0, 16.0, 1.0);
        mixerChannelsSlider.setTooltip ("Number of mixer strips shown when not auto-expanding to multi-instrument layers.");
        addAndMakeVisible (mixerChannelsSlider);

        for (auto* editor : { &mixerLabelsEdit, &mixerVolumeParamsEdit, &mixerPanParamsEdit,
                              &mixerMuteParamsEdit, &mixerSoloParamsEdit })
        {
            editor->setMultiLine (true, false);
            editor->setReturnKeyStartsNewLine (true);
            editor->setIndents (6, 4);
            addAndMakeVisible (*editor);
        }
        mixerLabelsEdit.setTooltip ("One channel label per line. Example: Main, Drums, Pads, FX Bus.");
        mixerVolumeParamsEdit.setTooltip ("One volume parameter id per line. Channel 1 defaults to volume if left empty.");
        mixerPanParamsEdit.setTooltip ("One pan parameter id per line. Channel 1 defaults to pan if left empty.");
        mixerMuteParamsEdit.setTooltip ("Optional toggle parameter ids for mute buttons, one per line.");
        mixerSoloParamsEdit.setTooltip ("Optional toggle parameter ids for solo buttons, one per line.");
        mixerModeBox.onChange = rewrite;
        mixerChannelsSlider.onValueChange = rewrite;
        mixerLabelsEdit.onTextChange = rewrite;
        mixerVolumeParamsEdit.onTextChange = rewrite;
        mixerPanParamsEdit.onTextChange = rewrite;
        mixerMuteParamsEdit.onTextChange = rewrite;
        mixerSoloParamsEdit.onTextChange = rewrite;

        for (auto* editor : { &macroTargetsEdit, &modRoutesEdit })
        {
            editor->setMultiLine (true, false);
            editor->setReturnKeyStartsNewLine (true);
            editor->setIndents (6, 4);
            addAndMakeVisible (*editor);
        }
        macroTargetsEdit.setTooltip ("One target per line: targetParameterId targetMin targetMax curve. Example: filterCutoff 400 8000 1.4");
        modRoutesEdit.setTooltip ("One route per line: sourceId -> targetParameterId amount smoothing enabled. Example: lfo_1 -> filterCutoff 0.25 0.02 on");

        for (auto* button : { &macroApplyBtn, &macroClearBtn, &modApplyBtn, &modClearBtn })
        {
            button->getProperties().set ("fontSize", 10.5);
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }
        macroApplyBtn.setTooltip ("Apply the macro target list to the selected Macro Control.");
        macroClearBtn.setTooltip ("Remove all targets driven by the selected Macro Control.");
        modApplyBtn.setTooltip ("Apply these modulation routes to the DSP graph.");
        modClearBtn.setTooltip ("Clear all modulation routes from the DSP graph.");
        macroApplyBtn.onClick = [this] { writeMacroTargetsFromUi(); };
        macroClearBtn.onClick = [this]
        {
            macroTargetsEdit.clear();
            writeMacroTargetsFromUi();
        };
        modApplyBtn.onClick = [this] { writeModRoutesFromUi(); };
        modClearBtn.onClick = [this]
        {
            modRoutesEdit.clear();
            writeModRoutesFromUi();
        };

        granularDirectionBox.addItem ("Forward", 1);
        granularDirectionBox.addItem ("Reverse", 2);
        granularDirectionBox.addItem ("Ping-Pong", 3);
        granularDirectionBox.addItem ("Multi", 4);
        granularDirectionBox.setTooltip ("Playback motion for grain scanning. Multi adds layered opposing grain motion.");
        addAndMakeVisible (granularDirectionBox);

        auto setupGranularSlider = [this] (juce::Slider& slider, double min, double max, double step,
                                           const juce::String& suffix, const juce::String& tip)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 58, 22);
            slider.setRange (min, max, step);
            slider.setTextValueSuffix (suffix);
            slider.setTooltip (tip);
            addAndMakeVisible (slider);
        };

        setupGranularSlider (granularDensitySlider, 0.5, 220.0, 0.5, " g/s", "Grains per second. Low values pulse; high values become smooth clouds.");
        setupGranularSlider (granularSizeSlider, 2.0, 1000.0, 1.0, " ms", "Individual grain duration.");
        setupGranularSlider (granularRandomSlider, 0.0, 100.0, 1.0, " %", "Random variation in grain size.");
        setupGranularSlider (granularSpreadSlider, 0.0, 100.0, 1.0, " %", "How wide the grain cloud sprays around the playhead.");
        setupGranularSlider (granularScanSlider, -3.0, 3.0, 0.01, "x", "Autonomous scan speed. Negative scans backward.");
        setupGranularSlider (granularPitchSlider, 0.0, 36.0, 0.1, " st", "Random pitch spread in semitones.");
        setupGranularSlider (granularPanSlider, 0.0, 100.0, 1.0, " %", "Stereo position spread.");
        setupGranularSlider (granularTextureSlider, 0.0, 100.0, 1.0, " %", "Adds grit, density irregularity, and cloud complexity.");

        for (auto* toggle : { &granularOnToggle, &granularFreezeToggle, &granularReverseToggle })
        {
            toggle->setTooltip ("Granular runtime switch. These values are exported and affect Player playback.");
            addAndMakeVisible (*toggle);
        }

        auto granularRewrite = [this] { writeGranularControlsFromUi(); };
        granularDirectionBox.onChange = granularRewrite;
        granularDensitySlider.onValueChange = granularRewrite;
        granularSizeSlider.onValueChange = granularRewrite;
        granularRandomSlider.onValueChange = granularRewrite;
        granularSpreadSlider.onValueChange = granularRewrite;
        granularScanSlider.onValueChange = granularRewrite;
        granularPitchSlider.onValueChange = granularRewrite;
        granularPanSlider.onValueChange = granularRewrite;
        granularTextureSlider.onValueChange = granularRewrite;
        granularOnToggle.onClick = granularRewrite;
        granularFreezeToggle.onClick = granularRewrite;
        granularReverseToggle.onClick = granularRewrite;

        refresh();
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 0, 1, getHeight());
    }

    bool InspectorPanel::isSectionOpen (InspectorSection section) const
    {
        return sectionOpen[(size_t) section];
    }

    void InspectorPanel::mouseDown (const juce::MouseEvent& event)
    {
        for (size_t i = 0; i < sectionHeaderBounds.size(); ++i)
        {
            if (! sectionHeaderBounds[i].contains (event.getPosition()))
                continue;

            sectionOpen[i] = ! sectionOpen[i];
            resized();
            repaint();
            return;
        }
    }

    void InspectorPanel::layoutRow (juce::Rectangle<int>& area, juce::Label& l,
                                    juce::Component* control, int height)
    {
        auto row = area.removeFromTop (height + 8);
        l.setBounds (row.removeFromLeft (110).reduced (10, 4));
        if (control != nullptr)
            control->setBounds (row.reduced (4, 4));
    }

    void InspectorPanel::layoutColourRow (juce::Rectangle<int>& area, juce::Label& l,
                                          juce::TextEditor& editor, juce::TextButton& button)
    {
        auto row = area.removeFromTop (34);
        l.setBounds (row.removeFromLeft (110).reduced (10, 4));
        button.setBounds (row.removeFromRight (52).reduced (4, 4));
        editor.setBounds (row.reduced (4, 4));
    }

    void InspectorPanel::showColourPicker (const juce::String& title,
                                           juce::TextEditor& target,
                                           juce::Colour current)
    {
        auto* alert = new juce::AlertWindow (title,
            "Choose a color. The field stores RGBA as #AARRGGBB.",
            juce::MessageBoxIconType::NoIcon);
        auto selector = std::make_shared<juce::ColourSelector> (
            juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showColourspace
            | juce::ColourSelector::showAlphaChannel);
        selector->setName (title);
        selector->setCurrentColour (current);
        selector->setSize (360, 300);
        alert->addCustomComponent (selector.get());
        alert->addButton ("Apply", 1);
        alert->addButton ("Cancel", 0);
        auto* targetEditor = &target;
        alert->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [this, alert, selector, targetEditor] (int result)
                {
                    std::unique_ptr<juce::AlertWindow> owned (alert);
                    if (result != 1)
                        return;

                    targetEditor->setText (colourToHex (selector->getCurrentColour()), juce::dontSendNotification);
                    writeFromUi();
                    refresh();
                }), true);
    }

    void InspectorPanel::layoutDoubleRow (juce::Rectangle<int>& area,
                                          juce::Label& title, juce::Component& c1, juce::String,
                                          juce::Label& title2, juce::Component& c2,
                                          int height)
    {
        auto row = area.removeFromTop (height + 8);
        title.setBounds (row.removeFromLeft (110).reduced (10, 4));
        const int half = row.getWidth() / 2;
        auto left  = row.removeFromLeft (half).reduced (4, 4);
        auto right = row.reduced (4, 4);

        // For position/size: small "X"/"Y" mini label inside left edge
        title2.setBounds (left.removeFromLeft (16));
        c1.setBounds (left);
        // Right column second mini label
        // (placed externally by caller through specific labels)
        c2.setBounds (right);
    }

    void InspectorPanel::resized()
    {
        auto r = getLocalBounds();
        header.setBounds (r.removeFromTop (32).reduced (10, 8));
        r.reduce (4, 0);

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        const auto type = el != nullptr ? el->type : ElementType::Knob;
        const bool hasParam = (type == ElementType::Knob || type == ElementType::Slider
                               || type == ElementType::Meter || type == ElementType::Toggle
                               || type == ElementType::ValueDisplay
                               || type == ElementType::MacroControl);
        const bool isImage  = (type == ElementType::Image);
        const bool isLabel  = (type == ElementType::Label);
        const bool isShape  = (type == ElementType::Shape || type == ElementType::Panel);
        const bool isDrumGrid = (type == ElementType::DrumGrid);
        const bool isMixer = (type == ElementType::Mixer);
        const bool isMacroControl = (type == ElementType::MacroControl);
        const bool isModMatrix = (type == ElementType::ModMatrix);
        const bool isGranularField = (type == ElementType::GranularField);
        const bool isTabPanel = (type == ElementType::TabPanel);
        const bool canAnimate = (type != ElementType::Group && type != ElementType::Separator);
        const bool hasStyle = (type == ElementType::Knob || type == ElementType::Slider
                               || type == ElementType::Meter || type == ElementType::Button
                               || type == ElementType::Panel || type == ElementType::Shape);
        const bool isKnob   = (type == ElementType::Knob);
        const bool showContainerManager = (el != nullptr && isContainerElement (type));
        const bool showFilmstrip = (type == ElementType::Knob
                                    || type == ElementType::Slider
                                    || type == ElementType::Meter);
        const bool showLabelLayout = (el != nullptr && (type == ElementType::Knob || type == ElementType::Slider
                                                        || type == ElementType::Button || type == ElementType::Toggle
                                                        || type == ElementType::Dropdown || type == ElementType::ValueDisplay
                                                        || type == ElementType::MacroControl));
        const bool showColour = (type == ElementType::Panel || type == ElementType::Group
                                 || type == ElementType::Shape || type == ElementType::Button
                                 || type == ElementType::Toggle || type == ElementType::Dropdown
                                  || type == ElementType::DrumGrid || type == ElementType::DrumPad
                                  || type == ElementType::PadGrid || type == ElementType::Mixer
                                  || type == ElementType::MacroControl || type == ElementType::ModMatrix
                                  || type == ElementType::GranularField);
        sectionHeaderBounds.fill ({});
        auto sectionHeader = [this, &r] (juce::Label& label, InspectorSection section, const juce::String& title)
        {
            label.setVisible (true);
            r.removeFromTop (6);
            const auto bounds = r.removeFromTop (24).reduced (6, 2);
            sectionHeaderBounds[(size_t) section] = bounds;
            label.setText ((isSectionOpen (section) ? "v  " : ">  ") + title, juce::dontSendNotification);
            label.setBounds (bounds);
        };
        auto setVisibleFor = [] (std::initializer_list<juce::Component*> components, bool visible)
        {
            for (auto* component : components)
                if (component != nullptr)
                    component->setVisible (visible);
        };

        // Common: type, id
        sectionHeader (lblLayoutSection, InspectorSection::Layout, "LAYOUT");
        const bool layoutOpen = isSectionOpen (InspectorSection::Layout);
        const bool supportsAction = el != nullptr
            && (type == ElementType::Button || type == ElementType::Toggle
                || type == ElementType::Label || type == ElementType::Image);
        setVisibleFor ({ &typeBox, &idEdit, &xEdit, &yEdit, &wEdit, &hEdit,
                         &labelEdit, &opacitySlider, &visibleToggle, &lockedToggle,
                         &lblType, &lblId, &lblPos, &lblSize, &lblLabel, &lblOpacity,
                         &lblState, &lblPosX, &lblPosY, &lblSizeW, &lblSizeH }, layoutOpen);
        actionEdit.setVisible (layoutOpen && supportsAction);
        lblAction.setVisible (layoutOpen && supportsAction);
        if (layoutOpen)
        {
        layoutRow (r, lblType, &typeBox);
        layoutRow (r, lblId,   &idEdit);

        // Position X/Y
        {
            auto row = r.removeFromTop (34);
            lblPos.setBounds (row.removeFromLeft (110).reduced (10, 4));
            const int half = row.getWidth() / 2;
            auto left  = row.removeFromLeft (half).reduced (4, 4);
            auto right = row.reduced (4, 4);
            lblPosX.setBounds (left.removeFromLeft (14));   xEdit.setBounds (left);
            lblPosY.setBounds (right.removeFromLeft (14));  yEdit.setBounds (right);
        }
        // Size W/H
        {
            auto row = r.removeFromTop (34);
            lblSize.setBounds (row.removeFromLeft (110).reduced (10, 4));
            const int half = row.getWidth() / 2;
            auto left  = row.removeFromLeft (half).reduced (4, 4);
            auto right = row.reduced (4, 4);
            lblSizeW.setBounds (left.removeFromLeft (14));  wEdit.setBounds (left);
            lblSizeH.setBounds (right.removeFromLeft (14)); hEdit.setBounds (right);
        }

        // Label is always shown - useful for buttons/panels too
        layoutRow (r, lblLabel, &labelEdit);
        if (supportsAction)
            layoutRow (r, lblAction, &actionEdit);
        layoutRow (r, lblOpacity, &opacitySlider);
        {
            auto row = r.removeFromTop (34);
            lblState.setBounds (row.removeFromLeft (110).reduced (10, 4));
            const int half = row.getWidth() / 2;
            visibleToggle.setBounds (row.removeFromLeft (half).reduced (4, 4));
            lockedToggle.setBounds (row.reduced (4, 4));
        }
        }

        // Parameter binding section
        const bool showParam = hasParam;
        const bool showParamControls = showParam && isSectionOpen (InspectorSection::Parameter);
        lblParameterSection.setVisible (showParam);
        parameterBox.setVisible (showParamControls);
        midiLearnButton.setVisible (showParamControls);
        valueFormatBox.setVisible (showParamControls);
        minEdit.setVisible (showParamControls);
        maxEdit.setVisible (showParamControls);
        defaultEdit.setVisible (showParamControls);
        stepEdit.setVisible (showParamControls);
        valueTypeBox.setVisible (showParamControls);
        smoothingSlider.setVisible (showParamControls);
        lblParam.setVisible (showParamControls);
        lblValFmt.setVisible (showParamControls);
        lblMin.setVisible (showParamControls);
        lblMax.setVisible (showParamControls);
        lblDefault.setVisible (showParamControls);
        lblStep.setVisible (showParamControls);
        lblValType.setVisible (showParamControls);
        lblSmoothing.setVisible (showParamControls);

        if (showParam)
        {
            sectionHeader (lblParameterSection, InspectorSection::Parameter, "PARAMETER BINDING");
            if (showParamControls)
            {
                layoutRow (r, lblParam,  &parameterBox);
                {
                    auto bounds = parameterBox.getBounds();
                    midiLearnButton.setBounds (bounds.removeFromRight (58).reduced (2));
                    parameterBox.setBounds (bounds.reduced (0, 0));
                }
                layoutRow (r, lblValFmt, &valueFormatBox);
                // Min / Max share a row
                {
                    auto row = r.removeFromTop (34);
                    lblMin.setBounds (row.removeFromLeft (110).reduced (10, 4));
                    const int half = row.getWidth() / 2;
                    minEdit.setBounds (row.removeFromLeft (half).reduced (4, 4));
                    maxEdit.setBounds (row.reduced (4, 4));
                }
                layoutRow (r, lblDefault,   &defaultEdit);
                layoutRow (r, lblStep,      &stepEdit);
                layoutRow (r, lblValType,   &valueTypeBox);
                layoutRow (r, lblSmoothing, &smoothingSlider);
            }
        }

        // Style section
        const bool showVisualSection = hasStyle || isKnob || isShape || canAnimate || isImage
                                    || (el != nullptr && ! el->locked)
                                    || showLabelLayout || showColour || showFilmstrip;
        const bool showVisualControls = showVisualSection && isSectionOpen (InspectorSection::Style);
        lblStyleSection.setVisible (showVisualSection);
        if (showVisualSection)
            sectionHeader (lblStyleSection, InspectorSection::Style, "VISUAL STYLE");

        styleBox.setVisible (hasStyle && showVisualControls);
        knobStyleBox.setVisible (isKnob && showVisualControls);
        lblStyle.setVisible (hasStyle && showVisualControls);
        lblKnobStyle.setVisible (isKnob && showVisualControls);
        if (showVisualControls && hasStyle)
            layoutRow (r, lblStyle, &styleBox);
        if (showVisualControls && isKnob)
            layoutRow (r, lblKnobStyle, &knobStyleBox);

        // Filmstrip override (Knob / Slider / Meter)
        filmstripPathEdit.setVisible   (showFilmstrip && showVisualControls);
        filmstripBrowseBtn.setVisible  (showFilmstrip && showVisualControls);
        filmstripFramesEdit.setVisible (showFilmstrip && showVisualControls);
        filmstripAutoBtn.setVisible    (showFilmstrip && showVisualControls);
        lblFilmstripPath.setVisible    (showFilmstrip && showVisualControls);
        lblFilmstripFrames.setVisible  (showFilmstrip && showVisualControls);
        if (showFilmstrip && showVisualControls)
        {
            auto row = r.removeFromTop (34);
            lblFilmstripPath.setBounds (row.removeFromLeft (110).reduced (10, 4));
            filmstripBrowseBtn.setBounds (row.removeFromRight (80).reduced (4, 4));
            filmstripPathEdit.setBounds  (row.reduced (4, 4));

            auto row2 = r.removeFromTop (34);
            lblFilmstripFrames.setBounds (row2.removeFromLeft (110).reduced (10, 4));
            filmstripAutoBtn.setBounds   (row2.removeFromRight (60).reduced (4, 4));
            filmstripFramesEdit.setBounds (row2.reduced (4, 4));
        }

        shapeKindBox.setVisible (isShape && showVisualControls);
        cornerSlider.setVisible (isShape && showVisualControls);
        strokeSlider.setVisible (isShape && showVisualControls);
        shadowSlider.setVisible (isShape && showVisualControls);
        glowSlider.setVisible (isShape && showVisualControls);
        blurSlider.setVisible (isShape && showVisualControls);
        lblShapeKind.setVisible (isShape && showVisualControls);
        lblCorner.setVisible (isShape && showVisualControls);
        lblStroke.setVisible (isShape && showVisualControls);
        lblShadow.setVisible (isShape && showVisualControls);
        lblGlow.setVisible (isShape && showVisualControls);
        lblBlur.setVisible (isShape && showVisualControls);
        if (isShape && showVisualControls)
        {
            if (type == ElementType::Shape)
                layoutRow (r, lblShapeKind, &shapeKindBox);
            layoutRow (r, lblCorner, &cornerSlider);
            layoutRow (r, lblStroke, &strokeSlider);
            layoutRow (r, lblShadow, &shadowSlider);
            layoutRow (r, lblGlow, &glowSlider);
            layoutRow (r, lblBlur, &blurSlider);
        }

        const bool showAudioDetail = canAnimate && el != nullptr && el->audioReactive;
        const bool showAnimationRate = canAnimate && el != nullptr
            && el->animationMode.isNotEmpty() && el->animationMode != "none";
        audioReactiveToggle.setVisible (canAnimate && showVisualControls);
        audioReactiveModeBox.setVisible (showAudioDetail && showVisualControls);
        audioReactiveAmountSlider.setVisible (showAudioDetail && showVisualControls);
        animationModeBox.setVisible (canAnimate && showVisualControls);
        animationRateSlider.setVisible (showAnimationRate && showVisualControls);
        lblAudioReactive.setVisible (canAnimate && showVisualControls);
        lblAudioReactiveMode.setVisible (showAudioDetail && showVisualControls);
        lblAudioReactiveAmount.setVisible (showAudioDetail && showVisualControls);
        lblAnimationMode.setVisible (canAnimate && showVisualControls);
        lblAnimationRate.setVisible (showAnimationRate && showVisualControls);
        if (canAnimate && showVisualControls)
        {
            layoutRow (r, lblAudioReactive, &audioReactiveToggle);
            if (showAudioDetail)
            {
                layoutRow (r, lblAudioReactiveMode, &audioReactiveModeBox);
                layoutRow (r, lblAudioReactiveAmount, &audioReactiveAmountSlider);
            }
            layoutRow (r, lblAnimationMode, &animationModeBox);
            if (showAnimationRate)
                layoutRow (r, lblAnimationRate, &animationRateSlider);
        }

        // Image-only: asset path + browse
        assetEdit.setVisible (isImage && showVisualControls);
        browseAssetBtn.setVisible (isImage && showVisualControls);
        lblAsset.setVisible (isImage && showVisualControls);
        if (isImage && showVisualControls)
        {
            auto row = r.removeFromTop (34);
            lblAsset.setBounds (row.removeFromLeft (110).reduced (10, 4));
            browseAssetBtn.setBounds (row.removeFromRight (80).reduced (4, 4));
            assetEdit.setBounds (row.reduced (4, 4));
        }

        // Group ID (any non-locked element)
        const bool showGroup = (el != nullptr && ! el->locked);
        containerBox.setVisible (showGroup && showVisualControls);
        lblGroup.setVisible (showGroup && showVisualControls);
        if (showGroup && showVisualControls)
            layoutRow (r, lblGroup, &containerBox);

        for (auto* component : { static_cast<juce::Component*> (&labelPositionBox),
                                 static_cast<juce::Component*> (&labelOffsetXSlider),
                                 static_cast<juce::Component*> (&labelOffsetYSlider),
                                 static_cast<juce::Component*> (&labelSpacingSlider),
                                 static_cast<juce::Component*> (&labelSizeSlider) })
            component->setVisible (showLabelLayout && showVisualControls);
        for (auto* label : { &lblLabelPosition, &lblLabelOffsetX, &lblLabelOffsetY, &lblLabelSpacing, &lblLabelSize })
            label->setVisible (showLabelLayout && showVisualControls);
        if (showLabelLayout && showVisualControls)
        {
            layoutRow (r, lblLabelPosition, &labelPositionBox);
            layoutRow (r, lblLabelOffsetX, &labelOffsetXSlider);
            layoutRow (r, lblLabelOffsetY, &labelOffsetYSlider);
            layoutRow (r, lblLabelSpacing, &labelSpacingSlider);
            layoutRow (r, lblLabelSize, &labelSizeSlider);
        }

        for (auto* component : { static_cast<juce::Component*> (&backgroundColourEdit),
                                 static_cast<juce::Component*> (&borderColourEdit),
                                 static_cast<juce::Component*> (&accentColourEdit),
                                 static_cast<juce::Component*> (&backgroundColourButton),
                                 static_cast<juce::Component*> (&borderColourButton),
                                 static_cast<juce::Component*> (&accentColourButton) })
            component->setVisible (showColour && showVisualControls);
        for (auto* label : { &lblBgColour, &lblBorderColour, &lblAccentColour })
            label->setVisible (showColour && showVisualControls);
        if (showColour && showVisualControls)
        {
            layoutColourRow (r, lblBgColour, backgroundColourEdit, backgroundColourButton);
            layoutColourRow (r, lblBorderColour, borderColourEdit, borderColourButton);
            layoutColourRow (r, lblAccentColour, accentColourEdit, accentColourButton);
        }

        const bool showSpecialSection = isDrumGrid || isMixer || isMacroControl || isModMatrix
                                     || isGranularField || isTabPanel || showContainerManager;
        const bool showAdvancedControls = showSpecialSection && isSectionOpen (InspectorSection::Advanced);
        lblSpecialSection.setVisible (showSpecialSection);
        if (showSpecialSection)
            sectionHeader (lblSpecialSection, InspectorSection::Advanced, "ADVANCED ELEMENT TOOLS");

        for (auto* component : { static_cast<juce::Component*> (&drumPatternBox),
                                 static_cast<juce::Component*> (&drumTrackBox),
                                 static_cast<juce::Component*> (&drumStepBox),
                                 static_cast<juce::Component*> (&drumDivisionBox),
                                 static_cast<juce::Component*> (&drumPadFxTargetBox),
                                 static_cast<juce::Component*> (&drumCellFxTargetBox),
                                 static_cast<juce::Component*> (&drumTracksSlider),
                                 static_cast<juce::Component*> (&drumStepsSlider),
                                 static_cast<juce::Component*> (&drumVelocitySlider),
                                 static_cast<juce::Component*> (&drumGateSlider),
                                 static_cast<juce::Component*> (&drumProbabilitySlider),
                                 static_cast<juce::Component*> (&drumPadFxAmountSlider),
                                 static_cast<juce::Component*> (&drumCellFxAmountSlider),
                                 static_cast<juce::Component*> (&drumCellEnabledToggle),
                                 static_cast<juce::Component*> (&drumApplyTrapRollBtn),
                                 static_cast<juce::Component*> (&drumClearPatternBtn),
                                 static_cast<juce::Component*> (&drumCopyPatternBtn),
                                 static_cast<juce::Component*> (&drumPastePatternBtn),
                                 static_cast<juce::Component*> (&drumDuplicatePatternBtn),
                                 static_cast<juce::Component*> (&drumOpenPerformanceBtn) })
            component->setVisible (isDrumGrid && showAdvancedControls && isSectionOpen (InspectorSection::DrumGrid));
        for (auto* label : { &lblDrumGrid, &lblDrumPattern, &lblDrumTracks, &lblDrumSteps,
                             &lblDrumCell, &lblDrumVelocity, &lblDrumGate,
                             &lblDrumProbability, &lblDrumDivision, &lblDrumPadFxTarget,
                             &lblDrumPadFxAmount, &lblDrumCellFxTarget, &lblDrumCellFxAmount })
            label->setVisible (isDrumGrid && showAdvancedControls
                               && (label == &lblDrumGrid || isSectionOpen (InspectorSection::DrumGrid)));

        if (isDrumGrid && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblDrumGrid, InspectorSection::DrumGrid, "DRUM GRID / FX TRIGGERS");
            if (isSectionOpen (InspectorSection::DrumGrid))
            {
                layoutRow (r, lblDrumPattern, &drumPatternBox);
                layoutRow (r, lblDrumTracks, &drumTracksSlider);
                layoutRow (r, lblDrumSteps, &drumStepsSlider);

                {
                    auto row = r.removeFromTop (34);
                    lblDrumCell.setBounds (row.removeFromLeft (110).reduced (10, 4));
                    const int half = row.getWidth() / 2;
                    drumTrackBox.setBounds (row.removeFromLeft (half).reduced (4, 4));
                    drumStepBox.setBounds (row.reduced (4, 4));
                }

                layoutRow (r, lblDrumDivision, &drumDivisionBox);
                layoutRow (r, lblDrumVelocity, &drumVelocitySlider);
                layoutRow (r, lblDrumGate, &drumGateSlider);
                layoutRow (r, lblDrumProbability, &drumProbabilitySlider);
                layoutRow (r, lblDrumPadFxTarget, &drumPadFxTargetBox);
                layoutRow (r, lblDrumPadFxAmount, &drumPadFxAmountSlider);
                layoutRow (r, lblDrumCellFxTarget, &drumCellFxTargetBox);
                layoutRow (r, lblDrumCellFxAmount, &drumCellFxAmountSlider);

                {
                    auto row = r.removeFromTop (34);
                    lblDrumCell.setBounds (row.removeFromLeft (110).reduced (10, 4));
                    drumCellEnabledToggle.setBounds (row.reduced (4, 4));
                }

                {
                    auto row = r.removeFromTop (34);
                    const int buttonW = juce::jmax (52, row.getWidth() / 3);
                    drumApplyTrapRollBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                    drumClearPatternBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                    drumOpenPerformanceBtn.setBounds (row.reduced (2));
                }
                {
                    auto row = r.removeFromTop (34);
                    const int buttonW = juce::jmax (52, row.getWidth() / 3);
                    drumCopyPatternBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                    drumPastePatternBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                    drumDuplicatePatternBtn.setBounds (row.reduced (2));
                }
            }
        }

        for (auto* component : { static_cast<juce::Component*> (&mixerModeBox),
                                 static_cast<juce::Component*> (&mixerChannelsSlider),
                                 static_cast<juce::Component*> (&mixerLabelsEdit),
                                 static_cast<juce::Component*> (&mixerVolumeParamsEdit),
                                 static_cast<juce::Component*> (&mixerPanParamsEdit),
                                 static_cast<juce::Component*> (&mixerMuteParamsEdit),
                                 static_cast<juce::Component*> (&mixerSoloParamsEdit),
                                 static_cast<juce::Component*> (&mixerHelpLabel) })
            component->setVisible (isMixer && showAdvancedControls && isSectionOpen (InspectorSection::Mixer));
        for (auto* label : { &lblMixer, &lblMixerMode, &lblMixerChannels, &lblMixerLabels,
                             &lblMixerVolumes, &lblMixerPans, &lblMixerMutes, &lblMixerSolos })
            label->setVisible (isMixer && showAdvancedControls
                               && (label == &lblMixer || isSectionOpen (InspectorSection::Mixer)));

        if (isMixer && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblMixer, InspectorSection::Mixer, "MIXER");
            if (isSectionOpen (InspectorSection::Mixer))
            {
                mixerHelpLabel.setBounds (r.removeFromTop (78).reduced (2, 4));
                layoutRow (r, lblMixerMode, &mixerModeBox);
                layoutRow (r, lblMixerChannels, &mixerChannelsSlider);
                layoutRow (r, lblMixerLabels, &mixerLabelsEdit, 64);
                layoutRow (r, lblMixerVolumes, &mixerVolumeParamsEdit, 56);
                layoutRow (r, lblMixerPans, &mixerPanParamsEdit, 56);
                layoutRow (r, lblMixerMutes, &mixerMuteParamsEdit, 48);
                layoutRow (r, lblMixerSolos, &mixerSoloParamsEdit, 48);
            }
        }

        for (auto* component : { static_cast<juce::Component*> (&macroTargetsEdit),
                                 static_cast<juce::Component*> (&macroApplyBtn),
                                 static_cast<juce::Component*> (&macroClearBtn) })
            component->setVisible (isMacroControl && showAdvancedControls && isSectionOpen (InspectorSection::Macro));
        for (auto* label : { &lblMacroEditor, &lblMacroTargets })
            label->setVisible (isMacroControl && showAdvancedControls
                               && (label == &lblMacroEditor || isSectionOpen (InspectorSection::Macro)));

        if (isMacroControl && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblMacroEditor, InspectorSection::Macro, "MACRO ROUTING");
            if (isSectionOpen (InspectorSection::Macro))
            {
                layoutRow (r, lblMacroTargets, &macroTargetsEdit, 96);
                auto row = r.removeFromTop (30).reduced (114, 3);
                const int buttonW = juce::jmax (58, row.getWidth() / 2);
                macroApplyBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                macroClearBtn.setBounds (row.reduced (2));
            }
        }

        for (auto* component : { static_cast<juce::Component*> (&modRoutesEdit),
                                 static_cast<juce::Component*> (&modApplyBtn),
                                 static_cast<juce::Component*> (&modClearBtn) })
            component->setVisible (isModMatrix && showAdvancedControls && isSectionOpen (InspectorSection::ModMatrix));
        for (auto* label : { &lblModMatrixEditor, &lblModRoutes })
            label->setVisible (isModMatrix && showAdvancedControls
                               && (label == &lblModMatrixEditor || isSectionOpen (InspectorSection::ModMatrix)));

        if (isModMatrix && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblModMatrixEditor, InspectorSection::ModMatrix, "MOD MATRIX");
            if (isSectionOpen (InspectorSection::ModMatrix))
            {
                layoutRow (r, lblModRoutes, &modRoutesEdit, 118);
                auto row = r.removeFromTop (30).reduced (114, 3);
                const int buttonW = juce::jmax (58, row.getWidth() / 2);
                modApplyBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                modClearBtn.setBounds (row.reduced (2));
            }
        }

        for (auto* component : { static_cast<juce::Component*> (&granularOnToggle),
                                 static_cast<juce::Component*> (&granularFreezeToggle),
                                 static_cast<juce::Component*> (&granularReverseToggle),
                                 static_cast<juce::Component*> (&granularDirectionBox),
                                 static_cast<juce::Component*> (&granularDensitySlider),
                                 static_cast<juce::Component*> (&granularSizeSlider),
                                 static_cast<juce::Component*> (&granularRandomSlider),
                                 static_cast<juce::Component*> (&granularSpreadSlider),
                                 static_cast<juce::Component*> (&granularScanSlider),
                                 static_cast<juce::Component*> (&granularPitchSlider),
                                 static_cast<juce::Component*> (&granularPanSlider),
                                 static_cast<juce::Component*> (&granularTextureSlider) })
            component->setVisible (isGranularField && showAdvancedControls && isSectionOpen (InspectorSection::Granular));
        for (auto* label : { &lblGranularEditor, &lblGranularDirection, &lblGranularDensity,
                             &lblGranularSize, &lblGranularRandom, &lblGranularSpread,
                             &lblGranularScan, &lblGranularPitch, &lblGranularPan,
                             &lblGranularTexture })
            label->setVisible (isGranularField && showAdvancedControls
                               && (label == &lblGranularEditor || isSectionOpen (InspectorSection::Granular)));

        if (isGranularField && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblGranularEditor, InspectorSection::Granular, "GRANULAR ENGINE");
            if (isSectionOpen (InspectorSection::Granular))
            {
            {
                auto row = r.removeFromTop (34);
                row.removeFromLeft (110);
                const int third = juce::jmax (70, row.getWidth() / 3);
                granularOnToggle.setBounds (row.removeFromLeft (third).reduced (4, 4));
                granularFreezeToggle.setBounds (row.removeFromLeft (third).reduced (4, 4));
                granularReverseToggle.setBounds (row.reduced (4, 4));
            }
            layoutRow (r, lblGranularDirection, &granularDirectionBox);
            layoutRow (r, lblGranularDensity, &granularDensitySlider);
            layoutRow (r, lblGranularSize, &granularSizeSlider);
            layoutRow (r, lblGranularRandom, &granularRandomSlider);
            layoutRow (r, lblGranularSpread, &granularSpreadSlider);
            layoutRow (r, lblGranularScan, &granularScanSlider);
            layoutRow (r, lblGranularPitch, &granularPitchSlider);
            layoutRow (r, lblGranularPan, &granularPanSlider);
            layoutRow (r, lblGranularTexture, &granularTextureSlider);
            }
        }

        // TabPanel: tab list editor (one tab label per line)
        tabsEdit.setVisible (isTabPanel && showAdvancedControls);
        lblTabs.setVisible (isTabPanel && showAdvancedControls);
        if (isTabPanel && showAdvancedControls)
        {
            auto row = r.removeFromTop (110);
            lblTabs.setBounds (row.removeFromLeft (110).reduced (10, 4));
            tabsEdit.setBounds (row.reduced (4, 4));
        }

        for (auto* component : { static_cast<juce::Component*> (&containerChildrenBox),
                                 static_cast<juce::Component*> (&containerAddSelectedBtn),
                                 static_cast<juce::Component*> (&containerRemoveChildBtn),
                                 static_cast<juce::Component*> (&containerSelectChildBtn),
                                 static_cast<juce::Component*> (&containerAddTabBtn),
                                 static_cast<juce::Component*> (&containerRemoveTabBtn) })
            component->setVisible (showContainerManager && showAdvancedControls && isSectionOpen (InspectorSection::Container));
        lblContainerManager.setVisible (showContainerManager && showAdvancedControls);
        lblContainerChildren.setVisible (showContainerManager && showAdvancedControls && isSectionOpen (InspectorSection::Container));
        containerAddTabBtn.setVisible (showContainerManager && showAdvancedControls && isSectionOpen (InspectorSection::Container) && isTabPanel);
        containerRemoveTabBtn.setVisible (showContainerManager && showAdvancedControls && isSectionOpen (InspectorSection::Container) && isTabPanel);

        if (showContainerManager && showAdvancedControls)
        {
            r.removeFromTop (6);
            sectionHeader (lblContainerManager, InspectorSection::Container, "CONTAINER MANAGER");
            if (isSectionOpen (InspectorSection::Container))
            {
                layoutRow (r, lblContainerChildren, &containerChildrenBox);
                auto row = r.removeFromTop (34);
                const int buttonW = juce::jmax (52, row.getWidth() / (isTabPanel ? 5 : 3));
                containerAddSelectedBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                containerRemoveChildBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                containerSelectChildBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                if (isTabPanel)
                {
                    containerAddTabBtn.setBounds (row.removeFromLeft (buttonW).reduced (2));
                    containerRemoveTabBtn.setBounds (row.reduced (2));
                }
            }
        }

        juce::ignoreUnused (isLabel);

        // Actions
        sectionHeader (lblActions, InspectorSection::Actions, "ACTIONS");
        const bool actionsOpen = isSectionOpen (InspectorSection::Actions);
        for (auto* component : { static_cast<juce::Component*> (&btnDuplicate),
                                 static_cast<juce::Component*> (&btnCopy),
                                 static_cast<juce::Component*> (&btnCopyNoParams),
                                 static_cast<juce::Component*> (&btnPaste),
                                 static_cast<juce::Component*> (&btnAllTabs),
                                 static_cast<juce::Component*> (&btnDelete),
                                 static_cast<juce::Component*> (&btnForward),
                                 static_cast<juce::Component*> (&btnBackward),
                                 static_cast<juce::Component*> (&copyTabsAsReferenceToggle) })
            component->setVisible (actionsOpen);
        if (actionsOpen)
        {
            auto actions = r.removeFromTop (30).reduced (10, 3);
            const int aw = juce::jmax (48, juce::jmin (68, actions.getWidth() / 4));
            btnDuplicate.setBounds (actions.removeFromLeft (aw));
            btnCopy.setBounds      (actions.removeFromLeft (aw));
            btnPaste.setBounds     (actions.removeFromLeft (aw));
            btnAllTabs.setBounds   (actions.removeFromLeft (aw));
            auto actions2 = r.removeFromTop (30).reduced (10, 3);
            btnCopyNoParams.setBounds (actions2.removeFromLeft (aw * 2).reduced (0, 1));
            btnDelete.setBounds    (actions2.removeFromLeft (aw).reduced (0, 1));
            btnForward.setBounds   (actions2.removeFromLeft (aw).reduced (0, 1));
            btnBackward.setBounds  (actions2.reduced (0, 1));
            copyTabsAsReferenceToggle.setBounds (r.removeFromTop (26).reduced (10, 2));
        }
    }

    void InspectorPanel::selectionChanged()
    {
        refresh();
        // resized() decides which fields are visible based on the selected
        // element's type (Image -> Asset field, TabPanel -> Tabs editor, etc.)
        // so we have to re-run it whenever the selection changes.
        resized();
    }

    void InspectorPanel::refresh()
    {
        const juce::ScopedValueSetter<bool> s (inhibitCallbacks, true);

        // Re-populate parameter combo. Use dontSendNotification on clear so the
        // (otherwise async) onChange callback does not re-enter writeFromUi.
        parameterBox.clear (juce::dontSendNotification);
        parameterBox.addItem ("(none)", 1);
        int pid = 2;
        for (auto& p : owner.getProject().getParameters().getAll())
        {
            if (! p.visible)
                continue;
            parameterBox.addItem (p.name + "  [" + p.section + "]  (" + p.id + ")", pid++);
        }

        containerBox.clear (juce::dontSendNotification);
        containerBox.addItem ("(none)", 1);
        containerChildrenBox.clear (juce::dontSendNotification);
        containerChildrenBox.addItem ("(no children)", 1);
        int containerItemId = 2;
        for (const auto& candidate : owner.getProject().getLayout().getAll())
        {
            if (candidate.type == ElementType::Panel || candidate.type == ElementType::Group)
                containerBox.addItem ((candidate.label.isNotEmpty() ? candidate.label : candidate.id) + "  (" + candidate.id + ")",
                                      containerItemId++);
            if (candidate.type == ElementType::TabPanel)
                for (const auto& tab : candidate.tabs)
                    containerBox.addItem ((candidate.label.isNotEmpty() ? candidate.label : candidate.id)
                                          + " / " + tab + "  (" + scopedTabGroupId (candidate, tab) + ")",
                                          containerItemId++);
        }

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        const bool enabled = (el != nullptr);

        const std::initializer_list<juce::Component*> allControls {
            static_cast<juce::Component*> (&typeBox),
            static_cast<juce::Component*> (&idEdit),
            static_cast<juce::Component*> (&xEdit),
            static_cast<juce::Component*> (&yEdit),
            static_cast<juce::Component*> (&wEdit),
            static_cast<juce::Component*> (&hEdit),
            static_cast<juce::Component*> (&parameterBox),
            static_cast<juce::Component*> (&midiLearnButton),
            static_cast<juce::Component*> (&labelEdit),
            static_cast<juce::Component*> (&actionEdit),
            static_cast<juce::Component*> (&valueFormatBox),
            static_cast<juce::Component*> (&styleBox),
            static_cast<juce::Component*> (&knobStyleBox),
            static_cast<juce::Component*> (&opacitySlider),
            static_cast<juce::Component*> (&visibleToggle),
            static_cast<juce::Component*> (&lockedToggle),
            static_cast<juce::Component*> (&shapeKindBox),
            static_cast<juce::Component*> (&cornerSlider),
            static_cast<juce::Component*> (&strokeSlider),
            static_cast<juce::Component*> (&shadowSlider),
            static_cast<juce::Component*> (&glowSlider),
            static_cast<juce::Component*> (&blurSlider),
            static_cast<juce::Component*> (&audioReactiveToggle),
            static_cast<juce::Component*> (&audioReactiveModeBox),
            static_cast<juce::Component*> (&audioReactiveAmountSlider),
            static_cast<juce::Component*> (&animationModeBox),
            static_cast<juce::Component*> (&animationRateSlider),
            static_cast<juce::Component*> (&containerBox),
            static_cast<juce::Component*> (&containerChildrenBox),
            static_cast<juce::Component*> (&containerAddSelectedBtn),
            static_cast<juce::Component*> (&containerRemoveChildBtn),
            static_cast<juce::Component*> (&containerSelectChildBtn),
            static_cast<juce::Component*> (&containerAddTabBtn),
            static_cast<juce::Component*> (&containerRemoveTabBtn),
            static_cast<juce::Component*> (&labelPositionBox),
            static_cast<juce::Component*> (&labelOffsetXSlider),
            static_cast<juce::Component*> (&labelOffsetYSlider),
            static_cast<juce::Component*> (&labelSpacingSlider),
            static_cast<juce::Component*> (&labelSizeSlider),
            static_cast<juce::Component*> (&backgroundColourEdit),
            static_cast<juce::Component*> (&borderColourEdit),
            static_cast<juce::Component*> (&accentColourEdit),
            static_cast<juce::Component*> (&backgroundColourButton),
            static_cast<juce::Component*> (&borderColourButton),
            static_cast<juce::Component*> (&accentColourButton),
            static_cast<juce::Component*> (&minEdit),
            static_cast<juce::Component*> (&maxEdit),
            static_cast<juce::Component*> (&defaultEdit),
            static_cast<juce::Component*> (&stepEdit),
            static_cast<juce::Component*> (&valueTypeBox),
            static_cast<juce::Component*> (&smoothingSlider),
            static_cast<juce::Component*> (&drumPatternBox),
            static_cast<juce::Component*> (&drumTrackBox),
            static_cast<juce::Component*> (&drumStepBox),
            static_cast<juce::Component*> (&drumDivisionBox),
            static_cast<juce::Component*> (&drumTracksSlider),
            static_cast<juce::Component*> (&drumStepsSlider),
            static_cast<juce::Component*> (&drumVelocitySlider),
            static_cast<juce::Component*> (&drumGateSlider),
            static_cast<juce::Component*> (&drumProbabilitySlider),
            static_cast<juce::Component*> (&drumCellEnabledToggle),
            static_cast<juce::Component*> (&drumApplyTrapRollBtn),
            static_cast<juce::Component*> (&drumClearPatternBtn),
            static_cast<juce::Component*> (&drumCopyPatternBtn),
            static_cast<juce::Component*> (&drumPastePatternBtn),
            static_cast<juce::Component*> (&drumDuplicatePatternBtn),
            static_cast<juce::Component*> (&drumOpenPerformanceBtn),
            static_cast<juce::Component*> (&mixerModeBox),
            static_cast<juce::Component*> (&mixerChannelsSlider),
            static_cast<juce::Component*> (&mixerLabelsEdit),
            static_cast<juce::Component*> (&mixerVolumeParamsEdit),
            static_cast<juce::Component*> (&mixerPanParamsEdit),
            static_cast<juce::Component*> (&mixerMuteParamsEdit),
            static_cast<juce::Component*> (&mixerSoloParamsEdit),
            static_cast<juce::Component*> (&mixerHelpLabel),
            static_cast<juce::Component*> (&macroTargetsEdit),
            static_cast<juce::Component*> (&macroApplyBtn),
            static_cast<juce::Component*> (&macroClearBtn),
            static_cast<juce::Component*> (&modRoutesEdit),
            static_cast<juce::Component*> (&modApplyBtn),
            static_cast<juce::Component*> (&modClearBtn),
            static_cast<juce::Component*> (&granularOnToggle),
            static_cast<juce::Component*> (&granularFreezeToggle),
            static_cast<juce::Component*> (&granularReverseToggle),
            static_cast<juce::Component*> (&granularDirectionBox),
            static_cast<juce::Component*> (&granularDensitySlider),
            static_cast<juce::Component*> (&granularSizeSlider),
            static_cast<juce::Component*> (&granularRandomSlider),
            static_cast<juce::Component*> (&granularSpreadSlider),
            static_cast<juce::Component*> (&granularScanSlider),
            static_cast<juce::Component*> (&granularPitchSlider),
            static_cast<juce::Component*> (&granularPanSlider),
            static_cast<juce::Component*> (&granularTextureSlider),
            static_cast<juce::Component*> (&btnDuplicate),
            static_cast<juce::Component*> (&btnCopy),
            static_cast<juce::Component*> (&btnCopyNoParams),
            static_cast<juce::Component*> (&btnPaste),
            static_cast<juce::Component*> (&btnAllTabs),
            static_cast<juce::Component*> (&copyTabsAsReferenceToggle),
            static_cast<juce::Component*> (&btnDelete),
            static_cast<juce::Component*> (&btnForward),
            static_cast<juce::Component*> (&btnBackward)
        };
        auto setTip = [] (juce::Component* component, const juce::String& text)
        {
            if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (component))
                tip->setTooltip (text);
        };
        for (auto* c : allControls)
        {
            c->setEnabled (enabled);
            setTip (c, enabled ? juce::String() : "Select an element on the canvas or in Layers to enable this Inspector control.");
        }
        btnPaste.setEnabled (owner.hasCopiedElements());
        copyTabsAsReferenceToggle.setToggleState (owner.getCopySelectionToTabsAsReference(), juce::dontSendNotification);

        if (! enabled)
        {
            idEdit.setText ("(no selection)", juce::dontSendNotification);
            xEdit.setText ("", juce::dontSendNotification);
            yEdit.setText ("", juce::dontSendNotification);
            wEdit.setText ("", juce::dontSendNotification);
            hEdit.setText ("", juce::dontSendNotification);
            labelEdit.setText ("", juce::dontSendNotification);
            actionEdit.setText ("", juce::dontSendNotification);
            return;
        }

        if (isContainerElement (el->type))
        {
            int childItemId = 2;
            for (const auto& candidate : owner.getProject().getLayout().getAll())
            {
                if (! isElementChildOfContainer (candidate, *el))
                    continue;

                const auto label = candidate.label.isNotEmpty() ? candidate.label : elementTypeDisplayName (candidate.type);
                containerChildrenBox.addItem (label + "  (" + candidate.id + ")", childItemId++);
            }
            containerChildrenBox.setSelectedId (containerChildrenBox.getNumItems() > 1 ? 2 : 1, juce::dontSendNotification);
        }

        typeBox.setSelectedId ((int) el->type + 1, juce::dontSendNotification);
        idEdit.setText (el->id, juce::dontSendNotification);
        xEdit.setText (juce::String (el->x), juce::dontSendNotification);
        yEdit.setText (juce::String (el->y), juce::dontSendNotification);
        wEdit.setText (juce::String (el->width),  juce::dontSendNotification);
        hEdit.setText (juce::String (el->height), juce::dontSendNotification);
        labelEdit.setText (el->label, juce::dontSendNotification);
        actionEdit.setText (el->action, juce::dontSendNotification);
        opacitySlider.setValue (juce::jlimit (0.0f, 1.0f, el->opacity) * 100.0f, juce::dontSendNotification);
        visibleToggle.setToggleState (el->visible, juce::dontSendNotification);
        lockedToggle.setToggleState (el->locked, juce::dontSendNotification);
        shapeKindBox.setSelectedId (el->shapeKind == "ellipse" ? 2
                                  : el->shapeKind == "triangle" ? 3
                                  : el->shapeKind == "diamond" ? 4
                                  : el->shapeKind == "line" ? 5 : 1,
                                  juce::dontSendNotification);
        cornerSlider.setValue (el->cornerRadius, juce::dontSendNotification);
        strokeSlider.setValue (el->strokeWidth, juce::dontSendNotification);
        shadowSlider.setValue (el->shadowAmount * 100.0f, juce::dontSendNotification);
        glowSlider.setValue (el->glowAmount * 100.0f, juce::dontSendNotification);
        blurSlider.setValue (el->blurAmount * 100.0f, juce::dontSendNotification);
        audioReactiveToggle.setToggleState (el->audioReactive, juce::dontSendNotification);
        audioReactiveModeBox.setSelectedId (el->audioReactiveMode == "peak" ? 2
                                          : el->audioReactiveMode == "lowBand" ? 3
                                          : el->audioReactiveMode == "midBand" ? 4
                                          : el->audioReactiveMode == "highBand" ? 5
                                          : el->audioReactiveMode == "transient" ? 6 : 1,
                                          juce::dontSendNotification);
        audioReactiveAmountSlider.setValue (el->audioReactiveAmount * 100.0f, juce::dontSendNotification);
        animationModeBox.setSelectedId (el->animationMode == "pulse" ? 2
                                      : el->animationMode == "breathe" ? 3
                                      : el->animationMode == "shake" ? 4
                                      : el->animationMode == "glow" ? 5 : 1,
                                      juce::dontSendNotification);
        animationRateSlider.setValue (el->animationRate, juce::dontSendNotification);
        assetEdit.setText (el->asset, juce::dontSendNotification);
        const auto parentId = el->containerId.isNotEmpty() ? el->containerId : el->groupId;
        int selectedContainerId = 1;
        for (int item = 0; item < containerBox.getNumItems(); ++item)
            if (containerBox.getItemText (item).contains ("(" + parentId + ")"))
                selectedContainerId = containerBox.getItemId (item);
        containerBox.setSelectedId (selectedContainerId, juce::dontSendNotification);
        tabsEdit.setText  (el->tabs.joinIntoString ("\n"), juce::dontSendNotification);
        filmstripPathEdit.setText   (el->filmstripAsset, juce::dontSendNotification);
        filmstripFramesEdit.setText (juce::String (el->filmstripFrames), juce::dontSendNotification);
        labelPositionBox.setSelectedId (el->labelPosition == "top" ? 2
                                       : el->labelPosition == "left" ? 3
                                       : el->labelPosition == "right" ? 4
                                       : el->labelPosition == "hidden" ? 5 : 1,
                                       juce::dontSendNotification);
        labelOffsetXSlider.setValue (el->labelOffsetX, juce::dontSendNotification);
        labelOffsetYSlider.setValue (el->labelOffsetY, juce::dontSendNotification);
        labelSpacingSlider.setValue (el->labelSpacing, juce::dontSendNotification);
        labelSizeSlider.setValue (el->labelSize, juce::dontSendNotification);
        backgroundColourEdit.setText (colourToHex (el->backgroundColour), juce::dontSendNotification);
        borderColourEdit.setText (colourToHex (el->borderColour), juce::dontSendNotification);
        accentColourEdit.setText (colourToHex (el->accentColour), juce::dontSendNotification);
        const bool isPadElement = el->type == ElementType::DrumPad || el->type == ElementType::PadGrid;
        const auto accentTip = isPadElement
            ? juce::String ("Pad trigger/highlight color. Hardware MIDI, software keys, and mouse-triggered pads use this Accent Color in the Player.")
            : juce::String ("Primary accent color for this element.");
        lblAccentColour.setTooltip (accentTip);
        accentColourEdit.setTooltip (accentTip);
        accentColourButton.setTooltip (accentTip);
        mixerModeBox.setSelectedId (el->mixerMode == "layers" ? 2
                                  : el->mixerMode == "parameters" ? 3 : 1,
                                  juce::dontSendNotification);
        mixerChannelsSlider.setValue (juce::jlimit (1, 16, el->mixerChannels), juce::dontSendNotification);
        mixerLabelsEdit.setText (el->mixerChannelLabels.joinIntoString ("\n"), juce::dontSendNotification);
        mixerVolumeParamsEdit.setText (el->mixerVolumeParams.joinIntoString ("\n"), juce::dontSendNotification);
        mixerPanParamsEdit.setText (el->mixerPanParams.joinIntoString ("\n"), juce::dontSendNotification);
        mixerMuteParamsEdit.setText (el->mixerMuteParams.joinIntoString ("\n"), juce::dontSendNotification);
        mixerSoloParamsEdit.setText (el->mixerSoloParams.joinIntoString ("\n"), juce::dontSendNotification);
        refreshMacroControls();
        refreshModMatrixControls();
        refreshGranularControls();

        // Parameter box selection
        int matchId = 1;
        int idx = 2;
        for (auto& p : owner.getProject().getParameters().getAll())
        {
            if (! p.visible)
                continue;
            if (p.id == el->parameterId) { matchId = idx; break; }
            ++idx;
        }
        parameterBox.setSelectedId (matchId, juce::dontSendNotification);

        // Style
        for (int i = 1; i <= styleBox.getNumItems(); ++i)
            if (styleBox.getItemText (i - 1) == el->style)
            { styleBox.setSelectedId (i, juce::dontSendNotification); break; }
        for (int i = 1; i <= knobStyleBox.getNumItems(); ++i)
            if (knobStyleBox.getItemText (i - 1) == el->knobStyle)
            { knobStyleBox.setSelectedId (i, juce::dontSendNotification); break; }
        for (int i = 1; i <= valueFormatBox.getNumItems(); ++i)
            if (valueFormatBox.getItemText (i - 1) == el->valueFormat)
            { valueFormatBox.setSelectedId (i, juce::dontSendNotification); break; }

        if (auto* p = owner.getProject().getParameters().find (el->parameterId))
        {
            minEdit.setText (juce::String (p->min, 2), juce::dontSendNotification);
            maxEdit.setText (juce::String (p->max, 2), juce::dontSendNotification);
            defaultEdit.setText (juce::String (p->defaultValue, 2), juce::dontSendNotification);
            stepEdit.setText (juce::String (p->step, 2), juce::dontSendNotification);
            smoothingSlider.setValue (p->smoothing * 1000.0f, juce::dontSendNotification);
            valueTypeBox.setSelectedId (p->type.equalsIgnoreCase ("midi") ? 2
                                      : p->type.equalsIgnoreCase ("custom") ? 3 : 1,
                                      juce::dontSendNotification);

            const bool paramEnabled = parameterIsEnabled (owner.getProject(), *p);
            const juce::String tip = parameterTooltip (owner.getProject(), *p);
            midiLearnButton.setEnabled (p->midiLearnable);
            midiLearnButton.setTooltip (p->midiLearnable
                ? "Start MIDI Learn for " + p->name + ". Then move a hardware MIDI control on the Test page."
                : "MIDI Learn is disabled for this parameter.");
            for (auto* component : { static_cast<juce::Component*> (&parameterBox),
                                     static_cast<juce::Component*> (&valueFormatBox),
                                     static_cast<juce::Component*> (&minEdit),
                                     static_cast<juce::Component*> (&maxEdit),
                                     static_cast<juce::Component*> (&defaultEdit),
                                     static_cast<juce::Component*> (&stepEdit),
                                     static_cast<juce::Component*> (&valueTypeBox),
                                     static_cast<juce::Component*> (&smoothingSlider) })
                setComponentTooltip (*component, tip);

            for (auto* component : { static_cast<juce::Component*> (&valueFormatBox),
                                     static_cast<juce::Component*> (&defaultEdit),
                                     static_cast<juce::Component*> (&smoothingSlider) })
                component->setEnabled (paramEnabled);

            if (! paramEnabled)
                smoothingSlider.setValue (0.0, juce::dontSendNotification);
        }
        else if (el->parameterId.isNotEmpty())
        {
            const auto tip = "This element references missing parameter '" + el->parameterId
                           + "'. Choose a valid parameter or clear the assignment.";
            for (auto* component : { static_cast<juce::Component*> (&parameterBox),
                                     static_cast<juce::Component*> (&valueFormatBox),
                                     static_cast<juce::Component*> (&minEdit),
                                     static_cast<juce::Component*> (&maxEdit),
                                     static_cast<juce::Component*> (&defaultEdit),
                                     static_cast<juce::Component*> (&stepEdit),
                                     static_cast<juce::Component*> (&valueTypeBox),
                                     static_cast<juce::Component*> (&smoothingSlider) })
            {
                component->setEnabled (component == &parameterBox);
                setComponentTooltip (*component, tip);
            }
        }

        if (el->type == ElementType::DrumGrid)
            refreshDrumControls();
    }

    void InspectorPanel::writeFromUi()
    {
        if (inhibitCallbacks) return;
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr) return;

        el->id = idEdit.getText();
        el->x  = xEdit.getText().getIntValue();
        el->y  = yEdit.getText().getIntValue();
        el->width  = juce::jmax (1, wEdit.getText().getIntValue());
        el->height = juce::jmax (1, hEdit.getText().getIntValue());
        el->label  = labelEdit.getText();
        el->action = actionEdit.getText().trim();
        el->opacity = juce::jlimit (0.0f, 1.0f, (float) opacitySlider.getValue() * 0.01f);
        el->visible = visibleToggle.getToggleState();
        el->locked = lockedToggle.getToggleState();
        el->shapeKind = shapeKindBox.getSelectedId() == 2 ? "ellipse"
                      : shapeKindBox.getSelectedId() == 3 ? "triangle"
                      : shapeKindBox.getSelectedId() == 4 ? "diamond"
                      : shapeKindBox.getSelectedId() == 5 ? "line" : "roundedRect";
        el->cornerRadius = (float) cornerSlider.getValue();
        el->strokeWidth = (float) strokeSlider.getValue();
        el->shadowAmount = (float) shadowSlider.getValue() * 0.01f;
        el->glowAmount = (float) glowSlider.getValue() * 0.01f;
        el->blurAmount = (float) blurSlider.getValue() * 0.01f;
        el->audioReactive = audioReactiveToggle.getToggleState();
        el->audioReactiveMode = audioReactiveModeBox.getSelectedId() == 2 ? "peak"
                              : audioReactiveModeBox.getSelectedId() == 3 ? "lowBand"
                              : audioReactiveModeBox.getSelectedId() == 4 ? "midBand"
                              : audioReactiveModeBox.getSelectedId() == 5 ? "highBand"
                              : audioReactiveModeBox.getSelectedId() == 6 ? "transient" : "level";
        el->audioReactiveAmount = (float) audioReactiveAmountSlider.getValue() * 0.01f;
        el->animationMode = animationModeBox.getSelectedId() == 2 ? "pulse"
                          : animationModeBox.getSelectedId() == 3 ? "breathe"
                          : animationModeBox.getSelectedId() == 4 ? "shake"
                          : animationModeBox.getSelectedId() == 5 ? "glow" : "none";
        el->animationRate = juce::jmax (0.05f, (float) animationRateSlider.getValue());
        el->asset  = assetEdit.getText();
        const auto containerTarget = containerBox.getSelectedId() > 1
            ? extractIdFromComboText (containerBox.getText())
            : juce::String();
        if (containerTarget.isEmpty())
        {
            el->containerId.clear();
            el->groupId.clear();
        }
        else if (auto* container = owner.getProject().getLayout().find (containerTarget);
                 container != nullptr
                 && (container->type == ElementType::Panel || container->type == ElementType::Group))
        {
            el->containerId = containerTarget;
        }
        else
        {
            el->containerId.clear();
            el->groupId = containerTarget;
        }
        el->labelPosition = labelPositionBox.getSelectedId() == 2 ? "top"
                          : labelPositionBox.getSelectedId() == 3 ? "left"
                          : labelPositionBox.getSelectedId() == 4 ? "right"
                          : labelPositionBox.getSelectedId() == 5 ? "hidden" : "bottom";
        el->labelOffsetX = (float) labelOffsetXSlider.getValue();
        el->labelOffsetY = (float) labelOffsetYSlider.getValue();
        el->labelSpacing = (float) labelSpacingSlider.getValue();
        el->labelSize = (float) labelSizeSlider.getValue();
        el->backgroundColour = colourFromHex (backgroundColourEdit.getText(), el->backgroundColour);
        el->borderColour = colourFromHex (borderColourEdit.getText(), el->borderColour);
        el->accentColour = colourFromHex (accentColourEdit.getText(), el->accentColour);

        if (el->type == ElementType::TabPanel)
        {
            el->tabs.clear();
            el->tabs.addTokens (tabsEdit.getText(), "\n", "");
            el->tabs.removeEmptyStrings (true);
        }

        // Filmstrip
        if (el->type == ElementType::Knob || el->type == ElementType::Slider
            || el->type == ElementType::Meter)
        {
            el->filmstripAsset  = filmstripPathEdit.getText().trim();
            el->filmstripFrames = juce::jmax (0, filmstripFramesEdit.getText().getIntValue());
        }

        const int typeIdx = typeBox.getSelectedId() - 1;
        if (typeIdx >= 0) el->type = (ElementType) typeIdx;

        if (parameterBox.getSelectedId() >= 2)
            el->parameterId = extractIdFromComboText (parameterBox.getText());
        else
            el->parameterId.clear();

        if (styleBox.getSelectedId() > 0)
            el->style = styleBox.getText();
        if (knobStyleBox.getSelectedId() > 0)
            el->knobStyle = knobStyleBox.getText();
        if (valueFormatBox.getSelectedId() > 0)
            el->valueFormat = valueFormatBox.getText();

        if (el->type == ElementType::DrumGrid)
        {
            el->drumPattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
            el->drumTracks = juce::jlimit (1, 16, juce::roundToInt (drumTracksSlider.getValue()));
            el->drumSteps = juce::jlimit (1, 64, juce::roundToInt (drumStepsSlider.getValue()));

            auto& block = ensureDrumMachineBlock();
            block.values["dmPattern"] = (float) el->drumPattern;
            block.values["dmTracks"] = (float) el->drumTracks;
            block.values["dmSteps"] = (float) el->drumSteps;
            block.values["dmTransport"] = 1.0f;
        }

        if (el->type == ElementType::Mixer)
        {
            el->mixerMode = mixerModeBox.getSelectedId() == 2 ? "layers"
                           : mixerModeBox.getSelectedId() == 3 ? "parameters" : "auto";
            el->mixerChannels = juce::jlimit (1, 16, juce::roundToInt (mixerChannelsSlider.getValue()));
            el->mixerChannelLabels = linesPreservingChannelSlots (mixerLabelsEdit.getText());
            el->mixerVolumeParams = linesPreservingChannelSlots (mixerVolumeParamsEdit.getText());
            el->mixerPanParams = linesPreservingChannelSlots (mixerPanParamsEdit.getText());
            el->mixerMuteParams = linesPreservingChannelSlots (mixerMuteParamsEdit.getText());
            el->mixerSoloParams = linesPreservingChannelSlots (mixerSoloParamsEdit.getText());
        }

        if (auto* p = owner.getProject().getParameters().find (el->parameterId))
        {
            p->min          = minEdit.getText().getFloatValue();
            p->max          = juce::jmax (p->min + 0.0001f, maxEdit.getText().getFloatValue());
            const float newDefault = defaultEdit.getText().getFloatValue();
            const bool defaultChanged = (p->defaultValue != newDefault);
            p->defaultValue = newDefault;
            p->step         = stepEdit.getText().getFloatValue();
            p->smoothing    = (float) smoothingSlider.getValue() * 0.001f;
            if (valueTypeBox.getSelectedId() > 0)
                p->type = valueTypeBox.getText().toLowerCase();

            // Setting the default also pushes it to the live value so the user
            // hears the change immediately in preview.
            if (defaultChanged)
                owner.getProject().getLiveValues().setValue (p->id, newDefault);
        }

        owner.propagateLinkedElementChange (el->id);
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::refreshMacroControls()
    {
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::MacroControl)
        {
            macroTargetsEdit.setText ({}, juce::dontSendNotification);
            return;
        }

        juce::StringArray lines;
        const auto macroId = el->parameterId;
        for (const auto& macro : owner.getProject().getDspGraph().macros)
        {
            if (macro.macroId != macroId)
                continue;

            lines.add (macro.targetId
                + " " + juce::String (macro.targetMin, 3)
                + " " + juce::String (macro.targetMax, 3)
                + " " + juce::String (macro.curve, 3));
        }

        macroTargetsEdit.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
    }

    void InspectorPanel::writeMacroTargetsFromUi()
    {
        if (inhibitCallbacks) return;
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::MacroControl)
            return;

        auto& graph = owner.getProject().getDspGraph();
        auto macroId = el->parameterId.trim();
        if (macroId.isEmpty())
        {
            macroId = "macro_" + juce::String ((int) graph.macros.size() + 1);
            el->parameterId = macroId;
        }

        auto blockExists = [&graph] (const juce::String& id)
        {
            for (const auto& block : graph.blocks)
                if (block.id == id)
                    return true;
            return false;
        };

        if (owner.getProject().getParameters().find (macroId) == nullptr && ! blockExists (macroId))
        {
            DspBlock block;
            block.id = macroId;
            block.section = "mod";
            block.type = "macro";
            block.name = el->label.isNotEmpty() ? el->label : "Macro Control";
            block.enabled = true;
            block.values["value"] = 0.5f;
            graph.blocks.push_back (std::move (block));
        }

        graph.macros.erase (std::remove_if (graph.macros.begin(), graph.macros.end(),
            [&macroId] (const MacroAssignment& macro) { return macro.macroId == macroId; }),
            graph.macros.end());

        juce::StringArray lines;
        lines.addLines (macroTargetsEdit.getText());
        int index = 1;
        for (auto line : lines)
        {
            line = line.trim();
            if (line.isEmpty())
                continue;

            juce::String normalised = line.replace ("->", " ")
                                         .replace (":", " ")
                                         .replace (",", " ");
            juce::StringArray tokens;
            tokens.addTokens (normalised, " \t", "\"'");
            tokens.removeEmptyStrings (true);
            if (tokens.isEmpty())
                continue;

            const auto targetId = tokens[0].trim();
            const auto* targetParam = owner.getProject().getParameters().find (targetId);
            const bool targetBlockExists = blockExists (targetId);
            if (targetParam == nullptr && ! targetBlockExists)
                continue;

            MacroAssignment macro;
            macro.id = macroId + "_to_" + targetId + "_" + juce::String (index++);
            macro.macroId = macroId;
            macro.targetId = targetId;
            macro.sourceMin = 0.0f;
            macro.sourceMax = 1.0f;
            const float fallbackMin = targetParam != nullptr ? targetParam->min : 0.0f;
            const float fallbackMax = targetParam != nullptr ? targetParam->max : 1.0f;
            macro.targetMin = tokens.size() > 1 ? tokens[1].getFloatValue() : fallbackMin;
            macro.targetMax = tokens.size() > 2 ? tokens[2].getFloatValue() : fallbackMax;
            macro.curve = tokens.size() > 3 ? juce::jmax (0.05f, tokens[3].getFloatValue()) : 1.0f;
            graph.macros.push_back (std::move (macro));
        }

        graph.userConfigured = true;
        owner.getProject().notifyChanged();
        refreshMacroControls();
    }

    void InspectorPanel::refreshModMatrixControls()
    {
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::ModMatrix)
        {
            modRoutesEdit.setText ({}, juce::dontSendNotification);
            return;
        }

        juce::StringArray lines;
        for (const auto& route : owner.getProject().getDspGraph().modulation)
        {
            lines.add (route.sourceId + " -> " + route.targetId
                + " " + juce::String (route.amount, 3)
                + " " + juce::String (route.smoothing, 3)
                + " " + (route.enabled ? "on" : "off"));
        }
        modRoutesEdit.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
    }

    void InspectorPanel::writeModRoutesFromUi()
    {
        if (inhibitCallbacks) return;

        auto& graph = owner.getProject().getDspGraph();
        auto nodeExists = [this, &graph] (const juce::String& id)
        {
            if (owner.getProject().getParameters().find (id) != nullptr)
                return true;
            for (const auto& block : graph.blocks)
                if (block.id == id)
                    return true;
            return false;
        };

        std::vector<ModRoute> routes;
        juce::StringArray lines;
        lines.addLines (modRoutesEdit.getText());
        int index = 1;
        for (auto line : lines)
        {
            line = line.trim();
            if (line.isEmpty())
                continue;

            juce::String normalised = line.replace ("->", " ")
                                         .replace (":", " ")
                                         .replace (",", " ");
            juce::StringArray tokens;
            tokens.addTokens (normalised, " \t", "\"'");
            tokens.removeEmptyStrings (true);
            if (tokens.size() < 2)
                continue;

            const auto sourceId = tokens[0].trim();
            const auto targetId = tokens[1].trim();
            if (! nodeExists (sourceId) || ! nodeExists (targetId))
                continue;

            ModRoute route;
            route.id = sourceId + "_to_" + targetId + "_" + juce::String (index++);
            route.sourceId = sourceId;
            route.targetId = targetId;
            route.amount = tokens.size() > 2 ? tokens[2].getFloatValue() : 0.25f;
            route.smoothing = tokens.size() > 3 ? juce::jmax (0.0f, tokens[3].getFloatValue()) : 0.02f;
            route.enabled = tokens.size() > 4 ? ! tokens[4].equalsIgnoreCase ("off") : true;
            routes.push_back (std::move (route));
        }

        graph.modulation = std::move (routes);
        graph.userConfigured = true;
        owner.getProject().notifyChanged();
        refreshModMatrixControls();
    }

    float InspectorPanel::granularValue (const juce::String& parameterId, float fallback) const
    {
        const auto* def = owner.getProject().getParameters().find (parameterId);
        return owner.getProject().getLiveValues().getValue (parameterId, def != nullptr ? def->defaultValue : fallback);
    }

    void InspectorPanel::setGranularValue (const juce::String& parameterId, float value, bool notify)
    {
        auto* def = owner.getProject().getParameters().find (parameterId);
        if (def != nullptr)
            value = juce::jlimit (def->min, def->max, value);

        owner.getProject().getLiveValues().setValue (parameterId, value);
        if (notify)
            owner.getProject().notifyChanged();
    }

    void InspectorPanel::refreshGranularControls()
    {
        const juce::ScopedValueSetter<bool> s (inhibitCallbacks, true);
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::GranularField)
            return;

        granularOnToggle.setToggleState (granularValue ("granularOn", 0.0f) >= 0.5f, juce::dontSendNotification);
        granularFreezeToggle.setToggleState (granularValue ("granularFreeze", 0.0f) >= 0.5f, juce::dontSendNotification);
        granularReverseToggle.setToggleState (granularValue ("granularReverse", 0.0f) >= 0.5f, juce::dontSendNotification);
        granularDirectionBox.setSelectedId (juce::jlimit (1, 4, juce::roundToInt (granularValue ("granularDirection", 3.0f)) + 1),
                                            juce::dontSendNotification);
        granularDensitySlider.setValue (granularValue ("granularDensity", 24.0f), juce::dontSendNotification);
        granularSizeSlider.setValue (granularValue ("granularSizeMs", 90.0f), juce::dontSendNotification);
        granularRandomSlider.setValue (granularValue ("granularSizeRandom", 0.25f) * 100.0f, juce::dontSendNotification);
        granularSpreadSlider.setValue (granularValue ("granularSpread", 0.18f) * 100.0f, juce::dontSendNotification);
        granularScanSlider.setValue (granularValue ("granularScan", 0.0f), juce::dontSendNotification);
        granularPitchSlider.setValue (granularValue ("granularPitchSpread", 0.0f), juce::dontSendNotification);
        granularPanSlider.setValue (granularValue ("granularPanSpread", 0.45f) * 100.0f, juce::dontSendNotification);
        granularTextureSlider.setValue (granularValue ("granularTexture", 0.20f) * 100.0f, juce::dontSendNotification);
    }

    void InspectorPanel::writeGranularControlsFromUi()
    {
        if (inhibitCallbacks)
            return;

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::GranularField)
            return;

        setGranularValue ("granularOn", granularOnToggle.getToggleState() ? 1.0f : 0.0f, false);
        setGranularValue ("granularFreeze", granularFreezeToggle.getToggleState() ? 1.0f : 0.0f, false);
        setGranularValue ("granularReverse", granularReverseToggle.getToggleState() ? 1.0f : 0.0f, false);
        setGranularValue ("granularDirection", (float) juce::jlimit (0, 3, granularDirectionBox.getSelectedId() - 1), false);
        setGranularValue ("granularDensity", (float) granularDensitySlider.getValue(), false);
        setGranularValue ("granularSizeMs", (float) granularSizeSlider.getValue(), false);
        setGranularValue ("granularSizeRandom", (float) granularRandomSlider.getValue() * 0.01f, false);
        setGranularValue ("granularSpread", (float) granularSpreadSlider.getValue() * 0.01f, false);
        setGranularValue ("granularScan", (float) granularScanSlider.getValue(), false);
        setGranularValue ("granularPitchSpread", (float) granularPitchSlider.getValue(), false);
        setGranularValue ("granularPanSpread", (float) granularPanSlider.getValue() * 0.01f, false);
        setGranularValue ("granularTexture", (float) granularTextureSlider.getValue() * 0.01f, false);

        owner.getProject().notifyChanged();
    }

    DspBlock* InspectorPanel::findDrumMachineBlock()
    {
        for (auto& block : owner.getProject().getDspGraph().blocks)
            if (isDrumMachineBlock (block))
                return &block;
        return nullptr;
    }

    const DspBlock* InspectorPanel::findDrumMachineBlock() const
    {
        for (const auto& block : owner.getProject().getDspGraph().blocks)
            if (isDrumMachineBlock (block))
                return &block;
        return nullptr;
    }

    DspBlock& InspectorPanel::ensureDrumMachineBlock()
    {
        if (auto* existing = findDrumMachineBlock())
            return *existing;

        DspBlock block;
        block.section = "mod";
        block.type = "drumMachine";
        block.name = "Drum Machine Performance";
        block.targetId = "midiDrumMachine";
        block.enabled = true;

        auto idAvailable = [this] (const juce::String& id)
        {
            for (const auto& existing : owner.getProject().getDspGraph().blocks)
                if (existing.id == id)
                    return false;
            return true;
        };

        block.id = "midi_drum_machine";
        int suffix = 2;
        while (! idAvailable (block.id))
            block.id = "midi_drum_machine_" + juce::String (suffix++);

        block.values["dmTracks"] = 8.0f;
        block.values["dmSteps"] = 16.0f;
        block.values["dmPattern"] = 0.0f;
        block.values["dmTransport"] = 1.0f;
        block.values["dmSwing"] = 0.08f;
        block.values["dmProbability"] = 1.0f;
        block.values["dmGate"] = 0.65f;
        block.values["rate"] = 1.0f;
        block.values["sync"] = 1.0f;
        block.values["enabled"] = 1.0f;

        for (int track = 0; track < 16; ++track)
        {
            block.values["dmTrack" + juce::String (track) + "Note"] = (float) defaultDrumTrackNote (track);
            block.metadata["dmTrack" + juce::String (track) + "Label"] = defaultDrumTrackLabel (track);
        }

        auto& blocks = owner.getProject().getDspGraph().blocks;
        blocks.push_back (std::move (block));
        owner.getProject().getDspGraph().userConfigured = true;
        return blocks.back();
    }

    juce::String InspectorPanel::drumPrefix (int pattern, int track, int step) const
    {
        return "dmP" + juce::String (juce::jlimit (0, 7, pattern))
             + "T" + juce::String (juce::jlimit (0, 15, track))
             + "S" + juce::String (juce::jlimit (0, 63, step));
    }

    float InspectorPanel::drumValue (const juce::String& key, float fallback) const
    {
        if (const auto* block = findDrumMachineBlock())
            return valueForBlockKey (*block, key, fallback);
        return fallback;
    }

    void InspectorPanel::setDrumValue (const juce::String& key, float value, bool notify)
    {
        auto& block = ensureDrumMachineBlock();
        block.values[key] = value;
        owner.getProject().getDspGraph().userConfigured = true;
        if (notify)
            owner.getProject().notifyChanged();
        else
            owner.getProject().markDirty();
    }

    void InspectorPanel::refreshDrumControls()
    {
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::DrumGrid)
            return;

        const juce::ScopedValueSetter<bool> s (inhibitCallbacks, true);
        const auto* block = findDrumMachineBlock();
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (block != nullptr
            ? valueForBlockKey (*block, "dmTracks", (float) el->drumTracks)
            : (float) el->drumTracks));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (block != nullptr
            ? valueForBlockKey (*block, "dmSteps", (float) el->drumSteps)
            : (float) el->drumSteps));
        const int pattern = juce::jlimit (0, 7, juce::roundToInt (block != nullptr
            ? valueForBlockKey (*block, "dmPattern", (float) el->drumPattern)
            : (float) el->drumPattern));

        drumPatternBox.setSelectedId (pattern + 1, juce::dontSendNotification);
        drumTracksSlider.setValue (tracks, juce::dontSendNotification);
        drumStepsSlider.setValue (steps, juce::dontSendNotification);

        const int track = juce::jlimit (0, tracks - 1, juce::jmax (0, drumTrackBox.getSelectedId() - 1));
        const int step = juce::jlimit (0, steps - 1, juce::jmax (0, drumStepBox.getSelectedId() - 1));
        drumTrackBox.setSelectedId (track + 1, juce::dontSendNotification);
        drumStepBox.setSelectedId (step + 1, juce::dontSendNotification);

        const auto prefix = drumPrefix (pattern, track, step);
        const auto trackFxTargetKey = "dmTrack" + juce::String (track) + "FxTarget";
        const auto trackFxAmountKey = "dmTrack" + juce::String (track) + "FxAmount";
        drumPadFxTargetBox.setSelectedId (juce::jlimit (1, 9,
            juce::roundToInt (drumValue (trackFxTargetKey, 0.0f)) + 1),
            juce::dontSendNotification);
        drumPadFxAmountSlider.setValue (juce::jlimit (0.0f, 1.0f,
            drumValue (trackFxAmountKey, 0.0f)) * 100.0f,
            juce::dontSendNotification);
        drumCellEnabledToggle.setToggleState (drumValue (prefix + "On", 0.0f) >= 0.5f,
                                              juce::dontSendNotification);
        drumVelocitySlider.setValue (juce::jlimit (0.0f, 1.0f, drumValue (prefix + "Vel", 0.82f)) * 100.0f,
                                     juce::dontSendNotification);
        drumGateSlider.setValue (juce::jlimit (0.05f, 1.0f, drumValue (prefix + "Gate", 0.65f)) * 100.0f,
                                 juce::dontSendNotification);
        drumProbabilitySlider.setValue (juce::jlimit (0.0f, 1.0f, drumValue (prefix + "Prob", 1.0f)) * 100.0f,
                                        juce::dontSendNotification);
        drumDivisionBox.setSelectedId (juce::jlimit (1, 4, juce::roundToInt (drumValue (prefix + "Div", 1.0f))),
                                       juce::dontSendNotification);
        drumCellFxTargetBox.setSelectedId (juce::jlimit (1, 9,
            juce::roundToInt (drumValue (prefix + "FxTarget", drumValue (trackFxTargetKey, 0.0f))) + 1),
            juce::dontSendNotification);
        drumCellFxAmountSlider.setValue (juce::jlimit (0.0f, 1.0f,
            drumValue (prefix + "FxAmount", drumValue (trackFxAmountKey, 0.0f))) * 100.0f,
            juce::dontSendNotification);
        drumPastePatternBtn.setEnabled (hasDrumPatternClipboard);

        const auto tip = "Edits Pattern " + juce::String (pattern + 1)
                       + ", " + defaultDrumTrackLabel (track)
                       + ", Step " + juce::String (step + 1)
                       + ". Ctrl/Cmd-click a cell on the canvas cycles x1/x2/x3/x4 divisions; Shift/Alt-click toggles hits.";
        for (auto* component : { static_cast<juce::Component*> (&drumCellEnabledToggle),
                                 static_cast<juce::Component*> (&drumVelocitySlider),
                                 static_cast<juce::Component*> (&drumGateSlider),
                                 static_cast<juce::Component*> (&drumProbabilitySlider),
                                 static_cast<juce::Component*> (&drumDivisionBox),
                                 static_cast<juce::Component*> (&drumPadFxTargetBox),
                                 static_cast<juce::Component*> (&drumPadFxAmountSlider),
                                 static_cast<juce::Component*> (&drumCellFxTargetBox),
                                 static_cast<juce::Component*> (&drumCellFxAmountSlider) })
            setComponentTooltip (*component, tip);
    }

    void InspectorPanel::writeDrumGridElementFromUi()
    {
        if (inhibitCallbacks) return;
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::DrumGrid)
            return;

        el->drumPattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        el->drumTracks = juce::jlimit (1, 16, juce::roundToInt (drumTracksSlider.getValue()));
        el->drumSteps = juce::jlimit (1, 64, juce::roundToInt (drumStepsSlider.getValue()));

        auto& block = ensureDrumMachineBlock();
        block.values["dmPattern"] = (float) el->drumPattern;
        block.values["dmTracks"] = (float) el->drumTracks;
        block.values["dmSteps"] = (float) el->drumSteps;
        block.values["dmTransport"] = 1.0f;
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::writeDrumTrackFxFromUi()
    {
        if (inhibitCallbacks) return;
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::DrumGrid)
            return;

        const int track = juce::jlimit (0, 15, drumTrackBox.getSelectedId() - 1);
        auto& block = ensureDrumMachineBlock();
        block.values["dmTrack" + juce::String (track) + "FxTarget"] =
            (float) juce::jlimit (0, 8, drumPadFxTargetBox.getSelectedId() - 1);
        block.values["dmTrack" + juce::String (track) + "FxAmount"] =
            juce::jlimit (0.0f, 1.0f, (float) drumPadFxAmountSlider.getValue() * 0.01f);
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::writeDrumCellFromUi()
    {
        if (inhibitCallbacks) return;
        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (el == nullptr || el->type != ElementType::DrumGrid)
            return;

        const int pattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        const int track = juce::jlimit (0, 15, drumTrackBox.getSelectedId() - 1);
        const int step = juce::jlimit (0, 63, drumStepBox.getSelectedId() - 1);
        const auto prefix = drumPrefix (pattern, track, step);
        auto& block = ensureDrumMachineBlock();
        block.values[prefix + "On"] = drumCellEnabledToggle.getToggleState() ? 1.0f : 0.0f;
        block.values[prefix + "Vel"] = juce::jlimit (0.0f, 1.0f, (float) drumVelocitySlider.getValue() * 0.01f);
        block.values[prefix + "Gate"] = juce::jlimit (0.05f, 1.0f, (float) drumGateSlider.getValue() * 0.01f);
        block.values[prefix + "Prob"] = juce::jlimit (0.0f, 1.0f, (float) drumProbabilitySlider.getValue() * 0.01f);
        block.values[prefix + "Div"] = (float) juce::jlimit (1, 4, drumDivisionBox.getSelectedId());
        block.values[prefix + "FxTarget"] = (float) juce::jlimit (0, 8, drumCellFxTargetBox.getSelectedId() - 1);
        block.values[prefix + "FxAmount"] = juce::jlimit (0.0f, 1.0f, (float) drumCellFxAmountSlider.getValue() * 0.01f);
        block.values["dmPattern"] = (float) pattern;
        block.values["dmTracks"] = (float) juce::jlimit (1, 16, juce::roundToInt (drumTracksSlider.getValue()));
        block.values["dmSteps"] = (float) juce::jlimit (1, 64, juce::roundToInt (drumStepsSlider.getValue()));
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::clearCurrentDrumPattern()
    {
        const int pattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        auto& block = ensureDrumMachineBlock();
        for (int track = 0; track < 16; ++track)
            for (int step = 0; step < 64; ++step)
            {
                const auto prefix = drumPrefix (pattern, track, step);
                block.values[prefix + "On"] = 0.0f;
                block.values[prefix + "Div"] = 1.0f;
                block.values[prefix + "FxTarget"] = 0.0f;
                block.values[prefix + "FxAmount"] = 0.0f;
            }
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::copyCurrentDrumPattern()
    {
        const int pattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        const auto head = "dmP" + juce::String (pattern);
        drumPatternClipboard.clear();
        if (const auto* block = findDrumMachineBlock())
        {
            for (const auto& entry : block->values)
                if (entry.first.startsWith (head))
                    drumPatternClipboard[entry.first.substring (head.length())] = entry.second;
        }
        drumPatternClipboardTracks = juce::jlimit (1, 16, juce::roundToInt (drumTracksSlider.getValue()));
        drumPatternClipboardSteps = juce::jlimit (1, 64, juce::roundToInt (drumStepsSlider.getValue()));
        hasDrumPatternClipboard = true;
        drumPastePatternBtn.setEnabled (true);
    }

    void InspectorPanel::pasteCurrentDrumPattern()
    {
        if (! hasDrumPatternClipboard)
            return;

        const int pattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        auto& block = ensureDrumMachineBlock();
        for (int track = 0; track < 16; ++track)
            for (int step = 0; step < 64; ++step)
            {
                const auto prefix = drumPrefix (pattern, track, step);
                block.values[prefix + "On"] = 0.0f;
                block.values[prefix + "Div"] = 1.0f;
            }
        for (const auto& entry : drumPatternClipboard)
            block.values["dmP" + juce::String (pattern) + entry.first] = entry.second;
        block.values["dmTracks"] = (float) juce::jlimit (1, 16, drumPatternClipboardTracks);
        block.values["dmSteps"] = (float) juce::jlimit (1, 64, drumPatternClipboardSteps);
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

    void InspectorPanel::duplicateCurrentDrumPatternToNext()
    {
        copyCurrentDrumPattern();
        const int current = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        {
            const juce::ScopedValueSetter<bool> s (inhibitCallbacks, true);
            drumPatternBox.setSelectedId ((current + 1) % 8 + 1, juce::dontSendNotification);
        }
        pasteCurrentDrumPattern();
    }

    void InspectorPanel::applyTrapRollToCurrentPattern()
    {
        const int pattern = juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (drumTracksSlider.getValue()));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (drumStepsSlider.getValue()));
        auto& block = ensureDrumMachineBlock();
        const int hatTrack = juce::jlimit (0, tracks - 1, 2);
        const int percTrack = juce::jlimit (0, tracks - 1, 6);

        for (int step = 0; step < steps; ++step)
        {
            if (step % 2 == 0)
            {
                const auto prefix = drumPrefix (pattern, hatTrack, step);
                block.values[prefix + "On"] = 1.0f;
                block.values[prefix + "Vel"] = step % 8 == 0 ? 0.86f : 0.64f;
                block.values[prefix + "Gate"] = 0.28f;
                block.values[prefix + "Prob"] = 0.96f;
                block.values[prefix + "Div"] = (step % 16 == 14) ? 4.0f
                                             : (step % 8 == 6) ? 3.0f
                                             : (step % 4 == 2) ? 2.0f : 1.0f;
            }
            if (tracks > 6 && (step == steps / 2 - 1 || step == steps - 2 || step == steps - 1))
            {
                const auto prefix = drumPrefix (pattern, percTrack, step);
                block.values[prefix + "On"] = 1.0f;
                block.values[prefix + "Vel"] = step == steps - 1 ? 0.74f : 0.58f;
                block.values[prefix + "Gate"] = 0.42f;
                block.values[prefix + "Prob"] = 0.88f;
                block.values[prefix + "Div"] = step == steps - 1 ? 4.0f : 2.0f;
            }
        }

        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().notifyChanged();
    }

} // namespace patchcraft
