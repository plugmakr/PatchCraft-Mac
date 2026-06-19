#include "BottomPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>

#include "SampleMapEditor.h"
#include "KeyzonesComponent.h"
#include "VelocityMapViewer.h"
#include "ParametersComponent.h"
#include "PresetsComponent.h"
#include "ControlBuilderComponent.h"
#include "ControlNodeEditor.h"
#include "TestPage.h"
#include "BrandingLabPage.h"
#include "MidiPlaygroundPage.h"
#include "OneShotMakerPage.h"
#include "WorkflowPage.h"
#include "ProjectBrowserPage.h"
#include "LaunchCenterPage.h"
#include "ExpansionsPage.h"
#include "AnimationLabPage.h"
#include "ArpeggiatorRuntime.h"
#include "PatchCraftProject.h"

namespace patchcraft
{
    static void styleSubTab (juce::TextButton& b, int radioId)
    {
        b.setClickingTogglesState (true);
        b.setRadioGroupId (radioId);
        b.getProperties().set ("flatTab", true);
        b.getProperties().set ("fontSize", 11.5);
        b.getProperties().set ("bold",    true);
    }

    BottomPanel::BottomPanel (StudioMainComponent& o) : owner (o)
    {
        // Workflow page -----------------------------------------------------
        workflowPage = std::make_unique<WorkflowPage> (owner);
        addAndMakeVisible (*workflowPage);

        projectBrowserPage = std::make_unique<ProjectBrowserPage> (owner);
        addChildComponent (*projectBrowserPage);

        // Design page -------------------------------------------------------
        designDspHeader.setText ("CONTROL BINDINGS", juce::dontSendNotification);
        designDspHeader.setTooltip ("Select a layout control and wire it to sound, motion, MIDI, effects, and output parameters.");
        designDspHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        designDspHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (designDspHeader);

        parameters = std::make_unique<ParametersComponent> (owner);
        addChildComponent (*parameters);

        // Sample Mapper page ------------------------------------------------
        styleSubTab (btnMapperMain,     8810);
        styleSubTab (btnMapperKeyzones, 8810);
        styleSubTab (btnMapperVelocity, 8810);
        btnMapperFull.getProperties().set ("smallButton", true);
        btnMapperFull.setTooltip ("Open the Sample Mapper, Keyzones, and Velocity zones in a large floating workspace.");
        btnMapperMain.setToggleState (true, juce::dontSendNotification);
        btnMapperMain.onClick     = [this] { activeMapperSubTab = 0; rebuildMapperSubVisibility(); };
        btnMapperKeyzones.onClick = [this] { activeMapperSubTab = 1; rebuildMapperSubVisibility(); };
        btnMapperVelocity.onClick = [this] { activeMapperSubTab = 2; rebuildMapperSubVisibility(); };
        btnMapperFull.onClick     = [this] { owner.openSampleMapperZoneManager(); };
        addAndMakeVisible (btnMapperMain);
        addAndMakeVisible (btnMapperKeyzones);
        addAndMakeVisible (btnMapperVelocity);
        addAndMakeVisible (btnMapperFull);

        sampleMapper = std::make_unique<SampleMapEditor> (owner);
        keyzones     = std::make_unique<KeyzonesComponent>     (owner);
        velocityMap = std::make_unique<VelocityMapViewer>();
        velocityMap->onZoneClicked = [this] (int index)
        {
            if (sampleMapper != nullptr)
                sampleMapper->selectZone (index);
            if (velocityMap != nullptr)
                velocityMap->setSelectedZone (index);
        };
        velocityMap->onZoneEdited = [this] (int index, const SampleZoneDef& zone)
        {
            auto& zones = owner.getProject().getSampleMap().getZones();
            if (index >= 0 && index < (int) zones.size())
            {
                zones[(size_t) index] = zone;
                owner.getProject().notifyChanged();
                if (sampleMapper) sampleMapper->refresh();
                if (keyzones) keyzones->refresh();
            }
        };
        addChildComponent (*sampleMapper);
        addChildComponent (*keyzones);
        addChildComponent (*velocityMap);

        // One Shot Maker page ------------------------------------------------
        oneShotMaker = std::make_unique<OneShotMakerPage> (owner);
        addChildComponent (*oneShotMaker);

        // MIDI Playground page -----------------------------------------------
        midiPlayground = std::make_unique<MidiPlaygroundPage> (owner);
        addChildComponent (*midiPlayground);

        // Test page ----------------------------------------------------------
        testPage = std::make_unique<TestPage> (owner);
        addChildComponent (*testPage);

        // Build page ---------------------------------------------------------
        builder = std::make_unique<ControlBuilderComponent> (owner);
        addChildComponent (*builder);

        // Animation Lab ------------------------------------------------------
        animationLab = std::make_unique<AnimationLabPage> (owner);
        addChildComponent (*animationLab);

        // Branding Lab -------------------------------------------------------
        brandingLab = std::make_unique<BrandingLabPage> (owner);
        addChildComponent (*brandingLab);

        // Launch Center ------------------------------------------------------
        launchCenter = std::make_unique<LaunchCenterPage> (owner);
        addChildComponent (*launchCenter);

        // Store / Expansions -------------------------------------------------
        expansionsPage = std::make_unique<ExpansionsPage> (owner);
        addChildComponent (*expansionsPage);

        // Global Graph View --------------------------------------------------
        globalGraphView = std::make_unique<ControlNodeEditor> (owner, "");
        addChildComponent (*globalGraphView);

        rebuildPageVisibility();
    }

