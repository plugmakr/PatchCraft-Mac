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
            int midiChannel = 0;       // 0 = omni, 1..16 = channel filtered
            int outputRoute = 0;       // 0 = main, 1..n = named internal route
            int transposeSemitones = 0;
            int autoPlayNote = 60;
            float autoPlayVelocity = 1.0f;
            bool enabled = true;
            bool muted = false;
            bool solo = false;
            bool autoPlayWithTransport = false;
            bool autoPlayHeld = false;
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
        void setLayerEnabled (const juce::String& id, bool enabled);
        void setLayerMidiChannel (const juce::String& id, int channel);
        void setLayerOutputRoute (const juce::String& id, int route);
        void setLayerTransposeSemitones (const juce::String& id, int semitones);
        void setLayerMute (const juce::String& id, bool muted);
        void setLayerSolo (const juce::String& id, bool solo);

        // Audio processing
        void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

        // Note routing
        void noteOn (int midiNote, float velocity) override;
        void noteOff (int midiNote) override;
        void noteOnForChannel (int midiNote, float velocity, int midiChannel);
        void noteOffForChannel (int midiNote, int midiChannel);
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
        bool isAnyLayerSoloed() const;

    private:
        std::vector<InstrumentLayer> layers;
        juce::CriticalSection layerLock;
        juce::AudioBuffer<float> layerScratch;
        double currentSampleRate = 44100.0;
        int currentBlockSize = 512;
        int currentNumChannels = 2;
        RenderContext renderContext;

        // Audio processing
        void processLayer (InstrumentLayer& layer, juce::AudioBuffer<float>& buffer, 
                           int startSample, int numSamples);
        void mixLayers (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
        void updateTransportTriggeredLayers();
        InstrumentLayer* findLayerById (const juce::String& id);
        bool isLayerAudible (const InstrumentLayer& layer, bool anySoloed) const noexcept;
        bool layerAcceptsMidiChannel (const InstrumentLayer& layer, int midiChannel) const noexcept;
    };
}
