#include "PatchCraftTypes.h"
#include "DspModuleRegistry.h"
#include "SoundStack.h"
#include "SampleSliceUtils.h"

#include <cmath>

namespace patchcraft
{
    static juce::String colourToString (juce::Colour c)
    {
        // Output #RRGGBB (alpha not preserved - palette colours are opaque).
        return "#" + c.toDisplayString (false);
    }

    static juce::Colour colourFromString (const juce::String& s, juce::Colour fallback)
    {
        if (s.isEmpty()) return fallback;
        auto v = s.startsWithChar ('#') ? s.substring (1) : s;
        // juce::Colour::fromString expects AARRGGBB - prepend opaque alpha.
        return juce::Colour::fromString ("ff" + v.toLowerCase());
    }

    static juce::var stringArrayToVar (const juce::StringArray& values)
    {
        juce::Array<juce::var> arr;
        for (const auto& value : values)
            arr.add (value);
        return arr;
    }

    static void readStringArrayProperty (juce::DynamicObject* object,
                                         const juce::Identifier& property,
                                         juce::StringArray& destination)
    {
        destination.clear();
        if (object == nullptr)
            return;

        if (auto* arr = object->getProperty (property).getArray())
            for (const auto& value : *arr)
                destination.add (value.toString());
    }

    static juce::var floatArrayToVar (const juce::Array<float>& values)
    {
        juce::Array<juce::var> arr;
        for (const auto value : values)
            arr.add ((double) value);
        return arr;
    }

    static juce::var intArrayToVar (const juce::Array<int>& values)
    {
        juce::Array<juce::var> arr;
        for (const auto value : values)
            arr.add (value);
        return arr;
    }

    static void readFloatArrayProperty (juce::DynamicObject* object,
                                        const juce::Identifier& property,
                                        juce::Array<float>& destination)
    {
        destination.clear();
        if (object == nullptr)
            return;

        if (auto* arr = object->getProperty (property).getArray())
            for (const auto& value : *arr)
                destination.add ((float) value);
    }

    static void readIntArrayProperty (juce::DynamicObject* object,
                                      const juce::Identifier& property,
                                      juce::Array<int>& destination)
    {
        destination.clear();
        if (object == nullptr)
            return;

        if (auto* arr = object->getProperty (property).getArray())
            for (const auto& value : *arr)
                destination.add ((int) value);
    }

    // Manifest -----------------------------------------------------------------
    juce::var Manifest::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("formatVersion",   formatVersion);
        obj->setProperty ("instrumentName",  instrumentName);
        obj->setProperty ("creator",         creator);
        obj->setProperty ("description",     description);
        obj->setProperty ("category",        category);
        obj->setProperty ("engine",          engine);
        obj->setProperty ("backgroundImage", backgroundImage);
        obj->setProperty ("defaultPreset",   defaultPreset);
        obj->setProperty ("createdWith",     createdWith);
        obj->setProperty ("multiInstrumentMode", multiInstrumentMode);
        obj->setProperty ("libraryThumbnail", libraryThumbnail);
        obj->setProperty ("version",         version);
        obj->setProperty ("website",         website);
        obj->setProperty ("playerDisplayName",       playerDisplayName);
        obj->setProperty ("playerTagline",           playerTagline);
        obj->setProperty ("playerLogoImage",         playerLogoImage);
        obj->setProperty ("playerTitleBannerImage",  playerTitleBannerImage);
        obj->setProperty ("playerTitleBarTheme",     playerTitleBarTheme);
        obj->setProperty ("playerTitleTextPlacement", playerTitleTextPlacement);
        obj->setProperty ("playerTitleButtonStyle",  playerTitleButtonStyle);
        obj->setProperty ("playerTitleFontFamily",   playerTitleFontFamily);
        obj->setProperty ("playerClientName",        playerClientName);
        obj->setProperty ("playerSupportEmail",      playerSupportEmail);
        obj->setProperty ("playerSupportUrl",        playerSupportUrl);
        obj->setProperty ("playerManualUrl",         playerManualUrl);
        obj->setProperty ("playerStoreUrl",          playerStoreUrl);
        obj->setProperty ("playerCopyright",         playerCopyright);
        obj->setProperty ("playerLegalText",         playerLegalText);
        obj->setProperty ("salesHeadline",           salesHeadline);
        obj->setProperty ("salesSubheadline",        salesSubheadline);
        obj->setProperty ("salesCtaText",            salesCtaText);
        obj->setProperty ("salesCheckoutUrl",        salesCheckoutUrl);
        obj->setProperty ("salesDemoVideoUrl",       salesDemoVideoUrl);
        obj->setProperty ("salesAudioDemoUrl",       salesAudioDemoUrl);
        obj->setProperty ("salesCurrency",           salesCurrency);
        obj->setProperty ("salesPrice",              salesPrice);
        obj->setProperty ("salesCompareAtPrice",     salesCompareAtPrice);
        obj->setProperty ("salesHighlights",         stringArrayToVar (salesHighlights));
        obj->setProperty ("salesIncludes",           stringArrayToVar (salesIncludes));
        obj->setProperty ("salesFaq",                stringArrayToVar (salesFaq));
        obj->setProperty ("playerBackgroundColour",  playerBackgroundColour.toString());
        obj->setProperty ("playerPanelColour",       playerPanelColour.toString());
        obj->setProperty ("playerAccentColour",      playerAccentColour.toString());
        obj->setProperty ("playerTextColour",        playerTextColour.toString());
        obj->setProperty ("playerTextDimColour",     playerTextDimColour.toString());
        obj->setProperty ("playerBorderColour",      playerBorderColour.toString());
        obj->setProperty ("playerShowPackMenu",      playerShowPackMenu);
        obj->setProperty ("playerAllowPackLoading",  playerAllowPackLoading);
        obj->setProperty ("playerShowLibraryBrowser", playerShowLibraryBrowser);
        obj->setProperty ("playerAllowMidiLearn",    playerAllowMidiLearn);
        obj->setProperty ("playerShowAbout",         playerShowAbout);
        obj->setProperty ("playerShowParameterGuidance", playerShowParameterGuidance);
        obj->setProperty ("playerShowPatchCraftBranding", playerShowPatchCraftBranding);
        obj->setProperty ("studioShowTutorials",     studioShowTutorials);
        obj->setProperty ("playerShowTopBar",        playerShowTopBar);
        obj->setProperty ("playerShowLeftSidebar",   playerShowLeftSidebar);
        obj->setProperty ("playerShowFooter",        playerShowFooter);
        obj->setProperty ("playerShowRightPanel",    playerShowRightPanel);
        obj->setProperty ("playerShowKeyboard",      playerShowKeyboard);
        obj->setProperty ("playerTopShowBrowse",     playerTopShowBrowse);
        obj->setProperty ("playerTopShowSave",       playerTopShowSave);
        obj->setProperty ("playerTopShowSettings",   playerTopShowSettings);
        obj->setProperty ("playerTopShowCategory",   playerTopShowCategory);
        obj->setProperty ("playerTopShowFavorite",   playerTopShowFavorite);
        obj->setProperty ("playerTopShowPresetNav",  playerTopShowPresetNav);
        obj->setProperty ("playerTopShowMasterVolume", playerTopShowMasterVolume);
        obj->setProperty ("playerTopShowOutputMeter", playerTopShowOutputMeter);
        obj->setProperty ("rightPanelShowMacros",    rightPanelShowMacros);
        obj->setProperty ("rightPanelShowEffects",   rightPanelShowEffects);
        obj->setProperty ("rightPanelShowSends",     rightPanelShowSends);
        obj->setProperty ("rightPanelShowUtility",   rightPanelShowUtility);
        obj->setProperty ("rightPanelMacroNames",    stringArrayToVar (rightPanelMacroNames));
        obj->setProperty ("quickBuildMode",          quickBuildMode);
        obj->setProperty ("productRecipeId",         productRecipeId);
        obj->setProperty ("productKindLabel",      productKindLabel);
        obj->setProperty ("whiteLabelPackageName",   whiteLabelPackageName);
        obj->setProperty ("whiteLabelPublisher",     whiteLabelPublisher);
        obj->setProperty ("whiteLabelProductCode",   whiteLabelProductCode);
        obj->setProperty ("whiteLabelBundleIdentifier", whiteLabelBundleIdentifier);
        obj->setProperty ("whiteLabelInstallerId",   whiteLabelInstallerId);
        obj->setProperty ("whiteLabelInstallerIcon", whiteLabelInstallerIcon);
        obj->setProperty ("whiteLabelEulaPath",      whiteLabelEulaPath);
        obj->setProperty ("whiteLabelPrivacyUrl",    whiteLabelPrivacyUrl);
        obj->setProperty ("whiteLabelInstallNotes",  whiteLabelInstallNotes);
        obj->setProperty ("whiteLabelWindowsVst3Path", whiteLabelWindowsVst3Path);
        obj->setProperty ("whiteLabelMacVst3Path",   whiteLabelMacVst3Path);
        obj->setProperty ("whiteLabelRequireLicenseOnFirstRun", whiteLabelRequireLicenseOnFirstRun);
        obj->setProperty ("whiteLabelIncludeStandalone", whiteLabelIncludeStandalone);
        obj->setProperty ("whiteLabelIncludeVst3",   whiteLabelIncludeVst3);
        obj->setProperty ("licenseRequired",         licenseRequired);
        obj->setProperty ("licenseKey",              licenseKey);
        obj->setProperty ("licenseProductId",        licenseProductId);
        obj->setProperty ("licenseServerUrl",        licenseServerUrl);
        obj->setProperty ("licensePublicKey",        licensePublicKey);
        obj->setProperty ("licensePolicy",           licensePolicy);
        obj->setProperty ("trialDays",               trialDays);
        obj->setProperty ("isTrial",                 isTrial);
        obj->setProperty ("trialExpiryDate",         trialExpiryDate);
        obj->setProperty ("licenseOwner",              licenseOwner);
        obj->setProperty ("licenseOfflineGraceDays", licenseOfflineGraceDays);
        obj->setProperty ("licenseBindToMachine",    licenseBindToMachine);
        obj->setProperty ("licenseAllowTrialConversion", licenseAllowTrialConversion);

        juce::Array<juce::var> tagArray;
        for (auto& tag : tags)
            tagArray.add (tag);
        obj->setProperty ("tags", tagArray);

        juce::Array<juce::var> instrumentIdArray;
        for (auto& id : instrumentIds)
            instrumentIdArray.add (id);
        obj->setProperty ("instrumentIds", instrumentIdArray);

        juce::Array<juce::var> instrumentNameArray;
        for (auto& name : instrumentNames)
            instrumentNameArray.add (name);
        obj->setProperty ("instrumentNames", instrumentNameArray);

        juce::Array<juce::var> instrumentFileArray;
        for (auto& file : instrumentFiles)
            instrumentFileArray.add (file);
        obj->setProperty ("instrumentFiles", instrumentFileArray);
        obj->setProperty ("instrumentVolumes", instrumentVolumes.isEmpty() ? juce::var() : floatArrayToVar (instrumentVolumes));
        obj->setProperty ("instrumentPans", instrumentPans.isEmpty() ? juce::var() : floatArrayToVar (instrumentPans));
        obj->setProperty ("instrumentMidiChannels", instrumentMidiChannels.isEmpty() ? juce::var() : intArrayToVar (instrumentMidiChannels));
        obj->setProperty ("instrumentOutputRoutes", instrumentOutputRoutes.isEmpty() ? juce::var() : intArrayToVar (instrumentOutputRoutes));
        obj->setProperty ("instrumentTransposeSemitones", instrumentTransposeSemitones.isEmpty() ? juce::var() : intArrayToVar (instrumentTransposeSemitones));
        obj->setProperty ("instrumentEnabled", instrumentEnabled.isEmpty() ? juce::var() : intArrayToVar (instrumentEnabled));
        obj->setProperty ("instrumentAutoPlay", instrumentAutoPlay.isEmpty() ? juce::var() : intArrayToVar (instrumentAutoPlay));
        obj->setProperty ("instrumentAutoPlayNotes", instrumentAutoPlayNotes.isEmpty() ? juce::var() : intArrayToVar (instrumentAutoPlayNotes));
        obj->setProperty ("instrumentAutoPlayVelocities", instrumentAutoPlayVelocities.isEmpty() ? juce::var() : floatArrayToVar (instrumentAutoPlayVelocities));

