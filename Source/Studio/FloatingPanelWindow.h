#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    /**
        A free-floating, resizable, draggable container for a docked panel.
        The panel keeps its original parent intact — when the window closes,
        the host can restore the panel to its docked location.

        Usage from the host:
        @code
            // Pop out:
            floatingWindow = std::make_unique<FloatingPanelWindow> (
                "Presets", &myPanel, [this] { onPanelClosed(); });
            // Restore on close: the callback fires, host hides the floating
            // window and re-shows the docked placeholder.
        @endcode
    */
    class FloatingPanelWindow : public juce::DocumentWindow
    {
    public:
        FloatingPanelWindow (const juce::String& title,
                              juce::Component* contents,
                              std::function<void()> onClose,
                              juce::Colour backgroundColour = juce::Colour (0xff15171b))
            : juce::DocumentWindow (title, backgroundColour,
                                     juce::DocumentWindow::allButtons),
              onCloseCallback (std::move (onClose))
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setContentNonOwned (contents, true);
            setSize (juce::jmax (320, contents != nullptr ? contents->getWidth() : 480),
                     juce::jmax (240, contents != nullptr ? contents->getHeight() : 360));
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (onCloseCallback) onCloseCallback();
        }

    private:
        std::function<void()> onCloseCallback;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingPanelWindow)
    };
}
