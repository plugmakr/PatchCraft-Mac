#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class ProjectBrowserPage final : public juce::Component,
                                     private juce::ListBoxModel
    {
    public:
        explicit ProjectBrowserPage (StudioMainComponent& owner);

        void refresh();
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Entry
        {
            juce::File folder;
            juce::String title;
            juce::String type;
            juce::String engine;
            juce::String location;
            juce::String modified;
            bool recent = false;
        };

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;

        void openSelected();
        void browseForProject();
        void addProjectFolder (juce::File folder, bool recent);
        void scanRoot (const juce::File& root, int depth);

        StudioMainComponent& owner;
        juce::Label title;
        juce::Label subtitle;
        juce::TextButton browseButton { "Browse..." };
        juce::TextButton openButton { "Open Selected" };
        juce::TextButton refreshButton { "Refresh" };
        juce::ListBox list { "Project Browser", this };
        std::vector<Entry> entries;
        juce::StringArray seenPaths;
        std::shared_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectBrowserPage)
    };
}