    BottomPanel::~BottomPanel() = default;

    void BottomPanel::setPage (Page p)
    {
        const bool enterCircles = (p == Page::ArpStudio);
        if (enterCircles)
            p = Page::MidiPlayground;

        const bool wasPerform = (currentPage == Page::MidiPlayground || currentPage == Page::ArpStudio);
        const bool toPerform = (p == Page::MidiPlayground);

        if (currentPage == p && ! enterCircles)
            return;

        currentPage = p;

        if (midiPlayground)
        {
            if (enterCircles)
                midiPlayground->showArpStudioMode();
            else if (toPerform && ! wasPerform)
                midiPlayground->showPlaygroundMode();
        }

        rebuildPageVisibility();
    }

    void BottomPanel::setPreviewActive (bool active)
    {
        if (isPreviewActive() == active) return;

        if (active)
        {
            if (testPage != nullptr) testPage->activate();
        }
        else
        {
            if (testPage != nullptr)
                testPage->deactivate();
        }
    }

    bool BottomPanel::isPreviewActive() const
    {
        return testPage != nullptr && testPage->isAudioRunning();
    }

    const SampleZoneDef* BottomPanel::getSelectedSampleZone() const
    {
        return sampleMapper != nullptr ? sampleMapper->getSelectedZone() : nullptr;
    }

    juce::String BottomPanel::getDspPatchSectionId() const
    {
        return "source";
    }

    juce::String BottomPanel::getDspPatchSectionLabel() const
    {
        return "Source";
    }

    void BottomPanel::showDspBuilderTutorial()
    {
        setPage (Page::DSP);
    }

    void BottomPanel::addArpBlock()
    {
        auto& graph = owner.getProject().getDspGraph();
        bool hasArp = false;
        for (const auto& block : graph.blocks)
        {
            if (block.enabled && ArpeggiatorRuntime::isArpBlock (block))
            {
                hasArp = true;
                break;
            }
        }

        if (! hasArp)
        {
            DspBlock arpBlock;
            arpBlock.id = "arp_" + juce::String (juce::Random::getSystemRandom().nextInt (99999));
            arpBlock.section = "mod";
            arpBlock.type = "arp";
            arpBlock.name = "Arpeggiator";
            arpBlock.enabled = true;
            arpBlock.values = {
                { "rate", 1.0f },
                { "sync", 1.0f },
                { "arpSteps", 8.0f },
                { "arpGate", 0.55f },
                { "arpPattern", 0.0f },
                { "arpOctaves", 2.0f },
                { "arpNote0", 0.0f },
                { "arpNote1", 4.0f },
                { "arpNote2", 7.0f },
                { "arpNote3", 12.0f },
                { "arpNote4", 7.0f },
                { "arpNote5", 4.0f },
                { "arpNote6", 10.0f },
                { "arpNote7", 14.0f }
            };
            graph.blocks.push_back (std::move (arpBlock));
            owner.getProject().notifyChanged();
        }

        setPage (Page::DSP);
        if (globalGraphView)
            globalGraphView->rebuild();
    }

    void BottomPanel::refresh()
    {
        if (workflowPage) workflowPage->refresh();
        if (projectBrowserPage) projectBrowserPage->refresh();
        if (parameters)  parameters->refresh();
        if (sampleMapper) sampleMapper->refresh();
        if (keyzones)    keyzones->refresh();
        if (velocityMap)
        {
            velocityMap->setZones (owner.getProject().getSampleMap().getZones());
            velocityMap->setSelectedZone (sampleMapper != nullptr ? sampleMapper->getSelectedZoneIndex() : -1);
        }
        if (oneShotMaker) oneShotMaker->refresh();
        if (midiPlayground) midiPlayground->refresh();
        if (animationLab) animationLab->refresh();
        if (launchCenter) launchCenter->refresh();
        if (expansionsPage) expansionsPage->refresh();
        repaint();
    }

    void BottomPanel::refreshDesignSelection()
    {
        if (parameters != nullptr)
            parameters->refresh();
    }

