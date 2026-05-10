#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Library-style browser for expansion packs in the current project.
        Replaces the old expansion-pack dropdown UX with a grid of cards
        (thumbnail + name + creator + counts). The selected card drives the
        rest of the app's expansion-aware operations (Save Patch, Send To
        Expansion, etc.) by writing to the project.
    */
    class ExpansionLibraryPanel : public juce::Component
    {
    public:
        explicit ExpansionLibraryPanel (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void refresh();

        const juce::String& getSelectedExpansionId() const noexcept { return selectedExpansionId; }

    private:
        struct CardLayout
        {
            juce::Rectangle<int> bounds;
            int expansionIndex = -1;
        };

        void rebuildCards();
        std::vector<CardLayout> computeCardLayouts() const;
        void paintCard (juce::Graphics&, const CardLayout&);
        int hitCard (juce::Point<int>) const;
        void onCardClicked (int expansionIndex, bool doubleClick);

        StudioMainComponent& owner;
        juce::TextButton newExpansionButton { "+ New" };
        juce::TextButton refreshButton      { "Refresh" };
        juce::TextButton popOutBtn          { "\xe2\xa4\xa2" };
        juce::Label header;
        juce::String selectedExpansionId;
        std::vector<CardLayout> cards;
        juce::Rectangle<int> contentArea;
    };
}
