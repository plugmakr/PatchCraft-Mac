#include "BrandingLabPage.h"
#include "PackRuntimeHost.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <functional>
#include <utility>

namespace patchcraft
{
    namespace
    {
        static juce::String colourToHex (juce::Colour c)
        {
            return "#" + juce::String::toHexString ((int) c.getARGB()).paddedLeft ('0', 8).toUpperCase();
        }

        static constexpr int kPlayerTitleBarHeight = 108;

        static const juce::StringArray& titleBarThemeIds()
        {
            static const juce::StringArray ids {
                "classic", "aurora", "banner", "minimal", "compact-daw",
                "split-brand", "neon-strip", "glass", "dark-utility", "artist-card",
                "logo-rail", "clean-pro", "wide-banner", "custom", "bottom-tools", "no-chrome"
            };
            return ids;
        }

        static const juce::StringArray& titleBarThemeNames()
        {
            static const juce::StringArray names {
                "Classic", "Aurora", "Banner Image", "Minimal", "Compact DAW",
                "Split Brand", "Neon Strip", "Glass", "Dark Utility", "Artist Card",
                "Logo Rail", "Clean Pro", "Wide Banner", "Custom", "Bottom Tools", "No Chrome"
            };
            return names;
        }

        static const juce::StringArray& titleTextPlacementIds()
        {
            static const juce::StringArray ids { "left", "center", "right", "hidden" };
            return ids;
        }

        static const juce::StringArray& titleButtonStyleIds()
        {
            static const juce::StringArray ids { "outlined", "filled", "minimal", "square", "pill" };
            return ids;
        }

        static const juce::StringArray& titleFontIds()
        {
            static const juce::StringArray ids { "Default", "Arial", "Segoe UI", "Verdana", "Georgia", "Consolas" };
            return ids;
        }

        static int comboIdForValue (const juce::StringArray& values, const juce::String& value)
        {
            const int index = values.indexOf (value);
            return index >= 0 ? index + 1 : 1;
        }

        static juce::String valueForComboId (const juce::StringArray& values, int comboId)
        {
            const int index = comboId - 1;
            return index >= 0 && index < values.size() ? values[index] : values[0];
        }

        static void applyTitleBarThemeRecipe (Manifest& m, const juce::String& themeId)
        {
            auto setCommon = [&] (juce::Colour bg, juce::Colour panel, juce::Colour accent,
                                  juce::Colour text, juce::Colour dim, juce::Colour border,
                                  juce::String placement, juce::String buttonStyle, juce::String font)
            {
                m.playerBackgroundColour = bg;
                m.playerPanelColour = panel;
                m.playerAccentColour = accent;
                m.playerTextColour = text;
                m.playerTextDimColour = dim;
                m.playerBorderColour = border;
                m.playerTitleTextPlacement = std::move (placement);
                m.playerTitleButtonStyle = std::move (buttonStyle);
                m.playerTitleFontFamily = std::move (font);
                m.playerShowTopBar = true;
            };

            if (themeId == "aurora")
                setCommon (juce::Colour (0xff050913), juce::Colour (0xdd111827), juce::Colour (0xff34f5e5), juce::Colour (0xffeefcff), juce::Colour (0xff89a8b6), juce::Colour (0xff245064), "left", "pill", "Segoe UI");
            else if (themeId == "banner")
                setCommon (juce::Colour (0xff08090d), juce::Colour (0xee10151e), juce::Colour (0xffffb02e), juce::Colour (0xfffff5df), juce::Colour (0xffaa9c84), juce::Colour (0xff6a4b19), "left", "filled", "Georgia");
            else if (themeId == "minimal")
                setCommon (juce::Colour (0xff070809), juce::Colour (0x0015171b), juce::Colour (0xffffffff), juce::Colour (0xfff5f5f5), juce::Colour (0xff8b9098), juce::Colour (0x55485058), "left", "minimal", "Default");
            else if (themeId == "compact-daw")
                setCommon (juce::Colour (0xff080a0e), juce::Colour (0xff161a20), juce::Colour (0xff72a7ff), juce::Colour (0xfff0f4ff), juce::Colour (0xff929cac), juce::Colour (0xff303946), "left", "square", "Segoe UI");
            else if (themeId == "split-brand")
                setCommon (juce::Colour (0xff0b0806), juce::Colour (0xff18110b), juce::Colour (0xffff7a2a), juce::Colour (0xffffefe2), juce::Colour (0xffb99076), juce::Colour (0xff5d321b), "left", "filled", "Verdana");
            else if (themeId == "neon-strip")
                setCommon (juce::Colour (0xff04070a), juce::Colour (0xee071018), juce::Colour (0xff18f6ff), juce::Colour (0xfff2feff), juce::Colour (0xff7ec7d0), juce::Colour (0xff1e6c78), "center", "outlined", "Consolas");
            else if (themeId == "glass")
                setCommon (juce::Colour (0xff080b12), juce::Colour (0x99182330), juce::Colour (0xff8f6cff), juce::Colour (0xfff7f3ff), juce::Colour (0xffaaa2c4), juce::Colour (0x99a8c7ff), "center", "pill", "Segoe UI");
            else if (themeId == "dark-utility")
                setCommon (juce::Colour (0xff050607), juce::Colour (0xff111318), juce::Colour (0xfff5a623), juce::Colour (0xffe8edf5), juce::Colour (0xff8d96a3), juce::Colour (0xff2f3540), "left", "outlined", "Default");
            else if (themeId == "artist-card")
                setCommon (juce::Colour (0xff090807), juce::Colour (0xff1a1410), juce::Colour (0xffffcf73), juce::Colour (0xfffff2d5), juce::Colour (0xffc0a47d), juce::Colour (0xff75532b), "left", "pill", "Georgia");
            else if (themeId == "logo-rail")
                setCommon (juce::Colour (0xff07090f), juce::Colour (0xff10151d), juce::Colour (0xff5cf0c8), juce::Colour (0xffecfffa), juce::Colour (0xff87aaa2), juce::Colour (0xff1f5f55), "left", "square", "Verdana");
            else if (themeId == "clean-pro")
                setCommon (juce::Colour (0xff0b0d10), juce::Colour (0xff171b22), juce::Colour (0xffd8e4ff), juce::Colour (0xfff3f6fb), juce::Colour (0xff9aa4b4), juce::Colour (0xff3a4351), "left", "minimal", "Segoe UI");
            else if (themeId == "wide-banner")
                setCommon (juce::Colour (0xff07070a), juce::Colour (0xff0f1420), juce::Colour (0xffb86cff), juce::Colour (0xfff6efff), juce::Colour (0xffaf9ac8), juce::Colour (0xff50336f), "center", "filled", "Georgia");
            else if (themeId == "bottom-tools")
                setCommon (juce::Colour (0xff06090c), juce::Colour (0xff111820), juce::Colour (0xff00d2ff), juce::Colour (0xffe9fbff), juce::Colour (0xff87a7b0), juce::Colour (0xff264b57), "left", "outlined", "Consolas");
            else if (themeId == "no-chrome")
            {
                m.playerShowTopBar = false;
                m.playerTitleButtonStyle = "minimal";
                m.playerTitleTextPlacement = "hidden";
            }
            else if (themeId == "custom")
            {
                m.playerShowTopBar = true;
                if (m.playerTitleButtonStyle.isEmpty()) m.playerTitleButtonStyle = "outlined";
                if (m.playerTitleTextPlacement.isEmpty()) m.playerTitleTextPlacement = "left";
            }
            else
                setCommon (juce::Colour (0xff0b0d10), juce::Colour (0xff15171b), juce::Colour (0xfff5a623), juce::Colour (0xffe6e6e6), juce::Colour (0xff8b9098), juce::Colour (0xff2a2a2a), "left", "outlined", "Default");
        }

        static bool isRuntimeAudioFile (const juce::File& file)
        {
            const auto ext = file.getFileExtension().toLowerCase();
            return file.existsAsFile()
                && (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac");
        }

        static bool isRuntimeMidiFile (const juce::File& file)
        {
            const auto ext = file.getFileExtension().toLowerCase();
            return file.existsAsFile() && (ext == ".mid" || ext == ".midi");
        }

        static bool isRuntimeImportPath (const juce::File& file)
        {
            return file.isDirectory() || isRuntimeAudioFile (file) || isRuntimeMidiFile (file);
        }

        static void collectRuntimeImportFiles (const juce::StringArray& paths,
                                               juce::Array<juce::File>& audioFiles,
                                               juce::Array<juce::File>& midiFiles)
        {
            for (const auto& path : paths)
            {
                const juce::File file (path);
                if (file.isDirectory())
                {
                    juce::Array<juce::File> children;
                    file.findChildFiles (children, juce::File::findFiles, true,
                                         "*.wav;*.aif;*.aiff;*.flac;*.mid;*.midi");
                    for (const auto& child : children)
                    {
                        if (isRuntimeAudioFile (child))
                            audioFiles.add (child);
                        else if (isRuntimeMidiFile (child))
                            midiFiles.add (child);
                    }
                }
                else if (isRuntimeAudioFile (file))
                {
                    audioFiles.add (file);
                }
                else if (isRuntimeMidiFile (file))
                {
                    midiFiles.add (file);
                }
            }
        }
    }

    BrandingLabPage::BrandingLabPage (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);

        header.setText ("Branding Lab", juce::dontSendNotification);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        header.setFont (juce::Font (16.0f, juce::Font::bold));
        addAndMakeVisible (header);

