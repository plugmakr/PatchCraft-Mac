#include "MultiInstrumentEngine.h"
#include "PatchCraftTypes.h"

#include <algorithm>

namespace patchcraft
{
    namespace
    {
        static juce::File firstExistingFile (const std::vector<juce::File>& candidates)
        {
            for (const auto& candidate : candidates)
                if (candidate.existsAsFile())
                    return candidate;

            return {};
        }

        static bool mapHasReachableSamples (const juce::File& root, const SampleMap& map)
        {
            if (! root.isDirectory())
                return false;

            const auto& zones = map.getZones();
            if (zones.empty())
                return true;

            for (const auto& zone : zones)
            {
                if (zone.samplePath.isEmpty())
                    continue;

                const auto sample = juce::File::isAbsolutePath (zone.samplePath)
                    ? juce::File (zone.samplePath)
                    : root.getChildFile (zone.samplePath);

                if (sample.existsAsFile())
                    return true;
            }

            return false;
        }

        static juce::File chooseSampleRoot (const juce::File& packFolder,
                                            const juce::File& preferredRoot,
                                            const SampleMap& map)
        {
            if (mapHasReachableSamples (preferredRoot, map))
                return preferredRoot;

            if (mapHasReachableSamples (packFolder, map))
                return packFolder;

            return preferredRoot.isDirectory() ? preferredRoot : packFolder;
        }

        static bool parseLayerScopedParameter (const juce::String& parameterId,
                                               int& layerIndex,
                                               juce::String& childParameter)
        {
            layerIndex = -1;
            childParameter.clear();

            if (! parameterId.startsWithIgnoreCase ("layer"))
                return false;

            const auto remainder = parameterId.substring (5);
            int digitCount = 0;
            while (digitCount < remainder.length()
                   && juce::CharacterFunctions::isDigit (remainder[digitCount]))
                ++digitCount;

            if (digitCount <= 0 || digitCount >= remainder.length()
                || remainder[digitCount] != '_')
                return false;

            layerIndex = remainder.substring (0, digitCount).getIntValue();
            childParameter = remainder.substring (digitCount + 1).trim();
            return childParameter.isNotEmpty();
        }
    }

    MultiInstrumentEngine::MultiInstrumentEngine()
    {
    }

    MultiInstrumentEngine::~MultiInstrumentEngine()
    {
    }

    void MultiInstrumentEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        juce::ScopedLock lock (layerLock);
        currentSampleRate = sampleRate;
        currentBlockSize = maxBlockSize;
        currentNumChannels = numChannels;
        layerScratch.setSize (juce::jmax (1, numChannels),
                              juce::jmax (1, maxBlockSize),
                              false, true, true);
        
