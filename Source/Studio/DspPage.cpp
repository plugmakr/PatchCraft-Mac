#include "DspPage.h"

#include "DebugLog.h"
#include "EffectEngine.h"
#include "PatchCraftLookAndFeel.h"
#include "PresetGenerator.h"
#include "StudioMainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace patchcraft
{
    namespace
    {
        constexpr int kSourceMatrixBankSize = 6;
        constexpr int kSourceMatrixBankCount = 4;

        static bool parameterIsEnabled (const PatchCraftProject& project, const ParameterDef& parameter)
        {
            if (parameter.enabledBy.isEmpty())
                return true;

            const auto* gate = project.getParameters().find (parameter.enabledBy);
            const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
            const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
            return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
        }

        static juce::String parameterTooltip (const PatchCraftProject& project, const ParameterDef& parameter)
        {
            juce::String text = parameter.name + " (" + parameter.id + ")";
            text += "\nSection: " + parameter.section + " / " + parameter.category;
            text += "\nDrag this parameter label to the Design canvas to create a bound knob.";
            if (! parameter.hostAutomatable)
                text += "\nNot exposed for host automation.";
            if (! parameter.midiLearnable)
                text += "\nMIDI learn is disabled for this parameter.";
            if (! parameter.modulatable)
                text += "\nCannot be used as a modulation target.";
            if (! parameterIsEnabled (project, parameter))
                text += "\nDisabled: " + (parameter.enableHint.isNotEmpty()
                    ? parameter.enableHint
                    : ("Enable " + parameter.enabledBy + " first."));
            return text;
        }

        static void styleTab (juce::TextButton& button, int groupId)
        {
            button.setClickingTogglesState (true);
            button.setRadioGroupId (groupId);
            button.getProperties().set ("flatTab", true);
            button.getProperties().set ("fontSize", 12.0f);
        }

        static void styleEngineButton (juce::TextButton& button, int groupId)
        {
            button.setClickingTogglesState (true);
            button.setRadioGroupId (groupId);
            button.getProperties().set ("smallButton", true);
        }

        static juce::uint16 readU16LE (juce::InputStream& in)
        {
            const auto b0 = (juce::uint16) in.readByte();
            const auto b1 = (juce::uint16) in.readByte();
            return (juce::uint16) (b0 | (b1 << 8));
        }

        static juce::uint32 readU32LE (juce::InputStream& in)
        {
            const auto b0 = (juce::uint32) in.readByte();
            const auto b1 = (juce::uint32) in.readByte();
            const auto b2 = (juce::uint32) in.readByte();
            const auto b3 = (juce::uint32) in.readByte();
            return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        }

        static juce::String readFourCC (juce::InputStream& in)
        {
            char id[5] {};
            return in.read (id, 4) == 4 ? juce::String::fromUTF8 (id, 4) : juce::String();
        }

        static juce::String shortenedSampleName (const juce::File& file, int maxChars = 52)
        {
            auto name = file.getFileName();
            if (name.length() <= maxChars)
                return name;

            const auto extension = file.getFileExtension();
            const int suffixChars = juce::jmin (18, juce::jmax (0, maxChars / 3));
            const int prefixChars = juce::jmax (8, maxChars - suffixChars - extension.length() - 3);
            return name.substring (0, prefixChars)
                + "..."
                + name.substring (juce::jmax (0, name.length() - suffixChars - extension.length()));
        }

        static int bankForBlockOrdinal (const DspBlock& block, int ordinal)
        {
            const auto it = block.values.find ("bank");
            if (it != block.values.end())
                return juce::jlimit (0, kSourceMatrixBankCount - 1, juce::roundToInt (it->second));
            return juce::jlimit (0, kSourceMatrixBankCount - 1, ordinal / kSourceMatrixBankSize);
        }

        static void normaliseDspGraphSectionBanks (DspGraph& graph, const juce::String& sectionId)
        {
            std::array<int, kSourceMatrixBankCount> bankCounts {};
            int ordinal = 0;
            for (auto& block : graph.blocks)
            {
                if (block.section == sectionId)
                {
                    int bank = bankForBlockOrdinal (block, ordinal);
                    if (bankCounts[(size_t) bank] >= kSourceMatrixBankSize)
                    {
                        for (int candidate = 0; candidate < kSourceMatrixBankCount; ++candidate)
                        {
                            if (bankCounts[(size_t) candidate] < kSourceMatrixBankSize)
                            {
                                bank = candidate;
                                break;
                            }
                        }
                    }

                    block.values["bank"] = (float) bank;
                    ++bankCounts[(size_t) bank];
                    ++ordinal;
                }
            }
        }

        static void normaliseDspGraphBanks (DspGraph& graph)
        {
            for (const auto& section : { juce::String ("source"), juce::String ("filter"), juce::String ("amp"),
                                         juce::String ("mod"), juce::String ("fx"), juce::String ("out") })
                normaliseDspGraphSectionBanks (graph, section);
        }

        static int countBlocksInSectionBank (const DspGraph& graph, const juce::String& sectionId, int targetBank)
        {
            int ordinal = 0;
            int count = 0;
            for (const auto& block : graph.blocks)
            {
                if (block.section != sectionId)
                    continue;

                if (bankForBlockOrdinal (block, ordinal) == targetBank)
                    ++count;
                ++ordinal;
            }
            return count;
        }

        static int countBlocksInSection (const DspGraph& graph, const juce::String& sectionId)
        {
            return (int) std::count_if (graph.blocks.begin(), graph.blocks.end(),
                [&] (const DspBlock& block) { return block.section == sectionId; });
        }

        static int findPatchIndexById (const PatchCraftProject& project, const juce::String& patchId)
        {
            const auto& patches = project.getPatches();
            for (int i = 0; i < (int) patches.size(); ++i)
                if (patches[(size_t) i].id == patchId)
                    return i;
            return -1;
        }

        static int findPresetIndexByName (const PatchCraftProject& project, const juce::String& presetName)
        {
            const auto& presets = project.getPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].name == presetName)
                    return i;
            return -1;
        }

        static juce::StringArray packTokensFromText (const juce::String& text)
        {
            auto tokens = juce::StringArray::fromTokens (text, ",;\n", "\"");
            tokens.trim();
            tokens.removeEmptyStrings();
            return tokens;
        }

        static void showBlockBankFullAlert (const juce::String& sectionLabel, int bank)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Bank Full")
                    .withMessage (sectionLabel + " Bank " + juce::String (bank + 1)
                                  + " already has 6 blocks. Select another bank or delete a block first.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::InfoIcon),
                nullptr);
        }

        static bool isAudioReactiveModType (const juce::String& type)
        {
            return type.containsIgnoreCase ("follower")
                || type.containsIgnoreCase ("transient")
                || type.containsIgnoreCase ("centroid")
                || type.containsIgnoreCase ("bandEnergy")
                || type.containsIgnoreCase ("gateTrigger");
        }

        static float frequencyToNormalised (float frequency)
        {
            const auto f = juce::jlimit (20.0f, 20000.0f, frequency);
            return juce::jlimit (0.0f, 1.0f, std::log (f / 20.0f) / std::log (1000.0f));
        }

        static float normalisedToFrequency (float normalised)
        {
            return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, normalised));
        }

        static juce::String arpPatternName (int pattern)
        {
            switch (juce::jlimit (0, 7, pattern))
            {
                case 1: return "Down";
                case 2: return "Up/Down";
                case 3: return "Chord Pulse";
                case 4: return "Odd Steps";
                case 5: return "Even Steps";
                case 6: return "Euclidean";
                case 7: return "Seeded Random";
                default: return "Custom Notes";
            }
        }

        static float eqQToNormalised (float q)
        {
            const auto limited = juce::jlimit (0.10f, 18.0f, q);
            return juce::jlimit (0.0f, 1.0f, std::log (limited / 0.10f) / std::log (180.0f));
        }

        static float normalisedToEqQ (float normalised)
        {
            return 0.10f * std::pow (180.0f, juce::jlimit (0.0f, 1.0f, normalised));
        }

        static float valueForBlockKey (const DspBlock& block, const juce::String& key, float fallback)
        {
            const auto it = block.values.find (key);
            return it == block.values.end() ? fallback : it->second;
        }

        static int eqTypeForBlock (const DspBlock& block)
        {
            return juce::jlimit (0, 5, juce::roundToInt (valueForBlockKey (block, "eqType", 0.0f)));
        }

        static juce::String eqTypeName (int type)
        {
            switch (juce::jlimit (0, 5, type))
            {
                case 1: return "Low Shelf";
                case 2: return "High Shelf";
                case 3: return "High Pass";
                case 4: return "Low Pass";
                case 5: return "Notch";
                default: break;
            }
            return "Bell";
        }

        static juce::String eqModeName (int mode)
        {
            switch (juce::jlimit (0, 4, mode))
            {
                case 1: return "Left";
                case 2: return "Right";
                case 3: return "Mid";
                case 4: return "Side";
                default: break;
            }
            return "Stereo";
        }

        static juce::String wtTableName (int table)
        {
            switch (juce::jlimit (0, 8, table))
            {
                case 1: return "Glass";
                case 2: return "PWM";
                case 3: return "Formant";
                case 4: return "Razor";
                case 5: return "Organ";
                case 6: return "Aggro";
                case 7: return "Hybrid";
                case 8: return "Custom";
                default: break;
            }
            return "Analog";
        }

        static juce::String formatEqFrequency (float frequency)
        {
            frequency = juce::jlimit (20.0f, 20000.0f, frequency);
            return frequency >= 1000.0f ? juce::String (frequency / 1000.0f, 2) + "k"
                                         : juce::String (juce::roundToInt (frequency));
        }

        static float frequencyToX (juce::Rectangle<int> bounds, float frequency)
        {
            return bounds.getX() + frequencyToNormalised (frequency) * (float) juce::jmax (1, bounds.getWidth());
        }

        static float xToEqFrequency (juce::Rectangle<int> bounds, int x)
        {
            return normalisedToFrequency ((float) (x - bounds.getX()) / (float) juce::jmax (1, bounds.getWidth()));
        }

        static float eqDbToY (juce::Rectangle<int> bounds, float db)
        {
            const auto normalised = juce::jmap (juce::jlimit (-24.0f, 24.0f, db), -24.0f, 24.0f, 1.0f, 0.0f);
            return bounds.getY() + normalised * (float) juce::jmax (1, bounds.getHeight());
        }

        static float yToEqDb (juce::Rectangle<int> bounds, int y)
        {
            const auto normalised = juce::jlimit (0.0f, 1.0f, (float) (y - bounds.getY()) / (float) juce::jmax (1, bounds.getHeight()));
            return juce::jmap (normalised, 1.0f, 0.0f, -24.0f, 24.0f);
        }

        static float eqQToY (juce::Rectangle<int> bounds, float q)
        {
            const auto normalised = eqQToNormalised (q);
            return bounds.getBottom() - normalised * (float) juce::jmax (1, bounds.getHeight());
        }

        static float yToEqQ (juce::Rectangle<int> bounds, int y)
        {
            const auto normalised = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (y - bounds.getY()) / (float) juce::jmax (1, bounds.getHeight()));
            return normalisedToEqQ (normalised);
        }

        static float eqNodeY (juce::Rectangle<int> bounds, const DspBlock& block)
        {
            const int type = eqTypeForBlock (block);
            if (type >= 3)
                return eqQToY (bounds, valueForBlockKey (block, "eqQ", 1.0f));

            return eqDbToY (bounds, valueForBlockKey (block, "eqGainDb", 0.0f));
        }

        static float smoothStep01 (float value)
        {
            value = juce::jlimit (0.0f, 1.0f, value);
            return value * value * (3.0f - 2.0f * value);
        }

        static float eqDisplayDbAt (const DspBlock& block, float frequency)
        {
            if (! block.enabled || ! block.type.containsIgnoreCase ("eq"))
                return 0.0f;

            const float f0 = juce::jlimit (20.0f, 20000.0f, valueForBlockKey (block, "eqFreq", 1000.0f));
            const float q = juce::jlimit (0.10f, 18.0f, valueForBlockKey (block, "eqQ", 1.0f));
            const float gain = juce::jlimit (-24.0f, 24.0f, valueForBlockKey (block, "eqGainDb", 0.0f));
            const float octaves = (float) (std::log (juce::jlimit (20.0f, 20000.0f, frequency) / f0) / std::log (2.0f));
            const float bell = std::exp (-0.5f * std::pow (octaves * juce::jlimit (0.35f, 12.0f, q * 0.95f), 2.0f));

            switch (eqTypeForBlock (block))
            {
                case 1: return gain * (1.0f - smoothStep01 ((octaves * juce::jlimit (0.35f, 4.0f, q * 0.5f)) * 0.5f + 0.5f));
                case 2: return gain * smoothStep01 ((octaves * juce::jlimit (0.35f, 4.0f, q * 0.5f)) * 0.5f + 0.5f);
                case 3:
                    return frequency < f0 ? -juce::jlimit (0.0f, 48.0f, (float) (std::log (f0 / frequency) / std::log (2.0f)) * 18.0f) : 0.0f;
                case 4:
                    return frequency > f0 ? -juce::jlimit (0.0f, 48.0f, (float) (std::log (frequency / f0) / std::log (2.0f)) * 18.0f) : 0.0f;
                case 5:
                    return -juce::jlimit (6.0f, 30.0f, 8.0f + q * 2.0f) * bell;
                default:
                    break;
            }

            return gain * bell;
        }

        static juce::StringArray mixerLabelsForSection (const juce::String& section)
        {
            if (section == "filter") return { "FREQ", "GAIN", "Q/LFO", "MIX/RATE", "ON" };
            if (section == "amp")    return { "ATT", "DEC", "SUS", "REL", "ON" };
            if (section == "mod")    return { "DEPTH", "RATE", "SYNC", "VALUE", "ON" };
            if (section == "fx")     return { "MIX", "TIME", "FB", "SPACE", "ON" };
            if (section == "out")    return { "VOL", "PAN", "SYNC", "RETRIG", "ON" };
            return { "VOL/POS", "BLEND/MORPH", "SUB/WARP", "NOISE/LEVEL", "ON" };
        }

        static float blockMixerValue (const DspBlock& block, int column)
        {
            auto get = [&] (const juce::String& key, float fallback)
            {
                const auto it = block.values.find (key);
                return it == block.values.end() ? fallback : it->second;
            };

            if (column == 4)
                return block.enabled ? 1.0f : 0.0f;

            if (block.section == "filter")
            {
                if (block.type.containsIgnoreCase ("eq"))
                {
                    if (column == 0) return frequencyToNormalised (get ("eqFreq", 1000.0f));
                    if (column == 1) return juce::jmap (juce::jlimit (-24.0f, 24.0f, get ("eqGainDb", 0.0f)), -24.0f, 24.0f, 0.0f, 1.0f);
                    if (column == 2) return eqQToNormalised (get ("eqQ", 1.0f));
                    return juce::jlimit (0.0f, 1.0f, get ("eqMix", 1.0f));
                }
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("cutoff", 0.5f));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("resonance", 0.2f));
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("lfoAmount", 0.0f));
                return juce::jlimit (0.0f, 1.0f, get ("rate", 1.0f) / 20.0f);
            }
            if (block.section == "amp")
            {
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("attack", 0.05f));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("decay", 0.2f));
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("sustain", 0.8f));
                return juce::jlimit (0.0f, 1.0f, get ("release", 0.4f));
            }
            if (block.section == "mod")
            {
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("amount", 0.2f));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("rate", 1.0f) / 20.0f);
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("sync", 0.0f));
                return juce::jlimit (0.0f, 1.0f, get ("value", 0.5f));
            }
            if (block.section == "fx")
            {
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("delayMix", get ("reverbMix", get ("mix", 1.0f))));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("rate", get ("delayTime", 0.25f)) / 8.0f);
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("delayFeedback", 0.35f));
                return juce::jlimit (0.0f, 1.0f, get ("reverbMix", get ("drive", 0.0f)));
            }
            if (block.section == "out")
            {
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("volume", 0.75f));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("pan", 0.5f));
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("bpmSync", 1.0f));
                return juce::jlimit (0.0f, 1.0f, get ("retrigger", 1.0f));
            }

            if (block.section == "source" && block.type.containsIgnoreCase ("wavetable"))
            {
                if (column == 0) return juce::jlimit (0.0f, 1.0f, get ("wtPosition", 0.0f));
                if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("wtMorph", 0.0f));
                if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("wtWarp", 0.0f));
                return juce::jlimit (0.0f, 1.0f, get ("wtLevel", 1.0f));
            }

            if (column == 0) return juce::jlimit (0.0f, 1.0f, block.type.containsIgnoreCase ("noise") ? get ("noiseBlend", 0.18f) : get ("volume", 0.75f));
            if (column == 1) return juce::jlimit (0.0f, 1.0f, get ("oscBlend", 0.0f));
            if (column == 2) return juce::jlimit (0.0f, 1.0f, get ("subBlend", 0.0f));
            return juce::jlimit (0.0f, 1.0f, get ("noiseBlend", 0.0f));
        }

        static void setBlockMixerValue (DspBlock& block, int column, float value)
        {
            value = juce::jlimit (0.0f, 1.0f, value);
            if (column == 4)
            {
                block.enabled = value >= 0.5f;
                return;
            }

            if (block.section == "filter")
            {
                if (block.type.containsIgnoreCase ("eq"))
                {
                    if (column == 0) block.values["eqFreq"] = normalisedToFrequency (value);
                    else if (column == 1) block.values["eqGainDb"] = juce::jmap (value, 0.0f, 1.0f, -24.0f, 24.0f);
                    else if (column == 2) block.values["eqQ"] = normalisedToEqQ (value);
                    else block.values["eqMix"] = value;
                    return;
                }
                if (column == 0) block.values["cutoff"] = value;
                else if (column == 1) block.values["resonance"] = value;
                else if (column == 2) block.values["lfoAmount"] = value;
                else block.values["rate"] = juce::jlimit (0.01f, 20.0f, value * 20.0f);
                return;
            }
            if (block.section == "amp")
            {
                if (column == 0) block.values["attack"] = value;
                else if (column == 1) block.values["decay"] = value;
                else if (column == 2) block.values["sustain"] = value;
                else block.values["release"] = value;
                return;
            }
            if (block.section == "mod")
            {
                if (column == 0) block.values["amount"] = value;
                else if (column == 1) block.values["rate"] = juce::jlimit (0.01f, 20.0f, value * 20.0f);
                else if (column == 2) block.values["sync"] = value >= 0.5f ? 1.0f : 0.0f;
                else block.values["value"] = value;
                return;
            }
            if (block.section == "fx")
            {
                if (column == 0)
                {
                    if (block.type.containsIgnoreCase ("delay")) block.values["delayMix"] = value;
                    else if (block.type.containsIgnoreCase ("reverb")) block.values["reverbMix"] = value;
                    else block.values["mix"] = value;
                }
                else if (column == 1) block.values["rate"] = juce::jlimit (0.0625f, 8.0f, value * 8.0f);
                else if (column == 2) block.values["delayFeedback"] = value;
                else if (block.type.containsIgnoreCase ("dist")) block.values["drive"] = value;
                else block.values["reverbMix"] = value;
                return;
            }
            if (block.section == "out")
            {
                if (column == 0) block.values["volume"] = value;
                else if (column == 1) block.values["pan"] = value;
                else if (column == 2) block.values["bpmSync"] = value >= 0.5f ? 1.0f : 0.0f;
                else block.values["retrigger"] = value >= 0.5f ? 1.0f : 0.0f;
                return;
            }

            if (block.section == "source" && block.type.containsIgnoreCase ("wavetable"))
            {
                if (column == 0) block.values["wtPosition"] = value;
                else if (column == 1) block.values["wtMorph"] = value;
                else if (column == 2) block.values["wtWarp"] = value;
                else block.values["wtLevel"] = value;
                return;
            }

            if (column == 0)
            {
                if (block.type.containsIgnoreCase ("noise")) block.values["noiseBlend"] = value;
                else block.values["volume"] = value;
            }
            else if (column == 1) block.values["oscBlend"] = value;
            else if (column == 2) block.values["subBlend"] = value;
            else block.values["noiseBlend"] = value;
        }

        static juce::String sectionDisplayName (const juce::String& section)
        {
            if (section == "source") return "SOURCE";
            if (section == "filter") return "FILTER";
            if (section == "amp")    return "AMP";
            if (section == "mod")    return "MOD";
            if (section == "fx")     return "FX";
            if (section == "out")    return "OUT";
            return section.toUpperCase();
        }

        static juce::String formatFormulaMixerValue (const DspBlock& block, int column)
        {
            auto get = [&] (const juce::String& key, float fallback)
            {
                return valueForBlockKey (block, key, fallback);
            };

            if (column == 4)
                return block.enabled ? "ON" : "OFF";

            if (block.section == "filter")
            {
                if (block.type.containsIgnoreCase ("eq"))
                {
                    if (column == 0) return formatEqFrequency (get ("eqFreq", 1000.0f));
                    if (column == 1) return juce::String (get ("eqGainDb", 0.0f), 1) + " dB";
                    if (column == 2) return "Q " + juce::String (get ("eqQ", 1.0f), 2);
                    return juce::String (juce::roundToInt (get ("eqMix", 1.0f) * 100.0f)) + "%";
                }

                if (column == 0) return juce::String (juce::roundToInt (get ("cutoff", 0.5f) * 100.0f)) + "%";
                if (column == 1) return juce::String (juce::roundToInt (get ("resonance", 0.2f) * 100.0f)) + "%";
                if (column == 2) return juce::String (juce::roundToInt (get ("lfoAmount", 0.0f) * 100.0f)) + "%";
                return juce::String (get ("rate", 1.0f), 2);
            }

            if (block.section == "amp")
            {
                const juce::String keys[] { "attack", "decay", "sustain", "release" };
                return juce::String (juce::roundToInt (get (keys[column], column == 2 ? 0.8f : 0.2f) * 100.0f)) + "%";
            }

            if (block.section == "mod")
            {
                if (column == 0) return juce::String (juce::roundToInt (get ("amount", 0.2f) * 100.0f)) + "%";
                if (column == 1) return juce::String (get ("rate", 1.0f), 2);
                if (column == 2) return get ("sync", 0.0f) >= 0.5f ? "SYNC" : "FREE";
                return juce::String (juce::roundToInt (get ("value", 0.5f) * 100.0f)) + "%";
            }

            if (block.section == "fx")
            {
                if (column == 0) return juce::String (juce::roundToInt (blockMixerValue (block, column) * 100.0f)) + "%";
                if (column == 1) return juce::String (get ("rate", get ("delayTime", 0.25f)), 2);
                if (column == 2) return juce::String (juce::roundToInt (get ("delayFeedback", 0.35f) * 100.0f)) + "%";
                return juce::String (juce::roundToInt (blockMixerValue (block, column) * 100.0f)) + "%";
            }

            if (block.section == "out")
            {
                if (column == 0) return juce::String (juce::roundToInt (get ("volume", 0.75f) * 100.0f)) + "%";
                if (column == 1) return juce::String (juce::roundToInt (get ("pan", 0.5f) * 100.0f)) + "%";
                if (column == 2) return get ("bpmSync", 1.0f) >= 0.5f ? "SYNC" : "FREE";
                return get ("retrigger", 1.0f) >= 0.5f ? "ON" : "OFF";
            }

            if (block.section == "source" && block.type.containsIgnoreCase ("wavetable"))
            {
                if (column == 0) return juce::String (juce::roundToInt (get ("wtPosition", 0.0f) * 100.0f)) + "%";
                if (column == 1) return juce::String (juce::roundToInt (get ("wtMorph", 0.0f) * 100.0f)) + "%";
                if (column == 2) return juce::String (juce::roundToInt (get ("wtWarp", 0.0f) * 100.0f)) + "%";
                return juce::String (juce::roundToInt (get ("wtLevel", 1.0f) * 100.0f)) + "%";
            }

            return juce::String (juce::roundToInt (blockMixerValue (block, column) * 100.0f)) + "%";
        }

        struct NodeMapBlockSummary
        {
            juce::String name;
            juce::String type;
            juce::String target;
            juce::StringArray values;
            int graphIndex = -1;
            bool enabled = true;
        };

        struct NodeMapRouteSummary
        {
            juce::String label;
            juce::String detail;
            float amount = 0.0f;
            int kind = 0;
            int index = -1;
            bool enabled = true;
        };

        class DspNodeMapView : public juce::Component
        {
        public:
            DspNodeMapView (juce::String sectionNameToUse, int bankToUse,
                            std::vector<NodeMapBlockSummary> blockSummaries,
                            std::vector<NodeMapRouteSummary> routeSummaries,
                            std::function<void (int)> blockSelectedIn,
                            std::function<void (int)> blockEditIn,
                            std::function<void (int, int)> routeSelectedIn)
                : sectionName (std::move (sectionNameToUse)),
                  bank (bankToUse),
                  blocks (std::move (blockSummaries)),
                  routes (std::move (routeSummaries)),
                  blockSelected (std::move (blockSelectedIn)),
                  blockEdit (std::move (blockEditIn)),
                  routeSelectedCallback (std::move (routeSelectedIn))
            {
                setSize (920, 560);
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());

                auto bounds = getLocalBounds().reduced (18);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));
                g.drawText (sectionName + " Node Map - Bank " + juce::String (bank + 1),
                            bounds.removeFromTop (28), juce::Justification::centredLeft);

                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (12.0f));
                g.drawText ("Click blocks/routes to select them in Graph Inspector. Double-click a block for the larger Sound Formula editor.",
                            bounds.removeFromTop (22), juce::Justification::centredLeft);
                bounds.removeFromTop (8);

                auto side = bounds.removeFromRight (260);
                bounds.removeFromRight (14);
                drawFlow (g, bounds);
                drawRoutes (g, side);
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                for (int i = 0; i < (int) blockHitRects.size(); ++i)
                {
                    if (blockHitRects[(size_t) i].contains (e.getPosition()))
                    {
                        selectedBlock = i;
                        selectedRoute = -1;
                        if (blockSelected && blocks[(size_t) i].graphIndex >= 0)
                            blockSelected (blocks[(size_t) i].graphIndex);
                        repaint();
                        return;
                    }
                }

                for (int i = 0; i < (int) routeHitRects.size(); ++i)
                {
                    if (routeHitRects[(size_t) i].contains (e.getPosition()))
                    {
                        selectedRoute = i;
                        selectedBlock = -1;
                        if (routeSelectedCallback)
                            routeSelectedCallback (routes[(size_t) i].kind, routes[(size_t) i].index);
                        repaint();
                        return;
                    }
                }

                selectedBlock = -1;
                selectedRoute = -1;
                repaint();
            }

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                for (int i = 0; i < (int) blockHitRects.size(); ++i)
                {
                    if (blockHitRects[(size_t) i].contains (e.getPosition()))
                    {
                        selectedBlock = i;
                        selectedRoute = -1;
                        if (blockEdit && blocks[(size_t) i].graphIndex >= 0)
                            blockEdit (blocks[(size_t) i].graphIndex);
                        repaint();
                        return;
                    }
                }
            }

        private:
            juce::String sectionName;
            int bank = 0;
            std::vector<NodeMapBlockSummary> blocks;
            std::vector<NodeMapRouteSummary> routes;
            mutable std::vector<juce::Rectangle<int>> blockHitRects;
            mutable std::vector<juce::Rectangle<int>> routeHitRects;
            std::function<void (int)> blockSelected;
            std::function<void (int)> blockEdit;
            std::function<void (int, int)> routeSelectedCallback;
            int selectedBlock = -1;
            int selectedRoute = -1;

            void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::String title) const
            {
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (area.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (area.toFloat(), 10.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
                g.drawText (title, area.reduced (12, 8).removeFromTop (18), juce::Justification::centredLeft);
            }

            void drawNode (juce::Graphics& g, juce::Rectangle<int> area,
                           const juce::String& title, const juce::String& subtitle,
                           bool enabled, juce::StringArray valueLines,
                           bool selected = false) const
            {
                g.setColour (enabled ? PatchCraftLookAndFeel::accent().withAlpha (0.18f)
                                     : PatchCraftLookAndFeel::panel().brighter (0.06f));
                g.fillRoundedRectangle (area.toFloat(), 9.0f);
                g.setColour (selected ? juce::Colour (0xff20d6ff)
                                      : enabled ? PatchCraftLookAndFeel::accent()
                                     : PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (area.toFloat(), 9.0f, selected ? 2.2f : (enabled ? 1.4f : 1.0f));

                auto text = area.reduced (10, 7);
                g.setColour (enabled ? PatchCraftLookAndFeel::textBright()
                                     : PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
                g.drawFittedText (title, text.removeFromTop (18), juce::Justification::centredLeft, 1);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (10.5f));
                g.drawFittedText (subtitle, text.removeFromTop (16), juce::Justification::centredLeft, 1);

                text.removeFromTop (5);
                const int rows = juce::jmin (4, valueLines.size());
                for (int i = 0; i < rows; ++i)
                {
                    g.setColour (PatchCraftLookAndFeel::text().withAlpha (enabled ? 0.9f : 0.45f));
                    g.drawFittedText (valueLines[i], text.removeFromTop (15), juce::Justification::centredLeft, 1);
                }
            }

            void drawConnector (juce::Graphics& g, juce::Point<float> start, juce::Point<float> end,
                                juce::Colour colour) const
            {
                juce::Path path;
                path.startNewSubPath (start);
                const auto midX = (start.x + end.x) * 0.5f;
                path.cubicTo (midX, start.y, midX, end.y, end.x, end.y);
                g.setColour (colour);
                g.strokePath (path, juce::PathStrokeType (2.0f));
            }

            static juce::Point<float> leftCentre (juce::Rectangle<int> area)
            {
                return { (float) area.getX(), (float) area.getCentreY() };
            }

            static juce::Point<float> rightCentre (juce::Rectangle<int> area)
            {
                return { (float) area.getRight(), (float) area.getCentreY() };
            }

            void drawFlow (juce::Graphics& g, juce::Rectangle<int> area) const
            {
                blockHitRects.clear();
                drawPanel (g, area, "COMPOSITION FLOW");
                auto flow = area.reduced (16, 38);
                auto lane = flow.removeFromTop (juce::jmax (170, flow.getHeight() - 110));

                auto inputNode = lane.withWidth (86).withHeight (70).withY (lane.getCentreY() - 35);
                auto outputNode = lane.withTrimmedLeft (lane.getWidth() - 94).withHeight (70).withY (lane.getCentreY() - 35);
                juce::StringArray inputValues { "MIDI / Sample", "Patch input" };
                juce::StringArray outputValues { "Mixer", "Player out" };
                drawNode (g, inputNode, "INPUT", "Signal source", true, inputValues);
                drawNode (g, outputNode, "OUT", "Section output", true, outputValues);

                if (blocks.empty())
                {
                    auto empty = lane.withTrimmedLeft (112).withTrimmedRight (122).reduced (8);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawFittedText ("No blocks in this bank yet. Add blocks to create a signal path, then use the mixer and Graph Inspector to shape it.",
                                      empty, juce::Justification::centred, 3);
                    drawConnector (g, rightCentre (inputNode), leftCentre (outputNode),
                                   PatchCraftLookAndFeel::border());
                }
                else
                {
                    const int count = (int) blocks.size();
                    auto nodeArea = lane.withTrimmedLeft (106).withTrimmedRight (112);
                    const int gap = 12;
                    const int nodeW = juce::jlimit (92, 132, (nodeArea.getWidth() - gap * juce::jmax (0, count - 1)) / count);
                    const int nodeH = 118;
                    juce::Rectangle<int> previous = inputNode;

                    for (int i = 0; i < count; ++i)
                    {
                        const int x = nodeArea.getX() + i * (nodeW + gap);
                        auto node = juce::Rectangle<int> (x, lane.getCentreY() - nodeH / 2, nodeW, nodeH);
                        drawConnector (g, rightCentre (previous), leftCentre (node),
                                       blocks[(size_t) i].enabled ? PatchCraftLookAndFeel::accent().withAlpha (0.75f)
                                                                  : PatchCraftLookAndFeel::border());
                        blockHitRects.push_back (node);
                        drawNode (g, node, blocks[(size_t) i].name, blocks[(size_t) i].type + " -> " + blocks[(size_t) i].target,
                                  blocks[(size_t) i].enabled, blocks[(size_t) i].values,
                                  selectedBlock == i);
                        previous = node;
                    }

                    drawConnector (g, rightCentre (previous), leftCentre (outputNode),
                                   PatchCraftLookAndFeel::accent().withAlpha (0.75f));
                }

                auto legend = flow.removeFromBottom (84);
                g.setColour (PatchCraftLookAndFeel::panel().brighter (0.05f));
                g.fillRoundedRectangle (legend.toFloat(), 8.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (legend.toFloat(), 8.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
                g.drawText ("What this view means", legend.reduced (12, 8).removeFromTop (18), juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (11.0f));
                g.drawFittedText ("Blocks flow left-to-right through the current module bank. Bypassed blocks stay in the map but use a dim outline. Routes on the right show macros, modulation, and automation targeting this module.",
                                  legend.reduced (12, 28), juce::Justification::topLeft, 3);
            }

            void drawRoutes (juce::Graphics& g, juce::Rectangle<int> area) const
            {
                routeHitRects.clear();
                drawPanel (g, area, "MACROS / MOD / AUTOMATION");
                auto list = area.reduced (12, 36);

                auto inspector = list.removeFromTop (116).reduced (0, 3);
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.50f));
                g.fillRoundedRectangle (inspector.toFloat(), 8.0f);
                g.setColour (PatchCraftLookAndFeel::borderSoft());
                g.drawRoundedRectangle (inspector.toFloat(), 8.0f, 1.0f);
                auto text = inspector.reduced (10, 8);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.setFont (juce::FontOptions (11.5f).withStyle ("Bold"));
                g.drawText ("INSPECTOR", text.removeFromTop (16), juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (10.0f));
                if (selectedBlock >= 0 && selectedBlock < (int) blocks.size())
                {
                    const auto& block = blocks[(size_t) selectedBlock];
                    g.drawFittedText (block.name + "\n" + block.type + " -> " + block.target + "\n"
                                      + (block.enabled ? "Enabled" : "Bypassed"),
                                      text, juce::Justification::topLeft, 4);
                }
                else if (selectedRoute >= 0 && selectedRoute < (int) routes.size())
                {
                    const auto& route = routes[(size_t) selectedRoute];
                    g.drawFittedText (route.label + "\n" + route.detail + "\nAmount "
                                      + juce::String (juce::roundToInt (std::abs (route.amount) * 100.0f)) + "%",
                                      text, juce::Justification::topLeft, 4);
                }
                else
                {
                    g.drawFittedText ("Click a block card or route row to inspect it here. This map is the readable routing overview for the selected bank.",
                                      text, juce::Justification::topLeft, 4);
                }
                list.removeFromTop (8);

                if (routes.empty())
                {
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (12.0f));
                    g.drawFittedText ("No active macro, modulation, or automation routes target this module yet.",
                                      list, juce::Justification::centred, 4);
                    return;
                }

                for (const auto& route : routes)
                {
                    auto row = list.removeFromTop (54).reduced (0, 3);
                    const auto routeIndex = (int) routeHitRects.size();
                    routeHitRects.push_back (row);
                    g.setColour (route.enabled ? PatchCraftLookAndFeel::accent().withAlpha (0.14f)
                                               : PatchCraftLookAndFeel::panel().brighter (0.05f));
                    g.fillRoundedRectangle (row.toFloat(), 7.0f);
                    g.setColour (selectedRoute == routeIndex ? juce::Colour (0xff20d6ff)
                                               : route.enabled ? PatchCraftLookAndFeel::accent().withAlpha (0.55f)
                                               : PatchCraftLookAndFeel::border());
                    g.drawRoundedRectangle (row.toFloat(), 7.0f, selectedRoute == routeIndex ? 2.0f : 1.0f);

                    auto text = row.reduced (9, 6);
                    g.setColour (route.enabled ? PatchCraftLookAndFeel::textBright()
                                               : PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
                    g.drawFittedText (route.label, text.removeFromTop (16), juce::Justification::centredLeft, 1);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawFittedText (route.detail, text.removeFromTop (14), juce::Justification::centredLeft, 1);

                    const auto amount = juce::jlimit (0.0f, 1.0f, std::abs (route.amount));
                    auto meter = text.removeFromTop (7);
                    meter = meter.withWidth (juce::roundToInt ((float) meter.getWidth() * amount));
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.fillRoundedRectangle (meter.toFloat(), 3.0f);

                    if (list.getHeight() < 54)
                        break;
                }
            }
        };

        static float clampSample (double value)
        {
            return std::isfinite (value) ? juce::jlimit (-1.0f, 1.0f, (float) value) : 0.0f;
        }

        static float decodeWavSample (const unsigned char* frame,
                                      int availableBytes,
                                      int offset,
                                      int bytesPerSample,
                                      int bitsPerSample,
                                      int audioFormat)
        {
            if (frame == nullptr || offset < 0 || bytesPerSample <= 0 || offset + bytesPerSample > availableBytes)
                return 0.0f;

            const auto* data = frame + offset;
            if (audioFormat == 3)
            {
                if (bitsPerSample == 32 && bytesPerSample >= 4)
                {
                    float value = 0.0f;
                    std::memcpy (&value, data, sizeof (float));
                    return clampSample (value);
                }
                if (bitsPerSample == 64 && bytesPerSample >= 8)
                {
                    double value = 0.0;
                    std::memcpy (&value, data, sizeof (double));
                    return clampSample (value);
                }
                return 0.0f;
            }

            if (bitsPerSample == 8 && bytesPerSample >= 1)
                return clampSample (((int) data[0] - 128) / 128.0);
            if (bitsPerSample == 16 && bytesPerSample >= 2)
            {
                const auto value = (short) ((juce::uint16) data[0] | ((juce::uint16) data[1] << 8));
                return clampSample ((double) value / 32768.0);
            }
            if (bitsPerSample == 24 && bytesPerSample >= 3)
            {
                int value = (int) data[0] | ((int) data[1] << 8) | ((int) data[2] << 16);
                if ((value & 0x00800000) != 0)
                    value |= (int) 0xff000000;
                return clampSample ((double) value / 8388608.0);
            }
            if (bitsPerSample == 32 && bytesPerSample >= 4)
            {
                if (audioFormat == 3)
                {
                    float value = 0.0f;
                    std::memcpy (&value, data, sizeof (value));
                    return clampSample ((double) value);
                }
                const int value = (int) ((juce::uint32) data[0]
                                      | ((juce::uint32) data[1] << 8)
                                      | ((juce::uint32) data[2] << 16)
                                      | ((juce::uint32) data[3] << 24));
                return clampSample ((double) value / 2147483648.0);
            }
            return 0.0f;
        }

        static bool tryLoadWavPreviewSafely (const juce::File& file,
                                             juce::AudioBuffer<float>& loaded,
                                             double& sampleRate,
                                             juce::String& error)
        {
            auto stream = file.createInputStream();
            if (stream == nullptr || ! stream->openedOk())
            {
                error = "Could not open WAV stream";
                return false;
            }

            if (readFourCC (*stream) != "RIFF")
            {
                error = "Unsupported WAV container";
                return false;
            }

            (void) readU32LE (*stream);
            if (readFourCC (*stream) != "WAVE")
            {
                error = "Not a WAVE file";
                return false;
            }

            int audioFormat = 0;
            int numChannels = 0;
            int bitsPerSample = 0;
            int blockAlign = 0;
            juce::int64 dataStart = -1;
            juce::uint32 dataSize = 0;
            bool haveFmt = false;

            while (! stream->isExhausted())
            {
                const auto chunkId = readFourCC (*stream);
                if (chunkId.length() != 4)
                    break;

                const auto chunkSize = readU32LE (*stream);
                const auto chunkDataStart = stream->getPosition();
                if (chunkId == "fmt ")
                {
                    if (chunkSize < 16)
                    {
                        error = "Invalid WAV fmt chunk";
                        return false;
                    }

                    audioFormat = (int) readU16LE (*stream);
                    numChannels = (int) readU16LE (*stream);
                    sampleRate = (double) readU32LE (*stream);
                    (void) readU32LE (*stream);
                    blockAlign = (int) readU16LE (*stream);
                    bitsPerSample = (int) readU16LE (*stream);
                    if (audioFormat == 0xfffe && chunkSize >= 40)
                    {
                        (void) readU16LE (*stream);
                        (void) readU16LE (*stream);
                        (void) readU32LE (*stream);
                        audioFormat = (int) readU16LE (*stream);
                    }
                    haveFmt = true;
                }
                else if (chunkId == "data")
                {
                    dataStart = chunkDataStart;
                    dataSize = chunkSize;
                }

                stream->setPosition (chunkDataStart + (juce::int64) chunkSize + (chunkSize & 1u));
                if (haveFmt && dataStart >= 0)
                    break;
            }

            if (! haveFmt || dataStart < 0)
            {
                error = "WAV is missing fmt or data chunk";
                return false;
            }

            if ((audioFormat != 1 && audioFormat != 3) || sampleRate <= 0.0 || numChannels <= 0
                || bitsPerSample <= 0 || blockAlign <= 0)
            {
                error = "Unsupported WAV format";
                return false;
            }

            const int bytesPerSample = (bitsPerSample + 7) / 8;
            if (bytesPerSample <= 0 || blockAlign < bytesPerSample * numChannels || blockAlign > 4096)
            {
                error = "Unsupported WAV frame layout";
                return false;
            }

            const int channelsToRead = juce::jlimit (1, 2, numChannels);
            const auto fileSize = file.getSize();
            const auto availableDataBytes = dataStart >= 0 && fileSize > dataStart
                ? juce::jmin ((juce::int64) dataSize, fileSize - dataStart)
                : (juce::int64) dataSize;
            const auto totalFrames = availableDataBytes / blockAlign;
            const auto maxPreviewFrames = (juce::int64) (juce::jmax (1.0, sampleRate) * 60.0);
            const auto maxFramesByMemory = (128ll * 1024ll * 1024ll)
                                          / (juce::int64) channelsToRead
                                          / (juce::int64) sizeof (float);
            const auto frames64 = juce::jmin (juce::jmin (totalFrames, maxPreviewFrames), maxFramesByMemory);
            if (frames64 <= 0 || frames64 > (juce::int64) std::numeric_limits<int>::max())
            {
                error = "Sample is empty or too large";
                return false;
            }

            const int framesToRead = (int) frames64;
            const auto bytesToRead64 = frames64 * (juce::int64) blockAlign;
            const auto estimatedBytes = (juce::int64) channelsToRead * framesToRead * (juce::int64) sizeof (float);
            PC_DBG ("FX safe WAV metadata: channels=%d bits=%d blockAlign=%d dataStart=%lld dataSize=%u fileSize=%lld frames=%d bytes=%lld",
                    numChannels,
                    bitsPerSample,
                    blockAlign,
                    (long long) dataStart,
                    (unsigned int) dataSize,
                    (long long) fileSize,
                    framesToRead,
                    (long long) bytesToRead64);
            if (estimatedBytes > 128ll * 1024ll * 1024ll)
            {
                error = "Sample preview could not be capped safely";
                return false;
            }

            if (bytesToRead64 <= 0 || bytesToRead64 > (juce::int64) std::numeric_limits<int>::max())
            {
                error = "WAV preview byte range is unsupported";
                return false;
            }

            std::vector<unsigned char> sampleData;
            try
            {
                sampleData.resize ((size_t) bytesToRead64, 0);
            }
            catch (...)
            {
                error = "Could not allocate WAV preview read buffer";
                return false;
            }

            stream->setPosition (dataStart);
            const int bytesRead = stream->read (sampleData.data(), (int) bytesToRead64);
            const int actualFrames = bytesRead > 0 ? bytesRead / blockAlign : 0;
            if (actualFrames <= 0)
            {
                error = "Unexpected end of WAV data";
                return false;
            }

            try
            {
                loaded.setSize (channelsToRead, actualFrames, false, true, true);
            }
            catch (...)
            {
                error = "Could not allocate WAV preview buffer";
                return false;
            }

            for (int ch = 0; ch < channelsToRead; ++ch)
            {
                auto* dest = loaded.getWritePointer (ch);
                const int sampleOffset = ch * bytesPerSample;
                for (int i = 0; i < actualFrames; ++i)
                {
                    const auto* frame = sampleData.data() + (size_t) i * (size_t) blockAlign;
                    dest[i] = decodeWavSample (frame, blockAlign, sampleOffset,
                                               bytesPerSample, bitsPerSample, audioFormat);
                }
            }

            return true;
        }
    }

    DspPage::ParamStrip::ParamStrip()
    {
        name.setFont (juce::Font (9.5f, juce::Font::bold));
        name.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        name.setJustificationType (juce::Justification::centred);
        name.setInterceptsMouseClicks (false, false);
        name.setTooltip ("Drag this parameter label to the Design canvas to create a knob bound to it.");
        addAndMakeVisible (name);

        value.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        value.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 13);
        value.onValueChange = [this]
        {
            if (syncing || project == nullptr || parameterId.isEmpty()) return;
            project->getLiveValues().setValue (parameterId, (float) value.getValue());
            project->markDirty();
        };
        addAndMakeVisible (value);
    }

    void DspPage::ParamStrip::mouseDown (const juce::MouseEvent&)
    {
        draggingToCanvas = false;
    }

    void DspPage::ParamStrip::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingToCanvas || parameterId.isEmpty() || e.getDistanceFromDragStart() < 5)
            return;

        if (auto* dragContainer = findParentComponentOfClass<StudioMainComponent>())
        {
            draggingToCanvas = true;
            dragContainer->startDragging ("param:" + parameterId, this);
        }
    }

    void DspPage::ParamStrip::mouseUp (const juce::MouseEvent&)
    {
        draggingToCanvas = false;
    }

    void DspPage::ParamStrip::bind (PatchCraftProject& owner, const juce::String& id)
    {
        project = &owner;
        parameterId = id;
        syncFromProject();
    }

    void DspPage::ParamStrip::syncFromProject()
    {
        if (project == nullptr)
        {
            setVisible (false);
            return;
        }

        const auto* def = project->getParameters().find (parameterId);
        if (def == nullptr)
        {
            setVisible (false);
            return;
        }

        setVisible (true);
        name.setText (def->name, juce::dontSendNotification);
        const bool enabled = parameterIsEnabled (*project, *def);
        const auto tooltip = parameterTooltip (*project, *def);
        name.setTooltip (tooltip);
        value.setTooltip (tooltip);
        value.setEnabled (enabled);
        name.setAlpha (enabled ? 1.0f : 0.45f);
        value.setAlpha (enabled ? 1.0f : 0.35f);

        syncing = true;
        value.setRange (def->min, def->max, parameterId == "oscType" || parameterId == "octave" ? 1.0 : 0.001);
        value.setSkewFactor (1.0);
        if (parameterId == "filterCutoff")
            value.setSkewFactorFromMidPoint (1000.0);
        value.setTextValueSuffix (def->unit.isNotEmpty() ? " " + def->unit : "");
        value.setValue (project->getLiveValues().getValue (parameterId, def->defaultValue),
                        juce::dontSendNotification);
        syncing = false;
    }

    void DspPage::ParamStrip::resized()
    {
        auto r = getLocalBounds().reduced (1);
        name.setBounds (r.removeFromTop (12));
        value.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), r.getHeight()));
    }

    DspPage::Section::Section (juce::String id, juce::String sectionTitle, std::initializer_list<const char*> defaults)
        : sectionId (std::move (id))
    {
        title.setText (std::move (sectionTitle), juce::dontSendNotification);
        title.setFont (juce::Font (11.5f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (title);
        addButton.getProperties().set ("smallButton", true);
        editButton.getProperties().set ("smallButton", true);
        addAndMakeVisible (addButton);
        addAndMakeVisible (editButton);

        for (auto* parameterId : defaults)
            parameterIds.add (parameterId);
    }

    void DspPage::Section::bind (PatchCraftProject& owner)
    {
        project = &owner;
        if (controls.size() != parameterIds.size())
            setParameterIds (parameterIds);
        for (auto* strip : controls)
            strip->bind (owner, strip->parameterId);
    }

    void DspPage::Section::setParameterIds (const juce::StringArray& ids)
    {
        parameterIds = ids;
        controls.clear();
        for (auto& id : parameterIds)
        {
            auto* strip = new ParamStrip();
            strip->parameterId = id;
            controls.add (strip);
            addAndMakeVisible (strip);
            if (project != nullptr)
                strip->bind (*project, id);
        }
        resized();
    }

    void DspPage::Section::syncFromProject()
    {
        for (auto* strip : controls)
            strip->syncFromProject();
        resized();
    }

    void DspPage::Section::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
    }

    void DspPage::Section::resized()
    {
        auto r = getLocalBounds().reduced (7);
        auto header = r.removeFromTop (18);
        title.setBounds (header.removeFromLeft (juce::jmin (180, header.getWidth() / 2)));
        editButton.setBounds (header.removeFromRight (44));
        addButton.setBounds (header.removeFromRight (26).reduced (2, 0));
        r.removeFromTop (2);

        int visibleCount = 0;
        for (auto* strip : controls)
            if (strip->isVisible())
                ++visibleCount;

        if (visibleCount == 0) return;

        const int minKnobW = 58;
        const int columns = juce::jmax (1, juce::jmin (visibleCount, r.getWidth() / minKnobW));
        const int rows = (visibleCount + columns - 1) / columns;
        const int rowH = juce::jlimit (54, 68, r.getHeight() / rows);

        int visibleIndex = 0;
        for (auto* strip : controls)
        {
            if (! strip->isVisible()) continue;
            const int row = visibleIndex / columns;
            const int col = visibleIndex % columns;
            const int colW = r.getWidth() / columns;
            strip->setBounds ({ r.getX() + col * colW, r.getY() + row * rowH, colW, rowH });
            strip->setBounds (strip->getBounds().reduced (3, 1));
            ++visibleIndex;
        }
    }

    DspPage::BuilderPanel::BuilderPanel()
    {
        title.setFont (juce::Font (16.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (title);

        subtitle.setFont (juce::Font (12.0f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        for (auto* button : { &addBlockButton, &addMacroButton, &addModButton, &addArpButton, &addAutomationButton,
                              &importSampleButton,
                              &savePatchButton, &savePatchAsButton, &saveSectionPresetButton,
                              &sendExpansionButton, &packCreatorButton,
                              &openSectionEditorButton, &mixerButton,
                              &clearSectionButton, &clearAllButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }
        expansionBox.setTextWhenNothingSelected ("Expansion Pack");
        expansionBox.setTooltip ("Choose the sellable expansion pack that receives presets from this Advanced patch.");
        addAndMakeVisible (expansionBox);
        sendExpansionButton.setTooltip ("Capture the current full patch as a playable preset inside the selected expansion pack.");
        packCreatorButton.setTooltip ("Create or edit expansion-pack metadata, keywords, and folders/groups.");
        openSectionEditorButton.setTooltip ("Open this section's deep editor in a larger popout instead of compressing it into the DSP Builder.");
        openSectionEditorButton.onClick = [this]
        {
            if (onSectionEditorRequested)
                onSectionEditorRequested();
        };
        mixerButton.setTooltip ("Open the selected bank mixer in a larger modal for the current DSP section.");
        mixerButton.onClick = [this]
        {
            if (onMixerRequested)
                onMixerRequested();
        };
        addArpButton.setTooltip ("Add a MIDI Playground generator: scale-aware ARP, chords, phrase motion, swing, probability, and performance MIDI.");
        addArpButton.setVisible (false);

        auto setupBankButton = [this] (juce::TextButton& button, int bank)
        {
            button.getProperties().set ("flatTab", true);
            button.getProperties().set ("fontSize", 11.0);
            button.setClickingTogglesState (false);
            button.onClick = [this, bank]
            {
                activeSectionBank = bank;
                if (onSectionBankSelected)
                    onSectionBankSelected (bank);
                repaint();
                resized();
            };
            addChildComponent (button);
        };
        setupBankButton (sectionBankButton1, 0);
        setupBankButton (sectionBankButton2, 1);
        setupBankButton (sectionBankButton3, 2);
        setupBankButton (sectionBankButton4, 3);
        nodeMapButton.getProperties().set ("smallButton", true);
        nodeMapButton.setTooltip ("Open the interactive routing map for the current bank. Click blocks or routes to inspect what they contribute.");
        nodeMapButton.onClick = [this]
        {
            if (onNodeMapRequested)
                onNodeMapRequested();
        };
        addAndMakeVisible (nodeMapButton);
    }

    void DspPage::BuilderPanel::setContent (const juce::String& sectionName,
                                            const juce::String& description,
                                            juce::StringArray cardLabels,
                                            juce::StringArray descriptions,
                                            juce::Array<int> itemIds,
                                            juce::Array<bool> enabledFlags)
    {
        title.setText (sectionName, juce::dontSendNotification);
        subtitle.setText (description, juce::dontSendNotification);
        cards = std::move (cardLabels);
        cardDescriptions = std::move (descriptions);
        cardItemIds = std::move (itemIds);
        cardEnabled = std::move (enabledFlags);
        for (int i = selectedItemIds.size(); --i >= 0;)
            if (! cardItemIds.contains (selectedItemIds[i]))
                selectedItemIds.remove (i);
        if (selectedItemId != 0 && ! cardItemIds.contains (selectedItemId))
            selectedItemId = 0;
        repaint();
    }

    void DspPage::BuilderPanel::setSectionBankState (bool shouldShow, int activeBank, const juce::Array<int>& counts)
    {
        showSectionBanks = shouldShow;
        activeSectionBank = juce::jlimit (0, kSourceMatrixBankCount - 1, activeBank);
        sectionBankCounts = counts;
        while (sectionBankCounts.size() < kSourceMatrixBankCount)
            sectionBankCounts.add (0);

        int bank = 0;
        for (auto* button : { &sectionBankButton1, &sectionBankButton2, &sectionBankButton3, &sectionBankButton4 })
        {
            button->setButtonText ("Bank " + juce::String (bank + 1)
                                   + "  " + juce::String (sectionBankCounts[bank]) + "/6");
            button->setToggleState (activeSectionBank == bank, juce::dontSendNotification);
            button->setVisible (showSectionBanks);
            ++bank;
        }
        nodeMapButton.setVisible (shouldShow);
        mixerButton.setVisible (shouldShow);
        resized();
        repaint();
    }

    void DspPage::BuilderPanel::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced (1).toFloat();
        g.setColour (juce::Colour (0xff0b0d11));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
        g.fillRoundedRectangle (bounds.withHeight (2.0f), 2.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.35f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.4f);
        auto r = getLocalBounds().reduced (14);
        r.removeFromTop (96);
        if (showSectionBanks)
        {
            auto hint = r.removeFromTop (28).reduced (4, 0);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.5f);
            g.drawText ("Each module has 4 banks. Each bank holds up to 6 blocks; Node Map shows the selected bank.",
                        hint.removeFromRight (juce::jmin (360, hint.getWidth())),
                        juce::Justification::centredRight, true);
            r.removeFromTop (4);
        }

        const int columns = juce::jmax (1, juce::jmin (3, r.getWidth() / 260));
        const int cardW = r.getWidth() / columns;
        const int cardH = 86;

        if (cards.isEmpty())
        {
            auto empty = r.removeFromTop (76).reduced (5);
            g.setColour (PatchCraftLookAndFeel::panelAlt());
            g.fillRoundedRectangle (empty.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (empty.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.5f);
            g.drawText ("No blocks in this bank. Use + Block or presets; edit macros, routes, and automation in Graph Inspector or Mod Matrix.",
                        empty.reduced (12), juce::Justification::centredLeft, true);
            return;
        }

        for (int i = 0; i < cards.size(); ++i)
        {
            const int row = i / columns;
            const int col = i % columns;
            auto card = juce::Rectangle<int> (r.getX() + col * cardW,
                                              r.getY() + row * (cardH + 10),
                                              cardW, cardH).reduced (5);
            const bool selected = i < cardItemIds.size() && selectedItemIds.contains (cardItemIds[i]);
            const bool enabled = i >= cardEnabled.size() || cardEnabled[i];
            g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.22f)
                                  : enabled ? juce::Colour (0xff171b21)
                                            : juce::Colour (0xff171b21).withAlpha (0.48f));
            g.fillRoundedRectangle (card.toFloat(), 5.0f);
            g.setColour (selected ? PatchCraftLookAndFeel::accent()
                                  : enabled ? PatchCraftLookAndFeel::border().brighter (0.45f)
                                            : PatchCraftLookAndFeel::border().withAlpha (0.45f));
            g.drawRoundedRectangle (card.toFloat(), 5.0f, selected ? 2.0f : 1.25f);

            g.setColour (enabled ? PatchCraftLookAndFeel::accent()
                                 : PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            auto titleArea = card.reduced (10).removeFromTop (20);
            g.drawText (cards[i], titleArea, juce::Justification::centredLeft);
            if (! enabled)
            {
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText ("BYPASSED", titleArea.removeFromRight (64), juce::Justification::centredRight);
            }

            g.setColour (enabled ? PatchCraftLookAndFeel::textDim()
                                 : PatchCraftLookAndFeel::textDim().withAlpha (0.55f));
            g.setFont (10.5f);
            const auto body = i < cardDescriptions.size()
                ? cardDescriptions[i]
                : juce::String ("Click to select. Then edit type, source, target, amount, rate, range, curve, or delete.");
            g.drawText (body,
                        card.reduced (10).withTrimmedTop (26),
                        juce::Justification::topLeft, true);
        }
    }

    void DspPage::BuilderPanel::mouseDown (const juce::MouseEvent& e)
    {
        auto r = getLocalBounds().reduced (14);
        r.removeFromTop (96);
        if (showSectionBanks)
            r.removeFromTop (32);
        const int columns = juce::jmax (1, juce::jmin (3, r.getWidth() / 260));
        const int cardW = r.getWidth() / columns;
        const int cardH = 86;

        for (int i = 0; i < cards.size() && i < cardItemIds.size(); ++i)
        {
            const int row = i / columns;
            const int col = i % columns;
            auto card = juce::Rectangle<int> (r.getX() + col * cardW,
                                              r.getY() + row * (cardH + 10),
                                              cardW, cardH).reduced (5);
            if (card.contains (e.getPosition()))
            {
                const bool multi = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown();
                const auto itemId = cardItemIds[i];
                if (multi)
                {
                    if (selectedItemIds.contains (itemId))
                        selectedItemIds.removeFirstMatchingValue (itemId);
                    else
                        selectedItemIds.addIfNotAlreadyThere (itemId);
                    selectedItemId = selectedItemIds.isEmpty() ? 0 : selectedItemIds.getLast();
                }
                else
                {
                    selectedItemIds.clear();
                    selectedItemIds.add (itemId);
                    selectedItemId = itemId;
                }
                if (onCardSelected)
                    onCardSelected (selectedItemId, multi);
                repaint();
                return;
            }
        }
    }

    void DspPage::BuilderPanel::resized()
    {
        auto r = getLocalBounds().reduced (14);
        auto header = r.removeFromTop (76);
        auto top = header.removeFromTop (36);
        auto actions = header.removeFromTop (36);

        title.setBounds (top.removeFromLeft (180));
        expansionBox.setBounds (top.removeFromLeft (172).reduced (2));
        packCreatorButton.setBounds (top.removeFromLeft (104).reduced (2));

        clearAllButton.setBounds (top.removeFromRight (74).reduced (2));
        clearSectionButton.setBounds (top.removeFromRight (78).reduced (2));
        importSampleButton.setBounds (top.removeFromRight (104).reduced (2));
        sendExpansionButton.setBounds (top.removeFromRight (92).reduced (2));
        saveSectionPresetButton.setBounds (top.removeFromRight (104).reduced (2));
        savePatchAsButton.setBounds (top.removeFromRight (104).reduced (2));
        savePatchButton.setBounds (top.removeFromRight (86).reduced (2));

        addBlockButton.setBounds (actions.removeFromLeft (78).reduced (2));
        addMacroButton.setBounds (actions.removeFromLeft (86).reduced (2));
        addModButton.setBounds (actions.removeFromLeft (72).reduced (2));
        if (addArpButton.isVisible())
            addArpButton.setBounds (actions.removeFromLeft (78).reduced (2));
        else
            addArpButton.setBounds ({});
        addAutomationButton.setBounds (actions.removeFromLeft (124).reduced (2));
        mixerButton.setBounds (actions.removeFromLeft (78).reduced (2));
        openSectionEditorButton.setBounds (actions.removeFromLeft (112).reduced (2));

        subtitle.setBounds (r.removeFromTop (20));
        if (showSectionBanks)
        {
            auto banks = r.removeFromTop (28);
            nodeMapButton.setBounds (banks.removeFromRight (86).reduced (2));
            auto left = banks.removeFromLeft (juce::jmin (380, banks.getWidth()));
            const int bankW = juce::jmax (1, left.getWidth() / kSourceMatrixBankCount);
            sectionBankButton1.setBounds (left.removeFromLeft (bankW).reduced (2));
            sectionBankButton2.setBounds (left.removeFromLeft (bankW).reduced (2));
            sectionBankButton3.setBounds (left.removeFromLeft (bankW).reduced (2));
            sectionBankButton4.setBounds (left.reduced (2));
        }
    }

    DspPage::FxWaveformView::FxWaveformView (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void DspPage::FxWaveformView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().reduced (8, 6);
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);

        if (owner.fxWaveformPeaks.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawText ("Import a sample to test the FX chain here.", r.reduced (10), juce::Justification::centredLeft, true);
            return;
        }

        auto wave = r.reduced (10, 8);
        const auto loopStartX = wave.getX() + juce::roundToInt (owner.fxLoopStart01.load() * (float) wave.getWidth());
        const auto loopEndX = wave.getX() + juce::roundToInt (owner.fxLoopEnd01.load() * (float) wave.getWidth());
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.10f));
        g.fillRect (juce::Rectangle<int> (loopStartX, wave.getY(),
                                          juce::jmax (1, loopEndX - loopStartX), wave.getHeight()));
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.72f));
        const int count = (int) owner.fxWaveformPeaks.size();
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const int index = juce::jlimit (0, count - 1, x * count / juce::jmax (1, wave.getWidth()));
            const float peak = juce::jlimit (0.0f, 1.0f, owner.fxWaveformPeaks[(size_t) index]);
            const int h = juce::roundToInt (peak * (float) wave.getHeight() * 0.5f);
            g.drawVerticalLine (wave.getX() + x, (float) wave.getCentreY() - h, (float) wave.getCentreY() + h);
        }

        const int playX = wave.getX() + juce::roundToInt (owner.getFxPlaybackPosition01() * (float) wave.getWidth());
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.8f));
        g.drawVerticalLine (loopStartX, (float) wave.getY(), (float) wave.getBottom());
        g.drawVerticalLine (loopEndX, (float) wave.getY(), (float) wave.getBottom());
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) loopStartX - 4.0f, (float) wave.getY(), 8.0f, 12.0f), 3.0f);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) loopEndX - 4.0f, (float) wave.getY(), 8.0f, 12.0f), 3.0f);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.0f);
        g.drawText ("Loop", juce::Rectangle<int> (loopStartX + 6, wave.getY(), 42, 14), juce::Justification::centredLeft, true);
        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.drawLine ((float) playX, (float) wave.getY(), (float) playX, (float) wave.getBottom(), 2.0f);
    }

    juce::Rectangle<int> DspPage::FxWaveformView::waveformBounds() const
    {
        return getLocalBounds().reduced (8, 6).reduced (10, 8);
    }

    float DspPage::FxWaveformView::xToPosition01 (int x) const
    {
        const auto wave = waveformBounds();
        return wave.getWidth() > 0 ? juce::jlimit (0.0f, 1.0f, (float) (x - wave.getX()) / (float) wave.getWidth()) : 0.0f;
    }

    void DspPage::FxWaveformView::mouseDown (const juce::MouseEvent& e)
    {
        if (owner.fxWaveformPeaks.empty())
            return;

        const auto wave = waveformBounds();
        if (! wave.contains (e.position.toInt()))
            return;

        dragAnchor01 = xToPosition01 (e.x);
        dragStart01 = owner.fxLoopStart01.load();
        dragEnd01 = owner.fxLoopEnd01.load();

        const int startX = wave.getX() + juce::roundToInt (dragStart01 * (float) wave.getWidth());
        const int endX = wave.getX() + juce::roundToInt (dragEnd01 * (float) wave.getWidth());
        if (std::abs (e.x - startX) <= 8)
            dragMode = DragMode::loopStart;
        else if (std::abs (e.x - endX) <= 8)
            dragMode = DragMode::loopEnd;
        else if (e.mods.isCtrlDown() && dragAnchor01 >= dragStart01 && dragAnchor01 <= dragEnd01)
            dragMode = DragMode::loopRegion;
        else
            dragMode = DragMode::playhead;

        updateFromMouse (e);
    }

    void DspPage::FxWaveformView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (owner.fxWaveformPeaks.empty())
            return;

        dragMode = DragMode::playhead;
        updateFromMouse (e);
        dragMode = DragMode::none;
        owner.retriggerFxSamplePlayback (true);
    }

    void DspPage::FxWaveformView::mouseDrag (const juce::MouseEvent& e)
    {
        updateFromMouse (e);
    }

    void DspPage::FxWaveformView::mouseUp (const juce::MouseEvent&)
    {
        dragMode = DragMode::none;
    }

    void DspPage::FxWaveformView::updateFromMouse (const juce::MouseEvent& e)
    {
        const auto length = owner.fxSampleLength.load();
        if (length <= 0 || dragMode == DragMode::none)
            return;

        const auto position01 = xToPosition01 (e.x);
        if (dragMode == DragMode::playhead)
        {
            owner.fxPlayhead.store (juce::jlimit (0, juce::jmax (0, length - 1),
                juce::roundToInt (position01 * (float) length)));
            repaint();
            return;
        }

        if (dragMode == DragMode::loopStart)
            owner.setFxLoopRegion (position01, owner.fxLoopEnd01.load(), true);
        else if (dragMode == DragMode::loopEnd)
            owner.setFxLoopRegion (owner.fxLoopStart01.load(), position01, true);
        else if (dragMode == DragMode::loopRegion)
        {
            const auto width = dragEnd01 - dragStart01;
            auto start = dragStart01 + (position01 - dragAnchor01);
            start = juce::jlimit (0.0f, 1.0f - width, start);
            owner.setFxLoopRegion (start, start + width, true);
        }
    }

    DspPage::SourceMatrixPanel::SourceMatrixPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<int> DspPage::SourceMatrixPanel::tableBounds() const
    {
        return getLocalBounds().reduced (10, 8);
    }

    std::vector<int> DspPage::SourceMatrixPanel::allSectionBlockIndices() const
    {
        std::vector<int> indices;
        const auto& blocks = owner.project.getDspGraph().blocks;
        for (int i = 0; i < (int) blocks.size(); ++i)
            if (blocks[(size_t) i].section == owner.currentSectionId())
                indices.push_back (i);
        return indices;
    }

    std::vector<int> DspPage::SourceMatrixPanel::sectionBlockIndices() const
    {
        const auto allIndices = allSectionBlockIndices();
        std::vector<int> indices;
        const int bank = owner.currentSectionBank();
        for (int ordinal = 0; ordinal < (int) allIndices.size(); ++ordinal)
        {
            const auto& block = owner.project.getDspGraph().blocks[(size_t) allIndices[(size_t) ordinal]];
            if (bankForBlockOrdinal (block, ordinal) == bank)
                indices.push_back (allIndices[(size_t) ordinal]);
        }
        return indices;
    }

    int DspPage::SourceMatrixPanel::columnAt (int x) const
    {
        auto r = tableBounds();
        const int nameW = juce::jlimit (120, 220, r.getWidth() / 4);
        r.removeFromLeft (nameW);
        if (r.isEmpty() || x < r.getX() || x > r.getRight())
            return -1;
        const int colW = juce::jmax (1, r.getWidth() / 5);
        return juce::jlimit (0, 4, (x - r.getX()) / colW);
    }

    int DspPage::SourceMatrixPanel::blockAt (int y) const
    {
        auto r = tableBounds();
        r.removeFromTop (22);
        const auto indices = sectionBlockIndices();
        if (indices.empty() || y < r.getY() || y > r.getBottom())
            return -1;
        const int rowH = juce::jlimit (28, 52, r.getHeight() / juce::jmax (1, kSourceMatrixBankSize));
        const int row = (y - r.getY()) / rowH;
        return row >= 0 && row < (int) indices.size() ? indices[(size_t) row] : -1;
    }

    void DspPage::SourceMatrixPanel::selectBlock (int blockIndex)
    {
        if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
            return;

        const auto section = owner.project.getDspGraph().blocks[(size_t) blockIndex].section;
        const int targetTab = section == "source" ? 0
                            : section == "filter" ? 1
                            : section == "amp"    ? 2
                            : section == "mod"    ? 3
                            : section == "fx"     ? 4 : 5;
        if (owner.currentTab != targetTab)
            owner.setTab (targetTab);

        owner.selectedGraphKind = 1;
        owner.selectedGraphIndex = blockIndex;
        owner.builderPanel.selectedItemIds.clear();
        owner.builderPanel.selectedItemId = 1000 + blockIndex + 1;
        owner.builderPanel.selectedItemIds.add (owner.builderPanel.selectedItemId);
        owner.editorItemBox.setSelectedId (owner.builderPanel.selectedItemId, juce::sendNotification);
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        repaint();
    }

    void DspPage::SourceMatrixPanel::setBlockValueFromMouse (const juce::MouseEvent& e)
    {
        if (dragBlockIndex < 0 || dragBlockIndex >= (int) owner.project.getDspGraph().blocks.size() || dragColumn < 0)
            return;

        auto r = tableBounds();
        const int nameW = juce::jlimit (120, 220, r.getWidth() / 4);
        r.removeFromLeft (nameW);
        const int colW = juce::jmax (1, r.getWidth() / 5);
        const int cellX = r.getX() + dragColumn * colW;
        const float value = juce::jlimit (0.0f, 1.0f, (float) (e.x - cellX) / (float) colW);

        auto& block = owner.project.getDspGraph().blocks[(size_t) dragBlockIndex];
        setBlockMixerValue (block, dragColumn, value);

        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        owner.surgicalEqPanel.repaint();
        repaint();
    }

    void DspPage::SourceMatrixPanel::paint (juce::Graphics& g)
    {
        auto outer = getLocalBounds().reduced (2);
        g.setColour (juce::Colour (0xff0b0d11));
        g.fillRoundedRectangle (outer.toFloat(), 6.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.70f));
        g.fillRoundedRectangle (outer.withHeight (2).toFloat(), 2.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.40f));
        g.drawRoundedRectangle (outer.toFloat(), 6.0f, 1.35f);

        auto r = tableBounds();
        auto header = r.removeFromTop (22);
        const int nameW = juce::jlimit (120, 220, header.getWidth() / 4);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText (owner.getCurrentPatchSectionLabel().toUpperCase() + " MIXER - BANK "
                    + juce::String (owner.currentSectionBank() + 1),
                    header.removeFromLeft (nameW), juce::Justification::centredLeft);

        const auto labels = mixerLabelsForSection (owner.currentSectionId());
        const int colW = juce::jmax (1, header.getWidth() / labels.size());
        for (auto& label : labels)
            g.drawText (label, header.removeFromLeft (colW), juce::Justification::centred);

        const auto indices = sectionBlockIndices();
        if (indices.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawText ("Add a " + owner.getCurrentPatchSectionLabel() + " block to create editable channels in this bank.",
                        r, juce::Justification::centredLeft, true);
            return;
        }

        const int rowH = juce::jlimit (28, 52, r.getHeight() / juce::jmax (1, kSourceMatrixBankSize));
        for (int rowIndex = 0; rowIndex < (int) indices.size(); ++rowIndex)
        {
            const int blockIndex = indices[(size_t) rowIndex];
            const auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
            auto row = r.removeFromTop (rowH).reduced (0, 2);
            const bool selected = owner.selectedGraphKind == 1 && owner.selectedGraphIndex == blockIndex;
            g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.18f)
                                  : juce::Colour (0xff171b21).withAlpha (block.enabled ? 0.92f : 0.42f));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border().brighter (0.35f).withAlpha (0.85f));
            g.drawRoundedRectangle (row.toFloat(), 4.0f, selected ? 1.5f : 1.0f);

            auto nameArea = row.removeFromLeft (nameW).reduced (8, 0);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.setColour (block.enabled ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            g.drawText (block.name, nameArea.removeFromTop (row.getHeight() / 2), juce::Justification::centredLeft);
            g.setFont (9.5f);
            g.drawText (block.type + " -> " + block.targetId, nameArea, juce::Justification::centredLeft);

            const int valueColW = juce::jmax (1, row.getWidth() / 5);
            for (int col = 0; col < 5; ++col)
            {
                const float value = blockMixerValue (block, col);
                auto cell = row.removeFromLeft (valueColW).reduced (7, 7);
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.65f));
                g.drawRoundedRectangle (cell.toFloat(), 3.0f, 1.0f);
                g.setColour ((col == 4 && value < 0.5f) ? PatchCraftLookAndFeel::textDim()
                                                              : PatchCraftLookAndFeel::accent());
                auto fill = cell.withWidth (juce::roundToInt ((float) cell.getWidth() * value));
                g.fillRoundedRectangle (fill.toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (9.5f);
                g.drawText (col == 4 ? (value >= 0.5f ? "ON" : "OFF")
                                     : juce::String (juce::roundToInt (value * 100.0f)) + "%",
                            cell, juce::Justification::centred);
            }
        }
    }

    void DspPage::SourceMatrixPanel::mouseDown (const juce::MouseEvent& e)
    {
        dragBlockIndex = blockAt (e.y);
        dragColumn = columnAt (e.x);
        draggingValue = false;
        if (dragBlockIndex >= 0)
            selectBlock (dragBlockIndex);
        if (dragBlockIndex >= 0 && dragColumn >= 0)
        {
            draggingValue = true;
            setBlockValueFromMouse (e);
        }
    }

    void DspPage::SourceMatrixPanel::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const auto blockIndex = blockAt (e.y);
        if (blockIndex < 0)
            return;

        selectBlock (blockIndex);
        owner.showBlockEditorPopout (blockIndex);
    }

    void DspPage::SourceMatrixPanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingValue)
            setBlockValueFromMouse (e);
    }

    void DspPage::SourceMatrixPanel::mouseUp (const juce::MouseEvent&)
    {
        dragBlockIndex = -1;
        dragColumn = -1;
        draggingValue = false;
    }

    DspPage::SurgicalEqPanel::SurgicalEqPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<int> DspPage::SurgicalEqPanel::graphBounds() const
    {
        return getLocalBounds().reduced (12, 10).withTrimmedTop (40).withTrimmedBottom (26);
    }

    std::vector<int> DspPage::SurgicalEqPanel::eqBlockIndices() const
    {
        std::vector<int> indices;
        const auto& blocks = owner.project.getDspGraph().blocks;
        int filterOrdinal = 0;
        for (int i = 0; i < (int) blocks.size(); ++i)
        {
            const auto& block = blocks[(size_t) i];
            if (block.section != "filter")
                continue;

            if (bankForBlockOrdinal (block, filterOrdinal) == owner.currentSectionBank()
                && block.type.containsIgnoreCase ("eq"))
                indices.push_back (i);

            ++filterOrdinal;
        }
        return indices;
    }

    int DspPage::SurgicalEqPanel::nodeAt (juce::Point<int> point) const
    {
        const auto graph = graphBounds();
        if (! graph.expanded (14).contains (point))
            return -1;

        const auto indices = eqBlockIndices();
        int bestIndex = -1;
        float bestDistance = 9999.0f;
        for (int blockIndex : indices)
        {
            const auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
            const auto x = frequencyToX (graph, valueForBlockKey (block, "eqFreq", 1000.0f));
            const auto y = eqNodeY (graph, block);
            const auto distance = juce::Point<float> ((float) point.x, (float) point.y).getDistanceFrom ({ x, y });
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = blockIndex;
            }
        }

        return bestDistance <= 16.0f ? bestIndex : -1;
    }

    bool DspPage::SurgicalEqPanel::isBlockSelected (int blockIndex) const
    {
        return owner.builderPanel.selectedItemIds.contains (1000 + blockIndex + 1);
    }

    void DspPage::SurgicalEqPanel::selectBlock (int blockIndex, bool additive)
    {
        if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
            return;

        const int itemId = 1000 + blockIndex + 1;
        owner.selectedGraphKind = 1;
        if (! additive)
        {
            owner.builderPanel.selectedItemIds.clear();
            owner.builderPanel.selectedItemIds.add (itemId);
        }
        else if (owner.builderPanel.selectedItemIds.contains (itemId))
        {
            owner.builderPanel.selectedItemIds.removeFirstMatchingValue (itemId);
        }
        else
        {
            owner.builderPanel.selectedItemIds.addIfNotAlreadyThere (itemId);
        }

        if (owner.builderPanel.selectedItemIds.isEmpty())
            owner.builderPanel.selectedItemIds.add (itemId);

        owner.builderPanel.selectedItemId = owner.builderPanel.selectedItemIds.getLast();
        owner.selectedGraphIndex = owner.builderPanel.selectedItemId - 1001;
        owner.editorItemBox.setSelectedId (owner.builderPanel.selectedItemId, juce::sendNotification);
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        owner.sourceMatrix.repaint();
        repaint();
    }

    void DspPage::SurgicalEqPanel::selectBlocksInMarquee()
    {
        const auto graph = graphBounds();
        const auto area = marquee.getIntersection (graph.expanded (12));
        if (area.getWidth() < 4 || area.getHeight() < 4)
            return;

        owner.builderPanel.selectedItemIds.clear();
        for (int blockIndex : eqBlockIndices())
        {
            const auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
            const auto x = frequencyToX (graph, valueForBlockKey (block, "eqFreq", 1000.0f));
            const auto y = eqNodeY (graph, block);
            if (area.contains (juce::roundToInt (x), juce::roundToInt (y)))
                owner.builderPanel.selectedItemIds.addIfNotAlreadyThere (1000 + blockIndex + 1);
        }

        if (owner.builderPanel.selectedItemIds.isEmpty())
            return;

        owner.selectedGraphKind = 1;
        owner.builderPanel.selectedItemId = owner.builderPanel.selectedItemIds.getLast();
        owner.selectedGraphIndex = owner.builderPanel.selectedItemId - 1001;
        owner.editorItemBox.setSelectedId (owner.builderPanel.selectedItemId, juce::sendNotification);
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        owner.sourceMatrix.repaint();
        repaint();
    }

    void DspPage::SurgicalEqPanel::beginGroupDrag (int blockIndex, juce::Point<int> point)
    {
        dragStartPoint = point;
        dragStartValues.clear();

        if (! isBlockSelected (blockIndex))
            selectBlock (blockIndex, false);

        for (int index : eqBlockIndices())
        {
            if (! isBlockSelected (index))
                continue;

            const auto& block = owner.project.getDspGraph().blocks[(size_t) index];
            const int type = eqTypeForBlock (block);
            dragStartValues[index] = {
                valueForBlockKey (block, "eqFreq", 1000.0f),
                type >= 3 ? valueForBlockKey (block, "eqQ", 1.0f)
                          : valueForBlockKey (block, "eqGainDb", 0.0f)
            };
        }
    }

    void DspPage::SurgicalEqPanel::updateGroupDrag (juce::Point<int> point)
    {
        if (dragStartValues.empty())
        {
            updateBlockFromPoint (dragBlockIndex, point);
            return;
        }

        const auto graph = graphBounds();
        const float dx = (float) (point.x - dragStartPoint.x) / (float) juce::jmax (1, graph.getWidth());
        const float dy = (float) (point.y - dragStartPoint.y) / (float) juce::jmax (1, graph.getHeight());

        for (const auto& entry : dragStartValues)
        {
            const int blockIndex = entry.first;
            if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
                continue;

            auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
            block.values["eqFreq"] = normalisedToFrequency (frequencyToNormalised (entry.second.first) + dx);

            const int type = eqTypeForBlock (block);
            if (type >= 3)
                block.values["eqQ"] = normalisedToEqQ (eqQToNormalised (entry.second.second) - dy);
            else
                block.values["eqGainDb"] = juce::jlimit (-24.0f, 24.0f, entry.second.second - dy * 48.0f);

            const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
            block.targetId = "eqBand" + juce::String (band) + "Freq";
            block.values["eqOn"] = 1.0f;
        }

        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.syncGraphEditor();
        owner.sourceMatrix.repaint();
        owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        repaint();
    }

    void DspPage::SurgicalEqPanel::updateBlockFromPoint (int blockIndex, juce::Point<int> point)
    {
        if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
            return;

        const auto graph = graphBounds();
        auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
        block.values["eqFreq"] = xToEqFrequency (graph, point.x);

        const int type = eqTypeForBlock (block);
        if (type >= 3)
            block.values["eqQ"] = yToEqQ (graph, point.y);
        else
            block.values["eqGainDb"] = yToEqDb (graph, point.y);

        const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
        block.targetId = "eqBand" + juce::String (band) + "Freq";
        block.values["eqOn"] = 1.0f;

        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.syncGraphEditor();
        owner.sourceMatrix.repaint();
        owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        repaint();
    }

    void DspPage::SurgicalEqPanel::createBlockAt (juce::Point<int> point)
    {
        const auto graph = graphBounds();
        if (! graph.contains (point))
            return;

        int bankCount = 0;
        bool usedBands[9] {};
        int filterOrdinal = 0;
        for (const auto& block : owner.project.getDspGraph().blocks)
        {
            if (block.section != "filter")
                continue;

            const int bank = bankForBlockOrdinal (block, filterOrdinal);
            if (bank == owner.currentSectionBank())
            {
                ++bankCount;
                if (block.type.containsIgnoreCase ("eq"))
                {
                    const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
                    usedBands[band] = true;
                }
            }

            ++filterOrdinal;
        }

        if (bankCount >= kSourceMatrixBankSize)
        {
            owner.editorHint.setText ("This Filter bank is full. Switch banks or delete a block before adding another EQ node.",
                                      juce::dontSendNotification);
            return;
        }

        int band = 1;
        while (band <= 8 && usedBands[band])
            ++band;
        if (band > 8)
            band = juce::jlimit (1, 8, bankCount + 1);

        DspBlock block;
        block.section = "filter";
        block.type = "surgicalEq";
        block.id = "eq_band_" + juce::String ((int) owner.project.getDspGraph().blocks.size() + 1);
        block.name = "EQ Band " + juce::String (band);
        block.targetId = "eqBand" + juce::String (band) + "Freq";
        block.values["bank"] = (float) owner.currentSectionBank();
        block.values["eqBand"] = (float) band;
        block.values["eqOn"] = 1.0f;
        block.values["eqType"] = 0.0f;
        block.values["eqMode"] = 0.0f;
        block.values["eqFreq"] = xToEqFrequency (graph, point.x);
        block.values["eqGainDb"] = yToEqDb (graph, point.y);
        block.values["eqQ"] = 1.0f;
        block.values["eqMix"] = 1.0f;
        block.values["eqSolo"] = 0.0f;

        auto& blocks = owner.project.getDspGraph().blocks;
        blocks.push_back (block);
        owner.markGraphEdited();
        selectBlock ((int) blocks.size() - 1, false);
        owner.rebuildGraphEditorItems();
        owner.refreshFxPreviewRouting();
        owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
    }

    void DspPage::SurgicalEqPanel::paint (juce::Graphics& g)
    {
        auto outer = getLocalBounds().reduced (2);
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRoundedRectangle (outer.toFloat(), 7.0f);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (outer.toFloat(), 7.0f, 1.0f);

        auto header = getLocalBounds().reduced (12, 8).removeFromTop (32);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText ("SURGICAL EQ EDITOR - BANK " + juce::String (owner.currentSectionBank() + 1),
                    header.removeFromLeft (210), juce::Justification::centredLeft);
        g.setFont (10.5f);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.drawText ("Double-click empty space to add a Bell band. Drag Bell/Shelf vertically for gain; HP/LP/Notch vertically for Q.",
                    header, juce::Justification::centredLeft, true);

        const auto graph = graphBounds();
        g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.72f));
        g.fillRoundedRectangle (graph.toFloat(), 5.0f);

        const int dbLines[] { -24, -12, 0, 12, 24 };
        for (int db : dbLines)
        {
            const auto y = eqDbToY (graph, (float) db);
            g.setColour (db == 0 ? PatchCraftLookAndFeel::accent().withAlpha (0.28f)
                                 : PatchCraftLookAndFeel::border().withAlpha (0.35f));
            g.drawHorizontalLine (juce::roundToInt (y), (float) graph.getX(), (float) graph.getRight());
            g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.72f));
            g.setFont (9.0f);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db) + "dB",
                        graph.withY (juce::roundToInt (y) - 8).withHeight (16).withWidth (44).reduced (3, 0),
                        juce::Justification::centredLeft);
        }

        const float freqLines[] { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        for (float freq : freqLines)
        {
            const auto x = frequencyToX (graph, freq);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.25f));
            g.drawVerticalLine (juce::roundToInt (x), (float) graph.getY(), (float) graph.getBottom());
            g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.68f));
            g.setFont (8.8f);
            g.drawText (formatEqFrequency (freq), juce::Rectangle<int> (juce::roundToInt (x) - 20, graph.getBottom() + 3, 40, 14),
                        juce::Justification::centred);
        }

        if (owner.eqAnalyzerToggle.getToggleState())
        {
            bool hasEnergy = false;
            for (float bin : owner.eqAnalyzerDisplay)
                hasEnergy = hasEnergy || bin > 0.012f;

            if (hasEnergy)
            {
                juce::Path analyzerFill;
                juce::Path analyzerLine;
                analyzerFill.startNewSubPath ((float) graph.getX(), (float) graph.getBottom());
                for (int i = 0; i < DspPage::kEqAnalyzerDisplayBins; ++i)
                {
                    const float x01 = (float) i / (float) juce::jmax (1, DspPage::kEqAnalyzerDisplayBins - 1);
                    const float x = graph.getX() + x01 * (float) graph.getWidth();
                    const float y = graph.getBottom() - owner.eqAnalyzerDisplay[(size_t) i] * (float) graph.getHeight();
                    if (i == 0)
                        analyzerLine.startNewSubPath (x, y);
                    else
                        analyzerLine.lineTo (x, y);
                    analyzerFill.lineTo (x, y);
                }
                analyzerFill.lineTo ((float) graph.getRight(), (float) graph.getBottom());
                analyzerFill.closeSubPath();

                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (owner.eqAnalyzerFreezeToggle.getToggleState() ? 0.18f : 0.12f));
                g.fillPath (analyzerFill);
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (owner.eqAnalyzerFreezeToggle.getToggleState() ? 0.62f : 0.38f));
                g.strokePath (analyzerLine, juce::PathStrokeType (1.2f));
            }
            else
            {
                g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.52f));
                g.setFont (10.5f);
                g.drawText ("Analyzer idle: load/play an FX sample or enable Live Input, then return to Filter.",
                            graph.reduced (12).removeFromTop (18), juce::Justification::centredRight);
            }
        }

        const auto indices = eqBlockIndices();
        juce::Path response;
        const int samples = juce::jmax (2, graph.getWidth());
        for (int i = 0; i < samples; ++i)
        {
            const float x01 = (float) i / (float) (samples - 1);
            const float frequency = normalisedToFrequency (x01);
            float db = 0.0f;
            for (int blockIndex : indices)
                db += eqDisplayDbAt (owner.project.getDspGraph().blocks[(size_t) blockIndex], frequency);

            db = juce::jlimit (-24.0f, 24.0f, db);
            const float x = graph.getX() + x01 * (float) graph.getWidth();
            const float y = eqDbToY (graph, db);
            if (i == 0) response.startNewSubPath (x, y);
            else        response.lineTo (x, y);
        }

        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.95f));
        g.strokePath (response, juce::PathStrokeType (2.2f));

        if (indices.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawFittedText ("No surgical EQ bands in this bank yet. Double-click the graph to add one, or choose Filter preset: Surgical Cleanup EQ.",
                              graph.reduced (28), juce::Justification::centred, 3);
        }

        for (int blockIndex : indices)
        {
            const auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
            const bool selected = owner.selectedGraphKind == 1 && isBlockSelected (blockIndex);
            const int type = eqTypeForBlock (block);
            const float frequency = valueForBlockKey (block, "eqFreq", 1000.0f);
            const float x = frequencyToX (graph, frequency);
            const float y = eqNodeY (graph, block);
            const float q = valueForBlockKey (block, "eqQ", 1.0f);
            const float gain = valueForBlockKey (block, "eqGainDb", 0.0f);
            const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
            const int mode = juce::jlimit (0, 4, juce::roundToInt (valueForBlockKey (block, "eqMode", 0.0f)));

            auto colour = selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textBright();
            if (! block.enabled)
                colour = PatchCraftLookAndFeel::textDim().withAlpha (0.45f);

            g.setColour (colour.withAlpha (0.18f));
            g.fillEllipse (x - 12.0f, y - 12.0f, 24.0f, 24.0f);
            g.setColour (colour);
            g.fillEllipse (x - 5.5f, y - 5.5f, 11.0f, 11.0f);
            g.drawEllipse (x - 10.0f, y - 10.0f, 20.0f, 20.0f, selected ? 2.0f : 1.0f);

            juce::String detail = "B" + juce::String (band) + " " + eqTypeName (type) + " / " + eqModeName (mode) + " "
                                + formatEqFrequency (frequency) + "Hz ";
            detail += type >= 3 ? ("Q " + juce::String (q, 2))
                                : ((gain >= 0.0f ? "+" : "") + juce::String (gain, 1) + "dB");
            auto label = juce::Rectangle<int> (juce::roundToInt (x) + 8, juce::roundToInt (y) - 18, 160, 36)
                            .getIntersection (graph.expanded (0, 20));
            g.setColour (juce::Colours::black.withAlpha (selected ? 0.78f : 0.52f));
            g.fillRoundedRectangle (label.toFloat(), 5.0f);
            g.setColour (colour.withAlpha (0.9f));
            g.drawRoundedRectangle (label.toFloat(), 5.0f, selected ? 1.2f : 0.8f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (9.5f);
            g.drawFittedText (detail, label.reduced (6, 3), juce::Justification::centredLeft, 2);
        }

        if (marqueeSelecting && marquee.getWidth() > 0 && marquee.getHeight() > 0)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.10f));
            g.fillRect (marquee);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.75f));
            g.drawRect (marquee, 1);
        }

        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.82f));
        g.setFont (9.5f);
        g.drawText ("Ctrl/Shift-click or drag a marquee to select multiple bands. Drag selected bands together. Save/Insert stores one reusable EQ band in the local library.",
                    getLocalBounds().reduced (12, 5).removeFromBottom (18), juce::Justification::centredLeft);
    }

    void DspPage::SurgicalEqPanel::mouseDown (const juce::MouseEvent& e)
    {
        const auto point = e.position.roundToInt();
        dragBlockIndex = nodeAt (e.position.roundToInt());
        if (dragBlockIndex >= 0)
        {
            const bool additive = e.mods.isCtrlDown() || e.mods.isCommandDown() || e.mods.isShiftDown();
            selectBlock (dragBlockIndex, additive);
            if (! isBlockSelected (dragBlockIndex))
            {
                dragBlockIndex = -1;
                repaint();
                return;
            }
            beginGroupDrag (dragBlockIndex, point);
            return;
        }

        if (e.getNumberOfClicks() > 1)
        {
            createBlockAt (point);
            return;
        }

        if (graphBounds().contains (point))
        {
            marqueeSelecting = true;
            dragStartPoint = point;
            marquee = { point.x, point.y, 0, 0 };
            if (! (e.mods.isCtrlDown() || e.mods.isCommandDown() || e.mods.isShiftDown()))
            {
                owner.builderPanel.selectedItemIds.clear();
                owner.builderPanel.selectedItemId = 0;
                owner.selectedGraphIndex = -1;
            }
            repaint();
        }
    }

    void DspPage::SurgicalEqPanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragBlockIndex >= 0)
            updateGroupDrag (e.position.roundToInt());
        else if (marqueeSelecting)
        {
            const auto point = e.position.roundToInt();
            marquee = juce::Rectangle<int> (juce::jmin (dragStartPoint.x, point.x),
                                            juce::jmin (dragStartPoint.y, point.y),
                                            std::abs (point.x - dragStartPoint.x),
                                            std::abs (point.y - dragStartPoint.y));
            repaint();
        }
    }

    void DspPage::SurgicalEqPanel::mouseUp (const juce::MouseEvent&)
    {
        if (marqueeSelecting)
            selectBlocksInMarquee();

        dragBlockIndex = -1;
        marqueeSelecting = false;
        marquee = {};
        dragStartValues.clear();
    }

    DspPage::WavetableEditorPanel::WavetableEditorPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<int> DspPage::WavetableEditorPanel::graphBounds() const
    {
        return getLocalBounds().reduced (12, 10).withTrimmedTop (34).withTrimmedBottom (20);
    }

    DspBlock* DspPage::WavetableEditorPanel::selectedWavetableBlock() const
    {
        return const_cast<DspPage&> (owner).selectedWavetableBlock();
    }

    int DspPage::WavetableEditorPanel::pointAt (juce::Point<int> point) const
    {
        const auto graph = graphBounds();
        if (! graph.expanded (12).contains (point))
            return -1;

        const int pointCount = 32;
        const float step = (float) graph.getWidth() / (float) (pointCount - 1);
        const int index = juce::jlimit (0, pointCount - 1,
            juce::roundToInt ((float) (point.x - graph.getX()) / juce::jmax (1.0f, step)));
        return index;
    }

    void DspPage::WavetableEditorPanel::writePointFromMouse (juce::Point<int> point)
    {
        auto* block = selectedWavetableBlock();
        if (block == nullptr || dragPoint < 0)
            return;

        auto graph = graphBounds();
        const float value = juce::jlimit (-1.0f, 1.0f,
            juce::jmap ((float) point.y, (float) graph.getBottom(), (float) graph.getY(), -1.0f, 1.0f));
        block->values["wtTable"] = 8.0f;
        const int frame = juce::jlimit (0, 3, juce::roundToInt (valueForBlockKey (*block, "wtFramePosition", 0.0f)
            * (juce::jmax (1.0f, valueForBlockKey (*block, "wtFrameCount", 1.0f)) - 1.0f)));
        block->values["wtFrame" + juce::String (frame) + "Shape" + juce::String (dragPoint)] = value;
        if (frame == 0)
            block->values["wtShape" + juce::String (dragPoint)] = value;
        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.project.notifyChanged();
        repaint();
    }

    void DspPage::WavetableEditorPanel::paint (juce::Graphics& g)
    {
        auto outer = getLocalBounds().reduced (2);
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRoundedRectangle (outer.toFloat(), 7.0f);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (outer.toFloat(), 7.0f, 1.0f);

        auto header = getLocalBounds().reduced (12, 8).removeFromTop (26);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("CUSTOM WAVETABLE FRAME", header.removeFromLeft (190), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Draw directly, drag/drop WAV or AIFF, or import a cycle. Stored in the patch as WT Table: Custom.",
                    header, juce::Justification::centredLeft, true);

        const auto graph = graphBounds();
        g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.72f));
        g.fillRoundedRectangle (graph.toFloat(), 5.0f);

        for (int i = 0; i <= 4; ++i)
        {
            const float y = graph.getY() + (float) i / 4.0f * (float) graph.getHeight();
            g.setColour (i == 2 ? PatchCraftLookAndFeel::accent().withAlpha (0.28f)
                                : PatchCraftLookAndFeel::border().withAlpha (0.28f));
            g.drawHorizontalLine (juce::roundToInt (y), (float) graph.getX(), (float) graph.getRight());
        }

        auto* block = selectedWavetableBlock();
        if (block == nullptr)
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawFittedText ("Double-click here to create a Wavetable Source block, then drag points or import WAV/AIFF.",
                              graph.reduced (16), juce::Justification::centred, 3);
            return;
        }

        owner.ensureWavetableShapeDefaults (*block);

        const int frameCount = juce::jlimit (1, 4, juce::roundToInt (valueForBlockKey (*block, "wtFrameCount", 1.0f)));
        const int activeFrame = juce::jlimit (0, frameCount - 1, juce::roundToInt (valueForBlockKey (*block, "wtFramePosition", 0.0f)
            * (float) juce::jmax (0, frameCount - 1)));
        juce::Path wave;
        for (int i = 0; i < 32; ++i)
        {
            const auto key = "wtFrame" + juce::String (activeFrame) + "Shape" + juce::String (i);
            const float fallback = std::sin ((float) i / 32.0f * juce::MathConstants<float>::twoPi);
            const float value = valueForBlockKey (*block, key, fallback);
            const float x = graph.getX() + (float) i / 31.0f * (float) graph.getWidth();
            const float y = juce::jmap (value, -1.0f, 1.0f, (float) graph.getBottom(), (float) graph.getY());
            if (i == 0) wave.startNewSubPath (x, y);
            else        wave.lineTo (x, y);

            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.34f));
            g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }

        g.setColour (PatchCraftLookAndFeel::accent());
        g.strokePath (wave, juce::PathStrokeType (2.0f));
        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.8f));
        g.setFont (9.5f);
        g.drawText ("Frame " + juce::String (activeFrame + 1) + "/" + juce::String (frameCount)
                    + " - import a longer WAV to resynthesize up to 4 frames; automate WT Frame Position to scan them.",
                    getLocalBounds().reduced (12, 5).removeFromBottom (16), juce::Justification::centredLeft);
    }

    void DspPage::WavetableEditorPanel::mouseDown (const juce::MouseEvent& e)
    {
        if (selectedWavetableBlock() == nullptr && e.getNumberOfClicks() > 1)
        {
            auto& graph = owner.project.getDspGraph();
            normaliseDspGraphSectionBanks (graph, "source");
            const int targetBank = owner.currentSectionBank();
            if (countBlocksInSectionBank (graph, "source", targetBank) >= kSourceMatrixBankSize)
            {
                showBlockBankFullAlert ("Source", targetBank);
                return;
            }

            DspBlock block;
            block.section = "source";
            block.type = "wavetable";
            block.id = "wavetable_" + juce::String ((int) graph.blocks.size() + 1);
            block.name = "Wavetable Source";
            block.targetId = "wtPosition";
            block.values["bank"] = (float) targetBank;
            block.values["wtEnabled"] = 1.0f;
            block.values["wtTable"] = 8.0f;
            block.values["wtPosition"] = 0.0f;
            block.values["wtMorph"] = 0.0f;
            block.values["wtWarp"] = 0.0f;
            block.values["wtUnison"] = 1.0f;
            block.values["wtLevel"] = 1.0f;
            block.values["wtFrameCount"] = 1.0f;
            graph.blocks.push_back (block);
            owner.selectedGraphKind = 1;
            owner.selectedGraphIndex = (int) graph.blocks.size() - 1;
            owner.ensureWavetableShapeDefaults (graph.blocks.back());
            owner.markGraphEdited();
            owner.project.notifyChanged();
            owner.rebuildGraphEditorItems();
            owner.syncGraphEditor();
            repaint();
            return;
        }

        dragPoint = pointAt (e.position.roundToInt());
        writePointFromMouse (e.position.roundToInt());
    }

    void DspPage::WavetableEditorPanel::mouseDrag (const juce::MouseEvent& e)
    {
        const int point = pointAt (e.position.roundToInt());
        if (point >= 0)
            dragPoint = point;
        writePointFromMouse (e.position.roundToInt());
    }

    void DspPage::WavetableEditorPanel::mouseUp (const juce::MouseEvent&)
    {
        dragPoint = -1;
    }

    bool DspPage::WavetableEditorPanel::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& path : files)
        {
            const auto extension = juce::File (path).getFileExtension().toLowerCase();
            if (extension == ".wav" || extension == ".aif" || extension == ".aiff")
                return selectedWavetableBlock() != nullptr;
        }
        return false;
    }

    void DspPage::WavetableEditorPanel::filesDropped (const juce::StringArray& files, int, int)
    {
        for (const auto& path : files)
        {
            const juce::File file (path);
            const auto extension = file.getFileExtension().toLowerCase();
            if (file.existsAsFile() && (extension == ".wav" || extension == ".aif" || extension == ".aiff"))
            {
                owner.importWavetableShapeFromFile (file);
                return;
            }
        }
    }

    DspPage::ModMatrixPanel::ModMatrixPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<int> DspPage::ModMatrixPanel::tableBounds() const
    {
        return getLocalBounds().reduced (10, 8);
    }

    std::vector<DspPage::ModMatrixPanel::Row> DspPage::ModMatrixPanel::matrixRows() const
    {
        std::vector<Row> rows;
        const auto& graph = owner.project.getDspGraph();
        for (int i = 0; i < (int) graph.macros.size(); ++i)
            rows.push_back ({ 2, i });
        for (int i = 0; i < (int) graph.modulation.size(); ++i)
            rows.push_back ({ 3, i });
        for (int i = 0; i < (int) graph.automation.size(); ++i)
            rows.push_back ({ 4, i });
        return rows;
    }

    int DspPage::ModMatrixPanel::columnAt (int x) const
    {
        auto r = tableBounds();
        const int typeW = 86;
        r.removeFromLeft (typeW);
        if (r.isEmpty() || x < r.getX() || x > r.getRight())
            return -1;
        const int colW = juce::jmax (1, r.getWidth() / 4);
        return juce::jlimit (0, 3, (x - r.getX()) / colW);
    }

    DspPage::ModMatrixPanel::Row DspPage::ModMatrixPanel::rowAt (int y) const
    {
        auto r = tableBounds();
        r.removeFromTop (22);
        const auto rows = matrixRows();
        if (rows.empty() || y < r.getY() || y > r.getBottom())
            return {};
        const int rowH = juce::jlimit (22, 34, r.getHeight() / juce::jmax (1, (int) rows.size()));
        const int row = (y - r.getY()) / rowH;
        return row >= 0 && row < (int) rows.size() ? rows[(size_t) row] : Row {};
    }

    void DspPage::ModMatrixPanel::selectRow (Row row)
    {
        if (row.kind < 2 || row.index < 0)
            return;

        owner.selectedGraphKind = row.kind;
        owner.selectedGraphIndex = row.index;
        owner.builderPanel.selectedItemIds.clear();
        owner.builderPanel.selectedItemId = row.kind * 1000 + row.index + 1;
        owner.builderPanel.selectedItemIds.add (owner.builderPanel.selectedItemId);
        owner.editorItemBox.setSelectedId (owner.builderPanel.selectedItemId, juce::sendNotification);
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        repaint();
    }

    void DspPage::ModMatrixPanel::setRowValueFromMouse (const juce::MouseEvent& e)
    {
        if (dragRow.kind < 2 || dragRow.index < 0 || dragColumn < 0)
            return;

        auto r = tableBounds();
        const int typeW = 86;
        r.removeFromLeft (typeW);
        const int colW = juce::jmax (1, r.getWidth() / 4);
        const int cellX = r.getX() + dragColumn * colW;
        const float value = juce::jlimit (0.0f, 1.0f, (float) (e.x - cellX) / (float) colW);
        auto& graph = owner.project.getDspGraph();

        if (dragRow.kind == 3 && dragRow.index < (int) graph.modulation.size())
        {
            auto& route = graph.modulation[(size_t) dragRow.index];
            if (dragColumn == 2)
                route.amount = value;
            else if (dragColumn == 3)
                route.enabled = value >= 0.5f;
        }
        else if (dragRow.kind == 2 && dragRow.index < (int) graph.macros.size())
        {
            auto& macro = graph.macros[(size_t) dragRow.index];
            if (dragColumn == 2)
                macro.curve = juce::jmap (value, 0.05f, 4.0f);
        }
        else if (dragRow.kind == 4 && dragRow.index < (int) graph.automation.size())
        {
            auto& lane = graph.automation[(size_t) dragRow.index];
            if (dragColumn == 2)
                lane.rate = juce::jmap (value, 0.05f, 8.0f);
            else if (dragColumn == 3)
                lane.syncToTempo = value >= 0.5f;
        }
        else
        {
            return;
        }

        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        owner.project.notifyChanged();
        repaint();
    }

    void DspPage::ModMatrixPanel::paint (juce::Graphics& g)
    {
        auto outer = getLocalBounds().reduced (2);
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRoundedRectangle (outer.toFloat(), 6.0f);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (outer.toFloat(), 6.0f, 1.0f);

        auto r = tableBounds();
        auto header = r.removeFromTop (22);
        const int typeW = 86;
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText ("MOD MATRIX", header.removeFromLeft (typeW), juce::Justification::centredLeft);

        const juce::StringArray labels { "SOURCE", "TARGET", "DEPTH/RATE", "STATE" };
        const int colW = juce::jmax (1, header.getWidth() / labels.size());
        for (auto& label : labels)
            g.drawText (label, header.removeFromLeft (colW), juce::Justification::centred);

        const auto rows = matrixRows();
        if (rows.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawText ("Add + Macro, + Mod, or + Auto to build visible modulation routes.", r, juce::Justification::centredLeft, true);
            return;
        }

        const int rowH = juce::jlimit (22, 34, r.getHeight() / juce::jmax (1, (int) rows.size()));
        auto drawBar = [&g] (juce::Rectangle<int> cell, float value, juce::String text)
        {
            cell = cell.reduced (7, 7);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.65f));
            g.drawRoundedRectangle (cell.toFloat(), 3.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.fillRoundedRectangle (cell.withWidth (juce::roundToInt ((float) cell.getWidth() * juce::jlimit (0.0f, 1.0f, value))).toFloat(), 3.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (9.5f);
            g.drawText (std::move (text), cell, juce::Justification::centred);
        };

        const auto& graph = owner.project.getDspGraph();
        for (auto rowInfo : rows)
        {
            auto row = r.removeFromTop (rowH).reduced (0, 2);
            const bool selected = owner.selectedGraphKind == rowInfo.kind && owner.selectedGraphIndex == rowInfo.index;
            bool enabled = true;
            juce::String type;
            juce::String source;
            juce::String target;
            float value = 0.0f;
            juce::String valueText;
            juce::String stateText = "ACTIVE";

            // Draw connection line for visual feedback
            if (selected && (rowInfo.kind == 3 || rowInfo.kind == 4))
            {
                if (rowInfo.kind == 3)
                {
                    const auto& route = graph.modulation[(size_t) rowInfo.index];
                    if (route.enabled)
                    {
                        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.3f));
                        g.drawLine ((float) row.getCentreX(), (float) row.getCentreY(),
                                   (float) row.getRight(), (float) row.getCentreY(), 2.0f);
                    }
                }
                else
                {
                    const auto& route = graph.automation[(size_t) rowInfo.index];
                    // AutomationLane doesn't have enabled member, always draw if it exists
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.3f));
                    g.drawLine ((float) row.getCentreX(), (float) row.getCentreY(),
                               (float) row.getRight(), (float) row.getCentreY(), 2.0f);
                }
            }

            if (rowInfo.kind == 2 && rowInfo.index < (int) graph.macros.size())
            {
                const auto& macro = graph.macros[(size_t) rowInfo.index];
                type = "MACRO";
                source = macro.macroId;
                target = macro.targetId;
                value = juce::jmap (juce::jlimit (0.05f, 4.0f, macro.curve), 0.05f, 4.0f, 0.0f, 1.0f);
                valueText = "Curve " + juce::String (macro.curve, 2);
            }
            else if (rowInfo.kind == 3 && rowInfo.index < (int) graph.modulation.size())
            {
                const auto& route = graph.modulation[(size_t) rowInfo.index];
                type = "MOD";
                source = route.sourceId;
                target = route.targetId;
                value = route.amount;
                valueText = juce::String (juce::roundToInt (route.amount * 100.0f)) + "%";
                enabled = route.enabled;
                stateText = enabled ? "ON" : "OFF";
            }
            else if (rowInfo.kind == 4 && rowInfo.index < (int) graph.automation.size())
            {
                const auto& lane = graph.automation[(size_t) rowInfo.index];
                type = "AUTO";
                source = lane.mode;
                target = lane.targetId;
                value = juce::jmap (juce::jlimit (0.05f, 8.0f, lane.rate), 0.05f, 8.0f, 0.0f, 1.0f);
                valueText = juce::String (lane.rate, 2) + (lane.syncToTempo ? " beat" : " Hz");
                stateText = lane.syncToTempo ? "SYNC" : "FREE";
            }

            g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.14f)
                                  : PatchCraftLookAndFeel::panelAlt().withAlpha (enabled ? 0.80f : 0.42f));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border().withAlpha (0.75f));
            g.drawRoundedRectangle (row.toFloat(), 4.0f, selected ? 1.5f : 1.0f);

            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.setColour (enabled ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim());
            g.drawText (type, row.removeFromLeft (typeW).reduced (8, 0), juce::Justification::centredLeft);

            const int valueColW = juce::jmax (1, row.getWidth() / 4);
            g.setFont (10.5f);
            g.setColour (enabled ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            g.drawText (source, row.removeFromLeft (valueColW).reduced (8, 0), juce::Justification::centredLeft, true);
            g.drawText (target, row.removeFromLeft (valueColW).reduced (8, 0), juce::Justification::centredLeft, true);
            drawBar (row.removeFromLeft (valueColW), value, valueText);
            drawBar (row, enabled ? 1.0f : 0.0f, stateText);
        }
    }

    void DspPage::ModMatrixPanel::mouseDown (const juce::MouseEvent& e)
    {
        dragRow = rowAt (e.y);
        dragColumn = columnAt (e.x);
        draggingValue = false;
        if (dragRow.kind >= 2)
            selectRow (dragRow);
        if (dragRow.kind >= 2 && dragColumn >= 2)
        {
            draggingValue = true;
            setRowValueFromMouse (e);
        }
    }

    void DspPage::ModMatrixPanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingValue)
            setRowValueFromMouse (e);
    }

    void DspPage::ModMatrixPanel::mouseUp (const juce::MouseEvent&)
    {
        dragRow = {};
        dragColumn = -1;
        draggingValue = false;
    }

    DspPage::FormulaPanel::FormulaPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void DspPage::FormulaPanel::paint (juce::Graphics& g)
    {
        hitZones.clear();

        auto outer = getLocalBounds().reduced (2);
        g.setColour (juce::Colour (0xff0b0d11));
        g.fillRoundedRectangle (outer.toFloat(), 7.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.70f));
        g.fillRoundedRectangle (outer.withHeight (2).toFloat(), 2.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.35f));
        g.drawRoundedRectangle (outer.toFloat(), 7.0f, 1.25f);

        auto r = outer.reduced (12, 8);
        auto titleRow = r.removeFromTop (22);
        const auto& graph = owner.project.getDspGraph();
        const auto presetName = owner.project.getManifest().defaultPreset.isNotEmpty()
            ? owner.project.getManifest().defaultPreset
            : owner.project.getManifest().instrumentName;

        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("SOUND FORMULA", titleRow.removeFromLeft (128), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText ("Preset: " + presetName
                    + "  |  Blocks: " + juce::String ((int) graph.blocks.size())
                    + "  Macros: " + juce::String ((int) graph.macros.size())
                    + "  Mods: " + juce::String ((int) graph.modulation.size())
                    + "  Autos: " + juce::String ((int) graph.automation.size()),
                    titleRow, juce::Justification::centredLeft, true);

        r.removeFromTop (4);
        if (graph.blocks.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawText ("No sound formula yet. Add Source, Filter, Amp, Mod, FX, and Out blocks; this panel will become the readable recipe for the patch.",
                        r, juce::Justification::centredLeft, true);
            return;
        }

        const juce::String sections[] { "source", "filter", "amp", "mod", "fx", "out" };
        const int sectionW = juce::jmax (1, r.getWidth() / 6);
        for (int sectionIndex = 0; sectionIndex < 6; ++sectionIndex)
        {
            const auto section = sections[sectionIndex];
            auto column = juce::Rectangle<int> (r.getX() + sectionIndex * sectionW,
                                                r.getY(),
                                                sectionIndex == 5 ? r.getRight() - (r.getX() + sectionIndex * sectionW) : sectionW,
                                                r.getHeight()).reduced (4, 0);
            int count = 0;
            for (const auto& block : graph.blocks)
                if (block.section == section)
                    ++count;

            auto head = column.removeFromTop (18);
            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.86f));
            g.fillRoundedRectangle (head.toFloat(), 4.0f);
            g.setColour (sectionIndex == owner.currentTab ? PatchCraftLookAndFeel::accent()
                                                          : PatchCraftLookAndFeel::border().brighter (0.25f));
            g.drawRoundedRectangle (head.toFloat(), 4.0f, sectionIndex == owner.currentTab ? 1.4f : 1.0f);
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawText (sectionDisplayName (section) + "  " + juce::String (count), head.reduced (6, 0),
                        juce::Justification::centredLeft);

            column.removeFromTop (4);
            const int rowH = 40;
            int visibleRows = juce::jmax (1, column.getHeight() / rowH);
            int drawn = 0;
            for (int blockIndex = 0; blockIndex < (int) graph.blocks.size(); ++blockIndex)
            {
                const auto& block = graph.blocks[(size_t) blockIndex];
                if (block.section != section)
                    continue;

                if (drawn >= visibleRows)
                    break;

                auto row = column.removeFromTop (rowH).reduced (0, 2);
                const bool selected = owner.selectedGraphKind == 1 && owner.selectedGraphIndex == blockIndex;
                g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.20f)
                                      : juce::Colour (0xff171b21).withAlpha (block.enabled ? 0.92f : 0.46f));
                g.fillRoundedRectangle (row.toFloat(), 4.0f);
                g.setColour (selected ? PatchCraftLookAndFeel::accent()
                                      : PatchCraftLookAndFeel::border().brighter (0.32f).withAlpha (0.85f));
                g.drawRoundedRectangle (row.toFloat(), 4.0f, selected ? 1.5f : 1.0f);

                hitZones.push_back ({ row, blockIndex, -1 });

                auto text = row.reduced (6, 1).removeFromTop (15);
                g.setColour (block.enabled ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
                g.setFont (juce::Font (9.5f, juce::Font::bold));
                g.drawText (block.name, text.removeFromLeft (text.getWidth() - 34), juce::Justification::centredLeft, true);
                g.setColour (block.enabled ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim());
                g.drawText (block.type, text, juce::Justification::centredRight, true);

                auto bars = row.reduced (6, 0).withTrimmedTop (18).withTrimmedBottom (4);
                const auto labels = mixerLabelsForSection (section);
                const int lanes = juce::jmin (4, labels.size());
                const int laneW = juce::jmax (1, bars.getWidth() / lanes);
                for (int lane = 0; lane < lanes; ++lane)
                {
                    auto laneArea = bars.removeFromLeft (laneW).reduced (2, 0);
                    const float value = blockMixerValue (block, lane);
                    g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.65f));
                    g.drawRoundedRectangle (laneArea.toFloat(), 3.0f, 1.0f);
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (block.enabled ? 0.95f : 0.42f));
                    g.fillRoundedRectangle (laneArea.withWidth (juce::roundToInt ((float) laneArea.getWidth() * value)).toFloat(), 3.0f);
                    g.setColour (PatchCraftLookAndFeel::text().withAlpha (0.94f));
                    g.setFont (7.8f);
                    g.drawText (labels[lane] + " " + formatFormulaMixerValue (block, lane),
                                laneArea.reduced (3, 0), juce::Justification::centred, true);
                    hitZones.push_back ({ laneArea.expanded (2, 3), blockIndex, lane });
                }

                ++drawn;
            }

            if (count > drawn)
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (9.0f);
                g.drawText ("+" + juce::String (count - drawn) + " more in Node Map",
                            column.removeFromTop (16), juce::Justification::centred);
            }
        }
    }

    void DspPage::FormulaPanel::selectBlock (int blockIndex)
    {
        if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
            return;

        owner.selectedGraphKind = 1;
        owner.selectedGraphIndex = blockIndex;
        owner.builderPanel.selectedItemIds.clear();
        owner.builderPanel.selectedItemId = 1000 + blockIndex + 1;
        owner.builderPanel.selectedItemIds.add (owner.builderPanel.selectedItemId);
        owner.editorItemBox.setSelectedId (owner.builderPanel.selectedItemId, juce::sendNotification);
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        owner.repaint();
        repaint();
    }

    void DspPage::FormulaPanel::setValueFromMouse (const juce::MouseEvent& e)
    {
        if (activeHit.blockIndex < 0 || activeHit.column < 0
            || activeHit.blockIndex >= (int) owner.project.getDspGraph().blocks.size())
            return;

        const float value = juce::jlimit (0.0f, 1.0f,
            (float) (e.x - activeHit.bounds.getX()) / (float) juce::jmax (1, activeHit.bounds.getWidth()));
        auto& block = owner.project.getDspGraph().blocks[(size_t) activeHit.blockIndex];
        setBlockMixerValue (block, activeHit.column, value);
        owner.markGraphEdited();
        owner.refreshFxPreviewRouting();
        owner.syncGraphEditor();
        owner.project.notifyChanged();
        owner.builderPanel.repaint();
        owner.repaint();
        repaint();
    }

    void DspPage::FormulaPanel::mouseDown (const juce::MouseEvent& e)
    {
        activeHit = {};
        draggingValue = false;

        for (auto hit : hitZones)
        {
            if (! hit.bounds.contains (e.getPosition()))
                continue;

            activeHit = hit;
            selectBlock (hit.blockIndex);
            if (hit.column >= 0)
            {
                draggingValue = true;
                setValueFromMouse (e);
            }
            return;
        }
    }

    void DspPage::FormulaPanel::mouseDoubleClick (const juce::MouseEvent& e)
    {
        for (auto hit : hitZones)
        {
            if (! hit.bounds.contains (e.getPosition()))
                continue;

            selectBlock (hit.blockIndex);
            owner.showBlockEditorPopout (hit.blockIndex);
            return;
        }
    }

    void DspPage::FormulaPanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingValue)
            setValueFromMouse (e);
    }

    void DspPage::FormulaPanel::mouseUp (const juce::MouseEvent&)
    {
        activeHit = {};
        draggingValue = false;
    }

    DspPage::MidiPlaygroundPanel::MidiPlaygroundPanel (DspPage& p) : owner (p)
    {
        setOpaque (false);
        for (auto* button : { &addButton, &chordButton, &sampleButton, &randomButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }

        addButton.onClick = [this] { ensureMidiPlaygroundBlock(); repaint(); };
        chordButton.onClick = [this] { configureChordPhrase(); repaint(); };
        sampleButton.onClick = [this] { configureSampleSliceControl(); repaint(); };
        randomButton.onClick = [this] { randomiseSeed(); repaint(); };
    }

    DspBlock* DspPage::MidiPlaygroundPanel::selectedMidiPlaygroundBlock()
    {
        auto& graph = owner.project.getDspGraph();
        if (owner.selectedGraphKind == 1
            && owner.selectedGraphIndex >= 0
            && owner.selectedGraphIndex < (int) graph.blocks.size())
        {
            auto& block = graph.blocks[(size_t) owner.selectedGraphIndex];
            if (block.section == "mod"
                && (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi")))
                return &block;
        }

        for (auto& block : graph.blocks)
            if (block.section == "mod"
                && (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi")))
                return &block;

        return nullptr;
    }

    DspBlock* DspPage::MidiPlaygroundPanel::ensureMidiPlaygroundBlock()
    {
        if (auto* existing = selectedMidiPlaygroundBlock())
            return existing;

        if (owner.currentTab != 3)
            owner.setTab (3);

        auto& graph = owner.project.getDspGraph();
        const auto previousSize = (int) graph.blocks.size();
        owner.addBuilderBlock();
        if ((int) graph.blocks.size() <= previousSize)
            return nullptr;

        auto& block = graph.blocks.back();
        const auto previousType = block.type;
        block.type = "midiPlayground";
        block.name = "MIDI Playground " + juce::String ((int) graph.blocks.size());
        block.targetId = "filterCutoff";
        owner.applyBlockTypeDefaults (block, previousType);
        owner.selectedGraphKind = 1;
        owner.selectedGraphIndex = (int) graph.blocks.size() - 1;
        owner.project.notifyChanged();
        owner.rebuildGraphEditorItems();
        owner.refreshBuilderPanel();
        owner.syncGraphEditor();
        return &graph.blocks.back();
    }

    void DspPage::MidiPlaygroundPanel::configureChordPhrase()
    {
        if (auto* block = ensureMidiPlaygroundBlock())
        {
            block->name = "Chord Phrase Playground";
            block->type = "midiPlayground";
            block->targetId = "filterCutoff";
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
            block->values["mpSampleControl"] = 0.0f;
            const float notes[] { 0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 7.0f, 4.0f, 2.0f };
            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = notes[step % 8];
                block->values["mpStep" + juce::String (step) + "On"] = step < 8 ? 1.0f : 0.0f;
                block->values["mpVelocity" + juce::String (step)] = step % 2 == 0 ? 0.95f : 0.68f;
                block->values["mpGate" + juce::String (step)] = 0.62f;
            }
            owner.project.notifyChanged();
            owner.refreshBuilderPanel();
            owner.syncGraphEditor();
        }
    }

    void DspPage::MidiPlaygroundPanel::configureSampleSliceControl()
    {
        if (owner.project.getEngineType() != "sample")
            owner.setEngine ("sample");

        if (auto* block = ensureMidiPlaygroundBlock())
        {
            block->name = "Sample Slice MIDI Playground";
            block->type = "midiPlayground";
            block->targetId = "sampleSlice";
            block->values["arpSteps"] = 16.0f;
            block->values["arpPattern"] = 0.0f;
            block->values["arpGate"] = 0.42f;
            block->values["arpSwing"] = 0.08f;
            block->values["mpScaleType"] = 0.0f;
            block->values["mpChordMode"] = 0.0f;
            block->values["mpChordSize"] = 1.0f;
            block->values["mpProbability"] = 1.0f;
            block->values["mpHumanize"] = 0.05f;
            block->values["mpSampleControl"] = 1.0f;
            block->values["sampleStart"] = 0.0f;
            block->values["sampleLength"] = 0.18f;
            block->values["sampleSliceCount"] = 16.0f;
            block->values["samplePitch"] = 0.0f;
            for (int step = 0; step < 16; ++step)
            {
                block->values["arpNote" + juce::String (step)] = 0.0f;
                block->values["mpSampleSlice" + juce::String (step)] = (float) step;
                block->values["mpStep" + juce::String (step) + "On"] = 1.0f;
                block->values["mpVelocity" + juce::String (step)] = step % 4 == 0 ? 1.0f : 0.72f;
                block->values["mpGate" + juce::String (step)] = 0.30f + (step % 4) * 0.08f;
            }
            owner.project.notifyChanged();
            owner.refreshBuilderPanel();
            owner.syncGraphEditor();
        }
    }

    void DspPage::MidiPlaygroundPanel::randomiseSeed()
    {
        if (auto* block = ensureMidiPlaygroundBlock())
        {
            auto& rng = juce::Random::getSystemRandom();
            block->values["mpSeed"] = (float) rng.nextInt (0x3fffffff);
            block->values["arpPattern"] = 7.0f;
            block->values["mpProbability"] = 0.55f + rng.nextFloat() * 0.45f;
            block->values["mpHumanize"] = rng.nextFloat() * 0.35f;
            owner.project.notifyChanged();
            owner.refreshBuilderPanel();
            owner.syncGraphEditor();
        }
    }

    juce::String DspPage::MidiPlaygroundPanel::selectedBlockSummary() const
    {
        auto& graph = owner.project.getDspGraph();
        const DspBlock* block = nullptr;
        if (owner.selectedGraphKind == 1
            && owner.selectedGraphIndex >= 0
            && owner.selectedGraphIndex < (int) graph.blocks.size())
            block = &graph.blocks[(size_t) owner.selectedGraphIndex];

        if (block == nullptr || block->section != "mod")
            return "No MIDI Playground block selected. Use Add Playground, then choose a mode.";

        auto get = [block] (const juce::String& id, float fallback)
        {
            if (auto it = block->values.find (id); it != block->values.end())
                return it->second;
            return fallback;
        };

        return block->name + "  |  "
             + juce::String (juce::roundToInt (get ("arpSteps", 8.0f))) + " steps, "
             + "pattern " + arpPatternName (juce::roundToInt (get ("arpPattern", 0.0f))) + ", "
             + "chord mode " + juce::String (juce::roundToInt (get ("mpChordMode", 0.0f))) + ", "
             + "sample control " + (get ("mpSampleControl", 0.0f) >= 0.5f ? "ON" : "OFF");
    }

    void DspPage::MidiPlaygroundPanel::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0b0e13));
        auto r = getLocalBounds().reduced (14);
        auto summary = r.removeFromTop (54);
        g.setColour (juce::Colour (0xff111821));
        g.fillRoundedRectangle (summary.toFloat(), 8.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.45f));
        g.drawRoundedRectangle (summary.toFloat(), 8.0f, 1.2f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText ("MIDI Playground Sections", summary.removeFromTop (22).reduced (12, 2), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (11.5f);
        g.drawText (selectedBlockSummary(), summary.reduced (12, 2), juce::Justification::centredLeft);

        r.removeFromTop (10);
        auto cards = r.removeFromTop (250);
        const juce::StringArray names {
            "1. Input",
            "2. Harmony",
            "3. Rhythm",
            "4. Performance",
            "5. Sample Control",
            "6. Output"
        };
        const juce::StringArray descriptions {
            "Held notes, latch, key switches, hardware MIDI, and software keyboard input.",
            "Scale root/type, chord mode, chord size, inversions, spread, and future Scaler-style progression tools.",
            "Steps, pattern, Euclidean/seeded random order, swing, gate, probability, velocity, and humanize.",
            "Macros, mod wheel, aftertouch, expression, XY, and DAW-automatable performer switches.",
            "MIDI controls sample start, length, slice count, slice index, reverse, and pitch for chopped playback.",
            "Generated notes drive Synth/Sampler/Player; routes can also trigger DSP mods, animations, and exportable MIDI clips."
        };

        const int columns = 3;
        const int cardW = cards.getWidth() / columns;
        for (int i = 0; i < names.size(); ++i)
        {
            auto card = juce::Rectangle<int> (cards.getX() + (i % columns) * cardW,
                                             cards.getY() + (i / columns) * 122,
                                             cardW - 8,
                                             112).reduced (2);
            g.setColour (i == 4 ? juce::Colour (0xff1b2014) : juce::Colour (0xff10141a));
            g.fillRoundedRectangle (card.toFloat(), 7.0f);
            g.setColour (i == 4 ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border().brighter (0.35f));
            g.drawRoundedRectangle (card.toFloat(), 7.0f, 1.1f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText (names[i], card.removeFromTop (24).reduced (9, 2), juce::Justification::centredLeft);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.0f);
            g.drawFittedText (descriptions[i], card.reduced (9, 4), juce::Justification::topLeft, 4);
        }

        auto bottom = r.removeFromBottom (92);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (12.0f);
        g.drawFittedText ("MIDI sample control path: hardware/software MIDI -> MIDI Playground step -> sampleSlice/sampleStart/sampleLength/samplePitch -> SampleSynthEngine -> Player export. CC20-23 also provide direct fixed shortcuts for Sample Start, Slice, Length, and Pitch; MIDI Learn can map any visible sample control.",
                          bottom.reduced (4), juce::Justification::topLeft, 3);
    }

    void DspPage::MidiPlaygroundPanel::resized()
    {
        auto r = getLocalBounds().reduced (14).removeFromBottom (36);
        randomButton.setBounds (r.removeFromRight (120).reduced (3));
        sampleButton.setBounds (r.removeFromRight (158).reduced (3));
        chordButton.setBounds (r.removeFromRight (118).reduced (3));
        addButton.setBounds (r.removeFromRight (132).reduced (3));
    }

    struct DspPage::TutorialOverlay : public juce::Component
    {
        explicit TutorialOverlay (DspPage& p) : owner (p)
        {
            setInterceptsMouseClicks (false, true);
            setAlwaysOnTop (true);

            backButton.getProperties().set ("smallButton", true);
            nextButton.getProperties().set ("smallButton", true);
            closeButton.getProperties().set ("smallButton", true);
            doNotShowAgain.getProperties().set ("smallButton", true);

            addAndMakeVisible (backButton);
            addAndMakeVisible (nextButton);
            addAndMakeVisible (closeButton);
            addAndMakeVisible (doNotShowAgain);

            backButton.onClick = [this] { setStep (step - 1); };
            nextButton.onClick = [this]
            {
                if (step >= getStepCount() - 1)
                    dismiss();
                else
                    setStep (step + 1);
            };
            closeButton.onClick = [this] { dismiss(); };
        }

        void restart()
        {
            setVisible (true);
            setStep (0);
            toFront (false);
        }

        int getStepCount() const { return 13; }

        void setStep (int newStep)
        {
            step = juce::jlimit (0, getStepCount() - 1, newStep);

            if (step <= 5)
                owner.setTab (0);
            else if (step == 6)
                owner.setTab (1);
            else if (step == 7)
                owner.setTab (2);
            else if (step == 8)
                owner.setTab (3);
            else if (step == 9)
                owner.setTab (4);
            else if (step == 10)
                owner.setTab (5);

            backButton.setEnabled (step > 0);
            nextButton.setButtonText (step >= getStepCount() - 1 ? "Done" : "Next");
            owner.hideHoverHelp();
            owner.resized();
            repaint();
        }

        juce::Rectangle<int> componentBounds (juce::Component& component) const
        {
            return owner.getLocalArea (&component, component.getLocalBounds()).expanded (6);
        }

        juce::Rectangle<int> unionBounds (std::initializer_list<juce::Component*> components) const
        {
            juce::Rectangle<int> bounds;
            for (auto* component : components)
                if (component != nullptr)
                    bounds = bounds.isEmpty() ? componentBounds (*component) : bounds.getUnion (componentBounds (*component));
            return bounds;
        }

        juce::Rectangle<int> highlightBounds() const
        {
            switch (step)
            {
                case 0: return unionBounds ({ &owner.samplerEngineButton, &owner.synthEngineButton, &owner.fxEngineButton });
                case 1: return componentBounds (owner.tabEngine);
                case 2: return componentBounds (owner.builderPanel.addBlockButton);
                case 3: return componentBounds (owner.builderPanel).reduced (8).withTrimmedTop (72);
                case 4: return unionBounds ({ &owner.editorItemBox, &owner.typeBox, &owner.sourceBox, &owner.targetBox,
                                              &owner.amountSlider, &owner.rateSlider, &owner.valueSlider,
                                              &owner.minSlider, &owner.maxSlider, &owner.curveSlider });
                case 5: return componentBounds (owner.sourceMatrix);
                case 6: return unionBounds ({ &owner.tabTone, &owner.builderPanel.openSectionEditorButton });
                case 7: return unionBounds ({ &owner.tabAmp, &owner.builderPanel.addBlockButton, &owner.builderPanel.openSectionEditorButton });
                case 8: return unionBounds ({ &owner.builderPanel.addMacroButton, &owner.builderPanel.addModButton,
                                              &owner.builderPanel.addAutomationButton, &owner.targetBox });
                case 9: return unionBounds ({ &owner.builderPanel.openSectionEditorButton, &owner.fxTrackImportButton, &owner.fxTrackUseMapperButton,
                                              &owner.fxTrackPlayButton,
                                              &owner.fxWaveform, &owner.fxTrackMonitorBox });
                case 10: return unionBounds ({ &owner.tabOut, &owner.builderPanel.addBlockButton, &owner.sourceMatrix });
                case 11: return unionBounds ({ &owner.builderPanel.savePatchButton, &owner.builderPanel.savePatchAsButton,
                                              &owner.builderPanel.sendExpansionButton });
                case 12: return unionBounds ({ &owner.builderPanel.nodeMapButton, &owner.builderPanel.openSectionEditorButton });
                default: break;
            }
            return getLocalBounds().reduced (40);
        }

        juce::String title() const
        {
            switch (step)
            {
                case 0: return "1. Choose the instrument engine";
                case 1: return "2. Start on Source";
                case 2: return "3. Add a sound block";
                case 3: return "4. Select the block card";
                case 4: return "5. Edit block parameters";
                case 5: return "6. Balance the bank mixer";
                case 6: return "7. Shape the Filter section";
                case 7: return "8. Build Amp behavior";
                case 8: return "9. Add motion and macros";
                case 9: return "10. Audition FX with audio";
                case 10: return "11. Finalize Output";
                case 11: return "12. Save the playable patch";
                case 12: return "13. Use larger views";
                default: return {};
            }
        }

        juce::String body() const
        {
            switch (step)
            {
                case 0: return "Pick Sampler, Synth, or FX first. This decides which blocks, parameters, and presets are valid. Check Do not show again to keep auto tutorials off; replay anytime from Help > DSP Builder Tutorial.";
                case 1: return "Click Source first. This is where oscillators, samples, noise, wavetable layers, and source blends enter the instrument.";
                case 2: return "Click + Block to add one real sound-building unit. For Synth, start with an oscillator or wavetable block.";
                case 3: return "Click the new card to select it. Ctrl/Shift-click lets you select multiple cards for inspection or deletion.";
                case 4: return "The Graph Inspector changes the selected item. Type chooses the block family, Target chooses what it drives, and the labeled knobs edit exact values.";
                case 5: return "Use the bank mixer for fast balancing. Each module has 4 banks; each bank holds up to 6 blocks and 6 mixer rows.";
                case 6: return "Filter shapes tone. Add filter/EQ blocks, then open EQ Editor for the larger surgical view instead of squeezing it inline.";
                case 7: return "Amp controls envelope, velocity response, and gain behavior. Add an Amp block, then adjust attack, decay, sustain, release, and related routing.";
                case 8: return "MOD creates movement. Add Macro for one-knob control, Mod for LFO/random/followers, or Auto for looping lanes, then assign a target.";
                case 9: return "FX can be tested against a sample. Use Sample Player for a larger waveform/playback view, then edit delay, reverb, distortion, or other FX blocks.";
                case 10: return "OUT is the final safety and performance layer: volume, pan, BPM sync, retrigger, width, limiter, and output level.";
                case 11: return "Save Patch stores the complete playable preset: Source, Filter, Amp, Mod, FX, Out, macros, routes, automation, and metadata.";
                case 12: return "Use Node Map and Deep Editor whenever the main layout gets cramped. The main Builder stays readable; detailed editing moves into popouts.";
                default: return {};
            }
        }

        juce::Rectangle<int> panelBounds() const
        {
            const auto bounds = getLocalBounds();
            const auto highlight = highlightBounds();
            const int width = juce::jmin (560, bounds.getWidth() - 40);
            const int height = 206;
            int x = juce::jlimit (20, juce::jmax (20, bounds.getWidth() - width - 20), highlight.getCentreX() - width / 2);
            int y = highlight.getCentreY() < bounds.getCentreY() ? bounds.getBottom() - height - 22 : bounds.getY() + 22;
            return { x, y, width, height };
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::black.withAlpha (0.52f));
            g.fillAll();

            const auto highlight = highlightBounds().getIntersection (getLocalBounds()).expanded (2);
            const auto panel = panelBounds();

            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.16f));
            g.fillRoundedRectangle (highlight.toFloat(), 8.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRoundedRectangle (highlight.toFloat(), 8.0f, 2.0f);

            g.drawArrow (juce::Line<float> (panel.getCentre().toFloat(), highlight.getCentre().toFloat()),
                         2.0f, 10.0f, 8.0f);

            g.setColour (juce::Colours::black.withAlpha (0.92f));
            g.fillRoundedRectangle (panel.toFloat(), 8.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRoundedRectangle (panel.toFloat(), 8.0f, 2.0f);

            auto text = panel.reduced (14, 12);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (15.0f, juce::Font::bold));
            g.drawText (title(), text.removeFromTop (22), juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.setFont (juce::Font (12.2f));
            g.drawFittedText (body(), text.removeFromTop (104), juce::Justification::topLeft, 5);

            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.5f));
            g.drawText ("Step " + juce::String (step + 1) + " of " + juce::String (getStepCount()),
                        text, juce::Justification::bottomLeft);
        }

        void resized() override
        {
            auto controls = panelBounds().reduced (12, 10).removeFromBottom (28);
            closeButton.setBounds (controls.removeFromRight (74).reduced (2));
            nextButton.setBounds (controls.removeFromRight (72).reduced (2));
            backButton.setBounds (controls.removeFromRight (72).reduced (2));
            doNotShowAgain.setBounds (controls.removeFromLeft (160).reduced (2));
        }

        void dismiss()
        {
            if (doNotShowAgain.getToggleState())
            {
                if (owner.studioOwner != nullptr)
                    owner.studioOwner->setStudioTutorialsEnabled (false);
                else
                {
                    owner.project.getManifest().studioShowTutorials = false;
                    owner.project.notifyChanged();
                }
            }
            setVisible (false);
        }

        DspPage& owner;
        int step = 0;
        juce::TextButton backButton { "Back" };
        juce::TextButton nextButton { "Next" };
        juce::TextButton closeButton { "Close" };
        juce::ToggleButton doNotShowAgain { "Do not show again" };
    };

    struct DspPage::SectionEditorPopout : public juce::Component,
                                          private juce::Timer
    {
        SectionEditorPopout (DspPage& page, int sectionTab)
            : owner (page), tab (sectionTab)
        {
            setSize (1120, 680);
            title.setFont (juce::Font (16.0f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
            title.setJustificationType (juce::Justification::centredLeft);
            help.setFont (juce::Font (12.0f));
            help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            help.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (title);
            addAndMakeVisible (help);

            auto setupButton = [this] (juce::Button& button)
            {
                button.getProperties().set ("smallButton", true);
                addAndMakeVisible (button);
            };

            if (tab == 0 && owner.project.getEngineType() == "synth")
            {
                title.setText ("Custom Wavetable Editor", juce::dontSendNotification);
                help.setText ("Draw, import, normalize, and audition a selected Wavetable Source block without compressing the main Builder layout.",
                              juce::dontSendNotification);
                wtPanel = std::make_unique<WavetableEditorPanel> (owner);
                addAndMakeVisible (*wtPanel);
                setupButton (primaryButton);
                setupButton (secondaryButton);
                setupButton (thirdButton);
                primaryButton.setButtonText ("Import WT");
                secondaryButton.setButtonText ("Normalize");
                thirdButton.setButtonText ("Sine");
                primaryButton.onClick = [this] { owner.importWavetableShape(); repaintAllPanels(); };
                secondaryButton.onClick = [this] { owner.normalizeSelectedWavetableShape(); repaintAllPanels(); };
                thirdButton.onClick = [this] { owner.setSelectedWavetableShapeToSine(); repaintAllPanels(); };
            }
            else if (tab == 1)
            {
                title.setText ("Surgical EQ Editor", juce::dontSendNotification);
                help.setText ("Use the larger EQ surface for precise band edits, analyzer view, copy/paste, and saved band library actions.",
                              juce::dontSendNotification);
                eqPanel = std::make_unique<SurgicalEqPanel> (owner);
                addAndMakeVisible (*eqPanel);
                for (auto* button : { &primaryButton, &secondaryButton, &thirdButton, &fourthButton })
                    setupButton (*button);
                setupButton (analyzerButton);
                setupButton (freezeButton);
                primaryButton.setButtonText ("Copy Band");
                secondaryButton.setButtonText ("Paste Band");
                thirdButton.setButtonText ("Save Band");
                fourthButton.setButtonText ("Insert Saved");
                analyzerButton.setButtonText ("Analyzer");
                freezeButton.setButtonText ("Freeze");
                analyzerButton.setClickingTogglesState (true);
                freezeButton.setClickingTogglesState (true);
                analyzerButton.setToggleState (owner.eqAnalyzerToggle.getToggleState(), juce::dontSendNotification);
                freezeButton.setToggleState (owner.eqAnalyzerFreezeToggle.getToggleState(), juce::dontSendNotification);
                primaryButton.onClick = [this] { owner.copySelectedEqBand(); repaintAllPanels(); };
                secondaryButton.onClick = [this] { owner.pasteSelectedEqBand(); repaintAllPanels(); };
                thirdButton.onClick = [this] { owner.saveSelectedEqBandToLibrary(); repaintAllPanels(); };
                fourthButton.onClick = [this] { owner.insertSavedEqBand(); repaintAllPanels(); };
                analyzerButton.onClick = [this]
                {
                    owner.eqAnalyzerToggle.setToggleState (analyzerButton.getToggleState(), juce::dontSendNotification);
                    repaintAllPanels();
                };
                freezeButton.onClick = [this]
                {
                    owner.eqAnalyzerFreezeToggle.setToggleState (freezeButton.getToggleState(), juce::dontSendNotification);
                    repaintAllPanels();
                };
            }
            else if (tab == 3)
            {
                title.setText ("Mod Matrix", juce::dontSendNotification);
                help.setText ("Edit actual Macro, Mod, and Automation graph routes. Click a row to select it; drag Depth/Rate or State cells to change values.",
                              juce::dontSendNotification);
                modPanel = std::make_unique<ModMatrixPanel> (owner);
                addAndMakeVisible (*modPanel);
                for (auto* button : { &primaryButton, &secondaryButton, &thirdButton })
                    setupButton (*button);
                primaryButton.setButtonText ("+ Macro");
                secondaryButton.setButtonText ("+ Mod");
                thirdButton.setButtonText ("+ Auto");
                primaryButton.onClick = [this] { owner.addBuilderMacro(); repaintAllPanels(); };
                secondaryButton.onClick = [this] { owner.addBuilderModRoute(); repaintAllPanels(); };
                thirdButton.onClick = [this] { owner.addBuilderAutomation(); repaintAllPanels(); };
            }
            else if (tab == 4)
            {
                title.setText ("FX Sample Player", juce::dontSendNotification);
                help.setText ("Import or use a mapped sample, then play through the current FX chain while editing blocks and routes.",
                              juce::dontSendNotification);
                fxView = std::make_unique<FxWaveformView> (owner);
                addAndMakeVisible (*fxView);
                for (auto* button : { &primaryButton, &secondaryButton, &thirdButton, &fourthButton })
                    setupButton (*button);
                primaryButton.setButtonText ("Import Sample");
                secondaryButton.setButtonText ("Use Mapper");
                thirdButton.setButtonText ("Play");
                fourthButton.setButtonText ("Stop");
                primaryButton.onClick = [this] { owner.importFxSampleForTesting(); repaintAllPanels(); };
                secondaryButton.onClick = [this]
                {
                    if (owner.studioOwner != nullptr)
                        if (const auto* zone = owner.studioOwner->getSelectedSampleZone())
                            owner.loadFxSampleZone (*zone);
                    repaintAllPanels();
                };
                thirdButton.onClick = [this] { owner.startFxSamplePlayback(); repaintAllPanels(); };
                fourthButton.onClick = [this] { owner.stopFxSamplePlayback(); repaintAllPanels(); };
                startTimerHz (24);
            }
            else
            {
                title.setText (owner.getCurrentPatchSectionLabel() + " Deep View", juce::dontSendNotification);
                help.setText ("This section uses block cards, the bank mixer, Graph Inspector, and Node Map. Use Node Map for a larger routing view.",
                              juce::dontSendNotification);
                setupButton (primaryButton);
                primaryButton.setButtonText ("Open Node Map");
                primaryButton.onClick = [this] { owner.showNodeMapPopout(); };
            }
        }

        ~SectionEditorPopout() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff090b0f));
            auto panel = getLocalBounds().reduced (12).withTrimmedTop (58).toFloat();
            g.setColour (juce::Colour (0xff10141a));
            g.fillRoundedRectangle (panel, 8.0f);
            g.setColour (PatchCraftLookAndFeel::border().brighter (0.45f));
            g.drawRoundedRectangle (panel, 8.0f, 1.4f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            auto header = r.removeFromTop (50);
            title.setBounds (header.removeFromTop (24));
            help.setBounds (header);

            auto buttonRow = r.removeFromTop (34);
            fourthButton.setBounds (buttonRow.removeFromRight (104).reduced (3));
            thirdButton.setBounds (buttonRow.removeFromRight (96).reduced (3));
            secondaryButton.setBounds (buttonRow.removeFromRight (104).reduced (3));
            primaryButton.setBounds (buttonRow.removeFromRight (112).reduced (3));
            freezeButton.setBounds (buttonRow.removeFromRight (84).reduced (3));
            analyzerButton.setBounds (buttonRow.removeFromRight (92).reduced (3));
            r.removeFromTop (6);

            auto body = r.reduced (4);
            if (wtPanel != nullptr) wtPanel->setBounds (body);
            if (eqPanel != nullptr) eqPanel->setBounds (body);
            if (modPanel != nullptr) modPanel->setBounds (body);
            if (midiPanel != nullptr) midiPanel->setBounds (body);
            if (fxView != nullptr) fxView->setBounds (body);
        }

        void timerCallback() override
        {
            repaintAllPanels();
        }

        void repaintAllPanels()
        {
            if (wtPanel != nullptr) wtPanel->repaint();
            if (eqPanel != nullptr) eqPanel->repaint();
            if (modPanel != nullptr) modPanel->repaint();
            if (midiPanel != nullptr) midiPanel->repaint();
            if (fxView != nullptr) fxView->repaint();
            owner.refreshBuilderPanel();
            owner.sourceMatrix.repaint();
            owner.surgicalEqPanel.repaint();
            owner.wavetableEditor.repaint();
            repaint();
        }

        DspPage& owner;
        int tab = 0;
        juce::Label title;
        juce::Label help;
        juce::TextButton primaryButton;
        juce::TextButton secondaryButton;
        juce::TextButton thirdButton;
        juce::TextButton fourthButton;
        juce::ToggleButton analyzerButton;
        juce::ToggleButton freezeButton;
        std::unique_ptr<WavetableEditorPanel> wtPanel;
        std::unique_ptr<SurgicalEqPanel> eqPanel;
        std::unique_ptr<ModMatrixPanel> modPanel;
        std::unique_ptr<MidiPlaygroundPanel> midiPanel;
        std::unique_ptr<FxWaveformView> fxView;
    };

    struct DspPage::SectionMixerPopout : public juce::Component,
                                         private juce::Timer
    {
        explicit SectionMixerPopout (DspPage& page)
            : owner (page), mixer (page)
        {
            setSize (1160, 460);
            title.setFont (juce::Font (16.0f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
            title.setJustificationType (juce::Justification::centredLeft);
            help.setFont (juce::Font (12.0f));
            help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            help.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (title);
            addAndMakeVisible (help);
            addAndMakeVisible (mixer);
            startTimerHz (20);
        }

        ~SectionMixerPopout() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff090b0f));
            auto panel = getLocalBounds().reduced (12).withTrimmedTop (58).toFloat();
            g.setColour (juce::Colour (0xff10141a));
            g.fillRoundedRectangle (panel, 8.0f);
            g.setColour (PatchCraftLookAndFeel::border().brighter (0.50f));
            g.drawRoundedRectangle (panel, 8.0f, 1.5f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            auto header = r.removeFromTop (50);
            title.setBounds (header.removeFromTop (24));
            help.setBounds (header);
            r.removeFromTop (12);
            mixer.setBounds (r.reduced (4));
        }

        void timerCallback() override
        {
            // The mixer panel reflects bank/section selection plus per-block
            // values. All those changes already route through project change
            // notifications which mark this panel dirty; the timer only
            // refreshes the title when the user switches banks. Repainting
            // the entire mixer at 24 Hz unconditionally was visibly slowing
            // knob drags.
            const auto newTitle = owner.getCurrentPatchSectionLabel() + " Mixer - Bank "
                                + juce::String (owner.currentSectionBank() + 1);
            if (newTitle != lastTitle)
            {
                lastTitle = newTitle;
                title.setText (newTitle, juce::dontSendNotification);
                help.setText ("Edit the six channels in the selected bank. Use the bank tabs in DSP Builder to switch banks.",
                              juce::dontSendNotification);
                mixer.repaint();
            }
        }

        DspPage& owner;
        SourceMatrixPanel mixer;
        juce::Label title;
        juce::Label help;
        juce::String lastTitle;
    };

    DspPage::DspPage (PatchCraftProject& p, bool quickEditMode, StudioMainComponent* owner)
        : project (p), studioOwner (owner), quickEdit (quickEditMode)
    {
        setOpaque (true);
        fxFormatManager.registerBasicFormats();

        titleLabel.setText (quickEdit ? "DSP" : "DSP BUILDER", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (titleLabel);

        subtitleLabel.setFont (juce::Font (11.5f));
        subtitleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitleLabel);

        for (auto* b : { &samplerEngineButton, &synthEngineButton, &fxEngineButton })
        {
            styleEngineButton (*b, 8911);
            addAndMakeVisible (*b);
        }

        samplerEngineButton.onClick = [this] { setEngine ("sample"); };
        synthEngineButton.onClick   = [this] { setEngine ("synth"); };
        fxEngineButton.onClick      = [this] { setEngine ("fx"); };

        for (auto* b : { &easyModeButton, &advancedModeButton })
        {
            styleEngineButton (*b, 8913);
            b->setVisible (! quickEdit);
            addAndMakeVisible (*b);
        }
        advancedModeButton.setToggleState (true, juce::dontSendNotification);
        easyModeButton.onClick = [this] { setWorkflowMode (true); };
        advancedModeButton.onClick = [this] { setWorkflowMode (false); };

        for (auto* b : { &tabEngine, &tabTone, &tabAmp, &tabMod, &tabFx, &tabOut })
        {
            styleTab (*b, 8912);
            addAndMakeVisible (*b);
        }

        tabEngine.setToggleState (true, juce::dontSendNotification);
        tabEngine.onClick = [this] { setTab (0); };
        tabTone.onClick   = [this] { setTab (1); };
        tabAmp.onClick    = [this] { setTab (2); };
        tabMod.onClick    = [this] { setTab (3); };
        tabFx.onClick     = [this] { setTab (4); };
        tabOut.onClick    = [this] { setTab (5); };

        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
            addChildComponent (*section);
        easyTitleLabel.setText ("Easy Sound Builder", juce::dontSendNotification);
        easyTitleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
        easyTitleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        easyHelpLabel.setFont (juce::Font (12.5f));
        easyHelpLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        easyHelpLabel.setJustificationType (juce::Justification::topLeft);
        addChildComponent (easyTitleLabel);
        addChildComponent (easyHelpLabel);
        for (auto* label : { &easyRecipeLabel, &easyParametersLabel, &easyWorkflowLabel })
        {
            label->setFont (juce::Font (12.0f));
            label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
            label->setJustificationType (juce::Justification::topLeft);
            addChildComponent (*label);
        }
        int themeId = 1;
        for (const auto& theme : PresetGenerator::themes())
            easyThemeBox.addItem (theme, themeId++);
        easyThemeBox.setSelectedId (1, juce::dontSendNotification);
        easyThemeBox.setTextWhenNothingSelected ("Sound Type");
        easyThemeBox.setTooltip ("Choose a musical preset direction. Easy mode creates useful blocks and values, then Advanced mode can fine-tune them.");
        easyThemeBox.onChange = [this] { refreshEasyModeSummary(); };
        addChildComponent (easyThemeBox);
        easyGenerateButton.getProperties().set ("smallButton", true);
        easyRandomButton.getProperties().set ("smallButton", true);
        easyPackCreatorButton.getProperties().set ("smallButton", true);
        easyAdvancedButton.getProperties().set ("smallButton", true);
        easyGenerateButton.onClick = [this] { applyEasyPreset(); };
        easyRandomButton.onClick = [this]
        {
            const auto theme = easyThemeBox.getText().isNotEmpty()
                ? easyThemeBox.getText()
                : juce::String ("Arps");
            applyEasyPresetForTheme (theme, true);
        };
        easyAddToPackToggle.setTooltip ("When enabled, Create Preset also captures the full playable patch into the selected expansion pack.");
        easyExpansionBox.setTextWhenNothingSelected ("Expansion Pack");
        easyExpansionBox.setTooltip ("Choose the sellable expansion pack for generated Easy presets.");
        easyExpansionBox.onChange = [this]
        {
            if (easyExpansionBox.getSelectedId() >= 100)
                easyAddToPackToggle.setToggleState (true, juce::dontSendNotification);
            refreshEasyModeSummary();
        };
        easyPackCreatorButton.onClick = [this] { showPackCreator(); };
        easyAdvancedButton.onClick = [this] { setWorkflowMode (false); };
        addChildComponent (easyGenerateButton);
        addChildComponent (easyRandomButton);
        addChildComponent (easyAddToPackToggle);
        addChildComponent (easyExpansionBox);
        addChildComponent (easyPackCreatorButton);
        addChildComponent (easyAdvancedButton);
        addChildComponent (builderPanel);
        addChildComponent (formulaPanel);
        addChildComponent (sourceMatrix);
        addChildComponent (surgicalEqPanel);
        addChildComponent (wavetableEditor);
        addChildComponent (modMatrix);
        builderPanel.onCardSelected = [this] (int itemId, bool multiSelect)
        {
            if (multiSelect)
            {
                selectedGraphKind = itemId > 0 ? itemId / 1000 : 0;
                selectedGraphIndex = itemId > 0 ? (itemId % 1000) - 1 : -1;
                editorItemBox.setSelectedId (itemId, juce::dontSendNotification);
                syncGraphEditor();
                return;
            }

            editorItemBox.setSelectedId (itemId, juce::sendNotification);
        };
        builderPanel.onSectionBankSelected = [this] (int bank)
        {
            setCurrentSectionBank (bank);
            refreshBuilderPanel();
            sourceMatrix.repaint();
            surgicalEqPanel.repaint();
        };
        builderPanel.onNodeMapRequested = [this] { showNodeMapPopout(); };
        builderPanel.onSectionEditorRequested = [this] { showSectionEditorPopout(); };
        builderPanel.onMixerRequested = [this] { showSectionMixerPopout(); };
        builderPanel.addBlockButton.onClick = [this] { addBuilderBlock(); };
        builderPanel.addMacroButton.onClick = [this] { addBuilderMacro(); };
        builderPanel.addModButton.onClick   = [this] { addBuilderModRoute(); };
        builderPanel.addArpButton.onClick = [this] { addArpBlock(); };
        builderPanel.addAutomationButton.onClick = [this] { addBuilderAutomation(); };
        builderPanel.importSampleButton.onClick = [this] { importFxSampleForTesting(); };
        builderPanel.savePatchButton.onClick = [this]
        {
            if (studioOwner != nullptr)
                studioOwner->saveCurrentDspPatch();
        };
        builderPanel.savePatchAsButton.onClick = [this]
        {
            if (studioOwner != nullptr)
                studioOwner->saveCurrentDspPatchAs();
        };
        builderPanel.saveSectionPresetButton.onClick = [this]
        {
            if (studioOwner != nullptr)
                studioOwner->saveCurrentSectionPreset();
        };
        builderPanel.sendExpansionButton.onClick = [this] { addCurrentPatchToSelectedExpansion(); };
        builderPanel.packCreatorButton.onClick = [this] { showPackCreator(); };
        builderPanel.clearSectionButton.onClick = [this] { clearCurrentBuilderSection(); };
        builderPanel.clearAllButton.onClick = [this] { clearAllBuilderSections(); };

        for (auto* component : { static_cast<juce::Component*> (&fxTrackImportButton),
                                 static_cast<juce::Component*> (&fxTrackUseMapperButton),
                                 static_cast<juce::Component*> (&fxTrackPlayButton),
                                 static_cast<juce::Component*> (&fxTrackStopButton),
                                 static_cast<juce::Component*> (&fxTrackLoopToggle),
                                 static_cast<juce::Component*> (&fxTrackRetriggerToggle),
                                 static_cast<juce::Component*> (&fxTrackLiveInputToggle),
                                 static_cast<juce::Component*> (&fxTrackSliceBox),
                                 static_cast<juce::Component*> (&fxTrackTriggerBox),
                                 static_cast<juce::Component*> (&fxTrackPrevSliceButton),
                                 static_cast<juce::Component*> (&fxTrackNextSliceButton),
                                 static_cast<juce::Component*> (&fxTrackGainSlider),
                                 static_cast<juce::Component*> (&fxTrackMonitorBox),
                                 static_cast<juce::Component*> (&fxTrackLoopStartSlider),
                                 static_cast<juce::Component*> (&fxTrackLoopEndSlider),
                                 static_cast<juce::Component*> (&fxTrackNameLabel),
                                 static_cast<juce::Component*> (&fxTrackStatusLabel),
                                 static_cast<juce::Component*> (&fxWaveform) })
            addChildComponent (*component);

        for (auto* component : { static_cast<juce::Component*> (&eqAnalyzerToggle),
                                 static_cast<juce::Component*> (&eqAnalyzerFreezeToggle),
                                 static_cast<juce::Component*> (&eqBandCopyButton),
                                 static_cast<juce::Component*> (&eqBandPasteButton),
                                 static_cast<juce::Component*> (&eqBandSaveButton),
                                 static_cast<juce::Component*> (&eqBandInsertButton) })
            addChildComponent (*component);
        for (auto* component : { static_cast<juce::Component*> (&wtImportButton),
                                 static_cast<juce::Component*> (&wtNormalizeButton),
                                 static_cast<juce::Component*> (&wtSineButton) })
            addChildComponent (*component);
        eqAnalyzerToggle.setToggleState (true, juce::dontSendNotification);
        eqAnalyzerToggle.setTooltip ("Show the live FX preview spectrum behind the authored EQ curve.");
        eqAnalyzerFreezeToggle.setTooltip ("Freeze the current analyzer trace while continuing to edit bands.");
        eqBandCopyButton.getProperties().set ("smallButton", true);
        eqBandPasteButton.getProperties().set ("smallButton", true);
        eqBandSaveButton.getProperties().set ("smallButton", true);
        eqBandInsertButton.getProperties().set ("smallButton", true);
        eqAnalyzerToggle.getProperties().set ("smallButton", true);
        eqAnalyzerFreezeToggle.getProperties().set ("smallButton", true);
        eqAnalyzerToggle.onClick = [this] { surgicalEqPanel.repaint(); };
        eqAnalyzerFreezeToggle.onClick = [this] { surgicalEqPanel.repaint(); };
        eqBandCopyButton.onClick = [this] { copySelectedEqBand(); };
        eqBandPasteButton.onClick = [this] { pasteSelectedEqBand(); };
        eqBandSaveButton.onClick = [this] { saveSelectedEqBandToLibrary(); };
        eqBandInsertButton.onClick = [this] { insertSavedEqBand(); };
        wtImportButton.getProperties().set ("smallButton", true);
        wtNormalizeButton.getProperties().set ("smallButton", true);
        wtSineButton.getProperties().set ("smallButton", true);
        wtImportButton.onClick = [this] { importWavetableShape(); };
        wtNormalizeButton.onClick = [this] { normalizeSelectedWavetableShape(); };
        wtSineButton.onClick = [this] { setSelectedWavetableShapeToSine(); };
        fxTrackImportButton.getProperties().set ("smallButton", true);
        fxTrackUseMapperButton.getProperties().set ("smallButton", true);
        fxTrackPlayButton.getProperties().set ("smallButton", true);
        fxTrackStopButton.getProperties().set ("smallButton", true);
        fxTrackPrevSliceButton.getProperties().set ("smallButton", true);
        fxTrackNextSliceButton.getProperties().set ("smallButton", true);
        fxTrackLoopToggle.setToggleState (true, juce::dontSendNotification);
        fxTrackRetriggerToggle.setToggleState (true, juce::dontSendNotification);
        fxTrackLiveInputToggle.setToggleState (false, juce::dontSendNotification);
        fxTrackNameLabel.setText ("No sample loaded", juce::dontSendNotification);
        fxTrackNameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        fxTrackStatusLabel.setText ("FX sample player idle", juce::dontSendNotification);
        fxTrackStatusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        fxTrackImportButton.onClick = [this] { importFxSampleForTesting(); };
        fxTrackUseMapperButton.setTooltip ("Load the currently selected Sample Mapper zone into the FX preview track.");
        fxTrackUseMapperButton.onClick = [this]
        {
            if (studioOwner == nullptr)
            {
                fxTrackStatusLabel.setText ("Sample Mapper is not available here", juce::dontSendNotification);
                return;
            }

            if (const auto* zone = studioOwner->getSelectedSampleZone())
                loadFxSampleZone (*zone);
            else
                fxTrackStatusLabel.setText ("Select a Sample Mapper zone first", juce::dontSendNotification);
        };
        fxTrackPlayButton.onClick = [this] { startFxSamplePlayback(); };
        fxTrackStopButton.onClick = [this] { stopFxSamplePlayback(); };
        fxTrackLoopToggle.onClick = [this] { fxLooping.store (fxTrackLoopToggle.getToggleState()); };
        fxTrackRetriggerToggle.setTooltip ("When enabled, Play jumps to the selected loop or slice start. Turning it on while playing retriggers immediately.");
        fxTrackRetriggerToggle.onClick = [this]
        {
            const bool enabled = fxTrackRetriggerToggle.getToggleState();
            fxRetriggerOnPlay.store (enabled);
            if (enabled && fxPlaying.load())
                retriggerFxSamplePlayback (true);
            else
                fxTrackStatusLabel.setText (enabled ? "Retrigger on: Play resets to region start"
                                                    : "Retrigger off: Play resumes from current position",
                                            juce::dontSendNotification);
        };
        fxTrackLiveInputToggle.setTooltip ("Use the selected audio input as the FX Builder source instead of the imported sample.");
        fxTrackLiveInputToggle.onClick = [this]
        {
            const bool enabled = fxTrackLiveInputToggle.getToggleState();
            fxUseLiveInput.store (enabled);
            fxTrackStatusLabel.setText (enabled ? "Live input selected - press Play to monitor through FX"
                                                : (fxSampleLength.load() > 0 ? "Sample preview selected" : "FX sample player idle"),
                                        juce::dontSendNotification);
        };
        fxTrackSliceBox.addItem ("Full", 1);
        fxTrackSliceBox.addItem ("4 slices", 2);
        fxTrackSliceBox.addItem ("8 slices", 3);
        fxTrackSliceBox.addItem ("16 slices", 4);
        fxTrackSliceBox.addItem ("32 slices", 5);
        fxTrackSliceBox.setSelectedId (1, juce::dontSendNotification);
        fxTrackSliceBox.setTooltip ("Divide the imported sample into musical trigger slices.");
        fxTrackSliceBox.onChange = [this]
        {
            const int selected = fxTrackSliceBox.getSelectedId();
            const int slices = selected == 2 ? 4 : selected == 3 ? 8 : selected == 4 ? 16 : selected == 5 ? 32 : 1;
            setFxSliceCount (slices);
        };
        fxTrackTriggerBox.addItem ("Loop", 1);
        fxTrackTriggerBox.addItem ("One Shot", 2);
        fxTrackTriggerBox.addItem ("Sequence", 3);
        fxTrackTriggerBox.addItem ("Random", 4);
        fxTrackTriggerBox.setSelectedId (1, juce::dontSendNotification);
        fxTrackTriggerBox.setTooltip ("Loop repeats the selected region, One Shot stops at the end, Sequence steps through slices, Random jumps between slices.");
        fxTrackTriggerBox.onChange = [this] { fxTriggerMode.store (juce::jmax (0, fxTrackTriggerBox.getSelectedId() - 1)); };
        fxTrackPrevSliceButton.onClick = [this] { selectFxSlice (fxSelectedSlice.load() - 1); };
        fxTrackNextSliceButton.onClick = [this] { selectFxSlice (fxSelectedSlice.load() + 1); };
        fxTrackGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        fxTrackGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
        fxTrackGainSlider.setRange (-24.0, 12.0, 0.1);
        fxTrackGainSlider.setValue (0.0, juce::dontSendNotification);
        fxTrackGainSlider.setTextValueSuffix (" dB");
        fxTrackGainSlider.setTooltip ("Sample preview gain before the FX chain.");
        fxTrackGainSlider.onValueChange = [this]
        {
            fxPreviewGain.store (juce::Decibels::decibelsToGain ((float) fxTrackGainSlider.getValue()));
        };
        fxTrackMonitorBox.addItem ("Wet", 1);
        fxTrackMonitorBox.addItem ("Dry", 2);
        fxTrackMonitorBox.addItem ("Split", 3);
        fxTrackMonitorBox.setSelectedId (1, juce::dontSendNotification);
        fxTrackMonitorBox.setTooltip ("Wet = processed FX, Dry = original sample, Split = dry left / wet right.");
        fxTrackMonitorBox.onChange = [this] { fxMonitorMode.store (juce::jmax (0, fxTrackMonitorBox.getSelectedId() - 1)); };
        for (auto* slider : { &fxTrackLoopStartSlider, &fxTrackLoopEndSlider })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
            slider->setRange (0.0, 100.0, 0.1);
            slider->setTextValueSuffix (" %");
        }
        fxTrackLoopStartSlider.setValue (0.0, juce::dontSendNotification);
        fxTrackLoopEndSlider.setValue (100.0, juce::dontSendNotification);
        fxTrackLoopStartSlider.setTooltip ("Loop start for FX sample preview.");
        fxTrackLoopEndSlider.setTooltip ("Loop end for FX sample preview.");
        fxTrackLoopStartSlider.onValueChange = [this]
        {
            const auto start = (float) fxTrackLoopStartSlider.getValue() * 0.01f;
            setFxLoopRegion (start, fxLoopEnd01.load(), true);
        };
        fxTrackLoopEndSlider.onValueChange = [this]
        {
            const auto end = (float) fxTrackLoopEndSlider.getValue() * 0.01f;
            setFxLoopRegion (fxLoopStart01.load(), end, true);
        };

        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
        {
            section->addButton.onClick = [this, section] { showQuickParamMenu (*section, false); };
            section->editButton.onClick = [this, section] { showQuickParamMenu (*section, true); };
        }

        editorTitle.setText ("GRAPH INSPECTOR", juce::dontSendNotification);
        editorTitle.setFont (juce::Font (12.0f, juce::Font::bold));
        editorTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        editorHint.setText ("Select a block, macro, modulation route, or automation lane. Changes are live, saved, and exported.",
                            juce::dontSendNotification);
        editorHint.setFont (juce::Font (11.0f));
        editorHint.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());

        for (auto* label : { &amountLabel, &rateLabel, &valueLabel, &minLabel, &maxLabel, &curveLabel })
        {
            label->setFont (juce::Font (11.5f, juce::Font::bold));
            label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent().brighter (0.08f));
            label->setJustificationType (juce::Justification::centred);
        }
        amountLabel.setText ("AMOUNT / RESONANCE", juce::dontSendNotification);
        rateLabel.setText ("RATE / TIME", juce::dontSendNotification);
        valueLabel.setText ("VALUE / TONE", juce::dontSendNotification);
        minLabel.setText ("MIN / SYNC", juce::dontSendNotification);
        maxLabel.setText ("MAX / RANGE", juce::dontSendNotification);
        curveLabel.setText ("CURVE / MIX", juce::dontSendNotification);

        configurePresetBoxes();

        for (auto* c : { static_cast<juce::Component*> (&editorTitle),
                         static_cast<juce::Component*> (&editorHint),
                         static_cast<juce::Component*> (&editorItemBox),
                         static_cast<juce::Component*> (&typeBox),
                         static_cast<juce::Component*> (&sourceBox),
                         static_cast<juce::Component*> (&targetBox),
                         static_cast<juce::Component*> (&globalPresetBox),
                         static_cast<juce::Component*> (&sectionPresetBox),
                         static_cast<juce::Component*> (&deleteGraphItemButton),
                         static_cast<juce::Component*> (&enableGraphItemButton),
                         static_cast<juce::Component*> (&amountLabel),
                         static_cast<juce::Component*> (&rateLabel),
                         static_cast<juce::Component*> (&valueLabel),
                         static_cast<juce::Component*> (&minLabel),
                         static_cast<juce::Component*> (&maxLabel),
                         static_cast<juce::Component*> (&curveLabel),
                         static_cast<juce::Component*> (&amountSlider),
                         static_cast<juce::Component*> (&rateSlider),
                         static_cast<juce::Component*> (&valueSlider),
                         static_cast<juce::Component*> (&minSlider),
                         static_cast<juce::Component*> (&maxSlider),
                         static_cast<juce::Component*> (&curveSlider),
                         static_cast<juce::Component*> (&amountSwitch),
                         static_cast<juce::Component*> (&rateSwitch),
                         static_cast<juce::Component*> (&valueSwitch),
                         static_cast<juce::Component*> (&minSwitch),
                         static_cast<juce::Component*> (&maxSwitch),
                         static_cast<juce::Component*> (&curveSwitch) })
            addChildComponent (*c);

        for (auto* slider : { &amountSlider, &rateSlider, &valueSlider, &minSlider, &maxSlider, &curveSlider })
        {
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
            slider->onValueChange = [this] { applyGraphEditorChange(); };
        }
        amountSlider.setName ("Amount");
        rateSlider.setName ("Rate");
        valueSlider.setName ("Value");
        minSlider.setName ("Min");
        maxSlider.setName ("Max");
        curveSlider.setName ("Curve");
        amountSlider.setRange (-1.0, 1.0, 0.001);
        rateSlider.setRange (0.01, 40.0, 0.001);
        valueSlider.setRange (0.0, 1.0, 0.001);
        minSlider.setRange (0.0, 1.0, 0.001);
        maxSlider.setRange (0.0, 1.0, 0.001);
        curveSlider.setRange (0.05, 4.0, 0.001);
        valueSlider.setSkewFactor (1.0);

        auto configureSwitch = [this] (juce::ToggleButton& button, juce::Slider& linkedSlider)
        {
            button.getProperties().set ("smallButton", true);
            button.onClick = [&button, &linkedSlider]
            {
                const bool on = button.getToggleState();
                button.setButtonText (on ? "ON" : "OFF");
                linkedSlider.setValue (on ? 1.0 : 0.0, juce::sendNotificationSync);
            };
        };
        configureSwitch (amountSwitch, amountSlider);
        configureSwitch (rateSwitch, rateSlider);
        configureSwitch (valueSwitch, valueSlider);
        configureSwitch (minSwitch, minSlider);
        configureSwitch (maxSwitch, maxSlider);
        configureSwitch (curveSwitch, curveSlider);

        editorItemBox.onChange = [this] { selectGraphEditorItem (editorItemBox.getSelectedId()); };
        typeBox.onChange = [this] { applyGraphEditorChange(); };
        sourceBox.onChange = [this] { applyGraphEditorChange(); };
        targetBox.onChange = [this] { applyGraphEditorChange(); };
        deleteGraphItemButton.getProperties().set ("smallButton", true);
        deleteGraphItemButton.onClick = [this] { deleteSelectedGraphItem(); };
        enableGraphItemButton.getProperties().set ("smallButton", true);
        enableGraphItemButton.onClick = [this] { toggleSelectedGraphItemEnabled(); };

        hoverHelpLabel.setInterceptsMouseClicks (false, false);
        hoverHelpLabel.setJustificationType (juce::Justification::centredLeft);
        hoverHelpLabel.setFont (juce::Font (11.0f));
        hoverHelpLabel.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.90f));
        hoverHelpLabel.setColour (juce::Label::outlineColourId, PatchCraftLookAndFeel::accent().withAlpha (0.70f));
        hoverHelpLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.94f));
        addChildComponent (hoverHelpLabel);
        addMouseListener (this, true);

        project.addListener (this);
        project.getLiveValues().addListener (this);
        startTimerHz (30);
        ensureQuickEditControls();
        bindSections();
        syncEngineButtons();
        refreshExpansionChoices();
        refreshQuickEditSections();
        refreshEasyModeSummary();
        refreshBuilderPanel();
        rebuildGraphEditorItems();
        rebuildVisibility();
    }

    DspPage::~DspPage()
    {
        removeMouseListener (this);
        stopTimer();
        stopFxSamplePlayback();
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void DspPage::resetGraphControlModes()
    {
        graphControlIsSwitch.fill (false);

        for (auto* slider : { &amountSlider, &rateSlider, &valueSlider, &minSlider, &maxSlider, &curveSlider })
        {
            slider->setVisible (! quickEdit);
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
            slider->setSkewFactor (1.0);
        }

        for (auto* button : { &amountSwitch, &rateSwitch, &valueSwitch, &minSwitch, &maxSwitch, &curveSwitch })
            button->setVisible (false);
    }

    void DspPage::setGraphControlSwitchMode (int index, bool active, bool enabled, const juce::String& tooltip)
    {
        std::array<juce::Slider*, 6> sliders { &amountSlider, &rateSlider, &valueSlider,
                                               &minSlider, &maxSlider, &curveSlider };
        std::array<juce::ToggleButton*, 6> switches { &amountSwitch, &rateSwitch, &valueSwitch,
                                                     &minSwitch, &maxSwitch, &curveSwitch };

        if (index < 0 || index >= (int) graphControlIsSwitch.size())
            return;

        graphControlIsSwitch[(size_t) index] = active;
        auto* slider = sliders[(size_t) index];
        auto* button = switches[(size_t) index];
        if (slider == nullptr || button == nullptr)
            return;

        if (! active)
        {
            button->setVisible (false);
            slider->setVisible (! quickEdit);
            return;
        }

        const bool on = slider->getValue() >= 0.5;
        slider->setVisible (false);
        slider->setEnabled (false);
        button->setVisible (! quickEdit);
        button->setEnabled (enabled);
        button->setToggleState (on, juce::dontSendNotification);
        button->setButtonText (on ? "ON" : "OFF");
        button->setTooltip (tooltip);
        slider->setTooltip (tooltip);
    }

    void DspPage::syncGraphControlLabelTooltips()
    {
        std::array<juce::Label*, 6> labels { &amountLabel, &rateLabel, &valueLabel,
                                             &minLabel, &maxLabel, &curveLabel };
        std::array<juce::Slider*, 6> sliders { &amountSlider, &rateSlider, &valueSlider,
                                               &minSlider, &maxSlider, &curveSlider };
        std::array<juce::ToggleButton*, 6> switches { &amountSwitch, &rateSwitch, &valueSwitch,
                                                     &minSwitch, &maxSwitch, &curveSwitch };

        for (int i = 0; i < (int) labels.size(); ++i)
            labels[(size_t) i]->setTooltip (graphControlIsSwitch[(size_t) i]
                ? switches[(size_t) i]->getTooltip()
                : sliders[(size_t) i]->getTooltip());
    }

    void DspPage::beginGuidedTutorial()
    {
        if (guidedTutorial == nullptr)
        {
            guidedTutorial = std::make_unique<TutorialOverlay> (*this);
            addAndMakeVisible (*guidedTutorial);
        }

        setTab (0);
        guidedTutorial->setBounds (getLocalBounds());
        guidedTutorial->restart();
    }

    void DspPage::addArpBlock()
    {
        // Mirrors the BuilderPanel's "+ MIDI" click behaviour so the canvas
        // right-click menu can drop an arpeggiator without forcing the user
        // to navigate the DSP builder by hand.
        if (currentTab != 3)
            setTab (3);

        auto& graph = project.getDspGraph();
        const auto previousSize = (int) graph.blocks.size();
        addBuilderBlock();
        if ((int) graph.blocks.size() <= previousSize)
            return;

        auto& block = graph.blocks.back();
        const auto previousType = block.type;
        block.type = "arp";
        block.name = "MIDI Playground " + juce::String ((int) graph.blocks.size());
        applyBlockTypeDefaults (block, previousType);
        selectedGraphKind = 1;
        selectedGraphIndex = (int) graph.blocks.size() - 1;
        project.notifyChanged();
        rebuildGraphEditorItems();
        refreshBuilderPanel();
        syncGraphEditor();
    }

    void DspPage::bindSections()
    {
        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
            section->bind (project);
    }

    void DspPage::setTab (int index)
    {
        currentTab = index;
        selectedGraphKind = 0;
        selectedGraphIndex = -1;
        refreshBuilderPanel();
        rebuildGraphEditorItems();
        rebuildVisibility();
        if (onPatchSectionChanged)
            onPatchSectionChanged();
    }

    void DspPage::setWorkflowMode (bool easy)
    {
        if (quickEdit)
            return;

        easyMode = easy;
        easyModeButton.setToggleState (easyMode, juce::dontSendNotification);
        advancedModeButton.setToggleState (! easyMode, juce::dontSendNotification);
        refreshEasyModeSummary();
        rebuildVisibility();
    }

    void DspPage::refreshEasyModeSummary()
    {
        const auto theme = easyThemeBox.getText().isNotEmpty() ? easyThemeBox.getText() : juce::String ("Arps");
        easyHelpLabel.setText (
            "Easy mode builds a complete playable patch, not a placeholder. Pick a musical direction, create or randomize it, then use Advanced only when you want to inspect or surgically edit the blocks.",
            juce::dontSendNotification);

        const auto packTarget = easyExpansionBox.getSelectedId() >= 100 ? easyExpansionBox.getText() : juce::String ("None");
        easyRecipeLabel.setText (
            "PATCH RECIPE\n"
            "Sound type: " + theme + "\n"
            "Engine: " + project.getEngineType() + "\n"
            "Blocks: " + easyBlockCountSummary() + "\n"
            "Samples: " + juce::String ((int) project.getSampleMap().getZones().size()) + " mapped zone(s)\n"
            "Expansion target: " + packTarget,
            juce::dontSendNotification);

        easyParametersLabel.setText (
            "PARAMS YOU CAN EDIT\n"
            + easyParameterSummary() + "\n\n"
            "To expose a parameter on the instrument UI, go to Design, add a knob/slider/toggle, then assign its parameter in the Inspector.",
            juce::dontSendNotification);

        easyWorkflowLabel.setText (
            "START-TO-FINISH\n"
            "1. Choose a sound type and click Create Preset.\n"
            "2. Generate Random for a different musical variation of that same type.\n"
            "3. Enable Add to Pack if this preset belongs in a sellable expansion.\n"
            "4. Open Advanced to edit source, filter, amp, mod, FX, output, macros, and graph routes.\n"
            "5. Test, save the patch, then export the Player pack.",
            juce::dontSendNotification);
    }

    juce::String DspPage::easyBlockCountSummary() const
    {
        std::map<juce::String, int> counts;
        for (const auto& block : project.getDspGraph().blocks)
            ++counts[block.section.isNotEmpty() ? block.section : juce::String ("source")];

        auto value = [&] (const juce::String& section)
        {
            auto it = counts.find (section);
            return it == counts.end() ? 0 : it->second;
        };

        return "Source " + juce::String (value ("source"))
             + ", Filter " + juce::String (value ("filter"))
             + ", Amp " + juce::String (value ("amp"))
             + ", Mod " + juce::String (value ("mod"))
             + ", FX " + juce::String (value ("fx"))
             + ", Out " + juce::String (value ("out"));
    }

    juce::String DspPage::easyParameterSummary() const
    {
        const auto& live = project.getLiveValues();
        const auto& parameters = project.getParameters();
        juce::StringArray lines;

        auto add = [&] (const juce::String& id)
        {
            if (const auto* def = parameters.find (id))
            {
                const auto value = live.getValue (id, def->defaultValue);
                juce::String display = def->name.isNotEmpty() ? def->name : id;
                display << ": " << juce::String (value, def->step >= 1.0f ? 0 : 2);
                if (def->unit.isNotEmpty())
                    display << " " << def->unit;
                lines.add (display);
            }
        };

        if (project.getEngineType() == "sample")
        {
            add ("sampleStart");
            add ("sampleLength");
            add ("samplePitch");
            add ("granularDensity");
            add ("filterCutoff");
            add ("reverbMix");
        }
        else if (project.getEngineType() == "fx")
        {
            add ("drive");
            add ("delayMix");
            add ("reverbMix");
            add ("multiTapMix");
            add ("lofiMix");
            add ("volume");
        }
        else
        {
            add ("oscBlend");
            add ("filterCutoff");
            add ("filterResonance");
            add ("attack");
            add ("delayMix");
            add ("reverbMix");
        }

        if (lines.isEmpty())
            return "No editable parameters are available for this engine yet.";

        return lines.joinIntoString ("\n");
    }

    void DspPage::applyEasyPreset()
    {
        const auto theme = easyThemeBox.getText().isNotEmpty() ? easyThemeBox.getText() : juce::String ("Arps");
        applyEasyPresetForTheme (theme, false);
    }

    void DspPage::applyEasyPresetForTheme (const juce::String& theme, bool forceRandomSeed)
    {
        if (quickEdit)
            return;

        PresetGenerationOptions options;
        options.theme = theme;
        options.count = 1;
        options.includeCurrentAsAnchor = ! forceRandomSeed;
        if (forceRandomSeed)
        {
            const auto ticks = (juce::uint64) juce::Time::getHighResolutionTicks();
            options.seed = (juce::uint32) (ticks ^ (ticks >> 32)
                                           ^ (juce::uint32) juce::Random::getSystemRandom().nextInt());
            if (options.seed == 0)
                options.seed = (juce::uint32) juce::Time::getMillisecondCounter();
        }

        auto generated = PresetGenerator::generate (project.getParameters(),
                                                    project.getLiveValues(),
                                                    project.getEngineType(),
                                                    options);
        auto& graph = project.getDspGraph();
        graph.resetForEngine (project.getEngineType());
        graph.userConfigured = true;

        Preset preset;
        bool hasPreset = false;
        if (! generated.empty())
        {
            preset = generated.front();
            preset.name = "Easy " + theme + " " + juce::Time::getCurrentTime().formatted ("%H%M%S")
                + (forceRandomSeed ? ("-" + juce::String::toHexString ((int) options.seed).toUpperCase().substring (0, 4))
                                   : juce::String());
            preset.generated = true;
            if (forceRandomSeed)
                applyEasyRandomVariation (preset, options.seed);
            clampEasyPresetSafety (preset);
            for (const auto& value : preset.values)
                project.getLiveValues().setValue (value.first, value.second);
            project.getManifest().defaultPreset = preset.name;
            hasPreset = true;
        }

        const auto lower = theme.toLowerCase();
        if (lower.contains ("arp"))
        {
            setTab (3);
            applySectionPreset (403);
            setTab (2);
            applySectionPreset (501);
        }
        else if (lower.contains ("pluck"))
        {
            setTab (2);
            applySectionPreset (501);
            setTab (1);
            applySectionPreset (203);
        }
        else if (lower.contains ("string") || lower.contains ("pad"))
        {
            applyGlobalPreset (2);
        }
        else if (lower.contains ("wavetable") || lower == "wt")
        {
            setTab (0);
            const int sourcePresetId = 301 + (int) ((options.seed != 0 ? options.seed : (juce::uint32) theme.hashCode()) % 3u);
            applySectionPreset (sourcePresetId);
            setTab (1);
            applySectionPreset (202);
            setTab (2);
            applySectionPreset (501);
        }
        else if (lower.contains ("bass"))
        {
            setTab (0);
            applySectionPreset (303);
            setTab (1);
            applySectionPreset (203);
        }
        else if (lower.contains ("fx"))
        {
            setTab (4);
            applySectionPreset (105);
        }
        else if (lower.contains ("lfo") || lower.contains ("motion"))
        {
            setTab (3);
            applySectionPreset (401);
        }
        else
        {
            applyGlobalPreset (1);
        }

        if (forceRandomSeed)
            randomizeEasyGraphBlocks (options.seed);

        configureEasyMidiForTheme (theme, options.seed, forceRandomSeed);
        if (hasPreset)
            syncEasyPresetValuesToGraphBlocks (preset);

        if (hasPreset)
        {
            clampEasyPresetSafety (preset);
            for (const auto& value : preset.values)
                project.getLiveValues().setValue (value.first, value.second);
        }

        if (hasPreset)
            upsertPlayablePreset (std::move (preset),
                                  easyAddToPackToggle.getToggleState()
                                  || easyExpansionBox.getSelectedId() >= 100);

        refreshEasyModeSummary();
        project.notifyChanged();
        refresh();
        setWorkflowMode (true);
    }

    void DspPage::applyEasyRandomVariation (Preset& preset, juce::uint32 seed)
    {
        juce::Random rng ((juce::int64) seed ^ (juce::int64) juce::Time::getHighResolutionTicks());

        auto setRaw = [&] (const juce::String& id, float value)
        {
            if (const auto* def = project.getParameters().find (id))
                preset.values[id] = juce::jlimit (def->min, def->max, value);
        };

        auto setRange = [&] (const juce::String& id, float low, float high)
        {
            setRaw (id, low + rng.nextFloat() * (high - low));
        };

        auto setNorm = [&] (const juce::String& id, float low01, float high01)
        {
            if (const auto* def = project.getParameters().find (id))
                setRaw (id, def->min + (low01 + rng.nextFloat() * (high01 - low01)) * (def->max - def->min));
        };

        auto setLogHz = [&] (const juce::String& id, float lowHz, float highHz)
        {
            if (project.getParameters().find (id) == nullptr)
                return;

            const auto low = std::log (juce::jmax (1.0f, lowHz));
            const auto high = std::log (juce::jmax (lowHz + 1.0f, highHz));
            setRaw (id, std::exp (low + rng.nextFloat() * (high - low)));
        };

        const auto lower = preset.theme.toLowerCase();

        setNorm ("volume", 0.44f, lower.contains ("arp") ? 0.66f : 0.74f);
        setLogHz ("filterCutoff", 380.0f, lower.contains ("string") || lower.contains ("pad") ? 11000.0f : 13500.0f);
        setRange ("filterResonance", 0.04f, lower.contains ("arp") ? 0.48f : 0.62f);
        setRange ("delayMix", lower.contains ("arp") || lower.contains ("motion") ? 0.10f : 0.0f, lower.contains ("arp") ? 0.36f : 0.46f);
        setRange ("delayFeedback", 0.12f, lower.contains ("arp") ? 0.58f : 0.68f);
        setRange ("delayTime", 0.08f, 0.86f);
        setRange ("reverbMix", lower.contains ("pluck") || lower.contains ("bass") ? 0.02f : 0.10f, lower.contains ("arp") ? 0.34f : 0.58f);

        if (project.getEngineType() == "synth")
        {
            setRaw ("oscType", (float) rng.nextInt (5));
            setRaw ("osc2Type", (float) rng.nextInt (5));
            setRange ("oscBlend", 0.08f, 0.76f);
            setRange ("osc2Detune", -18.0f, 18.0f);
            setRange ("subBlend", 0.0f, lower.contains ("bass") ? 0.62f : 0.30f);
            setRange ("noiseBlend", 0.0f, lower.contains ("fx") ? 0.30f : lower.contains ("arp") ? 0.10f : 0.18f);
        }

        if (lower.contains ("arp"))
        {
            setRange ("attack", 0.001f, 0.045f);
            setRange ("decay", 0.045f, 0.42f);
            setRange ("sustain", 0.04f, 0.50f);
            setRange ("release", 0.035f, 0.46f);
            setRange ("lfoAmount", 0.10f, 0.55f);
            setRange ("lfoRate", 0.35f, 8.0f);
            setRange ("vibratoDepth", 0.0f, 0.16f);
            setRange ("vibratoRate", 2.0f, 11.5f);
        }
        else if (lower.contains ("pluck"))
        {
            setRange ("attack", 0.001f, 0.02f);
            setRange ("decay", 0.06f, 0.55f);
            setRange ("sustain", 0.0f, 0.35f);
            setRange ("release", 0.05f, 0.55f);
        }
        else if (lower.contains ("string") || lower.contains ("pad"))
        {
            setRange ("attack", 0.45f, 3.8f);
            setRange ("decay", 0.40f, 3.5f);
            setRange ("sustain", 0.45f, 1.0f);
            setRange ("release", 1.0f, 7.0f);
            setRange ("lfoAmount", 0.06f, 0.48f);
            setRange ("lfoRate", 0.12f, 4.2f);
        }
        else if (lower.contains ("bass"))
        {
            setRange ("attack", 0.001f, 0.035f);
            setRange ("decay", 0.08f, 0.50f);
            setRange ("sustain", 0.35f, 0.92f);
            setRange ("release", 0.05f, 0.38f);
            setLogHz ("filterCutoff", 180.0f, 5200.0f);
        }
    }

    void DspPage::clampEasyPresetSafety (Preset& preset)
    {
        auto clamp = [&] (const juce::String& id, float low, float high)
        {
            auto it = preset.values.find (id);
            if (it != preset.values.end())
                it->second = juce::jlimit (low, high, it->second);
        };

        const auto lower = preset.theme.toLowerCase();
        clamp ("volume", 0.0f, lower.contains ("arp") ? 0.66f : 0.78f);
        clamp ("wtLevel", 0.0f, 0.92f);
        clamp ("drive", 0.0f, 0.58f);
        clamp ("mix", 0.0f, 1.0f);
        clamp ("delayMix", 0.0f, lower.contains ("arp") ? 0.38f : 0.52f);
        clamp ("delayFeedback", 0.0f, lower.contains ("arp") ? 0.62f : 0.72f);
        clamp ("reverbMix", 0.0f, lower.contains ("arp") ? 0.36f : 0.62f);
        clamp ("filterResonance", 0.0f, lower.contains ("arp") ? 0.55f : 0.72f);
        clamp ("noiseBlend", 0.0f, lower.contains ("arp") ? 0.12f : 0.32f);
        clamp ("subBlend", 0.0f, lower.contains ("bass") ? 0.68f : 0.40f);
        clamp ("lfoAmount", 0.0f, lower.contains ("arp") ? 0.58f : 0.78f);
        clamp ("vibratoDepth", 0.0f, lower.contains ("arp") ? 0.16f : 0.36f);
        clamp ("outputGainDb", -24.0f, 0.0f);
        preset.values["outputLimiter"] = 1.0f;
        preset.values["outputCeilingDb"] = -1.0f;
    }

    void DspPage::syncEasyPresetValuesToGraphBlocks (const Preset& preset)
    {
        auto& graph = project.getDspGraph();
        const auto& model = project.getParameters();

        auto raw = [&] (const juce::String& id, float fallback)
        {
            const auto it = preset.values.find (id);
            return it == preset.values.end() ? fallback : it->second;
        };

        auto normalised = [&] (const juce::String& id, float fallback)
        {
            const auto it = preset.values.find (id);
            const auto* def = model.find (id);
            if (it == preset.values.end() || def == nullptr || std::abs (def->max - def->min) <= 0.000001f)
                return fallback;

            return juce::jlimit (0.0f, 1.0f, (it->second - def->min) / (def->max - def->min));
        };

        for (auto& block : graph.blocks)
        {
            if (block.section == "source")
            {
                if (block.type.containsIgnoreCase ("wavetable"))
                {
                    block.targetId = "wtPosition";
                    block.values["wtEnabled"] = 1.0f;
                    block.values["wtTable"] = raw ("wtTable", block.values.count ("wtTable") ? block.values["wtTable"] : 0.0f);
                    block.values["wtPosition"] = raw ("wtPosition", block.values.count ("wtPosition") ? block.values["wtPosition"] : 0.0f);
                    block.values["wtMorph"] = raw ("wtMorph", block.values.count ("wtMorph") ? block.values["wtMorph"] : 0.0f);
                    block.values["wtWarp"] = raw ("wtWarp", block.values.count ("wtWarp") ? block.values["wtWarp"] : 0.0f);
                    block.values["wtFold"] = raw ("wtFold", block.values.count ("wtFold") ? block.values["wtFold"] : 0.0f);
                    block.values["wtUnison"] = raw ("wtUnison", block.values.count ("wtUnison") ? block.values["wtUnison"] : 1.0f);
                    block.values["wtDetune"] = raw ("wtDetune", block.values.count ("wtDetune") ? block.values["wtDetune"] : 12.0f);
                    block.values["wtSpread"] = raw ("wtSpread", block.values.count ("wtSpread") ? block.values["wtSpread"] : 0.0f);
                    block.values["wtLevel"] = juce::jlimit (0.0f, 0.92f, raw ("wtLevel", block.values.count ("wtLevel") ? block.values["wtLevel"] : 0.75f));
                    block.values["wtBend"] = raw ("wtBend", block.values.count ("wtBend") ? block.values["wtBend"] : 0.0f);
                    block.values["wtSyncRatio"] = raw ("wtSyncRatio", block.values.count ("wtSyncRatio") ? block.values["wtSyncRatio"] : 1.0f);
                    block.values["wtSpectralTilt"] = raw ("wtSpectralTilt", block.values.count ("wtSpectralTilt") ? block.values["wtSpectralTilt"] : 0.0f);
                    block.values["wtPhaseMode"] = raw ("wtPhaseMode", block.values.count ("wtPhaseMode") ? block.values["wtPhaseMode"] : 0.0f);
                    block.values["wtFramePosition"] = raw ("wtFramePosition", block.values.count ("wtFramePosition") ? block.values["wtFramePosition"] : 0.0f);
                    block.values["wtFrameCount"] = raw ("wtFrameCount", block.values.count ("wtFrameCount") ? block.values["wtFrameCount"] : 1.0f);
                }
                else
                {
                    block.targetId = block.type.containsIgnoreCase ("noise") ? "noiseBlend" : "oscBlend";
                    block.values["oscType"] = normalised ("oscType", block.values.count ("oscType") ? block.values["oscType"] : 0.25f);
                    block.values["osc2Type"] = normalised ("osc2Type", block.values.count ("osc2Type") ? block.values["osc2Type"] : 0.75f);
                    block.values["oscBlend"] = normalised ("oscBlend", block.values.count ("oscBlend") ? block.values["oscBlend"] : 0.0f);
                    block.values["osc2Detune"] = normalised ("osc2Detune", block.values.count ("osc2Detune") ? block.values["osc2Detune"] : 0.535f);
                    block.values["detune"] = normalised ("detune", block.values.count ("detune") ? block.values["detune"] : 0.50f);
                    block.values["octave"] = normalised ("octave", block.values.count ("octave") ? block.values["octave"] : 0.50f);
                    block.values["subBlend"] = normalised ("subBlend", block.values.count ("subBlend") ? block.values["subBlend"] : 0.0f);
                    block.values["noiseBlend"] = normalised ("noiseBlend", block.values.count ("noiseBlend") ? block.values["noiseBlend"] : 0.0f);
                    block.values["volume"] = juce::jmin (0.72f, normalised ("volume", block.values.count ("volume") ? block.values["volume"] : 0.65f));
                }
            }
            else if (block.section == "filter" && ! block.type.containsIgnoreCase ("eq"))
            {
                block.targetId = "filterCutoff";
                block.values["cutoff"] = normalised ("filterCutoff", block.values.count ("cutoff") ? block.values["cutoff"] : 0.50f);
                block.values["resonance"] = normalised ("filterResonance", block.values.count ("resonance") ? block.values["resonance"] : 0.20f);
                block.values["lfoAmount"] = normalised ("lfoAmount", block.values.count ("lfoAmount") ? block.values["lfoAmount"] : 0.0f);
            }
            else if (block.section == "amp")
            {
                block.targetId = "attack";
                block.values["attack"] = normalised ("attack", block.values.count ("attack") ? block.values["attack"] : 0.01f);
                block.values["decay"] = normalised ("decay", block.values.count ("decay") ? block.values["decay"] : 0.20f);
                block.values["sustain"] = normalised ("sustain", block.values.count ("sustain") ? block.values["sustain"] : 0.80f);
                block.values["release"] = normalised ("release", block.values.count ("release") ? block.values["release"] : 0.40f);
            }
            else if (block.section == "mod" && block.type.containsIgnoreCase ("lfo"))
            {
                block.targetId = "filterCutoff";
                block.values["rate"] = raw ("lfoRate", block.values.count ("rate") ? block.values["rate"] : 1.0f);
                block.values["amount"] = normalised ("lfoAmount", block.values.count ("amount") ? block.values["amount"] : 0.15f);
                block.values["sync"] = raw ("bpmSync", 1.0f) >= 0.5f ? 1.0f : 0.0f;
            }
            else if (block.section == "fx")
            {
                if (block.type.containsIgnoreCase ("delay"))
                {
                    block.targetId = "delayMix";
                    block.values["delayMix"] = normalised ("delayMix", block.values.count ("delayMix") ? block.values["delayMix"] : 0.18f);
                    block.values["delayFeedback"] = normalised ("delayFeedback", block.values.count ("delayFeedback") ? block.values["delayFeedback"] : 0.35f);
                    block.values["delayTime"] = normalised ("delayTime", block.values.count ("delayTime") ? block.values["delayTime"] : 0.25f);
                    block.values["sync"] = raw ("bpmSync", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                }
                else if (block.type.containsIgnoreCase ("reverb"))
                {
                    block.targetId = "reverbMix";
                    block.values["reverbMix"] = normalised ("reverbMix", block.values.count ("reverbMix") ? block.values["reverbMix"] : 0.20f);
                }
                else if (block.values.count ("mix") != 0)
                {
                    block.values["mix"] = normalised ("mix", block.values["mix"]);
                }
            }
            else if (block.section == "out")
            {
                block.values["volume"] = juce::jmin (0.72f, normalised ("volume", block.values.count ("volume") ? block.values["volume"] : 0.65f));
                block.values["pan"] = normalised ("pan", block.values.count ("pan") ? block.values["pan"] : 0.50f);
                block.values["bpmSync"] = raw ("bpmSync", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                block.values["retrigger"] = raw ("retrigger", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                block.values["outputLimiter"] = 1.0f;
                block.values["outputCeilingDb"] = -1.0f;
                block.values["outputGainDb"] = juce::jmin (0.0f, raw ("outputGainDb", 0.0f));
            }
        }

        graph.userConfigured = true;
        normaliseDspGraphBanks (graph);
    }

    DspBlock& DspPage::ensureEasyMidiPlaygroundBlock()
    {
        auto& graph = project.getDspGraph();

        for (int i = 0; i < (int) graph.blocks.size(); ++i)
        {
            auto& block = graph.blocks[(size_t) i];
            if (block.section == "mod"
                && (block.type.containsIgnoreCase ("midi")
                    || block.type.containsIgnoreCase ("arp")
                    || block.type.containsIgnoreCase ("sequencer")))
            {
                const auto previousType = block.type;
                block.type = "midiPlayground";
                block.name = block.name.isNotEmpty() ? block.name : "Easy MIDI Playground";
                block.section = "mod";
                block.enabled = true;
                applyBlockTypeDefaults (block, previousType);
                selectedGraphKind = 1;
                selectedGraphIndex = i;
                return block;
            }
        }

        auto idExists = [&] (const juce::String& id)
        {
            for (const auto& block : graph.blocks)
                if (block.id == id)
                    return true;
            return false;
        };

        juce::String blockId = "easy_midi_playground";
        for (int suffix = 2; idExists (blockId); ++suffix)
            blockId = "easy_midi_playground_" + juce::String (suffix);

        int bank = 0;
        for (int candidate = 0; candidate < kSourceMatrixBankCount; ++candidate)
        {
            if (countBlocksInSectionBank (graph, "mod", candidate) < kSourceMatrixBankSize)
            {
                bank = candidate;
                break;
            }
        }

        DspBlock block;
        block.id = blockId;
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "Easy MIDI Playground";
        block.targetId = "filterCutoff";
        block.enabled = true;
        block.values["bank"] = (float) bank;
        applyBlockTypeDefaults (block, {});

        graph.blocks.push_back (std::move (block));
        normaliseDspGraphSectionBanks (graph, "mod");
        selectedGraphKind = 1;
        selectedGraphIndex = (int) graph.blocks.size() - 1;
        return graph.blocks.back();
    }

    void DspPage::configureEasyMidiForTheme (const juce::String& theme, juce::uint32 seed, bool forceVariation)
    {
        if (seed == 0)
            seed = (juce::uint32) (0x5043455au ^ (juce::uint32) theme.hashCode());

        auto& block = ensureEasyMidiPlaygroundBlock();
        const auto lower = theme.toLowerCase();
        juce::Random rng ((juce::int64) seed ^ 0x31415926);

        std::array<float, 16> notes { 0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 10.0f, 14.0f,
                                      12.0f, 7.0f, 4.0f, 0.0f, 5.0f, 9.0f, 12.0f, 16.0f };
        int steps = 16;
        int pattern = 0;
        int scaleType = 2;
        int chordMode = 0;
        int chordSize = 1;
        int octaves = 2;
        int ratchet = 1;
        float rate = 1.0f;
        float gate = 0.55f;
        float swing = 0.0f;
        float probability = 1.0f;
        float humanize = 0.04f;
        float mutation = forceVariation ? 0.06f : 0.0f;
        float chordSpread = 0.0f;
        float velocityCurve = 0.0f;
        float strum = 0.0f;
        float flam = 0.0f;
        int euclideanPulses = 0;
        int euclideanRotate = 0;
        bool octaveFold = false;
        bool sampleControl = project.getEngineType() == "sample";
        juce::String target = sampleControl ? "sampleSlice" : "filterCutoff";

        auto choosePattern = [&] (const std::array<std::array<float, 16>, 3>& patterns)
        {
            return patterns[(size_t) (forceVariation ? rng.nextInt ((int) patterns.size()) : 0)];
        };

        if (lower.contains ("arp"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 2.0f, 7.0f, 0.0f, 3.0f, 7.0f, 10.0f, 12.0f, 10.0f, 7.0f, 3.0f }},
                {{ 0.0f, 7.0f, 12.0f, 7.0f, 3.0f, 7.0f, 10.0f, 7.0f, 0.0f, 5.0f, 9.0f, 12.0f, 9.0f, 5.0f, 2.0f, 7.0f }},
                {{ 0.0f, 2.0f, 7.0f, 9.0f, 12.0f, 9.0f, 7.0f, 2.0f, 3.0f, 7.0f, 10.0f, 15.0f, 10.0f, 7.0f, 3.0f, 0.0f }}
            }});
            scaleType = forceVariation && rng.nextBool() ? 3 : 2;
            pattern = forceVariation ? (rng.nextBool() ? 2 : 6) : 0;
            rate = forceVariation && rng.nextBool() ? 2.0f : 1.0f;
            gate = 0.42f + (forceVariation ? rng.nextFloat() * 0.16f : 0.0f);
            swing = forceVariation ? 0.06f + rng.nextFloat() * 0.16f : 0.08f;
            octaves = forceVariation ? 2 + rng.nextInt (2) : 2;
            ratchet = forceVariation && rng.nextFloat() > 0.65f ? 2 : 1;
            humanize = 0.03f;
            mutation = forceVariation ? 0.05f : 0.0f;
            euclideanPulses = forceVariation ? (5 + rng.nextInt (4)) : 0;
            euclideanRotate = forceVariation ? rng.nextInt (4) : 0;
        }
        else if (lower.contains ("pluck"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 12.0f, 7.0f, 12.0f, 3.0f, 10.0f, 7.0f, 10.0f, 0.0f, 12.0f, 5.0f, 9.0f, 2.0f, 7.0f, 3.0f, 10.0f }},
                {{ 0.0f, 7.0f, 10.0f, 7.0f, 3.0f, 7.0f, 12.0f, 7.0f, 5.0f, 9.0f, 12.0f, 9.0f, 2.0f, 7.0f, 10.0f, 7.0f }},
                {{ 0.0f, 5.0f, 12.0f, 5.0f, 3.0f, 7.0f, 15.0f, 7.0f, 0.0f, 7.0f, 14.0f, 7.0f, 2.0f, 10.0f, 14.0f, 10.0f }}
            }});
            scaleType = 2;
            gate = 0.24f + (forceVariation ? rng.nextFloat() * 0.14f : 0.0f);
            swing = forceVariation ? 0.04f + rng.nextFloat() * 0.14f : 0.04f;
            probability = 0.96f;
            velocityCurve = 0.18f;
            flam = forceVariation ? 0.08f + rng.nextFloat() * 0.12f : 0.06f;
            euclideanPulses = forceVariation ? 5 + rng.nextInt (3) : 0;
        }
        else if (lower.contains ("bass"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 0.0f, -5.0f, 0.0f, -2.0f, 0.0f, -5.0f, -7.0f, 0.0f, 0.0f, -5.0f, 0.0f, 3.0f, 0.0f, -2.0f, -5.0f }},
                {{ 0.0f, -12.0f, 0.0f, -5.0f, 3.0f, -5.0f, 0.0f, -7.0f, 0.0f, -12.0f, 0.0f, -2.0f, 5.0f, 0.0f, -5.0f, -7.0f }},
                {{ 0.0f, 0.0f, 3.0f, 0.0f, -5.0f, 0.0f, 7.0f, 5.0f, 0.0f, -7.0f, 0.0f, -5.0f, 3.0f, 0.0f, -2.0f, -12.0f }}
            }});
            scaleType = 2;
            steps = 16;
            gate = 0.46f;
            swing = forceVariation ? 0.03f + rng.nextFloat() * 0.12f : 0.0f;
            octaves = 1;
            octaveFold = true;
            target = "filterCutoff";
            euclideanPulses = forceVariation ? 4 + rng.nextInt (3) : 4;
        }
        else if (lower.contains ("string") || lower.contains ("pad"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 0.0f, 5.0f, 5.0f, 7.0f, 7.0f, 3.0f, 3.0f, 0.0f, 0.0f, 5.0f, 5.0f, 10.0f, 10.0f, 7.0f, 7.0f }},
                {{ 0.0f, 4.0f, 7.0f, 11.0f, 5.0f, 9.0f, 12.0f, 16.0f, 3.0f, 7.0f, 10.0f, 14.0f, 2.0f, 5.0f, 9.0f, 12.0f }},
                {{ 0.0f, 7.0f, 12.0f, 7.0f, 5.0f, 12.0f, 17.0f, 12.0f, 3.0f, 10.0f, 15.0f, 10.0f, 7.0f, 14.0f, 19.0f, 14.0f }}
            }});
            scaleType = lower.contains ("string") ? 1 : 2;
            pattern = 2;
            gate = 0.86f;
            chordMode = 1;
            chordSize = forceVariation && rng.nextBool() ? 4 : 3;
            chordSpread = 0.24f + (forceVariation ? rng.nextFloat() * 0.22f : 0.0f);
            probability = 1.0f;
            humanize = 0.08f;
            strum = 0.16f + (forceVariation ? rng.nextFloat() * 0.18f : 0.0f);
        }
        else if (lower.contains ("fx"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 12.0f, 7.0f, 15.0f, 10.0f, 22.0f, 14.0f, 19.0f, 0.0f, -12.0f, -5.0f, 3.0f, 7.0f, 15.0f, 10.0f, 0.0f }},
                {{ 0.0f, 7.0f, 3.0f, 10.0f, 15.0f, 10.0f, 7.0f, 3.0f, 0.0f, -5.0f, 2.0f, 9.0f, 14.0f, 9.0f, 2.0f, -5.0f }},
                {{ 0.0f, 5.0f, 10.0f, 17.0f, 10.0f, 5.0f, -2.0f, 3.0f, 7.0f, 14.0f, 19.0f, 14.0f, 7.0f, 2.0f, -5.0f, 0.0f }}
            }});
            scaleType = 6;
            pattern = 7;
            gate = 0.34f;
            swing = 0.12f;
            probability = 0.88f;
            humanize = 0.14f;
            mutation = forceVariation ? 0.12f : 0.04f;
            flam = 0.14f;
            euclideanPulses = forceVariation ? 5 + rng.nextInt (5) : 0;
            sampleControl = sampleControl || ! project.getSampleMap().getZones().empty();
            target = sampleControl ? "sampleSlice" : "delayMix";
        }
        else if (lower.contains ("lfo") || lower.contains ("motion"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 7.0f, 4.0f, 2.0f, 0.0f, -2.0f, -5.0f, -2.0f, 0.0f, 4.0f, 7.0f, 12.0f }},
                {{ 0.0f, 5.0f, 9.0f, 12.0f, 14.0f, 12.0f, 9.0f, 5.0f, 3.0f, 7.0f, 10.0f, 15.0f, 10.0f, 7.0f, 3.0f, 0.0f }},
                {{ 0.0f, 7.0f, 2.0f, 9.0f, 4.0f, 12.0f, 7.0f, 14.0f, 5.0f, 12.0f, 7.0f, 14.0f, 3.0f, 10.0f, 5.0f, 12.0f }}
            }});
            scaleType = 3;
            pattern = 6;
            gate = 0.50f;
            swing = forceVariation ? 0.08f + rng.nextFloat() * 0.16f : 0.08f;
            probability = 0.92f;
            humanize = 0.06f;
            mutation = forceVariation ? 0.10f : 0.02f;
            euclideanPulses = forceVariation ? 5 + rng.nextInt (4) : 0;
            euclideanRotate = forceVariation ? rng.nextInt (8) : 0;
            target = "lfoAmount";
        }
        else if (lower.contains ("wavetable"))
        {
            notes = choosePattern ({{
                {{ 0.0f, 7.0f, 12.0f, 19.0f, 12.0f, 7.0f, 5.0f, 12.0f, 0.0f, 4.0f, 11.0f, 16.0f, 11.0f, 4.0f, 7.0f, 14.0f }},
                {{ 0.0f, 3.0f, 7.0f, 15.0f, 10.0f, 7.0f, 3.0f, 10.0f, 0.0f, 5.0f, 12.0f, 17.0f, 12.0f, 5.0f, 2.0f, 9.0f }},
                {{ 0.0f, 12.0f, 7.0f, 19.0f, 10.0f, 22.0f, 14.0f, 26.0f, 12.0f, 7.0f, 3.0f, 10.0f, 15.0f, 10.0f, 7.0f, 0.0f }}
            }});
            scaleType = 2;
            pattern = 2;
            gate = 0.58f;
            chordMode = 1;
            chordSize = 2;
            chordSpread = 0.18f;
            strum = 0.10f;
            target = "wtPosition";
            octaveFold = true;
        }

        static constexpr int roots[] { 0, 2, 3, 5, 7, 9, 10 };
        const int root = roots[forceVariation ? rng.nextInt ((int) (sizeof (roots) / sizeof (roots[0]))) : 0];
        const int rotation = forceVariation ? rng.nextInt (4) * 2 : 0;
        const float accentLift = forceVariation ? rng.nextFloat() * 0.08f : 0.0f;
        const int sampleSlices = juce::jlimit (1, 64, project.getSampleMap().getZones().empty()
            ? 16 : (int) project.getSampleMap().getZones().size());

        block.name = "Easy " + theme + " MIDI";
        block.type = "midiPlayground";
        block.section = "mod";
        block.targetId = target;
        block.enabled = true;
        block.values["rate"] = rate;
        block.values["sync"] = 1.0f;
        block.values["amount"] = 0.35f;
        block.values["arpSteps"] = (float) steps;
        block.values["arpPattern"] = (float) pattern;
        block.values["arpGate"] = gate;
        block.values["arpOctaves"] = (float) octaves;
        block.values["arpSwing"] = swing;
        block.values["mpScaleRoot"] = (float) root;
        block.values["mpScaleType"] = (float) scaleType;
        block.values["mpChordMode"] = (float) chordMode;
        block.values["mpChordSize"] = (float) chordSize;
        block.values["mpChordSpread"] = chordSpread;
        block.values["mpProbability"] = probability;
        block.values["mpHumanize"] = humanize;
        block.values["mpMutation"] = mutation;
        block.values["mpVelocityCurve"] = velocityCurve;
        block.values["mpOctaveFold"] = octaveFold ? 1.0f : 0.0f;
        block.values["mpRatchet"] = (float) ratchet;
        block.values["mpStrum"] = strum;
        block.values["mpFlam"] = flam;
        block.values["mpEuclideanPulses"] = (float) euclideanPulses;
        block.values["mpEuclideanRotate"] = (float) euclideanRotate;
        block.values["mpLatch"] = 0.0f;
        block.values["mpSampleControl"] = sampleControl ? 1.0f : 0.0f;
        block.values["mpSampleSliceCount"] = (float) sampleSlices;
        block.values["sampleSliceCount"] = (float) sampleSlices;
        block.values["mpSampleStart"] = 0.0f;
        block.values["mpSampleLength"] = lower.contains ("fx") ? 0.22f : 0.36f;
        block.values["mpSamplePitch"] = 0.0f;
        block.values["sampleStart"] = block.values["mpSampleStart"];
        block.values["sampleLength"] = block.values["mpSampleLength"];
        block.values["samplePitch"] = block.values["mpSamplePitch"];
        block.values["mpSeed"] = (float) seed;

        for (int step = 0; step < 16; ++step)
        {
            const int sourceIndex = (step + rotation) % 16;
            const bool stepActive = step < steps;
            const bool strongBeat = (step % 4) == 0;
            block.values["arpNote" + juce::String (step)] = notes[(size_t) sourceIndex];
            block.values["mpStep" + juce::String (step) + "On"] = stepActive ? 1.0f : 0.0f;
            block.values["mpVelocity" + juce::String (step)] = stepActive
                ? juce::jlimit (0.0f, 1.0f, (strongBeat ? 0.96f : 0.72f) + accentLift)
                : 0.0f;
            block.values["mpGate" + juce::String (step)] = stepActive ? gate : 0.05f;
            block.values["mpStepProb" + juce::String (step)] = stepActive ? probability : 0.0f;
            block.values["mpSampleSlice" + juce::String (step)] = sampleControl
                ? (float) (step % sampleSlices)
                : -1.0f;
        }

        project.getDspGraph().userConfigured = true;
    }

    void DspPage::randomizeEasyGraphBlocks (juce::uint32 seed)
    {
        auto& graph = project.getDspGraph();
        juce::Random rng ((juce::int64) seed ^ 0x5f3759df);

        auto setIfPresent = [&] (DspBlock& block, const juce::String& key, float low, float high)
        {
            if (block.values.count (key) != 0)
                block.values[key] = low + rng.nextFloat() * (high - low);
        };

        for (auto& block : graph.blocks)
        {
            if (! block.enabled)
                continue;

            if (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi"))
                continue;

            setIfPresent (block, "amount", 0.04f, 0.58f);
            setIfPresent (block, "depth", 0.04f, 0.62f);
            setIfPresent (block, "value", 0.08f, 0.86f);
            setIfPresent (block, "mix", 0.08f, 0.72f);
            setIfPresent (block, "rate", 0.12f, 10.0f);
            setIfPresent (block, "cutoff", 0.10f, 0.84f);
            setIfPresent (block, "resonance", 0.02f, 0.58f);
            setIfPresent (block, "drive", 0.0f, 0.52f);
            setIfPresent (block, "volume", 0.42f, 0.72f);
            setIfPresent (block, "detune", 0.36f, 0.64f);
            setIfPresent (block, "oscBlend", 0.10f, 0.74f);
            setIfPresent (block, "subBlend", 0.0f, 0.50f);
            setIfPresent (block, "noiseBlend", 0.0f, 0.20f);
            setIfPresent (block, "delayMix", 0.0f, 0.42f);
            setIfPresent (block, "delayFeedback", 0.12f, 0.62f);
            setIfPresent (block, "reverbMix", 0.02f, 0.50f);
            setIfPresent (block, "wtPosition", 0.0f, 1.0f);
            setIfPresent (block, "wtMorph", 0.0f, 1.0f);
            setIfPresent (block, "wtWarp", 0.0f, 0.72f);
            setIfPresent (block, "wtLevel", 0.42f, 0.88f);
        }

        graph.userConfigured = true;
    }

    void DspPage::refreshExpansionChoices()
    {
        auto refill = [this] (juce::ComboBox& box, int previousId)
        {
            box.clear (juce::dontSendNotification);
            box.addItem ("No Expansion Pack", 1);
            int itemId = 100;
            for (const auto& expansion : project.getExpansions())
                box.addItem (expansion.name.isNotEmpty() ? expansion.name : expansion.id, itemId++);

            if (previousId > 0)
                box.setSelectedId (previousId, juce::dontSendNotification);
            if (box.getSelectedId() == 0)
                box.setSelectedId (1, juce::dontSendNotification);
        };

        refill (easyExpansionBox, easyExpansionBox.getSelectedId());
        refill (builderPanel.expansionBox, builderPanel.expansionBox.getSelectedId());
    }

    void DspPage::selectExpansionById (const juce::String& expansionId)
    {
        int itemId = 100;
        int selectedId = 1;
        for (const auto& expansion : project.getExpansions())
        {
            if (expansion.id == expansionId)
            {
                selectedId = itemId;
                break;
            }
            ++itemId;
        }

        easyExpansionBox.setSelectedId (selectedId, juce::dontSendNotification);
        builderPanel.expansionBox.setSelectedId (selectedId, juce::dontSendNotification);
        if (selectedId >= 100)
            easyAddToPackToggle.setToggleState (true, juce::dontSendNotification);
    }

    ExpansionMetadata* DspPage::selectedExpansionForMode()
    {
        const int selectedId = easyMode ? easyExpansionBox.getSelectedId()
                                        : builderPanel.expansionBox.getSelectedId();
        const int index = selectedId - 100;
        auto& expansions = project.getExpansions();
        if (index >= 0 && index < (int) expansions.size())
            return &expansions[(size_t) index];
        return nullptr;
    }

    ExpansionMetadata& DspPage::ensureSelectedExpansion()
    {
        if (auto* expansion = selectedExpansionForMode())
            return *expansion;

        auto& expansion = project.ensureExpansion (project.getManifest().instrumentName + " Expansion");
        if (expansion.folders.isEmpty())
            expansion.folders = { "Arps", "Bass", "Pads", "Plucks", "Strings", "Wavetables", "FX" };
        refreshExpansionChoices();
        selectExpansionById (expansion.id);
        return expansion;
    }

    juce::String DspPage::categoryForPresetTheme (const juce::String& theme) const
    {
        const auto lower = theme.toLowerCase();
        if (lower.contains ("arp")) return "Arps";
        if (lower.contains ("lfo") || lower.contains ("motion")) return "Motion";
        if (lower.contains ("wavetable") || lower == "wt") return "Wavetables";
        if (lower.contains ("pluck")) return "Plucks";
        if (lower.contains ("string")) return "Strings";
        if (lower.contains ("pad")) return "Pads";
        if (lower.contains ("bass")) return "Bass";
        if (lower.contains ("fx")) return "FX";
        if (theme.isNotEmpty()) return theme;
        if (project.getManifest().category.isNotEmpty()) return project.getManifest().category;
        return project.getEngineType() == "fx" ? "FX" : "Patches";
    }

    void DspPage::addPatchPresetToExpansion (InstrumentPatch& patch,
                                             Preset& preset,
                                             ExpansionMetadata& expansion,
                                             const juce::String& category)
    {
        patch.expansionId = expansion.id;
        patch.packId = expansion.id;
        patch.category = category;
        preset.expansionId = expansion.id;
        preset.packId = expansion.id;

        for (const auto& tag : { category, "category:" + category, "expansion:" + expansion.name })
        {
            if (tag.isNotEmpty())
            {
                patch.tags.addIfNotAlreadyThere (tag);
                preset.tags.addIfNotAlreadyThere (tag);
                expansion.tags.addIfNotAlreadyThere (tag);
            }
        }

        if (expansion.category.isEmpty())
            expansion.category = category;
        expansion.folders.addIfNotAlreadyThere (category);
        expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
        expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
        for (const auto& asset : patch.includedAssets)
            expansion.includedAssets.addIfNotAlreadyThere (asset);
    }

    void DspPage::upsertPlayablePreset (Preset preset, bool addToExpansion)
    {
        for (auto& existing : project.getPatches())
            existing.isDefault = false;
        for (auto& existing : project.getPresets())
            existing.isDefault = false;

        auto patch = project.captureCurrentPatch (preset.name);
        patch.description = preset.description;
        patch.generated = preset.generated;
        patch.isDefault = true;
        patch.category = categoryForPresetTheme (preset.theme);
        patch.tags = preset.tags;
        patch.tags.addIfNotAlreadyThere (patch.category);

        auto playablePreset = patch.toPreset();
        playablePreset.theme = preset.theme;
        playablePreset.description = preset.description;
        playablePreset.generated = preset.generated;
        playablePreset.isDefault = true;
        playablePreset.tags = patch.tags;

        if (addToExpansion)
        {
            auto& expansion = ensureSelectedExpansion();
            addPatchPresetToExpansion (patch, playablePreset, expansion, patch.category);
        }

        const int patchIndex = findPatchIndexById (project, patch.id);
        if (patchIndex >= 0)
            project.getPatches()[(size_t) patchIndex] = patch;
        else
            project.getPatches().push_back (std::move (patch));

        const int presetIndex = findPresetIndexByName (project, playablePreset.name);
        if (presetIndex >= 0)
            project.getPresets()[(size_t) presetIndex] = playablePreset;
        else
            project.getPresets().push_back (std::move (playablePreset));

        project.getManifest().defaultPreset = preset.name;
    }

    void DspPage::addCurrentPatchToSelectedExpansion()
    {
        if (quickEdit)
            return;

        auto& expansion = ensureSelectedExpansion();
        auto patchName = project.getManifest().defaultPreset;
        if (patchName.isEmpty())
            patchName = project.getManifest().instrumentName + " Patch";

        for (auto& existing : project.getPatches())
            existing.isDefault = false;
        for (auto& existing : project.getPresets())
            existing.isDefault = false;

        auto patch = project.captureCurrentPatch (patchName);
        patch.isDefault = true;
        patch.generated = false;
        patch.category = categoryForPresetTheme (patch.category.isNotEmpty() ? patch.category : project.getManifest().category);
        patch.tags.addIfNotAlreadyThere (patch.category);
        auto preset = patch.toPreset();
        preset.theme = patch.category;
        preset.isDefault = true;
        preset.tags = patch.tags;

        addPatchPresetToExpansion (patch, preset, expansion, patch.category);

        const int patchIndex = findPatchIndexById (project, patch.id);
        if (patchIndex >= 0)
            project.getPatches()[(size_t) patchIndex] = patch;
        else
            project.getPatches().push_back (std::move (patch));

        const int presetIndex = findPresetIndexByName (project, preset.name);
        if (presetIndex >= 0)
            project.getPresets()[(size_t) presetIndex] = preset;
        else
            project.getPresets().push_back (std::move (preset));

        project.getManifest().defaultPreset = patchName;
        project.notifyChanged();
        refreshExpansionChoices();
        selectExpansionById (expansion.id);

        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("Added To Expansion")
                .withMessage (patchName + " was added to " + expansion.name + " under " + patch.category + ".")
                .withButton ("OK")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            nullptr);
    }

    void DspPage::showPackCreator()
    {
        auto* selected = selectedExpansionForMode();
        const auto defaultName = selected != nullptr && selected->name.isNotEmpty()
            ? selected->name
            : project.getManifest().instrumentName + " Expansion";

        auto* window = new juce::AlertWindow ("Pack Creator",
            "Create or edit a sellable expansion pack. Presets are automatically filed into folders by preset type when added.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", defaultName, "Pack Name:");
        window->addTextEditor ("author", selected != nullptr ? selected->author : project.getManifest().creator, "Author:");
        window->addTextEditor ("brand", selected != nullptr ? selected->brand : project.getManifest().playerDisplayName, "Brand:");
        window->addTextEditor ("category", selected != nullptr ? selected->category : project.getManifest().category, "Default Category:");
        window->addTextEditor ("folders", selected != nullptr ? selected->folders.joinIntoString (", ")
                                                             : "Arps, Bass, Pads, Plucks, Strings, Wavetables, FX",
                               "Folders / Groups:");
        window->addTextEditor ("tags", selected != nullptr ? selected->tags.joinIntoString (", ")
                                                          : project.getManifest().tags.joinIntoString (", "),
                               "Keywords:");
        window->addTextEditor ("description", selected != nullptr ? selected->description : project.getManifest().description, "Description:");
        window->addButton ("Save Pack", 1);
        window->addButton ("Cancel", 0);

        juce::Component::SafePointer<DspPage> safeThis (this);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([safeThis, window] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (safeThis == nullptr || result != 1)
                    return;

                auto& self = *safeThis;
                const auto name = window->getTextEditorContents ("name").trim();
                if (name.isEmpty())
                    return;

                auto& expansion = self.project.ensureExpansion (name);
                expansion.name = name;
                expansion.author = window->getTextEditorContents ("author").trim();
                expansion.brand = window->getTextEditorContents ("brand").trim();
                expansion.category = window->getTextEditorContents ("category").trim();
                expansion.description = window->getTextEditorContents ("description").trim();
                expansion.tags = packTokensFromText (window->getTextEditorContents ("tags"));
                expansion.folders = packTokensFromText (window->getTextEditorContents ("folders"));
                if (expansion.category.isNotEmpty())
                {
                    expansion.tags.addIfNotAlreadyThere (expansion.category);
                    expansion.folders.addIfNotAlreadyThere (expansion.category);
                }

                self.project.notifyChanged();
                self.refreshExpansionChoices();
                self.selectExpansionById (expansion.id);
                self.refreshEasyModeSummary();
            }), true);
    }

    void DspPage::setEngine (const juce::String& engineId)
    {
        if (project.getEngineType() == engineId) return;
        project.setEngineType (engineId);
    }

    void DspPage::syncEngineButtons()
    {
        auto engine = project.getEngineType();
        if (engine.isEmpty())
            engine = "synth";

        samplerEngineButton.setToggleState (engine == "sample", juce::dontSendNotification);
        synthEngineButton.setToggleState (engine == "synth", juce::dontSendNotification);
        fxEngineButton.setToggleState (engine == "fx", juce::dontSendNotification);

        subtitleLabel.setText ("Engine: " + engine + "    Parameters: "
                                  + juce::String ((int) project.getParameters().getAll().size()),
                              juce::dontSendNotification);
    }

    juce::String DspPage::currentSectionId() const
    {
        return currentTab == 0 ? "source"
             : currentTab == 1 ? "filter"
             : currentTab == 2 ? "amp"
             : currentTab == 3 ? "mod"
             : currentTab == 4 ? "fx" : "out";
    }

    int DspPage::currentSectionBank() const
    {
        return sectionBanks[juce::jlimit (0, 5, currentTab)];
    }

    void DspPage::setCurrentSectionBank (int bank)
    {
        sectionBanks[juce::jlimit (0, 5, currentTab)] = juce::jlimit (0, kSourceMatrixBankCount - 1, bank);
    }

    juce::String DspPage::getCurrentPatchSectionId() const
    {
        return currentSectionId();
    }

    juce::String DspPage::getCurrentPatchSectionLabel() const
    {
        return currentTab == 0 ? "Source"
             : currentTab == 1 ? "Filter"
             : currentTab == 2 ? "Amp"
             : currentTab == 3 ? "Mod"
             : currentTab == 4 ? "FX" : "Out";
    }

    void DspPage::markGraphEdited()
    {
        project.getDspGraph().userConfigured = true;
    }

    juce::String DspPage::sectionForTarget (const juce::String& targetId) const
    {
        for (const auto& block : project.getDspGraph().blocks)
            if (block.id == targetId)
                return block.section;

        if (targetId == "filterCutoff" || targetId == "filterResonance")
            return "filter";
        if (targetId.startsWith ("eq"))
            return "filter";
        if (targetId == "attack" || targetId == "decay" || targetId == "sustain" || targetId == "release")
            return "amp";
        if (targetId == "lfoRate" || targetId == "lfoAmount" || targetId == "vibratoRate" || targetId == "vibratoDepth")
            return "mod";
        if (targetId == "drive" || targetId == "mix" || targetId == "delayTime"
            || targetId == "delayFeedback" || targetId == "delayMix" || targetId == "reverbMix"
            || targetId == "dynThresholdDb" || targetId == "dynRatio" || targetId == "dynAttackMs"
            || targetId == "dynReleaseMs" || targetId == "dynMakeupDb" || targetId == "dynMix"
            || targetId == "chorusRate" || targetId == "chorusDepth" || targetId == "chorusFeedback"
            || targetId == "chorusMix" || targetId == "phaserRate" || targetId == "phaserDepth"
            || targetId == "phaserFeedback" || targetId == "phaserMix" || targetId == "combFreq"
            || targetId == "combFeedback" || targetId == "combMix" || targetId == "resonatorFreq"
            || targetId == "resonatorQ" || targetId == "resonatorMix" || targetId == "convolutionSize"
            || targetId == "convolutionMix" || targetId == "spectralTilt" || targetId == "spectralMix"
            || targetId == "tapeDrive" || targetId == "tapeTone" || targetId == "tapeFlutter" || targetId == "tapeMix"
            || targetId == "vinylAge" || targetId == "vinylDust" || targetId == "vinylWarp" || targetId == "vinylMix"
            || targetId == "lofiBits" || targetId == "lofiRate" || targetId == "lofiMix"
            || targetId == "vocalFormant" || targetId == "vocalBody" || targetId == "vocalMix"
            || targetId == "multiTapTime" || targetId == "multiTapFeedback" || targetId == "multiTapSpread" || targetId == "multiTapMix")
            return "fx";
        if (targetId == "volume" || targetId == "pan" || targetId == "bpmSync" || targetId == "retrigger"
            || targetId == "inputTrimDb" || targetId == "phaseInvert" || targetId == "stereoWidth"
            || targetId == "monoMaker" || targetId == "outputGainDb" || targetId == "outputLimiter"
            || targetId == "outputCeilingDb")
            return "out";
        if (targetId == "oscType" || targetId == "octave" || targetId == "detune" || targetId.startsWith ("wt"))
            return "source";

        for (const auto& kv : project.getDspGraph().quickEditControls)
            if (kv.second.contains (targetId))
                return kv.first;

        return {};
    }

    bool DspPage::targetAppliesToSection (const juce::String& targetId, const juce::String& sectionId) const
    {
        return sectionId == "mod" || sectionForTarget (targetId) == sectionId;
    }

    juce::String DspPage::blockImpactDescription (const DspBlock& block) const
    {
        auto get = [&] (const juce::String& key, float fallback)
        {
            auto it = block.values.find (key);
            return it == block.values.end() ? fallback : it->second;
        };
        auto pct = [] (float value)
        {
            return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * 100.0f)) + "%";
        };

        if (! block.enabled)
            return "Disabled. This block is stored but does not affect sound until enabled.";

        if (block.section == "source")
        {
            if (project.getEngineType() == "fx")
                return "Writes Drive " + pct (get ("drive", 0.0f)) + " and wet Mix "
                    + pct (get ("mix", 1.0f)) + " to the FX input chain.";

            if (block.type.containsIgnoreCase ("wavetable"))
                return "Writes Wavetable " + wtTableName (juce::roundToInt (get ("wtTable", 0.0f)))
                    + ": position " + pct (get ("wtPosition", 0.0f))
                    + ", morph " + pct (get ("wtMorph", 0.0f))
                    + ", warp " + pct (get ("wtWarp", 0.0f))
                    + ", fold " + pct (get ("wtFold", 0.0f))
                    + ", bend " + juce::String (get ("wtBend", 0.0f), 2)
                    + ", sync " + juce::String (juce::roundToInt (get ("wtSyncRatio", 1.0f))) + "x"
                    + ", unison " + juce::String (juce::roundToInt (get ("wtUnison", 1.0f)))
                    + ", level " + pct (get ("wtLevel", 1.0f)) + ".";

            if (project.getEngineType() == "sample")
                return "Writes sample playback: volume " + pct (get ("volume", 0.75f))
                    + ", pan " + pct (get ("pan", 0.5f))
                    + ", start " + pct (get ("sampleStart", 0.0f))
                    + ", length " + pct (get ("sampleLength", 1.0f))
                    + ", slice " + juce::String (juce::roundToInt (get ("sampleSlice", 0.0f)))
                    + "/" + juce::String (juce::roundToInt (get ("sampleSliceCount", 1.0f))) + ".";

            return "Writes source stack: volume " + pct (get ("volume", 0.75f))
                + ", detune " + pct (get ("detune", 0.5f))
                + ", osc blend " + pct (get ("oscBlend", 0.0f))
                + ", sub " + pct (get ("subBlend", 0.0f))
                + ", noise " + pct (get ("noiseBlend", 0.0f)) + ".";
        }

        if (block.section == "filter" && block.type.containsIgnoreCase ("eq"))
        {
            const int band = juce::jlimit (1, 8, juce::roundToInt (get ("eqBand", 1.0f)));
            const int type = juce::jlimit (0, 5, juce::roundToInt (get ("eqType", 0.0f)));
            const int mode = juce::jlimit (0, 4, juce::roundToInt (get ("eqMode", 0.0f)));
            return "Writes Surgical EQ band " + juce::String (band) + " "
                + eqTypeName (type) + " / " + eqModeName (mode) + " at "
                + juce::String (get ("eqFreq", 1000.0f), 0) + " Hz, "
                + juce::String (get ("eqGainDb", 0.0f), 1) + " dB, Q "
                + juce::String (get ("eqQ", 1.0f), 2)
                + (get ("eqDynMode", 0.0f) >= 0.5f
                    ? ", dynamic mode " + juce::String (juce::roundToInt (get ("eqDynMode", 0.0f)))
                        + " range " + juce::String (get ("eqDynRangeDb", 0.0f), 1) + " dB"
                    : "")
                + ", mix " + pct (get ("eqMix", 1.0f)) + ".";
        }

        if (block.section == "filter")
            return "Writes Cutoff " + pct (get ("cutoff", 0.5f))
                + ", Resonance " + pct (get ("resonance", 0.2f))
                + ", optional LFO amount " + pct (get ("lfoAmount", 0.0f)) + ".";

        if (block.section == "amp")
            return "Writes ADSR: attack " + pct (get ("attack", 0.01f))
                + ", decay " + pct (get ("decay", 0.2f))
                + ", sustain " + pct (get ("sustain", 0.8f))
                + ", release " + pct (get ("release", 0.4f)) + ".";

        if (block.section == "mod")
        {
            if (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi"))
                return "Generates MIDI Playground phrase at " + juce::String (get ("rate", 1.0f), 2)
                    + (get ("sync", 1.0f) >= 0.5f ? " beat-rate" : " Hz")
                    + ", " + juce::String (juce::roundToInt (get ("arpSteps", 8.0f))) + " steps"
                    + ", " + arpPatternName (juce::roundToInt (get ("arpPattern", 0.0f)))
                    + ", gate " + pct (get ("arpGate", 0.55f))
                    + ", range " + juce::String (juce::roundToInt (get ("arpOctaves", 2.0f))) + " oct"
                    + (get ("mpSampleControl", 0.0f) >= 0.5f ? ", sample slicing ON" : "")
                    + " for target " + (block.targetId.isNotEmpty() ? block.targetId : "none") + ".";
            if (block.type.containsIgnoreCase ("lfo"))
                return "Generates bipolar LFO motion at " + juce::String (get ("rate", 1.0f), 2)
                    + (get ("sync", 0.0f) >= 0.5f ? " beat-rate" : " Hz")
                    + " for target " + (block.targetId.isNotEmpty() ? block.targetId : "none") + ".";
            if (block.type.containsIgnoreCase ("random"))
                return "Generates random bipolar modulation for target "
                    + (block.targetId.isNotEmpty() ? block.targetId : "none") + ".";
            return "Macro/control source value " + pct (get ("value", 0.5f))
                + " for target " + (block.targetId.isNotEmpty() ? block.targetId : "none") + ".";
        }

        if (block.section == "fx")
        {
            if (block.type.containsIgnoreCase ("multiTap"))
                return "Writes MultiTap Delay: time " + juce::String (get ("multiTapTime", 0.375f), 2)
                    + " s, feedback " + pct (get ("multiTapFeedback", 0.35f))
                    + ", spread " + pct (get ("multiTapSpread", 0.45f))
                    + ", mix " + pct (get ("multiTapMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("delay"))
                return "Writes Delay Mix " + pct (get ("delayMix", 0.0f))
                    + ", Feedback " + pct (get ("delayFeedback", 0.35f))
                    + ", " + (get ("sync", 0.0f) >= 0.5f ? "tempo beats " + juce::String (get ("rate", 1.0f), 2)
                                                          : "time " + juce::String (get ("delayTime", 0.25f), 2));
            if (block.type.containsIgnoreCase ("dist") || block.type.containsIgnoreCase ("shape") || block.type.containsIgnoreCase ("crush"))
                return "Writes Drive " + pct (get ("drive", 0.55f))
                    + " and FX Mix " + pct (get ("mix", 1.0f)) + ".";
            if (block.type.containsIgnoreCase ("dynamics") || block.type.containsIgnoreCase ("compress"))
                return "Writes Dynamics: threshold " + juce::String (get ("dynThresholdDb", -18.0f), 1)
                    + " dB, ratio " + juce::String (get ("dynRatio", 2.0f), 1)
                    + ":1, mix " + pct (get ("dynMix", 0.5f)) + ".";
            if (block.type.containsIgnoreCase ("chorus"))
                return "Writes Chorus: depth " + pct (get ("chorusDepth", 0.35f))
                    + ", rate " + juce::String (get ("chorusRate", 0.35f), 2)
                    + " Hz, mix " + pct (get ("chorusMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("phaser"))
                return "Writes Phaser: depth " + pct (get ("phaserDepth", 0.45f))
                    + ", rate " + juce::String (get ("phaserRate", 0.25f), 2)
                    + " Hz, mix " + pct (get ("phaserMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("comb"))
                return "Writes Comb: " + juce::String (get ("combFreq", 220.0f), 0)
                    + " Hz, feedback " + juce::String (get ("combFeedback", 0.35f), 2)
                    + ", mix " + pct (get ("combMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("resonator"))
                return "Writes Resonator: " + juce::String (get ("resonatorFreq", 440.0f), 0)
                    + " Hz, Q " + juce::String (get ("resonatorQ", 4.0f), 1)
                    + ", mix " + pct (get ("resonatorMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("convolution"))
                return "Writes Convolution tone: taps " + juce::String (juce::roundToInt (get ("convolutionSize", 3.0f)))
                    + ", mix " + pct (get ("convolutionMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("spectral"))
                return "Writes Spectral tilt " + juce::String (get ("spectralTilt", 0.0f), 2)
                    + ", mix " + pct (get ("spectralMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("tape"))
                return "Writes Tape: drive " + pct (get ("tapeDrive", 0.32f))
                    + ", tone " + pct (get ("tapeTone", 0.58f))
                    + ", flutter " + pct (get ("tapeFlutter", 0.12f))
                    + ", mix " + pct (get ("tapeMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("vinyl") || block.type.containsIgnoreCase ("oldSchool"))
                return "Writes Vinyl: age " + pct (get ("vinylAge", 0.42f))
                    + ", dust " + pct (get ("vinylDust", 0.10f))
                    + ", warp " + pct (get ("vinylWarp", 0.16f))
                    + ", mix " + pct (get ("vinylMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("lofi"))
                return "Writes LoFi: " + juce::String (juce::roundToInt (get ("lofiBits", 10.0f)))
                    + " bits, rate crush " + pct (get ("lofiRate", 0.22f))
                    + ", mix " + pct (get ("lofiMix", 0.35f)) + ".";
            if (block.type.containsIgnoreCase ("vocal") || block.type.containsIgnoreCase ("formant"))
                return "Writes Vocal Formant: vowel " + pct (get ("vocalFormant", 0.40f))
                    + ", body " + pct (get ("vocalBody", 0.35f))
                    + ", mix " + pct (get ("vocalMix", 0.35f)) + ".";
            return "Writes Reverb wet level " + pct (get ("reverbMix", 0.22f))
                + " and FX Mix " + pct (get ("mix", 1.0f))
                + ". It affects sound when audio is entering the FX chain.";
        }

        return "Writes Output volume " + pct (get ("volume", 0.85f))
            + ", pan " + pct (get ("pan", 0.5f))
            + ", BPM sync " + (get ("bpmSync", 1.0f) >= 0.5f ? "on" : "off")
            + ", retrigger " + (get ("retrigger", 1.0f) >= 0.5f ? "on" : "off") + ".";
    }

    DspPage::Section* DspPage::currentSection()
    {
        switch (currentTab)
        {
            case 0: return &engineSection;
            case 1: return &toneSection;
            case 2: return &ampSection;
            case 3: return &modSection;
            case 4: return &fxSection;
            default: return &outputSection;
        }
    }

    void DspPage::ensureQuickEditControls()
    {
        auto& quick = project.getDspGraph().quickEditControls;
        auto setDefault = [&] (const juce::String& section, const juce::StringArray& ids)
        {
            if (quick.find (section) == quick.end() || quick[section].isEmpty())
                quick[section] = ids;
        };

        setDefault ("source", { "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend",
                                "wtEnabled", "wtPosition", "wtMorph", "wtWarp", "wtBend", "wtSyncRatio",
                                "wtSpectralTilt", "wtFramePosition", "wtFrameCount", "wtLevel", "detune", "volume" });
        setDefault ("filter", { "filterCutoff", "filterResonance", "eqEnabled", "eqMix", "eqBand1Freq", "eqBand1GainDb",
                                "eqBand1Q", "eqBand1Solo", "eqBand1Mode", "eqBand1DynMode", "eqBand1DynRangeDb" });
        setDefault ("amp",    { "attack", "decay", "sustain", "release", "volume" });
        setDefault ("mod",    { "lfoRate", "lfoAmount", "vibratoRate", "vibratoDepth" });
        setDefault ("fx",     { "drive", "mix", "delayTime", "delayFeedback", "delayMix", "reverbMix",
                                "tapeMix", "vinylMix", "lofiMix", "vocalMix", "multiTapMix" });
        setDefault ("out",    { "volume", "pan", "inputTrimDb", "stereoWidth", "monoMaker",
                                "outputGainDb", "outputLimiter", "outputCeilingDb", "bpmSync", "retrigger" });
    }

    void DspPage::refreshQuickEditSections()
    {
        ensureQuickEditControls();
        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
        {
            auto it = project.getDspGraph().quickEditControls.find (section->sectionId);
            if (it != project.getDspGraph().quickEditControls.end())
                section->setParameterIds (it->second);
            section->bind (project);
        }
    }

    void DspPage::showQuickParamMenu (Section& section, bool editMode)
    {
        auto* sectionPtr = &section;
        juce::PopupMenu menu;
        int itemId = 1;
        const auto current = sectionPtr->parameterIds;
        for (const auto& def : project.getParameters().getAll())
        {
            if (! def.visible)
                continue;
            if (def.section != sectionPtr->sectionId && sectionPtr->sectionId != "out")
                continue;
            const bool alreadyShown = current.contains (def.id);
            if (! editMode && alreadyShown)
                continue;
            const bool enabled = parameterIsEnabled (project, def);
            auto label = def.name + "  [" + def.category + "]  (" + def.id + ")";
            if (! enabled)
                label += " - disabled";
            menu.addItem (itemId++, label, enabled || editMode, editMode && alreadyShown);
        }

        if (itemId == 1)
        {
            menu.addItem (1, editMode ? "No parameters available" : "All parameters already shown", false);
        }

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this, sectionPtr, editMode] (int result)
            {
                if (result <= 0) return;
                int itemId = 1;
                for (const auto& def : project.getParameters().getAll())
                {
                    if (! def.visible)
                        continue;
                    if (def.section != sectionPtr->sectionId && sectionPtr->sectionId != "out")
                        continue;
                    const bool alreadyShown = sectionPtr->parameterIds.contains (def.id);
                    if (! editMode && alreadyShown)
                        continue;

                    if (itemId++ == result)
                    {
                        auto ids = sectionPtr->parameterIds;
                        if (editMode && alreadyShown)
                            ids.removeString (def.id);
                        else if (! alreadyShown)
                            ids.add (def.id);

                        project.getDspGraph().quickEditControls[sectionPtr->sectionId] = ids;
                        markGraphEdited();
                        refreshQuickEditSections();
                        project.notifyChanged();
                        return;
                    }
                }
            });
    }

    void DspPage::fillTargetBox()
    {
        targetBox.clear (juce::dontSendNotification);
        int itemId = 1;

        juce::String currentTarget;
        bool selectedBlockCanRoute = false;
        if (selectedGraphKind == 1 && selectedGraphIndex >= 0
            && selectedGraphIndex < (int) project.getDspGraph().blocks.size())
        {
            const auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
            currentTarget = block.targetId;
            selectedBlockCanRoute = block.section == "mod";
        }
        else if (selectedGraphKind == 2 && selectedGraphIndex >= 0
                 && selectedGraphIndex < (int) project.getDspGraph().macros.size())
        {
            currentTarget = project.getDspGraph().macros[(size_t) selectedGraphIndex].targetId;
        }
        else if (selectedGraphKind == 3 && selectedGraphIndex >= 0
                 && selectedGraphIndex < (int) project.getDspGraph().modulation.size())
        {
            currentTarget = project.getDspGraph().modulation[(size_t) selectedGraphIndex].targetId;
        }
        else if (selectedGraphKind == 4 && selectedGraphIndex >= 0
                 && selectedGraphIndex < (int) project.getDspGraph().automation.size())
        {
            currentTarget = project.getDspGraph().automation[(size_t) selectedGraphIndex].targetId;
        }

        const bool targetEditable = selectedBlockCanRoute
            || selectedGraphKind == 2 || selectedGraphKind == 3 || selectedGraphKind == 4;
        if (! targetEditable)
        {
            if (currentTarget.isNotEmpty())
            {
                if (const auto* def = project.getParameters().find (currentTarget))
                    targetBox.addItem ("Internal: " + def->name + " / " + def->category + " (" + def->id + ")", itemId++);
                else
                    targetBox.addItem ("Internal: " + currentTarget + " (" + currentTarget + ")", itemId++);
            }
            targetBox.setTextWhenNothingSelected ("Internal Target");
            targetBox.setTooltip ("Disabled: audio/source/filter/amp/FX/output blocks route to their own active DSP parameters. Use the visible controls, or add + Mod/+ Macro when you need a selectable target.");
            return;
        }

        juce::StringArray addedIds;
        auto addTargetItem = [&] (const ParameterDef& def, const juce::String& prefix)
        {
            if (addedIds.contains (def.id))
                return;

            const auto gateText = def.enabledBy.isNotEmpty() && ! parameterIsEnabled (project, def)
                ? " - needs " + def.enabledBy
                : juce::String();
            targetBox.addItem (prefix + def.name + " / " + def.category + gateText + " (" + def.id + ")", itemId++);
            addedIds.add (def.id);
        };

        auto graphWritesParameter = [this] (const juce::String& id)
        {
            if (id.isEmpty())
                return true;

            for (const auto& block : project.getDspGraph().blocks)
            {
                if (! block.enabled)
                    continue;
                if (block.targetId == id || block.values.count (id) != 0)
                    return true;
            }
            return false;
        };

        auto targetReady = [&] (const ParameterDef& def)
        {
            return def.enabledBy.isEmpty()
                || def.id == currentTarget
                || parameterIsEnabled (project, def)
                || graphWritesParameter (def.enabledBy);
        };

        auto sectionAllowed = [this] (const juce::String& parameterSection)
        {
            const auto sectionId = currentSectionId();
            return sectionId == "mod" || parameterSection == sectionId;
        };

        auto addParameterSection = [&] (const juce::String& sectionId, const juce::String& heading)
        {
            bool headingAdded = false;
            for (const auto& def : project.getParameters().getAll())
            {
                if (! def.visible || ! def.modulatable || def.section != sectionId)
                    continue;
                if (! sectionAllowed (sectionId) && def.id != currentTarget)
                    continue;
                if (! targetReady (def))
                    continue;

                if (! headingAdded)
                {
                    if (itemId > 1)
                        targetBox.addSeparator();
                    targetBox.addSectionHeading (heading);
                    headingAdded = true;
                }

                addTargetItem (def, {});
            }
        };

        addParameterSection ("source", "Source Targets");
        addParameterSection ("filter", "Filter / EQ Targets");
        addParameterSection ("amp", "Amp Targets");
        addParameterSection ("mod", "Mod / MIDI Targets");
        addParameterSection ("fx", "FX Targets");
        addParameterSection ("out", "Output Targets");

        if (currentTarget.isNotEmpty() && ! addedIds.contains (currentTarget))
        {
            if (const auto* def = project.getParameters().find (currentTarget))
            {
                if (itemId > 1)
                    targetBox.addSeparator();
                targetBox.addSectionHeading ("Current Target");
                addTargetItem (*def, {});
            }
        }

        bool blocksHeadingAdded = false;
        for (const auto& block : project.getDspGraph().blocks)
        {
            const bool blockAllowed = currentSectionId() == "mod" || block.section == currentSectionId();
            if (! blockAllowed && block.id != currentTarget)
                continue;

            if (! blocksHeadingAdded)
            {
                if (itemId > 1)
                    targetBox.addSeparator();
                targetBox.addSectionHeading ("DSP Blocks");
                blocksHeadingAdded = true;
            }
            targetBox.addItem (block.section.toUpperCase() + ": " + block.name + " (" + block.id + ")", itemId++);
        }
        targetBox.setTextWhenNothingSelected ("Sound Target");
        targetBox.setTooltip ("Select a sound target for the selected Macro, Mod Route, Automation lane, or Mod block. The menu is filtered to targets that can affect this section now.");
    }

    void DspPage::fillSourceBox()
    {
        sourceBox.clear (juce::dontSendNotification);
        int itemId = 1;

        juce::String currentSource;
        if (selectedGraphKind == 2 && selectedGraphIndex >= 0
            && selectedGraphIndex < (int) project.getDspGraph().macros.size())
            currentSource = project.getDspGraph().macros[(size_t) selectedGraphIndex].macroId;
        else if (selectedGraphKind == 3 && selectedGraphIndex >= 0
                 && selectedGraphIndex < (int) project.getDspGraph().modulation.size())
            currentSource = project.getDspGraph().modulation[(size_t) selectedGraphIndex].sourceId;

        const bool sourceEditable = selectedGraphKind == 2 || selectedGraphKind == 3;
        if (! sourceEditable)
        {
            sourceBox.setTextWhenNothingSelected ("Source Not Used");
            sourceBox.setTooltip ("Disabled: this selected item does not use an external source. Use + Macro or + Mod when you need source-to-target routing.");
            return;
        }

        juce::StringArray addedIds;
        auto addSourceItem = [&] (const juce::String& label, const juce::String& id)
        {
            if (id.isEmpty() || addedIds.contains (id))
                return;
            sourceBox.addItem (label + " (" + id + ")", itemId++);
            addedIds.add (id);
        };

        auto addParameterSection = [&] (const juce::String& sectionId, const juce::String& heading)
        {
            bool headingAdded = false;
            for (const auto& def : project.getParameters().getAll())
            {
                if (! def.visible || def.section != sectionId)
                    continue;

                if (! headingAdded)
                {
                    if (itemId > 1)
                        sourceBox.addSeparator();
                    sourceBox.addSectionHeading (heading);
                    headingAdded = true;
                }

                addSourceItem (def.name + " / " + def.category, def.id);
            }
        };

        addParameterSection ("mod", "Mod / MIDI Sources");

        bool blocksHeadingAdded = false;
        for (const auto& block : project.getDspGraph().blocks)
        {
            if (! blocksHeadingAdded)
            {
                if (itemId > 1)
                    sourceBox.addSeparator();
                sourceBox.addSectionHeading ("DSP Block Sources");
                blocksHeadingAdded = true;
            }
            addSourceItem (block.section.toUpperCase() + ": " + block.name, block.id);
        }

        if (currentSource.isNotEmpty() && ! addedIds.contains (currentSource))
        {
            if (itemId > 1)
                sourceBox.addSeparator();
            sourceBox.addSectionHeading ("Current Source");
            if (const auto* def = project.getParameters().find (currentSource))
                addSourceItem (def->name + " / " + def->category, def->id);
            else
                addSourceItem (currentSource, currentSource);
        }
        sourceBox.setTextWhenNothingSelected ("Mod Source");
        sourceBox.setTooltip ("Select the source that drives the selected Macro or Mod Route. Sources are limited to live Mod/MIDI parameters and DSP blocks that can output a control signal.");
    }

    void DspPage::configurePresetBoxes()
    {
        globalPresetBox.clear (juce::dontSendNotification);
        globalPresetBox.addSectionHeading ("Starter Patches");
        globalPresetBox.addItem ("Warm Analog Starter", 1);
        globalPresetBox.addItem ("Cinematic Bloom Motion", 2);
        globalPresetBox.addSeparator();
        globalPresetBox.addSectionHeading ("Performance / Motion");
        globalPresetBox.addItem ("Aggressive Beat FX Rack", 3);
        globalPresetBox.addItem ("Tempo Wobble Performer", 4);
        globalPresetBox.addItem ("Animation Pulse Gate", 5);
        globalPresetBox.addItem ("Destroyed Throw FX", 6);
        globalPresetBox.setTextWhenNothingSelected ("Global Preset");
        globalPresetBox.onChange = [this] { applyGlobalPreset (globalPresetBox.getSelectedId()); };

        sectionPresetBox.clear (juce::dontSendNotification);
        sectionPresetBox.addSectionHeading ("Current Section Basics");
        sectionPresetBox.addItem ("Init / Clean Foundation", 1);
        sectionPresetBox.addItem ("Tempo Wobble Motion", 2);
        sectionPresetBox.addItem ("Wide Animated Space", 3);
        sectionPresetBox.addItem ("Severe Brain-Breaker", 4);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("Source");
        sectionPresetBox.addItem ("Source: Wavetable Motion", 301);
        sectionPresetBox.addItem ("Source: Glass Pad Stack", 302);
        sectionPresetBox.addItem ("Source: Razor Bass Table", 303);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("Modulation");
        sectionPresetBox.addItem ("Mod: Audio Reactive Motion", 401);
        sectionPresetBox.addItem ("Mod: Transient Gate Pump", 402);
        sectionPresetBox.addItem ("ARP: Classic Up Notes", 403);
        sectionPresetBox.addItem ("ARP: Down Octaves", 404);
        sectionPresetBox.addItem ("ARP: Up/Down Motion", 405);
        sectionPresetBox.addItem ("ARP: Chord Pulse", 406);
        sectionPresetBox.addItem ("ARP: Syncopated Odd Steps", 407);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("Amp");
        sectionPresetBox.addItem ("Amp: Pluck Envelope", 501);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("Filter / EQ");
        sectionPresetBox.addItem ("Filter: Surgical Cleanup EQ", 201);
        sectionPresetBox.addItem ("Filter: Modern Smile EQ", 202);
        sectionPresetBox.addItem ("Filter: Resonance Hunter", 203);
        sectionPresetBox.addItem ("Filter: Mid/Side Polish EQ", 204);
        sectionPresetBox.addItem ("Filter: Dynamic Tamer EQ", 205);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("FX");
        sectionPresetBox.addItem ("FX: 1/4 Dub Delay", 101);
        sectionPresetBox.addItem ("FX: 1/8 Ping-Pong", 102);
        sectionPresetBox.addItem ("FX: Triplet Wobble", 103);
        sectionPresetBox.addItem ("FX: Chop Gate Performer", 104);
        sectionPresetBox.addItem ("FX: Dirty Space Throw", 105);
        sectionPresetBox.addSeparator();
        sectionPresetBox.addSectionHeading ("FX Lab");
        sectionPresetBox.addItem ("FX Lab: Vinyl Tape Wash", 106);
        sectionPresetBox.addItem ("FX Lab: Vocal Throw Designer", 107);
        sectionPresetBox.addItem ("FX Lab: LoFi Old Sampler", 108);
        sectionPresetBox.addItem ("FX Lab: MultiTap Space Engine", 109);
        sectionPresetBox.addItem ("FX Lab: Retro Destruction Chain", 110);
        sectionPresetBox.setTextWhenNothingSelected ("Section Preset");
        sectionPresetBox.onChange = [this] { applySectionPreset (sectionPresetBox.getSelectedId()); };
    }

    void DspPage::applyGlobalPreset (int presetId)
    {
        if (presetId <= 0) return;
        auto& graph = project.getDspGraph();
        graph.resetForEngine (project.getEngineType());
        graph.userConfigured = true;

        if (presetId == 2)
        {
            graph.modulation.push_back ({ "slow_pan_motion", "lfo_1", "pan", 0.18f, 0.02f, true });
            graph.macros.push_back ({ "macro_space", "macro_motion", "reverbMix", 0.0f, 1.0f, 0.15f, 0.85f, 1.2f, false });
            if (! graph.automation.empty())
            {
                graph.automation.front().targetId = "macro_motion";
                graph.automation.front().rate = 0.5f;
                graph.automation.front().points = { 0.0f, 0.2f, 0.85f, 1.0f, 0.35f, 0.0f };
            }
        }
        else if (presetId == 3)
        {
            graph.resetForEngine ("fx");
            graph.userConfigured = true;
            graph.blocks.push_back ({ "fx_lfo_1", "mod", "lfo", "FX Pump LFO", "mix", true, { { "rate", 2.0f }, { "amount", 0.35f }, { "sync", 1.0f } } });
            graph.modulation.push_back ({ "fx_pump_mix", "fx_lfo_1", "mix", 0.3f, 0.02f, true });
            graph.modulation.push_back ({ "fx_pump_delay", "fx_lfo_1", "delayMix", 0.22f, 0.02f, true });
        }
        else if (presetId == 4)
        {
            graph.blocks.push_back ({ "wobble_lfo_fast", "mod", "lfo", "Severe Wobble LFO", "filterCutoff", true,
                                      { { "rate", 2.0f }, { "amount", 0.55f }, { "sync", 1.0f } } });
            graph.blocks.push_back ({ "wobble_random", "mod", "random", "Jitter Random", "filterResonance", true,
                                      { { "value", 0.5f }, { "amount", 0.22f } } });
            graph.modulation.push_back ({ "wobble_cutoff", "wobble_lfo_fast", "filterCutoff", 0.42f, 0.02f, true });
            graph.modulation.push_back ({ "wobble_res", "wobble_random", "filterResonance", 0.18f, 0.02f, true });
            graph.modulation.push_back ({ "wobble_delay", "wobble_lfo_fast", "delayMix", 0.28f, 0.02f, true });
            graph.automation.push_back ({ "auto_wobble_gate", "volume", "loop", 1.0f, true,
                                          { 0.25f, 1.0f, 0.4f, 0.95f, 0.15f, 0.85f, 0.0f } });
        }
        else if (presetId == 5)
        {
            graph.blocks.push_back ({ "anim_lfo_pan", "mod", "lfo", "Animation Pan LFO", "pan", true,
                                      { { "rate", 0.5f }, { "amount", 0.6f }, { "sync", 1.0f } } });
            graph.modulation.push_back ({ "anim_pan_sweep", "anim_lfo_pan", "pan", 0.55f, 0.02f, true });
            graph.automation.push_back ({ "auto_animation_volume", "volume", "loop", 2.0f, true,
                                          { 0.05f, 1.0f, 0.15f, 0.9f, 0.3f, 1.0f, 0.05f } });
            graph.automation.push_back ({ "auto_animation_space", "reverbMix", "loop", 0.5f, true,
                                          { 0.1f, 0.35f, 0.85f, 0.5f, 0.2f, 0.7f } });
        }
        else if (presetId == 6)
        {
            graph.resetForEngine ("fx");
            graph.userConfigured = true;
            graph.blocks.push_back ({ "destroy_delay", "fx", "delay", "Beat-Mangled Delay", "delayMix", true,
                                      { { "delayMix", 0.72f }, { "delayFeedback", 0.82f }, { "rate", 0.5f }, { "sync", 1.0f }, { "reverbMix", 0.45f } } });
            graph.blocks.push_back ({ "destroy_lfo", "mod", "lfo", "Crusher Motion LFO", "delayMix", true,
                                      { { "rate", 4.0f }, { "amount", 0.65f }, { "sync", 1.0f } } });
            graph.modulation.push_back ({ "destroy_delay_motion", "destroy_lfo", "delayMix", 0.38f, 0.02f, true });
            graph.modulation.push_back ({ "destroy_filter_motion", "destroy_lfo", "filterCutoff", -0.35f, 0.02f, true });
            graph.automation.push_back ({ "destroy_output_gate", "mix", "loop", 2.0f, true,
                                          { 1.0f, 0.2f, 0.85f, 0.0f, 1.0f, 0.35f } });
        }

        normaliseDspGraphBanks (graph);
        selectedGraphKind = 0;
        selectedGraphIndex = -1;
        project.notifyChanged();
        refresh();
        globalPresetBox.setSelectedId (presetId, juce::dontSendNotification);
    }

    void DspPage::applySectionPreset (int presetId)
    {
        if (presetId <= 0) return;

        const auto sectionId = currentSectionId();
        if (presetId >= 100 && presetId < 200 && sectionId != "fx")
        {
            setTab (4);
            applySectionPreset (presetId);
            return;
        }
        if (presetId >= 200 && presetId < 300 && sectionId != "filter")
        {
            setTab (1);
            applySectionPreset (presetId);
            return;
        }
        if (presetId >= 300 && presetId < 400 && sectionId != "source")
        {
            setTab (0);
            applySectionPreset (presetId);
            return;
        }
        if (presetId >= 400 && presetId < 500 && sectionId != "mod")
        {
            setTab (3);
            applySectionPreset (presetId);
            return;
        }
        if (presetId >= 500 && presetId < 600 && sectionId != "amp")
        {
            setTab (2);
            applySectionPreset (presetId);
            return;
        }

        auto& graph = project.getDspGraph();
        graph.userConfigured = true;
        graph.blocks.erase (std::remove_if (graph.blocks.begin(), graph.blocks.end(),
            [&] (const DspBlock& block) { return block.section == sectionId; }), graph.blocks.end());

        auto addBlock = [&] (juce::String id, juce::String type, juce::String name,
                             juce::String target, std::map<juce::String, float> values) -> juce::String
        {
            const int targetBank = currentSectionBank();
            if (countBlocksInSectionBank (graph, sectionId, targetBank) >= kSourceMatrixBankSize)
                return {};

            DspBlock block;
            block.id = sectionId + "_" + id + "_" + juce::String ((int) graph.blocks.size() + 1);
            block.section = sectionId;
            block.type = type;
            block.name = name;
            block.targetId = target;
            block.values = std::move (values);
            block.values["bank"] = (float) targetBank;
            graph.blocks.push_back (block);
            return graph.blocks.back().id;
        };

        auto addLfo = [&] (juce::String id, juce::String name, juce::String target,
                           float rate, float amount, bool sync)
        {
            const int targetBank = currentTab == 3 ? currentSectionBank() : 0;
            if (countBlocksInSectionBank (graph, "mod", targetBank) >= kSourceMatrixBankSize)
                return juce::String();

            DspBlock block;
            block.id = "fxpreset_mod_" + id + "_" + juce::String ((int) graph.blocks.size() + 1);
            block.section = "mod";
            block.type = "lfo";
            block.name = name;
            block.targetId = target;
            block.values["rate"] = rate;
            block.values["amount"] = amount;
            block.values["sync"] = sync ? 1.0f : 0.0f;
            block.values["bank"] = (float) targetBank;
            graph.blocks.push_back (block);
            return block.id;
        };

        auto addRoute = [&] (juce::String id, juce::String source, juce::String target, float amount)
        {
            if (source.isEmpty())
                return;

            graph.modulation.push_back ({ "fx_" + id + "_" + juce::String ((int) graph.modulation.size() + 1),
                                          source, target, amount, 0.02f, true });
        };

        auto addAuto = [&] (juce::String id, juce::String target, float rate, std::vector<float> points)
        {
            graph.automation.push_back ({ "fx_" + id + "_" + juce::String ((int) graph.automation.size() + 1),
                                          target, "loop", rate, true, std::move (points) });
        };

        if (sectionId == "source" && presetId >= 300 && presetId < 400)
        {
            const auto addWavetable = [&] (juce::String id, juce::String name, int table,
                                           float position, float morph, float warp, float fold,
                                           int unison, float detune, float spread, float level,
                                           float bend = 0.0f, int syncRatio = 1,
                                           float spectralTilt = 0.0f, int phaseMode = 0)
            {
                addBlock (std::move (id), "wavetable", std::move (name), "wtPosition",
                          { { "wtEnabled", 1.0f },
                            { "wtTable", (float) table },
                            { "wtPosition", position },
                            { "wtMorph", morph },
                            { "wtWarp", warp },
                            { "wtFold", fold },
                            { "wtUnison", (float) unison },
                            { "wtDetune", detune },
                            { "wtSpread", spread },
                            { "wtLevel", level },
                            { "wtBend", bend },
                            { "wtSyncRatio", (float) syncRatio },
                            { "wtSpectralTilt", spectralTilt },
                            { "wtPhaseMode", (float) phaseMode } });
            };

            if (presetId == 301)
            {
                addWavetable ("wt_motion", "Motion Scan Table", 1, 0.15f, 0.35f, 0.18f, 0.08f, 3, 14.0f, 0.35f, 0.90f,
                              0.18f, 1, -0.22f, 2);
                const auto lfo = addLfo ("wt_scan", "WT Scan LFO", "wtPosition", 0.5f, 0.30f, true);
                addRoute ("wt_scan_route", lfo, "wtPosition", 0.30f);
                addRoute ("wt_frame_scan_route", lfo, "wtFramePosition", 0.55f);
            }
            else if (presetId == 302)
            {
                addWavetable ("wt_glass", "Glass Pad Table", 1, 0.62f, 0.48f, 0.10f, 0.0f, 5, 18.0f, 0.72f, 0.78f,
                              -0.10f, 1, 0.28f, 1);
                addBlock ("analog_body", "oscillator", "Analog Body", "oscBlend",
                          { { "oscType", 0.0f }, { "osc2Type", 3.0f }, { "oscBlend", 0.24f },
                            { "osc2Detune", 9.0f }, { "subBlend", 0.05f }, { "noiseBlend", 0.01f },
                            { "volume", 0.78f } });
            }
            else
            {
                addWavetable ("wt_razor", "Razor Bass Table", 6, 0.42f, 0.62f, 0.55f, 0.38f, 2, 7.0f, 0.18f, 1.0f,
                              0.44f, 2, -0.55f, 0);
                addBlock ("sub_anchor", "oscillator", "Sub Anchor", "subBlend",
                          { { "oscType", 2.0f }, { "osc2Type", 1.0f }, { "oscBlend", 0.12f },
                            { "subBlend", 0.35f }, { "noiseBlend", 0.02f }, { "volume", 0.82f } });
            }

            selectedGraphKind = 1;
            selectedGraphIndex = -1;
            for (int i = (int) graph.blocks.size(); --i >= 0;)
                if (graph.blocks[(size_t) i].section == "source")
                {
                    selectedGraphIndex = i;
                    break;
                }
            markGraphEdited();
            normaliseDspGraphSectionBanks (graph, "source");
            normaliseDspGraphSectionBanks (graph, "mod");
            project.notifyChanged();
            rebuildGraphEditorItems();
            refreshBuilderPanel();
            syncGraphEditor();
            return;
        }

        if (sectionId == "filter" && presetId >= 200 && presetId < 300)
        {
            const auto addEqBand = [&] (juce::String id, juce::String name, int band, int type,
                                        float freq, float gainDb, float q, float mix = 1.0f, int mode = 0,
                                        int dynMode = 0, float dynThresholdDb = -24.0f, float dynRangeDb = 0.0f,
                                        float dynAttackMs = 10.0f, float dynReleaseMs = 120.0f)
            {
                addBlock (std::move (id), "surgicalEq", std::move (name), "eqBand" + juce::String (band) + "Freq",
                          { { "eqBand", (float) band },
                            { "eqOn", 1.0f },
                            { "eqType", (float) type },
                            { "eqMode", (float) mode },
                            { "eqFreq", freq },
                            { "eqGainDb", gainDb },
                            { "eqQ", q },
                            { "eqMix", mix },
                            { "eqSolo", 0.0f },
                            { "eqDynMode", (float) dynMode },
                            { "eqDynThresholdDb", dynThresholdDb },
                            { "eqDynRangeDb", dynRangeDb },
                            { "eqDynAttackMs", dynAttackMs },
                            { "eqDynReleaseMs", dynReleaseMs } });
            };

            if (presetId == 201)
            {
                addEqBand ("cleanup_rumble", "Rumble High-Pass", 1, 3, 32.0f, 0.0f, 0.75f);
                addEqBand ("cleanup_mud", "Mud Surgical Cut", 2, 0, 240.0f, -4.5f, 4.5f);
                addEqBand ("cleanup_box", "Boxiness Cut", 3, 0, 520.0f, -2.8f, 2.2f);
                addEqBand ("cleanup_air", "Air Lift", 4, 2, 9500.0f, 1.8f, 0.75f);
            }
            else if (presetId == 202)
            {
                addEqBand ("smile_low", "Low Weight Shelf", 1, 1, 85.0f, 2.4f, 0.80f);
                addEqBand ("smile_lowmid", "Low-Mid Dip", 2, 0, 360.0f, -1.8f, 1.3f);
                addEqBand ("smile_presence", "Presence Bell", 3, 0, 3200.0f, 2.2f, 1.0f);
                addEqBand ("smile_air", "Expensive Air Shelf", 4, 2, 12500.0f, 3.2f, 0.70f);
            }
            else if (presetId == 203)
            {
                addEqBand ("hunt_1", "Narrow Ring Finder 1", 1, 0, 180.0f, -6.0f, 10.0f);
                addEqBand ("hunt_2", "Narrow Ring Finder 2", 2, 0, 430.0f, -7.0f, 12.0f);
                addEqBand ("hunt_3", "Narrow Ring Finder 3", 3, 0, 1150.0f, -6.5f, 14.0f);
                addEqBand ("hunt_4", "Harshness Notch", 4, 5, 3100.0f, 0.0f, 12.0f);
                addEqBand ("hunt_5", "Fizz Low-Pass", 5, 4, 16500.0f, 0.0f, 0.7f);
            }
            else if (presetId == 205)
            {
                addEqBand ("dyn_sub_tamer", "Dynamic Sub Tamer", 1, 1, 72.0f, 1.2f, 0.75f, 1.0f, 0, 1, -20.0f, 4.5f, 8.0f, 180.0f);
                addEqBand ("dyn_mud_duck", "Dynamic Mud Duck", 2, 0, 260.0f, 0.0f, 3.5f, 1.0f, 3, 1, -26.0f, 6.0f, 14.0f, 220.0f);
                addEqBand ("dyn_presence_lift", "Dynamic Presence Lift", 3, 0, 2700.0f, 1.0f, 1.4f, 1.0f, 0, 2, -34.0f, 3.5f, 18.0f, 160.0f);
                addEqBand ("dyn_harsh_tamer", "Dynamic Harsh Tamer", 4, 0, 5200.0f, 0.0f, 5.5f, 1.0f, 0, 1, -30.0f, 7.5f, 4.0f, 140.0f);
                addEqBand ("dyn_side_air", "Dynamic Side Air", 5, 2, 10800.0f, 1.8f, 0.65f, 1.0f, 4, 2, -38.0f, 2.8f, 24.0f, 260.0f);
            }
            else
            {
                addEqBand ("ms_mid_weight", "Mid Weight Control", 1, 1, 110.0f, 1.4f, 0.8f, 1.0f, 3);
                addEqBand ("ms_mid_cleanup", "Mid Mud Carve", 2, 0, 340.0f, -2.8f, 2.4f, 1.0f, 3);
                addEqBand ("ms_side_width", "Side Air Width", 3, 2, 7800.0f, 3.4f, 0.7f, 1.0f, 4);
                addEqBand ("ms_side_ring", "Side Harshness Notch", 4, 5, 3900.0f, 0.0f, 10.0f, 1.0f, 4);
                addEqBand ("ms_left_balance", "Left Tone Balance", 5, 0, 1200.0f, -0.8f, 1.1f, 1.0f, 1);
                addEqBand ("ms_right_balance", "Right Tone Balance", 6, 0, 1200.0f, 0.8f, 1.1f, 1.0f, 2);
            }

            selectedGraphKind = 1;
            selectedGraphIndex = -1;
            for (int i = (int) graph.blocks.size(); --i >= 0;)
                if (graph.blocks[(size_t) i].section == "filter")
                {
                    selectedGraphIndex = i;
                    break;
                }
            normaliseDspGraphSectionBanks (graph, "filter");
            project.notifyChanged();
            refresh();
            sectionPresetBox.setSelectedId (presetId, juce::dontSendNotification);
            return;
        }

        if (sectionId == "mod" && presetId >= 400 && presetId < 500)
        {
            graph.modulation.clear();

            auto addReactive = [&] (juce::String id, juce::String type, juce::String name,
                                    juce::String target, float amount, float thresholdDb,
                                    float smoothingMs, float band = 0.0f, float bipolar = 0.0f)
            {
                return addBlock (std::move (id), std::move (type), std::move (name), std::move (target),
                                 { { "amount", amount },
                                   { "thresholdDb", thresholdDb },
                                   { "smoothingMs", smoothingMs },
                                   { "band", band },
                                   { "bipolar", bipolar },
                                   { "sensitivity", 1.0f } });
            };

            auto addArp = [&] (juce::String id, juce::String name, int pattern, int steps,
                               float rate, float gate, float amount, float octaves,
                               std::initializer_list<float> notes)
            {
                std::map<juce::String, float> values {
                    { "amount", amount }, { "rate", rate }, { "sync", 1.0f },
                    { "arpGate", gate }, { "arpSteps", (float) steps },
                    { "arpPattern", (float) pattern }, { "arpOctaves", octaves },
                    { "arpSwing", 0.0f }, { "mpScaleRoot", 0.0f }, { "mpScaleType", 1.0f },
                    { "mpChordMode", pattern == 3 ? 1.0f : 0.0f }, { "mpChordSize", pattern == 3 ? 3.0f : 1.0f },
                    { "mpChordSpread", pattern == 3 ? 0.65f : 0.0f }, { "mpProbability", 1.0f },
                    { "mpHumanize", 0.0f }, { "mpSeed", 12001.0f + (float) pattern }
                };

                int step = 0;
                float lastNote = 0.0f;
                for (auto note : notes)
                {
                    if (step >= 16)
                        break;
                    lastNote = note;
                    values["arpNote" + juce::String (step++)] = note;
                }
                while (step < 16)
                    values["arpNote" + juce::String (step++)] = lastNote;
                for (int i = 0; i < 16; ++i)
                {
                    values["mpVelocity" + juce::String (i)] = 1.0f;
                    values["mpGate" + juce::String (i)] = gate;
                    values["mpStep" + juce::String (i) + "On"] = 1.0f;
                }

                return addBlock (std::move (id), "arp", std::move (name), "filterCutoff", std::move (values));
            };

            if (presetId == 401)
            {
                const auto env = addReactive ("env_follow", "envelopeFollower", "Output Envelope Follow",
                                              "filterCutoff", 0.28f, -38.0f, 90.0f);
                const auto high = addReactive ("high_energy", "bandEnergy", "High Energy Spark",
                                               "eqBand4GainDb", 0.16f, -44.0f, 65.0f, 2.0f);
                const auto centroid = addReactive ("centroid_tilt", "spectralCentroid", "Centroid Tilt",
                                                   "wtPosition", 0.12f, -54.0f, 120.0f);
                addRoute ("react_env_cutoff", env, "filterCutoff", 0.26f);
                addRoute ("react_high_air", high, "eqBand4GainDb", 0.12f);
                addRoute ("react_centroid_wt", centroid, "wtPosition", 0.10f);
            }
            else if (presetId == 403)
            {
                const auto arp = addArp ("arp_classic_up", "Classic Up ARP Sequencer", 0, 8, 1.0f, 0.72f, 0.36f, 2.0f,
                                         { 0.0f, 4.0f, 7.0f, 12.0f, 16.0f, 19.0f, 24.0f, 28.0f });
                addRoute ("arp_cutoff", arp, "filterCutoff", 0.30f);
                addRoute ("arp_wt_position", arp, "wtPosition", 0.18f);
            }
            else if (presetId == 404)
            {
                const auto arp = addArp ("arp_down_octaves", "Down Octaves ARP Sequencer", 1, 8, 0.5f, 0.64f, 0.42f, 3.0f,
                                         { 0.0f, 3.0f, 7.0f, 12.0f, 15.0f, 19.0f, 24.0f, 31.0f });
                addRoute ("arp_filter_descent", arp, "filterCutoff", -0.34f);
                addRoute ("arp_res_pulse", arp, "filterResonance", 0.12f);
            }
            else if (presetId == 405)
            {
                const auto arp = addArp ("arp_updown_motion", "Up/Down Motion ARP Sequencer", 2, 12, 1.5f, 0.58f, 0.40f, 3.0f,
                                         { 0.0f, 5.0f, 7.0f, 12.0f, 17.0f, 19.0f, 24.0f, 19.0f, 17.0f, 12.0f, 7.0f, 5.0f });
                addRoute ("arp_updown_cutoff", arp, "filterCutoff", 0.28f);
                addRoute ("arp_delay_motion", arp, "delayMix", 0.16f);
            }
            else if (presetId == 406)
            {
                const auto arp = addArp ("arp_chord_pulse", "Chord Pulse ARP Sequencer", 3, 16, 2.0f, 0.42f, 0.55f, 2.0f,
                                         { 0.0f, 0.0f, 7.0f, 7.0f, 12.0f, 12.0f, 19.0f, 19.0f,
                                           7.0f, 7.0f, 12.0f, 12.0f, 24.0f, 24.0f, 19.0f, 19.0f });
                addRoute ("arp_chord_amp", arp, "volume", 0.22f);
                addRoute ("arp_chord_space", arp, "reverbMix", 0.18f);
            }
            else if (presetId == 407)
            {
                const auto arp = addArp ("arp_syncopated_odd", "Syncopated Odd-Step ARP Sequencer", 4, 16, 4.0f, 0.36f, 0.48f, 4.0f,
                                         { 0.0f, 2.0f, 7.0f, 10.0f, 14.0f, 19.0f, 21.0f, 26.0f,
                                           31.0f, 26.0f, 21.0f, 19.0f, 14.0f, 10.0f, 7.0f, 2.0f });
                addRoute ("arp_odd_cutoff", arp, "filterCutoff", 0.38f);
                addRoute ("arp_odd_drive", arp, "drive", 0.20f);
            }
            else
            {
                const auto transient = addReactive ("transient_pump", "transientDetector", "Transient Pump",
                                                    "mix", 0.45f, -48.0f, 18.0f, 0.0f, 1.0f);
                const auto gate = addReactive ("gate_trigger", "gateTrigger", "Gate Trigger",
                                               "delayMix", 0.32f, -42.0f, 10.0f);
                const auto low = addReactive ("low_energy", "bandEnergy", "Low Energy Weight",
                                              "filterResonance", 0.18f, -46.0f, 110.0f, 0.0f);
                addRoute ("transient_mix", transient, "mix", -0.34f);
                addRoute ("gate_delay", gate, "delayMix", 0.22f);
                addRoute ("low_resonance", low, "filterResonance", 0.16f);
            }

            selectedGraphKind = 1;
            selectedGraphIndex = -1;
            for (int i = (int) graph.blocks.size(); --i >= 0;)
                if (graph.blocks[(size_t) i].section == "mod")
                {
                    selectedGraphIndex = i;
                    break;
                }
            normaliseDspGraphSectionBanks (graph, "mod");
            project.notifyChanged();
            refresh();
            sectionPresetBox.setSelectedId (presetId, juce::dontSendNotification);
            return;
        }

        if (sectionId == "fx" && presetId >= 100)
        {
            graph.blocks.erase (std::remove_if (graph.blocks.begin(), graph.blocks.end(),
                [] (const DspBlock& block)
                {
                    if (block.section != "mod")
                        return false;

                    return block.id.startsWith ("fxpreset_mod_")
                        || block.id.startsWith ("mod_dub_")
                        || block.id.startsWith ("mod_ping_")
                        || block.id.startsWith ("mod_triplet_")
                        || block.id.startsWith ("mod_chop_")
                        || block.id.startsWith ("mod_dirty_");
                }), graph.blocks.end());
            graph.modulation.erase (std::remove_if (graph.modulation.begin(), graph.modulation.end(),
                [this] (const ModRoute& route) { return targetAppliesToSection (route.targetId, "fx"); }), graph.modulation.end());
            graph.automation.erase (std::remove_if (graph.automation.begin(), graph.automation.end(),
                [this] (const AutomationLane& lane) { return targetAppliesToSection (lane.targetId, "fx"); }), graph.automation.end());

            if (presetId == 101)
            {
                addBlock ("dub_delay", "delay", "1/4 Dub Delay", "delayMix",
                          { { "delayMix", 0.42f }, { "delayFeedback", 0.62f }, { "delayTime", 0.25f },
                            { "rate", 1.0f }, { "sync", 1.0f }, { "mix", 1.0f } });
                addBlock ("dub_space", "reverb", "Warm Dub Space", "reverbMix",
                          { { "reverbMix", 0.30f }, { "mix", 1.0f } });
                const auto lfo = addLfo ("dub_bloom", "Dub Feedback Bloom", "delayFeedback", 0.5f, 0.18f, true);
                addRoute ("dub_feedback", lfo, "delayFeedback", 0.16f);
            }
            else if (presetId == 102)
            {
                addBlock ("ping_pong", "delay", "1/8 Ping-Pong Delay", "delayMix",
                          { { "delayMix", 0.36f }, { "delayFeedback", 0.48f }, { "delayTime", 0.125f },
                            { "rate", 0.5f }, { "sync", 1.0f }, { "mix", 1.0f } });
                const auto lfo = addLfo ("ping_pan", "Ping-Pong Width LFO", "pan", 1.0f, 0.45f, true);
                addRoute ("ping_pan", lfo, "pan", 0.35f);
            }
            else if (presetId == 103)
            {
                addBlock ("triplet_delay", "delay", "Triplet Wobble Delay", "delayMix",
                          { { "delayMix", 0.52f }, { "delayFeedback", 0.58f }, { "delayTime", 0.1875f },
                            { "rate", 0.75f }, { "sync", 1.0f }, { "mix", 1.0f } });
                addBlock ("triplet_filter", "distortion", "Edge Saturation", "drive",
                          { { "drive", 0.18f }, { "mix", 0.72f } });
                const auto lfo = addLfo ("triplet_wobble", "Triplet Filter Wobble", "filterCutoff", 1.5f, 0.42f, true);
                addRoute ("triplet_cutoff", lfo, "filterCutoff", -0.32f);
                addRoute ("triplet_delay", lfo, "delayMix", 0.22f);
            }
            else if (presetId == 104)
            {
                addBlock ("chop_delay", "delay", "Chop Echo", "delayMix",
                          { { "delayMix", 0.22f }, { "delayFeedback", 0.38f }, { "delayTime", 0.0625f },
                            { "rate", 0.25f }, { "sync", 1.0f }, { "mix", 1.0f } });
                const auto lfo = addLfo ("chop_gate", "Tempo Chop Gate", "mix", 4.0f, 0.9f, true);
                addRoute ("chop_mix", lfo, "mix", 0.55f);
                addAuto ("chop_pattern", "mix", 4.0f, { 1.0f, 0.0f, 0.75f, 0.0f, 1.0f, 0.2f, 0.0f, 0.85f });
            }
            else if (presetId == 105)
            {
                addBlock ("dirty_throw_delay", "delay", "Dirty Space Throw Delay", "delayMix",
                          { { "delayMix", 0.64f }, { "delayFeedback", 0.74f }, { "delayTime", 0.375f },
                            { "rate", 1.5f }, { "sync", 1.0f }, { "mix", 1.0f } });
                addBlock ("dirty_throw_drive", "distortion", "Throw Saturation", "drive",
                          { { "drive", 0.42f }, { "mix", 0.86f } });
                addBlock ("dirty_throw_space", "reverb", "Long Dark Space", "reverbMix",
                          { { "reverbMix", 0.58f }, { "mix", 1.0f } });
                const auto lfo = addLfo ("dirty_throw_motion", "Throw Motion", "delayMix", 0.5f, 0.3f, true);
                addRoute ("dirty_throw_motion", lfo, "delayMix", 0.24f);
            }
            else if (presetId == 106)
            {
                addBlock ("vinyl_wash_tape", "tape", "Warm Tape Saturator", "tapeMix",
                          { { "tapeDrive", 0.38f }, { "tapeTone", 0.52f }, { "tapeFlutter", 0.16f }, { "tapeMix", 0.42f } });
                addBlock ("vinyl_wash_dust", "vinyl", "Aged Vinyl Texture", "vinylMix",
                          { { "vinylAge", 0.62f }, { "vinylDust", 0.16f }, { "vinylWarp", 0.22f }, { "vinylMix", 0.34f } });
                addBlock ("vinyl_wash_space", "reverb", "Soft Room Tail", "reverbMix",
                          { { "reverbMix", 0.22f }, { "mix", 1.0f } });
            }
            else if (presetId == 107)
            {
                addBlock ("vocal_throw_formant", "vocalFormant", "Vocal Formant Throw", "vocalMix",
                          { { "vocalFormant", 0.38f }, { "vocalBody", 0.72f }, { "vocalMix", 0.50f } });
                addBlock ("vocal_throw_multitap", "multiTapDelay", "Vowel MultiTap Delay", "multiTapMix",
                          { { "multiTapTime", 0.24f }, { "multiTapFeedback", 0.42f }, { "multiTapSpread", 0.74f }, { "multiTapMix", 0.32f } });
                const auto lfo = addLfo ("vocal_sweep", "Formant Sweep LFO", "vocalFormant", 0.5f, 0.30f, true);
                addRoute ("vocal_sweep", lfo, "vocalFormant", 0.28f);
            }
            else if (presetId == 108)
            {
                addBlock ("lofi_sampler_crush", "lofi", "Old Sampler Crunch", "lofiMix",
                          { { "lofiBits", 9.0f }, { "lofiRate", 0.36f }, { "lofiMix", 0.42f } });
                addBlock ("lofi_sampler_tape", "tape", "Input Tape Push", "tapeMix",
                          { { "tapeDrive", 0.30f }, { "tapeTone", 0.44f }, { "tapeFlutter", 0.10f }, { "tapeMix", 0.30f } });
            }
            else if (presetId == 109)
            {
                addBlock ("space_multitap", "multiTapDelay", "Wide MultiTap Space", "multiTapMix",
                          { { "multiTapTime", 0.375f }, { "multiTapFeedback", 0.55f }, { "multiTapSpread", 0.88f }, { "multiTapMix", 0.44f } });
                addBlock ("space_chorus", "chorus", "Air Chorus", "chorusMix",
                          { { "chorusRate", 0.18f }, { "chorusDepth", 0.42f }, { "chorusFeedback", 0.08f }, { "chorusMix", 0.22f } });
                addBlock ("space_spectral", "spectral", "Spectral Air Lift", "spectralMix",
                          { { "spectralTilt", 0.32f }, { "spectralMix", 0.18f } });
            }
            else
            {
                addBlock ("retro_destroy_tape", "tape", "Tape Overload", "tapeMix",
                          { { "tapeDrive", 0.78f }, { "tapeTone", 0.38f }, { "tapeFlutter", 0.28f }, { "tapeMix", 0.64f } });
                addBlock ("retro_destroy_lofi", "lofi", "Digital Breakdown", "lofiMix",
                          { { "lofiBits", 6.0f }, { "lofiRate", 0.58f }, { "lofiMix", 0.46f } });
                addBlock ("retro_destroy_vinyl", "vinyl", "Damaged Vinyl Bed", "vinylMix",
                          { { "vinylAge", 0.70f }, { "vinylDust", 0.20f }, { "vinylWarp", 0.32f }, { "vinylMix", 0.18f } });
                addBlock ("retro_destroy_multitap", "multiTapDelay", "Broken MultiTap", "multiTapMix",
                          { { "multiTapTime", 0.17f }, { "multiTapFeedback", 0.66f }, { "multiTapSpread", 0.93f }, { "multiTapMix", 0.38f } });
                const auto lfo = addLfo ("retro_destroy_wobble", "Retro Destroy Wobble", "multiTapMix", 2.0f, 0.45f, true);
                addRoute ("retro_destroy_mix", lfo, "multiTapMix", 0.22f);
                addRoute ("retro_destroy_vinyl", lfo, "vinylMix", 0.12f);
            }

            selectedGraphKind = 1;
            selectedGraphIndex = -1;
            for (int i = (int) graph.blocks.size(); --i >= 0;)
                if (graph.blocks[(size_t) i].section == "fx")
                {
                    selectedGraphIndex = i;
                    break;
                }
            normaliseDspGraphSectionBanks (graph, "fx");
            normaliseDspGraphSectionBanks (graph, "mod");
            project.notifyChanged();
            refresh();
            sectionPresetBox.setSelectedId (presetId, juce::dontSendNotification);
            return;
        }

        if (sectionId == "amp" && presetId >= 500 && presetId < 600)
        {
            addBlock ("pluck_env", "adsr", "Pluck Envelope", "volume",
                      { { "attack", 0.002f }, { "decay", 0.11f }, { "sustain", 0.0f },
                        { "release", 0.16f }, { "velocity", 0.65f }, { "curve", 0.72f } });

            selectedGraphKind = 1;
            selectedGraphIndex = -1;
            for (int i = (int) graph.blocks.size(); --i >= 0;)
                if (graph.blocks[(size_t) i].section == "amp")
                {
                    selectedGraphIndex = i;
                    break;
                }
            normaliseDspGraphSectionBanks (graph, "amp");
            project.notifyChanged();
            refresh();
            sectionPresetBox.setSelectedId (presetId, juce::dontSendNotification);
            return;
        }

        if (sectionId == "source")
            addBlock ("layer", "oscillator", presetId == 4 ? "Severe Detuned Source" : presetId == 3 ? "Wide Detuned Source" : "Core Source", "volume",
                      { { "volume", presetId == 4 ? 1.0f : presetId == 2 ? 0.65f : 0.5f },
                        { "detune", presetId == 4 ? 0.95f : presetId == 3 ? 0.75f : 0.5f },
                        { "pan", presetId == 3 ? 0.2f : 0.5f },
                        { "amount", presetId == 4 ? 0.75f : presetId == 3 ? 0.35f : 0.12f } });
        else if (sectionId == "filter")
            addBlock ("morph", "stateVariable", presetId == 4 ? "Screaming Morph Filter" : presetId == 2 ? "Moving Morph Filter" : "Filter Stage", "filterCutoff",
                      { { "cutoff", presetId == 1 ? 0.55f : presetId == 4 ? 0.18f : 0.28f },
                        { "resonance", presetId == 4 ? 0.82f : presetId == 3 ? 0.45f : 0.22f } });
        else if (sectionId == "amp")
            addBlock ("env", "envelope", presetId == 4 ? "Hard Gate Envelope" : presetId == 2 ? "Slow Bloom Envelope" : "Amp Envelope", "volume",
                      { { "attack", presetId == 4 ? 0.005f : presetId == 2 ? 0.35f : 0.05f },
                        { "release", presetId == 4 ? 0.04f : presetId == 2 ? 0.55f : 0.2f },
                        { "sustain", presetId == 4 ? 0.35f : 0.8f } });
        else if (sectionId == "mod")
            addBlock ("lfo", presetId == 4 ? "random" : "lfo", presetId == 4 ? "Brain-Break Random" : presetId == 3 ? "Wide LFO" : "Motion LFO", "filterCutoff",
                      { { "rate", presetId == 4 ? 8.0f : presetId == 2 ? 0.5f : 1.5f },
                        { "amount", presetId == 4 ? 0.65f : presetId == 3 ? 0.35f : 0.18f },
                        { "sync", 1.0f } });
        else if (sectionId == "fx")
            addBlock ("space", "effect", presetId == 4 ? "Severe Beat FX" : presetId == 3 ? "Wide Space FX" : "Space FX", "reverbMix",
                      { { "reverbMix", presetId == 4 ? 0.88f : presetId == 1 ? 0.22f : 0.65f },
                        { "delayMix", presetId == 4 ? 0.72f : presetId == 3 ? 0.45f : 0.18f },
                        { "delayFeedback", presetId == 4 ? 0.84f : 0.35f },
                        { "rate", presetId == 4 ? 0.25f : 1.0f },
                        { "sync", 1.0f } });
        else
            addBlock ("out", "output", presetId == 4 ? "Performance Lock Output" : presetId == 3 ? "Wide Output" : "Output Safety", "volume",
                      { { "volume", presetId == 4 ? 1.0f : 0.75f },
                        { "pan", presetId == 3 ? 0.15f : 0.5f },
                        { "bpmSync", 1.0f },
                        { "retrigger", presetId == 4 ? 0.0f : 1.0f } });

        normaliseDspGraphSectionBanks (graph, sectionId);
        selectedGraphKind = 1;
        selectedGraphIndex = (int) graph.blocks.size() - 1;
        project.notifyChanged();
        refresh();
        sectionPresetBox.setSelectedId (presetId, juce::dontSendNotification);
    }

    void DspPage::showBlockEditorPopout (int blockIndex)
    {
        if (blockIndex < 0 || blockIndex >= (int) project.getDspGraph().blocks.size())
            return;

        struct FormulaValueRow final : public juce::Component
        {
            FormulaValueRow (DspPage& ownerIn, int blockIndexIn, juce::String keyIn, float valueIn, bool editsLiveParameterIn)
                : owner (ownerIn), blockIndex (blockIndexIn), key (std::move (keyIn)), editsLiveParameter (editsLiveParameterIn)
            {
                if (editsLiveParameter)
                {
                    if (const auto* def = owner.project.getParameters().find (key))
                    {
                        label.setText (def->name + " (" + key + ")", juce::dontSendNotification);
                        valueIn = owner.project.getLiveValues().getValue (key, def->defaultValue);
                    }
                    else
                        label.setText (key, juce::dontSendNotification);
                }
                else
                    label.setText (key, juce::dontSendNotification);

                label.setFont (juce::Font (11.5f, juce::Font::bold));
                label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
                addAndMakeVisible (label);

                valueLabel.setFont (juce::Font (11.0f));
                valueLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                valueLabel.setJustificationType (juce::Justification::centredRight);
                addAndMakeVisible (valueLabel);

                double min = 0.0;
                double max = 1.0;
                double step = 0.001;
                if (editsLiveParameter)
                {
                    if (const auto* def = owner.project.getParameters().find (key))
                    {
                        min = def->min;
                        max = def->max;
                        step = def->step > 0.0f ? def->step : 0.001;
                    }
                }

                const auto lower = key.toLowerCase();
                if (! editsLiveParameter && lower.contains ("freq") && ! lower.contains ("feedback"))
                {
                    min = 20.0; max = 20000.0; step = 1.0;
                }
                else if (! editsLiveParameter && (lower.contains ("db") || lower.contains ("gain")))
                {
                    min = -48.0; max = 24.0; step = 0.1;
                }
                else if (! editsLiveParameter && (lower.contains ("steps") || lower == "steps"))
                {
                    min = 1.0; max = 16.0; step = 1.0;
                }
                else if (! editsLiveParameter && (lower.contains ("pattern") || lower.contains ("type") || lower.contains ("mode") || lower.contains ("band")))
                {
                    min = 0.0; max = 8.0; step = 1.0;
                }
                else if (! editsLiveParameter && lower.contains ("note"))
                {
                    min = -24.0; max = 24.0; step = 1.0;
                }
                else if (! editsLiveParameter && lower.contains ("rate") && ! lower.contains ("lofirate"))
                {
                    min = 0.01; max = 20.0; step = 0.001;
                }
                else if (! editsLiveParameter && lower.contains ("time"))
                {
                    min = 0.0; max = 8.0; step = 0.001;
                }
                else if (! editsLiveParameter && lower.contains ("detune"))
                {
                    min = 0.0; max = key.startsWithIgnoreCase ("wt") ? 80.0 : 1.0; step = 0.001;
                }
                else if (! editsLiveParameter && lower.contains ("ratio"))
                {
                    min = 1.0; max = 16.0; step = 1.0;
                }

                if (valueIn < min || valueIn > max)
                {
                    min = std::floor ((double) valueIn - 1.0);
                    max = std::ceil ((double) valueIn + 1.0);
                    step = 0.001;
                }

                slider.setRange (min, max, step);
                slider.setValue (valueIn, juce::dontSendNotification);
                slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                slider.onValueChange = [this]
                {
                    if (editsLiveParameter)
                    {
                        owner.project.getLiveValues().setValue (key, (float) slider.getValue());
                        refreshValueLabel();
                        owner.refreshFxPreviewRouting();
                        owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                        owner.formulaPanel.repaint();
                        owner.repaint();
                        return;
                    }

                    if (blockIndex < 0 || blockIndex >= (int) owner.project.getDspGraph().blocks.size())
                        return;
                    auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
                    block.values[key] = (float) slider.getValue();
                    refreshValueLabel();
                    owner.markGraphEdited();
                    owner.refreshFxPreviewRouting();
                    owner.syncGraphEditor();
                    owner.project.notifyChanged();
                    owner.builderPanel.repaint();
                    owner.formulaPanel.repaint();
                    owner.repaint();
                };
                addAndMakeVisible (slider);
                refreshValueLabel();
            }

            void resized() override
            {
                auto row = getLocalBounds().reduced (2);
                label.setBounds (row.removeFromLeft (132));
                valueLabel.setBounds (row.removeFromRight (76));
                row.removeFromRight (8);
                slider.setBounds (row);
            }

            void refreshValueLabel()
            {
                valueLabel.setText (juce::String (slider.getValue(), slider.getInterval() >= 1.0 ? 0 : 3),
                                    juce::dontSendNotification);
            }

            DspPage& owner;
            int blockIndex = -1;
            juce::String key;
            bool editsLiveParameter = false;
            juce::Label label;
            juce::Label valueLabel;
            juce::Slider slider;
        };

        struct FormulaRowsPanel final : public juce::Component
        {
            explicit FormulaRowsPanel (juce::String titleIn) : title (std::move (titleIn))
            {
                setOpaque (true);
            }

            void addBlockRow (DspPage& owner, int blockIndex, const juce::String& key, float value)
            {
                rows.push_back (std::make_unique<FormulaValueRow> (owner, blockIndex, key, value, false));
                addAndMakeVisible (*rows.back());
            }

            void addLiveParameterRow (DspPage& owner, const juce::String& id)
            {
                if (const auto* def = owner.project.getParameters().find (id))
                {
                    if (! def->visible)
                        return;
                    const auto value = owner.project.getLiveValues().getValue (id, def->defaultValue);
                    rows.push_back (std::make_unique<FormulaValueRow> (owner, -1, id, value, true));
                    addAndMakeVisible (*rows.back());
                }
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (16);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::Font (16.0f, juce::Font::bold));
                g.drawText (title, r.removeFromTop (24), juce::Justification::centredLeft, true);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (12.0f);
                if (rows.empty())
                    g.drawFittedText ("No editable values in this group. Check Routes or Downstream for controls affecting the final sound.",
                                      r.removeFromTop (50), juce::Justification::topLeft, 2);
            }

            void resized() override
            {
                auto list = getLocalBounds().reduced (16);
                list.removeFromTop (34);
                for (auto& row : rows)
                {
                    row->setBounds (list.removeFromTop (34));
                    list.removeFromTop (4);
                }
            }

            juce::String title;
            std::vector<std::unique_ptr<FormulaValueRow>> rows;
        };

        struct FormulaInfoPanel final : public juce::Component
        {
            FormulaInfoPanel (juce::String titleIn, juce::StringArray linesIn)
                : title (std::move (titleIn)), lines (std::move (linesIn))
            {
                setOpaque (true);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (18);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::Font (17.0f, juce::Font::bold));
                g.drawText (title, r.removeFromTop (28), juce::Justification::centredLeft, true);
                r.removeFromTop (8);
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
                r.reduce (14, 12);
                g.setFont (13.0f);
                for (const auto& line : lines)
                {
                    g.setColour (line.startsWithIgnoreCase ("warning")
                        ? PatchCraftLookAndFeel::accent()
                        : PatchCraftLookAndFeel::text());
                    g.drawFittedText (line, r.removeFromTop (36), juce::Justification::topLeft, 2);
                    r.removeFromTop (2);
                    if (r.getHeight() <= 20)
                        break;
                }
            }

            juce::String title;
            juce::StringArray lines;
        };

        struct BlockEditor final : public juce::Component
        {
            BlockEditor (DspPage& ownerIn, int blockIndexIn)
                : owner (ownerIn), blockIndex (blockIndexIn), tabs (juce::TabbedButtonBar::TabsAtTop)
            {
                setSize (940, 700);
                tabs.setTabBarDepth (34);
                addAndMakeVisible (tabs);
                rebuildTabs();

                closeButton.getProperties().set ("smallButton", true);
                toggleButton.getProperties().set ("smallButton", true);
                copyButton.getProperties().set ("smallButton", true);
                toggleButton.onClick = [this]
                {
                    if (blockIndex >= 0 && blockIndex < (int) owner.project.getDspGraph().blocks.size())
                    {
                        auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
                        block.enabled = ! block.enabled;
                        owner.markGraphEdited();
                        owner.project.notifyChanged();
                        owner.refresh();
                        repaint();
                    }
                };
                copyButton.onClick = [this]
                {
                    if (blockIndex >= 0 && blockIndex < (int) owner.project.getDspGraph().blocks.size())
                    {
                        const auto& block = owner.project.getDspGraph().blocks[(size_t) blockIndex];
                        juce::StringArray lines;
                        lines.add (block.name + " [" + block.type + "]");
                        lines.add (owner.blockImpactDescription (block));
                        for (const auto& [key, value] : block.values)
                            lines.add (key + ": " + juce::String (value, 3));
                        juce::SystemClipboard::copyTextToClipboard (lines.joinIntoString ("\n"));
                    }
                };
                closeButton.onClick = [this]
                {
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };
                for (auto* button : { &toggleButton, &copyButton, &closeButton })
                    addAndMakeVisible (*button);
            }

            bool validBlock() const
            {
                return blockIndex >= 0 && blockIndex < (int) owner.project.getDspGraph().blocks.size();
            }

            const DspBlock* block() const
            {
                return validBlock() ? &owner.project.getDspGraph().blocks[(size_t) blockIndex] : nullptr;
            }

            static bool isShapeDataKey (const juce::String& key)
            {
                return key.startsWith ("wtShape") || key.startsWith ("wtFrame");
            }

            void rebuildTabs()
            {
                tabs.clearTabs();
                const auto* currentBlock = block();
                if (currentBlock == nullptr)
                    return;

                tabs.addTab ("Overview", PatchCraftLookAndFeel::panel(), makeOverviewPanel (*currentBlock), true);
                tabs.addTab ("Block Values", PatchCraftLookAndFeel::panel(), makeBlockValuesPanel (*currentBlock), true);
                if (currentBlock->section == "source")
                    tabs.addTab ("Source Stack", PatchCraftLookAndFeel::panel(), makeSourceStackPanel (*currentBlock), true);
                if (currentBlock->section == "source" && currentBlock->type.containsIgnoreCase ("wavetable"))
                    tabs.addTab ("Wavetable", PatchCraftLookAndFeel::panel(), makeWavetablePanel (*currentBlock), true);
                tabs.addTab ("Routes", PatchCraftLookAndFeel::panel(), makeRoutesPanel (*currentBlock), true);
                tabs.addTab ("Downstream", PatchCraftLookAndFeel::panel(), makeDownstreamPanel (*currentBlock), true);
            }

            juce::Component* makeOverviewPanel (const DspBlock& currentBlock)
            {
                juce::StringArray lines;
                lines.add (currentBlock.name + " [" + currentBlock.type + "] is in " + currentBlock.section.toUpperCase() + ".");
                lines.add (owner.blockImpactDescription (currentBlock));
                lines.add ("Block Values are stored on this formula block. Source Stack shows global source parameters that can still affect what you hear.");
                if (currentBlock.section == "source" && currentBlock.type.containsIgnoreCase ("wavetable"))
                {
                    lines.add ("Warning: OSC 2 Type can be set to Noise. That noise is independent of Noise Blend.");
                    lines.add ("Use Source Stack > OSC 2 Type to remove OSC2 noise, or Source Stack > Noise Blend to remove the separate additive noise layer.");
                }
                lines.add ("Routes shows Macro, Mod, and Automation lanes that touch this block or this section.");
                lines.add ("Downstream shows Filter, Amp, FX, and Out blocks that still shape the final result after this source.");
                return new FormulaInfoPanel ("What changes this sound?", lines);
            }

            juce::Component* makeBlockValuesPanel (const DspBlock& currentBlock)
            {
                auto* panel = new FormulaRowsPanel ("Block-local values");
                for (const auto& [key, value] : currentBlock.values)
                {
                    if (key == "bank")
                        continue;
                    if (isShapeDataKey (key))
                        continue;
                    panel->addBlockRow (owner, blockIndex, key, value);
                }
                return panel;
            }

            juce::Component* makeSourceStackPanel (const DspBlock&)
            {
                auto* panel = new FormulaRowsPanel ("Audible source stack");
                const juce::StringArray ids {
                    "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend",
                    "wtEnabled", "wtTable", "wtPosition", "wtMorph", "wtWarp", "wtFold",
                    "wtUnison", "wtDetune", "wtSpread", "wtLevel", "wtBend", "wtSyncRatio",
                    "wtSpectralTilt", "wtFramePosition", "wtFrameCount", "volume", "pan"
                };
                for (const auto& id : ids)
                    panel->addLiveParameterRow (owner, id);
                return panel;
            }

            juce::Component* makeWavetablePanel (const DspBlock& currentBlock)
            {
                auto* panel = new FormulaRowsPanel ("Wavetable block values");
                for (const auto& [key, value] : currentBlock.values)
                {
                    if (key.startsWith ("wt") && ! isShapeDataKey (key))
                        panel->addBlockRow (owner, blockIndex, key, value);
                }
                return panel;
            }

            bool targetTouchesBlock (const DspBlock& currentBlock, const juce::String& target) const
            {
                if (target.isEmpty())
                    return false;
                if (target == currentBlock.id || target == currentBlock.targetId)
                    return true;
                if (currentBlock.values.find (target) != currentBlock.values.end())
                    return true;
                return owner.targetAppliesToSection (target, currentBlock.section);
            }

            juce::Component* makeRoutesPanel (const DspBlock& currentBlock)
            {
                juce::StringArray lines;
                for (const auto& macro : owner.project.getDspGraph().macros)
                    if (targetTouchesBlock (currentBlock, macro.targetId) || targetTouchesBlock (currentBlock, macro.macroId))
                        lines.add ("Macro: " + macro.macroId + " -> " + macro.targetId
                                   + "  range " + juce::String (macro.targetMin, 2) + " to " + juce::String (macro.targetMax, 2));
                for (const auto& route : owner.project.getDspGraph().modulation)
                    if (targetTouchesBlock (currentBlock, route.targetId) || targetTouchesBlock (currentBlock, route.sourceId))
                        lines.add ("Mod: " + route.sourceId + " -> " + route.targetId
                                   + "  amount " + juce::String (route.amount, 2)
                                   + (route.enabled ? "  enabled" : "  bypassed"));
                for (const auto& lane : owner.project.getDspGraph().automation)
                    if (targetTouchesBlock (currentBlock, lane.targetId))
                        lines.add ("Automation: " + lane.targetId
                                   + "  rate " + juce::String (lane.rate, 2)
                                   + (lane.syncToTempo ? "  BPM synced" : "  free"));
                if (lines.isEmpty())
                    lines.add ("No Macro, Mod, or Automation route currently targets this block or section.");
                lines.add ("Tip: use + Automation for curve movement, + Mod for LFO/random/audio-reactive movement, and + Macro for performance controls.");
                return new FormulaInfoPanel ("Routes that can move this sound", lines);
            }

            juce::Component* makeDownstreamPanel (const DspBlock& currentBlock)
            {
                juce::StringArray lines;
                for (const auto& other : owner.project.getDspGraph().blocks)
                {
                    if (&other == &currentBlock)
                        continue;
                    if (other.section == currentBlock.section && currentBlock.section == "source")
                        lines.add ("Source layer: " + other.name + " - " + owner.blockImpactDescription (other));
                    else if (other.section == "filter" || other.section == "amp" || other.section == "fx" || other.section == "out")
                        lines.add (other.section.toUpperCase() + ": " + other.name + " - " + owner.blockImpactDescription (other));
                }
                if (lines.isEmpty())
                    lines.add ("No downstream blocks are active yet. Add Filter, Amp, FX, and Out blocks to finish the instrument chain.");
                return new FormulaInfoPanel ("What happens after this block", lines);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (18);
                const auto* selectedBlock = block();

                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::Font (20.0f, juce::Font::bold));
                g.drawText (selectedBlock != nullptr ? selectedBlock->name : juce::String ("Block Editor"),
                            r.removeFromTop (28), juce::Justification::centredLeft, true);

                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (12.0f);
                g.drawFittedText (selectedBlock != nullptr ? owner.blockImpactDescription (*selectedBlock) : juce::String(),
                                  r.removeFromTop (46), juce::Justification::topLeft, 2);
                r.removeFromTop (8);
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r.withTrimmedBottom (48).toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.withTrimmedBottom (48).toFloat(), 10.0f, 1.0f);
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (18);
                r.removeFromTop (90);
                auto buttonRow = r.removeFromBottom (36);
                closeButton.setBounds (buttonRow.removeFromRight (90));
                buttonRow.removeFromRight (8);
                copyButton.setBounds (buttonRow.removeFromRight (120));
                buttonRow.removeFromRight (8);
                toggleButton.setBounds (buttonRow.removeFromRight (130));
                r.removeFromBottom (12);
                tabs.setBounds (r.reduced (8, 10));
            }

            DspPage& owner;
            int blockIndex = -1;
            juce::TabbedComponent tabs;
            juce::TextButton toggleButton { "Toggle On/Off" };
            juce::TextButton copyButton { "Copy Formula" };
            juce::TextButton closeButton { "Close" };
        };

        auto* editor = new BlockEditor (*this, blockIndex);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Sound Formula Block Editor";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (editor);
        options.launchAsync();
    }

    void DspPage::selectNodeMapRoute (int kind, int index)
    {
        selectedGraphKind = kind;
        selectedGraphIndex = index;
        const int itemId = kind * 1000 + index + 1;
        builderPanel.selectedItemId = itemId;
        builderPanel.selectedItemIds.clear();
        builderPanel.selectedItemIds.add (itemId);
        editorItemBox.setSelectedId (itemId, juce::sendNotification);
        syncGraphEditor();
        refreshBuilderPanel();
        repaint();
    }

    void DspPage::showNodeMapPopout()
    {
        const auto sectionId = currentSectionId();
        const int bank = currentSectionBank();
        std::vector<NodeMapBlockSummary> blocks;
        int ordinal = 0;
        for (int blockIndex = 0; blockIndex < (int) project.getDspGraph().blocks.size(); ++blockIndex)
        {
            const auto& block = project.getDspGraph().blocks[(size_t) blockIndex];
            if (block.section != sectionId)
                continue;

            const int blockBank = bankForBlockOrdinal (block, ordinal++);
            if (blockBank != bank)
                continue;

            NodeMapBlockSummary summary;
            summary.name = block.name.isNotEmpty() ? block.name : block.type;
            summary.type = block.type.isNotEmpty() ? block.type : "block";
            summary.target = block.targetId.isNotEmpty() ? block.targetId : "section";
            summary.graphIndex = blockIndex;
            summary.enabled = block.enabled;
            summary.values.add ("Target: " + summary.target);

            int valueCount = 0;
            for (const auto& [key, value] : block.values)
            {
                if (key == "bank")
                    continue;

                summary.values.add (key + ": " + juce::String (value, 2));
                if (++valueCount >= 3)
                    break;
            }

            if (summary.values.size() == 1)
                summary.values.add (blockImpactDescription (block));

            blocks.push_back (std::move (summary));
        }

        std::vector<NodeMapRouteSummary> routes;
        for (int macroIndex = 0; macroIndex < (int) project.getDspGraph().macros.size(); ++macroIndex)
        {
            const auto& macro = project.getDspGraph().macros[(size_t) macroIndex];
            if (! targetAppliesToSection (macro.targetId, sectionId))
                continue;

            NodeMapRouteSummary route;
            route.label = "Macro: " + macro.macroId;
            route.detail = macro.targetId + "  " + juce::String (macro.targetMin, 2)
                         + " -> " + juce::String (macro.targetMax, 2);
            route.amount = std::abs (macro.targetMax - macro.targetMin);
            route.kind = 2;
            route.index = macroIndex;
            route.enabled = true;
            routes.push_back (std::move (route));
        }

        for (int modIndex = 0; modIndex < (int) project.getDspGraph().modulation.size(); ++modIndex)
        {
            const auto& mod = project.getDspGraph().modulation[(size_t) modIndex];
            if (! targetAppliesToSection (mod.targetId, sectionId))
                continue;

            NodeMapRouteSummary route;
            route.label = "Mod: " + mod.sourceId;
            route.detail = mod.targetId + "  amount " + juce::String (mod.amount, 2);
            route.amount = mod.amount;
            route.kind = 3;
            route.index = modIndex;
            route.enabled = mod.enabled;
            routes.push_back (std::move (route));
        }

        for (int laneIndex = 0; laneIndex < (int) project.getDspGraph().automation.size(); ++laneIndex)
        {
            const auto& lane = project.getDspGraph().automation[(size_t) laneIndex];
            if (! targetAppliesToSection (lane.targetId, sectionId))
                continue;

            NodeMapRouteSummary route;
            route.label = "Automation: " + lane.mode;
            route.detail = lane.targetId + "  rate " + juce::String (lane.rate, 2)
                         + (lane.syncToTempo ? " sync" : " free");
            route.amount = 1.0f;
            route.kind = 4;
            route.index = laneIndex;
            route.enabled = true;
            routes.push_back (std::move (route));
        }

        auto* view = new DspNodeMapView (getCurrentPatchSectionLabel(), bank,
                                         std::move (blocks), std::move (routes),
                                         [this] (int blockIndex)
                                         {
                                             selectedGraphKind = 1;
                                             selectedGraphIndex = blockIndex;
                                             const int itemId = 1000 + blockIndex + 1;
                                             builderPanel.selectedItemId = itemId;
                                             builderPanel.selectedItemIds.clear();
                                             builderPanel.selectedItemIds.add (itemId);
                                             editorItemBox.setSelectedId (itemId, juce::sendNotification);
                                             syncGraphEditor();
                                             refreshBuilderPanel();
                                         },
                                         [this] (int blockIndex) { showBlockEditorPopout (blockIndex); },
                                         [this] (int kind, int index) { selectNodeMapRoute (kind, index); });

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "DSP Node Map";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (view);
        options.launchAsync();
    }

    void DspPage::showSectionEditorPopout()
    {
        auto* view = new SectionEditorPopout (*this, currentTab);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = builderPanel.openSectionEditorButton.getButtonText();
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (view);
        options.launchAsync();
    }

    void DspPage::showSectionMixerPopout()
    {
        auto* view = new SectionMixerPopout (*this);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = getCurrentPatchSectionLabel() + " Mixer";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (view);
        options.launchAsync();
    }

    void DspPage::rebuildGraphEditorItems()
    {
        syncingEditor = true;
        editorItemBox.clear (juce::dontSendNotification);
        int itemId = 1;
        const auto sectionId = currentSectionId();
        for (int i = 0; i < (int) project.getDspGraph().blocks.size(); ++i)
        {
            const auto& block = project.getDspGraph().blocks[(size_t) i];
            if (block.section == sectionId)
                editorItemBox.addItem ("Block: " + block.name, 1000 + i + 1);
        }
        if (sectionId == "mod")
        {
            for (int i = 0; i < (int) project.getDspGraph().macros.size(); ++i)
            {
                const auto& macro = project.getDspGraph().macros[(size_t) i];
                editorItemBox.addItem ("Macro: " + macro.macroId + " -> " + macro.targetId, 2000 + i + 1);
            }
            for (int i = 0; i < (int) project.getDspGraph().modulation.size(); ++i)
            {
                const auto& route = project.getDspGraph().modulation[(size_t) i];
                editorItemBox.addItem ("Mod: " + route.sourceId + " -> " + route.targetId, 3000 + i + 1);
            }
            for (int i = 0; i < (int) project.getDspGraph().automation.size(); ++i)
            {
                const auto& lane = project.getDspGraph().automation[(size_t) i];
                editorItemBox.addItem ("Auto: " + lane.targetId, 4000 + i + 1);
            }
        }
        else
        {
            for (int i = 0; i < (int) project.getDspGraph().macros.size(); ++i)
            {
                const auto& macro = project.getDspGraph().macros[(size_t) i];
                if (targetAppliesToSection (macro.targetId, sectionId))
                    editorItemBox.addItem ("Macro: " + macro.macroId + " -> " + macro.targetId, 2000 + i + 1);
            }
            for (int i = 0; i < (int) project.getDspGraph().modulation.size(); ++i)
            {
                const auto& route = project.getDspGraph().modulation[(size_t) i];
                if (targetAppliesToSection (route.targetId, sectionId))
                    editorItemBox.addItem ("Mod: " + route.sourceId + " -> " + route.targetId, 3000 + i + 1);
            }
            for (int i = 0; i < (int) project.getDspGraph().automation.size(); ++i)
            {
                const auto& lane = project.getDspGraph().automation[(size_t) i];
                if (targetAppliesToSection (lane.targetId, sectionId))
                    editorItemBox.addItem ("Auto: " + lane.targetId, 4000 + i + 1);
            }
        }
        editorItemBox.setTextWhenNothingSelected ("Select graph item");
        syncingEditor = false;

        if (selectedGraphKind == 0)
            editorItemBox.setSelectedId (0, juce::dontSendNotification);
        else
            editorItemBox.setSelectedId (selectedGraphKind * 1000 + selectedGraphIndex + 1,
                                         juce::dontSendNotification);
        syncGraphEditor();
    }

    void DspPage::selectGraphEditorItem (int itemId)
    {
        if (syncingEditor) return;
        if (itemId <= 0)
        {
            selectedGraphKind = 0;
            selectedGraphIndex = -1;
            builderPanel.selectedItemIds.clear();
            builderPanel.selectedItemId = 0;
        }
        else
        {
            selectedGraphKind = itemId / 1000;
            selectedGraphIndex = (itemId % 1000) - 1;
            builderPanel.selectedItemIds.clear();
            builderPanel.selectedItemIds.add (itemId);
            builderPanel.selectedItemId = itemId;
        }
        syncGraphEditor();
        builderPanel.repaint();
    }

    void DspPage::syncGraphEditor()
    {
        syncingEditor = true;
        resetGraphControlModes();
        fillSourceBox();
        fillTargetBox();
        typeBox.clear (juce::dontSendNotification);
        typeBox.setTextWhenNothingSelected ("Type");

        auto selectBySuffix = [] (juce::ComboBox& box, const juce::String& id)
        {
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemText (i).contains ("(" + id + ")"))
                    box.setSelectedItemIndex (i, juce::dontSendNotification);
        };

        const bool hasSelection = selectedGraphKind > 0 && selectedGraphIndex >= 0;
        bool selectedBlockCanRoute = false;
        if (selectedGraphKind == 1 && selectedGraphIndex >= 0
            && selectedGraphIndex < (int) project.getDspGraph().blocks.size())
            selectedBlockCanRoute = project.getDspGraph().blocks[(size_t) selectedGraphIndex].section == "mod";
        const bool sourceEditable = selectedGraphKind == 2 || selectedGraphKind == 3;
        const bool targetEditable = selectedBlockCanRoute || selectedGraphKind == 2 || selectedGraphKind == 3 || selectedGraphKind == 4;
        deleteGraphItemButton.setEnabled (hasSelection);
        const bool canBypass = (selectedGraphKind == 1 || selectedGraphKind == 3) && selectedGraphIndex >= 0;
        enableGraphItemButton.setEnabled (canBypass);
        enableGraphItemButton.setToggleState (false, juce::dontSendNotification);
        enableGraphItemButton.setTooltip (canBypass
            ? "Bypass/enable this Block or Mod Route without deleting it."
            : "Bypass is available for Blocks and Mod Routes. Automation and Macros remain editable/deletable.");

        amountSlider.setValue (0.0, juce::dontSendNotification);
        rateSlider.setValue (1.0, juce::dontSendNotification);
        valueSlider.setValue (0.5, juce::dontSendNotification);
        minSlider.setValue (0.0, juce::dontSendNotification);
        maxSlider.setValue (1.0, juce::dontSendNotification);
        curveSlider.setValue (1.0, juce::dontSendNotification);
        amountSlider.setRange (-1.0, 1.0, 0.001);
        rateSlider.setRange (0.01, 40.0, 0.001);
        valueSlider.setRange (0.0, 1.0, 0.001);
        minSlider.setRange (0.0, 1.0, 0.001);
        maxSlider.setRange (0.0, 1.0, 0.001);
        curveSlider.setRange (0.05, 4.0, 0.001);
        amountLabel.setText ("AMOUNT / RESONANCE", juce::dontSendNotification);
        rateLabel.setText ("RATE / TIME", juce::dontSendNotification);
        valueLabel.setText ("VALUE / TONE", juce::dontSendNotification);
        minLabel.setText ("MIN / SYNC", juce::dontSendNotification);
        maxLabel.setText ("MAX / RANGE", juce::dontSendNotification);
        curveLabel.setText ("CURVE / MIX", juce::dontSendNotification);
        typeBox.setEnabled (selectedGraphKind == 1);
        sourceBox.setEnabled (sourceEditable);
        targetBox.setEnabled (targetEditable);
        amountSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 3);
        rateSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 3 || selectedGraphKind == 4);
        valueSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 4);
        minSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 2 || selectedGraphKind == 3 || selectedGraphKind == 4);
        maxSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 2);
        curveSlider.setEnabled (selectedGraphKind == 1 || selectedGraphKind == 2);
        typeBox.setTooltip (selectedGraphKind == 1
            ? "Choose what this block is: oscillator, filter, LFO, macro, effect, output, etc."
            : "Enabled when a Block card is selected. Macros, Mod Routes, and Automation do not have a block type.");
        sourceBox.setTooltip (sourceEditable
            ? "Choose the knob, parameter, or block that drives this macro/mod route."
            : "Disabled: this selected item has no external source. Add + Macro or + Mod, then select that card.");
        targetBox.setTooltip (targetEditable
            ? "Select the sound parameter or block this selected item changes. The list is filtered to active targets for this section."
            : hasSelection
                ? "Disabled: this block writes its own internal DSP target. Use the visible controls or add + Mod/+ Macro for routing."
                : "Select a block, macro, mod route, or automation lane before choosing a target.");
        amountSlider.setTooltip ((selectedGraphKind == 1 || selectedGraphKind == 3)
            ? "For Blocks this is amount/resonance/depth. For Mod Routes this is modulation depth."
            : "Enabled for Blocks and Mod Routes. Select a Block or add/select + Mod.");
        rateSlider.setTooltip ((selectedGraphKind == 1 || selectedGraphKind == 3 || selectedGraphKind == 4)
            ? "Controls block rate, source LFO rate, or automation speed."
            : "Enabled for Blocks, Mod Routes, and Automation. Select one of those graph items.");
        valueSlider.setTooltip ((selectedGraphKind == 1 || selectedGraphKind == 4)
            ? "Sets the block base value/tone or automation shape."
            : "Enabled for Blocks and Automation. Use Macro min/max/curve or Mod depth for routes.");
        minSlider.setTooltip ((selectedGraphKind == 1 || selectedGraphKind == 2 || selectedGraphKind == 3 || selectedGraphKind == 4)
            ? "For Blocks/Mod/Automation this toggles sync. For Macros it sets target minimum."
            : "Select a graph item to enable this control.");
        maxSlider.setTooltip (selectedGraphKind == 1
            ? "For Blocks this controls the extra audible parameter shown in its label."
            : selectedGraphKind == 2
                ? "Sets the maximum target value reached by this macro."
                : "Enabled for Blocks and Macros.");
        curveSlider.setTooltip (selectedGraphKind == 1
            ? "For Blocks this controls the extra audible parameter shown in its label."
            : selectedGraphKind == 2
                ? "Shapes the macro response curve between min and max."
                : "Enabled for Blocks and Macros.");
        amountLabel.setTooltip (amountSlider.getTooltip());
        rateLabel.setTooltip (rateSlider.getTooltip());
        valueLabel.setTooltip (valueSlider.getTooltip());
        minLabel.setTooltip (minSlider.getTooltip());
        maxLabel.setTooltip (maxSlider.getTooltip());
        curveLabel.setTooltip (curveSlider.getTooltip());

        if (selectedGraphKind == 1 && selectedGraphIndex < (int) project.getDspGraph().blocks.size())
        {
            auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
            enableGraphItemButton.setToggleState (block.enabled, juce::dontSendNotification);
            editorHint.setText ("Block '" + block.name + "' directly shapes " + block.section
                                + ". Target chooses what it drives; value/rate/amount define the audible behavior.",
                                juce::dontSendNotification);
            int typeItemId = 1;
            auto addTypeGroup = [&] (const juce::String& heading, std::initializer_list<const char*> types)
            {
                if (typeItemId > 1)
                    typeBox.addSeparator();
                typeBox.addSectionHeading (heading);
                for (auto* type : types)
                    typeBox.addItem (type, typeItemId++);
            };

            if (block.section == "source")
            {
                if (project.getEngineType() == "fx")
                    addTypeGroup ("FX Input", { "drive" });
                else if (project.getEngineType() == "sample")
                {
                    addTypeGroup ("Sample Sources", { "sampleLayer" });
                    addTypeGroup ("Texture", { "noise" });
                }
                else
                {
                    addTypeGroup ("Oscillators", { "oscillator", "wavetable" });
                    addTypeGroup ("Texture", { "noise" });
                }
            }
            else if (block.section == "filter")
            {
                addTypeGroup ("Filters", { "stateVariable" });
                addTypeGroup ("Surgical EQ", { "surgicalEq" });
            }
            else if (block.section == "amp")
            {
                addTypeGroup ("Envelopes", { "adsr", "gate" });
            }
            else if (block.section == "mod")
            {
                addTypeGroup ("Sequencers", { "midiPlayground", "arp", "stepSequencer" });
                addTypeGroup ("LFO / Random", { "lfo", "random" });
                addTypeGroup ("Macro", { "macro" });
                addTypeGroup ("MIDI / Performance", { "velocity", "keytrack", "midiCC" });
                addTypeGroup ("Audio Reactive", { "envelopeFollower", "peakFollower", "rmsFollower",
                                                  "transientDetector", "spectralCentroid", "bandEnergy", "gateTrigger" });
            }
            else if (block.section == "fx")
            {
                addTypeGroup ("Time", { "delay", "multiTapDelay" });
                addTypeGroup ("Space", { "reverb" });
                addTypeGroup ("Vintage / LoFi", { "tape", "vinyl", "lofi" });
                addTypeGroup ("Saturation", { "distortion", "waveshaper", "bitcrush" });
                addTypeGroup ("Dynamics", { "dynamics" });
                addTypeGroup ("Modulation", { "chorus", "phaser" });
                addTypeGroup ("Vocal", { "vocalFormant" });
                addTypeGroup ("Resonators", { "comb", "resonator" });
                addTypeGroup ("Convolution / Spectral", { "convolution", "spectral" });
            }
            else
            {
                addTypeGroup ("Output", { "output" });
                addTypeGroup ("Utilities", { "utility", "performance" });
            }
            typeBox.setText (block.type, juce::dontSendNotification);
            selectBySuffix (targetBox, block.targetId);
            auto get = [&] (const juce::String& key, float fallback)
            {
                auto it = block.values.find (key);
                return it == block.values.end() ? fallback : it->second;
            };
            amountSlider.setValue (get ("amount", get ("resonance", get ("delayMix", 0.15f))), juce::dontSendNotification);
            rateSlider.setValue (get ("rate", 1.0f), juce::dontSendNotification);
            valueSlider.setValue (get ("value", get ("cutoff", get ("reverbMix", get ("volume", 0.5f)))), juce::dontSendNotification);
            minSlider.setValue (get ("sync", 0.0f), juce::dontSendNotification);
            if (block.section == "fx")
            {
                const bool multiTap = block.type.containsIgnoreCase ("multiTap");
                const bool delay = block.type.containsIgnoreCase ("delay") && ! multiTap;
                const bool distortion = block.type.containsIgnoreCase ("dist") || block.type.containsIgnoreCase ("shape") || block.type.containsIgnoreCase ("crush");
                const bool dynamics = block.type.containsIgnoreCase ("dynamics") || block.type.containsIgnoreCase ("compress");
                const bool chorus = block.type.containsIgnoreCase ("chorus");
                const bool phaser = block.type.containsIgnoreCase ("phaser");
                const bool comb = block.type.containsIgnoreCase ("comb");
                const bool resonator = block.type.containsIgnoreCase ("resonator");
                const bool convolution = block.type.containsIgnoreCase ("convolution");
                const bool spectral = block.type.containsIgnoreCase ("spectral");
                const bool tape = block.type.containsIgnoreCase ("tape");
                const bool vinyl = block.type.containsIgnoreCase ("vinyl") || block.type.containsIgnoreCase ("oldSchool");
                const bool lofi = block.type.containsIgnoreCase ("lofi");
                const bool vocal = block.type.containsIgnoreCase ("vocal") || block.type.containsIgnoreCase ("formant");
                amountSlider.setRange (0.0, 1.0, 0.001);
                curveSlider.setRange (0.0, 1.0, 0.001);
                if (multiTap)
                {
                    amountLabel.setText ("MIX", juce::dontSendNotification);
                    rateLabel.setText ("TIME", juce::dontSendNotification);
                    valueLabel.setText ("SPREAD", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("FEEDBACK", juce::dontSendNotification);
                    curveLabel.setText ("-", juce::dontSendNotification);
                    minSlider.setEnabled (false);
                    curveSlider.setEnabled (false);
                    rateSlider.setRange (0.02, 2.0, 0.001);
                    valueSlider.setRange (0.0, 1.0, 0.001);
                    maxSlider.setRange (0.0, 0.92, 0.001);
                    amountSlider.setValue (get ("multiTapMix", 0.35f), juce::dontSendNotification);
                    rateSlider.setValue (get ("multiTapTime", 0.375f), juce::dontSendNotification);
                    valueSlider.setValue (get ("multiTapSpread", 0.45f), juce::dontSendNotification);
                    maxSlider.setValue (get ("multiTapFeedback", 0.35f), juce::dontSendNotification);
                    editorHint.setText ("FX Lab MultiTap Delay creates three musical taps from one block. Use Mix, Time, Spread, and Feedback for complex echo plugins without manual routing.",
                                        juce::dontSendNotification);
                }
                else if (delay)
                {
                    amountLabel.setText ("DELAY MIX", juce::dontSendNotification);
                    rateLabel.setText (get ("sync", 0.0f) >= 0.5f ? "BEATS" : "TIME", juce::dontSendNotification);
                    valueLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("BPM SYNC", juce::dontSendNotification);
                    maxLabel.setText ("FEEDBACK", juce::dontSendNotification);
                    curveLabel.setText ("-", juce::dontSendNotification);
                    valueSlider.setEnabled (false);
                    curveSlider.setEnabled (false);
                    valueSlider.setTooltip ("Disabled for Delay blocks. Add/select a Reverb block to edit reverb.");
                    curveSlider.setTooltip ("Disabled for Delay blocks. Add/select a Distortion block to edit drive.");
                    rateSlider.setRange (0.0625, get ("sync", 0.0f) >= 0.5f ? 8.0 : 2.0, 0.001);
                    amountSlider.setValue (get ("delayMix", 0.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("delayFeedback", 0.35f), juce::dontSendNotification);
                    setGraphControlSwitchMode (3, true, true, "BPM Sync switch: ON makes Rate use musical beat divisions; OFF makes Rate use free delay time.");
                }
                else if (distortion)
                {
                    valueLabel.setText ("DRIVE", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    amountLabel.setText ("-", juce::dontSendNotification);
                    rateLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    amountSlider.setEnabled (false);
                    rateSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setTooltip ("Disabled for Distortion blocks. Add/select a Delay block for delay mix.");
                    rateSlider.setTooltip ("Disabled for Distortion blocks. Add/select a Delay block for timing.");
                    minSlider.setTooltip ("Disabled for Distortion blocks. Add/select a Delay block for BPM sync.");
                    maxSlider.setTooltip ("Disabled for Distortion blocks. Add/select a Delay block for feedback.");
                    valueSlider.setValue (get ("drive", 0.55f), juce::dontSendNotification);
                    curveSlider.setValue (get ("mix", 1.0f), juce::dontSendNotification);
                }
                else if (dynamics)
                {
                    amountLabel.setText ("THRESHOLD", juce::dontSendNotification);
                    rateLabel.setText ("RATIO", juce::dontSendNotification);
                    valueLabel.setText ("ATTACK", juce::dontSendNotification);
                    minLabel.setText ("RELEASE", juce::dontSendNotification);
                    maxLabel.setText ("MAKEUP", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    amountSlider.setRange (-80.0, 12.0, 0.1);
                    rateSlider.setRange (1.0, 40.0, 0.01);
                    valueSlider.setRange (0.1, 250.0, 0.1);
                    minSlider.setRange (5.0, 1000.0, 0.1);
                    maxSlider.setRange (-24.0, 24.0, 0.1);
                    amountSlider.setValue (get ("dynThresholdDb", -18.0f), juce::dontSendNotification);
                    rateSlider.setValue (get ("dynRatio", 2.0f), juce::dontSendNotification);
                    valueSlider.setValue (get ("dynAttackMs", 10.0f), juce::dontSendNotification);
                    minSlider.setValue (get ("dynReleaseMs", 120.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("dynMakeupDb", 0.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("dynMix", 0.5f), juce::dontSendNotification);
                }
                else if (chorus || phaser)
                {
                    const auto prefix = chorus ? "chorus" : "phaser";
                    amountLabel.setText ("DEPTH", juce::dontSendNotification);
                    rateLabel.setText ("RATE", juce::dontSendNotification);
                    valueLabel.setText ("FEEDBACK", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    rateSlider.setRange (0.01, 20.0, 0.001);
                    valueSlider.setRange (-0.95, 0.95, 0.001);
                    amountSlider.setValue (get (prefix + juce::String ("Depth"), chorus ? 0.35f : 0.45f), juce::dontSendNotification);
                    rateSlider.setValue (get (prefix + juce::String ("Rate"), chorus ? 0.35f : 0.25f), juce::dontSendNotification);
                    valueSlider.setValue (get (prefix + juce::String ("Feedback"), 0.0f), juce::dontSendNotification);
                    curveSlider.setValue (get (prefix + juce::String ("Mix"), 0.35f), juce::dontSendNotification);
                }
                else if (comb || resonator)
                {
                    const auto prefix = comb ? "comb" : "resonator";
                    amountLabel.setText ("FREQUENCY", juce::dontSendNotification);
                    rateLabel.setText (comb ? "FEEDBACK" : "Q", juce::dontSendNotification);
                    valueLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    valueSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setRange (20.0, comb ? 8000.0 : 16000.0, 1.0);
                    rateSlider.setRange (comb ? -0.95 : 0.05, comb ? 0.95 : 18.0, 0.001);
                    amountSlider.setValue (get (prefix + juce::String ("Freq"), comb ? 220.0f : 440.0f), juce::dontSendNotification);
                    rateSlider.setValue (get (comb ? "combFeedback" : "resonatorQ", comb ? 0.35f : 4.0f), juce::dontSendNotification);
                    curveSlider.setValue (get (prefix + juce::String ("Mix"), 0.35f), juce::dontSendNotification);
                }
                else if (convolution || spectral)
                {
                    amountLabel.setText (convolution ? "TAPS" : "TILT", juce::dontSendNotification);
                    rateLabel.setText ("-", juce::dontSendNotification);
                    valueLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    rateSlider.setEnabled (false);
                    valueSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setRange (convolution ? 1.0 : -1.0, convolution ? 8.0 : 1.0, convolution ? 1.0 : 0.001);
                    amountSlider.setValue (get (convolution ? "convolutionSize" : "spectralTilt", convolution ? 3.0f : 0.0f), juce::dontSendNotification);
                    curveSlider.setValue (get (convolution ? "convolutionMix" : "spectralMix", 0.35f), juce::dontSendNotification);
                }
                else if (tape || vinyl)
                {
                    amountLabel.setText (tape ? "DRIVE" : "AGE", juce::dontSendNotification);
                    rateLabel.setText (tape ? "FLUTTER" : "DUST", juce::dontSendNotification);
                    valueLabel.setText (tape ? "TONE" : "WARP", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setValue (get (tape ? "tapeDrive" : "vinylAge", tape ? 0.32f : 0.42f), juce::dontSendNotification);
                    rateSlider.setValue (get (tape ? "tapeFlutter" : "vinylDust", tape ? 0.12f : 0.10f), juce::dontSendNotification);
                    valueSlider.setValue (get (tape ? "tapeTone" : "vinylWarp", tape ? 0.58f : 0.16f), juce::dontSendNotification);
                    curveSlider.setValue (get (tape ? "tapeMix" : "vinylMix", 0.35f), juce::dontSendNotification);
                    editorHint.setText (tape
                        ? "FX Lab Tape adds saturation, tone shaping, and subtle flutter as one easy vintage block."
                        : "FX Lab Vinyl adds age, dust/crackle, and warp as one old-school texture block.",
                        juce::dontSendNotification);
                }
                else if (lofi)
                {
                    amountLabel.setText ("BITS", juce::dontSendNotification);
                    rateLabel.setText ("RATE CRUSH", juce::dontSendNotification);
                    valueLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    valueSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setRange (4.0, 16.0, 1.0);
                    amountSlider.setValue (get ("lofiBits", 10.0f), juce::dontSendNotification);
                    rateSlider.setValue (get ("lofiRate", 0.22f), juce::dontSendNotification);
                    curveSlider.setValue (get ("lofiMix", 0.35f), juce::dontSendNotification);
                    editorHint.setText ("FX Lab LoFi combines bit depth and sample-rate style crush for old sampler, SP, and digital degradation sounds.",
                                        juce::dontSendNotification);
                }
                else if (vocal)
                {
                    amountLabel.setText ("FORMANT", juce::dontSendNotification);
                    rateLabel.setText ("BODY", juce::dontSendNotification);
                    valueLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    valueSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setValue (get ("vocalFormant", 0.40f), juce::dontSendNotification);
                    rateSlider.setValue (get ("vocalBody", 0.35f), juce::dontSendNotification);
                    curveSlider.setValue (get ("vocalMix", 0.35f), juce::dontSendNotification);
                    editorHint.setText ("FX Lab Vocal Formant turns any source into vowel-like motion. Automate Formant or map it to a macro for playable vocal effects.",
                                        juce::dontSendNotification);
                }
                else
                {
                    valueLabel.setText ("REVERB", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    amountLabel.setText ("-", juce::dontSendNotification);
                    rateLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    amountSlider.setEnabled (false);
                    rateSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setTooltip ("Disabled for Reverb blocks. Add/select a Delay block for delay mix.");
                    rateSlider.setTooltip ("Disabled for Reverb blocks. Add/select a Delay block for timing.");
                    minSlider.setTooltip ("Disabled for Reverb blocks. Add/select a Delay block for BPM sync.");
                    maxSlider.setTooltip ("Disabled for Reverb blocks. Add/select a Delay block for feedback.");
                    valueSlider.setValue (get ("reverbMix", 0.22f), juce::dontSendNotification);
                    curveSlider.setValue (get ("mix", 1.0f), juce::dontSendNotification);
                }
            }
            else if (block.section == "source")
            {
                if (project.getEngineType() == "fx")
                {
                    valueLabel.setText ("DRIVE", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    amountLabel.setText ("-", juce::dontSendNotification);
                    rateLabel.setText ("-", juce::dontSendNotification);
                    minLabel.setText ("-", juce::dontSendNotification);
                    maxLabel.setText ("-", juce::dontSendNotification);
                    amountSlider.setEnabled (false);
                    rateSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    maxSlider.setEnabled (false);
                    amountSlider.setTooltip ("Disabled for FX Source. FX input blocks only write Drive and Mix.");
                    rateSlider.setTooltip ("Disabled for FX Source. Timing is controlled in FX or Mod blocks.");
                    minSlider.setTooltip ("Disabled for FX Source. Add Output or Mod blocks for transport controls.");
                    maxSlider.setTooltip ("Disabled for FX Source. Oscillator type is not used by FX instruments.");
                    curveSlider.setRange (0.0, 1.0, 0.001);
                    valueSlider.setValue (get ("drive", 0.12f), juce::dontSendNotification);
                    curveSlider.setValue (get ("mix", 1.0f), juce::dontSendNotification);
                    editorHint.setText ("FX Source blocks process imported/host audio input. Drive and Mix are active; oscillator controls are disabled for FX instruments.",
                                        juce::dontSendNotification);
                }
                else
                {
                    const bool sampleEngine = project.getEngineType() == "sample";
                    const bool noiseBlock = block.type.containsIgnoreCase ("noise");
                    const bool wavetableBlock = block.type.containsIgnoreCase ("wavetable");
                    if (wavetableBlock)
                    {
                        valueLabel.setText ("POSITION", juce::dontSendNotification);
                        amountLabel.setText ("MORPH", juce::dontSendNotification);
                        rateLabel.setText ("WARP", juce::dontSendNotification);
                        minLabel.setText ("TABLE", juce::dontSendNotification);
                        maxLabel.setText ("UNISON", juce::dontSendNotification);
                        curveLabel.setText ("LEVEL", juce::dontSendNotification);
                        valueSlider.setRange (0.0, 1.0, 0.001);
                        amountSlider.setRange (0.0, 1.0, 0.001);
                        rateSlider.setRange (0.0, 1.0, 0.001);
                        minSlider.setRange (0.0, 8.0, 1.0);
                        maxSlider.setRange (1.0, 8.0, 1.0);
                        curveSlider.setRange (0.0, 1.5, 0.001);
                        valueSlider.setValue (get ("wtPosition", 0.0f), juce::dontSendNotification);
                        amountSlider.setValue (get ("wtMorph", 0.0f), juce::dontSendNotification);
                        rateSlider.setValue (get ("wtWarp", 0.0f), juce::dontSendNotification);
                        minSlider.setValue (get ("wtTable", 0.0f), juce::dontSendNotification);
                        maxSlider.setValue (get ("wtUnison", 1.0f), juce::dontSendNotification);
                        curveSlider.setValue (get ("wtLevel", 1.0f), juce::dontSendNotification);
                        editorHint.setText ("Wavetable Source writes a real synth source into Player. Table: 0 Analog, 1 Glass, 2 PWM, 3 Formant, 4 Razor, 5 Organ, 6 Aggro, 7 Hybrid, 8 Custom. WT Bend, Sync Ratio, Spectral Tilt, Fold/Detune/Spread are available in Quick Edit or macros.",
                                            juce::dontSendNotification);
                    }
                    else
                    {
                        amountLabel.setText (sampleEngine ? "START" : noiseBlock ? "-" : "DETUNE", juce::dontSendNotification);
                        rateLabel.setText (sampleEngine ? "PAN" : "OSC BLEND", juce::dontSendNotification);
                        valueLabel.setText (noiseBlock ? "NOISE" : "VOLUME", juce::dontSendNotification);
                        minLabel.setText (sampleEngine ? "LENGTH" : "SUB", juce::dontSendNotification);
                        maxLabel.setText (sampleEngine ? "SLICE" : "OSC TYPE", juce::dontSendNotification);
                        curveLabel.setText (sampleEngine ? "SLICES" : "NOISE", juce::dontSendNotification);
                        amountSlider.setRange (0.0, 1.0, 0.001);
                        rateSlider.setRange (0.0, 1.0, 0.001);
                        minSlider.setRange (sampleEngine ? 0.01 : 0.0, 1.0, 0.001);
                        maxSlider.setRange (sampleEngine ? 0.0 : 0.0, sampleEngine ? 63.0 : 1.0, sampleEngine ? 1.0 : 0.001);
                        curveSlider.setRange (sampleEngine ? 1.0 : 0.0, sampleEngine ? 64.0 : 1.0, sampleEngine ? 1.0 : 0.001);
                        if (sampleEngine)
                        {
                            amountSlider.setTooltip ("Sample Start. MIDI Learn or CC20 can scrub the next triggered sample start.");
                            minSlider.setTooltip ("Sample Length. MIDI Learn or CC22 can shorten playback for chops.");
                            maxSlider.setTooltip ("Sample Slice. MIDI Learn or CC21 can choose a region inside each mapped sample.");
                            curveSlider.setTooltip ("Slice Count. Defines how many equal regions the sample is divided into.");
                            editorHint.setText ("Sample Source blocks now expose MIDI-controllable playback start, length, slice, slice count, pitch, volume, and pan. Use MIDI Playground Sample Slice Control for step-driven sample chops.",
                                                juce::dontSendNotification);
                        }
                        else if (noiseBlock)
                        {
                            amountSlider.setEnabled (false);
                            amountSlider.setTooltip ("Disabled for Noise Source. This block writes noise texture amount, not pitch.");
                            editorHint.setText ("Noise Source blocks add texture into the synth stack without changing global output volume.",
                                                juce::dontSendNotification);
                        }
                        else
                        {
                            editorHint.setText ("Synth Source blocks build a stack: primary oscillator, blended second oscillator, sub layer, and noise texture.",
                                                juce::dontSendNotification);
                        }
                        valueSlider.setValue (noiseBlock ? get ("noiseBlend", 0.18f) : get ("volume", get ("value", 0.75f)), juce::dontSendNotification);
                        amountSlider.setValue (sampleEngine ? get ("sampleStart", 0.0f) : get ("detune", get ("amount", 0.5f)), juce::dontSendNotification);
                        rateSlider.setValue (sampleEngine ? get ("pan", 0.5f) : get ("oscBlend", 0.0f), juce::dontSendNotification);
                        minSlider.setValue (sampleEngine ? get ("sampleLength", 1.0f) : get ("subBlend", 0.0f), juce::dontSendNotification);
                        maxSlider.setValue (sampleEngine ? get ("sampleSlice", 0.0f) : get ("oscType", 0.25f), juce::dontSendNotification);
                        curveSlider.setValue (sampleEngine ? get ("sampleSliceCount", 1.0f) : get ("noiseBlend", 0.0f), juce::dontSendNotification);
                    }
                }
            }
            else if (block.section == "filter")
            {
                if (block.type.containsIgnoreCase ("eq"))
                {
                    valueLabel.setText ("FREQ", juce::dontSendNotification);
                    amountLabel.setText ("GAIN dB", juce::dontSendNotification);
                    rateLabel.setText ("Q", juce::dontSendNotification);
                    minLabel.setText ("TYPE", juce::dontSendNotification);
                    maxLabel.setText ("MODE", juce::dontSendNotification);
                    curveLabel.setText ("EQ MIX", juce::dontSendNotification);
                    valueSlider.setRange (20.0, 20000.0, 1.0);
                    amountSlider.setRange (-24.0, 24.0, 0.01);
                    rateSlider.setRange (0.10, 18.0, 0.01);
                    minSlider.setRange (0.0, 5.0, 1.0);
                    maxSlider.setRange (0.0, 4.0, 1.0);
                    curveSlider.setRange (0.0, 1.0, 0.001);
                    valueSlider.setSkewFactorFromMidPoint (1000.0);
                    valueSlider.setValue (get ("eqFreq", 1000.0f), juce::dontSendNotification);
                    amountSlider.setValue (get ("eqGainDb", 0.0f), juce::dontSendNotification);
                    rateSlider.setValue (get ("eqQ", 1.0f), juce::dontSendNotification);
                    minSlider.setValue (get ("eqType", 0.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("eqMode", 0.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("eqMix", 1.0f), juce::dontSendNotification);
                    editorHint.setText ("Surgical EQ writes real Player audio. Type: 0 Bell, 1 Low Shelf, 2 High Shelf, 3 High Pass, 4 Low Pass, 5 Notch. Mode: 0 Stereo, 1 Left, 2 Right, 3 Mid, 4 Side.",
                                        juce::dontSendNotification);
                }
                else
                {
                    amountLabel.setText ("RESONANCE", juce::dontSendNotification);
                    valueLabel.setText ("CUTOFF", juce::dontSendNotification);
                    rateLabel.setText ("LFO RATE", juce::dontSendNotification);
                    minLabel.setText ("SYNC", juce::dontSendNotification);
                    maxLabel.setText ("LFO AMT", juce::dontSendNotification);
                    curveLabel.setText ("MORPH", juce::dontSendNotification);
                    amountSlider.setRange (0.0, 1.0, 0.001);
                    valueSlider.setValue (get ("cutoff", 0.5f), juce::dontSendNotification);
                    amountSlider.setValue (get ("resonance", 0.2f), juce::dontSendNotification);
                    maxSlider.setValue (get ("lfoAmount", 0.0f), juce::dontSendNotification);
                    setGraphControlSwitchMode (3, true, true, "Filter LFO Sync switch: ON locks the filter LFO rate to project tempo.");
                }
            }
            else if (block.section == "amp")
            {
                amountLabel.setText ("RELEASE", juce::dontSendNotification);
                rateLabel.setText ("DECAY", juce::dontSendNotification);
                valueLabel.setText ("ATTACK", juce::dontSendNotification);
                minLabel.setText ("SUSTAIN", juce::dontSendNotification);
                maxLabel.setText ("VEL", juce::dontSendNotification);
                curveLabel.setText ("CURVE", juce::dontSendNotification);
                amountSlider.setRange (0.0, 1.0, 0.001);
                rateSlider.setRange (0.0, 1.0, 0.001);
                valueSlider.setValue (get ("attack", 0.01f), juce::dontSendNotification);
                rateSlider.setValue (get ("decay", 0.2f), juce::dontSendNotification);
                minSlider.setValue (get ("sustain", 0.8f), juce::dontSendNotification);
                amountSlider.setValue (get ("release", 0.4f), juce::dontSendNotification);
            }
            else if (block.section == "mod")
            {
                const bool lfo = block.type.containsIgnoreCase ("lfo");
                const bool random = block.type.containsIgnoreCase ("random");
                const bool arp = block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi");
                const bool audioReactive = isAudioReactiveModType (block.type);
                amountLabel.setText (arp ? "NOTE DEPTH" : lfo || random || audioReactive ? "DEPTH" : "-", juce::dontSendNotification);
                rateLabel.setText (audioReactive ? "SMOOTH" : lfo || random || arp ? "RATE" : "-", juce::dontSendNotification);
                valueLabel.setText (audioReactive ? "THRESH dB" : arp ? "GATE" : lfo || random ? "-" : "VALUE", juce::dontSendNotification);
                minLabel.setText (audioReactive ? "BAND" : lfo || arp ? "BPM SYNC" : "-", juce::dontSendNotification);
                maxLabel.setText (audioReactive ? "BIPOLAR" : arp ? "STEPS" : "-", juce::dontSendNotification);
                curveLabel.setText (arp ? "PATTERN" : "-", juce::dontSendNotification);
                amountSlider.setRange (0.0, 1.0, 0.001);
                rateSlider.setRange (audioReactive ? 1.0 : 0.01, audioReactive ? 500.0 : random ? 40.0 : 20.0, audioReactive ? 1.0 : 0.001);
                maxSlider.setEnabled (false);
                curveSlider.setEnabled (false);
                if (arp)
                {
                    valueSlider.setEnabled (true);
                    minSlider.setEnabled (true);
                    maxSlider.setEnabled (true);
                    curveSlider.setEnabled (true);
                    rateSlider.setRange (0.0625, 16.0, 0.001);
                    valueSlider.setRange (0.05, 1.0, 0.001);
                    maxSlider.setRange (1.0, 16.0, 1.0);
                    curveSlider.setRange (0.0, 7.0, 1.0);
                    amountSlider.setValue (get ("amount", 0.35f), juce::dontSendNotification);
                    rateSlider.setValue (get ("rate", 1.0f), juce::dontSendNotification);
                    valueSlider.setValue (get ("arpGate", 0.55f), juce::dontSendNotification);
                    minSlider.setValue (get ("sync", 1.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("arpSteps", 8.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("arpPattern", 0.0f), juce::dontSendNotification);
                    setGraphControlSwitchMode (3, true, true, "ARP BPM Sync switch: ON locks the step sequencer rate to tempo.");
                    amountSlider.setTooltip ("How strongly the note-step sequence moves the selected target.");
                    rateSlider.setTooltip ("Step rate. With BPM Sync enabled this is beat-rate; otherwise it runs in Hz.");
                    valueSlider.setTooltip ("Gate length stored for each ARP step; the sequencer holds note values instead of acting like a simple gate.");
                    minSlider.setTooltip ("BPM Sync for the ARP Step Sequencer.");
                    maxSlider.setTooltip ("Number of note steps in the ARP pattern.");
                    curveSlider.setTooltip ("Pattern: 0 Custom Notes, 1 Down, 2 Up/Down, 3 Chord Pulse, 4 Odd Steps, 5 Even Steps, 6 Euclidean, 7 Seeded Random.");
                    editorHint.setText ("MIDI Playground is the shared runtime for scale-aware ARP, chord/phrase generation, swing, probability, and future MIDI tools. This compact inspector edits the basics; the dedicated Playground editor is the next UI slice.",
                                        juce::dontSendNotification);
                }
                else if (lfo)
                {
                    valueSlider.setEnabled (false);
                    amountSlider.setValue (get ("amount", 0.2f), juce::dontSendNotification);
                    rateSlider.setValue (get ("rate", 1.0f), juce::dontSendNotification);
                    minSlider.setValue (get ("sync", 0.0f), juce::dontSendNotification);
                    setGraphControlSwitchMode (3, true, true, "LFO Sync switch: ON locks this LFO to tempo; OFF runs it in Hz.");
                    valueSlider.setTooltip ("Disabled for LFO blocks. LFO output is generated from Rate, Depth, and Sync.");
                    maxSlider.setTooltip ("Disabled for LFO blocks. Route target is selected in the Target menu.");
                    curveSlider.setTooltip ("Disabled for LFO blocks. Add Automation for custom curves.");
                }
                else if (random)
                {
                    valueSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    amountSlider.setValue (get ("amount", 0.2f), juce::dontSendNotification);
                    rateSlider.setValue (get ("rate", 4.0f), juce::dontSendNotification);
                    valueSlider.setTooltip ("Disabled for Random blocks. Output is generated randomly.");
                    minSlider.setTooltip ("Disabled for Random blocks. Random is currently free-running.");
                    maxSlider.setTooltip ("Disabled for Random blocks. Route target is selected in the Target menu.");
                    curveSlider.setTooltip ("Disabled for Random blocks. Add Automation for custom shapes.");
                }
                else if (audioReactive)
                {
                    minSlider.setRange (0.0, 2.0, 1.0);
                    maxSlider.setRange (0.0, 1.0, 1.0);
                    maxSlider.setEnabled (true);
                    curveSlider.setEnabled (false);
                    amountSlider.setValue (get ("amount", 0.25f), juce::dontSendNotification);
                    rateSlider.setValue (get ("smoothingMs", 80.0f), juce::dontSendNotification);
                    valueSlider.setRange (-90.0, 0.0, 0.1);
                    valueSlider.setValue (get ("thresholdDb", -36.0f), juce::dontSendNotification);
                    minSlider.setValue (get ("band", block.type.containsIgnoreCase ("bandEnergy") ? 1.0f : 0.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("bipolar", 0.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("sensitivity", 1.0f), juce::dontSendNotification);
                    setGraphControlSwitchMode (4, true, true, "Bipolar switch: ON maps follower output around zero for positive and negative modulation.");
                    minSlider.setTooltip ("Band is used by Band Energy only: 0 Low, 1 Mid, 2 High. Other audio-reactive sources ignore it.");
                    maxSlider.setTooltip ("Bipolar converts 0..1 follower output into -1..1 modulation.");
                    curveSlider.setTooltip ("Sensitivity is stored but edited through defaults for now; use Threshold and Depth first.");
                    editorHint.setText ("Audio-reactive Mod blocks follow the previous processed audio buffer. Route them to any parameter with + Mod for envelope, peak/RMS, transient, centroid, band-energy, or gate motion.",
                                        juce::dontSendNotification);
                }
                else
                {
                    amountSlider.setEnabled (false);
                    rateSlider.setEnabled (false);
                    minSlider.setEnabled (false);
                    valueSlider.setValue (get ("value", 0.5f), juce::dontSendNotification);
                    amountSlider.setTooltip ("Disabled for Macro blocks. Macro amount is set by Macro assignments.");
                    rateSlider.setTooltip ("Disabled for Macro blocks. Macro sources are static unless automated/modulated.");
                    minSlider.setTooltip ("Disabled for Macro blocks. Use Macro min/max for target range.");
                    maxSlider.setTooltip ("Disabled for Macro blocks. Use Macro assignments for ranges.");
                    curveSlider.setTooltip ("Disabled for Macro blocks. Use Macro assignment curve.");
                }
            }
            else if (block.section == "out")
            {
                if (block.type.containsIgnoreCase ("utility"))
                {
                    valueLabel.setText ("OUT dB", juce::dontSendNotification);
                    rateLabel.setText ("IN dB", juce::dontSendNotification);
                    amountLabel.setText ("WIDTH", juce::dontSendNotification);
                    minLabel.setText ("MONO", juce::dontSendNotification);
                    maxLabel.setText ("LIMIT", juce::dontSendNotification);
                    curveLabel.setText ("CEIL dB", juce::dontSendNotification);
                    valueSlider.setRange (-48.0, 24.0, 0.1);
                    rateSlider.setRange (-48.0, 24.0, 0.1);
                    amountSlider.setRange (0.0, 2.0, 0.001);
                    minSlider.setRange (0.0, 1.0, 0.001);
                    maxSlider.setRange (0.0, 1.0, 0.001);
                    curveSlider.setRange (-24.0, 0.0, 0.1);
                    valueSlider.setValue (get ("outputGainDb", 0.0f), juce::dontSendNotification);
                    rateSlider.setValue (get ("inputTrimDb", 0.0f), juce::dontSendNotification);
                    amountSlider.setValue (get ("stereoWidth", 1.0f), juce::dontSendNotification);
                    minSlider.setValue (get ("monoMaker", 0.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("outputLimiter", 1.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("outputCeilingDb", -0.5f), juce::dontSendNotification);
                    setGraphControlSwitchMode (4, true, true, "Limiter switch: ON enables the output limiter before export/player playback.");
                }
                else
                {
                    amountLabel.setText ("PAN", juce::dontSendNotification);
                    valueLabel.setText ("VOLUME", juce::dontSendNotification);
                    minLabel.setText ("BPM SYNC", juce::dontSendNotification);
                    maxLabel.setText ("RETRIG", juce::dontSendNotification);
                    curveLabel.setText ("MIX", juce::dontSendNotification);
                    amountSlider.setRange (0.0, 1.0, 0.001);
                    curveSlider.setRange (0.0, 1.0, 0.001);
                    valueSlider.setValue (get ("volume", 0.75f), juce::dontSendNotification);
                    amountSlider.setValue (get ("pan", 0.5f), juce::dontSendNotification);
                    minSlider.setValue (get ("bpmSync", 1.0f), juce::dontSendNotification);
                    maxSlider.setValue (get ("retrigger", 1.0f), juce::dontSendNotification);
                    curveSlider.setValue (get ("mix", 1.0f), juce::dontSendNotification);
                    setGraphControlSwitchMode (3, true, true, "Global BPM Sync switch: ON allows tempo-synced blocks to follow host/project BPM.");
                    setGraphControlSwitchMode (4, true, true, "Retrigger switch: ON restarts triggered sound/animation behavior on new notes.");
                }
            }
        }
        else if (selectedGraphKind == 2 && selectedGraphIndex < (int) project.getDspGraph().macros.size())
        {
            auto& macro = project.getDspGraph().macros[(size_t) selectedGraphIndex];
            editorHint.setText ("Macro maps one source to a target range. Use min/max/curve to make one knob create complex multi-parameter motion.",
                                juce::dontSendNotification);
            amountLabel.setText ("AMOUNT", juce::dontSendNotification);
            rateLabel.setText ("SOURCE", juce::dontSendNotification);
            valueLabel.setText ("TARGET", juce::dontSendNotification);
            minLabel.setText ("TARGET MIN", juce::dontSendNotification);
            maxLabel.setText ("TARGET MAX", juce::dontSendNotification);
            selectBySuffix (sourceBox, macro.macroId);
            selectBySuffix (targetBox, macro.targetId);
            auto blockExists = [this] (const juce::String& id)
            {
                for (const auto& block : project.getDspGraph().blocks)
                    if (block.id == id)
                        return true;
                return false;
            };
            const bool macroSourceExists = project.getParameters().find (macro.macroId) != nullptr
                || blockExists (macro.macroId);
            const bool macroTargetExists = project.getParameters().find (macro.targetId) != nullptr
                || blockExists (macro.targetId);
            if (! macroTargetExists)
                editorHint.setText ("Not connected: this Macro has no valid target, so it cannot change sound. Choose a Sound Target from the filtered target menu.",
                                    juce::dontSendNotification);
            else if (! macroSourceExists)
                editorHint.setText ("Source not connected: this Macro currently outputs a fixed midpoint. Choose a Mod/MIDI source or DSP block to make it move.",
                                    juce::dontSendNotification);
            if (const auto* def = project.getParameters().find (macro.targetId))
            {
                minSlider.setValue (juce::jmap (macro.targetMin, def->min, def->max, 0.0f, 1.0f), juce::dontSendNotification);
                maxSlider.setValue (juce::jmap (macro.targetMax, def->min, def->max, 0.0f, 1.0f), juce::dontSendNotification);
            }
            curveSlider.setValue (macro.curve, juce::dontSendNotification);
        }
        else if (selectedGraphKind == 3 && selectedGraphIndex < (int) project.getDspGraph().modulation.size())
        {
            auto& route = project.getDspGraph().modulation[(size_t) selectedGraphIndex];
            enableGraphItemButton.setToggleState (route.enabled, juce::dontSendNotification);
            editorHint.setText ("Mod route: source creates movement, target receives it. Depth controls range; source rate/sync edit the LFO/random source when available.",
                                juce::dontSendNotification);
            amountLabel.setText ("DEPTH", juce::dontSendNotification);
            rateLabel.setText ("SRC RATE", juce::dontSendNotification);
            minLabel.setText ("SRC SYNC", juce::dontSendNotification);
            selectBySuffix (sourceBox, route.sourceId);
            selectBySuffix (targetBox, route.targetId);
            amountSlider.setValue (route.amount, juce::dontSendNotification);
            auto blockExists = [this] (const juce::String& id)
            {
                for (const auto& block : project.getDspGraph().blocks)
                    if (block.id == id)
                        return true;
                return false;
            };
            const bool routeTargetExists = project.getParameters().find (route.targetId) != nullptr
                || blockExists (route.targetId);
            if (! routeTargetExists)
                editorHint.setText ("Not connected: this Mod Route has no valid target, so it cannot change sound. Choose a Sound Target from the filtered target menu.",
                                    juce::dontSendNotification);
            bool sourceFound = false;
            for (const auto& block : project.getDspGraph().blocks)
            {
                if (block.id != route.sourceId)
                    continue;
                sourceFound = true;
                auto get = [&] (const juce::String& key, float fallback)
                {
                    auto it = block.values.find (key);
                    return it == block.values.end() ? fallback : it->second;
                };
                rateSlider.setValue (get ("rate", 1.0f), juce::dontSendNotification);
                minSlider.setValue (get ("sync", 0.0f), juce::dontSendNotification);
                const bool sourceSupportsSync = block.values.count ("sync") != 0
                    && ! isAudioReactiveModType (block.type)
                    && ! block.type.containsIgnoreCase ("random");
                if (sourceSupportsSync)
                    setGraphControlSwitchMode (3, true, true, "Source Sync switch: ON tempo-syncs this modulation source.");
                else
                {
                    minLabel.setText ("-", juce::dontSendNotification);
                    minSlider.setEnabled (false);
                    minSlider.setTooltip ("Disabled: the selected modulation source is free-running or audio-reactive, so sync is not used.");
                }
                break;
            }
            if (! sourceFound)
            {
                rateSlider.setEnabled (false);
                minSlider.setEnabled (false);
                rateSlider.setTooltip ("Disabled: choose a valid source block or Mod/MIDI parameter before editing source rate.");
                minSlider.setTooltip ("Disabled: choose a valid source block that supports sync before editing source sync.");
                editorHint.setText ("Not connected: this Mod Route has no valid source. Choose a Mod block/source and a target before expecting sound changes.",
                                    juce::dontSendNotification);
            }
        }
        else if (selectedGraphKind == 4 && selectedGraphIndex < (int) project.getDspGraph().automation.size())
        {
            auto& lane = project.getDspGraph().automation[(size_t) selectedGraphIndex];
            editorHint.setText ("Automation lane writes a looping curve into a parameter or block. Rate controls speed; sync locks to tempo.",
                                juce::dontSendNotification);
            rateLabel.setText ("RATE", juce::dontSendNotification);
            valueLabel.setText ("SHAPE", juce::dontSendNotification);
            minLabel.setText ("SYNC", juce::dontSendNotification);
            selectBySuffix (targetBox, lane.targetId);
            if (project.getParameters().find (lane.targetId) == nullptr)
                editorHint.setText ("Not connected: this Automation lane has no valid parameter target. Choose a Sound Target before expecting sound changes.",
                                    juce::dontSendNotification);
            rateSlider.setValue (lane.rate, juce::dontSendNotification);
            valueSlider.setValue (lane.points.empty() ? 0.5f : lane.points.front(), juce::dontSendNotification);
            minSlider.setValue (lane.syncToTempo ? 1.0 : 0.0, juce::dontSendNotification);
            setGraphControlSwitchMode (3, true, true, "Automation Sync switch: ON locks this automation lane to tempo.");
        }
        else
        {
            editorHint.setText ("Click a block card above or choose an item here. Delete removes the selected graph item.",
                                juce::dontSendNotification);
        }

        syncGraphControlLabelTooltips();
        refreshEqActionButtons();
        surgicalEqPanel.repaint();
        syncingEditor = false;
    }

    void DspPage::applyGraphEditorChange()
    {
        if (syncingEditor || selectedGraphKind <= 0 || selectedGraphIndex < 0)
            return;

        auto extractId = [] (const juce::String& text)
        {
            const auto open = text.lastIndexOfChar ('(');
            const auto close = text.lastIndexOfChar (')');
            return open >= 0 && close > open ? text.substring (open + 1, close) : text;
        };
        auto selectedComboId = [&extractId] (const juce::ComboBox& box)
        {
            return box.getSelectedId() > 0 ? extractId (box.getText()) : juce::String();
        };

        if (selectedGraphKind == 1 && selectedGraphIndex < (int) project.getDspGraph().blocks.size())
        {
            auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
            const auto previousType = block.type;
            if (typeBox.getText().isNotEmpty())
                block.type = typeBox.getText();
            const bool typeChanged = block.type != previousType;
            applyBlockTypeDefaults (block, previousType);
            if (! typeChanged && block.section == "mod")
            {
                const auto targetId = selectedComboId (targetBox);
                if (targetId.isNotEmpty())
                    block.targetId = targetId;
            }
            block.values["rate"] = (float) rateSlider.getValue();
            block.values["value"] = (float) valueSlider.getValue();
            block.values["amount"] = (float) amountSlider.getValue();
            if (block.section == "filter")
            {
                if (block.type.containsIgnoreCase ("eq"))
                {
                    const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
                    block.targetId = "eqBand" + juce::String (band) + "Freq";
                    block.values["eqBand"] = (float) band;
                    block.values["eqOn"] = 1.0f;
                    block.values["eqFreq"] = juce::jlimit (20.0f, 20000.0f, (float) valueSlider.getValue());
                    block.values["eqGainDb"] = juce::jlimit (-24.0f, 24.0f, (float) amountSlider.getValue());
                    block.values["eqQ"] = juce::jlimit (0.10f, 18.0f, (float) rateSlider.getValue());
                    block.values["eqType"] = juce::jlimit (0.0f, 5.0f, (float) minSlider.getValue());
                    block.values["eqMode"] = juce::jlimit (0.0f, 4.0f, (float) maxSlider.getValue());
                    block.values["eqMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                    block.values.erase ("cutoff");
                    block.values.erase ("resonance");
                    block.values.erase ("lfoAmount");
                    block.values.erase ("rate");
                    block.values.erase ("sync");
                }
                else
                {
                    block.values["cutoff"] = (float) valueSlider.getValue();
                    block.values["resonance"] = (float) amountSlider.getValue();
                    block.values["lfoAmount"] = (float) maxSlider.getValue();
                }
            }
            else if (block.section == "source")
            {
                if (project.getEngineType() == "fx")
                {
                    block.targetId = "drive";
                    block.values["drive"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["mix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else
                {
                    const bool noiseBlock = block.type.containsIgnoreCase ("noise");
                    const bool wavetableBlock = block.type.containsIgnoreCase ("wavetable");
                    if (wavetableBlock)
                    {
                        block.targetId = "wtPosition";
                        block.values["wtEnabled"] = 1.0f;
                        block.values["wtPosition"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                        block.values["wtMorph"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                        block.values["wtWarp"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                        block.values["wtTable"] = juce::jlimit (0.0f, 8.0f, (float) minSlider.getValue());
                        block.values["wtUnison"] = juce::jlimit (1.0f, 8.0f, (float) maxSlider.getValue());
                        block.values["wtLevel"] = juce::jlimit (0.0f, 1.5f, (float) curveSlider.getValue());
                        block.values.erase ("volume");
                        block.values.erase ("oscType");
                        block.values.erase ("osc2Type");
                        block.values.erase ("oscBlend");
                        block.values.erase ("subBlend");
                        block.values.erase ("noiseBlend");
                        block.values.erase ("pan");
                    }
                    else if (noiseBlock)
                    {
                        block.values.erase ("volume");
                        block.values["noiseBlend"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    }
                    else
                    {
                        block.values["volume"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    }
                    if (project.getEngineType() == "sample")
                    {
                        block.values["pan"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                        block.values["sampleStart"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                        block.values["sampleLength"] = juce::jlimit (0.01f, 1.0f, (float) minSlider.getValue());
                        block.values["sampleSlice"] = juce::jlimit (0.0f, 63.0f, (float) maxSlider.getValue());
                        block.values["sampleSliceCount"] = juce::jlimit (1.0f, 64.0f, (float) curveSlider.getValue());
                    }
                    else
                    {
                        block.values.erase ("pan");
                        block.values["detune"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                        block.values["oscBlend"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                        block.values["subBlend"] = juce::jlimit (0.0f, 1.0f, (float) minSlider.getValue());
                        block.values["oscType"] = juce::jlimit (0.0f, 1.0f, (float) maxSlider.getValue());
                        block.values["osc2Type"] = juce::jlimit (0.0f, 1.0f, 1.0f - (float) maxSlider.getValue());
                        block.values["osc2Detune"] = juce::jlimit (0.0f, 1.0f, 0.50f + ((float) amountSlider.getValue() - 0.50f) * 0.5f + 0.035f);
                        if (! noiseBlock)
                            block.values["noiseBlend"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                        block.values["mix"] = 1.0f;
                    }
                }
            }
            else if (block.section == "fx")
            {
                if (block.type.containsIgnoreCase ("multiTap"))
                {
                    block.targetId = "multiTapMix";
                    block.values["multiTapMix"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["multiTapTime"] = juce::jlimit (0.02f, 2.0f, (float) rateSlider.getValue());
                    block.values["multiTapSpread"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["multiTapFeedback"] = juce::jlimit (0.0f, 0.92f, (float) maxSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("delay"))
                {
                    block.targetId = "delayMix";
                    block.values["delayMix"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["delayFeedback"] = juce::jlimit (0.0f, 1.0f, (float) maxSlider.getValue());
                    if (minSlider.getValue() >= 0.5)
                        block.values["rate"] = juce::jlimit (0.0625f, 8.0f, (float) rateSlider.getValue());
                    else
                        block.values["delayTime"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue() / 2.0f);
                    block.values["sync"] = minSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                }
                else if (block.type.containsIgnoreCase ("dist") || block.type.containsIgnoreCase ("shape") || block.type.containsIgnoreCase ("crush"))
                {
                    block.targetId = "drive";
                    block.values["drive"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["mix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("dynamics") || block.type.containsIgnoreCase ("compress"))
                {
                    block.targetId = "dynMix";
                    block.values["dynThresholdDb"] = juce::jlimit (-80.0f, 12.0f, (float) amountSlider.getValue());
                    block.values["dynRatio"] = juce::jlimit (1.0f, 40.0f, (float) rateSlider.getValue());
                    block.values["dynAttackMs"] = juce::jlimit (0.1f, 250.0f, (float) valueSlider.getValue());
                    block.values["dynReleaseMs"] = juce::jlimit (5.0f, 1000.0f, (float) minSlider.getValue());
                    block.values["dynMakeupDb"] = juce::jlimit (-24.0f, 24.0f, (float) maxSlider.getValue());
                    block.values["dynMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("chorus"))
                {
                    block.targetId = "chorusMix";
                    block.values["chorusDepth"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["chorusRate"] = juce::jlimit (0.01f, 20.0f, (float) rateSlider.getValue());
                    block.values["chorusFeedback"] = juce::jlimit (-0.95f, 0.95f, (float) valueSlider.getValue());
                    block.values["chorusMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("phaser"))
                {
                    block.targetId = "phaserMix";
                    block.values["phaserDepth"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["phaserRate"] = juce::jlimit (0.01f, 20.0f, (float) rateSlider.getValue());
                    block.values["phaserFeedback"] = juce::jlimit (-0.95f, 0.95f, (float) valueSlider.getValue());
                    block.values["phaserMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("comb"))
                {
                    block.targetId = "combMix";
                    block.values["combFreq"] = juce::jlimit (20.0f, 8000.0f, (float) amountSlider.getValue());
                    block.values["combFeedback"] = juce::jlimit (-0.95f, 0.95f, (float) rateSlider.getValue());
                    block.values["combMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("resonator"))
                {
                    block.targetId = "resonatorMix";
                    block.values["resonatorFreq"] = juce::jlimit (20.0f, 16000.0f, (float) amountSlider.getValue());
                    block.values["resonatorQ"] = juce::jlimit (0.05f, 18.0f, (float) rateSlider.getValue());
                    block.values["resonatorMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("convolution"))
                {
                    block.targetId = "convolutionMix";
                    block.values["convolutionSize"] = juce::jlimit (1.0f, 8.0f, (float) amountSlider.getValue());
                    block.values["convolutionMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("spectral"))
                {
                    block.targetId = "spectralMix";
                    block.values["spectralTilt"] = juce::jlimit (-1.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["spectralMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("tape"))
                {
                    block.targetId = "tapeMix";
                    block.values["tapeDrive"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["tapeFlutter"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                    block.values["tapeTone"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["tapeMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("vinyl") || block.type.containsIgnoreCase ("oldSchool"))
                {
                    block.targetId = "vinylMix";
                    block.values["vinylAge"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["vinylDust"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                    block.values["vinylWarp"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["vinylMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("lofi"))
                {
                    block.targetId = "lofiMix";
                    block.values["lofiBits"] = juce::jlimit (4.0f, 16.0f, (float) amountSlider.getValue());
                    block.values["lofiRate"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                    block.values["lofiMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else if (block.type.containsIgnoreCase ("vocal") || block.type.containsIgnoreCase ("formant"))
                {
                    block.targetId = "vocalMix";
                    block.values["vocalFormant"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["vocalBody"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                    block.values["vocalMix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
                else
                {
                    block.targetId = "reverbMix";
                    block.values["reverbMix"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                    block.values["mix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
            }
            else if (block.section == "out")
            {
                if (block.type.containsIgnoreCase ("utility"))
                {
                    block.targetId = "outputGainDb";
                    block.values["outputGainDb"] = juce::jlimit (-48.0f, 24.0f, (float) valueSlider.getValue());
                    block.values["inputTrimDb"] = juce::jlimit (-48.0f, 24.0f, (float) rateSlider.getValue());
                    block.values["stereoWidth"] = juce::jlimit (0.0f, 2.0f, (float) amountSlider.getValue());
                    block.values["monoMaker"] = juce::jlimit (0.0f, 1.0f, (float) minSlider.getValue());
                    block.values["outputLimiter"] = maxSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                    block.values["outputCeilingDb"] = juce::jlimit (-24.0f, 0.0f, (float) curveSlider.getValue());
                }
                else
                {
                    block.values["volume"] = (float) valueSlider.getValue();
                    block.values["pan"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["bpmSync"] = minSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                    block.values["retrigger"] = maxSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                    block.values["mix"] = juce::jlimit (0.0f, 1.0f, (float) curveSlider.getValue());
                }
            }
            else if (block.section == "amp")
            {
                block.values["attack"] = (float) valueSlider.getValue();
                block.values["decay"] = juce::jlimit (0.0f, 1.0f, (float) rateSlider.getValue());
                block.values["sustain"] = juce::jlimit (0.0f, 1.0f, (float) minSlider.getValue());
                block.values["release"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
            }
            else if (block.section == "mod")
            {
                if (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi"))
                {
                    block.values["amount"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["rate"] = juce::jlimit (0.0625f, 16.0f, (float) rateSlider.getValue());
                    block.values["arpGate"] = juce::jlimit (0.05f, 1.0f, (float) valueSlider.getValue());
                    block.values["sync"] = minSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                    block.values["arpSteps"] = juce::jlimit (1.0f, 16.0f, (float) maxSlider.getValue());
                    block.values["arpPattern"] = juce::jlimit (0.0f, 7.0f, (float) curveSlider.getValue());
                    block.values.erase ("value");
                }
                else if (isAudioReactiveModType (block.type))
                {
                    block.values["amount"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["smoothingMs"] = juce::jlimit (1.0f, 500.0f, (float) rateSlider.getValue());
                    block.values["thresholdDb"] = juce::jlimit (-90.0f, 0.0f, (float) valueSlider.getValue());
                    block.values["band"] = juce::jlimit (0.0f, 2.0f, (float) minSlider.getValue());
                    block.values["bipolar"] = maxSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                    block.values["sensitivity"] = juce::jlimit (0.25f, 4.0f, valueForBlockKey (block, "sensitivity", 1.0f));
                    block.values.erase ("sync");
                    block.values.erase ("rate");
                    block.values.erase ("value");
                }
                else if (block.type.containsIgnoreCase ("lfo"))
                {
                    block.values["amount"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["rate"] = juce::jlimit (0.01f, 20.0f, (float) rateSlider.getValue());
                    block.values["sync"] = minSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                }
                else if (block.type.containsIgnoreCase ("random"))
                {
                    block.values["amount"] = juce::jlimit (0.0f, 1.0f, (float) amountSlider.getValue());
                    block.values["rate"] = juce::jlimit (0.01f, 40.0f, (float) rateSlider.getValue());
                    block.values["sync"] = 0.0f;
                }
                else
                {
                    block.values["value"] = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue());
                }
            }
            if (! (block.section == "filter" && block.type.containsIgnoreCase ("eq"))
                && (block.section != "fx" || block.type.containsIgnoreCase ("delay")))
                block.values["sync"] = (float) minSlider.getValue() >= 0.5f ? 1.0f : 0.0f;
        }
        else if (selectedGraphKind == 2 && selectedGraphIndex < (int) project.getDspGraph().macros.size())
        {
            auto& macro = project.getDspGraph().macros[(size_t) selectedGraphIndex];
            if (const auto sourceId = selectedComboId (sourceBox); sourceId.isNotEmpty())
                macro.macroId = sourceId;
            if (const auto targetId = selectedComboId (targetBox); targetId.isNotEmpty())
                macro.targetId = targetId;
            if (const auto* def = project.getParameters().find (macro.targetId))
            {
                macro.targetMin = juce::jmap ((float) minSlider.getValue(), 0.0f, 1.0f, def->min, def->max);
                macro.targetMax = juce::jmap ((float) maxSlider.getValue(), 0.0f, 1.0f, def->min, def->max);
            }
            macro.curve = (float) curveSlider.getValue();
        }
        else if (selectedGraphKind == 3 && selectedGraphIndex < (int) project.getDspGraph().modulation.size())
        {
            auto& route = project.getDspGraph().modulation[(size_t) selectedGraphIndex];
            if (const auto sourceId = selectedComboId (sourceBox); sourceId.isNotEmpty())
                route.sourceId = sourceId;
            if (const auto targetId = selectedComboId (targetBox); targetId.isNotEmpty())
                route.targetId = targetId;
            route.amount = (float) amountSlider.getValue();
            for (auto& block : project.getDspGraph().blocks)
            {
                if (block.id != route.sourceId)
                    continue;
                if (isAudioReactiveModType (block.type))
                {
                    block.values["smoothingMs"] = juce::jlimit (1.0f, 500.0f, (float) rateSlider.getValue());
                    block.values.erase ("rate");
                    block.values.erase ("sync");
                }
                else
                {
                    block.values["rate"] = (float) rateSlider.getValue();
                    block.values["sync"] = minSlider.getValue() >= 0.5 ? 1.0f : 0.0f;
                }
                break;
            }
        }
        else if (selectedGraphKind == 4 && selectedGraphIndex < (int) project.getDspGraph().automation.size())
        {
            auto& lane = project.getDspGraph().automation[(size_t) selectedGraphIndex];
            if (const auto targetId = selectedComboId (targetBox); targetId.isNotEmpty())
                lane.targetId = targetId;
            lane.rate = (float) rateSlider.getValue();
            lane.syncToTempo = minSlider.getValue() >= 0.5;
            lane.points = { 0.0f, (float) valueSlider.getValue(), 1.0f, 1.0f - (float) valueSlider.getValue(), 0.0f };
        }

        markGraphEdited();
        project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        refreshBuilderPanel();
        surgicalEqPanel.repaint();
        refreshEqActionButtons();
    }

    void DspPage::toggleSelectedGraphItemEnabled()
    {
        if (syncingEditor || selectedGraphIndex < 0)
            return;

        const bool enabled = enableGraphItemButton.getToggleState();
        if (selectedGraphKind == 1 && selectedGraphIndex < (int) project.getDspGraph().blocks.size())
        {
            project.getDspGraph().blocks[(size_t) selectedGraphIndex].enabled = enabled;
        }
        else if (selectedGraphKind == 3 && selectedGraphIndex < (int) project.getDspGraph().modulation.size())
        {
            project.getDspGraph().modulation[(size_t) selectedGraphIndex].enabled = enabled;
        }
        else
        {
            return;
        }

        markGraphEdited();
        project.notifyChanged();
        refreshBuilderPanel();
        syncGraphEditor();
        surgicalEqPanel.repaint();
    }

    void DspPage::applyBlockTypeDefaults (DspBlock& block, const juce::String& previousType)
    {
        if (block.type == previousType)
            return;

        auto putIfMissing = [&] (const juce::String& key, float value)
        {
            if (block.values.find (key) == block.values.end())
                block.values[key] = value;
        };
        auto get = [&] (const juce::String& key, float fallback)
        {
            const auto it = block.values.find (key);
            return it == block.values.end() ? fallback : it->second;
        };
        auto keepOnly = [&] (std::initializer_list<const char*> keys, bool keepWavetableShapeData = false)
        {
            for (auto it = block.values.begin(); it != block.values.end();)
            {
                bool keep = false;
                if (it->first == "bank")
                    keep = true;
                if (keepWavetableShapeData
                    && (it->first.startsWith ("wtShape")
                        || it->first.startsWith ("wtFrame")))
                    keep = true;
                for (auto* key : keys)
                    if (it->first == key)
                        keep = true;
                if (keep)
                    ++it;
                else
                    it = block.values.erase (it);
            }
        };

        if (block.section == "source")
        {
            if (project.getEngineType() == "fx" || block.type.containsIgnoreCase ("drive") || block.type.containsIgnoreCase ("input"))
            {
                block.targetId = "drive";
                putIfMissing ("drive", 0.12f);
                putIfMissing ("mix", 1.0f);
                return;
            }
            if (project.getEngineType() == "sample")
            {
                block.targetId = "volume";
                keepOnly ({ "volume", "pan", "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount",
                            "samplePitch", "sampleReverse", "granularOn", "granularDensity", "granularSizeMs",
                            "granularSizeRandom", "granularSpread", "granularScan", "granularPitchSpread",
                            "granularPanSpread", "granularReverse", "granularTexture", "granularMaxGrains",
                            "granularDirection", "granularWindow", "granularFreeze", "value", "amount", "rate" });
                putIfMissing ("volume", 0.85f);
                putIfMissing ("pan", 0.50f);
                putIfMissing ("sampleStart", 0.0f);
                putIfMissing ("sampleLength", 1.0f);
                putIfMissing ("sampleSlice", 0.0f);
                putIfMissing ("sampleSliceCount", 1.0f);
                putIfMissing ("samplePitch", 0.0f);
                putIfMissing ("sampleReverse", 0.0f);
                putIfMissing ("granularOn", block.type.containsIgnoreCase ("granular") ? 1.0f : 0.0f);
                putIfMissing ("granularDensity", 24.0f);
                putIfMissing ("granularSizeMs", 90.0f);
                putIfMissing ("granularSizeRandom", 0.25f);
                putIfMissing ("granularSpread", 0.18f);
                putIfMissing ("granularScan", 0.0f);
                putIfMissing ("granularPitchSpread", 0.0f);
                putIfMissing ("granularPanSpread", 0.45f);
                putIfMissing ("granularReverse", 0.0f);
                putIfMissing ("granularTexture", 0.20f);
                putIfMissing ("granularMaxGrains", 16.0f);
                putIfMissing ("granularDirection", 3.0f);
                putIfMissing ("granularWindow", 0.0f);
                putIfMissing ("granularFreeze", 0.0f);
                return;
            }
            if (block.type.containsIgnoreCase ("wavetable"))
            {
                block.targetId = "wtPosition";
                keepOnly ({ "wtEnabled", "wtTable", "wtPosition", "wtMorph", "wtWarp", "wtFold",
                            "wtUnison", "wtDetune", "wtSpread", "wtLevel", "wtBend", "wtSyncRatio",
                            "wtSpectralTilt", "wtPhaseMode", "wtFramePosition", "wtFrameCount",
                            "value", "amount", "rate" }, true);
                putIfMissing ("wtEnabled", 1.0f);
                putIfMissing ("wtTable", 0.0f);
                putIfMissing ("wtPosition", 0.0f);
                putIfMissing ("wtMorph", 0.0f);
                putIfMissing ("wtWarp", 0.0f);
                putIfMissing ("wtFold", 0.0f);
                putIfMissing ("wtUnison", 1.0f);
                putIfMissing ("wtDetune", 12.0f);
                putIfMissing ("wtSpread", 0.0f);
                putIfMissing ("wtLevel", 1.0f);
                putIfMissing ("wtBend", 0.0f);
                putIfMissing ("wtSyncRatio", 1.0f);
                putIfMissing ("wtSpectralTilt", 0.0f);
                putIfMissing ("wtPhaseMode", 0.0f);
                putIfMissing ("wtFramePosition", 0.0f);
                putIfMissing ("wtFrameCount", 1.0f);
                ensureWavetableShapeDefaults (block);
                return;
            }
            block.targetId = block.type.containsIgnoreCase ("noise") ? "noiseBlend" : "oscBlend";
            putIfMissing ("oscType", block.type.containsIgnoreCase ("noise") ? 0.75f : 0.25f);
            putIfMissing ("osc2Type", block.type.containsIgnoreCase ("noise") ? 0.25f : 0.75f);
            putIfMissing ("oscBlend", block.type.containsIgnoreCase ("blend") ? 0.45f : 0.0f);
            if (! block.type.containsIgnoreCase ("noise"))
                putIfMissing ("volume", 0.75f);
            putIfMissing ("detune", 0.50f);
            putIfMissing ("osc2Detune", 0.535f);
            putIfMissing ("subBlend", block.type.containsIgnoreCase ("sub") ? 0.35f : 0.0f);
            putIfMissing ("noiseBlend", block.type.containsIgnoreCase ("noise") ? 0.18f : 0.0f);
            putIfMissing ("octave", 0.50f);
        }
        else if (block.section == "filter")
        {
            if (block.type.containsIgnoreCase ("eq"))
            {
                keepOnly ({ "eqBand", "eqOn", "eqType", "eqMode", "eqFreq", "eqGainDb", "eqQ", "eqMix", "eqSolo", "eqOutputTrimDb",
                            "eqDynMode", "eqDynThresholdDb", "eqDynRangeDb", "eqDynAttackMs", "eqDynReleaseMs" });
                const int band = juce::jlimit (1, 8, juce::roundToInt (get ("eqBand", 1.0f)));
                block.targetId = "eqBand" + juce::String (band) + "Freq";
                putIfMissing ("eqBand", (float) band);
                putIfMissing ("eqOn", 1.0f);
                putIfMissing ("eqType", block.type.containsIgnoreCase ("notch") ? 5.0f : 0.0f);
                putIfMissing ("eqMode", 0.0f);
                putIfMissing ("eqFreq", 1000.0f);
                putIfMissing ("eqGainDb", 0.0f);
                putIfMissing ("eqQ", 1.0f);
                putIfMissing ("eqMix", 1.0f);
                putIfMissing ("eqSolo", 0.0f);
                putIfMissing ("eqDynMode", 0.0f);
                putIfMissing ("eqDynThresholdDb", -24.0f);
                putIfMissing ("eqDynRangeDb", 0.0f);
                putIfMissing ("eqDynAttackMs", 10.0f);
                putIfMissing ("eqDynReleaseMs", 120.0f);
            }
            else
            {
                block.targetId = "filterCutoff";
                putIfMissing ("cutoff", block.type.containsIgnoreCase ("formant") ? 0.38f : 0.55f);
                putIfMissing ("resonance", block.type.containsIgnoreCase ("comb") ? 0.55f : 0.20f);
                putIfMissing ("lfoAmount", 0.0f);
                putIfMissing ("rate", 1.0f);
            }
        }
        else if (block.section == "amp")
        {
            block.targetId = "attack";
            putIfMissing ("attack", block.type.containsIgnoreCase ("gate") ? 0.001f : 0.01f);
            putIfMissing ("decay", 0.20f);
            putIfMissing ("sustain", block.type.containsIgnoreCase ("gate") ? 0.45f : 0.80f);
            putIfMissing ("release", block.type.containsIgnoreCase ("gate") ? 0.08f : 0.40f);
        }
        else if (block.section == "mod")
        {
            block.targetId = block.type.containsIgnoreCase ("macro") ? "volume" : "filterCutoff";
            if (block.type.containsIgnoreCase ("arp") || block.type.containsIgnoreCase ("midi"))
            {
                block.targetId = "filterCutoff";
                putIfMissing ("amount", 0.35f);
                putIfMissing ("rate", 1.0f);
                putIfMissing ("sync", 1.0f);
                putIfMissing ("arpGate", 0.55f);
                putIfMissing ("arpSteps", 8.0f);
                putIfMissing ("arpPattern", 0.0f);
                putIfMissing ("arpOctaves", 2.0f);
                putIfMissing ("arpSwing", 0.0f);
                putIfMissing ("mpScaleRoot", 0.0f);
                putIfMissing ("mpScaleType", 1.0f);
                putIfMissing ("mpChordMode", 0.0f);
                putIfMissing ("mpChordSize", 1.0f);
                putIfMissing ("mpChordSpread", 0.0f);
                putIfMissing ("mpProbability", 1.0f);
                putIfMissing ("mpHumanize", 0.0f);
                putIfMissing ("mpLatch", 0.0f);
                putIfMissing ("mpStrum", 0.0f);
                putIfMissing ("mpFlam", 0.0f);
                putIfMissing ("mpEuclideanPulses", 0.0f);
                putIfMissing ("mpEuclideanRotate", 0.0f);
                putIfMissing ("mpSampleControl", 0.0f);
                putIfMissing ("mpSampleSliceCount", 1.0f);
                putIfMissing ("mpSampleStart", 0.0f);
                putIfMissing ("mpSampleLength", 1.0f);
                putIfMissing ("mpSamplePitch", 0.0f);
                putIfMissing ("mpSeed", 12001.0f);
                const float notes[] { 0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 10.0f, 14.0f,
                                      12.0f, 7.0f, 4.0f, 0.0f, 5.0f, 9.0f, 12.0f, 16.0f };
                for (int step = 0; step < 16; ++step)
                {
                    putIfMissing ("arpNote" + juce::String (step), notes[step]);
                    putIfMissing ("mpVelocity" + juce::String (step), 1.0f);
                    putIfMissing ("mpGate" + juce::String (step), 0.55f);
                    putIfMissing ("mpStep" + juce::String (step) + "On", 1.0f);
                    putIfMissing ("mpSampleSlice" + juce::String (step), -1.0f);
                }
            }
            else if (block.type.containsIgnoreCase ("step") || block.type.containsIgnoreCase ("sequencer"))
            {
                keepOnly ({ "amount", "rate", "sync", "steps", "bipolar", "value",
                            "step0", "step1", "step2", "step3", "step4", "step5", "step6", "step7",
                            "step8", "step9", "step10", "step11", "step12", "step13", "step14", "step15" });
                putIfMissing ("amount", 0.30f);
                putIfMissing ("rate", 1.0f);
                putIfMissing ("sync", 1.0f);
                putIfMissing ("steps", 8.0f);
                putIfMissing ("bipolar", 0.0f);
                for (int step = 0; step < 16; ++step)
                    putIfMissing ("step" + juce::String (step), step % 2 == 0 ? 1.0f : 0.0f);
            }
            else if (isAudioReactiveModType (block.type))
            {
                keepOnly ({ "amount", "smoothingMs", "thresholdDb", "band", "bipolar", "sensitivity" });
                putIfMissing ("amount", 0.25f);
                putIfMissing ("smoothingMs", block.type.containsIgnoreCase ("transient") ? 18.0f : 80.0f);
                putIfMissing ("thresholdDb", block.type.containsIgnoreCase ("gate") ? -42.0f : -36.0f);
                putIfMissing ("band", block.type.containsIgnoreCase ("bandEnergy") ? 1.0f : 0.0f);
                putIfMissing ("bipolar", 0.0f);
                putIfMissing ("sensitivity", block.type.containsIgnoreCase ("centroid") ? 1.4f : 1.0f);
            }
            else
            {
                putIfMissing ("rate", block.type.containsIgnoreCase ("random") ? 4.0f : 1.0f);
                putIfMissing ("amount", 0.20f);
                putIfMissing ("sync", block.type.containsIgnoreCase ("step") ? 1.0f : 0.0f);
                putIfMissing ("value", 0.5f);
            }
        }
        else if (block.section == "fx")
        {
            if (block.type.containsIgnoreCase ("multiTap"))
            {
                keepOnly ({ "multiTapMix", "multiTapTime", "multiTapFeedback", "multiTapSpread", "amount", "value" });
                block.targetId = "multiTapMix";
                putIfMissing ("multiTapMix", 0.35f);
                putIfMissing ("multiTapTime", 0.375f);
                putIfMissing ("multiTapFeedback", 0.35f);
                putIfMissing ("multiTapSpread", 0.45f);
            }
            else if (block.type.containsIgnoreCase ("delay"))
            {
                keepOnly ({ "delayMix", "delayFeedback", "delayTime", "rate", "sync", "amount", "value" });
                block.targetId = "delayMix";
                putIfMissing ("delayMix", 0.25f);
                putIfMissing ("delayFeedback", 0.35f);
                putIfMissing ("delayTime", 0.25f);
                putIfMissing ("rate", 1.0f);
                putIfMissing ("sync", 1.0f);
            }
            else if (block.type.containsIgnoreCase ("dist") || block.type.containsIgnoreCase ("shape") || block.type.containsIgnoreCase ("crush"))
            {
                keepOnly ({ "drive", "mix", "amount", "value" });
                block.targetId = "drive";
                putIfMissing ("drive", 0.55f);
                putIfMissing ("mix", 1.0f);
            }
            else if (block.type.containsIgnoreCase ("dynamics") || block.type.containsIgnoreCase ("compress"))
            {
                keepOnly ({ "dynThresholdDb", "dynRatio", "dynAttackMs", "dynReleaseMs", "dynMakeupDb", "dynMix", "amount", "value" });
                block.targetId = "dynMix";
                putIfMissing ("dynThresholdDb", -18.0f);
                putIfMissing ("dynRatio", 2.0f);
                putIfMissing ("dynAttackMs", 10.0f);
                putIfMissing ("dynReleaseMs", 120.0f);
                putIfMissing ("dynMakeupDb", 0.0f);
                putIfMissing ("dynMix", 0.50f);
            }
            else if (block.type.containsIgnoreCase ("chorus"))
            {
                keepOnly ({ "chorusRate", "chorusDepth", "chorusFeedback", "chorusMix", "amount", "value" });
                block.targetId = "chorusMix";
                putIfMissing ("chorusRate", 0.35f);
                putIfMissing ("chorusDepth", 0.35f);
                putIfMissing ("chorusFeedback", 0.0f);
                putIfMissing ("chorusMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("phaser"))
            {
                keepOnly ({ "phaserRate", "phaserDepth", "phaserFeedback", "phaserMix", "amount", "value" });
                block.targetId = "phaserMix";
                putIfMissing ("phaserRate", 0.25f);
                putIfMissing ("phaserDepth", 0.45f);
                putIfMissing ("phaserFeedback", 0.0f);
                putIfMissing ("phaserMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("comb"))
            {
                keepOnly ({ "combFreq", "combFeedback", "combMix", "amount", "value" });
                block.targetId = "combMix";
                putIfMissing ("combFreq", 220.0f);
                putIfMissing ("combFeedback", 0.35f);
                putIfMissing ("combMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("resonator"))
            {
                keepOnly ({ "resonatorFreq", "resonatorQ", "resonatorMix", "amount", "value" });
                block.targetId = "resonatorMix";
                putIfMissing ("resonatorFreq", 440.0f);
                putIfMissing ("resonatorQ", 4.0f);
                putIfMissing ("resonatorMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("convolution"))
            {
                keepOnly ({ "convolutionSize", "convolutionMix", "amount", "value" });
                block.targetId = "convolutionMix";
                putIfMissing ("convolutionSize", 3.0f);
                putIfMissing ("convolutionMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("spectral"))
            {
                keepOnly ({ "spectralTilt", "spectralMix", "amount", "value" });
                block.targetId = "spectralMix";
                putIfMissing ("spectralTilt", 0.0f);
                putIfMissing ("spectralMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("tape"))
            {
                keepOnly ({ "tapeDrive", "tapeTone", "tapeFlutter", "tapeMix", "amount", "value" });
                block.targetId = "tapeMix";
                putIfMissing ("tapeDrive", 0.32f);
                putIfMissing ("tapeTone", 0.58f);
                putIfMissing ("tapeFlutter", 0.12f);
                putIfMissing ("tapeMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("vinyl") || block.type.containsIgnoreCase ("oldSchool"))
            {
                keepOnly ({ "vinylAge", "vinylDust", "vinylWarp", "vinylMix", "amount", "value" });
                block.targetId = "vinylMix";
                putIfMissing ("vinylAge", 0.42f);
                putIfMissing ("vinylDust", 0.10f);
                putIfMissing ("vinylWarp", 0.16f);
                putIfMissing ("vinylMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("lofi"))
            {
                keepOnly ({ "lofiBits", "lofiRate", "lofiMix", "amount", "value" });
                block.targetId = "lofiMix";
                putIfMissing ("lofiBits", 10.0f);
                putIfMissing ("lofiRate", 0.22f);
                putIfMissing ("lofiMix", 0.35f);
            }
            else if (block.type.containsIgnoreCase ("vocal") || block.type.containsIgnoreCase ("formant"))
            {
                keepOnly ({ "vocalFormant", "vocalBody", "vocalMix", "amount", "value" });
                block.targetId = "vocalMix";
                putIfMissing ("vocalFormant", 0.40f);
                putIfMissing ("vocalBody", 0.35f);
                putIfMissing ("vocalMix", 0.35f);
            }
            else
            {
                keepOnly ({ "reverbMix", "mix", "amount", "value" });
                block.targetId = "reverbMix";
                putIfMissing ("reverbMix", block.type.containsIgnoreCase ("reverb") ? 0.35f : 0.18f);
                putIfMissing ("mix", 1.0f);
            }
        }
        else
        {
            if (block.type.containsIgnoreCase ("utility"))
            {
                block.targetId = "outputGainDb";
                putIfMissing ("inputTrimDb", 0.0f);
                putIfMissing ("phaseInvert", 0.0f);
                putIfMissing ("stereoWidth", 1.0f);
                putIfMissing ("monoMaker", 0.0f);
                putIfMissing ("outputGainDb", 0.0f);
                putIfMissing ("outputLimiter", 1.0f);
                putIfMissing ("outputCeilingDb", -0.5f);
            }
            else
            {
                block.targetId = "volume";
                putIfMissing ("volume", 0.85f);
                putIfMissing ("pan", 0.50f);
                putIfMissing ("bpmSync", 1.0f);
                putIfMissing ("retrigger", 1.0f);
                putIfMissing ("mix", 1.0f);
            }
        }
    }

    void DspPage::rebuildVisibility()
    {
        const bool showEasy = ! quickEdit && easyMode;
        const bool showAdvanced = ! quickEdit && ! easyMode;
        easyModeButton.setVisible (! quickEdit);
        advancedModeButton.setVisible (! quickEdit);
        for (auto* tab : { &tabEngine, &tabTone, &tabAmp, &tabMod, &tabFx, &tabOut })
            tab->setVisible (quickEdit || showAdvanced);
        builderPanel.setVisible (showAdvanced);
        formulaPanel.setVisible (showAdvanced);
        for (auto* component : { static_cast<juce::Component*> (&easyTitleLabel),
                                 static_cast<juce::Component*> (&easyHelpLabel),
                                 static_cast<juce::Component*> (&easyThemeBox),
                                 static_cast<juce::Component*> (&easyGenerateButton),
                                 static_cast<juce::Component*> (&easyRandomButton),
                                 static_cast<juce::Component*> (&easyAddToPackToggle),
                                 static_cast<juce::Component*> (&easyExpansionBox),
                                 static_cast<juce::Component*> (&easyPackCreatorButton),
                                 static_cast<juce::Component*> (&easyAdvancedButton),
                                 static_cast<juce::Component*> (&easyRecipeLabel),
                                 static_cast<juce::Component*> (&easyParametersLabel),
                                 static_cast<juce::Component*> (&easyWorkflowLabel) })
            component->setVisible (showEasy);
        sourceMatrix.setVisible (false);
        surgicalEqPanel.setVisible (showAdvanced && currentTab == 1);
        wavetableEditor.setVisible (showAdvanced && currentTab == 0 && project.getEngineType() == "synth");
        modMatrix.setVisible (false);
        const bool showFxTrack = showAdvanced && currentTab == 4;
        for (auto* component : { static_cast<juce::Component*> (&fxTrackImportButton),
                                 static_cast<juce::Component*> (&fxTrackUseMapperButton),
                                 static_cast<juce::Component*> (&fxTrackPlayButton),
                                 static_cast<juce::Component*> (&fxTrackStopButton),
                                 static_cast<juce::Component*> (&fxTrackLoopToggle),
                                 static_cast<juce::Component*> (&fxTrackRetriggerToggle),
                                 static_cast<juce::Component*> (&fxTrackLiveInputToggle),
                                 static_cast<juce::Component*> (&fxTrackSliceBox),
                                 static_cast<juce::Component*> (&fxTrackTriggerBox),
                                 static_cast<juce::Component*> (&fxTrackPrevSliceButton),
                                 static_cast<juce::Component*> (&fxTrackNextSliceButton),
                                 static_cast<juce::Component*> (&fxTrackGainSlider),
                                 static_cast<juce::Component*> (&fxTrackMonitorBox),
                                 static_cast<juce::Component*> (&fxTrackLoopStartSlider),
                                 static_cast<juce::Component*> (&fxTrackLoopEndSlider),
                                 static_cast<juce::Component*> (&fxTrackNameLabel),
                                 static_cast<juce::Component*> (&fxTrackStatusLabel),
                                 static_cast<juce::Component*> (&fxWaveform) })
            component->setVisible (showFxTrack);
        const bool showEqControls = showAdvanced && currentTab == 1;
        for (auto* component : { static_cast<juce::Component*> (&eqAnalyzerToggle),
                                 static_cast<juce::Component*> (&eqAnalyzerFreezeToggle),
                                 static_cast<juce::Component*> (&eqBandCopyButton),
                                 static_cast<juce::Component*> (&eqBandPasteButton),
                                 static_cast<juce::Component*> (&eqBandSaveButton),
                                 static_cast<juce::Component*> (&eqBandInsertButton) })
            component->setVisible (showEqControls);
        const bool showWtControls = showAdvanced && currentTab == 0 && project.getEngineType() == "synth";
        for (auto* component : { static_cast<juce::Component*> (&wtImportButton),
                                 static_cast<juce::Component*> (&wtNormalizeButton),
                                 static_cast<juce::Component*> (&wtSineButton) })
            component->setVisible (showWtControls);
        for (auto* component : { static_cast<juce::Component*> (&editorTitle),
                                 static_cast<juce::Component*> (&editorHint),
                                 static_cast<juce::Component*> (&editorItemBox),
                                 static_cast<juce::Component*> (&typeBox),
                                 static_cast<juce::Component*> (&sourceBox),
                                 static_cast<juce::Component*> (&targetBox),
                                 static_cast<juce::Component*> (&globalPresetBox),
                                 static_cast<juce::Component*> (&sectionPresetBox),
                                 static_cast<juce::Component*> (&deleteGraphItemButton),
                                 static_cast<juce::Component*> (&enableGraphItemButton),
                                 static_cast<juce::Component*> (&amountLabel),
                                 static_cast<juce::Component*> (&rateLabel),
                                 static_cast<juce::Component*> (&valueLabel),
                                 static_cast<juce::Component*> (&minLabel),
                                 static_cast<juce::Component*> (&maxLabel),
                                 static_cast<juce::Component*> (&curveLabel),
                                 static_cast<juce::Component*> (&amountSlider),
                                 static_cast<juce::Component*> (&rateSlider),
                                 static_cast<juce::Component*> (&valueSlider),
                                 static_cast<juce::Component*> (&minSlider),
                                 static_cast<juce::Component*> (&maxSlider),
                                 static_cast<juce::Component*> (&curveSlider) })
            component->setVisible (showAdvanced);
        std::array<juce::Slider*, 6> graphSliders { &amountSlider, &rateSlider, &valueSlider,
                                                    &minSlider, &maxSlider, &curveSlider };
        std::array<juce::ToggleButton*, 6> graphSwitches { &amountSwitch, &rateSwitch, &valueSwitch,
                                                          &minSwitch, &maxSwitch, &curveSwitch };
        for (int i = 0; i < (int) graphSliders.size(); ++i)
        {
            graphSliders[(size_t) i]->setVisible (showAdvanced && ! graphControlIsSwitch[(size_t) i]);
            graphSwitches[(size_t) i]->setVisible (showAdvanced && graphControlIsSwitch[(size_t) i]);
        }
        engineSection.setVisible (quickEdit && currentTab == 0);
        toneSection.setVisible   (quickEdit && currentTab == 1);
        ampSection.setVisible    (quickEdit && currentTab == 2);
        modSection.setVisible    (quickEdit && currentTab == 3);
        fxSection.setVisible     (quickEdit && currentTab == 4);
        outputSection.setVisible (quickEdit && currentTab == 5);
        resized();
        repaint();
    }

    void DspPage::refreshBuilderPanel()
    {
        auto& graph = project.getDspGraph();
        normaliseDspGraphBanks (graph);

        auto sectionId = currentTab == 0 ? "source"
                       : currentTab == 1 ? "filter"
                       : currentTab == 2 ? "amp"
                       : currentTab == 3 ? "mod"
                       : currentTab == 4 ? "fx" : "out";
        juce::StringArray graphCards;
        juce::StringArray graphDescriptions;
        juce::Array<int> graphItemIds;
        juce::Array<bool> graphEnabled;
        juce::Array<int> sectionBankCounts;
        for (int i = 0; i < kSourceMatrixBankCount; ++i)
            sectionBankCounts.add (0);
        int sectionBlockOrdinal = 0;
        for (int i = 0; i < (int) graph.blocks.size(); ++i)
        {
            auto& block = graph.blocks[(size_t) i];
            if (block.section == sectionId)
            {
                const int bank = bankForBlockOrdinal (block, sectionBlockOrdinal);
                if (bank < sectionBankCounts.size())
                    sectionBankCounts.set (bank, sectionBankCounts[bank] + 1);
                ++sectionBlockOrdinal;
                if (bank != currentSectionBank())
                    continue;

                graphCards.add (block.name + "  [" + block.type + "]");
                graphDescriptions.add (blockImpactDescription (block));
                graphItemIds.add (1000 + i + 1);
                graphEnabled.add (block.enabled);
            }
        }
        switch (currentTab)
        {
            case 0:
                builderPanel.setContent ("Source Builder",
                    "Create oscillators, sample layers, noise, blends, unison, routing, and source macros.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
            case 1:
                builderPanel.setContent ("Filter Builder",
                    "Build serial/parallel filters, morphing filter types, keytracking, envelope follow, and cutoff macros.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
            case 2:
                builderPanel.setContent ("Amp Builder",
                    "Shape gain, envelopes, velocity response, layer amp rules, and dynamic macro behavior.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
            case 3:
                builderPanel.setContent ("Modulation Lab",
                    "Design LFOs, custom shapes, step lanes, random/S&H, MIDI CC routing, and deep modulation matrices.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
            case 4:
                builderPanel.setContent ("FX Builder",
                    "Create serial/parallel FX chains with macro-controlled wet/dry, feedback, drive, space, and movement.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
            default:
                builderPanel.setContent ("Output Builder",
                    "Finalize instrument response with output gain, pan, width, limiter, macro polish, and performance routing.",
                    graphCards, graphDescriptions, graphItemIds, graphEnabled);
                break;
        }
        builderPanel.selectedItemId = selectedGraphKind > 0 ? selectedGraphKind * 1000 + selectedGraphIndex + 1 : 0;
        if (builderPanel.selectedItemId != 0 && ! builderPanel.selectedItemIds.contains (builderPanel.selectedItemId))
            builderPanel.selectedItemIds.add (builderPanel.selectedItemId);
        builderPanel.setSectionBankState (! quickEdit,
                                          currentSectionBank(),
                                          sectionBankCounts);
        builderPanel.importSampleButton.setVisible (! quickEdit && currentTab == 4);
        builderPanel.expansionBox.setVisible (! quickEdit);
        builderPanel.packCreatorButton.setVisible (! quickEdit);
        builderPanel.openSectionEditorButton.setVisible (! quickEdit);
        builderPanel.mixerButton.setVisible (! quickEdit);
        builderPanel.addArpButton.setVisible (! quickEdit && currentTab == 3);
        builderPanel.openSectionEditorButton.setButtonText (
            currentTab == 0 && project.getEngineType() == "synth" ? "WT Editor"
          : currentTab == 1 ? "EQ Editor"
          : currentTab == 3 ? "Mod Matrix"
          : currentTab == 4 ? "Sample Player"
          : "Deep Editor");
        builderPanel.savePatchButton.setButtonText ("Save Patch");
        builderPanel.savePatchAsButton.setButtonText ("Save Patch As");
        builderPanel.sendExpansionButton.setButtonText ("Add To Pack");
        builderPanel.resized();
        formulaPanel.repaint();
        sourceMatrix.repaint();
        surgicalEqPanel.repaint();
        wavetableEditor.repaint();
        modMatrix.repaint();
        refreshEqActionButtons();
    }

    void DspPage::addBuilderBlock()
    {
        juce::String sectionId = currentTab == 0 ? "source"
                               : currentTab == 1 ? "filter"
                               : currentTab == 2 ? "amp"
                               : currentTab == 3 ? "mod"
                               : currentTab == 4 ? "fx" : "out";
        juce::String defaultType = currentTab == 0 ? "oscillator"
                                 : currentTab == 1 ? "stateVariable"
                                 : currentTab == 2 ? "envelope"
                                 : currentTab == 3 ? "lfo"
                                 : currentTab == 4 ? "effect" : "output";

        auto& graph = project.getDspGraph();
        normaliseDspGraphSectionBanks (graph, sectionId);
        const int sectionBlockCount = countBlocksInSection (graph, sectionId);
        juce::Array<int> bankCounts;
        for (int i = 0; i < kSourceMatrixBankCount; ++i)
            bankCounts.add (0);
        for (int bank = 0; bank < kSourceMatrixBankCount; ++bank)
            bankCounts.set (bank, countBlocksInSectionBank (graph, sectionId, bank));

        if (sectionBlockCount >= kSourceMatrixBankSize * kSourceMatrixBankCount)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Section Block Limit")
                    .withMessage ("This section is capped at 24 blocks: 4 banks with 6 blocks each.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::InfoIcon),
                nullptr);
            return;
        }

        const int targetBank = currentSectionBank();
        if (targetBank >= bankCounts.size() || bankCounts[targetBank] >= kSourceMatrixBankSize)
        {
            showBlockBankFullAlert (getCurrentPatchSectionLabel(), targetBank);
            editorHint.setText ("Bank " + juce::String (targetBank + 1)
                                + " is full. Select another bank or delete a block before adding more.",
                                juce::dontSendNotification);
            return;
        }

        DspBlock block;
        block.section = sectionId;
        block.type = defaultType;
        block.id = sectionId + "_" + juce::String ((int) graph.blocks.size() + 1);
        block.name = "Block " + juce::String ((int) graph.blocks.size() + 1);
        block.values["bank"] = (float) targetBank;
        if (sectionId == "source") block.name = "OSC / Layer " + juce::String ((int) graph.blocks.size() + 1);
        if (sectionId == "mod")
        {
            block.name = "LFO / Mod " + juce::String ((int) graph.blocks.size() + 1);
            block.values["rate"] = 1.0f;
            block.values["sync"] = 0.0f;
        }
        if (sectionId == "filter") block.values["cutoff"] = 0.5f;
        if (sectionId == "source")
        {
            if (project.getEngineType() == "sample")
            {
                block.type = "sample";
                block.name = "Sample Layer " + juce::String ((int) graph.blocks.size() + 1);
                block.targetId = "volume";
                block.values["volume"] = 0.85f;
                block.values["pan"] = 0.50f;
            }
            else
            {
                block.targetId = "oscBlend";
                block.values["oscType"] = 0.25f;
                block.values["osc2Type"] = 0.75f;
                block.values["oscBlend"] = 0.20f;
                block.values["osc2Detune"] = 0.535f;
                block.values["subBlend"] = 0.0f;
                block.values["noiseBlend"] = 0.0f;
                block.values["volume"] = 0.75f;
                block.values["detune"] = 0.50f;
            }
        }
        if (sectionId == "filter")
        {
            block.targetId = "filterCutoff";
            block.values["resonance"] = 0.25f;
        }
        if (sectionId == "fx")
        {
            block.targetId = "reverbMix";
            block.values["mix"] = 1.0f;
            block.values["reverbMix"] = 0.35f;
            block.values["delayMix"] = 0.18f;
            block.values["delayFeedback"] = 0.35f;
            block.values["delayTime"] = 0.25f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
        }
        if (sectionId == "out")
        {
            block.targetId = "volume";
            block.values["volume"] = 0.75f;
        }
        graph.blocks.push_back (block);
        markGraphEdited();
        selectedGraphKind = 1;
        selectedGraphIndex = (int) graph.blocks.size() - 1;
        project.notifyChanged();
        rebuildGraphEditorItems();
    }

    void DspPage::addBuilderMacro()
    {
        auto ensureMacroParameter = [this] (const juce::String& id, const juce::String& name)
        {
            if (project.getParameters().find (id) != nullptr)
                return;
            ParameterDef def;
            def.id = id;
            def.name = name;
            def.min = 0.0f;
            def.max = 1.0f;
            def.defaultValue = 0.5f;
            project.getParameters().add (def);
            project.getLiveValues().getOrAddRaw (id, def.defaultValue);
        };

        auto& graph = project.getDspGraph();
        normaliseDspGraphSectionBanks (graph, "mod");
        const int targetBank = currentTab == 3 ? currentSectionBank() : 0;
        if (countBlocksInSectionBank (graph, "mod", targetBank) >= kSourceMatrixBankSize)
        {
            showBlockBankFullAlert ("Modulation", targetBank);
            return;
        }

        DspBlock sourceBlock;
        sourceBlock.section = "mod";
        sourceBlock.type = "macro";
        sourceBlock.id = "macro_" + juce::String ((int) graph.macros.size() + 1);
        sourceBlock.name = "Macro Source " + juce::String ((int) graph.macros.size() + 1);
        sourceBlock.targetId = currentTab == 4 ? "reverbMix" : currentTab == 1 ? "filterCutoff" : "volume";
        sourceBlock.values["value"] = 0.5f;
        sourceBlock.values["bank"] = (float) targetBank;
        ensureMacroParameter (sourceBlock.id, sourceBlock.name);
        graph.blocks.push_back (sourceBlock);

        MacroAssignment macro;
        macro.id = "macro_" + juce::String ((int) graph.macros.size() + 1);
        macro.macroId = sourceBlock.id;
        macro.targetId = currentTab == 4 ? "reverbMix" : currentTab == 1 ? "filterCutoff" : "volume";
        macro.targetMin = 0.0f;
        macro.targetMax = 1.0f;
        if (macro.targetId == "filterCutoff")
        {
            macro.targetMin = 200.0f;
            macro.targetMax = 12000.0f;
        }
        graph.macros.push_back (macro);
        markGraphEdited();
        selectedGraphKind = 2;
        selectedGraphIndex = (int) graph.macros.size() - 1;
        project.notifyChanged();
        rebuildGraphEditorItems();
    }

    void DspPage::addBuilderModRoute()
    {
        auto ensureLfoSource = [this]() -> juce::String
        {
            auto& graph = project.getDspGraph();
            for (const auto& block : graph.blocks)
                if (block.type.containsIgnoreCase ("lfo"))
                    return block.id;

            normaliseDspGraphSectionBanks (graph, "mod");
            const int targetBank = currentTab == 3 ? currentSectionBank() : 0;
            if (countBlocksInSectionBank (graph, "mod", targetBank) >= kSourceMatrixBankSize)
            {
                showBlockBankFullAlert ("Modulation", targetBank);
                return {};
            }

            DspBlock lfo;
            lfo.section = "mod";
            lfo.type = "lfo";
            lfo.id = "lfo_" + juce::String ((int) graph.blocks.size() + 1);
            lfo.name = "LFO " + juce::String ((int) graph.blocks.size() + 1);
            lfo.targetId = "filterCutoff";
            lfo.values["rate"] = 1.0f;
            lfo.values["amount"] = 0.2f;
            lfo.values["bank"] = (float) targetBank;
            graph.blocks.push_back (lfo);
            return lfo.id;
        };

        const auto sourceId = ensureLfoSource();
        if (sourceId.isEmpty())
            return;

        ModRoute route;
        route.id = "mod_" + juce::String ((int) project.getDspGraph().modulation.size() + 1);
        route.sourceId = sourceId;
        route.targetId = currentTab == 4 ? "delayMix" : "filterCutoff";
        route.amount = 0.25f;
        project.getDspGraph().modulation.push_back (route);
        markGraphEdited();
        selectedGraphKind = 3;
        selectedGraphIndex = (int) project.getDspGraph().modulation.size() - 1;
        project.notifyChanged();
        rebuildGraphEditorItems();
    }

    void DspPage::addBuilderAutomation()
    {
        auto chooseFirstValidTarget = [this] (std::initializer_list<const char*> candidates)
        {
            for (auto* candidate : candidates)
                if (project.getParameters().find (candidate) != nullptr)
                    return juce::String (candidate);
            return juce::String ("volume");
        };

        AutomationLane lane;
        lane.id = "auto_" + juce::String ((int) project.getDspGraph().automation.size() + 1);
        if (currentTab == 0)
        {
            if (project.getEngineType() == "sample")
                lane.targetId = chooseFirstValidTarget ({ "sampleStart", "sampleSlice", "granularScan", "volume" });
            else if (project.getEngineType() == "fx")
                lane.targetId = chooseFirstValidTarget ({ "drive", "mix", "filterCutoff", "volume" });
            else
                lane.targetId = selectedWavetableBlock() != nullptr
                    ? chooseFirstValidTarget ({ "wtPosition", "wtMorph", "oscBlend", "volume" })
                    : chooseFirstValidTarget ({ "oscBlend", "osc2Detune", "subBlend", "volume" });
        }
        else if (currentTab == 1)
            lane.targetId = chooseFirstValidTarget ({ "filterCutoff", "filterResonance", "eqMix", "volume" });
        else if (currentTab == 2)
            lane.targetId = chooseFirstValidTarget ({ "volume", "attack", "release" });
        else if (currentTab == 3)
            lane.targetId = chooseFirstValidTarget ({ "lfoAmount", "vibratoDepth", "filterCutoff", "volume" });
        else if (currentTab == 4)
            lane.targetId = chooseFirstValidTarget ({ "reverbMix", "delayMix", "drive", "mix" });
        else
            lane.targetId = chooseFirstValidTarget ({ "volume", "pan", "outputGainDb" });
        lane.rate = 1.0f;
        lane.syncToTempo = true;
        lane.points = { 0.0f, 0.35f, 1.0f, 0.65f, 0.0f };
        project.getDspGraph().automation.push_back (lane);
        markGraphEdited();
        selectedGraphKind = 4;
        selectedGraphIndex = (int) project.getDspGraph().automation.size() - 1;
        project.notifyChanged();
        rebuildGraphEditorItems();
        showAutomationEditorPopout (selectedGraphIndex);
    }

    void DspPage::showAutomationEditorPopout (int automationIndex)
    {
        if (automationIndex < 0 || automationIndex >= (int) project.getDspGraph().automation.size())
            return;

        struct CurvePreview final : public juce::Component
        {
            CurvePreview (DspPage& ownerIn, int indexIn) : owner (ownerIn), index (indexIn) {}

            void paint (juce::Graphics& g) override
            {
                g.fillAll (juce::Colours::transparentBlack);
                auto r = getLocalBounds().toFloat().reduced (8.0f);
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r, 8.0f, 1.0f);

                if (index < 0 || index >= (int) owner.project.getDspGraph().automation.size())
                    return;

                const auto& points = owner.project.getDspGraph().automation[(size_t) index].points;
                if (points.size() < 2)
                    return;

                juce::Path path;
                auto graph = r.reduced (14.0f, 12.0f);
                for (int i = 0; i < (int) points.size(); ++i)
                {
                    const auto x = graph.getX() + graph.getWidth() * ((float) i / (float) (points.size() - 1));
                    const auto y = graph.getBottom() - graph.getHeight() * juce::jlimit (0.0f, 1.0f, points[(size_t) i]);
                    if (i == 0) path.startNewSubPath (x, y);
                    else path.lineTo (x, y);
                }
                g.setColour (PatchCraftLookAndFeel::accent());
                g.strokePath (path, juce::PathStrokeType (2.5f));
            }

            DspPage& owner;
            int index = -1;
        };

        struct AutomationEditor final : public juce::Component
        {
            AutomationEditor (DspPage& ownerIn, int indexIn)
                : owner (ownerIn), index (indexIn), preview (ownerIn, indexIn)
            {
                setSize (780, 520);

                title.setText ("Automation Lane", juce::dontSendNotification);
                title.setFont (juce::Font (20.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                help.setText ("Automation writes a looping curve into one real parameter. Pick a target that belongs to this section, shape the curve, then keep playing while it moves.",
                              juce::dontSendNotification);
                help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                help.setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (help);

                targetLabel.setText ("TARGET", juce::dontSendNotification);
                rateLabel.setText ("RATE", juce::dontSendNotification);
                shapeLabel.setText ("SHAPE", juce::dontSendNotification);
                for (auto* label : { &targetLabel, &rateLabel, &shapeLabel })
                {
                    label->setFont (juce::Font (11.0f, juce::Font::bold));
                    label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                    addAndMakeVisible (*label);
                }

                fillTargets();
                addAndMakeVisible (targetBox);

                rateSlider.setRange (0.01, 16.0, 0.01);
                rateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
                shapeSlider.setRange (0.0, 1.0, 0.001);
                shapeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
                addAndMakeVisible (rateSlider);
                addAndMakeVisible (shapeSlider);

                syncToggle.setButtonText ("BPM Sync");
                syncToggle.setTooltip ("When enabled, automation speed follows the global BPM.");
                addAndMakeVisible (syncToggle);

                addAndMakeVisible (preview);

                closeButton.setButtonText ("Close");
                closeButton.getProperties().set ("smallButton", true);
                closeButton.onClick = [this]
                {
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };
                addAndMakeVisible (closeButton);

                targetBox.onChange = [this] { applyFromControls (true); };
                rateSlider.onValueChange = [this] { applyFromControls (false); };
                shapeSlider.onValueChange = [this] { applyFromControls (false); };
                syncToggle.onClick = [this] { applyFromControls (false); };

                syncControlsFromLane();
            }

            static juce::String extractId (const juce::String& text)
            {
                const auto open = text.lastIndexOfChar ('(');
                const auto close = text.lastIndexOfChar (')');
                return open >= 0 && close > open ? text.substring (open + 1, close) : text;
            }

            void fillTargets()
            {
                targetBox.clear (juce::dontSendNotification);
                int itemId = 1;
                const auto sectionId = owner.currentSectionId();
                for (const auto& def : owner.project.getParameters().getAll())
                {
                    if (! def.visible || ! def.modulatable)
                        continue;
                    if (! owner.targetAppliesToSection (def.id, sectionId))
                        continue;
                    targetBox.addItem (def.name + " (" + def.id + ") - " + def.section, itemId++);
                }

                if (targetBox.getNumItems() == 0)
                {
                    for (const auto& def : owner.project.getParameters().getAll())
                    {
                        if (! def.visible || ! def.modulatable)
                            continue;
                        targetBox.addItem (def.name + " (" + def.id + ") - " + def.section, itemId++);
                    }
                }
            }

            void selectTarget (const juce::String& target)
            {
                for (int i = 0; i < targetBox.getNumItems(); ++i)
                {
                    if (extractId (targetBox.getItemText (i)) == target)
                    {
                        targetBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        return;
                    }
                }
                if (targetBox.getNumItems() > 0)
                    targetBox.setSelectedItemIndex (0, juce::dontSendNotification);
            }

            void syncControlsFromLane()
            {
                if (index < 0 || index >= (int) owner.project.getDspGraph().automation.size())
                    return;
                const auto& lane = owner.project.getDspGraph().automation[(size_t) index];
                selectTarget (lane.targetId);
                rateSlider.setValue (lane.rate, juce::dontSendNotification);
                shapeSlider.setValue (lane.points.size() > 1 ? lane.points[1] : 0.5f, juce::dontSendNotification);
                syncToggle.setToggleState (lane.syncToTempo, juce::dontSendNotification);
            }

            void applyFromControls (bool targetChanged)
            {
                if (index < 0 || index >= (int) owner.project.getDspGraph().automation.size())
                    return;
                auto& lane = owner.project.getDspGraph().automation[(size_t) index];
                const auto selectedTarget = extractId (targetBox.getText());
                if (selectedTarget.isNotEmpty())
                    lane.targetId = selectedTarget;
                lane.rate = (float) rateSlider.getValue();
                lane.syncToTempo = syncToggle.getToggleState();
                const auto shape = (float) shapeSlider.getValue();
                lane.points = { 0.0f, shape, 1.0f, 1.0f - shape, 0.0f };

                owner.markGraphEdited();
                owner.project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                owner.builderPanel.repaint();
                owner.formulaPanel.repaint();
                preview.repaint();

                if (targetChanged)
                    owner.rebuildGraphEditorItems();
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (18);
                r.removeFromTop (82);
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r.withTrimmedBottom (48).toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.withTrimmedBottom (48).toFloat(), 10.0f, 1.0f);
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (18);
                title.setBounds (r.removeFromTop (28));
                help.setBounds (r.removeFromTop (44));
                r.removeFromTop (12);
                auto body = r.withTrimmedBottom (50).reduced (16, 14);
                targetLabel.setBounds (body.removeFromTop (18));
                targetBox.setBounds (body.removeFromTop (34));
                body.removeFromTop (14);
                rateLabel.setBounds (body.removeFromTop (18));
                rateSlider.setBounds (body.removeFromTop (34));
                body.removeFromTop (10);
                shapeLabel.setBounds (body.removeFromTop (18));
                shapeSlider.setBounds (body.removeFromTop (34));
                body.removeFromTop (8);
                syncToggle.setBounds (body.removeFromTop (30));
                body.removeFromTop (10);
                preview.setBounds (body.removeFromTop (120));
                closeButton.setBounds (r.removeFromBottom (36).removeFromRight (94));
            }

            DspPage& owner;
            int index = -1;
            CurvePreview preview;
            juce::Label title;
            juce::Label help;
            juce::Label targetLabel;
            juce::Label rateLabel;
            juce::Label shapeLabel;
            juce::ComboBox targetBox;
            juce::Slider rateSlider;
            juce::Slider shapeSlider;
            juce::ToggleButton syncToggle;
            juce::TextButton closeButton;
        };

        auto* editor = new AutomationEditor (*this, automationIndex);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Automation Editor";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (editor);
        options.launchAsync();
    }

    void DspPage::importFxSampleForTesting()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import sample for FX testing", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<DspPage> self (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [self, chooser] (const juce::FileChooser& fc)
            {
                auto* page = self.getComponent();
                if (page == nullptr)
                    return;

                const auto file = fc.getResult();
                if (file == juce::File())
                    return;

                page->loadFxSampleFile (file);
            });
    }

    bool DspPage::loadFxSampleFile (const juce::File& file)
    {
        stopFxSamplePlayback();
        fxTrackStatusLabel.setText ("Loading sample...", juce::dontSendNotification);
        const auto importPathForLog = file.getFullPathName().toStdString();
        PC_DBG ("FX sample import requested: %s", importPathForLog.c_str());

        if (! file.existsAsFile())
        {
            fxTrackStatusLabel.setText ("Sample file does not exist", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: file does not exist");
            return false;
        }

        auto installPreviewBuffer = [this, &file] (juce::AudioBuffer<float>& loaded,
                                                   double loadedSampleRate,
                                                   const juce::String& decoderName) -> bool
        {
            std::vector<float> waveformPeaks;
            try
            {
                const int samples = loaded.getNumSamples();
                const int bins = juce::jlimit (64, 1200, samples / 256);
                waveformPeaks.resize ((size_t) juce::jmax (64, bins), 0.0f);
                for (int bin = 0; bin < (int) waveformPeaks.size(); ++bin)
                {
                    const int start = bin * samples / (int) waveformPeaks.size();
                    const int end = (bin + 1) * samples / (int) waveformPeaks.size();
                    float peak = 0.0f;
                    for (int ch = 0; ch < loaded.getNumChannels(); ++ch)
                    {
                        const auto* data = loaded.getReadPointer (ch);
                        for (int i = start; i < end; ++i)
                            peak = juce::jmax (peak, std::abs (data[i]));
                    }
                    waveformPeaks[(size_t) bin] = peak;
                }
            }
            catch (...)
            {
                fxTrackStatusLabel.setText ("Could not build sample waveform", juce::dontSendNotification);
                PC_DBG ("FX sample import failed: waveform build threw");
                return false;
            }

            {
                const juce::SpinLock::ScopedLockType lock (fxAudioLock);
                try
                {
                    fxSampleBuffer.setSize (loaded.getNumChannels(), loaded.getNumSamples(), false, false, true);
                    for (int ch = 0; ch < loaded.getNumChannels(); ++ch)
                        fxSampleBuffer.copyFrom (ch, 0, loaded, ch, 0, loaded.getNumSamples());
                }
                catch (...)
                {
                    fxSampleBuffer.setSize (0, 0);
                    fxSampleLength.store (0);
                    fxTrackStatusLabel.setText ("Could not install sample preview buffer", juce::dontSendNotification);
                    PC_DBG ("FX sample import failed: install buffer threw");
                    return false;
                }
                fxSampleRate = loadedSampleRate;
                fxSampleLength.store (loaded.getNumSamples());
                fxPlayhead.store (0);
                fxLoopStart01.store (0.0f);
                fxLoopEnd01.store (1.0f);
            }

            setFxLoopRegion (0.0f, 1.0f, true);
            fxSliceCount.store (1);
            fxSelectedSlice.store (0);
            fxTrackSliceBox.setSelectedId (1, juce::dontSendNotification);
            fxWaveformPeaks = std::move (waveformPeaks);
            fxSampleFile = file;
            fxTrackNameLabel.setText (shortenedSampleName (file), juce::dontSendNotification);
            fxTrackNameLabel.setTooltip (file.getFullPathName());
            fxTrackStatusLabel.setText ("Ready - " + juce::String (loaded.getNumSamples()) + " samples @ "
                                        + juce::String (loadedSampleRate, 0) + " Hz", juce::dontSendNotification);
            PC_DBG ("FX sample import ready via %s: channels=%d samples=%d rate=%f",
                    decoderName.toRawUTF8(),
                    loaded.getNumChannels(),
                    loaded.getNumSamples(),
                    loadedSampleRate);
            fxWaveform.repaint();
            return true;
        };

        if (file.getFileExtension().equalsIgnoreCase (".wav"))
        {
            PC_DBG ("FX sample import using safe WAV decoder");
            juce::AudioBuffer<float> loaded;
            double wavSampleRate = 44100.0;
            juce::String wavError;
            if (! tryLoadWavPreviewSafely (file, loaded, wavSampleRate, wavError))
            {
                fxSampleBuffer.setSize (0, 0);
                fxSampleLength.store (0);
                fxTrackStatusLabel.setText ("Could not import WAV: " + wavError, juce::dontSendNotification);
                PC_DBG ("FX sample import failed: safe WAV decoder: %s", wavError.toRawUTF8());
                return false;
            }
            return installPreviewBuffer (loaded, wavSampleRate, "safe-wav");
        }

        std::unique_ptr<juce::AudioFormatReader> reader;
        try
        {
            reader.reset (fxFormatManager.createReaderFor (file));
        }
        catch (...)
        {
            fxTrackStatusLabel.setText ("Could not open sample", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: createReaderFor threw");
            return false;
        }

        if (reader == nullptr)
        {
            fxTrackStatusLabel.setText ("Could not read sample", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: no reader for extension %s", file.getFileExtension().toRawUTF8());
            return false;
        }

        if (reader->sampleRate <= 0.0 || reader->numChannels == 0)
        {
            fxTrackStatusLabel.setText ("Invalid WAV metadata", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: invalid metadata rate=%f channels=%u length=%lld",
                    reader->sampleRate,
                    (unsigned int) reader->numChannels,
                    (long long) reader->lengthInSamples);
            return false;
        }

        const auto maxPreviewSamples = static_cast<std::int64_t> (juce::jmax (1.0, reader->sampleRate) * 60.0);
        const int channelsToRead = juce::jlimit (1, 2, (int) reader->numChannels);
        const auto maxSamplesByMemory = (128ll * 1024ll * 1024ll)
                                      / static_cast<std::int64_t> (channelsToRead)
                                      / static_cast<std::int64_t> (sizeof (float));
        const auto samplesToRead64 = juce::jmin (
            juce::jmin (static_cast<std::int64_t> (reader->lengthInSamples), maxPreviewSamples),
            maxSamplesByMemory);
        if (samplesToRead64 <= 0 || samplesToRead64 > static_cast<std::int64_t> (std::numeric_limits<int>::max()))
        {
            fxTrackStatusLabel.setText ("Sample is empty or too large", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: unsupported sample length=%lld", (long long) reader->lengthInSamples);
            return false;
        }

        const int samplesToRead = (int) samplesToRead64;
        const auto estimatedBytes = static_cast<std::int64_t> (channelsToRead)
                                  * static_cast<std::int64_t> (samplesToRead)
                                  * static_cast<std::int64_t> (sizeof (float));
        if (estimatedBytes > 128ll * 1024ll * 1024ll)
        {
            fxTrackStatusLabel.setText ("Sample preview could not be capped safely", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: estimated preview bytes=%lld", (long long) estimatedBytes);
            return false;
        }

        juce::AudioBuffer<float> loaded;
        try
        {
            loaded.setSize (channelsToRead, samplesToRead, false, true, true);
        }
        catch (...)
        {
            fxTrackStatusLabel.setText ("Could not allocate sample preview buffer", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: allocation channels=%d samples=%d", channelsToRead, samplesToRead);
            return false;
        }

        bool decoded = false;
        try
        {
            decoded = reader->read (&loaded, 0, samplesToRead, 0, true, channelsToRead > 1);
        }
        catch (...)
        {
            decoded = false;
        }

        if (! decoded)
        {
            fxTrackStatusLabel.setText ("Could not decode sample", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: decode failed channels=%d samples=%d", channelsToRead, samplesToRead);
            return false;
        }

        std::vector<float> waveformPeaks;
        try
        {
            const int samples = loaded.getNumSamples();
            const int bins = juce::jlimit (64, 1200, samples / 256);
            waveformPeaks.resize ((size_t) juce::jmax (64, bins), 0.0f);
            for (int bin = 0; bin < (int) waveformPeaks.size(); ++bin)
            {
                const int start = bin * samples / (int) waveformPeaks.size();
                const int end = (bin + 1) * samples / (int) waveformPeaks.size();
                float peak = 0.0f;
                for (int ch = 0; ch < loaded.getNumChannels(); ++ch)
                {
                    const auto* data = loaded.getReadPointer (ch);
                    for (int i = start; i < end; ++i)
                        peak = juce::jmax (peak, std::abs (data[i]));
                }
                waveformPeaks[(size_t) bin] = peak;
            }
        }
        catch (...)
        {
            fxTrackStatusLabel.setText ("Could not build sample waveform", juce::dontSendNotification);
            PC_DBG ("FX sample import failed: waveform build threw");
            return false;
        }

        {
            const juce::SpinLock::ScopedLockType lock (fxAudioLock);
            try
            {
                fxSampleBuffer.setSize (loaded.getNumChannels(), loaded.getNumSamples(), false, false, true);
                for (int ch = 0; ch < loaded.getNumChannels(); ++ch)
                    fxSampleBuffer.copyFrom (ch, 0, loaded, ch, 0, loaded.getNumSamples());
            }
            catch (...)
            {
                fxSampleBuffer.setSize (0, 0);
                fxSampleLength.store (0);
                fxTrackStatusLabel.setText ("Could not install sample preview buffer", juce::dontSendNotification);
                PC_DBG ("FX sample import failed: install buffer threw");
                return false;
            }
            fxSampleRate = reader->sampleRate;
            fxSampleLength.store (loaded.getNumSamples());
            fxPlayhead.store (0);
            fxLoopStart01.store (0.0f);
            fxLoopEnd01.store (1.0f);
        }

        setFxLoopRegion (0.0f, 1.0f, true);
        fxSliceCount.store (1);
        fxSelectedSlice.store (0);
        fxTrackSliceBox.setSelectedId (1, juce::dontSendNotification);
        fxWaveformPeaks = std::move (waveformPeaks);
        fxSampleFile = file;
        fxTrackNameLabel.setText (shortenedSampleName (file), juce::dontSendNotification);
        fxTrackNameLabel.setTooltip (file.getFullPathName());
        fxTrackStatusLabel.setText ("Ready - " + juce::String (loaded.getNumSamples()) + " samples @ "
                                    + juce::String (reader->sampleRate, 0) + " Hz", juce::dontSendNotification);
        PC_DBG ("FX sample import ready: channels=%d samples=%d rate=%f",
                loaded.getNumChannels(),
                loaded.getNumSamples(),
                reader->sampleRate);
        fxWaveform.repaint();
        return true;
    }

    bool DspPage::loadFxSampleZone (const SampleZoneDef& zone)
    {
        if (zone.samplePath.trim().isEmpty())
        {
            fxTrackStatusLabel.setText ("Selected zone has no sample path", juce::dontSendNotification);
            return false;
        }

        juce::File file (zone.samplePath);
        if (! file.existsAsFile() && ! juce::File::isAbsolutePath (zone.samplePath))
        {
            const auto projectFolder = project.getProjectFolder();
            if (projectFolder.isDirectory())
            {
                file = projectFolder.getChildFile (zone.samplePath);
                if (! file.existsAsFile())
                    file = projectFolder.getChildFile ("samples").getChildFile (zone.samplePath);
                if (! file.existsAsFile())
                    file = projectFolder.getChildFile ("assets").getChildFile (zone.samplePath);
            }
        }

        if (! file.existsAsFile())
        {
            fxTrackStatusLabel.setText ("Mapper sample missing: " + zone.samplePath, juce::dontSendNotification);
            return false;
        }

        if (! loadFxSampleFile (file))
            return false;

        const int length = fxSampleLength.load();
        if (length > 0)
        {
            int start = zone.sampleStart;
            int end = zone.sampleEnd > zone.sampleStart ? zone.sampleEnd : length;

            if (zone.loopEnabled && zone.loopEnd > zone.loopStart)
            {
                start = zone.loopStart;
                end = zone.loopEnd;
            }

            start = juce::jlimit (0, juce::jmax (0, length - 1), start);
            end = juce::jlimit (start + 1, length, end);
            setFxLoopRegion ((float) start / (float) length, (float) end / (float) length, true);
        }

        fxTrackStatusLabel.setText ("Loaded mapper zone: " + shortenedSampleName (file, 42),
                                    juce::dontSendNotification);
        return true;
    }

    void DspPage::startFxSamplePlayback()
    {
        const auto length = fxSampleLength.load();
        const bool liveInput = fxUseLiveInput.load();
        if (studioOwner == nullptr)
            return;

        if (! liveInput && length <= 0)
        {
            fxTrackStatusLabel.setText ("Import a sample first or enable Live Input", juce::dontSendNotification);
            return;
        }

        juce::String error;
        if (! studioOwner->getAudio().ensureOpen (error, liveInput ? 1 : 0, 2))
        {
            fxTrackStatusLabel.setText ("No audio device: " + error, juce::dontSendNotification);
            PC_DBG ("FX sample preview audio open failed: %s", error.toRawUTF8());
            return;
        }

        if (! fxCallbackRegistered)
        {
            studioOwner->getAudio().getDeviceManager().addAudioCallback (this);
            fxCallbackRegistered = true;
        }
        fxRoutingEngine.bind (project.getDspGraph(), project.getParameters());
        fxRoutingEngine.prepare (RenderContext::forBlock (fxPreviewSampleRate,
                                                          fxPreviewBlockSize,
                                                          fxPreviewBlockSize,
                                                          0,
                                                          fxPreviewChannels,
                                                          120.0));
        fxRoutingEngine.syncFromLiveValues (project.getLiveValues());

        if (! liveInput)
        {
            const int loopStart = juce::jlimit (0, juce::jmax (0, length - 1),
                juce::roundToInt (fxLoopStart01.load() * (float) length));
            const int loopEnd = juce::jlimit (loopStart + 1, length,
                juce::roundToInt (fxLoopEnd01.load() * (float) length));
            if (fxRetriggerOnPlay.load() || fxPlayhead.load() < loopStart || fxPlayhead.load() >= loopEnd)
                retriggerFxSamplePlayback (false);
        }
        fxPlaying.store (true);
        fxTrackStatusLabel.setText (liveInput ? "Monitoring live input through FX chain"
                                              : "Playing through FX chain",
                                    juce::dontSendNotification);
    }

    void DspPage::stopFxSamplePlayback()
    {
        fxPlaying.store (false);
        fxPlayhead.store (0);
        if (fxCallbackRegistered && studioOwner != nullptr)
        {
            studioOwner->getAudio().getDeviceManager().removeAudioCallback (this);
            fxCallbackRegistered = false;
        }
        fxTrackStatusLabel.setText (fxSampleLength.load() > 0 ? "Stopped" : "FX sample player idle",
                                    juce::dontSendNotification);
        fxWaveform.repaint();
    }

    float DspPage::getFxPlaybackPosition01() const
    {
        const auto len = fxSampleLength.load();
        return len > 0 ? juce::jlimit (0.0f, 1.0f, (float) fxPlayhead.load() / (float) len) : 0.0f;
    }

    void DspPage::retriggerFxSamplePlayback (bool updateStatus)
    {
        const int length = fxSampleLength.load();
        if (length <= 0)
        {
            if (updateStatus)
                fxTrackStatusLabel.setText ("Import a sample first", juce::dontSendNotification);
            return;
        }

        int regionStart = juce::jlimit (0, juce::jmax (0, length - 1),
            juce::roundToInt (fxLoopStart01.load() * (float) length));

        const int sliceCount = juce::jlimit (1, 32, fxSliceCount.load());
        if (sliceCount > 1)
        {
            const int activeSlice = juce::jlimit (0, sliceCount - 1, fxSelectedSlice.load());
            regionStart = juce::jlimit (0, juce::jmax (0, length - 1),
                juce::roundToInt ((float) activeSlice / (float) sliceCount * (float) length));
        }

        fxPlayhead.store (regionStart);
        fxWaveform.repaint();
        if (updateStatus)
            fxTrackStatusLabel.setText ("Retriggered sample preview", juce::dontSendNotification);
    }

    void DspPage::setFxLoopRegion (float start01, float end01, bool clampPlayhead)
    {
        constexpr float minLoopWidth = 0.005f;
        start01 = juce::jlimit (0.0f, 1.0f - minLoopWidth, start01);
        end01 = juce::jlimit (start01 + minLoopWidth, 1.0f, end01);
        fxLoopStart01.store (start01);
        fxLoopEnd01.store (end01);
        fxTrackLoopStartSlider.setValue ((double) start01 * 100.0, juce::dontSendNotification);
        fxTrackLoopEndSlider.setValue ((double) end01 * 100.0, juce::dontSendNotification);

        if (clampPlayhead)
        {
            const auto length = fxSampleLength.load();
            if (length > 0)
            {
                const int loopStart = juce::jlimit (0, juce::jmax (0, length - 1),
                    juce::roundToInt (start01 * (float) length));
                const int loopEnd = juce::jlimit (loopStart + 1, length,
                    juce::roundToInt (end01 * (float) length));
                const auto playhead = fxPlayhead.load();
                if (playhead < loopStart || playhead >= loopEnd)
                    fxPlayhead.store (loopStart);
            }
        }

        fxWaveform.repaint();
    }

    void DspPage::setFxSliceCount (int slices)
    {
        slices = juce::jlimit (1, 32, slices);
        fxSliceCount.store (slices);
        fxSelectedSlice.store (0);
        if (slices <= 1)
        {
            setFxLoopRegion (0.0f, 1.0f, true);
            fxTrackStatusLabel.setText (fxSampleLength.load() > 0 ? "Full sample selected" : "FX sample player idle",
                                        juce::dontSendNotification);
            return;
        }

        selectFxSlice (0);
    }

    void DspPage::selectFxSlice (int sliceIndex)
    {
        const int slices = juce::jlimit (1, 32, fxSliceCount.load());
        if (slices <= 1)
        {
            fxSelectedSlice.store (0);
            setFxLoopRegion (0.0f, 1.0f, true);
            return;
        }

        sliceIndex = (sliceIndex % slices + slices) % slices;
        fxSelectedSlice.store (sliceIndex);
        const float start = (float) sliceIndex / (float) slices;
        const float end = (float) (sliceIndex + 1) / (float) slices;
        setFxLoopRegion (start, end, true);
        fxTrackStatusLabel.setText ("Slice " + juce::String (sliceIndex + 1) + " / " + juce::String (slices),
                                    juce::dontSendNotification);
    }

    DspBlock* DspPage::selectedWavetableBlock()
    {
        if (selectedGraphKind != 1 || selectedGraphIndex < 0
            || selectedGraphIndex >= (int) project.getDspGraph().blocks.size())
            return nullptr;

        auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
        return block.section == "source" && block.type.containsIgnoreCase ("wavetable") ? &block : nullptr;
    }

    const DspBlock* DspPage::selectedWavetableBlock() const
    {
        return const_cast<DspPage*> (this)->selectedWavetableBlock();
    }

    void DspPage::ensureWavetableShapeDefaults (DspBlock& block)
    {
        if (block.values.find ("wtFrameCount") == block.values.end())
            block.values["wtFrameCount"] = 1.0f;
        if (block.values.find ("wtFramePosition") == block.values.end())
            block.values["wtFramePosition"] = 0.0f;

        for (int point = 0; point < 32; ++point)
        {
            const auto key = "wtShape" + juce::String (point);
            if (block.values.find (key) == block.values.end())
                block.values[key] = std::sin ((float) point / 32.0f * juce::MathConstants<float>::twoPi);

            for (int frame = 0; frame < 4; ++frame)
            {
                const auto frameKey = "wtFrame" + juce::String (frame) + "Shape" + juce::String (point);
                if (block.values.find (frameKey) == block.values.end())
                    block.values[frameKey] = frame == 0
                        ? block.values[key]
                        : std::sin ((float) point / 32.0f * juce::MathConstants<float>::twoPi * (float) (frame + 1));
            }
        }
    }

    void DspPage::setSelectedWavetableShapeToSine()
    {
        auto* block = selectedWavetableBlock();
        if (block == nullptr)
            return;

        const int frame = juce::jlimit (0, 3, juce::roundToInt (valueForBlockKey (*block, "wtFramePosition", 0.0f)
            * (juce::jmax (1.0f, valueForBlockKey (*block, "wtFrameCount", 1.0f)) - 1.0f)));

        for (int point = 0; point < 32; ++point)
        {
            const auto value = std::sin ((float) point / 32.0f * juce::MathConstants<float>::twoPi);
            block->values["wtShape" + juce::String (point)] =
                frame == 0 ? value : valueForBlockKey (*block, "wtShape" + juce::String (point), value);
            block->values["wtFrame" + juce::String (frame) + "Shape" + juce::String (point)] = value;
        }

        block->values["wtTable"] = 8.0f;
        markGraphEdited();
        refreshFxPreviewRouting();
        project.notifyChanged();
        syncGraphEditor();
        wavetableEditor.repaint();
    }

    void DspPage::normalizeSelectedWavetableShape()
    {
        auto* block = selectedWavetableBlock();
        if (block == nullptr)
            return;

        ensureWavetableShapeDefaults (*block);
        const int frame = juce::jlimit (0, 3, juce::roundToInt (valueForBlockKey (*block, "wtFramePosition", 0.0f)
            * (juce::jmax (1.0f, valueForBlockKey (*block, "wtFrameCount", 1.0f)) - 1.0f)));

        float peak = 0.0f;
        for (int point = 0; point < 32; ++point)
            peak = juce::jmax (peak, std::abs (valueForBlockKey (*block, "wtFrame" + juce::String (frame) + "Shape" + juce::String (point), 0.0f)));

        if (peak <= 0.0001f)
            return;

        for (int point = 0; point < 32; ++point)
        {
            const auto key = "wtFrame" + juce::String (frame) + "Shape" + juce::String (point);
            block->values[key] = juce::jlimit (-1.0f, 1.0f, valueForBlockKey (*block, key, 0.0f) / peak);
            if (frame == 0)
                block->values["wtShape" + juce::String (point)] = block->values[key];
        }

        block->values["wtTable"] = 8.0f;
        markGraphEdited();
        refreshFxPreviewRouting();
        project.notifyChanged();
        syncGraphEditor();
        wavetableEditor.repaint();
    }

    void DspPage::importWavetableShape()
    {
        auto* block = selectedWavetableBlock();
        if (block == nullptr)
        {
            editorHint.setText ("Select a Wavetable Source block before importing a custom table.",
                                juce::dontSendNotification);
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser> ("Import one-cycle WAV/AIFF as custom wavetable",
                                                            juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                    importWavetableShapeFromFile (file);
            });
    }

    bool DspPage::importWavetableShapeFromFile (const juce::File& file)
    {
        auto* block = selectedWavetableBlock();
        if (block == nullptr)
        {
            editorHint.setText ("Select a Wavetable Source block before importing a custom table.",
                                juce::dontSendNotification);
            return false;
        }
        if (! file.existsAsFile())
            return false;

        std::unique_ptr<juce::AudioFormatReader> reader (fxFormatManager.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
        {
            editorHint.setText ("Could not read wavetable import file.", juce::dontSendNotification);
            return false;
        }

        const int readSamples = (int) juce::jlimit ((juce::int64) 32, (juce::int64) 65536, reader->lengthInSamples);
        juce::AudioBuffer<float> temp (1, readSamples);
        if (! reader->read (&temp, 0, readSamples, 0, true, false))
        {
            editorHint.setText ("Wavetable import failed while decoding audio.", juce::dontSendNotification);
            return false;
        }

        const int frameCount = juce::jlimit (1, 4, readSamples / 128);
        for (int frame = 0; frame < frameCount; ++frame)
        {
            float peak = 0.0f;
            const int frameStart = frame * readSamples / frameCount;
            const int frameEnd = (frame + 1) * readSamples / frameCount;
            const int frameSamples = juce::jmax (32, frameEnd - frameStart);
            for (int point = 0; point < 32; ++point)
            {
                const float source = (float) frameStart + (float) point / 32.0f * (float) frameSamples;
                const int a = juce::jlimit (0, readSamples - 1, (int) std::floor (source));
                const int b = juce::jlimit (0, readSamples - 1, a + 1);
                const float frac = source - (float) a;
                const float value = temp.getSample (0, a) + (temp.getSample (0, b) - temp.getSample (0, a)) * frac;
                block->values["wtFrame" + juce::String (frame) + "Shape" + juce::String (point)] = value;
                peak = juce::jmax (peak, std::abs (value));
            }

            if (peak > 0.0001f)
                for (int point = 0; point < 32; ++point)
                {
                    const auto key = "wtFrame" + juce::String (frame) + "Shape" + juce::String (point);
                    block->values[key] = juce::jlimit (-1.0f, 1.0f, valueForBlockKey (*block, key, 0.0f) / peak);
                    if (frame == 0)
                        block->values["wtShape" + juce::String (point)] = block->values[key];
                }
        }

        block->values["wtTable"] = 8.0f;
        block->values["wtFrameCount"] = (float) frameCount;
        block->values["wtFramePosition"] = 0.0f;
        block->name = frameCount > 1 ? "Resynth WT " + juce::String (frameCount) + "F" : "Imported WT";
        block->metadata["wavetableSourceFile"] = file.getFileName();
        block->metadata["wavetableSourcePath"] = file.getFullPathName();
        block->metadata["wavetableImportedAt"] = juce::Time::getCurrentTime().toISO8601 (true);
        block->metadata["wavetableFrameCount"] = juce::String (frameCount);
        block->metadata["wavetableSourceSamples"] = juce::String ((juce::int64) reader->lengthInSamples);
        markGraphEdited();
        refreshFxPreviewRouting();
        project.notifyChanged();
        syncGraphEditor();
        refreshBuilderPanel();
        wavetableEditor.repaint();
        editorHint.setText ("Imported " + file.getFileName() + " as a "
                            + juce::String (frameCount) + "-frame custom wavetable.",
                            juce::dontSendNotification);
        return true;
    }

    bool DspPage::selectedBlockIsSurgicalEq() const
    {
        if (selectedGraphKind != 1 || selectedGraphIndex < 0
            || selectedGraphIndex >= (int) project.getDspGraph().blocks.size())
            return false;

        const auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
        return block.section == "filter" && block.type.containsIgnoreCase ("eq");
    }

    void DspPage::refreshEqActionButtons()
    {
        const bool eqSelected = selectedBlockIsSurgicalEq();
        eqBandCopyButton.setEnabled (eqSelected);
        eqBandPasteButton.setEnabled (eqSelected && ! eqBandClipboard.empty());
        eqBandSaveButton.setEnabled (eqSelected);
        eqBandInsertButton.setEnabled (eqBandLibraryFile().existsAsFile());
        eqAnalyzerFreezeToggle.setEnabled (eqAnalyzerToggle.getToggleState());
    }

    void DspPage::copySelectedEqBand()
    {
        if (! selectedBlockIsSurgicalEq())
            return;

        const auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
        eqBandClipboard.clear();
        for (const auto& key : { juce::String ("eqOn"), juce::String ("eqType"), juce::String ("eqMode"), juce::String ("eqFreq"),
                                 juce::String ("eqGainDb"), juce::String ("eqQ"), juce::String ("eqMix"), juce::String ("eqSolo"),
                                 juce::String ("eqDynMode"), juce::String ("eqDynThresholdDb"), juce::String ("eqDynRangeDb"),
                                 juce::String ("eqDynAttackMs"), juce::String ("eqDynReleaseMs") })
            eqBandClipboard[key] = valueForBlockKey (block, key, key == "eqMix" || key == "eqOn" ? 1.0f : 0.0f);

        editorHint.setText ("Copied " + block.name + ". Select another Surgical EQ block and use Paste Band.",
                            juce::dontSendNotification);
        refreshEqActionButtons();
    }

    void DspPage::pasteSelectedEqBand()
    {
        if (! selectedBlockIsSurgicalEq() || eqBandClipboard.empty())
            return;

        auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
        for (const auto& entry : eqBandClipboard)
            block.values[entry.first] = entry.second;

        const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
        block.targetId = "eqBand" + juce::String (band) + "Freq";
        markGraphEdited();
        project.notifyChanged();
        refreshFxPreviewRouting();
        syncGraphEditor();
        refreshBuilderPanel();
        surgicalEqPanel.repaint();
    }

    juce::File DspPage::eqBandLibraryFile() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("eq_band_library.json");
    }

    bool DspPage::loadSavedEqBand (std::map<juce::String, float>& values) const
    {
        values.clear();
        const auto file = eqBandLibraryFile();
        if (! file.existsAsFile())
            return false;

        const auto root = juce::JSON::parse (file);
        const auto* rootObject = root.getDynamicObject();
        if (rootObject == nullptr)
            return false;

        const auto valueVar = rootObject->getProperty ("values");
        const auto* valueObject = valueVar.getDynamicObject();
        if (valueObject == nullptr)
            return false;

        const auto& properties = valueObject->getProperties();
        for (int i = 0; i < properties.size(); ++i)
            values[properties.getName (i).toString()] = (float) properties.getValueAt (i);

        return ! values.empty();
    }

    void DspPage::saveSelectedEqBandToLibrary()
    {
        if (! selectedBlockIsSurgicalEq())
            return;

        const auto& block = project.getDspGraph().blocks[(size_t) selectedGraphIndex];
        auto* root = new juce::DynamicObject();
        auto* values = new juce::DynamicObject();
        for (const auto& key : { juce::String ("eqOn"), juce::String ("eqType"), juce::String ("eqMode"), juce::String ("eqFreq"),
                                 juce::String ("eqGainDb"), juce::String ("eqQ"), juce::String ("eqMix"), juce::String ("eqSolo"),
                                 juce::String ("eqDynMode"), juce::String ("eqDynThresholdDb"), juce::String ("eqDynRangeDb"),
                                 juce::String ("eqDynAttackMs"), juce::String ("eqDynReleaseMs") })
            values->setProperty (key, valueForBlockKey (block, key, key == "eqMix" || key == "eqOn" ? 1.0f : 0.0f));

        root->setProperty ("format", 1);
        root->setProperty ("name", block.name);
        root->setProperty ("values", juce::var (values));

        const auto file = eqBandLibraryFile();
        file.getParentDirectory().createDirectory();
        if (file.replaceWithText (juce::JSON::toString (juce::var (root), true)))
            editorHint.setText ("Saved " + block.name + " to the local EQ band library.", juce::dontSendNotification);
        else
            editorHint.setText ("Could not save the EQ band library file.", juce::dontSendNotification);

        refreshEqActionButtons();
    }

    void DspPage::insertSavedEqBand()
    {
        std::map<juce::String, float> saved;
        if (! loadSavedEqBand (saved))
        {
            editorHint.setText ("No saved EQ band found. Select a band and use Save Band first.", juce::dontSendNotification);
            refreshEqActionButtons();
            return;
        }

        auto& graph = project.getDspGraph();
        int bankCount = 0;
        bool usedBands[9] {};
        int filterOrdinal = 0;
        for (const auto& block : graph.blocks)
        {
            if (block.section != "filter")
                continue;

            const int bank = bankForBlockOrdinal (block, filterOrdinal);
            if (bank == currentSectionBank())
            {
                ++bankCount;
                if (block.type.containsIgnoreCase ("eq"))
                {
                    const int band = juce::jlimit (1, 8, juce::roundToInt (valueForBlockKey (block, "eqBand", 1.0f)));
                    usedBands[band] = true;
                }
            }

            ++filterOrdinal;
        }

        if (bankCount >= kSourceMatrixBankSize)
        {
            editorHint.setText ("This Filter bank is full. Switch banks or delete a block before inserting a saved EQ band.",
                                juce::dontSendNotification);
            return;
        }

        int band = 1;
        while (band <= 8 && usedBands[band])
            ++band;
        if (band > 8)
            band = juce::jlimit (1, 8, bankCount + 1);

        DspBlock block;
        block.section = "filter";
        block.type = "surgicalEq";
        block.id = "saved_eq_band_" + juce::String ((int) graph.blocks.size() + 1);
        block.name = "Saved EQ Band " + juce::String (band);
        block.targetId = "eqBand" + juce::String (band) + "Freq";
        block.values = saved;
        block.values["bank"] = (float) currentSectionBank();
        block.values["eqBand"] = (float) band;
        block.values["eqOn"] = valueForBlockKey (block, "eqOn", 1.0f);
        block.values["eqFreq"] = valueForBlockKey (block, "eqFreq", 1000.0f);
        block.values["eqGainDb"] = valueForBlockKey (block, "eqGainDb", 0.0f);
        block.values["eqQ"] = valueForBlockKey (block, "eqQ", 1.0f);
        block.values["eqMix"] = valueForBlockKey (block, "eqMix", 1.0f);

        graph.blocks.push_back (block);
        selectedGraphKind = 1;
        selectedGraphIndex = (int) graph.blocks.size() - 1;
        builderPanel.selectedItemIds.clear();
        builderPanel.selectedItemId = 1000 + selectedGraphIndex + 1;
        builderPanel.selectedItemIds.add (builderPanel.selectedItemId);
        editorItemBox.setSelectedId (builderPanel.selectedItemId, juce::sendNotification);

        markGraphEdited();
        project.notifyChanged();
        rebuildGraphEditorItems();
        refreshFxPreviewRouting();
        refreshBuilderPanel();
        syncGraphEditor();
        surgicalEqPanel.repaint();
        editorHint.setText ("Inserted saved EQ band into Filter bank " + juce::String (currentSectionBank() + 1) + ".",
                            juce::dontSendNotification);
    }

    void DspPage::pushEqAnalyzerSamples (const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const int channels = buffer.getNumChannels();
        if (channels <= 0 || numSamples <= 0)
            return;

        int index = eqAnalyzerFifoIndex.load (std::memory_order_relaxed);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                mono += buffer.getSample (ch, sample);
            mono /= (float) channels;

            eqAnalyzerFifo[(size_t) index++] = mono;
            if (index >= kEqAnalyzerFftSize)
            {
                const juce::SpinLock::ScopedTryLockType lock (eqAnalyzerLock);
                if (lock.isLocked())
                {
                    std::copy (eqAnalyzerFifo.begin(), eqAnalyzerFifo.end(), eqAnalyzerSnapshot.begin());
                    eqAnalyzerSnapshotReady.store (true, std::memory_order_release);
                }
                index = 0;
            }
        }

        eqAnalyzerFifoIndex.store (index, std::memory_order_relaxed);
    }

    void DspPage::updateEqAnalyzerBins()
    {
        if (eqAnalyzerFreezeToggle.getToggleState())
            return;

        bool updated = false;
        if (eqAnalyzerSnapshotReady.exchange (false, std::memory_order_acquire))
        {
            const juce::SpinLock::ScopedTryLockType lock (eqAnalyzerLock);
            if (lock.isLocked())
            {
                std::fill (eqAnalyzerFftBuffer.begin(), eqAnalyzerFftBuffer.end(), 0.0f);
                for (int i = 0; i < kEqAnalyzerFftSize; ++i)
                {
                    const float phase = (float) i / (float) juce::jmax (1, kEqAnalyzerFftSize - 1);
                    const float window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
                    eqAnalyzerFftBuffer[(size_t) i] = eqAnalyzerSnapshot[(size_t) i] * window;
                }
                updated = true;
            }
        }

        if (updated)
        {
            eqAnalyzerFft.performFrequencyOnlyForwardTransform (eqAnalyzerFftBuffer.data());
            const int nyquistBin = kEqAnalyzerFftSize / 2;
            const auto sampleRate = juce::jmax (8000.0, fxPreviewSampleRate);

            for (int i = 0; i < kEqAnalyzerDisplayBins; ++i)
            {
                const float x0 = (float) i / (float) kEqAnalyzerDisplayBins;
                const float x1 = (float) (i + 1) / (float) kEqAnalyzerDisplayBins;
                const int firstBin = juce::jlimit (1, nyquistBin - 1,
                    juce::roundToInt (normalisedToFrequency (x0) / (float) sampleRate * (float) kEqAnalyzerFftSize));
                const int lastBin = juce::jlimit (firstBin + 1, nyquistBin,
                    juce::roundToInt (normalisedToFrequency (x1) / (float) sampleRate * (float) kEqAnalyzerFftSize));

                float peak = 0.0f;
                for (int bin = firstBin; bin < lastBin; ++bin)
                    peak = juce::jmax (peak, eqAnalyzerFftBuffer[(size_t) bin]);

                const float db = juce::Decibels::gainToDecibels (peak / (float) kEqAnalyzerFftSize, -120.0f);
                const float target = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -84.0f, -6.0f, 0.0f, 1.0f));
                eqAnalyzerBins[(size_t) i] = target;
                eqAnalyzerDisplay[(size_t) i] = juce::jmax (target, eqAnalyzerDisplay[(size_t) i] * 0.86f);
            }
        }
        else if (! fxPlaying.load())
        {
            for (auto& bin : eqAnalyzerDisplay)
                bin *= 0.92f;
        }
    }

    void DspPage::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        fxPreviewSampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
        fxPreviewBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
        fxPreviewChannels = device != nullptr ? juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits()) : 2;
        fxPreviewEngine = std::make_unique<EffectEngine>();
        fxPreviewEngine->prepare (fxPreviewSampleRate, fxPreviewBlockSize, fxPreviewChannels);
        fxPreviewEngine->setRenderContext (RenderContext::forBlock (fxPreviewSampleRate,
                                                                     fxPreviewBlockSize,
                                                                     fxPreviewBlockSize,
                                                                     device != nullptr ? device->getActiveInputChannels().countNumberOfSetBits() : 0,
                                                                     fxPreviewChannels,
                                                                     120.0));
        fxRoutingEngine.bind (project.getDspGraph(), project.getParameters());
        fxRoutingEngine.prepare (RenderContext::forBlock (fxPreviewSampleRate,
                                                          fxPreviewBlockSize,
                                                          fxPreviewBlockSize,
                                                          device != nullptr ? device->getActiveInputChannels().countNumberOfSetBits() : 0,
                                                          fxPreviewChannels,
                                                          120.0));
        fxRoutingEngine.syncFromLiveValues (project.getLiveValues());
        for (const auto& def : project.getParameters().getAll())
            fxPreviewEngine->setParameter (def.id, project.getLiveValues().getValue (def.id, def.defaultValue));
    }

    void DspPage::audioDeviceStopped()
    {
        const juce::SpinLock::ScopedLockType lock (fxAudioLock);
        fxPreviewEngine.reset();
        fxRoutingEngine.reset();
        fxCallbackBuffer.setSize (0, 0);
        fxDryCallbackBuffer.setSize (0, 0);
        fxPlaying.store (false);
        eqAnalyzerFifoIndex.store (0, std::memory_order_relaxed);
        eqAnalyzerSnapshotReady.store (false, std::memory_order_release);
        eqAnalyzerDisplay.fill (0.0f);
        eqAnalyzerBins.fill (0.0f);
    }

    void DspPage::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
    {
        if (outputs == nullptr || numOutputs <= 0)
            return;

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);

        const juce::SpinLock::ScopedTryLockType lock (fxAudioLock);
        const auto length = fxSampleLength.load();
        const bool liveInput = fxUseLiveInput.load();
        if (! lock.isLocked() || ! fxPlaying.load())
            return;
        if (! liveInput && (length <= 0 || fxSampleBuffer.getNumChannels() <= 0))
            return;
        if (liveInput && (inputs == nullptr || numInputs <= 0))
            return;

        if (fxCallbackBuffer.getNumChannels() < numOutputs || fxCallbackBuffer.getNumSamples() < numSamples)
            fxCallbackBuffer.setSize (juce::jmax (1, numOutputs), juce::jmax (1, numSamples), false, false, true);
        if (fxDryCallbackBuffer.getNumChannels() < numOutputs || fxDryCallbackBuffer.getNumSamples() < numSamples)
            fxDryCallbackBuffer.setSize (juce::jmax (1, numOutputs), juce::jmax (1, numSamples), false, false, true);
        fxCallbackBuffer.clear (0, numSamples);
        fxDryCallbackBuffer.clear (0, numSamples);

        if (liveInput)
        {
            const float gain = fxPreviewGain.load();
            for (int ch = 0; ch < numOutputs; ++ch)
            {
                const int srcCh = juce::jlimit (0, juce::jmax (0, numInputs - 1), ch);
                const float* src = inputs[srcCh];
                auto* wet = fxCallbackBuffer.getWritePointer (ch);
                auto* dry = fxDryCallbackBuffer.getWritePointer (ch);
                if (src != nullptr)
                {
                    for (int i = 0; i < numSamples; ++i)
                        wet[i] = dry[i] = src[i] * gain;
                }
            }

            if (numInputs == 1 && numOutputs > 1)
            {
                fxCallbackBuffer.copyFrom (1, 0, fxCallbackBuffer, 0, 0, numSamples);
                fxDryCallbackBuffer.copyFrom (1, 0, fxDryCallbackBuffer, 0, 0, numSamples);
            }

            if (fxPreviewEngine != nullptr)
            {
                auto context = RenderContext::forBlock (fxPreviewSampleRate,
                                                        numSamples,
                                                        fxPreviewBlockSize,
                                                        numInputs,
                                                        numOutputs,
                                                        120.0);
                context.isPlaying = true;
                fxRoutingEngine.processToEngine (*fxPreviewEngine, context);
                fxPreviewEngine->process (fxCallbackBuffer, 0, numSamples);
                fxRoutingEngine.captureAudioAnalysis (fxCallbackBuffer, 0, numSamples);
            }

            const int monitor = fxMonitorMode.load();
            if (monitor == 1)
            {
                for (int ch = 0; ch < numOutputs; ++ch)
                    fxCallbackBuffer.copyFrom (ch, 0, fxDryCallbackBuffer, ch, 0, numSamples);
            }
            else if (monitor == 2 && numOutputs > 1)
            {
                fxCallbackBuffer.copyFrom (0, 0, fxDryCallbackBuffer, 0, 0, numSamples);
            }

            pushEqAnalyzerSamples (fxCallbackBuffer, numSamples);

            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                    juce::FloatVectorOperations::copy (outputs[ch], fxCallbackBuffer.getReadPointer (ch), numSamples);
            return;
        }

        int pos = fxPlayhead.load();
        const int loopStart = juce::jlimit (0, juce::jmax (0, length - 1),
            juce::roundToInt (fxLoopStart01.load() * (float) length));
        const int loopEnd = juce::jlimit (loopStart + 1, length,
            juce::roundToInt (fxLoopEnd01.load() * (float) length));
        pos = juce::jlimit (loopStart, loopEnd - 1, pos);
        const float gain = fxPreviewGain.load();
        const int triggerMode = fxTriggerMode.load();
        const int sliceCount = juce::jlimit (1, 32, fxSliceCount.load());
        int activeSlice = juce::jlimit (0, sliceCount - 1, fxSelectedSlice.load());
        auto sliceStartFor = [length, sliceCount] (int slice)
        {
            return juce::jlimit (0, juce::jmax (0, length - 1),
                                 juce::roundToInt ((float) slice / (float) sliceCount * (float) length));
        };
        auto sliceEndFor = [length, sliceCount] (int slice)
        {
            return juce::jlimit (1, length,
                                 juce::roundToInt ((float) (slice + 1) / (float) sliceCount * (float) length));
        };
        int regionStart = loopStart;
        int regionEnd = loopEnd;
        if (sliceCount > 1 && triggerMode >= 2)
        {
            regionStart = sliceStartFor (activeSlice);
            regionEnd = juce::jmax (regionStart + 1, sliceEndFor (activeSlice));
            pos = juce::jlimit (regionStart, regionEnd - 1, pos);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            if (pos >= regionEnd)
            {
                if (triggerMode == 1)
                {
                    fxPlaying.store (false);
                    break;
                }
                else if (triggerMode == 2 && sliceCount > 1)
                {
                    activeSlice = activeSlice + 1;
                    if (activeSlice >= sliceCount)
                    {
                        if (! fxLooping.load())
                        {
                            fxPlaying.store (false);
                            break;
                        }
                        activeSlice = 0;
                    }
                    fxSelectedSlice.store (activeSlice);
                    regionStart = sliceStartFor (activeSlice);
                    regionEnd = juce::jmax (regionStart + 1, sliceEndFor (activeSlice));
                    pos = regionStart;
                }
                else if (triggerMode == 3 && sliceCount > 1)
                {
                    activeSlice = (activeSlice * 7 + 3) % sliceCount;
                    fxSelectedSlice.store (activeSlice);
                    regionStart = sliceStartFor (activeSlice);
                    regionEnd = juce::jmax (regionStart + 1, sliceEndFor (activeSlice));
                    pos = regionStart;
                }
                else if (fxLooping.load())
                    pos = regionStart;
                else
                {
                    fxPlaying.store (false);
                    break;
                }
            }

            for (int ch = 0; ch < numOutputs; ++ch)
            {
                const int srcCh = juce::jmin (ch, fxSampleBuffer.getNumChannels() - 1);
                const float sample = fxSampleBuffer.getSample (srcCh, pos) * gain;
                fxCallbackBuffer.setSample (ch, i, sample);
                fxDryCallbackBuffer.setSample (ch, i, sample);
            }
            ++pos;
        }

        fxPlayhead.store (pos);
        if (fxPreviewEngine != nullptr)
        {
            auto context = RenderContext::forBlock (fxPreviewSampleRate,
                                                    numSamples,
                                                    fxPreviewBlockSize,
                                                    numInputs,
                                                    numOutputs,
                                                    120.0);
            context.isPlaying = fxPlaying.load();
            context.timeInSamples = fxPlayhead.load();
            context.timeInSeconds = (double) context.timeInSamples / context.sampleRate;
            context.ppqPosition = context.timeInSeconds * context.bpm / 60.0;
            context.ppqPositionOfLastBarStart = std::floor (context.ppqPosition / context.barLengthInBeats())
                                              * context.barLengthInBeats();
            fxRoutingEngine.processToEngine (*fxPreviewEngine, context);
            fxPreviewEngine->process (fxCallbackBuffer, 0, numSamples);
            fxRoutingEngine.captureAudioAnalysis (fxCallbackBuffer, 0, numSamples);
        }

        const int monitor = fxMonitorMode.load();
        if (monitor == 1)
        {
            for (int ch = 0; ch < numOutputs; ++ch)
                fxCallbackBuffer.copyFrom (ch, 0, fxDryCallbackBuffer, ch, 0, numSamples);
        }
        else if (monitor == 2 && numOutputs > 1)
        {
            fxCallbackBuffer.copyFrom (0, 0, fxDryCallbackBuffer, 0, 0, numSamples);
        }

        pushEqAnalyzerSamples (fxCallbackBuffer, numSamples);

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::copy (outputs[ch], fxCallbackBuffer.getReadPointer (ch), numSamples);
    }

    void DspPage::deleteSelectedGraphItem()
    {
        std::vector<int> itemIds;
        for (auto id : builderPanel.selectedItemIds)
            if (id > 0)
                itemIds.push_back (id);

        if (itemIds.empty() && selectedGraphKind > 0 && selectedGraphIndex >= 0)
            itemIds.push_back (selectedGraphKind * 1000 + selectedGraphIndex + 1);

        if (itemIds.empty())
            return;

        std::sort (itemIds.begin(), itemIds.end());
        itemIds.erase (std::unique (itemIds.begin(), itemIds.end()), itemIds.end());

        auto eraseIndices = [&itemIds] (auto& list, int kind)
        {
            std::vector<int> indices;
            for (auto itemId : itemIds)
                if (itemId / 1000 == kind)
                    indices.push_back ((itemId % 1000) - 1);

            std::sort (indices.begin(), indices.end(), std::greater<int>());
            indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
            for (auto index : indices)
                if (index >= 0 && index < (int) list.size())
                    list.erase (list.begin() + index);
        };

        auto& graph = project.getDspGraph();
        eraseIndices (graph.automation, 4);
        eraseIndices (graph.modulation, 3);
        eraseIndices (graph.macros, 2);
        eraseIndices (graph.blocks, 1);
        markGraphEdited();

        selectedGraphKind = 0;
        selectedGraphIndex = -1;
        builderPanel.selectedItemId = 0;
        builderPanel.selectedItemIds.clear();
        project.notifyChanged();
        rebuildGraphEditorItems();
        refreshBuilderPanel();
    }

    void DspPage::clearCurrentBuilderSection()
    {
        const auto sectionId = currentSectionId();
        auto& graph = project.getDspGraph();
        graph.userConfigured = true;
        graph.blocks.erase (std::remove_if (graph.blocks.begin(), graph.blocks.end(),
            [&] (const DspBlock& block) { return block.section == sectionId; }), graph.blocks.end());

        if (sectionId == "mod")
        {
            graph.macros.clear();
            graph.modulation.clear();
            graph.automation.clear();
        }

        selectedGraphKind = 0;
        selectedGraphIndex = -1;
        builderPanel.selectedItemId = 0;
        builderPanel.selectedItemIds.clear();
        project.notifyChanged();
        rebuildGraphEditorItems();
        refreshBuilderPanel();
    }

    void DspPage::clearAllBuilderSections()
    {
        auto& graph = project.getDspGraph();
        graph.blocks.clear();
        graph.macros.clear();
        graph.modulation.clear();
        graph.automation.clear();
        graph.userConfigured = true;

        selectedGraphKind = 0;
        selectedGraphIndex = -1;
        builderPanel.selectedItemId = 0;
        builderPanel.selectedItemIds.clear();
        project.notifyChanged();
        rebuildGraphEditorItems();
        refreshBuilderPanel();
    }

    void DspPage::projectChanged()
    {
        refreshFxPreviewRouting();
        refresh();
    }

    void DspPage::projectChanged (PatchCraftProject::ChangeScope scope)
    {
        refreshFxPreviewRouting();
        if (scope == PatchCraftProject::ChangeScope::dspRealtime)
        {
            refreshBuilderPanel();
            surgicalEqPanel.repaint();
            formulaPanel.repaint();
            repaint();
            return;
        }

        refresh();
    }

    void DspPage::refreshFxPreviewRouting()
    {
        const juce::SpinLock::ScopedTryLockType lock (fxAudioLock);
        if (lock.isLocked())
        {
            fxRoutingEngine.bind (project.getDspGraph(), project.getParameters());
            fxRoutingEngine.prepare (RenderContext::forBlock (fxPreviewSampleRate,
                                                              fxPreviewBlockSize,
                                                              fxPreviewBlockSize,
                                                              0,
                                                              fxPreviewChannels,
                                                              120.0));
            fxRoutingEngine.syncFromLiveValues (project.getLiveValues());
        }
    }

    void DspPage::liveValueChanged (const juce::String& id, float value)
    {
        if (isShowing())
            for (auto* section : { &engineSection, &toneSection, &ampSection,
                                   &modSection, &fxSection, &outputSection })
                section->syncFromProject();
        const juce::SpinLock::ScopedTryLockType lock (fxAudioLock);
        if (lock.isLocked() && fxPreviewEngine != nullptr)
        {
            fxPreviewEngine->setParameter (id, value);
            fxRoutingEngine.setParameterValue (id, value);
        }
    }

    void DspPage::timerCallback()
    {
        updateEqAnalyzerBins();
        if (currentTab == 1 && eqAnalyzerToggle.getToggleState())
            surgicalEqPanel.repaint();

        if (fxPlaying.load())
            fxWaveform.repaint();
    }

    void DspPage::refresh()
    {
        ensureQuickEditControls();
        bindSections();
        syncEngineButtons();
        refreshExpansionChoices();
        refreshEasyModeSummary();
        refreshQuickEditSections();
        refreshBuilderPanel();
        rebuildGraphEditorItems();
        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
            section->syncFromProject();
        rebuildVisibility();
        repaint();
    }

    void DspPage::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0a0c10));
        if (quickEdit)
            return;

        auto drawSectionFrame = [&] (juce::Rectangle<int> bounds)
        {
            if (bounds.isEmpty())
                return;
            bounds = bounds.expanded (3).getIntersection (getLocalBounds().reduced (4));
            g.setColour (juce::Colour (0xff07090d));
            g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
            g.setColour (PatchCraftLookAndFeel::border().brighter (0.38f));
            g.drawRoundedRectangle (bounds.toFloat(), 8.0f, 1.2f);
        };

        drawSectionFrame (builderPanel.getBounds());
        drawSectionFrame (formulaPanel.getBounds());
        drawSectionFrame (sourceMatrix.getBounds());

        if (easyMode)
        {
            auto drawEasyCard = [&] (const juce::Label& label)
            {
                auto bounds = label.getBounds().expanded (12, 10);
                if (bounds.isEmpty())
                    return;
                g.setColour (juce::Colour (0xff0f131a));
                g.fillRoundedRectangle (bounds.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border().brighter (0.42f));
                g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 10.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.82f));
                g.fillRoundedRectangle (bounds.withHeight (3).toFloat(), 2.0f);
            };

            drawEasyCard (easyRecipeLabel);
            drawEasyCard (easyParametersLabel);
            drawEasyCard (easyWorkflowLabel);
            return;
        }

        auto inspector = editorTitle.getBounds();
        for (auto* component : { static_cast<juce::Component*> (&editorHint),
                                 static_cast<juce::Component*> (&editorItemBox),
                                 static_cast<juce::Component*> (&enableGraphItemButton),
                                 static_cast<juce::Component*> (&amountSlider),
                                 static_cast<juce::Component*> (&curveSlider),
                                 static_cast<juce::Component*> (&amountSwitch),
                                 static_cast<juce::Component*> (&curveSwitch) })
            if (component->isVisible())
                inspector = inspector.getUnion (component->getBounds());
        if (! inspector.isEmpty())
        {
            drawSectionFrame (inspector.expanded (8, 8));
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.75f));
            g.fillRoundedRectangle (inspector.expanded (8, 8).withHeight (2).toFloat(), 2.0f);
        }
    }

    void DspPage::mouseMove (const juce::MouseEvent& e)
    {
        const auto localEvent = e.getEventRelativeTo (this);
        const auto text = hoverHelpAt (localEvent.getPosition(), e.originalComponent);
        if (text.isNotEmpty())
            showHoverHelp (text, localEvent.getPosition());
        else
            hideHoverHelp();
    }

    void DspPage::mouseExit (const juce::MouseEvent&)
    {
        hideHoverHelp();
    }

    juce::String DspPage::hoverHelpForComponent (juce::Component* component)
    {
        auto shouldShow = [] (const juce::String& text, const juce::Component* c)
        {
            juce::ignoreUnused (c);
            if (text.isEmpty())
                return false;

            return true;
        };

        for (auto* current = component; current != nullptr; current = current->getParentComponent())
        {
            if (current == &hoverHelpLabel)
                return {};

            if (auto* tooltip = dynamic_cast<juce::TooltipClient*> (current))
            {
                const auto text = tooltip->getTooltip();
                if (shouldShow (text, current))
                    return text;
            }

            if (current == this)
                break;
        }

        return {};
    }

    juce::String DspPage::hoverHelpAt (juce::Point<int> localPos, juce::Component* originalComponent)
    {
        if (! project.getManifest().playerShowParameterGuidance)
            return {};

        if (quickEdit)
            return hoverHelpForComponent (originalComponent);

        if (auto text = hoverHelpForComponent (originalComponent); text.isNotEmpty())
            return text;

        auto tooltipText = [] (juce::Component& component)
        {
            if (auto* tooltip = dynamic_cast<juce::TooltipClient*> (&component))
                return tooltip->getTooltip();
            return juce::String();
        };

        auto ifOver = [&] (juce::Component& component)
        {
            if (! component.isVisible() || ! component.getBounds().contains (localPos))
                return juce::String();

            const auto text = tooltipText (component);
            if (text.isNotEmpty())
                return text;

            return juce::String();
        };

        for (auto* component : { static_cast<juce::Component*> (&typeBox),
                                 static_cast<juce::Component*> (&sourceBox),
                                 static_cast<juce::Component*> (&targetBox),
                                 static_cast<juce::Component*> (&deleteGraphItemButton),
                                 static_cast<juce::Component*> (&enableGraphItemButton),
                                 static_cast<juce::Component*> (&amountSlider),
                                 static_cast<juce::Component*> (&rateSlider),
                                  static_cast<juce::Component*> (&valueSlider),
                                  static_cast<juce::Component*> (&minSlider),
                                  static_cast<juce::Component*> (&maxSlider),
                                  static_cast<juce::Component*> (&curveSlider),
                                  static_cast<juce::Component*> (&amountSwitch),
                                  static_cast<juce::Component*> (&rateSwitch),
                                  static_cast<juce::Component*> (&valueSwitch),
                                  static_cast<juce::Component*> (&minSwitch),
                                  static_cast<juce::Component*> (&maxSwitch),
                                  static_cast<juce::Component*> (&curveSwitch) })
            if (auto text = ifOver (*component); text.isNotEmpty())
                return text;

        struct LabelTarget
        {
            juce::Label* label = nullptr;
            juce::Component* control = nullptr;
        };

        std::array<LabelTarget, 6> labelTargets {
            LabelTarget { &amountLabel, graphControlIsSwitch[0] ? static_cast<juce::Component*> (&amountSwitch) : static_cast<juce::Component*> (&amountSlider) },
            LabelTarget { &rateLabel,   graphControlIsSwitch[1] ? static_cast<juce::Component*> (&rateSwitch)   : static_cast<juce::Component*> (&rateSlider) },
            LabelTarget { &valueLabel,  graphControlIsSwitch[2] ? static_cast<juce::Component*> (&valueSwitch)  : static_cast<juce::Component*> (&valueSlider) },
            LabelTarget { &minLabel,    graphControlIsSwitch[3] ? static_cast<juce::Component*> (&minSwitch)    : static_cast<juce::Component*> (&minSlider) },
            LabelTarget { &maxLabel,    graphControlIsSwitch[4] ? static_cast<juce::Component*> (&maxSwitch)    : static_cast<juce::Component*> (&maxSlider) },
            LabelTarget { &curveLabel,  graphControlIsSwitch[5] ? static_cast<juce::Component*> (&curveSwitch)  : static_cast<juce::Component*> (&curveSlider) }
        };
        for (auto target : labelTargets)
        {
            if (target.label != nullptr && target.control != nullptr
                && target.label->isVisible() && target.label->getBounds().contains (localPos))
            {
                const auto text = tooltipText (*target.control);
                if (text.isNotEmpty() && (! target.control->isEnabled()
                    || text.startsWithIgnoreCase ("Disabled")
                    || text.startsWithIgnoreCase ("Enabled")
                    || text.startsWithIgnoreCase ("Select")
                    || text.containsIgnoreCase ("not connected")))
                    return text;
            }
        }

        if (! quickEdit && editorHint.isVisible() && editorHint.getBounds().contains (localPos))
            return editorHint.getText();

        return {};
    }

    void DspPage::showHoverHelp (juce::String text, juce::Point<int> localPos)
    {
        if (text == hoverHelpText && hoverHelpLabel.isVisible())
        {
            hoverHelpLabel.toFront (false);
            return;
        }

        hoverHelpText = std::move (text);
        hoverHelpLabel.setText ("  " + hoverHelpText, juce::dontSendNotification);

        constexpr int width = 390;
        constexpr int height = 68;
        int x = localPos.x + 18;
        int y = localPos.y + 18;
        if (x + width > getWidth() - 8)
            x = localPos.x - width - 18;
        if (y + height > getHeight() - 8)
            y = localPos.y - height - 14;

        hoverHelpLabel.setBounds (juce::jmax (8, x), juce::jmax (8, y), width, height);
        hoverHelpLabel.setVisible (true);
        hoverHelpLabel.toFront (false);
    }

    void DspPage::hideHoverHelp()
    {
        hoverHelpText.clear();
        hoverHelpLabel.setVisible (false);
    }

    void DspPage::resized()
    {
        auto r = getLocalBounds().reduced (quickEdit ? 8 : 12);

        auto header = r.removeFromTop (quickEdit ? 24 : 32);
        titleLabel.setBounds (header.removeFromLeft (quickEdit ? 46 : 128));
        subtitleLabel.setBounds (header.removeFromLeft (quickEdit ? 210 : 280));
        samplerEngineButton.setBounds (header.removeFromLeft (72).reduced (2));
        synthEngineButton.setBounds (header.removeFromLeft (62).reduced (2));
        fxEngineButton.setBounds (header.removeFromLeft (50).reduced (2));
        if (! quickEdit)
        {
            advancedModeButton.setBounds (header.removeFromRight (96).reduced (2));
            easyModeButton.setBounds (header.removeFromRight (70).reduced (2));
        }

        if (quickEdit || ! easyMode)
        {
            auto tabs = r.removeFromTop (quickEdit ? 24 : 30);
            const int tabW = tabs.getWidth() / 6;
            tabEngine.setBounds (tabs.removeFromLeft (tabW));
            tabTone.setBounds   (tabs.removeFromLeft (tabW));
            tabAmp.setBounds    (tabs.removeFromLeft (tabW));
            tabMod.setBounds    (tabs.removeFromLeft (tabW));
            tabFx.setBounds     (tabs.removeFromLeft (tabW));
            tabOut.setBounds    (tabs);
        }
        else
        {
            for (auto* tab : { &tabEngine, &tabTone, &tabAmp, &tabMod, &tabFx, &tabOut })
                tab->setBounds ({});
        }

        r.removeFromTop (4);
        auto content = r.reduced (0, 1);
        if (! quickEdit && easyMode)
        {
            auto easy = content.reduced (18, 14);
            easyTitleLabel.setBounds (easy.removeFromTop (28));
            auto help = easy.removeFromTop (38);
            easyHelpLabel.setBounds (help.reduced (0, 2));

            auto chooser = easy.removeFromTop (46);
            easyThemeBox.setBounds (chooser.removeFromLeft (210).reduced (2));
            easyGenerateButton.setBounds (chooser.removeFromLeft (132).reduced (2));
            easyRandomButton.setBounds (chooser.removeFromLeft (148).reduced (2));
            easyAddToPackToggle.setBounds (chooser.removeFromLeft (118).reduced (2));
            easyExpansionBox.setBounds (chooser.removeFromLeft (210).reduced (2));
            easyPackCreatorButton.setBounds (chooser.removeFromLeft (124).reduced (2));
            easyAdvancedButton.setBounds (chooser.removeFromLeft (132).reduced (2));
            easy.removeFromTop (14);

            auto cards = easy.removeFromTop (juce::jlimit (210, 330, easy.getHeight() / 2));
            const int gap = 12;
            const int cardW = juce::jmax (220, (cards.getWidth() - gap * 2) / 3);
            easyRecipeLabel.setBounds (cards.removeFromLeft (cardW).reduced (12, 10));
            cards.removeFromLeft (gap);
            easyParametersLabel.setBounds (cards.removeFromLeft (cardW).reduced (12, 10));
            cards.removeFromLeft (gap);
            easyWorkflowLabel.setBounds (cards.reduced (12, 10));
            content = {};
            builderPanel.setBounds ({});
            formulaPanel.setBounds ({});
            sourceMatrix.setBounds ({});
            surgicalEqPanel.setBounds ({});
            wavetableEditor.setBounds ({});
            modMatrix.setBounds ({});
        }
        else if (quickEdit)
        {
            builderPanel.setBounds (content);
            formulaPanel.setBounds ({});
        }
        else
        {
            const bool showFxTrack = currentTab == 4;
            const bool showEqEditor = currentTab == 1;
            const bool showWtEditor = currentTab == 0 && project.getEngineType() == "synth";
            const int fxTrackH = showFxTrack ? 164 : 0;
            const int eqEditorReserve = showEqEditor ? 178 : 0;
            const int wtEditorReserve = showWtEditor ? 162 : 0;
            const int editorReserve = 172;
            const int formulaReserve = content.getHeight() > 520 ? 128 : 0;
            const int cardColumns = juce::jmax (1, juce::jmin (3, content.getWidth() / 260));
            const int cardRows = juce::jmax (1, (builderPanel.cards.size() + cardColumns - 1) / cardColumns);
            const int desiredBuilderH = juce::jlimit (240, 430,
                                                      112 + cardRows * 104 + (builderPanel.showSectionBanks ? 34 : 0));
            const int maxBuilderH = juce::jmax (220, content.getHeight() - fxTrackH - formulaReserve - eqEditorReserve - wtEditorReserve - editorReserve - 24);
            const int builderH = juce::jmin (maxBuilderH, juce::jmax (220, desiredBuilderH));
            auto builder = content.removeFromTop (builderH);
            builderPanel.setBounds (builder);
            content.removeFromTop (8);

            if (formulaReserve > 0)
            {
                formulaPanel.setBounds (content.removeFromTop (formulaReserve).reduced (4, 0));
                content.removeFromTop (8);
            }
            else
            {
                formulaPanel.setBounds ({});
            }

            if (showEqEditor)
            {
                const int eqAvailable = content.getHeight() - fxTrackH - editorReserve - 16;
                const int eqH = eqAvailable > 124 ? juce::jlimit (140, 220, eqAvailable) : 0;
                auto eqArea = eqH > 0 ? content.removeFromTop (eqH) : juce::Rectangle<int>();
                if (eqH > 0)
                {
                    auto eqControls = eqArea.removeFromTop (28);
                    eqAnalyzerToggle.setBounds (eqControls.removeFromRight (96).reduced (2));
                    eqAnalyzerFreezeToggle.setBounds (eqControls.removeFromRight (86).reduced (2));
                    eqBandInsertButton.setBounds (eqControls.removeFromRight (100).reduced (2));
                    eqBandSaveButton.setBounds (eqControls.removeFromRight (88).reduced (2));
                    eqBandPasteButton.setBounds (eqControls.removeFromRight (92).reduced (2));
                    eqBandCopyButton.setBounds (eqControls.removeFromRight (92).reduced (2));
                    surgicalEqPanel.setBounds (eqArea);
                }
                else
                {
                    surgicalEqPanel.setBounds ({});
                }
                if (eqH > 0)
                    content.removeFromTop (8);
            }
            else
            {
                surgicalEqPanel.setBounds ({});
            }

            if (showWtEditor)
            {
                const int wtAvailable = content.getHeight() - fxTrackH - editorReserve - 16;
                const int wtH = wtAvailable > 118 ? juce::jlimit (128, 200, wtAvailable) : 0;
                auto wtArea = wtH > 0 ? content.removeFromTop (wtH) : juce::Rectangle<int>();
                if (wtH > 0)
                {
                    auto wtControls = wtArea.removeFromTop (28);
                    wtImportButton.setBounds (wtControls.removeFromRight (92).reduced (2));
                    wtNormalizeButton.setBounds (wtControls.removeFromRight (92).reduced (2));
                    wtSineButton.setBounds (wtControls.removeFromRight (70).reduced (2));
                    wavetableEditor.setBounds (wtArea);
                    content.removeFromTop (8);
                }
                else
                {
                    wavetableEditor.setBounds ({});
                }
            }
            else
            {
                wavetableEditor.setBounds ({});
            }

            sourceMatrix.setBounds ({});
            modMatrix.setBounds ({});

            if (showFxTrack)
            {
                auto track = content.removeFromTop (154).reduced (4, 0);
                auto controls = track.removeFromTop (26);
                fxTrackNameLabel.setBounds (controls.removeFromLeft (240));
                fxTrackStatusLabel.setBounds (controls.removeFromLeft (260));
                fxTrackImportButton.setBounds (controls.removeFromRight (104).reduced (2));
                fxTrackUseMapperButton.setBounds (controls.removeFromRight (92).reduced (2));
                fxTrackLiveInputToggle.setBounds (controls.removeFromRight (92).reduced (2));
                fxTrackRetriggerToggle.setBounds (controls.removeFromRight (88).reduced (2));
                fxTrackLoopToggle.setBounds (controls.removeFromRight (70).reduced (2));
                fxTrackStopButton.setBounds (controls.removeFromRight (62).reduced (2));
                fxTrackPlayButton.setBounds (controls.removeFromRight (62).reduced (2));
                auto options = track.removeFromTop (30);
                fxTrackMonitorBox.setBounds (options.removeFromLeft (96).reduced (2));
                fxTrackGainSlider.setBounds (options.removeFromLeft (170).reduced (2));
                fxTrackLoopStartSlider.setBounds (options.removeFromLeft (170).reduced (2));
                fxTrackLoopEndSlider.setBounds (options.removeFromLeft (170).reduced (2));
                auto chop = track.removeFromTop (30);
                fxTrackSliceBox.setBounds (chop.removeFromLeft (112).reduced (2));
                fxTrackTriggerBox.setBounds (chop.removeFromLeft (124).reduced (2));
                fxTrackPrevSliceButton.setBounds (chop.removeFromLeft (82).reduced (2));
                fxTrackNextSliceButton.setBounds (chop.removeFromLeft (82).reduced (2));
                fxWaveform.setBounds (track);
                content.removeFromTop (8);
            }

            const int inspectorH = juce::jmin (editorReserve, juce::jmax (0, content.getHeight()));
            auto editor = content.removeFromBottom (inspectorH).reduced (4);
            auto top = editor.removeFromTop (22);
            editorTitle.setBounds (top.removeFromLeft (120));
            editorHint.setBounds (top);

            auto row = editor.removeFromTop (28);
            editorItemBox.setBounds (row.removeFromLeft (220).reduced (2));
            typeBox.setBounds (row.removeFromLeft (120).reduced (2));
            sourceBox.setBounds (row.removeFromLeft (190).reduced (2));
            targetBox.setBounds (row.removeFromLeft (190).reduced (2));
            globalPresetBox.setBounds (row.removeFromLeft (160).reduced (2));
            sectionPresetBox.setBounds (row.removeFromLeft (150).reduced (2));
            deleteGraphItemButton.setBounds (row.removeFromLeft (70).reduced (2));
            enableGraphItemButton.setBounds (row.removeFromLeft (88).reduced (2));

            editor.removeFromTop (4);
            auto labelRow = editor.removeFromTop (22);
            constexpr int controlCount = 6;
            const int slotW = juce::jmax (64, editor.getWidth() / controlCount);
            const int knobSize = juce::jlimit (50, 68, juce::jmin (slotW - 12, editor.getHeight() - 8));

            auto placeEditorControl = [&] (juce::Label& label, juce::Slider& slider, juce::ToggleButton& button, int index)
            {
                auto labelSlot = juce::Rectangle<int> (labelRow.getX() + index * slotW,
                                                       labelRow.getY(),
                                                       index == controlCount - 1 ? labelRow.getRight() - (labelRow.getX() + index * slotW) : slotW,
                                                       labelRow.getHeight()).reduced (4, 0);
                label.setBounds (labelSlot);

                auto slot = juce::Rectangle<int> (editor.getX() + index * slotW,
                                                  editor.getY(),
                                                  index == controlCount - 1 ? editor.getRight() - (editor.getX() + index * slotW) : slotW,
                                                  editor.getHeight());
                slider.setBounds (slot.withSizeKeepingCentre (knobSize, knobSize));
                button.setBounds (slot.withSizeKeepingCentre (juce::jlimit (58, 88, slotW - 14), 28));
            };

            placeEditorControl (amountLabel, amountSlider, amountSwitch, 0);
            placeEditorControl (rateLabel, rateSlider, rateSwitch, 1);
            placeEditorControl (valueLabel, valueSlider, valueSwitch, 2);
            placeEditorControl (minLabel, minSlider, minSwitch, 3);
            placeEditorControl (maxLabel, maxSlider, maxSwitch, 4);
            placeEditorControl (curveLabel, curveSlider, curveSwitch, 5);
        }
        for (auto* section : { &engineSection, &toneSection, &ampSection,
                               &modSection, &fxSection, &outputSection })
            section->setBounds (content);
        hoverHelpLabel.toFront (false);
        if (guidedTutorial != nullptr)
        {
            guidedTutorial->setBounds (getLocalBounds());
            guidedTutorial->toFront (false);
        }
    }
}
