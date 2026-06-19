#include "ParametersComponent.h"

#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"

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

        static juce::Colour stageColour (int index)
        {
            static const juce::Colour colours[] {
                juce::Colour (0xff35b8d4), juce::Colour (0xff79c267), juce::Colour (0xffd889e8),
                juce::Colour (0xffffb84d), juce::Colour (0xfff06b78)
            };
            return colours[juce::jlimit (0, 4, index)];
        }
    }

    ParametersComponent::ParametersComponent (StudioMainComponent& targetOwner) : owner (targetOwner)
    {
        addAndMakeVisible (openEditor);
        addAndMakeVisible (addControl);
        addAndMakeVisible (selectionStatus);

        openEditor.setTooltip ("Open the node graph for the selected runtime control.");
        openEditor.onClick = [this] { owner.openControlNodeEditor(); };
        addControl.setTooltip ("Add a new knob to the Design canvas without opening another window.");
        addControl.onClick = [this] { owner.addElementToCanvas (ElementType::Knob); };
        selectionStatus.setFont (11.5f);
        selectionStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        refresh();
    }

    void ParametersComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        const juce::StringArray labels { "SOURCE", "SHAPE", "MOTION", "FX", "OUTPUT" };
        int counts[5] {};
        for (const auto& block : owner.getProject().getDspGraph().blocks)
        {
            const auto section = block.section.toLowerCase();
            const int index = section == "source" ? 0
                            : (section == "filter" || section == "amp" || section == "shape") ? 1
                            : (section == "mod" || section == "motion") ? 2
                            : section == "fx" ? 3 : section == "out" ? 4 : -1;
            if (index >= 0 && block.enabled)
                ++counts[index];
        }

        auto row = stageArea;
        const int gap = 6;
        const int width = juce::jmax (64, (row.getWidth() - gap * 4) / 5);
        for (int index = 0; index < 5; ++index)
        {
            auto card = row.removeFromLeft (width);
            row.removeFromLeft (gap);
            const auto colour = stageColour (index);
            g.setColour (juce::Colour (0xff151a21));
            g.fillRoundedRectangle (card.toFloat(), 4.0f);
            g.setColour (colour.withAlpha (0.75f));
            g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.fillRect (card.removeFromTop (3));
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (labels[index], card.removeFromTop (22), juce::Justification::centred);
            g.setColour (colour);
            g.setFont (juce::FontOptions (17.0f).withStyle ("bold"));
            g.drawText (juce::String (counts[index]), card, juce::Justification::centred);
        }
    }

    void ParametersComponent::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        selectionStatus.setBounds (r.removeFromTop (24));
        r.removeFromTop (3);
        auto buttons = r.removeFromBottom (30);
        openEditor.setBounds (buttons.removeFromLeft (148));
        buttons.removeFromLeft (6);
        addControl.setBounds (buttons.removeFromLeft (158));
        r.removeFromBottom (6);
        stageArea = r;
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
        repaint();
    }
}