        return juce::var (obj);
    }

    Manifest Manifest::fromVar (const juce::var& v)
    {
        Manifest m;
        if (auto* o = v.getDynamicObject())
        {
            m.formatVersion   = (int) o->getProperty ("formatVersion");
            m.instrumentName  = o->getProperty ("instrumentName").toString();
            m.creator         = o->getProperty ("creator").toString();
            m.description     = o->getProperty ("description").toString();
            m.category        = o->getProperty ("category").toString();
            m.engine          = o->getProperty ("engine").toString();
            m.backgroundImage = o->getProperty ("backgroundImage").toString();
            m.defaultPreset   = o->getProperty ("defaultPreset").toString();
            m.createdWith     = o->getProperty ("createdWith").toString();
            if (o->hasProperty ("multiInstrumentMode"))
                m.multiInstrumentMode = (bool) o->getProperty ("multiInstrumentMode");
            m.libraryThumbnail = o->getProperty ("libraryThumbnail").toString();
            m.version         = o->getProperty ("version").toString();
            m.website         = o->getProperty ("website").toString();
            if (o->hasProperty ("playerDisplayName"))
                m.playerDisplayName = o->getProperty ("playerDisplayName").toString();
            if (o->hasProperty ("playerTagline"))
                m.playerTagline = o->getProperty ("playerTagline").toString();
            if (o->hasProperty ("playerLogoImage"))
                m.playerLogoImage = o->getProperty ("playerLogoImage").toString();
            if (o->hasProperty ("playerTitleBannerImage"))
                m.playerTitleBannerImage = o->getProperty ("playerTitleBannerImage").toString();
            if (o->hasProperty ("playerTitleBarTheme"))
                m.playerTitleBarTheme = o->getProperty ("playerTitleBarTheme").toString();
            if (o->hasProperty ("playerTitleTextPlacement"))
                m.playerTitleTextPlacement = o->getProperty ("playerTitleTextPlacement").toString();
            if (o->hasProperty ("playerTitleButtonStyle"))
                m.playerTitleButtonStyle = o->getProperty ("playerTitleButtonStyle").toString();
            if (o->hasProperty ("playerTitleFontFamily"))
                m.playerTitleFontFamily = o->getProperty ("playerTitleFontFamily").toString();
            if (o->hasProperty ("playerClientName"))
                m.playerClientName = o->getProperty ("playerClientName").toString();
            if (o->hasProperty ("playerSupportEmail"))
                m.playerSupportEmail = o->getProperty ("playerSupportEmail").toString();
            if (o->hasProperty ("playerSupportUrl"))
                m.playerSupportUrl = o->getProperty ("playerSupportUrl").toString();
            if (o->hasProperty ("playerManualUrl"))
                m.playerManualUrl = o->getProperty ("playerManualUrl").toString();
            if (o->hasProperty ("playerStoreUrl"))
                m.playerStoreUrl = o->getProperty ("playerStoreUrl").toString();
            if (o->hasProperty ("playerCopyright"))
                m.playerCopyright = o->getProperty ("playerCopyright").toString();
            if (o->hasProperty ("playerLegalText"))
                m.playerLegalText = o->getProperty ("playerLegalText").toString();
            if (o->hasProperty ("salesHeadline"))
                m.salesHeadline = o->getProperty ("salesHeadline").toString();
            if (o->hasProperty ("salesSubheadline"))
                m.salesSubheadline = o->getProperty ("salesSubheadline").toString();
            if (o->hasProperty ("salesCtaText"))
                m.salesCtaText = o->getProperty ("salesCtaText").toString();
            if (o->hasProperty ("salesCheckoutUrl"))
                m.salesCheckoutUrl = o->getProperty ("salesCheckoutUrl").toString();
            if (o->hasProperty ("salesDemoVideoUrl"))
                m.salesDemoVideoUrl = o->getProperty ("salesDemoVideoUrl").toString();
            if (o->hasProperty ("salesAudioDemoUrl"))
                m.salesAudioDemoUrl = o->getProperty ("salesAudioDemoUrl").toString();
            if (o->hasProperty ("salesCurrency"))
                m.salesCurrency = o->getProperty ("salesCurrency").toString();
            if (o->hasProperty ("salesPrice"))
                m.salesPrice = (double) o->getProperty ("salesPrice");
            if (o->hasProperty ("salesCompareAtPrice"))
                m.salesCompareAtPrice = (double) o->getProperty ("salesCompareAtPrice");
            readStringArrayProperty (o, "salesHighlights", m.salesHighlights);
            readStringArrayProperty (o, "salesIncludes", m.salesIncludes);
            readStringArrayProperty (o, "salesFaq", m.salesFaq);
            if (o->hasProperty ("playerBackgroundColour"))
                m.playerBackgroundColour = juce::Colour::fromString (o->getProperty ("playerBackgroundColour").toString());
            if (o->hasProperty ("playerPanelColour"))
                m.playerPanelColour = juce::Colour::fromString (o->getProperty ("playerPanelColour").toString());
            if (o->hasProperty ("playerAccentColour"))
                m.playerAccentColour = juce::Colour::fromString (o->getProperty ("playerAccentColour").toString());
            if (o->hasProperty ("playerTextColour"))
                m.playerTextColour = juce::Colour::fromString (o->getProperty ("playerTextColour").toString());
            if (o->hasProperty ("playerTextDimColour"))
                m.playerTextDimColour = juce::Colour::fromString (o->getProperty ("playerTextDimColour").toString());
            if (o->hasProperty ("playerBorderColour"))
                m.playerBorderColour = juce::Colour::fromString (o->getProperty ("playerBorderColour").toString());
            if (o->hasProperty ("playerShowPackMenu"))
                m.playerShowPackMenu = (bool) o->getProperty ("playerShowPackMenu");
            if (o->hasProperty ("playerAllowPackLoading"))
                m.playerAllowPackLoading = (bool) o->getProperty ("playerAllowPackLoading");
            if (o->hasProperty ("playerShowLibraryBrowser"))
                m.playerShowLibraryBrowser = (bool) o->getProperty ("playerShowLibraryBrowser");
            if (o->hasProperty ("playerAllowMidiLearn"))
                m.playerAllowMidiLearn = (bool) o->getProperty ("playerAllowMidiLearn");
            if (o->hasProperty ("playerShowAbout"))
                m.playerShowAbout = (bool) o->getProperty ("playerShowAbout");
            if (o->hasProperty ("playerShowParameterGuidance"))
                m.playerShowParameterGuidance = (bool) o->getProperty ("playerShowParameterGuidance");
            if (o->hasProperty ("playerShowPatchCraftBranding"))
                m.playerShowPatchCraftBranding = (bool) o->getProperty ("playerShowPatchCraftBranding");
            if (o->hasProperty ("studioShowTutorials"))
                m.studioShowTutorials = (bool) o->getProperty ("studioShowTutorials");
            if (o->hasProperty ("playerShowTopBar"))
                m.playerShowTopBar = (bool) o->getProperty ("playerShowTopBar");
            if (o->hasProperty ("playerShowLeftSidebar"))
                m.playerShowLeftSidebar = (bool) o->getProperty ("playerShowLeftSidebar");
            if (o->hasProperty ("playerShowFooter"))
                m.playerShowFooter = (bool) o->getProperty ("playerShowFooter");
            if (o->hasProperty ("playerShowRightPanel"))
                m.playerShowRightPanel = (bool) o->getProperty ("playerShowRightPanel");
            if (o->hasProperty ("playerShowKeyboard"))
                m.playerShowKeyboard = (bool) o->getProperty ("playerShowKeyboard");
            if (o->hasProperty ("playerTopShowBrowse"))
                m.playerTopShowBrowse = (bool) o->getProperty ("playerTopShowBrowse");
            if (o->hasProperty ("playerTopShowSave"))
                m.playerTopShowSave = (bool) o->getProperty ("playerTopShowSave");
            if (o->hasProperty ("playerTopShowSettings"))
                m.playerTopShowSettings = (bool) o->getProperty ("playerTopShowSettings");
            if (o->hasProperty ("playerTopShowCategory"))
                m.playerTopShowCategory = (bool) o->getProperty ("playerTopShowCategory");
            if (o->hasProperty ("playerTopShowFavorite"))
                m.playerTopShowFavorite = (bool) o->getProperty ("playerTopShowFavorite");
            if (o->hasProperty ("playerTopShowPresetNav"))
                m.playerTopShowPresetNav = (bool) o->getProperty ("playerTopShowPresetNav");
            if (o->hasProperty ("playerTopShowMasterVolume"))
                m.playerTopShowMasterVolume = (bool) o->getProperty ("playerTopShowMasterVolume");
            if (o->hasProperty ("playerTopShowOutputMeter"))
                m.playerTopShowOutputMeter = (bool) o->getProperty ("playerTopShowOutputMeter");
            if (o->hasProperty ("rightPanelShowMacros"))
                m.rightPanelShowMacros = (bool) o->getProperty ("rightPanelShowMacros");
            if (o->hasProperty ("rightPanelShowEffects"))
                m.rightPanelShowEffects = (bool) o->getProperty ("rightPanelShowEffects");
            if (o->hasProperty ("rightPanelShowSends"))
                m.rightPanelShowSends = (bool) o->getProperty ("rightPanelShowSends");
            if (o->hasProperty ("rightPanelShowUtility"))
                m.rightPanelShowUtility = (bool) o->getProperty ("rightPanelShowUtility");
            if (o->hasProperty ("rightPanelMacroNames"))
            {
                m.rightPanelMacroNames.clear();
                if (auto* arr = o->getProperty ("rightPanelMacroNames").getArray())
                    for (const auto& item : *arr)
                        m.rightPanelMacroNames.add (item.toString());
            }
            if (m.rightPanelMacroNames.size() < 8)
            {
                const juce::String defaults[] = { "TONE", "WASH", "DRIVE", "WIDTH", "MOVEMENT", "PHASER", "DELAY", "REVERB" };
                while (m.rightPanelMacroNames.size() < 8)
                    m.rightPanelMacroNames.add (defaults[(size_t) m.rightPanelMacroNames.size()]);
            }
            if (o->hasProperty ("quickBuildMode"))
                m.quickBuildMode = (bool) o->getProperty ("quickBuildMode");
            if (o->hasProperty ("productRecipeId"))
                m.productRecipeId = o->getProperty ("productRecipeId").toString();
            if (o->hasProperty ("productKindLabel"))
                m.productKindLabel = o->getProperty ("productKindLabel").toString();
            if (o->hasProperty ("whiteLabelPackageName"))
                m.whiteLabelPackageName = o->getProperty ("whiteLabelPackageName").toString();
            if (o->hasProperty ("whiteLabelPublisher"))
                m.whiteLabelPublisher = o->getProperty ("whiteLabelPublisher").toString();
            if (o->hasProperty ("whiteLabelProductCode"))
                m.whiteLabelProductCode = o->getProperty ("whiteLabelProductCode").toString();
            if (o->hasProperty ("whiteLabelBundleIdentifier"))
                m.whiteLabelBundleIdentifier = o->getProperty ("whiteLabelBundleIdentifier").toString();
            if (o->hasProperty ("whiteLabelInstallerId"))
                m.whiteLabelInstallerId = o->getProperty ("whiteLabelInstallerId").toString();
            if (o->hasProperty ("whiteLabelInstallerIcon"))
                m.whiteLabelInstallerIcon = o->getProperty ("whiteLabelInstallerIcon").toString();
            if (o->hasProperty ("whiteLabelEulaPath"))
                m.whiteLabelEulaPath = o->getProperty ("whiteLabelEulaPath").toString();
            if (o->hasProperty ("whiteLabelPrivacyUrl"))
                m.whiteLabelPrivacyUrl = o->getProperty ("whiteLabelPrivacyUrl").toString();
            if (o->hasProperty ("whiteLabelInstallNotes"))
                m.whiteLabelInstallNotes = o->getProperty ("whiteLabelInstallNotes").toString();
            if (o->hasProperty ("whiteLabelWindowsVst3Path"))
                m.whiteLabelWindowsVst3Path = o->getProperty ("whiteLabelWindowsVst3Path").toString();
            if (o->hasProperty ("whiteLabelMacVst3Path"))
                m.whiteLabelMacVst3Path = o->getProperty ("whiteLabelMacVst3Path").toString();
            if (o->hasProperty ("whiteLabelRequireLicenseOnFirstRun"))
                m.whiteLabelRequireLicenseOnFirstRun = (bool) o->getProperty ("whiteLabelRequireLicenseOnFirstRun");
            if (o->hasProperty ("whiteLabelIncludeStandalone"))
                m.whiteLabelIncludeStandalone = (bool) o->getProperty ("whiteLabelIncludeStandalone");
            if (o->hasProperty ("whiteLabelIncludeVst3"))
                m.whiteLabelIncludeVst3 = (bool) o->getProperty ("whiteLabelIncludeVst3");
            if (o->hasProperty ("licenseRequired"))
                m.licenseRequired = (bool) o->getProperty ("licenseRequired");
            if (o->hasProperty ("licenseKey"))
                m.licenseKey = o->getProperty ("licenseKey").toString();
            if (o->hasProperty ("licenseProductId"))
                m.licenseProductId = o->getProperty ("licenseProductId").toString();
            if (o->hasProperty ("licenseServerUrl"))
                m.licenseServerUrl = o->getProperty ("licenseServerUrl").toString();
            if (o->hasProperty ("licensePublicKey"))
                m.licensePublicKey = o->getProperty ("licensePublicKey").toString();
            if (o->hasProperty ("licensePolicy"))
                m.licensePolicy = o->getProperty ("licensePolicy").toString();
            if (o->hasProperty ("trialDays"))
                m.trialDays = (int) o->getProperty ("trialDays");
            if (o->hasProperty ("isTrial"))
                m.isTrial = (bool) o->getProperty ("isTrial");
            if (o->hasProperty ("trialExpiryDate"))
                m.trialExpiryDate = o->getProperty ("trialExpiryDate").toString();
            if (o->hasProperty ("licenseOwner"))
                m.licenseOwner = o->getProperty ("licenseOwner").toString();
            if (o->hasProperty ("licenseOfflineGraceDays"))
                m.licenseOfflineGraceDays = juce::jlimit (0, 365, (int) o->getProperty ("licenseOfflineGraceDays"));
            if (o->hasProperty ("licenseBindToMachine"))
                m.licenseBindToMachine = (bool) o->getProperty ("licenseBindToMachine");
            if (o->hasProperty ("licenseAllowTrialConversion"))
                m.licenseAllowTrialConversion = (bool) o->getProperty ("licenseAllowTrialConversion");

            if (auto* tagArray = o->getProperty ("tags").getArray())
            {
                for (auto& tag : *tagArray)
                    m.tags.add (tag.toString());
            }

            if (auto* instrumentIdArray = o->getProperty ("instrumentIds").getArray())
                for (auto& id : *instrumentIdArray)
                    m.instrumentIds.add (id.toString());
            if (auto* instrumentNameArray = o->getProperty ("instrumentNames").getArray())
                for (auto& name : *instrumentNameArray)
                    m.instrumentNames.add (name.toString());
            if (auto* instrumentFileArray = o->getProperty ("instrumentFiles").getArray())
                for (auto& file : *instrumentFileArray)
                    m.instrumentFiles.add (file.toString());
            readFloatArrayProperty (o, "instrumentVolumes", m.instrumentVolumes);
            readFloatArrayProperty (o, "instrumentPans", m.instrumentPans);
            readIntArrayProperty (o, "instrumentMidiChannels", m.instrumentMidiChannels);
            readIntArrayProperty (o, "instrumentOutputRoutes", m.instrumentOutputRoutes);
            readIntArrayProperty (o, "instrumentTransposeSemitones", m.instrumentTransposeSemitones);
            readIntArrayProperty (o, "instrumentEnabled", m.instrumentEnabled);
            readIntArrayProperty (o, "instrumentAutoPlay", m.instrumentAutoPlay);
            readIntArrayProperty (o, "instrumentAutoPlayNotes", m.instrumentAutoPlayNotes);
            readFloatArrayProperty (o, "instrumentAutoPlayVelocities", m.instrumentAutoPlayVelocities);

            if (m.formatVersion <= 0)   m.formatVersion = kFormatVersion;
            if (m.engine.isEmpty())     m.engine = "sample";
            if (m.createdWith.isEmpty())m.createdWith = "Player Builder";
            if (m.licensePolicy.isEmpty()) m.licensePolicy = "online-or-offline-grace";
            if (m.salesCurrency.isEmpty()) m.salesCurrency = "USD";
            if (m.salesCtaText.isEmpty()) m.salesCtaText = "Buy Now";
            if (m.playerDisplayName.isEmpty())
                m.playerDisplayName = m.instrumentName;
            if (m.whiteLabelWindowsVst3Path.isEmpty()) m.whiteLabelWindowsVst3Path = R"(CommonFilesFolder\VST3)";
            if (m.whiteLabelMacVst3Path.isEmpty()) m.whiteLabelMacVst3Path = "/Library/Audio/Plug-Ins/VST3";
        }
        return m;
    }

    // Element type strings -----------------------------------------------------
    juce::String elementTypeToString (ElementType t)
    {
        switch (t)
        {
            case ElementType::Image:        return "image";
            case ElementType::Knob:         return "knob";
            case ElementType::Slider:       return "slider";
            case ElementType::Button:       return "button";
            case ElementType::Toggle:       return "toggle";
            case ElementType::Dropdown:     return "dropdown";
            case ElementType::Label:        return "label";
            case ElementType::ValueDisplay: return "valueDisplay";
            case ElementType::Meter:        return "meter";
            case ElementType::Waveform:     return "waveform";
            case ElementType::Keyboard:     return "keyboard";
            case ElementType::Panel:        return "panel";
            case ElementType::Shape:        return "shape";
            case ElementType::XYPad:        return "xyPad";
            case ElementType::GranularField:return "granular";
            case ElementType::TabPanel:     return "tabPanel";
            case ElementType::ScrollPanel:  return "scrollPanel";
            case ElementType::Group:        return "group";
            case ElementType::Separator:    return "separator";
            case ElementType::DrumPad:      return "drumPad";
            case ElementType::PadGrid:      return "padGrid";
            case ElementType::DrumGrid:     return "drumGrid";
            case ElementType::ArpLane:      return "arpLane";
            case ElementType::SequencerLane: return "sequencerLane";
            case ElementType::PianoRoll:    return "pianoRoll";
            case ElementType::Mixer:        return "mixer";
            case ElementType::MacroControl: return "macroControl";
            case ElementType::ModMatrix:    return "modMatrix";
            case ElementType::EqCurve:      return "eqCurve";
            case ElementType::SpectrumAnalyzer: return "spectrumAnalyzer";
            case ElementType::ReactiveImage: return "reactiveImage";
            case ElementType::SpriteAnimator: return "spriteAnimator";
            case ElementType::VisualFxLayer: return "visualFxLayer";
            case ElementType::AiVisualPrompt: return "aiVisualPrompt";
            case ElementType::SampleDropZone: return "sampleDropZone";
            case ElementType::RuntimeSampleLibrary: return "runtimeSampleLibrary";
            case ElementType::PitchWheel: return "pitchWheel";
            case ElementType::ModWheel: return "modWheel";
            case ElementType::AdsrCurve: return "adsrCurve";
        }
        return "knob";
    }

    ElementType elementTypeFromString (const juce::String& s)
    {
        if (s == "image")        return ElementType::Image;
        if (s == "knob")         return ElementType::Knob;
        if (s == "slider")       return ElementType::Slider;
        if (s == "button")       return ElementType::Button;
        if (s == "toggle")       return ElementType::Toggle;
        if (s == "dropdown")     return ElementType::Dropdown;
        if (s == "label")        return ElementType::Label;
        if (s == "valueDisplay") return ElementType::ValueDisplay;
        if (s == "meter")        return ElementType::Meter;
        if (s == "waveform")     return ElementType::Waveform;
        if (s == "keyboard")     return ElementType::Keyboard;
        if (s == "panel")        return ElementType::Panel;
        if (s == "shape")        return ElementType::Shape;
        if (s == "xyPad")        return ElementType::XYPad;
        if (s == "granular" || s == "granularField") return ElementType::GranularField;
        if (s == "tabPanel")     return ElementType::TabPanel;
        if (s == "scrollPanel")  return ElementType::ScrollPanel;
        if (s == "group")        return ElementType::Group;
        if (s == "separator")    return ElementType::Separator;
        if (s == "drumPad")      return ElementType::DrumPad;
        if (s == "padGrid")      return ElementType::PadGrid;
        if (s == "drumGrid")     return ElementType::DrumGrid;
        if (s == "arpLane")      return ElementType::ArpLane;
        if (s == "sequencerLane") return ElementType::SequencerLane;
        if (s == "pianoRoll")    return ElementType::PianoRoll;
        if (s == "mixer")        return ElementType::Mixer;
        if (s == "macroControl") return ElementType::MacroControl;
        if (s == "modMatrix")    return ElementType::ModMatrix;
        if (s == "eqCurve")      return ElementType::EqCurve;
        if (s == "spectrumAnalyzer") return ElementType::SpectrumAnalyzer;
        if (s == "reactiveImage") return ElementType::ReactiveImage;
        if (s == "spriteAnimator") return ElementType::SpriteAnimator;
        if (s == "visualFxLayer") return ElementType::VisualFxLayer;
        if (s == "aiVisualPrompt") return ElementType::AiVisualPrompt;
        if (s == "sampleDropZone") return ElementType::SampleDropZone;
        if (s == "runtimeSampleLibrary") return ElementType::RuntimeSampleLibrary;
        if (s == "pitchWheel") return ElementType::PitchWheel;
        if (s == "modWheel") return ElementType::ModWheel;
        if (s == "adsrCurve") return ElementType::AdsrCurve;
        return ElementType::Knob;
    }

    juce::String elementTypeDisplayName (ElementType t)
    {
        switch (t)
        {
            case ElementType::Image:        return "Image";
            case ElementType::Knob:         return "Knob";
            case ElementType::Slider:       return "Slider";
            case ElementType::Button:       return "Button";
            case ElementType::Toggle:       return "Toggle";
            case ElementType::Dropdown:     return "Dropdown";
            case ElementType::Label:        return "Label";
            case ElementType::ValueDisplay: return "Value Display";
            case ElementType::Meter:        return "Meter";
            case ElementType::Waveform:     return "Waveform";
            case ElementType::Keyboard:     return "Keyboard";
            case ElementType::Panel:        return "Panel";
            case ElementType::Shape:        return "Shape";
            case ElementType::XYPad:        return "XY Pad";
            case ElementType::GranularField:return "Granular Field";
            case ElementType::TabPanel:     return "Tab Panel";
            case ElementType::ScrollPanel:  return "Scroll Panel";
            case ElementType::Group:        return "Group";
            case ElementType::Separator:    return "Separator";
            case ElementType::DrumPad:      return "Drum Pad";
            case ElementType::PadGrid:      return "Pad Grid";
            case ElementType::DrumGrid:     return "Drum Grid";
            case ElementType::ArpLane:      return "Arp Lane";
            case ElementType::SequencerLane: return "Sequencer Lane";
            case ElementType::PianoRoll:    return "Piano Roll";
            case ElementType::Mixer:        return "Mixer";
            case ElementType::MacroControl: return "Macro Control";
            case ElementType::ModMatrix:    return "Mod Matrix";
            case ElementType::EqCurve:      return "EQ Curve";
            case ElementType::SpectrumAnalyzer: return "Spectrum Analyzer";
            case ElementType::ReactiveImage: return "Reactive Image";
            case ElementType::SpriteAnimator: return "Sprite Animator";
            case ElementType::VisualFxLayer: return "Visual FX Layer";
            case ElementType::AiVisualPrompt: return "AI Visual Prompt";
            case ElementType::SampleDropZone: return "Sample Drop Zone";
            case ElementType::RuntimeSampleLibrary: return "Runtime Sample Library";
            case ElementType::PitchWheel: return "Pitch Wheel";
            case ElementType::ModWheel: return "Mod Wheel";
            case ElementType::AdsrCurve: return "ADSR Curve";
        }
        return "Knob";
    }

    bool isRuntimeControlElement (ElementType type)
    {
        return type == ElementType::Knob
            || type == ElementType::Slider
            || type == ElementType::Button
            || type == ElementType::Toggle
            || type == ElementType::Dropdown
            || type == ElementType::ValueDisplay
            || type == ElementType::MacroControl
            || type == ElementType::PitchWheel
            || type == ElementType::ModWheel;
    }

    bool isPlayerRuntimeElementSupported (ElementType type)
    {
        return type == ElementType::Image
            || type == ElementType::Knob
            || type == ElementType::Slider
            || type == ElementType::Button
            || type == ElementType::Toggle
            || type == ElementType::Dropdown
            || type == ElementType::Label
            || type == ElementType::ValueDisplay
            || type == ElementType::Meter
            || type == ElementType::Waveform
            || type == ElementType::Keyboard
            || type == ElementType::Panel
            || type == ElementType::Shape
            || type == ElementType::XYPad
            || type == ElementType::GranularField
            || type == ElementType::TabPanel
            || type == ElementType::ScrollPanel
            || type == ElementType::Group
            || type == ElementType::Separator
            || type == ElementType::DrumPad
            || type == ElementType::PadGrid
            || type == ElementType::DrumGrid
            || type == ElementType::ArpLane
            || type == ElementType::PianoRoll
            || type == ElementType::Mixer
            || type == ElementType::MacroControl
            || type == ElementType::ModMatrix
            || type == ElementType::EqCurve
            || type == ElementType::SpectrumAnalyzer
            || type == ElementType::ReactiveImage
            || type == ElementType::SpriteAnimator
            || type == ElementType::VisualFxLayer
            || type == ElementType::AiVisualPrompt
            || type == ElementType::SampleDropZone
            || type == ElementType::RuntimeSampleLibrary
            || type == ElementType::PitchWheel
            || type == ElementType::ModWheel
            || type == ElementType::AdsrCurve;
    }

    // LayoutElement ------------------------------------------------------------
    juce::String LayoutElement::tabLabelToGroupId (const juce::String& label)
    {
        return label.toLowerCase().replace (" ", "_");
    }

    juce::var LayoutElement::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id",          id);
        obj->setProperty ("type",        elementTypeToString (type));
        obj->setProperty ("parameterId", parameterId);
        obj->setProperty ("label",       label);
        obj->setProperty ("style",       style);
        obj->setProperty ("knobStyle",   knobStyle);
        obj->setProperty ("valueFormat", valueFormat);
        obj->setProperty ("asset",       asset);
        if (action.isNotEmpty())
            obj->setProperty ("action",  action);
        if (semanticRole.isNotEmpty())
            obj->setProperty ("semanticRole", semanticRole);
        obj->setProperty ("shapeKind",   shapeKind);
        obj->setProperty ("labelPosition", labelPosition);
        obj->setProperty ("containerId", containerId);
        obj->setProperty ("linkedCopyGroupId", linkedCopyGroupId);
        if (pscriptFile.isNotEmpty())
            obj->setProperty ("pscriptFile", pscriptFile);
        obj->setProperty ("x",           x);
        obj->setProperty ("y",           y);
        obj->setProperty ("width",       width);
        obj->setProperty ("height",      height);
        obj->setProperty ("visible",     visible);
        obj->setProperty ("locked",      locked);
        obj->setProperty ("opacity",     (double) opacity);
        obj->setProperty ("cornerRadius", (double) cornerRadius);
        obj->setProperty ("strokeWidth",  (double) strokeWidth);
        obj->setProperty ("shadowAmount", (double) shadowAmount);
        obj->setProperty ("glowAmount",   (double) glowAmount);
        obj->setProperty ("blurAmount",   (double) blurAmount);
        obj->setProperty ("labelOffsetX", (double) labelOffsetX);
        obj->setProperty ("labelOffsetY", (double) labelOffsetY);
        obj->setProperty ("labelSpacing", (double) labelSpacing);
        obj->setProperty ("labelSize",    (double) labelSize);
        obj->setProperty ("contentPadding", (double) contentPadding);
        obj->setProperty ("controlPreviewValue", (double) controlPreviewValue);
        obj->setProperty ("audioReactive", audioReactive);
        obj->setProperty ("audioReactiveMode", audioReactiveMode);
        obj->setProperty ("audioReactiveAmount", (double) audioReactiveAmount);
        obj->setProperty ("animationMode", animationMode);
        obj->setProperty ("animationRate", (double) animationRate);
        obj->setProperty ("visualSource", visualSource);
        obj->setProperty ("visualAction", visualAction);
        obj->setProperty ("visualPreset", visualPreset);
        if (visualAiPrompt.isNotEmpty())
            obj->setProperty ("visualAiPrompt", visualAiPrompt);
        if (visualAiStyle.isNotEmpty())
            obj->setProperty ("visualAiStyle", visualAiStyle);
        obj->setProperty ("visualRequiresPro", visualRequiresPro);
        obj->setProperty ("visualAiGenerated", visualAiGenerated);
        obj->setProperty ("visualLowPowerFallback", visualLowPowerFallback);
        obj->setProperty ("textColour",       colourToString (textColour));
        obj->setProperty ("accentColour",     colourToString (accentColour));
        obj->setProperty ("borderColour",     colourToString (borderColour));
        obj->setProperty ("backgroundColour", colourToString (backgroundColour));
        obj->setProperty ("groupId",     groupId);

        if (! tabs.isEmpty())
        {
            juce::Array<juce::var> arr;
            for (auto& t : tabs) arr.add (t);
            obj->setProperty ("tabs", arr);
        }

        if (filmstripAsset.isNotEmpty())
        {
            obj->setProperty ("filmstripAsset",    filmstripAsset);
            obj->setProperty ("filmstripFrames",   filmstripFrames);
            obj->setProperty ("filmstripVertical", filmstripVertical);
        }

        if (type == ElementType::DrumPad || type == ElementType::PadGrid)
        {
            obj->setProperty ("padRows",     padRows);
            obj->setProperty ("padCols",     padCols);
            obj->setProperty ("padBaseNote", padBaseNote);
        }
        if (type == ElementType::DrumGrid)
        {
            obj->setProperty ("drumTracks",  drumTracks);
            obj->setProperty ("drumSteps",   drumSteps);
            obj->setProperty ("drumPattern", drumPattern);
        }
        if (type == ElementType::PianoRoll)
        {
            obj->setProperty ("pianoRollSteps",        pianoRollSteps);
            obj->setProperty ("pianoRollStepsPerBeat", pianoRollStepsPerBeat);
            obj->setProperty ("pianoRollLowNote",      pianoRollLowNote);
            obj->setProperty ("pianoRollRows",         pianoRollRows);
        }
        if (type == ElementType::ArpLane)
        {
            obj->setProperty ("arpLaneIndex", arpLaneIndex);
            obj->setProperty ("arpLaneSteps", arpLaneSteps);
            obj->setProperty ("arpLaneMode",  arpLaneMode);
            obj->setProperty ("arpLaneTarget", arpLaneTarget);
            obj->setProperty ("arpLaneRootNote", arpLaneRootNote);
            obj->setProperty ("arpLaneSampleSlots", arpLaneSampleSlots);
            obj->setProperty ("arpLaneDirection", arpLaneDirection);
            obj->setProperty ("arpLaneRotate", arpLaneRotate);
            obj->setProperty ("arpLaneEuclideanPulses", arpLaneEuclideanPulses);
            obj->setProperty ("arpLaneProbability", (double) arpLaneProbability);
            obj->setProperty ("arpLaneRatchet", arpLaneRatchet);
            obj->setProperty ("arpLaneFillPulses", arpLaneFillPulses);
            obj->setProperty ("arpLaneFillProbability", (double) arpLaneFillProbability);
        }
        else if (type == ElementType::SequencerLane)
        {
            obj->setProperty ("seqLaneIndex", seqLaneIndex);
            obj->setProperty ("seqLaneSteps", seqLaneSteps);
            obj->setProperty ("seqLaneType", seqLaneType);
            obj->setProperty ("seqLaneDirection", seqLaneDirection);
            obj->setProperty ("seqLaneTarget", seqLaneTarget);
            obj->setProperty ("seqLaneColour", seqLaneColour.toDisplayString (true));
        }
        if (type == ElementType::Mixer)
        {
            obj->setProperty ("mixerChannels", mixerChannels);
            obj->setProperty ("mixerMode",     mixerMode);
            if (! mixerChannelLabels.isEmpty()) obj->setProperty ("mixerChannelLabels", stringArrayToVar (mixerChannelLabels));
            if (! mixerVolumeParams.isEmpty())  obj->setProperty ("mixerVolumeParams",  stringArrayToVar (mixerVolumeParams));
            if (! mixerPanParams.isEmpty())     obj->setProperty ("mixerPanParams",     stringArrayToVar (mixerPanParams));
            if (! mixerMuteParams.isEmpty())    obj->setProperty ("mixerMuteParams",    stringArrayToVar (mixerMuteParams));
            if (! mixerSoloParams.isEmpty())    obj->setProperty ("mixerSoloParams",    stringArrayToVar (mixerSoloParams));
        }
        return juce::var (obj);
    }

    LayoutElement LayoutElement::fromVar (const juce::var& v)
    {
        LayoutElement e;
        if (auto* o = v.getDynamicObject())
        {
            e.id          = o->getProperty ("id").toString();
            e.type        = elementTypeFromString (o->getProperty ("type").toString());
            e.parameterId = o->getProperty ("parameterId").toString();
            e.label       = o->getProperty ("label").toString();
            e.style       = o->getProperty ("style").toString();
            e.knobStyle   = o->getProperty ("knobStyle").toString();
            e.valueFormat = o->getProperty ("valueFormat").toString();
            e.asset       = o->getProperty ("asset").toString();
            e.action      = o->getProperty ("action").toString();
            e.semanticRole = o->getProperty ("semanticRole").toString();
            e.shapeKind   = o->getProperty ("shapeKind").toString();
            e.labelPosition = o->getProperty ("labelPosition").toString();
            e.containerId = o->getProperty ("containerId").toString();
            e.linkedCopyGroupId = o->getProperty ("linkedCopyGroupId").toString();
            e.pscriptFile = o->getProperty ("pscriptFile").toString();
            e.x      = (int) o->getProperty ("x");
            e.y      = (int) o->getProperty ("y");
            e.width  = (int) o->getProperty ("width");
            e.height = (int) o->getProperty ("height");
            if (o->hasProperty ("visible")) e.visible = (bool) o->getProperty ("visible");
            if (o->hasProperty ("locked"))  e.locked  = (bool) o->getProperty ("locked");
            if (o->hasProperty ("opacity")) e.opacity = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("opacity"));
            if (o->hasProperty ("cornerRadius")) e.cornerRadius = juce::jmax (0.0f, (float) (double) o->getProperty ("cornerRadius"));
            if (o->hasProperty ("strokeWidth"))  e.strokeWidth  = juce::jmax (0.0f, (float) (double) o->getProperty ("strokeWidth"));
            if (o->hasProperty ("shadowAmount")) e.shadowAmount = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("shadowAmount"));
            if (o->hasProperty ("glowAmount"))   e.glowAmount   = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("glowAmount"));
            if (o->hasProperty ("blurAmount"))   e.blurAmount   = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("blurAmount"));
            if (o->hasProperty ("labelOffsetX")) e.labelOffsetX = (float) (double) o->getProperty ("labelOffsetX");
            if (o->hasProperty ("labelOffsetY")) e.labelOffsetY = (float) (double) o->getProperty ("labelOffsetY");
            if (o->hasProperty ("labelSpacing")) e.labelSpacing = (float) (double) o->getProperty ("labelSpacing");
            if (o->hasProperty ("labelSize"))    e.labelSize    = juce::jmax (0.0f, (float) (double) o->getProperty ("labelSize"));
            if (o->hasProperty ("contentPadding")) e.contentPadding = (float) (double) o->getProperty ("contentPadding");
            if (o->hasProperty ("controlPreviewValue")) e.controlPreviewValue = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("controlPreviewValue"));
            if (o->hasProperty ("audioReactive")) e.audioReactive = (bool) o->getProperty ("audioReactive");
            e.audioReactiveMode = o->getProperty ("audioReactiveMode").toString();
            if (o->hasProperty ("audioReactiveAmount")) e.audioReactiveAmount = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("audioReactiveAmount"));
            e.animationMode = o->getProperty ("animationMode").toString();
            if (o->hasProperty ("animationRate")) e.animationRate = juce::jmax (0.01f, (float) (double) o->getProperty ("animationRate"));
            if (o->hasProperty ("visualSource")) e.visualSource = o->getProperty ("visualSource").toString();
            if (o->hasProperty ("visualAction")) e.visualAction = o->getProperty ("visualAction").toString();
            if (o->hasProperty ("visualPreset")) e.visualPreset = o->getProperty ("visualPreset").toString();
            if (o->hasProperty ("visualAiPrompt")) e.visualAiPrompt = o->getProperty ("visualAiPrompt").toString();
            if (o->hasProperty ("visualAiStyle")) e.visualAiStyle = o->getProperty ("visualAiStyle").toString();
            if (o->hasProperty ("visualRequiresPro")) e.visualRequiresPro = (bool) o->getProperty ("visualRequiresPro");
            if (o->hasProperty ("visualAiGenerated")) e.visualAiGenerated = (bool) o->getProperty ("visualAiGenerated");
            if (o->hasProperty ("visualLowPowerFallback")) e.visualLowPowerFallback = (bool) o->getProperty ("visualLowPowerFallback");
            e.textColour       = colourFromString (o->getProperty ("textColour").toString(),       e.textColour);
            e.accentColour     = colourFromString (o->getProperty ("accentColour").toString(),     e.accentColour);
            e.borderColour     = colourFromString (o->getProperty ("borderColour").toString(),     e.borderColour);
            e.backgroundColour = colourFromString (o->getProperty ("backgroundColour").toString(), e.backgroundColour);
            e.groupId          = o->getProperty ("groupId").toString();
            if (auto* tabs = o->getProperty ("tabs").getArray())
                for (auto& t : *tabs) e.tabs.add (t.toString());

            e.filmstripAsset  = o->getProperty ("filmstripAsset").toString();
            e.filmstripFrames = (int) o->getProperty ("filmstripFrames");
            if (o->hasProperty ("filmstripVertical"))
                e.filmstripVertical = (bool) o->getProperty ("filmstripVertical");

            if (o->hasProperty ("padRows"))
                e.padRows = juce::jlimit (1, 8, (int) o->getProperty ("padRows"));
            if (o->hasProperty ("padCols"))
                e.padCols = juce::jlimit (1, 8, (int) o->getProperty ("padCols"));
            if (o->hasProperty ("padBaseNote"))
                e.padBaseNote = juce::jlimit (0, 127, (int) o->getProperty ("padBaseNote"));
            if (o->hasProperty ("drumTracks"))
                e.drumTracks = juce::jlimit (1, 16, (int) o->getProperty ("drumTracks"));
            if (o->hasProperty ("drumSteps"))
                e.drumSteps = juce::jlimit (1, 64, (int) o->getProperty ("drumSteps"));
            if (o->hasProperty ("drumPattern"))
                e.drumPattern = juce::jlimit (0, 7, (int) o->getProperty ("drumPattern"));
            if (o->hasProperty ("arpLaneIndex"))
                e.arpLaneIndex = juce::jlimit (0, 15, (int) o->getProperty ("arpLaneIndex"));
            if (o->hasProperty ("arpLaneSteps"))
                e.arpLaneSteps = juce::jlimit (1, 128, (int) o->getProperty ("arpLaneSteps"));
            if (o->hasProperty ("arpLaneMode"))
                e.arpLaneMode = o->getProperty ("arpLaneMode").toString();
            if (o->hasProperty ("arpLaneTarget"))
                e.arpLaneTarget = o->getProperty ("arpLaneTarget").toString();
            if (o->hasProperty ("arpLaneRootNote"))
                e.arpLaneRootNote = juce::jlimit (0, 127, (int) o->getProperty ("arpLaneRootNote"));
            if (o->hasProperty ("arpLaneSampleSlots"))
                e.arpLaneSampleSlots = juce::jlimit (1, 64, (int) o->getProperty ("arpLaneSampleSlots"));
            if (o->hasProperty ("arpLaneDirection"))
                e.arpLaneDirection = o->getProperty ("arpLaneDirection").toString();
            if (o->hasProperty ("arpLaneRotate"))
                e.arpLaneRotate = juce::jlimit (0, 127, (int) o->getProperty ("arpLaneRotate"));
            if (o->hasProperty ("arpLaneEuclideanPulses"))
                e.arpLaneEuclideanPulses = juce::jlimit (0, 128, (int) o->getProperty ("arpLaneEuclideanPulses"));
            if (o->hasProperty ("arpLaneProbability"))
                e.arpLaneProbability = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("arpLaneProbability"));
            if (o->hasProperty ("arpLaneRatchet"))
                e.arpLaneRatchet = juce::jlimit (1, 8, (int) o->getProperty ("arpLaneRatchet"));
            if (o->hasProperty ("arpLaneFillPulses"))
                e.arpLaneFillPulses = juce::jlimit (0, 128, (int) o->getProperty ("arpLaneFillPulses"));
            if (o->hasProperty ("arpLaneFillProbability"))
                e.arpLaneFillProbability = juce::jlimit (0.0f, 1.0f, (float) (double) o->getProperty ("arpLaneFillProbability"));
        }
        else if (e.type == ElementType::SequencerLane)
        {
            if (o->hasProperty ("seqLaneIndex"))
                e.seqLaneIndex = juce::jlimit (0, 15, (int) o->getProperty ("seqLaneIndex"));
            if (o->hasProperty ("seqLaneSteps"))
                e.seqLaneSteps = juce::jlimit (1, 64, (int) o->getProperty ("seqLaneSteps"));
            if (o->hasProperty ("seqLaneType"))
                e.seqLaneType = o->getProperty ("seqLaneType").toString();
            if (o->hasProperty ("seqLaneDirection"))
                e.seqLaneDirection = o->getProperty ("seqLaneDirection").toString();
            if (o->hasProperty ("seqLaneTarget"))
                e.seqLaneTarget = o->getProperty ("seqLaneTarget").toString();
            if (o->hasProperty ("seqLaneColour"))
                e.seqLaneColour = juce::Colour::fromString (o->getProperty ("seqLaneColour").toString());
        }
        else if (e.type == ElementType::PianoRoll)
        {
            if (o->hasProperty ("pianoRollSteps"))
                e.pianoRollSteps = juce::jlimit (1, 256, (int) o->getProperty ("pianoRollSteps"));
            if (o->hasProperty ("pianoRollStepsPerBeat"))
                e.pianoRollStepsPerBeat = juce::jlimit (1, 16, (int) o->getProperty ("pianoRollStepsPerBeat"));
            if (o->hasProperty ("pianoRollLowNote"))
                e.pianoRollLowNote = juce::jlimit (0, 120, (int) o->getProperty ("pianoRollLowNote"));
            if (o->hasProperty ("pianoRollRows"))
                e.pianoRollRows = juce::jlimit (4, 88, (int) o->getProperty ("pianoRollRows"));
        }
        else if (e.type == ElementType::Mixer)
        {
            if (o->hasProperty ("mixerChannels"))
                e.mixerChannels = juce::jlimit (1, 16, (int) o->getProperty ("mixerChannels"));
            if (o->hasProperty ("mixerMode"))
                e.mixerMode = o->getProperty ("mixerMode").toString();
            readStringArrayProperty (o, "mixerChannelLabels", e.mixerChannelLabels);
            readStringArrayProperty (o, "mixerVolumeParams",  e.mixerVolumeParams);
            readStringArrayProperty (o, "mixerPanParams",     e.mixerPanParams);
            readStringArrayProperty (o, "mixerMuteParams",    e.mixerMuteParams);
            readStringArrayProperty (o, "mixerSoloParams",    e.mixerSoloParams);

            if (e.style.isEmpty()) e.style = "Modern Dark";
            if (e.knobStyle.isEmpty()) e.knobStyle = "Vintage 01";
            if (e.valueFormat.isEmpty()) e.valueFormat = "Auto";
            if (e.shapeKind.isEmpty()) e.shapeKind = "roundedRect";
            if (e.labelPosition.isEmpty()) e.labelPosition = "bottom";
            if (e.audioReactiveMode.isEmpty()) e.audioReactiveMode = "level";
            if (e.animationMode.isEmpty()) e.animationMode = "none";
            if (e.visualSource.isEmpty()) e.visualSource = "audioLevel";
            if (e.visualAction.isEmpty()) e.visualAction = "pulseGlow";
            if (e.visualPreset.isEmpty()) e.visualPreset = "orbitAura";
            if (e.visualAiStyle.isEmpty()) e.visualAiStyle = "clean instrument artwork";
            if (e.arpLaneMode.isEmpty()) e.arpLaneMode = "bank";
            if (e.arpLaneTarget.isEmpty()) e.arpLaneTarget = "notes";
            if (e.arpLaneDirection.isEmpty()) e.arpLaneDirection = "forward";
            e.arpLaneSampleSlots = juce::jlimit (1, 64, e.arpLaneSampleSlots);
            e.arpLaneEuclideanPulses = juce::jlimit (0, e.arpLaneSteps, e.arpLaneEuclideanPulses);
            e.arpLaneProbability = juce::jlimit (0.0f, 1.0f, e.arpLaneProbability);
            e.arpLaneRatchet = juce::jlimit (1, 8, e.arpLaneRatchet);
            e.arpLaneFillPulses = juce::jlimit (0, e.arpLaneSteps, e.arpLaneFillPulses);
            e.arpLaneFillProbability = juce::jlimit (0.0f, 1.0f, e.arpLaneFillProbability);
            if (e.mixerMode.isEmpty()) e.mixerMode = "auto";
        }
        return e;
    }

    // ParameterDef -------------------------------------------------------------
    juce::var ParameterDef::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id",      id);
        obj->setProperty ("name",    name);
        obj->setProperty ("type",    type);
        obj->setProperty ("unit",    unit);
        obj->setProperty ("min",     (double) min);
        obj->setProperty ("max",     (double) max);
        obj->setProperty ("default", (double) defaultValue);
        obj->setProperty ("step",    (double) step);
        obj->setProperty ("smoothing", (double) smoothing);
        obj->setProperty ("category", category);
        obj->setProperty ("section", section);
        obj->setProperty ("displayMode", displayMode);
        obj->setProperty ("enabledBy", enabledBy);
        obj->setProperty ("enableHint", enableHint);
        obj->setProperty ("hostAutomatable", hostAutomatable);
        obj->setProperty ("midiLearnable", midiLearnable);
        obj->setProperty ("modulatable", modulatable);
        obj->setProperty ("visible", visible);
        return juce::var (obj);
    }

    ParameterDef ParameterDef::fromVar (const juce::var& v)
    {
        ParameterDef p;
        if (auto* o = v.getDynamicObject())
        {
            p.id   = o->getProperty ("id").toString();
            p.name = o->getProperty ("name").toString();
            p.type = o->getProperty ("type").toString();
            p.unit = o->getProperty ("unit").toString();
            p.min          = (float) (double) o->getProperty ("min");
            p.max          = (float) (double) o->getProperty ("max");
            p.defaultValue = (float) (double) o->getProperty ("default");
            p.step         = (float) (double) o->getProperty ("step");
            if (o->hasProperty ("smoothing"))
                p.smoothing = (float) (double) o->getProperty ("smoothing");
            if (o->hasProperty ("category"))
                p.category = o->getProperty ("category").toString();
            if (o->hasProperty ("section"))
                p.section = o->getProperty ("section").toString();
            if (o->hasProperty ("displayMode"))
                p.displayMode = o->getProperty ("displayMode").toString();
            if (o->hasProperty ("enabledBy"))
                p.enabledBy = o->getProperty ("enabledBy").toString();
            if (o->hasProperty ("enableHint"))
                p.enableHint = o->getProperty ("enableHint").toString();
            if (o->hasProperty ("hostAutomatable"))
                p.hostAutomatable = (bool) o->getProperty ("hostAutomatable");
            if (o->hasProperty ("midiLearnable"))
                p.midiLearnable = (bool) o->getProperty ("midiLearnable");
            if (o->hasProperty ("modulatable"))
                p.modulatable = (bool) o->getProperty ("modulatable");
            if (o->hasProperty ("visible"))
                p.visible = (bool) o->getProperty ("visible");
            if (p.type.isEmpty())
                p.type = "float";
            if (p.category.isEmpty())
                p.category = "General";
            if (p.section.isEmpty())
                p.section = "global";
            if (p.displayMode.isEmpty())
                p.displayMode = "continuous";
            if (p.max <= p.min)
                p.max = p.min + 1.0f;
            if (p.name.isEmpty())
                p.name = p.id;
        }
        return p;
    }

    juce::var HostParameterSlot::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("slotIndex", slotIndex);
        obj->setProperty ("slotId", slotId);
        obj->setProperty ("parameterId", parameterId);
        obj->setProperty ("name", name);
        obj->setProperty ("section", section);
        obj->setProperty ("category", category);
        obj->setProperty ("unit", unit);
        obj->setProperty ("min", (double) min);
        obj->setProperty ("max", (double) max);
        obj->setProperty ("default", (double) defaultValue);
        obj->setProperty ("midiLearnable", midiLearnable);
        obj->setProperty ("overflow", overflow);
        return juce::var (obj);
    }

    HostParameterSlot HostParameterSlot::fromVar (const juce::var& v)
    {
        HostParameterSlot slot;
        if (auto* o = v.getDynamicObject())
        {
            slot.slotIndex = (int) o->getProperty ("slotIndex");
            slot.slotId = o->getProperty ("slotId").toString();
            slot.parameterId = o->getProperty ("parameterId").toString();
            slot.name = o->getProperty ("name").toString();
            slot.section = o->getProperty ("section").toString();
            slot.category = o->getProperty ("category").toString();
            slot.unit = o->getProperty ("unit").toString();
            slot.min = (float) (double) o->getProperty ("min");
            slot.max = (float) (double) o->getProperty ("max");
            slot.defaultValue = (float) (double) o->getProperty ("default");
            if (o->hasProperty ("midiLearnable"))
                slot.midiLearnable = (bool) o->getProperty ("midiLearnable");
            if (o->hasProperty ("overflow"))
                slot.overflow = (bool) o->getProperty ("overflow");
            if (slot.slotId.isEmpty() && slot.slotIndex >= 0 && ! slot.overflow)
                slot.slotId = "p" + juce::String (slot.slotIndex);
            if (slot.name.isEmpty())
                slot.name = slot.parameterId;
            if (slot.max <= slot.min)
                slot.max = slot.min + 1.0f;
        }
        return slot;
    }

    // SampleZoneDef ------------------------------------------------------------
    juce::var SampleZoneDef::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("sample",       samplePath);
        obj->setProperty ("rootNote",     rootNote);
        obj->setProperty ("lowNote",      lowNote);
        obj->setProperty ("highNote",     highNote);
        obj->setProperty ("lowVelocity",  lowVelocity);
        obj->setProperty ("highVelocity", highVelocity);
        obj->setProperty ("gainDb",       (double) gainDb);
        obj->setProperty ("pan",          (double) pan);
        obj->setProperty ("loopEnabled",  loopEnabled);
        obj->setProperty ("loopStart",    loopStart);
        obj->setProperty ("loopEnd",      loopEnd);
        // HISE-style advanced features
        obj->setProperty ("roundRobinGroup", roundRobinGroup);
        obj->setProperty ("roundRobinIndex", roundRobinIndex);
        obj->setProperty ("sampleStart",    sampleStart);
        obj->setProperty ("sampleEnd",      sampleEnd);
        obj->setProperty ("fadeInStart",    fadeInStart);
        obj->setProperty ("fadeInLength",   fadeInLength);
        obj->setProperty ("fadeOutStart",   fadeOutStart);
        obj->setProperty ("fadeOutLength",  fadeOutLength);
        obj->setProperty ("pitchOffset",    (double) pitchOffset);
        obj->setProperty ("keyTracking",    (double) keyTracking);
        obj->setProperty ("velocityLowerVelXFade", (double) velocityLowerVelXFade);
        obj->setProperty ("velocityUpperVelXFade", (double) velocityUpperVelXFade);
        obj->setProperty ("reverse",        reverse);
        obj->setProperty ("priority",       priority);
        obj->setProperty ("group",          group);
        obj->setProperty ("bpm",           (double) bpm);
        obj->setProperty ("padIndex",       padIndex);
        obj->setProperty ("padLabel",       padLabel);
        obj->setProperty ("chokeGroup",     chokeGroup);
        obj->setProperty ("oneShot",        oneShot);
        obj->setProperty ("triggerProbability", triggerProbability);
        obj->setProperty ("playMode",       playMode);
        if (! cuePoints.empty())
        {
            juce::Array<juce::var> cueArray;
            for (int cue : cuePoints)
                cueArray.add (cue);
            obj->setProperty ("cuePoints", cueArray);
        }
        obj->setProperty ("midiPath",          midiPath);
        obj->setProperty ("midiPlaybackMode",  midiPlaybackMode);
        obj->setProperty ("midiHostSync",      midiHostSync);
        obj->setProperty ("midiTranspose",     midiTranspose);
        obj->setProperty ("midiVelocityAmount", (double) midiVelocityAmount);
        return juce::var (obj);
    }

    SampleZoneDef SampleZoneDef::fromVar (const juce::var& v)
    {
        SampleZoneDef z;
        if (auto* o = v.getDynamicObject())
        {
            z.samplePath   = o->getProperty ("sample").toString();
            z.rootNote     = (int) o->getProperty ("rootNote");
            z.lowNote      = (int) o->getProperty ("lowNote");
            z.highNote     = (int) o->getProperty ("highNote");
            z.lowVelocity  = (int) o->getProperty ("lowVelocity");
            z.highVelocity = (int) o->getProperty ("highVelocity");
            z.gainDb       = (float) (double) o->getProperty ("gainDb");
            z.pan          = (float) (double) o->getProperty ("pan");
            z.loopEnabled  = (bool) o->getProperty ("loopEnabled");
            z.loopStart    = (int) o->getProperty ("loopStart");
            z.loopEnd      = (int) o->getProperty ("loopEnd");
            // HISE-style advanced features
            if (o->hasProperty ("roundRobinGroup"))
                z.roundRobinGroup = (int) o->getProperty ("roundRobinGroup");
            if (o->hasProperty ("roundRobinIndex"))
                z.roundRobinIndex = (int) o->getProperty ("roundRobinIndex");
            if (o->hasProperty ("sampleStart"))
                z.sampleStart = (int) o->getProperty ("sampleStart");
            if (o->hasProperty ("sampleEnd"))
                z.sampleEnd = (int) o->getProperty ("sampleEnd");
            if (o->hasProperty ("fadeInStart"))
                z.fadeInStart = (int) o->getProperty ("fadeInStart");
            if (o->hasProperty ("fadeInLength"))
                z.fadeInLength = (int) o->getProperty ("fadeInLength");
            if (o->hasProperty ("fadeOutStart"))
                z.fadeOutStart = (int) o->getProperty ("fadeOutStart");
            if (o->hasProperty ("fadeOutLength"))
                z.fadeOutLength = (int) o->getProperty ("fadeOutLength");
            if (o->hasProperty ("pitchOffset"))
                z.pitchOffset = (float) (double) o->getProperty ("pitchOffset");
            if (o->hasProperty ("keyTracking"))
                z.keyTracking = (float) (double) o->getProperty ("keyTracking");
            if (o->hasProperty ("velocityLowerVelXFade"))
                z.velocityLowerVelXFade = (float) (double) o->getProperty ("velocityLowerVelXFade");
            if (o->hasProperty ("velocityUpperVelXFade"))
                z.velocityUpperVelXFade = (float) (double) o->getProperty ("velocityUpperVelXFade");
            if (o->hasProperty ("reverse"))
                z.reverse = (bool) o->getProperty ("reverse");
            if (o->hasProperty ("priority"))
                z.priority = (int) o->getProperty ("priority");
            if (o->hasProperty ("group"))
                z.group = o->getProperty ("group").toString();
            if (o->hasProperty ("padIndex"))
                z.padIndex = (int) o->getProperty ("padIndex");
            if (o->hasProperty ("padLabel"))
                z.padLabel = o->getProperty ("padLabel").toString();
            if (o->hasProperty ("chokeGroup"))
                z.chokeGroup = (int) o->getProperty ("chokeGroup");
            if (o->hasProperty ("oneShot"))
                z.oneShot = (bool) o->getProperty ("oneShot");
            if (o->hasProperty ("triggerProbability"))
                z.triggerProbability = (int) o->getProperty ("triggerProbability");
            if (o->hasProperty ("playMode"))
                z.playMode = (int) o->getProperty ("playMode");
            if (o->hasProperty ("cuePoints"))
            {
                if (auto* cueArray = o->getProperty ("cuePoints").getArray())
                    for (const auto& cue : *cueArray)
                        z.cuePoints.push_back ((int) cue);
            }
            if (o->hasProperty ("bpm"))
                z.bpm = (float) (double) o->getProperty ("bpm");
            if (o->hasProperty ("midiPath"))
                z.midiPath = o->getProperty ("midiPath").toString();
            if (o->hasProperty ("midiPlaybackMode"))
                z.midiPlaybackMode = o->getProperty ("midiPlaybackMode").toString();
            if (o->hasProperty ("midiHostSync"))
                z.midiHostSync = (bool) o->getProperty ("midiHostSync");
            if (o->hasProperty ("midiTranspose"))
                z.midiTranspose = (int) o->getProperty ("midiTranspose");
            if (o->hasProperty ("midiVelocityAmount"))
                z.midiVelocityAmount = (float) (double) o->getProperty ("midiVelocityAmount");
            if (z.lowVelocity  <= 0)   z.lowVelocity  = 1;
            if (z.highVelocity <= 0)   z.highVelocity = 127;
            if (z.highNote     <= 0)   z.highNote     = 127;
            z.padIndex = juce::jlimit (-1, kMaxChopPads - 1, z.padIndex);
            z.chokeGroup = juce::jlimit (0, 127, z.chokeGroup);
            z.triggerProbability = juce::jlimit (0, 100, z.triggerProbability);
            z.playMode = juce::jlimit (-1, 3, z.playMode);
            z.keyTracking = juce::jlimit (0.0f, 2.0f, z.keyTracking);
            if (! (z.midiPlaybackMode == "trigger" || z.midiPlaybackMode == "pitch"
                   || z.midiPlaybackMode == "slice" || z.midiPlaybackMode == "drum"
                   || z.midiPlaybackMode == "mod"))
                z.midiPlaybackMode = "trigger";
            z.midiTranspose = juce::jlimit (-48, 48, z.midiTranspose);
            z.midiVelocityAmount = juce::jlimit (0.0f, 1.0f, z.midiVelocityAmount);
        }
        return z;
    }

    // Preset -------------------------------------------------------------------
    juce::var Preset::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("name",        name);
        obj->setProperty ("description", description);
        obj->setProperty ("theme",       theme);
        obj->setProperty ("patchId",     patchId);
        obj->setProperty ("expansionId", expansionId);
        obj->setProperty ("packId",      packId);
        juce::Array<juce::var> libraryReferenceArray;
        for (const auto& ref : libraryReferences)
            libraryReferenceArray.add (ref);
        obj->setProperty ("libraryReferences", libraryReferenceArray);
        juce::Array<juce::var> tagArray;
        for (const auto& tag : tags)
            tagArray.add (tag);
        obj->setProperty ("tags",        tagArray);
        obj->setProperty ("generated",   generated);
        obj->setProperty ("isDefault",   isDefault);

        auto* valuesObj = new juce::DynamicObject();
        for (auto& kv : values)
            valuesObj->setProperty (juce::Identifier (kv.first), (double) kv.second);
        obj->setProperty ("values", juce::var (valuesObj));
        return juce::var (obj);
    }

    Preset Preset::fromVar (const juce::var& v)
    {
        Preset p;
        if (auto* o = v.getDynamicObject())
        {
            p.name        = o->getProperty ("name").toString();
            p.description = o->getProperty ("description").toString();
            if (o->hasProperty ("theme"))
                p.theme = o->getProperty ("theme").toString();
            if (o->hasProperty ("patchId"))
                p.patchId = o->getProperty ("patchId").toString();
            if (o->hasProperty ("expansionId"))
                p.expansionId = o->getProperty ("expansionId").toString();
            if (o->hasProperty ("packId"))
                p.packId = o->getProperty ("packId").toString();
            if (auto* refs = o->getProperty ("libraryReferences").getArray())
                for (const auto& ref : *refs)
                    p.libraryReferences.add (ref.toString());
            if (auto* tags = o->getProperty ("tags").getArray())
                for (const auto& tag : *tags)
                    p.tags.add (tag.toString());
            if (o->hasProperty ("generated"))
                p.generated = (bool) o->getProperty ("generated");
            p.isDefault   = (bool) o->getProperty ("isDefault");

            if (auto* values = o->getProperty ("values").getDynamicObject())
                for (auto& prop : values->getProperties())
                    p.values[prop.name.toString()] = (float) (double) prop.value;
        }
        return p;
    }

    bool MidiMapping::matches (const juce::MidiMessage& message) const
    {
        if (! enabled || parameterId.isEmpty())
            return false;
        if (channel > 0 && message.getChannel() != channel)
            return false;

        if (sourceType == "cc")
            return message.isController() && message.getControllerNumber() == controller;
        if (sourceType == "pitchWheel")
            return message.isPitchWheel();
        if (sourceType == "aftertouch")
            return message.isAftertouch();
        if (sourceType == "channelPressure")
            return message.isChannelPressure();
        return false;
    }

    float MidiMapping::normalisedValueFromMessage (const juce::MidiMessage& message) const
    {
        float normalised = 0.0f;
        if (sourceType == "cc" && message.isController())
            normalised = (float) message.getControllerValue() / 127.0f;
        else if (sourceType == "pitchWheel" && message.isPitchWheel())
            normalised = (float) message.getPitchWheelValue() / 16383.0f;
        else if (sourceType == "aftertouch" && message.isAftertouch())
            normalised = (float) message.getAfterTouchValue() / 127.0f;
        else if (sourceType == "channelPressure" && message.isChannelPressure())
            normalised = (float) message.getChannelPressureValue() / 127.0f;

        normalised = juce::jlimit (0.0f, 1.0f, normalised);
        return bipolar ? normalised * 2.0f - 1.0f : normalised;
    }

    juce::var MidiMapping::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("parameterId", parameterId);
        obj->setProperty ("sourceType", sourceType);
        obj->setProperty ("channel", channel);
        obj->setProperty ("controller", controller);
        obj->setProperty ("targetMin", (double) targetMin);
        obj->setProperty ("targetMax", (double) targetMax);
        obj->setProperty ("enabled", enabled);
        obj->setProperty ("bipolar", bipolar);
        return juce::var (obj);
    }

    MidiMapping MidiMapping::fromVar (const juce::var& v)
    {
        MidiMapping mapping;
        if (auto* o = v.getDynamicObject())
        {
            mapping.id = o->getProperty ("id").toString();
            mapping.parameterId = o->getProperty ("parameterId").toString();
            mapping.sourceType = o->getProperty ("sourceType").toString();
            mapping.channel = (int) o->getProperty ("channel");
            mapping.controller = (int) o->getProperty ("controller");
            mapping.targetMin = (float) (double) o->getProperty ("targetMin");
            mapping.targetMax = (float) (double) o->getProperty ("targetMax");
            if (o->hasProperty ("enabled"))
                mapping.enabled = (bool) o->getProperty ("enabled");
            if (o->hasProperty ("bipolar"))
                mapping.bipolar = (bool) o->getProperty ("bipolar");
            if (mapping.sourceType.isEmpty())
                mapping.sourceType = "cc";
            if (mapping.targetMax <= mapping.targetMin)
                mapping.targetMax = mapping.targetMin + 1.0f;
        }
        return mapping;
    }

    juce::var DspBlock::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("section", section);
        obj->setProperty ("type", type);
        obj->setProperty ("name", name);
        obj->setProperty ("targetId", targetId);
        obj->setProperty ("enabled", enabled);
        auto* valuesObj = new juce::DynamicObject();
        for (auto& kv : values)
            valuesObj->setProperty (juce::Identifier (kv.first), (double) kv.second);
        obj->setProperty ("values", juce::var (valuesObj));
        auto* metadataObj = new juce::DynamicObject();
        for (auto& kv : metadata)
            metadataObj->setProperty (juce::Identifier (kv.first), kv.second);
        obj->setProperty ("metadata", juce::var (metadataObj));
        return juce::var (obj);
    }

    DspBlock DspBlock::fromVar (const juce::var& v)
    {
        DspBlock b;
        if (auto* o = v.getDynamicObject())
        {
            b.id = o->getProperty ("id").toString();
            b.section = o->getProperty ("section").toString();
            b.type = o->getProperty ("type").toString();
            b.name = o->getProperty ("name").toString();
            b.targetId = o->getProperty ("targetId").toString();
            if (o->hasProperty ("enabled")) b.enabled = (bool) o->getProperty ("enabled");
            if (auto* valuesObj = o->getProperty ("values").getDynamicObject())
                for (auto& prop : valuesObj->getProperties())
                    b.values[prop.name.toString()] = (float) (double) prop.value;
            if (auto* metadataObj = o->getProperty ("metadata").getDynamicObject())
                for (auto& prop : metadataObj->getProperties())
                    b.metadata[prop.name.toString()] = prop.value.toString();
            if (b.section.isEmpty()) b.section = "source";
            if (b.name.isEmpty()) b.name = b.type;
        }
        return b;
    }

    juce::var MacroAssignment::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("macroId", macroId);
        obj->setProperty ("targetId", targetId);
        obj->setProperty ("sourceMin", (double) sourceMin);
        obj->setProperty ("sourceMax", (double) sourceMax);
        obj->setProperty ("targetMin", (double) targetMin);
        obj->setProperty ("targetMax", (double) targetMax);
        obj->setProperty ("curve", (double) curve);
        obj->setProperty ("bipolar", bipolar);
        return juce::var (obj);
    }

    MacroAssignment MacroAssignment::fromVar (const juce::var& v)
    {
        MacroAssignment m;
        if (auto* o = v.getDynamicObject())
        {
            m.id = o->getProperty ("id").toString();
            m.macroId = o->getProperty ("macroId").toString();
            m.targetId = o->getProperty ("targetId").toString();
            m.sourceMin = (float) (double) o->getProperty ("sourceMin");
            m.sourceMax = (float) (double) o->getProperty ("sourceMax");
            m.targetMin = (float) (double) o->getProperty ("targetMin");
            m.targetMax = (float) (double) o->getProperty ("targetMax");
            m.curve = (float) (double) o->getProperty ("curve");
            m.bipolar = (bool) o->getProperty ("bipolar");
            if (m.curve <= 0.0f) m.curve = 1.0f;
            if (m.sourceMax <= m.sourceMin) m.sourceMax = m.sourceMin + 1.0f;
            if (m.targetMax <= m.targetMin) m.targetMax = m.targetMin + 1.0f;
        }
        return m;
    }

    juce::var ModRoute::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("sourceId", sourceId);
        obj->setProperty ("targetId", targetId);
        obj->setProperty ("amount", (double) amount);
        obj->setProperty ("smoothing", (double) smoothing);
        obj->setProperty ("enabled", enabled);
        return juce::var (obj);
    }

    ModRoute ModRoute::fromVar (const juce::var& v)
    {
        ModRoute r;
        if (auto* o = v.getDynamicObject())
        {
            r.id = o->getProperty ("id").toString();
            r.sourceId = o->getProperty ("sourceId").toString();
            r.targetId = o->getProperty ("targetId").toString();
            r.amount = (float) (double) o->getProperty ("amount");
            r.smoothing = (float) (double) o->getProperty ("smoothing");
            if (o->hasProperty ("enabled")) r.enabled = (bool) o->getProperty ("enabled");
        }
        return r;
    }

    juce::var AutomationLane::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("targetId", targetId);
        obj->setProperty ("mode", mode);
        obj->setProperty ("rate", (double) rate);
        obj->setProperty ("syncToTempo", syncToTempo);
        juce::Array<juce::var> pointArray;
        for (auto point : points)
            pointArray.add ((double) point);
        obj->setProperty ("points", pointArray);
        return juce::var (obj);
    }

    AutomationLane AutomationLane::fromVar (const juce::var& v)
    {
        AutomationLane lane;
        if (auto* o = v.getDynamicObject())
        {
            lane.id = o->getProperty ("id").toString();
            lane.targetId = o->getProperty ("targetId").toString();
            lane.mode = o->getProperty ("mode").toString();
            lane.rate = (float) (double) o->getProperty ("rate");
            lane.syncToTempo = (bool) o->getProperty ("syncToTempo");
            if (auto* arr = o->getProperty ("points").getArray())
                for (auto& point : *arr)
                    lane.points.push_back ((float) (double) point);
            if (lane.mode.isEmpty()) lane.mode = "loop";
            if (lane.rate <= 0.0f) lane.rate = 1.0f;
        }
        return lane;
    }

    juce::String DspGraph::nodeKindToString (DspNodeKind kind)
    {
        switch (kind)
        {
            case DspNodeKind::source:     return "source";
            case DspNodeKind::processor:  return "processor";
            case DspNodeKind::modulation: return "modulation";
            case DspNodeKind::analysis:   return "analysis";
            case DspNodeKind::output:     return "output";
            case DspNodeKind::utility:    return "utility";
            case DspNodeKind::unknown:
            default:                      return "unknown";
        }
    }

    DspNodeKind DspGraph::nodeKindFromString (const juce::String& text)
    {
        if (text == "source")     return DspNodeKind::source;
        if (text == "processor")  return DspNodeKind::processor;
        if (text == "modulation") return DspNodeKind::modulation;
        if (text == "analysis")   return DspNodeKind::analysis;
        if (text == "output")     return DspNodeKind::output;
        if (text == "utility")    return DspNodeKind::utility;
        return DspNodeKind::unknown;
    }

    juce::String DspGraph::signalTypeToString (DspSignalType type)
    {
        switch (type)
        {
            case DspSignalType::modulation: return "modulation";
            case DspSignalType::event:      return "event";
            case DspSignalType::parameter:  return "parameter";
            case DspSignalType::audio:
            default:                        return "audio";
        }
    }

    DspSignalType DspGraph::signalTypeFromString (const juce::String& text)
    {
        if (text == "modulation") return DspSignalType::modulation;
        if (text == "event")      return DspSignalType::event;
        if (text == "parameter")  return DspSignalType::parameter;
        return DspSignalType::audio;
    }

    juce::var DspNodePort::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("name", name);
        obj->setProperty ("signalType", DspGraph::signalTypeToString (signalType));
        obj->setProperty ("input", input);
        return juce::var (obj);
    }

    DspNodePort DspNodePort::fromVar (const juce::var& v)
    {
        DspNodePort port;
        if (auto* o = v.getDynamicObject())
        {
            port.id = o->getProperty ("id").toString();
            port.name = o->getProperty ("name").toString();
            port.signalType = DspGraph::signalTypeFromString (o->getProperty ("signalType").toString());
            if (o->hasProperty ("input"))
                port.input = (bool) o->getProperty ("input");
        }
        return port;
    }

    juce::var TypedDspNode::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("sourceBlockId", sourceBlockId);
        obj->setProperty ("section", section);
        obj->setProperty ("type", type);
        obj->setProperty ("name", name);
        obj->setProperty ("targetId", targetId);
        obj->setProperty ("kind", DspGraph::nodeKindToString (kind));
        obj->setProperty ("enabled", enabled);

        juce::Array<juce::var> inputArray;
        for (const auto& port : inputs)
            inputArray.add (port.toVar());
        obj->setProperty ("inputs", inputArray);

        juce::Array<juce::var> outputArray;
        for (const auto& port : outputs)
            outputArray.add (port.toVar());
        obj->setProperty ("outputs", outputArray);

        auto* valuesObj = new juce::DynamicObject();
        for (const auto& value : values)
            valuesObj->setProperty (juce::Identifier (value.first), (double) value.second);
        obj->setProperty ("values", juce::var (valuesObj));

        auto* metadataObj = new juce::DynamicObject();
        for (const auto& item : metadata)
            metadataObj->setProperty (juce::Identifier (item.first), item.second);
        obj->setProperty ("metadata", juce::var (metadataObj));

        return juce::var (obj);
    }

    juce::var DspGraphEdge::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("sourceNodeId", sourceNodeId);
        obj->setProperty ("sourcePortId", sourcePortId);
        obj->setProperty ("targetNodeId", targetNodeId);
        obj->setProperty ("targetPortId", targetPortId);
        obj->setProperty ("signalType", DspGraph::signalTypeToString (signalType));
        obj->setProperty ("gain", (double) gain);
        obj->setProperty ("enabled", enabled);
        return juce::var (obj);
    }

    DspGraphEdge DspGraphEdge::fromVar (const juce::var& v)
    {
        DspGraphEdge edge;
        if (auto* o = v.getDynamicObject())
        {
            edge.id = o->getProperty ("id").toString();
            edge.sourceNodeId = o->getProperty ("sourceNodeId").toString();
            edge.sourcePortId = o->getProperty ("sourcePortId").toString();
            edge.targetNodeId = o->getProperty ("targetNodeId").toString();
            edge.targetPortId = o->getProperty ("targetPortId").toString();
            edge.signalType = DspGraph::signalTypeFromString (o->getProperty ("signalType").toString());
            if (o->hasProperty ("gain"))
                edge.gain = (float) (double) o->getProperty ("gain");
            if (o->hasProperty ("enabled"))
                edge.enabled = (bool) o->getProperty ("enabled");
            if (edge.sourcePortId.isEmpty()) edge.sourcePortId = "audioOut";
            if (edge.targetPortId.isEmpty()) edge.targetPortId = "audioIn";
            if (edge.id.isEmpty() && edge.sourceNodeId.isNotEmpty() && edge.targetNodeId.isNotEmpty())
                edge.id = edge.sourceNodeId + "_to_" + edge.targetNodeId;
        }
        return edge;
    }

    TypedDspNode TypedDspNode::fromVar (const juce::var& v)
    {
        TypedDspNode node;
        if (auto* o = v.getDynamicObject())
        {
            node.id = o->getProperty ("id").toString();
            node.sourceBlockId = o->getProperty ("sourceBlockId").toString();
            node.section = o->getProperty ("section").toString();
            node.type = o->getProperty ("type").toString();
            node.name = o->getProperty ("name").toString();
            node.targetId = o->getProperty ("targetId").toString();
            node.kind = DspGraph::nodeKindFromString (o->getProperty ("kind").toString());
            if (o->hasProperty ("enabled"))
                node.enabled = (bool) o->getProperty ("enabled");

            if (auto* arr = o->getProperty ("inputs").getArray())
                for (const auto& item : *arr)
                    node.inputs.push_back (DspNodePort::fromVar (item));
            if (auto* arr = o->getProperty ("outputs").getArray())
                for (const auto& item : *arr)
                    node.outputs.push_back (DspNodePort::fromVar (item));
            if (auto* valuesObj = o->getProperty ("values").getDynamicObject())
                for (auto& prop : valuesObj->getProperties())
                    node.values[prop.name.toString()] = (float) (double) prop.value;
            if (auto* metadataObj = o->getProperty ("metadata").getDynamicObject())
                for (auto& prop : metadataObj->getProperties())
                    node.metadata[prop.name.toString()] = prop.value.toString();
        }
        return node;
    }

    juce::String DspGraphValidationIssue::toString() const
    {
        auto text = severity.toUpperCase() + ": ";
        if (ownerId.isNotEmpty())
            text += ownerId + " - ";
        return text + message;
    }

    static DspNodeKind classifyBlockKind (const DspBlock& block)
    {
        return DspModuleRegistry::classifyBlockKind (block);
    }

    static DspNodePort makePort (juce::String id, juce::String name, DspSignalType type, bool input)
    {
        DspNodePort port;
        port.id = std::move (id);
        port.name = std::move (name);
        port.signalType = type;
        port.input = input;
        return port;
    }

    static bool containsAnyToken (const juce::String& text, std::initializer_list<const char*> tokens)
    {
        const auto lower = text.toLowerCase();
        for (auto* token : tokens)
            if (lower.contains (token))
                return true;
        return false;
    }

    static bool sectionLooksSupported (const juce::String& section)
    {
        const auto lower = section.toLowerCase();
        if (lower.isEmpty())
            return true;

        static const juce::StringArray supported {
            "source", "filter", "amp", "shape",
            "mod", "modulation", "motion", "perform", "circle", "midi", "macro",
            "fx", "out", "output", "input"
        };
        return supported.contains (lower, false);
    }

    static bool blockTypeLooksSupported (const TypedDspNode& node)
    {
        return DspModuleRegistry::isBlockSupported (node);
    }

    static float nodeValue (const TypedDspNode& node, const juce::String& key, float fallback = 0.0f)
    {
        const auto it = node.values.find (key);
        return it == node.values.end() ? fallback : it->second;
    }

    std::vector<TypedDspNode> DspGraph::buildTypedNodes (const juce::String& engineId) const
    {
        std::vector<TypedDspNode> nodes;
        nodes.reserve (blocks.size() + 1);

        for (const auto& block : blocks)
        {
            TypedDspNode node;
            node.id = block.id.isNotEmpty() ? block.id : ("unnamed_" + juce::String ((int) nodes.size() + 1));
            node.sourceBlockId = block.id;
            node.section = block.section;
            node.type = block.type;
            node.name = block.name.isNotEmpty() ? block.name : block.type;
            node.targetId = block.targetId;
            node.kind = classifyBlockKind (block);
            node.enabled = block.enabled;
            node.values = block.values;
            node.metadata = block.metadata;

            if (node.kind == DspNodeKind::source)
            {
                node.outputs.push_back (makePort ("audioOut", "Audio Out", DspSignalType::audio, false));
                if (engineId == "fx")
                    node.inputs.push_back (makePort ("audioIn", "Audio In", DspSignalType::audio, true));
            }
            else if (node.kind == DspNodeKind::modulation)
            {
                node.outputs.push_back (makePort ("modOut", "Mod Out", DspSignalType::modulation, false));
                if (node.targetId.isNotEmpty())
                    node.outputs.push_back (makePort ("target", node.targetId, DspSignalType::parameter, false));
            }
            else if (node.kind == DspNodeKind::output)
            {
                node.inputs.push_back (makePort ("audioIn", "Audio In", DspSignalType::audio, true));
            }
            else
            {
                node.inputs.push_back (makePort ("audioIn", "Audio In", DspSignalType::audio, true));
                node.outputs.push_back (makePort ("audioOut", "Audio Out", DspSignalType::audio, false));
                if (node.targetId.isNotEmpty())
                    node.outputs.push_back (makePort ("target", node.targetId, DspSignalType::parameter, false));
            }

            nodes.push_back (std::move (node));
        }

        bool hasOutput = false;
        for (const auto& node : nodes)
            if (node.kind == DspNodeKind::output)
                hasOutput = true;

        if (! hasOutput)
        {
            TypedDspNode output;
            output.id = "main_output";
            output.section = "out";
            output.type = "mainOutput";
            output.name = "Main Output";
            output.kind = DspNodeKind::output;
            output.inputs.push_back (makePort ("audioIn", "Audio In", DspSignalType::audio, true));
            nodes.push_back (std::move (output));
        }

        return nodes;
    }

    std::vector<DspGraphEdge> DspGraph::buildAudioEdges (const juce::String& engineId) const
    {
        std::vector<DspGraphEdge> resolved;

        for (auto edge : edges)
        {
            if (edge.sourcePortId.isEmpty()) edge.sourcePortId = "audioOut";
            if (edge.targetPortId.isEmpty()) edge.targetPortId = "audioIn";
            if (edge.id.isEmpty() && edge.sourceNodeId.isNotEmpty() && edge.targetNodeId.isNotEmpty())
                edge.id = edge.sourceNodeId + "_to_" + edge.targetNodeId;
            resolved.push_back (std::move (edge));
        }

        if (! resolved.empty())
            return resolved;

        const auto nodes = buildTypedNodes (engineId);
        juce::StringArray sourceIds;
        juce::StringArray chainIds;
        juce::String outputId;

        for (const auto& node : nodes)
        {
            if (! node.enabled)
                continue;

            if (node.kind == DspNodeKind::source)
                sourceIds.add (node.id);
            else if (node.kind == DspNodeKind::processor || node.kind == DspNodeKind::utility)
                chainIds.add (node.id);
            else if (node.kind == DspNodeKind::output && outputId.isEmpty())
                outputId = node.id;
        }

        if (outputId.isEmpty())
            outputId = "main_output";

        auto addEdge = [&resolved] (const juce::String& from, const juce::String& to, float gain = 1.0f)
        {
            if (from.isEmpty() || to.isEmpty() || from == to)
                return;
            DspGraphEdge edge;
            edge.id = from + "_to_" + to;
            edge.sourceNodeId = from;
            edge.targetNodeId = to;
            edge.gain = gain;
            resolved.push_back (std::move (edge));
        };

        const auto firstChain = chainIds.isEmpty() ? outputId : chainIds[0];
        for (const auto& sourceId : sourceIds)
            addEdge (sourceId, firstChain, sourceIds.size() > 1 ? 1.0f / std::sqrt ((float) sourceIds.size()) : 1.0f);

        for (int i = 0; i < chainIds.size() - 1; ++i)
            addEdge (chainIds[i], chainIds[i + 1]);

        if (! chainIds.isEmpty())
            addEdge (chainIds[chainIds.size() - 1], outputId);

        return resolved;
    }

    std::vector<DspGraphValidationIssue> DspGraph::validateTypedGraph (const juce::String& engineId) const
    {
        std::vector<DspGraphValidationIssue> issues;
        const auto nodes = buildTypedNodes (engineId);
        const auto graphEdges = buildAudioEdges (engineId);

        if (blocks.empty())
            issues.push_back ({ "warning", "dspGraph", "DSP graph has no author blocks; the engine will use defaults." });

        juce::StringArray ids;
        int audioSources = 0;
        int authoredOutputs = 0;
        int fxInputSources = 0;
        int instrumentOnlySourcesInFx = 0;
        int liveInputSourcesInInstrument = 0;
        int outputs = 0;
        float summedSourceLevel = 0.0f;
        bool limiterEnabled = true;
        for (const auto& node : nodes)
        {
            if (node.id.isEmpty())
                issues.push_back ({ "error", "dspGraph", "Typed node has an empty id." });
            else if (ids.contains (node.id, false))
                issues.push_back ({ "error", node.id, "Duplicate DSP node id." });
            else
                ids.add (node.id);

            if (node.kind == DspNodeKind::unknown)
                issues.push_back ({ "error", node.id, "DSP block could not be classified into a typed node kind." });
            if (! sectionLooksSupported (node.section))
                issues.push_back ({ "error", node.id, "DSP block section is unsupported by the current Player engine." });
            if (node.kind != DspNodeKind::unknown && ! blockTypeLooksSupported (node))
                issues.push_back ({ "warning", node.id, "DSP block type uses generic Player routing; test the exported patch to confirm the intended sound." });
            if (node.type.isEmpty())
                issues.push_back ({ "warning", node.id, "DSP node type is empty." });
            if (node.kind == DspNodeKind::source && node.enabled)
            {
                ++audioSources;
                summedSourceLevel += nodeValue (node, "volume", nodeValue (node, "wtLevel", 0.75f));

                const auto type = node.type.toLowerCase();
                const bool liveInput = type.contains ("input") || type.contains ("drive");
                if (liveInput)
                    ++fxInputSources;
                if (engineId == "fx" && ! liveInput)
                    ++instrumentOnlySourcesInFx;
                if (engineId != "fx" && liveInput)
                    ++liveInputSourcesInInstrument;
            }
            if (node.kind == DspNodeKind::output && node.enabled)
            {
                ++outputs;
                if (node.sourceBlockId.isNotEmpty())
                    ++authoredOutputs;
                if (node.values.find ("outputLimiter") != node.values.end())
                    limiterEnabled = nodeValue (node, "outputLimiter", 1.0f) >= 0.5f;
            }

            if (node.targetId == node.id && node.id.isNotEmpty())
                issues.push_back ({ "error", node.id, "DSP block targets itself; this would create an unsafe feedback mapping." });

            for (const auto& value : node.values)
            {
                if (value.second != value.second)
                    issues.push_back ({ "error", node.id, "DSP block contains a NaN parameter value." });
            }
            const auto oversampling = node.metadata.find ("oversampling");
            if (oversampling != node.metadata.end())
            {
                const auto factor = oversampling->second.getIntValue();
                if (factor != 1 && factor != 2 && factor != 4 && factor != 8)
                    issues.push_back ({ "warning", node.id, "Oversampling metadata must be one of 1x, 2x, 4x, or 8x." });
            }

            if (nodeValue (node, "volume", 0.0f) > 1.0f || nodeValue (node, "wtLevel", 0.0f) > 1.25f)
                issues.push_back ({ "warning", node.id, "Source level is above unity; export may clip when layers are combined." });
            if (nodeValue (node, "drive", 0.0f) > 0.9f)
                issues.push_back ({ "warning", node.id, "Drive is near maximum; export should be checked for clipping." });
            if (nodeValue (node, "delayFeedback", 0.0f) > 0.92f)
                issues.push_back ({ "warning", node.id, "Delay feedback is very high; runaway feedback is possible." });
            if (nodeValue (node, "eqGainDb", 0.0f) > 12.0f || nodeValue (node, "outputGainDb", 0.0f) > 12.0f || nodeValue (node, "inputTrimDb", 0.0f) > 12.0f)
                issues.push_back ({ "warning", node.id, "Gain staging exceeds +12 dB; export should be checked for clipping." });
        }

        if (audioSources == 0 && engineId != "fx")
            issues.push_back ({ "warning", "dspGraph", "Instrument graph has no enabled source node." });
        if (engineId == "fx" && fxInputSources == 0)
            issues.push_back ({ "warning", "dspGraph", "FX graph has no live input/drive source node; it may not process incoming audio as expected." });
        if (engineId == "fx" && instrumentOnlySourcesInFx > 0)
            issues.push_back ({ "warning", "dspGraph", "FX graph contains instrument-only source nodes; Player FX exports route live input, not note-triggered sources." });
        if (engineId != "fx" && liveInputSourcesInInstrument > 0)
            issues.push_back ({ "warning", "dspGraph", "Instrument graph contains live-input source nodes; instrument Players do not receive audio input." });
        if (outputs == 0)
            issues.push_back ({ "error", "dspGraph", "DSP graph has no enabled output node." });
        if (authoredOutputs == 0)
            issues.push_back ({ "warning", "dspGraph", "DSP graph relies on the synthetic main output; add an OUT block to expose limiter, gain, and routing safety." });
        if (summedSourceLevel > 2.2f && ! limiterEnabled)
            issues.push_back ({ "warning", "dspGraph", "Combined source levels are high while the output limiter is disabled." });

        auto nodeExists = [&] (const juce::String& id)
        {
            for (const auto& node : nodes)
                if (node.id == id)
                    return true;
            return false;
        };
        auto nodeKindFor = [&] (const juce::String& id)
        {
            for (const auto& node : nodes)
                if (node.id == id)
                    return node.kind;
            return DspNodeKind::unknown;
        };

        juce::StringArray reachable;
        juce::StringArray edgeIds;
        for (const auto& node : nodes)
            if (node.kind == DspNodeKind::source && node.enabled)
                reachable.addIfNotAlreadyThere (node.id);

        for (const auto& edge : graphEdges)
        {
            if (edge.id.isEmpty())
                issues.push_back ({ "warning", "dspGraph", "DSP graph edge has an empty id." });
            else if (edgeIds.contains (edge.id, false))
                issues.push_back ({ "warning", edge.id, "Duplicate DSP graph edge id." });
            else
                edgeIds.add (edge.id);

            if (! edge.enabled)
                continue;

            if (edge.sourceNodeId.isEmpty() || edge.targetNodeId.isEmpty())
                issues.push_back ({ "error", edge.id, "DSP graph edge must define source and target nodes." });
            if (edge.sourceNodeId == edge.targetNodeId && edge.sourceNodeId.isNotEmpty())
                issues.push_back ({ "error", edge.id, "DSP graph edge routes a node into itself." });
            if (edge.sourceNodeId.isNotEmpty() && ! nodeExists (edge.sourceNodeId))
                issues.push_back ({ "error", edge.id, "DSP graph edge source node is missing." });
            if (edge.targetNodeId.isNotEmpty() && ! nodeExists (edge.targetNodeId))
                issues.push_back ({ "error", edge.id, "DSP graph edge target node is missing." });
            if (edge.signalType == DspSignalType::audio && std::abs (edge.gain) > 2.0f)
                issues.push_back ({ "warning", edge.id, "DSP graph edge gain exceeds +6 dB; export should be checked for clipping." });
            if (edge.signalType == DspSignalType::audio && nodeKindFor (edge.targetNodeId) == DspNodeKind::source)
                issues.push_back ({ "error", edge.id, "Audio edges cannot target a source node in the current runtime." });
            if (edge.signalType == DspSignalType::audio && nodeKindFor (edge.sourceNodeId) == DspNodeKind::modulation)
                issues.push_back ({ "error", edge.id, "Audio edges cannot originate from modulation nodes." });
        }

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& edge : graphEdges)
            {
                if (! edge.enabled || edge.signalType != DspSignalType::audio)
                    continue;
                if (reachable.contains (edge.sourceNodeId, false)
                    && ! reachable.contains (edge.targetNodeId, false))
                {
                    reachable.add (edge.targetNodeId);
                    changed = true;
                }
            }
        }

        for (const auto& node : nodes)
        {
            if (! node.enabled || node.kind != DspNodeKind::output)
                continue;
            if (! reachable.contains (node.id, false))
                issues.push_back ({ "error", node.id, "Output node is not reachable from an enabled audio source." });
        }

        auto blockExists = [&] (const juce::String& id)
        {
            for (const auto& block : blocks)
                if (block.id == id)
                    return true;
            return false;
        };

        for (const auto& route : modulation)
        {
            if (route.enabled && route.sourceId == route.targetId && route.sourceId.isNotEmpty())
                issues.push_back ({ "error", route.id, "Modulation route feeds a source back into itself." });
            if (route.enabled && route.sourceId.isNotEmpty() && ! blockExists (route.sourceId))
                issues.push_back ({ "warning", route.id, "Modulation source is external or unresolved in the typed DSP graph." });
            if (route.enabled && route.targetId.isEmpty())
                issues.push_back ({ "warning", route.id, "Modulation route has no target." });
        }

        for (const auto& lane : automation)
        {
            if (lane.points.empty())
                issues.push_back ({ "warning", lane.id, "Automation lane has no points." });
            if (lane.targetId.isEmpty())
                issues.push_back ({ "warning", lane.id, "Automation lane has no target." });
        }

        return issues;
    }

    void DspGraph::resetForEngine (const juce::String& engineId)
    {
        SoundStack::resetSimpleGraph (*this, engineId);
    }

    void DspGraph::resetForEngineExpanded (const juce::String& engineId)
    {
        SoundStack::resetExpandedGraph (*this, engineId);
    }

    bool DspGraph::ensureAuthoredOutput()
    {
        juce::String lastAudioId;
        for (const auto& block : blocks)
        {
            if (! block.enabled)
                continue;

            const auto kind = DspModuleRegistry::classifyBlockKind (block);
            if (kind == DspNodeKind::output)
                return false;

            if (kind != DspNodeKind::modulation && kind != DspNodeKind::unknown)
                lastAudioId = block.id;
        }

        DspBlock out;
        out.id = "main_output";
        out.section = "out";
        out.type = "limiter";
        out.name = "Main Output";
        out.targetId = "volume";
        out.enabled = true;
        out.values["outputLimiter"] = 1.0f;
        out.values["outputCeilingDb"] = -1.0f;
        out.values["outputGainDb"] = -3.0f;
        out.metadata["uiX"] = "1240";
        out.metadata["uiY"] = "80";
        blocks.push_back (out);

        if (lastAudioId.isNotEmpty())
        {
            bool hasEdge = false;
            for (const auto& edge : edges)
                if (edge.enabled && edge.sourceNodeId == lastAudioId && edge.targetNodeId == out.id)
                {
                    hasEdge = true;
                    break;
                }

            if (! hasEdge)
            {
                DspGraphEdge edge;
                edge.id = lastAudioId + "_to_main_output";
                edge.sourceNodeId = lastAudioId;
                edge.targetNodeId = out.id;
                edge.signalType = DspSignalType::audio;
                edge.enabled = true;
                edges.push_back (std::move (edge));
            }
        }

        return true;
    }

    juce::var DspGraph::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        juce::Array<juce::var> blockArray;
        for (auto& b : blocks) blockArray.add (b.toVar());
        juce::Array<juce::var> edgeArray;
        for (const auto& edge : buildAudioEdges())
            edgeArray.add (edge.toVar());
        juce::Array<juce::var> macroArray;
        for (auto& m : macros) macroArray.add (m.toVar());
        juce::Array<juce::var> modArray;
        for (auto& r : modulation) modArray.add (r.toVar());
        juce::Array<juce::var> automationArray;
        for (auto& lane : automation) automationArray.add (lane.toVar());
        juce::Array<juce::var> typedNodeArray;
        for (const auto& node : buildTypedNodes())
            typedNodeArray.add (node.toVar());
        obj->setProperty ("blocks", blockArray);
        obj->setProperty ("edges", edgeArray);
        obj->setProperty ("macros", macroArray);
        obj->setProperty ("modulation", modArray);
        obj->setProperty ("automation", automationArray);
        obj->setProperty ("typedNodes", typedNodeArray);
        obj->setProperty ("userConfigured", userConfigured);
        auto* quickObj = new juce::DynamicObject();
        for (const auto& kv : quickEditControls)
        {
            juce::Array<juce::var> ids;
            for (const auto& id : kv.second)
                ids.add (id);
            quickObj->setProperty (juce::Identifier (kv.first), ids);
        }
        obj->setProperty ("quickEditControls", juce::var (quickObj));
        return juce::var (obj);
    }

    void DspGraph::fromVar (const juce::var& v)
    {
        blocks.clear();
        edges.clear();
        macros.clear();
        modulation.clear();
        automation.clear();
        quickEditControls.clear();
        userConfigured = false;
        if (auto* o = v.getDynamicObject())
        {
            if (auto* arr = o->getProperty ("blocks").getArray())
                for (auto& item : *arr) blocks.push_back (DspBlock::fromVar (item));
            if (auto* arr = o->getProperty ("edges").getArray())
                for (auto& item : *arr) edges.push_back (DspGraphEdge::fromVar (item));
            if (auto* arr = o->getProperty ("macros").getArray())
                for (auto& item : *arr) macros.push_back (MacroAssignment::fromVar (item));
            if (auto* arr = o->getProperty ("modulation").getArray())
                for (auto& item : *arr) modulation.push_back (ModRoute::fromVar (item));
            if (auto* arr = o->getProperty ("automation").getArray())
                for (auto& item : *arr) automation.push_back (AutomationLane::fromVar (item));
            if (auto* quickObj = o->getProperty ("quickEditControls").getDynamicObject())
            {
                for (auto& prop : quickObj->getProperties())
                {
                    juce::StringArray ids;
                    if (auto* arr = prop.value.getArray())
                        for (auto& item : *arr)
                            ids.add (item.toString());
                    quickEditControls[prop.name.toString()] = ids;
                }
            }
            userConfigured = o->hasProperty ("userConfigured")
                ? (bool) o->getProperty ("userConfigured")
                : (! blocks.empty() || ! macros.empty() || ! modulation.empty() || ! automation.empty());
        }
    }

    namespace
    {
        static juce::Array<juce::var> stringArrayToVarArray (const juce::StringArray& strings)
        {
            juce::Array<juce::var> array;
            for (const auto& text : strings)
                array.add (text);
            return array;
        }

        static juce::StringArray stringArrayFromVar (const juce::var& value)
        {
            juce::StringArray strings;
            if (auto* array = value.getArray())
                for (const auto& item : *array)
                    strings.add (item.toString());
            return strings;
        }

        static juce::var valuesToVar (const std::map<juce::String, float>& values)
        {
            auto* obj = new juce::DynamicObject();
            for (const auto& item : values)
                obj->setProperty (juce::Identifier (item.first), (double) item.second);
            return juce::var (obj);
        }

        static std::map<juce::String, float> valuesFromVar (const juce::var& value)
        {
            std::map<juce::String, float> values;
            if (auto* obj = value.getDynamicObject())
                for (auto& prop : obj->getProperties())
                    values[prop.name.toString()] = (float) (double) prop.value;
            return values;
        }
    }

    Preset InstrumentPatch::toPreset() const
    {
        Preset preset;
        preset.name = name;
        preset.description = description;
        preset.patchId = id;
        preset.expansionId = expansionId;
        preset.packId = packId;
        preset.libraryReferences = libraryReferences;
        for (const auto& asset : includedAssets)
            preset.libraryReferences.addIfNotAlreadyThere (asset);
        preset.tags = tags;
        preset.generated = generated;
        preset.isDefault = isDefault;
        preset.values = parameterValues;
        return preset;
    }

    juce::var InstrumentPatch::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("name", name);
        obj->setProperty ("description", description);
        obj->setProperty ("engine", engine);
        obj->setProperty ("category", category);
        obj->setProperty ("author", author);
        obj->setProperty ("version", version);
        obj->setProperty ("packId", packId);
        obj->setProperty ("expansionId", expansionId);
        obj->setProperty ("tags", stringArrayToVarArray (tags));
        obj->setProperty ("libraryReferences", stringArrayToVarArray (libraryReferences));
        obj->setProperty ("includedAssets", stringArrayToVarArray (includedAssets));
        obj->setProperty ("generated", generated);
        obj->setProperty ("isDefault", isDefault);
        obj->setProperty ("dspGraph", dspGraph.toVar());

        juce::Array<juce::var> sampleArray;
        for (const auto& zone : sampleZones)
            sampleArray.add (zone.toVar());
        obj->setProperty ("sampleZones", sampleArray);

        juce::Array<juce::var> midiArray;
        for (const auto& mapping : midiMappings)
            midiArray.add (mapping.toVar());
        obj->setProperty ("midiMappings", midiArray);
        obj->setProperty ("parameterValues", valuesToVar (parameterValues));
        return juce::var (obj);
    }

    InstrumentPatch InstrumentPatch::fromVar (const juce::var& value)
    {
        InstrumentPatch patch;
        if (auto* obj = value.getDynamicObject())
        {
            patch.id = obj->getProperty ("id").toString();
            patch.name = obj->getProperty ("name").toString();
            patch.description = obj->getProperty ("description").toString();
            patch.engine = obj->getProperty ("engine").toString();
            patch.category = obj->getProperty ("category").toString();
            patch.author = obj->getProperty ("author").toString();
            patch.version = obj->getProperty ("version").toString();
            patch.packId = obj->getProperty ("packId").toString();
            patch.expansionId = obj->getProperty ("expansionId").toString();
            patch.tags = stringArrayFromVar (obj->getProperty ("tags"));
            patch.libraryReferences = stringArrayFromVar (obj->getProperty ("libraryReferences"));
            patch.includedAssets = stringArrayFromVar (obj->getProperty ("includedAssets"));
            if (obj->hasProperty ("generated"))
                patch.generated = (bool) obj->getProperty ("generated");
            if (obj->hasProperty ("isDefault"))
                patch.isDefault = (bool) obj->getProperty ("isDefault");
            patch.dspGraph.fromVar (obj->getProperty ("dspGraph"));
            if (auto* samples = obj->getProperty ("sampleZones").getArray())
                for (const auto& item : *samples)
                    patch.sampleZones.push_back (SampleZoneDef::fromVar (item));
            if (auto* midi = obj->getProperty ("midiMappings").getArray())
                for (const auto& item : *midi)
                    patch.midiMappings.push_back (MidiMapping::fromVar (item));
            patch.parameterValues = valuesFromVar (obj->getProperty ("parameterValues"));
            if (patch.engine.isEmpty()) patch.engine = "sample";
            if (patch.version.isEmpty()) patch.version = "1.0";
        }
        return patch;
    }

    juce::var SectionPreset::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("name", name);
        obj->setProperty ("description", description);
        obj->setProperty ("section", section);
        obj->setProperty ("engine", engine);
        obj->setProperty ("category", category);
        obj->setProperty ("packId", packId);
        obj->setProperty ("expansionId", expansionId);
        obj->setProperty ("tags", stringArrayToVarArray (tags));
        obj->setProperty ("libraryReferences", stringArrayToVarArray (libraryReferences));

        juce::Array<juce::var> blockArray;
        for (const auto& block : blocks)
            blockArray.add (block.toVar());
        obj->setProperty ("blocks", blockArray);

        juce::Array<juce::var> edgeArray;
        for (const auto& edge : edges)
            edgeArray.add (edge.toVar());
        obj->setProperty ("edges", edgeArray);

        juce::Array<juce::var> macroArray;
        for (const auto& macro : macros)
            macroArray.add (macro.toVar());
        obj->setProperty ("macros", macroArray);

        juce::Array<juce::var> modArray;
        for (const auto& route : modulation)
            modArray.add (route.toVar());
        obj->setProperty ("modulation", modArray);

        juce::Array<juce::var> automationArray;
        for (const auto& lane : automation)
            automationArray.add (lane.toVar());
        obj->setProperty ("automation", automationArray);
        obj->setProperty ("parameterValues", valuesToVar (parameterValues));
        return juce::var (obj);
    }

    SectionPreset SectionPreset::fromVar (const juce::var& value)
    {
        SectionPreset preset;
        if (auto* obj = value.getDynamicObject())
        {
            preset.id = obj->getProperty ("id").toString();
            preset.name = obj->getProperty ("name").toString();
            preset.description = obj->getProperty ("description").toString();
            preset.section = obj->getProperty ("section").toString();
            preset.engine = obj->getProperty ("engine").toString();
            preset.category = obj->getProperty ("category").toString();
            preset.packId = obj->getProperty ("packId").toString();
            preset.expansionId = obj->getProperty ("expansionId").toString();
            preset.tags = stringArrayFromVar (obj->getProperty ("tags"));
            preset.libraryReferences = stringArrayFromVar (obj->getProperty ("libraryReferences"));
            if (auto* array = obj->getProperty ("blocks").getArray())
                for (const auto& item : *array)
                    preset.blocks.push_back (DspBlock::fromVar (item));
            if (auto* array = obj->getProperty ("edges").getArray())
                for (const auto& item : *array)
                    preset.edges.push_back (DspGraphEdge::fromVar (item));
            if (auto* array = obj->getProperty ("macros").getArray())
                for (const auto& item : *array)
                    preset.macros.push_back (MacroAssignment::fromVar (item));
            if (auto* array = obj->getProperty ("modulation").getArray())
                for (const auto& item : *array)
                    preset.modulation.push_back (ModRoute::fromVar (item));
            if (auto* array = obj->getProperty ("automation").getArray())
                for (const auto& item : *array)
                    preset.automation.push_back (AutomationLane::fromVar (item));
            preset.parameterValues = valuesFromVar (obj->getProperty ("parameterValues"));
            if (preset.engine.isEmpty()) preset.engine = "sample";
        }
        return preset;
    }

    juce::var ExpansionMetadata::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("name", name);
        obj->setProperty ("description", description);
        obj->setProperty ("author", author);
        obj->setProperty ("brand", brand);
        obj->setProperty ("artworkPath", artworkPath);
        obj->setProperty ("licensePath", licensePath);
        obj->setProperty ("category", category);
        obj->setProperty ("version", version);
        obj->setProperty ("compatibility", compatibility);
        obj->setProperty ("tags", stringArrayToVarArray (tags));
        obj->setProperty ("folders", stringArrayToVarArray (folders));
        obj->setProperty ("includedPatchIds", stringArrayToVarArray (includedPatchIds));
        obj->setProperty ("includedPresetNames", stringArrayToVarArray (includedPresetNames));
        obj->setProperty ("includedSectionPresetIds", stringArrayToVarArray (includedSectionPresetIds));
        obj->setProperty ("includedAssets", stringArrayToVarArray (includedAssets));
        return juce::var (obj);
    }

    ExpansionMetadata ExpansionMetadata::fromVar (const juce::var& value)
    {
        ExpansionMetadata metadata;
        if (auto* obj = value.getDynamicObject())
        {
            metadata.id = obj->getProperty ("id").toString();
            metadata.name = obj->getProperty ("name").toString();
            metadata.description = obj->getProperty ("description").toString();
            metadata.author = obj->getProperty ("author").toString();
            metadata.brand = obj->getProperty ("brand").toString();
            metadata.artworkPath = obj->getProperty ("artworkPath").toString();
            metadata.licensePath = obj->getProperty ("licensePath").toString();
            metadata.category = obj->getProperty ("category").toString();
            metadata.version = obj->getProperty ("version").toString();
            metadata.compatibility = obj->getProperty ("compatibility").toString();
            metadata.tags = stringArrayFromVar (obj->getProperty ("tags"));
            metadata.folders = stringArrayFromVar (obj->getProperty ("folders"));
            metadata.includedPatchIds = stringArrayFromVar (obj->getProperty ("includedPatchIds"));
            metadata.includedPresetNames = stringArrayFromVar (obj->getProperty ("includedPresetNames"));
            metadata.includedSectionPresetIds = stringArrayFromVar (obj->getProperty ("includedSectionPresetIds"));
            metadata.includedAssets = stringArrayFromVar (obj->getProperty ("includedAssets"));
            if (metadata.version.isEmpty()) metadata.version = "1.0";
            if (metadata.compatibility.isEmpty()) metadata.compatibility = "PatchCraft 0.1+";
        }
        return metadata;
    }

} // namespace patchcraft
