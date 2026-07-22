#include "ChopLabPage.h"



#include "ChopStudioPanel.h"

#include "SampleMapEditor.h"

#include "SampleSliceUtils.h"

#include "StudioMainComponent.h"

#include "BottomPanel.h"

#include "PackRuntimeHost.h"

#include "PatchCraftLookAndFeel.h"



namespace patchcraft

{

    ChopLabPage::ChopLabPage (StudioMainComponent& o) : owner (o)

    {

        header.setText ("Chop", juce::dontSendNotification);

        header.setFont (juce::Font (18.0f, juce::Font::bold));

        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());

        addAndMakeVisible (header);



        subtitle.setText ("Serato-style sample chopping — one WAV, up to 32 slices. Apply writes cue points to the Sound map.",

                          juce::dontSendNotification);

        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());

        subtitle.setFont (juce::Font (11.5f));

        addAndMakeVisible (subtitle);



        for (auto* b : { &importBtn, &soundBtn, &layoutBtn })

        {

            b->getProperties().set ("smallButton", true);

            addAndMakeVisible (*b);

        }



        keyLockToggle.setTooltip ("When on, pads play at fixed pitch (no keyboard tracking).");

        bpmSyncToggle.setTooltip ("Stretch chops to match host tempo using the sample BPM.");

        addAndMakeVisible (keyLockToggle);

        addAndMakeVisible (bpmSyncToggle);



        statusLabel.setFont (juce::Font (11.0f));

        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());

        addAndMakeVisible (statusLabel);



        importBtn.onClick = [this] { importSample(); };

        soundBtn.onClick = [this] { openSoundMapper(); };

        layoutBtn.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Design); };



        keyLockToggle.onClick = [this]

        {

            auto& lv = owner.getProject().getLiveValues();

            const bool locked = keyLockToggle.getToggleState();

            lv.setValue ("samplePitch", 0.0f);

            statusLabel.setText (locked ? "Key lock on — use Layout to tune global pitch." : "Key lock off.",

                                 juce::dontSendNotification);

        };



        bpmSyncToggle.onClick = [this]

        {

            owner.getProject().getLiveValues().setValue ("bpmSync", bpmSyncToggle.getToggleState() ? 1.0f : 0.0f);

        };



        chopPanel = std::make_unique<ChopStudioPanel> (owner.getAudio().getDeviceManager(),

            [this] (juce::String& err) { return owner.getAudio().ensureOpen (err); });

        chopPanel->onApply = [this] (const std::vector<int>& b, double bpm) { applySlices (b, bpm); };

        addAndMakeVisible (*chopPanel);



        bpmSyncToggle.setToggleState (owner.getProject().getLiveValues().getValue ("bpmSync", 1.0f) >= 0.5f,

                                      juce::dontSendNotification);

    }



    ChopLabPage::~ChopLabPage() = default;



    void ChopLabPage::paint (juce::Graphics& g)

    {

        g.fillAll (PatchCraftLookAndFeel::panel());

        g.setColour (PatchCraftLookAndFeel::border());

        g.fillRect (0, 0, getWidth(), 1);

    }



    void ChopLabPage::resized()

    {

        auto r = getLocalBounds().reduced (12, 8);

        auto top = r.removeFromTop (52);

        header.setBounds (top.removeFromTop (24));

        subtitle.setBounds (top);



        auto tool = r.removeFromTop (34);

        importBtn.setBounds (tool.removeFromLeft (120).reduced (2));

        tool.removeFromLeft (6);

        soundBtn.setBounds (tool.removeFromLeft (140).reduced (2));

        tool.removeFromLeft (6);

        layoutBtn.setBounds (tool.removeFromLeft (110).reduced (2));

        tool.removeFromLeft (12);

        keyLockToggle.setBounds (tool.removeFromLeft (100).reduced (2));

        tool.removeFromLeft (6);

        bpmSyncToggle.setBounds (tool.removeFromLeft (100).reduced (2));

        statusLabel.setBounds (tool.reduced (2));



        r.removeFromTop (6);

        chopPanel->setBounds (r);

    }



    void ChopLabPage::refresh()

    {

        bpmSyncToggle.setToggleState (owner.getProject().getLiveValues().getValue ("bpmSync", 1.0f) >= 0.5f,

                                      juce::dontSendNotification);

        reloadChopSource();

    }



    void ChopLabPage::reloadChopSource()

    {

        if (chopPanel == nullptr)

            return;



        juce::AudioBuffer<float> buffer;

        double rate = 44100.0;

        SampleZoneDef zone;

        if (! SampleMapEditor::loadChopSourceFromProject (owner, buffer, rate, zone))

        {

            statusLabel.setText ("Import a sample or use Import Sample above.", juce::dontSendNotification);

            return;

        }



        chopPanel->setSource (buffer, rate, owner.getProject().getProjectFolder(), zone);

        statusLabel.setText ("Editing: " + zone.samplePath, juce::dontSendNotification);

    }



    void ChopLabPage::applySlices (const std::vector<int>& boundaries, double bpm)

    {

        if (SampleMapEditor::commitCuePointChop (owner, boundaries, bpm, keyLockToggle.getToggleState()))

        {

            statusLabel.setText ("Applied " + juce::String (juce::jmin (kMaxChopPads, (int) boundaries.size() - 1))

                                + " slices to the sample map.", juce::dontSendNotification);

            statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());

            owner.getProject().notifyChanged();

            reloadChopSource();



            if (auto* runtime = owner.getPackRuntime())

            {

                runtime->requestReloadImmediate();

                if (owner.isGraphAudioListen())

                    runtime->activate();

            }

        }

        else

        {

            statusLabel.setText ("Could not apply slices — import a sample first.", juce::dontSendNotification);

            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));

        }

    }



    void ChopLabPage::importSample()

    {

        juce::Component::SafePointer<ChopLabPage> safeThis (this);

        owner.importSamples ([safeThis] (bool imported)

        {

            if (safeThis == nullptr || ! imported)

                return;



            safeThis->owner.setBottomTab (BottomPanel::Page::Chop);

            safeThis->refresh();

        });

    }



    void ChopLabPage::openSoundMapper()

    {

        owner.openSoundMapperForChopZone (SampleMapEditor::getChopSourceZoneIndex (owner));

    }



} // namespace patchcraft

