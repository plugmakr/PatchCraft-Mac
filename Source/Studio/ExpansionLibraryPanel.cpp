#include "ExpansionLibraryPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    static constexpr int kCardW = 168;
    static constexpr int kCardH = 192;
    static constexpr int kGap   = 10;
    static constexpr int kHeaderHeight = 36;

    ExpansionLibraryPanel::ExpansionLibraryPanel (StudioMainComponent& o) : owner (o)
    {
        header.setText ("Expansion Packs", juce::dontSendNotification);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        header.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (header);

        newExpansionButton.getProperties().set ("smallButton", true);
        newExpansionButton.setTooltip ("Create a new expansion pack in this project.");
        newExpansionButton.onClick = [this]
        {
            // Pop a name dialog, create on confirm.
            auto* alert = new juce::AlertWindow ("New Expansion Pack",
                                                  "Name the new expansion pack:",
                                                  juce::MessageBoxIconType::NoIcon);
            alert->addTextEditor ("name", "New Expansion", "Pack Name");
            alert->addButton ("Create", 1);
            alert->addButton ("Cancel", 0);
            alert->enterModalState (true,
                juce::ModalCallbackFunction::create ([this, alert] (int result)
                {
                    const auto name = alert->getTextEditorContents ("name").trim();
                    std::unique_ptr<juce::AlertWindow> owned (alert);
                    if (result != 1 || name.isEmpty())
                        return;
                    auto& expansion = owner.getProject().ensureExpansion (name);
                    selectedExpansionId = expansion.id;
                    owner.getProject().notifyChanged();
                    refresh();
                }), true);
        };
        addAndMakeVisible (newExpansionButton);

        refreshButton.getProperties().set ("smallButton", true);
        refreshButton.onClick = [this] { refresh(); };
        addAndMakeVisible (refreshButton);

        popOutBtn.setTooltip ("Pop the expansion pack library into a free-floating window. Click again to dock.");
        popOutBtn.getProperties().set ("smallButton", true);
        popOutBtn.onClick = [this] { owner.togglePanelFloat (this, "Expansion Packs"); };
        addAndMakeVisible (popOutBtn);

        refresh();
    }

    void ExpansionLibraryPanel::refresh()
    {
        rebuildCards();
        repaint();
    }

    void ExpansionLibraryPanel::rebuildCards()
    {
        cards = computeCardLayouts();
    }

    std::vector<ExpansionLibraryPanel::CardLayout> ExpansionLibraryPanel::computeCardLayouts() const
    {
        std::vector<CardLayout> out;
        const auto& expansions = owner.getProject().getExpansions();
        if (contentArea.isEmpty()) return out;

        const int columns = juce::jmax (1, contentArea.getWidth() / (kCardW + kGap));
        for (size_t i = 0; i < expansions.size(); ++i)
        {
            const int row = (int) i / columns;
            const int col = (int) i % columns;
            const int x = contentArea.getX() + col * (kCardW + kGap);
            const int y = contentArea.getY() + row * (kCardH + kGap);
            out.push_back ({ juce::Rectangle<int> (x, y, kCardW, kCardH), (int) i });
        }
        return out;
    }

    void ExpansionLibraryPanel::resized()
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (kHeaderHeight);
        popOutBtn.setBounds (top.removeFromRight (38).reduced (2));
        top.removeFromRight (4);
        header.setBounds (top.removeFromLeft (180));
        newExpansionButton.setBounds (top.removeFromLeft (60).reduced (2));
        top.removeFromLeft (4);
        refreshButton.setBounds (top.removeFromLeft (74).reduced (2));
        contentArea = r.reduced (0, 4);
        rebuildCards();
    }

    void ExpansionLibraryPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());

        if (cards.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (12.0f);
            g.drawFittedText ("No expansion packs yet.\nClick + New to create one, or save a patch to auto-create one.",
                              contentArea, juce::Justification::centred, 3);
            return;
        }

        for (const auto& c : cards)
            paintCard (g, c);
    }

    void ExpansionLibraryPanel::paintCard (juce::Graphics& g, const CardLayout& c)
    {
        const auto& expansions = owner.getProject().getExpansions();
        if (c.expansionIndex < 0 || c.expansionIndex >= (int) expansions.size())
            return;

        const auto& exp = expansions[(size_t) c.expansionIndex];
        const bool selected = exp.id == selectedExpansionId;
        const auto rect = c.bounds.toFloat();

        // Card chrome.
        g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.20f)
                              : juce::Colour (0xff181b21));
        g.fillRoundedRectangle (rect, 8.0f);
        g.setColour (selected ? PatchCraftLookAndFeel::accent()
                              : PatchCraftLookAndFeel::border().brighter (0.30f));
        g.drawRoundedRectangle (rect.reduced (0.5f), 8.0f, selected ? 1.6f : 1.0f);

        // Thumbnail area (top 60% of card).
        auto inner = c.bounds.reduced (8);
        const int thumbH = (inner.getHeight() * 60) / 100;
        auto thumb = inner.removeFromTop (thumbH);
        g.setColour (juce::Colour (0xff0e1116));
        g.fillRoundedRectangle (thumb.toFloat(), 5.0f);

        // Try to load artwork; otherwise draw a colour stamp + initial.
        juce::Image art;
        if (exp.artworkPath.isNotEmpty())
        {
            const auto file = juce::File::isAbsolutePath (exp.artworkPath)
                ? juce::File (exp.artworkPath)
                : owner.getProject().getProjectFolder().getChildFile (exp.artworkPath);
            if (file.existsAsFile())
                art = juce::ImageFileFormat::loadFrom (file);
        }
        if (art.isValid())
        {
            g.drawImage (art, thumb.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        }
        else
        {
            // Synthesize a coloured plate based on the expansion id hash so
            // each card stays visually distinguishable even without artwork.
            const auto hash = juce::String (exp.id.isEmpty() ? exp.name : exp.id).hashCode();
            const auto plate = juce::Colour::fromHSV ((float) ((hash % 360) / 360.0f), 0.55f, 0.55f, 1.0f);
            g.setColour (plate);
            g.fillRoundedRectangle (thumb.toFloat().reduced (4), 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font ((float) thumb.getHeight() * 0.55f, juce::Font::bold));
            const auto initial = exp.name.isNotEmpty() ? exp.name.substring (0, 1).toUpperCase()
                                                       : juce::String ("?");
            g.drawText (initial, thumb, juce::Justification::centred);
        }

        inner.removeFromTop (4);

        // Name.
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawFittedText (exp.name.isNotEmpty() ? exp.name : exp.id,
                          inner.removeFromTop (16),
                          juce::Justification::topLeft, 1);

        // Author / brand.
        const auto authorLine = exp.brand.isNotEmpty() ? exp.brand
                              : exp.author.isNotEmpty() ? exp.author : juce::String ("");
        if (authorLine.isNotEmpty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.5f);
            g.drawFittedText (authorLine, inner.removeFromTop (14),
                              juce::Justification::topLeft, 1);
        }

        // Counts.
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        const auto counts = juce::String (exp.includedPatchIds.size()) + " patches  •  "
                          + juce::String (exp.includedPresetNames.size()) + " presets";
        g.drawFittedText (counts, inner.removeFromBottom (14),
                          juce::Justification::bottomLeft, 1);
    }

    int ExpansionLibraryPanel::hitCard (juce::Point<int> pos) const
    {
        for (size_t i = 0; i < cards.size(); ++i)
            if (cards[i].bounds.contains (pos))
                return cards[i].expansionIndex;
        return -1;
    }

    void ExpansionLibraryPanel::mouseDown (const juce::MouseEvent& e)
    {
        const int idx = hitCard (e.getPosition());
        if (idx < 0) return;
        onCardClicked (idx, false);
    }

    void ExpansionLibraryPanel::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int idx = hitCard (e.getPosition());
        if (idx < 0) return;
        onCardClicked (idx, true);
    }

    void ExpansionLibraryPanel::onCardClicked (int expansionIndex, bool doubleClick)
    {
        const auto& expansions = owner.getProject().getExpansions();
        if (expansionIndex < 0 || expansionIndex >= (int) expansions.size())
            return;

        selectedExpansionId = expansions[(size_t) expansionIndex].id;
        repaint();

        if (doubleClick)
        {
            // Switch to the DSP page so the user lands in the editor for
            // this expansion's patches.
            owner.setBottomTab (BottomPanel::Page::DSP);
        }
    }
}
