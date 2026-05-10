#include "ControlBuilderComponent.h"
#include "StudioMainComponent.h"
#include "KnobBuilderComponent.h"
#include "SliderBuilderComponent.h"
#include "MeterBuilderComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    ControlBuilderComponent::ControlBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        title.setText ("Asset Build Lab", juce::dontSendNotification);
        title.setFont (juce::Font (18.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (title);

        subtitle.setText ("Build production-ready knobs, sliders, meters, switches, and filmstrips for instrument GUIs.", juce::dontSendNotification);
        subtitle.setFont (juce::Font (11.5f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        kindLabel.setText ("Build", juce::dontSendNotification);
        kindLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        kindLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (kindLabel);

        kindBox.addItem ("Knob",   1);
        kindBox.addItem ("Slider", 2);
        kindBox.addItem ("Meter",  3);
        kindBox.addSeparator();
        kindBox.addItem ("Switch / Button", 4);
        kindBox.addItem ("XY Pad", 5);
        kindBox.setSelectedId (1, juce::dontSendNotification);
        kindBox.onChange = [this] { rebuildVisibility(); };
        addAndMakeVisible (kindBox);

        for (auto* button : { &newAssetButton, &duplicateButton, &exportButton })
        {
            button->setTooltip ("Project asset library actions will be connected after the editor surface is locked.");
            addAndMakeVisible (*button);
        }
        exportButton.getProperties().set ("accent", true);

        knobBuilder   = std::make_unique<KnobBuilderComponent> (owner);
        sliderBuilder = std::make_unique<SliderBuilderComponent> (owner);
        meterBuilder  = std::make_unique<MeterBuilderComponent> (owner);
        addAndMakeVisible (*knobBuilder);
        addChildComponent (*sliderBuilder);
        addChildComponent (*meterBuilder);

        exportButton.setTooltip ("Render the current builder asset into the Design page Library.");
        exportButton.onClick = [this]
        {
            if (kindBox.getSelectedId() == 1 && knobBuilder != nullptr)
                knobBuilder->addKnobToLibrary();
        };
    }

    ControlBuilderComponent::~ControlBuilderComponent() = default;

    void ControlBuilderComponent::rebuildVisibility()
    {
        knobBuilder->setVisible   (kindBox.getSelectedId() == 1 || kindBox.getSelectedId() > 3);
        sliderBuilder->setVisible (kindBox.getSelectedId() == 2);
        meterBuilder->setVisible  (kindBox.getSelectedId() == 3);
        resized();
    }

    void ControlBuilderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        auto top = getLocalBounds().removeFromTop (54);
        g.setColour (PatchCraftLookAndFeel::bg());
        g.fillRect (top);
        g.setColour (PatchCraftLookAndFeel::borderSoft());
        g.drawHorizontalLine (top.getBottom() - 1, 0.0f, (float) getWidth());
    }

    void ControlBuilderComponent::resized()
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (54).reduced (12, 6);
        auto leftTitle = top.removeFromLeft (juce::jmin (520, top.getWidth() / 2));
        title.setBounds (leftTitle.removeFromTop (22));
        subtitle.setBounds (leftTitle);

        exportButton.setBounds (top.removeFromRight (86).reduced (3, 7));
        duplicateButton.setBounds (top.removeFromRight (92).reduced (3, 7));
        newAssetButton.setBounds (top.removeFromRight (92).reduced (3, 7));
        kindBox.setBounds (top.removeFromRight (170).reduced (3, 7));
        kindLabel.setBounds (top.removeFromRight (44).reduced (0, 8));

        r = r.reduced (10);
        knobBuilder->setBounds   (r);
        sliderBuilder->setBounds (r);
        meterBuilder->setBounds  (r);
    }

} // namespace patchcraft
