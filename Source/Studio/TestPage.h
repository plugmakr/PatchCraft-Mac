#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PatchCraftProject.h"
#include "AssetManager.h"
#include "IInstrumentEngine.h"
#include "DspRoutingEngine.h"
#include "MidiPlaygroundRuntime.h"

#include <map>

namespace patchcraft
{
    class StudioMainComponent;
    class StudioInstrumentRenderer;

    /**
        DAW-link test environment. Hosts the same IInstrumentEngine the Player
        will run, exercises it with the same audio/MIDI plumbing a DAW provides:

        - Transport: play / stop / record / loop / tempo
        - MIDI clip recorder + visualiser (piano-roll style)
        - Live MIDI keyboard + external MIDI input
        - Output level meters (L/R peak with dB readout)
        - Live parameter monitor

        When this page is active and "Play" is engaged, audio runs through the
        shared StudioAudioService device. Switching pages or pressing Stop
        suspends audio so the Studio doesn't fight other previews.
    */
    class TestPage : public juce::Component,
                     public juce::AudioIODeviceCallback,
                     public juce::MidiInputCallback,
                     private juce::MidiKeyboardStateListener,
                     public juce::KeyListener,
                     public juce::Timer,
                     private LiveValueStore::Listener,
                     private PatchCraftProject::Listener
    {
    public:
        explicit TestPage (StudioMainComponent& owner);
        ~TestPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        // Page lifecycle
        void activate();    // called when user switches TO this tab
        void deactivate();  // called when user switches AWAY
        bool isAudioRunning() const noexcept { return audioRunning; }
        void setBrandLabPreviewMode (bool shouldUse);
        juce::Rectangle<int> getBrandPreviewInstrumentBounds() const noexcept { return brandPreviewInstrumentBounds; }
        void showMidiClipEditor();
        void startPreviewPlayback();
        void stopPreviewPlayback();
        void togglePreviewPlayback();
        bool isTransportPlaying() const noexcept { return playing.load(); }
        double getSequencerPlaybackPosition01 (int steps) const noexcept;
        bool setDrumActivePatternFromUi (int pattern);
        bool setDrumPatternCellFromUi (int pattern, int track, int step, bool active,
                                       float velocity, float gate, float probability, int divisions);

        struct ClipEvent
        {
            double time;     // seconds from clip start
            double length;   // seconds
            int    note;
            float  velocity;
        };

    private:
        // AudioIODeviceCallback
        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;

        // MidiInputCallback
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

        // MidiKeyboardStateListener
        void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
        void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

        // Computer keyboard input
        bool keyPressed (const juce::KeyPress& key, juce::Component*) override;

        // Timer (UI repaints at ~30 Hz so meters / param monitor / playhead animate)
        void timerCallback() override;

        // LiveValueStore::Listener  -> push to engine
        void liveValueChanged (const juce::String&, float) override;

        // PatchCraftProject::Listener -> rebuild engine if type changed
        void projectChanged() override;
        void projectChanged (PatchCraftProject::ChangeScope scope) override;

        // ---- Helpers ---------------------------------------------------------
        void ensureEngineMatchesProject();
        void syncAllValuesToEngine();
        void syncRoutingFromProject (bool preserveActiveNotes = false);
        void pushSpectrumSample (float sample);
        void clearClip();
        void hardStop();
        double clipLengthSeconds() const noexcept;
        void onTransportPlayPressed();
        void onTransportStopPressed();
        void onTransportRecordPressed();
        bool applyMidiMappings (const juce::MidiMessage& message);
        RenderContext makeRenderContext (int numSamples, int numInputs, int numOutputs) const;
        void applyMidiLiveValueToEngine (const juce::String& parameterId, float value);
        void enqueueMidiLiveValueUpdate (const juce::String& parameterId, float value);
        void schedulePendingMidiLiveFlush();
        void flushPendingMidiLiveValues();
        void syncInstrumentRendererFromDesigner();
        juce::String makePlaybackStatusText();
        void refreshPlaybackStatus();

        StudioMainComponent& owner;

        // Engine / audio
        std::unique_ptr<IInstrumentEngine> engine;
        DspRoutingEngine routingEngine;
        MidiPlaygroundRuntime arpeggiator;
        juce::String engineId;
        juce::SpinLock engineLock;

        double currentSampleRate = 44100.0;
        int    currentBlockSize  = 512;
        int    currentNumChans   = 2;
        bool   audioRunning      = false;
        bool   brandLabPreviewMode = false;
        juce::Rectangle<int> brandPreviewInstrumentBounds;

        // MIDI
        juce::MidiKeyboardState keyboardState;
        std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
        std::unique_ptr<StudioInstrumentRenderer> instrumentRenderer;
        juce::CriticalSection hardwareMidiLock;
        juce::MidiBuffer hardwareMidiBuffer;
        juce::CriticalSection pendingMidiLiveValueLock;
        std::map<juce::String, float> pendingMidiLiveValues;
        std::atomic<bool> midiLiveFlushPending { false };

        // Transport
        std::atomic<bool> playing   { false };
        std::atomic<bool> recording { false };
        std::atomic<bool> looping   { true };
        std::atomic<double> bpm     { 120.0 };
        int clipBars = 4;

        // Playhead position in seconds (mutated on audio thread).
        std::atomic<double> playPosSeconds { 0.0 };

        // MIDI clip - vector of recorded events. Mutated on the message thread
        // when clearing/loading; appended to by the audio thread when recording.
        // Read by the audio thread when playing back. Guarded by clipLock.
        juce::CriticalSection clipLock;
        std::vector<ClipEvent> clip;
        size_t playbackIndex = 0;   // audio-thread cursor into clip[]
        std::map<int, size_t> recordingNoteIndices;
        int selectedClipIndex = -1;
        int dragClipIndex = -1;

        // Levels (audio thread writes, UI reads).
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
        std::atomic<float> peakHoldL { 0.0f }, peakHoldR { 0.0f };
        int peakHoldFrames = 0;
        static constexpr int fftOrder = 10;
        static constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT forwardFFT { fftOrder };
        std::array<float, fftSize> fftFifo {};
        std::array<float, fftSize * 2> fftData {};
        std::array<float, 48> spectrumBins {};
        juce::SpinLock spectrumLock;
        int fftFifoIndex = 0;

        // ---- Sub-views (declared first so we can give them references) -----
        class ClipView;
        class MeterView;
        class SpectrumView;
        class ParamMonitor;
        std::unique_ptr<ClipView>     clipView;
        std::unique_ptr<MeterView>    meterView;
        std::unique_ptr<SpectrumView> spectrumView;
        std::unique_ptr<ParamMonitor> paramMonitor;

        // Transport bar controls
        juce::TextButton playBtn   { "Play" };
        juce::TextButton stopBtn   { "Stop" };
        juce::TextButton recordBtn { "Arm Rec" };
        juce::ToggleButton loopToggle { "Loop" };
        juce::Slider tempoSlider;
        juce::TextButton clearClipBtn { "Clear" };

        // Section labels
        juce::Label transportLabel, clipLabel, keyboardLabel,
                    metersLabel, monitorLabel, spectrumLabel, statusLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestPage)
    };

} // namespace patchcraft
