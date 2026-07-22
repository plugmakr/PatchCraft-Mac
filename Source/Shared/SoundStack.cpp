#include "SoundStack.h"
#include "ArpeggiatorRuntime.h"
#include "DrumMachineUtil.h"

namespace patchcraft
{
    namespace
    {
        static DspBlock& addBlock (DspGraph& graph,
                                   juce::String id,
                                   juce::String section,
                                   juce::String type,
                                   juce::String name)
        {
            DspBlock b;
            b.id = std::move (id);
            b.section = std::move (section);
            b.type = std::move (type);
            b.name = std::move (name);
            b.enabled = true;
            graph.blocks.push_back (std::move (b));
            return graph.blocks.back();
        }

        static void addAudioEdge (DspGraph& graph,
                                  juce::String from,
                                  juce::String to,
                                  float gain = 1.0f)
        {
            if (from.isEmpty() || to.isEmpty())
                return;

            DspGraphEdge edge;
            edge.id = from + "_to_" + to;
            edge.sourceNodeId = std::move (from);
            edge.targetNodeId = std::move (to);
            edge.gain = gain;
            graph.edges.push_back (std::move (edge));
        }

        static void setCommonQuickEdits (DspGraph& graph, const juce::String& engineId)
        {
            graph.quickEditControls["source"] = engineId == "synth"
                ? juce::StringArray { "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend", "volume" }
                : juce::StringArray { "volume", "pan", "sampleStart", "sampleLength" };
            graph.quickEditControls["shape"] = { "filterCutoff", "filterResonance", "attack", "decay", "sustain", "release" };
            graph.quickEditControls["fx"] = { "drive", "mix", "delayTime", "delayFeedback", "delayMix", "reverbMix" };
        }
    }

    void SoundStack::resetSimpleGraph (DspGraph& graph, const juce::String& engineId)
    {
        graph.blocks.clear();
        graph.edges.clear();
        graph.macros.clear();
        graph.modulation.clear();
        graph.automation.clear();
        graph.quickEditControls.clear();
        graph.userConfigured = false;

        if (engineId == "fx")
        {
            auto& source = addBlock (graph, "source", "source", "liveInput", "Input Source");
            source.targetId = "volume";
            source.values["volume"] = 0.85f;
            source.values["mix"] = 1.0f;
        }
        else if (engineId == "synth")
        {
            auto& source = addBlock (graph, "source", "source", "oscillator", "Synth Source");
            source.targetId = "volume";
            source.values["oscType"] = 1.0f;
            source.values["osc2Type"] = 0.0f;
            source.values["oscBlend"] = 0.1f;
            source.values["subBlend"] = 0.06f;
            source.values["noiseBlend"] = 0.0f;
            source.values["volume"] = 0.78f;
        }
        else
        {
            auto& source = addBlock (graph, "source", "source", "sample", "Sample Source");
            source.targetId = "volume";
            source.values["volume"] = 0.9f;
            source.values["pan"] = 0.5f;
            source.values["sampleStart"] = 0.0f;
            source.values["sampleLength"] = 1.0f;
        }

        auto& shape = addBlock (graph, "shape", "shape", "filter", "Tone Shape");
        shape.targetId = "filterCutoff";
        shape.values["filterCutoff"] = engineId == "fx" ? 4200.0f : 2400.0f;
        shape.values["filterResonance"] = 0.08f;
        shape.values["attack"] = engineId == "sample" ? 0.002f : 0.01f;
        shape.values["decay"] = engineId == "sample" ? 0.12f : 0.35f;
        shape.values["sustain"] = engineId == "sample" ? 0.0f : 0.55f;
        shape.values["release"] = engineId == "sample" ? 0.1f : 0.55f;

        auto& fx = addBlock (graph, "fx", "fx", "fxChain", "Factory FX");
        fx.targetId = "delayMix";
        fx.values["drive"] = engineId == "fx" ? 0.02f : 0.0f;
        fx.values["mix"] = engineId == "fx" ? 0.55f : 1.0f;
        fx.values["delayTime"] = 0.125f;
        fx.values["delayFeedback"] = engineId == "fx" ? 0.16f : 0.0f;
        fx.values["delayMix"] = engineId == "fx" ? 0.08f : 0.0f;
        fx.values["reverbMix"] = 0.04f;

        auto& out = addBlock (graph, "main_output", "out", "limiter", "Main Output");
        out.targetId = "volume";
        out.values["outputLimiter"] = 1.0f;
        out.values["outputCeilingDb"] = -1.0f;
        out.values["outputGainDb"] = -3.0f;
        // Keep edges empty: buildAudioEdges() synthesizes the serial route.

        setCommonQuickEdits (graph, engineId);
        graph.quickEditControls["out"] = { "volume", "pan", "stereoWidth",
                                           "outputGainDb", "outputLimiter", "outputCeilingDb" };
    }

