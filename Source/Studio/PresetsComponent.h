#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Compact preset browser column - matches the reference image:
        search bar, numbered list, default star, and New / Save / Save As.
    */
    class PresetsComponent : public juce::Component,
                             private juce::ListBoxModel
    {
    public:
        explicit PresetsComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void refresh();

    private:
        StudioMainComponent& owner;
        int selectedPreset = 0;
        juce::String filterText;
        juce::String activeCategory;
        std::vector<int> visibleRowIndices;
        std::vector<juce::String> categoryChipOrder;
        std::vector<juce::Rectangle<int>> categoryChipBounds;

        juce::TextEditor search;
        juce::ListBox    list { "presets", this };
        juce::TextButton newBtn   { "New" };
        juce::TextButton saveBtn  { "Save" };
        juce::TextButton saveAsBtn{ "Save As..." };
        juce::TextButton generateBtn { "Generate" };
        juce::TextButton popOutBtn   { "\xe2\xa4\xa2" };

        int  getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        Preset makePresetFromCurrentValues (const juce::String& name) const;
        void upsertPatchForPreset (const Preset& preset);
        void promptAndSaveCurrentPreset (bool duplicate);
        void promptAndGeneratePresets();
        void rebuildVisibleRows();
        juce::String categoryFor (const Preset&) const;
        juce::Colour colourForCategory (const juce::String&) const;
    };

} // namespace patchcraft
