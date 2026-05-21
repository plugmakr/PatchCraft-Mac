#include "WorkflowPage.h"
#include "StudioMainComponent.h"
#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        static juce::String engineDisplayName (const juce::String& engine)
        {
            if (engine == "synth") return "Synth";
            if (engine == "fx") return "FX";
            if (engine == "sample") return "Sample";
            return engine.isNotEmpty() ? engine : "Unknown";
        }

        static int countRuntimeControls (const PatchCraftProject& project, bool bound)
        {
            int count = 0;
            for (const auto& element : project.getLayout().getAll())
                if (isRuntimeControlElement (element.type)
                    && element.type != ElementType::Dropdown
                    && element.parameterId.isNotEmpty() == bound)
                    ++count;
            return count;
        }

        static int countBlocksInSection (const PatchCraftProject& project, const juce::String& section)
        {
            int count = 0;
            for (const auto& block : project.getDspGraph().blocks)
                if (block.section == section)
                    ++count;
            return count;
        }

        static juce::Array<juce::File> factoryDemoSearchRoots()
        {
            juce::Array<juce::File> roots;
            juce::StringArray seenPaths;
            auto add = [&] (const juce::File& folder)
            {
                const auto key = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
                if (folder.isDirectory() && ! seenPaths.contains (key))
                {
                    roots.add (folder);
                    seenPaths.add (key);
                }
            };

            const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            const auto appDir = app.isDirectory() ? app : app.getParentDirectory();
            add (appDir.getChildFile ("FactoryDemos"));

            const auto cwd = juce::File::getCurrentWorkingDirectory();
            add (cwd.getChildFile ("FactoryDemos"));
            add (cwd.getChildFile ("Examples").getChildFile ("FactoryDemos"));
            return roots;
        }
    }

    WorkflowPage::WorkflowPage (StudioMainComponent& o) : owner (o)
    {
        title.setText ("PatchCraft Command Center", juce::dontSendNotification);
        title.setFont (juce::Font (27.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (title);

        subtitle.setText ("A guided path from sound idea to sellable instrument: sound, performance, UI, presets, runtime proof, export.",
                          juce::dontSendNotification);
        subtitle.setFont (juce::Font (13.0f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        styleCardLabel (productTitle, 16.0f, true, PatchCraftLookAndFeel::textBright());
        productTitle.setText ("Product Path", juce::dontSendNotification);
        addAndMakeVisible (productTitle);

        styleCardLabel (productBody, 12.0f, false, PatchCraftLookAndFeel::textDim());
        productBody.setText ("Pick the kind of product first. PatchCraft then keeps DSP, samples, MIDI, design, presets, and export pointed at one playable Patch.",
                             juce::dontSendNotification);
        addAndMakeVisible (productBody);

        styleCardLabel (pathTitle, 16.0f, true, PatchCraftLookAndFeel::textBright());
        pathTitle.setText ("Guided Launch Path", juce::dontSendNotification);
        addAndMakeVisible (pathTitle);

        styleCardLabel (pathBody, 12.0f, false, PatchCraftLookAndFeel::textDim());
        pathBody.setText ("Every step has a concrete output. If the output is not real, the instrument is not ready to ship.",
                          juce::dontSendNotification);
        addAndMakeVisible (pathBody);

        fullTutorialButton.setButtonText ("Build Full Demo Instrument");
        stylePrimary (fullTutorialButton);
        fullTutorialButton.onClick = [this] { showModuleTutorial (TutorialModule::FullDemo); };
        addAndMakeVisible (fullTutorialButton);

        styleCardLabel (truthTitle, 16.0f, true, PatchCraftLookAndFeel::textBright());
        truthTitle.setText ("Always Keep One Source of Truth", juce::dontSendNotification);
        addAndMakeVisible (truthTitle);

        styleCardLabel (truthBody, 12.0f, false, PatchCraftLookAndFeel::textDim());
        truthBody.setText ("The Patch is the source of truth. Design only controls it. Test and Export must load that same patch, not a separate mockup.",
                           juce::dontSendNotification);
        addAndMakeVisible (truthBody);

        advancedMode.setButtonText ("Advanced Explorer");
        advancedMode.setTooltip ("Guided mode shows the product-building path. Advanced mode exposes direct access to the deep editors.");
        advancedMode.onClick = [this] { updateModeVisibility(); repaint(); };
        addAndMakeVisible (advancedMode);

        healthTitle.setText ("Runtime Readiness", juce::dontSendNotification);
        healthTitle.setFont (juce::Font (15.0f, juce::Font::bold));
        healthTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (healthTitle);

        healthBody.setFont (juce::Font (12.0f));
        healthBody.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        healthBody.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (healthBody);

        stylePrimary (healthCheckButton);
        healthCheckButton.onClick = [this] { showHealthDialog(); };
        addAndMakeVisible (healthCheckButton);

        for (auto* button : { &synthButton, &sampleButton, &drumButton, &fxButton })
        {
            styleSecondary (*button);
            button->getProperties().set ("workflowProduct", true);
            button->getProperties().set ("headlineSize", 12.2);
            button->getProperties().set ("detailSize", 10.5);
            addAndMakeVisible (*button);
        }
        synthButton.setButtonText ("Synth Instrument\nOscillators, wavetables, modulation");
        sampleButton.setButtonText ("Sample Instrument\nKeyzones, velocity layers, playback");
        drumButton.setButtonText ("Drum Machine\nPads, patterns, sample performance");
        fxButton.setButtonText ("FX Plugin\nLive input, throws, EQ, movement");
        synthButton.onClick = [this] { switchTemplate ("synth", "Synth Instrument"); };
        sampleButton.onClick = [this] { switchTemplate ("sample", "Sample Instrument"); };
        drumButton.onClick = [this] { switchTemplate ("drum", "Drum Machine"); };
        fxButton.onClick = [this] { switchTemplate ("fx", "FX Plugin"); };

        styleCardLabel (factoryDemoLabel, 12.0f, true, PatchCraftLookAndFeel::accent());
        factoryDemoLabel.setText ("Ship-Ready Starting Points", juce::dontSendNotification);
        addAndMakeVisible (factoryDemoLabel);

        factoryDemoBox.setTextWhenNothingSelected ("Choose a shipped demo...");
        factoryDemoBox.setTooltip ("Load a complete PatchCraft factory demo with artwork, layout, presets, DSP graph, and samples where needed.");
        addAndMakeVisible (factoryDemoBox);

        stylePrimary (loadFactoryDemoButton);
        loadFactoryDemoButton.setTooltip ("Loads the selected factory demo as the current editable PatchCraft instrument.");
        loadFactoryDemoButton.onClick = [this] { loadSelectedFactoryDemo(); };
        addAndMakeVisible (loadFactoryDemoButton);

        for (auto* button : { &soundButton, &midiButton, &designButton, &presetsButton, &testButton, &exportButton })
        {
            stylePrimary (*button);
            button->getProperties().set ("workflowStep", true);
            button->getProperties().set ("primaryAction", false);
            addAndMakeVisible (*button);
        }

        soundButton.setButtonText ("1  Build Sound\nCreate the real playable source and routing");
        midiButton.setButtonText ("2  Perform\nArps, chords, sample control, motion, MIDI");
        designButton.setButtonText ("3  Design Player\nBuild the customer-facing interface");
        presetsButton.setButtonText ("4  Package Presets\nSave full patches and organize expansions");
        testButton.setButtonText ("5  Prove Runtime\nTest the exact exported Player behavior");
        exportButton.setButtonText ("6  Launch\nExport pack, VST3, or Plugin.club draft");

        soundButton.setTooltip ("Start with the actual sound source: DSP graph for synth/FX, Sample Mapper for samples/drums.");
        midiButton.setTooltip ("Create arps, chords, patterns, sample control, and performance MIDI.");
        designButton.setTooltip ("Build the Player UI and bind knobs/sliders/buttons to real parameters.");
        presetsButton.setTooltip ("Save playable patches and organize them into sellable expansion packs.");
        testButton.setTooltip ("Open the runtime Player surface and verify sound, UI interactions, MIDI, and presets.");
        exportButton.setTooltip ("Export a .patchcraft pack or a standalone VST3 bundle.");

        soundButton.onClick = [this] { showModuleTutorial (TutorialModule::BuildSound); };
        midiButton.onClick = [this] { showModuleTutorial (TutorialModule::MidiPerformance); };
        designButton.onClick = [this] { showModuleTutorial (TutorialModule::DesignPlayer); };
        presetsButton.onClick = [this] { showModuleTutorial (TutorialModule::PresetsPacks); };
        testButton.onClick = [this] { showModuleTutorial (TutorialModule::TestRuntime); };
        exportButton.onClick = [this] { showModuleTutorial (TutorialModule::Export); };

        for (auto* button : { &advDspButton, &advMapperButton, &advOneShotButton, &advMidiButton,
                              &advBuildButton, &advDesignButton, &advBrandButton })
        {
            styleSecondary (*button);
            button->getProperties().set ("workflowStep", true);
            addAndMakeVisible (*button);
        }
        advDspButton.setButtonText ("DSP Builder\nAdvanced sound graph and modulation");
        advMapperButton.setButtonText ("Sample Mapper\nZones, velocity, pads, sample playback");
        advOneShotButton.setButtonText ("One Shot Maker\nRender VST3 notes into sample packs");
        advMidiButton.setButtonText ("MIDI Playground\nArps, chords, patterns, performance");
        advBuildButton.setButtonText ("Asset Builder\nKnobs, sliders, meters, filmstrips");
        advDesignButton.setButtonText ("Design Surface\nPlayer UI, bindings, containers");
        advBrandButton.setButtonText ("Brand / Runtime Lab\nPlayer polish, testing, white-label");
        advDspButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::DSP); };
        advMapperButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::SampleMapper); };
        advOneShotButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::OneShotMaker); };
        advMidiButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::MidiPlayground); };
        advBuildButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Build); };
        advDesignButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Design); };
        advBrandButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Branding); };

        refresh();
        populateFactoryDemos();
        updateModeVisibility();
    }

    void WorkflowPage::stylePrimary (juce::Button& button)
    {
        button.getProperties().set ("fontSize", 13.0);
        button.getProperties().set ("bold", true);
        button.getProperties().set ("primaryAction", true);
        button.getProperties().set ("corner", 8.0);
    }

    void WorkflowPage::styleSecondary (juce::Button& button)
    {
        button.getProperties().set ("fontSize", 12.0);
        button.getProperties().set ("smallButton", true);
        button.getProperties().set ("corner", 8.0);
    }

    void WorkflowPage::styleCardLabel (juce::Label& label, float size, bool bold, juce::Colour colour)
    {
        label.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::topLeft);
    }

    void WorkflowPage::switchTemplate (const juce::String& engineId, const juce::String& label)
    {
        const auto current = owner.getProject().getEngineType();
        const bool sameEngine = (engineId == "drum" && current == "sample") || current == engineId;
        if (sameEngine)
        {
            owner.setBottomTab (engineId == "sample" || engineId == "drum"
                ? BottomPanel::Page::SampleMapper : BottomPanel::Page::DSP);
            return;
        }

        juce::Component::SafePointer<WorkflowPage> self (this);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("Switch Instrument Template")
                .withMessage ("Switching to " + label + " replaces the current engine palette, DSP graph, and starter layout. Save first if you want to keep this version.")
                .withButton ("Switch")
                .withButton ("Cancel")
                .withIconType (juce::MessageBoxIconType::QuestionIcon),
            [self, engineId] (int result)
            {
                if (result != 1)
                    return;

                if (auto* page = self.getComponent())
                {
                    page->owner.getProject().setEngineType (engineId);
                    page->owner.refreshAllPanels();
                    page->owner.setBottomTab (engineId == "sample" || engineId == "drum"
                        ? BottomPanel::Page::SampleMapper : BottomPanel::Page::DSP);
                }
            });
    }

    void WorkflowPage::populateFactoryDemos()
    {
        factoryDemoFolders.clear();
        factoryDemoBox.clear (juce::dontSendNotification);
        juce::StringArray seenFolders;

        for (const auto& root : factoryDemoSearchRoots())
        {
            auto folders = root.findChildFiles (juce::File::findDirectories, false, "*.patchcraft");
            folders.sort();
            for (const auto& folder : folders)
            {
                if (! folder.getChildFile ("manifest.json").existsAsFile())
                    continue;
                const auto folderKey = folder.getFullPathName().replaceCharacter ('\\', '/').trimCharactersAtEnd ("/").toLowerCase();
                if (seenFolders.contains (folderKey))
                    continue;

                auto manifest = juce::JSON::parse (folder.getChildFile ("manifest.json"));
                auto name = folder.getFileNameWithoutExtension();
                if (auto* obj = manifest.getDynamicObject())
                {
                    const auto display = obj->getProperty ("instrumentName").toString();
                    if (display.isNotEmpty())
                        name = display;
                }

                factoryDemoFolders.add (folder);
                seenFolders.add (folderKey);
                factoryDemoBox.addItem (name, factoryDemoFolders.size());
            }
        }

        factoryDemoBox.setEnabled (! factoryDemoFolders.isEmpty());
        loadFactoryDemoButton.setEnabled (! factoryDemoFolders.isEmpty());
    }

    void WorkflowPage::loadSelectedFactoryDemo()
    {
        const auto index = factoryDemoBox.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow (index, factoryDemoFolders.size()))
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Factory demos")
                    .withMessage ("Choose a factory demo first.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::InfoIcon),
                nullptr);
            return;
        }

        owner.loadFactoryDemo (factoryDemoFolders.getReference (index));
    }

    void WorkflowPage::showExportMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Export Runtime Pack...");
        menu.addItem (2, "Send to Expansion Pack...");
        menu.addSeparator();
        menu.addItem (3, "Export Standalone VST3...");
        menu.addItem (4, "Publish Draft to Plugin.club...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton),
            [this] (int result)
            {
                if (result == 1) owner.exportPack();
                else if (result == 2) owner.sendToExpansionPack();
                else if (result == 3) owner.exportVstPlugin();
                else if (result == 4) owner.publishToPluginClub();
            });
    }

    void WorkflowPage::showHealthDialog()
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            "Project Health",
            buildHealthSummary (true));
    }

    BottomPanel::Page WorkflowPage::targetPageFor (TutorialModule module) const
    {
        switch (module)
        {
            case TutorialModule::FullDemo:
            case TutorialModule::BuildSound:
                return owner.getProject().getEngineType() == "sample"
                    ? BottomPanel::Page::SampleMapper
                    : BottomPanel::Page::DSP;
            case TutorialModule::MidiPerformance:
                return BottomPanel::Page::MidiPlayground;
            case TutorialModule::DesignPlayer:
            case TutorialModule::PresetsPacks:
                return BottomPanel::Page::Design;
            case TutorialModule::TestRuntime:
                return BottomPanel::Page::Branding;
            case TutorialModule::Export:
                return BottomPanel::Page::Workflow;
        }
        return BottomPanel::Page::Workflow;
    }

    void WorkflowPage::openTutorialTarget (TutorialModule module)
    {
        if (module == TutorialModule::Export)
        {
            showExportMenu();
            return;
        }

        owner.setBottomTab (targetPageFor (module));
        if (module == TutorialModule::TestRuntime && ! owner.isPreviewActive())
            owner.togglePreview();
        if (module == TutorialModule::FullDemo)
            owner.showDspBuilderTutorial();
    }

    juce::String WorkflowPage::tutorialTextFor (TutorialModule module) const
    {
        const bool sample = owner.getProject().getEngineType() == "sample";
        switch (module)
        {
            case TutorialModule::FullDemo:
                return "Demo Instrument Walkthrough\n\n"
                       "Goal: create one shippable demo instrument, not a pile of disconnected screens.\n\n"
                       "1. Choose Synth Instrument unless you are building from samples.\n"
                       "2. Open Build Sound. On Source, add or select an oscillator/wavetable block. Change volume/blend while Preview is on.\n"
                       "3. Move through Filter, Amp, Mod, FX, and Out. Every block should have a clear target and audible purpose.\n"
                       "4. Open MIDI / Performance. Add an arp, chord phrase, or performance controller only if the sound needs it.\n"
                       "5. Open Design Player. Add knobs/sliders/buttons, then bind each control to a real parameter in the Inspector.\n"
                       "6. Open Presets + Packs. Save the current sound as a full Patch, then add it to an Expansion Pack.\n"
                       "7. Open Test Runtime. Play hardware MIDI, move every UI control, switch tabs/presets, and confirm it matches Design.\n"
                       "8. Run Health Check. Fix unbound controls, missing samples, graph errors, and preset issues.\n"
                       "9. Export Pack or VST3. Export to Documents first; install to the system VST3 folder separately as Administrator.";

            case TutorialModule::BuildSound:
                return sample
                    ? "Sample Instrument Tutorial\n\n"
                      "1. Click Import Samples or drag WAV/AIFF files into Sample Mapper.\n"
                      "2. Confirm Root, Low Key, High Key, Low Vel, and High Vel for each zone.\n"
                      "3. Use velocity handles to create playable dynamic layers.\n"
                      "4. Open Keyzones and Velocity views to verify there are no stacked full-keyboard mistakes.\n"
                      "5. Switch to DSP Builder and add Filter, Amp, FX, and Out shaping blocks.\n"
                      "6. Turn Preview on and play notes while editing cutoff, amp envelope, gain, and FX mix.\n"
                      "7. Save the result as a full Patch before designing the UI."
                    : "Synth / FX Sound Tutorial\n\n"
                      "1. Open DSP Builder and start on Source.\n"
                      "2. Add or select one Source block. For synths, use Oscillator, Wavetable, Noise, or Hybrid source.\n"
                      "3. Select the block card. The Graph Inspector shows what that block controls.\n"
                      "4. Move Source Mixer values while holding a note. If a control does nothing, it must show why or be disabled.\n"
                      "5. Open Filter. Add a filter block and target filter cutoff/resonance.\n"
                      "6. Open Amp. Shape Attack, Decay, Sustain, and Release until the sound has the right feel.\n"
                      "7. Open Mod. Add LFO/macro/automation only after the base sound works.\n"
                      "8. Open FX. Add musical delay/reverb/distortion/EQ blocks and keep mix levels sane.\n"
                      "9. Open Out. Confirm output gain, limiter, stereo width, and clipping safety.\n"
                      "10. Save Patch when the sound is playable.";

            case TutorialModule::MidiPerformance:
                return "MIDI / Performance Tutorial\n\n"
                       "1. Open Performance Builder.\n"
                       "2. Choose a curated progression, arp, drum, chop, or modulation behavior.\n"
                       "3. Edit notes in the piano roll or pattern grid.\n"
                       "4. Use chord presets when you need musical harmony quickly.\n"
                       "5. For samples, route MIDI steps to slices/pads so notes can trigger sections of the sample.\n"
                       "6. Test velocity, gate, probability, swing, strum, flam, and humanize while Preview is active.\n"
                       "7. Save MIDI behavior into the Patch so Easy mode, Test mode, and export all hear the same result.";

            case TutorialModule::DesignPlayer:
                return "Design Player Tutorial\n\n"
                       "1. Open Design.\n"
                       "2. Add a knob/slider/button from Elements or the Library.\n"
                       "3. Select it on the canvas. The Inspector is the source for position, style, text, and DSP assignment.\n"
                       "4. Assign the control to a real parameter such as filterCutoff, volume, delayMix, macro_motion, or modWheel.\n"
                       "5. Use labels deliberately. If you add a knob, edit its label position, size, and spacing.\n"
                       "6. Add containers/tabs only when they organize controls; every tab should switch correctly in Test.\n"
                       "7. Use alignment/order tools to build a clean customer-facing Player UI.\n"
                       "8. Preview and move controls while audio is playing. If it does not change sound, fix the binding.";

            case TutorialModule::PresetsPacks:
                return "Presets + Expansion Pack Tutorial\n\n"
                       "1. Build a playable sound first. A preset is not just knob values; it should recall the full Patch state.\n"
                       "2. Save Patch to capture DSP graph, samples, MIDI behavior, parameter values, and mappings.\n"
                       "3. Save Patch As for variations: Pad, Pluck, Motion, Bass, FX Throw, Drum Kit, etc.\n"
                       "4. Add each preset to an Expansion Pack.\n"
                       "5. Use categories, keywords, and folders so developers can sell organized packs.\n"
                       "6. Run Health Check before export. Missing patch IDs, missing sample refs, or stale graph refs must be fixed.";

            case TutorialModule::TestRuntime:
                return "Runtime Test Tutorial\n\n"
                       "1. Open Test / Brand Lab and turn Preview on.\n"
                       "2. Play the software keyboard and a hardware MIDI keyboard.\n"
                       "3. Confirm hardware note highlights also trigger sound.\n"
                       "4. Move every knob, slider, XY pad, mod wheel, expression, and macro while a note is held.\n"
                       "5. Switch tabs and presets. The Test UI must match the Design UI exactly.\n"
                       "6. Check volume, clipping, stuck notes, MIDI learn, retrigger, and BPM sync.\n"
                       "7. If a control does nothing, return to Design/DSP and fix the assignment before export.";

            case TutorialModule::Export:
                return "Export Tutorial\n\n"
                       "1. Run Health Check first.\n"
                       "2. Export Runtime Pack when you want a .patchcraft pack for the Player.\n"
                       "3. Send to Expansion Pack when you are building sellable preset/content add-ons.\n"
                       "4. Export Standalone VST3 when you want a dedicated plugin for this instrument.\n"
                       "5. Publish Draft to Plugin.club when the pack is ready for seller-dashboard review.\n"
                       "6. Export to Documents/PatchCraft/VST3 Exports. Do not write directly into Program Files.\n"
                       "7. To install into C:\\Program Files\\Common Files\\VST3, copy the exported .vst3 as Administrator or use a future elevated installer flow.\n"
                       "8. Re-scan your DAW and verify the plugin name, UI, presets, sound, and MIDI behavior.";
        }

        return {};
    }

    void WorkflowPage::showModuleTutorial (TutorialModule module)
    {
        juce::Component::SafePointer<WorkflowPage> self (this);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle (module == TutorialModule::FullDemo ? "Full Demo Instrument Tutorial" : "Guided Tutorial")
                .withMessage (tutorialTextFor (module))
                .withButton (module == TutorialModule::Export ? "Open Export Menu" : "Open Module")
                .withButton ("Stay Here")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            [self, module] (int result)
            {
                if (result != 1)
                    return;
                if (auto* page = self.getComponent())
                    page->openTutorialTarget (module);
            });
    }

    juce::String WorkflowPage::buildHealthSummary (bool detailed) const
    {
        const auto& project = owner.getProject();
        const auto engine = project.getEngineType();
        const int sourceBlocks = countBlocksInSection (project, "source");
        const int filterBlocks = countBlocksInSection (project, "filter");
        const int ampBlocks = countBlocksInSection (project, "amp");
        const int modBlocks = countBlocksInSection (project, "mod");
        const int fxBlocks = countBlocksInSection (project, "fx");
        const int outBlocks = countBlocksInSection (project, "out");
        const int boundControls = countRuntimeControls (project, true);
        const int unboundControls = countRuntimeControls (project, false);

        juce::StringArray lines;
        lines.add ("Instrument: " + project.getManifest().instrumentName);
        lines.add ("Engine: " + engineDisplayName (engine));
        lines.add ("Patch source of truth: DSP graph + samples + MIDI + parameter values.");
        lines.add ("Blocks: Source " + juce::String (sourceBlocks)
                   + ", Filter " + juce::String (filterBlocks)
                   + ", Amp " + juce::String (ampBlocks)
                   + ", Mod " + juce::String (modBlocks)
                   + ", FX " + juce::String (fxBlocks)
                   + ", Out " + juce::String (outBlocks));
        lines.add ("Samples: " + juce::String ((int) project.getSampleMap().getZones().size()) + " zones");
        lines.add ("UI controls: " + juce::String (boundControls) + " bound, "
                   + juce::String (unboundControls) + " unbound");
        lines.add ("Presets: " + juce::String ((int) project.getPresets().size())
                   + ", Patches: " + juce::String ((int) project.getPatches().size())
                   + ", Packs: " + juce::String ((int) project.getExpansions().size()));

        juce::StringArray issues;
        if ((engine == "sample" || engine == "drum") && project.getSampleMap().getZones().empty())
            issues.add ("Sample-based instruments need mapped sample zones before export.");
        if (project.getDspGraph().blocks.empty())
            issues.add ("DSP graph has no blocks; the engine will fall back to defaults.");
        if (unboundControls > 0)
            issues.add ("Some UI controls are not bound to parameters.");
        if (project.getPresets().empty())
            issues.add ("No presets exist yet; create playable presets before shipping.");

        for (const auto& issue : project.getParameters().validateReferences (
                 project.getLayout().getAll(), project.getDspGraph(), project.getPresets()))
        {
            if (issue.severity == "error")
                issues.add (issue.toString());
            if (issues.size() >= (detailed ? 12 : 4))
                break;
        }

        for (const auto& issue : project.getDspGraph().validateTypedGraph (engine))
        {
            if (issue.severity == "error")
                issues.add (issue.toString());
            if (issues.size() >= (detailed ? 12 : 4))
                break;
        }

        if (issues.isEmpty())
            lines.add ("Status: Ready to test/export.");
        else
        {
            lines.add ("Status: Needs attention.");
            lines.add ("");
            lines.add ("Next fixes:");
            for (const auto& issue : issues)
                lines.add ("- " + issue);
        }

        if (detailed)
        {
            lines.add ("");
            lines.add ("Recommended path:");
            lines.add ("1. Build or map the sound source.");
            lines.add ("2. Add MIDI/performance behavior if needed.");
            lines.add ("3. Bind Player UI controls to real parameters.");
            lines.add ("4. Save full patches/presets into expansion packs.");
            lines.add ("5. Test the runtime Player surface.");
            lines.add ("6. Export pack/VST3 from a clean health state.");
        }

        return lines.joinIntoString ("\n");
    }

    void WorkflowPage::refresh()
    {
        healthSummary = buildHealthSummary (false);
        detailedHealth = buildHealthSummary (true);
        healthBody.setText (healthSummary, juce::dontSendNotification);
        repaint();
    }

    void WorkflowPage::updateModeVisibility()
    {
        const bool advanced = advancedMode.getToggleState();
        for (auto* button : { &soundButton, &midiButton, &designButton, &presetsButton, &testButton, &exportButton })
            button->setVisible (! advanced);
        for (auto* button : { &advDspButton, &advMapperButton, &advMidiButton,
                              &advBuildButton, &advDesignButton, &advBrandButton })
            button->setVisible (advanced);
        resized();
    }

    void WorkflowPage::paint (juce::Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        juce::ColourGradient bg (juce::Colour (0xff0a0d12), full.getX(), full.getY(),
                                 juce::Colour (0xff07080b), full.getRight(), full.getBottom(), false);
        g.setGradientFill (bg);
        g.fillAll();

        g.setColour (juce::Colour (0xff24313f).withAlpha (0.18f));
        for (int x = 0; x < getWidth(); x += 48)
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
        for (int y = 0; y < getHeight(); y += 48)
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());

        auto bounds = getLocalBounds().reduced (24);
        auto header = bounds.removeFromTop (78);
        auto rail = header.removeFromBottom (26).withTrimmedRight (230);
        if (rail.getWidth() > 480)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.08f));
            g.fillRoundedRectangle (rail.toFloat().reduced (0.0f, 4.0f), 8.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.45f));
            g.drawRoundedRectangle (rail.toFloat().reduced (0.5f, 4.5f), 8.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText ("SOUND  ->  PERFORMANCE  ->  PLAYER UI  ->  PRESETS  ->  RUNTIME PROOF  ->  LAUNCH",
                        rail.reduced (14, 0), juce::Justification::centredLeft);
        }

        auto modeCard = bounds.removeFromLeft (juce::jmin (420, juce::jmax (320, getWidth() / 4))).reduced (18, 22);
        auto healthCard = bounds.removeFromRight (juce::jmin (430, juce::jmax (340, getWidth() / 4))).reduced (18, 22);
        auto pathCard = bounds.reduced (36, 22);

        auto drawCard = [&] (juce::Rectangle<int> area, juce::Colour accent)
        {
            auto rect = area.toFloat();
            juce::ColourGradient grad (PatchCraftLookAndFeel::panelAlt().brighter (0.04f), rect.getX(), rect.getY(),
                                       PatchCraftLookAndFeel::panel().darker (0.18f), rect.getX(), rect.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (rect, 14.0f);
            g.setColour (PatchCraftLookAndFeel::border().brighter (0.18f));
            g.drawRoundedRectangle (rect, 12.0f, 1.0f);
            g.setColour (accent.withAlpha (0.95f));
            g.fillRoundedRectangle (rect.withHeight (4.0f), 2.0f);
            g.setColour (accent.withAlpha (0.08f));
            g.fillRoundedRectangle (rect.reduced (10.0f).withTrimmedTop (46.0f), 10.0f);
            g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.8f));
            g.drawRoundedRectangle (rect.reduced (10.0f).withTrimmedTop (46.0f), 10.0f, 1.0f);

            g.setColour (accent.withAlpha (0.38f));
            const float notch = 16.0f;
            g.drawLine (rect.getX() + 14.0f, rect.getY() + 16.0f, rect.getX() + 14.0f + notch, rect.getY() + 16.0f, 1.2f);
            g.drawLine (rect.getX() + 14.0f, rect.getY() + 16.0f, rect.getX() + 14.0f, rect.getY() + 16.0f + notch, 1.2f);
        };

        drawCard (modeCard, PatchCraftLookAndFeel::accent());
        drawCard (pathCard, juce::Colour (0xff58b7ff));
        drawCard (healthCard, juce::Colour (0xff7bd88f));
    }

    void WorkflowPage::resized()
    {
        auto bounds = getLocalBounds().reduced (24);
        auto header = bounds.removeFromTop (78);
        title.setBounds (header.removeFromTop (34));
        subtitle.setBounds (header.removeFromTop (24));
        advancedMode.setBounds (getWidth() - 210, 30, 180, 24);

        auto modeCardOuter = bounds.removeFromLeft (juce::jmin (420, juce::jmax (320, getWidth() / 4))).reduced (18, 22);
        auto healthCardOuter = bounds.removeFromRight (juce::jmin (430, juce::jmax (340, getWidth() / 4))).reduced (18, 22);
        auto pathCardOuter = bounds.reduced (36, 22);

        auto modeCard = modeCardOuter.reduced (18, 16);
        productTitle.setBounds (modeCard.removeFromTop (24));
        productBody.setBounds (modeCard.removeFromTop (68));
        modeCard.removeFromTop (8);

        for (auto* button : { &synthButton, &sampleButton, &drumButton, &fxButton })
        {
            button->setBounds (modeCard.removeFromTop (58));
            modeCard.removeFromTop (9);
        }
        modeCard.removeFromTop (4);
        factoryDemoLabel.setBounds (modeCard.removeFromTop (22));
        factoryDemoBox.setBounds (modeCard.removeFromTop (34));
        modeCard.removeFromTop (8);
        loadFactoryDemoButton.setBounds (modeCard.removeFromTop (42));

        auto pathCard = pathCardOuter.reduced (22, 16);
        pathTitle.setText (advancedMode.getToggleState() ? "Advanced Module Matrix" : "Guided Launch Path",
                           juce::dontSendNotification);
        pathBody.setText (advancedMode.getToggleState()
            ? "Direct access for power users. Use this when you already know the specific system to edit."
            : "Follow the path left-to-right. Each module should produce one real, testable deliverable.",
            juce::dontSendNotification);
        pathTitle.setBounds (pathCard.removeFromTop (24));
        pathBody.setBounds (pathCard.removeFromTop (48));
        fullTutorialButton.setVisible (! advancedMode.getToggleState());
        if (! advancedMode.getToggleState())
        {
            fullTutorialButton.setBounds (pathCard.removeFromTop (46));
            pathCard.removeFromTop (14);
        }
        else
        {
            pathCard.removeFromTop (12);
        }

        const bool advanced = advancedMode.getToggleState();
        auto placeButtons = [&] (std::initializer_list<juce::TextButton*> buttons)
        {
            const int columns = getWidth() > 1450 ? 2 : 1;
            const int gap = 12;
            const int buttonH = 76;
            const int w = columns == 2 ? (pathCard.getWidth() - gap) / 2 : pathCard.getWidth();
            int index = 0;
            for (auto* button : buttons)
            {
                const int column = columns == 2 ? index % 2 : 0;
                const int row = columns == 2 ? index / 2 : index;
                button->setBounds (pathCard.getX() + column * (w + gap),
                                   pathCard.getY() + row * (buttonH + gap),
                                   w, buttonH);
                ++index;
            }
        };

        if (advanced)
            placeButtons ({ &advDspButton, &advMapperButton, &advOneShotButton, &advMidiButton,
                            &advBuildButton, &advDesignButton, &advBrandButton });
        else
            placeButtons ({ &soundButton, &midiButton, &designButton,
                            &presetsButton, &testButton, &exportButton });

        auto healthCard = healthCardOuter.reduced (22, 16);
        truthTitle.setBounds (healthCard.removeFromTop (24));
        truthBody.setBounds (healthCard.removeFromTop (50));
        healthTitle.setBounds (healthCard.removeFromTop (26));
        healthBody.setBounds (healthCard.removeFromTop (juce::jmax (150, healthCard.getHeight() - 56)));
        healthCard.removeFromTop (10);
        healthCheckButton.setBounds (healthCard.removeFromTop (42));
    }
}