    void SoundStack::resetExpandedGraph (DspGraph& graph, const juce::String& engineId)
    {
        graph.blocks.clear();
        graph.edges.clear();
        graph.macros.clear();
        graph.modulation.clear();
        graph.automation.clear();
        graph.quickEditControls.clear();
        graph.userConfigured = false;

        if (engineId == "fx")
        {
            auto& drive = addBlock (graph, "input_drive", "source", "drive", "Input Drive");
            drive.targetId = "drive";
            drive.values["drive"] = 0.12f;
            drive.values["mix"] = 1.0f;

            auto& filter = addBlock (graph, "filter_1", "filter", "stateVariable", "Morph Filter");
            filter.targetId = "filterCutoff";
            filter.values["cutoff"] = 0.72f;
            filter.values["resonance"] = 0.12f;

            auto& delay = addBlock (graph, "delay_1", "fx", "delay", "Tempo Delay");
            delay.targetId = "delayMix";
            delay.values["delayMix"] = 0.25f;
            delay.values["delayFeedback"] = 0.35f;
            delay.values["delayTime"] = 0.25f;
            delay.values["rate"] = 1.0f;
            delay.values["sync"] = 1.0f;
            delay.values["drive"] = 0.0f;

            auto& reverb = addBlock (graph, "reverb_1", "fx", "reverb", "Space Reverb");
            reverb.targetId = "reverbMix";
            reverb.values["reverbMix"] = 0.22f;
            reverb.values["mix"] = 1.0f;

            auto& lfo = addBlock (graph, "lfo_1", "mod", "lfo", "LFO 1");
            lfo.targetId = "filterCutoff";
            lfo.values["rate"] = 0.25f;
            lfo.values["sync"] = 1.0f;
            lfo.values["amount"] = 0.15f;

            auto& macro = addBlock (graph, "macro_motion", "mod", "macro", "Motion Macro");
            macro.targetId = "filterCutoff";
            macro.values["value"] = 0.45f;

            addAudioEdge (graph, "input_drive", "filter_1");
            addAudioEdge (graph, "filter_1", "delay_1");
            addAudioEdge (graph, "delay_1", "reverb_1");
        }
        else
        {
            if (engineId == "synth")
            {
                auto& osc1 = addBlock (graph, "osc_1", "source", "oscillator", "OSC 1");
                osc1.targetId = "volume";
                osc1.values["oscType"] = 0.25f;
                osc1.values["osc2Type"] = 0.75f;
                osc1.values["oscBlend"] = 0.18f;
                osc1.values["osc2Detune"] = 0.535f;
                osc1.values["subBlend"] = 0.0f;
                osc1.values["noiseBlend"] = 0.0f;
                osc1.values["volume"] = 0.80f;
                osc1.values["detune"] = 0.50f;

                auto& osc2 = addBlock (graph, "osc_2", "source", "oscillator", "OSC 2");
                osc2.targetId = "oscBlend";
                osc2.values["oscType"] = 0.50f;
                osc2.values["osc2Type"] = 0.25f;
                osc2.values["oscBlend"] = 0.42f;
                osc2.values["osc2Detune"] = 0.56f;
                osc2.values["subBlend"] = 0.10f;
                osc2.values["noiseBlend"] = 0.02f;
                osc2.values["volume"] = 0.65f;
                osc2.values["detune"] = 0.54f;

                auto& noise = addBlock (graph, "noise_1", "source", "noise", "Noise Texture");
                noise.targetId = "noiseBlend";
                noise.values["noiseBlend"] = 0.14f;
                noise.values["oscBlend"] = 0.0f;
                noise.values["subBlend"] = 0.0f;
            }
            else
            {
                auto& sample = addBlock (graph, "sample_1", "source", "sample", "Sample Layer");
                sample.targetId = "volume";
                sample.values["volume"] = 0.90f;
                sample.values["pan"] = 0.50f;
            }

            auto& filter = addBlock (graph, "filter_1", "filter", "stateVariable", "Morph Filter");
            filter.targetId = "filterCutoff";
            filter.values["cutoff"] = 0.56f;
            filter.values["resonance"] = 0.18f;

            auto& amp = addBlock (graph, "amp_env", "amp", "adsr", "Amp Envelope");
            amp.targetId = "attack";
            amp.values["attack"] = 0.01f;
            amp.values["decay"] = 0.20f;
            amp.values["sustain"] = 0.80f;
            amp.values["release"] = 0.40f;

            auto& lfo = addBlock (graph, "lfo_1", "mod", "lfo", "LFO 1");
            lfo.targetId = "filterCutoff";
            lfo.values["rate"] = 0.25f;
            lfo.values["sync"] = 1.0f;
            lfo.values["amount"] = 0.15f;

            auto& macro = addBlock (graph, "macro_motion", "mod", "macro", "Motion Macro");
            macro.targetId = "filterCutoff";
            macro.values["value"] = 0.45f;

            auto& delay = addBlock (graph, "delay_1", "fx", "delay", "Tempo Delay");
            delay.targetId = "delayMix";
            delay.values["delayMix"] = 0.18f;
            delay.values["delayFeedback"] = 0.35f;
            delay.values["delayTime"] = 0.25f;
            delay.values["rate"] = 1.0f;
            delay.values["sync"] = 1.0f;

            auto& reverb = addBlock (graph, "reverb_1", "fx", "reverb", "Space Reverb");
            reverb.targetId = "reverbMix";
            reverb.values["reverbMix"] = 0.20f;

            if (engineId == "synth")
            {
                addAudioEdge (graph, "osc_1", "filter_1", 0.577f);
                addAudioEdge (graph, "osc_2", "filter_1", 0.577f);
                addAudioEdge (graph, "noise_1", "filter_1", 0.577f);
            }
            else
            {
                addAudioEdge (graph, "sample_1", "filter_1");
            }
            addAudioEdge (graph, "filter_1", "amp_env");
            addAudioEdge (graph, "amp_env", "delay_1");
            addAudioEdge (graph, "delay_1", "reverb_1");
        }

        auto& utility = addBlock (graph, "output_utility", "out", "utility", "Output Utility");
        utility.targetId = "outputGainDb";
        utility.values["inputTrimDb"] = 0.0f;
        utility.values["phaseInvert"] = 0.0f;
        utility.values["stereoWidth"] = 1.0f;
        utility.values["monoMaker"] = 0.0f;
        utility.values["outputGainDb"] = 0.0f;
        utility.values["outputLimiter"] = 1.0f;
        utility.values["outputCeilingDb"] = -0.5f;
        addAudioEdge (graph, "reverb_1", "output_utility");

        MacroAssignment macroCutoff;
        macroCutoff.id = "macro_motion_cutoff";
        macroCutoff.macroId = "macro_motion";
        macroCutoff.targetId = "filterCutoff";
        macroCutoff.sourceMin = 0.0f;
        macroCutoff.sourceMax = 1.0f;
        macroCutoff.targetMin = 400.0f;
        macroCutoff.targetMax = 8000.0f;
        macroCutoff.curve = 1.6f;
        graph.macros.push_back (macroCutoff);

        if (engineId == "synth")
        {
            MacroAssignment macroLfo;
            macroLfo.id = "macro_motion_lfo";
            macroLfo.macroId = "macro_motion";
            macroLfo.targetId = "lfoAmount";
            macroLfo.targetMax = 0.75f;
            graph.macros.push_back (macroLfo);

            MacroAssignment macroBlend;
            macroBlend.id = "macro_motion_blend";
            macroBlend.macroId = "macro_motion";
            macroBlend.targetId = "oscBlend";
            macroBlend.targetMax = 0.65f;
            macroBlend.curve = 1.2f;
            graph.macros.push_back (macroBlend);
        }

        ModRoute lfoCutoff;
        lfoCutoff.id = "lfo_cutoff";
        lfoCutoff.sourceId = "lfo_1";
        lfoCutoff.targetId = "filterCutoff";
        lfoCutoff.amount = 0.25f;
        lfoCutoff.smoothing = 0.02f;
        graph.modulation.push_back (lfoCutoff);

        if (engineId == "synth")
        {
            ModRoute lfoBlend;
            lfoBlend.id = "lfo_source_blend";
            lfoBlend.sourceId = "lfo_1";
            lfoBlend.targetId = "oscBlend";
            lfoBlend.amount = 0.18f;
            lfoBlend.smoothing = 0.02f;
            graph.modulation.push_back (lfoBlend);
        }

        AutomationLane lane;
        lane.id = "auto_motion";
        lane.targetId = "macro_motion";
        lane.points = { 0.0f, 0.25f, 1.0f, 0.4f, 0.0f };
        graph.automation.push_back (lane);

        graph.quickEditControls["source"] = engineId == "synth"
            ? juce::StringArray { "oscType", "osc2Type", "oscBlend", "osc2Detune", "wtPosition", "wtFramePosition", "wtFrameCount", "wtLevel", "detune", "volume" }
            : juce::StringArray { "volume", "pan" };
        graph.quickEditControls["filter"] = { "filterCutoff", "filterResonance" };
        graph.quickEditControls["amp"] = { "attack", "decay", "sustain", "release", "volume" };
        graph.quickEditControls["mod"] = { "lfoRate", "lfoAmount", "vibratoRate", "vibratoDepth" };
        graph.quickEditControls["fx"] = { "drive", "mix", "delayTime", "delayFeedback", "delayMix", "reverbMix",
                                          "dynMix", "chorusMix", "phaserMix", "combMix", "resonatorMix", "spectralMix",
                                          "tapeMix", "vinylMix", "lofiMix", "vocalMix", "multiTapMix" };
        graph.quickEditControls["out"] = { "volume", "pan", "projectBpm", "inputTrimDb", "stereoWidth", "monoMaker",
                                           "outputGainDb", "outputLimiter", "outputCeilingDb", "bpmSync", "retrigger" };
    }