    void BottomPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        // Top divider
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 0, getWidth(), 1);
    }

    void BottomPanel::rebuildPageVisibility()
    {
        const bool workflow  = currentPage == Page::Dashboard;
        const bool projectBrowser = currentPage == Page::ProjectBrowser;
        const bool design    = currentPage == Page::Design;
        const bool mapper    = currentPage == Page::Samples;
        const bool oneShot   = currentPage == Page::OneShotMaker;
        const bool midi      = currentPage == Page::MidiPlayground || currentPage == Page::ArpStudio;
        const bool test      = currentPage == Page::Test;
        const bool build     = currentPage == Page::Widgets;
        const bool animation = currentPage == Page::Animation;
        const bool brand     = currentPage == Page::Branding;
        const bool launch    = currentPage == Page::Export;
        const bool expansions= currentPage == Page::Expansions;
        const bool graph     = currentPage == Page::DSP;

        if (workflowPage) workflowPage->setVisible (workflow);
        if (projectBrowserPage) projectBrowserPage->setVisible (projectBrowser);
        designDspHeader     .setVisible (design);
        if (parameters) parameters->setVisible (design);
        if (globalGraphView) globalGraphView->setVisible (graph);

        btnMapperMain    .setVisible (mapper);
        btnMapperKeyzones.setVisible (mapper);
        btnMapperVelocity.setVisible (mapper);
        btnMapperFull    .setVisible (mapper);
        if (mapper) rebuildMapperSubVisibility();
        else
        {
            if (sampleMapper)        sampleMapper->setVisible (false);
            if (keyzones)            keyzones->setVisible (false);
            if (velocityMap)          velocityMap->setVisible (false);
        }

        if (oneShotMaker) oneShotMaker->setVisible (oneShot);
        if (midiPlayground) midiPlayground->setVisible (midi);
        if (testPage) testPage->setVisible (test);
        if (launchCenter) launchCenter->setVisible (launch);
        if (expansionsPage) expansionsPage->setVisible (expansions);

        if (builder) builder->setVisible (build);
        if (animationLab) animationLab->setVisible (animation);
        if (brandingLab)
        {
            brandingLab->setVisible (brand);
            if (brand)
            {
                brandingLab->refresh();
                // Start the live engine + hardware MIDI callbacks immediately so a
                // connected MIDI keyboard triggers sound and lights the on-screen
                // keys without first requiring a mouse click in the preview.
                brandingLab->activateTest();
            }
            else
                brandingLab->deactivateTest();
        }

        resized();
        repaint();
    }

    void BottomPanel::rebuildMapperSubVisibility()
    {
        if (sampleMapper)        sampleMapper->setVisible (activeMapperSubTab == 0);
        if (keyzones)            keyzones->setVisible    (activeMapperSubTab == 1);
        if (velocityMap)
        {
            velocityMap->setZones (owner.getProject().getSampleMap().getZones());
            velocityMap->setSelectedZone (sampleMapper != nullptr ? sampleMapper->getSelectedZoneIndex() : -1);
            velocityMap->setVisible (activeMapperSubTab == 2);
        }
    }

    void BottomPanel::resized()
    {
        auto r = getLocalBounds().reduced (1, 0);

        switch (currentPage)
        {
            case Page::Dashboard:
            {
                if (workflowPage) workflowPage->setBounds (r);
                break;
            }

            case Page::ProjectBrowser:
            {
                if (projectBrowserPage) projectBrowserPage->setBounds (r);
                break;
            }

            case Page::DSP:
            {
                if (globalGraphView) globalGraphView->setBounds (r);
                break;
            }

            case Page::Design:
            {
                auto full = r.reduced (4);
                designDspHeader.setBounds (full.removeFromTop (22));
                if (parameters) parameters->setBounds (full);
                break;
            }

            case Page::Samples:
            {
                auto top = r.removeFromTop (28);
                btnMapperMain    .setBounds (top.removeFromLeft (130));
                top.removeFromLeft (4);
                btnMapperKeyzones.setBounds (top.removeFromLeft (90));
                top.removeFromLeft (4);
                btnMapperVelocity.setBounds (top.removeFromLeft (90));
                top.removeFromLeft (4);
                btnMapperFull.setBounds (top.removeFromLeft (64));

                auto content = r.reduced (4);
                if (sampleMapper)        sampleMapper->setBounds (content);
                if (keyzones)            keyzones->setBounds    (content);
                if (velocityMap)          velocityMap->setBounds (content);
                break;
            }

            case Page::MidiPlayground:
            case Page::ArpStudio:
            {
                if (midiPlayground) midiPlayground->setBounds (r);
                break;
            }

            case Page::OneShotMaker:
            {
                if (oneShotMaker) oneShotMaker->setBounds (r);
                break;
            }

            case Page::Test:
            {
                if (testPage) testPage->setBounds (r);
                break;
            }

            case Page::Widgets:
            {
                if (builder) builder->setBounds (r);
                break;
            }

            case Page::Animation:
            {
                if (animationLab) animationLab->setBounds (r);
                break;
            }

            case Page::Branding:
            {
                if (brandingLab) brandingLab->setBounds (r);
                break;
            }

            case Page::Export:
            {
                if (launchCenter) launchCenter->setBounds (r);
                break;
            }

            case Page::Expansions:
            {
                if (expansionsPage) expansionsPage->setBounds (r);
                break;
            }
        }
    }

} // namespace patchcraft
