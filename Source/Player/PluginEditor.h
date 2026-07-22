#pragma once

#include "PluginProcessor.h"
#include "PlayerGuiRenderer.h"
#include "PlayerLookAndFeel.h"
#include "AssetManager.h"
#include "LibraryBrowser.h"

#include <set>

namespace patchcraft
{
    class PlayerTopBar;
    class PlayerLeftSidebar;
    class PlayerCenterPanel;
    class PlayerRightPanel;
    class PlayerFooter;
    class PlayerKeyboardStrip;
    class PlayerPerformancePanel;
    class PlayerControlCenter;
    class PlayerUserImportPanel;

    /**
        Player editor. Shows a "Load Instrument" splash
        when no pack is loaded; otherwise renders the loaded pack's UI.
    */
    class PlayerEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         public PlayerProcessor::EditorListener
    {
    public:
        explicit PlayerEditor (PlayerProcessor&);
        ~PlayerEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void packChanged() override;

        juce::Colour getBgColor() const;
        juce::Colour getPanelColor() const;
        juce::Colour getAccentColor() const;
        juce::Colour getTextColor() const;
        juce::Colour getTextDimColor() const;
        juce::Colour getBorderColor() const;

        // FileDragAndDropTarget
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragMove (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray&, int, int) override;

    private:
        friend class PlayerTopBar;
        friend class PlayerLeftSidebar;
        friend class PlayerCenterPanel;
        friend class PlayerRightPanel;
        friend class PlayerFooter;
        friend class PlayerKeyboardStrip;

        PlayerProcessor& proc;
        PlayerLookAndFeel laf;
        AssetManager      assets;

        std::unique_ptr<PlayerTopBar> topBar;
        std::unique_ptr<PlayerLeftSidebar> leftSidebar;
        std::unique_ptr<PlayerCenterPanel> centerPanel;
        std::unique_ptr<PlayerRightPanel> rightPanel;
        std::unique_ptr<PlayerFooter> footer;
        std::unique_ptr<PlayerKeyboardStrip> keyboardStrip;

        std::unique_ptr<PlayerGuiRenderer> renderer;
        std::unique_ptr<LibraryBrowser> libraryBrowser;
        std::unique_ptr<PlayerPerformancePanel> performancePanel;
        std::unique_ptr<PlayerControlCenter> controlCenter;
        std::unique_ptr<PlayerUserImportPanel> userImportPanel;

        juce::TextButton loadBtn { "Load Instrument" };
        juce::TextButton libraryBtn { "Library" };
        juce::TextButton performanceBtn { "Sound" };
        juce::TextButton prevPresetBtn { "<" };
        juce::TextButton presetBtn { "Preset" };
        juce::TextButton nextPresetBtn { ">" };
        juce::TextButton controlBtn { "Control" };
        juce::Label presetLoadingLabel;
        juce::TooltipWindow tooltipWindow { this, 650 };

        bool libraryVisible = false;
        bool performanceVisible = false;
        bool controlCenterVisible = false;
        bool userImportVisible = false;
        bool performanceFloating = false;
        std::set<std::string> favoritePresetNames;
        bool presetAuditionOnSelect = true;
        bool presetCloseAfterLoad = false;

        void showLoadDialog();
        void toggleLibrary();
        void togglePerformancePanel();
        void toggleControlCenter();
        void toggleUserImportPanel();
        void showRackPanel();
        void showSnapshotsPanel();
        void showSoundDnaPanel();
        void showPresetMenu();
        void showPresetLoading (const juce::String& presetName);
        void showAboutDialog();
        void showLicenseActivationDialog();
        void refreshPresetControls();
        void refreshTooltipWindowState();
        void resizeToCurrentPackCanvas();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
    };

} // namespace patchcraft
