#include "LibraryScanner.h"
#include "PatchCraftPackReader.h"
#include "AssetManager.h"

namespace patchcraft
{
    LibraryScanner::LibraryScanner()
    {
        // Default search paths: user documents/PatchCraft/Library
        auto defaultPath = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile ("PatchCraft")
                              .getChildFile ("Library");
        if (defaultPath.isDirectory())
            searchPaths.add (defaultPath);
    }

    LibraryScanner::~LibraryScanner() = default;

    void LibraryScanner::addSearchPath (const juce::File& path)
    {
        if (path.isDirectory() && ! searchPaths.contains (path))
        {
            searchPaths.add (path);
        }
    }

    void LibraryScanner::removeSearchPath (const juce::File& path)
    {
        searchPaths.removeAllInstancesOf (path);
    }

    juce::Array<juce::File> LibraryScanner::getSearchPaths() const
    {
        return searchPaths;
    }

    void LibraryScanner::scanLibrary (juce::ThreadPool* threadPool)
    {
        entries.clear();

        for (auto& path : searchPaths)
        {
            scanDirectory (path);
        }

        listeners.call ([] (Listener& l) { l.libraryChanged(); });
    }

    void LibraryScanner::scanDirectory (const juce::File& dir)
    {
        if (! dir.isDirectory()) return;

        // Find all .patchcraft folders (they end with .patchcraft extension)
        auto subdirs = dir.findChildFiles (juce::File::findDirectories, false);

        for (auto& sub : subdirs)
        {
            // Check if this is a valid pack (has manifest.json)
            auto manifest = sub.getChildFile ("manifest.json");
            if (manifest.existsAsFile())
            {
                LibraryEntry entry;
                if (loadPackMetadata (sub, entry))
                {
                    entries.add (entry);
                }
            }
        }
    }

    bool LibraryScanner::loadPackMetadata (const juce::File& packFolder, LibraryEntry& entry)
    {
        entry.folder = packFolder;

        auto manifestFile = packFolder.getChildFile ("manifest.json");
        if (! manifestFile.existsAsFile())
            return false;

        try
        {
            auto json = juce::JSON::parse (manifestFile);
            auto manifest = Manifest::fromVar (json);

            entry.instrumentName = manifest.instrumentName;
            entry.creator = manifest.creator;
            entry.description = manifest.description;
            entry.category = manifest.category;
            entry.libraryThumbnail = manifest.libraryThumbnail;
            entry.tags = manifest.tags;
            entry.version = manifest.version;
            entry.website = manifest.website;

            // Load thumbnail image
            loadThumbnail (entry);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void LibraryScanner::loadThumbnail (LibraryEntry& entry)
    {
        if (entry.libraryThumbnail.isEmpty())
        {
            // Generate default thumbnail from hero image or background
            auto heroPath = entry.folder.getChildFile ("assets/hero.png");
            auto bgPath = entry.folder.getChildFile ("assets/background.png");

            if (heroPath.existsAsFile())
            {
                juce::PNGImageFormat png;
                std::unique_ptr<juce::FileInputStream> in (heroPath.createInputStream());
                if (in != nullptr)
                    entry.thumbnailImage = png.decodeImage (*in);
            }
            else if (bgPath.existsAsFile())
            {
                juce::PNGImageFormat png;
                std::unique_ptr<juce::FileInputStream> in (bgPath.createInputStream());
                if (in != nullptr)
                    entry.thumbnailImage = png.decodeImage (*in);
            }
            else
            {
                // Generate procedural thumbnail
                entry.thumbnailImage = AssetManager::renderDefaultHeroImage (300, 200);
            }
        }
        else
        {
            auto thumbPath = entry.folder.getChildFile (entry.libraryThumbnail);
            if (thumbPath.existsAsFile())
            {
                juce::PNGImageFormat png;
                std::unique_ptr<juce::FileInputStream> in (thumbPath.createInputStream());
                if (in != nullptr)
                    entry.thumbnailImage = png.decodeImage (*in);
            }
            else
            {
                // Fallback to generated
                entry.thumbnailImage = AssetManager::renderDefaultHeroImage (300, 200);
            }
        }
    }

    juce::Array<LibraryEntry> LibraryScanner::getEntries() const
    {
        return entries;
    }

    juce::Array<LibraryEntry> LibraryScanner::getEntriesByCategory (const juce::String& category) const
    {
        juce::Array<LibraryEntry> result;
        for (auto& e : entries)
            if (e.category == category)
                result.add (e);
        return result;
    }

    juce::Array<LibraryEntry> LibraryScanner::getEntriesByTag (const juce::String& tag) const
    {
        juce::Array<LibraryEntry> result;
        for (auto& e : entries)
            if (e.tags.contains (tag))
                result.add (e);
        return result;
    }

    juce::Array<LibraryEntry> LibraryScanner::search (const juce::String& query) const
    {
        juce::Array<LibraryEntry> result;
        auto q = query.toLowerCase();

        for (auto& e : entries)
        {
            if (e.instrumentName.toLowerCase().contains (q) ||
                e.description.toLowerCase().contains (q) ||
                e.creator.toLowerCase().contains (q) ||
                e.category.toLowerCase().contains (q))
            {
                result.add (e);
            }
        }
        return result;
    }

    LibraryEntry* LibraryScanner::getEntry (const juce::File& folder)
    {
        for (auto& e : entries)
            if (e.folder == folder)
                return &e;
        return nullptr;
    }

    void LibraryScanner::addListener (Listener* l)
    {
        listeners.add (l);
    }

    void LibraryScanner::removeListener (Listener* l)
    {
        listeners.remove (l);
    }

} // namespace patchcraft
