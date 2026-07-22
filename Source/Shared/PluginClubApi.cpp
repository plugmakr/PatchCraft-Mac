#include "PluginClubApi.h"

namespace patchcraft
{
    namespace
    {
        static juce::String joinBase (const juce::String& path)
        {
            return juce::String (PluginClubApi::kBaseUrl) + "/" + path;
        }

        static juce::String postJson (const juce::String& url,
                                      const juce::String& body,
                                      const juce::String& bearerToken,
                                      int timeoutMs,
                                      int& statusCode,
                                      juce::String& responseBody)
        {
            statusCode = 0;
            responseBody.clear();

            juce::String headers = "Content-Type: application/json\r\nAccept: application/json\r\n";
            if (bearerToken.trim().isNotEmpty())
                headers += "Authorization: Bearer " + bearerToken.trim() + "\r\n";

            auto stream = juce::URL (url)
                .withPOSTData (body)
                .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withHttpRequestCmd ("POST")
                    .withConnectionTimeoutMs (juce::jlimit (3000, 120000, timeoutMs))
                    .withExtraHeaders (headers)
                    .withStatusCode (&statusCode)
                    .withNumRedirectsToFollow (2));

            if (stream == nullptr)
                return {};

            responseBody = stream->readEntireStreamAsString();
            return responseBody;
        }
    }

    juce::String PluginClubApi::baseUrl() { return kBaseUrl; }
    juce::String PluginClubApi::sellerImportUrl() { return joinBase (kSellerImportPath); }
    juce::String PluginClubApi::activateLicenseUrl() { return joinBase (kActivateLicensePath); }
    juce::String PluginClubApi::validateLicenseUrl() { return joinBase (kValidateLicensePath); }
    juce::String PluginClubApi::deviceAuthStartUrl() { return joinBase (kDeviceAuthStartPath); }
    juce::String PluginClubApi::deviceAuthPollUrl() { return joinBase (kDeviceAuthPollPath); }
    juce::String PluginClubApi::deviceAuthLogoutUrl() { return joinBase (kDeviceAuthLogoutPath); }
    juce::String PluginClubApi::catalogUrl() { return joinBase (kCatalogPath); }
    juce::String PluginClubApi::buyerLibraryUrl() { return joinBase (kBuyerLibraryPath); }

    juce::String PluginClubApi::normaliseActivateLicenseEndpoint (const juce::String& configured)
    {
        auto value = configured.trim().trimCharactersAtEnd ("/");
        if (value.isEmpty())
            return activateLicenseUrl();

        if (value.containsIgnoreCase ("deviceAuth")
            || value.endsWithIgnoreCase ("/deviceAuthStart")
            || value.endsWithIgnoreCase ("/deviceAuthPoll")
            || value.endsWithIgnoreCase ("/deviceAuthLogout"))
            return activateLicenseUrl();

        if (value.endsWithIgnoreCase ("/validateLicense"))
            return value; // allow explicit validate endpoint

        if (value.endsWithIgnoreCase ("/activateLicense"))
            return value;

        if (value.equalsIgnoreCase (kBaseUrl)
            || value.equalsIgnoreCase ("https://plugin.club/functions")
            || value.equalsIgnoreCase ("https://plugin.club")
            || value.equalsIgnoreCase ("https://www.plugin.club"))
            return activateLicenseUrl();

        if (value.endsWithIgnoreCase ("/functions") || value.endsWithIgnoreCase ("/functions/v1"))
            return value.trimCharactersAtEnd ("/") + "/" + kActivateLicensePath;

        return value;
    }

    PluginClubApi::DeviceAuthStartResult PluginClubApi::startDeviceAuth (int timeoutMs)
    {
        DeviceAuthStartResult result;
        juce::String body;
        postJson (deviceAuthStartUrl(), "{}", {}, timeoutMs, result.statusCode, body);

        const auto parsed = juce::JSON::parse (body);
        if (auto* object = parsed.getDynamicObject())
        {
            result.deviceCode = object->getProperty ("device_code").toString().trim();
            result.userCode = object->getProperty ("user_code").toString().trim();
            result.verificationUrl = object->getProperty ("verification_url").toString().trim();
            result.verificationUrlComplete = object->getProperty ("verification_url_complete").toString().trim();
            result.expiresInSeconds = juce::jmax (30, (int) object->getProperty ("expires_in"));
            result.intervalSeconds = juce::jlimit (2, 30, (int) object->getProperty ("interval"));
            if (result.intervalSeconds <= 0)
                result.intervalSeconds = 5;
        }

        result.success = result.statusCode >= 200 && result.statusCode < 300
                      && result.deviceCode.isNotEmpty()
                      && result.userCode.isNotEmpty();
        if (! result.success)
        {
            result.message = body.isNotEmpty()
                ? ("Device auth start failed (HTTP " + juce::String (result.statusCode) + "): " + body.substring (0, 240))
                : "Could not reach Plugin.club deviceAuthStart.";
        }
        return result;
    }

    PluginClubApi::DeviceAuthPollResult PluginClubApi::pollDeviceAuth (const juce::String& deviceCode, int timeoutMs)
    {
        DeviceAuthPollResult result;
        auto* payload = new juce::DynamicObject();
        payload->setProperty ("device_code", deviceCode.trim());
        juce::String body;
        postJson (deviceAuthPollUrl(), juce::JSON::toString (juce::var (payload), false), {}, timeoutMs, result.statusCode, body);

        const auto parsed = juce::JSON::parse (body);
        if (auto* object = parsed.getDynamicObject())
        {
            result.accessToken = object->getProperty ("access_token").toString().trim();
            result.tokenType = object->getProperty ("token_type").toString().trim();
            if (result.tokenType.isEmpty())
                result.tokenType = "Bearer";
            result.expiresInSeconds = (int) object->getProperty ("expires_in");
            result.scope = object->getProperty ("scope").toString().trim();
            result.message = object->getProperty ("error").toString().trim();
            if (result.message.isEmpty())
                result.message = object->getProperty ("message").toString().trim();
        }

        if (result.statusCode == 428)
        {
            result.pending = true;
            result.message = result.message.isNotEmpty() ? result.message : "Authorization pending.";
            return result;
        }

        if (result.statusCode == 403)
        {
            result.denied = true;
            result.message = result.message.isNotEmpty() ? result.message : "Authorization denied.";
            return result;
        }

        if (result.statusCode == 400
            && (result.message.containsIgnoreCase ("expired") || body.containsIgnoreCase ("expired_token")))
        {
            result.expired = true;
            result.message = result.message.isNotEmpty() ? result.message : "Device code expired.";
            return result;
        }

        result.success = result.statusCode >= 200 && result.statusCode < 300 && result.accessToken.isNotEmpty();
        if (! result.success && result.message.isEmpty())
        {
            result.message = body.isNotEmpty()
                ? ("Device auth poll failed (HTTP " + juce::String (result.statusCode) + "): " + body.substring (0, 240))
                : "Could not reach Plugin.club deviceAuthPoll.";
        }
        return result;
    }

    PluginClubApi::LogoutResult PluginClubApi::logout (const juce::String& accessToken, int timeoutMs)
    {
        LogoutResult result;
        juce::String body;
        postJson (deviceAuthLogoutUrl(), "{}", accessToken, timeoutMs, result.statusCode, body);
        const auto parsed = juce::JSON::parse (body);
        const auto* object = parsed.getDynamicObject();
        result.success = result.statusCode >= 200 && result.statusCode < 300
                      && (object == nullptr || ! object->hasProperty ("success") || (bool) object->getProperty ("success"));
        if (! result.success)
        {
            result.message = body.isNotEmpty()
                ? ("Logout failed (HTTP " + juce::String (result.statusCode) + "): " + body.substring (0, 240))
                : "Could not reach Plugin.club deviceAuthLogout.";
        }
        return result;
    }
}