        subtitle.setText ("Brand identity on the left, live Player on the right — logo, colors, title bar, and your layout exactly as buyers will see it. Use top-bar Preview for full-screen play mode.",
                          juce::dontSendNotification);
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        subtitle.setFont (juce::Font (11.5f));
        subtitle.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (subtitle);

        // Field labels + editors.
        struct EditorWiring { juce::Label* label; const char* text; juce::TextEditor* editor; const char* placeholder; };
        const EditorWiring editors[] = {
            { &displayNameLabel, "Display Name",  &displayNameEdit,  "e.g. Cinematic Evolve Pad" },
            { &taglineLabel,     "Tagline",       &taglineEdit,      "Short marketing line" },
            { &creatorLabel,     "Creator",       &creatorEdit,      "Brand or studio name" },
            { &versionLabel,     "Version",       &versionEdit,      "1.0" },
            { &websiteLabel,     "Website",       &websiteEdit,      "https://..." },
            { &logoLabel,        "Logo Image",    &logoPathEdit,     "assets/logo.png" },
            { &titleBannerImageLabel, "Title Banner", &titleBannerImageEdit, "assets/player-title-banner.png  |  target: player width x 66 px; 1280 x 66 default" },
            { &playerBackgroundImageLabel, "Player BG", &playerBackgroundImageEdit, "assets/background.png" },
            { &thumbnailImageLabel, "Library Art", &thumbnailImageEdit, "assets/thumbnail.png" }
        };
        for (const auto& w : editors)
        {
            w.label->setText (w.text, juce::dontSendNotification);
            w.label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            w.label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*w.label);

            w.editor->setMultiLine (false);
            w.editor->setTextToShowWhenEmpty (w.placeholder, PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            w.editor->onTextChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*w.editor);
        }

        for (int i = 0; i < 8; ++i)
        {
            macroNameLabels[i].setText ("Macro " + juce::String (i + 1), juce::dontSendNotification);
            macroNameLabels[i].setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            macroNameLabels[i].setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (macroNameLabels[i]);

            macroNameEdits[i].setMultiLine (false);
            macroNameEdits[i].setTextToShowWhenEmpty ("e.g. TONE", PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            macroNameEdits[i].onTextChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (macroNameEdits[i]);
        }

        titleBarThemeLabel.setText ("Title Theme", juce::dontSendNotification);
        titleTextPlacementLabel.setText ("Title Text", juce::dontSendNotification);
        titleButtonStyleLabel.setText ("Button Style", juce::dontSendNotification);
        titleFontLabel.setText ("Title Font", juce::dontSendNotification);
        for (auto* label : { &titleBarThemeLabel, &titleTextPlacementLabel, &titleButtonStyleLabel, &titleFontLabel })
        {
            label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*label);
        }

