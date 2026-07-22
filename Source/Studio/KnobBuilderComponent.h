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
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void addKnobToLibrary();
        static juce::StringArray galleryPresetNames();
        void applyGalleryPreset (int index);

    private:
        StudioMainComponent& owner;

        struct Style
        {
            int   size = 132;
            int   frames = 96;
            float previewValue = 0.62f;
            float startAngle = -135.0f;
            float endAngle = 135.0f;
            float ringThickness = 9.0f;
            float pointerWidth = 4.0f;
            float bevel = 0.52f;
            float glow = 0.38f;
            float importedBaseOpacity = 1.0f;
            float overlayOpacity = 1.0f;
            float imageScale = 1.0f;
            float imageOffsetX = 0.0f;
            float imageOffsetY = 0.0f;
            float imageRotation = 0.0f;
            float animationDepth = 0.35f;
            float surfaceTexture = 0.16f;
            float lightAngle = -45.0f;
            float ringInset = 0.0f;
            float pointerLength = 0.82f;
            float motionCurve = 0.50f;
            float backgroundTolerance = 0.18f;
            float maskRadius = 0.32f;
            float maskFeather = 0.08f;
            float maskOffsetX = 0.0f;
            float maskOffsetY = 0.0f;
            float pivotX = 0.50f;
            float pivotY = 0.50f;
            int   maskShape = 1;
            bool  ring = true;
            bool  ticks = true;
            bool  shadow = true;
            bool  label = false;
            bool  removeBackground = false;
            bool  maskEnabled = false;
            bool  positiveMask = true;
            bool  rotateMaskedRegion = false;
            bool  lockUnmaskedRegion = true;
            juce::Colour indicator { 0xfff5a623 };
            juce::Colour ringColour { 0xfff5a623 };
            juce::Colour backgroundColour { 0xff1a1c20 };
            juce::Colour borderColour { 0xff080a0d };
            juce::Colour tickColour { 0xff64d8ff };
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
                     importedBaseOpacitySlider, overlayOpacitySlider,
                     imageScaleSlider, imageOffsetXSlider, imageOffsetYSlider,
                     imageRotationSlider, animationDepthSlider,
                     surfaceTextureSlider, lightAngleSlider, ringInsetSlider,
                     pointerLengthSlider, motionCurveSlider, backgroundToleranceSlider,
                     maskRadiusSlider, maskFeatherSlider, maskOffsetXSlider,
                     maskOffsetYSlider, pivotXSlider, pivotYSlider;
        juce::ToggleButton ringToggle { "Ring" };
        juce::ToggleButton ticksToggle { "Tick Marks" };
        juce::ToggleButton shadowToggle { "Shadow" };
        juce::ToggleButton labelToggle { "Text Label" };
        juce::ToggleButton importedBaseToggle { "Use Imported Base" };
        juce::ToggleButton overlayToggle { "Overlay Edits" };
        juce::ToggleButton removeBackgroundToggle { "Remove BG" };
        juce::ToggleButton maskToggle { "Mask" };
        juce::ToggleButton positiveMaskToggle { "Positive" };
        juce::ToggleButton rotateMaskedToggle { "Animate Mask" };
        juce::ToggleButton lockUnmaskedToggle { "Lock Rest" };
        juce::ComboBox styleBox, indicatorBox, outputBox, imageRoleBox, imageFitBox, animationBox, maskShapeBox;
        juce::TextButton imageNudgeLeftBtn { "Img <" };
        juce::TextButton imageNudgeRightBtn { "Img >" };
        juce::TextButton imageNudgeUpBtn { "Img ^" };
        juce::TextButton imageNudgeDownBtn { "Img v" };
        juce::TextButton maskNudgeLeftBtn { "Mask <" };
        juce::TextButton maskNudgeRightBtn { "Mask >" };
        juce::TextButton maskNudgeUpBtn { "Mask ^" };
        juce::TextButton maskNudgeDownBtn { "Mask v" };
        juce::TextButton importBtn { "Import Knob..." };
        juce::TextButton clearImportBtn { "Clear Import" };
        juce::TextButton proDemoBtn { "Load Pro Demo" };
        juce::TextButton indicatorColourBtn { "Indicator" };
        juce::TextButton ringColourBtn { "Ring" };
        juce::TextButton backgroundColourBtn { "Face" };
        juce::TextButton borderColourBtn { "Border" };
        juce::TextButton tickColourBtn { "Ticks" };
        juce::TextButton   exportBtn { "Export Knob..." };
        juce::TextButton   exportJsonBtn { "Export JSON" };
        juce::TextButton   addToProjectBtn { "Add To Project" };
        juce::TextButton   publishBtn { "Publish" };

        juce::Label assetLbl, importLbl, geometryLbl, paintLbl, behaviorLbl, exportLbl;

        juce::Image importedStrip;
        juce::File importedSourceFile;
        bool importedStripVertical = true;
        int importedFrameCount = 0;
        int importedFrameSize = 0;
        mutable juce::Image cachedProcessedActiveFrame;
        mutable juce::Image cachedProcessedPassiveFrame;
        mutable juce::String cachedProcessedActiveKey;
        mutable juce::String cachedProcessedPassiveKey;
        std::vector<BuildLayer> buildLayers;
        std::vector<juce::Rectangle<int>> buildLayerRows;
        std::vector<int> buildLayerRowIndices;
        struct WorkbenchCard
        {
            juce::Rectangle<int> bounds;
            juce::String actionId;
        };
        std::vector<WorkbenchCard> workbenchCards;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> sliderLabelRects;
        int selectedBuildLayer = 1;
        juce::Rectangle<int> previewKnobBounds;
        bool draggingPreviewKnob = false;

        enum SectionIndex { AssetSection = 0, ImportSection, GeometrySection, PaintSection, BehaviorSection, OutputSection, SectionCount };
        std::array<bool, SectionCount> sectionOpen {{ true, true, true, true, true, true }};
        std::array<juce::Rectangle<int>, SectionCount> sectionHeaderBounds {};
        juce::Rectangle<int> rightPanelViewportBounds;
        int rightPanelScrollOffset = 0;
        int rightPanelContentHeight = 0;
        int rightPanelMaxScroll = 0;

        void exportKnobFilmstrip();
        void exportKnobSourceJson();
        juce::var buildKnobSourceVar() const;
        bool writeKnobSourceJson (const juce::File& destination, juce::String& error) const;
        bool loadKnobSourceJson (const juce::File& source, juce::String& error);
        bool loadKnobManFile (const juce::File& source, juce::String& error);
        bool writeKnobAssetPackage (const juce::File& folder, juce::File& renderedPng, juce::String& error);
        void syncControlsFromStyle();
        void importExistingKnob();
        void clearImportedKnob();
        void loadProDemoKnob();
        void publishKnobToPluginClub();
        void detectImportedFilmstripLayout();
        bool hasImportedKnob() const noexcept;
        juce::Image getImportedFrame (float position) const;
        juce::Image processImportedFrame (const juce::Image& source, bool activeMaskRegion) const;
        void invalidateImportedProcessingCache() const;
        void normaliseImportedWorkingImage();
        bool scrollRightPanel (int deltaPixels);
        void updateStyleFromControls();
        void updatePreviewValueFromPoint (juce::Point<int> point);
        void nudgeImportedImage (float deltaX, float deltaY);
        void nudgeMask (float deltaX, float deltaY);
        void openSectionForLayer (const juce::String& layerName);
        bool isBuildLayerVisible (const juce::String& layerName) const;
        void applyWorkbenchAction (const juce::String& actionId);
        void drawWorkbenchCard (juce::Graphics&, juce::Rectangle<int>, const juce::String& title,
                                const juce::String& body, const juce::String& actionId,
                                juce::Colour accent);
        void configureSlider (juce::Slider&, double min, double max, double step, double value, juce::String suffix = {});
        void drawKnob (juce::Graphics&, juce::Rectangle<float>, float position, bool compact);
        void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
        void showColourPicker (juce::Colour initial, std::function<void (juce::Colour)> onChange);
        void cycleColour (juce::Colour& colour, juce::TextButton& button);
        void updateColourButtonText();
        juce::Image renderKnobFrame (int idx, int total);
    };

} // namespace patchcraft
