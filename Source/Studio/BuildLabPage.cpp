#include "BuildLabPage.h"
#include "StudioMainComponent.h"
#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"
#include "ProjectWizardDialog.h"

namespace patchcraft
{
    BuildLabPage::BuildLabPage (StudioMainComponent& o) : owner (o)
    {
        productLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        productLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (productLabel);

        changeTypeBtn.getProperties().set ("smallButton", true);
        changeTypeBtn.setTooltip ("Pick a different instrument type (synth, sampler, drums, loop chop, FX).");
        changeTypeBtn.onClick = [this]
        {
            juce::Component::SafePointer<StudioMainComponent> self (&owner);
            ProjectWizardDialog::show (&owner, [self] (ProductKind kind)
            {
                if (auto* component = self.getComponent())
                    component->createProductProject (kind);
            });
        };
        addAndMakeVisible (changeTypeBtn);

        stepHint.setFont (juce::Font (11.5f));
        stepHint.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addChildComponent (stepHint);

        previewContractLabel.setFont (juce::Font (11.0f, juce::Font::italic));
        previewContractLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent().withAlpha (0.85f));
        addChildComponent (previewContractLabel);

        styleStepButton (importBtn, 8820);
        styleStepButton (chopBtn, 8820);
        styleStepButton (stackBtn, 8820);
        styleStepButton (performBtn, 8820);

        importBtn.setButtonText ("1  Import");
        chopBtn.setButtonText ("2  Chop");
        stackBtn.setButtonText ("3  Shape");
        performBtn.setButtonText ("4  Perform");

        importSamplesBtn.getProperties().set ("smallButton", true);
        importSamplesBtn.setTooltip ("Import WAV/AIFF/FLAC samples into this project.");
        importSamplesBtn.onClick = [this]
        {
            owner.importSamples ([this] (bool)
            {
                owner.syncExportPreview();
                refresh();
            });
        };
        addAndMakeVisible (importSamplesBtn);

        advancedMapBtn.getProperties().set ("smallButton", true);
        advancedMapBtn.setTooltip ("Open the full sample mapper (keyzones, velocity, RR). Advanced Build only.");
        advancedMapBtn.onClick = [this]
        {
            owner.setAdvancedBuildUnlocked (true);
            setSubPage (SubPage::ImportSounds);
            if (auto* panel = owner.getBottomPanel())
                panel->setBuildSubPage (SubPage::ImportSounds);
            owner.openSampleMapperZoneManager();
        };
        addAndMakeVisible (advancedMapBtn);

        layerWizardBtn.getProperties().set ("smallButton", true);
        layerWizardBtn.setTooltip ("Create a two-layer Player rack from the current sample map for multi-instrument export.");
        layerWizardBtn.onClick = [this] { owner.showMultiLayerSetupWizard(); };
        addAndMakeVisible (layerWizardBtn);

        importBtn.onClick = [this]
        {
            setSubPage (SubPage::ImportSounds);
            if (auto* panel = owner.getBottomPanel())
                panel->setBuildSubPage (SubPage::ImportSounds);
        };
        chopBtn.onClick = [this]
        {
            setSubPage (SubPage::Chop);
            if (auto* panel = owner.getBottomPanel())
                panel->setBuildSubPage (SubPage::Chop);
        };
        stackBtn.onClick = [this]
        {
            setSubPage (SubPage::Stack);
            if (auto* panel = owner.getBottomPanel())
                panel->setBuildSubPage (SubPage::Stack);
        };
        performBtn.onClick = [this]
        {
            setSubPage (SubPage::Perform);
            if (auto* panel = owner.getBottomPanel())
                panel->setBuildSubPage (SubPage::Perform);
        };

        previewBtn.getProperties().set ("primaryAction", true);
        previewBtn.getProperties().set ("fontSize", 11.5);
        previewBtn.setTooltip ("Open Brand preview — this is what customers see and what exports.");
        previewBtn.onClick = [this]
        {
            owner.syncExportPreview();
            owner.setBottomTab (BottomPanel::Page::Branding);
        };
        addAndMakeVisible (previewBtn);

