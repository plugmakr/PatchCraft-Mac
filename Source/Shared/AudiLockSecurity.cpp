#include "AudiLockSecurity.h"

namespace patchcraft
{
    // For demo purposes, we do a very simple obfuscation.
    // AudiLock would use standard AES / RSA encryption.
    static juce::String obfuscateKey (const juce::String& key)
    {
        juce::String out;
        for (int i = 0; i < key.length(); ++i)
            out << (char) (key[i] ^ 0x42);
        return out;
    }

    juce::File AudiLockSecurity::getAiKeyFile()
    {
        // Store in the shared application data path where the AI expansion installs its models
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("Expansions")
            .getChildFile ("AI")
            .getChildFile ("audilock_ai.key");
    }

    bool AudiLockSecurity::isAiKeyEmbedded()
    {
        return getAiKeyFile().existsAsFile();
    }

    juce::String AudiLockSecurity::getDecryptedAiApiKey()
    {
        auto file = getAiKeyFile();
        if (! file.existsAsFile())
            return {};

        juce::String encrypted = file.loadFileAsString();
        return obfuscateKey (encrypted).trim();
    }

    void AudiLockSecurity::embedTestKey (const juce::String& rawKey)
    {
        auto file = getAiKeyFile();
        file.getParentDirectory().createDirectory();
        
        juce::String encrypted = obfuscateKey (rawKey);
        file.replaceWithText (encrypted);
    }

    juce::String AudiLockSecurity::getStatusMessage()
    {
        if (isAiKeyEmbedded())
            return "AudiLock AI Key is ACTIVE (Encrypted Embedded Payload)";
        else
            return "AudiLock AI Key is MISSING. Please install the AI Expansion.";
    }

} // namespace patchcraft
