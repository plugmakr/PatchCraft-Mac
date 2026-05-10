#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Right-hand inspector showing properties of the selected element.
        Mirrors the reference image: Type, ID, Position, Size, Parameter,
        Label, Value Format, Style, Knob Style, Min, Max, Default, Step,
        Value Type, Smoothing + Actions row.
    */
    class InspectorPanel : public juce::Component
    {
    public:
        explicit InspectorPanel (StudioMainComponent& owner);
        ~InspectorPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void selectionChanged();
        void refresh();

    private:
        StudioMainComponent& owner;

        // Header label
        juce::Label header;

        // Two-column rows
        struct Field
        {
            juce::Label label;
            std::unique_ptr<juce::Component> control;
            int rowSpan = 1;
        };

        // ---- Controls --------------------------------------------------------
        juce::ComboBox typeBox;
        juce::TextEditor idEdit;
        juce::TextEditor xEdit, yEdit, wEdit, hEdit;
        juce::ComboBox  parameterBox;
        juce::TextButton midiLearnButton { "Learn" };
        juce::TextEditor labelEdit;
        juce::ComboBox  valueFormatBox;
        juce::ComboBox  styleBox;
        juce::ComboBox  knobStyleBox;
        juce::Slider    opacitySlider;
        juce::ToggleButton visibleToggle { "Visible" };
        juce::ToggleButton lockedToggle  { "Locked" };
        juce::ComboBox  shapeKindBox;
        juce::Slider    cornerSlider;
        juce::Slider    strokeSlider;
        juce::Slider    shadowSlider;
        juce::Slider    glowSlider;
        juce::Slider    blurSlider;
        juce::ToggleButton audioReactiveToggle { "Audio Reactive" };
        juce::ComboBox  audioReactiveModeBox;
        juce::Slider    audioReactiveAmountSlider;
        juce::ComboBox  animationModeBox;
        juce::Slider    animationRateSlider;
        juce::ComboBox  labelPositionBox;
        juce::Slider    labelOffsetXSlider;
        juce::Slider    labelOffsetYSlider;
        juce::Slider    labelSpacingSlider;
        juce::Slider    labelSizeSlider;
        juce::TextEditor backgroundColourEdit;
        juce::TextEditor borderColourEdit;
        juce::TextEditor accentColourEdit;
        juce::TextButton backgroundColourButton { "Pick" };
        juce::TextButton borderColourButton { "Pick" };
        juce::TextButton accentColourButton { "Pick" };
        juce::TextEditor minEdit, maxEdit, defaultEdit, stepEdit;
        juce::ComboBox  valueTypeBox;
        juce::Slider    smoothingSlider;

        juce::TextButton btnDuplicate { "Duplicate" };
        juce::TextButton btnDelete    { "Delete" };
        juce::TextButton btnForward   { "Forward" };
        juce::TextButton btnBackward  { "Backward" };

        // Image-only controls
        juce::TextEditor assetEdit;
        juce::TextButton browseAssetBtn { "Browse..." };
        juce::Label      lblAsset;

        // Group / page membership (any element).
        juce::ComboBox   containerBox;
        juce::Label      lblGroup;

        // TabPanel-only: list of tab names + add/remove.
        juce::TextEditor tabsEdit;     // newline-separated labels
        juce::Label      lblTabs;

        // Container manager for Panel / Group / TabPanel elements.
        juce::ComboBox   containerChildrenBox;
        juce::TextButton containerAddSelectedBtn { "Add Sel" };
        juce::TextButton containerRemoveChildBtn { "Remove" };
        juce::TextButton containerSelectChildBtn { "Select" };
        juce::TextButton containerAddTabBtn { "+ Tab" };
        juce::TextButton containerRemoveTabBtn { "- Tab" };

        // Filmstrip override (Knob / Slider / Meter only)
        juce::TextEditor filmstripPathEdit;
        juce::TextButton filmstripBrowseBtn { "Image..." };
        juce::TextEditor filmstripFramesEdit;
        juce::TextButton filmstripAutoBtn   { "Auto" };
        juce::Label      lblFilmstripPath;
        juce::Label      lblFilmstripFrames;

        bool inhibitCallbacks = false;

        void hookEdit (juce::TextEditor& e, std::function<void (const juce::String&)>);
        void hookCombo (juce::ComboBox& c, std::function<void (int)>);

        void writeFromUi();
        void showColourPicker (const juce::String& title, juce::TextEditor& target, juce::Colour current);
        void layoutRow (juce::Rectangle<int>& area, juce::Label& label,
                        juce::Component* control, int height = 26);
        void layoutColourRow (juce::Rectangle<int>& area, juce::Label& label,
                              juce::TextEditor& editor, juce::TextButton& button);
        void layoutDoubleRow (juce::Rectangle<int>& area,
                              juce::Label& l1, juce::Component& c1, juce::String mid,
                              juce::Label& l2, juce::Component& c2,
                              int height = 26);

        // Persistent labels (rebuilt at construction)
        juce::Label lblType, lblId, lblPos, lblSize, lblParam, lblLabel,
                    lblValFmt, lblStyle, lblKnobStyle, lblMin, lblMax,
                    lblOpacity, lblState, lblShapeKind, lblCorner, lblStroke,
                    lblShadow, lblGlow, lblBlur, lblAudioReactive, lblAudioReactiveMode,
                    lblAudioReactiveAmount, lblAnimationMode, lblAnimationRate,
                    lblLabelPosition, lblLabelOffsetX, lblLabelOffsetY,
                    lblLabelSpacing, lblLabelSize, lblBgColour, lblBorderColour, lblAccentColour,
                    lblContainerManager, lblContainerChildren,
                    lblDefault, lblStep, lblValType, lblSmoothing,
                    lblActions, lblPosX, lblPosY, lblSizeW, lblSizeH;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
    };

} // namespace patchcraft
