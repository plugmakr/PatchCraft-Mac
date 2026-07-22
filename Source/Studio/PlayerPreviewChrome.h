#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/AssetManager.h"

#include <functional>

namespace patchcraft
{
    class PlayerProcessor;
    class PatchCraftProject;

    /** Studio preview of the Player top chrome (title + preset toolbar). */
    class PlayerPreviewChrome : public juce::Component,
                                 private juce::Timer
    {
    public:
        static constexpr int kTotalHeight = 108;
        static constexpr int kTitleHeight = 66;

        PlayerPreviewChrome (PatchCraftProject& project, PlayerProcessor& processor);
        ~PlayerPreviewChrome() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();
        void setStudioExitHandler (std::function<void()> handler);

    private:
        void timerCallback() override;
        void refreshPresetLabel();
        void showPresetMenu();

        PatchCraftProject& project;
        PlayerProcessor& processor;
        AssetManager assets;

        juce::TextButton libraryBtn { "Library" };
        juce::TextButton soundBtn { "Sound" };
        juce::TextButton prevPresetBtn { "<" };
        juce::TextButton presetBtn { "Preset" };
        juce::TextButton nextPresetBtn { ">" };
        juce::TextButton playBtn { "Play" };
        juce::TextButton studioBackBtn { "Back to Studio" };

        std::function<void()> onStudioExit;
    };

} // namespace patchcraft
