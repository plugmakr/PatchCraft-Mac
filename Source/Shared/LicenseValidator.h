#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Lightweight license validation for PatchCraft Player packs.
        Supports:
            - License key checksum validation
            - Trial mode with day countdown
            - Watermark generation for trial/unlicensed use
    */
    class LicenseValidator
    {
    public:
        struct LicenseInfo
        {
            juce::String licenseKey;
            juce::String ownerName;
            juce::String instrumentName;
            juce::String creator;
            juce::String instrumentId;
            juce::String productId;
            juce::String licenseServerUrl;
            juce::String publicKey;
            juce::String policy;
            juce::String expiryDate;   // ISO 8601 or empty = perpetual
            int          trialDays = 0;  // 0 = not a trial
            bool         isTrial = false;
            int          offlineGraceDays = 14;
            bool         bindToMachine = true;
        };

        struct ActivationStatus
        {
            bool authorized = false;
            bool trial = false;
            bool offlineGrace = false;
            juce::String status { "unlicensed" };
            juce::String message;
            juce::String code;
            juce::String ownerName;
            juce::String productId;
            juce::String instrumentId;
            juce::String machineId;
            juce::String expiryDate;
            juce::String lastValidatedAt;
            juce::String graceUntil;
            juce::String entitlementToken;
            juce::String integrity;

            juce::var toVar() const;
            static ActivationStatus fromVar (const juce::var& value);
        };

        static bool isValidKeyFormat (const juce::String& key);
        static bool validateKey (const juce::String& key, const juce::String& instrumentId);
        static juce::String machineFingerprint();
        static LicenseInfo fromManifest (const Manifest& manifest);
        static juce::var buildActivationRequest (const LicenseInfo& info,
                                                 const juce::String& machineId = {});
        static ActivationStatus parseActivationResponse (const juce::var& response,
                                                         const LicenseInfo& info,
                                                         int httpStatusCode = 200);
        static ActivationStatus activateOnline (const LicenseInfo& info,
                                                const juce::String& licenseKey,
                                                int timeoutMs,
                                                juce::String& diagnostic);
        static bool isActivationUsable (const LicenseInfo& info,
                                        const ActivationStatus& activation,
                                        juce::String& reason);
        static juce::File cacheFileFor (const LicenseInfo& info);
        static bool saveCachedActivation (const LicenseInfo& info,
                                          ActivationStatus activation,
                                          juce::String& error,
                                          const juce::File& overrideFile = {});
        static ActivationStatus loadCachedActivation (const LicenseInfo& info,
                                                      juce::String& reason,
                                                      const juce::File& overrideFile = {});
        static juce::String generateWatermarkText (const LicenseInfo& info);
        static bool isExpired (const LicenseInfo& info);
        static int  daysRemaining (const LicenseInfo& info);
        static juce::String hashInstrumentId (const juce::String& instrumentName,
                                            const juce::String& creator);
    };

} // namespace patchcraft
