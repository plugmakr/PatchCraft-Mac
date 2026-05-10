#include "SampleMapEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "SampleMap.h"
#include "PatchCraftProject.h"
#include "SampleSynthEngine.h"
#include "SampleWaveformViewer.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <map>

namespace patchcraft
{
    //============================================================================
    // StepperControl implementation
    //============================================================================
    SampleMapEditor::StepperControl::StepperControl (const juce::String& name)
    {
        nameLabel.setText (name, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (nameLabel);
        addAndMakeVisible (minusBtn);
        addAndMakeVisible (plusBtn);
        addAndMakeVisible (valueLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setText ("0", juce::dontSendNotification);

        minusBtn.onClick = [this] { setValue (value - 1); };
        plusBtn.onClick = [this] { setValue (value + 1); };
    }

    void SampleMapEditor::StepperControl::resized()
    {
        auto r = getLocalBounds();
        nameLabel.setBounds (r.removeFromTop (16));
        r.removeFromTop (2);

        minusBtn.setBounds (r.removeFromLeft (24));
        r.removeFromLeft (2);
        plusBtn.setBounds (r.removeFromRight (24));
        r.removeFromRight (2);
        valueLabel.setBounds (r);
    }

    void SampleMapEditor::StepperControl::setValue (int v, bool notify)
    {
        value = juce::jlimit (minValue, maxValue, v);
        valueLabel.setText (juce::String (value), juce::dontSendNotification);
        if (notify && onChange)
            onChange (value);
    }

    //============================================================================
    // SampleMapEditor implementation
    //============================================================================
    SampleMapEditor::SampleMapEditor (StudioMainComponent& owner)
        : owner (owner)
    {
        setOpaque (true);

        // Toolbar
        // Main toolbar
        addAndMakeVisible (editModeBtn);
        addAndMakeVisible (autoBtn);
        addAndMakeVisible (zoneSoloBtn);
        addAndMakeVisible (selectByMidiBtn);
        addAndMakeVisible (gridViewBtn);
        addAndMakeVisible (listViewBtn);
        addAndMakeVisible (importBtn);
        addAndMakeVisible (removeBtn);
        addAndMakeVisible (selectAllBtn);
        addAndMakeVisible (clearAllBtn);
        addAndMakeVisible (autoMapBtn);
        addAndMakeVisible (mapPresetBox);

        // Map preset dropdown — replaces the standalone Pad Map / Glitch Kit
        // buttons with a grouped picker so the toolbar stays compact and the
        // user can discover variants (different pad counts, slice strategies,
        // normalize) without hunting through right-click menus.
        mapPresetBox.setTextWhenNothingSelected ("Apply Preset...");
        mapPresetBox.addSectionHeading ("Pad Map");
        mapPresetBox.addItem ("16 pads from C1 (one-shot)",  101);
        mapPresetBox.addItem ("16 pads from C2 (one-shot)",  102);
        mapPresetBox.addItem ("8 pads from C1 (one-shot)",   103);
        mapPresetBox.addSeparator();
        mapPresetBox.addSectionHeading ("Slice (Glitch Kit)");
        mapPresetBox.addItem ("16 slices  (transient)",      201);
        mapPresetBox.addItem ("8 slices  (transient)",       202);
        mapPresetBox.addItem ("16 slices  (equal grid)",     211);
        mapPresetBox.addItem ("8 slices  (equal grid)",      212);
        mapPresetBox.addItem ("32 slices  (equal grid)",     213);
        mapPresetBox.addSeparator();
        mapPresetBox.addSectionHeading ("Build Patch");
        mapPresetBox.addItem ("Build Drum Kit (auto)",       401);
        mapPresetBox.addItem ("Build Multi-Sample Patch",    402);
        mapPresetBox.addSeparator();
        mapPresetBox.addSectionHeading ("Levels");
        mapPresetBox.addItem ("Normalize selected to 0 dB",  301);
        mapPresetBox.onChange = [this]
        {
            const int id = mapPresetBox.getSelectedId();
            if (id > 0)
            {
                applyMapPreset (id);
                // Reset to placeholder so re-selecting the same preset still
                // fires onChange.
                mapPresetBox.setSelectedId (0, juce::dontSendNotification);
            }
        };
        addAndMakeVisible (zoomInBtn);
        addAndMakeVisible (zoomOutBtn);
        addAndMakeVisible (auditionBtn);
        addAndMakeVisible (stopAuditionBtn);
        addAndMakeVisible (loopToggle);
        addAndMakeVisible (fadeInBtn);
        addAndMakeVisible (fadeOutBtn);
        addAndMakeVisible (zoomFitBtn);

        editModeBtn.onClick = [this] { showEditMenu(); };
        autoBtn.onClick = [this] { autoMap(); };
        zoneSoloBtn.setClickingTogglesState (true);
        zoneSoloBtn.onClick = [this]
        {
            zoneSoloEnabled = zoneSoloBtn.getToggleState();
            repaint();
        };
        selectByMidiBtn.setClickingTogglesState (true);
        selectByMidiBtn.onClick = [this] { setMidiZoneSelectEnabled (selectByMidiBtn.getToggleState()); };
        gridViewBtn.setClickingTogglesState (true);
        listViewBtn.setClickingTogglesState (true);
        gridViewBtn.setRadioGroupId (8821);
        listViewBtn.setRadioGroupId (8821);
        gridViewBtn.getProperties().set ("smallButton", true);
        listViewBtn.getProperties().set ("smallButton", true);
        gridViewBtn.setToggleState (true, juce::dontSendNotification);
        gridViewBtn.onClick = [this]
        {
            listViewMode = false;
            gridViewBtn.setToggleState (true, juce::dontSendNotification);
            listViewBtn.setToggleState (false, juce::dontSendNotification);
            resized();
            repaint();
        };
        listViewBtn.onClick = [this]
        {
            listViewMode = true;
            gridViewBtn.setToggleState (false, juce::dontSendNotification);
            listViewBtn.setToggleState (true, juce::dontSendNotification);
            resized();
            repaint();
        };
        importBtn.onClick = [this] { addSample(); };
        removeBtn.onClick = [this] { removeSample(); };
        selectAllBtn.onClick = [this] { selectAllZones(); };
        clearAllBtn.onClick = [this] { clearAllSamples(); };
        autoMapBtn.onClick = [this] { autoMap(); };
        zoomInBtn.onClick = [this] { zoomLevel = juce::jmin (4.0f, zoomLevel * 1.2f); repaint(); };
        zoomOutBtn.onClick = [this] { zoomLevel = juce::jmax (0.5f, zoomLevel / 1.2f); repaint(); };
        auditionBtn.onClick = [this] { auditionSelectedZone(); };
        stopAuditionBtn.onClick = [this] { stopAudition(); };
        loopToggle.onClick = [this] { toggleLoopForSelected(); };
        fadeInBtn.onClick = [this] { addDefaultFade (true); };
        fadeOutBtn.onClick = [this] { addDefaultFade (false); };
        applyImportVelocitySelectedBtn.onClick = [this] { applyImportDefaultVelocity (false); };
        applyImportVelocityAllBtn.onClick = [this] { applyImportDefaultVelocity (true); };
        applyImportVelocitySelectedBtn.setTooltip ("Apply the Import Low/High velocity range to selected zones.");
        applyImportVelocityAllBtn.setTooltip ("Apply the Import Low/High velocity range to every mapped zone.");
        zoomFitBtn.onClick = [this]
        {
            if (waveformViewer != nullptr)
            {
                waveformViewer->setZoomLevel (1.0);
                waveformViewer->setViewOffset (0);
            }
        };

        // Info bar
        addAndMakeVisible (keyRangeLabel);
        addAndMakeVisible (velRangeLabel);
        addAndMakeVisible (rootKeyLabel);
        addAndMakeVisible (sampleNameLabel);
        keyRangeLabel.setFont (juce::FontOptions (12.0f));
        velRangeLabel.setFont (juce::FontOptions (12.0f));
        rootKeyLabel.setFont (juce::FontOptions (12.0f));
        sampleNameLabel.setFont (juce::FontOptions (12.0f));

        addAndMakeVisible (healthFixEngineBtn);
        addAndMakeVisible (healthAutoMapBtn);
        addAndMakeVisible (healthFindMissingBtn);
        addAndMakeVisible (healthGoTestBtn);
        healthFixEngineBtn.onClick = [this] { fixHealthEngine(); };
        healthAutoMapBtn.onClick = [this] { autoMapFromHealth(); };
        healthFindMissingBtn.onClick = [this] { findMissingSamplesFromHealth(); };
        healthGoTestBtn.onClick = [this] { goToTestFromHealth(); };
        
        // Sample list (for list view mode)
        addChildComponent (samplesList);
        addChildComponent (samplesHeader);
        samplesHeader.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        samplesHeader.setFont (juce::Font (12.0f, juce::Font::bold));
        samplesList.setColour (juce::ListBox::backgroundColourId, PatchCraftLookAndFeel::panel());
        samplesList.setMultipleSelectionEnabled (true);

        // Bottom panel - Source section controls
        addAndMakeVisible (sourceMode);
        sourceMode.addItem ("DFD", 1);
        sourceMode.addItem ("Sampler", 2);
        sourceMode.setSelectedId (1);
        addAndMakeVisible (reverseBtn);
        reverseBtn.setClickingTogglesState (true);
        reverseBtn.onClick = [this] { toggleReverseForSelected(); };
        addAndMakeVisible (oneShotToggle);
        oneShotToggle.onClick = [this] { updateZoneFromSteppers(); };
        addAndMakeVisible (tuneStepper);
        addAndMakeVisible (trackStepper);
        addAndMakeVisible (padIndexStepper);
        addAndMakeVisible (chokeGroupStepper);
        addAndMakeVisible (triggerChanceStepper);
        tuneStepper.minValue = -100; tuneStepper.maxValue = 100;
        trackStepper.minValue = -100; trackStepper.maxValue = 100;
        padIndexStepper.minValue = -1; padIndexStepper.maxValue = 15;
        chokeGroupStepper.minValue = 0; chokeGroupStepper.maxValue = 127;
        triggerChanceStepper.minValue = 0; triggerChanceStepper.maxValue = 100;
        
        // Bottom panel - Amplifier section
        addAndMakeVisible (volumeStepper);
        addAndMakeVisible (panStepper);
        volumeStepper.minValue = -48; volumeStepper.maxValue = 24;
        panStepper.minValue = -64; panStepper.maxValue = 64;
        trackStepper.setEnabled (false);
        trackStepper.nameLabel.setTooltip ("Key tracking is planned for the advanced sampler engine. Root key and tune are active now.");
        trackStepper.valueLabel.setTooltip ("Key tracking is planned for the advanced sampler engine. Root key and tune are active now.");

        // Set up stepper callbacks
        auto onStep = [this] (int) { updateZoneFromSteppers(); };
        rrGroupStepper.onChange = onStep;
        rootStepper.onChange = onStep;
        loKeyStepper.onChange = onStep;
        hiKeyStepper.onChange = onStep;
        loVelStepper.onChange = onStep;
        hiVelStepper.onChange = onStep;
        gainStepper.onChange = [this] (int v)
        {
            volumeStepper.setValue (v, false);
            updateZoneFromSteppers();
        };
        volumeStepper.onChange = [this] (int v)
        {
            gainStepper.setValue (v, false);
            updateZoneFromSteppers();
        };
        panStepper.onChange = onStep;
        loopStartStepper.onChange = onStep;
        loopEndStepper.onChange = onStep;
        sampleStartStepper.onChange = onStep;
        sampleEndStepper.onChange = onStep;
        tuneStepper.onChange = onStep;
        trackStepper.onChange = onStep;
        padIndexStepper.onChange = onStep;
        chokeGroupStepper.onChange = onStep;
        triggerChanceStepper.onChange = onStep;
        
        // Zone editing steppers (shown in properties panel, not bottom row)
        rrGroupStepper.minValue = 0; rrGroupStepper.maxValue = 127;
        rootStepper.minValue = 0; rootStepper.maxValue = 127;
        loKeyStepper.minValue = 0; loKeyStepper.maxValue = 127;
        hiKeyStepper.minValue = 0; hiKeyStepper.maxValue = 127;
        loVelStepper.minValue = 1; loVelStepper.maxValue = 127;
        hiVelStepper.minValue = 1; hiVelStepper.maxValue = 127;
        importLowVelocityStepper.minValue = 1; importLowVelocityStepper.maxValue = 127;
        importHighVelocityStepper.minValue = 1; importHighVelocityStepper.maxValue = 127;
        importLowVelocityStepper.setValue (1, false);
        importHighVelocityStepper.setValue (127, false);
        gainStepper.minValue = -48; gainStepper.maxValue = 24;
        loopStartStepper.minValue = 0; loopStartStepper.maxValue = 2000000;
        loopEndStepper.minValue = 0; loopEndStepper.maxValue = 2000000;
        sampleStartStepper.minValue = 0; sampleStartStepper.maxValue = 2000000;
        sampleEndStepper.minValue = 0; sampleEndStepper.maxValue = 2000000;

        addAndMakeVisible (rrGroupStepper);
        addAndMakeVisible (rootStepper);
        addAndMakeVisible (loKeyStepper);
        addAndMakeVisible (hiKeyStepper);
        addAndMakeVisible (loVelStepper);
        addAndMakeVisible (hiVelStepper);
        addAndMakeVisible (importLowVelocityStepper);
        addAndMakeVisible (importHighVelocityStepper);
        addAndMakeVisible (applyImportVelocitySelectedBtn);
        addAndMakeVisible (applyImportVelocityAllBtn);
        addAndMakeVisible (gainStepper);
        addAndMakeVisible (loopStartStepper);
        addAndMakeVisible (loopEndStepper);
        addAndMakeVisible (sampleStartStepper);
        addAndMakeVisible (sampleEndStepper);

        waveformViewer = std::make_unique<SampleWaveformViewer>();
        waveformViewer->onZoneChanged = [this] { applyWaveformZoneToSelected(); };
        addAndMakeVisible (*waveformViewer);
        waveformStatus.setFont (juce::FontOptions (11.0f));
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (waveformStatus);

        refresh();
    }

    SampleMapEditor::~SampleMapEditor()
    {
        setMidiZoneSelectEnabled (false);
        stopAudition();
    }

    juce::Colour SampleMapEditor::getZoneColour (int index)
    {
        const juce::uint32 cols[] = {
            (juce::uint32) PatchCraftLookAndFeel::kZoneA,
            (juce::uint32) PatchCraftLookAndFeel::kZoneB,
            (juce::uint32) PatchCraftLookAndFeel::kZoneC,
            (juce::uint32) PatchCraftLookAndFeel::kZoneD
        };
        return juce::Colour (cols[index % 4]);
    }

    juce::String SampleMapEditor::noteToString (int midiNote)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int oct = midiNote / 12 - 2;
        return juce::String (names[midiNote % 12]) + juce::String (oct);
    }

    int SampleMapEditor::stringToNote (const juce::String& str)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        auto s = str.trim().toUpperCase();
        for (int oct = -2; oct < 10; ++oct)
            for (int n = 0; n < 12; ++n)
                if (s == juce::String (names[n]) + juce::String (oct))
                    return (oct + 2) * 12 + n;
        return -1;
    }

