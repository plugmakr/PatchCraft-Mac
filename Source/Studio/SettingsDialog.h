#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PcexpManager.h"
#include "PatchCraftProject.h"

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
        SettingsDialogContent (StudioAudioService& svc, AiAssistService& aiService, PatchCraftProject& projectToEdit);
        ~SettingsDialogContent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void loadAiSettings();
        void saveAiSettings();
        void refreshAiEnabledState();
        void resetAiDefaults();
        void loadCloudSettings();
        void saveCloudSettings();
        void refreshCloudEnabledState();
        void refreshPluginClubAuthStatus();
        void signInToPluginClub();
        void signOutOfPluginClub();
        void refreshExpansionList();
        void installExpansionPackage();
        void loadUiSettings();
        void saveUiSettings();

        StudioAudioService& service;
        AiAssistService& ai;
        PatchCraftProject& project;
        PcexpManager expansionManager;
        juce::Component hardwareTab;
        juce::Component uiTab;
        juce::Component aiTab;
        juce::Component cloudTab;
        juce::Component expansionsTab;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        juce::AudioDeviceSelectorComponent selector;
        juce::Label header;
        juce::Label uiHeader;
        juce::Label uiHelp;
        juce::ToggleButton showTooltipsToggle { "Show mouseover help and guidance" };
        juce::ToggleButton showTutorialsToggle { "Auto-show guided tutorials" };
        juce::TextButton uiSaveButton { "Save UI" };
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

        juce::Label cloudHeader;
        juce::Label cloudHelp;
        juce::TextEditor cloudGuideText;
        juce::ComboBox imageProviderBox;
        juce::Label imageProviderLabel;
        juce::Label imageApiKeyLabel;
        juce::TextEditor imageApiKeyEditor;
        juce::Label imageModelLabel;
        juce::TextEditor imageModelEditor;
        
        juce::ComboBox textProviderBox;
        juce::Label textProviderLabel;
        juce::Label textEndpointLabel;
        juce::TextEditor textEndpointEditor;
        juce::Label textModelLabel;
        juce::TextEditor textModelEditor;
        juce::Label textApiKeyLabel;
        juce::TextEditor textApiKeyEditor;
        
        juce::Label audiLockStatusLabel;
        juce::Label audiLockStatusMessage;
        
        juce::Label murekaApiKeyLabel;
        juce::TextEditor murekaApiKeyEditor;
        juce::Label pluginEndpointLabel;
        juce::TextEditor pluginEndpointEditor;
        juce::Label pluginApiKeyLabel;
        juce::TextEditor pluginApiKeyEditor;
        juce::Label pluginAuthStatusLabel;
        juce::TextButton pluginSignInButton { "Sign in to Plugin.club" };
        juce::TextButton pluginSignOutButton { "Sign out" };
        juce::Label licenseEndpointLabel;
        juce::TextEditor licenseEndpointEditor;
        juce::Label licensePublicKeyLabel;
        juce::TextEditor licensePublicKeyEditor;
        juce::TextButton cloudSaveButton { "Save Cloud" };
        std::atomic<bool> pluginClubAuthBusy { false };

        juce::Label expansionsHeader;
        juce::Label expansionsHelp;
        juce::TextEditor expansionsList;
        juce::TextButton installExpansionButton { "Install .pcexp" };
        juce::TextButton refreshExpansionButton { "Refresh" };
        juce::TextButton openExpansionFolderButton { "Open Folder" };
        std::unique_ptr<juce::FileChooser> expansionChooser;
    };

    class SettingsWindow : public juce::DocumentWindow
    {
    public:
        SettingsWindow (StudioAudioService& svc, AiAssistService& aiService, PatchCraftProject& projectToEdit, std::function<void()> onClose);
        ~SettingsWindow() override;
        void closeButtonPressed() override;

    private:
        std::function<void()> onCloseFn;
        std::unique_ptr<SettingsDialogContent> content;
    };

} // namespace patchcraft
