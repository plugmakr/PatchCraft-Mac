#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <memory>

#include "MidiPlaygroundRuntime.h"
#include "PatchCraftProject.h"

namespace patchcraft
{
    class StudioMainComponent;
    class SampleSynthEngine;

    class MidiPlaygroundPage : public juce::Component,
                               public juce::AudioIODeviceCallback,
                               public juce::MidiInputCallback,
                               private juce::Timer
    {
    public:
        explicit MidiPlaygroundPage (StudioMainComponent& owner);
        ~MidiPlaygroundPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void refresh();
        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
        void showPlaygroundMode();
        void showArpStudioMode();

    private:
        StudioMainComponent& owner;

        juce::Label title;
        juce::Label subtitle;
        juce::Label activeSummary;
        juce::TextButton performanceViewButton { "Steps" };
        juce::TextButton arpLaneViewButton { "Circles" };

        juce::ComboBox sourceBox;
        juce::ComboBox modeBox;
        juce::ComboBox editorViewBox;
        juce::ComboBox musicalPresetBox;
        juce::ComboBox chordPresetBox;
        juce::ComboBox midiTemplateBox;
        juce::ComboBox guiTemplateBox;
        juce::ComboBox phraseBankBox;
        juce::ComboBox drumPatternBox;
        juce::ComboBox progressionBox;
        juce::ComboBox rootBox;
        juce::ComboBox scaleBox;
        juce::ComboBox targetBox;
        juce::ComboBox laneTargetBox;
        juce::ComboBox laneSoundBox;

        juce::ToggleButton multiLaneToggle { "Play All Slots" };
        juce::ToggleButton retriggerToggle { "Retrigger" };

        juce::Slider stepsSlider;
        juce::Slider rateSlider;
        juce::Slider gateSlider;
        juce::Slider swingSlider;
        juce::Slider probabilitySlider;
        juce::Slider humanizeSlider;
        juce::Slider sliceCountSlider;
        juce::Slider chordSizeSlider;
        juce::Slider chordSpreadSlider;
        juce::Slider octaveSlider;
        juce::Slider mutationSlider;
        juce::Slider ratchetSlider;
        juce::Slider velocityCurveSlider;
        juce::Slider strumSlider;
        juce::Slider flamSlider;
        juce::Slider euclideanPulsesSlider;
        juce::Slider euclideanRotateSlider;
        juce::ToggleButton octaveFoldToggle { "Fold Octaves" };

        juce::TextButton addPlaygroundButton { "Add Pattern Player" };
        juce::TextButton chordPhraseButton { "Chords" };
        juce::TextButton sampleSliceButton { "Sample Chops" };
        juce::TextButton drumMachineButton { "Drums" };
        juce::TextButton operatorsButton { "Musical Tools" };
        juce::TextButton generateAiMidiButton { "AI Generator" };
        juce::TextButton phraseLibraryButton { "Phrase Library" };
        juce::TextButton randomButton { "Create Variation" };
        juce::TextButton applyMusicalPresetButton { "Use This Pattern" };
        juce::TextButton storeBankButton { "Save Slot" };
        juce::TextButton duplicateBankButton { "Duplicate Slot" };
        juce::TextButton applyProgressionButton { "Apply Prog" };
        juce::TextButton applyMidiTemplateButton { "Make Pattern" };
        juce::TextButton applyGuiTemplateButton { "Add Player Controls" };
        juce::TextButton exportMidiButton { "Export MIDI" };
        juce::TextButton playPatternButton { "Play Pattern" };
        juce::TextButton stopPatternButton { "Stop" };
        juce::TextButton sourceBuilderButton { "Build Sound" };
        juce::TextButton sampleMapperButton { "Sample Mapper" };
        juce::TextButton testButton { "Test in Player" };
        std::unique_ptr<juce::FileChooser> exportChooser;

        juce::String activeBlockId;
        int selectedSectionCard = 0;
        bool syncingControls = false;
        bool pendingGraphNotification = false;
        bool arpStudioMode = false;
        bool arpStudioMidiDragArmed = false;
        bool patternPreviewActive = false;
        bool arpStudioHardwareMidiActive = false;
        int arpStudioDragLane = -1;
        int arpStudioEditingLane = -1;
        int arpStudioEditingStep = -1;
        int arpStudioEditingBand = 1;
        bool arpStudioEditingVelocity = false;
        juce::Point<int> arpStudioMidiDragStart;
        double arpStudioPreviewStartMs = 0.0;
        double previewSampleRate = 44100.0;
        int previewBlockSize = 512;
        int previewChannels = 2;
        int previewHeldNote = 60;
        std::unique_ptr<SampleSynthEngine> previewEngine;
        MidiPlaygroundRuntime previewRuntime;
        juce::SpinLock previewLock;

        struct MidiOutputLane : public juce::Component
        {
            explicit MidiOutputLane (MidiPlaygroundPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;

        private:
            MidiPlaygroundPage& owner;
            int dragStep = -1;

            enum class EditLane { note, velocity, gate, probability };
            EditLane editLane = EditLane::note;

            int stepAt (int x) const;
            EditLane laneAt (int y) const;
            void editFromMouse (const juce::MouseEvent&);
        };

        MidiOutputLane midiOutputLane { *this };

        struct PianoRollEditor : public juce::Component
        {
            explicit PianoRollEditor (MidiPlaygroundPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseMove (const juce::MouseEvent&) override;
            void mouseExit (const juce::MouseEvent&) override;

        private:
            MidiPlaygroundPage& owner;
            int dragStep = -1;
            bool dragActive = true;
            int hoverStep = -1;
            int hoverNoteOffset = 0;

            // Constants used by paint + hit-testing — kept in one place so a
            // change to vertical range stays consistent. 73 rows = 6 octaves
            // (-36..+36 semitones) centred on the root.
            static constexpr int kRollRows = 73;
            static constexpr int kRollKeyWidth = 88;
            static constexpr int kRollHeaderHeight = 48;

            juce::Rectangle<int> rollArea() const;
            int stepAt (int x) const;
            int noteOffsetAt (int y) const;
            int rowHeight (juce::Rectangle<int> area) const;
            float gateAt (int x, int step) const;
            void editFromMouse (const juce::MouseEvent&);
            int keyPreviewNoteAt (juce::Point<int> pos) const;
        };

        PianoRollEditor pianoRollEditor { *this };

        struct DrumPatternGrid : public juce::Component
        {
            explicit DrumPatternGrid (MidiPlaygroundPage& owner);
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            MidiPlaygroundPage& owner;
            int dragTrack = -1;
            int dragStep = -1;
            bool dragState = false;

            juce::Rectangle<int> gridArea() const;
            int trackAt (int y) const;
            int stepAt (int x) const;
            void editFromMouse (const juce::MouseEvent&);
        };

        DrumPatternGrid drumPatternGrid { *this };

        DspBlock* activeMidiBlock();
        const DspBlock* activeMidiBlock() const;
        DspBlock* ensureMidiBlock();
        DspBlock& createMidiBlock();

        void configureChordPhrase();
        void configureArpSequencer();
        void configureSampleSliceControl();
        void configureModulationLane();
        void configureDrumMachine();
        void randomiseSeed();
        void showOperatorsMenu();
        void showPhraseLibraryMenu();
        void applyOperator (int operatorId);
        void applyPhraseFromLibrary (int phraseId);
        void applyDrumOperator (int operatorId);
        void applyDrumTemplate (int templateId);
        void applySelectedMusicalPreset();
        void switchPhraseBank (int bank);
        void generateAiMidi();
        void storeActivePhraseBank();
        void duplicateActivePhraseBank();
        void applySelectedProgression();
        void applySelectedChordPreset();
        void applySelectedMidiTemplate();
        void applySelectedGuiTemplate();
        void exportMidiClip();
        void startPatternPreview();
        void stopPatternPreview();
        void setArpStudioHardwarePreviewActive (bool active);
        void triggerArpStudioPreviewNote (int note, float velocity, bool noteOn);
        void syncControlsFromBlock();
        void updateBlockFromControls();
        void notifyGraphChanged (bool immediate);
        void timerCallback() override;
        void setStepValueFromEditor (int step, int noteOffset, float velocity, float gate,
                                     float probability, bool active, bool editNote,
                                     bool editVelocity, bool editGate, bool editProbability);
        void setDrumStepFromEditor (int track, int step, bool active, float velocity,
                                    bool editVelocity, int divisions = -1);
        bool isDrumMachineBlock (const DspBlock&) const;

        bool hasDrumPatternClipboard = false;
        int drumPatternClipboardTracks = 0;
        int drumPatternClipboardSteps = 0;
        std::map<juce::String, float> drumPatternClipboard;

        juce::String blockSummary() const;
        juce::String performanceRouteSummary() const;
        juce::Rectangle<int> drawControl (juce::Graphics&, juce::Rectangle<int>,
                                          const juce::String&, juce::Component&);
        void drawSectionCards (juce::Graphics&, juce::Rectangle<int>);
        void drawArpStudio (juce::Graphics&);
        void drawArpStudioLane (juce::Graphics&, juce::Rectangle<int>, int lane, const DspBlock*) const;
        void drawArpStudioKnob (juce::Graphics&, juce::Rectangle<int>, const juce::String& label,
                                const juce::String& value, juce::Colour accent, float normalised) const;
        void drawArpStudioFxGroup (juce::Graphics&, juce::Rectangle<int>, int group, const juce::String& name,
                                   const juce::String& mode, juce::Colour accent) const;
        juce::Rectangle<int> arpStudioLaneBounds (juce::Rectangle<int> area, int lane) const;
        juce::Rectangle<int> arpStudioMidiDragBounds (juce::Rectangle<int> area, int lane) const;
        juce::Rectangle<int> arpStudioStepGridArea (juce::Rectangle<int> localBounds) const;
        bool arpStudioStepHitTest (juce::Point<int> pos, int& lane, int& step, int& band, float& value) const;
        void editArpStudioStepFromMouse (const juce::MouseEvent&, bool toggleStep);
        bool startArpStudioMidiDrag (int lane);
        juce::Rectangle<int> sectionCardBounds (juce::Rectangle<int> area, int index) const;
        juce::String sectionCardName (int index) const;
        juce::String sectionCardDescription (int index) const;
        juce::String sourceHelpText() const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPlaygroundPage)
    };
}
