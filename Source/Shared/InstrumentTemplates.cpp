#include "InstrumentTemplates.h"

namespace patchcraft
{
    namespace
    {
        // Helper: insert a knob bound to a parameter id, on a specific tab group.
        static LayoutElement makeKnob (const juce::String& id,
                                       const juce::String& label,
                                       const juce::String& parameterId,
                                       int x, int y,
                                       const juce::String& groupId,
                                       int w = 90, int h = 100)
        {
            LayoutElement k;
            k.type = ElementType::Knob;
            k.id = id;
            k.x = x; k.y = y; k.width = w; k.height = h;
            k.label = label; k.parameterId = parameterId;
            k.style = "Vintage Gold";
            k.groupId = groupId;
            return k;
        }

        // Builds the 8 macro knobs on a particular tab. Lays them out evenly.
        static void addKnobRow (LayoutModel& layout,
                                const juce::String& groupId,
                                std::initializer_list<std::pair<juce::String, juce::String>> knobs)
        {
            int i = 0;
            for (auto& kv : knobs)
            {
                if (i >= 8) break;
                const int x = 180 + i * 110;
                layout.add (makeKnob ("knob_" + groupId + "_" + kv.second,
                                      kv.first, kv.second, x, 530, groupId));
                ++i;
            }
        }

        static void addCommonChrome (LayoutModel& layout, const juce::String& engine,
                                     const CanvasSize& canvas, bool keyboardVisible)
        {
            // Background image (locked - selectable so artwork can be replaced
            // via the Inspector's Asset field).
            { LayoutElement e; e.type = ElementType::Image; e.id = "background";
              e.x = 0; e.y = 0; e.width = canvas.width; e.height = canvas.height;
              e.locked = true; layout.add (e); }

            // Title
            { LayoutElement e; e.type = ElementType::Label; e.id = "title";
              e.x = 40; e.y = 20; e.width = 500; e.height = 50;
              juce::String name = "Cinematic Evolve Pad";
              if (engine == "synth") name = "PatchCraft Synth";
              if (engine == "fx")    name = "PatchCraft FX";
              if (engine == "drum")  name = "PatchCraft Drums";
              e.label = name; layout.add (e); }

            // Preset dropdown (right of title)
            { LayoutElement e; e.type = ElementType::Dropdown; e.id = "presets";
              e.x = 600; e.y = 25; e.width = 320; e.height = 36;
              e.label = "Preset"; layout.add (e); }

            // Hero artwork window - now an Image so the Asset field can replace it
            { LayoutElement e; e.type = ElementType::Image; e.id = "hero";
              e.x = 40; e.y = 90; e.width = 1200; e.height = 360;
              e.asset = "assets/hero.png";  // Will be generated at runtime if missing
              e.label = "Artwork"; layout.add (e); }

            // Tab strip
            { LayoutElement e; e.type = ElementType::TabPanel; e.id = "tabs";
              e.x = 380; e.y = 470; e.width = 540; e.height = 32;
              if (engine == "fx")
                  e.tabs = { "Main", "Filter", "Delay", "Reverb" };
              else if (engine == "drum")
                  e.tabs = { "Pads", "Amp", "Filter", "FX" };
              else
                  e.tabs = { "Main", "Amp", "Filter", "Mod", "FX", "Space", "Arp" };
              layout.add (e); }

            // Master section (always visible)
            if (engine != "fx")
            {
                LayoutElement vol; vol.type = ElementType::Knob; vol.id = "masterVol";
                vol.x = 1100; vol.y = 480; vol.width = 70; vol.height = 70;
                vol.label = "Volume"; vol.parameterId = "volume";
                layout.add (vol);

                LayoutElement pan; pan.type = ElementType::Knob; pan.id = "masterPan";
                pan.x = 1180; pan.y = 480; pan.width = 70; pan.height = 70;
                pan.label = "Pan"; pan.parameterId = "pan";
                layout.add (pan);
            }

            { LayoutElement e; e.type = ElementType::Meter; e.id = "outMeter";
              e.x = 1100; e.y = 560; e.width = 150; e.height = 24;
              e.label = "Output"; layout.add (e); }

            // Performance sliders (left side)
            if (engine != "fx" && engine != "drum")
            {
                LayoutElement exp; exp.type = ElementType::Slider; exp.id = "expression";
                exp.x = 40; exp.y = 470; exp.width = 24; exp.height = 220;
                exp.label = "Expr"; layout.add (exp);

                LayoutElement mod; mod.type = ElementType::Slider; mod.id = "modwheel";
                mod.x = 75; mod.y = 470; mod.width = 24; mod.height = 220;
                mod.label = "Mod"; layout.add (mod);
            }

            // Bottom keyboard
            if (keyboardVisible)
            {
                LayoutElement kb; kb.type = ElementType::Keyboard; kb.id = "keyboard";
                kb.x = 40; kb.y = 720; kb.width = 1200; kb.height = 60;
                layout.add (kb);
            }
        }
    }

