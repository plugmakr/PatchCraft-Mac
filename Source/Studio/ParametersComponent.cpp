#include "ParametersComponent.h"

#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"
#include "TutorialHelp.h"

namespace patchcraft
{
    namespace
    {
        static bool isBindable (const LayoutElement* element)
        {
            return element != nullptr
                && (element->type == ElementType::Knob || element->type == ElementType::Slider
                    || element->type == ElementType::Button || element->type == ElementType::Toggle
                    || element->type == ElementType::Dropdown || element->type == ElementType::ValueDisplay
                    || element->type == ElementType::MacroControl || element->type == ElementType::SampleDropZone);
        }
    }

    ParametersComponent::ParametersComponent (StudioMainComponent& targetOwner) : owner (targetOwner)
    {
        addAndMakeVisible (openEditor);
        addAndMakeVisible (addControl);
        addAndMakeVisible (selectionStatus);
        addAndMakeVisible (hintLabel);

        openEditor.setTooltip ("Open the node graph for the selected runtime control.");
        openEditor.onClick = [this] { owner.openControlNodeEditor(); };
        addControl.setTooltip ("Add a new knob to the Layout canvas without opening another window.");
        addControl.onClick = [this] { owner.addElementToCanvas (ElementType::Knob); };

        TutorialHelp::attach (openEditor, "layout.opennodeeditor");
        TutorialHelp::attach (addControl, "layout.addknob");

        hintLabel.setFont (juce::FontOptions (11.0f));
        hintLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        hintLabel.setJustificationType (juce::Justification::topLeft);
        TutorialHelp::attach (hintLabel, "layout.controlbindings");

        selectionStatus.setFont (11.5f);
        selectionStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        refresh();
    }

    void ParametersComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void ParametersComponent::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        selectionStatus.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        hintLabel.setBounds (r.removeFromTop (juce::jmax (36, r.getHeight() / 2)));
        r.removeFromTop (6);
        auto buttons = r.removeFromBottom (30);
        openEditor.setBounds (buttons.removeFromLeft (148));
        buttons.removeFromLeft (6);
        addControl.setBounds (buttons.removeFromLeft (158));
    }

    void ParametersComponent::refresh()
    {
        const auto* element = owner.getProject().getLayout().find (owner.getSelectedElementId());
        const bool bindable = isBindable (element);
        openEditor.setEnabled (bindable);

        juce::String text = "Select a runtime control to edit its sound connection.";
        if (bindable)
        {
            const auto* parameter = owner.getProject().getParameters().find (element->parameterId);
            const auto name = element->label.isNotEmpty() ? element->label : element->id;
            text = name + "  ->  " + (parameter != nullptr ? parameter->name
                                      : element->parameterId.isNotEmpty() ? element->parameterId : "Not connected");
        }
        selectionStatus.setText (text, juce::dontSendNotification);

        hintLabel.setText ("Wire Layout controls to sound parameters here. Use the Sound Stack tab for Source, Tone, Space, and Motion.",
                           juce::dontSendNotification);
    }
}
