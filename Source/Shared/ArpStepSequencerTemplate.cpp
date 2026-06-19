#include "ArpStepSequencerTemplate.h"

#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    namespace
    {
        struct StepPattern
        {
            std::array<int, 16> on {};
            std::array<float, 16> notes {};
            std::array<float, 16> velocity {};
            float gate = 0.58f;
            float rate = 1.0f;
            float swing = 0.06f;
        };

        static const std::array<StepPattern, kArpStepPatternCount>& stepPatterns()
        {
            static const std::array<StepPattern, kArpStepPatternCount> patterns =
            {{
                { {{1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0}},
                  {{0,0,3,0,5,0,7,0,10,0,12,0,15,0,17,0}},
                  {{0.88f,0.2f,0.72f,0.2f,0.84f,0.2f,0.76f,0.2f,0.82f,0.2f,0.74f,0.2f,0.8f,0.2f,0.7f,0.2f}}, 0.42f, 1.0f, 0.04f },
                { {{1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0}},
                  {{0,0,0,0,12,0,0,0,24,0,0,0,36,0,0,0}},
                  {{0.9f,0.2f,0.2f,0.2f,0.82f,0.2f,0.2f,0.2f,0.78f,0.2f,0.2f,0.2f,0.74f,0.2f,0.2f,0.2f}}, 0.56f, 1.0f, 0.06f },
                { {{1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,0}},
                  {{0,0,0,7,0,0,10,0,0,12,0,0,15,0,0,17}},
                  {{0.86f,0.2f,0.2f,0.7f,0.2f,0.2f,0.74f,0.2f,0.2f,0.68f,0.2f,0.2f,0.72f,0.2f,0.2f,0.66f}}, 0.36f, 1.0f, 0.14f },
                { {{1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0}},
                  {{0,0,0,0,0,0,3,0,0,0,0,0,7,0,0,0}},
                  {{0.72f,0.2f,0.2f,0.2f,0.2f,0.2f,0.64f,0.2f,0.2f,0.2f,0.2f,0.2f,0.6f,0.2f,0.2f,0.2f}}, 0.72f, 0.5f, 0.02f },
                { {{1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0}},
                  {{0,1,0,0,3,4,0,0,7,8,0,0,10,11,0,0}},
                  {{0.92f,0.78f,0.2f,0.2f,0.84f,0.76f,0.2f,0.2f,0.8f,0.74f,0.2f,0.2f,0.76f,0.7f,0.2f,0.2f}}, 0.28f, 1.0f, 0.08f },
                { {{1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0}},
                  {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
                  {{0.8f,0.2f,0.2f,0.2f,0.8f,0.2f,0.2f,0.2f,0.8f,0.2f,0.2f,0.2f,0.8f,0.2f,0.2f,0.2f}}, 0.58f, 0.5f, 0.0f }
            }};
            return patterns;
        }

        static LayoutElement makeKnob (const juce::String& id, const juce::String& label,
                                       const juce::String& parameterId,
                                       int x, int y, int w = 80, int h = 90)
        {
            LayoutElement k;
            k.type = ElementType::Knob;
            k.id = id;
            k.x = x;
            k.y = y;
            k.width = w;
            k.height = h;
            k.label = label;
            k.parameterId = parameterId;
            k.style = "Vintage Gold";
            k.labelPosition = "bottom";
            k.labelSize = 10.0f;
            k.accentColour = juce::Colour (0xff9b6dff);
            return k;
        }

        static void buildArpLayout (LayoutModel& layout, CanvasSize& canvas)
        {
            canvas.width = 1280;
            canvas.height = 800;
            layout.clear();

            LayoutElement title;
            title.type = ElementType::Label;
            title.id = "seq_title";
            title.label = "16-STEP ORBIT";
            title.x = 64;
            title.y = 224;
            title.width = 220;
            title.height = 22;
            title.labelSize = 14.0f;
            title.textColour = juce::Colour (0xffeef6ff);
            layout.add (title);

            LayoutElement macroTitle;
            macroTitle.type = ElementType::Label;
            macroTitle.id = "macro_title";
            macroTitle.label = "TONE + MOTION";
            macroTitle.x = 760;
            macroTitle.y = 224;
            macroTitle.width = 220;
            macroTitle.height = 22;
            macroTitle.labelSize = 14.0f;
            macroTitle.textColour = juce::Colour (0xffeef6ff);
            layout.add (macroTitle);

            LayoutElement retrigger;
            retrigger.type = ElementType::Toggle;
            retrigger.id = "orbit_retrigger";
            retrigger.label = "RTRG";
            retrigger.parameterId = "retrigger";
            retrigger.x = 64;
            retrigger.y = 252;
            retrigger.width = 92;
            retrigger.height = 28;
            retrigger.accentColour = juce::Colour (0xff9b6dff);
            layout.add (retrigger);

            LayoutElement multiLane;
            multiLane.type = ElementType::Toggle;
            multiLane.id = "orbit_multi_lane";
            multiLane.label = "Multi";
            multiLane.parameterId = "arpLaneMultiLane";
            multiLane.x = 168;
            multiLane.y = 252;
            multiLane.width = 108;
            multiLane.height = 28;
            multiLane.accentColour = juce::Colour (0xff20e0ff);
            layout.add (multiLane);

            LayoutElement target;
            target.type = ElementType::Dropdown;
            target.id = "orbit_target";
            target.label = "Target";
            target.parameterId = "arpLaneTarget";
            target.x = 760;
            target.y = 252;
            target.width = 118;
            target.height = 28;
            layout.add (target);

            LayoutElement sound;
            sound.type = ElementType::Dropdown;
            sound.id = "orbit_sound";
            sound.label = "Sound";
            sound.parameterId = "arpLaneSound";
            sound.x = 888;
            sound.y = 252;
            sound.width = 118;
            sound.height = 28;
            layout.add (sound);

            LayoutElement orbit;
            orbit.type = ElementType::ArpLane;
            orbit.id = "step_orbit";
            orbit.label = "16 Steps";
            orbit.x = 56;
            orbit.y = 256;
            orbit.width = 648;
            orbit.height = 400;
            orbit.arpLaneIndex = 0;
            orbit.arpLaneSteps = 16;
            orbit.arpLaneMode = "multiRing";
            orbit.arpLaneTarget = "notes";
            orbit.cornerRadius = 12.0f;
            orbit.backgroundColour = juce::Colour (0xee090d12);
            orbit.borderColour = juce::Colour (0xff4a5568);
            orbit.accentColour = juce::Colour (0xff9b6dff);
            layout.add (orbit);

            layout.add (makeKnob ("knob_rate", "Rate", "arpLaneRate", 787, 298, 51, 73));
            layout.add (makeKnob ("knob_gate", "Gate", "arpLaneGate", 904, 298, 51, 73));
            layout.add (makeKnob ("knob_swing", "Swing", "arpLaneSwing", 1021, 298, 51, 73));
            layout.add (makeKnob ("knob_prob", "Chance", "arpLaneProbability", 1138, 298, 51, 73));

            layout.add (makeKnob ("knob_cutoff", "Cutoff", "filterCutoff", 787, 375, 51, 73));
            layout.add (makeKnob ("knob_res", "Res", "filterResonance", 904, 375, 51, 73));
            layout.add (makeKnob ("knob_delay", "Delay", "delayMix", 1021, 375, 51, 73));
            layout.add (makeKnob ("knob_space", "Space", "reverbMix", 1138, 375, 51, 73));

            layout.add (makeKnob ("knob_vol", "Volume", "volume", 1126, 596, 72, 94));
            layout.add (makeKnob ("knob_pan", "Pan", "pan", 1126, 690, 72, 94));

            LayoutElement meter;
            meter.type = ElementType::Meter;
            meter.id = "out_meter";
            meter.label = "Output";
            meter.x = 760;
            meter.y = 642;
            meter.width = 456;
            meter.height = 22;
            layout.add (meter);
        }

        static void buildArpDspGraph (DspGraph& graph)
        {
            graph.blocks.clear();
            graph.edges.clear();
            graph.modulation.clear();
            graph.userConfigured = true;

            DspBlock osc;
            osc.id = "seq_source";
            osc.section = "source";
            osc.type = "oscillator";
            osc.name = "SEQ OSC";
            osc.targetId = "volume";
            osc.values["oscType"] = 0.0f;
            osc.values["oscBlend"] = 0.12f;
            osc.values["octave"] = 0.5f;
            osc.values["detune"] = 0.08f;
            osc.metadata["uiX"] = "40";
            osc.metadata["uiY"] = "80";
            graph.blocks.push_back (osc);

            DspBlock filter;
            filter.id = "seq_filter";
            filter.section = "filter";
            filter.type = "stateVariable";
            filter.name = "SEQ FILTER";
            filter.targetId = "filterCutoff";
            filter.values["cutoff"] = 0.62f;
            filter.values["resonance"] = 0.22f;
            filter.metadata["uiX"] = "340";
            filter.metadata["uiY"] = "80";
            graph.blocks.push_back (filter);

            DspBlock env;
            env.id = "seq_amp";
            env.section = "amp";
            env.type = "adsr";
            env.name = "SEQ AMP";
            env.targetId = "attack";
            env.values["attack"] = 0.01f;
            env.values["decay"] = 0.22f;
            env.values["sustain"] = 0.18f;
            env.values["release"] = 0.24f;
            env.metadata["uiX"] = "340";
            env.metadata["uiY"] = "240";
            graph.blocks.push_back (env);

            DspBlock arp;
            arp.id = "seq_arp";
            arp.section = "mod";
            arp.type = "midiPlayground";
            arp.name = "STEP ARP";
            arp.targetId = "filterCutoff";
            arp.metadata["uiX"] = "640";
            arp.metadata["uiY"] = "80";
            seedArpStepPattern (arp, 0);
            graph.blocks.push_back (arp);

            DspBlock delay;
            delay.id = "seq_delay";
            delay.section = "fx";
            delay.type = "delay";
            delay.name = "SEQ DELAY";
            delay.targetId = "delayMix";
            delay.values["delayTime"] = 0.1875f;
            delay.values["delayFeedback"] = 0.28f;
            delay.values["delayMix"] = 0.14f;
            delay.values["sync"] = 1.0f;
            delay.metadata["uiX"] = "940";
            delay.metadata["uiY"] = "80";
            graph.blocks.push_back (delay);

            DspBlock out;
            out.id = "seq_output";
            out.section = "out";
            out.type = "limiter";
            out.name = "OUTPUT";
            out.targetId = "volume";
            out.values["outputLimiter"] = 1.0f;
            out.values["outputCeilingDb"] = -0.8f;
            out.metadata["uiX"] = "1240";
            out.metadata["uiY"] = "80";
            graph.blocks.push_back (out);

            auto addEdge = [&] (const juce::String& src, const juce::String& dst)
            {
                DspGraphEdge edge;
                edge.id = src + "_to_" + dst;
                edge.sourceNodeId = src;
                edge.targetNodeId = dst;
                edge.signalType = DspSignalType::audio;
                edge.enabled = true;
                graph.edges.push_back (std::move (edge));
            };

            addEdge ("seq_source", "seq_filter");
            addEdge ("seq_filter", "seq_amp");
            addEdge ("seq_amp", "seq_delay");
            addEdge ("seq_delay", "seq_output");

            DspGraphEdge eventEdge;
            eventEdge.id = "seq_arp_event";
            eventEdge.sourceNodeId = "seq_arp";
            eventEdge.targetNodeId = "seq_source";
            eventEdge.signalType = DspSignalType::event;
            eventEdge.enabled = true;
            graph.edges.push_back (eventEdge);
        }

        static juce::String presetDescription (const juce::String& name)
        {
            if (name == "Driving 16ths") return "Fast 16th pulse — hold Cm7, tight gate, bright filter.";
            if (name == "Octave Ladder") return "Quarter-note jumps up octaves — good for hook lines.";
            if (name == "Syncopated Funk") return "Off-beat 16ths with swing — try Em9.";
            if (name == "Wide Pad Arp") return "Slow half-note arp with space and long gate — hold Am.";
            if (name == "Acid Step") return "Short resonant steps at half rate — classic 303 feel.";
            if (name == "Init Quarter Notes") return "Simple quarter-note pulse — blank canvas for your pattern.";
            return "Musical step pattern. Hold a chord at 120 BPM.";
        }
    }

    void seedArpStepPattern (DspBlock& block, int patternIndex)
    {
        const auto& patterns = stepPatterns();
        const auto& p = patterns[(size_t) juce::jlimit (0, kArpStepPatternCount - 1, patternIndex)];

        block.values["arpSteps"] = 16.0f;
        block.values["rate"] = p.rate;
        block.values["sync"] = 1.0f;
        block.values["arpGate"] = p.gate;
        block.values["mpProbability"] = 1.0f;
        block.values["mpActiveBank"] = 0.0f;
        block.values["mpMultiLane"] = 0.0f;

        for (int step = 0; step < 16; ++step)
        {
            const auto suffix = juce::String (step);
            block.values["mpStep" + suffix + "On"] = p.on[(size_t) step] != 0 ? 1.0f : 0.0f;
            block.values["arpNote" + suffix] = p.notes[(size_t) step];
            block.values["mpVelocity" + suffix] = p.velocity[(size_t) step];
            block.values["mpGate" + suffix] = p.gate;
            block.values["mpStepProb" + suffix] = 1.0f;
        }
    }

    PatchCraftPack buildArpStepSequencerPack()
    {
        PatchCraftPack pack;
        pack.manifest.engine = "synth";
        pack.manifest.instrumentName = "Arp Step Sequencer Studio";
        pack.manifest.creator = "PatchCraft";
        pack.manifest.category = "Step Sequencer Template";
        pack.manifest.description = "Flagship arp/step template: aligned orbit UI, category-routed DSP graph, six musical presets with per-preset step data.";
        pack.manifest.createdWith = "PatchCraft Studio";
        pack.manifest.playerDisplayName = "Arp Step Sequencer";
        pack.manifest.playerTagline = "16-step orbit sequencer with rate, gate, swing, and tone macros";
        pack.manifest.playerAccentColour = juce::Colour (0xff9b6dff);
        pack.manifest.playerBackgroundColour = juce::Colour (0xff07090f);
        pack.manifest.playerPanelColour = juce::Colour (0xff101722);
        pack.manifest.tags = { "template", "arp", "sequencer", "step", "factory-demo", "musical", "true-arp" };
        pack.manifest.defaultPreset = "Driving 16ths";

        pack.parameters.loadSynthPalette();
        buildArpLayout (pack.layout, pack.canvasSize);
        buildArpDspGraph (pack.dspGraph);

        const juce::StringArray presetNames {
            "Driving 16ths", "Octave Ladder", "Syncopated Funk",
            "Wide Pad Arp", "Acid Step", "Init Quarter Notes"
        };

        for (int i = 0; i < presetNames.size(); ++i)
        {
            const auto& pattern = stepPatterns()[(size_t) i];
            Preset preset;
            preset.name = presetNames[i];
            preset.theme = "Arps";
            preset.isDefault = (i == 0);
            preset.tags = { "arp", "sequencer", "template", "true-arp", "steps" };
            preset.description = presetDescription (preset.name);

            for (const auto& def : pack.parameters.getAll())
                preset.values[def.id] = def.defaultValue;

            preset.values["attack"] = i == 3 ? 0.08f : 0.01f;
            preset.values["decay"] = i == 3 ? 0.55f : 0.22f;
            preset.values["sustain"] = i == 3 ? 0.62f : 0.18f;
            preset.values["release"] = i == 3 ? 0.72f : 0.28f;
            preset.values["filterCutoff"] = i == 4 ? 3200.0f : (i == 3 ? 6800.0f : 5200.0f);
            preset.values["filterResonance"] = i == 4 ? 0.52f : 0.16f;
            preset.values["delayMix"] = i == 3 ? 0.22f : 0.12f;
            preset.values["reverbMix"] = i == 3 ? 0.32f : 0.08f;
            preset.values["volume"] = 0.78f;
            preset.values["arpLaneRate"] = pattern.rate;
            preset.values["arpLaneGate"] = pattern.gate;
            preset.values["arpLaneSwing"] = pattern.swing;
            preset.values["projectBpm"] = 120.0f;
            preset.values["bpmSync"] = 1.0f;
            pack.presets.push_back (std::move (preset));
        }

        ensurePresetBackedPatches (pack, true);
        return pack;
    }
}
