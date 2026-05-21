#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Left sidebar 'Layers' tab. List of layout elements with hide/lock/delete.
    */
    class LayersPanel : public juce::Component,
                        public juce::DragAndDropTarget,
                        private juce::ListBoxModel
    {
    public:
        explicit LayersPanel (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();

    private:
        struct RowInfo
        {
            int layoutIndex = -1;
            int depth = 0;
            bool isGroup = false;
        };

        // ListBoxModel
        int  getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;
        void backgroundClicked (const juce::MouseEvent&) override;
        void deleteKeyPressed (int lastRowSelected) override;

        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

        StudioMainComponent& owner;
        std::vector<RowInfo> rows;
        juce::StringArray collapsedGroups;
        juce::ListBox listBox { "layers", this };
        juce::TextButton addGroupButton { "+ Group" };
        juce::TextButton groupSelectedButton { "Group Sel" };
        juce::TextButton ungroupButton { "Ungroup" };
        juce::TextButton popOutBtn      { "Pop" };
        juce::TextEditor searchBox;
        juce::TextButton clearSearchButton { "Clear" };
        int lastClickedRow = -1;

        void rebuildRows();
        bool rowMatchesSearch (const LayoutElement&) const;
        void renameRow (int row);
        void createGroupFromSelection();
        void showGroupNameModal (const juce::String& groupId);
        void ungroupSelection();
        int rowToLayoutIndex (int row) const;
    };

} // namespace patchcraft
