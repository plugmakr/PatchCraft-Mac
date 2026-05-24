#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace patchcraft
{

    class AiExpansionModule
    {
    public:
        // Returns true if the PatchCraft AI Expansion package is installed
        static bool isAiExpansionInstalled();

        // Returns a helpful message instructing the user to install the expansion
        static juce::String aiExpansionInstallMessage();

        // Attempts to compile a Faust DSP string into a JUCE AudioProcessor
        // Returns nullptr if the expansion is not installed or if compilation fails.
        // errorMsg will contain the compilation error if it fails.
        static std::unique_ptr<juce::AudioProcessor> compileFaustDsp (const juce::String& faustCode, juce::String& errorMsg);

    private:
        // Check for the presence of the AI Expansion payload
        static juce::File findAiExpansionPayload();
    };

} // namespace patchcraft
