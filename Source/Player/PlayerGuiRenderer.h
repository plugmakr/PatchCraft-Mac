#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PatchCraftPackFormat.h"
#include "AssetManager.h"

#include <array>
#include <functional>
#include <map>
#include <vector>

namespace patchcraft
{
    class PlayerProcessor;

    /**
        Renders a loaded pack's layout.json as a live, parameter-bound UI.

        - Real juce::Slider widgets are spawned for Knob and Slider elements
          and attached to APVTS slots (so DAW automation works).
        - All other element types (Image, Label, Meter, Keyboard, Panel,
          Dropdown, TabPanel) are drawn directly in paint() so we don't
          require dedicated Component subclasses for each.
        - Click handling on the panel:
            TabPanel -> switch the active group (filters which controls are
                        visible, mirroring the Studio canvas behaviour).
            Dropdown id="presets" -> show preset menu.
    */
    class PlayerGuiRenderer : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              public juce::TooltipClient,
                              private juce::Timer
    {
    public:
        PlayerGuiRenderer (PlayerProcessor& proc, AssetManager& assets);
        ~PlayerGuiRenderer() override;

        std::function<void (const juce::String& report)> onRuntimeImportReport;
        std::function<void()> onPresetBrowserRequested;

        void rebuild();

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray&, int, int) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragMove (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        juce::String getTooltip() override;
        void timerCallback() override;

    private:
        PlayerProcessor& proc;
        AssetManager&    assets;

        juce::OwnedArray<juce::Component> controls;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;

        juce::Image background;
        juce::Image heroImage;
        std::vector<LayoutElement> elementsCopy;
        mutable std::map<juce::String, ParameterDef> registryParameterCache;

        juce::String currentTabGroup { "main" };
        std::map<juce::String, juce::String> activeTabGroupsByPanel;
        std::map<juce::String, bool> manualContainerOpen;
        juce::StringArray manualContainerOrder;
        int lastPlayedNote = -1;
        int activePadNote = -1;
        juce::String activeMomentaryParameter;
        float lastOutputPeak = 0.0f;
        bool hasMeterOrReactiveElement = false;
        bool hasContinuousFxElement = false;
        // Rolling output-level history (newest at back) for waveform/scope FX.
        static constexpr int kLevelHistorySize = 128;
        std::array<float, kLevelHistorySize> levelHistory {};
        int levelHistoryHead = 0;
        juce::String lastPendingMidiLearn;
        bool drumGridDragActive = false;
        bool drumGridPaintValue = false;
        bool pianoRollDragActive = false;
        juce::String pianoRollDragElementId;
        int pianoRollDragPitch = -1;
        int pianoRollDragStartStep = -1;
        bool arpMidiDragArmed = false;
        int lastDrumGridPattern = -1;
        int lastDrumGridTrack = -1;
        int lastDrumGridStep = -1;
        juce::Point<int> arpMidiDragStart;
        juce::String arpMidiDragElementId;

        // Geometry helpers
        struct CanvasMetrics { float scale; juce::Rectangle<int> canvas; };
        CanvasMetrics metrics() const;
        juce::Rectangle<int> elementRect (const LayoutElement&, const CanvasMetrics&) const;
        bool isElementOnCurrentTab (const LayoutElement&) const;
        bool isManualContainerGroup (const juce::String& groupId) const;
        bool triggerManualContainer (const LayoutElement& element);
        void initialiseManualContainers();
        int parameterIndexForId (const juce::String&) const;
        const ParameterDef* parameterForId (const juce::String&) const;
        bool parameterIsEnabled (const ParameterDef&) const;
        void refreshControlEnablement();
        float parameterValueForElement (const LayoutElement&, float fallback = 0.0f) const;
        juce::String formattedParameterValue (const LayoutElement&) const;
        juce::Rectangle<int> animatedElementRect (const LayoutElement&, juce::Rectangle<int>) const;
        juce::Rectangle<int> tabBounds (juce::Rectangle<int>, int tabIndex, int tabCount) const;
        void drawLabelElement (juce::Graphics&, const LayoutElement&, juce::Rectangle<int>) const;
        void drawControlLabelOverlay (juce::Graphics&, const LayoutElement&, juce::Rectangle<int>) const;
        juce::Rectangle<int> controlBodyRect (const LayoutElement&, juce::Rectangle<int>) const;
        void drawRuntimeControl (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;

        // Per-type drawing
        void drawHeroPlaceholder (juce::Graphics&, juce::Rectangle<int>) const;
        void drawMeter   (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawEqCurve (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawSpectrumAnalyzer (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        // Curated runtime visual FX (audio-reactive special effects).
        float reactiveLevelFor (const LayoutElement&) const;
        void drawVisualFx (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&, float level) const;
        void drawPanel   (juce::Graphics&, juce::Rectangle<int>, const juce::String& label) const;
        void drawButton  (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawValueDisplay (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawDropdown(juce::Graphics&, juce::Rectangle<int>, const juce::String& display) const;
        void drawKeyboard(juce::Graphics&, juce::Rectangle<int>) const;
        void drawTabPanel(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawXYPad  (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawGranularField (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawPadGrid(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawDrumGrid(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawArpLane(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawSequencerLane(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawPianoRoll(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawMixer  (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawRuntimeSampleLibrary (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawMultiLayerDock (juce::Graphics&, juce::Rectangle<int>);
        juce::String tabTargetGroup (const LayoutElement&, const juce::String& label) const;
        bool tabTargetIsGlobal (const LayoutElement&, const juce::String& targetGroup) const;

        int hitTabIndex (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>) const;
        const Manifest* manifest() const;
        juce::Colour playerBg() const;
        juce::Colour playerPanel() const;
        juce::Colour playerAccent() const;
        juce::Colour playerText() const;
        juce::Colour playerTextDim() const;
        juce::Colour playerBorder() const;

        const LayoutElement* findBindableElementAt (juce::Point<int>) const;
        void showControlContextMenu (const LayoutElement&, const juce::Point<int>& screenPos);
        void showPresetMenu (const juce::Point<int>& screenPos);
        void showParameterMenu (const LayoutElement&, const juce::Point<int>& screenPos);
        void showDrumLaneMenu (int track, const juce::Point<int>& screenPos);
        juce::String drumTrackLabel (const DspBlock* block, int track) const;
        int drumTrackNote (const DspBlock* block, int track) const;
        bool drumTrackMuted (const DspBlock* block, int track) const;
        bool drumTrackSoloed (const DspBlock* block, int track) const;
        int noteForKeyboardPosition (juce::Rectangle<int> r, juce::Point<int> pos) const;
        void handleKeyboardClick (juce::Rectangle<int> r, juce::Point<int> pos);
        bool handleMultiLayerDockClick (juce::Point<int>);
        bool handleXYPadGesture (const juce::MouseEvent&);
        bool handleGranularGesture (const juce::MouseEvent&);
        bool advanceGranularFields();
        bool handlePadClick (const juce::MouseEvent&);
        bool handleDrumGridGesture (const juce::MouseEvent&, bool drag);
        bool handleArpLaneGesture (const juce::MouseEvent&, bool drag);
        bool handlePianoRollGesture (const juce::MouseEvent&, bool drag);
        bool handlePianoRollSurface (const LayoutElement&, juce::Rectangle<int> bounds,
                                     const juce::MouseEvent&, bool drag);
        void showPianoRollEditorModal (const LayoutElement& element);
        void cancelPianoRollDrag();
        friend struct PianoRollModalContent;
        struct PianoRollGeometry
        {
            bool valid = false;
            juce::Rectangle<int> header;
            juce::Rectangle<int> expandButton;
            juce::Rectangle<int> playButton;
            juce::Rectangle<int> gutter;
            juce::Rectangle<int> grid;
            int steps = 16;
            int stepsPerBeat = 4;
            int rows = 25;
            int lowNote = 48;
            float cellW = 0.0f;
            float cellH = 0.0f;
        };
        PianoRollGeometry pianoRollGeometry (const LayoutElement&, juce::Rectangle<int>) const;
        bool pianoRollCellAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>,
                              int& pitch, int& step) const;
        bool handleSequencerLaneGesture (const juce::MouseEvent&, bool drag);
        bool arpLaneStepAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>,
                            int& lane, int& step, float& velocity) const;
        juce::Rectangle<int> arpLaneMidiDragHandleBounds (juce::Rectangle<int>) const;
        juce::Rectangle<int> arpLanePlayButtonBounds (juce::Rectangle<int>) const;
        juce::Rectangle<int> arpLaneBypassButtonBounds (juce::Rectangle<int>) const;
        juce::Rectangle<int> arpLaneSoloButtonBounds (juce::Rectangle<int>) const;
        bool startArpLaneMidiDrag (const LayoutElement&);
        bool handleMixerGesture (const juce::MouseEvent&, bool drag);
        int  padNoteAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>) const;
        struct RuntimeDropTarget
        {
            juce::String mappingMode { "pads" };
            int note = -1;
            int padIndex = -1;
            bool targeted = false;
        };
        RuntimeDropTarget runtimeDropTargetAt (juce::Point<int>) const;
        void updateFileDragHighlight (juce::Point<int>);
        bool fileDragActive = false;
        juce::Rectangle<int> fileDragHighlight;
        juce::String fileDragLabel;
        bool drumCellAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>,
                         int& pattern, int& track, int& step, float& velocity,
                         float& gate, float& probability, bool& active,
                         int& note, int& divisions) const;

        bool multiLayerDockCollapsed = true;
        bool mixerDragActive = false;
        bool arpLaneDragActive = false;
        int lastArpLane = -1;
        int lastArpStep = -1;
        bool seqLaneDragActive = false;
        int lastSeqStep = -1;
        juce::String lastSeqElementId;
        double lastGranularAdvanceSeconds = 0.0;
        int mixerDragChannel = -1;
        juce::String mixerDragElementId;
        juce::String mixerDragKind;
        juce::Rectangle<int> multiLayerDockBounds;
        juce::Rectangle<int> multiLayerToggleBounds;
        std::vector<juce::Rectangle<int>> multiLayerMuteBounds;
        std::vector<juce::Rectangle<int>> multiLayerSoloBounds;
        std::vector<juce::Rectangle<int>> multiLayerVolumeBounds;
        std::vector<juce::Rectangle<int>> multiLayerPanBounds;
        std::vector<juce::Rectangle<int>> multiLayerCollapseBounds;
        std::vector<bool> multiLayerRowCollapsed;
    };

} // namespace patchcraft