        shipBtn.getProperties().set ("smallButton", true);
        shipBtn.setTooltip ("Open Ship and export.");
        shipBtn.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Export); };
        addAndMakeVisible (shipBtn);

        addAndMakeVisible (importBtn);
        addAndMakeVisible (chopBtn);
        addAndMakeVisible (stackBtn);
        addAndMakeVisible (performBtn);

        refresh();
        updateStepVisibility();
    }

    void BuildLabPage::styleStepButton (juce::TextButton& b, int radioId)
    {
        b.setClickingTogglesState (true);
        b.setRadioGroupId (radioId);
        b.getProperties().set ("flatTab", true);
        b.getProperties().set ("fontSize", 11.5);
        b.getProperties().set ("bold", true);
    }

    bool BuildLabPage::chopStepAvailable() const
    {
        const auto& manifest = owner.getProject().getManifest();
        const auto id = manifest.productRecipeId;
        return id == "loop_chopper" || id.contains ("chop") || id.contains ("loop")
            || manifest.productKindLabel.containsIgnoreCase ("loop")
            || manifest.productKindLabel.containsIgnoreCase ("chop");
    }

    void BuildLabPage::setSubPage (SubPage page)
    {
        if (page == SubPage::Chop && ! chopStepAvailable())
            page = SubPage::ImportSounds;

        activeSubPage = page;
        importBtn.setToggleState (page == SubPage::ImportSounds, juce::dontSendNotification);
        chopBtn.setToggleState (page == SubPage::Chop, juce::dontSendNotification);
        stackBtn.setToggleState (page == SubPage::Stack, juce::dontSendNotification);
        performBtn.setToggleState (page == SubPage::Perform, juce::dontSendNotification);
    }

    void BuildLabPage::refresh()
    {
        const auto& manifest = owner.getProject().getManifest();
        const auto kind = manifest.productKindLabel.isNotEmpty()
                            ? manifest.productKindLabel
                            : (manifest.category.isNotEmpty() ? manifest.category : juce::String ("Instrument"));
        productLabel.setText ("Sounds · " + kind, juce::dontSendNotification);

        switch (activeSubPage)
        {
            case SubPage::ImportSounds:
                stepHint.setText ("Import samples, then map them simply. Use Advanced Mapping only if you need keyzones/velocity layers.",
                                  juce::dontSendNotification);
                break;
            case SubPage::Chop:
                stepHint.setText ("Chop the loop into playable slices for this product type.",
                                  juce::dontSendNotification);
                break;
            case SubPage::Stack:
                stepHint.setText ("Shape the sound stack — filters, FX, and motion that ship with the product.",
                                  juce::dontSendNotification);
                break;
            case SubPage::Perform:
                stepHint.setText ("Optional performance patterns (Advanced Build).",
                                  juce::dontSendNotification);
                break;
        }

        updateStepVisibility();
    }

    void BuildLabPage::updateStepVisibility()
    {
        const bool chop = chopStepAvailable();
        const bool advanced = owner.isAdvancedBuildUnlocked();
        chopBtn.setVisible (chop);
        performBtn.setVisible (advanced);
        advancedMapBtn.setVisible (advanced);
        layerWizardBtn.setVisible (advanced);
        stepHint.setVisible (true);
    }

    void BuildLabPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel().darker (0.06f));
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void BuildLabPage::resized()
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto top = r.removeFromTop (18);
        productLabel.setBounds (top.removeFromLeft (juce::jmax (160, getWidth() / 4)));
        top.removeFromLeft (6);
        changeTypeBtn.setBounds (top.removeFromLeft (88).reduced (0, 1));
        shipBtn.setBounds (top.removeFromRight (52).reduced (0, 1));
        top.removeFromRight (4);
        previewBtn.setBounds (top.removeFromRight (130).reduced (0, 1));

        r.removeFromTop (4);
        stepHint.setBounds (r.removeFromTop (16));
        previewContractLabel.setBounds ({});
        r.removeFromTop (4);

        auto steps = r.removeFromTop (24);
        importBtn.setBounds (steps.removeFromLeft (88));
        steps.removeFromLeft (4);
        if (chopBtn.isVisible())
        {
            chopBtn.setBounds (steps.removeFromLeft (80));
            steps.removeFromLeft (4);
        }
        stackBtn.setBounds (steps.removeFromLeft (88));
        steps.removeFromLeft (4);
        if (performBtn.isVisible())
            performBtn.setBounds (steps.removeFromLeft (88));

        auto actions = r.removeFromTop (26);
        importSamplesBtn.setBounds (actions.removeFromLeft (118).reduced (0, 1));
        actions.removeFromLeft (6);
        if (advancedMapBtn.isVisible())
        {
            advancedMapBtn.setBounds (actions.removeFromLeft (150).reduced (0, 1));
            actions.removeFromLeft (6);
        }
        if (layerWizardBtn.isVisible())
            layerWizardBtn.setBounds (actions.removeFromLeft (112).reduced (0, 1));
    }

} // namespace patchcraft
