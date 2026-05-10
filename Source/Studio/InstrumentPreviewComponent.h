#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PatchCraftProject.h"
#include "AssetManager.h"
#include "IInstrumentEngine.h"
#include "DspRoutingEngine.h"
#include "ArpeggiatorRuntime.h"

namespace patchcraft
{
    /**
        In-Studio preview engine + floating preview window content.

        Owns a SampleSynthEngine, an AudioDeviceManager, a MidiKeyboardState
        and a MidiKeyboardComponent so the user can play notes from the QWERTY
        keyboard or the on-screen piano. Listens to the project's
        LiveValueStore so any value change in the Studio canvas immediately
        feeds the engine.
    */
    class InstrumentPreviewComponent : public juce::Component,
                                       public juce::AudioIODeviceCallback,
                                       public juce::MidiInputCallback,
                                       private juce::Timer,
                                       private LiveValueStore::Listener,
                                       private PatchCraftProject::Listener
    {
    public:
        InstrumentPreviewComponent (PatchCraftProject& project,
                                    AssetManager& assets,
                                    juce::AudioDeviceManager& sharedDeviceManager);
        ~InstrumentPreviewComponent() override;

        void start();
        void stop();
        bool isActive() const noexcept                     { return active; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        // AudioIODeviceCallback
        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;

        // MIDI input from external device
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

        // LiveValueStore::Listener
        void liveValueChanged (const juce::String& parameterId, float newValue) override;

        // PatchCraftProject::Listener
        void projectChanged() override;

        // Timer
        void timerCallback() override;

        // Push every current live value into the engine.
        void syncAllValuesToEngine();
        void syncRoutingFromProject();
        juce::String makePlaybackStatusText();
        void refreshPlaybackStatus();

        PatchCraftProject& project;
        AssetManager&      assets;

        juce::AudioDeviceManager& deviceManager;
        std::unique_ptr<IInstrumentEngine> engine;
        DspRoutingEngine routingEngine;
        ArpeggiatorRuntime arpeggiator;
        juce::String engineId;
        juce::SpinLock engineLock;
        double currentSampleRate = 44100.0;
        int    currentBlockSize  = 512;
        int    currentNumChans   = 2;

        // Rebuild engine to match project.manifest.engine on next start.
        void ensureEngineMatches();
        juce::MidiKeyboardState  keyboardState;
        std::unique_ptr<juce::MidiKeyboardComponent> keyboard;

        juce::Label headerLabel;
        juce::Label statusLabel;

        bool active = false;
    };

} // namespace patchcraft