    void SampleMapEditor::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        if (! healthBounds.isEmpty())
            paintHealthPanel (g, healthBounds);

        if (! editPanelBounds.isEmpty())
            paintEditPanel (g, editPanelBounds);

        if (! drumPadBounds.isEmpty())
            paintDrumPadGrid (g, drumPadBounds);

        // Paint zone grid
        if (! listViewMode && gridBounds.getWidth() > 0 && gridBounds.getHeight() > 0)
        {
            paintZoneGrid (g, gridBounds);
        }
    }

    SampleMapEditor::HealthStatus SampleMapEditor::computeHealthStatus() const
    {
        return SampleMap::evaluateHealth (owner.getProject().getSampleMap(),
                                          owner.getProject().getProjectFolder(),
                                          owner.getProject().getEngineType());
    }

    void SampleMapEditor::paintHealthPanel (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto status = computeHealthStatus();
        auto panel = bounds.reduced (0, 2);
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (panel.toFloat(), 6.0f);
        g.setColour (status.exportReady ? PatchCraftLookAndFeel::accent().withAlpha (0.55f)
                                        : juce::Colour (0xffffc857).withAlpha (0.7f));
        g.drawRoundedRectangle (panel.toFloat(), 6.0f, 1.0f);

        auto r = panel.reduced (10, 7);
        auto title = r.removeFromLeft (138);
        if (r.getWidth() > 740)
        {
            r.removeFromRight (326);
            r.removeFromRight (8);
        }

        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText ("SAMPLE HEALTH", title.removeFromTop (18), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (status.exportReady ? PatchCraftLookAndFeel::textDim() : juce::Colour (0xffffc857));
        g.drawText (status.primaryIssue, title, juce::Justification::centredLeft);

        auto drawMetric = [&] (juce::Rectangle<int> area, const juce::String& label,
                              const juce::String& value, juce::Colour colour)
        {
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.7f));
            g.fillRoundedRectangle (area.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (area.toFloat(), 5.0f, 1.0f);
            auto inner = area.reduced (8, 4);
            g.setFont (juce::FontOptions (9.5f));
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.drawText (label, inner.removeFromTop (14), juce::Justification::centredLeft);
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.setColour (colour);
            g.drawText (value, inner, juce::Justification::centredLeft);
        };

        const int gap = 6;
        const int metricW = juce::jmax (118, (r.getWidth() - gap * 3) / 4);
        const auto good = PatchCraftLookAndFeel::accent();
        const auto warn = juce::Colour (0xffffc857);
        const auto bad = juce::Colour (0xffe6504a);

        auto metric = r.removeFromLeft (metricW);
        drawMetric (metric, "Zones", juce::String (status.playableZones) + "/" + juce::String (status.totalZones),
                    status.totalZones > 0 && status.playableZones == status.totalZones ? good : warn);
        r.removeFromLeft (gap);

        metric = r.removeFromLeft (metricW);
        drawMetric (metric, "Files", status.missingFiles == 0 ? "All found" : juce::String (status.missingFiles) + " missing",
                    status.missingFiles == 0 ? good : bad);
        r.removeFromLeft (gap);

        juce::String coverage = status.coveredNotes > 0
            ? noteToString (status.firstCoveredNote) + "-" + noteToString (status.lastCoveredNote)
                + " (" + juce::String (status.coveredNotes) + ")"
            : "None";
        metric = r.removeFromLeft (metricW);
        drawMetric (metric, "Key Coverage", coverage, status.coveredNotes > 0 ? good : warn);
        r.removeFromLeft (gap);

        juce::String exportText = status.exportReady ? "Ready" : "Blocked";
        if (! status.engineIsSample && status.totalZones > 0)
            exportText = "Engine";
        metric = r.removeFromLeft (metricW);
        drawMetric (metric, "Export", exportText, status.exportReady ? good : bad);
    }

    void SampleMapEditor::paintEditCard (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        auto card = bounds.toFloat();
        g.setColour (juce::Colour (0xff12161c));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.22f));
        g.drawRoundedRectangle (card, 8.0f, 1.25f);

        auto header = bounds.reduced (10, 7).removeFromTop (18);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText (title.toUpperCase(), header, juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.75f));
        g.fillRect (bounds.withY (bounds.getY() + 31).withHeight (2).reduced (10, 0));
    }

    void SampleMapEditor::paintEditPanel (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto panel = bounds.reduced (0, 2);
        g.setColour (juce::Colour (0xff0c0f13));
        g.fillRoundedRectangle (panel.toFloat(), 9.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.18f));
        g.drawRoundedRectangle (panel.toFloat(), 9.0f, 1.0f);

        auto r = panel.reduced (10);
        auto titleRow = r.removeFromTop (24);
        g.setFont (juce::Font (12.5f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.drawText ("ZONE EDITOR", titleRow.removeFromLeft (110), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.drawText ("Select a zone, edit mapping/playback, or set Import Low/High before importing samples.",
                    titleRow, juce::Justification::centredLeft);

        r.removeFromTop (6);
        const int gap = 8;
        const int totalW = r.getWidth();
        const int mappingW = juce::jlimit (420, 560, totalW / 3);
        const int playbackW = juce::jlimit (220, 300, totalW / 6);
        const int ampW = juce::jlimit (170, 220, totalW / 8);

        auto mappingCard = r.removeFromLeft (mappingW);
        r.removeFromLeft (gap);
        auto playbackCard = r.removeFromLeft (playbackW);
        r.removeFromLeft (gap);
        auto ampCard = r.removeFromLeft (ampW);
        r.removeFromLeft (gap);
        auto boundsCard = r;

        paintEditCard (g, mappingCard, "Mapping");
        paintEditCard (g, playbackCard, "Playback");
        paintEditCard (g, ampCard, "Amp");
        paintEditCard (g, boundsCard, "Bounds + Loop");
    }

    int SampleMapEditor::zoneIndexForPad (int padIndex) const
    {
        const auto& zones = owner.getProject().getSampleMap().getZones();
        for (int i = 0; i < (int) zones.size(); ++i)
            if (zones[(size_t) i].padIndex == padIndex)
                return i;
        return -1;
    }

    int SampleMapEditor::padIndexAtPosition (juce::Point<int> position) const
    {
        if (! drumPadBounds.contains (position))
            return -1;

        auto r = drumPadBounds.reduced (10);
        r.removeFromTop (24);
        const int columns = 8;
        const int rows = 2;
        const int gap = 6;
        const int padW = (r.getWidth() - gap * (columns - 1)) / columns;
        const int padH = (r.getHeight() - gap * (rows - 1)) / rows;
        if (padW <= 0 || padH <= 0)
            return -1;

        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < columns; ++col)
            {
                const int index = row * columns + col;
                juce::Rectangle<int> pad (r.getX() + col * (padW + gap),
                                          r.getY() + row * (padH + gap),
                                          padW,
                                          padH);
                if (pad.contains (position))
                    return index;
            }

        return -1;
    }

    void SampleMapEditor::paintDrumPadGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto panel = bounds.reduced (0, 2);
        g.setColour (juce::Colour (0xff0c0f13));
        g.fillRoundedRectangle (panel.toFloat(), 9.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.2f));
        g.drawRoundedRectangle (panel.toFloat(), 9.0f, 1.0f);

        auto r = panel.reduced (10);
        auto title = r.removeFromTop (22);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::accent());
        g.drawText ("DRUM PADS / PERFORMANCE", title.removeFromLeft (180), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.drawText ("Pad Map turns imported samples into one-shot pads. Glitch Kit slices one sample across pads.",
                    title, juce::Justification::centredLeft);

        const auto& zones = owner.getProject().getSampleMap().getZones();
        const int columns = 8;
        const int rows = 2;
        const int gap = 6;
        const int padW = (r.getWidth() - gap * (columns - 1)) / columns;
        const int padH = (r.getHeight() - gap * (rows - 1)) / rows;

        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < columns; ++col)
            {
                const int padIndex = row * columns + col;
                const int zoneIndex = zoneIndexForPad (padIndex);
                const bool hasZone = zoneIndex >= 0 && zoneIndex < (int) zones.size();
                const bool selected = hasZone && isZoneSelected (zoneIndex);
                const auto area = juce::Rectangle<int> (r.getX() + col * (padW + gap),
                                                        r.getY() + row * (padH + gap),
                                                        padW,
                                                        padH).toFloat();

                g.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.32f)
                                      : hasZone ? juce::Colour (0xff171d24)
                                                : juce::Colour (0xff10141a));
                g.fillRoundedRectangle (area, 7.0f);
                g.setColour (selected ? PatchCraftLookAndFeel::accent()
                                      : hasZone ? PatchCraftLookAndFeel::border().brighter (0.35f)
                                                : PatchCraftLookAndFeel::border().withAlpha (0.7f));
                g.drawRoundedRectangle (area, 7.0f, selected ? 2.0f : 1.0f);

                auto text = area.toNearestInt().reduced (8, 5);
                g.setFont (juce::Font (11.0f, juce::Font::bold));
                g.setColour (hasZone ? PatchCraftLookAndFeel::textBright() : PatchCraftLookAndFeel::textDim());
                const auto note = noteToString (36 + padIndex);
                const auto label = hasZone
                    ? (zones[(size_t) zoneIndex].padLabel.isNotEmpty()
                        ? zones[(size_t) zoneIndex].padLabel
                        : juce::File (zones[(size_t) zoneIndex].samplePath).getFileNameWithoutExtension())
                    : "Empty";
                g.drawText ("Pad " + juce::String (padIndex + 1) + "  " + note,
                            text.removeFromTop (16), juce::Justification::centredLeft);
                g.setFont (juce::FontOptions (10.0f));
                g.setColour (hasZone ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim());
                g.drawFittedText (label, text.removeFromTop (16), juce::Justification::centredLeft, 1);
                if (hasZone)
                {
                    const auto& zone = zones[(size_t) zoneIndex];
                    g.setFont (juce::FontOptions (9.0f));
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    juce::String flags = zone.oneShot ? "One-shot" : "Gate";
                    if (zone.chokeGroup > 0)
                        flags << "  Choke " << zone.chokeGroup;
                    if (zone.triggerProbability < 100)
                        flags << "  " << zone.triggerProbability << "%";
                    g.drawFittedText (flags, text, juce::Justification::centredLeft, 1);
                }
            }
    }

    void SampleMapEditor::updateHealthButtons()
    {
        const auto status = computeHealthStatus();
        healthFixEngineBtn.setEnabled (status.totalZones > 0 && ! status.engineIsSample);
        healthAutoMapBtn.setEnabled (status.totalZones > 0);
        healthFindMissingBtn.setEnabled (status.missingFiles > 0);
        healthGoTestBtn.setEnabled (status.playableZones > 0 || status.totalZones == 0);

        healthFixEngineBtn.setTooltip (healthFixEngineBtn.isEnabled()
            ? "Switch this project to the Sampler engine so mapped zones can play."
            : "Project is already using the Sampler engine, or no zones are mapped.");
        healthAutoMapBtn.setTooltip (status.totalZones > 0
            ? "Rebuild key ranges from detected root notes."
            : "Import samples before auto mapping.");
        healthFindMissingBtn.setTooltip (status.missingFiles > 0
            ? "Choose a folder and relink missing samples by filename."
            : "All mapped sample files currently resolve.");
        healthGoTestBtn.setTooltip ("Open the Test page to play this map with software or hardware MIDI.");
    }

    void SampleMapEditor::fixHealthEngine()
    {
        if (owner.getProject().getEngineType() == "sample")
            return;

        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("Switch to Sampler")
                .withMessage ("This switches the project engine to Sampler so Sample Mapper zones can play. It may replace the current engine parameter template and layout. Continue?")
                .withButton ("Switch")
                .withButton ("Cancel")
                .withIconType (juce::MessageBoxIconType::QuestionIcon),
            [safeThis = juce::Component::SafePointer<SampleMapEditor> (this)] (int result)
            {
                if (result != 1 || safeThis == nullptr)
                    return;

                safeThis->owner.getProject().setEngineType ("sample");
                safeThis->refresh();
            });
    }

    void SampleMapEditor::autoMapFromHealth()
    {
        autoMap();
        updateHealthButtons();
    }

    void SampleMapEditor::findMissingSamplesFromHealth()
    {
        if (computeHealthStatus().missingFiles <= 0)
            return;

        auto chooser = std::make_shared<juce::FileChooser> ("Choose folder containing the missing samples",
                                                            juce::File(),
                                                            "*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis = juce::Component::SafePointer<SampleMapEditor> (this), chooser] (const juce::FileChooser& fc)
            {
                auto* editor = safeThis.getComponent();
                if (editor == nullptr)
                    return;

                const auto root = fc.getResult();
                if (! root.isDirectory())
                    return;

                juce::Array<juce::File> candidates;
                for (const auto& wildcard : { "*.wav", "*.aif", "*.aiff", "*.flac" })
                    root.findChildFiles (candidates, juce::File::findFiles, true, wildcard);

                auto& zones = editor->owner.getProject().getSampleMap().getZones();
                int relinked = 0;
                for (auto& zone : zones)
                {
                    if (editor->resolveSampleFile (zone).existsAsFile())
                        continue;

                    const auto wantedName = juce::File (zone.samplePath).getFileName();
                    if (wantedName.isEmpty())
                        continue;

                    for (const auto& candidate : candidates)
                    {
                        if (candidate.getFileName().equalsIgnoreCase (wantedName))
                        {
                            zone.samplePath = candidate.getFullPathName();
                            ++relinked;
                            break;
                        }
                    }
                }

                if (relinked > 0)
                {
                    editor->owner.getProject().notifyChanged();
                    editor->waveformStatus.setText ("Relinked " + juce::String (relinked) + " missing sample"
                                                    + (relinked == 1 ? "." : "s."),
                                                    juce::dontSendNotification);
                    editor->waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                }
                else
                {
                    editor->waveformStatus.setText ("No missing samples matched by filename in that folder.",
                                                    juce::dontSendNotification);
                    editor->waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffffc857));
                }

                editor->refresh();
            });
    }

    void SampleMapEditor::goToTestFromHealth()
    {
        owner.setBottomTab (BottomPanel::Page::Test);
    }

    void SampleMapEditor::paintZoneGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // Background
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRect (bounds);

        // Grid lines
        const float noteWidth = 12.0f * zoomLevel;
        const int startNote = scrollOffset;
        const int endNote = startNote + (int) (bounds.getWidth() / noteWidth) + 1;
        const auto zoneArea = getZoneArea (bounds);

        // Vertical grid lines (semitones)
        for (int n = startNote; n <= endNote && n < 128; ++n)
        {
            float x = bounds.getX() + (n - startNote) * noteWidth;
            if (x > bounds.getRight()) break;

            // Octave lines darker
            bool isOctave = (n % 12 == 0);
            g.setColour (isOctave ? juce::Colour (0xff3a3a3a) : juce::Colour (0xff2a2a2a));
            g.drawVerticalLine ((int) x, (float) bounds.getY(), (float) bounds.getBottom());

            // Note labels at top
            if (isOctave)
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (10.0f));
                g.drawText (noteToString (n), (int) x + 2, bounds.getY() + 2, 30, 12, juce::Justification::left);
            }
        }

        // Velocity lanes
        g.setFont (juce::FontOptions (9.5f));
        for (int velocity : { 127, 96, 64, 32, 1 })
        {
            const float normalised = (127.0f - (float) velocity) / 126.0f;
            const int y = zoneArea.getY() + juce::roundToInt (normalised * (float) zoneArea.getHeight());
            g.setColour (velocity == 127 || velocity == 1
                ? juce::Colour (0xff3a3a3a)
                : juce::Colour (0xff2d2d2d));
            g.drawHorizontalLine (y, (float) zoneArea.getX(), (float) zoneArea.getRight());
            g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.7f));
            g.drawText ("V" + juce::String (velocity), zoneArea.getX() + 3, y - 10, 28, 18,
                        juce::Justification::centredLeft);
        }

        // Draw zones
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (zones.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("Import WAV/AIFF/FLAC samples. Filenames like Pad_C3_vel1-80_RR1.wav are parsed automatically.",
                        bounds.reduced (20), juce::Justification::centred);
        }
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            if (zoneSoloEnabled && selectedZone >= 0 && (int) i != selectedZone)
                continue;
            if (z.highNote < startNote || z.lowNote > endNote) continue;

            auto zoneRect = zoneRectFor (z, zoneArea, startNote, noteWidth);
            float w = zoneRect.getWidth();

            bool isSelected = isZoneSelected ((int) i);
            juce::Colour zoneCol = getZoneColour ((int) i);

            // Fill
            g.setColour (zoneCol.withAlpha (isSelected ? 0.6f : 0.3f));
            g.fillRect (zoneRect);

            // Border
            g.setColour (isSelected ? PatchCraftLookAndFeel::accent() : zoneCol);
            g.drawRect (zoneRect, isSelected ? 2 : 1);

            const float velocityHandleHeight = juce::jmin (8.0f, juce::jmax (4.0f, zoneRect.getHeight() * 0.18f));
            const auto topHandle = zoneRect.withHeight (velocityHandleHeight);
            const auto bottomHandle = zoneRect.withY (zoneRect.getBottom() - velocityHandleHeight)
                                              .withHeight (velocityHandleHeight);
            g.setColour ((isSelected ? PatchCraftLookAndFeel::accent() : zoneCol.brighter (0.3f)).withAlpha (0.88f));
            g.fillRect (topHandle);
            g.fillRect (bottomHandle);
            if (isSelected && zoneRect.getWidth() > 76.0f && zoneRect.getHeight() > 34.0f)
            {
                g.setFont (juce::FontOptions (8.5f));
                g.setColour (juce::Colours::black.withAlpha (0.78f));
                g.drawText ("VEL HIGH", topHandle.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
                g.drawText ("VEL LOW", bottomHandle.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
            }

            // Root note highlight
            float rootX = bounds.getX() + (z.rootNote - startNote) * noteWidth;
            g.setColour (juce::Colours::white);
            g.drawVerticalLine ((int) rootX, zoneRect.getY(), zoneRect.getBottom());

            // Sample name
            if (w > 40.0f)
            {
                g.setColour (juce::Colours::white);
                g.setFont (juce::FontOptions (11.0f));
                juce::String name = juce::File (z.samplePath).getFileNameWithoutExtension();
                g.drawText (name, zoneRect.reduced (4.0f), juce::Justification::topLeft);

                g.setFont (juce::FontOptions (9.5f));
                g.setColour (juce::Colours::white.withAlpha (0.72f));
                auto meta = noteToString (z.rootNote) + "  V" + juce::String (z.lowVelocity) + "-"
                          + juce::String (z.highVelocity);
                if (z.roundRobinGroup > 0)
                    meta += "  RR" + juce::String (z.roundRobinIndex > 0 ? z.roundRobinIndex : z.roundRobinGroup);
                if (z.reverse)
                    meta += "  REV";
                g.drawText (meta, zoneRect.reduced (4.0f), juce::Justification::bottomLeft);
            }
        }

        // Draw keyboard at bottom
        paintKeyboard (g, bounds.removeFromBottom (30));
    }

    void SampleMapEditor::paintKeyboard (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const float noteWidth = 12.0f * zoomLevel;
        const int startNote = scrollOffset;

        // White keys background
        g.setColour (juce::Colour (0xffd4c5a9));
        g.fillRect (bounds.toFloat());

        // Draw white keys
        const int whiteKeyIndices[7] = { 0, 2, 4, 5, 7, 9, 11 }; // C, D, E, F, G, A, B
        for (int n = startNote; n < 128; ++n)
        {
            float x = bounds.getX() + (n - startNote) * noteWidth;
            if (x > bounds.getRight()) break;

            bool isBlack = ((n % 12) == 1 || (n % 12) == 3 || (n % 12) == 6 || (n % 12) == 8 || (n % 12) == 10);
            if (!isBlack)
            {
                g.setColour (juce::Colour (0xff8a7958));
                g.drawVerticalLine ((int) x, (float) bounds.getY(), (float) bounds.getBottom());
            }
        }

        // Draw black keys
        for (int n = startNote; n < 128; ++n)
        {
            float x = bounds.getX() + (n - startNote) * noteWidth;
            if (x > bounds.getRight()) break;

            bool isBlack = ((n % 12) == 1 || (n % 12) == 3 || (n % 12) == 6 || (n % 12) == 8 || (n % 12) == 10);
            if (isBlack)
            {
                juce::Rectangle<float> key (x - noteWidth * 0.3f, (float) bounds.getY(), noteWidth * 0.6f, bounds.getHeight() * 0.65f);
                g.setColour (juce::Colour (0xff141413));
                g.fillRect (key);
            }
        }
    }

    void SampleMapEditor::resized()
    {
        auto r = getLocalBounds().reduced (4);
        
        // Main toolbar at top
        auto toolbar = r.removeFromTop (28);
        editModeBtn.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (4);
        autoBtn.setBounds (toolbar.removeFromLeft (50));
        toolbar.removeFromLeft (4);
        zoneSoloBtn.setBounds (toolbar.removeFromLeft (70));
        toolbar.removeFromLeft (4);
        selectByMidiBtn.setBounds (toolbar.removeFromLeft (110));
        toolbar.removeFromLeft (8);
        gridViewBtn.setBounds (toolbar.removeFromLeft (50));
        toolbar.removeFromLeft (3);
        listViewBtn.setBounds (toolbar.removeFromLeft (68));
        toolbar.removeFromLeft (6);
        importBtn.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (4);
        removeBtn.setBounds (toolbar.removeFromLeft (74));
        toolbar.removeFromLeft (4);
        selectAllBtn.setBounds (toolbar.removeFromLeft (70));
        toolbar.removeFromLeft (4);
        clearAllBtn.setBounds (toolbar.removeFromLeft (64));
        toolbar.removeFromLeft (4);
        autoMapBtn.setBounds (toolbar.removeFromLeft (70));
        toolbar.removeFromLeft (4);
        mapPresetBox.setBounds (toolbar.removeFromLeft (220));
        toolbar.removeFromLeft (8);
        zoomOutBtn.setBounds (toolbar.removeFromLeft (28));
        toolbar.removeFromLeft (2);
        zoomInBtn.setBounds (toolbar.removeFromLeft (28));
        toolbar.removeFromLeft (10);
        auditionBtn.setBounds (toolbar.removeFromLeft (74));
        toolbar.removeFromLeft (4);
        stopAuditionBtn.setBounds (toolbar.removeFromLeft (54));

        // Info bar
        r.removeFromTop (4);
        auto infoBar = r.removeFromTop (20);
        keyRangeLabel.setBounds (infoBar.removeFromLeft (120));
        infoBar.removeFromLeft (16);
        velRangeLabel.setBounds (infoBar.removeFromLeft (120));
        infoBar.removeFromLeft (16);
        rootKeyLabel.setBounds (infoBar.removeFromLeft (100));
        infoBar.removeFromLeft (16);
        sampleNameLabel.setBounds (infoBar);

        r.removeFromTop (4);
        healthBounds = r.removeFromTop (62);
        auto healthActions = healthBounds.reduced (10, 8).removeFromRight (316);
        const int buttonW = 76;
        healthFixEngineBtn.setBounds (healthActions.removeFromLeft (buttonW));
        healthActions.removeFromLeft (4);
        healthAutoMapBtn.setBounds (healthActions.removeFromLeft (buttonW));
        healthActions.removeFromLeft (4);
        healthFindMissingBtn.setBounds (healthActions.removeFromLeft (buttonW + 16));
        healthActions.removeFromLeft (4);
        healthGoTestBtn.setBounds (healthActions.removeFromLeft (buttonW));
        r.removeFromTop (4);
        drumPadBounds = r.removeFromTop (112).reduced (0, 2);
        r.removeFromTop (4);

        // Bottom editor: waveform plus professional zone/playback cards.
        editPanelBounds = r.removeFromBottom (224).reduced (0, 2);
        auto waveformArea = r.removeFromBottom (132).reduced (0, 4);
        waveformStatus.setBounds (waveformArea.removeFromTop (18));
        if (waveformViewer != nullptr)
            waveformViewer->setBounds (waveformArea);
        r.removeFromBottom (4);

        auto editor = editPanelBounds.reduced (10);
        editor.removeFromTop (30);
        const int gap = 8;
        const int totalW = editor.getWidth();
        const int mappingW = juce::jlimit (420, 560, totalW / 3);
        const int playbackW = juce::jlimit (220, 300, totalW / 6);
        const int ampW = juce::jlimit (170, 220, totalW / 8);

        auto mappingCard = editor.removeFromLeft (mappingW);
        editor.removeFromLeft (gap);
        auto playbackCard = editor.removeFromLeft (playbackW);
        editor.removeFromLeft (gap);
        auto ampCard = editor.removeFromLeft (ampW);
        editor.removeFromLeft (gap);
        auto boundsCard = editor;

        auto mapping = mappingCard.reduced (10);
        mapping.removeFromTop (28);
        auto mappingRow1 = mapping.removeFromTop (54);
        rootStepper.setBounds (mappingRow1.removeFromLeft (88).reduced (3));
        loKeyStepper.setBounds (mappingRow1.removeFromLeft (88).reduced (3));
        hiKeyStepper.setBounds (mappingRow1.removeFromLeft (88).reduced (3));
        rrGroupStepper.setBounds (mappingRow1.removeFromLeft (88).reduced (3));
        auto mappingRow2 = mapping.removeFromTop (54);
        const int mappingControlW = juce::jmax (70, mappingRow2.getWidth() / 5);
        loVelStepper.setBounds (mappingRow2.removeFromLeft (mappingControlW).reduced (3));
        hiVelStepper.setBounds (mappingRow2.removeFromLeft (mappingControlW).reduced (3));
        padIndexStepper.setBounds (mappingRow2.removeFromLeft (mappingControlW).reduced (3));
        chokeGroupStepper.setBounds (mappingRow2.removeFromLeft (mappingControlW).reduced (3));
        triggerChanceStepper.setBounds (mappingRow2.removeFromLeft (mappingControlW).reduced (3));
        auto mappingRow3 = mapping.removeFromTop (54);
        importLowVelocityStepper.setBounds (mappingRow3.removeFromLeft (88).reduced (3));
        importHighVelocityStepper.setBounds (mappingRow3.removeFromLeft (88).reduced (3));
        applyImportVelocitySelectedBtn.setBounds (mappingRow3.removeFromLeft (86).reduced (3, 14));
        applyImportVelocityAllBtn.setBounds (mappingRow3.removeFromLeft (82).reduced (3, 14));

        auto playback = playbackCard.reduced (10);
        playback.removeFromTop (28);
        sourceMode.setBounds (playback.removeFromTop (26).reduced (2));
        playback.removeFromTop (4);
        auto playbackButtons = playback.removeFromTop (26);
        const int playbackButtonW = (playbackButtons.getWidth() - 12) / 3;
        reverseBtn.setBounds (playbackButtons.removeFromLeft (playbackButtonW).reduced (2));
        playbackButtons.removeFromLeft (6);
        loopToggle.setBounds (playbackButtons.removeFromLeft (playbackButtonW).reduced (2));
        playbackButtons.removeFromLeft (6);
        oneShotToggle.setBounds (playbackButtons.reduced (2));
        auto playbackRow = playback.removeFromTop (56);
        tuneStepper.setBounds (playbackRow.removeFromLeft ((playbackRow.getWidth() - 6) / 2).reduced (3));
        playbackRow.removeFromLeft (6);
        trackStepper.setBounds (playbackRow.reduced (3));

        auto amp = ampCard.reduced (10);
        amp.removeFromTop (28);
        auto ampRow1 = amp.removeFromTop (56);
        gainStepper.setBounds (ampRow1.removeFromLeft ((ampRow1.getWidth() - 6) / 2).reduced (3));
        ampRow1.removeFromLeft (6);
        panStepper.setBounds (ampRow1.reduced (3));
        volumeStepper.setBounds (amp.removeFromTop (56).reduced (3));

        auto boundsControls = boundsCard.reduced (10);
        boundsControls.removeFromTop (28);
        auto boundsRow1 = boundsControls.removeFromTop (54);
        sampleStartStepper.setBounds (boundsRow1.removeFromLeft (110).reduced (3));
        sampleEndStepper.setBounds (boundsRow1.removeFromLeft (110).reduced (3));
        loopStartStepper.setBounds (boundsRow1.removeFromLeft (110).reduced (3));
        loopEndStepper.setBounds (boundsRow1.removeFromLeft (110).reduced (3));
        auto boundsRow2 = boundsControls.removeFromTop (30);
        fadeInBtn.setBounds (boundsRow2.removeFromLeft (88).reduced (2));
        boundsRow2.removeFromLeft (4);
        fadeOutBtn.setBounds (boundsRow2.removeFromLeft (94).reduced (2));
        boundsRow2.removeFromLeft (4);
        zoomFitBtn.setBounds (boundsRow2.removeFromLeft (54).reduced (2));
        
        // Grid fills remaining space
        gridBounds = r;
        
        // List view mode
        if (listViewMode)
        {
            samplesHeader.setVisible (true);
            samplesList.setVisible (true);
            auto listArea = r.reduced (2);
            const auto count = owner.getProject().getSampleMap().getZones().size();
            samplesHeader.setText ("Imported Samples  |  " + juce::String ((int) count)
                                  + " zones  |  " + juce::String (selectedZoneIndexes.size()) + " selected",
                                  juce::dontSendNotification);
            samplesHeader.setBounds (listArea.removeFromTop (24));
            samplesList.setBounds (listArea);
        }
        else
        {
            samplesHeader.setVisible (false);
            samplesList.setVisible (false);
        }
    }

    void SampleMapEditor::refresh()
    {
        samplesList.updateContent();
        samplesList.repaint();
        updateSteppersFromZone();
        updateHealthButtons();
        repaint();
    }

    int SampleMapEditor::getNumRows()
    {
        return (int) owner.getProject().getSampleMap().getZones().size();
    }

    void SampleMapEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (row < 0 || row >= (int) zones.size()) return;

        auto& z = zones[(size_t) row];
        selected = selected || isZoneSelected (row);

        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.3f));
            g.fillRect (0, 0, w, h);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (0, 0, w, h, 1);
        }

        g.setColour (selected ? PatchCraftLookAndFeel::textBright() : PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (12.0f));
        juce::String name = juce::File (z.samplePath).getFileNameWithoutExtension();
        if (name.isEmpty()) name = "<empty>";
        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (8, 4);
        g.drawText (name, r.removeFromTop (18), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.5f));
        g.setColour (selected ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim());
        const auto detail = "Root " + noteToString (z.rootNote)
                          + "  Key " + noteToString (z.lowNote) + "-" + noteToString (z.highNote)
                          + "  Vel " + juce::String (z.lowVelocity) + "-" + juce::String (z.highVelocity)
                          + (z.padIndex >= 0 ? "  Pad " + juce::String (z.padIndex + 1) : "")
                          + (z.oneShot ? "  One-shot" : "")
                          + (z.chokeGroup > 0 ? "  Choke " + juce::String (z.chokeGroup) : "")
                          + (z.roundRobinGroup > 0 ? "  RR" + juce::String (z.roundRobinIndex > 0 ? z.roundRobinIndex : z.roundRobinGroup) : "");
        g.drawText (detail, r, juce::Justification::centredLeft);
    }

    void SampleMapEditor::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        selectZone (row);
    }

    void SampleMapEditor::selectedRowsChanged (int lastRowSelected)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        juce::Array<int> rows;
        for (int i = 0; i < (int) zones.size(); ++i)
            if (samplesList.isRowSelected (i))
                rows.add (i);

        if (rows.isEmpty() && lastRowSelected >= 0 && lastRowSelected < (int) zones.size())
            rows.add (lastRowSelected);

        setSelectedZones (rows, false);
    }

    void SampleMapEditor::selectZone (int index)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        juce::Array<int> indexes;
        if (index >= 0 && index < (int) zones.size())
            indexes.add (index);
        setSelectedZones (indexes, true);
    }

    bool SampleMapEditor::isZoneSelected (int index) const
    {
        return selectedZoneIndexes.contains (index);
    }

    void SampleMapEditor::setSelectedZones (juce::Array<int> indexes, bool syncList)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        juce::Array<int> normalised;
        for (auto index : indexes)
            if (index >= 0 && index < (int) zones.size() && ! normalised.contains (index))
                normalised.add (index);
        normalised.sort();

        selectedZoneIndexes = normalised;
        selectedZone = selectedZoneIndexes.isEmpty() ? -1 : selectedZoneIndexes.getLast();

        if (syncList)
        {
            samplesList.deselectAllRows();
            for (auto index : selectedZoneIndexes)
                samplesList.selectRow (index, true, false);
        }

        updateSteppersFromZone();
        resized();
        repaint();
    }

    void SampleMapEditor::selectAllZones()
    {
        const auto& zones = owner.getProject().getSampleMap().getZones();
        juce::Array<int> indexes;
        for (int i = 0; i < (int) zones.size(); ++i)
            indexes.add (i);
        setSelectedZones (indexes, true);
    }

    void SampleMapEditor::clearZoneSelection()
    {
        setSelectedZones (juce::Array<int>(), true);
    }

    const SampleZoneDef* SampleMapEditor::getSelectedZone() const
    {
        const auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return nullptr;

        return &zones[(size_t) selectedZone];
    }

    void SampleMapEditor::updateSteppersFromZone()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
        {
            rrGroupStepper.setValue (0, false);
            rootStepper.setValue (60, false);
            loKeyStepper.setValue (0, false);
            hiKeyStepper.setValue (127, false);
            loVelStepper.setValue (0, false);
            hiVelStepper.setValue (127, false);
            gainStepper.setValue (0, false);
            volumeStepper.setValue (0, false);
            panStepper.setValue (0, false);
            loopStartStepper.setValue (0, false);
            loopEndStepper.setValue (0, false);
            sampleStartStepper.setValue (0, false);
            sampleEndStepper.setValue (0, false);
            tuneStepper.setValue (0, false);
            padIndexStepper.setValue (-1, false);
            chokeGroupStepper.setValue (0, false);
            triggerChanceStepper.setValue (100, false);
            reverseBtn.setToggleState (false, juce::dontSendNotification);
            loopToggle.setToggleState (false, juce::dontSendNotification);
            oneShotToggle.setToggleState (false, juce::dontSendNotification);
            keyRangeLabel.setText ("Key Range: -", juce::dontSendNotification);
            velRangeLabel.setText ("Vel Range: -", juce::dontSendNotification);
            rootKeyLabel.setText ("Root: -", juce::dontSendNotification);
            sampleNameLabel.setText ("Sample: (none selected)", juce::dontSendNotification);
            sampleNameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
            if (waveformViewer != nullptr)
                waveformViewer->clearSampleData();
            waveformStatus.setText ("Select a zone to edit waveform, loop, fades, and bounds.",
                                    juce::dontSendNotification);
            return;
        }

        auto& z = zones[(size_t) selectedZone];
        rrGroupStepper.setValue (z.roundRobinGroup, false);
        rootStepper.setValue (z.rootNote, false);
        loKeyStepper.setValue (z.lowNote, false);
        hiKeyStepper.setValue (z.highNote, false);
        loVelStepper.setValue (z.lowVelocity, false);
        hiVelStepper.setValue (z.highVelocity, false);
        gainStepper.setValue ((int) z.gainDb, false);
        volumeStepper.setValue ((int) z.gainDb, false);
        panStepper.setValue ((int) (z.pan * 64.0f), false);
        loopStartStepper.setValue (z.loopStart, false);
        loopEndStepper.setValue (z.loopEnd, false);
        sampleStartStepper.setValue (z.sampleStart, false);
        sampleEndStepper.setValue (z.sampleEnd, false);
        tuneStepper.setValue (juce::roundToInt (z.pitchOffset * 100.0f), false);
        padIndexStepper.setValue (juce::jlimit (-1, 15, z.padIndex), false);
        chokeGroupStepper.setValue (juce::jlimit (0, 127, z.chokeGroup), false);
        triggerChanceStepper.setValue (juce::jlimit (0, 100, z.triggerProbability), false);
        reverseBtn.setToggleState (z.reverse, juce::dontSendNotification);
        loopToggle.setToggleState (z.loopEnabled, juce::dontSendNotification);
        oneShotToggle.setToggleState (z.oneShot, juce::dontSendNotification);

        // Update value labels with note names for note-based steppers
        rootStepper.valueLabel.setText (noteToString (z.rootNote), juce::dontSendNotification);
        loKeyStepper.valueLabel.setText (noteToString (z.lowNote), juce::dontSendNotification);
        hiKeyStepper.valueLabel.setText (noteToString (z.highNote), juce::dontSendNotification);
        
        // Update info bar
        keyRangeLabel.setText ("Key Range: " + noteToString (z.lowNote) + " - " + noteToString (z.highNote), juce::dontSendNotification);
        velRangeLabel.setText ("Vel Range: " + juce::String (z.lowVelocity) + " - " + juce::String (z.highVelocity), juce::dontSendNotification);
        rootKeyLabel.setText ("Root: " + noteToString (z.rootNote), juce::dontSendNotification);
        auto sampleFile = juce::File::isAbsolutePath (z.samplePath)
            ? juce::File (z.samplePath)
            : owner.getProject().getProjectFolder().getChildFile (z.samplePath);
        const auto missing = sampleFile.existsAsFile() ? juce::String() : "  [MISSING]";
        sampleNameLabel.setText ("Sample: " + z.samplePath.fromLastOccurrenceOf ("/", false, true).fromLastOccurrenceOf ("\\", false, true) + missing,
                                 juce::dontSendNotification);
        sampleNameLabel.setColour (juce::Label::textColourId,
                                   missing.isEmpty() ? PatchCraftLookAndFeel::text()
                                                     : juce::Colour (0xffe6504a));
        refreshWaveformFromSelectedZone();
    }

    void SampleMapEditor::updateZoneFromSteppers()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size()) return;

        auto& z = zones[(size_t) selectedZone];
        z.roundRobinGroup = rrGroupStepper.value;
        z.rootNote = rootStepper.value;
        z.lowNote = juce::jmin (loKeyStepper.value, hiKeyStepper.value);
        z.highNote = juce::jmax (loKeyStepper.value, hiKeyStepper.value);
        z.lowVelocity = juce::jmin (loVelStepper.value, hiVelStepper.value);
        z.highVelocity = juce::jmax (loVelStepper.value, hiVelStepper.value);
        z.gainDb = (float) gainStepper.value;
        z.pan = juce::jlimit (-1.0f, 1.0f, panStepper.value / 64.0f);
        z.loopStart = loopStartStepper.value;
        z.loopEnd = loopEndStepper.value;
        z.loopEnabled = (z.loopEnd > z.loopStart + 1);
        z.sampleStart = sampleStartStepper.value;
        z.sampleEnd = sampleEndStepper.value;
        z.pitchOffset = tuneStepper.value / 100.0f;
        z.reverse = reverseBtn.getToggleState();
        z.oneShot = oneShotToggle.getToggleState();
        z.padIndex = juce::jlimit (-1, 15, padIndexStepper.value);
        z.chokeGroup = juce::jlimit (0, 127, chokeGroupStepper.value);
        z.triggerProbability = juce::jlimit (0, 100, triggerChanceStepper.value);
        if (z.padIndex >= 0 && z.padLabel.isEmpty())
            z.padLabel = juce::File (z.samplePath).getFileNameWithoutExtension();
        loopToggle.setToggleState (z.loopEnabled, juce::dontSendNotification);

        // Update display strings
        rootStepper.valueLabel.setText (noteToString (z.rootNote), juce::dontSendNotification);
        loKeyStepper.valueLabel.setText (noteToString (z.lowNote), juce::dontSendNotification);
        hiKeyStepper.valueLabel.setText (noteToString (z.highNote), juce::dontSendNotification);

        owner.getProject().notifyChanged();
        if (waveformViewer != nullptr)
            waveformViewer->setZone (z);
        updateHealthButtons();
        repaint();
    }

    void SampleMapEditor::mouseDown (const juce::MouseEvent& e)
    {
        if (drumPadBounds.contains (e.getPosition()))
        {
            const int pad = padIndexAtPosition (e.getPosition());
            const int zoneIndex = zoneIndexForPad (pad);
            if (zoneIndex >= 0)
            {
                selectZone (zoneIndex);
                if (e.getNumberOfClicks() > 1)
                    auditionSelectedZone();
            }
            return;
        }

        if (! gridBounds.contains (e.getPosition())) return;

        // Check if clicking on a zone
        auto& zones = owner.getProject().getSampleMap().getZones();
        const float noteWidth = 12.0f * zoomLevel;
        const int startNote = scrollOffset;
        const auto zoneArea = getZoneArea (gridBounds);

        for (int i = (int) zones.size() - 1; i >= 0; --i)
        {
            auto& z = zones[(size_t) i];
            auto zoneRect = zoneRectFor (z, zoneArea, startNote, noteWidth);

            if (zoneRect.expanded (3.0f, 3.0f).contains ((float) e.x, (float) e.y))
            {
                juce::Array<int> nextSelection = selectedZoneIndexes;
                if (e.mods.isShiftDown() && selectedZone >= 0)
                {
                    nextSelection.clear();
                    const int first = juce::jmin (selectedZone, i);
                    const int last = juce::jmax (selectedZone, i);
                    for (int row = first; row <= last; ++row)
                        nextSelection.add (row);
                }
                else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                {
                    if (nextSelection.contains (i))
                        nextSelection.removeFirstMatchingValue (i);
                    else
                        nextSelection.add (i);
                }
                else
                {
                    nextSelection.clear();
                    nextSelection.add (i);
                }

                setSelectedZones (nextSelection, true);
                if (! isZoneSelected (i))
                    return;

                // Determine drag mode based on where clicked
                const float handleSize = 16.0f;
                const bool tinyVelocityHeight = zoneRect.getHeight() <= handleSize * 1.5f;
                const bool nearVelocityHigh = std::abs ((float) e.y - zoneRect.getY()) < handleSize
                                           || (tinyVelocityHeight && (float) e.y <= zoneRect.getCentreY());
                const bool nearVelocityLow = std::abs ((float) e.y - zoneRect.getBottom()) < handleSize
                                          || (tinyVelocityHeight && (float) e.y > zoneRect.getCentreY());

                if (nearVelocityHigh)
                    dragMode = DragMode::resizeVelocityHigh;
                else if (nearVelocityLow)
                    dragMode = DragMode::resizeVelocityLow;
                else if (std::abs ((float) e.x - zoneRect.getX()) < handleSize)
                    dragMode = DragMode::resizeLeft;
                else if (std::abs ((float) e.x - zoneRect.getRight()) < handleSize)
                    dragMode = DragMode::resizeRight;
                else if (e.mods.isShiftDown())
                    dragMode = DragMode::moveVelocity;
                else
                    dragMode = DragMode::moveZone;

                dragStartX = e.x;
                dragStartY = e.y;
                dragStartNote = noteAtX (e.x);
                dragStartVelocity = velocityAtY (e.y, zoneArea);
                dragStartLoKey = z.lowNote;
                dragStartHiKey = z.highNote;
                dragStartRoot = z.rootNote;
                dragStartLoVel = z.lowVelocity;
                dragStartHiVel = z.highVelocity;

                repaint();
                return;
            }
        }

        // Clicked empty area - deselect
        if (! (e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown()))
            clearZoneSelection();
    }

    void SampleMapEditor::mouseMove (const juce::MouseEvent& e)
    {
        if (! gridBounds.contains (e.getPosition()))
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            return;
        }

        const auto& zones = owner.getProject().getSampleMap().getZones();
        const float noteWidth = 12.0f * zoomLevel;
        const int startNote = scrollOffset;
        const auto zoneArea = getZoneArea (gridBounds);
        constexpr float handleSize = 16.0f;

        for (int i = (int) zones.size() - 1; i >= 0; --i)
        {
            const auto zoneRect = zoneRectFor (zones[(size_t) i], zoneArea, startNote, noteWidth);
            if (! zoneRect.expanded (3.0f, 3.0f).contains ((float) e.x, (float) e.y))
                continue;

            const bool tinyVelocityHeight = zoneRect.getHeight() <= handleSize * 1.5f;
            const bool nearVelocityHigh = std::abs ((float) e.y - zoneRect.getY()) < handleSize
                                       || (tinyVelocityHeight && (float) e.y <= zoneRect.getCentreY());
            const bool nearVelocityLow = std::abs ((float) e.y - zoneRect.getBottom()) < handleSize
                                      || (tinyVelocityHeight && (float) e.y > zoneRect.getCentreY());

            if (nearVelocityHigh || nearVelocityLow)
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            else if (std::abs ((float) e.x - zoneRect.getX()) < handleSize
                || std::abs ((float) e.x - zoneRect.getRight()) < handleSize)
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            else
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }

        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void SampleMapEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragMode == DragMode::none || selectedZone < 0 || selectedZone >= (int) owner.getProject().getSampleMap().getZones().size()) return;

        auto& zones = owner.getProject().getSampleMap().getZones();
        auto& z = zones[(size_t) selectedZone];

        int currentNote = noteAtX (e.x);
        int delta = currentNote - dragStartNote;
        const auto zoneArea = getZoneArea (gridBounds);
        const int currentVelocity = velocityAtY (e.y, zoneArea);
        const int velocityDelta = currentVelocity - dragStartVelocity;

        switch (dragMode)
        {
            case DragMode::moveZone:
            {
                int newLo = juce::jlimit (0, 127, dragStartLoKey + delta);
                int newHi = juce::jlimit (0, 127, dragStartHiKey + delta);
                int newRoot = juce::jlimit (0, 127, dragStartRoot + delta);
                if (newHi >= newLo)
                {
                    z.lowNote = newLo;
                    z.highNote = newHi;
                    z.rootNote = newRoot;
                }
                break;
            }
            case DragMode::resizeLeft:
            {
                int newLo = juce::jlimit (0, z.highNote, dragStartLoKey + delta);
                z.lowNote = newLo;
                break;
            }
            case DragMode::resizeRight:
            {
                int newHi = juce::jlimit (z.lowNote, 127, dragStartHiKey + delta);
                z.highNote = newHi;
                break;
            }
            case DragMode::resizeVelocityLow:
            {
                z.lowVelocity = juce::jlimit (1, z.highVelocity, currentVelocity);
                break;
            }
            case DragMode::resizeVelocityHigh:
            {
                z.highVelocity = juce::jlimit (z.lowVelocity, 127, currentVelocity);
                break;
            }
            case DragMode::moveVelocity:
            {
                const int span = dragStartHiVel - dragStartLoVel;
                int newLow = juce::jlimit (1, 127 - span, dragStartLoVel + velocityDelta);
                z.lowVelocity = newLow;
                z.highVelocity = juce::jlimit (newLow, 127, newLow + span);
                break;
            }
            default:
                break;
        }

        updateSteppersFromZone();
        owner.getProject().notifyChanged();
        updateHealthButtons();
        repaint();
    }

    void SampleMapEditor::mouseUp (const juce::MouseEvent&)
    {
        dragMode = DragMode::none;
    }

    void SampleMapEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (! gridBounds.contains (e.getPosition())) return;

        float delta = wheel.deltaY * 12; // Scroll by octaves
        scrollOffset = juce::jlimit (-24, 96, scrollOffset - (int) delta);
        repaint();
    }

    int SampleMapEditor::noteAtX (int x)
    {
        const float noteWidth = 12.0f * zoomLevel;
        return scrollOffset + (int) ((x - gridBounds.getX()) / noteWidth);
    }

    int SampleMapEditor::xAtNote (int note)
    {
        const float noteWidth = 12.0f * zoomLevel;
        return gridBounds.getX() + (int) ((note - scrollOffset) * noteWidth);
    }

    juce::Rectangle<int> SampleMapEditor::getZoneArea (juce::Rectangle<int> grid) const
    {
        grid.removeFromTop (20);
        grid.removeFromBottom (30);
        return grid.reduced (0, 2);
    }

    int SampleMapEditor::velocityAtY (int y, juce::Rectangle<int> zoneArea) const
    {
        if (zoneArea.getHeight() <= 0)
            return 1;

        const float normalised = juce::jlimit (0.0f, 1.0f,
            (y - zoneArea.getY()) / (float) zoneArea.getHeight());
        return juce::jlimit (1, 127, juce::roundToInt (127.0f - normalised * 126.0f));
    }

    juce::Rectangle<float> SampleMapEditor::zoneRectFor (const SampleZoneDef& zone,
                                                         juce::Rectangle<int> zoneArea,
                                                         int startNote,
                                                         float noteWidth) const
    {
        const float x1 = zoneArea.getX() + (zone.lowNote - startNote) * noteWidth;
        const float x2 = zoneArea.getX() + (zone.highNote + 1 - startNote) * noteWidth;

        const auto lowVelocity = juce::jlimit (1, 127, zone.lowVelocity);
        const auto highVelocity = juce::jlimit (lowVelocity, 127, zone.highVelocity);
        const float yHigh = zoneArea.getY()
            + ((127.0f - (float) highVelocity) / 126.0f) * (float) zoneArea.getHeight();
        const float yLow = zoneArea.getY()
            + ((128.0f - (float) lowVelocity) / 127.0f) * (float) zoneArea.getHeight();

        return juce::Rectangle<float> (x1,
                                       juce::jlimit ((float) zoneArea.getY(), (float) zoneArea.getBottom(), yHigh),
                                       juce::jmax (4.0f, x2 - x1),
                                       juce::jmax (8.0f, yLow - yHigh));
    }

    juce::File SampleMapEditor::resolveSampleFile (const SampleZoneDef& zone) const
    {
        if (zone.samplePath.isEmpty())
            return {};
        if (juce::File::isAbsolutePath (zone.samplePath))
            return juce::File (zone.samplePath);

        auto projectFolder = owner.getProject().getProjectFolder();
        if (projectFolder.isDirectory())
        {
            auto direct = projectFolder.getChildFile (zone.samplePath);
            if (direct.existsAsFile())
                return direct;

            auto samples = projectFolder.getChildFile ("samples").getChildFile (zone.samplePath);
            if (samples.existsAsFile())
                return samples;

            auto assets = projectFolder.getChildFile ("assets").getChildFile (zone.samplePath);
            if (assets.existsAsFile())
                return assets;
        }

        return juce::File (zone.samplePath);
    }

    bool SampleMapEditor::loadWaveformForZone (const SampleZoneDef& zone, juce::String& status)
    {
        const auto file = resolveSampleFile (zone);
        if (! file.existsAsFile())
        {
            status = "Waveform unavailable: missing sample file.";
            selectedWaveformBuffer.setSize (0, 0);
            selectedWaveformRate = 44100.0;
            loadedWaveformPath.clear();
            return false;
        }

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader;
        try
        {
            reader.reset (formatManager.createReaderFor (file));
        }
        catch (...)
        {
            status = "Waveform unavailable: decoder failed.";
            return false;
        }

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        {
            status = "Waveform unavailable: unsupported or empty audio file.";
            return false;
        }

        const auto maxFrames = (juce::int64) (juce::jmax (1.0, reader->sampleRate) * 90.0);
        const int framesToRead = (int) juce::jmin<juce::int64> (reader->lengthInSamples, maxFrames);
        const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
        if (framesToRead <= 0)
        {
            status = "Waveform unavailable: empty audio file.";
            return false;
        }

        try
        {
            selectedWaveformBuffer.setSize (channels, framesToRead, false, true, true);
        }
        catch (...)
        {
            status = "Waveform unavailable: file is too large to preview safely.";
            return false;
        }

        if (! reader->read (&selectedWaveformBuffer, 0, framesToRead, 0, true, channels > 1))
        {
            status = "Waveform unavailable: could not read audio data.";
            selectedWaveformBuffer.setSize (0, 0);
            return false;
        }

        selectedWaveformRate = reader->sampleRate;
        loadedWaveformPath = file.getFullPathName();
        status = "Waveform loaded: " + file.getFileName();
        if (reader->lengthInSamples > framesToRead)
            status += " (first 90 seconds shown)";
        return true;
    }

    void SampleMapEditor::refreshWaveformFromSelectedZone()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (waveformViewer == nullptr || selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        const auto& zone = zones[(size_t) selectedZone];
        const auto file = resolveSampleFile (zone);
        const auto path = file.existsAsFile() ? file.getFullPathName() : juce::String();

        if (path.isEmpty())
        {
            waveformViewer->clearSampleData();
            waveformStatus.setText ("Waveform unavailable: missing sample file.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        if (path != loadedWaveformPath || selectedWaveformBuffer.getNumSamples() <= 0)
        {
            juce::String status;
            if (loadWaveformForZone (zone, status))
            {
                waveformViewer->setSampleData (selectedWaveformBuffer, selectedWaveformRate);
                waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            }
            else
            {
                waveformViewer->clearSampleData();
                waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            }
            waveformStatus.setText (status, juce::dontSendNotification);
        }

        waveformViewer->setZone (zone);
    }

    void SampleMapEditor::applyWaveformZoneToSelected()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (waveformViewer == nullptr || selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& zone = zones[(size_t) selectedZone];
        const auto edited = waveformViewer->getZone();
        zone.sampleStart = edited.sampleStart;
        zone.sampleEnd = edited.sampleEnd;
        zone.loopEnabled = edited.loopEnabled;
        zone.loopStart = edited.loopStart;
        zone.loopEnd = edited.loopEnd;
        zone.fadeInStart = edited.fadeInStart;
        zone.fadeInLength = edited.fadeInLength;
        zone.fadeOutStart = edited.fadeOutStart;
        zone.fadeOutLength = edited.fadeOutLength;

        sampleStartStepper.setValue (zone.sampleStart, false);
        sampleEndStepper.setValue (zone.sampleEnd, false);
        loopStartStepper.setValue (zone.loopStart, false);
        loopEndStepper.setValue (zone.loopEnd, false);
        loopToggle.setToggleState (zone.loopEnabled, juce::dontSendNotification);

        owner.getProject().notifyChanged();
        repaint();
    }

    void SampleMapEditor::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        auditionSampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
        auditionBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
        auditionChannels = device != nullptr
            ? juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits())
            : 2;

        const juce::SpinLock::ScopedLockType lock (auditionLock);
        if (auditionEngine != nullptr)
        {
            auditionEngine->prepare (auditionSampleRate, auditionBlockSize, auditionChannels);
            auditionEngine->setRenderContext (RenderContext::forBlock (auditionSampleRate,
                                                                       auditionBlockSize,
                                                                       auditionBlockSize,
                                                                       0,
                                                                       auditionChannels,
                                                                       120.0));
        }
    }

    void SampleMapEditor::audioDeviceStopped()
    {
        const juce::SpinLock::ScopedLockType lock (auditionLock);
        if (auditionEngine != nullptr)
            auditionEngine->reset();
    }

    void SampleMapEditor::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                            float* const* outputChannelData,
                                                            int numOutputChannels,
                                                            int numSamples,
                                                            const juce::AudioIODeviceCallbackContext&)
    {
        juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
        output.clear();

        const juce::SpinLock::ScopedTryLockType lock (auditionLock);
        if (lock.isLocked() && auditionEngine != nullptr)
        {
            auditionEngine->setRenderContext (RenderContext::forBlock (auditionSampleRate,
                                                                       numSamples,
                                                                       auditionBlockSize,
                                                                       0,
                                                                       numOutputChannels,
                                                                       120.0));
            auditionEngine->process (output, 0, numSamples);
        }
    }

    void SampleMapEditor::setMidiZoneSelectEnabled (bool enabled)
    {
        if (midiLearnMode == enabled)
            return;

        midiLearnMode = enabled;
        selectByMidiBtn.setToggleState (enabled, juce::dontSendNotification);
        auto& deviceManager = owner.getAudio().getDeviceManager();

        if (enabled)
        {
            for (const auto& input : juce::MidiInput::getAvailableDevices())
                deviceManager.setMidiInputDeviceEnabled (input.identifier, true);
            deviceManager.addMidiInputDeviceCallback ({}, this);
            waveformStatus.setText ("MIDI zone select on: play a key to select its mapped zone.",
                                    juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        }
        else
        {
            deviceManager.removeMidiInputDeviceCallback ({}, this);
            waveformStatus.setText ("MIDI zone select off.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        }
    }

    void SampleMapEditor::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        if (! midiLearnMode || ! message.isNoteOn())
            return;

        const int note = message.getNoteNumber();
        const int velocity = juce::jlimit (1, 127, (int) (message.getFloatVelocity() * 127.0f));
        juce::Component::SafePointer<SampleMapEditor> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, note, velocity]
        {
            if (safeThis != nullptr)
                safeThis->selectZoneForMidi (note, velocity);
        });
    }

    void SampleMapEditor::selectZoneForMidi (int note, int velocity)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        int bestIndex = -1;
        int bestDistance = 9999;

        for (int i = 0; i < (int) zones.size(); ++i)
        {
            const auto& zone = zones[(size_t) i];
            const bool noteMatches = note >= zone.lowNote && note <= zone.highNote;
            const bool velocityMatches = velocity >= zone.lowVelocity && velocity <= zone.highVelocity;
            if (noteMatches && velocityMatches)
            {
                bestIndex = i;
                break;
            }

            const int noteDistance = note < zone.lowNote ? zone.lowNote - note
                                  : note > zone.highNote ? note - zone.highNote
                                  : 0;
            const int velocityDistance = velocity < zone.lowVelocity ? zone.lowVelocity - velocity
                                      : velocity > zone.highVelocity ? velocity - zone.highVelocity
                                      : 0;
            const int distance = noteDistance * 4 + velocityDistance + std::abs (note - zone.rootNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        if (bestIndex >= 0)
        {
            selectZone (bestIndex);
            const auto& zone = zones[(size_t) bestIndex];
            waveformStatus.setText ("Selected zone from MIDI: " + noteToString (note)
                                    + " / V" + juce::String (velocity)
                                    + " -> " + juce::File (zone.samplePath).getFileName(),
                                    juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
            repaint();
        }
    }

    void SampleMapEditor::applyImportDefaultVelocity (bool allZones)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (zones.empty())
            return;

        const int lowVelocity = juce::jmin (importLowVelocityStepper.value, importHighVelocityStepper.value);
        const int highVelocity = juce::jmax (importLowVelocityStepper.value, importHighVelocityStepper.value);
        int changed = 0;

        if (allZones)
        {
            for (auto& zone : zones)
            {
                zone.lowVelocity = lowVelocity;
                zone.highVelocity = highVelocity;
                ++changed;
            }
        }
        else
        {
            auto targets = selectedZoneIndexes;
            if (targets.isEmpty() && selectedZone >= 0)
                targets.add (selectedZone);

            for (const int index : targets)
            {
                if (index < 0 || index >= (int) zones.size())
                    continue;

                zones[(size_t) index].lowVelocity = lowVelocity;
                zones[(size_t) index].highVelocity = highVelocity;
                ++changed;
            }
        }

        if (changed == 0)
            return;

        owner.getProject().notifyChanged();
        refresh();
        waveformStatus.setText ((allZones ? "Applied default velocity to all zones: "
                                          : "Applied default velocity to selected zones: ")
                                + juce::String (lowVelocity) + "-" + juce::String (highVelocity),
                                juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::addSample()
    {
        auto* chooser = new juce::FileChooser (
            "Import sample", juce::File(), "*.wav;*.aiff;*.aif;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                if (! files.isEmpty())
                    importSampleFiles (files);
                delete chooser;
            });
    }

    bool SampleMapEditor::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& path : files)
        {
            const juce::File f (path);
            if (f.isDirectory()) return true;
            const auto ext = f.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac")
                return true;
        }
        return false;
    }

    void SampleMapEditor::filesDropped (const juce::StringArray& files, int, int)
    {
        // Walk dropped paths: a file goes straight in, a folder recurses one
        // level deep collecting any audio files. This makes "drop a kit
        // folder" do the right thing.
        juce::Array<juce::File> collected;
        const juce::String wildcard ("*.wav;*.aiff;*.aif;*.flac");
        for (const auto& path : files)
        {
            const juce::File f (path);
            if (f.isDirectory())
            {
                juce::Array<juce::File> inFolder;
                f.findChildFiles (inFolder, juce::File::findFiles, true, wildcard);
                for (const auto& child : inFolder)
                    collected.add (child);
            }
            else if (f.existsAsFile())
            {
                const auto ext = f.getFileExtension().toLowerCase();
                if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac")
                    collected.add (f);
            }
        }
        if (! collected.isEmpty())
            importSampleFiles (collected);
    }

    void SampleMapEditor::importSampleFiles (const juce::Array<juce::File>& files)
    {
        if (files.isEmpty())
            return;

        {
            int baseNote = 24; // C0 fallback when no pitch metadata is present.
                    int zoneSize = 1;  // Chromatic fallback keeps imported samples predictable.
                    bool anyParsedRoot = false;
                    int imported = 0;
                    int parsedRoots = 0;
                    int audioDetectedRoots = 0;
                    int parsedVelocity = 0;
                    int defaultVelocityApplied = 0;
                    int parsedRoundRobin = 0;
                    const int importLowVelocity = juce::jmin (importLowVelocityStepper.value,
                                                              importHighVelocityStepper.value);
                    const int importHighVelocity = juce::jmax (importLowVelocityStepper.value,
                                                               importHighVelocityStepper.value);
                    std::vector<SampleZoneDef> importedZones;
                    importedZones.reserve ((size_t) files.size());

                    for (auto& f : files)
                    {
                        if (! f.existsAsFile()) continue;
                        const int fallbackRoot = juce::jlimit (0, 127, baseNote);
                        bool usedNamePitch = false;
                        bool usedAudioPitch = false;
                        bool usedVelocityRange = false;
                        SampleZoneDef z = SampleMap::inferZoneFromFileWithAudio (f,
                                                                                 fallbackRoot,
                                                                                 baseNote,
                                                                                 juce::jmin (127, baseNote + zoneSize - 1),
                                                                                 &usedNamePitch,
                                                                                 &usedAudioPitch,
                                                                                 &usedVelocityRange);
                        anyParsedRoot = anyParsedRoot || usedNamePitch || usedAudioPitch;
                        if (usedNamePitch)
                            ++parsedRoots;
                        if (usedAudioPitch)
                            ++audioDetectedRoots;
                        if (usedVelocityRange)
                            ++parsedVelocity;
                        else
                        {
                            z.lowVelocity = importLowVelocity;
                            z.highVelocity = importHighVelocity;
                            ++defaultVelocityApplied;
                        }
                        if (z.roundRobinGroup > 0 || z.roundRobinIndex > 0)
                            ++parsedRoundRobin;
                        importedZones.push_back (z);
                        ++imported;
                        baseNote += zoneSize; // Move to next range
                        if (baseNote > 108) baseNote = 24; // Wrap back to C0
                    }

                    std::map<int, int> initialRootCounts;
                    for (const auto& zone : importedZones)
                        ++initialRootCounts[zone.rootNote];

                    bool audioCollapsedToFallback = false;
                    if (parsedRoots == 0
                        && audioDetectedRoots > 0
                        && importedZones.size() > 1
                        && initialRootCounts.size() <= 1)
                    {
                        int fallbackNote = 24;
                        for (auto& zone : importedZones)
                        {
                            zone.rootNote = fallbackNote;
                            zone.lowNote = fallbackNote;
                            zone.highNote = fallbackNote;
                            if (++fallbackNote > 108)
                                fallbackNote = 24;
                        }
                        anyParsedRoot = false;
                        audioCollapsedToFallback = true;
                    }

                    std::map<int, int> rootCounts;
                    for (const auto& zone : importedZones)
                        ++rootCounts[zone.rootNote];

                    std::map<int, int> rootRoundRobinIndex;
                    int autoRoundRobin = 0;
                    for (auto& zone : importedZones)
                    {
                        const bool stackedRoot = rootCounts[zone.rootNote] > 1;
                        const bool noVelocityLayer = zone.lowVelocity == 1 && zone.highVelocity == 127;
                        const bool noRoundRobin = zone.roundRobinGroup == 0 && zone.roundRobinIndex == 0;
                        if (stackedRoot && noVelocityLayer && noRoundRobin)
                        {
                            zone.roundRobinGroup = 1;
                            zone.roundRobinIndex = ++rootRoundRobinIndex[zone.rootNote];
                            ++autoRoundRobin;
                        }
                        owner.getProject().getSampleMap().add (zone);
                    }

                    if (anyParsedRoot)
                        owner.getProject().getSampleMap().autoMapByRootNotes();
                    if (owner.getProject().getEngineType() != "sample")
                        owner.getProject().setEngineType ("sample");
                    else
                        owner.getProject().notifyChanged();
                    refresh();

                    juce::String status = "Imported " + juce::String (imported) + " sample"
                        + (imported == 1 ? "." : "s.")
                        + " Parsed roots: " + juce::String (parsedRoots)
                        + ", audio-detected roots: " + juce::String (audioDetectedRoots)
                        + ", velocity layers: " + juce::String (parsedVelocity)
                        + ", default velocity: " + juce::String (defaultVelocityApplied)
                        + ", RR tags: " + juce::String (parsedRoundRobin) + ".";
                    if (autoRoundRobin > 0)
                        status += " Auto-marked " + juce::String (autoRoundRobin) + " same-root zones as round robin.";
                    if (audioCollapsedToFallback)
                        status += " Audio pitch collapsed to one note, so chromatic C0 fallback was used.";
                    if (! anyParsedRoot && ! audioCollapsedToFallback)
                        status += " No filename/audio roots found, so chromatic C0 fallback mapping was assigned.";

                    waveformStatus.setText (status, juce::dontSendNotification);
                    waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                    if (imported > 0)
                    {
                        const int firstNew = juce::jmax (0, (int) owner.getProject().getSampleMap().getZones().size() - imported);
                        selectZone (firstNew);
                    }
        }
    }

    void SampleMapEditor::removeSample()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZoneIndexes.isEmpty())
            return;

        auto toRemove = selectedZoneIndexes;
        toRemove.sort();
        for (int i = toRemove.size() - 1; i >= 0; --i)
        {
            const int index = toRemove[i];
            if (index >= 0 && index < (int) zones.size())
                owner.getProject().getSampleMap().removeAt (index);
        }

        selectedZone = -1;
        selectedZoneIndexes.clear();
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::clearAllSamples()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (zones.empty())
            return;

        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("Clear Sample Map")
                .withMessage ("Remove all imported sample zones from this map? This does not delete audio files from disk.")
                .withButton ("Clear All")
                .withButton ("Cancel")
                .withIconType (juce::MessageBoxIconType::WarningIcon),
            [safeThis = juce::Component::SafePointer<SampleMapEditor> (this)] (int result)
            {
                if (result != 1 || safeThis == nullptr)
                    return;

                safeThis->owner.getProject().getSampleMap().clear();
                safeThis->selectedZone = -1;
                safeThis->selectedZoneIndexes.clear();
                safeThis->owner.getProject().notifyChanged();
                safeThis->refresh();
            });
    }

    void SampleMapEditor::autoMap()
    {
        owner.getProject().getSampleMap().autoMapByRootNotes();
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::autoMapDrumPads()
    {
        autoMapDrumPadsAt (36, 16);
    }

    void SampleMapEditor::autoMapDrumPadsAt (int startNote, int padCount)
    {
        auto& map = owner.getProject().getSampleMap();
        if (map.getZones().empty())
        {
            waveformStatus.setText ("Pad Map needs imported samples first.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        map.autoMapDrumPads (startNote, padCount);
        if (owner.getProject().getEngineType() != "sample")
            owner.getProject().setEngineType ("sample");
        else
            owner.getProject().notifyChanged();
        refresh();
        const auto rootName = juce::MidiMessage::getMidiNoteName (startNote, true, true, 4);
        waveformStatus.setText ("Mapped imported samples to " + juce::String (padCount)
                                + " one-shot drum pads starting at " + rootName
                                + ". Hats auto-share choke group 1.",
                                juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::normalizeSelectedZones()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        juce::Array<int> targets = selectedZoneIndexes;
        if (targets.isEmpty() && selectedZone >= 0)
            targets.add (selectedZone);

        int normalized = 0;
        for (const auto idx : targets)
        {
            if (idx < 0 || idx >= (int) zones.size()) continue;
            auto& zone = zones[(size_t) idx];

            // Load the zone's audio if it isn't already cached.
            juce::AudioBuffer<float> buffer;
            double rate = 44100.0;
            const auto file = resolveSampleFile (zone);
            if (! file.existsAsFile()) continue;

            juce::AudioFormatManager fmtMgr;
            fmtMgr.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples == 0) continue;

            buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
            rate = reader->sampleRate;
            juce::ignoreUnused (rate);

            const int playStart = juce::jmax (0, zone.sampleStart);
            const int playEnd   = zone.sampleEnd > playStart
                ? juce::jmin (zone.sampleEnd, buffer.getNumSamples())
                : buffer.getNumSamples();
            float peak = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* data = buffer.getReadPointer (ch);
                for (int i = playStart; i < playEnd; ++i)
                    peak = juce::jmax (peak, std::abs (data[i]));
            }
            if (peak > 1.0e-6f)
            {
                // Set gainDb so peak * 10^(gainDb/20) == 1.0.
                const float linear = 1.0f / peak;
                zone.gainDb = juce::jlimit (-24.0f, 24.0f,
                    20.0f * std::log10 (juce::jmax (1.0e-6f, linear)));
                ++normalized;
            }
        }

        if (normalized == 0)
        {
            waveformStatus.setText ("Normalize: no playable zones in selection.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        owner.getProject().notifyChanged();
        refresh();
        waveformStatus.setText ("Normalized " + juce::String (normalized) + " zone(s) to 0 dB peak.",
                                juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::applyMapPreset (int presetId)
    {
        switch (presetId)
        {
            case 101: autoMapDrumPadsAt (36, 16); break;          // Pad Map 16 from C1
            case 102: autoMapDrumPadsAt (48, 16); break;          // Pad Map 16 from C2
            case 103: autoMapDrumPadsAt (36, 8);  break;          // Pad Map 8 from C1
            case 201: makeSelectedZoneGlitchKit(); break;         // Transient slice 16
            case 202: chopSelectedZoneAtTransients (8);  break;   // Transient slice 8
            case 211: chopSelectedZoneIntoSlices (16); break;     // Equal grid 16
            case 212: chopSelectedZoneIntoSlices (8);  break;     // Equal grid 8
            case 213: chopSelectedZoneIntoSlices (32); break;     // Equal grid 32
            case 301: normalizeSelectedZones(); break;
            case 401:                                            // Build Drum Kit
            {
                autoMapDrumPadsAt (36, 16);
                break;
            }
            case 402:                                            // Build Multi-Sample Patch
            {
                auto& map = owner.getProject().getSampleMap();
                if (map.getZones().empty())
                {
                    waveformStatus.setText ("Multi-sample patch needs imported samples first.",
                                             juce::dontSendNotification);
                    waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
                    break;
                }
                map.autoMapByRootNotes();
                if (owner.getProject().getEngineType() != "sample")
                    owner.getProject().setEngineType ("sample");
                else
                    owner.getProject().notifyChanged();
                refresh();
                waveformStatus.setText ("Built multi-sample patch — zones spread across the keyboard by root note.",
                                         juce::dontSendNotification);
                waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                break;
            }
            default: break;
        }
    }

    void SampleMapEditor::makeSelectedZoneGlitchKit()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
        {
            waveformStatus.setText ("Glitch Kit needs one selected sample zone.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        auto base = zones[(size_t) selectedZone];
        if (selectedWaveformBuffer.getNumSamples() <= 0 || resolveSampleFile (base).getFullPathName() != loadedWaveformPath)
        {
            juce::String status;
            if (! loadWaveformForZone (base, status))
            {
                waveformStatus.setText (status, juce::dontSendNotification);
                waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
                return;
            }
        }

        const int sampleLength = selectedWaveformBuffer.getNumSamples();
        const int sourceStart = juce::jlimit (0, juce::jmax (0, sampleLength - 1), base.sampleStart);
        const int sourceEnd = base.sampleEnd > sourceStart
            ? juce::jlimit (sourceStart + 1, sampleLength, base.sampleEnd)
            : sampleLength;
        if (sourceEnd <= sourceStart + 16)
        {
            waveformStatus.setText ("Glitch Kit needs a longer sample to slice.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        auto slicePoints = detectTransientSlicePoints (base, 16);
        if (slicePoints.size() < 3)
        {
            slicePoints.clear();
            const int equalSlices = 16;
            for (int i = 0; i <= equalSlices; ++i)
                slicePoints.push_back (sourceStart + ((sourceEnd - sourceStart) * i) / equalSlices);
        }

        const int sliceCount = juce::jlimit (1, 16, (int) slicePoints.size() - 1);
        std::vector<SampleZoneDef> slices;
        slices.reserve ((size_t) sliceCount);
        const int fadeSamples = juce::jmax (1, juce::roundToInt (selectedWaveformRate * 0.004));
        for (int i = 0; i < sliceCount; ++i)
        {
            const int sliceStart = slicePoints[(size_t) i];
            const int sliceEnd = slicePoints[(size_t) i + 1];
            if (sliceEnd <= sliceStart)
                continue;

            auto slice = base;
            const int note = 36 + i;
            slice.sampleStart = sliceStart;
            slice.sampleEnd = sliceEnd;
            slice.loopEnabled = false;
            slice.loopStart = 0;
            slice.loopEnd = 0;
            slice.fadeInStart = slice.sampleStart;
            slice.fadeInLength = juce::jmin (slice.sampleEnd - slice.sampleStart, fadeSamples);
            slice.fadeOutLength = juce::jmin (slice.sampleEnd - slice.sampleStart, fadeSamples);
            slice.fadeOutStart = slice.sampleEnd - slice.fadeOutLength;
            slice.lowNote = note;
            slice.highNote = note;
            slice.rootNote = note;
            slice.lowVelocity = 1;
            slice.highVelocity = 127;
            slice.roundRobinGroup = 0;
            slice.roundRobinIndex = 0;
            slice.group = "Glitch Kit";
            slice.padIndex = i;
            slice.padLabel = "Slice " + juce::String (i + 1);
            slice.chokeGroup = 0;
            slice.oneShot = true;
            slice.triggerProbability = 100;
            slices.push_back (slice);
        }

        if (slices.empty())
            return;

        zones.erase (zones.begin() + selectedZone);
        zones.insert (zones.begin() + selectedZone, slices.begin(), slices.end());
        if (owner.getProject().getEngineType() != "sample")
            owner.getProject().setEngineType ("sample");
        else
            owner.getProject().notifyChanged();
        juce::Array<int> firstSlice;
        firstSlice.add (selectedZone);
        setSelectedZones (firstSlice, true);
        refresh();
        waveformStatus.setText ("Created a " + juce::String ((int) slices.size())
                                + "-pad Glitch Kit from the selected sample. Double-click pads to audition.",
                                juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::showEditMenu()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        const bool hasSelection = selectedZone >= 0 && selectedZone < (int) zones.size();
        const bool canMergeNext = hasSelection
            && selectedZone + 1 < (int) zones.size()
            && zones[(size_t) selectedZone].samplePath == zones[(size_t) selectedZone + 1].samplePath;

        juce::PopupMenu menu;
        menu.addItem (1, "Auto-map by root notes");
        menu.addItem (2, "Spread evenly across keyboard");
        menu.addItem (17, "Map all samples to drum pads");
        menu.addSeparator();
        menu.addItem (5, "Duplicate selected zone", hasSelection);
        menu.addItem (6, "Split selected at root/midpoint", hasSelection);
        menu.addItem (7, "Split selected velocity range", hasSelection);
        menu.addItem (18, "Create Glitch Kit from selected sample", hasSelection);
        menu.addItem (11, "Chop selected sample into 4 key slices", hasSelection);
        menu.addItem (12, "Chop selected sample into 8 key slices", hasSelection);
        menu.addItem (13, "Transient chop selected sample (up to 8)", hasSelection);
        menu.addItem (14, "Transient chop selected sample (up to 16)", hasSelection);
        menu.addItem (8, "Merge selected with next matching sample", canMergeNext);
        menu.addSeparator();
        menu.addItem (9, "Set selected bounds to full sample", hasSelection);
        menu.addItem (10, "Reset selected playback edits", hasSelection);
        menu.addSeparator();
        menu.addItem (3, "Select first zone", ! zones.empty());
        menu.addItem (15, "Select all zones", ! zones.empty());
        menu.addItem (16, "Delete selected zones", ! selectedZoneIndexes.isEmpty());
        menu.addItem (4, "Clear selection", ! selectedZoneIndexes.isEmpty());

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editModeBtn),
            [this] (int result)
            {
                if (result == 1)
                {
                    owner.getProject().getSampleMap().autoMapByRootNotes();
                    owner.getProject().notifyChanged();
                    refresh();
                }
                else if (result == 2)
                {
                    owner.getProject().getSampleMap().autoMapAcrossKeyboard();
                    owner.getProject().notifyChanged();
                    refresh();
                }
                else if (result == 3)
                {
                    selectZone (0);
                }
                else if (result == 4)
                {
                    clearZoneSelection();
                }
                else if (result == 15)
                {
                    selectAllZones();
                }
                else if (result == 16)
                {
                    removeSample();
                }
                else if (result == 5)
                {
                    duplicateSelectedZone();
                }
                else if (result == 17)
                {
                    autoMapDrumPads();
                }
                else if (result == 18)
                {
                    makeSelectedZoneGlitchKit();
                }
                else if (result == 6)
                {
                    splitSelectedZoneAtRoot();
                }
                else if (result == 7)
                {
                    splitSelectedZoneVelocity();
                }
                else if (result == 8)
                {
                    mergeSelectedZoneWithNext();
                }
                else if (result == 11)
                {
                    chopSelectedZoneIntoSlices (4);
                }
                else if (result == 12)
                {
                    chopSelectedZoneIntoSlices (8);
                }
                else if (result == 13)
                {
                    chopSelectedZoneAtTransients (8);
                }
                else if (result == 14)
                {
                    chopSelectedZoneAtTransients (16);
                }
                else if (result == 9)
                {
                    setSelectedZoneBoundsToFullSample();
                }
                else if (result == 10)
                {
                    resetSelectedZonePlaybackEdits();
                }
            });
    }

    void SampleMapEditor::toggleReverseForSelected()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
        {
            reverseBtn.setToggleState (false, juce::dontSendNotification);
            return;
        }

        zones[(size_t) selectedZone].reverse = reverseBtn.getToggleState();
        owner.getProject().notifyChanged();
        repaint();
    }

    void SampleMapEditor::auditionSelectedZone()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
        {
            waveformStatus.setText ("Audition unavailable: select a sample zone first.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        auto zone = zones[(size_t) selectedZone];
        const auto file = resolveSampleFile (zone);
        if (! file.existsAsFile())
        {
            waveformStatus.setText ("Audition unavailable: missing sample file.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            waveformStatus.setText ("Audition unavailable: " + error, juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        stopAudition();

        SampleMap auditionMap;
        zone.lowNote = 0;
        zone.highNote = 127;
        zone.lowVelocity = 1;
        zone.highVelocity = 127;
        auditionMap.add (zone);

        auto engine = std::make_unique<SampleSynthEngine>();
        engine->prepare (auditionSampleRate, auditionBlockSize, auditionChannels);
        engine->setRenderContext (RenderContext::forBlock (auditionSampleRate,
                                                           auditionBlockSize,
                                                           auditionBlockSize,
                                                           0,
                                                           auditionChannels,
                                                           120.0));
        engine->loadFromPack (owner.getProject().getProjectFolder(), auditionMap);

        {
            const juce::SpinLock::ScopedLockType lock (auditionLock);
            auditionEngine = std::move (engine);
        }

        owner.getAudio().getDeviceManager().addAudioCallback (this);
        auditionCallbackActive = true;
        auditionNote = zone.rootNote;
        {
            const juce::SpinLock::ScopedLockType lock (auditionLock);
            if (auditionEngine != nullptr)
                auditionEngine->noteOn (auditionNote, 0.9f);
        }

        waveformStatus.setText ("Auditioning " + file.getFileName() + " at " + noteToString (auditionNote),
                                juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::stopAudition()
    {
        if (auditionCallbackActive)
        {
            owner.getAudio().getDeviceManager().removeAudioCallback (this);
            auditionCallbackActive = false;
        }

        {
            const juce::SpinLock::ScopedLockType lock (auditionLock);
            if (auditionEngine != nullptr)
            {
                auditionEngine->allNotesOff();
                auditionEngine->reset();
            }
            auditionEngine.reset();
        }

        auditionNote = -1;
    }

    void SampleMapEditor::toggleLoopForSelected()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
        {
            loopToggle.setToggleState (false, juce::dontSendNotification);
            return;
        }

        auto& zone = zones[(size_t) selectedZone];
        zone.loopEnabled = loopToggle.getToggleState();
        const int length = waveformViewer != nullptr ? waveformViewer->getSampleLength() : 0;
        const int playEnd = zone.sampleEnd > zone.sampleStart ? zone.sampleEnd
                         : length > 0 ? length
                         : zone.loopEnd;
        if (zone.loopEnabled && zone.loopEnd <= zone.loopStart + 1 && playEnd > zone.sampleStart + 1)
        {
            zone.loopStart = zone.sampleStart;
            zone.loopEnd = playEnd;
        }

        updateSteppersFromZone();
        owner.getProject().notifyChanged();
    }

    void SampleMapEditor::addDefaultFade (bool fadeIn)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& zone = zones[(size_t) selectedZone];
        const int length = waveformViewer != nullptr ? waveformViewer->getSampleLength() : 0;
        const int playEnd = zone.sampleEnd > zone.sampleStart ? zone.sampleEnd : length;
        if (playEnd <= zone.sampleStart)
            return;

        const int fadeLength = juce::jlimit (1, playEnd - zone.sampleStart,
                                             (int) (selectedWaveformRate * 0.02));
        if (fadeIn)
        {
            zone.fadeInStart = zone.sampleStart;
            zone.fadeInLength = fadeLength;
        }
        else
        {
            zone.fadeOutStart = juce::jmax (zone.sampleStart, playEnd - fadeLength);
            zone.fadeOutLength = fadeLength;
        }

        if (waveformViewer != nullptr)
            waveformViewer->setZone (zone);
        owner.getProject().notifyChanged();
        repaint();
    }

    void SampleMapEditor::duplicateSelectedZone()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto copy = zones[(size_t) selectedZone];
        if (copy.roundRobinGroup > 0)
            ++copy.roundRobinIndex;

        zones.insert (zones.begin() + selectedZone + 1, copy);
        ++selectedZone;
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::splitSelectedZoneAtRoot()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& original = zones[(size_t) selectedZone];
        if (original.highNote <= original.lowNote)
            return;

        int split = original.rootNote;
        if (split <= original.lowNote || split >= original.highNote)
            split = (original.lowNote + original.highNote) / 2;

        auto upper = original;
        original.highNote = split;
        original.rootNote = juce::jlimit (original.lowNote, original.highNote, original.rootNote);
        upper.lowNote = split + 1;
        upper.rootNote = juce::jlimit (upper.lowNote, upper.highNote, upper.rootNote);

        zones.insert (zones.begin() + selectedZone + 1, upper);
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::splitSelectedZoneVelocity()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& original = zones[(size_t) selectedZone];
        if (original.highVelocity <= original.lowVelocity)
            return;

        const int split = (original.lowVelocity + original.highVelocity) / 2;
        auto upper = original;
        original.highVelocity = split;
        upper.lowVelocity = split + 1;

        zones.insert (zones.begin() + selectedZone + 1, upper);
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::chopSelectedZoneIntoSlices (int sliceCount)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        sliceCount = juce::jlimit (2, 32, sliceCount);
        auto base = zones[(size_t) selectedZone];

        if (selectedWaveformBuffer.getNumSamples() <= 0 || resolveSampleFile (base).getFullPathName() != loadedWaveformPath)
        {
            juce::String status;
            loadWaveformForZone (base, status);
        }

        const int sampleLength = selectedWaveformBuffer.getNumSamples();
        if (sampleLength <= sliceCount)
        {
            waveformStatus.setText ("Chop unavailable: load a valid sample waveform first.", juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        const int sourceStart = juce::jlimit (0, sampleLength - 1, base.sampleStart);
        const int sourceEnd = base.sampleEnd > sourceStart
            ? juce::jlimit (sourceStart + 1, sampleLength, base.sampleEnd)
            : sampleLength;
        const int sourceLength = sourceEnd - sourceStart;
        if (sourceLength <= sliceCount)
            return;

        std::vector<SampleZoneDef> slices;
        slices.reserve ((size_t) sliceCount);
        const int startKey = juce::jlimit (0, 127, base.rootNote);
        for (int i = 0; i < sliceCount; ++i)
        {
            auto slice = base;
            const int sliceStart = sourceStart + (sourceLength * i) / sliceCount;
            const int sliceEnd = i == sliceCount - 1
                ? sourceEnd
                : sourceStart + (sourceLength * (i + 1)) / sliceCount;
            const int note = juce::jlimit (0, 127, startKey + i);

            slice.sampleStart = sliceStart;
            slice.sampleEnd = juce::jmax (sliceStart + 1, sliceEnd);
            slice.loopEnabled = false;
            slice.loopStart = 0;
            slice.loopEnd = 0;
            slice.fadeInStart = slice.sampleStart;
            slice.fadeInLength = juce::jmin (slice.sampleEnd - slice.sampleStart, juce::roundToInt (selectedWaveformRate * 0.005));
            slice.fadeOutLength = juce::jmin (slice.sampleEnd - slice.sampleStart, juce::roundToInt (selectedWaveformRate * 0.005));
            slice.fadeOutStart = slice.sampleEnd - slice.fadeOutLength;
            slice.lowNote = note;
            slice.highNote = note;
            slice.rootNote = note;
            slice.lowVelocity = base.lowVelocity;
            slice.highVelocity = base.highVelocity;
            slice.group = base.group.isNotEmpty() ? base.group : "Chops";
            slice.roundRobinGroup = 0;
            slice.roundRobinIndex = i + 1;
            slices.push_back (slice);
        }

        zones.erase (zones.begin() + selectedZone);
        zones.insert (zones.begin() + selectedZone, slices.begin(), slices.end());
        owner.getProject().notifyChanged();
        refresh();
        waveformStatus.setText ("Created " + juce::String (sliceCount) + " key slices starting at "
                                + noteToString (startKey) + ".", juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    std::vector<int> SampleMapEditor::detectTransientSlicePoints (const SampleZoneDef& zone, int maxSlices)
    {
        if (selectedWaveformBuffer.getNumSamples() <= 0 || resolveSampleFile (zone).getFullPathName() != loadedWaveformPath)
        {
            juce::String status;
            loadWaveformForZone (zone, status);
        }

        const int sampleLength = selectedWaveformBuffer.getNumSamples();
        if (sampleLength <= 0)
            return {};

        const int sourceStart = juce::jlimit (0, sampleLength - 1, zone.sampleStart);
        const int sourceEnd = zone.sampleEnd > sourceStart
            ? juce::jlimit (sourceStart + 1, sampleLength, zone.sampleEnd)
            : sampleLength;
        const int sourceLength = sourceEnd - sourceStart;
        if (sourceLength < 1024)
            return { sourceStart, sourceEnd };

        maxSlices = juce::jlimit (2, 32, maxSlices);
        const int hop = juce::jlimit (64, 1024, (int) (selectedWaveformRate * 0.005));
        const int minSpacing = juce::jmax (hop * 4, sourceLength / juce::jmax (4, maxSlices * 3));

        std::vector<float> envelope;
        std::vector<int> samplePositions;
        for (int pos = sourceStart; pos < sourceEnd; pos += hop)
        {
            const int blockEnd = juce::jmin (sourceEnd, pos + hop);
            double sum = 0.0;
            int count = 0;
            for (int ch = 0; ch < selectedWaveformBuffer.getNumChannels(); ++ch)
            {
                const auto* data = selectedWaveformBuffer.getReadPointer (ch);
                for (int i = pos; i < blockEnd; ++i)
                {
                    sum += std::abs ((double) data[i]);
                    ++count;
                }
            }
            envelope.push_back (count > 0 ? (float) (sum / (double) count) : 0.0f);
            samplePositions.push_back (pos);
        }

        if (envelope.size() < 4)
            return { sourceStart, sourceEnd };

        double sumEnvelope = 0.0;
        for (float value : envelope)
            sumEnvelope += value;
        const float average = (float) (sumEnvelope / (double) envelope.size());
        const float threshold = juce::jmax (0.015f, average * 1.75f);

        struct Candidate
        {
            int sample = 0;
            float score = 0.0f;
        };
        std::vector<Candidate> candidates;
        for (int i = 2; i < (int) envelope.size() - 2; ++i)
        {
            const float previous = juce::jmax (0.0001f, (envelope[(size_t) i - 1] + envelope[(size_t) i - 2]) * 0.5f);
            const float current = envelope[(size_t) i];
            const bool localPeak = current >= envelope[(size_t) i - 1] && current >= envelope[(size_t) i + 1];
            if (localPeak && current >= threshold && current > previous * 1.35f)
                candidates.push_back ({ samplePositions[(size_t) i], current / previous });
        }

        std::sort (candidates.begin(), candidates.end(),
                   [] (const Candidate& a, const Candidate& b) { return a.score > b.score; });

        std::vector<int> boundaries { sourceStart };
        for (const auto& candidate : candidates)
        {
            if ((int) boundaries.size() >= maxSlices)
                break;
            bool tooClose = false;
            for (int existing : boundaries)
                if (std::abs (candidate.sample - existing) < minSpacing)
                {
                    tooClose = true;
                    break;
                }
            if (! tooClose && std::abs (sourceEnd - candidate.sample) >= minSpacing)
                boundaries.push_back (candidate.sample);
        }

        boundaries.push_back (sourceEnd);
        std::sort (boundaries.begin(), boundaries.end());
        boundaries.erase (std::unique (boundaries.begin(), boundaries.end()), boundaries.end());
        return boundaries;
    }

    void SampleMapEditor::chopSelectedZoneAtSlicePoints (const std::vector<int>& slicePoints, const juce::String& label)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size() || slicePoints.size() < 2)
            return;

        auto base = zones[(size_t) selectedZone];
        const int maxPlayableSlices = juce::jmax (1, 128 - juce::jlimit (0, 127, base.rootNote));
        const int sliceCount = juce::jmin ((int) slicePoints.size() - 1, maxPlayableSlices);
        if (sliceCount <= 0)
            return;

        std::vector<SampleZoneDef> slices;
        slices.reserve ((size_t) sliceCount);
        const int startKey = juce::jlimit (0, 127, base.rootNote);
        const int fadeSamples = juce::jmax (1, juce::roundToInt (selectedWaveformRate * 0.004));
        for (int i = 0; i < sliceCount; ++i)
        {
            const int sliceStart = slicePoints[(size_t) i];
            const int sliceEnd = slicePoints[(size_t) i + 1];
            if (sliceEnd <= sliceStart)
                continue;

            auto slice = base;
            const int note = juce::jlimit (0, 127, startKey + i);
            slice.sampleStart = sliceStart;
            slice.sampleEnd = sliceEnd;
            slice.loopEnabled = false;
            slice.loopStart = 0;
            slice.loopEnd = 0;
            slice.fadeInStart = slice.sampleStart;
            slice.fadeInLength = juce::jmin (slice.sampleEnd - slice.sampleStart, fadeSamples);
            slice.fadeOutLength = juce::jmin (slice.sampleEnd - slice.sampleStart, fadeSamples);
            slice.fadeOutStart = slice.sampleEnd - slice.fadeOutLength;
            slice.lowNote = note;
            slice.highNote = note;
            slice.rootNote = note;
            slice.roundRobinGroup = 0;
            slice.roundRobinIndex = i + 1;
            slice.group = label;
            slices.push_back (slice);
        }

        if (slices.empty())
            return;

        zones.erase (zones.begin() + selectedZone);
        zones.insert (zones.begin() + selectedZone, slices.begin(), slices.end());
        owner.getProject().notifyChanged();
        refresh();
        waveformStatus.setText ("Created " + juce::String ((int) slices.size()) + " " + label + " slices starting at "
                                + noteToString (startKey) + ".", juce::dontSendNotification);
        waveformStatus.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
    }

    void SampleMapEditor::chopSelectedZoneAtTransients (int maxSlices)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        const auto points = detectTransientSlicePoints (zones[(size_t) selectedZone], maxSlices);
        if (points.size() < 3)
        {
            waveformStatus.setText ("Transient chop found too few attacks. Try equal slicing or a more percussive sample.",
                                    juce::dontSendNotification);
            waveformStatus.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
            return;
        }

        chopSelectedZoneAtSlicePoints (points, "Transient Chops");
    }

    void SampleMapEditor::mergeSelectedZoneWithNext()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone + 1 >= (int) zones.size())
            return;

        auto& first = zones[(size_t) selectedZone];
        const auto second = zones[(size_t) selectedZone + 1];
        if (first.samplePath != second.samplePath)
            return;

        first.lowNote = juce::jmin (first.lowNote, second.lowNote);
        first.highNote = juce::jmax (first.highNote, second.highNote);
        first.lowVelocity = juce::jmin (first.lowVelocity, second.lowVelocity);
        first.highVelocity = juce::jmax (first.highVelocity, second.highVelocity);
        first.rootNote = juce::jlimit (first.lowNote, first.highNote, first.rootNote);
        first.gainDb = (first.gainDb + second.gainDb) * 0.5f;
        first.pan = (first.pan + second.pan) * 0.5f;

        zones.erase (zones.begin() + selectedZone + 1);
        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::resetSelectedZonePlaybackEdits()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& zone = zones[(size_t) selectedZone];
        zone.sampleStart = 0;
        zone.sampleEnd = 0;
        zone.loopEnabled = false;
        zone.loopStart = 0;
        zone.loopEnd = 0;
        zone.fadeInStart = 0;
        zone.fadeInLength = 0;
        zone.fadeOutStart = 0;
        zone.fadeOutLength = 0;
        zone.reverse = false;
        zone.pitchOffset = 0.0f;

        owner.getProject().notifyChanged();
        refresh();
    }

    void SampleMapEditor::setSelectedZoneBoundsToFullSample()
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (selectedZone < 0 || selectedZone >= (int) zones.size())
            return;

        auto& zone = zones[(size_t) selectedZone];
        if (selectedWaveformBuffer.getNumSamples() <= 0)
        {
            juce::String status;
            loadWaveformForZone (zone, status);
        }

        if (selectedWaveformBuffer.getNumSamples() <= 0)
            return;

        zone.sampleStart = 0;
        zone.sampleEnd = selectedWaveformBuffer.getNumSamples();
        if (zone.loopEnabled)
        {
            zone.loopStart = zone.sampleStart;
            zone.loopEnd = zone.sampleEnd;
        }

        owner.getProject().notifyChanged();
        refresh();
    }

} // namespace patchcraft
