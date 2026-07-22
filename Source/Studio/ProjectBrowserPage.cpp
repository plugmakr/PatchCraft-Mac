#include "ProjectBrowserPage.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>
#include <map>

namespace patchcraft
{
    namespace
    {
        static juce::String normalisePath (const juce::File& file)
        {
            return file.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
        }

        static juce::String readJsonString (const juce::File& jsonFile, const juce::String& key)
        {
            if (! jsonFile.existsAsFile())
                return {};

            auto parsed = juce::JSON::parse (jsonFile);
            if (auto* object = parsed.getDynamicObject())
                return object->getProperty (key).toString();
            return {};
        }

        static juce::String formatModified (const juce::File& folder)
        {
            const auto time = folder.getLastModificationTime();
            return time.toString (true, true, true, true);
        }
    }

    ProjectBrowserPage::ProjectBrowserPage (StudioMainComponent& o) : owner (o)
    {
        title.setText ("Project Browser", juce::dontSendNotification);
        title.setFont (juce::Font (22.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (title);

        subtitle.setText ("Open recent work, factory demos, templates, or browse to a project folder.", juce::dontSendNotification);
        subtitle.setFont (juce::Font (12.0f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        browseButton.onClick = [this] { browseForProject(); };
        openButton.onClick = [this] { openSelected(); };
        refreshButton.onClick = [this] { refresh(); };
        for (auto* button : { &browseButton, &openButton, &refreshButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }

        list.setRowHeight (68);
        list.setOutlineThickness (0);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (list);

        refresh();
    }

    void ProjectBrowserPage::refresh()
    {
        entries.clear();
        seenPaths.clear();

        for (const auto& path : owner.getRecentProjectPaths())
            addProjectFolder (juce::File (path), true);

        const auto cwd = juce::File::getCurrentWorkingDirectory();
        const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
        const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        scanRoot (cwd.getChildFile ("FactoryDemos"), 1);
        scanRoot (cwd.getChildFile ("Library").getChildFile ("Templates"), 2);
        scanRoot (appDir.getChildFile ("FactoryDemos"), 1);
        scanRoot (appDir.getChildFile ("Library").getChildFile ("Templates"), 2);
        scanRoot (docs, 2);

        const auto appFactoryRoot = normalisePath (appDir.getChildFile ("FactoryDemos"));
        std::vector<Entry> uniqueEntries;
        std::map<juce::String, size_t> runtimeDemoIndex;
        auto demoPriority = [&appFactoryRoot] (const Entry& entry)
        {
            const auto path = normalisePath (entry.folder);
            if (path == appFactoryRoot || path.startsWith (appFactoryRoot + "/"))
                return 3;
            return entry.recent ? 2 : 1;
        };

        for (auto& entry : entries)
        {
            if (entry.type != "Runtime Pack / Demo")
            {
                uniqueEntries.push_back (std::move (entry));
                continue;
            }

            const auto identity = entry.title.trim().toLowerCase() + "|" + entry.engine.trim().toLowerCase();
            const auto found = runtimeDemoIndex.find (identity);
            if (found == runtimeDemoIndex.end())
            {
                runtimeDemoIndex[identity] = uniqueEntries.size();
                uniqueEntries.push_back (std::move (entry));
            }
            else if (demoPriority (entry) > demoPriority (uniqueEntries[found->second]))
            {
                uniqueEntries[found->second] = std::move (entry);
            }
        }
        entries = std::move (uniqueEntries);

        std::stable_sort (entries.begin(), entries.end(),
            [] (const Entry& a, const Entry& b)
            {
                if (a.recent != b.recent)
                    return a.recent;
                return a.title.compareIgnoreCase (b.title) < 0;
            });

        list.updateContent();
        list.repaint();
    }

    void ProjectBrowserPage::addProjectFolder (juce::File folder, bool recent)
    {
        if (! folder.isDirectory())
            return;

        const bool authoring = folder.getChildFile ("project.json").existsAsFile();
        const bool runtime = folder.getChildFile ("manifest.json").existsAsFile();
        if (! authoring && ! runtime)
            return;

        const auto key = normalisePath (folder);
        if (seenPaths.contains (key))
            return;
        seenPaths.add (key);

        Entry entry;
        entry.folder = folder;
        entry.recent = recent;
        entry.type = authoring ? "Authoring Project" : "Runtime Pack / Demo";
        entry.title = folder.getFileNameWithoutExtension();
        entry.engine = readJsonString (folder.getChildFile ("manifest.json"), "engine");
        auto displayName = readJsonString (folder.getChildFile ("manifest.json"), "instrumentName");
        if (displayName.isEmpty())
            displayName = readJsonString (folder.getChildFile ("project.json"), "instrumentName");
        if (displayName.isNotEmpty())
            entry.title = displayName;
        if (entry.engine.isEmpty())
            entry.engine = readJsonString (folder.getChildFile ("project.json"), "engine");
        entry.location = folder.getFullPathName();
        entry.modified = formatModified (folder);
        entries.push_back (std::move (entry));
    }

    void ProjectBrowserPage::scanRoot (const juce::File& root, int depth)
    {
        if (! root.isDirectory())
            return;

        addProjectFolder (root, false);
        if (depth <= 0)
            return;

        for (const auto& child : root.findChildFiles (juce::File::findDirectories, false, "*"))
        {
            addProjectFolder (child, false);
            if (! child.getFileName().endsWithIgnoreCase (".patchcraft")
                && ! child.getFileName().endsWithIgnoreCase (".patchcraftproject"))
                scanRoot (child, depth - 1);
        }
    }

    int ProjectBrowserPage::getNumRows()
    {
        return (int) entries.size();
    }

    void ProjectBrowserPage::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= (int) entries.size())
            return;

        const auto& entry = entries[(size_t) row];
        auto r = juce::Rectangle<int> (0, 0, width, height).reduced (8, 5);
        g.setColour (selected ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (r.toFloat(), 7.0f);
        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 7.0f, selected ? 1.6f : 1.0f);

        auto badge = r.removeFromLeft (92).reduced (8, 10);
        g.setColour ((entry.recent ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim()).withAlpha (0.18f));
        g.fillRoundedRectangle (badge.toFloat(), 5.0f);
        g.setColour (entry.recent ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawFittedText (entry.recent ? "RECENT" : entry.type.upToFirstOccurrenceOf (" ", false, false).toUpperCase(),
                          badge, juce::Justification::centred, 1);

        r.removeFromLeft (4);
        auto top = r.removeFromTop (24);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (entry.title, top.removeFromLeft (juce::jmax (120, top.getWidth() - 220)),
                    juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (entry.engine.isNotEmpty() ? entry.engine.toUpperCase() : "PATCHCRAFT",
                    top, juce::Justification::centredRight, true);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f));
        g.drawText (entry.type + "  |  Modified " + entry.modified,
                    r.removeFromTop (18), juce::Justification::centredLeft, true);
        g.drawText (entry.location, r, juce::Justification::centredLeft, true);
    }

    void ProjectBrowserPage::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        list.selectRow (row);
        openSelected();
    }

    void ProjectBrowserPage::selectedRowsChanged (int)
    {
        openButton.setEnabled (list.getSelectedRow() >= 0);
    }

    void ProjectBrowserPage::openSelected()
    {
        const int row = list.getSelectedRow();
        if (row < 0 || row >= (int) entries.size())
            return;
        owner.openProjectFolder (entries[(size_t) row].folder);
    }

    void ProjectBrowserPage::browseForProject()
    {
        chooser = std::make_shared<juce::FileChooser> (
            "Open PatchCraft project or demo",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.patchcraftproject;*.patchcraft");

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser = chooser] (const juce::FileChooser& fc)
            {
                const auto folder = fc.getResult();
                if (folder != juce::File())
                    owner.openProjectFolder (folder);
            });
    }

    void ProjectBrowserPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        auto frame = getLocalBounds().reduced (12).toFloat();
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.55f));
        g.drawRoundedRectangle (frame, 8.0f, 1.0f);
    }

    void ProjectBrowserPage::resized()
    {
        auto r = getLocalBounds().reduced (20, 16);
        auto header = r.removeFromTop (58);
        auto actions = header.removeFromRight (300);
        refreshButton.setBounds (actions.removeFromRight (82).reduced (3, 12));
        openButton.setBounds (actions.removeFromRight (112).reduced (3, 12));
        browseButton.setBounds (actions.removeFromRight (94).reduced (3, 12));
        title.setBounds (header.removeFromTop (28));
        subtitle.setBounds (header);
        r.removeFromTop (8);
        list.setBounds (r);
    }
}
