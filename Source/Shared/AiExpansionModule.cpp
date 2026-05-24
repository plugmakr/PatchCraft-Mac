#include "AiExpansionModule.h"

namespace patchcraft
{

    juce::File AiExpansionModule::findAiExpansionPayload()
    {
        const auto self    = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto selfDir = self.getParentDirectory();

        // Check for an AiExpansion folder dropped next to the executable
        auto checkPath = selfDir.getChildFile ("AiExpansion");
        if (checkPath.exists())
            return checkPath;

        // In dev environments, also check two directories up 
        checkPath = selfDir.getParentDirectory().getParentDirectory().getChildFile ("AiExpansion");
        if (checkPath.exists())
            return checkPath;

        return juce::File();
    }

    bool AiExpansionModule::isAiExpansionInstalled()
    {
        return findAiExpansionPayload().exists();
    }

    juce::String AiExpansionModule::aiExpansionInstallMessage()
    {
        return "PatchCraft AI Expansion is not installed.\n\n"
               "To use Prompt-to-Plugin Faust DSP generation and other AI capabilities, "
               "you must install the paid PatchCraft AI Expansion package.";
    }

    std::unique_ptr<juce::AudioProcessor> AiExpansionModule::compileFaustDsp (const juce::String& faustCode, juce::String& errorMsg)
    {
        if (! isAiExpansionInstalled())
        {
            errorMsg = aiExpansionInstallMessage();
            return nullptr;
        }

        auto payloadDir = findAiExpansionPayload();

        // TODO: Dynamically load libfaust.dylib or call an embedded faust JIT compiler here.
        // For now, we return nullptr with a warning that the local compiler isn't linked yet.
        errorMsg = "Faust compilation via AI Expansion is not yet linked in this build. Please ensure libfaust is present in the AiExpansion folder.";
        return nullptr;
    }

} // namespace patchcraft
