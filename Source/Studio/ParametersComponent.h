#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /** Compact sound-path overview. Detailed editing lives in the control node editor. */
    class ParametersComponent : public juce::Component
    {
    public:
        explicit ParametersComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        StudioMainComponent& owner;
        juce::TextButton openEditor { "OPEN NODE EDITOR" };
        juce::TextButton addControl { "ADD KNOB TO CANVAS" };
        juce::Label selectionStatus;
        juce::Rectangle<int> stageArea;
    };
}
