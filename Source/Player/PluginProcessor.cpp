#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EngineFactory.h"
#include "InstrumentTemplates.h"
#include "LibraryScanner.h"
#include "../Shared/MultiInstrumentEngine.h"
#include "ArpLaneUi.h"
#include "DrumMachineUtil.h"
#include "ParameterModel.h"

#include <algorithm>
#include <algorithm>
#include <cmath>

namespace patchcraft
{
    static juce::String slotId (int index)
    {
        return "p" + juce::String (index);
    }

    static void applyPatchStateToPack (PatchCraftPack& pack, const InstrumentPatch& patch)
    {
        pack.dspGraph = patch.dspGraph;
        if (! patch.sampleZones.empty())
        {
            pack.sampleMap.clear();
            for (const auto& zone : patch.sampleZones)
                pack.sampleMap.add (zone);
        }
        if (! patch.midiMappings.empty())
            pack.midiMappings = patch.midiMappings;
    }

    static bool engineIsEffect (juce::String engineId)
    {
        engineId = engineId.toLowerCase();
        return engineId == "fx" || engineId == "effect" || engineId.contains ("fx");
    }

    static bool packEngineIsCompatibleWithThisBinary (const juce::String& engineId)
    {
       #if PATCHCRAFT_PLAYER_FX
        juce::ignoreUnused (engineId);
        return true;
       #else
        return ! engineIsEffect (engineId);
       #endif
    }

    static juce::String incompatiblePackMessage (const PatchCraftPack& pack)
    {
       #if PATCHCRAFT_PLAYER_FX
        return "This is an instrument pack (" + pack.manifest.instrumentName
            + "). Player FX can load it for inspection and MIDI-triggered playback, "
              "but it will pass the host track through instead of replacing it.";
       #else
        return "This is an FX pack (" + pack.manifest.instrumentName
            + "). Open it in Player FX, not Player.\n\n"
              "Player only loads synth, sample, drum, and multi-instrument packs.";
       #endif
    }

    static juce::String floatMapToJson (const std::map<juce::String, float>& values)
    {
        auto* root = new juce::DynamicObject();
        for (const auto& value : values)
            root->setProperty (value.first, value.second);
        return juce::JSON::toString (juce::var (root), false);
    }

    static std::map<juce::String, float> floatMapFromJson (const juce::String& json)
    {
        std::map<juce::String, float> values;
        if (json.trim().isEmpty())
            return values;

        auto parsed = juce::JSON::parse (json);
        if (auto* root = parsed.getDynamicObject())
        {
            const auto& properties = root->getProperties();
            for (int i = 0; i < properties.size(); ++i)
                values[properties.getName (i).toString()] = (float) properties.getValueAt (i);
        }
        return values;
    }

    static DspBlock* findMidiPlaygroundBlock (DspGraph& graph)
    {
        for (auto& block : graph.blocks)
            if (block.type.containsIgnoreCase ("arp")
                || block.type.containsIgnoreCase ("midi")
                || block.values.find ("arpSteps") != block.values.end())
                return &block;
        return nullptr;
    }

    static juce::String arpBankPrefix (int lane)
    {
        return "mpBank" + juce::String (juce::jlimit (0, 15, lane) + 1) + "_";
    }

    static void setArpLaneValue (DspBlock& block, int lane, const juce::String& key, float newValue)
    {
        block.values[arpBankPrefix (lane) + key] = newValue;
        if (lane == juce::jlimit (0, 15, juce::roundToInt (block.values.count ("mpActiveBank") != 0 ? block.values["mpActiveBank"] : 0.0f)))
            block.values[key] = newValue;
    }

