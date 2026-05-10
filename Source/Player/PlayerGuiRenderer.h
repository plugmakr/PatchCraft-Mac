#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PatchCraftPackFormat.h"
#include "AssetManager.h"

#include <map>

namespace patchcraft
{
    class PlayerProcessor;

    /**
        Renders a loaded pack's layout.json as a live, parameter-bound UI.

        - Real juce::Slider widgets are spawned for Knob and Slider elements
          and attached to APVTS slots (so DAW automation works).
        - All other element types (Image, Label, Meter, Keyboard, Panel,
          Dropdown, TabPanel) are drawn directly in paint() so we don't
          require dedicated Component subclasses for each.
        - Click handling on the panel:
            TabPanel -> switch the active group (filters which controls are
                        visible, mirroring the Studio canvas behaviour).
            Dropdown id="presets" -> show preset menu.
    */
    class PlayerGuiRenderer : public juce::Component,
                              private juce::Timer
    {
    public:
        PlayerGuiRenderer (PlayerProcessor& proc, AssetManager& assets);
        ~PlayerGuiRenderer() override;

        void rebuild();

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void timerCallback() override;

    private:
        PlayerProcessor& proc;
        AssetManager&    assets;

        juce::OwnedArray<juce::Component> controls;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;

        juce::Image background;
        juce::Image heroImage;
        std::vector<LayoutElement> elementsCopy;

        juce::String currentTabGroup { "main" };
        std::map<juce::String, juce::String> activeTabGroupsByPanel;
        int lastPlayedNote = -1;
        int activePadNote = -1;
        juce::String activeMomentaryParameter;
        float lastOutputPeak = 0.0f;
        bool hasMeterOrReactiveElement = false;
        juce::String lastPendingMidiLearn;

        // Geometry helpers
        struct CanvasMetrics { float scale; juce::Rectangle<int> canvas; };
        CanvasMetrics metrics() const;
        juce::Rectangle<int> elementRect (const LayoutElement&, const CanvasMetrics&) const;
        bool isElementOnCurrentTab (const LayoutElement&) const;
        int parameterIndexForId (const juce::String&) const;
        const ParameterDef* parameterForId (const juce::String&) const;
        bool parameterIsEnabled (const ParameterDef&) const;
        void refreshControlEnablement();
        float parameterValueForElement (const LayoutElement&, float fallback = 0.0f) const;
        juce::String formattedParameterValue (const LayoutElement&) const;
        juce::Rectangle<int> animatedElementRect (const LayoutElement&, juce::Rectangle<int>) const;

        // Per-type drawing
        void drawHeroPlaceholder (juce::Graphics&, juce::Rectangle<int>) const;
        void drawMeter   (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawPanel   (juce::Graphics&, juce::Rectangle<int>, const juce::String& label) const;
        void drawButton  (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawValueDisplay (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawDropdown(juce::Graphics&, juce::Rectangle<int>, const juce::String& display) const;
        void drawKeyboard(juce::Graphics&, juce::Rectangle<int>) const;
        void drawTabPanel(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawXYPad  (juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;
        void drawPadGrid(juce::Graphics&, juce::Rectangle<int>, const LayoutElement&) const;

        int hitTabIndex (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>) const;
        const Manifest* manifest() const;
        juce::Colour playerBg() const;
        juce::Colour playerPanel() const;
        juce::Colour playerAccent() const;
        juce::Colour playerText() const;
        juce::Colour playerTextDim() const;
        juce::Colour playerBorder() const;

        const LayoutElement* findBindableElementAt (juce::Point<int>) const;
        void showControlContextMenu (const LayoutElement&, const juce::Point<int>& screenPos);
        void showPresetMenu (const juce::Point<int>& screenPos);
        void showParameterMenu (const LayoutElement&, const juce::Point<int>& screenPos);
        void handleKeyboardClick (juce::Rectangle<int> r, juce::Point<int> pos);
        bool handleXYPadGesture (const juce::MouseEvent&);
        bool handlePadClick (const juce::MouseEvent&);
        int  padNoteAt (const LayoutElement&, juce::Rectangle<int>, juce::Point<int>) const;
    };

} // namespace patchcraft
