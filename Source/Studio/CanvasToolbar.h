#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;
    class CanvasEditor;

    /**
        Thin toolbar that sits above the canvas. Holds:
        - Canvas size dropdown (preset list of approved instrument sizes).
        - Engine dropdown (Sampler / Synth / Effect).
        - Section tabs (Design / Sample Mapper / Test / Build).
        - Snap-grid value + Zoom dropdown + Fit button.

        Canvas size is a hard property of the project: changing it updates
        project.getCanvasSize() and forces the canvas to repaint at the new
        size (background images stretch to fit).
    */
    class CanvasToolbar : public juce::Component
    {
    public:
        CanvasToolbar (StudioMainComponent& owner, CanvasEditor& canvas);

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();

        // Reflect external tab changes (e.g. clicking Preview switches to Test).
        void syncSectionTabFromOwner();

    private:
        StudioMainComponent& owner;
        CanvasEditor&        canvas;

        juce::Label    canvasLabel;
        juce::Label    engineLabel;
        juce::ComboBox engineBox;
        juce::ComboBox sizeBox;
        juce::ComboBox snapBox;
        juce::ComboBox zoomBox;
        juce::TextButton canvasPropsBtn { "Canvas" };
        juce::TextButton fitBtn   { "Fit" };
        juce::TextButton fillBtn  { "100%" };
        juce::Label    snapLabel;

        // Alignment / distribute / order cluster (Design-page-only).
        // The button labels use compact glyph hints (e.g. "L", "T") rather
        // than full text so the toolbar stays at one row.
        struct AlignButton : public juce::Button
        {
            AlignButton (const juce::String& g, const juce::String& tt, std::function<void()> on);
            void paintButton (juce::Graphics&, bool over, bool down) override;
            juce::String glyph;
            std::function<void()> onAction;
        };
        std::vector<std::unique_ptr<AlignButton>> alignButtons;
        std::unique_ptr<juce::TextButton> orderBtn;
        juce::Label alignSeparator;
        bool alignmentVisible = false;

        // Section tab strip - sits to the right of the engine dropdown and
        // controls which page the bottom panel shows.
        juce::TextButton tabDesign { "Design" };

        juce::TextButton tabMapper { "Samples" };
        juce::TextButton tabOneShot { "One-Shots" };
        juce::TextButton tabMidi   { "MIDI" };
        juce::TextButton tabArp    { "Arp" };
        juce::TextButton tabDSP    { "DSP" };
        juce::TextButton tabBuild  { "Widgets" };
        juce::TextButton tabAnimation { "Anim" };
        juce::TextButton tabBranding { "Branding" };
        juce::TextButton tabLaunch { "Export" };
        juce::Label      patchSeparator;
        juce::TextButton savePatchBtn   { "Save Patch" };
        juce::TextButton savePatchAsBtn { "Save Patch As" };
        void onSectionTabClick (int index);

        void applySelectedEngine();

        struct SizePreset { int w, h; juce::String label; };
        std::vector<SizePreset> sizePresets;

        void applySelectedSize();
        void applySelectedZoom();
        void applySelectedSnap();
        void showCanvasProperties();
        void showCanvasSizeDialog();
        void showCanvasColourMenu (bool gridColour);

        // Find the dropdown index that matches the current canvas size, or -1.
        int presetIndexFor (int w, int h) const;
    };

} // namespace patchcraft