    bool SoundStack::isMotionBlock (const DspBlock& block)
    {
        if (! block.enabled)
            return false;

        const auto type = block.type.trim().toLowerCase();
        if (ArpeggiatorRuntime::isArpBlock (block))
            return true;

        return type.contains ("drummachine")
            || type.contains ("midiplayground")
            || type.contains ("harmonycomposer")
            || type.contains ("stepsequencer")
            || block.section.equalsIgnoreCase ("motion")
            || block.values.count ("mpStep0On") != 0
            || block.values.count ("dmTracks") != 0;
    }

    bool SoundStack::hasMotionBlock (const DspGraph& graph)
    {
        for (const auto& block : graph.blocks)
            if (isMotionBlock (block))
                return true;
        return false;
    }

    bool SoundStack::usesAdvancedGraphFeatures (const DspGraph& graph)
    {
        if (! graph.edges.empty() || ! graph.macros.empty()
            || ! graph.modulation.empty() || ! graph.automation.empty())
            return true;

        static const juce::StringArray coreIds { "source", "shape", "fx" };
        for (const auto& block : graph.blocks)
        {
            if (coreIds.contains (block.id))
                continue;
            if (isMotionBlock (block))
                continue;
            if (block.section == "out" || block.type.containsIgnoreCase ("utility"))
                continue;
            return true;
        }

        return false;
    }

