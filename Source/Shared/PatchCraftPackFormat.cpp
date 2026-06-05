#include "PatchCraftPackFormat.h"

#include <array>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        static juce::String makeStableContentId (juce::String name)
        {
            name = name.trim().toLowerCase();
            if (name.isEmpty())
                name = "untitled";

            juce::String out;
            for (int i = 0; i < name.length(); ++i)
            {
                const auto c = name[i];
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                    out += c;
                else if (c == ' ' || c == '-' || c == '_')
                    out += '_';
            }

            while (out.contains ("__"))
                out = out.replace ("__", "_");
            out = out.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
            return out.isNotEmpty() ? out : "untitled";
        }

        static juce::String presetSearchText (const Preset& preset)
        {
            juce::String text = preset.name + " " + preset.theme + " " + preset.description;
            for (const auto& tag : preset.tags)
                text << " " << tag;
            return text.toLowerCase();
        }

        static bool isArpPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("arp") || text.contains ("arpeggio") || text.contains ("sequence") || text.contains ("steps");
        }

        static bool isStringPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("string") || text.contains ("violin") || text.contains ("cello")
                || text.contains ("orchestral") || text.contains ("choir") || text.contains ("vocal");
        }

        static bool isPadPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("pad") || text.contains ("bloom") || text.contains ("ambient")
                || text.contains ("drone") || text.contains ("cloud") || text.contains ("evolve")
                || text.contains ("horizon") || text.contains ("space");
        }

        static bool isBassPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("bass") || text.contains ("reese") || text.contains ("sub")
                || text.contains ("acid") || text.contains ("wobble");
        }

        static bool isPluckPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("pluck") || text.contains ("stab") || text.contains ("bell")
                || text.contains ("keys");
        }

        static bool isMotionPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("motion") || text.contains ("lfo") || text.contains ("mod")
                || text.contains ("stutter") || text.contains ("texture") || text.contains ("gate");
        }

        static bool isLeadPreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("lead") || text.contains ("supersaw") || text.contains ("saw")
                || text.contains ("hoover") || text.contains ("goa") || text.contains ("festival")
                || text.contains ("hands up") || text.contains ("stadium");
        }

        static bool isWavetablePreset (const Preset& preset)
        {
            const auto text = presetSearchText (preset);
            return text.contains ("wavetable") || text.contains ("wt ") || text.startsWith ("wt")
                || text.contains ("glass") || text.contains ("razor") || text.contains ("fold");
        }

        static DspBlock& ensureArpBlock (DspGraph& graph)
        {
            for (auto& block : graph.blocks)
            {
                const auto type = block.type.trim().toLowerCase();
                if (type == "arp" || type == "arpeggiator" || type == "arpsequencer"
                    || type == "arpstepsequencer" || type == "arp step sequencer")
                    return block;
            }

            DspBlock block;
            block.id = "preset_arp";
            block.section = "mod";
            block.type = "arpStepSequencer";
            block.name = "Preset Step Arp";
            block.enabled = true;
            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
            return graph.blocks.back();
        }

        static void applyArpRecipe (DspGraph& graph, Preset& preset, int index)
        {
            static constexpr std::array<std::array<float, 16>, 6> patterns {{
                {{ 0, 1, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 4, 3, 2 }},
                {{ 0, 2, 4, 7, 12, 7, 4, 2, 0, 3, 7, 10, 15, 10, 7, 3 }},
                {{ 0, 7, 12, 16, 14, 12, 7, 4, 0, 4, 7, 11, 12, 16, 19, 24 }},
                {{ 0, 12, 7, 19, 3, 15, 10, 22, 0, 10, 5, 17, 7, 19, 12, 24 }},
                {{ 0, 0, 7, 0, 12, 0, 7, 0, 3, 0, 10, 0, 15, 0, 10, 0 }},
                {{ 0, 4, 7, 11, 14, 11, 7, 4, 2, 5, 9, 12, 17, 12, 9, 5 }}
            }};
            static constexpr float rates[]    { 1.00f, 1.50f, 2.00f, 0.75f, 3.00f, 0.50f };
            static constexpr float gates[]    { 0.48f, 0.36f, 0.58f, 0.42f, 0.28f, 0.70f };
            static constexpr float patternsId[] { 0.0f, 2.0f, 4.0f, 5.0f, 3.0f, 1.0f };
            const int recipe = index % (int) patterns.size();

            auto& block = ensureArpBlock (graph);
            block.name = "Preset Step Arp - " + preset.name;
            block.enabled = true;
            block.values["sync"] = 1.0f;
            block.values["rate"] = rates[recipe];
            block.values["arpSteps"] = 16.0f;
            block.values["arpPattern"] = patternsId[recipe];
            block.values["arpGate"] = gates[recipe];
            block.values["arpOctaves"] = recipe == 5 ? 1.0f : 2.0f;
            for (int step = 0; step < 16; ++step)
                block.values["arpNote" + juce::String (step)] = patterns[(size_t) recipe][(size_t) step];

            preset.values["attack"] = 0.002f;
            preset.values["decay"] = 0.12f + 0.025f * (float) recipe;
            preset.values["sustain"] = recipe == 5 ? 0.32f : 0.10f;
            preset.values["release"] = 0.12f + 0.04f * (float) recipe;
            preset.values["delayMix"] = 0.10f + 0.025f * (float) recipe;
            preset.values["delayFeedback"] = 0.20f + 0.035f * (float) recipe;
            preset.values["bpmSync"] = 1.0f;
            preset.values["retrigger"] = 1.0f;
            preset.tags.addIfNotAlreadyThere ("true-arp");
            graph.userConfigured = true;
        }

        static void applyStringRecipe (DspGraph& graph, Preset& preset, int index)
        {
            const float variant = (float) (index % 4);
            preset.values["oscType"] = index % 2 == 0 ? 3.0f : 1.0f;
            preset.values["osc2Type"] = index % 2 == 0 ? 1.0f : 3.0f;
            preset.values["oscBlend"] = 0.36f + 0.08f * variant;
            preset.values["detune"] = 7.0f + 3.0f * variant;
            preset.values["osc2Detune"] = -5.0f - 2.0f * variant;
            preset.values["attack"] = 0.55f + 0.18f * variant;
            preset.values["decay"] = 1.20f + 0.35f * variant;
            preset.values["sustain"] = 0.78f + 0.03f * variant;
            preset.values["release"] = 2.20f + 0.55f * variant;
            preset.values["filterCutoff"] = 2600.0f + 700.0f * variant;
            preset.values["filterResonance"] = 0.10f + 0.025f * variant;
            preset.values["vibratoDepth"] = 0.10f + 0.03f * variant;
            preset.values["vibratoRate"] = 4.6f + 0.25f * variant;
            preset.values["reverbMix"] = 0.52f + 0.06f * variant;
            preset.values["delayMix"] = 0.08f + 0.02f * variant;

            for (auto& block : graph.blocks)
            {
                if (block.id == "lfo_1" || block.type.containsIgnoreCase ("lfo"))
                {
                    block.name = "String Bow Motion";
                    block.targetId = "filterCutoff";
                    block.values["rate"] = 0.18f + 0.04f * variant;
                    block.values["sync"] = 1.0f;
                    block.values["amount"] = 0.08f + 0.03f * variant;
                    break;
                }
            }
            preset.tags.addIfNotAlreadyThere ("string-like");
        }

        static void applyPadRecipe (DspGraph&, Preset& preset, int index)
        {
            const float variant = (float) (index % 5);
            preset.values["oscType"] = index % 2 == 0 ? 1.0f : 3.0f;
            preset.values["osc2Type"] = index % 3 == 0 ? 0.0f : 1.0f;
            preset.values["oscBlend"] = 0.34f + 0.06f * variant;
            preset.values["detune"] = 8.0f + 4.0f * variant;
            preset.values["osc2Detune"] = -5.0f - 3.0f * variant;
            preset.values["attack"] = 0.65f + 0.42f * variant;
            preset.values["decay"] = 1.40f + 0.55f * variant;
            preset.values["sustain"] = 0.78f + 0.035f * (float) (index % 3);
            preset.values["release"] = 2.60f + 0.85f * variant;
            preset.values["filterCutoff"] = 2400.0f + 760.0f * variant;
            preset.values["filterResonance"] = 0.09f + 0.035f * variant;
            preset.values["lfoRate"] = 0.12f + 0.08f * variant;
            preset.values["lfoAmount"] = 0.12f + 0.08f * variant;
            preset.values["delayMix"] = 0.14f + 0.035f * variant;
            preset.values["reverbMix"] = 0.48f + 0.07f * variant;
            preset.tags.addIfNotAlreadyThere ("pad-like");
        }

        static void applyBassRecipe (DspGraph&, Preset& preset, int index)
        {
            const int variant = index % 5;
            preset.values["oscType"] = variant == 2 ? 1.0f : 2.0f;
            preset.values["osc2Type"] = 0.0f;
            preset.values["octave"] = variant == 0 ? -2.0f : -1.0f;
            preset.values["oscBlend"] = 0.10f + 0.06f * (float) variant;
            preset.values["subBlend"] = 0.52f + 0.08f * (float) (variant % 3);
            preset.values["noiseBlend"] = 0.0f;
            preset.values["attack"] = 0.001f;
            preset.values["decay"] = variant == 1 ? 0.20f : 0.10f + 0.05f * (float) variant;
            preset.values["sustain"] = variant == 2 ? 0.82f : 0.0f + 0.12f * (float) (variant % 2);
            preset.values["release"] = 0.08f + 0.06f * (float) variant;
            preset.values["filterCutoff"] = 520.0f + 360.0f * (float) variant;
            preset.values["filterResonance"] = 0.32f + 0.09f * (float) variant;
            preset.values["lfoRate"] = variant == 4 ? 3.0f : 0.5f;
            preset.values["lfoAmount"] = variant == 4 ? 0.75f : 0.05f * (float) variant;
            preset.values["delayMix"] = variant >= 3 ? 0.08f : 0.0f;
            preset.values["reverbMix"] = 0.03f + 0.02f * (float) variant;
            preset.tags.addIfNotAlreadyThere ("bass-like");
        }

        static void applyPluckRecipe (DspGraph&, Preset& preset, int index)
        {
            const int variant = index % 5;
            preset.values["oscType"] = variant == 3 ? 0.0f : (variant % 2 == 0 ? 3.0f : 1.0f);
            preset.values["osc2Type"] = variant == 4 ? 2.0f : 1.0f;
            preset.values["oscBlend"] = 0.18f + 0.07f * (float) variant;
            preset.values["attack"] = 0.001f;
            preset.values["decay"] = 0.08f + 0.06f * (float) variant;
            preset.values["sustain"] = variant == 4 ? 0.30f : 0.0f;
            preset.values["release"] = 0.10f + 0.08f * (float) variant;
            preset.values["filterCutoff"] = 3200.0f + 850.0f * (float) variant;
            preset.values["filterResonance"] = 0.20f + 0.055f * (float) variant;
            preset.values["delayTime"] = variant % 2 == 0 ? 0.1875f : 0.25f;
            preset.values["delayFeedback"] = 0.28f + 0.06f * (float) variant;
            preset.values["delayMix"] = 0.06f + 0.025f * (float) variant;
            preset.values["delayFeedback"] = 0.18f + 0.035f * (float) variant;
            preset.values["reverbMix"] = 0.20f + 0.055f * (float) variant;
            preset.values["retrigger"] = 1.0f;
            preset.tags.addIfNotAlreadyThere ("pluck-like");
        }

        static void applyLeadRecipe (DspGraph&, Preset& preset, int index)
        {
            const int variant = index % 6;
            preset.values["oscType"] = variant == 3 ? 2.0f : 1.0f;
            preset.values["osc2Type"] = variant == 4 ? 3.0f : 1.0f;
            preset.values["oscBlend"] = 0.24f + 0.055f * (float) variant;
            preset.values["detune"] = 5.0f + 3.5f * (float) variant;
            preset.values["osc2Detune"] = -4.0f + 2.0f * (float) variant;
            preset.values["attack"] = variant == 5 ? 0.012f : 0.002f;
            preset.values["decay"] = 0.18f + 0.07f * (float) variant;
            preset.values["sustain"] = 0.62f + 0.045f * (float) (variant % 4);
            preset.values["release"] = 0.18f + 0.11f * (float) variant;
            preset.values["filterCutoff"] = 4200.0f + 1000.0f * (float) variant;
            preset.values["filterResonance"] = 0.18f + 0.045f * (float) variant;
            preset.values["lfoRate"] = variant == 2 ? 5.5f : 0.5f + 0.25f * (float) variant;
            preset.values["lfoAmount"] = variant == 2 ? 0.18f : 0.04f + 0.025f * (float) variant;
            preset.values["delayTime"] = variant % 2 == 0 ? 0.125f : 0.1875f;
            preset.values["delayMix"] = 0.04f + 0.025f * (float) (variant % 4);
            preset.values["delayFeedback"] = 0.14f + 0.035f * (float) variant;
            preset.values["reverbMix"] = 0.12f + 0.035f * (float) variant;
            preset.values["retrigger"] = 1.0f;
            preset.tags.addIfNotAlreadyThere ("lead-like");
            preset.tags.addIfNotAlreadyThere ("crisp");
        }

        static void sanitisePresetPlaybackValues (Preset& preset)
        {
            preset.values["noiseBlend"] = 0.0f;

            for (const auto& id : { juce::String ("oscType"), juce::String ("osc2Type") })
            {
                auto it = preset.values.find (id);
                if (it == preset.values.end())
                    continue;

                it->second = it->second < 0.5f ? 0.0f : 1.0f;
            }

            if (auto blend = preset.values.find ("oscBlend"); blend != preset.values.end())
                blend->second = juce::jlimit (0.0f, 0.24f, blend->second);
            if (auto detune = preset.values.find ("detune"); detune != preset.values.end())
                detune->second = juce::jlimit (-8.0f, 8.0f, detune->second);
            if (auto detune2 = preset.values.find ("osc2Detune"); detune2 != preset.values.end())
                detune2->second = juce::jlimit (-6.0f, 6.0f, detune2->second);
            if (auto resonance = preset.values.find ("filterResonance"); resonance != preset.values.end())
                resonance->second = juce::jlimit (0.0f, 0.32f, resonance->second);
            if (auto feedback = preset.values.find ("delayFeedback"); feedback != preset.values.end())
                feedback->second = juce::jlimit (0.0f, 0.45f, feedback->second);
            if (auto reverb = preset.values.find ("reverbMix"); reverb != preset.values.end())
                reverb->second = juce::jlimit (0.0f, 0.40f, reverb->second);
            if (auto wtWarp = preset.values.find ("wtWarp"); wtWarp != preset.values.end())
                wtWarp->second = juce::jlimit (0.0f, 0.35f, wtWarp->second);
            if (auto wtFold = preset.values.find ("wtFold"); wtFold != preset.values.end())
                wtFold->second = juce::jlimit (0.0f, 0.20f, wtFold->second);
            if (auto wtLevel = preset.values.find ("wtLevel"); wtLevel != preset.values.end())
                wtLevel->second = juce::jlimit (0.0f, 0.78f, wtLevel->second);
        }

        static void applyMotionRecipe (DspGraph&, Preset& preset, int index)
        {
            const int variant = index % 6;
            preset.values["lfoRate"] = (float) (variant + 1) * 0.75f;
            preset.values["lfoAmount"] = 0.26f + 0.10f * (float) variant;
            preset.values["filterCutoff"] = 1800.0f + 900.0f * (float) variant;
            preset.values["filterResonance"] = 0.22f + 0.06f * (float) variant;
            preset.values["attack"] = variant < 2 ? 0.002f : 0.12f + 0.08f * (float) variant;
            preset.values["decay"] = 0.18f + 0.08f * (float) variant;
            preset.values["sustain"] = 0.24f + 0.09f * (float) variant;
            preset.values["release"] = 0.20f + 0.12f * (float) variant;
            preset.values["delayMix"] = 0.04f + 0.025f * (float) variant;
            preset.values["delayFeedback"] = 0.16f + 0.035f * (float) variant;
            preset.values["reverbMix"] = 0.10f + 0.035f * (float) variant;
            preset.tags.addIfNotAlreadyThere ("motion-like");
        }

        static void applyWavetableRecipe (DspGraph&, Preset& preset, int index)
        {
            const int variant = index % 8;
            preset.values["wtEnabled"] = 1.0f;
            preset.values["oscType"] = 1.0f;
            preset.values["osc2Type"] = 3.0f;
            preset.values["oscBlend"] = 0.24f;
            preset.values["noiseBlend"] = 0.0f;
            preset.values["wtTable"] = (float) variant;
            preset.values["wtPosition"] = 0.12f + 0.10f * (float) variant;
            preset.values["wtMorph"] = 0.25f + 0.07f * (float) variant;
            preset.values["wtWarp"] = 0.10f + 0.06f * (float) (variant % 5);
            preset.values["wtFold"] = 0.04f + 0.055f * (float) variant;
            preset.values["detune"] = 5.0f + 3.0f * (float) variant;
            preset.values["filterCutoff"] = 1600.0f + 1200.0f * (float) variant;
            preset.values["filterResonance"] = 0.16f + 0.045f * (float) variant;
            preset.values["lfoRate"] = 0.25f + 0.20f * (float) variant;
            preset.values["lfoAmount"] = 0.14f + 0.06f * (float) variant;
            preset.tags.addIfNotAlreadyThere ("wavetable-like");
        }

        static void syncPresetValuesIntoGraph (DspGraph& graph, const Preset& preset, const ParameterModel& model)
        {
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
                        block.values["wtLevel"] = juce::jlimit (0.0f, 0.92f, raw ("wtLevel", block.values.count ("wtLevel") ? block.values["wtLevel"] : 0.72f));
                        block.values["wtUnison"] = raw ("wtUnison", block.values.count ("wtUnison") ? block.values["wtUnison"] : 1.0f);
                        block.values["wtDetune"] = raw ("wtDetune", block.values.count ("wtDetune") ? block.values["wtDetune"] : 12.0f);
                    }
                else
                {
                    block.targetId = block.type.containsIgnoreCase ("noise") ? "noiseBlend" : "oscBlend";
                    const auto safeOscNorm = [] (float value01)
                    {
                        return value01 < 0.125f ? 0.0f : 0.25f;
                    };
                    block.values["oscType"] = safeOscNorm (normalised ("oscType", block.values.count ("oscType") ? block.values["oscType"] : 0.25f));
                    block.values["osc2Type"] = safeOscNorm (normalised ("osc2Type", block.values.count ("osc2Type") ? block.values["osc2Type"] : 0.25f));
                    block.values["oscBlend"] = juce::jmin (0.24f, normalised ("oscBlend", block.values.count ("oscBlend") ? block.values["oscBlend"] : 0.0f));
                        block.values["osc2Detune"] = juce::jlimit (0.44f, 0.56f, normalised ("osc2Detune", block.values.count ("osc2Detune") ? block.values["osc2Detune"] : 0.50f));
                        block.values["detune"] = juce::jlimit (0.42f, 0.58f, normalised ("detune", block.values.count ("detune") ? block.values["detune"] : 0.50f));
                        block.values["octave"] = normalised ("octave", block.values.count ("octave") ? block.values["octave"] : 0.50f);
                        block.values["subBlend"] = normalised ("subBlend", block.values.count ("subBlend") ? block.values["subBlend"] : 0.0f);
                        block.values["noiseBlend"] = 0.0f;
                        block.values["volume"] = juce::jmin (0.72f, normalised ("volume", block.values.count ("volume") ? block.values["volume"] : 0.62f));
                    }
                }
                else if (block.section == "filter" && ! block.type.containsIgnoreCase ("eq"))
                {
                    block.targetId = "filterCutoff";
                    block.values["cutoff"] = normalised ("filterCutoff", block.values.count ("cutoff") ? block.values["cutoff"] : 0.50f);
                    block.values["resonance"] = juce::jmin (0.32f, normalised ("filterResonance", block.values.count ("resonance") ? block.values["resonance"] : 0.20f));
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
                        block.values["delayMix"] = normalised ("delayMix", block.values.count ("delayMix") ? block.values["delayMix"] : 0.16f);
                        block.values["delayFeedback"] = juce::jmin (0.45f, normalised ("delayFeedback", block.values.count ("delayFeedback") ? block.values["delayFeedback"] : 0.32f));
                        block.values["delayTime"] = normalised ("delayTime", block.values.count ("delayTime") ? block.values["delayTime"] : 0.25f);
                        block.values["sync"] = raw ("bpmSync", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                    }
                    else if (block.type.containsIgnoreCase ("reverb"))
                    {
                        block.targetId = "reverbMix";
                        block.values["reverbMix"] = juce::jmin (0.40f, normalised ("reverbMix", block.values.count ("reverbMix") ? block.values["reverbMix"] : 0.20f));
                    }
                    else if (block.values.count ("mix") != 0)
                    {
                        block.values["mix"] = normalised ("mix", block.values["mix"]);
                    }
                }
                else if (block.section == "out")
                {
                    block.values["volume"] = juce::jmin (0.72f, normalised ("volume", block.values.count ("volume") ? block.values["volume"] : 0.62f));
                    block.values["pan"] = normalised ("pan", block.values.count ("pan") ? block.values["pan"] : 0.50f);
                    block.values["bpmSync"] = raw ("bpmSync", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                    block.values["retrigger"] = raw ("retrigger", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                    block.values["outputLimiter"] = 1.0f;
                    block.values["outputCeilingDb"] = -1.0f;
                    block.values["outputGainDb"] = juce::jmin (0.0f, raw ("outputGainDb", 0.0f));
                }
            }

            graph.userConfigured = true;
        }

        static void specialisePresetPatch (InstrumentPatch& patch, Preset& preset, const ParameterModel& model, int index)
        {
            if (isArpPreset (preset))
                applyArpRecipe (patch.dspGraph, preset, index);
            else if (isStringPreset (preset))
                applyStringRecipe (patch.dspGraph, preset, index);
            else if (isBassPreset (preset))
                applyBassRecipe (patch.dspGraph, preset, index);
            else if (isPluckPreset (preset))
                applyPluckRecipe (patch.dspGraph, preset, index);
            else if (isLeadPreset (preset))
                applyLeadRecipe (patch.dspGraph, preset, index);
            else if (isWavetablePreset (preset))
                applyWavetableRecipe (patch.dspGraph, preset, index);
            else if (isMotionPreset (preset))
                applyMotionRecipe (patch.dspGraph, preset, index);
            else if (isPadPreset (preset))
                applyPadRecipe (patch.dspGraph, preset, index);

            sanitisePresetPlaybackValues (preset);
            syncPresetValuesIntoGraph (patch.dspGraph, preset, model);
            patch.parameterValues = preset.values;
        }
    }

    const Preset* PatchCraftPack::findPreset (const juce::String& name) const
    {
        for (auto& p : presets)
            if (p.name == name) return &p;
        return nullptr;
    }

    const Preset* PatchCraftPack::findDefaultPreset() const
    {
        for (auto& p : presets)
            if (p.isDefault) return &p;
        if (! manifest.defaultPreset.isEmpty())
            if (auto* p = findPreset (manifest.defaultPreset)) return p;
        return presets.empty() ? nullptr : &presets.front();
    }

    const InstrumentPatch* PatchCraftPack::findPatch (const juce::String& id) const
    {
        for (const auto& patch : patches)
            if (patch.id == id)
                return &patch;
        return nullptr;
    }

    const InstrumentPatch* PatchCraftPack::findPatchForPreset (const Preset& preset) const
    {
        if (preset.patchId.isNotEmpty())
            if (auto* patch = findPatch (preset.patchId))
                return patch;
        for (const auto& patch : patches)
            if (patch.name == preset.name)
                return &patch;
        return nullptr;
    }

    const InstrumentPatch* PatchCraftPack::findDefaultPatch() const
    {
        if (const auto* preset = findDefaultPreset())
            if (auto* patch = findPatchForPreset (*preset))
                return patch;
        for (const auto& patch : patches)
            if (patch.isDefault)
                return &patch;
        return patches.empty() ? nullptr : &patches.front();
    }

    void ensurePresetBackedPatches (PatchCraftPack& pack, bool replaceExisting)
    {
        if (pack.presets.empty())
            return;

        if (! replaceExisting && ! pack.patches.empty())
        {
            for (auto& patch : pack.patches)
            {
                const Preset* matchingPreset = nullptr;
                for (const auto& preset : pack.presets)
                {
                    if ((preset.patchId.isNotEmpty() && preset.patchId == patch.id)
                        || (preset.name.isNotEmpty() && preset.name == patch.name))
                    {
                        matchingPreset = &preset;
                        break;
                    }
                }

                if (matchingPreset == nullptr)
                    continue;

                if (patch.dspGraph.blocks.empty())
                    patch.dspGraph = pack.dspGraph;
                syncPresetValuesIntoGraph (patch.dspGraph, *matchingPreset, pack.parameters);
            }
            return;
        }

        if (replaceExisting)
            pack.patches.clear();

        pack.patches.reserve (pack.presets.size());
        for (int index = 0; index < (int) pack.presets.size(); ++index)
        {
            auto& preset = pack.presets[(size_t) index];
            InstrumentPatch patch;
            patch.name = preset.name.trim().isNotEmpty()
                ? preset.name.trim()
                : pack.manifest.instrumentName + " Preset " + juce::String (index + 1);
            patch.id = makeStableContentId (patch.name);
            patch.description = preset.description.trim().isNotEmpty()
                ? preset.description
                : "Full playable PatchCraft preset state.";
            patch.engine = pack.manifest.engine;
            patch.category = pack.manifest.category;
            patch.author = pack.manifest.creator;
            patch.version = pack.manifest.version;
            patch.packId = pack.manifest.instrumentName;
            patch.tags = preset.tags;
            patch.generated = preset.generated;
            patch.isDefault = preset.isDefault
                || (preset.name.isNotEmpty() && preset.name == pack.manifest.defaultPreset)
                || (index == 0 && pack.manifest.defaultPreset.isEmpty());
            patch.dspGraph = pack.dspGraph;
            patch.sampleZones = pack.sampleMap.getZones();
            patch.midiMappings = pack.midiMappings;

            specialisePresetPatch (patch, preset, pack.parameters, index);
            preset.patchId = patch.id;
            preset.isDefault = patch.isDefault;
            pack.patches.push_back (std::move (patch));
        }

        if (pack.manifest.defaultPreset.isEmpty() && ! pack.presets.empty())
            pack.manifest.defaultPreset = pack.presets.front().name;
    }

} // namespace patchcraft
