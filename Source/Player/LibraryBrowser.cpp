#include "LibraryBrowser.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
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

        // Background
        g.setColour (isHovered ? juce::Colour (0xff1a1c23) : juce::Colour (0xff12141a));
        g.fillRoundedRectangle (bounds, corner);

        // Border
        g.setColour (isHovered ? PatchCraftLookAndFeel::accent() : juce::Colour (0xff2a2d35));
        g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 1.0f);

        // Thumbnail area
        auto thumbArea = bounds.reduced (8.0f, 8.0f);
        thumbArea = thumbArea.withHeight (thumbArea.getHeight() * 0.6f);

        if (thumbnail.isValid())
        {
            g.drawImage (thumbnail, thumbArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
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
        g.setFont (juce::FontOptions (14.0f));
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

        // Category badge
        auto badge = textArea.withY (textArea.getY() + 42).withHeight (18).withWidth (80);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.3f));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (entry.category, badge, juce::Justification::centred);
    }

    void LibraryItemComponent::mouseUp (const juce::MouseEvent&)
    {
        if (onLoadClicked)
            onLoadClicked();
    }

    // ---- LibraryBrowser -------------------------------------------------------
    LibraryBrowser::LibraryBrowser (LibraryScanner& s)
        : scanner (s)
    {
        // Search box
        searchBox = std::make_unique<juce::TextEditor>();
        searchBox->setTextToShowWhenEmpty ("Search instruments...", PatchCraftLookAndFeel::textDim());
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
        
        // Close button
        closeButton = std::make_unique<juce::TextButton> ("X");
        closeButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d35));
        closeButton->setColour (juce::TextButton::textColourOffId, PatchCraftLookAndFeel::text());
        closeButton->onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (*closeButton);

        // Grid container with viewport
        gridContainer = std::make_unique<juce::Component>();
        viewport = std::make_unique<juce::Viewport>();
        viewport->setViewedComponent (gridContainer.get(), false);
        viewport->setScrollBarsShown (true, false);
        addAndMakeVisible (*viewport);

        scanner.addListener (this);
        refreshGrid();
    }

    LibraryBrowser::~LibraryBrowser()
    {
        scanner.removeListener (this);
    }

    void LibraryBrowser::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0a0b12));
    }

    void LibraryBrowser::resized()
    {
        auto bounds = getLocalBounds().reduced (10);

        // Top bar: search + category + close
        auto topBar = bounds.removeFromTop (40);
        closeButton->setBounds (topBar.removeFromRight (40).reduced (0, 5));
        topBar.removeFromRight (10);
        categoryButton->setBounds (topBar.removeFromRight (110).reduced (0, 5));
        topBar.removeFromRight (10);
        searchBox->setBounds (topBar.reduced (0, 5));

        // Grid viewport
        viewport->setBounds (bounds);
        refreshGrid();
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
        const int itemHeight = 220;
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
        auto query = searchBox->getText().trim();

        if (currentCategory == "All" && query.isEmpty())
        {
            filteredEntries = scanner.getEntries();
        }
        else if (currentCategory == "All")
        {
            filteredEntries = scanner.search (query);
        }
        else if (query.isEmpty())
        {
            filteredEntries = scanner.getEntriesByCategory (currentCategory);
        }
        else
        {
            auto byCategory = scanner.getEntriesByCategory (currentCategory);
            auto bySearch = scanner.search (query);

            // Manual intersection since LibraryEntry doesn't have operator==
            for (auto& catEntry : byCategory)
            {
                for (auto& searchEntry : bySearch)
                {
                    if (catEntry.folder == searchEntry.folder)
                    {
                        filteredEntries.add (catEntry);
                        break;
                    }
                }
            }
        }

        refreshGrid();
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
