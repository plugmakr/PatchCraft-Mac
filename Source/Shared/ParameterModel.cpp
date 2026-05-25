#include "ParameterModel.h"

#include <array>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        static ParameterDef makeDef (juce::String id, juce::String name, float mn, float mx,
                                     float def, juce::String unit, juce::String section,
                                     juce::String category, juce::String displayMode = "continuous",
                                     float step = 0.0f)
        {
            ParameterDef p;
            p.id = std::move (id);
            p.name = std::move (name);
            p.min = mn;
            p.max = mx;
            p.defaultValue = def;
            p.unit = std::move (unit);
            p.section = std::move (section);
            p.category = std::move (category);
            p.displayMode = std::move (displayMode);
            p.step = step;
            if (displayMode == "toggle" || displayMode == "stepped")
                p.type = "choice";
            return p;
        }

        static std::vector<ParameterDef> registryForEngine (const juce::String& engineId)
        {
            std::vector<ParameterDef> defs;
            auto add = [&defs] (const ParameterDef& p) { defs.push_back (p); };

            const bool fx = engineId == "fx";
            const bool synth = engineId == "synth";

            if (synth)
            {
                add (makeDef ("oscType", "Osc Type", 0.0f, 4.0f, 1.0f, "", "source", "Oscillator", "stepped", 1.0f));
                add (makeDef ("osc2Type", "OSC 2 Type", 0.0f, 4.0f, 3.0f, "", "source", "Layer Blend", "stepped", 1.0f));
                add (makeDef ("oscBlend", "OSC Blend", 0.0f, 1.0f, 0.0f, "", "source", "Layer Blend"));
                add (makeDef ("octave", "Octave", -2.0f, 2.0f, 0.0f, "", "source", "Pitch", "stepped", 1.0f));
                add (makeDef ("detune", "Detune", -100.0f, 100.0f, 0.0f, "ct", "source", "Pitch"));
                add (makeDef ("osc2Detune", "OSC 2 Detune", -100.0f, 100.0f, 7.0f, "ct", "source", "Layer Blend"));
                add (makeDef ("subBlend", "Sub Blend", 0.0f, 1.0f, 0.0f, "", "source", "Layer Blend"));
                add (makeDef ("noiseBlend", "Noise Blend", 0.0f, 1.0f, 0.0f, "", "source", "Layer Blend"));
                auto wtEnabled = makeDef ("wtEnabled", "Wavetable Enabled", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable", "toggle", 1.0f);
                wtEnabled.enableHint = "Add a Wavetable Source block or turn Wavetable Enabled on before wavetable controls affect sound.";
                add (wtEnabled);
                auto wtTable = makeDef ("wtTable", "WT Table", 0.0f, 8.0f, 0.0f, "", "source", "Wavetable", "stepped", 1.0f);
                wtTable.enabledBy = "wtEnabled";
                wtTable.enableHint = "Enable a Wavetable Source first. Tables: Analog, Glass, PWM, Formant, Razor, Organ, Aggro, Hybrid, Custom.";
                add (wtTable);
                auto wtPosition = makeDef ("wtPosition", "WT Position", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable");
                wtPosition.enabledBy = "wtEnabled";
                wtPosition.enableHint = "Enable a Wavetable Source first.";
                add (wtPosition);
                auto wtMorph = makeDef ("wtMorph", "WT Morph", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable");
                wtMorph.enabledBy = "wtEnabled";
                wtMorph.enableHint = "Enable a Wavetable Source first.";
                add (wtMorph);
                auto wtWarp = makeDef ("wtWarp", "WT Warp", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable");
                wtWarp.enabledBy = "wtEnabled";
                wtWarp.enableHint = "Enable a Wavetable Source first.";
                add (wtWarp);
                auto wtFold = makeDef ("wtFold", "WT Fold", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable");
                wtFold.enabledBy = "wtEnabled";
                wtFold.enableHint = "Enable a Wavetable Source first.";
                add (wtFold);
                auto wtUnison = makeDef ("wtUnison", "WT Unison", 1.0f, 8.0f, 1.0f, "", "source", "Wavetable", "stepped", 1.0f);
                wtUnison.enabledBy = "wtEnabled";
                wtUnison.enableHint = "Enable a Wavetable Source first.";
                add (wtUnison);
                auto wtDetune = makeDef ("wtDetune", "WT Detune", 0.0f, 80.0f, 12.0f, "ct", "source", "Wavetable");
                wtDetune.enabledBy = "wtEnabled";
                wtDetune.enableHint = "Enable a Wavetable Source first.";
                add (wtDetune);
                auto wtSpread = makeDef ("wtSpread", "WT Spread", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable");
                wtSpread.enabledBy = "wtEnabled";
                wtSpread.enableHint = "Enable a Wavetable Source first and raise WT Unison above 1.";
                add (wtSpread);
                auto wtLevel = makeDef ("wtLevel", "WT Level", 0.0f, 1.5f, 0.0f, "", "source", "Wavetable");
                wtLevel.enabledBy = "wtEnabled";
                wtLevel.enableHint = "Enable a Wavetable Source first.";
                add (wtLevel);
                auto wtBend = makeDef ("wtBend", "WT Bend", -1.0f, 1.0f, 0.0f, "", "source", "Wavetable Warp");
                wtBend.enabledBy = "wtEnabled";
                wtBend.enableHint = "Enable a Wavetable Source first. Bend phase-distorts the table like a Serum-style warp control.";
                add (wtBend);
                auto wtSyncRatio = makeDef ("wtSyncRatio", "WT Sync Ratio", 1.0f, 8.0f, 1.0f, "x", "source", "Wavetable Warp", "stepped", 1.0f);
                wtSyncRatio.enabledBy = "wtEnabled";
                wtSyncRatio.enableHint = "Enable a Wavetable Source first. Higher ratios hard-sync the wavetable cycle.";
                add (wtSyncRatio);
                auto wtSpectralTilt = makeDef ("wtSpectralTilt", "WT Spectral Tilt", -1.0f, 1.0f, 0.0f, "", "source", "Wavetable Warp");
                wtSpectralTilt.enabledBy = "wtEnabled";
                wtSpectralTilt.enableHint = "Enable a Wavetable Source first. Negative values brighten harmonics; positive values darken them.";
                add (wtSpectralTilt);
                auto wtPhaseMode = makeDef ("wtPhaseMode", "WT Phase Mode", 0.0f, 2.0f, 0.0f, "", "source", "Wavetable Warp", "stepped", 1.0f);
                wtPhaseMode.enabledBy = "wtEnabled";
                wtPhaseMode.enableHint = "Enable a Wavetable Source first. Modes: 0 reset, 1 random note phase, 2 unison spread.";
                add (wtPhaseMode);
                auto wtFramePosition = makeDef ("wtFramePosition", "WT Frame Position", 0.0f, 1.0f, 0.0f, "", "source", "Wavetable Frames");
                wtFramePosition.enabledBy = "wtEnabled";
                wtFramePosition.enableHint = "Enable a Wavetable Source first. Frame Position scans through imported/resynthesized custom wavetable frames.";
                add (wtFramePosition);
                auto wtFrameCount = makeDef ("wtFrameCount", "WT Frame Count", 1.0f, 4.0f, 1.0f, "", "source", "Wavetable Frames", "stepped", 1.0f);
                wtFrameCount.enabledBy = "wtEnabled";
                wtFrameCount.enableHint = "Import or resynthesize a multi-frame wavetable first.";
                add (wtFrameCount);
                for (int point = 0; point < 32; ++point)
                {
                    const auto id = "wtShape" + juce::String (point);
                    const float phase = (float) point / 32.0f * juce::MathConstants<float>::twoPi;
                    auto shape = makeDef (id, "WT Shape " + juce::String (point + 1), -1.0f, 1.0f,
                                          std::sin (phase), "", "source", "Wavetable Shape");
                    shape.enabledBy = "wtEnabled";
                    shape.visible = false;
                    shape.hostAutomatable = false;
                    shape.midiLearnable = false;
                    shape.modulatable = false;
                    add (shape);
                }
                for (int frame = 0; frame < 4; ++frame)
                {
                    for (int point = 0; point < 32; ++point)
                    {
                        const auto id = "wtFrame" + juce::String (frame) + "Shape" + juce::String (point);
                        const float phase = (float) point / 32.0f * juce::MathConstants<float>::twoPi;
                        auto shape = makeDef (id, "WT Frame " + juce::String (frame + 1) + " Shape " + juce::String (point + 1),
                                              -1.0f, 1.0f, std::sin (phase * (float) (frame + 1)),
                                              "", "source", "Wavetable Frames");
                        shape.enabledBy = "wtEnabled";
                        shape.visible = false;
                        shape.hostAutomatable = false;
                        shape.midiLearnable = false;
                        shape.modulatable = false;
                        add (shape);
                    }
                }
                auto lfoRate = makeDef ("lfoRate", "LFO Rate", 0.1f, 20.0f, 4.0f, "Hz", "mod", "LFO");
                lfoRate.enabledBy = "lfoAmount";
                lfoRate.enableHint = "Raise LFO Amount above zero to hear LFO rate changes.";
                add (lfoRate);
                add (makeDef ("lfoAmount", "LFO Amount", 0.0f, 1.0f, 0.0f, "", "mod", "LFO"));
                auto vibratoRate = makeDef ("vibratoRate", "Vibrato Rate", 0.1f, 12.0f, 5.0f, "Hz", "mod", "Pitch Mod");
                vibratoRate.enabledBy = "vibratoDepth";
                vibratoRate.enableHint = "Raise Vibrato Depth above zero to hear vibrato rate changes.";
                add (vibratoRate);
                add (makeDef ("vibratoDepth", "Vibrato Depth", 0.0f, 1.0f, 0.0f, "", "mod", "Pitch Mod"));
            }

            if (! fx)
            {
                if (engineId == "sample")
                {
                    auto sampleStart = makeDef ("sampleStart", "Sample Start", 0.0f, 1.0f, 0.0f, "", "source", "Sample Control");
                    sampleStart.enableHint = "Map or import samples first. MIDI Learn this parameter to scrub the playback start point.";
                    add (sampleStart);
                    auto sampleLength = makeDef ("sampleLength", "Sample Length", 0.01f, 1.0f, 1.0f, "", "source", "Sample Control");
                    sampleLength.enableHint = "Map or import samples first. Shorter values turn MIDI notes into chops/slices.";
                    add (sampleLength);
                    auto sampleSlice = makeDef ("sampleSlice", "Sample Slice", 0.0f, 63.0f, 0.0f, "", "source", "Sample Control", "stepped", 1.0f);
                    sampleSlice.enableHint = "Raise Sample Slice Count above 1, then MIDI Learn this to trigger different sample regions.";
                    add (sampleSlice);
                    auto sampleSliceCount = makeDef ("sampleSliceCount", "Slice Count", 1.0f, 64.0f, 1.0f, "", "source", "Sample Control", "stepped", 1.0f);
                    sampleSliceCount.enableHint = "Set how many equal regions each mapped sample is divided into for MIDI-driven chopping.";
                    add (sampleSliceCount);
                    auto samplePitch = makeDef ("samplePitch", "Sample Pitch", -48.0f, 48.0f, 0.0f, "st", "source", "Sample Control");
                    samplePitch.enableHint = "MIDI Learn this to bend sample playback independently from the keyboard note.";
                    add (samplePitch);
                    auto sampleReverse = makeDef ("sampleReverse", "Sample Reverse", 0.0f, 1.0f, 0.0f, "", "source", "Sample Control", "toggle", 1.0f);
                    sampleReverse.enableHint = "MIDI Learn this as a switch to reverse sample playback for the next triggered notes.";
                    add (sampleReverse);
                    auto sampleGlitch = makeDef ("sampleGlitch", "Glitch Chance", 0.0f, 1.0f, 0.0f, "", "source", "Sample Performance");
                    sampleGlitch.enableHint = "Map or import samples first. Raises the chance that incoming notes jump to a chopped slice.";
                    add (sampleGlitch);
                    auto sampleGlitchGrid = makeDef ("sampleGlitchGrid", "Glitch Grid", 2.0f, 64.0f, 16.0f, "", "source", "Sample Performance", "stepped", 1.0f);
                    sampleGlitchGrid.enableHint = "Set the slice grid used by Glitch Chance. Higher values create tighter stutters.";
                    add (sampleGlitchGrid);

                    for (int pad = 1; pad <= 16; ++pad)
                    {
                        const auto padText = juce::String (pad);
                        auto padLevel = makeDef ("pad" + padText + "Volume", "Pad " + padText + " Level",
                                                 0.0f, 2.0f, 1.0f, "", "source", "Drum Pads");
                        padLevel.enableHint = "Map samples to drum pads first. Controls the runtime level for pad " + padText + ".";
                        add (padLevel);

                        auto padPitch = makeDef ("pad" + padText + "Pitch", "Pad " + padText + " Pitch",
                                                 -24.0f, 24.0f, 0.0f, "st", "source", "Drum Pads");
                        padPitch.enableHint = "Map samples to drum pads first. Retunes pad " + padText + " without changing the sample map.";
                        add (padPitch);

                        auto padPan = makeDef ("pad" + padText + "Pan", "Pad " + padText + " Pan",
                                               -1.0f, 1.0f, 0.0f, "", "source", "Drum Pads");
                        padPan.enableHint = "Map samples to drum pads first. Offsets the stereo position for pad " + padText + ".";
                        add (padPan);
                    }

                    auto granularOn = makeDef ("granularOn", "Granular Engine", 0.0f, 1.0f, 0.0f, "", "source", "Granular", "toggle", 1.0f);
                    granularOn.enableHint = "Turn this on to use PatchCraft's grain-cloud voice engine instead of standard sample playback.";
                    add (granularOn);
                    auto granularDensity = makeDef ("granularDensity", "Grain Density", 0.5f, 220.0f, 24.0f, "g/s", "source", "Granular");
                    granularDensity.enabledBy = "granularOn";
                    granularDensity.enableHint = "Enable Granular Engine first. Higher density creates smoother clouds; lower density creates pointillistic pulses.";
                    add (granularDensity);
                    auto granularSizeMs = makeDef ("granularSizeMs", "Grain Size", 2.0f, 1000.0f, 90.0f, "ms", "source", "Granular");
                    granularSizeMs.enabledBy = "granularOn";
                    granularSizeMs.enableHint = "Enable Granular Engine first. Short grains sound stuttery; long grains become pads and smears.";
                    add (granularSizeMs);
                    auto granularSizeRandom = makeDef ("granularSizeRandom", "Size Chaos", 0.0f, 1.0f, 0.25f, "", "source", "Granular");
                    granularSizeRandom.enabledBy = "granularOn";
                    granularSizeRandom.enableHint = "Enable Granular Engine first. Randomizes each grain length for organic movement.";
                    add (granularSizeRandom);
                    auto granularSpread = makeDef ("granularSpread", "Position Spray", 0.0f, 1.0f, 0.18f, "", "source", "Granular");
                    granularSpread.enabledBy = "granularOn";
                    granularSpread.enableHint = "Enable Granular Engine first. Sprays grains around Sample Start for wider textures.";
                    add (granularSpread);
                    auto granularScan = makeDef ("granularScan", "Scan Motion", -3.0f, 3.0f, 0.0f, "x", "source", "Granular");
                    granularScan.enabledBy = "granularOn";
                    granularScan.enableHint = "Enable Granular Engine first. Moves the grain source through the sample while notes are held.";
                    add (granularScan);
                    auto granularPitchSpread = makeDef ("granularPitchSpread", "Pitch Spray", 0.0f, 36.0f, 0.0f, "st", "source", "Granular");
                    granularPitchSpread.enabledBy = "granularOn";
                    granularPitchSpread.enableHint = "Enable Granular Engine first. Randomizes grain pitch in semitones for shimmer, swarms, and chaos.";
                    add (granularPitchSpread);
                    auto granularPanSpread = makeDef ("granularPanSpread", "Pan Spray", 0.0f, 1.0f, 0.45f, "", "source", "Granular");
                    granularPanSpread.enabledBy = "granularOn";
                    granularPanSpread.enableHint = "Enable Granular Engine first. Spreads grains across stereo for wide clouds.";
                    add (granularPanSpread);
                    auto granularReverse = makeDef ("granularReverse", "Reverse Chance", 0.0f, 1.0f, 0.0f, "", "source", "Granular");
                    granularReverse.enabledBy = "granularOn";
                    granularReverse.enableHint = "Enable Granular Engine first. In Multi Direction mode, controls how often grains play backwards.";
                    add (granularReverse);
                    auto granularTexture = makeDef ("granularTexture", "Texture Chaos", 0.0f, 1.0f, 0.20f, "", "source", "Granular");
                    granularTexture.enabledBy = "granularOn";
                    granularTexture.enableHint = "Enable Granular Engine first. Adds timing, position, and spray instability for less static clouds.";
                    add (granularTexture);
                    auto granularMaxGrains = makeDef ("granularMaxGrains", "Max Grains", 1.0f, 32.0f, 16.0f, "", "source", "Granular", "stepped", 1.0f);
                    granularMaxGrains.enabledBy = "granularOn";
                    granularMaxGrains.enableHint = "Enable Granular Engine first. Limits simultaneous grains to balance density and CPU.";
                    add (granularMaxGrains);
                    auto granularDirection = makeDef ("granularDirection", "Grain Direction", 0.0f, 3.0f, 3.0f, "", "source", "Granular", "stepped", 1.0f);
                    granularDirection.enabledBy = "granularOn";
                    granularDirection.enableHint = "Enable Granular Engine first. 0 Forward, 1 Reverse, 2 Ping-Pong, 3 Multi Direction.";
                    add (granularDirection);
                    auto granularWindow = makeDef ("granularWindow", "Grain Window", 0.0f, 3.0f, 0.0f, "", "source", "Granular", "stepped", 1.0f);
                    granularWindow.enabledBy = "granularOn";
                    granularWindow.enableHint = "Enable Granular Engine first. 0 Hann, 1 Triangle, 2 Blackman, 3 Smooth Plateau.";
                    add (granularWindow);
                    auto granularFreeze = makeDef ("granularFreeze", "Freeze Position", 0.0f, 1.0f, 0.0f, "", "source", "Granular", "toggle", 1.0f);
                    granularFreeze.enabledBy = "granularOn";
                    granularFreeze.enableHint = "Enable Granular Engine first. Freezes scan motion so grains orbit a fixed sample position.";
                    add (granularFreeze);
                }
                add (makeDef ("attack", "Attack", 0.001f, 5.0f, 0.01f, "s", "amp", "Envelope"));
                add (makeDef ("decay", "Decay", 0.001f, 5.0f, 0.20f, "s", "amp", "Envelope"));
                add (makeDef ("sustain", "Sustain", 0.0f, 1.0f, 0.80f, "", "amp", "Envelope"));
                add (makeDef ("release", "Release", 0.01f, 10.0f, engineId == "sample" ? 0.50f : 0.40f, "s", "amp", "Envelope"));
            }

            if (fx)
            {
                add (makeDef ("drive", "Drive", 0.0f, 1.0f, 0.0f, "", "source", "Input"));
                add (makeDef ("mix", "Mix", 0.0f, 1.0f, 1.0f, "", "fx", "Output"));
            }

            add (makeDef ("filterCutoff", "Cutoff", 20.0f, 20000.0f, fx ? 12000.0f : 4200.0f, "Hz", "filter", "Filter"));
            add (makeDef ("filterResonance", "Resonance", 0.0f, 1.0f, fx ? 0.10f : 0.20f, "", "filter", "Filter"));
            auto eqEnabled = makeDef ("eqEnabled", "EQ Enabled", 0.0f, 1.0f, 0.0f, "", "filter", "Surgical EQ", "toggle", 1.0f);
            eqEnabled.enableHint = "Add a Surgical EQ block or turn EQ Enabled on before EQ bands affect sound.";
            add (eqEnabled);
            auto eqMix = makeDef ("eqMix", "EQ Mix", 0.0f, 1.0f, 1.0f, "", "filter", "Surgical EQ");
            eqMix.enabledBy = "eqEnabled";
            eqMix.enableHint = "Turn EQ Enabled on or add a Surgical EQ block.";
            add (eqMix);
            auto eqTrim = makeDef ("eqOutputTrimDb", "EQ Trim", -24.0f, 24.0f, 0.0f, "dB", "filter", "Surgical EQ");
            eqTrim.enabledBy = "eqEnabled";
            eqTrim.enableHint = "Turn EQ Enabled on or add a Surgical EQ block.";
            add (eqTrim);
            const std::array<float, 8> eqDefaultFreqs { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 6000.0f, 12000.0f };
            for (int band = 1; band <= 8; ++band)
            {
                const auto prefix = "eqBand" + juce::String (band);
                auto on = makeDef (prefix + "On", "EQ " + juce::String (band) + " On", 0.0f, 1.0f, 0.0f, "",
                                   "filter", "Surgical EQ", "toggle", 1.0f);
                on.enabledBy = "eqEnabled";
                on.enableHint = "Turn EQ Enabled on first.";
                add (on);
                auto type = makeDef (prefix + "Type", "EQ " + juce::String (band) + " Type", 0.0f, 5.0f, 0.0f, "",
                                     "filter", "Surgical EQ", "stepped", 1.0f);
                type.enabledBy = prefix + "On";
                type.enableHint = "Turn this EQ band on first. Types: 0 Bell, 1 Low Shelf, 2 High Shelf, 3 High Pass, 4 Low Pass, 5 Notch.";
                add (type);
                auto mode = makeDef (prefix + "Mode", "EQ " + juce::String (band) + " Mode", 0.0f, 4.0f, 0.0f, "",
                                     "filter", "Surgical EQ", "stepped", 1.0f);
                mode.enabledBy = prefix + "On";
                mode.enableHint = "Turn this EQ band on first. Modes: 0 Stereo, 1 Left, 2 Right, 3 Mid, 4 Side.";
                add (mode);
                auto freq = makeDef (prefix + "Freq", "EQ " + juce::String (band) + " Freq", 20.0f, 20000.0f,
                                     eqDefaultFreqs[(size_t) band - 1], "Hz", "filter", "Surgical EQ");
                freq.enabledBy = prefix + "On";
                freq.enableHint = "Turn this EQ band on first.";
                add (freq);
                auto gain = makeDef (prefix + "GainDb", "EQ " + juce::String (band) + " Gain", -24.0f, 24.0f, 0.0f,
                                     "dB", "filter", "Surgical EQ");
                gain.enabledBy = prefix + "On";
                gain.enableHint = "Turn this EQ band on first. Gain affects Bell and Shelf bands.";
                add (gain);
                auto q = makeDef (prefix + "Q", "EQ " + juce::String (band) + " Q", 0.10f, 18.0f, 1.0f,
                                  "", "filter", "Surgical EQ");
                q.enabledBy = prefix + "On";
                q.enableHint = "Turn this EQ band on first. Higher Q is more surgical.";
                add (q);
                auto solo = makeDef (prefix + "Solo", "EQ " + juce::String (band) + " Solo", 0.0f, 1.0f, 0.0f,
                                     "", "filter", "Surgical EQ", "toggle", 1.0f);
                solo.enabledBy = prefix + "On";
                solo.enableHint = "Turn this EQ band on first. Solo auditions only this active EQ band while any band solo is enabled.";
                add (solo);
                auto dynMode = makeDef (prefix + "DynMode", "EQ " + juce::String (band) + " Dyn Mode", 0.0f, 3.0f, 0.0f,
                                        "", "filter", "Surgical EQ Dynamics", "stepped", 1.0f);
                dynMode.enabledBy = prefix + "On";
                dynMode.enableHint = "Turn this EQ band on first. Dynamic modes: 0 Off, 1 Duck Above Threshold, 2 Boost Above Threshold, 3 Duck Below Threshold.";
                add (dynMode);
                auto dynThreshold = makeDef (prefix + "DynThresholdDb", "EQ " + juce::String (band) + " Dyn Threshold", -80.0f, 12.0f, -24.0f,
                                             "dB", "filter", "Surgical EQ Dynamics");
                dynThreshold.enabledBy = prefix + "DynMode";
                dynThreshold.enableHint = "Set Dyn Mode above Off. Threshold decides when the EQ band moves.";
                add (dynThreshold);
                auto dynRange = makeDef (prefix + "DynRangeDb", "EQ " + juce::String (band) + " Dyn Range", -24.0f, 24.0f, 0.0f,
                                         "dB", "filter", "Surgical EQ Dynamics");
                dynRange.enabledBy = prefix + "DynMode";
                dynRange.enableHint = "Set Dyn Mode above Off. Range is the maximum dynamic EQ movement.";
                add (dynRange);
                auto dynAttack = makeDef (prefix + "DynAttackMs", "EQ " + juce::String (band) + " Dyn Attack", 0.1f, 250.0f, 10.0f,
                                          "ms", "filter", "Surgical EQ Dynamics");
                dynAttack.enabledBy = prefix + "DynMode";
                dynAttack.enableHint = "Set Dyn Mode above Off. Attack controls how quickly the dynamic EQ reacts.";
                add (dynAttack);
                auto dynRelease = makeDef (prefix + "DynReleaseMs", "EQ " + juce::String (band) + " Dyn Release", 5.0f, 1000.0f, 120.0f,
                                           "ms", "filter", "Surgical EQ Dynamics");
                dynRelease.enabledBy = prefix + "DynMode";
                dynRelease.enableHint = "Set Dyn Mode above Off. Release controls how quickly the band returns.";
                add (dynRelease);
            }
            auto delayTime = makeDef ("delayTime", "Delay Time", 0.0f, 2.0f, 0.30f, "s", "fx", "Delay");
            delayTime.enabledBy = "delayMix";
            delayTime.enableHint = "Raise Delay Mix above zero to hear delay time changes.";
            add (delayTime);
            auto delayFeedback = makeDef ("delayFeedback", "Delay Feedback", 0.0f, 0.95f, 0.40f, "", "fx", "Delay");
            delayFeedback.enabledBy = "delayMix";
            delayFeedback.enableHint = "Raise Delay Mix above zero to hear feedback changes.";
            add (delayFeedback);
            add (makeDef ("delayMix", "Delay Mix", 0.0f, 1.0f, engineId == "sample" ? 0.20f : 0.0f, "", "fx", "Delay"));
            add (makeDef ("reverbMix", "Reverb", 0.0f, 1.0f, engineId == "sample" ? 0.30f : 0.0f, "", "fx", "Reverb"));

            auto dynThreshold = makeDef ("dynThresholdDb", "Dynamics Threshold", -80.0f, 12.0f, -18.0f, "dB", "fx", "Dynamics");
            dynThreshold.enabledBy = "dynMix";
            dynThreshold.enableHint = "Raise Dynamics Mix or add a Dynamics block before threshold changes affect sound.";
            add (dynThreshold);
            auto dynRatio = makeDef ("dynRatio", "Dynamics Ratio", 1.0f, 40.0f, 2.0f, ":1", "fx", "Dynamics");
            dynRatio.enabledBy = "dynMix";
            dynRatio.enableHint = "Raise Dynamics Mix or add a Dynamics block first.";
            add (dynRatio);
            auto dynAttack = makeDef ("dynAttackMs", "Dynamics Attack", 0.1f, 250.0f, 10.0f, "ms", "fx", "Dynamics");
            dynAttack.enabledBy = "dynMix";
            dynAttack.enableHint = "Raise Dynamics Mix or add a Dynamics block first.";
            add (dynAttack);
            auto dynRelease = makeDef ("dynReleaseMs", "Dynamics Release", 5.0f, 1000.0f, 120.0f, "ms", "fx", "Dynamics");
            dynRelease.enabledBy = "dynMix";
            dynRelease.enableHint = "Raise Dynamics Mix or add a Dynamics block first.";
            add (dynRelease);
            auto dynMakeup = makeDef ("dynMakeupDb", "Dynamics Makeup", -24.0f, 24.0f, 0.0f, "dB", "fx", "Dynamics");
            dynMakeup.enabledBy = "dynMix";
            dynMakeup.enableHint = "Raise Dynamics Mix or add a Dynamics block first.";
            add (dynMakeup);
            add (makeDef ("dynMix", "Dynamics Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Dynamics"));

            auto chorusRate = makeDef ("chorusRate", "Chorus Rate", 0.01f, 20.0f, 0.35f, "Hz", "fx", "Modulation FX");
            chorusRate.enabledBy = "chorusMix";
            chorusRate.enableHint = "Raise Chorus Mix or add a Chorus block first.";
            add (chorusRate);
            auto chorusDepth = makeDef ("chorusDepth", "Chorus Depth", 0.0f, 1.0f, 0.35f, "", "fx", "Modulation FX");
            chorusDepth.enabledBy = "chorusMix";
            chorusDepth.enableHint = "Raise Chorus Mix or add a Chorus block first.";
            add (chorusDepth);
            auto chorusFeedback = makeDef ("chorusFeedback", "Chorus Feedback", -0.95f, 0.95f, 0.0f, "", "fx", "Modulation FX");
            chorusFeedback.enabledBy = "chorusMix";
            chorusFeedback.enableHint = "Raise Chorus Mix or add a Chorus block first.";
            add (chorusFeedback);
            add (makeDef ("chorusMix", "Chorus Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Modulation FX"));

            auto phaserRate = makeDef ("phaserRate", "Phaser Rate", 0.01f, 20.0f, 0.25f, "Hz", "fx", "Modulation FX");
            phaserRate.enabledBy = "phaserMix";
            phaserRate.enableHint = "Raise Phaser Mix or add a Phaser block first.";
            add (phaserRate);
            auto phaserDepth = makeDef ("phaserDepth", "Phaser Depth", 0.0f, 1.0f, 0.45f, "", "fx", "Modulation FX");
            phaserDepth.enabledBy = "phaserMix";
            phaserDepth.enableHint = "Raise Phaser Mix or add a Phaser block first.";
            add (phaserDepth);
            auto phaserFeedback = makeDef ("phaserFeedback", "Phaser Feedback", -0.95f, 0.95f, 0.0f, "", "fx", "Modulation FX");
            phaserFeedback.enabledBy = "phaserMix";
            phaserFeedback.enableHint = "Raise Phaser Mix or add a Phaser block first.";
            add (phaserFeedback);
            add (makeDef ("phaserMix", "Phaser Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Modulation FX"));

            auto combFreq = makeDef ("combFreq", "Comb Frequency", 20.0f, 8000.0f, 220.0f, "Hz", "fx", "Resonators");
            combFreq.enabledBy = "combMix";
            combFreq.enableHint = "Raise Comb Mix or add a Comb block first.";
            add (combFreq);
            auto combFeedback = makeDef ("combFeedback", "Comb Feedback", -0.95f, 0.95f, 0.35f, "", "fx", "Resonators");
            combFeedback.enabledBy = "combMix";
            combFeedback.enableHint = "Raise Comb Mix or add a Comb block first.";
            add (combFeedback);
            add (makeDef ("combMix", "Comb Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Resonators"));

            auto resonatorFreq = makeDef ("resonatorFreq", "Resonator Freq", 20.0f, 16000.0f, 440.0f, "Hz", "fx", "Resonators");
            resonatorFreq.enabledBy = "resonatorMix";
            resonatorFreq.enableHint = "Raise Resonator Mix or add a Resonator block first.";
            add (resonatorFreq);
            auto resonatorQ = makeDef ("resonatorQ", "Resonator Q", 0.05f, 18.0f, 4.0f, "", "fx", "Resonators");
            resonatorQ.enabledBy = "resonatorMix";
            resonatorQ.enableHint = "Raise Resonator Mix or add a Resonator block first.";
            add (resonatorQ);
            add (makeDef ("resonatorMix", "Resonator Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Resonators"));

            auto convolutionSize = makeDef ("convolutionSize", "Convolution Taps", 1.0f, 8.0f, 3.0f, "", "fx", "Convolution", "stepped", 1.0f);
            convolutionSize.enabledBy = "convolutionMix";
            convolutionSize.enableHint = "Raise Convolution Mix or add a Convolution block first.";
            add (convolutionSize);
            add (makeDef ("convolutionMix", "Convolution Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Convolution"));

            auto spectralTilt = makeDef ("spectralTilt", "Spectral Tilt", -1.0f, 1.0f, 0.0f, "", "fx", "Spectral");
            spectralTilt.enabledBy = "spectralMix";
            spectralTilt.enableHint = "Raise Spectral Mix or add a Spectral block first.";
            add (spectralTilt);
            add (makeDef ("spectralMix", "Spectral Mix", 0.0f, 1.0f, 0.0f, "", "fx", "Spectral"));

            auto tapeDrive = makeDef ("tapeDrive", "Tape Drive", 0.0f, 1.0f, 0.25f, "", "fx", "FX Lab / Tape");
            tapeDrive.enabledBy = "tapeMix";
            tapeDrive.enableHint = "Raise Tape Mix or add a Tape block first. Tape Drive adds analog saturation.";
            add (tapeDrive);
            auto tapeTone = makeDef ("tapeTone", "Tape Tone", 0.0f, 1.0f, 0.55f, "", "fx", "FX Lab / Tape");
            tapeTone.enabledBy = "tapeMix";
            tapeTone.enableHint = "Raise Tape Mix or add a Tape block first. Tape Tone brightens or darkens the saturation.";
            add (tapeTone);
            auto tapeFlutter = makeDef ("tapeFlutter", "Tape Flutter", 0.0f, 1.0f, 0.12f, "", "fx", "FX Lab / Tape");
            tapeFlutter.enabledBy = "tapeMix";
            tapeFlutter.enableHint = "Raise Tape Mix or add a Tape block first. Flutter adds subtle analog instability.";
            add (tapeFlutter);
            add (makeDef ("tapeMix", "Tape Mix", 0.0f, 1.0f, 0.0f, "", "fx", "FX Lab / Tape"));

            auto vinylAge = makeDef ("vinylAge", "Vinyl Age", 0.0f, 1.0f, 0.35f, "", "fx", "FX Lab / Vinyl");
            vinylAge.enabledBy = "vinylMix";
            vinylAge.enableHint = "Raise Vinyl Mix or add a Vinyl block first. Age darkens and softens the signal.";
            add (vinylAge);
            auto vinylDust = makeDef ("vinylDust", "Vinyl Dust", 0.0f, 1.0f, 0.08f, "", "fx", "FX Lab / Vinyl");
            vinylDust.enabledBy = "vinylMix";
            vinylDust.enableHint = "Raise Vinyl Mix or add a Vinyl block first. Dust adds crackle and texture.";
            add (vinylDust);
            auto vinylWarp = makeDef ("vinylWarp", "Vinyl Warp", 0.0f, 1.0f, 0.12f, "", "fx", "FX Lab / Vinyl");
            vinylWarp.enabledBy = "vinylMix";
            vinylWarp.enableHint = "Raise Vinyl Mix or add a Vinyl block first. Warp adds slow old-school movement.";
            add (vinylWarp);
            add (makeDef ("vinylMix", "Vinyl Mix", 0.0f, 1.0f, 0.0f, "", "fx", "FX Lab / Vinyl"));

            auto lofiBits = makeDef ("lofiBits", "LoFi Bits", 4.0f, 16.0f, 12.0f, "bit", "fx", "FX Lab / LoFi", "stepped", 1.0f);
            lofiBits.enabledBy = "lofiMix";
            lofiBits.enableHint = "Raise LoFi Mix or add a LoFi block first. Lower bits sound more crushed.";
            add (lofiBits);
            auto lofiRate = makeDef ("lofiRate", "LoFi Rate Crush", 0.0f, 1.0f, 0.20f, "", "fx", "FX Lab / LoFi");
            lofiRate.enabledBy = "lofiMix";
            lofiRate.enableHint = "Raise LoFi Mix or add a LoFi block first. Higher values reduce the effective sample rate.";
            add (lofiRate);
            add (makeDef ("lofiMix", "LoFi Mix", 0.0f, 1.0f, 0.0f, "", "fx", "FX Lab / LoFi"));

            auto vocalFormant = makeDef ("vocalFormant", "Vocal Formant", 0.0f, 1.0f, 0.40f, "", "fx", "FX Lab / Vocal");
            vocalFormant.enabledBy = "vocalMix";
            vocalFormant.enableHint = "Raise Vocal Mix or add a Vocal Formant block first. Formant sweeps the vowel position.";
            add (vocalFormant);
            auto vocalBody = makeDef ("vocalBody", "Vocal Body", 0.0f, 1.0f, 0.35f, "", "fx", "FX Lab / Vocal");
            vocalBody.enabledBy = "vocalMix";
            vocalBody.enableHint = "Raise Vocal Mix or add a Vocal Formant block first. Body controls resonance intensity.";
            add (vocalBody);
            add (makeDef ("vocalMix", "Vocal Mix", 0.0f, 1.0f, 0.0f, "", "fx", "FX Lab / Vocal"));

            auto multiTapTime = makeDef ("multiTapTime", "MultiTap Time", 0.02f, 2.0f, 0.375f, "s", "fx", "FX Lab / Advanced Delay");
            multiTapTime.enabledBy = "multiTapMix";
            multiTapTime.enableHint = "Raise MultiTap Mix or add a MultiTap Delay block first.";
            add (multiTapTime);
            auto multiTapFeedback = makeDef ("multiTapFeedback", "MultiTap Feedback", 0.0f, 0.92f, 0.35f, "", "fx", "FX Lab / Advanced Delay");
            multiTapFeedback.enabledBy = "multiTapMix";
            multiTapFeedback.enableHint = "Raise MultiTap Mix or add a MultiTap Delay block first.";
            add (multiTapFeedback);
            auto multiTapSpread = makeDef ("multiTapSpread", "MultiTap Spread", 0.0f, 1.0f, 0.45f, "", "fx", "FX Lab / Advanced Delay");
            multiTapSpread.enabledBy = "multiTapMix";
            multiTapSpread.enableHint = "Raise MultiTap Mix or add a MultiTap Delay block first. Spread spaces the secondary taps.";
            add (multiTapSpread);
            add (makeDef ("multiTapMix", "MultiTap Mix", 0.0f, 1.0f, 0.0f, "", "fx", "FX Lab / Advanced Delay"));

            auto projectBpm = makeDef ("projectBpm", "Project BPM", 40.0f, 220.0f, 120.0f, "BPM", "out", "Transport", "continuous", 1.0f);
            projectBpm.hostAutomatable = false;
            projectBpm.modulatable = false;
            add (projectBpm);
            add (makeDef ("bpmSync", "BPM Sync", 0.0f, 1.0f, 1.0f, "", "out", "Transport", "toggle", 1.0f));
            add (makeDef ("retrigger", "Retrigger", 0.0f, 1.0f, 1.0f, "", "out", "Performance", "toggle", 1.0f));
            add (makeDef ("inputTrimDb", "Input Trim", -48.0f, 24.0f, 0.0f, "dB", "out", "Utility"));
            add (makeDef ("phaseInvert", "Phase Invert", 0.0f, 1.0f, 0.0f, "", "out", "Utility", "toggle", 1.0f));
            add (makeDef ("stereoWidth", "Stereo Width", 0.0f, 2.0f, 1.0f, "", "out", "Utility"));
            add (makeDef ("monoMaker", "Mono Maker", 0.0f, 1.0f, 0.0f, "", "out", "Utility"));
            add (makeDef ("outputGainDb", "Output Gain", -48.0f, 24.0f, 0.0f, "dB", "out", "Utility"));
            add (makeDef ("outputLimiter", "Output Limiter", 0.0f, 1.0f, 1.0f, "", "out", "Utility", "toggle", 1.0f));
            add (makeDef ("outputCeilingDb", "Output Ceiling", -24.0f, 0.0f, -0.5f, "dB", "out", "Utility"));
            add (makeDef ("volume", "Volume", 0.0f, 1.5f, fx ? 1.0f : synth ? 0.8f : 1.0f, "", "out", "Output"));
            add (makeDef ("pan", "Pan", -1.0f, 1.0f, 0.0f, "", "out", "Output"));

            add (makeDef ("arpLaneIndex", "Lane Index", 0.0f, 15.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneSteps", "Steps", 1.0f, 128.0f, 16.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneMode", "Mode", 0.0f, 1.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneTarget", "Target", 0.0f, 4.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneRootNote", "Root Note", 0.0f, 127.0f, 60.0f, "MIDI", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneSampleSlots", "Sample Slots", 1.0f, 64.0f, 1.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneDirection", "Direction", 0.0f, 3.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneRotate", "Rotate", 0.0f, 127.0f, 0.0f, "step", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneEuclideanPulses", "Pulses", 0.0f, 128.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneProbability", "Chance", 0.0f, 1.0f, 1.0f, "", "mod", "Arp Lane"));
            add (makeDef ("arpLaneRatchet", "Ratchets", 1.0f, 8.0f, 1.0f, "x", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneFillPulses", "Fill Pulses", 0.0f, 128.0f, 0.0f, "", "mod", "Arp Lane", "stepped", 1.0f));
            add (makeDef ("arpLaneFillProbability", "Fill Chance", 0.0f, 1.0f, 0.0f, "", "mod", "Arp Lane"));
            add (makeDef ("arpLaneRate", "Rate", 0.0625f, 16.0f, 1.0f, "beat", "mod", "Arp Lane"));
            add (makeDef ("arpLaneGate", "Gate", 0.05f, 1.0f, 0.58f, "", "mod", "Arp Lane"));
            add (makeDef ("arpLaneSwing", "Swing", 0.0f, 0.5f, 0.0f, "", "mod", "Arp Lane"));

            auto modWheel = makeDef ("modWheel", "Mod Wheel", 0.0f, 1.0f, 0.0f, "", "mod", "MIDI", "continuous");
            modWheel.hostAutomatable = false;
            modWheel.modulatable = false;
            defs.push_back (modWheel);
            auto expression = makeDef ("expression", "Expression", 0.0f, 1.0f, 1.0f, "", "mod", "MIDI", "continuous");
            expression.hostAutomatable = false;
            expression.midiLearnable = false;
            expression.modulatable = false;
            defs.push_back (expression);
            auto pitchWheel = makeDef ("pitchWheel", "Pitch Wheel", -1.0f, 1.0f, 0.0f, "", "mod", "MIDI", "continuous");
            pitchWheel.hostAutomatable = false;
            pitchWheel.midiLearnable = false;
            pitchWheel.modulatable = false;
            defs.push_back (pitchWheel);
            auto aftertouch = makeDef ("aftertouch", "Aftertouch", 0.0f, 1.0f, 0.0f, "", "mod", "MIDI", "continuous");
            aftertouch.hostAutomatable = false;
            aftertouch.midiLearnable = false;
            aftertouch.modulatable = false;
            defs.push_back (aftertouch);
            auto sustainPedal = makeDef ("sustainPedal", "Sustain Pedal", 0.0f, 1.0f, 0.0f, "", "mod", "MIDI", "toggle", 1.0f);
            sustainPedal.hostAutomatable = false;
            sustainPedal.midiLearnable = false;
            sustainPedal.modulatable = false;
            defs.push_back (sustainPedal);

            return defs;
        }

        static bool isMidiRuntimePerformanceParameter (const juce::String& parameterId)
        {
            return parameterId == "modWheel"
                || parameterId == "expression"
                || parameterId == "pitchWheel"
                || parameterId == "aftertouch"
                || parameterId == "sustainPedal";
        }
    }

    ParameterModel::ParameterModel()
    {
        addDefaultPalette();
    }

    void ParameterModel::addDefaultPalette()
    {
        loadSamplerPalette();
    }

    void ParameterModel::loadSamplerPalette()
    {
        params.clear();
        for (const auto& def : registryForEngine ("sample"))
            params.push_back (def);
    }

    void ParameterModel::loadSynthPalette()
    {
        params.clear();
        for (const auto& def : registryForEngine ("synth"))
            params.push_back (def);
    }

    void ParameterModel::loadEffectPalette()
    {
        params.clear();
        for (const auto& def : registryForEngine ("fx"))
            params.push_back (def);
    }

    ParameterDef* ParameterModel::find (const juce::String& id)
    {
        for (auto& p : params)
            if (p.id == id) return &p;
        return nullptr;
    }

    const ParameterDef* ParameterModel::find (const juce::String& id) const
    {
        for (auto& p : params)
            if (p.id == id) return &p;
        return nullptr;
    }

    void ParameterModel::add (const ParameterDef& p)
    {
        if (find (p.id) != nullptr) return;
        params.push_back (p);
    }

    ParameterDef& ParameterModel::addOrUpdate (const ParameterDef& p)
    {
        if (auto* existing = find (p.id))
        {
            *existing = p;
            return *existing;
        }
        params.push_back (p);
        return params.back();
    }

    bool ParameterModel::addFromRegistry (const juce::String& id, const juce::String& engineId)
    {
        ParameterDef def;
        if (! getRegistryDefinition (id, engineId, def))
            return false;
        add (def);
        return true;
    }

    void ParameterModel::ensureRegistryMetadata (const juce::String& engineId)
    {
        for (auto& param : params)
        {
            ParameterDef registry;
            if (getRegistryDefinition (param.id, engineId, registry))
            {
                const auto defaultValue = param.defaultValue;
                param = registry;
                param.defaultValue = juce::jlimit (param.min, param.max, defaultValue);
            }
            else if (param.id.startsWith ("macro_"))
            {
                param.category = "Macro";
                param.section = "mod";
                param.displayMode = "continuous";
                param.hostAutomatable = true;
                param.midiLearnable = true;
                param.modulatable = true;
            }
        }
    }

    bool ParameterModel::remove (const juce::String& id)
    {
        for (auto it = params.begin(); it != params.end(); ++it)
        {
            if (it->id == id)
            {
                params.erase (it);
                return true;
            }
        }
        return false;
    }

    juce::String ParameterModel::ValidationIssue::toString() const
    {
        auto s = severity.toUpperCase() + ": ";
        if (ownerId.isNotEmpty())
            s += ownerId + " - ";
        if (parameterId.isNotEmpty())
            s += parameterId + ": ";
        return s + message;
    }

    std::vector<ParameterModel::ValidationIssue> ParameterModel::validateReferences (
        const std::vector<LayoutElement>& layout,
        const DspGraph& graph,
        const std::vector<Preset>& presets) const
    {
        std::vector<ValidationIssue> issues;
        auto require = [&] (juce::String ownerId, juce::String parameterId, juce::String message)
        {
            if (parameterId.isEmpty())
                return;
            if (find (parameterId) == nullptr)
                issues.push_back ({ "error", std::move (ownerId), std::move (parameterId), std::move (message) });
        };
        auto isControlElement = [] (ElementType type)
        {
            return type == ElementType::Knob
                || type == ElementType::Slider
                || type == ElementType::Button
                || type == ElementType::Toggle
                || type == ElementType::Dropdown
                || type == ElementType::XYPad;
        };

        for (const auto& element : layout)
        {
            require (element.id, element.parameterId, "Layout element is assigned to a missing parameter.");
            if (element.parameterId.isNotEmpty())
            {
                if (const auto* def = find (element.parameterId))
                {
                    const bool isMidiPerformanceControl = isMidiRuntimePerformanceParameter (element.parameterId);
                    if (isControlElement (element.type) && ! def->hostAutomatable && ! isMidiPerformanceControl)
                        issues.push_back ({ "warning", element.id, element.parameterId,
                                            "Layout control is assigned to an internal parameter that will not expose host automation." });
                    if (isControlElement (element.type) && ! def->midiLearnable && ! isMidiPerformanceControl)
                        issues.push_back ({ "warning", element.id, element.parameterId,
                                            "Layout control is assigned to a parameter that will not support Player MIDI Learn." });
                }
            }
        }

        for (const auto& block : graph.blocks)
            require (block.id, block.targetId, "DSP block targets a missing parameter.");

        for (const auto& macro : graph.macros)
        {
            require (macro.id, macro.macroId, "Macro source is missing from the parameter registry.");
            require (macro.id, macro.targetId, "Macro target is missing from the parameter registry.");
        }

        for (const auto& route : graph.modulation)
        {
            if (route.sourceId.isNotEmpty() && find (route.sourceId) == nullptr)
            {
                bool sourceIsBlock = false;
                for (const auto& block : graph.blocks)
                    if (block.id == route.sourceId)
                        sourceIsBlock = true;
                if (! sourceIsBlock)
                    issues.push_back ({ "error", route.id, route.sourceId, "Modulation source is not a parameter or DSP block." });
            }
            require (route.id, route.targetId, "Modulation target is missing from the parameter registry.");
        }

        for (const auto& lane : graph.automation)
            require (lane.id, lane.targetId, "Automation lane targets a missing parameter.");

        for (const auto& preset : presets)
        {
            for (const auto& value : preset.values)
            {
                if (find (value.first) == nullptr)
                    issues.push_back ({ "warning", preset.name, value.first, "Preset contains a value for a missing parameter." });
            }
        }

        return issues;
    }

    bool ParameterModel::getRegistryDefinition (const juce::String& id,
                                                const juce::String& engineId,
                                                ParameterDef& out)
    {
        const auto engine = engineId.isNotEmpty() ? engineId : juce::String ("synth");
        for (const auto& def : registryForEngine (engine))
        {
            if (def.id == id)
            {
                out = def;
                return true;
            }
        }
        return false;
    }

    juce::StringArray ParameterModel::getRegistryIdsForEngine (const juce::String& engineId)
    {
        juce::StringArray ids;
        for (const auto& def : registryForEngine (engineId))
            ids.add (def.id);
        return ids;
    }

    std::vector<HostParameterSlot> ParameterModel::buildHostParameterSlots (int maxSlots) const
    {
        std::vector<HostParameterSlot> slots;
        int slotIndex = 0;
        for (const auto& def : params)
        {
            if (! def.hostAutomatable)
                continue;

            HostParameterSlot slot;
            slot.parameterId = def.id;
            slot.name = def.name.isNotEmpty() ? def.name : def.id;
            slot.section = def.section;
            slot.category = def.category;
            slot.unit = def.unit;
            slot.min = def.min;
            slot.max = def.max;
            slot.defaultValue = def.defaultValue;
            slot.midiLearnable = def.midiLearnable;

            if (slotIndex < maxSlots)
            {
                slot.slotIndex = slotIndex;
                slot.slotId = "p" + juce::String (slotIndex);
                ++slotIndex;
            }
            else
            {
                slot.slotIndex = -1;
                slot.overflow = true;
            }
            slots.push_back (slot);
        }
        return slots;
    }

    juce::var ParameterModel::toVar() const
    {
        juce::Array<juce::var> arr;
        for (auto& p : params) arr.add (p.toVar());

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("parameters", arr);
        return juce::var (obj);
    }

    void ParameterModel::fromVar (const juce::var& v)
    {
        params.clear();
        if (auto* o = v.getDynamicObject())
        {
            auto arr = o->getProperty ("parameters");
            if (auto* a = arr.getArray())
                for (auto& item : *a)
                    params.push_back (ParameterDef::fromVar (item));
        }
    }

} // namespace patchcraft
