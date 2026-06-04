#include "LayersPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <functional>

namespace patchcraft
{
    namespace
    {
        static juce::String scopedTabGroupId (const LayoutElement& tabPanel, const juce::String& label)
        {
            if (tabPanel.id == "tabs")
                return LayoutElement::tabLabelToGroupId (label);
            return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
        }

        juce::File groupDialogSettingsFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("PatchCraft")
                .getChildFile ("layer-group-dialog.txt");
        }

        bool shouldShowGroupDialog()
        {
            const auto file = groupDialogSettingsFile();
            return ! (file.existsAsFile() && file.loadFileAsString().trim() == "hidden");
        }

        void setShouldShowGroupDialog (bool shouldShow)
        {
            const auto file = groupDialogSettingsFile();
            file.getParentDirectory().createDirectory();
            file.replaceWithText (shouldShow ? "show" : "hidden");
        }

        bool isDescendantOfLayer (const std::vector<LayoutElement>& items,
                                  const LayoutElement& candidate,
                                  const juce::String& parentId)
        {
            juce::String current = candidate.containerId;
            if (current.isEmpty())
                current = candidate.groupId;

            for (int guard = 0; guard < 64 && current.isNotEmpty(); ++guard)
            {
                if (current == parentId)
                    return true;

                juce::String next;
                for (const auto& possibleParent : items)
                    if (possibleParent.id == current)
                    {
                        next = possibleParent.containerId.isNotEmpty() ? possibleParent.containerId : possibleParent.groupId;
                        break;
                    }
                current = next;
            }

            return false;
        }

        bool layerIsContainer (const LayoutElement& layer)
        {
            return layer.type == ElementType::Group || layer.type == ElementType::Panel || layer.type == ElementType::TabPanel;
        }

