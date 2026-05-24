#include "BuiltAssetLibraryComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace patchcraft
{
    BuiltAssetLibraryComponent::BuiltAssetLibraryComponent (StudioMainComponent& o) : owner (o)
    {
        title.setText ("Library", juce::dontSendNotification);
        title.setFont (juce::Font (13.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (title);

        for (auto* button : { &backgroundsButton, &templatesButton, &assetsButton, &soundsButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }
        backgroundsButton.onClick = [this] { setMode (LibraryMode::Backgrounds); };
        templatesButton.onClick = [this] { setMode (LibraryMode::Templates); };
        assetsButton.onClick = [this] { setMode (LibraryMode::Assets); };
        soundsButton.onClick = [this] { setMode (LibraryMode::Sounds); };

        for (auto* button : { &refreshButton, &importButton, &folderButton, &deleteButton, &addButton, &previewButton })
            button->getProperties().set ("smallButton", true);
        autoAuditionToggle.getProperties().set ("smallButton", true);

        refreshButton.onClick = [this] { refresh(); };
        importButton.onClick = [this] { importAssets(); };
        folderButton.onClick = [this] { createFolderForMode(); };
        deleteButton.onClick = [this] { deleteSelectedEntry(); };
        addButton.onClick = [this] { addSelectedToCanvas(); };
        previewButton.onClick = [this] { showSelectedPreview(); };
        autoAuditionToggle.setToggleState (true, juce::dontSendNotification);
        autoAuditionToggle.onClick = [this]
        {
            if (! autoAuditionToggle.getToggleState())
                stopSoundPreview();
        };
        refreshButton.setTooltip ("Rescan the PatchCraft app library and factory folders.");
        importButton.setTooltip ("Bulk import files into the selected library: backgrounds, templates, built assets, or factory sounds.");
        folderButton.setTooltip ("Create a new folder in the selected user library section for manual organization.");
        deleteButton.setTooltip ("Delete the selected user library item. Factory and installed demo assets are protected.");
        previewButton.setTooltip ("Open a larger preview of the selected background, template, or asset before loading it.");
        autoAuditionToggle.setTooltip ("Turn automatic sample audition on or off while browsing the sound library.");
        addButton.setTooltip ("Add the selected asset. Templates replace the current project after confirmation. Sounds import into the Sample Mapper.");
        addAndMakeVisible (refreshButton);
        addAndMakeVisible (importButton);
        addAndMakeVisible (folderButton);
        addAndMakeVisible (deleteButton);
        addAndMakeVisible (addButton);
        addAndMakeVisible (previewButton);
        addAndMakeVisible (autoAuditionToggle);

        previewFormatManager.registerBasicFormats();

        list.setRowHeight (54);
        list.setOutlineThickness (0);
        list.setMultipleSelectionEnabled (true);
        list.setClickingTogglesRowSelection (false);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (list);
        refresh();
    }

    BuiltAssetLibraryComponent::~BuiltAssetLibraryComponent()
    {
        stopSoundPreview();
    }

    juce::File BuiltAssetLibraryComponent::getAssetLibraryRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("AssetLibrary");
    }

    juce::File BuiltAssetLibraryComponent::getCategoryFolder (const juce::String& category)
    {
        return getAssetLibraryRoot().getChildFile (category.toLowerCase());
    }

    juce::File BuiltAssetLibraryComponent::getWritableModeRoot() const
    {
        if (mode == LibraryMode::Templates)
            return getAssetLibraryRoot().getChildFile ("templates");
        if (mode == LibraryMode::Assets)
            return getAssetLibraryRoot().getChildFile ("assets");
        if (mode == LibraryMode::Sounds)
            return getAssetLibraryRoot().getChildFile ("sounds");
        return getAssetLibraryRoot().getChildFile ("backgrounds");
    }

    bool BuiltAssetLibraryComponent::isUserLibraryFile (const juce::File& file)
    {
        auto normalise = [] (const juce::File& f)
        {
            return f.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
        };

        const auto root = normalise (getAssetLibraryRoot());
        const auto target = normalise (file);
        return target == root || target.startsWith (root + "/");
    }

    bool BuiltAssetLibraryComponent::isSupportedImageFile (const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp";
    }

    bool BuiltAssetLibraryComponent::isSupportedAssetFile (const juce::File& file)
    {
        return isSupportedImageFile (file);
    }

    bool BuiltAssetLibraryComponent::isSupportedSoundFile (const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac";
    }

    juce::String BuiltAssetLibraryComponent::folderLabelFor (const juce::File& root, const juce::File& file)
    {
        auto folder = file.isDirectory() ? file : file.getParentDirectory();
        auto rootPath = root.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/");
        auto folderPath = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/");

        if (folderPath.startsWithIgnoreCase (rootPath))
        {
            auto relative = folderPath.substring (rootPath.length()).trimCharactersAtStart ("/");
            return relative.isNotEmpty() ? relative : root.getFileName();
        }

        return folder.getFileName();
    }

    juce::File BuiltAssetLibraryComponent::makeUniqueChildFile (const juce::File& folder, const juce::String& fileName)
    {
        auto candidate = folder.getChildFile (fileName);
        if (! candidate.exists())
            return candidate;

        const auto base = candidate.getFileNameWithoutExtension();
        const auto ext = candidate.getFileExtension();
        for (int index = 2; index < 10000; ++index)
        {
            auto numbered = folder.getChildFile (base + "_" + juce::String (index) + ext);
            if (! numbered.exists())
                return numbered;
        }

        return folder.getNonexistentChildFile (base + "_copy", ext);
    }

    void BuiltAssetLibraryComponent::copyAssetWithSidecars (const juce::File& source,
                                                            const juce::File& destinationFolder,
                                                            juce::StringArray& errors)
    {
        if (! source.existsAsFile() || ! isSupportedAssetFile (source))
        {
            errors.add (source.getFileName() + ": unsupported asset file");
            return;
        }

        destinationFolder.createDirectory();
        const auto destination = makeUniqueChildFile (destinationFolder, source.getFileName());
        if (! source.copyFileTo (destination))
        {
            errors.add (source.getFileName() + ": copy failed");
            return;
        }

        for (const auto& sidecarExtension : { juce::String ("patchcraft-knob.json"),
                                             juce::String ("patchcraft-slider.json"),
                                             juce::String ("patchcraft-meter.json") })
        {
            auto sidecar = source.withFileExtension (sidecarExtension);
            if (sidecar.existsAsFile())
                sidecar.copyFileTo (destination.withFileExtension (sidecarExtension));
        }
    }

    void BuiltAssetLibraryComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 88, getWidth(), 1);

        if (entries.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.5f);
            juce::String message;
            if (mode == LibraryMode::Templates)
                message = "No templates found.\nInstall templates into the app FactoryDemos folder or the project FactoryDemos folder.";
            else if (mode == LibraryMode::Backgrounds)
                message = "No backgrounds found.\nImport images or install backgrounds into PatchCraft/AssetLibrary/backgrounds.";
            else if (mode == LibraryMode::Sounds)
                message = "No factory sounds found.\nInstall WAV, AIFF, or FLAC files into Library/Sounds or PatchCraft/AssetLibrary/sounds.";
            else
                message = "No built assets yet.\nImport images in bulk or use Build > Add To Library to add knobs, sliders, and meters.";

            g.drawFittedText (message,
                              getLocalBounds().reduced (14).withTrimmedTop (108),
                              juce::Justification::centred, 4);
        }
    }

    void BuiltAssetLibraryComponent::resized()
    {
        auto r = getLocalBounds().reduced (5);
        auto header = r.removeFromTop (26);
        title.setBounds (header.removeFromLeft (juce::jmax (70, header.getWidth() - 72)));
        refreshButton.setBounds (header.reduced (2));
        r.removeFromTop (3);

        auto modes = r.removeFromTop (28);
        const int modeWidth = modes.getWidth() / 4;
        backgroundsButton.setBounds (modes.removeFromLeft (modeWidth).reduced (2));
        templatesButton.setBounds (modes.removeFromLeft (modeWidth).reduced (2));
        assetsButton.setBounds (modes.removeFromLeft (modeWidth).reduced (2));
        soundsButton.setBounds (modes.reduced (2));
        r.removeFromTop (3);

        auto actions = r.removeFromTop (28);
        const int actionWidth = actions.getWidth() / 6;
        importButton.setBounds (actions.removeFromLeft (actionWidth).reduced (2));
        folderButton.setBounds (actions.removeFromLeft (actionWidth).reduced (2));
        deleteButton.setBounds (actions.removeFromLeft (actionWidth).reduced (2));
        autoAuditionToggle.setBounds (actions.removeFromLeft (actionWidth).reduced (2));
        previewButton.setBounds (actions.removeFromLeft (actionWidth).reduced (2));
        addButton.setBounds (actions.reduced (2));
        r.removeFromTop (5);
        list.setBounds (r);
    }

    void BuiltAssetLibraryComponent::refresh()
    {
        stopSoundPreview();
        entries.clear();
        selectedRow = -1;
        backgroundsButton.setToggleState (mode == LibraryMode::Backgrounds, juce::dontSendNotification);
        templatesButton.setToggleState (mode == LibraryMode::Templates, juce::dontSendNotification);
        assetsButton.setToggleState (mode == LibraryMode::Assets, juce::dontSendNotification);
        soundsButton.setToggleState (mode == LibraryMode::Sounds, juce::dontSendNotification);
        addButton.setButtonText (mode == LibraryMode::Sounds ? "To Mapper" : "Add");
        autoAuditionToggle.setVisible (mode == LibraryMode::Sounds);

        if (mode == LibraryMode::Backgrounds)
            scanBackgrounds();
        else if (mode == LibraryMode::Templates)
            scanTemplates();
        else if (mode == LibraryMode::Sounds)
            scanSounds();
        else
            scanBuiltAssets();

        std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
        {
            if (a.isFolder != b.isFolder)
                return a.isFolder;
            if (a.category != b.category)
                return a.category < b.category;
            return a.title.compareIgnoreCase (b.title) < 0;
        });

        list.updateContent();
        list.repaint();
        repaint();
    }

    void BuiltAssetLibraryComponent::setMode (LibraryMode newMode)
    {
        if (mode == newMode)
            return;

        mode = newMode;
        if (mode != LibraryMode::Sounds)
            activeSoundFolder = juce::File();
        refresh();
    }

    void BuiltAssetLibraryComponent::showSoundsLibrary()
    {
        setMode (LibraryMode::Sounds);
        refresh();
    }

    juce::Array<juce::File> BuiltAssetLibraryComponent::getTemplateRoots()
    {
        juce::Array<juce::File> roots;
        juce::StringArray seenPaths;
        auto add = [&] (const juce::File& folder, bool allowMissingUserRoot = false)
        {
            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if ((folder.isDirectory() || allowMissingUserRoot) && ! seenPaths.contains (key))
            {
                roots.add (folder);
                seenPaths.add (key);
            }
        };

        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        add (appDir.getChildFile ("FactoryDemos"));
        add (appDir.getChildFile ("Library").getChildFile ("Templates"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("FactoryDemos"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("Library").getChildFile ("Templates"));
        add (getAssetLibraryRoot().getChildFile ("templates"));
        add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("PatchCraft").getChildFile ("Templates"));
        return roots;
    }

    juce::Array<juce::File> BuiltAssetLibraryComponent::getBackgroundRoots()
    {
        juce::Array<juce::File> roots;
        juce::StringArray seenPaths;
        auto add = [&] (const juce::File& folder, bool allowMissingUserRoot = false)
        {
            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if ((folder.isDirectory() || allowMissingUserRoot) && ! seenPaths.contains (key))
            {
                roots.add (folder);
                seenPaths.add (key);
            }
        };

        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        add (appDir.getChildFile ("Backgrounds"));
        add (appDir.getChildFile ("Library").getChildFile ("Backgrounds"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("Backgrounds"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("Library").getChildFile ("Backgrounds"));
        add (getAssetLibraryRoot().getChildFile ("backgrounds"));
        add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("PatchCraft").getChildFile ("Backgrounds"));
        return roots;
    }

    juce::Array<juce::File> BuiltAssetLibraryComponent::getSoundRoots()
    {
        juce::Array<juce::File> roots;
        juce::StringArray seenPaths;
        auto add = [&] (const juce::File& folder, bool allowMissingUserRoot = false)
        {
            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if ((folder.isDirectory() || allowMissingUserRoot) && ! seenPaths.contains (key))
            {
                roots.add (folder);
                seenPaths.add (key);
            }
        };

        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        add (appDir.getChildFile ("Sounds"));
        add (appDir.getChildFile ("Library").getChildFile ("Sounds"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("Sounds"));
        add (juce::File::getCurrentWorkingDirectory().getChildFile ("Library").getChildFile ("Sounds"));
        add (getAssetLibraryRoot().getChildFile ("sounds"), true);
        add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("PatchCraft").getChildFile ("Sounds"), true);
        return roots;
    }

    void BuiltAssetLibraryComponent::scanBackgrounds()
    {
        for (const auto& root : getTemplateRoots())
        {
            auto packs = root.findChildFiles (juce::File::findDirectories, true, "*.patchcraft");
            for (const auto& pack : packs)
            {
                auto manifestFile = pack.getChildFile ("manifest.json");
                if (! manifestFile.existsAsFile())
                    continue;

                juce::String title = pack.getFileNameWithoutExtension();
                auto manifest = juce::JSON::parse (manifestFile);
                if (auto* obj = manifest.getDynamicObject())
                {
                    const auto display = obj->getProperty ("instrumentName").toString();
                    if (display.isNotEmpty())
                        title = display;
                }

                const auto assets = pack.getChildFile ("assets");
                for (const auto& name : { juce::String ("background.png"),
                                          juce::String ("background-sectioned.png"),
                                          juce::String ("background-clean.png") })
                {
                    auto file = assets.getChildFile (name);
                    if (file.existsAsFile())
                    {
                        Entry entry;
                        entry.category = "backgrounds";
                        entry.file = file;
                        entry.title = title + " - " + file.getFileNameWithoutExtension();
                        entry.subtitle = pack.getFileName();
                        entry.folderPath = folderLabelFor (root, pack);
                        entries.push_back (entry);
                    }
                }
            }
        }

        for (const auto& root : getBackgroundRoots())
        {
            if (isUserLibraryFile (root))
                root.createDirectory();
            addFolderEntriesForRoot (root, "backgrounds", false);
            for (auto file : root.findChildFiles (juce::File::findFiles, true, "*.png;*.jpg;*.jpeg;*.gif;*.webp"))
            {
                if (! isSupportedImageFile (file))
                    continue;
                Entry entry;
                entry.category = "backgrounds";
                entry.file = file;
                entry.title = file.getFileNameWithoutExtension();
                entry.subtitle = "Background";
                entry.folderPath = folderLabelFor (root, file);
                entries.push_back (entry);
            }
        }
    }

    void BuiltAssetLibraryComponent::scanTemplates()
    {
        for (const auto& root : getTemplateRoots())
        {
            if (isUserLibraryFile (root))
                root.createDirectory();
            addFolderEntriesForRoot (root, "templates", true);

            auto folders = root.findChildFiles (juce::File::findDirectories, true, "*.patchcraft");
            for (const auto& folder : folders)
            {
                auto manifestFile = folder.getChildFile ("manifest.json");
                if (! manifestFile.existsAsFile())
                    continue;

                Entry entry;
                entry.category = "templates";
                entry.file = folder;
                entry.title = folder.getFileNameWithoutExtension();
                entry.subtitle = "PatchCraft template";
                entry.folderPath = folderLabelFor (root, folder);

                auto manifest = juce::JSON::parse (manifestFile);
                if (auto* obj = manifest.getDynamicObject())
                {
                    const auto display = obj->getProperty ("instrumentName").toString();
                    const auto engine = obj->getProperty ("engine").toString();
                    const auto category = obj->getProperty ("category").toString();
                    if (display.isNotEmpty())
                        entry.title = display;
                    entry.subtitle = (engine.isNotEmpty() ? engine.toUpperCase() : juce::String ("PACK"))
                        + (category.isNotEmpty() ? " | " + category : juce::String());
                }

                entries.push_back (entry);
            }
        }
    }

    void BuiltAssetLibraryComponent::scanBuiltAssets()
    {
        juce::Array<juce::File> assetRoots;
        juce::StringArray seenRootPaths;
        auto addRoot = [&] (const juce::File& folder)
        {
            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (! seenRootPaths.contains (key))
            {
                assetRoots.add (folder);
                seenRootPaths.add (key);
            }
        };
        juce::StringArray seenAssetPaths;
        auto addEntry = [&] (Entry entry)
        {
            const auto key = entry.file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (seenAssetPaths.contains (key))
                return;
            seenAssetPaths.add (key);
            entries.push_back (std::move (entry));
        };
        auto categoryForFile = [] (const juce::File& file)
        {
            const auto path = file.getFullPathName().replaceCharacter ('\\', '/').toLowerCase();
            if (path.contains ("/sliders/") || path.contains ("/slider/"))
                return juce::String ("sliders");
            if (path.contains ("/meters/") || path.contains ("/meter/"))
                return juce::String ("meters");
            if (path.contains ("/knobs/") || path.contains ("/knob/"))
                return juce::String ("knobs");
            if (file.withFileExtension ("patchcraft-slider.json").existsAsFile())
                return juce::String ("sliders");
            if (file.withFileExtension ("patchcraft-meter.json").existsAsFile())
                return juce::String ("meters");
            if (file.withFileExtension ("patchcraft-knob.json").existsAsFile())
                return juce::String ("knobs");
            return juce::String ("assets");
        };

        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        addRoot (getAssetLibraryRoot());
        addRoot (getAssetLibraryRoot().getChildFile ("assets"));
        addRoot (appDir.getChildFile ("AssetLibrary"));
        addRoot (appDir.getChildFile ("Library").getChildFile ("Assets"));
        addRoot (juce::File::getCurrentWorkingDirectory().getChildFile ("AssetLibrary"));
        addRoot (juce::File::getCurrentWorkingDirectory().getChildFile ("Library").getChildFile ("Assets"));

        for (const auto& category : { juce::String ("knobs"), juce::String ("sliders"), juce::String ("meters") })
        {
            for (const auto& root : assetRoots)
            {
                auto folder = root.getChildFile (category.toLowerCase());
                if (root == getAssetLibraryRoot())
                    folder.createDirectory();
                if (! folder.isDirectory())
                    continue;

                for (auto file : folder.findChildFiles (juce::File::findFiles, true, "*.png;*.jpg;*.jpeg;*.gif;*.webp"))
                {
                    if (! isSupportedAssetFile (file))
                        continue;
                    auto entry = inspectAssetFile (file, category);
                    entry.folderPath = folderLabelFor (folder, file);
                    addEntry (std::move (entry));
                }
            }
        }

        for (const auto& root : assetRoots)
        {
            if (root == getAssetLibraryRoot())
                continue;
            if (root == getAssetLibraryRoot().getChildFile ("assets"))
                root.createDirectory();
            if (! root.isDirectory())
                continue;
            addFolderEntriesForRoot (root, "assets", false);

            for (auto file : root.findChildFiles (juce::File::findFiles, true, "*.png;*.jpg;*.jpeg;*.gif;*.webp"))
            {
                if (! isSupportedAssetFile (file))
                    continue;
                auto entry = inspectAssetFile (file, categoryForFile (file));
                entry.folderPath = folderLabelFor (root, file);
                if (entry.category == "assets")
                    entry.subtitle = "Image asset";
                addEntry (std::move (entry));
            }
        }
    }

    void BuiltAssetLibraryComponent::scanSounds()
    {
        juce::StringArray seenSoundPaths;
        auto addEntry = [&] (Entry entry)
        {
            const auto key = entry.file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (seenSoundPaths.contains (key))
                return;
            seenSoundPaths.add (key);
            entries.push_back (std::move (entry));
        };

        auto addSoundFile = [&] (const juce::File& root, const juce::File& file)
        {
            if (! isSupportedSoundFile (file))
                return;

            Entry entry;
            entry.category = "sounds";
            entry.file = file;
            entry.title = file.getFileNameWithoutExtension();
            entry.subtitle = "Sound Library";
            entry.folderPath = folderLabelFor (root, file);
            addEntry (std::move (entry));
        };

        if (activeSoundFolder.isDirectory())
        {
            Entry back;
            back.category = "sounds";
            back.title = "All Sounds";
            back.subtitle = "Back to the main sound library";
            back.folderPath = "Library";
            back.isFolder = true;
            entries.push_back (std::move (back));

            addFolderEntriesForRoot (activeSoundFolder, "sounds", false);
            for (auto file : activeSoundFolder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac"))
                addSoundFile (activeSoundFolder, file);
            return;
        }

        for (const auto& root : getSoundRoots())
        {
            if (isUserLibraryFile (root))
                root.createDirectory();
            addFolderEntriesForRoot (root, "sounds", false);

            for (auto file : root.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac"))
                addSoundFile (root, file);
        }
    }

    void BuiltAssetLibraryComponent::addFolderEntriesForRoot (const juce::File& root,
                                                              const juce::String& category,
                                                              bool skipPatchcraftTemplates)
    {
        if (! isUserLibraryFile (root) || ! root.isDirectory())
            return;

        juce::StringArray seenPaths;
        for (const auto& existing : entries)
            if (existing.isFolder)
                seenPaths.addIfNotAlreadyThere (existing.file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase());

        auto folders = root.findChildFiles (juce::File::findDirectories, true, "*");
        for (const auto& folder : folders)
        {
            if (skipPatchcraftTemplates
                && (folder.getFileName().endsWithIgnoreCase (".patchcraft")
                    || folder.getChildFile ("manifest.json").existsAsFile()))
                continue;

            const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (seenPaths.contains (key))
                continue;

            seenPaths.add (key);
            Entry entry;
            entry.category = category;
            entry.file = folder;
            entry.title = folder.getFileName();
            entry.subtitle = "Library folder";
            entry.folderPath = folderLabelFor (root, folder);
            entry.isFolder = true;
            entries.push_back (std::move (entry));
        }
    }

    void BuiltAssetLibraryComponent::selectEntryForFile (const juce::File& file)
    {
        const auto target = file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const auto current = entries[(size_t) i].file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
            if (current == target)
            {
                selectedRow = i;
                list.selectRow (i);
                list.scrollToEnsureRowIsOnscreen (i);
                return;
            }
        }
    }

    BuiltAssetLibraryComponent::Entry BuiltAssetLibraryComponent::inspectAssetFile (juce::File file,
                                                                                   juce::String category)
    {
        Entry entry;
        entry.file = std::move (file);
        entry.category = std::move (category);
        entry.title = entry.file.getFileNameWithoutExtension();

        auto sidecarForCategory = [&]
        {
            if (entry.category.startsWithIgnoreCase ("slider"))
                return entry.file.withFileExtension ("patchcraft-slider.json");
            if (entry.category.startsWithIgnoreCase ("meter"))
                return entry.file.withFileExtension ("patchcraft-meter.json");
            return entry.file.withFileExtension ("patchcraft-knob.json");
        };

        auto sidecar = sidecarForCategory();
        if (sidecar.existsAsFile())
        {
            auto parsed = juce::JSON::parse (sidecar);
            if (auto* root = parsed.getDynamicObject())
            {
                const int frames = (int) root->getProperty ("frames");
                if (frames > 0)
                    entry.frames = frames;
                if (root->hasProperty ("stripVertical"))
                    entry.vertical = (bool) root->getProperty ("stripVertical");
                if (auto* style = root->getProperty ("style").getDynamicObject())
                {
                    const int styleFrames = (int) style->getProperty ("frames");
                    if (styleFrames > 0 && entry.frames <= 1)
                        entry.frames = styleFrames;
                    if (style->hasProperty ("outputMode"))
                        entry.vertical = ! style->getProperty ("outputMode").toString().containsIgnoreCase ("horizontal");
                }
            }
        }

        auto image = juce::ImageFileFormat::loadFrom (entry.file);
        if (image.isValid())
        {
            const int width = image.getWidth();
            const int height = image.getHeight();
            if (entry.frames > 1)
            {
                if (entry.vertical && height / entry.frames <= 0)
                    entry.frames = 1;
                if (! entry.vertical && width / entry.frames <= 0)
                    entry.frames = 1;
            }
            else if (height >= width && width > 0 && height % width == 0)
            {
                entry.vertical = true;
                entry.frames = juce::jmax (1, height / width);
            }
            else if (width >= height && height > 0 && width % height == 0)
            {
                entry.vertical = false;
                entry.frames = juce::jmax (1, width / height);
            }
        }
        return entry;
    }

    int BuiltAssetLibraryComponent::getNumRows()
    {
        return (int) entries.size();
    }

    void BuiltAssetLibraryComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= (int) entries.size())
            return;

        const auto& entry = entries[(size_t) row];
        const bool isDropTarget = row == dropTargetRow && entry.isFolder && isUserLibraryFile (entry.file);
        auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (4, 3);
        g.setColour (selected ? PatchCraftLookAndFeel::raised()
                              : (isDropTarget ? PatchCraftLookAndFeel::accent().withAlpha (0.18f)
                                              : PatchCraftLookAndFeel::panelAlt()));
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);
        g.setColour ((selected || isDropTarget) ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (bounds.toFloat(), 6.0f, isDropTarget ? 2.0f : 1.0f);

        auto thumb = bounds.removeFromLeft (46).reduced (5);
        juce::Image image;
        if (entry.isFolder)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.14f));
            g.fillRoundedRectangle (thumb.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            auto folder = thumb.reduced (7, 11).toFloat();
            juce::Path icon;
            icon.addRoundedRectangle (folder.getX(), folder.getY() + folder.getHeight() * 0.24f,
                                      folder.getWidth(), folder.getHeight() * 0.66f, 3.0f);
            icon.addRoundedRectangle (folder.getX() + 2.0f, folder.getY(),
                                      folder.getWidth() * 0.45f, folder.getHeight() * 0.34f, 2.0f);
            g.fillPath (icon);
        }
        else if (entry.category == "templates")
        {
            auto thumbFile = entry.file.getChildFile ("assets").getChildFile ("thumbnail.png");
            if (! thumbFile.existsAsFile())
                thumbFile = entry.file.getChildFile ("assets").getChildFile ("background.png");
            image = juce::ImageFileFormat::loadFrom (thumbFile);
        }
        else
        {
            image = juce::ImageFileFormat::loadFrom (entry.file);
        }
        if (image.isValid())
        {
            const int frameW = entry.vertical ? image.getWidth()
                                              : juce::jmax (1, image.getWidth() / juce::jmax (1, entry.frames));
            const int frameH = entry.vertical ? juce::jmax (1, image.getHeight() / juce::jmax (1, entry.frames))
                                              : image.getHeight();
            auto frame = image.getClippedImage ({ 0, 0, frameW, frameH });
            g.drawImageWithin (frame, thumb.getX(), thumb.getY(), thumb.getWidth(), thumb.getHeight(),
                               juce::RectanglePlacement::centred);
        }
        else if (entry.category == "sounds")
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.14f));
            g.fillRoundedRectangle (thumb.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            auto wave = thumb.reduced (6, 12);
            juce::Path path;
            for (int i = 0; i < wave.getWidth(); ++i)
            {
                const float phase = (float) i / (float) juce::jmax (1, wave.getWidth() - 1);
                const float sample = std::sin (phase * juce::MathConstants<float>::twoPi * 2.0f)
                                   * (0.35f + 0.45f * std::sin (phase * juce::MathConstants<float>::pi));
                const auto x = (float) wave.getX() + (float) i;
                const auto y = (float) wave.getCentreY() - sample * (float) wave.getHeight() * 0.45f;
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            g.strokePath (path, juce::PathStrokeType (1.6f));
        }

        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (entry.title.isNotEmpty() ? entry.title : entry.file.getFileNameWithoutExtension(),
                    bounds.removeFromTop (22), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        auto detail = entry.subtitle;
        if (entry.isFolder)
            detail = entry.folderPath.isNotEmpty() ? entry.folderPath + "  |  Folder" : "Folder";
        if (detail.isEmpty())
            detail = entry.category + "  |  " + juce::String (entry.frames) + " frames  |  "
                + (entry.vertical ? "vertical" : "horizontal");
        if (entry.category == "sounds")
            detail = entry.folderPath.isNotEmpty() ? entry.folderPath + "  |  Import to Sample Mapper" : "Import to Sample Mapper";
        else if (entry.folderPath.isNotEmpty())
            detail = entry.folderPath + "  |  " + detail;
        g.drawText (detail, bounds, juce::Justification::centredLeft);
    }

    void BuiltAssetLibraryComponent::selectedRowsChanged (int lastRowSelected)
    {
        selectedRow = lastRowSelected;
        if (selectedRow >= 0
            && selectedRow < (int) entries.size()
            && mode == LibraryMode::Sounds
            && list.getSelectedRows().size() == 1)
        {
            const auto& entry = entries[(size_t) selectedRow];
            if (entry.isFolder)
            {
                activeSoundFolder = entry.file.isDirectory() ? entry.file : juce::File();
                refresh();
                return;
            }

            if (autoAuditionToggle.getToggleState() && entry.file.existsAsFile())
                auditionSoundFile (entry.file);
        }
    }

    void BuiltAssetLibraryComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        selectedRow = row;
        if (selectedRow >= 0
            && selectedRow < (int) entries.size()
            && mode == LibraryMode::Sounds
            && entries[(size_t) selectedRow].isFolder)
        {
            const auto& entry = entries[(size_t) selectedRow];
            activeSoundFolder = entry.file.isDirectory() ? entry.file : juce::File();
            refresh();
            return;
        }

        addSelectedToCanvas();
    }

    juce::var BuiltAssetLibraryComponent::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
    {
        if (selectedRows.size() <= 0)
            return {};

        juce::Array<juce::var> paths;
        juce::String category;
        int frames = 1;
        bool vertical = true;
        juce::String title;
        juce::String primaryPath;

        for (int i = 0; i < selectedRows.size(); ++i)
        {
            const int row = selectedRows[i];
            if (row < 0 || row >= (int) entries.size())
                continue;

            const auto& entry = entries[(size_t) row];
            if (entry.isFolder || ! entry.file.existsAsFile())
                continue;

            if (category.isEmpty())
            {
                category = entry.category;
                frames = entry.frames;
                vertical = entry.vertical;
                title = entry.title.isNotEmpty() ? entry.title : entry.file.getFileNameWithoutExtension();
                primaryPath = entry.file.getFullPathName();
            }

            if (entry.category == category)
                paths.add (entry.file.getFullPathName());
        }

        if (paths.isEmpty() || primaryPath.isEmpty())
            return {};

        auto* object = new juce::DynamicObject();
        object->setProperty ("patchcraftDragType", "libraryAsset");
        object->setProperty ("category", category);
        object->setProperty ("path", primaryPath);
        object->setProperty ("paths", juce::var (paths));
        object->setProperty ("frames", frames);
        object->setProperty ("vertical", vertical);
        object->setProperty ("title", paths.size() > 1 ? juce::String (paths.size()) + " selected items" : title);
        return juce::var (object);
    }

    int BuiltAssetLibraryComponent::rowAtLocalPosition (juce::Point<int> localPosition) const
    {
        if (! list.getBounds().contains (localPosition))
            return -1;

        const auto listPoint = localPosition - list.getPosition();
        return list.getRowContainingPosition (listPoint.x, listPoint.y);
    }

    juce::File BuiltAssetLibraryComponent::getTargetFolderForImport (juce::Point<int> localPosition) const
    {
        auto root = getWritableModeRoot();
        root.createDirectory();

        const int hoverRow = rowAtLocalPosition (localPosition);
        if (hoverRow >= 0 && hoverRow < (int) entries.size())
        {
            const auto& hovered = entries[(size_t) hoverRow];
            if (hovered.isFolder && hovered.file.isDirectory() && isUserLibraryFile (hovered.file))
                return hovered.file;
        }

        if (selectedRow >= 0 && selectedRow < (int) entries.size())
        {
            const auto& selected = entries[(size_t) selectedRow];
            if (selected.isFolder && selected.file.isDirectory() && isUserLibraryFile (selected.file))
                return selected.file;
            if (! selected.isFolder && selected.file.existsAsFile() && isUserLibraryFile (selected.file))
                return selected.file.getParentDirectory();
        }

        return root.getChildFile ("Imported");
    }

    bool BuiltAssetLibraryComponent::entryCanBeDroppedIntoCurrentMode (const juce::File& file) const
    {
        if (file.isDirectory())
            return mode == LibraryMode::Sounds || mode == LibraryMode::Templates;

        if (mode == LibraryMode::Templates)
            return false;
        if (mode == LibraryMode::Sounds)
            return file.existsAsFile() && isSupportedSoundFile (file);
        if (mode == LibraryMode::Backgrounds)
            return file.existsAsFile() && isSupportedImageFile (file);
        return file.existsAsFile() && isSupportedAssetFile (file);
    }

    bool BuiltAssetLibraryComponent::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& path : files)
            if (entryCanBeDroppedIntoCurrentMode (juce::File (path)))
                return true;

        return false;
    }

    void BuiltAssetLibraryComponent::fileDragMove (const juce::StringArray& files, int x, int y)
    {
        int newDropTarget = -1;
        if (isInterestedInFileDrag (files))
        {
            const int row = rowAtLocalPosition ({ x, y });
            if (row >= 0 && row < (int) entries.size())
            {
                const auto& entry = entries[(size_t) row];
                if (entry.isFolder && entry.file.isDirectory() && isUserLibraryFile (entry.file))
                    newDropTarget = row;
            }
        }

        if (dropTargetRow != newDropTarget)
        {
            dropTargetRow = newDropTarget;
            list.repaint();
        }
    }

    void BuiltAssetLibraryComponent::fileDragExit (const juce::StringArray&)
    {
        if (dropTargetRow != -1)
        {
            dropTargetRow = -1;
            list.repaint();
        }
    }

    void BuiltAssetLibraryComponent::filesDropped (const juce::StringArray& files, int x, int y)
    {
        dropTargetRow = -1;
        copyExternalFilesIntoFolder (files, getTargetFolderForImport ({ x, y }));
        refresh();
    }

    bool BuiltAssetLibraryComponent::isInterestedInDragSource (const SourceDetails& details)
    {
        if (auto* object = details.description.getDynamicObject())
        {
            if (object->getProperty ("patchcraftDragType").toString() != "libraryAsset")
                return false;

            if (auto* paths = object->getProperty ("paths").getArray())
            {
                for (const auto& path : *paths)
                    if (entryCanBeDroppedIntoCurrentMode (juce::File (path.toString())))
                        return true;
                return false;
            }

            return entryCanBeDroppedIntoCurrentMode (juce::File (object->getProperty ("path").toString()));
        }

        return false;
    }

    void BuiltAssetLibraryComponent::itemDragMove (const SourceDetails& details)
    {
        int newDropTarget = -1;
        if (isInterestedInDragSource (details))
        {
            const int row = rowAtLocalPosition (details.localPosition);
            if (row >= 0 && row < (int) entries.size())
            {
                const auto& entry = entries[(size_t) row];
                if (entry.isFolder && entry.file.isDirectory() && isUserLibraryFile (entry.file))
                    newDropTarget = row;
            }
        }

        if (dropTargetRow != newDropTarget)
        {
            dropTargetRow = newDropTarget;
            list.repaint();
        }
    }

    void BuiltAssetLibraryComponent::itemDragExit (const SourceDetails&)
    {
        if (dropTargetRow != -1)
        {
            dropTargetRow = -1;
            list.repaint();
        }
    }

    void BuiltAssetLibraryComponent::itemDropped (const SourceDetails& details)
    {
        dropTargetRow = -1;
        if (auto* object = details.description.getDynamicObject())
            if (object->getProperty ("patchcraftDragType").toString() == "libraryAsset")
            {
                const auto targetFolder = getTargetFolderForImport (details.localPosition);
                if (auto* paths = object->getProperty ("paths").getArray())
                {
                    for (const auto& path : *paths)
                        moveOrCopyLibraryEntryToFolder (juce::File (path.toString()), targetFolder);
                }
                else
                {
                    moveOrCopyLibraryEntryToFolder (juce::File (object->getProperty ("path").toString()),
                                                    targetFolder);
                }
            }

        list.repaint();
    }

    void BuiltAssetLibraryComponent::addSelectedToCanvas()
    {
        if (selectedRow < 0 || selectedRow >= (int) entries.size())
            return;

        const auto& entry = entries[(size_t) selectedRow];
        if (entry.isFolder)
            return;

        if (entry.category == "sounds")
        {
            juce::Array<juce::File> files;
            files.add (entry.file);
            owner.importSampleFiles (files, true, false);
            owner.setBottomTab (BottomPanel::Page::Samples);
            return;
        }

        owner.addLibraryAssetToCanvas (entry.category, entry.file, entry.frames, entry.vertical);
    }

    void BuiltAssetLibraryComponent::importAssets()
    {
        const auto modeToImport = mode;
        auto targetFolder = getTargetFolderForImport();
        targetFolder.createDirectory();

        const auto titleText = modeToImport == LibraryMode::Templates
            ? "Import PatchCraft template folders"
            : (modeToImport == LibraryMode::Backgrounds ? "Import background images"
                : (modeToImport == LibraryMode::Sounds ? "Import sounds into the PatchCraft sound library"
                                                       : "Import built assets or image assets"));

        const auto wildcards = modeToImport == LibraryMode::Templates
            ? juce::String ("*")
            : (modeToImport == LibraryMode::Sounds ? juce::String ("*.wav;*.aif;*.aiff;*.flac")
                                                   : juce::String ("*.png;*.jpg;*.jpeg;*.gif;*.webp"));

        int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems;
        if (modeToImport == LibraryMode::Templates)
            flags |= juce::FileBrowserComponent::canSelectDirectories;
        else
            flags |= juce::FileBrowserComponent::canSelectFiles;
        if (modeToImport == LibraryMode::Sounds)
            flags |= juce::FileBrowserComponent::canSelectDirectories;

        importChooser = std::make_unique<juce::FileChooser> (titleText,
                                                             juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
                                                             wildcards);
        juce::Component::SafePointer<BuiltAssetLibraryComponent> safe (this);
        importChooser->launchAsync (flags, [safe, modeToImport, targetFolder] (const juce::FileChooser& chooser)
        {
            auto* component = safe.getComponent();
            if (component == nullptr)
                return;

            juce::StringArray errors;
            int imported = 0;

            for (const auto& source : chooser.getResults())
            {
                if (modeToImport == LibraryMode::Templates)
                {
                    if (! source.isDirectory() || ! source.getChildFile ("manifest.json").existsAsFile())
                    {
                        errors.add (source.getFileName() + ": template folders must contain manifest.json");
                        continue;
                    }

                    const auto destination = makeUniqueChildFile (targetFolder, source.getFileName().endsWithIgnoreCase (".patchcraft")
                        ? source.getFileName()
                        : source.getFileName() + ".patchcraft");
                    if (source.copyDirectoryTo (destination))
                        ++imported;
                    else
                        errors.add (source.getFileName() + ": template copy failed");
                    continue;
                }

                if (modeToImport == LibraryMode::Sounds)
                {
                    if (source.isDirectory())
                    {
                        const auto destinationFolder = makeUniqueChildFile (targetFolder, source.getFileName());
                        destinationFolder.createDirectory();

                        juce::Array<juce::File> children;
                        source.findChildFiles (children, juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac");
                        for (const auto& child : children)
                        {
                            const auto relative = child.getRelativePathFrom (source).replaceCharacter ('\\', '/');
                            const auto childDestinationFolder = destinationFolder.getChildFile (relative).getParentDirectory();
                            childDestinationFolder.createDirectory();
                            if (child.copyFileTo (makeUniqueChildFile (childDestinationFolder, child.getFileName())))
                                ++imported;
                            else
                                errors.add (child.getFileName() + ": sound copy failed");
                        }

                        if (children.isEmpty())
                            errors.add (source.getFileName() + ": no supported audio files found");
                        continue;
                    }

                    if (! source.existsAsFile() || ! isSupportedSoundFile (source))
                    {
                        errors.add (source.getFileName() + ": unsupported sound file");
                        continue;
                    }

                    targetFolder.createDirectory();
                    const auto destination = makeUniqueChildFile (targetFolder, source.getFileName());
                    if (source.copyFileTo (destination))
                        ++imported;
                    else
                        errors.add (source.getFileName() + ": sound copy failed");
                    continue;
                }

                if (! source.existsAsFile() || ! isSupportedImageFile (source))
                {
                    errors.add (source.getFileName() + ": unsupported image file");
                    continue;
                }

                const auto errorCountBefore = errors.size();
                copyAssetWithSidecars (source, targetFolder, errors);
                if (errors.size() == errorCountBefore)
                    ++imported;
            }

            component->refresh();

            juce::String message = juce::String (imported) + " file" + (imported == 1 ? "" : "s")
                + " imported into:\n" + targetFolder.getFullPathName();
            if (! errors.isEmpty())
                message += "\n\nSkipped:\n" + errors.joinIntoString ("\n");

            juce::AlertWindow::showMessageBoxAsync (errors.isEmpty() ? juce::MessageBoxIconType::InfoIcon
                                                                      : juce::MessageBoxIconType::WarningIcon,
                                                    "Library Import",
                                                    message);
        });
    }

    void BuiltAssetLibraryComponent::copyExternalFilesIntoFolder (const juce::StringArray& paths,
                                                                  const juce::File& targetFolder)
    {
        targetFolder.createDirectory();
        juce::StringArray errors;
        int imported = 0;

        auto copySound = [&] (const juce::File& source, const juce::File& destinationFolder)
        {
            if (! source.existsAsFile() || ! isSupportedSoundFile (source))
                return;

            destinationFolder.createDirectory();
            if (source.copyFileTo (makeUniqueChildFile (destinationFolder, source.getFileName())))
                ++imported;
            else
                errors.add (source.getFileName() + ": copy failed");
        };

        for (const auto& path : paths)
        {
            const juce::File source (path);

            if (mode == LibraryMode::Templates)
            {
                if (source.isDirectory() && source.getChildFile ("manifest.json").existsAsFile())
                {
                    const auto destination = makeUniqueChildFile (targetFolder,
                        source.getFileName().endsWithIgnoreCase (".patchcraft")
                            ? source.getFileName()
                            : source.getFileName() + ".patchcraft");
                    if (source.copyDirectoryTo (destination))
                        ++imported;
                    else
                        errors.add (source.getFileName() + ": template copy failed");
                }
                continue;
            }

            if (mode == LibraryMode::Sounds)
            {
                if (source.isDirectory())
                {
                    const auto destinationFolder = makeUniqueChildFile (targetFolder, source.getFileName());
                    destinationFolder.createDirectory();

                    juce::Array<juce::File> children;
                    source.findChildFiles (children, juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac");
                    for (const auto& child : children)
                    {
                        const auto relative = child.getRelativePathFrom (source).replaceCharacter ('\\', '/');
                        copySound (child, destinationFolder.getChildFile (relative).getParentDirectory());
                    }
                }
                else
                {
                    copySound (source, targetFolder);
                }
                continue;
            }

            if (! source.existsAsFile() || ! isSupportedImageFile (source))
            {
                errors.add (source.getFileName() + ": unsupported file");
                continue;
            }

            const auto errorCountBefore = errors.size();
            copyAssetWithSidecars (source, targetFolder, errors);
            if (errors.size() == errorCountBefore)
                ++imported;
        }

        refresh();
        selectEntryForFile (targetFolder);

        juce::String message = juce::String (imported) + " item" + (imported == 1 ? "" : "s")
            + " imported into:\n" + targetFolder.getFullPathName();
        if (! errors.isEmpty())
            message += "\n\nSkipped:\n" + errors.joinIntoString ("\n");

        juce::AlertWindow::showMessageBoxAsync (errors.isEmpty() ? juce::MessageBoxIconType::InfoIcon
                                                                  : juce::MessageBoxIconType::WarningIcon,
                                                "Library Drop Import",
                                                message);
    }

    void BuiltAssetLibraryComponent::moveOrCopyLibraryEntryToFolder (const juce::File& source,
                                                                     const juce::File& targetFolder)
    {
        if (! source.existsAsFile() || ! entryCanBeDroppedIntoCurrentMode (source))
            return;

        targetFolder.createDirectory();
        if (source.getParentDirectory() == targetFolder)
            return;

        const bool canMove = isUserLibraryFile (source);
        const auto destination = makeUniqueChildFile (targetFolder, source.getFileName());
        const bool ok = canMove ? source.moveFileTo (destination)
                                : source.copyFileTo (destination);

        if (! ok)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Library Move Failed",
                                                    "Could not move or copy:\n" + source.getFullPathName()
                                                    + "\n\nInto:\n" + targetFolder.getFullPathName());
            return;
        }

        if (mode == LibraryMode::Assets)
        {
            for (const auto& sidecarExtension : { juce::String ("patchcraft-knob.json"),
                                                 juce::String ("patchcraft-slider.json"),
                                                 juce::String ("patchcraft-meter.json") })
            {
                auto sidecar = source.withFileExtension (sidecarExtension);
                if (! sidecar.existsAsFile())
                    continue;

                auto destinationSidecar = destination.withFileExtension (sidecarExtension);
                if (canMove && isUserLibraryFile (sidecar))
                    sidecar.moveFileTo (destinationSidecar);
                else
                    sidecar.copyFileTo (destinationSidecar);
            }
        }

        refresh();
        selectEntryForFile (destination);
    }

    void BuiltAssetLibraryComponent::deleteSelectedEntry()
    {
        if (selectedRow < 0 || selectedRow >= (int) entries.size())
            return;

        const auto entry = entries[(size_t) selectedRow];
        if (! isUserLibraryFile (entry.file))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Protected Library Item",
                                                    "Factory demos and installed assets are read-only from this panel.\n\n"
                                                    "Only files inside the user PatchCraft AssetLibrary can be deleted here.");
            return;
        }

        juce::Component::SafePointer<BuiltAssetLibraryComponent> safe (this);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Delete Library Item")
                .withMessage ("Move this item to the trash?\n\n" + entry.title + "\n" + entry.file.getFullPathName())
                .withButton ("Delete")
                .withButton ("Cancel"),
            [safe, entry] (int result)
            {
                if (result != 1)
                    return;

                if (entry.file.isDirectory())
                {
                    entry.file.moveToTrash();
                }
                else
                {
                    for (const auto& sidecarExtension : { juce::String ("patchcraft-knob.json"),
                                                         juce::String ("patchcraft-slider.json"),
                                                         juce::String ("patchcraft-meter.json") })
                    {
                        auto sidecar = entry.file.withFileExtension (sidecarExtension);
                        if (sidecar.existsAsFile() && isUserLibraryFile (sidecar))
                            sidecar.moveToTrash();
                    }
                    entry.file.moveToTrash();
                }

                if (auto* component = safe.getComponent())
                    component->refresh();
            });
    }

    void BuiltAssetLibraryComponent::createFolderForMode()
    {
        auto root = getWritableModeRoot();
        root.createDirectory();

        auto* alert = new juce::AlertWindow ("New Library Folder",
                                             "Create a folder inside:\n" + root.getFullPathName(),
                                             juce::MessageBoxIconType::QuestionIcon);
        alert->addTextEditor ("folderName", "New Folder", "Folder name");
        alert->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<BuiltAssetLibraryComponent> safe (this);
        alert->enterModalState (true,
                                juce::ModalCallbackFunction::create ([safe, alert, root] (int result)
                                {
                                    if (result != 1)
                                        return;

                                    auto name = juce::File::createLegalFileName (alert->getTextEditorContents ("folderName").trim());
                                    if (name.isEmpty())
                                        name = "New Folder";

                                    auto folder = makeUniqueChildFile (root, name);
                                    if (folder.createDirectory())
                                    {
                                        if (auto* component = safe.getComponent())
                                        {
                                            component->refresh();
                                            component->selectEntryForFile (folder);
                                        }
                                        return;
                                    }

                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                            "Folder Not Created",
                                                                            "Could not create a folder in:\n" + root.getFullPathName());
                                }),
                                true);
    }

    void BuiltAssetLibraryComponent::showSelectedPreview()
    {
        if (selectedRow < 0 || selectedRow >= (int) entries.size())
            return;

        const auto entry = entries[(size_t) selectedRow];
        if (entry.isFolder)
            return;

        if (entry.category == "sounds")
        {
            auditionSoundFile (entry.file);
            return;
        }

        juce::File imageFile = entry.file;
        if (entry.category == "templates")
        {
            imageFile = entry.file.getChildFile ("assets").getChildFile ("thumbnail.png");
            if (! imageFile.existsAsFile())
                imageFile = entry.file.getChildFile ("assets").getChildFile ("background.png");
            if (! imageFile.existsAsFile())
                imageFile = entry.file.getChildFile ("assets").getChildFile ("background-clean.png");
        }

        struct PreviewContent final : public juce::Component
        {
            PreviewContent (StudioMainComponent& ownerToUse,
                            Entry entryToUse,
                            juce::Image imageToUse)
                : owner (ownerToUse),
                  entry (std::move (entryToUse)),
                  image (std::move (imageToUse))
            {
                addButton.setButtonText (entry.category == "templates" ? "Load Template"
                    : (entry.category == "sounds" ? "Import To Mapper" : "Add To Canvas"));
                addButton.setTooltip (entry.category == "templates"
                    ? "Load this full template. This replaces the current project; it does not merge controls."
                    : (entry.category == "sounds" ? "Import this sound into the current Sample Mapper."
                                                   : "Add this asset to the current canvas."));
                addButton.onClick = [this]
                {
                    if (entry.category == "sounds")
                    {
                        juce::Array<juce::File> files;
                        files.add (entry.file);
                        owner.importSampleFiles (files, true, false);
                        owner.setBottomTab (BottomPanel::Page::Samples);
                    }
                    else
                    {
                        owner.addLibraryAssetToCanvas (entry.category, entry.file, entry.frames, entry.vertical);
                    }
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };
                closeButton.onClick = [this]
                {
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };
                for (auto* button : { &addButton, &closeButton })
                {
                    button->getProperties().set ("fontSize", 12.0);
                    addAndMakeVisible (*button);
                }
                setSize (760, 560);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto r = getLocalBounds().reduced (18);
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::Font (19.0f, juce::Font::bold));
                g.drawText (entry.title, r.removeFromTop (28), juce::Justification::centredLeft, true);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (12.0f);
                const auto modeText = entry.category == "templates"
                    ? "Template preview - loading replaces the current project, controls, samples, DSP graph, presets, and artwork."
                    : (entry.category == "sounds" ? "Sound library item - import sends this audio to the Sample Mapper and switches the engine to Sampler."
                                                  : entry.subtitle);
                g.drawText (modeText, r.removeFromTop (22), juce::Justification::centredLeft, true);
                r.removeFromTop (10);
                r.removeFromBottom (46);

                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (r.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

                if (image.isValid())
                {
                    g.drawImageWithin (image, r.getX() + 10, r.getY() + 10,
                                       r.getWidth() - 20, r.getHeight() - 20,
                                       juce::RectanglePlacement::centred);
                }
                else if (entry.category == "sounds")
                {
                    auto info = r.reduced (26);
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.11f));
                    g.fillRoundedRectangle (info.toFloat(), 10.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.drawRoundedRectangle (info.toFloat(), 10.0f, 1.0f);

                    auto top = info.removeFromTop (72);
                    g.setFont (juce::Font (18.0f, juce::Font::bold));
                    g.drawText ("Audio File", top.removeFromTop (28), juce::Justification::centredLeft, true);
                    g.setColour (PatchCraftLookAndFeel::text());
                    g.setFont (13.0f);
                    g.drawText (entry.file.getFullPathName(), top, juce::Justification::centredLeft, true);

                    info.removeFromTop (24);
                    juce::Path path;
                    for (int i = 0; i < info.getWidth(); ++i)
                    {
                        const float phase = (float) i / (float) juce::jmax (1, info.getWidth() - 1);
                        const float sample = std::sin (phase * juce::MathConstants<float>::twoPi * 7.0f)
                                           * (0.25f + 0.70f * std::sin (phase * juce::MathConstants<float>::pi));
                        const auto x = (float) info.getX() + (float) i;
                        const auto y = (float) info.getCentreY() - sample * (float) info.getHeight() * 0.38f;
                        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
                    }
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.strokePath (path, juce::PathStrokeType (2.0f));
                }
                else
                {
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (14.0f);
                    g.drawText ("No preview image found", r, juce::Justification::centred);
                }
            }

            void resized() override
            {
                auto bottom = getLocalBounds().reduced (18).removeFromBottom (34);
                closeButton.setBounds (bottom.removeFromRight (90));
                bottom.removeFromRight (8);
                addButton.setBounds (bottom.removeFromRight (130));
            }

            StudioMainComponent& owner;
            Entry entry;
            juce::Image image;
            juce::TextButton addButton;
            juce::TextButton closeButton { "Close" };
        };

        auto* content = new PreviewContent (owner, entry, juce::ImageFileFormat::loadFrom (imageFile));
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = entry.title + " Preview";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (content);
        options.launchAsync();
    }

    void BuiltAssetLibraryComponent::auditionSoundFile (const juce::File& file)
    {
        if (! file.existsAsFile() || ! isSupportedSoundFile (file))
            return;

        stopSoundPreview();

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            title.setTooltip (error);
            return;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (previewFormatManager.createReaderFor (file));
        if (reader == nullptr)
            return;

        const auto sourceSampleRate = reader->sampleRate;
        previewReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        previewTransport.setSource (previewReaderSource.get(), 0, nullptr, sourceSampleRate);
        previewPlayer.setSource (&previewTransport);
        owner.getAudio().getDeviceManager().addAudioCallback (&previewPlayer);
        previewCallbackActive = true;
        previewTransport.setPosition (0.0);
        previewTransport.start();
    }

    void BuiltAssetLibraryComponent::stopSoundPreview()
    {
        previewTransport.stop();
        previewTransport.setSource (nullptr);
        previewPlayer.setSource (nullptr);
        previewReaderSource.reset();
        if (previewCallbackActive)
        {
            owner.getAudio().getDeviceManager().removeAudioCallback (&previewPlayer);
            previewCallbackActive = false;
        }
    }
}
