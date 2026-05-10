#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>

#include "PatchCraftProject.h"

namespace patchcraft
{
    class StudioMainComponent;

    class MidiPlaygroundPage : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit MidiPlaygroundPage (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();

    private:
        StudioMainComponent& owner;

        juce::Label title;
        juce::Label subtitle;
        juce::Label activeSummary;

        juce::ComboBox sourceBox;
        juce::ComboBox modeBox;
        juce::ComboBox editorViewBox;
        juce::ComboBox chordPresetBox;
        juce::ComboBox midiTemplateBox;
        juce::ComboBox guiTemplateBox;
        juce::ComboBox phraseBankBox;
        juce::ComboBox drumPatternBox;
        juce::ComboBox progressionBox;
        juce::ComboBox rootBox;
        juce::ComboBox scaleBox;
        juce::ComboBox targetBox;

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

        juce::TextButton addPlaygroundButton { "Add Playground" };
        juce::TextButton chordPhraseButton { "Chord Phrase" };
        juce::TextButton sampleSliceButton { "Sample Slice Control" };
        juce::TextButton drumMachineButton { "Drum Machine" };
        juce::TextButton operatorsButton { "Operators" };
        juce::TextButton phraseLibraryButton { "Phrases" };
        juce::TextButton randomButton { "Seed Variation" };
        juce::TextButton storeBankButton { "Store Bank" };
        juce::TextButton duplicateBankButton { "Duplicate Bank" };
        juce::TextButton applyProgressionButton { "Apply Prog" };
        juce::TextButton applyMidiTemplateButton { "Apply MIDI" };
        juce::TextButton applyGuiTemplateButton { "Apply GUI" };
        juce::TextButton exportMidiButton { "Export MIDI" };
        juce::TextButton sourceBuilderButton { "Source Builder" };
        juce::TextButton sampleMapperButton { "Sample Mapper" };
        juce::TextButton testButton { "Go To Test" };
        std::unique_ptr<juce::FileChooser> exportChooser;

        juce::String activeBlockId;
        int selectedSectionCard = 0;
        bool syncingControls = false;
        bool pendingGraphNotification = false;

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
        void configureSampleSliceControl();
        void configureDrumMachine();
        void randomiseSeed();
        void showOperatorsMenu();
        void showPhraseLibraryMenu();
        void applyOperator (int operatorId);
        void applyPhraseFromLibrary (int phraseId);
        void switchPhraseBank (int bank);
        void storeActivePhraseBank();
        void duplicateActivePhraseBank();
        void applySelectedProgression();
        void applySelectedChordPreset();
        void applySelectedMidiTemplate();
        void applySelectedGuiTemplate();
        void exportMidiClip();
        void syncControlsFromBlock();
        void updateBlockFromControls();
        void notifyGraphChanged (bool immediate);
        void timerCallback() override;
        void setStepValueFromEditor (int step, int noteOffset, float velocity, float gate,
                                     float probability, bool active, bool editNote,
                                     bool editVelocity, bool editGate, bool editProbability);
        void setDrumStepFromEditor (int track, int step, bool active, float velocity, bool editVelocity);
        bool isDrumMachineBlock (const DspBlock&) const;

        juce::String blockSummary() const;
        juce::Rectangle<int> drawControl (juce::Graphics&, juce::Rectangle<int>,
                                          const juce::String&, juce::Component&);
        void drawSectionCards (juce::Graphics&, juce::Rectangle<int>);
        juce::Rectangle<int> sectionCardBounds (juce::Rectangle<int> area, int index) const;
        juce::String sectionCardName (int index) const;
        juce::String sectionCardDescription (int index) const;
        juce::String sourceHelpText() const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPlaygroundPage)
    };
}
