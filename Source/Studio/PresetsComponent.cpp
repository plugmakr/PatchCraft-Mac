#include "PresetsComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "PresetGenerator.h"

namespace patchcraft
{
    PresetsComponent::PresetsComponent (StudioMainComponent& o) : owner (o)
    {
        search.setTextToShowWhenEmpty ("Search presets...", PatchCraftLookAndFeel::textDim());
        search.setIndents (8, 4);
        search.onTextChange = [this]
        {
            filterText = search.getText().trim().toLowerCase();
            rebuildVisibleRows();
            list.updateContent();
            repaint();
        };
        addAndMakeVisible (search);

        // Bigger card-style rows so each preset shows category, name and
        // tags clearly. Replaces the original 22px numbered list.
        list.setRowHeight (52);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);

        addAndMakeVisible (newBtn);
        addAndMakeVisible (saveBtn);
        addAndMakeVisible (saveAsBtn);
        addAndMakeVisible (generateBtn);

        popOutBtn.setTooltip ("Pop the preset browser into a free-floating window. Click again to dock.");
        popOutBtn.getProperties().set ("smallButton", true);
        popOutBtn.onClick = [this]
        {
            owner.togglePanelFloat (this, "Presets");
        };
        addAndMakeVisible (popOutBtn);

        // Build the visible-row mapping immediately so the list isn't blank
        // before the first refresh() arrives from the host.
        rebuildVisibleRows();

