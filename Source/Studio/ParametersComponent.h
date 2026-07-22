#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /** Compact control-binding strip on the Layout page. Full DSP editing lives on the Graph tab. */
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
        juce::Label hintLabel;
    };
}