    void buildDemoLayout (LayoutModel& layout, CanvasSize& canvas,
                          const juce::String& engine)
    {
        layout.clear();
        canvas = {};

        const bool keyboardVisible = (engine != "fx" && engine != "drum");
        addCommonChrome (layout, engine, canvas, keyboardVisible);

        if (engine == "synth")
        {
            // -------- Synth demo: full instrument with knobs in every tab.
            // Main tab: top-level macros
            addKnobRow (layout, "main", {
                { "Wave",    "oscType" },
                { "Octave",  "octave" },
                { "Detune",  "detune" },
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Cutoff",  "filterCutoff" }
            });

            // Amp tab: ADSR + master out
            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            // Filter tab: cutoff + reso + LFO modulation amount
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "LFO Amt", "lfoAmount" },
                { "LFO Rate","lfoRate" }
            });

            // Mod tab: LFO + detune + octave
            addKnobRow (layout, "mod", {
                { "LFO Rate","lfoRate" },
                { "LFO Amt", "lfoAmount" },
                { "Detune",  "detune" },
                { "Octave",  "octave" }
            });

            // FX tab: filter + delay + reverb chain
            addKnobRow (layout, "fx", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "Delay",   "delayMix" },
                { "Reverb",  "reverbMix" }
            });

            // Space tab: reverb + delay detail
            addKnobRow (layout, "space", {
                { "Reverb",  "reverbMix" },
                { "Dly Time","delayTime" },
                { "Dly Fb",  "delayFeedback" },
                { "Dly Mix", "delayMix" }
            });

            // Arp tab: ADSR-as-arp-envelope until a real arpeggiator engine ships
            addKnobRow (layout, "arp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "LFO Rate","lfoRate" },
                { "LFO Amt", "lfoAmount" }
            });
        }
        else if (engine == "drum")
        {
            // -------- Drum Machine: 4x4 pad grid + macro knobs.
            // Pads send MIDI notes 36..51; the sampler engine routes those to
            // any zones the user maps via the SampleMapEditor (Pad Map button).

            { LayoutElement grid; grid.type = ElementType::PadGrid;
              grid.id = "drumPadGrid";
              grid.x = 380; grid.y = 96; grid.width = 540; grid.height = 360;
              grid.padRows = 4; grid.padCols = 4; grid.padBaseNote = 36;
              grid.cornerRadius = 8.0f;
              layout.add (grid); }

            // Macro knob row beneath the pad grid (groupId == "pads" tab).
            addKnobRow (layout, "pads", {
                { "Volume",  "volume" },
                { "Pan",     "pan" },
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "Attack",  "attack" },
                { "Release", "release" },
                { "Reverb",  "reverbMix" },
                { "Drive",   "drive" }
            });

            // Amp tab: ADSR + master out
            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            // Filter tab: cutoff + resonance + envelope shaping
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "Attack",  "attack" },
                { "Decay",   "decay" }
            });

            // FX tab: drive + delay + reverb
            addKnobRow (layout, "fx", {
                { "Drive",   "drive" },
                { "Dly Mix", "delayMix" },
                { "Dly Time","delayTime" },
                { "Dly Fb",  "delayFeedback" },
                { "Reverb",  "reverbMix" }
            });
        }
        else if (engine == "fx")
        {
            // FX engine has tabs: Main, Filter, Delay, Reverb. Each is filled.
            addKnobRow (layout, "main", {
                { "Drive",   "drive" },
                { "Mix",     "mix" },
                { "Cutoff",  "filterCutoff" },
                { "Res",     "filterResonance" },
                { "Delay",   "delayMix" },
                { "Reverb",  "reverbMix" },
                { "Vol",     "volume" },
                { "Pan",     "pan" }
            });
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Res",     "filterResonance" },
                { "Drive",   "drive" },
                { "Mix",     "mix" }
            });
            addKnobRow (layout, "delay", {
                { "Time",    "delayTime" },
                { "Feedback","delayFeedback" },
                { "Mix",     "delayMix" },
                { "Drive",   "drive" }
            });
            addKnobRow (layout, "reverb", {
                { "Mix",     "reverbMix" },
                { "Drive",   "drive" },
                { "Vol",     "volume" },
                { "Pan",     "pan" }
            });
        }
        else
        {
            // -------- Sampler demo: full instrument with knobs in every tab.
            //
            // The parameter palette already includes: attack, decay, sustain,
            // release, filterCutoff, filterResonance, reverbMix, delayTime,
            // delayFeedback, delayMix, vibratoDepth, vibratoRate, volume, pan.
            // We bind the knobs on each tab to the parameters that fit.

            // Main tab: macro overview
            addKnobRow (layout, "main", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Cutoff",  "filterCutoff" },
                { "Reverb",  "reverbMix" },
                { "Delay",   "delayMix" },
                { "Vibrato", "vibratoDepth" }
            });

            // Amp tab: ADSR + global volume + pan
            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            // Filter tab: cutoff + resonance + envelope shaping
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" }
            });

            // Mod tab: vibrato controls
            addKnobRow (layout, "mod", {
                { "Vib Dep", "vibratoDepth" },
                { "Vib Rate","vibratoRate" }
            });

            // FX tab: distortion-ish (we only have filter+delay+reverb)
            addKnobRow (layout, "fx", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" },
                { "Delay",   "delayMix" },
                { "Reverb",  "reverbMix" }
            });

            // Space tab: reverb + delay detail
            addKnobRow (layout, "space", {
                { "Reverb",  "reverbMix" },
                { "Dly Time","delayTime" },
                { "Dly Fb",  "delayFeedback" },
                { "Dly Mix", "delayMix" }
            });

            // Arp tab: re-uses ADSR + vibrato as a faux arp envelope panel.
            // (A real arpeggiator engine is Phase C work.)
            addKnobRow (layout, "arp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Vib Rate","vibratoRate" },
                { "Vib Dep", "vibratoDepth" }
            });
        }
    }

    static void applySamplerDefaults (ParameterModel& params)
    {
        auto setDef = [&] (const juce::String& id, float v) {
            if (auto* p = params.find (id)) p->defaultValue = v;
        };
        setDef ("attack",       1.00f);
        setDef ("decay",        2.50f);
        setDef ("sustain",      0.70f);
        setDef ("release",      3.20f);
        setDef ("filterCutoff", 4200.0f);
        setDef ("reverbMix",    0.45f);
        setDef ("delayMix",     0.25f);
        setDef ("vibratoDepth", 0.35f);
    }

    // Drum-machine defaults: short snappy envelope, neutral filter, light FX.
    static void applyDrumDefaults (ParameterModel& params)
    {
        auto setDef = [&] (const juce::String& id, float v) {
            if (auto* p = params.find (id)) p->defaultValue = v;
        };
        setDef ("attack",       0.005f);   // near-instant
        setDef ("decay",        0.25f);
        setDef ("sustain",      0.0f);     // one-shot character
        setDef ("release",      0.15f);
        setDef ("filterCutoff", 12000.0f);
        setDef ("filterResonance", 0.10f);
        setDef ("reverbMix",    0.18f);
        setDef ("delayMix",     0.0f);
        setDef ("delayTime",    0.30f);
        setDef ("delayFeedback",0.30f);
        setDef ("vibratoDepth", 0.0f);
        setDef ("drive",        0.10f);
        setDef ("volume",       0.85f);
        setDef ("pan",          0.0f);
    }

    // Cinematic Evolve Pad-flavoured defaults for the synth engine: a slow,
    // detuned saw pad with low cutoff, slight LFO motion, lots of reverb.
    static void applySynthEvolvePadDefaults (ParameterModel& params)
    {
        auto setDef = [&] (const juce::String& id, float v) {
            if (auto* p = params.find (id)) p->defaultValue = v;
        };
        setDef ("oscType",         1.0f);     // saw
        setDef ("octave",          0.0f);
        setDef ("detune",          5.0f);     // 5 cents detune
        setDef ("attack",          1.20f);
        setDef ("decay",           2.00f);
        setDef ("sustain",          0.85f);
        setDef ("release",         3.50f);
        setDef ("filterCutoff",    1800.0f);
        setDef ("filterResonance", 0.30f);
        setDef ("lfoRate",         3.5f);
        setDef ("lfoAmount",       0.15f);
        setDef ("delayTime",       0.45f);
        setDef ("delayFeedback",   0.40f);
        setDef ("delayMix",        0.20f);
        setDef ("reverbMix",       0.55f);
        setDef ("volume",          0.65f);
        setDef ("pan",             0.0f);
    }

    static void applyEffectDefaults (ParameterModel& params)
    {
        auto setDef = [&] (const juce::String& id, float v) {
            if (auto* p = params.find (id)) p->defaultValue = v;
        };
        setDef ("drive",           0.20f);
        setDef ("mix",             1.0f);
        setDef ("filterCutoff",    8000.0f);
        setDef ("filterResonance", 0.10f);
        setDef ("delayTime",       0.30f);
        setDef ("delayFeedback",   0.40f);
        setDef ("delayMix",        0.15f);
        setDef ("reverbMix",       0.20f);
        setDef ("volume",          1.0f);
        setDef ("pan",             0.0f);
    }

    // ------------------------------------------------------------------------
    // 20 factory synth presets focused on Trance / Psytrance / Electronic.
    // Each entry is name + a list of (parameter-id, value) pairs applied on
    // top of the engine defaults. Filter cutoff is in Hz; oscType uses the
    // synth palette (0=sine, 1=saw, 2=square, 3=triangle, 4=noise/wt).
    // ------------------------------------------------------------------------
    struct TranceSynthPreset
    {
        const char* name;
        std::vector<std::pair<juce::String, float>> values;
    };

    static const std::vector<TranceSynthPreset>& tranceSynthPresets()
    {
        static const std::vector<TranceSynthPreset> presets = {
            // ---- LEADS -----------------------------------------------------
            { "Stadium Supersaw", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 14.0f },
                { "attack", 0.005f }, { "decay", 0.40f }, { "sustain", 0.85f }, { "release", 1.20f },
                { "filterCutoff", 9000.0f }, { "filterResonance", 0.18f },
                { "lfoRate", 0.35f }, { "lfoAmount", 0.04f },
                { "delayTime", 0.375f }, { "delayFeedback", 0.45f }, { "delayMix", 0.28f },
                { "reverbMix", 0.40f }, { "volume", 0.75f }
            } },
            { "Trance Hands Up", {
                { "oscType", 1 }, { "octave", 1 }, { "detune", 9.0f },
                { "attack", 0.005f }, { "decay", 0.30f }, { "sustain", 0.90f }, { "release", 1.40f },
                { "filterCutoff", 11000.0f }, { "filterResonance", 0.22f },
                { "lfoRate", 6.0f }, { "lfoAmount", 0.10f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.40f }, { "delayMix", 0.32f },
                { "reverbMix", 0.55f }, { "volume", 0.78f }
            } },
            { "Big Room Saw", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 18.0f },
                { "attack", 0.01f }, { "decay", 0.45f }, { "sustain", 0.78f }, { "release", 1.10f },
                { "filterCutoff", 7800.0f }, { "filterResonance", 0.30f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.06f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.30f }, { "delayMix", 0.22f },
                { "reverbMix", 0.38f }, { "volume", 0.80f }
            } },
            { "Hoover Stab", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 22.0f },
                { "attack", 0.005f }, { "decay", 0.20f }, { "sustain", 0.60f }, { "release", 0.30f },
                { "filterCutoff", 2200.0f }, { "filterResonance", 0.55f },
                { "lfoRate", 4.0f }, { "lfoAmount", 0.18f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.35f }, { "delayMix", 0.18f },
                { "reverbMix", 0.20f }, { "volume", 0.78f }
            } },
            { "Goa Lead", {
                { "oscType", 1 }, { "octave", 1 }, { "detune", 4.0f },
                { "attack", 0.002f }, { "decay", 0.18f }, { "sustain", 0.55f }, { "release", 0.40f },
                { "filterCutoff", 6500.0f }, { "filterResonance", 0.40f },
                { "lfoRate", 8.0f }, { "lfoAmount", 0.30f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.55f }, { "delayMix", 0.30f },
                { "reverbMix", 0.30f }, { "volume", 0.78f }
            } },
            { "Trance Pluck", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 6.0f },
                { "attack", 0.002f }, { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.35f },
                { "filterCutoff", 5500.0f }, { "filterResonance", 0.35f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.04f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.42f }, { "delayMix", 0.28f },
                { "reverbMix", 0.40f }, { "volume", 0.75f }
            } },
            // ---- ARPS / RHYTHMIC ------------------------------------------
            { "Liquid Arp", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 5.0f },
                { "attack", 0.002f }, { "decay", 0.18f }, { "sustain", 0.10f }, { "release", 0.18f },
                { "filterCutoff", 3800.0f }, { "filterResonance", 0.32f },
                { "lfoRate", 4.0f }, { "lfoAmount", 0.22f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.55f }, { "delayMix", 0.36f },
                { "reverbMix", 0.32f }, { "volume", 0.72f }
            } },
            { "Off-Beat Pluck", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 4.0f },
                { "attack", 0.001f }, { "decay", 0.12f }, { "sustain", 0.05f }, { "release", 0.20f },
                { "filterCutoff", 4500.0f }, { "filterResonance", 0.40f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.375f }, { "delayFeedback", 0.55f }, { "delayMix", 0.42f },
                { "reverbMix", 0.30f }, { "volume", 0.72f }
            } },
            { "Trance Gate", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 8.0f },
                { "attack", 0.005f }, { "decay", 0.50f }, { "sustain", 0.85f }, { "release", 0.80f },
                { "filterCutoff", 6000.0f }, { "filterResonance", 0.20f },
                { "lfoRate", 8.0f }, { "lfoAmount", 0.85f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.30f }, { "delayMix", 0.18f },
                { "reverbMix", 0.35f }, { "volume", 0.76f }
            } },
            { "Sidechain Pad", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 12.0f },
                { "attack", 0.40f }, { "decay", 1.20f }, { "sustain", 0.85f }, { "release", 2.20f },
                { "filterCutoff", 4200.0f }, { "filterResonance", 0.18f },
                { "lfoRate", 2.0f }, { "lfoAmount", 0.55f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.30f }, { "delayMix", 0.18f },
                { "reverbMix", 0.50f }, { "volume", 0.70f }
            } },
            // ---- BASSES ----------------------------------------------------
            { "Psy Bass", {
                { "oscType", 2 }, { "octave", -2 }, { "detune", 0.0f },
                { "attack", 0.001f }, { "decay", 0.12f }, { "sustain", 0.0f }, { "release", 0.10f },
                { "filterCutoff", 1100.0f }, { "filterResonance", 0.50f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.0f }, { "delayMix", 0.0f },
                { "reverbMix", 0.0f }, { "volume", 0.85f }
            } },
            { "Acid Bass", {
                { "oscType", 2 }, { "octave", -1 }, { "detune", 0.0f },
                { "attack", 0.002f }, { "decay", 0.20f }, { "sustain", 0.20f }, { "release", 0.20f },
                { "filterCutoff", 800.0f }, { "filterResonance", 0.78f },
                { "lfoRate", 4.0f }, { "lfoAmount", 0.30f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.30f }, { "delayMix", 0.10f },
                { "reverbMix", 0.10f }, { "volume", 0.80f }
            } },
            { "Trance Reese", {
                { "oscType", 1 }, { "octave", -1 }, { "detune", 26.0f },
                { "attack", 0.01f }, { "decay", 0.40f }, { "sustain", 0.85f }, { "release", 0.50f },
                { "filterCutoff", 1400.0f }, { "filterResonance", 0.42f },
                { "lfoRate", 0.25f }, { "lfoAmount", 0.18f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.20f }, { "delayMix", 0.10f },
                { "reverbMix", 0.18f }, { "volume", 0.80f }
            } },
            { "Sub Drop", {
                { "oscType", 0 }, { "octave", -2 }, { "detune", 0.0f },
                { "attack", 0.001f }, { "decay", 1.20f }, { "sustain", 0.0f }, { "release", 0.40f },
                { "filterCutoff", 380.0f }, { "filterResonance", 0.10f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.0f }, { "delayMix", 0.0f },
                { "reverbMix", 0.0f }, { "volume", 0.90f }
            } },
            // ---- MOD / TEXTURE --------------------------------------------
            { "Forest Mod", {
                { "oscType", 2 }, { "octave", 0 }, { "detune", 8.0f },
                { "attack", 0.002f }, { "decay", 0.40f }, { "sustain", 0.30f }, { "release", 0.25f },
                { "filterCutoff", 3400.0f }, { "filterResonance", 0.55f },
                { "lfoRate", 12.0f }, { "lfoAmount", 0.65f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.45f }, { "delayMix", 0.22f },
                { "reverbMix", 0.20f }, { "volume", 0.74f }
            } },
            { "Psy Tweet", {
                { "oscType", 0 }, { "octave", 2 }, { "detune", 3.0f },
                { "attack", 0.001f }, { "decay", 0.10f }, { "sustain", 0.0f }, { "release", 0.10f },
                { "filterCutoff", 8000.0f }, { "filterResonance", 0.45f },
                { "lfoRate", 14.0f }, { "lfoAmount", 0.45f },
                { "delayTime", 0.125f }, { "delayFeedback", 0.55f }, { "delayMix", 0.40f },
                { "reverbMix", 0.30f }, { "volume", 0.70f }
            } },
            { "Drift Modular", {
                { "oscType", 3 }, { "octave", 0 }, { "detune", 7.0f },
                { "attack", 1.20f }, { "decay", 2.20f }, { "sustain", 0.70f }, { "release", 3.50f },
                { "filterCutoff", 2400.0f }, { "filterResonance", 0.30f },
                { "lfoRate", 0.20f }, { "lfoAmount", 0.55f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.45f }, { "delayMix", 0.32f },
                { "reverbMix", 0.62f }, { "volume", 0.65f }
            } },
            { "Wavetable Reese", {
                { "oscType", 4 }, { "octave", -1 }, { "detune", 22.0f },
                { "attack", 0.01f }, { "decay", 0.50f }, { "sustain", 0.80f }, { "release", 0.60f },
                { "filterCutoff", 1600.0f }, { "filterResonance", 0.40f },
                { "lfoRate", 0.30f }, { "lfoAmount", 0.20f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.30f }, { "delayMix", 0.14f },
                { "reverbMix", 0.20f }, { "volume", 0.78f },
                { "wtEnabled", 1.0f }, { "wtTable", 6.0f }, { "wtPosition", 0.48f },
                { "wtMorph", 0.62f }, { "wtFold", 0.42f }
            } },
            // ---- PADS / AMBIENT -------------------------------------------
            { "Ambient Bloom", {
                { "oscType", 0 }, { "octave", 0 }, { "detune", 4.0f },
                { "attack", 2.50f }, { "decay", 3.20f }, { "sustain", 0.80f }, { "release", 5.00f },
                { "filterCutoff", 5500.0f }, { "filterResonance", 0.10f },
                { "lfoRate", 0.20f }, { "lfoAmount", 0.18f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.40f }, { "delayMix", 0.24f },
                { "reverbMix", 0.78f }, { "volume", 0.65f }
            } },
            { "Aurora Pad", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 16.0f },
                { "attack", 1.80f }, { "decay", 2.50f }, { "sustain", 0.90f }, { "release", 4.20f },
                { "filterCutoff", 3800.0f }, { "filterResonance", 0.18f },
                { "lfoRate", 0.30f }, { "lfoAmount", 0.30f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.45f }, { "delayMix", 0.26f },
                { "reverbMix", 0.70f }, { "volume", 0.68f }
            } },
            // ---- MORE LEADS (21-28) ---------------------------------------
            { "Uplifter Lead", {
                { "oscType", 1 }, { "octave", 1 }, { "detune", 11.0f },
                { "attack", 0.005f }, { "decay", 0.55f }, { "sustain", 0.75f }, { "release", 1.60f },
                { "filterCutoff", 8500.0f }, { "filterResonance", 0.28f },
                { "lfoRate", 0.25f }, { "lfoAmount", 0.20f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.50f }, { "delayMix", 0.30f },
                { "reverbMix", 0.55f }, { "volume", 0.78f }
            } },
            { "Dutch Hard Lead", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 7.0f },
                { "attack", 0.003f }, { "decay", 0.30f }, { "sustain", 0.65f }, { "release", 0.40f },
                { "filterCutoff", 6500.0f }, { "filterResonance", 0.45f },
                { "lfoRate", 4.0f }, { "lfoAmount", 0.08f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.30f }, { "delayMix", 0.18f },
                { "reverbMix", 0.20f }, { "volume", 0.82f }
            } },
            { "Anjuna Lead", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 13.0f },
                { "attack", 0.004f }, { "decay", 0.40f }, { "sustain", 0.85f }, { "release", 1.80f },
                { "filterCutoff", 7800.0f }, { "filterResonance", 0.22f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.10f },
                { "delayTime", 0.375f }, { "delayFeedback", 0.55f }, { "delayMix", 0.36f },
                { "reverbMix", 0.62f }, { "volume", 0.74f }
            } },
            { "Plucky Lead", {
                { "oscType", 2 }, { "octave", 0 }, { "detune", 4.0f },
                { "attack", 0.001f }, { "decay", 0.20f }, { "sustain", 0.20f }, { "release", 0.30f },
                { "filterCutoff", 5200.0f }, { "filterResonance", 0.42f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.45f }, { "delayMix", 0.30f },
                { "reverbMix", 0.30f }, { "volume", 0.76f }
            } },
            { "Festival Saw", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 24.0f },
                { "attack", 0.008f }, { "decay", 0.50f }, { "sustain", 0.80f }, { "release", 1.50f },
                { "filterCutoff", 9500.0f }, { "filterResonance", 0.22f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.05f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.30f }, { "delayMix", 0.20f },
                { "reverbMix", 0.45f }, { "volume", 0.78f }
            } },
            { "PWM Lead", {
                { "oscType", 2 }, { "octave", 0 }, { "detune", 6.0f },
                { "attack", 0.002f }, { "decay", 0.35f }, { "sustain", 0.70f }, { "release", 0.80f },
                { "filterCutoff", 5400.0f }, { "filterResonance", 0.30f },
                { "lfoRate", 1.5f }, { "lfoAmount", 0.40f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.40f }, { "delayMix", 0.24f },
                { "reverbMix", 0.32f }, { "volume", 0.76f }
            } },
            { "Acid Lead", {
                { "oscType", 2 }, { "octave", 0 }, { "detune", 0.0f },
                { "attack", 0.002f }, { "decay", 0.18f }, { "sustain", 0.30f }, { "release", 0.20f },
                { "filterCutoff", 1500.0f }, { "filterResonance", 0.85f },
                { "lfoRate", 4.0f }, { "lfoAmount", 0.50f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.55f }, { "delayMix", 0.28f },
                { "reverbMix", 0.22f }, { "volume", 0.80f }
            } },
            { "Bell Lead", {
                { "oscType", 0 }, { "octave", 1 }, { "detune", 4.0f },
                { "attack", 0.001f }, { "decay", 0.80f }, { "sustain", 0.20f }, { "release", 1.20f },
                { "filterCutoff", 9000.0f }, { "filterResonance", 0.18f },
                { "lfoRate", 4.5f }, { "lfoAmount", 0.06f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.50f }, { "delayMix", 0.32f },
                { "reverbMix", 0.55f }, { "volume", 0.72f }
            } },
            // ---- MORE ARPS (29-34) ----------------------------------------
            { "16th Arp", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 5.0f },
                { "attack", 0.001f }, { "decay", 0.10f }, { "sustain", 0.05f }, { "release", 0.10f },
                { "filterCutoff", 4800.0f }, { "filterResonance", 0.42f },
                { "lfoRate", 8.0f }, { "lfoAmount", 0.30f },
                { "delayTime", 0.125f }, { "delayFeedback", 0.55f }, { "delayMix", 0.36f },
                { "reverbMix", 0.30f }, { "volume", 0.72f }
            } },
            { "Triplet Arp", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 6.0f },
                { "attack", 0.001f }, { "decay", 0.18f }, { "sustain", 0.10f }, { "release", 0.18f },
                { "filterCutoff", 4200.0f }, { "filterResonance", 0.36f },
                { "lfoRate", 6.0f }, { "lfoAmount", 0.22f },
                { "delayTime", 0.166f }, { "delayFeedback", 0.50f }, { "delayMix", 0.34f },
                { "reverbMix", 0.34f }, { "volume", 0.72f }
            } },
            { "Wet Arp", {
                { "oscType", 0 }, { "octave", 0 }, { "detune", 8.0f },
                { "attack", 0.005f }, { "decay", 0.30f }, { "sustain", 0.20f }, { "release", 0.50f },
                { "filterCutoff", 5200.0f }, { "filterResonance", 0.20f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.10f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.60f }, { "delayMix", 0.45f },
                { "reverbMix", 0.78f }, { "volume", 0.70f }
            } },
            { "Detuned Arp", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 22.0f },
                { "attack", 0.001f }, { "decay", 0.25f }, { "sustain", 0.20f }, { "release", 0.30f },
                { "filterCutoff", 5800.0f }, { "filterResonance", 0.28f },
                { "lfoRate", 0.3f }, { "lfoAmount", 0.18f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.55f }, { "delayMix", 0.40f },
                { "reverbMix", 0.50f }, { "volume", 0.72f }
            } },
            { "Sequence Stab", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 9.0f },
                { "attack", 0.001f }, { "decay", 0.10f }, { "sustain", 0.0f }, { "release", 0.12f },
                { "filterCutoff", 3800.0f }, { "filterResonance", 0.55f },
                { "lfoRate", 1.0f }, { "lfoAmount", 0.05f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.40f }, { "delayMix", 0.22f },
                { "reverbMix", 0.18f }, { "volume", 0.78f }
            } },
            { "Polyphonic Stab", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 14.0f },
                { "attack", 0.005f }, { "decay", 0.25f }, { "sustain", 0.40f }, { "release", 0.40f },
                { "filterCutoff", 4500.0f }, { "filterResonance", 0.38f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.375f }, { "delayFeedback", 0.42f }, { "delayMix", 0.30f },
                { "reverbMix", 0.40f }, { "volume", 0.74f }
            } },
            // ---- MORE BASSES (35-39) --------------------------------------
            { "Wobble Bass", {
                { "oscType", 2 }, { "octave", -1 }, { "detune", 2.0f },
                { "attack", 0.002f }, { "decay", 0.20f }, { "sustain", 0.85f }, { "release", 0.30f },
                { "filterCutoff", 1200.0f }, { "filterResonance", 0.65f },
                { "lfoRate", 2.0f }, { "lfoAmount", 0.85f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.0f }, { "delayMix", 0.0f },
                { "reverbMix", 0.10f }, { "volume", 0.85f }
            } },
            { "Hard Acid", {
                { "oscType", 2 }, { "octave", -1 }, { "detune", 0.0f },
                { "attack", 0.001f }, { "decay", 0.12f }, { "sustain", 0.10f }, { "release", 0.15f },
                { "filterCutoff", 600.0f }, { "filterResonance", 0.92f },
                { "lfoRate", 8.0f }, { "lfoAmount", 0.40f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.45f }, { "delayMix", 0.18f },
                { "reverbMix", 0.10f }, { "volume", 0.82f }
            } },
            { "Saw Bass", {
                { "oscType", 1 }, { "octave", -1 }, { "detune", 8.0f },
                { "attack", 0.002f }, { "decay", 0.30f }, { "sustain", 0.70f }, { "release", 0.30f },
                { "filterCutoff", 2400.0f }, { "filterResonance", 0.30f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.05f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.20f }, { "delayMix", 0.10f },
                { "reverbMix", 0.12f }, { "volume", 0.82f }
            } },
            { "Pluck Bass", {
                { "oscType", 2 }, { "octave", -1 }, { "detune", 4.0f },
                { "attack", 0.001f }, { "decay", 0.10f }, { "sustain", 0.0f }, { "release", 0.10f },
                { "filterCutoff", 1800.0f }, { "filterResonance", 0.45f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.0f }, { "delayMix", 0.0f },
                { "reverbMix", 0.05f }, { "volume", 0.82f }
            } },
            { "Trance Bass", {
                { "oscType", 1 }, { "octave", -1 }, { "detune", 6.0f },
                { "attack", 0.002f }, { "decay", 0.18f }, { "sustain", 0.20f }, { "release", 0.16f },
                { "filterCutoff", 1500.0f }, { "filterResonance", 0.40f },
                { "lfoRate", 0.5f }, { "lfoAmount", 0.0f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.10f }, { "delayMix", 0.06f },
                { "reverbMix", 0.10f }, { "volume", 0.85f }
            } },
            // ---- MORE MOD/TEXTURE (40-44) ---------------------------------
            { "WT Glass Drift", {
                { "oscType", 4 }, { "octave", 0 }, { "detune", 12.0f },
                { "attack", 1.20f }, { "decay", 1.80f }, { "sustain", 0.78f }, { "release", 3.00f },
                { "filterCutoff", 6500.0f }, { "filterResonance", 0.20f },
                { "lfoRate", 0.30f }, { "lfoAmount", 0.32f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.45f }, { "delayMix", 0.30f },
                { "reverbMix", 0.62f }, { "volume", 0.68f },
                { "wtEnabled", 1.0f }, { "wtTable", 1.0f }, { "wtPosition", 0.42f },
                { "wtMorph", 0.78f }, { "wtFold", 0.10f }
            } },
            { "WT Razor Pulse", {
                { "oscType", 4 }, { "octave", 0 }, { "detune", 8.0f },
                { "attack", 0.003f }, { "decay", 0.28f }, { "sustain", 0.42f }, { "release", 0.30f },
                { "filterCutoff", 1900.0f }, { "filterResonance", 0.55f },
                { "lfoRate", 6.0f }, { "lfoAmount", 0.40f },
                { "delayTime", 0.1875f }, { "delayFeedback", 0.40f }, { "delayMix", 0.18f },
                { "reverbMix", 0.22f }, { "volume", 0.80f },
                { "wtEnabled", 1.0f }, { "wtTable", 4.0f }, { "wtPosition", 0.46f },
                { "wtMorph", 0.42f }, { "wtFold", 0.36f }
            } },
            { "Drone Texture", {
                { "oscType", 3 }, { "octave", -1 }, { "detune", 10.0f },
                { "attack", 3.20f }, { "decay", 4.20f }, { "sustain", 0.85f }, { "release", 6.00f },
                { "filterCutoff", 2200.0f }, { "filterResonance", 0.25f },
                { "lfoRate", 0.10f }, { "lfoAmount", 0.40f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.50f }, { "delayMix", 0.30f },
                { "reverbMix", 0.78f }, { "volume", 0.62f }
            } },
            { "Random LFO", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 4.0f },
                { "attack", 0.002f }, { "decay", 0.30f }, { "sustain", 0.60f }, { "release", 0.50f },
                { "filterCutoff", 4500.0f }, { "filterResonance", 0.55f },
                { "lfoRate", 16.0f }, { "lfoAmount", 0.95f },
                { "delayTime", 0.250f }, { "delayFeedback", 0.45f }, { "delayMix", 0.22f },
                { "reverbMix", 0.32f }, { "volume", 0.74f }
            } },
            { "Stutter Mod", {
                { "oscType", 2 }, { "octave", 0 }, { "detune", 5.0f },
                { "attack", 0.001f }, { "decay", 0.12f }, { "sustain", 0.20f }, { "release", 0.12f },
                { "filterCutoff", 3200.0f }, { "filterResonance", 0.50f },
                { "lfoRate", 16.0f }, { "lfoAmount", 0.85f },
                { "delayTime", 0.125f }, { "delayFeedback", 0.55f }, { "delayMix", 0.35f },
                { "reverbMix", 0.25f }, { "volume", 0.74f }
            } },
            // ---- MORE PADS (45-48) ----------------------------------------
            { "Trance Pad", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 18.0f },
                { "attack", 1.40f }, { "decay", 2.80f }, { "sustain", 0.85f }, { "release", 4.00f },
                { "filterCutoff", 4800.0f }, { "filterResonance", 0.22f },
                { "lfoRate", 0.30f }, { "lfoAmount", 0.22f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.50f }, { "delayMix", 0.32f },
                { "reverbMix", 0.72f }, { "volume", 0.68f }
            } },
            { "Cinematic Sweep", {
                { "oscType", 1 }, { "octave", 0 }, { "detune", 12.0f },
                { "attack", 4.50f }, { "decay", 4.50f }, { "sustain", 0.95f }, { "release", 6.00f },
                { "filterCutoff", 800.0f }, { "filterResonance", 0.35f },
                { "lfoRate", 0.05f }, { "lfoAmount", 0.95f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.50f }, { "delayMix", 0.30f },
                { "reverbMix", 0.85f }, { "volume", 0.65f }
            } },
            { "Choir Pad", {
                { "oscType", 0 }, { "octave", 0 }, { "detune", 6.0f },
                { "attack", 1.80f }, { "decay", 2.20f }, { "sustain", 0.90f }, { "release", 3.60f },
                { "filterCutoff", 4200.0f }, { "filterResonance", 0.16f },
                { "lfoRate", 4.5f }, { "lfoAmount", 0.10f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.40f }, { "delayMix", 0.22f },
                { "reverbMix", 0.78f }, { "volume", 0.66f },
                { "vibratoDepth", 0.20f }, { "vibratoRate", 4.5f }
            } },
            { "Glass Pad", {
                { "oscType", 4 }, { "octave", 0 }, { "detune", 8.0f },
                { "attack", 1.20f }, { "decay", 2.00f }, { "sustain", 0.80f }, { "release", 3.00f },
                { "filterCutoff", 7800.0f }, { "filterResonance", 0.18f },
                { "lfoRate", 0.40f }, { "lfoAmount", 0.20f },
                { "delayTime", 0.500f }, { "delayFeedback", 0.45f }, { "delayMix", 0.28f },
                { "reverbMix", 0.72f }, { "volume", 0.66f },
                { "wtEnabled", 1.0f }, { "wtTable", 7.0f }, { "wtPosition", 0.34f }
            } },
            // ---- SPECIAL (49-50) ------------------------------------------
            { "White Noise Riser", {
                { "oscType", 0 }, { "octave", 1 }, { "detune", 0.0f },
                { "noiseBlend", 0.85f }, { "oscBlend", 0.15f },
                { "attack", 6.00f }, { "decay", 0.10f }, { "sustain", 1.0f }, { "release", 0.20f },
                { "filterCutoff", 1500.0f }, { "filterResonance", 0.30f },
                { "lfoRate", 0.10f }, { "lfoAmount", 0.85f },
                { "delayTime", 0.125f }, { "delayFeedback", 0.30f }, { "delayMix", 0.15f },
                { "reverbMix", 0.45f }, { "volume", 0.55f }
            } },
            { "Snare Roll", {
                { "oscType", 0 }, { "octave", 0 }, { "detune", 0.0f },
                { "noiseBlend", 0.70f }, { "oscBlend", 0.30f },
                { "attack", 0.001f }, { "decay", 0.08f }, { "sustain", 0.0f }, { "release", 0.04f },
                { "filterCutoff", 5500.0f }, { "filterResonance", 0.40f },
                { "lfoRate", 16.0f }, { "lfoAmount", 0.40f },
                { "delayTime", 0.125f }, { "delayFeedback", 0.20f }, { "delayMix", 0.10f },
                { "reverbMix", 0.30f }, { "volume", 0.78f }
            } }
        };
        return presets;
    }

    static juce::StringArray tranceSynthPresetNames()
    {
        juce::StringArray out;
        for (const auto& p : tranceSynthPresets())
            out.add (juce::String (p.name));
        return out;
    }

    static void applyTranceSynthPreset (int presetIndex,
                                         std::map<juce::String, float>& values)
    {
        const auto& list = tranceSynthPresets();
        if (presetIndex < 0 || presetIndex >= (int) list.size()) return;
        for (const auto& kv : list[(size_t) presetIndex].values)
            values[kv.first] = kv.second;
    }

    // Per-preset value overrides so each preset actually sounds different,
    // not just a renamed copy of the defaults. Each lambda receives the
    // current parameter value map and tweaks it.
    static void applyPresetVariation (const juce::String& engine,
                                      int presetIndex,
                                      std::map<juce::String, float>& values)
    {
        auto set = [&] (const juce::String& id, float v) { values[id] = v; };

        if (engine == "synth")
        {
            applyTranceSynthPreset (presetIndex, values);
            return;
        }
        if (engine == "fx")
        {
            switch (presetIndex)
            {
                case 0: set ("drive", 0.20f); set ("filterCutoff", 8000.0f);
                        set ("delayMix", 0.15f); set ("reverbMix", 0.20f); break;
                case 1: set ("drive", 0.45f); set ("filterCutoff", 4500.0f);
                        set ("delayMix", 0.25f); set ("reverbMix", 0.30f); break;
                case 2: set ("drive", 0.10f); set ("filterCutoff", 12000.0f);
                        set ("reverbMix", 0.55f); break;
                case 3: set ("drive", 0.65f); set ("filterCutoff", 2500.0f);
                        set ("filterResonance", 0.50f); set ("delayMix", 0.35f); break;
                case 4: set ("drive", 0.30f); set ("delayMix", 0.45f);
                        set ("delayFeedback", 0.55f); set ("reverbMix", 0.30f); break;
                case 5: set ("drive", 0.55f); set ("filterCutoff", 1800.0f);
                        set ("reverbMix", 0.40f); break;
                case 6: set ("drive", 0.25f); set ("delayTime", 0.45f);
                        set ("delayMix", 0.50f); set ("reverbMix", 0.45f); break;
                case 7: set ("drive", 0.05f); set ("filterCutoff", 16000.0f);
                        set ("reverbMix", 0.75f); break;
            }
        }
        else /* sample */
        {
            switch (presetIndex)
            {
                case 0: set ("attack", 1.0f); set ("release", 3.2f);
                        set ("filterCutoff", 4200.0f); set ("reverbMix", 0.45f); break;
                case 1: set ("attack", 0.6f); set ("release", 2.0f);
                        set ("filterCutoff", 2800.0f); set ("vibratoDepth", 0.20f); break;
                case 2: set ("attack", 0.3f); set ("release", 4.0f);
                        set ("filterCutoff", 8000.0f); set ("reverbMix", 0.65f); break;
                case 3: set ("attack", 1.8f); set ("release", 5.5f);
                        set ("filterCutoff", 5000.0f); set ("reverbMix", 0.70f); break;
                case 4: set ("attack", 1.2f); set ("release", 3.5f);
                        set ("filterCutoff", 3500.0f); set ("vibratoDepth", 0.30f); break;
                case 5: set ("attack", 1.5f); set ("release", 4.2f);
                        set ("filterCutoff", 1800.0f); set ("reverbMix", 0.50f); break;
                case 6: set ("attack", 0.8f); set ("release", 3.0f);
                        set ("delayMix", 0.40f); set ("delayFeedback", 0.55f); break;
                case 7: set ("attack", 2.5f); set ("release", 6.0f);
                        set ("filterCutoff", 6500.0f); set ("reverbMix", 0.85f); break;
            }
        }
    }

    PatchCraftPack buildDemoPack (const juce::String& engine)
    {
        PatchCraftPack pack;
        pack.manifest.engine          = engine;
        pack.manifest.creator         = "PatchCraft";
        pack.manifest.category        = "Demo Instrument";
        pack.manifest.createdWith     = "PatchCraft Studio";
        pack.manifest.backgroundImage = "assets/background.png";

        // The "Cinematic Evolve Pad" name is shared across engines so every
        // out-of-the-box demo carries the same product identity. The sound
        // engine differs.
        pack.manifest.instrumentName = "Cinematic Evolve Pad";

        if (engine == "synth")
        {
            pack.parameters.loadSynthPalette();
            applySynthEvolvePadDefaults (pack.parameters);
        }
        else if (engine == "fx")
        {
            pack.manifest.instrumentName = "PatchCraft FX";
            pack.parameters.loadEffectPalette();
            applyEffectDefaults (pack.parameters);
        }
        else if (engine == "drum")
        {
            pack.manifest.instrumentName = "PatchCraft Drums";
            pack.manifest.category       = "Drum Machine";
            pack.parameters.loadSamplerPalette();
            applyDrumDefaults (pack.parameters);
        }
        else
        {
            pack.parameters.loadSamplerPalette();
            applySamplerDefaults (pack.parameters);
        }

        buildDemoLayout (pack.layout, pack.canvasSize, engine);
        pack.dspGraph.resetForEngine (engine);

        // Named factory presets. The synth engine ships 20 trance / psytrance
        // / electronic presets covering leads, arps, basses, mod-textures and
        // pads. Other engines retain their original 8 cinematic presets.
        juce::StringArray names;
        if (engine == "synth")
        {
            names = tranceSynthPresetNames();
        }
        else
        {
            names = juce::StringArray { "Deep Horizon", "Warm Motion", "Shimmering Clouds",
                                        "Ethereal Rise", "Vast Beauty", "Nightfall",
                                        "Floating Memories", "Infinite Space" };
        }
        for (int i = 0; i < names.size(); ++i)
        {
            Preset p;
            p.name = names[i];
            p.isDefault = (i == 0);
            for (auto& def : pack.parameters.getAll())
                p.values[def.id] = def.defaultValue;
            applyPresetVariation (engine, i, p.values);
            if (engine == "synth")
                p.tags = { "trance", "electronic" };
            pack.presets.push_back (p);
        }
        pack.manifest.defaultPreset = pack.presets[0].name;
        return pack;
    }

} // namespace patchcraft
