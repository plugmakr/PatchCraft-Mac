#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PatchCraftPackFormat.h"
#include "PatchCraftPackReader.h"
#include "IInstrumentEngine.h"
#include "DspRoutingEngine.h"
#include "MidiPlaygroundRuntime.h"
#include "PolyrhythmicSequencerRuntime.h"
#include "ComposerRuntime.h"
#include "PianoRollRuntime.h"
#include "SampleSynthEngine.h"
#include "PScriptEngine.h"
#include "LicenseValidator.h"

#include <array>
#include <map>

namespace patchcraft
{
    // Forward declaration to avoid circular dependencies
    class LibraryScanner;

    /**
        White-label Player VST3 instrument plugin. Loads a .patchcraft pack
        folder at runtime and renders the custom UI from the pack's layout.
    */
    class PlayerProcessor : public juce::AudioProcessor
    {
    public:
        /** @param skipEmbeddedPackAutoload When true, do not auto-load an embedded
            VST-bundled pack (used by Studio Brand Lab so the project layout wins). */
        explicit PlayerProcessor (bool skipEmbeddedPackAutoload = false);
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
            if (loadedPath.getFileName() == "EmbeddedPack"
                && pack.manifest.instrumentName.isNotEmpty())
            {
                return pack.manifest.instrumentName;
            }

           #if PATCHCRAFT_PLAYER_MIDI
            return "Composer";
           #elif PATCHCRAFT_PLAYER_FX
            return "Player FX";
           #else
            return "Player";
           #endif
        }
        bool acceptsMidi() const override                         { return true; }
        bool producesMidi() const override
        {
           #if PATCHCRAFT_PLAYER_MIDI
            return true;
           #else
            return false;
           #endif
        }
        bool isMidiEffect() const override
        {
           #if PATCHCRAFT_PLAYER_MIDI
            return true;
           #else
            return false;
           #endif
        }
        double getTailLengthSeconds() const override
        {
           #if PATCHCRAFT_PLAYER_MIDI
            return 0.0;
           #else
            return 4.0;
           #endif
        }

        int  getNumPrograms() override                             { return 1; }
        int  getCurrentProgram() override                          { return 0; }
        void setCurrentProgram (int) override                      {}
        const juce::String getProgramName (int) override           { return getName(); }
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
        // Per-band audio analysis for runtime audio-reactive visuals (0..1, message-thread read).
        float getAudioRms()       const noexcept { return analysisRms.load(); }
        float getAudioLowBand()   const noexcept { return analysisLow.load(); }
        float getAudioMidBand()   const noexcept { return analysisMid.load(); }
        float getAudioHighBand()  const noexcept { return analysisHigh.load(); }
        float getAudioTransient() const noexcept { return analysisTransient.load(); }
        float getAudioCentroid()  const noexcept { return analysisCentroid.load(); }
        // Returns the reactive level for an authored audioReactiveMode string.
        float getAudioReactiveSignal (const juce::String& mode) const noexcept;
        // Downsampled peaks (0..1) of the most recently dropped sample, for runtime
        // waveform display. Populated and read on the message thread only.
        const std::vector<float>& getUserWaveformPeaks() const noexcept { return userWaveformPeaks; }
        float getNoteHighlightLevel (int midiNote) const noexcept;
        bool decayNoteHighlightLevels() noexcept;
        bool isInternalTransportPlaying() const noexcept              { return internalTransportPlaying.load(); }
        void setInternalTransportPlaying (bool shouldPlay);
        void toggleInternalTransport();
        bool isAnyTransportPlaying() const;
        double getSequencerPlaybackPosition01 (int stepsPerCycle = 16) const;
        int getActiveVoiceCount() const;
        int getLoadedSampleCount() const;
        juce::String getEngineDiagnosticStatus() const;
        float getPackParameterValue (const juce::String& parameterId) const;
        int getHostParameterSlotIndex (const juce::String& parameterId) const;
        bool setPackParameterFromUi (const juce::String& parameterId, float value);
        bool setDrumPatternCellFromUi (int pattern, int track, int step, bool enabled,
                                       float velocity, float gate = 0.35f, float probability = 1.0f,
                                       int divisions = -1);
        bool setDrumActivePatternFromUi (int pattern);
        bool setDrumTrackMutedFromUi (int track, bool muted);
        bool setDrumTrackSoloFromUi (int track, bool solo);
        bool setDrumTrackNoteFromUi (int track, int midiNote);
        bool setDrumSongModeFromUi (bool songMode);
        bool setMidiPlaygroundActiveBankFromUi (int bank);
        bool setArpLaneStepFromUi (int lane, int step, float velocity, bool active);
        bool setArpLaneStepsFromUi (int lane, int steps);
        bool setSeqLaneStepFromUi (int laneIndex, int step, float value, bool active, const juce::String& laneType);
        bool setArpLaneMutedFromUi (int lane, bool muted);
        bool setArpLaneSoloFromUi (int lane, bool solo);

