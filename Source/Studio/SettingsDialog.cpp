#include "SettingsDialog.h"
#include "StudioAudioService.h"
#include "AiAssistService.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    SettingsDialogContent::SettingsDialogContent (StudioAudioService& svc, AiAssistService& aiService)
        : service (svc),
          ai (aiService),
          selector (svc.getDeviceManager(),
                    /*minInputChannels*/  0, /*maxInputChannels*/  8,
                    /*minOutputChannels*/ 1, /*maxOutputChannels*/ 8,
                    /*showMidiInputOptions*/    true,
                    /*showMidiOutputSelector*/  false,
                    /*showChannelsAsStereoPairs*/ true,
                    /*hideAdvancedOptionsWithButton*/ false)
    {
        header.setText ("HARDWARE  -  AUDIO + MIDI", juce::dontSendNotification);
        header.setFont (juce::Font (12.0f, juce::Font::bold));
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (header);
        addAndMakeVisible (selector);

        aiHeader.setText ("LOCAL AI COPILOT", juce::dontSendNotification);
        aiHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        aiHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (aiHeader);

        aiHelp.setText ("Use built-in templates by default, or connect to a local llama.cpp OpenAI-compatible server. "
                        "Recommended premium-light setup: Qwen3 4B GGUF via llama-server.",
                        juce::dontSendNotification);
        aiHelp.setFont (juce::Font (11.0f));
        aiHelp.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        aiHelp.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (aiHelp);

        aiProviderBox.addItem ("Built-in Templates (no model)", 1);
        aiProviderBox.addItem ("Local llama.cpp Server", 2);
        aiProviderBox.onChange = [this] { refreshAiEnabledState(); };
        addAndMakeVisible (aiProviderBox);

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
            addAndMakeVisible (*component);

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
        auto r = getLocalBounds().reduced (12);
        header.setBounds (r.removeFromTop (28));
        selector.setBounds (r.removeFromTop (juce::jmin (360, juce::jmax (220, r.getHeight() - 245))));
        r.removeFromTop (12);

        aiHeader.setBounds (r.removeFromTop (22));
        aiHelp.setBounds (r.removeFromTop (38));
        auto row = r.removeFromTop (28);
        aiProviderBox.setBounds (row.removeFromLeft (240));
        aiIncludeContextToggle.setBounds (row.removeFromLeft (190));
        aiSaveButton.setBounds (row.removeFromRight (78));
        aiResetButton.setBounds (row.removeFromRight (82).reduced (4, 0));

        r.removeFromTop (6);
        auto endpointRow = r.removeFromTop (26);
        aiEndpointLabel.setBounds (endpointRow.removeFromLeft (82));
        aiEndpointEditor.setBounds (endpointRow);

        auto modelRow = r.removeFromTop (26);
        aiModelLabel.setBounds (modelRow.removeFromLeft (82));
        aiModelEditor.setBounds (modelRow);

        auto sliderRow = r.removeFromTop (34);
        auto third = sliderRow.getWidth() / 3;
        auto timeoutArea = sliderRow.removeFromLeft (third).reduced (0, 2);
        aiTimeoutLabel.setBounds (timeoutArea.removeFromTop (12));
        aiTimeoutSlider.setBounds (timeoutArea);
        auto tokensArea = sliderRow.removeFromLeft (third).reduced (8, 2);
        aiTokensLabel.setBounds (tokensArea.removeFromTop (12));
        aiTokensSlider.setBounds (tokensArea);
        auto tempArea = sliderRow.reduced (8, 2);
        aiTemperatureLabel.setBounds (tempArea.removeFromTop (12));
        aiTemperatureSlider.setBounds (tempArea);
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

    // -------------------------------------------------------------------------
    SettingsWindow::SettingsWindow (StudioAudioService& svc,
                                    AiAssistService& aiService,
                                    std::function<void()> onClose)
        : juce::DocumentWindow ("Settings",
                                PatchCraftLookAndFeel::bg(),
                                juce::DocumentWindow::closeButton),
          onCloseFn (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        content = std::make_unique<SettingsDialogContent> (svc, aiService);
        setContentNonOwned (content.get(), false);
        setResizable (true, false);
        setResizeLimits (620, 560, 1200, 1000);
        centreWithSize (760, 720);
        setVisible (true);
    }

    SettingsWindow::~SettingsWindow() = default;

    void SettingsWindow::closeButtonPressed()
    {
        if (onCloseFn) onCloseFn();
    }

} // namespace patchcraft
