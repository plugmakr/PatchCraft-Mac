#include "DspModuleRegistry.h"

namespace patchcraft
{
    namespace
    {
        static DspModuleDescriptor makeModule (juce::String typeId,
                                               juce::String displayName,
                                               DspNodeKind kind,
                                               juce::String defaultSection,
                                               std::initializer_list<const char*> aliases = {},
                                               std::initializer_list<const char*> engines = {})
        {
            DspModuleDescriptor desc;
            desc.typeId = std::move (typeId);
            desc.displayName = std::move (displayName);
            desc.kind = kind;
            desc.defaultSection = std::move (defaultSection);
            for (auto* alias : aliases)
                desc.aliases.add (alias);
            for (auto* engine : engines)
                desc.engines.add (engine);
            return desc;
        }

        static const std::vector<DspModuleDescriptor>& buildRegistry()
        {
            static const std::vector<DspModuleDescriptor> registry {
                // --- Source nodes ---
                makeModule ("oscillator", "Oscillator", DspNodeKind::source, "source",
                            { "osc", "basicOsc" }),
                makeModule ("oscStack", "OSC Stack", DspNodeKind::source, "source",
                            { "oscstack" }, { "synth" }),
                makeModule ("wavetable", "Wavetable", DspNodeKind::source, "source",
                            { "wt", "wavetableOsc" }, { "synth" }),
                makeModule ("serumWavetable", "Serum Wavetable", DspNodeKind::source, "source",
                            { "serum", "serumTable" }, { "synth" }),
                makeModule ("noise", "Noise", DspNodeKind::source, "source", {}, { "synth" }),
                makeModule ("subOsc", "Sub Oscillator", DspNodeKind::source, "source",
                            { "sub" }, { "synth" }),
                makeModule ("samplePlayer", "Sample Player", DspNodeKind::source, "source",
                            { "sample", "sampler", "multisample" }, { "sample" }),
                makeModule ("sliceChop", "Slice Chop", DspNodeKind::source, "source",
                            { "slice", "chop", "chopGrid", "loopSlicer" }, { "sample" }),
                makeModule ("scratchDeck", "Scratch Deck", DspNodeKind::source, "source",
                            { "scratch", "deck" }, { "sample" }),
                makeModule ("granularSampler", "Granular Sampler", DspNodeKind::source, "source",
                            { "granular" }, { "sample" }),
                makeModule ("drumRack", "Drum Rack", DspNodeKind::source, "source",
                            { "drum", "drumrack", "808" }, { "sample" }),
                makeModule ("liveInput", "Live Input", DspNodeKind::source, "source",
                            { "input", "externalInput", "drive" }, { "fx" }),
                makeModule ("hybridSource", "Hybrid Source", DspNodeKind::source, "source",
                            { "hybrid", "layer" }),

                // --- Processor nodes ---
                makeModule ("stateVariable", "State Variable Filter", DspNodeKind::processor, "filter",
                            { "filter", "svf", "morphFilter" }),
                makeModule ("dynamicEq", "Dynamic EQ", DspNodeKind::processor, "filter",
                            { "eq", "surgicalEq", "surgical" }),
                makeModule ("adsr", "ADSR Envelope", DspNodeKind::processor, "amp",
                            { "envelope", "ampEnvelope", "gate" }),
                makeModule ("delay", "Delay", DspNodeKind::processor, "fx",
                            { "echo" }),
                makeModule ("multiTapDelay", "MultiTap Delay", DspNodeKind::processor, "fx",
                            { "multitap", "multiTap" }),
                makeModule ("reverb", "Reverb", DspNodeKind::processor, "fx",
                            { "space", "room" }),
                makeModule ("chorus", "Chorus", DspNodeKind::processor, "fx"),
                makeModule ("phaser", "Phaser", DspNodeKind::processor, "fx"),
                makeModule ("flanger", "Flanger", DspNodeKind::processor, "fx"),
                makeModule ("comb", "Comb Filter", DspNodeKind::processor, "fx",
                            { "combFilter" }),
                makeModule ("resonator", "Resonator", DspNodeKind::processor, "fx"),
                makeModule ("vocalFormant", "Vocal Formant", DspNodeKind::processor, "fx",
                            { "vocal", "formant" }),
                makeModule ("distortion", "Distortion", DspNodeKind::processor, "fx",
                            { "dist", "drive", "waveshaper", "shape", "crush" }),
                makeModule ("dynamics", "Dynamics", DspNodeKind::processor, "fx",
                            { "compressor", "compress", "dynamic", "limiter" }),
                makeModule ("transientShaper", "Transient Shaper", DspNodeKind::processor, "fx",
                            { "transient", "deess" }),
                makeModule ("convolution", "Convolution", DspNodeKind::processor, "fx",
                            { "fir", "impulse" }),
                makeModule ("spectralTilt", "Spectral Tilt", DspNodeKind::processor, "fx",
                            { "spectral" }),
                makeModule ("tape", "Tape", DspNodeKind::processor, "fx",
                            { "lofi", "vinyl" }),
                makeModule ("vinyl", "Vinyl Texture", DspNodeKind::processor, "fx"),
                makeModule ("fxChain", "FX Chain", DspNodeKind::processor, "fx",
                            { "effect" }),
                makeModule ("amp", "Amp", DspNodeKind::processor, "amp",
                            { "amplifier" }),

                // --- Modulation nodes ---
                makeModule ("lfo", "LFO", DspNodeKind::modulation, "mod",
                            { "lowFrequencyOscillator" }),
                makeModule ("stepLfo", "Step LFO", DspNodeKind::modulation, "mod",
                            { "steplfo" }),
                makeModule ("macro", "Macro", DspNodeKind::modulation, "mod"),
                makeModule ("random", "Random", DspNodeKind::modulation, "mod",
                            { "randomLfo" }),
                makeModule ("envelopeFollower", "Envelope Follower", DspNodeKind::modulation, "mod",
                            { "follower", "peakFollower", "rmsFollower" }),
                makeModule ("transientDetector", "Transient Detector", DspNodeKind::modulation, "mod"),
                makeModule ("spectralCentroid", "Spectral Centroid", DspNodeKind::modulation, "mod"),
                makeModule ("bandEnergy", "Band Energy", DspNodeKind::modulation, "mod",
                            { "bandenergy" }),
                makeModule ("gateTrigger", "Gate Trigger", DspNodeKind::modulation, "mod",
                            { "gatetrigger" }),
                makeModule ("velocity", "Velocity", DspNodeKind::modulation, "mod",
                            { "vel" }),
                makeModule ("keyTrack", "Key Track", DspNodeKind::modulation, "mod",
                            { "keytrack" }),
                makeModule ("midiCC", "MIDI CC", DspNodeKind::modulation, "mod",
                            { "cc", "midi" }),
                makeModule ("arp", "Arpeggiator", DspNodeKind::modulation, "mod",
                            { "arpeggiator", "arpLane" }),
                makeModule ("arpStepSequencer", "ARP Step Sequencer", DspNodeKind::modulation, "mod",
                            { "stepSequencer" }),
                makeModule ("midiPlayground", "MIDI Playground", DspNodeKind::modulation, "mod",
                            { "sequencer", "step", "chordProgression", "midiLoop" }),
                makeModule ("drumSequencer", "Drum Sequencer", DspNodeKind::modulation, "mod",
                            { "drumSeq" }),
                makeModule ("drumMachine", "Drum Machine", DspNodeKind::modulation, "mod",
                            { "drummachine" }),
                makeModule ("harmonyComposer", "Harmony Composer", DspNodeKind::modulation, "mod",
                            { "harmony", "composer", "chord", "progression", "voicing", "scale" }),
                makeModule ("pianoRoll", "Piano Roll", DspNodeKind::modulation, "mod",
                            { "pianoroll" }),
                makeModule ("automation", "Automation", DspNodeKind::modulation, "mod",
                            { "auto" }),

                // --- Analysis nodes ---
                makeModule ("analyzer", "Analyzer", DspNodeKind::analysis, "fx",
                            { "spectrum", "scope" }),
                makeModule ("meter", "Meter", DspNodeKind::analysis, "out",
                            { "levelMeter" }),

                // --- Utility nodes ---
                makeModule ("utility", "Utility", DspNodeKind::utility, "fx",
                            { "gain", "trim" }),
                makeModule ("router", "Router", DspNodeKind::utility, "fx"),
                makeModule ("mixer", "Mixer", DspNodeKind::utility, "fx",
                            { "mix" }),

                // --- Output nodes ---
                makeModule ("limiter", "Limiter", DspNodeKind::output, "out",
                            { "brickwall" }),
                makeModule ("masterBus", "Master Bus", DspNodeKind::output, "out",
                            { "master", "bus" }),
                makeModule ("mainOutput", "Main Output", DspNodeKind::output, "out",
                            { "output", "stereoOut" }),
            };
            return registry;
        }