    static void setArpLaneMetadata (DspBlock& block, int lane, const juce::String& key, const juce::String& newValue)
    {
        block.metadata["arpLane" + juce::String (lane + 1) + key] = newValue;
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

    static bool updateGraphBlockParameterValue (DspGraph& graph,
                                                const ParameterModel& parameters,
                                                const juce::String& parameterId,
                                                float value)
    {
        bool changed = false;
        auto graphValue = value;
        if (parameterId == "oscType" || parameterId == "osc2Type"
            || parameterId == "oscBlend" || parameterId == "octave" || parameterId == "detune"
            || parameterId == "osc2Detune" || parameterId == "subBlend" || parameterId == "noiseBlend"
            || parameterId == "volume" || parameterId == "pan" || parameterId == "filterCutoff"
            || parameterId == "filterResonance" || parameterId == "attack" || parameterId == "decay"
            || parameterId == "sustain" || parameterId == "release" || parameterId == "delayTime"
            || parameterId == "delayFeedback" || parameterId == "delayMix" || parameterId == "reverbMix"
            || parameterId == "drive" || parameterId == "mix")
        {
            if (const auto* def = parameters.find (parameterId))
                if (std::abs (def->max - def->min) > 0.000001f)
                    graphValue = juce::jlimit (0.0f, 1.0f, (value - def->min) / (def->max - def->min));
        }

        for (auto& block : graph.blocks)
        {
            if (auto found = block.values.find (parameterId); found != block.values.end())
            {
                found->second = graphValue;
                changed = true;
            }

            if (parameterId == "filterCutoff"
                && (block.section.equalsIgnoreCase ("filter")
                    || block.section.equalsIgnoreCase ("shape")
                    || block.section.equalsIgnoreCase ("tone")))
            {
                if (auto alias = block.values.find ("cutoff"); alias != block.values.end())
                {
                    alias->second = graphValue;
                    changed = true;
                }
            }
            else if (parameterId == "filterResonance"
                     && (block.section.equalsIgnoreCase ("filter")
                         || block.section.equalsIgnoreCase ("shape")
                         || block.section.equalsIgnoreCase ("tone")))
            {
                if (auto alias = block.values.find ("resonance"); alias != block.values.end())
                {
                    alias->second = graphValue;
                    changed = true;
                }
            }
        }
        return changed;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout PlayerProcessor::createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        // Generic float slots; the pack maps its parameter ids to slots at load time.
        for (int i = 0; i < kPatchCraftHostParameterSlots; ++i)
        {
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { slotId (i), 1 },
                juce::String ("Instrument Control ") + juce::String (i + 1),
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
                0.0f));
        }
        return { params.begin(), params.end() };
    }

    // Resolve the folder containing an embedded .patchcraft pack, if this
    // binary was produced by the Studio's VST Export module. The exporter
    // drops the pack into one of these locations:
    //
    //   - Inside a VST3 bundle:  <Bundle>.vst3/Contents/Resources/EmbeddedPack/
    //   - Next to a standalone:  <exe dir>/EmbeddedPack/
    //
    // Returns juce::File() when no embedded pack is present (vanilla Player).
    static juce::File findEmbeddedPackFolder()
    {
        const auto looksLikePack = [] (const juce::File& f)
        {
            return f.isDirectory()
                && (f.getChildFile ("manifest.json").existsAsFile()
                    || f.getChildFile ("pack.json").existsAsFile());
        };

        const auto self = juce::File::getSpecialLocation (juce::File::currentApplicationFile);

        // Walk up a few levels - works for both VST3 bundles
        // (Bundle.vst3/Contents/<arch>/Bundle.vst3) and standalone exes.
        auto node = self.isDirectory() ? self : self.getParentDirectory();
        for (int depth = 0; depth < 5 && node.exists(); ++depth)
        {
            const auto candidates = {
                node.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("EmbeddedPack"),
                node.getChildFile ("Resources").getChildFile ("EmbeddedPack"),
                node.getChildFile ("EmbeddedPack")
            };
            for (const auto& c : candidates)
                if (looksLikePack (c))
                    return c;
            node = node.getParentDirectory();
        }
        return {};
    }

    PlayerProcessor::PlayerProcessor (bool skipEmbeddedPackAutoload)
        : juce::AudioProcessor (
           #if PATCHCRAFT_PLAYER_FX
            BusesProperties()
                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Main",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Aux 1", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 2", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 3", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 4", juce::AudioChannelSet::stereo(), false)
           #else
            BusesProperties()
                .withOutput ("Main",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Aux 1", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 2", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 3", juce::AudioChannelSet::stereo(), false)
                .withOutput ("Aux 4", juce::AudioChannelSet::stereo(), false)
           #endif
            ),
          apvts (*this, nullptr, "Player", createLayout())
    {
        try
        {
            bypassLicenseEnforcement = skipEmbeddedPackAutoload;
            scriptEngine.bindStore (&liveValues);

            for (auto& level : noteHighlightLevels)
                level.store (0.0f);

            for (int i = 0; i < kPatchCraftHostParameterSlots; ++i)
            {
                ParamSlot s;
                s.id    = slotId (i);
                s.value = apvts.getRawParameterValue (s.id);
                paramSlots.push_back (s);
            }

            // Initialize library scanner
            libraryScanner = std::make_unique<LibraryScanner>();
            libraryScanner->scanLibrary();

           #if PATCHCRAFT_PLAYER_FX
            engine = createEngine (EngineType::Effect);
            pack   = buildDemoPack ("fx");
           #else
            // Synth-backed default: makes audible sound the moment the
            // plugin loads in any DAW. No WAV samples required.
            engine = createEngine (EngineType::Synth);
            pack   = buildDemoPack ("synth");
           #endif
            loaded = true;          // the demo pack is always there
            authoredSampleMap = pack.sampleMap;
            bindRoutingFromPack();
            rebuildApvtsFromPack();
            refreshLicenseStatusFromCache();

            // If the Studio's VST Export module bundled a pack with this
            // plugin, load it now so the host opens straight into the
            // packaged instrument instead of the demo.
            if (! skipEmbeddedPackAutoload)
            {
                if (const auto embedded = findEmbeddedPackFolder(); embedded != juce::File())
                {
                    juce::String embeddedError;
                    loadPack (embedded, embeddedError);
                }
            }
        }
        catch (...) { jassertfalse; engine.reset(); }
    }

    PlayerProcessor::~PlayerProcessor() = default;

    void PlayerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        try
        {
            currentSampleRate = juce::jmax (8000.0, sampleRate);
            currentBlockSize  = juce::jmax (16, samplesPerBlock);
            currentNumChans   = juce::jmax (1, getTotalNumOutputChannels());
            const auto context = makeRenderContext (currentBlockSize);
            const juce::SpinLock::ScopedLockType lk (engineLock);
            if (engine)
            {
                engine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
                engine->setRenderContext (context);
            }
            userSampleOverlay.prepare (currentSampleRate, currentBlockSize, currentNumChans);
            userSampleOverlay.setRenderContext (context);
            routingEngine.prepare (context);
        }
        catch (...) { jassertfalse; }
    }

    void PlayerProcessor::releaseResources()
    {
        try
        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            if (engine)
            {
                arpeggiator.allNotesOff (*engine);
                polySequencer.allNotesOff (*engine);
                engine->reset();
            }
            userSampleOverlay.reset();
        }
        catch (...) { jassertfalse; }
    }

    bool PlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::stereo()
            && out != juce::AudioChannelSet::mono())
            return false;
        for (const auto& outputBus : layouts.outputBuses)
        {
            if (! outputBus.isDisabled()
                && outputBus != juce::AudioChannelSet::mono()
                && outputBus != juce::AudioChannelSet::stereo())
                return false;
        }
       #if PATCHCRAFT_PLAYER_FX
        // FX supports practical mono/stereo use: mono->mono, mono->stereo, stereo->stereo.
        const auto in = layouts.getMainInputChannelSet();
        const bool validInput = in == juce::AudioChannelSet::mono()
                             || in == juce::AudioChannelSet::stereo();
        const bool outputCanCarryInput = out.size() >= in.size();
        return validInput && outputCanCarryInput;
       #else
        return true;
       #endif
    }

    void PlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals nd;
        bool restoreFxDryOnFailure = false;

        // Note: try/catch here is paranoid - the audio thread shouldn't throw,
        // but FL's plugin scanner has been known to call processBlock during
        // scan with weird buffer sizes. Cheaper than a host crash.
        try
        {
            const juce::SpinLock::ScopedTryLockType lk (engineLock);
            if (! lk.isLocked() || engine == nullptr)
            {
               #if PATCHCRAFT_PLAYER_FX
                return;
               #else
                buffer.clear();
                return;
               #endif
            }

            if (! licenseAuthorized.load())
            {
               #if PATCHCRAFT_PLAYER_FX
                return;
               #else
                buffer.clear();
                return;
               #endif
            }

            const int inputChannels = getTotalNumInputChannels();
            const int outputChannels = getTotalNumOutputChannels();
            const int mainOutputChannels = juce::jmax (1, getChannelCountOfBus (false, 0));
            auto context = makeRenderContext (buffer.getNumSamples());
            if (! context.isPlaying && internalTransportPlaying.load())
            {
                context.isPlaying = true;
                context.ppqPosition = internalTransportPpq.load();
                context.timeInSeconds = context.ppqPosition * 60.0 / RenderContext::sanitiseBpm (context.bpm);
                auto nextPpq = context.ppqPosition + context.beatsPerBlock();
                if (nextPpq >= 16384.0)
                    nextPpq = std::fmod (nextPpq, 16384.0);
                internalTransportPpq.store (nextPpq);
            }

            // Source/synth engines start with a clean buffer; FX engines read
            // input audio from the buffer and normalize mono input to stereo output.
            if (! engine->needsAudioInput())
            {
               #if PATCHCRAFT_PLAYER_FX
                // Player FX may be used on a mixer insert to preview or inspect
                // instrument packs. In that case the instrument engine does not
                // consume audio input, but the plugin must still pass the host
                // track through instead of muting the insert.
                if (inputChannels > 0)
                {
                    fxDryPassthroughBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(),
                                                    false, false, true);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        if (ch < mainOutputChannels && ch < outputChannels)
                        {
                            const int sourceChannel = inputChannels == 1 ? 0 : ch % inputChannels;
                            fxDryPassthroughBuffer.copyFrom (ch, 0, buffer, sourceChannel, 0, buffer.getNumSamples());
                        }
                        else
                        {
                            fxDryPassthroughBuffer.clear (ch, 0, buffer.getNumSamples());
                        }
                    }
                    restoreFxDryOnFailure = true;
                }
               #endif
                buffer.clear();
            }
            else
            {
                if (inputChannels <= 0)
                {
                    buffer.clear();
                    return;
                }

                for (int ch = inputChannels; ch < mainOutputChannels && ch < buffer.getNumChannels(); ++ch)
                {
                    const int sourceChannel = inputChannels == 1 ? 0 : ch % inputChannels;
                    buffer.copyFrom (ch, 0, buffer, sourceChannel, 0, buffer.getNumSamples());
                }

                for (int ch = mainOutputChannels; ch < buffer.getNumChannels(); ++ch)
                    buffer.clear (ch, 0, buffer.getNumSamples());
            }

            // Apply current APVTS values (slot index -> pack parameter index).
            if (loaded)
            {
                const auto& defs = pack.parameters.getAll();

                // First, sync up current values to liveValues
                for (const auto& def : defs)
                {
                    liveValues.setValue (def.id, getCurrentPackParameterValue (def.id));
                }

                for (const auto& def : defs)
                {
                    float v = def.defaultValue;
                    if (const auto runtime = runtimeParameterValues.find (def.id);
                        runtime != runtimeParameterValues.end())
                    {
                        v = runtime->second;
                    }

                    if (const auto slotIt = hostSlotByParameterId.find (def.id);
                        slotIt != hostSlotByParameterId.end()
                        && slotIt->second >= 0
                        && slotIt->second < (int) paramSlots.size())
                    {
                        const auto& slot = paramSlots[(size_t) slotIt->second];
                        const float nv = slot.value != nullptr ? slot.value->load() : 0.0f;
                        v = juce::jmap (juce::jlimit (0.0f, 1.0f, nv), 0.0f, 1.0f, def.min, def.max);
                        
                        const float oldVal = liveValues.getValue (def.id, def.defaultValue);
                        if (std::abs (v - oldVal) > 0.0001f)
                        {
                            liveValues.setValue (def.id, v);
                            std::map<juce::String, float> args;
                            args["value"] = v;
                            scriptEngine.triggerEvent ("knob moves", args, def.id);
                        }
                    }
                }

                // Copy updated values from liveValues back to engine and routing engine
                for (const auto& def : defs)
                {
                    float v = liveValues.getValue (def.id, def.defaultValue);
                    runtimeParameterValues[def.id] = v;
                    engine->setParameter (def.id, v);
                    if (userSampleOverlayEnabled)
                        userSampleOverlay.setParameter (def.id, v);
                    routingEngine.setParameterValue (def.id, v);
                    routingEngine.setFxBlockParameterValue (def.id, v);
                }

                // Layer persistent MIDI overrides on top of the static params.
                // Wheels emit events only on value changes, so without this
                // re-application the next block would erase the bend back to
                // the user's static detune.
                const float bendCents = midiPitchBendCents.load();
                if (std::abs (bendCents) > 0.0001f)
                {
                    const float baseDetune = getCurrentPackParameterValue ("detune");
                    engine->setParameter ("detune", baseDetune + bendCents);
                    if (userSampleOverlayEnabled)
                        userSampleOverlay.setParameter ("samplePitch", bendCents / 100.0f);
                    routingEngine.setParameterValue ("detune", baseDetune + bendCents);
                }
                const float modWheel = midiModWheel.load();
                if (modWheel >= 0.0f)
                {
                    // The mod wheel layers on top of the user's static LFO
                    // depth so it always brightens motion, but never reduces
                    // it below what the patch sets.
                    const float baseLfo  = getCurrentPackParameterValue ("lfoAmount");
                    const float baseVib  = getCurrentPackParameterValue ("vibratoDepth");
                    engine->setParameter ("lfoAmount",     juce::jmax (baseLfo, modWheel));
                    engine->setParameter ("vibratoDepth",  juce::jmax (baseVib, modWheel));
                    if (userSampleOverlayEnabled)
                        userSampleOverlay.setParameter ("granularTexture", modWheel);
                    routingEngine.setParameterValue ("lfoAmount",    juce::jmax (baseLfo, modWheel));
                    routingEngine.setParameterValue ("vibratoDepth", juce::jmax (baseVib, modWheel));
                }
            }

            engine->setRenderContext (context);

            // MIDI must update the same parameter model before graph routing so
            // MIDI CC, wheels, aftertouch, sustain, macros, and modulation lanes
            // affect the current audio block rather than arriving one block late.
            for (auto md : midi)
            {
                const auto m = md.getMessage();
                handleRealtimeMidi (m);
            }

            // Add UI-generated MIDI events (virtual keyboard)
            {
                const juce::SpinLock::ScopedLockType lk (uiMidiLock);
                for (auto md : uiMidiBuffer)
                {
                    const auto m = md.getMessage();
                    handleRealtimeMidi (m);
                }
                uiMidiBuffer.clear();
            }

            if (loaded)
                routingEngine.processToEngine (*engine, context);

            arpeggiator.process (*engine, context, userSampleOverlayEnabled ? &userSampleOverlay : nullptr);
            polySequencer.process (*engine, context);
            pianoRoll.process (*engine, context);
            if (composerRuntime.isEnabled())
            {
                composerMidi.clear();
                composerRuntime.process (context, composerMidi);
                sendComposerMidiToEngine (composerMidi);
            }
            engine->process (buffer, 0, buffer.getNumSamples());
            if (userSampleOverlayEnabled)
            {
                userSampleOverlay.setRenderContext (context);
                userSampleOverlay.process (buffer, 0, buffer.getNumSamples());
            }
            if (dynamic_cast<MultiInstrumentEngine*> (engine.get()) == nullptr)
                for (int ch = mainOutputChannels; ch < buffer.getNumChannels(); ++ch)
                    buffer.clear (ch, 0, buffer.getNumSamples());
           #if PATCHCRAFT_PLAYER_FX
            if (restoreFxDryOnFailure
                && fxDryPassthroughBuffer.getNumChannels() == buffer.getNumChannels()
                && fxDryPassthroughBuffer.getNumSamples() >= buffer.getNumSamples())
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addFrom (ch, 0, fxDryPassthroughBuffer, ch, 0, buffer.getNumSamples());
            }
           #endif
            routingEngine.captureAudioAnalysis (buffer, 0, buffer.getNumSamples());

            float peak = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
            const auto previous = outputPeak.load();
            outputPeak.store (juce::jmax (peak, previous * 0.82f));

            // Publish per-band analysis to the GUI for audio-reactive visuals.
            const auto& an = routingEngine.getAudioAnalysis();
            analysisRms.store (an.rms);
            analysisLow.store (an.lowEnergy);
            analysisMid.store (an.midEnergy);
            analysisHigh.store (an.highEnergy);
            analysisTransient.store (juce::jmax (an.transient, analysisTransient.load() * 0.55f));
            analysisCentroid.store (an.spectralCentroid);
        }
        catch (...)
        {
           #if PATCHCRAFT_PLAYER_FX
            if (restoreFxDryOnFailure
                && fxDryPassthroughBuffer.getNumChannels() == buffer.getNumChannels()
                && fxDryPassthroughBuffer.getNumSamples() >= buffer.getNumSamples())
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.copyFrom (ch, 0, fxDryPassthroughBuffer, ch, 0, buffer.getNumSamples());
            }
           #else
            buffer.clear();
           #endif
            jassertfalse;
        }
    }

    juce::AudioProcessorEditor* PlayerProcessor::createEditor()
    {
        return new PlayerEditor (*this);
    }

    float PlayerProcessor::getNoteHighlightLevel (int midiNote) const noexcept
    {
        if (midiNote < 0 || midiNote >= (int) noteHighlightLevels.size())
            return 0.0f;
        return juce::jlimit (0.0f, 1.0f, noteHighlightLevels[(size_t) midiNote].load());
    }

    float PlayerProcessor::getAudioReactiveSignal (const juce::String& mode) const noexcept
    {
        const auto m = mode.trim().toLowerCase();
        if (m == "lowband"  || m == "low"  || m == "bass")   return juce::jlimit (0.0f, 1.0f, getAudioLowBand());
        if (m == "midband"  || m == "mid")                   return juce::jlimit (0.0f, 1.0f, getAudioMidBand());
        if (m == "highband" || m == "high" || m == "treble") return juce::jlimit (0.0f, 1.0f, getAudioHighBand());
        if (m == "transient" || m == "attack")               return juce::jlimit (0.0f, 1.0f, getAudioTransient());
        if (m == "rms" || m == "loudness")                   return juce::jlimit (0.0f, 1.0f, getAudioRms() * 2.0f);
        if (m == "centroid" || m == "brightness")            return juce::jlimit (0.0f, 1.0f, getAudioCentroid());
        // Default: smoothed output peak ("level"/"peak").
        return juce::jlimit (0.0f, 1.0f, getOutputPeak());
    }

    bool PlayerProcessor::decayNoteHighlightLevels() noexcept
    {
        bool anyVisible = false;
        for (auto& level : noteHighlightLevels)
        {
            const float current = level.load();
            if (current <= 0.01f)
            {
                if (current > 0.0f)
                    level.store (0.0f);
                continue;
            }

            level.store (current * 0.82f);
            anyVisible = true;
        }
        return anyVisible;
    }

    void PlayerProcessor::setInternalTransportPlaying (bool shouldPlay)
    {
        const bool wasPlaying = internalTransportPlaying.load();
        internalTransportPlaying.store (shouldPlay);
        if (! shouldPlay)
        {
            internalTransportPpq.store (0.0);
            if (wasPlaying)
                handleNoteOff (60);
        }
        else if (! wasPlaying)
        {
            handleNoteOn (60, 0.78f);
        }
    }

    void PlayerProcessor::toggleInternalTransport()
    {
        setInternalTransportPlaying (! internalTransportPlaying.load());
    }

    bool PlayerProcessor::isAnyTransportPlaying() const
    {
        const auto context = makeRenderContext (currentBlockSize);
        return context.isPlaying || internalTransportPlaying.load();
    }

    double PlayerProcessor::getSequencerPlaybackPosition01 (int stepsPerCycle) const
    {
        const auto context = makeRenderContext (currentBlockSize);
        const bool hostPlaying = context.isPlaying;
        const bool internalPlaying = internalTransportPlaying.load();
        if (! hostPlaying && ! internalPlaying)
        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            return arpeggiator.getPlaybackPosition01 (stepsPerCycle);
        }

        const double ppq = hostPlaying ? context.ppqPosition : internalTransportPpq.load();
        const double cycles = ppq / 4.0;
        const double phase = cycles - std::floor (cycles);
        return juce::jlimit (0.0, 0.999999, phase);
    }

    void PlayerProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        try
        {
            auto state = apvts.copyState();
            state.setProperty ("packPath", loadedPath.getFullPathName(), nullptr);
            state.setProperty ("userMidiMappingsJson", midiMappingsToJson(), nullptr);
            state.setProperty ("defaultPresetValuesJson", floatMapToJson (defaultPresetValues), nullptr);
            state.setProperty ("userSnapshotsJson", userSnapshotsToJson(), nullptr);
            state.setProperty ("multiLayerRackJson", multiLayerRackToJson(), nullptr);
            state.setProperty ("userContentJson", userContentToJson(), nullptr);
            state.setProperty ("pianoRollNotes", runtimePianoRollNotes, nullptr);
            if (auto xml = std::unique_ptr<juce::XmlElement> (state.createXml()))
                copyXmlToBinary (*xml, dest);
        }
        catch (...) { jassertfalse; }
    }

    void PlayerProcessor::setStateInformation (const void* data, int size)
    {
        // FL Studio's plugin scanner is notoriously strict - any exception
        // here is logged as "crashed while loading its settings". Each step
        // is null/range-checked and the whole body is wrapped.
        if (data == nullptr || size <= 0) return;

        try
        {
            std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
            if (xml == nullptr) return;

            auto state = juce::ValueTree::fromXml (*xml);
            if (! state.isValid()) return;

            // Only accept state produced by THIS plugin (root id matches).
            if (! state.hasType (apvts.state.getType())) return;

            apvts.replaceState (state);

            const auto userMidiMappingsJson = state.getProperty ("userMidiMappingsJson").toString();
            const auto defaultPresetValuesJson = state.getProperty ("defaultPresetValuesJson").toString();
            const auto userSnapshotsJson = state.getProperty ("userSnapshotsJson").toString();
            const auto multiLayerRackJson = state.getProperty ("multiLayerRackJson").toString();
            const auto userContentJson = state.getProperty ("userContentJson").toString();
            const auto path = state.getProperty ("packPath").toString();
            if (path.isEmpty() || ! juce::File::isAbsolutePath (path)) return;

            const juce::File folder (path);
            if (! folder.isDirectory()) return;     // pack moved or deleted

            // Synchronous load - simpler and avoids any cross-thread issue
            // with the host's plugin scan environment. loadPack itself is
            // wrapped in try/catch.
            juce::String error;
            loadPack (folder, error);
            userContentFromJson (userContentJson);
            userSnapshotsFromJson (userSnapshotsJson);
            midiMappingsFromJson (userMidiMappingsJson);
            defaultPresetValues = floatMapFromJson (defaultPresetValuesJson);
            multiLayerRackFromJson (multiLayerRackJson);

            const auto pianoRollNotes = state.getProperty ("pianoRollNotes").toString();
            if (pianoRollNotes.isNotEmpty())
            {
                const juce::SpinLock::ScopedLockType lk (engineLock);
                runtimePianoRollNotes = pianoRollNotes;
                applyPianoRollOverrideLocked();
                pianoRoll.bind (pack.dspGraph);
            }
        }
        catch (...) { jassertfalse; }
    }

    void PlayerProcessor::handleNoteOn (int midiNote)
    {
        handleNoteOn (midiNote, 0.8f);
    }

    void PlayerProcessor::handleNoteOn (int midiNote, float velocity)
    {
        const juce::SpinLock::ScopedLockType lk (uiMidiLock);
        uiMidiBuffer.addEvent (juce::MidiMessage::noteOn (1, midiNote,
                                                          juce::jlimit (0.01f, 1.0f, velocity)), 0);
    }

    void PlayerProcessor::handleNoteOff (int midiNote)
    {
        const juce::SpinLock::ScopedLockType lk (uiMidiLock);
        uiMidiBuffer.addEvent (juce::MidiMessage::noteOff (1, midiNote, 0.0f), 0);
    }

    void PlayerProcessor::beginMidiLearn (juce::String parameterId)
    {
        if (! isParameterMidiLearnable (parameterId))
            return;

        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        pendingMidiLearnParameter = std::move (parameterId);
    }

    void PlayerProcessor::clearMidiLearn()
    {
        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        pendingMidiLearnParameter.clear();
    }

    void PlayerProcessor::removeMidiMappingForParameter (const juce::String& parameterId)
    {
        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        userMidiMappings.erase (std::remove_if (userMidiMappings.begin(), userMidiMappings.end(),
            [&] (const MidiMapping& mapping) { return mapping.parameterId == parameterId; }),
            userMidiMappings.end());
    }

    juce::String PlayerProcessor::getMidiMappingSummary (const juce::String& parameterId) const
    {
        auto describeMapping = [] (const MidiMapping& mapping, const juce::String& prefix) -> juce::String
        {
            const auto channel = mapping.channel > 0 ? juce::String (mapping.channel) : juce::String ("Any");
            if (mapping.sourceType == "cc")
                return prefix + "CC " + juce::String (mapping.controller) + " / Ch " + channel;
            if (mapping.sourceType == "pitchWheel")
                return prefix + "Pitch Wheel / Ch " + channel;
            if (mapping.sourceType == "aftertouch")
                return prefix + "Aftertouch / Ch " + channel;
            if (mapping.sourceType == "channelPressure")
                return prefix + "Channel Pressure / Ch " + channel;
            return prefix + mapping.sourceType + " / Ch " + channel;
        };

        {
            const juce::SpinLock::ScopedLockType lk (midiMappingLock);
            for (const auto& mapping : userMidiMappings)
            {
                if (mapping.parameterId == parameterId)
                    return describeMapping (mapping, {});
            }
        }

        for (const auto& mapping : pack.midiMappings)
        {
            if (mapping.parameterId == parameterId)
                return describeMapping (mapping, "Pack: ");
        }
        return {};
    }

    juce::String PlayerProcessor::getPendingMidiLearnParameter() const
    {
        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        return pendingMidiLearnParameter;
    }

    bool PlayerProcessor::isParameterMidiLearnable (const juce::String& parameterId) const
    {
        if (! pack.manifest.playerAllowMidiLearn)
            return false;
        for (const auto& def : pack.parameters.getAll())
            if (def.id == parameterId)
                return def.midiLearnable;
        return false;
    }

    float PlayerProcessor::getPackParameterValue (const juce::String& parameterId) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        return getCurrentPackParameterValue (parameterId);
    }

    int PlayerProcessor::getActiveVoiceCount() const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || engine == nullptr)
            return 0;
        return engine->getActiveVoiceCount();
    }

    int PlayerProcessor::getLoadedSampleCount() const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || engine == nullptr)
            return 0;
        return engine->getLoadedSampleCount();
    }

    juce::String PlayerProcessor::getEngineDiagnosticStatus() const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || engine == nullptr)
            return "Engine busy";
        return engine->getDiagnosticStatus();
    }

    int PlayerProcessor::getHostParameterSlotIndex (const juce::String& parameterId) const
    {
        if (parameterId.isEmpty())
            return -1;
        const auto it = hostSlotByParameterId.find (parameterId);
        return it != hostSlotByParameterId.end() ? it->second : -1;
    }

    void PlayerProcessor::computeUserWaveformPeaks (const juce::File& file, int numBuckets)
    {
        userWaveformPeaks.clear();
        if (! file.existsAsFile())
            return;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return;

        numBuckets = juce::jlimit (32, 1024, numBuckets);
        const auto totalSamples = reader->lengthInSamples;
        const int channels = juce::jmax (1, (int) reader->numChannels);
        // Read in chunks so large files stay light on memory.
        const int chunk = 1 << 15;
        juce::AudioBuffer<float> buffer (channels, chunk);
        std::vector<float> peaks ((size_t) numBuckets, 0.0f);
        juce::int64 pos = 0;
        while (pos < totalSamples)
        {
            const int toRead = (int) juce::jmin ((juce::int64) chunk, totalSamples - pos);
            if (! reader->read (&buffer, 0, toRead, pos, true, channels > 1))
                break;
            for (int i = 0; i < toRead; ++i)
            {
                float mag = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    mag = juce::jmax (mag, std::abs (buffer.getSample (ch, i)));
                const auto bucket = (size_t) juce::jlimit (0, numBuckets - 1,
                    (int) (((pos + i) * (juce::int64) numBuckets) / totalSamples));
                peaks[bucket] = juce::jmax (peaks[bucket], mag);
            }
            pos += toRead;
        }

        float maxPeak = 0.0f;
        for (auto v : peaks) maxPeak = juce::jmax (maxPeak, v);
        if (maxPeak > 0.0001f)
            for (auto& v : peaks) v = juce::jlimit (0.0f, 1.0f, v / maxPeak);

        userWaveformPeaks = std::move (peaks);
    }

    bool PlayerProcessor::isSupportedUserSampleFile (const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return file.existsAsFile()
            && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg");
    }

    bool PlayerProcessor::isSupportedUserMidiFile (const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return file.existsAsFile() && (ext == ".mid" || ext == ".midi");
    }

    juce::String PlayerProcessor::safeUserContentFileName (const juce::File& file)
    {
        auto base = file.getFileNameWithoutExtension()
            .retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .#")
            .trim();
        if (base.isEmpty())
            base = "import";
        return base + file.getFileExtension().toLowerCase();
    }

    juce::File PlayerProcessor::getUserContentRoot() const
    {
        auto brand = loaded ? pack.manifest.whiteLabelPublisher.trim() : juce::String();
        if (brand.isEmpty() || brand.equalsIgnoreCase ("PatchCraft"))
            brand = loaded ? pack.manifest.creator.trim() : juce::String();
        if (brand.equalsIgnoreCase ("PatchCraft"))
            brand.clear();
        if (brand.isEmpty())
            brand = loaded && pack.manifest.playerDisplayName.isNotEmpty()
                ? pack.manifest.playerDisplayName
                : juce::String ("Player Instruments");

        auto key = loaded
            ? (pack.manifest.playerDisplayName.isNotEmpty()
                ? pack.manifest.playerDisplayName
                : pack.manifest.instrumentName)
            : juce::String ("Instrument");
        if (loaded && pack.manifest.version.isNotEmpty())
            key += "_" + pack.manifest.version;

        brand = brand.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .").trim();
        key = key.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ .").trim();
        if (brand.isEmpty()) brand = "Player Instruments";
        if (key.isEmpty()) key = "Instrument";

        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile (brand)
            .getChildFile ("Player")
            .getChildFile ("User Content")
            .getChildFile (key);
    }

    std::vector<PlayerProcessor::UserContentItem> PlayerProcessor::getUserContentSnapshot() const
    {
        const juce::ScopedLock lock (userContentLock);
        return userContent;
    }

    juce::String PlayerProcessor::userContentToJson() const
    {
        juce::Array<juce::var> items;
        {
            const juce::ScopedLock lock (userContentLock);
            for (const auto& item : userContent)
            {
                auto* object = new juce::DynamicObject();
                object->setProperty ("id", item.id);
                object->setProperty ("kind", item.kind);
                object->setProperty ("name", item.name);
                object->setProperty ("filePath", item.filePath);
                object->setProperty ("role", item.role);
                object->setProperty ("summary", item.summary);
                object->setProperty ("rootNote", item.rootNote);
                object->setProperty ("lowNote", item.lowNote);
                object->setProperty ("highNote", item.highNote);
                object->setProperty ("padIndex", item.padIndex);
                object->setProperty ("noteCount", item.noteCount);
                object->setProperty ("bpm", item.bpm);
                object->setProperty ("tuneSemitones", item.tuneSemitones);
                object->setProperty ("midiMode", item.midiMode);
                object->setProperty ("midiVelocityAmount", (double) item.midiVelocityAmount);
                object->setProperty ("midiStack", item.midiStack);
                items.add (juce::var (object));
            }
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("version", 1);
        root->setProperty ("items", items);
        return juce::JSON::toString (juce::var (root), false);
    }

    void PlayerProcessor::saveUserContentManifest() const
    {
        const auto root = getUserContentRoot();
        root.createDirectory();
        root.getChildFile ("user-imports.json").replaceWithText (userContentToJson());
    }

    void PlayerProcessor::userContentFromJson (const juce::String& json)
    {
        std::vector<UserContentItem> restored;
        const auto parsed = juce::JSON::parse (json);
        if (auto* root = parsed.getDynamicObject())
        {
            const auto* array = root->getProperty ("items").getArray();
            if (array != nullptr)
            {
                for (const auto& value : *array)
                {
                    if (auto* object = value.getDynamicObject())
                    {
                        UserContentItem item;
                        item.id = object->getProperty ("id").toString();
                        item.kind = object->getProperty ("kind").toString();
                        item.name = object->getProperty ("name").toString();
                        item.filePath = object->getProperty ("filePath").toString();
                        item.role = object->getProperty ("role").toString();
                        item.summary = object->getProperty ("summary").toString();
                        item.rootNote = juce::jlimit (0, 127, (int) object->getProperty ("rootNote"));
                        item.lowNote = juce::jlimit (0, 127, (int) object->getProperty ("lowNote"));
                        item.highNote = juce::jlimit (0, 127, (int) object->getProperty ("highNote"));
                        item.padIndex = (int) object->getProperty ("padIndex");
                        item.noteCount = juce::jmax (0, (int) object->getProperty ("noteCount"));
                        item.bpm = (double) object->getProperty ("bpm");
                        item.tuneSemitones = (int) object->getProperty ("tuneSemitones");
                        if (object->hasProperty ("midiMode"))
                            item.midiMode = object->getProperty ("midiMode").toString();
                        if (object->hasProperty ("midiVelocityAmount"))
                            item.midiVelocityAmount = juce::jlimit (0.0f, 1.0f,
                                (float) (double) object->getProperty ("midiVelocityAmount"));
                        if (object->hasProperty ("midiStack"))
                            item.midiStack = (bool) object->getProperty ("midiStack");
                        if (item.id.isNotEmpty() && item.kind.isNotEmpty() && juce::File (item.filePath).existsAsFile())
                            restored.push_back (std::move (item));
                    }
                }
            }
        }

        {
            const juce::ScopedLock lock (userContentLock);
            userContent = std::move (restored);
        }

        {
            const juce::SpinLock::ScopedLockType engineGuard (engineLock);
            rebuildRuntimeUserContentLocked (true);
        }

        editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
    }

    bool PlayerProcessor::applyMidiContentToGraphLocked (const UserContentItem& item)
    {
        juce::FileInputStream input (juce::File (item.filePath));
        if (! input.openedOk())
            return false;

        juce::MidiFile midiFile;
        if (! midiFile.readFrom (input) || midiFile.getNumTracks() <= 0)
            return false;

        const bool targetZoneMidi = item.role.equalsIgnoreCase ("zoneMidi")
                                 && item.rootNote >= 0
                                 && item.rootNote <= 127;

        if (! targetZoneMidi
            && (item.role.isEmpty() || item.role.equalsIgnoreCase ("playground")))
        {
            for (auto& block : pack.dspGraph.blocks)
            {
                if (! PianoRollRuntime::isPianoRollBlock (block))
                    continue;

                const int steps = juce::jlimit (1, 256, juce::roundToInt (
                    block.values.count ("prSteps") != 0 ? block.values.at ("prSteps") : 16.0f));
                const int defaultLength = juce::jmax (1, juce::roundToInt (
                    block.values.count ("prStepsPerBeat") != 0 ? block.values.at ("prStepsPerBeat") : 4.0f));
                auto imported = PianoRollRuntime::notesFromMidiFile (midiFile, steps, defaultLength);
                if (imported.empty())
                    return false;

                std::vector<PianoRollRuntime::Note> merged = imported;
                if (item.midiStack)
                {
                    const auto existingEncoded = runtimePianoRollNotes.isNotEmpty()
                        ? runtimePianoRollNotes
                        : (block.metadata.count ("notes") != 0 ? block.metadata.at ("notes") : juce::String());
                    const auto existing = PianoRollRuntime::decodeNotes (existingEncoded);
                    merged.insert (merged.begin(), existing.begin(), existing.end());
                }

                const auto encoded = PianoRollRuntime::encodeNotes (merged);
                block.metadata["notes"] = encoded;
                runtimePianoRollNotes = encoded;

                for (auto& candidate : pack.dspGraph.blocks)
                    if (candidate.id == "user_midi_playground")
                        candidate.enabled = false;

                pianoRoll.bind (pack.dspGraph);
                pack.dspGraph.userConfigured = true;
                bindRoutingFromPack();
                return true;
            }
        }

        struct NoteEvent { double time = 0.0; int note = 60; float velocity = 1.0f; };
        std::vector<NoteEvent> notes;
        for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
        {
            const auto* track = midiFile.getTrack (trackIndex);
            if (track == nullptr)
                continue;

            for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
            {
                const auto message = track->getEventPointer (eventIndex)->message;
                if (message.isNoteOn())
                    notes.push_back ({ message.getTimeStamp(),
                                       juce::jlimit (0, 127, message.getNoteNumber()),
                                       message.getFloatVelocity() });
            }
        }

        if (notes.empty())
            return false;

        std::stable_sort (notes.begin(), notes.end(),
            [] (const NoteEvent& a, const NoteEvent& b) { return a.time < b.time; });

        if (targetZoneMidi)
        {
            auto safeBlockSuffix = item.id.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
            if (safeBlockSuffix.isEmpty())
                safeBlockSuffix = juce::String (item.rootNote);
            const auto targetBlockId = item.id.startsWithIgnoreCase ("authored_zone_midi_")
                ? item.id
                : "user_midi_zone_player_" + safeBlockSuffix;
            DspBlock* block = nullptr;
            for (auto& candidate : pack.dspGraph.blocks)
            {
                if (candidate.id == targetBlockId)
                {
                    block = &candidate;
                    break;
                }
            }

            if (block == nullptr)
            {
                DspBlock created;
                created.id = targetBlockId;
                created.section = "mod";
                created.type = "drumMachine";
                created.name = "Runtime Sample MIDI Player";
                created.targetId = "sample";
                created.enabled = true;
                pack.dspGraph.blocks.push_back (std::move (created));
                block = &pack.dspGraph.blocks.back();
            }

            static constexpr int kImportPattern = 0;
            static constexpr int kImportSteps = 16;
            static constexpr int kMaxTracks = 8;
            const auto mode = (item.midiMode.isNotEmpty() ? item.midiMode : juce::String ("trigger")).trim().toLowerCase();
            const bool pitchMode = mode == "pitch";
            const bool sliceMode = mode == "slice";
            std::vector<int> trackNotes;
            if (pitchMode || sliceMode)
            {
                for (const auto& note : notes)
                {
                    const int mapped = juce::jlimit (0, 127, note.note + item.tuneSemitones);
                    if (std::find (trackNotes.begin(), trackNotes.end(), mapped) == trackNotes.end())
                    {
                        trackNotes.push_back (mapped);
                        if ((int) trackNotes.size() >= kMaxTracks)
                            break;
                    }
                }
            }
            if (trackNotes.empty())
                trackNotes.push_back (juce::jlimit (0, 127, item.rootNote + item.tuneSemitones));

            block->enabled = true;
            block->type = "drumMachine";
            block->name = item.name.isNotEmpty() ? "Sample MIDI: " + item.name : "Runtime Sample MIDI Player";
            block->targetId = "sample";
            block->values["rate"] = 1.0f;
            block->values["sync"] = 1.0f;
            block->values["dmTracks"] = (float) trackNotes.size();
            block->values["dmSteps"] = (float) kImportSteps;
            block->values["dmPattern"] = (float) kImportPattern;
            block->values["dmTransport"] = 1.0f;
            block->values["dmTriggerPadSlots"] = 0.0f;
            block->values["dmSongMode"] = 0.0f;
            block->values["dmSwing"] = 0.0f;
            block->values["dmProbability"] = 1.0f;
            if (sliceMode)
            {
                block->values["sampleSliceCount"] = (float) juce::jlimit (1, kMaxTracks, (int) trackNotes.size());
                block->values["sampleStart"] = 0.0f;
                block->values["sampleLength"] = 1.0f;
                block->values["samplePitch"] = 0.0f;
            }
            block->metadata["midiMode"] = mode;
            block->metadata["midiSource"] = item.filePath;

            for (int track = 0; track < kMaxTracks; ++track)
            {
                const int mappedNote = track < (int) trackNotes.size()
                    ? trackNotes[(size_t) track]
                    : juce::jlimit (0, 127, item.rootNote + item.tuneSemitones);
                const int playbackNote = sliceMode
                    ? juce::jlimit (0, 127, item.rootNote + item.tuneSemitones)
                    : mappedNote;
                block->values["dmTrack" + juce::String (track) + "Note"] = (float) playbackNote;
                for (int step = 0; step < kImportSteps; ++step)
                {
                    const auto prefix = "dmP" + juce::String (kImportPattern)
                                      + "T" + juce::String (track)
                                      + "S" + juce::String (step);
                    block->values[prefix + "On"] = 0.0f;
                    block->values[prefix + "Vel"] = 0.78f;
                    block->values[prefix + "Gate"] = 0.34f;
                    block->values[prefix + "Prob"] = 1.0f;
                    block->values[prefix + "Div"] = 1.0f;
                    block->values[prefix + "SampleSlice"] = -1.0f;
                }
            }

            const double firstTime = notes.front().time;
            const double lastTime = juce::jmax (firstTime + 1.0, notes.back().time);
            const double duration = juce::jmax (1.0, lastTime - firstTime);
            for (const auto& note : notes)
            {
                const int eventNote = (pitchMode || sliceMode)
                    ? juce::jlimit (0, 127, note.note + item.tuneSemitones)
                    : trackNotes.front();
                auto trackIt = std::find (trackNotes.begin(), trackNotes.end(), eventNote);
                if (trackIt == trackNotes.end())
                    continue;
                const int track = (int) std::distance (trackNotes.begin(), trackIt);
                const int step = juce::jlimit (0, kImportSteps - 1,
                    juce::roundToInt (((note.time - firstTime) / duration) * (double) (kImportSteps - 1)));
                const auto prefix = "dmP" + juce::String (kImportPattern)
                                  + "T" + juce::String (track)
                                  + "S" + juce::String (step);
                block->values[prefix + "On"] = 1.0f;
                block->values[prefix + "Vel"] = juce::jlimit (0.05f, 1.0f,
                    note.velocity * juce::jlimit (0.0f, 1.0f, item.midiVelocityAmount));
                block->values[prefix + "Gate"] = 0.34f;
                block->values[prefix + "Prob"] = 1.0f;
                if (sliceMode)
                {
                    block->values[prefix + "SampleSlice"] = (float) track;
                }
            }

            pack.dspGraph.userConfigured = true;
            bindRoutingFromPack();
            return true;
        }

        DspBlock* drumBlock = nullptr;
        for (auto& candidate : pack.dspGraph.blocks)
        {
            if (candidate.type.containsIgnoreCase ("drum")
                || candidate.values.find ("dmTracks") != candidate.values.end())
            {
                drumBlock = &candidate;
                break;
            }
        }

        if (drumBlock != nullptr)
        {
            static constexpr int kImportPattern = 0;
            static constexpr int kImportSteps = 16;
            static constexpr int kMaxTracks = 8;

            std::vector<int> trackNotes;
            trackNotes.reserve (kMaxTracks);
            for (const auto& note : notes)
            {
                if (std::find (trackNotes.begin(), trackNotes.end(), note.note) == trackNotes.end())
                {
                    trackNotes.push_back (note.note);
                    if ((int) trackNotes.size() >= kMaxTracks)
                        break;
                }
            }

            if (trackNotes.empty())
                return false;

            drumBlock->enabled = true;
            drumBlock->type = "drumMachine";
            drumBlock->name = item.name.isNotEmpty() ? "Imported MIDI: " + item.name : "Imported MIDI Drum Pattern";
            drumBlock->values["rate"] = 1.0f;
            drumBlock->values["sync"] = 1.0f;
            drumBlock->values["dmTracks"] = (float) trackNotes.size();
            drumBlock->values["dmSteps"] = (float) kImportSteps;
            drumBlock->values["dmPattern"] = (float) kImportPattern;
            drumBlock->values["dmTransport"] = 1.0f;
            drumBlock->values["dmTriggerPadSlots"] = 0.0f;
            drumBlock->values["dmSongMode"] = 0.0f;
            drumBlock->values["dmSwing"] = 0.08f;
            drumBlock->values["dmProbability"] = 1.0f;
            drumBlock->values["dmSeed"] = (float) (juce::Time::getCurrentTime().toMilliseconds() & 0x7fffffff);

            for (int track = 0; track < kMaxTracks; ++track)
            {
                const int mappedNote = track < (int) trackNotes.size() ? trackNotes[(size_t) track] : 36 + track;
                drumBlock->values["dmTrack" + juce::String (track) + "Note"] = (float) mappedNote;
                for (int step = 0; step < kImportSteps; ++step)
                {
                    const auto prefix = "dmP" + juce::String (kImportPattern)
                                      + "T" + juce::String (track)
                                      + "S" + juce::String (step);
                    drumBlock->values[prefix + "On"] = 0.0f;
                    drumBlock->values[prefix + "Vel"] = 0.78f;
                    drumBlock->values[prefix + "Gate"] = 0.34f;
                    drumBlock->values[prefix + "Prob"] = 1.0f;
                    drumBlock->values[prefix + "Div"] = 1.0f;
                }
            }

            const double firstTime = notes.front().time;
            const double lastTime = juce::jmax (firstTime + 1.0, notes.back().time);
            const double duration = juce::jmax (1.0, lastTime - firstTime);
            for (const auto& note : notes)
            {
                auto trackIt = std::find (trackNotes.begin(), trackNotes.end(), note.note);
                if (trackIt == trackNotes.end())
                    continue;

                const int track = (int) std::distance (trackNotes.begin(), trackIt);
                const int step = juce::jlimit (0, kImportSteps - 1,
                    juce::roundToInt (((note.time - firstTime) / duration) * (double) (kImportSteps - 1)));
                const auto prefix = "dmP" + juce::String (kImportPattern)
                                  + "T" + juce::String (track)
                                  + "S" + juce::String (step);
                drumBlock->values[prefix + "On"] = 1.0f;
                drumBlock->values[prefix + "Vel"] = juce::jlimit (0.05f, 1.0f, note.velocity);
                drumBlock->values[prefix + "Gate"] = 0.34f;
                drumBlock->values[prefix + "Prob"] = 1.0f;
                if (note.note == 42 || note.note == 44 || note.note == 46)
                    drumBlock->values[prefix + "Div"] = note.velocity > 0.88f ? 2.0f : 1.0f;
            }

            pack.dspGraph.userConfigured = true;
            bindRoutingFromPack();
            return true;
        }

        DspBlock* block = nullptr;
        for (auto& candidate : pack.dspGraph.blocks)
        {
            if (candidate.id == "user_midi_playground")
            {
                block = &candidate;
                break;
            }
        }

        if (block == nullptr)
        {
            DspBlock created;
            created.id = "user_midi_playground";
            created.section = "mod";
            created.type = "arpStepSequencer";
            created.name = "User MIDI Pattern";
            created.enabled = true;
            pack.dspGraph.blocks.push_back (std::move (created));
            block = &pack.dspGraph.blocks.back();
        }

        block->enabled = true;
        block->type = "arpStepSequencer";
        block->name = item.name.isNotEmpty() ? "User MIDI: " + item.name : "User MIDI Pattern";
        block->values["arpRate"] = 1.0f;
        block->values["arpGate"] = 0.72f;
        block->values["arpPattern"] = 0.0f;
        block->values["arpOctaves"] = 1.0f;
        block->values["mpProbability"] = 1.0f;
        block->values["mpHumanize"] = 0.02f;
        block->values["mpSwing"] = 0.0f;
        block->values["mpLatch"] = 1.0f;
        block->values["mpScaleType"] = 0.0f;
        block->values["mpSeed"] = (float) (juce::Time::getCurrentTime().toMilliseconds() & 0x7fffffff);

        const int steps = juce::jlimit (1, 16, (int) notes.size());
        const int root = notes.front().note;
        block->values["arpSteps"] = (float) steps;
        for (int step = 0; step < 16; ++step)
        {
            const bool active = step < steps;
            block->values["mpStep" + juce::String (step) + "On"] = active ? 1.0f : 0.0f;
            block->values["arpNote" + juce::String (step)] = active
                ? (float) juce::jlimit (-36, 36, notes[(size_t) step].note - root)
                : 0.0f;
            block->values["mpVelocity" + juce::String (step)] = active
                ? juce::jlimit (0.05f, 1.0f, notes[(size_t) step].velocity)
                : 0.0f;
            block->values["mpGate" + juce::String (step)] = active ? 0.72f : 0.05f;
            block->values["mpStepProb" + juce::String (step)] = 1.0f;
        }

        pack.dspGraph.userConfigured = true;
        bindRoutingFromPack();
        return true;
    }

    void PlayerProcessor::applyAuthoredZoneMidiToGraphLocked()
    {
        pack.dspGraph.blocks.erase (std::remove_if (pack.dspGraph.blocks.begin(), pack.dspGraph.blocks.end(),
            [] (const DspBlock& block)
            {
                return block.id.startsWithIgnoreCase ("authored_zone_midi_");
            }), pack.dspGraph.blocks.end());

        const auto projectBase = loadedPath.isDirectory() ? loadedPath : juce::File();
        const auto& zones = pack.sampleMap.getZones();
        for (int zoneIndex = 0; zoneIndex < (int) zones.size(); ++zoneIndex)
        {
            const auto& zone = zones[(size_t) zoneIndex];
            if (zone.midiPath.isEmpty())
                continue;

            auto midiFile = SampleMap::resolveSamplePath (projectBase, zone.midiPath);
            if (! midiFile.existsAsFile())
                continue;

            UserContentItem item;
            item.id = "authored_zone_midi_" + juce::String (zoneIndex);
            item.kind = "midi";
            item.name = midiFile.getFileNameWithoutExtension();
            item.filePath = midiFile.getFullPathName();
            item.role = "zoneMidi";
            item.summary = "Authored " + zone.midiPlaybackMode + " MIDI drives "
                         + juce::MidiMessage::getMidiNoteName (zone.rootNote, true, true, 4);
            item.rootNote = juce::jlimit (0, 127, zone.rootNote);
            item.lowNote = item.rootNote;
            item.highNote = item.rootNote;
            item.padIndex = zone.padIndex;
            item.bpm = zone.bpm;
            item.tuneSemitones = juce::jlimit (-48, 48, zone.midiTranspose);
            item.midiMode = zone.midiPlaybackMode.isNotEmpty() ? zone.midiPlaybackMode : juce::String ("trigger");
            item.midiVelocityAmount = juce::jlimit (0.0f, 1.0f, zone.midiVelocityAmount);
            applyMidiContentToGraphLocked (item);
        }
    }

    void PlayerProcessor::rebuildRuntimeUserContentLocked (bool reloadEngine)
    {
        std::vector<UserContentItem> snapshot;
        {
            const juce::ScopedLock lock (userContentLock);
            snapshot = userContent;
        }

        pack.sampleMap = authoredSampleMap;
        SampleMap overlayMap;
        int padCounter = 0;
        for (const auto& item : snapshot)
        {
            if (item.kind != "sample")
                continue;

            const juce::File file (item.filePath);
            if (! file.existsAsFile())
                continue;

            SampleZoneDef zone;
            zone.samplePath = file.getFullPathName();
            zone.rootNote = juce::jlimit (0, 127, item.rootNote);
            zone.lowNote = juce::jlimit (0, 127, item.lowNote);
            zone.highNote = juce::jlimit (zone.lowNote, 127, item.highNote);
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            zone.oneShot = item.role != "keyboard";
            zone.padIndex = item.role == "pads" ? (item.padIndex >= 0 ? item.padIndex : padCounter) : -1;
            zone.padLabel = item.name;
            zone.group = "__user_import__";
            zone.bpm = (float) item.bpm;
            zone.pitchOffset = (float) item.tuneSemitones;
            overlayMap.add (zone);
            pack.sampleMap.add (zone);
            ++padCounter;
        }

        userSampleOverlayEnabled = ! overlayMap.getZones().empty();
        userSampleOverlay.loadFromPack (getUserContentRoot(), overlayMap);

        if (reloadEngine && engine != nullptr)
        {
            engine->allNotesOff();
            userSampleOverlay.allNotesOff();
            // The main engine only ever plays the pack's authored zones. User-dropped
            // samples are played exclusively by userSampleOverlay, so we must NOT feed the
            // merged map back into the main engine or every dropped sample would trigger
            // twice (once per engine).
            if (loadedPath.isDirectory())
                engine->loadFromPack (loadedPath, authoredSampleMap);
        }

        applyAuthoredZoneMidiToGraphLocked();

        for (const auto& item : snapshot)
            if (item.kind == "midi" && (item.role == "playground" || item.role == "zoneMidi"))
                applyMidiContentToGraphLocked (item);

        applyPianoRollOverrideLocked();
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        pianoRoll.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
    }

    bool PlayerProcessor::importUserContentFiles (const juce::StringArray& files,
                                                  const juce::String& sampleMappingMode,
                                                  juce::String& report,
                                                  int targetNote,
                                                  int targetPadIndex,
                                                  bool midiStack)
    {
        targetNote = juce::jlimit (-1, 127, targetNote);
        targetPadIndex = juce::jlimit (-1, 63, targetPadIndex);
        const auto root = getUserContentRoot();
        const auto sampleDir = root.getChildFile ("Samples");
        const auto midiDir = root.getChildFile ("MIDI");
        sampleDir.createDirectory();
        midiDir.createDirectory();

        std::vector<UserContentItem> imported;
        int importedSamples = 0;
        int importedMidi = 0;
        int skippedUnsupported = 0;
        int failedCopies = 0;
        const bool keyboardMode = sampleMappingMode.equalsIgnoreCase ("keyboard");
        const bool hasTargetNote = targetNote >= 0;
        const bool hasTargetPad = targetPadIndex >= 0;
        int nextPadIndex = 0;
        {
            const juce::ScopedLock lock (userContentLock);
            for (const auto& item : userContent)
            {
                if (item.kind == "sample" && item.role == "pads")
                    nextPadIndex = juce::jmax (nextPadIndex, item.padIndex + 1);
            }
        }

        for (const auto& path : files)
        {
            const juce::File source (path);
            if (! source.existsAsFile())
            {
                ++skippedUnsupported;
                continue;
            }

            const bool isSample = isSupportedUserSampleFile (source);
            const bool isMidi = isSupportedUserMidiFile (source);
            if (! isSample && ! isMidi)
            {
                ++skippedUnsupported;
                continue;
            }

            const auto destinationDir = isSample ? sampleDir : midiDir;
            auto destination = destinationDir.getChildFile (safeUserContentFileName (source));
            const auto safeBase = destination.getFileNameWithoutExtension();
            const auto safeExtension = destination.getFileExtension();
            for (int suffix = 2; destination.existsAsFile(); ++suffix)
            {
                destination = destinationDir.getChildFile (
                    safeBase + "-" + juce::String (suffix) + safeExtension);
            }

            if (! source.copyFileTo (destination))
            {
                ++failedCopies;
                continue;
            }

            UserContentItem item;
            item.id = juce::Uuid().toString();
            item.kind = isSample ? "sample" : "midi";
            item.name = source.getFileNameWithoutExtension();
            item.filePath = destination.getFullPathName();
            item.role = isSample
                ? (keyboardMode ? "keyboard" : (hasTargetPad ? "pads" : (hasTargetNote ? "zone" : "pads")))
                : (hasTargetNote ? "zoneMidi" : "playground");
            item.bpm = getHostBpm();

            if (isSample)
            {
                if (hasTargetNote && ! keyboardMode)
                {
                    item.padIndex = hasTargetPad ? juce::jlimit (0, 63, targetPadIndex + importedSamples) : -1;
                    item.rootNote = hasTargetPad
                        ? juce::jlimit (0, 127, targetNote + importedSamples)
                        : targetNote;
                    item.lowNote = item.rootNote;
                    item.highNote = item.rootNote;
                    item.summary = hasTargetPad
                        ? "Pad " + juce::String (item.padIndex + 1)
                            + " / " + juce::MidiMessage::getMidiNoteName (item.rootNote, true, true, 4)
                        : "Zone " + juce::MidiMessage::getMidiNoteName (targetNote, true, true, 4);
                }
                else if (keyboardMode)
                {
                    bool usedNamePitch = false;
                    bool usedAudioPitch = false;
                    auto zone = SampleMap::inferZoneFromFileWithAudio (destination, 60, 0, 127,
                                                                        &usedNamePitch, &usedAudioPitch, nullptr);
                    item.rootNote = hasTargetNote ? targetNote : zone.rootNote;
                    item.lowNote = 0;
                    item.highNote = 127;
                    item.summary = "Keyboard map"
                        + juce::String (usedNamePitch ? " / name pitch" : (usedAudioPitch ? " / detected pitch" : ""));
                }
                else
                {
                    item.padIndex = hasTargetPad ? targetPadIndex : nextPadIndex + importedSamples;
                    item.rootNote = juce::jlimit (0, 127, 36 + item.padIndex);
                    if (hasTargetNote)
                        item.rootNote = targetNote;
                    item.lowNote = item.rootNote;
                    item.highNote = item.rootNote;
                    item.summary = "Pad " + juce::String (item.padIndex + 1)
                        + " / " + juce::MidiMessage::getMidiNoteName (item.rootNote, true, true, 4);
                }
                ++importedSamples;
            }
            else
            {
                item.rootNote = hasTargetNote ? targetNote : 60;
                item.lowNote = item.rootNote;
                item.highNote = item.rootNote;
                item.padIndex = hasTargetPad ? targetPadIndex : -1;
                item.midiMode = hasTargetPad ? "drum" : "trigger";
                item.midiStack = midiStack && ! hasTargetNote && ! hasTargetPad;
                juce::FileInputStream input (destination);
                juce::MidiFile midiFile;
                if (input.openedOk() && midiFile.readFrom (input))
                {
                    int noteCount = 0;
                    for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
                    {
                        const auto* track = midiFile.getTrack (trackIndex);
                        if (track == nullptr)
                            continue;
                        for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
                            if (track->getEventPointer (eventIndex)->message.isNoteOn())
                                ++noteCount;
                    }
                    item.noteCount = noteCount;
                    item.summary = hasTargetNote
                        ? juce::String (noteCount) + " notes / drives "
                            + juce::MidiMessage::getMidiNoteName (item.rootNote, true, true, 4)
                        : juce::String (noteCount) + " notes / "
                            + (item.midiStack ? "stacked on piano roll" : "replaces piano roll");
                }
                else
                {
                    item.summary = "MIDI file copied; parse failed";
                }
                ++importedMidi;
            }

            imported.push_back (std::move (item));
        }

        if (imported.empty())
        {
            report = "No supported files imported. Drop WAV, AIFF, FLAC, MID, or MIDI files.";
            return false;
        }

        {
            const juce::ScopedLock lock (userContentLock);
            // Dropping a new melodic sample onto a zone replaces the previous one so a
            // single-zone instrument stays one playable sample rather than stacking layers.
            const bool replaceKeyboard = keyboardMode;
            const bool importingPlaygroundMidi = ! midiStack
                && std::any_of (imported.begin(), imported.end(),
                    [] (const UserContentItem& item)
                    {
                        return item.kind == "midi" && item.role == "playground";
                    });
            if (hasTargetPad || hasTargetNote || replaceKeyboard || importingPlaygroundMidi)
            {
                userContent.erase (std::remove_if (userContent.begin(), userContent.end(),
                    [&imported, hasTargetPad, hasTargetNote, replaceKeyboard, importingPlaygroundMidi, targetPadIndex, targetNote] (const UserContentItem& existing)
                    {
                        if (existing.kind == "sample" && replaceKeyboard && existing.role == "keyboard")
                            return true;

                        if (existing.kind == "sample" && hasTargetPad && existing.role == "pads")
                            for (const auto& item : imported)
                                if (item.kind == "sample" && item.role == "pads" && existing.padIndex == item.padIndex)
                                    return true;

                        if (existing.kind == "midi"
                            && existing.role == "zoneMidi"
                            && hasTargetNote
                            && existing.rootNote == targetNote)
                            return true;

                        if (importingPlaygroundMidi
                            && existing.kind == "midi"
                            && existing.role == "playground")
                            return true;

                        return false;
                    }), userContent.end());
            }
            userContent.insert (userContent.end(), imported.begin(), imported.end());
        }

        // Cache a waveform of the most recent dropped sample for runtime display.
        for (auto it = imported.rbegin(); it != imported.rend(); ++it)
            if (it->kind == "sample")
            {
                computeUserWaveformPeaks (juce::File (it->filePath));
                break;
            }

        {
            const juce::SpinLock::ScopedLockType engineGuard (engineLock);
            rebuildRuntimeUserContentLocked (true);
        }

        saveUserContentManifest();
        report = "Imported " + juce::String (importedSamples) + " sample"
               + (importedSamples == 1 ? "" : "s")
               + " and " + juce::String (importedMidi) + " MIDI file"
               + (importedMidi == 1 ? "" : "s") + ".";
        if (importedMidi > 0)
            report += hasTargetNote ? " MIDI now drives the target sample zone." : " MIDI pattern is active.";
        if (skippedUnsupported > 0)
            report += " Skipped " + juce::String (skippedUnsupported) + " unsupported item"
                + (skippedUnsupported == 1 ? "" : "s") + ".";
        if (failedCopies > 0)
            report += " Failed to copy " + juce::String (failedCopies) + " item"
                + (failedCopies == 1 ? "" : "s") + ".";
        editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
        return true;
    }

    bool PlayerProcessor::clearUserContent()
    {
        {
            const juce::ScopedLock lock (userContentLock);
            if (userContent.empty())
                return false;
            userContent.clear();
        }

        userWaveformPeaks.clear();

        {
            const juce::SpinLock::ScopedLockType engineGuard (engineLock);
            rebuildRuntimeUserContentLocked (true);
        }

        saveUserContentManifest();
        editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
        return true;
    }

    bool PlayerProcessor::applyUserMidiToPlayground (const juce::String& contentId, bool stack)
    {
        UserContentItem target;
        {
            const juce::ScopedLock lock (userContentLock);
            for (auto& item : userContent)
            {
                if (item.id == contentId && item.kind == "midi")
                {
                    item.midiStack = stack;
                    target = item;
                    break;
                }
            }
        }

        if (target.id.isEmpty())
            return false;

        const juce::SpinLock::ScopedLockType engineGuard (engineLock);
        const bool ok = applyMidiContentToGraphLocked (target);
        if (ok)
        {
            bindRoutingFromPack();
            editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
        }
        return ok;
    }

    bool PlayerProcessor::loadPack (const juce::File& packFolder, juce::String& error)
    {
        bool ok = false;
        try
        {
            PatchCraftPackReader reader;
            PatchCraftPack np;
            if (! reader.read (packFolder, np, error)) return false;
            if (! packEngineIsCompatibleWithThisBinary (np.manifest.engine))
            {
                error = incompatiblePackMessage (np);
                return false;
            }
            if (const auto* patch = np.findDefaultPatch())
                applyPatchStateToPack (np, *patch);

            juce::File authoredWaveformFile;
            if (! np.sampleMap.getZones().empty())
            {
                const auto samplePath = np.sampleMap.getZones().front().samplePath;
                authoredWaveformFile = juce::File::isAbsolutePath (samplePath)
                    ? juce::File (samplePath)
                    : packFolder.getChildFile (samplePath);
            }

            // Build the right engine for this pack (off the audio thread).
            auto newEngine = createEngineFromManifest (np.manifest.engine);
            if (newEngine == nullptr) { error = "Unknown engine type."; return false; }

            newEngine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
            newEngine->setRenderContext (makeRenderContext (currentBlockSize));
            newEngine->loadFromPack (packFolder, np.sampleMap);

            {
                const juce::SpinLock::ScopedLockType lk (engineLock);
                engine     = std::move (newEngine);
                pack       = std::move (np);
                loaded     = true;
                loadedPath = packFolder;
                authoredSampleMap = pack.sampleMap;
                {
                    const juce::ScopedLock contentLock (userContentLock);
                    userContent.clear();
                }
                userSampleOverlayEnabled = false;
                userSampleOverlay.allNotesOff();
                heldNotes.fill (false);
                sustainPedalDown = false;
                runtimePianoRollNotes.clear();
                applyAuthoredZoneMidiToGraphLocked();
            }
            if (authoredWaveformFile.existsAsFile())
                computeUserWaveformPeaks (authoredWaveformFile);
            else
                userWaveformPeaks.clear();
            if (libraryScanner != nullptr)
            {
                juce::Array<juce::File> rootsToScan;
                auto addRoot = [&rootsToScan] (const juce::File& folder)
                {
                    if (folder.isDirectory())
                        rootsToScan.addIfNotAlreadyThere (folder);
                };

                const auto packParent = packFolder.getParentDirectory();
                const auto packGrandparent = packParent.getParentDirectory();
                addRoot (packParent);
                addRoot (packGrandparent.getChildFile ("FactoryDemos"));
                addRoot (packGrandparent.getChildFile ("Library"));
                addRoot (packGrandparent.getChildFile ("Library").getChildFile ("Instruments"));
                addRoot (packGrandparent.getChildFile ("Library").getChildFile ("Templates"));
                addRoot (packGrandparent.getChildFile ("Examples").getChildFile ("FactoryDemos"));

                for (const auto& root : rootsToScan)
                    libraryScanner->addSearchPath (root);
                libraryScanner->scanLibrary();
            }
            bindRoutingFromPack();
            rebuildApvtsFromPack();
            refreshLicenseStatusFromCache();

            {
                auto scriptFile = packFolder.getChildFile ("pscript.txt");
                if (scriptFile.existsAsFile())
                    scriptEngine.compile (scriptFile.loadFileAsString());
                else
                    scriptEngine.compile ("");

                scriptEngine.triggerEvent ("preset loads", {});
            }

            editorListeners.call ([] (EditorListener& l) { l.packChanged(); });
            ok = true;
        }
        catch (...) { jassertfalse; }
        if (! ok)
        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            loaded = false;
        }
        return ok;
    }

    void PlayerProcessor::unloadPack()
    {
        // "Unload" reverts to the bundled demo so the Player is never blank.
       #if PATCHCRAFT_PLAYER_FX
        const auto engineId = juce::String ("fx");
        auto newEngine = createEngine (EngineType::Effect);
       #else
        const auto engineId = juce::String ("synth");
        auto newEngine = createEngine (EngineType::Synth);
       #endif
        newEngine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
        newEngine->setRenderContext (makeRenderContext (currentBlockSize));

        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            engine     = std::move (newEngine);
            pack       = buildDemoPack (engineId);
            loaded     = true;
            loadedPath = juce::File();
            authoredSampleMap = pack.sampleMap;
            {
                const juce::ScopedLock contentLock (userContentLock);
                userContent.clear();
            }
            userSampleOverlayEnabled = false;
            userSampleOverlay.allNotesOff();
            heldNotes.fill (false);
            sustainPedalDown = false;
            if (engine) engine->allNotesOff();
        }
        bindRoutingFromPack();
        rebuildApvtsFromPack();
        refreshLicenseStatusFromCache();
        editorListeners.call ([] (EditorListener& l) { l.packChanged(); });
    }

    LicenseValidator::LicenseInfo PlayerProcessor::getLicenseInfo() const
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        return LicenseValidator::fromManifest (pack.manifest);
    }

    LicenseValidator::ActivationStatus PlayerProcessor::getLicenseActivationStatus() const
    {
        const juce::ScopedLock lock (licenseLock);
        return licenseStatus;
    }

    void PlayerProcessor::applyLicenseActivationStatus (const LicenseValidator::ActivationStatus& status)
    {
        const auto info = getLicenseInfo();
        juce::String reason;
        const bool usable = bypassLicenseEnforcement
                         || ! (getPack() != nullptr && getPack()->manifest.licenseRequired)
                         || LicenseValidator::isActivationUsable (info, status, reason);
        {
            const juce::ScopedLock lock (licenseLock);
            licenseStatus = status;
            if (! usable && licenseStatus.message.isEmpty())
                licenseStatus.message = reason;
        }
        licenseAuthorized.store (usable);
    }

    void PlayerProcessor::refreshLicenseStatusFromCache()
    {
        const auto info = getLicenseInfo();
        const auto* currentPack = getPack();
        if (bypassLicenseEnforcement || currentPack == nullptr || ! currentPack->manifest.licenseRequired)
        {
            LicenseValidator::ActivationStatus status;
            status.authorized = true;
            status.status = currentPack != nullptr && currentPack->manifest.licenseRequired
                ? "preview-bypass"
                : "not-required";
            status.message = currentPack != nullptr && currentPack->manifest.licenseRequired
                ? "License enforcement is bypassed inside Studio preview."
                : "This product does not require activation.";
            status.productId = info.productId;
            status.instrumentId = info.instrumentId;
            status.machineId = LicenseValidator::machineFingerprint();
            applyLicenseActivationStatus (status);
            return;
        }

        juce::String reason;
        auto cached = LicenseValidator::loadCachedActivation (info, reason);
        if (! cached.authorized && cached.message.isEmpty())
            cached.message = reason;
        applyLicenseActivationStatus (cached);
    }

    juce::String PlayerProcessor::getLicenseStatusText() const
    {
        const auto* currentPack = getPack();
        if (currentPack == nullptr || ! currentPack->manifest.licenseRequired)
            return "License not required";

        const auto status = getLicenseActivationStatus();
        if (status.authorized)
        {
            if (status.trial)
                return "Trial active";
            if (status.offlineGrace)
                return "Licensed (offline grace)";
            return status.ownerName.isNotEmpty() ? "Licensed to " + status.ownerName : "Licensed";
        }
        if (status.status == "misconfigured")
            return "License setup error";
        if (status.status == "offline")
            return "Activation server unavailable";
        return "Activation required";
    }

    void PlayerProcessor::rebuildApvtsFromPack()
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);

        hostSlotByParameterId.clear();
        runtimeParameterValues.clear();

        auto hostSlots = pack.hostParameterSlots;
        if (hostSlots.empty())
            hostSlots = pack.parameters.buildHostParameterSlots();
        for (const auto& slot : hostSlots)
            if (! slot.overflow && slot.slotIndex >= 0 && slot.slotIndex < (int) paramSlots.size())
                hostSlotByParameterId[slot.parameterId] = slot.slotIndex;

        // Initial APVTS values come from the default preset (or the first
        // preset, or the parameter definition fallback). This is what the
        // user expects to hear when they drop the plugin onto a track.
        const Preset* startPreset = pack.findDefaultPreset();

        for (const auto& slot : paramSlots)
            if (slot.value != nullptr)
                slot.value->store (0.0f);

        for (const auto& def : pack.parameters.getAll())
        {
            float v = def.defaultValue;
            if (startPreset != nullptr)
            {
                auto it = startPreset->values.find (def.id);
                if (it != startPreset->values.end()) v = it->second;
            }
            v = juce::jlimit (def.min, def.max, v);
            runtimeParameterValues[def.id] = v;

            const auto slotIt = hostSlotByParameterId.find (def.id);
            if (slotIt == hostSlotByParameterId.end())
                continue;

            const auto slotIndex = slotIt->second;
            if (slotIndex < 0 || slotIndex >= (int) paramSlots.size())
                continue;

            const float n = juce::jmap (v, def.min, def.max, 0.0f, 1.0f);
            if (paramSlots[(size_t) slotIndex].value)
                *paramSlots[(size_t) slotIndex].value = juce::jlimit (0.0f, 1.0f, n);
        }

        // Engage all parameter values in the DSP engine and routing engine
        for (const auto& kv : runtimeParameterValues)
        {
            const auto& parameterId = kv.first;
            const float val = kv.second;

            if (engine != nullptr)
                engine->setParameter (parameterId, val);
            if (userSampleOverlayEnabled)
                userSampleOverlay.setParameter (parameterId, val);
            routingEngine.setParameterValue (parameterId, val);
            updateGraphBlockParameterValue (pack.dspGraph, pack.parameters, parameterId, val);
            routingEngine.setFxBlockParameterValue (parameterId, val);
        }
    }

    static void ensureDrumMachinePatternsSeeded (DspGraph& graph)
    {
        for (auto& block : graph.blocks)
        {
            if (! block.enabled)
                continue;
            if (! block.type.containsIgnoreCase ("drum") && block.values.find ("dmTracks") == block.values.end())
                continue;
            if (block.values.find ("dmP0T0S0On") != block.values.end())
                continue;

            DrumMachineUtil::seedEmptyPatterns (block);
            graph.userConfigured = true;
            break;
        }
    }

    void PlayerProcessor::bindRoutingFromPack()
    {
        ensureDrumMachinePatternsSeeded (pack.dspGraph);

        if (engine)
        {
            arpeggiator.allNotesOff (*engine);
            pianoRoll.allNotesOff (*engine);
            if (composerRuntime.isEnabled())
            {
                composerMidi.clear();
                composerRuntime.releaseAll (composerMidi);
                sendComposerMidiToEngine (composerMidi);
            }
        }
        applyPianoRollOverrideLocked();
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        pianoRoll.bind (pack.dspGraph);
        composerRuntime.bind (pack.dspGraph);
        composerRuntime.reset();
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
    }

    void PlayerProcessor::sendComposerMidiToEngine (juce::MidiBuffer& buffer)
    {
        if (engine == nullptr)
            return;

        auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get());
        for (const auto meta : buffer)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const int note = juce::jlimit (0, 127, m.getNoteNumber());
                if (multi != nullptr)
                    multi->noteOnForChannel (note, m.getFloatVelocity(), m.getChannel());
                else
                    engine->noteOn (note, m.getFloatVelocity());
                if (userSampleOverlayEnabled)
                    userSampleOverlay.noteOn (note, m.getFloatVelocity());
            }
            else if (m.isNoteOff())
            {
                const int note = juce::jlimit (0, 127, m.getNoteNumber());
                if (multi != nullptr)
                    multi->noteOffForChannel (note, m.getChannel());
                else
                    engine->noteOff (note);
                if (userSampleOverlayEnabled)
                    userSampleOverlay.noteOff (note);
            }
        }
        buffer.clear();
    }

    void PlayerProcessor::applyPianoRollOverrideLocked()
    {
        if (runtimePianoRollNotes.isEmpty())
            return;

        for (auto& block : pack.dspGraph.blocks)
        {
            if (PianoRollRuntime::isPianoRollBlock (block))
            {
                block.metadata["notes"] = runtimePianoRollNotes;
                break;
            }
        }
    }

    RenderContext PlayerProcessor::makeRenderContext (int numSamples) const
    {
        double tempo = 120.0;
        if (const auto found = runtimeParameterValues.find ("projectBpm");
            found != runtimeParameterValues.end())
            tempo = found->second;

        auto context = RenderContext::forBlock (currentSampleRate,
                                               numSamples,
                                               currentBlockSize,
                                               getTotalNumInputChannels(),
                                               getTotalNumOutputChannels(),
                                               tempo);

        if (auto* playHead = getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                if (auto bpm = position->getBpm())
                    context.bpm = RenderContext::sanitiseBpm (*bpm);
                context.isPlaying = position->getIsPlaying();
                context.isRecording = position->getIsRecording();
                if (auto ppq = position->getPpqPosition())
                    context.ppqPosition = *ppq;
                if (auto barStart = position->getPpqPositionOfLastBarStart())
                    context.ppqPositionOfLastBarStart = *barStart;
                if (auto seconds = position->getTimeInSeconds())
                    context.timeInSeconds = *seconds;
                if (auto samples = position->getTimeInSamples())
                    context.timeInSamples = *samples;
                if (auto signature = position->getTimeSignature())
                {
                    context.timeSigNumerator = RenderContext::positiveOr (signature->numerator, 4);
                    context.timeSigDenominator = RenderContext::positiveOr (signature->denominator, 4);
                }
            }
        }

        return context;
    }

    double PlayerProcessor::getHostBpm() const
    {
        return makeRenderContext (currentBlockSize).bpm;
    }

    float PlayerProcessor::getCurrentPackParameterValue (const juce::String& parameterId) const
    {
        if (const auto runtime = runtimeParameterValues.find (parameterId);
            runtime != runtimeParameterValues.end())
        {
            return runtime->second;
        }

        const auto* def = pack.parameters.find (parameterId);
        ParameterDef registryDef;
        if (def == nullptr
            && ParameterModel::getRegistryDefinition (parameterId, pack.manifest.engine, registryDef))
        {
            def = &registryDef;
        }

        const int slotIndex = getParameterSlotIndex (parameterId);
        if (def != nullptr && slotIndex >= 0 && slotIndex < (int) paramSlots.size())
        {
            const float normalised = paramSlots[(size_t) slotIndex].value != nullptr
                ? paramSlots[(size_t) slotIndex].value->load() : 0.0f;
            return juce::jmap (juce::jlimit (0.0f, 1.0f, normalised), 0.0f, 1.0f, def->min, def->max);
        }

        return def != nullptr ? def->defaultValue : 0.0f;
    }

    int PlayerProcessor::getParameterSlotIndex (const juce::String& parameterId) const
    {
        return getHostParameterSlotIndex (parameterId);
    }

    bool PlayerProcessor::setEngineParameterIfPresent (const juce::String& parameterId, float value)
    {
        for (const auto& def : pack.parameters.getAll())
        {
            if (def.id == parameterId)
            {
                const float limited = juce::jlimit (def.min, def.max, value);
                engine->setParameter (parameterId, limited);
                routingEngine.setParameterValue (parameterId, limited);
                return true;
            }
        }

        return false;
    }

    bool PlayerProcessor::setPackParameterValue (const juce::String& parameterId, float value, bool notifyHost)
    {
        const int slotIndex = getParameterSlotIndex (parameterId);
        const auto* defPtr = pack.parameters.find (parameterId);
        ParameterDef registryDef;
        if (defPtr == nullptr
            && ParameterModel::getRegistryDefinition (parameterId, pack.manifest.engine, registryDef))
        {
            defPtr = &registryDef;
        }
        if (defPtr == nullptr)
            return false;

        const auto& def = *defPtr;
        const float limited = juce::jlimit (def.min, def.max, value);
        const float normalised = def.max > def.min
            ? juce::jlimit (0.0f, 1.0f, (limited - def.min) / (def.max - def.min))
            : 0.0f;

        runtimeParameterValues[parameterId] = limited;

        if (slotIndex >= 0 && slotIndex < (int) paramSlots.size())
        {
            if (auto* parameter = apvts.getParameter (slotId (slotIndex)))
            {
                if (notifyHost)
                    parameter->setValueNotifyingHost (normalised);
                else if (paramSlots[(size_t) slotIndex].value != nullptr)
                    paramSlots[(size_t) slotIndex].value->store (normalised);
            }
            else if (paramSlots[(size_t) slotIndex].value != nullptr)
            {
                paramSlots[(size_t) slotIndex].value->store (normalised);
            }
        }

        if (engine != nullptr)
            engine->setParameter (parameterId, limited);
        if (userSampleOverlayEnabled)
            userSampleOverlay.setParameter (parameterId, limited);
        routingEngine.setParameterValue (parameterId, limited);
        updateGraphBlockParameterValue (pack.dspGraph, pack.parameters, parameterId, limited);
        routingEngine.setFxBlockParameterValue (parameterId, limited);

        if (parameterId == "retrigger" || parameterId == "arpLaneMultiLane")
        {
            if (auto* block = findMidiPlaygroundBlock (pack.dspGraph))
            {
                if (parameterId == "retrigger")
                    block->values["retrigger"] = limited >= 0.5f ? 1.0f : 0.0f;
                else
                    block->values["mpMultiLane"] = limited >= 0.5f ? 1.0f : 0.0f;

                arpeggiator.bind (pack.dspGraph);
                polySequencer.bind (pack.dspGraph);
                routingEngine.bind (pack.dspGraph, pack.parameters);
                routingEngine.prepare (makeRenderContext (currentBlockSize));
            }
        }

        if (parameterId.startsWith ("arpLane"))
        {
            if (auto* block = findMidiPlaygroundBlock (pack.dspGraph))
            {
                auto value = [this] (const juce::String& id, float fallback)
                {
                    const auto found = runtimeParameterValues.find (id);
                    return found != runtimeParameterValues.end() ? found->second : fallback;
                };

                const int elementLane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneIndex", 0.0f)));
                const int lane = parameterId == "arpLaneIndex"
                    ? elementLane
                    : juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneControlBank", (float) elementLane)));
                const int steps = juce::jlimit (1, 128, juce::roundToInt (value ("arpLaneSteps", 16.0f)));
                const int target = juce::jlimit (0, 4, juce::roundToInt (value ("arpLaneTarget", 0.0f)));
                const int direction = juce::jlimit (0, 3, juce::roundToInt (value ("arpLaneDirection", 0.0f)));
                const int sound = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneSound", (float) lane)));
                const int rootNote = juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRootNote", 60.0f)));
                const int slots = juce::jlimit (1, 64, juce::roundToInt (value ("arpLaneSampleSlots", 1.0f)));
                const auto targetName = orbitLaneTargetName (target);
                juce::ignoreUnused (rootNote);

                block->values["mpActiveBank"] = (float) lane;
                block->values["mpMultiLane"] = value ("arpLaneMultiLane", 0.0f) >= 0.5f ? 1.0f : 0.0f;
                setArpLaneValue (*block, lane, "arpSteps", (float) steps);
                setArpLaneValue (*block, lane, "arpPattern", direction == 1 ? 1.0f : direction == 2 ? 2.0f : direction == 3 ? 7.0f : 0.0f);
                setArpLaneValue (*block, lane, "arpGate", juce::jlimit (0.05f, 1.0f, value ("arpLaneGate", 0.58f)));
                setArpLaneValue (*block, lane, "arpSwing", juce::jlimit (0.0f, 0.5f, value ("arpLaneSwing", 0.0f)));
                setArpLaneValue (*block, lane, "rate", juce::jlimit (0.0625f, 16.0f, value ("arpLaneRate", 1.0f)));
                const bool fillActive = value ("arpLaneFillMomentary", 0.0f) >= 0.5f
                                     || value ("arpLaneFillLatch", 0.0f) >= 0.5f;
                const int basePulses = juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneEuclideanPulses", 0.0f)));
                const int fillPulses = juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneFillPulses", 0.0f)));
                const float baseProbability = juce::jlimit (0.0f, 1.0f, value ("arpLaneProbability", 1.0f));
                const float fillProbability = juce::jlimit (0.0f, 1.0f, value ("arpLaneFillProbability", 0.0f));
                setArpLaneValue (*block, lane, "mpProbability", fillActive && fillProbability > 0.0f ? fillProbability : baseProbability);
                setArpLaneValue (*block, lane, "mpRatchet", juce::jlimit (1.0f, 8.0f, value ("arpLaneRatchet", 1.0f)));
                setArpLaneValue (*block, lane, "mpEuclideanPulses", (float) (fillActive && fillPulses > 0 ? fillPulses : basePulses));
                setArpLaneValue (*block, lane, "mpEuclideanRotate", (float) juce::jlimit (0, 127, juce::roundToInt (value ("arpLaneRotate", 0.0f))));
                setArpLaneValue (*block, lane, "mpSampleControl", target == 0 ? 0.0f : 1.0f);
                setArpLaneValue (*block, lane, "mpSampleSliceCount", (float) juce::jmax (slots, sound + 1));
                setArpLaneValue (*block, lane, "mpLaneMute", value ("arpLaneMute", 0.0f) >= 0.5f ? 1.0f : 0.0f);
                setArpLaneValue (*block, lane, "mpLaneSolo", value ("arpLaneSolo", 0.0f) >= 0.5f ? 1.0f : 0.0f);
                setArpLaneValue (*block, lane, "mpLaneFxTarget", (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4)))));
                const float laneFxAmount = juce::jlimit (0.0f, 1.0f, value ("arpLaneFxAmount", 0.0f));
                block->values["mpPatternLaunch"] = (float) juce::jlimit (0, 7, juce::roundToInt (value ("arpLanePatternLaunch", 0.0f)));

                const int controlLane = juce::jlimit (0, 15, juce::roundToInt (value ("arpLaneControlBank", (float) lane)));
                const int sliderRole = juce::jlimit (0, 10, juce::roundToInt (value ("arpLaneSliderRole", 0.0f)));
                for (int step = 0; step < steps; ++step)
                {
                    const auto suffix = juce::String (step);
                    if (targetName == "loops")
                        setArpLaneValue (*block, lane, "mpSampleSlice" + suffix, (float) (step % juce::jmax (1, slots)));
                    else if (target != 0)
                        setArpLaneValue (*block, lane, "mpSampleSlice" + suffix, (float) sound);
                    if (parameterId == "arpLaneFxAmount")
                        setArpLaneValue (*block, lane, "mpAutoFxSend" + suffix, laneFxAmount);
                }

                if (parameterId == "arpLaneControlBank" || parameterId == "arpLaneSliderRole")
                {
                    for (int step = 0; step < 16; ++step)
                    {
                        const float v = juce::jlimit (0.0f, 1.0f, value ("arpLaneStep" + juce::String (step + 1),
                                                                         step % 4 == 0 ? 0.92f : 0.68f));
                        const auto suffix = juce::String (step);
                        if (sliderRole == 0)
                            setArpLaneValue (*block, controlLane, "mpVelocity" + suffix, v);
                        else if (sliderRole == 1)
                            setArpLaneValue (*block, controlLane, "mpGate" + suffix, juce::jlimit (0.05f, 1.0f, 0.05f + v * 0.95f));
                        else if (sliderRole == 2)
                            setArpLaneValue (*block, controlLane, "mpStepProb" + suffix, v);
                        else if (sliderRole == 3)
                            setArpLaneValue (*block, controlLane, "mpStepDiv" + suffix, (float) juce::jlimit (1, 8, 1 + juce::roundToInt (v * 7.0f)));
                        else if (sliderRole == 4)
                            setArpLaneValue (*block, controlLane, "mpStep" + suffix + "On", v >= 0.5f ? 1.0f : 0.0f);
                        else if (sliderRole == 5)
                            setArpLaneValue (*block, controlLane, "mpStepDelay" + suffix, juce::jlimit (0.0f, 0.85f, v * 0.85f));
                        else if (sliderRole == 6)
                            setArpLaneValue (*block, controlLane, "mpSampleSlice" + suffix, (float) juce::jlimit (0, slots - 1, juce::roundToInt (v * (float) juce::jmax (1, slots - 1))));
                        else if (sliderRole == 7)
                            setArpLaneValue (*block, controlLane, "mpStepTranspose" + suffix, (float) juce::jlimit (-24, 24, juce::roundToInt (v * 48.0f - 24.0f)));
                        else if (sliderRole == 8)
                            setArpLaneValue (*block, controlLane, "mpAutoFilter" + suffix, v);
                        else if (sliderRole == 9)
                            setArpLaneValue (*block, controlLane, "mpAutoPan" + suffix, juce::jlimit (-1.0f, 1.0f, v * 2.0f - 1.0f));
                        else if (sliderRole == 10)
                            setArpLaneValue (*block, controlLane, "mpAutoFxSend" + suffix, v);
                    }
                }
                else if (parameterId.startsWith ("arpLaneStep"))
                {
                    const int changedStep = parameterId.fromLastOccurrenceOf ("arpLaneStep", false, false).getIntValue() - 1;
                    if (changedStep >= 0 && changedStep < 16)
                    {
                        const float v = juce::jlimit (0.0f, 1.0f, value (parameterId, 0.68f));
                        const auto suffix = juce::String (changedStep);
                        if (sliderRole == 0)
                            setArpLaneValue (*block, controlLane, "mpVelocity" + suffix, v);
                        else if (sliderRole == 1)
                            setArpLaneValue (*block, controlLane, "mpGate" + suffix, juce::jlimit (0.05f, 1.0f, 0.05f + v * 0.95f));
                        else if (sliderRole == 2)
                            setArpLaneValue (*block, controlLane, "mpStepProb" + suffix, v);
                        else if (sliderRole == 3)
                            setArpLaneValue (*block, controlLane, "mpStepDiv" + suffix, (float) juce::jlimit (1, 8, 1 + juce::roundToInt (v * 7.0f)));
                        else if (sliderRole == 4)
                            setArpLaneValue (*block, controlLane, "mpStep" + suffix + "On", v >= 0.5f ? 1.0f : 0.0f);
                        else if (sliderRole == 5)
                            setArpLaneValue (*block, controlLane, "mpStepDelay" + suffix, juce::jlimit (0.0f, 0.85f, v * 0.85f));
                        else if (sliderRole == 6)
                            setArpLaneValue (*block, controlLane, "mpSampleSlice" + suffix, (float) juce::jlimit (0, slots - 1, juce::roundToInt (v * (float) juce::jmax (1, slots - 1))));
                        else if (sliderRole == 7)
                            setArpLaneValue (*block, controlLane, "mpStepTranspose" + suffix, (float) juce::jlimit (-24, 24, juce::roundToInt (v * 48.0f - 24.0f)));
                        else if (sliderRole == 8)
                            setArpLaneValue (*block, controlLane, "mpAutoFilter" + suffix, v);
                        else if (sliderRole == 9)
                            setArpLaneValue (*block, controlLane, "mpAutoPan" + suffix, juce::jlimit (-1.0f, 1.0f, v * 2.0f - 1.0f));
                        else if (sliderRole == 10)
                            setArpLaneValue (*block, controlLane, "mpAutoFxSend" + suffix, v);
                    }
                }

                setArpLaneMetadata (*block, lane, "Target", targetName);
                setArpLaneMetadata (*block, lane, "Direction",
                    direction == 1 ? "reverse" : direction == 2 ? "bounce" : direction == 3 ? "random" : "forward");
                setArpLaneMetadata (*block, lane, "RootNote", juce::String (rootNote));
                setArpLaneMetadata (*block, lane, "Sound", juce::String (sound));
                setArpLaneMetadata (*block, lane, "SoundName", orbitLaneSoundName (sound));
                setArpLaneMetadata (*block, lane, "FxTarget", juce::String (juce::jlimit (0, 7, juce::roundToInt (value ("arpLaneFxTarget", (float) (lane % 4))))));
                setArpLaneMetadata (*block, lane, "FillPulses",
                    juce::String (juce::jlimit (0, steps, juce::roundToInt (value ("arpLaneFillPulses", 0.0f)))));
                setArpLaneMetadata (*block, lane, "FillProbability",
                    juce::String (juce::jlimit (0.0f, 1.0f, value ("arpLaneFillProbability", 0.0f)), 2));
                setArpLaneMetadata (*block, controlLane, "SliderRole",
                    sliderRole == 0 ? "velocity" : sliderRole == 1 ? "gate" : sliderRole == 2 ? "probability"
                    : sliderRole == 3 ? "ratchet" : sliderRole == 4 ? "mute" : sliderRole == 5 ? "delay"
                    : sliderRole == 6 ? "slice" : sliderRole == 7 ? "transpose"
                    : sliderRole == 8 ? "filter" : sliderRole == 9 ? "pan" : "fxSend");

                if (engine != nullptr)
                {
                    arpeggiator.bind (pack.dspGraph);
                    polySequencer.bind (pack.dspGraph);
                }
                routingEngine.bind (pack.dspGraph, pack.parameters);
                routingEngine.prepare (makeRenderContext (currentBlockSize));
            }
        }
        return true;
    }

    bool PlayerProcessor::setPackParameterFromUi (const juce::String& parameterId, float value)
    {
        const auto effectiveParameterId = parameterId == "pitchBend"
            ? juce::String ("pitchWheel")
            : parameterId;
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! setPackParameterValue (effectiveParameterId, value, true))
            return false;

        if (effectiveParameterId == "pitchWheel")
            midiPitchBendCents.store (juce::jlimit (-1.0f, 1.0f, value) * 200.0f);
        else if (effectiveParameterId == "modWheel")
        {
            const auto normalised = juce::jlimit (0.0f, 1.0f, value);
            midiModWheel.store (normalised);
            scriptEngine.triggerEvent ("modwheel moves", {{"modwheel", normalised * 127.0f}});
        }

        return true;
    }

    bool PlayerProcessor::setDrumPatternCellFromUi (int pattern, int track, int step, bool enabled,
                                                    float velocity, float gate, float probability,
                                                    int divisions)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        DspBlock* drumBlock = nullptr;
        for (auto& block : pack.dspGraph.blocks)
        {
            if (block.type.containsIgnoreCase ("drum")
                || block.values.find ("dmTracks") != block.values.end())
            {
                drumBlock = &block;
                break;
            }
        }

        if (drumBlock == nullptr)
            return false;

        pattern = juce::jlimit (0, 7, pattern);
        track = juce::jlimit (0, 15, track);
        step = juce::jlimit (0, 63, step);

        drumBlock->values["dmTracks"] = (float) juce::jmax (track + 1,
            juce::roundToInt (drumBlock->values.count ("dmTracks") != 0 ? drumBlock->values["dmTracks"] : 8.0f));
        drumBlock->values["dmSteps"] = (float) juce::jmax (step + 1,
            juce::roundToInt (drumBlock->values.count ("dmSteps") != 0 ? drumBlock->values["dmSteps"] : 16.0f));
        drumBlock->values["dmPattern"] = (float) pattern;

        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        drumBlock->values[prefix + "On"] = enabled ? 1.0f : 0.0f;
        drumBlock->values[prefix + "Vel"] = juce::jlimit (0.01f, 1.0f, velocity);
        drumBlock->values[prefix + "Gate"] = juce::jlimit (0.05f, 1.0f, gate);
        drumBlock->values[prefix + "Prob"] = juce::jlimit (0.0f, 1.0f, probability);
        if (divisions > 0)
            drumBlock->values[prefix + "Div"] = (float) juce::jlimit (1, 4, divisions);
        else if (drumBlock->values.find (prefix + "Div") == drumBlock->values.end())
            drumBlock->values[prefix + "Div"] = 1.0f;

        if (engine != nullptr)
            arpeggiator.allNotesOff (*engine);
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    bool PlayerProcessor::setDrumActivePatternFromUi (int pattern)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        DspBlock* drumBlock = nullptr;
        for (auto& block : pack.dspGraph.blocks)
        {
            if (block.type.containsIgnoreCase ("drum")
                || block.values.find ("dmTracks") != block.values.end())
            {
                drumBlock = &block;
                break;
            }
        }

        if (drumBlock == nullptr)
            return false;

        drumBlock->values["dmPattern"] = (float) juce::jlimit (0, 7, pattern);
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    static DspBlock* findMutableDrumBlock (DspGraph& graph)
    {
        for (auto& block : graph.blocks)
            if (block.type.containsIgnoreCase ("drum") || block.values.find ("dmTracks") != block.values.end())
                return &block;
        return nullptr;
    }

    bool PlayerProcessor::setDrumTrackMutedFromUi (int track, bool muted)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;
        auto* drumBlock = findMutableDrumBlock (pack.dspGraph);
        if (drumBlock == nullptr)
            return false;
        track = juce::jlimit (0, 15, track);
        drumBlock->values["dmTrack" + juce::String (track) + "Mute"] = muted ? 1.0f : 0.0f;
        arpeggiator.bind (pack.dspGraph);
        return true;
    }

    bool PlayerProcessor::setDrumTrackSoloFromUi (int track, bool solo)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;
        auto* drumBlock = findMutableDrumBlock (pack.dspGraph);
        if (drumBlock == nullptr)
            return false;
        track = juce::jlimit (0, 15, track);
        if (solo)
        {
            const int tracks = juce::jlimit (1, 16, juce::roundToInt (
                drumBlock->values.count ("dmTracks") != 0 ? drumBlock->values.at ("dmTracks") : 8.0f));
            for (int other = 0; other < tracks; ++other)
                drumBlock->values["dmTrack" + juce::String (other) + "Solo"] = other == track ? 1.0f : 0.0f;
        }
        else
        {
            drumBlock->values["dmTrack" + juce::String (track) + "Solo"] = 0.0f;
        }
        arpeggiator.bind (pack.dspGraph);
        return true;
    }

    bool PlayerProcessor::setDrumTrackNoteFromUi (int track, int midiNote)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;
        auto* drumBlock = findMutableDrumBlock (pack.dspGraph);
        if (drumBlock == nullptr)
            return false;
        track = juce::jlimit (0, 15, track);
        midiNote = juce::jlimit (0, 127, midiNote);
        drumBlock->values["dmTriggerPadSlots"] = 0.0f;
        drumBlock->values["dmTrack" + juce::String (track) + "Note"] = (float) midiNote;
        drumBlock->metadata["dmTrack" + juce::String (track) + "Label"] =
            juce::MidiMessage::getMidiNoteName (midiNote, true, true, 4);
        arpeggiator.bind (pack.dspGraph);
        editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
        return true;
    }

    bool PlayerProcessor::setDrumSongModeFromUi (bool songMode)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;
        auto* drumBlock = findMutableDrumBlock (pack.dspGraph);
        if (drumBlock == nullptr)
            return false;
        drumBlock->values["dmSongMode"] = songMode ? 1.0f : 0.0f;
        arpeggiator.bind (pack.dspGraph);
        return true;
    }

    bool PlayerProcessor::setMidiPlaygroundActiveBankFromUi (int bank)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        DspBlock* midiBlock = nullptr;
        for (auto& block : pack.dspGraph.blocks)
        {
            if (block.type.containsIgnoreCase ("arp")
                || block.type.containsIgnoreCase ("midi")
                || block.values.find ("arpSteps") != block.values.end())
            {
                midiBlock = &block;
                break;
            }
        }

        if (midiBlock == nullptr)
            return false;

        midiBlock->values["mpActiveBank"] = (float) juce::jlimit (0, 4, bank);
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    bool PlayerProcessor::setArpLaneStepFromUi (int lane, int step, float velocity, bool active)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        auto* midiBlock = findMidiPlaygroundBlock (pack.dspGraph);
        if (midiBlock == nullptr)
            return false;

        lane = juce::jlimit (0, 15, lane);
        step = juce::jlimit (0, 127, step);
        const float limitedVelocity = juce::jlimit (0.0f, 1.0f, velocity);
        const auto suffix = juce::String (step);
        midiBlock->values["mpActiveBank"] = (float) juce::jlimit (0, 4, lane);
        midiBlock->values["mpMultiLane"] = 1.0f;
        setArpLaneValue (*midiBlock, lane, "mpLaneTarget", (float) juce::jlimit (0, 4, lane));
        setArpLaneValue (*midiBlock, lane, "mpVelocity" + suffix, limitedVelocity);
        setArpLaneValue (*midiBlock, lane, "mpStep" + suffix + "On", active ? 1.0f : 0.0f);

        // Orbit ring roles: Pitch=vel/notes, Filter/Pan/FX/Slice write their control params.
        if (lane == 1)
            setArpLaneValue (*midiBlock, lane, "mpAutoFilter" + suffix, limitedVelocity);
        else if (lane == 2)
            setArpLaneValue (*midiBlock, lane, "mpAutoPan" + suffix, limitedVelocity * 2.0f - 1.0f);
        else if (lane == 3)
        {
            setArpLaneValue (*midiBlock, lane, "mpAutoFxSend" + suffix, limitedVelocity);
            if (midiBlock->values.find (arpBankPrefix (lane) + "mpLaneFxTarget") == midiBlock->values.end())
                setArpLaneValue (*midiBlock, lane, "mpLaneFxTarget", 0.0f); // delay
        }
        else if (lane == 4)
        {
            setArpLaneValue (*midiBlock, lane, "mpSampleControl", 1.0f);
            setArpLaneValue (*midiBlock, lane, "mpSampleSliceCount", 16.0f);
            setArpLaneValue (*midiBlock, lane, "mpSampleSlice" + suffix,
                             (float) juce::jlimit (0, 15, juce::roundToInt (limitedVelocity * 15.0f)));
        }

        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    bool PlayerProcessor::setArpLaneStepsFromUi (int lane, int steps)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        auto* midiBlock = findMidiPlaygroundBlock (pack.dspGraph);
        if (midiBlock == nullptr)
            return false;

        lane = juce::jlimit (0, 15, lane);
        steps = juce::jlimit (1, 128, steps);
        setArpLaneValue (*midiBlock, lane, "arpSteps", (float) steps);

        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    bool PlayerProcessor::setSeqLaneStepFromUi (int laneIndex, int step, float value, bool active, const juce::String& laneType)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        auto* midiBlock = findMidiPlaygroundBlock (pack.dspGraph);
        if (midiBlock == nullptr)
            return false;

        laneIndex = juce::jlimit (0, 15, laneIndex);
        step = juce::jlimit (0, 127, step);
        const auto suffix = juce::String (step);

        if (laneType == "pitch")
        {
            setArpLaneValue (*midiBlock, laneIndex, "arpNote" + suffix, juce::jmap (value, -24.0f, 24.0f, -24.0f, 24.0f));
            setArpLaneValue (*midiBlock, laneIndex, "mpStep" + suffix + "On", active ? 1.0f : 0.0f);
        }
        else if (laneType == "chance")
        {
            setArpLaneValue (*midiBlock, laneIndex, "mpStepProb" + suffix, juce::jlimit (0.0f, 1.0f, value));
            setArpLaneValue (*midiBlock, laneIndex, "mpStep" + suffix + "On", active ? 1.0f : 0.0f);
        }
        else if (laneType == "gate")
        {
            const float velocity = juce::jlimit (0.05f, 1.0f,
                ArpLaneUi::readLaneValue (midiBlock, laneIndex, "mpVelocity" + suffix, 0.72f));
            return setArpLaneStepFromUi (laneIndex, step, velocity, active);
        }
        else
        {
            return setArpLaneStepFromUi (laneIndex, step, juce::jlimit (0.0f, 1.0f, value), active);
        }

        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    juce::String PlayerProcessor::getPianoRollNotesEncoded() const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (runtimePianoRollNotes.isNotEmpty())
            return runtimePianoRollNotes;
        for (const auto& block : pack.dspGraph.blocks)
            if (PianoRollRuntime::isPianoRollBlock (block))
                if (const auto found = block.metadata.find ("notes"); found != block.metadata.end())
                    return found->second;
        return {};
    }

    bool PlayerProcessor::setPianoRollNotesFromUi (const juce::String& encodedNotes)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        bool hasBlock = false;
        for (auto& block : pack.dspGraph.blocks)
        {
            if (PianoRollRuntime::isPianoRollBlock (block))
            {
                block.metadata["notes"] = encodedNotes;
                hasBlock = true;
                break;
            }
        }
        if (! hasBlock)
            return false;

        runtimePianoRollNotes = encodedNotes;
        if (engine)
            pianoRoll.allNotesOff (*engine);
        pianoRoll.bind (pack.dspGraph);
        return true;
    }

    double PlayerProcessor::getPianoRollPlaybackPosition01() const
    {
        if (! isAnyTransportPlaying())
            return -1.0;
        return pianoRoll.getPlaybackPosition01();
    }

    bool PlayerProcessor::allowsExternalPackLoading() const
    {
        if (! loaded)
            return true;
        return pack.manifest.playerAllowPackLoading || pack.manifest.playerShowLibraryBrowser;
    }

    bool PlayerProcessor::applyPresetByIndex (int presetIndex)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded || presetIndex < 0 || presetIndex >= (int) pack.presets.size())
            return false;

        const auto& preset = pack.presets[(size_t) presetIndex];

        // Reset every parameter to its default first so the preset can't
        // inherit values from the previously-selected preset for any keys it
        // doesn't override. This guarantees identical sound regardless of
        // the order presets are selected.
        for (const auto& def : pack.parameters.getAll())
            setPackParameterValue (def.id, def.defaultValue, true);

        if (const auto* patch = pack.findPatchForPreset (preset))
        {
            applyPatchStateToPack (pack, *patch);
            authoredSampleMap = pack.sampleMap;
            rebuildRuntimeUserContentLocked (false);
            if (engine != nullptr)
            {
                engine->allNotesOff();
                if (loadedPath.isDirectory())
                    engine->loadFromPack (loadedPath, pack.sampleMap);
            }
            bindRoutingFromPack();
            // Apply the patch's recorded parameter snapshot, then let the
            // preset's own .values overlay so the preset is authoritative
            // (a patch is treated as a backing snapshot, not the source of
            // truth for tonal differences between presets).
            for (const auto& kv : patch->parameterValues)
                setPackParameterValue (kv.first, kv.second, true);
        }

        pack.manifest.defaultPreset = preset.name;
        for (const auto& def : pack.parameters.getAll())
        {
            auto valueIt = preset.values.find (def.id);
            if (valueIt != preset.values.end())
                setPackParameterValue (def.id, valueIt->second, true);
        }
        return true;
    }

    int PlayerProcessor::getPresetCount() const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || ! loaded)
            return 0;
        return (int) pack.presets.size();
    }

    int PlayerProcessor::getCurrentPresetIndex() const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || ! loaded || pack.presets.empty())
            return -1;

        const auto current = pack.manifest.defaultPreset.trim();
        if (current.isNotEmpty())
        {
            for (int index = 0; index < (int) pack.presets.size(); ++index)
                if (pack.presets[(size_t) index].name == current)
                    return index;
        }

        return 0;
    }

    juce::String PlayerProcessor::getPresetName (int presetIndex) const
    {
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || ! loaded
            || presetIndex < 0 || presetIndex >= (int) pack.presets.size())
            return {};

        return pack.presets[(size_t) presetIndex].name;
    }

    juce::StringArray PlayerProcessor::getPresetNames() const
    {
        juce::StringArray names;
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (! lock.isLocked() || ! loaded)
            return names;

        for (const auto& preset : pack.presets)
            names.add (preset.name.isNotEmpty() ? preset.name : "Preset " + juce::String (names.size() + 1));
        return names;
    }

    bool PlayerProcessor::applyPresetOffset (int delta)
    {
        const int count = getPresetCount();
        if (count <= 0)
            return false;

        int current = getCurrentPresetIndex();
        if (current < 0)
            current = 0;

        const int next = (current + delta + count) % count;
        return applyPresetByIndex (next);
    }

    void PlayerProcessor::randomizeCurrentPreset()
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded || engine == nullptr) return;
        juce::Random rng (static_cast<int> (juce::Time::getCurrentTime().toMilliseconds() & 0x7fffffff));
        for (const auto& def : pack.parameters.getAll())
        {
            if (! def.modulatable) continue;
            const float v = def.min + rng.nextFloat() * (def.max - def.min);
            setPackParameterValue (def.id, v, true);
        }
    }

    void PlayerProcessor::restoreAllPresets()
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded) return;

        arpeggiator.allNotesOff (*engine);
        engine->allNotesOff();
        heldNotes.fill (false);
        sustainPedalDown = false;
        midiPitchBendCents.store (0.0f);
        midiModWheel.store (-1.0f);

        for (const auto& def : pack.parameters.getAll())
        {
            auto defaultIt = defaultPresetValues.find (def.id);
            setPackParameterValue (def.id,
                                   defaultIt != defaultPresetValues.end()
                                       ? defaultIt->second
                                       : def.defaultValue,
                                   true);
        }
    }

    void PlayerProcessor::setDefaultPreset()
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded) return;
        
        defaultPresetValues.clear();
        for (const auto& def : pack.parameters.getAll())
            defaultPresetValues[def.id] = getCurrentPackParameterValue (def.id);
    }

    void PlayerProcessor::saveAbSnapshot (int slot)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded) return;
        auto& target = (slot == 0) ? abSnapshotA : abSnapshotB;
        target.clear();
        for (const auto& def : pack.parameters.getAll())
            target[def.id] = getCurrentPackParameterValue (def.id);
    }

    void PlayerProcessor::recallAbSnapshot (int slot)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded) return;
        const auto& source = (slot == 0) ? abSnapshotA : abSnapshotB;
        if (source.empty()) return;
        for (const auto& kv : source)
            setPackParameterValue (kv.first, kv.second, true);
    }

    juce::String PlayerProcessor::getAbSnapshotName (int slot) const
    {
        const auto& source = (slot == 0) ? abSnapshotA : abSnapshotB;
        return source.empty() ? (slot == 0 ? "A (empty)" : "B (empty)")
                              : (slot == 0 ? "A" : "B");
    }

    std::vector<PlayerProcessor::UserSnapshotInfo> PlayerProcessor::getUserSnapshots() const
    {
        const juce::ScopedLock lock (snapshotLock);
        std::vector<UserSnapshotInfo> result;
        result.reserve (userSnapshots.size());
        for (const auto& snapshot : userSnapshots)
        {
            UserSnapshotInfo info;
            info.id = snapshot.id;
            info.name = snapshot.name;
            info.notes = snapshot.notes;
            info.favorite = snapshot.favorite;
            info.sourcePresetIndex = snapshot.sourcePresetIndex;
            info.parameterCount = (int) snapshot.values.size();
            result.push_back (info);
        }
        return result;
    }

    bool PlayerProcessor::saveUserSnapshot (const juce::String& name, bool favorite)
    {
        UserSnapshot snapshot;
        snapshot.id = juce::Uuid().toString();
        snapshot.favorite = favorite;

        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            if (! loaded)
                return false;

            snapshot.name = name.trim().isNotEmpty()
                ? name.trim()
                : (pack.manifest.defaultPreset.isNotEmpty()
                    ? pack.manifest.defaultPreset + " User Snapshot"
                    : juce::String ("User Snapshot ") + juce::String ((int) userSnapshots.size() + 1));
            snapshot.notes = "Captured inside the Player from "
                + (pack.manifest.playerDisplayName.isNotEmpty()
                    ? pack.manifest.playerDisplayName
                    : pack.manifest.instrumentName);
            snapshot.sourcePresetIndex = -1;
            const auto current = pack.manifest.defaultPreset.trim();
            if (current.isNotEmpty())
                for (int index = 0; index < (int) pack.presets.size(); ++index)
                    if (pack.presets[(size_t) index].name == current)
                    {
                        snapshot.sourcePresetIndex = index;
                        break;
                    }

            for (const auto& def : pack.parameters.getAll())
                snapshot.values[def.id] = getCurrentPackParameterValue (def.id);
        }

        const juce::ScopedLock lock (snapshotLock);
        userSnapshots.push_back (std::move (snapshot));
        return true;
    }

    bool PlayerProcessor::applyUserSnapshot (const juce::String& id)
    {
        std::map<juce::String, float> values;
        {
            const juce::ScopedLock lock (snapshotLock);
            for (const auto& snapshot : userSnapshots)
            {
                if (snapshot.id == id)
                {
                    values = snapshot.values;
                    break;
                }
            }
        }

        if (values.empty())
            return false;

        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        bool changed = false;
        for (const auto& value : values)
            changed = setPackParameterValue (value.first, value.second, true) || changed;
        return changed;
    }

    bool PlayerProcessor::deleteUserSnapshot (const juce::String& id)
    {
        const juce::ScopedLock lock (snapshotLock);
        const auto before = userSnapshots.size();
        userSnapshots.erase (std::remove_if (userSnapshots.begin(), userSnapshots.end(),
                                             [&] (const UserSnapshot& snapshot) { return snapshot.id == id; }),
                             userSnapshots.end());
        return userSnapshots.size() != before;
    }

    bool PlayerProcessor::toggleUserSnapshotFavorite (const juce::String& id)
    {
        const juce::ScopedLock lock (snapshotLock);
        for (auto& snapshot : userSnapshots)
        {
            if (snapshot.id == id)
            {
                snapshot.favorite = ! snapshot.favorite;
                return true;
            }
        }
        return false;
    }

    bool PlayerProcessor::isMultiInstrumentPack() const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        return loaded && dynamic_cast<const MultiInstrumentEngine*> (engine.get()) != nullptr;
    }

    int PlayerProcessor::getMultiLayerCount() const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
            return multi->getLayerCount();
        return 0;
    }

    juce::String PlayerProcessor::getMultiLayerName (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].name.isNotEmpty() ? layers[(size_t) index].name
                                                                : layers[(size_t) index].id;
        }
        return {};
    }

    int PlayerProcessor::getMultiLayerActiveVoiceCount (int index) const
    {
        const juce::SpinLock::ScopedTryLockType lk (engineLock);
        if (! lk.isLocked())
            return 0;

        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size() && layers[(size_t) index].engine)
                return layers[(size_t) index].engine->getActiveVoiceCount();
        }

        return 0;
    }

    int PlayerProcessor::getMultiLayerLoadedSampleCount (int index) const
    {
        const juce::SpinLock::ScopedTryLockType lk (engineLock);
        if (! lk.isLocked())
            return 0;

        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size() && layers[(size_t) index].engine)
                return layers[(size_t) index].engine->getLoadedSampleCount();
        }

        return 0;
    }

    bool PlayerProcessor::getMultiLayerMuted (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].muted;
        }
        return false;
    }

    bool PlayerProcessor::getMultiLayerSoloed (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].solo;
        }
        return false;
    }

    bool PlayerProcessor::getMultiLayerEnabled (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].enabled;
        }
        return true;
    }

    int PlayerProcessor::getMultiLayerMidiChannel (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].midiChannel;
        }
        return 0;
    }

    int PlayerProcessor::getMultiLayerOutputRoute (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].outputRoute;
        }
        return 0;
    }

    int PlayerProcessor::getMultiLayerTransposeSemitones (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].transposeSemitones;
        }
        return 0;
    }

    float PlayerProcessor::getMultiLayerVolume (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].volume;
        }
        return 1.0f;
    }

    float PlayerProcessor::getMultiLayerPan (int index) const
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                return layers[(size_t) index].pan;
        }
        return 0.0f;
    }

    void PlayerProcessor::setMultiLayerVolume (int index, float volume)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerVolume (layers[(size_t) index].id, juce::jlimit (0.0f, 1.0f, volume));
        }
    }

    void PlayerProcessor::setMultiLayerPan (int index, float pan)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerPan (layers[(size_t) index].id, juce::jlimit (-1.0f, 1.0f, pan));
        }
    }

    void PlayerProcessor::setMultiLayerMuted (int index, bool muted)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerMute (layers[(size_t) index].id, muted);
        }
    }

    void PlayerProcessor::setMultiLayerSoloed (int index, bool solo)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerSolo (layers[(size_t) index].id, solo);
        }
    }

    void PlayerProcessor::setMultiLayerEnabled (int index, bool enabled)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerEnabled (layers[(size_t) index].id, enabled);
        }
    }

    void PlayerProcessor::setMultiLayerMidiChannel (int index, int channel)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerMidiChannel (layers[(size_t) index].id, channel);
        }
    }

    void PlayerProcessor::setMultiLayerOutputRoute (int index, int route)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerOutputRoute (layers[(size_t) index].id, route);
        }
    }

    void PlayerProcessor::setMultiLayerTransposeSemitones (int index, int semitones)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
        {
            const auto& layers = multi->getLayers();
            if (index >= 0 && index < (int) layers.size())
                multi->setLayerTransposeSemitones (layers[(size_t) index].id, semitones);
        }
    }

    bool PlayerProcessor::applyMidiMappings (const juce::MidiMessage& message)
    {
        bool handled = false;
        {
            const juce::SpinLock::ScopedLockType lk (midiMappingLock);
            for (const auto& mapping : userMidiMappings)
            {
                if (! mapping.matches (message))
                    continue;

                const float normalised = mapping.normalisedValueFromMessage (message);
                const float value = mapping.bipolar
                    ? juce::jmap (normalised, -1.0f, 1.0f, mapping.targetMin, mapping.targetMax)
                    : juce::jmap (normalised, 0.0f, 1.0f, mapping.targetMin, mapping.targetMax);
                handled = setPackParameterValue (mapping.parameterId, value, true) || handled;
            }
        }

        for (const auto& mapping : pack.midiMappings)
        {
            if (! mapping.matches (message))
                continue;

            const float normalised = mapping.normalisedValueFromMessage (message);
            const float value = mapping.bipolar
                ? juce::jmap (normalised, -1.0f, 1.0f, mapping.targetMin, mapping.targetMax)
                : juce::jmap (normalised, 0.0f, 1.0f, mapping.targetMin, mapping.targetMax);
            handled = setPackParameterValue (mapping.parameterId, value, true) || handled;
        }
        return handled;
    }

    bool PlayerProcessor::captureMidiLearnMessage (const juce::MidiMessage& message)
    {
        const bool supported = message.isController() || message.isPitchWheel()
                            || message.isAftertouch() || message.isChannelPressure();
        if (! supported)
            return false;

        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        if (pendingMidiLearnParameter.isEmpty())
            return false;

        const auto parameterId = pendingMidiLearnParameter;
        const ParameterDef* def = nullptr;
        for (const auto& candidate : pack.parameters.getAll())
        {
            if (candidate.id == parameterId)
            {
                def = &candidate;
                break;
            }
        }
        if (def == nullptr || ! def->midiLearnable)
        {
            pendingMidiLearnParameter.clear();
            return true;
        }

        MidiMapping mapping;
        mapping.id = "user_midi_" + parameterId;
        mapping.parameterId = parameterId;
        mapping.channel = message.getChannel();
        mapping.targetMin = def->min;
        mapping.targetMax = def->max;

        if (message.isController())
        {
            mapping.sourceType = "cc";
            mapping.controller = message.getControllerNumber();
        }
        else if (message.isPitchWheel())
        {
            mapping.sourceType = "pitchWheel";
            mapping.bipolar = true;
        }
        else if (message.isAftertouch())
        {
            mapping.sourceType = "aftertouch";
        }
        else if (message.isChannelPressure())
        {
            mapping.sourceType = "channelPressure";
        }

        userMidiMappings.erase (std::remove_if (userMidiMappings.begin(), userMidiMappings.end(),
            [&] (const MidiMapping& existing)
            {
                return existing.parameterId == mapping.parameterId
                    || (existing.sourceType == mapping.sourceType
                        && existing.channel == mapping.channel
                        && existing.controller == mapping.controller);
            }), userMidiMappings.end());
        userMidiMappings.push_back (mapping);
        pendingMidiLearnParameter.clear();
        return true;
    }

    void PlayerProcessor::handleRealtimeMidi (const juce::MidiMessage& message)
    {
        if (message.isNoteOn())
        {
            const int note = juce::jlimit (0, 127, message.getNoteNumber());
            noteHighlightLevels[(size_t) note].store (1.0f);
            heldNotes[(size_t) note] = false;

            scriptEngine.triggerEvent ("note starts", {
                { "velocity", message.getFloatVelocity() * 127.0f },
                { "note", (float) note }
            });

            if (! polySequencer.handleNoteOn (*engine, note, message.getFloatVelocity()) && ! arpeggiator.handleNoteOn (*engine, note, message.getFloatVelocity()))
            {
                if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
                    multi->noteOnForChannel (note, message.getFloatVelocity(), message.getChannel());
                else
                    engine->noteOn (note, message.getFloatVelocity());
            }
            if (userSampleOverlayEnabled)
                userSampleOverlay.noteOn (note, message.getFloatVelocity());
            return;
        }

        if (message.isNoteOff())
        {
            const int note = juce::jlimit (0, 127, message.getNoteNumber());

            scriptEngine.triggerEvent ("note ends", {
                { "velocity", 0.0f },
                { "note", (float) note }
            });

            if (sustainPedalDown)
                heldNotes[(size_t) note] = true;
            else if (! polySequencer.handleNoteOff (*engine, note) && ! arpeggiator.handleNoteOff (*engine, note))
            {
                if (auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get()))
                    multi->noteOffForChannel (note, message.getChannel());
                else
                    engine->noteOff (note);
            }
            if (! sustainPedalDown && userSampleOverlayEnabled)
                userSampleOverlay.noteOff (note);
            return;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            arpeggiator.allNotesOff (*engine);
            engine->allNotesOff();
            userSampleOverlay.allNotesOff();
            heldNotes.fill (false);
            sustainPedalDown = false;
            setPackParameterValue ("sustainPedal", 0.0f, true);
            return;
        }

        if (message.isPitchWheel())
        {
            if (captureMidiLearnMessage (message))
                return;
            if (applyMidiMappings (message))
                return;
            const float bend = ((float) message.getPitchWheelValue() - 8192.0f) / 8192.0f;
            setPackParameterValue ("pitchWheel", bend, false);
            // Persistent state - applied in processBlock AFTER the static
            // parameter sync, so the bend isn't wiped out the moment the
            // host stops emitting wheel events (i.e. while the user is
            // holding the wheel bent).
            midiPitchBendCents.store (bend * 200.0f);   // +/- 2 semitones
            return;
        }

        if (message.isController())
        {
            if (captureMidiLearnMessage (message))
                return;
            if (applyMidiMappings (message))
                return;
            const int controller = message.getControllerNumber();
            const float normalised = (float) message.getControllerValue() / 127.0f;

            if (controller == 1)
            {
                setPackParameterValue ("modWheel", normalised, false);
                // Persistent override re-applied each block; see processBlock.
                // Stored as -1 when the wheel hasn't been touched so the
                // user's static LFO/vibrato depths still apply.
                midiModWheel.store (normalised);
                scriptEngine.triggerEvent ("modwheel moves", {{"modwheel", normalised * 127.0f}});
            }
            else if (controller == 11)
            {
                setPackParameterValue ("expression", normalised, true);
            }
            else if (controller == 7)
            {
                setPackParameterValue ("volume", normalised * 1.5f, true);
            }
            else if (controller == 10)
            {
                setPackParameterValue ("pan", normalised * 2.0f - 1.0f, true);
            }
            else if (controller == 64)
            {
                setSustainPedal (normalised >= 0.5f);
                setPackParameterValue ("sustainPedal", normalised, true);
            }
            else if (controller == 74)
            {
                const float cutoff = 20.0f * std::pow (1000.0f, normalised);
                setPackParameterValue ("filterCutoff", cutoff, true);
            }
            else if (controller == 20)
            {
                setPackParameterValue ("sampleStart", normalised, true);
            }
            else if (controller == 21)
            {
                setPackParameterValue ("sampleSlice", normalised * 63.0f, true);
            }
            else if (controller == 22)
            {
                setPackParameterValue ("sampleLength", 0.01f + normalised * 0.99f, true);
            }
            else if (controller == 23)
            {
                setPackParameterValue ("samplePitch", normalised * 48.0f - 24.0f, true);
            }
        }

        if (message.isAftertouch() || message.isChannelPressure())
        {
            if (captureMidiLearnMessage (message))
                return;
            if (applyMidiMappings (message))
                return;
            const float pressure = message.isAftertouch()
                ? (float) message.getAfterTouchValue() / 127.0f
                : (float) message.getChannelPressureValue() / 127.0f;
            setPackParameterValue ("aftertouch", pressure, true);
        }
    }

    void PlayerProcessor::setSustainPedal (bool down)
    {
        if (sustainPedalDown == down)
            return;

        sustainPedalDown = down;
        if (! sustainPedalDown && engine != nullptr)
        {
            for (int note = 0; note < (int) heldNotes.size(); ++note)
            {
                if (heldNotes[(size_t) note])
                {
                    if (! polySequencer.handleNoteOff (*engine, note) && ! arpeggiator.handleNoteOff (*engine, note))
                        engine->noteOff (note);
                    if (userSampleOverlayEnabled)
                        userSampleOverlay.noteOff (note);
                    heldNotes[(size_t) note] = false;
                }
            }
        }
    }

    juce::String PlayerProcessor::midiMappingsToJson() const
    {
        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        juce::Array<juce::var> arr;
        for (const auto& mapping : userMidiMappings)
            arr.add (mapping.toVar());
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("mappings", arr);
        return juce::JSON::toString (juce::var (obj), false);
    }

    void PlayerProcessor::midiMappingsFromJson (const juce::String& json)
    {
        auto parsed = juce::JSON::parse (json);
        std::vector<MidiMapping> loadedMappings;
        if (auto* obj = parsed.getDynamicObject())
            if (auto* arr = obj->getProperty ("mappings").getArray())
                for (const auto& item : *arr)
                    loadedMappings.push_back (MidiMapping::fromVar (item));

        const juce::SpinLock::ScopedLockType lk (midiMappingLock);
        userMidiMappings = std::move (loadedMappings);
        pendingMidiLearnParameter.clear();
    }

    juce::String PlayerProcessor::userSnapshotsToJson() const
    {
        juce::Array<juce::var> snapshots;
        {
            const juce::ScopedLock lock (snapshotLock);
            for (const auto& snapshot : userSnapshots)
            {
                auto* row = new juce::DynamicObject();
                row->setProperty ("id", snapshot.id);
                row->setProperty ("name", snapshot.name);
                row->setProperty ("notes", snapshot.notes);
                row->setProperty ("favorite", snapshot.favorite);
                row->setProperty ("sourcePresetIndex", snapshot.sourcePresetIndex);
                row->setProperty ("values", juce::JSON::parse (floatMapToJson (snapshot.values)));
                snapshots.add (juce::var (row));
            }
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("snapshots", snapshots);
        return juce::JSON::toString (juce::var (root), false);
    }

    void PlayerProcessor::userSnapshotsFromJson (const juce::String& json)
    {
        std::vector<UserSnapshot> loadedSnapshots;
        if (json.trim().isEmpty())
        {
            const juce::ScopedLock lock (snapshotLock);
            userSnapshots.clear();
            return;
        }

        auto parsed = juce::JSON::parse (json);
        if (auto* root = parsed.getDynamicObject())
        {
            if (auto* rows = root->getProperty ("snapshots").getArray())
            {
                for (const auto& item : *rows)
                {
                    auto* row = item.getDynamicObject();
                    if (row == nullptr)
                        continue;

                    UserSnapshot snapshot;
                    snapshot.id = row->getProperty ("id").toString();
                    if (snapshot.id.isEmpty())
                        snapshot.id = juce::Uuid().toString();
                    snapshot.name = row->getProperty ("name").toString();
                    snapshot.notes = row->getProperty ("notes").toString();
                    snapshot.favorite = (bool) row->getProperty ("favorite");
                    snapshot.sourcePresetIndex = row->hasProperty ("sourcePresetIndex")
                        ? (int) row->getProperty ("sourcePresetIndex") : -1;

                    if (auto* values = row->getProperty ("values").getDynamicObject())
                    {
                        const auto& props = values->getProperties();
                        for (int i = 0; i < props.size(); ++i)
                            snapshot.values[props.getName (i).toString()] = (float) props.getValueAt (i);
                    }

                    if (snapshot.name.isEmpty())
                        snapshot.name = "User Snapshot";
                    if (! snapshot.values.empty())
                        loadedSnapshots.push_back (std::move (snapshot));
                }
            }
        }

        const juce::ScopedLock lock (snapshotLock);
        userSnapshots = std::move (loadedSnapshots);
    }

    juce::String PlayerProcessor::multiLayerRackToJson() const
    {
        juce::Array<juce::var> rows;
        {
            const juce::SpinLock::ScopedLockType lk (engineLock);
            if (auto* multi = dynamic_cast<const MultiInstrumentEngine*> (engine.get()))
            {
                const auto& layers = multi->getLayers();
                for (int index = 0; index < (int) layers.size(); ++index)
                {
                    const auto& layer = layers[(size_t) index];
                    auto* row = new juce::DynamicObject();
                    row->setProperty ("index", index);
                    row->setProperty ("id", layer.id);
                    row->setProperty ("enabled", layer.enabled);
                    row->setProperty ("muted", layer.muted);
                    row->setProperty ("solo", layer.solo);
                    row->setProperty ("volume", layer.volume);
                    row->setProperty ("pan", layer.pan);
                    row->setProperty ("midiChannel", layer.midiChannel);
                    row->setProperty ("outputRoute", layer.outputRoute);
                    row->setProperty ("transposeSemitones", layer.transposeSemitones);
                    rows.add (juce::var (row));
                }
            }
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("layers", rows);
        return juce::JSON::toString (juce::var (root), false);
    }

    void PlayerProcessor::multiLayerRackFromJson (const juce::String& json)
    {
        if (json.trim().isEmpty())
            return;

        auto parsed = juce::JSON::parse (json);
        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
            return;

        auto* rows = root->getProperty ("layers").getArray();
        if (rows == nullptr)
            return;

        const juce::SpinLock::ScopedLockType lk (engineLock);
        auto* multi = dynamic_cast<MultiInstrumentEngine*> (engine.get());
        if (multi == nullptr)
            return;

        const auto& layers = multi->getLayers();
        for (const auto& item : *rows)
        {
            auto* row = item.getDynamicObject();
            if (row == nullptr)
                continue;

            const auto savedId = row->getProperty ("id").toString();
            int index = row->hasProperty ("index") ? (int) row->getProperty ("index") : -1;
            if (index < 0 || index >= (int) layers.size()
                || (savedId.isNotEmpty() && layers[(size_t) index].id != savedId))
            {
                index = -1;
                for (int candidate = 0; candidate < (int) layers.size(); ++candidate)
                    if (layers[(size_t) candidate].id == savedId)
                    {
                        index = candidate;
                        break;
                    }
            }

            if (index < 0 || index >= (int) layers.size())
                continue;

            const auto id = layers[(size_t) index].id;
            if (row->hasProperty ("enabled")) multi->setLayerEnabled (id, (bool) row->getProperty ("enabled"));
            if (row->hasProperty ("muted"))   multi->setLayerMute (id, (bool) row->getProperty ("muted"));
            if (row->hasProperty ("solo"))    multi->setLayerSolo (id, (bool) row->getProperty ("solo"));
            if (row->hasProperty ("volume"))  multi->setLayerVolume (id, (float) row->getProperty ("volume"));
            if (row->hasProperty ("pan"))     multi->setLayerPan (id, (float) row->getProperty ("pan"));
            if (row->hasProperty ("midiChannel"))
                multi->setLayerMidiChannel (id, (int) row->getProperty ("midiChannel"));
            if (row->hasProperty ("outputRoute"))
                multi->setLayerOutputRoute (id, (int) row->getProperty ("outputRoute"));
            if (row->hasProperty ("transposeSemitones"))
                multi->setLayerTransposeSemitones (id, (int) row->getProperty ("transposeSemitones"));
        }
    }

    bool PlayerProcessor::setUserContentTuneSemitones (const juce::String& contentId, int semitones)
    {
        bool found = false;
        {
            const juce::ScopedLock lock (userContentLock);
            for (auto& item : userContent)
            {
                if (item.id == contentId)
                {
                    item.tuneSemitones = semitones;
                    found = true;
                    break;
                }
            }
        }

        if (! found)
            return false;

        saveUserContentManifest();

        {
            const juce::SpinLock::ScopedLockType engineGuard (engineLock);
            rebuildRuntimeUserContentLocked (true);
        }

        editorListeners.call ([] (EditorListener& listener) { listener.packChanged(); });
        return true;
    }

    bool PlayerProcessor::setArpLaneMutedFromUi (int lane, bool muted)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        auto* midiBlock = findMidiPlaygroundBlock (pack.dspGraph);
        if (midiBlock == nullptr)
            return false;

        lane = juce::jlimit (0, 15, lane);
        setArpLaneValue (*midiBlock, lane, "mpLaneMute", muted ? 1.0f : 0.0f);
        
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

    bool PlayerProcessor::setArpLaneSoloFromUi (int lane, bool solo)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded)
            return false;

        auto* midiBlock = findMidiPlaygroundBlock (pack.dspGraph);
        if (midiBlock == nullptr)
            return false;

        lane = juce::jlimit (0, 15, lane);
        setArpLaneValue (*midiBlock, lane, "mpLaneSolo", solo ? 1.0f : 0.0f);
        
        arpeggiator.bind (pack.dspGraph);
        polySequencer.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
        return true;
    }

} // namespace patchcraft

// JUCE plugin entry point ---------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new patchcraft::PlayerProcessor();
}
