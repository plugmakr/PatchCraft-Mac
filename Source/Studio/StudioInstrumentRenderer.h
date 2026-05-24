#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftProject.h"
#include "AssetManager.h"
#include <atomic>
#include <map>

namespace patchcraft
{
    class CanvasEditor;
    class StudioMainComponent;

    /**
        Renders a project's layout.json as a live, parameter-bound UI for the Studio Test page.

        Similar to PlayerGuiRenderer but works with LiveValueStore instead of APVTS.
        - Real juce::Slider widgets for Knob and Slider elements
        - All other element types drawn directly in paint()
        - Click handling for TabPanel switching
        - Keyboard element sends MIDI note events via callback
    */
    class StudioInstrumentRenderer : public juce::Component,
                                     private LiveValueStore::Listener,
                                     private PatchCraftProject::Listener,
                                     private juce::Timer
    {
    public:
        StudioInstrumentRenderer (StudioMainComponent& owner);
        ~StudioInstrumentRenderer() override;

        void rebuild();
        void syncFromDesignerState (const CanvasEditor&);
        void setAudioReactiveLevel (float level) noexcept
        {
            audioReactiveLevel.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
        }

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag  (const juce::MouseEvent&) override;
        void mouseUp    (const juce::MouseEvent&) override;
        void timerCallback() override;

        // MIDI event callbacks - set these to receive keyboard input
        std::function<void(int note, float velocity)> onNoteOn;
        std::function<void(int note)> onNoteOff;
        std::function<bool()> isTransportPlaying;
        std::function<double(int steps)> getSequencerPlaybackPosition01;
        std::function<void()> onToggleTransport;
        std::function<bool(int pattern)> onSetDrumActivePattern;
        std::function<bool(int pattern, int track, int step, bool active,
                           float velocity, float gate, float probability, int divisions)> onSetDrumPatternCell;
        std::function<bool(int lane, int step, float velocity, bool active)> onSetArpLaneStep;

    private:
        StudioMainComponent& owner;
        PatchCraftProject& project;
        AssetManager& assets;

        juce::OwnedArray<juce::Slider> knobs;
        std::vector<juce::String> knobParamIds;
        std::map<juce::String, std::vector<int>> knobIndicesByParam;
        std::atomic<float> audioReactiveLevel { 0.0f };
        double lastGranularAdvanceSeconds = 0.0;

        juce::Image background;
        juce::Image heroImage;
        std::vector<LayoutElement> elementsCopy;

        juce::String currentTabGroup { "main" };
        std::map<juce::String, juce::String> activeTabGroupsByPanel;
        int lastPlayedNote = -1;
        bool drumGridDragActive = false;
        bool drumGridPaintValue = false;
        int lastDrumGridPattern = -1;
        int lastDrumGridTrack = -1;
        int lastDrumGridStep = -1;
        bool arpLaneDragActive = false;
        int lastArpLane = -1;
        int lastArpStep = -1;

        // LiveValueStore::Listener
        void liveValueChanged (const juce::String& parameterId, float newValue) override;

        // PatchCraftProject::Listener
        void projectChanged() override;
        void projectChanged (PatchCraftProject::ChangeScope scope) override;

        // Geometry helpers
        struct CanvasMetrics { float scale; juce::Rectangle<int> canvas; };
        CanvasMetrics metrics() const;
        juce::Rectangle<int> elementRect (const LayoutElement&, const CanvasMetrics&) const;
        juce::Rectangle<int> animatedElementRect (const LayoutElement&, juce::Rectangle<int>) const;
        bool isElementOnCurrentTab (const LayoutElement&) const;

        // Per-type drawing
        void drawHeroPlaceholder (juce::Graphics&, juce::Rectangle<int>) const;
        void drawMeter   (juce::Graphics&, juce::Rectangle<int>) const;
        void drawPanel   (juce::Graphics&, juce::Rectangle<int>, const juce::String& label) const;
        void drawDropdown(juce::Graphics&, juce::Rectangle<int>, const juce::String& display) const;
        void drawKeyboard(juce::Graphics&, juce::Rectangle<int>) const;
        void drawTabPanel(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawLabel  (juce::Graphics&, juce::Rectangle<int>, const juce::String& text,
                         float fontSize = 14.0f,
                         juce::Colour colour = juce::Colour()) const;
        void drawRuntimeControl (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;

        int hitTabIndex (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>) const;
        int hitKeyboardNote (juce::Rectangle<int>, juce::Point<int>) const;
        bool handleXYPadGesture (const juce::MouseEvent&);
        bool handleGranularGesture (const juce::MouseEvent&);
        bool handleDrumGridGesture (const juce::MouseEvent&, bool drag);
        bool handleArpLaneGesture (const juce::MouseEvent&, bool drag);
        bool drumCellAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>,
                         int& pattern, int& track, int& step, float& velocity,
                         float& gate, float& probability, bool& active,
                         int& note, int& divisions) const;
        bool arpLaneStepAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>,
                            int& lane, int& step, float& velocity) const;
        const LayoutElement* findElementAt (juce::Point<int>) const;
        void showElementAnimationMenu (const LayoutElement&, const juce::Point<int>& screenPos);
        bool advanceGranularFields();
        void syncKnobsToLiveValues();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioInstrumentRenderer)
    };

} // namespace patchcraft
