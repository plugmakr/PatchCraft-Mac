#include "CanvasEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <cmath>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static float blockValue (const DspBlock& block, const juce::String& key, float fallback)
        {
            const auto it = block.values.find (key);
            return it != block.values.end() ? it->second : fallback;
        }

        static const DspBlock* findDrumMachineBlock (const DspGraph& graph)
        {
            for (const auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("drum") || block.values.find ("dmTracks") != block.values.end())
                    return &block;
            return nullptr;
        }

        static DspBlock* findDrumMachineBlock (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
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

        static DspBlock* findArpBlock (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
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

        static DspBlock& ensureArpBlock (DspGraph& graph)
        {
            if (auto* existing = findArpBlock (graph))
                return *existing;

            DspBlock block;
            block.section = "mod";
            block.type = "midiPlayground";
            block.name = "ArpLane Performance";
            block.targetId = "filterCutoff";
            block.enabled = true;
            block.id = "midi_playground";

            int suffix = 2;
            auto idExists = [&] (const juce::String& id)
            {
                for (const auto& existing : graph.blocks)
                    if (existing.id == id)
                        return true;
                return false;
            };
            while (idExists (block.id))
                block.id = "midi_playground_" + juce::String (suffix++);

            block.values["amount"] = 0.35f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
            block.values["arpGate"] = 0.58f;
            block.values["arpSteps"] = 16.0f;
            block.values["arpPattern"] = 0.0f;
            block.values["arpOctaves"] = 1.0f;
            block.values["arpSwing"] = 0.0f;
            block.values["mpActiveBank"] = 0.0f;
            block.values["mpProbability"] = 1.0f;
            block.values["mpRatchet"] = 1.0f;
            block.values["mpSampleControl"] = 0.0f;
            block.values["mpSampleSliceCount"] = 1.0f;
            block.values["mpEuclideanPulses"] = 0.0f;
            block.values["mpEuclideanRotate"] = 0.0f;
            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
            return graph.blocks.back();
        }

        static juce::String arpBankPrefix (int lane)
        {
            return "mpBank" + juce::String (juce::jlimit (0, 15, lane) + 1) + "_";
        }

        static void setArpLaneValue (DspBlock& block, int lane, const juce::String& key, float newValue)
        {
            block.values[arpBankPrefix (lane) + key] = newValue;
            if (lane == juce::jlimit (0, 15, juce::roundToInt (blockValue (block, "mpActiveBank", 0.0f))))
                block.values[key] = newValue;
        }

        static void setArpLaneMetadata (DspBlock& block, int lane, const juce::String& key, const juce::String& newValue)
        {
            block.metadata["arpLane" + juce::String (lane + 1) + key] = newValue;
        }

        static void applyArpLaneSliderBankToGraph (PatchCraftProject& project, DspBlock& block)
        {
            auto value = [&] (const juce::String& id, float fallback)
            {
                if (auto* def = project.getParameters().find (id))
                    return project.getLiveValues().getValue (id, def->defaultValue);
                return project.getLiveValues().getValue (id, fallback);
            };

            const int lane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneControlBank", value ("arpLaneIndex", 0.0f))));
            const int role = juce::jlimit (0, 10, juce::roundToInt (value ("arpLaneSliderRole", 0.0f)));
            const int slots = juce::jlimit (1, 64, juce::roundToInt (value ("arpLaneSampleSlots", 1.0f)));
            block.values["mpActiveBank"] = (float) lane;

            for (int step = 0; step < 16; ++step)
            {
                const float v = juce::jlimit (0.0f, 1.0f, value ("arpLaneStep" + juce::String (step + 1),
                                                                 step % 4 == 0 ? 0.92f : 0.68f));
                const auto suffix = juce::String (step);
                if (role == 0)
                    setArpLaneValue (block, lane, "mpVelocity" + suffix, v);
                else if (role == 1)
                    setArpLaneValue (block, lane, "mpGate" + suffix, juce::jlimit (0.05f, 1.0f, 0.05f + v * 0.95f));
                else if (role == 2)
                    setArpLaneValue (block, lane, "mpStepProb" + suffix, v);
                else if (role == 3)
                    setArpLaneValue (block, lane, "mpStepDiv" + suffix, (float) juce::jlimit (1, 8, 1 + juce::roundToInt (v * 7.0f)));
                else if (role == 4)
                    setArpLaneValue (block, lane, "mpStep" + suffix + "On", v >= 0.5f ? 1.0f : 0.0f);
                else if (role == 5)
                    setArpLaneValue (block, lane, "mpStepDelay" + suffix, juce::jlimit (0.0f, 0.85f, v * 0.85f));
                else if (role == 6)
                    setArpLaneValue (block, lane, "mpSampleSlice" + suffix, (float) juce::jlimit (0, slots - 1, juce::roundToInt (v * (float) juce::jmax (1, slots - 1))));
                else if (role == 7)
                    setArpLaneValue (block, lane, "mpStepTranspose" + suffix, (float) juce::jlimit (-24, 24, juce::roundToInt (v * 48.0f - 24.0f)));
                else if (role == 8)
                    setArpLaneValue (block, lane, "mpAutoFilter" + suffix, v);
                else if (role == 9)
                    setArpLaneValue (block, lane, "mpAutoPan" + suffix, juce::jlimit (-1.0f, 1.0f, v * 2.0f - 1.0f));
                else if (role == 10)
                    setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, v);
            }

            setArpLaneMetadata (block, lane, "SliderRole", role == 0 ? "velocity"
                                                       : role == 1 ? "gate"
                                                       : role == 2 ? "probability"
                                                       : role == 3 ? "ratchet"
                                                       : role == 4 ? "mute"
                                                       : role == 5 ? "delay"
                                                       : role == 6 ? "slice"
                                                       : role == 7 ? "transpose"
                                                       : role == 8 ? "filter"
                                                       : role == 9 ? "pan"
                                                       : "fxSend");
        }

        static void applyArpLaneParameterToGraph (PatchCraftProject& project,
                                                  const juce::String& parameterId)
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
            const int lane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneIndex", 0.0f)));
            const int steps = juce::jlimit (1, 128, juce::roundToInt (value ("arpLaneSteps", 16.0f)));
            const int target = juce::jlimit (0, 4, juce::roundToInt (value ("arpLaneTarget", 0.0f)));
            const int direction = juce::jlimit (0, 3, juce::roundToInt (value ("arpLaneDirection", 0.0f)));

            block.values["mpActiveBank"] = (float) lane;
            setArpLaneValue (block, lane, "arpSteps", (float) steps);
            setArpLaneValue (block, lane, "arpPattern", direction == 1 ? 1.0f : direction == 2 ? 2.0f : direction == 3 ? 7.0f : 0.0f);
            setArpLaneValue (block, lane, "arpGate", juce::jlimit (0.05f, 1.0f, value ("arpLaneGate", 0.58f)));
            setArpLaneValue (block, lane, "arpSwing", juce::jlimit (0.0f, 0.5f, value ("arpLaneSwing", 0.0f)));
            setArpLaneValue (block, lane, "rate", juce::jlimit (0.0625f, 16.0f, value ("arpLaneRate", 1.0f)));
            const bool fillActive = value ("arpLaneFillMomentary", 0.0f) >= 0.5f
                                 || value ("arpLaneFillLatch", 0.0f) >= 0.5f;
            const int basePulses = juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneEuclideanPulses", 0.0f)));
            const int fillPulses = juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneFillPulses", 0.0f)));
            const float baseProbability = juce::jlimit (0.0f, 1.0f, value ("arpLaneProbability", 1.0f));
            const float fillProbability = juce::jlimit (0.0f, 1.0f, value ("arpLaneFillProbability", 0.0f));
            setArpLaneValue (block, lane, "mpProbability", fillActive && fillProbability > 0.0f ? fillProbability : baseProbability);
            setArpLaneValue (block, lane, "mpRatchet", juce::jlimit (1.0f, 8.0f, value ("arpLaneRatchet", 1.0f)));
            setArpLaneValue (block, lane, "mpEuclideanPulses", (float) (fillActive && fillPulses > 0 ? fillPulses : basePulses));
            setArpLaneValue (block, lane, "mpEuclideanRotate", (float) juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRotate", 0.0f))));
            setArpLaneValue (block, lane, "mpSampleControl", target == 0 ? 0.0f : 1.0f);
            setArpLaneValue (block, lane, "mpSampleSliceCount", (float) juce::jlimit (1, 64, juce::roundToInt (value ("arpLaneSampleSlots", 1.0f))));
            setArpLaneValue (block, lane, "mpLaneMute", value ("arpLaneMute", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            setArpLaneValue (block, lane, "mpLaneSolo", value ("arpLaneSolo", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            block.values["mpPatternLaunch"] = (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLanePatternLaunch", 0.0f)));

            setArpLaneMetadata (block, lane, "Mode", juce::roundToInt (value ("arpLaneMode", 0.0f)) == 1 ? "performance" : "bank");
            setArpLaneMetadata (block, lane, "Target", target == 1 ? "drums" : target == 2 ? "oneShots" : target == 3 ? "loops" : target == 4 ? "samples" : "notes");
            setArpLaneMetadata (block, lane, "Direction", direction == 1 ? "reverse" : direction == 2 ? "bounce" : direction == 3 ? "random" : "forward");
            setArpLaneMetadata (block, lane, "RootNote", juce::String (juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRootNote", 60.0f)))));
            setArpLaneMetadata (block, lane, "FillPulses", juce::String (juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneFillPulses", 0.0f)))));
            setArpLaneMetadata (block, lane, "FillProbability", juce::String (juce::jlimit (0.0f, 1.0f, value ("arpLaneFillProbability", 0.0f)), 2));

            applyArpLaneSliderBankToGraph (project, block);

            graph.userConfigured = true;
        }

        static juce::String defaultDrumTrackLabel (int track)
        {
            static const char* labels[] =
            {
                "Kick", "Snare", "Closed Hat", "Open Hat",
                "Clap", "Low Tom", "Perc", "Crash",
                "Ride", "Rim", "Shaker", "FX",
                "Pad 13", "Pad 14", "Pad 15", "Pad 16"
            };
            return track >= 0 && track < 16 ? juce::String (labels[track])
                                            : "Track " + juce::String (track + 1);
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

        static juce::String stringAtOr (const juce::StringArray& values,
                                        int index,
                                        const juce::String& fallback)
        {
            return (index >= 0 && index < values.size()) ? values[index] : fallback;
        }

        static DspBlock& ensureDrumMachineBlock (DspGraph& graph)
        {
            if (auto* existing = findDrumMachineBlock (graph))
                return *existing;

            DspBlock block;
            block.section = "mod";
            block.type = "drumMachine";
            block.name = "Drum Machine Performance";
            block.targetId = "midiDrumMachine";
            block.enabled = true;
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

            block.values["dmTracks"] = 8.0f;
            block.values["dmSteps"] = 16.0f;
            block.values["dmPattern"] = 0.0f;
            block.values["dmTransport"] = 1.0f;
            block.values["dmGate"] = 0.65f;
            block.values["dmProbability"] = 1.0f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
            block.values["enabled"] = 1.0f;
            for (int track = 0; track < 16; ++track)
            {
                block.values["dmTrack" + juce::String (track) + "Note"] = (float) defaultDrumTrackNote (track);
                block.metadata["dmTrack" + juce::String (track) + "Label"] = defaultDrumTrackLabel (track);
            }
            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
            return graph.blocks.back();
        }

        static void drawArpLanePreview (juce::Graphics& g,
                                        juce::Rectangle<int> r,
                                        const LayoutElement& element,
                                        const DspGraph& graph)
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

            const auto bg = element.backgroundColour.isTransparent() ? juce::Colour (0xff10141a) : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : element.accentColour;
            const auto text = PatchCraftLookAndFeel::text();
            const auto dim = PatchCraftLookAndFeel::textDim();

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, element.cornerRadius));
            g.setColour (laneSelected ? accent : border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, element.cornerRadius), laneSelected ? 2.0f : 1.0f);

            auto area = r.reduced (12);
            auto header = area.removeFromTop (28);
            auto dragHandle = header.removeFromRight (78).reduced (2, 4);
            g.setFont (juce::FontOptions (12.5f).withStyle ("bold"));
            g.setColour (accent);
            g.drawText (juce::String (lane + 1), header.removeFromLeft (24), juce::Justification::centredLeft, true);
            g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "ARP LANE",
                        header, juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.90f));
            g.fillRoundedRectangle (dragHandle.toFloat(), 5.0f);
            g.setColour (accent.withAlpha (0.78f));
            g.drawRoundedRectangle (dragHandle.toFloat().reduced (0.5f), 5.0f, 1.0f);
            g.setColour (text);
            g.setFont (juce::FontOptions (7.4f).withStyle ("bold"));
            g.drawText ("DRAG MIDI", dragHandle, juce::Justification::centred, true);

            if (orbitMultiRing)
            {
                const int laneCount = 5;
                const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 42);
                if (size <= 24.0f)
                    return;

                const juce::Point<float> centre ((float) area.getCentreX(),
                                                 (float) area.getY() + size * 0.52f);
                const float radius = size * 0.42f;
                const float innerRadius = radius * 0.25f;
                const float outerRadius = radius * 0.94f;
                const float band = (outerRadius - innerRadius) / (float) laneCount;

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
                        if (active >= 0.5f)
                        {
                            const float dot = 2.5f + velocity * (activeRing ? 2.4f : 1.2f);
                            g.setColour (ringColour.withAlpha ((activeRing ? 0.86f : 0.48f) * (0.5f + velocity * 0.5f)));
                            g.fillEllipse (p.x - dot, p.y - dot, dot * 2.0f, dot * 2.0f);
                        }
                    }
                }

                g.setColour (text);
                g.setFont (juce::FontOptions (22.0f).withStyle ("bold"));
                g.drawText ("ORBIT", juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y - 23, 90, 25),
                            juce::Justification::centred, true);
                g.setColour (dim);
                g.setFont (juce::FontOptions (8.5f).withStyle ("bold"));
                g.drawText ("ALL LANES", juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y + 3, 90, 18),
                            juce::Justification::centred, true);
                return;
            }

            const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 42);
            const juce::Point<float> centre ((float) area.getCentreX(),
                                             (float) area.getY() + size * 0.52f);
            const float radius = size * 0.40f;
            const float innerRadius = radius * 0.70f;
            const float noteRadius = radius * 1.10f;
            const int maxDrawSteps = juce::jmin (steps, 64);
            const int slotCount = juce::jlimit (1, 12, element.arpLaneSampleSlots);
            const bool multiRing = slotCount > 1 && element.arpLaneTarget != "notes";

            g.setColour (border.withAlpha (0.75f));
            g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
            g.setColour (border.withAlpha (0.24f));
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

            static const char* noteLabels[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            g.setFont (juce::FontOptions (8.5f));
            for (int note = 0; note < 12; ++note)
            {
                const float angle = -juce::MathConstants<float>::halfPi
                    + juce::MathConstants<float>::twoPi * (float) note / 12.0f;
                const auto p = centre + juce::Point<float> (std::cos (angle) * noteRadius,
                                                            std::sin (angle) * noteRadius);
                g.setColour (dim);
                g.drawText (noteLabels[note], juce::Rectangle<int> ((int) p.x - 12, (int) p.y - 6, 24, 12),
                            juce::Justification::centred, true);
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
                g.setColour (border.withAlpha (0.22f));
                g.drawLine (gridStart.x, gridStart.y, outer.x, outer.y, 0.7f);
                if (active >= 0.5f)
                {
                    const float dotSize = 3.0f + velocity * 5.0f;
                    g.setColour (accent.withAlpha (0.58f));
                    g.drawLine (centre.x, centre.y, velocityEnd.x, velocityEnd.y, 1.35f);
                    g.setColour (accent.withAlpha (0.30f));
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
                        g.setFont (juce::FontOptions (7.0f).withStyle ("bold"));
                        g.drawText (juce::String (divisions),
                                    juce::Rectangle<int> ((int) badge.x - 6, (int) badge.y - 6, 12, 12),
                                    juce::Justification::centred, true);
                    }
                }
            }

            const auto targetLabel = element.arpLaneTarget == "drums" ? "DRUMS"
                                  : element.arpLaneTarget == "oneShots" ? "ONE SHOTS"
                                  : element.arpLaneTarget == "loops" ? "LOOP SLICES"
                                  : element.arpLaneTarget == "samples" ? "SAMPLES" : "NOTES";

            g.setColour (text);
            g.setFont (juce::FontOptions (26.0f));
            g.drawText (juce::String (steps), juce::Rectangle<int> ((int) centre.x - 46, (int) centre.y - 24, 92, 32),
                        juce::Justification::centred, true);
            g.setColour (dim);
            g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
            g.drawText (targetLabel, juce::Rectangle<int> ((int) centre.x - 46, (int) centre.y + 6, 92, 18),
                        juce::Justification::centred, true);

            auto footer = r.reduced (12).removeFromBottom (34);
            const juce::String footerLabels[] =
            {
                element.arpLaneDirection.toUpperCase().substring (0, 4),
                "PUL " + juce::String (element.arpLaneEuclideanPulses),
                "RAT " + juce::String (element.arpLaneRatchet),
                "FIL " + juce::String (element.arpLaneFillPulses)
            };
            for (int i = 0; i < 4; ++i)
            {
                auto cell = footer.removeFromLeft (juce::jmax (1, footer.getWidth() / (4 - i))).reduced (3, 1);
                g.setColour (dim);
                g.setFont (juce::FontOptions (8.0f).withStyle ("bold"));
                g.drawText (footerLabels[i], cell.removeFromTop (12), juce::Justification::centred, true);
                g.setColour (accent.withAlpha (0.85f));
                g.fillRoundedRectangle (cell.toFloat().withHeight (4.0f).withY ((float) cell.getCentreY() - 2.0f), 2.0f);
            }
        }

        static void drawDrumGridPreview (juce::Graphics& g,
                                         juce::Rectangle<int> r,
                                         const LayoutElement& element,
                                         const DspGraph& graph)
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

            const auto bg = element.backgroundColour.isTransparent() ? juce::Colour (0xff15191f) : element.backgroundColour;
            const auto border = element.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : element.borderColour;
            const auto accent = element.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : element.accentColour;

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, element.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, element.cornerRadius), 1.0f);

            auto area = r.reduced (8);
            auto header = area.removeFromTop (20);
            g.setColour (accent);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText ((element.label.isNotEmpty() ? element.label : "DRUM GRID")
                            + "  P" + juce::String (pattern + 1)
                            + "  " + juce::String (tracks) + "x" + juce::String (steps),
                        header, juce::Justification::centredLeft, true);

            area.removeFromTop (4);
            if (area.isEmpty())
                return;

            const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
            auto grid = area.withTrimmedLeft (labelW);
            const float cellW = (float) grid.getWidth() / (float) steps;
            const float cellH = (float) area.getHeight() / (float) tracks;
            static const char* names[] = { "Kick", "Snare", "Hat", "Clap", "Tom", "Perc", "Ride", "Crash" };

            for (int track = 0; track < tracks; ++track)
            {
                const int y = area.getY() + juce::roundToInt ((float) track * cellH);
                const int h = juce::roundToInt (cellH);
                auto label = juce::Rectangle<int> (area.getX(), y, labelW - 5, h);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (juce::jlimit (8.0f, 10.5f, cellH * 0.40f), juce::Font::bold));
                const juce::String trackLabel = track < 8 ? juce::String (names[track]) : "T" + juce::String (track + 1);
                g.drawText (trackLabel, label, juce::Justification::centredLeft, true);

                for (int step = 0; step < steps; ++step)
                {
                    const int x = grid.getX() + juce::roundToInt ((float) step * cellW);
                    const auto cell = juce::Rectangle<float> ((float) x + 1.0f,
                                                              (float) y + 1.0f,
                                                              juce::jmax (1.0f, cellW - 2.0f),
                                                              juce::jmax (1.0f, cellH - 2.0f));
                    const auto prefix = "dmP" + juce::String (pattern)
                                      + "T" + juce::String (track)
                                      + "S" + juce::String (step);
                    const bool fallbackHit = (track == 0 && step % 4 == 0)
                                          || (track == 1 && (step == 4 || step == 12))
                                          || (track == 2 && step % 2 == 0)
                                          || (track == 3 && (step == 7 || step == 15));
                    const bool active = block != nullptr
                        ? blockValue (*block, prefix + "On", 0.0f) >= 0.5f
                        : fallbackHit;
                    const float velocity = block != nullptr
                        ? juce::jlimit (0.1f, 1.0f, blockValue (*block, prefix + "Vel", 0.8f))
                        : 0.75f;
                    const int divisions = block != nullptr
                        ? juce::jlimit (1, 4, juce::roundToInt (blockValue (*block, prefix + "Div", 1.0f)))
                        : 1;
                    g.setColour ((step % 4 == 0 ? PatchCraftLookAndFeel::panelAlt().brighter (0.08f)
                                                 : PatchCraftLookAndFeel::panelAlt())
                                     .withAlpha (0.86f));
                    g.fillRoundedRectangle (cell, 2.5f);
                    if (divisions > 1 && cellW >= 11.0f)
                    {
                        g.setColour (accent.withAlpha (0.20f));
                        for (int division = 1; division < divisions; ++division)
                        {
                            const float xLine = cell.getX() + cell.getWidth() * (float) division / (float) divisions;
                            g.drawVerticalLine (juce::roundToInt (xLine), cell.getY() + 2.0f, cell.getBottom() - 2.0f);
                        }
                    }
                    if (active)
                    {
                        auto hit = cell.reduced (2.0f);
                        hit.removeFromTop (hit.getHeight() * (1.0f - velocity));
                        g.setColour (accent.withAlpha (0.68f + velocity * 0.28f));
                        g.fillRoundedRectangle (hit, 2.0f);
                        if (divisions > 1 && cellW >= 18.0f && cellH >= 12.0f)
                        {
                            g.setColour (juce::Colours::black.withAlpha (0.70f));
                            g.setFont (juce::Font (juce::jlimit (7.0f, 9.0f, cellH * 0.34f), juce::Font::bold));
                            g.drawText ("x" + juce::String (divisions), cell.toNearestInt(), juce::Justification::centred, true);
                        }
                    }
                }
            }
        }

        // Searchable parameter picker. Replaces the right-click "Add DSP
        // Control" submenu cascade with a single popup containing a search
        // field + filtered list - the submenu was unusable once the
        // parameter palette grew past ~20 entries.
        class ParameterSearchPopup : public juce::Component,
                                     private juce::TextEditor::Listener,
                                     private juce::ListBoxModel
        {
        public:
            struct Entry
            {
                juce::String id;
                juce::String label;       // display string ("Cutoff  (filterCutoff) - Filter")
                juce::String haystack;    // lowercased search text
            };

            using SelectCallback = std::function<void (const juce::String& parameterId)>;

            ParameterSearchPopup (std::vector<Entry> entries, SelectCallback onSelect)
                : allEntries (std::move (entries)),
                  callback (std::move (onSelect))
            {
                visibleEntries = allEntries;

                search.setTextToShowWhenEmpty ("Search parameters...",
                                               juce::Colours::grey);
                search.setMultiLine (false);
                search.setReturnKeyStartsNewLine (false);
                search.addListener (this);
                addAndMakeVisible (search);

                list.setModel (this);
                list.setRowHeight (22);
                addAndMakeVisible (list);

                setSize (380, 360);
                juce::Timer::callAfterDelay (50, [self = juce::Component::SafePointer<ParameterSearchPopup> (this)]
                {
                    if (auto* p = self.getComponent()) p->search.grabKeyboardFocus();
                });
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::panel());
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (6);
                search.setBounds (r.removeFromTop (28));
                r.removeFromTop (4);
                list.setBounds (r);
            }

            // ListBoxModel
            int getNumRows() override { return (int) visibleEntries.size(); }

            void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
            {
                if (row < 0 || row >= (int) visibleEntries.size()) return;
                if (selected)
                {
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.25f));
                    g.fillRect (0, 0, w, h);
                }
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (12.0f);
                g.drawText (visibleEntries[(size_t) row].label,
                            8, 0, w - 16, h, juce::Justification::centredLeft);
            }

            void listBoxItemClicked (int row, const juce::MouseEvent&) override
            {
                commitRow (row);
            }

            void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
            {
                commitRow (row);
            }

            void returnKeyPressed (int row) override
            {
                commitRow (row >= 0 ? row : 0);
            }

        private:
            void textEditorTextChanged (juce::TextEditor&) override
            {
                rebuildVisible();
            }

            void textEditorReturnKeyPressed (juce::TextEditor&) override
            {
                if (! visibleEntries.empty())
                    commitRow (0);
            }

            void textEditorEscapeKeyPressed (juce::TextEditor&) override
            {
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->exitModalState (0);
            }

            void rebuildVisible()
            {
                const auto query = search.getText().trim().toLowerCase();
                visibleEntries.clear();
                if (query.isEmpty())
                {
                    visibleEntries = allEntries;
                }
                else
                {
                    juce::StringArray tokens;
                    tokens.addTokens (query, " ", "");
                    tokens.removeEmptyStrings();
                    for (const auto& entry : allEntries)
                    {
                        bool matchesAll = true;
                        for (const auto& tok : tokens)
                        {
                            if (! entry.haystack.contains (tok))
                            {
                                matchesAll = false;
                                break;
                            }
                        }
                        if (matchesAll)
                            visibleEntries.push_back (entry);
                    }
                }
                list.updateContent();
                list.repaint();
                if (! visibleEntries.empty())
                    list.selectRow (0);
            }

            void commitRow (int row)
            {
                if (row < 0 || row >= (int) visibleEntries.size()) return;
                const auto id = visibleEntries[(size_t) row].id;
                auto cb = callback;
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->exitModalState (0);
                if (cb)
                    cb (id);
            }

            std::vector<Entry> allEntries;
            std::vector<Entry> visibleEntries;
            SelectCallback     callback;
            juce::TextEditor   search;
            juce::ListBox      list { "paramSearch", nullptr };
        };

        class CanvasActionSearchPopup : public juce::Component,
                                        private juce::TextEditor::Listener,
                                        private juce::ListBoxModel
        {
        public:
            struct Entry
            {
                juce::String id;
                juce::String title;
                juce::String detail;
                juce::String haystack;
            };

            using SelectCallback = std::function<void (const juce::String& actionId)>;

            CanvasActionSearchPopup (std::vector<Entry> entries, SelectCallback onSelect)
                : allEntries (std::move (entries)),
                  callback (std::move (onSelect))
            {
                visibleEntries = allEntries;
                search.setTextToShowWhenEmpty ("Search canvas actions, elements, controls...",
                                               PatchCraftLookAndFeel::textDim());
                search.setMultiLine (false);
                search.setReturnKeyStartsNewLine (false);
                search.addListener (this);
                addAndMakeVisible (search);

                list.setModel (this);
                list.setRowHeight (34);
                addAndMakeVisible (list);

                setSize (430, 420);
                juce::Timer::callAfterDelay (50, [self = juce::Component::SafePointer<CanvasActionSearchPopup> (this)]
                {
                    if (auto* p = self.getComponent()) p->search.grabKeyboardFocus();
                });
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::panel());
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (8);
                search.setBounds (r.removeFromTop (30));
                r.removeFromTop (6);
                list.setBounds (r);
            }

            int getNumRows() override { return (int) visibleEntries.size(); }

            void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
            {
                if (row < 0 || row >= (int) visibleEntries.size()) return;
                const auto& entry = visibleEntries[(size_t) row];
                if (selected)
                {
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.20f));
                    g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 2.0f, (float) w - 4.0f, (float) h - 4.0f), 5.0f);
                }
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::Font (12.0f, juce::Font::bold));
                g.drawText (entry.title, 10, 2, w - 20, 16, juce::Justification::centredLeft, true);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (10.5f);
                g.drawText (entry.detail, 10, 18, w - 20, 14, juce::Justification::centredLeft, true);
            }

            void listBoxItemClicked (int row, const juce::MouseEvent&) override { commitRow (row); }
            void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { commitRow (row); }
            void returnKeyPressed (int row) override { commitRow (row >= 0 ? row : 0); }

        private:
            void textEditorTextChanged (juce::TextEditor&) override
            {
                const auto query = search.getText().trim().toLowerCase();
                visibleEntries.clear();
                if (query.isEmpty())
                {
                    visibleEntries = allEntries;
                }
                else
                {
                    juce::StringArray tokens;
                    tokens.addTokens (query, " ", "");
                    tokens.removeEmptyStrings();
                    for (const auto& entry : allEntries)
                    {
                        bool matches = true;
                        for (const auto& token : tokens)
                            if (! entry.haystack.contains (token))
                            {
                                matches = false;
                                break;
                            }
                        if (matches)
                            visibleEntries.push_back (entry);
                    }
                }
                list.updateContent();
                list.repaint();
                if (! visibleEntries.empty())
                    list.selectRow (0);
            }

            void textEditorReturnKeyPressed (juce::TextEditor&) override
            {
                if (! visibleEntries.empty())
                    commitRow (0);
            }

            void textEditorEscapeKeyPressed (juce::TextEditor&) override
            {
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->exitModalState (0);
            }

            void commitRow (int row)
            {
                if (row < 0 || row >= (int) visibleEntries.size()) return;
                const auto id = visibleEntries[(size_t) row].id;
                auto cb = callback;
                if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                    parent->exitModalState (0);
                if (cb)
                    cb (id);
            }

            std::vector<Entry> allEntries;
            std::vector<Entry> visibleEntries;
            SelectCallback callback;
            juce::TextEditor search;
            juce::ListBox list { "canvasActionSearch", nullptr };
        };

        static std::vector<ParameterSearchPopup::Entry>
            collectParameterEntries (const PatchCraftProject& project)
        {
            std::vector<ParameterSearchPopup::Entry> entries;
            for (const auto& def : project.getParameters().getAll())
            {
                ParameterSearchPopup::Entry e;
                e.id = def.id;
                const auto cat = def.category.isNotEmpty() ? def.category : juce::String ("Other");
                e.label    = def.name + "  (" + def.id + ")  - " + cat;
                e.haystack = (def.name + " " + def.id + " " + cat + " " + def.section).toLowerCase();
                entries.push_back (std::move (e));
            }
            return entries;
        }

        static void launchParameterPicker (juce::Component* attachTo,
                                           juce::Rectangle<int> screenAnchor,
                                           std::vector<ParameterSearchPopup::Entry> entries,
                                           ParameterSearchPopup::SelectCallback cb)
        {
            auto popup = std::make_unique<ParameterSearchPopup> (std::move (entries), std::move (cb));
            juce::CallOutBox::launchAsynchronously (std::move (popup), screenAnchor,
                                                    attachTo != nullptr ? attachTo->getTopLevelComponent()
                                                                        : nullptr);
        }

        static std::vector<CanvasActionSearchPopup::Entry> collectCanvasActionEntries()
        {
            auto make = [] (juce::String id, juce::String title, juce::String detail, juce::String keywords)
            {
                CanvasActionSearchPopup::Entry entry;
                entry.id = std::move (id);
                entry.title = std::move (title);
                entry.detail = std::move (detail);
                entry.haystack = (entry.id + " " + entry.title + " " + entry.detail + " " + keywords).toLowerCase();
                return entry;
            };

            return {
                make ("findParameter", "Find Parameter / Add Knob", "Search all DSP parameters and add a connected knob.", "control dsp assign knob"),
                make ("panel", "Add Panel / Container", "Add a resizable visual container.", "container box section"),
                make ("tabPanel", "Add Tab Panel", "Add tabs for multi-page instrument UIs.", "tabs pages container"),
                make ("roundedRect", "Add Rounded Rectangle", "Add a shape with editable colour, opacity, and border.", "shape rect box"),
                make ("ellipse", "Add Ellipse", "Add a circular/oval shape.", "shape circle"),
                make ("circleSeqBg", "Build CircleSEQ Background Kit", "Add editable glow plates, radial rings, stage panels, and divider lines.", "background circles sequencer radial photoshop design"),
                make ("mixer", "Add Mixer", "Add a mixer surface mapped to output or bus parameters.", "fader volume pan bus"),
                make ("mixerChannel", "Add Single Mixer Channel", "Add one compact mixer strip instead of a full mixer group.", "fader channel strip volume pan"),
                make ("explodeMixer", "Break Mixer Into Channels", "Convert a selected mixer into separate one-channel strips.", "ungroup split mixer channels"),
                make ("macro", "Add Macro Control", "Add a performable macro control.", "macro performance"),
                make ("modMatrix", "Add Mod Matrix", "Add a modulation matrix UI element.", "modulation routing"),
                make ("granular", "Add Granular Field", "Add a runtime granular control surface.", "sample grain cloud"),
                make ("arpLane", "Add Arp Studio Lane", "Add a circular arp lane that selects and visualizes a MIDI Playground bank.", "arp sequencer circle lane bank steps"),
                make ("drumMachine", "Add Drum Machine Surface", "Add pads, pattern grid, bank controls, and mixer.", "drums sequencer pads"),
                make ("bpm", "Add Project BPM Control", "Add a knob connected to the global preview/standalone BPM.", "tempo global sync"),
                make ("bpmSync", "Add BPM Sync Toggle", "Add an on/off switch for tempo-synced blocks.", "tempo sync toggle"),
                make ("retrigger", "Add Retrigger Toggle", "Add an on/off switch for retrigger behaviour.", "performance toggle"),
                make ("copy", "Copy Selection", "Copy selected elements with parameters.", "clipboard duplicate"),
                make ("copyNoParams", "Copy Selection Without Parameters", "Copy UI design only; leave parameter assignments behind.", "clipboard style"),
                make ("paste", "Paste Elements", "Paste the current copied elements.", "clipboard"),
                make ("copyTabs", "Copy Selection To All Tabs", "Place selected controls across every tab.", "tabs reference duplicate"),
                make ("group", "Create Group From Selection", "Group selected elements in the layer tree.", "folder group")
            };
        }

        static void launchCanvasActionPicker (juce::Component* attachTo,
                                              juce::Rectangle<int> screenAnchor,
                                              CanvasActionSearchPopup::SelectCallback cb)
        {
            auto popup = std::make_unique<CanvasActionSearchPopup> (collectCanvasActionEntries(), std::move (cb));
            juce::CallOutBox::launchAsynchronously (std::move (popup), screenAnchor,
                                                    attachTo != nullptr ? attachTo->getTopLevelComponent()
                                                                        : nullptr);
        }

        static juce::String mixerSlotAt (const juce::StringArray& values, int index, juce::String fallback = {})
        {
            return index >= 0 && index < values.size() && values[index].isNotEmpty()
                ? values[index]
                : std::move (fallback);
        }
    }

    static constexpr int kRulerSize = 22;

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

    static juce::String manualContainerTargetForCanvasElement (const LayoutElement& element, bool& toggleMode)
    {
        const auto action = element.action.isNotEmpty() ? element.action.trim() : element.parameterId.trim();
        toggleMode = false;
        if (action.startsWithIgnoreCase ("showContainer:"))
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        if (action.startsWithIgnoreCase ("toggleContainer:"))
        {
            toggleMode = true;
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        }
        if (action.startsWithIgnoreCase ("showGroup:"))
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        if (action.startsWithIgnoreCase ("toggleGroup:"))
        {
            toggleMode = true;
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        }
        return {};
    }

    static bool canvasParameterIsEnabled (const PatchCraftProject& project, const ParameterDef& parameter)
    {
        if (parameter.enabledBy.isEmpty())
            return true;

        const auto* gate = project.getParameters().find (parameter.enabledBy);
        const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
        const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
        return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
    }

    static juce::String canvasControlGuidance (const PatchCraftProject& project, const LayoutElement& element)
    {
        if (element.parameterId.isEmpty())
            return "This control is not assigned to any parameter.\nSelect it, then use Inspector > DSP Assignment/Parameter or drag a DSP Quick Edit parameter onto the canvas.";

        const auto* parameter = project.getParameters().find (element.parameterId);
        if (parameter == nullptr)
            return "This control points to missing parameter '" + element.parameterId + "'.\nReconnect it in the Inspector or replace it by dragging a valid parameter onto the canvas.";

        if (! canvasParameterIsEnabled (project, *parameter))
            return "This control is disabled: "
                + (parameter->enableHint.isNotEmpty() ? parameter->enableHint : ("enable " + parameter->enabledBy + " first."))
                + "\nAfter enabling the source parameter, this knob will move and affect sound.";

        return {};
    }

    CanvasEditor::CanvasEditor (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);
        setWantsKeyboardFocus (true);
        startTimerHz (24);
    }

    void CanvasEditor::setCurrentTabGroup (juce::String groupId)
    {
        currentTabGroup = std::move (groupId);
        activeTabGroupsByPanel["tabs"] = currentTabGroup;
        repaint();
    }

    void CanvasEditor::setGridVisible (bool shouldShow)
    {
        if (showGrid == shouldShow) return;
        showGrid = shouldShow;
        repaint();
    }

    void CanvasEditor::setRulersVisible (bool shouldShow)
    {
        if (showRulers == shouldShow) return;
        showRulers = shouldShow;
        resized();
        repaint();
    }

    void CanvasEditor::mouseWheelMove (const juce::MouseEvent& e,
                                       const juce::MouseWheelDetails& wheel)
    {
        if (! e.mods.isShiftDown())
        {
            Component::mouseWheelMove (e, wheel);
            return;
        }

        const float delta = std::abs (wheel.deltaY) >= std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (std::abs (delta) < 0.0001f)
            return;

        const float factor = delta > 0.0f ? 1.10f : 0.90f;
        setZoom (zoom * factor);
        owner.refreshCanvasToolbar();
    }

    bool CanvasEditor::isElementOnCurrentTab (const LayoutElement& e) const
    {
        if (e.groupId.isEmpty()) return true;

        if (isManualContainerGroup (e.groupId))
        {
            const auto state = manualContainerOpen.find (e.groupId);
            return state != manualContainerOpen.end() && state->second;
        }

        for (const auto& parent : owner.getProject().getLayout().getAll())
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

    bool CanvasEditor::isManualContainerGroup (const juce::String& groupId) const
    {
        return manualContainerOrder.contains (groupId);
    }

    void CanvasEditor::ensureManualContainerDefaults()
    {
        manualContainerOrder.clear();
        for (const auto& element : owner.getProject().getLayout().getAll())
        {
            bool toggleMode = false;
            const auto target = manualContainerTargetForCanvasElement (element, toggleMode);
            juce::ignoreUnused (toggleMode);
            if (target.isNotEmpty())
                manualContainerOrder.addIfNotAlreadyThere (target);
        }

        for (int i = 0; i < manualContainerOrder.size(); ++i)
            if (manualContainerOpen.find (manualContainerOrder[i]) == manualContainerOpen.end())
                manualContainerOpen[manualContainerOrder[i]] = (i == 0);
    }

    bool CanvasEditor::triggerManualContainer (const LayoutElement& element)
    {
        bool toggleMode = false;
        const auto target = manualContainerTargetForCanvasElement (element, toggleMode);
        if (target.isEmpty())
            return false;

        ensureManualContainerDefaults();
        if (! manualContainerOrder.contains (target))
            manualContainerOrder.add (target);

        if (toggleMode)
            manualContainerOpen[target] = ! manualContainerOpen[target];
        else
        {
            for (const auto& group : manualContainerOrder)
                manualContainerOpen[group] = false;
            manualContainerOpen[target] = true;
        }

        repaint();
        return true;
    }

    juce::Rectangle<int> CanvasEditor::canvasScreenRect() const
    {
        const auto& cs = owner.getProject().getCanvasSize();
        const int w = juce::roundToInt (cs.width  * zoom);
        const int h = juce::roundToInt (cs.height * zoom);
        const int x = (getWidth()  - w) / 2 + (showRulers ? kRulerSize / 2 : 0);
        const int y = (getHeight() - h) / 2 + (showRulers ? kRulerSize / 2 : 0);
        return { x, y, w, h };
    }

    juce::Rectangle<int> CanvasEditor::elementScreenRect (const LayoutElement& e) const
    {
        auto c = canvasScreenRect();
        return juce::Rectangle<int> (
            c.getX() + juce::roundToInt (e.x * zoom),
            c.getY() + juce::roundToInt (e.y * zoom),
            juce::roundToInt (e.width  * zoom),
            juce::roundToInt (e.height * zoom));
    }

    juce::Point<int> CanvasEditor::screenToCanvas (juce::Point<int> p) const
    {
        auto c = canvasScreenRect();
        return { juce::roundToInt ((p.x - c.getX()) / zoom),
                 juce::roundToInt ((p.y - c.getY()) / zoom) };
    }

    void CanvasEditor::setZoom (float z)
    {
        autoFitCanvas = false;
        zoom = juce::jlimit (0.10f, 4.0f, z);
        resized();
        repaint();
    }

    void CanvasEditor::refreshZoomForBounds()
    {
        if (autoFitCanvas)
        {
            fit();
            return;
        }

        resized();
        repaint();
    }

    void CanvasEditor::fit()
    {
        autoFitCanvas = true;
        const auto& cs = owner.getProject().getCanvasSize();
        if (cs.width <= 0 || cs.height <= 0) return;
        const int avail = juce::jmax (200, getWidth() - kRulerSize * 2 - 60);
        const int availV = juce::jmax (200, getHeight() - kRulerSize * 2 - 60);
        zoom = juce::jmin ((float) avail / (float) cs.width,
                           (float) availV / (float) cs.height);
        zoom = juce::jlimit (0.10f, 4.0f, zoom);
        resized();
        repaint();
    }

    // -------------------------------------------------------------------------
    void CanvasEditor::paint (juce::Graphics& g)
    {
        ensureManualContainerDefaults();
        g.fillAll (PatchCraftLookAndFeel::bg());

        if (showRulers) drawRulers (g);

        auto canvas = canvasScreenRect();
        // outer drop shadow
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.6f), 24, {});
        ds.drawForRectangle (g, canvas);

        drawCanvasBackground (g, canvas);

        // Draw elements in z-order (back-to-front).
        const auto& elements = owner.getProject().getLayout().getAll();
        for (auto& e : elements)
        {
            if (! e.visible) continue;
            if (e.type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (e)) continue;
            drawElement (g, e, elementScreenRect (e), owner.isElementSelected (e.id));
        }

        if (mode == DragMode::Marquee && ! marqueeRect.isEmpty())
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.12f));
            g.fillRect (marqueeRect);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (marqueeRect, 1);
        }

        drawSelectionGuides (g);

        if (hoverGuidance.isNotEmpty() && ! hoverGuidanceBounds.isEmpty())
        {
            auto bubble = hoverGuidanceBounds.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.88f));
            g.fillRoundedRectangle (bubble, 7.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.75f));
            g.drawRoundedRectangle (bubble, 7.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.setFont (juce::Font (11.0f));
            g.drawFittedText (hoverGuidance, hoverGuidanceBounds.reduced (10, 7),
                              juce::Justification::centredLeft, 4);
        }
    }

    void CanvasEditor::resized()
    {
    }

    void CanvasEditor::timerCallback()
    {
        // Only repaint when there's actually something animated on the
        // current tab. Skipping the full-canvas repaint when nothing is
        // moving avoids burning a frame's worth of work behind every
        // knob/slider drag event.
        if (mode != DragMode::None)
            return;

        for (const auto& item : owner.getProject().getLayout().getAll())
        {
            if (! item.visible || ! isElementOnCurrentTab (item))
                continue;
            if ((item.animationMode.isNotEmpty() && item.animationMode != "none")
                || item.audioReactive)
            {
                repaint();
                return;
            }
        }
    }

    bool CanvasEditor::isInterestedInDragSource (const SourceDetails& details)
    {
        if (details.description.toString().startsWith ("param:"))
            return true;

        if (auto* object = details.description.getDynamicObject())
            return object->getProperty ("patchcraftDragType").toString() == "libraryAsset";

        return false;
    }

    void CanvasEditor::itemDropped (const SourceDetails& details)
    {
        if (auto* object = details.description.getDynamicObject())
        {
            if (object->getProperty ("patchcraftDragType").toString() == "libraryAsset")
            {
                const auto category = object->getProperty ("category").toString();
                const juce::File file (object->getProperty ("path").toString());
                const int frames = juce::jmax (1, (int) object->getProperty ("frames"));
                const bool vertical = (bool) object->getProperty ("vertical");
                owner.addLibraryAssetToCanvas (category, file, frames, vertical, screenToCanvas (details.localPosition));
                return;
            }
        }

        const auto descriptor = details.description.toString();
        if (! descriptor.startsWith ("param:"))
            return;

        const auto parameterId = descriptor.fromFirstOccurrenceOf ("param:", false, false).trim();
        if (parameterId.isEmpty())
            return;

        addElementAt (ElementType::Knob, screenToCanvas (details.localPosition), parameterId);
    }

    void CanvasEditor::addMoveOriginWithChildren (const juce::String& id)
    {
        if (id.isEmpty() || multiDragOrigins.find (id) != multiDragOrigins.end())
            return;

        if (auto* element = owner.getProject().getLayout().find (id))
            multiDragOrigins[id] = { element->x, element->y };
        else
            return;

        for (const auto& child : owner.getProject().getLayout().getAll())
            if (child.containerId == id)
                addMoveOriginWithChildren (child.id);
    }

    void CanvasEditor::captureMoveOriginsForSelection()
    {
        multiDragOrigins.clear();
        for (const auto& id : owner.getSelectedElementIds())
            addMoveOriginWithChildren (id);
    }

    // ---- Rulers --------------------------------------------------------------
    void CanvasEditor::drawRulers (juce::Graphics& g) const
    {
        auto canvas = canvasScreenRect();

        // Top ruler bar
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRect (0, 0, getWidth(), kRulerSize);
        g.fillRect (0, 0, kRulerSize, getHeight());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, kRulerSize, getWidth(), 1);
        g.fillRect (kRulerSize, 0, 1, getHeight());

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.0f));

        const auto& cs = owner.getProject().getCanvasSize();
        const int step = 100;
        for (int x = 0; x <= cs.width; x += step)
        {
            const int sx = canvas.getX() + juce::roundToInt (x * zoom);
            g.drawLine ((float) sx, (float) kRulerSize - 6,
                        (float) sx, (float) kRulerSize, 1.0f);
            g.drawText (juce::String (x), sx - 18, 2, 36, kRulerSize - 6,
                        juce::Justification::centred);
        }
        for (int y = 0; y <= cs.height; y += step)
        {
            const int sy = canvas.getY() + juce::roundToInt (y * zoom);
            g.drawLine ((float) kRulerSize - 6, (float) sy,
                        (float) kRulerSize, (float) sy, 1.0f);
            g.drawText (juce::String (y), 0, sy - 9, kRulerSize - 8, 18,
                        juce::Justification::centredRight);
        }
    }

    void CanvasEditor::drawCanvasBackground (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        // Outer canvas surface
        g.setColour (juce::Colour (0xff0c0e12));
        g.fillRect (r);

        // Grid
        if (showGrid)
        {
            const auto minorStep = (float) snapGrid * zoom;
            if (minorStep >= 3.0f)
            {
                g.setColour (gridColour.withAlpha (0.80f));
                for (float x = (float) r.getX(); x < (float) r.getRight(); x += minorStep)
                    g.drawVerticalLine ((int) x, (float) r.getY(), (float) r.getBottom());
                for (float y = (float) r.getY(); y < (float) r.getBottom(); y += minorStep)
                    g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
            }

            g.setColour (snapColour.withAlpha (0.95f));
            const auto step = (float) snapGrid * zoom * 5.0f;
            if (step >= 3.0f)
            {
                for (float x = (float) r.getX(); x < (float) r.getRight(); x += step)
                    g.drawVerticalLine ((int) x, (float) r.getY(), (float) r.getBottom());
                for (float y = (float) r.getY(); y < (float) r.getBottom(); y += step)
                    g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
            }
        }

        // Background image / hero placeholder is drawn by the 'background' layer
        // element. Frame the canvas with a subtle border.
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRect (r, 1);
    }

    // ---- Element rendering --------------------------------------------------
    static void drawHeroArtwork (juce::Graphics& g, juce::Rectangle<int> r)
    {
        // Cinematic mountain hero with EVOLVE wordmark
        juce::ColourGradient grad (juce::Colour (0xff0a0d12), 0.0f, (float) r.getY(),
                                   juce::Colour (0xff1a1a18), 0.0f, (float) r.getBottom(), false);
        grad.addColour (0.45, juce::Colour (0xff5a3a1a));
        grad.addColour (0.60, juce::Colour (0xffc88a3a));
        grad.addColour (0.75, juce::Colour (0xff1a1612));
        g.setGradientFill (grad);
        g.fillRect (r);

        // Sun glow
        const float cx = r.getCentreX();
        const float cy = r.getY() + r.getHeight() * 0.62f;
        juce::ColourGradient glow (juce::Colour (0xfff5d089), cx, cy,
                                   juce::Colours::transparentBlack,
                                   cx, cy - r.getHeight() * 0.3f, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - r.getHeight() * 0.5f, cy - r.getHeight() * 0.5f,
                       r.getHeight() * 1.0f, r.getHeight() * 1.0f);

        // Mountains
        juce::Path m;
        const float baseY = r.getBottom() - r.getHeight() * 0.18f;
        m.startNewSubPath ((float) r.getX(), (float) r.getBottom());
        m.lineTo ((float) r.getX(), baseY);
        const int peaks = juce::jmax (5, r.getWidth() / 100);
        juce::Random rnd (4242);
        for (int i = 0; i <= peaks; ++i)
        {
            float x = juce::jmap ((float) i, 0.0f, (float) peaks,
                                  (float) r.getX(), (float) r.getRight());
            float y = baseY - rnd.nextFloat() * r.getHeight() * 0.18f;
            m.lineTo (x, y);
        }
        m.lineTo ((float) r.getRight(), baseY);
        m.lineTo ((float) r.getRight(), (float) r.getBottom());
        m.closeSubPath();
        g.setColour (juce::Colour (0xff1a1714).withAlpha (0.85f));
        g.fillPath (m);

        // Foreground silhouette
        juce::Path fg;
        const float fgY = r.getBottom() - r.getHeight() * 0.10f;
        fg.startNewSubPath ((float) r.getX(), (float) r.getBottom());
        fg.lineTo ((float) r.getX(), fgY);
        for (int i = 0; i <= peaks * 2; ++i)
        {
            float x = juce::jmap ((float) i, 0.0f, (float) (peaks * 2),
                                  (float) r.getX(), (float) r.getRight());
            float y = fgY + rnd.nextFloat() * r.getHeight() * 0.06f;
            fg.lineTo (x, y);
        }
        fg.lineTo ((float) r.getRight(), (float) r.getBottom());
        fg.closeSubPath();
        g.setColour (juce::Colour (0xff05060a).withAlpha (0.95f));
        g.fillPath (fg);

        // EVOLVE wordmark
        auto title = r.withSizeKeepingCentre (r.getWidth(),
                                              juce::jmax (40, (int) (r.getHeight() * 0.3f)));
        title = title.withY (r.getY() + (int) (r.getHeight() * 0.18f));
        g.setColour (juce::Colour (0xfff7d28d));
        g.setFont (juce::Font (juce::jmax (32.0f, (float) r.getHeight() * 0.18f),
                               juce::Font::bold));
        g.drawText ("EVOLVE", title, juce::Justification::centredTop);

        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (juce::jmax (10.0f, r.getHeight() * 0.038f), juce::Font::plain));
        auto sub = title.translated (0, juce::roundToInt (r.getHeight() * 0.21f));
        g.drawText ("CINEMATIC PAD", sub, juce::Justification::centredTop);
    }

    static void drawControlLabel (juce::Graphics& g, juce::Rectangle<int> r,
                                  const LayoutElement& e, juce::String valueText)
    {
        if (e.labelPosition == "hidden")
            return;

        const auto label = e.label.isNotEmpty() ? e.label : e.parameterId;
        if (label.isEmpty() && valueText.isEmpty())
            return;

        juce::Rectangle<int> labelArea;
        if (e.labelPosition == "top")
            labelArea = r.withY (r.getY() + juce::roundToInt (e.labelOffsetY)).withHeight (juce::jmax (20, r.getHeight() / 5));
        else if (e.labelPosition == "left")
            labelArea = r.withX (r.getX() - juce::roundToInt (48 + e.labelSpacing - e.labelOffsetX)).withWidth (juce::jmax (42, r.getWidth() / 2));
        else if (e.labelPosition == "right")
            labelArea = r.withX (r.getRight() + juce::roundToInt (e.labelSpacing + e.labelOffsetX)).withWidth (juce::jmax (42, r.getWidth() / 2));
        else
            labelArea = r.withTrimmedTop ((int) (r.getHeight() * 0.65f) + juce::roundToInt (e.labelSpacing + e.labelOffsetY))
                         .withHeight ((int) (r.getHeight() * 0.20f));

        labelArea.translate (juce::roundToInt (e.labelOffsetX), 0);
        const auto fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, r.getHeight() * 0.13f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (fontSize, juce::Font::bold));
        g.drawText (label.toUpperCase(), labelArea, juce::Justification::centred, true);

        if (valueText.isNotEmpty())
        {
            auto valueArea = labelArea.translated (0, juce::roundToInt (fontSize + 2.0f));
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (juce::jmax (8.5f, fontSize * 0.85f)));
            g.drawText (valueText, valueArea, juce::Justification::centred, true);
        }
    }

    static void drawCanvasKnob (juce::Graphics& g, juce::Rectangle<int> r,
                                const LayoutElement& e, juce::String valueText,
                                juce::Colour accent, float pos01)
    {
        const float cx = r.getCentreX();
        const float cy = r.getCentreY() - r.getHeight() * 0.12f;
        const float rad = juce::jmin (r.getWidth(), (int) (r.getHeight() * 0.7f)) * 0.5f;

        // outer ring
        const float ringW = juce::jmax (3.0f, rad * 0.13f);
        const float startA = juce::degreesToRadians (-135.0f);
        const float endA   = juce::degreesToRadians ( 135.0f);
        const float pos    = juce::jlimit (0.0f, 1.0f, pos01);

        juce::Path track;
        track.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                             0.0f, startA, endA, true);
        g.setColour (juce::Colour (0xff202227));
        g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        juce::Path active;
        active.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                              0.0f, startA, startA + (endA - startA) * pos, true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // body
        const float br = rad - ringW - 3.0f;
        juce::ColourGradient grad (juce::Colour (0xff262830), cx, cy - br,
                                   juce::Colour (0xff0c0d11), cx, cy + br, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
        g.setColour (juce::Colour (0xff050607));
        g.drawEllipse (cx - br, cy - br, br * 2, br * 2, 1.0f);

        // indicator line
        const float ang = startA + (endA - startA) * pos;
        juce::Path ind;
        ind.addRoundedRectangle (-1.5f, -br * 0.95f, 3.0f, br * 0.55f, 1.5f);
        ind.applyTransform (juce::AffineTransform::rotation (ang).translated (cx, cy));
        g.setColour (accent);
        g.fillPath (ind);

        drawControlLabel (g, r, e, valueText);
    }

    static void drawMacroControlElement (juce::Graphics& g, juce::Rectangle<int> r,
                                         const LayoutElement& e,
                                         const PatchCraftProject& project)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (9, 7);
        auto title = area.removeFromTop (18);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MACRO",
                    title, juce::Justification::centredLeft, true);

        const auto* def = project.getParameters().find (e.parameterId);
        const float raw = def != nullptr ? project.getLiveValues().getValue (def->id, def->defaultValue) : 0.5f;
        const float norm = def != nullptr
            ? juce::jlimit (0.0f, 1.0f, (raw - def->min) / juce::jmax (0.0001f, def->max - def->min))
            : 0.5f;

        auto knob = area.removeFromLeft (juce::jmin (area.getHeight(), 74)).reduced (4);
        LayoutElement knobElement = e;
        knobElement.labelPosition = "hidden";
        drawCanvasKnob (g, knob, knobElement, {}, accent, norm);

        auto lanes = area.reduced (5, 4);
        const juce::StringArray targetLabels { "TONE", "MOTION", "SPACE", "DRIVE" };
        for (int i = 0; i < targetLabels.size(); ++i)
        {
            auto row = lanes.removeFromTop (juce::jmax (14, lanes.getHeight() / (targetLabels.size() - i))).reduced (0, 2);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.70f));
            g.fillRoundedRectangle (row.toFloat(), 3.0f);
            g.setColour (accent.withAlpha (0.35f + 0.10f * (float) i));
            g.fillRoundedRectangle (row.withWidth (juce::roundToInt ((float) row.getWidth() * norm)).toFloat(), 3.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText (targetLabels[i], row.reduced (5, 0), juce::Justification::centredLeft, true);
        }
    }

    static void drawModMatrixElement (juce::Graphics& g, juce::Rectangle<int> r,
                                      const LayoutElement& e,
                                      const PatchCraftProject& project)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MOD MATRIX",
                    header.removeFromLeft (140), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.0f));
        g.drawText (juce::String ((int) project.getDspGraph().modulation.size()) + " routes",
                    header, juce::Justification::centredRight, true);

        auto list = area.reduced (4, 6);
        const auto& routes = project.getDspGraph().modulation;
        if (routes.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawFittedText ("No routes yet. Select this element, then add source -> target rows in the Inspector.",
                              list, juce::Justification::centred, 3);
            return;
        }

        const int maxRows = juce::jmin (6, (int) routes.size());
        for (int i = 0; i < maxRows; ++i)
        {
            const auto& route = routes[(size_t) i];
            auto row = list.removeFromTop (juce::jmax (18, list.getHeight() / (maxRows - i))).reduced (0, 2);
            g.setColour (route.enabled ? accent.withAlpha (0.16f) : PatchCraftLookAndFeel::bg().withAlpha (0.78f));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (route.enabled ? accent.withAlpha (0.78f) : PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (8.8f, juce::Font::bold));
            g.drawText (route.sourceId + " -> " + route.targetId, row.removeFromLeft (juce::jmax (80, row.getWidth() - 52)).reduced (6, 0),
                        juce::Justification::centredLeft, true);
            g.drawText (juce::String (route.amount, 2), row.reduced (4, 0), juce::Justification::centredRight, true);
        }
    }

    static void drawCanvasVerticalSlider (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e)
    {
        // label on top
        if (e.labelPosition != "hidden")
        {
            auto labelRect = r.removeFromTop (16 + juce::roundToInt (e.labelSpacing));
            labelRect.translate (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (e.labelSize > 0.0f ? e.labelSize : 10.0f, juce::Font::bold));
            g.drawText ((e.label.isNotEmpty() ? e.label : e.parameterId).toUpperCase(), labelRect, juce::Justification::centred);
        }

        const float trackW = juce::jmax (4.0f, r.getWidth() * 0.18f);
        auto track = r.toFloat().withSizeKeepingCentre (trackW, (float) r.getHeight());
        g.setColour (juce::Colour (0xff202227));
        g.fillRoundedRectangle (track, trackW * 0.5f);

        const float thumbY = track.getY() + track.getHeight() * 0.45f;
        auto fill = juce::Rectangle<float> (track.getX(), thumbY,
                                            track.getWidth(),
                                            track.getBottom() - thumbY);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.55f));
        g.fillRoundedRectangle (fill, trackW * 0.5f);

        const float tw = juce::jmin ((float) r.getWidth() - 2, 22.0f);
        const float th = 14.0f;
        auto thumb = juce::Rectangle<float> (r.getCentreX() - tw * 0.5f,
                                             thumbY - th * 0.5f, tw, th);
        juce::ColourGradient grad (juce::Colour (0xff2a2d33), thumb.getX(), thumb.getY(),
                                   juce::Colour (0xff101216), thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (juce::Colour (0xff050608));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
    }

    static void drawCanvasMeter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const bool vertical = r.getHeight() > r.getWidth() * 1.2f;
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        const int segs = vertical ? 16 : 24;
        const auto inner = r.reduced (4);

        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            juce::Colour c = (t < 0.6f) ? juce::Colour (0xff5fb37b)
                            : (t < 0.85f ? juce::Colour (0xffe8b840)
                                         : juce::Colour (0xffe6504a));
            const float alpha = (t < 0.7f) ? 1.0f : 0.85f;

            if (vertical)
            {
                const int sh = juce::jmax (2, inner.getHeight() / segs - 1);
                const int sy = inner.getBottom() - (i + 1) * (sh + 1);
                g.setColour (c.withAlpha (alpha));
                g.fillRect (inner.getX(), sy, inner.getWidth(), sh);
            }
            else
            {
                const int sw = juce::jmax (2, inner.getWidth() / segs - 1);
                const int sx = inner.getX() + i * (sw + 1);
                g.setColour (c.withAlpha (alpha));
                g.fillRect (sx, inner.getY(), sw, inner.getHeight());
            }
        }
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

    static void drawEqCurveElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e,
                                    const DspGraph& graph)
    {
        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "EQ CURVE", header.removeFromLeft (130),
                    juce::Justification::centredLeft, true);
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

    static void drawSpectrumElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e)
    {
        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? juce::Colour (0xff20d6ff) : e.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "SPECTRUM", header.removeFromLeft (130),
                    juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (9.0f);
        g.drawText ("runtime analyzer", header, juce::Justification::centredRight, true);
        auto graphArea = area.reduced (2, 4);
        g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);

        constexpr int bars = 36;
        for (int i = 0; i < bars; ++i)
        {
            const float x01 = (float) i / (float) (bars - 1);
            const float h01 = 0.18f + 0.72f * std::abs (std::sin (x01 * 9.0f + (float) i * 0.37f));
            const int barW = juce::jmax (2, graphArea.getWidth() / bars - 2);
            const int x = graphArea.getX() + i * graphArea.getWidth() / bars;
            const int h = juce::roundToInt (h01 * (float) graphArea.getHeight());
            g.setColour (accent.interpolatedWith (PatchCraftLookAndFeel::accent(), x01).withAlpha (0.78f));
            g.fillRoundedRectangle ((float) x, (float) graphArea.getBottom() - (float) h,
                                    (float) barW, (float) h, 2.0f);
        }
    }

    static void drawCanvasKeyboard (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        const int totalKeys = 52; // 8 octaves of white keys
        const float kw = (float) (r.getWidth() - 8) / (float) totalKeys;
        const float keyTop = (float) r.getY() + 4.0f;
        const float keyH   = (float) r.getHeight() - 8.0f;

        // White keys
        for (int i = 0; i < totalKeys; ++i)
        {
            juce::Rectangle<float> key (r.getX() + 4 + i * kw, keyTop, kw - 1.0f, keyH);
            g.setColour (juce::Colour (0xffe9d8b8));
            g.fillRoundedRectangle (key, 1.5f);
            g.setColour (juce::Colour (0xff8a7958));
            g.drawRoundedRectangle (key, 1.5f, 0.5f);
        }
        // Black keys (rough pattern based on octave)
        const float bkH = keyH * 0.62f;
        const float bkW = kw * 0.62f;
        for (int oct = 0; oct < 8; ++oct)
        {
            const int base = oct * 7;
            const int blackOffsets[5] = { 0, 1, 3, 4, 5 };
            for (int b : blackOffsets)
            {
                const float x = r.getX() + 4 + (base + b + 1) * kw - bkW * 0.5f;
                if (x + bkW > r.getRight() - 4) continue;
                juce::Rectangle<float> key (x, keyTop, bkW, bkH);
                g.setColour (juce::Colour (0xff141413));
                g.fillRoundedRectangle (key, 1.5f);
                g.setColour (juce::Colour (0xff050505));
                g.drawRoundedRectangle (key, 1.5f, 0.5f);
            }
        }
    }

    void CanvasEditor::drawSelectionGuides (juce::Graphics& g) const
    {
        const auto& selectedIds = owner.getSelectedElementIds();
        if (selectedIds.size() < 2) return;

        struct Item { juce::Rectangle<int> screen; juce::Rectangle<int> canvas; };
        std::vector<Item> items;
        items.reserve ((size_t) selectedIds.size());
        for (const auto& id : selectedIds)
        {
            if (auto* el = owner.getProject().getLayout().find (id))
                items.push_back ({ elementScreenRect (*el),
                                   { el->x, el->y, el->width, el->height } });
        }
        if (items.size() < 2) return;

        // Selection bounding rect in screen space, used to extend the guide
        // lines past the selection.
        auto bounds = items.front().screen;
        for (auto& it : items) bounds = bounds.getUnion (it.screen);

        const auto guide = PatchCraftLookAndFeel::accent().withAlpha (0.45f);
        const auto guideStrong = PatchCraftLookAndFeel::accent().withAlpha (0.8f);
        const float dashes[] = { 4.0f, 4.0f };

        auto dashedH = [&] (int y)
        {
            g.setColour (guide);
            g.drawDashedLine (juce::Line<float> ((float) bounds.getX() - 24.0f, (float) y,
                                                 (float) bounds.getRight() + 24.0f, (float) y),
                              dashes, 2, 1.0f);
        };
        auto dashedV = [&] (int x)
        {
            g.setColour (guide);
            g.drawDashedLine (juce::Line<float> ((float) x, (float) bounds.getY() - 24.0f,
                                                 (float) x, (float) bounds.getBottom() + 24.0f),
                              dashes, 2, 1.0f);
        };

        // Horizontal alignment guides: top of topmost element, common middle,
        // bottom of bottommost element. These mirror the Align Top / Vertical
        // Middle / Align Bottom buttons.
        int topMin = items.front().screen.getY();
        int botMax = items.front().screen.getBottom();
        for (auto& it : items)
        {
            topMin = juce::jmin (topMin, it.screen.getY());
            botMax = juce::jmax (botMax, it.screen.getBottom());
        }
        const int midY = (topMin + botMax) / 2;
        dashedH (topMin);
        dashedH (midY);
        dashedH (botMax);

        // Vertical alignment guides: left, centre, right edges of selection.
        int leftMin = items.front().screen.getX();
        int rightMax = items.front().screen.getRight();
        for (auto& it : items)
        {
            leftMin = juce::jmin (leftMin, it.screen.getX());
            rightMax = juce::jmax (rightMax, it.screen.getRight());
        }
        const int midX = (leftMin + rightMax) / 2;
        dashedV (leftMin);
        dashedV (midX);
        dashedV (rightMax);

        // Pairwise distance labels showing pixel gap between adjacent
        // elements. Distances use the canvas-space size (the value the user
        // sees in the inspector) — not the on-screen scaled size.
        g.setFont (juce::Font (10.0f, juce::Font::bold));

        // Vertical gaps (sorted top-to-bottom).
        auto sortedY = items;
        std::sort (sortedY.begin(), sortedY.end(),
                   [] (const Item& a, const Item& b) { return a.screen.getY() < b.screen.getY(); });
        for (size_t i = 1; i < sortedY.size(); ++i)
        {
            const int prevBottomScreen = sortedY[i - 1].screen.getBottom();
            const int currTopScreen    = sortedY[i].screen.getY();
            if (currTopScreen <= prevBottomScreen) continue;

            const int prevBottomCanvas = sortedY[i - 1].canvas.getBottom();
            const int currTopCanvas    = sortedY[i].canvas.getY();
            const int gap = currTopCanvas - prevBottomCanvas;

            const int x = juce::jmax (sortedY[i - 1].screen.getRight(),
                                      sortedY[i].screen.getRight()) + 14;
            g.setColour (guideStrong);
            g.drawLine ((float) x, (float) prevBottomScreen, (float) x, (float) currTopScreen, 1.0f);
            g.drawLine ((float) x - 3.0f, (float) prevBottomScreen, (float) x + 3.0f, (float) prevBottomScreen, 1.0f);
            g.drawLine ((float) x - 3.0f, (float) currTopScreen,    (float) x + 3.0f, (float) currTopScreen,    1.0f);
            g.drawText (juce::String (gap) + " px", x + 6,
                        (prevBottomScreen + currTopScreen) / 2 - 8, 70, 16,
                        juce::Justification::centredLeft);
        }

        // Horizontal gaps (sorted left-to-right).
        auto sortedX = items;
        std::sort (sortedX.begin(), sortedX.end(),
                   [] (const Item& a, const Item& b) { return a.screen.getX() < b.screen.getX(); });
        for (size_t i = 1; i < sortedX.size(); ++i)
        {
            const int prevRightScreen = sortedX[i - 1].screen.getRight();
            const int currLeftScreen  = sortedX[i].screen.getX();
            if (currLeftScreen <= prevRightScreen) continue;

            const int prevRightCanvas = sortedX[i - 1].canvas.getRight();
            const int currLeftCanvas  = sortedX[i].canvas.getX();
            const int gap = currLeftCanvas - prevRightCanvas;

            const int y = juce::jmin (sortedX[i - 1].screen.getY(),
                                      sortedX[i].screen.getY()) - 14;
            g.setColour (guideStrong);
            g.drawLine ((float) prevRightScreen, (float) y, (float) currLeftScreen, (float) y, 1.0f);
            g.drawLine ((float) prevRightScreen, (float) y - 3.0f, (float) prevRightScreen, (float) y + 3.0f, 1.0f);
            g.drawLine ((float) currLeftScreen,  (float) y - 3.0f, (float) currLeftScreen,  (float) y + 3.0f, 1.0f);
            g.drawText (juce::String (gap) + " px",
                        (prevRightScreen + currLeftScreen) / 2 - 20, y - 18, 60, 16,
                        juce::Justification::centred);
        }
    }

    void CanvasEditor::drawElement (juce::Graphics& g, const LayoutElement& e,
                                    juce::Rectangle<int> r, bool selected) const
    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity));

        if ((e.animationMode.isNotEmpty() && e.animationMode != "none") || e.audioReactive)
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float rate = juce::jmax (0.05f, e.animationRate);
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi * rate) * 0.5) + 0.5);
            const float animationAmount = e.animationMode == "none" ? 0.0f : wave;
            const float reactiveAmount = e.audioReactive ? juce::jmax (0.08f, e.audioReactiveAmount) * 0.55f : 0.0f;
            const float combined = juce::jlimit (0.0f, 1.0f, animationAmount * 0.8f + reactiveAmount);

            if (e.animationMode == "shake")
            {
                const int dx = juce::roundToInt ((wave - 0.5f) * 10.0f * juce::jmax (0.25f, e.audioReactiveAmount));
                r.translate (dx, 0);
            }
            else if (combined > 0.001f)
            {
                const int grow = juce::roundToInt (combined * 8.0f);
                r = r.expanded (grow, grow);
            }

            if (e.animationMode == "glow" || e.audioReactive)
            {
                auto halo = r.expanded (juce::roundToInt (6.0f + combined * 16.0f)).toFloat();
                g.setColour ((e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour)
                             .withAlpha (juce::jlimit (0.05f, 0.28f, 0.08f + combined * 0.20f)));
                g.fillRoundedRectangle (halo, juce::jmax (4.0f, e.cornerRadius + 8.0f));
            }
        }

        if (e.blurAmount > 0.001f && e.type != ElementType::Image)
        {
            const int blurPx = juce::roundToInt (e.blurAmount * 14.0f);
            for (int i = 3; i >= 1; --i)
            {
                const float alpha = e.blurAmount * 0.035f * (float) i;
                g.setColour ((e.backgroundColour.isTransparent() ? e.accentColour : e.backgroundColour).withAlpha (alpha));
                g.fillRoundedRectangle (r.expanded (blurPx * i / 3).toFloat(),
                                        juce::jmax (4.0f, e.cornerRadius + (float) blurPx));
            }
        }

        // ---- Image element ------------------------------------------------
        // 'background' (id == "background") falls back to the procedural hero
        //   artwork when no asset is set.
        // 'hero' or any other Image with an empty asset draws an "Artwork"
        //   placeholder so the user knows where to drop a PNG.
        // Any Image with an asset path loads + draws the file.
        if (e.type == ElementType::Image)
        {
            // Resolve the asset path: explicit asset wins; for the special
            // 'background' element, fall back to project.backgroundImageRelative.
            juce::String relPath = e.asset;
            if (relPath.isEmpty() && e.id == "background")
                relPath = owner.getProject().backgroundImageRelative;

            juce::Image img;
            if (relPath.isNotEmpty())
            {
                juce::File f = juce::File::isAbsolutePath (relPath)
                    ? juce::File (relPath)
                    : owner.getProject().getProjectFolder().getChildFile (relPath);
                if (f.existsAsFile())
                    img = owner.getAssets().loadImage (f);
            }

            if (img.isValid())
            {
                g.drawImage (img, r.toFloat());
            }
            else if (e.id == "background")
            {
                drawHeroArtwork (g, r);
            }
            else
            {
                // "Drop artwork here" placeholder
                g.setColour (juce::Colour (0xff141618));
                g.fillRoundedRectangle (r.toFloat(), 6.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);

                // Frame icon
                const float cx = r.getCentreX(), cy = r.getCentreY() - r.getHeight() * 0.06f;
                const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.35f;
                g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.6f));
                g.drawRoundedRectangle (cx - s * 0.5f, cy - s * 0.5f, s, s, 4.0f, 1.5f);
                g.fillEllipse (cx - s * 0.18f, cy - s * 0.20f, s * 0.16f, s * 0.16f);
                juce::Path mountains;
                mountains.startNewSubPath (cx - s * 0.45f, cy + s * 0.40f);
                mountains.lineTo (cx - s * 0.10f, cy);
                mountains.lineTo (cx + s * 0.10f, cy + s * 0.20f);
                mountains.lineTo (cx + s * 0.30f, cy - s * 0.10f);
                mountains.lineTo (cx + s * 0.50f, cy + s * 0.40f);
                g.strokePath (mountains, juce::PathStrokeType (1.5f));

                // Caption
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (juce::jmax (12.0f, r.getHeight() * 0.06f),
                                       juce::Font::bold));
                const auto label = e.label.isNotEmpty() ? e.label : juce::String ("Artwork");
                g.drawText (label.toUpperCase(),
                            r.withTrimmedTop ((int) (r.getHeight() * 0.55f))
                             .withHeight ((int) (r.getHeight() * 0.10f)),
                            juce::Justification::centred);
                g.setFont (juce::Font (juce::jmax (10.0f, r.getHeight() * 0.045f)));
                g.drawText ("Click to select, then use Inspector -> Asset to load a PNG",
                            r.withTrimmedTop ((int) (r.getHeight() * 0.66f))
                             .withHeight ((int) (r.getHeight() * 0.10f)),
                            juce::Justification::centred);
            }
        }
        else if (e.type == ElementType::Knob)
        {
            const auto* p = owner.getProject().getParameters().find (e.parameterId);
            juce::String value = "-";
            float pos01 = 0.5f;
            if (p != nullptr)
            {
                const float live = owner.getProject().getLiveValues()
                                        .getValue (p->id, p->defaultValue);
                pos01 = (p->max > p->min) ? (live - p->min) / (p->max - p->min) : 0.0f;

                if (p->unit == "Hz" && live >= 1000.0f)
                    value = juce::String (live / 1000.0f, 1) + " kHz";
                else if (p->unit == "Hz")
                    value = juce::String (live, 0) + " Hz";
                else if (p->unit == "%")
                    value = juce::String (juce::roundToInt (live * 100.0f)) + " %";
                else if (p->unit == "s")
                    value = juce::String (live, 2) + " s";
                else if (p->unit.isNotEmpty())
                    value = juce::String (live, 2) + " " + p->unit;
                else
                    value = juce::String (live, 2);
            }

            // Filmstrip override - load PNG and draw frame.
            if (e.filmstripAsset.isNotEmpty())
            {
                juce::File f (juce::File::isAbsolutePath (e.filmstripAsset)
                                ? e.filmstripAsset
                                : owner.getProject().getProjectFolder()
                                       .getChildFile (e.filmstripAsset).getFullPathName());
                if (auto img = owner.getAssets().loadImage (f); img.isValid())
                {
                    int frames = e.filmstripFrames;
                    if (frames <= 0)
                        frames = PatchCraftLookAndFeel::detectFilmstripFrames (img, e.filmstripVertical);
                    auto stripRect = r.withTrimmedBottom (juce::roundToInt (r.getHeight() * 0.30f));
                    PatchCraftLookAndFeel::drawFilmstripFrame (
                        g, stripRect, img, frames, pos01, e.filmstripVertical);

                    drawControlLabel (g, r, e, value);
                }
                else
                {
                    drawCanvasKnob (g, r, e, value, e.accentColour, pos01);
                }
            }
            else
            {
                drawCanvasKnob (g, r, e, value, e.accentColour, pos01);
            }
        }
        else if (e.type == ElementType::Slider)
        {
            drawCanvasVerticalSlider (g, r, e);
        }
        else if (e.type == ElementType::Toggle)
        {
            const auto* def = owner.getProject().getParameters().find (e.parameterId);
            const auto value = owner.getProject().getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
            const bool on = value >= 0.5f;
            auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 92), juce::jmin (r.getHeight(), 38)).toFloat();
            g.setColour (on ? e.accentColour.withAlpha (0.85f) : juce::Colour (0xff202329));
            g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
            g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
            g.drawRoundedRectangle (toggle, toggle.getHeight() * 0.5f, 1.0f);

            const float knobSize = toggle.getHeight() - 8.0f;
            const float knobX = on ? toggle.getRight() - knobSize - 4.0f : toggle.getX() + 4.0f;
            g.setColour (PatchCraftLookAndFeel::textBright());
            g.fillEllipse (knobX, toggle.getY() + 4.0f, knobSize, knobSize);
            drawControlLabel (g, r, e, on ? "ON" : "OFF");
        }
        else if (e.type == ElementType::Meter)
        {
            drawCanvasMeter (g, r);
        }
        else if (e.type == ElementType::EqCurve)
        {
            drawEqCurveElement (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::SpectrumAnalyzer)
        {
            drawSpectrumElement (g, r, e);
        }
        else if (e.type == ElementType::Keyboard)
        {
            drawCanvasKeyboard (g, r);
        }
        else if (e.type == ElementType::Label)
        {
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (juce::jmax (12.0f, (float) r.getHeight() * 0.5f),
                                   juce::Font::bold));
            g.drawText (e.label, r, juce::Justification::centredLeft);
        }
        else if (e.type == ElementType::Dropdown)
        {
            g.setColour (juce::Colour (0xff181a1e));
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            auto rr = r.reduced (28, 0);
            g.drawText ("Deep Horizon", rr, juce::Justification::centred);

            // arrows
            g.setColour (PatchCraftLookAndFeel::textDim());
            const float cy = r.getCentreY();
            juce::Path l; l.addTriangle ((float) r.getX() + 14.0f, cy - 5.0f,
                                          (float) r.getX() + 14.0f, cy + 5.0f,
                                          (float) r.getX() + 8.0f, cy);
            juce::Path rArr; rArr.addTriangle ((float) r.getRight() - 14.0f, cy - 5.0f,
                                               (float) r.getRight() - 14.0f, cy + 5.0f,
                                               (float) r.getRight() - 8.0f, cy);
            g.fillPath (l); g.fillPath (rArr);
        }
        else if (e.type == ElementType::Shape)
        {
            auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 2.0f);
            if (e.shadowAmount > 0.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                g.fillRoundedRectangle (shapeBounds.translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount),
                                        juce::jmax (0.0f, e.cornerRadius));
            }
            if (e.glowAmount > 0.0f)
            {
                g.setColour (e.accentColour.withAlpha (0.18f * e.glowAmount));
                g.fillRoundedRectangle (shapeBounds.expanded (8.0f * e.glowAmount),
                                        juce::jmax (0.0f, e.cornerRadius + 8.0f * e.glowAmount));
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
            {
                path.addRoundedRectangle (shapeBounds, juce::jmax (0.0f, e.cornerRadius));
            }

            if (e.shapeKind != "line")
            {
                g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0x33141822) : e.backgroundColour);
                g.fillPath (path);
            }
            g.setColour (e.borderColour);
            g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));

            if (e.audioReactive)
            {
                g.setColour (e.accentColour.withAlpha (0.70f));
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText ("AUDIO", r.reduced (6), juce::Justification::bottomRight);
            }
        }
        else if (e.type == ElementType::Panel)
        {
            const auto radius = juce::jmax (0.0f, e.cornerRadius);
            if (e.shadowAmount > 0.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                g.fillRoundedRectangle (r.toFloat().reduced (1.0f).translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount), radius);
            }
            g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour);
            g.fillRoundedRectangle (r.toFloat(), radius);
            g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
            g.drawRoundedRectangle (r.toFloat(), radius, juce::jmax (0.5f, e.strokeWidth));
            if (e.label.isNotEmpty())
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (11.0f));
                g.drawText (e.label, r.reduced (8, 4), juce::Justification::topLeft);
            }
        }
        else if (e.type == ElementType::Button)
        {
            // Static label-style button; useful as a chrome decoration. Real
            // tab strips use ElementType::TabPanel.
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.toUpperCase(), r, juce::Justification::centred);
        }
        else if (e.type == ElementType::TabPanel)
        {
            drawTabPanel (g, e, r, selected);
        }
        else if (e.type == ElementType::GranularField)
        {
            const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
            const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (22);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "GRANULAR FIELD",
                        header.removeFromLeft (160), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f));
            g.drawText ("sample position / length / slice / glitch",
                        header, juce::Justification::centredRight, true);
            area.removeFromTop (4);

            g.setColour (border.withAlpha (0.22f));
            for (int i = 1; i < 6; ++i)
            {
                const float x = (float) area.getX() + (float) area.getWidth() * (float) i / 6.0f;
                g.drawVerticalLine (juce::roundToInt (x), (float) area.getY(), (float) area.getBottom());
            }

            const float centreX = (float) area.getX() + (float) area.getWidth() * 0.38f;
            const float span = (float) area.getWidth() * 0.34f;
            g.setColour (accent.withAlpha (0.25f));
            g.fillEllipse (centreX - span * 0.45f, (float) area.getCentreY() - (float) area.getHeight() * 0.28f,
                           span * 0.9f, (float) area.getHeight() * 0.56f);
            g.setColour (accent.withAlpha (0.9f));
            g.drawLine (centreX, (float) area.getY(), centreX, (float) area.getBottom(), 1.4f);
            for (int i = 0; i < 42; ++i)
            {
                const float phase = (float) i * 0.71f;
                const float x = centreX + std::sin (phase * 1.3f) * span * 0.48f;
                const float y = (float) area.getCentreY() + std::cos (phase * 0.9f) * (float) area.getHeight() * 0.23f;
                const float size = 2.0f + (float) (i % 4);
                g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
            }
        }
        else if (e.type == ElementType::MacroControl)
        {
            drawMacroControlElement (g, r, e, owner.getProject());
        }
        else if (e.type == ElementType::ModMatrix)
        {
            drawModMatrixElement (g, r, e, owner.getProject());
        }
        else if (e.type == ElementType::DrumGrid)
        {
            drawDrumGridPreview (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::ArpLane)
        {
            drawArpLanePreview (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::Mixer)
        {
            const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
            const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            const int channels = juce::jlimit (1, 16, e.mixerChannels);

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (22);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MIXER",
                        header.removeFromLeft (160), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f));
            g.drawText ("Auto layer / parameter mixer",
                        header, juce::Justification::centredRight, true);
            area.removeFromTop (6);

            const int stripW = juce::jmax (38, area.getWidth() / channels);
            for (int channel = 0; channel < channels; ++channel)
            {
                auto strip = juce::Rectangle<int> (area.getX() + channel * stripW,
                                                   area.getY(),
                                                   channel == channels - 1
                                                        ? area.getRight() - (area.getX() + channel * stripW)
                                                        : stripW,
                                                   area.getHeight()).reduced (3, 0);
                if (strip.getWidth() <= 10)
                    continue;

                const auto label = stringAtOr (e.mixerChannelLabels, channel,
                                               channel == 0 ? "Main" : "Bus " + juce::String (channel + 1));
                g.setColour (juce::Colour (0xaa0b0f14));
                g.fillRoundedRectangle (strip.toFloat(), 5.0f);
                g.setColour (border.withAlpha (0.75f));
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
                const float level = channel == 0 ? 0.78f : 0.55f - (float) channel * 0.06f;
                const int thumbY = juce::roundToInt (juce::jmap (juce::jlimit (0.15f, 0.90f, level),
                                                                  0.0f, 1.0f,
                                                                  (float) track.getBottom(),
                                                                  (float) track.getY()));
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                g.fillRoundedRectangle (track.toFloat(), 4.0f);
                g.setColour (accent.withAlpha (0.72f));
                g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (track.getX(), thumbY,
                                                                                  track.getRight(), track.getBottom()).toFloat(), 4.0f);
                g.setColour (PatchCraftLookAndFeel::text());
                g.fillRoundedRectangle (juce::Rectangle<float> ((float) centre - 11.0f,
                                                                (float) thumbY - 4.0f,
                                                                22.0f, 8.0f), 3.0f);

                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.0f));
                g.drawText (juce::String (juce::roundToInt (level * 100.0f)),
                            value, juce::Justification::centred, true);
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                g.fillRoundedRectangle (pan.toFloat(), 3.0f);
                g.setColour (accent.withAlpha (0.45f));
                g.drawLine ((float) pan.getX(), (float) pan.getCentreY(),
                            (float) pan.getRight(), (float) pan.getCentreY(), 2.0f);

                auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0);
                auto solo = buttons.reduced (1, 0);
                g.setColour (PatchCraftLookAndFeel::bg().brighter (0.08f));
                g.fillRoundedRectangle (mute.toFloat(), 3.0f);
                g.fillRoundedRectangle (solo.toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.0f, juce::Font::bold));
                g.drawText ("M", mute, juce::Justification::centred, true);
                g.drawText ("S", solo, juce::Justification::centred, true);
            }
        }
        else if (e.type == ElementType::DrumPad || e.type == ElementType::PadGrid)
        {
            const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
            const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
            const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
            const auto inner = r.reduced (e.type == ElementType::PadGrid ? 4 : 0);
            const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
            const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
            const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff1a1d23) : e.backgroundColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            for (int row = 0; row < rows; ++row)
            {
                for (int col = 0; col < cols; ++col)
                {
                    juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                                (float) inner.getY() + row * (padH + gap),
                                                padW, padH);
                    g.setColour (bg.brighter (0.05f));
                    g.fillRoundedRectangle (pad, 4.0f);
                    g.setColour (accent.withAlpha (0.55f));
                    g.drawRoundedRectangle (pad.reduced (0.5f), 4.0f, 1.0f);

                    g.setColour (PatchCraftLookAndFeel::text().withAlpha (0.85f));
                    g.setFont (juce::Font (juce::jmin (12.0f, padH * 0.28f), juce::Font::bold));
                    const int padIdx = row * cols + col;
                    const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                    juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                        ? e.label : juce::String (padIdx + 1);
                    g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f),
                                juce::Justification::centred);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::Font (juce::jmin (10.0f, padH * 0.22f)));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                                pad.reduced (4.0f).removeFromBottom (padH * 0.35f),
                                juce::Justification::centred);
                }
            }
        }
        else
        {
            g.setColour (juce::Colour (0xff15171b));
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (11.0f));
            g.drawText (elementTypeDisplayName (e.type),
                        r, juce::Justification::centred);
        }

        // Selection outline
        g.setOpacity (1.0f);
        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (r.expanded (1), 1);
            // resize handles
            const int hs = 6;
            for (auto p : { r.getTopLeft(), r.getTopRight(),
                            r.getBottomLeft(), r.getBottomRight() })
            {
                g.fillRect (p.x - hs / 2, p.y - hs / 2, hs, hs);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Mouse interaction
    //
    // Click priority:
    //   1. BR corner of *selected* element → resize.
    //   2. Inside the control body of a knob/slider bound to a parameter →
    //      value drag (vertical drag changes the live parameter value).
    //      This works whether the element is currently selected or not, so
    //      the canvas behaves like a real plug-in.
    //   3. Otherwise click selects + starts a move drag.
    // -------------------------------------------------------------------------
    bool CanvasEditor::hitTestControlBody (const LayoutElement& el,
                                           juce::Rectangle<int> r,
                                           juce::Point<int> p) const
    {
        if (el.parameterId.isEmpty()) return false;
        if (el.type == ElementType::Knob || el.type == ElementType::MacroControl)
        {
            // Use a circle inside r (matching how the knob is drawn).
            auto body = r;
            if (el.type == ElementType::MacroControl)
                body = r.reduced (9, 7).withTrimmedTop (18).withTrimmedRight (juce::jmax (0, r.getWidth() - 92));
            const float cx = body.getCentreX();
            const float cy = body.getCentreY() - body.getHeight() * 0.12f;
            const float rad = juce::jmin (body.getWidth(), (int) (body.getHeight() * 0.7f)) * 0.5f;
            const float dx = p.x - cx, dy = p.y - cy;
            return dx * dx + dy * dy <= rad * rad;
        }
        if (el.type == ElementType::Slider)
            return r.contains (p);
        return false;
    }

    bool CanvasEditor::drumCellAt (const LayoutElement& element, juce::Rectangle<int> r,
                                   juce::Point<int> p, int& pattern, int& track,
                                   int& step, float& velocity) const
    {
        const auto* block = findDrumMachineBlock (owner.getProject().getDspGraph());
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
        if (area.getHeight() <= 28 || area.getWidth() <= 80)
            return false;
        area.removeFromTop (20);
        area.removeFromTop (4);

        const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
        auto grid = area.withTrimmedLeft (labelW);
        if (! grid.contains (p))
            return false;

        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        if (cellW <= 0.0f || cellH <= 0.0f)
            return false;

        step = juce::jlimit (0, steps - 1, (int) ((float) (p.x - grid.getX()) / cellW));
        track = juce::jlimit (0, tracks - 1, (int) ((float) (p.y - area.getY()) / cellH));
        const float localY = (float) p.y - ((float) area.getY() + (float) track * cellH);
        velocity = juce::jlimit (0.08f, 1.0f, 1.0f - (localY / cellH));
        return true;
    }

    bool CanvasEditor::editDrumGridCellAt (const LayoutElement& element, juce::Rectangle<int> r,
                                           juce::Point<int> p, const juce::ModifierKeys& mods,
                                           bool startGesture)
    {
        if (! (mods.isAltDown() || mods.isShiftDown() || mods.isCtrlDown() || mods.isCommandDown()))
            return false;

        int pattern = 0, track = 0, step = 0;
        float velocity = 0.8f;
        if (! drumCellAt (element, r, p, pattern, track, step, velocity))
            return false;

        if (! startGesture && track == lastDrumGridTrack && step == lastDrumGridStep)
            return true;

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureDrumMachineBlock (graph);
        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        const bool wasOn = blockValue (block, prefix + "On", 0.0f) >= 0.5f;

        if (auto* mutableElement = owner.getProject().getLayout().find (element.id))
        {
            mutableElement->drumPattern = pattern;
            if (block.values.find ("dmTracks") == block.values.end())
                block.values["dmTracks"] = (float) juce::jlimit (1, 16, mutableElement->drumTracks);
            if (block.values.find ("dmSteps") == block.values.end())
                block.values["dmSteps"] = (float) juce::jlimit (1, 64, mutableElement->drumSteps);
        }
        block.values["dmPattern"] = (float) pattern;
        block.values["dmTransport"] = 1.0f;

        if (mods.isCtrlDown() || mods.isCommandDown())
        {
            const int current = juce::jlimit (1, 4, juce::roundToInt (blockValue (block, prefix + "Div", 1.0f)));
            block.values[prefix + "On"] = 1.0f;
            block.values[prefix + "Vel"] = velocity;
            block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.42f);
            block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
            block.values[prefix + "Div"] = (float) (current >= 4 ? 1 : current + 1);
            drumGridPaintState = true;
        }
        else if (mods.isShiftDown() && ! mods.isAltDown())
        {
            block.values[prefix + "On"] = 1.0f;
            block.values[prefix + "Vel"] = velocity;
            block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.55f);
            block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
            block.values[prefix + "Div"] = blockValue (block, prefix + "Div", 1.0f);
            drumGridPaintState = true;
        }
        else
        {
            if (startGesture)
                drumGridPaintState = ! wasOn;
            block.values[prefix + "On"] = drumGridPaintState ? 1.0f : 0.0f;
            if (drumGridPaintState)
            {
                block.values[prefix + "Vel"] = velocity;
                block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.55f);
                block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
                block.values[prefix + "Div"] = blockValue (block, prefix + "Div", 1.0f);
            }
        }

        graph.userConfigured = true;
        owner.getProject().markDirty();
        layoutChangedDuringDrag = true;
        lastDrumGridTrack = track;
        lastDrumGridStep = step;
        repaint (r.expanded (8));
        return true;
    }

    void CanvasEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus();
        layoutChangedDuringDrag = false;
        dragLayoutBefore.clear();
        dragActionName.clear();
        if (e.mods.isPopupMenu())
        {
            showContextMenu (e.getPosition());
            return;
        }
        const auto& elements = owner.getProject().getLayout().getAll();

        // 0. TabPanel tabs: normal click swaps the current page. Holding Shift
        // bypasses navigation so the tab strip itself can be selected/moved.
        if (! e.mods.isShiftDown())
        {
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type != ElementType::TabPanel) continue;
                if (! isElementOnCurrentTab (*it)) continue;
                auto r = elementScreenRect (*it);
                const int tabIdx = hitTabIndex (*it, r, e.getPosition());
                if (tabIdx >= 0 && tabIdx < it->tabs.size())
                {
                    const auto targetGroup = scopedTabGroupId (*it, it->tabs[tabIdx]);
                    if (it->id == "tabs")
                        setCurrentTabGroup (targetGroup);
                    else
                    {
                        activeTabGroupsByPanel[it->id] = targetGroup;
                        repaint();
                    }
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);
                    return;
                }
            }

            ensureManualContainerDefaults();
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type != ElementType::Button && it->type != ElementType::Toggle
                    && it->type != ElementType::Label && it->type != ElementType::Image)
                    continue;
                if (! isElementOnCurrentTab (*it)) continue;
                if (! elementScreenRect (*it).contains (e.getPosition())) continue;
                if (triggerManualContainer (*it))
                {
                    owner.setSelectedElementId (it->id);
                    return;
                }
            }
        }

        // 1. BR-resize on selected element first.
        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            const int hs = 8;
            if (juce::Rectangle<int> (r.getRight() - hs, r.getBottom() - hs, hs * 2, hs * 2)
                .contains (e.getPosition()))
            {
                dragStart    = e.getPosition();
                dragOriginal = *sel;
                dragLayoutBefore = owner.getProject().getLayout().getAll();
                dragActionName = owner.getSelectedElementIds().size() > 1 ? "Scale selection" : "Resize element";
                mode = DragMode::ResizeBR;
                return;
            }
        }

        // 2/3. Hit test in reverse z-order (front to back).
        for (auto it = elements.rbegin(); it != elements.rend(); ++it)
        {
            if (! it->visible) continue;
            if (it->type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementScreenRect (*it);
            if (! r.contains (e.getPosition())) continue;

            const auto canvas = owner.getProject().getCanvasSize();
            const bool passiveLockedBackground =
                it->locked
                && (it->id == "background"
                    || (it->type == ElementType::Image
                        && it->x <= 0
                        && it->y <= 0
                        && it->width >= canvas.width - 4
                        && it->height >= canvas.height - 4));
            if (passiveLockedBackground && ! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
                continue;

            // Locked elements (e.g. background artwork): selectable so the
            // inspector's Asset/Browse field is accessible, but no move/resize.
            if (it->locked)
            {
                if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                    owner.toggleSelectedElementId (it->id);
                else
                    owner.setSelectedElementId (it->id);
                mode = DragMode::None;
                return;
            }

            if (it->type == ElementType::DrumGrid
                && owner.isElementSelected (it->id)
                && (e.mods.isAltDown() || e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown())
                && editDrumGridCellAt (*it, r, e.getPosition(), e.mods, true))
            {
                drumGridEditElementId = it->id;
                dragStart = e.getPosition();
                mode = DragMode::DrumGridEdit;
                owner.refreshAllPanels();
                return;
            }

            const bool isInteractiveControl = it->type == ElementType::Knob || it->type == ElementType::Slider
                                           || it->type == ElementType::Toggle || it->type == ElementType::Dropdown
                                           || it->type == ElementType::ValueDisplay
                                           || it->type == ElementType::MacroControl;
            if (! e.mods.isShiftDown() && owner.getProject().getManifest().playerShowParameterGuidance && isInteractiveControl)
            {
                const bool attemptedControlUse = (it->type == ElementType::Knob || it->type == ElementType::Slider
                                               || it->type == ElementType::MacroControl)
                    ? hitTestControlBody (*it, r, e.getPosition())
                    : r.contains (e.getPosition());
                if (attemptedControlUse)
                {
                    const auto guidance = canvasControlGuidance (owner.getProject(), *it);
                    if (guidance.isNotEmpty())
                    {
                        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                            owner.toggleSelectedElementId (it->id);
                        else
                            owner.setSelectedElementId (it->id);
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                            "Control is not connected", guidance);
                        mode = DragMode::None;
                        return;
                    }
                }
            }

            if (! e.mods.isShiftDown() && it->type == ElementType::Toggle && it->parameterId.isNotEmpty())
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

                    const auto current = owner.getProject().getLiveValues().getValue (it->parameterId, def->defaultValue);
                    owner.getProject().getLiveValues().setValue (it->parameterId, current >= 0.5f ? def->min : def->max);
                    applyArpLaneParameterToGraph (owner.getProject(), it->parameterId);
                    if (it->parameterId.startsWith ("arpLane"))
                        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                    repaint();
                    return;
                }
            }

            if (! e.mods.isShiftDown() && it->type == ElementType::Dropdown && it->parameterId.isNotEmpty())
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

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
                        [this, parameterId = it->parameterId, values] (int result)
                        {
                            if (result <= 0 || result > (int) values.size()) return;
                            owner.getProject().getLiveValues().setValue (parameterId, values[(size_t) result - 1]);
                            applyArpLaneParameterToGraph (owner.getProject(), parameterId);
                            if (parameterId.startsWith ("arpLane"))
                                owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                            repaint();
                        });
                    return;
                }
            }

            // Value drag if the click is inside the control body.
            if (! e.mods.isShiftDown() && hitTestControlBody (*it, r, e.getPosition()))
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);
                    dragStart        = e.getPosition();
                    dragParameterId  = it->parameterId;
                    dragValueElementId = it->id;
                    dragValueStart   = owner.getProject().getLiveValues()
                                            .getValue (it->parameterId, def->defaultValue);
                    mode = DragMode::ValueDrag;
                    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
                    return;
                }
            }

            // Move drag.
            if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                owner.toggleSelectedElementId (it->id);
            else if (! owner.isElementSelected (it->id))
                owner.setSelectedElementId (it->id);
            dragStart    = e.getPosition();
            dragOriginal = *it;
            dragLayoutBefore = owner.getProject().getLayout().getAll();
            dragActionName = "Move selection";
            captureMoveOriginsForSelection();
            mode = DragMode::Move;
            return;
        }

        if (! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
            owner.clearSelection();
        dragStart = e.getPosition();
        marqueeRect = { e.x, e.y, 0, 0 };
        mode = DragMode::Marquee;
    }

    void CanvasEditor::addElementAt (ElementType type, juce::Point<int> canvasPos, juce::String parameterId)
    {
        LayoutElement el;
        el.type = type;
        el.x = canvasPos.x;
        el.y = canvasPos.y;
        el.width = (type == ElementType::Panel) ? 320
                 : type == ElementType::Shape ? 180
                 : type == ElementType::TabPanel ? 420
                 : type == ElementType::GranularField ? 440
                 : type == ElementType::EqCurve ? 460
                 : type == ElementType::SpectrumAnalyzer ? 460
                 : type == ElementType::DrumGrid ? 560
                 : type == ElementType::ArpLane ? 260
                 : type == ElementType::Mixer ? 520
                 : type == ElementType::MacroControl ? 190
                 : type == ElementType::ModMatrix ? 420
                 : type == ElementType::PadGrid ? 360
                 : type == ElementType::DrumPad ? 80
                 : type == ElementType::Toggle ? 128 : 96;
        el.height = (type == ElementType::Panel) ? 180
                  : type == ElementType::Shape ? 120
                  : type == ElementType::TabPanel ? 44
                  : type == ElementType::GranularField ? 220
                  : type == ElementType::EqCurve ? 180
                  : type == ElementType::SpectrumAnalyzer ? 160
                  : type == ElementType::DrumGrid ? 220
                  : type == ElementType::ArpLane ? 330
                  : type == ElementType::Mixer ? 260
                  : type == ElementType::MacroControl ? 132
                  : type == ElementType::ModMatrix ? 220
                  : type == ElementType::PadGrid ? 360
                  : type == ElementType::DrumPad ? 80
                  : type == ElementType::Toggle ? 54 : 96;
        el.parameterId = std::move (parameterId);
        if (auto* def = owner.getProject().getParameters().find (el.parameterId))
            el.label = def->name;
        else
            el.label = el.parameterId.isNotEmpty() ? el.parameterId : elementTypeDisplayName (type);
        el.style = "Modern Dark";
        el.groupId = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        if (type == ElementType::TabPanel)
            el.tabs = { "Tab 1", "Tab 2" };
        if (type != ElementType::Panel && type != ElementType::Group && type != ElementType::TabPanel)
        {
            if (auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId()))
            {
                if (selected->type == ElementType::Panel || selected->type == ElementType::Group)
                {
                    el.containerId = selected->id;
                    el.groupId = selected->groupId;
                }
                else if (selected->type == ElementType::TabPanel && ! selected->tabs.isEmpty())
                {
                    const auto active = activeTabGroupsByPanel.find (selected->id);
                    el.groupId = active != activeTabGroupsByPanel.end()
                        ? active->second
                        : scopedTabGroupId (*selected, selected->tabs[0]);
                    el.containerId.clear();
                }
            }
        }
        if (type == ElementType::Group)
            el.label = "New Group";
        if (type == ElementType::Panel)
        {
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::accent();
        }
        if (el.parameterId == "bpmSync" || el.parameterId == "retrigger")
        {
            el.groupId.clear();
            el.containerId.clear();
            el.labelPosition = "right";
            el.labelSpacing = 6.0f;
        }
        if (type == ElementType::Shape)
        {
            el.backgroundColour = juce::Colour (0x66141822);
            el.borderColour = PatchCraftLookAndFeel::accent();
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.cornerRadius = 16.0f;
            el.strokeWidth = 2.0f;
        }
        if (type == ElementType::DrumPad)
        {
            el.label = "Pad";
            el.padRows = 1;
            el.padCols = 1;
            el.padBaseNote = 36;
            el.cornerRadius = 6.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::PadGrid)
        {
            el.label = {};
            el.padRows = 4;
            el.padCols = 4;
            el.padBaseNote = 36;
            el.cornerRadius = 6.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::DrumGrid)
        {
            el.label = "Drum Pattern";
            el.drumTracks = 8;
            el.drumSteps = 16;
            el.drumPattern = 0;
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::ArpLane)
        {
            el.label = "Arp Lane";
            el.parameterId.clear();
            el.arpLaneIndex = 0;
            el.arpLaneSteps = 16;
            el.arpLaneMode = "bank";
            el.cornerRadius = 12.0f;
            el.strokeWidth = 1.4f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd10141a);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::GranularField)
        {
            el.label = "Granular Field";
            el.parameterId = el.parameterId.isNotEmpty() ? el.parameterId : "sampleStart";
            el.cornerRadius = 10.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::EqCurve)
        {
            el.label = "EQ Curve";
            el.parameterId.clear();
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::SpectrumAnalyzer)
        {
            el.label = "Spectrum";
            el.parameterId.clear();
            el.cornerRadius = 8.0f;
            el.accentColour = juce::Colour (0xff20d6ff);
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::Mixer)
        {
            el.label = "Mixer";
            el.labelPosition = "hidden";
            el.mixerMode = "auto";
            el.mixerChannels = 4;
            el.mixerChannelLabels.add ("Main");
            el.mixerChannelLabels.add ("Bus 2");
            el.mixerChannelLabels.add ("Bus 3");
            el.mixerChannelLabels.add ("Bus 4");
            el.mixerVolumeParams.add ("volume");
            el.mixerPanParams.add ("pan");
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::MacroControl)
        {
            el.label = "Macro 1";
            if (el.parameterId.isEmpty())
            {
                auto idExists = [this] (const juce::String& id)
                {
                    if (owner.getProject().getParameters().find (id) != nullptr)
                        return true;
                    for (const auto& block : owner.getProject().getDspGraph().blocks)
                        if (block.id == id)
                            return true;
                    return false;
                };
                int suffix = 1;
                do
                {
                    el.parameterId = "macro_" + juce::String (suffix++);
                }
                while (idExists (el.parameterId));
            }
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();

            auto& graph = owner.getProject().getDspGraph();
            bool hasMacroBlock = false;
            for (const auto& block : graph.blocks)
                if (block.id == el.parameterId)
                    hasMacroBlock = true;

            if (! hasMacroBlock)
            {
                DspBlock block;
                block.id = el.parameterId;
                block.section = "mod";
                block.type = "macro";
                block.name = el.label;
                block.enabled = true;
                block.values["value"] = 0.5f;
                graph.blocks.push_back (std::move (block));
                graph.userConfigured = true;
            }
        }
        if (type == ElementType::ModMatrix)
        {
            el.label = "Mod Matrix";
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }

        juce::String addedId;
        owner.getProject().performLayoutEdit ("Add " + elementTypeDisplayName (type),
            [&] (LayoutModel& m)
            {
                auto copy = el;
                auto& added = m.add (copy);
                addedId = added.id;
            });
        if (addedId.isNotEmpty())
            owner.setSelectedElementId (addedId);
    }

    void CanvasEditor::addMixerChannelAt (juce::Point<int> canvasPos)
    {
        LayoutElement el;
        el.type = ElementType::Mixer;
        el.x = canvasPos.x;
        el.y = canvasPos.y;
        el.width = 116;
        el.height = 260;
        el.label = "Main";
        el.labelPosition = "hidden";
        el.style = "Modern Dark";
        el.groupId = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        el.mixerMode = "parameters";
        el.mixerChannels = 1;
        el.mixerChannelLabels.add ("Main");
        el.mixerVolumeParams.add ("volume");
        el.mixerPanParams.add ("pan");
        el.cornerRadius = 8.0f;
        el.accentColour = PatchCraftLookAndFeel::accent();
        el.backgroundColour = juce::Colour (0x33141822);
        el.borderColour = PatchCraftLookAndFeel::border();

        juce::String addedId;
        owner.getProject().performLayoutEdit ("Add mixer channel",
            [&] (LayoutModel& m)
            {
                auto& added = m.add (el);
                addedId = added.id;
            });

        if (addedId.isNotEmpty())
            owner.setSelectedElementId (addedId);
    }

    bool CanvasEditor::selectionContainsMixer() const
    {
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* el = owner.getProject().getLayout().find (id))
                if (el->type == ElementType::Mixer)
                    return true;

        return false;
    }

    void CanvasEditor::explodeSelectedMixers()
    {
        struct SourceMixer
        {
            LayoutElement element;
        };

        std::vector<SourceMixer> mixers;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* el = owner.getProject().getLayout().find (id))
                if (el->type == ElementType::Mixer)
                    mixers.push_back ({ *el });

        if (mixers.empty())
            return;

        juce::StringArray newIds;
        owner.getProject().performLayoutEdit ("Break mixer into channel strips",
            [&] (LayoutModel& layout)
            {
                for (const auto& source : mixers)
                {
                    const auto& src = source.element;
                    const int channels = juce::jlimit (1, 16, src.mixerChannels);
                    const int stripW = juce::jmax (92, juce::jmin (140, src.width / channels));
                    const int gap = 10;

                    for (int channel = 0; channel < channels; ++channel)
                    {
                        LayoutElement strip = src;
                        strip.id.clear();
                        strip.x = src.x + channel * (stripW + gap);
                        strip.y = src.y;
                        strip.width = stripW;
                        strip.height = src.height;
                        strip.label = mixerSlotAt (src.mixerChannelLabels, channel,
                                                   channel == 0 ? juce::String ("Main")
                                                                : "Bus " + juce::String (channel + 1));
                        strip.labelPosition = "hidden";
                        strip.mixerMode = "parameters";
                        strip.mixerChannels = 1;
                        strip.mixerChannelLabels.clear();
                        strip.mixerChannelLabels.add (strip.label);
                        strip.mixerVolumeParams.clear();
                        strip.mixerPanParams.clear();
                        strip.mixerMuteParams.clear();
                        strip.mixerSoloParams.clear();
                        strip.mixerVolumeParams.add (mixerSlotAt (src.mixerVolumeParams, channel,
                                                                  channel == 0 ? juce::String ("volume") : juce::String()));
                        strip.mixerPanParams.add (mixerSlotAt (src.mixerPanParams, channel,
                                                               channel == 0 ? juce::String ("pan") : juce::String()));
                        strip.mixerMuteParams.add (mixerSlotAt (src.mixerMuteParams, channel));
                        strip.mixerSoloParams.add (mixerSlotAt (src.mixerSoloParams, channel));
                        auto& added = layout.add (strip);
                        newIds.add (added.id);
                    }

                    layout.remove (src.id);
                }
            });

        if (! newIds.isEmpty())
            owner.setSelectedElementIds (newIds);
    }

    void CanvasEditor::addDrumMachineControlLayout (juce::Point<int> canvasPos)
    {
        auto& graph = owner.getProject().getDspGraph();
        ensureDrumMachineBlock (graph);

        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        owner.getProject().performLayoutEdit ("Add Drum Machine Control Surface",
            [&] (LayoutModel& layout)
            {
                LayoutElement panel;
                panel.type = ElementType::Panel;
                panel.label = "Drum Machine Surface";
                panel.x = canvasPos.x;
                panel.y = canvasPos.y;
                panel.width = 1080;
                panel.height = 420;
                panel.groupId = tabGroup;
                panel.cornerRadius = 18.0f;
                panel.strokeWidth = 2.0f;
                panel.backgroundColour = juce::Colour (0xcc070a0f);
                panel.borderColour = PatchCraftLookAndFeel::accent();
                panel.accentColour = PatchCraftLookAndFeel::accent();
                auto& addedPanel = layout.add (panel);
                const auto panelId = addedPanel.id;
                addedIds.add (panelId);

                auto addChild = [&] (LayoutElement child, const juce::String& prefix)
                {
                    child.containerId = panelId;
                    child.groupId = tabGroup;
                    if (child.id.isEmpty())
                        child.id = layout.generateUniqueId (prefix);
                    auto& added = layout.add (child);
                    addedIds.add (added.id);
                };

                LayoutElement title;
                title.type = ElementType::Label;
                title.label = "DRUM MACHINE BUILDER";
                title.x = canvasPos.x + 20;
                title.y = canvasPos.y + 14;
                title.width = 360;
                title.height = 24;
                title.labelSize = 16.0f;
                title.textColour = PatchCraftLookAndFeel::textBright();
                title.accentColour = PatchCraftLookAndFeel::accent();
                addChild (title, "label_");

                LayoutElement help;
                help.type = ElementType::Label;
                help.label = "Pads trigger mapped one-shots. The pattern grid drives playback. Pad level knobs are live per-pad controls.";
                help.x = canvasPos.x + 392;
                help.y = canvasPos.y + 16;
                help.width = 650;
                help.height = 22;
                help.labelSize = 11.0f;
                help.textColour = PatchCraftLookAndFeel::textDim();
                addChild (help, "label_");

                LayoutElement pads;
                pads.type = ElementType::PadGrid;
                pads.label = "Performance Pads";
                pads.x = canvasPos.x + 20;
                pads.y = canvasPos.y + 58;
                pads.width = 300;
                pads.height = 300;
                pads.padRows = 4;
                pads.padCols = 4;
                pads.padBaseNote = 36;
                pads.cornerRadius = 10.0f;
                pads.backgroundColour = juce::Colour (0x55111822);
                pads.borderColour = PatchCraftLookAndFeel::border();
                pads.accentColour = PatchCraftLookAndFeel::accent();
                addChild (pads, "padGrid_");

                LayoutElement grid;
                grid.type = ElementType::DrumGrid;
                grid.label = "Pattern Sequencer";
                grid.x = canvasPos.x + 340;
                grid.y = canvasPos.y + 58;
                grid.width = 700;
                grid.height = 210;
                grid.drumTracks = 8;
                grid.drumSteps = 16;
                grid.drumPattern = 0;
                grid.cornerRadius = 10.0f;
                grid.backgroundColour = juce::Colour (0x55111822);
                grid.borderColour = PatchCraftLookAndFeel::border();
                grid.accentColour = PatchCraftLookAndFeel::accent();
                addChild (grid, "drumGrid_");

                for (int pad = 0; pad < 16; ++pad)
                {
                    const int row = pad / 8;
                    const int col = pad % 8;

                    LayoutElement knob;
                    knob.type = ElementType::Knob;
                    knob.parameterId = "pad" + juce::String (pad + 1) + "Volume";
                    knob.label = "P" + juce::String (pad + 1);
                    knob.x = canvasPos.x + 346 + col * 86;
                    knob.y = canvasPos.y + 292 + row * 56;
                    knob.width = 58;
                    knob.height = 50;
                    knob.labelPosition = "bottom";
                    knob.labelSpacing = 3.0f;
                    knob.labelSize = 9.0f;
                    knob.backgroundColour = juce::Colour (0x33141822);
                    knob.borderColour = PatchCraftLookAndFeel::border();
                    knob.accentColour = PatchCraftLookAndFeel::accent();
                    addChild (knob, "knob_");
                }
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        repaint();
    }

    void CanvasEditor::addCircleSeqBackgroundKit (juce::Point<int> canvasPos)
    {
        const auto canvas = owner.getProject().getCanvasSize();
        const int canvasW = juce::jmax (900, canvas.width);
        const int canvasH = juce::jmax (540, canvas.height);
        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        const int cx = canvasW / 2;
        const int cy = juce::roundToInt ((float) canvasH * 0.43f);
        const int ring = juce::jlimit (280, 680, juce::jmin (canvasW, canvasH) - 150);
        const int panelY = juce::jmax (cy + ring / 2 + 22, canvasH - 182);

        owner.getProject().performLayoutEdit ("Add CircleSEQ background kit",
            [&] (LayoutModel& layout)
            {
                auto addShape = [&] (juce::String label,
                                     juce::String shapeKind,
                                     int x, int y, int w, int h,
                                     juce::Colour fill,
                                     juce::Colour border,
                                     float stroke,
                                     float radius,
                                     float opacity = 1.0f)
                {
                    LayoutElement shape;
                    shape.type = ElementType::Shape;
                    shape.label = std::move (label);
                    shape.shapeKind = std::move (shapeKind);
                    shape.x = x;
                    shape.y = y;
                    shape.width = juce::jmax (1, w);
                    shape.height = juce::jmax (1, h);
                    shape.groupId = tabGroup;
                    shape.backgroundColour = fill;
                    shape.borderColour = border;
                    shape.accentColour = border;
                    shape.strokeWidth = stroke;
                    shape.cornerRadius = radius;
                    shape.opacity = opacity;
                    auto& added = layout.add (shape);
                    addedIds.add (added.id);
                };

                addShape ("Background plate", "roundedRect",
                          0, 0, canvasW, canvasH,
                          juce::Colour (0xff05080c), juce::Colour (0xff101722),
                          1.0f, 18.0f);

                addShape ("Top glass haze", "ellipse",
                          cx - ring, cy - ring, ring * 2, ring * 2,
                          juce::Colour (0x2210c6ff), juce::Colour (0x6630d9ff),
                          2.0f, 0.0f, 0.70f);
                addShape ("Outer radial ring", "ellipse",
                          cx - ring / 2, cy - ring / 2, ring, ring,
                          juce::Colour (0x1110c6ff), juce::Colour (0xaa00d4ff),
                          2.0f, 0.0f);
                addShape ("Middle radial ring", "ellipse",
                          cx - ring * 39 / 100, cy - ring * 39 / 100, ring * 78 / 100, ring * 78 / 100,
                          juce::Colour (0x0500d4ff), juce::Colour (0x7718a8ff),
                          1.4f, 0.0f);
                addShape ("Inner shadow hub", "ellipse",
                          cx - ring * 16 / 100, cy - ring * 16 / 100, ring * 32 / 100, ring * 32 / 100,
                          juce::Colour (0xcc03070b), juce::Colour (0x5530d9ff),
                          1.0f, 0.0f);

                addShape ("Horizontal orbit guide", "line",
                          cx - ring / 2, cy - 1, ring, 2,
                          juce::Colours::transparentBlack, juce::Colour (0x6600d4ff),
                          1.2f, 0.0f);
                addShape ("Vertical orbit guide", "line",
                          cx - 1, cy - ring / 2, 2, ring,
                          juce::Colours::transparentBlack, juce::Colour (0x6600d4ff),
                          1.2f, 0.0f);

                const int sideW = juce::jmax (180, (canvasW - ring) / 2 - 46);
                addShape ("Left utility rail", "roundedRect",
                          24, 72, sideW, juce::jmax (280, canvasH - 268),
                          juce::Colour (0x77101822), juce::Colour (0x442b8cff),
                          1.0f, 10.0f);
                addShape ("Right inspector rail", "roundedRect",
                          canvasW - sideW - 24, 72, sideW, juce::jmax (280, canvasH - 268),
                          juce::Colour (0x77101822), juce::Colour (0x442b8cff),
                          1.0f, 10.0f);

                const int panelGap = 14;
                const int panelW = juce::jmax (180, (canvasW - 48 - panelGap * 3) / 4);
                for (int i = 0; i < 4; ++i)
                {
                    addShape ("Bottom control bay " + juce::String (i + 1), "roundedRect",
                              24 + i * (panelW + panelGap), panelY, panelW, juce::jmax (112, canvasH - panelY - 24),
                              juce::Colour (0x88101720),
                              juce::Colour (i == 0 ? 0x8840d8ff : i == 1 ? 0x88a96bff : i == 2 ? 0x88ff4f82 : 0x88ffa600),
                              1.2f, 9.0f);
                }
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        repaint();
        (void) canvasPos;
    }

    void CanvasEditor::showContextMenu (juce::Point<int> screenPos)
    {
        const auto canvasPos = screenToCanvas (screenPos);
        juce::String clickedElementId;
        bool clickedAssignableControl = false;
        for (auto it = owner.getProject().getLayout().getAll().rbegin();
             it != owner.getProject().getLayout().getAll().rend(); ++it)
        {
            if (! it->visible || it->type == ElementType::Group || ! isElementOnCurrentTab (*it))
                continue;
            if (! elementScreenRect (*it).contains (screenPos))
                continue;

            clickedElementId = it->id;
            clickedAssignableControl = it->type == ElementType::Knob
                                    || it->type == ElementType::Slider
                                    || it->type == ElementType::Button
                                    || it->type == ElementType::Toggle
                                    || it->type == ElementType::Dropdown
                                    || it->type == ElementType::ValueDisplay
                                    || it->type == ElementType::MacroControl;
            break;
        }

        if (clickedElementId.isNotEmpty() && ! owner.isElementSelected (clickedElementId))
            owner.setSelectedElementId (clickedElementId);

        juce::PopupMenu menu;
        menu.addItem (99, "Search Canvas Actions...");
        menu.addSeparator();
        menu.addSectionHeader ("Add Container");
        menu.addItem (1, "Panel / Container");
        menu.addItem (2, "Folder Group");
        menu.addItem (3, "Tab Panel");
        menu.addSeparator();
        menu.addSectionHeader ("Add Shape");
        menu.addItem (5, "Rounded Rectangle");
        menu.addItem (6, "Ellipse");
        menu.addItem (7, "Triangle");
        menu.addItem (8, "Diamond");
        menu.addItem (9, "Line");
        menu.addSeparator();
        const bool directAssignMode = clickedAssignableControl && ! owner.getSelectedElementIds().isEmpty();
        menu.addSectionHeader (directAssignMode ? "Assign Selected Element" : "Add DSP Control");
        menu.addItem (11, "Find Parameter...");
        menu.addItem (12, "Add Arpeggiator (MIDI Playground)...");
        menu.addItem (14, "Add Mixer");
        menu.addItem (24, "Add Single Mixer Channel");
        menu.addItem (25, "Break Selected Mixer Into Channels", selectionContainsMixer());
        menu.addItem (20, "Add Macro Control");
        menu.addItem (21, "Add Mod Matrix");
        menu.addItem (22, "Add Granular Field");
        menu.addItem (23, "Add Drum Machine Surface");
        menu.addItem (26, "Add Arp Studio Lane");
        menu.addSeparator();

        // Group parameters by ParameterDef::category so the menu doesn't dump
        // every parameter as a flat 50-row wall of text. Each category becomes
        // its own submenu; the user reaches a parameter via Add DSP Control →
        // <category> → <param>. Items without a category fall under "Other".
        std::map<juce::String, std::vector<const ParameterDef*>> byCategory;
        juce::StringArray categoryOrder;
        for (const auto& def : owner.getProject().getParameters().getAll())
        {
            auto cat = def.category.isNotEmpty() ? def.category : juce::String ("Other");
            if (! byCategory.count (cat))
                categoryOrder.add (cat);
            byCategory[cat].push_back (&def);
        }
        int itemId = 100;
        std::map<int, juce::String> paramByItem;
        std::map<int, juce::String> assignParamByItem;
        for (const auto& cat : categoryOrder)
        {
            const auto& defs = byCategory[cat];
            juce::PopupMenu sub;
            for (const auto* def : defs)
            {
                sub.addItem (itemId, def->name + "  (" + def->id + ")");
                if (directAssignMode)
                    assignParamByItem[itemId++] = def->id;
                else
                    paramByItem[itemId++] = def->id;
            }
            menu.addSubMenu (cat, sub);
        }

        if (! owner.getSelectedElementIds().isEmpty())
        {
            juce::PopupMenu assignMenu;
            assignMenu.addItem (13, "Find Parameter...");
            assignMenu.addSeparator();
            int assignItemId = 10000;
            for (const auto& cat : categoryOrder)
            {
                juce::PopupMenu sub;
                for (const auto* def : byCategory[cat])
                {
                    sub.addItem (assignItemId, def->name + "  (" + def->id + ")");
                    assignParamByItem[assignItemId++] = def->id;
                }
                assignMenu.addSubMenu (cat, sub);
            }
            menu.addSubMenu ("Assign Selected To Parameter", assignMenu);
        }
        juce::String prerequisiteId;
        float prerequisiteValue = 1.0f;
        if (auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId()))
        {
            if (auto* parameter = owner.getProject().getParameters().find (selected->parameterId))
            {
                if (parameter->enabledBy.isNotEmpty()
                    && ! canvasParameterIsEnabled (owner.getProject(), *parameter))
                {
                    if (auto* prerequisite = owner.getProject().getParameters().find (parameter->enabledBy))
                    {
                        prerequisiteId = prerequisite->id;
                        prerequisiteValue = prerequisite->displayMode == "toggle"
                            ? prerequisite->max
                            : juce::jmax (prerequisite->defaultValue,
                                          prerequisite->min + (prerequisite->max - prerequisite->min) * 0.5f);
                        menu.addItem (10, "Enable Prerequisite: "
                            + (prerequisite->name.isNotEmpty() ? prerequisite->name : prerequisite->id), true);
                    }
                }
            }
        }
        menu.addSeparator();
        bool canDetachLabels = false;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* selected = owner.getProject().getLayout().find (id))
                if (isRuntimeControlElement (selected->type)
                    && selected->labelPosition != "hidden"
                    && (selected->label.isNotEmpty() || selected->parameterId.isNotEmpty()))
                {
                    canDetachLabels = true;
                    break;
        }
        menu.addItem (15, "Detach Labels From Selection", canDetachLabels);
        menu.addItem (16, "Copy Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (17, "Copy Selection Without Parameters", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (18, "Paste Elements", owner.hasCopiedElements());
        menu.addItem (19, "Copy Selection To All Tabs", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (4, "Create Group From Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addSeparator();
        juce::PopupMenu animationMenu;
        animationMenu.addItem (401, "None");
        animationMenu.addItem (402, "Breathe");
        animationMenu.addItem (403, "Pulse");
        animationMenu.addItem (404, "Glow");
        animationMenu.addItem (405, "Shake");
        animationMenu.addItem (406, "Audio Reactive Glow");
        menu.addSubMenu ("Assign Visual Automation", animationMenu, ! owner.getSelectedElementIds().isEmpty());
        menu.addSeparator();
        juce::PopupMenu alignMenu;
        alignMenu.addItem (201, "Left", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (202, "Horizontal Center", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (203, "Right", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addSeparator();
        alignMenu.addItem (204, "Top", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (205, "Vertical Center", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (206, "Bottom", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addSeparator();
        alignMenu.addItem (207, "Distribute Horizontally", owner.getSelectedElementIds().size() >= 3);
        alignMenu.addItem (208, "Distribute Vertically", owner.getSelectedElementIds().size() >= 3);
        menu.addSubMenu ("Align / Distribute", alignMenu);
        juce::PopupMenu orderMenu;
        orderMenu.addItem (301, "Bring to Front", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (302, "Bring Forward", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (303, "Send Backward", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (304, "Send to Back", ! owner.getSelectedElementIds().isEmpty());
        menu.addSubMenu ("Arrange Order", orderMenu);

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this, canvasPos, screenPos, paramByItem, assignParamByItem, prerequisiteId, prerequisiteValue, directAssignMode] (int result)
            {
                if (result == 1) addElementAt (ElementType::Panel, canvasPos);
                else if (result == 2) addElementAt (ElementType::Group, canvasPos);
                else if (result == 3) addElementAt (ElementType::TabPanel, canvasPos);
                else if (result == 99)
                {
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchCanvasActionPicker (this, anchor,
                        [safe, canvasPos, screenPos] (const juce::String& actionId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr) return;

                            if (actionId == "findParameter")
                            {
                                auto entries = collectParameterEntries (c->owner.getProject());
                                const juce::Rectangle<int> parameterAnchor (screenPos.x, screenPos.y, 1, 1);
                                launchParameterPicker (c, parameterAnchor, std::move (entries),
                                    [safe, canvasPos] (const juce::String& paramId)
                                    {
                                        if (auto* canvas = safe.getComponent())
                                            canvas->addElementAt (ElementType::Knob, canvasPos, paramId);
                                    });
                            }
                            else if (actionId == "panel") c->addElementAt (ElementType::Panel, canvasPos);
                            else if (actionId == "tabPanel") c->addElementAt (ElementType::TabPanel, canvasPos);
                            else if (actionId == "roundedRect") c->addElementAt (ElementType::Shape, canvasPos);
                            else if (actionId == "ellipse")
                            {
                                c->addElementAt (ElementType::Shape, canvasPos);
                                const auto id = c->owner.getSelectedElementId();
                                c->owner.getProject().performLayoutEdit ("Set shape kind",
                                    [id] (LayoutModel& m)
                                    {
                                        if (auto* el = m.find (id))
                                            el->shapeKind = "ellipse";
                                    });
                            }
                            else if (actionId == "circleSeqBg") c->addCircleSeqBackgroundKit (canvasPos);
                            else if (actionId == "mixer") c->addElementAt (ElementType::Mixer, canvasPos);
                            else if (actionId == "mixerChannel") c->addMixerChannelAt (canvasPos);
                            else if (actionId == "explodeMixer") c->explodeSelectedMixers();
                            else if (actionId == "macro") c->addElementAt (ElementType::MacroControl, canvasPos);
                            else if (actionId == "modMatrix") c->addElementAt (ElementType::ModMatrix, canvasPos);
                            else if (actionId == "granular") c->addElementAt (ElementType::GranularField, canvasPos);
                            else if (actionId == "arpLane") c->addElementAt (ElementType::ArpLane, canvasPos);
                            else if (actionId == "drumMachine") c->addDrumMachineControlLayout (canvasPos);
                            else if (actionId == "bpm") c->addElementAt (ElementType::Knob, canvasPos, "projectBpm");
                            else if (actionId == "bpmSync") c->addElementAt (ElementType::Toggle, canvasPos, "bpmSync");
                            else if (actionId == "retrigger") c->addElementAt (ElementType::Toggle, canvasPos, "retrigger");
                            else if (actionId == "copy") c->owner.copySelectedElements (true);
                            else if (actionId == "copyNoParams") c->owner.copySelectedElements (false);
                            else if (actionId == "paste") c->owner.pasteCopiedElements();
                            else if (actionId == "copyTabs") c->owner.copySelectedToAllTabs();
                            else if (actionId == "group") c->owner.groupSelectedElements();
                        });
                }
                else if (result == 11)
                {
                    // Search-driven parameter picker: on a selected control it
                    // assigns; on empty canvas it creates a connected knob.
                    // filter instead of cascading category submenus.
                    auto entries = collectParameterEntries (owner.getProject());
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchParameterPicker (this, anchor, std::move (entries),
                        [safe, canvasPos, directAssignMode] (const juce::String& paramId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr)
                                return;

                            if (! directAssignMode)
                            {
                                c->addElementAt (ElementType::Knob, canvasPos, paramId);
                                return;
                            }

                            const auto ids = c->owner.getSelectedElementIds();
                            const auto* def = c->owner.getProject().getParameters().find (paramId);
                            const auto label = def != nullptr ? def->name : juce::String();
                            c->owner.getProject().performLayoutEdit ("Assign parameter",
                                [ids, paramId, label] (LayoutModel& m)
                                {
                                    for (const auto& id : ids)
                                        if (auto* el = m.find (id))
                                        {
                                            const bool assignable = el->type == ElementType::Knob
                                                || el->type == ElementType::Slider
                                                || el->type == ElementType::Button
                                                || el->type == ElementType::Toggle
                                                || el->type == ElementType::Dropdown
                                                || el->type == ElementType::ValueDisplay
                                                || el->type == ElementType::MacroControl;
                                            if (! assignable)
                                                continue;
                                            el->parameterId = paramId;
                                            if (el->label.isEmpty() && label.isNotEmpty())
                                                el->label = label;
                                        }
                                });
                            c->repaint();
                        });
                }
                else if (result == 12)
                {
                    // One-click: switch to the DSP builder's MIDI tab and
                    // drop in an arpeggiator block ready for editing.
                    owner.addArpBlock();
                }
                else if (result == 14)
                {
                    addElementAt (ElementType::Mixer, canvasPos);
                }
                else if (result == 24)
                {
                    addMixerChannelAt (canvasPos);
                }
                else if (result == 25)
                {
                    explodeSelectedMixers();
                }
                else if (result == 20)
                {
                    addElementAt (ElementType::MacroControl, canvasPos);
                }
                else if (result == 21)
                {
                    addElementAt (ElementType::ModMatrix, canvasPos);
                }
                else if (result == 22)
                {
                    addElementAt (ElementType::GranularField, canvasPos);
                }
                else if (result == 23)
                {
                    addDrumMachineControlLayout (canvasPos);
                }
                else if (result == 26)
                {
                    addElementAt (ElementType::ArpLane, canvasPos);
                }
                else if (result == 15)
                {
                    owner.detachLabelsFromSelectedControls();
                }
                else if (result == 16)
                {
                    owner.copySelectedElements (true);
                }
                else if (result == 17)
                {
                    owner.copySelectedElements (false);
                }
                else if (result == 18)
                {
                    owner.pasteCopiedElements();
                }
                else if (result == 19)
                {
                    owner.copySelectedToAllTabs();
                }
                else if (result == 13)
                {
                    // Searchable "Assign Selected To Parameter".
                    auto entries = collectParameterEntries (owner.getProject());
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchParameterPicker (this, anchor, std::move (entries),
                        [safe] (const juce::String& paramId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr) return;
                            const auto ids = c->owner.getSelectedElementIds();
                            const auto label = c->owner.getProject().getParameters().find (paramId) != nullptr
                                ? c->owner.getProject().getParameters().find (paramId)->name
                                : juce::String();
                            c->owner.getProject().performLayoutEdit ("Assign parameter",
                                [ids, paramId, label] (LayoutModel& m)
                                {
                                    for (const auto& id : ids)
                                    {
                                        if (auto* el = m.find (id))
                                        {
                                            const bool assignable = el->type == ElementType::Knob
                                                || el->type == ElementType::Slider
                                                || el->type == ElementType::Button
                                                || el->type == ElementType::Toggle
                                                || el->type == ElementType::Dropdown
                                                || el->type == ElementType::ValueDisplay
                                                || el->type == ElementType::MacroControl;
                                            if (! assignable) continue;
                                            el->parameterId = paramId;
                                            if (el->label.isEmpty() && label.isNotEmpty())
                                                el->label = label;
                                        }
                                    }
                                });
                            c->repaint();
                        });
                }
                else if (result >= 5 && result <= 9)
                {
                    addElementAt (ElementType::Shape, canvasPos);
                    const auto id = owner.getSelectedElementId();
                    juce::String shapeKind;
                    if (result == 6) shapeKind = "ellipse";
                    if (result == 7) shapeKind = "triangle";
                    if (result == 8) shapeKind = "diamond";
                    if (result == 9) shapeKind = "line";
                    if (shapeKind.isNotEmpty())
                        owner.getProject().performLayoutEdit ("Set shape kind",
                            [id, shapeKind] (LayoutModel& m)
                            {
                                if (auto* el = m.find (id))
                                    el->shapeKind = shapeKind;
                            });
                }
                else if (result == 4)
                {
                    owner.groupSelectedElements();
                }
                else if (result == 10 && prerequisiteId.isNotEmpty())
                {
                    owner.getProject().getLiveValues().setValue (prerequisiteId, prerequisiteValue);
                    applyArpLaneParameterToGraph (owner.getProject(), prerequisiteId);
                    owner.getProject().notifyChanged();
                    repaint();
                }
                else if (result >= 401 && result <= 406)
                {
                    const auto ids = owner.getSelectedElementIds();
                    owner.getProject().performLayoutEdit ("Assign visual automation",
                        [ids, result] (LayoutModel& m)
                        {
                            for (const auto& id : ids)
                            {
                                if (auto* el = m.find (id))
                                {
                                    el->animationRate = result == 405 ? 2.0f
                                                       : result == 402 ? 0.55f
                                                       : 1.0f;
                                    el->audioReactive = result == 406;
                                    el->audioReactiveMode = "level";
                                    el->audioReactiveAmount = result == 406 ? 0.85f : 0.35f;
                                    el->animationMode = result == 401 ? "none"
                                                      : result == 402 ? "breathe"
                                                      : result == 403 ? "pulse"
                                                      : result == 404 ? "glow"
                                                      : result == 405 ? "shake"
                                                      : "glow";
                                }
                            }
                        });
                    repaint();
                }
                else if (result == 201) owner.alignSelected ("left");
                else if (result == 202) owner.alignSelected ("hcenter");
                else if (result == 203) owner.alignSelected ("right");
                else if (result == 204) owner.alignSelected ("top");
                else if (result == 205) owner.alignSelected ("vcenter");
                else if (result == 206) owner.alignSelected ("bottom");
                else if (result == 207) owner.distributeSelected (true);
                else if (result == 208) owner.distributeSelected (false);
                else if (result == 301) owner.orderSelected ("front");
                else if (result == 302) owner.orderSelected ("forward");
                else if (result == 303) owner.orderSelected ("backward");
                else if (result == 304) owner.orderSelected ("back");
                else if (auto it = assignParamByItem.find (result); it != assignParamByItem.end())
                {
                    const auto paramId = it->second;
                    const auto ids = owner.getSelectedElementIds();
                    const auto* def = owner.getProject().getParameters().find (paramId);
                    const auto label = def != nullptr ? def->name : juce::String();
                    owner.getProject().performLayoutEdit ("Assign parameter",
                        [ids, paramId, label] (LayoutModel& m)
                        {
                            for (const auto& id : ids)
                            {
                                if (auto* el = m.find (id))
                                {
                                    const bool assignable = el->type == ElementType::Knob
                                        || el->type == ElementType::Slider
                                        || el->type == ElementType::Button
                                        || el->type == ElementType::Toggle
                                        || el->type == ElementType::Dropdown
                                        || el->type == ElementType::ValueDisplay
                                        || el->type == ElementType::MacroControl;
                                    if (! assignable)
                                        continue;
                                    el->parameterId = paramId;
                                    if (el->label.isEmpty() && label.isNotEmpty())
                                        el->label = label;
                                }
                            }
                        });
                    repaint();
                }
                else if (auto it = paramByItem.find (result); it != paramByItem.end())
                    addElementAt (ElementType::Knob, canvasPos, it->second);
            });
    }

    void CanvasEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (mode == DragMode::None) return;

        auto snap = [this] (int v) -> int
        {
            if (! snapEnabled || snapGrid <= 1) return v;
            return juce::roundToInt ((float) v / snapGrid) * snapGrid;
        };

        if (mode == DragMode::ValueDrag)
        {
            auto* def = owner.getProject().getParameters().find (dragParameterId);
            if (def == nullptr) return;
            const int dyPixels = dragStart.y - e.getPosition().y;     // up = increase
            const float fineMult = e.mods.isShiftDown() ? 0.2f : 1.0f;
            const float pixelsPerFullRange = 220.0f;
            const float deltaNorm = (float) dyPixels / pixelsPerFullRange * fineMult;
            const float range = def->max - def->min;
            float v = dragValueStart + deltaNorm * range;
            v = juce::jlimit (def->min, def->max, v);
            owner.getProject().getLiveValues().setValue (dragParameterId, v);
            applyArpLaneParameterToGraph (owner.getProject(), dragParameterId);
            if (dragParameterId.startsWith ("arpLane"))
                owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
            if (auto* dragged = owner.getProject().getLayout().find (dragValueElementId))
                repaint (elementScreenRect (*dragged).expanded (8));
            else
                repaint();
            return;
        }

        if (mode == DragMode::DrumGridEdit)
        {
            if (auto* grid = owner.getProject().getLayout().find (drumGridEditElementId);
                grid != nullptr && grid->type == ElementType::DrumGrid)
                editDrumGridCellAt (*grid, elementScreenRect (*grid), e.getPosition(), e.mods, false);
            return;
        }

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());

        const auto deltaCanvas = juce::Point<float> (
            (e.getPosition().x - dragStart.x) / zoom,
            (e.getPosition().y - dragStart.y) / zoom);

        if (mode == DragMode::Move)
        {
            for (auto& kv : multiDragOrigins)
                if (auto* selected = owner.getProject().getLayout().find (kv.first); selected != nullptr && ! selected->locked)
                {
                    selected->x = snap (kv.second.x + (int) deltaCanvas.x);
                    selected->y = snap (kv.second.y + (int) deltaCanvas.y);
                    owner.propagateLinkedElementChange (selected->id);
                }
            layoutChangedDuringDrag = true;
            owner.getProject().markDirty();
        }
        else if (mode == DragMode::ResizeBR)
        {
            if (el == nullptr) return;
            const auto selectedIds = owner.getSelectedElementIds();
            if (selectedIds.size() > 1 && ! dragLayoutBefore.empty())
            {
                juce::Rectangle<int> originalBounds;
                bool hasBounds = false;
                for (const auto& original : dragLayoutBefore)
                {
                    if (! selectedIds.contains (original.id) || original.locked || original.type == ElementType::Group)
                        continue;
                    const juce::Rectangle<int> itemBounds (original.x, original.y,
                                                           juce::jmax (1, original.width),
                                                           juce::jmax (1, original.height));
                    originalBounds = hasBounds ? originalBounds.getUnion (itemBounds) : itemBounds;
                    hasBounds = true;
                }

                if (hasBounds && originalBounds.getWidth() > 0 && originalBounds.getHeight() > 0)
                {
                    const float scaleX = juce::jmax (0.05f, (float) juce::jmax (8, originalBounds.getWidth() + (int) deltaCanvas.x)
                                                           / (float) originalBounds.getWidth());
                    const float scaleY = juce::jmax (0.05f, (float) juce::jmax (8, originalBounds.getHeight() + (int) deltaCanvas.y)
                                                           / (float) originalBounds.getHeight());

                    for (const auto& original : dragLayoutBefore)
                    {
                        if (! selectedIds.contains (original.id) || original.locked || original.type == ElementType::Group)
                            continue;
                        if (auto* selected = owner.getProject().getLayout().find (original.id))
                        {
                            selected->x = snap (originalBounds.getX()
                                + juce::roundToInt ((float) (original.x - originalBounds.getX()) * scaleX));
                            selected->y = snap (originalBounds.getY()
                                + juce::roundToInt ((float) (original.y - originalBounds.getY()) * scaleY));
                            selected->width = juce::jmax (8, snap (juce::roundToInt ((float) original.width * scaleX)));
                            selected->height = juce::jmax (8, snap (juce::roundToInt ((float) original.height * scaleY)));
                            owner.propagateLinkedElementChange (selected->id);
                        }
                    }
                }
            }
            else
            {
                el->width  = juce::jmax (8, snap (dragOriginal.width  + (int) deltaCanvas.x));
                el->height = juce::jmax (8, snap (dragOriginal.height + (int) deltaCanvas.y));
                owner.propagateLinkedElementChange (el->id);
            }
            layoutChangedDuringDrag = true;
            owner.getProject().markDirty();
        }
        else if (mode == DragMode::Marquee)
        {
            marqueeRect = juce::Rectangle<int>::leftTopRightBottom (
                juce::jmin (dragStart.x, e.getPosition().x),
                juce::jmin (dragStart.y, e.getPosition().y),
                juce::jmax (dragStart.x, e.getPosition().x),
                juce::jmax (dragStart.y, e.getPosition().y));
            juce::StringArray ids;
            for (const auto& item : owner.getProject().getLayout().getAll())
                if (item.visible && ! item.locked && item.type != ElementType::Group && isElementOnCurrentTab (item)
                    && marqueeRect.intersects (elementScreenRect (item)))
                    ids.add (item.id);
            owner.setSelectedElementIds (ids);
            repaint();
            return;
        }
        repaint();
    }

    void CanvasEditor::mouseUp (const juce::MouseEvent&)
    {
        const bool wasMarquee = mode == DragMode::Marquee;
        const auto previousMarquee = marqueeRect;
        const bool shouldNotify = layoutChangedDuringDrag;
        const bool shouldCommitLayoutUndo = layoutChangedDuringDrag
                                         && ! dragLayoutBefore.empty()
                                         && (mode == DragMode::Move || mode == DragMode::ResizeBR);
        const auto afterLayout = shouldCommitLayoutUndo
            ? owner.getProject().getLayout().getAll()
            : std::vector<LayoutElement>();
        const auto beforeLayout = dragLayoutBefore;
        const auto actionName = dragActionName.isNotEmpty() ? dragActionName : juce::String ("Edit layout");
        mode = DragMode::None;
        dragParameterId.clear();
        dragValueElementId.clear();
        drumGridEditElementId.clear();
        lastDrumGridTrack = -1;
        lastDrumGridStep = -1;
        marqueeRect = {};
        multiDragOrigins.clear();
        dragLayoutBefore.clear();
        dragActionName.clear();
        layoutChangedDuringDrag = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (shouldCommitLayoutUndo)
        {
            owner.getProject().getLayout().getAll() = beforeLayout;
            owner.getProject().performLayoutEdit (actionName,
                [afterLayout] (LayoutModel& m)
                {
                    m.getAll() = afterLayout;
                });
            owner.refreshAllPanels();
        }
        else if (shouldNotify)
            owner.getProject().notifyChanged();

        if (wasMarquee)
            repaint (previousMarquee.expanded (3));
    }

    void CanvasEditor::mouseMove (const juce::MouseEvent& e)
    {
        const auto previousHover = hoverGuidanceBounds.expanded (4);
        const auto previousText = hoverGuidance;
        hoverGuidance.clear();
        hoverGuidanceBounds = {};

        if (owner.getProject().getManifest().playerShowParameterGuidance)
        {
            const auto& elements = owner.getProject().getLayout().getAll();
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type == ElementType::Group) continue;
                if (! isElementOnCurrentTab (*it)) continue;

                const bool isInteractiveControl = it->type == ElementType::Knob || it->type == ElementType::Slider
                                               || it->type == ElementType::Button || it->type == ElementType::Toggle
                                               || it->type == ElementType::Dropdown
                                               || it->type == ElementType::ValueDisplay
                                               || it->type == ElementType::MacroControl;
                if (! isInteractiveControl)
                    continue;

                auto r = elementScreenRect (*it);
                if (! r.contains (e.getPosition()))
                    continue;

                const bool overControl = (it->type == ElementType::Knob || it->type == ElementType::Slider
                                       || it->type == ElementType::MacroControl)
                    ? hitTestControlBody (*it, r, e.getPosition())
                    : true;
                if (! overControl)
                    break;

                hoverGuidance = canvasControlGuidance (owner.getProject(), *it);
                if (hoverGuidance.isNotEmpty())
                {
                    constexpr int tipW = 330;
                    constexpr int tipH = 76;
                    int x = e.x + 16;
                    int y = e.y + 18;
                    if (x + tipW > getWidth() - 6)
                        x = e.x - tipW - 16;
                    if (y + tipH > getHeight() - 6)
                        y = e.y - tipH - 14;
                    hoverGuidanceBounds = { juce::jmax (6, x), juce::jmax (6, y), tipW, tipH };
                }
                break;
            }
        }

        if (previousText != hoverGuidance || previousHover != hoverGuidanceBounds.expanded (4))
        {
            if (! previousHover.isEmpty())
                repaint (previousHover);
            if (! hoverGuidanceBounds.isEmpty())
                repaint (hoverGuidanceBounds.expanded (4));
        }

        // BR-corner resize cursor on selected element
        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            const int hs = 8;
            if (juce::Rectangle<int> (r.getRight() - hs, r.getBottom() - hs, hs * 2, hs * 2)
                .contains (e.getPosition()))
            {
                setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
                return;
            }
        }

        if (sel != nullptr && sel->type == ElementType::DrumGrid && owner.isElementSelected (sel->id)
            && (e.mods.isAltDown() || e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
        {
            int pattern = 0, track = 0, step = 0;
            float velocity = 0.0f;
            if (drumCellAt (*sel, elementScreenRect (*sel), e.getPosition(), pattern, track, step, velocity))
            {
                setMouseCursor (juce::MouseCursor::CrosshairCursor);
                return;
            }
        }

        // Up/down cursor over a parameter-bound control body
        const auto& elements = owner.getProject().getLayout().getAll();
        for (auto it = elements.rbegin(); it != elements.rend(); ++it)
        {
            if (! it->visible) continue;
            if (it->type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementScreenRect (*it);
            if (! r.contains (e.getPosition())) continue;
            if (hitTestControlBody (*it, r, e.getPosition()))
            {
                setMouseCursor (canvasControlGuidance (owner.getProject(), *it).isEmpty()
                    ? juce::MouseCursor::UpDownResizeCursor
                    : juce::MouseCursor::NormalCursor);
                return;
            }
            break;
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void CanvasEditor::mouseExit (const juce::MouseEvent&)
    {
        if (hoverGuidance.isEmpty() && hoverGuidanceBounds.isEmpty())
            return;

        const auto previousHover = hoverGuidanceBounds.expanded (4);
        hoverGuidance.clear();
        hoverGuidanceBounds = {};
        repaint (previousHover);
    }

    void CanvasEditor::selectionChanged()
    {
        repaint();
    }

    // -------------------------------------------------------------------------
    // Tab panel rendering + interaction
    // -------------------------------------------------------------------------
    int CanvasEditor::hitTabIndex (const LayoutElement& tabPanel,
                                   juce::Rectangle<int> r,
                                   juce::Point<int> pos) const
    {
        if (! r.contains (pos)) return -1;
        const int n = tabPanel.tabs.size();
        if (n <= 0) return -1;
        const float tabW = (float) r.getWidth() / (float) n;
        const int idx = juce::jlimit (0, n - 1,
                                      (int) ((pos.x - r.getX()) / tabW));
        return idx;
    }

    void CanvasEditor::drawTabPanel (juce::Graphics& g, const LayoutElement& e,
                                     juce::Rectangle<int> r, bool selected) const
    {
        const int n = juce::jmax (1, e.tabs.size());
        const float tabW = (float) r.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            const auto label = i < e.tabs.size() ? e.tabs[i] : juce::String ("Tab");
            const auto groupId = scopedTabGroupId (e, label);
            const auto found = activeTabGroupsByPanel.find (e.id);
            const auto activeGroup = found != activeTabGroupsByPanel.end()
                ? found->second
                : (e.id == "tabs" ? currentTabGroup
                                   : (e.tabs.isEmpty() ? juce::String() : scopedTabGroupId (e, e.tabs[0])));
            const bool active = (groupId == activeGroup);

            const float x = r.getX() + i * tabW;
            juce::Rectangle<float> tabRect (x, (float) r.getY(), tabW, (float) r.getHeight());

            // Active = bright accent; inactive = mid-tone (clearly readable, not
            // textDim() which the user reported as too dark).
            g.setColour (active ? PatchCraftLookAndFeel::textBright()
                                : juce::Colour (0xffb8bcc4));
            g.setFont (juce::Font (juce::jmax (10.0f, tabRect.getHeight() * 0.42f),
                                   juce::Font::bold));
            g.drawText (label.toUpperCase(), tabRect.toNearestInt(),
                        juce::Justification::centred);

            if (active)
            {
                g.setColour (PatchCraftLookAndFeel::accent());
                g.fillRect (tabRect.removeFromBottom (2.0f).toNearestInt());
            }
        }

        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (r.expanded (1), 1);
        }
    }

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------
    bool CanvasEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            const auto id = owner.getSelectedElementId();
            if (id.isEmpty()) return false;
            // Don't allow deleting the locked background.
            auto* el = owner.getProject().getLayout().find (id);
            if (el != nullptr && el->locked) return true;
            owner.deleteSelected();
            return true;
        }
        if (key.getKeyCode() == 'D' && key.getModifiers().isCommandDown())
        {
            owner.duplicateSelected();
            return true;
        }
        if (key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown()
            && ! key.getModifiers().isShiftDown())
        {
            owner.undo();
            return true;
        }
        if ((key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown()
             && key.getModifiers().isShiftDown())
            || (key.getKeyCode() == 'Y' && key.getModifiers().isCommandDown()))
        {
            owner.redo();
            return true;
        }
        // Arrow keys nudge selected element by 1px (or 10px with shift).
        if (key.isKeyCode (juce::KeyPress::leftKey)
            || key.isKeyCode (juce::KeyPress::rightKey)
            || key.isKeyCode (juce::KeyPress::upKey)
            || key.isKeyCode (juce::KeyPress::downKey))
        {
            const int step = key.getModifiers().isShiftDown() ? 10 : 1;
            const auto ids = owner.getSelectedElementIds();
            bool hasMovable = false;
            for (const auto& id : ids)
                if (auto* el = owner.getProject().getLayout().find (id); el != nullptr && ! el->locked)
                {
                    hasMovable = true;
                    break;
                }
            if (! hasMovable) return false;

            const bool left = key.isKeyCode (juce::KeyPress::leftKey);
            const bool right = key.isKeyCode (juce::KeyPress::rightKey);
            const bool up = key.isKeyCode (juce::KeyPress::upKey);
            const bool down = key.isKeyCode (juce::KeyPress::downKey);
            owner.getProject().performLayoutEdit ("Nudge selection",
                [ids, step, left, right, up, down] (LayoutModel& m)
                {
                    for (const auto& id : ids)
                        if (auto* el = m.find (id); el != nullptr && ! el->locked)
                        {
                            if (left)  el->x -= step;
                            if (right) el->x += step;
                            if (up)    el->y -= step;
                            if (down)  el->y += step;
                        }
                });
            repaint();
            return true;
        }
        return false;
    }

} // namespace patchcraft