        void applyLayerVisibility (LayoutModel& model, const juce::String& id, bool visible)
        {
            auto& items = model.getAll();
            bool cascadeChildren = false;

            if (auto* item = model.find (id))
            {
                item->visible = visible;
                cascadeChildren = layerIsContainer (*item);
            }

            if (cascadeChildren)
                for (auto& child : items)
                    if (child.id != id && isDescendantOfLayer (items, child, id))
                        child.visible = visible;
        }
    }

    LayersPanel::LayersPanel (StudioMainComponent& o) : owner (o)
    {
        titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (titleLabel);

        for (auto* label : { &actionsLabel, &stackLabel })
        {
            label->setFont (juce::Font (10.0f, juce::Font::bold));
            label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            label->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (*label);
        }

        for (auto* button : { &addGroupButton, &groupSelectedButton, &ungroupButton, &selectAllButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }

        addGroupButton.onClick = [this] { createGroupFromSelection(); };
        groupSelectedButton.onClick = [this] { createGroupFromSelection(); };
        ungroupButton.onClick = [this] { ungroupSelection(); };
        selectAllButton.onClick = [this] { owner.selectAllElements(); };
        selectAllButton.setTooltip ("Select every layer on the canvas for bulk lock, unlock, delete, group, and visibility actions.");

        popOutBtn.setTooltip ("Pop the Layers panel into a free-floating window. Click again to dock.");
        popOutBtn.getProperties().set ("smallButton", true);
        popOutBtn.onClick = [this] { owner.togglePanelFloat (this, "Layers"); };
        addAndMakeVisible (popOutBtn);

        searchBox.setTextToShowWhenEmpty ("Search layers, ids, params...", PatchCraftLookAndFeel::textDim());
        searchBox.setTooltip ("Filter layers by label, id, type, parameter, group, or container.");
        searchBox.onTextChange = [this] { refresh(); };
        addAndMakeVisible (searchBox);

        clearSearchButton.getProperties().set ("smallButton", true);
        clearSearchButton.setTooltip ("Clear the layer search filter.");
        clearSearchButton.onClick = [this]
        {
            searchBox.clear();
            refresh();
        };
        addAndMakeVisible (clearSearchButton);

        addAndMakeVisible (listBox);
        listBox.setRowHeight (30);
        listBox.setMultipleSelectionEnabled (true);
        listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        listBox.setOutlineThickness (0);
        rebuildRows();
    }

    void LayersPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 30, getWidth(), 1);
        g.fillRect (0, 92, getWidth(), 1);
        g.fillRect (0, 128, getWidth(), 1);
    }

    void LayersPanel::resized()
    {
        auto r = getLocalBounds().reduced (4);
        auto header = r.removeFromTop (26);
        popOutBtn.setBounds (header.removeFromRight (38).reduced (2));
        header.removeFromRight (4);
        titleLabel.setBounds (header);
        r.removeFromTop (6);

        actionsLabel.setBounds (r.removeFromTop (16));
        auto top = r.removeFromTop (30);
        addGroupButton.setBounds (top.removeFromLeft (58).reduced (2));
        groupSelectedButton.setBounds (top.removeFromLeft (68).reduced (2));
        ungroupButton.setBounds (top.removeFromLeft (64).reduced (2));
        selectAllButton.setBounds (top.removeFromLeft (42).reduced (2));
        r.removeFromTop (4);
        auto search = r.removeFromTop (28);
        clearSearchButton.setBounds (search.removeFromRight (52).reduced (2));
        searchBox.setBounds (search.reduced (2));
        r.removeFromTop (6);
        stackLabel.setBounds (r.removeFromTop (16));
        r.removeFromTop (2);
        listBox.setBounds (r);
    }

    void LayersPanel::refresh()
    {
        rebuildRows();
        listBox.updateContent();
        for (int row = 0; row < (int) rows.size(); ++row)
        {
            const int idx = rows[(size_t) row].layoutIndex;
            const auto& list = owner.getProject().getLayout().getAll();
            if (idx >= 0 && idx < (int) list.size() && owner.isElementSelected (list[(size_t) idx].id))
            {
                listBox.scrollToEnsureRowIsOnscreen (row);
                break;
            }
        }
        listBox.repaint();
        repaint();
    }

    void LayersPanel::rebuildRows()
    {
        rows.clear();
        const auto& list = owner.getProject().getLayout().getAll();
        const auto query = searchBox.getText().trim().toLowerCase();

        auto parentFor = [&list] (const LayoutElement& el)
        {
            if (el.containerId.isNotEmpty())
                return el.containerId;
            for (const auto& parent : list)
                if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                    && parent.id == el.groupId)
                    return el.groupId;
            return juce::String();
        };

        auto matches = [&] (const LayoutElement& el)
        {
            if (query.isEmpty())
                return true;

            juce::String haystack;
            haystack << el.id << " " << el.label << " " << elementTypeDisplayName (el.type)
                     << " " << el.parameterId << " " << el.groupId << " " << el.containerId;
            return haystack.toLowerCase().contains (query);
        };

        std::function<bool (juce::String)> hasMatchingDescendant;
        hasMatchingDescendant = [&] (juce::String parentId)
        {
            for (const auto& el : list)
                if (parentFor (el) == parentId)
                    if (matches (el) || hasMatchingDescendant (el.id))
                        return true;

            return false;
        };

        std::function<void (juce::String, int)> addRowsForParent;
        addRowsForParent = [&] (juce::String parentId, int depth)
        {
            for (int i = (int) list.size() - 1; i >= 0; --i)
            {
                const auto& el = list[(size_t) i];
                if (parentFor (el) != parentId)
                    continue;

                const bool isGroup = el.type == ElementType::Group || el.type == ElementType::Panel;
                const bool include = query.isEmpty() || matches (el) || (isGroup && hasMatchingDescendant (el.id));
                if (! include)
                    continue;

                rows.push_back ({ i, depth, isGroup });
                if (isGroup && (query.isNotEmpty() || ! collapsedGroups.contains (el.id)))
                    addRowsForParent (el.id, depth + 1);
            }
        };

        addRowsForParent ({}, 0);
    }

    bool LayersPanel::rowMatchesSearch (const LayoutElement& el) const
    {
        const auto query = searchBox.getText().trim().toLowerCase();
        if (query.isEmpty())
            return true;

        juce::String haystack;
        haystack << el.id << " " << el.label << " " << elementTypeDisplayName (el.type)
                 << " " << el.parameterId << " " << el.groupId << " " << el.containerId;
        return haystack.toLowerCase().contains (query);
    }
    int LayersPanel::rowToLayoutIndex (int row) const
    {
        return row >= 0 && row < (int) rows.size() ? rows[(size_t) row].layoutIndex : -1;
    }

    juce::StringArray LayersPanel::getBulkTargetIdsFor (const LayoutElement& clicked) const
    {
        if (owner.isElementSelected (clicked.id))
        {
            auto ids = owner.getSelectedElementIds();
            if (ids.size() > 1)
                return ids;
        }

        juce::StringArray ids;
        ids.add (clicked.id);
        return ids;
    }

    int LayersPanel::getNumRows()
    {
        return (int) rows.size();
    }

    void LayersPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
    {
        const int idx = rowToLayoutIndex (row);
        auto& list = owner.getProject().getLayout().getAll();
        if (idx < 0 || idx >= (int) list.size()) return;
        auto& el = list[(size_t) idx];

        const bool selected = owner.isElementSelected (el.id);
        juce::Rectangle<int> r (0, 0, w, h);
        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.18f));
            g.fillRect (r);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.fillRect (0, 0, 3, h);
        }

        const int indent = rows[(size_t) row].depth * 18;
        auto body = r.reduced (4, 0).withTrimmedLeft (indent);
        auto eye = body.removeFromLeft (22);
        auto lock = body.removeFromLeft (20);
        auto disclosure = body.removeFromLeft (18);

        g.setFont (12.0f);
        g.setColour (el.visible ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim());
        g.drawText (el.visible ? "V" : "H", eye, juce::Justification::centred);
        g.setColour (el.locked ? juce::Colour (0xffe8b840) : PatchCraftLookAndFeel::textDim());
        g.drawText (el.locked ? "L" : "-", lock, juce::Justification::centred);

        if (rows[(size_t) row].isGroup)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawText (collapsedGroups.contains (el.id) ? ">" : "v", disclosure, juce::Justification::centred);
        }

        const auto label = el.label.isNotEmpty() ? el.label : el.id;
        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.5f, rows[(size_t) row].isGroup ? juce::Font::bold : juce::Font::plain));
        const int opacityW = body.getWidth() >= 120 ? 38 : 0;
        const int idW = body.getWidth() >= 260 ? 82 : 0;
        const int labelW = juce::jmax (60, body.getWidth() - opacityW - idW);
        g.drawText ((rows[(size_t) row].isGroup ? "Folder  " : elementTypeDisplayName (el.type) + "  ") + label,
                    body.removeFromLeft (labelW),
                    juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        if (opacityW > 0)
            g.drawText (juce::String (juce::roundToInt (el.opacity * 100.0f)) + "%",
                        body.removeFromLeft (opacityW), juce::Justification::centredRight);
        if (idW > 0)
            g.drawText (el.id, body, juce::Justification::centredRight);
    }
    void LayersPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
    {
        const int idx = rowToLayoutIndex (row);
        auto& list = owner.getProject().getLayout().getAll();
        if (idx < 0 || idx >= (int) list.size()) return;
        auto& el = list[(size_t) idx];

        if (e.mods.isPopupMenu())
        {
            if (! owner.isElementSelected (el.id))
                owner.setSelectedElementId (el.id);
            showContextMenu (row);
            return;
        }

        const int x = e.x;
        const int indent = rows[(size_t) row].depth * 18;
        if (x >= 4 + indent && x < 26 + indent)
        {
            const bool newVisible = ! el.visible;
            const auto targetIds = getBulkTargetIdsFor (el);
            owner.getProject().performLayoutEdit (newVisible
                    ? (targetIds.size() > 1 ? "Show selected layers" : "Show layer")
                    : (targetIds.size() > 1 ? "Hide selected layers" : "Hide layer"),
                [targetIds, newVisible] (LayoutModel& m)
                {
                    for (const auto& targetId : targetIds)
                        applyLayerVisibility (m, targetId, newVisible);
                });
            refresh();
            return;
        }
        if (x >= 26 + indent && x < 46 + indent)
        {
            const auto targetIds = getBulkTargetIdsFor (el);
            const bool locked = ! el.locked;
            owner.getProject().performLayoutEdit (locked
                    ? (targetIds.size() > 1 ? "Lock selected layers" : "Lock layer")
                    : (targetIds.size() > 1 ? "Unlock selected layers" : "Unlock layer"),
                [targetIds, locked] (LayoutModel& m)
                {
                    for (const auto& targetId : targetIds)
                        if (auto* item = m.find (targetId))
                            item->locked = locked;
                });
            refresh();
            return;
        }
        if (rows[(size_t) row].isGroup && x >= 46 + indent && x < 64 + indent)
        {
            if (collapsedGroups.contains (el.id)) collapsedGroups.removeString (el.id);
            else collapsedGroups.add (el.id);
            refresh();
            return;
        }
        if (x >= listBox.getWidth() - 92 && x <= listBox.getWidth() - 54)
        {
            const auto targetIds = getBulkTargetIdsFor (el);
            const float opacity = e.mods.isShiftDown()
                ? juce::jmax (0.0f, el.opacity - 0.1f)
                : (el.opacity >= 1.0f ? 0.25f : juce::jmin (1.0f, el.opacity + 0.25f));
            owner.getProject().performLayoutEdit (targetIds.size() > 1 ? "Adjust selected layer opacity" : "Adjust layer opacity",
                [targetIds, opacity] (LayoutModel& m)
                {
                    for (const auto& targetId : targetIds)
                        if (auto* item = m.find (targetId))
                            item->opacity = opacity;
                });
            refresh();
            return;
        }

        if (e.mods.isShiftDown())
        {
            const int anchor = lastClickedRow >= 0 ? lastClickedRow : row;
            juce::StringArray ids = (e.mods.isCommandDown() || e.mods.isCtrlDown())
                ? owner.getSelectedElementIds()
                : juce::StringArray();
            const int first = juce::jmin (anchor, row);
            const int last = juce::jmax (anchor, row);
            for (int r = first; r <= last; ++r)
            {
                const int layoutIndex = rowToLayoutIndex (r);
                if (layoutIndex >= 0 && layoutIndex < (int) list.size())
                    ids.addIfNotAlreadyThere (list[(size_t) layoutIndex].id);
            }
            owner.setSelectedElementIds (ids);
            // Keep the anchor stable across successive shift-clicks so the
            // user can keep extending the range from their first pick.
            return;
        }
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            owner.toggleSelectedElementId (el.id);
        }
        else
        {
            owner.setSelectedElementId (el.id);
        }

        lastClickedRow = row;
    }

    void LayersPanel::deleteKeyPressed (int /*lastRowSelected*/)
    {
        owner.deleteSelected();
    }

    void LayersPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        renameRow (row);
    }

    juce::var LayersPanel::getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe)
    {
        if (rowsToDescribe.isEmpty()) return {};

        // If the drag started on an element that's already part of the user's
        // selection, drag the whole selection. Otherwise drag just the rows
        // the listbox handed us. Use a unit-separator so multiple ids round-
        // trip through juce::var without depending on stable encodings.
        juce::StringArray ids;
        const auto& list = owner.getProject().getLayout().getAll();

        juce::StringArray draggedIds;
        for (int i = 0; i < rowsToDescribe.size(); ++i)
        {
            const int idx = rowToLayoutIndex (rowsToDescribe[i]);
            if (idx >= 0 && idx < (int) list.size())
                draggedIds.addIfNotAlreadyThere (list[(size_t) idx].id);
        }

        const auto selected = owner.getSelectedElementIds();
        bool startedFromSelection = false;
        for (const auto& id : draggedIds)
            if (selected.contains (id)) { startedFromSelection = true; break; }

        ids = startedFromSelection ? selected : draggedIds;
        return ids.joinIntoString ("\x1f");
    }

    bool LayersPanel::isInterestedInDragSource (const SourceDetails& details)
    {
        return details.description.toString().isNotEmpty();
    }

    void LayersPanel::itemDropped (const SourceDetails& details)
    {
        const auto encoded = details.description.toString();
        if (encoded.isEmpty()) return;

        juce::StringArray sourceIds;
        sourceIds.addTokens (encoded, "\x1f", {});
        sourceIds.removeEmptyStrings();
        if (sourceIds.isEmpty()) return;

        auto& list = owner.getProject().getLayout().getAll();

        // Resolve the target row at the drop position. A drop ON a container
        // or tab panel reparents; a drop on a regular row reorders the source
        // to land just before the target.
        const auto localToList = listBox.getLocalPoint (this, details.localPosition);
        const int targetRow = listBox.getRowContainingPosition (localToList.x, localToList.y);
        const int targetIdx = rowToLayoutIndex (targetRow);
        const LayoutElement* targetEl = (targetIdx >= 0 && targetIdx < (int) list.size())
            ? &list[(size_t) targetIdx] : nullptr;

        // Avoid recursive parenting: refuse to drop a container into itself
        // or any of its descendants.
        auto isDescendantOf = [&list] (const juce::String& candidateId, const juce::String& parentId)
        {
            if (candidateId == parentId) return true;
            juce::String current = parentId;
            for (int guard = 0; guard < 64 && current.isNotEmpty(); ++guard)
            {
                juce::String next;
                for (const auto& el : list)
                    if (el.id == current)
                    {
                        next = el.containerId.isNotEmpty() ? el.containerId : el.groupId;
                        break;
                    }
                if (next == candidateId) return true;
                current = next;
            }
            return false;
        };

        // Snapshot target identity into plain values — the layout vector is
        // mutated inside the edit lambda, which would invalidate any pointer
        // we kept alive across the call.
        const bool hasTarget = targetEl != nullptr;
        const juce::String targetId = hasTarget ? targetEl->id : juce::String();
        const ElementType targetType = hasTarget ? targetEl->type : ElementType::Knob;
        const juce::String firstTabGroup = (hasTarget && targetType == ElementType::TabPanel
                                           && ! targetEl->tabs.isEmpty())
            ? scopedTabGroupId (*targetEl, targetEl->tabs[0])
            : juce::String();

        const bool dropOnContainer = hasTarget
            && (targetType == ElementType::Group
             || targetType == ElementType::Panel);
        const bool dropOnTabPanel = hasTarget
            && targetType == ElementType::TabPanel
            && firstTabGroup.isNotEmpty();

        owner.getProject().performLayoutEdit (
            dropOnContainer ? "Move into container"
            : dropOnTabPanel ? "Move into tab"
            : "Reorder layers",
            [&] (LayoutModel& m)
        {
            auto& v = m.getAll();
            for (const auto& sourceId : sourceIds)
            {
                if (hasTarget && sourceId == targetId)
                    continue;
                if ((dropOnContainer || dropOnTabPanel)
                    && isDescendantOf (targetId, sourceId))
                    continue;

                auto* src = m.find (sourceId);
                if (src == nullptr)
                    continue;

                if (dropOnContainer)
                {
                    src->containerId = targetId;
                    src->groupId.clear();
                }
                else if (dropOnTabPanel)
                {
                    src->containerId.clear();
                    src->groupId = firstTabGroup;
                }
                else
                {
                    // Plain reorder within the flat layout list.
                    int fromIdx = -1;
                    for (size_t i = 0; i < v.size(); ++i)
                        if (v[i].id == sourceId) { fromIdx = (int) i; break; }
                    if (fromIdx < 0) continue;

                    LayoutElement moved = v[(size_t) fromIdx];
                    v.erase (v.begin() + fromIdx);

                    int insertIdx = (int) v.size();
                    if (hasTarget)
                    {
                        for (size_t i = 0; i < v.size(); ++i)
                            if (v[i].id == targetId) { insertIdx = (int) i; break; }
                    }
                    v.insert (v.begin() + juce::jlimit (0, (int) v.size(), insertIdx), moved);
                }
            }
        });

        owner.setSelectedElementIds (sourceIds);
        refresh();
    }

    void LayersPanel::backgroundClicked (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showContextMenu (-1);
            return;
        }
        owner.clearSelection();
    }

    void LayersPanel::showContextMenu (int row)
    {
        const bool hasSelection = ! owner.getSelectedElementIds().isEmpty();
        bool clickedGroup = false;
        juce::String clickedId;
        if (row >= 0 && row < (int) rows.size())
        {
            clickedGroup = rows[(size_t) row].isGroup;
            const int idx = rowToLayoutIndex (row);
            const auto& elements = owner.getProject().getLayout().getAll();
            if (idx >= 0 && idx < (int) elements.size())
                clickedId = elements[(size_t) idx].id;
        }

        juce::PopupMenu menu;
        menu.addItem (1, "Select All Layers", ! owner.getProject().getLayout().getAll().empty());
        menu.addItem (2, "Clear Selection", hasSelection);
        menu.addSeparator();
        menu.addItem (3, "Group Selection", hasSelection);
        menu.addItem (4, "Ungroup Selection", hasSelection);
        if (clickedGroup)
            menu.addItem (5, collapsedGroups.contains (clickedId) ? "Expand Group" : "Collapse Group");
        menu.addSeparator();
        menu.addItem (6, "Lock Selection", hasSelection);
        menu.addItem (7, "Unlock Selection", hasSelection);
        menu.addItem (8, "Show Selection", hasSelection);
        menu.addItem (9, "Hide Selection", hasSelection);
        menu.addSeparator();
        menu.addItem (10, "Duplicate Selection", hasSelection);
        menu.addItem (11, "Delete Selection", hasSelection);
        menu.addSeparator();
        menu.addItem (12, "Rename Layer", row >= 0);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&listBox),
            [safeThis = juce::Component::SafePointer<LayersPanel> (this), row, clickedId] (int result)
            {
                if (auto* panel = safeThis.getComponent())
                {
                    switch (result)
                    {
                        case 1: panel->owner.selectAllElements(); break;
                        case 2: panel->owner.clearSelection(); break;
                        case 3: panel->owner.groupSelectedElements(); break;
                        case 4: panel->owner.ungroupSelectedElements(); break;
                        case 5:
                            if (panel->collapsedGroups.contains (clickedId))
                                panel->collapsedGroups.removeString (clickedId);
                            else
                                panel->collapsedGroups.addIfNotAlreadyThere (clickedId);
                            panel->refresh();
                            break;
                        case 6: panel->owner.setSelectedLocked (true); break;
                        case 7: panel->owner.setSelectedLocked (false); break;
                        case 8: panel->owner.setSelectedVisibility (true); break;
                        case 9: panel->owner.setSelectedVisibility (false); break;
                        case 10: panel->owner.duplicateSelected(); break;
                        case 11: panel->owner.deleteSelected(); break;
                        case 12: panel->renameRow (row); break;
                        default: break;
                    }
                }
            });
    }

    void LayersPanel::renameRow (int row)
    {
        const int idx = rowToLayoutIndex (row);
        auto& list = owner.getProject().getLayout().getAll();
        if (idx < 0 || idx >= (int) list.size()) return;
        auto& el = list[(size_t) idx];

        auto* alert = new juce::AlertWindow ("Rename Layer", "Layer label:", juce::MessageBoxIconType::NoIcon);
        alert->addTextEditor ("name", el.label.isNotEmpty() ? el.label : el.id);
        alert->addButton ("Rename", 1);
        alert->addButton ("Cancel", 0);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert, id = el.id] (int result)
        {
            std::unique_ptr<juce::AlertWindow> owned (alert);
            if (result != 1) return;
            const auto name = alert->getTextEditorContents ("name").trim();
            owner.getProject().performLayoutEdit ("Rename layer",
                [id, name] (LayoutModel& m)
                {
                    if (auto* item = m.find (id))
                        item->label = name;
                });
            refresh();
        }), true);
    }

    void LayersPanel::createGroupFromSelection()
    {
        const auto ids = owner.getSelectedElementIds();
        juce::String groupId;
        owner.getProject().performLayoutEdit ("Create layer group",
            [&] (LayoutModel& m)
            {
                juce::Rectangle<int> bounds;
                juce::String commonContainer;
                juce::String commonGroup;
                bool first = true;

                for (const auto& id : ids)
                    if (auto* el = m.find (id); el != nullptr && el->type != ElementType::Group && ! el->locked)
                    {
                        const auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                        bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                        if (first)
                        {
                            commonContainer = el->containerId;
                            commonGroup = el->groupId;
                            first = false;
                        }
                        else
                        {
                            if (commonContainer != el->containerId) commonContainer.clear();
                            if (commonGroup != el->groupId) commonGroup.clear();
                        }
                    }

                LayoutElement group;
                group.type = ElementType::Group;
                group.label = "New Group";
                group.id = m.generateUniqueId ("group_");
                group.x = bounds.isEmpty() ? 40 : bounds.getX();
                group.y = bounds.isEmpty() ? 40 : bounds.getY();
                group.width = bounds.isEmpty() ? 240 : juce::jmax (32, bounds.getWidth());
                group.height = bounds.isEmpty() ? 160 : juce::jmax (32, bounds.getHeight());
                group.containerId = commonContainer;
                group.groupId = commonGroup;
                groupId = group.id;
                m.add (group);

                for (const auto& id : ids)
                    if (auto* el = m.find (id); el != nullptr && el->id != groupId && ! el->locked)
                    {
                        el->containerId = groupId;
                        el->groupId.clear();
                    }
            });

        if (groupId.isNotEmpty())
        {
            owner.setSelectedElementId (groupId);
            if (shouldShowGroupDialog())
                showGroupNameModal (groupId);
        }
        refresh();
    }

    void LayersPanel::showGroupNameModal (const juce::String& groupId)
    {
        auto* group = owner.getProject().getLayout().find (groupId);
        if (group == nullptr)
            return;

        auto* alert = new juce::AlertWindow ("Create Layer Group", "Name this layer group:", juce::MessageBoxIconType::NoIcon);
        alert->addTextEditor ("name", group->label.isNotEmpty() ? group->label : "New Group", "Group Name");

        auto options = std::make_shared<juce::Component>();
        auto autoOpen = std::make_shared<juce::ToggleButton> ("Auto Open");
        auto doNotAutoOpen = std::make_shared<juce::ToggleButton> ("Do not auto open");
        options->setSize (260, 48);
        options->addAndMakeVisible (*autoOpen);
        options->addAndMakeVisible (*doNotAutoOpen);
        autoOpen->setBounds (0, 0, 240, 22);
        doNotAutoOpen->setBounds (0, 24, 240, 22);
        autoOpen->setToggleState (true, juce::dontSendNotification);
        doNotAutoOpen->setTooltip ("When checked, new groups will be created without this naming popup.");
        alert->addCustomComponent (options.get());

        alert->addButton ("Create", 1);
        alert->addButton ("Cancel", 0);
        alert->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [this, alert, groupId, options, autoOpen, doNotAutoOpen] (int result)
                {
                    const juce::String name = alert->getTextEditorContents ("name").trim();
                    const bool shouldAutoOpen = autoOpen->getToggleState();
                    const bool hideFuturePopups = doNotAutoOpen->getToggleState();
                    std::unique_ptr<juce::AlertWindow> owned (alert);

                    if (hideFuturePopups)
                        setShouldShowGroupDialog (false);

                    if (result != 1)
                        return;

                    if (owner.getProject().getLayout().find (groupId) != nullptr)
                    {
                        if (shouldAutoOpen)
                            collapsedGroups.removeString (groupId);
                        else
                            collapsedGroups.addIfNotAlreadyThere (groupId);
                        owner.getProject().performLayoutEdit ("Rename layer group",
                            [groupId, label = (name.isNotEmpty() ? name : juce::String ("New Group"))] (LayoutModel& m)
                            {
                                if (auto* group = m.find (groupId))
                                    group->label = label;
                            });
                        refresh();
                    }
                }), true);
    }

    void LayersPanel::ungroupSelection()
    {
        const auto ids = owner.getSelectedElementIds();
        owner.getProject().performLayoutEdit ("Ungroup layers",
            [ids] (LayoutModel& m)
            {
                for (const auto& id : ids)
                    if (auto* el = m.find (id))
                        el->containerId.clear();
            });
        refresh();
    }

} // namespace patchcraft
