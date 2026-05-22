#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    class StudioApplication : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override        { return "PatchCraft Studio"; }
        const juce::String getApplicationVersion() override     { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override              { return true; }

        void initialise (const juce::String&) override;
        void shutdown() override;
        void systemRequestedQuit() override                     { quit(); }
        void anotherInstanceStarted (const juce::String&) override {}

    private:
        class StudioWindow : public juce::DocumentWindow
        {
        public:
            StudioWindow (juce::String name, PatchCraftLookAndFeel& laf);

            bool keyPressed (const juce::KeyPress& key) override;

            void closeButtonPressed() override
            {
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }
        };

        PatchCraftLookAndFeel laf;
        std::unique_ptr<StudioWindow> mainWindow;
    };

} // namespace patchcraft
