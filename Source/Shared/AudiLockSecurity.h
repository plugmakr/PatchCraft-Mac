#pragma once

#include <juce_core/juce_core.h>

namespace patchcraft
{

    class AudiLockSecurity
    {
    public:
        /**
         * Checks if the AudiLock AI embedded key exists in the standard expansion path.
         */
        static bool isAiKeyEmbedded();

        /**
         * Decrypts and returns the embedded AudiLock AI API key.
         * In a real implementation, this would use a proprietary AudiLock decryption algorithm
         * tied to the machine ID or a hardcoded master key.
         */
        static juce::String getDecryptedAiApiKey();

        /**
         * Simulates embedding an encrypted key (for development/testing).
         */
        static void embedTestKey (const juce::String& rawKey);
        
        /**
         * Returns the status message for Settings Dialog UI.
         */
        static juce::String getStatusMessage();

    private:
        static juce::File getAiKeyFile();
    };

} // namespace patchcraft
