#include "LicenseValidator.h"
#include "PluginClubApi.h"
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace patchcraft
{
    static juce::String compactKey (const juce::String& key)
    {
        return key.removeCharacters (" -").toUpperCase();
    }

    static uint32_t simpleHash (const juce::String& s)
    {
        uint32_t h = 0x811c9dc5u;
        for (auto c : s)
            h = (h ^ (uint32_t) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) c)) * 0x01000193u;
        return h;
    }

    static uint32_t keyChecksum (const juce::String& key)
    {
        const auto k = compactKey (key);
        uint32_t h = 0;
        for (int i = 0; i < k.length(); ++i)
        {
            h += (uint32_t) k[i] * (uint32_t) (i + 1);
            h ^= h >> 3;
        }
        return h % 100000000u;
    }

    static juce::String firstStringProperty (const juce::DynamicObject* object,
                                             std::initializer_list<const char*> names)
    {
        if (object == nullptr)
            return {};
        for (const auto* name : names)
        {
            if (object->hasProperty (name))
            {
                const auto value = object->getProperty (name).toString().trim();
                if (value.isNotEmpty())
                    return value;
            }
        }
        return {};
    }

    static bool firstBoolProperty (const juce::DynamicObject* object,
                                   std::initializer_list<const char*> names,
                                   bool fallback = false)
    {
        if (object == nullptr)
            return fallback;
        for (const auto* name : names)
            if (object->hasProperty (name))
                return (bool) object->getProperty (name);
        return fallback;
    }

    static const juce::DynamicObject* entitlementObject (const juce::DynamicObject* root)
    {
        if (root == nullptr)
            return nullptr;
        for (const auto* name : { "license", "entitlement", "activation" })
            if (auto* nested = root->getProperty (name).getDynamicObject())
                return nested;
        return root;
    }

    static juce::String activationIntegrity (const LicenseValidator::LicenseInfo& info,
                                             const LicenseValidator::ActivationStatus& activation)
    {
        juce::String canonical;
        canonical << (activation.authorized ? 1 : 0) << "|" << (activation.trial ? 1 : 0) << "|"
                  << (activation.offlineGrace ? 1 : 0) << "|" << activation.status << "|"
                  << activation.ownerName << "|" << activation.productId << "|"
                  << activation.instrumentId << "|" << activation.machineId << "|"
                  << activation.expiryDate << "|" << activation.lastValidatedAt << "|"
                  << activation.graceUntil << "|" << activation.entitlementToken << "|"
                  << info.productId << "|" << info.instrumentId << "|"
                  << info.licenseServerUrl << "|" << info.publicKey << "|"
                  << LicenseValidator::machineFingerprint() << "|"
                  << info.policy;
        return juce::SHA256 (canonical.toRawUTF8(), (size_t) canonical.getNumBytesAsUTF8()).toHexString();
    }

    juce::var LicenseValidator::ActivationStatus::toVar() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("authorized", authorized);
        object->setProperty ("trial", trial);
        object->setProperty ("offlineGrace", offlineGrace);
        object->setProperty ("status", status);
        object->setProperty ("message", message);
        object->setProperty ("code", code);
        object->setProperty ("ownerName", ownerName);
        object->setProperty ("productId", productId);
        object->setProperty ("instrumentId", instrumentId);
        object->setProperty ("machineId", machineId);
        object->setProperty ("expiryDate", expiryDate);
        object->setProperty ("lastValidatedAt", lastValidatedAt);
        object->setProperty ("graceUntil", graceUntil);
        object->setProperty ("entitlementToken", entitlementToken);
        object->setProperty ("integrity", integrity);
        return juce::var (object);
    }

    LicenseValidator::ActivationStatus LicenseValidator::ActivationStatus::fromVar (const juce::var& value)
    {
        ActivationStatus result;
        if (auto* object = value.getDynamicObject())
        {
            result.authorized = (bool) object->getProperty ("authorized");
            result.trial = (bool) object->getProperty ("trial");
            result.offlineGrace = (bool) object->getProperty ("offlineGrace");
            result.status = object->getProperty ("status").toString();
            result.message = object->getProperty ("message").toString();
            result.code = object->getProperty ("code").toString();
            result.ownerName = object->getProperty ("ownerName").toString();
            result.productId = object->getProperty ("productId").toString();
            result.instrumentId = object->getProperty ("instrumentId").toString();
            result.machineId = object->getProperty ("machineId").toString();
            result.expiryDate = object->getProperty ("expiryDate").toString();
            result.lastValidatedAt = object->getProperty ("lastValidatedAt").toString();
            result.graceUntil = object->getProperty ("graceUntil").toString();
            result.entitlementToken = object->getProperty ("entitlementToken").toString();
            result.integrity = object->getProperty ("integrity").toString();
        }
        return result;
    }

    bool LicenseValidator::isValidKeyFormat (const juce::String& key)
    {
        const auto k = compactKey (key);
        if (k.length() != 24) return false;
        return k.containsOnly ("0123456789ABCDEFabcdef");
    }

    bool LicenseValidator::validateKey (const juce::String& key, const juce::String& instrumentId)
    {
        if (! isValidKeyFormat (key)) return false;
        const auto k = compactKey (key);
        // Key format: 12-char instrument hash + 8-char checksum + 4-char random
        const auto instHash = k.substring (0, 12);
        const auto compactInstrumentId = compactKey (instrumentId);
        const auto expectedInst = compactInstrumentId.length() == 12
                                  && compactInstrumentId.containsOnly ("0123456789ABCDEFabcdef")
            ? compactInstrumentId
            : juce::String::toHexString ((int) simpleHash (instrumentId)).toUpperCase().paddedLeft ('0', 12);
        if (instHash != expectedInst) return false;
        const auto check = k.substring (12, 20);
        const auto base = k.substring (0, 12) + k.substring (20);
        const auto expectedCheck = juce::String::toHexString ((int) keyChecksum (base)).toUpperCase().paddedLeft ('0', 8);
        return check == expectedCheck;
    }

    juce::String LicenseValidator::machineFingerprint()
    {
        juce::String source;
        source << juce::SystemStats::getComputerName() << "|"
               << juce::SystemStats::getLogonName() << "|"
               << juce::SystemStats::getOperatingSystemName() << "|"
               << juce::SystemStats::getDeviceDescription();
        return juce::String::toHexString ((int) simpleHash (source)).toUpperCase().paddedLeft ('0', 12);
    }

    LicenseValidator::LicenseInfo LicenseValidator::fromManifest (const Manifest& manifest)
    {
        LicenseInfo info;
        info.licenseKey = manifest.licenseKey;
        info.ownerName = manifest.licenseOwner;
        info.instrumentName = manifest.instrumentName;
        info.creator = manifest.creator;
        info.instrumentId = hashInstrumentId (manifest.instrumentName, manifest.creator);
        info.productId = manifest.licenseProductId;
        info.licenseServerUrl = manifest.licenseServerUrl;
        info.publicKey = manifest.licensePublicKey;
        info.policy = manifest.licensePolicy;
        info.expiryDate = manifest.trialExpiryDate;
        info.trialDays = manifest.trialDays;
        info.isTrial = manifest.isTrial;
        info.offlineGraceDays = manifest.licenseOfflineGraceDays;
        info.bindToMachine = manifest.licenseBindToMachine;
        return info;
    }

    juce::var LicenseValidator::buildActivationRequest (const LicenseInfo& info,
                                                        const juce::String& machineId)
    {
        auto instrumentId = info.instrumentId.trim();
        if (instrumentId.isEmpty())
            instrumentId = hashInstrumentId (info.instrumentName, info.creator);

        const auto hardwareId = machineId.trim().isNotEmpty() ? machineId.trim()
                                                              : machineFingerprint();

        auto* object = new juce::DynamicObject();
        // Plugin.club activateLicense / validateLicense contract
        // https://plugin.club/api-docs (Licenses tab)
        object->setProperty ("license_key", info.licenseKey);
        object->setProperty ("hardware_id", hardwareId);
        object->setProperty ("machine_name", juce::SystemStats::getComputerName());
        object->setProperty ("os_info", juce::SystemStats::getOperatingSystemName());

        // PatchCraft / AudiLock-compatible fields (kept for future backends)
        object->setProperty ("type", "patchcraft.license.activationRequest");
        object->setProperty ("instrumentId", instrumentId);
        object->setProperty ("productId", info.productId);
        object->setProperty ("instrumentName", info.instrumentName);
        object->setProperty ("creator", info.creator);
        object->setProperty ("licenseKey", info.licenseKey);
        object->setProperty ("machineId", hardwareId);
        object->setProperty ("policy", info.policy.isNotEmpty() ? info.policy
                                                                 : juce::String ("online-or-offline-grace"));
        object->setProperty ("trial", info.isTrial);
        object->setProperty ("trialDays", info.trialDays);
        object->setProperty ("offlineGraceDays", info.offlineGraceDays);
        object->setProperty ("bindToMachine", info.bindToMachine);
        object->setProperty ("requestedAt", juce::Time::getCurrentTime().toISO8601 (true));
        return juce::var (object);
    }

    LicenseValidator::ActivationStatus LicenseValidator::parseActivationResponse (const juce::var& response,
                                                                                  const LicenseInfo& info,
                                                                                  int httpStatusCode)
    {
        ActivationStatus result;
        result.productId = info.productId;
        result.instrumentId = info.instrumentId.isNotEmpty()
            ? info.instrumentId
            : hashInstrumentId (info.instrumentName, info.creator);
        result.machineId = machineFingerprint();
        result.lastValidatedAt = juce::Time::getCurrentTime().toISO8601 (true);

        const auto* root = response.getDynamicObject();
        const auto* entitlement = entitlementObject (root);
        const auto* activation = root != nullptr && root->getProperty ("activation").isObject()
            ? root->getProperty ("activation").getDynamicObject()
            : nullptr;

        result.status = firstStringProperty (entitlement, { "status", "license_status", "entitlement_status" }).toLowerCase();
        if (result.status.isEmpty() && activation != nullptr)
            result.status = activation->getProperty ("status").toString().toLowerCase();

        result.message = firstStringProperty (root, { "message", "error", "detail" });
        result.code = firstStringProperty (root, { "code", "error_code" });
        result.ownerName = firstStringProperty (entitlement, { "owner_name", "ownerName", "customer_name", "email", "product_title" });
        if (result.ownerName.isEmpty() && root != nullptr)
            result.ownerName = root->getProperty ("product_title").toString().trim();

        result.expiryDate = firstStringProperty (entitlement, { "expires_at", "expiresAt", "expiry_date", "expiryDate" });
        result.graceUntil = firstStringProperty (entitlement, { "offline_grace_until", "grace_until", "graceUntil" });
        result.entitlementToken = firstStringProperty (entitlement, { "entitlement_token", "license_token", "token", "activation_token" });
        if (result.entitlementToken.isEmpty() && activation != nullptr)
            result.entitlementToken = activation->getProperty ("activation_token").toString().trim();
        if (result.entitlementToken.isEmpty() && root != nullptr)
            result.entitlementToken = root->getProperty ("activation_token").toString().trim();

        const auto responseProduct = firstStringProperty (entitlement, { "product_id", "productId" });
        if (responseProduct.isNotEmpty())
            result.productId = responseProduct;

        const bool rootRejected = root != nullptr && root->hasProperty ("ok") && ! (bool) root->getProperty ("ok");
        const bool pluginClubValid = root != nullptr && root->hasProperty ("valid") && (bool) root->getProperty ("valid");
        const bool pluginClubSuccess = root != nullptr && root->hasProperty ("success") && (bool) root->getProperty ("success");
        const bool statusAccepted = result.status == "active" || result.status == "valid"
                                 || result.status == "licensed" || result.status == "trial"
                                 || result.status == "grace";
        const bool flagAccepted = firstBoolProperty (entitlement,
                                                     { "authorized", "active", "licensed", "valid", "success" });
        const bool rootAccepted = root != nullptr && firstBoolProperty (root, { "ok", "success", "valid" });
        const bool rootHasEntitlementEvidence = result.entitlementToken.isNotEmpty()
                                             || result.ownerName.isNotEmpty()
                                             || responseProduct.isNotEmpty();
        result.authorized = httpStatusCode >= 200 && httpStatusCode < 300
                         && ! rootRejected
                         && (statusAccepted || flagAccepted || pluginClubValid || pluginClubSuccess
                             || (rootAccepted && rootHasEntitlementEvidence));
        result.trial = result.status == "trial" || firstBoolProperty (entitlement, { "trial", "is_trial" })
                    || (root != nullptr && root->getProperty ("license_type").toString().equalsIgnoreCase ("trial"));
        result.offlineGrace = result.status == "grace" || firstBoolProperty (entitlement, { "offline_grace", "in_grace" });

        if (result.status.isEmpty())
            result.status = result.authorized ? (result.trial ? "trial" : "licensed") : "unlicensed";
        if (! result.authorized && result.message.isEmpty())
            result.message = httpStatusCode > 0
                ? "Activation was rejected by the license server (HTTP " + juce::String (httpStatusCode) + ")."
                : "Activation response was not valid.";
        return result;
    }

    LicenseValidator::ActivationStatus LicenseValidator::activateOnline (const LicenseInfo& info,
                                                                         const juce::String& licenseKey,
                                                                         int timeoutMs,
                                                                         juce::String& diagnostic)
    {
        LicenseInfo requestInfo = info;
        requestInfo.licenseKey = licenseKey.trim();
        ActivationStatus result;
        result.productId = info.productId;
        result.instrumentId = info.instrumentId;
        result.machineId = machineFingerprint();

        if (info.licenseServerUrl.trim().isEmpty())
        {
            result.status = "misconfigured";
            result.code = "LICENSE_CONFIG_MISSING";
            result.message = "This product is missing its license activation URL. Contact the publisher.";
            diagnostic = result.message;
            return result;
        }
        if (requestInfo.licenseKey.isEmpty())
        {
            result.message = "Enter a license key before activating.";
            diagnostic = result.message;
            return result;
        }

        const auto endpoint = PluginClubApi::normaliseActivateLicenseEndpoint (info.licenseServerUrl);
        const auto payload = buildActivationRequest (requestInfo);
        int statusCode = 0;
        juce::StringPairArray responseHeaders;
        auto stream = juce::URL (endpoint)
            .withPOSTData (juce::JSON::toString (payload, false))
            .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                .withHttpRequestCmd ("POST")
                .withConnectionTimeoutMs (juce::jlimit (3000, 120000, timeoutMs))
                .withExtraHeaders ("Content-Type: application/json\r\nAccept: application/json\r\n")
                .withResponseHeaders (&responseHeaders)
                .withStatusCode (&statusCode)
                .withNumRedirectsToFollow (2));

        if (stream == nullptr)
        {
            result.status = "offline";
            result.code = "LICENSE_SERVER_UNREACHABLE";
            result.message = "Could not reach the license server. Check your connection and try again.";
            diagnostic = result.message;
            return result;
        }

        const auto body = stream->readEntireStreamAsString();
        const auto parsed = juce::JSON::parse (body);
        result = parseActivationResponse (parsed, info, statusCode);
        diagnostic = "HTTP " + juce::String (statusCode) + " @ " + endpoint
                   + (body.isNotEmpty() ? ": " + body : juce::String());

        if (result.authorized)
        {
            juce::String cacheError;
            if (! saveCachedActivation (info, result, cacheError))
            {
                result.message = "Activated, but the offline license cache could not be saved: " + cacheError;
                diagnostic += "\n" + result.message;
            }
        }
        return result;
    }

    bool LicenseValidator::isActivationUsable (const LicenseInfo& info,
                                               const ActivationStatus& activation,
                                               juce::String& reason)
    {
        if (! activation.authorized)
        {
            reason = activation.message.isNotEmpty() ? activation.message : "License activation is required.";
            return false;
        }
        if (info.productId.isNotEmpty() && activation.productId != info.productId)
        {
            reason = "The cached license belongs to a different product.";
            return false;
        }
        if (info.bindToMachine && activation.machineId != machineFingerprint())
        {
            reason = "The cached license belongs to a different machine.";
            return false;
        }
        if (activation.expiryDate.isNotEmpty())
        {
            const auto expiry = juce::Time::fromISO8601 (activation.expiryDate);
            if (expiry != juce::Time() && expiry <= juce::Time::getCurrentTime())
            {
                reason = "The license has expired.";
                return false;
            }
        }
        if (activation.graceUntil.isNotEmpty())
        {
            const auto grace = juce::Time::fromISO8601 (activation.graceUntil);
            if (grace != juce::Time() && grace <= juce::Time::getCurrentTime())
            {
                reason = "The offline grace period has expired. Reconnect and activate again.";
                return false;
            }
        }
        reason = activation.offlineGrace ? "Offline grace active." : "Licensed.";
        return true;
    }

    juce::File LicenseValidator::cacheFileFor (const LicenseInfo& info)
    {
        auto id = info.productId.trim();
        if (id.isEmpty())
            id = info.instrumentId.trim();
        if (id.isEmpty())
            id = hashInstrumentId (info.instrumentName, info.creator);
        id = juce::File::createLegalFileName (id);
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("Licenses")
            .getChildFile (id + ".json");
    }

    bool LicenseValidator::saveCachedActivation (const LicenseInfo& info,
                                                 ActivationStatus activation,
                                                 juce::String& error,
                                                 const juce::File& overrideFile)
    {
        const auto file = overrideFile != juce::File() ? overrideFile : cacheFileFor (info);
        if (! file.getParentDirectory().createDirectory())
        {
            error = "Could not create the license cache folder.";
            return false;
        }
        activation.integrity.clear();
        activation.integrity = activationIntegrity (info, activation);
        if (! file.replaceWithText (juce::JSON::toString (activation.toVar(), true)))
        {
            error = "Could not write " + file.getFullPathName();
            return false;
        }
        error.clear();
        return true;
    }

    LicenseValidator::ActivationStatus LicenseValidator::loadCachedActivation (const LicenseInfo& info,
                                                                               juce::String& reason,
                                                                               const juce::File& overrideFile)
    {
        const auto file = overrideFile != juce::File() ? overrideFile : cacheFileFor (info);
        if (! file.existsAsFile())
        {
            reason = "No activation is stored for this product.";
            return {};
        }

        auto activation = ActivationStatus::fromVar (juce::JSON::parse (file));
        const auto storedIntegrity = activation.integrity;
        activation.integrity.clear();
        const auto expectedIntegrity = activationIntegrity (info, activation);
        activation.integrity = storedIntegrity;
        if (storedIntegrity.isEmpty() || storedIntegrity != expectedIntegrity)
        {
            activation.authorized = false;
            activation.status = "invalid-cache";
            activation.message = "The cached license is invalid. Activate again.";
            reason = activation.message;
            return activation;
        }

        if (! isActivationUsable (info, activation, reason))
            activation.authorized = false;
        return activation;
    }

    juce::String LicenseValidator::generateWatermarkText (const LicenseInfo& info)
    {
        if (info.isTrial)
        {
            const int days = daysRemaining (info);
            if (days <= 0) return "TRIAL EXPIRED";
            return "TRIAL: " + juce::String (days) + " day" + (days == 1 ? "" : "s") + " left";
        }
        if (info.licenseKey.isEmpty())
            return "UNLICENSED";

        auto instrumentId = info.instrumentId.trim();
        if (instrumentId.isEmpty() && (info.instrumentName.isNotEmpty() || info.creator.isNotEmpty()))
            instrumentId = hashInstrumentId (info.instrumentName, info.creator);

        if (instrumentId.isEmpty())
            return {};

        if (! validateKey (info.licenseKey, instrumentId))
            return "INVALID LICENSE";
        return juce::String();
    }

    bool LicenseValidator::isExpired (const LicenseInfo& info)
    {
        if (! info.isTrial || info.trialDays <= 0) return false;
        return daysRemaining (info) <= 0;
    }

    int LicenseValidator::daysRemaining (const LicenseInfo& info)
    {
        if (! info.isTrial || info.trialDays <= 0) return 999;
        if (info.expiryDate.isEmpty()) return info.trialDays;
        const auto expiry = juce::Time::fromISO8601 (info.expiryDate);
        if (expiry == juce::Time()) return info.trialDays;
        const auto now = juce::Time::getCurrentTime();
        const auto diff = expiry - now;
        return juce::jmax (0, (int) (diff.inDays()));
    }

    juce::String LicenseValidator::hashInstrumentId (const juce::String& instrumentName,
                                                        const juce::String& creator)
    {
        return juce::String::toHexString ((int) simpleHash (instrumentName + "|" + creator))
               .toUpperCase().paddedLeft ('0', 12);
    }

} // namespace patchcraft
