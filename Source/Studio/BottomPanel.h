#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "SoundStack.h"
#include "BuildLabPage.h"

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
    class ChopLabPage;
    class ControlNodeEditor;
    struct SampleZoneDef;

    /**
        Bottom workspace - tabbed via the section tabs in the canvas toolbar.
        One Page is visible at a time:
            Workflow     - Guided instrument-building path and health checks.
            Design       - Layout canvas bindings strip (CONTROL BINDINGS).
            Samples      - Mapper / Keyzones / Velocity sub-tabs.
            MidiPlayground / ArpStudio - Perform workspace (Steps + Circles share one page).
            DSP          - Global typed graph editor (ControlNodeEditor).
            Test         - Embedded MIDI keyboard + preview engine.
            Widgets      - Unified Knob / Slider / Meter builder.
    */
    class BottomPanel : public juce::Component
    {
    public:
        enum class Page { Dashboard = 0, ProjectBrowser = 1, Design = 2, Samples = 3, OneShotMaker = 4, MidiPlayground = 5, ArpStudio = 6, Test = 7, DSP = 8, Widgets = 9, Animation = 10, Branding = 11, Export = 12, Expansions = 13, Chop = 14, Build = 15 };

        explicit BottomPanel (StudioMainComponent& owner);
        ~BottomPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void setPage (Page);
        Page getPage() const                              { return currentPage; }
        
        void refreshDesignSelection();

        // Test page: start/stop the embedded preview audio.
        void setPreviewActive (bool active);
        bool isPreviewActive() const;
        const SampleZoneDef* getSelectedSampleZone() const;
        juce::String getDspPatchSectionId() const;
        juce::String getDspPatchSectionLabel() const;
        void showDspBuilderTutorial();
        // Canvas shortcut: insert a motion block and open Sound Stack.
        void addArpBlock();
        void addMotionBlock (SoundStack::MotionKind kind);

        void refresh();

        /** Switch to Sound Mapper and highlight the chop source zone. */
        void selectSampleZone (int index);

        /** Switch to Brand Lab and show the full Player/DAW preview. */
        void enterDawPreviewMode();

        using BuildSubPage = BuildLabPage::SubPage;
        void setBuildSubPage (BuildSubPage);
        BuildSubPage getBuildSubPage() const { return buildSubPage; }

        SampleMapEditor* getSampleMapper() noexcept { return sampleMapper.get(); }

    private:
        StudioMainComponent& owner;
        Page currentPage = Page::Design;

        // ---- Workflow page -------------------------------------------------
        std::unique_ptr<WorkflowPage> workflowPage;
        std::unique_ptr<ProjectBrowserPage> projectBrowserPage;

        // ---- Design page ---------------------------------------------------
        juce::Label                          designDspHeader;
        juce::Label                          designPresetsHeader;
        std::unique_ptr<ParametersComponent> parameters;
        std::unique_ptr<PresetsComponent>    presets;

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

        // ---- Build page (unified Knob/Slider/Meter builder) ---------------
        std::unique_ptr<ControlBuilderComponent> builder;

        // ---- Global Graph View ---------------------------------------------
        std::unique_ptr<ControlNodeEditor> globalGraphView;

        // ---- Animation / reactive visual authoring ------------------------
        std::unique_ptr<AnimationLabPage> animationLab;

        // ---- Branding Lab -------------------------------------------------
        std::unique_ptr<BrandingLabPage> brandingLab;

        // ---- Build Lab (Quick Build guided workflow) -----------------------
        std::unique_ptr<BuildLabPage> buildLab;
        BuildSubPage buildSubPage = BuildSubPage::ImportSounds;

        // ---- Chop Lab -----------------------------------------------------
        std::unique_ptr<ChopLabPage> chopLab;

        // ---- Launch Center ------------------------------------------------
        std::unique_ptr<LaunchCenterPage> launchCenter;

        // ---- Store / Expansion add-ons ------------------------------------
        std::unique_ptr<ExpansionsPage> expansionsPage;

        void rebuildPageVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
    };

} // namespace patchcraft
