#include "StudioInstrumentRenderer.h"
#include "CanvasEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "MidiPlaygroundPattern.h"

#include <cmath>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static bool parameterIsEnabledForRenderer (const PatchCraftProject& project, const ParameterDef& parameter)
        {
            if (parameter.enabledBy.isEmpty())
                return true;

            const auto* gate = project.getParameters().find (parameter.enabledBy);
            const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
            const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
            return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
        }

        constexpr int kPianoFirstMidiNote = 21;  // A0
        constexpr int kPianoLastMidiNote  = 108; // C8

        static bool isBlackPianoKey (int midiNote)
        {
            const int semitone = ((midiNote % 12) + 12) % 12;
            return semitone == 1 || semitone == 3 || semitone == 6 || semitone == 8 || semitone == 10;
        }

        static std::vector<int> pianoWhiteNotes()
        {
            std::vector<int> notes;
            notes.reserve (52);
            for (int note = kPianoFirstMidiNote; note <= kPianoLastMidiNote; ++note)
                if (! isBlackPianoKey (note))
                    notes.push_back (note);
            return notes;
        }

        static juce::String orbitLaneSoundName (int sound)
        {
            return "DSP Slot " + juce::String (juce::jlimit (0, 15, sound) + 1);
        }

        static juce::String circleSeqPatternName (int preset)
        {
            static const char* names[] =
            {
                "Pentatonic Pulse", "Bass Anchor", "Melody Answer", "Bell Topline",
                "Soft Syncopation", "Arp Climb", "Open Fifths", "Reset Empty"
            };
            return names[(size_t) juce::jlimit (0, 7, preset)];
        }

        static juce::String arpLaneMidiFileName (juce::String instrumentName, juce::String laneName, int lane)
        {
            if (instrumentName.trim().isEmpty())
                instrumentName = "PatchCraft";
            if (laneName.trim().isEmpty())
                laneName = "ArpLane" + juce::String (lane + 1);

            auto name = juce::File::createLegalFileName (instrumentName.trim()
                + "_" + laneName.trim() + "_midi");
            return name.replaceCharacter (' ', '_') + ".mid";
        }

        static juce::String displayValueForParameter (const PatchCraftProject& project, const juce::String& parameterId)
        {
            const auto* def = project.getParameters().find (parameterId);
            const auto value = project.getLiveValues().getValue (parameterId, def != nullptr ? def->defaultValue : 0.0f);
            const int stepped = juce::roundToInt (value);

            if (parameterId == "arpLaneMode")
                return stepped == 1 ? "Performance" : "Bank";
            if (parameterId == "arpLaneTarget")
            {
                static const char* values[] = { "Notes", "Drums", "One Shots", "Loops", "Samples" };
                return values[(size_t) juce::jlimit (0, 4, stepped)];
            }
            if (parameterId == "arpLaneDirection")
            {
                static const char* values[] = { "Forward", "Reverse", "Bounce", "Random" };
                return values[(size_t) juce::jlimit (0, 3, stepped)];
            }
            if (parameterId == "arpLaneControlBank")
                return "Lane " + juce::String (juce::jlimit (0, 15, stepped) + 1);
            if (parameterId == "arpLaneGroup")
                return "Group " + juce::String (juce::jlimit (0, 7, stepped) + 1);
            if (parameterId == "arpLaneSliderRole")
            {
                static const char* values[] = { "Velocity", "Gate", "Chance", "Ratchet", "On/Off", "Delay", "Slice", "Transpose", "Filter", "Pan", "FX Send" };
                return values[(size_t) juce::jlimit (0, 10, stepped)];
            }
            if (parameterId == "arpLaneSound")
                return orbitLaneSoundName (stepped);
            if (parameterId == "arpLaneFxTarget")
            {
                static const char* values[] = { "Delay", "Reverb", "Chorus", "Phaser", "Drive", "Resonance", "Width", "Tape" };
                return values[(size_t) juce::jlimit (0, 7, stepped)];
            }
            if (parameterId == "arpLanePatternLaunch")
                return circleSeqPatternName (stepped);

            return def != nullptr && def->unit.isNotEmpty()
                ? juce::String (value, 2) + " " + def->unit
                : juce::String (value, 2);
        }

        static int whiteNotesBefore (int midiNote)
        {
            int count = 0;
            for (int note = kPianoFirstMidiNote; note < midiNote; ++note)
                if (! isBlackPianoKey (note))
                    ++count;
            return count;
        }

        static juce::String disconnectedControlMessage (const PatchCraftProject& project,
                                                        const LayoutElement& element,
                                                        const ParameterDef* parameter)
        {
            if (element.parameterId.isEmpty())
                return "This control is not connected to a parameter.\nDesign page: select it, open Inspector > DSP Assignment/Parameter, then choose a target parameter.";

            if (parameter == nullptr)
                return "This control points to missing parameter '" + element.parameterId + "'.\nReconnect it in the Inspector or drag a valid DSP Quick Edit parameter onto the canvas.";

            if (! parameterIsEnabledForRenderer (project, *parameter))
                return "This control is currently disabled: "
                    + (parameter->enableHint.isNotEmpty() ? parameter->enableHint
                                                          : ("enable " + parameter->enabledBy + " first."))
                    + "\nConnect or raise the enabling parameter, then this control will move and affect sound.";

            return parameter->name + " (" + parameter->id + ")\nConnected and active.";
        }

        static float blockValue (const DspBlock& block, const juce::String& key, float fallback)
        {
            const auto it = block.values.find (key);
            return it != block.values.end() ? it->second : fallback;
        }

        static float eqFrequencyToX01 (float frequency)
        {
            const float f = juce::jlimit (20.0f, 20000.0f, frequency);
            return juce::jlimit (0.0f, 1.0f,
                std::log (f / 20.0f) / std::log (20000.0f / 20.0f));
        }

        static float eqGainToY01 (float gainDb)
        {
            return juce::jlimit (0.0f, 1.0f, juce::jmap (gainDb, 24.0f, -24.0f, 0.0f, 1.0f));
        }

        static void drawEqCurveElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& element,
                                        const DspGraph& graph)
        {
            const auto bg = element.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : element.accentColour;
            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, element.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, element.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (20);
            g.setColour (accent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "EQ CURVE",
                        header.removeFromLeft (130), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (9.0f);
            g.drawText ("live patch EQ", header, juce::Justification::centredRight, true);

            auto graphArea = area.reduced (2, 4);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.55f));
            g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);
            g.setColour (border.withAlpha (0.22f));
            for (int i = 1; i < 5; ++i)
            {
                const int x = graphArea.getX() + (graphArea.getWidth() * i) / 5;
                const int y = graphArea.getY() + (graphArea.getHeight() * i) / 5;
                g.drawVerticalLine (x, (float) graphArea.getY(), (float) graphArea.getBottom());
                g.drawHorizontalLine (y, (float) graphArea.getX(), (float) graphArea.getRight());
            }

            juce::Path curve;
            for (int i = 0; i < 96; ++i)
            {
                const float x01 = (float) i / 95.0f;
                float y = (float) graphArea.getCentreY();
                for (const auto& block : graph.blocks)
                {
                    if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                        continue;
                    const float freqX = eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f));
                    const float gain = blockValue (block, "eqGainDb", 0.0f);
                    const float q = juce::jlimit (0.15f, 18.0f, blockValue (block, "eqQ", 1.0f));
                    const float width = juce::jlimit (0.025f, 0.22f, 0.11f / std::sqrt (q));
                    const float influence = std::exp (-std::pow ((x01 - freqX) / width, 2.0f));
                    y -= (gain / 24.0f) * influence * (float) graphArea.getHeight() * 0.44f;
                }
                const float x = (float) graphArea.getX() + x01 * (float) graphArea.getWidth();
                y = juce::jlimit ((float) graphArea.getY(), (float) graphArea.getBottom(), y);
                if (i == 0) curve.startNewSubPath (x, y);
                else        curve.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.92f));
            g.strokePath (curve, juce::PathStrokeType (2.0f));

            for (const auto& block : graph.blocks)
            {
                if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                    continue;
                const float x = (float) graphArea.getX() + eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f)) * (float) graphArea.getWidth();
                const float y = (float) graphArea.getY() + eqGainToY01 (blockValue (block, "eqGainDb", 0.0f)) * (float) graphArea.getHeight();
                g.setColour (accent);
                g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            }
        }

        static void drawSpectrumElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& element,
                                         float outputPeak)
        {
            const auto bg = element.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? juce::Colour (0xff20d6ff) : element.accentColour;
            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, element.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, element.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (20);
            g.setColour (accent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "SPECTRUM",
                        header.removeFromLeft (130), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (9.0f);
            g.drawText ("runtime analyzer", header, juce::Justification::centredRight, true);
            auto graphArea = area.reduced (2, 4);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.55f));
            g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);
            const float level = juce::jlimit (0.08f, 1.0f, outputPeak + 0.14f);
            constexpr int bars = 36;
            for (int i = 0; i < bars; ++i)
            {
                const float x01 = (float) i / (float) (bars - 1);
                const float h01 = level * (0.18f + 0.72f * std::abs (std::sin (x01 * 9.0f + (float) i * 0.37f)));
                const int barW = juce::jmax (2, graphArea.getWidth() / bars - 2);
                const int x = graphArea.getX() + i * graphArea.getWidth() / bars;
                const int h = juce::roundToInt (h01 * (float) graphArea.getHeight());
                g.setColour (accent.interpolatedWith (PatchCraftLookAndFeel::accent(), x01).withAlpha (0.78f));
                g.fillRoundedRectangle ((float) x, (float) graphArea.getBottom() - (float) h,
                                        (float) barW, (float) h, 2.0f);
            }
        }

        static const DspBlock* findDrumMachineBlock (const DspGraph& graph)
        {
            for (const auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("drum") || block.values.find ("dmTracks") != block.values.end())
                    return &block;
            return nullptr;
        }

        static const DspBlock* findArpBlock (const DspGraph& graph)
        {
            for (const auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("arp")
                    || block.type.containsIgnoreCase ("midi")
                    || block.values.find ("arpSteps") != block.values.end())
                    return &block;
            return nullptr;
        }

        static float arpLaneValue (const DspBlock& block, int bank, const juce::String& key, float fallback)
        {
            const auto bankPrefix = "mpBank" + juce::String (juce::jlimit (0, 15, bank) + 1) + "_";
            if (bank > 0)
            {
                const auto bankIt = block.values.find (bankPrefix + key);
                if (bankIt != block.values.end())
                    return bankIt->second;
            }

            const auto directIt = block.values.find (key);
            if (directIt != block.values.end())
                return directIt->second;

            const auto bankOneIt = block.values.find (bankPrefix + key);
            return bankOneIt != block.values.end() ? bankOneIt->second : fallback;
        }

        static void drawArpLanePreview (juce::Graphics& g,
                                        juce::Rectangle<int> r,
                                        const LayoutElement& element,
                                        const DspGraph& graph,
                                        double playback01)
        {
            const auto* block = findArpBlock (graph);
            const int lane = juce::jlimit (0, 15, element.arpLaneIndex);
            const int activeLane = block != nullptr ? juce::jlimit (0, 15, juce::roundToInt (blockValue (*block, "mpActiveBank", 0.0f))) : 0;
            const bool laneSelected = lane == activeLane;
            const bool orbitMultiRing = element.arpLaneMode.equalsIgnoreCase ("multiRing")
                                     || element.arpLaneMode.equalsIgnoreCase ("orbit")
                                     || element.arpLaneMode.equalsIgnoreCase ("orbitMulti");
            const int steps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) element.arpLaneSteps)))
                : juce::jlimit (1, 128, element.arpLaneSteps);
            const int maxDrawSteps = juce::jmin (steps, 64);
            const double displayPlayback01 = playback01 >= 0.0
                ? playback01
                : std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.00010 + (double) lane * 0.071, 1.0);
            const int playbackStep = juce::jlimit (0, maxDrawSteps - 1,
                (int) std::floor (displayPlayback01 * (double) maxDrawSteps));

            const auto bg = element.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : element.accentColour;

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, element.cornerRadius));
            g.setColour (laneSelected ? accent : border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, element.cornerRadius), laneSelected ? 2.0f : 1.0f);

            auto area = r.reduced (10);
            auto header = area.removeFromTop (24);
            auto dragHandle = header.removeFromRight (72).reduced (2, 3);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (juce::String (lane + 1) + "  " + (element.label.isNotEmpty() ? element.label.toUpperCase() : "ARP LANE"),
                        header, juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.88f));
            g.fillRoundedRectangle (dragHandle.toFloat(), 5.0f);
            g.setColour (accent.withAlpha (0.78f));
            g.drawRoundedRectangle (dragHandle.toFloat().reduced (0.5f), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (7.4f, juce::Font::bold));
            g.drawText ("DRAG MIDI", dragHandle, juce::Justification::centred, true);

            if (orbitMultiRing)
            {
                const int laneCount = 5;
                const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
                if (size <= 24.0f)
                    return;

                const juce::Point<float> centre ((float) area.getCentreX(), (float) area.getY() + size * 0.52f);
                const float radius = size * 0.42f;
                const float innerRadius = radius * 0.25f;
                const float outerRadius = radius * 0.94f;
                const float band = (outerRadius - innerRadius) / (float) laneCount;
                const int activeSteps = block != nullptr
                    ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, activeLane, "arpSteps", (float) element.arpLaneSteps)))
                    : juce::jlimit (1, 128, element.arpLaneSteps);
                const double ringPlayback01 = playback01 >= 0.0
                    ? playback01
                    : std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.00010 + (double) activeLane * 0.071, 1.0);
                const bool realPlayback = playback01 >= 0.0;

                g.setColour (border.withAlpha (0.22f));
                for (int spoke = 0; spoke < 16; ++spoke)
                {
                    const float angle = -juce::MathConstants<float>::halfPi
                        + juce::MathConstants<float>::twoPi * (float) spoke / 16.0f;
                    const auto start = centre + juce::Point<float> (std::cos (angle) * innerRadius,
                                                                    std::sin (angle) * innerRadius);
                    const auto end = centre + juce::Point<float> (std::cos (angle) * outerRadius,
                                                                  std::sin (angle) * outerRadius);
                    g.drawLine (start.x, start.y, end.x, end.y, spoke % 4 == 0 ? 1.0f : 0.55f);
                }

                for (int ringLane = 0; ringLane < laneCount; ++ringLane)
                {
                    const int laneToDraw = ringLane;
                    const int laneSteps = block != nullptr
                        ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, laneToDraw, "arpSteps", (float) element.arpLaneSteps)))
                        : juce::jlimit (1, 128, element.arpLaneSteps);
                    const int drawSteps = juce::jmin (laneSteps, 64);
                    const float ringRadius = innerRadius + band * ((float) ringLane + 0.5f);
                    const bool activeRing = laneToDraw == activeLane;
                    const auto ringColour = activeRing ? accent : accent.interpolatedWith (border, 0.45f + 0.08f * (float) ringLane);

                    g.setColour ((activeRing ? ringColour : border).withAlpha (activeRing ? 0.82f : 0.28f));
                    g.drawEllipse (centre.x - ringRadius, centre.y - ringRadius,
                                   ringRadius * 2.0f, ringRadius * 2.0f, activeRing ? 2.0f : 0.9f);

                    for (int stepIndex = 0; stepIndex < drawSteps; ++stepIndex)
                    {
                        const float active = block != nullptr
                            ? arpLaneValue (*block, laneToDraw, "mpStep" + juce::String (stepIndex) + "On", stepIndex % 2 == 0 ? 1.0f : 0.0f)
                            : (stepIndex % 3 == 0 ? 1.0f : 0.0f);
                        const float velocity = block != nullptr
                            ? juce::jlimit (0.05f, 1.0f, arpLaneValue (*block, laneToDraw, "mpVelocity" + juce::String (stepIndex), 0.74f))
                            : 0.72f;
                        const float angle = -juce::MathConstants<float>::halfPi
                            + juce::MathConstants<float>::twoPi * (float) stepIndex / (float) drawSteps;
                        const float rOffset = juce::jmap (velocity, 0.0f, 1.0f, -band * 0.34f, band * 0.34f);
                        const auto p = centre + juce::Point<float> (std::cos (angle) * (ringRadius + rOffset),
                                                                    std::sin (angle) * (ringRadius + rOffset));

                        const float dot = 3.0f + velocity * 3.2f;
                        if (active >= 0.5f)
                        {
                            g.setColour (ringColour.withAlpha ((activeRing ? 0.86f : 0.42f) * (0.5f + velocity * 0.5f)));
                            g.drawLine (centre.x, centre.y, p.x, p.y, activeRing ? 1.15f : 0.75f);
                            g.fillEllipse (p.x - dot, p.y - dot, dot * 2.0f, dot * 2.0f);
                        }
                        else
                        {
                            g.setColour (border.withAlpha (activeRing ? 0.22f : 0.12f));
                            g.drawEllipse (p.x - 2.4f, p.y - 2.4f, 4.8f, 4.8f, 0.8f);
                        }
                    }
                }

                juce::ignoreUnused (activeSteps);
                {
                    const float playheadAngle = -juce::MathConstants<float>::halfPi
                        + juce::MathConstants<float>::twoPi * (float) std::fmod (ringPlayback01, 1.0);
                    const auto dot = centre + juce::Point<float> (std::cos (playheadAngle) * (outerRadius + band * 0.46f),
                                                                  std::sin (playheadAngle) * (outerRadius + band * 0.46f));
                    const auto tick0 = centre + juce::Point<float> (std::cos (playheadAngle) * (innerRadius - band * 0.10f),
                                                                    std::sin (playheadAngle) * (innerRadius - band * 0.10f));
                    const auto tick1 = centre + juce::Point<float> (std::cos (playheadAngle) * (outerRadius + band * 0.18f),
                                                                    std::sin (playheadAngle) * (outerRadius + band * 0.18f));
                    g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.70f : 0.38f));
                    g.drawLine (tick0.x, tick0.y, tick1.x, tick1.y, realPlayback ? 1.4f : 1.0f);
                    g.setColour (accent.withAlpha (realPlayback ? 0.92f : 0.58f));
                    g.fillEllipse (dot.x - 5.2f, dot.y - 5.2f, 10.4f, 10.4f);
                    g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.82f : 0.50f));
                    g.drawEllipse (dot.x - 5.2f, dot.y - 5.2f, 10.4f, 10.4f, 1.1f);
                }
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (juce::Font (21.0f, juce::Font::bold));
                g.drawText ("ORBIT", juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y - 22, 90, 24),
                            juce::Justification::centred, true);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (8.5f, juce::Font::bold));
                g.drawText ("LANE " + juce::String (activeLane + 1) + " ACTIVE",
                            juce::Rectangle<int> ((int) centre.x - 48, (int) centre.y + 3, 96, 18),
                            juce::Justification::centred, true);
                return;
            }

            const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
            const juce::Point<float> centre ((float) area.getCentreX(), (float) area.getY() + size * 0.52f);
            const float radius = size * 0.40f;
            const float innerRadius = radius * 0.70f;
            const int slotCount = juce::jlimit (1, 12, element.arpLaneSampleSlots);
            const bool multiRing = slotCount > 1 && element.arpLaneTarget != "notes";

            g.setColour (border.withAlpha (0.70f));
            g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
            g.setColour (border.withAlpha (0.20f));
            g.drawEllipse (centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);
            if (multiRing)
            {
                for (int slot = 0; slot < slotCount; ++slot)
                {
                    const float rr = juce::jmap ((float) slot, 0.0f, (float) juce::jmax (1, slotCount - 1),
                                                 radius * 0.32f, radius * 0.93f);
                    g.setColour (border.withAlpha (slot == 0 ? 0.28f : 0.16f));
                    g.drawEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f, 0.7f);
                }
            }

            for (int step = 0; step < maxDrawSteps; ++step)
            {
                const float active = block != nullptr
                    ? arpLaneValue (*block, lane, "mpStep" + juce::String (step) + "On", step % 2 == 0 ? 1.0f : 0.0f)
                    : (step % 3 == 0 ? 1.0f : 0.0f);
                const float velocity = block != nullptr
                    ? juce::jlimit (0.15f, 1.0f, arpLaneValue (*block, lane, "mpVelocity" + juce::String (step), 0.74f))
                    : 0.72f;
                const int divisions = block != nullptr
                    ? juce::jlimit (1, 8, juce::roundToInt (arpLaneValue (*block, lane, "mpStepDiv" + juce::String (step), 1.0f)))
                    : 1;
                const int slot = multiRing
                    ? juce::jlimit (0, slotCount - 1,
                        block != nullptr && element.arpLaneTarget == "loops"
                            ? juce::roundToInt (arpLaneValue (*block, lane, "mpSampleSlice" + juce::String (step), (float) (step % slotCount)))
                            : (step + element.arpLaneRotate) % slotCount)
                    : 0;
                const float angle = -juce::MathConstants<float>::halfPi
                    + juce::MathConstants<float>::twoPi * (float) step / (float) maxDrawSteps;
                const float activeRadius = multiRing
                    ? juce::jmap ((float) slot, 0.0f, (float) juce::jmax (1, slotCount - 1), radius * 0.32f, radius * 0.93f)
                    : juce::jmap (velocity, 0.0f, 1.0f, radius * 0.25f, radius * 0.93f);
                const auto outer = centre + juce::Point<float> (std::cos (angle) * radius,
                                                                std::sin (angle) * radius);
                const auto gridStart = centre + juce::Point<float> (std::cos (angle) * radius * 0.18f,
                                                                    std::sin (angle) * radius * 0.18f);
                const auto velocityEnd = centre + juce::Point<float> (std::cos (angle) * activeRadius,
                                                                      std::sin (angle) * activeRadius);
                g.setColour (border.withAlpha (0.20f));
                g.drawLine (gridStart.x, gridStart.y, outer.x, outer.y, 0.7f);
                if (active >= 0.5f)
                {
                    const float dotSize = (step == playbackStep ? 6.0f : 3.0f) + velocity * 4.0f;
                    g.setColour (accent.withAlpha (step == playbackStep ? 0.88f : 0.58f));
                    g.drawLine (centre.x, centre.y, velocityEnd.x, velocityEnd.y, step == playbackStep ? 2.2f : 1.35f);
                    g.setColour (accent.withAlpha (step == playbackStep ? 0.40f : 0.24f));
                    g.fillEllipse (velocityEnd.x - dotSize, velocityEnd.y - dotSize, dotSize * 2.0f, dotSize * 2.0f);
                    g.setColour (accent);
                    g.fillEllipse (velocityEnd.x - dotSize * 0.42f, velocityEnd.y - dotSize * 0.42f, dotSize * 0.84f, dotSize * 0.84f);
                    if (divisions > 1)
                    {
                        const auto badge = centre + juce::Point<float> (std::cos (angle) * radius * 1.06f,
                                                                        std::sin (angle) * radius * 1.06f);
                        g.setColour (bg.withAlpha (0.94f));
                        g.fillEllipse (badge.x - 6.0f, badge.y - 6.0f, 12.0f, 12.0f);
                        g.setColour (accent);
                        g.drawEllipse (badge.x - 6.0f, badge.y - 6.0f, 12.0f, 12.0f, 1.0f);
                        g.setFont (juce::Font (7.0f, juce::Font::bold));
                        g.drawText (juce::String (divisions),
                                    juce::Rectangle<int> ((int) badge.x - 6, (int) badge.y - 6, 12, 12),
                                    juce::Justification::centred, true);
                    }
                }
            }

            {
                const float playheadAngle = -juce::MathConstants<float>::halfPi
                    + juce::MathConstants<float>::twoPi * (float) std::fmod (displayPlayback01, 1.0);
                const auto playheadInner = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 0.18f,
                                                                        std::sin (playheadAngle) * radius * 0.18f);
                const auto playheadOuter = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 1.04f,
                                                                        std::sin (playheadAngle) * radius * 1.04f);
                const auto playheadDot = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 1.10f,
                                                                      std::sin (playheadAngle) * radius * 1.10f);
                g.setColour (juce::Colours::white.withAlpha (playback01 >= 0.0 ? 0.72f : 0.42f));
                g.drawLine (playheadInner.x, playheadInner.y, playheadOuter.x, playheadOuter.y, playback01 >= 0.0 ? 2.0f : 1.2f);
                g.setColour (accent.withAlpha (playback01 >= 0.0 ? 0.90f : 0.58f));
                g.fillEllipse (playheadDot.x - 5.0f, playheadDot.y - 5.0f, 10.0f, 10.0f);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawEllipse (playheadDot.x - 5.0f, playheadDot.y - 5.0f, 10.0f, 10.0f, 1.2f);
            }

            const auto targetLabel = element.arpLaneTarget == "drums" ? "DRUMS"
                                  : element.arpLaneTarget == "oneShots" ? "ONE SHOTS"
                                  : element.arpLaneTarget == "loops" ? "LOOP SLICES"
                                  : element.arpLaneTarget == "effects" ? "FX SENDS"
                                  : element.arpLaneTarget == "samples" ? "SAMPLES" : "NOTES";

            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (24.0f));
            g.drawText (juce::String (steps), juce::Rectangle<int> ((int) centre.x - 44, (int) centre.y - 22, 88, 30),
                        juce::Justification::centred, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (9.0f, juce::Font::bold));
            g.drawText (targetLabel, juce::Rectangle<int> ((int) centre.x - 44, (int) centre.y + 7, 88, 18),
                        juce::Justification::centred, true);
        }

        static void drawDrumGridPreview (juce::Graphics& g,
                                         juce::Rectangle<int> r,
                                         const LayoutElement& element,
                                         const DspGraph& graph,
                                         bool transportPlaying,
                                         double playback01)
        {
            const auto* block = findDrumMachineBlock (graph);
            const int tracks = block != nullptr
                ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) element.drumTracks)))
                : juce::jlimit (1, 16, element.drumTracks);
            const int steps = block != nullptr
                ? juce::jlimit (1, 64, juce::roundToInt (blockValue (*block, "dmSteps", (float) element.drumSteps)))
                : juce::jlimit (1, 64, element.drumSteps);
            const int pattern = block != nullptr
                ? juce::jlimit (0, 7, juce::roundToInt (blockValue (*block, "dmPattern", (float) element.drumPattern)))
                : juce::jlimit (0, 7, element.drumPattern);

            const auto bg = element.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : element.accentColour;

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, element.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, element.cornerRadius), 1.0f);

            auto area = r.reduced (8);
            auto header = area.removeFromTop (28);
            auto playButton = header.removeFromRight (58).reduced (2);
            auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);
            g.setColour (accent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ((element.label.isNotEmpty() ? element.label : "DRUM GRID")
                            + "  P" + juce::String (pattern + 1),
                        header, juce::Justification::centredLeft, true);

            g.setColour (transportPlaying ? accent : PatchCraftLookAndFeel::panel().brighter (0.08f));
            g.fillRoundedRectangle (playButton.toFloat(), 5.0f);
            g.setColour (transportPlaying ? juce::Colour (0xff071014) : border);
            g.drawRoundedRectangle (playButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
            g.setColour (transportPlaying ? juce::Colour (0xff071014) : PatchCraftLookAndFeel::text().withAlpha (0.92f));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (transportPlaying ? "STOP" : "PLAY", playButton, juce::Justification::centred, true);

            constexpr int bankCount = 8;
            constexpr int bankGap = 3;
            const int bankW = juce::jmax (16, (bankStrip.getWidth() - bankGap * (bankCount - 1)) / bankCount);
            for (int bank = 0; bank < bankCount; ++bank)
            {
                auto chip = juce::Rectangle<int> (bankStrip.getX() + bank * (bankW + bankGap),
                                                  bankStrip.getY(),
                                                  bankW,
                                                  bankStrip.getHeight()).reduced (0, 2);
                const bool activeBank = bank == pattern;
                g.setColour (activeBank ? accent.withAlpha (0.92f) : PatchCraftLookAndFeel::panel().brighter (0.05f));
                g.fillRoundedRectangle (chip.toFloat(), 4.0f);
                g.setColour (activeBank ? juce::Colour (0xff071014) : PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (8.5f, juce::Font::bold));
                g.drawText (juce::String (bank + 1), chip, juce::Justification::centred, true);
            }

            area.removeFromTop (4);
            if (area.isEmpty())
                return;

            const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
            auto grid = area.withTrimmedLeft (labelW);
            const float cellW = (float) grid.getWidth() / (float) steps;
            const float cellH = (float) area.getHeight() / (float) tracks;
            const int playbackStep = playback01 >= 0.0
                ? juce::jlimit (0, steps - 1, (int) std::floor (playback01 * (double) steps))
                : -1;
            static const char* names[] = { "Kick", "Snare", "Hat", "Clap", "Tom", "Perc", "Ride", "Crash" };

            for (int track = 0; track < tracks; ++track)
            {
                const int y = area.getY() + juce::roundToInt ((float) track * cellH);
                const int h = juce::roundToInt (cellH);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (juce::jlimit (8.0f, 10.5f, cellH * 0.40f), juce::Font::bold));
                const juce::String trackLabel = track < 8 ? juce::String (names[track]) : "T" + juce::String (track + 1);
                g.drawText (trackLabel, area.getX(), y, labelW - 5, h, juce::Justification::centredLeft, true);

                for (int step = 0; step < steps; ++step)
                {
                    const int x = grid.getX() + juce::roundToInt ((float) step * cellW);
                    const auto cell = juce::Rectangle<float> ((float) x + 1.0f,
                                                              (float) y + 1.0f,
                                                              juce::jmax (1.0f, cellW - 2.0f),
                                                              juce::jmax (1.0f, cellH - 2.0f));
                    if (step == playbackStep)
                    {
                        g.setColour (accent.withAlpha (0.13f));
                        g.fillRoundedRectangle (cell.expanded (0.5f, 0.0f), 2.5f);
                    }
                    const auto prefix = "dmP" + juce::String (pattern)
                                      + "T" + juce::String (track)
                                      + "S" + juce::String (step);
                    const bool active = block != nullptr && blockValue (*block, prefix + "On", 0.0f) >= 0.5f;
                    const float velocity = block != nullptr
                        ? juce::jlimit (0.1f, 1.0f, blockValue (*block, prefix + "Vel", 0.8f))
                        : 0.75f;
                    const int divisions = block != nullptr
                        ? juce::jlimit (1, 4, juce::roundToInt (blockValue (*block, prefix + "Div", 1.0f)))
                        : 1;
                    g.setColour ((step % 4 == 0 ? PatchCraftLookAndFeel::panel().brighter (0.08f)
                                                 : PatchCraftLookAndFeel::panel())
                                     .withAlpha (0.86f));
                    g.fillRoundedRectangle (cell, 2.5f);
                    if (divisions > 1)
                    {
                        g.setColour (border.brighter (0.25f).withAlpha (0.70f));
                        for (int division = 1; division < divisions; ++division)
                        {
                            const float divX = cell.getX() + cell.getWidth() * (float) division / (float) divisions;
                            g.drawVerticalLine (juce::roundToInt (divX), cell.getY() + 2.0f, cell.getBottom() - 2.0f);
                        }
                    }
                    if (active)
                    {
                        auto hit = cell.reduced (2.0f);
                        hit.removeFromTop (hit.getHeight() * (1.0f - velocity));
                        g.setColour (accent.withAlpha (0.68f + velocity * 0.28f));
                        g.fillRoundedRectangle (hit, 2.0f);
                        if (divisions > 1 && cell.getWidth() >= 14.0f && cell.getHeight() >= 12.0f)
                        {
                            g.setColour (juce::Colour (0xff0a0c10));
                            g.setFont (juce::Font (juce::jmin (9.0f, cell.getHeight() * 0.48f), juce::Font::bold));
                            g.drawText ("x" + juce::String (divisions), cell.toNearestInt().reduced (1),
                                        juce::Justification::centred, true);
                        }
                    }
                }
            }

            if (playback01 >= 0.0)
            {
                const float playheadX = (float) grid.getX()
                    + (float) playback01 * (float) grid.getWidth();
                g.setColour (accent.withAlpha (0.95f));
                g.drawLine (playheadX, (float) grid.getY(), playheadX, (float) grid.getBottom(), 2.0f);
                g.setColour (accent.withAlpha (0.28f));
                g.fillRoundedRectangle (playheadX - 4.0f, (float) header.getY(), 8.0f,
                                        (float) (grid.getBottom() - header.getY()), 4.0f);
            }
        }
    }

    static juce::String scopedTabGroupId (const LayoutElement& tabPanel, const juce::String& label)
    {
        if (tabPanel.id == "tabs")
            return LayoutElement::tabLabelToGroupId (label);
        return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
    }

    static bool isScopedTabGroupId (const juce::String& groupId)
    {
        return groupId.contains ("__tab__");
    }

    static void drawRuntimeLabelText (juce::Graphics& g, juce::Rectangle<int> r,
                                      const LayoutElement& e, const juce::String& valueText)
    {
        if (e.labelPosition == "hidden")
            return;

        auto labelArea = r.removeFromBottom (juce::jmax (20, r.getHeight() / 4));
        labelArea.translate (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY + e.labelSpacing));
        if (e.labelPosition == "top")
            labelArea = r.removeFromTop (juce::jmax (20, r.getHeight() / 4)).translated (juce::roundToInt (e.labelOffsetX),
                                                                                         juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "left")
            labelArea = juce::Rectangle<int> (r.getX() - r.getWidth() / 2 - juce::roundToInt (e.labelSpacing),
                                              r.getCentreY() - 18, r.getWidth() / 2, 36)
                            .translated (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "right")
            labelArea = juce::Rectangle<int> (r.getRight() + juce::roundToInt (e.labelSpacing),
                                              r.getCentreY() - 18, r.getWidth() / 2, 36)
                            .translated (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));

        const auto fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, labelArea.getHeight() * 0.42f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (fontSize, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : e.parameterId.toUpperCase(),
                    labelArea.removeFromTop (labelArea.getHeight() / 2),
                    juce::Justification::centred, true);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (juce::jmax (8.0f, fontSize * 0.85f)));
        g.drawText (valueText, labelArea, juce::Justification::centred, true);
    }

    StudioInstrumentRenderer::StudioInstrumentRenderer (StudioMainComponent& o)
        : owner (o), project (o.getProject()), assets (o.getAssets())
    {
        setOpaque (true);
        project.addListener (this);
        project.getLiveValues().addListener (this);
        startTimerHz (24);
        rebuild();
    }

    StudioInstrumentRenderer::~StudioInstrumentRenderer()
    {
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void StudioInstrumentRenderer::syncFromDesignerState (const CanvasEditor& canvas)
    {
        const auto nextTabGroup = canvas.getCurrentTabGroup();
        const auto nextPanels = canvas.getActiveTabGroupsByPanel();
        if (currentTabGroup == nextTabGroup && activeTabGroupsByPanel == nextPanels)
        {
            rebuild();
            return;
        }

        currentTabGroup = nextTabGroup;
        activeTabGroupsByPanel = nextPanels;
        rebuild();
    }

    bool StudioInstrumentRenderer::isElementOnCurrentTab (const LayoutElement& e) const
    {
        if (e.groupId.isEmpty()) return true;
        for (const auto& parent : elementsCopy)
        {
            if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                && parent.id == e.groupId)
                return true;
            if (parent.type == ElementType::TabPanel)
            {
                for (const auto& tab : parent.tabs)
                {
                    const auto group = scopedTabGroupId (parent, tab);
                    if (group == e.groupId)
                    {
                        const auto active = activeTabGroupsByPanel.find (parent.id);
                        const auto activeGroup = active != activeTabGroupsByPanel.end()
                            ? active->second
                            : (parent.tabs.isEmpty() ? juce::String() : scopedTabGroupId (parent, parent.tabs[0]));
                        return activeGroup == e.groupId && isElementOnCurrentTab (parent);
                    }
                }
            }
        }
        if (isScopedTabGroupId (e.groupId))
            return false;
        return e.groupId == currentTabGroup;
    }

    void StudioInstrumentRenderer::rebuild()
    {
        const auto requestedTabGroup = currentTabGroup;
        const auto requestedPanels = activeTabGroupsByPanel;
        activeTabGroupsByPanel.clear();
        knobs.clear();
        knobParamIds.clear();
        knobIndicesByParam.clear();
        elementsCopy.clear();

        elementsCopy = project.getLayout().getAll();

        // Default tab = first one defined in the first TabPanel, or "main",
        // while preserving an already selected tab across rebuilds.
        juce::String defaultTabGroup = "main";
        bool requestedTabStillExists = requestedTabGroup.isNotEmpty() && requestedTabGroup == "main";
        for (auto& e : elementsCopy)
        {
            if (e.type == ElementType::TabPanel && ! e.tabs.isEmpty())
            {
                defaultTabGroup = scopedTabGroupId (e, e.tabs[0]);
                auto active = e.id == "tabs" && requestedTabGroup.isNotEmpty()
                    ? requestedTabGroup
                    : defaultTabGroup;
                if (auto it = requestedPanels.find (e.id);
                    it != requestedPanels.end())
                {
                    for (auto& tab : e.tabs)
                        if (scopedTabGroupId (e, tab) == it->second)
                            active = it->second;
                }
                activeTabGroupsByPanel[e.id] = active;
                for (auto& tab : e.tabs)
                    if (scopedTabGroupId (e, tab) == requestedTabGroup)
                        requestedTabStillExists = true;
            }
        }
        currentTabGroup = requestedTabStillExists ? requestedTabGroup : defaultTabGroup;

        // Load background
        background = juce::Image();
        heroImage = juce::Image();
        auto bgPath = project.getManifest().backgroundImage;
        if (bgPath.isNotEmpty())
        {
            auto f = juce::File::isAbsolutePath (bgPath)
                ? juce::File (bgPath)
                : project.getProjectFolder().getChildFile (bgPath);
            background = assets.loadImage (f);
        }
        if (! background.isValid())
            background = AssetManager::renderDefaultHeroImage (800, 600);

        heroImage = AssetManager::renderDefaultHeroImage (600, 200);

        // Create knobs/sliders for visible elements
        for (auto& e : elementsCopy)
        {
            if (! e.visible || ! isElementOnCurrentTab (e))
            {
                knobs.add (nullptr);
                knobParamIds.push_back ("");
                continue;
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
            {
                auto slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle (e.type == ElementType::Slider
                    ? juce::Slider::LinearVertical
                    : juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
                slider->setColour (juce::Slider::rotarySliderFillColourId, e.accentColour);
                slider->setColour (juce::Slider::thumbColourId, e.accentColour);
                slider->setAlpha (0.01f);

                const bool showGuidance = project.getManifest().playerShowParameterGuidance;

                // Set range from parameter definition if available
                auto* paramDef = project.getParameters().find (e.parameterId);
                const bool visualOnlyControl = e.parameterId.isEmpty()
                    && (e.type == ElementType::Knob || e.type == ElementType::Slider);
                if (paramDef != nullptr)
                {
                    slider->setRange (paramDef->min, paramDef->max, paramDef->step > 0.0f ? paramDef->step : 0.001);
                    slider->setValue (project.getLiveValues().getValue (e.parameterId, paramDef->defaultValue), juce::dontSendNotification);
                    slider->setDoubleClickReturnValue (true, paramDef->defaultValue);
                }
                else
                {
                    slider->setRange (0.0, 1.0, 0.01);
                    const auto previewValue = juce::jlimit (0.0f, 1.0f, e.controlPreviewValue);
                    slider->setValue (visualOnlyControl ? previewValue
                                                        : project.getLiveValues().getValue (e.parameterId, 0.5f),
                                      juce::dontSendNotification);
                    slider->setDoubleClickReturnValue (true, visualOnlyControl ? previewValue : 0.5);
                }

                const bool connected = e.parameterId.isNotEmpty() && paramDef != nullptr
                    && parameterIsEnabledForRenderer (project, *paramDef);
                slider->setEnabled (connected || visualOnlyControl);
                slider->setTooltip (showGuidance && ! visualOnlyControl ? disconnectedControlMessage (project, e, paramDef) : juce::String());

                // On change, update LiveValueStore
                slider->onValueChange = [this,
                                         paramId = e.parameterId,
                                         elementId = e.id,
                                         visualOnlyControl,
                                         s = slider.get()]
                {
                    if (visualOnlyControl)
                    {
                        const auto value = juce::jlimit (0.0f, 1.0f, (float) s->getValue());
                        for (auto& element : elementsCopy)
                        {
                            if (element.id == elementId)
                            {
                                element.controlPreviewValue = value;
                                break;
                            }
                        }

                        if (auto* element = project.getLayout().find (elementId))
                        {
                            element->controlPreviewValue = value;
                            project.markDirty();
                        }

                        repaint();
                        return;
                    }

                    if (paramId.isEmpty() || project.getParameters().find (paramId) == nullptr)
                        return;
                    project.getLiveValues().setValue (paramId, (float) s->getValue());
                };

                addAndMakeVisible (*slider);
                const int knobIndex = knobs.size();
                knobs.add (slider.release());
                knobParamIds.push_back (e.parameterId);
                if (connected)
                    knobIndicesByParam[e.parameterId].push_back (knobIndex);
            }
            else
            {
                knobs.add (nullptr);
                knobParamIds.push_back ("");
            }
        }

        repaint();
        resized();
    }

    void StudioInstrumentRenderer::liveValueChanged (const juce::String& parameterId, float newValue)
    {
        const auto found = knobIndicesByParam.find (parameterId);
        if (found == knobIndicesByParam.end())
        {
            repaint();
            return;
        }

        for (auto i : found->second)
        {
            if (i >= 0 && i < knobs.size() && knobs[i] != nullptr
                && std::abs ((float) knobs[i]->getValue() - newValue) > 0.0001f)
            {
                knobs[i]->setValue (newValue, juce::dontSendNotification);
            }
        }
    }

    void StudioInstrumentRenderer::projectChanged()
    {
        rebuild();
    }

    void StudioInstrumentRenderer::projectChanged (PatchCraftProject::ChangeScope scope)
    {
        if (scope == PatchCraftProject::ChangeScope::dspRealtime)
        {
            syncKnobsToLiveValues();
            repaint();
            return;
        }

        projectChanged();
    }

    void StudioInstrumentRenderer::syncKnobsToLiveValues()
    {
        for (int i = 0; i < knobs.size(); ++i)
        {
            if (knobs[i] != nullptr && knobParamIds[i].isNotEmpty())
            {
                float value = project.getLiveValues().getValue (knobParamIds[i], (float) knobs[i]->getValue());
                knobs[i]->setValue (value, juce::dontSendNotification);
            }
        }
    }

    StudioInstrumentRenderer::CanvasMetrics StudioInstrumentRenderer::metrics() const
    {
        CanvasMetrics m;
        auto bounds = getLocalBounds();
        auto canvasW = project.getCanvasSize().width;
        auto canvasH = project.getCanvasSize().height;
        if (canvasW <= 0) canvasW = 1000;
        if (canvasH <= 0) canvasH = 600;

        float scaleX = (float) bounds.getWidth() / (float) canvasW;
        float scaleY = (float) bounds.getHeight() / (float) canvasH;
        m.scale = juce::jmin (scaleX, scaleY);

        int drawW = (int) (canvasW * m.scale);
        int drawH = (int) (canvasH * m.scale);
        m.canvas = bounds.withSizeKeepingCentre (drawW, drawH);
        return m;
    }

    juce::Rectangle<int> StudioInstrumentRenderer::elementRect (const LayoutElement& e, const CanvasMetrics& m) const
    {
        int x = m.canvas.getX() + (int) (e.x * m.scale);
        int y = m.canvas.getY() + (int) (e.y * m.scale);
        int w = (int) (e.width * m.scale);
        int h = (int) (e.height * m.scale);
        return { x, y, w, h };
    }

    juce::Rectangle<int> StudioInstrumentRenderer::animatedElementRect (const LayoutElement& element,
                                                                        juce::Rectangle<int> rect) const
    {
        float amount = element.audioReactive
            ? audioReactiveLevel.load (std::memory_order_relaxed) * juce::jmax (0.08f, element.audioReactiveAmount)
            : 0.0f;
        if (element.animationMode != "none")
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi
                                                  * juce::jmax (0.05f, element.animationRate)) * 0.5) + 0.5);
            if (element.animationMode == "shake")
            {
                rect.translate (juce::roundToInt ((wave - 0.5f) * 8.0f), 0);
                return rect;
            }
            amount += wave * 0.10f;
        }

        if (amount <= 0.0001f)
            return rect;

        const int grow = juce::roundToInt (juce::jlimit (0.0f, 16.0f, amount * 14.0f));
        return rect.expanded (grow, grow);
    }

    void StudioInstrumentRenderer::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        auto m = metrics();

        // Background
        if (background.isValid())
            g.drawImage (background, m.canvas.toFloat(), juce::RectanglePlacement::stretchToFit);

        // Elements (non-knob types)
        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& e = elementsCopy[i];
            if (! e.visible || ! isElementOnCurrentTab (e)) continue;

            auto r = animatedElementRect (e, elementRect (e, m));
            juce::Graphics::ScopedSaveState opacityState (g);
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity));

            switch (e.type)
            {
                case ElementType::Image:
                {
                    juce::String assetPath = e.asset;
                    if (assetPath.isEmpty() && e.id == "background")
                        assetPath = project.backgroundImageRelative;

                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                            ? juce::File (assetPath)
                            : project.getProjectFolder().getChildFile (assetPath);
                        auto img = assets.loadImage (f);
                        if (img.isValid())
                            g.drawImage (img, r.toFloat(), juce::RectanglePlacement::stretchToFit);
                    }
                    else if (e.id == "background" && background.isValid())
                    {
                        g.drawImage (background, r.toFloat(), juce::RectanglePlacement::stretchToFit);
                    }
                    else
                    {
                        drawHeroPlaceholder (g, r);
                    }
                    break;
                }

                case ElementType::Label:
                    drawLabel (g, r, e.label, e.labelSize > 0.0f ? e.labelSize : 14.0f,
                               e.textColour.isTransparent() ? PatchCraftLookAndFeel::text() : e.textColour);
                    break;

                case ElementType::Panel:
                    drawPanel (g, r, e.label);
                    break;

                case ElementType::Shape:
                {
                    auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 1.0f);
                    if (e.shadowAmount > 0.0f)
                    {
                        g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                        g.fillRoundedRectangle (shapeBounds.translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount), e.cornerRadius);
                    }
                    if (e.glowAmount > 0.0f)
                    {
                        g.setColour (e.accentColour.withAlpha (0.18f * e.glowAmount));
                        g.fillRoundedRectangle (shapeBounds.expanded (8.0f * e.glowAmount), e.cornerRadius + 8.0f * e.glowAmount);
                    }

                    juce::Path path;
                    if (e.shapeKind == "ellipse")
                        path.addEllipse (shapeBounds);
                    else if (e.shapeKind == "triangle")
                        path.addTriangle (shapeBounds.getCentreX(), shapeBounds.getY(),
                                          shapeBounds.getRight(), shapeBounds.getBottom(),
                                          shapeBounds.getX(), shapeBounds.getBottom());
                    else if (e.shapeKind == "diamond")
                    {
                        path.startNewSubPath (shapeBounds.getCentreX(), shapeBounds.getY());
                        path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
                        path.lineTo (shapeBounds.getCentreX(), shapeBounds.getBottom());
                        path.lineTo (shapeBounds.getX(), shapeBounds.getCentreY());
                        path.closeSubPath();
                    }
                    else if (e.shapeKind == "line")
                    {
                        path.startNewSubPath (shapeBounds.getX(), shapeBounds.getCentreY());
                        path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
                    }
                    else
                        path.addRoundedRectangle (shapeBounds, e.cornerRadius);

                    if (e.shapeKind != "line")
                    {
                        g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panel().withAlpha (0.5f) : e.backgroundColour);
                        g.fillPath (path);
                    }
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));
                    break;
                }

                case ElementType::Meter:
                    drawMeter (g, r);
                    break;

                case ElementType::EqCurve:
                    drawEqCurveElement (g, r, e, project.getDspGraph());
                    break;

                case ElementType::SpectrumAnalyzer:
                    drawSpectrumElement (g, r, e, audioReactiveLevel.load (std::memory_order_relaxed));
                    break;

                case ElementType::ReactiveImage:
                {
                    juce::Image img;
                    if (e.asset.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (e.asset)
                            ? juce::File (e.asset)
                            : project.getProjectFolder().getChildFile (e.asset);
                        img = assets.loadImage (f);
                    }
                    if (img.isValid())
                        g.drawImage (img, r.toFloat(), juce::RectanglePlacement::stretchToFit);
                    else
                        drawHeroPlaceholder (g, r);
                    const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                    const float level = audioReactiveLevel.load (std::memory_order_relaxed) * juce::jmax (0.1f, e.audioReactiveAmount);
                    g.setColour (accent.withAlpha (juce::jlimit (0.08f, 0.36f, 0.12f + level * 0.25f)));
                    g.drawRoundedRectangle (r.toFloat().expanded (level * 12.0f), juce::jmax (4.0f, e.cornerRadius + level * 8.0f), 1.5f + level * 2.0f);
                    break;
                }

                case ElementType::SpriteAnimator:
                {
                    const auto assetPath = e.asset.isNotEmpty() ? e.asset : e.filmstripAsset;
                    juce::Image img;
                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                            ? juce::File (assetPath)
                            : project.getProjectFolder().getChildFile (assetPath);
                        img = assets.loadImage (f);
                    }
                    if (img.isValid())
                    {
                        const int frames = juce::jmax (1, e.filmstripFrames > 0 ? e.filmstripFrames : 8);
                        const int frame = ((int) std::floor (juce::Time::getMillisecondCounterHiRes() * 0.001
                                                             * juce::jmax (0.05f, e.animationRate))) % frames;
                        if (e.filmstripVertical)
                        {
                            const int h = juce::jmax (1, img.getHeight() / frames);
                            g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                                         0, frame * h, img.getWidth(), h);
                        }
                        else
                        {
                            const int w = juce::jmax (1, img.getWidth() / frames);
                            g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                                         frame * w, 0, w, img.getHeight());
                        }
                    }
                    else
                    {
                        g.setColour (PatchCraftLookAndFeel::panelAlt());
                        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
                        drawLabel (g, r.reduced (8), e.label.isNotEmpty() ? e.label : "Sprite Animator");
                    }
                    break;
                }

                case ElementType::VisualFxLayer:
                {
                    const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                    const float level = audioReactiveLevel.load (std::memory_order_relaxed) * juce::jmax (0.1f, e.audioReactiveAmount);
                    auto bounds = r.reduced (8).toFloat();
                    const auto centre = bounds.getCentre();
                    const float seconds = (float) (juce::Time::getMillisecondCounterHiRes() * 0.001);
                    const float base = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.20f;
                    for (int i = 0; i < 30; ++i)
                    {
                        const float phase = seconds * juce::jmax (0.05f, e.animationRate) + (float) i * 0.45f;
                        const float radius = base + std::fmod ((float) i * 9.0f + seconds * 20.0f, base * (1.5f + level));
                        g.setColour (accent.withAlpha (0.12f + level * 0.25f));
                        g.fillEllipse (centre.x + std::cos (phase) * radius - 2.0f,
                                       centre.y + std::sin (phase * 0.8f) * radius * 0.7f - 2.0f,
                                       4.0f + level * 4.0f,
                                       4.0f + level * 4.0f);
                    }
                    break;
                }

                case ElementType::AiVisualPrompt:
                    g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt().withAlpha (0.8f) : e.backgroundColour);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
                    g.setColour (e.accentColour.isTransparent() ? juce::Colour (0xffb98cff) : e.accentColour);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);
                    drawLabel (g, r.reduced (8), "Pro AI Visual Brief", 12.0f, e.accentColour);
                    break;

                case ElementType::SampleDropZone:
                    g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt().withAlpha (0.8f) : e.backgroundColour);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
                    g.setColour (e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);
                    drawLabel (g, r.reduced (8), e.label.isNotEmpty() ? e.label : "Sample Drop Zone", 12.0f, e.accentColour);
                    break;

                case ElementType::Knob:
                case ElementType::Slider:
                case ElementType::MacroControl:
                    drawRuntimeControl (g,
                                        e.labelPosition == "hidden"
                                            ? r.reduced (2)
                                            : r.withTrimmedBottom (juce::jmax (20, r.getHeight() / 4)),
                                        e);
                    break;

                case ElementType::Button:
                {
                    g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
                    g.setColour (e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.2f);
                    drawLabel (g, r.reduced (6), e.label.isNotEmpty() ? e.label : "Button");
                    break;
                }

                case ElementType::Toggle:
                {
                    const auto* def = project.getParameters().find (e.parameterId);
                    const auto value = project.getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
                    const bool on = value >= 0.5f;
                    auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())).reduced (4);
                    g.setColour (on ? e.accentColour : PatchCraftLookAndFeel::panelAlt());
                    g.fillEllipse (toggle.toFloat());
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.drawEllipse (toggle.toFloat(), 1.0f);
                    drawRuntimeLabelText (g, r, e, on ? "ON" : "OFF");
                    break;
                }

                case ElementType::Dropdown:
                    drawDropdown (g, r, e.parameterId.isEmpty()
                        ? "Select..."
                        : ((e.label.isNotEmpty() ? e.label + ": " : juce::String()) + displayValueForParameter (project, e.parameterId)));
                    break;

                case ElementType::ValueDisplay:
                {
                    const auto* def = project.getParameters().find (e.parameterId);
                    const auto value = project.getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 5.0f);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (10.0f);
                    g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId, r.removeFromTop (16).reduced (5, 0), juce::Justification::centredLeft, true);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.setFont (juce::Font (16.0f, juce::Font::bold));
                    g.drawText (juce::String (value, 2), r.reduced (5, 0), juce::Justification::centredRight, true);
                    break;
                }

                case ElementType::Waveform:
                {
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 4.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    juce::Path wave;
                    wave.startNewSubPath ((float) r.getX(), (float) r.getCentreY());
                    for (int x = 0; x < r.getWidth(); x += 4)
                    {
                        const auto phase = (float) x / juce::jmax (1, r.getWidth()) * juce::MathConstants<float>::twoPi * 3.0f;
                        wave.lineTo ((float) r.getX() + x, (float) r.getCentreY() + std::sin (phase) * (float) r.getHeight() * 0.28f);
                    }
                    g.strokePath (wave, juce::PathStrokeType (1.5f));
                    break;
                }

                case ElementType::Keyboard:
                    drawKeyboard (g, r);
                    break;

                case ElementType::TabPanel:
                    drawTabPanel (g, r, e);
                    break;

                case ElementType::XYPad:
                {
                    float normalised = 0.5f;
                    if (const auto* def = project.getParameters().find (e.parameterId))
                    {
                        const auto value = project.getLiveValues().getValue (def->id, def->defaultValue);
                        normalised = juce::jlimit (0.0f, 1.0f,
                            (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
                    }
                    const auto inner = r.reduced (6);
                    const float dotX = juce::jmap (normalised, 0.0f, 1.0f,
                        (float) inner.getX(), (float) inner.getRight());
                    const float dotY = (float) inner.getCentreY();
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 5.0f);
                    g.setColour (PatchCraftLookAndFeel::border());
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.drawLine ((float) r.getCentreX(), (float) r.getY() + 6.0f, (float) r.getCentreX(), (float) r.getBottom() - 6.0f, 1.0f);
                    g.drawLine ((float) r.getX() + 6.0f, (float) r.getCentreY(), (float) r.getRight() - 6.0f, (float) r.getCentreY(), 1.0f);
                    g.fillEllipse (dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
                    break;
                }

                case ElementType::GranularField:
                {
                    const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
                    const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
                    const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                    auto normalized = [this] (const juce::String& parameterId, float fallback)
                    {
                        const auto* def = project.getParameters().find (parameterId);
                        if (def == nullptr)
                            return fallback;

                        const auto value = project.getLiveValues().getValue (parameterId, def->defaultValue);
                        return juce::jlimit (0.0f, 1.0f,
                            (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
                    };
                    auto raw = [this] (const juce::String& parameterId, float fallback)
                    {
                        const auto* def = project.getParameters().find (parameterId);
                        return def != nullptr ? project.getLiveValues().getValue (parameterId, def->defaultValue) : fallback;
                    };
                    auto directionName = [] (int direction)
                    {
                        switch (juce::jlimit (0, 3, direction))
                        {
                            case 0:  return juce::String ("Forward");
                            case 1:  return juce::String ("Reverse");
                            case 2:  return juce::String ("Ping-Pong");
                            default: return juce::String ("Multi");
                        }
                    };

                    g.setColour (bg);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
                    g.setColour (border);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

                    auto area = r.reduced (10, 8);
                    auto header = area.removeFromTop (22);
                    g.setColour (accent);
                    g.setFont (juce::Font (12.0f, juce::Font::bold));
                    if (e.label.isNotEmpty())
                        g.drawText (e.label.toUpperCase(),
                                    header.removeFromLeft (160), juce::Justification::centredLeft, true);
                    area.removeFromTop (4);

                    if (area.getHeight() < 60 || area.getWidth() < 160)
                        break;

                    auto chipArea = area.removeFromBottom (24);
                    area.removeFromBottom (4);

                    const float start = normalized ("sampleStart", 0.35f);
                    const float length = normalized ("sampleLength", 0.65f);
                    const float density = raw ("granularDensity", 24.0f);
                    const float scan = raw ("granularScan", 0.0f);
                    const float spread = normalized ("granularSpread", 0.18f);
                    const float reverse = normalized ("granularReverse", 0.0f);
                    const float texture = normalized ("granularTexture", 0.20f);
                    const int direction = juce::roundToInt (raw ("granularDirection", 3.0f));
                    const bool frozen = raw ("granularFreeze", 0.0f) >= 0.5f;
                    const bool granularOn = raw ("granularOn", 0.0f) >= 0.5f;
                    const double seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
                    float visualPosition = start;
                    if (granularOn && ! frozen)
                    {
                        visualPosition += (float) seconds * scan * 0.055f;
                        visualPosition -= std::floor (visualPosition);
                    }
                    const float centreX = juce::jmap (visualPosition, 0.0f, 1.0f, (float) area.getX(), (float) area.getRight());
                    const float span = juce::jmap (length, 0.0f, 1.0f, 14.0f, (float) area.getWidth() * 0.54f);
                    g.setColour (border.withAlpha (0.22f));
                    for (int i = 1; i < 6; ++i)
                        g.drawVerticalLine (area.getX() + (area.getWidth() * i) / 6, (float) area.getY(), (float) area.getBottom());
                    g.setColour (accent.withAlpha (0.25f));
                    g.fillEllipse (centreX - span * 0.45f, (float) area.getCentreY() - (float) area.getHeight() * 0.28f,
                                   span * 0.9f, (float) area.getHeight() * 0.56f);
                    g.setColour (accent.withAlpha (0.9f));
                    g.drawLine (centreX, (float) area.getY(), centreX, (float) area.getBottom(), 1.4f);
                    const int grainDots = juce::jlimit (18, 72, juce::roundToInt (juce::jmap (density, 0.5f, 220.0f, 18.0f, 72.0f)));
                    const float motionPhase = granularOn && ! frozen ? (float) seconds * (0.50f + std::abs (scan) * 0.72f) : 0.0f;
                    for (int i = 0; i < grainDots; ++i)
                    {
                        const float phase = (float) i * 0.71f + motionPhase;
                        const float spray = 0.16f + spread * 0.42f + texture * 0.16f;
                        const float directionSign = direction == 1 ? -1.0f
                                                  : direction == 2 ? std::sin (motionPhase * 0.75f) >= 0.0f ? 1.0f : -1.0f
                                                  : scan < 0.0f ? -1.0f : 1.0f;
                        const float x = centreX
                            + std::sin (phase * 1.3f) * span * (0.18f + spray)
                            + directionSign * std::cos (phase * 0.67f) * span * (0.10f + reverse * 0.16f);
                        const float y = (float) area.getCentreY() + std::cos (phase * 0.9f) * (float) area.getHeight() * (0.10f + spray * 0.32f);
                        const float size = 2.0f + (float) (i % 4);
                        g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
                    }

                    juce::Path arrow;
                    const float arrowY = (float) area.getBottom() - 13.0f;
                    const float arrowW = juce::jmin (70.0f, (float) area.getWidth() * 0.16f);
                    const float arrowStart = direction == 1 ? centreX + arrowW * 0.5f : centreX - arrowW * 0.5f;
                    const float arrowEnd = direction == 1 ? centreX - arrowW * 0.5f : centreX + arrowW * 0.5f;
                    g.setColour (accent.withAlpha (granularOn ? 0.88f : 0.38f));
                    g.drawArrow ({ arrowStart, arrowY, arrowEnd, arrowY }, 1.8f, 9.0f, 7.0f);
                    if (direction == 2 || direction == 3)
                        g.drawArrow ({ arrowEnd, arrowY - 9.0f, arrowStart, arrowY - 9.0f }, 1.4f, 8.0f, 6.0f);

                    const juce::StringArray chips { "FWD", "REV", "PING", "MULTI", frozen ? "UNFREEZE" : "FREEZE" };
                    const int chipGap = 5;
                    const int chipW = juce::jmax (48, (chipArea.getWidth() - chipGap * (chips.size() - 1)) / chips.size());
                    for (int i = 0; i < chips.size(); ++i)
                    {
                        auto chip = juce::Rectangle<int> (chipArea.getX() + i * (chipW + chipGap),
                                                          chipArea.getY(), chipW, chipArea.getHeight()).reduced (1);
                        const bool active = (i < 4 && i == juce::jlimit (0, 3, direction)) || (i == 4 && frozen);
                        g.setColour (active ? accent.withAlpha (0.85f) : bg.brighter (0.08f));
                        g.fillRoundedRectangle (chip.toFloat(), 5.0f);
                        g.setColour (active ? accent : border.withAlpha (0.74f));
                        g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 5.0f, active ? 1.4f : 1.0f);
                        g.setColour (active ? juce::Colour (0xff080a0f) : PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (9.0f, juce::Font::bold));
                        g.drawText (chips[i], chip, juce::Justification::centred, true);
                    }

                    juce::ignoreUnused (directionName, scan, density, granularOn);
                    break;
                }

                case ElementType::ScrollPanel:
                    drawPanel (g, r, e.label);
                    break;

                case ElementType::Group:
                    break;

                case ElementType::Separator:
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.drawLine ((float) r.getX(), (float) r.getCentreY(), (float) r.getRight(), (float) r.getCentreY(), juce::jmax (1.0f, e.strokeWidth));
                    break;

                case ElementType::DrumGrid:
                    drawDrumGridPreview (g, r, e, project.getDspGraph(),
                                         isTransportPlaying != nullptr && isTransportPlaying(),
                                         getSequencerPlaybackPosition01 != nullptr
                                            ? getSequencerPlaybackPosition01 (juce::jlimit (1, 64, e.drumSteps))
                                            : -1.0);
                    break;

                case ElementType::ArpLane:
                    drawArpLanePreview (g, r, e, project.getDspGraph(),
                                        getSequencerPlaybackPosition01 != nullptr
                                            ? getSequencerPlaybackPosition01 (juce::jlimit (1, 128, e.arpLaneSteps))
                                            : -1.0);
                    break;

                case ElementType::Mixer:
                {
                    const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
                    const auto borderC = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
                    const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                    const int channels = juce::jlimit (1, 16, e.mixerChannels);
                    g.setColour (bg);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour (borderC);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);

                    auto area = r.reduced (10, 8);
                    if (e.labelPosition != "hidden")
                    {
                        auto header = area.removeFromTop (22);
                        g.setColour (accent);
                        g.setFont (juce::Font (12.0f, juce::Font::bold));
                        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MIXER",
                                    header.removeFromLeft (150), juce::Justification::centredLeft, true);
                        g.setColour (PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (10.0f));
                        g.drawText ("Runtime mixer", header, juce::Justification::centredRight, true);
                        area.removeFromTop (6);
                    }

                    const int stripW = juce::jmax (38, area.getWidth() / channels);
                    for (int channel = 0; channel < channels; ++channel)
                    {
                        auto strip = juce::Rectangle<int> (area.getX() + channel * stripW,
                                                           area.getY(),
                                                           channel == channels - 1
                                                                ? area.getRight() - (area.getX() + channel * stripW)
                                                                : stripW,
                                                           area.getHeight()).reduced (3, 0);
                        const juce::String label = channel < e.mixerChannelLabels.size()
                            ? e.mixerChannelLabels[channel]
                            : (channel == 0 ? "Main" : "Bus " + juce::String (channel + 1));
                        g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                        g.fillRoundedRectangle (strip.toFloat(), 5.0f);
                        g.setColour (borderC.withAlpha (0.65f));
                        g.drawRoundedRectangle (strip.toFloat().reduced (0.5f), 5.0f, 1.0f);
                        g.setColour (PatchCraftLookAndFeel::text());
                        g.setFont (juce::Font (10.0f, juce::Font::bold));
                        g.drawText (label, strip.removeFromTop (20), juce::Justification::centred, true);

                        auto buttons = strip.removeFromBottom (22).reduced (2, 2);
                        auto pan = strip.removeFromBottom (22).reduced (5, 5);
                        auto value = strip.removeFromBottom (14);
                        auto fader = strip.reduced (5, 6);
                        const int centre = fader.getCentreX();
                        const auto track = juce::Rectangle<int> (centre - 4, fader.getY() + 2,
                                                                 8, juce::jmax (12, fader.getHeight() - 4));
                        const float level = channel == 0 ? 0.80f : 0.56f;
                        const int thumbY = juce::roundToInt (juce::jmap (level, 0.0f, 1.0f,
                                                                          (float) track.getBottom(),
                                                                          (float) track.getY()));
                        g.setColour (PatchCraftLookAndFeel::bg().brighter (0.08f));
                        g.fillRoundedRectangle (track.toFloat(), 4.0f);
                        g.setColour (accent.withAlpha (0.72f));
                        g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (track.getX(), thumbY,
                                                                                          track.getRight(), track.getBottom()).toFloat(), 4.0f);
                        g.setColour (PatchCraftLookAndFeel::text());
                        g.fillRoundedRectangle (juce::Rectangle<float> ((float) centre - 11.0f, (float) thumbY - 4.0f,
                                                                        22.0f, 8.0f), 3.0f);
                        g.setColour (PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (9.0f));
                        g.drawText (juce::String (juce::roundToInt (level * 100.0f)),
                                    value, juce::Justification::centred, true);
                        g.setColour (PatchCraftLookAndFeel::bg().brighter (0.08f));
                        g.fillRoundedRectangle (pan.toFloat(), 3.0f);
                        auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0);
                        auto solo = buttons.reduced (1, 0);
                        g.fillRoundedRectangle (mute.toFloat(), 3.0f);
                        g.fillRoundedRectangle (solo.toFloat(), 3.0f);
                        g.setColour (PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (9.0f, juce::Font::bold));
                        g.drawText ("M", mute, juce::Justification::centred, true);
                        g.drawText ("S", solo, juce::Justification::centred, true);
                    }
                    break;
                }

                case ElementType::ModMatrix:
                {
                    const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
                    const auto borderC = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
                    const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                    g.setColour (bg);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour (borderC);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    auto area = r.reduced (10, 8);
                    auto header = area.removeFromTop (20);
                    g.setColour (accent);
                    g.setFont (juce::Font (11.0f, juce::Font::bold));
                    g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MOD MATRIX",
                                header.removeFromLeft (130), juce::Justification::centredLeft, true);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (9.0f);
                    g.drawText ("performable routing", header, juce::Justification::centredRight, true);
                    auto list = area.reduced (2, 6);
                    const auto& routes = project.getDspGraph().modulation;
                    if (routes.empty())
                    {
                        g.setColour (PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (10.0f, juce::Font::bold));
                        g.drawFittedText ("No modulation routes. Select in Inspector to add source -> target rows.",
                                          list, juce::Justification::centred, 3);
                        break;
                    }
                    const int maxRows = juce::jmin (6, (int) routes.size());
                    for (int i = 0; i < maxRows; ++i)
                    {
                        const auto& route = routes[(size_t) i];
                        auto row = list.removeFromTop (juce::jmax (18, list.getHeight() / (maxRows - i))).reduced (0, 2);
                        g.setColour (route.enabled ? accent.withAlpha (0.16f) : PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                        g.fillRoundedRectangle (row.toFloat(), 4.0f);
                        g.setColour (route.enabled ? accent.withAlpha (0.78f) : PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::Font (8.8f, juce::Font::bold));
                        g.drawText (route.sourceId + " -> " + route.targetId,
                                    row.removeFromLeft (juce::jmax (80, row.getWidth() - 52)).reduced (6, 0),
                                    juce::Justification::centredLeft, true);
                        g.drawText (juce::String (route.amount, 2), row.reduced (4, 0), juce::Justification::centredRight, true);
                    }
                    break;
                }

                case ElementType::DrumPad:
                case ElementType::PadGrid:
                {
                    const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
                    const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
                    const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
                    const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
                    if (! inner.isEmpty())
                    {
                        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
                        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
                        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
                        const auto borderC = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
                        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                        for (int row = 0; row < rows; ++row)
                        {
                            for (int col = 0; col < cols; ++col)
                            {
                                juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                                            (float) inner.getY() + row * (padH + gap),
                                                            padW, padH);
                                g.setColour (bg.brighter (0.04f));
                                g.fillRoundedRectangle (pad, juce::jmax (3.0f, e.cornerRadius * 0.6f));
                                g.setColour (accent.withAlpha (0.55f));
                                g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), 1.0f);
                                const int padIdx = row * cols + col;
                                const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                                const bool active = note == lastPlayedNote;
                                if (active)
                                {
                                    g.setColour (accent.withAlpha (0.82f));
                                    g.fillRoundedRectangle (pad.reduced (1.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f));
                                    g.setColour (accent);
                                    g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), 1.8f);
                                }
                                g.setColour ((active ? juce::Colour (0xff0a0c10) : PatchCraftLookAndFeel::text()).withAlpha (0.85f));
                                g.setFont (juce::Font (juce::jmin (12.0f, padH * 0.28f), juce::Font::bold));
                                const juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                                    ? e.label : juce::String (padIdx + 1);
                                g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f).toNearestInt(),
                                            juce::Justification::centred);
                                g.setColour (PatchCraftLookAndFeel::textDim());
                                g.setFont (juce::jmin (10.0f, padH * 0.22f));
                                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                                            pad.reduced (4.0f).removeFromBottom (padH * 0.35f).toNearestInt(),
                                            juce::Justification::centred);
                                juce::ignoreUnused (borderC);
                            }
                        }
                    }
                    break;
                }

                default:
                    break;
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
            {
                const auto* def = project.getParameters().find (e.parameterId);
                juce::String valueText;
                if (def != nullptr)
                {
                    const auto v = project.getLiveValues().getValue (def->id, def->defaultValue);
                    if (def->unit == "Hz" && v >= 1000.0f) valueText = juce::String (v / 1000.0f, 1) + " kHz";
                    else if (def->unit.isNotEmpty())       valueText = juce::String (v, 2) + " " + def->unit;
                    else                                   valueText = juce::String (v, 2);
                }

                drawRuntimeLabelText (g, r, e, valueText);
            }
        }
    }

    void StudioInstrumentRenderer::resized()
    {
        auto m = metrics();

        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& e = elementsCopy[i];
            if (knobs[(int) i] != nullptr && e.visible && isElementOnCurrentTab (e))
            {
                auto bounds = elementRect (e, m);
                if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
                    bounds.removeFromBottom (juce::jmax (20, bounds.getHeight() / 4));
                bounds = animatedElementRect (e, bounds);
                knobs[(int) i]->setBounds (bounds.reduced (2));
            }
        }
    }

    bool StudioInstrumentRenderer::drumCellAt (const LayoutElement& element,
                                               juce::Rectangle<int> r,
                                               juce::Point<int> pos,
                                               int& pattern,
                                               int& track,
                                               int& step,
                                               float& velocity,
                                               float& gate,
                                               float& probability,
                                               bool& active,
                                               int& note,
                                               int& divisions) const
    {
        const auto* block = findDrumMachineBlock (project.getDspGraph());
        const int tracks = block != nullptr
            ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) element.drumTracks)))
            : juce::jlimit (1, 16, element.drumTracks);
        const int steps = block != nullptr
            ? juce::jlimit (1, 64, juce::roundToInt (blockValue (*block, "dmSteps", (float) element.drumSteps)))
            : juce::jlimit (1, 64, element.drumSteps);
        pattern = block != nullptr
            ? juce::jlimit (0, 7, juce::roundToInt (blockValue (*block, "dmPattern", (float) element.drumPattern)))
            : juce::jlimit (0, 7, element.drumPattern);

        auto area = r.reduced (8);
        area.removeFromTop (28);
        area.removeFromTop (4);
        if (area.isEmpty())
            return false;

        const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
        auto grid = area.withTrimmedLeft (labelW);
        if (grid.isEmpty() || ! grid.contains (pos))
            return false;

        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        if (cellW <= 0.0f || cellH <= 0.0f)
            return false;

        step = juce::jlimit (0, steps - 1, (int) ((pos.x - grid.getX()) / cellW));
        track = juce::jlimit (0, tracks - 1, (int) ((pos.y - area.getY()) / cellH));

        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        active = block != nullptr && blockValue (*block, prefix + "On", 0.0f) >= 0.5f;
        gate = block != nullptr ? juce::jlimit (0.05f, 1.0f, blockValue (*block, prefix + "Gate", 0.35f)) : 0.35f;
        probability = block != nullptr ? juce::jlimit (0.0f, 1.0f, blockValue (*block, prefix + "Prob", 1.0f)) : 1.0f;
        divisions = block != nullptr
            ? juce::jlimit (1, 4, juce::roundToInt (blockValue (*block, prefix + "Div", 1.0f)))
            : 1;
        note = block != nullptr
            ? juce::jlimit (0, 127, juce::roundToInt (blockValue (*block, "dmTrack" + juce::String (track) + "Note", (float) (36 + track))))
            : juce::jlimit (0, 127, 36 + track);

        const auto trackTop = (float) area.getY() + (float) track * cellH;
        const float localY = juce::jlimit (0.0f, 1.0f, ((float) pos.y - trackTop) / cellH);
        velocity = juce::jlimit (0.08f, 1.0f, 1.0f - localY);
        return true;
    }

    bool StudioInstrumentRenderer::handleDrumGridGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& element = *it;
            if (! element.visible || element.type != ElementType::DrumGrid || ! isElementOnCurrentTab (element))
                continue;

            const auto r = animatedElementRect (element, elementRect (element, m));
            if (! r.contains (pos))
                continue;

            auto area = r.reduced (8);
            auto header = area.removeFromTop (28);
            auto playButton = header.removeFromRight (58).reduced (2);
            auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);

            if (! drag && playButton.contains (pos))
            {
                if (onToggleTransport)
                    onToggleTransport();
                repaint (r);
                return true;
            }

            if (! drag && bankStrip.contains (pos))
            {
                constexpr int bankCount = 8;
                constexpr int bankGap = 3;
                const int bankW = juce::jmax (16, (bankStrip.getWidth() - bankGap * (bankCount - 1)) / bankCount);
                for (int bank = 0; bank < bankCount; ++bank)
                {
                    const auto chip = juce::Rectangle<int> (bankStrip.getX() + bank * (bankW + bankGap),
                                                            bankStrip.getY(),
                                                            bankW,
                                                            bankStrip.getHeight()).reduced (0, 2);
                    if (chip.contains (pos))
                    {
                        if (onSetDrumActivePattern)
                            onSetDrumActivePattern (bank);
                        repaint (r);
                        return true;
                    }
                }
            }

            int pattern = -1;
            int track = -1;
            int step = -1;
            int note = -1;
            int divisions = 1;
            float velocity = 0.8f;
            float gate = 0.35f;
            float probability = 1.0f;
            bool active = false;
            if (! drumCellAt (element, r, pos, pattern, track, step, velocity, gate, probability, active, note, divisions))
                return true;

            const bool cycleDivisions = event.mods.isCtrlDown() || event.mods.isCommandDown();
            if (drag && ! drumGridDragActive)
                return true;
            if (drag
                && pattern == lastDrumGridPattern
                && track == lastDrumGridTrack
                && step == lastDrumGridStep)
                return true;

            const bool newValue = cycleDivisions ? true : (drag ? drumGridPaintValue : ! active);
            const int newDivisions = cycleDivisions ? (active ? (divisions >= 4 ? 1 : divisions + 1) : 2) : divisions;
            drumGridDragActive = true;
            drumGridPaintValue = newValue;
            lastDrumGridPattern = pattern;
            lastDrumGridTrack = track;
            lastDrumGridStep = step;

            if (onSetDrumPatternCell
                && onSetDrumPatternCell (pattern, track, step, newValue, velocity, gate, probability, newDivisions))
            {
                if (newValue && note >= 0)
                {
                    if (onNoteOn)
                        onNoteOn (note, velocity);
                    juce::Timer::callAfterDelay (90,
                        [safe = juce::Component::SafePointer<StudioInstrumentRenderer> (this), note]
                        {
                            if (auto* self = safe.getComponent())
                                if (self->onNoteOff)
                                    self->onNoteOff (note);
                        });
                }
                repaint (r);
            }

            return true;
        }

        return false;
    }

    bool StudioInstrumentRenderer::arpLaneStepAt (const LayoutElement& element,
                                                  juce::Rectangle<int> r,
                                                  juce::Point<int> pos,
                                                  int& lane,
                                                  int& step,
                                                  float& velocity) const
    {
        const auto* block = findArpBlock (project.getDspGraph());
        lane = juce::jlimit (0, 15, element.arpLaneIndex);
        const bool orbitMultiRing = element.arpLaneMode.equalsIgnoreCase ("multiRing")
                                 || element.arpLaneMode.equalsIgnoreCase ("orbit")
                                 || element.arpLaneMode.equalsIgnoreCase ("orbitMulti");
        int steps = block != nullptr
            ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) element.arpLaneSteps)))
            : juce::jlimit (1, 128, element.arpLaneSteps);

        auto area = r.reduced (10);
        area.removeFromTop (24);
        const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
        if (size <= 24.0f)
            return false;

        const juce::Point<float> centre ((float) area.getCentreX(), (float) area.getY() + size * 0.52f);
        const float radius = size * 0.40f;
        const auto delta = pos.toFloat() - centre;
        const float distance = delta.getDistanceFromOrigin();
        if (distance < radius * 0.16f || distance > radius * 1.22f)
            return false;

        if (orbitMultiRing)
        {
            const int laneCount = 5;
            const float multiRadius = size * 0.42f;
            const float innerRadius = multiRadius * 0.25f;
            const float outerRadius = multiRadius * 0.94f;
            if (distance < innerRadius - 4.0f || distance > outerRadius + 8.0f)
                return false;

            const float lanePos = juce::jmap (distance, innerRadius, outerRadius, 0.0f, (float) laneCount);
            lane = juce::jlimit (0, laneCount - 1, (int) std::floor (lanePos));
            steps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) element.arpLaneSteps)))
                : juce::jlimit (1, 128, element.arpLaneSteps);
            const float laneCentre = innerRadius + ((outerRadius - innerRadius) / (float) laneCount) * ((float) lane + 0.5f);
            velocity = juce::jlimit (0.05f, 1.0f,
                juce::jmap (distance - laneCentre, -(outerRadius - innerRadius) / (float) laneCount * 0.45f,
                            (outerRadius - innerRadius) / (float) laneCount * 0.45f, 0.05f, 1.0f));
        }

        float angle01 = (std::atan2 (delta.y, delta.x) + juce::MathConstants<float>::halfPi)
            / juce::MathConstants<float>::twoPi;
        while (angle01 < 0.0f) angle01 += 1.0f;
        while (angle01 >= 1.0f) angle01 -= 1.0f;

        const int maxDrawSteps = juce::jmin (steps, 64);
        step = juce::jlimit (0, maxDrawSteps - 1, juce::roundToInt (angle01 * (float) maxDrawSteps) % maxDrawSteps);
        if (! orbitMultiRing)
            velocity = juce::jlimit (0.05f, 1.0f,
                juce::jmap (distance, radius * 0.25f, radius * 0.93f, 0.05f, 1.0f));
        return true;
    }

    bool StudioInstrumentRenderer::handleArpLaneGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& element = *it;
            if (! element.visible || element.type != ElementType::ArpLane || ! isElementOnCurrentTab (element))
                continue;

            const auto r = animatedElementRect (element, elementRect (element, m));
            if (! r.contains (pos))
                continue;

            auto area = r.reduced (10);
            auto header = area.removeFromTop (24);
            auto dragHandle = header.removeFromRight (72).reduced (2, 3);
            if (! drag && dragHandle.contains (pos))
            {
                arpMidiDragArmed = true;
                arpMidiDragStart = pos;
                arpMidiDragElementId = element.id;
                return true;
            }

            if (drag && ! arpLaneDragActive)
                return true;

            int lane = -1;
            int step = -1;
            float velocity = 0.8f;
            auto velocityForLockedLane = [&] (int lockedLane, float& v)
            {
                auto area = r.reduced (10);
                area.removeFromTop (24);
                const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
                if (size <= 24.0f)
                    return false;

                const juce::Point<float> centre ((float) area.getCentreX(), (float) area.getY() + size * 0.52f);
                const float distance = (pos.toFloat() - centre).getDistanceFromOrigin();
                const bool orbitMultiRing = element.arpLaneMode.equalsIgnoreCase ("multiRing")
                                         || element.arpLaneMode.equalsIgnoreCase ("orbit")
                                         || element.arpLaneMode.equalsIgnoreCase ("orbitMulti");
                if (orbitMultiRing)
                {
                    const int laneCount = 5;
                    const float multiRadius = size * 0.42f;
                    const float innerRadius = multiRadius * 0.25f;
                    const float outerRadius = multiRadius * 0.94f;
                    const float band = (outerRadius - innerRadius) / (float) laneCount;
                    const float laneCentre = innerRadius + band * ((float) juce::jlimit (0, laneCount - 1, lockedLane) + 0.5f);
                    v = juce::jlimit (0.05f, 1.0f,
                        juce::jmap (distance - laneCentre, -band * 0.50f, band * 0.50f, 0.05f, 1.0f));
                }
                else
                {
                    const float radius = size * 0.40f;
                    v = juce::jlimit (0.05f, 1.0f,
                        juce::jmap (distance, radius * 0.25f, radius * 0.93f, 0.05f, 1.0f));
                }
                return true;
            };

            if (drag && arpLaneDragActive && lastArpLane >= 0 && lastArpStep >= 0)
            {
                lane = lastArpLane;
                step = lastArpStep;
                if (! velocityForLockedLane (lane, velocity))
                    return true;
            }
            else if (! arpLaneStepAt (element, r, pos, lane, step, velocity))
            {
                return true;
            }

            arpLaneDragActive = true;
            lastArpLane = lane;
            lastArpStep = step;

            if (onSetArpLaneStep && onSetArpLaneStep (lane, step, velocity, true))
                repaint (r);

            return true;
        }

        return false;
    }

    bool StudioInstrumentRenderer::startArpLaneMidiDrag (const LayoutElement& element)
    {
        const auto* block = findArpBlock (project.getDspGraph());
        if (block == nullptr)
        {
            if (onRuntimeStatus)
                onRuntimeStatus ("MIDI drag failed: no Arp Studio or MIDI Playground block exists.");
            return false;
        }

        DspBlock exportBlock = *block;
        const int lane = juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, element.arpLaneIndex);
        MidiPlaygroundPattern::loadBank (exportBlock, lane, false);

        auto outputFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("MidiDrag");
        if (! outputFolder.createDirectory())
        {
            if (onRuntimeStatus)
                onRuntimeStatus ("MIDI drag failed: could not create " + outputFolder.getFullPathName());
            return false;
        }

        auto target = outputFolder.getChildFile (arpLaneMidiFileName (project.getManifest().instrumentName, element.label, lane));
        int duplicateIndex = 2;
        while (target.existsAsFile())
            target = outputFolder.getChildFile (arpLaneMidiFileName (project.getManifest().instrumentName,
                                                                     element.label + "_" + juce::String (duplicateIndex++),
                                                                     lane));

        juce::String error;
        if (! MidiPlaygroundPattern::writeMidiClip (exportBlock, target, 120.0, 60, error))
        {
            if (onRuntimeStatus)
                onRuntimeStatus ("MIDI drag failed: " + error);
            return false;
        }

        juce::StringArray files;
        files.add (target.getFullPathName());
        const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
        if (onRuntimeStatus)
            onRuntimeStatus (started ? "Dragging MIDI clip: " + target.getFileName()
                                     : "MIDI drag failed: OS did not start an external file drag.");
        return started;
    }

    const LayoutElement* StudioInstrumentRenderer::findElementAt (juce::Point<int> position) const
    {
        const auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& element = *it;
            if (! element.visible || element.type == ElementType::Group)
                continue;
            if (! isElementOnCurrentTab (element))
                continue;
            if (animatedElementRect (element, elementRect (element, m)).contains (position))
                return &element;
        }

        return nullptr;
    }

    void StudioInstrumentRenderer::showElementAnimationMenu (const LayoutElement& element,
                                                             const juce::Point<int>& screenPos)
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (element.label.isNotEmpty() ? element.label : element.id);
        menu.addItem (1, "None");
        menu.addItem (2, "Breathe");
        menu.addItem (3, "Pulse");
        menu.addItem (4, "Glow");
        menu.addItem (5, "Shake");
        menu.addItem (6, "Audio Reactive Glow");

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (220)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, elementId = element.id] (int result)
            {
                if (result < 1 || result > 6)
                    return;

                const auto apply = [result] (LayoutElement& el)
                {
                    el.animationRate = result == 5 ? 2.0f
                                     : result == 2 ? 0.55f
                                     : 1.0f;
                    el.audioReactive = result == 6;
                    el.audioReactiveMode = "level";
                    el.audioReactiveAmount = result == 6 ? 0.85f : 0.35f;
                    el.animationMode = result == 1 ? "none"
                                     : result == 2 ? "breathe"
                                     : result == 3 ? "pulse"
                                     : result == 4 ? "glow"
                                     : result == 5 ? "shake"
                                     : "glow";
                };

                project.performLayoutEdit ("Assign visual automation",
                    [elementId, apply] (LayoutModel& layout)
                    {
                        if (auto* el = layout.find (elementId))
                            apply (*el);
                    });

                for (auto& el : elementsCopy)
                    if (el.id == elementId)
                    {
                        apply (el);
                        break;
                    }

                resized();
                repaint();
            });
    }

    void StudioInstrumentRenderer::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            if (const auto* element = findElementAt (e.getPosition()))
                showElementAnimationMenu (*element, e.getScreenPosition());
            return;
        }

        if (handleGranularGesture (e))
            return;

        if (handleXYPadGesture (e))
            return;

        if (handleDrumGridGesture (e, false))
            return;

        if (handleArpLaneGesture (e, false))
            return;

        auto m = metrics();

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            if (! it->visible || it->type != ElementType::TabPanel) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementRect (*it, m);
            if (! r.contains (e.getPosition())) continue;

            const int hit = hitTabIndex (*it, r, e.getPosition());
            if (hit >= 0 && hit < it->tabs.size())
            {
                const auto targetGroup = scopedTabGroupId (*it, it->tabs[hit]);
                if (it->id == "tabs")
                {
                    currentTabGroup = targetGroup;
                    activeTabGroupsByPanel[it->id] = targetGroup;
                }
                else
                    activeTabGroupsByPanel[it->id] = targetGroup;
                rebuild();
                return;
            }
        }

        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& el = elementsCopy[i];
            if (! el.visible || ! isElementOnCurrentTab (el)) continue;

            auto r = animatedElementRect (el, elementRect (el, m));
            if (! r.contains (e.getPosition())) continue;

            if (el.type == ElementType::TabPanel)
            {
                int hit = hitTabIndex (el, r, e.getPosition());
                if (hit >= 0 && hit < el.tabs.size())
                {
                    const auto targetGroup = scopedTabGroupId (el, el.tabs[hit]);
                    if (el.id == "tabs")
                    {
                        currentTabGroup = targetGroup;
                        activeTabGroupsByPanel[el.id] = targetGroup;
                    }
                    else
                        activeTabGroupsByPanel[el.id] = targetGroup;
                    rebuild(); // Rebuild to show/hide elements for new tab
                }
            }
            else if (el.type == ElementType::Dropdown && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
                    juce::PopupMenu menu;
                    std::vector<float> values;
                    if (def->id == "arpLaneMode")
                    {
                        values = { 0.0f, 1.0f };
                        menu.addItem (1, "Bank");
                        menu.addItem (2, "Performance");
                    }
                    else if (def->id == "arpLaneTarget")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                        menu.addItem (1, "Notes");
                        menu.addItem (2, "Drums");
                        menu.addItem (3, "One Shots");
                        menu.addItem (4, "Loops");
                        menu.addItem (5, "Samples");
                    }
                    else if (def->id == "arpLaneDirection")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f };
                        menu.addItem (1, "Forward");
                        menu.addItem (2, "Reverse");
                        menu.addItem (3, "Bounce");
                        menu.addItem (4, "Random");
                    }
                    else if (def->id == "arpLaneControlBank")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                        for (int bank = 0; bank < 5; ++bank)
                            menu.addItem (bank + 1, "Lane " + juce::String (bank + 1));
                    }
                    else if (def->id == "arpLaneGroup")
                    {
                        for (int groupIndex = 0; groupIndex < 8; ++groupIndex)
                        {
                            values.push_back ((float) groupIndex);
                            menu.addItem (groupIndex + 1, "Group " + juce::String (groupIndex + 1));
                        }
                    }
                    else if (def->id == "arpLaneSliderRole")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
                        menu.addItem (1, "Velocity");
                        menu.addItem (2, "Gate");
                        menu.addItem (3, "Probability");
                        menu.addItem (4, "Ratchet");
                        menu.addItem (5, "Mute / Active");
                        menu.addItem (6, "Delay");
                        menu.addItem (7, "Sample Slice");
                        menu.addItem (8, "Transpose");
                        menu.addItem (9, "Filter");
                        menu.addItem (10, "Pan");
                        menu.addItem (11, "FX Send");
                    }
                    else if (def->id == "arpLaneSound")
                    {
                        for (int sound = 0; sound < 16; ++sound)
                        {
                            values.push_back ((float) sound);
                            menu.addItem (sound + 1, orbitLaneSoundName (sound));
                        }
                    }
                    else if (def->id == "arpLaneFxTarget")
                    {
                        static const char* fxTargets[] = { "Delay", "Reverb", "Chorus", "Phaser", "Drive", "Resonance", "Width", "Tape" };
                        for (int target = 0; target < 8; ++target)
                        {
                            values.push_back ((float) target);
                            menu.addItem (target + 1, fxTargets[target]);
                        }
                    }
                    else if (def->id == "arpLanePatternLaunch")
                    {
                        for (int preset = 0; preset < 8; ++preset)
                        {
                            values.push_back ((float) preset);
                            menu.addItem (preset + 1, circleSeqPatternName (preset));
                        }
                    }
                    else if (def->step >= 1.0f && def->max - def->min <= 32.0f)
                    {
                        for (int value = (int) def->min; value <= (int) def->max; value += (int) juce::jmax (1.0f, def->step))
                        {
                            values.push_back ((float) value);
                            menu.addItem ((int) values.size(), juce::String (value) + (def->unit.isNotEmpty() ? " " + def->unit : ""));
                        }
                    }
                    else
                    {
                        values = { def->min, def->defaultValue, def->max };
                        menu.addItem (1, "Min  " + juce::String (def->min, 2));
                        menu.addItem (2, "Default  " + juce::String (def->defaultValue, 2));
                        menu.addItem (3, "Max  " + juce::String (def->max, 2));
                    }
                    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this, parameterId = el.parameterId, values] (int result)
                        {
                            if (result <= 0 || result > (int) values.size()) return;
                            project.getLiveValues().setValue (parameterId, values[(size_t) result - 1]);
                            if (parameterId.startsWith ("arpLane"))
                                project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                            repaint();
                        });
                    return;
                }
            }
            else if ((el.type == ElementType::Knob || el.type == ElementType::Slider || el.type == ElementType::MacroControl)
                     && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
                    activeContinuousParameter = el.parameterId;
                    continuousDragStart = e.getPosition();
                    continuousDragStartValue = project.getLiveValues().getValue (el.parameterId, def->defaultValue);
                    return;
                }
            }
            else if (el.type == ElementType::Toggle && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
                    const auto current = project.getLiveValues().getValue (el.parameterId, def->defaultValue);
                    project.getLiveValues().setValue (el.parameterId, current >= 0.5f ? def->min : def->max);
                    if (el.parameterId.startsWith ("arpLane"))
                        project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                    repaint (r);
                    return;
                }
            }
            else if (el.type == ElementType::Button && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
                    activeMomentaryParameter = el.parameterId;
                    project.getLiveValues().setValue (el.parameterId, def->max);
                    if (el.parameterId.startsWith ("arpLane"))
                        project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                    repaint (r);
                    return;
                }
            }
            else if (el.type == ElementType::Keyboard)
            {
                int note = hitKeyboardNote (r, e.getPosition());
                if (note >= 0 && note != lastPlayedNote)
                {
                    // Release previous note if any
                    if (lastPlayedNote >= 0 && onNoteOff)
                        onNoteOff (lastPlayedNote);

                    lastPlayedNote = note;
                    if (onNoteOn)
                        onNoteOn (note, 0.8f);
                    repaint (r);
                }
            }
            else if (el.type == ElementType::DrumPad || el.type == ElementType::PadGrid)
            {
                const int rows = el.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, el.padRows);
                const int cols = el.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, el.padCols);
                const int gap = el.type == ElementType::DrumPad ? 0 : 4;
                const auto inner = el.type == ElementType::PadGrid ? r.reduced (4) : r;
                if (inner.isEmpty() || ! inner.contains (e.getPosition()))
                    return;

                const float padW = (float) (inner.getWidth() - gap * (cols - 1)) / (float) cols;
                const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
                if (padW <= 0.0f || padH <= 0.0f)
                    return;

                const int col = juce::jlimit (0, cols - 1, (int) ((e.x - inner.getX()) / (padW + gap)));
                const int row = juce::jlimit (0, rows - 1, (int) ((e.y - inner.getY()) / (padH + gap)));
                const int note = juce::jlimit (0, 127, el.padBaseNote + row * cols + col);
                const float yNorm = juce::jlimit (0.0f, 1.0f,
                    (float) (e.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));
                const float velocity = juce::jlimit (0.2f, 1.0f, 0.4f + yNorm * 0.6f);

                if (lastPlayedNote >= 0 && lastPlayedNote != note && onNoteOff)
                    onNoteOff (lastPlayedNote);
                lastPlayedNote = note;
                if (onNoteOn)
                    onNoteOn (note, velocity);
                repaint (r);
                return;
            }
        }
    }

    void StudioInstrumentRenderer::mouseDrag (const juce::MouseEvent& e)
    {
        if (arpMidiDragArmed && arpMidiDragElementId.isNotEmpty())
        {
            const auto delta = e.getPosition() - arpMidiDragStart;
            if (std::abs (delta.x) + std::abs (delta.y) >= 8)
            {
                for (const auto& element : elementsCopy)
                {
                    if (element.id == arpMidiDragElementId)
                    {
                        startArpLaneMidiDrag (element);
                        break;
                    }
                }
                arpMidiDragArmed = false;
                arpMidiDragElementId.clear();
            }
            return;
        }

        if (handleGranularGesture (e))
            return;

        if (handleXYPadGesture (e))
            return;

        if (handleDrumGridGesture (e, true))
            return;

        if (handleArpLaneGesture (e, true))
            return;

        if (activeContinuousParameter.isNotEmpty())
        {
            if (auto* def = project.getParameters().find (activeContinuousParameter))
            {
                const int deltaY = continuousDragStart.y - e.getPosition().y;
                const int deltaX = e.getPosition().x - continuousDragStart.x;
                const float drag = (float) deltaY + (float) deltaX * 0.35f;
                const float sensitivity = def->step >= 1.0f ? 0.08f : 0.0025f;
                float next = continuousDragStartValue + drag * (def->max - def->min) * sensitivity;
                next = juce::jlimit (def->min, def->max, next);
                if (def->step >= 1.0f)
                    next = std::round ((next - def->min) / juce::jmax (0.0001f, def->step)) * def->step + def->min;
                project.getLiveValues().setValue (activeContinuousParameter, next);
                project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                repaint();
            }
            return;
        }

        if (lastPlayedNote < 0) return;

        auto m = metrics();
        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& el = elementsCopy[i];
            if (el.type != ElementType::Keyboard
                && el.type != ElementType::DrumPad
                && el.type != ElementType::PadGrid) continue;
            if (! el.visible || ! isElementOnCurrentTab (el)) continue;

            auto r = animatedElementRect (el, elementRect (el, m));
            int note = -1;
            float velocity = 0.8f;
            if (el.type == ElementType::Keyboard)
            {
                note = hitKeyboardNote (r, e.getPosition());
            }
            else
            {
                const int rows = el.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, el.padRows);
                const int cols = el.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, el.padCols);
                const int gap = el.type == ElementType::DrumPad ? 0 : 4;
                const auto inner = el.type == ElementType::PadGrid ? r.reduced (4) : r;
                if (! inner.contains (e.getPosition()))
                    continue;
                const float padW = (float) (inner.getWidth() - gap * (cols - 1)) / (float) cols;
                const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
                if (padW <= 0.0f || padH <= 0.0f)
                    continue;
                const int col = juce::jlimit (0, cols - 1, (int) ((e.x - inner.getX()) / (padW + gap)));
                const int row = juce::jlimit (0, rows - 1, (int) ((e.y - inner.getY()) / (padH + gap)));
                note = juce::jlimit (0, 127, el.padBaseNote + row * cols + col);
                const float yNorm = juce::jlimit (0.0f, 1.0f,
                    (float) (e.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));
                velocity = juce::jlimit (0.2f, 1.0f, 0.4f + yNorm * 0.6f);
            }
            if (note >= 0 && note != lastPlayedNote)
            {
                // Release previous note
                if (onNoteOff)
                    onNoteOff (lastPlayedNote);

                lastPlayedNote = note;
                if (onNoteOn)
                    onNoteOn (note, velocity);
                repaint (r);
            }
        }
    }

    void StudioInstrumentRenderer::mouseUp (const juce::MouseEvent&)
    {
        if (lastPlayedNote >= 0 && onNoteOff)
        {
            onNoteOff (lastPlayedNote);
            lastPlayedNote = -1;
            repaint();
        }

        if (activeMomentaryParameter.isNotEmpty())
        {
            if (auto* def = project.getParameters().find (activeMomentaryParameter))
                project.getLiveValues().setValue (activeMomentaryParameter, def->min);
            activeMomentaryParameter.clear();
            repaint();
        }

        activeContinuousParameter.clear();

        drumGridDragActive = false;
        lastDrumGridPattern = -1;
        lastDrumGridTrack = -1;
        lastDrumGridStep = -1;
        arpLaneDragActive = false;
        arpMidiDragArmed = false;
        arpMidiDragElementId.clear();
        lastArpLane = -1;
        lastArpStep = -1;
    }

    bool StudioInstrumentRenderer::handleXYPadGesture (const juce::MouseEvent& e)
    {
        auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            auto& el = *it;
            if (! el.visible || el.type != ElementType::XYPad || ! isElementOnCurrentTab (el))
                continue;

            auto r = elementRect (el, m);
            if (! r.contains (e.getPosition()))
                continue;

            auto* def = project.getParameters().find (el.parameterId);
            if (def == nullptr)
                return true;

            const auto inner = r.reduced (6);
            const float normalised = juce::jlimit (0.0f, 1.0f,
                (float) (e.getPosition().x - inner.getX()) / (float) juce::jmax (1, inner.getWidth()));
            project.getLiveValues().setValue (def->id, juce::jmap (normalised, 0.0f, 1.0f, def->min, def->max));
            repaint (r.expanded (2));
            return true;
        }
        return false;
    }

    bool StudioInstrumentRenderer::handleGranularGesture (const juce::MouseEvent& e)
    {
        auto m = metrics();
        const auto position = e.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            auto& el = *it;
            if (! el.visible || el.type != ElementType::GranularField || ! isElementOnCurrentTab (el))
                continue;

            auto r = animatedElementRect (el, elementRect (el, m));
            if (! r.contains (position))
                continue;

            auto setParameter = [this] (const juce::String& parameterId, float value)
            {
                auto* def = project.getParameters().find (parameterId);
                if (def == nullptr)
                    return false;

                if (def->enabledBy == "granularOn")
                {
                    if (auto* gate = project.getParameters().find ("granularOn"))
                        project.getLiveValues().setValue ("granularOn", gate->max);
                }

                project.getLiveValues().setValue (parameterId, juce::jlimit (def->min, def->max, value));
                return true;
            };

            bool changed = false;
            if (project.getParameters().find ("granularOn") != nullptr)
                changed = setParameter ("granularOn", 1.0f) || changed;

            auto controlArea = r.reduced (12, 10);
            controlArea.removeFromTop (22);
            controlArea.removeFromTop (4);
            if (controlArea.getHeight() < 60 || controlArea.getWidth() < 160)
                return true;

            auto chipArea = controlArea.removeFromBottom (24);
            controlArea.removeFromBottom (4);
            const juce::StringArray chips { "FWD", "REV", "PING", "MULTI", "FREEZE" };
            if (chipArea.contains (position))
            {
                const int chipGap = 5;
                const int chipW = juce::jmax (48, (chipArea.getWidth() - chipGap * (chips.size() - 1)) / chips.size());
                for (int chipIndex = 0; chipIndex < chips.size(); ++chipIndex)
                {
                    auto chip = juce::Rectangle<int> (chipArea.getX() + chipIndex * (chipW + chipGap),
                                                      chipArea.getY(), chipW, chipArea.getHeight()).reduced (1);
                    if (! chip.contains (position))
                        continue;

                    if (chipIndex < 4)
                        changed = setParameter ("granularDirection", (float) chipIndex) || changed;
                    else
                    {
                        const auto current = project.getLiveValues().getValue ("granularFreeze", 0.0f);
                        changed = setParameter ("granularFreeze", current >= 0.5f ? 0.0f : 1.0f) || changed;
                    }

                    if (changed)
                        repaint (r.expanded (2));
                    return true;
                }
            }

            const auto inner = controlArea;
            const float x = juce::jlimit (0.0f, 1.0f,
                (float) (position.x - inner.getX()) / (float) juce::jmax (1, inner.getWidth()));
            const float y = juce::jlimit (0.0f, 1.0f,
                (float) (position.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));

            if (e.mods.isShiftDown())
            {
                changed = setParameter ("granularScan", juce::jmap (x, 0.0f, 1.0f, -3.0f, 3.0f)) || changed;
                changed = setParameter ("granularSpread", 1.0f - y) || changed;
            }
            else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            {
                changed = setParameter ("granularDirection", (float) juce::jlimit (0, 3, (int) std::floor (x * 4.0f))) || changed;
                changed = setParameter ("granularReverse", 1.0f - y) || changed;
            }
            else if (e.mods.isAltDown())
            {
                changed = setParameter ("granularPitchSpread", juce::jmap (x, 0.0f, 1.0f, 0.0f, 36.0f)) || changed;
                changed = setParameter ("granularPanSpread", 1.0f - y) || changed;
            }
            else
            {
                changed = setParameter ("sampleStart", x) || changed;
                changed = setParameter ("sampleLength", juce::jmap (1.0f - y, 0.0f, 1.0f, 0.01f, 1.0f)) || changed;
            }

            if (changed)
                repaint (r.expanded (2));
            return true;
        }
        return false;
    }

    bool StudioInstrumentRenderer::advanceGranularFields()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (lastGranularAdvanceSeconds <= 0.0)
        {
            lastGranularAdvanceSeconds = now;
            return false;
        }

        const float dt = juce::jlimit (0.0f, 0.10f, (float) (now - lastGranularAdvanceSeconds));
        lastGranularAdvanceSeconds = now;
        if (dt <= 0.0f)
            return false;

        bool changed = false;
        for (const auto& element : elementsCopy)
        {
            if (! element.visible || element.type != ElementType::GranularField || ! isElementOnCurrentTab (element))
                continue;

            const auto* startDef = project.getParameters().find ("sampleStart");
            if (startDef == nullptr)
                continue;

            const float on = project.getLiveValues().getValue ("granularOn", 0.0f);
            const float freeze = project.getLiveValues().getValue ("granularFreeze", 0.0f);
            const float scan = project.getLiveValues().getValue ("granularScan", 0.0f);
            if (on < 0.5f || freeze >= 0.5f || std::abs (scan) < 0.0001f)
                continue;

            float next = project.getLiveValues().getValue ("sampleStart", startDef->defaultValue)
                       + scan * dt * 0.055f;
            next -= std::floor (next);
            project.getLiveValues().setValue ("sampleStart", juce::jlimit (startDef->min, startDef->max, next));
            changed = true;
        }
        return changed;
    }

    void StudioInstrumentRenderer::timerCallback()
    {
        if (advanceGranularFields())
        {
            repaint();
            return;
        }

        for (const auto& item : elementsCopy)
        {
            if (! item.visible || ! isElementOnCurrentTab (item))
                continue;
            if ((item.animationMode.isNotEmpty() && item.animationMode != "none")
                || item.audioReactive
                || item.type == ElementType::ArpLane
                || item.type == ElementType::GranularField
                || item.type == ElementType::SpectrumAnalyzer)
            {
                if (item.animationMode.isNotEmpty() && item.animationMode != "none")
                    resized();
                repaint();
                return;
            }
        }
    }

    int StudioInstrumentRenderer::hitKeyboardNote (juce::Rectangle<int> r, juce::Point<int> p) const
    {
        const auto whiteNotes = pianoWhiteNotes();
        const auto bounds = r.reduced (6, 6).toFloat();
        const float whiteKeyW = bounds.getWidth() / (float) whiteNotes.size();
        const float blackKeyW = whiteKeyW * 0.62f;
        const float blackKeyH = bounds.getHeight() * 0.62f;

        for (int midiNote = kPianoFirstMidiNote; midiNote <= kPianoLastMidiNote; ++midiNote)
        {
            if (! isBlackPianoKey (midiNote))
                continue;

            const float x = bounds.getX() + (float) whiteNotesBefore (midiNote) * whiteKeyW - blackKeyW * 0.5f;
            if (x < bounds.getX() - 1.0f || x + blackKeyW > bounds.getRight() + 1.0f)
                continue;

            if (juce::Rectangle<float> (x, bounds.getY(), blackKeyW, blackKeyH).contains (p.toFloat()))
                return midiNote;
        }

        for (int i = 0; i < (int) whiteNotes.size(); ++i)
            if (juce::Rectangle<float> (bounds.getX() + (float) i * whiteKeyW, bounds.getY(),
                                        whiteKeyW - 1.0f, bounds.getHeight()).contains (p.toFloat()))
                return whiteNotes[(size_t) i];

        return -1;
    }

    //------------------------------------------------------------------------------
    // Drawing helpers

    void StudioInstrumentRenderer::drawHeroPlaceholder (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        if (heroImage.isValid())
            g.drawImage (heroImage, r.toFloat(), juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillRect (r);
            g.setColour (juce::Colours::white);
            g.setFont (14.0f);
            g.drawText ("Hero Art", r, juce::Justification::centred);
        }
    }

    void StudioInstrumentRenderer::drawMeter (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        const auto bounds = r;
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (bounds);
        g.setColour (PatchCraftLookAndFeel::accent());
        const float level = audioReactiveLevel.load (std::memory_order_relaxed);
        int fillH = (int) (bounds.getHeight() * level);
        g.fillRect (r.removeFromBottom (fillH));
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (bounds, 1);
    }

    void StudioInstrumentRenderer::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label) const
    {
        g.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.5f));
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (r, 1);
        if (label.isNotEmpty())
        {
            g.setFont (12.0f);
            g.drawText (label, r.removeFromTop (20), juce::Justification::left, true);
        }
    }

    void StudioInstrumentRenderer::drawDropdown (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& display) const
    {
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (r, 1);
        g.setFont (12.0f);
        g.drawText (display, r.reduced (4), juce::Justification::centredLeft, true);
        g.drawText ("v", r.reduced (4), juce::Justification::centredRight, true);
    }

    void StudioInstrumentRenderer::drawKeyboard (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        const auto whiteNotes = pianoWhiteNotes();
        const auto body = r.toFloat();
        const auto bounds = r.reduced (6, 6).toFloat();
        const float whiteKeyW = bounds.getWidth() / (float) whiteNotes.size();
        const float blackKeyW = whiteKeyW * 0.62f;
        const float blackKeyH = bounds.getHeight() * 0.62f;

        g.setColour (juce::Colour (0xff05060a).withAlpha (0.94f));
        g.fillRoundedRectangle (body, 8.0f);
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.78f));
        g.drawRoundedRectangle (body.reduced (0.5f), 8.0f, 1.0f);

        for (int i = 0; i < (int) whiteNotes.size(); ++i)
        {
            const auto key = juce::Rectangle<float> (bounds.getX() + (float) i * whiteKeyW,
                                                     bounds.getY(),
                                                     whiteKeyW - 1.0f,
                                                     bounds.getHeight());
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2ead9), key.getX(), key.getY(),
                                                     juce::Colour (0xffd8c8aa), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.2f);
            g.setColour (juce::Colour (0xff7d705d));
            g.drawRoundedRectangle (key, 1.2f, 0.45f);
        }

        for (int midiNote = kPianoFirstMidiNote; midiNote <= kPianoLastMidiNote; ++midiNote)
        {
            if (! isBlackPianoKey (midiNote))
                continue;

            const float x = bounds.getX() + (float) whiteNotesBefore (midiNote) * whiteKeyW - blackKeyW * 0.5f;
            if (x < bounds.getX() - 1.0f || x + blackKeyW > bounds.getRight() + 1.0f)
                continue;

            const auto key = juce::Rectangle<float> (x, bounds.getY(), blackKeyW, blackKeyH);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff202226), key.getX(), key.getY(),
                                                     juce::Colour (0xff050507), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.4f);
            g.setColour (juce::Colour (0xff050505));
            g.drawRoundedRectangle (key, 1.4f, 0.55f);
        }
    }

    void StudioInstrumentRenderer::drawTabPanel (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e) const
    {
        const int n = juce::jmax (1, e.tabs.size());
        const float tabW = (float) r.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            const auto label = i < e.tabs.size() ? e.tabs[i] : juce::String ("Tab");
            const auto group = scopedTabGroupId (e, label);
            const auto found = activeTabGroupsByPanel.find (e.id);
            const auto activeGroup = found != activeTabGroupsByPanel.end()
                ? found->second
                : (e.id == "tabs" ? currentTabGroup
                                   : (e.tabs.isEmpty() ? juce::String() : scopedTabGroupId (e, e.tabs[0])));
            const bool active = (group == activeGroup);
            const float x = r.getX() + (float) i * tabW;
            juce::Rectangle<float> tabRect (x, (float) r.getY(), tabW, (float) r.getHeight());

            g.setColour (active ? PatchCraftLookAndFeel::textBright()
                                : juce::Colour (0xffb8bcc4));
            g.setFont (juce::Font (juce::jmax (10.0f, tabRect.getHeight() * 0.42f), juce::Font::bold));
            g.drawText (label.toUpperCase(), tabRect.toNearestInt(), juce::Justification::centred, true);

            if (active)
            {
                g.setColour (PatchCraftLookAndFeel::accent());
                g.fillRect (tabRect.removeFromBottom (2.0f).toNearestInt());
            }
        }
    }

    void StudioInstrumentRenderer::drawLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text,
                                              float fontSize, juce::Colour colour) const
    {
        g.setColour (colour.isTransparent() ? PatchCraftLookAndFeel::text() : colour);
        g.setFont (juce::Font (juce::jlimit (8.0f, 36.0f, fontSize),
                               fontSize >= 16.0f ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText (text, r, juce::Justification::centredLeft, 2, 0.92f);
    }

    void StudioInstrumentRenderer::drawRuntimeControl (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e) const
    {
        const auto* def = project.getParameters().find (e.parameterId);
        const auto minValue = def != nullptr ? def->min : 0.0f;
        const auto maxValue = def != nullptr ? def->max : 1.0f;
        const auto fallback = def != nullptr ? def->defaultValue : juce::jlimit (0.0f, 1.0f, e.controlPreviewValue);
        const auto value = project.getLiveValues().getValue (e.parameterId, fallback);
        const auto norm = juce::jlimit (0.0f, 1.0f, (value - minValue) / juce::jmax (0.0001f, maxValue - minValue));
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto fill = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;

        if ((e.type == ElementType::Knob || e.type == ElementType::Slider)
            && e.filmstripAsset.isNotEmpty())
        {
            const auto filmstripFile = juce::File::isAbsolutePath (e.filmstripAsset)
                ? juce::File (e.filmstripAsset)
                : project.getProjectFolder().getChildFile (e.filmstripAsset);

            if (auto strip = assets.loadControlFilmstrip (filmstripFile, e.filmstripFrames, e.filmstripVertical); strip.isValid())
            {
                const int detectedFrames = PatchCraftLookAndFeel::detectFilmstripFrames (strip, e.filmstripVertical);
                const int frames = e.filmstripFrames > 1
                    ? e.filmstripFrames
                    : juce::jmax (1, detectedFrames);
                PatchCraftLookAndFeel::drawFilmstripFrame (g, r, strip, frames, norm, e.filmstripVertical);
                return;
            }
        }

        if (e.type == ElementType::Slider)
        {
            auto track = r.reduced (r.getWidth() / 3, 6).toFloat();
            g.setColour (fill);
            g.fillRoundedRectangle (track, 4.0f);
            g.setColour (accent.withAlpha (0.85f));
            auto active = track;
            active.setY (juce::jmap (norm, track.getBottom(), track.getY()));
            g.fillRoundedRectangle (active, 4.0f);
            g.setColour (border);
            g.drawRoundedRectangle (track, 4.0f, 1.0f);
            return;
        }

        if (e.type == ElementType::MacroControl)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
            r = r.reduced (8, 6);
            auto title = r.removeFromTop (18);
            g.setColour (accent);
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText ((e.label.isNotEmpty() ? e.label : "Macro").toUpperCase(),
                        title, juce::Justification::centredLeft, true);
            r.removeFromTop (2);
        }

        juce::Rectangle<int> macroLanes;
        if (e.type == ElementType::MacroControl && r.getWidth() > 118)
            macroLanes = r.removeFromRight (juce::jmax (58, r.getWidth() / 2)).reduced (4, 2);

        auto dial = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                             juce::jmin (r.getWidth(), r.getHeight())).toFloat().reduced (3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (dial.translated (0.0f, 2.0f));
        g.setColour (fill);
        g.fillEllipse (dial);
        g.setColour (border);
        g.drawEllipse (dial, 1.0f);

        const float start = juce::degreesToRadians (-135.0f);
        const float end = juce::degreesToRadians (135.0f);
        juce::Path arc;
        arc.addCentredArc (dial.getCentreX(), dial.getCentreY(),
                           dial.getWidth() * 0.43f, dial.getHeight() * 0.43f,
                           0.0f, start, juce::jmap (norm, start, end), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.0f, dial.getWidth() * 0.08f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        const auto angle = juce::jmap (norm, start, end);
        const auto radius = dial.getWidth() * 0.32f;
        const auto centre = dial.getCentre();
        g.drawLine (centre.x, centre.y,
                    centre.x + std::cos (angle) * radius,
                    centre.y + std::sin (angle) * radius,
                    juce::jmax (1.0f, dial.getWidth() * 0.04f));

        if (e.type == ElementType::MacroControl && ! macroLanes.isEmpty())
        {
            juce::StringArray targets;
            for (const auto& macro : project.getDspGraph().macros)
                if (macro.macroId == e.parameterId)
                    targets.add (macro.targetId);

            if (targets.isEmpty())
                targets.addArray ({ "No routes", "Select in Inspector", "Add target", "Apply" });

            for (int i = 0; i < juce::jmin (4, targets.size()); ++i)
            {
                auto row = macroLanes.removeFromTop (juce::jmax (14, macroLanes.getHeight() / (4 - i))).reduced (0, 2);
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                g.fillRoundedRectangle (row.toFloat(), 3.0f);
                g.setColour (targets[i] == "No routes" ? border.withAlpha (0.35f) : accent.withAlpha (0.62f));
                g.fillRoundedRectangle (row.withWidth (juce::roundToInt ((float) row.getWidth() * norm)).toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (8.0f, juce::Font::bold));
                g.drawText (targets[i], row.reduced (4, 0), juce::Justification::centredLeft, true);
            }
        }
    }

    int StudioInstrumentRenderer::hitTabIndex (const LayoutElement& e, juce::Rectangle<int> r, juce::Point<int> p) const
    {
        if (e.tabs.isEmpty()) return -1;
        int tabW = r.getWidth() / (int) e.tabs.size();
        int idx = (p.x - r.getX()) / tabW;
        return juce::jlimit (0, (int) e.tabs.size() - 1, idx);
    }

} // namespace patchcraft
