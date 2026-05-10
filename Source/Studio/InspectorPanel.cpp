#include "InspectorPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "CanvasEditor.h"

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

    InspectorPanel::InspectorPanel (StudioMainComponent& o) : owner (o)
    {
        styleLabel (header, "INSPECTOR", juce::Justification::centredLeft, 12.0f, true);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (header);

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
        styleLabel (lblActions,     "ACTIONS", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblAsset,       "Asset");
        styleLabel (lblGroup,       "Container");
        styleLabel (lblTabs,        "Tabs");
        styleLabel (lblContainerManager, "CONTAINER MANAGER", juce::Justification::centredLeft, 11.0f, true);
        styleLabel (lblContainerChildren, "Children");
        styleLabel (lblFilmstripPath,   "Knob Image");
        styleLabel (lblFilmstripFrames, "Frames");

        for (auto* l : { &lblType, &lblId, &lblPos, &lblSize, &lblParam, &lblLabel,
                         &lblValFmt, &lblStyle, &lblKnobStyle, &lblMin, &lblMax,
                         &lblOpacity, &lblState, &lblShapeKind, &lblCorner, &lblStroke,
                         &lblShadow, &lblGlow, &lblBlur, &lblAudioReactive,
                         &lblAudioReactiveMode, &lblAudioReactiveAmount,
                         &lblAnimationMode, &lblAnimationRate,
                         &lblLabelPosition, &lblLabelOffsetX, &lblLabelOffsetY,
                         &lblLabelSpacing, &lblLabelSize, &lblBgColour, &lblBorderColour, &lblAccentColour,
                         &lblDefault, &lblStep, &lblValType, &lblSmoothing,
                         &lblActions, &lblPosX, &lblPosY, &lblSizeW, &lblSizeH,
                         &lblAsset, &lblGroup, &lblTabs, &lblContainerManager, &lblContainerChildren,
                         &lblFilmstripPath, &lblFilmstripFrames })
            addAndMakeVisible (*l);

        // Type combo
        const char* types[] = { "Image", "Knob", "Slider", "Button", "Toggle",
                                "Dropdown", "Label", "Value Display", "Meter",
                                "Waveform", "Keyboard", "Panel", "Shape", "XY Pad",
                                "Tab Panel", "Scroll Panel", "Group", "Separator" };
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

        for (auto* b : { &btnDuplicate, &btnDelete, &btnForward, &btnBackward })
        {
            b->getProperties().set ("fontSize", 10.5);
            addAndMakeVisible (*b);
        }
        btnDuplicate.setTooltip ("Duplicate");
        btnDelete.setTooltip    ("Delete");
        btnForward.setTooltip   ("Bring Forward");
        btnBackward.setTooltip  ("Send Backward");

        btnDuplicate.onClick = [this]
        {
            auto id = owner.getSelectedElementId();
            if (auto* el = owner.getProject().getLayout().find (id))
            {
                LayoutElement copy = *el;
                copy.id.clear();
                copy.x += 16; copy.y += 16;
                auto& added = owner.getProject().getLayout().add (copy);
                owner.setSelectedElementId (added.id);
                owner.getProject().notifyChanged();
            }
        };
        btnDelete.onClick = [this]
        {
            auto id = owner.getSelectedElementId();
            owner.getProject().getLayout().remove (id);
            owner.setSelectedElementId ({});
            owner.getProject().notifyChanged();
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

        refresh();
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 0, 1, getHeight());
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
                               || type == ElementType::ValueDisplay);
        const bool isImage  = (type == ElementType::Image);
        const bool isLabel  = (type == ElementType::Label);
        const bool isShape  = (type == ElementType::Shape || type == ElementType::Panel);
        const bool canAnimate = (type != ElementType::Group && type != ElementType::Separator);
        const bool hasStyle = (type == ElementType::Knob || type == ElementType::Slider
                               || type == ElementType::Meter || type == ElementType::Button
                               || type == ElementType::Panel || type == ElementType::Shape);
        const bool isKnob   = (type == ElementType::Knob);
        const bool showContainerManager = (el != nullptr && isContainerElement (type));

        // Common: type, id
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
        layoutRow (r, lblOpacity, &opacitySlider);
        {
            auto row = r.removeFromTop (34);
            lblState.setBounds (row.removeFromLeft (110).reduced (10, 4));
            const int half = row.getWidth() / 2;
            visibleToggle.setBounds (row.removeFromLeft (half).reduced (4, 4));
            lockedToggle.setBounds (row.reduced (4, 4));
        }

        // Parameter binding section
        const bool showParam = hasParam;
        parameterBox.setVisible (showParam);
        midiLearnButton.setVisible (showParam);
        valueFormatBox.setVisible (showParam);
        minEdit.setVisible (showParam);
        maxEdit.setVisible (showParam);
        defaultEdit.setVisible (showParam);
        stepEdit.setVisible (showParam);
        valueTypeBox.setVisible (showParam);
        smoothingSlider.setVisible (showParam);
        lblParam.setVisible (showParam);
        lblValFmt.setVisible (showParam);
        lblMin.setVisible (showParam);
        lblMax.setVisible (showParam);
        lblDefault.setVisible (showParam);
        lblStep.setVisible (showParam);
        lblValType.setVisible (showParam);
        lblSmoothing.setVisible (showParam);

        if (showParam)
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

        // Style section
        styleBox.setVisible (hasStyle);
        knobStyleBox.setVisible (isKnob);
        lblStyle.setVisible (hasStyle);
        lblKnobStyle.setVisible (isKnob);
        if (hasStyle)
            layoutRow (r, lblStyle, &styleBox);
        if (isKnob)
            layoutRow (r, lblKnobStyle, &knobStyleBox);

        // Filmstrip override (Knob / Slider / Meter)
        const bool showFilmstrip = (type == ElementType::Knob
                                    || type == ElementType::Slider
                                    || type == ElementType::Meter);
        filmstripPathEdit.setVisible   (showFilmstrip);
        filmstripBrowseBtn.setVisible  (showFilmstrip);
        filmstripFramesEdit.setVisible (showFilmstrip);
        filmstripAutoBtn.setVisible    (showFilmstrip);
        lblFilmstripPath.setVisible    (showFilmstrip);
        lblFilmstripFrames.setVisible  (showFilmstrip);
        if (showFilmstrip)
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

        shapeKindBox.setVisible (isShape);
        cornerSlider.setVisible (isShape);
        strokeSlider.setVisible (isShape);
        shadowSlider.setVisible (isShape);
        glowSlider.setVisible (isShape);
        blurSlider.setVisible (isShape);
        lblShapeKind.setVisible (isShape);
        lblCorner.setVisible (isShape);
        lblStroke.setVisible (isShape);
        lblShadow.setVisible (isShape);
        lblGlow.setVisible (isShape);
        lblBlur.setVisible (isShape);
        if (isShape)
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
        audioReactiveToggle.setVisible (canAnimate);
        audioReactiveModeBox.setVisible (showAudioDetail);
        audioReactiveAmountSlider.setVisible (showAudioDetail);
        animationModeBox.setVisible (canAnimate);
        animationRateSlider.setVisible (showAnimationRate);
        lblAudioReactive.setVisible (canAnimate);
        lblAudioReactiveMode.setVisible (showAudioDetail);
        lblAudioReactiveAmount.setVisible (showAudioDetail);
        lblAnimationMode.setVisible (canAnimate);
        lblAnimationRate.setVisible (showAnimationRate);
        if (canAnimate)
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
        assetEdit.setVisible (isImage);
        browseAssetBtn.setVisible (isImage);
        lblAsset.setVisible (isImage);
        if (isImage)
        {
            auto row = r.removeFromTop (34);
            lblAsset.setBounds (row.removeFromLeft (110).reduced (10, 4));
            browseAssetBtn.setBounds (row.removeFromRight (80).reduced (4, 4));
            assetEdit.setBounds (row.reduced (4, 4));
        }

        // Group ID (any non-locked element)
        const bool showGroup = (el != nullptr && ! el->locked);
        containerBox.setVisible (showGroup);
        lblGroup.setVisible (showGroup);
        if (showGroup)
            layoutRow (r, lblGroup, &containerBox);

        const bool showLabelLayout = (el != nullptr && (type == ElementType::Knob || type == ElementType::Slider
                                                        || type == ElementType::Button || type == ElementType::Toggle
                                                        || type == ElementType::Dropdown || type == ElementType::ValueDisplay));
        for (auto* component : { static_cast<juce::Component*> (&labelPositionBox),
                                 static_cast<juce::Component*> (&labelOffsetXSlider),
                                 static_cast<juce::Component*> (&labelOffsetYSlider),
                                 static_cast<juce::Component*> (&labelSpacingSlider),
                                 static_cast<juce::Component*> (&labelSizeSlider) })
            component->setVisible (showLabelLayout);
        for (auto* label : { &lblLabelPosition, &lblLabelOffsetX, &lblLabelOffsetY, &lblLabelSpacing, &lblLabelSize })
            label->setVisible (showLabelLayout);
        if (showLabelLayout)
        {
            layoutRow (r, lblLabelPosition, &labelPositionBox);
            layoutRow (r, lblLabelOffsetX, &labelOffsetXSlider);
            layoutRow (r, lblLabelOffsetY, &labelOffsetYSlider);
            layoutRow (r, lblLabelSpacing, &labelSpacingSlider);
            layoutRow (r, lblLabelSize, &labelSizeSlider);
        }

        const bool showColour = (type == ElementType::Panel || type == ElementType::Group
                                 || type == ElementType::Shape || type == ElementType::Button
                                 || type == ElementType::Toggle || type == ElementType::Dropdown);
        for (auto* component : { static_cast<juce::Component*> (&backgroundColourEdit),
                                 static_cast<juce::Component*> (&borderColourEdit),
                                 static_cast<juce::Component*> (&accentColourEdit),
                                 static_cast<juce::Component*> (&backgroundColourButton),
                                 static_cast<juce::Component*> (&borderColourButton),
                                 static_cast<juce::Component*> (&accentColourButton) })
            component->setVisible (showColour);
        for (auto* label : { &lblBgColour, &lblBorderColour, &lblAccentColour })
            label->setVisible (showColour);
        if (showColour)
        {
            layoutColourRow (r, lblBgColour, backgroundColourEdit, backgroundColourButton);
            layoutColourRow (r, lblBorderColour, borderColourEdit, borderColourButton);
            layoutColourRow (r, lblAccentColour, accentColourEdit, accentColourButton);
        }

        // TabPanel: tab list editor (one tab label per line)
        const bool isTabPanel = (type == ElementType::TabPanel);
        tabsEdit.setVisible (isTabPanel);
        lblTabs.setVisible (isTabPanel);
        if (isTabPanel)
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
            component->setVisible (showContainerManager);
        lblContainerManager.setVisible (showContainerManager);
        lblContainerChildren.setVisible (showContainerManager);
        containerAddTabBtn.setVisible (showContainerManager && isTabPanel);
        containerRemoveTabBtn.setVisible (showContainerManager && isTabPanel);

        if (showContainerManager)
        {
            r.removeFromTop (6);
            lblContainerManager.setBounds (r.removeFromTop (20).reduced (10, 0));
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

        juce::ignoreUnused (isLabel);

        // Actions
        r.removeFromTop (12);
        lblActions.setBounds (r.removeFromTop (20).reduced (10, 0));
        auto actions = r.removeFromTop (54).reduced (10, 4);
        const int aw = juce::jmin (60, actions.getWidth() / 4);
        btnDuplicate.setBounds (actions.removeFromLeft (aw));
        btnDelete.setBounds    (actions.removeFromLeft (aw));
        btnForward.setBounds   (actions.removeFromLeft (aw));
        btnBackward.setBounds  (actions);
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
            static_cast<juce::Component*> (&btnDuplicate),
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

        if (! enabled)
        {
            idEdit.setText ("(no selection)", juce::dontSendNotification);
            xEdit.setText ("", juce::dontSendNotification);
            yEdit.setText ("", juce::dontSendNotification);
            wEdit.setText ("", juce::dontSendNotification);
            hEdit.setText ("", juce::dontSendNotification);
            labelEdit.setText ("", juce::dontSendNotification);
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

        owner.getProject().notifyChanged();
    }

} // namespace patchcraft
