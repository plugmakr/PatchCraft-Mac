#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "IInstrumentEngine.h"
#include "SampleSynthEngine.h"
#include "PatchCraftTypes.h"

#include <memory>
#include <vector>
#include <atomic>

namespace patchcraft
{
    /**
        Multi-instrument engine that layers multiple SampleSynthEngine instances
        to create combined sounds. Each instrument can be independently
        controlled and mixed together with configurable routing and effects.
    */
    class MultiInstrumentEngine : public IInstrumentEngine
    {
    public:
        struct InstrumentLayer
        {
            juce::String id;
            juce::String name;
            std::unique_ptr<SampleSynthEngine> engine;
            float volume = 1.0f;
            float pan = 0.0f;
            bool muted = false;
            bool solo = false;
        };

        MultiInstrumentEngine();
        ~MultiInstrumentEngine() override;

        const char* engineId() const override { return "multi"; }
        bool needsAudioInput() const override { return false; }

        void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
        void reset() override;
        void setRenderContext (const RenderContext& context) override;

        // Layer management
        void addInstrumentLayer (const juce::String& id, const juce::String& name);
        void removeInstrumentLayer (const juce::String& id);
        void setLayerVolume (const juce::String& id, float volume);
        void setLayerPan (const juce::String& id, float pan);
        void setLayerMute (const juce::String& id, bool muted);
        void setLayerSolo (const juce::String& id, bool solo);

        // Note routing
        void noteOn (int midiNote, float velocity) override;
        void noteOff (int midiNote) override;
        void allNotesOff() override;

        // Parameter control
        void setParameter (const juce::String& parameterId, float value) override;

        // Pack loading
        void loadFromPack (const juce::File& packFolder, const SampleMap& map) override;

        // Status
        int getActiveVoiceCount() const noexcept override;
        int getLoadedSampleCount() const noexcept override;
        juce::String getDiagnosticStatus() const override;

        // Layer access
        const std::vector<InstrumentLayer>& getLayers() const noexcept { return layers; }
        int getLayerCount() const noexcept { return (int) layers.size(); }

    private:
        std::vector<InstrumentLayer> layers;
        juce::CriticalSection layerLock;
        double currentSampleRate = 44100.0;
        int currentBlockSize = 512;
        int currentNumChannels = 2;
        RenderContext renderContext;

        // Audio processing
        void processLayer (InstrumentLayer& layer, juce::AudioBuffer<float>& buffer, 
                           int startSample, int numSamples);
        void mixLayers (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
        InstrumentLayer* findLayerById (const juce::String& id);
        void updateLayerStates();
    };
}
