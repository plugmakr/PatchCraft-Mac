#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;
    class KnobBuilderComponent;
    class SliderBuilderComponent;
    class MeterBuilderComponent;
    class AiImageBuilderComponent;

    /**
        Combined Knob / Slider / Meter builder. One large window with a Kind
        dropdown that switches the editor between the three flavours, plus a
        live preview area. Replaces the previous three-tab layout.
    */
    class ControlBuilderComponent : public juce::Component,
                                    private juce::ListBoxModel
    {
    public:
        explicit ControlBuilderComponent (StudioMainComponent& owner);
        ~ControlBuilderComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        StudioMainComponent& owner;

        juce::Label    title;
        juce::Label    subtitle;
        juce::Label    kindLabel;
        juce::ComboBox kindBox;
        juce::TextButton newAssetButton { "New Asset" };
        juce::TextButton duplicateButton { "Duplicate" };
        juce::TextButton exportButton { "Export" };
        juce::Label galleryTitle;
        juce::Label galleryHint;
        juce::ListBox galleryList { "WidgetGallery", this };

        std::unique_ptr<KnobBuilderComponent>   knobBuilder;
        std::unique_ptr<SliderBuilderComponent> sliderBuilder;
        std::unique_ptr<MeterBuilderComponent>  meterBuilder;
        std::unique_ptr<AiImageBuilderComponent> aiImageBuilder;

        void rebuildVisibility();
        juce::StringArray getCurrentGalleryNames() const;
        void applyGalleryPreset (int row);

        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        int selectedGalleryRow = 0;
    };

} // namespace patchcraft
