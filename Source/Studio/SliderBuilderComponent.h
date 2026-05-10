#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Slider builder: orientation, colours, dimensions; preview + export JSON.
    */
    class SliderBuilderComponent : public juce::Component
    {
    public:
        explicit SliderBuilderComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        StudioMainComponent& owner;
        juce::ComboBox orientationBox, capBox, fillModeBox;
        juce::Slider widthSlider, heightSlider, trackSlider, thumbSlider, valueSlider, framesSlider;
        juce::ToggleButton scaleToggle { "Scale Marks" };
        juce::ToggleButton bipolarToggle { "Bipolar Center" };
        juce::ToggleButton labelToggle { "Value Label" };
        juce::Label  assetLbl, geometryLbl, paintLbl, behaviorLbl;
        juce::TextButton trackColourBtn { "Track" };
        juce::TextButton fillColourBtn { "Fill" };
        juce::TextButton thumbColourBtn { "Thumb" };
        juce::TextButton exportBtn { "Export PNG" };
        juce::TextButton exportJsonBtn { "Source JSON" };
        juce::TextButton addToProjectBtn { "Add To Library" };

        juce::Colour trackColour { 0xff202227 };
        juce::Colour fillColour { 0xfff5a623 };
        juce::Colour thumbColour { 0xffd9dde2 };

        void configureSlider (juce::Slider&, double, double, double, double, juce::String suffix = {});
        void cycleColour (juce::Colour&, juce::TextButton&);
        void drawSliderPreview (juce::Graphics&, juce::Rectangle<float>, float);
        juce::Image renderSliderFrame (int index, int totalFrames);
        juce::Image renderSliderFilmstrip (bool verticalStrip);
        juce::var buildSliderSourceVar (bool verticalStrip) const;
        bool writeSliderSourceJson (const juce::File&, bool verticalStrip, juce::String& error) const;
        void exportSliderFilmstrip();
        void exportSliderSourceJson();
        void addSliderToLibrary();
    };

} // namespace patchcraft