    static int defaultDrumTrackNote (int track)
    {
        static constexpr int notes[] = {
            36, 38, 42, 46, 39, 43, 47, 41,
            45, 49, 51, 53, 55, 57, 59, 61
        };
        return track >= 0 && track < 16 ? notes[track] : 36 + track;
    }

    static juce::String defaultDrumTrackLabel (int track)
    {
        static const char* labels[] = {
            "Kick", "Snare", "Rim", "Clap", "Tom", "Tom", "Hat", "Hat",
            "Crash", "Ride", "Perc", "Perc", "Perc", "Perc", "Perc", "Perc"
        };
        return track >= 0 && track < 16 ? labels[track] : "Track";
    }

    static bool motionKindExists (const DspGraph& graph, SoundStack::MotionKind kind)
    {
        for (const auto& block : graph.blocks)
        {
            if (! SoundStack::isMotionBlock (block))
                continue;

            const auto type = block.type.toLowerCase();
            if (kind == SoundStack::MotionKind::Arp && ArpeggiatorRuntime::isArpBlock (block))
                return true;
            if (kind == SoundStack::MotionKind::DrumMachine && type.contains ("drummachine"))
                return true;
            if (kind == SoundStack::MotionKind::CircleSequencer
                && (type.contains ("midiplayground") || block.values.count ("mpStep0On") != 0))
                return true;
        }
        return false;
    }