        static bool typeMatchesDescriptor (const DspModuleDescriptor& desc, const juce::String& type)
        {
            if (type.equalsIgnoreCase (desc.typeId))
                return true;
            for (const auto& alias : desc.aliases)
                if (type.equalsIgnoreCase (alias))
                    return true;
            return false;
        }

        static bool legacyTokenFallback (const TypedDspNode& node)
        {
            const auto type = node.type.toLowerCase();
            if (type.isEmpty())
                return false;

            auto contains = [&] (std::initializer_list<const char*> tokens)
            {
                for (auto* token : tokens)
                    if (type.contains (token))
                        return true;
                return false;
            };

            switch (node.kind)
            {
                case DspNodeKind::source:
                    return contains ({ "osc", "wavetable", "serum", "noise", "sub", "sample", "sampler",
                                       "slice", "chop", "scratch", "deck", "drumrack", "drum", "layer",
                                       "granular", "input", "external", "drive", "hybrid" });
                case DspNodeKind::processor:
                    return contains ({ "state", "filter", "eq", "surgical", "envelope", "adsr", "gate",
                                       "delay", "reverb", "multitap", "dist", "shape", "crush", "dynamics",
                                       "dynamic", "compress", "limiter", "transient", "deess", "chorus",
                                       "phaser", "flanger", "comb", "resonator", "vocal", "formant", "tape",
                                       "lofi", "vinyl", "convolution", "spectral", "effect", "utility",
                                       "router", "mixer", "amp", "master" });
                case DspNodeKind::modulation:
                    return contains ({ "lfo", "random", "macro", "midi", "drum", "cc", "velocity",
                                       "keytrack", "step", "sequencer", "arp", "auto", "envelopefollower",
                                       "peakfollower", "rmsfollower", "transientdetector", "spectralcentroid",
                                       "bandenergy", "gatetrigger", "harmony", "composer", "chord",
                                       "progression", "voicing", "scale", "piano" });
                case DspNodeKind::analysis:
                    return contains ({ "analyzer", "meter", "scope", "spectrum" });
                case DspNodeKind::utility:
                    return contains ({ "utility", "router", "mixer", "output", "input" });
                case DspNodeKind::output:
                    return contains ({ "output", "mainoutput", "utility", "limiter", "mixer", "drummixer",
                                       "master", "bus", "stereo" });
                default:
                    break;
            }
            return false;
        }

