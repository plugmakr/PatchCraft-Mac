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
                 : type == ElementType::SpectrumAnalyzer ? 460
                 : type == ElementType::ReactiveImage ? 320
                 : type == ElementType::SpriteAnimator ? 260
                 : type == ElementType::VisualFxLayer ? 420
                 : type == ElementType::AiVisualPrompt ? 360
                 : type == ElementType::SampleDropZone ? 320
                 : type == ElementType::RuntimeSampleLibrary ? 360
                 : type == ElementType::DrumGrid ? 560
                 : type == ElementType::ArpLane ? 260
                 : type == ElementType::Mixer ? 520
                 : type == ElementType::MacroControl ? 190
                 : type == ElementType::ModMatrix ? 420
                 : type == ElementType::PadGrid ? 360
                 : type == ElementType::DrumPad ? 80
                 : type == ElementType::Toggle ? 128 : 96;
        el.height = (type == ElementType::Panel) ? 180
                  : type == ElementType::Shape ? 120
                  : type == ElementType::TabPanel ? 44
                  : type == ElementType::GranularField ? 220
                  : type == ElementType::EqCurve ? 180
                  : type == ElementType::SpectrumAnalyzer ? 160
                  : type == ElementType::ReactiveImage ? 200
                  : type == ElementType::SpriteAnimator ? 180
                  : type == ElementType::VisualFxLayer ? 180
                  : type == ElementType::AiVisualPrompt ? 170
                  : type == ElementType::SampleDropZone ? 156
                  : type == ElementType::RuntimeSampleLibrary ? 220
                  : type == ElementType::DrumGrid ? 220
                  : type == ElementType::ArpLane ? 330
                  : type == ElementType::Mixer ? 260
                  : type == ElementType::MacroControl ? 132
                  : type == ElementType::ModMatrix ? 220
                  : type == ElementType::PadGrid ? 360
                  : type == ElementType::DrumPad ? 80
                  : type == ElementType::Toggle ? 54 : 96;
        el.parameterId = std::move (parameterId);
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
            el.borderColour = PatchCraftLookAndFeel::accent();
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
                pads.width = 300;
                pads.height = 300;
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
            owner.getProject().notifyChanged();
            repaint();
        };

        const auto moduleKey = moduleType.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").toLowerCase();
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

            addModulePanel ("Add Synth Plugin Starter", "Synth Plugin Starter", 780, 430,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::Label, "SOURCE", juce::String(), 22, 28, 100, 24);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Wavetable", "wtPosition", 22, 58, 190, 82);
                    addChildKnob (layout, panelId, "Wave", "oscType", 234, 58);
                    addChildKnob (layout, panelId, "WT Pos", "wtPosition", 316, 58);
                    addChildKnob (layout, panelId, "Blend", "oscBlend", 398, 58);
                    addChildKnob (layout, panelId, "Sub", "subBlend", 480, 58);
                    addChildValue (layout, panelId, "Noise Off", "noiseBlend", 566, 78, 78, 30);

                    addChildSurface (layout, panelId, ElementType::Label, "SHAPE", juce::String(), 22, 158, 100, 24);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 22, 190);
                    addChildKnob (layout, panelId, "Res", "filterResonance", 104, 190);
                    addChildSlider (layout, panelId, "A", "attack", 206, 182);
                    addChildSlider (layout, panelId, "D", "decay", 258, 182);
                    addChildSlider (layout, panelId, "S", "sustain", 310, 182);
                    addChildSlider (layout, panelId, "R", "release", 362, 182);

                    addChildSurface (layout, panelId, ElementType::Label, "MOTION + FX", juce::String(), 454, 158, 150, 24);
                    addChildKnob (layout, panelId, "LFO Rate", "lfoRate", 454, 190);
                    addChildKnob (layout, panelId, "LFO Amt", "lfoAmount", 536, 190);
                    addChildKnob (layout, panelId, "Delay", "delayMix", 618, 190);
                    addChildKnob (layout, panelId, "Reverb", "reverbMix", 700, 190);

                    addChildSurface (layout, panelId, ElementType::MacroControl, "Tone", "filterCutoff", 22, 326, 126, 74);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Motion", "lfoAmount", 162, 326, 126, 74);
                    addChildSurface (layout, panelId, ElementType::MacroControl, "Space", "delayMix", 302, 326, 126, 74);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 452, 326, 236, 74);
                    addChildKnob (layout, panelId, "Out", "volume", 704, 326, 58);
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

            addModulePanel ("Add Sampler Instrument Starter", "Sampler Instrument Starter", 760, 430,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 22, 42, 168, 156);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Sample", "sampleStart", 208, 42, 164, 156);
                    addChildSurface (layout, panelId, ElementType::Waveform, "Waveform", "sampleStart", 392, 42, 232, 78);
                    addChildKnob (layout, panelId, "Start", "sampleStart", 392, 136);
                    addChildKnob (layout, panelId, "Length", "sampleLength", 474, 136);
                    addChildKnob (layout, panelId, "Pitch", "samplePitch", 556, 136);
                    addChildToggle (layout, panelId, "Reverse", "sampleReverse", 640, 154, 82, 30);

                    addChildSurface (layout, panelId, ElementType::Label, "MAP + PLAY", juce::String(), 22, 220, 120, 24);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 22, 252, 160, 140);
                    addChildSurface (layout, panelId, ElementType::Keyboard, "Keys", juce::String(), 204, 330, 250, 64);
                    addChildKnob (layout, panelId, "Cutoff", "filterCutoff", 220, 236);
                    addChildKnob (layout, panelId, "Grain", "granularDensity", 302, 236);
                    addChildKnob (layout, panelId, "Delay", "delayMix", 384, 236);
                    addChildKnob (layout, panelId, "Verb", "reverbMix", 466, 236);
                    addChildSurface (layout, panelId, ElementType::GranularField, "Granular", "granularScan", 552, 236, 166, 118);
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

            addModulePanel ("Add Drum Machine Starter", "Drum Machine Starter", 800, 460,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildSurface (layout, panelId, ElementType::RuntimeSampleLibrary, "Sample Library", juce::String(), 22, 38, 160, 130);
                    addChildSurface (layout, panelId, ElementType::SampleDropZone, "Drop Drum Samples", "sampleStart", 202, 38, 164, 130);
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Pads", "padGrid", 386, 38, 220, 220);
                    addChildSurface (layout, panelId, ElementType::DrumGrid, "Pattern Grid", "arpLaneRate", 22, 204, 484, 190);
                    addChildKnob (layout, panelId, "Rate", "arpLaneRate", 536, 286);
                    addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 618, 286);
                    addChildKnob (layout, panelId, "Chance", "arpLaneProbability", 700, 286);
                    addChildSlider (layout, panelId, "Kick", "pad1Volume", 630, 48);
                    addChildSlider (layout, panelId, "Snare", "pad2Volume", 676, 48);
                    addChildSlider (layout, panelId, "Hat", "pad3Volume", 722, 48);
                    addChildKnob (layout, panelId, "Out", "volume", 700, 372, 58);
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
                    addChildSurface (layout, panelId, ElementType::PadGrid, "Trigger Pads", "padGrid", 548, 174, 168, 150);
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
                        addChildSurface (layout, panelId, ElementType::Meter, "Input", "drive", 22, 44, 68, 250);
                        addChildSurface (layout, panelId, ElementType::EqCurve, "Tone EQ", "eqMix", 116, 44, 206, 112);
                        addChildKnob (layout, panelId, "Time", "multiTapTime", 348, 58);
                        addChildKnob (layout, panelId, "Feedback", "multiTapFeedback", 430, 58);
                        addChildKnob (layout, panelId, "Spread", "multiTapSpread", 512, 58);
                        addChildKnob (layout, panelId, "Mix", "multiTapMix", 594, 58);
                        addChildSurface (layout, panelId, ElementType::Waveform, "Feedback Shape", "multiTapFeedback", 116, 188, 250, 90);
                        addChildKnob (layout, panelId, "Duck", "dynMix", 392, 204);
                        addChildKnob (layout, panelId, "Width", "stereoWidth", 474, 204);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 556, 204);
                        addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 704, 44, 68, 250);
                        addChildToggle (layout, panelId, "Limiter", "outputLimiter", 616, 300, 88, 30);
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
                        addChildSurface (layout, panelId, ElementType::Meter, "Input", "drive", 22, 44, 68, 250);
                        addChildSurface (layout, panelId, ElementType::SpectrumAnalyzer, "Analyzer", "eqMix", 116, 44, 220, 108);
                        addChildSurface (layout, panelId, ElementType::EqCurve, "EQ", "eqMix", 116, 188, 220, 108);
                        addChildKnob (layout, panelId, "Low Cut", "eqBand1Freq", 364, 58);
                        addChildKnob (layout, panelId, "Presence", "eqBand2GainDb", 446, 58);
                        addChildKnob (layout, panelId, "Comp", "dynMix", 528, 58);
                        addChildKnob (layout, panelId, "Formant", "vocalFormant", 364, 188);
                        addChildKnob (layout, panelId, "Body", "vocalBody", 446, 188);
                        addChildKnob (layout, panelId, "Width", "stereoWidth", 528, 188);
                        addChildKnob (layout, panelId, "Ceiling", "outputCeilingDb", 610, 188);
                        addChildSurface (layout, panelId, ElementType::Meter, "Output", "outputGainDb", 704, 44, 68, 250);
                        addChildToggle (layout, panelId, "Limiter", "outputLimiter", 610, 300, 88, 30);
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
            addModulePanel ("Add OSC Stack Module", "OSC Stack", 420, 164,
                [&] (LayoutModel& layout, const juce::String& panelId)
                {
                    addChildKnob (layout, panelId, "Wave", "oscType", 22, 38);
                    addChildKnob (layout, panelId, "Wave 2", "osc2Type", 104, 38);
                    addChildKnob (layout, panelId, "Blend", "oscBlend", 186, 38);
                    addChildKnob (layout, panelId, "Sub", "subBlend", 268, 38);
                    addChildValue (layout, panelId, "No Noise", "noiseBlend", 328, 108, 78, 28);
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
                        addChildSurface (layout, panelId, ElementType::PadGrid, "16 Pads", "padGrid", 20, 38, 256, 256);
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
        if (moduleKey == "arplanemodule" || moduleKey == "lfo" || moduleKey == "steplfo" || moduleKey == "macrobank")
        {
            ensureParams ({ "arpLaneIndex", "arpLaneSteps", "arpLaneRate", "arpLaneGate", "arpLaneSwing",
                            "arpLaneProbability", "arpLaneGroup", "arpLaneFxTarget", "arpLaneFxAmount",
                            "lfoRate", "lfoAmount", "filterCutoff" }, "synth");
            if (moduleKey == "arplanemodule")
            {
                ensureBlock ("arp_lane_module", "mod", "arp", "Arp Lane", "arpLaneRate", "midi", "arp", "event",
                             { { "arpLaneSteps", 16.0f }, { "arpLaneRate", 1.0f }, { "arpLaneGate", 0.58f },
                               { "arpLaneProbability", 1.0f }, { "arpLaneFxAmount", 0.0f } });
                addModulePanel ("Add Arp Lane Module", "Arp Lane", 520, 390,
                    [&] (LayoutModel& layout, const juce::String& panelId)
                    {
                        addChildSurface (layout, panelId, ElementType::ArpLane, "Arp Lane", "arpLaneRate", 24, 34, 260, 300);
                        addChildKnob (layout, panelId, "Rate", "arpLaneRate", 318, 42);
                        addChildKnob (layout, panelId, "Gate", "arpLaneGate", 398, 42);
                        addChildKnob (layout, panelId, "Swing", "arpLaneSwing", 318, 122);
                        addChildKnob (layout, panelId, "FX Amt", "arpLaneFxAmount", 398, 122);
                        addChildValue (layout, panelId, "Group", "arpLaneGroup", 320, 220, 78, 30);
                        addChildValue (layout, panelId, "FX", "arpLaneFxTarget", 410, 220, 78, 30);
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

                    addChildKnob ("Cutoff", "filterCutoff", 25);
                    addChildKnob ("Reso", "filterResonance", 115);
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
                    panel.width = 300;
                    panel.height = 180;
                    panel.groupId = tabGroup;
                    panel.cornerRadius = 12.0f;
                    panel.strokeWidth = 1.5f;
                    panel.backgroundColour = juce::Colour (0xee11141e);
                    panel.borderColour = PatchCraftLookAndFeel::accent();
                    panel.accentColour = PatchCraftLookAndFeel::accent();
                    auto& addedPanel = layout.add (panel);
                    const auto panelId = addedPanel.id;

                    auto addChildSlider = [&] (const juce::String& label, const juce::String& paramId, int xOffset)
                    {
                        LayoutElement slider;
                        slider.type = ElementType::Slider;
                        slider.label = label;
                        slider.parameterId = paramId;
                        slider.x = pos.x + xOffset;
                        slider.y = pos.y + 30;
                        slider.width = 40;
                        slider.height = 120;
                        slider.containerId = panelId;
                        slider.groupId = tabGroup;
                        slider.id = layout.generateUniqueId ("slider_");
                        slider.accentColour = PatchCraftLookAndFeel::accent();
                        layout.add (slider);
                    };

                    addChildSlider ("A", "attack", 25);
                    addChildSlider ("D", "decay", 90);
                    addChildSlider ("S", "sustain", 155);
                    addChildSlider ("R", "release", 220);
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

        juce::PopupMenu menu;
        menu.addItem (99, "Search Canvas Actions...");
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
        bool canCreatePscriptHandler = false;
        for (const auto& id : owner.getSelectedElementIds())
            if (auto* selected = owner.getProject().getLayout().find (id))
            {
                if (isRuntimeControlElement (selected->type)
                    && selected->labelPosition != "hidden"
                    && (selected->label.isNotEmpty() || selected->parameterId.isNotEmpty()))
                {
                    canDetachLabels = true;
                }
                if ((isRuntimeControlElement (selected->type)
                     || selected->type == ElementType::ValueDisplay
                     || selected->type == ElementType::SampleDropZone)
                    && selected->parameterId.isNotEmpty()
                    && owner.getProject().getParameters().find (selected->parameterId) != nullptr)
                {
                    canCreatePscriptHandler = true;
                }
            }
        menu.addItem (15, "Detach Labels From Selection", canDetachLabels);
        menu.addItem (36, "Hide Labels For Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (37, "Show Labels For Selection", ! owner.getSelectedElementIds().isEmpty());
        menu.addItem (38, "Create pScript Handler For Selected Control", canCreatePscriptHandler);
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

