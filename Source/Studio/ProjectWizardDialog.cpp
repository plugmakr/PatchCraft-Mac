#include "ProjectWizardDialog.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    ProjectWizardDialog::ChoiceButton::ChoiceButton (const juce::String& name, const juce::String& b)
        : juce::TextButton (name), blurb (b)
    {
        getProperties().set ("flatTab", true);
        getProperties().set ("fontSize", 13.0);
        getProperties().set ("bold", true);
    }

    ProjectWizardDialog::ProjectWizardDialog (std::function<void (ProductKind)> onChosen)
        : callback (std::move (onChosen))
    {
        title.setText ("What are you building?", juce::dontSendNotification);
        title.setFont (juce::Font (22.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        addAndMakeVisible (title);

        subtitle.setText ("Pick a type first. Sounds, Design, Brand, and Ship all follow from that choice.",
                          juce::dontSendNotification);
        subtitle.setFont (juce::Font (13.5f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        auto makeChoice = [this] (const juce::String& name, const juce::String& blurb, ProductKind kind)
        {
            auto btn = std::make_unique<ChoiceButton> (name, blurb);
            btn->setTooltip (blurb);
            btn->onClick = [this, kind]
            {
                if (callback)
                    callback (kind);
            };
            addAndMakeVisible (*btn);
            return btn;
        };

        synthBtn  = makeChoice ("Synth",
                                "Playable synth products with no samples required.",
                                ProductKind::SynthInstrument);
        sampleBtn = makeChoice ("Sample Instrument",
                                "Keys, pads, one-shots, guitars, cinematic tools.",
                                ProductKind::SampleInstrument);
        drumBtn   = makeChoice ("Drum Machine",
                                "Pad grids and drum kits mapped to MIDI.",
                                ProductKind::DrumMachine);
        loopBtn   = makeChoice ("Loop Chopper",
                                "Import a loop, chop slices, map them to pads/keys.",
                                ProductKind::LoopChopInstrument);
        hybridBtn = makeChoice ("Hybrid",
                                "Samples plus synth layers in one instrument.",
                                ProductKind::HybridInstrument);
        fxBtn     = makeChoice ("FX Plugin",
                                "EQ, delay, reverb, lo-fi, mastering, vocal FX.",
                                ProductKind::FXPlugin);
    }

    void ProjectWizardDialog::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel().brighter (0.04f));
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRect (getLocalBounds(), 1);
    }

    void ProjectWizardDialog::resized()
    {
        auto r = getLocalBounds().reduced (24);
        title.setBounds (r.removeFromTop (34));
        r.removeFromTop (6);
        subtitle.setBounds (r.removeFromTop (40));
        r.removeFromTop (12);

        const int gap = 6;
        const int rowH = 48;
        for (auto* btn : { synthBtn.get(), sampleBtn.get(), drumBtn.get(),
                           loopBtn.get(), hybridBtn.get(), fxBtn.get() })
        {
            if (btn != nullptr)
                btn->setBounds (r.removeFromTop (rowH));
            r.removeFromTop (gap);
        }
    }

    void ProjectWizardDialog::show (juce::Component* parent, std::function<void (ProductKind)> onChosen)
    {
        auto* dialog = new ProjectWizardDialog (std::move (onChosen));
        dialog->setSize (520, 480);

        juce::DialogWindow::LaunchOptions opts;
        opts.dialogTitle                   = "Choose Instrument Type";
        opts.dialogBackgroundColour        = PatchCraftLookAndFeel::panel();
        opts.escapeKeyTriggersCloseButton  = true;
        opts.useNativeTitleBar             = true;
        opts.resizable                     = false;
        opts.content.setOwned (dialog);

        if (parent != nullptr)
            opts.componentToCentreAround = parent;

        if (auto* window = opts.launchAsync())
            window->centreWithSize (520, 480);
    }

} // namespace patchcraft
