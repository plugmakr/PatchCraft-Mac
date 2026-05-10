#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Detailed table view of all sample zones (Sample / Root / Low / High /
        Low Vel / High Vel / Gain / Pan / Loop / Start / End).
    */
    class KeyzonesComponent : public juce::Component,
                              private juce::TableListBoxModel
    {
    public:
        explicit KeyzonesComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        StudioMainComponent& owner;
        juce::TableListBox table { "keyzones", this };

        int  getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
        void paintCell (juce::Graphics&, int row, int colId, int w, int h, bool selected) override;
        juce::Component* refreshComponentForCell (int row, int colId, bool selected,
                                                  juce::Component* existingComponentToUpdate) override;

        juce::String textForCell (const SampleZoneDef& zone, int colId) const;
        bool isEditableColumn (int colId) const;
        void commitCellEdit (int row, int colId, const juce::String& text);
    };

} // namespace patchcraft
