#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <functional>
#include <vector>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Knob builder: live preview + style controls (Size, Ring, Indicator,
        Ring Color, Background, Border, Style, Frames, Export Knob...)
    */
    class KnobBuilderComponent : public juce::Component
    {
    public:
        explicit KnobBuilderComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void addKnobToLibrary();

    private:
        StudioMainComponent& owner;

        struct Style
        {
            int   size = 112;
            int   frames = 64;
            float previewValue = 0.62f;
            float startAngle = -135.0f;
            float endAngle = 135.0f;
            float ringThickness = 7.0f;
            float pointerWidth = 3.0f;
            float bevel = 0.35f;
            float glow = 0.22f;
            float importedBaseOpacity = 1.0f;
            float overlayOpacity = 1.0f;
            bool  ring = true;
            bool  ticks = true;
            bool  shadow = true;
            bool  label = true;
            juce::Colour indicator { 0xfff5a623 };
            juce::Colour ringColour { 0xfff5a623 };
            juce::Colour backgroundColour { 0xff1a1c20 };
            juce::Colour borderColour { 0xff080a0d };
            juce::Colour tickColour { 0xff7a8088 };
            juce::String name { "PatchCraft Pro Knob" };
        } style;

        struct BuildLayer
        {
            juce::String name;
            bool visible = true;
        };

        juce::TextEditor nameEdit;
        juce::Slider sizeSlider, valueSlider, startSlider, endSlider, ringWidthSlider,
                     pointerWidthSlider, bevelSlider, glowSlider, framesSlider,
                     importedBaseOpacitySlider, overlayOpacitySlider;
        juce::ToggleButton ringToggle { "Ring" };
        juce::ToggleButton ticksToggle { "Tick Marks" };
        juce::ToggleButton shadowToggle { "Shadow" };
        juce::ToggleButton labelToggle { "Text Label" };
        juce::ToggleButton importedBaseToggle { "Use Imported Base" };
        juce::ToggleButton overlayToggle { "Overlay Edits" };
        juce::ComboBox styleBox, indicatorBox, outputBox;
        juce::TextButton importBtn { "Import Knob..." };
        juce::TextButton clearImportBtn { "Clear Import" };
        juce::TextButton indicatorColourBtn { "Indicator" };
        juce::TextButton ringColourBtn { "Ring" };
        juce::TextButton backgroundColourBtn { "Face" };
        juce::TextButton borderColourBtn { "Border" };
        juce::TextButton tickColourBtn { "Ticks" };
        juce::TextButton   exportBtn { "Export Knob..." };
        juce::TextButton   exportJsonBtn { "Export JSON" };
        juce::TextButton   addToProjectBtn { "Add To Project" };

        juce::Label assetLbl, importLbl, geometryLbl, paintLbl, behaviorLbl, exportLbl;

        juce::Image importedStrip;
        juce::File importedSourceFile;
        bool importedStripVertical = true;
        int importedFrameCount = 0;
        int importedFrameSize = 0;
        std::vector<BuildLayer> buildLayers;
        std::vector<juce::Rectangle<int>> buildLayerRows;
        std::vector<int> buildLayerRowIndices;
        int selectedBuildLayer = 1;
        juce::Rectangle<int> previewKnobBounds;
        bool draggingPreviewKnob = false;

        enum SectionIndex { AssetSection = 0, ImportSection, GeometrySection, PaintSection, BehaviorSection, OutputSection, SectionCount };
        std::array<bool, SectionCount> sectionOpen {{ true, true, false, false, false, true }};
        std::array<juce::Rectangle<int>, SectionCount> sectionHeaderBounds {};

        void exportKnobFilmstrip();
        void exportKnobSourceJson();
        juce::var buildKnobSourceVar() const;
        bool writeKnobSourceJson (const juce::File& destination, juce::String& error) const;
        bool loadKnobSourceJson (const juce::File& source, juce::String& error);
        void syncControlsFromStyle();
        void importExistingKnob();
        void clearImportedKnob();
        void detectImportedFilmstripLayout();
        bool hasImportedKnob() const noexcept;
        juce::Image getImportedFrame (float position) const;
        void updateStyleFromControls();
        void updatePreviewValueFromPoint (juce::Point<int> point);
        void openSectionForLayer (const juce::String& layerName);
        bool isBuildLayerVisible (const juce::String& layerName) const;
        void configureSlider (juce::Slider&, double min, double max, double step, double value, juce::String suffix = {});
        void drawKnob (juce::Graphics&, juce::Rectangle<float>, float position, bool compact);
        void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
        void showColourPicker (juce::Colour initial, std::function<void (juce::Colour)> onChange);
        void cycleColour (juce::Colour& colour, juce::TextButton& button);
        void updateColourButtonText();
        juce::Image renderKnobFrame (int idx, int total);
    };

} // namespace patchcraft
