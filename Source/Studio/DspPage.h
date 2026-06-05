#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "DspRoutingEngine.h"
#include "PatchCraftProject.h"
#include "PromptToPluginComponent.h"

#include <array>
#include <atomic>
#include <map>

namespace patchcraft
{
    class StudioMainComponent;
    class EffectEngine;

    class DspPage : public juce::Component,
                    public juce::AudioIODeviceCallback,
                    private PatchCraftProject::Listener,
                    private LiveValueStore::Listener,
                    private juce::Timer
    {
    public:
        explicit DspPage (PatchCraftProject& project, bool quickEditMode = false, StudioMainComponent* owner = nullptr);
        ~DspPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void refresh();
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext (const float* const*, int, float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        juce::String getCurrentPatchSectionId() const;
        juce::String getCurrentPatchSectionLabel() const;
        void beginGuidedTutorial();
        // Programmatic equivalent of the BuilderPanel's "+ Perf" button -
        // switches the DSP builder to the Performance/MIDI tab and drops in a default
        // arpeggiator block. Used by the canvas right-click "Add ARP" shortcut.
        void addArpBlock();
        std::function<void()> onPatchSectionChanged;

    private:
        struct ParamStrip : public juce::Component
        {
            juce::Label name;
            juce::Slider value;
            juce::String parameterId;
            PatchCraftProject* project = nullptr;
            bool syncing = false;

            ParamStrip();
            void bind (PatchCraftProject& owner, const juce::String& id);
            void syncFromProject();
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            void resized() override;
            bool draggingToCanvas = false;
        };

        struct Section : public juce::Component
        {
            juce::String sectionId;
            juce::Label title;
            juce::TextButton addButton { "+" };
            juce::TextButton editButton { "Edit" };
            juce::OwnedArray<ParamStrip> controls;
            juce::StringArray parameterIds;
            PatchCraftProject* project = nullptr;

            Section (juce::String id, juce::String sectionTitle, std::initializer_list<const char*> defaults);
            void bind (PatchCraftProject& project);
            void setParameterIds (const juce::StringArray& ids);
            void syncFromProject();
            void paint (juce::Graphics&) override;
            void resized() override;
        };

        struct BuilderPanel : public juce::Component
        {
            juce::Label title;
            juce::Label subtitle;
            juce::TextButton addBlockButton { "+ Block" };
            juce::TextButton addMacroButton { "+ Macro" };
            juce::TextButton addModButton   { "+ Mod" };
            juce::TextButton addArpButton   { "+ Perf" };
            juce::TextButton addAutomationButton { "+ Automation" };
            juce::TextButton importSampleButton { "Import Sample" };
            juce::TextButton savePatchButton { "Save Patch" };
            juce::TextButton savePatchAsButton { "Save Patch As" };
            juce::TextButton saveSectionPresetButton { "Save Section" };
            juce::ComboBox expansionBox;
            juce::TextButton sendExpansionButton { "Add To Pack" };
            juce::TextButton packCreatorButton { "Pack Creator" };
            juce::TextButton openSectionEditorButton { "Open Editor" };
            juce::TextButton mixerButton { "Mixer" };
            juce::TextButton clearSectionButton { "Clear Sec" };
            juce::TextButton clearAllButton { "Clear All" };
            juce::TextButton sectionBankButton1 { "Bank 1" };
            juce::TextButton sectionBankButton2 { "Bank 2" };
            juce::TextButton sectionBankButton3 { "Bank 3" };
            juce::TextButton sectionBankButton4 { "Bank 4" };
            juce::TextButton nodeMapButton { "Node Map" };
            juce::StringArray cards;
            juce::StringArray cardDescriptions;
            juce::Array<int> cardItemIds;
            juce::Array<bool> cardEnabled;
            int selectedItemId = 0;
            juce::Array<int> selectedItemIds;
            bool showSectionBanks = false;
            int activeSectionBank = 0;
            juce::Array<int> sectionBankCounts;
            std::function<void (int, bool)> onCardSelected;
            std::function<void (int)> onSectionBankSelected;
            std::function<void()> onNodeMapRequested;
            std::function<void()> onSectionEditorRequested;
            std::function<void()> onMixerRequested;

            BuilderPanel();
            void setContent (const juce::String& sectionName,
                             const juce::String& description,
                             juce::StringArray cardLabels,
                             juce::StringArray descriptions,
                             juce::Array<int> itemIds,
                             juce::Array<bool> enabledFlags);
            void setSectionBankState (bool shouldShow, int activeBank, const juce::Array<int>& counts);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void resized() override;
        };

        struct FxWaveformView : public juce::Component
        {
            explicit FxWaveformView (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDoubleClick (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            DspPage& owner;

        private:
            enum class DragMode
            {
                none,
                playhead,
                loopStart,
                loopEnd,
                loopRegion
            };

            DragMode dragMode = DragMode::none;
            float dragAnchor01 = 0.0f;
            float dragStart01 = 0.0f;
            float dragEnd01 = 1.0f;

            juce::Rectangle<int> waveformBounds() const;
            float xToPosition01 (int x) const;
            void updateFromMouse (const juce::MouseEvent&);
        };

        struct SourceMatrixPanel : public juce::Component
        {
            explicit SourceMatrixPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDoubleClick (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            DspPage& owner;
            int dragBlockIndex = -1;
            int dragColumn = -1;
            bool draggingValue = false;

            juce::Rectangle<int> tableBounds() const;
            std::vector<int> allSectionBlockIndices() const;
            std::vector<int> sectionBlockIndices() const;
            int columnAt (int x) const;
            int blockAt (int y) const;
            void selectBlock (int blockIndex);
            void setBlockValueFromMouse (const juce::MouseEvent&);
        };

        struct SurgicalEqPanel : public juce::Component
        {
            explicit SurgicalEqPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            DspPage& owner;
            int dragBlockIndex = -1;
            bool marqueeSelecting = false;
            juce::Point<int> dragStartPoint;
            juce::Rectangle<int> marquee;
            std::map<int, std::pair<float, float>> dragStartValues;

            juce::Rectangle<int> graphBounds() const;
            std::vector<int> eqBlockIndices() const;
            int nodeAt (juce::Point<int>) const;
            bool isBlockSelected (int blockIndex) const;
            void selectBlock (int blockIndex, bool additive);
            void selectBlocksInMarquee();
            void beginGroupDrag (int blockIndex, juce::Point<int>);
            void updateGroupDrag (juce::Point<int>);
            void updateBlockFromPoint (int blockIndex, juce::Point<int>);
            void createBlockAt (juce::Point<int>);
        };

        struct WavetableEditorPanel : public juce::Component,
                                      public juce::FileDragAndDropTarget
        {
            explicit WavetableEditorPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            bool isInterestedInFileDrag (const juce::StringArray& files) override;
            void filesDropped (const juce::StringArray& files, int, int) override;

        private:
            DspPage& owner;
            int dragPoint = -1;

            juce::Rectangle<int> graphBounds() const;
            DspBlock* selectedWavetableBlock() const;
            int pointAt (juce::Point<int>) const;
            void writePointFromMouse (juce::Point<int>);
        };

        struct ModMatrixPanel : public juce::Component
        {
            explicit ModMatrixPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            struct Row
            {
                int kind = 0;
                int index = -1;
            };

            DspPage& owner;
            Row dragRow;
            int dragColumn = -1;
            bool draggingValue = false;

            juce::Rectangle<int> tableBounds() const;
            std::vector<Row> matrixRows() const;
            int columnAt (int x) const;
            Row rowAt (int y) const;
            void selectRow (Row row);
            void setRowValueFromMouse (const juce::MouseEvent&);
        };

        struct FormulaPanel : public juce::Component
        {
            explicit FormulaPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDoubleClick (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            struct HitZone
            {
                juce::Rectangle<int> bounds;
                int blockIndex = -1;
                int column = -1;
                bool opensNodeMap = false;
            };

            DspPage& owner;
            std::vector<HitZone> hitZones;
            HitZone activeHit;
            bool draggingValue = false;

            void selectBlock (int blockIndex);
            void setValueFromMouse (const juce::MouseEvent&);
        };

        struct MidiPlaygroundPanel : public juce::Component
        {
            explicit MidiPlaygroundPanel (DspPage& owner);
            void paint (juce::Graphics&) override;
            void resized() override;

        private:
            DspPage& owner;
            juce::TextButton addButton { "Add Playground" };
            juce::TextButton chordButton { "Chord Phrase" };
            juce::TextButton sampleButton { "Sample Slice Control" };
            juce::TextButton randomButton { "Seed Variation" };

            DspBlock* ensureMidiPlaygroundBlock();
            DspBlock* selectedMidiPlaygroundBlock();
            void configureChordPhrase();
            void configureSampleSliceControl();
            void randomiseSeed();
            juce::String selectedBlockSummary() const;
        };

        struct TutorialOverlay;
        struct SectionEditorPopout;
        struct SectionMixerPopout;

        PatchCraftProject& project;
        StudioMainComponent* studioOwner = nullptr;
        bool quickEdit = false;

        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::Label soundSourceLabel;
        juce::Label stageHelpLabel;
        juce::TextButton samplerEngineButton { "Sampler" };
        juce::TextButton synthEngineButton   { "Synth" };
        juce::TextButton fxEngineButton      { "FX" };
        juce::TextButton easyModeButton      { "Easy" };
        juce::TextButton advancedModeButton  { "Advanced" };
        juce::TextButton aiModeButton        { "AI Builder" };
        bool easyMode = false;
        bool aiMode = false;

        juce::TextButton tabEngine { "SOURCE" };
        juce::TextButton tabTone   { "FILTER" };
        juce::TextButton tabAmp    { "AMP" };
        juce::TextButton tabMod    { "MOD" };
        juce::TextButton tabFx     { "FX" };
        juce::TextButton tabOut    { "OUT" };
        int currentTab = 0;

        Section engineSection { "source", "Source",       { "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend", "volume" } };
        Section toneSection   { "filter", "Filter",       { "filterCutoff", "filterResonance" } };
        Section ampSection    { "amp",    "Envelope",     { "attack", "decay", "sustain", "release", "volume" } };
        Section modSection    { "mod",    "Modulation",   { "lfoRate", "lfoAmount", "vibratoRate", "vibratoDepth" } };
        Section fxSection     { "fx",     "Effects",      { "drive", "mix", "delayTime", "delayFeedback", "delayMix", "reverbMix" } };
        Section outputSection { "out",    "Output",       { "volume", "pan" } };
        juce::Label easyTitleLabel;
        juce::Label easyHelpLabel;
        juce::ComboBox easyThemeBox;
        juce::TextButton easyGenerateButton { "Create Preset" };
        juce::TextButton easyRandomButton { "Generate Random" };
        juce::ToggleButton easyAddToPackToggle { "Add to Pack" };
        juce::ComboBox easyExpansionBox;
        juce::TextButton easyPackCreatorButton { "Pack Creator" };
        juce::TextButton easyAdvancedButton { "Open Advanced" };
        juce::Label easyRecipeLabel;
        juce::Label easyParametersLabel;
        juce::Label easyWorkflowLabel;
        BuilderPanel builderPanel;
        FormulaPanel formulaPanel { *this };
        SourceMatrixPanel sourceMatrix { *this };
        SurgicalEqPanel surgicalEqPanel { *this };
        WavetableEditorPanel wavetableEditor { *this };
        ModMatrixPanel modMatrix { *this };
        std::unique_ptr<TutorialOverlay> guidedTutorial;
        int sectionBanks[6] { 0, 0, 0, 0, 0, 0 };
        FxWaveformView fxWaveform { *this };
        juce::TextButton fxTrackImportButton { "Import Sample" };
        juce::TextButton fxTrackUseMapperButton { "Use Mapper" };
        juce::TextButton fxTrackPlayButton { "Play" };
        juce::TextButton fxTrackStopButton { "Stop" };
        
        std::unique_ptr<PromptToPluginComponent> promptToPlugin;
        
        juce::ToggleButton fxTrackLoopToggle { "Loop" };
        juce::ToggleButton fxTrackRetriggerToggle { "Retrigger" };
        juce::ToggleButton fxTrackLiveInputToggle { "Live Input" };
        juce::ComboBox fxTrackSliceBox;
        juce::ComboBox fxTrackTriggerBox;
        juce::TextButton fxTrackPrevSliceButton { "< Slice" };
        juce::TextButton fxTrackNextSliceButton { "Slice >" };
        juce::Slider fxTrackGainSlider;
        juce::ComboBox fxTrackMonitorBox;
        juce::Slider fxTrackLoopStartSlider;
        juce::Slider fxTrackLoopEndSlider;
        juce::Label fxTrackNameLabel;
        juce::Label fxTrackStatusLabel;

        juce::Label editorTitle;
        juce::Label editorHint;
        juce::ComboBox editorItemBox;
        juce::ComboBox typeBox;
        juce::ComboBox sourceBox;
        juce::ComboBox targetBox;
        juce::ComboBox globalPresetBox;
        juce::ComboBox sectionPresetBox;
        juce::TextButton deleteGraphItemButton { "Delete" };
        juce::ToggleButton enableGraphItemButton { "Enabled" };
        juce::ToggleButton eqAnalyzerToggle { "Analyzer" };
        juce::ToggleButton eqAnalyzerFreezeToggle { "Freeze" };
        juce::TextButton eqBandCopyButton { "Copy Band" };
        juce::TextButton eqBandPasteButton { "Paste Band" };
        juce::TextButton eqBandSaveButton { "Save Band" };
        juce::TextButton eqBandInsertButton { "Insert Saved" };
        juce::TextButton wtImportButton { "Import WT" };
        juce::TextButton wtNormalizeButton { "Normalize" };
        juce::TextButton wtSineButton { "Sine" };
        juce::Label amountLabel;
        juce::Label rateLabel;
        juce::Label valueLabel;
        juce::Label minLabel;
        juce::Label maxLabel;
        juce::Label curveLabel;
        juce::Slider amountSlider;
        juce::Slider rateSlider;
        juce::Slider valueSlider;
        juce::Slider minSlider;
        juce::Slider maxSlider;
        juce::Slider curveSlider;
        juce::ToggleButton amountSwitch { "OFF" };
        juce::ToggleButton rateSwitch { "OFF" };
        juce::ToggleButton valueSwitch { "OFF" };
        juce::ToggleButton minSwitch { "OFF" };
        juce::ToggleButton maxSwitch { "OFF" };
        juce::ToggleButton curveSwitch { "OFF" };
        juce::Label hoverHelpLabel;
        juce::String hoverHelpText;
        bool syncingEditor = false;
        int selectedGraphKind = 0;
        int selectedGraphIndex = -1;
        std::array<bool, 6> graphControlIsSwitch {};
        juce::AudioFormatManager fxFormatManager;
        juce::AudioBuffer<float> fxSampleBuffer;
        std::vector<float> fxWaveformPeaks;
        juce::File fxSampleFile;
        double fxSampleRate = 44100.0;
        double fxPreviewSampleRate = 44100.0;
        int fxPreviewBlockSize = 512;
        int fxPreviewChannels = 2;
        std::atomic<int> fxSampleLength { 0 };
        std::atomic<int> fxPlayhead { 0 };
        std::atomic<bool> fxPlaying { false };
        std::atomic<bool> fxLooping { true };
        std::atomic<bool> fxRetriggerOnPlay { true };
        std::atomic<bool> fxUseLiveInput { false };
        std::atomic<int> fxSliceCount { 1 };
        std::atomic<int> fxSelectedSlice { 0 };
        std::atomic<int> fxTriggerMode { 0 }; // 0 loop, 1 one-shot, 2 sequence, 3 random
        std::atomic<float> fxPreviewGain { 1.0f };
        std::atomic<float> fxLoopStart01 { 0.0f };
        std::atomic<float> fxLoopEnd01 { 1.0f };
        std::atomic<int> fxMonitorMode { 0 }; // 0 wet, 1 dry, 2 split dry/wet
        bool fxCallbackRegistered = false;
        juce::SpinLock fxAudioLock;
        std::unique_ptr<EffectEngine> fxPreviewEngine;
        DspRoutingEngine fxRoutingEngine;
        juce::AudioBuffer<float> fxCallbackBuffer;
        juce::AudioBuffer<float> fxDryCallbackBuffer;
        static constexpr int kEqAnalyzerOrder = 10;
        static constexpr int kEqAnalyzerFftSize = 1 << kEqAnalyzerOrder;
        static constexpr int kEqAnalyzerDisplayBins = 96;
        juce::dsp::FFT eqAnalyzerFft { kEqAnalyzerOrder };
        std::array<float, kEqAnalyzerFftSize> eqAnalyzerFifo {};
        std::array<float, kEqAnalyzerFftSize> eqAnalyzerSnapshot {};
        std::array<float, kEqAnalyzerFftSize * 2> eqAnalyzerFftBuffer {};
        std::array<float, kEqAnalyzerDisplayBins> eqAnalyzerBins {};
        std::array<float, kEqAnalyzerDisplayBins> eqAnalyzerDisplay {};
        std::atomic<int> eqAnalyzerFifoIndex { 0 };
        std::atomic<bool> eqAnalyzerSnapshotReady { false };
        juce::SpinLock eqAnalyzerLock;
        std::map<juce::String, float> eqBandClipboard;

        void setTab (int index);
        void setWorkflowMode (bool easy, bool ai = false);
        void applyEasyPreset();
        void applyEasyPresetForTheme (const juce::String& theme, bool forceRandomSeed);
        void applyEasyRandomVariation (Preset& preset, juce::uint32 seed);
        void clampEasyPresetSafety (Preset& preset);
        void syncEasyPresetValuesToGraphBlocks (const Preset& preset);
        void randomizeEasyGraphBlocks (juce::uint32 seed);
        DspBlock& ensureEasyMidiPlaygroundBlock();
        void configureEasyMidiForTheme (const juce::String& theme, juce::uint32 seed, bool forceVariation);
        void refreshEasyModeSummary();
        juce::String easyBlockCountSummary() const;
        juce::String easyParameterSummary() const;
        void refreshExpansionChoices();
        void showPackCreator();
        void addCurrentPatchToSelectedExpansion();
        void selectExpansionById (const juce::String& expansionId);
        ExpansionMetadata* selectedExpansionForMode();
        ExpansionMetadata& ensureSelectedExpansion();
        juce::String categoryForPresetTheme (const juce::String& theme) const;
        void addPatchPresetToExpansion (InstrumentPatch&, Preset&, ExpansionMetadata&, const juce::String& category);
        void upsertPlayablePreset (Preset preset, bool addToExpansion);
        void setEngine (const juce::String& engineId);
        void syncEngineButtons();
        void rebuildVisibility();
        void refreshBuilderPanel();
        void addBuilderBlock();
        void addBuilderMacro();
        void addBuilderModRoute();
        void addBuilderAutomation();
        void showAutomationEditorPopout (int automationIndex);
        void importFxSampleForTesting();
        bool loadFxSampleFile (const juce::File&);
        bool loadFxSampleZone (const SampleZoneDef&);
        void startFxSamplePlayback();
        void stopFxSamplePlayback();
        float getFxPlaybackPosition01() const;
        void retriggerFxSamplePlayback (bool updateStatus);
        void setFxLoopRegion (float start01, float end01, bool clampPlayhead);
        void setFxSliceCount (int slices);
        void selectFxSlice (int sliceIndex);
        bool selectedBlockIsSurgicalEq() const;
        void refreshEqActionButtons();
        void copySelectedEqBand();
        void pasteSelectedEqBand();
        juce::File eqBandLibraryFile() const;
        bool loadSavedEqBand (std::map<juce::String, float>& values) const;
        void saveSelectedEqBandToLibrary();
        void insertSavedEqBand();
        void pushEqAnalyzerSamples (const juce::AudioBuffer<float>&, int numSamples);
        void updateEqAnalyzerBins();
        DspBlock* selectedWavetableBlock();
        const DspBlock* selectedWavetableBlock() const;
        void ensureWavetableShapeDefaults (DspBlock&);
        void importWavetableShape();
        bool importWavetableShapeFromFile (const juce::File&);
        void normalizeSelectedWavetableShape();
        void setSelectedWavetableShapeToSine();
        void deleteSelectedGraphItem();
        void clearCurrentBuilderSection();
        void clearAllBuilderSections();
        void markGraphEdited();
        void bindSections();
        juce::String currentSectionId() const;
        juce::String currentStageId() const;
        juce::String currentStageLabel() const;
        juce::String currentSoundSourceLabel() const;
        juce::String currentStageHelpText() const;
        int currentSectionBank() const;
        void setCurrentSectionBank (int bank);
        juce::String sectionForTarget (const juce::String& targetId) const;
        bool targetAppliesToSection (const juce::String& targetId, const juce::String& sectionId) const;
        juce::String blockImpactDescription (const DspBlock&) const;
        juce::String fxFamilyForBlock (const DspBlock&) const;
        juce::String fxRoleForBlock (const DspBlock&) const;
        juce::String fxIoModeForBlock (const DspBlock&) const;
        Section* currentSection();
        void ensureQuickEditControls();
        void refreshQuickEditSections();
        void showQuickParamMenu (Section& section, bool editMode);
        void rebuildGraphEditorItems();
        void selectGraphEditorItem (int itemId);
        void syncGraphEditor();
        void applyGraphEditorChange();
        void toggleSelectedGraphItemEnabled();
        void applyBlockTypeDefaults (DspBlock&, const juce::String& previousType);
        void resetGraphControlModes();
        void setGraphControlSwitchMode (int index, bool active, bool enabled, const juce::String& tooltip);
        void syncGraphControlLabelTooltips();
        void refreshFxPreviewRouting();
        juce::String hoverHelpForComponent (juce::Component*);
        juce::String hoverHelpAt (juce::Point<int>, juce::Component*);
        void showHoverHelp (juce::String, juce::Point<int>);
        void hideHoverHelp();
        void fillTargetBox();
        void fillSourceBox();
        void configurePresetBoxes();
        void applyGlobalPreset (int presetId);
        void applySectionPreset (int presetId);
        void showBlockEditorPopout (int blockIndex);
        void selectNodeMapRoute (int kind, int index);
        void showNodeMapPopout();
        void showSectionEditorPopout();
        void showSectionMixerPopout();

        void projectChanged() override;
        void projectChanged (PatchCraftProject::ChangeScope scope) override;
        void liveValueChanged (const juce::String&, float) override;
        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DspPage)
    };
}