        // Prepare all layer engines
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->prepare (sampleRate, maxBlockSize, numChannels);
        }
    }

    void MultiInstrumentEngine::reset()
    {
        juce::ScopedLock lock (layerLock);
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->reset();
            layer.autoPlayHeld = false;
        }
    }

    void MultiInstrumentEngine::setRenderContext (const RenderContext& context)
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        renderContext = context;
        
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->setRenderContext (context);
        }
    }

    void MultiInstrumentEngine::addInstrumentLayer (const juce::String& id, const juce::String& name)
    {
        juce::ScopedLock lock (layerLock);
        
        // Create new layer
        InstrumentLayer newLayer;
        newLayer.id = id;
        newLayer.name = name;
        newLayer.engine = std::make_unique<SampleSynthEngine>();
        newLayer.volume = 1.0f;
        newLayer.pan = 0.0f;
        newLayer.midiChannel = 0;
        newLayer.outputRoute = 0;
        newLayer.transposeSemitones = 0;
        newLayer.autoPlayNote = 60;
        newLayer.autoPlayVelocity = 1.0f;
        newLayer.enabled = true;
        newLayer.muted = false;
        newLayer.solo = false;
        newLayer.autoPlayWithTransport = false;
        newLayer.autoPlayHeld = false;
        newLayer.engine->prepare (currentSampleRate, currentBlockSize, currentNumChannels);
        newLayer.engine->setRenderContext (renderContext);
        
        layers.push_back (std::move (newLayer));
    }

    void MultiInstrumentEngine::removeInstrumentLayer (const juce::String& id)
    {
        juce::ScopedLock lock (layerLock);
        
        auto it = std::remove_if (layers.begin(), layers.end(),
            [&id] (const InstrumentLayer& layer) { return layer.id == id; });
        
        layers.erase (it, layers.end());
    }

    void MultiInstrumentEngine::setLayerVolume (const juce::String& id, float volume)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->volume = volume;
    }

    void MultiInstrumentEngine::setLayerPan (const juce::String& id, float pan)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->pan = pan;
    }

    void MultiInstrumentEngine::setLayerEnabled (const juce::String& id, bool enabled)
    {
        juce::ScopedLock lock (layerLock);

        if (auto* layer = findLayerById (id))
        {
            layer->enabled = enabled;
            if (! enabled && layer->engine)
            {
                layer->engine->reset();
                layer->autoPlayHeld = false;
            }
        }
    }

    void MultiInstrumentEngine::setLayerMidiChannel (const juce::String& id, int channel)
    {
        juce::ScopedLock lock (layerLock);

        if (auto* layer = findLayerById (id))
            layer->midiChannel = juce::jlimit (0, 16, channel);
    }

    void MultiInstrumentEngine::setLayerOutputRoute (const juce::String& id, int route)
    {
        juce::ScopedLock lock (layerLock);

        if (auto* layer = findLayerById (id))
            layer->outputRoute = juce::jlimit (0, 8, route);
    }

    void MultiInstrumentEngine::setLayerTransposeSemitones (const juce::String& id, int semitones)
    {
        juce::ScopedLock lock (layerLock);

        if (auto* layer = findLayerById (id))
            layer->transposeSemitones = juce::jlimit (-48, 48, semitones);
    }

    void MultiInstrumentEngine::setLayerMute (const juce::String& id, bool muted)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->muted = muted;

        const bool anySoloed = isAnyLayerSoloed();
        for (auto& layer : layers)
            if (layer.engine && ! isLayerAudible (layer, anySoloed))
            {
                layer.engine->reset();
                layer.autoPlayHeld = false;
            }
    }

    void MultiInstrumentEngine::setLayerSolo (const juce::String& id, bool solo)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->solo = solo;

        const bool anySoloed = isAnyLayerSoloed();
        for (auto& layer : layers)
            if (layer.engine && ! isLayerAudible (layer, anySoloed))
            {
                layer.engine->reset();
                layer.autoPlayHeld = false;
            }
    }

    void MultiInstrumentEngine::noteOn (int midiNote, float velocity)
    {
        noteOnForChannel (midiNote, velocity, 0);
    }

    void MultiInstrumentEngine::noteOff (int midiNote)
    {
        noteOffForChannel (midiNote, 0);
    }

    void MultiInstrumentEngine::noteOnForChannel (int midiNote, float velocity, int midiChannel)
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        const bool anySoloed = isAnyLayerSoloed();
        for (auto& layer : layers)
        {
            if (layer.engine && isLayerAudible (layer, anySoloed) && layerAcceptsMidiChannel (layer, midiChannel))
                layer.engine->noteOn (juce::jlimit (0, 127, midiNote + layer.transposeSemitones), velocity);
        }
    }

    void MultiInstrumentEngine::noteOffForChannel (int midiNote, int midiChannel)
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        for (auto& layer : layers)
        {
            if (layer.engine && layerAcceptsMidiChannel (layer, midiChannel))
                layer.engine->noteOff (juce::jlimit (0, 127, midiNote + layer.transposeSemitones));
        }
    }

    void MultiInstrumentEngine::allNotesOff()
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->allNotesOff();
            layer.autoPlayHeld = false;
        }
    }

    void MultiInstrumentEngine::setParameter (const juce::String& parameterId, float value)
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        int scopedLayerIndex = -1;
        juce::String childParameter;
        if (parseLayerScopedParameter (parameterId, scopedLayerIndex, childParameter))
        {
            if (scopedLayerIndex >= 0 && scopedLayerIndex < (int) layers.size())
            {
                auto& layer = layers[(size_t) scopedLayerIndex];
                if (childParameter == "mixVolume")
                    layer.volume = juce::jlimit (0.0f, 2.0f, value);
                else if (childParameter == "mixPan")
                    layer.pan = juce::jlimit (-1.0f, 1.0f, value);
                else if (childParameter == "mute")
                    layer.muted = value >= 0.5f;
                else if (childParameter == "solo")
                    layer.solo = value >= 0.5f;
                else if (childParameter == "enabled")
                    layer.enabled = value >= 0.5f;
                else if (childParameter == "transpose")
                    layer.transposeSemitones = juce::jlimit (-48, 48, juce::roundToInt (value));
                else if (layer.engine)
                    layer.engine->setParameter (childParameter, value);

                if (layer.engine && (! layer.enabled || layer.muted))
                {
                    layer.engine->allNotesOff();
                    layer.autoPlayHeld = false;
                }
            }
            return;
        }

        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->setParameter (parameterId, value);
        }
    }

    void MultiInstrumentEngine::loadFromPack (const juce::File& packFolder, const SampleMap& map)
    {
        juce::ScopedLock lock (layerLock);
        layers.clear();
        
        const auto manifest = juce::JSON::parse (packFolder.getChildFile ("manifest.json"));
        bool loadedAnyLayer = false;
        if (auto* root = manifest.getDynamicObject())
        {
            const auto multiProp = root->getProperty ("multiInstrumentMode");
            if (multiProp.isBool() && (bool) multiProp)
            {
                auto* ids = root->getProperty ("instrumentIds").getArray();
                auto* names = root->getProperty ("instrumentNames").getArray();
                auto* files = root->getProperty ("instrumentFiles").getArray();
                auto* volumes = root->getProperty ("instrumentVolumes").getArray();
                auto* pans = root->getProperty ("instrumentPans").getArray();
                auto* midiChannels = root->getProperty ("instrumentMidiChannels").getArray();
                auto* outputRoutes = root->getProperty ("instrumentOutputRoutes").getArray();
                auto* transposes = root->getProperty ("instrumentTransposeSemitones").getArray();
                auto* enabledStates = root->getProperty ("instrumentEnabled").getArray();
                auto* autoPlayStates = root->getProperty ("instrumentAutoPlay").getArray();
                auto* autoPlayNotes = root->getProperty ("instrumentAutoPlayNotes").getArray();
                auto* autoPlayVelocities = root->getProperty ("instrumentAutoPlayVelocities").getArray();

                if (ids != nullptr)
                {
                    for (int i = 0; i < ids->size(); ++i)
                    {
                        const auto instrumentId = (*ids)[i].toString().trim();
                        if (instrumentId.isEmpty())
                            continue;

                        const auto instrumentFolder = packFolder.getChildFile (instrumentId);
                        std::vector<juce::File> mappingCandidates;
                        if (files != nullptr && i < files->size())
                        {
                            const auto fileRef = (*files)[i].toString().trim();
                            if (fileRef.isNotEmpty())
                                mappingCandidates.push_back (juce::File::isAbsolutePath (fileRef)
                                    ? juce::File (fileRef)
                                    : packFolder.getChildFile (fileRef));
                        }
                        mappingCandidates.push_back (instrumentFolder.getChildFile ("mappings.json"));
                        mappingCandidates.push_back (packFolder.getChildFile ("instruments")
                                                               .getChildFile (instrumentId + ".json"));
                        mappingCandidates.push_back (packFolder.getChildFile ("instruments")
                                                               .getChildFile (instrumentId)
                                                               .getChildFile ("mappings.json"));
                        mappingCandidates.push_back (packFolder.getChildFile (instrumentId + ".json"));

                        const auto mappingFile = firstExistingFile (mappingCandidates);
                        SampleMap instrumentMap;
                        if (mappingFile.existsAsFile())
                            instrumentMap.fromVar (juce::JSON::parse (mappingFile));
                        else
                            instrumentMap = map;

                        const auto mapRoot = mappingFile.existsAsFile()
                            ? chooseSampleRoot (packFolder, mappingFile.getParentDirectory(), instrumentMap)
                            : chooseSampleRoot (packFolder, instrumentFolder, instrumentMap);

                        InstrumentLayer layer;
                        layer.id = instrumentId;
                        layer.name = names != nullptr && i < names->size()
                            ? (*names)[i].toString().trim()
                            : "Layer " + juce::String (i + 1);
                        if (layer.name.isEmpty())
                            layer.name = "Layer " + juce::String (i + 1);
                        if (volumes != nullptr && i < volumes->size())
                            layer.volume = juce::jlimit (0.0f, 2.0f, (float) (*volumes)[i]);
                        if (pans != nullptr && i < pans->size())
                            layer.pan = juce::jlimit (-1.0f, 1.0f, (float) (*pans)[i]);
                        if (midiChannels != nullptr && i < midiChannels->size())
                            layer.midiChannel = juce::jlimit (0, 16, (int) (*midiChannels)[i]);
                        if (outputRoutes != nullptr && i < outputRoutes->size())
                            layer.outputRoute = juce::jlimit (0, 8, (int) (*outputRoutes)[i]);
                        if (transposes != nullptr && i < transposes->size())
                            layer.transposeSemitones = juce::jlimit (-48, 48, (int) (*transposes)[i]);
                        if (enabledStates != nullptr && i < enabledStates->size())
                            layer.enabled = (bool) (*enabledStates)[i];
                        if (autoPlayStates != nullptr && i < autoPlayStates->size())
                            layer.autoPlayWithTransport = (bool) (*autoPlayStates)[i];
                        if (autoPlayNotes != nullptr && i < autoPlayNotes->size())
                            layer.autoPlayNote = juce::jlimit (0, 127, (int) (*autoPlayNotes)[i]);
                        if (autoPlayVelocities != nullptr && i < autoPlayVelocities->size())
                            layer.autoPlayVelocity = juce::jlimit (0.0f, 1.0f, (float) (*autoPlayVelocities)[i]);
                        layer.engine = std::make_unique<SampleSynthEngine>();
                        layer.engine->prepare (currentSampleRate, currentBlockSize, currentNumChannels);
                        layer.engine->setRenderContext (renderContext);
                        layer.engine->loadFromPack (mapRoot, instrumentMap);
                        layers.push_back (std::move (layer));
                        loadedAnyLayer = true;
                    }
                }
            }
        }

        if (! loadedAnyLayer)
        {
            InstrumentLayer layer;
            layer.id = "main";
            layer.name = "Main";
            layer.engine = std::make_unique<SampleSynthEngine>();
            layer.engine->prepare (currentSampleRate, currentBlockSize, currentNumChannels);
            layer.engine->setRenderContext (renderContext);
            layer.engine->loadFromPack (packFolder, map);
            layers.push_back (std::move (layer));
        }
    }

    int MultiInstrumentEngine::getActiveVoiceCount() const noexcept
    {
        int total = 0;
        const bool anySoloed = isAnyLayerSoloed();
        for (const auto& layer : layers)
        {
            if (layer.engine && isLayerAudible (layer, anySoloed))
                total += layer.engine->getActiveVoiceCount();
        }
        return total;
    }

    int MultiInstrumentEngine::getLoadedSampleCount() const noexcept
    {
        int total = 0;
        for (const auto& layer : layers)
        {
            if (layer.engine)
                total += layer.engine->getLoadedSampleCount();
        }
        return total;
    }

    juce::String MultiInstrumentEngine::getDiagnosticStatus() const
    {
        juce::String status = "Layers: " + juce::String (layers.size());
        
        for (int i = 0; i < layers.size(); ++i)
        {
            const auto& layer = layers[i];
            status += "\n" + juce::String (i + 1) + ": " + layer.name;
            status += " [" + juce::String (layer.enabled ? "ON" : "OFF") + "]";
            status += " CH " + juce::String (layer.midiChannel == 0 ? "Omni" : juce::String (layer.midiChannel));
            status += " OUT " + juce::String (layer.outputRoute == 0 ? "Main" : "Aux " + juce::String (layer.outputRoute));
            if (layer.muted) status += " (MUTED)";
            if (layer.solo) status += " (SOLO)";
            if (layer.engine) status += " - " + layer.engine->getDiagnosticStatus();
        }
        
        return status;
    }

    void MultiInstrumentEngine::processLayer (InstrumentLayer& layer, juce::AudioBuffer<float>& buffer, 
                                       int startSample, int numSamples)
    {
        if (! layer.engine || ! layer.enabled || layer.muted)
            return;
        
        const int safeSamples = juce::jmin (numSamples, buffer.getNumSamples() - startSample);
        if (safeSamples <= 0)
            return;

        const int bufferChannels = buffer.getNumChannels();
        const int scratchChannels = layerScratch.getNumChannels();
        if (bufferChannels <= 0 || scratchChannels <= 0 || layerScratch.getNumSamples() < safeSamples)
            return;

        layerScratch.clear (0, safeSamples);
        
        layer.engine->process (layerScratch, 0, safeSamples);

        int routeOffset = juce::jmax (0, layer.outputRoute) * 2;
        if (routeOffset >= bufferChannels)
            routeOffset = 0;
        const int routeChannels = juce::jmin (2, bufferChannels - routeOffset);

        for (int ch = 0; ch < routeChannels; ++ch)
        {
            const int sourceChannel = juce::jmin (ch, scratchChannels - 1);
            const auto* layerData = layerScratch.getReadPointer (sourceChannel);
            auto* outData = buffer.getWritePointer (routeOffset + ch, startSample);
            
            for (int i = 0; i < safeSamples; ++i)
            {
                float sample = layerData[i];
                
                if (routeChannels >= 2)
                {
                    if (ch == 0)
                        sample *= layer.pan <= 0.0f ? 1.0f : 1.0f - juce::jlimit (0.0f, 1.0f, layer.pan);
                    else
                        sample *= layer.pan >= 0.0f ? 1.0f : 1.0f + juce::jlimit (-1.0f, 0.0f, layer.pan);
                }
                
                outData[i] += sample * juce::jlimit (0.0f, 2.0f, layer.volume);
            }
        }
    }

    void MultiInstrumentEngine::mixLayers (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        const bool anySoloed = isAnyLayerSoloed();
        for (auto& layer : layers)
        {
            if (layer.engine && isLayerAudible (layer, anySoloed))
                processLayer (layer, buffer, startSample, numSamples);
        }
    }

    void MultiInstrumentEngine::updateTransportTriggeredLayers()
    {
        const bool anySoloed = isAnyLayerSoloed();
        for (auto& layer : layers)
        {
            if (! layer.engine || ! layer.autoPlayWithTransport)
                continue;

            const bool shouldPlay = renderContext.isPlaying && isLayerAudible (layer, anySoloed);
            const int note = juce::jlimit (0, 127, layer.autoPlayNote);
            if (shouldPlay && ! layer.autoPlayHeld)
            {
                layer.engine->noteOn (note, juce::jlimit (0.0f, 1.0f, layer.autoPlayVelocity));
                layer.autoPlayHeld = true;
            }
            else if (! shouldPlay && layer.autoPlayHeld)
            {
                layer.engine->noteOff (note);
                layer.autoPlayHeld = false;
            }
        }
    }

    MultiInstrumentEngine::InstrumentLayer* MultiInstrumentEngine::findLayerById (const juce::String& id)
    {
        for (auto& layer : layers)
        {
            if (layer.id == id)
                return &layer;
        }
        return nullptr;
    }

    bool MultiInstrumentEngine::isAnyLayerSoloed() const
    {
        for (const auto& layer : layers)
        {
            if (layer.solo)
                return true;
        }
        return false;
    }

    bool MultiInstrumentEngine::isLayerAudible (const InstrumentLayer& layer, bool anySoloed) const noexcept
    {
        return layer.enabled && ! layer.muted && (! anySoloed || layer.solo);
    }

    bool MultiInstrumentEngine::layerAcceptsMidiChannel (const InstrumentLayer& layer, int midiChannel) const noexcept
    {
        return layer.midiChannel <= 0 || midiChannel <= 0 || layer.midiChannel == midiChannel;
    }

    void MultiInstrumentEngine::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        const juce::CriticalSection::ScopedTryLockType lock (layerLock);
        if (! lock.isLocked())
            return;

        const int safeStart = juce::jlimit (0, buffer.getNumSamples(), startSample);
        int remaining = juce::jmin (numSamples, buffer.getNumSamples() - safeStart);
        int offset = safeStart;
        const int scratchSamples = layerScratch.getNumSamples();
        if (scratchSamples <= 0)
            return;

        updateTransportTriggeredLayers();

        while (remaining > 0)
        {
            const int chunk = juce::jmin (remaining, scratchSamples);
            mixLayers (buffer, offset, chunk);
            offset += chunk;
            remaining -= chunk;
        }
    }
}
