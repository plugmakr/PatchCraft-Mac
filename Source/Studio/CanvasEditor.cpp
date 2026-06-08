#include "CanvasEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "SampleMap.h"

#include <array>
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

        static bool isSupportedSampleFile (const juce::File& file)
        {
            const auto ext = file.getFileExtension().toLowerCase();
            return file.existsAsFile()
                && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac");
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
            block.values["mpMultiLane"] = 1.0f;
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

        static bool isArpLanePatternKey (const juce::String& key)
        {
            return key == "arpSteps"
                || key == "arpPattern"
                || key == "arpGate"
                || key == "arpSwing"
                || key == "rate"
                || key == "mpProbability"
                || key == "mpRatchet"
                || key == "mpEuclideanPulses"
                || key == "mpEuclideanRotate"
                || key == "mpSampleControl"
                || key == "mpSampleSliceCount"
                || key == "mpLaneFxTarget"
                || key.startsWith ("mpStep")
                || key.startsWith ("arpNote")
                || key.startsWith ("mpVelocity")
                || key.startsWith ("mpGate")
                || key.startsWith ("mpStepProb")
                || key.startsWith ("mpStepDiv")
                || key.startsWith ("mpStepDelay")
                || key.startsWith ("mpStepTranspose")
                || key.startsWith ("mpSampleSlice")
                || key.startsWith ("mpAutoFilter")
                || key.startsWith ("mpAutoPan")
                || key.startsWith ("mpAutoFxSend");
        }

        static float normalisedArpLaneSliderValue (const DspBlock& block, int lane, int step, int role, int slots)
        {
            const auto suffix = juce::String (step);
            switch (juce::jlimit (0, 10, role))
            {
                case 0:  return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpVelocity" + suffix, 0.72f));
                case 1:  return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpGate" + suffix, 0.58f));
                case 2:  return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpStepProb" + suffix, 1.0f));
                case 3:  return juce::jlimit (0.0f, 1.0f, (arpLaneValue (block, lane, "mpStepDiv" + suffix, 1.0f) - 1.0f) / 7.0f);
                case 4:  return arpLaneValue (block, lane, "mpStep" + suffix + "On", 0.0f) >= 0.5f ? 1.0f : 0.0f;
                case 5:  return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpStepDelay" + suffix, 0.0f) / 0.85f);
                case 6:  return slots > 1 ? juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpSampleSlice" + suffix, 0.0f) / (float) (slots - 1)) : 0.0f;
                case 7:  return juce::jlimit (0.0f, 1.0f, (arpLaneValue (block, lane, "mpStepTranspose" + suffix, 0.0f) + 24.0f) / 48.0f);
                case 8:  return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpAutoFilter" + suffix, 0.0f));
                case 9:  return juce::jlimit (0.0f, 1.0f, (arpLaneValue (block, lane, "mpAutoPan" + suffix, 0.0f) + 1.0f) * 0.5f);
                default: return juce::jlimit (0.0f, 1.0f, arpLaneValue (block, lane, "mpAutoFxSend" + suffix, 0.0f));
            }
        }

        static void initialiseArpLaneBank (DspBlock& block, int lane, int steps)
        {
            steps = juce::jlimit (1, 128, steps);
            block.values["mpMultiLane"] = 1.0f;
            if (block.values.find ("mpActiveBank") == block.values.end())
                block.values["mpActiveBank"] = (float) lane;

            setArpLaneValue (block, lane, "arpSteps", (float) steps);
            setArpLaneValue (block, lane, "arpPattern", 0.0f);
            setArpLaneValue (block, lane, "arpGate", 0.58f);
            setArpLaneValue (block, lane, "arpSwing", 0.0f);
            setArpLaneValue (block, lane, "rate", 1.0f);
            setArpLaneValue (block, lane, "mpProbability", 1.0f);
            setArpLaneValue (block, lane, "mpRatchet", 1.0f);
            setArpLaneValue (block, lane, "mpEuclideanPulses", 0.0f);
            setArpLaneValue (block, lane, "mpEuclideanRotate", 0.0f);
            setArpLaneValue (block, lane, "mpSampleControl", 0.0f);
            setArpLaneValue (block, lane, "mpSampleSliceCount", 16.0f);
            setArpLaneValue (block, lane, "mpLaneMute", 0.0f);
            setArpLaneValue (block, lane, "mpLaneSolo", 0.0f);
            setArpLaneValue (block, lane, "mpLaneRetrigger", 1.0f);
            setArpLaneValue (block, lane, "mpLaneFxTarget", (float) (lane % 4));

            for (int step = 0; step < 128; ++step)
            {
                const auto suffix = juce::String (step);
                setArpLaneValue (block, lane, "mpStep" + suffix + "On", step < steps && step % 4 == 0 ? 1.0f : 0.0f);
                setArpLaneValue (block, lane, "arpNote" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpVelocity" + suffix, step % 4 == 0 ? 0.82f : 0.30f);
                setArpLaneValue (block, lane, "mpGate" + suffix, 0.58f);
                setArpLaneValue (block, lane, "mpStepProb" + suffix, 1.0f);
                setArpLaneValue (block, lane, "mpStepDiv" + suffix, 1.0f);
                setArpLaneValue (block, lane, "mpStepDelay" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpStepTranspose" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpSampleSlice" + suffix, (float) (step % 16));
                setArpLaneValue (block, lane, "mpAutoFilter" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpAutoPan" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, 0.0f);
            }
        }

        static void setArpLaneMetadata (DspBlock& block, int lane, const juce::String& key, const juce::String& newValue)
        {
            block.metadata["arpLane" + juce::String (lane + 1) + key] = newValue;
        }

        static juce::String orbitLaneSoundName (int sound);

        static juce::String circleSeqPatternName (int preset)
        {
            static const char* names[] =
            {
                "Pentatonic Pulse", "Bass Anchor", "Melody Answer", "Bell Topline",
                "Soft Syncopation", "Arp Climb", "Open Fifths", "Reset Empty"
            };
            return names[(size_t) juce::jlimit (0, 7, preset)];
        }

        static void writeCircleSeqMusicalPreset (DspBlock& block, int lane, int preset)
        {
            lane = juce::jlimit (0, 15, lane);
            preset = juce::jlimit (0, 7, preset);

            struct PatternSeed
            {
                float pulses = 4.0f;
                float gate = 0.58f;
                std::array<int, 16> active {};
                std::array<float, 16> notes {};
                std::array<float, 16> velocity {};
            };

            const std::array<PatternSeed, 8> patterns =
            {{
                { 4.0f, 0.68f,
                  {{ 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 }},
                  {{ -12,-12,-12,-12, -10,-10,-10,-10, -5,-5,-5,-5, -10,-10,-10,-10 }},
                  {{ 0.92f,0.30f,0.30f,0.30f, 0.82f,0.30f,0.30f,0.30f, 0.88f,0.30f,0.30f,0.30f, 0.78f,0.30f,0.30f,0.30f }} },
                { 5.0f, 0.72f,
                  {{ 1,0,0,1, 0,0,1,0, 1,0,0,0, 0,1,0,0 }},
                  {{ -24,-24,-19,-17, -12,-12,-10,-7, -12,-12,-10,-7, -5,-7,-10,-12 }},
                  {{ 0.94f,0.28f,0.28f,0.70f, 0.28f,0.28f,0.76f,0.28f, 0.86f,0.28f,0.28f,0.28f, 0.28f,0.72f,0.28f,0.28f }} },
                { 6.0f, 0.46f,
                  {{ 1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1 }},
                  {{ 0,2,4,7, 9,7,4,2, 0,2,7,9, 12,9,7,4 }},
                  {{ 0.56f,0.34f,0.66f,0.34f, 0.30f,0.70f,0.34f,0.62f, 0.58f,0.34f,0.68f,0.34f, 0.30f,0.72f,0.34f,0.66f }} },
                { 3.0f, 0.38f,
                  {{ 0,0,0,1, 0,0,1,0, 0,0,1,0, 0,1,0,0 }},
                  {{ 12,12,16,19, 21,19,16,12, 16,19,21,24, 21,19,16,12 }},
                  {{ 0.24f,0.24f,0.24f,0.76f, 0.24f,0.24f,0.68f,0.24f, 0.24f,0.24f,0.72f,0.24f, 0.24f,0.66f,0.24f,0.24f }} },
                { 7.0f, 0.52f,
                  {{ 1,0,0,1, 0,1,0,0, 1,0,1,0, 0,1,0,1 }},
                  {{ 0,2,4,7, 9,7,4,2, 4,7,9,12, 9,7,4,2 }},
                  {{ 0.70f,0.30f,0.30f,0.62f, 0.30f,0.76f,0.30f,0.30f, 0.68f,0.30f,0.72f,0.30f, 0.30f,0.66f,0.30f,0.60f }} },
                { 8.0f, 0.34f,
                  {{ 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 }},
                  {{ 0,2,4,7, 9,12,9,7, 4,7,9,12, 16,12,9,7 }},
                  {{ 0.52f,0.38f,0.56f,0.42f, 0.62f,0.46f,0.58f,0.40f, 0.54f,0.42f,0.60f,0.46f, 0.66f,0.48f,0.60f,0.44f }} },
                { 4.0f, 0.64f,
                  {{ 1,0,0,0, 0,0,1,0, 1,0,0,0, 0,1,0,0 }},
                  {{ -12,-12,-10,-10, -7,-7,-5,-5, 0,0,2,2, 7,7,9,9 }},
                  {{ 0.86f,0.28f,0.28f,0.28f, 0.28f,0.28f,0.70f,0.28f, 0.78f,0.28f,0.28f,0.28f, 0.28f,0.66f,0.28f,0.28f }} },
                { 0.0f, 0.58f,
                  {{ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }},
                  {{ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }},
                  {{ 0.30f,0.30f,0.30f,0.30f, 0.30f,0.30f,0.30f,0.30f, 0.30f,0.30f,0.30f,0.30f, 0.30f,0.30f,0.30f,0.30f }} }
            }};

            const auto& seed = patterns[(size_t) preset];
            setArpLaneValue (block, lane, "arpSteps", 16.0f);
            setArpLaneValue (block, lane, "arpGate", seed.gate);
            setArpLaneValue (block, lane, "mpEuclideanPulses", seed.pulses);
            setArpLaneValue (block, lane, "mpSampleControl", 0.0f);
            setArpLaneValue (block, lane, "mpSampleSliceCount", 16.0f);
            for (int step = 0; step < 16; ++step)
            {
                const auto suffix = juce::String (step);
                setArpLaneValue (block, lane, "mpStep" + suffix + "On", seed.active[(size_t) step] != 0 ? 1.0f : 0.0f);
                setArpLaneValue (block, lane, "arpNote" + suffix, seed.notes[(size_t) step]);
                setArpLaneValue (block, lane, "mpVelocity" + suffix, seed.velocity[(size_t) step]);
                setArpLaneValue (block, lane, "mpGate" + suffix, seed.gate);
                setArpLaneValue (block, lane, "mpStepProb" + suffix, 1.0f);
                setArpLaneValue (block, lane, "mpStepDiv" + suffix, 1.0f);
                setArpLaneValue (block, lane, "mpStepDelay" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpStepTranspose" + suffix, 0.0f);
                setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, lane >= 2 && seed.active[(size_t) step] != 0 ? 0.16f : 0.0f);
            }
            setArpLaneMetadata (block, lane, "Target", "notes");
            setArpLaneMetadata (block, lane, "Preset", circleSeqPatternName (preset));
            setArpLaneMetadata (block, lane, "SliderRole", "velocity");
        }

        static bool hasSeededOrbitLaneData (const DspBlock& block)
        {
            for (const auto& value : block.values)
                if (value.first.startsWith ("mpBank2_")
                    || value.first.startsWith ("mpBank3_")
                    || value.first.startsWith ("mpBank4_")
                    || value.first.startsWith ("mpBank5_"))
                    return true;

            return block.metadata.find ("arpLane1SoundName") != block.metadata.end();
        }

        static void seedMusicalOrbitLaneData (DspBlock& block)
        {
            if (hasSeededOrbitLaneData (block))
                return;

            struct LaneSeed
            {
                int sound = 0;
                float pulses = 0.0f;
                std::array<int, 16> active {};
                std::array<float, 16> notes {};
                std::array<float, 16> velocities {};
                float gate = 0.58f;
            };

            const std::array<LaneSeed, 5> lanes =
            {{
                { 12, 4.0f,
                  {{ 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 }},
                  {{ -12,-12,-12,-12, -10,-10,-10,-10, -7,-7,-7,-7, -10,-10,-10,-10 }},
                  {{ 0.92f,0.34f,0.32f,0.38f, 0.84f,0.34f,0.32f,0.38f, 0.88f,0.34f,0.32f,0.38f, 0.82f,0.34f,0.32f,0.38f }},
                  0.72f },
                { 13, 6.0f,
                  {{ 1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1 }},
                  {{ 0,3,5,7, 10,7,5,3, 0,3,7,10, 12,10,7,5 }},
                  {{ 0.58f,0.38f,0.66f,0.42f, 0.36f,0.70f,0.40f,0.62f, 0.58f,0.38f,0.68f,0.42f, 0.36f,0.72f,0.40f,0.66f }},
                  0.46f },
                { 14, 8.0f,
                  {{ 1,1,0,1, 1,0,1,0, 1,1,0,1, 0,1,0,1 }},
                  {{ 12,10,7,10, 12,15,14,10, 12,10,7,5, 7,10,12,15 }},
                  {{ 0.50f,0.42f,0.30f,0.48f, 0.54f,0.34f,0.46f,0.32f, 0.52f,0.42f,0.30f,0.50f, 0.34f,0.48f,0.38f,0.58f }},
                  0.32f },
                { 15, 5.0f,
                  {{ 0,0,0,1, 0,0,1,0, 0,0,1,0, 0,1,0,0 }},
                  {{ 24,22,19,15, 19,17,15,12, 15,17,19,22, 24,22,19,15 }},
                  {{ 0.24f,0.24f,0.24f,0.72f, 0.24f,0.24f,0.66f,0.24f, 0.24f,0.24f,0.70f,0.24f, 0.24f,0.64f,0.24f,0.24f }},
                  0.38f },
                { 11, 3.0f,
                  {{ 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1 }},
                  {{ 7,7,7,7, 10,10,10,10, 12,12,12,12, 15,15,15,19 }},
                  {{ 0.18f,0.18f,0.18f,0.18f, 0.78f,0.18f,0.18f,0.18f, 0.18f,0.18f,0.18f,0.18f, 0.74f,0.18f,0.18f,0.64f }},
                  0.60f }
            }};

            block.values["sync"] = 1.0f;
            block.values["rate"] = 1.0f;
            block.values["arpSteps"] = 16.0f;
            block.values["arpGate"] = 0.58f;
            block.values["arpSwing"] = 0.08f;
            block.values["mpActiveBank"] = 0.0f;
            block.values["mpMultiLane"] = 1.0f;
            block.values["mpScaleRoot"] = 9.0f;
            block.values["mpScaleType"] = 8.0f;
            block.values["mpPatternMorph"] = 0.0f;
            block.values["mpHumanize"] = 0.015f;
            block.values["mpMutation"] = 0.0f;
            block.values["mpProbability"] = 1.0f;
            block.values["mpRatchet"] = 1.0f;
            block.values["mpSampleControl"] = 0.0f;
            block.values["mpSampleSliceCount"] = 16.0f;

            for (int lane = 0; lane < (int) lanes.size(); ++lane)
            {
                const auto& seed = lanes[(size_t) lane];
                setArpLaneValue (block, lane, "arpSteps", 16.0f);
                setArpLaneValue (block, lane, "arpGate", seed.gate);
                setArpLaneValue (block, lane, "arpSwing", 0.08f);
                setArpLaneValue (block, lane, "mpProbability", 1.0f);
                setArpLaneValue (block, lane, "mpRatchet", 1.0f);
                setArpLaneValue (block, lane, "mpEuclideanPulses", seed.pulses);
                setArpLaneValue (block, lane, "mpEuclideanRotate", (float) (lane * 2));
                setArpLaneValue (block, lane, "mpSampleControl", 0.0f);
                setArpLaneValue (block, lane, "mpSampleSliceCount", 16.0f);
                setArpLaneValue (block, lane, "mpLaneMute", 0.0f);
                setArpLaneValue (block, lane, "mpLaneSolo", 0.0f);
                setArpLaneValue (block, lane, "mpLaneRetrigger", 1.0f);
                setArpLaneValue (block, lane, "mpLaneFxTarget", (float) (lane % 4));

                for (int step = 0; step < 16; ++step)
                {
                    const auto suffix = juce::String (step);
                    setArpLaneValue (block, lane, "mpStep" + suffix + "On", seed.active[(size_t) step] != 0 ? 1.0f : 0.0f);
                    setArpLaneValue (block, lane, "arpNote" + suffix, seed.notes[(size_t) step]);
                    setArpLaneValue (block, lane, "mpVelocity" + suffix, seed.velocities[(size_t) step]);
                    setArpLaneValue (block, lane, "mpGate" + suffix, seed.gate);
                    setArpLaneValue (block, lane, "mpStepProb" + suffix, 1.0f);
                    setArpLaneValue (block, lane, "mpStepDiv" + suffix, 1.0f);
                    setArpLaneValue (block, lane, "mpStepTranspose" + suffix, 0.0f);
                    setArpLaneValue (block, lane, "mpSampleSlice" + suffix, (float) seed.sound);
                    setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, lane >= 2 && seed.active[(size_t) step] != 0 ? 0.18f : 0.0f);
                }

                setArpLaneMetadata (block, lane, "Mode", "performance");
                setArpLaneMetadata (block, lane, "Target", "notes");
                setArpLaneMetadata (block, lane, "Direction", "forward");
                setArpLaneMetadata (block, lane, "Sound", juce::String (seed.sound));
                setArpLaneMetadata (block, lane, "SoundName", orbitLaneSoundName (seed.sound));
                setArpLaneMetadata (block, lane, "FxTarget", juce::String (lane % 4));
                setArpLaneMetadata (block, lane, "SliderRole", "velocity");
                writeCircleSeqMusicalPreset (block, lane, lane == 0 ? 0 : lane == 1 ? 2 : lane == 2 ? 3 : lane == 3 ? 4 : 6);
                setArpLaneMetadata (block, lane, "Sound", juce::String (seed.sound));
                setArpLaneMetadata (block, lane, "SoundName", orbitLaneSoundName (seed.sound));
            }
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

        static juce::String orbitLaneSoundName (int sound)
        {
            return "DSP Slot " + juce::String (juce::jlimit (0, 15, sound) + 1);
        }

        static int orbitLaneSoundNote (int target, int sound, int rootNote)
        {
            static const int drumNotes[] =
            {
                36, 38, 42, 46, 39, 45, 48, 49,
                51, 37, 44, 52, 53, 54, 55, 56
            };
            sound = juce::jlimit (0, 15, sound);
            rootNote = juce::jlimit (0, 127, rootNote);
            if (target == 1)
                return drumNotes[sound];
            if (target == 2)
                return juce::jlimit (0, 127, 48 + sound);
            if (target == 4)
                return juce::jlimit (0, 127, rootNote + sound - 7);
            return rootNote;
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
            const int elementLane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneIndex", 0.0f)));
            const int controlLane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneControlBank", (float) elementLane)));
            const int lane = parameterId == "arpLaneIndex" ? elementLane : controlLane;
            const int steps = juce::jlimit (1, 128, juce::roundToInt (value ("arpLaneSteps", 16.0f)));
            const int target = juce::jlimit (0, 4, juce::roundToInt (value ("arpLaneTarget", 0.0f)));
            const int direction = juce::jlimit (0, 3, juce::roundToInt (value ("arpLaneDirection", 0.0f)));
            const int sound = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneSound", (float) lane)));
            const int group = juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneGroup", (float) (lane % 5))));
            const int rootNote = juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRootNote", 60.0f)));
            const int slots = juce::jlimit (1, 64, juce::roundToInt (value ("arpLaneSampleSlots", 1.0f)));
            const auto targetName = orbitLaneTargetName (target);

            block.values["mpActiveBank"] = (float) lane;
            block.values["mpMultiLane"] = 1.0f;
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
            setArpLaneValue (block, lane, "mpSampleSliceCount", (float) juce::jmax (slots, sound + 1));
            setArpLaneValue (block, lane, "mpLaneMute", value ("arpLaneMute", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            setArpLaneValue (block, lane, "mpLaneSolo", value ("arpLaneSolo", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            setArpLaneValue (block, lane, "mpLaneRetrigger", value ("arpLaneRetrigger", 1.0f) >= 0.5f ? 1.0f : 0.0f);
            setArpLaneValue (block, lane, "mpLaneGroup", (float) group);
            setArpLaneValue (block, lane, "mpLaneFxTarget", (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4)))));
            const float laneFxAmount = juce::jlimit (0.0f, 1.0f, value ("arpLaneFxAmount", 0.0f));
            block.values["mpPatternLaunch"] = (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLanePatternLaunch", 0.0f)));
            if (parameterId == "arpLanePatternLaunch")
                writeCircleSeqMusicalPreset (block, lane, juce::roundToInt (block.values["mpPatternLaunch"]));

            setArpLaneMetadata (block, lane, "Mode", juce::roundToInt (value ("arpLaneMode", 0.0f)) == 1 ? "performance" : "bank");
            setArpLaneMetadata (block, lane, "Target", targetName);
            setArpLaneMetadata (block, lane, "Direction", direction == 1 ? "reverse" : direction == 2 ? "bounce" : direction == 3 ? "random" : "forward");
            setArpLaneMetadata (block, lane, "RootNote", juce::String (rootNote));
            setArpLaneMetadata (block, lane, "Sound", juce::String (sound));
            setArpLaneMetadata (block, lane, "SoundName", orbitLaneSoundName (sound));
            setArpLaneMetadata (block, lane, "Group", "Group " + juce::String (group + 1));
            setArpLaneMetadata (block, lane, "FxTarget", juce::String (juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4))))));
            setArpLaneMetadata (block, lane, "FillPulses", juce::String (juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneFillPulses", 0.0f)))));
            setArpLaneMetadata (block, lane, "FillProbability", juce::String (juce::jlimit (0.0f, 1.0f, value ("arpLaneFillProbability", 0.0f)), 2));
            setArpLaneMetadata (block, lane, "Retrigger", value ("arpLaneRetrigger", 1.0f) >= 0.5f ? "on" : "off");

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

            if (parameterId == "arpLanePatternLaunch")
            {
                auto& live = project.getLiveValues();
                for (int step = 0; step < 16; ++step)
                    live.setValue ("arpLaneStep" + juce::String (step + 1),
                                   arpLaneValue (block, lane, "mpVelocity" + juce::String (step), 0.5f));
            }
            else if (parameterId == "arpLaneControlBank" || parameterId == "arpLaneSliderRole")
            {
                auto& live = project.getLiveValues();
                const int sliderRole = juce::jlimit (0, 10, juce::roundToInt (value ("arpLaneSliderRole", 0.0f)));
                for (int step = 0; step < 16; ++step)
                    live.setValue ("arpLaneStep" + juce::String (step + 1),
                                   normalisedArpLaneSliderValue (block, lane, step, sliderRole, slots));
            }
            else if (parameterId.startsWith ("arpLaneStep"))
            {
                applyArpLaneSliderBankToGraph (project, block);
            }

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
            block.values["dmTriggerPadSlots"] = 1.0f;
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
                            const float dot = 3.0f + velocity * 3.2f;
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

                const auto idleDot = centre + juce::Point<float> (0.0f, -(outerRadius + band * 0.46f));
                g.setColour (accent.withAlpha (0.35f));
                g.drawEllipse (idleDot.x - 4.2f, idleDot.y - 4.2f, 8.4f, 8.4f, 1.0f);
                g.setColour (text);
                g.setFont (juce::FontOptions (22.0f).withStyle ("bold"));
                g.drawText ("ORBIT", juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y - 23, 90, 25),
                            juce::Justification::centred, true);
                g.setColour (dim);
                g.setFont (juce::FontOptions (8.5f).withStyle ("bold"));
                g.drawText ("LANE " + juce::String (activeLane + 1), juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y + 3, 90, 18),
                            juce::Justification::centred, true);
                return;
            }

            const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
            const juce::Point<float> centre ((float) area.getCentreX(),
                                             (float) area.getY() + size * 0.52f);
            const float radius = size * 0.40f;
            const float innerRadius = radius * 0.70f;
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
                                  : element.arpLaneTarget == "effects" ? "FX SENDS"
                                  : element.arpLaneTarget == "samples" ? "SAMPLES" : "NOTES";

            g.setColour (text);
            g.setFont (juce::FontOptions (26.0f));
            g.drawText (juce::String (steps), juce::Rectangle<int> ((int) centre.x - 46, (int) centre.y - 24, 92, 32),
                        juce::Justification::centred, true);
            g.setColour (dim);
            g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
            g.drawText (targetLabel, juce::Rectangle<int> ((int) centre.x - 46, (int) centre.y + 6, 92, 18),
                        juce::Justification::centred, true);
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
                make ("orbitInstrument", "Add CircleSEQ Musical Surface", "Add a five-ring CircleSEQ element with lane source, timing, fill, FX, bypass, and step-role controls.", "patterning circular drum sequencer circleseq orbit arplane fills automation lanes musical surface"),
                make ("visualKit", "Add Animation Lab Visual Kit", "Add non-Pro reactive artwork, sprite animation, procedural FX, and Pro AI visual brief elements.", "animation reactive visuals sprite ai pro imagery artwork"),
                make ("reactiveImage", "Add Reactive Image", "Add an imported artwork slot that can pulse, scale, glow, or fade from audio/MIDI/BPM.", "visual audio reactive image artwork"),
                make ("spriteAnimator", "Add Sprite Animator", "Add a sprite-sheet animation element with BPM or note-triggered frame playback.", "sprite filmstrip animation bpm"),
                make ("visualFx", "Add Visual FX Layer", "Add a procedural native JUCE particle/ring/meter visual effect layer.", "particles rings glow procedural visual effects"),
                make ("aiVisualPrompt", "Add Pro AI Visual Prompt", "Add a Pro brief card for generated banners, thumbnails, masks, and sprite assets.", "ai pro generated artwork prompt"),
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
        if (! e.mods.isShiftDown() && ! e.mods.isCtrlDown() && ! e.mods.isCommandDown())
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

        bool hasTabPanels = false;
        for (const auto& parent : owner.getProject().getLayout().getAll())
        {
            if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                && parent.id == e.groupId)
                return true;
            if (parent.type == ElementType::TabPanel)
            {
                hasTabPanels = true;
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
        if (! hasTabPanels && ! isScopedTabGroupId (e.groupId))
            return true;
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
        const int x = (getWidth()  - w) / 2 + (showRulers ? kRulerSize / 2 : 0) + canvasPanOffset.x;
        const int y = (getHeight() - h) / 2 + (showRulers ? kRulerSize / 2 : 0) + canvasPanOffset.y;
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
        canvasPanOffset = {};
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

        juce::Rectangle<int> multiBounds;
        if (getMultiSelectionScreenBounds (multiBounds))
        {
            const auto accent = PatchCraftLookAndFeel::accent();
            g.setColour (accent.withAlpha (0.95f));
            g.drawRect (multiBounds.expanded (3), 2);

            constexpr int hs = 8;
            juce::Array<juce::Rectangle<int>> handles;
            handles.add ({ multiBounds.getX() - hs, multiBounds.getY() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getCentreX() - hs, multiBounds.getY() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getRight() - hs, multiBounds.getY() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getRight() - hs, multiBounds.getCentreY() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getRight() - hs, multiBounds.getBottom() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getCentreX() - hs, multiBounds.getBottom() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getX() - hs, multiBounds.getBottom() - hs, hs * 2, hs * 2 });
            handles.add ({ multiBounds.getX() - hs, multiBounds.getCentreY() - hs, hs * 2, hs * 2 });

            for (const auto& handle : handles)
            {
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.92f));
                g.fillRect (handle);
                g.setColour (accent);
                g.drawRect (handle, 2);
            }
        }

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
                const int frames = juce::jmax (0, (int) object->getProperty ("frames"));
                const bool vertical = (bool) object->getProperty ("vertical");
                if (isSupportedSampleFile (file))
                {
                    if (const auto* zone = sampleDropZoneAt (details.localPosition))
                    {
                        juce::Array<juce::File> samples;
                        samples.add (file);
                        assignSamplesToDropZone (zone->id, samples);
                        return;
                    }
                }
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

    bool CanvasEditor::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& path : files)
            if (isSupportedSampleFile (juce::File (path)))
                return true;
        return false;
    }

    void CanvasEditor::filesDropped (const juce::StringArray& files, int x, int y)
    {
        juce::Array<juce::File> samples;
        for (const auto& path : files)
        {
            juce::File file (path);
            if (isSupportedSampleFile (file))
                samples.add (file);
        }

        if (samples.isEmpty())
            return;

        if (const auto* zone = sampleDropZoneAt ({ x, y }))
        {
            assignSamplesToDropZone (zone->id, samples);
            return;
        }
    }

    const LayoutElement* CanvasEditor::sampleDropZoneAt (juce::Point<int> localPosition) const
    {
        const auto& elements = owner.getProject().getLayout().getAll();
        for (auto it = elements.rbegin(); it != elements.rend(); ++it)
        {
            if (! it->visible || it->type != ElementType::SampleDropZone || ! isElementOnCurrentTab (*it))
                continue;
            if (elementScreenRect (*it).contains (localPosition))
                return &*it;
        }
        return nullptr;
    }

    void CanvasEditor::assignSamplesToDropZone (const juce::String& elementId, const juce::Array<juce::File>& files)
    {
        if (files.isEmpty())
            return;

        int circleSeqLane = -1;
        if (const auto* dropZone = owner.getProject().getLayout().find (elementId))
        {
            if (dropZone->semanticRole.startsWith ("circleSeqLaneSample:"))
                circleSeqLane = juce::jlimit (0, 4, dropZone->semanticRole.fromFirstOccurrenceOf (":", false, false).getIntValue());
        }

        std::vector<SampleZoneDef> zonesToAdd;
        zonesToAdd.reserve ((size_t) files.size());
        int fallbackNote = 60;
        for (const auto& file : files)
        {
            bool usedNamePitch = false;
            bool usedAudioPitch = false;
            auto zone = SampleMap::inferZoneFromFileWithAudio (file,
                                                               fallbackNote,
                                                               files.size() > 1 ? fallbackNote : 0,
                                                               files.size() > 1 ? fallbackNote : 127,
                                                               &usedNamePitch,
                                                               &usedAudioPitch);
            zone.group = elementId;
            if (circleSeqLane >= 0)
            {
                zone.rootNote = juce::jlimit (0, 127, 36 + circleSeqLane * 12 + (int) zonesToAdd.size());
                zone.lowNote = zone.rootNote;
                zone.highNote = zone.rootNote;
                zone.oneShot = true;
                zone.padIndex = circleSeqLane * 16 + (int) zonesToAdd.size();
            }
            zone.padLabel = file.getFileNameWithoutExtension();
            zonesToAdd.push_back (zone);
            if (++fallbackNote > 84)
                fallbackNote = 60;
        }

        const auto firstPath = files[0].getFullPathName();
        const auto firstName = files[0].getFileNameWithoutExtension();
        owner.getProject().performSampleMapEdit ("Drop sample on canvas",
            [zonesToAdd, circleSeqLane] (SampleMap& map)
            {
                for (const auto& zone : zonesToAdd)
                    map.add (zone);
                if (circleSeqLane < 0 && zonesToAdd.size() > 1)
                    map.autoMapByRootNotes();
            });

        if (circleSeqLane >= 0)
        {
            auto& graph = owner.getProject().getDspGraph();
            auto& block = ensureArpBlock (graph);
            const int slots = juce::jlimit (1, 64, (int) files.size());
            block.values["mpActiveBank"] = (float) circleSeqLane;
            block.values["mpMultiLane"] = 1.0f;
            setArpLaneValue (block, circleSeqLane, "mpSampleControl", 1.0f);
            setArpLaneValue (block, circleSeqLane, "mpSampleSliceCount", (float) slots);
            setArpLaneMetadata (block, circleSeqLane, "Target", "samples");
            setArpLaneMetadata (block, circleSeqLane, "SoundName", "Sample Group " + juce::String (circleSeqLane + 1));
            for (int step = 0; step < 128; ++step)
                setArpLaneValue (block, circleSeqLane, "mpSampleSlice" + juce::String (step), (float) (step % slots));

            auto& live = owner.getProject().getLiveValues();
            live.setValue ("arpLaneControlBank", (float) circleSeqLane);
            live.setValue ("arpLaneTarget", 4.0f);
            live.setValue ("arpLaneSampleSlots", (float) slots);
            live.setValue ("arpLaneSliderRole", 6.0f);
            graph.userConfigured = true;
            owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        }
        else if (owner.getProject().getEngineType() != "sample")
            owner.getProject().setEngineType ("sample");

        owner.getProject().performLayoutEdit ("Bind sample drop zone",
            [this, elementId, firstPath, firstName, circleSeqLane] (LayoutModel& layout)
            {
                auto* zone = layout.find (elementId);
                if (zone == nullptr)
                    return;

                zone->asset = firstPath;
                zone->label = firstName.isNotEmpty() ? firstName : "Dropped Sample";
                zone->parameterId = zone->parameterId.isNotEmpty() ? zone->parameterId : "sampleStart";
                if (circleSeqLane < 0)
                    zone->semanticRole = "sampleDropZone:" + zone->id;

                const auto role = zone->semanticRole;
                if (circleSeqLane >= 0)
                    return;

                bool hasLinkedControls = false;
                for (const auto& element : layout.getAll())
                    if (element.id != zone->id && element.semanticRole == role)
                    {
                        hasLinkedControls = true;
                        break;
                    }

                if (hasLinkedControls)
                    return;

                const auto controlGroupId = zone->groupId;
                const auto controlContainerId = zone->containerId;
                const auto controlAccent = zone->accentColour;
                const int baseX = zone->x;
                const int baseY = zone->y + zone->height + 16;

                auto addLinkedControl = [&] (ElementType type, const juce::String& parameterId,
                                             const juce::String& label, int x, int y, int w, int h)
                {
                    if (owner.getProject().getParameters().find (parameterId) == nullptr)
                        return;

                    LayoutElement control;
                    control.type = type;
                    control.id = layout.generateUniqueId (elementTypeToString (type) + "_");
                    control.x = x;
                    control.y = y;
                    control.width = w;
                    control.height = h;
                    control.parameterId = parameterId;
                    control.label = label;
                    control.groupId = controlGroupId;
                    control.containerId = controlContainerId;
                    control.semanticRole = role;
                    control.style = "Modern Dark";
                    control.accentColour = controlAccent;
                    control.borderColour = PatchCraftLookAndFeel::border();
                    control.backgroundColour = juce::Colour (0x33141822);
                    control.cornerRadius = type == ElementType::Slider ? 8.0f : 12.0f;
                    layout.add (control);
                };

                addLinkedControl (ElementType::Slider, "sampleStart",  "Start",  baseX,       baseY, 78, 128);
                addLinkedControl (ElementType::Slider, "sampleLength", "Length", baseX + 86,  baseY, 78, 128);
                addLinkedControl (ElementType::Knob,   "samplePitch",  "Pitch",  baseX + 180, baseY, 92, 92);
                addLinkedControl (ElementType::Knob,   "volume",       "Level",  baseX + 282, baseY, 92, 92);
            });

        repaint();
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

    bool CanvasEditor::getMultiSelectionScreenBounds (juce::Rectangle<int>& bounds) const
    {
        const auto& selectedIds = owner.getSelectedElementIds();
        if (selectedIds.size() < 2)
            return false;

        bool hasBounds = false;
        int scalableItems = 0;
        for (const auto& id : selectedIds)
        {
            if (auto* element = owner.getProject().getLayout().find (id))
            {
                if (! element->visible || element->locked || element->type == ElementType::Group || ! isElementOnCurrentTab (*element))
                    continue;

                const auto r = elementScreenRect (*element);
                bounds = hasBounds ? bounds.getUnion (r) : r;
                hasBounds = true;
                ++scalableItems;
            }
        }

        return scalableItems >= 2;
    }

    bool CanvasEditor::multiSelectionResizeHandleContains (juce::Point<int> point) const
    {
        juce::Rectangle<int> bounds;
        if (! getMultiSelectionScreenBounds (bounds))
            return false;

        return resizeHandleAt (point, bounds) != ResizeHandle::None;
    }

    CanvasEditor::ResizeHandle CanvasEditor::resizeHandleAt (juce::Point<int> point, juce::Rectangle<int> bounds) const
    {
        constexpr int hs = 8;
        const int cx = bounds.getCentreX();
        const int cy = bounds.getCentreY();
        const juce::Rectangle<int> tl (bounds.getX() - hs, bounds.getY() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> tr (bounds.getRight() - hs, bounds.getY() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> br (bounds.getRight() - hs, bounds.getBottom() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> bl (bounds.getX() - hs, bounds.getBottom() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> top (cx - hs, bounds.getY() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> bottom (cx - hs, bounds.getBottom() - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> left (bounds.getX() - hs, cy - hs, hs * 2, hs * 2);
        const juce::Rectangle<int> right (bounds.getRight() - hs, cy - hs, hs * 2, hs * 2);

        if (tl.contains (point)) return ResizeHandle::TopLeft;
        if (tr.contains (point)) return ResizeHandle::TopRight;
        if (br.contains (point)) return ResizeHandle::BottomRight;
        if (bl.contains (point)) return ResizeHandle::BottomLeft;
        if (top.contains (point)) return ResizeHandle::Top;
        if (bottom.contains (point)) return ResizeHandle::Bottom;
        if (left.contains (point)) return ResizeHandle::Left;
        if (right.contains (point)) return ResizeHandle::Right;
        return ResizeHandle::None;
    }

    // ---- Rulers --------------------------------------------------------------

#include "CanvasEditor_Rendering.cpp"
#include "CanvasEditor_Interaction.cpp"
#include "CanvasEditor_Actions.cpp"

} // namespace patchcraft