        static DspNodeKind classifyBySectionHeuristic (const DspBlock& block)
        {
            const auto section = block.section.toLowerCase();
            const auto type = block.type.toLowerCase();

            if (section == "out")
                return DspNodeKind::output;
            if (type.contains ("analyzer") || type.contains ("meter") || type.contains ("scope"))
                return DspNodeKind::analysis;
            if (section == "source")
                return DspNodeKind::source;
            if (section == "filter" || section == "amp" || section == "fx")
                return DspNodeKind::processor;
            if (section == "mod")
                return DspNodeKind::modulation;
            if (type.contains ("utility") || type.contains ("router") || type.contains ("mixer"))
                return DspNodeKind::utility;
            return DspNodeKind::unknown;
        }
    }

    const std::vector<DspModuleDescriptor>& DspModuleRegistry::all()
    {
        return buildRegistry();
    }

    const DspModuleDescriptor* DspModuleRegistry::findByType (const juce::String& type)
    {
        if (type.isEmpty())
            return nullptr;

        for (const auto& desc : all())
            if (typeMatchesDescriptor (desc, type))
                return &desc;
        return nullptr;
    }

    bool DspModuleRegistry::isBlockSupported (const TypedDspNode& node)
    {
        if (node.type.isEmpty())
            return false;

        if (const auto* desc = findByType (node.type))
        {
            // Exact registry match, or compatible cross-kind (e.g. utility type
            // used as the output node in the "out" section).
            if (desc->kind == node.kind)
                return true;

            if (node.kind == DspNodeKind::output
                && (desc->kind == DspNodeKind::utility || desc->kind == DspNodeKind::processor))
                return true;
        }

        return legacyTokenFallback (node);
    }

    DspNodeKind DspModuleRegistry::classifyBlockKind (const DspBlock& block)
    {
        // Author section defines graph topology; use it first so e.g. an Output
        // Utility in the "out" section stays an output node, not a utility node.
        const auto sectionKind = classifyBySectionHeuristic (block);
        if (sectionKind != DspNodeKind::unknown)
            return sectionKind;

        if (const auto* desc = findByType (block.type))
            return desc->kind;
        return DspNodeKind::unknown;
    }

    juce::StringArray DspModuleRegistry::allTypeIdsForKind (DspNodeKind kind)
    {
        juce::StringArray result;
        for (const auto& desc : all())
            if (desc.kind == kind)
                result.add (desc.typeId);
        return result;
    }

} // namespace patchcraft
