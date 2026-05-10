#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Compact parameter list shown in the bottom strip - mirrors the
        reference image (8 visible rows showing id + current value).
    */
    class ParametersComponent : public juce::Component,
                                private juce::ListBoxModel
    {
    public:
        explicit ParametersComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        StudioMainComponent& owner;

        juce::ListBox list { "params", this };
        juce::TextButton addBtn { "+" };
        juce::TextButton menuBtn { "" };

        int  getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

        static juce::String formatValue (const struct ParameterDef& p);
    };

} // namespace patchcraft
