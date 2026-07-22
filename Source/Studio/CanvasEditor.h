#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"

#include <map>
#include <vector>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Central canvas: rulers, grid, instrument design, selection + drag/resize.
        Renders LayoutElements with the same drawing routines used at runtime
        by the Player plugin.
    */
    class CanvasEditor : public juce::Component,
                         public juce::DragAndDropTarget,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
    {
    public:
        explicit CanvasEditor (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void timerCallback() override;

        bool keyPressed (const juce::KeyPress&) override;
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

        void selectionChanged();

        // Toolbar controls
        void setSnap (int gridSize) { snapGrid = juce::jmax (1, gridSize); repaint(); }
        int getSnapGrid() const     { return snapGrid; }
        void setSnapEnabled (bool shouldSnap) { snapEnabled = shouldSnap; repaint(); }
        bool isSnapEnabled() const  { return snapEnabled; }
        void setZoom (float z);
        float getZoom() const       { return zoom; }
        bool isAutoFitEnabled() const { return autoFitCanvas; }
        void refreshZoomForBounds();
        void fit();
        bool isGridVisible() const  { return showGrid; }
        bool areRulersVisible() const { return showRulers; }
        void setGridVisible (bool shouldShow);
        void setRulersVisible (bool shouldShow);
        void setGridColour (juce::Colour c) { gridColour = c; repaint(); }
        void setSnapColour (juce::Colour c) { snapColour = c; repaint(); }
        void setDesignerModeActive (bool active) { designerModeActive = active; repaint(); }
        bool isDesignerModeActive() const { return designerModeActive; }
        juce::Colour getGridColour() const  { return gridColour; }
        juce::Colour getSnapColour() const  { return snapColour; }

        // Tab / group switching.
        const juce::String& getCurrentTabGroup() const { return currentTabGroup; }
        const std::map<juce::String, juce::String>& getActiveTabGroupsByPanel() const { return activeTabGroupsByPanel; }
        void setCurrentTabGroup (juce::String groupId);
        bool triggerManualContainer (const LayoutElement& element);

        // Returns true if the element should be visible given the current tab.
        bool isElementOnCurrentTab (const LayoutElement&) const;
        void addElementAt (ElementType type, juce::Point<int> canvasPos, juce::String parameterId = {});
        void addMixerChannelAt (juce::Point<int> canvasPos);
        void addDrumMachineControlLayout (juce::Point<int> canvasPos);
        void addCircleSeqInstrumentLayout (juce::Point<int> canvasPos);
        void addOrbitInstrumentControlLayout (juce::Point<int> canvasPos);
        void addVisualReactivityControlLayout (juce::Point<int> canvasPos);
        void addCircleSeqBackgroundKit (juce::Point<int> canvasPos);
        void addModuleLayout (const juce::String& moduleType, juce::Point<int> pos);

        const LayoutElement* scriptableControlAt (juce::Point<int> localPosition) const;

    private:
        enum class DragMode { None, Move, Resize, ValueDrag, AuditionNote, Marquee, DrumGridEdit, ArpLaneEdit, Pan };
        enum class ResizeHandle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

        juce::Rectangle<int> canvasScreenRect() const;
        juce::Rectangle<int> elementScreenRect (const LayoutElement&) const;
        juce::Point<int> screenToCanvas (juce::Point<int>) const;
        void drawRulers (juce::Graphics&) const;
        void drawCanvasBackground (juce::Graphics&, juce::Rectangle<int>) const;
        void drawElement (juce::Graphics&, const LayoutElement&,
                          juce::Rectangle<int> screenRect, bool selected) const;
        void drawSelectionGuides (juce::Graphics&) const;

        // Returns true if (px,py) lies inside the active control body of this
        // element (so a drag should change the parameter value rather than
        // move/resize the element).
        bool hitTestControlBody (const LayoutElement&, juce::Rectangle<int> r,
                                 juce::Point<int> p) const;
        bool drumCellAt (const LayoutElement&, juce::Rectangle<int> r,
                         juce::Point<int> p, int& pattern, int& track,
                         int& step, float& velocity) const;
        bool editDrumGridCellAt (const LayoutElement&, juce::Rectangle<int> r,
                                 juce::Point<int> p, const juce::ModifierKeys& mods,
                                 bool startGesture);
        bool arpLaneStepAt (const LayoutElement&, juce::Rectangle<int> r,
                            juce::Point<int> p, int& lane, int& step, float& velocity) const;
        bool editArpLaneStepAt (const LayoutElement&, juce::Rectangle<int> r,
                                juce::Point<int> p, bool startGesture);
        const LayoutElement* sampleDropZoneAt (juce::Point<int> localPosition) const;
        void assignSamplesToDropZone (const juce::String& elementId, const juce::Array<juce::File>& files);
        void showContextMenu (juce::Point<int> screenPos, bool forceMainMenu = false);
        void copySelectedArpLanePattern();
        void pasteArpLanePatternToSelection();
        void resetSelectedArpLanePattern();
        void explodeSelectedMixers();
        bool selectionContainsMixer() const;
        void captureMoveOriginsForSelection();
        void addMoveOriginWithChildren (const juce::String& id);
        bool getMultiSelectionScreenBounds (juce::Rectangle<int>& bounds) const;
        bool multiSelectionResizeHandleContains (juce::Point<int> point) const;
        ResizeHandle resizeHandleAt (juce::Point<int> point, juce::Rectangle<int> bounds) const;

        StudioMainComponent& owner;

        float zoom = 0.45f;
        juce::Point<int> canvasPanOffset;
        juce::Point<int> panDragOrigin;
        bool autoFitCanvas = true;
        int   snapGrid = 8;
        bool  snapEnabled = true;
        bool  showGrid = true;
        bool  showRulers = true;
        juce::Colour gridColour { 0xff15181e };
        juce::Colour snapColour { 0xff26313d };

        DragMode mode = DragMode::None;
        juce::Point<int> dragStart;
        LayoutElement dragOriginal;
        ResizeHandle activeResizeHandle = ResizeHandle::None;
        juce::Rectangle<int> marqueeRect;
        std::map<juce::String, juce::Point<int>> multiDragOrigins;
        std::vector<LayoutElement> dragLayoutBefore;
        juce::String dragActionName;
        bool layoutChangedDuringDrag = false;
        int auditionNote = -1;
        juce::String auditionElementId;

        // Value-drag state
        juce::String dragParameterId;
        juce::String dragValueElementId;
        float        dragValueStart = 0.0f;
        bool         dragValueIsLocalPreview = false;
        juce::String hoverGuidance;
        juce::Rectangle<int> hoverGuidanceBounds;
        juce::String drumGridEditElementId;
        bool drumGridPaintState = true;
        int lastDrumGridTrack = -1;
        int lastDrumGridStep = -1;
        juce::String arpLaneEditElementId;
        int lastArpLane = -1;
        int lastArpStep = -1;
        std::map<juce::String, float> copiedArpLanePattern;
        bool designerModeActive = true;

        // Active tab page. Default is "main" so the seeded macro-knob group
        // shows up out of the box.
        juce::String currentTabGroup { "main" };
        std::map<juce::String, juce::String> activeTabGroupsByPanel;
        std::map<juce::String, bool> manualContainerOpen;
        juce::StringArray manualContainerOrder;
        bool isManualContainerGroup (const juce::String& groupId) const;
        void ensureManualContainerDefaults();

        // Hit-test the on-canvas tabs of a TabPanel element.
        // Returns -1 if outside, else the tab index (0..tabs.size()-1).
        int hitTabIndex (const LayoutElement& tabPanel,
                         juce::Rectangle<int> screenRect,
                         juce::Point<int> pos) const;

        void drawTabPanel (juce::Graphics&, const LayoutElement&,
                           juce::Rectangle<int> screenRect, bool selected) const;
    };

} // namespace patchcraft
