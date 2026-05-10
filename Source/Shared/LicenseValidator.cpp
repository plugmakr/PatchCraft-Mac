#include "LicenseValidator.h"
#include <juce_core/juce_core.h>

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
            h = (h ^ (uint32_t) c.toUpperCase()) * 0x01000193u;
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

    bool LicenseValidator::isValidKeyFormat (const juce::String& key)
    {
        const auto k = compactKey (key);
        if (k.length() != 24) return false;
        for (auto c : k)
            if (! c.isHexDigit()) return false;
        return true;
    }

    bool LicenseValidator::validateKey (const juce::String& key, const juce::String& instrumentId)
    {
        if (! isValidKeyFormat (key)) return false;
        const auto k = compactKey (key);
        // Key format: 12-char instrument hash + 8-char checksum + 4-char random
        const auto instHash = k.substring (0, 12);
        const auto expectedInst = juce::String::toHexString ((int) simpleHash (instrumentId)).toUpperCase().paddedLeftChar ('0', 12);
        if (instHash != expectedInst) return false;
        const auto check = k.substring (12, 20);
        const auto base = k.substring (0, 12) + k.substring (20);
        const auto expectedCheck = juce::String::toHexString ((int) keyChecksum (base)).toUpperCase().paddedLeftChar ('0', 8);
        return check == expectedCheck;
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
        if (! validateKey (info.licenseKey, hashInstrumentId ("", "")))
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
               .toUpperCase().paddedLeftChar ('0', 12);
    }

} // namespace patchcraft
