#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "TopToolbar.h"
#include "ElementPalette.h"
#include "BuiltAssetLibraryComponent.h"
#include "ExpansionLibraryPanel.h"
#include "FloatingPanelWindow.h"
#include "LayersPanel.h"
#include "CanvasEditor.h"
#include "InspectorPanel.h"
#include "BottomPanel.h"
#include "CanvasToolbar.h"
#include "SettingsDialog.h"
#include "PatchCraftPackWriter.h"
#include "SampleMap.h"

#include <algorithm>
#include <map>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static juce::String validationWarningSummary (const PatchCraftProject& project)
        {
            const auto issues = project.getParameters().validateReferences (
                project.getLayout().getAll(), project.getDspGraph(), project.getPresets());

            juce::StringArray warnings;
            for (const auto& issue : issues)
            {
                if (issue.severity != "error")
                    warnings.add (issue.toString());
                if (warnings.size() >= 8)
                    break;
            }
            for (const auto& issue : project.getDspGraph().validateTypedGraph (project.getManifest().engine))
            {
                if (issue.severity != "error")
                    warnings.add (issue.toString());
                if (warnings.size() >= 8)
                    break;
            }

            return warnings.isEmpty()
                ? juce::String()
                : ("\n\nWarnings:\n" + warnings.joinIntoString ("\n"));
        }

        static bool showSampleExportValidationIfBlocked (StudioMainComponent& owner,
                                                         const PatchCraftProject& project,
                                                         const juce::String& title)
        {
            const auto health = SampleMap::evaluateHealth (project.getSampleMap(),
                                                           project.getProjectFolder(),
                                                           project.getEngineType());
            if (! health.blocksExport())
                return false;

            juce::Component::SafePointer<StudioMainComponent> safeOwner (&owner);
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (health.exportMessage())
                    .withButton ("Open Sample Mapper")
                    .withButton ("Cancel Export")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                [safeOwner] (int result)
                {
                    if (result == 1)
                        if (auto* component = safeOwner.getComponent())
                            component->setBottomTab (BottomPanel::Page::SampleMapper);
            });
            return true;
        }

        static juce::String studioScopedTabGroupId (const LayoutElement& tabPanel,
                                                    const juce::String& label)
        {
            if (tabPanel.id == "tabs")
                return LayoutElement::tabLabelToGroupId (label);
            return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
        }

        static juce::File studioPreferencesFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("PatchCraft")
                .getChildFile ("studio-preferences.json");
        }

        static bool readStudioTutorialPreference (bool fallback)
        {
            const auto file = studioPreferencesFile();
            if (! file.existsAsFile())
                return fallback;

            auto parsed = juce::JSON::parse (file);
            if (auto* object = parsed.getDynamicObject())
                if (object->hasProperty ("studioShowTutorials"))
                    return (bool) object->getProperty ("studioShowTutorials");

            return fallback;
        }

        static void writeStudioTutorialPreference (bool enabled)
        {
            auto file = studioPreferencesFile();
            if (! file.getParentDirectory().createDirectory())
                return;

            auto* object = new juce::DynamicObject();
            object->setProperty ("studioShowTutorials", enabled);
            file.replaceWithText (juce::JSON::toString (juce::var (object), true));
        }

        static juce::String defaultParameterForLibraryAsset (const PatchCraftProject& project,
                                                             ElementType type)
        {
            auto choose = [&] (std::initializer_list<const char*> ids) -> juce::String
            {
                for (auto* id : ids)
                    if (project.getParameters().find (id) != nullptr)
                        return id;
                return {};
            };

            if (type == ElementType::Knob)
                if (auto id = choose ({ "filterCutoff", "volume", "oscBlend", "delayMix", "reverbMix" }); id.isNotEmpty())
                    return id;

            if (type == ElementType::Slider)
                if (auto id = choose ({ "volume", "filterCutoff", "delayMix", "reverbMix" }); id.isNotEmpty())
                    return id;

            if (type == ElementType::Meter)
                if (auto id = choose ({ "volume" }); id.isNotEmpty())
                    return id;

            for (const auto& def : project.getParameters().getAll())
            {
                if (! def.visible || ! def.hostAutomatable)
                    continue;
                if (def.displayMode == "toggle" || def.displayMode == "stepped")
                    continue;
                return def.id;
            }

            return {};
        }
    }

    bool StudioMainComponent::isPreviewActive() const
    {
        return bottomPanel != nullptr && bottomPanel->isPreviewActive();
    }

    const SampleZoneDef* StudioMainComponent::getSelectedSampleZone() const
    {
        return bottomPanel != nullptr ? bottomPanel->getSelectedSampleZone() : nullptr;
    }

    void StudioMainComponent::setBottomTab (BottomPanel::Page p)
    {
        if (bottomTab == p) return;
        bottomTab = p;
        if (bottomPanel) bottomPanel->setPage (p);
        if (canvasToolbar) canvasToolbar->syncSectionTabFromOwner();
        if (p == BottomPanel::Page::DSP
            && ! dspTutorialShownThisSession
            && getStudioTutorialsEnabled())
        {
            dspTutorialShownThisSession = true;
            juce::Component::SafePointer<StudioMainComponent> self (this);
            juce::MessageManager::callAsync ([self]
            {
                if (auto* component = self.getComponent())
                    component->showDspBuilderTutorial();
            });
        }
        // Hide/show sidebar/canvas/inspector based on the new tab.
        resized();
        repaint();
    }

    // SettingsWindowHolder is just a unique_ptr<SettingsWindow> with a deleter
    // that runs on the message thread (done implicitly by the host caller).
    class StudioMainComponent::SettingsWindowHolder
    {
    public:
        std::unique_ptr<SettingsWindow> w;
    };

    void StudioMainComponent::openSettings()
    {
        if (settingsWindow != nullptr && settingsWindow->w != nullptr)
        {
            settingsWindow->w->toFront (true);
            return;
        }
        settingsWindow = std::make_unique<SettingsWindowHolder>();
        juce::Component::SafePointer<StudioMainComponent> self (this);
        settingsWindow->w = std::make_unique<SettingsWindow> (audioService, ai,
            [self]
            {
                juce::MessageManager::callAsync ([self]
                {
                    if (auto* p = self.getComponent()) p->settingsWindow.reset();
                });
            });
    }

    StudioMainComponent::StudioMainComponent()
    {
        setOpaque (true);
        menuBar.setModel (this);

        topToolbar      = std::make_unique<TopToolbar> (*this);
        elementPalette  = std::make_unique<ElementPalette> (*this);
        assetLibraryPanel = std::make_unique<BuiltAssetLibraryComponent> (*this);
        expansionLibraryPanel = std::make_unique<ExpansionLibraryPanel> (*this);
        layersPanel     = std::make_unique<LayersPanel> (*this);
        canvasEditor    = std::make_unique<CanvasEditor> (*this);
        canvasToolbar   = std::make_unique<CanvasToolbar> (*this, *canvasEditor);
        inspectorPanel  = std::make_unique<InspectorPanel> (*this);
        bottomPanel     = std::make_unique<BottomPanel> (*this);

        addAndMakeVisible (menuBar);
        addAndMakeVisible (*topToolbar);
        addAndMakeVisible (*elementPalette);
        addChildComponent (*assetLibraryPanel);
        addChildComponent (*expansionLibraryPanel);
        addChildComponent (*layersPanel);
        addAndMakeVisible (*canvasToolbar);
        addAndMakeVisible (*canvasEditor);
        addAndMakeVisible (*inspectorPanel);
        addAndMakeVisible (*bottomPanel);

        // Left tabs (Elements / Layers / Library)
        // Layers moved to the right (Inspector) panel. Left side is now
        // Elements / Library / Expansions only.
        leftTabs.addTab ("Elements",   PatchCraftLookAndFeel::panel(), -1);
        leftTabs.addTab ("Library",    PatchCraftLookAndFeel::panel(), -1);
        leftTabs.addTab ("Expansions", PatchCraftLookAndFeel::panel(), -1);
        leftTabs.setCurrentTabIndex (0, juce::dontSendNotification);
        addAndMakeVisible (leftTabs);
        leftCollapseButton.getProperties().set ("smallButton", true);
        leftCollapseButton.setTooltip ("Collapse or expand the Elements/Layers panel");
        leftCollapseButton.onClick = [this]
        {
            leftPanelCollapsed = ! leftPanelCollapsed;
            leftCollapseButton.setButtonText (leftPanelCollapsed ? ">" : "<");
            resized();
            repaint();
        };
        addAndMakeVisible (leftCollapseButton);

        rightCollapseButton.getProperties().set ("smallButton", true);
        rightCollapseButton.setTooltip ("Collapse or expand the Inspector panel");
        rightCollapseButton.onClick = [this]
        {
            rightPanelCollapsed = ! rightPanelCollapsed;
            rightCollapseButton.setButtonText (rightPanelCollapsed ? "<" : ">");
            resized();
            repaint();
        };
        addAndMakeVisible (rightCollapseButton);

        // Right panel tabs: Inspector (default) and Layers.
        rightTabs.addTab ("Inspector", PatchCraftLookAndFeel::panel(), -1);
        rightTabs.addTab ("Layers",    PatchCraftLookAndFeel::panel(), -1);
        rightTabs.setCurrentTabIndex (0, juce::dontSendNotification);
        addAndMakeVisible (rightTabs);
        for (int i = 0; i < rightTabs.getNumTabs(); ++i)
        {
            auto* tb = rightTabs.getTabButton (i);
            tb->onClick = [this, i]
            {
                rightTabs.setCurrentTabIndex (i);
                showLayersInRightPanel = (i == 1);
                inspectorPanel->setVisible (! showLayersInRightPanel);
                layersPanel->setVisible (showLayersInRightPanel);
                if (showLayersInRightPanel) layersPanel->refresh();
                resized();
            };
        }

        for (int i = 0; i < leftTabs.getNumTabs(); ++i)
        {
            auto* tb = leftTabs.getTabButton (i);
            tb->onClick = [this, i] {
                leftTabs.setCurrentTabIndex (i);
                // Layers tab is now hosted on the right; left tabs are
                // Elements (0), Library (1), Expansions (2).
                showLayersInsteadOfElements      = false;
                showLibraryInsteadOfElements     = (i == 1);
                showExpansionsInsteadOfElements  = (i == 2);
                const bool showElements = ! showLibraryInsteadOfElements
                                       && ! showExpansionsInsteadOfElements;
                elementPalette->setVisible (showElements);
                assetLibraryPanel->setVisible (showLibraryInsteadOfElements);
                expansionLibraryPanel->setVisible (showExpansionsInsteadOfElements);
                if (showExpansionsInsteadOfElements)
                    expansionLibraryPanel->refresh();
                resized();
            };
        }

        project.addListener (this);
        project.getLiveValues().addListener (this);
    }

    StudioMainComponent::~StudioMainComponent()
    {
        menuBar.setModel (nullptr);
        settingsWindow.reset();
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void StudioMainComponent::liveValueChanged (const juce::String&, float)
    {
        // Canvas and test controls derive directly from LiveValueStore.
        // Avoid full panel rebuilds while dragging controls; that stalls audio/UI response.
        if (canvasEditor != nullptr)
            canvasEditor->repaint();
        if (bottomPanel != nullptr)
            bottomPanel->repaint();
        if (inspectorPanel != nullptr)
            inspectorPanel->repaint();
    }

    void StudioMainComponent::projectChanged()
    {
        refreshAllPanels();
    }

    void StudioMainComponent::refreshAllPanels()
    {
        topToolbar->setProjectName (project.getManifest().instrumentName,
                                    project.hasUnsavedChanges());
        bottomPanel->refresh();
        inspectorPanel->refresh();
        layersPanel->refresh();
        assetLibraryPanel->refresh();
        if (canvasToolbar)
        {
            canvasToolbar->refresh();
            canvasToolbar->syncSectionTabFromOwner();
        }
        canvasEditor->repaint();
    }

    void StudioMainComponent::refreshCanvasToolbar()
    {
        if (canvasToolbar)
        {
            canvasToolbar->refresh();
            canvasToolbar->syncSectionTabFromOwner();
        }
    }

    juce::String StudioMainComponent::getCurrentDspPatchSectionLabel() const
    {
        return bottomPanel != nullptr ? bottomPanel->getDspPatchSectionLabel() : juce::String ("Source");
    }

    void StudioMainComponent::addElementToCanvas (ElementType type, juce::String parameterId)
    {
        if (canvasEditor == nullptr)
            return;

        const auto& canvas = project.getCanvasSize();
        canvasEditor->addElementAt (type,
            { canvas.width / 2 - 48, canvas.height / 2 - 48 },
            std::move (parameterId));
    }

    void StudioMainComponent::addLibraryAssetToCanvas (const juce::String& category, const juce::File& file,
                                                       int frames, bool vertical)
    {
        auto type = ElementType::Knob;
        if (category.startsWithIgnoreCase ("slider"))
            type = ElementType::Slider;
        else if (category.startsWithIgnoreCase ("meter"))
            type = ElementType::Meter;

        auto& canvas = project.getCanvasSize();
        int assetWidth = type == ElementType::Slider ? 52 : 112;
        int assetHeight = type == ElementType::Slider ? 220 : 112;
        if (auto image = juce::ImageFileFormat::loadFrom (file); image.isValid())
        {
            const int safeFrames = juce::jmax (1, frames);
            const int frameWidth = vertical ? image.getWidth()
                                            : juce::jmax (1, image.getWidth() / safeFrames);
            const int frameHeight = vertical ? juce::jmax (1, image.getHeight() / safeFrames)
                                             : image.getHeight();
            assetWidth = juce::jlimit (24, 320, frameWidth);
            assetHeight = juce::jlimit (24, 520, frameHeight);
        }
        project.performLayoutEdit ("Add library asset", [&] (LayoutModel& layout)
        {
            LayoutElement element;
            element.type = type;
            element.id = layout.generateUniqueId (elementTypeToString (type) + "_");
            element.label = file.getFileNameWithoutExtension();
            element.x = canvas.width / 2 - assetWidth / 2;
            element.y = canvas.height / 2 - assetHeight / 2;
            element.width = assetWidth;
            element.height = assetHeight;
            element.filmstripAsset = file.getFullPathName();
            element.filmstripFrames = frames;
            element.filmstripVertical = vertical;
            element.parameterId = defaultParameterForLibraryAsset (project, type);
            layout.add (element);
        });

        setSelectedElementId (project.getLayout().getAll().back().id);
    }

    void StudioMainComponent::setSelectedElementId (juce::String id)
    {
        if (selectedElementId == id && selectedElementIds.size() == (id.isEmpty() ? 0 : 1)) return;
        selectedElementId = std::move (id);
        selectedElementIds.clear();
        if (selectedElementId.isNotEmpty())
            selectedElementIds.add (selectedElementId);
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    bool StudioMainComponent::isElementSelected (const juce::String& id) const
    {
        return selectedElementIds.contains (id);
    }

    void StudioMainComponent::setSelectedElementIds (juce::StringArray ids)
    {
        ids.removeEmptyStrings();
        selectedElementIds = std::move (ids);
        selectedElementId = selectedElementIds.isEmpty() ? juce::String() : selectedElementIds[0];
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    void StudioMainComponent::toggleSelectedElementId (const juce::String& id)
    {
        if (id.isEmpty()) return;
        if (selectedElementIds.contains (id))
            selectedElementIds.removeString (id);
        else
            selectedElementIds.add (id);
        selectedElementId = selectedElementIds.isEmpty() ? juce::String() : selectedElementIds[0];
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    void StudioMainComponent::clearSelection()
    {
        setSelectedElementIds ({});
    }

    // -------------------------------------------------------------------------
    // Painting & layout
    // -------------------------------------------------------------------------
    void StudioMainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        // Status bar text along the bottom.
        const int sbH = 24;
        auto sb = getLocalBounds().removeFromBottom (sbH);
        g.setColour (juce::Colour (0xff0d0e11));
        g.fillRect (sb);
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (sb.removeFromTop (1));

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f));
        const auto cpu    = juce::String::formatted ("CPU: %.1f%%", statusCpu.load());
        const auto voices = juce::String::formatted ("Voices: %d", statusVoices.load());
        const auto sr     = juce::String ("Sample Rate: 48.0 kHz");

        auto sbContent = sb.reduced (12, 0);
        g.drawText (cpu + "      " + voices + "      " + sr, sbContent,
                    juce::Justification::centredLeft);

        const auto right = "Project: " + project.getManifest().instrumentName
            + "      Last saved: " + (project.hasUnsavedChanges() ? "Just now" : "Saved");
        g.drawText (right, sbContent, juce::Justification::centredRight);

        if (bottomTab == BottomPanel::Page::Design)
        {
            g.setColour (PatchCraftLookAndFeel::border());
            g.fillRect (leftResizeHandle);
            g.fillRect (rightResizeHandle);
        }
    }

    void StudioMainComponent::resized()
    {
        auto r = getLocalBounds();

        menuBar.setBounds (r.removeFromTop (24));

        // Top toolbar
        topToolbar->setBounds (r.removeFromTop (72));

        // Status bar at bottom
        r.removeFromBottom (24);

        // Canvas toolbar (always visible - hosts the section tabs).
        const int canvasToolbarH = 32;
        const auto toolbarStrip = r.removeFromTop (canvasToolbarH);
        canvasToolbar->setBounds (toolbarStrip);

        const bool designTab = (bottomTab == BottomPanel::Page::Design);

        // Visibility: only Design shows sidebar / canvas / inspector.
        leftCollapseButton.setVisible (designTab);
        rightCollapseButton.setVisible (designTab);
        leftTabs.setVisible       (designTab && ! leftPanelCollapsed);
        // Left column (Elements / Library / Expansions). Layers moved out
        // to the right column.
        elementPalette->setVisible(designTab && ! leftPanelCollapsed
                                   && ! showLibraryInsteadOfElements
                                   && ! showExpansionsInsteadOfElements);
        assetLibraryPanel->setVisible (designTab && ! leftPanelCollapsed && showLibraryInsteadOfElements);
        expansionLibraryPanel->setVisible (designTab && ! leftPanelCollapsed && showExpansionsInsteadOfElements);

        // Right column (Inspector / Layers).
        const bool rightVisible = designTab && ! rightPanelCollapsed;
        rightTabs.setVisible (rightVisible);
        layersPanel->setVisible (rightVisible && showLayersInRightPanel);
        canvasEditor->setVisible  (designTab);
        inspectorPanel->setVisible(designTab && ! rightPanelCollapsed && ! showLayersInRightPanel);

        if (designTab)
        {
            // Bottom panel takes ~25% as in the original layout.
            const int bottomH = juce::jmax (210, r.getHeight() / 4);
            bottomPanel->setBounds (r.removeFromBottom (bottomH));

            leftPanelWidth = juce::jlimit (180, juce::jmax (180, getWidth() / 2), leftPanelWidth);
            inspectorPanelWidth = juce::jlimit (220, juce::jmax (220, getWidth() / 2), inspectorPanelWidth);

            if (leftPanelCollapsed)
            {
                auto collapsed = r.removeFromLeft (30);
                leftCollapseButton.setBounds (collapsed.removeFromTop (30).reduced (3));
                leftResizeHandle = {};
                r.removeFromLeft (4);
            }
            else
            {
                auto leftCol = r.removeFromLeft (leftPanelWidth);
                auto leftHeader = leftCol.removeFromTop (32);
                leftCollapseButton.setBounds (leftHeader.removeFromRight (28).reduced (3));
                leftTabs.setBounds (leftHeader);
                elementPalette->setBounds (leftCol);
                layersPanel->setBounds    (leftCol);
                assetLibraryPanel->setBounds (leftCol);
                expansionLibraryPanel->setBounds (leftCol);
                leftResizeHandle = r.removeFromLeft (5);
            }

            if (rightPanelCollapsed)
            {
                auto collapsed = r.removeFromRight (30);
                rightCollapseButton.setBounds (collapsed.removeFromTop (30).reduced (3));
                rightResizeHandle = {};
                r.removeFromRight (4);
            }
            else
            {
                auto rightCol = r.removeFromRight (inspectorPanelWidth);
                auto rightHeader = rightCol.removeFromTop (32);
                rightCollapseButton.setBounds (rightHeader.removeFromLeft (28).reduced (3));
                rightTabs.setBounds (rightHeader);
                // Inspector and Layers share the body — only the active one
                // is visible (rebuildPageVisibility() / our tab handler).
                inspectorPanel->setBounds (rightCol);
                layersPanel->setBounds (rightCol);
                rightResizeHandle = r.removeFromRight (5);
            }

            canvasEditor->setBounds (r);
            canvasEditor->fit();
        }
        else
        {
            leftResizeHandle = {};
            rightResizeHandle = {};
            // Non-Design tabs: bottom panel grows to fill the entire workspace.
            bottomPanel->setBounds (r);
        }

        canvasToolbar->refresh();
    }

    juce::StringArray StudioMainComponent::getMenuBarNames()
    {
        return { "File", "Design", "Help" };
    }

    juce::PopupMenu StudioMainComponent::getMenuForIndex (int, const juce::String& menuName)
    {
        juce::PopupMenu menu;
        if (menuName == "File")
        {
            menu.addItem (1001, "New Project");
            menu.addItem (1002, "Open Project...");
            menu.addSeparator();
            menu.addItem (1003, "Save Project");
            menu.addItem (1004, "Save Project As...");
            menu.addSeparator();
            menu.addItem (1005, "Import Samples...");
            menu.addItem (1006, "Import Background...");
            menu.addSeparator();
            menu.addItem (1007, "Export Pack...");
            menu.addItem (1008, "Send to Expansion Pack...");
            menu.addSeparator();
            menu.addItem (1009, "Settings...");
            menu.addItem (1010, "Quit");
        }
        else if (menuName == "Design")
        {
            const auto selectedCount = selectedElementIds.size();
            const bool hasSelection = selectedCount > 0;
            const bool canAlign = selectedCount >= 2;
            const bool canDistribute = selectedCount >= 3;

            menu.addItem (3001, "Duplicate Selection", hasSelection);
            menu.addItem (3002, "Delete Selection", hasSelection);
            menu.addSeparator();

            juce::PopupMenu groupMenu;
            groupMenu.addItem (3010, "Group Selection", hasSelection);
            groupMenu.addItem (3011, "Ungroup Selection", hasSelection);
            groupMenu.addItem (3012, "Remove From Container", hasSelection);
            menu.addSubMenu ("Containers / Groups", groupMenu);

            juce::PopupMenu layerMenu;
            layerMenu.addItem (3050, "Show Selection", hasSelection);
            layerMenu.addItem (3051, "Hide Selection", hasSelection);
            layerMenu.addSeparator();
            layerMenu.addItem (3052, "Lock Selection", hasSelection);
            layerMenu.addItem (3053, "Unlock Selection", hasSelection);
            menu.addSubMenu ("Layer State", layerMenu);

            juce::PopupMenu alignMenu;
            alignMenu.addItem (3020, "Align Left", canAlign);
            alignMenu.addItem (3021, "Align Horizontal Center", canAlign);
            alignMenu.addItem (3022, "Align Right", canAlign);
            alignMenu.addSeparator();
            alignMenu.addItem (3023, "Align Top", canAlign);
            alignMenu.addItem (3024, "Align Vertical Center", canAlign);
            alignMenu.addItem (3025, "Align Bottom", canAlign);
            alignMenu.addSeparator();
            alignMenu.addItem (3026, "Distribute Horizontally", canDistribute);
            alignMenu.addItem (3027, "Distribute Vertically", canDistribute);
            menu.addSubMenu ("Align / Distribute", alignMenu);

            juce::PopupMenu canvasAlignMenu;
            canvasAlignMenu.addItem (3060, "Left Edge", hasSelection);
            canvasAlignMenu.addItem (3061, "Horizontal Center", hasSelection);
            canvasAlignMenu.addItem (3062, "Right Edge", hasSelection);
            canvasAlignMenu.addSeparator();
            canvasAlignMenu.addItem (3063, "Top Edge", hasSelection);
            canvasAlignMenu.addItem (3064, "Vertical Center", hasSelection);
            canvasAlignMenu.addItem (3065, "Bottom Edge", hasSelection);
            canvasAlignMenu.addSeparator();
            canvasAlignMenu.addItem (3066, "Center Both Axes", hasSelection);
            menu.addSubMenu ("Align To Canvas", canvasAlignMenu);

            juce::PopupMenu precisionMenu;
            precisionMenu.addItem (3070, "Match Width", canAlign);
            precisionMenu.addItem (3071, "Match Height", canAlign);
            precisionMenu.addItem (3072, "Match Width + Height", canAlign);
            precisionMenu.addSeparator();
            precisionMenu.addItem (3073, "Snap Selection To Grid", hasSelection);
            menu.addSubMenu ("Transform Precision", precisionMenu);

            juce::PopupMenu styleMenu;
            styleMenu.addItem (3080, "Copy Style From Primary Selection", hasSelection);
            styleMenu.addItem (3081, "Paste Style To Selection", hasSelection && hasCopiedDesignStyle);
            styleMenu.addSeparator();
            styleMenu.addItem (3082, "Apply: Glass Panel", hasSelection);
            styleMenu.addItem (3083, "Apply: Gold Hardware", hasSelection);
            styleMenu.addItem (3084, "Apply: Minimal Flat", hasSelection);
            styleMenu.addItem (3085, "Apply: Neon Reactive", hasSelection);
            menu.addSubMenu ("Reusable Styles", styleMenu);

            juce::PopupMenu orderMenu;
            orderMenu.addItem (3030, "Bring to Front", hasSelection);
            orderMenu.addItem (3031, "Bring Forward", hasSelection);
            orderMenu.addItem (3032, "Send Backward", hasSelection);
            orderMenu.addItem (3033, "Send to Back", hasSelection);
            menu.addSubMenu ("Arrange Order", orderMenu);

            menu.addSeparator();
            menu.addItem (3040, "Show Grid", true, canvasEditor != nullptr && canvasEditor->isGridVisible());
            menu.addItem (3041, "Show Rulers", true, canvasEditor != nullptr && canvasEditor->areRulersVisible());
        }
        else if (menuName == "Help")
        {
            menu.addItem (2001, "DSP Builder Tutorial...");
            menu.addItem (2003, "Auto-Show Tutorials", true, getStudioTutorialsEnabled());
            menu.addSeparator();
            menu.addItem (2002, "Show Help Tooltips", true, project.getManifest().playerShowParameterGuidance);
        }
        return menu;
    }

    void StudioMainComponent::menuItemSelected (int menuItemID, int)
    {
        switch (menuItemID)
        {
            case 1001: newProject(); break;
            case 1002: openProject(); break;
            case 1003: saveProject(); break;
            case 1004: saveProjectAs(); break;
            case 1005: importSamples(); break;
            case 1006: importBackground(); break;
            case 1007: exportPack(); break;
            case 1008: sendToExpansionPack(); break;
            case 1009: openSettings(); break;
            case 1010: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 3001: duplicateSelected(); break;
            case 3002: deleteSelected(); break;
            case 3010: groupSelectedElements(); break;
            case 3011: ungroupSelectedElements(); break;
            case 3012: removeSelectedFromContainer(); break;
            case 3020: alignSelected ("left"); break;
            case 3021: alignSelected ("hcenter"); break;
            case 3022: alignSelected ("right"); break;
            case 3023: alignSelected ("top"); break;
            case 3024: alignSelected ("vcenter"); break;
            case 3025: alignSelected ("bottom"); break;
            case 3026: distributeSelected (true); break;
            case 3027: distributeSelected (false); break;
            case 3030: orderSelected ("front"); break;
            case 3031: orderSelected ("forward"); break;
            case 3032: orderSelected ("backward"); break;
            case 3033: orderSelected ("back"); break;
            case 3050: setSelectedVisibility (true); break;
            case 3051: setSelectedVisibility (false); break;
            case 3052: setSelectedLocked (true); break;
            case 3053: setSelectedLocked (false); break;
            case 3060: alignSelectedToCanvas ("left"); break;
            case 3061: alignSelectedToCanvas ("hcenter"); break;
            case 3062: alignSelectedToCanvas ("right"); break;
            case 3063: alignSelectedToCanvas ("top"); break;
            case 3064: alignSelectedToCanvas ("vcenter"); break;
            case 3065: alignSelectedToCanvas ("bottom"); break;
            case 3066: alignSelectedToCanvas ("center"); break;
            case 3070: matchSelectedSize ("width"); break;
            case 3071: matchSelectedSize ("height"); break;
            case 3072: matchSelectedSize ("both"); break;
            case 3073: snapSelectedToGrid(); break;
            case 3080: copySelectedDesignStyle(); break;
            case 3081: pasteDesignStyle(); break;
            case 3082: applyDesignStylePreset ("glass"); break;
            case 3083: applyDesignStylePreset ("gold"); break;
            case 3084: applyDesignStylePreset ("minimal"); break;
            case 3085: applyDesignStylePreset ("neon"); break;
            case 3040: toggleCanvasGrid(); break;
            case 3041: toggleCanvasRulers(); break;
            case 2001: showDspBuilderTutorial(); break;
            case 2002: toggleHelpTooltips(); break;
            case 2003:
                setStudioTutorialsEnabled (! getStudioTutorialsEnabled());
                break;
            default: break;
        }
    }

    void StudioMainComponent::toggleHelpTooltips()
    {
        auto& manifest = project.getManifest();
        manifest.playerShowParameterGuidance = ! manifest.playerShowParameterGuidance;
        project.notifyChanged();
        menuBar.repaint();
    }

    void StudioMainComponent::showDspBuilderTutorial()
    {
        if (bottomTab != BottomPanel::Page::DSP)
            setBottomTab (BottomPanel::Page::DSP);

        if (bottomPanel != nullptr)
            bottomPanel->showDspBuilderTutorial();
    }

    bool StudioMainComponent::getStudioTutorialsEnabled() const
    {
        return readStudioTutorialPreference (project.getManifest().studioShowTutorials);
    }

    void StudioMainComponent::setStudioTutorialsEnabled (bool enabled)
    {
        project.getManifest().studioShowTutorials = enabled;
        writeStudioTutorialPreference (enabled);
        project.notifyChanged();
        menuBar.repaint();
    }

    void StudioMainComponent::mouseDown (const juce::MouseEvent& e)
    {
        panelDragMode = PanelDragMode::None;
        if (bottomTab != BottomPanel::Page::Design)
            return;

        if (! leftPanelCollapsed && leftResizeHandle.expanded (3, 0).contains (e.getPosition()))
            panelDragMode = PanelDragMode::LeftPanel;
        else if (! rightPanelCollapsed && rightResizeHandle.expanded (3, 0).contains (e.getPosition()))
            panelDragMode = PanelDragMode::Inspector;
    }

    void StudioMainComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (panelDragMode == PanelDragMode::LeftPanel)
        {
            leftPanelWidth = juce::jlimit (180, getWidth() / 2, e.x);
            resized();
        }
        else if (panelDragMode == PanelDragMode::Inspector)
        {
            inspectorPanelWidth = juce::jlimit (220, getWidth() / 2, getWidth() - e.x);
            resized();
        }
    }

    void StudioMainComponent::mouseUp (const juce::MouseEvent&)
    {
        panelDragMode = PanelDragMode::None;
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void StudioMainComponent::mouseMove (const juce::MouseEvent& e)
    {
        if (bottomTab == BottomPanel::Page::Design
            && ((! leftPanelCollapsed && leftResizeHandle.expanded (3, 0).contains (e.getPosition()))
                || (! rightPanelCollapsed && rightResizeHandle.expanded (3, 0).contains (e.getPosition()))))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    // -------------------------------------------------------------------------
    // Top-level actions
    // -------------------------------------------------------------------------
    void StudioMainComponent::newProject()
    {
        project.resetToDefaultInstrument();
        selectedElementId.clear();
        selectedElementIds.clear();
        project.notifyChanged();
    }

    void StudioMainComponent::openProject()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Open PatchCraft project",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.patchcraftproject");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;
                juce::String err;
                if (! project.load (folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Open project")
                            .withMessage (err.isEmpty() ? "Failed to open project." : err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }
                project.notifyChanged();
            });
    }

    void StudioMainComponent::saveProject()
    {
        if (project.getProjectFolder().isDirectory())
        {
            juce::String err;
            if (! project.save (project.getProjectFolder(), err))
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Save project")
                        .withMessage (err.isEmpty() ? "Failed to save project." : err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
                return;
            }
            refreshAllPanels();
            return;
        }

        saveProjectAs();
    }

    void StudioMainComponent::saveProjectAs()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Save PatchCraft project as...",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (project.getManifest().instrumentName + ".patchcraftproject"));
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;
                juce::String err;
                if (! project.save (folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Save project")
                            .withMessage (err.isEmpty() ? "Failed to save project." : err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }
                refreshAllPanels();
            });
    }

    namespace
    {
        static juce::String sanitiseFileName (juce::String name)
        {
            name = name.trim();
            if (name.isEmpty())
                name = "Untitled";
            return name.replaceCharacters ("\\/:*?\"<>|", "_________");
        }

        static bool writeDspPatchJson (const PatchCraftProject& project,
                                       const juce::File& file,
                                       const juce::String& sectionId,
                                       const juce::String& patchName,
                                       juce::String& error)
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("format", "PatchCraft DSP Patch");
            root->setProperty ("formatVersion", 1);
            root->setProperty ("name", patchName);
            root->setProperty ("engine", project.getEngineType());
            root->setProperty ("section", sectionId);
            root->setProperty ("dspGraph", project.getDspGraph().toVar());

            if (! file.getParentDirectory().createDirectory())
            {
                error = "Could not create patch folder.";
                return false;
            }

            if (! file.replaceWithText (juce::JSON::toString (juce::var (root), true)))
            {
                error = "Could not write patch file.";
                return false;
            }

            return true;
        }

        static Preset makePlayablePresetFromCurrentPatch (PatchCraftProject& project,
                                                          const juce::String& name)
        {
            return project.captureCurrentPatch (name).toPreset();
        }

        static int findPresetIndexByName (PatchCraftProject& project, const juce::String& name)
        {
            auto& presets = project.getPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].name == name)
                    return i;
            return -1;
        }

        static int findPatchIndexById (PatchCraftProject& project, const juce::String& id)
        {
            auto& patches = project.getPatches();
            for (int i = 0; i < (int) patches.size(); ++i)
                if (patches[(size_t) i].id == id)
                    return i;
            return -1;
        }

        // Wire a saved patch into the active expansion (pack) so the patch
        // shows up in the pack's preset list automatically. Mirrors what
        // DspPage::addPatchPresetToExpansion does for Easy Mode preset
        // creation, but kept local here so the toolbar's Save Patch button
        // doesn't need to reach across into the DSP page.
        static void registerPatchWithExpansion (PatchCraftProject& project,
                                                  InstrumentPatch& patch,
                                                  Preset& preset)
        {
            // Pick (or create) a default expansion. Use the first existing
            // one if any; otherwise create one named after the instrument so
            // saving a patch into a fresh project still lands somewhere
            // sensible — that becomes the default pack on export.
            ExpansionMetadata* expansion = nullptr;
            if (! project.getExpansions().empty())
                expansion = &project.getExpansions().front();
            else
                expansion = &project.ensureExpansion (
                    project.getManifest().instrumentName.isNotEmpty()
                        ? project.getManifest().instrumentName + " Pack"
                        : juce::String ("Pack"));

            if (expansion == nullptr) return;

            patch.expansionId = expansion->id;
            patch.packId = expansion->id;
            preset.expansionId = expansion->id;
            preset.packId = expansion->id;

            const auto category = patch.category.isNotEmpty()
                ? patch.category : project.getManifest().category;
            if (category.isNotEmpty())
            {
                expansion->folders.addIfNotAlreadyThere (category);
                if (expansion->category.isEmpty())
                    expansion->category = category;
            }
            expansion->includedPatchIds.addIfNotAlreadyThere (patch.id);
            expansion->includedPresetNames.addIfNotAlreadyThere (preset.name);
            for (const auto& asset : patch.includedAssets)
                expansion->includedAssets.addIfNotAlreadyThere (asset);
        }

        static void showSaveError (const juce::String& title, const juce::String& message)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (message)
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
        }
    }

    void StudioMainComponent::saveCurrentDspPatch()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Patch", "Save the project first so the playable patch and layout changes can be written to disk.");
            return;
        }

        auto patchName = project.getManifest().defaultPreset;
        if (patchName.isEmpty())
            patchName = project.getManifest().instrumentName + " Patch";

        auto patch = project.captureCurrentPatch (patchName);
        patch.isDefault = true;
        auto preset = patch.toPreset();
        preset.isDefault = true;
        for (auto& existing : project.getPatches())
            existing.isDefault = false;
        for (auto& existing : project.getPresets())
            existing.isDefault = false;

        // Register the patch with the active expansion so it ships in the
        // pack on the next export, without the user needing to manually
        // attach it via the DSP page.
        registerPatchWithExpansion (project, patch, preset);

        const int existingPatchIndex = findPatchIndexById (project, patch.id);
        if (existingPatchIndex >= 0)
            project.getPatches()[(size_t) existingPatchIndex] = patch;
        else
            project.getPatches().push_back (patch);

        const int existingIndex = findPresetIndexByName (project, preset.name);
        if (existingIndex >= 0)
            project.getPresets()[(size_t) existingIndex] = preset;
        else
            project.getPresets().push_back (preset);

        project.getManifest().defaultPreset = preset.name;
        project.notifyChanged();

        juce::String error;
        if (! project.save (project.getProjectFolder(), error))
        {
            showSaveError ("Save Patch", error);
            return;
        }

        refreshAllPanels();
    }

    void StudioMainComponent::saveCurrentDspPatchAs()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Patch As", "Save the project first so the playable patch and layout changes can be written to disk.");
            return;
        }

        auto* window = new juce::AlertWindow ("Save Patch As",
            "Name this playable patch. It stores the full current instrument sound across Source, Filter, Amp, Mod, FX, and Output.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", project.getManifest().instrumentName + " Patch", "Patch Name:");
        window->addButton ("Save Patch", 1);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                const auto name = window->getTextEditorContents ("name").trim();
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result != 1 || name.isEmpty())
                    return;

                auto patch = project.captureCurrentPatch (name);
                patch.isDefault = true;
                auto preset = patch.toPreset();
                preset.isDefault = true;
                for (auto& existing : project.getPatches())
                    existing.isDefault = false;
                for (auto& existing : project.getPresets())
                    existing.isDefault = false;

                registerPatchWithExpansion (project, patch, preset);

                const int existingPatchIndex = findPatchIndexById (project, patch.id);
                if (existingPatchIndex >= 0)
                    project.getPatches()[(size_t) existingPatchIndex] = patch;
                else
                    project.getPatches().push_back (patch);

                const int existingIndex = findPresetIndexByName (project, preset.name);
                if (existingIndex >= 0)
                    project.getPresets()[(size_t) existingIndex] = preset;
                else
                    project.getPresets().push_back (preset);
                project.getManifest().defaultPreset = preset.name;
                project.notifyChanged();

                juce::String error;
                if (! project.save (project.getProjectFolder(), error))
                {
                    showSaveError ("Save Patch As", error);
                    return;
                }

                refreshAllPanels();
            }), true);
    }

    void StudioMainComponent::saveCurrentSectionPreset()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Section Preset", "Save the project first so section presets can be written to disk.");
            return;
        }

        const auto sectionId = bottomPanel != nullptr ? bottomPanel->getDspPatchSectionId() : juce::String ("source");
        const auto sectionLabel = bottomPanel != nullptr ? bottomPanel->getDspPatchSectionLabel() : juce::String ("Source");
        auto* window = new juce::AlertWindow ("Save Section Preset",
            "Name this reusable " + sectionLabel + " section. It stores only this section's blocks, routes, automation, and parameter values.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", sectionLabel + " Preset", "Section Preset Name:");
        window->addButton ("Save Section", 1);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window, sectionId] (int result)
            {
                const auto name = window->getTextEditorContents ("name").trim();
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result != 1 || name.isEmpty())
                    return;

                auto preset = project.captureSectionPreset (sectionId, name);
                auto& presets = project.getSectionPresets();
                bool replaced = false;
                for (auto& existing : presets)
                {
                    if (existing.id == preset.id)
                    {
                        existing = preset;
                        replaced = true;
                        break;
                    }
                }
                if (! replaced)
                    presets.push_back (std::move (preset));
                project.notifyChanged();

                juce::String error;
                if (! project.save (project.getProjectFolder(), error))
                {
                    showSaveError ("Save Section Preset", error);
                    return;
                }
                refreshAllPanels();
            }), true);
    }

    void StudioMainComponent::sendToExpansionPack()
    {
        if (showSampleExportValidationIfBlocked (*this, project, "Send to Expansion Pack"))
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose Expansion Pack folder",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto expansionFolder = fc.getResult();
                if (expansionFolder == juce::File()) return;

                const auto safeName = project.getManifest().instrumentName
                    .replaceCharacters ("\\/:*?\"<>|", "_________");
                auto packFolder = expansionFolder.getChildFile (safeName + ".patchcraft");
                auto& expansion = project.ensureExpansion (expansionFolder.getFileName().isNotEmpty()
                    ? expansionFolder.getFileName()
                    : project.getManifest().instrumentName + " Expansion");
                auto patch = project.captureCurrentPatch (project.getManifest().defaultPreset.isNotEmpty()
                    ? project.getManifest().defaultPreset
                    : project.getManifest().instrumentName + " Patch");
                patch.expansionId = expansion.id;
                patch.isDefault = true;
                for (auto& existing : project.getPatches())
                    existing.isDefault = false;
                const int existingPatchIndex = findPatchIndexById (project, patch.id);
                if (existingPatchIndex >= 0)
                    project.getPatches()[(size_t) existingPatchIndex] = patch;
                else
                    project.getPatches().push_back (patch);

                auto preset = patch.toPreset();
                preset.expansionId = expansion.id;
                preset.isDefault = true;
                for (auto& existing : project.getPresets())
                    existing.isDefault = false;
                const int existingPresetIndex = findPresetIndexByName (project, preset.name);
                if (existingPresetIndex >= 0)
                    project.getPresets()[(size_t) existingPresetIndex] = preset;
                else
                    project.getPresets().push_back (preset);

                expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
                expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
                for (const auto& asset : patch.includedAssets)
                    expansion.includedAssets.addIfNotAlreadyThere (asset);
                project.notifyChanged();

                PatchCraftPackWriter writer;
                juce::String err;
                const auto warnings = validationWarningSummary (project);
                if (! writer.write (project, packFolder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Send to Expansion Pack")
                            .withMessage ("Send failed: " + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }

                auto* expansionRoot = new juce::DynamicObject();
                expansionRoot->setProperty ("format", "PatchCraft Expansion");
                expansionRoot->setProperty ("formatVersion", 1);
                expansionRoot->setProperty ("metadata", expansion.toVar());
                juce::Array<juce::var> packs;
                packs.add (packFolder.getFileName());
                expansionRoot->setProperty ("includedPacks", packs);
                expansionFolder.createDirectory();
                expansionFolder.getChildFile ("expansion.json")
                    .replaceWithText (juce::JSON::toString (juce::var (expansionRoot), true));

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Send to Expansion Pack")
                        .withMessage ("Added instrument pack:\n" + packFolder.getFullPathName() + warnings)
                        .withButton ("OK")
                        .withIconType (warnings.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon
                                                             : juce::MessageBoxIconType::InfoIcon),
                    nullptr);
            });
    }

    void StudioMainComponent::importSamples()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import samples", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectMultipleItems
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                int baseNote = 24; // C0 fallback when filenames/audio do not expose pitch.
                constexpr int zoneSize = 1;
                bool anyParsedRoot = false;
                bool anyNamePitch = false;
                bool anyAudioPitch = false;
                std::vector<SampleZoneDef> importedZones;
                importedZones.reserve ((size_t) files.size());
                for (auto& f : files)
                {
                    const int fallbackRoot = juce::jlimit (0, 127, baseNote);
                    bool usedNamePitch = false;
                    bool usedAudioPitch = false;
                    auto z = SampleMap::inferZoneFromFileWithAudio (f,
                                                                    fallbackRoot,
                                                                    baseNote,
                                                                    juce::jmin (127, baseNote + zoneSize - 1),
                                                                    &usedNamePitch,
                                                                    &usedAudioPitch);
                    anyParsedRoot = anyParsedRoot || usedNamePitch || usedAudioPitch;
                    anyNamePitch = anyNamePitch || usedNamePitch;
                    anyAudioPitch = anyAudioPitch || usedAudioPitch;
                    importedZones.push_back (z);
                    baseNote += zoneSize;
                    if (baseNote > 108) baseNote = 24;
                }
                std::map<int, int> initialRootCounts;
                for (const auto& zone : importedZones)
                    ++initialRootCounts[zone.rootNote];

                if (! anyNamePitch
                    && anyAudioPitch
                    && initialRootCounts.size() <= 1
                    && importedZones.size() > 1)
                {
                    int fallbackNote = 24;
                    for (auto& zone : importedZones)
                    {
                        zone.rootNote = fallbackNote;
                        zone.lowNote = fallbackNote;
                        zone.highNote = fallbackNote;
                        if (++fallbackNote > 108)
                            fallbackNote = 24;
                    }
                    anyParsedRoot = false;
                }

                std::map<int, int> rootCounts;
                for (const auto& zone : importedZones)
                    ++rootCounts[zone.rootNote];
                std::map<int, int> rootRoundRobinIndex;
                for (auto& zone : importedZones)
                {
                    const bool stackedRoot = rootCounts[zone.rootNote] > 1;
                    const bool noVelocityLayer = zone.lowVelocity == 1 && zone.highVelocity == 127;
                    const bool noRoundRobin = zone.roundRobinGroup == 0 && zone.roundRobinIndex == 0;
                    if (stackedRoot && noVelocityLayer && noRoundRobin)
                    {
                        zone.roundRobinGroup = 1;
                        zone.roundRobinIndex = ++rootRoundRobinIndex[zone.rootNote];
                    }
                    project.getSampleMap().add (zone);
                }
                if (anyParsedRoot)
                    project.getSampleMap().autoMapByRootNotes();
                if (! files.isEmpty() && project.getEngineType() != "sample")
                    project.setEngineType ("sample");
                else
                    project.notifyChanged();
            });
    }

    void StudioMainComponent::importBackground()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import background image", juce::File(), "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;

                if (project.getProjectFolder().isDirectory())
                {
                    auto dst = project.getAssetsFolder().getChildFile ("background.png");
                    project.getAssetsFolder().createDirectory();
                    f.copyFileTo (dst);
                    project.backgroundImageRelative = "assets/background.png";
                    project.getManifest().backgroundImage = project.backgroundImageRelative;
                }
                else
                {
                    project.backgroundImageRelative = f.getFullPathName();
                    project.getManifest().backgroundImage = project.backgroundImageRelative;
                }
                if (auto* bg = project.getLayout().find ("background"))
                    bg->asset.clear();
                assets.clear();
                project.notifyChanged();
            });
    }

    void StudioMainComponent::aiAssist()
    {
        auto* aw = new juce::AlertWindow ("PatchCraft Copilot",
            "Local preview-first copilot. It reads project context and proposes guidance without changing your project.",
            juce::MessageBoxIconType::NoIcon);

        aw->addTextBlock ("Pick a copilot action:");
        aw->addTextBlock (AiAssistService::providerStatusText());
        for (auto t : AiAssistService::defaultTasks())
            aw->addButton (AiAssistService::displayName (t), (int) t);
        aw->addButton ("Local AI Settings", 9001);
        aw->addButton ("Close", -1);

        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int which)
            {
                std::unique_ptr<juce::AlertWindow> own (aw);
                if (which < 0) return;
                if (which == 9001)
                {
                    openSettings();
                    return;
                }
                const auto proposal = ai.run ((AiAssistService::TaskType) which, project);

                auto* result = new juce::AlertWindow (
                    proposal.title,
                    juce::String(), juce::MessageBoxIconType::NoIcon);
                result->addTextBlock (proposal.summary + "\n\nContext pack:\n" + proposal.contextSummary);
                result->addTextEditor ("preview", proposal.details, "Preview", false);
                if (auto* editor = result->getTextEditor ("preview"))
                {
                    editor->setMultiLine (true);
                    editor->setReadOnly (true);
                    editor->setScrollbarsShown (true);
                    editor->setCaretVisible (false);
                    editor->setSize (760, 360);
                }
                result->addButton ("Copy Preview", 1);
                result->addButton ("Close", 0);
                result->enterModalState (true,
                    juce::ModalCallbackFunction::create ([result, text = proposal.details] (int action)
                    {
                        if (action == 1)
                            juce::SystemClipboard::copyTextToClipboard (text);
                        std::unique_ptr<juce::AlertWindow> r (result);
                    }), true);
            }), true);
    }

    void StudioMainComponent::deleteSelected()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Delete selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    juce::StringArray tabGroups;
                    if (el->type == ElementType::TabPanel)
                        for (const auto& tab : el->tabs)
                            tabGroups.addIfNotAlreadyThere (studioScopedTabGroupId (*el, tab));

                    for (auto& candidate : m.getAll())
                    {
                        if (candidate.id == id)
                            continue;
                        if (candidate.containerId == id)
                            candidate.containerId.clear();
                        if (candidate.groupId == id || tabGroups.contains (candidate.groupId))
                            candidate.groupId.clear();
                    }
                    m.remove (id);
                }
        });
        clearSelection();
    }

    void StudioMainComponent::duplicateSelected()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        juce::StringArray newIds;
        std::map<juce::String, juce::String> idRemap;
        project.performLayoutEdit ("Duplicate selection", [&] (LayoutModel& m)
        {
            // First pass: copy every selected element and record old->new id.
            for (const auto& id : ids)
            {
                if (auto* el = m.find (id))
                {
                    LayoutElement copy = *el;
                    const auto oldId = copy.id;
                    copy.id.clear();          // forces add() to mint a unique id
                    copy.x += 16; copy.y += 16;
                    auto& added = m.add (copy);
                    idRemap[oldId] = added.id;
                    newIds.add (added.id);
                }
            }

            // Second pass: rewrite container/group references that point to
            // siblings inside the duplicated set, so a duplicated Group keeps
            // owning its (now duplicated) children. References that point
            // outside the set are left alone.
            for (const auto& newId : newIds)
            {
                if (auto* el = m.find (newId))
                {
                    if (auto it = idRemap.find (el->containerId); it != idRemap.end())
                        el->containerId = it->second;
                    if (auto it = idRemap.find (el->groupId); it != idRemap.end())
                        el->groupId = it->second;
                }
            }
        });
        if (! newIds.isEmpty())
            setSelectedElementIds (newIds);
    }

    void StudioMainComponent::groupSelectedElements()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        juce::String groupId;
        project.performLayoutEdit ("Group selection", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            juce::String commonContainer;
            juce::String commonGroup;
            bool first = true;

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && el->type != ElementType::Group)
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
            group.id = m.generateUniqueId ("group_");
            group.label = "New Group";
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
                    el->containerId = groupId;
        });

        if (groupId.isNotEmpty())
            setSelectedElementId (groupId);
        refreshAllPanels();
    }

    void StudioMainComponent::ungroupSelectedElements()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Ungroup selection", [&] (LayoutModel& m)
        {
            juce::StringArray groupsToRemove;
            for (const auto& id : ids)
            {
                if (auto* el = m.find (id))
                {
                    if (el->type == ElementType::Group)
                    {
                        for (auto& child : m.getAll())
                            if (child.containerId == id)
                                child.containerId = el->containerId;
                        groupsToRemove.addIfNotAlreadyThere (id);
                    }
                    else
                    {
                        el->containerId.clear();
                    }
                }
            }

            for (const auto& id : groupsToRemove)
                m.remove (id);
        });

        clearSelection();
        refreshAllPanels();
    }

    void StudioMainComponent::removeSelectedFromContainer()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Remove from container", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    el->containerId.clear();
        });
        refreshAllPanels();
    }

    void StudioMainComponent::toggleCanvasGrid()
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->setGridVisible (! canvasEditor->isGridVisible());
        menuBar.repaint();
    }

    void StudioMainComponent::toggleCanvasRulers()
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->setRulersVisible (! canvasEditor->areRulersVisible());
        resized();
        menuBar.repaint();
    }

    void StudioMainComponent::alignSelected (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 2) return;

        project.performLayoutEdit ("Align selection", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                    bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                }

            if (bounds.isEmpty()) return;

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    if (command == "left")    el->x = bounds.getX();
                    if (command == "hcenter") el->x = bounds.getCentreX() - el->width / 2;
                    if (command == "right")   el->x = bounds.getRight() - el->width;
                    if (command == "top")     el->y = bounds.getY();
                    if (command == "vcenter") el->y = bounds.getCentreY() - el->height / 2;
                    if (command == "bottom")  el->y = bounds.getBottom() - el->height;
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::alignSelectedToCanvas (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        const auto canvas = project.getCanvasSize();
        project.performLayoutEdit ("Align selection to canvas", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    const auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                    bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                }

            if (bounds.isEmpty()) return;

            int dx = 0;
            int dy = 0;
            if (command == "left")         dx = -bounds.getX();
            else if (command == "hcenter") dx = canvas.width / 2 - bounds.getCentreX();
            else if (command == "right")   dx = canvas.width - bounds.getRight();
            else if (command == "top")     dy = -bounds.getY();
            else if (command == "vcenter") dy = canvas.height / 2 - bounds.getCentreY();
            else if (command == "bottom")  dy = canvas.height - bounds.getBottom();
            else if (command == "center")
            {
                dx = canvas.width / 2 - bounds.getCentreX();
                dy = canvas.height / 2 - bounds.getCentreY();
            }

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    el->x += dx;
                    el->y += dy;
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::distributeSelected (bool horizontal)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 3) return;

        project.performLayoutEdit ("Distribute selection", [&] (LayoutModel& m)
        {
            std::vector<LayoutElement*> elements;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    elements.push_back (el);

            if (elements.size() < 3) return;

            std::sort (elements.begin(), elements.end(), [horizontal] (const LayoutElement* a, const LayoutElement* b)
            {
                return horizontal ? a->x < b->x : a->y < b->y;
            });

            const auto firstStart = horizontal ? elements.front()->x : elements.front()->y;
            const auto lastEnd = horizontal ? elements.back()->x + elements.back()->width
                                            : elements.back()->y + elements.back()->height;
            int totalSize = 0;
            for (const auto* el : elements)
                totalSize += horizontal ? el->width : el->height;

            const int gap = (lastEnd - firstStart - totalSize) / juce::jmax (1, (int) elements.size() - 1);
            int cursor = firstStart;
            for (auto* el : elements)
            {
                if (horizontal)
                {
                    el->x = cursor;
                    cursor += el->width + gap;
                }
                else
                {
                    el->y = cursor;
                    cursor += el->height + gap;
                }
            }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::matchSelectedSize (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 2) return;

        LayoutElement reference;
        bool hasReference = false;
        if (auto* selected = project.getLayout().find (selectedElementId);
            selected != nullptr && ! selected->locked)
        {
            reference = *selected;
            hasReference = true;
        }
        if (! hasReference)
            return;

        project.performLayoutEdit ("Match selection size", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (id != reference.id)
                    if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    {
                        if (command == "width" || command == "both")
                            el->width = reference.width;
                        if (command == "height" || command == "both")
                            el->height = reference.height;
                    }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::snapSelectedToGrid()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty() || canvasEditor == nullptr) return;
        const int grid = juce::jmax (1, canvasEditor->getSnapGrid());

        auto snap = [grid] (int value)
        {
            return juce::roundToInt ((float) value / (float) grid) * grid;
        };

        project.performLayoutEdit ("Snap selection to grid", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    el->x = snap (el->x);
                    el->y = snap (el->y);
                    el->width = juce::jmax (1, snap (el->width));
                    el->height = juce::jmax (1, snap (el->height));
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::setSelectedVisibility (bool visible)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit (visible ? "Show selection" : "Hide selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr)
                    el->visible = visible;
        });
        refreshAllPanels();
    }

    void StudioMainComponent::setSelectedLocked (bool locked)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit (locked ? "Lock selection" : "Unlock selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr)
                    el->locked = locked;
        });
        refreshAllPanels();
    }

    namespace
    {
        static void copyDesignStyleFields (LayoutElement& target, const LayoutElement& source)
        {
            target.style = source.style;
            target.knobStyle = source.knobStyle;
            target.shapeKind = source.shapeKind;
            target.opacity = source.opacity;
            target.cornerRadius = source.cornerRadius;
            target.strokeWidth = source.strokeWidth;
            target.shadowAmount = source.shadowAmount;
            target.glowAmount = source.glowAmount;
            target.blurAmount = source.blurAmount;
            target.textColour = source.textColour;
            target.accentColour = source.accentColour;
            target.borderColour = source.borderColour;
            target.backgroundColour = source.backgroundColour;
            target.labelPosition = source.labelPosition;
            target.labelOffsetX = source.labelOffsetX;
            target.labelOffsetY = source.labelOffsetY;
            target.labelSpacing = source.labelSpacing;
            target.labelSize = source.labelSize;
            target.audioReactive = source.audioReactive;
            target.audioReactiveMode = source.audioReactiveMode;
            target.audioReactiveAmount = source.audioReactiveAmount;
            target.animationMode = source.animationMode;
            target.animationRate = source.animationRate;
        }

        static void applyBuiltInDesignStyle (LayoutElement& target, const juce::String& presetId)
        {
            if (presetId == "glass")
            {
                target.style = "Glass";
                target.opacity = 0.86f;
                target.cornerRadius = 18.0f;
                target.strokeWidth = 1.5f;
                target.shadowAmount = 0.28f;
                target.glowAmount = 0.18f;
                target.blurAmount = 0.20f;
                target.backgroundColour = juce::Colour (0x66242a34);
                target.borderColour = juce::Colour (0x99c6d4ff);
                target.accentColour = juce::Colour (0xff8fb6ff);
            }
            else if (presetId == "gold")
            {
                target.style = "Vintage Gold";
                target.opacity = 1.0f;
                target.cornerRadius = 14.0f;
                target.strokeWidth = 2.0f;
                target.shadowAmount = 0.34f;
                target.glowAmount = 0.20f;
                target.blurAmount = 0.0f;
                target.backgroundColour = juce::Colour (0xff18120a);
                target.borderColour = juce::Colour (0xffb9872d);
                target.accentColour = juce::Colour (0xfff5a623);
            }
            else if (presetId == "minimal")
            {
                target.style = "Minimal Flat";
                target.opacity = 1.0f;
                target.cornerRadius = 8.0f;
                target.strokeWidth = 1.0f;
                target.shadowAmount = 0.0f;
                target.glowAmount = 0.0f;
                target.blurAmount = 0.0f;
                target.backgroundColour = juce::Colour (0xff15171b);
                target.borderColour = juce::Colour (0xff30343b);
                target.accentColour = juce::Colour (0xffc8ccd4);
            }
            else if (presetId == "neon")
            {
                target.style = "Space Blue";
                target.opacity = 0.94f;
                target.cornerRadius = 16.0f;
                target.strokeWidth = 2.0f;
                target.shadowAmount = 0.18f;
                target.glowAmount = 0.62f;
                target.blurAmount = 0.10f;
                target.backgroundColour = juce::Colour (0xff07111e);
                target.borderColour = juce::Colour (0xff186fba);
                target.accentColour = juce::Colour (0xff2bd6ff);
                target.audioReactive = true;
                target.audioReactiveMode = "level";
                target.audioReactiveAmount = 0.35f;
                target.animationMode = "glow";
                target.animationRate = 0.45f;
            }
        }
    }

    void StudioMainComponent::copySelectedDesignStyle()
    {
        if (auto* el = project.getLayout().find (selectedElementId))
        {
            copiedDesignStyle = *el;
            hasCopiedDesignStyle = true;
            menuBar.repaint();
        }
    }

    void StudioMainComponent::pasteDesignStyle()
    {
        if (! hasCopiedDesignStyle || selectedElementIds.isEmpty())
            return;

        const auto ids = selectedElementIds;
        project.performLayoutEdit ("Paste design style", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    copyDesignStyleFields (*el, copiedDesignStyle);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::applyDesignStylePreset (const juce::String& presetId)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Apply design style", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    applyBuiltInDesignStyle (*el, presetId);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::orderSelected (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Reorder selection", [&] (LayoutModel& m)
        {
            auto elementBounds = [] (const LayoutElement& e)
            {
                return juce::Rectangle<int> (e.x, e.y, e.width, e.height);
            };

            auto visuallyRelated = [&] (const LayoutElement& a, const LayoutElement& b)
            {
                if (a.id == b.id || ! a.visible || ! b.visible)
                    return false;
                if (a.containerId != b.containerId)
                    return false;
                if (a.groupId != b.groupId)
                    return false;
                return elementBounds (a).intersects (elementBounds (b));
            };

            auto moveBackwardBehindOverlap = [&] (const juce::String& id)
            {
                auto idx = m.findIndex (id);
                auto& all = m.getAll();
                if (idx <= 0 || idx >= (int) all.size())
                    return;

                int target = -1;
                for (int i = idx - 1; i >= 0; --i)
                    if (visuallyRelated (all[(size_t) idx], all[(size_t) i]))
                    {
                        target = i;
                        break;
                    }

                if (target < 0)
                    m.sendBackward (id);
                else
                    while (idx > target)
                    {
                        std::swap (all[(size_t) idx], all[(size_t) idx - 1]);
                        --idx;
                    }
            };

            auto moveForwardInFrontOfOverlap = [&] (const juce::String& id)
            {
                auto idx = m.findIndex (id);
                auto& all = m.getAll();
                if (idx < 0 || idx >= (int) all.size() - 1)
                    return;

                int target = -1;
                for (int i = idx + 1; i < (int) all.size(); ++i)
                    if (visuallyRelated (all[(size_t) idx], all[(size_t) i]))
                    {
                        target = i;
                        break;
                    }

                if (target < 0)
                    m.bringForward (id);
                else
                    while (idx < target)
                    {
                        std::swap (all[(size_t) idx], all[(size_t) idx + 1]);
                        ++idx;
                    }
            };

            if (command == "forward")
                for (const auto& id : ids) moveForwardInFrontOfOverlap (id);
            else if (command == "backward")
                for (int i = ids.size(); --i >= 0;) moveBackwardBehindOverlap (ids[i]);
            else if (command == "front")
                for (const auto& id : ids)
                    for (int guard = 0; guard < (int) m.getAll().size(); ++guard) m.bringForward (id);
            else if (command == "back")
                for (int i = ids.size(); --i >= 0;)
                    for (int guard = 0; guard < (int) m.getAll().size(); ++guard) m.sendBackward (ids[i]);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::undo() { project.undo(); }
    void StudioMainComponent::redo() { project.redo(); }

    bool StudioMainComponent::isPanelFloating (juce::Component* panel) const
    {
        for (const auto& f : floatingPanels)
            if (f.panel == panel) return true;
        return false;
    }

    void StudioMainComponent::togglePanelFloat (juce::Component* panel, juce::String title)
    {
        if (panel == nullptr) return;

        // Already floating? Bring focus + close — restoring it to the host.
        for (auto it = floatingPanels.begin(); it != floatingPanels.end(); ++it)
        {
            if (it->panel == panel)
            {
                auto* originalParent = it->originalParent;
                if (originalParent != nullptr)
                {
                    originalParent->addAndMakeVisible (panel);
                    originalParent->resized();
                }
                // Tear down the window after we've reparented.
                it->window.reset();
                floatingPanels.erase (it);
                resized();
                repaint();
                return;
            }
        }

        // Not floating: stash original parent, open a window with the panel
        // as content. Closing the window re-docks via the lambda above.
        auto* originalParent = panel->getParentComponent();
        FloatingEntry entry;
        entry.panel = panel;
        entry.originalParent = originalParent;
        entry.window = std::make_unique<FloatingPanelWindow> (
            title, panel,
            [this, panel]
            {
                // Defer so we never destroy the window from inside its own
                // closeButtonPressed callback.
                juce::MessageManager::callAsync ([this, panel]
                {
                    togglePanelFloat (panel, juce::String());
                });
            });
        floatingPanels.push_back (std::move (entry));

        // Host now has a hole where the panel was — re-flow.
        if (originalParent != nullptr)
            originalParent->resized();
        resized();
        repaint();
    }

    void StudioMainComponent::togglePreview()
    {
        if (! bottomPanel) return;

        const bool wasActive = bottomPanel->isPreviewActive();
        bottomPanel->setPreviewActive (! wasActive);
        topToolbar->setPreviewActive (! wasActive);
    }

    void StudioMainComponent::beginMidiLearn (juce::String parameterId)
    {
        auto* def = project.getParameters().find (parameterId);
        if (def == nullptr || ! def->midiLearnable)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("MIDI Learn")
                    .withMessage ("Choose a MIDI-learnable parameter first.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
            return;
        }

        pendingMidiLearnParameter = parameterId;
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("MIDI Learn")
                .withMessage ("Move a hardware knob, wheel, aftertouch, or expression control to map it to:\n"
                              + def->name + " (" + def->id + ")")
                .withButton ("OK")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            nullptr);
    }

    bool StudioMainComponent::captureMidiLearnMessage (const juce::MidiMessage& message)
    {
        if (pendingMidiLearnParameter.isEmpty())
            return false;

        auto* def = project.getParameters().find (pendingMidiLearnParameter);
        if (def == nullptr || ! def->midiLearnable)
        {
            pendingMidiLearnParameter.clear();
            return false;
        }

        MidiMapping mapping;
        mapping.id = "midi_" + def->id;
        mapping.parameterId = def->id;
        mapping.channel = message.getChannel();
        mapping.targetMin = def->min;
        mapping.targetMax = def->max;

        if (message.isController())
        {
            mapping.sourceType = "cc";
            mapping.controller = message.getControllerNumber();
        }
        else if (message.isPitchWheel())
        {
            mapping.sourceType = "pitchWheel";
            mapping.bipolar = true;
        }
        else if (message.isAftertouch())
        {
            mapping.sourceType = "aftertouch";
        }
        else if (message.isChannelPressure())
        {
            mapping.sourceType = "channelPressure";
        }
        else
        {
            return false;
        }

        auto& mappings = project.getMidiMappings();
        mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
            [&] (const MidiMapping& existing)
            {
                return existing.parameterId == mapping.parameterId
                    || (existing.sourceType == mapping.sourceType
                        && existing.channel == mapping.channel
                        && existing.controller == mapping.controller);
            }), mappings.end());
        mappings.push_back (mapping);
        pendingMidiLearnParameter.clear();
        project.notifyChanged();
        refreshAllPanels();

        juce::String sourceLabel = mapping.sourceType;
        if (mapping.sourceType == "cc")
            sourceLabel += " " + juce::String (mapping.controller);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("MIDI Learn")
                .withMessage ("Mapped " + sourceLabel + " on channel "
                              + juce::String (mapping.channel) + " to "
                              + def->name + " (" + def->id + ").")
                .withButton ("OK")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            nullptr);
        return true;
    }

    void StudioMainComponent::exportPack()
    {
        if (showSampleExportValidationIfBlocked (*this, project, "Export Pack"))
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Export PatchCraft pack",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (project.getManifest().instrumentName + ".patchcraft"),
            "");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;

                PatchCraftPackWriter writer;
                juce::String err;
                const auto warnings = validationWarningSummary (project);
                if (! writer.write (project, folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Export Pack")
                            .withMessage ("Export failed: " + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Export Pack")
                        .withMessage ("Exported to:\n" + folder.getFullPathName() + warnings)
                        .withButton ("OK")
                        .withIconType (warnings.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon
                                                             : juce::MessageBoxIconType::InfoIcon),
                    nullptr);
            });
    }

} // namespace patchcraft
