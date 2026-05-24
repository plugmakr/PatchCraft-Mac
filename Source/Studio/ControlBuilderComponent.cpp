#include "ControlBuilderComponent.h"
#include "StudioMainComponent.h"
#include "KnobBuilderComponent.h"
#include "SliderBuilderComponent.h"
#include "MeterBuilderComponent.h"
#include "AiImageBuilderComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    ControlBuilderComponent::ControlBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        title.setText ("Asset Build Lab", juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (title);

        subtitle.setText ("Build production-ready knobs, sliders, meters, and filmstrips for instrument GUIs.", juce::dontSendNotification);
        subtitle.setFont (juce::FontOptions (11.5f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        kindLabel.setText ("Build", juce::dontSendNotification);
        kindLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        kindLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (kindLabel);

        kindBox.addItem ("Knob",   1);
        kindBox.addItem ("Slider", 2);
        kindBox.addItem ("Meter",  3);
        kindBox.addItem ("AI Image", 4);
        kindBox.setSelectedId (1, juce::dontSendNotification);
        kindBox.onChange = [this] { rebuildVisibility(); };
        addAndMakeVisible (kindBox);

        newAssetButton.setTooltip ("Reset the active builder to a clean default asset.");
        duplicateButton.setTooltip ("Duplicate the current builder asset into the Design page Library.");
        exportButton.setTooltip ("Render the current builder asset into the Design page Library.");
        for (auto* button : { &newAssetButton, &duplicateButton, &exportButton })
            addAndMakeVisible (*button);
        exportButton.getProperties().set ("accent", true);

        knobBuilder   = std::make_unique<KnobBuilderComponent> (owner);
        sliderBuilder = std::make_unique<SliderBuilderComponent> (owner);
        meterBuilder  = std::make_unique<MeterBuilderComponent> (owner);
        aiImageBuilder = std::make_unique<AiImageBuilderComponent> (owner);
        addAndMakeVisible (*knobBuilder);
        addChildComponent (*sliderBuilder);
        addChildComponent (*meterBuilder);
        addChildComponent (*aiImageBuilder);

        auto addCurrentAssetToLibrary = [this]
        {
            if (kindBox.getSelectedId() == 1 && knobBuilder != nullptr)
                knobBuilder->addKnobToLibrary();
            else if (kindBox.getSelectedId() == 2 && sliderBuilder != nullptr)
                sliderBuilder->addSliderToLibrary();
            else if (kindBox.getSelectedId() == 3 && meterBuilder != nullptr)
                meterBuilder->addMeterToLibrary();
            else if (kindBox.getSelectedId() == 4 && aiImageBuilder != nullptr)
                aiImageBuilder->addGeneratedAssetToLibrary();
        };
        exportButton.onClick = addCurrentAssetToLibrary;
        duplicateButton.onClick = addCurrentAssetToLibrary;

        newAssetButton.onClick = [this]
        {
            const auto selected = kindBox.getSelectedId();
            if (selected == 1)
            {
                removeChildComponent (knobBuilder.get());
                knobBuilder = std::make_unique<KnobBuilderComponent> (owner);
                addAndMakeVisible (*knobBuilder);
            }
            else if (selected == 2)
            {
                removeChildComponent (sliderBuilder.get());
                sliderBuilder = std::make_unique<SliderBuilderComponent> (owner);
                addChildComponent (*sliderBuilder);
            }
            else if (selected == 3)
            {
                removeChildComponent (meterBuilder.get());
                meterBuilder = std::make_unique<MeterBuilderComponent> (owner);
                addChildComponent (*meterBuilder);
            }
            else if (selected == 4)
            {
                removeChildComponent (aiImageBuilder.get());
                aiImageBuilder = std::make_unique<AiImageBuilderComponent> (owner);
                addChildComponent (*aiImageBuilder);
            }

            rebuildVisibility();
        };
    }

    ControlBuilderComponent::~ControlBuilderComponent() = default;

    void ControlBuilderComponent::rebuildVisibility()
    {
        knobBuilder->setVisible   (kindBox.getSelectedId() == 1);
        sliderBuilder->setVisible (kindBox.getSelectedId() == 2);
        meterBuilder->setVisible  (kindBox.getSelectedId() == 3);
        aiImageBuilder->setVisible (kindBox.getSelectedId() == 4);
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
        aiImageBuilder->setBounds (r);
    }

} // namespace patchcraft
