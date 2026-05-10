#include "MultiInstrumentEngine.h"
#include "PatchCraftTypes.h"

namespace patchcraft
{
    MultiInstrumentEngine::MultiInstrumentEngine()
    {
    }

    MultiInstrumentEngine::~MultiInstrumentEngine()
    {
    }

    void MultiInstrumentEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = maxBlockSize;
        currentNumChannels = numChannels;
        
        // Prepare all layer engines
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->prepare (sampleRate, maxBlockSize, numChannels);
        }
    }

    void MultiInstrumentEngine::reset()
    {
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->reset();
        }
    }

    void MultiInstrumentEngine::setRenderContext (const RenderContext& context)
    {
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
        newLayer.muted = false;
        newLayer.solo = false;
        
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

    void MultiInstrumentEngine::setLayerMute (const juce::String& id, bool muted)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->muted = muted;
    }

    void MultiInstrumentEngine::setLayerSolo (const juce::String& id, bool solo)
    {
        juce::ScopedLock lock (layerLock);
        
        if (auto* layer = findLayerById (id))
            layer->solo = solo;
        
        updateLayerStates();
    }

    void MultiInstrumentEngine::noteOn (int midiNote, float velocity)
    {
        // Route note to all non-muted, non-soloed layers
        for (auto& layer : layers)
        {
            if (layer.engine && !layer.muted && (!layer.solo || isAnyLayerSoloed()))
                layer.engine->noteOn (midiNote, velocity);
        }
    }

    void MultiInstrumentEngine::noteOff (int midiNote)
    {
        // Send note off to all layers
        for (auto& layer : layers)
        {
            if (layer.engine && !layer.muted && (!layer.solo || isAnyLayerSoloed()))
                layer.engine->noteOff (midiNote);
        }
    }

    void MultiInstrumentEngine::allNotesOff()
    {
        for (auto& layer : layers)
        {
            if (layer.engine)
                layer.engine->allNotesOff();
        }
    }

    void MultiInstrumentEngine::setParameter (const juce::String& parameterId, float value)
    {
        // Route parameter to first matching layer
        for (auto& layer : layers)
        {
            if (layer.engine)
            {
                layer.engine->setParameter (parameterId, value);
                return; // First match wins
            }
        }
    }

    void MultiInstrumentEngine::loadFromPack (const juce::File& packFolder, const SampleMap& map)
    {
        // Load all instrument layers from pack
        juce::String manifestPath = packFolder.getChildFile ("manifest.json").getFullPathName();
        
        if (auto manifest = juce::JSON::parse (juce::File (manifestPath)))
        {
            if (auto* root = manifest.getDynamicObject())
            {
                // Check for multi-instrument mode
                if (auto* multiProp = root->getProperty ("multiInstrumentMode"))
                {
                    if (multiProp->isBool())
                    {
                        // Load multiple instruments
                        if (auto* instruments = root->getProperty ("instrumentIds"))
                        {
                            if (auto* ids = instruments->getArray())
                            {
                                for (int i = 0; i < ids->size(); ++i)
                                {
                                    juce::String instrumentId = ids->getString (i);
                                    juce::String instrumentName = "Layer " + juce::String (i + 1);
                                    
                                    // Load sample map for this instrument
                                    juce::String instrumentFolder = packFolder.getChildFile (instrumentId);
                                    SampleMap instrumentMap;
                                    instrumentMap.loadFromFolder (instrumentFolder);
                                    
                                    // Create layer
                                    addInstrumentLayer (instrumentId, instrumentName);
                                    
                                    // Load samples into the layer's engine
                                    if (layers.back().engine)
                                        layers.back().engine->loadFromPack (packFolder, instrumentMap);
                                }
                            }
                        }
                    }
                }
                else
                {
                    // Single instrument mode - load normally
                    addInstrumentLayer ("main", "Main");
                    if (layers.back().engine)
                        layers.back().engine->loadFromPack (packFolder, map);
                }
            }
        }
    }

    int MultiInstrumentEngine::getActiveVoiceCount() const noexcept
    {
        int total = 0;
        for (const auto& layer : layers)
        {
            if (layer.engine && !layer.muted && (!layer.solo || isAnyLayerSoloed()))
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
            status += "n" + juce::String (i + 1) + ": " + layer.name;
            if (layer.muted) status += " (MUTED)";
            if (layer.solo) status += " (SOLO)";
            if (layer.engine) status += " - " + layer.engine->getDiagnosticStatus();
        }
        
        return status;
    }

    void MultiInstrumentEngine::processLayer (InstrumentLayer& layer, juce::AudioBuffer<float>& buffer, 
                                       int startSample, int numSamples)
    {
        if (!layer.engine || layer.muted) return;
        
        // Create layer buffer
        juce::AudioBuffer<float> layerBuffer (buffer.getNumChannels(), buffer.getNumSamples());
        layerBuffer.clear();
        
        // Process layer
        layer.engine->process (layerBuffer, startSample, numSamples);
        
        // Apply layer volume and pan
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* layerData = layerBuffer.getWritePointer (ch);
            auto* outData = buffer.getWritePointer (ch);
            
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = layerData[i];
                
                // Apply pan (simple stereo pan)
                if (buffer.getNumChannels() >= 2)
                {
                    if (ch == 0)
                        sample *= (1.0f - layer.pan) * 0.5f; // Left
                    else
                        sample *= (1.0f + layer.pan) * 0.5f; // Right
                }
                
                // Apply volume
                sample *= layer.volume;
                
                // Mix to output
                outData[i] += sample;
            }
        }
    }

    void MultiInstrumentEngine::mixLayers (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        buffer.clear();
        
        // Process each layer and mix together
        for (auto& layer : layers)
        {
            if (layer.engine && !layer.muted && (!layer.solo || isAnyLayerSoloed()))
            {
                processLayer (layer, buffer, startSample, numSamples);
            }
        }
    }

    InstrumentLayer* MultiInstrumentEngine::findLayerById (const juce::String& id)
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

    void MultiInstrumentEngine::updateLayerStates()
    {
        // Update engine states based on solo/mute configuration
        for (auto& layer : layers)
        {
            if (layer.engine)
            {
                bool shouldBeActive = !layer.muted && (!layer.solo || isAnyLayerSoloed());
                // Note: We could add engine enable/disable here if needed
            }
        }
    }
}
