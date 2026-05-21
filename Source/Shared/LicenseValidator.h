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
            juce::String policy;
            juce::String expiryDate;   // ISO 8601 or empty = perpetual
            int          trialDays = 0;  // 0 = not a trial
            bool         isTrial = false;
            int          offlineGraceDays = 14;
            bool         bindToMachine = true;
        };

        static bool isValidKeyFormat (const juce::String& key);
        static bool validateKey (const juce::String& key, const juce::String& instrumentId);
        static juce::String machineFingerprint();
        static juce::var buildActivationRequest (const LicenseInfo& info,
                                                 const juce::String& machineId = {});
        static juce::String generateWatermarkText (const LicenseInfo& info);
        static bool isExpired (const LicenseInfo& info);
        static int  daysRemaining (const LicenseInfo& info);
        static juce::String hashInstrumentId (const juce::String& instrumentName,
                                            const juce::String& creator);
    };

} // namespace patchcraft
