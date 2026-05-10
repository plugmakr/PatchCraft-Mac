#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace patchcraft
{
    /**
        Single shared AudioDeviceManager + persisted state for the Studio app.
        Owned by StudioMainComponent. The Preview window and the Settings
        dialog both read/write this same manager.
    */
    class StudioAudioService
    {
    public:
        StudioAudioService();
        ~StudioAudioService();

        juce::AudioDeviceManager& getDeviceManager()      { return deviceManager; }

        // Persist current device + I/O settings to <userAppData>/PatchCraft/audio.xml.
        void saveState();
        void loadState();

        // Make sure the device is open and ready - idempotent.
        bool ensureOpen (juce::String& errorOut, int minInputChannels = 0, int minOutputChannels = 2);

    private:
        juce::AudioDeviceManager deviceManager;

        static juce::File settingsFile();
    };

} // namespace patchcraft
