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
    class LaunchCenterPage;
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
        enum class Page { Dashboard = 0, Design = 1, Samples = 2, OneShotMaker = 3, MidiPlayground = 4, ArpStudio = 5, Test = 6, DSP = 7, Widgets = 8, Branding = 9, Export = 10 };

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

        // ---- Branding Lab -------------------------------------------------
        std::unique_ptr<BrandingLabPage> brandingLab;

        // ---- Launch Center ------------------------------------------------
        std::unique_ptr<LaunchCenterPage> launchCenter;

        void rebuildPageVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
    };

} // namespace patchcraft
