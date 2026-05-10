#pragma once

#include "PluginProcessor.h"
#include "PlayerGuiRenderer.h"
#include "PlayerLookAndFeel.h"
#include "AssetManager.h"
#include "LibraryBrowser.h"

namespace patchcraft
{
    /**
        PatchCraft Player editor. Shows a "Load PatchCraft Instrument" splash
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
        void filesDropped (const juce::StringArray&, int, int) override;

    private:
        PlayerProcessor& proc;
        PlayerLookAndFeel laf;
        AssetManager      assets;

        std::unique_ptr<PlayerGuiRenderer> renderer;
        std::unique_ptr<LibraryBrowser> libraryBrowser;

        juce::TextButton loadBtn { "Load PatchCraft Instrument" };
        juce::TextButton menuBtn { "Pack" };
        juce::TextButton randomizeBtn { "Randomize" };
        juce::TextButton abBtn { "A / B" };

        bool libraryVisible = false;

        void showLoadDialog();
        void showPackMenu();
        void toggleLibrary();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
    };

} // namespace patchcraft
