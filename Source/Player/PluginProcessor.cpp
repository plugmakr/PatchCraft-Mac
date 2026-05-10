#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EngineFactory.h"
#include "InstrumentTemplates.h"
#include "LibraryScanner.h"

#include <algorithm>

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

    juce::AudioProcessorValueTreeState::ParameterLayout PlayerProcessor::createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        // Generic float slots; the pack maps its parameter ids to slots at load time.
        for (int i = 0; i < kPatchCraftHostParameterSlots; ++i)
        {
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { slotId (i), 1 },
                juce::String ("PatchCraft Slot ") + juce::String (i + 1),
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
                0.0f));
        }
        return { params.begin(), params.end() };
    }

    PlayerProcessor::PlayerProcessor()
        : juce::AudioProcessor (
           #if PATCHCRAFT_PLAYER_FX
            BusesProperties()
                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
           #else
            BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
           #endif
            ),
          apvts (*this, nullptr, "PatchCraft", createLayout())
    {
        try
        {
            for (int i = 0; i < kPatchCraftHostParameterSlots; ++i)
            {
                ParamSlot s;
                s.id    = slotId (i);
                s.value = apvts.getRawParameterValue (s.id);
                paramSlots.push_back (s);
            }

            // Initialize library scanner
            libraryScanner = std::make_unique<LibraryScanner>();

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
            bindRoutingFromPack();
            rebuildApvtsFromPack();
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
                engine->reset();
            }
        }
        catch (...) { jassertfalse; }
    }

    bool PlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::stereo()
            && out != juce::AudioChannelSet::mono())
            return false;
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

        // Note: try/catch here is paranoid - the audio thread shouldn't throw,
        // but FL's plugin scanner has been known to call processBlock during
        // scan with weird buffer sizes. Cheaper than a host crash.
        try
        {
            const juce::SpinLock::ScopedTryLockType lk (engineLock);
            if (! lk.isLocked() || engine == nullptr)
            {
                buffer.clear();
                return;
            }

            const int inputChannels = getTotalNumInputChannels();
            const int outputChannels = getTotalNumOutputChannels();
            const auto context = makeRenderContext (buffer.getNumSamples());

            // Source/synth engines start with a clean buffer; FX engines read
            // input audio from the buffer and normalize mono input to stereo output.
            if (! engine->needsAudioInput())
            {
                buffer.clear();
            }
            else
            {
                if (inputChannels <= 0)
                {
                    buffer.clear();
                    return;
                }

                for (int ch = inputChannels; ch < outputChannels && ch < buffer.getNumChannels(); ++ch)
                {
                    const int sourceChannel = inputChannels == 1 ? 0 : ch % inputChannels;
                    buffer.copyFrom (ch, 0, buffer, sourceChannel, 0, buffer.getNumSamples());
                }

                for (int ch = outputChannels; ch < buffer.getNumChannels(); ++ch)
                    buffer.clear (ch, 0, buffer.getNumSamples());
            }

            // Apply current APVTS values (slot index -> pack parameter index).
            if (loaded)
            {
                const auto& defs = pack.parameters.getAll();
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
                        runtimeParameterValues[def.id] = v;
                    }

                    engine->setParameter (def.id, v);
                    routingEngine.setParameterValue (def.id, v);
                }
            }

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

            arpeggiator.process (*engine, context);
            engine->process (buffer, 0, buffer.getNumSamples());
            routingEngine.captureAudioAnalysis (buffer, 0, buffer.getNumSamples());

            float peak = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
            const auto previous = outputPeak.load();
            outputPeak.store (juce::jmax (peak, previous * 0.82f));
        }
        catch (...) { buffer.clear(); jassertfalse; }
    }

    juce::AudioProcessorEditor* PlayerProcessor::createEditor()
    {
        return new PlayerEditor (*this);
    }

    void PlayerProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        try
        {
            auto state = apvts.copyState();
            state.setProperty ("packPath", loadedPath.getFullPathName(), nullptr);
            state.setProperty ("userMidiMappingsJson", midiMappingsToJson(), nullptr);
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
            const auto path = state.getProperty ("packPath").toString();
            if (path.isEmpty() || ! juce::File::isAbsolutePath (path)) return;

            const juce::File folder (path);
            if (! folder.isDirectory()) return;     // pack moved or deleted

            // Synchronous load - simpler and avoids any cross-thread issue
            // with the host's plugin scan environment. loadPack itself is
            // wrapped in try/catch.
            juce::String error;
            loadPack (folder, error);
            midiMappingsFromJson (userMidiMappingsJson);
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

    int PlayerProcessor::getHostParameterSlotIndex (const juce::String& parameterId) const
    {
        if (parameterId.isEmpty())
            return -1;
        const auto it = hostSlotByParameterId.find (parameterId);
        return it != hostSlotByParameterId.end() ? it->second : -1;
    }

    bool PlayerProcessor::loadPack (const juce::File& packFolder, juce::String& error)
    {
        bool ok = false;
        try
        {
            PatchCraftPackReader reader;
            PatchCraftPack np;
            if (! reader.read (packFolder, np, error)) return false;
            if (const auto* patch = np.findDefaultPatch())
                applyPatchStateToPack (np, *patch);

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
                heldNotes.fill (false);
                sustainPedalDown = false;
            }
            bindRoutingFromPack();
            rebuildApvtsFromPack();
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
            heldNotes.fill (false);
            sustainPedalDown = false;
            if (engine) engine->allNotesOff();
        }
        bindRoutingFromPack();
        rebuildApvtsFromPack();
        editorListeners.call ([] (EditorListener& l) { l.packChanged(); });
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
    }

    void PlayerProcessor::bindRoutingFromPack()
    {
        if (engine)
            arpeggiator.allNotesOff (*engine);
        arpeggiator.bind (pack.dspGraph);
        routingEngine.bind (pack.dspGraph, pack.parameters);
        routingEngine.prepare (makeRenderContext (currentBlockSize));
    }

    RenderContext PlayerProcessor::makeRenderContext (int numSamples) const
    {
        auto context = RenderContext::forBlock (currentSampleRate,
                                               numSamples,
                                               currentBlockSize,
                                               getTotalNumInputChannels(),
                                               getTotalNumOutputChannels(),
                                               120.0);

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
        routingEngine.setParameterValue (parameterId, limited);
        return true;
    }

    bool PlayerProcessor::setPackParameterFromUi (const juce::String& parameterId, float value)
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        return setPackParameterValue (parameterId, value, true);
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

    void PlayerProcessor::randomizeCurrentPreset()
    {
        const juce::SpinLock::ScopedLockType lk (engineLock);
        if (! loaded) return;
        juce::Random rng (static_cast<int> (juce::Time::getCurrentTime().toMilliseconds() & 0x7fffffff));
        for (const auto& def : pack.parameters.getAll())
        {
            if (! def.modulatable) continue;
            const float v = def.min + rng.nextFloat() * (def.max - def.min);
            setPackParameterValue (def.id, v, true);
        }
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
            heldNotes[(size_t) note] = false;
            if (! arpeggiator.handleNoteOn (*engine, note, message.getFloatVelocity()))
                engine->noteOn (note, message.getFloatVelocity());
            return;
        }

        if (message.isNoteOff())
        {
            const int note = juce::jlimit (0, 127, message.getNoteNumber());
            if (sustainPedalDown)
                heldNotes[(size_t) note] = true;
            else if (! arpeggiator.handleNoteOff (*engine, note))
                engine->noteOff (note);
            return;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            arpeggiator.allNotesOff (*engine);
            engine->allNotesOff();
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
            setPackParameterValue ("pitchWheel", bend, true);
            const float baseDetune = getCurrentPackParameterValue ("detune");
            setEngineParameterIfPresent ("detune", baseDetune + bend * 200.0f);
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
                setPackParameterValue ("modWheel", normalised, true);
                setPackParameterValue ("lfoAmount", normalised, true);
                setPackParameterValue ("vibratoDepth", normalised, true);
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
                    if (! arpeggiator.handleNoteOff (*engine, note))
                        engine->noteOff (note);
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

} // namespace patchcraft

// JUCE plugin entry point ---------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new patchcraft::PlayerProcessor();
}
