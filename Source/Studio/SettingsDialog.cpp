#include "SettingsDialog.h"
#include "StudioAudioService.h"
#include "AiAssistService.h"
#include "AudiLockSecurity.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    SettingsDialogContent::SettingsDialogContent (StudioAudioService& svc, AiAssistService& aiService, PatchCraftProject& projectToEdit)
        : service (svc),
          ai (aiService),
          project (projectToEdit),
          selector (svc.getDeviceManager(),
                    /*minInputChannels*/  0, /*maxInputChannels*/  8,
                    /*minOutputChannels*/ 1, /*maxOutputChannels*/ 8,
                    /*showMidiInputOptions*/    true,
                    /*showMidiOutputSelector*/  false,
                    /*showChannelsAsStereoPairs*/ true,
                    /*hideAdvancedOptionsWithButton*/ false)
    {
        addAndMakeVisible (tabs);
        tabs.addTab ("Hardware", PatchCraftLookAndFeel::panel(), &hardwareTab, false);
        tabs.addTab ("User Interface", PatchCraftLookAndFeel::panel(), &uiTab, false);
#if PATCHCRAFT_ENABLE_AI_STUDIO
        tabs.addTab ("AI Copilot", PatchCraftLookAndFeel::panel(), &aiTab, false);
        tabs.addTab ("Cloud + Licensing", PatchCraftLookAndFeel::panel(), &cloudTab, false);
#else
        tabs.addTab ("Licensing + Plugin.club", PatchCraftLookAndFeel::panel(), &cloudTab, false);
#endif
        tabs.addTab ("Extensions", PatchCraftLookAndFeel::panel(), &expansionsTab, false);

        header.setText ("HARDWARE  -  AUDIO + MIDI", juce::dontSendNotification);
        header.setFont (juce::Font (12.0f, juce::Font::bold));
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        hardwareTab.addAndMakeVisible (header);
        hardwareTab.addAndMakeVisible (selector);

        uiHeader.setText ("USER INTERFACE", juce::dontSendNotification);
        uiHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        uiHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        uiTab.addAndMakeVisible (uiHeader);

        uiHelp.setText ("Controls Studio guidance popups and the exported Player's parameter help. Turn this off for a cleaner expert workflow.",
                        juce::dontSendNotification);
        uiHelp.setFont (juce::Font (11.0f));
        uiHelp.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        uiHelp.setJustificationType (juce::Justification::topLeft);
        uiTab.addAndMakeVisible (uiHelp);

        showTooltipsToggle.setTooltip ("Show or hide mouseover help throughout Studio and exported Player controls.");
        showTutorialsToggle.setTooltip ("Auto-open guided tutorials for major work areas when enabled.");
        uiSaveButton.setTooltip ("Save the current interface preferences into this project.");
        uiSaveButton.onClick = [this] { saveUiSettings(); };
        showTooltipsToggle.onClick = [this] { saveUiSettings(); };
        showTutorialsToggle.onClick = [this] { saveUiSettings(); };
        for (auto* component : { static_cast<juce::Component*> (&showTooltipsToggle),
                                 static_cast<juce::Component*> (&showTutorialsToggle),
                                 static_cast<juce::Component*> (&uiSaveButton) })
            uiTab.addAndMakeVisible (*component);
        loadUiSettings();

        aiHeader.setText ("LOCAL AI COPILOT", juce::dontSendNotification);
        aiHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        aiHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        aiTab.addAndMakeVisible (aiHeader);

        aiHelp.setText ("Use built-in templates by default, or connect to a local llama.cpp OpenAI-compatible server. "
                        "Recommended premium-light setup: Qwen3 4B GGUF via llama-server.",
                        juce::dontSendNotification);
        aiHelp.setFont (juce::Font (11.0f));
        aiHelp.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        aiHelp.setJustificationType (juce::Justification::topLeft);
        aiTab.addAndMakeVisible (aiHelp);

        aiProviderBox.addItem ("Built-in Templates (no model)", 1);
        aiProviderBox.addItem ("Local llama.cpp Server", 2);
        aiProviderBox.onChange = [this] { refreshAiEnabledState(); };
        aiTab.addAndMakeVisible (aiProviderBox);

        auto setupLabel = [] (juce::Label& label, const juce::String& text)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (juce::Font (10.5f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            label.setJustificationType (juce::Justification::centredLeft);
        };
        setupLabel (aiEndpointLabel, "Endpoint");
        setupLabel (aiModelLabel, "Model");
        setupLabel (aiTimeoutLabel, "Timeout");
        setupLabel (aiTokensLabel, "Max Tokens");
        setupLabel (aiTemperatureLabel, "Temperature");
        setupLabel (imageProviderLabel, "Image");
        setupLabel (imageApiKeyLabel, "Image Key");
        setupLabel (imageModelLabel, "Image Model");
        setupLabel (textProviderLabel, "Text Provider");
        setupLabel (textEndpointLabel, "Text URL");
        setupLabel (textModelLabel, "Text Model");
        setupLabel (textApiKeyLabel, "Text Key");
        setupLabel (murekaApiKeyLabel, "Mureka Key");
        setupLabel (pluginEndpointLabel, "Plugin.club");
        setupLabel (pluginApiKeyLabel, "Plugin Key");
        setupLabel (licenseEndpointLabel, "License URL");
        setupLabel (licensePublicKeyLabel, "Public Key");
        setupLabel (expansionsHeader, "PATCHCRAFT EXTENSIONS");

        for (auto* component : { static_cast<juce::Component*> (&aiEndpointLabel),
                                 static_cast<juce::Component*> (&aiEndpointEditor),
                                 static_cast<juce::Component*> (&aiModelLabel),
                                 static_cast<juce::Component*> (&aiModelEditor),
                                 static_cast<juce::Component*> (&aiTimeoutLabel),
                                 static_cast<juce::Component*> (&aiTimeoutSlider),
                                 static_cast<juce::Component*> (&aiTokensLabel),
                                 static_cast<juce::Component*> (&aiTokensSlider),
                                 static_cast<juce::Component*> (&aiTemperatureLabel),
                                 static_cast<juce::Component*> (&aiTemperatureSlider),
                                 static_cast<juce::Component*> (&aiIncludeContextToggle),
                                 static_cast<juce::Component*> (&aiSaveButton),
                                 static_cast<juce::Component*> (&aiResetButton) })
            aiTab.addAndMakeVisible (*component);

        aiEndpointEditor.setTooltip ("Default llama.cpp server endpoint: http://127.0.0.1:8080/v1/chat/completions");
        aiModelEditor.setTooltip ("Model name sent to the local server. llama.cpp accepts any name if one model is loaded.");
        aiTimeoutSlider.setRange (1000.0, 30000.0, 500.0);
        aiTimeoutSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 18);
        aiTimeoutSlider.setTextValueSuffix (" ms");
        aiTokensSlider.setRange (128.0, 4096.0, 64.0);
        aiTokensSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
        aiTemperatureSlider.setRange (0.0, 1.5, 0.05);
        aiTemperatureSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        aiSaveButton.onClick = [this] { saveAiSettings(); };
        aiResetButton.onClick = [this] { resetAiDefaults(); };
        loadAiSettings();

        cloudHeader.setText ("CLOUD INTEGRATIONS", juce::dontSendNotification);
        cloudHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        cloudHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());

