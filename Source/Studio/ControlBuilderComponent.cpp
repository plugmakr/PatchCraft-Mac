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

        galleryTitle.setText ("Widget Gallery", juce::dontSendNotification);
        galleryTitle.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        galleryTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (galleryTitle);

        galleryHint.setText ("Select a factory widget, then edit it here.", juce::dontSendNotification);
        galleryHint.setFont (juce::FontOptions (10.5f));
        galleryHint.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (galleryHint);

        galleryList.setRowHeight (34);
        galleryList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        galleryList.setOutlineThickness (0);
        addAndMakeVisible (galleryList);

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
        selectedGalleryRow = juce::jlimit (0, juce::jmax (0, getNumRows() - 1), selectedGalleryRow);
        galleryList.updateContent();
        galleryList.selectRow (selectedGalleryRow);
        resized();
    }

    juce::StringArray ControlBuilderComponent::getCurrentGalleryNames() const
    {
        if (kindBox.getSelectedId() == 1) return KnobBuilderComponent::galleryPresetNames();
        if (kindBox.getSelectedId() == 2) return SliderBuilderComponent::galleryPresetNames();
        if (kindBox.getSelectedId() == 3) return MeterBuilderComponent::galleryPresetNames();
        return {};
    }

    void ControlBuilderComponent::applyGalleryPreset (int row)
    {
        const auto names = getCurrentGalleryNames();
        if (row < 0 || row >= names.size())
            return;

        selectedGalleryRow = row;
        galleryList.selectRow (row);
        if (kindBox.getSelectedId() == 1 && knobBuilder != nullptr)
            knobBuilder->applyGalleryPreset (row);
        else if (kindBox.getSelectedId() == 2 && sliderBuilder != nullptr)
            sliderBuilder->applyGalleryPreset (row);
        else if (kindBox.getSelectedId() == 3 && meterBuilder != nullptr)
            meterBuilder->applyGalleryPreset (row);
        galleryList.repaint();
    }

    int ControlBuilderComponent::getNumRows()
    {
        return getCurrentGalleryNames().size();
    }

    void ControlBuilderComponent::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        const auto names = getCurrentGalleryNames();
        if (rowNumber < 0 || rowNumber >= names.size())
            return;

        auto row = juce::Rectangle<int> (0, 0, width, height).reduced (3, 3);
        const auto accent = kindBox.getSelectedId() == 2 ? juce::Colour (0xff48c7e8)
                          : kindBox.getSelectedId() == 3 ? juce::Colour (0xff69d77a)
                                                         : PatchCraftLookAndFeel::accent();
        g.setColour (rowIsSelected ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        g.setColour ((rowIsSelected ? accent : PatchCraftLookAndFeel::border()).withAlpha (rowIsSelected ? 0.9f : 0.65f));
        g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 6.0f, 1.0f);

        auto text = row.reduced (9, 0);
        g.setColour (accent);
        g.fillEllipse ((float) text.getX(), (float) text.getCentreY() - 3.0f, 6.0f, 6.0f);
        text.removeFromLeft (14);
        g.setColour (rowIsSelected ? PatchCraftLookAndFeel::textBright() : PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (11.0f, rowIsSelected ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText (names[rowNumber], text, juce::Justification::centredLeft, 1);
    }

    void ControlBuilderComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        applyGalleryPreset (row);
    }

    void ControlBuilderComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        applyGalleryPreset (row);
        exportButton.triggerClick();
    }

    void ControlBuilderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        auto top = getLocalBounds().removeFromTop (54);
        g.setColour (PatchCraftLookAndFeel::bg());
        g.fillRect (top);
        g.setColour (PatchCraftLookAndFeel::borderSoft());
        g.drawHorizontalLine (top.getBottom() - 1, 0.0f, (float) getWidth());

        auto gallery = getLocalBounds().withTrimmedTop (54).reduced (10);
        gallery = gallery.removeFromLeft (juce::jlimit (190, 250, gallery.getWidth() / 5));
        PatchCraftLookAndFeel::drawPanel (g, gallery, 8.0f);
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
        auto gallery = r.removeFromLeft (juce::jlimit (190, 250, r.getWidth() / 5)).reduced (10, 8);
        galleryTitle.setBounds (gallery.removeFromTop (22));
        galleryHint.setBounds (gallery.removeFromTop (34));
        gallery.removeFromTop (6);
        galleryList.setBounds (gallery);
        r.removeFromLeft (8);
        knobBuilder->setBounds   (r);
        sliderBuilder->setBounds (r);
        meterBuilder->setBounds  (r);
        aiImageBuilder->setBounds (r);
    }

} // namespace patchcraft
