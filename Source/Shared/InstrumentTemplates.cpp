#include "InstrumentTemplates.h"
#include "PresetGenerator.h"

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
                                       int w = 78, int h = 94)
        {
            LayoutElement k;
            k.type = ElementType::Knob;
            k.id = id;
            k.x = x; k.y = y; k.width = w; k.height = h;
            k.label = label; k.parameterId = parameterId;
            k.style = "Vintage Gold";
            k.groupId = groupId;
            k.labelPosition = "bottom";
            k.labelSize = 10.5f;
            k.labelSpacing = 6.0f;
            k.labelOffsetX = 0.0f;
            k.labelOffsetY = 1.0f;
            return k;
        }

        // Builds the 8 macro knobs on a particular tab. Lays them out evenly.
        static void addKnobRow (LayoutModel& layout,
                                const juce::String& groupId,
                                std::initializer_list<std::pair<juce::String, juce::String>> knobs)
        {
            LayoutElement panel;
            panel.type = ElementType::Shape;
            panel.id = "panel_" + groupId;
            panel.x = 136; panel.y = 520; panel.width = 936; panel.height = 174;
            panel.groupId = groupId;
            panel.shapeKind = "roundedRect";
            panel.cornerRadius = 14.0f;
            panel.backgroundColour = juce::Colour (0x33141822);
            panel.borderColour = juce::Colour (0x8858f0c8);
            panel.strokeWidth = 1.0f;
            layout.add (panel);

            LayoutElement heading;
            heading.type = ElementType::Label;
            heading.id = "label_" + groupId;
            heading.x = 160; heading.y = 530; heading.width = 330; heading.height = 22;
            heading.groupId = groupId;
            heading.label = groupId.toUpperCase() + " CONTROLS";
            heading.textColour = juce::Colour (0xfff7f7ff);
            layout.add (heading);

            const int count = juce::jmin (8, (int) knobs.size());
            const int knobW = 78;
            const int knobH = 94;
            const int innerLeft = panel.x + 34;
            const int innerRight = panel.x + panel.width - 34;
            const int usable = innerRight - innerLeft - knobW;
            const int gap = count > 1 ? usable / (count - 1) : 0;
            const int y = panel.y + 54;

            int i = 0;
            for (auto& kv : knobs)
            {
                if (i >= 8) break;
                const int x = count <= 1
                    ? panel.x + (panel.width - knobW) / 2
                    : innerLeft + i * gap;
                layout.add (makeKnob ("knob_" + groupId + "_" + kv.second,
                                      kv.first, kv.second, x, y, groupId, knobW, knobH));
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

            // Hero artwork window. This is a styled well instead of a second
            // image layer so generated templates do not cover the designed
            // background wells or add placeholder "Artwork" text.
            { LayoutElement e; e.type = ElementType::Shape; e.id = "hero_frame";
              e.x = 40; e.y = 92; e.width = 1200; e.height = 348;
              e.shapeKind = "roundedRect";
              e.cornerRadius = 18.0f;
              e.backgroundColour = juce::Colour (0x22141822);
              e.borderColour = juce::Colour (0x8858f0c8);
              e.strokeWidth = 1.0f;
              layout.add (e); }

            // Player top-section artwork mock: a real 3:2 library-art slot
            // with companion product-info labels so templates show how the
            // buyer-facing Player header will read before final art is dropped in.
            { LayoutElement e; e.type = ElementType::Image; e.id = "player_artwork";
              e.asset = "assets/thumbnail.png";
              e.semanticRole = "playerArtwork";
              e.x = 76; e.y = 124; e.width = 390; e.height = 260;
              e.cornerRadius = 16.0f;
              e.locked = true;
              layout.add (e); }

            { LayoutElement e; e.type = ElementType::Shape; e.id = "player_artwork_frame";
              e.semanticRole = "playerArtworkFrame";
              e.x = 76; e.y = 124; e.width = 390; e.height = 260;
              e.shapeKind = "roundedRect";
              e.cornerRadius = 16.0f;
              e.backgroundColour = juce::Colour (0x00000000);
              e.borderColour = juce::Colour (0xa858f0c8);
              e.strokeWidth = 1.4f;
              e.locked = true;
              layout.add (e); }

            const auto productName = engine == "fx" ? "Modular Motion FX"
                : engine == "drum" ? "Neon Pulse Drum Machine"
                : engine == "synth" ? "Cinematic Evolve Pad"
                : "Organic Tape Sampler";
            const juce::String productKind = engine == "fx" ? "FX PLUGIN"
                : engine == "drum" ? "DRUM MACHINE"
                : engine == "synth" ? "SYNTH INSTRUMENT"
                : "SAMPLE INSTRUMENT";

            { LayoutElement e; e.type = ElementType::Label; e.id = "player_artwork_badge";
              e.semanticRole = "playerArtworkInfo";
              e.x = 510; e.y = 128; e.width = 250; e.height = 22;
              e.label = "LIBRARY ARTWORK 300 x 200";
              e.textColour = juce::Colour (0xff58f0c8);
              e.labelSize = 11.0f;
              e.locked = true;
              layout.add (e); }

            { LayoutElement e; e.type = ElementType::Label; e.id = "player_product_name";
              e.semanticRole = "playerHeaderTitle";
              e.x = 510; e.y = 158; e.width = 600; e.height = 40;
              e.label = productName;
              e.textColour = juce::Colour (0xfff7f7ff);
              e.labelSize = 24.0f;
              e.locked = true;
              layout.add (e); }

            { LayoutElement e; e.type = ElementType::Label; e.id = "player_product_kind";
              e.semanticRole = "playerHeaderMetadata";
              e.x = 512; e.y = 206; e.width = 420; e.height = 22;
              e.label = productKind + "  |  PLAYER TOP SECTION";
              e.textColour = juce::Colour (0xffb8c8d6);
              e.labelSize = 12.0f;
              e.locked = true;
              layout.add (e); }

            { LayoutElement e; e.type = ElementType::Label; e.id = "player_product_description";
              e.semanticRole = "playerHeaderDescription";
              e.x = 512; e.y = 238; e.width = 590; e.height = 44;
              e.label = "Mock product info preview: final pack art, title, category, and creator metadata are reviewed here before export.";
              e.textColour = juce::Colour (0xffdce3ea);
              e.labelSize = 13.0f;
              e.locked = true;
              layout.add (e); }

            { LayoutElement e; e.type = ElementType::Label; e.id = "player_artwork_dimensions";
              e.semanticRole = "playerArtworkSpec";
              e.x = 512; e.y = 318; e.width = 420; e.height = 24;
              e.label = "Artwork slot: x76 y124  w390 h260  (3:2)";
              e.textColour = juce::Colour (0xff8fa0ad);
              e.labelSize = 11.0f;
              e.locked = true;
              layout.add (e); }

            // Tab strip
            { LayoutElement e; e.type = ElementType::TabPanel; e.id = "tabs";
              e.x = 370; e.y = 470; e.width = 540; e.height = 34;
              if (engine == "fx")
                  e.tabs = { "Main", "Filter", "Delay", "Reverb" };
              else if (engine == "drum")
                  e.tabs = { "Pads", "Pattern", "Amp", "Filter", "FX" };
              else
                  // Each tab owns a distinct slice of the signal path so no
                  // two tabs show the same controls. Arp lives in its own
                  // workspace once the DSP graph adds an arp block.
                  e.tabs = { "Main", "Amp", "Filter", "Mod", "FX", "Space" };
              layout.add (e); }

            // Master section (always visible)
            if (engine != "fx")
            {
                LayoutElement vol; vol.type = ElementType::Knob; vol.id = "masterVol";
                vol.x = 1090; vol.y = 478; vol.width = 72; vol.height = 78;
                vol.label = "Volume"; vol.parameterId = "volume";
                vol.labelPosition = "bottom"; vol.labelSize = 10.0f; vol.labelSpacing = 2.0f;
                layout.add (vol);

                LayoutElement pan; pan.type = ElementType::Knob; pan.id = "masterPan";
                pan.x = 1170; pan.y = 478; pan.width = 72; pan.height = 78;
                pan.label = "Pan"; pan.parameterId = "pan";
                pan.labelPosition = "bottom"; pan.labelSize = 10.0f; pan.labelSpacing = 2.0f;
                layout.add (pan);
            }

            { LayoutElement e; e.type = ElementType::Meter; e.id = "outMeter";
              e.x = 1090; e.y = 568; e.width = 150; e.height = 24;
              e.label = "Output"; layout.add (e); }

            // Performance sliders (left side)
            if (engine != "fx" && engine != "drum")
            {
                LayoutElement pitch; pitch.type = ElementType::Slider; pitch.id = "pitchwheel";
                pitch.x = 40; pitch.y = 485; pitch.width = 34; pitch.height = 205;
                pitch.label = "Pitch"; pitch.parameterId = "pitchWheel"; layout.add (pitch);

                LayoutElement mod; mod.type = ElementType::Slider; mod.id = "modwheel";
                mod.x = 82; mod.y = 485; mod.width = 34; mod.height = 205;
                mod.label = "Mod"; mod.parameterId = "modWheel"; layout.add (mod);
            }

            // Bottom keyboard
            if (keyboardVisible)
            {
                LayoutElement kb; kb.type = ElementType::Keyboard; kb.id = "keyboard";
                kb.x = 40; kb.y = 718; kb.width = 1200; kb.height = 62;
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
            // -------- Synth demo. Each tab owns a distinct slice of the
            // signal path: oscillator, amp, filter, modulation, distortion,
            // spatial FX, arpeggiator. No parameter appears in two tabs -
            // duplicating controls makes them feel identical and obscures
            // what each section actually shapes.
            //
            // Main: oscillators and pitch (sound source)
            addKnobRow (layout, "main", {
                { "Wave",       "oscType" },
                { "Wave 2",     "osc2Type" },
                { "Blend",      "oscBlend" },
                { "Octave",     "octave" },
                { "Detune",     "detune" },
                { "Sub",        "subBlend" },
                { "Noise",      "noiseBlend" },
                { "Osc2 Det.",  "osc2Detune" }
            });

            // Amp: amplitude envelope + master out
            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            // Filter: cutoff + resonance + (in-section) filter shape
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" }
            });

            // Mod: LFO and vibrato dedicated to modulation
            addKnobRow (layout, "mod", {
                { "LFO Rate",   "lfoRate" },
                { "LFO Amt",    "lfoAmount" },
                { "Vib Rate",   "vibratoRate" },
                { "Vib Depth",  "vibratoDepth" }
            });

            // FX: output-bus shaping (the synth engine doesn't have a drive
            // stage, so this tab is dedicated to the things its palette
            // actually exposes: stereo and output gain).
            addKnobRow (layout, "fx", {
                { "Width",  "stereoWidth" },
                { "Mono",   "monoMaker" },
                { "Out dB", "outputGainDb" }
            });

            // Space: time-based effects only
            addKnobRow (layout, "space", {
                { "Reverb",   "reverbMix" },
                { "Dly Time", "delayTime" },
                { "Dly Fb",   "delayFeedback" },
                { "Dly Mix",  "delayMix" }
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

            { LayoutElement pattern; pattern.type = ElementType::DrumGrid;
              pattern.id = "drumPatternGrid";
              pattern.x = 180; pattern.y = 520; pattern.width = 860; pattern.height = 170;
              pattern.groupId = "pattern";
              pattern.label = "Pattern Sequencer";
              pattern.drumTracks = 8; pattern.drumSteps = 16; pattern.drumPattern = 0;
              pattern.cornerRadius = 8.0f;
              pattern.backgroundColour = juce::Colour (0x33141822);
              layout.add (pattern); }

            // Macro knob row beneath the pad grid (groupId == "pads" tab).
            // Each drum tab owns a distinct slice of the chain.
            // Pads: per-hit macros (level, character, sample shape)
            addKnobRow (layout, "pads", {
                { "Pitch",  "samplePitch" },
                { "Start",  "sampleStart" },
                { "Length", "sampleLength" },
                { "Slice",  "sampleSlice" },
                { "Glitch", "sampleGlitch" }
            });

            // Amp: amplitude envelope + master out
            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            // Filter: just the filter section
            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" }
            });

            // FX: delay + reverb (sampler palette has no drive stage).
            addKnobRow (layout, "fx", {
                { "Dly Mix",  "delayMix" },
                { "Dly Time", "delayTime" },
                { "Dly Fb",   "delayFeedback" },
                { "Reverb",   "reverbMix" }
            });
        }
        else if (engine == "fx")
        {
            // FX engine tabs - Main, Filter, Delay, Reverb. Each tab owns a
            // distinct part of the chain so no two tabs show the same knob.
            addKnobRow (layout, "main", {
                { "Drive", "drive" },
                { "Mix",   "mix" },
                { "Vol",   "volume" },
                { "Pan",   "pan" }
            });
            addKnobRow (layout, "filter", {
                { "Cutoff", "filterCutoff" },
                { "Res",    "filterResonance" }
            });
            addKnobRow (layout, "delay", {
                { "Time",     "delayTime" },
                { "Feedback", "delayFeedback" },
                { "Mix",      "delayMix" }
            });
            addKnobRow (layout, "reverb", {
                { "Mix", "reverbMix" }
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

            // Sampler demo - tabs are dedicated to disjoint slices of the
            // signal path. Sampler-specific controls live in Main; the
            // remaining tabs follow the same Amp/Filter/Mod/FX/Space split
            // as the synth.
            addKnobRow (layout, "main", {
                { "Start",   "sampleStart" },
                { "Length",  "sampleLength" },
                { "Pitch",   "samplePitch" },
                { "Slice",   "sampleSlice" },
                { "Reverse", "sampleReverse" }
            });

            addKnobRow (layout, "amp", {
                { "Attack",  "attack" },
                { "Decay",   "decay" },
                { "Sustain", "sustain" },
                { "Release", "release" },
                { "Volume",  "volume" },
                { "Pan",     "pan" }
            });

            addKnobRow (layout, "filter", {
                { "Cutoff",  "filterCutoff" },
                { "Reso",    "filterResonance" }
            });

            addKnobRow (layout, "mod", {
                { "Vib Rate", "vibratoRate" },
                { "Vib Dep",  "vibratoDepth" }
            });

            // Sampler palette has no drive stage either - same output bus
            // approach as the synth tab.
            addKnobRow (layout, "fx", {
                { "Width",  "stereoWidth" },
                { "Mono",   "monoMaker" },
                { "Out dB", "outputGainDb" }
            });

            addKnobRow (layout, "space", {
                { "Reverb",   "reverbMix" },
                { "Dly Time", "delayTime" },
                { "Dly Fb",   "delayFeedback" },
                { "Dly Mix",  "delayMix" }
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

    static juce::String inferSynthPresetTheme (const juce::String& name)
    {
        const auto lower = name.toLowerCase();
        if (lower.contains ("arp") || lower.contains ("sequence") || lower.contains ("gate"))
            return "Arps";
        if (lower.contains ("bass") || lower.contains ("reese") || lower.contains ("sub") || lower.contains ("acid"))
            return "Bass";
        if (lower.contains ("pluck") || lower.contains ("stab") || lower.contains ("bell"))
            return "Plucks";
        if (lower.contains ("choir") || lower.contains ("vocal") || lower.contains ("string"))
            return "Strings";
        if (lower.contains ("wt") || lower.contains ("wavetable") || lower.contains ("glass"))
            return "Wavetables";
        if (lower.contains ("pad") || lower.contains ("bloom") || lower.contains ("ambient") || lower.contains ("sweep") || lower.contains ("drone"))
            return "Pads";
        if (lower.contains ("lfo") || lower.contains ("mod") || lower.contains ("motion") || lower.contains ("stutter") || lower.contains ("texture"))
            return "Motion";
        if (lower.contains ("fx") || lower.contains ("riser") || lower.contains ("roll"))
            return "FX";
        return "Leads";
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

        if (engine == "synth")
        {
            const auto& curated = tranceSynthPresets();
            for (int i = 0; i < (int) curated.size(); ++i)
            {
                Preset preset;
                preset.name = curated[(size_t) i].name;
                preset.theme = inferSynthPresetTheme (preset.name);
                preset.tags = { preset.theme, "synth", "factory", "curated" };
                preset.generated = false;
                preset.isDefault = pack.presets.empty();
                preset.description = "Curated PatchCraft synth preset. The name is backed by a full parameter recipe and patch state, not a random rename.";
                for (const auto& def : pack.parameters.getAll())
                    preset.values[def.id] = def.defaultValue;
                applyTranceSynthPreset (i, preset.values);
                pack.presets.push_back (std::move (preset));
            }

            LiveValueStore anchorValues;
            for (const auto& def : pack.parameters.getAll())
                anchorValues.setValue (def.id, def.defaultValue);

            int presetOffset = 0;
            for (const auto& theme : PresetGenerator::themes())
            {
                PresetGenerationOptions options;
                options.theme = theme;
                options.count = theme.equalsIgnoreCase ("Wavetables") ? 10 : 12;
                options.seed = (juce::uint32) (0x5eed1200u + (juce::uint32) presetOffset * 131u);
                options.includeCurrentAsAnchor = true;
                auto generated = PresetGenerator::generate (pack.parameters, anchorValues, engine, options);
                for (auto& preset : generated)
                {
                    preset.isDefault = false;
                    preset.generated = false;
                    preset.tags.addIfNotAlreadyThere ("factory");
                    preset.description = "Factory patch preset for " + theme
                        + ". Designed to apply musically to PatchCraft synth instruments.";
                    pack.presets.push_back (std::move (preset));
                    if ((int) pack.presets.size() >= 120)
                        break;
                }
                if ((int) pack.presets.size() >= 120)
                    break;
                presetOffset += options.count;
            }

            while ((int) pack.presets.size() < 120)
            {
                PresetGenerationOptions options;
                options.theme = "Motion";
                options.count = 1;
                options.seed = (juce::uint32) (0x5eed2000u + (juce::uint32) pack.presets.size() * 97u);
                options.includeCurrentAsAnchor = true;
                auto generated = PresetGenerator::generate (pack.parameters, anchorValues, engine, options);
                if (! generated.empty())
                {
                    auto preset = std::move (generated.front());
                    preset.name = "Factory Motion " + juce::String ((int) pack.presets.size() + 1);
                    preset.generated = false;
                    preset.tags.addIfNotAlreadyThere ("factory");
                    pack.presets.push_back (std::move (preset));
                }
            }
        }
        else
        {
            const juce::StringArray names { "Deep Horizon", "Warm Motion", "Shimmering Clouds",
                                            "Ethereal Rise", "Vast Beauty", "Nightfall",
                                            "Floating Memories", "Infinite Space" };
            for (int i = 0; i < names.size(); ++i)
            {
                Preset p;
                p.name = names[i];
                p.isDefault = (i == 0);
                for (auto& def : pack.parameters.getAll())
                    p.values[def.id] = def.defaultValue;
                applyPresetVariation (engine, i, p.values);
                pack.presets.push_back (p);
            }
        }
        ensurePresetBackedPatches (pack, true);
        pack.manifest.defaultPreset = pack.presets[0].name;
        return pack;
    }

} // namespace patchcraft
