#include "LibraryBrowser.h"
#include "PatchCraftLookAndFeel.h"

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
        const float corner = 10.0f;

        // Background
        g.setColour (isHovered ? juce::Colour (0xff1b202b) : juce::Colour (0xff12141a));
        g.fillRoundedRectangle (bounds, corner);

        // Border
        g.setColour (isHovered ? PatchCraftLookAndFeel::accent() : juce::Colour (0xff2a2d35));
        g.drawRoundedRectangle (bounds.reduced (1.0f), corner, isHovered ? 1.6f : 1.0f);

        // Thumbnail area
        auto thumbArea = bounds.reduced (8.0f, 8.0f);
        thumbArea = thumbArea.withHeight (104.0f);

        if (thumbnail.isValid())
        {
            g.saveState();
            g.reduceClipRegion (thumbArea.toNearestInt());
            g.drawImage (thumbnail, thumbArea, juce::RectanglePlacement::fillDestination);
            g.restoreState();
            g.setColour (juce::Colour (0xaa000000));
            g.fillRoundedRectangle (thumbArea.withY (thumbArea.getBottom() - 24.0f).withHeight (24.0f), 4.0f);
        }
        else
        {
            // Placeholder
            g.setColour (juce::Colour (0xff1e2028));
            g.fillRoundedRectangle (thumbArea, 4.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("No Image", thumbArea, juce::Justification::centred);
        }

        // Text info area
        auto textArea = bounds.reduced (8.0f, 8.0f);
        textArea = textArea.withTop (thumbArea.getBottom() + 8.0f);

        // Title
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
        auto title = entry.instrumentName;
        if (title.length() > 25) title = title.substring (0, 22) + "...";
        g.drawText (title, textArea.withHeight (20), juce::Justification::centredLeft, true);

        // Creator
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (11.0f));
        auto creator = "by " + entry.creator;
        if (creator.length() > 30) creator = creator.substring (0, 27) + "...";
        g.drawText (creator, textArea.withY (textArea.getY() + 22).withHeight (16),
                   juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.86f));
        g.setFont (juce::FontOptions (10.5f));
        auto description = entry.description;
        if (description.length() > 58) description = description.substring (0, 55) + "...";
        if (description.isNotEmpty())
            g.drawText (description, textArea.withY (textArea.getY() + 41).withHeight (16),
                        juce::Justification::centredLeft, true);

        // Category badge
        auto badge = textArea.withY (bounds.getBottom() - 32.0f).withHeight (20.0f).withWidth (82.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.3f));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (entry.category, badge, juce::Justification::centred);

        auto engineBadge = badge.translated (88.0f, 0.0f).withWidth (58.0f);
        g.setColour (juce::Colour (0xff263140));
        g.fillRoundedRectangle (engineBadge, 4.0f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText (entry.engineId.isNotEmpty() ? entry.engineId.toUpperCase() : "PACK",
                    engineBadge, juce::Justification::centred, true);

        auto loadBadge = bounds.reduced (8.0f).withTop (bounds.getBottom() - 34.0f).withLeft (bounds.getRight() - 58.0f);
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
        g.fillAll (juce::Colour (0xff0a0b12));

        auto header = getLocalBounds().reduced (12).removeFromTop (30);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (17.0f).withStyle ("bold"));
        g.drawText ("PatchCraft Library", header.removeFromLeft (190), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::String (filteredEntries.size()) + " compatible "
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
        auto bounds = getLocalBounds().reduced (10);

        bounds.removeFromTop (32);

        // Top bar: search + category + close
        auto topBar = bounds.removeFromTop (40);
        closeButton->setBounds (topBar.removeFromRight (90).reduced (0, 5));
        topBar.removeFromRight (10);
        refreshButton->setBounds (topBar.removeFromRight (84).reduced (0, 5));
        topBar.removeFromRight (10);
        categoryButton->setBounds (topBar.removeFromRight (110).reduced (0, 5));
        topBar.removeFromRight (10);
        searchBox->setBounds (topBar.reduced (0, 5));

        bounds.removeFromTop (6);

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

        const int itemWidth = 200;
        const int itemHeight = 240;
        const int padding = 16;
        const int cols = std::max (1, viewport->getWidth() / (itemWidth + padding));

        int x = padding;
        int y = padding;
        int col = 0;

        for (auto& entry : filteredEntries)
        {
            auto item = std::make_unique<LibraryItemComponent> (entry);
            item->setBounds (x, y, itemWidth, itemHeight);
            item->onLoadClicked = [this, entry] {
                if (onPackSelected)
                    onPackSelected (entry.folder);
            };
            gridContainer->addAndMakeVisible (item.release());

            col++;
            x += itemWidth + padding;
            if (col >= cols)
            {
                col = 0;
                x = padding;
                y += itemHeight + padding;
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
                categoryButton->setButtonText (cat);
                applyFilters();
            });
        }
        menu.showMenuAsync (juce::PopupMenu::Options()
                               .withTargetComponent (categoryButton.get()));
    }

} // namespace patchcraft
