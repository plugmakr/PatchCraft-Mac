#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"

#include <map>

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
        void timerCallback() override;

        bool keyPressed (const juce::KeyPress&) override;
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

        void selectionChanged();

        // Toolbar controls
        void setSnap (int gridSize) { snapGrid = gridSize; repaint(); }
        int getSnapGrid() const     { return snapGrid; }
        void setZoom (float z)      { zoom = z; resized(); repaint(); }
        void fit();
        bool isGridVisible() const  { return showGrid; }
        bool areRulersVisible() const { return showRulers; }
        void setGridVisible (bool shouldShow);
        void setRulersVisible (bool shouldShow);

        // Tab / group switching.
        const juce::String& getCurrentTabGroup() const { return currentTabGroup; }
        const std::map<juce::String, juce::String>& getActiveTabGroupsByPanel() const { return activeTabGroupsByPanel; }
        void setCurrentTabGroup (juce::String groupId);

        // Returns true if the element should be visible given the current tab.
        bool isElementOnCurrentTab (const LayoutElement&) const;
        void addElementAt (ElementType type, juce::Point<int> canvasPos, juce::String parameterId = {});

    private:
        enum class DragMode { None, Move, ResizeBR, ValueDrag, Marquee };

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
        void showContextMenu (juce::Point<int> screenPos);

        StudioMainComponent& owner;

        float zoom = 0.45f;
        int   snapGrid = 8;
        bool  showGrid = true;
        bool  showRulers = true;

        DragMode mode = DragMode::None;
        juce::Point<int> dragStart;
        LayoutElement dragOriginal;
        juce::Rectangle<int> marqueeRect;
        std::map<juce::String, juce::Point<int>> multiDragOrigins;
        bool layoutChangedDuringDrag = false;

        // Value-drag state
        juce::String dragParameterId;
        juce::String dragValueElementId;
        float        dragValueStart = 0.0f;
        juce::String hoverGuidance;
        juce::Rectangle<int> hoverGuidanceBounds;

        // Active tab page. Default is "main" so the seeded macro-knob group
        // shows up out of the box.
        juce::String currentTabGroup { "main" };
        std::map<juce::String, juce::String> activeTabGroupsByPanel;

        // Hit-test the on-canvas tabs of a TabPanel element.
        // Returns -1 if outside, else the tab index (0..tabs.size()-1).
        int hitTabIndex (const LayoutElement& tabPanel,
                         juce::Rectangle<int> screenRect,
                         juce::Point<int> pos) const;

        void drawTabPanel (juce::Graphics&, const LayoutElement&,
                           juce::Rectangle<int> screenRect, bool selected) const;
    };

} // namespace patchcraft
