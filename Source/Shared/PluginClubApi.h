#pragma once

#include <juce_core/juce_core.h>

namespace patchcraft
{
    /** Live Plugin.club Platform Integration API helpers.
        Source of truth: https://plugin.club/api-docs (verified against running endpoints).
    */
    class PluginClubApi
    {
    public:
        static constexpr auto kBaseUrl = "https://plugin.club/functions/v1";
        static constexpr auto kSellerImportPath = "sellerImport";
        static constexpr auto kActivateLicensePath = "activateLicense";
        static constexpr auto kValidateLicensePath = "validateLicense";
        static constexpr auto kDeviceAuthStartPath = "deviceAuthStart";
        static constexpr auto kDeviceAuthPollPath = "deviceAuthPoll";
        static constexpr auto kDeviceAuthLogoutPath = "deviceAuthLogout";
        // Live catalog/library paths (romplur*). Docs also mention catalogAPI/marketplaceAPI, which currently 404.
        static constexpr auto kCatalogPath = "romplurCatalog";
        static constexpr auto kBuyerLibraryPath = "romplurBuyer/library";

        static juce::String baseUrl();
        static juce::String sellerImportUrl();
        static juce::String activateLicenseUrl();
        static juce::String validateLicenseUrl();
        static juce::String deviceAuthStartUrl();
        static juce::String deviceAuthPollUrl();
        static juce::String deviceAuthLogoutUrl();
        static juce::String catalogUrl();
        static juce::String buyerLibraryUrl();

        /** Rewrite legacy license URLs (deviceAuth*, romplur*) to activateLicense. */
        static juce::String normaliseActivateLicenseEndpoint (const juce::String& configured);

        struct DeviceAuthStartResult
        {
            bool success = false;
            juce::String message;
            int statusCode = 0;
            juce::String deviceCode;
            juce::String userCode;
            juce::String verificationUrl;
            juce::String verificationUrlComplete;
            int expiresInSeconds = 600;
            int intervalSeconds = 5;
        };

        struct DeviceAuthPollResult
        {
            bool success = false;
            bool pending = false;
            bool expired = false;
            bool denied = false;
            juce::String message;
            int statusCode = 0;
            juce::String accessToken;
            juce::String tokenType { "Bearer" };
            int expiresInSeconds = 0;
            juce::String scope;
        };

        struct LogoutResult
        {
            bool success = false;
            juce::String message;
            int statusCode = 0;
        };

        static DeviceAuthStartResult startDeviceAuth (int timeoutMs = 20000);
        static DeviceAuthPollResult pollDeviceAuth (const juce::String& deviceCode, int timeoutMs = 20000);
        static LogoutResult logout (const juce::String& accessToken, int timeoutMs = 15000);
    };
}
