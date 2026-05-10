#include "StudioAudioService.h"

namespace patchcraft
{
    StudioAudioService::StudioAudioService()
    {
        loadState();
    }

    StudioAudioService::~StudioAudioService()
    {
        saveState();
    }

    juce::File StudioAudioService::settingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                  .getChildFile ("PatchCraft").getChildFile ("audio.xml");
    }

    void StudioAudioService::loadState()
    {
        const auto f = settingsFile();
        std::unique_ptr<juce::XmlElement> state;
        if (f.existsAsFile())
            state.reset (juce::XmlDocument::parse (f).release());

        const auto err = deviceManager.initialise (
            /*numInputChannelsNeeded*/  0,
            /*numOutputChannelsNeeded*/ 2,
            /*savedState*/              state.get(),
            /*selectDefaultDeviceOnFailure*/ true);

        if (err.isNotEmpty())
        {
            // Silent: a settings dialog will surface the failure to the user.
            DBG ("StudioAudioService::loadState: " << err);
        }
    }

    void StudioAudioService::saveState()
    {
        const auto f = settingsFile();
        f.getParentDirectory().createDirectory();
        std::unique_ptr<juce::XmlElement> state (deviceManager.createStateXml());
        if (state != nullptr)
            state->writeTo (f);
    }

    bool StudioAudioService::ensureOpen (juce::String& errorOut, int minInputChannels, int minOutputChannels)
    {
        minInputChannels = juce::jlimit (0, 8, minInputChannels);
        minOutputChannels = juce::jlimit (1, 8, minOutputChannels);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            const int activeInputs = device->getActiveInputChannels().countNumberOfSetBits();
            const int activeOutputs = device->getActiveOutputChannels().countNumberOfSetBits();
            if (activeInputs >= minInputChannels && activeOutputs >= minOutputChannels)
                return true;
        }

        const auto err = deviceManager.initialise (minInputChannels,
                                                   minOutputChannels,
                                                   nullptr,
                                                   true);
        if (err.isNotEmpty()) { errorOut = err; return false; }
        return deviceManager.getCurrentAudioDevice() != nullptr;
    }

} // namespace patchcraft
