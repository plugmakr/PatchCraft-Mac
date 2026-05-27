#include "MidiPlaygroundPage.h"

#include "BottomPanel.h"
#include "MidiPlaygroundPattern.h"
#include "PatchCraftLookAndFeel.h"
#include "SampleSynthEngine.h"
#include "StudioMainComponent.h"
#include "AiAssistService.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        static bool isMidiPlaygroundBlock (const DspBlock& block)
        {
            return block.section == "mod"
                && (block.type.containsIgnoreCase ("midi")
                    || block.type.containsIgnoreCase ("arp")
                    || block.type.containsIgnoreCase ("sequencer")
                    || block.type.containsIgnoreCase ("drum"));
        }

        static float valueFor (const DspBlock& block, const juce::String& key, float fallback)
        {
            if (auto it = block.values.find (key); it != block.values.end())
                return it->second;
            return fallback;
        }

        static juce::String uniqueMidiBlockId (const DspGraph& graph)
        {
            for (int index = 1; index < 10000; ++index)
            {
                const auto candidate = "midi_playground_" + juce::String (index);
                const auto used = std::any_of (graph.blocks.begin(), graph.blocks.end(),
                                               [&] (const DspBlock& block) { return block.id == candidate; });
                if (! used)
                    return candidate;
            }
            return "midi_playground_" + juce::String (juce::Time::getMillisecondCounter());
        }

        static void styleButton (juce::TextButton& button)
        {
            button.getProperties().set ("smallButton", true);
            button.getProperties().set ("fontSize", 11.5);
        }

        static void styleSlider (juce::Slider& slider, double min, double max, double interval)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 20);
            slider.setRange (min, max, interval);
        }

        static constexpr int kAdvancedMidiMaxSteps = MidiPlaygroundPattern::kStepCount;

        static int midiStepCount (const DspBlock& block, float fallback = 16.0f)
        {
            return juce::jlimit (1, kAdvancedMidiMaxSteps,
                                 juce::roundToInt (valueFor (block, "arpSteps", fallback)));
        }

        static float arpBankValue (const DspBlock& block, int bank, const juce::String& key, float fallback)
        {
            const auto bankPrefix = "mpBank" + juce::String (juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, bank) + 1) + "_";
            if (bank > 0)
            {
                const auto bankIt = block.values.find (bankPrefix + key);
                if (bankIt != block.values.end())
                    return bankIt->second;
            }

            if (const auto directIt = block.values.find (key); directIt != block.values.end())
                return directIt->second;

            const auto bankOneIt = block.values.find (bankPrefix + key);
            return bankOneIt != block.values.end() ? bankOneIt->second : fallback;
        }

        static juce::String arpStudioMidiFileName (juce::String laneName, int lane)
        {
            if (laneName.trim().isEmpty())
                laneName = "ArpLane" + juce::String (lane + 1);
            return juce::File::createLegalFileName ("PatchCraft_" + laneName.trim() + ".mid")
                .replaceCharacter (' ', '_');
        }

        static int roundStepNote (float value)
        {
            return juce::jlimit (-36, 36, juce::roundToInt (value));
        }

        static juce::String drumPrefix (const DspBlock& block, int track, int step)
        {
            const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (block, "dmPattern", 0.0f)));
            return "dmP" + juce::String (pattern) + "T" + juce::String (track) + "S" + juce::String (step);
        }

        static juce::String drumPrefixForPattern (int pattern, int track, int step)
        {
            pattern = juce::jlimit (0, 7, pattern);
            return "dmP" + juce::String (pattern) + "T" + juce::String (track) + "S" + juce::String (step);
        }

        static juce::String defaultTrackLabel (int track)
        {
            static const juce::StringArray labels {
                "Kick", "Snare", "Closed Hat", "Open Hat", "Low Tom", "Mid Tom", "Crash", "Ride",
                "Rim", "Clap", "Pedal Hat", "High Tom", "Shaker", "Perc", "FX 1", "FX 2"
            };
            return labels[juce::jlimit (0, labels.size() - 1, track)];
        }

        static int defaultTrackNote (int track)
        {
            static constexpr std::array<int, 16> notes {{
                36, 38, 42, 46, 41, 45, 49, 51, 37, 39, 44, 48, 50, 47, 52, 53
            }};
            return notes[(size_t) juce::jlimit (0, 15, track)];
        }

        static juce::String midiNoteName (int midiNote)
        {
            static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            midiNote = juce::jlimit (0, 127, midiNote);
            return names[midiNote % 12] + juce::String (midiNote / 12 - 1);
        }

        static juce::String chordPresetNameForId (int presetId)
        {
            static const juce::StringArray names {
                "Chord: Major", "Chord: Minor", "Chord: Diminished", "Chord: Augmented",
                "Chord: Sus2", "Chord: Sus4", "Chord: Major 6", "Chord: Minor 6",
                "Chord: Dominant 7", "Chord: Major 7", "Chord: Minor 7", "Chord: Half-Dim 7",
                "Chord: Diminished 7", "Chord: Minor Major 7", "Chord: Add9", "Chord: Minor Add9",
                "Chord: Dominant 9", "Chord: Minor 9", "Chord: 11", "Chord: 13",
                "Chord: 7sus4", "Chord: Quartal", "Chord: Power",
                "Progression: I-V-vi-IV Pop", "Progression: ii-V-I Jazz", "Progression: I-vi-IV-V 50s",
                "Progression: vi-IV-I-V Modern", "Progression: i-VII-VI-VII Dark",
                "Progression: i-VI-III-VII Epic", "Progression: I-IV-V Blues",
                "Progression: Canon I-V-vi-iii-IV-I-IV-V"
            };
            return names[juce::jlimit (0, names.size() - 1, presetId - 1)];
        }

        static int chordModeForPresetId (int presetId)
        {
            static constexpr std::array<int, 23> modes {{
                7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29
            }};
            if (presetId >= 1 && presetId <= (int) modes.size())
                return modes[(size_t) presetId - 1];
            return presetId == 25 ? 2 : 1;
        }

        static std::vector<int> chordIntervalsForMode (int mode, int chordSize)
        {
            std::vector<int> intervals;
            auto add = [&] (int semitone) { intervals.push_back (semitone); };
            if (mode <= 0) add (0);
            else if (mode == 1) { add (0); add (4); add (7); }
            else if (mode == 2) { add (0); add (4); add (7); add (11); }
            else if (mode == 3 || mode == 29) { add (0); add (7); add (12); }
            else if (mode == 4) { add (0); add (5); add (7); add (12); }
            else if (mode == 5) { add (0); add (2); add (5); add (9); }
            else if (mode == 6) { add (0); add (7); add (12); add (17); }
            else if (mode == 7) { add (0); add (4); add (7); }
            else if (mode == 8) { add (0); add (3); add (7); }
            else if (mode == 9) { add (0); add (3); add (6); }
            else if (mode == 10) { add (0); add (4); add (8); }
            else if (mode == 11) { add (0); add (2); add (7); }
            else if (mode == 12) { add (0); add (5); add (7); }
            else if (mode == 13) { add (0); add (4); add (7); add (9); }
            else if (mode == 14) { add (0); add (3); add (7); add (9); }
            else if (mode == 15) { add (0); add (4); add (7); add (10); }
            else if (mode == 16) { add (0); add (4); add (7); add (11); }
            else if (mode == 17) { add (0); add (3); add (7); add (10); }
            else if (mode == 18) { add (0); add (3); add (6); add (10); }
            else if (mode == 19) { add (0); add (3); add (6); add (9); }
            else if (mode == 20) { add (0); add (3); add (7); add (11); }
            else if (mode == 21) { add (0); add (4); add (7); add (14); }
            else if (mode == 22) { add (0); add (3); add (7); add (14); }
            else if (mode == 23) { add (0); add (4); add (7); add (10); add (14); }
            else if (mode == 24) { add (0); add (3); add (7); add (10); add (14); }
            else if (mode == 25) { add (0); add (4); add (7); add (10); add (14); add (17); }
            else if (mode == 26) { add (0); add (4); add (7); add (10); add (14); add (21); }
            else if (mode == 27) { add (0); add (5); add (7); add (10); }
            else if (mode == 28) { add (0); add (5); add (10); add (15); }
            else { add (0); add (7); add (12); }

            const int wanted = juce::jlimit (1, 8, juce::jmax (1, chordSize));
            if ((int) intervals.size() > wanted)
                intervals.resize ((size_t) wanted);
            return intervals;
        }

        static void setDrumCell (DspBlock& block, int pattern, int track, int step,
                                 bool active, float velocity, float gate = 0.36f,
                                 float probability = 1.0f, int divisions = 1)
        {
            const auto prefix = drumPrefixForPattern (pattern, track, step);
            block.values[prefix + "On"] = active ? 1.0f : 0.0f;
            block.values[prefix + "Vel"] = juce::jlimit (0.0f, 1.0f, velocity);
            block.values[prefix + "Gate"] = juce::jlimit (0.05f, 1.0f, gate);
            block.values[prefix + "Prob"] = juce::jlimit (0.0f, 1.0f, probability);
            block.values[prefix + "Div"] = (float) juce::jlimit (1, 4, divisions);
        }

        static int drumCellDivisions (const DspBlock& block, int pattern, int track, int step)
        {
            const auto prefix = drumPrefixForPattern (pattern, track, step);
            return juce::jlimit (1, 4, juce::roundToInt (valueFor (block, prefix + "Div", 1.0f)));
        }

        static void clearDrumPattern (DspBlock& block, int pattern, int tracks, int steps)
        {
            pattern = juce::jlimit (0, 7, pattern);
            tracks = juce::jlimit (1, 16, tracks);
            steps = juce::jlimit (1, 64, steps);
            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                    setDrumCell (block, pattern, track, step, false, track == 0 ? 1.0f : 0.75f,
                                 track == 2 ? 0.16f : 0.36f, 1.0f, 1);
        }

        static void copyDrumPattern (DspBlock& block, int sourcePattern, int destinationPattern,
                                     int tracks, int steps)
        {
            sourcePattern = juce::jlimit (0, 7, sourcePattern);
            destinationPattern = juce::jlimit (0, 7, destinationPattern);
            tracks = juce::jlimit (1, 16, tracks);
            steps = juce::jlimit (1, 64, steps);

            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                {
                    const auto src = drumPrefixForPattern (sourcePattern, track, step);
                    setDrumCell (block, destinationPattern, track, step,
                                 valueFor (block, src + "On", 0.0f) >= 0.5f,
                                 valueFor (block, src + "Vel", track == 0 ? 1.0f : 0.75f),
                                 valueFor (block, src + "Gate", track == 2 ? 0.16f : 0.36f),
                                 valueFor (block, src + "Prob", 1.0f),
                                 juce::roundToInt (valueFor (block, src + "Div", 1.0f)));
                }
        }

        static void shiftDrumPattern (DspBlock& block, int pattern, int tracks, int steps, int offset)
        {
            pattern = juce::jlimit (0, 7, pattern);
            tracks = juce::jlimit (1, 16, tracks);
            steps = juce::jlimit (1, 64, steps);
            std::map<juce::String, float> snapshot;
            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                {
                    const auto src = drumPrefixForPattern (pattern, track, step);
                    const int destStep = (step + offset + steps) % steps;
                    const auto dest = drumPrefixForPattern (pattern, track, destStep);
                    for (auto* suffix : { "On", "Vel", "Gate", "Prob", "Div" })
                        snapshot[dest + suffix] = valueFor (block, src + suffix, juce::String (suffix) == "Div" ? 1.0f : 0.0f);
                }

            for (const auto& value : snapshot)
                block.values[value.first] = value.second;
        }
    }

        MidiPlaygroundPage::MidiOutputLane::MidiOutputLane (MidiPlaygroundPage& p) : owner (p)
    {
        setOpaque (false);
    }

    MidiPlaygroundPage::PianoRollEditor::PianoRollEditor (MidiPlaygroundPage& p) : owner (p)
    {
        setOpaque (false);
    }

    MidiPlaygroundPage::MidiPlaygroundPage (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);

        title.setText ("PATTERN PLAYER", juce::dontSendNotification);
        title.setFont (juce::Font (17.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (title);

        subtitle.setText ("Build the playable pattern layer for the current DSP sound: notes, chops, drums, gates, and modulation.",
                          juce::dontSendNotification);
        subtitle.setFont (juce::Font (12.0f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        activeSummary.setText ("Start here: 1 Build Sound in DSP  2 Choose or drag in a pattern  3 Use This Pattern  4 Add Player Controls  5 Test in Player.",
                               juce::dontSendNotification);
        activeSummary.setFont (juce::Font (12.0f, juce::Font::bold));
        activeSummary.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (activeSummary);

        sourceBox.addItem ("Current Synth Source", 1);
        sourceBox.addItem ("Current Sample Map", 2);
        sourceBox.addItem ("Current FX Input", 3);
        sourceBox.setSelectedId (owner.getProject().getEngineType() == "sample" ? 2
                                : owner.getProject().getEngineType() == "fx" ? 3 : 1,
                                juce::dontSendNotification);
        sourceBox.setTooltip ("Choose what this pattern plays or moves: DSP synth notes, Sample Mapper zones/slices, drum pads, FX input, or modulation targets.");
        addAndMakeVisible (sourceBox);

        modeBox.addItem ("Chord / Progression", 1);
        modeBox.addItem ("Sample Chopper", 2);
        modeBox.addItem ("Arp / Step Sequencer", 3);
        modeBox.addItem ("Modulation Lane", 4);
        modeBox.addItem ("Drum Machine", 5);
        modeBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (modeBox);

        editorViewBox.addItem ("Step Grid", 1);
        editorViewBox.addItem ("Piano Roll", 2);
        editorViewBox.setSelectedId (1, juce::dontSendNotification);
        editorViewBox.onChange = [this] { repaint(); midiOutputLane.repaint(); pianoRollEditor.repaint(); };
        addAndMakeVisible (editorViewBox);

        musicalPresetBox.setTextWhenNothingSelected ("Choose musical idea...");
        musicalPresetBox.addSectionHeading ("Chords");
        musicalPresetBox.addItem ("Major triad", 1001);
        musicalPresetBox.addItem ("Minor triad", 1002);
        musicalPresetBox.addItem ("Dominant 7", 1009);
        musicalPresetBox.addItem ("Major 7", 1010);
        musicalPresetBox.addItem ("Minor 9", 1018);
        musicalPresetBox.addSeparator();
        musicalPresetBox.addSectionHeading ("Progressions");
        musicalPresetBox.addItem ("Pop lift: I-V-vi-IV", 2021);
        musicalPresetBox.addItem ("Jazz color: ii-V-I", 2022);
        musicalPresetBox.addItem ("Dark cinematic: i-VII-VI-VII", 2025);
        musicalPresetBox.addItem ("Epic trailer: i-VI-III-VII", 2026);
        musicalPresetBox.addSeparator();
        musicalPresetBox.addSectionHeading ("Melodies + Bass");
        musicalPresetBox.addItem ("Trance arp climb", 3003);
        musicalPresetBox.addItem ("Acid bassline", 3005);
        musicalPresetBox.addItem ("Anjuna lead", 3008);
        musicalPresetBox.addItem ("Walking bass", 3015);
        musicalPresetBox.addItem ("Sparse melodic skeleton", 3020);
        musicalPresetBox.addSeparator();
        musicalPresetBox.addSectionHeading ("Drum Grooves");
        musicalPresetBox.addItem ("Modern trap rolls", 4301);
        musicalPresetBox.addItem ("Four-on-the-floor house", 4303);
        musicalPresetBox.addItem ("DnB roller", 4305);
        musicalPresetBox.addItem ("UK garage shuffle", 4309);
        musicalPresetBox.addSeparator();
        musicalPresetBox.addSectionHeading ("Sample Remix");
        musicalPresetBox.addItem ("Sample chopper performance", 5001);
        musicalPresetBox.addItem ("Glitch gate performance", 5002);
        musicalPresetBox.setTooltip ("Pick a chord, progression, melody, drum groove, or sample-remix idea, then press Use This Pattern to make it part of the instrument.");
        addAndMakeVisible (musicalPresetBox);

        for (int presetId = 1; presetId <= 31; ++presetId)
            chordPresetBox.addItem (chordPresetNameForId (presetId), presetId);
        chordPresetBox.setTextWhenNothingSelected ("Choose chord or progression");
        chordPresetBox.onChange = [this]
        {
            if (! syncingControls && chordPresetBox.getSelectedId() > 0)
                applySelectedChordPreset();
        };
        addAndMakeVisible (chordPresetBox);

        midiTemplateBox.addItem ("Melodic Lead Line", 1);
        midiTemplateBox.addItem ("Cinematic Chord Bed", 2);
        midiTemplateBox.addItem ("Drum Pattern", 3);
        midiTemplateBox.addItem ("Sample Chopper", 4);
        midiTemplateBox.addItem ("Glitch Gate", 5);
        midiTemplateBox.addItem ("Polymeter Key-Switch Banks", 6);
        midiTemplateBox.addItem ("MIDI Echo Throw", 7);
        midiTemplateBox.addItem ("Pattern Morph Performance", 8);
        midiTemplateBox.addItem ("Chord Pad Performer", 9);
        midiTemplateBox.addItem ("Riff Generator", 10);
        midiTemplateBox.addItem ("Advanced Arp Instrument", 11);
        midiTemplateBox.addItem ("CircleSEQ Flagship Instrument", 12);
        midiTemplateBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (midiTemplateBox);

        guiTemplateBox.addItem ("Performance Strip", 1);
        guiTemplateBox.addItem ("Sample Controls", 2);
        guiTemplateBox.addItem ("Drum Pad Panel", 3);
        guiTemplateBox.addItem ("MIDI Macro Panel", 4);
        guiTemplateBox.addItem ("XY Performance Pad", 5);
        guiTemplateBox.addItem ("Drum Song Controls", 6);
        guiTemplateBox.addItem ("CircleSEQ Flagship Player", 7);
        guiTemplateBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (guiTemplateBox);

        for (int bank = 0; bank < MidiPlaygroundPattern::kPhraseBankCount; ++bank)
            phraseBankBox.addItem ("Slot " + juce::String (bank + 1), bank + 1);
        phraseBankBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (phraseBankBox);

        for (int pattern = 0; pattern < 8; ++pattern)
            drumPatternBox.addItem ("Pattern " + juce::String (pattern + 1), pattern + 1);
        drumPatternBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (drumPatternBox);

        const auto progressionNames = MidiPlaygroundPattern::getProgressionNames();
        for (int i = 0; i < progressionNames.size(); ++i)
            progressionBox.addItem (progressionNames[i], i + 1);
        progressionBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (progressionBox);

        const juce::StringArray roots { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < roots.size(); ++i)
            rootBox.addItem (roots[i], i + 1);
        rootBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (rootBox);

        scaleBox.addItem ("Chromatic", 1);
        scaleBox.addItem ("Major", 2);
        scaleBox.addItem ("Minor", 3);
        scaleBox.addItem ("Dorian", 4);
        scaleBox.addItem ("Phrygian", 5);
        scaleBox.addItem ("Lydian", 6);
        scaleBox.addItem ("Mixolydian", 7);
        scaleBox.addItem ("Harmonic Minor", 8);
        scaleBox.addItem ("Major Pentatonic", 9);
        scaleBox.addItem ("Blues", 10);
        scaleBox.setSelectedId (2, juce::dontSendNotification);
        addAndMakeVisible (scaleBox);

        targetBox.addItem ("Filter Cutoff", 1);
        targetBox.addItem ("Volume", 2);
        targetBox.addItem ("Sample Slice", 3);
        targetBox.addItem ("Sample Start", 4);
        targetBox.addItem ("Sample Length", 5);
        targetBox.addItem ("Wavetable Position", 6);
        targetBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (targetBox);

        styleSlider (stepsSlider, 1.0, (double) kAdvancedMidiMaxSteps, 1.0);
        styleSlider (rateSlider, 0.0625, 16.0, 0.0625);
        styleSlider (gateSlider, 0.05, 1.0, 0.01);
        styleSlider (swingSlider, 0.0, 1.0, 0.01);
        styleSlider (probabilitySlider, 0.0, 1.0, 0.01);
        styleSlider (humanizeSlider, 0.0, 1.0, 0.01);
        styleSlider (sliceCountSlider, 1.0, 32.0, 1.0);
        styleSlider (chordSizeSlider, 1.0, 8.0, 1.0);
        styleSlider (chordSpreadSlider, 0.0, 1.0, 0.01);
        styleSlider (octaveSlider, 1.0, 4.0, 1.0);
        styleSlider (mutationSlider, 0.0, 1.0, 0.01);
        styleSlider (ratchetSlider, 1.0, 4.0, 1.0);
        styleSlider (velocityCurveSlider, -1.0, 1.0, 0.01);
        styleSlider (strumSlider, 0.0, 1.0, 0.01);
        styleSlider (flamSlider, 0.0, 1.0, 0.01);
        styleSlider (euclideanPulsesSlider, 0.0, (double) kAdvancedMidiMaxSteps, 1.0);
        styleSlider (euclideanRotateSlider, 0.0, (double) kAdvancedMidiMaxSteps - 1.0, 1.0);

        stepsSlider.setValue (8.0, juce::dontSendNotification);
        rateSlider.setValue (1.0, juce::dontSendNotification);
        gateSlider.setValue (0.55, juce::dontSendNotification);
        swingSlider.setValue (0.0, juce::dontSendNotification);
        probabilitySlider.setValue (1.0, juce::dontSendNotification);
        humanizeSlider.setValue (0.0, juce::dontSendNotification);
        sliceCountSlider.setValue (16.0, juce::dontSendNotification);
        chordSizeSlider.setValue (1.0, juce::dontSendNotification);
        chordSpreadSlider.setValue (0.0, juce::dontSendNotification);
        octaveSlider.setValue (2.0, juce::dontSendNotification);
        mutationSlider.setValue (0.0, juce::dontSendNotification);
        ratchetSlider.setValue (1.0, juce::dontSendNotification);
        velocityCurveSlider.setValue (0.0, juce::dontSendNotification);
        strumSlider.setValue (0.0, juce::dontSendNotification);
        flamSlider.setValue (0.0, juce::dontSendNotification);
        euclideanPulsesSlider.setValue (0.0, juce::dontSendNotification);
        euclideanRotateSlider.setValue (0.0, juce::dontSendNotification);

        for (auto* slider : { &stepsSlider, &rateSlider, &gateSlider, &swingSlider,
                              &probabilitySlider, &humanizeSlider, &sliceCountSlider,
                              &chordSizeSlider, &chordSpreadSlider, &octaveSlider,
                              &mutationSlider, &ratchetSlider, &velocityCurveSlider,
                              &strumSlider, &flamSlider, &euclideanPulsesSlider,
                              &euclideanRotateSlider })
        {
            addAndMakeVisible (*slider);
            slider->onValueChange = [this] { updateBlockFromControls(); };
            slider->onDragEnd = [this] { notifyGraphChanged (true); };
        }
        addAndMakeVisible (octaveFoldToggle);
        octaveFoldToggle.onClick = [this] { updateBlockFromControls(); notifyGraphChanged (true); };

        modeBox.onChange = [this]
        {
            if (syncingControls)
                return;

            switch (modeBox.getSelectedId())
            {
                case 1: configureChordPhrase(); break;
                case 2: configureSampleSliceControl(); break;
                case 3: configureArpSequencer(); break;
                case 4: configureModulationLane(); break;
                case 5: configureDrumMachine(); break;
                default: updateBlockFromControls(); break;
            }
        };

        phraseBankBox.onChange = [this]
        {
            if (! syncingControls)
                switchPhraseBank (phraseBankBox.getSelectedId() - 1);
        };

        drumPatternBox.onChange = [this]
        {
            if (syncingControls)
                return;

            auto* block = activeMidiBlock();
            if (block == nullptr || ! isDrumMachineBlock (*block))
            {
                configureDrumMachine();
                block = activeMidiBlock();
            }

            if (block != nullptr)
            {
                block->values["dmPattern"] = (float) juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
                notifyGraphChanged (true);
                syncControlsFromBlock();
                drumPatternGrid.repaint();
                repaint();
            }
        };

        for (auto* combo : { &rootBox, &scaleBox, &targetBox })
            combo->onChange = [this] { updateBlockFromControls(); notifyGraphChanged (true); };

        sourceBox.onChange = [this]
        {
            if (syncingControls)
                return;

            if (arpStudioMode)
            {
                if (auto* block = ensureMidiBlock())
                {
                    const int lane = juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1,
                                                   phraseBankBox.getSelectedId() > 0 ? phraseBankBox.getSelectedId() - 1
                                                                                     : juce::roundToInt (valueFor (*block, "mpActiveBank", 0.0f)));
                    block->metadata["arpLane" + juce::String (lane + 1) + "Source"] = sourceBox.getText();
                    notifyGraphChanged (true);
                    repaint();
                }
                return;
            }

            if (sourceBox.getSelectedId() == 2)
                owner.getProject().setEngineType ("sample");
            else if (sourceBox.getSelectedId() == 3)
                owner.getProject().setEngineType ("fx");
            else
                owner.getProject().setEngineType ("synth");

            owner.getProject().markDirty();
            owner.getProject().notifyChanged();
            repaint();
        };

        // The hidden variation action is deterministic. Authored phrase
        // libraries and musical operators are the primary creation path.
        performanceViewButton.setClickingTogglesState (true);
        arpLaneViewButton.setClickingTogglesState (true);
        performanceViewButton.setRadioGroupId (8822);
        arpLaneViewButton.setRadioGroupId (8822);
        performanceViewButton.setTooltip ("Build the instrument's playable pattern with piano roll, step grid, drum grid, sample chops, and modulation lanes.");
        arpLaneViewButton.setTooltip ("See and edit the same pattern slots as circles for Player-facing sequencer instruments.");
        performanceViewButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::MidiPlayground); };
        arpLaneViewButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::ArpStudio); };
        performanceViewButton.setToggleState (true, juce::dontSendNotification);

        for (auto* button : { &performanceViewButton, &arpLaneViewButton,
                              &addPlaygroundButton, &chordPhraseButton, &sampleSliceButton,
                              &drumMachineButton, &operatorsButton, &generateAiMidiButton, &phraseLibraryButton,
                              &applyMusicalPresetButton, &storeBankButton, &duplicateBankButton,
                              &applyProgressionButton, &applyMidiTemplateButton, &applyGuiTemplateButton, &exportMidiButton,
                              &playPatternButton, &stopPatternButton,
                              &sourceBuilderButton, &sampleMapperButton, &testButton })
        {
            styleButton (*button);
            addAndMakeVisible (*button);
        }

        addPlaygroundButton.onClick = [this] { ensureMidiBlock(); syncControlsFromBlock(); repaint(); };
        chordPhraseButton.onClick = [this] { configureChordPhrase(); };
        sampleSliceButton.onClick = [this] { configureSampleSliceControl(); };
        drumMachineButton.onClick = [this] { configureDrumMachine(); };
        randomButton.onClick = [this] { randomiseSeed(); };
        operatorsButton.onClick = [this] { showOperatorsMenu(); };
        operatorsButton.setTooltip ("Deterministic pattern operators: transpose, invert, retrograde, scale-quantize.");

        generateAiMidiButton.onClick = [this] { generateAiMidi(); };
        generateAiMidiButton.setTooltip ("Generate MIDI using AI (requires prompt).");
        phraseLibraryButton.onClick = [this] { showPhraseLibraryMenu(); };
        phraseLibraryButton.setTooltip ("Open categorized hand-authored phrases and grooves for the current editor.");
        applyMusicalPresetButton.onClick = [this] { applySelectedMusicalPreset(); };
        applyMusicalPresetButton.getProperties().set ("primaryAction", true);
        applyMusicalPresetButton.setTooltip ("Make the selected musical idea the active pattern used by Circle View, Player test, MIDI export, and the exported plugin.");
        storeBankButton.onClick = [this] { storeActivePhraseBank(); };
        duplicateBankButton.onClick = [this] { duplicateActivePhraseBank(); };
        applyProgressionButton.onClick = [this] { applySelectedProgression(); };
        applyMidiTemplateButton.onClick = [this] { applySelectedMidiTemplate(); };
        applyGuiTemplateButton.onClick = [this] { applySelectedGuiTemplate(); };
        exportMidiButton.onClick = [this] { exportMidiClip(); };
        playPatternButton.onClick = [this] { startPatternPreview(); };
        stopPatternButton.onClick = [this] { stopPatternPreview(); };
        playPatternButton.getProperties().set ("primaryAction", true);
        sourceBuilderButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::DSP); };
        sampleMapperButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Samples); };
        testButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Test); };

        addAndMakeVisible (midiOutputLane);
        addAndMakeVisible (pianoRollEditor);
        pianoRollEditor.setVisible (false);
        addAndMakeVisible (drumPatternGrid);
        drumPatternGrid.setVisible (false);
    }

    MidiPlaygroundPage::~MidiPlaygroundPage()
    {
        setArpStudioHardwarePreviewActive (false);
        stopPatternPreview();
    }

    void MidiPlaygroundPage::showPlaygroundMode()
    {
        setArpStudioHardwarePreviewActive (false);
        arpStudioMode = false;
        title.setText ("PATTERN PLAYER", juce::dontSendNotification);
        subtitle.setText ("Build the playable pattern layer for the current DSP sound: notes, chops, drums, gates, and modulation.",
                          juce::dontSendNotification);
        performanceViewButton.setToggleState (true, juce::dontSendNotification);
        arpLaneViewButton.setToggleState (false, juce::dontSendNotification);
        if (midiTemplateBox.getSelectedId() == 12)
            midiTemplateBox.setSelectedId (1, juce::dontSendNotification);
        if (guiTemplateBox.getSelectedId() == 7)
            guiTemplateBox.setSelectedId (1, juce::dontSendNotification);
        applyMidiTemplateButton.setButtonText ("Make Pattern");
        applyGuiTemplateButton.setButtonText ("Add Player Controls");
        activeSummary.setText (activeMidiBlock() != nullptr
            ? blockSummary()
            : "Start here: 1 Build Sound in DSP  2 Choose or drag in a pattern  3 Use This Pattern  4 Add Player Controls  5 Test in Player.",
            juce::dontSendNotification);
        repaint();
    }

    void MidiPlaygroundPage::showArpStudioMode()
    {
        arpStudioMode = true;
        setArpStudioHardwarePreviewActive (isShowing());
        title.setText ("CIRCLE PATTERN VIEW", juce::dontSendNotification);
        subtitle.setText ("Patterning-style circle editor for the same slots. Sound still comes from DSP, Sample Mapper, or the selected FX input.",
                          juce::dontSendNotification);
        performanceViewButton.setToggleState (false, juce::dontSendNotification);
        arpLaneViewButton.setToggleState (true, juce::dontSendNotification);
        sourceBox.setSelectedId (1, juce::dontSendNotification);
        modeBox.setSelectedId (3, juce::dontSendNotification);
        editorViewBox.setSelectedId (1, juce::dontSendNotification);
        midiTemplateBox.setSelectedId (12, juce::dontSendNotification);
        guiTemplateBox.setSelectedId (7, juce::dontSendNotification);
        phraseBankBox.setSelectedId (1, juce::dontSendNotification);
        applyMidiTemplateButton.setButtonText ("Make Circle Pattern");
        applyGuiTemplateButton.setButtonText ("Update Player Controls");
        activeSummary.setText ("Circle workflow: Build Sound in DSP, pick a Slot, edit the ring/steps, assign the lane target, then update the Player controls.",
                               juce::dontSendNotification);
        repaint();
    }

    void MidiPlaygroundPage::refresh()
    {
        syncControlsFromBlock();
        repaint();
    }

    void MidiPlaygroundPage::mouseDown (const juce::MouseEvent& e)
    {
        if (arpStudioMode)
        {
            auto content = getLocalBounds().reduced (12, 10);
            content.removeFromTop (104);

            int hitLane = -1;
            int hitStep = -1;
            int hitBand = -1;
            float hitValue = 0.0f;
            if (arpStudioStepHitTest (content, e.getPosition(), hitLane, hitStep, hitBand, hitValue))
            {
                arpStudioMidiDragArmed = false;
                arpStudioDragLane = -1;
                arpStudioEditingLane = hitLane;
                arpStudioEditingStep = hitStep;
                arpStudioEditingBand = hitBand;
                const bool toggleStep = e.mods.isPopupMenu() || e.mods.isCtrlDown();
                arpStudioEditingVelocity = ! toggleStep;
                editArpStudioStepFromMouse (e, toggleStep);
                return;
            }

            for (int lane = 0; lane < MidiPlaygroundPattern::kPhraseBankCount; ++lane)
            {
                if (arpStudioMidiDragBounds (content, lane).contains (e.getPosition()))
                {
                    arpStudioMidiDragArmed = true;
                    arpStudioDragLane = lane;
                    arpStudioMidiDragStart = e.getPosition();
                    arpStudioEditingVelocity = false;
                    arpStudioEditingLane = -1;
                    arpStudioEditingStep = -1;
                    return;
                }

                if (arpStudioLaneBounds (content, lane).contains (e.getPosition()))
                {
                    arpStudioMidiDragArmed = false;
                    arpStudioDragLane = -1;
                    arpStudioEditingVelocity = false;
                    arpStudioEditingLane = -1;
                    arpStudioEditingStep = -1;
                    phraseBankBox.setSelectedId (lane + 1, juce::dontSendNotification);
                    switchPhraseBank (lane);
                    repaint();
                    return;
                }
            }

            arpStudioMidiDragArmed = false;
            arpStudioDragLane = -1;
            arpStudioEditingLane = -1;
            arpStudioEditingStep = -1;
            arpStudioEditingVelocity = false;
            return;
        }

        auto r = getLocalBounds().reduced (18, 14);
        r.removeFromTop (88);
        r.removeFromTop (12);
        r.removeFromLeft (360);
        auto cards = r.removeFromTop (juce::jmax (224, r.getHeight() / 2));

        for (int i = 0; i < 6; ++i)
        {
            if (sectionCardBounds (cards, i).contains (e.getPosition()))
            {
                selectedSectionCard = i;
                activeSummary.setText (sectionCardName (i) + ": " + sectionCardDescription (i),
                                       juce::dontSendNotification);
                repaint();
                return;
            }
        }
    }

    void MidiPlaygroundPage::mouseDrag (const juce::MouseEvent& e)
    {
        if (arpStudioEditingVelocity)
        {
            editArpStudioStepFromMouse (e, false);
            return;
        }

        if (! arpStudioMidiDragArmed || arpStudioDragLane < 0)
            return;

        const auto delta = e.getPosition() - arpStudioMidiDragStart;
        if (std::abs (delta.x) + std::abs (delta.y) < 8)
            return;

        startArpStudioMidiDrag (arpStudioDragLane);
        arpStudioMidiDragArmed = false;
        arpStudioDragLane = -1;
    }

    DspBlock* MidiPlaygroundPage::activeMidiBlock()
    {
        auto& graph = owner.getProject().getDspGraph();

        if (activeBlockId.isNotEmpty())
            for (auto& block : graph.blocks)
                if (block.id == activeBlockId && isMidiPlaygroundBlock (block))
                    return &block;

        for (auto& block : graph.blocks)
            if (isMidiPlaygroundBlock (block))
            {
                activeBlockId = block.id;
                return &block;
            }

        return nullptr;
    }

    const DspBlock* MidiPlaygroundPage::activeMidiBlock() const
    {
        const auto& graph = owner.getProject().getDspGraph();

        if (activeBlockId.isNotEmpty())
            for (const auto& block : graph.blocks)
                if (block.id == activeBlockId && isMidiPlaygroundBlock (block))
                    return &block;

        for (const auto& block : graph.blocks)
            if (isMidiPlaygroundBlock (block))
                return &block;

        return nullptr;
    }

    DspBlock* MidiPlaygroundPage::ensureMidiBlock()
    {
        if (auto* block = activeMidiBlock())
            return block;

        return &createMidiBlock();
    }

    DspBlock& MidiPlaygroundPage::createMidiBlock()
    {
        auto& graph = owner.getProject().getDspGraph();

        DspBlock block;
        block.id = uniqueMidiBlockId (graph);
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "Performance Builder";
        block.targetId = "filterCutoff";
        block.enabled = true;
        block.metadata["bank"] = "0";
        block.values["amount"] = 0.35f;
        block.values["rate"] = 1.0f;
        block.values["sync"] = 1.0f;
        block.values["arpGate"] = 0.55f;
        block.values["arpSteps"] = 8.0f;
        block.values["arpPattern"] = 0.0f;
        block.values["arpOctaves"] = 2.0f;
        block.values["arpSwing"] = 0.0f;
        block.values["mpScaleRoot"] = 0.0f;
        block.values["mpScaleType"] = 1.0f;
        block.values["mpChordMode"] = 0.0f;
        block.values["mpChordSize"] = 1.0f;
        block.values["mpChordSpread"] = 0.0f;
        block.values["mpProbability"] = 1.0f;
        block.values["mpHumanize"] = 0.0f;
        block.values["mpMutation"] = 0.0f;
        block.values["mpRatchet"] = 1.0f;
        block.values["mpVelocityCurve"] = 0.0f;
        block.values["mpStrum"] = 0.0f;
        block.values["mpFlam"] = 0.0f;
        block.values["mpEuclideanPulses"] = 0.0f;
        block.values["mpEuclideanRotate"] = 0.0f;
        block.values["mpPolymeterSteps"] = 0.0f;
        block.values["mpEchoRepeats"] = 0.0f;
        block.values["mpEchoDelay"] = 0.18f;
        block.values["mpEchoDecay"] = 0.55f;
        block.values["mpPatternMorph"] = 0.0f;
        block.values["mpKeySwitchEnabled"] = 0.0f;
        block.values["mpKeySwitchBase"] = 24.0f;
        block.values["mpModLane"] = 0.0f;
        block.values["mpOctaveFold"] = 0.0f;
        block.values["mpLatch"] = 0.0f;
        block.values["mpSampleControl"] = 0.0f;
        block.values["mpSampleSliceCount"] = 1.0f;
        block.values["mpSampleStart"] = 0.0f;
        block.values["mpSampleLength"] = 1.0f;
        block.values["mpSamplePitch"] = 0.0f;
        block.values["mpSeed"] = 12001.0f;

        const float notes[] { 0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 10.0f, 14.0f,
                              12.0f, 7.0f, 4.0f, 0.0f, 5.0f, 9.0f, 12.0f, 16.0f };
        for (int step = 0; step < 16; ++step)
        {
            block.values["arpNote" + juce::String (step)] = notes[step];
            block.values["mpVelocity" + juce::String (step)] = 1.0f;
            block.values["mpGate" + juce::String (step)] = 0.55f;
            block.values["mpStepProb" + juce::String (step)] = 1.0f;
            block.values["mpStep" + juce::String (step) + "On"] = 1.0f;
            block.values["mpSampleSlice" + juce::String (step)] = -1.0f;
            block.values["mpStepDiv" + juce::String (step)] = 1.0f;
            block.values["mpStepTranspose" + juce::String (step)] = 0.0f;
            block.values["mpStepChordMode" + juce::String (step)] = -1.0f;
            block.values["mpStepChordSize" + juce::String (step)] = -1.0f;
            block.values["mpStepTie" + juce::String (step)] = 0.0f;
        }

        MidiPlaygroundPattern::storeActiveBank (block, 0);

        graph.blocks.push_back (std::move (block));
        activeBlockId = graph.blocks.back().id;
        notifyGraphChanged (true);
        return graph.blocks.back();
    }

    void MidiPlaygroundPage::configureChordPhrase()
    {
        if (auto* block = ensureMidiBlock())
        {
            block->name = "Chord Progression Performance";
            block->type = "midiPlayground";
            block->section = "mod";
            block->targetId = "filterCutoff";
            block->enabled = true;
            block->values["arpSteps"] = 8.0f;
            block->values["arpPattern"] = 2.0f;
            block->values["arpGate"] = 0.62f;
            block->values["arpSwing"] = 0.18f;
            block->values["mpScaleRoot"] = 0.0f;
            block->values["mpScaleType"] = 1.0f;
            block->values["mpChordMode"] = 1.0f;
            block->values["mpChordSize"] = 3.0f;
            block->values["mpChordSpread"] = 0.35f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.10f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpVelocityCurve"] = -0.10f;
            block->values["mpStrum"] = 0.18f;
            block->values["mpFlam"] = 0.0f;
            block->values["mpEuclideanPulses"] = 0.0f;
            block->values["mpEuclideanRotate"] = 0.0f;
            block->values["mpPolymeterSteps"] = 0.0f;
            block->values["mpEchoRepeats"] = 0.0f;
            block->values["mpEchoDelay"] = 0.18f;
            block->values["mpEchoDecay"] = 0.55f;
            block->values["mpPatternMorph"] = 0.0f;
            block->values["mpKeySwitchEnabled"] = 0.0f;
            block->values["mpKeySwitchBase"] = 24.0f;
            block->values["mpOctaveFold"] = 1.0f;
            block->values["mpSampleControl"] = 0.0f;
            block->values["mpModLane"] = 0.0f;
            modeBox.setSelectedId (1, juce::dontSendNotification);

            const float notes[] { 0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 7.0f, 4.0f, 2.0f };
            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = notes[step % 8];
                block->values["mpStep" + juce::String (step) + "On"] = step < 8 ? 1.0f : 0.0f;
                block->values["mpVelocity" + juce::String (step)] = step % 2 == 0 ? 0.95f : 0.68f;
                block->values["mpGate" + juce::String (step)] = 0.62f;
                block->values["mpStepProb" + juce::String (step)] = 1.0f;
                block->values["mpSampleSlice" + juce::String (step)] = -1.0f;
            }

            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
        }
    }

    void MidiPlaygroundPage::configureArpSequencer()
    {
        if (auto* block = ensureMidiBlock())
        {
            block->name = "Arp Step Sequencer";
            block->type = "midiPlayground";
            block->section = "mod";
            block->targetId = "filterCutoff";
            block->enabled = true;
            block->values["arpSteps"] = 16.0f;
            block->values["arpPattern"] = 7.0f;
            block->values["arpGate"] = 0.46f;
            block->values["arpSwing"] = 0.12f;
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);
            block->values["mpScaleType"] = (float) juce::jmax (0, scaleBox.getSelectedId() - 1);
            block->values["mpChordMode"] = 0.0f;
            block->values["mpChordSize"] = 1.0f;
            block->values["mpChordSpread"] = 0.0f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.04f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpVelocityCurve"] = 0.10f;
            block->values["mpStrum"] = 0.0f;
            block->values["mpFlam"] = 0.0f;
            block->values["mpEuclideanPulses"] = 0.0f;
            block->values["mpEuclideanRotate"] = 0.0f;
            block->values["mpModLane"] = 0.0f;
            block->values["mpSampleControl"] = 0.0f;
            block->values["mpSeed"] = 12002.0f;

            const float notes[] { 0.0f, 7.0f, 12.0f, 16.0f, 14.0f, 12.0f, 7.0f, 4.0f,
                                  0.0f, 4.0f, 7.0f, 11.0f, 12.0f, 16.0f, 19.0f, 24.0f };
            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = notes[step];
                block->values["mpStep" + juce::String (step) + "On"] = 1.0f;
                block->values["mpVelocity" + juce::String (step)] = step % 4 == 0 ? 0.96f : 0.68f;
                block->values["mpGate" + juce::String (step)] = step % 4 == 0 ? 0.58f : 0.42f;
                block->values["mpStepProb" + juce::String (step)] = 1.0f;
                block->values["mpSampleSlice" + juce::String (step)] = -1.0f;
            }

            modeBox.setSelectedId (3, juce::dontSendNotification);
            editorViewBox.setSelectedId (2, juce::dontSendNotification);
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
        }
    }

    void MidiPlaygroundPage::configureSampleSliceControl()
    {

        if (auto* block = ensureMidiBlock())
        {
            block->name = "Sample Chopper Performance";
            block->type = "midiPlayground";
            block->section = "mod";
            block->targetId = "sampleSlice";
            block->enabled = true;
            block->values["arpSteps"] = 16.0f;
            block->values["arpPattern"] = 0.0f;
            block->values["arpGate"] = 0.42f;
            block->values["arpSwing"] = 0.08f;
            block->values["mpScaleRoot"] = 0.0f;
            block->values["mpScaleType"] = 0.0f;
            block->values["mpChordMode"] = 0.0f;
            block->values["mpChordSize"] = 1.0f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.05f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpVelocityCurve"] = 0.0f;
            block->values["mpStrum"] = 0.0f;
            block->values["mpFlam"] = 0.10f;
            block->values["mpEuclideanPulses"] = 0.0f;
            block->values["mpEuclideanRotate"] = 0.0f;
            block->values["mpPolymeterSteps"] = 0.0f;
            block->values["mpEchoRepeats"] = 0.0f;
            block->values["mpEchoDelay"] = 0.18f;
            block->values["mpEchoDecay"] = 0.55f;
            block->values["mpPatternMorph"] = 0.0f;
            block->values["mpKeySwitchEnabled"] = 0.0f;
            block->values["mpKeySwitchBase"] = 24.0f;
            block->values["mpOctaveFold"] = 0.0f;
            block->values["mpSampleControl"] = 1.0f;
            block->values["mpModLane"] = 0.0f;
            block->values["mpSeed"] = 12003.0f;
            block->values["sampleStart"] = 0.0f;
            block->values["sampleLength"] = 0.18f;
            block->values["sampleSliceCount"] = 16.0f;
            block->values["samplePitch"] = 0.0f;
            block->values["mpSampleSliceCount"] = 16.0f;
            block->values["mpSampleStart"] = 0.0f;
            block->values["mpSampleLength"] = 0.18f;
            block->values["mpSamplePitch"] = 0.0f;

            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = 0.0f;
                block->values["mpSampleSlice" + juce::String (step)] = (float) step;
                block->values["mpStep" + juce::String (step) + "On"] = 1.0f;
                block->values["mpVelocity" + juce::String (step)] = step % 4 == 0 ? 1.0f : 0.72f;
                block->values["mpGate" + juce::String (step)] = 0.30f + (float) (step % 4) * 0.08f;
                block->values["mpStepProb" + juce::String (step)] = 1.0f;
            }

            modeBox.setSelectedId (2, juce::dontSendNotification);
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
        }
    }

    void MidiPlaygroundPage::configureModulationLane()
    {
        if (auto* block = ensureMidiBlock())
        {
            block->name = "Performance Modulation Lane";
            block->type = "midiPlayground";
            block->section = "mod";
            block->enabled = true;

            switch (targetBox.getSelectedId())
            {
                case 2: block->targetId = "volume"; break;
                case 3: block->targetId = "sampleSlice"; break;
                case 4: block->targetId = "sampleStart"; break;
                case 5: block->targetId = "sampleLength"; break;
                case 6: block->targetId = "wtPosition"; break;
                default: block->targetId = "filterCutoff"; break;
            }

            block->values["arpSteps"] = 16.0f;
            block->values["arpPattern"] = 0.0f;
            block->values["arpGate"] = 0.72f;
            block->values["arpSwing"] = 0.0f;
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);
            block->values["mpScaleType"] = (float) juce::jmax (0, scaleBox.getSelectedId() - 1);
            block->values["mpChordMode"] = 0.0f;
            block->values["mpChordSize"] = 1.0f;
            block->values["mpChordSpread"] = 0.0f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.0f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpVelocityCurve"] = 0.0f;
            block->values["mpStrum"] = 0.0f;
            block->values["mpFlam"] = 0.0f;
            block->values["mpEuclideanPulses"] = 0.0f;
            block->values["mpEuclideanRotate"] = 0.0f;
            block->values["mpSampleControl"] = 0.0f;
            block->values["mpModLane"] = 1.0f;
            block->values["mpSeed"] = 12004.0f;

            const float lane[] { 0.18f, 0.28f, 0.46f, 0.72f, 0.94f, 0.70f, 0.48f, 0.30f,
                                 0.22f, 0.42f, 0.78f, 1.0f, 0.82f, 0.58f, 0.36f, 0.24f };
            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = 0.0f;
                block->values["mpStep" + juce::String (step) + "On"] = 1.0f;
                block->values["mpVelocity" + juce::String (step)] = lane[step];
                block->values["mpGate" + juce::String (step)] = 0.72f;
                block->values["mpStepProb" + juce::String (step)] = 1.0f;
                block->values["mpSampleSlice" + juce::String (step)] = -1.0f;
            }

            modeBox.setSelectedId (4, juce::dontSendNotification);
            editorViewBox.setSelectedId (1, juce::dontSendNotification);
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
        }
    }

    void MidiPlaygroundPage::configureDrumMachine()
    {
        if (auto* block = ensureMidiBlock())
        {
            block->name = "Drum Machine Performance";
            block->type = "drumMachine";
            block->section = "mod";
            block->targetId = "sample";
            block->enabled = true;
            block->values["rate"] = 1.0f;
            block->values["sync"] = 1.0f;
            block->values["dmTracks"] = 8.0f;
            block->values["dmSteps"] = 16.0f;
            block->values["dmPattern"] = (float) juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
            block->values["dmTransport"] = 1.0f;
            block->values["dmSwing"] = 0.0f;
            block->values["dmProbability"] = 1.0f;
            block->values["dmSongMode"] = 0.0f;
            block->values["dmChainLength"] = 4.0f;
            for (int chain = 0; chain < 8; ++chain)
                block->values["dmChain" + juce::String (chain)] = (float) juce::jlimit (0, 7, chain);
            block->values["dmSeed"] = 16001.0f;
            block->values["mpSampleControl"] = 1.0f;
            if (owner.getProject().getEngineType() != "sample")
                owner.getProject().setEngineType ("sample");

            for (int track = 0; track < 16; ++track)
            {
                block->values["dmTrack" + juce::String (track) + "Note"] = (float) defaultTrackNote (track);
                block->metadata["dmTrack" + juce::String (track) + "Label"] = defaultTrackLabel (track);
            }

            for (int pattern = 0; pattern < 8; ++pattern)
                for (int track = 0; track < 16; ++track)
                    for (int step = 0; step < 64; ++step)
                    {
                        const auto prefix = drumPrefixForPattern (pattern, track, step);
                        block->values.erase (prefix + "On");
                        block->values.erase (prefix + "Vel");
                        block->values.erase (prefix + "Gate");
                        block->values.erase (prefix + "Prob");
                        block->values.erase (prefix + "Div");
                    }

            for (int step : { 0, 4, 8, 12 }) setDrumCell (*block, 0, 0, step, true, 1.0f, 0.42f);
            for (int step : { 4, 12 }) setDrumCell (*block, 0, 1, step, true, 0.88f, 0.34f);
            for (int step = 0; step < 16; step += 2) setDrumCell (*block, 0, 2, step, true, step % 4 == 0 ? 0.72f : 0.58f, 0.18f);
            for (int step : { 7, 15 }) setDrumCell (*block, 0, 3, step, true, 0.64f, 0.30f);

            for (int step : { 0, 3, 10, 14 }) setDrumCell (*block, 1, 0, step, true, step == 0 ? 1.0f : 0.82f, 0.38f);
            for (int step : { 8 }) setDrumCell (*block, 1, 1, step, true, 0.92f, 0.34f);
            for (int step = 0; step < 16; ++step) setDrumCell (*block, 1, 2, step, true, step % 2 == 0 ? 0.52f : 0.38f, 0.12f, step % 4 == 3 ? 0.70f : 1.0f, step % 4 == 3 ? 2 : 1);
            for (int step : { 11 }) setDrumCell (*block, 1, 3, step, true, 0.68f, 0.24f);

            for (int step : { 0, 6, 10 }) setDrumCell (*block, 2, 0, step, true, 0.94f, 0.40f);
            for (int step : { 4, 12 }) setDrumCell (*block, 2, 1, step, true, 0.90f, 0.34f);
            for (int step : { 2, 5, 7, 9, 11, 13, 15 }) setDrumCell (*block, 2, 2, step, true, 0.60f, 0.16f, 1.0f, (step == 7 || step == 15) ? 3 : 1);
            setDrumCell (*block, 2, 6, 0, true, 0.78f, 0.48f);

            for (int step : { 0, 4, 8, 12 }) setDrumCell (*block, 3, 0, step, true, 1.0f, 0.38f);
            for (int step : { 4, 12 }) setDrumCell (*block, 3, 1, step, true, 0.70f, 0.28f);
            for (int step : { 2, 6, 10, 14 }) setDrumCell (*block, 3, 3, step, true, 0.68f, 0.28f);
            for (int step : { 0, 8 }) setDrumCell (*block, 3, 7, step, true, 0.44f, 0.50f);

            modeBox.setSelectedId (5, juce::dontSendNotification);
            syncControlsFromBlock();
            notifyGraphChanged (true);
            drumPatternGrid.repaint();
            repaint();
        }
    }

    void MidiPlaygroundPage::randomiseSeed()
    {
        if (auto* block = ensureMidiBlock())
        {
            if (isDrumMachineBlock (*block))
            {
                const int pattern = juce::jlimit (0, 7, (drumPatternBox.getSelectedId() - 1 + 1) % 4);
                const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
                const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (*block, "dmSteps", 16.0f)));
                block->values["dmPattern"] = (float) pattern;
                block->values["dmSeed"] = 16001.0f + (float) pattern;
                block->values["dmSwing"] = 0.04f * (float) pattern;
                block->values["dmProbability"] = 0.92f;

                for (int track = 0; track < tracks; ++track)
                    for (int step = 0; step < steps; ++step)
                    {
                        bool active = false;
                        if (track == 0)
                            active = step == 0 || step % 4 == 0 || (pattern == 1 && step == 10);
                        else if (track == 1)
                            active = step % 8 == 4 || (pattern == 2 && step == 14);
                        else if (track == 2)
                            active = step % 2 == 0 || (pattern == 3 && step % 4 == 3);
                        else if (track == 3)
                            active = step % 8 == 6 || (pattern == 1 && step == 15);
                        else
                            active = pattern > 1 && step % juce::jmax (3, 9 - juce::jmin (track, 6)) == 0;

                        const float velocity = active ? juce::jlimit (0.0f, 1.0f, 0.58f + 0.08f * (float) ((step + track + pattern) % 5)) : 0.75f;
                        const float gate = track == 2 ? 0.14f : 0.28f + 0.04f * (float) (pattern % 3);
                        const float probability = active && (step + track) % 7 == 0 ? 0.78f : 1.0f;
                        setDrumCell (*block, pattern, track, step, active, velocity, gate, probability);
                    }

                drumPatternBox.setSelectedId (pattern + 1, juce::dontSendNotification);
                notifyGraphChanged (true);
                syncControlsFromBlock();
                drumPatternGrid.repaint();
                return;
            }

            const int bank = MidiPlaygroundPattern::getActiveBank (*block);
            block->values["mpSeed"] = 12001.0f + (float) bank;
            block->values["arpPattern"] = 7.0f;
            block->values["mpProbability"] = 0.92f;
            block->values["mpHumanize"] = 0.06f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = bank % 2 == 0 ? 1.0f : 2.0f;
            block->values["mpVelocityCurve"] = bank % 2 == 0 ? 0.10f : -0.12f;
            block->values["mpStrum"] = valueFor (*block, "mpChordMode", 0.0f) > 0.0f ? 0.12f : 0.0f;
            block->values["mpFlam"] = valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f ? 0.10f : 0.0f;
            block->values["mpEuclideanPulses"] = bank % 2 == 0 ? 0.0f : 5.0f;
            block->values["mpEuclideanRotate"] = (float) ((bank * 3) % 16);

            const int steps = juce::roundToInt (valueFor (*block, "arpSteps", 8.0f));
            static constexpr std::array<float, 16> motif {{ 0, 4, 7, 12, 14, 12, 7, 4, 2, 5, 9, 14, 17, 14, 9, 5 }};
            for (int step = 0; step < 16; ++step)
            {
                block->values["mpStep" + juce::String (step) + "On"] = step < steps && step % 7 != 6 ? 1.0f : 0.0f;
                block->values["arpNote" + juce::String (step)] = motif[(size_t) ((step + bank) % 16)];
                block->values["mpVelocity" + juce::String (step)] = step % 4 == 0 ? 0.96f : 0.64f + 0.06f * (float) ((step + bank) % 3);
                block->values["mpGate" + juce::String (step)] = step % 4 == 0 ? 0.70f : 0.42f;
                block->values["mpStepProb" + juce::String (step)] = step % 5 == 4 ? 0.82f : 1.0f;
                if (valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f)
                    block->values["mpSampleSlice" + juce::String (step)] = (float) (step % juce::jmax (1, juce::roundToInt (valueFor (*block, "mpSampleSliceCount", 16.0f))));
            }

            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
        }
    }

    // ------------------------------------------------------------------------
    // Deterministic pattern operators + authored phrase library.
    // No random anywhere — every transform is reversible and depends only on
    // the current pattern and (for scale-quantize) the selected scale.
    // ------------------------------------------------------------------------
    void MidiPlaygroundPage::showOperatorsMenu()
    {
        if (const auto* block = activeMidiBlock(); block != nullptr && isDrumMachineBlock (*block))
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Pattern Clipboard");
            m.addItem (200, "Copy Current Pattern");
            m.addItem (201, "Paste Into Current Pattern", hasDrumPatternClipboard);
            m.addItem (202, "Duplicate To Next Pattern");
            m.addSeparator();
            m.addSectionHeader ("Pattern Shape");
            m.addItem (210, "Shift One Step Left");
            m.addItem (211, "Shift One Step Right");
            m.addItem (212, "Accent Downbeats");
            m.addItem (213, "Add Ghost Notes");
            m.addItem (214, "Add Trap Hat Rolls");
            m.addSeparator();
            m.addSectionHeader ("Clear");
            m.addItem (220, "Clear Current Pattern");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&operatorsButton),
                [this] (int result) { if (result > 0) applyDrumOperator (result); });
            return;
        }

        juce::PopupMenu m;
        m.addSectionHeader ("Transpose");
        m.addItem (10, "Up 1 octave (+12)");
        m.addItem (11, "Down 1 octave (-12)");
        m.addItem (12, "Up a 5th (+7)");
        m.addItem (13, "Down a 5th (-7)");
        m.addItem (14, "Up a semitone (+1)");
        m.addItem (15, "Down a semitone (-1)");
        m.addSeparator();
        m.addSectionHeader ("Reshape");
        m.addItem (20, "Invert (mirror around root)");
        m.addItem (21, "Retrograde (reverse step order)");
        m.addItem (22, "Scale-quantize to current scale");
        m.addItem (23, "Compress to one octave");
        m.addSeparator();
        m.addSectionHeader ("Steps");
        m.addItem (30, "Activate every step");
        m.addItem (31, "Activate every other step");
        m.addItem (32, "Clear all steps");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&operatorsButton),
            [this] (int result) { if (result > 0) applyOperator (result); });
    }

    void MidiPlaygroundPage::applyOperator (int operatorId)
    {
        auto* block = ensureMidiBlock();
        if (block == nullptr)
            return;
        if (isDrumMachineBlock (*block))
        {
            applyDrumOperator (operatorId);
            return;
        }

        const int steps = midiStepCount (*block);

        // Scale degrees (semitones from root) for each scale id; mirrors what
        // the runtime uses. Index by scaleBox selectedId-1.
        static const std::vector<std::vector<int>> scaleDegrees = {
            { 0, 2, 4, 5, 7, 9, 11 },        // Major
            { 0, 2, 3, 5, 7, 8, 10 },        // Minor
            { 0, 2, 3, 5, 7, 8, 11 },        // Harmonic Minor
            { 0, 2, 3, 5, 7, 9, 11 },        // Melodic Minor
            { 0, 2, 4, 7, 9 },               // Pentatonic Major
            { 0, 3, 5, 7, 10 },              // Pentatonic Minor
            { 0, 2, 3, 5, 6, 8, 9, 11 },     // Diminished
            { 0, 2, 4, 6, 8, 10 },           // Whole Tone
            { 0, 1, 4, 5, 7, 8, 11 }         // Phrygian Dominant
        };

        auto snapToScale = [&] (int n)
        {
            const int idx = juce::jlimit (0, (int) scaleDegrees.size() - 1, scaleBox.getSelectedId() - 1);
            const auto& degs = scaleDegrees[(size_t) idx];
            const int oct = (int) std::floor ((double) n / 12.0);
            const int pc  = ((n % 12) + 12) % 12;
            int best = degs.front();
            int bestDist = 24;
            for (int d : degs)
            {
                const int dist = std::abs (d - pc);
                if (dist < bestDist) { bestDist = dist; best = d; }
            }
            return juce::jlimit (-24, 24, oct * 12 + best);
        };

        // Snapshot current step values so retrograde / invert see clean inputs.
        std::vector<int> notes ((size_t) steps);
        std::vector<bool> on ((size_t) steps);
        for (int s = 0; s < steps; ++s)
        {
            notes[(size_t) s] = juce::jlimit (-24, 24, roundStepNote (valueFor (*block, "arpNote" + juce::String (s), 0.0f)));
            on[(size_t) s]    = valueFor (*block, "mpStep" + juce::String (s) + "On", 1.0f) >= 0.5f;
        }

        auto setNotes = [&] ()
        {
            for (int s = 0; s < steps; ++s)
            {
                block->values["arpNote" + juce::String (s)] = (float) notes[(size_t) s];
                block->values["mpStep" + juce::String (s) + "On"] = on[(size_t) s] ? 1.0f : 0.0f;
            }
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
            repaint();
        };

        switch (operatorId)
        {
            case 10: case 11: case 12: case 13: case 14: case 15:
            {
                const int delta = operatorId == 10 ? 12 : operatorId == 11 ? -12
                                : operatorId == 12 ?  7 : operatorId == 13 ? -7
                                : operatorId == 14 ?  1 : -1;
                for (auto& n : notes) n = juce::jlimit (-24, 24, n + delta);
                setNotes(); break;
            }
            case 20: // Invert
                for (auto& n : notes) n = juce::jlimit (-24, 24, -n);
                setNotes(); break;
            case 21: // Retrograde
            {
                std::reverse (notes.begin(), notes.end());
                std::reverse (on.begin(), on.end());
                setNotes(); break;
            }
            case 22: // Scale-quantize
                for (auto& n : notes) n = snapToScale (n);
                setNotes(); break;
            case 23: // Compress to one octave (mod 12, keep sign of original)
                for (auto& n : notes)
                {
                    const int sign = n >= 0 ? 1 : -1;
                    n = sign * (std::abs (n) % 12);
                }
                setNotes(); break;
            case 30: std::fill (on.begin(), on.end(), true);  setNotes(); break;
            case 31:
                for (size_t s = 0; s < on.size(); ++s) on[s] = (s % 2) == 0;
                setNotes(); break;
            case 32: std::fill (on.begin(), on.end(), false); setNotes(); break;
            default: break;
        }
    }

    void MidiPlaygroundPage::applyDrumOperator (int operatorId)
    {
        auto* block = ensureMidiBlock();
        if (block == nullptr || ! isDrumMachineBlock (*block))
            return;

        const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (*block, "dmSteps", 16.0f)));

        auto finish = [&]
        {
            notifyGraphChanged (true);
            activeSummary.setText (blockSummary(), juce::dontSendNotification);
            syncControlsFromBlock();
            drumPatternGrid.repaint();
            repaint();
        };

        if (operatorId == 200)
        {
            drumPatternClipboard.clear();
            drumPatternClipboardTracks = tracks;
            drumPatternClipboardSteps = steps;
            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                {
                    const auto source = drumPrefixForPattern (pattern, track, step);
                    const auto clip = "T" + juce::String (track) + "S" + juce::String (step);
                    for (auto* suffix : { "On", "Vel", "Gate", "Prob", "Div" })
                        drumPatternClipboard[clip + suffix] = valueFor (*block, source + suffix,
                            juce::String (suffix) == "Vel" ? (track == 0 ? 1.0f : 0.75f)
                          : juce::String (suffix) == "Gate" ? (track == 2 ? 0.16f : 0.36f)
                          : juce::String (suffix) == "Prob" || juce::String (suffix) == "Div" ? 1.0f : 0.0f);
                }
            hasDrumPatternClipboard = true;
            return;
        }

        if (operatorId == 201 && hasDrumPatternClipboard)
        {
            const int pasteTracks = juce::jmin (tracks, drumPatternClipboardTracks);
            const int pasteSteps = juce::jmin (steps, drumPatternClipboardSteps);
            for (int track = 0; track < pasteTracks; ++track)
                for (int step = 0; step < pasteSteps; ++step)
                {
                    const auto clip = "T" + juce::String (track) + "S" + juce::String (step);
                    setDrumCell (*block, pattern, track, step,
                                 drumPatternClipboard[clip + "On"] >= 0.5f,
                                 drumPatternClipboard[clip + "Vel"],
                                 drumPatternClipboard[clip + "Gate"],
                                 drumPatternClipboard[clip + "Prob"],
                                 juce::roundToInt (drumPatternClipboard[clip + "Div"]));
                }
            finish();
            return;
        }

        if (operatorId == 202)
        {
            const int destination = (pattern + 1) % 8;
            copyDrumPattern (*block, pattern, destination, tracks, steps);
            block->values["dmPattern"] = (float) destination;
            drumPatternBox.setSelectedId (destination + 1, juce::dontSendNotification);
            finish();
            return;
        }

        if (operatorId == 210 || operatorId == 211)
        {
            shiftDrumPattern (*block, pattern, tracks, steps, operatorId == 210 ? -1 : 1);
            finish();
            return;
        }

        if (operatorId == 212)
        {
            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                {
                    const auto prefix = drumPrefixForPattern (pattern, track, step);
                    if (valueFor (*block, prefix + "On", 0.0f) < 0.5f)
                        continue;
                    const bool downbeat = (step % 4) == 0;
                    block->values[prefix + "Vel"] = juce::jlimit (0.0f, 1.0f,
                        valueFor (*block, prefix + "Vel", track == 0 ? 1.0f : 0.75f)
                            + (downbeat ? 0.16f : -0.04f));
                }
            finish();
            return;
        }

        if (operatorId == 213)
        {
            for (int step = 2; step < steps; step += 4)
            {
                setDrumCell (*block, pattern, 1, step, true, 0.30f, 0.18f, 0.70f, 1);
                if (tracks > 9)
                    setDrumCell (*block, pattern, 9, (step + 3) % steps, true, 0.26f, 0.16f, 0.60f, 1);
            }
            finish();
            return;
        }

        if (operatorId == 214)
        {
            const int hatTrack = juce::jmin (2, tracks - 1);
            for (int step = 0; step < steps; ++step)
            {
                if ((step % 2) == 0)
                    setDrumCell (*block, pattern, hatTrack, step, true, step % 4 == 0 ? 0.68f : 0.52f, 0.12f, 1.0f, 1);
                if (step == 3 || step == 7 || step == 11 || step == 15 || (steps > 16 && (step == 23 || step == 31)))
                    setDrumCell (*block, pattern, hatTrack, step, true, 0.58f, 0.10f, 1.0f, (step % 8 == 7) ? 4 : 3);
            }
            finish();
            return;
        }

        if (operatorId == 220)
        {
            clearDrumPattern (*block, pattern, tracks, steps);
            finish();
        }
    }

    void MidiPlaygroundPage::showPhraseLibraryMenu()
    {
        if (const auto* block = activeMidiBlock(); block != nullptr && isDrumMachineBlock (*block))
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Drum Machine Templates");
            m.addItem (301, "Modern Trap Rolls");
            m.addItem (302, "808 Bounce");
            m.addItem (303, "Four-On-The-Floor House");
            m.addItem (304, "Industrial Techno Drive");
            m.addItem (305, "DnB Roller");
            m.addItem (306, "Reggaeton Dembow");
            m.addItem (307, "Boom Bap Pocket");
            m.addItem (308, "Afro House Percussion");
            m.addItem (309, "UK Garage Shuffle");
            m.addItem (310, "Cinematic Half-Time Impacts");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&phraseLibraryButton),
                [this] (int result) { if (result > 0) applyDrumTemplate (result); });
            return;
        }

        juce::PopupMenu m;
        m.addSectionHeader ("Trance / Electronic Phrases (16 steps)");
        m.addItem (1,  "1. Driving 8th-note root");
        m.addItem (2,  "2. Off-beat octave bounce");
        m.addItem (3,  "3. Trance arp climb (1-3-5-1)");
        m.addItem (4,  "4. Goa pulse (root + 7ths)");
        m.addItem (5,  "5. Acid bassline (1-5-3-7)");
        m.addItem (6,  "6. Pluck riff (descending 5ths)");
        m.addItem (7,  "7. Roll-up (chromatic ascent)");
        m.addItem (8,  "8. Anjuna lead (4-5-1-2)");
        m.addItem (9,  "9. 16th hi-hat hat");
        m.addItem (10, "10. Triplet feel (3 against 4)");
        m.addSeparator();
        m.addSectionHeader ("Bass / Lead Patterns");
        m.addItem (11, "11. Two-octave run");
        m.addItem (12, "12. Skipping 5ths");
        m.addItem (13, "13. Power-fifth stab");
        m.addItem (14, "14. Sub-octave drop");
        m.addItem (15, "15. Walking bass");
        m.addSeparator();
        m.addSectionHeader ("Atmospheric / Pad");
        m.addItem (16, "16. Sustained root");
        m.addItem (17, "17. Slow octave alternate");
        m.addItem (18, "18. Suspended fourth swell");
        m.addItem (19, "19. Whole-note progression frame");
        m.addItem (20, "20. Sparse melodic skeleton");
        m.addSeparator();
        m.addSectionHeader ("Advanced Arp Instrument");
        m.addItem (21, "21. 64-step performance arp");
        m.addSeparator();
        m.addItem (99, "Clear pattern");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&phraseLibraryButton),
            [this] (int r) { if (r > 0) applyPhraseFromLibrary (r); });
    }

    void MidiPlaygroundPage::applyPhraseFromLibrary (int phraseId)
    {
        auto* block = ensureMidiBlock();
        if (block == nullptr)
            return;
        if (isDrumMachineBlock (*block))
        {
            applyDrumTemplate (phraseId);
            return;
        }

        if (phraseId == 21)
        {
            block->name = "64-step Performance Arp";
            block->values["arpSteps"] = 64.0f;
            block->values["arpPattern"] = 0.0f;
            block->values["arpGate"] = 0.50f;
            block->values["arpSwing"] = 0.10f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpChordMode"] = 0.0f;
            block->values["mpChordSize"] = 1.0f;
            static constexpr std::array<float, 16> notes {{
                0, 7, 12, 14, 19, 14, 12, 7, 3, 10, 15, 17, 22, 17, 15, 10
            }};
            for (int s = 0; s < 64; ++s)
            {
                const int local = s % 16;
                const auto suffix = juce::String (s);
                block->values["arpNote" + suffix] = notes[(size_t) local] + (s >= 32 ? 12.0f : 0.0f);
                block->values["mpStep" + suffix + "On"] = 1.0f;
                block->values["mpVelocity" + suffix] = local % 4 == 0 ? 0.98f : 0.58f + 0.08f * (float) (local % 4);
                block->values["mpGate" + suffix] = local % 4 == 0 ? 0.70f : 0.36f;
                block->values["mpStepProb" + suffix] = local == 15 ? 0.76f : 1.0f;
                block->values["mpSampleSlice" + suffix] = -1.0f;
                block->values["mpStepDiv" + suffix] = (local == 6 || local == 14) ? 3.0f : (local == 7 ? 2.0f : 1.0f);
                block->values["mpStepTranspose" + suffix] = (s / 16 == 1) ? 5.0f : (s / 16 == 2 ? 7.0f : 0.0f);
                block->values["mpStepChordMode" + suffix] = local % 8 == 0 ? 15.0f : -1.0f;
                block->values["mpStepChordSize" + suffix] = local % 8 == 0 ? 3.0f : -1.0f;
                block->values["mpStepTie" + suffix] = local == 3 ? 1.0f : 0.0f;
            }
            modeBox.setSelectedId (3, juce::dontSendNotification);
            editorViewBox.setSelectedId (2, juce::dontSendNotification);
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
            repaint();
            return;
        }

        // Each phrase is a 16-element array of {note, on}. note is offset
        // semitones from root, on is whether the step is active.
        struct Phrase { std::array<std::pair<int,bool>, 16> steps; };
        const auto P = [] (std::initializer_list<std::pair<int,bool>> il)
        {
            Phrase p {};
            int i = 0;
            for (auto kv : il) { if (i < 16) p.steps[(size_t) i++] = kv; }
            for (; i < 16; ++i) p.steps[(size_t) i] = {0, false};
            return p;
        };

        const std::map<int, Phrase> library = {
            // 1. Driving 8th-note root
            { 1, P ({{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},
                     {0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true}}) },
            // 2. Off-beat octave bounce
            { 2, P ({{0,true},{12,false},{0,true},{12,false},{0,true},{12,false},{0,true},{12,false},
                     {0,true},{12,false},{0,true},{12,false},{0,true},{12,false},{0,true},{12,false}}) },
            // 3. Trance arp climb 1-3-5-1
            { 3, P ({{0,true},{4,true},{7,true},{12,true},{0,true},{4,true},{7,true},{12,true},
                     {0,true},{4,true},{7,true},{12,true},{0,true},{4,true},{7,true},{12,true}}) },
            // 4. Goa pulse (root + 7ths)
            { 4, P ({{0,true},{0,true},{10,true},{0,true},{0,true},{10,true},{0,true},{10,true},
                     {0,true},{0,true},{10,true},{0,true},{0,true},{10,true},{0,true},{10,true}}) },
            // 5. Acid bassline 1-5-3-7
            { 5, P ({{-12,true},{-5,true},{-9,true},{-2,true},{-12,true},{-5,true},{-9,true},{-2,true},
                     {-12,true},{-5,true},{-9,true},{-2,true},{-12,true},{-5,true},{-9,true},{-2,true}}) },
            // 6. Pluck riff (descending 5ths)
            { 6, P ({{12,true},{7,true},{0,true},{-5,true},{12,true},{7,true},{0,true},{-5,true},
                     {12,true},{7,true},{0,true},{-5,true},{12,true},{7,true},{0,true},{-5,true}}) },
            // 7. Roll-up (chromatic ascent)
            { 7, P ({{0,true},{1,true},{2,true},{3,true},{4,true},{5,true},{6,true},{7,true},
                     {8,true},{9,true},{10,true},{11,true},{12,true},{13,true},{14,true},{15,true}}) },
            // 8. Anjuna lead 4-5-1-2
            { 8, P ({{5,true},{7,true},{0,true},{2,true},{5,true},{7,true},{0,true},{2,true},
                     {5,true},{7,true},{0,true},{2,true},{5,true},{7,true},{0,true},{2,true}}) },
            // 9. 16th hat
            { 9, P ({{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},
                     {0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true},{0,true}}) },
            // 10. Triplet feel
            { 10, P ({{0,true},{0,false},{0,true},{0,false},{0,true},{0,false},{0,true},{0,false},
                      {0,true},{0,false},{0,true},{0,false},{0,true},{0,false},{0,true},{0,false}}) },
            // 11. Two-octave run
            { 11, P ({{0,true},{2,true},{4,true},{5,true},{7,true},{9,true},{11,true},{12,true},
                      {14,true},{16,true},{17,true},{19,true},{21,true},{23,true},{24,true},{0,false}}) },
            // 12. Skipping 5ths
            { 12, P ({{0,true},{7,true},{0,false},{7,true},{0,true},{7,true},{0,false},{7,true},
                      {0,true},{7,true},{0,false},{7,true},{0,true},{7,true},{0,false},{7,true}}) },
            // 13. Power-fifth stab
            { 13, P ({{0,true},{0,false},{0,false},{0,false},{0,true},{0,false},{0,false},{0,false},
                      {0,true},{0,false},{0,false},{0,false},{0,true},{0,false},{0,false},{0,false}}) },
            // 14. Sub-octave drop
            { 14, P ({{-24,true},{0,false},{0,false},{0,false},{0,true},{0,false},{0,false},{0,false},
                      {-12,true},{0,false},{0,false},{0,false},{0,true},{0,false},{0,false},{0,false}}) },
            // 15. Walking bass
            { 15, P ({{0,true},{2,true},{4,true},{5,true},{7,true},{5,true},{4,true},{2,true},
                      {0,true},{-3,true},{-5,true},{-3,true},{0,true},{2,true},{4,true},{5,true}}) },
            // 16. Sustained root (1 long note)
            { 16, P ({{0,true},{0,false},{0,false},{0,false},{0,false},{0,false},{0,false},{0,false},
                      {0,false},{0,false},{0,false},{0,false},{0,false},{0,false},{0,false},{0,false}}) },
            // 17. Slow octave alternate
            { 17, P ({{0,true},{0,false},{0,false},{0,false},{12,true},{0,false},{0,false},{0,false},
                      {0,true},{0,false},{0,false},{0,false},{12,true},{0,false},{0,false},{0,false}}) },
            // 18. Suspended fourth swell
            { 18, P ({{0,true},{0,false},{5,true},{0,false},{7,true},{0,false},{5,true},{0,false},
                      {0,true},{0,false},{5,true},{0,false},{7,true},{0,false},{5,true},{0,false}}) },
            // 19. Whole-note progression frame
            { 19, P ({{0,true},{0,false},{0,false},{0,false},{5,true},{0,false},{0,false},{0,false},
                      {-3,true},{0,false},{0,false},{0,false},{7,true},{0,false},{0,false},{0,false}}) },
            // 20. Sparse melodic skeleton
            { 20, P ({{0,true},{0,false},{0,false},{4,true},{0,false},{0,false},{7,true},{0,false},
                      {0,true},{0,false},{4,true},{0,false},{0,false},{7,true},{0,false},{12,true}}) },
        };

        if (phraseId == 99)
        {
            for (int s = 0; s < 16; ++s)
            {
                block->values["arpNote" + juce::String (s)] = 0.0f;
                block->values["mpStep" + juce::String (s) + "On"] = 0.0f;
            }
        }
        else
        {
            auto it = library.find (phraseId);
            if (it == library.end()) return;
            const auto& ph = it->second;
            for (int s = 0; s < 16; ++s)
            {
                block->values["arpNote" + juce::String (s)] = (float) ph.steps[(size_t) s].first;
                block->values["mpStep" + juce::String (s) + "On"] = ph.steps[(size_t) s].second ? 1.0f : 0.0f;
            }
        }

        MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
        notifyGraphChanged (true);
        syncControlsFromBlock();
        repaint();
    }

    void MidiPlaygroundPage::applyDrumTemplate (int templateId)
    {
        auto* block = ensureMidiBlock();
        if (block == nullptr || ! isDrumMachineBlock (*block))
            return;

        const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (*block, "dmSteps", 16.0f)));
        clearDrumPattern (*block, pattern, tracks, steps);

        auto hit = [&] (int track, int step, float velocity, float gate = 0.32f, float probability = 1.0f, int divisions = 1)
        {
            if (track < tracks && step < steps)
                setDrumCell (*block, pattern, track, step, true, velocity, gate, probability, divisions);
        };
        auto every = [&] (int track, int everySteps, float velocity, float gate, int divisions = 1)
        {
            for (int step = 0; step < steps; step += everySteps)
                hit (track, step, velocity, gate, 1.0f, divisions);
        };

        switch (templateId)
        {
            case 301: // Modern Trap Rolls
                hit (0, 0, 1.0f, 0.40f); hit (0, 7, 0.90f, 0.32f); hit (0, 10, 0.82f, 0.30f);
                hit (1, 4, 0.92f, 0.30f); hit (1, 12, 0.96f, 0.30f);
                for (int step = 0; step < steps; step += 2) hit (2, step, step % 4 == 0 ? 0.68f : 0.50f, 0.12f);
                for (int step : { 3, 7, 11, 15 }) hit (2, step, 0.58f, 0.10f, 1.0f, step == 15 ? 4 : 3);
                hit (9, 6, 0.38f, 0.18f, 0.80f); hit (9, 14, 0.34f, 0.18f, 0.70f);
                break;

            case 302: // 808 Bounce
                hit (0, 0, 1.0f, 0.46f); hit (0, 3, 0.72f, 0.34f); hit (0, 10, 0.86f, 0.40f); hit (0, 14, 0.62f, 0.28f);
                hit (1, 4, 0.90f, 0.30f); hit (1, 12, 0.92f, 0.30f);
                for (int step = 0; step < steps; ++step) hit (2, step, step % 2 == 0 ? 0.42f : 0.30f, 0.10f, step % 4 == 3 ? 0.75f : 1.0f);
                hit (3, 15, 0.56f, 0.22f, 1.0f, 2);
                break;

            case 303: // House
                every (0, 4, 1.0f, 0.38f);
                hit (1, 4, 0.88f, 0.30f); hit (1, 12, 0.88f, 0.30f);
                for (int step = 2; step < steps; step += 4) hit (3, step, 0.64f, 0.20f);
                for (int step = 0; step < steps; step += 2) hit (2, step, step % 4 == 0 ? 0.46f : 0.58f, 0.12f);
                hit (12, 5, 0.36f, 0.18f, 0.75f); hit (12, 13, 0.34f, 0.18f, 0.75f);
                break;

            case 304: // Industrial Techno
                every (0, 4, 1.0f, 0.38f);
                hit (1, 4, 0.78f, 0.24f); hit (1, 12, 0.82f, 0.24f);
                for (int step : { 1, 3, 6, 9, 11, 14 }) hit (13, step, 0.48f, 0.18f, 0.92f);
                for (int step = 0; step < steps; step += 2) hit (2, step, 0.42f + 0.12f * (float) ((step / 2) % 2), 0.10f);
                hit (6, 0, 0.52f, 0.50f); hit (6, 8, 0.42f, 0.44f);
                break;

            case 305: // DnB
                hit (0, 0, 1.0f, 0.32f); hit (0, 6, 0.86f, 0.28f); hit (0, 10, 0.92f, 0.30f);
                hit (1, 4, 0.96f, 0.24f); hit (1, 12, 0.98f, 0.24f);
                for (int step = 0; step < steps; ++step) hit (2, step, step % 4 == 0 ? 0.62f : 0.40f, 0.09f);
                hit (13, 7, 0.44f, 0.16f); hit (13, 15, 0.50f, 0.16f, 1.0f, 2);
                break;

            case 306: // Reggaeton
                hit (0, 0, 1.0f, 0.40f); hit (0, 3, 0.78f, 0.30f); hit (0, 8, 0.96f, 0.38f); hit (0, 11, 0.72f, 0.28f);
                hit (1, 4, 0.88f, 0.28f); hit (1, 12, 0.92f, 0.28f);
                for (int step : { 2, 6, 10, 14 }) hit (2, step, 0.48f, 0.12f);
                hit (9, 7, 0.46f, 0.16f); hit (9, 15, 0.50f, 0.16f);
                break;

            case 307: // Boom Bap
                hit (0, 0, 1.0f, 0.38f); hit (0, 6, 0.76f, 0.30f); hit (0, 10, 0.84f, 0.30f);
                hit (1, 4, 0.94f, 0.30f); hit (1, 12, 0.96f, 0.30f);
                for (int step : { 0, 2, 6, 8, 10, 14 }) hit (2, step, 0.38f, 0.11f);
                hit (8, 3, 0.28f, 0.12f, 0.70f); hit (8, 11, 0.28f, 0.12f, 0.70f);
                break;

            case 308: // Afro House
                every (0, 4, 0.96f, 0.36f);
                hit (1, 4, 0.78f, 0.24f); hit (1, 12, 0.78f, 0.24f);
                for (int step : { 1, 4, 6, 9, 12, 14 }) hit (13, step, 0.52f, 0.17f);
                for (int step : { 2, 5, 7, 10, 13, 15 }) hit (12, step, 0.42f, 0.12f, 0.88f);
                break;

            case 309: // UK Garage
                hit (0, 0, 1.0f, 0.34f); hit (0, 5, 0.78f, 0.28f); hit (0, 10, 0.86f, 0.28f);
                hit (1, 4, 0.88f, 0.25f); hit (1, 11, 0.78f, 0.22f); hit (1, 12, 0.92f, 0.25f);
                for (int step : { 1, 3, 6, 8, 10, 13, 15 }) hit (2, step, 0.44f, 0.10f);
                hit (3, 7, 0.58f, 0.18f, 1.0f, 2);
                break;

            case 310: // Cinematic Half-Time
                hit (0, 0, 1.0f, 0.55f); hit (0, 10, 0.86f, 0.44f);
                hit (1, 8, 0.98f, 0.38f); hit (6, 0, 0.72f, 0.66f); hit (7, 12, 0.54f, 0.70f);
                for (int step : { 3, 7, 11, 15 }) hit (2, step, 0.38f, 0.12f, 0.82f, step == 15 ? 3 : 1);
                hit (14, 15, 0.62f, 0.22f, 1.0f, 4);
                break;

            default:
                return;
        }

        notifyGraphChanged (true);
        activeSummary.setText (blockSummary(), juce::dontSendNotification);
        syncControlsFromBlock();
        drumPatternGrid.repaint();
        repaint();
    }

    void MidiPlaygroundPage::applySelectedMusicalPreset()
    {
        const int id = musicalPresetBox.getSelectedId();
        if (id <= 0)
        {
            activeSummary.setText ("Choose a musical idea, then press Use This Pattern to make it the active playable slot.",
                                   juce::dontSendNotification);
            repaint();
            return;
        }

        if (id >= 1000 && id < 2000)
        {
            const int chordId = id - 1000;
            chordPresetBox.setSelectedId (chordId, juce::dontSendNotification);
            applySelectedChordPreset();
            activeSummary.setText ("Dropped chord preset into Piano Roll: " + musicalPresetBox.getText(),
                                   juce::dontSendNotification);
            return;
        }

        if (id >= 2000 && id < 3000)
        {
            int progressionId = 1;
            switch (id)
            {
                case 2021: progressionId = 1;  break;  // I - V - vi - IV
                case 2022: progressionId = 3;  break;  // ii - V - I
                case 2025: progressionId = 11; break;  // Dark Trailer Resolve
                case 2026: progressionId = 8;  break;  // Cinematic Lift
                default:   progressionId = 1;  break;
            }

            progressionBox.setSelectedId (progressionId, juce::dontSendNotification);
            applySelectedProgression();
            activeSummary.setText ("Dropped progression into Piano Roll: " + musicalPresetBox.getText(),
                                   juce::dontSendNotification);
            return;
        }

        if (id >= 3000 && id < 4000)
        {
            configureChordPhrase();
            applyPhraseFromLibrary (id - 3000);
            editorViewBox.setSelectedId (2, juce::dontSendNotification);
            activeSummary.setText ("Dropped editable melody/bass phrase: " + musicalPresetBox.getText(),
                                   juce::dontSendNotification);
            return;
        }

        if (id >= 4000 && id < 5000)
        {
            configureDrumMachine();
            applyDrumTemplate (id - 4000);
            activeSummary.setText ("Dropped drum groove into the Drum Machine: " + musicalPresetBox.getText(),
                                   juce::dontSendNotification);
            return;
        }

        if (id == 5001)
        {
            configureSampleSliceControl();
            editorViewBox.setSelectedId (1, juce::dontSendNotification);
            activeSummary.setText ("Dropped Sample Chopper behavior. Import/slice samples in Sample Mapper, then edit steps here.",
                                   juce::dontSendNotification);
            return;
        }

        if (id == 5002)
        {
            midiTemplateBox.setSelectedId (5, juce::dontSendNotification);
            applySelectedMidiTemplate();
            activeSummary.setText ("Dropped Glitch Gate behavior. Edit active steps, probability, ratchet, and slice targets.",
                                   juce::dontSendNotification);
        }
    }

    void MidiPlaygroundPage::generateAiMidi()
    {
        auto* window = new juce::AlertWindow ("AI MIDI Pattern Generator",
            "Describe the pattern you want (e.g. 'A driving 16th note bassline' or 'A syncopated tech-house drum groove').",
            juce::MessageBoxIconType::QuestionIcon);
        
        window->addTextEditor ("Prompt", "", "Prompt:");
        window->addButton ("Generate", 1);
        window->addButton ("Cancel", 0);

        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result == 1)
                {
                    auto* text = window->getTextEditor ("Prompt");
                    if (text == nullptr) return;
                    juce::String prompt = text->getText();
                    if (prompt.trim().isEmpty()) return;

                    auto* block = activeMidiBlock();
                    if (block == nullptr) return;

                    juce::Thread::launch ([this, prompt]()
                    {
                        AiAssistService::ProjectContextPack ctx;
                        AiAssistService service;
                        AiAssistService::Suggestion suggestion = service.runWithPrompt (AiAssistService::TaskType::GenerateMidiJson, ctx, prompt);
                        
                        juce::MessageManager::callAsync ([this, suggestion]()
                        {
                            auto trimmed = suggestion.details.trim();
                            if (trimmed.startsWith ("{") || trimmed.startsWith ("["))
                            {
                                auto parsed = juce::JSON::parse (trimmed);
                                if (auto* array = parsed.getArray())
                                {
                                    auto* block = activeMidiBlock();
                                    if (block != nullptr)
                                    {
                                        bool isDrum = isDrumMachineBlock (*block);
                                        int numSteps = (int) block->values["arpSteps"];
                                        if (numSteps <= 0) numSteps = 16;
                                        
                                        // Clear current pattern
                                        for (int i = 0; i < 128; ++i)
                                        {
                                            if (isDrum)
                                            {
                                                for (int t = 0; t < 8; ++t)
                                                    setDrumStepFromEditor (t, i, false, 0.0f, true);
                                            }
                                            else
                                            {
                                                // Just set step to off
                                                block->values["mpStep" + juce::String(i) + "On"] = 0.0f;
                                            }
                                        }

                                        for (auto& item : *array)
                                        {
                                            if (auto* obj = item.getDynamicObject())
                                            {
                                                int note = obj->hasProperty ("note") ? (int) obj->getProperty ("note") : 60;
                                                float velocity = obj->hasProperty ("velocity") ? (float) obj->getProperty ("velocity") : 0.8f;
                                                float gate = obj->hasProperty ("duration") ? (float) obj->getProperty ("duration") : 0.25f;
                                                float start = obj->hasProperty ("start") ? (float) obj->getProperty ("start") : 0.0f;
                                                
                                                int step = juce::roundToInt (start * 4.0f);
                                                if (step >= 0 && step < numSteps)
                                                {
                                                    if (isDrum)
                                                    {
                                                        int track = (note - 36) % 8;
                                                        if (track >= 0 && track < 8)
                                                            setDrumStepFromEditor (track, step, true, velocity, true);
                                                    }
                                                    else
                                                    {
                                                        int noteOffset = note - 60;
                                                        block->values["mpStep" + juce::String(step) + "On"] = 1.0f;
                                                        setStepValueFromEditor (step, noteOffset, velocity, gate, 1.0f, true, false, true, true, true);
                                                    }
                                                }
                                            }
                                        }
                                        refresh();
                                    }
                                }
                                else
                                {
                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AI Error", "Failed to parse MIDI JSON.");
                                }
                            }
                            else
                            {
                                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AI Error", suggestion.details);
                            }
                        });
                    });
                }
            }), true);
    }

    void MidiPlaygroundPage::switchPhraseBank (int bank)
    {
        if (auto* block = ensureMidiBlock())
        {
            const auto previousBank = MidiPlaygroundPattern::getActiveBank (*block);
            MidiPlaygroundPattern::storeActiveBank (*block, previousBank);
            MidiPlaygroundPattern::loadBank (*block, bank, true);
            notifyGraphChanged (true);
            syncControlsFromBlock();
            midiOutputLane.repaint();
        }
    }

    void MidiPlaygroundPage::storeActivePhraseBank()
    {
        if (auto* block = ensureMidiBlock())
        {
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            activeSummary.setText ("Saved " + phraseBankBox.getText() + " for " + block->name
                                   + ". It now plays this DSP sound and can be tested, exported as MIDI, or shown in the Player.",
                                   juce::dontSendNotification);
            notifyGraphChanged (true);
            repaint();
        }
    }

    void MidiPlaygroundPage::duplicateActivePhraseBank()
    {
        if (auto* block = ensureMidiBlock())
        {
            const int sourceBank = MidiPlaygroundPattern::getActiveBank (*block);
            const int destinationBank = (sourceBank + 1) % MidiPlaygroundPattern::kPhraseBankCount;
            MidiPlaygroundPattern::storeActiveBank (*block, sourceBank);
            MidiPlaygroundPattern::copyBank (*block, sourceBank, destinationBank);
            MidiPlaygroundPattern::loadBank (*block, destinationBank, false);
            notifyGraphChanged (true);
            syncControlsFromBlock();
            midiOutputLane.repaint();
        }
    }

    void MidiPlaygroundPage::applySelectedProgression()
    {
        if (auto* block = ensureMidiBlock())
        {
            const int bank = MidiPlaygroundPattern::getActiveBank (*block);
            const int progressionIndex = juce::jmax (0, progressionBox.getSelectedId() - 1);
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);
            MidiPlaygroundPattern::applyProgressionPreset (*block, progressionIndex, bank);
            notifyGraphChanged (true);
            syncControlsFromBlock();
            midiOutputLane.repaint();
        }
    }

    void MidiPlaygroundPage::applySelectedChordPreset()
    {
        const int presetId = chordPresetBox.getSelectedId();
        if (presetId <= 0)
            return;

        if (auto* block = ensureMidiBlock())
        {
            if (isDrumMachineBlock (*block))
                configureChordPhrase();

            block = ensureMidiBlock();
            if (block == nullptr)
                return;

            block->type = "midiPlayground";
            block->section = "mod";
            block->enabled = true;
            block->targetId = "filterCutoff";
            block->name = chordPresetNameForId (presetId);
            block->values["sync"] = 1.0f;
            block->values["rate"] = 1.0f;
            block->values["arpSteps"] = 16.0f;
            block->values["arpPattern"] = 0.0f;
            block->values["arpGate"] = 0.92f;
            block->values["arpSwing"] = 0.0f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.0f;
            block->values["mpMutation"] = 0.0f;
            block->values["mpRatchet"] = 1.0f;
            block->values["mpSampleControl"] = 0.0f;
            block->values["mpChordPreset"] = (float) presetId;
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);

            auto fillStep = [&] (int step, float noteOffset, bool active, float velocity = 0.86f, float gate = 0.92f)
            {
                const auto suffix = juce::String (step);
                block->values["arpNote" + suffix] = noteOffset;
                block->values["mpStep" + suffix + "On"] = active ? 1.0f : 0.0f;
                block->values["mpVelocity" + suffix] = velocity;
                block->values["mpGate" + suffix] = gate;
                block->values["mpStepProb" + suffix] = 1.0f;
                block->values["mpSampleSlice" + suffix] = -1.0f;
            };

            for (int step = 0; step < 16; ++step)
                fillStep (step, 0.0f, false);

            if (presetId <= 23)
            {
                const int chordMode = chordModeForPresetId (presetId);
                const int chordSize = (int) chordIntervalsForMode (chordMode, 8).size();
                block->values["mpScaleType"] = 0.0f;
                block->values["mpChordMode"] = (float) chordMode;
                block->values["mpChordSize"] = (float) chordSize;
                block->values["mpChordSpread"] = 0.0f;
                fillStep (0, 0.0f, true, 0.95f, 1.0f);
            }
            else
            {
                block->values["mpChordSpread"] = presetId == 25 ? 0.18f : 0.0f;
                block->values["mpChordMode"] = (float) chordModeForPresetId (presetId);
                block->values["mpChordSize"] = presetId == 25 ? 4.0f : 3.0f;

                std::array<float, 8> offsets {};
                int chordCount = 4;
                if (presetId == 24)
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 0.0f, 7.0f, 9.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                }
                else if (presetId == 25)
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 2.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                    chordCount = 3;
                }
                else if (presetId == 26)
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 0.0f, 9.0f, 5.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                }
                else if (presetId == 27)
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 9.0f, 5.0f, 0.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                }
                else if (presetId == 28)
                {
                    block->values["mpScaleType"] = 2.0f;
                    offsets = {{ 0.0f, 10.0f, 8.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                    block->values["mpChordMode"] = 1.0f;
                }
                else if (presetId == 29)
                {
                    block->values["mpScaleType"] = 2.0f;
                    offsets = {{ 0.0f, 8.0f, 3.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                    block->values["mpChordMode"] = 1.0f;
                }
                else if (presetId == 30)
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 0.0f, 5.0f, 7.0f, 7.0f, 0.0f, 0.0f, 0.0f, 0.0f }};
                }
                else
                {
                    block->values["mpScaleType"] = 1.0f;
                    offsets = {{ 0.0f, 7.0f, 9.0f, 4.0f, 5.0f, 0.0f, 5.0f, 7.0f }};
                    chordCount = 8;
                }

                const int spacing = chordCount > 4 ? 2 : 4;
                for (int chord = 0; chord < chordCount; ++chord)
                {
                    const int step = chord * spacing;
                    if (step >= 16)
                        break;
                    fillStep (step, offsets[(size_t) chord], true, chord == 0 ? 0.96f : 0.84f, chordCount > 4 ? 0.88f : 0.96f);
                }
            }

            editorViewBox.setSelectedId (2, juce::dontSendNotification);
            modeBox.setSelectedId (1, juce::dontSendNotification);
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
            pianoRollEditor.repaint();
            midiOutputLane.repaint();
            repaint();
        }
    }

    void MidiPlaygroundPage::applySelectedMidiTemplate()
    {
        const int templateId = midiTemplateBox.getSelectedId();
        if (templateId == 3)
        {
            configureDrumMachine();
            return;
        }

        if (templateId == 4)
        {
            configureSampleSliceControl();
            editorViewBox.setSelectedId (1, juce::dontSendNotification);
            return;
        }

        if (templateId == 2)
            configureChordPhrase();
        else
            ensureMidiBlock();

        if (auto* block = activeMidiBlock())
        {
            block->type = "midiPlayground";
            block->section = "mod";
            block->enabled = true;
            block->values["sync"] = 1.0f;
            block->values["arpSteps"] = 16.0f;
            block->values["mpSampleControl"] = 0.0f;
            block->values["mpModLane"] = 0.0f;
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);
            block->values["mpScaleType"] = (float) juce::jmax (0, scaleBox.getSelectedId() - 1);
            block->targetId = "filterCutoff";

            auto fillStep = [&] (int step, float note, bool active, float velocity, float gate, float probability = 1.0f)
            {
                const auto suffix = juce::String (step);
                block->values["arpNote" + suffix] = note;
                block->values["mpStep" + suffix + "On"] = active ? 1.0f : 0.0f;
                block->values["mpVelocity" + suffix] = juce::jlimit (0.0f, 1.0f, velocity);
                block->values["mpGate" + suffix] = juce::jlimit (0.05f, 1.0f, gate);
                block->values["mpStepProb" + suffix] = juce::jlimit (0.0f, 1.0f, probability);
                block->values["mpSampleSlice" + suffix] = -1.0f;
                block->values["mpStepDiv" + suffix] = 1.0f;
                block->values["mpStepTranspose" + suffix] = 0.0f;
                block->values["mpStepChordMode" + suffix] = -1.0f;
                block->values["mpStepChordSize" + suffix] = -1.0f;
                block->values["mpStepTie" + suffix] = 0.0f;
            };

            if (templateId == 2)
            {
                block->name = "Cinematic Chord Bed";
                block->values["arpGate"] = 0.82f;
                block->values["arpSwing"] = 0.08f;
                block->values["mpChordMode"] = 1.0f;
                block->values["mpChordSize"] = 3.0f;
                block->values["mpChordSpread"] = 0.30f;
                const float notes[] { 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, 5.0f, 5.0f, 5.0f,
                                      9.0f, 9.0f, 9.0f, 9.0f, 7.0f, 7.0f, 7.0f, 7.0f };
                for (int step = 0; step < 16; ++step)
                    fillStep (step, notes[step], step % 4 == 0, step % 8 == 0 ? 0.92f : 0.74f, 0.88f);
                modeBox.setSelectedId (1, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 5)
            {
                block->name = "Glitch Gate Template";
                block->targetId = "sampleSlice";
                block->values["mpSampleControl"] = 1.0f;
                block->values["mpModLane"] = 0.0f;
                block->values["arpGate"] = 0.20f;
                block->values["arpSwing"] = 0.12f;
                block->values["mpProbability"] = 0.78f;
                block->values["mpRatchet"] = 2.0f;
                block->values["mpSampleSliceCount"] = 16.0f;
                block->values["sampleSliceCount"] = 16.0f;
                for (int step = 0; step < 16; ++step)
                {
                    fillStep (step, 0.0f, step % 4 != 3, step % 4 == 0 ? 1.0f : 0.62f,
                              step % 2 == 0 ? 0.18f : 0.32f, step % 4 == 2 ? 0.62f : 1.0f);
                    block->values["mpSampleSlice" + juce::String (step)] = (float) step;
                }
                modeBox.setSelectedId (2, juce::dontSendNotification);
                editorViewBox.setSelectedId (1, juce::dontSendNotification);
            }
            else if (templateId == 6)
            {
                block->name = "Polymeter Key-Switch Banks";
                block->values["arpGate"] = 0.64f;
                block->values["arpSwing"] = 0.10f;
                block->values["mpPolymeterSteps"] = 5.0f;
                block->values["mpKeySwitchEnabled"] = 1.0f;
                block->values["mpKeySwitchBase"] = 24.0f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpMutation"] = 0.18f;
                block->values["mpEuclideanPulses"] = 3.0f;
                block->values["mpEuclideanRotate"] = 0.0f;
                static constexpr std::array<std::array<float, 16>, 4> bankNotes {{
                    {{ 0, 3, 7, 10, 12, 7, 3, 0, 5, 8, 12, 15, 17, 12, 8, 5 }},
                    {{ 0, 5, 7, 12, 14, 12, 7, 5, 3, 7, 10, 15, 19, 15, 10, 7 }},
                    {{ 0, 2, 5, 9, 12, 14, 9, 5, 7, 10, 14, 17, 21, 17, 14, 10 }},
                    {{ 0, -2, 3, 7, 10, 14, 17, 21, 19, 15, 12, 10, 7, 3, 0, -5 }}
                }};
                for (int bank = 0; bank < 4; ++bank)
                {
                    for (int step = 0; step < 16; ++step)
                    {
                        const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
                        const auto suffix = juce::String (step);
                        block->values[prefix + "arpNote" + suffix] = bankNotes[(size_t) bank][(size_t) step];
                        block->values[prefix + "mpStep" + suffix + "On"] = step < 5 ? 1.0f : 0.0f;
                        block->values[prefix + "mpVelocity" + suffix] = step == 0 ? 0.96f : 0.58f + 0.08f * (float) ((step + bank) % 4);
                        block->values[prefix + "mpGate" + suffix] = 0.42f + 0.08f * (float) ((step + bank) % 3);
                        block->values[prefix + "mpStepProb" + suffix] = step % 5 == 4 ? 0.72f : 1.0f;
                        block->values[prefix + "mpSampleSlice" + suffix] = -1.0f;
                    }
                }
                for (int step = 0; step < 16; ++step)
                    fillStep (step, bankNotes[0][(size_t) step], step < 5, step == 0 ? 0.96f : 0.70f, 0.50f);
                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 7)
            {
                block->name = "MIDI Echo Throw";
                block->values["arpGate"] = 0.96f;
                block->values["mpChordMode"] = 15.0f;
                block->values["mpChordSize"] = 4.0f;
                block->values["mpEchoRepeats"] = 3.0f;
                block->values["mpEchoDelay"] = 0.18f;
                block->values["mpEchoDecay"] = 0.52f;
                block->values["mpStrum"] = 0.08f;
                block->values["mpFlam"] = 0.12f;
                for (int step = 0; step < 16; ++step)
                    fillStep (step, (float) ((step / 4) * 3), step % 4 == 0, step == 0 ? 0.96f : 0.82f, 0.96f);
                modeBox.setSelectedId (1, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 8)
            {
                block->name = "Pattern Morph Performance";
                block->values["arpGate"] = 0.58f;
                block->values["mpPatternMorph"] = 0.50f;
                block->values["mpModLane"] = 0.0f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpMutation"] = 0.12f;
                static constexpr std::array<float, 16> a {{ 0, 4, 7, 12, 16, 12, 7, 4, 2, 5, 9, 14, 17, 14, 9, 5 }};
                static constexpr std::array<float, 16> b {{ 0, -5, 7, 10, 15, 19, 22, 15, 12, 7, 3, -2, 0, 10, 14, 19 }};
                for (int step = 0; step < 16; ++step)
                {
                    fillStep (step, a[(size_t) step], true, step % 4 == 0 ? 0.96f : 0.68f, step % 4 == 0 ? 0.72f : 0.38f);
                    const juce::String prefix = "mpBank2_";
                    const auto suffix = juce::String (step);
                    block->values[prefix + "arpNote" + suffix] = b[(size_t) step];
                    block->values[prefix + "mpStep" + suffix + "On"] = 1.0f;
                    block->values[prefix + "mpVelocity" + suffix] = step % 3 == 0 ? 0.94f : 0.58f;
                    block->values[prefix + "mpGate" + suffix] = step % 2 == 0 ? 0.48f : 0.76f;
                    block->values[prefix + "mpStepProb" + suffix] = step % 5 == 0 ? 0.78f : 1.0f;
                    block->values[prefix + "mpSampleSlice" + suffix] = -1.0f;
                }
                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (1, juce::dontSendNotification);
            }
            else if (templateId == 9)
            {
                block->name = "Chord Pad Performer";
                block->values["arpSteps"] = 4.0f;
                block->values["arpGate"] = 1.0f;
                block->values["mpChordMode"] = 23.0f;
                block->values["mpChordSize"] = 5.0f;
                block->values["mpChordSpread"] = 0.38f;
                block->values["mpHumanize"] = 0.04f;
                block->values["mpVelocityCurve"] = -0.18f;
                const float pads[] { 0.0f, 5.0f, 9.0f, 7.0f };
                for (int step = 0; step < 16; ++step)
                    fillStep (step, pads[step % 4], step < 4, 0.94f, 1.0f);
                modeBox.setSelectedId (1, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 10)
            {
                block->name = "Arp Riff Sequencer";
                block->values["arpGate"] = 0.46f;
                block->values["arpSwing"] = 0.16f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpMutation"] = 0.36f;
                block->values["mpRatchet"] = 2.0f;
                block->values["mpEuclideanPulses"] = 11.0f;
                block->values["mpEuclideanRotate"] = 2.0f;
                const float notes[] { 0, 3, 5, 7, 10, 12, 7, 5, 0, -2, 3, 7, 12, 15, 10, 7 };
                for (int step = 0; step < 16; ++step)
                    fillStep (step, notes[step], true, step % 4 == 0 ? 0.98f : 0.66f, step % 2 == 0 ? 0.40f : 0.62f,
                              step % 7 == 0 ? 0.82f : 1.0f);
                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 11)
            {
                block->name = "Advanced Arp Instrument";
                block->values["arpSteps"] = 64.0f;
                block->values["arpPattern"] = 0.0f;
                block->values["arpGate"] = 0.52f;
                block->values["arpSwing"] = 0.12f;
                block->values["arpOctaves"] = 3.0f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpChordSpread"] = 0.0f;
                block->values["mpProbability"] = 0.96f;
                block->values["mpHumanize"] = 0.08f;
                block->values["mpMutation"] = 0.06f;
                block->values["mpRatchet"] = 1.0f;
                block->values["mpVelocityCurve"] = -0.08f;
                block->values["mpEuclideanPulses"] = 0.0f;
                block->values["mpEuclideanRotate"] = 0.0f;
                block->values["mpPatternMorph"] = 0.0f;
                block->values["mpKeySwitchEnabled"] = 1.0f;
                block->values["mpKeySwitchBase"] = 24.0f;
                static constexpr std::array<float, 16> motif {{
                    0, 7, 12, 16, 19, 16, 12, 7, 3, 10, 15, 19, 22, 19, 15, 10
                }};

                for (int step = 0; step < 64; ++step)
                {
                    const int barStep = step % 16;
                    const auto suffix = juce::String (step);
                    const float note = motif[(size_t) barStep] + (step >= 32 ? 12.0f : 0.0f);
                    const bool downbeat = barStep % 4 == 0;
                    fillStep (step, note, true,
                              downbeat ? 0.98f : (barStep % 2 == 0 ? 0.78f : 0.60f),
                              downbeat ? 0.72f : 0.38f,
                              (barStep == 15 || barStep == 31) ? 0.78f : 1.0f);
                    block->values["mpStepDiv" + suffix] = (barStep == 6 || barStep == 14) ? 3.0f
                                                        : (barStep == 7 || barStep == 15) ? 2.0f : 1.0f;
                    block->values["mpStepTranspose" + suffix] = (step / 16 == 1) ? 5.0f
                                                            : (step / 16 == 2) ? 7.0f
                                                            : (step / 16 == 3) ? -2.0f : 0.0f;
                    block->values["mpStepTie" + suffix] = (barStep == 3 || barStep == 11) ? 1.0f : 0.0f;
                    block->values["mpStepChordMode" + suffix] = downbeat ? 15.0f : -1.0f;
                    block->values["mpStepChordSize" + suffix] = downbeat ? 3.0f : -1.0f;
                }

                for (int bank = 1; bank < 4; ++bank)
                {
                    const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
                    for (int step = 0; step < 64; ++step)
                    {
                        const auto suffix = juce::String (step);
                        const int barStep = step % 16;
                        const float bankOffset = bank == 1 ? -12.0f : (bank == 2 ? 5.0f : 7.0f);
                        block->values[prefix + "arpNote" + suffix] = motif[(size_t) ((barStep + bank * 3) % 16)] + bankOffset;
                        block->values[prefix + "mpStep" + suffix + "On"] = (barStep + bank) % 5 == 0 ? 0.0f : 1.0f;
                        block->values[prefix + "mpVelocity" + suffix] = barStep % 4 == 0 ? 0.96f : 0.55f + 0.08f * (float) ((barStep + bank) % 4);
                        block->values[prefix + "mpGate" + suffix] = barStep % 2 == 0 ? 0.42f : 0.62f;
                        block->values[prefix + "mpStepProb" + suffix] = barStep == 15 ? 0.72f : 1.0f;
                        block->values[prefix + "mpSampleSlice" + suffix] = -1.0f;
                        block->values[prefix + "mpStepDiv" + suffix] = (barStep + bank) % 8 == 0 ? 4.0f : ((barStep + bank) % 6 == 0 ? 3.0f : 1.0f);
                        block->values[prefix + "mpStepTranspose" + suffix] = (float) (bank * 2);
                        block->values[prefix + "mpStepChordMode" + suffix] = barStep % 8 == 0 ? 23.0f : -1.0f;
                        block->values[prefix + "mpStepChordSize" + suffix] = barStep % 8 == 0 ? 4.0f : -1.0f;
                        block->values[prefix + "mpStepTie" + suffix] = barStep == 3 ? 1.0f : 0.0f;
                    }
                }

                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else if (templateId == 12)
            {
                block->name = "CircleSEQ Flagship Instrument";
                block->values["arpSteps"] = 16.0f;
                block->values["arpPattern"] = 0.0f;
                block->values["arpGate"] = 0.58f;
                block->values["arpSwing"] = 0.10f;
                block->values["arpOctaves"] = 3.0f;
                block->values["mpActiveBank"] = 0.0f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpChordSpread"] = 0.0f;
                block->values["mpProbability"] = 0.97f;
                block->values["mpHumanize"] = 0.05f;
                block->values["mpMutation"] = 0.05f;
                block->values["mpRatchet"] = 1.0f;
                block->values["mpVelocityCurve"] = -0.06f;
                block->values["mpPatternMorph"] = 0.18f;
                block->values["mpKeySwitchEnabled"] = 1.0f;
                block->values["mpKeySwitchBase"] = 24.0f;

                static constexpr std::array<std::array<float, 16>, 5> laneNotes {{
                    {{ 0, 7, 12, 16, 19, 16, 12, 7, 3, 10, 15, 19, 22, 19, 15, 10 }},
                    {{ 0, 3, 7, 12, 15, 12, 7, 3, 5, 8, 12, 17, 20, 17, 12, 8 }},
                    {{ 0, 12, 7, 15, 10, 19, 14, 22, 17, 10, 15, 7, 12, 5, 10, 3 }},
                    {{ 0, 5, -2, 10, 3, 12, 7, 15, 10, 2, 14, 5, 17, 9, 21, 12 }},
                    {{ 0, 4, 7, 11, 14, 19, 23, 26, 24, 19, 16, 12, 9, 7, 4, 0 }}
                }};
                static constexpr std::array<std::array<int, 16>, 5> laneActive {{
                    {{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }},
                    {{ 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 }},
                    {{ 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 }},
                    {{ 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0 }},
                    {{ 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1 }}
                }};

                auto writeLaneStep = [&] (int lane, int step)
                {
                    const auto suffix = juce::String (step);
                    const auto prefix = lane == 0 ? juce::String()
                                                  : "mpBank" + juce::String (lane + 1) + "_";
                    const bool downbeat = step % 4 == 0;
                    const bool active = laneActive[(size_t) lane][(size_t) step] != 0;
                    const float velocity = downbeat ? 0.98f : 0.58f + 0.08f * (float) ((step + lane) % 5);
                    const float gate = lane == 1 ? 0.72f
                                     : lane == 2 ? 0.46f
                                     : lane == 3 ? 0.34f
                                                 : 0.58f;
                    block->values[prefix + "arpNote" + suffix] = laneNotes[(size_t) lane][(size_t) step];
                    block->values[prefix + "mpStep" + suffix + "On"] = active ? 1.0f : 0.0f;
                    block->values[prefix + "mpVelocity" + suffix] = juce::jlimit (0.0f, 1.0f, velocity);
                    block->values[prefix + "mpGate" + suffix] = juce::jlimit (0.05f, 1.0f, gate);
                    block->values[prefix + "mpStepProb" + suffix] = lane == 3 && step % 5 == 0 ? 0.76f : 1.0f;
                    block->values[prefix + "mpSampleSlice" + suffix] = -1.0f;
                    block->values[prefix + "mpStepDiv" + suffix] = (lane == 3 && (step == 6 || step == 14)) ? 4.0f
                                                        : (lane == 2 && step % 5 == 0) ? 3.0f
                                                        : (lane == 1 && step % 7 == 0) ? 2.0f
                                                        : 1.0f;
                    block->values[prefix + "mpStepTranspose" + suffix] = lane == 4 && step >= 8 ? 12.0f : 0.0f;
                    block->values[prefix + "mpStepChordMode" + suffix] = downbeat && (lane == 1 || lane == 4) ? 15.0f : -1.0f;
                    block->values[prefix + "mpStepChordSize" + suffix] = downbeat && (lane == 1 || lane == 4) ? 3.0f : -1.0f;
                    block->values[prefix + "mpStepTie" + suffix] = (lane == 4 && (step == 3 || step == 11)) ? 1.0f : 0.0f;
                };

                for (int lane = 0; lane < 5; ++lane)
                    for (int step = 0; step < 16; ++step)
                        writeLaneStep (lane, step);

                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }
            else
            {
                block->name = "Melodic Lead Line";
                block->values["arpGate"] = 0.58f;
                block->values["arpSwing"] = 0.06f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
                block->values["mpProbability"] = 1.0f;
                const float notes[] { 0.0f, 2.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 2.0f,
                                      0.0f, 5.0f, 7.0f, 9.0f, 12.0f, 9.0f, 7.0f, 4.0f };
                for (int step = 0; step < 16; ++step)
                    fillStep (step, notes[step], true, step % 4 == 0 ? 0.94f : 0.68f,
                              step % 4 == 0 ? 0.72f : 0.46f);
                modeBox.setSelectedId (3, juce::dontSendNotification);
                editorViewBox.setSelectedId (2, juce::dontSendNotification);
            }

            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (true);
            syncControlsFromBlock();
            pianoRollEditor.repaint();
            midiOutputLane.repaint();
            repaint();
        }
    }

    void MidiPlaygroundPage::applySelectedGuiTemplate()
    {
        const int templateId = guiTemplateBox.getSelectedId();
        auto& project = owner.getProject();

        auto parameterExists = [&] (const juce::String& parameterId)
        {
            return parameterId.isNotEmpty() && project.getParameters().find (parameterId) != nullptr;
        };

        project.performLayoutEdit ("Apply MIDI GUI template", [&] (LayoutModel& layout)
        {
            auto addElement = [&] (LayoutElement element, const juce::String& prefix)
            {
                element.id = layout.generateUniqueId (prefix);
                if (element.parameterId.isNotEmpty() && ! parameterExists (element.parameterId))
                    element.parameterId.clear();
                layout.add (element);
            };

            auto panel = [] (juce::String label, int x, int y, int w, int h)
            {
                LayoutElement element;
                element.type = ElementType::Panel;
                element.label = label;
                element.x = x; element.y = y; element.width = w; element.height = h;
                element.opacity = 0.84f;
                element.cornerRadius = 18.0f;
                element.strokeWidth = 1.4f;
                element.style = "Glass";
                return element;
            };

            auto knob = [] (juce::String label, juce::String parameterId, int x, int y)
            {
                LayoutElement element;
                element.type = ElementType::Knob;
                element.label = label;
                element.parameterId = parameterId;
                element.x = x; element.y = y; element.width = 82; element.height = 92;
                element.knobStyle = "Vintage 01";
                element.labelPosition = "bottom";
                return element;
            };

            auto slider = [] (juce::String label, juce::String parameterId, int x, int y)
            {
                LayoutElement element;
                element.type = ElementType::Slider;
                element.label = label;
                element.parameterId = parameterId;
                element.x = x; element.y = y; element.width = 32; element.height = 120;
                element.labelPosition = "bottom";
                return element;
            };

            auto miniKnob = [] (juce::String label, juce::String parameterId, int x, int y)
            {
                LayoutElement element;
                element.type = ElementType::Knob;
                element.label = label;
                element.parameterId = parameterId;
                element.x = x; element.y = y; element.width = 38; element.height = 44;
                element.knobStyle = "Vintage 01";
                element.labelPosition = "bottom";
                element.labelSize = 8.0f;
                return element;
            };

            auto button = [] (juce::String label, int x, int y, int w, int h)
            {
                LayoutElement element;
                element.type = ElementType::Button;
                element.label = label;
                element.x = x; element.y = y; element.width = w; element.height = h;
                element.cornerRadius = 10.0f;
                return element;
            };

            if (templateId == 7)
            {
                addElement (panel ("CircleSEQ Flagship", 24, 72, 1232, 682), "arp_studio_shell_");

                LayoutElement title;
                title.type = ElementType::Label;
                title.label = "CIRCLESEQ";
                title.x = 48; title.y = 90; title.width = 220; title.height = 32;
                title.labelSize = 24.0f;
                title.textColour = juce::Colour (0xfff2f6ff);
                addElement (title, "arp_studio_title_");

                LayoutElement subtitle;
                subtitle.type = ElementType::Label;
                subtitle.label = "Five playable circular phrase engines";
                subtitle.x = 50; subtitle.y = 122; subtitle.width = 310; subtitle.height = 20;
                subtitle.labelSize = 11.0f;
                subtitle.textColour = juce::Colour (0xff8e96a6);
                addElement (subtitle, "arp_studio_subtitle_");

                auto topButton = [&] (juce::String label, int x, int y, int w, juce::Colour accent)
                {
                    auto element = button (label, x, y, w, 34);
                    element.backgroundColour = juce::Colour (0x6610141a);
                    element.borderColour = accent.withAlpha (0.55f);
                    element.accentColour = accent;
                    element.labelPosition = "hidden";
                    return element;
                };

                addElement (topButton ("SYNC ON", 300, 92, 86, juce::Colour (0xff16d6e9)), "arp_studio_btn_");
                addElement (topButton ("POLY", 398, 92, 80, juce::Colour (0xff8f67ff)), "arp_studio_btn_");
                addElement (topButton ("HOST", 490, 92, 82, juce::Colour (0xff8f67ff)), "arp_studio_btn_");
                addElement (topButton ("HOLD", 988, 92, 80, juce::Colour (0xff8f67ff)), "arp_studio_btn_");
                addElement (topButton ("RND", 1080, 92, 74, juce::Colour (0xfff5a623)), "arp_studio_btn_");

                addElement (panel ("Pattern Transport", 596, 82, 320, 58), "arp_studio_transport_");
                LayoutElement bpm;
                bpm.type = ElementType::ValueDisplay;
                bpm.label = "120.00 BPM";
                bpm.parameterId = "projectBpm";
                bpm.x = 620; bpm.y = 94; bpm.width = 130; bpm.height = 32;
                bpm.textColour = juce::Colour (0xfff2f6ff);
                addElement (bpm, "arp_studio_bpm_");

                addElement (knob ("Swing", "bpmSync", 922, 82), "arp_studio_global_knob_");
                addElement (knob ("Groove", "lfoAmount", 1014, 82), "arp_studio_global_knob_");
                addElement (knob ("Mix", "volume", 1160, 76), "arp_studio_global_knob_");

                addElement (panel ("Scale + Key", 34, 154, 1180, 58), "arp_studio_scale_");
                const juce::StringArray scaleLabels { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                for (int i = 0; i < scaleLabels.size(); ++i)
                {
                    auto noteButton = topButton (scaleLabels[i], 360 + i * 54, 166, 48,
                                                 i == 9 ? juce::Colour (0xffa266ff) : juce::Colour (0xff334052));
                    noteButton.cornerRadius = 5.0f;
                    addElement (noteButton, "arp_studio_note_");
                }

                static const juce::Colour laneColours[] =
                {
                    juce::Colour (0xff16d6e9),
                    juce::Colour (0xff9b6dff),
                    juce::Colour (0xffff5bb9),
                    juce::Colour (0xffffa32b),
                    juce::Colour (0xff8df04b)
                };
                static const char* laneLabels[] = { "Spiral", "Ascent", "Bounce", "Random", "Rondo" };
                const int laneY = 226;
                const int laneW = 224;
                const int laneH = 332;
                for (int lane = 0; lane < 5; ++lane)
                {
                    LayoutElement arp;
                    arp.type = ElementType::ArpLane;
                    arp.label = laneLabels[lane];
                    arp.x = 34 + lane * 240;
                    arp.y = laneY;
                    arp.width = laneW;
                    arp.height = laneH;
                    arp.arpLaneIndex = lane;
                    arp.arpLaneSteps = 16;
                    arp.cornerRadius = 12.0f;
                    arp.strokeWidth = 1.4f;
                    arp.accentColour = laneColours[lane];
                    arp.borderColour = laneColours[lane].withAlpha (0.55f);
                    arp.backgroundColour = juce::Colour (0xdd10141a);
                    arp.labelPosition = "hidden";
                    addElement (arp, "arp_studio_lane_");
                }

                addElement (panel ("Global Mods", 34, 580, 214, 130), "arp_studio_mods_");
                addElement (knob ("Random", "lfoAmount", 58, 615), "arp_studio_mod_knob_");
                addElement (knob ("Gate", "lfoRate", 150, 615), "arp_studio_mod_knob_");

                addElement (panel ("Macro", 270, 580, 300, 130), "arp_studio_macro_");
                addElement (knob ("Cutoff", "filterCutoff", 294, 615), "arp_studio_macro_knob_");
                addElement (knob ("Reso", "filterResonance", 386, 615), "arp_studio_macro_knob_");
                addElement (knob ("Delay", "delayMix", 478, 615), "arp_studio_macro_knob_");

                LayoutElement xy;
                xy.type = ElementType::XYPad;
                xy.label = "XY Mutate";
                xy.x = 592; xy.y = 580; xy.width = 250; xy.height = 130;
                xy.cornerRadius = 10.0f;
                xy.accentColour = juce::Colour (0xff5ea2ff);
                xy.backgroundColour = juce::Colour (0xaa10141a);
                addElement (xy, "arp_studio_xy_");

                addElement (panel ("Direction", 862, 580, 194, 130), "arp_studio_direction_");
                addElement (topButton ("→", 888, 616, 46, juce::Colour (0xff80a6ff)), "arp_studio_dir_");
                addElement (topButton ("←", 942, 616, 46, juce::Colour (0xff80a6ff)), "arp_studio_dir_");
                addElement (topButton ("↑", 996, 616, 46, juce::Colour (0xff80a6ff)), "arp_studio_dir_");
                addElement (topButton ("RND", 910, 660, 112, juce::Colour (0xff80a6ff)), "arp_studio_dir_");

                addElement (panel ("Output", 1074, 580, 154, 130), "arp_studio_output_");
                LayoutElement meter;
                meter.type = ElementType::Meter;
                meter.label = "Output";
                meter.x = 1094; meter.y = 622; meter.width = 92; meter.height = 22;
                meter.accentColour = juce::Colour (0xff85e85f);
                addElement (meter, "arp_studio_meter_");
                addElement (knob ("Out", "volume", 1120, 644), "arp_studio_out_knob_");

                addElement (panel ("Group FX", 34, 718, 1194, 54), "arp_studio_group_fx_shell_");
                addElement (topButton ("LINK ON", 50, 730, 70, juce::Colour (0xff8f67ff)), "arp_studio_group_link_");
                static const char* fxGroupNames[] = { "G1 MUTE", "G2 DELAY", "G3 VERB", "G4 FILTER", "G5 GATE" };
                static const char* fxGroupParams[][3] =
                {
                    { "lfoAmount", "filterCutoff", "volume" },
                    { "delayTime", "delayFeedback", "delayMix" },
                    { "reverbMix", "stereoWidth", "outputGainDb" },
                    { "filterCutoff", "filterResonance", "eqMix" },
                    { "dynThresholdDb", "dynRatio", "dynMix" }
                };
                static const char* fxGroupLabels[][3] =
                {
                    { "Amt", "Tone", "Out" },
                    { "Time", "Feed", "Wet" },
                    { "Verb", "Wide", "Out" },
                    { "Cut", "Res", "Mix" },
                    { "Thrs", "Ratio", "Mix" }
                };
                for (int group = 0; group < 5; ++group)
                {
                    const int groupX = 140 + group * 188;
                    auto groupButton = topButton (fxGroupNames[group], groupX, 730, 78,
                                                  laneColours[group]);
                    groupButton.cornerRadius = 8.0f;
                    addElement (groupButton, "arp_studio_fx_group_btn_");
                    for (int control = 0; control < 3; ++control)
                        addElement (miniKnob (fxGroupLabels[group][control],
                                              fxGroupParams[group][control],
                                              groupX + 80 + control * 34,
                                              724),
                                    "arp_studio_fx_group_knob_");
                }
                addElement (panel ("FX Library", 1084, 724, 126, 42), "arp_studio_fx_library_");
            }
            else if (templateId == 2)
            {
                addElement (panel ("Sample Performance", 590, 600, 610, 150), "sample_gui_panel_");
                addElement (knob ("Glitch", "sampleGlitch", 620, 628), "sample_gui_knob_");
                addElement (knob ("Grid", "sampleGlitchGrid", 720, 628), "sample_gui_knob_");
                addElement (knob ("Start", "sampleStart", 820, 628), "sample_gui_knob_");
                addElement (knob ("Length", "sampleLength", 920, 628), "sample_gui_knob_");
                addElement (knob ("Slice", "sampleSlice", 1020, 628), "sample_gui_knob_");
                addElement (knob ("Pitch", "samplePitch", 1120, 628), "sample_gui_knob_");
            }
            else if (templateId == 3)
            {
                addElement (panel ("Drum Machine", 560, 500, 690, 280), "drum_gui_panel_");
                LayoutElement pattern;
                pattern.type = ElementType::DrumGrid;
                pattern.label = "Pattern";
                pattern.x = 585; pattern.y = 525; pattern.width = 610; pattern.height = 130;
                pattern.drumTracks = 8;
                pattern.drumSteps = 16;
                pattern.drumPattern = 0;
                pattern.cornerRadius = 10.0f;
                pattern.opacity = 0.95f;
                addElement (pattern, "drum_gui_pattern_grid_");
                LayoutElement pads;
                pads.type = ElementType::PadGrid;
                pads.label = "Drum Pads";
                pads.x = 585; pads.y = 670; pads.width = 300; pads.height = 90;
                pads.padRows = 2;
                pads.padCols = 8;
                pads.padBaseNote = 36;
                pads.cornerRadius = 12.0f;
                pads.opacity = 0.95f;
                addElement (pads, "drum_gui_pad_grid_");
                addElement (knob ("Glitch", "sampleGlitch", 935, 668), "drum_gui_knob_");
                addElement (knob ("Grid", "sampleGlitchGrid", 1035, 668), "drum_gui_knob_");
                addElement (knob ("Slice", "sampleSlice", 1135, 668), "drum_gui_knob_");
            }
            else if (templateId == 4)
            {
                addElement (panel ("MIDI Macro Panel", 640, 540, 540, 220), "midi_macro_panel_");
                addElement (knob ("Cutoff", "filterCutoff", 665, 620), "midi_macro_knob_");
                addElement (knob ("LFO Amt", "lfoAmount", 765, 620), "midi_macro_knob_");
                addElement (knob ("LFO Rate", "lfoRate", 865, 620), "midi_macro_knob_");
                addElement (knob ("WT Pos", "wtPosition", 965, 620), "midi_macro_knob_");
                LayoutElement xy;
                xy.type = ElementType::XYPad;
                xy.label = "XY Morph";
                xy.x = 1060; xy.y = 570; xy.width = 100; xy.height = 100;
                addElement (xy, "midi_macro_xy_");
            }
            else if (templateId == 5)
            {
                addElement (panel ("XY Performance", 620, 530, 500, 230), "xy_perf_panel_");
                LayoutElement xy;
                xy.type = ElementType::XYPad;
                xy.label = "Morph / Velocity";
                xy.x = 650; xy.y = 560; xy.width = 160; xy.height = 160;
                addElement (xy, "xy_perf_pad_");
                addElement (knob ("Morph", "macro_motion", 840, 585), "xy_perf_knob_");
                addElement (knob ("Glitch", "sampleGlitch", 940, 585), "xy_perf_knob_");
                addElement (knob ("Cutoff", "filterCutoff", 1040, 585), "xy_perf_knob_");
            }
            else if (templateId == 6)
            {
                addElement (panel ("Drum Song Controls", 590, 555, 600, 190), "drum_song_panel_");
                addElement (button ("Pattern 1", 625, 590, 100, 42), "drum_song_button_");
                addElement (button ("Pattern 2", 735, 590, 100, 42), "drum_song_button_");
                addElement (button ("Pattern 3", 845, 590, 100, 42), "drum_song_button_");
                addElement (button ("Pattern 4", 955, 590, 100, 42), "drum_song_button_");
                addElement (knob ("Swing", "bpmSync", 625, 650), "drum_song_knob_");
                addElement (knob ("Glitch", "sampleGlitch", 725, 650), "drum_song_knob_");
                addElement (knob ("Grid", "sampleGlitchGrid", 825, 650), "drum_song_knob_");
            }
            else
            {
                addElement (panel ("Performance Strip", 40, 600, 540, 150), "perf_gui_panel_");
                addElement (slider ("Expr", "expression", 70, 620), "perf_gui_slider_");
                addElement (slider ("Mod", "modWheel", 120, 620), "perf_gui_slider_");
                addElement (knob ("Cutoff", "filterCutoff", 190, 628), "perf_gui_knob_");
                addElement (knob ("Reverb", "reverbMix", 290, 628), "perf_gui_knob_");
                addElement (knob ("Delay", "delayMix", 390, 628), "perf_gui_knob_");
                addElement (knob ("Volume", "volume", 490, 628), "perf_gui_knob_");
            }
        });

        owner.setBottomTab (BottomPanel::Page::Design);
        owner.refreshAllPanels();
        owner.getProject().notifyChanged();
    }

    void MidiPlaygroundPage::exportMidiClip()
    {
        const auto* block = activeMidiBlock();
        if (block == nullptr)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Export MIDI")
                    .withMessage ("Add or select a Performance Builder block before exporting a clip.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::InfoIcon),
                nullptr);
            return;
        }

        const auto blockId = block->id;
        auto safeName = block->name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
        if (safeName.isEmpty())
            safeName = "PatchCraft MIDI Clip";

        exportChooser = std::make_unique<juce::FileChooser> (
            "Export MIDI Clip",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (safeName + ".mid"),
            "*.mid;*.midi");

        exportChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, blockId] (const juce::FileChooser& chooser)
            {
                auto target = chooser.getResult();
                if (target == juce::File())
                    return;

                const DspBlock* selectedBlock = nullptr;
                for (const auto& candidate : owner.getProject().getDspGraph().blocks)
                    if (candidate.id == blockId)
                    {
                        selectedBlock = &candidate;
                        break;
                    }

                if (selectedBlock == nullptr)
                    return;

                juce::String error;
                if (! MidiPlaygroundPattern::writeMidiClip (*selectedBlock, target, 120.0, 60, error))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("MIDI Export Failed")
                            .withMessage (error)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("MIDI Exported")
                        .withMessage ("Wrote MIDI clip:\n" + target.getFullPathName())
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::InfoIcon),
                    nullptr);
            });
    }

    void MidiPlaygroundPage::startPatternPreview()
    {
        auto* block = ensureMidiBlock();
        if (block == nullptr)
            return;

        if (modeBox.getSelectedId() == 5 && ! isDrumMachineBlock (*block))
        {
            configureDrumMachine();
            block = activeMidiBlock();
            if (block == nullptr)
                return;
        }

        stopPatternPreview();

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            activeSummary.setText ("Pattern preview unavailable: " + error, juce::dontSendNotification);
            return;
        }

        auto engine = std::make_unique<SampleSynthEngine>();
        engine->prepare (previewSampleRate, previewBlockSize, previewChannels);
        engine->setRenderContext (RenderContext::forBlock (previewSampleRate,
                                                           previewBlockSize,
                                                           previewBlockSize,
                                                           0,
                                                           previewChannels,
                                                           120.0));
        engine->loadFromPack (owner.getProject().getProjectFolder(),
                              owner.getProject().getSampleMap());

        previewRuntime.bind (owner.getProject().getDspGraph());
        previewRuntime.reset();

        if (! isDrumMachineBlock (*block))
        {
            previewHeldNote = juce::jlimit (0, 127, 60 + juce::jmax (0, rootBox.getSelectedId() - 1));
            previewRuntime.handleNoteOn (*engine, previewHeldNote, 0.9f);
        }

        {
            const juce::SpinLock::ScopedLockType lock (previewLock);
            previewEngine = std::move (engine);
        }

        owner.getAudio().getDeviceManager().addAudioCallback (this);
        patternPreviewActive = true;
        arpStudioPreviewStartMs = juce::Time::getMillisecondCounterHiRes();
        playPatternButton.setButtonText ("Playing");
        activeSummary.setText (blockSummary() + "  |  preview playing on this page", juce::dontSendNotification);
        startTimerHz (30);
        repaint();
    }

    void MidiPlaygroundPage::stopPatternPreview()
    {
        if (patternPreviewActive)
            owner.getAudio().getDeviceManager().removeAudioCallback (this);

        patternPreviewActive = false;
        arpStudioPreviewStartMs = 0.0;
        {
            const juce::SpinLock::ScopedLockType lock (previewLock);
            if (previewEngine != nullptr)
            {
                previewRuntime.allNotesOff (*previewEngine);
                previewEngine->allNotesOff();
                previewEngine->reset();
            }
            previewEngine.reset();
        }
        previewRuntime.reset();
        playPatternButton.setButtonText ("Play Pattern");
        if (! pendingGraphNotification)
            stopTimer();
        repaint();
    }

    void MidiPlaygroundPage::setArpStudioHardwarePreviewActive (bool active)
    {
        if (arpStudioHardwareMidiActive == active)
            return;

        auto& deviceManager = owner.getAudio().getDeviceManager();
        if (active)
        {
            for (const auto& input : juce::MidiInput::getAvailableDevices())
                deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

            deviceManager.addMidiInputDeviceCallback ({}, this);
        }
        else
        {
            deviceManager.removeMidiInputDeviceCallback ({}, this);
        }

        arpStudioHardwareMidiActive = active;
    }

    void MidiPlaygroundPage::triggerArpStudioPreviewNote (int note, float velocity, bool noteOn)
    {
        if (! arpStudioMode || ! isShowing())
            return;

        if (! patternPreviewActive)
            startPatternPreview();

        const juce::SpinLock::ScopedLockType lock (previewLock);
        if (previewEngine == nullptr)
            return;

        if (noteOn)
        {
            if (previewHeldNote >= 0 && previewHeldNote != note)
                previewRuntime.handleNoteOff (*previewEngine, previewHeldNote);

            previewHeldNote = juce::jlimit (0, 127, note);
            previewRuntime.handleNoteOn (*previewEngine, previewHeldNote, juce::jlimit (0.0f, 1.0f, velocity));
            arpStudioPreviewStartMs = juce::Time::getMillisecondCounterHiRes();
            activeSummary.setText (blockSummary() + "  |  hardware key preview: "
                                   + juce::MidiMessage::getMidiNoteName (previewHeldNote, true, true, 3),
                                   juce::dontSendNotification);
        }
        else if (note == previewHeldNote)
        {
            previewRuntime.handleNoteOff (*previewEngine, note);
        }

        repaint();
    }

    void MidiPlaygroundPage::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        if (! arpStudioMode || (! message.isNoteOn() && ! message.isNoteOff()))
            return;

        juce::Component::SafePointer<MidiPlaygroundPage> safeThis (this);
        const int note = message.getNoteNumber();
        const float velocity = message.getFloatVelocity();
        const bool noteOn = message.isNoteOn();
        juce::MessageManager::callAsync ([safeThis, note, velocity, noteOn]
        {
            if (safeThis != nullptr)
                safeThis->triggerArpStudioPreviewNote (note, velocity, noteOn);
        });
    }

    void MidiPlaygroundPage::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        previewSampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
        previewBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
        previewChannels = device != nullptr
            ? juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits())
            : 2;

        const juce::SpinLock::ScopedLockType lock (previewLock);
        if (previewEngine != nullptr)
        {
            previewEngine->prepare (previewSampleRate, previewBlockSize, previewChannels);
            previewEngine->setRenderContext (RenderContext::forBlock (previewSampleRate,
                                                                      previewBlockSize,
                                                                      previewBlockSize,
                                                                      0,
                                                                      previewChannels,
                                                                      120.0));
        }
    }

    void MidiPlaygroundPage::audioDeviceStopped()
    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        if (previewEngine != nullptr)
        {
            previewRuntime.allNotesOff (*previewEngine);
            previewEngine->reset();
        }
    }

    void MidiPlaygroundPage::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                               float* const* outputChannelData,
                                                               int numOutputChannels,
                                                               int numSamples,
                                                               const juce::AudioIODeviceCallbackContext&)
    {
        juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
        output.clear();

        const juce::SpinLock::ScopedTryLockType lock (previewLock);
        if (! lock.isLocked() || previewEngine == nullptr || ! patternPreviewActive)
            return;

        auto context = RenderContext::forBlock (previewSampleRate,
                                                numSamples,
                                                previewBlockSize,
                                                0,
                                                numOutputChannels,
                                                120.0);
        context.isPlaying = true;
        previewEngine->setRenderContext (context);
        previewRuntime.process (*previewEngine, context);
        previewEngine->process (output, 0, numSamples);
    }

    void MidiPlaygroundPage::syncControlsFromBlock()
    {
        syncingControls = true;

        if (const auto* block = activeMidiBlock())
        {
            sourceBox.setSelectedId (owner.getProject().getEngineType() == "sample" ? 2
                                    : owner.getProject().getEngineType() == "fx" ? 3 : 1,
                                    juce::dontSendNotification);
            activeSummary.setText (blockSummary(), juce::dontSendNotification);
            if (isDrumMachineBlock (*block))
            {
                stepsSlider.setRange (1.0, 64.0, 1.0);
                sliceCountSlider.setRange (1.0, 16.0, 1.0);
                modeBox.setSelectedId (5, juce::dontSendNotification);
                drumPatternBox.setSelectedId (juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f))) + 1,
                                              juce::dontSendNotification);
                stepsSlider.setValue (valueFor (*block, "dmSteps", 16.0f), juce::dontSendNotification);
                sliceCountSlider.setValue (valueFor (*block, "dmTracks", 8.0f), juce::dontSendNotification);
                rateSlider.setValue (valueFor (*block, "rate", 1.0f), juce::dontSendNotification);
                gateSlider.setValue (0.36f, juce::dontSendNotification);
                swingSlider.setValue (valueFor (*block, "dmSwing", 0.0f), juce::dontSendNotification);
                probabilitySlider.setValue (valueFor (*block, "dmProbability", 1.0f), juce::dontSendNotification);
            }
            else
            {
                stepsSlider.setRange (1.0, (double) kAdvancedMidiMaxSteps, 1.0);
                sliceCountSlider.setRange (1.0, 32.0, 1.0);
                phraseBankBox.setSelectedId (MidiPlaygroundPattern::getActiveBank (*block) + 1, juce::dontSendNotification);
                progressionBox.setSelectedId (juce::roundToInt (valueFor (*block, "mpProgressionPreset", 0.0f)) + 1,
                                              juce::dontSendNotification);
                stepsSlider.setValue (valueFor (*block, "arpSteps", 8.0f), juce::dontSendNotification);
                rateSlider.setValue (valueFor (*block, "rate", 1.0f), juce::dontSendNotification);
                gateSlider.setValue (valueFor (*block, "arpGate", 0.55f), juce::dontSendNotification);
                swingSlider.setValue (valueFor (*block, "arpSwing", 0.0f), juce::dontSendNotification);
                probabilitySlider.setValue (valueFor (*block, "mpProbability", 1.0f), juce::dontSendNotification);
                humanizeSlider.setValue (valueFor (*block, "mpHumanize", 0.0f), juce::dontSendNotification);
                sliceCountSlider.setValue (valueFor (*block, "mpSampleSliceCount", valueFor (*block, "sampleSliceCount", 16.0f)),
                                           juce::dontSendNotification);
                chordSizeSlider.setValue (valueFor (*block, "mpChordSize", 1.0f), juce::dontSendNotification);
                chordSpreadSlider.setValue (valueFor (*block, "mpChordSpread", 0.0f), juce::dontSendNotification);
                octaveSlider.setValue (valueFor (*block, "arpOctaves", 2.0f), juce::dontSendNotification);
                mutationSlider.setValue (valueFor (*block, "mpMutation", 0.0f), juce::dontSendNotification);
                ratchetSlider.setValue (valueFor (*block, "mpRatchet", 1.0f), juce::dontSendNotification);
                velocityCurveSlider.setValue (valueFor (*block, "mpVelocityCurve", 0.0f), juce::dontSendNotification);
                strumSlider.setValue (valueFor (*block, "mpStrum", 0.0f), juce::dontSendNotification);
                flamSlider.setValue (valueFor (*block, "mpFlam", 0.0f), juce::dontSendNotification);
                euclideanPulsesSlider.setValue (valueFor (*block, "mpEuclideanPulses", 0.0f), juce::dontSendNotification);
                euclideanRotateSlider.setValue (valueFor (*block, "mpEuclideanRotate", 0.0f), juce::dontSendNotification);
                octaveFoldToggle.setToggleState (valueFor (*block, "mpOctaveFold", 0.0f) >= 0.5f, juce::dontSendNotification);
                const int chordPreset = juce::roundToInt (valueFor (*block, "mpChordPreset", 0.0f));
                chordPresetBox.setSelectedId (chordPreset >= 1 && chordPreset <= 31 ? chordPreset : 0,
                                              juce::dontSendNotification);
                rootBox.setSelectedId (juce::roundToInt (valueFor (*block, "mpScaleRoot", 0.0f)) + 1, juce::dontSendNotification);
                scaleBox.setSelectedId (juce::roundToInt (valueFor (*block, "mpScaleType", 1.0f)) + 1, juce::dontSendNotification);

                if (valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f)
                    modeBox.setSelectedId (2, juce::dontSendNotification);
                else if (valueFor (*block, "mpModLane", 0.0f) >= 0.5f)
                    modeBox.setSelectedId (4, juce::dontSendNotification);
                else if (valueFor (*block, "arpPattern", 0.0f) >= 7.0f)
                    modeBox.setSelectedId (3, juce::dontSendNotification);
                else
                    modeBox.setSelectedId (1, juce::dontSendNotification);

                if (block->targetId == "volume") targetBox.setSelectedId (2, juce::dontSendNotification);
                else if (block->targetId == "sampleSlice") targetBox.setSelectedId (3, juce::dontSendNotification);
                else if (block->targetId == "sampleStart") targetBox.setSelectedId (4, juce::dontSendNotification);
                else if (block->targetId == "sampleLength") targetBox.setSelectedId (5, juce::dontSendNotification);
                else if (block->targetId == "wtPosition") targetBox.setSelectedId (6, juce::dontSendNotification);
                else targetBox.setSelectedId (1, juce::dontSendNotification);
            }
        }
        else
        {
            activeSummary.setText ("No Pattern Player yet. Add one, choose a musical idea, then test the current Patch in Player.",
                                   juce::dontSendNotification);
        }

        syncingControls = false;
    }

    void MidiPlaygroundPage::updateBlockFromControls()
    {
        if (syncingControls)
            return;

        if (auto* block = ensureMidiBlock())
        {
            if (modeBox.getSelectedId() == 5 || isDrumMachineBlock (*block))
            {
                if (! isDrumMachineBlock (*block))
                {
                    configureDrumMachine();
                    return;
                }

                block->values["rate"] = (float) rateSlider.getValue();
                block->values["sync"] = 1.0f;
                block->values["dmSteps"] = (float) juce::jlimit (1, 64, juce::roundToInt ((float) stepsSlider.getValue()));
                block->values["dmTracks"] = (float) juce::jlimit (1, 16, juce::roundToInt ((float) sliceCountSlider.getValue()));
                block->values["dmPattern"] = (float) juce::jlimit (0, 7, drumPatternBox.getSelectedId() - 1);
                block->values["dmSwing"] = (float) swingSlider.getValue();
                block->values["dmProbability"] = (float) probabilitySlider.getValue();
                block->values["dmTransport"] = 1.0f;
                block->values["mpSampleControl"] = 1.0f;
                block->name = "Drum Machine Performance";
                block->targetId = "sample";
                notifyGraphChanged (false);
                activeSummary.setText (blockSummary(), juce::dontSendNotification);
                drumPatternGrid.repaint();
                repaint();
                return;
            }

            block->values["arpSteps"] = (float) stepsSlider.getValue();
            block->values["rate"] = (float) rateSlider.getValue();
            block->values["sync"] = 1.0f;
            block->values["arpGate"] = (float) gateSlider.getValue();
            block->values["arpSwing"] = (float) swingSlider.getValue();
            block->values["mpProbability"] = (float) probabilitySlider.getValue();
            block->values["mpHumanize"] = (float) humanizeSlider.getValue();
            block->values["mpChordSize"] = (float) chordSizeSlider.getValue();
            block->values["mpChordSpread"] = (float) chordSpreadSlider.getValue();
            block->values["arpOctaves"] = (float) octaveSlider.getValue();
            block->values["mpMutation"] = (float) mutationSlider.getValue();
            block->values["mpRatchet"] = (float) ratchetSlider.getValue();
            block->values["mpVelocityCurve"] = (float) velocityCurveSlider.getValue();
            block->values["mpStrum"] = (float) strumSlider.getValue();
            block->values["mpFlam"] = (float) flamSlider.getValue();
            block->values["mpEuclideanPulses"] = (float) euclideanPulsesSlider.getValue();
            block->values["mpEuclideanRotate"] = (float) euclideanRotateSlider.getValue();
            block->values["mpOctaveFold"] = octaveFoldToggle.getToggleState() ? 1.0f : 0.0f;
            block->values["mpScaleRoot"] = (float) juce::jmax (0, rootBox.getSelectedId() - 1);
            block->values["mpScaleType"] = (float) juce::jmax (0, scaleBox.getSelectedId() - 1);
            block->values["mpSampleSliceCount"] = (float) sliceCountSlider.getValue();
            block->values["sampleSliceCount"] = (float) sliceCountSlider.getValue();

            switch (targetBox.getSelectedId())
            {
                case 2: block->targetId = "volume"; break;
                case 3: block->targetId = "sampleSlice"; break;
                case 4: block->targetId = "sampleStart"; break;
                case 5: block->targetId = "sampleLength"; break;
                case 6: block->targetId = "wtPosition"; break;
                default: block->targetId = "filterCutoff"; break;
            }

            if (modeBox.getSelectedId() == 2)
            {
                block->name = "Sample Chopper Performance";
                block->targetId = "sampleSlice";
                block->values["mpSampleControl"] = 1.0f;
                block->values["mpModLane"] = 0.0f;
            }
            else if (modeBox.getSelectedId() == 4)
            {
                block->name = "Performance Modulation Lane";
                block->values["mpSampleControl"] = 0.0f;
                block->values["mpModLane"] = 1.0f;
                block->values["mpChordMode"] = 0.0f;
                block->values["mpChordSize"] = 1.0f;
            }
            else
            {
                block->name = modeBox.getSelectedId() == 3 ? "Arp Step Sequencer" : "Chord Progression Performance";
                block->values["mpSampleControl"] = 0.0f;
                block->values["mpModLane"] = 0.0f;
                if (modeBox.getSelectedId() == 3)
                    block->values["arpPattern"] = 7.0f;
            }

            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (false);
            activeSummary.setText (blockSummary(), juce::dontSendNotification);
            midiOutputLane.repaint();
            repaint();
        }
    }

    void MidiPlaygroundPage::notifyGraphChanged (bool immediate)
    {
        owner.getProject().getDspGraph().userConfigured = true;
        owner.getProject().markDirty();
        if (patternPreviewActive)
        {
            const juce::SpinLock::ScopedLockType lock (previewLock);
            if (previewEngine != nullptr)
                previewRuntime.bind (owner.getProject().getDspGraph());
        }

        if (immediate)
        {
            if (! patternPreviewActive)
                stopTimer();
            pendingGraphNotification = false;
            owner.getProject().notifyChanged();
            return;
        }

        pendingGraphNotification = true;
        if (! isTimerRunning())
            startTimer (patternPreviewActive ? 33 : 75);
    }

    void MidiPlaygroundPage::timerCallback()
    {
        if (pendingGraphNotification)
        {
            pendingGraphNotification = false;
            owner.getProject().notifyChanged();
        }

        if (patternPreviewActive)
        {
            repaint();
            return;
        }

        stopTimer();
    }

    void MidiPlaygroundPage::setStepValueFromEditor (int step, int noteOffset, float velocity, float gate,
                                                     float probability, bool active, bool editNote,
                                                     bool editVelocity, bool editGate, bool editProbability)
    {
        if (auto* block = ensureMidiBlock())
        {
            step = juce::jlimit (0, kAdvancedMidiMaxSteps - 1, step);
            const auto suffix = juce::String (step);

            if (editNote)
            {
                if (valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f
                    && juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())
                {
                    const int slices = juce::jmax (1, juce::roundToInt (valueFor (*block, "mpSampleSliceCount", 16.0f)));
                    block->values["mpSampleSlice" + suffix] = (float) juce::jlimit (0, slices - 1, noteOffset);
                }
                else
                {
                    block->values["arpNote" + suffix] = (float) roundStepNote ((float) noteOffset);
                }
            }

            if (editVelocity)
                block->values["mpVelocity" + suffix] = juce::jlimit (0.0f, 1.0f, velocity);

            if (editGate)
                block->values["mpGate" + suffix] = juce::jlimit (0.05f, 1.0f, gate);

            if (editProbability)
                block->values["mpStepProb" + suffix] = juce::jlimit (0.0f, 1.0f, probability);

            block->values["mpStep" + suffix + "On"] = active ? 1.0f : 0.0f;
            MidiPlaygroundPattern::storeActiveBank (*block, MidiPlaygroundPattern::getActiveBank (*block));
            notifyGraphChanged (false);
            activeSummary.setText (blockSummary(), juce::dontSendNotification);
            midiOutputLane.repaint();
            pianoRollEditor.repaint();
        }
    }

    void MidiPlaygroundPage::setDrumStepFromEditor (int track, int step, bool active, float velocity,
                                                    bool editVelocity, int divisions)
    {
        auto* block = activeMidiBlock();
        if (block == nullptr || ! isDrumMachineBlock (*block))
        {
            configureDrumMachine();
            block = activeMidiBlock();
        }

        if (block == nullptr)
            return;

        track = juce::jlimit (0, 15, track);
        step = juce::jlimit (0, 63, step);
        const auto prefix = drumPrefix (*block, track, step);
        block->values[prefix + "On"] = active ? 1.0f : 0.0f;
        if (editVelocity)
            block->values[prefix + "Vel"] = juce::jlimit (0.0f, 1.0f, velocity);
        else if (block->values.find (prefix + "Vel") == block->values.end())
            block->values[prefix + "Vel"] = track == 0 ? 1.0f : 0.75f;

        if (block->values.find (prefix + "Gate") == block->values.end())
            block->values[prefix + "Gate"] = track == 2 ? 0.16f : 0.36f;
        if (block->values.find (prefix + "Prob") == block->values.end())
            block->values[prefix + "Prob"] = 1.0f;
        if (divisions > 0)
            block->values[prefix + "Div"] = (float) juce::jlimit (1, 4, divisions);
        else if (block->values.find (prefix + "Div") == block->values.end())
            block->values[prefix + "Div"] = 1.0f;

        notifyGraphChanged (false);
        activeSummary.setText (blockSummary(), juce::dontSendNotification);
        drumPatternGrid.repaint();
    }

    bool MidiPlaygroundPage::isDrumMachineBlock (const DspBlock& block) const
    {
        return block.type.containsIgnoreCase ("drum")
            || block.values.find ("dmTracks") != block.values.end()
            || block.values.find ("dmSteps") != block.values.end();
    }

    juce::String MidiPlaygroundPage::blockSummary() const
    {
        if (const auto* block = activeMidiBlock())
        {
            if (isDrumMachineBlock (*block))
            {
                const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
                return block->name + "  |  pattern " + juce::String (pattern + 1)
                     + "  |  " + juce::String (juce::roundToInt (valueFor (*block, "dmTracks", 8.0f))) + " tracks"
                     + "  |  " + juce::String (juce::roundToInt (valueFor (*block, "dmSteps", 16.0f))) + " steps"
                     + "  |  " + (valueFor (*block, "dmSongMode", 0.0f) >= 0.5f ? "song chain ON" : "single pattern")
                     + "  |  transport sync"
                     + "  |  sampler pads / one-shots";
            }

            const auto progressionNames = MidiPlaygroundPattern::getProgressionNames();
            const auto progressionIndex = juce::jlimit (0, progressionNames.size() - 1,
                                                        juce::roundToInt (valueFor (*block, "mpProgressionPreset", 0.0f)));
            return block->name + "  |  "
                 + "slot " + juce::String (MidiPlaygroundPattern::getActiveBank (*block) + 1)
                 + "  |  progression " + progressionNames[progressionIndex]
                 + "  |  "
                 + juce::String (juce::roundToInt (valueFor (*block, "arpSteps", 8.0f))) + " steps"
                 + "  |  target: " + (block->targetId.isNotEmpty() ? block->targetId : "notes")
                  + "  |  sample: " + (valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f ? "ON" : "OFF")
                  + "  |  poly " + (valueFor (*block, "mpPolymeterSteps", 0.0f) > 0.0f
                                      ? juce::String (juce::roundToInt (valueFor (*block, "mpPolymeterSteps", 0.0f)))
                                      : juce::String ("off"))
                  + "  |  ratchet x" + juce::String (juce::roundToInt (valueFor (*block, "mpRatchet", 1.0f)))
                  + "  |  echo x" + juce::String (juce::roundToInt (valueFor (*block, "mpEchoRepeats", 0.0f)))
                  + "  |  strum " + juce::String (juce::roundToInt (valueFor (*block, "mpStrum", 0.0f) * 100.0f)) + "%"
                  + "  |  mutation " + juce::String (juce::roundToInt (valueFor (*block, "mpMutation", 0.0f) * 100.0f)) + "%";
        }

        return "No pattern player yet.";
    }

    juce::String MidiPlaygroundPage::performanceRouteSummary() const
    {
        const auto engine = owner.getProject().getEngineType();
        const auto source = engine == "sample" ? juce::String ("Sample Mapper")
                         : engine == "fx"     ? juce::String ("FX Input")
                                              : juce::String ("DSP Synth Source");
        const auto* block = activeMidiBlock();
        if (block == nullptr)
            return "Input MIDI -> no pattern player yet -> " + source + " -> DSP/FX -> Player output";

        const bool drum = isDrumMachineBlock (*block);
        const bool sample = valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f;
        const auto bank = drum ? juce::String ("Pattern " + juce::String (juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)) + 1))
                               : juce::String ("Slot " + juce::String (MidiPlaygroundPattern::getActiveBank (*block) + 1));
        const auto target = drum ? juce::String ("Sampler drum pads")
                         : sample ? juce::String ("Sample slices/start/length")
                                  : juce::String ("Notes + " + (block->targetId.isNotEmpty() ? block->targetId : juce::String ("parameter target")));

        return "Input MIDI / transport -> Pattern Player " + bank + " -> " + target + " -> " + source + " -> DSP/FX -> Player output";
    }

    juce::Rectangle<int> MidiPlaygroundPage::drawControl (juce::Graphics& g, juce::Rectangle<int> row,
                                                          const juce::String& label, juce::Component& component)
    {
        auto labelArea = row.removeFromLeft (116);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText (label, labelArea, juce::Justification::centredLeft);
        component.setBounds (row);
        return row;
    }

    void MidiPlaygroundPage::drawSectionCards (juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Single status strip in place of the 6-tile section grid. The cards
        // were navigation labels that didn't drive anything; this returns
        // the vertical real estate to the editor below.
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.30f));
        g.drawRoundedRectangle (area.toFloat(), 6.0f, 1.0f);

        const auto* block = activeMidiBlock();
        const juce::String scale = block != nullptr
            ? rootBox.getText() + " " + scaleBox.getText()
            : juce::String ("(no pattern player)");
        const int steps = block != nullptr ? midiStepCount (*block) : 0;
        const auto view = arpStudioMode ? juce::String ("Circle View") : juce::String ("Pattern Builder");

        auto inner = area.reduced (12, 0);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        const auto pieces = juce::StringArray {
            "View: " + view,
            "Scale: " + scale,
            "Steps: " + juce::String (steps),
            performanceRouteSummary()
        };
        const int sectionW = inner.getWidth() / juce::jmax (1, pieces.size());
        for (int i = 0; i < pieces.size(); ++i)
        {
            const auto cell = inner.removeFromLeft (sectionW);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawText (pieces[i], cell, juce::Justification::centredLeft);
        }
    }

    juce::Rectangle<int> MidiPlaygroundPage::sectionCardBounds (juce::Rectangle<int> area, int index) const
    {
        // Legacy hit-test: the 6-card grid is gone, so always return an
        // empty rect. mouseDown still calls this for back-compat.
        juce::ignoreUnused (area, index);
        return {};
    }

    juce::String MidiPlaygroundPage::sectionCardName (int index) const
    {
        static const juce::StringArray names { "Input", "Harmony", "Rhythm", "Performance", "Sample Control", "Output" };
        return names[juce::jlimit (0, names.size() - 1, index)];
    }

    juce::String MidiPlaygroundPage::sectionCardDescription (int index) const
    {
        static const juce::StringArray descriptions {
            "Hardware MIDI, software keyboard, latch, key switches, and future MIDI input filters.",
            "Scale root/type, chord size, spread, phrase notes, and progression tools.",
            "Step count, rate, gate, swing, probability, velocity, humanize, and musical transforms.",
            "Mod wheel, aftertouch, expression, XY motion, macros, and live performance switches.",
            "Per-step slice, sample start, length, pitch, retrigger, chopping, and glitch workflows.",
            "Routes generated notes and control data into Synth, Sampler, Player, DSP, and Test."
        };
        return descriptions[juce::jlimit (0, descriptions.size() - 1, index)];
    }

    juce::Rectangle<int> MidiPlaygroundPage::PianoRollEditor::rollArea() const
    {
        return getLocalBounds().reduced (12, kRollHeaderHeight).withTrimmedBottom (10);
    }

    int MidiPlaygroundPage::PianoRollEditor::rowHeight (juce::Rectangle<int> area) const
    {
        return juce::jmax (4, area.getHeight() / kRollRows);
    }

    int MidiPlaygroundPage::PianoRollEditor::stepAt (int x) const
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr)
            return -1;

        auto area = rollArea();
        area.removeFromLeft (kRollKeyWidth);
        const int steps = midiStepCount (*block);
        const int gap = steps > 64 ? 1 : 2;
        const int cellW = juce::jmax (steps > 64 ? 3 : 8,
                                      (area.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
        const int relative = x - area.getX();
        if (relative < 0)
            return -1;

        const int step = relative / (cellW + gap);
        return step >= 0 && step < steps ? step : -1;
    }

    int MidiPlaygroundPage::PianoRollEditor::noteOffsetAt (int y) const
    {
        auto area = rollArea();
        if (y < area.getY() || y >= area.getBottom())
            return 0;

        const int rH = rowHeight (area);
        const int row = juce::jlimit (0, kRollRows - 1, (y - area.getY()) / juce::jmax (1, rH));
        // Row 0 is the topmost = highest pitch (+36 semitones); we centre at 0.
        return (kRollRows / 2) - row;
    }

    float MidiPlaygroundPage::PianoRollEditor::gateAt (int x, int step) const
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr)
            return 0.58f;

        auto area = rollArea();
        area.removeFromLeft (kRollKeyWidth);
        const int steps = midiStepCount (*block);
        const int gap = steps > 64 ? 1 : 2;
        const int cellW = juce::jmax (steps > 64 ? 3 : 8,
                                      (area.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
        const int stepX = area.getX() + juce::jlimit (0, steps - 1, step) * (cellW + gap);
        const float position = juce::jlimit (0.0f, 1.0f, (float) (x - stepX) / (float) juce::jmax (1, cellW));
        return juce::jlimit (0.08f, 1.0f, 0.10f + position * 0.90f);
    }

    int MidiPlaygroundPage::PianoRollEditor::keyPreviewNoteAt (juce::Point<int> pos) const
    {
        // Hit-test the piano-key column on the left of the roll, returns the
        // MIDI note for the row at this Y, or -1 if outside.
        auto area = rollArea();
        const auto keyArea = area.removeFromLeft (kRollKeyWidth);
        if (! keyArea.contains (pos)) return -1;
        const int rH = rowHeight (keyArea);
        const int row = juce::jlimit (0, kRollRows - 1, (pos.y - keyArea.getY()) / juce::jmax (1, rH));
        const int rootMidi = 60 + juce::jmax (0, owner.rootBox.getSelectedId() - 1);
        const int offset = (kRollRows / 2) - row;
        return juce::jlimit (0, 127, rootMidi + offset);
    }

    void MidiPlaygroundPage::PianoRollEditor::editFromMouse (const juce::MouseEvent& e)
    {
        auto* block = owner.ensureMidiBlock();
        if (block == nullptr || owner.isDrumMachineBlock (*block))
            return;

        const int step = stepAt (e.x);
        if (step < 0)
            return;

        const auto suffix = juce::String (step);
        const int noteOffset = juce::jlimit (-24, 24, noteOffsetAt (e.y));
        const float velocity = valueFor (*block, "mpVelocity" + suffix, 0.85f);
        const float gate = e.mods.isShiftDown() ? gateAt (e.x, step)
                                                : valueFor (*block, "mpGate" + suffix, valueFor (*block, "arpGate", 0.58f));
        const float probability = valueFor (*block, "mpStepProb" + suffix, 1.0f);

        owner.setStepValueFromEditor (step, noteOffset, velocity, gate, probability, dragActive,
                                      true, false, e.mods.isShiftDown(), false);
    }

    void MidiPlaygroundPage::PianoRollEditor::mouseDown (const juce::MouseEvent& e)
    {
        auto* block = owner.ensureMidiBlock();
        if (block == nullptr || owner.isDrumMachineBlock (*block))
            return;

        // Click in the piano-key column auditions that pitch via the
        // existing TestPage keyboard state; the user can pre-listen to a
        // note before drawing it.
        if (auto note = keyPreviewNoteAt (e.getPosition()); note >= 0)
        {
            // Audition feedback uses the TestPage's keyboard state if the
            // user has already activated audio. Quick-and-dirty: just
            // animate hover so something visible happens.
            hoverNoteOffset = (kRollRows / 2) - juce::jlimit (0, kRollRows - 1,
                (e.y - rollArea().getY()) / juce::jmax (1, rowHeight (rollArea())));
            repaint();
            return;
        }

        dragStep = stepAt (e.x);
        if (dragStep < 0)
            return;

        // Right-click deletes the step. Otherwise: paint mode = on. Ctrl
        // toggles the existing state for that step.
        const bool currentActive = valueFor (*block, "mpStep" + juce::String (dragStep) + "On", 1.0f) >= 0.5f;
        dragActive = e.mods.isPopupMenu() ? false
                   : (e.mods.isCommandDown() || e.mods.isCtrlDown()) ? ! currentActive
                   : true;
        editFromMouse (e);
    }

    void MidiPlaygroundPage::PianoRollEditor::mouseDrag (const juce::MouseEvent& e)
    {
        dragStep = stepAt (e.x);
        hoverStep = dragStep;
        hoverNoteOffset = noteOffsetAt (e.y);
        editFromMouse (e);
    }

    void MidiPlaygroundPage::PianoRollEditor::mouseMove (const juce::MouseEvent& e)
    {
        const int s = stepAt (e.x);
        const int o = noteOffsetAt (e.y);
        if (s != hoverStep || o != hoverNoteOffset)
        {
            hoverStep = s;
            hoverNoteOffset = o;
            repaint();
        }
    }

    void MidiPlaygroundPage::PianoRollEditor::mouseExit (const juce::MouseEvent&)
    {
        if (hoverStep != -1)
        {
            hoverStep = -1;
            repaint();
        }
    }

    void MidiPlaygroundPage::PianoRollEditor::paint (juce::Graphics& g)
    {
        PatchCraftLookAndFeel::drawDarkPanel (g, getLocalBounds(), 9.0f);

        auto content = getLocalBounds().reduced (12, 10);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("PIANO ROLL", content.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Drag to draw notes. Ctrl-click to toggle. Right-click to clear. Shift-drag horizontally sets gate length.",
                    content.removeFromTop (18), juce::Justification::centredLeft, true);

        const auto* block = owner.activeMidiBlock();
        if (block == nullptr || owner.isDrumMachineBlock (*block))
        {
            g.setFont (12.0f);
            g.drawFittedText ("Apply a MIDI template or add a Chord Phrase block to edit notes in piano-roll form.",
                              content, juce::Justification::centred, 2);
            return;
        }

        auto area = rollArea();
        auto keyArea = area.removeFromLeft (kRollKeyWidth);
        const int rH = rowHeight (area);
        const int steps = midiStepCount (*block);
        const int gap = steps > 64 ? 1 : 2;
        const int cellW = juce::jmax (steps > 64 ? 3 : 8,
                                      (area.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
        const int rootMidi = 60 + juce::jmax (0, owner.rootBox.getSelectedId() - 1);
        const int centreRow = kRollRows / 2;

        // ---- Piano keyboard column on the left -------------------------
        // Black-key rows get a darker fill, white-key rows lighter, with a
        // pronounced border at every C so octave boundaries stand out. Note
        // labels render at every C, plus G as a secondary marker.
        for (int row = 0; row < kRollRows; ++row)
        {
            const int offset = centreRow - row;
            const int y = keyArea.getY() + row * rH;
            const int midiNote = juce::jlimit (0, 127, rootMidi + offset);
            const int pitchClass = midiNote % 12;
            const bool black = pitchClass == 1 || pitchClass == 3 || pitchClass == 6
                            || pitchClass == 8 || pitchClass == 10;
            const bool isC = pitchClass == 0;
            const bool isF = pitchClass == 5;
            const bool isRoot = offset == 0;
            const bool isHover = (hoverStep >= 0 && hoverNoteOffset == offset);

            const auto keyFill = black ? juce::Colour (0xff111316)
                                       : juce::Colour (0xffd9d4cd);
            g.setColour (isHover ? PatchCraftLookAndFeel::accent().withAlpha (black ? 0.55f : 0.30f) : keyFill);
            g.fillRect (keyArea.withY (y).withHeight (rH));

            // Octave / fifth dividers in the key column.
            if (isC)
            {
                g.setColour (juce::Colour (0xff000000));
                g.drawHorizontalLine (y, (float) keyArea.getX(), (float) keyArea.getRight());
            }
            else if (isF)
            {
                g.setColour (juce::Colour (0x66000000));
                g.drawHorizontalLine (y, (float) keyArea.getX(), (float) keyArea.getRight());
            }

            // Grid row separators across the note area.
            g.setColour (isRoot ? PatchCraftLookAndFeel::accent().withAlpha (0.55f)
                       : isC    ? juce::Colour (0xff2a2e35)
                                : PatchCraftLookAndFeel::borderSoft().withAlpha (0.25f));
            g.drawHorizontalLine (y, (float) area.getX(), (float) area.getRight());

            // Note label at every C (plus the root row).
            if (rH >= 8 && (isC || isRoot))
            {
                const auto labelArea = keyArea.withY (y).withHeight (rH).reduced (4, 0);
                g.setColour (isRoot ? PatchCraftLookAndFeel::accent()
                           : black  ? juce::Colour (0xffb7afa3)
                                    : juce::Colour (0xff222426));
                g.setFont (juce::Font (juce::jmin (10.0f, (float) rH * 0.55f), juce::Font::bold));
                g.drawText (midiNoteName (midiNote), labelArea, juce::Justification::centredLeft);
            }
        }

        // Right edge of the piano keys.
        g.setColour (juce::Colour (0xff050608));
        g.drawVerticalLine (keyArea.getRight() - 1, (float) keyArea.getY(), (float) keyArea.getBottom());

        // ---- Step grid on the note area --------------------------------
        for (int step = 0; step <= steps; ++step)
        {
            const int x = area.getX() + step * (cellW + gap);
            const bool beat = step % 4 == 0;
            g.setColour (beat ? PatchCraftLookAndFeel::accent().withAlpha (0.30f)
                              : PatchCraftLookAndFeel::borderSoft().withAlpha (0.40f));
            g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
        }

        // Beat headers above the grid showing 1.1, 1.2, ... when there's room.
        if (cellW >= 28)
        {
            g.setFont (juce::Font (9.5f, juce::Font::bold));
            for (int step = 0; step < steps; step += 4)
            {
                const int x = area.getX() + step * (cellW + gap);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawText (juce::String (step / 4 + 1) + "." + juce::String ((step % 16) / 4 + 1),
                            x + 2, area.getY() - 14, 40, 12,
                            juce::Justification::centredLeft);
            }
        }

        // Hover cell highlight.
        if (hoverStep >= 0)
        {
            const int row = centreRow - juce::jlimit (-centreRow, centreRow, hoverNoteOffset);
            const int hx = area.getX() + hoverStep * (cellW + gap);
            const int hy = keyArea.getY() + row * rH;
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.16f));
            g.fillRect (juce::Rectangle<int> (hx, hy, cellW, rH));
        }

        // ---- Notes -----------------------------------------------------
        for (int step = 0; step < steps; ++step)
        {
            const auto suffix = juce::String (step);
            const bool active = valueFor (*block, "mpStep" + suffix + "On", 1.0f) >= 0.5f;
            if (! active) continue;

            const int offset = juce::jlimit (-centreRow, centreRow, roundStepNote (valueFor (*block, "arpNote" + suffix, 0.0f)));
            const float velocity = juce::jlimit (0.0f, 1.0f, valueFor (*block, "mpVelocity" + suffix, 0.85f));
            const float gate = juce::jlimit (0.08f, 1.0f,
                valueFor (*block, "mpGate" + suffix, valueFor (*block, "arpGate", 0.58f)));
            const int chordMode = juce::roundToInt (valueFor (*block, "mpChordMode", 0.0f));
            const int chordSize = juce::roundToInt (valueFor (*block, "mpChordSize", chordMode == 0 ? 1.0f : 3.0f));
            const auto intervals = chordIntervalsForMode (chordMode, chordSize);
            for (int i = 0; i < (int) intervals.size(); ++i)
            {
                const int noteOffset = juce::jlimit (-centreRow, centreRow, offset + intervals[(size_t) i]);
                const int row = centreRow - noteOffset;
                auto noteRect = juce::Rectangle<int> (area.getX() + step * (cellW + gap) + 1,
                                                       keyArea.getY() + row * rH + 1,
                                                       juce::jmax (2, juce::roundToInt ((float) cellW * gate)) - 1,
                                                       juce::jmax (6, rH - 2));
                const float alpha = i == 0 ? 0.55f + velocity * 0.40f : 0.32f + velocity * 0.36f;
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (alpha));
                g.fillRoundedRectangle (noteRect.toFloat(), 3.0f);
                g.setColour (i == 0 ? PatchCraftLookAndFeel::accent().brighter (0.25f)
                                    : PatchCraftLookAndFeel::accentDim().brighter (0.20f));
                g.drawRoundedRectangle (noteRect.toFloat(), 3.0f, 1.0f);
                if (i == 0 && noteRect.getWidth() >= 24 && rH >= 12)
                {
                    g.setColour (juce::Colour (0xff0a0c10));
                    g.setFont (juce::Font (juce::jmin (9.0f, (float) rH * 0.55f), juce::Font::bold));
                    g.drawText (midiNoteName (juce::jlimit (0, 127, rootMidi + noteOffset)),
                                noteRect.reduced (3, 0), juce::Justification::centredLeft);
                }
            }
        }
    }

    MidiPlaygroundPage::DrumPatternGrid::DrumPatternGrid (MidiPlaygroundPage& p) : owner (p)
    {
        setOpaque (false);
    }

    juce::Rectangle<int> MidiPlaygroundPage::DrumPatternGrid::gridArea() const
    {
        return getLocalBounds().reduced (12, 52).withTrimmedBottom (8);
    }

    int MidiPlaygroundPage::DrumPatternGrid::trackAt (int y) const
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr)
            return -1;

        auto area = gridArea();
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
        if (y < area.getY() || y >= area.getBottom())
            return -1;

        return juce::jlimit (0, tracks - 1, (y - area.getY()) * tracks / juce::jmax (1, area.getHeight()));
    }

    int MidiPlaygroundPage::DrumPatternGrid::stepAt (int x) const
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr)
            return -1;

        auto area = gridArea();
        area.removeFromLeft (112);
        const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (*block, "dmSteps", 16.0f)));
        const int gap = steps > 32 ? 2 : 4;
        const int cellW = juce::jmax (5, (area.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
        const int relative = x - area.getX();
        if (relative < 0)
            return -1;

        const int step = relative / (cellW + gap);
        return step >= 0 && step < steps ? step : -1;
    }

    void MidiPlaygroundPage::DrumPatternGrid::editFromMouse (const juce::MouseEvent& e)
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr || ! owner.isDrumMachineBlock (*block))
            return;

        const int track = trackAt (e.y);
        const int step = stepAt (e.x);
        if (track < 0 || step < 0)
            return;

        const bool shiftVelocity = e.mods.isShiftDown();
        float velocity = valueFor (*block, drumPrefix (*block, track, step) + "Vel", track == 0 ? 1.0f : 0.75f);
        bool active = dragState;
        if (shiftVelocity)
        {
            auto area = gridArea();
            const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
            const int rowH = juce::jmax (1, area.getHeight() / tracks);
            const int rowY = area.getY() + track * rowH;
            velocity = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.y - rowY) / (float) rowH);
            active = true;
        }

        owner.setDrumStepFromEditor (track, step, active, velocity, shiftVelocity);
    }

    void MidiPlaygroundPage::DrumPatternGrid::mouseDown (const juce::MouseEvent& e)
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr || ! owner.isDrumMachineBlock (*block))
            return;

        dragTrack = trackAt (e.y);
        dragStep = stepAt (e.x);
        if (dragTrack < 0 || dragStep < 0)
            return;

        const bool currentActive = valueFor (*block, drumPrefix (*block, dragTrack, dragStep) + "On", 0.0f) >= 0.5f;
        if (e.mods.isPopupMenu())
        {
            const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
            const int currentDivisions = drumCellDivisions (*block, pattern, dragTrack, dragStep);
            juce::PopupMenu m;
            m.addSectionHeader ("Cell Divisions");
            m.addItem (101, "Single", true, currentDivisions == 1);
            m.addItem (102, "Double", true, currentDivisions == 2);
            m.addItem (103, "Triple", true, currentDivisions == 3);
            m.addItem (104, "Quad", true, currentDivisions == 4);
            m.addSeparator();
            m.addItem (120, "Clear Cell");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                [this, track = dragTrack, step = dragStep] (int result)
                {
                    if (result >= 101 && result <= 104)
                        owner.setDrumStepFromEditor (track, step, true, 0.82f, false, result - 100);
                    else if (result == 120)
                        owner.setDrumStepFromEditor (track, step, false, 0.75f, false, 1);
                });
            dragTrack = -1;
            dragStep = -1;
            return;
        }

        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            const int pattern = juce::jlimit (0, 7, juce::roundToInt (valueFor (*block, "dmPattern", 0.0f)));
            const int currentDivisions = currentActive ? drumCellDivisions (*block, pattern, dragTrack, dragStep) : 1;
            const int nextDivisions = currentDivisions >= 4 ? 1 : currentDivisions + 1;
            const float velocity = valueFor (*block, drumPrefix (*block, dragTrack, dragStep) + "Vel",
                                             dragTrack == 0 ? 1.0f : 0.75f);
            owner.setDrumStepFromEditor (dragTrack, dragStep, true, velocity, false, nextDivisions);
            dragTrack = -1;
            dragStep = -1;
            return;
        }

        dragState = e.mods.isShiftDown() ? true : ! currentActive;
        editFromMouse (e);
    }

    void MidiPlaygroundPage::DrumPatternGrid::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragTrack < 0 || dragStep < 0)
            return;
        editFromMouse (e);
    }

    void MidiPlaygroundPage::DrumPatternGrid::mouseUp (const juce::MouseEvent&)
    {
        dragTrack = -1;
        dragStep = -1;
    }

    void MidiPlaygroundPage::DrumPatternGrid::paint (juce::Graphics& g)
    {
        PatchCraftLookAndFeel::drawDarkPanel (g, getLocalBounds(), 9.0f);

        auto content = getLocalBounds().reduced (12, 10);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("DRUM MACHINE PATTERN GRID", content.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Click/drag toggles hits. Shift-drag edits velocity. Ctrl-click cycles x2/x3/x4 divisions. Right-click opens cell tools.",
                    content.removeFromTop (18), juce::Justification::centredLeft, true);

        const auto* block = owner.activeMidiBlock();
        if (block == nullptr || ! owner.isDrumMachineBlock (*block))
        {
            g.setFont (12.0f);
            g.drawFittedText ("Choose Drum Machine to create an FL-style pattern block.",
                              content, juce::Justification::centred, 2);
            return;
        }

        auto area = gridArea();
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (*block, "dmTracks", 8.0f)));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (*block, "dmSteps", 16.0f)));
        const int labelW = 112;
        const int rowGap = 4;
        const int colGap = steps > 32 ? 2 : 4;
        const int rowH = juce::jmax (18, (area.getHeight() - rowGap * juce::jmax (0, tracks - 1)) / tracks);
        auto stepsArea = area.withTrimmedLeft (labelW);
        const int cellW = juce::jmax (5, (stepsArea.getWidth() - colGap * juce::jmax (0, steps - 1)) / steps);

        for (int step = 0; step < steps; ++step)
        {
            const int x = stepsArea.getX() + step * (cellW + colGap);
            if (step % 16 == 0)
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.22f));
            else if (step % 4 == 0)
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.70f));
            else
                g.setColour (PatchCraftLookAndFeel::borderSoft());
            g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
        }

        for (int track = 0; track < tracks; ++track)
        {
            const int y = area.getY() + track * (rowH + rowGap);
            auto labelArea = juce::Rectangle<int> (area.getX(), y, labelW - 8, rowH);
            const auto labelKey = "dmTrack" + juce::String (track) + "Label";
            const auto label = block->metadata.count (labelKey) != 0 ? block->metadata.at (labelKey) : defaultTrackLabel (track);
            const int note = juce::roundToInt (valueFor (*block, "dmTrack" + juce::String (track) + "Note", (float) defaultTrackNote (track)));

            g.setColour (PatchCraftLookAndFeel::panelAlt());
            g.fillRoundedRectangle (labelArea.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (labelArea.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::textBright());
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (label, labelArea.reduced (7, 1), juce::Justification::centredLeft);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (9.0f);
            g.drawText ("N" + juce::String (note), labelArea.reduced (7, 1), juce::Justification::centredRight);

            for (int step = 0; step < steps; ++step)
            {
                auto cell = juce::Rectangle<int> (stepsArea.getX() + step * (cellW + colGap), y, cellW, rowH);
                const auto prefix = drumPrefix (*block, track, step);
                const bool active = valueFor (*block, prefix + "On", 0.0f) >= 0.5f;
                const float velocity = juce::jlimit (0.0f, 1.0f, valueFor (*block, prefix + "Vel", track == 0 ? 1.0f : 0.75f));
                const int divisions = juce::jlimit (1, 4, juce::roundToInt (valueFor (*block, prefix + "Div", 1.0f)));

                g.setColour (step % 4 == 0 ? PatchCraftLookAndFeel::panelAlt().brighter (0.10f)
                                            : PatchCraftLookAndFeel::panelAlt());
                g.fillRoundedRectangle (cell.toFloat(), 4.0f);
                if (divisions > 1)
                {
                    g.setColour (PatchCraftLookAndFeel::border().brighter (0.35f).withAlpha (0.82f));
                    for (int division = 1; division < divisions; ++division)
                    {
                        const int x = cell.getX() + juce::roundToInt ((float) cell.getWidth() * (float) division / (float) divisions);
                        g.drawVerticalLine (x, (float) cell.getY() + 3.0f, (float) cell.getBottom() - 3.0f);
                    }
                }
                if (active)
                {
                    auto fill = cell.reduced (2).withTrimmedTop (juce::roundToInt ((1.0f - velocity) * (float) juce::jmax (1, cell.getHeight() - 4)));
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.38f + velocity * 0.55f));
                    g.fillRoundedRectangle (fill.toFloat(), 3.0f);
                    if (divisions > 1 && cell.getWidth() >= 18)
                    {
                        g.setColour (juce::Colour (0xff0a0c10));
                        g.setFont (juce::Font (juce::jmin (9.0f, (float) cell.getHeight() * 0.44f), juce::Font::bold));
                        g.drawText ("x" + juce::String (divisions), cell.reduced (1), juce::Justification::centred, true);
                    }
                }

                g.setColour (active ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::borderSoft());
                g.drawRoundedRectangle (cell.toFloat(), 4.0f, active ? 1.4f : 0.8f);
            }
        }
    }

    int MidiPlaygroundPage::MidiOutputLane::stepAt (int x) const
    {
        const auto* block = owner.activeMidiBlock();
        if (block == nullptr)
            return -1;

        auto area = getLocalBounds().reduced (12, 38);
        const int steps = midiStepCount (*block, 16.0f);
        const int gap = steps > 64 ? 1 : (steps > 32 ? 2 : 4);
        const int cellW = juce::jmax (steps > 64 ? 3 : 6,
                                      (area.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
        const int relative = x - area.getX();
        if (relative < 0)
            return -1;
        const int step = relative / (cellW + gap);
        return step >= 0 && step < steps ? step : -1;
    }

    MidiPlaygroundPage::MidiOutputLane::EditLane MidiPlaygroundPage::MidiOutputLane::laneAt (int y) const
    {
        auto area = getLocalBounds().reduced (12, 38);
        const int noteH = juce::roundToInt (area.getHeight() * 0.48f);
        const int velocityH = juce::roundToInt (area.getHeight() * 0.18f);
        const int gateH = juce::roundToInt (area.getHeight() * 0.17f);
        if (y < area.getY() + noteH)
            return EditLane::note;
        if (y < area.getY() + noteH + velocityH)
            return EditLane::velocity;
        if (y < area.getY() + noteH + velocityH + gateH)
            return EditLane::gate;
        return EditLane::probability;
    }

    void MidiPlaygroundPage::MidiOutputLane::editFromMouse (const juce::MouseEvent& e)
    {
        if (auto* block = owner.ensureMidiBlock())
        {
            const int step = dragStep >= 0 ? dragStep : stepAt (e.x);
            if (step < 0)
                return;

            auto area = getLocalBounds().reduced (12, 38);
            const int noteH = juce::roundToInt (area.getHeight() * 0.48f);
            const int velocityH = juce::roundToInt (area.getHeight() * 0.18f);
            const int gateH = juce::roundToInt (area.getHeight() * 0.17f);
            auto noteArea = area.removeFromTop (noteH);
            auto velocityArea = area.removeFromTop (velocityH);
            auto gateArea = area.removeFromTop (gateH);
            auto probabilityArea = area;

            const bool toggle = e.mods.isCtrlDown() || e.mods.isCommandDown();
            const bool currentActive = valueFor (*block, "mpStep" + juce::String (step) + "On", 1.0f) >= 0.5f;
            bool active = toggle ? ! currentActive : currentActive;
            int noteOffset = roundStepNote (valueFor (*block, "arpNote" + juce::String (step), 0.0f));
            float velocity = valueFor (*block, "mpVelocity" + juce::String (step), 1.0f);
            float gate = valueFor (*block, "mpGate" + juce::String (step), valueFor (*block, "arpGate", 0.55f));
            float probability = valueFor (*block, "mpStepProb" + juce::String (step), 1.0f);

            if (editLane == EditLane::note)
            {
                const float y01 = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.y - noteArea.getY()) / (float) juce::jmax (1, noteArea.getHeight()));
                if (valueFor (*block, "mpSampleControl", 0.0f) >= 0.5f && e.mods.isShiftDown())
                {
                    const int slices = juce::jmax (1, juce::roundToInt (valueFor (*block, "mpSampleSliceCount", 16.0f)));
                    noteOffset = juce::jlimit (0, slices - 1, juce::roundToInt (y01 * (float) (slices - 1)));
                }
                else
                {
                    noteOffset = roundStepNote (-12.0f + y01 * 36.0f);
                }
            }
            else if (editLane == EditLane::velocity)
            {
                velocity = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.y - velocityArea.getY()) / (float) juce::jmax (1, velocityArea.getHeight()));
            }
            else if (editLane == EditLane::gate)
            {
                gate = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.y - gateArea.getY()) / (float) juce::jmax (1, gateArea.getHeight()));
                gate = juce::jlimit (0.05f, 1.0f, gate);
            }
            else
            {
                probability = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.y - probabilityArea.getY()) / (float) juce::jmax (1, probabilityArea.getHeight()));
            }

            owner.setStepValueFromEditor (step, noteOffset, velocity, gate, probability, active,
                                          editLane == EditLane::note,
                                          editLane == EditLane::velocity,
                                          editLane == EditLane::gate,
                                          editLane == EditLane::probability);
        }
    }

    void MidiPlaygroundPage::MidiOutputLane::mouseDown (const juce::MouseEvent& e)
    {
        dragStep = stepAt (e.x);
        editLane = laneAt (e.y);
        editFromMouse (e);
    }

    void MidiPlaygroundPage::MidiOutputLane::mouseDrag (const juce::MouseEvent& e)
    {
        dragStep = stepAt (e.x);
        editFromMouse (e);
    }

    void MidiPlaygroundPage::MidiOutputLane::paint (juce::Graphics& g)
    {
        PatchCraftLookAndFeel::drawDarkPanel (g, getLocalBounds(), 9.0f);

        auto content = getLocalBounds().reduced (12, 10);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("MIDI OUTPUT EDITOR", content.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Drag pitch/slice, velocity, gate, or probability lanes. Ctrl-click toggles a step. Shift-drag pitch edits sample slices.",
                    content.removeFromTop (16), juce::Justification::centredLeft, true);

        if (const auto* block = owner.activeMidiBlock())
        {
            const int steps = midiStepCount (*block, 8.0f);
            const int gap = steps > 64 ? 1 : (steps > 32 ? 2 : 4);
            const int cellW = juce::jmax (steps > 64 ? 3 : 6,
                                          (content.getWidth() - gap * juce::jmax (0, steps - 1)) / steps);
            auto editor = content.removeFromTop (juce::jmax (112, content.getHeight() - 38));
            const int noteH = juce::roundToInt (editor.getHeight() * 0.48f);
            const int velocityH = juce::roundToInt (editor.getHeight() * 0.18f);
            const int gateH = juce::roundToInt (editor.getHeight() * 0.17f);
            auto noteLane = editor.removeFromTop (noteH);
            auto velocityLane = editor.removeFromTop (velocityH);
            auto gateLane = editor.removeFromTop (gateH);
            auto probabilityLane = editor;

            g.setColour (PatchCraftLookAndFeel::borderSoft());
            for (int line = 0; line <= 6; ++line)
            {
                const int y = noteLane.getY() + line * noteLane.getHeight() / 6;
                g.drawHorizontalLine (y, (float) noteLane.getX(), (float) noteLane.getRight());
            }

            for (int step = 0; step < steps; ++step)
            {
                auto cell = juce::Rectangle<int> (noteLane.getX() + step * (cellW + gap), noteLane.getY(), cellW, noteLane.getHeight());
                const bool active = valueFor (*block, "mpStep" + juce::String (step) + "On", 1.0f) >= 0.5f;
                const auto velocity = valueFor (*block, "mpVelocity" + juce::String (step), 1.0f);
                const auto gate = valueFor (*block, "mpGate" + juce::String (step), valueFor (*block, "arpGate", 0.55f));
                const auto probability = valueFor (*block, "mpStepProb" + juce::String (step), 1.0f);
                const auto noteOffset = roundStepNote (valueFor (*block, "arpNote" + juce::String (step), 0.0f));
                const float y01 = juce::jmap ((float) noteOffset, -12.0f, 24.0f, 1.0f, 0.0f);
                auto noteMarker = cell.withY (noteLane.getY() + juce::roundToInt (y01 * (float) juce::jmax (1, noteLane.getHeight() - 22)))
                                      .withHeight (22);

                g.setColour (active ? PatchCraftLookAndFeel::accent().withAlpha (0.22f + velocity * 0.35f)
                                    : PatchCraftLookAndFeel::panelAlt());
                g.fillRoundedRectangle (noteMarker.toFloat(), 5.0f);
                g.setColour (active ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (noteMarker.toFloat(), 5.0f, 1.0f);
                g.setColour (active ? PatchCraftLookAndFeel::textBright() : PatchCraftLookAndFeel::textDim());
                g.setFont (9.5f);
                const auto sampleSlice = valueFor (*block, "mpSampleSlice" + juce::String (step), -1.0f);
                const auto label = sampleSlice >= 0.0f ? "S" + juce::String (juce::roundToInt (sampleSlice))
                                                       : juce::String (noteOffset >= 0 ? "+" : "") + juce::String (noteOffset);
                g.drawText (label, noteMarker.reduced (3), juce::Justification::centred);

                auto velocityCell = juce::Rectangle<int> (velocityLane.getX() + step * (cellW + gap), velocityLane.getY(), cellW, velocityLane.getHeight());
                auto velocityFill = velocityCell.withTrimmedTop (juce::roundToInt ((1.0f - velocity) * (float) velocityCell.getHeight()));
                g.setColour (PatchCraftLookAndFeel::panelAlt());
                g.fillRoundedRectangle (velocityCell.toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (active ? 0.75f : 0.25f));
                g.fillRoundedRectangle (velocityFill.toFloat(), 3.0f);

                auto gateCell = juce::Rectangle<int> (gateLane.getX() + step * (cellW + gap), gateLane.getY(), cellW, gateLane.getHeight());
                auto gateFill = gateCell.withTrimmedTop (juce::roundToInt ((1.0f - gate) * (float) gateCell.getHeight()));
                g.setColour (PatchCraftLookAndFeel::panelAlt());
                g.fillRoundedRectangle (gateCell.toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::accentDim().withAlpha (active ? 0.80f : 0.25f));
                g.fillRoundedRectangle (gateFill.toFloat(), 3.0f);

                auto probabilityCell = juce::Rectangle<int> (probabilityLane.getX() + step * (cellW + gap), probabilityLane.getY(), cellW, probabilityLane.getHeight());
                auto probabilityFill = probabilityCell.withTrimmedTop (juce::roundToInt ((1.0f - probability) * (float) probabilityCell.getHeight()));
                g.setColour (PatchCraftLookAndFeel::panelAlt());
                g.fillRoundedRectangle (probabilityCell.toFloat(), 3.0f);
                g.setColour (juce::Colour (0xff8fd6ff).withAlpha (active ? 0.76f : 0.24f));
                g.fillRoundedRectangle (probabilityFill.toFloat(), 3.0f);
            }

            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawText ("Pitch / Slice", noteLane.removeFromLeft (76), juce::Justification::centredLeft);
            g.drawText ("Velocity", velocityLane.removeFromLeft (76), juce::Justification::centredLeft);
            g.drawText ("Gate", gateLane.removeFromLeft (76), juce::Justification::centredLeft);
            g.drawText ("Probability", probabilityLane.removeFromLeft (76), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawFittedText ("Add a Pattern Player or choose a musical idea to create playable notes, chops, drums, or modulation for the current DSP sound.",
                              content, juce::Justification::centred, 3);
        }
    }

    juce::String MidiPlaygroundPage::sourceHelpText() const
    {
        if (sourceBox.getSelectedId() == 2)
            return "Sampler target: Pattern slots trigger Sample Mapper zones, pads, slices, start/length, and velocity. Use Sample Mapper first if no samples play.";
        if (sourceBox.getSelectedId() == 3)
            return "FX target: Pattern slots drive gates, throws, rhythmic modulation, and performable FX targets against live/imported audio.";
        return "Synth target: Pattern slots generate note/gate/velocity/chord/arp data. DSP Builder still defines the oscillator/filter/amp sound.";
    }

    juce::Rectangle<int> MidiPlaygroundPage::arpStudioLaneBounds (juce::Rectangle<int> area, int lane) const
    {
        auto laneList = area.removeFromLeft (252);
        laneList.removeFromTop (54);
        const int gap = 8;
        const int rowH = juce::jmax (56, (laneList.getHeight() - gap * (MidiPlaygroundPattern::kPhraseBankCount - 1)) / MidiPlaygroundPattern::kPhraseBankCount);
        return juce::Rectangle<int> (laneList.getX(),
                                     laneList.getY() + lane * (rowH + gap),
                                     laneList.getWidth(),
                                     rowH).reduced (1);
    }

    juce::Rectangle<int> MidiPlaygroundPage::arpStudioMidiDragBounds (juce::Rectangle<int> area, int lane) const
    {
        return arpStudioLaneBounds (area, lane).reduced (10, 8).removeFromRight (76).withTrimmedBottom (18);
    }

    bool MidiPlaygroundPage::arpStudioStepHitTest (juce::Rectangle<int> area, juce::Point<int> pos,
                                                   int& lane, int& step, int& band, float& value) const
    {
        lane = -1;
        step = -1;
        band = -1;
        value = 0.0f;

        const auto* block = activeMidiBlock();
        static constexpr int fallbackSteps[] = { 16, 12, 16, 8, 16 };

        lane = block != nullptr
            ? juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, juce::roundToInt (valueFor (*block, "mpActiveBank", 0.0f)))
            : juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, phraseBankBox.getSelectedId() - 1);

        auto editor = area;
        editor.removeFromLeft (264);
        editor.removeFromRight (330);
        editor.removeFromTop (54);
        editor = editor.removeFromTop (282).reduced (12, 10);
        editor.removeFromTop (38);
        editor.removeFromLeft (78);
        if (! editor.contains (pos))
            return false;

        const int steps = block != nullptr
            ? juce::jlimit (1, 128, juce::roundToInt (arpBankValue (*block, lane, "arpSteps", (float) fallbackSteps[lane])))
            : fallbackSteps[lane];
        const int drawSteps = juce::jmin (steps, 64);
        const int gap = drawSteps > 32 ? 2 : 4;
        const int cellW = juce::jmax (5, (editor.getWidth() - gap * juce::jmax (0, drawSteps - 1)) / drawSteps);
        const int bandH = juce::jmax (20, editor.getHeight() / 4);
        const int relative = pos.x - editor.getX();
        if (relative < 0)
            return false;

        const int stride = cellW + gap;
        step = relative / stride;
        if (step < 0 || step >= drawSteps)
            return false;
        if ((relative % stride) >= cellW)
            return false;

        band = juce::jlimit (0, 3, (pos.y - editor.getY()) / bandH);
        const auto cell = juce::Rectangle<int> (editor.getX() + step * stride,
                                                editor.getY() + band * bandH + 2,
                                                cellW,
                                                juce::jmax (8, bandH - 4));
        if (! cell.contains (pos))
            return false;

        value = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (pos.y - cell.getY()) / (float) juce::jmax (1, cell.getHeight()));
        return true;
    }

    void MidiPlaygroundPage::editArpStudioStepFromMouse (const juce::MouseEvent& e, bool toggleStep)
    {
        auto content = getLocalBounds().reduced (12, 10);
        content.removeFromTop (104);

        int lane = -1;
        int step = -1;
        int band = -1;
        float value = 0.0f;
        if (! arpStudioStepHitTest (content, e.getPosition(), lane, step, band, value))
            return;

        auto* block = ensureMidiBlock();
        if (block == nullptr)
            return;

        const int activeBank = MidiPlaygroundPattern::getActiveBank (*block);
        if (activeBank != lane)
        {
            phraseBankBox.setSelectedId (lane + 1, juce::dontSendNotification);
            switchPhraseBank (lane);
            block = ensureMidiBlock();
            if (block == nullptr)
                return;
        }

        const auto suffix = juce::String (step);
        const auto activeKey = "mpStep" + suffix + "On";
        const bool wasActive = valueFor (*block, activeKey, 0.0f) >= 0.5f;
        block->values[activeKey] = toggleStep ? (wasActive ? 0.0f : 1.0f) : 1.0f;
        if (block->values.find ("mpGate" + suffix) == block->values.end())
            block->values["mpGate" + suffix] = valueFor (*block, "arpGate", 0.58f);
        if (block->values.find ("mpStepProb" + suffix) == block->values.end())
            block->values["mpStepProb" + suffix] = 1.0f;
        if (block->values.find ("arpNote" + suffix) == block->values.end())
            block->values["arpNote" + suffix] = 0.0f;
        if (block->values.find ("mpStepDiv" + suffix) == block->values.end())
            block->values["mpStepDiv" + suffix] = 1.0f;

        const juce::StringArray bandNames { "note", "velocity", "gate", "chance" };
        juce::String editedValue;
        if (! toggleStep)
        {
            switch (band)
            {
                case 0:
                {
                    const float note = (float) roundStepNote (juce::jmap (value, 0.0f, 1.0f, -12.0f, 24.0f));
                    block->values["arpNote" + suffix] = note;
                    editedValue = juce::String ((int) note) + " st";
                    break;
                }
                case 1:
                    block->values["mpVelocity" + suffix] = juce::jlimit (0.0f, 1.0f, value);
                    editedValue = juce::String (juce::roundToInt (value * 127.0f));
                    break;
                case 2:
                    block->values["mpGate" + suffix] = juce::jlimit (0.05f, 1.0f, value);
                    editedValue = juce::String (juce::roundToInt (value * 100.0f)) + "%";
                    break;
                case 3:
                    block->values["mpStepProb" + suffix] = juce::jlimit (0.0f, 1.0f, value);
                    editedValue = juce::String (juce::roundToInt (value * 100.0f)) + "%";
                    break;
                default:
                    break;
            }
        }

        MidiPlaygroundPattern::storeActiveBank (*block, lane);
        arpStudioEditingLane = lane;
        arpStudioEditingStep = step;
        arpStudioEditingBand = band;
        activeSummary.setText ("Slot " + juce::String (lane + 1)
                               + " step " + juce::String (step + 1)
                               + ": " + bandNames[juce::jlimit (0, bandNames.size() - 1, band)]
                               + (editedValue.isNotEmpty() ? " " + editedValue : "")
                               + (block->values[activeKey] >= 0.5f ? " on" : " off"),
                               juce::dontSendNotification);
        notifyGraphChanged (false);
        syncControlsFromBlock();
        repaint();
    }

    void MidiPlaygroundPage::drawArpStudioKnob (juce::Graphics& g, juce::Rectangle<int> bounds,
                                                const juce::String& label, const juce::String& value,
                                                juce::Colour accent, float normalised) const
    {
        auto knob = bounds.withTrimmedBottom (20).toFloat().reduced (3.0f);
        const float diameter = (float) juce::jmin (knob.getWidth(), knob.getHeight());
        knob = juce::Rectangle<float> (knob.getCentreX() - diameter * 0.5f, knob.getY(), diameter, diameter);
        g.setColour (juce::Colour (0xff05070a));
        g.fillEllipse (knob);
        g.setColour (accent.withAlpha (0.34f));
        g.drawEllipse (knob.reduced (1.0f), 2.0f);
        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end = juce::MathConstants<float>::pi * 2.75f;
        const float angle = juce::jmap (juce::jlimit (0.0f, 1.0f, normalised), 0.0f, 1.0f, start, end);
        g.setColour (accent);
        juce::Path arc;
        arc.addCentredArc (knob.getCentreX(), knob.getCentreY(), diameter * 0.43f, diameter * 0.43f, 0.0f, start, angle, true);
        g.strokePath (arc, juce::PathStrokeType (2.0f));
        auto pointer = knob.getCentre() + juce::Point<float> (std::cos (angle) * diameter * 0.28f,
                                                              std::sin (angle) * diameter * 0.28f);
        g.drawLine (knob.getCentreX(), knob.getCentreY(), pointer.x, pointer.y, 2.0f);
        g.setFont (juce::Font (8.2f, juce::Font::bold));
        g.drawText (label, bounds.withTrimmedTop (bounds.getHeight() - 22), juce::Justification::centred, true);
        g.setFont (juce::Font (8.0f));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText (value, bounds.withTrimmedTop (bounds.getHeight() - 11), juce::Justification::centred, true);
    }

    void MidiPlaygroundPage::drawArpStudioLane (juce::Graphics& g, juce::Rectangle<int> bounds,
                                                int lane, const DspBlock* block) const
    {
        static const juce::Colour laneColours[] =
        {
            juce::Colour (0xff16d6e9), juce::Colour (0xff9b6dff), juce::Colour (0xffff5bb9),
            juce::Colour (0xffffa32b), juce::Colour (0xff8df04b)
        };
        static const char* laneNames[] = { "SPIRAL", "ASCENT", "BOUNCE", "RANDOM", "RONDO" };
        static const int fallbackSteps[] = { 16, 12, 16, 8, 16 };
        const auto accent = laneColours[juce::jlimit (0, 4, lane)];
        const int activeLane = block != nullptr
            ? juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, juce::roundToInt (valueFor (*block, "mpActiveBank", 0.0f)))
            : 0;
        const bool selected = lane == activeLane;
        const int steps = block != nullptr
            ? juce::jlimit (1, 128, juce::roundToInt (arpBankValue (*block, lane, "arpSteps", (float) fallbackSteps[lane])))
            : fallbackSteps[lane];
        const int drawSteps = juce::jmin (steps, 64);
        int activeCount = 0;
        for (int step = 0; step < drawSteps; ++step)
            if (block == nullptr || arpBankValue (*block, lane, "mpStep" + juce::String (step) + "On", step % 2 == 0 ? 1.0f : 0.0f) >= 0.5f)
                ++activeCount;

        g.setColour (selected ? juce::Colour (0xff0d141d) : juce::Colour (0xff080c12));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (accent.withAlpha (selected ? 0.95f : 0.42f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, selected ? 1.8f : 1.0f);

        auto area = bounds.reduced (10, 8);
        auto dragHandle = area.removeFromRight (76).withTrimmedBottom (18);
        auto meterArea = area.removeFromBottom (18);
        auto titleArea = area.removeFromTop (22);

        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.setColour (accent);
        g.drawText (juce::String (lane + 1) + "  " + laneNames[lane], titleArea, juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (9.5f);
        g.drawText (juce::String (steps) + " steps  |  " + juce::String (activeCount) + " active",
                    area.removeFromTop (16), juce::Justification::centredLeft, true);
        g.drawText (lane == 0 ? "Synth lane / Group 1"
                  : lane == 1 ? "Sample lane / Group 2"
                  : lane == 2 ? "Accent lane / Group 3"
                  : lane == 3 ? "Random lane / Group 4"
                              : "Macro lane / Group 5",
                    area.removeFromTop (16), juce::Justification::centredLeft, true);

        g.setColour (juce::Colour (0xff141a22));
        g.fillRoundedRectangle (dragHandle.toFloat(), 5.0f);
        g.setColour (accent.withAlpha (0.80f));
        g.drawRoundedRectangle (dragHandle.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setFont (juce::Font (7.4f, juce::Font::bold));
        g.drawText ("DRAG MIDI", dragHandle, juce::Justification::centred, true);

        meterArea.removeFromRight (84);
        const int gap = drawSteps > 32 ? 1 : 2;
        const int cellW = juce::jmax (2, (meterArea.getWidth() - gap * juce::jmax (0, drawSteps - 1)) / drawSteps);
        for (int step = 0; step < drawSteps; ++step)
        {
            const juce::String suffix (step);
            const bool active = block != nullptr
                ? arpBankValue (*block, lane, "mpStep" + suffix + "On", step % 2 == 0 ? 1.0f : 0.0f) >= 0.5f
                : step % 2 == 0;
            const float velocity = block != nullptr
                ? juce::jlimit (0.0f, 1.0f, arpBankValue (*block, lane, "mpVelocity" + suffix, 0.72f))
                : (step % 4 == 0 ? 0.96f : 0.62f);
            auto cell = juce::Rectangle<int> (meterArea.getX() + step * (cellW + gap),
                                              meterArea.getY(), cellW, meterArea.getHeight());
            g.setColour (PatchCraftLookAndFeel::panelAlt());
            g.fillRect (cell);
            if (active)
            {
                auto fill = cell.withTrimmedTop (juce::roundToInt ((1.0f - velocity) * (float) cell.getHeight()));
                g.setColour (accent.withAlpha (selected ? 0.82f : 0.46f));
                g.fillRect (fill);
            }
        }
    }

    void MidiPlaygroundPage::drawArpStudioFxGroup (juce::Graphics& g, juce::Rectangle<int> bounds, int group,
                                                   const juce::String& name, const juce::String& mode,
                                                   juce::Colour accent) const
    {
        g.setColour (juce::Colour (0xff080c12).withAlpha (0.98f));
        g.fillRoundedRectangle (bounds.toFloat(), 10.0f);
        g.setColour (accent.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 10.0f, 1.0f);
        auto area = bounds.reduced (10, 8);
        auto header = area.removeFromTop (22);
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.setColour (accent);
        g.drawText ("GROUP " + juce::String (group + 1), header.removeFromLeft (66), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText (name, header.removeFromLeft (90), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.drawText (mode, header, juce::Justification::centredRight, true);

        drawArpStudioKnob (g, area.removeFromLeft (area.getWidth() / 4), group == 1 ? "TIME" : group == 3 ? "CUT" : "AMT",
                           group == 1 ? "1/4" : group == 3 ? "2.1k" : "65%", accent, group == 3 ? 0.42f : 0.65f);
        drawArpStudioKnob (g, area.removeFromLeft (area.getWidth() / 3), group == 2 ? "DECAY" : "WET",
                           group == 2 ? "2.4s" : "60%", accent, group == 2 ? 0.56f : 0.60f);
        drawArpStudioKnob (g, area.removeFromLeft (area.getWidth() / 2), group == 4 ? "RATIO" : "OUT",
                           group == 4 ? "1.8" : "0dB", accent, 0.50f);
        g.setColour (accent.withAlpha (0.18f));
        g.fillRoundedRectangle (area.reduced (4, 14).toFloat(), 4.0f);
    }

    bool MidiPlaygroundPage::startArpStudioMidiDrag (int lane)
    {
        auto* block = activeMidiBlock();
        if (block == nullptr)
            block = ensureMidiBlock();
        if (block == nullptr)
            return false;

        DspBlock exportBlock = *block;
        lane = juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, lane);
        MidiPlaygroundPattern::loadBank (exportBlock, lane, false);
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("MidiDrag");
        if (! folder.createDirectory())
        {
            activeSummary.setText ("MIDI drag failed: could not create temp MIDI folder.", juce::dontSendNotification);
            return false;
        }

        static const char* names[] = { "Spiral", "Ascent", "Bounce", "Random", "Rondo" };
        auto target = folder.getChildFile (arpStudioMidiFileName (names[lane], lane));
        int duplicateIndex = 2;
        while (target.existsAsFile())
            target = folder.getChildFile (arpStudioMidiFileName (juce::String (names[lane]) + "_" + juce::String (duplicateIndex++), lane));

        juce::String error;
        if (! MidiPlaygroundPattern::writeMidiClip (exportBlock, target, 120.0, 60, error))
        {
            activeSummary.setText ("MIDI drag failed: " + error, juce::dontSendNotification);
            return false;
        }

        juce::StringArray files;
        files.add (target.getFullPathName());
        const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
        activeSummary.setText (started ? "Dragging MIDI clip: " + target.getFileName()
                                       : "MIDI drag failed: OS did not start an external file drag.",
                               juce::dontSendNotification);
        return started;
    }

    void MidiPlaygroundPage::drawArpStudio (juce::Graphics& g)
    {
        {
            juce::Component* hidden[] =
            {
                &title, &subtitle, &activeSummary, &modeBox, &editorViewBox, &musicalPresetBox,
                &chordPresetBox, &drumPatternBox, &progressionBox, &sliceCountSlider,
                &chordSizeSlider, &chordSpreadSlider, &octaveSlider, &flamSlider,
                &euclideanPulsesSlider, &euclideanRotateSlider, &octaveFoldToggle,
                &addPlaygroundButton, &chordPhraseButton, &sampleSliceButton, &drumMachineButton,
                &operatorsButton, &generateAiMidiButton, &phraseLibraryButton, &applyMusicalPresetButton,
                &storeBankButton, &duplicateBankButton, &applyProgressionButton,
                &midiOutputLane, &pianoRollEditor, &drumPatternGrid
            };
            for (auto* component : hidden)
                component->setVisible (false);

            juce::Component* visible[] =
            {
                &performanceViewButton, &arpLaneViewButton,
                &sourceBox, &midiTemplateBox, &guiTemplateBox, &rootBox, &scaleBox, &targetBox,
                &phraseBankBox, &stepsSlider, &rateSlider, &gateSlider, &swingSlider,
                &probabilitySlider, &humanizeSlider, &mutationSlider, &ratchetSlider,
                &velocityCurveSlider, &strumSlider, &applyMidiTemplateButton, &applyGuiTemplateButton,
                &exportMidiButton, &playPatternButton, &stopPatternButton, &sourceBuilderButton,
                &sampleMapperButton, &testButton
            };
            for (auto* component : visible)
                component->setVisible (true);

            static const juce::Colour laneColours[] =
            {
                juce::Colour (0xff16d6e9), juce::Colour (0xff9b6dff), juce::Colour (0xffff5bb9),
                juce::Colour (0xffffa32b), juce::Colour (0xff8df04b)
            };
            static const char* laneNames[] = { "Spiral", "Ascent", "Bounce", "Random", "Rondo" };

            auto full = getLocalBounds().reduced (12, 10);
            g.fillAll (juce::Colour (0xff05080d));

            const auto* block = activeMidiBlock();
            const int selectedLane = block != nullptr
                ? juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, juce::roundToInt (valueFor (*block, "mpActiveBank", 0.0f)))
                : juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, phraseBankBox.getSelectedId() - 1);
            const auto selectedAccent = laneColours[juce::jlimit (0, 4, selectedLane)];

            auto top = full.removeFromTop (108);
            PatchCraftLookAndFeel::drawDarkPanel (g, top, 10.0f);
            auto topArea = top.reduced (16, 10);

            const int actionsWidth = juce::jmin (topArea.getWidth(),
                                                 juce::jmax (640, topArea.getWidth() - 330));
            auto actions = topArea.removeFromRight (actionsWidth);
            topArea.removeFromRight (12);
            auto brand = topArea;

            g.setColour (PatchCraftLookAndFeel::textBright());
            g.setFont (juce::Font (22.0f, juce::Font::bold));
            g.drawText ("CIRCLE PATTERN VIEW", brand.removeFromTop (28), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawText ("Patterning-style circular view of the same Pattern Player slots.",
                        brand.removeFromTop (22), juce::Justification::centredLeft, true);
            g.setColour (selectedAccent);
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText ("Selected lane: " + juce::String (selectedLane + 1) + " " + laneNames[selectedLane],
                        brand, juce::Justification::centredLeft, true);

            auto viewTabs = actions.removeFromTop (28);
            performanceViewButton.setBounds (viewTabs.removeFromLeft (142).reduced (2));
            arpLaneViewButton.setBounds (viewTabs.removeFromLeft (118).reduced (2));
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.0f);
            g.drawText ("Same instrument data: slots -> notes/slices/modulation -> current DSP sound",
                        viewTabs, juce::Justification::centredRight, true);
            actions.removeFromTop (2);
            auto actionTop = actions.removeFromTop (32);
            auto placeActionButton = [] (juce::Rectangle<int>& row, juce::Component& component, int width)
            {
                component.setBounds (row.removeFromLeft (juce::jmin (width, row.getWidth())).reduced (2));
                row.removeFromLeft (6);
            };
            placeActionButton (actionTop, playPatternButton, 118);
            placeActionButton (actionTop, stopPatternButton, 82);
            placeActionButton (actionTop, sourceBuilderButton, 148);
            placeActionButton (actionTop, sampleMapperButton, 140);
            placeActionButton (actionTop, testButton, 116);
            auto actionBottom = actions.removeFromTop (34);
            placeActionButton (actionBottom, applyMidiTemplateButton, 150);
            placeActionButton (actionBottom, applyGuiTemplateButton, 150);
            placeActionButton (actionBottom, exportMidiButton, 118);
            midiTemplateBox.setBounds (actionBottom.removeFromLeft (juce::jmin (190, actionBottom.getWidth())).reduced (2));

            full.removeFromTop (12);
            auto body = full;
            auto leftPanel = body.removeFromLeft (252);
            body.removeFromLeft (12);
            auto rightPanel = body.removeFromRight (318);
            body.removeFromRight (12);
            auto main = body;

            PatchCraftLookAndFeel::drawDarkPanel (g, leftPanel, 10.0f);
            auto left = leftPanel.reduced (12, 10);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("LANES / CIRCLES", left.removeFromTop (22), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.0f);
            g.drawText ("Click a lane to edit its slot. Drag MIDI from a lane into a DAW.",
                        left.removeFromTop (20), juce::Justification::centredLeft, true);
            for (int lane = 0; lane < MidiPlaygroundPattern::kPhraseBankCount; ++lane)
                drawArpStudioLane (g, arpStudioLaneBounds (full, lane), lane, block);

            auto sourceCard = leftPanel.reduced (12, 10).removeFromBottom (112);
            PatchCraftLookAndFeel::drawPanel (g, sourceCard, 8.0f);
            auto sourceInner = sourceCard.reduced (10, 8);
            g.setColour (selectedAccent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ("SELECTED LANE SOURCE", sourceInner.removeFromTop (18), juce::Justification::centredLeft, true);
            auto sourceRow = sourceInner.removeFromTop (28);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (9.5f);
            g.drawText ("DSP", sourceRow.removeFromLeft (52), juce::Justification::centredLeft, true);
            sourceBox.setBounds (sourceRow.reduced (2));
            auto targetRow = sourceInner.removeFromTop (28);
            g.drawText ("Target", targetRow.removeFromLeft (52), juce::Justification::centredLeft, true);
            targetBox.setBounds (targetRow.reduced (2));
            g.drawText ("Player preview currently plays one active slot. Lane source choices are stored for builder/export work.",
                        sourceInner, juce::Justification::centredLeft, true);

            auto projectStrip = main.removeFromTop (46);
            PatchCraftLookAndFeel::drawPanel (g, projectStrip, 8.0f);
            auto strip = projectStrip.reduced (10, 8);
            auto placeBox = [&] (const juce::String& label, juce::Component& component, int w)
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText (label, strip.removeFromLeft (54), juce::Justification::centredLeft, true);
                component.setBounds (strip.removeFromLeft (w).reduced (2));
                strip.removeFromLeft (10);
            };
            placeBox ("Slot", phraseBankBox, 92);
            placeBox ("Key", rootBox, 82);
            placeBox ("Scale", scaleBox, 140);
            placeBox ("UI", guiTemplateBox, 178);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText ("DSP owns the sound. Lane buttons choose which pattern slot you are editing and playing.",
                        strip, juce::Justification::centredLeft, true);

            main.removeFromTop (8);
            auto stepPanel = main.removeFromTop (282);
            PatchCraftLookAndFeel::drawDarkPanel (g, stepPanel, 10.0f);
            auto stepArea = stepPanel.reduced (12, 10);
            auto stepHeader = stepArea.removeFromTop (30);
            g.setColour (selectedAccent);
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText ("STEP EDITOR - " + juce::String (laneNames[selectedLane]).toUpperCase(),
                        stepHeader.removeFromLeft (260), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.0f);
            g.drawText ("Click/drag Note, Velocity, Gate, or Chance cells. Ctrl-click or right-click toggles the step.",
                        stepHeader, juce::Justification::centredRight, true);

            auto grid = stepArea;
            auto labels = grid.removeFromLeft (78);
            const int steps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (arpBankValue (*block, selectedLane, "arpSteps", 16.0f)))
                : 16;
            const int drawSteps = juce::jmin (steps, 64);
            const int gap = drawSteps > 32 ? 2 : 4;
            const int cellW = juce::jmax (5, (grid.getWidth() - gap * juce::jmax (0, drawSteps - 1)) / drawSteps);
            const int bandH = juce::jmax (20, grid.getHeight() / 4);
            const juce::StringArray bandLabels { "Note", "Velocity", "Gate", "Chance" };
            for (int band = 0; band < 4; ++band)
            {
                auto bandLabel = labels.withY (grid.getY() + band * bandH).withHeight (bandH);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.5f, juce::Font::bold));
                g.drawText (bandLabels[band], bandLabel, juce::Justification::centredLeft, true);
                g.setColour (PatchCraftLookAndFeel::borderSoft());
                g.drawHorizontalLine (bandLabel.getY(), (float) grid.getX(), (float) grid.getRight());
            }

            const int playbackStep = patternPreviewActive && arpStudioPreviewStartMs > 0.0
                ? juce::jlimit (0, drawSteps - 1,
                                (int) std::floor (std::fmod ((juce::Time::getMillisecondCounterHiRes() - arpStudioPreviewStartMs) * 0.008,
                                                             (double) drawSteps)))
                : -1;

            for (int step = 0; step < drawSteps; ++step)
            {
                const juce::String suffix (step);
                const bool active = block != nullptr
                    ? arpBankValue (*block, selectedLane, "mpStep" + suffix + "On", 1.0f) >= 0.5f
                    : true;
                const float note = block != nullptr ? arpBankValue (*block, selectedLane, "arpNote" + suffix, 0.0f) : 0.0f;
                const float velocity = block != nullptr ? juce::jlimit (0.0f, 1.0f, arpBankValue (*block, selectedLane, "mpVelocity" + suffix, 0.72f)) : 0.72f;
                const float gate = block != nullptr ? juce::jlimit (0.05f, 1.0f, arpBankValue (*block, selectedLane, "mpGate" + suffix, 0.58f)) : 0.58f;
                const float probability = block != nullptr ? juce::jlimit (0.0f, 1.0f, arpBankValue (*block, selectedLane, "mpStepProb" + suffix, 1.0f)) : 1.0f;
                const bool editing = selectedLane == arpStudioEditingLane && step == arpStudioEditingStep;
                const bool playing = step == playbackStep;
                const float values[] =
                {
                    juce::jlimit (0.0f, 1.0f, juce::jmap (note, -12.0f, 24.0f, 0.0f, 1.0f)),
                    velocity,
                    gate,
                    probability
                };

                const int x = grid.getX() + step * (cellW + gap);
                for (int band = 0; band < 4; ++band)
                {
                    auto cell = juce::Rectangle<int> (x, grid.getY() + band * bandH + 2,
                                                      cellW, juce::jmax (8, bandH - 4));
                    g.setColour (active ? juce::Colour (0xff101823) : juce::Colour (0xff0a0d12));
                    g.fillRoundedRectangle (cell.toFloat(), 3.0f);
                    auto fill = cell.withTrimmedTop (juce::roundToInt ((1.0f - values[band]) * (float) cell.getHeight()));
                    g.setColour (selectedAccent.withAlpha (active ? (band == 1 ? 0.84f : 0.55f) : 0.16f));
                    g.fillRoundedRectangle (fill.toFloat(), 3.0f);
                    const bool editingBand = editing && band == arpStudioEditingBand;
                    if (playing || editingBand)
                    {
                        g.setColour (playing ? juce::Colours::white.withAlpha (0.75f) : selectedAccent);
                        g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 3.0f, playing ? 1.4f : 1.1f);
                    }
                }
            }

            main.removeFromTop (8);
            auto lower = main;
            auto macroPanel = lower.removeFromLeft (juce::jmax (320, lower.getWidth() / 2));
            lower.removeFromLeft (10);
            PatchCraftLookAndFeel::drawPanel (g, macroPanel, 8.0f);
            auto macro = macroPanel.reduced (12, 10);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("PERFORMANCE / MODULATION", macro.removeFromTop (20), juce::Justification::centredLeft, true);
            auto sliderRow = [&] (const juce::String& label, juce::Slider& slider)
            {
                auto row = macro.removeFromTop (28);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.5f, juce::Font::bold));
                g.drawText (label, row.removeFromLeft (92), juce::Justification::centredLeft, true);
                slider.setBounds (row.reduced (2));
            };
            sliderRow ("Steps", stepsSlider);
            sliderRow ("Rate", rateSlider);
            sliderRow ("Gate", gateSlider);
            sliderRow ("Swing", swingSlider);
            sliderRow ("Probability", probabilitySlider);
            sliderRow ("Humanize", humanizeSlider);
            sliderRow ("Mutation", mutationSlider);
            sliderRow ("Ratchet", ratchetSlider);
            sliderRow ("Strum", strumSlider);

            PatchCraftLookAndFeel::drawPanel (g, lower, 8.0f);
            auto routing = lower.reduced (12, 10);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("FX GROUP ROUTING", routing.removeFromTop (20), juce::Justification::centredLeft, true);
            const juce::StringArray groups { "Group 1: Harmonic mute", "Group 2: Delay throw", "Group 3: Reverb space", "Group 4: Filter motion", "Group 5: Gate/pulse" };
            for (int i = 0; i < groups.size(); ++i)
            {
                auto row = routing.removeFromTop (28).reduced (0, 2);
                g.setColour (juce::Colour (0xff0b1017));
                g.fillRoundedRectangle (row.toFloat(), 5.0f);
                g.setColour (laneColours[i].withAlpha (i == selectedLane ? 0.95f : 0.45f));
                g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 5.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (juce::Font (9.5f, juce::Font::bold));
                g.drawText (groups[i], row.reduced (8, 0), juce::Justification::centredLeft, true);
            }

            PatchCraftLookAndFeel::drawDarkPanel (g, rightPanel, 10.0f);
            auto right = rightPanel.reduced (12, 10);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("LIVE PLAYER PREVIEW", right.removeFromTop (20), juce::Justification::centredLeft, true);
            auto preview = right.removeFromTop (172);
            PatchCraftLookAndFeel::drawPanel (g, preview, 8.0f);
            auto previewContent = preview.reduced (12, 12);
            const int circleW = juce::jmax (44, previewContent.getWidth() / 5 - 6);
            for (int lane = 0; lane < 5; ++lane)
            {
                auto circleBox = previewContent.removeFromLeft (circleW).reduced (2);
                previewContent.removeFromLeft (6);
                const float d = (float) juce::jmin (circleBox.getWidth(), circleBox.getHeight() - 26);
                const juce::Point<float> c ((float) circleBox.getCentreX(), (float) circleBox.getY() + d * 0.48f);
                const auto accent = laneColours[lane];
                g.setColour (juce::Colour (0xff080c12));
                g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
                g.setColour (accent.withAlpha (lane == selectedLane ? 0.95f : 0.35f));
                g.drawEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d, lane == selectedLane ? 2.0f : 1.0f);
                for (int step = 0; step < 16; ++step)
                {
                    const bool active = block == nullptr || arpBankValue (*block, lane, "mpStep" + juce::String (step) + "On", step % 2 == 0 ? 1.0f : 0.0f) >= 0.5f;
                    if (! active)
                        continue;
                    const float angle = -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * (float) step / 16.0f;
                    const float velocity = block != nullptr ? arpBankValue (*block, lane, "mpVelocity" + juce::String (step), 0.72f) : 0.72f;
                    const float radius = d * juce::jmap (juce::jlimit (0.18f, 1.0f, velocity), 0.18f, 1.0f, 0.12f, 0.42f);
                    const auto p = c + juce::Point<float> (std::cos (angle) * radius, std::sin (angle) * radius);
                    g.setColour (accent.withAlpha (0.76f));
                    g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
                }
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (8.5f, juce::Font::bold));
                g.drawText (laneNames[lane], circleBox.removeFromBottom (18), juce::Justification::centred, true);
            }

            right.removeFromTop (10);
            auto inspector = right.removeFromTop (134);
            PatchCraftLookAndFeel::drawPanel (g, inspector, 8.0f);
            auto inspect = inspector.reduced (10, 8);
            g.setColour (selectedAccent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ("STEP INSPECTOR", inspect.removeFromTop (18), juce::Justification::centredLeft, true);
            const int step = juce::jlimit (0, 63, arpStudioEditingStep >= 0 ? arpStudioEditingStep : 0);
            auto info = [&] (const juce::String& label, const juce::String& value)
            {
                auto row = inspect.removeFromTop (22);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (9.5f);
                g.drawText (label, row.removeFromLeft (86), juce::Justification::centredLeft, true);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.drawText (value, row, juce::Justification::centredLeft, true);
            };
            info ("Lane", juce::String (selectedLane + 1) + " " + laneNames[selectedLane]);
            info ("Step", juce::String (step + 1));
            info ("Velocity", block != nullptr ? juce::String (juce::roundToInt (arpBankValue (*block, selectedLane, "mpVelocity" + juce::String (step), 0.72f) * 127.0f)) : "91");
            info ("Gate", block != nullptr ? juce::String (juce::roundToInt (arpBankValue (*block, selectedLane, "mpGate" + juce::String (step), 0.58f) * 100.0f)) + "%" : "58%");

            right.removeFromTop (10);
            auto exportPanel = right;
            PatchCraftLookAndFeel::drawPanel (g, exportPanel, 8.0f);
            auto exportInfo = exportPanel.reduced (10, 8);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ("OUTPUT CONTRACT", exportInfo.removeFromTop (18), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.0f);
            g.drawFittedText ("Make Circle Pattern writes the five slot runtime data. Update Player Controls generates the finished CircleSEQ-style template. MIDI export writes the selected slot as a DAW-ready clip.",
                              exportInfo, juce::Justification::topLeft, 5);
        }
        return;
    }

    void MidiPlaygroundPage::paint (juce::Graphics& g)
    {
        if (arpStudioMode)
        {
            drawArpStudio (g);
            return;
        }

        for (auto* component : { static_cast<juce::Component*> (&title), static_cast<juce::Component*> (&subtitle),
                                 static_cast<juce::Component*> (&performanceViewButton),
                                 static_cast<juce::Component*> (&arpLaneViewButton),
                                 static_cast<juce::Component*> (&activeSummary) })
            component->setVisible (true);

        g.fillAll (PatchCraftLookAndFeel::bg());

        auto r = getLocalBounds().reduced (18, 14);
        auto header = r.removeFromTop (88);

        PatchCraftLookAndFeel::drawDarkPanel (g, header, 10.0f);
        auto headerContent = header.reduced (14, 10);
        auto viewTabs = headerContent.removeFromRight (290);
        performanceViewButton.setBounds (viewTabs.removeFromTop (28).removeFromLeft (150).reduced (2));
        viewTabs.removeFromTop (2);
        arpLaneViewButton.setBounds (viewTabs.removeFromTop (28).removeFromLeft (126).reduced (2));
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (9.5f);
        g.drawFittedText ("Pattern Builder and Circle View edit the same instrument slots. Start with Build Sound, then make a playable pattern.",
                          viewTabs, juce::Justification::topLeft, 3);
        headerContent.removeFromRight (12);
        title.setBounds (headerContent.removeFromTop (24));
        subtitle.setBounds (headerContent.removeFromTop (20));
        activeSummary.setBounds (headerContent);

        r.removeFromTop (12);
        auto controls = r.removeFromLeft (360);
        auto main = r;
        const auto* currentBlock = activeMidiBlock();
        const bool drumMode = (currentBlock != nullptr && isDrumMachineBlock (*currentBlock))
                           || modeBox.getSelectedId() == 5;

        editorViewBox.setVisible (! drumMode);
        musicalPresetBox.setVisible (true);
        applyMusicalPresetButton.setVisible (true);
        chordPresetBox.setVisible (! drumMode);
        midiTemplateBox.setVisible (true);
        guiTemplateBox.setVisible (true);
        phraseBankBox.setVisible (! drumMode);
        drumPatternBox.setVisible (drumMode);
        progressionBox.setVisible (! drumMode);
        rootBox.setVisible (! drumMode);
        scaleBox.setVisible (! drumMode);
        targetBox.setVisible (! drumMode);
        chordSizeSlider.setVisible (! drumMode);
        chordSpreadSlider.setVisible (! drumMode);
        octaveSlider.setVisible (! drumMode);
        gateSlider.setVisible (! drumMode);
        humanizeSlider.setVisible (! drumMode);
        mutationSlider.setVisible (! drumMode);
        ratchetSlider.setVisible (! drumMode);
        velocityCurveSlider.setVisible (! drumMode);
        strumSlider.setVisible (! drumMode);
        flamSlider.setVisible (! drumMode);
        euclideanPulsesSlider.setVisible (! drumMode);
        euclideanRotateSlider.setVisible (! drumMode);
        octaveFoldToggle.setVisible (! drumMode);
        sliceCountSlider.setVisible (true);
        applyMidiTemplateButton.setVisible (true);
        applyGuiTemplateButton.setVisible (true);

        PatchCraftLookAndFeel::drawPanel (g, controls, 10.0f);
        auto controlContent = controls.reduced (12, 12);

        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("CURRENT PATCH SOURCE", controlContent.removeFromTop (22), juce::Justification::centredLeft);

        drawControl (g, controlContent.removeFromTop (30), "Source", sourceBox);
        controlContent.removeFromTop (4);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawFittedText (sourceHelpText(), controlContent.removeFromTop (42), juce::Justification::topLeft, 2);
        auto sourceRow = controlContent.removeFromTop (30);
        sourceBuilderButton.setBounds (sourceRow.removeFromLeft (160).reduced (2));
        sampleMapperButton.setBounds (sourceRow.reduced (2));
        controlContent.removeFromTop (8);

        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("PERFORMANCE WORKFLOW", controlContent.removeFromTop (22), juce::Justification::centredLeft);

        auto presetRow = controlContent.removeFromTop (30);
        auto presetLabel = presetRow.removeFromLeft (116);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Drop Preset", presetLabel, juce::Justification::centredLeft);
        applyMusicalPresetButton.setBounds (presetRow.removeFromRight (82).reduced (2));
        musicalPresetBox.setBounds (presetRow.reduced (0, 0));
        controlContent.removeFromTop (6);

        drawControl (g, controlContent.removeFromTop (30), "Mode", modeBox);
        controlContent.removeFromTop (6);
        if (! drumMode)
        {
            drawControl (g, controlContent.removeFromTop (30), "Editor", editorViewBox);
            controlContent.removeFromTop (6);
            drawControl (g, controlContent.removeFromTop (30), "Chord Preset", chordPresetBox);
            controlContent.removeFromTop (6);
        }
        drawControl (g, controlContent.removeFromTop (30), "Behavior", midiTemplateBox);
        controlContent.removeFromTop (6);
        drawControl (g, controlContent.removeFromTop (30), "GUI Template", guiTemplateBox);
        controlContent.removeFromTop (8);

        if (drumMode)
        {
            drawControl (g, controlContent.removeFromTop (30), "Drum Pattern", drumPatternBox);
            controlContent.removeFromTop (8);
            drawControl (g, controlContent.removeFromTop (28), "Grid Steps", stepsSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Drum Tracks", sliceCountSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Rate", rateSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Swing", swingSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Probability", probabilitySlider);
            controlContent.removeFromTop (12);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.5f);
            g.drawFittedText ("Drum Machine runs from DAW/Test transport. Track labels map to Sample Mapper drum-pad notes; cells store hit, velocity, gate, and probability.",
                              controlContent.removeFromTop (54), juce::Justification::topLeft, 3);
        }
        else
        {
            drawControl (g, controlContent.removeFromTop (30), "Phrase Bank", phraseBankBox);
            controlContent.removeFromTop (6);
            drawControl (g, controlContent.removeFromTop (30), "Progression", progressionBox);
            controlContent.removeFromTop (6);
            drawControl (g, controlContent.removeFromTop (30), "Scale Root", rootBox);
            controlContent.removeFromTop (6);
            drawControl (g, controlContent.removeFromTop (30), "Scale Type", scaleBox);
            controlContent.removeFromTop (6);
            drawControl (g, controlContent.removeFromTop (30), "Target", targetBox);
            controlContent.removeFromTop (10);

            drawControl (g, controlContent.removeFromTop (28), "Chord Size", chordSizeSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Chord Spread", chordSpreadSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Octaves", octaveSlider);
            controlContent.removeFromTop (8);

            drawControl (g, controlContent.removeFromTop (28), "Steps", stepsSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Rate", rateSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Gate", gateSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Swing", swingSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Probability", probabilitySlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Humanize", humanizeSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Mutation", mutationSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Ratchet", ratchetSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Vel Curve", velocityCurveSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Strum", strumSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Flam", flamSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Euclid", euclideanPulsesSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Rotate", euclideanRotateSlider);
            controlContent.removeFromTop (4);
            drawControl (g, controlContent.removeFromTop (28), "Slice Count", sliceCountSlider);
            controlContent.removeFromTop (4);
            auto foldRow = controlContent.removeFromTop (26);
            foldRow.removeFromLeft (116);
            octaveFoldToggle.setBounds (foldRow);
        }

        auto buttonRows = controlContent.removeFromBottom (192);
        auto row = buttonRows.removeFromTop (32);
        addPlaygroundButton.setBounds (row.removeFromLeft (158).reduced (2));
        chordPhraseButton.setBounds (row.reduced (2));
        row = buttonRows.removeFromTop (32);
        sampleSliceButton.setBounds (row.removeFromLeft (80).reduced (2));
        drumMachineButton.setBounds (row.removeFromLeft (80).reduced (2));
        operatorsButton.setBounds (row.removeFromLeft (104).reduced (2));
        generateAiMidiButton.setBounds (row.removeFromLeft (90).reduced (2));
        phraseLibraryButton.setBounds (row.removeFromLeft (112).reduced (2));
        row = buttonRows.removeFromTop (32);
        applyMidiTemplateButton.setBounds (row.removeFromLeft (170).reduced (2));
        applyGuiTemplateButton.setBounds (row.reduced (2));
        row = buttonRows.removeFromTop (32);
        storeBankButton.setBounds (row.removeFromLeft (104).reduced (2));
        duplicateBankButton.setBounds (row.reduced (2));
        row = buttonRows.removeFromTop (32);
        applyProgressionButton.setBounds (row.removeFromLeft (108).reduced (2));
        exportMidiButton.setBounds (row.removeFromLeft (108).reduced (2));
        testButton.setBounds (row.reduced (2));
        row = buttonRows.removeFromTop (32);
        playPatternButton.setBounds (row.removeFromLeft (158).reduced (2));
        stopPatternButton.setBounds (row.reduced (2));

        // Compact status strip in place of the old 6-card grid.
        auto cards = main.removeFromTop (32);
        drawSectionCards (g, cards);
        main.removeFromTop (8);
        const bool pianoRollMode = ! drumMode && editorViewBox.getSelectedId() == 2;
        drumPatternGrid.setVisible (drumMode);
        midiOutputLane.setVisible (! drumMode && ! pianoRollMode);
        pianoRollEditor.setVisible (pianoRollMode);
        if (drumMode)
            drumPatternGrid.setBounds (main);
        else if (pianoRollMode)
            pianoRollEditor.setBounds (main);
        else
            midiOutputLane.setBounds (main);
    }

    void MidiPlaygroundPage::resized()
    {
        repaint();
    }

    void MidiPlaygroundPage::visibilityChanged()
    {
        setArpStudioHardwarePreviewActive (arpStudioMode && isShowing());
    }
}
