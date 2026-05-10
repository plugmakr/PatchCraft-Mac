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
        // Design page -------------------------------------------------------
        designDspHeader.setText ("DSP QUICK EDIT", juce::dontSendNotification);
        designDspHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        designDspHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (designDspHeader);

        designPresetsHeader.setText ("PRESETS", juce::dontSendNotification);
        designPresetsHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        designPresetsHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (designPresetsHeader);

        parameters = std::make_unique<ParametersComponent> (owner);
        presets    = std::make_unique<PresetsComponent>    (owner);
        designDspPage = std::make_unique<DspPage> (owner.getProject(), true, &owner);
        addAndMakeVisible (*designDspPage);
        addAndMakeVisible (*presets);

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

        // Branding Lab -------------------------------------------------------
        brandingLab = std::make_unique<BrandingLabPage> (owner);
        addChildComponent (*brandingLab);

        rebuildPageVisibility();
    }

    BottomPanel::~BottomPanel() = default;

    void BottomPanel::setPage (Page p)
    {
        if (currentPage == p) return;
        currentPage = p;
        rebuildPageVisibility();
    }

    void BottomPanel::setPreviewActive (bool active)
    {
        if (isPreviewActive() == active) return;

        // The Brand Lab now hosts the live test instance; preview audio runs
        // through it. The standalone testPage (still owned here for legacy
        // bottom-panel routing) mirrors the call so existing toolbar
        // integrations keep working in either place.
        if (active)
        {
            if (brandingLab) brandingLab->activateTest();
            if (testPage != nullptr) testPage->activate();
        }
        else
        {
            if (brandingLab) brandingLab->deactivateTest();
            if (testPage != nullptr)
                testPage->deactivate();
        }
    }

    bool BottomPanel::isPreviewActive() const
    {
        return (brandingLab != nullptr && brandingLab->isTestActive())
            || (testPage != nullptr && testPage->isAudioRunning());
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

    void BottomPanel::refresh()
    {
        if (parameters)  parameters->refresh();
        if (presets)     presets->refresh();
        if (sampleMapper) sampleMapper->refresh();
        if (keyzones)    keyzones->refresh();
        if (velocityMap)
        {
            velocityMap->setZones (owner.getProject().getSampleMap().getZones());
            velocityMap->setSelectedZone (sampleMapper != nullptr ? sampleMapper->getSelectedZoneIndex() : -1);
        }
        if (designDspPage) designDspPage->refresh();
        if (midiPlayground) midiPlayground->refresh();
        if (dspPage) dspPage->refresh();
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
        const bool design  = currentPage == Page::Design;
        const bool mapper  = currentPage == Page::SampleMapper;
        const bool midi    = currentPage == Page::MidiPlayground;
        const bool dsp     = currentPage == Page::DSP;
        const bool build   = currentPage == Page::Build;
        // "Test" now routes to the Brand Lab — the developer's live test
        // environment is the same surface as their branding workspace.
        const bool brand   = currentPage == Page::Branding || currentPage == Page::Test;
        const bool test    = false;

        designDspHeader     .setVisible (design);
        designPresetsHeader .setVisible (design);
        if (parameters) parameters->setVisible (false);
        if (designDspPage) designDspPage->setVisible (design);
        if (presets)    presets   ->setVisible (design);

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

        if (midiPlayground) midiPlayground->setVisible (midi);
        if (testPage) testPage->setVisible (test);
        if (dspPage) dspPage->setVisible (dsp);

        if (builder) builder->setVisible (build);
        if (brandingLab)
        {
            brandingLab->setVisible (brand);
            if (brand)
            {
                brandingLab->refresh();
                brandingLab->activateTest();
            }
            else
            {
                brandingLab->deactivateTest();
            }
        }
        juce::ignoreUnused (test);

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
            case Page::Design:
            {
                // Two columns: DSP quick edit (70%) + compact presets (30%).
                const int leftW = juce::jmax (420, r.getWidth() * 7 / 10);
                auto left  = r.removeFromLeft (leftW).reduced (4);
                auto right = r.reduced (4);

                designDspHeader.setBounds (left.removeFromTop (22));
                if (designDspPage) designDspPage->setBounds (left);

                designPresetsHeader.setBounds (right.removeFromTop (22));
                if (presets) presets->setBounds (right);
                break;
            }

            case Page::SampleMapper:
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
            {
                if (midiPlayground) midiPlayground->setBounds (r);
                break;
            }

            case Page::Test:
            {
                // Test page is now folded into the Brand Lab — route the
                // bounds there so older callers that ask for Page::Test
                // still display correctly.
                if (brandingLab) brandingLab->setBounds (r);
                break;
            }

            case Page::DSP:
            {
                if (dspPage) dspPage->setBounds (r);
                break;
            }

            case Page::Build:
            {
                if (builder) builder->setBounds (r);
                break;
            }

            case Page::Branding:
            {
                if (brandingLab) brandingLab->setBounds (r);
                break;
            }
        }
    }

} // namespace patchcraft
