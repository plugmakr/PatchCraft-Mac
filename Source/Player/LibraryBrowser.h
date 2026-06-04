#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "LibraryScanner.h"

namespace patchcraft
{
    /**
        Grid item component for displaying a single library entry.
        Shows thumbnail, title, creator, and description.
    */
    class LibraryItemComponent : public juce::Component
    {
    public:
        LibraryItemComponent (const LibraryEntry& entry);
        ~LibraryItemComponent() override;

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        std::function<void()> onLoadClicked;

    private:
        LibraryEntry entry;
        juce::Image thumbnail;
        bool isHovered = false;
    };

    class LibraryFolderComponent : public juce::Component,
                                   public juce::SettableTooltipClient
    {
    public:
        LibraryFolderComponent (juce::File folderToUse, int itemCountToUse);

        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        std::function<void()> onOpenFolder;

    private:
        juce::File folder;
        int itemCount = 0;
        bool isHovered = false;
    };

    /**
        Main library browser component.
        Displays a grid of instruments with search/filter controls.
    */
    class LibraryBrowser : public juce::Component,
                          public juce::TextEditor::Listener,
                          public LibraryScanner::Listener
    {
    public:
        enum class PackFilter
        {
            Any,
            InstrumentsOnly,
            EffectsOnly
        };

        LibraryBrowser (LibraryScanner& scanner, PackFilter filter = PackFilter::Any);
        ~LibraryBrowser() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress& key) override;

        void textEditorTextChanged (juce::TextEditor&) override;
        void libraryChanged() override;

        // Callback when user selects a pack to load
        std::function<void(const juce::File& packFolder)> onPackSelected;
        
        // Callback when user clicks close button
        std::function<void()> onClose;

    private:
        LibraryScanner& scanner;
        juce::Array<LibraryEntry> filteredEntries;

        // UI components
        std::unique_ptr<juce::TextEditor> searchBox;
        std::unique_ptr<juce::TextButton> categoryButton;
        std::unique_ptr<juce::TextButton> refreshButton;
        std::unique_ptr<juce::TextButton> upButton;
        std::unique_ptr<juce::TextButton> closeButton;
        std::unique_ptr<juce::Viewport> viewport;
        std::unique_ptr<juce::Component> gridContainer;

        // Category filter
        juce::String currentCategory = "All";
        juce::StringArray categories = { "All", "Instruments", "Synth", "Samples", "Drums", "FX", "Factory Demos" };
        juce::File currentFolder;
        PackFilter packFilter = PackFilter::Any;

        void refreshGrid();
        void applyFilters();
        bool entryMatchesCategory (const LibraryEntry&) const;
        bool entryMatchesPackFilter (const LibraryEntry&) const;
        void rebuildCategories();
        void showCategoryMenu();
    };

} // namespace patchcraft