        // Runtime piano-roll editing. The encoded note list is "start,len,pitch,vel;..."
        juce::String getPianoRollNotesEncoded() const;
        bool setPianoRollNotesFromUi (const juce::String& encodedNotes);
        double getPianoRollPlaybackPosition01() const;
        bool allowsExternalPackLoading() const;
        bool applyPresetByIndex (int presetIndex);
        int getPresetCount() const;
        int getCurrentPresetIndex() const;
        juce::String getPresetName (int presetIndex) const;
        juce::StringArray getPresetNames() const;
        bool applyPresetOffset (int delta);
        void randomizeCurrentPreset();
        void restoreAllPresets();
        void setDefaultPreset();
        void saveAbSnapshot (int slot);
        void recallAbSnapshot (int slot);
        juce::String getAbSnapshotName (int slot) const;

        struct UserSnapshotInfo
        {
            juce::String id;
            juce::String name;
            juce::String notes;
            bool favorite = false;
            int sourcePresetIndex = -1;
            int parameterCount = 0;
        };
        std::vector<UserSnapshotInfo> getUserSnapshots() const;
        bool saveUserSnapshot (const juce::String& name, bool favorite);
        bool applyUserSnapshot (const juce::String& id);
        bool deleteUserSnapshot (const juce::String& id);
        bool toggleUserSnapshotFavorite (const juce::String& id);

        bool isMultiInstrumentPack() const;
        int getMultiLayerCount() const;
        juce::String getMultiLayerName (int index) const;
        int getMultiLayerActiveVoiceCount (int index) const;
        int getMultiLayerLoadedSampleCount (int index) const;
        bool getMultiLayerMuted (int index) const;
        bool getMultiLayerSoloed (int index) const;
        bool getMultiLayerEnabled (int index) const;
        int getMultiLayerMidiChannel (int index) const;
        int getMultiLayerOutputRoute (int index) const;
        int getMultiLayerTransposeSemitones (int index) const;
        float getMultiLayerVolume (int index) const;
        float getMultiLayerPan (int index) const;
        void setMultiLayerVolume (int index, float volume);
        void setMultiLayerPan (int index, float pan);
        void setMultiLayerMuted (int index, bool muted);
        void setMultiLayerSoloed (int index, bool solo);
        void setMultiLayerEnabled (int index, bool enabled);
        void setMultiLayerMidiChannel (int index, int channel);
        void setMultiLayerOutputRoute (int index, int route);
        void setMultiLayerTransposeSemitones (int index, int semitones);

        // Library scanner access
        LibraryScanner& getLibraryScanner() { return *libraryScanner; }

        // Runtime user imports. These are end-user overlays saved with the
        // host session and copied to the user's writable instrument folder.
        struct UserContentItem
        {
            juce::String id;
            juce::String kind;      // "sample" or "midi"
            juce::String name;
            juce::String filePath;
            juce::String role;      // "pads", "keyboard", "zone", "playground", or "zoneMidi"
            juce::String summary;
            int rootNote = 60;
            int lowNote = 0;
            int highNote = 127;
            int padIndex = -1;
            int noteCount = 0;
            double bpm = 120.0;
            int tuneSemitones = 0;
            juce::String midiMode { "trigger" };
            float midiVelocityAmount = 1.0f;
            bool midiStack = false;
        };
        std::vector<UserContentItem> getUserContentSnapshot() const;
        bool importUserContentFiles (const juce::StringArray& files,
                                     const juce::String& sampleMappingMode,
                                     juce::String& report,
                                     int targetNote = -1,
                                     int targetPadIndex = -1,
                                     bool midiStack = false);
        bool clearUserContent();
        bool applyUserMidiToPlayground (const juce::String& contentId, bool stack = false);
        bool setUserContentTuneSemitones (const juce::String& contentId, int semitones);
        juce::File getUserContentRoot() const;
        double getHostBpm() const;

        LicenseValidator::LicenseInfo getLicenseInfo() const;
        LicenseValidator::ActivationStatus getLicenseActivationStatus() const;
        void applyLicenseActivationStatus (const LicenseValidator::ActivationStatus& status);
        void refreshLicenseStatusFromCache();
        bool isLicenseAuthorized() const noexcept { return licenseAuthorized.load(); }
        juce::String getLicenseStatusText() const;

