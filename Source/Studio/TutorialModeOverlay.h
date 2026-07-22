#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace patchcraft
{
    class StudioMainComponent;

    /** Floating tutorial callout shown when Tutorial Mode is enabled. */
    class TutorialModeOverlay final : public juce::Component,
                                      private juce::Timer
    {
    public:
        explicit TutorialModeOverlay (StudioMainComponent& owner);

        void setActive (bool shouldBeActive);
        bool isActive() const noexcept { return active; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void hideCallout();
        void showCallout (const juce::String& title, const juce::String& body, juce::Point<int> screenAnchor);
        void applyInspectorLabelHelp();

        StudioMainComponent& studio;
        bool active = false;
        bool calloutVisible = false;
        juce::String calloutTitle;
        juce::String calloutBody;
        juce::Rectangle<int> calloutBounds;
        const juce::Component* hoverComponent = nullptr;
        juce::uint32 hoverStartMs = 0;
        juce::String lastHelpSignature;
    };
}
