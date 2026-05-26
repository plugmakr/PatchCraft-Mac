#include "BottomPanel.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>

#include "SampleMapEditor.h"
#include "KeyzonesComponent.h"
#include "VelocityMapViewer.h"
#include "ParametersComponent.h"
#include "ControlBuilderComponent.h"
#include "PresetsComponent.h"
#include "TestPage.h"
#include "BrandingLabPage.h"
#include "DspPage.h"
#include "MidiPlaygroundPage.h"
#include "OneShotMakerPage.h"
#include "WorkflowPage.h"
#include "LaunchCenterPage.h"
#include "AnimationLabPage.h"

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

        // Design page -------------------------------------------------------
        designDspHeader.setText ("DSP QUICK EDIT", juce::dontSendNotification);
        designDspHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        designDspHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (designDspHeader);

        // The Presets list lives in the right tab panel (next to Layers and
        // Inspector) now, so the bottom panel's Design page is just the DSP
        // quick-edit grid at full width.
        parameters = std::make_unique<ParametersComponent> (owner);
        designDspPage = std::make_unique<DspPage> (owner.getProject(), true, &owner);
        addAndMakeVisible (*designDspPage);

        // Sample Mapper page ------------------------------------------------
        styleSubTab (btnMapperMain,     8810);
        styleSubTab (btnMapperKeyzones, 8810);
        styleSubTab (btnMapperVelocity, 8810);
        btnMapperMain.setToggleState (true, juce::dontSendNotification);
        btnMapperMain.onClick     = [this] { activeMapperSubTab = 0; rebuildMapperSubVisibility(); };
        btnMapperKeyzones.onClick = [this] { activeMapperSubTab = 1; rebuildMapperSubVisibility(); };
        btnMapperVelocity.onClick = [this] { activeMapperSubTab = 2; rebuildMapperSubVisibility(); };
        addAndMakeVisible (btnMapperMain);
        addAndMakeVisible (btnMapperKeyzones);
        addAndMakeVisible (btnMapperVelocity);

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

        // DSP page -----------------------------------------------------------
        dspPage = std::make_unique<DspPage> (owner.getProject(), false, &owner);
        dspPage->onPatchSectionChanged = [this] { owner.refreshCanvasToolbar(); };
        addChildComponent (*dspPage);

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

        rebuildPageVisibility();
    }

    BottomPanel::~BottomPanel() = default;

    void BottomPanel::setPage (Page p)
    {
        if (currentPage == p) return;
        currentPage = p;
        if (midiPlayground)
        {
            if (p == Page::ArpStudio)
                midiPlayground->showArpStudioMode();
            else if (p == Page::MidiPlayground)
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
        return dspPage != nullptr ? dspPage->getCurrentPatchSectionId() : juce::String ("source");
    }

    juce::String BottomPanel::getDspPatchSectionLabel() const
    {
        return dspPage != nullptr ? dspPage->getCurrentPatchSectionLabel() : juce::String ("Source");
    }

    void BottomPanel::showDspBuilderTutorial()
    {
        currentPage = Page::DSP;
        rebuildPageVisibility();
        if (dspPage != nullptr)
            dspPage->beginGuidedTutorial();
    }

    void BottomPanel::addArpBlock()
    {
        currentPage = Page::DSP;
        rebuildPageVisibility();
        if (dspPage != nullptr)
            dspPage->addArpBlock();
    }

    void BottomPanel::refresh()
    {
        if (workflowPage) workflowPage->refresh();
        if (parameters)  parameters->refresh();
        if (sampleMapper) sampleMapper->refresh();
        if (keyzones)    keyzones->refresh();
        if (velocityMap)
        {
            velocityMap->setZones (owner.getProject().getSampleMap().getZones());
            velocityMap->setSelectedZone (sampleMapper != nullptr ? sampleMapper->getSelectedZoneIndex() : -1);
        }
        if (oneShotMaker) oneShotMaker->refresh();
        if (designDspPage) designDspPage->refresh();
        if (midiPlayground) midiPlayground->refresh();
        if (dspPage) dspPage->refresh();
        if (animationLab) animationLab->refresh();
        if (launchCenter) launchCenter->refresh();
        repaint();
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
        const bool workflow = currentPage == Page::Dashboard;
        const bool design  = currentPage == Page::Design;
        const bool mapper  = currentPage == Page::Samples;
        const bool oneShot = currentPage == Page::OneShotMaker;
        const bool midi    = currentPage == Page::MidiPlayground || currentPage == Page::ArpStudio;
        const bool dsp     = currentPage == Page::DSP;
        const bool build   = currentPage == Page::Widgets;
        const bool animation = currentPage == Page::Animation;
        const bool launch  = currentPage == Page::Export;
        // "Test" now routes to the Brand Lab — the developer's live test
        // environment is the same surface as their branding workspace.
        const bool brand   = currentPage == Page::Branding;
        const bool test    = currentPage == Page::Test;

        if (workflowPage) workflowPage->setVisible (workflow);
        designDspHeader     .setVisible (design);
        if (parameters) parameters->setVisible (false);
        if (designDspPage) designDspPage->setVisible (design);

        btnMapperMain    .setVisible (mapper);
        btnMapperKeyzones.setVisible (mapper);
        btnMapperVelocity.setVisible (mapper);
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
        if (dspPage) dspPage->setVisible (dsp);
        if (launchCenter) launchCenter->setVisible (launch);

        if (builder) builder->setVisible (build);
        if (animationLab) animationLab->setVisible (animation);
        if (brandingLab)
        {
            brandingLab->setVisible (brand);
            if (brand)
                brandingLab->refresh();
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
        resized();
        repaint();
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

            case Page::Design:
            {
                // Presets now live in the right tab panel, so the DSP quick
                // edit gets the full width of the bottom panel.
                auto full = r.reduced (4);
                designDspHeader.setBounds (full.removeFromTop (22));
                if (designDspPage) designDspPage->setBounds (full);
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
                // Test page is now folded into the Brand Lab — route the
                if (testPage) testPage->setBounds (r);
                break;
            }

            case Page::DSP:
            {
                if (dspPage) dspPage->setBounds (r);
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
        }
    }

} // namespace patchcraft
