#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /** A target-focused node graph opened from an individual UI control. */
    class ControlNodeEditor final : public juce::Component
    {
    public:
        ControlNodeEditor (StudioMainComponent& owner, juce::String elementId);
        ~ControlNodeEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void rebuild();

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void showAddNodeMenu (const juce::String& anchorBlockId, juce::Point<int> canvasPosition,
                              juce::Point<int> screenPosition);

    private:
        class NodeCanvas;
        class GraphMiniMap;

        void addNode (const juce::String& definitionId, bool forceNew);
        void addNodeAt (const juce::String& definitionId, juce::Point<int> canvasPosition,
                        const juce::String& anchorBlockId = {});
        void setStatus (const juce::String& message);
        void applyCanvasZoom();
        void fitCanvasToView();
        void updateValidationBadge();
        void applySearchFilter();

        StudioMainComponent& owner;
        juce::String elementId;
        juce::Label title;
        juce::Label targetSummary;
        juce::TextButton tidyButton { "Tidy" };
        juce::TextButton fitButton { "Fit" };
        juce::TextButton listenButton { "Listen" };
        juce::TextButton advancedButton { "Advanced" };
        juce::TextButton addMotionButton { "+ Motion" };
        juce::TextEditor searchField;
        juce::ComboBox graphTemplateBox;
        juce::Label validationBadge;
        juce::Label status;
        juce::Viewport viewport;
        std::unique_ptr<GraphMiniMap> miniMap;
        std::unique_ptr<NodeCanvas> nodeCanvas;
        std::vector<std::unique_ptr<juce::TextButton>> addButtons;
        std::vector<std::unique_ptr<juce::Label>> paletteLabels;
        juce::String searchQuery;
        float zoomScale = 1.0f;
        int baseCanvasWidth = 1400;
        int baseCanvasHeight = 700;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlNodeEditor)
    };
}
