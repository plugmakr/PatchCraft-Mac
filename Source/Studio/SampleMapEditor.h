#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "SampleMap.h"

namespace patchcraft
{
    class StudioMainComponent;
    class SampleSynthEngine;
    class SampleWaveformViewer;

    /**
        Kontakt-style Sample Map Editor.
        
        Layout (top to bottom):
        - Main toolbar (Edit, Auto, Zone Solo, tools)
        - Info bar (Key Range, Vel Range, Root, Sample name)
        - Zone grid with velocity layers
        - Bottom panel with Source/Amp/FX sections
        - Compact keyboard
    */
    class SampleMapEditor : public juce::Component,
                            public juce::AudioIODeviceCallback,
                            public juce::MidiInputCallback,
                            public juce::ListBoxModel,
                            public juce::FileDragAndDropTarget,
                            public juce::DragAndDropTarget,
                            private juce::Timer
    {
    public:
        SampleMapEditor (StudioMainComponent& owner);
        ~SampleMapEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();
        int getSelectedZoneIndex() const                   { return selectedZone; }
        const SampleZoneDef* getSelectedZone() const;
        void selectZone (int index);

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;

        // AudioIODeviceCallback for selected-zone audition.
        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    private:
        StudioMainComponent& owner;
        int selectedZone = -1;

        // Main toolbar
        juce::TextButton editModeBtn{ "Edit" };
        juce::TextButton autoBtn{ "Auto" };
        juce::TextButton zoneSoloBtn{ "Zone Solo" };
        juce::TextButton selectByMidiBtn{ "Select Zone by MIDI" };
        juce::TextButton gridViewBtn{ "Grid" };
        juce::TextButton listViewBtn{ "List View" };
        juce::TextButton importBtn{ "Import" };
        juce::TextButton libraryDrawerBtn{ "Library" };
        juce::TextButton recordVoiceBtn{ "Count In Rec" };
        juce::TextButton recordNowBtn{ "Record Now" };
        juce::TextButton stopVoiceRecordBtn{ "Stop Rec" };
        juce::TextButton cancelVoiceRecordBtn{ "Cancel" };
        juce::TextButton previewRecordingBtn{ "Preview Take" };
        juce::TextButton placeRecordingBtn{ "Place On Key" };
        juce::TextButton deleteRecordingBtn{ "Delete Take" };
        juce::ComboBox recordingTakesBox;
        juce::TextButton removeBtn{ "Delete Sel" };
        juce::TextButton selectAllBtn{ "Select All" };
        juce::TextButton clearAllBtn{ "Clear All" };
        juce::TextButton autoMapBtn{ "Auto Map" };
        juce::ToggleButton spanOnDropToggle{ "Span on Drop" };
        juce::ToggleButton stackPadsToggle{ "Stack Samples" };
        juce::ComboBox   mapPresetBox;
        juce::TextButton zoomInBtn{ "+" };
        juce::TextButton zoomOutBtn{ "-" };
        juce::TextButton auditionBtn{ "Audition" };
        juce::TextButton stopAuditionBtn{ "Stop" };
        juce::TextButton playModeBtn{ "Play Mode" };
        juce::ToggleButton loopToggle{ "Loop" };
        juce::TextButton fadeInBtn{ "+ Fade In" };
        juce::TextButton fadeOutBtn{ "+ Fade Out" };
        juce::TextButton zoomFitBtn{ "Fit" };
        juce::TextButton assignMidiBtn{ "Assign MIDI" };
        juce::TextButton clearMidiBtn{ "Clear MIDI" };
        juce::ComboBox midiModeBox;
        juce::ToggleButton midiSyncToggle{ "Host Sync" };

        // Smart zone tools: one-click professional sample recipes for the
        // currently selected zones. These keep Easy mode simple while giving
        // Advanced mode a fast path for common sound-design edits.
        juce::TextButton smartTrimBtn{ "Auto Trim" };
        juce::TextButton smartDrumBtn{ "Drum One-Shot" };
        juce::TextButton smartLoopBtn{ "Loop Pad" };
        juce::TextButton smartVelLayerBtn{ "Velocity Layers" };
        juce::TextButton smartRRBtn{ "Round Robin" };
        juce::TextButton smartHumanizeBtn{ "Humanize" };
        juce::TextButton smartResetBtn{ "Reset Edits" };

        // Easy / Advanced workflow
        bool easyMode = true;
        juce::Rectangle<int> easyGuideBounds;
        juce::TextButton easyModeBtn{ "Easy" };
        juce::TextButton advancedModeBtn{ "Advanced" };
        juce::Label easyTitleLabel{ "EasySampleTitle", "Easy Sample Builder" };
        juce::Label easyHelpLabel{ "EasySampleHelp",
                                   "Sampler is the sound source for sample-based instruments. Import samples, choose Keyboard / Drum / Slice mapping, Play Mapper, then Test in the Player. Advanced edits zones, velocity, round robin, fades, loops, and pad metadata." };
        juce::Label easySummaryLabel{ "EasySampleSummary", "" };
        juce::TextButton easyImportBtn{ "1  Import Samples" };
        juce::ComboBox   easyMapTypeBox;
        juce::TextButton easyKeyboardMapBtn{ "2  Build Map" };
        juce::TextButton easyDrumKitBtn{ "2B Drum Kit" };
        juce::TextButton easyGlitchKitBtn{ "2C Slice Kit" };
        juce::TextButton easyRemixKitBtn{ "2D Remix Kit" };
        juce::TextButton easyPlayModeBtn{ "3  Play Mapper" };
        juce::TextButton easyAuditionBtn{ "Audition Selected" };
        juce::TextButton easyStopBtn{ "Stop" };
        juce::TextButton easyTestBtn{ "4  Test Player" };
        juce::TextButton easyClearBtn{ "Clear" };
        
        // Info bar labels
        juce::Label keyRangeLabel{ "KeyRange", "Key Range: -" };
        juce::Label velRangeLabel{ "VelRange", "Vel Range: -" };
        juce::Label rootKeyLabel{ "RootKey", "Root Key: -" };
        juce::Label sampleNameLabel{ "Sample", "Sample: (none selected)" };
        juce::TextButton healthFixEngineBtn { "Fix Engine" };
        juce::TextButton healthAutoMapBtn { "Auto Map" };
        juce::TextButton healthFindMissingBtn { "Find Missing" };
        juce::TextButton healthGoTestBtn { "Go Test" };

        // Zone grid area (custom painted)
        juce::Rectangle<int> gridBounds;
        juce::Rectangle<int> drumPadBounds;
        juce::Rectangle<int> healthBounds;
        juce::Rectangle<int> recorderBounds;
        float zoomLevel = 1.0f;
        int scrollOffset = 0; // in semitones from C-2

        // Parameter controls (stepper style)
        struct StepperControl : public juce::Component
        {
            juce::Label nameLabel;
            juce::TextButton minusBtn{ "-" };
            juce::TextButton plusBtn{ "+" };
            juce::Label valueLabel;
            std::function<void(int)> onChange;
            int value = 0;
            int minValue = 0;
            int maxValue = 127;

            StepperControl (const juce::String& name);
            void resized() override;
            void setValue (int v, bool notify = true);
            void setTooltip (const juce::String& tooltip);
        };

        // Bottom panel controls - Source section
        juce::ComboBox sourceMode{ "Source" };
        juce::TextButton reverseBtn{ "Reverse" };
        juce::ToggleButton oneShotToggle{ "One Shot" };
        StepperControl tuneStepper{ "Tune" };
        StepperControl trackStepper{ "Tracking" };
        StepperControl midiTransposeStepper{ "MIDI Trans" };
        StepperControl midiVelocityStepper{ "MIDI Vel" };
        StepperControl padIndexStepper{ "Pad" };
        StepperControl chokeGroupStepper{ "Choke" };
        StepperControl triggerChanceStepper{ "Chance" };
        
        // Bottom panel controls - Amplifier section  
        StepperControl volumeStepper{ "Volume" };
        StepperControl panStepper{ "Pan" };
        
        // Zone editing steppers (for selected zone)
        StepperControl rrGroupStepper{ "RR Group" };
        StepperControl rrIndexStepper{ "RR Index" };
        StepperControl rootStepper{ "Root" };
        StepperControl loKeyStepper{ "Low Key" };
        StepperControl hiKeyStepper{ "High Key" };
        StepperControl loVelStepper{ "Low Vel" };
        StepperControl hiVelStepper{ "High Vel" };
        StepperControl importLowVelocityStepper{ "Import Low" };
        StepperControl importHighVelocityStepper{ "Import High" };
        StepperControl recordingKeyStepper{ "Place Key" };
        StepperControl recordCountInStepper{ "Count In" };
        juce::TextButton applyImportVelocitySelectedBtn{ "Apply Sel" };
        juce::TextButton applyImportVelocityAllBtn{ "Apply All" };
        StepperControl gainStepper{ "Gain dB" };
        StepperControl loopStartStepper{ "Loop Start" };
        StepperControl loopEndStepper{ "Loop End" };
        StepperControl sampleStartStepper{ "Start" };
        StepperControl sampleEndStepper{ "End" };

        std::unique_ptr<SampleWaveformViewer> waveformViewer;
        juce::Label waveformStatus{ "WaveformStatus", "Select a zone to edit waveform, loop, fades, and bounds." };
        juce::AudioBuffer<float> selectedWaveformBuffer;
        double selectedWaveformRate = 44100.0;
        juce::String loadedWaveformPath;

        // Sample list (List View mode)
        juce::ListBox samplesList{ "Samples", this };
        juce::Label samplesHeader{ "Samples", "Imported Samples" };
        
        // View mode: grid vs list
        bool listViewMode = false;
        bool zoneSoloEnabled = false;
        juce::Array<int> selectedZoneIndexes;

        // Zone colors
        static juce::Colour getZoneColour (int index);

        // Helpers
        juce::String noteToString (int midiNote);
        int stringToNote (const juce::String& s);
        int noteAtX (int x);
        int xAtNote (int note);
        int velocityAtY (int y, juce::Rectangle<int> zoneArea) const;
        juce::Rectangle<int> getZoneArea (juce::Rectangle<int> grid) const;
        juce::Rectangle<float> zoneRectFor (const SampleZoneDef& zone,
                                            juce::Rectangle<int> zoneArea,
                                            int startNote,
                                            float noteWidth) const;
        void updateSteppersFromZone();
        void updateZoneFromSteppers();
        bool isZoneSelected (int index) const;
        void setSelectedZones (juce::Array<int> indexes, bool syncList);
        void selectAllZones();
        void clearZoneSelection();
        void refreshWaveformFromSelectedZone();
        void applyWaveformZoneToSelected();
        juce::File resolveSampleFile (const SampleZoneDef&) const;
        bool loadWaveformForZone (const SampleZoneDef&, juce::String& status);
        std::vector<int> detectTransientSlicePoints (const SampleZoneDef& zone, int maxSlices);
        void chopSelectedZoneAtSlicePoints (const std::vector<int>& slicePoints, const juce::String& label);

        using HealthStatus = SampleMapHealthStatus;

        HealthStatus computeHealthStatus() const;
        void updateHealthButtons();
        void fixHealthEngine();
        void autoMapFromHealth();
        void findMissingSamplesFromHealth();
        void goToTestFromHealth();

        // Mouse handling for zone editing
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        // Zone manipulation
        enum class DragMode { none, moveZone, resizeLeft, resizeRight, resizeVelocityLow, resizeVelocityHigh, moveVelocity, movePad };
        DragMode dragMode = DragMode::none;
        int dragStartX = 0;
        int dragStartY = 0;
        int dragStartPad = -1;
        int dragStartNote = 0;
        int dragStartVelocity = 0;
        int dragStartLoKey = 0;
        int dragStartHiKey = 0;
        int dragStartRoot = 0;
        int dragStartLoVel = 1;
        int dragStartHiVel = 127;
        std::vector<SampleZoneDef> dragZonesBefore;
        bool dragChangedSampleMap = false;
        
        // MIDI learn for zone selection
        bool midiLearnMode = false;
        bool midiCallbackActive = false;

        // Paint methods
        juce::Rectangle<int> editPanelBounds;
        void paintZoneGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintKeyboard (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintDrumPadGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintHealthPanel (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintRecorderPanel (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintEditPanel (juce::Graphics& g, juce::Rectangle<int> bounds);
        void paintEditCard (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title);
        void paintEasyGuide (juce::Graphics& g, juce::Rectangle<int> bounds);
        void setEasyMode (bool shouldUseEasyMode);
        void updateModeVisibility();
        void timerCallback() override;
        juce::String buildEasySummary();
        bool ensureAuditionEngineForMap (juce::String& error);
        void setPlayModeEnabled (bool enabled);
        void triggerPreviewNoteOn (int midiNote, float velocity, bool selectMappedZone);
        void triggerPreviewNoteOff (int midiNote);
        void stopPreviewNotesOnly();
        void updateMidiCallbackRegistration();
        int noteAtKeyboardPosition (juce::Point<int> position) const;
        int padIndexAtPosition (juce::Point<int> position) const;
        int zoneIndexForPad (int padIndex) const;

        // Actions
        void addSample();
        void startVoiceRecording();
        void startVoiceRecordingWithCountIn();
        void beginVoiceRecordingNow();
        void stopVoiceRecordingAndImport();
        void cancelVoiceRecording();
        void previewSelectedRecording();
        void placeSelectedRecordingOnKey();
        void deleteSelectedRecordingTake();
        void refreshRecordingTakes();
        void refreshRecordingControls();
        juce::File selectedRecordingTakeFile() const;
        int findZoneForRecordingFile (const juce::File& file) const;
        void importSampleFiles (const juce::Array<juce::File>& files, bool spanMappedRoots = true);
        void importDroppedSampleFiles (const juce::Array<juce::File>& files,
                                       juce::Point<int> localPosition);
        void assignMidiToSelectedZone();
        void clearMidiFromSelectedZones();
        void assignMidiFilesToZone (const juce::Array<juce::File>& files,
                                    juce::Point<int> localPosition);
        bool assignMidiFileToZone (int zoneIndex, const juce::File& file,
                                   const juce::String& actionName);
        int zoneIndexAtPosition (juce::Point<int> position) const;
        void commitSampleMapEdit (const juce::String& actionName,
                                  std::vector<SampleZoneDef> beforeZones);

        // FileDragAndDropTarget — accept WAV / AIFF / FLAC drops, including
        // folders (recurses one level deep collecting audio files).
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;
        bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
        void itemDropped (const SourceDetails& dragSourceDetails) override;
        void removeSample();
        void clearAllSamples();
        void autoMap();
        void autoMapDrumPads();
        void autoMapDrumPadsAt (int startNote, int padCount);
        void makeSelectedZoneGlitchKit();
        void normalizeSelectedZones();
        void applyMapPreset (int presetId);
        void showEditMenu();
        juce::Array<int> editableZoneIndexes() const;
        void applySmartTrimToSelected();
        void applyDrumOneShotRecipeToSelected();
        void applySustainLoopRecipeToSelected();
        void applyVelocityLayersToSelected();
        void applyRoundRobinToSelected();
        void applyHumanizeToSelected();
        void selectAllRoundRobinAndHumanize();
        void fabricateRoundRobinVariations (int variationCount = 4);
        void resetPlaybackEditsForSelectedZones();
        void toggleReverseForSelected();
        void auditionSelectedZone();
        void stopAudition();
        void toggleLoopForSelected();
        void addDefaultFade (bool fadeIn);
        void duplicateSelectedZone();
        void splitSelectedZoneAtRoot();
        void splitSelectedZoneVelocity();
        void chopSelectedZoneIntoSlices (int sliceCount);
        void chopSelectedZoneAtTransients (int maxSlices);
        void mergeSelectedZoneWithNext();
        void showPrecisionSampleEditor();
        void copySelectedZoneToClipboard();
        void cutSelectedZoneToClipboard();
        void pasteZoneClipboard();
        void resetSelectedZonePlaybackEdits();
        void setSelectedZoneBoundsToFullSample();
        void selectZoneForMidi (int note, int velocity);
        void setMidiZoneSelectEnabled (bool enabled);
        void applyImportDefaultVelocity (bool allZones);

        std::unique_ptr<SampleSynthEngine> auditionEngine;
        juce::SpinLock auditionLock;
        double auditionSampleRate = 44100.0;
        int auditionBlockSize = 512;
        int auditionChannels = 2;
        bool auditionCallbackActive = false;
        int auditionNote = -1;
        enum class RecordingState { idle, countIn, recording, recorded };
        RecordingState recordingState = RecordingState::idle;
        bool voiceRecordingActive = false;
        bool voiceRecordCallbackOwned = false;
        juce::CriticalSection voiceRecordLock;
        juce::AudioBuffer<float> voiceRecordBuffer;
        int voiceRecordSamples = 0;
        double voiceRecordSampleRate = 44100.0;
        int voiceRecordMaxSamples = 0;
        float voiceRecordInputLevel = 0.0f;
        juce::uint32 voiceRecordStartMs = 0;
        juce::uint32 voiceRecordCountInStartMs = 0;
        juce::StringArray recordingTakePaths;
        juce::File lastRecordingFile;
        bool playModeEnabled = false;
        int mousePreviewNote = -1;
        SampleZoneDef zoneClipboard;
        bool hasZoneClipboard = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleMapEditor)
    };

} // namespace patchcraft
