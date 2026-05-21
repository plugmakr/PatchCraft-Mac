#include "BrandingLabPage.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "TestPage.h"

#include <utility>

namespace patchcraft
{
    namespace
    {
        static juce::String colourToHex (juce::Colour c)
        {
            return "#" + juce::String::toHexString ((int) c.getARGB()).paddedLeft ('0', 8).toUpperCase();
        }

        static void stylePlayerToolbarButton (juce::TextButton& button)
        {
            button.getProperties().set ("toolbarIcon", true);
            button.getProperties().set ("fontSize", 10.5);
            button.getProperties().set ("bold", true);
            button.getProperties().set ("corner", 5.0);
            button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff171b21));
            button.setColour (juce::TextButton::buttonOnColourId, PatchCraftLookAndFeel::accent());
            button.setColour (juce::TextButton::textColourOffId, PatchCraftLookAndFeel::text());
            button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        }

        static juce::Rectangle<int> fitCanvasIntoBounds (CanvasSize canvasSize,
                                                         juce::Rectangle<int> bounds)
        {
            if (canvasSize.width <= 0) canvasSize.width = 1280;
            if (canvasSize.height <= 0) canvasSize.height = 800;
            if (bounds.isEmpty())
                return bounds;

            const float scale = juce::jmin ((float) bounds.getWidth() / (float) canvasSize.width,
                                            (float) bounds.getHeight() / (float) canvasSize.height);
            const int width = juce::jmax (1, juce::roundToInt ((float) canvasSize.width * scale));
            const int height = juce::jmax (1, juce::roundToInt ((float) canvasSize.height * scale));
            return bounds.withSizeKeepingCentre (width, height);
        }
    }

    BrandingLabPage::BrandingLabPage (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);

        header.setText ("Branding Lab", juce::dontSendNotification);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        header.setFont (juce::Font (16.0f, juce::Font::bold));
        addAndMakeVisible (header);

        subtitle.setText ("Brand and test the actual Player experience. Use PLAY/STOP, the keyboard, pads, tab pages, drum-grid banks, and canvas controls here before exporting.",
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
        clientSectionBtn.onClick = [this] { clientSectionOpen = ! clientSectionOpen; resized(); };
        licensingSectionBtn.onClick = [this] { licensingSectionOpen = ! licensingSectionOpen; resized(); };

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
                              &showPatchCraftBrandingToggle, &licenseRequiredToggle, &bindMachineToggle,
                              &includeVst3Toggle, &includeStandaloneToggle, &requireLicenseFirstRunToggle })
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
        licenseUrlEdit.setTextToShowWhenEmpty ("https://plugin.club/functions/deviceAuth", PatchCraftLookAndFeel::textDim().withAlpha (0.65f));

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
                 static_cast<juce::Component*> (&showPatchCraftBrandingToggle),
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

        // Live instrument inside a Player-shaped frame.
        testPage = std::make_unique<TestPage> (owner);
        testPage->setBrandLabPreviewMode (true);
        addAndMakeVisible (*testPage);

        for (auto* button : { &playerFileBtn, &playerLibraryBtn, &playerViewBtn, &playerToolsBtn,
                              &playerSoundBtn, &playerRackBtn, &playerControlBtn, &playerMidiClipBtn,
                              &playerPlayBtn, &playerStopBtn,
                              &playerPrevPresetBtn, &playerPresetBtn, &playerNextPresetBtn })
        {
            stylePlayerToolbarButton (*button);
            addAndMakeVisible (*button);
        }

        playerFileBtn.setButtonText ("F");
        playerLibraryBtn.setButtonText ("LIB");
        playerViewBtn.setButtonText ("VIEW");
        playerToolsBtn.setButtonText ("TOOL");
        playerSoundBtn.setButtonText ("SND");
        playerRackBtn.setButtonText ("RACK");
        playerControlBtn.setButtonText ("CTRL");
        playerMidiClipBtn.setButtonText ("MIDI");
        playerPlayBtn.setButtonText (">");
        playerStopBtn.setButtonText ("■");
        playerPrevPresetBtn.setButtonText ("<");
        playerNextPresetBtn.setButtonText (">");
        playerPresetBtn.getProperties().set ("fontSize", 12.0);

        playerFileBtn.setTooltip ("Open the same Player file/actions menu users see in the plugin.");
        playerFileBtn.onClick = [this] { showPlayerFileMenu(); };
        playerLibraryBtn.setTooltip ("Preview the Player library entry point and pack browser state.");
        playerLibraryBtn.onClick = [this]
        {
            showPlayerPreviewPanel ("Player Library",
                                    "This is where the DAW Player exposes installed patches, user packs, and licensed expansion content. Brand Lab keeps it visible so the exact buyer-facing chrome can be reviewed before export.");
        };
        playerViewBtn.setTooltip ("Toggle the Brand Lab form so the Player can be checked at full DAW size.");
        playerViewBtn.onClick = [this]
        {
            showFormToggle.setToggleState (! showFormToggle.getToggleState(), juce::dontSendNotification);
            if (showFormToggle.onClick)
                showFormToggle.onClick();
        };
        playerToolsBtn.setTooltip ("Open Player tools: MIDI clip editor, runtime diagnostics, and routing-oriented controls.");
        playerToolsBtn.onClick = [this] { showPlayerToolsMenu(); };
        playerSoundBtn.setTooltip ("Preview the Player Sound Control Center surface.");
        playerSoundBtn.onClick = [this]
        {
            showPlayerPreviewPanel ("Sound Control Center",
                                    "Sound Control Center shows only controls exposed by this instrument: synth/sample parameters, FX, drum-pad controls, performance macros, bypass switches, and MIDI learn targets.");
        };
        playerRackBtn.setTooltip ("Preview the Player Rack for stacked instruments, routing, solo/mute, and layer balance.");
        playerRackBtn.onClick = [this]
        {
            showPlayerPreviewPanel ("Rack",
                                    "Rack is the multi-instrument area: each loaded patch gets power, mute, solo, MIDI channel, output route, tuning, pan, and level controls.");
        };
        playerControlBtn.setTooltip ("Preview the Player Control Center for branding, license status, routing, and global mix.");
        playerControlBtn.onClick = [this]
        {
            showPlayerPreviewPanel ("Control Center",
                                    "Control Center contains instrument info, license status, global mix/routing, MIDI learn management, and buyer-facing support links.");
        };
        playerMidiClipBtn.setTooltip ("Open the MIDI clip editor as a popout so the Player preview stays full sized.");
        playerMidiClipBtn.onClick = [this]
        {
            if (testPage != nullptr)
                testPage->showMidiClipEditor();
        };
        playerPlayBtn.setTooltip ("Start the Brand Lab runtime transport. Drum grids, MIDI clips, synced FX, and playheads run here.");
        playerPlayBtn.onClick = [this]
        {
            if (testPage != nullptr)
                testPage->startPreviewPlayback();
            repaint();
        };
        playerStopBtn.setTooltip ("Stop Brand Lab runtime transport and release all active notes.");
        playerStopBtn.onClick = [this]
        {
            if (testPage != nullptr)
                testPage->stopPreviewPlayback();
            repaint();
        };
        playerBpmLabel.setText ("BPM", juce::dontSendNotification);
        playerBpmLabel.setFont (juce::Font (9.5f, juce::Font::bold));
        playerBpmLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        playerBpmLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (playerBpmLabel);
        playerBpmSlider.setRange (40.0, 220.0, 1.0);
        playerBpmSlider.setValue (owner.getProject().getLiveValues().getValue ("projectBpm", 120.0f),
                                  juce::dontSendNotification);
        playerBpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        playerBpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 38, 18);
        playerBpmSlider.setTooltip ("Global Brand Lab tempo. This drives Studio preview, MIDI/drum playback, BPM-synced FX, and exported standalone fallback BPM.");
        playerBpmSlider.onValueChange = [this]
        {
            owner.getProject().getLiveValues().setValue ("projectBpm", (float) playerBpmSlider.getValue());
        };
        addAndMakeVisible (playerBpmSlider);
        playerPrevPresetBtn.setTooltip ("Preview previous preset button placement.");
        playerNextPresetBtn.setTooltip ("Preview next preset button placement.");
        playerPresetBtn.setTooltip ("Current patch/preset selector preview.");
        playerPrevPresetBtn.onClick = [this]
        {
            auto& presets = owner.getProject().getPresets();
            if (presets.empty())
                return;
            int current = 0;
            const auto name = owner.getProject().getManifest().defaultPreset;
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].name == name || presets[(size_t) i].isDefault)
                    current = i;
            current = (current + (int) presets.size() - 1) % (int) presets.size();
            owner.getProject().applyPreset (presets[(size_t) current]);
            refresh();
        };
        playerNextPresetBtn.onClick = [this]
        {
            auto& presets = owner.getProject().getPresets();
            if (presets.empty())
                return;
            int current = 0;
            const auto name = owner.getProject().getManifest().defaultPreset;
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].name == name || presets[(size_t) i].isDefault)
                    current = i;
            current = (current + 1) % (int) presets.size();
            owner.getProject().applyPreset (presets[(size_t) current]);
            refresh();
        };
        playerPresetBtn.onClick = [this]
        {
            showPlayerPresetMenu();
        };

        showFormToggle.setToggleState (true, juce::dontSendNotification);
        showFormToggle.setTooltip ("Toggle the branding form column. Hide it to preview the player at full width.");
        showFormToggle.onClick = [this]
        {
            const bool show = showFormToggle.getToggleState();
            formViewport.setVisible (show);
            displayNameLabel.setVisible (show); displayNameEdit.setVisible (show);
            taglineLabel.setVisible (show);     taglineEdit.setVisible (show);
            creatorLabel.setVisible (show);     creatorEdit.setVisible (show);
            websiteLabel.setVisible (show);     websiteEdit.setVisible (show);
            versionLabel.setVisible (show);     versionEdit.setVisible (show);
            logoLabel.setVisible (show);        logoPathEdit.setVisible (show);
            browseLogoBtn.setVisible (show);    clearLogoBtn.setVisible (show);
            accentLabel.setVisible (show);      accentSwatch.setVisible (show);
            panelLabel.setVisible (show);       panelSwatch.setVisible (show);
            bgLabel.setVisible (show);          bgSwatch.setVisible (show);
            textLabel.setVisible (show);        textSwatch.setVisible (show);
            dimTextLabel.setVisible (show);     dimTextSwatch.setVisible (show);
            borderLabel.setVisible (show);      borderSwatch.setVisible (show);
            resetColoursBtn.setVisible (show);
            runtimeSectionLabel.setVisible (show);
            showPackMenuToggle.setVisible (show);
            allowPackLoadingToggle.setVisible (show);
            showLibraryToggle.setVisible (show);
            allowMidiLearnToggle.setVisible (show);
            showAboutToggle.setVisible (show);
            showGuidanceToggle.setVisible (show);
            showPatchCraftBrandingToggle.setVisible (show);
            clientSectionBtn.setVisible (show);
            clientSectionLabel.setVisible (show);
            clientNameLabel.setVisible (show); clientNameEdit.setVisible (show);
            supportEmailLabel.setVisible (show); supportEmailEdit.setVisible (show);
            supportUrlLabel.setVisible (show); supportUrlEdit.setVisible (show);
            manualUrlLabel.setVisible (show); manualUrlEdit.setVisible (show);
            storeUrlLabel.setVisible (show); storeUrlEdit.setVisible (show);
            copyrightLabel.setVisible (show); copyrightEdit.setVisible (show);
            legalTextLabel.setVisible (show); legalTextEdit.setVisible (show);
            commerceSectionLabel.setVisible (show);
            licenseRequiredToggle.setVisible (show);
            bindMachineToggle.setVisible (show);
            productIdLabel.setVisible (show);   productIdEdit.setVisible (show);
            licenseUrlLabel.setVisible (show);  licenseUrlEdit.setVisible (show);
            trialDaysLabel.setVisible (show);   trialDaysSlider.setVisible (show);
            offlineGraceLabel.setVisible (show); offlineGraceSlider.setVisible (show);
            applyWhiteLabelPresetBtn.setVisible (show);
            resized();
            repaint();
        };
        addAndMakeVisible (showFormToggle);

        startTimerHz (4);
        refresh();
    }

    BrandingLabPage::~BrandingLabPage() = default;

    void BrandingLabPage::activateTest()    { if (testPage) testPage->activate(); }
    void BrandingLabPage::deactivateTest()  { if (testPage) testPage->deactivate(); }
    bool BrandingLabPage::isTestActive() const { return testPage && testPage->isAudioRunning(); }

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
            }
        }

        playerPlayBtn.setButtonText (testPage != nullptr && testPage->isTransportPlaying() ? "ON" : ">");
        repaint();
    }

    void BrandingLabPage::refresh()
    {
        readFromManifest();
        repaint();
    }

    bool BrandingLabPage::isInterestedInDragSource (const SourceDetails& details)
    {
        if (auto* object = details.description.getDynamicObject())
            return object->getProperty ("patchcraftDragType").toString() == "libraryAsset";
        return false;
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

        if (category == "sounds")
        {
            juce::Array<juce::File> files;
            files.add (file);
            owner.importSampleFiles (files, false, false);
            refresh();
            return;
        }

        owner.addLibraryAssetToCanvas (category, file, frames, vertical);
        refresh();
    }

    void BrandingLabPage::readFromManifest()
    {
        const auto& m = owner.getProject().getManifest();
        const auto presetName = m.defaultPreset.isNotEmpty()
            ? m.defaultPreset
            : (owner.getProject().getPresets().empty()
                ? juce::String ("Current Patch")
                : owner.getProject().getPresets().front().name);
        playerPresetBtn.setButtonText (presetName);
        playerBpmSlider.setValue (owner.getProject().getLiveValues().getValue ("projectBpm", 120.0f),
                                  juce::dontSendNotification);
        syncingFromManifest = true;
        displayNameEdit.setText (m.playerDisplayName, juce::dontSendNotification);
        taglineEdit.setText (m.playerTagline, juce::dontSendNotification);
        creatorEdit.setText (m.creator, juce::dontSendNotification);
        websiteEdit.setText (m.website, juce::dontSendNotification);
        versionEdit.setText (m.version, juce::dontSendNotification);
        logoPathEdit.setText (m.playerLogoImage, juce::dontSendNotification);
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
        showPatchCraftBrandingToggle.setToggleState (m.playerShowPatchCraftBranding, juce::dontSendNotification);
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
        m.playerDisplayName = displayNameEdit.getText().trim();
        m.playerTagline     = taglineEdit.getText().trim();
        m.creator           = creatorEdit.getText().trim();
        m.website           = websiteEdit.getText().trim();
        m.version           = versionEdit.getText().trim();
        m.playerLogoImage   = logoPathEdit.getText().trim();
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
        m.playerShowPatchCraftBranding = showPatchCraftBrandingToggle.getToggleState();
        m.licenseRequired = licenseRequiredToggle.getToggleState();
        m.licenseBindToMachine = bindMachineToggle.getToggleState();
        m.licenseProductId = productIdEdit.getText().trim();
        m.licenseServerUrl = licenseUrlEdit.getText().trim();
        m.trialDays = juce::roundToInt (trialDaysSlider.getValue());
        m.licenseOfflineGraceDays = juce::roundToInt (offlineGraceSlider.getValue());
        owner.getProject().markDirty();
        scheduleProjectNotify();
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

    void BrandingLabPage::showPlayerPreviewPanel (const juce::String& titleText, const juce::String& bodyText)
    {
        struct PanelPreview final : public juce::Component
        {
            PanelPreview (juce::String titleIn, juce::String bodyIn)
                : title (std::move (titleIn)), body (std::move (bodyIn))
            {
                setSize (560, 260);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (18);
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.4f);

                auto inner = r.reduced (18);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.setFont (juce::Font (18.0f, juce::Font::bold));
                g.drawText (title, inner.removeFromTop (30), juce::Justification::centredLeft, true);
                inner.removeFromTop (8);
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (juce::Font (13.0f));
                g.drawFittedText (body, inner, juce::Justification::topLeft, 8);
            }

            juce::String title;
            juce::String body;
        };

        auto* panel = new PanelPreview (titleText, bodyText);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = titleText;
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (panel);
        options.launchAsync();
    }

    void BrandingLabPage::showPlayerFileMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Open MIDI Clip Editor");
        menu.addItem (2, showFormToggle.getToggleState() ? "Hide Branding Form" : "Show Branding Form");
        menu.addSeparator();
        menu.addItem (3, "About Player Chrome");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (playerFileBtn),
            [this] (int result)
            {
                if (result == 1 && testPage != nullptr)
                    testPage->showMidiClipEditor();
                else if (result == 2)
                {
                    showFormToggle.setToggleState (! showFormToggle.getToggleState(), juce::dontSendNotification);
                    if (showFormToggle.onClick)
                        showFormToggle.onClick();
                }
                else if (result == 3)
                    showPlayerPreviewPanel ("Player Chrome",
                                            "Brand Lab now previews the same top-level buyer controls as the exported Player: File, Library, View, Tools, Sound, Rack, Control, preset navigation, full instrument body, and full-width keyboard.");
            });
    }

    void BrandingLabPage::showPlayerToolsMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "MIDI Clip Editor");
        menu.addItem (2, "Runtime Routing Overview");
        menu.addItem (3, "MIDI Learn Preview");
        menu.addItem (4, "Full-Width Player Preview");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (playerToolsBtn),
            [this] (int result)
            {
                if (result == 1 && testPage != nullptr)
                    testPage->showMidiClipEditor();
                else if (result == 2)
                    showPlayerPreviewPanel ("Runtime Routing",
                                            "Routing preview covers instrument output, stacked patch routes, mixer lanes, and global controls. These controls are reserved in the Player toolbar so routing does not have to be added as loose overlay buttons.");
                else if (result == 3)
                    showPlayerPreviewPanel ("MIDI Learn",
                                            "MIDI learn is exposed from the Player toolbar and control surfaces. Right-click runtime controls in the exported Player to assign hardware controls when MIDI learn is enabled.");
                else if (result == 4)
                {
                    showFormToggle.setToggleState (false, juce::dontSendNotification);
                    if (showFormToggle.onClick)
                        showFormToggle.onClick();
                }
            });
    }

    void BrandingLabPage::showPlayerPresetMenu()
    {
        auto& presets = owner.getProject().getPresets();
        juce::PopupMenu menu;
        if (presets.empty())
            menu.addItem (1, "No presets in this instrument", false);
        else
        {
            for (int i = 0; i < (int) presets.size(); ++i)
            {
                const auto& preset = presets[(size_t) i];
                const bool current = preset.name == owner.getProject().getManifest().defaultPreset || preset.isDefault;
                menu.addItem (100 + i, preset.name + (current ? "  ✓" : juce::String()), true);
            }
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (playerPresetBtn),
            [this] (int result)
            {
                if (result < 100)
                    return;
                const int index = result - 100;
                auto& presets = owner.getProject().getPresets();
                if (index < 0 || index >= (int) presets.size())
                    return;

                owner.getProject().applyPreset (presets[(size_t) index]);
                refresh();
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

        // Two-column layout: form on left (collapsible), Player-style frame
        // around the live test page on the right.
        const bool showForm = showFormToggle.getToggleState();
        const int formW = showForm ? juce::jmin (420, r.getWidth() * 4 / 10) : 0;
        auto formBounds = r.removeFromLeft (formW);
        if (showForm) r.removeFromLeft (16);
        previewArea = r;
        formViewport.setVisible (showForm);
        if (showForm)
        {
            formViewport.setBounds (formBounds);
            formContent.setSize (formBounds.getWidth() - 14, 760);
        }

        // Reserve the exact DAW-facing Player toolbar strip. The embedded
        // TestPage below runs in Brand Lab preview mode so the body stays full
        // sized and the keyboard spans the whole bottom edge.
        const int headerH = 78;
        auto headerStrip = previewArea.removeFromTop (headerH);
        previewArea.removeFromTop (4);

        auto previewBody = previewArea.reduced (12);
        const int previewKeyboardHeight = juce::jlimit (62, 86, previewArea.getHeight() / 8);
        previewBody.removeFromBottom (previewKeyboardHeight);
        const auto fittedInstrument = fitCanvasIntoBounds (owner.getProject().getCanvasSize(), previewBody);
        playerHeaderArea = fittedInstrument.isEmpty()
            ? headerStrip.reduced (12, 0)
            : juce::Rectangle<int> (fittedInstrument.getX(), headerStrip.getY(),
                                    fittedInstrument.getWidth(), headerStrip.getHeight());

        auto chrome = playerHeaderArea.reduced (10, 7);
        chrome.removeFromTop (34);
        chrome.removeFromTop (4);
        auto toolbar = chrome.withHeight (juce::jmin (30, chrome.getHeight()));

        auto left = toolbar.removeFromLeft (juce::jmin (252, toolbar.getWidth() / 3));
        left.removeFromLeft (48);
        playerFileBtn.setBounds (left.removeFromLeft (34).reduced (2));
        playerLibraryBtn.setBounds (left.removeFromLeft (46).reduced (2));
        playerViewBtn.setBounds (left.removeFromLeft (48).reduced (2));
        playerToolsBtn.setBounds (left.removeFromLeft (50).reduced (2));

        auto right = toolbar.removeFromRight (juce::jmin (260, toolbar.getWidth() / 2));
        playerMidiClipBtn.setBounds (right.removeFromRight (50).reduced (2));
        playerControlBtn.setBounds (right.removeFromRight (50).reduced (2));
        playerRackBtn.setBounds (right.removeFromRight (50).reduced (2));
        playerSoundBtn.setBounds (right.removeFromRight (46).reduced (2));

        auto middle = toolbar;
        const int transportW = juce::jmin (82, juce::jmax (0, middle.getWidth() / 4));
        auto transport = middle.removeFromLeft (transportW);
        playerPlayBtn.setBounds (transport.removeFromLeft (44).reduced (2));
        playerStopBtn.setBounds (transport.reduced (2));

        auto bpm = middle.removeFromLeft (juce::jmin (104, juce::jmax (0, middle.getWidth() / 4)));
        playerBpmLabel.setBounds (bpm.removeFromLeft (28).reduced (0, 2));
        playerBpmSlider.setBounds (bpm.reduced (2));

        const int navW = juce::jlimit (150, 248, middle.getWidth());
        auto nav = middle.withSizeKeepingCentre (navW, 30);
        playerPrevPresetBtn.setBounds (nav.removeFromLeft (34).reduced (2));
        playerNextPresetBtn.setBounds (nav.removeFromRight (34).reduced (2));
        playerPresetBtn.setBounds (nav.reduced (2));

        // The TestPage fills what's left so the user can play their instrument.
        if (testPage) testPage->setBounds (previewArea);

        if (! showForm) return;

        auto form = formContent.getLocalBounds().reduced (0, 0);

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
                                        &showPatchCraftBrandingToggle });
        auto togglePair = [&form] (juce::ToggleButton& a, juce::ToggleButton& b)
        {
            auto toggleRow = form.removeFromTop (26);
            a.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2));
            b.setBounds (toggleRow);
            form.removeFromTop (4);
        };
        if (runtimeSectionOpen)
        {
            togglePair (showPackMenuToggle, allowPackLoadingToggle);
            togglePair (showLibraryToggle, allowMidiLearnToggle);
            togglePair (showAboutToggle, showGuidanceToggle);
            auto creditRow = form.removeFromTop (26);
            showPatchCraftBrandingToggle.setBounds (creditRow);
            form.removeFromTop (4);
            form.removeFromTop (8);
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

        formContent.setSize (formContent.getWidth(), juce::jmax (760, form.getY() + 20));
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

        // Live Player chrome around the embedded TestPage (the actual
        // playable instrument). The header strip carries the branded
        // logo + display name + tagline; the colour stripes mirror the
        // accent / background palette so the developer sees their skin.
        if (! playerHeaderArea.isEmpty())
            paintPlayerPreview (g, playerHeaderArea);
    }

    void BrandingLabPage::paintPlayerPreview (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto& m = owner.getProject().getManifest();
        const auto presetName = m.defaultPreset.isNotEmpty()
            ? m.defaultPreset
            : (owner.getProject().getPresets().empty()
                ? juce::String ("Current Patch")
                : owner.getProject().getPresets().front().name);
        playerPresetBtn.setButtonText (presetName);

        // Branded Player header strip — what the end user sees in their DAW
        // above the instrument layout. The TestPage rendered below is the
        // actual playable instrument; this strip is the chrome around it.
        g.setColour (m.playerBackgroundColour);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (m.playerBorderColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.2f);
        g.setColour (m.playerAccentColour.withAlpha (0.72f));
        g.fillRoundedRectangle (r.withHeight (3).toFloat(), 2.0f);

        auto inner = r.reduced (12, 7);
        auto titleRow = inner.removeFromTop (30);
        inner.removeFromTop (5);
        auto toolRow = inner;

        g.setColour (m.playerPanelColour.withAlpha (0.70f));
        g.fillRoundedRectangle (toolRow.toFloat(), 5.0f);
        g.setColour (m.playerBorderColour.withAlpha (0.72f));
        g.drawRoundedRectangle (toolRow.toFloat().reduced (0.5f), 5.0f, 1.0f);

        // Logo plate on the left.
        const auto logoBox = titleRow.removeFromLeft (30);
        titleRow.removeFromLeft (10);
        const auto logoFile = juce::File::isAbsolutePath (m.playerLogoImage)
            ? juce::File (m.playerLogoImage)
            : owner.getProject().getProjectFolder().getChildFile (m.playerLogoImage);
        juce::Image logo;
        if (m.playerLogoImage.isNotEmpty() && logoFile.existsAsFile())
            logo = juce::ImageFileFormat::loadFrom (logoFile);
        if (logo.isValid())
        {
            g.drawImage (logo, logoBox.toFloat(),
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour (m.playerAccentColour.withAlpha (0.20f));
            g.fillRoundedRectangle (logoBox.toFloat(), 4.0f);
            g.setColour (m.playerAccentColour);
            g.setFont (juce::Font ((float) logoBox.getHeight() * 0.55f, juce::Font::bold));
            const auto initial = m.playerDisplayName.isNotEmpty()
                ? m.playerDisplayName.substring (0, 1)
                : (m.instrumentName.isNotEmpty() ? m.instrumentName.substring (0, 1) : juce::String ("P"));
            g.drawText (initial.toUpperCase(), logoBox, juce::Justification::centred);
        }

        // Title + tagline stacked vertically.
        const auto title = m.playerDisplayName.isNotEmpty() ? m.playerDisplayName : m.instrumentName;
        auto titleColumn = titleRow.removeFromLeft (juce::jmin (420, titleRow.getWidth() / 2));
        auto titleArea = titleColumn.removeFromTop (18);
        g.setColour (m.playerTextColour);
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText (title, titleArea, juce::Justification::centredLeft);

        if (m.playerTagline.isNotEmpty())
        {
            g.setColour (m.playerTextDimColour);
            g.setFont (juce::Font (10.0f));
            g.drawText (m.playerTagline, titleColumn, juce::Justification::centredLeft);
        }

        // Right-side DAW/runtime meta.
        auto rightBlock = juce::Rectangle<int> (r.getRight() - 190, r.getY() + 8, 170, 30);
        g.setColour (m.playerTextDimColour);
        g.setFont (juce::Font (10.0f));
        const auto creator = m.creator.isNotEmpty() ? m.creator : juce::String ("PatchCraft");
        g.drawText (creator + (m.version.isNotEmpty() ? "  v" + m.version : juce::String()),
                    rightBlock.removeFromTop (18), juce::Justification::centredRight);
        g.drawText (m.playerClientName.isNotEmpty()
                        ? "For " + m.playerClientName
                        : (isTestActive() ? "Audio active" : "DAW preview"),
                    rightBlock, juce::Justification::centredRight);

        // The exported Player top bar owns all branding text. Drawing an
        // additional "Powered by" line here collided with the right-side
        // toolbar buttons at smaller widths.
    }
}
