#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "BottomPanel.h"

namespace patchcraft
{
    class StudioMainComponent;

    class WorkflowPage : public juce::Component
    {
    public:
        explicit WorkflowPage (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();

    private:
        StudioMainComponent& owner;

        juce::Label title;
        juce::Label subtitle;
        juce::ToggleButton advancedMode { "Advanced Explorer" };
        juce::Label productTitle;
        juce::Label productBody;
        juce::Label pathTitle;
        juce::Label pathBody;
        juce::TextButton fullTutorialButton { "Start Full Demo Tutorial" };
        juce::Label truthTitle;
        juce::Label truthBody;
        juce::Label healthTitle;
        juce::Label healthBody;
        juce::TextButton healthCheckButton { "Run Health Check" };

        juce::TextButton synthButton { "Synth Instrument" };
        juce::TextButton sampleButton { "Sample Instrument" };
        juce::TextButton drumButton { "Drum Machine" };
        juce::TextButton orbitButton { "Orbit Groove Instrument" };
        juce::TextButton fxButton { "FX Plugin" };
        juce::Label factoryDemoLabel;
        juce::ComboBox factoryDemoBox;
        juce::TextButton loadFactoryDemoButton { "Choose / Load Demo" };

        juce::TextButton soundButton { "1  Build Sound" };
        juce::TextButton midiButton { "2  MIDI / Performance" };
        juce::TextButton designButton { "3  Design Player" };
        juce::TextButton presetsButton { "4  Presets + Packs" };
        juce::TextButton testButton { "5  Test Runtime" };
        juce::TextButton exportButton { "6  Export" };

        juce::TextButton advDspButton { "Open DSP Builder" };
        juce::TextButton advMapperButton { "Open Sample Mapper" };
        juce::TextButton advOneShotButton { "Open One Shot Maker" };
        juce::TextButton advMidiButton { "Open Performance Builder" };
        juce::TextButton advBuildButton { "Open Asset Builder" };
        juce::TextButton advAnimationButton { "Open Animation Lab" };
        juce::TextButton advDesignButton { "Open Design Surface" };
        juce::TextButton advBrandButton { "Open Brand/Test Lab" };

        juce::String healthSummary;
        juce::String detailedHealth;
        juce::Array<juce::File> factoryDemoFolders;
        juce::StringArray factoryDemoNames;

        enum class TutorialModule
        {
            FullDemo,
            BuildSound,
            MidiPerformance,
            DesignPlayer,
            PresetsPacks,
            TestRuntime,
            Export
        };

        void switchTemplate (const juce::String& engineId, const juce::String& label);
        void createOrbitStarter();
        void populateFactoryDemos();
        void showFactoryDemoMenu();
        void loadSelectedFactoryDemo();
        void updateModeVisibility();
        void showExportMenu();
        void showHealthDialog();
        void showModuleTutorial (TutorialModule module);
        juce::String tutorialTextFor (TutorialModule module) const;
        BottomPanel::Page targetPageFor (TutorialModule module) const;
        void openTutorialTarget (TutorialModule module);
        juce::String buildHealthSummary (bool detailed) const;
        void stylePrimary (juce::Button&);
        void styleSecondary (juce::Button&);
        void styleCardLabel (juce::Label&, float size, bool bold, juce::Colour colour);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorkflowPage)
    };
}
