#include "LibraryScanner.h"
#include "PatchCraftPackReader.h"
#include "AssetManager.h"

namespace patchcraft
{
    LibraryScanner::LibraryScanner()
    {
        juce::StringArray seenPaths;
        auto addIfDirectory = [this, &seenPaths] (const juce::File& folder)
        {
            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (folder.isDirectory() && ! seenPaths.contains (key))
            {
                searchPaths.addIfNotAlreadyThere (folder);
                seenPaths.add (key);
            }
        };

        const auto documentsPatchCraft = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("PatchCraft");
        addIfDirectory (documentsPatchCraft.getChildFile ("Library"));
        addIfDirectory (documentsPatchCraft.getChildFile ("Instruments"));
        addIfDirectory (documentsPatchCraft.getChildFile ("Patches"));
        addIfDirectory (documentsPatchCraft.getChildFile ("Packs"));
        addIfDirectory (documentsPatchCraft.getChildFile ("FactoryDemos"));
        addIfDirectory (documentsPatchCraft.getChildFile ("VST3 Exports"));
        addIfDirectory (documentsPatchCraft.getChildFile ("Exports"));

        const auto appDataPatchCraft = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft");
        addIfDirectory (appDataPatchCraft.getChildFile ("Library"));
        addIfDirectory (appDataPatchCraft.getChildFile ("Instruments"));
        addIfDirectory (appDataPatchCraft.getChildFile ("Packs"));

        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        addIfDirectory (appDir.getChildFile ("FactoryDemos"));
        addIfDirectory (appDir.getChildFile ("Library").getChildFile ("Templates"));
        addIfDirectory (appDir.getChildFile ("Library").getChildFile ("Instruments"));

        const auto cwd = juce::File::getCurrentWorkingDirectory();
        addIfDirectory (cwd.getChildFile ("FactoryDemos"));
        addIfDirectory (cwd.getChildFile ("Library").getChildFile ("Templates"));
        addIfDirectory (cwd.getChildFile ("Library").getChildFile ("Instruments"));
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
        juce::ignoreUnused (threadPool);
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

        scanPackFolder (dir);

        auto subdirs = dir.findChildFiles (juce::File::findDirectories, true);
        for (auto& sub : subdirs)
            scanPackFolder (sub);
    }

    void LibraryScanner::scanPackFolder (const juce::File& packFolder)
    {
        auto manifest = packFolder.getChildFile ("manifest.json");
        if (! manifest.existsAsFile())
            return;

        const auto packKey = packFolder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
        for (const auto& existing : entries)
        {
            const auto existingKey = existing.folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (existingKey == packKey)
                return;
        }

        LibraryEntry entry;
        if (loadPackMetadata (packFolder, entry))
        {
            if (entry.instrumentName.isEmpty())
                entry.instrumentName = packFolder.getFileNameWithoutExtension();
            entries.add (entry);
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
            entry.engineId = manifest.engine;
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
            if (e.category.equalsIgnoreCase (category)
                || e.engineId.equalsIgnoreCase (category)
                || e.tags.contains (category))
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
                e.category.toLowerCase().contains (q) ||
                e.engineId.toLowerCase().contains (q) ||
                e.tags.joinIntoString (" ").toLowerCase().contains (q))
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
