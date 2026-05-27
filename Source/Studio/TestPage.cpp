#include "TestPage.h"

#include "CanvasEditor.h"
#include "EngineFactory.h"
#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"
#include "StudioAudioService.h"
#include "StudioInstrumentRenderer.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        constexpr int kTimerHz = 30;

        static void styleHeader (juce::Label& label, const juce::String& text)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (juce::Font (12.0f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        }

        static float peakForChannel (const juce::AudioBuffer<float>& buffer, int channel)
        {
            return channel < buffer.getNumChannels()
                ? buffer.getMagnitude (channel, 0, buffer.getNumSamples())
                : 0.0f;
        }

        static juce::Rectangle<int> fitCanvasIntoBounds (CanvasSize canvasSize,
                                                         juce::Rectangle<int> bounds)
        {
            if (canvasSize.width <= 0) canvasSize.width = 1280;
            if (canvasSize.height <= 0) canvasSize.height = 800;
            if (bounds.isEmpty())
                return bounds;

            const float scale = juce::jmin ((float) bounds.getWidth() / (float) canvasSize.width,
                                            (float) bounds.getHeight() / (float) canvasSize.height);
            const int width = juce::jmax (1, juce::roundToInt ((float) canvasSize.width * scale));
            const int height = juce::jmax (1, juce::roundToInt ((float) canvasSize.height * scale));
            return bounds.withSizeKeepingCentre (width, height);
        }

        static DspBlock* findDrumMachineBlock (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("drum") || block.values.find ("dmTracks") != block.values.end())
                    return &block;
            return nullptr;
        }

        static DspBlock* findArpBlock (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("arp")
                    || block.type.containsIgnoreCase ("midi")
                    || block.values.find ("arpSteps") != block.values.end())
                    return &block;
            return nullptr;
        }

        static int defaultDrumTrackNote (int track)
        {
            static const int notes[] =
            {
                36, 38, 42, 46, 39, 45, 48, 49,
                51, 37, 44, 52, 53, 54, 55, 56
            };
            return track >= 0 && track < 16 ? notes[track] : 36 + track;
        }

        static DspBlock& ensureDrumMachineBlock (DspGraph& graph)
        {
            if (auto* existing = findDrumMachineBlock (graph))
                return *existing;

            DspBlock block;
            block.id = "midi_drum_machine";
            int suffix = 2;
            auto idExists = [&] (const juce::String& id)
            {
                for (const auto& existing : graph.blocks)
                    if (existing.id == id)
                        return true;
                return false;
            };
            while (idExists (block.id))
                block.id = "midi_drum_machine_" + juce::String (suffix++);

            block.section = "mod";
            block.type = "drumMachine";
            block.name = "Drum Machine Performance";
            block.targetId = "midiDrumMachine";
            block.enabled = true;
            block.values["dmTracks"] = 8.0f;
            block.values["dmSteps"] = 16.0f;
            block.values["dmPattern"] = 0.0f;
            block.values["dmTransport"] = 1.0f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
            block.values["enabled"] = 1.0f;
            for (int track = 0; track < 16; ++track)
                block.values["dmTrack" + juce::String (track) + "Note"] = (float) defaultDrumTrackNote (track);

            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
            return graph.blocks.back();
        }

        static DspBlock& ensureArpBlock (DspGraph& graph)
        {
            if (auto* existing = findArpBlock (graph))
                return *existing;

            DspBlock block;
            block.id = "midi_arp_sequencer";
            int suffix = 2;
            auto idExists = [&] (const juce::String& id)
            {
                for (const auto& existing : graph.blocks)
                    if (existing.id == id)
                        return true;
                return false;
            };
            while (idExists (block.id))
                block.id = "midi_arp_sequencer_" + juce::String (suffix++);

            block.section = "mod";
            block.type = "arpSequencer";
            block.name = "Circle Sequencer";
            block.targetId = "midiArpSequencer";
            block.enabled = true;
            block.values["arpSteps"] = 16.0f;
            block.values["mpActiveBank"] = 0.0f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
            block.values["enabled"] = 1.0f;
            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
            return graph.blocks.back();
        }

        static float blockValue (const DspBlock& block, const juce::String& key, float fallback)
        {
            const auto it = block.values.find (key);
            return it != block.values.end() ? it->second : fallback;
        }

        static juce::String arpBankPrefix (int lane)
        {
            return "mpBank" + juce::String (juce::jlimit (0, 15, lane) + 1) + "_";
        }

        static void setArpLaneValue (DspBlock& block, int lane, const juce::String& key, float value)
        {
            block.values[arpBankPrefix (lane) + key] = value;
            if (lane == juce::jlimit (0, 15, juce::roundToInt (blockValue (block, "mpActiveBank", 0.0f))))
                block.values[key] = value;
        }

        static juce::String orbitLaneSoundName (int sound)
        {
            return "DSP Slot " + juce::String (juce::jlimit (0, 15, sound) + 1);
        }

        static juce::String orbitLaneTargetName (int target)
        {
            switch (juce::jlimit (0, 4, target))
            {
                case 1:  return "drums";
                case 2:  return "oneShots";
                case 3:  return "loops";
                case 4:  return "samples";
                default: return "notes";
            }
        }

        static int orbitLaneSoundNote (int target, int sound, int rootNote)
        {
            sound = juce::jlimit (0, 15, sound);
            rootNote = juce::jlimit (0, 127, rootNote);
            if (target == 1)
                return defaultDrumTrackNote (sound);
            if (target == 2)
                return juce::jlimit (0, 127, 48 + sound);
            if (target == 4)
                return juce::jlimit (0, 127, rootNote + sound - 7);
            return rootNote;
        }

        static void applyArpLaneLiveControlToGraph (PatchCraftProject& project, const juce::String& parameterId)
        {
            if (! parameterId.startsWith ("arpLane"))
                return;

            auto value = [&] (const juce::String& id, float fallback)
            {
                if (auto* def = project.getParameters().find (id))
                    return project.getLiveValues().getValue (id, def->defaultValue);
                return project.getLiveValues().getValue (id, fallback);
            };

            auto& graph = project.getDspGraph();
            auto& block = ensureArpBlock (graph);
            const int elementLane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneIndex", 0.0f)));
            const int lane = parameterId == "arpLaneIndex"
                ? elementLane
                : juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneControlBank", (float) elementLane)));
            const int steps = juce::jlimit (1, 128, juce::roundToInt (value ("arpLaneSteps", 16.0f)));
            const int target = juce::jlimit (0, 4, juce::roundToInt (value ("arpLaneTarget", 0.0f)));
            const int direction = juce::jlimit (0, 3, juce::roundToInt (value ("arpLaneDirection", 0.0f)));
            const int sound = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneSound", (float) lane)));
            const int rootNote = juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRootNote", 60.0f)));
            const int slots = juce::jlimit (1, 64, juce::roundToInt (value ("arpLaneSampleSlots", 1.0f)));
            const auto targetName = orbitLaneTargetName (target);
            juce::ignoreUnused (rootNote);

            block.values["mpActiveBank"] = (float) lane;
            block.values["mpMultiLane"] = 1.0f;
            setArpLaneValue (block, lane, "arpSteps", (float) steps);
            setArpLaneValue (block, lane, "arpPattern", direction == 1 ? 1.0f : direction == 2 ? 2.0f : direction == 3 ? 7.0f : 0.0f);
            setArpLaneValue (block, lane, "arpGate", juce::jlimit (0.05f, 1.0f, value ("arpLaneGate", 0.58f)));
            setArpLaneValue (block, lane, "arpSwing", juce::jlimit (0.0f, 0.5f, value ("arpLaneSwing", 0.0f)));
            setArpLaneValue (block, lane, "rate", juce::jlimit (0.0625f, 16.0f, value ("arpLaneRate", 1.0f)));
            setArpLaneValue (block, lane, "mpProbability", juce::jlimit (0.0f, 1.0f, value ("arpLaneProbability", 1.0f)));
            setArpLaneValue (block, lane, "mpRatchet", juce::jlimit (1.0f, 8.0f, value ("arpLaneRatchet", 1.0f)));
            setArpLaneValue (block, lane, "mpEuclideanPulses", (float) juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneEuclideanPulses", 0.0f))));
            setArpLaneValue (block, lane, "mpEuclideanRotate", (float) juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRotate", 0.0f))));
            setArpLaneValue (block, lane, "mpSampleControl", target == 0 ? 0.0f : 1.0f);
            setArpLaneValue (block, lane, "mpSampleSliceCount", (float) juce::jmax (slots, sound + 1));
            setArpLaneValue (block, lane, "mpLaneMute", value ("arpLaneMute", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            setArpLaneValue (block, lane, "mpLaneFxTarget", (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4)))));
            const float laneFxAmount = juce::jlimit (0.0f, 1.0f, value ("arpLaneFxAmount", 0.0f));

            for (int step = 0; step < steps; ++step)
            {
                const auto suffix = juce::String (step);
                if (targetName == "loops")
                    setArpLaneValue (block, lane, "mpSampleSlice" + suffix, (float) (step % juce::jmax (1, slots)));
                else if (target != 0)
                    setArpLaneValue (block, lane, "mpSampleSlice" + suffix, (float) sound);
                if (parameterId == "arpLaneFxAmount")
                    setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, laneFxAmount);
            }

            block.metadata["arpLane" + juce::String (lane + 1) + "Target"] = targetName;
            block.metadata["arpLane" + juce::String (lane + 1) + "Sound"] = juce::String (sound);
            block.metadata["arpLane" + juce::String (lane + 1) + "SoundName"] = orbitLaneSoundName (sound);
            block.metadata["arpLane" + juce::String (lane + 1) + "FxTarget"] = juce::String (juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4)))));
            graph.userConfigured = true;
        }
    }

    class TestPage::MeterView : public juce::Component
    {
    public:
        explicit MeterView (TestPage& p) : page (p) {}

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PatchCraftLookAndFeel::panelAlt());
            auto r = getLocalBounds().reduced (10);
            drawMeter (g, r.removeFromTop ((r.getHeight() - 8) / 2), "L", page.peakL.load());
            r.removeFromTop (8);
            drawMeter (g, r, "R", page.peakR.load());
        }

    private:
        TestPage& page;

        static void drawMeter (juce::Graphics& g, juce::Rectangle<int> r,
                               const juce::String& name, float peak)
        {
            auto label = r.removeFromLeft (18);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawText (name, label, juce::Justification::centredLeft);

            g.setColour (PatchCraftLookAndFeel::bg());
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 3.0f, 1.0f);

            const auto fill = r.withWidth (juce::roundToInt (r.getWidth() * juce::jlimit (0.0f, 1.0f, peak)));
            g.setColour (PatchCraftLookAndFeel::accent());
            g.fillRoundedRectangle (fill.toFloat().reduced (1.0f), 2.0f);
        }
    };

    class TestPage::ClipView : public juce::Component, public juce::KeyListener
    {
    public:
        explicit ClipView (TestPage& p) : page (p)
        {
            setWantsKeyboardFocus (true);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PatchCraftLookAndFeel::panelAlt());
            auto r = getLocalBounds().reduced (10);

            g.setColour (PatchCraftLookAndFeel::bg());
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f);

            {
                const juce::ScopedLock lock (page.clipLock);
                for (int i = 0; i < (int) page.clip.size(); ++i)
                {
                    const auto& event = page.clip[(size_t) i];
                    const auto x = r.getX() + juce::roundToInt ((event.time / page.clipLengthSeconds()) * r.getWidth());
                    const auto w = juce::jmax (8, juce::roundToInt ((event.length / page.clipLengthSeconds()) * r.getWidth()));
                    const auto y = juce::jmap (event.note, 84, 36, r.getY() + 4, r.getBottom() - 8);
                    g.setColour (i == page.selectedClipIndex
                        ? PatchCraftLookAndFeel::accent()
                        : PatchCraftLookAndFeel::accent().withAlpha (0.72f));
                    g.fillRoundedRectangle ((float) x, (float) y, (float) w, 7.0f, 2.0f);
                }
            }

            const auto playX = r.getX() + juce::roundToInt ((page.playPosSeconds.load() / page.clipLengthSeconds()) * r.getWidth());
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawVerticalLine (playX, (float) r.getY(), (float) r.getBottom());
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            auto r = getLocalBounds().reduced (10);
            page.selectedClipIndex = hitNote (r, e.getPosition());
            page.dragClipIndex = page.selectedClipIndex;

            if (page.selectedClipIndex < 0 && e.getNumberOfClicks() >= 2 && r.contains (e.getPosition()))
            {
                const auto time = juce::jlimit (0.0, page.clipLengthSeconds(),
                    ((double) (e.x - r.getX()) / (double) r.getWidth()) * page.clipLengthSeconds());
                const auto note = juce::jlimit (36, 84,
                    juce::roundToInt (juce::jmap ((float) e.y, (float) r.getBottom(), (float) r.getY(), 36.0f, 84.0f)));

                const juce::ScopedLock lock (page.clipLock);
                page.clip.push_back ({ time, 0.5, note, 0.8f });
                page.selectedClipIndex = (int) page.clip.size() - 1;
                page.dragClipIndex = page.selectedClipIndex;
            }

            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (page.dragClipIndex < 0) return;

            auto r = getLocalBounds().reduced (10);
            const auto endTime = juce::jlimit (0.0, page.clipLengthSeconds(),
                ((double) (e.x - r.getX()) / (double) r.getWidth()) * page.clipLengthSeconds());

            const juce::ScopedLock lock (page.clipLock);
            if (page.dragClipIndex >= 0 && page.dragClipIndex < (int) page.clip.size())
            {
                auto& event = page.clip[(size_t) page.dragClipIndex];
                event.length = juce::jmax (0.05, endTime - event.time);
            }
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            page.dragClipIndex = -1;
        }

        bool keyPressed (const juce::KeyPress& key, juce::Component*) override
        {
            // Handle computer keyboard input for virtual keyboard
            if (key.isKeyCode (juce::KeyPress::spaceKey))
            {
                // Space bar = toggle transport play/pause
                if (page.playing.load())
                    page.onTransportStopPressed();
                else
                    page.onTransportPlayPressed();
                return true;
            }

            // Map computer keyboard A-K to MIDI notes 60-71
            if (key.getTextCharacter() >= 'A' && key.getTextCharacter() <= 'K')
            {
                const int midiNote = 60 + (key.getTextCharacter() - 'A');
                const float velocity = key.getModifiers().isShiftDown() ? 0.8f : 0.6f;
                page.handleNoteOn (&page.keyboardState, 1, midiNote, velocity);
                return true;
            }

            if (key != juce::KeyPress::deleteKey && key != juce::KeyPress::backspaceKey)
                return false;

            const juce::ScopedLock lock (page.clipLock);
            if (page.selectedClipIndex >= 0 && page.selectedClipIndex < (int) page.clip.size())
            {
                page.clip.erase (page.clip.begin() + page.selectedClipIndex);
                page.selectedClipIndex = -1;
            }
            repaint();
            return true;
        }

    private:
        TestPage& page;

        int hitNote (juce::Rectangle<int> r, juce::Point<int> point) const
        {
            const juce::ScopedLock lock (page.clipLock);
            for (int i = (int) page.clip.size() - 1; i >= 0; --i)
            {
                const auto& event = page.clip[(size_t) i];
                const auto x = r.getX() + juce::roundToInt ((event.time / page.clipLengthSeconds()) * r.getWidth());
                const auto w = juce::jmax (8, juce::roundToInt ((event.length / page.clipLengthSeconds()) * r.getWidth()));
                const auto y = juce::jmap (event.note, 84, 36, r.getY() + 4, r.getBottom() - 8);
                if (juce::Rectangle<int> (x, y - 2, w, 11).contains (point))
                    return i;
            }
            return -1;
        }
    };

    class TestPage::ParamMonitor : public juce::Component
    {
    public:
        explicit ParamMonitor (TestPage& p) : page (p) {}

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PatchCraftLookAndFeel::panelAlt());
            auto r = getLocalBounds().reduced (10);
            g.setFont (11.0f);

            int shown = 0;
            for (const auto& def : page.owner.getProject().getParameters().getAll())
            {
                if (shown >= 8) break;
                auto row = r.removeFromTop (20);
                const auto value = page.owner.getProject().getLiveValues().getValue (def.id, def.defaultValue);
                g.setColour (PatchCraftLookAndFeel::text());
                g.drawText (def.name, row.removeFromLeft (90), juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawText (juce::String (value, 2) + (def.unit.isNotEmpty() ? " " + def.unit : ""),
                            row, juce::Justification::centredRight);
                ++shown;
            }

            if (shown == 0)
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.drawText ("No parameters in this project.", r, juce::Justification::centred);
            }
        }

    private:
        TestPage& page;
    };

    class TestPage::SpectrumView : public juce::Component
    {
    public:
        explicit SpectrumView (TestPage& p) : page (p) {}

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PatchCraftLookAndFeel::panelAlt());
            auto r = getLocalBounds().reduced (10);
            g.setColour (PatchCraftLookAndFeel::bg());
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f);

            std::array<float, 48> bins {};
            {
                const juce::SpinLock::ScopedLockType lock (page.spectrumLock);
                bins = page.spectrumBins;
            }

            const int barCount = (int) bins.size();
            const float barW = (float) r.getWidth() / (float) barCount;
            for (int i = 0; i < barCount; ++i)
            {
                const float level = juce::jlimit (0.0f, 1.0f, bins[(size_t) i]);
                const auto barH = level * (float) r.getHeight();
                auto bar = juce::Rectangle<float> (r.getX() + i * barW,
                                                   r.getBottom() - barH,
                                                   juce::jmax (1.0f, barW - 1.0f),
                                                   barH);
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.35f + level * 0.65f));
                g.fillRoundedRectangle (bar, 1.5f);
            }

            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (9.5f);
            g.drawText ("20 Hz", r.removeFromLeft (48), juce::Justification::bottomLeft);
            g.drawText ("20 kHz", r.removeFromRight (54), juce::Justification::bottomRight);
        }

    private:
        TestPage& page;
    };

    TestPage::TestPage (StudioMainComponent& o) : owner (o)
    {
        instrumentRenderer = std::make_unique<StudioInstrumentRenderer> (owner);
        instrumentRenderer->onNoteOn = [this] (int note, float velocity)
        {
            keyboardState.noteOn (1, note, velocity);
        };
        instrumentRenderer->onNoteOff = [this] (int note)
        {
            keyboardState.noteOff (1, note, 0.0f);
        };
        instrumentRenderer->isTransportPlaying = [this] { return isTransportPlaying(); };
        instrumentRenderer->getSequencerPlaybackPosition01 = [this] (int steps)
        {
            return getSequencerPlaybackPosition01 (steps);
        };
        instrumentRenderer->onToggleTransport = [this] { togglePreviewPlayback(); };
        instrumentRenderer->onSetDrumActivePattern = [this] (int pattern)
        {
            return setDrumActivePatternFromUi (pattern);
        };
        instrumentRenderer->onSetDrumPatternCell = [this] (int pattern, int track, int step, bool active,
                                                           float velocity, float gate, float probability, int divisions)
        {
            return setDrumPatternCellFromUi (pattern, track, step, active, velocity, gate, probability, divisions);
        };
        instrumentRenderer->onSetArpLaneStep = [this] (int lane, int step, float velocity, bool active)
        {
            return setArpLaneStepFromUi (lane, step, velocity, active);
        };
        addAndMakeVisible (*instrumentRenderer);

        keyboard = std::make_unique<juce::MidiKeyboardComponent> (
            keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
        keyboard->setAvailableRange (24, 108);
        keyboard->setLowestVisibleKey (36);
        keyboard->setKeyWidth (18.0f);
        keyboard->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9d8b8));
        keyboard->setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141413));
        keyboard->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                             PatchCraftLookAndFeel::accent().withAlpha (0.5f));
        addAndMakeVisible (*keyboard);

        clipView = std::make_unique<ClipView> (*this);
        meterView = std::make_unique<MeterView> (*this);
        spectrumView = std::make_unique<SpectrumView> (*this);
        paramMonitor = std::make_unique<ParamMonitor> (*this);
        addAndMakeVisible (*clipView);
        addAndMakeVisible (*meterView);
        addAndMakeVisible (*spectrumView);
        addAndMakeVisible (*paramMonitor);

        styleHeader (transportLabel, "TRANSPORT");
        styleHeader (clipLabel, "MIDI CLIP");
        styleHeader (keyboardLabel, "SOFTWARE + HARDWARE MIDI");
        styleHeader (metersLabel, "OUTPUT");
        styleHeader (spectrumLabel, "SPECTRUM");
        styleHeader (monitorLabel, "LIVE PARAMETERS");
        addAndMakeVisible (transportLabel);
        addAndMakeVisible (clipLabel);
        addAndMakeVisible (keyboardLabel);
        addAndMakeVisible (metersLabel);
        addAndMakeVisible (spectrumLabel);
        addAndMakeVisible (monitorLabel);

        statusLabel.setText ("Audio idle", juce::dontSendNotification);
        statusLabel.setFont (juce::Font (11.0f));
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (statusLabel);

        playBtn.onClick = [this] { onTransportPlayPressed(); };
        stopBtn.onClick = [this] { onTransportStopPressed(); };
        recordBtn.setClickingTogglesState (true);
        recordBtn.onClick = [this] { onTransportRecordPressed(); };
        loopToggle.setToggleState (true, juce::dontSendNotification);
        loopToggle.onClick = [this] { looping.store (loopToggle.getToggleState()); };
        tempoSlider.setRange (40.0, 220.0, 1.0);
        tempoSlider.setValue (owner.getProject().getLiveValues().getValue ("projectBpm", 120.0f));
        tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
        tempoSlider.setTextValueSuffix (" BPM");
        tempoSlider.setTooltip ("Global test BPM. Synced LFOs, automation lanes, and FX delay blocks follow this tempo.");
        tempoSlider.onValueChange = [this]
        {
            const auto value = tempoSlider.getValue();
            bpm.store (value);
            owner.getProject().getLiveValues().setValue ("projectBpm", (float) value);
        };
        clearClipBtn.onClick = [this] { clearClip(); };

        for (auto* component : { static_cast<juce::Component*> (&playBtn),
                                 static_cast<juce::Component*> (&stopBtn),
                                 static_cast<juce::Component*> (&recordBtn),
                                 static_cast<juce::Component*> (&loopToggle),
                                 static_cast<juce::Component*> (&tempoSlider),
                                 static_cast<juce::Component*> (&clearClipBtn) })
            addAndMakeVisible (component);

        owner.getProject().addListener (this);
        owner.getProject().getLiveValues().addListener (this);
        keyboardState.addListener (this);
        startTimerHz (kTimerHz);
    }

    TestPage::~TestPage()
    {
        stopTimer();
        deactivate();
        keyboardState.removeListener (this);
        owner.getProject().getLiveValues().removeListener (this);
        owner.getProject().removeListener (this);
    }

    void TestPage::setBrandLabPreviewMode (bool shouldUse)
    {
        if (brandLabPreviewMode == shouldUse)
            return;

        brandLabPreviewMode = shouldUse;
        resized();
        repaint();
    }

    void TestPage::showMidiClipEditor()
    {
        struct ClipModalContent final : public juce::Component
        {
            explicit ClipModalContent (TestPage& p)
                : page (p), clipView (p)
            {
                title.setText ("MIDI Clip Editor", juce::dontSendNotification);
                title.setFont (juce::Font (15.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                addAndMakeVisible (title);

                subtitle.setText ("Record, draw, drag, lengthen, and delete notes without permanently occupying the Player preview.",
                                  juce::dontSendNotification);
                subtitle.setFont (juce::Font (11.0f));
                subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (subtitle);

                for (auto* button : { &playButton, &stopButton, &recordButton, &clearButton })
                    addAndMakeVisible (*button);

                loopToggle.setButtonText ("Loop");
                addAndMakeVisible (loopToggle);

                tempo.setRange (40.0, 220.0, 1.0);
                tempo.setSliderStyle (juce::Slider::LinearHorizontal);
                tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
                tempo.setTextValueSuffix (" BPM");
                addAndMakeVisible (tempo);

                addAndMakeVisible (clipView);
                setSize (860, 380);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (10);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (18);
                title.setBounds (r.removeFromTop (24));
                subtitle.setBounds (r.removeFromTop (22));
                r.removeFromTop (8);

                auto controls = r.removeFromTop (34);
                playButton.setBounds (controls.removeFromLeft (74).reduced (2));
                stopButton.setBounds (controls.removeFromLeft (74).reduced (2));
                recordButton.setBounds (controls.removeFromLeft (92).reduced (2));
                clearButton.setBounds (controls.removeFromLeft (78).reduced (2));
                loopToggle.setBounds (controls.removeFromLeft (86).reduced (4, 0));
                controls.removeFromLeft (12);
                tempo.setBounds (controls.removeFromLeft (220));

                r.removeFromTop (10);
                clipView.setBounds (r);
            }

            TestPage& page;
            TestPage::ClipView clipView;
            juce::Label title, subtitle;
            juce::TextButton playButton { "Play" };
            juce::TextButton stopButton { "Stop" };
            juce::TextButton recordButton { "Arm Rec" };
            juce::TextButton clearButton { "Clear" };
            juce::ToggleButton loopToggle;
            juce::Slider tempo;
        };

        auto* content = new ClipModalContent (*this);
        content->playButton.onClick = [this] { onTransportPlayPressed(); };
        content->stopButton.onClick = [this] { onTransportStopPressed(); };
        content->recordButton.setClickingTogglesState (true);
        content->recordButton.setToggleState (recording.load(), juce::dontSendNotification);
        content->recordButton.onClick = [this, content]
        {
            recordBtn.setToggleState (content->recordButton.getToggleState(), juce::dontSendNotification);
            onTransportRecordPressed();
        };
        content->clearButton.onClick = [this] { clearClip(); };
        content->loopToggle.setToggleState (looping.load(), juce::dontSendNotification);
        content->loopToggle.onClick = [this, content] { looping.store (content->loopToggle.getToggleState()); };
        content->tempo.setValue (bpm.load(), juce::dontSendNotification);
        content->tempo.onValueChange = [this, content]
        {
            bpm.store (content->tempo.getValue());
            tempoSlider.setValue (content->tempo.getValue(), juce::dontSendNotification);
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "MIDI Clip Editor";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (content);
        options.launchAsync();
    }

    void TestPage::activate()
    {
        if (audioRunning) return;

        ensureEngineMatchesProject();
        syncInstrumentRendererFromDesigner();
        syncRoutingFromProject();
        syncAllValuesToEngine();

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            statusLabel.setText ("No audio device: " + error, juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        auto& deviceManager = owner.getAudio().getDeviceManager();
        for (const auto& input : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        deviceManager.addMidiInputDeviceCallback ({}, this);
        deviceManager.addAudioCallback (this);
        audioRunning = true;

        refreshPlaybackStatus();
    }

    void TestPage::deactivate()
    {
        if (! audioRunning) return;

        auto& deviceManager = owner.getAudio().getDeviceManager();
        deviceManager.removeMidiInputDeviceCallback ({}, this);
        deviceManager.removeAudioCallback (this);

        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            if (engine != nullptr)
            {
                arpeggiator.allNotesOff (*engine);
                engine->allNotesOff();
            }
        }
        {
            const juce::ScopedLock hardwareMidiGuard (hardwareMidiLock);
            hardwareMidiBuffer.clear();
        }

        audioRunning = false;
        playing.store (false);
        recording.store (false);
        recordBtn.setToggleState (false, juce::dontSendNotification);
        statusLabel.setText ("Audio idle", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
    }

    void TestPage::ensureEngineMatchesProject()
    {
        auto wanted = owner.getProject().getEngineType();
        if (wanted.isEmpty())
            wanted = "synth";

        if (engine != nullptr && engineId == wanted)
        {
            engine->loadFromPack (owner.getProject().getProjectFolder(), owner.getProject().getSampleMap());
            return;
        }

        auto newEngine = createEngineFromManifest (wanted);
        if (newEngine == nullptr)
            newEngine = createEngineFromManifest ("synth");

        newEngine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
        newEngine->setRenderContext (makeRenderContext (currentBlockSize, 0, currentNumChans));
        newEngine->loadFromPack (owner.getProject().getProjectFolder(), owner.getProject().getSampleMap());

        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine = std::move (newEngine);
        engineId = wanted;
        syncRoutingFromProject();
    }

    void TestPage::syncAllValuesToEngine()
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine == nullptr) return;

        const auto projectTempo = owner.getProject().getLiveValues().getValue ("projectBpm", 120.0f);
        bpm.store (juce::jlimit (40.0, 220.0, (double) projectTempo));
        tempoSlider.setValue (bpm.load(), juce::dontSendNotification);

        for (const auto& def : owner.getProject().getParameters().getAll())
        {
            const auto value = owner.getProject().getLiveValues().getValue (def.id, def.defaultValue);
            engine->setParameter (def.id, value);
            routingEngine.setParameterValue (def.id, value);
            routingEngine.setFxBlockParameterValue (def.id, value);
        }
    }

    void TestPage::syncRoutingFromProject (bool preserveActiveNotes)
    {
        if (! preserveActiveNotes && engine != nullptr)
            arpeggiator.allNotesOff (*engine);
        arpeggiator.bind (owner.getProject().getDspGraph());
        routingEngine.bind (owner.getProject().getDspGraph(), owner.getProject().getParameters());
        routingEngine.prepare (makeRenderContext (currentBlockSize, 0, currentNumChans));
        routingEngine.syncFromLiveValues (owner.getProject().getLiveValues());
    }

    void TestPage::projectChanged()
    {
        projectChanged (PatchCraftProject::ChangeScope::structural);
    }

    void TestPage::projectChanged (PatchCraftProject::ChangeScope scope)
    {
        if (scope != PatchCraftProject::ChangeScope::dspRealtime || engine == nullptr
            || engineId != owner.getProject().getEngineType())
            ensureEngineMatchesProject();
        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            syncRoutingFromProject (scope == PatchCraftProject::ChangeScope::dspRealtime);
        }
        if (scope == PatchCraftProject::ChangeScope::dspRealtime)
        {
            repaint();
            return;
        }

        syncAllValuesToEngine();
        syncInstrumentRendererFromDesigner();
        refreshPlaybackStatus();
        repaint();
    }

    void TestPage::liveValueChanged (const juce::String& id, float value)
    {
        if (id == "projectBpm")
        {
            const auto tempo = juce::jlimit (40.0, 220.0, (double) value);
            bpm.store (tempo);
            tempoSlider.setValue (tempo, juce::dontSendNotification);
        }

        if (id.startsWith ("arpLane"))
        {
            applyArpLaneLiveControlToGraph (owner.getProject(), id);
            const juce::SpinLock::ScopedLockType lock (engineLock);
            syncRoutingFromProject (true);
            if (engine != nullptr)
                engine->setParameter (id, value);
            return;
        }

        const juce::SpinLock::ScopedLockType lock (engineLock);
        routingEngine.setParameterValue (id, value);
        routingEngine.setFxBlockParameterValue (id, value);
        if (engine != nullptr)
            engine->setParameter (id, value);
    }

    void TestPage::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        currentSampleRate = device->getCurrentSampleRate();
        currentBlockSize = device->getCurrentBufferSizeSamples();
        currentNumChans = juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits());

        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine != nullptr)
        {
            engine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
            engine->setRenderContext (makeRenderContext (currentBlockSize,
                                                         device->getActiveInputChannels().countNumberOfSetBits(),
                                                         currentNumChans));
        }
        routingEngine.prepare (makeRenderContext (currentBlockSize,
                                                  device->getActiveInputChannels().countNumberOfSetBits(),
                                                  currentNumChans));
    }

    void TestPage::audioDeviceStopped()
    {
        {
            const juce::ScopedLock hardwareMidiGuard (hardwareMidiLock);
            hardwareMidiBuffer.clear();
        }

        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine != nullptr)
        {
            arpeggiator.allNotesOff (*engine);
            engine->reset();
        }
    }

    void TestPage::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
    {
        if (outputs == nullptr || numOutputs <= 0) return;

        juce::AudioBuffer<float> buffer (outputs, numOutputs, numSamples);
        buffer.clear();

        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || engine == nullptr) return;

        if (engine->needsAudioInput() && inputs != nullptr)
        {
            for (int ch = 0; ch < juce::jmin (numOutputs, numInputs); ++ch)
                if (inputs[ch] != nullptr)
                    juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), inputs[ch], numSamples);
        }

        juce::MidiBuffer liveMidi;
        keyboardState.processNextMidiBuffer (liveMidi, 0, numSamples, true);
        {
            const juce::ScopedLock hardwareMidiGuard (hardwareMidiLock);
            liveMidi.addEvents (hardwareMidiBuffer, 0, numSamples, 0);
            hardwareMidiBuffer.clear();
        }

        for (const auto metadata : liveMidi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
            {
                if (! arpeggiator.handleNoteOn (*engine, message.getNoteNumber(), message.getFloatVelocity()))
                    engine->noteOn (message.getNoteNumber(), message.getFloatVelocity());
            }
            else if (message.isNoteOff())
            {
                if (! arpeggiator.handleNoteOff (*engine, message.getNoteNumber()))
                    engine->noteOff (message.getNoteNumber());
            }
        }

        const auto blockSeconds = numSamples / currentSampleRate;
        const auto oldPos = playPosSeconds.load();
        auto newPos = oldPos + blockSeconds;
        const auto length = clipLengthSeconds();

        if (playing.load())
        {
            const juce::ScopedLock clipGuard (clipLock);
            for (const auto& event : clip)
            {
                const auto noteOnTime = event.time;
                const auto noteOffTime = juce::jmin (length, event.time + event.length);
                const bool noteOnDue = newPos >= length
                    ? (noteOnTime >= oldPos || noteOnTime < std::fmod (newPos, length))
                    : (noteOnTime >= oldPos && noteOnTime < newPos);
                const bool noteOffDue = newPos >= length
                    ? (noteOffTime >= oldPos || noteOffTime < std::fmod (newPos, length))
                    : (noteOffTime >= oldPos && noteOffTime < newPos);

                if (noteOnDue)
                {
                    if (! arpeggiator.handleNoteOn (*engine, event.note, event.velocity))
                        engine->noteOn (event.note, event.velocity);
                }
                if (noteOffDue)
                {
                    if (! arpeggiator.handleNoteOff (*engine, event.note))
                        engine->noteOff (event.note);
                }
            }
        }

        const auto context = makeRenderContext (numSamples, numInputs, numOutputs);
        routingEngine.processToEngine (*engine, context);
        arpeggiator.process (*engine, context);
        engine->process (buffer, 0, numSamples);
        routingEngine.captureAudioAnalysis (buffer, 0, numSamples);

        peakL.store (peakForChannel (buffer, 0));
        peakR.store (peakForChannel (buffer, 1));
        if (instrumentRenderer != nullptr)
            instrumentRenderer->setAudioReactiveLevel (juce::jmax (peakL.load(), peakR.load()));
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = buffer.getSample (0, i);
            const float r = buffer.getNumChannels() > 1 ? buffer.getSample (1, i) : l;
            pushSpectrumSample ((l + r) * 0.5f);
        }

        if (newPos >= length)
            newPos = looping.load() ? std::fmod (newPos, length) : length;

        playPosSeconds.store (newPos);
        if (newPos >= length && ! looping.load())
            playing.store (false);
    }

    void TestPage::pushSpectrumSample (float sample)
    {
        if (fftFifoIndex == fftSize)
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fftFifo.begin(), fftFifo.end(), fftData.begin());
            forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

            const juce::SpinLock::ScopedLockType lock (spectrumLock);
            for (int i = 0; i < (int) spectrumBins.size(); ++i)
            {
                const auto start = juce::jmap (i, 0, (int) spectrumBins.size(), 1, fftSize / 2);
                const auto end = juce::jmap (i + 1, 0, (int) spectrumBins.size(), 1, fftSize / 2);
                float peak = 0.0f;
                for (int bin = start; bin < juce::jmax (start + 1, end); ++bin)
                    peak = juce::jmax (peak, fftData[(size_t) bin]);
                const float db = juce::Decibels::gainToDecibels (peak / (float) fftSize, -100.0f);
                const float norm = juce::jmap (juce::jlimit (-80.0f, 0.0f, db), -80.0f, 0.0f, 0.0f, 1.0f);
                spectrumBins[(size_t) i] = spectrumBins[(size_t) i] * 0.78f + norm * 0.22f;
            }
            fftFifoIndex = 0;
        }

        fftFifo[(size_t) fftFifoIndex++] = sample;
    }

    void TestPage::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        keyboardState.processNextMidiEvent (message);
        if (message.isNoteOn() || message.isNoteOff())
        {
            const juce::ScopedLock hardwareMidiGuard (hardwareMidiLock);
            hardwareMidiBuffer.addEvent (message, 0);
        }

        if (owner.captureMidiLearnMessage (message))
            return;

        if (recording.load() && message.isNoteOn())
        {
            if (! playing.load())
            {
                playPosSeconds.store (0.0);
                playing.store (true);
            }

            const juce::ScopedLock lock (clipLock);
            clip.push_back ({ playPosSeconds.load(), 0.25, message.getNoteNumber(), message.getFloatVelocity() });
            recordingNoteIndices[message.getNoteNumber()] = clip.size() - 1;
            selectedClipIndex = (int) clip.size() - 1;
        }
        else if (recording.load() && message.isNoteOff())
        {
            const juce::ScopedLock lock (clipLock);
            auto it = recordingNoteIndices.find (message.getNoteNumber());
            if (it != recordingNoteIndices.end() && it->second < clip.size())
            {
                auto& event = clip[it->second];
                event.length = juce::jmax (0.05, playPosSeconds.load() - event.time);
                recordingNoteIndices.erase (it);
            }
        }

        if (message.isController())
        {
            if (applyMidiMappings (message))
                return;

            const auto cc = message.getControllerNumber();
            const auto value01 = message.getControllerValue() / 127.0f;
            auto& project = owner.getProject();

            auto setIfPresent = [&] (const juce::String& parameterId, float value)
            {
                if (project.getParameters().find (parameterId) != nullptr
                    || project.getLiveValues().getRaw (parameterId) != nullptr)
                {
                    applyMidiLiveValueToEngine (parameterId, value);
                    enqueueMidiLiveValueUpdate (parameterId, value);
                }
            };

            if (cc == 1)
            {
                setIfPresent ("modWheel", value01);
                setIfPresent ("lfoAmount", value01);
                setIfPresent ("vibratoDepth", value01);
            }
            else if (cc == 11)
            {
                setIfPresent ("expression", value01);
            }
            else if (cc == 7)
            {
                setIfPresent ("volume", value01);
            }
            else if (cc == 10)
            {
                setIfPresent ("pan", juce::jmap (value01, -1.0f, 1.0f));
            }
            else if (cc == 64)
            {
                setIfPresent ("sustainPedal", value01);
            }
            else if (cc == 20)
            {
                setIfPresent ("sampleStart", value01);
            }
            else if (cc == 21)
            {
                setIfPresent ("sampleSlice", value01 * 63.0f);
            }
            else if (cc == 22)
            {
                setIfPresent ("sampleLength", 0.01f + value01 * 0.99f);
            }
            else if (cc == 23)
            {
                setIfPresent ("samplePitch", value01 * 48.0f - 24.0f);
            }
        }

        if (message.isPitchWheel())
        {
            if (applyMidiMappings (message))
                return;

            const auto bend = (message.getPitchWheelValue() - 8192) / 8192.0f;
            if (owner.getProject().getParameters().find ("pitchWheel") != nullptr)
            {
                applyMidiLiveValueToEngine ("pitchWheel", bend);
                enqueueMidiLiveValueUpdate ("pitchWheel", bend);
            }
            if (owner.getProject().getParameters().find ("detune") != nullptr)
            {
                const auto detuneCents = bend * 100.0f;
                applyMidiLiveValueToEngine ("detune", detuneCents);
                enqueueMidiLiveValueUpdate ("detune", detuneCents);
            }
        }

        if (message.isAftertouch() || message.isChannelPressure())
        {
            if (applyMidiMappings (message))
                return;
            const auto pressure = message.isAftertouch()
                ? (float) message.getAfterTouchValue() / 127.0f
                : (float) message.getChannelPressureValue() / 127.0f;
            if (owner.getProject().getParameters().find ("aftertouch") != nullptr)
            {
                applyMidiLiveValueToEngine ("aftertouch", pressure);
                enqueueMidiLiveValueUpdate ("aftertouch", pressure);
            }
        }
    }

    void TestPage::handleNoteOn (juce::MidiKeyboardState*, int, int, float)
    {
        if (! audioRunning && juce::MessageManager::getInstance()->isThisTheMessageThread())
            activate();
    }

    void TestPage::handleNoteOff (juce::MidiKeyboardState*, int, int, float)
    {
    }

    bool TestPage::applyMidiMappings (const juce::MidiMessage& message)
    {
        bool handled = false;
        auto& project = owner.getProject();

        for (const auto& mapping : project.getMidiMappings())
        {
            if (! mapping.matches (message))
                continue;
            if (project.getParameters().find (mapping.parameterId) == nullptr)
                continue;

            const float normalised = mapping.normalisedValueFromMessage (message);
            const float value = mapping.bipolar
                ? juce::jmap (normalised, -1.0f, 1.0f, mapping.targetMin, mapping.targetMax)
                : juce::jmap (normalised, 0.0f, 1.0f, mapping.targetMin, mapping.targetMax);
            applyMidiLiveValueToEngine (mapping.parameterId, value);
            enqueueMidiLiveValueUpdate (mapping.parameterId, value);
            handled = true;
        }

        return handled;
    }

    void TestPage::applyMidiLiveValueToEngine (const juce::String& parameterId, float value)
    {
        const juce::SpinLock::ScopedLockType engineGuard (engineLock);
        routingEngine.setParameterValue (parameterId, value);
        routingEngine.setFxBlockParameterValue (parameterId, value);

        if (engine != nullptr)
            engine->setParameter (parameterId, value);
    }

    void TestPage::enqueueMidiLiveValueUpdate (const juce::String& parameterId, float value)
    {
        {
            const juce::ScopedLock lock (pendingMidiLiveValueLock);
            pendingMidiLiveValues[parameterId] = value;
        }

        schedulePendingMidiLiveFlush();
    }

    void TestPage::schedulePendingMidiLiveFlush()
    {
        if (midiLiveFlushPending.exchange (true))
            return;

        juce::Component::SafePointer<TestPage> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (auto* page = safeThis.getComponent())
                page->flushPendingMidiLiveValues();
        });
    }

    void TestPage::flushPendingMidiLiveValues()
    {
        std::map<juce::String, float> updates;

        {
            const juce::ScopedLock lock (pendingMidiLiveValueLock);
            updates.swap (pendingMidiLiveValues);
        }

        auto& liveValues = owner.getProject().getLiveValues();
        for (const auto& [parameterId, value] : updates)
            liveValues.setValue (parameterId, value);

        midiLiveFlushPending.store (false);

        {
            const juce::ScopedLock lock (pendingMidiLiveValueLock);
            if (! pendingMidiLiveValues.empty())
                schedulePendingMidiLiveFlush();
        }
    }

    void TestPage::syncInstrumentRendererFromDesigner()
    {
        if (instrumentRenderer == nullptr)
            return;

        if (auto* canvas = owner.getCanvasEditor())
            instrumentRenderer->syncFromDesignerState (*canvas);
        else
            instrumentRenderer->rebuild();
    }

    void TestPage::timerCallback()
    {
        peakL.store (peakL.load() * 0.86f);
        peakR.store (peakR.load() * 0.86f);
        if (audioRunning)
            refreshPlaybackStatus();
        repaint();
    }

    juce::String TestPage::makePlaybackStatusText()
    {
        juce::String prefix = "Audio on";
        if (auto* device = owner.getAudio().getDeviceManager().getCurrentAudioDevice())
            prefix << ": " << device->getName();

        const auto projectEngine = owner.getProject().getEngineType();
        const auto zoneCount = (int) owner.getProject().getSampleMap().getZones().size();
        if (zoneCount > 0 && projectEngine != "sample")
            return prefix + " | Engine mismatch: Sample Mapper has "
                 + juce::String (zoneCount) + " zone(s), but project engine is "
                 + projectEngine + ". Switch engine to Sampler.";

        juce::String diagnostic;
        {
            const juce::SpinLock::ScopedTryLockType lock (engineLock);
            if (lock.isLocked() && engine != nullptr)
                diagnostic = engine->getDiagnosticStatus();
        }

        if (diagnostic.isNotEmpty())
            return prefix + " | " + diagnostic;

        return prefix + " | Engine: " + (projectEngine.isNotEmpty() ? projectEngine : "synth");
    }

    void TestPage::refreshPlaybackStatus()
    {
        if (! audioRunning)
        {
            statusLabel.setText ("Audio idle", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            return;
        }

        const auto text = makePlaybackStatusText();
        statusLabel.setText (text, juce::dontSendNotification);
        const bool warning = text.containsIgnoreCase ("mismatch")
                          || text.containsIgnoreCase ("missing")
                          || text.containsIgnoreCase ("failed")
                          || text.containsIgnoreCase ("no zone")
                          || text.containsIgnoreCase ("0/");
        statusLabel.setColour (juce::Label::textColourId,
                               warning ? juce::Colour (0xffffc857)
                                       : PatchCraftLookAndFeel::accent());
    }

    void TestPage::clearClip()
    {
        const juce::ScopedLock lock (clipLock);
        clip.clear();
        recordingNoteIndices.clear();
        selectedClipIndex = -1;
        dragClipIndex = -1;
        playbackIndex = 0;
        playPosSeconds.store (0.0);
    }

    void TestPage::hardStop()
    {
        playing.store (false);
        recording.store (false);
        recordBtn.setToggleState (false, juce::dontSendNotification);
        playPosSeconds.store (0.0);
        recordingNoteIndices.clear();

        for (int channel = 1; channel <= 16; ++channel)
            keyboardState.allNotesOff (channel);

        {
            const juce::ScopedLock hardwareMidiGuard (hardwareMidiLock);
            hardwareMidiBuffer.clear();
        }

        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine != nullptr)
        {
            arpeggiator.allNotesOff (*engine);
            engine->allNotesOff();
            engine->reset();
            engine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
            engine->setRenderContext (makeRenderContext (currentBlockSize, 0, currentNumChans));
            routingEngine.reset();
            routingEngine.prepare (makeRenderContext (currentBlockSize, 0, currentNumChans));
        }
    }

    RenderContext TestPage::makeRenderContext (int numSamples, int numInputs, int numOutputs) const
    {
        const auto projectTempo = owner.getProject().getLiveValues().getValue ("projectBpm", (float) bpm.load());
        auto context = RenderContext::forBlock (currentSampleRate,
                                               numSamples,
                                               currentBlockSize,
                                               numInputs,
                                               numOutputs,
                                               projectTempo);
        const auto playSeconds = playPosSeconds.load();
        context.isPlaying = playing.load();
        context.isRecording = recording.load();
        context.timeInSeconds = playSeconds;
        context.timeInSamples = (std::int64_t) (playSeconds * context.sampleRate);
        context.ppqPosition = playSeconds * context.bpm / 60.0;
        context.ppqPositionOfLastBarStart = std::floor (context.ppqPosition / context.barLengthInBeats())
                                          * context.barLengthInBeats();
        return context;
    }

    double TestPage::clipLengthSeconds() const noexcept
    {
        return (60.0 / bpm.load()) * 4.0 * clipBars;
    }

    void TestPage::startPreviewPlayback()
    {
        onTransportPlayPressed();
    }

    void TestPage::stopPreviewPlayback()
    {
        onTransportStopPressed();
    }

    void TestPage::togglePreviewPlayback()
    {
        if (playing.load())
            onTransportStopPressed();
        else
            onTransportPlayPressed();
    }

    double TestPage::getSequencerPlaybackPosition01 (int steps) const noexcept
    {
        if (! playing.load())
            return arpeggiator.getPlaybackPosition01 (steps);

        const auto length = clipLengthSeconds();
        if (length <= 0.0)
            return arpeggiator.getPlaybackPosition01 (steps);

        return juce::jlimit (0.0, 1.0, std::fmod (playPosSeconds.load(), length) / length);
    }

    bool TestPage::setDrumActivePatternFromUi (int pattern)
    {
        pattern = juce::jlimit (0, 7, pattern);
        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureDrumMachineBlock (graph);
        block.values["dmPattern"] = (float) pattern;
        block.values["dmTransport"] = 1.0f;
        graph.userConfigured = true;
        owner.getProject().markDirty();

        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            syncRoutingFromProject();
        }

        if (instrumentRenderer != nullptr)
            instrumentRenderer->repaint();

        return true;
    }

    bool TestPage::setDrumPatternCellFromUi (int pattern, int track, int step, bool active,
                                             float velocity, float gate, float probability, int divisions)
    {
        pattern = juce::jlimit (0, 7, pattern);
        track = juce::jlimit (0, 15, track);
        step = juce::jlimit (0, 63, step);

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureDrumMachineBlock (graph);
        const int tracks = juce::jlimit (1, 16, juce::roundToInt (blockValue (block, "dmTracks", 8.0f)));
        const int steps = juce::jlimit (1, 64, juce::roundToInt (blockValue (block, "dmSteps", 16.0f)));
        if (track >= tracks || step >= steps)
            return false;

        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        block.values["dmPattern"] = (float) pattern;
        block.values["dmTransport"] = 1.0f;
        block.values[prefix + "On"] = active ? 1.0f : 0.0f;
        if (active)
        {
            block.values[prefix + "Vel"] = juce::jlimit (0.08f, 1.0f, velocity);
            block.values[prefix + "Gate"] = juce::jlimit (0.05f, 1.0f, gate);
            block.values[prefix + "Prob"] = juce::jlimit (0.0f, 1.0f, probability);
            block.values[prefix + "Div"] = (float) juce::jlimit (1, 4, divisions);
        }

        graph.userConfigured = true;
        owner.getProject().markDirty();

        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            syncRoutingFromProject();
        }

        return true;
    }

    bool TestPage::setArpLaneStepFromUi (int lane, int step, float velocity, bool active)
    {
        lane = juce::jlimit (0, 15, lane);
        step = juce::jlimit (0, 127, step);
        velocity = juce::jlimit (0.05f, 1.0f, velocity);

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureArpBlock (graph);
        const int steps = juce::jlimit (1, 128, juce::roundToInt (blockValue (block, "arpSteps", 16.0f)));
        if (step >= steps)
            return false;

        const auto keyStep = juce::String (step);
        const auto setLaneValue = [&] (const juce::String& key, float value)
        {
            if (lane == 0)
                block.values[key] = value;

            block.values["mpBank" + juce::String (lane + 1) + "_" + key] = value;
        };

        block.values["mpActiveBank"] = (float) lane;
        block.values["mpMultiLane"] = 1.0f;
        setLaneValue ("mpStep" + keyStep + "On", active ? 1.0f : 0.0f);
        setLaneValue ("mpVelocity" + keyStep, velocity);
        setLaneValue ("mpGate" + keyStep, 0.72f);
        setLaneValue ("mpStepProb" + keyStep, 1.0f);
        setLaneValue ("mpStepDiv" + keyStep, 1.0f);
        owner.getProject().getLiveValues().setValue ("arpLaneControlBank", (float) juce::jlimit (0, 4, lane));
        if (step < 16)
            owner.getProject().getLiveValues().setValue ("arpLaneStep" + juce::String (step + 1), velocity);
        graph.userConfigured = true;
        owner.getProject().markDirty();

        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            syncRoutingFromProject();
        }

        return true;
    }

    void TestPage::onTransportPlayPressed()
    {
        activate();
        playing.store (true);
    }

    void TestPage::onTransportStopPressed()
    {
        hardStop();
    }

    void TestPage::onTransportRecordPressed()
    {
        activate();
        recording.store (recordBtn.getToggleState());
        if (recording.load())
        {
            playing.store (false);
            playPosSeconds.store (0.0);
        }
    }


    void TestPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 0, getWidth(), 1);
    }

    void TestPage::resized()
    {
        auto r = getLocalBounds().reduced (12);

        const bool showDiagnostics = ! brandLabPreviewMode;
        for (auto* component : {
                 static_cast<juce::Component*> (&transportLabel),
                 static_cast<juce::Component*> (&playBtn),
                 static_cast<juce::Component*> (&stopBtn),
                 static_cast<juce::Component*> (&recordBtn),
                 static_cast<juce::Component*> (&loopToggle),
                 static_cast<juce::Component*> (&tempoSlider),
                 static_cast<juce::Component*> (&clearClipBtn),
                 static_cast<juce::Component*> (&clipLabel),
                 static_cast<juce::Component*> (clipView.get()),
                 static_cast<juce::Component*> (&metersLabel),
                 static_cast<juce::Component*> (meterView.get()),
                 static_cast<juce::Component*> (&spectrumLabel),
                 static_cast<juce::Component*> (spectrumView.get()),
                 static_cast<juce::Component*> (&monitorLabel),
                 static_cast<juce::Component*> (paramMonitor.get()),
                 static_cast<juce::Component*> (&statusLabel),
                 static_cast<juce::Component*> (&keyboardLabel) })
            if (component != nullptr)
                component->setVisible (showDiagnostics);

        if (brandLabPreviewMode)
        {
            const int keyboardHeight = juce::jlimit (62, 86, getHeight() / 8);
            auto keyboardSlot = r.removeFromBottom (keyboardHeight);
            brandPreviewInstrumentBounds = fitCanvasIntoBounds (owner.getProject().getCanvasSize(), r);
            auto keyboardArea = keyboardSlot.withX (brandPreviewInstrumentBounds.getX())
                                            .withWidth (brandPreviewInstrumentBounds.getWidth())
                                            .reduced (0, 4);

            if (keyboard != nullptr)
                keyboard->setBounds (keyboardArea);

            if (instrumentRenderer != nullptr)
                instrumentRenderer->setBounds (brandPreviewInstrumentBounds);

            return;
        }

        auto top = r.removeFromTop (34);
        transportLabel.setBounds (top.removeFromLeft (110));
        playBtn.setBounds (top.removeFromLeft (70).reduced (2));
        stopBtn.setBounds (top.removeFromLeft (70).reduced (2));
        recordBtn.setBounds (top.removeFromLeft (82).reduced (2));
        loopToggle.setBounds (top.removeFromLeft (78).reduced (4, 0));
        top.removeFromLeft (12);
        tempoSlider.setBounds (top.removeFromLeft (190));
        clearClipBtn.setBounds (top.removeFromLeft (72).reduced (2));
        statusLabel.setBounds (top.reduced (10, 0));

        r.removeFromTop (8);
        auto main = r.removeFromTop (juce::jmax (320, r.getHeight() * 2 / 3));
        auto right = main.removeFromRight (280);
        main.removeFromRight (10);

        if (instrumentRenderer != nullptr)
            instrumentRenderer->setBounds (main);

        metersLabel.setBounds (right.removeFromTop (22));
        meterView->setBounds (right.removeFromTop (76));
        right.removeFromTop (10);
        spectrumLabel.setBounds (right.removeFromTop (22));
        spectrumView->setBounds (right.removeFromTop (112));
        right.removeFromTop (10);
        monitorLabel.setBounds (right.removeFromTop (22));
        paramMonitor->setBounds (right);

        r.removeFromTop (8);
        auto clipArea = r.removeFromLeft (juce::jmax (280, r.getWidth() / 2));
        clipArea.removeFromRight (8);
        clipLabel.setBounds (clipArea.removeFromTop (22));
        clipView->setBounds (clipArea);

        keyboardLabel.setBounds (r.removeFromTop (22));
        keyboard->setBounds (r);
    }

    bool TestPage::keyPressed (const juce::KeyPress& key, juce::Component*)
    {
        // Delegate to ClipView for keyboard handling
        return clipView->keyPressed (key, nullptr);
    }
}
