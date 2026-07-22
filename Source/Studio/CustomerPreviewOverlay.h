#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace patchcraft
{
    class StudioMainComponent;

    /** Full-screen customer preview — Player chrome + layout, floating exit control. */
    class CustomerPreviewOverlay : public juce::Component
    {
    public:
        explicit CustomerPreviewOverlay (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        void enterPreview();
        void exitPreview();
        bool isActive() const noexcept { return active; }

        std::function<void()> onExit;

    private:
        StudioMainComponent& owner;
        juce::TextButton exitButton { "Back to Studio" };
        bool active = false;
    };

} // namespace patchcraft
