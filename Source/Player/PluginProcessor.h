#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PatchCraftPackFormat.h"
#include "PatchCraftPackReader.h"
#include "IInstrumentEngine.h"
#include "DspRoutingEngine.h"
#include "MidiPlaygroundRuntime.h"

#include <array>
#include <map>

namespace patchcraft
{
    // Forward declaration to avoid circular dependencies
    class LibraryScanner;

    /**
        PatchCraft Player VST3 instrument plugin. Loads a .patchcraft pack
        folder at runtime and renders the custom UI from the pack's layout.
    */
    class PlayerProcessor : public juce::AudioProcessor
    {
    public:
        PlayerProcessor();
        ~PlayerProcessor() override;

        // AudioProcessor
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout&) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override                            { return true; }

        const juce::String getName() const override
        {
           #if PATCHCRAFT_PLAYER_FX
            return "PatchCraft Player FX";
           #else
            return "PatchCraft Player";
           #endif
        }
        bool acceptsMidi() const override                         { return true; }
        bool producesMidi() const override                         { return false; }
        bool isMidiEffect() const override                         { return false; }
        double getTailLengthSeconds() const override               { return 4.0; }

        int  getNumPrograms() override                             { return 1; }
        int  getCurrentProgram() override                          { return 0; }
        void setCurrentProgram (int) override                      {}
        const juce::String getProgramName (int) override           { return "PatchCraft Player"; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        // ---- Pack lifecycle (message thread) -------------------------------
        bool loadPack (const juce::File& packFolder, juce::String& error);
        void unloadPack();

        const PatchCraftPack* getPack() const                      { return loaded ? &pack : nullptr; }
        juce::File            getLoadedPackPath() const            { return loadedPath; }
        bool                  isPackLoaded() const                 { return loaded; }

        // APVTS exposed for editor binding.
        juce::AudioProcessorValueTreeState& getApvts()             { return apvts; }

        // Listener interface used by the editor.
        struct EditorListener
        {
            virtual ~EditorListener() = default;
            virtual void packChanged() = 0;
        };
        void addEditorListener (EditorListener* l)                 { editorListeners.add (l); }
        void removeEditorListener (EditorListener* l)              { editorListeners.remove (l); }

        // Direct MIDI input from UI (e.g., virtual keyboard, drum pads)
        void handleNoteOn (int midiNote);
        void handleNoteOn (int midiNote, float velocity);
        void handleNoteOff (int midiNote);

        // Runtime MIDI learn in the exported Player.
        void beginMidiLearn (juce::String parameterId);
        void clearMidiLearn();
        void removeMidiMappingForParameter (const juce::String& parameterId);
        juce::String getMidiMappingSummary (const juce::String& parameterId) const;
        juce::String getPendingMidiLearnParameter() const;
        bool isParameterMidiLearnable (const juce::String& parameterId) const;
        float getOutputPeak() const noexcept                         { return outputPeak.load(); }
        float getPackParameterValue (const juce::String& parameterId) const;
        int getHostParameterSlotIndex (const juce::String& parameterId) const;
        bool setPackParameterFromUi (const juce::String& parameterId, float value);
        bool applyPresetByIndex (int presetIndex);
        void randomizeCurrentPreset();
        void saveAbSnapshot (int slot);
        void recallAbSnapshot (int slot);
        juce::String getAbSnapshotName (int slot) const;

        // Library scanner access
        LibraryScanner& getLibraryScanner() { return *libraryScanner; }

    private:
        juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
        void rebuildApvtsFromPack();
        void bindRoutingFromPack();
        RenderContext makeRenderContext (int numSamples) const;
        double getHostBpm() const;
        float getCurrentPackParameterValue (const juce::String& parameterId) const;
        int getParameterSlotIndex (const juce::String& parameterId) const;
        bool setEngineParameterIfPresent (const juce::String& parameterId, float value);
        bool setPackParameterValue (const juce::String& parameterId, float value, bool notifyHost);
        bool captureMidiLearnMessage (const juce::MidiMessage& message);
        bool applyMidiMappings (const juce::MidiMessage& message);
        void handleRealtimeMidi (const juce::MidiMessage& message);
        void setSustainPedal (bool down);
        juce::String midiMappingsToJson() const;
        void midiMappingsFromJson (const juce::String& json);

        juce::AudioProcessorValueTreeState apvts;
        std::unique_ptr<IInstrumentEngine> engine;
        DspRoutingEngine routingEngine;
        MidiPlaygroundRuntime arpeggiator;
        mutable juce::SpinLock engineLock;
        double currentSampleRate = 44100.0;
        int    currentBlockSize  = 512;
        int    currentNumChans   = 2;

        PatchCraftPack pack;
        bool           loaded = false;
        juce::File     loadedPath;

        juce::ListenerList<EditorListener> editorListeners;

        // MIDI events from UI (virtual keyboard) - added to buffer in audio thread
        juce::MidiBuffer uiMidiBuffer;
        juce::SpinLock uiMidiLock;
        mutable juce::SpinLock midiMappingLock;
        juce::String pendingMidiLearnParameter;
        std::vector<MidiMapping> userMidiMappings;
        std::array<bool, 128> heldNotes {};
        bool sustainPedalDown = false;
        std::atomic<float> outputPeak { 0.0f };

        // Library scanner for instrument browser
        std::unique_ptr<LibraryScanner> libraryScanner;

        // Generic floats backing parameters (mapped to pack's parameters by id at runtime).
        struct ParamSlot { juce::String id; std::atomic<float>* value = nullptr; };
        std::vector<ParamSlot> paramSlots;
        std::map<juce::String, int> hostSlotByParameterId;
        std::map<juce::String, float> runtimeParameterValues;
        std::map<juce::String, float> abSnapshotA;
        std::map<juce::String, float> abSnapshotB;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerProcessor)
    };

} // namespace patchcraft