    private:
        juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
        void rebuildApvtsFromPack();
        void bindRoutingFromPack();
        void applyPianoRollOverrideLocked();
        // Translate ComposerRuntime-generated MIDI into engine note events.
        void sendComposerMidiToEngine (juce::MidiBuffer& buffer);
        RenderContext makeRenderContext (int numSamples) const;
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
        juce::String userSnapshotsToJson() const;
        void userSnapshotsFromJson (const juce::String& json);
        juce::String multiLayerRackToJson() const;
        void multiLayerRackFromJson (const juce::String& json);
        juce::String userContentToJson() const;
        void userContentFromJson (const juce::String& json);
        void saveUserContentManifest() const;
        void rebuildRuntimeUserContentLocked (bool reloadEngine);
        void applyAuthoredZoneMidiToGraphLocked();
        bool applyMidiContentToGraphLocked (const UserContentItem& item);
        static juce::String safeUserContentFileName (const juce::File& file);
        static bool isSupportedUserSampleFile (const juce::File& file);
        static bool isSupportedUserMidiFile (const juce::File& file);

        juce::AudioProcessorValueTreeState apvts;
        std::unique_ptr<IInstrumentEngine> engine;
        DspRoutingEngine routingEngine;
        MidiPlaygroundRuntime arpeggiator;
        PolyrhythmicSequencerRuntime polySequencer;
        PianoRollRuntime pianoRoll;
        SampleSynthEngine userSampleOverlay;
        mutable juce::SpinLock engineLock;
        double currentSampleRate = 44100.0;
        int    currentBlockSize  = 512;
        int    currentNumChans   = 2;

        PatchCraftPack pack;
        bool           loaded = false;
        juce::File     loadedPath;
        SampleMap      authoredSampleMap;

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
        std::atomic<float> analysisRms { 0.0f };
        std::atomic<float> analysisLow { 0.0f };
        std::atomic<float> analysisMid { 0.0f };
        std::atomic<float> analysisHigh { 0.0f };
        std::atomic<float> analysisTransient { 0.0f };
        std::atomic<float> analysisCentroid { 0.0f };
        std::array<std::atomic<float>, 128> noteHighlightLevels {};
        std::atomic<bool> internalTransportPlaying { false };
        std::atomic<double> internalTransportPpq { 0.0 };
        ComposerRuntime composerRuntime;
        juce::MidiBuffer composerMidi;          // scratch buffer for ComposerRuntime output
        juce::AudioBuffer<float> fxDryPassthroughBuffer;

        // Runtime override of the authored piano-roll note list, persisted with
        // the host session. Empty means "use the pack's authored notes".
        juce::String runtimePianoRollNotes;

        // Persistent MIDI overrides. The pitch and mod wheels only emit
        // events when their value changes, so we have to remember the
        // current state and re-apply it after each per-block parameter
        // sync (which would otherwise restore the user's static detune /
        // LFO depth and erase the bend).
        std::atomic<float> midiPitchBendCents { 0.0f };
        std::atomic<float> midiModWheel       { -1.0f }; // -1 means "untouched"

        // Library scanner for instrument browser
        std::unique_ptr<LibraryScanner> libraryScanner;
        PScriptEngine scriptEngine;
        LiveValueStore liveValues;

        // Generic floats backing parameters (mapped to pack's parameters by id at runtime).
        struct ParamSlot { juce::String id; std::atomic<float>* value = nullptr; };
        std::vector<ParamSlot> paramSlots;
        std::map<juce::String, int> hostSlotByParameterId;
        std::map<juce::String, float> runtimeParameterValues;
        std::map<juce::String, float> abSnapshotA;
        std::map<juce::String, float> abSnapshotB;
        std::map<juce::String, float> defaultPresetValues;
        struct UserSnapshot
        {
            juce::String id;
            juce::String name;
            juce::String notes;
            bool favorite = false;
            int sourcePresetIndex = -1;
            std::map<juce::String, float> values;
        };
        mutable juce::CriticalSection snapshotLock;
        std::vector<UserSnapshot> userSnapshots;
        mutable juce::CriticalSection userContentLock;
        std::vector<UserContentItem> userContent;
        bool userSampleOverlayEnabled = false;
        std::vector<float> userWaveformPeaks;
        void computeUserWaveformPeaks (const juce::File& file, int numBuckets = 256);
        mutable juce::CriticalSection licenseLock;
        LicenseValidator::ActivationStatus licenseStatus;
        std::atomic<bool> licenseAuthorized { true };
        bool bypassLicenseEnforcement = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerProcessor)
    };

} // namespace patchcraft
