#include "OneShotMakerPage.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "SampleMap.h"
#include "SampleWaveformViewer.h"
#include "FloatingPanelWindow.h"
#include "PluginClubPublisher.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace patchcraft
{
    namespace
    {
        constexpr int kRenderBlockSize = 512;

        juce::File defaultVst3Folder()
        {
           #if JUCE_WINDOWS
            auto folder = juce::File ("C:\\Program Files\\Common Files\\VST3");
           #elif JUCE_MAC
            auto folder = juce::File ("/Library/Audio/Plug-Ins/VST3");
           #else
            auto folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
           #endif

            if (folder.exists())
                return folder;

            return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        }

        juce::String formatPercent (float value)
        {
            return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * 100.0f)) + "%";
        }

        int findPresetIndexByNameOrPatchId (PatchCraftProject& project, const juce::String& name, const juce::String& patchId)
        {
            auto& presets = project.getPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                if ((name.isNotEmpty() && presets[(size_t) i].name == name)
                    || (patchId.isNotEmpty() && presets[(size_t) i].patchId == patchId))
                    return i;
            return -1;
        }

        int findPatchIndexByIdOrName (PatchCraftProject& project, const juce::String& id, const juce::String& name)
        {
            auto& patches = project.getPatches();
            for (int i = 0; i < (int) patches.size(); ++i)
                if ((id.isNotEmpty() && patches[(size_t) i].id == id)
                    || (name.isNotEmpty() && patches[(size_t) i].name == name))
                    return i;
            return -1;
        }
    }

    OneShotMakerPage::OneShotMakerPage (StudioMainComponent& o) : owner (o)
    {
        juce::addDefaultFormatsToManager (pluginFormatManager);
        audioFormatManager.registerBasicFormats();

        outputBaseFolder = defaultOutputFolder();

        setupLabel (title, "One Shot Maker", 26.0f, true, PatchCraftLookAndFeel::textBright());
        setupLabel (subtitle,
                    "SampleRobot-style capture: import a VST3, render clean note sets, then map them into a playable PatchCraft instrument.",
                    13.0f, false, PatchCraftLookAndFeel::textDim());
        setupLabel (pluginSectionTitle, "1  Plugin Source", 14.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (settingsSectionTitle, "2  Capture Template", 14.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (exportSectionTitle, "3  Render Plan", 14.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (planSectionTitle, "Render Log", 13.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (pluginEditorTitle, "VST Interface", 14.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (pluginEditorStatusLabel, "Load a VST3, then open its real plugin interface here or in a floating window.", 12.0f, false, PatchCraftLookAndFeel::textDim());
        setupLabel (sampleEditorTitle, "One Shot Editor", 14.0f, true, PatchCraftLookAndFeel::accent());
        setupLabel (sampleEditorStatusLabel, "Render or audition a sound to review the captured WAV.", 12.0f, false, PatchCraftLookAndFeel::textDim());
        setupLabel (pluginPathLabel, "No VST3 loaded.", 12.0f, false, PatchCraftLookAndFeel::textDim());
        setupLabel (pluginStatusLabel,
                    pluginFormatManager.getNumFormats() > 0 ? "Ready for a VST3 instrument." : "VST3 hosting is not enabled in this build.",
                    12.0f, false, PatchCraftLookAndFeel::textDim());
        setupFieldLabel (hardwareMidiOutputLabel, "MIDI Out");
        setupLabel (outputPathLabel, outputBaseFolder.getFullPathName(), 12.0f, false, PatchCraftLookAndFeel::textDim());
        setupLabel (planLabel, {}, 12.0f, false, PatchCraftLookAndFeel::text());
        setupLabel (statusLabel, "Idle.", 12.0f, false, PatchCraftLookAndFeel::textDim());

        for (auto* label : { &title, &subtitle, &pluginSectionTitle, &settingsSectionTitle,
                             &exportSectionTitle, &planSectionTitle, &pluginEditorTitle,
                             &pluginEditorStatusLabel, &sampleEditorTitle, &sampleEditorStatusLabel,
                             &pluginPathLabel, &pluginStatusLabel, &outputPathLabel, &planLabel,
                             &statusLabel, &hardwareMidiOutputLabel })
            addAndMakeVisible (*label);

        for (auto* button : { &builderViewButton, &libraryViewButton, &refreshLibraryButton,
                              &openLibraryPackButton, &loadLibraryPackButton, &previewLibraryPackButton, &publishLibraryPackButton,
                              &importPluginButton, &chooseOutputButton, &chooseArtworkButton,
                              &renderButton, &buildBundleButton, &sendToMapperButton, &createPatchButton,
                              &publishPackButton,
                              &refreshPluginEditorButton, &auditionPluginButton,
                              &floatPluginEditorButton, &closePluginEditorButton,
                              &previewSampleButton, &stopSampleButton })
        {
            button->getProperties().set ("corner", 8.0);
            button->getProperties().set ("bold", true);
            addAndMakeVisible (*button);
        }
        renderButton.getProperties().set ("primaryAction", true);
        auditionPluginButton.getProperties().set ("primaryAction", true);
        builderViewButton.getProperties().set ("primaryAction", true);

        builderViewButton.setTooltip ("Open the One Shot capture, editor, packaging, and export workspace.");
        libraryViewButton.setTooltip ("Browse existing one-shot packs with artwork and metadata.");
        refreshLibraryButton.setTooltip ("Rescan known one-shot pack folders.");
        openLibraryPackButton.setTooltip ("Open the selected pack folder in Explorer.");
        loadLibraryPackButton.setTooltip ("Load and audition the selected pack from this page. Studio Preview is muted while the pack player is active.");
        publishLibraryPackButton.setTooltip ("Publish the selected pack as a Plugin.club draft.");
        previewLibraryPackButton.setTooltip ("Open an in-app pack preview with sample playback, keyboard audition, artwork, and metadata editing.");
        importPluginButton.setTooltip ("Load a VST3 instrument to capture one-shot samples.");
        chooseOutputButton.setTooltip ("Choose where rendered one-shot packs are written.");
        chooseArtworkButton.setTooltip ("Attach cover artwork for the one-shot pack and commercial bundle.");
        renderButton.setTooltip ("Render the current capture plan into WAV files.");
        buildBundleButton.setTooltip ("Write commercial pack metadata, artwork, and README around the rendered WAVs.");
        sendToMapperButton.setTooltip ("Map the rendered WAVs into Sample Mapper as playable zones.");
        createPatchButton.setTooltip ("Create a playable PatchCraft sample patch and preset from this pack.");
        publishPackButton.setTooltip ("Publish the current rendered pack to Plugin.club as a seller draft.");
        refreshPluginEditorButton.setTooltip ("Open or rebuild the loaded VST plugin's own editor interface.");
        auditionPluginButton.setTooltip ("Render and preview one note from the current plugin sound.");
        floatPluginEditorButton.setTooltip ("Pop the plugin editor into a floating window.");
        closePluginEditorButton.setTooltip ("Close the embedded/floating plugin editor.");
        livePluginToggle.setTooltip ("Route hardware/software MIDI through only the loaded VST. Studio Preview is stopped first so you do not hear two instruments at once.");
        hardwareCaptureToggle.setTooltip ("Record one-shots from your audio interface input. Optionally sends the render notes to a selected MIDI output for external synths.");
        hardwareMidiOutputBox.setTooltip ("Optional MIDI output used to trigger an external keyboard or synth during hardware capture.");
        previewSampleButton.setTooltip ("Preview the selected rendered WAV with current edit settings.");
        stopSampleButton.setTooltip ("Stop rendered sample preview playback.");

        builderViewButton.onClick = [this] { setViewMode (ViewMode::Builder); };
        libraryViewButton.onClick = [this] { setViewMode (ViewMode::Library); };
        refreshLibraryButton.onClick = [this] { refreshPackLibrary(); };
        openLibraryPackButton.onClick = [this] { openSelectedPackFolder(); };
        loadLibraryPackButton.onClick = [this] { loadSelectedLibraryPackForAudition(); };
        previewLibraryPackButton.onClick = [this] { previewSelectedLibraryPack(); };
        publishLibraryPackButton.onClick = [this] { publishSelectedLibraryPack(); };
        importPluginButton.onClick = [this] { choosePlugin(); };
        chooseOutputButton.onClick = [this] { chooseOutputFolder(); };
        chooseArtworkButton.onClick = [this] { chooseArtwork(); };
        renderButton.onClick = [this] { renderPack(); };
        buildBundleButton.onClick = [this] { buildCommercialBundle(); };
        sendToMapperButton.onClick = [this] { sendToSampleMapper(); };
        createPatchButton.onClick = [this] { createPatchFromOneShotPack(); };
        publishPackButton.onClick = [this] { publishOneShotPackToPluginClub(); };
        refreshPluginEditorButton.onClick = [this]
        {
            if (pluginEditor == nullptr)
                rebuildPluginEditor();
            floatPluginEditor();
        };
        auditionPluginButton.onClick = [this] { auditionCurrentPluginSound(); };
        floatPluginEditorButton.onClick = [this] { floatPluginEditor(); };
        closePluginEditorButton.onClick = [this] { closePluginEditor(); };
        previewSampleButton.onClick = [this] { startReviewPreview(); };
        stopSampleButton.onClick = [this] { stopReviewPreview(); };

        setupFieldLabel (packNameLabel, "Pack Name");
        setupFieldLabel (creatorLabel, "Creator");
        setupFieldLabel (categoryLabel, "Category");
        setupFieldLabel (namingLabel, "Naming");
        setupFieldLabel (templateLabel, "Template");
        setupFieldLabel (barLengthLabel, "Bar Length");
        setupFieldLabel (sampleRateLabel, "Sample Rate");
        setupFieldLabel (bpmLabel, "BPM");
        setupFieldLabel (velocityLabel, "Velocity");
        setupFieldLabel (velocityLayersLabel, "Vel Layers");
        setupFieldLabel (roundRobinLabel, "Round Robin");
        setupFieldLabel (tailLabel, "Tail");
        setupFieldLabel (renderedSampleLabel, "Rendered WAV");
        setupFieldLabel (editGainLabel, "Gain");
        setupFieldLabel (fadeInLabel, "Fade In");
        setupFieldLabel (fadeOutLabel, "Fade Out");

        for (auto* label : { &packNameLabel, &creatorLabel, &categoryLabel, &namingLabel,
                             &templateLabel, &barLengthLabel, &sampleRateLabel,
                             &bpmLabel, &velocityLabel, &velocityLayersLabel, &roundRobinLabel,
                             &tailLabel, &renderedSampleLabel,
                             &editGainLabel, &fadeInLabel, &fadeOutLabel })
            addAndMakeVisible (*label);

        packNameEditor.setText ("One Shot Pack", false);
        packNameEditor.setSelectAllWhenFocused (true);
        packNameEditor.onTextChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (packNameEditor);

        creatorEditor.setText (owner.getProject().getManifest().creator, false);
        creatorEditor.setSelectAllWhenFocused (true);
        addAndMakeVisible (creatorEditor);

        categoryBox.addItem ("Keys", 1);
        categoryBox.addItem ("Synth", 2);
        categoryBox.addItem ("Bass", 3);
        categoryBox.addItem ("Drums", 4);
        categoryBox.addItem ("Pads", 5);
        categoryBox.addItem ("FX", 6);
        categoryBox.addItem ("Cinematic", 7);
        categoryBox.addItem ("General", 8);
        categoryBox.setSelectedId (8, juce::dontSendNotification);
        categoryBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (categoryBox);

        namingSchemeBox.addItem ("Pack_025_C#0", 1);
        namingSchemeBox.addItem ("001_C#0_Pack", 2);
        namingSchemeBox.addItem ("Pack_C#0", 3);
        namingSchemeBox.setSelectedId (1, juce::dontSendNotification);
        namingSchemeBox.setTooltip ("# is allowed in local sample filenames. Numeric prefixes keep Explorer/Finder sorted in pitch order.");
        namingSchemeBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (namingSchemeBox);

        templateBox.addItem ("C0-C6 Chromatic", 1);
        templateBox.addItem ("C1-C5 Chromatic", 2);
        templateBox.addItem ("C0-C6 Octaves", 3);
        templateBox.addItem ("Drum 16 Pads C1-D#2", 4);
        templateBox.addItem ("Root Only C3", 5);
        templateBox.setSelectedId (1, juce::dontSendNotification);
        templateBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (templateBox);

        barLengthBox.addItem ("1/4 Bar", 1);
        barLengthBox.addItem ("1/2 Bar", 2);
        barLengthBox.addItem ("1 Bar", 3);
        barLengthBox.addItem ("2 Bars", 4);
        barLengthBox.addItem ("4 Bars", 5);
        barLengthBox.setSelectedId (3, juce::dontSendNotification);
        barLengthBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (barLengthBox);

        sampleRateBox.addItem ("44.1 kHz", 1);
        sampleRateBox.addItem ("48 kHz", 2);
        sampleRateBox.addItem ("96 kHz", 3);
        sampleRateBox.setSelectedId (2, juce::dontSendNotification);
        sampleRateBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (sampleRateBox);

        setupSlider (bpmSlider, 40.0, 240.0, 1.0, 120.0, " BPM");
        setupSlider (velocitySlider, 1.0, 127.0, 1.0, 110.0, "");
        for (int count : { 1, 2, 3, 4 })
        {
            velocityLayersBox.addItem (juce::String (count) + (count == 1 ? " layer" : " layers"), count);
            roundRobinBox.addItem (juce::String (count) + (count == 1 ? " take" : " takes"), count);
        }
        velocityLayersBox.setSelectedId (1, juce::dontSendNotification);
        roundRobinBox.setSelectedId (1, juce::dontSendNotification);
        velocityLayersBox.setTooltip ("Render multiple velocity layers per note. Sample Mapper maps each layer to its own velocity range.");
        roundRobinBox.setTooltip ("Render repeated takes per note/velocity. Sample Mapper maps them as round-robin alternates.");
        velocityLayersBox.onChange = [this] { updateRenderPlan(); };
        roundRobinBox.onChange = [this] { updateRenderPlan(); };
        setupSlider (tailSlider, 0.0, 5000.0, 10.0, 750.0, " ms");
        setupSlider (editGainSlider, -24.0, 12.0, 0.1, 0.0, " dB");
        setupSlider (fadeInSlider, 0.0, 500.0, 1.0, 0.0, " ms");
        setupSlider (fadeOutSlider, 0.0, 5000.0, 1.0, 25.0, " ms");
        bpmSlider.onValueChange = [this] { updateRenderPlan(); };
        velocitySlider.onValueChange = [this] { updateRenderPlan(); };
        tailSlider.onValueChange = [this] { updateRenderPlan(); };
        fadeInSlider.onValueChange = [this]
        {
            if (reviewBuffer.getNumSamples() > 0)
            {
                SampleZoneDef zone = reviewZone;
                zone.fadeInStart = zone.sampleStart;
                zone.fadeInLength = juce::roundToInt ((fadeInSlider.getValue() / 1000.0) * reviewSampleRate);
                {
                    const juce::SpinLock::ScopedLockType lock (reviewLock);
                    reviewZone = zone;
                }
                if (selectedRenderedIndex >= 0 && selectedRenderedIndex < (int) renderedZones.size())
                    renderedZones[(size_t) selectedRenderedIndex] = zone;
                if (sampleWaveform != nullptr)
                    sampleWaveform->setZone (zone);
            }
            updateRenderPlan();
        };
        fadeOutSlider.onValueChange = [this]
        {
            if (reviewBuffer.getNumSamples() > 0)
            {
                SampleZoneDef zone = reviewZone;
                const int playEnd = zone.sampleEnd > 0 ? zone.sampleEnd : reviewBuffer.getNumSamples();
                zone.fadeOutLength = juce::roundToInt ((fadeOutSlider.getValue() / 1000.0) * reviewSampleRate);
                zone.fadeOutStart = juce::jmax (zone.sampleStart, playEnd - zone.fadeOutLength);
                {
                    const juce::SpinLock::ScopedLockType lock (reviewLock);
                    reviewZone = zone;
                }
                if (selectedRenderedIndex >= 0 && selectedRenderedIndex < (int) renderedZones.size())
                    renderedZones[(size_t) selectedRenderedIndex] = zone;
                if (sampleWaveform != nullptr)
                    sampleWaveform->setZone (zone);
            }
            updateRenderPlan();
        };
        editGainSlider.onValueChange = [this]
        {
            SampleZoneDef zone = reviewZone;
            zone.gainDb = (float) editGainSlider.getValue();
            {
                const juce::SpinLock::ScopedLockType lock (reviewLock);
                reviewZone = zone;
            }
            if (selectedRenderedIndex >= 0 && selectedRenderedIndex < (int) renderedZones.size())
            {
                renderedZones[(size_t) selectedRenderedIndex].gainDb = zone.gainDb;
                if (sampleWaveform != nullptr)
                    sampleWaveform->setZone (renderedZones[(size_t) selectedRenderedIndex]);
            }
            else if (sampleWaveform != nullptr)
            {
                sampleWaveform->setZone (zone);
            }
        };
        addAndMakeVisible (bpmSlider);
        addAndMakeVisible (velocitySlider);
        addAndMakeVisible (velocityLayersBox);
        addAndMakeVisible (roundRobinBox);
        addAndMakeVisible (tailSlider);
        addAndMakeVisible (editGainSlider);
        addAndMakeVisible (fadeInSlider);
        addAndMakeVisible (fadeOutSlider);

        normalizeToggle.setToggleState (true, juce::dontSendNotification);
        normalizeToggle.setTooltip ("Normalize rendered WAVs so one-shot packs import at consistent playback level.");
        normalizeToggle.onClick = [this] { updateRenderPlan(); };
        trimStartToggle.setToggleState (true, juce::dontSendNotification);
        trimStartToggle.setTooltip ("Removes plugin latency/empty attack space before writing WAVs for samplers and sample-pack platforms.");
        trimStartToggle.onClick = [this] { updateRenderPlan(); };
        replaceMapperToggle.setToggleState (true, juce::dontSendNotification);
        replaceMapperToggle.setTooltip ("Replace the current Sample Mapper zones when sending this one-shot pack into the project.");
        editReverseToggle.setTooltip ("Reverse the selected rendered one-shot while previewing or sending to Sample Mapper.");
        editReverseToggle.onClick = [this]
        {
            SampleZoneDef zone = reviewZone;
            zone.reverse = editReverseToggle.getToggleState();
            {
                const juce::SpinLock::ScopedLockType lock (reviewLock);
                reviewZone = zone;
            }
            if (selectedRenderedIndex >= 0 && selectedRenderedIndex < (int) renderedZones.size())
            {
                renderedZones[(size_t) selectedRenderedIndex].reverse = zone.reverse;
                if (sampleWaveform != nullptr)
                    sampleWaveform->setZone (renderedZones[(size_t) selectedRenderedIndex]);
            }
            else if (sampleWaveform != nullptr)
            {
                sampleWaveform->setZone (zone);
            }
        };
        livePluginToggle.onClick = [this]
        {
            if (livePluginToggle.getToggleState())
                startLivePluginHost();
            else
                stopLivePluginHost (true);
        };
        hardwareCaptureToggle.onClick = [this]
        {
            if (hardwareCaptureToggle.getToggleState())
            {
                stopLivePluginHost (false);
                pluginStatusLabel.setText ("Hardware input capture armed. Audio input will be recorded into one-shot WAVs.",
                                           juce::dontSendNotification);
            }
            else
            {
                pluginStatusLabel.setText (pluginInstance != nullptr ? "Plugin loaded. Ready to render."
                                                                      : "Ready for a VST3 instrument or hardware input.",
                                           juce::dontSendNotification);
            }
            updateRenderPlan();
            setControlsEnabledForRenderState();
        };

        for (auto* toggle : { &normalizeToggle, &trimStartToggle, &replaceMapperToggle, &editReverseToggle,
                              &livePluginToggle, &hardwareCaptureToggle })
        {
            toggle->setColour (juce::ToggleButton::textColourId, PatchCraftLookAndFeel::text());
            addAndMakeVisible (*toggle);
        }

        populateHardwareMidiOutputs();
        hardwareMidiOutputBox.onChange = [this] { updateRenderPlan(); };
        addAndMakeVisible (hardwareMidiOutputBox);

        renderedSampleBox.onChange = [this] { loadRenderedSampleForReview (renderedSampleBox.getSelectedId() - 1); };
        addAndMakeVisible (renderedSampleBox);

        pluginEditorViewport.setViewedComponent (&pluginEditorHost, false);
        pluginEditorViewport.setScrollBarsShown (true, true);
        addAndMakeVisible (pluginEditorViewport);

        sampleWaveform = std::make_unique<SampleWaveformViewer>();
        sampleWaveform->onZoneChanged = [this] { updateReviewZoneFromWaveform(); };
        addAndMakeVisible (*sampleWaveform);

        renderLog.setMultiLine (true);
        renderLog.setReadOnly (true);
        renderLog.setScrollbarsShown (true);
        renderLog.setCaretVisible (false);
        renderLog.setText ("Load a VST3 instrument, choose a template, then render a pack.\n", false);
        addAndMakeVisible (renderLog);

        packLibraryList.setRowHeight (86);
        packLibraryList.setMultipleSelectionEnabled (false);
        addAndMakeVisible (packLibraryList);

        updateRenderPlan();
        refreshPackLibrary();
        setViewMode (ViewMode::Builder);
    }

    OneShotMakerPage::~OneShotMakerPage()
    {
        hardwareCaptureActive.store (false);
        hardwareCaptureCallbackActive = false;
        stopLivePluginHost (true);
        stopReviewPreview();
        destroyPluginEditor();
        if (pluginInstance != nullptr)
            pluginInstance->releaseResources();
    }

    void OneShotMakerPage::setupLabel (juce::Label& label, const juce::String& text, float size,
                                       bool bold, juce::Colour colour)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void OneShotMakerPage::setupFieldLabel (juce::Label& label, const juce::String& text)
    {
        setupLabel (label, text, 11.0f, true, PatchCraftLookAndFeel::textDim());
    }

    void OneShotMakerPage::setupSlider (juce::Slider& slider, double min, double max, double interval,
                                        double value, const juce::String& suffix)
    {
        slider.setRange (min, max, interval);
        slider.setValue (value, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 22);
        slider.setTextValueSuffix (suffix);
    }

    void OneShotMakerPage::drawCard (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent) const
    {
        auto r = bounds.toFloat();
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (PatchCraftLookAndFeel::border().brighter (0.12f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.withHeight (3.0f), 2.0f);
    }

    void OneShotMakerPage::setViewMode (ViewMode mode)
    {
        viewMode = mode;
        const bool library = viewMode == ViewMode::Library;
        setBuilderControlsVisible (! library);
        packLibraryList.setVisible (library);
        refreshLibraryButton.setVisible (library);
        openLibraryPackButton.setVisible (library);
        loadLibraryPackButton.setVisible (library);
        previewLibraryPackButton.setVisible (library);
        publishLibraryPackButton.setVisible (library);
        builderViewButton.getProperties().set ("primaryAction", ! library);
        libraryViewButton.getProperties().set ("primaryAction", library);
        if (library)
            refreshPackLibrary();
        resized();
        repaint();
    }

    void OneShotMakerPage::setBuilderControlsVisible (bool show)
    {
        for (auto* component : {
                 static_cast<juce::Component*> (&pluginSectionTitle),
                 static_cast<juce::Component*> (&settingsSectionTitle),
                 static_cast<juce::Component*> (&exportSectionTitle),
                 static_cast<juce::Component*> (&planSectionTitle),
                 static_cast<juce::Component*> (&pluginEditorTitle),
                 static_cast<juce::Component*> (&pluginEditorStatusLabel),
                 static_cast<juce::Component*> (&sampleEditorTitle),
                 static_cast<juce::Component*> (&sampleEditorStatusLabel),
                 static_cast<juce::Component*> (&pluginPathLabel),
                 static_cast<juce::Component*> (&pluginStatusLabel),
                 static_cast<juce::Component*> (&outputPathLabel),
                 static_cast<juce::Component*> (&planLabel),
                 static_cast<juce::Component*> (&statusLabel),
                 static_cast<juce::Component*> (&importPluginButton),
                 static_cast<juce::Component*> (&chooseOutputButton),
                 static_cast<juce::Component*> (&chooseArtworkButton),
                 static_cast<juce::Component*> (&renderButton),
                 static_cast<juce::Component*> (&buildBundleButton),
                 static_cast<juce::Component*> (&sendToMapperButton),
                 static_cast<juce::Component*> (&createPatchButton),
                 static_cast<juce::Component*> (&publishPackButton),
                 static_cast<juce::Component*> (&refreshPluginEditorButton),
                 static_cast<juce::Component*> (&auditionPluginButton),
                 static_cast<juce::Component*> (&floatPluginEditorButton),
                 static_cast<juce::Component*> (&closePluginEditorButton),
                 static_cast<juce::Component*> (&livePluginToggle),
                 static_cast<juce::Component*> (&hardwareCaptureToggle),
                 static_cast<juce::Component*> (&hardwareMidiOutputLabel),
                 static_cast<juce::Component*> (&hardwareMidiOutputBox),
                 static_cast<juce::Component*> (&previewSampleButton),
                 static_cast<juce::Component*> (&stopSampleButton),
                 static_cast<juce::Component*> (&packNameLabel),
                 static_cast<juce::Component*> (&packNameEditor),
                 static_cast<juce::Component*> (&creatorLabel),
                 static_cast<juce::Component*> (&creatorEditor),
                 static_cast<juce::Component*> (&categoryLabel),
                 static_cast<juce::Component*> (&categoryBox),
                 static_cast<juce::Component*> (&namingLabel),
                 static_cast<juce::Component*> (&namingSchemeBox),
                 static_cast<juce::Component*> (&templateLabel),
                 static_cast<juce::Component*> (&templateBox),
                 static_cast<juce::Component*> (&barLengthLabel),
                 static_cast<juce::Component*> (&barLengthBox),
                 static_cast<juce::Component*> (&sampleRateLabel),
                 static_cast<juce::Component*> (&sampleRateBox),
                 static_cast<juce::Component*> (&bpmLabel),
                 static_cast<juce::Component*> (&bpmSlider),
                 static_cast<juce::Component*> (&velocityLabel),
                 static_cast<juce::Component*> (&velocitySlider),
                 static_cast<juce::Component*> (&tailLabel),
                 static_cast<juce::Component*> (&tailSlider),
                 static_cast<juce::Component*> (&normalizeToggle),
                 static_cast<juce::Component*> (&trimStartToggle),
                 static_cast<juce::Component*> (&replaceMapperToggle),
                 static_cast<juce::Component*> (&renderedSampleLabel),
                 static_cast<juce::Component*> (&renderedSampleBox),
                 static_cast<juce::Component*> (&editGainLabel),
                 static_cast<juce::Component*> (&editGainSlider),
                 static_cast<juce::Component*> (&fadeInLabel),
                 static_cast<juce::Component*> (&fadeInSlider),
                 static_cast<juce::Component*> (&fadeOutLabel),
                 static_cast<juce::Component*> (&fadeOutSlider),
                 static_cast<juce::Component*> (&editReverseToggle),
                 static_cast<juce::Component*> (&renderLog),
                 static_cast<juce::Component*> (&pluginEditorViewport) })
            component->setVisible (show);

        if (sampleWaveform != nullptr)
            sampleWaveform->setVisible (show);
    }

    void OneShotMakerPage::choosePlugin()
    {
        if (rendering.load()) return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose a VST3 instrument",
            defaultVst3Folder(),
            "*.vst3");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                loadPlugin (file);
            });
    }

    void OneShotMakerPage::chooseOutputFolder()
    {
        if (rendering.load()) return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose where one-shot packs should be written",
            outputBaseFolder,
            "*");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;
                outputBaseFolder = folder;
                outputPathLabel.setText (outputBaseFolder.getFullPathName(), juce::dontSendNotification);
                updateRenderPlan();
            });
    }

    void OneShotMakerPage::chooseArtwork()
    {
        if (rendering.load()) return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose one-shot pack artwork",
            juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
            "*.png;*.jpg;*.jpeg");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                artworkFile = file;
                outputPathLabel.setText ("Artwork: " + artworkFile.getFileName()
                                         + "  |  Output: " + outputBaseFolder.getFullPathName(),
                                         juce::dontSendNotification);
            });
    }

    void OneShotMakerPage::loadPlugin (const juce::File& file)
    {
        stopLivePluginHost (true);
        stopReviewPreview();
        destroyPluginEditor();
        pluginInstance.reset();
        pluginFile = file;
        renderedSamples.clear();
        renderedZones.clear();
        populateRenderedSampleBox();

        if (pluginFormatManager.getNumFormats() == 0)
        {
            pluginStatusLabel.setText ("VST3 hosting is unavailable. Rebuild Studio with JUCE_PLUGINHOST_VST3 enabled.",
                                       juce::dontSendNotification);
            updateRenderPlan();
            return;
        }

        juce::OwnedArray<juce::PluginDescription> found;
        for (int i = 0; i < pluginFormatManager.getNumFormats(); ++i)
        {
            if (auto* format = pluginFormatManager.getFormat (i))
            {
                if (! format->getName().containsIgnoreCase ("VST3"))
                    continue;

                knownPlugins.scanAndAddFile (file.getFullPathName(), false, found, *format);
            }
        }

        if (found.isEmpty())
        {
            pluginPathLabel.setText (file.getFullPathName(), juce::dontSendNotification);
            pluginStatusLabel.setText ("No VST3 instrument was found in that file/folder.", juce::dontSendNotification);
            updateRenderPlan();
            return;
        }

        pluginDescription = *found.getFirst();
        pluginPathLabel.setText (file.getFullPathName(), juce::dontSendNotification);
        pluginStatusLabel.setText ("Loading " + pluginDescription.name + "...", juce::dontSendNotification);
        appendLog ("Scanning VST3: " + file.getFullPathName());
        setControlsEnabledForRenderState();

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        pluginFormatManager.createPluginInstanceAsync (
            pluginDescription,
            selectedSampleRate(),
            kRenderBlockSize,
            [safeThis] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
            {
                if (safeThis == nullptr) return;

                if (instance == nullptr)
                {
                    safeThis->destroyPluginEditor();
                    safeThis->pluginInstance.reset();
                    safeThis->pluginStatusLabel.setText (error.isNotEmpty() ? error : "Plugin load failed.",
                                                         juce::dontSendNotification);
                    safeThis->pluginEditorStatusLabel.setText ("Plugin did not load.", juce::dontSendNotification);
                    safeThis->appendLog ("Plugin load failed: " + error);
                }
                else
                {
                    safeThis->pluginInstance = std::move (instance);
                    safeThis->pluginInstance->setNonRealtime (true);
                    safeThis->pluginStatusLabel.setText ("Loaded: " + safeThis->pluginDescription.name,
                                                         juce::dontSendNotification);
                    safeThis->appendLog ("Loaded plugin: " + safeThis->pluginDescription.name);
                    safeThis->rebuildPluginEditor();
                    safeThis->startLivePluginHost();
                }

                safeThis->updateRenderPlan();
            });
    }

    void OneShotMakerPage::destroyPluginEditor()
    {
        if (floatingPluginWindow != nullptr)
        {
            if (pluginEditor != nullptr)
                pluginEditorHost.addAndMakeVisible (*pluginEditor);
            floatingPluginWindow.reset();
        }

        if (pluginEditor != nullptr)
            pluginEditorHost.removeChildComponent (pluginEditor.get());

        pluginEditor.reset();
        pluginEditorHost.setSize (0, 0);
        pluginEditorStatusLabel.setText ("Load a VST3, then open its real plugin interface here or in a floating window.", juce::dontSendNotification);
        resized();
    }

    void OneShotMakerPage::rebuildPluginEditor()
    {
        destroyPluginEditor();

        if (pluginInstance == nullptr)
        {
            pluginEditorStatusLabel.setText ("No plugin loaded.", juce::dontSendNotification);
            return;
        }

        if (pluginInstance->hasEditor())
        {
            pluginEditor.reset (pluginInstance->createEditor());
        }

        if (pluginEditor == nullptr)
        {
            pluginEditor = std::make_unique<juce::GenericAudioProcessorEditor> (*pluginInstance);
        }

        if (pluginEditor == nullptr)
        {
            pluginEditorStatusLabel.setText ("This plugin did not provide an editor.", juce::dontSendNotification);
            return;
        }

        pluginEditorHost.addAndMakeVisible (*pluginEditor);

        const int editorWidth = pluginEditor->getWidth() > 0 ? pluginEditor->getWidth() : 760;
        const int editorHeight = pluginEditor->getHeight() > 0 ? pluginEditor->getHeight() : 420;
        pluginEditor->setBounds (0, 0, editorWidth, editorHeight);
        pluginEditorHost.setSize (editorWidth, editorHeight);
        pluginEditor->setVisible (true);
        pluginEditorViewport.setVisible (true);
        pluginEditorViewport.toFront (false);

        pluginEditorStatusLabel.setText ("VST interface loaded. Choose a preset/sound here, then audition, monitor, or render.",
                                         juce::dontSendNotification);
        resized();
    }

    void OneShotMakerPage::floatPluginEditor()
    {
        if (pluginEditor == nullptr)
            rebuildPluginEditor();
        if (pluginEditor == nullptr)
            return;

        if (floatingPluginWindow != nullptr)
        {
            floatingPluginWindow->toFront (true);
            return;
        }

        pluginEditorHost.removeChildComponent (pluginEditor.get());

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        floatingPluginWindow = std::make_unique<FloatingPanelWindow> (
            pluginDescription.name.isNotEmpty() ? pluginDescription.name : juce::String ("Plugin Editor"),
            pluginEditor.get(),
            [safeThis]
            {
                juce::MessageManager::callAsync ([safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->redockPluginEditor();
                });
            });
        pluginEditorStatusLabel.setText ("Plugin editor is floating. Close the floating window to dock it again.",
                                         juce::dontSendNotification);
        resized();
    }

    void OneShotMakerPage::redockPluginEditor()
    {
        if (pluginEditor == nullptr)
        {
            floatingPluginWindow.reset();
            resized();
            return;
        }

        pluginEditorHost.addAndMakeVisible (*pluginEditor);
        floatingPluginWindow.reset();
        const int editorWidth = pluginEditor->getWidth() > 0 ? pluginEditor->getWidth() : 760;
        const int editorHeight = pluginEditor->getHeight() > 0 ? pluginEditor->getHeight() : 420;
        pluginEditor->setBounds (0, 0, editorWidth, editorHeight);
        pluginEditorHost.setSize (editorWidth, editorHeight);
        pluginEditorStatusLabel.setText ("VST interface docked. Choose a preset/sound here, then play MIDI or render.",
                                         juce::dontSendNotification);
        resized();
    }

    void OneShotMakerPage::closePluginEditor()
    {
        destroyPluginEditor();
        pluginEditorStatusLabel.setText ("VST interface closed. The plugin stays loaded; use Open VST UI to reopen it.",
                                         juce::dontSendNotification);
    }

    void OneShotMakerPage::syncPluginEditorHostSize()
    {
        if (pluginEditor == nullptr || floatingPluginWindow != nullptr)
            return;

        const int editorWidth = juce::jmax (1, pluginEditor->getWidth());
        const int editorHeight = juce::jmax (1, pluginEditor->getHeight());
        const int hostWidth = juce::jmax (editorWidth, pluginEditorViewport.getWidth());
        const int hostHeight = juce::jmax (editorHeight, pluginEditorViewport.getHeight());

        if (pluginEditorHost.getWidth() != hostWidth || pluginEditorHost.getHeight() != hostHeight)
            pluginEditorHost.setSize (hostWidth, hostHeight);
    }

    void OneShotMakerPage::ensureSharedAudioCallback()
    {
        if (! audioCallbackRegistered)
        {
            owner.getAudio().getDeviceManager().addAudioCallback (this);
            audioCallbackRegistered = true;
        }
    }

    void OneShotMakerPage::releaseSharedAudioCallbackIfIdle()
    {
        if (audioCallbackRegistered && ! livePluginCallbackActive && ! reviewCallbackActive && ! hardwareCaptureCallbackActive)
        {
            owner.getAudio().getDeviceManager().removeAudioCallback (this);
            audioCallbackRegistered = false;
        }
    }

    void OneShotMakerPage::prepareLivePluginForDevice (juce::AudioIODevice* device)
    {
        if (pluginInstance == nullptr || device == nullptr)
            return;

        livePluginSampleRate = device->getCurrentSampleRate();
        livePluginBlockSize = device->getCurrentBufferSizeSamples();
        livePluginChannels = juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits());

        const juce::SpinLock::ScopedLockType lock (pluginProcessLock);
        pluginInstance->setNonRealtime (false);
        pluginInstance->prepareToPlay (livePluginSampleRate, livePluginBlockSize);
        pluginProcessBuffer.setSize (juce::jmax (2, juce::jmax (livePluginChannels,
                                                                pluginInstance->getTotalNumOutputChannels())),
                                     livePluginBlockSize);
        pluginProcessBuffer.clear();
        livePluginPrepared.store (true);
    }

    void OneShotMakerPage::populateHardwareMidiOutputs()
    {
        const auto previousText = hardwareMidiOutputBox.getText();
        hardwareMidiOutputBox.clear (juce::dontSendNotification);
        hardwareMidiOutputBox.addItem ("No MIDI output", 1);

        int id = 2;
        for (const auto& output : juce::MidiOutput::getAvailableDevices())
            hardwareMidiOutputBox.addItem (output.name, id++);

        hardwareMidiOutputBox.setSelectedId (1, juce::dontSendNotification);
        if (previousText.isNotEmpty())
            hardwareMidiOutputBox.setText (previousText, juce::dontSendNotification);
    }

    void OneShotMakerPage::startLivePluginHost()
    {
        if (pluginInstance == nullptr)
        {
            livePluginToggle.setToggleState (false, juce::dontSendNotification);
            return;
        }

        if (owner.isPreviewActive())
            owner.togglePreview();
        if (reviewCallbackActive || reviewPreviewPlaying.load())
            stopReviewPreview();

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            livePluginToggle.setToggleState (false, juce::dontSendNotification);
            pluginEditorStatusLabel.setText ("Live monitor unavailable: " + error, juce::dontSendNotification);
            return;
        }

        auto& deviceManager = owner.getAudio().getDeviceManager();
        for (const auto& input : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        if (! livePluginCallbackActive)
            deviceManager.addMidiInputDeviceCallback ({}, this);

        livePluginCallbackActive = true;
        livePluginEnabled.store (true);
        livePluginToggle.setToggleState (true, juce::dontSendNotification);

        if (auto* device = deviceManager.getCurrentAudioDevice())
            prepareLivePluginForDevice (device);

        ensureSharedAudioCallback();
        pluginEditorStatusLabel.setText ("Live monitor on. Studio Preview is muted; MIDI is routed to the loaded VST only.",
                                         juce::dontSendNotification);
        startTimerHz (20);
        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::stopLivePluginHost (bool resetPlugin)
    {
        livePluginEnabled.store (false);
        livePluginToggle.setToggleState (false, juce::dontSendNotification);

        if (livePluginCallbackActive)
        {
            owner.getAudio().getDeviceManager().removeMidiInputDeviceCallback ({}, this);
            livePluginCallbackActive = false;
        }

        {
            const juce::ScopedLock midiGuard (liveMidiLock);
            liveMidiBuffer.clear();
        }

        if (resetPlugin && pluginInstance != nullptr)
        {
            const juce::SpinLock::ScopedLockType lock (pluginProcessLock);
            pluginInstance->reset();
            livePluginPrepared.store (false);
        }

        releaseSharedAudioCallbackIfIdle();
        if (! audioCallbackRegistered && ! rendering.load())
            stopTimer();
        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::auditionCurrentPluginSound()
    {
        if (rendering.load()) return;
        if (pluginInstance == nullptr)
        {
            showMessage ("One Shot Maker", "Load a VST3 instrument before auditioning.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const bool restoreLive = livePluginEnabled.load();
        stopLivePluginHost (false);
        auto plan = buildRenderPlan();
        const auto note = plan.empty()
            ? RenderNote { midiCForOctave (3), juce::roundToInt (velocitySlider.getValue()), 0, 1, 0, 1, noteName (midiCForOctave (3)) }
            : plan.front();
        auto settings = currentRenderSettings();
        settings.packName = "Audition";
        settings.bars = selectedBarLength();
        settings.tailMs = juce::roundToInt (tailSlider.getValue());
        settings.normalize = normalizeToggle.getToggleState();

        auto auditionFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("OneShotAuditions");
        auditionFolder.createDirectory();
        const auto file = auditionFolder.getChildFile ("Audition_" + note.label.replaceCharacter ('#', 's')
                                                       + "_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");

        renderedSampleBox.setSelectedId (0, juce::dontSendNotification);
        selectedRenderedIndex = -1;
        sampleEditorStatusLabel.setText ("Rendering audition for " + note.label + "...", juce::dontSendNotification);
        appendLog ("Auditioning current plugin sound at " + note.label);
        setControlsEnabledForRenderState();

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        juce::Thread::launch ([safeThis, settings, note, file, restoreLive]
        {
            if (safeThis == nullptr) return;
            juce::String error;
            const bool ok = safeThis->renderSingleNote (settings, note, file, error);
            juce::MessageManager::callAsync ([safeThis, ok, file, note, error, restoreLive]
            {
                if (safeThis == nullptr) return;
                if (! ok)
                {
                    safeThis->sampleEditorStatusLabel.setText ("Audition failed: " + error, juce::dontSendNotification);
                    safeThis->appendLog ("Audition failed: " + error);
                    return;
                }

                safeThis->loadReviewFile (file, note.midiNote, "Audition " + note.label, false);
                safeThis->startReviewPreview();
                if (restoreLive)
                    safeThis->startLivePluginHost();
            });
        });
    }

    void OneShotMakerPage::updateRenderPlan()
    {
        const auto notes = buildRenderPlan();
        const auto bars = selectedBarLength();
        const auto seconds = (60.0 / juce::jmax (1.0, bpmSlider.getValue())) * 4.0 * bars
                           + tailSlider.getValue() / 1000.0;

        juce::String summary = selectedTemplateName() + "  |  "
                             + juce::String ((int) notes.size()) + " WAVs  |  "
                             + juce::String (seconds, 2) + " sec each  |  "
                             + "fade " + juce::String (juce::roundToInt (fadeInSlider.getValue()))
                             + "/" + juce::String (juce::roundToInt (fadeOutSlider.getValue())) + " ms";
        if (trimStartToggle.getToggleState())
            summary += "  |  trim start";
        if (velocityLayersBox.getSelectedId() > 1)
            summary += "  |  " + juce::String (velocityLayersBox.getSelectedId()) + " vel layers";
        if (roundRobinBox.getSelectedId() > 1)
            summary += "  |  " + juce::String (roundRobinBox.getSelectedId()) + " RR";
        if (hardwareCaptureToggle.getToggleState())
        {
            summary += "  |  hardware input";
            if (hardwareMidiOutputBox.getSelectedId() > 1)
                summary += " -> " + hardwareMidiOutputBox.getText();
        }
        if (! notes.empty())
            summary += "  |  " + notes.front().label + " to " + notes.back().label;
        planLabel.setText (summary, juce::dontSendNotification);

        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::setControlsEnabledForRenderState()
    {
        const bool busy = rendering.load();
        const bool canEdit = ! busy;
        const bool hasPlugin = pluginInstance != nullptr;
        const bool hardwareMode = hardwareCaptureToggle.getToggleState();
        const bool hasPlan = ! buildRenderPlan().empty();

        for (auto* c : { static_cast<juce::Component*> (&importPluginButton),
                         static_cast<juce::Component*> (&chooseOutputButton),
                         static_cast<juce::Component*> (&chooseArtworkButton),
                         static_cast<juce::Component*> (&creatorEditor),
                         static_cast<juce::Component*> (&categoryBox),
                         static_cast<juce::Component*> (&namingSchemeBox),
                         static_cast<juce::Component*> (&packNameEditor),
                         static_cast<juce::Component*> (&templateBox),
                         static_cast<juce::Component*> (&barLengthBox),
                         static_cast<juce::Component*> (&sampleRateBox),
                         static_cast<juce::Component*> (&bpmSlider),
                         static_cast<juce::Component*> (&velocitySlider),
                         static_cast<juce::Component*> (&velocityLayersBox),
                         static_cast<juce::Component*> (&roundRobinBox),
                         static_cast<juce::Component*> (&tailSlider),
                         static_cast<juce::Component*> (&fadeInSlider),
                         static_cast<juce::Component*> (&fadeOutSlider),
                         static_cast<juce::Component*> (&normalizeToggle),
                         static_cast<juce::Component*> (&trimStartToggle),
                         static_cast<juce::Component*> (&replaceMapperToggle),
                         static_cast<juce::Component*> (&refreshPluginEditorButton),
                         static_cast<juce::Component*> (&auditionPluginButton),
                         static_cast<juce::Component*> (&createPatchButton),
                         static_cast<juce::Component*> (&publishPackButton),
                         static_cast<juce::Component*> (&floatPluginEditorButton),
                         static_cast<juce::Component*> (&closePluginEditorButton),
                         static_cast<juce::Component*> (&livePluginToggle),
                         static_cast<juce::Component*> (&hardwareCaptureToggle),
                         static_cast<juce::Component*> (&hardwareMidiOutputBox),
                         static_cast<juce::Component*> (&renderedSampleBox),
                         static_cast<juce::Component*> (&editGainSlider),
                         static_cast<juce::Component*> (&editReverseToggle),
                         static_cast<juce::Component*> (&previewSampleButton),
                         static_cast<juce::Component*> (&stopSampleButton) })
            c->setEnabled (canEdit);

        renderButton.setEnabled (canEdit && hasPlan && (hasPlugin || hardwareMode));
        auditionPluginButton.setEnabled (canEdit && hasPlugin && hasPlan);
        refreshPluginEditorButton.setEnabled (canEdit && hasPlugin);
        floatPluginEditorButton.setEnabled (canEdit && pluginEditor != nullptr);
        closePluginEditorButton.setEnabled (canEdit && pluginEditor != nullptr);
        livePluginToggle.setEnabled (canEdit && hasPlugin && ! hardwareMode);
        hardwareCaptureToggle.setEnabled (canEdit);
        hardwareMidiOutputBox.setEnabled (canEdit && hardwareMode);
        previewSampleButton.setEnabled (canEdit && reviewBuffer.getNumSamples() > 0);
        stopSampleButton.setEnabled (reviewCallbackActive || reviewPreviewPlaying.load());
        sendToMapperButton.setEnabled (canEdit && ! renderedSamples.empty());
        createPatchButton.setEnabled (canEdit && ! renderedSamples.empty());
        publishPackButton.setEnabled (canEdit && lastPackFolder.isDirectory() && ! renderedSamples.empty());
        buildBundleButton.setEnabled (canEdit && lastPackFolder.isDirectory() && ! renderedSamples.empty());
    }

    std::vector<OneShotMakerPage::RenderNote> OneShotMakerPage::buildRenderPlan() const
    {
        std::vector<RenderNote> baseNotes;
        std::vector<RenderNote> notes;
        const int templateId = templateBox.getSelectedId();

        auto addRange = [&] (int start, int end, int step)
        {
            for (int note = start; note <= end; note += step)
                baseNotes.push_back ({ note, juce::roundToInt (velocitySlider.getValue()), 0, 1, 0, 1, noteName (note) });
        };

        if (templateId == 2)
            addRange (midiCForOctave (1), midiCForOctave (5), 1);
        else if (templateId == 3)
            addRange (midiCForOctave (0), midiCForOctave (6), 12);
        else if (templateId == 4)
            addRange (midiCForOctave (1), midiCForOctave (1) + 15, 1);
        else if (templateId == 5)
            baseNotes.push_back ({ midiCForOctave (3), juce::roundToInt (velocitySlider.getValue()), 0, 1, 0, 1, noteName (midiCForOctave (3)) });
        else
            addRange (midiCForOctave (0), midiCForOctave (6), 1);

        const int velocityLayerCount = juce::jlimit (1, 4, velocityLayersBox.getSelectedId() > 0 ? velocityLayersBox.getSelectedId() : 1);
        const int roundRobinCount = juce::jlimit (1, 4, roundRobinBox.getSelectedId() > 0 ? roundRobinBox.getSelectedId() : 1);
        const int requestedVelocity = juce::roundToInt (velocitySlider.getValue());

        auto velocityForLayer = [&] (int layerIndex)
        {
            if (velocityLayerCount <= 1)
                return requestedVelocity;

            const float t = (float) (layerIndex + 1) / (float) velocityLayerCount;
            return juce::jlimit (1, 127, juce::roundToInt (juce::jmap (t, 28.0f, 127.0f)));
        };

        for (const auto& base : baseNotes)
        {
            for (int layer = 0; layer < velocityLayerCount; ++layer)
            {
                for (int rr = 0; rr < roundRobinCount; ++rr)
                {
                    RenderNote note = base;
                    note.velocity = velocityForLayer (layer);
                    note.velocityLayerIndex = layer;
                    note.velocityLayerCount = velocityLayerCount;
                    note.roundRobinIndex = rr;
                    note.roundRobinCount = roundRobinCount;
                    note.label = noteName (note.midiNote);
                    if (velocityLayerCount > 1)
                        note.label += "_V" + juce::String (note.velocity).paddedLeft ('0', 3);
                    if (roundRobinCount > 1)
                        note.label += "_RR" + juce::String (rr + 1).paddedLeft ('0', 2);
                    notes.push_back (note);
                }
            }
        }

        return notes;
    }

    OneShotMakerPage::RenderSettings OneShotMakerPage::currentRenderSettings() const
    {
        RenderSettings settings;
        settings.packName = selectedPackName();
        settings.creatorName = selectedCreatorName();
        settings.categoryName = selectedCategoryName();
        settings.namingScheme = selectedNamingScheme();
        settings.templateName = selectedTemplateName();
        settings.sourcePluginName = pluginDescription.name;
        settings.sourcePluginPath = pluginDescription.fileOrIdentifier;
        settings.hardwareCaptureMode = hardwareCaptureToggle.getToggleState();
        if (settings.hardwareCaptureMode)
        {
            settings.sourcePluginName = "Hardware Input";
            settings.sourcePluginPath = "audio-interface-input";
            const auto selectedMidi = hardwareMidiOutputBox.getText();
            settings.hardwareMidiOutputName = selectedMidi == "No MIDI output" ? juce::String() : selectedMidi;
            for (const auto& output : juce::MidiOutput::getAvailableDevices())
                if (output.name == selectedMidi)
                    settings.hardwareMidiOutputId = output.identifier;
        }
        settings.artworkPath = artworkFile.getFullPathName();
        settings.packFolder = outputBaseFolder.getChildFile (legalStem (settings.packName));
        settings.templateId = templateBox.getSelectedId();
        settings.sampleRate = selectedSampleRate();
        settings.blockSize = kRenderBlockSize;
        settings.bpm = bpmSlider.getValue();
        settings.bars = selectedBarLength();
        settings.velocity = juce::roundToInt (velocitySlider.getValue());
        settings.tailMs = juce::roundToInt (tailSlider.getValue());
        settings.fadeInMs = juce::roundToInt (fadeInSlider.getValue());
        settings.fadeOutMs = juce::roundToInt (fadeOutSlider.getValue());
        settings.velocityLayerCount = juce::jlimit (1, 4, velocityLayersBox.getSelectedId() > 0 ? velocityLayersBox.getSelectedId() : 1);
        settings.roundRobinCount = juce::jlimit (1, 4, roundRobinBox.getSelectedId() > 0 ? roundRobinBox.getSelectedId() : 1);
        settings.normalize = normalizeToggle.getToggleState();
        settings.trimStartSilence = trimStartToggle.getToggleState();
        return settings;
    }

    juce::File OneShotMakerPage::defaultOutputFolder() const
    {
        auto folder = owner.getProject().getProjectFolder();
        if (folder.isDirectory())
            return folder.getChildFile ("OneShotPacks");

        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("OneShotPacks");
    }

    juce::String OneShotMakerPage::selectedPackName() const
    {
        auto name = packNameEditor.getText().trim();
        return name.isNotEmpty() ? name : juce::String ("One Shot Pack");
    }

    juce::String OneShotMakerPage::selectedCreatorName() const
    {
        auto name = creatorEditor.getText().trim();
        return name.isNotEmpty() ? name : juce::String ("PatchCraft User");
    }

    juce::String OneShotMakerPage::selectedCategoryName() const
    {
        return categoryBox.getText().isNotEmpty() ? categoryBox.getText() : juce::String ("General");
    }

    juce::String OneShotMakerPage::selectedTemplateName() const
    {
        return templateBox.getText().isNotEmpty() ? templateBox.getText() : juce::String ("C0-C6 Chromatic");
    }

    juce::String OneShotMakerPage::selectedNamingScheme() const
    {
        return namingSchemeBox.getSelectedId() == 2 ? juce::String ("sequence-note-pack")
             : namingSchemeBox.getSelectedId() == 3 ? juce::String ("pack-note")
             : juce::String ("pack-midi-note");
    }

    double OneShotMakerPage::selectedBarLength() const
    {
        switch (barLengthBox.getSelectedId())
        {
            case 1: return 0.25;
            case 2: return 0.5;
            case 4: return 2.0;
            case 5: return 4.0;
            default: return 1.0;
        }
    }

    double OneShotMakerPage::selectedSampleRate() const
    {
        switch (sampleRateBox.getSelectedId())
        {
            case 1: return 44100.0;
            case 3: return 96000.0;
            default: return 48000.0;
        }
    }

    juce::String OneShotMakerPage::fileStemForRenderedNote (const RenderSettings& settings,
                                                            const RenderNote& note,
                                                            int index) const
    {
        const auto packStem = legalStem (settings.packName);
        const auto noteStem = note.label.replaceCharacters (" /\\:*?\"<>|", "__________");
        const auto midi = juce::String (note.midiNote).paddedLeft ('0', 3);
        const auto seq = juce::String (index + 1).paddedLeft ('0', 3);

        if (settings.templateId == 4)
            return packStem + "_Pad" + seq + "_" + midi + "_" + noteStem;

        const auto scheme = settings.namingScheme;
        if (scheme == "sequence-note-pack")
            return seq + "_" + noteStem + "_" + packStem;
        if (scheme == "pack-note")
            return packStem + "_" + noteStem;

        return packStem + "_" + midi + "_" + noteStem;
    }

    juce::File OneShotMakerPage::fileForRenderedNote (const RenderSettings& settings,
                                                       const RenderNote& note,
                                                       int index) const
    {
        return settings.packFolder.getChildFile (fileStemForRenderedNote (settings, note, index) + ".wav");
    }

    void OneShotMakerPage::renderPack()
    {
        if (rendering.load()) return;
        stopReviewPreview();
        resumeLivePluginAfterRender = livePluginEnabled.load();
        stopLivePluginHost (false);
        const bool hardwareMode = hardwareCaptureToggle.getToggleState();
        if (! hardwareMode && pluginInstance == nullptr)
        {
            resumeLivePluginAfterRender = false;
            showMessage ("One Shot Maker", "Load a VST3 instrument, or enable Hardware Input to record an external synth.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        auto notes = buildRenderPlan();
        if (notes.empty())
        {
            if (resumeLivePluginAfterRender)
                startLivePluginHost();
            resumeLivePluginAfterRender = false;
            showMessage ("One Shot Maker", "Choose a capture template before rendering.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        auto settings = currentRenderSettings();
        if (! settings.packFolder.createDirectory())
        {
            if (resumeLivePluginAfterRender)
                startLivePluginHost();
            resumeLivePluginAfterRender = false;
            showMessage ("One Shot Maker", "Could not create output folder:\n" + settings.packFolder.getFullPathName(),
                         juce::MessageBoxIconType::WarningIcon);
            return;
        }

        renderedSamples.clear();
        renderProgress.store (0.0f);
        rendering.store (true);
        statusLabel.setText ((hardwareMode ? "Recording " : "Rendering ") + juce::String ((int) notes.size()) + " one-shots...",
                             juce::dontSendNotification);
        appendLog ((hardwareMode ? "Recording hardware pack: " : "Rendering pack: ") + settings.packName);
        setControlsEnabledForRenderState();
        startTimerHz (20);

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        juce::Thread::launch ([safeThis, settings, notes]
        {
            if (safeThis == nullptr) return;

            std::vector<RenderedSample> rendered;
            juce::String error;
            const bool ok = safeThis->renderPackToFolder (settings, notes, rendered, error);

            juce::MessageManager::callAsync ([safeThis, ok, rendered = std::move (rendered),
                                              folder = settings.packFolder, error]
            {
                if (safeThis != nullptr)
                    safeThis->finishRender (ok, std::move (rendered), folder, error);
            });
        });
    }

    bool OneShotMakerPage::renderPackToFolder (const RenderSettings& settings,
                                               const std::vector<RenderNote>& notes,
                                               std::vector<RenderedSample>& rendered,
                                               juce::String& error)
    {
        if (settings.hardwareCaptureMode)
        {
            auto metadataSettings = settings;
            if (! settings.packFolder.createDirectory())
            {
                error = "Could not create output folder: " + settings.packFolder.getFullPathName();
                return false;
            }

            for (size_t i = 0; i < notes.size(); ++i)
            {
                const auto& note = notes[i];
                const auto file = fileForRenderedNote (settings, note, (int) i);
                juce::String noteError;
                if (! renderHardwareSingleNote (settings, note, file, noteError))
                {
                    error = "Failed recording " + note.label + ": " + noteError;
                    return false;
                }

                rendered.push_back ({ note.midiNote,
                                      note.velocity,
                                      note.velocityLayerIndex,
                                      note.velocityLayerCount,
                                      note.roundRobinIndex,
                                      note.roundRobinCount,
                                      note.label,
                                      file });
                renderProgress.store ((float) (i + 1) / (float) juce::jmax ((size_t) 1, notes.size()));
                metadataSettings.sampleRate = hardwareCaptureSampleRate;
            }

            return writeMetadata (metadataSettings, rendered, error);
        }

        const juce::SpinLock::ScopedLockType processLock (pluginProcessLock);
        if (pluginInstance == nullptr)
        {
            error = "Plugin is not loaded.";
            return false;
        }

        if (! settings.packFolder.createDirectory())
        {
            error = "Could not create output folder: " + settings.packFolder.getFullPathName();
            return false;
        }

        pluginInstance->setNonRealtime (true);
        pluginInstance->prepareToPlay (settings.sampleRate, settings.blockSize);
        pluginInstance->reset();

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& note = notes[i];
            const auto file = fileForRenderedNote (settings, note, (int) i);
            juce::String noteError;

            bool resetPluginState = true;
            if (i > 0)
            {
                const auto& previous = notes[i - 1];
                const bool sameVariationGroup = note.roundRobinCount > 1
                    && previous.roundRobinCount == note.roundRobinCount
                    && note.roundRobinIndex > 0
                    && note.midiNote == previous.midiNote
                    && note.velocity == previous.velocity
                    && note.velocityLayerIndex == previous.velocityLayerIndex;
                resetPluginState = ! sameVariationGroup;
            }

            if (! renderPreparedSingleNote (settings, note, file, resetPluginState, noteError))
            {
                error = "Failed rendering " + note.label + ": " + noteError;
                pluginInstance->releaseResources();
                return false;
            }

            rendered.push_back ({ note.midiNote,
                                  note.velocity,
                                  note.velocityLayerIndex,
                                  note.velocityLayerCount,
                                  note.roundRobinIndex,
                                  note.roundRobinCount,
                                  note.label,
                                  file });
            renderProgress.store ((float) (i + 1) / (float) juce::jmax ((size_t) 1, notes.size()));
        }

        pluginInstance->releaseResources();

        if (! writeMetadata (settings, rendered, error))
            return false;

        return true;
    }

    bool OneShotMakerPage::renderSingleNote (const RenderSettings& settings, const RenderNote& note,
                                             const juce::File& file, juce::String& error)
    {
        const juce::SpinLock::ScopedLockType processLock (pluginProcessLock);
        if (pluginInstance == nullptr)
        {
            error = "Plugin is not loaded.";
            return false;
        }

        pluginInstance->setNonRealtime (true);
        pluginInstance->prepareToPlay (settings.sampleRate, settings.blockSize);
        const bool ok = renderPreparedSingleNote (settings, note, file, true, error);
        pluginInstance->releaseResources();
        return ok;
    }

    bool OneShotMakerPage::renderPreparedSingleNote (const RenderSettings& settings,
                                                     const RenderNote& note,
                                                     const juce::File& file,
                                                     bool resetPluginState,
                                                     juce::String& error)
    {
        if (pluginInstance == nullptr)
        {
            error = "Plugin is not loaded.";
            return false;
        }

        const int noteSamples = juce::roundToInt ((60.0 / juce::jmax (1.0, settings.bpm)) * 4.0 * settings.bars * settings.sampleRate);
        const int tailSamples = juce::roundToInt ((settings.tailMs / 1000.0) * settings.sampleRate);
        const int totalSamples = juce::jmax (settings.blockSize, noteSamples + tailSamples);

        if (resetPluginState)
            pluginInstance->reset();

        const int outputChannels = juce::jmax (2, pluginInstance->getTotalNumOutputChannels());
        juce::AudioBuffer<float> finalBuffer (outputChannels, totalSamples);
        finalBuffer.clear();

        juce::AudioBuffer<float> blockBuffer (outputChannels, settings.blockSize);

        for (int sample = 0; sample < totalSamples; sample += settings.blockSize)
        {
            const int numThisBlock = juce::jmin (settings.blockSize, totalSamples - sample);
            blockBuffer.setSize (outputChannels, numThisBlock, false, false, true);
            blockBuffer.clear();

            juce::MidiBuffer midi;
            if (sample == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note.midiNote,
                                                          juce::jlimit (0.0f, 1.0f,
                                                                        (float) (note.velocity > 0 ? note.velocity : settings.velocity) / 127.0f)), 0);

            if (noteSamples >= sample && noteSamples < sample + numThisBlock)
                midi.addEvent (juce::MidiMessage::noteOff (1, note.midiNote), noteSamples - sample);

            pluginInstance->processBlock (blockBuffer, midi);

            for (int ch = 0; ch < outputChannels; ++ch)
                finalBuffer.copyFrom (ch, sample, blockBuffer, ch, 0, numThisBlock);
        }

        int renderedSamplesCount = totalSamples;
        if (settings.trimStartSilence)
        {
            constexpr float silenceThreshold = 0.0005f;
            const int keepPreRoll = juce::jlimit (0, 128, juce::roundToInt (settings.sampleRate * 0.0005));
            int firstAudible = totalSamples;

            for (int sample = 0; sample < totalSamples; ++sample)
            {
                float peak = 0.0f;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                    peak = juce::jmax (peak, std::abs (finalBuffer.getSample (ch, sample)));

                if (peak >= silenceThreshold)
                {
                    firstAudible = sample;
                    break;
                }
            }

            const int trimStart = juce::jlimit (0, totalSamples - 1, firstAudible - keepPreRoll);
            if (trimStart > 0 && trimStart < totalSamples)
            {
                renderedSamplesCount = totalSamples - trimStart;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                {
                    finalBuffer.copyFrom (ch, 0, finalBuffer, ch, trimStart, renderedSamplesCount);
                    finalBuffer.clear (ch, renderedSamplesCount, totalSamples - renderedSamplesCount);
                }
            }
        }

        auto applyLinearFade = [&] (int startSample, int fadeSamples, bool fadeOut)
        {
            if (fadeSamples <= 0)
                return;

            const int start = juce::jlimit (0, renderedSamplesCount, startSample);
            const int end = juce::jlimit (start, renderedSamplesCount, start + fadeSamples);
            const int length = end - start;
            if (length <= 0)
                return;

            for (int i = 0; i < length; ++i)
            {
                const float t = (float) i / (float) juce::jmax (1, length - 1);
                const float gain = fadeOut ? (1.0f - t) : t;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                    finalBuffer.setSample (ch, start + i, finalBuffer.getSample (ch, start + i) * gain);
            }
        };

        const int fadeInSamples = juce::roundToInt ((settings.fadeInMs / 1000.0) * settings.sampleRate);
        const int fadeOutSamples = juce::roundToInt ((settings.fadeOutMs / 1000.0) * settings.sampleRate);
        applyLinearFade (0, fadeInSamples, false);
        applyLinearFade (juce::jmax (0, renderedSamplesCount - fadeOutSamples), fadeOutSamples, true);

        if (settings.normalize)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, finalBuffer.getMagnitude (ch, 0, renderedSamplesCount));

            if (peak > 0.0001f)
                finalBuffer.applyGain (0, renderedSamplesCount, juce::jmin (8.0f, 0.98f / peak));
        }

        return writeWavFile (file, finalBuffer, renderedSamplesCount, settings.sampleRate, error);
    }

    bool OneShotMakerPage::renderHardwareSingleNote (const RenderSettings& settings,
                                                     const RenderNote& note,
                                                     const juce::File& file,
                                                     juce::String& error)
    {
        juce::String audioError;
        if (! owner.getAudio().ensureOpen (audioError, 1, 2))
        {
            error = "Hardware input is not available: " + audioError;
            return false;
        }

        auto& deviceManager = owner.getAudio().getDeviceManager();
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr || device->getActiveInputChannels().countNumberOfSetBits() <= 0)
        {
            error = "Choose an audio device with at least one active input in Settings.";
            return false;
        }

        hardwareCaptureSampleRate = device->getCurrentSampleRate();
        const int noteSamples = juce::roundToInt ((60.0 / juce::jmax (1.0, settings.bpm)) * 4.0 * settings.bars * hardwareCaptureSampleRate);
        const int tailSamples = juce::roundToInt ((settings.tailMs / 1000.0) * hardwareCaptureSampleRate);
        const int totalSamples = juce::jmax (device->getCurrentBufferSizeSamples(), noteSamples + tailSamples);
        const int captureChannels = juce::jmax (1, juce::jmin (2, device->getActiveInputChannels().countNumberOfSetBits()));

        {
            const juce::ScopedLock lock (hardwareCaptureLock);
            hardwareCaptureBuffer.setSize (captureChannels, totalSamples, false, false, true);
            hardwareCaptureBuffer.clear();
            hardwareCaptureWritePosition.store (0);
        }

        std::unique_ptr<juce::MidiOutput> midiOutput;
        if (settings.hardwareMidiOutputId.isNotEmpty())
            midiOutput = juce::MidiOutput::openDevice (settings.hardwareMidiOutputId);

        hardwareCaptureCallbackActive = true;
        ensureSharedAudioCallback();
        hardwareCaptureActive.store (true);

        if (midiOutput != nullptr)
            midiOutput->sendMessageNow (juce::MidiMessage::noteOn (1, note.midiNote,
                                                                    juce::jlimit (0.0f, 1.0f,
                                                                        (float) note.velocity / 127.0f)));

        const auto started = juce::Time::getMillisecondCounterHiRes();
        const auto noteOffAt = started + (1000.0 * noteSamples / hardwareCaptureSampleRate);
        const auto stopAt = started + (1000.0 * totalSamples / hardwareCaptureSampleRate);
        bool sentNoteOff = false;
        while (juce::Time::getMillisecondCounterHiRes() < stopAt
               && hardwareCaptureWritePosition.load() < totalSamples)
        {
            if (! sentNoteOff && juce::Time::getMillisecondCounterHiRes() >= noteOffAt)
            {
                if (midiOutput != nullptr)
                    midiOutput->sendMessageNow (juce::MidiMessage::noteOff (1, note.midiNote));
                sentNoteOff = true;
            }
            juce::Thread::sleep (3);
        }

        if (! sentNoteOff && midiOutput != nullptr)
            midiOutput->sendMessageNow (juce::MidiMessage::noteOff (1, note.midiNote));

        hardwareCaptureActive.store (false);
        hardwareCaptureCallbackActive = false;
        releaseSharedAudioCallbackIfIdle();

        juce::AudioBuffer<float> finalBuffer;
        int renderedSamplesCount = 0;
        {
            const juce::ScopedLock lock (hardwareCaptureLock);
            renderedSamplesCount = juce::jlimit (0, totalSamples, hardwareCaptureWritePosition.load());
            if (renderedSamplesCount <= 0)
            {
                error = "No hardware input was captured. Check your input device, gain, and cables.";
                return false;
            }
            finalBuffer.setSize (hardwareCaptureBuffer.getNumChannels(), renderedSamplesCount);
            for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                finalBuffer.copyFrom (ch, 0, hardwareCaptureBuffer, ch, 0, renderedSamplesCount);
        }

        if (settings.trimStartSilence)
        {
            constexpr float silenceThreshold = 0.0005f;
            const int keepPreRoll = juce::jlimit (0, 128, juce::roundToInt (hardwareCaptureSampleRate * 0.0005));
            int firstAudible = renderedSamplesCount;
            for (int sample = 0; sample < renderedSamplesCount; ++sample)
            {
                float peak = 0.0f;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                    peak = juce::jmax (peak, std::abs (finalBuffer.getSample (ch, sample)));
                if (peak >= silenceThreshold)
                {
                    firstAudible = sample;
                    break;
                }
            }

            const int trimStart = juce::jlimit (0, juce::jmax (0, renderedSamplesCount - 1), firstAudible - keepPreRoll);
            if (trimStart > 0 && trimStart < renderedSamplesCount)
            {
                const int newCount = renderedSamplesCount - trimStart;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                    finalBuffer.copyFrom (ch, 0, finalBuffer, ch, trimStart, newCount);
                renderedSamplesCount = newCount;
            }
        }

        auto applyLinearFade = [&] (int startSample, int fadeSamples, bool fadeOut)
        {
            const int start = juce::jlimit (0, renderedSamplesCount, startSample);
            const int end = juce::jlimit (start, renderedSamplesCount, start + fadeSamples);
            const int length = end - start;
            for (int i = 0; i < length; ++i)
            {
                const float t = (float) i / (float) juce::jmax (1, length - 1);
                const float gain = fadeOut ? (1.0f - t) : t;
                for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                    finalBuffer.setSample (ch, start + i, finalBuffer.getSample (ch, start + i) * gain);
            }
        };

        applyLinearFade (0, juce::roundToInt ((settings.fadeInMs / 1000.0) * hardwareCaptureSampleRate), false);
        const int fadeOutSamples = juce::roundToInt ((settings.fadeOutMs / 1000.0) * hardwareCaptureSampleRate);
        applyLinearFade (juce::jmax (0, renderedSamplesCount - fadeOutSamples), fadeOutSamples, true);

        if (settings.normalize)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < finalBuffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, finalBuffer.getMagnitude (ch, 0, renderedSamplesCount));
            if (peak > 0.0001f)
                finalBuffer.applyGain (0, renderedSamplesCount, juce::jmin (8.0f, 0.98f / peak));
        }

        return writeWavFile (file, finalBuffer, renderedSamplesCount, hardwareCaptureSampleRate, error);
    }

    bool OneShotMakerPage::writeWavFile (const juce::File& file, juce::AudioBuffer<float>& buffer,
                                         int numSamples, double sampleRate, juce::String& error) const
    {
        file.getParentDirectory().createDirectory();
        if (file.existsAsFile() && ! file.deleteFile())
        {
            error = "Could not overwrite " + file.getFullPathName();
            return false;
        }

        juce::WavAudioFormat format;
        auto stream = file.createOutputStream();
        if (stream == nullptr)
        {
            error = "Could not open " + file.getFullPathName() + " for writing.";
            return false;
        }

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));

        if (writer == nullptr)
        {
            error = "Could not create WAV writer for " + file.getFullPathName();
            return false;
        }

        stream.release();
        if (! writer->writeFromAudioSampleBuffer (buffer, 0, numSamples))
        {
            error = "Failed writing " + file.getFullPathName();
            return false;
        }

        return true;
    }

    bool OneShotMakerPage::writeMetadata (const RenderSettings& settings,
                                          const std::vector<RenderedSample>& rendered,
                                          juce::String& error) const
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("format", "PatchCraftOneShotPack");
        root->setProperty ("version", 1);
        root->setProperty ("packType", "commercial-one-shot");
        root->setProperty ("createdWith", "PatchCraft One Shot Maker");
        root->setProperty ("name", settings.packName);
        root->setProperty ("sourcePlugin", settings.sourcePluginName);
        root->setProperty ("sourcePluginPath", settings.sourcePluginPath);
        root->setProperty ("hardwareCaptureMode", settings.hardwareCaptureMode);
        if (settings.hardwareMidiOutputName.isNotEmpty())
            root->setProperty ("hardwareMidiOutput", settings.hardwareMidiOutputName);
        root->setProperty ("template", settings.templateName);
        root->setProperty ("creator", settings.creatorName);
        root->setProperty ("category", settings.categoryName);
        root->setProperty ("namingScheme", settings.namingScheme);
        root->setProperty ("sampleRate", settings.sampleRate);
        root->setProperty ("bpm", settings.bpm);
        root->setProperty ("barLength", settings.bars);
        root->setProperty ("tailMs", settings.tailMs);
        root->setProperty ("fadeInMs", settings.fadeInMs);
        root->setProperty ("fadeOutMs", settings.fadeOutMs);
        root->setProperty ("trimStartSilence", settings.trimStartSilence);
        root->setProperty ("velocityLayerCount", settings.velocityLayerCount);
        root->setProperty ("roundRobinCount", settings.roundRobinCount);

        juce::String artworkPath;
        juce::String artworkError;
        if (copyArtworkToPack (settings.packFolder, juce::File (settings.artworkPath), artworkPath, artworkError))
            root->setProperty ("artwork", artworkPath);

        juce::Array<juce::var> samples;
        for (const auto& item : rendered)
        {
            juce::DynamicObject::Ptr sample = new juce::DynamicObject();
            sample->setProperty ("note", item.midiNote);
            sample->setProperty ("velocity", item.velocity);
            sample->setProperty ("velocityLayerIndex", item.velocityLayerIndex);
            sample->setProperty ("velocityLayerCount", item.velocityLayerCount);
            sample->setProperty ("roundRobinIndex", item.roundRobinIndex);
            sample->setProperty ("roundRobinCount", item.roundRobinCount);
            sample->setProperty ("label", item.label);
            sample->setProperty ("file", item.file.getFileName());
            samples.add (juce::var (sample.get()));
        }
        root->setProperty ("samples", juce::var (samples));

        const auto metadataFile = settings.packFolder.getChildFile ("oneshot_pack.json");
        if (! metadataFile.replaceWithText (juce::JSON::toString (juce::var (root.get()), true)))
        {
            error = "Could not write " + metadataFile.getFullPathName();
            return false;
        }

        return true;
    }

    bool OneShotMakerPage::writeCurrentPackMetadata (juce::String& error) const
    {
        if (! lastPackFolder.isDirectory() || renderedSamples.empty())
            return true;

        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("format", "PatchCraftOneShotPack");
        root->setProperty ("version", 1);
        root->setProperty ("packType", "commercial-one-shot");
        root->setProperty ("createdWith", "PatchCraft One Shot Maker");
        root->setProperty ("name", selectedPackName());
        root->setProperty ("sourcePlugin", pluginDescription.name);
        root->setProperty ("sourcePluginPath", pluginDescription.fileOrIdentifier);
        root->setProperty ("template", selectedTemplateName());
        root->setProperty ("creator", selectedCreatorName());
        root->setProperty ("category", selectedCategoryName());
        root->setProperty ("namingScheme", selectedNamingScheme());
        root->setProperty ("sampleRate", selectedSampleRate());
        root->setProperty ("bpm", bpmSlider.getValue());
        root->setProperty ("barLength", selectedBarLength());
        root->setProperty ("tailMs", tailSlider.getValue());
        root->setProperty ("fadeInMs", fadeInSlider.getValue());
        root->setProperty ("fadeOutMs", fadeOutSlider.getValue());
        root->setProperty ("trimStartSilence", trimStartToggle.getToggleState());
        root->setProperty ("velocityLayerCount", velocityLayersBox.getSelectedId() > 0 ? velocityLayersBox.getSelectedId() : 1);
        root->setProperty ("roundRobinCount", roundRobinBox.getSelectedId() > 0 ? roundRobinBox.getSelectedId() : 1);

        juce::String artworkPath;
        juce::String artworkError;
        if (copyArtworkToPack (lastPackFolder, artworkFile, artworkPath, artworkError))
            root->setProperty ("artwork", artworkPath);

        juce::Array<juce::var> samples;
        for (size_t i = 0; i < renderedSamples.size(); ++i)
        {
            const auto& item = renderedSamples[i];
            const SampleZoneDef zone = i < renderedZones.size()
                ? renderedZones[i]
                : makeZoneForRenderedSample (item, (int) i);

            juce::DynamicObject::Ptr sample = new juce::DynamicObject();
            sample->setProperty ("note", item.midiNote);
            sample->setProperty ("velocity", item.velocity);
            sample->setProperty ("velocityLayerIndex", item.velocityLayerIndex);
            sample->setProperty ("velocityLayerCount", item.velocityLayerCount);
            sample->setProperty ("roundRobinIndex", item.roundRobinIndex);
            sample->setProperty ("roundRobinCount", item.roundRobinCount);
            sample->setProperty ("label", item.label);
            sample->setProperty ("file", item.file.getFileName());
            sample->setProperty ("sampleStart", zone.sampleStart);
            sample->setProperty ("sampleEnd", zone.sampleEnd);
            sample->setProperty ("fadeInLength", zone.fadeInLength);
            sample->setProperty ("fadeOutLength", zone.fadeOutLength);
            sample->setProperty ("gainDb", zone.gainDb);
            sample->setProperty ("reverse", zone.reverse);
            sample->setProperty ("lowVelocity", zone.lowVelocity);
            sample->setProperty ("highVelocity", zone.highVelocity);
            sample->setProperty ("roundRobinGroup", zone.roundRobinGroup);
            sample->setProperty ("zoneRoundRobinIndex", zone.roundRobinIndex);
            samples.add (juce::var (sample.get()));
        }
        root->setProperty ("samples", juce::var (samples));

        const auto metadataFile = lastPackFolder.getChildFile ("oneshot_pack.json");
        if (! metadataFile.replaceWithText (juce::JSON::toString (juce::var (root.get()), true)))
        {
            error = "Could not write " + metadataFile.getFullPathName();
            return false;
        }

        return true;
    }

    bool OneShotMakerPage::copyArtworkToPack (const juce::File& packFolder,
                                              const juce::File& sourceArtwork,
                                              juce::String& relativePath,
                                              juce::String& error) const
    {
        relativePath.clear();
        if (sourceArtwork == juce::File())
            return true;
        if (! sourceArtwork.existsAsFile())
        {
            error = "Artwork file is missing: " + sourceArtwork.getFullPathName();
            return false;
        }

        auto artworkDir = packFolder.getChildFile ("artwork");
        if (! artworkDir.createDirectory())
        {
            error = "Could not create artwork folder.";
            return false;
        }

        auto dst = artworkDir.getChildFile ("cover" + sourceArtwork.getFileExtension());
        if (dst.existsAsFile() && ! dst.deleteFile())
        {
            error = "Could not replace artwork file.";
            return false;
        }

        if (! sourceArtwork.copyFileTo (dst))
        {
            error = "Could not copy artwork into pack.";
            return false;
        }

        relativePath = dst.getRelativePathFrom (packFolder).replaceCharacter ('\\', '/');
        return true;
    }

    void OneShotMakerPage::buildCommercialBundle()
    {
        if (lastPackFolder == juce::File() || ! lastPackFolder.isDirectory())
        {
            showMessage ("Build Commercial Bundle",
                         "Render a One Shot Pack first. The bundle uses the rendered WAVs, artwork, and metadata.",
                         juce::MessageBoxIconType::WarningIcon);
            return;
        }

        juce::String error;
        if (! writeCurrentPackMetadata (error))
        {
            showMessage ("Build Commercial Bundle", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        auto readme = lastPackFolder.getChildFile ("README.txt");
        const auto text = selectedPackName() + "\n"
            + "Creator: " + selectedCreatorName() + "\n"
            + "Category: " + selectedCategoryName() + "\n"
            + "Source Plugin: " + pluginDescription.name + "\n"
            + "Template: " + selectedTemplateName() + "\n\n"
            + "Naming: " + selectedNamingScheme() + "\n"
            + "Artwork: " + (artworkFile.existsAsFile() ? artworkFile.getFileName() : juce::String ("None")) + "\n\n"
            + "This commercial one-shot bundle was created with PatchCraft One Shot Maker.\n"
            + "Files use the selected naming scheme and include metadata in oneshot_pack.json.\n";

        if (! readme.replaceWithText (text))
        {
            showMessage ("Build Commercial Bundle", "Could not write README.txt.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        showMessage ("Commercial Bundle Ready",
                     "Bundle folder is ready for packaging:\n" + lastPackFolder.getFullPathName()
                     + "\n\nCurrent build writes the commercial folder structure and metadata. Zip/export delivery can be added next.",
                     juce::MessageBoxIconType::InfoIcon);
    }

    void OneShotMakerPage::finishRender (bool ok, std::vector<RenderedSample> samples,
                                         juce::File packFolder, juce::String error)
    {
        rendering.store (false);
        renderProgress.store (ok ? 1.0f : 0.0f);
        stopTimer();

        if (ok)
        {
            renderedSamples = std::move (samples);
            renderedZones.clear();
            for (size_t i = 0; i < renderedSamples.size(); ++i)
                renderedZones.push_back (makeZoneForRenderedSample (renderedSamples[i], (int) i));

            lastPackFolder = packFolder;
            statusLabel.setText ("Rendered " + juce::String ((int) renderedSamples.size()) + " WAVs.",
                                 juce::dontSendNotification);
            appendLog ("Render complete: " + packFolder.getFullPathName());
            populateRenderedSampleBox();
            if (! renderedSamples.empty())
            {
                renderedSampleBox.setSelectedId (1, juce::dontSendNotification);
                loadRenderedSampleForReview (0);
            }
            showMessage ("One Shot Pack Complete",
                         "Rendered to:\n" + packFolder.getFullPathName()
                         + "\n\nUse Send to Sample Mapper to build a playable sample instrument.",
                         juce::MessageBoxIconType::InfoIcon);
        }
        else
        {
            renderedSamples.clear();
            renderedZones.clear();
            populateRenderedSampleBox();
            statusLabel.setText ("Render failed.", juce::dontSendNotification);
            appendLog ("Render failed: " + error);
            showMessage ("One Shot Render Failed", error, juce::MessageBoxIconType::WarningIcon);
        }

        if (resumeLivePluginAfterRender)
            startLivePluginHost();
        resumeLivePluginAfterRender = false;

        setControlsEnabledForRenderState();
        repaint();
    }

    void OneShotMakerPage::sendToSampleMapper()
    {
        stopLivePluginHost (true);
        closePluginEditor();

        juce::String error;
        if (! mapRenderedSamplesToProject (true, error))
        {
            showMessage ("One Shot Maker", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }
    }

    bool OneShotMakerPage::mapRenderedSamplesToProject (bool switchToSampleMapper, juce::String& error)
    {
        if (renderedSamples.empty())
        {
            error = "Render a one-shot pack first.";
            return false;
        }

        auto& project = owner.getProject();
        if (project.getEngineType() != "sample")
            project.setEngineType ("sample");

        auto& map = project.getSampleMap();
        if (replaceMapperToggle.getToggleState())
            map.clear();

        juce::String metadataError;
        if (! writeCurrentPackMetadata (metadataError))
            appendLog ("Could not update one-shot metadata: " + metadataError);

        const int templateId = templateBox.getSelectedId();
        const auto packName = selectedPackName();
        for (size_t i = 0; i < renderedSamples.size(); ++i)
        {
            const auto& item = renderedSamples[i];
            SampleZoneDef zone = (i < renderedZones.size())
                ? renderedZones[i]
                : makeZoneForRenderedSample (item, (int) i);
            zone.rootNote = item.midiNote;
            zone.lowNote = item.midiNote;
            zone.highNote = item.midiNote;
            zone.samplePath = item.file.getFullPathName();
            zone.bpm = (float) bpmSlider.getValue();
            zone.group = packName;
            zone.padLabel = item.label;

            const int padIndex = item.midiNote - midiCForOctave (1);
            if (templateId == 4 && padIndex >= 0 && padIndex < 16)
                zone.padIndex = padIndex;

            map.add (zone);
        }

        project.notifyChanged();
        if (switchToSampleMapper)
        {
            owner.setBottomTab (BottomPanel::Page::Samples);
            pluginEditorStatusLabel.setText ("Rendered pack is now the active sample source in this project.",
                                             juce::dontSendNotification);
            sampleEditorStatusLabel.setText ("Rendered pack is now the active sample source in this project.",
                                             juce::dontSendNotification);
            statusLabel.setText ("Rendered pack sent to Sample Mapper and set as the active source.",
                                 juce::dontSendNotification);
            appendLog ("Mapped " + juce::String ((int) renderedSamples.size())
                       + " samples into Sample Mapper. Rendered pack is now the active sample source in this project.");
        }
        else
        {
            appendLog ("Mapped " + juce::String ((int) renderedSamples.size())
                       + " samples into Sample Mapper. Create Patch From Pack captured a named sample patch/preset.");
        }
        return true;
    }

    void OneShotMakerPage::createPatchFromOneShotPack()
    {
        juce::String error;
        if (! mapRenderedSamplesToProject (false, error))
        {
            showMessage ("Create Patch From Pack", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        auto& project = owner.getProject();
        auto patchName = selectedPackName().trim();
        if (patchName.isEmpty())
            patchName = "One Shot Patch";

        auto& manifest = project.getManifest();
        if (manifest.instrumentName.trim().isEmpty())
            manifest.instrumentName = patchName;
        manifest.defaultPreset = patchName;
        manifest.engine = "sample";
        manifest.category = selectedCategoryName();
        if (selectedCreatorName().isNotEmpty())
            manifest.creator = selectedCreatorName();

        auto patch = project.captureCurrentPatch (patchName);
        patch.description = "Playable one-shot sample patch created from " + selectedPackName() + ".";
        patch.engine = "sample";
        patch.category = selectedCategoryName();
        patch.author = selectedCreatorName();
        patch.generated = false;
        patch.isDefault = true;
        patch.tags.addIfNotAlreadyThere ("one-shot");
        patch.tags.addIfNotAlreadyThere (selectedCategoryName());

        auto& expansion = project.ensureExpansion (selectedPackName() + " Pack");
        expansion.name = selectedPackName() + " Pack";
        expansion.category = selectedCategoryName();
        expansion.author = selectedCreatorName();
        expansion.description = "Commercial-ready one-shot pack generated in PatchCraft Studio.";
        if (artworkFile.existsAsFile())
            expansion.artworkPath = artworkFile.getFullPathName();

        patch.expansionId = expansion.id;
        patch.packId = expansion.id;
        expansion.folders.addIfNotAlreadyThere (selectedCategoryName());
        expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
        for (const auto& asset : patch.includedAssets)
            expansion.includedAssets.addIfNotAlreadyThere (asset);

        auto preset = patch.toPreset();
        preset.isDefault = true;
        preset.expansionId = expansion.id;
        preset.packId = expansion.id;
        expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
        pluginEditorStatusLabel.setText ("Create Patch From Pack captured this as a named sample patch/preset.",
                                         juce::dontSendNotification);
        sampleEditorStatusLabel.setText ("Create Patch From Pack captured this as a named sample patch/preset.",
                                         juce::dontSendNotification);
        statusLabel.setText ("Current one-shot pack captured as a named sample patch/preset.",
                             juce::dontSendNotification);

        for (auto& existing : project.getPatches())
            existing.isDefault = false;
        for (auto& existing : project.getPresets())
            existing.isDefault = false;

        const int patchIndex = findPatchIndexByIdOrName (project, patch.id, patch.name);
        if (patchIndex >= 0)
            project.getPatches()[(size_t) patchIndex] = patch;
        else
            project.getPatches().push_back (patch);

        const int presetIndex = findPresetIndexByNameOrPatchId (project, preset.name, preset.patchId);
        if (presetIndex >= 0)
            project.getPresets()[(size_t) presetIndex] = preset;
        else
            project.getPresets().push_back (preset);

        project.notifyChanged();

        juce::String saveNote;
        if (project.getProjectFolder().isDirectory())
        {
            juce::String saveError;
            if (! project.save (project.getProjectFolder(), saveError))
            {
                showMessage ("Create Patch From Pack",
                             "Patch was created in memory, but the project did not save:\n" + saveError,
                             juce::MessageBoxIconType::WarningIcon);
                owner.refreshAllPanels();
                return;
            }
            saveNote = "\n\nProject saved.";
        }
        else
        {
            saveNote = "\n\nSave the project to persist this patch on disk.";
        }

        owner.refreshAllPanels();
        appendLog ("Created playable patch: " + patch.name);
        showMessage ("Patch Created",
                     "Created playable sample patch:\n" + patch.name
                     + "\n\nIt is now the default preset and is attached to expansion:\n" + expansion.name
                     + saveNote,
                     juce::MessageBoxIconType::InfoIcon);
    }

    void OneShotMakerPage::publishOneShotPackToPluginClub()
    {
        if (lastPackFolder == juce::File() || ! lastPackFolder.isDirectory() || renderedSamples.empty())
        {
            showMessage ("Publish One Shot Pack",
                         "Render a One Shot Pack before publishing to Plugin.club.",
                         juce::MessageBoxIconType::WarningIcon);
            return;
        }

        juce::String metadataError;
        if (! writeCurrentPackMetadata (metadataError))
        {
            showMessage ("Publish One Shot Pack", metadataError, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        auto readme = lastPackFolder.getChildFile ("README.txt");
        if (! readme.existsAsFile())
        {
            readme.replaceWithText (selectedPackName() + "\n"
                + "Creator: " + selectedCreatorName() + "\n"
                + "Category: " + selectedCategoryName() + "\n"
                + "Source Plugin: " + pluginDescription.name + "\n\n"
                + "Commercial one-shot pack created with PatchCraft Studio.\n");
        }

        juce::String metadataDescription;
        juce::String metadataSourcePlugin = pluginDescription.name;
        auto parsedMetadata = juce::JSON::parse (lastPackFolder.getChildFile ("oneshot_pack.json"));
        if (auto* metadataObject = parsedMetadata.getDynamicObject())
        {
            metadataDescription = metadataObject->getProperty ("description").toString().trim();
            if (metadataObject->getProperty ("sourcePlugin").toString().isNotEmpty())
                metadataSourcePlugin = metadataObject->getProperty ("sourcePlugin").toString();
        }

        PluginClubPublisher::PublishArtifact artifact;
        artifact.kind = PluginClubPublisher::ArtifactKind::OneShotPack;
        artifact.title = selectedPackName();
        artifact.description = metadataDescription.isNotEmpty()
            ? metadataDescription
            : "Commercial one-shot sample pack created from "
                + (metadataSourcePlugin.isNotEmpty() ? metadataSourcePlugin : juce::String ("a hosted instrument"))
                + ". Includes rendered WAV files and PatchCraft one-shot metadata.";
        artifact.creator = selectedCreatorName();
        artifact.category = selectedCategoryName();
        artifact.version = "1.0.0";
        artifact.status = "draft";
        artifact.tags.add ("one-shot");
        artifact.tags.add ("sample-pack");
        artifact.tags.add (selectedCategoryName());
        artifact.formats.add ("WAV");
        artifact.sourcePath = lastPackFolder;
        artifact.artworkFile = artworkFile;

        auto* extra = new juce::DynamicObject();
        extra->setProperty ("sourcePlugin", metadataSourcePlugin);
        extra->setProperty ("template", selectedTemplateName());
        extra->setProperty ("sampleCount", (int) renderedSamples.size());
        extra->setProperty ("bpm", bpmSlider.getValue());
        extra->setProperty ("namingScheme", selectedNamingScheme());
        artifact.extraMetadata = juce::var (extra);

        const auto options = PluginClubPublisher::optionsFromCloudConfig (AiAssistService::loadCloudIntegrationConfig());
        setControlsEnabledForRenderState();
        statusLabel.setText ("Publishing one-shot pack to Plugin.club...", juce::dontSendNotification);
        appendLog ("Publishing one-shot pack to Plugin.club: " + lastPackFolder.getFullPathName());

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        juce::Thread::launch ([safeThis, artifact, options]
        {
            const auto result = PluginClubPublisher::publishArtifact (artifact, options);
            juce::MessageManager::callAsync ([safeThis, result]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->statusLabel.setText (result.success
                                                   ? (result.uploaded ? "Published one-shot draft." : "Prepared Plugin.club package.")
                                                   : "Plugin.club publish failed.",
                                               juce::dontSendNotification);
                safeThis->appendLog (result.message);
                safeThis->showMessage (result.success
                                           ? (result.uploaded ? "Plugin.club Draft Published" : "Plugin.club Package Prepared")
                                           : "Plugin.club Publish Failed",
                                       result.message
                                       + "\n\nArchive:\n" + result.archiveFile.getFullPathName()
                                       + "\n\nPayload:\n" + result.payloadFile.getFullPathName(),
                                       result.success ? juce::MessageBoxIconType::InfoIcon
                                                      : juce::MessageBoxIconType::WarningIcon);
                safeThis->setControlsEnabledForRenderState();
            });
        });
    }

    void OneShotMakerPage::refreshPackLibrary()
    {
        packLibraryEntries.clear();
        selectedPackLibraryIndex = -1;

        scanOneShotPackFolder (outputBaseFolder);
        scanOneShotPackFolder (defaultOutputFolder());
        scanOneShotPackFolder (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("PatchCraft").getChildFile ("OneShotPacks"));

        std::sort (packLibraryEntries.begin(), packLibraryEntries.end(),
                   [] (const PackLibraryEntry& a, const PackLibraryEntry& b)
                   {
                       return a.modified > b.modified;
                   });

        packLibraryList.updateContent();
        packLibraryList.repaint();
    }

    void OneShotMakerPage::scanOneShotPackFolder (const juce::File& folder)
    {
        if (! folder.isDirectory())
            return;

        juce::Array<juce::File> metadataFiles;
        folder.findChildFiles (metadataFiles, juce::File::findFiles, true, "oneshot_pack.json");
        for (const auto& metadataFile : metadataFiles)
        {
            const auto packFolder = metadataFile.getParentDirectory();
            bool alreadyAdded = false;
            for (const auto& entry : packLibraryEntries)
                if (entry.folder == packFolder)
                    alreadyAdded = true;
            if (alreadyAdded)
                continue;

            auto parsed = juce::JSON::parse (metadataFile);
            auto* obj = parsed.getDynamicObject();
            if (obj == nullptr)
                continue;

            PackLibraryEntry entry;
            entry.folder = packFolder;
            entry.name = obj->getProperty ("name").toString();
            if (entry.name.isEmpty())
                entry.name = packFolder.getFileName();
            entry.creator = obj->getProperty ("creator").toString();
            entry.category = obj->getProperty ("category").toString();
            entry.sourcePlugin = obj->getProperty ("sourcePlugin").toString();
            entry.templateName = obj->getProperty ("template").toString();
            entry.modified = metadataFile.getLastModificationTime();

            const auto artwork = obj->getProperty ("artwork").toString();
            if (artwork.isNotEmpty())
                entry.artworkFile = juce::File::isAbsolutePath (artwork)
                    ? juce::File (artwork)
                    : packFolder.getChildFile (artwork);

            if (auto* samples = obj->getProperty ("samples").getArray())
                entry.sampleCount = samples->size();

            packLibraryEntries.push_back (entry);
        }
    }

    bool OneShotMakerPage::loadPackLibraryEntry (int index, bool switchToBuilder)
    {
        if (index < 0 || index >= (int) packLibraryEntries.size())
            return false;

        const auto entry = packLibraryEntries[(size_t) index];
        const auto metadataFile = entry.folder.getChildFile ("oneshot_pack.json");
        auto parsed = juce::JSON::parse (metadataFile);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return false;

        renderedSamples.clear();
        renderedZones.clear();

        if (auto* samples = obj->getProperty ("samples").getArray())
        {
            for (const auto& sampleVar : *samples)
            {
                if (auto* sampleObj = sampleVar.getDynamicObject())
                {
                    RenderedSample sample;
                    sample.midiNote = (int) sampleObj->getProperty ("note");
                    sample.velocity = sampleObj->hasProperty ("velocity") ? (int) sampleObj->getProperty ("velocity") : juce::roundToInt (velocitySlider.getValue());
                    sample.velocityLayerIndex = sampleObj->hasProperty ("velocityLayerIndex") ? (int) sampleObj->getProperty ("velocityLayerIndex") : 0;
                    sample.velocityLayerCount = sampleObj->hasProperty ("velocityLayerCount") ? (int) sampleObj->getProperty ("velocityLayerCount") : 1;
                    sample.roundRobinIndex = sampleObj->hasProperty ("roundRobinIndex") ? (int) sampleObj->getProperty ("roundRobinIndex") : 0;
                    sample.roundRobinCount = sampleObj->hasProperty ("roundRobinCount") ? (int) sampleObj->getProperty ("roundRobinCount") : 1;
                    sample.label = sampleObj->getProperty ("label").toString();
                    const auto fileName = sampleObj->getProperty ("file").toString();
                    sample.file = entry.folder.getChildFile (fileName);
                    if (sample.file.existsAsFile())
                    {
                        renderedSamples.push_back (sample);
                        auto zone = makeZoneForRenderedSample (sample, (int) renderedSamples.size() - 1);
                        zone.samplePath = sample.file.getFullPathName();
                        if (sampleObj->hasProperty ("lowVelocity"))
                            zone.lowVelocity = (int) sampleObj->getProperty ("lowVelocity");
                        if (sampleObj->hasProperty ("highVelocity"))
                            zone.highVelocity = (int) sampleObj->getProperty ("highVelocity");
                        if (sampleObj->hasProperty ("roundRobinGroup"))
                            zone.roundRobinGroup = (int) sampleObj->getProperty ("roundRobinGroup");
                        if (sampleObj->hasProperty ("zoneRoundRobinIndex"))
                            zone.roundRobinIndex = (int) sampleObj->getProperty ("zoneRoundRobinIndex");
                        renderedZones.push_back (zone);
                    }
                }
            }
        }

        if (renderedSamples.empty())
            return false;

        lastPackFolder = entry.folder;
        artworkFile = entry.artworkFile;
        pluginDescription.name = entry.sourcePlugin;
        packNameEditor.setText (entry.name, juce::dontSendNotification);
        creatorEditor.setText (entry.creator, juce::dontSendNotification);
        categoryBox.setText (entry.category.isNotEmpty() ? entry.category : "General", juce::dontSendNotification);
        if (entry.templateName.isNotEmpty())
            templateBox.setText (entry.templateName, juce::dontSendNotification);
        if (obj->hasProperty ("velocityLayerCount"))
            velocityLayersBox.setSelectedId (juce::jlimit (1, 4, (int) obj->getProperty ("velocityLayerCount")), juce::dontSendNotification);
        if (obj->hasProperty ("roundRobinCount"))
            roundRobinBox.setSelectedId (juce::jlimit (1, 4, (int) obj->getProperty ("roundRobinCount")), juce::dontSendNotification);
        populateRenderedSampleBox();
        renderedSampleBox.setSelectedId (1, juce::dontSendNotification);
        loadRenderedSampleForReview (0);
        if (switchToBuilder)
            setViewMode (ViewMode::Builder);
        appendLog ("Loaded one-shot pack from library: " + entry.folder.getFullPathName());
        return true;
    }

    void OneShotMakerPage::openSelectedPackFolder()
    {
        if (selectedPackLibraryIndex < 0 || selectedPackLibraryIndex >= (int) packLibraryEntries.size())
            return;
        packLibraryEntries[(size_t) selectedPackLibraryIndex].folder.revealToUser();
    }

    void OneShotMakerPage::loadSelectedLibraryPackForAudition()
    {
        if (selectedPackLibraryIndex < 0 || selectedPackLibraryIndex >= (int) packLibraryEntries.size())
        {
            showMessage ("Load Pack", "Select a one-shot pack first.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        if (owner.isPreviewActive())
            owner.togglePreview();

        previewSelectedLibraryPack();
        appendLog ("Pack Library audition uses the selected one-shot pack; Design/DSP preview is muted.");
    }

    void OneShotMakerPage::previewSelectedLibraryPack()
    {
        if (selectedPackLibraryIndex < 0 || selectedPackLibraryIndex >= (int) packLibraryEntries.size())
        {
            showMessage ("Preview Pack", "Select a one-shot pack first.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto selectedFolder = packLibraryEntries[(size_t) selectedPackLibraryIndex].folder;
        if (! loadPackLibraryEntry (selectedPackLibraryIndex, false))
        {
            showMessage ("Preview Pack", "Could not load the selected one-shot pack.", juce::MessageBoxIconType::WarningIcon);
            return;
        }

        juce::String description;
        const auto metadataFile = selectedFolder.getChildFile ("oneshot_pack.json");
        auto parsedMetadata = juce::JSON::parse (metadataFile);
        if (auto* object = parsedMetadata.getDynamicObject())
            description = object->getProperty ("description").toString();

        juce::StringArray sampleLabels;
        juce::Array<int> sampleNotes;
        for (const auto& sample : renderedSamples)
        {
            sampleLabels.add (sample.label + "  -  " + sample.file.getFileName());
            if (! sampleNotes.contains (sample.midiNote))
                sampleNotes.add (sample.midiNote);
        }

        sampleNotes.sort();

        class PreviewKeyboard final : public juce::Component
        {
        public:
            std::function<void(int)> onNote;

            void setNotes (const juce::Array<int>& newNotes)
            {
                notes = newNotes;
                repaint();
            }

            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat();
                g.setColour (juce::Colour (0xff090c11));
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (PatchCraftLookAndFeel::border().brighter (0.2f));
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

                if (notes.isEmpty())
                {
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (12.0f));
                    g.drawText ("No samples in this pack.", getLocalBounds(), juce::Justification::centred);
                    return;
                }

                const int first = notes.getFirst();
                const int last = notes.getLast();
                const int count = juce::jmax (1, last - first + 1);
                const float keyW = r.getWidth() / (float) count;

                for (int note = first; note <= last; ++note)
                {
                    const float x = r.getX() + (float) (note - first) * keyW;
                    const bool black = isBlackKey (note);
                    auto key = juce::Rectangle<float> (x, r.getY(), keyW + 0.5f, r.getHeight());
                    g.setColour (black ? juce::Colour (0xff15191f) : juce::Colour (0xffd9caa7));
                    g.fillRect (key);
                    g.setColour (juce::Colour (0xff2a3038));
                    g.drawRect (key, 1.0f);

                    if (notes.contains (note))
                    {
                        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.72f));
                        g.fillRoundedRectangle (key.reduced (3.0f, 8.0f).withY (key.getBottom() - 26.0f).withHeight (18.0f), 4.0f);
                        g.setColour (black ? PatchCraftLookAndFeel::textBright() : juce::Colour (0xff0b0e12));
                        g.setFont (juce::FontOptions (9.5f));
                        g.drawText (noteNameFor (note), key.toNearestInt().reduced (2, 4), juce::Justification::centredBottom);
                    }
                }
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (notes.isEmpty() || onNote == nullptr)
                    return;

                const int first = notes.getFirst();
                const int last = notes.getLast();
                const int count = juce::jmax (1, last - first + 1);
                const int note = juce::jlimit (first, last,
                    first + juce::roundToInt ((float) e.x / (float) juce::jmax (1, getWidth()) * (float) (count - 1)));
                onNote (note);
            }

        private:
            juce::Array<int> notes;

            static bool isBlackKey (int midiNote)
            {
                const int pc = midiNote % 12;
                return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            }

            static juce::String noteNameFor (int midiNote)
            {
                static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                return juce::String (names[midiNote % 12]) + juce::String (midiNote / 12 - 2);
            }
        };

        class PreviewContent final : public juce::Component
        {
        public:
            PreviewContent (juce::String packName,
                            juce::String packCreator,
                            juce::String packCategory,
                            juce::String packDescription,
                            juce::File initialArtwork,
                            juce::StringArray samples,
                            juce::Array<int> notes,
                            std::function<void(int)> loadSample,
                            std::function<void()> playSample,
                            std::function<void()> stopSample,
                            std::function<void(int)> playNote,
                            std::function<bool(const juce::String&, const juce::String&, const juce::String&, const juce::String&, const juce::File&, juce::String&)> saveMetadata)
                : sampleLabels (std::move (samples)),
                  onLoadSample (std::move (loadSample)),
                  onPlaySample (std::move (playSample)),
                  onStopSample (std::move (stopSample)),
                  onPlayNote (std::move (playNote)),
                  onSaveMetadata (std::move (saveMetadata)),
                  artwork (initialArtwork)
            {
                title.setText ("Pack Preview + Metadata", juce::dontSendNotification);
                title.setFont (juce::Font (18.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                setupLabel (nameLabel, "Pack Name");
                setupLabel (creatorLabel, "Creator");
                setupLabel (categoryLabel, "Category");
                setupLabel (descriptionLabel, "Description");
                setupLabel (sampleLabel, "Sample Preview");

                nameEditor.setText (packName, false);
                creatorEditor.setText (packCreator, false);
                descriptionEditor.setMultiLine (true);
                descriptionEditor.setReturnKeyStartsNewLine (true);
                descriptionEditor.setText (packDescription, false);
                for (auto* editor : { &nameEditor, &creatorEditor, &descriptionEditor })
                {
                    editor->setSelectAllWhenFocused (true);
                    addAndMakeVisible (*editor);
                }

                for (const auto& category : { "Keys", "Synth", "Bass", "Drums", "Pads", "FX", "Cinematic", "General" })
                    categoryBox.addItem (category, categoryBox.getNumItems() + 1);
                categoryBox.setText (packCategory.isNotEmpty() ? packCategory : "General", juce::dontSendNotification);
                addAndMakeVisible (categoryBox);

                for (int i = 0; i < sampleLabels.size(); ++i)
                    sampleBox.addItem (sampleLabels[i], i + 1);
                sampleBox.setTextWhenNothingSelected ("Select a sample");
                if (! sampleLabels.isEmpty())
                    sampleBox.setSelectedId (1, juce::dontSendNotification);
                sampleBox.onChange = [this]
                {
                    if (onLoadSample != nullptr)
                        onLoadSample (sampleBox.getSelectedId() - 1);
                };
                addAndMakeVisible (sampleBox);

                for (auto* button : { &playButton, &stopButton, &artworkButton, &saveButton, &closeButton })
                {
                    button->getProperties().set ("smallButton", true);
                    addAndMakeVisible (*button);
                }
                playButton.setButtonText ("Play");
                stopButton.setButtonText ("Stop");
                artworkButton.setButtonText ("Change Artwork");
                saveButton.setButtonText ("Save Metadata");
                closeButton.setButtonText ("Close");
                playButton.setTooltip ("Preview the selected one-shot sample.");
                stopButton.setTooltip ("Stop pack preview playback.");
                artworkButton.setTooltip ("Choose cover artwork for this one-shot pack.");
                saveButton.setTooltip ("Write pack name, creator, category, description, and artwork to oneshot_pack.json.");
                closeButton.setTooltip ("Close the pack preview.");

                playButton.onClick = [this] { if (onPlaySample != nullptr) onPlaySample(); };
                stopButton.onClick = [this] { if (onStopSample != nullptr) onStopSample(); };
                artworkButton.onClick = [this] { chooseArtwork(); };
                saveButton.onClick = [this] { save(); };
                closeButton.onClick = [this]
                {
                    if (onStopSample != nullptr)
                        onStopSample();
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };

                keyboard.onNote = [this] (int midiNote)
                {
                    if (onPlayNote != nullptr)
                        onPlayNote (midiNote);
                };
                keyboard.setNotes (notes);
                addAndMakeVisible (keyboard);

                status.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                status.setFont (juce::FontOptions (11.0f));
                status.setText ("Click keys with orange markers to audition mapped samples.", juce::dontSendNotification);
                addAndMakeVisible (status);

                loadArtworkImage();
                setSize (760, 540);
            }

            ~PreviewContent() override
            {
                if (onStopSample != nullptr)
                    onStopSample();
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto art = artworkBounds.toFloat();
                g.setColour (juce::Colour (0xff111720));
                g.fillRoundedRectangle (art, 10.0f);
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.7f));
                g.drawRoundedRectangle (art.reduced (0.5f), 10.0f, 1.2f);

                if (artworkImage.isValid())
                    g.drawImageWithin (artworkImage, artworkBounds.getX() + 10, artworkBounds.getY() + 10,
                                       artworkBounds.getWidth() - 20, artworkBounds.getHeight() - 20,
                                       juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
                else
                {
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawText ("No Artwork", artworkBounds, juce::Justification::centred);
                }
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (18, 14);
                auto header = area.removeFromTop (32);
                title.setBounds (header.removeFromLeft (360));
                closeButton.setBounds (header.removeFromRight (92).reduced (2));
                area.removeFromTop (12);

                auto top = area.removeFromTop (250);
                artworkBounds = top.removeFromLeft (220);
                top.removeFromLeft (16);
                auto fields = top;

                auto row = fields.removeFromTop (48);
                nameLabel.setBounds (row.removeFromLeft (92));
                nameEditor.setBounds (row);
                fields.removeFromTop (8);

                row = fields.removeFromTop (48);
                creatorLabel.setBounds (row.removeFromLeft (92));
                creatorEditor.setBounds (row);
                fields.removeFromTop (8);

                row = fields.removeFromTop (48);
                categoryLabel.setBounds (row.removeFromLeft (92));
                categoryBox.setBounds (row.removeFromLeft (180));
                row.removeFromLeft (8);
                artworkButton.setBounds (row.removeFromLeft (150).reduced (2, 6));
                row.removeFromLeft (8);
                saveButton.setBounds (row.removeFromLeft (150).reduced (2, 6));
                fields.removeFromTop (8);

                descriptionLabel.setBounds (fields.removeFromTop (18));
                descriptionEditor.setBounds (fields.removeFromTop (82));

                area.removeFromTop (14);
                row = area.removeFromTop (40);
                sampleLabel.setBounds (row.removeFromLeft (112));
                sampleBox.setBounds (row.removeFromLeft (juce::jmax (220, row.getWidth() - 180)));
                row.removeFromLeft (8);
                playButton.setBounds (row.removeFromLeft (74).reduced (2, 4));
                row.removeFromLeft (6);
                stopButton.setBounds (row.removeFromLeft (70).reduced (2, 4));

                area.removeFromTop (10);
                keyboard.setBounds (area.removeFromTop (108));
                area.removeFromTop (8);
                status.setBounds (area.removeFromTop (24));
            }

        private:
            juce::Label title;
            juce::Label nameLabel;
            juce::Label creatorLabel;
            juce::Label categoryLabel;
            juce::Label descriptionLabel;
            juce::Label sampleLabel;
            juce::Label status;
            juce::TextEditor nameEditor;
            juce::TextEditor creatorEditor;
            juce::TextEditor descriptionEditor;
            juce::ComboBox categoryBox;
            juce::ComboBox sampleBox;
            juce::TextButton playButton;
            juce::TextButton stopButton;
            juce::TextButton artworkButton;
            juce::TextButton saveButton;
            juce::TextButton closeButton;
            PreviewKeyboard keyboard;
            juce::Rectangle<int> artworkBounds;
            juce::File artwork;
            juce::Image artworkImage;
            std::unique_ptr<juce::FileChooser> chooser;
            juce::StringArray sampleLabels;
            std::function<void(int)> onLoadSample;
            std::function<void()> onPlaySample;
            std::function<void()> onStopSample;
            std::function<void(int)> onPlayNote;
            std::function<bool(const juce::String&, const juce::String&, const juce::String&, const juce::String&, const juce::File&, juce::String&)> onSaveMetadata;

            static void setupLabel (juce::Label& label, const juce::String& text)
            {
                label.setText (text, juce::dontSendNotification);
                label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                label.setFont (juce::FontOptions (11.0f));
                label.setJustificationType (juce::Justification::centredLeft);
            }

            void loadArtworkImage()
            {
                artworkImage = artwork.existsAsFile() ? juce::ImageCache::getFromFile (artwork) : juce::Image();
                repaint();
            }

            void chooseArtwork()
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose pack artwork",
                                                               juce::File(),
                                                               "*.png;*.jpg;*.jpeg;*.webp");
                juce::Component::SafePointer<PreviewContent> safeThis (this);
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safeThis] (const juce::FileChooser& fc)
                    {
                        if (safeThis == nullptr)
                            return;

                        const auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            safeThis->artwork = file;
                            safeThis->loadArtworkImage();
                            safeThis->status.setText ("Artwork selected. Click Save Metadata to write it into the pack.",
                                                      juce::dontSendNotification);
                        }
                    });
            }

            void save()
            {
                if (onSaveMetadata == nullptr)
                    return;

                juce::String error;
                const bool ok = onSaveMetadata (nameEditor.getText().trim(),
                                                creatorEditor.getText().trim(),
                                                categoryBox.getText().trim(),
                                                descriptionEditor.getText().trim(),
                                                artwork,
                                                error);
                status.setColour (juce::Label::textColourId, ok ? PatchCraftLookAndFeel::accent()
                                                                 : juce::Colour (0xffe6504a));
                status.setText (ok ? "Pack metadata saved." : error, juce::dontSendNotification);
            }
        };

        juce::Component::SafePointer<OneShotMakerPage> safeThis (this);
        auto* content = new PreviewContent (packNameEditor.getText(),
                                            creatorEditor.getText(),
                                            categoryBox.getText(),
                                            description,
                                            artworkFile,
                                            sampleLabels,
                                            sampleNotes,
                                            [safeThis] (int index)
                                            {
                                                if (safeThis != nullptr && index >= 0)
                                                    safeThis->loadRenderedSampleForReview (index);
                                            },
                                            [safeThis]
                                            {
                                                if (safeThis != nullptr)
                                                    safeThis->startReviewPreview();
                                            },
                                            [safeThis]
                                            {
                                                if (safeThis != nullptr)
                                                    safeThis->stopReviewPreview();
                                            },
                                            [safeThis] (int midiNote)
                                            {
                                                if (safeThis != nullptr)
                                                    safeThis->previewLibraryKeyboardNote (midiNote);
                                            },
                                            [safeThis] (const juce::String& name,
                                                        const juce::String& creator,
                                                        const juce::String& category,
                                                        const juce::String& desc,
                                                        const juce::File& art,
                                                        juce::String& error)
                                            {
                                                return safeThis != nullptr
                                                    && safeThis->saveLoadedPackLibraryMetadata (name, creator, category, desc, art, error);
                                            });

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Preview One-Shot Pack";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.content.setOwned (content);
        options.launchAsync();
    }

    bool OneShotMakerPage::saveLoadedPackLibraryMetadata (const juce::String& name,
                                                          const juce::String& creator,
                                                          const juce::String& category,
                                                          const juce::String& description,
                                                          const juce::File& artworkSource,
                                                          juce::String& error)
    {
        if (lastPackFolder == juce::File() || ! lastPackFolder.isDirectory())
        {
            error = "Load a pack before saving metadata.";
            return false;
        }

        const auto metadataFile = lastPackFolder.getChildFile ("oneshot_pack.json");
        auto parsed = juce::JSON::parse (metadataFile);
        auto* object = parsed.getDynamicObject();
        if (object == nullptr)
        {
            error = "Could not read " + metadataFile.getFullPathName();
            return false;
        }

        object->setProperty ("name", name.isNotEmpty() ? name : lastPackFolder.getFileName());
        object->setProperty ("creator", creator);
        object->setProperty ("category", category.isNotEmpty() ? category : juce::String ("General"));
        object->setProperty ("description", description);

        if (artworkSource.existsAsFile())
        {
            auto targetFolder = lastPackFolder.getChildFile ("artwork");
            if (! targetFolder.createDirectory())
            {
                error = "Could not create artwork folder.";
                return false;
            }

            auto target = targetFolder.getChildFile ("cover" + artworkSource.getFileExtension());
            if (artworkSource != target)
            {
                if (target.existsAsFile())
                    target.deleteFile();
                if (! artworkSource.copyFileTo (target))
                {
                    error = "Could not copy artwork into pack.";
                    return false;
                }
            }

            artworkFile = target;
            object->setProperty ("artwork", "artwork/" + target.getFileName());
        }

        if (! metadataFile.replaceWithText (juce::JSON::toString (parsed, true)))
        {
            error = "Could not write " + metadataFile.getFullPathName();
            return false;
        }

        packNameEditor.setText (object->getProperty ("name").toString(), juce::dontSendNotification);
        creatorEditor.setText (object->getProperty ("creator").toString(), juce::dontSendNotification);
        categoryBox.setText (object->getProperty ("category").toString(), juce::dontSendNotification);

        const auto folderToRestore = lastPackFolder;
        refreshPackLibrary();
        for (int i = 0; i < (int) packLibraryEntries.size(); ++i)
        {
            if (packLibraryEntries[(size_t) i].folder == folderToRestore)
            {
                selectedPackLibraryIndex = i;
                packLibraryList.selectRow (i);
                break;
            }
        }

        appendLog ("Saved one-shot pack metadata: " + metadataFile.getFullPathName());
        return true;
    }

    void OneShotMakerPage::previewLibraryKeyboardNote (int midiNote)
    {
        if (renderedSamples.empty())
            return;

        int bestIndex = 0;
        int bestDistance = 999;
        for (int i = 0; i < (int) renderedSamples.size(); ++i)
        {
            const int distance = std::abs (renderedSamples[(size_t) i].midiNote - midiNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        renderedSampleBox.setSelectedId (bestIndex + 1, juce::dontSendNotification);
        loadRenderedSampleForReview (bestIndex);
        startReviewPreview();
    }

    void OneShotMakerPage::publishSelectedLibraryPack()
    {
        if (! loadPackLibraryEntry (selectedPackLibraryIndex, false))
        {
            showMessage ("Publish Pack", "Select a valid one-shot pack first.", juce::MessageBoxIconType::WarningIcon);
            return;
        }
        publishOneShotPackToPluginClub();
    }

    SampleZoneDef OneShotMakerPage::makeZoneForRenderedSample (const RenderedSample& item, int index) const
    {
        SampleZoneDef zone;
        zone.samplePath = item.file.getFullPathName();
        zone.rootNote = item.midiNote;
        zone.lowNote = item.midiNote;
        zone.highNote = item.midiNote;
        const int velocityLayerCount = juce::jlimit (1, 4, item.velocityLayerCount);
        const int velocityLayerIndex = juce::jlimit (0, velocityLayerCount - 1, item.velocityLayerIndex);
        zone.lowVelocity = 1 + (velocityLayerIndex * 127) / velocityLayerCount;
        zone.highVelocity = ((velocityLayerIndex + 1) * 127) / velocityLayerCount;
        if (velocityLayerCount <= 1)
        {
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
        }
        zone.velocityLowerVelXFade = velocityLayerIndex > 0 ? 4.0f : 0.0f;
        zone.velocityUpperVelXFade = velocityLayerIndex + 1 < velocityLayerCount ? 4.0f : 0.0f;
        zone.loopEnabled = false;
        zone.oneShot = true;
        zone.roundRobinGroup = item.roundRobinCount > 1 ? juce::jlimit (1, 127, item.midiNote + 1) : 0;
        zone.roundRobinIndex = item.roundRobinCount > 1 ? item.roundRobinIndex + 1 : 0;
        zone.sampleStart = 0;
        zone.sampleEnd = 0;
        zone.fadeInStart = 0;
        zone.fadeInLength = juce::roundToInt ((fadeInSlider.getValue() / 1000.0) * selectedSampleRate());
        zone.fadeOutStart = 0;
        zone.fadeOutLength = juce::roundToInt ((fadeOutSlider.getValue() / 1000.0) * selectedSampleRate());
        zone.gainDb = 0.0f;
        zone.reverse = false;
        zone.bpm = (float) bpmSlider.getValue();
        zone.group = selectedPackName();
        zone.padLabel = item.label;

        const int padIndex = item.midiNote - midiCForOctave (1);
        if (templateBox.getSelectedId() == 4 && padIndex >= 0 && padIndex < 16)
            zone.padIndex = padIndex;

        return zone;
    }

    void OneShotMakerPage::populateRenderedSampleBox()
    {
        renderedSampleBox.clear (juce::dontSendNotification);
        for (size_t i = 0; i < renderedSamples.size(); ++i)
            renderedSampleBox.addItem (renderedSamples[i].label + "  -  " + renderedSamples[i].file.getFileName(), (int) i + 1);

        const bool hasSamples = ! renderedSamples.empty();
        if (! hasSamples)
        {
            selectedRenderedIndex = -1;
            if (sampleWaveform != nullptr)
                sampleWaveform->clearSampleData();
            sampleEditorStatusLabel.setText ("Render a pack to edit finished one-shots.", juce::dontSendNotification);
        }

        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::loadRenderedSampleForReview (int index)
    {
        if (index < 0 || index >= (int) renderedSamples.size())
            return;

        loadReviewFile (renderedSamples[(size_t) index].file,
                        renderedSamples[(size_t) index].midiNote,
                        renderedSamples[(size_t) index].label,
                        true);
    }

    void OneShotMakerPage::loadReviewFile (const juce::File& file, int midiNote,
                                           const juce::String& label, bool renderedPackSample)
    {
        stopReviewPreview();

        juce::AudioBuffer<float> buffer;
        double sr = 48000.0;
        juce::String error;
        if (! readAudioFile (file, buffer, sr, error))
        {
            sampleEditorStatusLabel.setText ("Could not load review sample: " + error, juce::dontSendNotification);
            return;
        }

        {
            const juce::SpinLock::ScopedLockType lock (reviewLock);
            reviewBuffer = buffer;
            reviewSampleRate = sr;
            reviewReadPosition = 0.0;
            reviewPlayStart = 0;
            reviewPlayEnd = buffer.getNumSamples();
        }

        SampleZoneDef zone;
        if (renderedPackSample)
        {
            const int index = renderedSampleBox.getSelectedId() - 1;
            selectedRenderedIndex = index;
            if (index >= 0 && index < (int) renderedZones.size())
                zone = renderedZones[(size_t) index];
            else
            {
                RenderedSample sample;
                sample.midiNote = midiNote;
                sample.velocity = juce::roundToInt (velocitySlider.getValue());
                sample.label = label;
                sample.file = file;
                zone = makeZoneForRenderedSample (sample, index);
            }
        }
        else
        {
            selectedRenderedIndex = -1;
            RenderedSample sample;
            sample.midiNote = midiNote;
            sample.velocity = juce::roundToInt (velocitySlider.getValue());
            sample.label = label;
            sample.file = file;
            zone = makeZoneForRenderedSample (sample, -1);
        }

        if (zone.sampleEnd <= 0 || zone.sampleEnd > buffer.getNumSamples())
            zone.sampleEnd = buffer.getNumSamples();
        zone.fadeInStart = zone.sampleStart;
        const int zoneLength = juce::jmax (0, zone.sampleEnd - zone.sampleStart);
        zone.fadeInLength = juce::jlimit (0, zoneLength,
                                          zone.fadeInLength > 0
                                              ? zone.fadeInLength
                                              : juce::roundToInt ((fadeInSlider.getValue() / 1000.0) * sr));
        zone.fadeOutLength = juce::jlimit (0, zoneLength,
                                           zone.fadeOutLength > 0
                                               ? zone.fadeOutLength
                                               : juce::roundToInt ((fadeOutSlider.getValue() / 1000.0) * sr));
        zone.fadeOutStart = juce::jmax (zone.sampleStart, zone.sampleEnd - zone.fadeOutLength);

        {
            const juce::SpinLock::ScopedLockType lock (reviewLock);
            reviewZone = zone;
        }

        if (sampleWaveform != nullptr)
        {
            sampleWaveform->setSampleData (buffer, sr);
            sampleWaveform->setZone (zone);
        }
        syncReviewControlsFromZone (zone);

        sampleEditorStatusLabel.setText (label + " loaded for review. Drag purple handles to trim; Shift keeps exact handle positions.",
                                         juce::dontSendNotification);
        setControlsEnabledForRenderState();
    }

    bool OneShotMakerPage::readAudioFile (const juce::File& file, juce::AudioBuffer<float>& buffer,
                                          double& sampleRate, juce::String& error)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor (file));
        if (reader == nullptr)
        {
            error = "Unsupported or missing audio file: " + file.getFullPathName();
            return false;
        }

        if (reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64) std::numeric_limits<int>::max())
        {
            error = "Invalid sample length.";
            return false;
        }

        buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        if (! reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true))
        {
            error = "Failed reading " + file.getFileName();
            return false;
        }

        sampleRate = reader->sampleRate;
        return true;
    }

    void OneShotMakerPage::updateReviewZoneFromWaveform()
    {
        if (sampleWaveform == nullptr)
            return;

        auto zone = sampleWaveform->getZone();
        zone.gainDb = (float) editGainSlider.getValue();
        zone.reverse = editReverseToggle.getToggleState();
        zone.fadeInStart = zone.sampleStart;
        const int zoneLength = juce::jmax (0, zone.sampleEnd - zone.sampleStart);
        zone.fadeInLength = juce::jlimit (0, zoneLength,
                                          juce::roundToInt ((fadeInSlider.getValue() / 1000.0) * reviewSampleRate));
        zone.fadeOutLength = juce::jlimit (0, zoneLength,
                                           juce::roundToInt ((fadeOutSlider.getValue() / 1000.0) * reviewSampleRate));
        zone.fadeOutStart = juce::jmax (zone.sampleStart, zone.sampleEnd - zone.fadeOutLength);

        if (selectedRenderedIndex >= 0 && selectedRenderedIndex < (int) renderedZones.size())
            renderedZones[(size_t) selectedRenderedIndex] = zone;

        {
            const juce::SpinLock::ScopedLockType lock (reviewLock);
            reviewZone = zone;
        }

        sampleEditorStatusLabel.setText ("Edit captured: start " + juce::String (zone.sampleStart)
                                         + ", end " + juce::String (zone.sampleEnd)
                                         + ", gain " + juce::String (zone.gainDb, 1) + " dB",
                                         juce::dontSendNotification);
    }

    void OneShotMakerPage::syncReviewControlsFromZone (const SampleZoneDef& zone)
    {
        editGainSlider.setValue (zone.gainDb, juce::dontSendNotification);
        if (reviewSampleRate > 0.0)
        {
            fadeInSlider.setValue ((double) zone.fadeInLength * 1000.0 / reviewSampleRate, juce::dontSendNotification);
            fadeOutSlider.setValue ((double) zone.fadeOutLength * 1000.0 / reviewSampleRate, juce::dontSendNotification);
        }
        editReverseToggle.setToggleState (zone.reverse, juce::dontSendNotification);
    }

    void OneShotMakerPage::startReviewPreview()
    {
        {
            const juce::SpinLock::ScopedLockType lock (reviewLock);
            if (reviewBuffer.getNumSamples() <= 0)
                return;

            const auto zone = reviewZone;

            reviewPlayStart = juce::jlimit (0, reviewBuffer.getNumSamples() - 1, zone.sampleStart);
            reviewPlayEnd = zone.sampleEnd > reviewPlayStart
                ? juce::jlimit (reviewPlayStart + 1, reviewBuffer.getNumSamples(), zone.sampleEnd)
                : reviewBuffer.getNumSamples();
            reviewReadPosition = (double) reviewPlayStart;
            reviewPreviewFinished.store (false);
            reviewPreviewPlaying.store (true);
        }

        if (! reviewCallbackActive)
        {
            ensureSharedAudioCallback();
            reviewCallbackActive = true;
        }

        sampleEditorStatusLabel.setText ("Previewing one-shot.", juce::dontSendNotification);
        startTimerHz (20);
        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::stopReviewPreview()
    {
        reviewPreviewPlaying.store (false);
        reviewPreviewFinished.store (false);

        if (reviewCallbackActive)
        {
            reviewCallbackActive = false;
        }

        releaseSharedAudioCallbackIfIdle();
        setControlsEnabledForRenderState();
    }

    void OneShotMakerPage::appendLog (const juce::String& text)
    {
        renderLog.moveCaretToEnd();
        renderLog.insertTextAtCaret (text + "\n");
    }

    void OneShotMakerPage::showMessage (const juce::String& titleText, const juce::String& message,
                                        juce::MessageBoxIconType icon)
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle (titleText)
                .withMessage (message)
                .withButton ("OK")
                .withIconType (icon),
            nullptr);
    }

    void OneShotMakerPage::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        reviewOutputSampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
        if (livePluginCallbackActive && pluginInstance != nullptr)
            prepareLivePluginForDevice (device);
    }

    void OneShotMakerPage::audioDeviceStopped()
    {
        reviewPreviewPlaying.store (false);
        reviewPreviewFinished.store (true);
        hardwareCaptureActive.store (false);
        hardwareCaptureWritePosition.store (0);
        livePluginPrepared.store (false);
        {
            const juce::ScopedLock midiGuard (liveMidiLock);
            liveMidiBuffer.clear();
        }
        if (pluginInstance != nullptr)
        {
            const juce::SpinLock::ScopedLockType lock (pluginProcessLock);
            pluginInstance->reset();
        }
    }

    void OneShotMakerPage::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        if (! livePluginCallbackActive)
            return;

        const juce::ScopedLock midiGuard (liveMidiLock);
        liveMidiBuffer.addEvent (message, 0);
    }

    void OneShotMakerPage::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                             float* const* outputChannelData,
                                                             int numOutputChannels,
                                                             int numSamples,
                                                             const juce::AudioIODeviceCallbackContext&)
    {
        juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
        output.clear();

        if (hardwareCaptureActive.load())
        {
            const juce::ScopedLock lock (hardwareCaptureLock);
            const int writePos = hardwareCaptureWritePosition.load();
            const int writable = juce::jmin (numSamples, hardwareCaptureBuffer.getNumSamples() - writePos);
            if (writable > 0)
            {
                for (int ch = 0; ch < hardwareCaptureBuffer.getNumChannels(); ++ch)
                {
                    const int srcCh = juce::jmin (ch, juce::jmax (0, numInputChannels - 1));
                    if (inputChannelData != nullptr && numInputChannels > 0 && inputChannelData[srcCh] != nullptr)
                        hardwareCaptureBuffer.copyFrom (ch, writePos, inputChannelData[srcCh], writable);
                    else
                        hardwareCaptureBuffer.clear (ch, writePos, writable);
                }
                hardwareCaptureWritePosition.store (writePos + writable);
            }
        }

        if (livePluginCallbackActive && pluginInstance != nullptr)
        {
            const juce::SpinLock::ScopedTryLockType processLock (pluginProcessLock);
            if (processLock.isLocked())
            {
                const int pluginChannels = juce::jmax (2, juce::jmax (numOutputChannels,
                                                                       pluginInstance->getTotalNumOutputChannels()));
                if (pluginProcessBuffer.getNumChannels() < pluginChannels
                    || pluginProcessBuffer.getNumSamples() < numSamples)
                    pluginProcessBuffer.setSize (pluginChannels, numSamples, false, false, true);

                pluginProcessBuffer.clear();

                juce::MidiBuffer blockMidi;
                {
                    const juce::ScopedLock midiGuard (liveMidiLock);
                    blockMidi.addEvents (liveMidiBuffer, 0, numSamples, 0);
                    liveMidiBuffer.clear();
                }

                if (! livePluginPrepared.load())
                {
                    pluginInstance->prepareToPlay (livePluginSampleRate, livePluginBlockSize);
                    livePluginPrepared.store (true);
                }

                pluginInstance->processBlock (pluginProcessBuffer, blockMidi);

                float peak = 0.0f;
                for (int ch = 0; ch < numOutputChannels; ++ch)
                {
                    const int srcCh = juce::jmin (ch, pluginProcessBuffer.getNumChannels() - 1);
                    output.addFrom (ch, 0, pluginProcessBuffer, srcCh, 0, numSamples);
                    peak = juce::jmax (peak, output.getMagnitude (ch, 0, numSamples));
                }
                livePluginPeak.store (peak);
            }
        }

        const juce::SpinLock::ScopedTryLockType reviewStateLock (reviewLock);
        if (! reviewStateLock.isLocked() || ! reviewPreviewPlaying.load() || reviewBuffer.getNumSamples() <= 0)
            return;

        const double ratio = reviewSampleRate / juce::jmax (1.0, reviewOutputSampleRate);
        const auto zone = reviewZone;

        const float gain = juce::Decibels::decibelsToGain (zone.gainDb);
        for (int i = 0; i < numSamples; ++i)
        {
            const int src = juce::roundToInt (reviewReadPosition);
            if (src >= reviewPlayEnd || src < 0)
            {
                reviewPreviewPlaying.store (false);
                reviewPreviewFinished.store (true);
                break;
            }

            const int readSrc = zone.reverse ? (reviewPlayEnd - 1 - (src - reviewPlayStart)) : src;
            float envelope = 1.0f;
            if (zone.fadeInLength > 0 && src < zone.fadeInStart + zone.fadeInLength)
                envelope *= juce::jlimit (0.0f, 1.0f,
                                          (float) (src - zone.fadeInStart) / (float) zone.fadeInLength);
            if (zone.fadeOutLength > 0 && src >= zone.fadeOutStart)
                envelope *= juce::jlimit (0.0f, 1.0f,
                                          (float) (zone.sampleEnd - src) / (float) zone.fadeOutLength);

            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                const int srcCh = juce::jmin (ch, reviewBuffer.getNumChannels() - 1);
                output.addSample (ch, i, reviewBuffer.getSample (srcCh, readSrc) * gain * envelope);
            }

            reviewReadPosition += ratio;
        }
    }

    void OneShotMakerPage::timerCallback()
    {
        syncPluginEditorHostSize();

        if (reviewPreviewFinished.load())
        {
            stopReviewPreview();
            sampleEditorStatusLabel.setText ("Preview finished.", juce::dontSendNotification);
        }

        if (! rendering.load())
        {
            if (livePluginCallbackActive)
                pluginEditorStatusLabel.setText ("Live monitor on. Studio Preview muted. Output peak "
                                                 + juce::String (juce::Decibels::gainToDecibels (livePluginPeak.load(), -100.0f), 1)
                                                 + " dB. Hardware MIDI is routed to the loaded VST only.",
                                                 juce::dontSendNotification);

            if (! reviewCallbackActive && ! livePluginCallbackActive)
                stopTimer();
            return;
        }

        statusLabel.setText ("Rendering... " + formatPercent (renderProgress.load()), juce::dontSendNotification);
        repaint (progressBounds);
    }

    void OneShotMakerPage::refresh()
    {
        if (outputBaseFolder == juce::File())
            outputBaseFolder = defaultOutputFolder();
        outputPathLabel.setText (outputBaseFolder.getFullPathName(), juce::dontSendNotification);
        updateRenderPlan();
    }

    void OneShotMakerPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());
        if (viewMode == ViewMode::Library)
        {
            drawCard (g, libraryCard, PatchCraftLookAndFeel::accent());
            if (packLibraryEntries.empty())
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (15.0f));
                g.drawText ("No one-shot packs found. Render a pack or choose an output folder under Builder.",
                            libraryCard.reduced (24), juce::Justification::centred);
            }
        }
        else
        {
            drawCard (g, pluginCard, PatchCraftLookAndFeel::accent());
            drawCard (g, settingsCard, juce::Colour (0xff57b6ff));
            drawCard (g, exportCard, juce::Colour (0xff7edc92));
            drawCard (g, pluginEditorCard, juce::Colour (0xffb87cff));
            drawCard (g, sampleEditorCard, PatchCraftLookAndFeel::accentDim());
        }

        if (! progressBounds.isEmpty())
        {
            auto r = progressBounds.toFloat();
            g.setColour (PatchCraftLookAndFeel::raised());
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * renderProgress.load()), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        }
    }

    void OneShotMakerPage::resized()
    {
        auto area = getLocalBounds().reduced (24, 18);

        auto header = area.removeFromTop (58);
        title.setBounds (header.removeFromTop (30));
        subtitle.setBounds (header);
        area.removeFromTop (12);

        auto viewTabs = area.removeFromTop (34);
        builderViewButton.setBounds (viewTabs.removeFromLeft (120));
        viewTabs.removeFromLeft (8);
        libraryViewButton.setBounds (viewTabs.removeFromLeft (140));
        area.removeFromTop (12);

        if (viewMode == ViewMode::Library)
        {
            libraryCard = area;
            auto library = libraryCard.reduced (18, 16);
            auto controls = library.removeFromTop (36);
            refreshLibraryButton.setBounds (controls.removeFromLeft (130));
            controls.removeFromLeft (8);
            loadLibraryPackButton.setBounds (controls.removeFromLeft (110));
            controls.removeFromLeft (8);
            previewLibraryPackButton.setBounds (controls.removeFromLeft (130));
            controls.removeFromLeft (8);
            publishLibraryPackButton.setBounds (controls.removeFromLeft (120));
            controls.removeFromLeft (8);
            openLibraryPackButton.setBounds (controls.removeFromLeft (110));
            library.removeFromTop (10);
            packLibraryList.setBounds (library);
            pluginCard = settingsCard = exportCard = pluginEditorCard = sampleEditorCard = {};
            return;
        }

        libraryCard = {};

        auto topRow = area.removeFromTop (juce::jlimit (380, 430, juce::roundToInt ((float) area.getHeight() * 0.55f)));
        pluginCard = topRow.removeFromLeft (juce::jlimit (310, 380, topRow.getWidth() / 4));
        topRow.removeFromLeft (14);
        settingsCard = topRow.removeFromLeft (juce::jlimit (360, 460, topRow.getWidth() / 3));
        topRow.removeFromLeft (14);
        exportCard = topRow;

        area.removeFromTop (14);
        auto bottom = area;
        pluginEditorCard = bottom.removeFromLeft (juce::roundToInt ((float) bottom.getWidth() * 0.58f));
        bottom.removeFromLeft (14);
        sampleEditorCard = bottom;

        auto p = pluginCard.reduced (18, 16);
        pluginSectionTitle.setBounds (p.removeFromTop (24));
        p.removeFromTop (10);
        importPluginButton.setBounds (p.removeFromTop (34));
        p.removeFromTop (12);
        pluginPathLabel.setBounds (p.removeFromTop (58));
        pluginStatusLabel.setBounds (p.removeFromTop (54));
        p.removeFromTop (8);
        auto pluginButtons = p.removeFromTop (34);
        livePluginToggle.setBounds (pluginButtons.removeFromLeft (112));
        pluginButtons.removeFromLeft (8);
        auditionPluginButton.setBounds (pluginButtons);
        p.removeFromTop (8);
        auto hardwareRow = p.removeFromTop (28);
        hardwareCaptureToggle.setBounds (hardwareRow.removeFromLeft (132));
        hardwareRow.removeFromLeft (8);
        hardwareMidiOutputLabel.setBounds (hardwareRow.removeFromLeft (58));
        hardwareMidiOutputBox.setBounds (hardwareRow);

        auto s = settingsCard.reduced (18, 16);
        settingsSectionTitle.setBounds (s.removeFromTop (24));
        s.removeFromTop (6);

        auto row = s.removeFromTop (44);
        auto left = row.removeFromLeft (row.getWidth() / 2 - 6);
        auto right = row;
        packNameLabel.setBounds (left.removeFromTop (16));
        packNameEditor.setBounds (left.removeFromTop (28));
        templateLabel.setBounds (right.removeFromTop (16));
        templateBox.setBounds (right.removeFromTop (28));

        s.removeFromTop (6);
        row = s.removeFromTop (44);
        left = row.removeFromLeft (row.getWidth() / 2 - 6);
        right = row;
        barLengthLabel.setBounds (left.removeFromTop (16));
        barLengthBox.setBounds (left.removeFromTop (28));
        sampleRateLabel.setBounds (right.removeFromTop (16));
        sampleRateBox.setBounds (right.removeFromTop (28));

        s.removeFromTop (6);
        row = s.removeFromTop (30);
        bpmLabel.setBounds (row.removeFromLeft (70));
        bpmSlider.setBounds (row);
        row = s.removeFromTop (30);
        velocityLabel.setBounds (row.removeFromLeft (70));
        velocitySlider.setBounds (row);
        row = s.removeFromTop (42);
        left = row.removeFromLeft (row.getWidth() / 2 - 6);
        right = row;
        velocityLayersLabel.setBounds (left.removeFromTop (16));
        velocityLayersBox.setBounds (left.removeFromTop (24));
        roundRobinLabel.setBounds (right.removeFromTop (16));
        roundRobinBox.setBounds (right.removeFromTop (24));
        row = s.removeFromTop (30);
        tailLabel.setBounds (row.removeFromLeft (70));
        tailSlider.setBounds (row);
        row = s.removeFromTop (30);
        fadeInLabel.setBounds (row.removeFromLeft (70));
        fadeInSlider.setBounds (row);
        row = s.removeFromTop (30);
        fadeOutLabel.setBounds (row.removeFromLeft (70));
        fadeOutSlider.setBounds (row);

        auto e = exportCard.reduced (18, 16);
        exportSectionTitle.setBounds (e.removeFromTop (24));
        e.removeFromTop (8);

        row = e.removeFromTop (48);
        const int metadataGap = 6;
        const int metadataWidth = juce::jmax (110, (row.getWidth() - metadataGap * 3) / 4);
        auto metadataCol = row.removeFromLeft (metadataWidth);
        creatorLabel.setBounds (metadataCol.removeFromTop (16));
        creatorEditor.setBounds (metadataCol.removeFromTop (28));
        row.removeFromLeft (metadataGap);
        metadataCol = row.removeFromLeft (metadataWidth);
        categoryLabel.setBounds (metadataCol.removeFromTop (16));
        categoryBox.setBounds (metadataCol.removeFromTop (28));
        row.removeFromLeft (metadataGap);
        metadataCol = row.removeFromLeft (metadataWidth);
        namingLabel.setBounds (metadataCol.removeFromTop (16));
        namingSchemeBox.setBounds (metadataCol.removeFromTop (28));
        row.removeFromLeft (metadataGap);
        chooseArtworkButton.setBounds (row.removeFromTop (28).translated (0, 16));

        e.removeFromTop (6);
        planLabel.setBounds (e.removeFromTop (24));
        e.removeFromTop (4);
        auto outputRow = e.removeFromTop (34);
        chooseOutputButton.setBounds (outputRow.removeFromLeft (150));
        outputRow.removeFromLeft (8);
        outputPathLabel.setBounds (outputRow);
        e.removeFromTop (6);
        auto toggles = e.removeFromTop (28);
        normalizeToggle.setBounds (toggles.removeFromLeft (150));
        trimStartToggle.setBounds (toggles.removeFromLeft (170));
        replaceMapperToggle.setBounds (toggles);
        e.removeFromTop (6);
        auto actionRow = e.removeFromTop (36);
        const int actionGap = 6;
        const int actionWidth = juce::jmax (110, (actionRow.getWidth() - actionGap * 4) / 5);
        renderButton.setBounds (actionRow.removeFromLeft (actionWidth));
        actionRow.removeFromLeft (actionGap);
        sendToMapperButton.setBounds (actionRow.removeFromLeft (actionWidth));
        actionRow.removeFromLeft (actionGap);
        createPatchButton.setBounds (actionRow.removeFromLeft (actionWidth));
        actionRow.removeFromLeft (actionGap);
        buildBundleButton.setBounds (actionRow.removeFromLeft (actionWidth));
        actionRow.removeFromLeft (actionGap);
        publishPackButton.setBounds (actionRow);
        e.removeFromTop (8);
        auto logHeader = e.removeFromTop (24);
        planSectionTitle.setBounds (logHeader.removeFromLeft (120));
        statusLabel.setBounds (logHeader.removeFromRight (210));
        progressBounds = logHeader.reduced (8, 7);
        renderLog.setBounds (e);

        auto pe = pluginEditorCard.reduced (16, 14);
        auto peHeader = pe.removeFromTop (30);
        auto peActions = peHeader.removeFromRight (juce::jmin (330, peHeader.getWidth() / 2));
        pluginEditorTitle.setBounds (peHeader.removeFromLeft (116));
        pluginEditorStatusLabel.setBounds (peHeader);
        refreshPluginEditorButton.setBounds (peActions.removeFromLeft (108));
        peActions.removeFromLeft (6);
        floatPluginEditorButton.setBounds (peActions.removeFromLeft (98));
        peActions.removeFromLeft (6);
        closePluginEditorButton.setBounds (peActions);
        pe.removeFromTop (8);
        pluginEditorViewport.setBounds (pe);
        syncPluginEditorHostSize();

        auto se = sampleEditorCard.reduced (16, 14);
        auto seHeader = se.removeFromTop (30);
        sampleEditorTitle.setBounds (seHeader.removeFromLeft (160));
        sampleEditorStatusLabel.setBounds (seHeader);
        se.removeFromTop (8);

        auto sampleRow = se.removeFromTop (48);
        renderedSampleLabel.setBounds (sampleRow.removeFromLeft (94));
        renderedSampleBox.setBounds (sampleRow.removeFromLeft (juce::jmax (190, sampleRow.getWidth() - 210)));
        sampleRow.removeFromLeft (8);
        previewSampleButton.setBounds (sampleRow.removeFromLeft (120));
        sampleRow.removeFromLeft (6);
        stopSampleButton.setBounds (sampleRow.removeFromLeft (58));

        auto editRow = se.removeFromTop (36);
        editGainLabel.setBounds (editRow.removeFromLeft (54));
        editGainSlider.setBounds (editRow.removeFromLeft (juce::jmax (180, editRow.getWidth() - 110)));
        editRow.removeFromLeft (8);
        editReverseToggle.setBounds (editRow);

        se.removeFromTop (8);
        if (sampleWaveform != nullptr)
            sampleWaveform->setBounds (se);
    }

    int OneShotMakerPage::getNumRows()
    {
        return (int) packLibraryEntries.size();
    }

    void OneShotMakerPage::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= (int) packLibraryEntries.size())
            return;

        const auto& entry = packLibraryEntries[(size_t) rowNumber];
        auto row = juce::Rectangle<int> (0, 0, width, height).reduced (8, 6);
        g.setColour (rowIsSelected ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (row.toFloat(), 10.0f);
        g.setColour (rowIsSelected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 10.0f, rowIsSelected ? 1.6f : 1.0f);

        auto art = row.removeFromLeft (70).reduced (6);
        if (entry.artworkFile.existsAsFile())
        {
            auto image = juce::ImageFileFormat::loadFrom (entry.artworkFile);
            if (image.isValid())
                g.drawImage (image, art.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.16f));
            g.fillRoundedRectangle (art.toFloat(), 8.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (22.0f, juce::Font::bold));
            g.drawText ("P", art, juce::Justification::centred);
        }

        row.removeFromLeft (6);
        auto meta = row;
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawText (entry.name, meta.removeFromTop (22), juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f));
        const auto line = (entry.creator.isNotEmpty() ? entry.creator : juce::String ("Unknown creator"))
            + "  •  " + (entry.category.isNotEmpty() ? entry.category : juce::String ("Uncategorized"))
            + "  •  " + juce::String (entry.sampleCount) + " samples";
        g.drawText (line, meta.removeFromTop (18), juce::Justification::centredLeft);

        const auto source = (entry.sourcePlugin.isNotEmpty() ? entry.sourcePlugin : juce::String ("Rendered audio"))
            + (entry.templateName.isNotEmpty() ? "  •  " + entry.templateName : juce::String());
        g.drawText (source, meta.removeFromTop (18), juce::Justification::centredLeft);
        g.drawText (entry.folder.getFullPathName(), meta, juce::Justification::centredLeft);
    }

    void OneShotMakerPage::selectedRowsChanged (int lastRowSelected)
    {
        selectedPackLibraryIndex = lastRowSelected;
        const bool hasSelection = selectedPackLibraryIndex >= 0 && selectedPackLibraryIndex < (int) packLibraryEntries.size();
        openLibraryPackButton.setEnabled (hasSelection);
        loadLibraryPackButton.setEnabled (hasSelection);
        previewLibraryPackButton.setEnabled (hasSelection);
        publishLibraryPackButton.setEnabled (hasSelection);
    }

    juce::String OneShotMakerPage::noteName (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int note = juce::jlimit (0, 127, midiNote);
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    int OneShotMakerPage::midiCForOctave (int octave)
    {
        return juce::jlimit (0, 127, (octave + 2) * 12);
    }

    juce::String OneShotMakerPage::legalStem (juce::String text)
    {
        text = text.trim();
        juce::String out;
        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_')
                out += juce::String::charToString (c);
            else if (c == ' ' || c == '.' || c == '#')
                out += "_";
        }

        while (out.contains ("__"))
            out = out.replace ("__", "_");

        out = out.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
        return out.isNotEmpty() ? out.substring (0, 72) : juce::String ("OneShotPack");
    }
}