        newBtn.setTooltip ("Name and lock in the current live sound as a new preset.");
        saveBtn.setTooltip ("Overwrite the selected preset with the current live knob/parameter values.");
        saveAsBtn.setTooltip ("Duplicate the current live sound into a newly named preset.");
        generateBtn.setTooltip ("Auto-generate a themed preset bank from the current instrument parameters.");
        newBtn.onClick = [this] { promptAndSaveCurrentPreset (true); };
        saveBtn.onClick = [this]
        {
            auto& presets = owner.getProject().getPresets();
            if (selectedPreset < 0 || selectedPreset >= (int) presets.size())
            {
                promptAndSaveCurrentPreset (true);
                return;
            }

            auto name = presets[(size_t) selectedPreset].name;
            auto preset = makePresetFromCurrentValues (name);
            preset.isDefault = presets[(size_t) selectedPreset].isDefault
                || owner.getProject().getManifest().defaultPreset == name;
            presets[(size_t) selectedPreset] = preset;
            upsertPatchForPreset (preset);
            owner.getProject().notifyChanged();
            refresh();
        };
        saveAsBtn.onClick = [this] { promptAndSaveCurrentPreset (true); };
        generateBtn.onClick = [this] { promptAndGeneratePresets(); };
    }

    void PresetsComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());

        // Render the category chip strip if there's space allocated.
        if (categoryChipBounds.size() != categoryChipOrder.size()
            || categoryChipBounds.empty())
            return;

        for (size_t i = 0; i < categoryChipOrder.size(); ++i)
        {
            const auto& chip = categoryChipOrder[i];
            const auto& bounds = categoryChipBounds[i];
            const bool active = chip == activeCategory;
            const auto colour = colourForCategory (chip);
            g.setColour (active ? colour.withAlpha (0.85f) : colour.withAlpha (0.20f));
            g.fillRoundedRectangle (bounds.toFloat(), 9.0f);
            g.setColour (active ? juce::Colour (0xff0a0c10) : colour.brighter (0.10f));
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawText (chip, bounds, juce::Justification::centred);
        }
    }

    void PresetsComponent::resized()
    {
        auto r = getLocalBounds().reduced (8);
        auto searchRow = r.removeFromTop (24);
        popOutBtn.setBounds (searchRow.removeFromRight (38));
        searchRow.removeFromRight (4);
        search.setBounds (searchRow);
        r.removeFromTop (6);

        // Category chip strip — height adapts to whether any chips exist.
        rebuildVisibleRows();   // also populates categoryChipOrder
        if (! categoryChipOrder.empty())
        {
            auto chipRow = r.removeFromTop (22);
            r.removeFromTop (6);
            categoryChipBounds.clear();
            int x = chipRow.getX();
            for (const auto& chip : categoryChipOrder)
            {
                const int chipW = juce::jmax (40, 12 + chip.length() * 6);
                if (x + chipW > chipRow.getRight()) break;   // hide overflowing chips
                categoryChipBounds.push_back (juce::Rectangle<int> (x, chipRow.getY(), chipW, chipRow.getHeight()));
                x += chipW + 4;
            }
        }
        else
        {
            categoryChipBounds.clear();
        }

        auto bottom = r.removeFromBottom (56);

        auto manual = bottom.removeFromTop (28);
        const int third = manual.getWidth() / 3;
        newBtn.setBounds    (manual.removeFromLeft (third).reduced (1));
        saveBtn.setBounds   (manual.removeFromLeft (third).reduced (1));
        saveAsBtn.setBounds (manual.reduced (1));
        generateBtn.setBounds (bottom.reduced (1));

        list.setBounds (r);
    }

    void PresetsComponent::refresh()
    {
        const bool hasSelection = selectedPreset >= 0 && selectedPreset < (int) owner.getProject().getPresets().size();
        saveBtn.setEnabled (hasSelection);
        saveBtn.setTooltip (hasSelection
            ? "Overwrite the selected preset with the current live knob/parameter values."
            : "Select a preset first, or use New / Save As to create one from the current sound.");
        rebuildVisibleRows();
        resized();   // re-layout chip strip when categories change
        list.updateContent();
        // Highlight the row that maps to the currently-selected preset (if it
        // survives the visibility filter).
        for (int i = 0; i < (int) visibleRowIndices.size(); ++i)
            if (visibleRowIndices[(size_t) i] == selectedPreset)
            {
                list.selectRow (i, true, true);
                break;
            }
        repaint();
    }

    int PresetsComponent::getNumRows()
    {
        return (int) visibleRowIndices.size();
    }

    juce::String PresetsComponent::categoryFor (const Preset& p) const
    {
        // Prefer explicit theme/tag if present, else heuristic on name.
        if (p.theme.isNotEmpty()) return p.theme;
        for (const auto& t : p.tags)
        {
            const auto lower = t.toLowerCase();
            if (lower == "lead" || lower == "bass" || lower == "pad"
                || lower == "arp"  || lower == "pluck" || lower == "fx"
                || lower == "drum" || lower == "key")
                return t;
        }
        const auto n = p.name.toLowerCase();
        if (n.contains ("bass") || n.contains ("sub"))   return "Bass";
        if (n.contains ("arp")  || n.contains ("16th") || n.contains ("triplet")
            || n.contains ("stab") || n.contains ("gate"))                return "Arp";
        if (n.contains ("pad")  || n.contains ("bloom") || n.contains ("aurora")
            || n.contains ("choir") || n.contains ("sweep"))              return "Pad";
        if (n.contains ("lead") || n.contains ("supersaw") || n.contains ("hoover")
            || n.contains ("anjuna") || n.contains ("goa") || n.contains ("acid lead"))
            return "Lead";
        if (n.contains ("riser") || n.contains ("fx") || n.contains ("noise")
            || n.contains ("snare"))
            return "FX";
        if (n.contains ("wt ") || n.contains ("wavetable") || n.contains ("glass")
            || n.contains ("razor") || n.contains ("drift") || n.contains ("modular")
            || n.contains ("forest") || n.contains ("psy tweet") || n.contains ("stutter")
            || n.contains ("random lfo") || n.contains ("drone"))
            return "Mod";
        return "Patch";
    }

    juce::Colour PresetsComponent::colourForCategory (const juce::String& cat) const
    {
        const auto lower = cat.toLowerCase();
        if (lower == "lead")  return juce::Colour (0xfff5a623);
        if (lower == "bass")  return juce::Colour (0xff8a4fff);
        if (lower == "pad")   return juce::Colour (0xff4ec6ff);
        if (lower == "arp")   return juce::Colour (0xff44d18a);
        if (lower == "fx")    return juce::Colour (0xffff5e6c);
        if (lower == "mod")   return juce::Colour (0xffd6c95a);
        if (lower == "drum")  return juce::Colour (0xffe07b6c);
        if (lower == "pluck") return juce::Colour (0xff44d18a);
        return juce::Colour (0xff7f7f88);
    }

    void PresetsComponent::rebuildVisibleRows()
    {
        visibleRowIndices.clear();
        const auto& presets = owner.getProject().getPresets();
        const auto filterLower = filterText.toLowerCase();
        const auto categoryLower = activeCategory.toLowerCase();

        // Build chip order from distinct categories present.
        std::map<juce::String, int> seen;
        categoryChipOrder.clear();
        for (const auto& p : presets)
        {
            const auto cat = categoryFor (p);
            if (! seen.count (cat))
            {
                seen[cat] = 1;
                categoryChipOrder.push_back (cat);
            }
        }

        for (size_t i = 0; i < presets.size(); ++i)
        {
            const auto& p = presets[i];
            if (! categoryLower.isEmpty()
                && categoryFor (p).toLowerCase() != categoryLower)
                continue;
            if (! filterLower.isEmpty())
            {
                const bool nameMatch = p.name.toLowerCase().contains (filterLower);
                const bool tagMatch  = std::any_of (p.tags.begin(), p.tags.end(),
                    [&] (const juce::String& t) { return t.toLowerCase().contains (filterLower); });
                const bool descMatch = p.description.toLowerCase().contains (filterLower);
                if (! (nameMatch || tagMatch || descMatch))
                    continue;
            }
            visibleRowIndices.push_back ((int) i);
        }
    }

    void PresetsComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
    {
        if (row < 0 || row >= (int) visibleRowIndices.size()) return;
        auto& presets = owner.getProject().getPresets();
        const int idx = visibleRowIndices[(size_t) row];
        if (idx < 0 || idx >= (int) presets.size()) return;
        auto& p = presets[(size_t) idx];

        // Card background — brighter base than the prior version so text
        // is readable, plus a clear hover/selected state.
        const juce::Rectangle<int> card (4, 2, w - 8, h - 4);
        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.22f));
            g.fillRoundedRectangle (card.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, 1.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xff242830));
            g.fillRoundedRectangle (card.toFloat(), 4.0f);
            g.setColour (juce::Colour (0xff3a3f48));
            g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, 1.0f);
        }

        const auto category = categoryFor (p);
        const auto categoryColour = colourForCategory (category);

        // Category badge — coloured stripe on the left.
        g.setColour (categoryColour);
        g.fillRect (4, 2, 4, h - 4);

        // Index number — brighter so it's actually readable.
        g.setColour (juce::Colour (0xffb5bcc8));
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.drawText (juce::String::formatted ("%02d", idx + 1),
                    14, 4, 24, 16, juce::Justification::topLeft);

        // Preset name — 14px, white. Reserve right side for pill + star.
        g.setColour (selected ? PatchCraftLookAndFeel::accent() : juce::Colour (0xfff1f3f7));
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        const int rightReserved = 80;
        g.drawText (p.name, 38, 4, w - 38 - rightReserved, 22, juce::Justification::centredLeft);

        // Description / category line — full text colour rather than dim, so
        // it's actually legible at the smaller size.
        g.setColour (juce::Colour (0xffc7cdd6));
        g.setFont (juce::Font (11.0f));
        const auto descLine = p.description.isNotEmpty()
            ? p.description
            : juce::String ("Category: ") + category
              + (p.tags.isEmpty() ? juce::String() : "  •  " + p.tags.joinIntoString (", "));
        g.drawText (descLine, 14, 28, w - 28 - rightReserved, 18, juce::Justification::topLeft, true);

        // Right side: category pill + default star.
        const int pillW = 64;
        const auto pillRect = juce::Rectangle<int> (w - pillW - 14, h / 2 - 10, pillW, 20);
        g.setColour (categoryColour.withAlpha (0.40f));
        g.fillRoundedRectangle (pillRect.toFloat(), 9.0f);
        g.setColour (categoryColour);
        g.drawRoundedRectangle (pillRect.toFloat().reduced (0.5f), 9.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.drawText (category.toUpperCase(), pillRect, juce::Justification::centred);

        if (p.isDefault)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            juce::Path star;
            const float cx = (float) w - 22.0f, cy = 12.0f, R = 5.5f, rr = 2.6f;
            for (int i = 0; i < 10; ++i)
            {
                const float ang = juce::MathConstants<float>::pi * 2.0f * i / 10.0f
                                  - juce::MathConstants<float>::halfPi;
                const float rad = (i % 2 == 0) ? R : rr;
                const float x = cx + std::cos (ang) * rad;
                const float y = cy + std::sin (ang) * rad;
                if (i == 0) star.startNewSubPath (x, y);
                else        star.lineTo (x, y);
            }
            star.closeSubPath();
            g.fillPath (star);
        }
    }

    void PresetsComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        if (row < 0 || row >= (int) visibleRowIndices.size()) return;
        const int idx = visibleRowIndices[(size_t) row];
        auto& presets = owner.getProject().getPresets();
        if (idx < 0 || idx >= (int) presets.size()) return;

        selectedPreset = idx;
        const auto& p = presets[(size_t) idx];
        owner.getProject().applyPreset (p);
        owner.refreshAllPanels();
        refresh();
    }

    void PresetsComponent::mouseDown (const juce::MouseEvent& e)
    {
        // Click on a category chip toggles that filter.
        for (size_t i = 0; i < categoryChipBounds.size() && i < categoryChipOrder.size(); ++i)
        {
            if (categoryChipBounds[i].contains (e.getPosition()))
            {
                const auto chip = categoryChipOrder[i];
                activeCategory = (activeCategory == chip) ? juce::String() : chip;
                rebuildVisibleRows();
                list.updateContent();
                repaint();
                return;
            }
        }
    }

    Preset PresetsComponent::makePresetFromCurrentValues (const juce::String& name) const
    {
        const auto presetName = name.trim().isNotEmpty() ? name.trim()
            : "Preset " + juce::String ((int) owner.getProject().getPresets().size() + 1);
        return owner.getProject().captureCurrentPatch (presetName).toPreset();
    }

    void PresetsComponent::upsertPatchForPreset (const Preset& preset)
    {
        auto patch = owner.getProject().captureCurrentPatch (preset.name);
        patch.id = preset.patchId.isNotEmpty() ? preset.patchId : patch.id;
        patch.isDefault = preset.isDefault;
        auto& patches = owner.getProject().getPatches();
        for (auto& existing : patches)
        {
            if (existing.id == patch.id)
            {
                existing = patch;
                return;
            }
        }
        patches.push_back (std::move (patch));
    }

    void PresetsComponent::promptAndSaveCurrentPreset (bool)
    {
        auto* window = new juce::AlertWindow ("Save Preset",
            "Name this sound. This locks in the current live parameter values as a reusable instrument preset.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", "Preset " + juce::String ((int) owner.getProject().getPresets().size() + 1), "Preset Name:");
        window->addButton ("Save Preset", 1);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result != 1) return;

                auto preset = makePresetFromCurrentValues (window->getTextEditorContents ("name"));
                for (auto& existing : owner.getProject().getPresets())
                    existing.isDefault = false;
                preset.isDefault = true;
                owner.getProject().getPresets().push_back (preset);
                upsertPatchForPreset (preset);
                selectedPreset = (int) owner.getProject().getPresets().size() - 1;
                owner.getProject().getManifest().defaultPreset = preset.name;
                owner.getProject().notifyChanged();
                refresh();
            }), true);
    }

    void PresetsComponent::promptAndGeneratePresets()
    {
        auto* window = new juce::AlertWindow ("Generate Preset Bank",
            "Create a themed preset bank from this instrument's current parameter set.",
            juce::MessageBoxIconType::NoIcon);

        window->addComboBox ("theme", PresetGenerator::themes(), "Theme:");
        window->addTextEditor ("count", "16", "Count:");
        if (auto* theme = window->getComboBoxComponent ("theme"))
        {
            theme->setSelectedId (3, juce::dontSendNotification);
            theme->onChange = [window, theme]
            {
                if (theme->getText().toLowerCase().contains ("wavetable"))
                    if (auto* count = window->getTextEditor ("count"))
                        count->setText ("10", juce::dontSendNotification);
            };
        }
        window->addTextEditor ("seed", "", "Seed (optional):");
        window->addButton ("Append", 1);
        window->addButton ("Replace Generated", 2);
        window->addButton ("Cancel", 0);

        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result <= 0) return;

                PresetGenerationOptions options;
                if (auto* theme = window->getComboBoxComponent ("theme"))
                    options.theme = theme->getText();
                options.count = juce::jlimit (1, 128, window->getTextEditorContents ("count").getIntValue());
                const auto seedText = window->getTextEditorContents ("seed").trim();
                options.seed = seedText.isNotEmpty()
                    ? (juce::uint32) seedText.hashCode()
                    : (juce::uint32) juce::Time::getMillisecondCounter();

                auto generated = PresetGenerator::generate (owner.getProject().getParameters(),
                                                            owner.getProject().getLiveValues(),
                                                            owner.getProject().getEngineType(),
                                                            options);
                auto& presets = owner.getProject().getPresets();
                if (result == 2)
                {
                    presets.erase (std::remove_if (presets.begin(), presets.end(),
                        [] (const Preset& preset) { return preset.generated; }), presets.end());
                }

                for (auto& preset : generated)
                {
                    auto baseName = preset.name;
                    int suffix = 2;
                    bool unique = false;
                    while (! unique)
                    {
                        unique = true;
                        for (const auto& existing : presets)
                            if (existing.name == preset.name)
                                unique = false;
                        if (! unique)
                            preset.name = baseName + " " + juce::String (suffix++);
                    }
                    presets.push_back (preset);
                }

                if (! generated.empty())
                {
                    selectedPreset = (int) presets.size() - (int) generated.size();
                    owner.getProject().getManifest().defaultPreset = presets[(size_t) selectedPreset].name;
                }
                owner.getProject().notifyChanged();
                refresh();
            }), true);
    }

} // namespace patchcraft
