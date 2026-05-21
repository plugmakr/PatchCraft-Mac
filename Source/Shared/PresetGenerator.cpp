#include "PresetGenerator.h"

#include <array>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        static float value01 (juce::Random& rng, float centre, float spread)
        {
            return juce::jlimit (0.0f, 1.0f, centre + (rng.nextFloat() * 2.0f - 1.0f) * spread);
        }

        static void setNorm (Preset& preset, const ParameterModel& model, const juce::String& id, float normalised)
        {
            if (const auto* def = model.find (id))
                preset.values[id] = juce::jmap (juce::jlimit (0.0f, 1.0f, normalised), 0.0f, 1.0f, def->min, def->max);
        }

        static void setValue (Preset& preset, const ParameterModel& model, const juce::String& id, float value)
        {
            if (const auto* def = model.find (id))
                preset.values[id] = juce::jlimit (def->min, def->max, value);
        }

        static juce::String nameFor (const juce::String& theme, int index)
        {
            static const char* suffixes[] =
            {
                "One", "Pulse", "Glass", "Wide", "Dark", "Bright", "Deep", "Air",
                "Edge", "Bloom", "Shift", "Lift", "Night", "Spark", "Drive", "Drift"
            };
            juce::String name = theme + " " + suffixes[(size_t) index % std::size (suffixes)];
            if (index >= (int) std::size (suffixes))
                name << " " << juce::String (index + 1);
            return name;
        }

        static bool isWavetableTheme (const juce::String& theme)
        {
            const auto lower = theme.toLowerCase();
            return lower.contains ("wavetable") || lower == "wt" || lower.startsWith ("wt ");
        }

        static juce::String wavetableNameFor (int index)
        {
            static const char* names[] =
            {
                "WT Prism Drift",
                "WT Razor Pulse",
                "WT Glass Motion",
                "WT Formant Choir",
                "WT PWM Pluck",
                "WT Aggro Reese",
                "WT Hybrid Bell",
                "WT Organ Bloom",
                "WT Custom Fold",
                "WT Sub Motion"
            };
            juce::String name = names[(size_t) index % std::size (names)];
            if (index >= (int) std::size (names))
                name << " " << juce::String (index + 1);
            return name;
        }

        static void setWavetableShape (Preset& preset, const ParameterModel& model, int shapeKind)
        {
            constexpr int points = 32;
            for (int frame = 0; frame < 4; ++frame)
            {
                for (int point = 0; point < points; ++point)
                {
                    const float phase = std::fmod ((float) point / (float) points + (float) frame * 0.035f, 1.0f);
                    const float angle = phase * juce::MathConstants<float>::twoPi;
                    float value = 0.0f;

                    switch ((shapeKind + frame) % 6)
                    {
                        case 0:
                            value = std::sin (angle);
                            break;
                        case 1:
                            value = 0.66f * std::sin (angle)
                                  + 0.24f * std::sin (angle * (3.0f + 0.25f * (float) frame))
                                  + 0.10f * std::sin (angle * 7.0f);
                            break;
                        case 2:
                            value = phase < 0.5f ? -1.0f + phase * 4.0f
                                                  :  3.0f - phase * 4.0f;
                            break;
                        case 3:
                            value = phase < (0.30f + 0.03f * (float) frame) ? 1.0f : -0.68f;
                            break;
                        case 4:
                            value = std::tanh (2.4f * std::sin (angle)
                                                + 0.70f * std::sin (angle * (5.0f + (float) frame)));
                            break;
                        default:
                            value = std::sin (angle) * (0.45f + 0.55f * std::abs (std::sin (angle * 2.0f)));
                            value += 0.28f * std::sin (angle * (4.0f + 0.5f * (float) frame) + 0.4f);
                            break;
                    }

                    value = juce::jlimit (-1.0f, 1.0f, value);
                    setValue (preset, model, "wtFrame" + juce::String (frame) + "Shape" + juce::String (point), value);
                    if (frame == 0)
                        setValue (preset, model, "wtShape" + juce::String (point), value);
                }
            }
        }

        struct WavetableRecipe
        {
            const char* description;
            float table = 0.0f;
            float position = 0.0f;
            float morph = 0.0f;
            float warp = 0.0f;
            float fold = 0.0f;
            float unison = 1.0f;
            float detune = 0.0f;
            float spread = 0.0f;
            float level = 1.0f;
            float attack = 0.01f;
            float decay = 0.20f;
            float sustain = 0.80f;
            float release = 0.40f;
            float cutoff = 4200.0f;
            float resonance = 0.20f;
            float lfoRate = 4.0f;
            float lfoAmount = 0.0f;
            float delayMix = 0.0f;
            float delayFeedback = 0.30f;
            float reverbMix = 0.0f;
            float subBlend = 0.0f;
            float noiseBlend = 0.0f;
            float oscBlend = 0.0f;
            float vibratoDepth = 0.0f;
            float vibratoRate = 5.0f;
            float octave = 0.0f;
            int shape = 0;
        };

        static const WavetableRecipe& wavetableRecipeFor (int index)
        {
            static const std::array<WavetableRecipe, 10> recipes {{
                { "wide glass table with slow spectral drift",        1.0f, 0.18f, 0.74f, 0.20f, 0.08f, 5.0f, 18.0f, 0.78f, 0.98f, 0.85f, 1.60f, 0.76f, 3.80f, 6200.0f, 0.18f, 0.35f, 0.24f, 0.18f, 0.36f, 0.46f, 0.08f, 0.02f, 0.04f, 0.08f, 4.6f,  0.0f, 1 },
                { "hard razor table for pulse bass movement",        4.0f, 0.42f, 0.36f, 0.58f, 0.34f, 2.0f, 10.0f, 0.28f, 1.10f, 0.004f, 0.18f, 0.42f, 0.18f, 1800.0f, 0.48f, 3.00f, 0.38f, 0.08f, 0.22f, 0.10f, 0.36f, 0.04f, 0.02f, 0.00f, 5.0f, -1.0f, 4 },
                { "bright glass motion patch with tempo-synced sweep", 1.0f, 0.68f, 0.84f, 0.36f, 0.12f, 4.0f, 22.0f, 0.62f, 1.00f, 0.12f, 0.54f, 0.58f, 1.40f, 9800.0f, 0.22f, 0.50f, 0.52f, 0.30f, 0.48f, 0.36f, 0.02f, 0.10f, 0.06f, 0.12f, 5.8f,  0.0f, 5 },
                { "formant choir pad with vocal-style morphing",     3.0f, 0.57f, 0.92f, 0.18f, 0.06f, 6.0f, 26.0f, 0.86f, 0.92f, 1.40f, 2.20f, 0.82f, 5.60f, 3200.0f, 0.16f, 0.22f, 0.18f, 0.14f, 0.30f, 0.72f, 0.04f, 0.01f, 0.02f, 0.18f, 4.2f,  0.0f, 1 },
                { "fast PWM pluck with fold bite and short ambience", 2.0f, 0.24f, 0.42f, 0.50f, 0.22f, 1.0f, 0.0f,  0.00f, 1.00f, 0.002f, 0.12f, 0.10f, 0.28f, 5200.0f, 0.34f, 6.00f, 0.14f, 0.14f, 0.24f, 0.18f, 0.10f, 0.05f, 0.00f, 0.00f, 5.0f,  0.0f, 3 },
                { "aggressive unison reese with controlled fold",    6.0f, 0.48f, 0.64f, 0.42f, 0.48f, 7.0f, 34.0f, 0.92f, 1.18f, 0.03f, 0.62f, 0.68f, 1.20f, 1400.0f, 0.36f, 0.18f, 0.22f, 0.04f, 0.18f, 0.14f, 0.42f, 0.02f, 0.03f, 0.04f, 4.8f, -1.0f, 4 },
                { "hybrid bell table with shaped custom harmonics",  7.0f, 0.76f, 0.70f, 0.32f, 0.10f, 3.0f, 16.0f, 0.58f, 0.88f, 0.01f, 1.10f, 0.34f, 2.80f, 12400.0f,0.12f, 1.50f, 0.18f, 0.22f, 0.28f, 0.60f, 0.00f, 0.00f, 0.02f, 0.10f, 6.0f,  1.0f, 5 },
                { "organ-style additive table with slow bloom",      5.0f, 0.34f, 0.54f, 0.10f, 0.04f, 4.0f, 12.0f, 0.42f, 0.96f, 0.34f, 0.90f, 0.80f, 2.60f, 4400.0f, 0.10f, 0.28f, 0.16f, 0.08f, 0.20f, 0.42f, 0.12f, 0.00f, 0.03f, 0.06f, 4.4f,  0.0f, 1 },
                { "custom folded table for unstable animated texture",8.0f, 0.52f, 0.86f, 0.72f, 0.38f, 5.0f, 28.0f, 0.80f, 1.05f, 0.22f, 1.40f, 0.50f, 3.40f, 7200.0f, 0.28f, 0.75f, 0.46f, 0.26f, 0.44f, 0.54f, 0.04f, 0.16f, 0.01f, 0.12f, 5.2f,  0.0f, 4 },
                { "deep sub motion table with clean low-end focus",  0.0f, 0.12f, 0.28f, 0.16f, 0.06f, 2.0f, 8.0f,  0.22f, 1.12f, 0.02f, 0.42f, 0.72f, 0.80f, 950.0f,  0.26f, 0.25f, 0.12f, 0.02f, 0.18f, 0.08f, 0.62f, 0.00f, 0.02f, 0.02f, 4.8f, -1.0f, 2 }
            }};

            return recipes[(size_t) index % recipes.size()];
        }

        static void applyWavetablePreset (Preset& preset,
                                          const ParameterModel& model,
                                          int index)
        {
            const auto& recipe = wavetableRecipeFor (index);
            static constexpr float bends[] { 0.10f, 0.42f, -0.18f, -0.08f, 0.36f, 0.58f, -0.24f, 0.04f, 0.62f, -0.12f };
            static constexpr float tilts[] { 0.20f, -0.55f, -0.30f, 0.34f, -0.42f, -0.68f, 0.08f, 0.44f, -0.50f, 0.18f };
            static constexpr int syncRatios[] { 1, 2, 1, 1, 3, 2, 4, 1, 2, 1 };
            static constexpr int phaseModes[] { 2, 0, 1, 1, 0, 2, 1, 2, 1, 0 };
            const int recipeIndex = index % 10;

            setValue (preset, model, "wtEnabled", 1.0f);
            setValue (preset, model, "wtTable", recipe.table);
            setValue (preset, model, "wtPosition", recipe.position);
            setValue (preset, model, "wtMorph", recipe.morph);
            setValue (preset, model, "wtWarp", recipe.warp);
            setValue (preset, model, "wtFold", recipe.fold);
            setValue (preset, model, "wtUnison", recipe.unison);
            setValue (preset, model, "wtDetune", recipe.detune);
            setValue (preset, model, "wtSpread", recipe.spread);
            setValue (preset, model, "wtLevel", recipe.level);
            setValue (preset, model, "wtBend", bends[recipeIndex]);
            setValue (preset, model, "wtSyncRatio", (float) syncRatios[recipeIndex]);
            setValue (preset, model, "wtSpectralTilt", tilts[recipeIndex]);
            setValue (preset, model, "wtPhaseMode", (float) phaseModes[recipeIndex]);
            setValue (preset, model, "wtFrameCount", recipe.table >= 8.0f ? 4.0f : 1.0f);
            setValue (preset, model, "wtFramePosition", recipe.table >= 8.0f ? recipe.position : 0.0f);
            setWavetableShape (preset, model, recipe.shape);

            setValue (preset, model, "oscType", 1.0f);
            setValue (preset, model, "osc2Type", 3.0f);
            setValue (preset, model, "oscBlend", recipe.oscBlend);
            setValue (preset, model, "octave", recipe.octave);
            setValue (preset, model, "detune", recipe.detune * 0.12f);
            setValue (preset, model, "osc2Detune", recipe.detune * 0.50f);
            setValue (preset, model, "subBlend", recipe.subBlend);
            setValue (preset, model, "noiseBlend", recipe.noiseBlend);
            setValue (preset, model, "attack", recipe.attack);
            setValue (preset, model, "decay", recipe.decay);
            setValue (preset, model, "sustain", recipe.sustain);
            setValue (preset, model, "release", recipe.release);
            setValue (preset, model, "filterCutoff", recipe.cutoff);
            setValue (preset, model, "filterResonance", recipe.resonance);
            setValue (preset, model, "lfoRate", recipe.lfoRate);
            setValue (preset, model, "lfoAmount", recipe.lfoAmount);
            setValue (preset, model, "delayMix", recipe.delayMix);
            setValue (preset, model, "delayFeedback", recipe.delayFeedback);
            setValue (preset, model, "reverbMix", recipe.reverbMix);
            setValue (preset, model, "vibratoDepth", recipe.vibratoDepth);
            setValue (preset, model, "vibratoRate", recipe.vibratoRate);
            setValue (preset, model, "bpmSync", 1.0f);
            setValue (preset, model, "retrigger", index == 4 ? 1.0f : 0.0f);
            setValue (preset, model, "volume", 0.78f);
            setValue (preset, model, "pan", 0.0f);

            preset.description = "Generated wavetable preset: " + juce::String (recipe.description)
                + ". Uses WT table " + juce::String ((int) recipe.table)
                + ", morph " + juce::String (recipe.morph, 2)
                + ", warp " + juce::String (recipe.warp, 2)
                + ", fold " + juce::String (recipe.fold, 2) + ".";
            preset.tags.addIfNotAlreadyThere ("wavetable");
            preset.tags.addIfNotAlreadyThere ("WT");
        }

        static void applyTheme (Preset& preset,
                                const ParameterModel& model,
                                juce::Random& rng,
                                const juce::String& theme,
                                const juce::String& engineId,
                                int index)
        {
            const auto lower = theme.toLowerCase();

            setNorm (preset, model, "volume", lower.contains ("arp") ? value01 (rng, 0.58f, 0.08f)
                                                                     : value01 (rng, 0.66f, 0.10f));
            setNorm (preset, model, "pan", value01 (rng, 0.50f, 0.18f));
            setValue (preset, model, "bpmSync", 1.0f);
            setValue (preset, model, "retrigger", lower.contains ("arp") || lower.contains ("pluck") ? 1.0f : 0.0f);

            if (engineId == "synth" && isWavetableTheme (theme))
            {
                applyWavetablePreset (preset, model, index);
                return;
            }

            if (lower.contains ("arp"))
            {
                // Generate actual arpeggio patterns instead of random patches
                setNorm (preset, model, "attack", 0.02f);
                setNorm (preset, model, "decay", 0.16f);
                setNorm (preset, model, "sustain", 0.18f);
                setNorm (preset, model, "release", 0.16f);
                setNorm (preset, model, "filterCutoff", 0.45f);
                setNorm (preset, model, "filterResonance", 0.38f);
                setNorm (preset, model, "lfoRate", (index % 4) + 1.0f);
                setNorm (preset, model, "lfoAmount", 0.28f);
                setNorm (preset, model, "delayMix", 0.28f);
                setNorm (preset, model, "delayFeedback", 0.38f);
                setNorm (preset, model, "reverbMix", 0.20f);
                
                // Set arpeggio-specific parameters
                setValue (preset, model, "arpMode", (float) (index % 4)); // Different arpeggio modes
                setValue (preset, model, "arpSpeed", 0.5f + (index % 3) * 0.5f); // Varied speeds
                setValue (preset, model, "arpOctave", 1.0f + (index % 2)); // Octave variations
                setValue (preset, model, "arpGate", 0.8f); // Consistent gate
                
                preset.description = "Generated Arpeggio preset: " + juce::String (index + 1) + ". "
                    + juce::String ((index % 4) == 0 ? "Up" : 
                    ((index % 4) == 1 ? "Down" : 
                    ((index % 4) == 2 ? "Random" : "Alternating"))) + " pattern.";
                preset.tags.addIfNotAlreadyThere ("arpeggio");
                preset.tags.addIfNotAlreadyThere ("ARP");
            }
            else if (lower.contains ("pluck"))
            {
                setNorm (preset, model, "attack", 0.0f);
                setNorm (preset, model, "decay", value01 (rng, 0.12f, 0.08f));
                setNorm (preset, model, "sustain", value01 (rng, 0.06f, 0.05f));
                setNorm (preset, model, "release", value01 (rng, 0.22f, 0.10f));
                setNorm (preset, model, "filterCutoff", value01 (rng, 0.38f, 0.20f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.30f, 0.18f));
                setNorm (preset, model, "delayMix", value01 (rng, 0.18f, 0.14f));
                setNorm (preset, model, "reverbMix", value01 (rng, 0.24f, 0.16f));
            }
            else if (lower.contains ("string"))
            {
                setNorm (preset, model, "attack", value01 (rng, 0.45f, 0.20f));
                setNorm (preset, model, "decay", value01 (rng, 0.34f, 0.18f));
                setNorm (preset, model, "sustain", value01 (rng, 0.72f, 0.18f));
                setNorm (preset, model, "release", value01 (rng, 0.58f, 0.22f));
                setNorm (preset, model, "filterCutoff", value01 (rng, 0.50f, 0.22f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.12f, 0.08f));
                setNorm (preset, model, "reverbMix", value01 (rng, 0.50f, 0.22f));
                setNorm (preset, model, "delayMix", value01 (rng, 0.08f, 0.08f));
            }
            else if (lower.contains ("lfo") || lower.contains ("motion"))
            {
                setNorm (preset, model, "attack", value01 (rng, 0.12f, 0.12f));
                setNorm (preset, model, "decay", value01 (rng, 0.36f, 0.22f));
                setNorm (preset, model, "sustain", value01 (rng, 0.55f, 0.28f));
                setNorm (preset, model, "release", value01 (rng, 0.38f, 0.20f));
                setValue (preset, model, "lfoRate", lower.contains ("lfo") ? juce::jmax (0.25f, (float) (index % 8 + 1) * 0.5f)
                                                                            : value01 (rng, 0.25f, 0.24f) * 12.0f);
                setNorm (preset, model, "lfoAmount", value01 (rng, 0.55f, 0.32f));
                setNorm (preset, model, "filterCutoff", value01 (rng, 0.42f, 0.25f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.32f, 0.24f));
                setNorm (preset, model, "delayMix", value01 (rng, 0.32f, 0.24f));
                setNorm (preset, model, "reverbMix", value01 (rng, 0.35f, 0.22f));
            }
            else if (lower.contains ("pad"))
            {
                // Pads: slow attack, long release, lots of space, wide stereo.
                // The index biases each preset toward a different character
                // (airy, dark, evolving, hollow) so a row of generated pads
                // doesn't all sound like one big drone.
                const int variant = index % 4;
                setNorm (preset, model, "attack",          0.45f + 0.15f * variant);
                setNorm (preset, model, "decay",           value01 (rng, 0.55f, 0.20f));
                setNorm (preset, model, "sustain",         value01 (rng, 0.82f, 0.10f));
                setNorm (preset, model, "release",         0.55f + 0.20f * variant);
                setNorm (preset, model, "filterCutoff",    variant == 1 ? 0.28f
                                                          : variant == 3 ? 0.65f
                                                          : value01 (rng, 0.45f, 0.18f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.10f, 0.10f));
                setNorm (preset, model, "delayMix",        value01 (rng, 0.22f, 0.14f));
                setNorm (preset, model, "reverbMix",       0.55f + 0.10f * variant);
            }
            else if (lower.contains ("bass"))
            {
                // Bass: punchy attack, short-to-medium release, low cutoff,
                // pronounced sub. Variant flips between mono/sub/reese/wobble.
                const int variant = index % 4;
                setNorm (preset, model, "attack",          0.02f);
                setNorm (preset, model, "decay",           value01 (rng, 0.25f, 0.12f));
                setNorm (preset, model, "sustain",         variant == 2 ? 0.90f : value01 (rng, 0.55f, 0.20f));
                setNorm (preset, model, "release",         variant == 0 ? 0.18f : value01 (rng, 0.30f, 0.15f));
                setNorm (preset, model, "filterCutoff",    variant == 3 ? 0.55f : 0.22f + 0.10f * variant);
                setNorm (preset, model, "filterResonance", variant == 3 ? 0.55f : value01 (rng, 0.25f, 0.15f));
                setNorm (preset, model, "delayMix",        variant == 3 ? value01 (rng, 0.20f, 0.10f) : 0.05f);
                setNorm (preset, model, "reverbMix",       value01 (rng, 0.12f, 0.08f));
            }
            else if (lower.contains ("choir") || lower.contains ("vocal"))
            {
                // Choirs are formant-heavy slow-attack pads with vibrato.
                setNorm (preset, model, "attack",          value01 (rng, 0.40f, 0.12f));
                setNorm (preset, model, "decay",           value01 (rng, 0.45f, 0.15f));
                setNorm (preset, model, "sustain",         value01 (rng, 0.88f, 0.06f));
                setNorm (preset, model, "release",         value01 (rng, 0.65f, 0.15f));
                setNorm (preset, model, "filterCutoff",    value01 (rng, 0.55f, 0.10f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.30f, 0.10f));
                setNorm (preset, model, "delayMix",        value01 (rng, 0.18f, 0.10f));
                setNorm (preset, model, "reverbMix",       value01 (rng, 0.65f, 0.10f));
                setNorm (preset, model, "vibratoDepth",    value01 (rng, 0.30f, 0.10f));
                setValue (preset, model, "vibratoRate",    value01 (rng, 0.45f, 0.10f) * 6.0f);
                setNorm (preset, model, "vocalMix",        value01 (rng, 0.26f, 0.14f));
                setNorm (preset, model, "vocalFormant",    value01 (rng, 0.45f, 0.18f));
                setNorm (preset, model, "vocalBody",       value01 (rng, 0.58f, 0.22f));
            }
            else if (lower.contains ("fx"))
            {
                // FX presets sweep the filter and lean on time-based effects.
                const int variant = index % 5;
                setNorm (preset, model, "attack",          variant == 0 ? 0.00f : value01 (rng, 0.10f, 0.10f));
                setNorm (preset, model, "decay",           value01 (rng, 0.40f, 0.20f));
                setNorm (preset, model, "sustain",         variant == 1 ? 0.10f : value01 (rng, 0.55f, 0.25f));
                setNorm (preset, model, "release",         value01 (rng, 0.45f, 0.20f));
                setNorm (preset, model, "filterCutoff",    juce::jlimit (0.05f, 0.95f, 0.18f + 0.18f * variant));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.55f, 0.20f));
                setNorm (preset, model, "delayMix",        0.30f + 0.10f * variant);
                setNorm (preset, model, "reverbMix",       0.40f + 0.08f * variant);
                setNorm (preset, model, "lfoAmount",       0.40f + 0.12f * variant);
                setValue (preset, model, "lfoRate",        juce::jmax (0.25f, 0.5f + (float) variant));
                setValue (preset, model, "drive",          value01 (rng, 0.30f, 0.20f));
                setNorm (preset, model, "multiTapMix",     value01 (rng, 0.26f + 0.06f * variant, 0.10f));
                setValue (preset, model, "multiTapTime",   0.12f + 0.08f * (float) variant);
                setNorm (preset, model, "multiTapFeedback", value01 (rng, 0.34f, 0.22f));
                setNorm (preset, model, "multiTapSpread",  value01 (rng, 0.60f, 0.24f));
                if (variant == 0 || variant == 4)
                {
                    setNorm (preset, model, "tapeMix",     value01 (rng, 0.30f, 0.18f));
                    setNorm (preset, model, "tapeDrive",   value01 (rng, 0.36f, 0.24f));
                    setNorm (preset, model, "tapeFlutter", value01 (rng, 0.12f, 0.12f));
                }
                if (variant == 1 || variant == 4)
                {
                    setNorm (preset, model, "vinylMix",    value01 (rng, 0.20f, 0.16f));
                    setNorm (preset, model, "vinylAge",    value01 (rng, 0.48f, 0.28f));
                    setNorm (preset, model, "vinylDust",   value01 (rng, 0.10f, 0.10f));
                }
                if (variant == 2 || variant == 4)
                {
                    setNorm (preset, model, "lofiMix",     value01 (rng, 0.22f, 0.18f));
                    setValue (preset, model, "lofiBits",   7.0f + (float) (index % 5));
                    setNorm (preset, model, "lofiRate",    value01 (rng, 0.28f, 0.22f));
                }
                if (variant == 3)
                {
                    setNorm (preset, model, "vocalMix",    value01 (rng, 0.24f, 0.18f));
                    setNorm (preset, model, "vocalFormant", value01 (rng, 0.50f, 0.30f));
                }
            }
            else
            {
                // Generic fallback - conservative and musical. Unknown themes
                // should still land as a usable starter patch, not random noise.
                setNorm (preset, model, "attack", value01 (rng, 0.10f, 0.10f));
                setNorm (preset, model, "decay", value01 (rng, 0.25f, 0.18f));
                setNorm (preset, model, "sustain", value01 (rng, 0.55f, 0.25f));
                setNorm (preset, model, "release", value01 (rng, 0.32f, 0.18f));
                setNorm (preset, model, "filterCutoff", value01 (rng, 0.50f, 0.20f));
                setNorm (preset, model, "filterResonance", value01 (rng, 0.18f, 0.12f));
                setNorm (preset, model, "delayMix", value01 (rng, 0.14f, 0.10f));
                setNorm (preset, model, "reverbMix", value01 (rng, 0.24f, 0.14f));
            }

            if (engineId == "synth")
            {
                // Bias the oscillator choice toward something that fits the
                // theme so a "Pads" preset doesn't accidentally start as a
                // square-wave bass and a "Bass" preset doesn't start as a
                // sine pad. 0=sine, 1=saw, 2=square, 3=triangle, 4=noise.
                float oscMain  = (float) (index % 5);
                float oscLayer = (float) ((index + 2) % 5);
                if (lower.contains ("pad") || lower.contains ("choir") || lower.contains ("string"))
                {
                    oscMain  = (index % 2 == 0) ? 1.0f : 3.0f;   // saw / triangle
                    oscLayer = (index % 2 == 0) ? 3.0f : 1.0f;
                }
                else if (lower.contains ("bass"))
                {
                    oscMain  = (index % 2 == 0) ? 2.0f : 1.0f;   // square / saw
                    oscLayer = 0.0f;                             // sine sub
                }
                else if (lower.contains ("pluck"))
                {
                    oscMain  = (index % 2 == 0) ? 3.0f : 0.0f;   // triangle / sine
                    oscLayer = 1.0f;
                }
                setValue (preset, model, "oscType", oscMain);
                setValue (preset, model, "osc2Type", oscLayer);
                setNorm (preset, model, "oscBlend", lower.contains ("bass") ? value01 (rng, 0.18f, 0.14f)
                                                                            : lower.contains ("string") || lower.contains ("pad") ? value01 (rng, 0.42f, 0.24f)
                                                                                                                                   : value01 (rng, 0.30f, 0.22f));
                setValue (preset, model, "octave", lower.contains ("string") ? 0.0f : (float) ((index % 3) - 1));
                setValue (preset, model, "detune", (rng.nextFloat() * 2.0f - 1.0f) * (lower.contains ("string") ? 14.0f : 7.0f));
                setValue (preset, model, "osc2Detune", (rng.nextFloat() * 2.0f - 1.0f)
                    * (lower.contains ("motion") || lower.contains ("lfo") ? 24.0f : 14.0f));
                setNorm (preset, model, "subBlend", lower.contains ("bass") ? value01 (rng, 0.52f, 0.24f)
                                                                             : lower.contains ("pluck") ? value01 (rng, 0.12f, 0.10f)
                                                                                                         : value01 (rng, 0.20f, 0.18f));
                setNorm (preset, model, "noiseBlend", lower.contains ("arp") || lower.contains ("motion") ? value01 (rng, 0.22f, 0.18f)
                                                                                                           : lower.contains ("string") ? value01 (rng, 0.08f, 0.08f)
                                                                                                                                      : value01 (rng, 0.12f, 0.10f));
                setNorm (preset, model, "vibratoDepth", lower.contains ("string") ? value01 (rng, 0.18f, 0.12f) : 0.0f);
                setValue (preset, model, "vibratoRate", value01 (rng, 0.45f, 0.25f) * 8.0f);
            }

            if (engineId == "fx")
            {
                setNorm (preset, model, "drive", lower.contains ("motion") ? value01 (rng, 0.20f, 0.18f) : value01 (rng, 0.08f, 0.08f));
                setNorm (preset, model, "mix", value01 (rng, 0.86f, 0.14f));
            }
        }
    }

    juce::StringArray PresetGenerator::themes()
    {
        return { "Arps", "LFO", "Motion", "Wavetables", "Plucks", "Strings",
                 "Pads", "Bass", "Choir", "FX" };
    }

    std::vector<Preset> PresetGenerator::generate (const ParameterModel& parameters,
                                                   const LiveValueStore& liveValues,
                                                   const juce::String& engineId,
                                                   const PresetGenerationOptions& options)
    {
        std::vector<Preset> generated;
        const int count = juce::jlimit (1, 128, options.count);
        juce::Random rng (options.seed != 0 ? (juce::int64) options.seed
                                            : (juce::int64) juce::Time::getMillisecondCounterHiRes());

        generated.reserve ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            Preset preset;
            preset.name = isWavetableTheme (options.theme) && engineId == "synth"
                ? wavetableNameFor (i)
                : nameFor (options.theme, i);
            preset.theme = options.theme;
            preset.tags = { options.theme, engineId, "generated" };
            preset.generated = true;
            preset.description = "Generated " + options.theme + " preset for " + engineId
                + ". Auto-created from PatchCraft parameter ranges.";

            for (const auto& def : parameters.getAll())
                preset.values[def.id] = options.includeCurrentAsAnchor
                    ? liveValues.getValue (def.id, def.defaultValue)
                    : def.defaultValue;

            applyTheme (preset, parameters, rng, options.theme, engineId, i);
            generated.push_back (std::move (preset));
        }

        if (! generated.empty())
            generated.front().isDefault = true;
        return generated;
    }
}
