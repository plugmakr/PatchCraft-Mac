    void CanvasEditor::addElementAt (ElementType type, juce::Point<int> canvasPos, juce::String parameterId)
    {
        LayoutElement el;
        el.type = type;
        el.x = canvasPos.x;
        el.y = canvasPos.y;
        el.width = (type == ElementType::Panel) ? 320
                 : type == ElementType::Shape ? 180
                 : type == ElementType::TabPanel ? 420
                 : type == ElementType::GranularField ? 440
                 : type == ElementType::EqCurve ? 460
                 : type == ElementType::AdsrCurve ? 460
                 : type == ElementType::SpectrumAnalyzer ? 460
                 : type == ElementType::ReactiveImage ? 320
                 : type == ElementType::SpriteAnimator ? 260
                 : type == ElementType::VisualFxLayer ? 420
                 : type == ElementType::AiVisualPrompt ? 360
                 : type == ElementType::SampleDropZone ? 320
                 : type == ElementType::RuntimeSampleLibrary ? 360
                 : type == ElementType::DrumGrid ? 560
                 : type == ElementType::ArpLane ? 260
                 : type == ElementType::SequencerLane ? 480
                 : type == ElementType::PianoRoll ? 620
                 : type == ElementType::Mixer ? 520
                 : type == ElementType::MacroControl ? 190
                 : type == ElementType::ModMatrix ? 420
                 : type == ElementType::PadGrid ? standardPadGridExtent (4, 4)
                 : type == ElementType::DrumPad ? 80
                 : type == ElementType::Toggle ? 128 : 96;
        el.height = (type == ElementType::Panel) ? 180
                  : type == ElementType::Shape ? 120
                  : type == ElementType::TabPanel ? 44
                  : type == ElementType::GranularField ? 220
                  : type == ElementType::EqCurve ? 180
                  : type == ElementType::AdsrCurve ? 180
                  : type == ElementType::SpectrumAnalyzer ? 160
                  : type == ElementType::ReactiveImage ? 200
                  : type == ElementType::SpriteAnimator ? 180
                  : type == ElementType::VisualFxLayer ? 180
                  : type == ElementType::AiVisualPrompt ? 170
                  : type == ElementType::SampleDropZone ? 156
                  : type == ElementType::RuntimeSampleLibrary ? 220
                  : type == ElementType::DrumGrid ? 220
                  : type == ElementType::ArpLane ? 330
                  : type == ElementType::SequencerLane ? 56
                  : type == ElementType::PianoRoll ? 300
                  : type == ElementType::Mixer ? 260
                  : type == ElementType::MacroControl ? 132
                  : type == ElementType::ModMatrix ? 220
                  : type == ElementType::PadGrid ? standardPadGridExtent (4, 4)
                  : type == ElementType::DrumPad ? 80
                  : type == ElementType::Toggle ? 54 : 96;
        el.parameterId = resolveControlParameter (type, parameterId, owner.getProject());
        if (layoutElementRequiresParameter (type) && el.parameterId.isNotEmpty())
        {
            ensureSimpleStackForEngine (owner.getProject(), owner.getProject().getEngineType());
            ensureProjectParameter (owner.getProject(), el.parameterId, {});
            attachControlToExistingRouting (owner.getProject(), el.parameterId);
        }
        if (auto* def = owner.getProject().getParameters().find (el.parameterId))
            el.label = def->name;
        else
            el.label = el.parameterId.isNotEmpty() ? el.parameterId : elementTypeDisplayName (type);
        el.style = "Modern Dark";
        el.groupId = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        if (type == ElementType::TabPanel)
            el.tabs = { "Tab 1", "Tab 2" };
        if (type != ElementType::Panel && type != ElementType::Group && type != ElementType::TabPanel)
        {
            if (auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId()))
            {
                if (selected->type == ElementType::Panel || selected->type == ElementType::Group)
                {
                    el.containerId = selected->id;
                    el.groupId = selected->groupId;
                }
                else if (selected->type == ElementType::TabPanel && ! selected->tabs.isEmpty())
                {
                    const auto active = activeTabGroupsByPanel.find (selected->id);
                    el.groupId = active != activeTabGroupsByPanel.end()
                        ? active->second
                        : scopedTabGroupId (*selected, selected->tabs[0]);
                    el.containerId.clear();
                }
            }
        }
        if (type == ElementType::Group)
            el.label = "New Group";
        if (type == ElementType::Panel)
        {
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
            el.strokeWidth = 1.0f;
            el.cornerRadius = 8.0f;
        }
        if (type == ElementType::TabPanel)
        {
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
            el.strokeWidth = 1.0f;
            el.cornerRadius = 8.0f;
        }
        if (el.parameterId == "bpmSync" || el.parameterId == "retrigger")
        {
            el.groupId.clear();
            el.containerId.clear();
            el.labelPosition = "right";
            el.labelSpacing = 6.0f;
        }
        if (type == ElementType::Shape)
        {
            el.backgroundColour = juce::Colour (0x66141822);
            el.borderColour = PatchCraftLookAndFeel::accent();
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.cornerRadius = 16.0f;
            el.strokeWidth = 2.0f;
        }
        if (type == ElementType::DrumPad)
        {
            el.label = "Pad";
            el.padRows = 1;
            el.padCols = 1;
            el.padBaseNote = 36;
            el.cornerRadius = 6.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::PadGrid)
        {
            el.label = {};
            el.padRows = 4;
            el.padCols = 4;
            el.padBaseNote = 36;
            el.cornerRadius = 6.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::DrumGrid)
        {
            el.label = "Drum Pattern";
            el.drumTracks = 8;
            el.drumSteps = 16;
            el.drumPattern = 0;
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
        }
        if (type == ElementType::ArpLane)
        {
            el.label = "Arp Lane";
            el.parameterId.clear();
            int nextLane = 0;
            for (const auto& existing : owner.getProject().getLayout().getAll())
                if (existing.type == ElementType::ArpLane)
                    nextLane = juce::jmax (nextLane, existing.arpLaneIndex + 1);

            el.arpLaneIndex = juce::jlimit (0, 15, nextLane);
            el.arpLaneSteps = 16;
            el.arpLaneMode = "multiRing";
            el.cornerRadius = 12.0f;
            el.strokeWidth = 1.4f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd10141a);
            el.borderColour = PatchCraftLookAndFeel::border();

            auto& graph = owner.getProject().getDspGraph();
            auto& block = ensureArpBlock (graph);
            block.values["mpActiveBank"] = (float) el.arpLaneIndex;
            block.values["mpMultiLane"] = 1.0f;
            seedMusicalOrbitLaneData (block);
            graph.userConfigured = true;
        }
        if (type == ElementType::SequencerLane)
        {
            el.label = "Gate Lane";
            el.parameterId.clear();
            el.seqLaneIndex = 0;
            el.seqLaneSteps = 16;
            el.seqLaneType = "gate";
            el.seqLaneDirection = "forward";
            el.cornerRadius = 6.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.seqLaneColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd10141a);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::PianoRoll)
        {
            el.label = "Piano Roll";
            el.parameterId.clear();
            el.pianoRollSteps = 16;
            el.pianoRollStepsPerBeat = 4;
            el.pianoRollLowNote = 48;
            el.pianoRollRows = 25;
            el.cornerRadius = 8.0f;
            el.strokeWidth = 1.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd111722);
            el.borderColour = PatchCraftLookAndFeel::border();

            auto& graph = owner.getProject().getDspGraph();
            DspBlock* prBlock = nullptr;
            for (auto& block : graph.blocks)
            {
                const auto t = block.type.trim().toLowerCase().removeCharacters (" _-");
                if (t == "pianoroll" || t == "pianorollclip" || t == "midiclip")
                {
                    prBlock = &block;
                    break;
                }
            }
            if (prBlock == nullptr)
            {
                DspBlock block;
                block.id = "piano_roll";
                block.section = "modulation";
                block.type = "pianoRoll";
                block.name = "Piano Roll";
                block.enabled = true;
                graph.blocks.push_back (block);
                prBlock = &graph.blocks.back();
            }
            prBlock->values["prSteps"] = (float) el.pianoRollSteps;
            prBlock->values["prStepsPerBeat"] = (float) el.pianoRollStepsPerBeat;
            prBlock->values["prLowNote"] = (float) el.pianoRollLowNote;
            prBlock->values["prRows"] = (float) el.pianoRollRows;
            prBlock->values["prRate"] = 1.0f;
            prBlock->values["prGate"] = 0.9f;
            prBlock->values["prVelocity"] = 1.0f;
            prBlock->values["prSync"] = 1.0f;
            prBlock->values["prLoop"] = 1.0f;
            if (prBlock->metadata.find ("notes") == prBlock->metadata.end())
            {
                // Seed a simple Cmaj7 arpeggio so the element is immediately audible.
                prBlock->metadata["notes"] = "0,4,60,0.85;4,4,64,0.85;8,4,67,0.85;12,4,71,0.85";
            }
            graph.userConfigured = true;
        }
        if (type == ElementType::GranularField)
        {
            el.label = "Granular Field";
            el.parameterId = el.parameterId.isNotEmpty() ? el.parameterId : "sampleStart";
            el.cornerRadius = 10.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::SampleDropZone)
        {
            el.label = "Drop Sample";
            el.parameterId = el.parameterId.isNotEmpty() ? el.parameterId : "sampleStart";
            el.semanticRole = "sampleDropZone:" + el.id;
            el.cornerRadius = 10.0f;
            el.strokeWidth = 1.4f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd10141a);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::RuntimeSampleLibrary)
        {
            el.label = "Runtime Samples";
            el.parameterId.clear();
            el.semanticRole = "runtimeSampleLibrary";
            el.cornerRadius = 10.0f;
            el.strokeWidth = 1.2f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0xdd10141a);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::EqCurve)
        {
            el.label = "EQ Curve";
            el.parameterId.clear();
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::SpectrumAnalyzer)
        {
            el.label = "Spectrum";
            el.parameterId.clear();
            el.cornerRadius = 8.0f;
            el.accentColour = juce::Colour (0xff20d6ff);
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::ReactiveImage)
        {
            el.label = "Reactive Image";
            el.parameterId.clear();
            el.audioReactive = true;
            el.audioReactiveMode = "level";
            el.audioReactiveAmount = 0.72f;
            el.animationMode = "breathe";
            el.animationRate = 0.75f;
            el.visualSource = "audioLevel";
            el.visualAction = "pulseGlow";
            el.cornerRadius = 10.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::SpriteAnimator)
        {
            el.label = "Sprite Animator";
            el.parameterId.clear();
            el.animationMode = "pulse";
            el.animationRate = 8.0f;
            el.visualSource = "bpmClock";
            el.visualAction = "frameIndex";
            el.filmstripFrames = 8;
            el.cornerRadius = 8.0f;
            el.accentColour = juce::Colour (0xff60e6b7);
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::VisualFxLayer)
        {
            el.label = "Visual FX Layer";
            el.parameterId.clear();
            el.audioReactive = true;
            el.audioReactiveMode = "peak";
            el.audioReactiveAmount = 0.62f;
            el.animationMode = "glow";
            el.animationRate = 1.2f;
            el.visualPreset = "orbitAura";
            el.visualSource = "audioPeak";
            el.visualAction = "particles";
            el.cornerRadius = 10.0f;
            el.accentColour = juce::Colour (0xff7ee7ff);
            el.backgroundColour = juce::Colour (0x22000000);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::AiVisualPrompt)
        {
            el.label = "AI Visual Prompt";
            el.parameterId.clear();
            el.visualRequiresPro = true;
            el.visualAiPrompt = "Create a clean title banner, matching library thumbnail, reactive glow mask, and optional sprite sheet for this instrument.";
            el.visualAiStyle = "premium playable instrument UI";
            el.cornerRadius = 10.0f;
            el.accentColour = juce::Colour (0xffb98cff);
            el.backgroundColour = juce::Colour (0x44120f19);
            el.borderColour = juce::Colour (0xff55406f);
        }
        if (type == ElementType::Mixer)
        {
            el.label = "Mixer";
            el.labelPosition = "hidden";
            el.mixerMode = "auto";
            el.mixerChannels = 4;
            el.mixerChannelLabels.add ("Main");
            el.mixerChannelLabels.add ("Bus 2");
            el.mixerChannelLabels.add ("Bus 3");
            el.mixerChannelLabels.add ("Bus 4");
            el.mixerVolumeParams.add ("volume");
            el.mixerPanParams.add ("pan");
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }
        if (type == ElementType::MacroControl)
        {
            el.label = "Macro 1";
            if (el.parameterId.isEmpty())
            {
                auto idExists = [this] (const juce::String& id)
                {
                    if (owner.getProject().getParameters().find (id) != nullptr)
                        return true;
                    for (const auto& block : owner.getProject().getDspGraph().blocks)
                        if (block.id == id)
                            return true;
                    return false;
                };
                int suffix = 1;
                do
                {
                    el.parameterId = "macro_" + juce::String (suffix++);
                }
                while (idExists (el.parameterId));
            }
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();

            auto& graph = owner.getProject().getDspGraph();
            bool hasMacroBlock = false;
            for (const auto& block : graph.blocks)
                if (block.id == el.parameterId)
                    hasMacroBlock = true;

            if (! hasMacroBlock)
            {
                DspBlock block;
                block.id = el.parameterId;
                block.section = "mod";
                block.type = "macro";
                block.name = el.label;
                block.enabled = true;
                block.values["value"] = 0.5f;
                graph.blocks.push_back (std::move (block));
                graph.userConfigured = true;
            }
        }
        if (type == ElementType::ModMatrix)
        {
            el.label = "Mod Matrix";
            el.cornerRadius = 8.0f;
            el.accentColour = PatchCraftLookAndFeel::accent();
            el.backgroundColour = juce::Colour (0x33141822);
            el.borderColour = PatchCraftLookAndFeel::border();
        }

        juce::String addedId;
        owner.getProject().performLayoutEdit ("Add " + elementTypeDisplayName (type),
            [&] (LayoutModel& m)
            {
                auto copy = el;
                auto& added = m.add (copy);
                if (added.type == ElementType::SampleDropZone)
                    added.semanticRole = "sampleDropZone:" + added.id;
                addedId = added.id;
            });
        if (addedId.isNotEmpty())
            owner.setSelectedElementId (addedId);

        syncDspGraphValuesToLiveStore (owner.getProject());
        owner.syncExportPreview();
    }

    void CanvasEditor::addMixerChannelAt (juce::Point<int> canvasPos)
    {
        LayoutElement el;
        el.type = ElementType::Mixer;
        el.x = canvasPos.x;
        el.y = canvasPos.y;
        el.width = 116;
        el.height = 260;
        el.label = "Main";
        el.labelPosition = "hidden";
        el.style = "Modern Dark";
        el.groupId = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        el.mixerMode = "parameters";
        el.mixerChannels = 1;
        el.mixerChannelLabels.add ("Main");
        el.mixerVolumeParams.add ("volume");
        el.mixerPanParams.add ("pan");
        el.cornerRadius = 8.0f;
        el.accentColour = PatchCraftLookAndFeel::accent();
        el.backgroundColour = juce::Colour (0x33141822);
        el.borderColour = PatchCraftLookAndFeel::border();

        juce::String addedId;
        owner.getProject().performLayoutEdit ("Add mixer channel",
            [&] (LayoutModel& m)
            {
                auto& added = m.add (el);
                addedId = added.id;
            });

        if (addedId.isNotEmpty())
            owner.setSelectedElementId (addedId);
    }

    bool CanvasEditor::selectionContainsMixer() const
    {
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* el = owner.getProject().getLayout().find (id))
                if (el->type == ElementType::Mixer)
                    return true;

        return false;
    }

    void CanvasEditor::explodeSelectedMixers()
    {
        struct SourceMixer
        {
            LayoutElement element;
        };

        std::vector<SourceMixer> mixers;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* el = owner.getProject().getLayout().find (id))
                if (el->type == ElementType::Mixer)
                    mixers.push_back ({ *el });

        if (mixers.empty())
            return;

        juce::StringArray newIds;
        owner.getProject().performLayoutEdit ("Break mixer into channel strips",
            [&] (LayoutModel& layout)
            {
                for (const auto& source : mixers)
                {
                    const auto& src = source.element;
                    const int channels = juce::jlimit (1, 16, src.mixerChannels);
                    const int stripW = juce::jmax (92, juce::jmin (140, src.width / channels));
                    const int gap = 10;

                    for (int channel = 0; channel < channels; ++channel)
                    {
                        LayoutElement strip = src;
                        strip.id.clear();
                        strip.x = src.x + channel * (stripW + gap);
                        strip.y = src.y;
                        strip.width = stripW;
                        strip.height = src.height;
                        strip.label = mixerSlotAt (src.mixerChannelLabels, channel,
                                                   channel == 0 ? juce::String ("Main")
                                                                : "Bus " + juce::String (channel + 1));
                        strip.labelPosition = "hidden";
                        strip.mixerMode = "parameters";
                        strip.mixerChannels = 1;
                        strip.mixerChannelLabels.clear();
                        strip.mixerChannelLabels.add (strip.label);
                        strip.mixerVolumeParams.clear();
                        strip.mixerPanParams.clear();
                        strip.mixerMuteParams.clear();
                        strip.mixerSoloParams.clear();
                        strip.mixerVolumeParams.add (mixerSlotAt (src.mixerVolumeParams, channel,
                                                                  channel == 0 ? juce::String ("volume") : juce::String()));
                        strip.mixerPanParams.add (mixerSlotAt (src.mixerPanParams, channel,
                                                               channel == 0 ? juce::String ("pan") : juce::String()));
                        strip.mixerMuteParams.add (mixerSlotAt (src.mixerMuteParams, channel));
                        strip.mixerSoloParams.add (mixerSlotAt (src.mixerSoloParams, channel));
                        auto& added = layout.add (strip);
                        newIds.add (added.id);
                    }

                    layout.remove (src.id);
                }
            });

        if (! newIds.isEmpty())
            owner.setSelectedElementIds (newIds);
    }

    void CanvasEditor::addDrumMachineControlLayout (juce::Point<int> canvasPos)
    {
        auto& graph = owner.getProject().getDspGraph();
        ensureDrumMachineBlock (graph);

        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        owner.getProject().performLayoutEdit ("Add Drum Machine Control Surface",
            [&] (LayoutModel& layout)
            {
                LayoutElement panel;
                panel.type = ElementType::Panel;
                panel.label = "Drum Machine Surface";
                panel.x = canvasPos.x;
                panel.y = canvasPos.y;
                panel.width = 1080;
                panel.height = 420;
                panel.groupId = tabGroup;
                panel.cornerRadius = 18.0f;
                panel.strokeWidth = 2.0f;
                panel.backgroundColour = juce::Colour (0xcc070a0f);
                panel.borderColour = PatchCraftLookAndFeel::accent();
                panel.accentColour = PatchCraftLookAndFeel::accent();
                auto& addedPanel = layout.add (panel);
                const auto panelId = addedPanel.id;
                addedIds.add (panelId);

                auto addChild = [&] (LayoutElement child, const juce::String& prefix)
                {
                    child.containerId = panelId;
                    child.groupId = tabGroup;
                    if (child.id.isEmpty())
                        child.id = layout.generateUniqueId (prefix);
                    auto& added = layout.add (child);
                    addedIds.add (added.id);
                };

                LayoutElement title;
                title.type = ElementType::Label;
                title.label = "DRUM MACHINE BUILDER";
                title.x = canvasPos.x + 20;
                title.y = canvasPos.y + 14;
                title.width = 360;
                title.height = 24;
                title.labelSize = 16.0f;
                title.textColour = PatchCraftLookAndFeel::textBright();
                title.accentColour = PatchCraftLookAndFeel::accent();
                addChild (title, "label_");

                LayoutElement help;
                help.type = ElementType::Label;
                help.label = "Pads trigger mapped one-shots. The pattern grid drives playback. Pad level knobs are live per-pad controls.";
                help.x = canvasPos.x + 392;
                help.y = canvasPos.y + 16;
                help.width = 650;
                help.height = 22;
                help.labelSize = 11.0f;
                help.textColour = PatchCraftLookAndFeel::textDim();
                addChild (help, "label_");

                LayoutElement pads;
                pads.type = ElementType::PadGrid;
                pads.label = "Performance Pads";
                pads.x = canvasPos.x + 20;
                pads.y = canvasPos.y + 58;
                pads.width = standardPadGridExtent (4, 4);
                pads.height = standardPadGridExtent (4, 4);
                pads.padRows = 4;
                pads.padCols = 4;
                pads.padBaseNote = 36;
                pads.cornerRadius = 10.0f;
                pads.backgroundColour = juce::Colour (0x55111822);
                pads.borderColour = PatchCraftLookAndFeel::border();
                pads.accentColour = PatchCraftLookAndFeel::accent();
                addChild (pads, "padGrid_");

                LayoutElement grid;
                grid.type = ElementType::DrumGrid;
                grid.label = "Pattern Sequencer";
                grid.x = canvasPos.x + 340;
                grid.y = canvasPos.y + 58;
                grid.width = 700;
                grid.height = 210;
                grid.drumTracks = 8;
                grid.drumSteps = 16;
                grid.drumPattern = 0;
                grid.cornerRadius = 10.0f;
                grid.backgroundColour = juce::Colour (0x55111822);
                grid.borderColour = PatchCraftLookAndFeel::border();
                grid.accentColour = PatchCraftLookAndFeel::accent();
                addChild (grid, "drumGrid_");

                for (int pad = 0; pad < 16; ++pad)
                {
                    const int row = pad / 8;
                    const int col = pad % 8;

                    LayoutElement knob;
                    knob.type = ElementType::Knob;
                    knob.parameterId = "pad" + juce::String (pad + 1) + "Volume";
                    knob.label = "P" + juce::String (pad + 1);
                    knob.x = canvasPos.x + 346 + col * 86;
                    knob.y = canvasPos.y + 292 + row * 56;
                    knob.width = 58;
                    knob.height = 50;
                    knob.labelPosition = "bottom";
                    knob.labelSpacing = 3.0f;
                    knob.labelSize = 9.0f;
                    knob.backgroundColour = juce::Colour (0x33141822);
                    knob.borderColour = PatchCraftLookAndFeel::border();
                    knob.accentColour = PatchCraftLookAndFeel::accent();
                    addChild (knob, "knob_");
                }
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        repaint();
    }

    void CanvasEditor::addCircleSeqInstrumentLayout (juce::Point<int> canvasPos)
    {
        auto& graph = owner.getProject().getDspGraph();
        auto& arpBlock = ensureArpBlock (graph);
        seedMusicalOrbitLaneData (arpBlock);
        auto& live = owner.getProject().getLiveValues();
        live.setValue ("arpLaneMode", 1.0f);
        live.setValue ("arpLaneControlBank", 0.0f);
        live.setValue ("arpLaneSliderRole", 0.0f);
        live.setValue ("arpLaneTarget", 0.0f);
        live.setValue ("arpLaneSound", 12.0f);
        live.setValue ("arpLaneRootNote", 57.0f);
        live.setValue ("arpLaneSampleSlots", 16.0f);
        live.setValue ("arpLaneDirection", 0.0f);
        live.setValue ("arpLaneRate", 1.0f);
        live.setValue ("arpLaneGate", 0.58f);
        live.setValue ("arpLaneSwing", 0.08f);
        live.setValue ("arpLaneProbability", 1.0f);
        live.setValue ("arpLaneEuclideanPulses", 4.0f);
        live.setValue ("arpLaneRatchet", 1.0f);
        live.setValue ("arpLaneFillPulses", 3.0f);
        live.setValue ("arpLaneFillProbability", 0.32f);
        live.setValue ("arpLaneRetrigger", 1.0f);
        live.setValue ("arpLaneMute", 0.0f);
        live.setValue ("arpLaneSolo", 0.0f);
        graph.userConfigured = true;

        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        owner.getProject().performLayoutEdit ("Add CircleSEQ musical surface",
            [&] (LayoutModel& layout)
            {
                LayoutElement panel;
                panel.type = ElementType::Panel;
                panel.label = "CircleSEQ Musical Surface";
                panel.x = canvasPos.x;
                panel.y = canvasPos.y;
                panel.width = 1120;
                panel.height = 560;
                panel.groupId = tabGroup;
                panel.cornerRadius = 18.0f;
                panel.strokeWidth = 2.0f;
                panel.backgroundColour = juce::Colour (0xcc070a0f);
                panel.borderColour = PatchCraftLookAndFeel::accent();
                panel.accentColour = PatchCraftLookAndFeel::accent();
                auto& addedPanel = layout.add (panel);
                const auto panelId = addedPanel.id;
                addedIds.add (panelId);

                auto addChild = [&] (LayoutElement child, const juce::String& prefix)
                {
                    child.containerId = panelId;
                    child.groupId = tabGroup;
                    if (child.id.isEmpty())
                        child.id = layout.generateUniqueId (prefix);
                    auto& added = layout.add (child);
                    addedIds.add (added.id);
                };

                LayoutElement title;
                title.type = ElementType::Label;
                title.label = "CIRCLESEQ MUSICAL SURFACE";
                title.x = canvasPos.x + 22;
                title.y = canvasPos.y + 16;
                title.width = 360;
                title.height = 24;
                title.labelSize = 16.0f;
                title.textColour = PatchCraftLookAndFeel::textBright();
                title.accentColour = PatchCraftLookAndFeel::accent();
                addChild (title, "label_");

                LayoutElement help;
                help.type = ElementType::Label;
                help.label = "Five rings share the DSP engine: choose a lane, pick sound/timing/role, then push and pull steps in real time.";
                help.x = canvasPos.x + 370;
                help.y = canvasPos.y + 18;
                help.width = 700;
                help.height = 22;
                help.labelSize = 11.0f;
                help.textColour = PatchCraftLookAndFeel::textDim();
                addChild (help, "label_");

                LayoutElement orbit;
                orbit.type = ElementType::ArpLane;
                orbit.label = "CircleSEQ";
                orbit.x = canvasPos.x + 22;
                orbit.y = canvasPos.y + 58;
                orbit.width = 650;
                orbit.height = 430;
                orbit.arpLaneIndex = 0;
                orbit.arpLaneSteps = 16;
                orbit.arpLaneMode = "multiRing";
                orbit.arpLaneTarget = "notes";
                orbit.arpLaneSampleSlots = 8;
                orbit.arpLaneRootNote = 57;
                orbit.arpLaneEuclideanPulses = 4;
                orbit.arpLaneRatchet = 1;
                orbit.arpLaneFillPulses = 3;
                orbit.arpLaneFillProbability = 0.32f;
                orbit.cornerRadius = 14.0f;
                orbit.backgroundColour = juce::Colour (0xee090d12);
                orbit.borderColour = PatchCraftLookAndFeel::border();
                orbit.accentColour = PatchCraftLookAndFeel::accent();
                addChild (orbit, "orbit_");

                for (int lane = 0; lane < 5; ++lane)
                {
                    LayoutElement drop;
                    drop.type = ElementType::SampleDropZone;
                    drop.label = "R" + juce::String (lane + 1) + " SAMPLES";
                    drop.x = canvasPos.x + 30 + lane * 128;
                    drop.y = canvasPos.y + 496;
                    drop.width = 118;
                    drop.height = 50;
                    drop.parameterId = "arpLaneSampleSlots";
                    drop.semanticRole = "circleSeqLaneSample:" + juce::String (lane);
                    drop.cornerRadius = 8.0f;
                    drop.backgroundColour = juce::Colour (0xaa0d1720);
                    drop.accentColour = lane == 0 ? juce::Colour (0xff14d9ff)
                                      : lane == 1 ? juce::Colour (0xff9b6cff)
                                      : lane == 2 ? juce::Colour (0xffff5d8f)
                                      : lane == 3 ? juce::Colour (0xffffa316)
                                                  : juce::Colour (0xff7ed957);
                    drop.borderColour = drop.accentColour;
                    addChild (drop, "sampleDrop_");
                }

                auto addControl = [&] (ElementType type, juce::String label, juce::String parameter,
                                       int x, int y, int w, int h, const juce::String& prefix)
                {
                    LayoutElement control;
                    control.type = type;
                    control.label = std::move (label);
                    control.parameterId = std::move (parameter);
                    control.x = canvasPos.x + x;
                    control.y = canvasPos.y + y;
                    control.width = w;
                    control.height = h;
                    control.labelPosition = type == ElementType::Knob || type == ElementType::Slider ? "bottom" : "hidden";
                    control.labelSize = type == ElementType::Slider ? 8.0f : 9.5f;
                    control.labelSpacing = 3.0f;
                    control.cornerRadius = 7.0f;
                    control.backgroundColour = juce::Colour (0x33141822);
                    control.borderColour = PatchCraftLookAndFeel::border();
                    control.accentColour = PatchCraftLookAndFeel::accent();
                    addChild (control, prefix);
                };

                addControl (ElementType::Dropdown, "Lane", "arpLaneControlBank", 700, 64, 98, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Target", "arpLaneTarget", 806, 64, 98, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Sound", "arpLaneSound", 912, 64, 92, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Role", "arpLaneSliderRole", 1012, 64, 96, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Direction", "arpLaneDirection", 700, 110, 132, 34, "dropdown_");
                addControl (ElementType::Dropdown, "FX", "arpLaneFxTarget", 840, 110, 88, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Group", "arpLaneGroup", 936, 110, 82, 34, "dropdown_");
                addControl (ElementType::Dropdown, "Preset", "arpLanePatternLaunch", 1034, 110, 74, 34, "dropdown_");
                addControl (ElementType::Button, "FILL", "arpLaneFillMomentary", 700, 166, 88, 30, "button_");
                addControl (ElementType::Toggle, "LATCH", "arpLaneFillLatch", 796, 166, 82, 30, "toggle_");
                addControl (ElementType::Toggle, "BYPASS", "arpLaneMute", 886, 166, 82, 30, "toggle_");
                addControl (ElementType::Toggle, "SOLO", "arpLaneSolo", 976, 166, 58, 30, "toggle_");
                addControl (ElementType::Toggle, "RTRG", "arpLaneRetrigger", 1042, 166, 58, 30, "toggle_");
                addControl (ElementType::Knob, "Rate", "arpLaneRate", 700, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "Gate", "arpLaneGate", 768, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "Swing", "arpLaneSwing", 836, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "Chance", "arpLaneProbability", 904, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "FX Amt", "arpLaneFxAmount", 972, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "Root", "arpLaneRootNote", 1040, 218, 58, 76, "knob_");
                addControl (ElementType::Knob, "Pulses", "arpLaneEuclideanPulses", 700, 314, 58, 76, "knob_");
                addControl (ElementType::Knob, "Ratchet", "arpLaneRatchet", 768, 314, 58, 76, "knob_");
                addControl (ElementType::Knob, "Rotate", "arpLaneRotate", 836, 314, 58, 76, "knob_");
                addControl (ElementType::Knob, "Fill", "arpLaneFillPulses", 904, 314, 58, 76, "knob_");
                addControl (ElementType::Knob, "Fill %", "arpLaneFillProbability", 972, 314, 58, 76, "knob_");
                addControl (ElementType::Knob, "Slots", "arpLaneSampleSlots", 1040, 314, 58, 76, "knob_");

                for (int step = 0; step < 16; ++step)
                {
                    const int col = step % 8;
                    const int row = step / 8;
                    addControl (ElementType::Slider,
                                juce::String (step + 1),
                                "arpLaneStep" + juce::String (step + 1),
                                700 + col * 48,
                                414 + row * 64,
                                36,
                                56,
                                "slider_");
                }
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        applyArpLaneParameterToGraph (owner.getProject(), "arpLaneIndex");
        repaint();
    }

    void CanvasEditor::addOrbitInstrumentControlLayout (juce::Point<int> canvasPos)
    {
        addCircleSeqInstrumentLayout (canvasPos);
    }

    void CanvasEditor::addVisualReactivityControlLayout (juce::Point<int> canvasPos)
    {
        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        owner.getProject().performLayoutEdit ("Add Animation Lab visual kit",
            [&] (LayoutModel& layout)
            {
                LayoutElement panel;
                panel.type = ElementType::Panel;
                panel.label = "Animation Lab Visual Kit";
                panel.x = canvasPos.x;
                panel.y = canvasPos.y;
                panel.width = 980;
                panel.height = 430;
                panel.groupId = tabGroup;
                panel.cornerRadius = 16.0f;
                panel.strokeWidth = 2.0f;
                panel.backgroundColour = juce::Colour (0xcc080b10);
                panel.borderColour = PatchCraftLookAndFeel::accent();
                panel.accentColour = PatchCraftLookAndFeel::accent();
                auto& addedPanel = layout.add (panel);
                const auto panelId = addedPanel.id;
                addedIds.add (panelId);

                auto addChild = [&] (LayoutElement child, const juce::String& prefix)
                {
                    child.containerId = panelId;
                    child.groupId = tabGroup;
                    if (child.id.isEmpty())
                        child.id = layout.generateUniqueId (prefix);
                    auto& added = layout.add (child);
                    addedIds.add (added.id);
                };

                LayoutElement title;
                title.type = ElementType::Label;
                title.label = "ANIMATION LAB STARTER";
                title.x = canvasPos.x + 22;
                title.y = canvasPos.y + 18;
                title.width = 280;
                title.height = 24;
                title.labelSize = 16.0f;
                title.textColour = PatchCraftLookAndFeel::textBright();
                addChild (title, "label_");

                LayoutElement guide;
                guide.type = ElementType::Label;
                guide.label = "Non-Pro: imported art, sprite sheets, procedural FX. Pro: AI prompts generate banners, thumbnails, masks, and animation sources.";
                guide.x = canvasPos.x + 312;
                guide.y = canvasPos.y + 20;
                guide.width = 620;
                guide.height = 22;
                guide.labelSize = 10.5f;
                guide.textColour = PatchCraftLookAndFeel::textDim();
                addChild (guide, "label_");

                LayoutElement reactive;
                reactive.type = ElementType::ReactiveImage;
                reactive.label = "Reactive Artwork Slot";
                reactive.x = canvasPos.x + 26;
                reactive.y = canvasPos.y + 64;
                reactive.width = 300;
                reactive.height = 238;
                reactive.audioReactive = true;
                reactive.audioReactiveMode = "level";
                reactive.audioReactiveAmount = 0.82f;
                reactive.animationMode = "breathe";
                reactive.animationRate = 0.7f;
                reactive.visualSource = "master level";
                reactive.visualAction = "scale + glow";
                reactive.cornerRadius = 12.0f;
                reactive.accentColour = PatchCraftLookAndFeel::accent();
                reactive.backgroundColour = juce::Colour (0x33141822);
                reactive.borderColour = PatchCraftLookAndFeel::border();
                addChild (reactive, "reactive_");

                LayoutElement fx;
                fx.type = ElementType::VisualFxLayer;
                fx.label = "CircleSEQ Aura FX";
                fx.x = canvasPos.x + 350;
                fx.y = canvasPos.y + 64;
                fx.width = 280;
                fx.height = 238;
                fx.audioReactive = true;
                fx.audioReactiveMode = "peak";
                fx.audioReactiveAmount = 0.68f;
                fx.animationMode = "glow";
                fx.animationRate = 1.2f;
                fx.visualPreset = "orbitAura";
                fx.visualSource = "audio peak";
                fx.visualAction = "particles + trails";
                fx.cornerRadius = 12.0f;
                fx.accentColour = juce::Colour (0xff7ee7ff);
                fx.backgroundColour = juce::Colour (0x22000000);
                fx.borderColour = PatchCraftLookAndFeel::border();
                addChild (fx, "visualfx_");

                LayoutElement sprite;
                sprite.type = ElementType::SpriteAnimator;
                sprite.label = "Sprite Sheet Slot";
                sprite.x = canvasPos.x + 654;
                sprite.y = canvasPos.y + 64;
                sprite.width = 280;
                sprite.height = 238;
                sprite.animationMode = "pulse";
                sprite.animationRate = 8.0f;
                sprite.visualSource = "BPM / note gate";
                sprite.visualAction = "frame index";
                sprite.filmstripFrames = 8;
                sprite.cornerRadius = 12.0f;
                sprite.accentColour = juce::Colour (0xff60e6b7);
                sprite.backgroundColour = juce::Colour (0x33141822);
                sprite.borderColour = PatchCraftLookAndFeel::border();
                addChild (sprite, "sprite_");

                LayoutElement pro;
                pro.type = ElementType::AiVisualPrompt;
                pro.label = "Pro AI Visual Brief";
                pro.x = canvasPos.x + 26;
                pro.y = canvasPos.y + 320;
                pro.width = 908;
                pro.height = 82;
                pro.visualRequiresPro = true;
                pro.visualAiPrompt = "Generate a title banner, library thumbnail, reactive glow mask, and 8-frame sprite accents that match this instrument's sound, CircleSEQ motion, and brand colors.";
                pro.visualAiStyle = "premium plugin artwork, clean, no center text, real instrument UI assets";
                pro.cornerRadius = 10.0f;
                pro.accentColour = juce::Colour (0xffb98cff);
                pro.backgroundColour = juce::Colour (0x44120f19);
                pro.borderColour = juce::Colour (0xff55406f);
                addChild (pro, "aivisual_");
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        repaint();
    }

    void CanvasEditor::addCircleSeqBackgroundKit (juce::Point<int> canvasPos)
    {
        const auto canvas = owner.getProject().getCanvasSize();
        const int canvasW = juce::jmax (900, canvas.width);
        const int canvasH = juce::jmax (540, canvas.height);
        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;
        juce::StringArray addedIds;

        const int cx = canvasW / 2;
        const int cy = juce::roundToInt ((float) canvasH * 0.43f);
        const int ring = juce::jlimit (280, 680, juce::jmin (canvasW, canvasH) - 150);
        const int panelY = juce::jmax (cy + ring / 2 + 22, canvasH - 182);

        owner.getProject().performLayoutEdit ("Add CircleSEQ background kit",
            [&] (LayoutModel& layout)
            {
                auto addShape = [&] (juce::String label,
                                     juce::String shapeKind,
                                     int x, int y, int w, int h,
                                     juce::Colour fill,
                                     juce::Colour border,
                                     float stroke,
                                     float radius,
                                     float opacity = 1.0f)
                {
                    LayoutElement shape;
                    shape.type = ElementType::Shape;
                    shape.label = std::move (label);
                    shape.shapeKind = std::move (shapeKind);
                    shape.x = x;
                    shape.y = y;
                    shape.width = juce::jmax (1, w);
                    shape.height = juce::jmax (1, h);
                    shape.groupId = tabGroup;
                    shape.backgroundColour = fill;
                    shape.borderColour = border;
                    shape.accentColour = border;
                    shape.strokeWidth = stroke;
                    shape.cornerRadius = radius;
                    shape.opacity = opacity;
                    auto& added = layout.add (shape);
                    addedIds.add (added.id);
                };

                addShape ("Background plate", "roundedRect",
                          0, 0, canvasW, canvasH,
                          juce::Colour (0xff05080c), juce::Colour (0xff101722),
                          1.0f, 18.0f);

                addShape ("Top glass haze", "ellipse",
                          cx - ring, cy - ring, ring * 2, ring * 2,
                          juce::Colour (0x2210c6ff), juce::Colour (0x6630d9ff),
                          2.0f, 0.0f, 0.70f);
                addShape ("Outer radial ring", "ellipse",
                          cx - ring / 2, cy - ring / 2, ring, ring,
                          juce::Colour (0x1110c6ff), juce::Colour (0xaa00d4ff),
                          2.0f, 0.0f);
                addShape ("Middle radial ring", "ellipse",
                          cx - ring * 39 / 100, cy - ring * 39 / 100, ring * 78 / 100, ring * 78 / 100,
                          juce::Colour (0x0500d4ff), juce::Colour (0x7718a8ff),
                          1.4f, 0.0f);
                addShape ("Inner shadow hub", "ellipse",
                          cx - ring * 16 / 100, cy - ring * 16 / 100, ring * 32 / 100, ring * 32 / 100,
                          juce::Colour (0xcc03070b), juce::Colour (0x5530d9ff),
                          1.0f, 0.0f);

                addShape ("Horizontal orbit guide", "line",
                          cx - ring / 2, cy - 1, ring, 2,
                          juce::Colours::transparentBlack, juce::Colour (0x6600d4ff),
                          1.2f, 0.0f);
                addShape ("Vertical orbit guide", "line",
                          cx - 1, cy - ring / 2, 2, ring,
                          juce::Colours::transparentBlack, juce::Colour (0x6600d4ff),
                          1.2f, 0.0f);

                const int sideW = juce::jmax (180, (canvasW - ring) / 2 - 46);
                addShape ("Left utility rail", "roundedRect",
                          24, 72, sideW, juce::jmax (280, canvasH - 268),
                          juce::Colour (0x77101822), juce::Colour (0x442b8cff),
                          1.0f, 10.0f);
                addShape ("Right inspector rail", "roundedRect",
                          canvasW - sideW - 24, 72, sideW, juce::jmax (280, canvasH - 268),
                          juce::Colour (0x77101822), juce::Colour (0x442b8cff),
                          1.0f, 10.0f);

                const int panelGap = 14;
                const int panelW = juce::jmax (180, (canvasW - 48 - panelGap * 3) / 4);
                for (int i = 0; i < 4; ++i)
                {
                    addShape ("Bottom control bay " + juce::String (i + 1), "roundedRect",
                              24 + i * (panelW + panelGap), panelY, panelW, juce::jmax (112, canvasH - panelY - 24),
                              juce::Colour (0x88101720),
                              juce::Colour (i == 0 ? 0x8840d8ff : i == 1 ? 0x88a96bff : i == 2 ? 0x88ff4f82 : 0x88ffa600),
                              1.2f, 9.0f);
                }
            });

        if (! addedIds.isEmpty())
            owner.setSelectedElementIds (addedIds);

        repaint();
        (void) canvasPos;
    }

    void CanvasEditor::addModuleLayout (const juce::String& moduleType, juce::Point<int> pos)
    {
        auto& projectObj = owner.getProject();
        auto& graph = projectObj.getDspGraph();
        auto& pm = projectObj.getParameters();
        auto& liveValues = projectObj.getLiveValues();
        const auto tabGroup = currentTabGroup == "main" ? juce::String() : currentTabGroup;

        auto ensureParam = [&] (const juce::String& paramId, const juce::String& engineHint)
        {
            if (paramId.isEmpty() || pm.find (paramId) != nullptr)
                return;

            juce::StringArray engines;
            if (engineHint.isNotEmpty())
                engines.addIfNotAlreadyThere (engineHint);
            engines.addIfNotAlreadyThere (projectObj.getEngineType());
            engines.addIfNotAlreadyThere ("synth");
            engines.addIfNotAlreadyThere ("sample");
            engines.addIfNotAlreadyThere ("fx");

            ParameterDef def;
            for (const auto& engine : engines)
            {
                if (ParameterModel::getRegistryDefinition (paramId, engine, def))
                {
                    pm.add (def);
                    liveValues.getOrAddRaw (def.id, def.defaultValue);
                    return;
                }
            }
        };

        auto ensureParams = [&] (const juce::StringArray& paramIds, const juce::String& engineHint)
        {
            for (const auto& paramId : paramIds)
                ensureParam (paramId, engineHint);
        };

        auto ensureBlock = [&] (const juce::String& blockId,
                                const juce::String& section,
                                const juce::String& type,
                                const juce::String& name,
                                const juce::String& targetId,
                                const juce::String& family,
                                const juce::String& role,
                                const juce::String& ioMode,
                                std::initializer_list<std::pair<const char*, float>> defaults)
        {
            for (auto& block : graph.blocks)
            {
                if (block.id == blockId)
                {
                    block.section = section;
                    block.type = type;
                    block.name = name;
                    block.targetId = targetId;
                    block.enabled = true;
                    block.metadata["family"] = family;
                    block.metadata["role"] = role;
                    block.metadata["ioMode"] = ioMode;
                    for (const auto& value : defaults)
                        if (block.values.find (value.first) == block.values.end())
                            block.values[value.first] = value.second;
                    for (const auto& value : defaults)
                        if (pm.find (value.first) != nullptr)
                            liveValues.setValue (value.first, block.values[value.first]);
                    graph.userConfigured = true;
                    return;
                }
            }

            DspBlock block;
            block.id = blockId;
            block.section = section;
            block.type = type;
            block.name = name;
            block.targetId = targetId;
            block.enabled = true;
            block.metadata["family"] = family;
            block.metadata["role"] = role;
            block.metadata["ioMode"] = ioMode;
            for (const auto& value : defaults)
                block.values[value.first] = value.second;
            for (const auto& value : defaults)
                if (pm.find (value.first) != nullptr)
                    liveValues.setValue (value.first, value.second);
            graph.blocks.push_back (std::move (block));
            graph.userConfigured = true;
        };

        auto addChildKnob = [&] (LayoutModel& layout, const juce::String& panelId,
                                 const juce::String& label, const juce::String& paramId,
                                 int xOffset, int yOffset, int size = 66)
        {
            LayoutElement knob;
            knob.type = ElementType::Knob;
            knob.label = label;
            knob.parameterId = paramId;
            knob.x = pos.x + xOffset;
            knob.y = pos.y + yOffset;
            knob.width = size;
            knob.height = size;
            knob.containerId = panelId;
            knob.groupId = tabGroup;
            knob.id = layout.generateUniqueId ("knob_");
            knob.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (knob);
        };

        auto addChildSlider = [&] (LayoutModel& layout, const juce::String& panelId,
                                   const juce::String& label, const juce::String& paramId,
                                   int xOffset, int yOffset, int width = 44, int height = 118)
        {
            LayoutElement slider;
            slider.type = ElementType::Slider;
            slider.label = label;
            slider.parameterId = paramId;
            slider.x = pos.x + xOffset;
            slider.y = pos.y + yOffset;
            slider.width = width;
            slider.height = height;
            slider.containerId = panelId;
            slider.groupId = tabGroup;
            slider.id = layout.generateUniqueId ("slider_");
            slider.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (slider);
        };

        auto addChildToggle = [&] (LayoutModel& layout, const juce::String& panelId,
                                   const juce::String& label, const juce::String& paramId,
                                   int xOffset, int yOffset, int width = 86, int height = 34)
        {
            LayoutElement toggle;
            toggle.type = ElementType::Toggle;
            toggle.label = label;
            toggle.parameterId = paramId;
            toggle.x = pos.x + xOffset;
            toggle.y = pos.y + yOffset;
            toggle.width = width;
            toggle.height = height;
            toggle.containerId = panelId;
            toggle.groupId = tabGroup;
            toggle.id = layout.generateUniqueId ("toggle_");
            toggle.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (toggle);
        };

        auto addChildValue = [&] (LayoutModel& layout, const juce::String& panelId,
                                  const juce::String& label, const juce::String& paramId,
                                  int xOffset, int yOffset, int width = 86, int height = 34)
        {
            LayoutElement value;
            value.type = ElementType::ValueDisplay;
            value.label = label;
            value.parameterId = paramId;
            value.x = pos.x + xOffset;
            value.y = pos.y + yOffset;
            value.width = width;
            value.height = height;
            value.containerId = panelId;
            value.groupId = tabGroup;
            value.id = layout.generateUniqueId ("value_");
            value.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (value);
        };

        auto addChildSurface = [&] (LayoutModel& layout, const juce::String& panelId,
                                    ElementType type, const juce::String& label,
                                    const juce::String& paramId,
                                    int xOffset, int yOffset, int width, int height)
        {
            LayoutElement surface;
            surface.type = type;
            surface.label = label;
            if (layoutElementUsesSemanticParameterId (type))
                surface.parameterId.clear();
            else
                surface.parameterId = paramId;
            surface.x = pos.x + xOffset;
            surface.y = pos.y + yOffset;
            surface.width = width;
            surface.height = height;
            surface.containerId = panelId;
            surface.groupId = tabGroup;
            surface.id = layout.generateUniqueId ("module_");
            surface.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (surface);
        };

        auto addModulePanel = [&] (const juce::String& undoName,
                                   const juce::String& title,
                                   int width,
                                   int height,
                                   auto buildChildren)
        {
            owner.getProject().performLayoutEdit (undoName,
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = title;
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = width;
                    panel.height = height;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 10.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;
                    buildChildren (layout, panelId);
                });
        };

        auto finishModernModule = [&]
        {
            syncDspGraphValuesToLiveStore (owner.getProject());
            owner.syncExportPreview();
            owner.getProject().notifyChanged();
            repaint();
        };

        const auto moduleKey = moduleType.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").toLowerCase();

        auto ensureProducerSamplerParams = [&] (bool includePads)
        {
            juce::StringArray params { "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount", "samplePitch",
                                       "sampleReverse", "sampleGlitch", "sampleGlitchGrid",
                                       "granularOn", "granularDensity", "granularSizeMs", "granularSpread", "granularScan",
                                       "filterCutoff", "filterResonance", "attack", "decay", "sustain", "release",
                                       "arpLaneRate", "arpLaneGate", "arpLaneSwing", "arpLaneProbability", "retrigger",
                                       "projectBpm", "bpmSync",
                                       "delayMix", "reverbMix",
                                       "multiTapTime", "multiTapFeedback", "multiTapSpread", "multiTapMix",
                                       "tapeDrive", "tapeTone", "tapeFlutter", "tapeMix",
                                       "vinylAge", "vinylDust", "vinylWarp", "vinylMix",
                                       "vocalFormant", "vocalBody", "vocalMix",
                                       "lofiBits", "lofiRate", "lofiMix",
                                       "dynThresholdDb", "dynRatio", "dynAttackMs", "dynReleaseMs", "dynMix",
                                       "stereoWidth", "monoMaker", "volume", "pan", "outputLimiter", "outputCeilingDb" };
            if (includePads)
            {
                for (int pad = 1; pad <= 16; ++pad)
                {
                    const auto padText = juce::String (pad);
                    params.add ("pad" + padText + "Volume");
                    params.add ("pad" + padText + "Pitch");
                    params.add ("pad" + padText + "Pan");
                }
            }

            ensureParams (params, "sample");
            liveValues.setValue ("outputLimiter", 1.0f);
            liveValues.setValue ("retrigger", 1.0f);
            liveValues.setValue ("bpmSync", 1.0f);
        };

        auto ensureProducerFxParams = [&]
        {
            ensureParams ({ "drive", "mix", "filterCutoff", "filterResonance",
                            "multiTapTime", "multiTapFeedback", "multiTapSpread", "multiTapMix",
                            "tapeDrive", "tapeTone", "tapeFlutter", "tapeMix",
                            "vinylAge", "vinylDust", "vinylWarp", "vinylMix",
                            "lofiBits", "lofiRate", "lofiMix",
                            "dynThresholdDb", "dynRatio", "dynAttackMs", "dynReleaseMs", "dynMix",
                            "stereoWidth", "monoMaker", "outputGainDb", "outputLimiter", "outputCeilingDb",
                            "arpLaneRate", "arpLaneSwing", "arpLaneProbability", "retrigger" }, "fx");
            liveValues.setValue ("outputLimiter", 1.0f);
            liveValues.setValue ("mix", 1.0f);
        };

        auto findGraphBlock = [&] (const juce::String& blockId) -> DspBlock*
        {
            for (auto& block : graph.blocks)
                if (block.id == blockId)
                    return &block;
            return nullptr;
        };

        auto ensureChordModuleParams = [&]
        {
            ensureParams ({ "arpLaneRate", "arpLaneGate", "arpLaneSwing", "arpLaneProbability",
                            "arpLanePatternLaunch", "arpLaneRetrigger", "retrigger",
                            "mpActiveBank", "mpProgressionPreset", "mpScaleRoot", "mpScaleType",
                            "mpChordMode", "mpChordSize", "mpChordSpread", "mpStrum",
                            "mpHumanize", "mpMutation", "mpProbability", "mpLatch", "mpSampleControl",
                            "filterCutoff", "delayMix", "reverbMix", "volume", "pan",
                            "outputLimiter", "outputCeilingDb" }, "synth");
            liveValues.setValue ("outputLimiter", 1.0f);
            liveValues.setValue ("retrigger", 1.0f);
        };

        auto ensureProgressionBlock = [&] (const juce::String& blockId,
                                           const juce::String& name,
                                           std::initializer_list<int> progressionIndexes,
                                           float rate,
                                           float swing,
                                           float strum,
                                           float humanize,
                                           float chordSpread) -> DspBlock*
        {
            ensureBlock (blockId, "mod", "midiPlayground", name, "filterCutoff", "midi", "chordProgression", "event",
                         { { "rate", rate }, { "sync", 1.0f }, { "arpSteps", 16.0f }, { "arpGate", 0.62f },
                           { "arpSwing", swing }, { "arpLaneRate", rate }, { "arpLaneSwing", swing },
                           { "arpLaneProbability", 1.0f }, { "mpProbability", 1.0f },
                           { "mpChordSize", 4.0f }, { "mpChordSpread", chordSpread },
                           { "mpStrum", strum }, { "mpHumanize", humanize }, { "mpMutation", 0.0f },
                           { "mpSampleControl", 0.0f }, { "retrigger", 1.0f } });

            auto* block = findGraphBlock (blockId);
            if (block == nullptr)
                return nullptr;

            int bank = 0;
            for (const auto progressionIndex : progressionIndexes)
            {
                if (bank >= MidiPlaygroundPattern::kPhraseBankCount)
                    break;
                MidiPlaygroundPattern::applyProgressionPreset (*block, progressionIndex, bank);
                ++bank;
            }

            if (bank == 0)
                MidiPlaygroundPattern::applyProgressionPreset (*block, 0, 0);

            MidiPlaygroundPattern::loadBank (*block, 0, false);
            block->name = name;
            block->targetId = "filterCutoff";
            block->metadata["family"] = "midi";
            block->metadata["role"] = "chordProgression";
            block->metadata["ioMode"] = "event";
            block->values["rate"] = rate;
            block->values["sync"] = 1.0f;
            block->values["arpSwing"] = swing;
            block->values["arpLaneRate"] = rate;
            block->values["arpLaneSwing"] = swing;
            block->values["arpLaneProbability"] = 1.0f;
            block->values["mpStrum"] = strum;
            block->values["mpHumanize"] = humanize;
            block->values["mpChordSpread"] = chordSpread;
            block->values["mpProbability"] = 1.0f;
            block->values["retrigger"] = 1.0f;
            graph.userConfigured = true;
            return block;
        };

        if (moduleKey == "startersynthplugin")
        {
            ensureParams ({ "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend",
                            "wtEnabled", "wtTable", "wtPosition", "wtMorph", "wtWarp", "wtUnison",
                            "wtDetune", "wtSpread", "wtLevel", "filterCutoff", "filterResonance",
                            "attack", "decay", "sustain", "release", "lfoRate", "lfoAmount",
                            "delayTime", "delayFeedback", "delayMix", "reverbMix", "stereoWidth",
                            "outputLimiter", "outputCeilingDb", "volume", "pan" }, "synth");
            liveValues.setValue ("oscType", 1.0f);
            liveValues.setValue ("osc2Type", 1.0f);
            liveValues.setValue ("noiseBlend", 0.0f);
            liveValues.setValue ("wtEnabled", 1.0f);
            liveValues.setValue ("wtLevel", 0.62f);
            liveValues.setValue ("outputLimiter", 1.0f);

            ensureBlock ("starter_synth_osc", "source", "oscStack", "Starter OSC Stack", "oscBlend", "starter", "source", "stereo",
                         { { "oscType", 1.0f }, { "osc2Type", 1.0f }, { "oscBlend", 0.22f }, { "osc2Detune", 7.0f },
                           { "subBlend", 0.08f }, { "noiseBlend", 0.0f }, { "volume", 0.72f } });
            ensureBlock ("starter_synth_wt", "source", "serumWavetable", "Starter Wavetable", "wtPosition", "starter", "source", "stereo",
                         { { "wtEnabled", 1.0f }, { "wtTable", 4.0f }, { "wtPosition", 0.28f }, { "wtMorph", 0.22f },
                           { "wtWarp", 0.18f }, { "wtUnison", 3.0f }, { "wtDetune", 12.0f }, { "wtSpread", 0.46f },
                           { "wtLevel", 0.62f } });
            ensureBlock ("starter_synth_filter", "filter", "stateVariable", "Starter Filter", "filterCutoff", "starter", "tone", "stereo",
                         { { "cutoff", 4200.0f }, { "resonance", 0.18f }, { "filterCutoff", 4200.0f }, { "filterResonance", 0.18f } });
            ensureBlock ("starter_synth_lfo", "mod", "lfo", "Starter Motion LFO", "filterCutoff", "starter", "modulation", "modulation",
                         { { "lfoRate", 3.0f }, { "lfoAmount", 0.18f } });
            ensureBlock ("starter_synth_delay", "fx", "delay", "Starter Delay", "delayMix", "starter", "space", "stereo",
                         { { "delayTime", 0.25f }, { "delayFeedback", 0.28f }, { "delayMix", 0.12f } });
            ensureBlock ("starter_synth_reverb", "fx", "reverb", "Starter Reverb", "reverbMix", "starter", "space", "stereo",
                         { { "reverbMix", 0.18f } });
            ensureBlock ("starter_synth_output", "out", "limiter", "Starter Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.78f } });

            addModulePanel ("Add Synth Plugin Starter", "Synth Plugin Starter", 920, 430,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Label, "SOURCE", juce::String(), 22, 28, 100, 24);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Wavetable", "wtPosition", 22, 58, 230, 82);
                    addChildKnob (layout, panelId, "WT Pos", "wtPosition", 268, 58);
                    addChildKnob (layout, panelId, "WT Morph", "wtMorph", 344, 58);
                    addChildKnob (layout, panelId, "Wave", "oscType", 420, 58);
                    addChildKnob (layout, panelId, "Blend", "oscBlend", 496, 58);
                    addChildKnob (layout, panelId, "Sub", "subBlend", 572, 58);
                    addChildKnob (layout, panelId, "Noise", "noiseBlend", 648, 58);
                    addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Spectrum", juce::String(), 736, 58, 160, 82);

                    addChildSurface (layout, panelId, ElementType::Label, "FILTER & ENVELOPE", juce::String(), 22, 158, 200, 24);
                    addChildSurface (layout, panelId, ElementType::EqCurve, "Filter EQ", "filterCutoff", 22, 182, 170, 92);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 204, 182);
                    addChildKnob (layout, panelId, "Res", "filterResonance", 280, 182);
                    addChildSurface (layout, panelId, ElementType::AdsrCurve, "ADSR Env", "attack", 368, 182, 190, 92);
                    addChildSlider (layout, panelId, "A", "attack", 578, 182);
                    addChildSlider (layout, panelId, "D", "decay", 618, 182);
                    addChildSlider (layout, panelId, "S", "sustain", 658, 182);
                    addChildSlider (layout, panelId, "R", "release", 698, 182);
                    addChildKnob (layout, panelId, "LFO Rate", "lfoRate", 750, 182);
                    addChildKnob (layout, panelId, "LFO Amt", "lfoAmount", 826, 182);

                    addChildSurface (layout, panelId, ElementType::Label, "MACROS & EFFECTS", juce::String(), 22, 292, 200, 24);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Tone", "filterCutoff", 22, 316, 110, 74);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Motion", "lfoAmount", 144, 316, 110, 74);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Space", "delayMix", 266, 316, 110, 74);
                    addChildKnob (layout, panelId, "Delay", "delayMix", 398, 316);
                    addChildKnob (layout, panelId, "Time", "delayTime", 468, 316);
                    addChildKnob (layout, panelId, "Reverb", "reverbMix", 538, 316);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 618, 316, 210, 74);
                    addChildKnob (layout, panelId, "Out", "volume", 844, 316, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterhybridinstrument")
        {
            ensureParams ({ "sampleStart", "sampleLength", "samplePitch", "sampleReverse",
                            "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "detune", "octave",
                            "wtEnabled", "wtTable", "wtPosition", "wtMorph", "wtWarp", "wtUnison",
                            "wtDetune", "wtSpread", "wtLevel",
                            "filterCutoff", "filterResonance", "attack", "decay", "sustain", "release",
                            "lfoRate", "lfoAmount", "delayTime", "delayFeedback", "delayMix", "reverbMix",
                            "stereoWidth", "volume", "pan", "outputLimiter", "outputCeilingDb",
                            "macro_motion", "macro_tone", "macro_character", "macro_space" }, "sample");
            liveValues.setValue ("sampleLength", 1.0f);
            liveValues.setValue ("oscBlend", 0.36f);
            liveValues.setValue ("subBlend", 0.10f);
            liveValues.setValue ("wtEnabled", 1.0f);
            liveValues.setValue ("wtLevel", 0.34f);
            liveValues.setValue ("filterCutoff", 5200.0f);
            liveValues.setValue ("filterResonance", 0.16f);
            liveValues.setValue ("delayMix", 0.14f);
            liveValues.setValue ("reverbMix", 0.24f);
            liveValues.setValue ("outputLimiter", 1.0f);

            ensureBlock ("starter_hybrid_sample", "source", "samplePlayer", "Hybrid Sample Layer", "sampleStart", "hybrid", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.78f } });
            ensureBlock ("starter_hybrid_osc", "source", "oscStack", "Hybrid Synth Layer", "oscBlend", "hybrid", "source", "stereo",
                         { { "oscType", 1.0f }, { "osc2Type", 2.0f }, { "oscBlend", 0.36f }, { "osc2Detune", 7.0f },
                           { "subBlend", 0.10f }, { "volume", 0.64f } });
            ensureBlock ("starter_hybrid_wt", "source", "serumWavetable", "Hybrid Wavetable Layer", "wtPosition", "hybrid", "source", "stereo",
                         { { "wtEnabled", 1.0f }, { "wtTable", 7.0f }, { "wtPosition", 0.32f }, { "wtMorph", 0.20f },
                           { "wtWarp", 0.16f }, { "wtUnison", 3.0f }, { "wtDetune", 10.0f }, { "wtSpread", 0.42f },
                           { "wtLevel", 0.34f } });
            ensureBlock ("starter_hybrid_filter", "filter", "stateVariable", "Hybrid Shared Filter", "filterCutoff", "hybrid", "tone", "stereo",
                         { { "filterCutoff", 5200.0f }, { "filterResonance", 0.16f }, { "attack", 0.08f }, { "release", 0.72f } });
            ensureBlock ("starter_hybrid_lfo", "mod", "lfo", "Hybrid Motion LFO", "filterCutoff", "hybrid", "modulation", "modulation",
                         { { "lfoRate", 1.4f }, { "lfoAmount", 0.12f } });
            ensureBlock ("starter_hybrid_delay", "fx", "delay", "Hybrid Delay", "delayMix", "hybrid", "space", "stereo",
                         { { "delayTime", 0.25f }, { "delayFeedback", 0.22f }, { "delayMix", 0.14f } });
            ensureBlock ("starter_hybrid_reverb", "fx", "reverb", "Hybrid Reverb", "reverbMix", "hybrid", "space", "stereo",
                         { { "reverbMix", 0.24f } });
            ensureBlock ("starter_hybrid_output", "out", "limiter", "Hybrid Output", "outputCeilingDb", "hybrid", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.82f } });

            addModulePanel ("Add Hybrid Synth + Sample Starter", "Hybrid Synth + Sample", 980, 560,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Label, "SAMPLE LAYER", juce::String(), 24, 28, 160, 24);
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 24, 60, 170, 150);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample", "sampleStart", 212, 60, 170, 150);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Waveform", "sampleStart", 400, 60, 260, 76);
                    addChildKnob (layout, panelId, "Start", "sampleStart", 402, 152);
                    addChildKnob (layout, panelId, "Length", "sampleLength", 484, 152);
                    addChildKnob (layout, panelId, "Tune", "samplePitch", 566, 152);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 668, 168, 82, 30);

                    addChildSurface (layout, panelId, ElementType::Label, "SYNTH LAYER", juce::String(), 24, 242, 160, 24);
                    addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Synth View", "oscBlend", 24, 274, 252, 86);
                    addChildKnob (layout, panelId, "Wave A", "oscType", 304, 274);
                    addChildKnob (layout, panelId, "Wave B", "osc2Type", 386, 274);
                    addChildKnob (layout, panelId, "Blend", "oscBlend", 468, 274);
                    addChildKnob (layout, panelId, "Sub", "subBlend", 550, 274);
                    addChildKnob (layout, panelId, "WT Pos", "wtPosition", 632, 274);
                    addChildKnob (layout, panelId, "WT Warp", "wtWarp", 714, 274);

                    addChildSurface (layout, panelId, ElementType::Label, "SHARED TONE + FX", juce::String(), 24, 398, 190, 24);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 24, 430);
                    addChildKnob (layout, panelId, "Res", "filterResonance", 106, 430);
                    addChildKnob (layout, panelId, "Attack", "attack", 188, 430);
                    addChildKnob (layout, panelId, "Release", "release", 270, 430);
                    addChildKnob (layout, panelId, "Motion", "lfoAmount", 352, 430);
                    addChildKnob (layout, panelId, "Delay", "delayMix", 434, 430);
                    addChildKnob (layout, panelId, "Reverb", "reverbMix", 516, 430);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Tone", "filterCutoff", 620, 420, 102, 78);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Space", "reverbMix", 734, 420, 102, 78);
                    addChildKnob (layout, panelId, "Out", "volume", 862, 438, 58);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 692, 60, 240, 78);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "startersamplerinstrument")
        {
            ensureParams ({ "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount", "samplePitch",
                            "sampleReverse", "sampleGlitch", "sampleGlitchGrid", "granularOn", "granularDensity",
                            "granularSizeMs", "granularSpread", "granularScan", "filterCutoff", "filterResonance",
                            "attack", "decay", "sustain", "release", "delayMix", "reverbMix", "volume", "pan",
                            "outputLimiter", "outputCeilingDb" }, "sample");
            liveValues.setValue ("granularOn", 0.0f);
            liveValues.setValue ("outputLimiter", 1.0f);

            ensureBlock ("starter_sampler_source", "source", "samplePlayer", "Starter Sample Player", "sampleStart", "starter", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.90f },
                           { "granularOn", 0.0f } });
            ensureBlock ("starter_sampler_filter", "filter", "stateVariable", "Starter Sampler Filter", "filterCutoff", "starter", "tone", "stereo",
                         { { "filterCutoff", 5200.0f }, { "filterResonance", 0.14f } });
            ensureBlock ("starter_sampler_granular", "source", "granularSampler", "Optional Granular Source", "granularDensity", "starter", "source", "stereo",
                         { { "granularOn", 0.0f }, { "granularDensity", 28.0f }, { "granularSizeMs", 90.0f }, { "granularSpread", 0.18f } });
            ensureBlock ("starter_sampler_fx", "fx", "reverb", "Starter Sampler Space", "reverbMix", "starter", "space", "stereo",
                         { { "delayMix", 0.10f }, { "reverbMix", 0.18f } });
            ensureBlock ("starter_sampler_output", "out", "limiter", "Starter Sampler Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.90f } });

            addModulePanel ("Add Sampler Instrument Starter", "Sampler Instrument Starter", 920, 430,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 22, 42, 180, 156);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample", "sampleStart", 214, 42, 180, 156);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Waveform", "sampleStart", 406, 42, 310, 78);
                    addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Spectrum", juce::String(), 728, 42, 170, 78);
                    addChildKnob (layout, panelId, "Start", "sampleStart", 406, 130);
                    addChildKnob (layout, panelId, "Length", "sampleLength", 476, 130);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 546, 130);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 618, 146, 82, 30);
                    addChildToggle (layout, panelId, "Limiter", "outputLimiter", 698, 146, 82, 30);

                    addChildSurface (layout, panelId, ElementType::Label, "MAP + PLAY", juce::String(), 22, 212, 120, 24);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 22, 244, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                    
                    addChildSurface (layout, panelId, ElementType::EqCurve, "Filter EQ", "filterCutoff", 214, 230, 160, 90);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 386, 230);
                    addChildKnob (layout, panelId, "Res", "filterResonance", 456, 230);
                    addChildKnob (layout, panelId, "Delay", "delayMix", 386, 310);
                    addChildKnob (layout, panelId, "Verb", "reverbMix", 456, 310);

                    addChildSurface (layout, panelId, ElementType::AdsrCurve, "ADSR Env", "attack", 536, 230, 170, 90);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 536, 330, 170, 64);

                    addChildSurface (layout, panelId, ElementType::GranularField, "Granular", "granularScan", 728, 230, 170, 90);
                    addChildKnob (layout, panelId, "Grain", "granularDensity", 728, 330);
                    addChildKnob (layout, panelId, "Scan", "granularScan", 798, 330);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "startereasysamplerworkstation")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("sampleSliceCount", 16.0f);
            liveValues.setValue ("granularOn", 0.0f);
            liveValues.setValue ("bpmSync", 1.0f);
            liveValues.setValue ("multiTapMix", 0.08f);
            liveValues.setValue ("reverbMix", 0.14f);

            ensureBlock ("starter_easy_sampler_source", "source", "samplePlayer", "Easy Sampler Source", "sampleStart", "starter", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.86f },
                           { "bpmSync", 1.0f }, { "granularOn", 0.0f } });
            ensureBlock ("starter_easy_sampler_chop", "source", "sliceChop", "Easy Sampler Slices", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 16.0f }, { "sampleLength", 0.25f }, { "sampleGlitchGrid", 16.0f } });
            ensureBlock ("starter_easy_sampler_filter", "filter", "stateVariable", "Easy Sampler Filter", "filterCutoff", "starter", "tone", "stereo",
                         { { "filterCutoff", 6800.0f }, { "filterResonance", 0.12f } });
            ensureBlock ("starter_easy_sampler_midi", "mod", "midiPlayground", "Easy Sampler MIDI", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.62f }, { "arpLaneSwing", 0.04f }, { "arpLaneProbability", 1.0f },
                           { "mpSampleControl", 1.0f }, { "sampleSliceCount", 16.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_easy_sampler_space", "fx", "multiTapDelay", "Easy Sampler Space", "multiTapMix", "creative", "space", "stereo",
                         { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.18f }, { "multiTapSpread", 0.30f }, { "multiTapMix", 0.08f } });
            ensureBlock ("starter_easy_sampler_output", "out", "limiter", "Easy Sampler Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });

            addModulePanel ("Add Easy Sampler Workstation", "Easy Sampler Workstation", 900, 520,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 20, 36, 166, 158);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Audio", "sampleStart", 204, 36, 166, 158);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Edit Sample", "sampleStart", 390, 36, 302, 82);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 714, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));

                    addChildKnob (layout, panelId, "Start", "sampleStart", 394, 136);
                    addChildKnob (layout, panelId, "Length", "sampleLength", 476, 136);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 558, 136);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 640, 136);
                    addChildToggle (layout, panelId, "Sync", "bpmSync", 722, 204, 74, 30);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 804, 204, 76, 30);

                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern / MIDI", "arpLaneRate", 20, 230, 446, 172);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 20, 430, 360, 58);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 500, 244);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 582, 244);
                    addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 664, 244);
                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 500, 336);
                    addChildKnob (layout, panelId, "Count", "sampleSliceCount", 582, 336);
                    addChildKnob (layout, panelId, "Echo", "multiTapMix", 664, 336);
                    addChildKnob (layout, panelId, "Out", "volume", 768, 350, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "startervocalchopinstrument")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("sampleSliceCount", 12.0f);
            liveValues.setValue ("vocalMix", 0.10f);
            liveValues.setValue ("multiTapMix", 0.10f);
            liveValues.setValue ("dynMix", 0.18f);
            liveValues.setValue ("bpmSync", 1.0f);

            ensureBlock ("starter_vocal_chop_source", "source", "sliceChop", "Vocal Chop Slicer", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 12.0f }, { "sampleLength", 0.20f }, { "samplePitch", 0.0f },
                           { "sampleGlitchGrid", 12.0f }, { "bpmSync", 1.0f }, { "volume", 0.82f } });
            ensureBlock ("starter_vocal_chop_midi", "mod", "midiPlayground", "Vocal Chop MIDI", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.58f }, { "arpLaneSwing", 0.06f }, { "arpLaneProbability", 1.0f },
                           { "mpSampleControl", 1.0f }, { "sampleSliceCount", 12.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_vocal_chop_formant", "fx", "vocalFormant", "Vocal Color", "vocalMix", "creative", "tone", "stereo",
                         { { "vocalFormant", 0.42f }, { "vocalBody", 0.32f }, { "vocalMix", 0.10f } });
            ensureBlock ("starter_vocal_chop_dynamics", "fx", "dynamics", "Vocal Chop Leveler", "dynMix", "studio", "dynamics", "stereo",
                         { { "dynThresholdDb", -20.0f }, { "dynRatio", 2.0f }, { "dynAttackMs", 8.0f }, { "dynReleaseMs", 120.0f }, { "dynMix", 0.18f } });
            ensureBlock ("starter_vocal_chop_delay", "fx", "multiTapDelay", "Vocal Chop Delay", "multiTapMix", "creative", "space", "stereo",
                         { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.20f }, { "multiTapSpread", 0.38f }, { "multiTapMix", 0.10f } });
            ensureBlock ("starter_vocal_chop_output", "out", "limiter", "Vocal Chop Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.82f } });

            addModulePanel ("Add Vocal Chop Instrument Starter", "Vocal Chop Instrument", 880, 500,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Vocal Takes", juce::String(), 20, 36, 150, 150);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Vocal", "sampleStart", 188, 36, 150, 150);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Chop Markers", "sampleSlice", 356, 36, 318, 80);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Chops", "padGrid", 700, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));

                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 362, 134);
                    addChildKnob (layout, panelId, "Count", "sampleSliceCount", 444, 134);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 526, 134);
                    addChildKnob (layout, panelId, "Formant", "vocalFormant", 608, 134);
                    addChildToggle (layout, panelId, "Sync", "bpmSync", 704, 196, 70, 30);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 784, 196, 76, 30);

                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Chop Performance", "arpLaneRate", 20, 230, 456, 164);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Playable Range", juce::String(), 20, 422, 360, 56);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 506, 242);
                    addChildKnob (layout, panelId, "Gate", "arpLaneGate", 588, 242);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 670, 242);
                    addChildKnob (layout, panelId, "Comp", "dynMix", 506, 334);
                    addChildKnob (layout, panelId, "Delay", "multiTapMix", 588, 334);
                    addChildKnob (layout, panelId, "Out", "volume", 670, 334, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterbeateditingsampler")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("sampleSliceCount", 32.0f);
            liveValues.setValue ("bpmSync", 1.0f);
            liveValues.setValue ("tapeMix", 0.08f);
            liveValues.setValue ("lofiMix", 0.05f);

            ensureBlock ("starter_beat_editor_source", "source", "sliceChop", "Beat Editor Slicer", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 32.0f }, { "sampleLength", 0.18f }, { "sampleGlitchGrid", 32.0f },
                           { "bpmSync", 1.0f }, { "volume", 0.84f } });
            ensureBlock ("starter_beat_editor_pads", "source", "drumRack", "Beat Editor Pad Rack", "pad1Volume", "starter", "source", "stereo",
                         { { "pad1Volume", 1.0f }, { "pad2Volume", 0.92f }, { "pad3Volume", 0.86f }, { "pad4Volume", 0.86f }, { "volume", 0.82f } });
            ensureBlock ("starter_beat_editor_midi", "mod", "drumSequencer", "Beat Editor Pattern", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.08f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_beat_editor_tape", "fx", "tape", "Beat Editor Tape", "tapeMix", "creative", "tone", "stereo",
                         { { "tapeDrive", 0.16f }, { "tapeTone", 0.46f }, { "tapeFlutter", 0.04f }, { "tapeMix", 0.08f } });
            ensureBlock ("starter_beat_editor_lofi", "fx", "lofi", "Beat Editor LoFi", "lofiMix", "creative", "destruction", "stereo",
                         { { "lofiBits", 14.0f }, { "lofiRate", 0.10f }, { "lofiMix", 0.05f } });
            ensureBlock ("starter_beat_editor_output", "out", "limiter", "Beat Editor Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });

            addModulePanel ("Add Beat Editing Sampler Starter", "Beat Editing Sampler", 920, 520,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Beat Crate", juce::String(), 20, 36, 154, 150);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Beat", "sampleStart", 192, 36, 154, 150);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Beat Slices", "sampleSlice", 366, 36, 332, 80);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Slice Pads", "padGrid", 724, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));

                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 372, 134);
                    addChildKnob (layout, panelId, "Count", "sampleSliceCount", 454, 134);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 536, 134);
                    addChildKnob (layout, panelId, "Glitch", "sampleGlitch", 618, 134);
                    addChildToggle (layout, panelId, "Sync", "bpmSync", 724, 198, 70, 30);
                    addChildToggle (layout, panelId, "Retrig", "retrigger", 804, 198, 76, 30);

                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Beat Pattern", "arpLaneRate", 20, 232, 520, 176);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 20, 434, 364, 56);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 574, 244);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 656, 244);
                    addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 738, 244);
                    addChildKnob (layout, panelId, "Tape", "tapeMix", 574, 336);
                    addChildKnob (layout, panelId, "LoFi", "lofiMix", 656, 336);
                    addChildKnob (layout, panelId, "Out", "volume", 738, 336, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterdrummachine")
        {
            juce::StringArray drumParams { "volume", "pan", "outputLimiter", "outputCeilingDb",
                                           "arpLaneRate", "arpLaneSwing", "arpLaneProbability", "retrigger" };
            for (int pad = 1; pad <= 16; ++pad)
            {
                const auto padText = juce::String (pad);
                drumParams.add ("pad" + padText + "Volume");
                drumParams.add ("pad" + padText + "Pitch");
                drumParams.add ("pad" + padText + "Pan");
            }
            ensureParams (drumParams, "sample");
            liveValues.setValue ("outputLimiter", 1.0f);
            liveValues.setValue ("retrigger", 1.0f);

            ensureBlock ("starter_drum_rack", "source", "drumRack", "Starter Drum Rack", "pad1Volume", "starter", "source", "stereo",
                         { { "pad1Volume", 1.0f }, { "pad2Volume", 1.0f }, { "pad3Volume", 1.0f }, { "pad4Volume", 1.0f }, { "volume", 0.90f } });
            ensureBlock ("starter_drum_seq", "mod", "drumSequencer", "Starter Drum Sequencer", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.04f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_drum_mixer", "out", "drumMixer", "Starter Drum Mixer", "pad1Volume", "starter", "mix", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.90f } });

            addModulePanel ("Add Drum Machine Starter", "Drum Machine Starter", 960, 430,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 22, 38, 160, 140);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Drum Samples", "sampleStart", 198, 38, 160, 140);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 374, 38, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                    addChildSlider (layout, panelId, "Kick", "pad1Volume", 570, 38);
                    addChildSlider (layout, panelId, "Snare", "pad2Volume", 610, 38);
                    addChildSlider (layout, panelId, "Hat", "pad3Volume", 650, 38);
                    addChildSlider (layout, panelId, "Clap", "pad4Volume", 690, 38);
                    addChildSlider (layout, panelId, "Perc 1", "pad5Volume", 730, 38);
                    addChildSlider (layout, panelId, "Perc 2", "pad6Volume", 770, 38);
                    addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Spectrum", juce::String(), 816, 38, 120, 140);

                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern Grid", "arpLaneRate", 22, 204, 520, 190);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 568, 204);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 638, 204);
                    addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 708, 204);
                    addChildKnob (layout, panelId, "Out", "volume", 794, 204, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterscratchslice")
        {
            ensureParams ({ "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount", "samplePitch",
                            "sampleReverse", "sampleGlitch", "sampleGlitchGrid", "multiTapTime", "multiTapFeedback",
                            "multiTapSpread", "multiTapMix", "filterCutoff", "outputLimiter", "outputCeilingDb",
                            "volume" }, "sample");
            liveValues.setValue ("sampleSliceCount", 16.0f);
            liveValues.setValue ("outputLimiter", 1.0f);

            ensureBlock ("starter_scratch_deck", "source", "scratchDeck", "Starter Scratch Deck", "sampleStart", "starter", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "sampleSliceCount", 16.0f }, { "samplePitch", 0.0f } });
            ensureBlock ("starter_scratch_chop", "source", "sliceChop", "Starter Slice Engine", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 16.0f }, { "sampleLength", 0.25f }, { "sampleGlitchGrid", 16.0f } });
            ensureBlock ("starter_scratch_delay", "fx", "multiTapDelay", "Starter Performance Delay", "multiTapMix", "starter", "space", "stereo",
                         { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.24f }, { "multiTapSpread", 0.42f }, { "multiTapMix", 0.18f } });
            ensureBlock ("starter_scratch_output", "out", "limiter", "Starter Scratch Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });

            addModulePanel ("Add Scratch Slice Starter", "Scratch / Slice Starter", 760, 390,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 22, 38, 150, 126);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 190, 38, 150, 126);
                    addChildSurface (layout, panelId, ElementType::XYPad, "Scratch Pad", "sampleStart", 360, 38, 150, 126);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Slices", "sampleSlice", 532, 38, 190, 74);
                    addChildKnob (layout, panelId, "Start", "sampleStart", 40, 204);
                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 122, 204);
                    addChildKnob (layout, panelId, "Count", "sampleSliceCount", 204, 204);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 286, 204);
                    addChildKnob (layout, panelId, "Glitch", "sampleGlitch", 368, 204);
                    addChildKnob (layout, panelId, "Delay", "multiTapMix", 450, 204);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Trigger Pads", "padGrid", 548, 174, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterhiphopsampler")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("sampleSliceCount", 16.0f);
            liveValues.setValue ("vinylMix", 0.16f);
            liveValues.setValue ("lofiMix", 0.10f);
            liveValues.setValue ("multiTapMix", 0.08f);

            ensureBlock ("starter_hiphop_source", "source", "samplePlayer", "Hip Hop Sample Source", "sampleStart", "starter", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.78f } });
            ensureBlock ("starter_hiphop_chop", "source", "sliceChop", "Hip Hop Chop Engine", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 16.0f }, { "sampleLength", 0.25f }, { "sampleGlitchGrid", 16.0f } });
            ensureBlock ("starter_hiphop_pads", "source", "drumRack", "Hip Hop Pad Rack", "pad1Volume", "starter", "source", "stereo",
                         { { "pad1Volume", 1.0f }, { "pad2Volume", 1.0f }, { "pad3Volume", 1.0f }, { "pad4Volume", 1.0f }, { "volume", 0.82f } });
            ensureBlock ("starter_hiphop_midi", "mod", "midiPlayground", "Hip Hop MIDI Chops", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.55f }, { "arpLaneSwing", 0.08f }, { "arpLaneProbability", 1.0f },
                           { "mpSampleControl", 1.0f }, { "sampleSliceCount", 16.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_hiphop_vinyl", "fx", "vinyl", "Vinyl Texture", "vinylMix", "creative", "destruction", "stereo",
                         { { "vinylAge", 0.34f }, { "vinylDust", 0.05f }, { "vinylWarp", 0.08f }, { "vinylMix", 0.16f } });
            ensureBlock ("starter_hiphop_lofi", "fx", "lofi", "Sampler LoFi", "lofiMix", "creative", "destruction", "stereo",
                         { { "lofiBits", 13.0f }, { "lofiRate", 0.16f }, { "lofiMix", 0.10f } });
            ensureBlock ("starter_hiphop_output", "out", "limiter", "Hip Hop Sampler Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });

            addModulePanel ("Add Hip Hop Sampler Starter", "Hip Hop Sampler Starter", 860, 500,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 20, 36, 160, 164);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample / Loop", "sampleStart", 196, 36, 166, 164);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Chop View", "sampleSlice", 382, 36, 276, 82);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "16 Pads", "padGrid", 680, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));

                    addChildKnob (layout, panelId, "Start", "sampleStart", 386, 136);
                    addChildKnob (layout, panelId, "Length", "sampleLength", 468, 136);
                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 550, 136);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 632, 136);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 720, 204, 88, 30);

                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern / MIDI Chops", "arpLaneRate", 20, 234, 498, 184);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 546, 242);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 628, 242);
                    addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 710, 242);
                    addChildKnob (layout, panelId, "Pad Tune", "pad1Pitch", 546, 328);
                    addChildKnob (layout, panelId, "Vinyl", "vinylMix", 628, 328);
                    addChildKnob (layout, panelId, "LoFi", "lofiMix", 710, 328);
                    addChildKnob (layout, panelId, "Out", "volume", 792, 328, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterchoplab")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("sampleSliceCount", 32.0f);
            liveValues.setValue ("tapeMix", 0.12f);
            liveValues.setValue ("multiTapMix", 0.14f);

            ensureBlock ("starter_chop_source", "source", "sliceChop", "Chop Lab Slice Engine", "sampleSlice", "starter", "source", "stereo",
                         { { "sampleSlice", 0.0f }, { "sampleSliceCount", 32.0f }, { "sampleLength", 0.18f }, { "sampleGlitchGrid", 32.0f } });
            ensureBlock ("starter_chop_scratch", "source", "scratchDeck", "Chop Lab Scratch Deck", "sampleStart", "starter", "source", "stereo",
                         { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.70f } });
            ensureBlock ("starter_chop_midi", "mod", "midiPlayground", "Chop Lab MIDI Trigger", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 2.0f }, { "arpLaneGate", 0.48f }, { "arpLaneSwing", 0.06f }, { "arpLaneProbability", 1.0f },
                           { "mpSampleControl", 1.0f }, { "sampleSliceCount", 32.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_chop_tape", "fx", "tape", "Chop Lab Tape", "tapeMix", "creative", "tone", "stereo",
                         { { "tapeDrive", 0.22f }, { "tapeTone", 0.50f }, { "tapeFlutter", 0.08f }, { "tapeMix", 0.12f } });
            ensureBlock ("starter_chop_delay", "fx", "multiTapDelay", "Chop Lab Echo", "multiTapMix", "creative", "space", "stereo",
                         { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.22f }, { "multiTapSpread", 0.36f }, { "multiTapMix", 0.14f } });
            ensureBlock ("starter_chop_output", "out", "limiter", "Chop Lab Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });

            addModulePanel ("Add Chop Lab Starter", "Chop Lab Starter", 860, 470,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Crate", juce::String(), 20, 34, 150, 134);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 188, 34, 150, 134);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Slice Markers", "sampleSlice", 356, 34, 312, 84);
                    addChildSurface (layout, panelId, ElementType::XYPad, "Scratch / Scrub", "sampleStart", 690, 34, 136, 134);

                    addChildSurface (layout, panelId, ElementType::PadGrid, "Slice Pads", "padGrid", 20, 204, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Chop Pattern", "arpLaneRate", 224, 204, 430, 166);
                    addChildKnob (layout, panelId, "Slice", "sampleSlice", 678, 204);
                    addChildKnob (layout, panelId, "Count", "sampleSliceCount", 760, 204);
                    addChildKnob (layout, panelId, "Glitch", "sampleGlitch", 678, 290);
                    addChildKnob (layout, panelId, "Tape", "tapeMix", 760, 290);
                    addChildKnob (layout, panelId, "Echo", "multiTapMix", 678, 376);
                    addChildKnob (layout, panelId, "Out", "volume", 760, 376, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "startermpcpads")
        {
            ensureProducerSamplerParams (true);
            liveValues.setValue ("lofiMix", 0.08f);
            liveValues.setValue ("tapeMix", 0.10f);

            ensureBlock ("starter_mpc_rack", "source", "drumRack", "MPC Pad Rack", "pad1Volume", "starter", "source", "stereo",
                         { { "pad1Volume", 1.0f }, { "pad2Volume", 1.0f }, { "pad3Volume", 1.0f }, { "pad4Volume", 1.0f }, { "volume", 0.86f } });
            ensureBlock ("starter_mpc_seq", "mod", "drumSequencer", "MPC Pad Sequencer", "arpLaneRate", "starter", "sequencer", "event",
                         { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.08f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
            ensureBlock ("starter_mpc_tape", "fx", "tape", "MPC Tape", "tapeMix", "creative", "tone", "stereo",
                         { { "tapeDrive", 0.20f }, { "tapeTone", 0.48f }, { "tapeFlutter", 0.06f }, { "tapeMix", 0.10f } });
            ensureBlock ("starter_mpc_lofi", "fx", "lofi", "MPC Crunch", "lofiMix", "creative", "destruction", "stereo",
                         { { "lofiBits", 13.0f }, { "lofiRate", 0.12f }, { "lofiMix", 0.08f } });
            ensureBlock ("starter_mpc_mixer", "out", "drumMixer", "MPC Pad Mixer", "pad1Volume", "starter", "mix", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });

            addModulePanel ("Add MPC Pad Instrument Starter", "MPC Pad Instrument Starter", 820, 500,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 20, 36, 158, 144);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Pads", "sampleStart", 196, 36, 158, 144);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "MPC Pads", "padGrid", 382, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Pad Pattern", "arpLaneRate", 20, 218, 328, 180);

                    addChildSlider (layout, panelId, "Pad 1", "pad1Volume", 660, 52);
                    addChildSlider (layout, panelId, "Pad 2", "pad2Volume", 706, 52);
                    addChildSlider (layout, panelId, "Pad 3", "pad3Volume", 752, 52);
                    addChildKnob (layout, panelId, "Tune 1", "pad1Pitch", 390, 330);
                    addChildKnob (layout, panelId, "Tune 2", "pad2Pitch", 472, 330);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 554, 330);
                    addChildKnob (layout, panelId, "Tape", "tapeMix", 636, 330);
                    addChildKnob (layout, panelId, "LoFi", "lofiMix", 718, 330);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterloopremixfx")
        {
            ensureProducerFxParams();
            liveValues.setValue ("multiTapMix", 0.24f);
            liveValues.setValue ("lofiMix", 0.08f);

            ensureBlock ("starter_loopfx_input", "source", "liveInput", "Loop Remix Input", "drive", "starter", "source", "stereo",
                         { { "drive", 0.0f }, { "mix", 1.0f } });
            ensureBlock ("starter_loopfx_delay", "fx", "multiTapDelay", "Loop Remix Delay", "multiTapMix", "creative", "space", "stereo",
                         { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.28f }, { "multiTapSpread", 0.52f }, { "multiTapMix", 0.24f } });
            ensureBlock ("starter_loopfx_lofi", "fx", "lofi", "Loop Remix LoFi", "lofiMix", "creative", "destruction", "stereo",
                         { { "lofiBits", 13.0f }, { "lofiRate", 0.10f }, { "lofiMix", 0.08f } });
            ensureBlock ("starter_loopfx_tape", "fx", "tape", "Loop Remix Tape", "tapeMix", "creative", "tone", "stereo",
                         { { "tapeDrive", 0.18f }, { "tapeTone", 0.52f }, { "tapeFlutter", 0.05f }, { "tapeMix", 0.10f } });
            ensureBlock ("starter_loopfx_duck", "fx", "dynamics", "Loop Remix Ducking", "dynMix", "studio", "dynamics", "stereo",
                         { { "dynThresholdDb", -20.0f }, { "dynRatio", 2.0f }, { "dynAttackMs", 8.0f }, { "dynReleaseMs", 140.0f }, { "dynMix", 0.18f } });
            ensureBlock ("starter_loopfx_output", "out", "limiter", "Loop Remix Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });

            addModulePanel ("Add Loop Remix FX Starter", "Loop Remix FX Starter", 800, 420,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Meter, "Input", "drive", 20, 44, 62, 250);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Loop Motion", "multiTapMix", 110, 44, 260, 92);
                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Gate / Repeat Pattern", "arpLaneRate", 110, 174, 330, 150);
                    addChildKnob (layout, panelId, "Time", "multiTapTime", 474, 54);
                    addChildKnob (layout, panelId, "Feedback", "multiTapFeedback", 556, 54);
                    addChildKnob (layout, panelId, "Mix", "multiTapMix", 638, 54);
                    addChildKnob (layout, panelId, "LoFi", "lofiMix", 474, 174);
                    addChildKnob (layout, panelId, "Tape", "tapeMix", 556, 174);
                    addChildKnob (layout, panelId, "Duck", "dynMix", 638, 174);
                    addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 718, 44, 62, 250);
                    addChildToggle (layout, panelId, "Limiter", "outputLimiter", 622, 314, 88, 30);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterchordprogressionplugin")
        {
            ensureChordModuleParams();
            ensureParams ({ "oscType", "osc2Type", "oscBlend", "noiseBlend", "wtEnabled", "wtPosition",
                            "wtLevel", "filterCutoff", "filterResonance", "attack", "release",
                            "delayMix", "reverbMix", "volume", "outputLimiter", "outputCeilingDb" }, "synth");
            liveValues.setValue ("oscType", 1.0f);
            liveValues.setValue ("osc2Type", 1.0f);
            liveValues.setValue ("noiseBlend", 0.0f);
            liveValues.setValue ("wtEnabled", 1.0f);
            liveValues.setValue ("wtLevel", 0.58f);
            liveValues.setValue ("outputLimiter", 1.0f);

            ensureProgressionBlock ("starter_chord_progression_midi", "Chord Progression Engine",
                                    { 7, 8, 13, 20, 21, 27, 28, 10 }, 1.0f, 0.08f, 0.16f, 0.04f, 0.64f);
            ensureBlock ("starter_chord_preview_source", "source", "serumWavetable", "Chord Preview Synth", "wtPosition", "starter", "source", "stereo",
                         { { "wtEnabled", 1.0f }, { "wtTable", 2.0f }, { "wtPosition", 0.24f }, { "wtMorph", 0.12f },
                           { "wtUnison", 2.0f }, { "wtDetune", 6.0f }, { "wtSpread", 0.34f }, { "wtLevel", 0.58f },
                           { "noiseBlend", 0.0f }, { "volume", 0.72f } });
            ensureBlock ("starter_chord_filter", "filter", "stateVariable", "Chord Tone Filter", "filterCutoff", "starter", "tone", "stereo",
                         { { "filterCutoff", 5200.0f }, { "filterResonance", 0.12f } });
            ensureBlock ("starter_chord_space", "fx", "reverb", "Chord Space", "reverbMix", "starter", "space", "stereo",
                         { { "delayMix", 0.08f }, { "reverbMix", 0.18f } });
            ensureBlock ("starter_chord_output", "out", "limiter", "Chord Plugin Output", "outputCeilingDb", "starter", "output", "stereo",
                         { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.78f } });

            addModulePanel ("Add Chord Progression Plugin Starter", "Chord Progression Plugin Starter", 900, 500,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Label, "PROGRESSIONS", juce::String(), 22, 28, 150, 24);
                    addChildSurface (layout, panelId, ElementType::ArpLane, "Chord Banks", "arpLaneRate", 22, 58, 248, 250);
                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Chord Timeline", "arpLaneRate", 296, 58, 356, 164);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Chord Pads", "mpActiveBank", 676, 58, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                    addChildValue (layout, panelId, "Progression", "mpProgressionPreset", 296, 242, 110, 30);
                    addChildValue (layout, panelId, "Root", "mpScaleRoot", 420, 242, 70, 30);
                    addChildValue (layout, panelId, "Scale", "mpScaleType", 504, 242, 70, 30);
                    addChildValue (layout, panelId, "Chord", "mpChordMode", 588, 242, 70, 30);

                    addChildSurface (layout, panelId, ElementType::Keyboard, "Preview Keys", juce::String(), 22, 344, 330, 70);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 382, 330);
                    addChildKnob (layout, panelId, "Spread", "mpChordSpread", 464, 330);
                    addChildKnob (layout, panelId, "Strum", "mpStrum", 546, 330);
                    addChildKnob (layout, panelId, "Human", "mpHumanize", 628, 330);
                    addChildKnob (layout, panelId, "Tone", "filterCutoff", 710, 330);
                    addChildKnob (layout, panelId, "Space", "reverbMix", 792, 330);
                    addChildToggle (layout, panelId, "Latch", "mpLatch", 382, 424, 82, 28);
                    addChildToggle (layout, panelId, "Retrig", "retrigger", 478, 424, 82, 28);
                    addChildValue (layout, panelId, "Drag MIDI", "mpActiveBank", 588, 424, 106, 28);
                    addChildKnob (layout, panelId, "Out", "volume", 792, 420, 58);
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "starterdelayfx" || moduleKey == "startervocalmasterfx")
        {
            ensureParams ({ "drive", "mix", "eqEnabled", "eqMix", "eqBand1On", "eqBand1Freq", "eqBand1GainDb",
                            "eqBand1Q", "eqBand2On", "eqBand2Freq", "eqBand2GainDb", "eqBand2Q",
                            "delayTime", "delayFeedback", "delayMix", "multiTapTime", "multiTapFeedback",
                            "multiTapSpread", "multiTapMix", "dynThresholdDb", "dynRatio", "dynAttackMs",
                            "dynReleaseMs", "dynMakeupDb", "dynMix", "vocalFormant", "vocalBody", "vocalMix",
                            "stereoWidth", "monoMaker", "outputLimiter", "outputCeilingDb", "outputGainDb" }, "fx");
            liveValues.setValue ("outputLimiter", 1.0f);
            liveValues.setValue ("eqEnabled", 1.0f);

            if (moduleKey == "starterdelayfx")
            {
                ensureBlock ("starter_delay_input", "source", "liveInput", "Starter FX Input", "drive", "starter", "source", "stereo",
                             { { "drive", 0.0f }, { "mix", 1.0f } });
                ensureBlock ("starter_delay_eq", "filter", "dynamicEq", "Starter Delay EQ", "eqMix", "starter", "tone", "stereo",
                             { { "eqEnabled", 1.0f }, { "eqMix", 1.0f }, { "eqBand1On", 1.0f }, { "eqBand1Freq", 180.0f },
                               { "eqBand1GainDb", -1.0f }, { "eqBand2On", 1.0f }, { "eqBand2Freq", 6500.0f }, { "eqBand2GainDb", -0.5f } });
                ensureBlock ("starter_delay_main", "fx", "multiTapDelay", "Starter MultiTap Delay", "multiTapMix", "starter", "space", "stereo",
                             { { "multiTapTime", 0.375f }, { "multiTapFeedback", 0.34f }, { "multiTapSpread", 0.55f }, { "multiTapMix", 0.32f } });
                ensureBlock ("starter_delay_duck", "fx", "dynamics", "Starter Ducking", "dynMix", "starter", "dynamics", "stereo",
                             { { "dynThresholdDb", -22.0f }, { "dynRatio", 2.0f }, { "dynAttackMs", 8.0f }, { "dynReleaseMs", 160.0f }, { "dynMix", 0.22f } });
                ensureBlock ("starter_delay_output", "out", "limiter", "Starter Delay Output", "outputCeilingDb", "starter", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });

                addModulePanel ("Add Delay FX Starter", "Delay FX Starter", 800, 430,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Meter, "Input", "drive", 24, 44, 60, 260);
                        addChildSurface (layout, panelId, ElementType::EqCurve, "Tone EQ", "eqMix", 108, 44, 220, 110);
                        addChildKnob (layout, panelId, "Time", "multiTapTime", 358, 54);
                        addChildKnob (layout, panelId, "Feedback", "multiTapFeedback", 440, 54);
                        addChildKnob (layout, panelId, "Spread", "multiTapSpread", 522, 54);
                        addChildKnob (layout, panelId, "Mix", "multiTapMix", 604, 54);
                        
                        addChildSurface (layout, panelId, ElementType::Waveform, "Feedback Visualizer", "multiTapFeedback", 108, 184, 220, 110);
                        addChildKnob (layout, panelId, "Duck", "dynMix", 358, 184);
                        addChildKnob (layout, panelId, "Width", "stereoWidth", 440, 184);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 522, 184);
                        addChildToggle (layout, panelId, "Limiter", "outputLimiter", 604, 200, 76, 30);
                        
                        addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 716, 44, 60, 260);
                    });
            }
            else
            {
                ensureBlock ("starter_vocal_input", "source", "liveInput", "Starter FX Input", "drive", "starter", "source", "stereo",
                             { { "drive", 0.0f }, { "mix", 1.0f } });
                ensureBlock ("starter_vocal_eq", "filter", "dynamicEq", "Starter Vocal EQ", "eqMix", "starter", "tone", "stereo",
                             { { "eqEnabled", 1.0f }, { "eqMix", 1.0f }, { "eqBand1On", 1.0f }, { "eqBand1Freq", 120.0f },
                               { "eqBand1GainDb", -1.2f }, { "eqBand2On", 1.0f }, { "eqBand2Freq", 3200.0f }, { "eqBand2GainDb", 1.4f } });
                ensureBlock ("starter_vocal_dynamics", "fx", "dynamics", "Starter Vocal Dynamics", "dynMix", "starter", "dynamics", "stereo",
                             { { "dynThresholdDb", -18.0f }, { "dynRatio", 2.4f }, { "dynAttackMs", 7.0f }, { "dynReleaseMs", 120.0f }, { "dynMix", 0.36f } });
                ensureBlock ("starter_vocal_formant", "fx", "vocalFormant", "Starter Vocal Character", "vocalMix", "starter", "tone", "stereo",
                             { { "vocalFormant", 0.40f }, { "vocalBody", 0.35f }, { "vocalMix", 0.12f } });
                ensureBlock ("starter_vocal_output", "out", "limiter", "Starter Master Output", "outputCeilingDb", "starter", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });

                addModulePanel ("Add Vocal Master FX Starter", "Vocal / Master FX Starter", 800, 430,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Meter, "Input", "drive", 24, 44, 60, 260);
                        addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Spectrum Analyzer", "eqMix", 108, 44, 220, 110);
                        addChildSurface (layout, panelId, ElementType::EqCurve, "Visual EQ", "eqMix", 108, 184, 220, 110);
                        
                        addChildKnob (layout, panelId, "Low Cut", "eqBand1Freq", 358, 54);
                        addChildKnob (layout, panelId, "Presence", "eqBand2GainDb", 440, 54);
                        addChildKnob (layout, panelId, "Compressor", "dynMix", 522, 54);
                        
                        addChildKnob (layout, panelId, "Formant", "vocalFormant", 358, 184);
                        addChildKnob (layout, panelId, "Body", "vocalBody", 440, 184);
                        addChildKnob (layout, panelId, "Stereo", "stereoWidth", 522, 184);
                        addChildKnob (layout, panelId, "Ceil DB", "outputCeilingDb", 604, 184);
                        addChildToggle (layout, panelId, "Limiter", "outputLimiter", 604, 70, 76, 30);
                        
                        addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 716, 44, 60, 260);
                    });
            }
            finishModernModule();
            return;
        }

        if (moduleKey == "oscstack")
        {
            ensureParams ({ "oscType", "osc2Type", "oscBlend", "osc2Detune", "subBlend", "noiseBlend", "volume", "outputLimiter" }, "synth");
            liveValues.setValue ("oscType", 1.0f);
            liveValues.setValue ("osc2Type", 1.0f);
            liveValues.setValue ("noiseBlend", 0.0f);
            liveValues.setValue ("outputLimiter", 1.0f);
            ensureBlock ("osc_stack_module", "source", "oscStack", "OSC Stack", "oscBlend", "synth", "source", "stereo",
                         { { "oscType", 1.0f }, { "osc2Type", 1.0f }, { "oscBlend", 0.0f }, { "osc2Detune", 7.0f },
                           { "subBlend", 0.0f }, { "noiseBlend", 0.0f }, { "volume", 0.78f } });
            addModulePanel ("Add OSC Stack Module", "OSC Stack", 590, 164,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    LayoutElement visualizer;
                    visualizer.type = ElementType::SpectrumAnalyzer;
                    visualizer.label = "Synth Spectrum";
                    visualizer.x = pos.x + 18;
                    visualizer.y = pos.y + 38;
                    visualizer.width = 160;
                    visualizer.height = 98;
                    visualizer.containerId = panelId;
                    visualizer.groupId = tabGroup;
                    visualizer.id = layout.generateUniqueId ("spect_");
                    visualizer.accentColour = PatchCraftLookAndFeel::accent();
                    layout.add (visualizer);

                    addChildKnob (layout, panelId, "Wave", "oscType", 192, 38);
                    addChildKnob (layout, panelId, "Wave 2", "osc2Type", 274, 38);
                    addChildKnob (layout, panelId, "Blend", "oscBlend", 356, 38);
                    addChildKnob (layout, panelId, "Sub", "subBlend", 438, 38);
                    addChildValue (layout, panelId, "No Noise", "noiseBlend", 498, 108, 78, 28);
                });
            finishModernModule();
            return;
        }
        if (moduleKey == "wavetable" || moduleKey == "serumtable")
        {
            const bool serumStyle = moduleKey == "serumtable";
            ensureParams ({ "wtEnabled", "wtTable", "wtPosition", "wtMorph", "wtWarp", "wtFold",
                            "wtUnison", "wtDetune", "wtSpread", "wtLevel", "wtBend", "wtSyncRatio",
                            "wtSpectralTilt", "wtPhaseMode", "filterCutoff", "outputLimiter" }, "synth");
            liveValues.setValue ("wtEnabled", 1.0f);
            liveValues.setValue ("wtLevel", serumStyle ? 0.82f : 0.70f);
            liveValues.setValue ("noiseBlend", 0.0f);
            liveValues.setValue ("outputLimiter", 1.0f);
            ensureBlock (serumStyle ? "serum_table_module" : "wavetable_module", "source",
                         serumStyle ? "serumWavetable" : "wavetable",
                         serumStyle ? "Serum-Style Wavetable" : "Wavetable Source",
                         "wtPosition", "synth", "source", "stereo",
                         { { "wtEnabled", 1.0f }, { "wtTable", serumStyle ? 4.0f : 1.0f }, { "wtPosition", 0.32f },
                           { "wtMorph", 0.20f }, { "wtWarp", serumStyle ? 0.34f : 0.12f }, { "wtFold", serumStyle ? 0.18f : 0.0f },
                           { "wtUnison", serumStyle ? 4.0f : 1.0f }, { "wtDetune", serumStyle ? 16.0f : 8.0f },
                           { "wtSpread", serumStyle ? 0.62f : 0.0f }, { "wtLevel", serumStyle ? 0.82f : 0.70f } });
            for (auto& block : graph.blocks)
            {
                if (block.id == (serumStyle ? "serum_table_module" : "wavetable_module"))
                    continue;
                if (block.section.equalsIgnoreCase ("source")
                    && (block.type.containsIgnoreCase ("osc") || block.type.equalsIgnoreCase ("oscillator")))
                {
                    block.values["volume"] = 0.0f;
                    block.values["oscBlend"] = 0.0f;
                }
            }
            addModulePanel (serumStyle ? "Add Serum-Style Wavetable Module" : "Add Wavetable Module",
                            serumStyle ? "Serum-Style Table" : "Wavetable Source", serumStyle ? 520 : 430, 196,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Waveform, "Table View", "wtPosition", 18, 34, serumStyle ? 210 : 170, 86);
                    addChildKnob (layout, panelId, "Pos", "wtPosition", serumStyle ? 244 : 198, 40);
                    addChildKnob (layout, panelId, "Morph", "wtMorph", serumStyle ? 326 : 280, 40);
                    addChildKnob (layout, panelId, "Warp", "wtWarp", serumStyle ? 408 : 198, serumStyle ? 40 : 116);
                    if (serumStyle)
                    {
                        addChildKnob (layout, panelId, "Unison", "wtUnison", 244, 116);
                        addChildKnob (layout, panelId, "Detune", "wtDetune", 326, 116);
                        addChildKnob (layout, panelId, "Fold", "wtFold", 408, 116);
                    }
                    else
                    {
                        addChildToggle (layout, panelId, "On", "wtEnabled", 300, 126, 70, 28);
                    }
                });
            finishModernModule();
            return;
        }

        if (moduleKey == "recorddropzone" || moduleKey == "vocalchoppad"
            || moduleKey == "beatsliceeditor" || moduleKey == "looptimestretch"
            || moduleKey == "samplefxstrip")
        {
            ensureProducerSamplerParams (moduleKey != "samplefxstrip");

            if (moduleKey == "recorddropzone")
            {
                ensureBlock ("record_drop_zone_source", "source", "samplePlayer", "Record / Drop Zone Source", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "bpmSync", 1.0f }, { "volume", 0.88f } });
                ensureBlock ("record_drop_zone_output", "out", "limiter", "Record / Drop Zone Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.88f } });
                addModulePanel ("Add Record Drop Zone Module", "Record / Drop Zone", 640, 300,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Recordings", juce::String(), 18, 36, 150, 128);
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop / Record", "sampleStart", 184, 36, 170, 128);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Captured Sample", "sampleStart", 374, 36, 218, 72);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 374, 132);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 456, 132);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 538, 132);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Map To Keys", juce::String(), 18, 214, 360, 52);
                        addChildToggle (layout, panelId, "Sync", "bpmSync", 408, 226, 72, 28);
                        addChildKnob (layout, panelId, "Out", "volume", 514, 204, 58);
                    });
            }
            else if (moduleKey == "vocalchoppad")
            {
                liveValues.setValue ("sampleSliceCount", 12.0f);
                liveValues.setValue ("vocalMix", 0.10f);
                ensureBlock ("vocal_chop_pad_source", "source", "sliceChop", "Vocal Chop Pad Source", "sampleSlice", "sampler", "source", "stereo",
                             { { "sampleSlice", 0.0f }, { "sampleSliceCount", 12.0f }, { "sampleLength", 0.20f }, { "samplePitch", 0.0f },
                               { "sampleGlitchGrid", 12.0f }, { "bpmSync", 1.0f }, { "volume", 0.82f } });
                ensureBlock ("vocal_chop_pad_formant", "fx", "vocalFormant", "Vocal Chop Color", "vocalMix", "creative", "tone", "stereo",
                             { { "vocalFormant", 0.42f }, { "vocalBody", 0.32f }, { "vocalMix", 0.10f } });
                ensureBlock ("vocal_chop_pad_delay", "fx", "multiTapDelay", "Vocal Chop Echo", "multiTapMix", "creative", "space", "stereo",
                             { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.18f }, { "multiTapSpread", 0.34f }, { "multiTapMix", 0.08f } });
                ensureBlock ("vocal_chop_pad_output", "out", "limiter", "Vocal Chop Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.82f } });
                addModulePanel ("Add Vocal Chop Pad Module", "Vocal Chop Pad", 700, 340,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Vocal", "sampleStart", 18, 36, 150, 124);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Chops", "sampleSlice", 184, 36, 250, 66);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Trigger Pads", "padGrid", 460, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Slice", "sampleSlice", 184, 126);
                        addChildKnob (layout, panelId, "Count", "sampleSliceCount", 266, 126);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 348, 126);
                        addChildKnob (layout, panelId, "Formant", "vocalFormant", 184, 222);
                        addChildKnob (layout, panelId, "Body", "vocalBody", 266, 222);
                        addChildKnob (layout, panelId, "Delay", "multiTapMix", 348, 222);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 458, 218, 196, 46);
                    });
            }
            else if (moduleKey == "beatsliceeditor")
            {
                liveValues.setValue ("sampleSliceCount", 32.0f);
                ensureBlock ("beat_slice_editor_source", "source", "sliceChop", "Beat Slice Editor", "sampleSlice", "sampler", "source", "stereo",
                             { { "sampleSlice", 0.0f }, { "sampleSliceCount", 32.0f }, { "sampleLength", 0.18f }, { "sampleGlitchGrid", 32.0f },
                               { "bpmSync", 1.0f }, { "volume", 0.84f } });
                ensureBlock ("beat_slice_editor_midi", "mod", "midiPlayground", "Beat Slice MIDI", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.52f }, { "arpLaneSwing", 0.08f }, { "arpLaneProbability", 1.0f },
                               { "mpSampleControl", 1.0f }, { "sampleSliceCount", 32.0f }, { "retrigger", 1.0f } });
                ensureBlock ("beat_slice_editor_output", "out", "limiter", "Beat Slice Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });
                addModulePanel ("Add Beat Slice Editor Module", "Beat Slice Editor", 760, 380,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Beat", "sampleStart", 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Slice Editor", "sampleSlice", 186, 36, 330, 76);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Slices", "padGrid", 540, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Slice", "sampleSlice", 190, 132);
                        addChildKnob (layout, panelId, "Count", "sampleSliceCount", 272, 132);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 354, 132);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 436, 132);
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Trigger Pattern", "arpLaneRate", 18, 216, 436, 128);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 478, 226);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 560, 226);
                        addChildToggle (layout, panelId, "Sync", "bpmSync", 640, 244, 70, 28);
                    });
            }
            else if (moduleKey == "looptimestretch")
            {
                liveValues.setValue ("bpmSync", 1.0f);
                ensureBlock ("loop_time_stretch_source", "source", "samplePlayer", "Loop Time Stretch Source", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "bpmSync", 1.0f }, { "volume", 0.86f } });
                ensureBlock ("loop_time_stretch_filter", "filter", "stateVariable", "Loop Tone", "filterCutoff", "sampler", "tone", "stereo",
                             { { "filterCutoff", 7200.0f }, { "filterResonance", 0.10f } });
                ensureBlock ("loop_time_stretch_output", "out", "limiter", "Loop Time Stretch Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });
                addModulePanel ("Add Loop Time Stretch Module", "Loop Time Stretch", 660, 300,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Tempo Loop", "sampleStart", 186, 36, 290, 76);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 188, 134);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 270, 134);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 352, 134);
                        addChildValue (layout, panelId, "BPM", "projectBpm", 462, 142, 70, 30);
                        addChildToggle (layout, panelId, "Sync", "bpmSync", 548, 142, 70, 30);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Trigger Keys", juce::String(), 18, 222, 350, 46);
                        addChildKnob (layout, panelId, "Tone", "filterCutoff", 402, 204);
                        addChildKnob (layout, panelId, "Out", "volume", 484, 204, 58);
                    });
            }
            else
            {
                ensureParams ({ "eqEnabled", "eqMix", "eqBand1On", "eqBand1Freq", "eqBand1GainDb",
                                "eqBand2On", "eqBand2Freq", "eqBand2GainDb", "outputGainDb" }, "fx");
                liveValues.setValue ("eqEnabled", 1.0f);
                liveValues.setValue ("eqMix", 1.0f);
                liveValues.setValue ("dynMix", 0.18f);
                liveValues.setValue ("multiTapMix", 0.08f);
                liveValues.setValue ("tapeMix", 0.08f);
                liveValues.setValue ("lofiMix", 0.05f);
                ensureBlock ("sample_fx_strip_eq", "filter", "dynamicEq", "Sample Clean EQ", "eqMix", "studio", "tone", "stereo",
                             { { "eqEnabled", 1.0f }, { "eqMix", 1.0f }, { "eqBand1On", 1.0f }, { "eqBand1Freq", 120.0f },
                               { "eqBand1GainDb", -0.8f }, { "eqBand2On", 1.0f }, { "eqBand2Freq", 6200.0f }, { "eqBand2GainDb", -0.4f } });
                ensureBlock ("sample_fx_strip_dynamics", "fx", "dynamics", "Sample Leveler", "dynMix", "studio", "dynamics", "stereo",
                             { { "dynThresholdDb", -18.0f }, { "dynRatio", 2.0f }, { "dynAttackMs", 8.0f }, { "dynReleaseMs", 140.0f }, { "dynMix", 0.18f } });
                ensureBlock ("sample_fx_strip_delay", "fx", "multiTapDelay", "Sample Delay", "multiTapMix", "creative", "space", "stereo",
                             { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.18f }, { "multiTapSpread", 0.32f }, { "multiTapMix", 0.08f } });
                ensureBlock ("sample_fx_strip_tape", "fx", "tape", "Sample Tape", "tapeMix", "creative", "tone", "stereo",
                             { { "tapeDrive", 0.16f }, { "tapeTone", 0.46f }, { "tapeFlutter", 0.04f }, { "tapeMix", 0.08f } });
                ensureBlock ("sample_fx_strip_lofi", "fx", "lofi", "Sample LoFi", "lofiMix", "creative", "destruction", "stereo",
                             { { "lofiBits", 14.0f }, { "lofiRate", 0.10f }, { "lofiMix", 0.05f } });
                ensureBlock ("sample_fx_strip_output", "out", "limiter", "Sample FX Output", "outputCeilingDb", "studio", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });
                addModulePanel ("Add Sample FX Strip Module", "Sample FX Strip", 780, 330,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::EqCurve, "Clean EQ", "eqMix", 18, 38, 230, 98);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Sample Motion", "multiTapMix", 18, 170, 230, 70);
                        addChildKnob (layout, panelId, "Low Cut", "eqBand1Freq", 280, 44);
                        addChildKnob (layout, panelId, "Presence", "eqBand2GainDb", 362, 44);
                        addChildKnob (layout, panelId, "Comp", "dynMix", 444, 44);
                        addChildKnob (layout, panelId, "Delay", "multiTapMix", 526, 44);
                        addChildKnob (layout, panelId, "Tape", "tapeMix", 280, 172);
                        addChildKnob (layout, panelId, "LoFi", "lofiMix", 362, 172);
                        addChildKnob (layout, panelId, "Width", "stereoWidth", 444, 172);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 526, 172);
                        addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 682, 44, 48, 210);
                    });
            }

            finishModernModule();
            return;
        }

        if (moduleKey == "sampleplayer" || moduleKey == "slicechop" || moduleKey == "scratchdeck" || moduleKey == "granularsampler")
        {
            ensureParams ({ "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount", "samplePitch",
                            "sampleReverse", "sampleGlitch", "sampleGlitchGrid", "granularOn", "granularDensity",
                            "granularSizeMs", "granularSpread", "granularScan", "granularPitchSpread",
                            "granularPanSpread", "granularReverse", "granularTexture", "granularMaxGrains",
                            "granularDirection", "granularFreeze", "volume", "pan", "outputLimiter" }, "sample");
            liveValues.setValue ("outputLimiter", 1.0f);

            if (moduleKey == "sampleplayer")
            {
                ensureBlock ("sample_player_module", "source", "samplePlayer", "Sample Player", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.90f } });
                addModulePanel ("Add Sample Player Module", "Sample Player", 430, 190,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample", "sampleStart", 18, 36, 150, 116);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Sample View", "sampleStart", 182, 36, 194, 58);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 184, 106);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 266, 106);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 348, 106);
                    });
            }
            else if (moduleKey == "slicechop")
            {
                ensureBlock ("slice_chop_module", "source", "sliceChop", "Slice / Chop Sampler", "sampleSlice", "sampler", "source", "stereo",
                             { { "sampleSliceCount", 16.0f }, { "sampleSlice", 0.0f }, { "sampleLength", 0.25f }, { "sampleGlitchGrid", 16.0f } });
                addModulePanel ("Add Slice Chop Module", "Slice / Chop", 520, 210,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Slices", "sampleSlice", 182, 36, 266, 60);
                        addChildKnob (layout, panelId, "Slice", "sampleSlice", 184, 116);
                        addChildKnob (layout, panelId, "Count", "sampleSliceCount", 266, 116);
                        addChildKnob (layout, panelId, "Glitch", "sampleGlitch", 348, 116);
                        addChildValue (layout, panelId, "Grid", "sampleGlitchGrid", 430, 132, 72, 30);
                    });
            }
            else if (moduleKey == "scratchdeck")
            {
                ensureBlock ("scratch_deck_module", "source", "scratchDeck", "Scratch Deck", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "sampleReverse", 0.0f } });
                addModulePanel ("Add Scratch Deck Module", "Scratch Deck", 520, 228,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Deck Audio", "sampleStart", 18, 38, 150, 138);
                        addChildSurface (layout, panelId, ElementType::XYPad, "Scratch Motion", "sampleStart", 188, 38, 160, 138);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 364, 44);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 442, 44);
                        addChildToggle (layout, panelId, "Reverse", "sampleReverse", 366, 136, 100, 30);
                    });
            }
            else
            {
                liveValues.setValue ("granularOn", 1.0f);
                ensureBlock ("granular_sampler_module", "source", "granularSampler", "Granular Sampler", "granularDensity", "sampler", "source", "stereo",
                             { { "granularOn", 1.0f }, { "granularDensity", 32.0f }, { "granularSizeMs", 90.0f },
                               { "granularSpread", 0.18f }, { "granularScan", 0.0f }, { "granularPanSpread", 0.45f } });
                addModulePanel ("Add Granular Sampler Module", "Granular Sampler", 560, 230,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Texture", "sampleStart", 18, 38, 150, 138);
                        addChildSurface (layout, panelId, ElementType::GranularField, "Grain Field", "granularScan", 184, 38, 176, 138);
                        addChildKnob (layout, panelId, "Density", "granularDensity", 378, 42);
                        addChildKnob (layout, panelId, "Size", "granularSizeMs", 456, 42);
                        addChildKnob (layout, panelId, "Spread", "granularSpread", 378, 122);
                        addChildKnob (layout, panelId, "Scan", "granularScan", 456, 122);
                    });
            }
            finishModernModule();
            return;
        }

        if (moduleKey == "multisamplekeymap" || moduleKey == "chopgrid" || moduleKey == "loopslicer"
            || moduleKey == "midiloopplayer" || moduleKey == "vinyltexturesampler")
        {
            ensureProducerSamplerParams (moduleKey != "midiloopplayer");

            if (moduleKey == "multisamplekeymap")
            {
                ensureBlock ("multisample_keymap_module", "source", "samplePlayer", "Multisample Keymap", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.90f } });
                ensureBlock ("multisample_keymap_filter", "filter", "stateVariable", "Keymap Filter", "filterCutoff", "sampler", "tone", "stereo",
                             { { "filterCutoff", 7200.0f }, { "filterResonance", 0.10f } });
                ensureBlock ("multisample_keymap_output", "out", "limiter", "Keymap Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.90f } });
                addModulePanel ("Add Multisample Keymap Module", "Multisample Keymap", 680, 300,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Multisamples", "sampleStart", 184, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Selected Sample", "sampleStart", 352, 36, 210, 66);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 352, 124);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 434, 124);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 516, 124);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Key Zones", juce::String(), 18, 204, 430, 58);
                        addChildSlider (layout, panelId, "A", "attack", 474, 186, 38, 82);
                        addChildSlider (layout, panelId, "R", "release", 520, 186, 38, 82);
                        addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 584, 194, 58);
                    });
            }
            else if (moduleKey == "chopgrid")
            {
                liveValues.setValue ("sampleSliceCount", 16.0f);
                ensureBlock ("chop_grid_module", "source", "sliceChop", "Chop Grid", "sampleSlice", "sampler", "source", "stereo",
                             { { "sampleSlice", 0.0f }, { "sampleSliceCount", 16.0f }, { "sampleLength", 0.25f }, { "sampleGlitchGrid", 16.0f } });
                ensureBlock ("chop_grid_output", "out", "limiter", "Chop Grid Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.86f } });
                addModulePanel ("Add Chop Grid Module", "Chop Grid", 640, 330,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 18, 36, 150, 116);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Slices", "sampleSlice", 184, 36, 260, 64);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Slice Pads", "padGrid", 466, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Slice", "sampleSlice", 184, 118);
                        addChildKnob (layout, panelId, "Count", "sampleSliceCount", 266, 118);
                        addChildKnob (layout, panelId, "Length", "sampleLength", 348, 118);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 430, 196);
                        addChildKnob (layout, panelId, "Glitch", "sampleGlitch", 512, 196);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Key Trigger", juce::String(), 18, 246, 380, 54);
                    });
            }
            else if (moduleKey == "loopslicer")
            {
                liveValues.setValue ("sampleSliceCount", 32.0f);
                ensureBlock ("loop_slicer_source", "source", "sliceChop", "Loop Slicer", "sampleSlice", "sampler", "source", "stereo",
                             { { "sampleSlice", 0.0f }, { "sampleSliceCount", 32.0f }, { "sampleLength", 0.20f }, { "sampleGlitchGrid", 32.0f } });
                // drumSequencer (not midiPlayground) so the step grid is recognised
                // by findDrumMachineBlock and actually triggers slices on playback.
                ensureBlock ("loop_slicer_midi", "mod", "drumSequencer", "Loop Slicer Sequencer", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 2.0f }, { "arpLaneGate", 0.50f }, { "arpLaneSwing", 0.05f }, { "arpLaneProbability", 1.0f },
                               { "mpSampleControl", 1.0f }, { "sampleSliceCount", 32.0f }, { "dmTracks", 4.0f },
                               { "dmTriggerPadSlots", 1.0f }, { "retrigger", 1.0f } });
                ensureBlock ("loop_slicer_delay", "fx", "multiTapDelay", "Loop Slicer Echo", "multiTapMix", "creative", "space", "stereo",
                             { { "multiTapTime", 0.25f }, { "multiTapFeedback", 0.18f }, { "multiTapSpread", 0.34f }, { "multiTapMix", 0.10f } });
                ensureBlock ("loop_slicer_output", "out", "limiter", "Loop Slicer Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });
                addModulePanel ("Add Loop Slicer Module", "Loop Slicer", 720, 360,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Loop", "sampleStart", 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Loop Slices", "sampleSlice", 188, 36, 318, 72);
                        addChildKnob (layout, panelId, "Count", "sampleSliceCount", 526, 42);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 606, 42);
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Trigger Pattern", "arpLaneRate", 188, 148, 366, 150);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 574, 158);
                        addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 654, 158);
                        addChildKnob (layout, panelId, "Echo", "multiTapMix", 574, 242);
                        addChildKnob (layout, panelId, "Out", "volume", 654, 242, 58);
                    });
            }
            else if (moduleKey == "midiloopplayer")
            {
                ensureParams ({ "arpLaneRate", "arpLaneGate", "arpLaneSwing", "arpLaneProbability", "retrigger", "outputLimiter" }, "sample");
                // drumSequencer block so the step grid binds + plays its pattern.
                ensureBlock ("midi_loop_player_module", "mod", "drumSequencer", "MIDI Loop Player", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.58f }, { "arpLaneSwing", 0.06f }, { "arpLaneProbability", 1.0f },
                               { "mpScaleRoot", 0.0f }, { "mpScaleType", 1.0f }, { "mpSampleControl", 1.0f }, { "sampleSliceCount", 16.0f },
                               { "dmTracks", 4.0f }, { "dmTriggerPadSlots", 1.0f }, { "retrigger", 1.0f } });
                addModulePanel ("Add MIDI Loop Player Module", "MIDI Loop Player", 560, 260,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "MIDI Pattern", "arpLaneRate", 18, 36, 336, 150);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Trigger Keys", juce::String(), 18, 204, 336, 42);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 380, 42);
                        addChildKnob (layout, panelId, "Gate", "arpLaneGate", 460, 42);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 380, 126);
                        addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 460, 126);
                        addChildToggle (layout, panelId, "Retrig", "retrigger", 384, 212, 86, 28);
                    });
            }
            else
            {
                liveValues.setValue ("vinylMix", 0.22f);
                liveValues.setValue ("tapeMix", 0.12f);
                liveValues.setValue ("lofiMix", 0.08f);
                ensureBlock ("vinyl_texture_source", "source", "samplePlayer", "Vinyl Texture Sample Source", "sampleStart", "sampler", "source", "stereo",
                             { { "sampleStart", 0.0f }, { "sampleLength", 1.0f }, { "samplePitch", 0.0f }, { "volume", 0.82f } });
                ensureBlock ("vinyl_texture_vinyl", "fx", "vinyl", "Vinyl Texture", "vinylMix", "creative", "destruction", "stereo",
                             { { "vinylAge", 0.44f }, { "vinylDust", 0.08f }, { "vinylWarp", 0.12f }, { "vinylMix", 0.22f } });
                ensureBlock ("vinyl_texture_tape", "fx", "tape", "Tape Color", "tapeMix", "creative", "tone", "stereo",
                             { { "tapeDrive", 0.20f }, { "tapeTone", 0.46f }, { "tapeFlutter", 0.08f }, { "tapeMix", 0.12f } });
                ensureBlock ("vinyl_texture_lofi", "fx", "lofi", "LoFi Texture", "lofiMix", "creative", "destruction", "stereo",
                             { { "lofiBits", 13.0f }, { "lofiRate", 0.14f }, { "lofiMix", 0.08f } });
                ensureBlock ("vinyl_texture_output", "out", "limiter", "Vinyl Texture Output", "outputCeilingDb", "sampler", "output", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.82f } });
                addModulePanel ("Add Vinyl Texture Sampler Module", "Vinyl Texture Sampler", 620, 260,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample", "sampleStart", 18, 36, 150, 126);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Sample", "sampleStart", 186, 36, 230, 66);
                        addChildKnob (layout, panelId, "Start", "sampleStart", 186, 124);
                        addChildKnob (layout, panelId, "Pitch", "samplePitch", 268, 124);
                        addChildKnob (layout, panelId, "Vinyl", "vinylMix", 350, 124);
                        addChildKnob (layout, panelId, "Tape", "tapeMix", 432, 124);
                        addChildKnob (layout, panelId, "LoFi", "lofiMix", 514, 124);
                    });
            }

            finishModernModule();
            return;
        }

        if (moduleKey == "eightoheightkit" || moduleKey == "boombappadbank" || moduleKey == "quickdrumkit")
        {
            ensureProducerSamplerParams (true);

            if (moduleKey == "eightoheightkit")
            {
                liveValues.setValue ("pad1Pitch", -12.0f);
                liveValues.setValue ("pad2Pitch", -7.0f);
                liveValues.setValue ("tapeMix", 0.08f);
                ensureBlock ("eight_oh_eight_rack", "source", "drumRack", "808 Kit Rack", "pad1Pitch", "drums", "source", "stereo",
                             { { "pad1Volume", 1.0f }, { "pad2Volume", 0.92f }, { "pad3Volume", 0.80f }, { "pad4Volume", 0.88f },
                               { "pad1Pitch", -12.0f }, { "pad2Pitch", -7.0f }, { "volume", 0.82f } });
                ensureBlock ("eight_oh_eight_seq", "mod", "drumSequencer", "808 Pattern", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.03f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
                ensureBlock ("eight_oh_eight_tape", "fx", "tape", "808 Saturation", "tapeMix", "creative", "tone", "stereo",
                             { { "tapeDrive", 0.18f }, { "tapeTone", 0.42f }, { "tapeFlutter", 0.02f }, { "tapeMix", 0.08f } });
                ensureBlock ("eight_oh_eight_output", "out", "drumMixer", "808 Kit Mixer", "pad1Volume", "drums", "mix", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.82f } });

                addModulePanel ("Add 808 Kit Builder Module", "808 Kit Builder", 680, 380,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "808 Library", juce::String(), 18, 36, 140, 124);
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop 808s", "sampleStart", 176, 36, 140, 124);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "808 Pads", "padGrid", 338, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Kick Tune", "pad1Pitch", 532, 44);
                        addChildKnob (layout, panelId, "Sub Tune", "pad2Pitch", 600, 44);
                        addChildKnob (layout, panelId, "Kick Vol", "pad1Volume", 532, 124);
                        addChildKnob (layout, panelId, "Sub Vol", "pad2Volume", 600, 124);
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "808 Pattern", "arpLaneRate", 18, 214, 384, 132);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 426, 234);
                        addChildKnob (layout, panelId, "Tape", "tapeMix", 508, 234);
                        addChildKnob (layout, panelId, "Out", "volume", 590, 234, 58);
                    });
            }
            else if (moduleKey == "boombappadbank")
            {
                liveValues.setValue ("vinylMix", 0.18f);
                liveValues.setValue ("lofiMix", 0.10f);
                ensureBlock ("boom_bap_rack", "source", "drumRack", "Boom Bap Pad Rack", "pad1Volume", "drums", "source", "stereo",
                             { { "pad1Volume", 1.0f }, { "pad2Volume", 0.92f }, { "pad3Volume", 0.78f }, { "pad4Volume", 0.90f }, { "volume", 0.84f } });
                ensureBlock ("boom_bap_seq", "mod", "drumSequencer", "Boom Bap Pattern", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.12f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
                ensureBlock ("boom_bap_vinyl", "fx", "vinyl", "Boom Bap Vinyl", "vinylMix", "creative", "destruction", "stereo",
                             { { "vinylAge", 0.38f }, { "vinylDust", 0.06f }, { "vinylWarp", 0.08f }, { "vinylMix", 0.18f } });
                ensureBlock ("boom_bap_lofi", "fx", "lofi", "Boom Bap LoFi", "lofiMix", "creative", "destruction", "stereo",
                             { { "lofiBits", 12.0f }, { "lofiRate", 0.18f }, { "lofiMix", 0.10f } });
                ensureBlock ("boom_bap_output", "out", "drumMixer", "Boom Bap Mixer", "pad1Volume", "drums", "mix", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });

                addModulePanel ("Add Boom Bap Pad Bank Module", "Boom Bap Pad Bank", 720, 390,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Crate", juce::String(), 18, 36, 148, 128);
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Drums", "sampleStart", 184, 36, 148, 128);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 356, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Kick", "pad1Volume", 574, 44);
                        addChildKnob (layout, panelId, "Snare", "pad2Volume", 574, 124);
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Groove Pattern", "arpLaneRate", 18, 224, 420, 132);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 462, 244);
                        addChildKnob (layout, panelId, "Vinyl", "vinylMix", 544, 244);
                        addChildKnob (layout, panelId, "LoFi", "lofiMix", 626, 244);
                    });
            }
            else
            {
                liveValues.setValue ("sampleSliceCount", 16.0f);
                liveValues.setValue ("bpmSync", 1.0f);
                ensureBlock ("quick_drum_kit_rack", "source", "drumRack", "Quick Drum Kit Rack", "pad1Volume", "drums", "source", "stereo",
                             { { "pad1Volume", 1.0f }, { "pad2Volume", 0.95f }, { "pad3Volume", 0.86f }, { "pad4Volume", 0.88f },
                               { "pad1Pitch", 0.0f }, { "pad2Pitch", 0.0f }, { "pad3Pitch", 0.0f }, { "pad4Pitch", 0.0f }, { "volume", 0.84f } });
                ensureBlock ("quick_drum_kit_seq", "mod", "drumSequencer", "Quick Drum Pattern", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.06f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
                ensureBlock ("quick_drum_kit_mixer", "out", "drumMixer", "Quick Drum Mixer", "pad1Volume", "drums", "mix", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "volume", 0.84f } });

                addModulePanel ("Add Quick Drum Kit Module", "Quick Drum Kit", 700, 390,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Drum Library", juce::String(), 18, 36, 148, 126);
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Drums", "sampleStart", 184, 36, 148, 126);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Playable Pads", "padGrid", 356, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildKnob (layout, panelId, "Pad 1 Vol", "pad1Volume", 578, 42);
                        addChildKnob (layout, panelId, "Pad 2 Vol", "pad2Volume", 578, 122);
                        addChildKnob (layout, panelId, "Pad 1 Tune", "pad1Pitch", 496, 230);
                        addChildKnob (layout, panelId, "Pad 2 Tune", "pad2Pitch", 578, 230);
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern", "arpLaneRate", 18, 214, 420, 132);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 496, 310);
                        addChildKnob (layout, panelId, "Out", "volume", 578, 310, 58);
                    });
            }

            finishModernModule();
            return;
        }

        if (moduleKey == "drumrack" || moduleKey == "drumsequencer" || moduleKey == "drummixer")
        {
            juce::StringArray drumParams { "volume", "pan", "outputLimiter" };
            for (int pad = 1; pad <= 16; ++pad)
            {
                const auto padText = juce::String (pad);
                drumParams.add ("pad" + padText + "Volume");
                drumParams.add ("pad" + padText + "Pitch");
                drumParams.add ("pad" + padText + "Pan");
            }
            ensureParams (drumParams, "sample");
            liveValues.setValue ("outputLimiter", 1.0f);

            if (moduleKey == "drumrack")
            {
                ensureBlock ("drum_rack_module", "source", "drumRack", "Drum Rack", "pad1Volume", "drums", "source", "stereo",
                             { { "pad1Volume", 1.0f }, { "pad2Volume", 1.0f }, { "pad3Volume", 1.0f }, { "pad4Volume", 1.0f } });
                addModulePanel ("Add Drum Rack Module", "Drum Rack", 520, 430,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::PadGrid, "16 Pads", "padGrid", 20, 38, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Samples", "sampleStart", 300, 38, 170, 116);
                        addChildKnob (layout, panelId, "Pad Level", "pad1Volume", 310, 182);
                        addChildKnob (layout, panelId, "Pad Tune", "pad1Pitch", 392, 182);
                        addChildKnob (layout, panelId, "Master", "volume", 310, 264);
                        addChildKnob (layout, panelId, "Pan", "pan", 392, 264);
                    });
            }
            else if (moduleKey == "drumsequencer")
            {
                ensureParams ({ "arpLaneRate", "arpLaneSwing", "arpLaneProbability", "retrigger" }, "sample");
                ensureBlock ("drum_sequencer_module", "mod", "drumSequencer", "Drum Sequencer", "arpLaneRate", "midi", "sequencer", "event",
                             { { "arpLaneRate", 1.0f }, { "arpLaneSwing", 0.04f }, { "arpLaneProbability", 1.0f }, { "retrigger", 1.0f } });
                addModulePanel ("Add Drum Sequencer Module", "Drum Sequencer", 650, 300,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern Grid", "arpLaneRate", 18, 36, 460, 190);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 500, 48);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 580, 48);
                        addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 500, 132);
                        addChildToggle (layout, panelId, "Retrig", "retrigger", 580, 150, 70, 30);
                    });
            }
            else
            {
                ensureBlock ("drum_mixer_module", "out", "drumMixer", "Drum Mixer", "pad1Volume", "drums", "mix", "stereo",
                             { { "pad1Volume", 1.0f }, { "pad2Volume", 1.0f }, { "pad3Volume", 1.0f }, { "pad4Volume", 1.0f } });
                addModulePanel ("Add Drum Mixer Module", "Drum Mixer", 560, 220,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Mixer, "Pad Mixer", "pad1Volume", 18, 34, 330, 160);
                        addChildSlider (layout, panelId, "1", "pad1Volume", 370, 42);
                        addChildSlider (layout, panelId, "2", "pad2Volume", 416, 42);
                        addChildSlider (layout, panelId, "3", "pad3Volume", 462, 42);
                        addChildSlider (layout, panelId, "4", "pad4Volume", 508, 42);
                    });
            }
            finishModernModule();
            return;
        }
        if (moduleKey == "harmonycomposer" || moduleKey == "chordcomposer")
        {
            // Diatonic chord engine driven by ComposerRuntime. Builds chords from
            // scale degrees and plays them on the transport, so a developer can
            // pick a key + progression and the Player performs it.
            ensureParams ({ "composerRoot", "composerScale", "composerChordCount", "composerRate",
                            "composerGate", "composerVelocity", "composerVoices", "composerOctave",
                            "composerSpread", "composerDegree1", "composerDegree2", "composerDegree3",
                            "composerDegree4", "composerChord1On", "composerChord2On",
                            "composerChord3On", "composerChord4On", "volume", "pan" }, "synth");

            ensureBlock ("harmony_composer_module", "mod", "harmonyComposer", "Harmony Composer",
                         "composerRoot", "midi", "harmony", "event",
                         { { "composerRoot", 0.0f }, { "composerScale", 1.0f }, { "composerChordCount", 4.0f },
                           { "composerRate", 1.0f }, { "composerGate", 0.82f }, { "composerVelocity", 0.82f },
                           { "composerVoices", 4.0f }, { "composerOctave", 4.0f }, { "composerSpread", 0.38f },
                           { "composerDegree1", 0.0f }, { "composerDegree2", 4.0f },
                           { "composerDegree3", 5.0f }, { "composerDegree4", 3.0f },
                           { "composerChord1On", 1.0f }, { "composerChord2On", 1.0f },
                           { "composerChord3On", 1.0f }, { "composerChord4On", 1.0f } });

            addModulePanel ("Add Harmony Composer Module", "Harmony Composer", 720, 332,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Dropdown, "Key", "composerRoot", 18, 40, 96, 32);
                    addChildSurface (layout, panelId, ElementType::Dropdown, "Scale", "composerScale", 124, 40, 124, 32);
                    addChildKnob  (layout, panelId, "Voices", "composerVoices", 264, 36, 60);
                    addChildKnob  (layout, panelId, "Spread", "composerSpread", 338, 36, 60);
                    addChildKnob  (layout, panelId, "Rate", "composerRate", 412, 36, 60);
                    addChildKnob  (layout, panelId, "Gate", "composerGate", 486, 36, 60);
                    addChildKnob  (layout, panelId, "Octave", "composerOctave", 560, 36, 60);

                    // Four selectable chord slots: degree dropdown + enable toggle.
                    const char* degParams[4] = { "composerDegree1", "composerDegree2", "composerDegree3", "composerDegree4" };
                    const char* onParams[4]  = { "composerChord1On", "composerChord2On", "composerChord3On", "composerChord4On" };
                    for (int i = 0; i < 4; ++i)
                    {
                        const int cx = 18 + i * 172;
                        addChildSurface (layout, panelId, ElementType::Dropdown, "Chord " + juce::String (i + 1),
                                         degParams[i], cx, 116, 150, 34);
                        addChildToggle (layout, panelId, "Play", onParams[i], cx, 158, 150, 28);
                    }

                    addChildSurface (layout, panelId, ElementType::Keyboard, "Chord Preview",
                                     juce::String(), 18, 200, 684, 110);
                });

            finishModernModule();
            return;
        }

        if (moduleKey == "chordprogressionbuilder" || moduleKey == "scalechordassistant"
            || moduleKey == "chordpadbank" || moduleKey == "voicinghumanize")
        {
            ensureChordModuleParams();

            if (moduleKey == "chordprogressionbuilder")
            {
                ensureProgressionBlock ("chord_progression_builder_module", "Chord Progression Builder",
                                        { 0, 7, 8, 13, 20, 21, 27, 28 }, 1.0f, 0.08f, 0.14f, 0.04f, 0.58f);
                addModulePanel ("Add Chord Progression Builder Module", "Chord Progression Builder", 720, 360,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::DrumGrid, "Progression Timeline", "arpLaneRate", 18, 36, 430, 156);
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Preview Keys", juce::String(), 18, 232, 430, 58);
                        addChildValue (layout, panelId, "Preset", "mpProgressionPreset", 474, 40, 100, 30);
                        addChildValue (layout, panelId, "Bank", "mpActiveBank", 590, 40, 70, 30);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 472, 92);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 552, 92);
                        addChildKnob (layout, panelId, "Chance", "mpProbability", 632, 92);
                        addChildKnob (layout, panelId, "Spread", "mpChordSpread", 472, 182);
                        addChildKnob (layout, panelId, "Strum", "mpStrum", 552, 182);
                        addChildKnob (layout, panelId, "Human", "mpHumanize", 632, 182);
                    });
            }
            else if (moduleKey == "scalechordassistant")
            {
                ensureProgressionBlock ("scale_chord_assistant_module", "Scale + Chord Assistant",
                                        { 0, 1, 2, 5, 6, 8, 15, 26 }, 1.0f, 0.04f, 0.10f, 0.03f, 0.52f);
                addModulePanel ("Add Scale Chord Assistant Module", "Scale + Chord Assistant", 620, 280,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Scale Keyboard", juce::String(), 18, 36, 360, 74);
                        addChildValue (layout, panelId, "Root", "mpScaleRoot", 402, 38, 70, 30);
                        addChildValue (layout, panelId, "Scale", "mpScaleType", 488, 38, 82, 30);
                        addChildValue (layout, panelId, "Chord", "mpChordMode", 402, 84, 70, 30);
                        addChildValue (layout, panelId, "Size", "mpChordSize", 488, 84, 82, 30);
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Suggested Chords", "mpActiveBank", 18, 142, 190, 98);
                        addChildKnob (layout, panelId, "Spread", "mpChordSpread", 238, 148);
                        addChildKnob (layout, panelId, "Strum", "mpStrum", 320, 148);
                        addChildKnob (layout, panelId, "Human", "mpHumanize", 402, 148);
                        addChildToggle (layout, panelId, "Latch", "mpLatch", 486, 166, 82, 28);
                    });
            }
            else if (moduleKey == "chordpadbank")
            {
                ensureProgressionBlock ("chord_pad_bank_module", "Chord Pad Bank",
                                        { 8, 13, 14, 20, 21, 22, 27, 28 }, 1.0f, 0.12f, 0.16f, 0.06f, 0.62f);
                addModulePanel ("Add Chord Pad Bank Module", "Chord Pad Bank", 580, 360,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::PadGrid, "Chord Pads", "mpActiveBank", 20, 36, standardPadGridExtent (4, 4), standardPadGridExtent (4, 4));
                        addChildSurface (layout, panelId, ElementType::Keyboard, "Preview", juce::String(), 20, 304, 250, 42);
                        addChildValue (layout, panelId, "Bank", "mpActiveBank", 302, 42, 72, 30);
                        addChildValue (layout, panelId, "Preset", "mpProgressionPreset", 388, 42, 100, 30);
                        addChildKnob (layout, panelId, "Spread", "mpChordSpread", 302, 102);
                        addChildKnob (layout, panelId, "Strum", "mpStrum", 382, 102);
                        addChildKnob (layout, panelId, "Human", "mpHumanize", 462, 102);
                        addChildKnob (layout, panelId, "Chance", "mpProbability", 302, 190);
                        addChildToggle (layout, panelId, "Latch", "mpLatch", 384, 208, 82, 28);
                    });
            }
            else
            {
                ensureProgressionBlock ("voicing_humanize_module", "Voicing + Humanize",
                                        { 7, 8, 13, 15, 20, 27, 28, 18 }, 1.0f, 0.06f, 0.18f, 0.06f, 0.70f);
                addModulePanel ("Add Voicing Humanize Module", "Voicing + Humanize", 520, 210,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildKnob (layout, panelId, "Chord Size", "mpChordSize", 22, 46);
                        addChildKnob (layout, panelId, "Spread", "mpChordSpread", 104, 46);
                        addChildKnob (layout, panelId, "Strum", "mpStrum", 186, 46);
                        addChildKnob (layout, panelId, "Human", "mpHumanize", 268, 46);
                        addChildKnob (layout, panelId, "Variation", "mpMutation", 350, 46);
                        addChildValue (layout, panelId, "Scale", "mpScaleType", 28, 142, 82, 30);
                        addChildValue (layout, panelId, "Chord", "mpChordMode", 122, 142, 82, 30);
                        addChildToggle (layout, panelId, "Latch", "mpLatch", 232, 142, 82, 28);
                        addChildToggle (layout, panelId, "Retrig", "retrigger", 326, 142, 82, 28);
                    });
            }

            finishModernModule();
            return;
        }

        if (moduleKey == "arplanemodule" || moduleKey == "seqsequencermodule"
            || moduleKey == "lfo" || moduleKey == "steplfo" || moduleKey == "macrobank")
        {
            ensureParams ({ "arpLaneIndex", "arpLaneSteps", "arpLaneRate", "arpLaneGate", "arpLaneSwing",
                            "arpLaneProbability", "arpLaneGroup", "arpLaneFxTarget", "arpLaneFxAmount",
                            "arpLaneHumanize", "arpLaneMutation",
                            "lfoRate", "lfoAmount", "filterCutoff" }, "synth");
            if (moduleKey == "arplanemodule")
            {
                ensureBlock ("arp_lane_module", "mod", "arp", "Arp Lane", "arpLaneRate", "midi", "arp", "event",
                             { { "arpLaneSteps", 16.0f }, { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.58f },
                               { "arpLaneProbability", 1.0f }, { "arpLaneFxAmount", 0.0f } });
                addModulePanel ("Add Arp Lane Module", "Arp Lane", 520, 390,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        LayoutElement lane;
                        lane.type = ElementType::ArpLane;
                        lane.label = "Arp Lane";
                        lane.parameterId = "arpLaneRate";
                        lane.arpLaneMode = "linear";
                        lane.arpLaneSteps = 16;
                        lane.x = pos.x + 24;
                        lane.y = pos.y + 34;
                        lane.width = 472;
                        lane.height = 120;
                        lane.containerId = panelId;
                        lane.groupId = tabGroup;
                        lane.id = layout.generateUniqueId ("module_");
                        lane.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (lane);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 318, 170);
                        addChildKnob (layout, panelId, "Gate", "arpLaneGate", 398, 170);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 318, 250);
                        addChildKnob (layout, panelId, "FX Amt", "arpLaneFxAmount", 398, 250);
                        addChildValue (layout, panelId, "Group", "arpLaneGroup", 320, 320, 78, 30);
                        addChildValue (layout, panelId, "FX", "arpLaneFxTarget", 410, 320, 78, 30);
                    });
            }
            else if (moduleKey == "seqsequencermodule")
            {
                ensureBlock ("step_sequencer_module", "mod", "arp", "Step Sequencer", "arpLaneRate", "midi", "arp", "event",
                             { { "arpSteps", 16.0f }, { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.58f },
                               { "arpLaneSwing", 0.0f }, { "arpLaneProbability", 1.0f } });
                addModulePanel ("Add Step Sequencer", "Step Sequencer", 540, 430,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        auto addSeqLane = [&] (const juce::String& label, const juce::String& laneType,
                                               juce::Colour colour, int yOffset)
                        {
                            LayoutElement seq;
                            seq.type = ElementType::SequencerLane;
                            seq.label = label;
                            seq.seqLaneType = laneType;
                            seq.seqLaneSteps = 16;
                            seq.seqLaneIndex = 0;
                            seq.seqLaneColour = colour;
                            seq.x = pos.x + 24;
                            seq.y = pos.y + yOffset;
                            seq.width = 492;
                            seq.height = 52;
                            seq.containerId = panelId;
                            seq.groupId = tabGroup;
                            seq.id = layout.generateUniqueId ("seq_");
                            seq.accentColour = PatchCraftLookAndFeel::accent();
                            seq.backgroundColour = juce::Colour (0xdd10141a);
                            layout.add (seq);
                        };

                        LayoutElement mainLane;
                        mainLane.type = ElementType::ArpLane;
                        mainLane.label = "Main Steps";
                        mainLane.arpLaneMode = "linear";
                        mainLane.arpLaneSteps = 16;
                        mainLane.x = pos.x + 24;
                        mainLane.y = pos.y + 34;
                        mainLane.width = 492;
                        mainLane.height = 110;
                        mainLane.containerId = panelId;
                        mainLane.groupId = tabGroup;
                        mainLane.id = layout.generateUniqueId ("module_");
                        mainLane.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (mainLane);

                        addSeqLane ("Gate", "gate", PatchCraftLookAndFeel::accent(), 156);
                        addSeqLane ("Velocity", "value", juce::Colour (0xff79c267), 216);
                        addSeqLane ("Pitch", "pitch", juce::Colour (0xff6ab0ff), 276);
                        addSeqLane ("Chance", "chance", juce::Colour (0xff8fd6ff), 336);

                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 24, 400);
                        addChildKnob (layout, panelId, "Gate", "arpLaneGate", 104, 400);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 184, 400);
                        addChildKnob (layout, panelId, "Prob", "arpLaneProbability", 264, 400);
                        addChildSlider (layout, panelId, "Humanize", "arpLaneHumanize", 344, 382, 44, 88);
                        addChildSlider (layout, panelId, "Mutation", "arpLaneMutation", 404, 382, 44, 88);
                    });
            }
            else if (moduleKey == "lfo")
            {
                ensureBlock ("lfo_module", "mod", "lfo", "LFO", "filterCutoff", "midi", "modulation", "modulation",
                             { { "lfoRate", 4.0f }, { "lfoAmount", 0.25f } });
                addModulePanel ("Add LFO Module", "LFO Module", 320, 156,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Waveform, "LFO Shape", "lfoRate", 18, 36, 130, 72);
                        addChildKnob (layout, panelId, "Rate", "lfoRate", 168, 42);
                        addChildKnob (layout, panelId, "Amount", "lfoAmount", 246, 42);
                    });
            }
            else if (moduleKey == "steplfo")
            {
                ensureBlock ("step_lfo_module", "mod", "stepLfo", "Step LFO", "filterCutoff", "midi", "modulation", "modulation",
                             { { "lfoRate", 2.0f }, { "lfoAmount", 0.35f } });
                addModulePanel ("Add Step LFO Module", "Step LFO", 480, 210,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::ModMatrix, "Step Matrix", "lfoAmount", 18, 36, 290, 132);
                        addChildKnob (layout, panelId, "Rate", "lfoRate", 330, 52);
                        addChildKnob (layout, panelId, "Amount", "lfoAmount", 408, 52);
                    });
            }
            else
            {
                ensureBlock ("macro_bank_module", "mod", "macroBank", "Macro Bank", "filterCutoff", "midi", "macro", "modulation",
                             { { "lfoAmount", 0.0f } });
                addModulePanel ("Add Macro Bank Module", "Macro Bank", 520, 180,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::MacroControl, "Macro 1", "filterCutoff", 20, 42, 116, 104);
                        addChildSurface (layout, panelId, ElementType::MacroControl, "Macro 2", "delayMix", 148, 42, 116, 104);
                        addChildSurface (layout, panelId, ElementType::MacroControl, "Macro 3", "reverbMix", 276, 42, 116, 104);
                        addChildSurface (layout, panelId, ElementType::MacroControl, "Macro 4", "stereoWidth", 404, 42, 116, 104);
                    });
            }
            finishModernModule();
            return;
        }
        if (moduleKey == "surgicaleq" || moduleKey == "dynamiceq" || moduleKey == "limiter" || moduleKey == "transient")
        {
            ensureParams ({ "eqEnabled", "eqMix", "eqOutputTrimDb", "eqBand1On", "eqBand1Freq", "eqBand1GainDb", "eqBand1Q",
                            "eqBand2On", "eqBand2Freq", "eqBand2GainDb", "eqBand2Q", "eqBand3On", "eqBand3Freq",
                            "eqBand3GainDb", "eqBand3Q", "eqBand4On", "eqBand4Freq", "eqBand4GainDb", "eqBand4Q",
                            "eqBand1DynMode", "eqBand1DynThresholdDb", "eqBand1DynRangeDb",
                            "dynThresholdDb", "dynRatio", "dynAttackMs", "dynReleaseMs", "dynMakeupDb", "dynMix",
                            "outputLimiter", "outputCeilingDb", "outputGainDb", "drive" }, "fx");
            if (moduleKey == "surgicaleq" || moduleKey == "dynamiceq")
            {
                const bool dynamic = moduleKey == "dynamiceq";
                liveValues.setValue ("eqEnabled", 1.0f);
                ensureBlock (dynamic ? "dynamic_eq_module" : "surgical_eq_module", "filter",
                             dynamic ? "dynamicEq" : "surgicalEq",
                             dynamic ? "Dynamic EQ" : "Surgical EQ",
                             "eqMix", "studio", "tone", "stereo",
                             { { "eqEnabled", 1.0f }, { "eqMix", 1.0f }, { "eqBand1On", 1.0f }, { "eqBand1Freq", 120.0f },
                               { "eqBand1GainDb", dynamic ? -1.5f : 0.0f }, { "eqBand1Q", 0.8f }, { "eqBand2On", 1.0f },
                               { "eqBand2Freq", 2500.0f }, { "eqBand2GainDb", 1.0f }, { "eqBand2Q", 1.2f },
                               { "eqBand1DynMode", dynamic ? 1.0f : 0.0f }, { "eqBand1DynThresholdDb", -24.0f },
                               { "eqBand1DynRangeDb", dynamic ? -3.0f : 0.0f } });
                addModulePanel (dynamic ? "Add Dynamic EQ Module" : "Add Surgical EQ Module",
                                dynamic ? "Dynamic EQ" : "Surgical EQ", 560, 236,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::EqCurve, "EQ Curve", "eqMix", 18, 36, 260, 132);
                        addChildKnob (layout, panelId, "Low Freq", "eqBand1Freq", 304, 44);
                        addChildKnob (layout, panelId, "Low Gain", "eqBand1GainDb", 382, 44);
                        addChildKnob (layout, panelId, "Mid Freq", "eqBand2Freq", 460, 44);
                        addChildKnob (layout, panelId, "Mid Gain", "eqBand2GainDb", 304, 124);
                        if (dynamic)
                        {
                            addChildKnob (layout, panelId, "Dyn Thr", "eqBand1DynThresholdDb", 382, 124);
                            addChildKnob (layout, panelId, "Dyn Rng", "eqBand1DynRangeDb", 460, 124);
                        }
                        else
                        {
                            addChildKnob (layout, panelId, "Trim", "eqOutputTrimDb", 382, 124);
                            addChildToggle (layout, panelId, "EQ On", "eqEnabled", 462, 142, 72, 28);
                        }
                    });
            }
            else if (moduleKey == "limiter")
            {
                liveValues.setValue ("outputLimiter", 1.0f);
                ensureBlock ("limiter_module", "out", "limiter", "Limiter", "outputCeilingDb", "studio", "dynamics", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });
                addModulePanel ("Add Limiter Module", "Limiter", 310, 150,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildToggle (layout, panelId, "On", "outputLimiter", 20, 42, 70, 30);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 112, 40);
                        addChildKnob (layout, panelId, "Gain", "outputGainDb", 202, 40);
                    });
            }
            else
            {
                ensureBlock ("transient_module", "fx", "transientShaper", "Transient Shaper", "dynMix", "studio", "dynamics", "stereo",
                             { { "dynMix", 0.35f }, { "dynAttackMs", 4.0f }, { "dynReleaseMs", 90.0f }, { "dynRatio", 1.5f } });
                addModulePanel ("Add Transient Shaper Module", "Transient Shaper", 390, 164,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::Waveform, "Transient View", "dynMix", 18, 42, 126, 72);
                        addChildKnob (layout, panelId, "Attack", "dynAttackMs", 164, 42);
                        addChildKnob (layout, panelId, "Release", "dynReleaseMs", 244, 42);
                        addChildKnob (layout, panelId, "Mix", "dynMix", 324, 42);
                    });
            }
            finishModernModule();
            return;
        }
        if (moduleKey == "multitapdelay" || moduleKey == "flanger" || moduleKey == "vocalfx" || moduleKey == "masterbus")
        {
            ensureParams ({ "multiTapTime", "multiTapFeedback", "multiTapSpread", "multiTapMix",
                            "chorusRate", "chorusDepth", "chorusFeedback", "chorusMix",
                            "vocalFormant", "vocalBody", "vocalMix",
                            "eqEnabled", "eqMix", "dynThresholdDb", "dynRatio", "dynMix",
                            "stereoWidth", "monoMaker", "outputLimiter", "outputCeilingDb", "outputGainDb" }, "fx");
            if (moduleKey == "multitapdelay")
            {
                ensureBlock ("multitap_delay_module", "fx", "multiTapDelay", "MultiTap Delay", "multiTapMix", "creative", "space", "stereo",
                             { { "multiTapTime", 0.375f }, { "multiTapFeedback", 0.35f }, { "multiTapSpread", 0.45f }, { "multiTapMix", 0.32f } });
                addModulePanel ("Add MultiTap Delay Module", "MultiTap Delay", 410, 164,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildKnob (layout, panelId, "Time", "multiTapTime", 22, 42);
                        addChildKnob (layout, panelId, "Feedback", "multiTapFeedback", 106, 42);
                        addChildKnob (layout, panelId, "Spread", "multiTapSpread", 190, 42);
                        addChildKnob (layout, panelId, "Mix", "multiTapMix", 274, 42);
                    });
            }
            else if (moduleKey == "flanger")
            {
                ensureBlock ("flanger_module", "fx", "flanger", "Flanger", "chorusMix", "creative", "modulation", "stereo",
                             { { "chorusRate", 0.18f }, { "chorusDepth", 0.72f }, { "chorusFeedback", 0.34f }, { "chorusMix", 0.36f } });
                addModulePanel ("Add Flanger Module", "Flanger", 410, 164,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildKnob (layout, panelId, "Rate", "chorusRate", 22, 42);
                        addChildKnob (layout, panelId, "Depth", "chorusDepth", 106, 42);
                        addChildKnob (layout, panelId, "Feedback", "chorusFeedback", 190, 42);
                        addChildKnob (layout, panelId, "Mix", "chorusMix", 274, 42);
                    });
            }
            else if (moduleKey == "vocalfx")
            {
                ensureBlock ("vocal_fx_module", "fx", "vocalFormant", "Vocal FX", "vocalMix", "creative", "tone", "stereo",
                             { { "vocalFormant", 0.40f }, { "vocalBody", 0.35f }, { "vocalMix", 0.22f }, { "dynMix", 0.20f } });
                addModulePanel ("Add Vocal FX Module", "Vocal FX", 430, 180,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Voice View", "vocalMix", 18, 38, 146, 90);
                        addChildKnob (layout, panelId, "Formant", "vocalFormant", 184, 46);
                        addChildKnob (layout, panelId, "Body", "vocalBody", 266, 46);
                        addChildKnob (layout, panelId, "Mix", "vocalMix", 348, 46);
                    });
            }
            else
            {
                liveValues.setValue ("outputLimiter", 1.0f);
                ensureBlock ("master_bus_eq_module", "filter", "surgicalEq", "Master Bus EQ", "eqMix", "studio", "tone", "stereo",
                             { { "eqEnabled", 1.0f }, { "eqMix", 1.0f }, { "eqBand1On", 1.0f }, { "eqBand1Freq", 120.0f }, { "eqBand1GainDb", -0.8f } });
                ensureBlock ("master_bus_comp_module", "fx", "dynamics", "Master Bus Glue", "dynMix", "studio", "dynamics", "stereo",
                             { { "dynThresholdDb", -18.0f }, { "dynRatio", 2.0f }, { "dynMix", 0.28f } });
                ensureBlock ("master_bus_limiter_module", "out", "limiter", "Master Limiter", "outputCeilingDb", "studio", "dynamics", "stereo",
                             { { "outputLimiter", 1.0f }, { "outputCeilingDb", -0.8f }, { "outputGainDb", 0.0f } });
                addModulePanel ("Add Master Bus Module", "Master Bus", 620, 206,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Spectrum", "eqMix", 18, 38, 170, 92);
                        addChildKnob (layout, panelId, "EQ Mix", "eqMix", 210, 44);
                        addChildKnob (layout, panelId, "Glue", "dynMix", 292, 44);
                        addChildKnob (layout, panelId, "Width", "stereoWidth", 374, 44);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 456, 44);
                        addChildToggle (layout, panelId, "Limit", "outputLimiter", 536, 64, 72, 30);
                    });
            }
            finishModernModule();
            return;
        }

        if (moduleType.equalsIgnoreCase ("Chorus"))
        {
            juce::StringArray paramIds { "chorusRate", "chorusDepth", "chorusFeedback", "chorusMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("chorus"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock chorusBlock;
                chorusBlock.id = "chorus_module";
                chorusBlock.section = "fx";
                chorusBlock.type = "chorus";
                chorusBlock.name = "Chorus Block";
                chorusBlock.targetId = "chorusMix";
                chorusBlock.enabled = true;
                chorusBlock.values["chorusRate"] = 0.35f;
                chorusBlock.values["chorusDepth"] = 0.35f;
                chorusBlock.values["chorusFeedback"] = 0.0f;
                chorusBlock.values["chorusMix"] = 0.5f;
                graph.blocks.push_back (std::move (chorusBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Chorus Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Chorus Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 300;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Rate", "chorusRate", 25);
                    addChildKnob ("Depth", "chorusDepth", 115);
                    addChildKnob ("Mix", "chorusMix", 205);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Filter"))
        {
            juce::StringArray paramIds { "filterCutoff", "filterResonance" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "synth", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("stateVariable") || b.type.equalsIgnoreCase ("filter"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock filterBlock;
                filterBlock.id = "filter_module";
                filterBlock.section = "filter";
                filterBlock.type = "stateVariable";
                filterBlock.name = "Morph Filter Block";
                filterBlock.targetId = "filterCutoff";
                filterBlock.enabled = true;
                filterBlock.values["cutoff"] = 0.56f;
                filterBlock.values["resonance"] = 0.18f;
                graph.blocks.push_back (std::move (filterBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Filter Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Filter Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 360;
                    panel.height = 140;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    LayoutElement curve;
                    curve.type = ElementType::EqCurve;
                    curve.x = pos.x + 18;
                    curve.y = pos.y + 40;
                    curve.width = 170;
                    curve.height = 80;
                    curve.containerId = panelId;
                    curve.groupId = tabGroup;
                    curve.id = layout.generateUniqueId ("eqcurve_");
                    curve.accentColour = PatchCraftLookAndFeel::accent();
                    layout.add (curve);

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 40;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Cutoff", "filterCutoff", 205);
                    addChildKnob ("Reso", "filterResonance", 280);
                });
        }
        else if (moduleType.equalsIgnoreCase ("ADSR"))
        {
            juce::StringArray paramIds { "attack", "decay", "sustain", "release" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "synth", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("adsr") || b.type.equalsIgnoreCase ("envelope"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock envBlock;
                envBlock.id = "amp_env_module";
                envBlock.section = "amp";
                envBlock.type = "adsr";
                envBlock.name = "ADSR Envelope Block";
                envBlock.targetId = "attack";
                envBlock.enabled = true;
                envBlock.values["attack"] = 0.01f;
                envBlock.values["decay"] = 0.20f;
                envBlock.values["sustain"] = 0.80f;
                envBlock.values["release"] = 0.40f;
                graph.blocks.push_back (std::move (envBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add ADSR Envelope Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Amp Envelope";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 440;
                    panel.height = 180;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    LayoutElement curve;
                    curve.type = ElementType::AdsrCurve;
                    curve.x = pos.x + 18;
                    curve.y = pos.y + 40;
                    curve.width = 210;
                    curve.height = 115;
                    curve.containerId = panelId;
                    curve.groupId = tabGroup;
                    curve.id = layout.generateUniqueId ("adsrcurve_");
                    curve.accentColour = PatchCraftLookAndFeel::accent();
                    layout.add (curve);

                    auto addChildSlider = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement slider;
                        slider.type = ElementType::Slider;
                        slider.label = label;
                        slider.parameterId = paramId;
                        slider.x = pos.x + xOffset;
                        slider.y = pos.y + 30;
                        slider.width = 35;
                        slider.height = 120;
                        slider.containerId = panelId;
                        slider.groupId = tabGroup;
                        slider.id = layout.generateUniqueId ("slider_");
                        slider.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (slider);
                    };

                    addChildSlider ("A", "attack", 250);
                    addChildSlider ("D", "decay", 295);
                    addChildSlider ("S", "sustain", 340);
                    addChildSlider ("R", "release", 385);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Delay"))
        {
            juce::StringArray paramIds { "delayTime", "delayFeedback", "delayMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("delay"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock delayBlock;
                delayBlock.id = "delay_module";
                delayBlock.section = "fx";
                delayBlock.type = "delay";
                delayBlock.name = "Delay Block";
                delayBlock.targetId = "delayMix";
                delayBlock.enabled = true;
                delayBlock.values["delayMix"] = 0.25f;
                delayBlock.values["delayFeedback"] = 0.35f;
                delayBlock.values["delayTime"] = 0.25f;
                graph.blocks.push_back (std::move (delayBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Delay Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Delay Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 300;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Time", "delayTime", 25);
                    addChildKnob ("Feedback", "delayFeedback", 115);
                    addChildKnob ("Mix", "delayMix", 205);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Reverb"))
        {
            juce::StringArray paramIds { "reverbMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("reverb"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock reverbBlock;
                reverbBlock.id = "reverb_module";
                reverbBlock.section = "fx";
                reverbBlock.type = "reverb";
                reverbBlock.name = "Reverb Block";
                reverbBlock.targetId = "reverbMix";
                reverbBlock.enabled = true;
                reverbBlock.values["reverbMix"] = 0.35f;
                graph.blocks.push_back (std::move (reverbBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Reverb Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Reverb Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 120;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Mix", "reverbMix", 25);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Phaser"))
        {
            juce::StringArray paramIds { "phaserRate", "phaserDepth", "phaserFeedback", "phaserMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("phaser"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock phaserBlock;
                phaserBlock.id = "phaser_module";
                phaserBlock.section = "fx";
                phaserBlock.type = "phaser";
                phaserBlock.name = "Phaser Block";
                phaserBlock.targetId = "phaserMix";
                phaserBlock.enabled = true;
                phaserBlock.values["phaserRate"] = 0.25f;
                phaserBlock.values["phaserDepth"] = 0.45f;
                phaserBlock.values["phaserFeedback"] = 0.0f;
                phaserBlock.values["phaserMix"] = 0.5f;
                graph.blocks.push_back (std::move (phaserBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Phaser Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Phaser Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 390;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Rate", "phaserRate", 25);
                    addChildKnob ("Depth", "phaserDepth", 115);
                    addChildKnob ("Feedback", "phaserFeedback", 205);
                    addChildKnob ("Mix", "phaserMix", 295);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Tape"))
        {
            juce::StringArray paramIds { "tapeDrive", "tapeTone", "tapeFlutter", "tapeMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("tape"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock tapeBlock;
                tapeBlock.id = "tape_module";
                tapeBlock.section = "fx";
                tapeBlock.type = "tape";
                tapeBlock.name = "Tape Block";
                tapeBlock.targetId = "tapeMix";
                tapeBlock.enabled = true;
                tapeBlock.values["tapeDrive"] = 0.25f;
                tapeBlock.values["tapeTone"] = 0.55f;
                tapeBlock.values["tapeFlutter"] = 0.12f;
                tapeBlock.values["tapeMix"] = 0.5f;
                graph.blocks.push_back (std::move (tapeBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Tape Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Tape Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 390;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Drive", "tapeDrive", 25);
                    addChildKnob ("Tone", "tapeTone", 115);
                    addChildKnob ("Flutter", "tapeFlutter", 205);
                    addChildKnob ("Mix", "tapeMix", 295);
                });
        }
        else if (moduleType.equalsIgnoreCase ("LoFi"))
        {
            juce::StringArray paramIds { "lofiBits", "lofiRate", "lofiMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("lofi"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock lofiBlock;
                lofiBlock.id = "lofi_module";
                lofiBlock.section = "fx";
                lofiBlock.type = "lofi";
                lofiBlock.name = "Lo-Fi Block";
                lofiBlock.targetId = "lofiMix";
                lofiBlock.enabled = true;
                lofiBlock.values["lofiBits"] = 12.0f;
                lofiBlock.values["lofiRate"] = 0.20f;
                lofiBlock.values["lofiMix"] = 0.5f;
                graph.blocks.push_back (std::move (lofiBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Lo-Fi Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Lo-Fi Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 300;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Bits", "lofiBits", 25);
                    addChildKnob ("Rate", "lofiRate", 115);
                    addChildKnob ("Mix", "lofiMix", 205);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Dynamics"))
        {
            juce::StringArray paramIds { "dynThresholdDb", "dynRatio", "dynAttackMs", "dynReleaseMs", "dynMakeupDb", "dynMix" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("dynamics"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock dynBlock;
                dynBlock.id = "dynamics_module";
                dynBlock.section = "fx";
                dynBlock.type = "dynamics";
                dynBlock.name = "Dynamics Block";
                dynBlock.targetId = "dynMix";
                dynBlock.enabled = true;
                dynBlock.values["dynThresholdDb"] = -18.0f;
                dynBlock.values["dynRatio"] = 2.0f;
                dynBlock.values["dynAttackMs"] = 10.0f;
                dynBlock.values["dynReleaseMs"] = 120.0f;
                dynBlock.values["dynMakeupDb"] = 0.0f;
                dynBlock.values["dynMix"] = 0.5f;
                graph.blocks.push_back (std::move (dynBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Dynamics Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Dynamics Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 300;
                    panel.height = 200;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset, int yOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + yOffset;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Thresh", "dynThresholdDb", 25, 35);
                    addChildKnob ("Ratio", "dynRatio", 115, 35);
                    addChildKnob ("Mix", "dynMix", 205, 35);

                    addChildKnob ("Attack", "dynAttackMs", 25, 115);
                    addChildKnob ("Release", "dynReleaseMs", 115, 115);
                    addChildKnob ("Makeup", "dynMakeupDb", 205, 115);
                });
        }
        else if (moduleType.equalsIgnoreCase ("Stereo"))
        {
            juce::StringArray paramIds { "stereoWidth", "monoMaker" };
            for (const auto& paramId : paramIds)
            {
                if (pm.find (paramId) == nullptr)
                {
                    ParameterDef def;
                    if (ParameterModel::getRegistryDefinition (paramId, "fx", def))
                    {
                        pm.add (def);
                        liveValues.getOrAddRaw (def.id, def.defaultValue);
                    }
                }
            }

            bool dspExists = false;
            for (const auto& b : graph.blocks)
            {
                if (b.type.equalsIgnoreCase ("utility"))
                {
                    dspExists = true;
                    break;
                }
            }

            if (! dspExists)
            {
                DspBlock utilityBlock;
                utilityBlock.id = "stereo_module";
                utilityBlock.section = "out";
                utilityBlock.type = "utility";
                utilityBlock.name = "Stereo Utility Block";
                utilityBlock.targetId = "stereoWidth";
                utilityBlock.enabled = true;
                utilityBlock.values["stereoWidth"] = 1.0f;
                utilityBlock.values["monoMaker"] = 0.0f;
                graph.blocks.push_back (std::move (utilityBlock));
                graph.userConfigured = true;
            }

            owner.getProject().performLayoutEdit ("Add Stereo Module Layout",
                [&] (LayoutModel& layout)
                {
                    LayoutElement panel;
                    panel.type = ElementType::Panel;
                    panel.label = "Stereo Module";
                    panel.x = pos.x;
                    panel.y = pos.y;
                    panel.width = 210;
                    panel.height = 130;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildKnob = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement knob;
                        knob.type = ElementType::Knob;
                        knob.label = label;
                        knob.parameterId = paramId;
                        knob.x = pos.x + xOffset;
                        knob.y = pos.y + 35;
                        knob.width = 65;
                        knob.height = 65;
                        knob.containerId = panelId;
                        knob.groupId = tabGroup;
                        knob.id = layout.generateUniqueId ("knob_");
                        knob.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (knob);
                    };

                    addChildKnob ("Width", "stereoWidth", 25);
                    addChildKnob ("Mono", "monoMaker", 115);
                });
        }

        owner.getProject().notifyChanged();
        repaint();
    }

    void CanvasEditor::copySelectedArpLanePattern()
    {
        copiedArpLanePattern.clear();

        const auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId());
        const auto* block = findArpBlock (owner.getProject().getDspGraph());
        if (selected == nullptr || selected->type != ElementType::ArpLane || block == nullptr)
            return;

        const int lane = juce::jlimit (0, 15, selected->arpLaneIndex);
        const auto prefix = arpBankPrefix (lane);
        for (const auto& value : block->values)
        {
            if (! value.first.startsWith (prefix))
                continue;

            const auto key = value.first.substring (prefix.length());
            if (isArpLanePatternKey (key))
                copiedArpLanePattern[key] = value.second;
        }

        if (copiedArpLanePattern.empty() && lane == 0)
            for (const auto& value : block->values)
                if (isArpLanePatternKey (value.first))
                    copiedArpLanePattern[value.first] = value.second;
    }

    void CanvasEditor::pasteArpLanePatternToSelection()
    {
        if (copiedArpLanePattern.empty())
            return;

        auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (selected == nullptr || selected->type != ElementType::ArpLane)
            return;

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureArpBlock (graph);
        const int lane = juce::jlimit (0, 15, selected->arpLaneIndex);
        block.values["mpActiveBank"] = (float) lane;
        block.values["mpMultiLane"] = 1.0f;

        for (const auto& value : copiedArpLanePattern)
            setArpLaneValue (block, lane, value.first, value.second);

        graph.userConfigured = true;
        owner.getProject().markDirty();
        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        repaint();
    }

    void CanvasEditor::resetSelectedArpLanePattern()
    {
        auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (selected == nullptr || selected->type != ElementType::ArpLane)
            return;

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureArpBlock (graph);
        const int lane = juce::jlimit (0, 15, selected->arpLaneIndex);
        const int steps = juce::jlimit (1, 128, selected->arpLaneSteps);

        block.values["mpActiveBank"] = (float) lane;
        block.values["mpMultiLane"] = 1.0f;
        setArpLaneValue (block, lane, "arpSteps", (float) steps);
        setArpLaneValue (block, lane, "mpLaneMute", 0.0f);
        setArpLaneValue (block, lane, "mpLaneSolo", 0.0f);
        setArpLaneValue (block, lane, "mpLaneRetrigger", 1.0f);

        for (int step = 0; step < 128; ++step)
        {
            const auto suffix = juce::String (step);
            setArpLaneValue (block, lane, "mpStep" + suffix + "On", 0.0f);
            setArpLaneValue (block, lane, "arpNote" + suffix, 0.0f);
            setArpLaneValue (block, lane, "mpVelocity" + suffix, 0.30f);
            setArpLaneValue (block, lane, "mpGate" + suffix, 0.58f);
            setArpLaneValue (block, lane, "mpStepProb" + suffix, 1.0f);
            setArpLaneValue (block, lane, "mpStepDiv" + suffix, 1.0f);
            setArpLaneValue (block, lane, "mpStepDelay" + suffix, 0.0f);
            setArpLaneValue (block, lane, "mpStepTranspose" + suffix, 0.0f);
            setArpLaneValue (block, lane, "mpAutoFilter" + suffix, 0.0f);
            setArpLaneValue (block, lane, "mpAutoPan" + suffix, 0.0f);
            setArpLaneValue (block, lane, "mpAutoFxSend" + suffix, 0.0f);
        }

        graph.userConfigured = true;
        owner.getProject().markDirty();
        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        repaint();
    }

    void CanvasEditor::showContextMenu (juce::Point<int> screenPos, bool forceMainMenu)
    {
        const auto canvasPos = screenToCanvas (screenPos);
        juce::String clickedElementId;
        bool clickedAssignableControl = false;
        if (! forceMainMenu)
        {
            for (auto it = owner.getProject().getLayout().getAll().rbegin();
                 it != owner.getProject().getLayout().getAll().rend(); ++it)
            {
                if (! it->visible || it->type == ElementType::Group || ! isElementOnCurrentTab (*it))
                    continue;
                if (! elementScreenRect (*it).contains (screenPos))
                    continue;

                clickedElementId = it->id;
                clickedAssignableControl = it->type == ElementType::Knob
                                        || it->type == ElementType::Slider
                                        || it->type == ElementType::Button
                                        || it->type == ElementType::Toggle
                                        || it->type == ElementType::Dropdown
                                        || it->type == ElementType::ValueDisplay
                                        || it->type == ElementType::MacroControl
                                        || it->type == ElementType::SampleDropZone;
                break;
            }
        }

        if (clickedElementId.isNotEmpty() && ! owner.isElementSelected (clickedElementId))
            owner.setSelectedElementId (clickedElementId);

        const auto* selectedForMenu = owner.getProject().getLayout().find (owner.getSelectedElementId());
        const bool selectedArpLane = selectedForMenu != nullptr && selectedForMenu->type == ElementType::ArpLane;

        bool canCreatePscriptHandler = false;
        bool canAttachPscriptFile = false;
        bool canDetachPscript = false;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* selected = owner.getProject().getLayout().find (id))
            {
                if ((isRuntimeControlElement (selected->type)
                     || selected->type == ElementType::ValueDisplay
                     || selected->type == ElementType::SampleDropZone)
                    && selected->parameterId.isNotEmpty()
                    && owner.getProject().getParameters().find (selected->parameterId) != nullptr)
                {
                    canCreatePscriptHandler = true;
                    canAttachPscriptFile = true;
                }
                if (selected->pscriptFile.isNotEmpty())
                    canDetachPscript = true;
            }

        juce::PopupMenu menu;
        menu.addItem (99, "Search Canvas Actions...");
        if (canCreatePscriptHandler || canAttachPscriptFile || canDetachPscript || ! owner.getSelectedElementIds().isEmpty())
        {
            menu.addSectionHeader ("pScript");
            menu.addItem (390, "Open pScript Editor", true);
            menu.addItem (38, "Create Handler For Selection", canCreatePscriptHandler);
            menu.addItem (391, "Attach pScript File...", canAttachPscriptFile);
            menu.addItem (392, "Detach pScript From Selection", canDetachPscript);
            menu.addSeparator();
        }
        menu.addSeparator();
        menu.addSectionHeader ("Add Container");
        menu.addItem (1, "Panel / Container");
        menu.addItem (2, "Folder Group");
        menu.addItem (3, "Tab Panel");
        menu.addSeparator();
        menu.addSectionHeader ("Add Shape");
        menu.addItem (5, "Rounded Rectangle");
        menu.addItem (6, "Ellipse");
        menu.addItem (7, "Triangle");
        menu.addItem (8, "Diamond");
        menu.addItem (9, "Line");
        menu.addSeparator();
        const bool directAssignMode = clickedAssignableControl && ! owner.getSelectedElementIds().isEmpty();
        menu.addSectionHeader (directAssignMode ? "Assign Selected Element" : "Add DSP Control");
        menu.addItem (11, "Find Parameter...");
        menu.addItem (12, "Add Arpeggiator (MIDI Playground)...");
        menu.addItem (14, "Add Mixer");
        menu.addItem (24, "Add Single Mixer Channel");
        menu.addItem (25, "Break Selected Mixer Into Channels", selectionContainsMixer());
        menu.addItem (20, "Add Macro Control");
        menu.addItem (21, "Add Mod Matrix");
        menu.addItem (22, "Add Granular Field");
        menu.addItem (23, "Add Drum Machine Surface");
        menu.addItem (26, "Add Arp Studio Lane");
        menu.addItem (27, "Add CircleSEQ Musical Surface");
        menu.addItem (28, "Add Animation Lab Visual Kit");
        menu.addSeparator();
        if (selectedArpLane)
        {
            menu.addSectionHeader ("Circle Pattern");
            menu.addItem (31, "Copy Circle Pattern");
            menu.addItem (32, "Paste Pattern To This Circle", ! copiedArpLanePattern.empty());
            menu.addItem (33, "Reset Circle To No Sound");
            menu.addSeparator();
        }

        // Group parameters by ParameterDef::category so the menu doesn't dump
        // every parameter as a flat 50-row wall of text. Each category becomes
        // its own submenu; the user reaches a parameter via Add DSP Control →
        // <category> → <param>. Items without a category fall under "Other".
        std::map<juce::String, std::vector<const ParameterDef*>> byCategory;
        juce::StringArray categoryOrder;
        for (const auto& def : owner.getProject().getParameters().getAll())
        {
            auto cat = def.category.isNotEmpty() ? def.category : juce::String ("Other");
            if (! byCategory.count (cat))
                categoryOrder.add (cat);
            byCategory[cat].push_back (&def);
        }
        int itemId = 100;
        std::map<int, juce::String> paramByItem;
        std::map<int, juce::String> assignParamByItem;
        for (const auto& cat : categoryOrder)
        {
            const auto& defs = byCategory[cat];
            juce::PopupMenu sub;
            for (const auto* def : defs)
            {
                sub.addItem (itemId, def->name + "  (" + def->id + ")");
                if (directAssignMode)
                    assignParamByItem[itemId++] = def->id;
                else
                    paramByItem[itemId++] = def->id;
            }
            menu.addSubMenu (cat, sub);
        }

        if (! owner.getSelectedElementIds().isEmpty())
        {
            juce::PopupMenu assignMenu;
            assignMenu.addItem (13, "Find Parameter...");
            assignMenu.addSeparator();
            int assignItemId = 10000;
            for (const auto& cat : categoryOrder)
            {
                juce::PopupMenu sub;
                for (const auto* def : byCategory[cat])
                {
                    sub.addItem (assignItemId, def->name + "  (" + def->id + ")");
                    assignParamByItem[assignItemId++] = def->id;
                }
                assignMenu.addSubMenu (cat, sub);
            }
            menu.addSubMenu ("Assign Selected To Parameter", assignMenu);
        }
        juce::String prerequisiteId;
        float prerequisiteValue = 1.0f;
        if (auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId()))
        {
            if (auto* parameter = owner.getProject().getParameters().find (selected->parameterId))
            {
                if (parameter->enabledBy.isNotEmpty()
                    && ! canvasParameterIsEnabled (owner.getProject(), *parameter))
                {
                    if (auto* prerequisite = owner.getProject().getParameters().find (parameter->enabledBy))
                    {
                        prerequisiteId = prerequisite->id;
                        prerequisiteValue = prerequisite->displayMode == "toggle"
                            ? prerequisite->max
                            : juce::jmax (prerequisite->defaultValue,
                                          prerequisite->min + (prerequisite->max - prerequisite->min) * 0.5f);
                        menu.addItem (10, "Enable Prerequisite: "
                            + (prerequisite->name.isNotEmpty() ? prerequisite->name : prerequisite->id), true);
                    }
                }
            }
        }
        menu.addSeparator();
        bool canDetachLabels = false;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* selected = owner.getProject().getLayout().find (id))
                if (isRuntimeControlElement (selected->type)
                    && selected->labelPosition != "hidden"
                    && (selected->label.isNotEmpty() || selected->parameterId.isNotEmpty()))
                    canDetachLabels = true;
        menu.addItem (15, "Detach Labels From Selection", canDetachLabels);
        menu.addItem (36, "Hide Labels For Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (37, "Show Labels For Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (34, "Convert Shape/Text To Button", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (35, "Convert Selection To Label", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (16, "Copy Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (17, "Copy Selection Without Parameters", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (18, "Paste Elements", owner.hasCopiedElements());
        menu.addItem (19, "Copy Selection To All Tabs", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (4, "Create Group From Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addSeparator();
        juce::PopupMenu animationMenu;
        animationMenu.addItem (401, "None");
        animationMenu.addItem (402, "Breathe");
        animationMenu.addItem (403, "Pulse");
        animationMenu.addItem (404, "Glow");
        animationMenu.addItem (405, "Shake");
        animationMenu.addItem (406, "Audio Reactive Glow");
        menu.addSubMenu ("Assign Visual Automation", animationMenu, ! owner.getSelectedElementIds().isEmpty());
        menu.addSeparator();
        juce::PopupMenu alignMenu;
        alignMenu.addItem (201, "Left", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (202, "Horizontal Center", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (203, "Right", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addSeparator();
        alignMenu.addItem (204, "Top", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (205, "Vertical Center", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addItem (206, "Bottom", owner.getSelectedElementIds().size() >= 2);
        alignMenu.addSeparator();
        alignMenu.addItem (207, "Distribute Horizontally", owner.getSelectedElementIds().size() >= 3);
        alignMenu.addItem (208, "Distribute Vertically", owner.getSelectedElementIds().size() >= 3);
        menu.addSubMenu ("Align / Distribute", alignMenu);
        juce::PopupMenu orderMenu;
        orderMenu.addItem (301, "Bring to Front", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (302, "Bring Forward", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (303, "Send Backward", ! owner.getSelectedElementIds().isEmpty());
        orderMenu.addItem (304, "Send to Back", ! owner.getSelectedElementIds().isEmpty());
        menu.addSubMenu ("Arrange Order", orderMenu);

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this, canvasPos, screenPos, paramByItem, assignParamByItem, prerequisiteId, prerequisiteValue, directAssignMode] (int result)
            {
                if (result == 1) addElementAt (ElementType::Panel, canvasPos);
                else if (result == 2) addElementAt (ElementType::Group, canvasPos);
                else if (result == 3) addElementAt (ElementType::TabPanel, canvasPos);
                else if (result == 99)
                {
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchCanvasActionPicker (this, anchor,
                        [safe, canvasPos, screenPos] (const juce::String& actionId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr) return;

                            if (actionId == "findParameter")
                            {
                                auto entries = collectParameterEntries (c->owner.getProject());
                                const juce::Rectangle<int> parameterAnchor (screenPos.x, screenPos.y, 1, 1);
                                launchParameterPicker (c, parameterAnchor, std::move (entries),
                                    [safe, canvasPos] (const juce::String& paramId)
                                    {
                                        if (auto* canvas = safe.getComponent())
                                            canvas->addElementAt (ElementType::Knob, canvasPos, paramId);
                                    });
                            }
                            else if (actionId == "panel") c->addElementAt (ElementType::Panel, canvasPos);
                            else if (actionId == "tabPanel") c->addElementAt (ElementType::TabPanel, canvasPos);
                            else if (actionId == "roundedRect") c->addElementAt (ElementType::Shape, canvasPos);
                            else if (actionId == "ellipse")
                            {
                                c->addElementAt (ElementType::Shape, canvasPos);
                                const auto id = c->owner.getSelectedElementId();
                                c->owner.getProject().performLayoutEdit ("Set shape kind",
                                    [id] (LayoutModel& m)
                                    {
                                        if (auto* el = m.find (id))
                                            el->shapeKind = "ellipse";
                                    });
                            }
                            else if (actionId == "circleSeqBg") c->addCircleSeqBackgroundKit (canvasPos);
                            else if (actionId == "mixer") c->addElementAt (ElementType::Mixer, canvasPos);
                            else if (actionId == "mixerChannel") c->addMixerChannelAt (canvasPos);
                            else if (actionId == "explodeMixer") c->explodeSelectedMixers();
                            else if (actionId == "macro") c->addElementAt (ElementType::MacroControl, canvasPos);
                            else if (actionId == "modMatrix") c->addElementAt (ElementType::ModMatrix, canvasPos);
                            else if (actionId == "granular") c->addElementAt (ElementType::GranularField, canvasPos);
                            else if (actionId == "arpLane") c->addElementAt (ElementType::ArpLane, canvasPos);
                            else if (actionId == "orbitInstrument") c->addCircleSeqInstrumentLayout (canvasPos);
                            else if (actionId == "visualKit") c->addVisualReactivityControlLayout (canvasPos);
                            else if (actionId == "reactiveImage") c->addElementAt (ElementType::ReactiveImage, canvasPos);
                            else if (actionId == "spriteAnimator") c->addElementAt (ElementType::SpriteAnimator, canvasPos);
                            else if (actionId == "visualFx") c->addElementAt (ElementType::VisualFxLayer, canvasPos);
                            else if (actionId == "aiVisualPrompt") c->addElementAt (ElementType::AiVisualPrompt, canvasPos);
                            else if (actionId == "drumMachine") c->addDrumMachineControlLayout (canvasPos);
                            else if (actionId == "bpm") c->addElementAt (ElementType::Knob, canvasPos, "projectBpm");
                            else if (actionId == "bpmSync") c->addElementAt (ElementType::Toggle, canvasPos, "bpmSync");
                            else if (actionId == "retrigger") c->addElementAt (ElementType::Toggle, canvasPos, "retrigger");
                            else if (actionId == "copy") c->owner.copySelectedElements (true);
                            else if (actionId == "copyNoParams") c->owner.copySelectedElements (false);
                            else if (actionId == "paste") c->owner.pasteCopiedElements();
                            else if (actionId == "copyTabs") c->owner.copySelectedToAllTabs();
                            else if (actionId == "group") c->owner.groupSelectedElements();
                        });
                }
                else if (result == 11)
                {
                    // Search-driven parameter picker: on a selected control it
                    // assigns; on empty canvas it creates a connected knob.
                    // filter instead of cascading category submenus.
                    auto entries = collectParameterEntries (owner.getProject());
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchParameterPicker (this, anchor, std::move (entries),
                        [safe, canvasPos, directAssignMode] (const juce::String& paramId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr)
                                return;

                            if (! directAssignMode)
                            {
                                c->addElementAt (ElementType::Knob, canvasPos, paramId);
                                return;
                            }

                            const auto ids = c->owner.getSelectedElementIds();
                            const auto* def = c->owner.getProject().getParameters().find (paramId);
                            const auto label = def != nullptr ? def->name : juce::String();
                            c->owner.getProject().performLayoutEdit ("Assign parameter",
                                [ids, paramId, label] (LayoutModel& m)
                                {
                                    for (const auto& id : ids)
                                        if (auto* el = m.find (id))
                                        {
                                            const bool assignable = el->type == ElementType::Knob
                                                || el->type == ElementType::Slider
                                                || el->type == ElementType::Button
                                                || el->type == ElementType::Toggle
                                                || el->type == ElementType::Dropdown
                                                || el->type == ElementType::ValueDisplay
                                                || el->type == ElementType::MacroControl
                                                || el->type == ElementType::SampleDropZone;
                                            if (! assignable)
                                                continue;
                                            el->parameterId = paramId;
                                            if (el->label.isEmpty() && label.isNotEmpty())
                                                el->label = label;
                                        }
                                });
                            c->repaint();
                        });
                }
                else if (result == 12)
                {
                    // One-click: switch to the DSP builder's MIDI tab and
                    // drop in an arpeggiator block ready for editing.
                    owner.addArpBlock();
                }
                else if (result == 14)
                {
                    addElementAt (ElementType::Mixer, canvasPos);
                }
                else if (result == 24)
                {
                    addMixerChannelAt (canvasPos);
                }
                else if (result == 25)
                {
                    explodeSelectedMixers();
                }
                else if (result == 20)
                {
                    addElementAt (ElementType::MacroControl, canvasPos);
                }
                else if (result == 21)
                {
                    addElementAt (ElementType::ModMatrix, canvasPos);
                }
                else if (result == 22)
                {
                    addElementAt (ElementType::GranularField, canvasPos);
                }
                else if (result == 23)
                {
                    addDrumMachineControlLayout (canvasPos);
                }
                else if (result == 34)
                {
                    const auto ids = owner.getSelectedElementIds();
                    owner.getProject().performLayoutEdit ("Convert selection to button", [ids] (LayoutModel& m)
                    {
                        for (const auto& id : ids)
                            if (auto* el = m.find (id); el != nullptr
                                && (el->type == ElementType::Shape || el->type == ElementType::Label))
                            {
                                el->type = ElementType::Button;
                                if (el->label.isEmpty())
                                    el->label = "Button";
                            }
                    });
                    owner.refreshAllPanels();
                }
                else if (result == 35)
                {
                    const auto ids = owner.getSelectedElementIds();
                    owner.getProject().performLayoutEdit ("Convert selection to label", [ids] (LayoutModel& m)
                    {
                        for (const auto& id : ids)
                            if (auto* el = m.find (id); el != nullptr)
                                el->type = ElementType::Label;
                    });
                    owner.refreshAllPanels();
                }
                else if (result == 36)
                {
                    owner.setSelectedLabelVisibility (false);
                }
                else if (result == 37)
                {
                    owner.setSelectedLabelVisibility (true);
                }
                else if (result == 38)
                {
                    owner.createPscriptHandlerForSelectedControl();
                }
                else if (result == 391)
                {
                    owner.attachPscriptFileToSelectedControl();
                }
                else if (result == 392)
                {
                    owner.detachPscriptFromSelectedControl();
                }
                else if (result == 390)
                {
                    owner.focusPscriptPanel();
                }
                else if (result == 26)
                {
                    addElementAt (ElementType::ArpLane, canvasPos);
                }
                else if (result == 27)
                {
                    addCircleSeqInstrumentLayout (canvasPos);
                }
                else if (result == 28)
                {
                    addVisualReactivityControlLayout (canvasPos);
                }
                else if (result == 31)
                {
                    copySelectedArpLanePattern();
                }
                else if (result == 32)
                {
                    pasteArpLanePatternToSelection();
                }
                else if (result == 33)
                {
                    resetSelectedArpLanePattern();
                }
                else if (result == 15)
                {
                    owner.detachLabelsFromSelectedControls();
                }
                else if (result == 16)
                {
                    owner.copySelectedElements (true);
                }
                else if (result == 17)
                {
                    owner.copySelectedElements (false);
                }
                else if (result == 18)
                {
                    owner.pasteCopiedElements();
                }
                else if (result == 19)
                {
                    owner.copySelectedToAllTabs();
                }
                else if (result == 13)
                {
                    // Searchable "Assign Selected To Parameter".
                    auto entries = collectParameterEntries (owner.getProject());
                    const juce::Rectangle<int> anchor (screenPos.x, screenPos.y, 1, 1);
                    juce::Component::SafePointer<CanvasEditor> safe (this);
                    launchParameterPicker (this, anchor, std::move (entries),
                        [safe] (const juce::String& paramId)
                        {
                            auto* c = safe.getComponent();
                            if (c == nullptr) return;
                            const auto ids = c->owner.getSelectedElementIds();
                            const auto label = c->owner.getProject().getParameters().find (paramId) != nullptr
                                ? c->owner.getProject().getParameters().find (paramId)->name
                                : juce::String();
                            c->owner.getProject().performLayoutEdit ("Assign parameter",
                                [ids, paramId, label] (LayoutModel& m)
                                {
                                    for (const auto& id : ids)
                                    {
                                        if (auto* el = m.find (id))
                                        {
                                            const bool assignable = el->type == ElementType::Knob
                                                || el->type == ElementType::Slider
                                                || el->type == ElementType::Button
                                                || el->type == ElementType::Toggle
                                                || el->type == ElementType::Dropdown
                                                || el->type == ElementType::ValueDisplay
                                                || el->type == ElementType::MacroControl
                                                || el->type == ElementType::SampleDropZone;
                                            if (! assignable) continue;
                                            el->parameterId = paramId;
                                            if (el->label.isEmpty() && label.isNotEmpty())
                                                el->label = label;
                                        }
                                    }
                                });
                            c->repaint();
                        });
                }
                else if (result >= 5 && result <= 9)
                {
                    addElementAt (ElementType::Shape, canvasPos);
                    const auto id = owner.getSelectedElementId();
                    juce::String shapeKind;
                    if (result == 6) shapeKind = "ellipse";
                    if (result == 7) shapeKind = "triangle";
                    if (result == 8) shapeKind = "diamond";
                    if (result == 9) shapeKind = "line";
                    if (shapeKind.isNotEmpty())
                        owner.getProject().performLayoutEdit ("Set shape kind",
                            [id, shapeKind] (LayoutModel& m)
                            {
                                if (auto* el = m.find (id))
                                    el->shapeKind = shapeKind;
                            });
                }
                else if (result == 4)
                {
                    owner.groupSelectedElements();
                }
                else if (result == 10 && prerequisiteId.isNotEmpty())
                {
                    owner.getProject().getLiveValues().setValue (prerequisiteId, prerequisiteValue);
                    applyArpLaneParameterToGraph (owner.getProject(), prerequisiteId);
                    owner.getProject().notifyChanged();
                    repaint();
                }
                else if (result >= 401 && result <= 406)
                {
                    const auto ids = owner.getSelectedElementIds();
                    owner.getProject().performLayoutEdit ("Assign visual automation",
                        [ids, result] (LayoutModel& m)
                        {
                            for (const auto& id : ids)
                            {
                                if (auto* el = m.find (id))
                                {
                                    el->animationRate = result == 405 ? 2.0f
                                                       : result == 402 ? 0.55f
                                                       : 1.0f;
                                    el->audioReactive = result == 406;
                                    el->audioReactiveMode = "level";
                                    el->audioReactiveAmount = result == 406 ? 0.85f : 0.35f;
                                    el->animationMode = result == 401 ? "none"
                                                      : result == 402 ? "breathe"
                                                      : result == 403 ? "pulse"
                                                      : result == 404 ? "glow"
                                                      : result == 405 ? "shake"
                                                      : "glow";
                                }
                            }
                        });
                    repaint();
                }
                else if (result == 201) owner.alignSelected ("left");
                else if (result == 202) owner.alignSelected ("hcenter");
                else if (result == 203) owner.alignSelected ("right");
                else if (result == 204) owner.alignSelected ("top");
                else if (result == 205) owner.alignSelected ("vcenter");
                else if (result == 206) owner.alignSelected ("bottom");
                else if (result == 207) owner.distributeSelected (true);
                else if (result == 208) owner.distributeSelected (false);
                else if (result == 301) owner.orderSelected ("front");
                else if (result == 302) owner.orderSelected ("forward");
                else if (result == 303) owner.orderSelected ("backward");
                else if (result == 304) owner.orderSelected ("back");
                else if (auto it = assignParamByItem.find (result); it != assignParamByItem.end())
                {
                    const auto paramId = it->second;
                    const auto ids = owner.getSelectedElementIds();
                    const auto* def = owner.getProject().getParameters().find (paramId);
                    const auto label = def != nullptr ? def->name : juce::String();
                    owner.getProject().performLayoutEdit ("Assign parameter",
                        [ids, paramId, label] (LayoutModel& m)
                        {
                            for (const auto& id : ids)
                            {
                                if (auto* el = m.find (id))
                                {
                                    const bool assignable = el->type == ElementType::Knob
                                        || el->type == ElementType::Slider
                                        || el->type == ElementType::Button
                                        || el->type == ElementType::Toggle
                                        || el->type == ElementType::Dropdown
                                        || el->type == ElementType::ValueDisplay
                                        || el->type == ElementType::MacroControl
                                        || el->type == ElementType::SampleDropZone;
                                    if (! assignable)
                                        continue;
                                    el->parameterId = paramId;
                                    if (el->label.isEmpty() && label.isNotEmpty())
                                        el->label = label;
                                }
                            }
                        });
                    repaint();
                }
                else if (auto it = paramByItem.find (result); it != paramByItem.end())
                    addElementAt (ElementType::Knob, canvasPos, it->second);
            });
    }

