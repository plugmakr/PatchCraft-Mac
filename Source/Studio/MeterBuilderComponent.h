#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class MeterBuilderComponent : public juce::Component
    {
    public:
        explicit MeterBuilderComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void addMeterToLibrary();
        static juce::StringArray galleryPresetNames();
        void applyGalleryPreset (int index);

    private:
        StudioMainComponent& owner;
        juce::ComboBox orientationBox, styleBox, scaleBox;
        juce::Slider widthSlider, heightSlider, segmentSlider, valueSlider, warningSlider, peakSlider;
        juce::ToggleButton peakHoldToggle { "Peak Hold" };
        juce::ToggleButton dbScaleToggle { "dB Scale" };
        juce::ToggleButton stereoToggle { "Stereo Pair" };
        juce::Label assetLbl, geometryLbl, behaviorLbl;
        juce::TextButton lowColourBtn { "Low" };
        juce::TextButton midColourBtn { "Mid" };
        juce::TextButton highColourBtn { "High" };
        juce::TextButton exportBtn { "Export PNG" };
        juce::TextButton exportJsonBtn { "Source JSON" };
        juce::TextButton addToProjectBtn { "Add To Library" };

        juce::Label imageLbl;
        juce::TextButton importBgBtn { "Empty Img" };
        juce::TextButton importFillBtn { "Fill Img" };
        juce::TextButton importStripBtn { "Filmstrip" };
        juce::TextButton clearImageBtn { "Clear Img" };

        juce::Colour lowColour { 0xff5fb37b };
        juce::Colour midColour { 0xfff5a623 };
        juce::Colour highColour { 0xffe24d42 };

        // Imported artwork. A ready-made filmstrip is used as-is; an empty/fill
        // image pair is composited into a level-reveal filmstrip.
        juce::Image importedBg;
        juce::Image importedFill;
        juce::Image importedStrip;
        juce::File  importedSource;
        bool hasImportedStrip() const { return importedStrip.isValid(); }

        void configureSlider (juce::Slider&, double, double, double, double, juce::String suffix = {});
        void cycleColour (juce::Colour&, juce::TextButton&);
        void drawMeterPreview (juce::Graphics&, juce::Rectangle<float>);
        juce::Image renderMeterFrame (int index, int totalFrames);
        juce::Image renderMeterFilmstrip (bool verticalStrip);
        juce::var buildMeterSourceVar (bool verticalStrip) const;
        bool writeMeterSourceJson (const juce::File&, bool verticalStrip, juce::String& error) const;
        void exportMeterFilmstrip();
        void exportMeterSourceJson();
        void importImage (int slot);   // 0 = empty/bg, 1 = fill, 2 = filmstrip
    };

} // namespace patchcraft
