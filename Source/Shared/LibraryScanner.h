#pragma once

#include <juce_core/juce_core.h>
#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    /**
        Represents a discovered pack in the library.
        Contains lightweight metadata for display without loading the full pack.
    */
    struct LibraryEntry
    {
        juce::File folder;           // Path to the .patchcraft folder
        juce::String instrumentName;
        juce::String creator;
        juce::String description;
        juce::String category;
        juce::String libraryThumbnail;  // Relative path within pack
        juce::StringArray tags;
        juce::String version;
        juce::String website;
        juce::Image thumbnailImage;   // Loaded thumbnail for display

        bool isValid() const { return folder.isDirectory(); }
    };

    /**
        Scans directories for .patchcraft folders and builds a library index.
        Supports multiple search paths (user library, system library, etc.).
    */
    class LibraryScanner
    {
    public:
        LibraryScanner();
        ~LibraryScanner();

        // Add a directory to scan for packs
        void addSearchPath (const juce::File& path);

        // Remove a search path
        void removeSearchPath (const juce::File& path);

        // Get all search paths
        juce::Array<juce::File> getSearchPaths() const;

        // Scan all paths and rebuild the library index
        void scanLibrary (juce::ThreadPool* threadPool = nullptr);

        // Get all discovered entries
        juce::Array<LibraryEntry> getEntries() const;

        // Get entries filtered by category
        juce::Array<LibraryEntry> getEntriesByCategory (const juce::String& category) const;

        // Get entries filtered by tag
        juce::Array<LibraryEntry> getEntriesByTag (const juce::String& tag) const;

        // Search entries by name/description
        juce::Array<LibraryEntry> search (const juce::String& query) const;

        // Get entry by folder path
        LibraryEntry* getEntry (const juce::File& folder);

        // Listener for library changes
        struct Listener
        {
            virtual ~Listener() = default;
            virtual void libraryChanged() = 0;
        };
        void addListener (Listener* l);
        void removeListener (Listener* l);

    private:
        juce::Array<juce::File> searchPaths;
        juce::Array<LibraryEntry> entries;
        juce::ListenerList<Listener> listeners;

        // Scan a single directory
        void scanDirectory (const juce::File& dir);

        // Load metadata from a pack folder
        bool loadPackMetadata (const juce::File& packFolder, LibraryEntry& entry);

        // Load thumbnail image
        void loadThumbnail (LibraryEntry& entry);
    };

} // namespace patchcraft
