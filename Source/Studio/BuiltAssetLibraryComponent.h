#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class BuiltAssetLibraryComponent : public juce::Component,
                                       private juce::ListBoxModel
    {
    public:
        explicit BuiltAssetLibraryComponent (StudioMainComponent& owner);

        static juce::File getAssetLibraryRoot();
        static juce::File getCategoryFolder (const juce::String& category);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        struct Entry
        {
            juce::String category;
            juce::File file;
            int frames = 1;
            bool vertical = true;
        };

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRowSelected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        void addSelectedToCanvas();
        static Entry inspectAssetFile (juce::File file, juce::String category);

        StudioMainComponent& owner;
        std::vector<Entry> entries;
        juce::ListBox list { "builtAssetLibrary", this };
        juce::Label title;
        juce::TextButton refreshButton { "Refresh" };
        juce::TextButton addButton { "Add" };
        int selectedRow = -1;
    };
}
