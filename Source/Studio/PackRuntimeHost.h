#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "../Player/PluginProcessor.h"
#include "../Shared/LiveValueStore.h"

#include <functional>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Single shared Player runtime for Studio: Player chrome + layout + pack audio.
    */
    class PackRuntimeHost : public juce::Component,
                            private LiveValueStore::Listener,
                            private juce::Timer
    {
    public:
        explicit PackRuntimeHost (StudioMainComponent& owner);
        ~PackRuntimeHost() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void attachToParent (juce::Component* parent, juce::Rectangle<int> area);
        void setPlayerChromeVisible (bool) { resized(); repaint(); }

        void activate();
        void deactivate();
        bool isAudioRunning() const noexcept { return audioRunning; }

        void reloadPack();
        void requestReload();
        void requestReloadImmediate();
        void ensurePlaybackReady();
        void previewNoteOn (int note, float velocity);
        void previewNoteOff (int note);
        void setStudioExitHandler (std::function<void()> handler);
        bool importUserContent (const juce::StringArray& paths, juce::String& report);

    private:
        void liveValueChanged (const juce::String& parameterId, float newValue) override;
        void timerCallback() override;

        void syncLiveValuesToProcessor();
        void attachAudioAndMidi();
        void detachAudioAndMidi();

        StudioMainComponent& owner;
        std::unique_ptr<juce::AudioProcessorEditor> playerEditor;
        std::unique_ptr<PlayerProcessor> processor;
        juce::AudioProcessorPlayer audioPlayer;
        juce::Label statusLabel;
        bool audioRunning = false;
        bool reloadPending = false;
        int reloadDebounceMs = 0;
        juce::File previewPackFolder;

        void setStatus (const juce::String& message, bool warning = false);
        bool exportPreviewPack (juce::String& errorOut);
        bool loadProjectPreviewPack();
    };

} // namespace patchcraft
