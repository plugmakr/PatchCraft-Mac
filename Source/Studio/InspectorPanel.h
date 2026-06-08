#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"

#include <array>
#include <map>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Right-hand inspector showing properties of the selected element.
        Mirrors the reference image: Type, ID, Position, Size, Parameter,
        Label, Value Format, Style, Knob Style, Min, Max, Default, Step,
        Value Type, Smoothing + Actions row.
    */
    class InspectorPanel : public juce::Component,
                           private juce::ChangeListener
    {
    public:
        explicit InspectorPanel (StudioMainComponent& owner);
        ~InspectorPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        void selectionChanged();
        void refresh();

    private:
        StudioMainComponent& owner;
        void changeListenerCallback (juce::ChangeBroadcaster*) override;

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
        juce::ComboBox  dropZoneLinkBox;
        juce::TextButton midiLearnButton { "Learn" };
        juce::TextEditor labelEdit;
        juce::TextEditor actionEdit;
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
        juce::Slider    contentPaddingSlider;
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
        juce::TextButton btnCopy      { "Copy" };
        juce::TextButton btnCopyNoParams { "Copy -P" };
        juce::TextButton btnPaste     { "Paste" };
        juce::TextButton btnAllTabs   { "All Tabs" };
        juce::ToggleButton copyTabsAsReferenceToggle { "Copy as linked reference" };
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

        // Drum Grid editor. A Drum Grid on the canvas is a visual/editor
        // surface for the project's drum-machine DSP block.
        juce::Label lblDrumGrid, lblDrumPattern, lblDrumTracks, lblDrumSteps,
                    lblDrumCell, lblDrumVelocity, lblDrumGate, lblDrumProbability,
                    lblDrumDivision, lblDrumPadFxTarget, lblDrumPadFxAmount,
                    lblDrumCellFxTarget, lblDrumCellFxAmount;
        juce::ComboBox drumPatternBox;
        juce::ComboBox drumTrackBox;
        juce::ComboBox drumStepBox;
        juce::ComboBox drumDivisionBox;
        juce::ComboBox drumPadFxTargetBox;
        juce::ComboBox drumCellFxTargetBox;
        juce::Slider drumTracksSlider;
        juce::Slider drumStepsSlider;
        juce::Slider drumVelocitySlider;
        juce::Slider drumGateSlider;
        juce::Slider drumProbabilitySlider;
        juce::Slider drumPadFxAmountSlider;
        juce::Slider drumCellFxAmountSlider;
        juce::ToggleButton drumCellEnabledToggle { "Hit On" };
        juce::TextButton drumApplyTrapRollBtn { "Trap Roll" };
        juce::TextButton drumClearPatternBtn { "Clear" };
        juce::TextButton drumCopyPatternBtn { "Copy" };
        juce::TextButton drumPastePatternBtn { "Paste" };
        juce::TextButton drumDuplicatePatternBtn { "Dup Next" };
        juce::TextButton drumOpenPerformanceBtn { "Full Editor" };

        // Arp lane authoring (Arp Lane only).
        juce::Label lblArpLane, lblArpLaneIndex, lblArpLaneSteps, lblArpLaneMode,
                    lblArpLaneTarget, lblArpLaneRootNote, lblArpLaneSampleSlots,
                    lblArpLaneDirection, lblArpLaneRotate, lblArpLaneEuclideanPulses,
                    lblArpLaneProbability, lblArpLaneRatchet, lblArpLaneFillPulses,
                    lblArpLaneFillProbability;
        juce::Slider arpLaneIndexSlider;
        juce::Slider arpLaneStepsSlider;
        juce::Slider arpLaneRootNoteSlider;
        juce::Slider arpLaneSampleSlotsSlider;
        juce::Slider arpLaneRotateSlider;
        juce::Slider arpLaneEuclideanPulsesSlider;
        juce::Slider arpLaneProbabilitySlider;
        juce::Slider arpLaneRatchetSlider;
        juce::Slider arpLaneFillPulsesSlider;
        juce::Slider arpLaneFillProbabilitySlider;
        juce::ComboBox arpLaneModeBox;
        juce::ComboBox arpLaneTargetBox;
        juce::ComboBox arpLaneDirectionBox;
        juce::TextButton arpLaneOpenPerformanceBtn { "Edit Bank" };
        juce::TextButton arpLaneApplySampleTargetBtn { "Build Ring" };

        // Mixer authoring (Mixer only).
        juce::Label lblMixer, lblMixerMode, lblMixerChannels, lblMixerLabels,
                    lblMixerVolumes, lblMixerPans, lblMixerMutes, lblMixerSolos;
        juce::ComboBox mixerModeBox;
        juce::Slider mixerChannelsSlider;
        juce::TextEditor mixerLabelsEdit;
        juce::TextEditor mixerVolumeParamsEdit;
        juce::TextEditor mixerPanParamsEdit;
        juce::TextEditor mixerMuteParamsEdit;
        juce::TextEditor mixerSoloParamsEdit;
        juce::Label mixerHelpLabel;

        // Macro / modulation authoring.
        juce::Label lblMacroEditor, lblMacroTargets, lblModMatrixEditor, lblModRoutes;
        juce::TextEditor macroTargetsEdit;
        juce::TextButton macroApplyBtn { "Apply" };
        juce::TextButton macroClearBtn { "Clear" };
        juce::TextEditor modRoutesEdit;
        juce::TextButton modApplyBtn { "Apply" };
        juce::TextButton modClearBtn { "Clear" };

        juce::Label lblGranularEditor, lblGranularDirection, lblGranularDensity,
                    lblGranularSize, lblGranularRandom, lblGranularSpread,
                    lblGranularScan, lblGranularPitch, lblGranularPan,
                    lblGranularTexture;
        juce::ToggleButton granularOnToggle { "Enabled" };
        juce::ToggleButton granularFreezeToggle { "Freeze" };
        juce::ToggleButton granularReverseToggle { "Reverse" };
        juce::ComboBox granularDirectionBox;
        juce::Slider granularDensitySlider;
        juce::Slider granularSizeSlider;
        juce::Slider granularRandomSlider;
        juce::Slider granularSpreadSlider;
        juce::Slider granularScanSlider;
        juce::Slider granularPitchSlider;
        juce::Slider granularPanSlider;
        juce::Slider granularTextureSlider;

        bool hasDrumPatternClipboard = false;
        int drumPatternClipboardTracks = 0;
        int drumPatternClipboardSteps = 0;
        std::map<juce::String, float> drumPatternClipboard;

        bool inhibitCallbacks = false;
        juce::ColourSelector* liveColourSelector = nullptr;
        juce::TextEditor* liveColourTarget = nullptr;
        juce::Colour liveColourOriginal { juce::Colours::transparentBlack };

        enum class InspectorSection
        {
            Layout = 0,
            Parameter,
            Style,
            Advanced,
            Actions,
            DrumGrid,
            ArpLane,
            Mixer,
            Macro,
            ModMatrix,
            Granular,
            Container,
            Count
        };

        std::array<bool, (size_t) InspectorSection::Count> sectionOpen {{
            true, true, true, true, true, true, true, true, true, true, true, true
        }};
        std::array<juce::Rectangle<int>, (size_t) InspectorSection::Count> sectionHeaderBounds {};

        bool isSectionOpen (InspectorSection section) const;

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

        DspBlock* findDrumMachineBlock();
        const DspBlock* findDrumMachineBlock() const;
        DspBlock& ensureDrumMachineBlock();
        juce::String drumPrefix (int pattern, int track, int step) const;
        float drumValue (const juce::String& key, float fallback) const;
        void setDrumValue (const juce::String& key, float value, bool notify = true);
        void refreshDrumControls();
        void writeDrumGridElementFromUi();
        void writeDrumTrackFxFromUi();
        void writeDrumCellFromUi();
        void clearCurrentDrumPattern();
        void copyCurrentDrumPattern();
        void pasteCurrentDrumPattern();
        void duplicateCurrentDrumPatternToNext();
        void applyTrapRollToCurrentPattern();
        DspBlock* findMidiPlaygroundBlock();
        DspBlock& ensureMidiPlaygroundBlock();
        void writeArpLaneSampleTargetFromUi (bool applyToBank);
        void refreshMacroControls();
        void writeMacroTargetsFromUi();
        void refreshModMatrixControls();
        void writeModRoutesFromUi();
        float granularValue (const juce::String& parameterId, float fallback) const;
        void setGranularValue (const juce::String& parameterId, float value, bool notify = true);
        void refreshGranularControls();
        void writeGranularControlsFromUi();

        // Persistent labels (rebuilt at construction)
        juce::Label lblType, lblId, lblPos, lblSize, lblParam, lblDropZoneLink, lblLabel, lblAction,
                    lblValFmt, lblStyle, lblKnobStyle, lblMin, lblMax,
                    lblLayoutSection, lblParameterSection, lblStyleSection, lblSpecialSection,
                    lblOpacity, lblState, lblShapeKind, lblCorner, lblStroke,
                    lblShadow, lblGlow, lblBlur, lblAudioReactive, lblAudioReactiveMode,
                    lblAudioReactiveAmount, lblAnimationMode, lblAnimationRate,
                    lblLabelPosition, lblLabelOffsetX, lblLabelOffsetY,
                    lblLabelSpacing, lblLabelSize, lblContentPadding, lblBgColour, lblBorderColour, lblAccentColour,
                    lblContainerManager, lblContainerChildren,
                    lblDefault, lblStep, lblValType, lblSmoothing,
                    lblActions, lblPosX, lblPosY, lblSizeW, lblSizeH;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
    };

} // namespace patchcraft
