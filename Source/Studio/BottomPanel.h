#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace patchcraft
{
    class StudioMainComponent;
    class SampleMapEditor;
    class KeyzonesComponent;
    class VelocityMapViewer;
    class ParametersComponent;
    class ControlBuilderComponent;
    class PresetsComponent;
    class InstrumentPreviewComponent;
    class StudioInstrumentRenderer;
    class DspPage;
    class MidiPlaygroundPage;
    class OneShotMakerPage;
    class WorkflowPage;
    class ProjectBrowserPage;
    class LaunchCenterPage;
    class ExpansionsPage;
    class AnimationLabPage;
    class IInstrumentEngine;
    class TestPage;
    class BrandingLabPage;
    struct SampleZoneDef;

    /**
        Bottom workspace - tabbed via the section tabs in the canvas toolbar.
        One Page is visible at a time:
            Workflow     - Guided instrument-building path and health checks.
        Parameters   - Parameters list + Presets browser side-by-side.
        Samples      - Mapper / Keyzones / Velocity sub-tabs.
        MidiPlayground - Musical MIDI generation and sample-control tools.
        ArpStudio    - Dedicated circular arpeggiator builder surface.
        Test         - Embedded MIDI keyboard + preview engine.
        Widgets      - Unified Knob / Slider / Meter builder.
    */
    class BottomPanel : public juce::Component
    {
    public:
        enum class Page { Dashboard = 0, ProjectBrowser = 1, Design = 2, Samples = 3, OneShotMaker = 4, MidiPlayground = 5, ArpStudio = 6, Test = 7, DSP = 8, Widgets = 9, Animation = 10, Branding = 11, Export = 12, Expansions = 13 };

        explicit BottomPanel (StudioMainComponent& owner);
        ~BottomPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void setPage (Page);
        Page getPage() const                              { return currentPage; }

        // Test page: start/stop the embedded preview audio.
        void setPreviewActive (bool active);
        bool isPreviewActive() const;
        const SampleZoneDef* getSelectedSampleZone() const;
        juce::String getDspPatchSectionId() const;
        juce::String getDspPatchSectionLabel() const;
        void showDspBuilderTutorial();
        // Forwards to the underlying DspPage so the canvas right-click
        // shortcut can drop in an arpeggiator without reaching into the
        // bottom panel's private members.
        void addArpBlock();

        void refresh();

    private:
        StudioMainComponent& owner;
        Page currentPage = Page::Dashboard;

        // ---- Workflow page -------------------------------------------------
        std::unique_ptr<WorkflowPage> workflowPage;
        std::unique_ptr<ProjectBrowserPage> projectBrowserPage;

        // ---- Design page ---------------------------------------------------
        juce::Label                          designDspHeader;
        juce::Label                          designPresetsHeader;
        std::unique_ptr<ParametersComponent> parameters;
        std::unique_ptr<PresetsComponent>    presets;
        std::unique_ptr<DspPage>             designDspPage;

        // ---- Sample Mapper page -------------------------------------------
        juce::TextButton btnMapperMain    { "Sample Mapper" };
        juce::TextButton btnMapperKeyzones { "Keyzones" };
        juce::TextButton btnMapperVelocity { "Velocity" };
        juce::TextButton btnMapperFull { "Full" };
        std::unique_ptr<SampleMapEditor> sampleMapper;
        std::unique_ptr<KeyzonesComponent>     keyzones;
        std::unique_ptr<VelocityMapViewer>     velocityMap;
        int activeMapperSubTab = 0;
        void rebuildMapperSubVisibility();

        // ---- MIDI Playground page ------------------------------------------
        std::unique_ptr<OneShotMakerPage> oneShotMaker;
        std::unique_ptr<MidiPlaygroundPage> midiPlayground;

        // ---- Test page -----------------------------------------------------
        std::unique_ptr<TestPage> testPage;

        // ---- DSP page -------------------------------------------------------
        std::unique_ptr<DspPage> dspPage;

        // ---- Build page (unified Knob/Slider/Meter builder) ---------------
        std::unique_ptr<ControlBuilderComponent> builder;

        // ---- Animation / reactive visual authoring ------------------------
        std::unique_ptr<AnimationLabPage> animationLab;

        // ---- Branding Lab -------------------------------------------------
        std::unique_ptr<BrandingLabPage> brandingLab;

        // ---- Launch Center ------------------------------------------------
        std::unique_ptr<LaunchCenterPage> launchCenter;

        // ---- Store / Expansion add-ons ------------------------------------
        std::unique_ptr<ExpansionsPage> expansionsPage;

        void rebuildPageVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
    };

} // namespace patchcraft
