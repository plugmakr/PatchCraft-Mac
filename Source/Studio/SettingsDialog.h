#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace patchcraft
{
    class AiAssistService;
    class StudioAudioService;

    /**
        Settings window: hardware (audio + MIDI device picker) and a few app
        preferences. Held inside a DocumentWindow created on demand by the
        toolbar Settings button.
    */
    class SettingsDialogContent : public juce::Component
    {
    public:
        SettingsDialogContent (StudioAudioService& svc, AiAssistService& aiService);
        ~SettingsDialogContent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void loadAiSettings();
        void saveAiSettings();
        void refreshAiEnabledState();
        void resetAiDefaults();

        StudioAudioService& service;
        AiAssistService& ai;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label header;
        juce::Label aiHeader;
        juce::Label aiHelp;
        juce::ComboBox aiProviderBox;
        juce::Label aiEndpointLabel;
        juce::TextEditor aiEndpointEditor;
        juce::Label aiModelLabel;
        juce::TextEditor aiModelEditor;
        juce::Label aiTimeoutLabel;
        juce::Slider aiTimeoutSlider;
        juce::Label aiTokensLabel;
        juce::Slider aiTokensSlider;
        juce::Label aiTemperatureLabel;
        juce::Slider aiTemperatureSlider;
        juce::ToggleButton aiIncludeContextToggle { "Include project context" };
        juce::TextButton aiSaveButton { "Save AI" };
        juce::TextButton aiResetButton { "Reset AI" };
    };

    class SettingsWindow : public juce::DocumentWindow
    {
    public:
        SettingsWindow (StudioAudioService& svc, AiAssistService& aiService, std::function<void()> onClose);
        ~SettingsWindow() override;
        void closeButtonPressed() override;

    private:
        std::function<void()> onCloseFn;
        std::unique_ptr<SettingsDialogContent> content;
    };

} // namespace patchcraft
