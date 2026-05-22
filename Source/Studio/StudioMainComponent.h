#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PatchCraftProject.h"
#include "AssetManager.h"
#include "AiAssistService.h"
#include "StudioAudioService.h"
#include "BottomPanel.h"

namespace patchcraft
{
    class TopToolbar;
    class ElementPalette;
    class BuiltAssetLibraryComponent;
    class ExpansionLibraryPanel;
    class LayersPanel;
    class FloatingPanelWindow;
    class CanvasEditor;
    class CanvasToolbar;
    class InspectorPanel;
    class BottomPanel;
    class PresetsComponent;

    /**
        Top-level Studio layout: top toolbar + left sidebar + canvas + inspector
        + bottom workspace + status bar. Selection state lives here.
    */
    class StudioMainComponent : public juce::Component,
                                public juce::DragAndDropContainer,
                                public juce::MenuBarModel,
                                private PatchCraftProject::Listener,
                                private LiveValueStore::Listener
    {
    public:
        StudioMainComponent();
        ~StudioMainComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        juce::StringArray getMenuBarNames() override;
        juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
        void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

        // ---- Project / pack actions ----------------------------------------
        void newProject();
        void openProject();
        void loadFactoryDemo (const juce::File& demoPackFolder);
        void saveProject();
        void saveProjectAs();
        void saveCurrentDspPatch();
        void saveCurrentDspPatchAs();
        void saveCurrentSectionPreset();
        void sendToExpansionPack();
        void importSamples();
        void importSampleFiles (const juce::Array<juce::File>& files,
                                bool switchToMapper = true,
                                bool spanMappedRoots = true);
        void toggleSampleLibraryDrawerForSamples();
        void showSampleLibraryDrawer (bool shouldShow);
        void importBackground();
        void generateAiBackground();
        void generateAiImageAsset();
        void aiAssist();
        void togglePreview();
        void exportPack();
        void exportVstPlugin();
        void publishToPluginClub();
        // Drops an arpeggiator block into the DSP graph and switches to
        // the MIDI builder so the user can edit it - hooked up from the
        // canvas right-click menu.
        void addArpBlock();
        void restoreAllPresets();
        void setDefaultPreset();

        // ---- Canvas / selection state --------------------------------------
        void setSelectedElementId (juce::String id);
        juce::String getSelectedElementId() const           { return selectedElementId; }
        const juce::StringArray& getSelectedElementIds() const { return selectedElementIds; }
        bool isElementSelected (const juce::String& id) const;
        void setSelectedElementIds (juce::StringArray ids);
        void toggleSelectedElementId (const juce::String& id);
        void clearSelection();

        // Edit operations (undoable).
        void deleteSelected();
        void duplicateSelected();
        void copySelectedElements (bool includeParameters);
        void pasteCopiedElements();
        void copySelectedToAllTabs();
        void setCopySelectionToTabsAsReference (bool shouldLink) { copySelectionToTabsAsReference = shouldLink; }
        bool getCopySelectionToTabsAsReference() const noexcept  { return copySelectionToTabsAsReference; }
        void propagateLinkedElementChange (const juce::String& sourceId);
        bool hasCopiedElements() const                    { return ! copiedLayoutElements.empty(); }
        void groupSelectedElements();
        void ungroupSelectedElements();
        void removeSelectedFromContainer();
        void alignSelected (const juce::String& command);
        void alignSelectedToCanvas (const juce::String& command);
        void distributeSelected (bool horizontal);
        void matchSelectedSize (const juce::String& command);
        void orderSelected (const juce::String& command);
        void snapSelectedToGrid();
        void setSelectedVisibility (bool visible);
        void setSelectedLocked (bool locked);
        void detachLabelsFromSelectedControls();
        void copySelectedDesignStyle();
        void pasteDesignStyle();
        void applyDesignStylePreset (const juce::String& presetId);
        void addElementToCanvas (ElementType type, juce::String parameterId = {});
        void addMixerChannelToCanvas();
        void addDrumMachineControlsToCanvas();
        void toggleCanvasGrid();
        void toggleCanvasRulers();
        void setCanvasZoom (float zoomFactor);
        void fitCanvasToWindow();
        void hideAllWindows();
        void showMainWindows();
        void dockAllFloatingPanels();
        void addLibraryAssetToCanvas (const juce::String& category, const juce::File& file,
                                      int frames, bool vertical,
                                      juce::Point<int> canvasPosition = juce::Point<int> (-1, -1));
        void undo();
        void redo();

        // Toggle a docked panel between its docked position and a free
        // floating window. The panel is re-parented in JUCE's component
        // tree; when the floating window closes, the panel returns to the
        // first parent it had at the time of floating.
        void togglePanelFloat (juce::Component* panel, juce::String title);
        bool isPanelFloating (juce::Component* panel) const;

        PatchCraftProject& getProject()                     { return project; }
        AssetManager&      getAssets()                      { return assets; }
        StudioAudioService& getAudio()                      { return audioService; }
        CanvasEditor* getCanvasEditor()                     { return canvasEditor.get(); }
        const CanvasEditor* getCanvasEditor() const         { return canvasEditor.get(); }
        bool isPreviewActive() const;
        const SampleZoneDef* getSelectedSampleZone() const;
        void openSettings();
        void showDspBuilderTutorial();
        bool getStudioTutorialsEnabled() const;
        void setStudioTutorialsEnabled (bool enabled);
        void toggleHelpTooltips();
        bool captureTutorialScreenshots (const juce::File& outputFolder, juce::String& error);

        // Section tab routing - canvas toolbar + bottom panel both consult this.
        void setBottomTab (BottomPanel::Page);
        BottomPanel::Page getBottomTab() const              { return bottomTab; }

        // Re-fan out a 'project changed' notification to all panels.
        void refreshAllPanels();
        void refreshCanvasToolbar();
        juce::String getCurrentDspPatchSectionLabel() const;
        void beginMidiLearn (juce::String parameterId);
        bool captureMidiLearnMessage (const juce::MidiMessage&);
        bool isMidiLearnActive() const                    { return pendingMidiLearnParameter.isNotEmpty(); }

    private:
        void projectChanged() override;
        void projectChanged (PatchCraftProject::ChangeScope scope) override;
        void liveValueChanged (const juce::String&, float) override;
        void publishToPluginClubWithArtifactChoice (int artifactChoice);
        void refreshTooltipWindowState();

        PatchCraftProject  project;
        AssetManager       assets;
        AiAssistService    ai;
        StudioAudioService audioService;

        juce::MenuBarComponent menuBar;
        juce::TooltipWindow studioTooltipWindow { this, 650 };
        std::unique_ptr<TopToolbar>      topToolbar;
        std::unique_ptr<ElementPalette>  elementPalette;
        std::unique_ptr<BuiltAssetLibraryComponent> assetLibraryPanel;
        std::unique_ptr<ExpansionLibraryPanel>      expansionLibraryPanel;
        std::unique_ptr<LayersPanel>     layersPanel;
        std::unique_ptr<CanvasEditor>    canvasEditor;
        std::unique_ptr<CanvasToolbar>   canvasToolbar;
        std::unique_ptr<InspectorPanel>  inspectorPanel;
        std::unique_ptr<juce::Viewport>  inspectorViewport;
        std::unique_ptr<PresetsComponent> presetsPanel;
        std::unique_ptr<BottomPanel>     bottomPanel;

        // Floating panel windows for the docking system.
        struct FloatingEntry
        {
            juce::Component* panel;
            juce::Component* originalParent;
            std::unique_ptr<FloatingPanelWindow> window;
        };
        std::vector<FloatingEntry> floatingPanels;

        juce::TabbedButtonBar leftTabs  { juce::TabbedButtonBar::TabsAtTop };
        juce::TabbedButtonBar rightTabs { juce::TabbedButtonBar::TabsAtTop };
        juce::TextButton leftCollapseButton  { "<" };
        juce::TextButton rightCollapseButton { ">" };
        juce::TextButton leftPopButton  { "Pop" };
        juce::TextButton rightPopButton { "Pop" };
        // Right-panel tab indices: 0=Inspector, 1=Layers, 2=Presets.
        int  rightTabIndex = 0;
        bool showLayersInRightPanel = false;

        BottomPanel::Page bottomTab { BottomPanel::Page::Workflow };

        // Settings window (audio/MIDI device picker).
        class SettingsWindowHolder;
        std::unique_ptr<SettingsWindowHolder> settingsWindow;

        juce::String selectedElementId;
        juce::StringArray selectedElementIds;
        juce::String pendingMidiLearnParameter;
        LayoutElement copiedDesignStyle;
        std::vector<LayoutElement> copiedLayoutElements;
        bool copiedLayoutIncludesParameters = true;
        bool copySelectionToTabsAsReference = false;
        bool hasCopiedDesignStyle = false;
        bool showLayersInsteadOfElements = false;
        bool showLibraryInsteadOfElements = false;
        bool showExpansionsInsteadOfElements = false;
        bool sampleLibraryDrawerOpen = false;
        bool dspTutorialShownThisSession = false;
        int leftPanelWidth = 240;
        int inspectorPanelWidth = 300;
        bool leftPanelCollapsed  = false;
        bool rightPanelCollapsed = false;
        juce::Rectangle<int> leftResizeHandle;
        juce::Rectangle<int> rightResizeHandle;
        enum class PanelDragMode { None, LeftPanel, Inspector };
        PanelDragMode panelDragMode = PanelDragMode::None;

        // Status bar values
        std::atomic<int>   statusVoices { 12 };
        std::atomic<float> statusCpu    { 3.2f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioMainComponent)
    };

} // namespace patchcraft
