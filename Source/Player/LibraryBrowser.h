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

        std::function<void()> onLoadClicked;

    private:
        LibraryEntry entry;
        juce::Image thumbnail;
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
        LibraryBrowser (LibraryScanner& scanner);
        ~LibraryBrowser() override;

        void paint (juce::Graphics&) override;
        void resized() override;

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
        std::unique_ptr<juce::TextButton> closeButton;
        std::unique_ptr<juce::Viewport> viewport;
        std::unique_ptr<juce::Component> gridContainer;

        // Category filter
        juce::String currentCategory = "All";
        juce::StringArray categories = { "All", "Sample Instrument", "Synth", "FX" };

        void refreshGrid();
        void applyFilters();
        void showCategoryMenu();
    };

} // namespace patchcraft