        const auto& themeIds = titleBarThemeIds();
        const auto& themeNames = titleBarThemeNames();
        for (int i = 0; i < themeIds.size(); ++i)
            titleBarThemeBox.addItem (themeNames[i], i + 1);
        titleTextPlacementBox.addItem ("Left", 1);
        titleTextPlacementBox.addItem ("Center", 2);
        titleTextPlacementBox.addItem ("Right", 3);
        titleTextPlacementBox.addItem ("Hidden", 4);
        titleButtonStyleBox.addItem ("Outlined", 1);
        titleButtonStyleBox.addItem ("Filled", 2);
        titleButtonStyleBox.addItem ("Minimal", 3);
        titleButtonStyleBox.addItem ("Square", 4);
        titleButtonStyleBox.addItem ("Pill", 5);
        const auto& fontIds = titleFontIds();
        for (int i = 0; i < fontIds.size(); ++i)
            titleFontBox.addItem (fontIds[i], i + 1);
        for (auto* box : { &titleBarThemeBox, &titleTextPlacementBox, &titleButtonStyleBox, &titleFontBox })
        {
            box->onChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*box);
        }
        titleBarThemeBox.onChange = [this]
        {
            if (! syncingFromManifest)
            {
                forceApplyTitleTheme = true;
                writeToManifest();
            }
        };

        const EditorWiring clientEditors[] = {
            { &clientNameLabel,   "Client / Artist", &clientNameEdit,   "e.g. DJ Name, producer brand, label" },
            { &supportEmailLabel, "Support Email",   &supportEmailEdit, "support@brand.com" },
            { &supportUrlLabel,   "Support URL",     &supportUrlEdit,   "https://brand.com/support" },
            { &manualUrlLabel,    "Manual URL",      &manualUrlEdit,    "https://brand.com/manual" },
            { &storeUrlLabel,     "Store URL",       &storeUrlEdit,     "https://brand.com/shop" },
            { &copyrightLabel,    "Copyright",       &copyrightEdit,    "© 2026 Brand. All rights reserved." },
            { &legalTextLabel,    "Legal / EULA",    &legalTextEdit,    "Short license or usage note shown in About." },
            { &packageNameLabel,  "Package Name",    &packageNameEdit,  "Installer/product name shown to customers" },
            { &publisherLabel,    "Publisher",       &publisherEdit,    "Customer-facing publisher/company" },
            { &productCodeLabel,  "Product Code",    &productCodeEdit,  "SKU or internal product code" },
            { &bundleIdLabel,     "Bundle ID",       &bundleIdEdit,     "com.brand.product" },
            { &windowsVst3PathLabel, "Win VST3 Path", &windowsVst3PathEdit, R"(CommonFilesFolder\VST3)" },
            { &macVst3PathLabel,  "Mac VST3 Path",   &macVst3PathEdit,  "/Library/Audio/Plug-Ins/VST3" },
            { &privacyUrlLabel,   "Privacy URL",     &privacyUrlEdit,   "https://brand.com/privacy" },
            { &installNotesLabel, "Install Notes",   &installNotesEdit, "Installer-specific delivery notes." }
        };
        for (const auto& w : clientEditors)
        {
            w.label->setText (w.text, juce::dontSendNotification);
            w.label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            w.label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*w.label);

            w.editor->setMultiLine (w.editor == &legalTextEdit || w.editor == &installNotesEdit);
            w.editor->setTextToShowWhenEmpty (w.placeholder, PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            w.editor->onTextChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*w.editor);
        }

        browseLogoBtn.setTooltip ("Pick a PNG/JPG that becomes the player’s logo image.");
        browseLogoBtn.onClick = [this] { chooseLogo(); };
        addAndMakeVisible (browseLogoBtn);

        clearLogoBtn.setTooltip ("Remove the logo image so the Player falls back to the title text.");
        clearLogoBtn.onClick = [this]
        {
            logoPathEdit.setText (juce::String(), true);
            writeToManifest();
        };
        addAndMakeVisible (clearLogoBtn);

        browseTitleBannerBtn.setTooltip ("Pick the image used by the Player title/banner area above the toolbar. Target size is Player width x 66 px; default factory size is 1280 x 66.");
        browseTitleBannerBtn.onClick = [this] { chooseImagePath (titleBannerImageEdit, "Pick Player title banner image"); };
        addAndMakeVisible (browseTitleBannerBtn);
        clearTitleBannerBtn.setTooltip ("Clear the Player title banner image path.");
        clearTitleBannerBtn.onClick = [this]
        {
            titleBannerImageEdit.setText (juce::String(), true);
            writeToManifest();
        };
        addAndMakeVisible (clearTitleBannerBtn);

        browsePlayerBackgroundBtn.setTooltip ("Pick the image used behind the exported Player instrument canvas.");
        browsePlayerBackgroundBtn.onClick = [this] { chooseImagePath (playerBackgroundImageEdit, "Pick Player background image"); };
        addAndMakeVisible (browsePlayerBackgroundBtn);
        clearPlayerBackgroundBtn.setTooltip ("Clear the Player background image path.");
        clearPlayerBackgroundBtn.onClick = [this]
        {
            playerBackgroundImageEdit.setText (juce::String(), true);
            writeToManifest();
        };
        addAndMakeVisible (clearPlayerBackgroundBtn);

        browseThumbnailBtn.setTooltip ("Pick the artwork shown in the Player library and Plugin.club package listing.");
        browseThumbnailBtn.onClick = [this] { chooseImagePath (thumbnailImageEdit, "Pick library thumbnail artwork"); };
        addAndMakeVisible (browseThumbnailBtn);
        clearThumbnailBtn.setTooltip ("Clear the library thumbnail image path.");
        clearThumbnailBtn.onClick = [this]
        {
            thumbnailImageEdit.setText (juce::String(), true);
            writeToManifest();
        };
        addAndMakeVisible (clearThumbnailBtn);

        struct SwatchWiring { juce::Label* label; const char* text; juce::TextButton* swatch; const char* fieldId; };
        const SwatchWiring swatches[] = {
            { &accentLabel, "Accent",     &accentSwatch, "accent" },
            { &panelLabel,  "Panel",      &panelSwatch,  "panel" },
            { &bgLabel,     "Background", &bgSwatch,     "bg" },
            { &textLabel,   "Text",       &textSwatch,   "text" },
            { &dimTextLabel,"Dim Text",   &dimTextSwatch,"dimText" },
            { &borderLabel, "Border",     &borderSwatch, "border" }
        };
        for (const auto& s : swatches)
        {
            s.label->setText (s.text, juce::dontSendNotification);
            s.label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            s.label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*s.label);

            const juce::String fieldId = s.fieldId;
            s.swatch->setTooltip ("Click to pick a new colour for the " + juce::String (s.text) + " role.");
            s.swatch->onClick = [this, fieldId]
            {
                const auto& mfst = owner.getProject().getManifest();
                const auto current = fieldId == "accent" ? mfst.playerAccentColour
                                   : fieldId == "panel"  ? mfst.playerPanelColour
                                   : fieldId == "bg"     ? mfst.playerBackgroundColour
                                   : fieldId == "text"   ? mfst.playerTextColour
                                   : fieldId == "dimText"? mfst.playerTextDimColour
                                                          : mfst.playerBorderColour;
                chooseColour (fieldId, current);
            };
            addAndMakeVisible (*s.swatch);
        }

        resetColoursBtn.setTooltip ("Restore the default Player skin colours.");
        resetColoursBtn.onClick = [this]
        {
            auto& m = owner.getProject().getManifest();
            m.playerAccentColour     = juce::Colour (0xfff5a623);
            m.playerPanelColour      = juce::Colour (0xff15171b);
            m.playerBackgroundColour = juce::Colour (0xff0b0d10);
            m.playerTextColour       = juce::Colour (0xffe6e6e6);
            m.playerTextDimColour    = juce::Colour (0xff8b9098);
            m.playerBorderColour     = juce::Colour (0xff2a2a2a);
            owner.getProject().notifyChanged();
            if (auto* runtime = owner.getPackRuntime())
                runtime->requestReloadImmediate();
            refresh();
        };
        addAndMakeVisible (resetColoursBtn);

        for (auto* section : { &identitySectionBtn, &skinSectionBtn, &runtimeSectionBtn, &clientSectionBtn, &licensingSectionBtn })
        {
            section->getProperties().set ("toolbarIcon", true);
            section->getProperties().set ("fontSize", 11.5);
            section->getProperties().set ("bold", true);
            section->setTooltip ("Expand or collapse this Brand Lab settings group.");
            addAndMakeVisible (*section);
        }
        identitySectionBtn.onClick = [this] { identitySectionOpen = ! identitySectionOpen; resized(); };
        skinSectionBtn.onClick = [this] { skinSectionOpen = ! skinSectionOpen; resized(); };
        runtimeSectionBtn.onClick = [this] { runtimeSectionOpen = ! runtimeSectionOpen; resized(); };
        clientSectionBtn.onClick = [this]
        {
            clientSectionOpen = ! clientSectionOpen;
            resized();
            if (clientSectionOpen)
                formViewport.setViewPosition (0, juce::jmax (0, clientSectionBtn.getY() - 20));
        };
        licensingSectionBtn.onClick = [this]
        {
            licensingSectionOpen = ! licensingSectionOpen;
            resized();
            if (licensingSectionOpen)
                formViewport.setViewPosition (0, juce::jmax (0, licensingSectionBtn.getY() - 20));
        };

        runtimeSectionLabel.setText ("PLAYER FEATURES", juce::dontSendNotification);
        runtimeSectionLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        runtimeSectionLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (runtimeSectionLabel);

        clientSectionLabel.setText ("CLIENT PACKAGE", juce::dontSendNotification);
        clientSectionLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        clientSectionLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (clientSectionLabel);

        for (auto* toggle : { &showPackMenuToggle, &allowPackLoadingToggle, &showLibraryToggle,
                              &allowMidiLearnToggle, &showAboutToggle, &showGuidanceToggle,
                              &showTopBarToggle, &showLeftSidebarToggle, &showFooterToggle,
                              &showRightPanelToggle, &showKeyboardToggle,
                              &topBrowseToggle, &topSaveToggle, &topSettingsToggle,
                              &topCategoryToggle, &topFavoriteToggle, &topPresetNavToggle,
                              &topMasterToggle, &topMeterToggle,
                              &licenseRequiredToggle, &bindMachineToggle,
                              &includeVst3Toggle, &includeStandaloneToggle, &requireLicenseFirstRunToggle,
                              &rightPanelShowMacrosToggle, &rightPanelShowEffectsToggle,
                              &rightPanelShowSendsToggle, &rightPanelShowUtilityToggle })
        {
            toggle->setColour (juce::ToggleButton::textColourId, PatchCraftLookAndFeel::text());
            toggle->onClick = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*toggle);
        }
        includeVst3Toggle.setTooltip ("Include the branded VST3 plugin in generated installer manifests when the VST Exporter expansion is installed.");
        includeStandaloneToggle.setTooltip ("Reserve room for a standalone app payload in the installer manifest.");
        requireLicenseFirstRunToggle.setTooltip ("Tell the Player installer/activation flow to require Plugin.club licensing before first use. AudiLock replaces this backend later.");

        commerceSectionLabel.setText ("SELLER / LICENSING", juce::dontSendNotification);
        commerceSectionLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        commerceSectionLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (commerceSectionLabel);

        productIdLabel.setText ("Product ID", juce::dontSendNotification);
        licenseUrlLabel.setText ("License URL", juce::dontSendNotification);
        trialDaysLabel.setText ("Trial Days", juce::dontSendNotification);
        offlineGraceLabel.setText ("Offline Grace", juce::dontSendNotification);
        for (auto* label : { &productIdLabel, &licenseUrlLabel, &trialDaysLabel, &offlineGraceLabel })
        {
            label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*label);
        }

        for (auto* editor : { &productIdEdit, &licenseUrlEdit })
        {
            editor->setMultiLine (false);
            editor->onTextChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*editor);
        }
        productIdEdit.setTextToShowWhenEmpty ("customer-product-id", PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
        licenseUrlEdit.setTextToShowWhenEmpty ("https://plugin.club/functions/v1/activateLicense", PatchCraftLookAndFeel::textDim().withAlpha (0.65f));

        for (auto* slider : { &trialDaysSlider, &offlineGraceSlider })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
            slider->onValueChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*slider);
        }
        trialDaysSlider.setRange (0.0, 90.0, 1.0);
        offlineGraceSlider.setRange (0.0, 90.0, 1.0);
        trialDaysSlider.setTextValueSuffix (" d");
        offlineGraceSlider.setTextValueSuffix (" d");

        applyWhiteLabelPresetBtn.setTooltip ("Set safe defaults for a customer-owned Player: no generic pack menu, no external pack loading, MIDI learn on, About panel on.");
        applyWhiteLabelPresetBtn.onClick = [this]
        {
            auto& m = owner.getProject().getManifest();
            m.playerShowPackMenu = false;
            m.playerAllowPackLoading = false;
            m.playerShowLibraryBrowser = false;
            m.playerAllowMidiLearn = true;
            m.playerShowAbout = true;
            m.playerShowParameterGuidance = true;
            m.playerShowPatchCraftBranding = false;
            m.playerShowTopBar = true;
            m.playerShowLeftSidebar = false;
            m.playerShowFooter = true;
            m.playerShowRightPanel = true;
            m.playerShowKeyboard = true;
            m.playerTopShowBrowse = false;
            m.playerTopShowSave = true;
            m.playerTopShowSettings = true;
            m.playerTopShowCategory = false;
            m.playerTopShowFavorite = true;
            m.playerTopShowPresetNav = true;
            m.playerTopShowMasterVolume = true;
            m.playerTopShowOutputMeter = true;
            m.rightPanelShowMacros = true;
            m.rightPanelShowEffects = true;
            m.rightPanelShowSends = true;
            m.rightPanelShowUtility = true;
            m.licenseRequired = true;
            m.whiteLabelIncludeVst3 = true;
            m.whiteLabelIncludeStandalone = true;
            m.whiteLabelRequireLicenseOnFirstRun = true;
            if (m.whiteLabelPackageName.isEmpty())
                m.whiteLabelPackageName = m.playerDisplayName.isNotEmpty() ? m.playerDisplayName : m.instrumentName;
            if (m.whiteLabelPublisher.isEmpty())
                m.whiteLabelPublisher = m.playerClientName.isNotEmpty() ? m.playerClientName : m.creator;
            if (m.whiteLabelProductCode.isEmpty())
                m.whiteLabelProductCode = m.instrumentName.toUpperCase().replaceCharacters (" \\/:*?\"<>|", "__________");
            if (m.whiteLabelBundleIdentifier.isEmpty())
                m.whiteLabelBundleIdentifier = "com."
                    + m.creator.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789")
                    + "."
                    + m.instrumentName.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
            if (m.playerClientName.isEmpty())
                m.playerClientName = m.creator;
            if (m.playerSupportUrl.isEmpty() && m.website.isNotEmpty())
                m.playerSupportUrl = m.website;
            if (m.playerStoreUrl.isEmpty() && m.website.isNotEmpty())
                m.playerStoreUrl = m.website;
            if (m.playerCopyright.isEmpty())
                m.playerCopyright = juce::String ("© ")
                    + juce::String (juce::Time::getCurrentTime().getYear())
                    + " " + (m.playerClientName.isNotEmpty() ? m.playerClientName : m.creator)
                    + ". All rights reserved.";
            if (m.licenseProductId.isEmpty())
                m.licenseProductId = m.instrumentName.toLowerCase().replaceCharacters (" \\/:*?\"<>|", "__________");
            owner.getProject().notifyChanged();
            if (auto* runtime = owner.getPackRuntime())
                runtime->requestReloadImmediate();
            refresh();
        };
        addAndMakeVisible (applyWhiteLabelPresetBtn);

        formViewport.setViewedComponent (&formContent, false);
        formViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (formViewport);
        for (auto* component : {
                 static_cast<juce::Component*> (&identitySectionBtn), static_cast<juce::Component*> (&skinSectionBtn),
                 static_cast<juce::Component*> (&runtimeSectionBtn), static_cast<juce::Component*> (&licensingSectionBtn),
                 static_cast<juce::Component*> (&displayNameLabel), static_cast<juce::Component*> (&displayNameEdit),
                 static_cast<juce::Component*> (&taglineLabel), static_cast<juce::Component*> (&taglineEdit),
                 static_cast<juce::Component*> (&creatorLabel), static_cast<juce::Component*> (&creatorEdit),
                 static_cast<juce::Component*> (&websiteLabel), static_cast<juce::Component*> (&websiteEdit),
                 static_cast<juce::Component*> (&versionLabel), static_cast<juce::Component*> (&versionEdit),
                 static_cast<juce::Component*> (&logoLabel), static_cast<juce::Component*> (&logoPathEdit),
                 static_cast<juce::Component*> (&browseLogoBtn), static_cast<juce::Component*> (&clearLogoBtn),
                 static_cast<juce::Component*> (&titleBannerImageLabel), static_cast<juce::Component*> (&titleBannerImageEdit),
                 static_cast<juce::Component*> (&browseTitleBannerBtn), static_cast<juce::Component*> (&clearTitleBannerBtn),
                 static_cast<juce::Component*> (&titleBarThemeLabel), static_cast<juce::Component*> (&titleBarThemeBox),
                 static_cast<juce::Component*> (&titleTextPlacementLabel), static_cast<juce::Component*> (&titleTextPlacementBox),
                 static_cast<juce::Component*> (&titleButtonStyleLabel), static_cast<juce::Component*> (&titleButtonStyleBox),
                 static_cast<juce::Component*> (&titleFontLabel), static_cast<juce::Component*> (&titleFontBox),
                 static_cast<juce::Component*> (&playerBackgroundImageLabel), static_cast<juce::Component*> (&playerBackgroundImageEdit),
                 static_cast<juce::Component*> (&browsePlayerBackgroundBtn), static_cast<juce::Component*> (&clearPlayerBackgroundBtn),
                 static_cast<juce::Component*> (&thumbnailImageLabel), static_cast<juce::Component*> (&thumbnailImageEdit),
                 static_cast<juce::Component*> (&browseThumbnailBtn), static_cast<juce::Component*> (&clearThumbnailBtn),
                 static_cast<juce::Component*> (&accentLabel), static_cast<juce::Component*> (&accentSwatch),
                 static_cast<juce::Component*> (&panelLabel), static_cast<juce::Component*> (&panelSwatch),
                 static_cast<juce::Component*> (&bgLabel), static_cast<juce::Component*> (&bgSwatch),
                 static_cast<juce::Component*> (&textLabel), static_cast<juce::Component*> (&textSwatch),
                 static_cast<juce::Component*> (&dimTextLabel), static_cast<juce::Component*> (&dimTextSwatch),
                 static_cast<juce::Component*> (&borderLabel), static_cast<juce::Component*> (&borderSwatch),
                 static_cast<juce::Component*> (&resetColoursBtn),
                 static_cast<juce::Component*> (&runtimeSectionLabel),
                 static_cast<juce::Component*> (&showPackMenuToggle), static_cast<juce::Component*> (&allowPackLoadingToggle),
                 static_cast<juce::Component*> (&showLibraryToggle), static_cast<juce::Component*> (&allowMidiLearnToggle),
                 static_cast<juce::Component*> (&showAboutToggle), static_cast<juce::Component*> (&showGuidanceToggle),
                 static_cast<juce::Component*> (&showTopBarToggle), static_cast<juce::Component*> (&showLeftSidebarToggle),
                 static_cast<juce::Component*> (&showFooterToggle),
                 static_cast<juce::Component*> (&topBrowseToggle), static_cast<juce::Component*> (&topSaveToggle),
                 static_cast<juce::Component*> (&topSettingsToggle), static_cast<juce::Component*> (&topCategoryToggle),
                 static_cast<juce::Component*> (&topFavoriteToggle), static_cast<juce::Component*> (&topPresetNavToggle),
                 static_cast<juce::Component*> (&clientSectionBtn), static_cast<juce::Component*> (&clientSectionLabel),
                 static_cast<juce::Component*> (&clientNameLabel), static_cast<juce::Component*> (&clientNameEdit),
                 static_cast<juce::Component*> (&supportEmailLabel), static_cast<juce::Component*> (&supportEmailEdit),
                 static_cast<juce::Component*> (&supportUrlLabel), static_cast<juce::Component*> (&supportUrlEdit),
                 static_cast<juce::Component*> (&manualUrlLabel), static_cast<juce::Component*> (&manualUrlEdit),
                 static_cast<juce::Component*> (&storeUrlLabel), static_cast<juce::Component*> (&storeUrlEdit),
                 static_cast<juce::Component*> (&copyrightLabel), static_cast<juce::Component*> (&copyrightEdit),
                 static_cast<juce::Component*> (&legalTextLabel), static_cast<juce::Component*> (&legalTextEdit),
                 static_cast<juce::Component*> (&packageNameLabel), static_cast<juce::Component*> (&packageNameEdit),
                 static_cast<juce::Component*> (&publisherLabel), static_cast<juce::Component*> (&publisherEdit),
                 static_cast<juce::Component*> (&productCodeLabel), static_cast<juce::Component*> (&productCodeEdit),
                 static_cast<juce::Component*> (&bundleIdLabel), static_cast<juce::Component*> (&bundleIdEdit),
                 static_cast<juce::Component*> (&windowsVst3PathLabel), static_cast<juce::Component*> (&windowsVst3PathEdit),
                 static_cast<juce::Component*> (&macVst3PathLabel), static_cast<juce::Component*> (&macVst3PathEdit),
                 static_cast<juce::Component*> (&privacyUrlLabel), static_cast<juce::Component*> (&privacyUrlEdit),
                 static_cast<juce::Component*> (&installNotesLabel), static_cast<juce::Component*> (&installNotesEdit),
                 static_cast<juce::Component*> (&includeVst3Toggle), static_cast<juce::Component*> (&includeStandaloneToggle),
                 static_cast<juce::Component*> (&requireLicenseFirstRunToggle),
                 static_cast<juce::Component*> (&commerceSectionLabel),
                 static_cast<juce::Component*> (&licenseRequiredToggle), static_cast<juce::Component*> (&bindMachineToggle),
                 static_cast<juce::Component*> (&productIdLabel), static_cast<juce::Component*> (&productIdEdit),
                 static_cast<juce::Component*> (&licenseUrlLabel), static_cast<juce::Component*> (&licenseUrlEdit),
                 static_cast<juce::Component*> (&trialDaysLabel), static_cast<juce::Component*> (&trialDaysSlider),
                 static_cast<juce::Component*> (&offlineGraceLabel), static_cast<juce::Component*> (&offlineGraceSlider),
                 static_cast<juce::Component*> (&applyWhiteLabelPresetBtn) })
            formContent.addAndMakeVisible (*component);

        // Shared pack runtime preview (owned by StudioMainComponent).
        runtimeDropStatusLabel.setVisible (false);
        runtimeDropStatusLabel.setJustificationType (juce::Justification::centredLeft);
        runtimeDropStatusLabel.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        runtimeDropStatusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        runtimeDropStatusLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xdd11161e));
        addChildComponent (runtimeDropStatusLabel);

        showFormToggle.setToggleState (false, juce::dontSendNotification);
        showFormToggle.setTooltip ("Show the branding form column (logo, colors, title bar). Off = full-width live Player.");
        showFormToggle.onClick = [this]
        {
            dawPreviewLayout = ! showFormToggle.getToggleState();
            resized();
            repaint();
        };
        addAndMakeVisible (showFormToggle);

        startTimerHz (4);
        refresh();
        if (auto* runtime = owner.getPackRuntime())
            runtime->requestReload();
    }

    BrandingLabPage::~BrandingLabPage()
    {
        deactivateTest();
    }

    void BrandingLabPage::activateTest()
    {
        if (auto* runtime = owner.getPackRuntime())
        {
            juce::Component::SafePointer<PackRuntimeHost> safeRuntime (runtime);
            juce::MessageManager::callAsync ([safeRuntime]
            {
                if (safeRuntime != nullptr)
                    safeRuntime->activate();
            });
        }
    }

    void BrandingLabPage::deactivateTest()
    {
        if (auto* runtime = owner.getPackRuntime())
            runtime->deactivate();
    }

    bool BrandingLabPage::isTestActive() const
    {
        return owner.getPackRuntime() != nullptr && owner.getPackRuntime()->isAudioRunning();
    }

    void BrandingLabPage::setDawPreviewLayout (bool dawPreview)
    {
        dawPreviewLayout = dawPreview;
        showFormToggle.setToggleState (! dawPreview, juce::dontSendNotification);
        resized();
        if (auto* runtime = owner.getPackRuntime())
        {
            runtime->requestReload();
            if (dawPreview)
            {
                juce::Component::SafePointer<PackRuntimeHost> safeRuntime (runtime);
                juce::MessageManager::callAsync ([safeRuntime]
                {
                    if (safeRuntime != nullptr)
                        safeRuntime->activate();
                });
            }
        }
        repaint();
    }

    void BrandingLabPage::timerCallback()
    {
        const bool typing = displayNameEdit.hasKeyboardFocus (true)
                         || taglineEdit.hasKeyboardFocus (true)
                         || creatorEdit.hasKeyboardFocus (true)
                         || websiteEdit.hasKeyboardFocus (true)
                         || versionEdit.hasKeyboardFocus (true)
                         || logoPathEdit.hasKeyboardFocus (true)
                         || clientNameEdit.hasKeyboardFocus (true)
                         || supportEmailEdit.hasKeyboardFocus (true)
                         || supportUrlEdit.hasKeyboardFocus (true)
                         || manualUrlEdit.hasKeyboardFocus (true)
                         || storeUrlEdit.hasKeyboardFocus (true)
                         || copyrightEdit.hasKeyboardFocus (true)
                         || legalTextEdit.hasKeyboardFocus (true);

        if (! typing)
            readFromManifest();

        // Debounced broadcast: fan the project-changed event out to the rest
        // of the app only after the user has paused. Per-keystroke broadcasts
        // refresh every panel in Studio and slow typing to a crawl.
        if (pendingProjectNotify)
        {
            ++ticksSinceLastEdit;
            if (ticksSinceLastEdit >= 2)         // ~500ms at 4 Hz
            {
                pendingProjectNotify = false;
                ticksSinceLastEdit = 0;
                owner.getProject().notifyChanged();
                if (auto* runtime = owner.getPackRuntime())
                    runtime->requestReload();
            }
        }

        if (runtimeDropStatusTicks > 0)
        {
            --runtimeDropStatusTicks;
            if (runtimeDropStatusTicks == 0)
                runtimeDropStatusLabel.setVisible (false);
        }
    }

    void BrandingLabPage::refresh()
    {
        readFromManifest();
        ensurePreviewAttached();
    }

    void BrandingLabPage::ensurePreviewAttached()
    {
        if (! isVisible() || getWidth() <= 0 || getHeight() <= 0)
            return;
        resized();
    }

    bool BrandingLabPage::isInterestedInDragSource (const SourceDetails& details)
    {
        if (auto* object = details.description.getDynamicObject())
            return object->getProperty ("patchcraftDragType").toString() == "libraryAsset";
        return false;
    }

    bool BrandingLabPage::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& path : files)
            if (isRuntimeImportPath (juce::File (path)))
                return true;
        return false;
    }

    void BrandingLabPage::filesDropped (const juce::StringArray& files, int x, int y)
    {
        importRuntimeFilesAt (files, { x, y });
    }

    void BrandingLabPage::showRuntimeDropStatus (const juce::String& message, bool warning)
    {
        runtimeDropStatusLabel.setText (message, juce::dontSendNotification);
        runtimeDropStatusLabel.setColour (juce::Label::backgroundColourId,
                                          warning ? juce::Colour (0xdd5a2718)
                                                  : juce::Colour (0xdd11161e));
        runtimeDropStatusLabel.setColour (juce::Label::textColourId,
                                          warning ? juce::Colour (0xffffd08a)
                                                  : juce::Colours::white);
        runtimeDropStatusLabel.setVisible (true);
        runtimeDropStatusLabel.toFront (false);
        runtimeDropStatusTicks = 120;
        repaint();
    }

    void BrandingLabPage::importRuntimeFilesAt (const juce::StringArray& paths, juce::Point<int> localPosition)
    {
        juce::Array<juce::File> audioFiles;
        juce::Array<juce::File> midiFiles;
        collectRuntimeImportFiles (paths, audioFiles, midiFiles);

        if (audioFiles.isEmpty() && midiFiles.isEmpty())
            return;

        if (audioFiles.isEmpty() && midiFiles.isEmpty())
            return;

        if (! audioFiles.isEmpty())
        {
            if (auto* runtime = owner.getPackRuntime(); runtime != nullptr && ! runtime->isAudioRunning())
                runtime->activate();

            owner.importSampleFiles (audioFiles, false, false, "keyboard", 60, -1);
        }

        juce::String report;
        bool ok = true;
        if (! audioFiles.isEmpty())
        {
            report = "Imported " + juce::String (audioFiles.size()) + " sample"
                   + (audioFiles.size() == 1 ? "" : "s") + " to the active sample map.";
        }

        if (! midiFiles.isEmpty())
        {
            const bool shouldAssignToSampleZone = owner.getProject().getSampleMap().getZones().size() == 1;

            if (shouldAssignToSampleZone)
            {
                juce::String midiReport;
                ok = owner.assignMidiFilesToSampleMap (midiFiles, midiReport, -1, -1);
                if (report.isNotEmpty())
                    report += " ";
                report += midiReport;
            }

            if (auto* runtime = owner.getPackRuntime())
            {
                if (! runtime->isAudioRunning())
                    runtime->activate();

                juce::StringArray paths;
                for (const auto& file : midiFiles)
                    paths.add (file.getFullPathName());

                juce::String clipReport;
                const bool clipLoaded = runtime->importUserContent (paths, clipReport);
                if (report.isNotEmpty())
                    report += " ";
                report += clipReport;
                ok = shouldAssignToSampleZone ? (ok && clipLoaded) : clipLoaded;
            }
        }

        refresh();
        if (report.isNotEmpty())
            showRuntimeDropStatus (report, ! ok);
    }

    void BrandingLabPage::itemDropped (const SourceDetails& details)
    {
        auto* object = details.description.getDynamicObject();
        if (object == nullptr || object->getProperty ("patchcraftDragType").toString() != "libraryAsset")
            return;

        const auto category = object->getProperty ("category").toString();
        const juce::File file (object->getProperty ("path").toString());
        const int frames = juce::jmax (1, (int) object->getProperty ("frames"));
        const bool vertical = (bool) object->getProperty ("vertical");
        juce::StringArray paths;
        if (auto* pathArray = object->getProperty ("paths").getArray())
        {
            for (const auto& path : *pathArray)
                paths.addIfNotAlreadyThere (path.toString());
        }
        if (paths.isEmpty() && file != juce::File())
            paths.add (file.getFullPathName());

        if (previewArea.contains (details.localPosition)
            && details.localPosition.y < previewArea.getY() + kPlayerTitleBarHeight)
        {
            const bool logoDrop = details.localPosition.x < previewArea.getX() + 84;
            auto brandingCategory = category;
            if (logoDrop && ! category.containsIgnoreCase ("title") && ! category.containsIgnoreCase ("banner"))
                brandingCategory = "branding-logos";
            else
                brandingCategory = "branding-titlebars";

            owner.applyBrandingAsset (brandingCategory, file);
            refresh();
            return;
        }

        if (category.startsWithIgnoreCase ("branding"))
        {
            owner.applyBrandingAsset (category, file);
            refresh();
            return;
        }

        if (category == "sounds")
        {
            importRuntimeFilesAt (paths, details.localPosition);
            refresh();
            return;
        }

        owner.addLibraryAssetToCanvas (category, file, frames, vertical);
        refresh();
    }

    void BrandingLabPage::readFromManifest()
    {
        const auto& m = owner.getProject().getManifest();
        syncingFromManifest = true;
        displayNameEdit.setText (m.playerDisplayName, juce::dontSendNotification);
        taglineEdit.setText (m.playerTagline, juce::dontSendNotification);
        creatorEdit.setText (m.creator, juce::dontSendNotification);
        websiteEdit.setText (m.website, juce::dontSendNotification);
        versionEdit.setText (m.version, juce::dontSendNotification);
        logoPathEdit.setText (m.playerLogoImage, juce::dontSendNotification);
        titleBannerImageEdit.setText (m.playerTitleBannerImage, juce::dontSendNotification);
        titleBarThemeBox.setSelectedId (comboIdForValue (titleBarThemeIds(), m.playerTitleBarTheme), juce::dontSendNotification);
        titleTextPlacementBox.setSelectedId (comboIdForValue (titleTextPlacementIds(), m.playerTitleTextPlacement),
                                             juce::dontSendNotification);
        titleButtonStyleBox.setSelectedId (comboIdForValue (titleButtonStyleIds(), m.playerTitleButtonStyle),
                                           juce::dontSendNotification);
        titleFontBox.setSelectedId (comboIdForValue (titleFontIds(), m.playerTitleFontFamily),
                                    juce::dontSendNotification);
        playerBackgroundImageEdit.setText (m.backgroundImage, juce::dontSendNotification);
        thumbnailImageEdit.setText (m.libraryThumbnail, juce::dontSendNotification);
        clientNameEdit.setText (m.playerClientName, juce::dontSendNotification);
        supportEmailEdit.setText (m.playerSupportEmail, juce::dontSendNotification);
        supportUrlEdit.setText (m.playerSupportUrl, juce::dontSendNotification);
        manualUrlEdit.setText (m.playerManualUrl, juce::dontSendNotification);
        storeUrlEdit.setText (m.playerStoreUrl, juce::dontSendNotification);
        copyrightEdit.setText (m.playerCopyright, juce::dontSendNotification);
        legalTextEdit.setText (m.playerLegalText, juce::dontSendNotification);
        packageNameEdit.setText (m.whiteLabelPackageName, juce::dontSendNotification);
        publisherEdit.setText (m.whiteLabelPublisher, juce::dontSendNotification);
        productCodeEdit.setText (m.whiteLabelProductCode, juce::dontSendNotification);
        bundleIdEdit.setText (m.whiteLabelBundleIdentifier, juce::dontSendNotification);
        windowsVst3PathEdit.setText (m.whiteLabelWindowsVst3Path, juce::dontSendNotification);
        macVst3PathEdit.setText (m.whiteLabelMacVst3Path, juce::dontSendNotification);
        privacyUrlEdit.setText (m.whiteLabelPrivacyUrl, juce::dontSendNotification);
        installNotesEdit.setText (m.whiteLabelInstallNotes, juce::dontSendNotification);
        includeVst3Toggle.setToggleState (m.whiteLabelIncludeVst3, juce::dontSendNotification);
        includeStandaloneToggle.setToggleState (m.whiteLabelIncludeStandalone, juce::dontSendNotification);
        requireLicenseFirstRunToggle.setToggleState (m.whiteLabelRequireLicenseOnFirstRun, juce::dontSendNotification);
        showPackMenuToggle.setToggleState (m.playerShowPackMenu, juce::dontSendNotification);
        allowPackLoadingToggle.setToggleState (m.playerAllowPackLoading, juce::dontSendNotification);
        showLibraryToggle.setToggleState (m.playerShowLibraryBrowser, juce::dontSendNotification);
        allowMidiLearnToggle.setToggleState (m.playerAllowMidiLearn, juce::dontSendNotification);
        showAboutToggle.setToggleState (m.playerShowAbout, juce::dontSendNotification);
        showGuidanceToggle.setToggleState (m.playerShowParameterGuidance, juce::dontSendNotification);
        showPatchCraftBrandingToggle.setToggleState (false, juce::dontSendNotification);
        showTopBarToggle.setToggleState (m.playerShowTopBar, juce::dontSendNotification);
        showLeftSidebarToggle.setToggleState (m.playerShowLeftSidebar, juce::dontSendNotification);
        showFooterToggle.setToggleState (m.playerShowFooter, juce::dontSendNotification);
        showRightPanelToggle.setToggleState (m.playerShowRightPanel, juce::dontSendNotification);
        showKeyboardToggle.setToggleState (m.playerShowKeyboard, juce::dontSendNotification);
        topBrowseToggle.setToggleState (m.playerTopShowBrowse, juce::dontSendNotification);
        topSaveToggle.setToggleState (m.playerTopShowSave, juce::dontSendNotification);
        topSettingsToggle.setToggleState (m.playerTopShowSettings, juce::dontSendNotification);
        topCategoryToggle.setToggleState (m.playerTopShowCategory, juce::dontSendNotification);
        topFavoriteToggle.setToggleState (m.playerTopShowFavorite, juce::dontSendNotification);
        topPresetNavToggle.setToggleState (m.playerTopShowPresetNav, juce::dontSendNotification);
        topMasterToggle.setToggleState (m.playerTopShowMasterVolume, juce::dontSendNotification);
        topMeterToggle.setToggleState (m.playerTopShowOutputMeter, juce::dontSendNotification);
        rightPanelShowMacrosToggle.setToggleState (m.rightPanelShowMacros, juce::dontSendNotification);
        rightPanelShowEffectsToggle.setToggleState (m.rightPanelShowEffects, juce::dontSendNotification);
        rightPanelShowSendsToggle.setToggleState (m.rightPanelShowSends, juce::dontSendNotification);
        rightPanelShowUtilityToggle.setToggleState (m.rightPanelShowUtility, juce::dontSendNotification);

        for (int i = 0; i < 8; ++i)
        {
            if (i < m.rightPanelMacroNames.size())
                macroNameEdits[i].setText (m.rightPanelMacroNames[i], juce::dontSendNotification);
            else
                macroNameEdits[i].setText ({}, juce::dontSendNotification);
        }

        licenseRequiredToggle.setToggleState (m.licenseRequired, juce::dontSendNotification);
        bindMachineToggle.setToggleState (m.licenseBindToMachine, juce::dontSendNotification);
        productIdEdit.setText (m.licenseProductId, juce::dontSendNotification);
        licenseUrlEdit.setText (m.licenseServerUrl, juce::dontSendNotification);
        trialDaysSlider.setValue (m.trialDays, juce::dontSendNotification);
        offlineGraceSlider.setValue (m.licenseOfflineGraceDays, juce::dontSendNotification);
        syncingFromManifest = false;
    }

    void BrandingLabPage::writeToManifest()
    {
        auto& m = owner.getProject().getManifest();
        const auto previousTitleTheme = m.playerTitleBarTheme;
        const auto selectedTitleTheme = valueForComboId (titleBarThemeIds(), titleBarThemeBox.getSelectedId());
        const bool titleThemeChanged = forceApplyTitleTheme || selectedTitleTheme != previousTitleTheme;
        forceApplyTitleTheme = false;

        m.playerDisplayName = displayNameEdit.getText().trim();
        m.playerTagline     = taglineEdit.getText().trim();
        m.creator           = creatorEdit.getText().trim();
        m.website           = websiteEdit.getText().trim();
        m.version           = versionEdit.getText().trim();
        m.playerLogoImage   = logoPathEdit.getText().trim();
        m.playerTitleBannerImage = titleBannerImageEdit.getText().trim();
        m.playerTitleBarTheme = selectedTitleTheme;
        m.playerTitleTextPlacement = valueForComboId (titleTextPlacementIds(), titleTextPlacementBox.getSelectedId());
        m.playerTitleButtonStyle = valueForComboId (titleButtonStyleIds(), titleButtonStyleBox.getSelectedId());
        m.playerTitleFontFamily = valueForComboId (titleFontIds(), titleFontBox.getSelectedId());
        m.backgroundImage   = playerBackgroundImageEdit.getText().trim();
        m.libraryThumbnail  = thumbnailImageEdit.getText().trim();
        owner.getProject().backgroundImageRelative = m.backgroundImage;
        m.playerClientName  = clientNameEdit.getText().trim();
        m.playerSupportEmail = supportEmailEdit.getText().trim();
        m.playerSupportUrl  = supportUrlEdit.getText().trim();
        m.playerManualUrl   = manualUrlEdit.getText().trim();
        m.playerStoreUrl    = storeUrlEdit.getText().trim();
        m.playerCopyright   = copyrightEdit.getText().trim();
        m.playerLegalText   = legalTextEdit.getText().trim();
        m.whiteLabelPackageName = packageNameEdit.getText().trim();
        m.whiteLabelPublisher = publisherEdit.getText().trim();
        m.whiteLabelProductCode = productCodeEdit.getText().trim();
        m.whiteLabelBundleIdentifier = bundleIdEdit.getText().trim();
        m.whiteLabelWindowsVst3Path = windowsVst3PathEdit.getText().trim();
        m.whiteLabelMacVst3Path = macVst3PathEdit.getText().trim();
        m.whiteLabelPrivacyUrl = privacyUrlEdit.getText().trim();
        m.whiteLabelInstallNotes = installNotesEdit.getText().trim();
        m.whiteLabelIncludeVst3 = includeVst3Toggle.getToggleState();
        m.whiteLabelIncludeStandalone = includeStandaloneToggle.getToggleState();
        m.whiteLabelRequireLicenseOnFirstRun = requireLicenseFirstRunToggle.getToggleState();
        m.playerShowPackMenu = showPackMenuToggle.getToggleState();
        m.playerAllowPackLoading = allowPackLoadingToggle.getToggleState();
        m.playerShowLibraryBrowser = showLibraryToggle.getToggleState();
        m.playerAllowMidiLearn = allowMidiLearnToggle.getToggleState();
        m.playerShowAbout = showAboutToggle.getToggleState();
        m.playerShowParameterGuidance = showGuidanceToggle.getToggleState();
        m.playerShowPatchCraftBranding = false;
        m.playerShowTopBar = showTopBarToggle.getToggleState()
                           && ! m.playerTitleBarTheme.equalsIgnoreCase ("no-chrome");
        m.playerShowLeftSidebar = showLeftSidebarToggle.getToggleState();
        m.playerShowFooter = showFooterToggle.getToggleState();
        m.playerShowRightPanel = showRightPanelToggle.getToggleState();
        m.playerShowKeyboard = showKeyboardToggle.getToggleState();
        m.playerTopShowBrowse = topBrowseToggle.getToggleState();
        m.playerTopShowSave = topSaveToggle.getToggleState();
        m.playerTopShowSettings = topSettingsToggle.getToggleState();
        m.playerTopShowCategory = topCategoryToggle.getToggleState();
        m.playerTopShowFavorite = topFavoriteToggle.getToggleState();
        m.playerTopShowPresetNav = topPresetNavToggle.getToggleState();
        m.playerTopShowMasterVolume = topMasterToggle.getToggleState();
        m.playerTopShowOutputMeter = topMeterToggle.getToggleState();
        m.rightPanelShowMacros = rightPanelShowMacrosToggle.getToggleState();
        m.rightPanelShowEffects = rightPanelShowEffectsToggle.getToggleState();
        m.rightPanelShowSends = rightPanelShowSendsToggle.getToggleState();
        m.rightPanelShowUtility = rightPanelShowUtilityToggle.getToggleState();

        m.rightPanelMacroNames.clear();
        for (int i = 0; i < 8; ++i)
            m.rightPanelMacroNames.add (macroNameEdits[i].getText().trim());

        m.licenseRequired = licenseRequiredToggle.getToggleState();
        m.licenseBindToMachine = bindMachineToggle.getToggleState();
        m.licenseProductId = productIdEdit.getText().trim();
        m.licenseServerUrl = licenseUrlEdit.getText().trim();
        m.trialDays = juce::roundToInt (trialDaysSlider.getValue());
        m.licenseOfflineGraceDays = juce::roundToInt (offlineGraceSlider.getValue());

        if (titleThemeChanged)
        {
            applyTitleBarThemeRecipe (m, selectedTitleTheme);
            syncingFromManifest = true;
            titleTextPlacementBox.setSelectedId (comboIdForValue (titleTextPlacementIds(), m.playerTitleTextPlacement), juce::dontSendNotification);
            titleButtonStyleBox.setSelectedId (comboIdForValue (titleButtonStyleIds(), m.playerTitleButtonStyle), juce::dontSendNotification);
            titleFontBox.setSelectedId (comboIdForValue (titleFontIds(), m.playerTitleFontFamily), juce::dontSendNotification);
            showTopBarToggle.setToggleState (m.playerShowTopBar, juce::dontSendNotification);
            syncingFromManifest = false;
        }

        owner.getProject().markDirty();
        if (auto* runtime = owner.getPackRuntime())
            runtime->requestReloadImmediate();
        scheduleProjectNotify();
        resized();
        repaint();
    }

    void BrandingLabPage::scheduleProjectNotify()
    {
        pendingProjectNotify = true;
        ticksSinceLastEdit = 0;
    }

    void BrandingLabPage::chooseColour (const juce::String& field, juce::Colour current)
    {
        auto* selector = new juce::ColourSelector (juce::ColourSelector::showColourAtTop
                                                   | juce::ColourSelector::showSliders
                                                   | juce::ColourSelector::showColourspace);
        selector->setSize (380, 360);
        selector->setCurrentColour (current);

        struct Listener : juce::ChangeListener
        {
            BrandingLabPage* page;
            juce::String field;
            juce::ColourSelector* sel;
            void changeListenerCallback (juce::ChangeBroadcaster*) override
            {
                auto& m = page->owner.getProject().getManifest();
                const auto c = sel->getCurrentColour();
                if (field == "accent") m.playerAccentColour = c;
                else if (field == "panel") m.playerPanelColour = c;
                else if (field == "bg") m.playerBackgroundColour = c;
                else if (field == "text") m.playerTextColour = c;
                else if (field == "dimText") m.playerTextDimColour = c;
                else if (field == "border") m.playerBorderColour = c;
                page->owner.getProject().notifyChanged();
                page->repaint();
            }
        };
        auto* l = new Listener();
        l->page = this; l->field = field; l->sel = selector;
        selector->addChangeListener (l);

        juce::CallOutBox::launchAsynchronously (
            std::unique_ptr<juce::Component> (selector),
            [&]
            {
                auto* swatch = field == "accent" ? &accentSwatch
                            : field == "panel"  ? &panelSwatch
                            : field == "bg"     ? &bgSwatch
                            : field == "text"   ? &textSwatch
                            : field == "dimText"? &dimTextSwatch : &borderSwatch;
                return swatch->getScreenBounds();
            }(),
            nullptr);
    }

    void BrandingLabPage::chooseLogo()
    {
        chooseImagePath (logoPathEdit, "Pick logo image");
    }

    void BrandingLabPage::chooseImagePath (juce::TextEditor& targetEditor, const juce::String& title)
    {
        logoChooser = std::make_unique<juce::FileChooser> (
            title, juce::File(), "*.png;*.jpg;*.jpeg;*.svg");
        auto* target = &targetEditor;
        logoChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this, target] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                target->setText (f.getFullPathName(), true);
                writeToManifest();
            });
    }


    void BrandingLabPage::resized()
    {
        auto r = getLocalBounds().reduced (16);

        // Header row: title + form-toggle in the top-right.
        auto headerRow = r.removeFromTop (28);
        showFormToggle.setBounds (headerRow.removeFromRight (180));
        header.setBounds (headerRow);

        subtitle.setBounds (r.removeFromTop (38));
        r.removeFromTop (8);

        // Two-column layout: form on left (collapsible), embedded Player on the right.
        const bool showForm = showFormToggle.getToggleState() && ! dawPreviewLayout;
        const int formW = showForm ? juce::jmin (420, r.getWidth() * 4 / 10) : 0;
        auto formBounds = r.removeFromLeft (formW);
        if (showForm) r.removeFromLeft (16);
        previewArea = r;
        formViewport.setVisible (showForm);
        if (showForm)
        {
            formViewport.setBounds (formBounds);
            formContent.setSize (formBounds.getWidth() - 14, juce::jmax (formContent.getHeight(), formBounds.getHeight()));
        }

        owner.attachPackRuntimePreview (this, previewArea);
        runtimeDropStatusLabel.setBounds (previewArea.reduced (18).removeFromTop (30));
        if (runtimeDropStatusLabel.isVisible())
            runtimeDropStatusLabel.toFront (false);

        if (! showForm) return;

        const int formWidth = showForm ? juce::jmax (1, formBounds.getWidth() - 14)
                                       : juce::jmax (1, formContent.getWidth());
        auto form = juce::Rectangle<int> (0, 0, formWidth, 3200);

        auto row = [&form] (int height)
        {
            auto out = form.removeFromTop (height);
            form.removeFromTop (6);
            return out;
        };

        auto pairRow = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::Component& edit)
        {
            lbl.setBounds (r.removeFromLeft (130));
            edit.setBounds (r);
        };

        auto sectionHeader = [&form] (juce::TextButton& button, bool open)
        {
            button.setButtonText (juce::String (open ? "v  " : ">  ") + button.getName());
            auto bounds = form.removeFromTop (28);
            form.removeFromTop (6);
            button.setBounds (bounds);
        };

        auto showMany = [] (bool visible, std::initializer_list<juce::Component*> components)
        {
            for (auto* component : components)
                component->setVisible (visible);
        };

        identitySectionBtn.setName ("Identity");
        skinSectionBtn.setName ("Skin + White Label");
        runtimeSectionBtn.setName ("Player Features");
        clientSectionBtn.setName ("Client Package");
        licensingSectionBtn.setName ("Licensing + Commerce");

        sectionHeader (identitySectionBtn, identitySectionOpen);
        showMany (identitySectionOpen, { &displayNameLabel, &displayNameEdit, &taglineLabel, &taglineEdit,
                                         &creatorLabel, &creatorEdit, &versionLabel, &versionEdit,
                                         &websiteLabel, &websiteEdit, &logoLabel, &logoPathEdit,
                                         &browseLogoBtn, &clearLogoBtn,
                                         &titleBannerImageLabel, &titleBannerImageEdit,
                                         &browseTitleBannerBtn, &clearTitleBannerBtn,
                                         &titleBarThemeLabel, &titleBarThemeBox,
                                         &titleTextPlacementLabel, &titleTextPlacementBox,
                                         &titleButtonStyleLabel, &titleButtonStyleBox,
                                         &titleFontLabel, &titleFontBox,
                                         &playerBackgroundImageLabel, &playerBackgroundImageEdit,
                                         &browsePlayerBackgroundBtn, &clearPlayerBackgroundBtn,
                                         &thumbnailImageLabel, &thumbnailImageEdit,
                                         &browseThumbnailBtn, &clearThumbnailBtn });
        if (identitySectionOpen)
        {
            pairRow (row (28), displayNameLabel, displayNameEdit);
            pairRow (row (28), taglineLabel,     taglineEdit);
            pairRow (row (28), creatorLabel,     creatorEdit);
            pairRow (row (28), versionLabel,     versionEdit);
            pairRow (row (28), websiteLabel,     websiteEdit);

            // Logo row uses three controls (label, edit, browse, clear).
            auto logoRow = row (28);
            logoLabel.setBounds (logoRow.removeFromLeft (130));
            clearLogoBtn.setBounds (logoRow.removeFromRight (60));
            logoRow.removeFromRight (4);
            browseLogoBtn.setBounds (logoRow.removeFromRight (74));
            logoRow.removeFromRight (6);
            logoPathEdit.setBounds (logoRow);

            auto bannerRow = row (28);
            titleBannerImageLabel.setBounds (bannerRow.removeFromLeft (130));
            clearTitleBannerBtn.setBounds (bannerRow.removeFromRight (60));
            bannerRow.removeFromRight (4);
            browseTitleBannerBtn.setBounds (bannerRow.removeFromRight (74));
            bannerRow.removeFromRight (6);
            titleBannerImageEdit.setBounds (bannerRow);

            auto themeRow = row (28);
            titleBarThemeLabel.setBounds (themeRow.removeFromLeft (130));
            titleBarThemeBox.setBounds (themeRow);

            auto placementRow = row (28);
            titleTextPlacementLabel.setBounds (placementRow.removeFromLeft (130));
            titleButtonStyleBox.setBounds (placementRow.removeFromRight (juce::jmax (120, placementRow.getWidth() / 2 - 6)));
            placementRow.removeFromRight (8);
            titleButtonStyleLabel.setBounds (placementRow.removeFromRight (92));
            titleTextPlacementBox.setBounds (placementRow);

            auto fontRow = row (28);
            titleFontLabel.setBounds (fontRow.removeFromLeft (130));
            titleFontBox.setBounds (fontRow);

            auto backgroundRow = row (28);
            playerBackgroundImageLabel.setBounds (backgroundRow.removeFromLeft (130));
            clearPlayerBackgroundBtn.setBounds (backgroundRow.removeFromRight (60));
            backgroundRow.removeFromRight (4);
            browsePlayerBackgroundBtn.setBounds (backgroundRow.removeFromRight (74));
            backgroundRow.removeFromRight (6);
            playerBackgroundImageEdit.setBounds (backgroundRow);

            auto thumbRow = row (28);
            thumbnailImageLabel.setBounds (thumbRow.removeFromLeft (130));
            clearThumbnailBtn.setBounds (thumbRow.removeFromRight (60));
            thumbRow.removeFromRight (4);
            browseThumbnailBtn.setBounds (thumbRow.removeFromRight (74));
            thumbRow.removeFromRight (6);
            thumbnailImageEdit.setBounds (thumbRow);
        }

        form.removeFromTop (8);
        sectionHeader (skinSectionBtn, skinSectionOpen);
        showMany (skinSectionOpen, { &accentLabel, &accentSwatch, &panelLabel, &panelSwatch,
                                     &bgLabel, &bgSwatch, &textLabel, &textSwatch,
                                     &dimTextLabel, &dimTextSwatch, &borderLabel, &borderSwatch,
                                     &resetColoursBtn });

        // Colour swatches in a grid.
        auto colourRow = [&form] (int rows)
        {
            auto out = form.removeFromTop (32 * rows + 6 * (rows - 1));
            form.removeFromTop (10);
            return out;
        };

        auto swatchPair = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::TextButton& swatch)
        {
            lbl.setBounds (r.removeFromLeft (90));
            swatch.setBounds (r.removeFromLeft (60).reduced (0, 4));
        };

        if (skinSectionOpen)
        {
            auto cRow = colourRow (1);
            swatchPair (cRow.removeFromLeft (160), accentLabel, accentSwatch);
            cRow.removeFromLeft (16);
            swatchPair (cRow.removeFromLeft (160), panelLabel, panelSwatch);

            auto cRow2 = colourRow (1);
            swatchPair (cRow2.removeFromLeft (160), bgLabel, bgSwatch);
            cRow2.removeFromLeft (16);
            swatchPair (cRow2.removeFromLeft (160), textLabel, textSwatch);

            auto cRow3 = colourRow (1);
            swatchPair (cRow3.removeFromLeft (160), dimTextLabel, dimTextSwatch);
            cRow3.removeFromLeft (16);
            swatchPair (cRow3.removeFromLeft (160), borderLabel, borderSwatch);

            resetColoursBtn.setBounds (form.removeFromTop (28));
            form.removeFromTop (12);
        }

        sectionHeader (runtimeSectionBtn, runtimeSectionOpen);
        runtimeSectionLabel.setVisible (false);
        showMany (runtimeSectionOpen, { &showPackMenuToggle, &allowPackLoadingToggle,
                                        &showLibraryToggle, &allowMidiLearnToggle,
                                        &showAboutToggle, &showGuidanceToggle,
                                        &showTopBarToggle, &showLeftSidebarToggle, &showFooterToggle,
                                        &showRightPanelToggle, &showKeyboardToggle,
                                        &topBrowseToggle, &topSaveToggle, &topSettingsToggle,
                                        &topCategoryToggle, &topFavoriteToggle, &topPresetNavToggle,
                                        &topMasterToggle, &topMeterToggle,
                                        &rightPanelShowMacrosToggle, &rightPanelShowEffectsToggle,
                                        &rightPanelShowSendsToggle, &rightPanelShowUtilityToggle });
        showMany (false, { &showPatchCraftBrandingToggle });
        auto togglePair = [&form] (juce::ToggleButton& a, juce::ToggleButton& b)
        {
            auto toggleRow = form.removeFromTop (26);
            a.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2));
            b.setBounds (toggleRow);
            form.removeFromTop (4);
        };
        if (runtimeSectionOpen)
        {
            togglePair (showTopBarToggle, showLeftSidebarToggle);
            togglePair (showRightPanelToggle, showKeyboardToggle);
            togglePair (showFooterToggle, showLibraryToggle);
            togglePair (showPackMenuToggle, allowPackLoadingToggle);
            togglePair (allowMidiLearnToggle, showAboutToggle);
            auto guidanceRow = form.removeFromTop (26);
            showGuidanceToggle.setBounds (guidanceRow);
            form.removeFromTop (4);
            form.removeFromTop (6);
            togglePair (topBrowseToggle, topSaveToggle);
            togglePair (topSettingsToggle, topCategoryToggle);
            togglePair (topFavoriteToggle, topPresetNavToggle);
            togglePair (topMasterToggle, topMeterToggle);
            form.removeFromTop (6);
            togglePair (rightPanelShowMacrosToggle, rightPanelShowEffectsToggle);
            togglePair (rightPanelShowSendsToggle, rightPanelShowUtilityToggle);
            form.removeFromTop (8);

            if (rightPanelShowMacrosToggle.getToggleState())
            {
                auto pairRowHalf = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::Component& edit)
                {
                    lbl.setBounds (r.removeFromLeft (56));
                    edit.setBounds (r);
                };

                for (int i = 0; i < 4; ++i)
                {
                    auto macroRow = form.removeFromTop (26);
                    auto leftArea = macroRow.removeFromLeft (macroRow.getWidth() / 2 - 4);
                    macroRow.removeFromLeft (8);
                    auto rightArea = macroRow;
                    
                    pairRowHalf (leftArea, macroNameLabels[i * 2], macroNameEdits[i * 2]);
                    pairRowHalf (rightArea, macroNameLabels[i * 2 + 1], macroNameEdits[i * 2 + 1]);
                    form.removeFromTop (4);
                }
                form.removeFromTop (4);
            }
        }

        const bool showMacros = runtimeSectionOpen && rightPanelShowMacrosToggle.getToggleState();
        for (int i = 0; i < 8; ++i)
        {
            macroNameLabels[i].setVisible (showMacros);
            macroNameEdits[i].setVisible (showMacros);
        }

        sectionHeader (clientSectionBtn, clientSectionOpen);
        clientSectionLabel.setVisible (false);
        showMany (clientSectionOpen, { &clientNameLabel, &clientNameEdit,
                                       &supportEmailLabel, &supportEmailEdit,
                                       &supportUrlLabel, &supportUrlEdit,
                                       &manualUrlLabel, &manualUrlEdit,
                                       &storeUrlLabel, &storeUrlEdit,
                                       &copyrightLabel, &copyrightEdit,
                                       &legalTextLabel, &legalTextEdit,
                                       &packageNameLabel, &packageNameEdit,
                                       &publisherLabel, &publisherEdit,
                                       &productCodeLabel, &productCodeEdit,
                                       &bundleIdLabel, &bundleIdEdit,
                                       &windowsVst3PathLabel, &windowsVst3PathEdit,
                                       &macVst3PathLabel, &macVst3PathEdit,
                                       &privacyUrlLabel, &privacyUrlEdit,
                                       &installNotesLabel, &installNotesEdit,
                                       &includeVst3Toggle, &includeStandaloneToggle,
                                       &requireLicenseFirstRunToggle });
        if (clientSectionOpen)
        {
            pairRow (row (28), clientNameLabel, clientNameEdit);
            pairRow (row (28), supportEmailLabel, supportEmailEdit);
            pairRow (row (28), supportUrlLabel, supportUrlEdit);
            pairRow (row (28), manualUrlLabel, manualUrlEdit);
            pairRow (row (28), storeUrlLabel, storeUrlEdit);
            pairRow (row (28), copyrightLabel, copyrightEdit);
            auto legalRow = row (64);
            legalTextLabel.setBounds (legalRow.removeFromLeft (130));
            legalTextEdit.setBounds (legalRow);
            form.removeFromTop (8);
            pairRow (row (28), packageNameLabel, packageNameEdit);
            pairRow (row (28), publisherLabel, publisherEdit);
            pairRow (row (28), productCodeLabel, productCodeEdit);
            pairRow (row (28), bundleIdLabel, bundleIdEdit);
            pairRow (row (28), windowsVst3PathLabel, windowsVst3PathEdit);
            pairRow (row (28), macVst3PathLabel, macVst3PathEdit);
            pairRow (row (28), privacyUrlLabel, privacyUrlEdit);
            auto installRow = row (64);
            installNotesLabel.setBounds (installRow.removeFromLeft (130));
            installNotesEdit.setBounds (installRow);
            togglePair (includeVst3Toggle, includeStandaloneToggle);
            auto requireRow = form.removeFromTop (26);
            requireLicenseFirstRunToggle.setBounds (requireRow);
            form.removeFromTop (8);
        }

        sectionHeader (licensingSectionBtn, licensingSectionOpen);
        commerceSectionLabel.setVisible (false);
        showMany (licensingSectionOpen, { &licenseRequiredToggle, &bindMachineToggle,
                                          &productIdLabel, &productIdEdit, &licenseUrlLabel, &licenseUrlEdit,
                                          &trialDaysLabel, &trialDaysSlider, &offlineGraceLabel, &offlineGraceSlider,
                                          &applyWhiteLabelPresetBtn });
        if (licensingSectionOpen)
        {
            togglePair (licenseRequiredToggle, bindMachineToggle);
            pairRow (row (28), productIdLabel, productIdEdit);
            pairRow (row (28), licenseUrlLabel, licenseUrlEdit);
            pairRow (row (28), trialDaysLabel, trialDaysSlider);
            pairRow (row (28), offlineGraceLabel, offlineGraceSlider);
            form.removeFromTop (4);
            applyWhiteLabelPresetBtn.setBounds (form.removeFromTop (30));
        }

        const int contentHeight = juce::jmax (formBounds.getHeight(), form.getY() + 36);
        formContent.setSize (formWidth, contentHeight);
    }

    void BrandingLabPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        // Recolour the swatch buttons to reflect manifest state.
        const auto& m = owner.getProject().getManifest();
        struct SwatchPaint { juce::TextButton* btn; juce::Colour c; };
        const SwatchPaint paints[] = {
            { &accentSwatch, m.playerAccentColour },
            { &panelSwatch,  m.playerPanelColour },
            { &bgSwatch,     m.playerBackgroundColour },
            { &textSwatch,   m.playerTextColour },
            { &dimTextSwatch,m.playerTextDimColour },
            { &borderSwatch, m.playerBorderColour }
        };
        for (const auto& s : paints)
        {
            s.btn->setColour (juce::TextButton::buttonColourId,   s.c);
            s.btn->setColour (juce::TextButton::buttonOnColourId, s.c.brighter (0.10f));
            s.btn->setButtonText (colourToHex (s.c));
            s.btn->setColour (juce::TextButton::textColourOnId,
                              s.c.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
            s.btn->setColour (juce::TextButton::textColourOffId,
                              s.c.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
        }
    }
}
