#include "LibraryBrowser.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>
#include <map>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static bool entryLooksLikeFx (const LibraryEntry& entry)
        {
            const auto haystack = (entry.category + " " + entry.engineId + " " + entry.tags.joinIntoString (" "))
                .toLowerCase();
            return entry.engineId.equalsIgnoreCase ("fx")
                || entry.engineId.equalsIgnoreCase ("effect")
                || haystack.contains ("fx")
                || haystack.contains ("effect");
        }

        static juce::String filterDisplayName (LibraryBrowser::PackFilter filter)
        {
            switch (filter)
            {
                case LibraryBrowser::PackFilter::EffectsOnly:     return "FX packs";
                case LibraryBrowser::PackFilter::InstrumentsOnly: return "instruments";
                case LibraryBrowser::PackFilter::Any:             return "packs";
            }
            return "packs";
        }
    }

    // ---- LibraryItemComponent ------------------------------------------------
    LibraryItemComponent::LibraryItemComponent (const LibraryEntry& e)
        : entry (e), thumbnail (e.thumbnailImage)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    LibraryItemComponent::~LibraryItemComponent() = default;

    void LibraryItemComponent::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float corner = 8.0f;

        g.setColour (isHovered ? juce::Colour (0xff1a202c) : juce::Colour (0xff10131a));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (isHovered ? PatchCraftLookAndFeel::accent().withAlpha (0.88f)
                                : juce::Colour (0xff2a303b));
        g.drawRoundedRectangle (bounds.reduced (0.7f), corner, isHovered ? 1.5f : 1.0f);

        auto thumbArea = bounds.reduced (9.0f, 9.0f);
        thumbArea = thumbArea.withHeight (128.0f);

        if (thumbnail.isValid())
        {
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle (thumbArea, 6.0f);
            g.reduceClipRegion (clip);
            g.drawImage (thumbnail, thumbArea, juce::RectanglePlacement::fillDestination);
            juce::ColourGradient shade (juce::Colours::transparentBlack,
                                        thumbArea.getCentreX(), thumbArea.getY(),
                                        juce::Colour (0xaa000000),
                                        thumbArea.getCentreX(), thumbArea.getBottom(),
                                        false);
            g.setGradientFill (shade);
            g.fillRect (thumbArea);
            g.restoreState();
        }
        else
        {
            g.setColour (juce::Colour (0xff1e2430));
            g.fillRoundedRectangle (thumbArea, 6.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.30f));
            g.drawRoundedRectangle (thumbArea.reduced (10.0f), 5.0f, 1.2f);
        }

        auto textArea = bounds.reduced (8.0f, 8.0f);
        textArea = textArea.withTop (thumbArea.getBottom() + 10.0f);

        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (14.5f).withStyle ("bold"));
        g.drawFittedText (entry.instrumentName,
                          textArea.toNearestInt().removeFromTop (38),
                          juce::Justification::centredLeft, 2, 0.88f);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        auto creator = "by " + entry.creator;
        g.drawText (creator, textArea.withY (textArea.getY() + 38.0f).withHeight (17.0f),
                    juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.86f));
        g.setFont (juce::FontOptions (10.5f));
        if (entry.description.isNotEmpty())
            g.drawFittedText (entry.description,
                              textArea.withY (textArea.getY() + 60.0f)
                                      .withHeight (42.0f)
                                      .toNearestInt(),
                              juce::Justification::topLeft, 2, 0.86f);

        auto badge = bounds.reduced (9.0f).withTop (bounds.getBottom() - 34.0f).withHeight (22.0f);
        auto loadBadge = badge.removeFromRight (60.0f);
        badge.removeFromRight (7.0f);
        auto engineBadge = badge.removeFromRight (56.0f);
        badge = badge.withWidth (juce::jmin (88.0f, badge.getWidth()));

        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.22f));
        g.fillRoundedRectangle (badge, 5.0f);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (entry.category, badge, juce::Justification::centred);

        g.setColour (juce::Colour (0xff222b38));
        g.fillRoundedRectangle (engineBadge, 5.0f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText (entry.engineId.isNotEmpty() ? entry.engineId.toUpperCase() : "PACK",
                    engineBadge, juce::Justification::centred, true);

        g.setColour (isHovered ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::accent().withAlpha (0.78f));
        g.fillRoundedRectangle (loadBadge, 5.0f);
        g.setColour (juce::Colour (0xff080a0d));
        g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
        g.drawText ("LOAD", loadBadge, juce::Justification::centred, true);
    }

    void LibraryItemComponent::mouseUp (const juce::MouseEvent&)
    {
        if (onLoadClicked)
            onLoadClicked();
    }

    void LibraryItemComponent::mouseEnter (const juce::MouseEvent&)
    {
        isHovered = true;
        repaint();
    }

    void LibraryItemComponent::mouseExit (const juce::MouseEvent&)
    {
        isHovered = false;
        repaint();
    }

    // ---- LibraryFolderComponent ---------------------------------------------
    LibraryFolderComponent::LibraryFolderComponent (juce::File folderToUse, int itemCountToUse)
        : folder (std::move (folderToUse)), itemCount (itemCountToUse)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Double-click to open " + folder.getFileName());
    }

    void LibraryFolderComponent::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (isHovered ? juce::Colour (0xff1a2230) : juce::Colour (0xff10131a));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (isHovered ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::borderSoft());
        g.drawRoundedRectangle (bounds.reduced (0.7f), 8.0f, isHovered ? 1.6f : 1.0f);

        auto icon = bounds.reduced (18.0f).withHeight (116.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.14f));
        g.fillRoundedRectangle (icon, 10.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
        auto folderBody = icon.reduced (18.0f, 28.0f);
        auto tab = folderBody.withHeight (18.0f).withWidth (folderBody.getWidth() * 0.45f);
        g.fillRoundedRectangle (tab, 4.0f);
        folderBody = folderBody.withTrimmedTop (10.0f);
        g.fillRoundedRectangle (folderBody, 7.0f);

        auto text = bounds.reduced (12.0f).withTop (icon.getBottom() + 12.0f);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (15.0f).withStyle ("bold"));
        g.drawFittedText (folder.getFileName(), text.toNearestInt().removeFromTop (42),
                          juce::Justification::centredLeft, 2, 0.88f);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::String (itemCount) + " pack" + (itemCount == 1 ? "" : "s"),
                    text.withY (text.getY() + 48.0f).withHeight (20.0f),
                    juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
        g.drawText ("DOUBLE-CLICK TO OPEN",
                    bounds.reduced (12.0f).withTop (bounds.getBottom() - 34.0f).withHeight (22.0f),
                    juce::Justification::centredLeft, true);
    }

    void LibraryFolderComponent::mouseDoubleClick (const juce::MouseEvent&)
    {
        if (onOpenFolder)
            onOpenFolder();
    }

    void LibraryFolderComponent::mouseEnter (const juce::MouseEvent&)
    {
        isHovered = true;
        repaint();
    }

    void LibraryFolderComponent::mouseExit (const juce::MouseEvent&)
    {
        isHovered = false;
        repaint();
    }

    // ---- LibraryBrowser -------------------------------------------------------
    LibraryBrowser::LibraryBrowser (LibraryScanner& s, PackFilter filter)
        : scanner (s), packFilter (filter)
    {
        // Search box
        searchBox = std::make_unique<juce::TextEditor>();
        searchBox->setTextToShowWhenEmpty ("Search " + filterDisplayName (packFilter) + "...",
                                           PatchCraftLookAndFeel::textDim());
        searchBox->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1a1c23));
        searchBox->setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
        searchBox->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2a2d35));
        searchBox->addListener (this);
        addAndMakeVisible (*searchBox);

        // Category button
        categoryButton = std::make_unique<juce::TextButton> (currentCategory);
        categoryButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d35));
        categoryButton->setColour (juce::TextButton::textColourOffId, PatchCraftLookAndFeel::text());
        categoryButton->onClick = [this] { showCategoryMenu(); };
        addAndMakeVisible (*categoryButton);

        refreshButton = std::make_unique<juce::TextButton> ("Refresh");
        refreshButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202631));
        refreshButton->setColour (juce::TextButton::textColourOffId, PatchCraftLookAndFeel::text());
        refreshButton->setTooltip ("Rescan PatchCraft library folders for instruments and patches.");
        refreshButton->onClick = [this]
        {
            scanner.scanLibrary();
            applyFilters();
        };
        addAndMakeVisible (*refreshButton);

        upButton = std::make_unique<juce::TextButton> ("Up");
        upButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202631));
        upButton->setColour (juce::TextButton::textColourOffId, PatchCraftLookAndFeel::text());
        upButton->setTooltip ("Return to library folders.");
        upButton->onClick = [this]
        {
            currentFolder = {};
            applyFilters();
        };
        addAndMakeVisible (*upButton);
        
        // Close button - labelled "Close" instead of bare X so users have a
        // discoverable exit even on a confused first encounter, and styled
        // bigger/redder than the toolbar buttons so it stands out.
        closeButton = std::make_unique<juce::TextButton> ("Close \xC3\x97");
        closeButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5a2024));
        closeButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        closeButton->setTooltip ("Close the library browser (or press Escape)");
        closeButton->onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (*closeButton);

        setWantsKeyboardFocus (true);

        // Grid container with viewport
        gridContainer = std::make_unique<juce::Component>();
        viewport = std::make_unique<juce::Viewport>();
        viewport->setViewedComponent (gridContainer.get(), false);
        viewport->setScrollBarsShown (true, false);
        addAndMakeVisible (*viewport);

        scanner.addListener (this);
        scanner.scanLibrary();
        applyFilters();
    }

    LibraryBrowser::~LibraryBrowser()
    {
        scanner.removeListener (this);
    }

    void LibraryBrowser::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff090b10));

        auto header = getLocalBounds().reduced (14).removeFromTop (34);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (18.5f).withStyle ("bold"));
        g.drawText ("PatchCraft Library", header.removeFromLeft (205), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::String (filteredEntries.size()) + " discovered "
                    + filterDisplayName (packFilter), header, juce::Justification::centredLeft, true);

        if (filteredEntries.isEmpty())
        {
            auto area = getLocalBounds().reduced (28).withTrimmedTop (112);
            g.setColour (PatchCraftLookAndFeel::textBright());
            g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
            g.drawText ("No PatchCraft " + filterDisplayName (packFilter) + " found",
                        area.removeFromTop (30), juce::Justification::centred);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText ("Use Refresh after exporting or copying compatible .patchcraft folders into Documents/PatchCraft/Library, Instruments, Packs, or FactoryDemos.",
                              area.removeFromTop (56), juce::Justification::centred, 3);
        }
    }

    void LibraryBrowser::resized()
    {
        auto bounds = getLocalBounds().reduced (12);

        bounds.removeFromTop (36);

        auto topBar = bounds.removeFromTop (42);
        closeButton->setBounds (topBar.removeFromRight (90).reduced (0, 5));
        topBar.removeFromRight (10);
        refreshButton->setBounds (topBar.removeFromRight (84).reduced (0, 5));
        topBar.removeFromRight (10);
        upButton->setBounds (topBar.removeFromRight (62).reduced (0, 5));
        upButton->setVisible (currentFolder.isDirectory());
        topBar.removeFromRight (10);
        categoryButton->setBounds (topBar.removeFromRight (110).reduced (0, 5));
        topBar.removeFromRight (10);
        searchBox->setBounds (topBar.reduced (0, 5));

        bounds.removeFromTop (8);

        // Grid viewport
        viewport->setBounds (bounds);
        refreshGrid();
    }

    bool LibraryBrowser::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        return false;
    }

    void LibraryBrowser::textEditorTextChanged (juce::TextEditor&)
    {
        applyFilters();
    }

    void LibraryBrowser::libraryChanged()
    {
        applyFilters();
    }

    void LibraryBrowser::refreshGrid()
    {
        gridContainer->removeAllChildren();
        gridContainer->setSize (viewport->getMaximumVisibleWidth(), 0);

        const int itemWidth = 214;
        const int itemHeight = 286;
        const int padding = 16;
        const int cols = std::max (1, viewport->getWidth() / (itemWidth + padding));

        int x = padding;
        int y = padding;
        int col = 0;

        auto addGridComponent = [&] (std::unique_ptr<juce::Component> item)
        {
            item->setBounds (x, y, itemWidth, itemHeight);
            gridContainer->addAndMakeVisible (item.release());

            col++;
            x += itemWidth + padding;
            if (col >= cols)
            {
                col = 0;
                x = padding;
                y += itemHeight + padding;
            }
        };

        if (! currentFolder.isDirectory())
        {
            std::map<juce::String, std::pair<juce::File, int>> folders;
            for (auto& entry : filteredEntries)
            {
                const auto parent = entry.folder.getParentDirectory();
                const auto key = parent.getFullPathName().replaceCharacter ('\\', '/').toLowerCase();
                auto& record = folders[key];
                record.first = parent;
                ++record.second;
            }

            std::vector<std::pair<juce::File, int>> sortedFolders;
            for (const auto& item : folders)
                sortedFolders.push_back (item.second);
            std::sort (sortedFolders.begin(), sortedFolders.end(),
                [] (const auto& a, const auto& b)
                {
                    return a.first.getFileName().compareIgnoreCase (b.first.getFileName()) < 0;
                });

            for (const auto& folder : sortedFolders)
            {
                auto item = std::make_unique<LibraryFolderComponent> (folder.first, folder.second);
                item->onOpenFolder = [this, folder]
                {
                    currentFolder = folder.first;
                    refreshGrid();
                };
                addGridComponent (std::move (item));
            }
        }
        else
        {
            for (auto& entry : filteredEntries)
            {
                if (entry.folder.getParentDirectory() != currentFolder)
                    continue;

                auto item = std::make_unique<LibraryItemComponent> (entry);
                item->onLoadClicked = [this, entry] {
                    if (onPackSelected)
                        onPackSelected (entry.folder);
                };
                addGridComponent (std::move (item));
            }
        }

        gridContainer->setSize (gridContainer->getWidth(), y + itemHeight + padding);
    }

    void LibraryBrowser::applyFilters()
    {
        rebuildCategories();
        auto query = searchBox->getText().trim();
        filteredEntries.clear();

        auto source = query.isEmpty() ? scanner.getEntries() : scanner.search (query);
        for (auto& entry : source)
        {
            if (entryMatchesPackFilter (entry) && entryMatchesCategory (entry))
                filteredEntries.add (entry);
        }

        refreshGrid();
    }

    bool LibraryBrowser::entryMatchesCategory (const LibraryEntry& entry) const
    {
        if (currentCategory == "All")
            return true;

        const auto haystack = (entry.category + " " + entry.engineId + " " + entry.tags.joinIntoString (" "))
            .toLowerCase();

        if (currentCategory == "Instruments")
            return ! entry.engineId.equalsIgnoreCase ("fx") && ! haystack.contains ("fx plugin");
        if (currentCategory == "Synth")
            return entry.engineId.equalsIgnoreCase ("synth") || haystack.contains ("synth");
        if (currentCategory == "Samples")
            return entry.engineId.equalsIgnoreCase ("sample") || haystack.contains ("sample") || haystack.contains ("sampler");
        if (currentCategory == "Drums")
            return haystack.contains ("drum") || haystack.contains ("kit") || haystack.contains ("pad");
        if (currentCategory == "FX")
            return entryLooksLikeFx (entry);
        if (currentCategory == "Factory Demos")
            return haystack.contains ("factory-demo") || entry.folder.getFullPathName().containsIgnoreCase ("FactoryDemos");

        return entry.category.equalsIgnoreCase (currentCategory);
    }

    bool LibraryBrowser::entryMatchesPackFilter (const LibraryEntry& entry) const
    {
        if (packFilter == PackFilter::EffectsOnly)
            return entryLooksLikeFx (entry);
        if (packFilter == PackFilter::InstrumentsOnly)
            return ! entryLooksLikeFx (entry);
        return true;
    }

    void LibraryBrowser::rebuildCategories()
    {
        juce::StringArray next = packFilter == PackFilter::EffectsOnly
            ? juce::StringArray { "All", "FX", "Factory Demos" }
            : packFilter == PackFilter::InstrumentsOnly
                ? juce::StringArray { "All", "Instruments", "Synth", "Samples", "Drums", "Factory Demos" }
                : juce::StringArray { "All", "Instruments", "Synth", "Samples", "Drums", "FX", "Factory Demos" };
        for (const auto& entry : scanner.getEntries())
            if (entryMatchesPackFilter (entry) && entry.category.isNotEmpty())
                next.addIfNotAlreadyThere (entry.category);

        next.sort (true);
        next.removeString ("All");
        next.insert (0, "All");
        categories = next;
        if (! categories.contains (currentCategory))
            currentCategory = "All";
        categoryButton->setButtonText (currentCategory);
    }

    void LibraryBrowser::showCategoryMenu()
    {
        juce::PopupMenu menu;
        for (auto& cat : categories)
        {
            menu.addItem (cat, true, cat == currentCategory, [this, cat] {
                currentCategory = cat;
                currentFolder = {};
                categoryButton->setButtonText (cat);
                applyFilters();
            });
        }
        menu.showMenuAsync (juce::PopupMenu::Options()
                               .withTargetComponent (categoryButton.get()));
    }

} // namespace patchcraft
