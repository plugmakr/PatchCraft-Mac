#pragma once

#include "PluginProcessor.h"
#include "PlayerGuiRenderer.h"
#include "PlayerLookAndFeel.h"
#include "AssetManager.h"
#include "LibraryBrowser.h"

#include <set>

namespace patchcraft
{
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

        // FileDragAndDropTarget
        bool isInterestedInFileDrag (const juce::StringArray&) override;
        void fileDragEnter (const juce::StringArray&, int, int) override;
        void fileDragMove (const juce::StringArray&, int, int) override;
        void fileDragExit (const juce::StringArray&) override;
        void filesDropped (const juce::StringArray&, int, int) override;

    private:
        PlayerProcessor& proc;
        PlayerLookAndFeel laf;
        AssetManager      assets;

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
        void refreshPresetControls();
        void refreshTooltipWindowState();
        void resizeToCurrentPackCanvas();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
    };

} // namespace patchcraft