#if PATCHCRAFT_ENABLE_AI_STUDIO
        cloudHelp.setText ("Optional provider keys for generated artwork, Mureka audio workflows, Plugin.club draft publishing, and your licensing platform.",
                           juce::dontSendNotification);
#else
        cloudHelp.setText ("Launch build: Plugin.club publishing and licensing settings only. AudiLock is the future licensing source of truth; use Plugin.club endpoints for this release.",
                           juce::dontSendNotification);
#endif
        cloudHelp.setFont (juce::Font (11.0f));
        cloudHelp.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        cloudHelp.setJustificationType (juce::Justification::topLeft);

        cloudGuideText.setMultiLine (true);
        cloudGuideText.setReadOnly (true);
        cloudGuideText.setScrollbarsShown (true);
        cloudGuideText.setCaretVisible (false);
        cloudGuideText.setFont (juce::Font (12.0f));
        cloudGuideText.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::bg().brighter (0.03f));
        cloudGuideText.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
        cloudGuideText.setColour (juce::TextEditor::focusedOutlineColourId, PatchCraftLookAndFeel::accent());
        cloudGuideText.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
        cloudGuideText.setText (
            "Licensing setup\n"
            "1. Create the product in Plugin.club seller tools for launch. AudiLock will become the canonical licensing source of truth when it is ready.\n"
            "2. Paste the activation / validation endpoint into License URL. Launch default: https://plugin.club/functions/deviceAuth. AudiLock should expose the same contract later.\n"
            "3. Paste the public verification key, public key id, or JWKS key id into Public Key. Never paste a private signing key here.\n"
            "4. In Brand Lab, enable Require License and confirm the Product ID. If Product ID is empty, PatchCraft derives one during export.\n"
            "5. Export/publish. PatchCraft embeds license.json and sends license_config to Plugin.club metadata so the same data can map to AudiLock later.\n\n"
            "Runtime contract\n"
            "- The Player stores the License URL and Product ID in the pack metadata.\n"
            "- The activation request includes productId, instrumentId, licenseKey, machineId, trial, grace days, and bind-to-machine.\n"
            "- Public Key is for client-side verification metadata. The private key stays on Plugin.club/AudiLock backend infrastructure.",
            false);

        imageProviderBox.addItem ("Built-in Placeholder Renderer", 1);
        imageProviderBox.addItem ("OpenAI Images", 2);
        imageProviderBox.onChange = [this] { refreshCloudEnabledState(); };

        textProviderBox.addItem ("Built-in Templates", 1);
        textProviderBox.addItem ("Local llama.cpp Server", 2);
        textProviderBox.addItem ("Cloud OpenAI/DeepSeek", 3);
        textProviderBox.onChange = [this] { refreshCloudEnabledState(); };

        imageApiKeyEditor.setPasswordCharacter (0x2022);
        textApiKeyEditor.setPasswordCharacter (0x2022);
        murekaApiKeyEditor.setPasswordCharacter (0x2022);
        pluginApiKeyEditor.setPasswordCharacter (0x2022);
        pluginEndpointEditor.setTextToShowWhenEmpty ("https://plugin.club/functions/sellerImport", PatchCraftLookAndFeel::textDim());
        licenseEndpointEditor.setTextToShowWhenEmpty ("https://plugin.club/functions/deviceAuth", PatchCraftLookAndFeel::textDim());
        licensePublicKeyEditor.setTextToShowWhenEmpty ("Public verification key or key id only", PatchCraftLookAndFeel::textDim());
        imageApiKeyEditor.setTooltip ("OpenAI image API key. Stored locally in PatchCraft cloud-integrations.json.");
        imageModelEditor.setTooltip ("Default image model sent to the image endpoint.");
        textApiKeyEditor.setTooltip ("API Key for OpenAI compatible text generation (e.g. DeepSeek Coder).");
        textEndpointEditor.setTooltip ("Default OpenAI-compatible text generation endpoint.");
        textModelEditor.setTooltip ("Model name for text generation (e.g. deepseek-coder).");
        murekaApiKeyEditor.setTooltip ("Reserved for Mureka audio/source/stem generation workflows.");
        pluginEndpointEditor.setTooltip ("Plugin.club-compatible publish base URL. Use https://plugin.club/functions or the full https://plugin.club/functions/sellerImport endpoint.");
        pluginApiKeyEditor.setTooltip ("Plugin.club publish API key.");
        licenseEndpointEditor.setTooltip ("HTTPS activation/validation endpoint embedded into protected packs. Use Plugin.club deviceAuth for launch; point this to AudiLock once AudiLock owns licensing.");
        licensePublicKeyEditor.setTooltip ("Public verification key or public key id embedded into protected pack metadata. Do not paste private signing keys.");
        cloudSaveButton.onClick = [this] { saveCloudSettings(); };

        audiLockStatusLabel.setText ("AudiLock AI:", juce::dontSendNotification);
        audiLockStatusMessage.setText (AudiLockSecurity::getStatusMessage(), juce::dontSendNotification);
        if (AudiLockSecurity::isAiKeyEmbedded())
            audiLockStatusMessage.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        else
            audiLockStatusMessage.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());

        for (auto* component : { static_cast<juce::Component*> (&cloudHeader),
                                 static_cast<juce::Component*> (&cloudHelp),
                                 static_cast<juce::Component*> (&cloudGuideText),
#if PATCHCRAFT_ENABLE_AI_STUDIO
                                 static_cast<juce::Component*> (&imageProviderLabel),
                                 static_cast<juce::Component*> (&imageProviderBox),
                                 static_cast<juce::Component*> (&imageApiKeyLabel),
                                 static_cast<juce::Component*> (&imageApiKeyEditor),
                                 static_cast<juce::Component*> (&imageModelLabel),
                                 static_cast<juce::Component*> (&imageModelEditor),
                                 static_cast<juce::Component*> (&textProviderLabel),
                                 static_cast<juce::Component*> (&textProviderBox),
                                 static_cast<juce::Component*> (&textEndpointLabel),
                                 static_cast<juce::Component*> (&textEndpointEditor),
                                 static_cast<juce::Component*> (&textModelLabel),
                                 static_cast<juce::Component*> (&textModelEditor),
                                 static_cast<juce::Component*> (&audiLockStatusLabel),
                                 static_cast<juce::Component*> (&audiLockStatusMessage),
                                 static_cast<juce::Component*> (&murekaApiKeyLabel),
                                 static_cast<juce::Component*> (&murekaApiKeyEditor),
#endif
                                 static_cast<juce::Component*> (&pluginEndpointLabel),
                                 static_cast<juce::Component*> (&pluginEndpointEditor),
                                 static_cast<juce::Component*> (&pluginApiKeyLabel),
                                 static_cast<juce::Component*> (&pluginApiKeyEditor),
                                 static_cast<juce::Component*> (&licenseEndpointLabel),
                                 static_cast<juce::Component*> (&licenseEndpointEditor),
                                 static_cast<juce::Component*> (&licensePublicKeyLabel),
                                 static_cast<juce::Component*> (&licensePublicKeyEditor),
                                 static_cast<juce::Component*> (&cloudSaveButton) })
            cloudTab.addAndMakeVisible (*component);

        loadCloudSettings();

        expansionsHeader.setText ("PATCHCRAFT EXTENSIONS  -  .pcexp MODULES", juce::dontSendNotification);
        expansionsHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        expansionsHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());

        expansionsHelp.setText ("Install PatchCraft application extensions such as exporters, script runtimes, synth modules, validators, and workflow tools. "
                                "pScript Core is built in; JavaScript, Lua, and other language bridges can ship as .pcexp extension modules.",
                                juce::dontSendNotification);
        expansionsHelp.setFont (juce::Font (11.0f));
        expansionsHelp.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        expansionsHelp.setJustificationType (juce::Justification::topLeft);

        expansionsList.setMultiLine (true);
        expansionsList.setReadOnly (true);
        expansionsList.setScrollbarsShown (true);
        expansionsList.setCaretVisible (false);
        expansionsList.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::bg().brighter (0.03f));
        expansionsList.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
        expansionsList.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());

        installExpansionButton.setTooltip ("Install a .pcexp archive or extension folder into the PatchCraft user extension directory.");
        refreshExpansionButton.setTooltip ("Rescan built-in, bundled, and user-installed PatchCraft extensions.");
        openExpansionFolderButton.setTooltip ("Open the user extension folder so installers or manual installs can copy .pcexp packages there.");
        installExpansionButton.onClick = [this] { installExpansionPackage(); };
        refreshExpansionButton.onClick = [this] { refreshExpansionList(); };
        openExpansionFolderButton.onClick = [this]
        {
            expansionManager.getUserRoot().createDirectory();
            expansionManager.getUserRoot().revealToUser();
        };

        for (auto* component : { static_cast<juce::Component*> (&expansionsHeader),
                                 static_cast<juce::Component*> (&expansionsHelp),
                                 static_cast<juce::Component*> (&installExpansionButton),
                                 static_cast<juce::Component*> (&refreshExpansionButton),
                                 static_cast<juce::Component*> (&openExpansionFolderButton),
                                 static_cast<juce::Component*> (&expansionsList) })
            expansionsTab.addAndMakeVisible (*component);

        refreshExpansionList();
    }

    SettingsDialogContent::~SettingsDialogContent()
    {
        // Persist on close.
        service.saveState();
    }

    void SettingsDialogContent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void SettingsDialogContent::resized()
    {
        tabs.setBounds (getLocalBounds().reduced (8));

        auto hardware = hardwareTab.getLocalBounds().reduced (12);
        header.setBounds (hardware.removeFromTop (28));
        selector.setBounds (hardware);

        auto uiBounds = uiTab.getLocalBounds().reduced (14);
        uiHeader.setBounds (uiBounds.removeFromTop (24));
        uiHelp.setBounds (uiBounds.removeFromTop (44));
        uiBounds.removeFromTop (10);
        showTooltipsToggle.setBounds (uiBounds.removeFromTop (32));
        showTutorialsToggle.setBounds (uiBounds.removeFromTop (32));
        uiBounds.removeFromTop (12);
        uiSaveButton.setBounds (uiBounds.removeFromTop (32).removeFromLeft (110));

        auto aiBounds = aiTab.getLocalBounds().reduced (14);
        aiHeader.setBounds (aiBounds.removeFromTop (24));
        aiHelp.setBounds (aiBounds.removeFromTop (44));
        aiBounds.removeFromTop (6);

        auto aiRow = aiBounds.removeFromTop (30);
        aiProviderBox.setBounds (aiRow.removeFromLeft (250));
        aiIncludeContextToggle.setBounds (aiRow.removeFromLeft (210));
        aiSaveButton.setBounds (aiRow.removeFromRight (86));
        aiResetButton.setBounds (aiRow.removeFromRight (92).reduced (4, 0));

        aiBounds.removeFromTop (10);
        auto endpointRow = aiBounds.removeFromTop (30);
        aiEndpointLabel.setBounds (endpointRow.removeFromLeft (90));
        aiEndpointEditor.setBounds (endpointRow);

        auto modelRow = aiBounds.removeFromTop (30);
        aiModelLabel.setBounds (modelRow.removeFromLeft (90));
        aiModelEditor.setBounds (modelRow);

        aiBounds.removeFromTop (10);
        auto sliderRow = aiBounds.removeFromTop (44);
        const auto third = sliderRow.getWidth() / 3;
        auto timeoutArea = sliderRow.removeFromLeft (third).reduced (0, 2);
        aiTimeoutLabel.setBounds (timeoutArea.removeFromTop (14));
        aiTimeoutSlider.setBounds (timeoutArea);
        auto tokensArea = sliderRow.removeFromLeft (third).reduced (8, 2);
        aiTokensLabel.setBounds (tokensArea.removeFromTop (14));
        aiTokensSlider.setBounds (tokensArea);
        auto tempArea = sliderRow.reduced (8, 2);
        aiTemperatureLabel.setBounds (tempArea.removeFromTop (14));
        aiTemperatureSlider.setBounds (tempArea);

        auto cloudBounds = cloudTab.getLocalBounds().reduced (14);
        cloudHeader.setBounds (cloudBounds.removeFromTop (24));
        cloudHelp.setBounds (cloudBounds.removeFromTop (36));
        auto cloudGuideBounds = cloudBounds.removeFromBottom (juce::jmin (230, juce::jmax (140, cloudBounds.getHeight() / 2)));
        cloudGuideBounds.removeFromTop (8);
        cloudGuideText.setBounds (cloudGuideBounds);
        cloudBounds.removeFromTop (8);

        auto cloudRow = cloudBounds.removeFromTop (30);
#if PATCHCRAFT_ENABLE_AI_STUDIO
        imageProviderLabel.setBounds (cloudRow.removeFromLeft (92));
        imageProviderBox.setBounds (cloudRow.removeFromLeft (260));
        cloudSaveButton.setBounds (cloudRow.removeFromRight (110));

        cloudBounds.removeFromTop (8);
        cloudRow = cloudBounds.removeFromTop (30);
        imageApiKeyLabel.setBounds (cloudRow.removeFromLeft (92));
        imageApiKeyEditor.setBounds (cloudRow.removeFromLeft (300));
        imageModelLabel.setBounds (cloudRow.removeFromLeft (96));
        imageModelEditor.setBounds (cloudRow);

        cloudRow = cloudBounds.removeFromTop (30);
        textProviderLabel.setBounds (cloudRow.removeFromLeft (92));
        textProviderBox.setBounds (cloudRow.removeFromLeft (260));
        
        cloudBounds.removeFromTop (8);
        cloudRow = cloudBounds.removeFromTop (30);
        textEndpointLabel.setBounds (cloudRow.removeFromLeft (92));
        textEndpointEditor.setBounds (cloudRow.removeFromLeft (300));
        textModelLabel.setBounds (cloudRow.removeFromLeft (96));
        textModelEditor.setBounds (cloudRow);

        cloudRow = cloudBounds.removeFromTop (30);
        audiLockStatusLabel.setBounds (cloudRow.removeFromLeft (92));
        audiLockStatusMessage.setBounds (cloudRow.removeFromLeft (300));
        
        cloudRow = cloudBounds.removeFromTop (30);
        murekaApiKeyLabel.setBounds (cloudRow.removeFromLeft (92));
        murekaApiKeyEditor.setBounds (cloudRow);

        cloudBounds.removeFromTop (8);
#else
        cloudSaveButton.setBounds (cloudRow.removeFromRight (110));
#endif
        cloudRow = cloudBounds.removeFromTop (30);
        pluginEndpointLabel.setBounds (cloudRow.removeFromLeft (92));
        pluginEndpointEditor.setBounds (cloudRow.removeFromLeft (360));
        pluginApiKeyLabel.setBounds (cloudRow.removeFromLeft (92));
        pluginApiKeyEditor.setBounds (cloudRow);

        cloudRow = cloudBounds.removeFromTop (30);
        licenseEndpointLabel.setBounds (cloudRow.removeFromLeft (92));
        licenseEndpointEditor.setBounds (cloudRow.removeFromLeft (360));
        licensePublicKeyLabel.setBounds (cloudRow.removeFromLeft (92));
        licensePublicKeyEditor.setBounds (cloudRow);

        auto expansionBounds = expansionsTab.getLocalBounds().reduced (14);
        expansionsHeader.setBounds (expansionBounds.removeFromTop (24));
        expansionsHelp.setBounds (expansionBounds.removeFromTop (52));
        expansionBounds.removeFromTop (8);
        auto expansionButtons = expansionBounds.removeFromTop (32);
        installExpansionButton.setBounds (expansionButtons.removeFromLeft (138));
        refreshExpansionButton.setBounds (expansionButtons.removeFromLeft (96).reduced (6, 0));
        openExpansionFolderButton.setBounds (expansionButtons.removeFromLeft (126).reduced (0, 0));
        expansionBounds.removeFromTop (10);
        expansionsList.setBounds (expansionBounds);
    }

    void SettingsDialogContent::loadAiSettings()
    {
        const auto config = AiAssistService::loadLocalLlmConfig();
        aiProviderBox.setSelectedId (config.provider == AiAssistService::ProviderMode::LocalLlamaServer ? 2 : 1,
                                     juce::dontSendNotification);
        aiEndpointEditor.setText (config.endpoint, false);
        aiModelEditor.setText (config.model, false);
        aiTimeoutSlider.setValue (config.timeoutMs, juce::dontSendNotification);
        aiTokensSlider.setValue (config.maxTokens, juce::dontSendNotification);
        aiTemperatureSlider.setValue (config.temperature, juce::dontSendNotification);
        aiIncludeContextToggle.setToggleState (config.includeProjectContext, juce::dontSendNotification);
        refreshAiEnabledState();
    }

    void SettingsDialogContent::saveAiSettings()
    {
        AiAssistService::LocalLlmConfig config;
        config.provider = aiProviderBox.getSelectedId() == 2
            ? AiAssistService::ProviderMode::LocalLlamaServer
            : AiAssistService::ProviderMode::BuiltInTemplates;
        config.endpoint = aiEndpointEditor.getText().trim();
        config.model = aiModelEditor.getText().trim();
        config.timeoutMs = juce::roundToInt (aiTimeoutSlider.getValue());
        config.maxTokens = juce::roundToInt (aiTokensSlider.getValue());
        config.temperature = (float) aiTemperatureSlider.getValue();
        config.includeProjectContext = aiIncludeContextToggle.getToggleState();
        AiAssistService::saveLocalLlmConfig (config);
        refreshAiEnabledState();
    }

    void SettingsDialogContent::refreshAiEnabledState()
    {
        const bool enabled = aiProviderBox.getSelectedId() == 2;
        for (auto* component : { static_cast<juce::Component*> (&aiEndpointLabel),
                                 static_cast<juce::Component*> (&aiEndpointEditor),
                                 static_cast<juce::Component*> (&aiModelLabel),
                                 static_cast<juce::Component*> (&aiModelEditor),
                                 static_cast<juce::Component*> (&aiTimeoutLabel),
                                 static_cast<juce::Component*> (&aiTimeoutSlider),
                                 static_cast<juce::Component*> (&aiTokensLabel),
                                 static_cast<juce::Component*> (&aiTokensSlider),
                                 static_cast<juce::Component*> (&aiTemperatureLabel),
                                 static_cast<juce::Component*> (&aiTemperatureSlider),
                                 static_cast<juce::Component*> (&aiIncludeContextToggle) })
            component->setEnabled (enabled);
    }

    void SettingsDialogContent::resetAiDefaults()
    {
        AiAssistService::saveLocalLlmConfig ({});
        loadAiSettings();
    }

    void SettingsDialogContent::loadCloudSettings()
    {
        const auto config = AiAssistService::loadCloudIntegrationConfig();
        imageProviderBox.setSelectedId (config.imageProvider == AiAssistService::ImageProviderMode::OpenAIImages ? 2 : 1,
                                        juce::dontSendNotification);
        imageApiKeyEditor.setText (config.imageApiKey, false);
        imageModelEditor.setText (config.imageModel, false);
        
        textProviderBox.setSelectedId (
            config.textProvider == AiAssistService::TextProviderMode::CloudOpenAICompatible ? 3 :
            (config.textProvider == AiAssistService::TextProviderMode::LocalLlamaServer ? 2 : 1),
            juce::dontSendNotification);
        textEndpointEditor.setText (config.textEndpoint, false);
        textModelEditor.setText (config.textModel, false);
        
        murekaApiKeyEditor.setText (config.murekaApiKey, false);
        pluginEndpointEditor.setText (config.pluginClubEndpoint, false);
        pluginApiKeyEditor.setText (config.pluginClubApiKey, false);
        licenseEndpointEditor.setText (config.licenseEndpoint, false);
        licensePublicKeyEditor.setText (config.licensePublicKey, false);
        refreshCloudEnabledState();
    }

    void SettingsDialogContent::saveCloudSettings()
    {
        auto config = AiAssistService::loadCloudIntegrationConfig();
#if PATCHCRAFT_ENABLE_AI_STUDIO
        config.imageProvider = imageProviderBox.getSelectedId() == 2
            ? AiAssistService::ImageProviderMode::OpenAIImages
            : AiAssistService::ImageProviderMode::BuiltInRenderer;
        config.imageApiKey = imageApiKeyEditor.getText().trim();
        config.imageModel = imageModelEditor.getText().trim();
        
        config.textProvider = textProviderBox.getSelectedId() == 3 
            ? AiAssistService::TextProviderMode::CloudOpenAICompatible 
            : (textProviderBox.getSelectedId() == 2 ? AiAssistService::TextProviderMode::LocalLlamaServer : AiAssistService::TextProviderMode::BuiltInTemplates);
        config.textEndpoint = textEndpointEditor.getText().trim();
        config.textModel = textModelEditor.getText().trim();
        
        config.murekaApiKey = murekaApiKeyEditor.getText().trim();
#endif
        config.pluginClubEndpoint = pluginEndpointEditor.getText().trim();
        config.pluginClubApiKey = pluginApiKeyEditor.getText().trim();
        config.licenseEndpoint = licenseEndpointEditor.getText().trim();
        config.licensePublicKey = licensePublicKeyEditor.getText().trim();
        AiAssistService::saveCloudIntegrationConfig (config);
        refreshCloudEnabledState();
    }

    void SettingsDialogContent::refreshCloudEnabledState()
    {
#if PATCHCRAFT_ENABLE_AI_STUDIO
        const bool imageEnabled = imageProviderBox.getSelectedId() == 2;
        imageApiKeyLabel.setEnabled (imageEnabled);
        imageApiKeyEditor.setEnabled (imageEnabled);
        imageModelLabel.setEnabled (imageEnabled);
        imageModelEditor.setEnabled (imageEnabled);
        
        const bool textEnabled = textProviderBox.getSelectedId() != 1;
        textEndpointLabel.setEnabled (textEnabled);
        textEndpointEditor.setEnabled (textEnabled);
        textModelLabel.setEnabled (textEnabled);
        textModelEditor.setEnabled (textEnabled);
#endif
    }

    void SettingsDialogContent::loadUiSettings()
    {
        const auto& manifest = project.getManifest();
        showTooltipsToggle.setToggleState (manifest.playerShowParameterGuidance, juce::dontSendNotification);
        showTutorialsToggle.setToggleState (manifest.studioShowTutorials, juce::dontSendNotification);
    }

    void SettingsDialogContent::saveUiSettings()
    {
        auto& manifest = project.getManifest();
        manifest.playerShowParameterGuidance = showTooltipsToggle.getToggleState();
        manifest.studioShowTutorials = showTutorialsToggle.getToggleState();
        project.notifyChanged();
    }

    void SettingsDialogContent::refreshExpansionList()
    {
        const auto& installed = expansionManager.scanInstalled();
        juce::String text;
        text << "User extension folder:\n" << expansionManager.getUserRoot().getFullPathName() << "\n\n";
        text << "Bundled extension folder:\n" << expansionManager.getBundledRoot().getFullPathName() << "\n\n";

        for (const auto& expansion : installed)
        {
            text << expansion.manifest.name << "  v" << expansion.manifest.version << "\n";
            text << "  ID: " << expansion.manifest.id << "\n";
            text << "  Kind: " << expansion.manifest.kind << "\n";
            text << "  Status: " << expansion.statusText();
            if (expansion.bundled && ! expansion.manifest.builtIn)
                text << " (bundled)";
            if (expansion.manifest.builtIn)
                text << " (built in)";
            text << "\n";
            text << "  License: " << PcexpManager::licenseModeToString (expansion.manifest.licenseMode) << "\n";
            text << "  Capabilities: " << expansion.manifest.capabilities.joinIntoString (", ") << "\n";
            if (! expansion.manifest.dependencies.isEmpty())
                text << "  Dependencies: " << expansion.manifest.dependencies.joinIntoString (", ") << "\n";
            if (expansion.installRoot.exists())
                text << "  Path: " << expansion.installRoot.getFullPathName() << "\n";
            else if (expansion.packageFile.existsAsFile())
                text << "  Package: " << expansion.packageFile.getFullPathName() << "\n";
            text << "\n";
        }

        expansionsList.setText (text, false);
    }

    void SettingsDialogContent::installExpansionPackage()
    {
        expansionChooser = std::make_unique<juce::FileChooser> (
            "Install PatchCraft Extension",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.pcexp");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::canSelectDirectories;

        expansionChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
        {
            const auto package = chooser.getResult();
            if (! package.exists())
                return;

            const auto result = expansionManager.installPackage (package, true);
            refreshExpansionList();

            juce::AlertWindow::showMessageBoxAsync (
                result.wasOk() ? juce::MessageBoxIconType::InfoIcon
                               : juce::MessageBoxIconType::WarningIcon,
                result.wasOk() ? "PatchCraft Extension Installed"
                               : "PatchCraft Extension Failed",
                result.wasOk() ? "Installed " + package.getFileName()
                               : result.getErrorMessage());
        });
    }

    // -------------------------------------------------------------------------
    SettingsWindow::SettingsWindow (StudioAudioService& svc,
                                    AiAssistService& aiService,
                                    PatchCraftProject& projectToEdit,
                                    std::function<void()> onClose)
        : juce::DocumentWindow ("Settings",
                                PatchCraftLookAndFeel::bg(),
                                juce::DocumentWindow::closeButton),
          onCloseFn (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        content = std::make_unique<SettingsDialogContent> (svc, aiService, projectToEdit);
        setContentNonOwned (content.get(), false);
        setResizable (true, false);
        setResizeLimits (720, 520, 1400, 1100);
        centreWithSize (920, 680);
        setVisible (true);
    }

    SettingsWindow::~SettingsWindow() = default;

    void SettingsWindow::closeButtonPressed()
    {
        if (onCloseFn) onCloseFn();
    }

} // namespace patchcraft