    bool SoundStack::addMotionBlock (DspGraph& graph, MotionKind kind, juce::String& error)
    {
        if (motionKindExists (graph, kind))
        {
            error = "This motion block is already in the Sound Stack.";
            return false;
        }

        DspBlock block;
        block.section = kind == SoundStack::MotionKind::CircleSequencer ? "motion" : "mod";
        block.enabled = true;

        if (kind == SoundStack::MotionKind::Arp)
        {
            block.id = "motion_arp";
            block.type = "arp";
            block.name = "Arpeggiator";
            block.targetId = "filterCutoff";
            block.values = {
                { "rate", 1.0f }, { "sync", 1.0f }, { "arpSteps", 8.0f }, { "arpGate", 0.55f },
                { "arpPattern", 0.0f }, { "arpOctaves", 2.0f },
                { "arpNote0", 0.0f }, { "arpNote1", 4.0f }, { "arpNote2", 7.0f }, { "arpNote3", 12.0f },
                { "arpNote4", 7.0f }, { "arpNote5", 4.0f }, { "arpNote6", 10.0f }, { "arpNote7", 14.0f }
            };
        }
        else if (kind == SoundStack::MotionKind::DrumMachine)
        {
            block.id = "motion_drums";
            block.type = "drumMachine";
            block.name = "Drum Sequencer";
            block.targetId = "filterCutoff";
            block.values["dmTracks"] = 8.0f;
            block.values["dmSteps"] = 16.0f;
            block.values["dmPattern"] = 0.0f;
            block.values["dmTransport"] = 1.0f;
            block.values["rate"] = 1.0f;
            block.values["sync"] = 1.0f;
            for (int track = 0; track < 16; ++track)
            {
                block.values["dmTrack" + juce::String (track) + "Note"] = (float) defaultDrumTrackNote (track);
                block.metadata["dmTrack" + juce::String (track) + "Label"] = defaultDrumTrackLabel (track);
            }
            DrumMachineUtil::seedFactoryPatterns (block);
        }
        else
        {
            block.id = "motion_circles";
            block.type = "midiPlayground";
            block.name = "Circle Sequencer";
            block.targetId = "filterCutoff";
            block.values["sync"] = 1.0f;
            block.values["rate"] = 1.0f;
            block.values["arpSteps"] = 16.0f;
            block.values["mpActiveBank"] = 0.0f;
            block.values["mpScaleRoot"] = 0.0f;
            block.values["mpScaleType"] = 2.0f;
            block.values["mpProbability"] = 1.0f;
            for (int step = 0; step < 8; ++step)
            {
                block.values["mpStep" + juce::String (step) + "On"] = 1.0f;
                block.values["mpVelocity" + juce::String (step)] = 0.55f;
                block.values["mpGate" + juce::String (step)] = 0.34f;
                block.values["arpNote" + juce::String (step)] = (float) (step * 3);
            }
        }

        int suffix = 2;
        while (std::any_of (graph.blocks.begin(), graph.blocks.end(),
                            [&block] (const DspBlock& existing) { return existing.id == block.id; }))
            block.id = block.id + "_" + juce::String (suffix++);

        graph.blocks.push_back (std::move (block));
        graph.userConfigured = true;
        error = {};
        return true;
    }
}
