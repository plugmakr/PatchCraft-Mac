#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;
    class KnobBuilderComponent;
    class SliderBuilderComponent;
    class MeterBuilderComponent;

    /**
        Combined Knob / Slider / Meter builder. One large window with a Kind
        dropdown that switches the editor between the three flavours, plus a
        live preview area. Replaces the previous three-tab layout.
    */
    class ControlBuilderComponent : public juce::Component
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

        std::unique_ptr<KnobBuilderComponent>   knobBuilder;
        std::unique_ptr<SliderBuilderComponent> sliderBuilder;
        std::unique_ptr<MeterBuilderComponent>  meterBuilder;

        void rebuildVisibility();
    };

} // namespace patchcraft
