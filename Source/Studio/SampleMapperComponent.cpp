#include "SampleMapperComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "SfzImporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace patchcraft
{
    static juce::Colour zoneColour (int idx)
    {
        const juce::uint32 cols[] = {
            (juce::uint32) PatchCraftLookAndFeel::kZoneA,
            (juce::uint32) PatchCraftLookAndFeel::kZoneB,
            (juce::uint32) PatchCraftLookAndFeel::kZoneC,
            (juce::uint32) PatchCraftLookAndFeel::kZoneD
        };
        return juce::Colour (cols[idx % 4]);
    }

    juce::String SampleMapperComponent::noteName (int midi)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int oct = midi / 12 - 1;
        return juce::String (names[midi % 12]) + juce::String (oct);
    }

    void SampleMapperComponent::noteToCombo (juce::ComboBox& cb)
    {
        cb.clear();
        for (int n = 0; n < 128; ++n) cb.addItem (noteName (n), n + 1);
    }

    SampleMapperComponent::SampleMapperComponent (StudioMainComponent& o) : owner (o)
    {
        samplesHeader.setText ("Samples", juce::dontSendNotification);
        samplesHeader.setFont (juce::Font (11.0f, juce::Font::bold));
        samplesHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (samplesHeader);

        samplesList.setRowHeight (26);
        samplesList.setColour (juce::ListBox::backgroundColourId, PatchCraftLookAndFeel::panelAlt());
        samplesList.setOutlineThickness (1);
        addAndMakeVisible (samplesList);

        addSampleBtn.setButtonText ("+ Add Sample");
        addSampleBtn.onClick = [this] { importSample(); };
        addAndMakeVisible (addSampleBtn);

        auto styleEdit = [] (juce::TextEditor& e)
        {
            e.setIndents (6, 4);
        };

        for (auto* l : { &rootLbl, &lowLbl, &highLbl, &lowVelLbl, &highVelLbl,
                         &gainLbl, &panLbl, &loopLbl, &startLbl, &endLbl,
                         &rrGroupLbl, &rrIndexLbl, &sampleStartLbl, &sampleEndLbl,
                         &fadeInStartLbl, &fadeInLenLbl, &fadeOutStartLbl, &fadeOutLenLbl,
                         &pitchOffsetLbl, &velXFadeLowerLbl, &velXFadeUpperLbl,
                         &priorityLbl, &groupLbl })
        {
            l->setFont (juce::Font (10.5f));
            l->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            l->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (*l);
        }
        rootLbl.setText ("Root Note", juce::dontSendNotification);
        lowLbl.setText  ("Low Note",  juce::dontSendNotification);
        highLbl.setText ("High Note", juce::dontSendNotification);
        lowVelLbl.setText ("Low Vel",   juce::dontSendNotification);
        highVelLbl.setText ("High Vel", juce::dontSendNotification);
        gainLbl.setText ("Gain", juce::dontSendNotification);
        panLbl.setText  ("Pan",  juce::dontSendNotification);
        loopLbl.setText ("Loop", juce::dontSendNotification);
        startLbl.setText ("Start", juce::dontSendNotification);
        endLbl.setText  ("End",  juce::dontSendNotification);
        bpmLbl.setText ("BPM", juce::dontSendNotification);
        // HISE-style advanced labels
        rrGroupLbl.setText ("RR Group", juce::dontSendNotification);
        rrIndexLbl.setText ("RR Index", juce::dontSendNotification);
        sampleStartLbl.setText ("Sample Start", juce::dontSendNotification);
        sampleEndLbl.setText ("Sample End", juce::dontSendNotification);
        fadeInStartLbl.setText ("Fade In Start", juce::dontSendNotification);
        fadeInLenLbl.setText ("Fade In Len", juce::dontSendNotification);
        fadeOutStartLbl.setText ("Fade Out Start", juce::dontSendNotification);
        fadeOutLenLbl.setText ("Fade Out Len", juce::dontSendNotification);
        pitchOffsetLbl.setText ("Pitch Offset", juce::dontSendNotification);
        velXFadeLowerLbl.setText ("Vel XFade Low", juce::dontSendNotification);
        velXFadeUpperLbl.setText ("Vel XFade High", juce::dontSendNotification);
        priorityLbl.setText ("Priority", juce::dontSendNotification);
        groupLbl.setText ("Group", juce::dontSendNotification);

        noteToCombo (rootBox);
        noteToCombo (lowBox);
        noteToCombo (highBox);

        addAndMakeVisible (rootBox);
        addAndMakeVisible (lowBox);
        addAndMakeVisible (highBox);

        for (auto* e : { &lowVelEdit, &highVelEdit, &gainEdit, &panEdit, &startEdit, &endEdit,
                         &bpmEdit, &rrGroupEdit, &rrIndexEdit, &sampleStartEdit, &sampleEndEdit,
                         &fadeInStartEdit, &fadeInLenEdit, &fadeOutStartEdit, &fadeOutLenEdit,
                         &pitchOffsetEdit, &velXFadeLowerEdit, &velXFadeUpperEdit,
                         &priorityEdit, &groupEdit })
        {
            styleEdit (*e);
            addAndMakeVisible (*e);
            e->onTextChange = [this] { writeFromUi(); };
        }
        rootBox.onChange = [this] { writeFromUi(); };
        lowBox.onChange  = [this] { writeFromUi(); };
        highBox.onChange = [this] { writeFromUi(); };

        loopToggle.onClick = [this] { writeFromUi(); };
        reverseToggle.onClick = [this] { writeFromUi(); };
        addAndMakeVisible (loopToggle);
        addAndMakeVisible (reverseToggle);

        importSfzBtn.onClick = [this] { importSfz(); };
        importSfzBtn.setColour (juce::TextButton::buttonColourId,
                                juce::Colour (0xff205830));
        autoMapBtn.onClick    = [this] { autoMap(); };
        addZoneBtn.onClick    = [this] { addZone(); };
        removeZoneBtn.onClick = [this] { removeZone(); };
        removeZoneBtn.setColour (juce::TextButton::buttonColourId,
                                 juce::Colour (0xff582020));
        addAndMakeVisible (importSfzBtn);
        addAndMakeVisible (autoMapBtn);
        addAndMakeVisible (addZoneBtn);
        addAndMakeVisible (removeZoneBtn);

        // Initialize waveform viewer
        waveformViewer = std::make_unique<SampleWaveformViewer>();
        addAndMakeVisible (*waveformViewer);
        waveformViewer->onZoneChanged = [this] { writeFromUi(); };

        refresh();
    }

    void SampleMapperComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());

        // Mini keyboard area is owned by the mapper layout and drawn from current bounds.
        if (auto kbArea = juce::Rectangle<int> (200, 100, getWidth() - 220, getHeight() - 160);
            kbArea.getWidth() > 60 && kbArea.getHeight() > 30)
        {
            // resized() below positions a dedicated keyboard rect; we redraw on top here.
        }
    }

    void SampleMapperComponent::resized()
    {
        auto r = getLocalBounds().reduced (8);

        // Left: sample list
        auto left = r.removeFromLeft (190);
        samplesHeader.setBounds (left.removeFromTop (16));
        addSampleBtn.setBounds (left.removeFromBottom (28));
        left.removeFromBottom (4);
        samplesList.setBounds (left);

        r.removeFromLeft (10);

        // Top zone-properties row (basic fields)
        auto topRow = r.removeFromTop (52);
        const int colW = topRow.getWidth() / 10;
        auto place = [&] (juce::Label& l, juce::Component& c)
        {
            auto col = topRow.removeFromLeft (colW).reduced (2);
            l.setBounds (col.removeFromTop (16));
            c.setBounds (col);
        };
        place (rootLbl, rootBox);
        place (lowLbl,  lowBox);
        place (highLbl, highBox);
        place (lowVelLbl, lowVelEdit);
        place (highVelLbl, highVelEdit);
        place (gainLbl, gainEdit);
        place (panLbl,   panEdit);
        place (loopLbl, loopToggle);
        place (startLbl, startEdit);
        place (endLbl,   endEdit);
        place (bpmLbl,   bpmEdit);

        // Advanced fields row (HISE-style)
        r.removeFromTop (8);
        auto advRow = r.removeFromTop (52);
        const int advColW = advRow.getWidth() / 10;
        auto placeAdv = [&] (juce::Label& l, juce::Component& c)
        {
            auto col = advRow.removeFromLeft (advColW).reduced (2);
            l.setBounds (col.removeFromTop (16));
            c.setBounds (col);
        };
        placeAdv (rrGroupLbl, rrGroupEdit);
        placeAdv (rrIndexLbl, rrIndexEdit);
        placeAdv (sampleStartLbl, sampleStartEdit);
        placeAdv (sampleEndLbl, sampleEndEdit);
        placeAdv (fadeInStartLbl, fadeInStartEdit);
        placeAdv (fadeInLenLbl, fadeInLenEdit);
        placeAdv (fadeOutStartLbl, fadeOutStartEdit);
        placeAdv (fadeOutLenLbl, fadeOutLenEdit);
        placeAdv (pitchOffsetLbl, pitchOffsetEdit);
        // Reverse toggle needs special handling (no label)
        auto revCol = advRow.removeFromLeft (advColW).reduced (2);
        reverseToggle.setBounds (revCol);

        // Second advanced row
        r.removeFromTop (8);
        auto advRow2 = r.removeFromTop (52);
        const int advColW2 = advRow2.getWidth() / 10;
        auto placeAdv2 = [&] (juce::Label& l, juce::Component& c)
        {
            auto col = advRow2.removeFromLeft (advColW2).reduced (2);
            l.setBounds (col.removeFromTop (16));
            c.setBounds (col);
        };
        placeAdv2 (velXFadeLowerLbl, velXFadeLowerEdit);
        placeAdv2 (velXFadeUpperLbl, velXFadeUpperEdit);
        placeAdv2 (priorityLbl, priorityEdit);
        placeAdv2 (groupLbl, groupEdit);
        // Remaining space unused

        // Bottom row of action buttons
        auto bottomRow = r.removeFromBottom (32);
        importSfzBtn.setBounds (bottomRow.removeFromLeft (90));
        bottomRow.removeFromLeft (8);
        autoMapBtn.setBounds   (bottomRow.removeFromLeft (90));
        bottomRow.removeFromLeft (8);
        addZoneBtn.setBounds   (bottomRow.removeFromLeft (90));
        bottomRow.removeFromLeft (8);
        removeZoneBtn.setBounds (bottomRow.removeFromLeft (110));

        // Waveform viewer fills the remaining space
        waveformViewer->setBounds (r);
    }

    int SampleMapperComponent::getNumRows()
    {
        return (int) owner.getProject().getSampleMap().getZones().size();
    }

    void SampleMapperComponent::paintListBoxItem (int row, juce::Graphics& g,
                                                  int w, int h, bool selected)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (row < 0 || row >= (int) zones.size()) return;
        const auto& z = zones[(size_t) row];

        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.18f));
            g.fillRect (0, 0, w, h);
        }

        // colour swatch
        g.setColour (zoneColour (row));
        g.fillRect (0, 0, 4, h);

        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.5f));
        auto name = juce::File (z.samplePath).getFileName();
        if (name.isEmpty()) name = "(empty)";
        g.drawText (name, 12, 0, w - 90, h, juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f));
        g.drawText (noteName (z.lowNote) + " - " + noteName (z.highNote),
                    w - 78, 0, 70, h, juce::Justification::centredRight);
    }

    void SampleMapperComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        selectedZone = row;
        refresh();
    }

    const SampleZoneDef* SampleMapperComponent::getSelectedZone() const
    {
        const auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return nullptr;

        return &zones[(size_t) selectedZone];
    }

    void SampleMapperComponent::writeFromUi()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size()) return;
        auto& z = zones[(size_t) selectedZone];

        z.rootNote = juce::jlimit (0, 127, rootBox.getSelectedId() - 1);
        z.lowNote  = juce::jlimit (0, 127, lowBox.getSelectedId() - 1);
        z.highNote = juce::jlimit (0, 127, highBox.getSelectedId() - 1);
        z.lowVelocity  = juce::jlimit (1, 127, lowVelEdit.getText().getIntValue());
        z.highVelocity = juce::jlimit (1, 127, highVelEdit.getText().getIntValue());
        z.gainDb = gainEdit.getText().getFloatValue();
        z.pan    = juce::jlimit (-1.0f, 1.0f, panEdit.getText().getFloatValue());
        z.loopEnabled = loopToggle.getToggleState();
        z.loopStart   = startEdit.getText().getIntValue();
        z.loopEnd     = endEdit.getText().getIntValue();

        // HISE-style advanced fields
        z.roundRobinGroup = rrGroupEdit.getText().getIntValue();
        z.roundRobinIndex = rrIndexEdit.getText().getIntValue();
        z.sampleStart = sampleStartEdit.getText().getIntValue();
        z.sampleEnd = sampleEndEdit.getText().getIntValue();
        z.fadeInStart = fadeInStartEdit.getText().getIntValue();
        z.fadeInLength = fadeInLenEdit.getText().getIntValue();
        z.fadeOutStart = fadeOutStartEdit.getText().getIntValue();
        z.fadeOutLength = fadeOutLenEdit.getText().getIntValue();
        z.pitchOffset = pitchOffsetEdit.getText().getFloatValue();
        z.velocityLowerVelXFade = velXFadeLowerEdit.getText().getFloatValue();
        z.velocityUpperVelXFade = velXFadeUpperEdit.getText().getFloatValue();
        z.reverse = reverseToggle.getToggleState();
        z.bpm = bpmEdit.getText().getFloatValue();
        z.priority = priorityEdit.getText().getIntValue();
        z.group = groupEdit.getText().trim();

        owner.getProject().notifyChanged();
    }

    void SampleMapperComponent::refresh()
    {
        // Ensure at least 4 demo zones if empty
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (zones.empty())
        {
            const char* names[] = { "Pad_A.wav", "Pad_B.wav", "Pad_C.wav", "Pad_D.wav" };
            const int   roots[] = { 36, 60, 72, 96 };
            const int   lows[]  = { 24, 48, 72, 96 };
            const int   highs[] = { 47, 71, 95, 108 };
            for (int i = 0; i < 4; ++i)
            {
                SampleZoneDef z;
                z.samplePath = names[i];
                z.rootNote = roots[i];
                z.lowNote  = lows[i];
                z.highNote = highs[i];
                zones.push_back (z);
            }
            selectedZone = 1;
        }

        samplesList.updateContent();
        samplesList.selectRow (selectedZone, true, true);

        if (selectedZone >= 0 && selectedZone < (int) zones.size())
        {
            auto& z = zones[(size_t) selectedZone];
            rootBox.setSelectedId (z.rootNote + 1, juce::dontSendNotification);
            lowBox.setSelectedId  (z.lowNote + 1,  juce::dontSendNotification);
            highBox.setSelectedId (z.highNote + 1, juce::dontSendNotification);
            lowVelEdit.setText  (juce::String (z.lowVelocity),  juce::dontSendNotification);
            highVelEdit.setText (juce::String (z.highVelocity), juce::dontSendNotification);
            gainEdit.setText  (juce::String (z.gainDb, 1) + " dB", juce::dontSendNotification);
            panEdit.setText   (z.pan == 0.0f ? "C" : juce::String (z.pan, 2),
                               juce::dontSendNotification);
            startEdit.setText (juce::String (z.loopStart), juce::dontSendNotification);
            endEdit.setText   (juce::String (z.loopEnd),   juce::dontSendNotification);
            loopToggle.setToggleState (z.loopEnabled, juce::dontSendNotification);
            bpmEdit.setText (juce::String (z.bpm, 1), juce::dontSendNotification);

            // HISE-style advanced fields
            rrGroupEdit.setText (juce::String (z.roundRobinGroup), juce::dontSendNotification);
            rrIndexEdit.setText (juce::String (z.roundRobinIndex), juce::dontSendNotification);
            sampleStartEdit.setText (juce::String (z.sampleStart), juce::dontSendNotification);
            sampleEndEdit.setText (juce::String (z.sampleEnd), juce::dontSendNotification);
            fadeInStartEdit.setText (juce::String (z.fadeInStart), juce::dontSendNotification);
            fadeInLenEdit.setText (juce::String (z.fadeInLength), juce::dontSendNotification);
            fadeOutStartEdit.setText (juce::String (z.fadeOutStart), juce::dontSendNotification);
            fadeOutLenEdit.setText (juce::String (z.fadeOutLength), juce::dontSendNotification);
            pitchOffsetEdit.setText (juce::String (z.pitchOffset, 2), juce::dontSendNotification);
            velXFadeLowerEdit.setText (juce::String (z.velocityLowerVelXFade, 2), juce::dontSendNotification);
            velXFadeUpperEdit.setText (juce::String (z.velocityUpperVelXFade, 2), juce::dontSendNotification);
            reverseToggle.setToggleState (z.reverse, juce::dontSendNotification);
            priorityEdit.setText (juce::String (z.priority), juce::dontSendNotification);
            groupEdit.setText (z.group, juce::dontSendNotification);

            // Update waveform viewer with current zone data
            waveformViewer->setZone (z);

            // Try to load the sample file for waveform display
            auto sampleFile = juce::File (z.samplePath);
            if (!sampleFile.existsAsFile())
            {
                // Try relative path from project assets
                auto assetsFolder = owner.getProject().getProjectFolder().getChildFile ("assets");
                sampleFile = assetsFolder.getChildFile (z.samplePath);
            }

            if (sampleFile.existsAsFile())
            {
                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (sampleFile));

                if (reader != nullptr)
                {
                    juce::AudioBuffer<float> buffer (reader->numChannels, (int) reader->lengthInSamples);
                    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
                    waveformViewer->setSampleData (buffer, reader->sampleRate);
                }
            }
        }

        repaint();
    }

    void SampleMapperComponent::importSample()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectMultipleItems
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                for (auto& f : files)
                {
                    SampleZoneDef z;
                    z.samplePath = f.getFullPathName();
                    z.rootNote = 60;
                    z.lowNote  = 0;
                    z.highNote = 127;
                    owner.getProject().getSampleMap().add (z);
                }
                if (! files.isEmpty() && owner.getProject().getEngineType() != "sample")
                    owner.getProject().setEngineType ("sample");
                else
                    owner.getProject().notifyChanged();
                refresh();
            });
    }

    void SampleMapperComponent::importSfz()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import SFZ", juce::File(), "*.sfz");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                if (files.isEmpty()) return;
                auto result = SfzImporter::parseFile (files.getFirst());
                if (! result.success)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::WarningIcon,
                        "SFZ Import",
                        result.warnings.joinIntoString ("\n"));
                    return;
                }
                for (auto& z : result.zones)
                    owner.getProject().getSampleMap().add (z);
                if (owner.getProject().getEngineType() != "sample")
                    owner.getProject().setEngineType ("sample");
                else
                    owner.getProject().notifyChanged();
                refresh();
            });
    }

    void SampleMapperComponent::autoMap()
    {
        owner.getProject().getSampleMap().autoMapAcrossKeyboard();
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapperComponent::addZone()
    {
        SampleZoneDef z;
        z.samplePath = "Sample_" + juce::String (selectedZone + 1) + ".wav";
        z.rootNote = 60;
        z.lowNote = 48;
        z.highNote = 71;
        owner.getProject().getSampleMap().add (z);
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapperComponent::removeZone()
    {
        owner.getProject().getSampleMap().removeAt (selectedZone);
        selectedZone = juce::jmax (0, selectedZone - 1);
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapperComponent::drawZoneKeyboard (juce::Graphics&, juce::Rectangle<int>) const
    {
    }

} // namespace patchcraft
