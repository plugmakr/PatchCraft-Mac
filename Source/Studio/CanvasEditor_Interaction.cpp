    namespace
    {
        void scaleElementTypographyFromOriginal (LayoutElement& el, const LayoutElement& original,
                                                 float scaleX, float scaleY)
        {
            if (original.labelSize > 0.0f)
                el.labelSize = juce::jmax (6.0f, original.labelSize * scaleY);
            el.labelSpacing = original.labelSpacing * scaleY;
            el.labelOffsetX = original.labelOffsetX * scaleX;
            el.labelOffsetY = original.labelOffsetY * scaleY;
            el.contentPadding = original.contentPadding * juce::jmax (scaleX, scaleY);
            el.cornerRadius = juce::jmax (0.0f, original.cornerRadius * juce::jmax (scaleX, scaleY));
            el.strokeWidth = juce::jmax (0.0f, original.strokeWidth * juce::jmax (scaleX, scaleY));
        }
    }

    bool CanvasEditor::hitTestControlBody (const LayoutElement& el,
                                           juce::Rectangle<int> r,
                                           juce::Point<int> p) const
    {
        const bool hasLocalPreviewValue = el.parameterId.isEmpty()
                                       && (el.type == ElementType::Knob || el.type == ElementType::Slider
                                           || el.type == ElementType::PitchWheel || el.type == ElementType::ModWheel);
        if (el.parameterId.isEmpty() && ! hasLocalPreviewValue) return false;
        if (el.labelPosition == "top")
            r = r.withTrimmedTop (juce::jmax (20, r.getHeight() / 5));
        else if (el.labelPosition == "bottom")
            r = r.withTrimmedBottom (juce::roundToInt ((float) r.getHeight() * 0.30f));
        const int padding = juce::roundToInt (el.contentPadding);
        r = padding >= 0 ? r.reduced (padding) : r.expanded (-padding);
        if (el.type == ElementType::Knob || el.type == ElementType::MacroControl)
        {
            // Use a circle inside r (matching how the knob is drawn).
            auto body = r;
            if (el.type == ElementType::MacroControl)
                body = r.reduced (9, 7).withTrimmedTop (18).withTrimmedRight (juce::jmax (0, r.getWidth() - 92));
            const float cx = body.getCentreX();
            const float cy = body.getCentreY() - body.getHeight() * 0.12f;
            const float rad = juce::jmin (body.getWidth(), (int) (body.getHeight() * 0.7f)) * 0.5f;
            const float dx = p.x - cx, dy = p.y - cy;
            return dx * dx + dy * dy <= rad * rad;
        }
        if (el.type == ElementType::Slider || el.type == ElementType::PitchWheel || el.type == ElementType::ModWheel)
            return r.contains (p);
        return false;
    }

    bool CanvasEditor::drumCellAt (const LayoutElement& element, juce::Rectangle<int> r,
                                   juce::Point<int> p, int& pattern, int& track,
                                   int& step, float& velocity) const
    {
        const auto* block = findDrumMachineBlock (owner.getProject().getDspGraph());
        const int tracks = block != nullptr
            ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) element.drumTracks)))
            : juce::jlimit (1, 16, element.drumTracks);
        const int steps = block != nullptr
            ? juce::jlimit (1, 64, juce::roundToInt (blockValue (*block, "dmSteps", (float) element.drumSteps)))
            : juce::jlimit (1, 64, element.drumSteps);
        pattern = block != nullptr
            ? juce::jlimit (0, 7, juce::roundToInt (blockValue (*block, "dmPattern", (float) element.drumPattern)))
            : juce::jlimit (0, 7, element.drumPattern);

        auto area = r.reduced (8);
        if (area.getHeight() <= 28 || area.getWidth() <= 80)
            return false;
        area.removeFromTop (20);
        area.removeFromTop (4);

        const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
        auto grid = area.withTrimmedLeft (labelW);
        if (! grid.contains (p))
            return false;

        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        if (cellW <= 0.0f || cellH <= 0.0f)
            return false;

        step = juce::jlimit (0, steps - 1, (int) ((float) (p.x - grid.getX()) / cellW));
        track = juce::jlimit (0, tracks - 1, (int) ((float) (p.y - area.getY()) / cellH));
        const float localY = (float) p.y - ((float) area.getY() + (float) track * cellH);
        velocity = juce::jlimit (0.08f, 1.0f, 1.0f - (localY / cellH));
        return true;
    }

    bool CanvasEditor::editDrumGridCellAt (const LayoutElement& element, juce::Rectangle<int> r,
                                           juce::Point<int> p, const juce::ModifierKeys& mods,
                                           bool startGesture)
    {
        if (! (mods.isAltDown() || mods.isShiftDown() || mods.isCtrlDown() || mods.isCommandDown()))
            return false;

        int pattern = 0, track = 0, step = 0;
        float velocity = 0.8f;
        if (! drumCellAt (element, r, p, pattern, track, step, velocity))
            return false;

        if (! startGesture && track == lastDrumGridTrack && step == lastDrumGridStep)
            return true;

        auto& graph = owner.getProject().getDspGraph();
        auto& block = ensureDrumMachineBlock (graph);
        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        const bool wasOn = blockValue (block, prefix + "On", 0.0f) >= 0.5f;

        if (auto* mutableElement = owner.getProject().getLayout().find (element.id))
        {
            mutableElement->drumPattern = pattern;
            if (block.values.find ("dmTracks") == block.values.end())
                block.values["dmTracks"] = (float) juce::jlimit (1, 16, mutableElement->drumTracks);
            if (block.values.find ("dmSteps") == block.values.end())
                block.values["dmSteps"] = (float) juce::jlimit (1, 64, mutableElement->drumSteps);
        }
        block.values["dmPattern"] = (float) pattern;
        block.values["dmTransport"] = 1.0f;

        if (mods.isCtrlDown() || mods.isCommandDown())
        {
            const int current = juce::jlimit (1, 4, juce::roundToInt (blockValue (block, prefix + "Div", 1.0f)));
            block.values[prefix + "On"] = 1.0f;
            block.values[prefix + "Vel"] = velocity;
            block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.42f);
            block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
            block.values[prefix + "Div"] = (float) (current >= 4 ? 1 : current + 1);
            drumGridPaintState = true;
        }
        else if (mods.isShiftDown() && ! mods.isAltDown())
        {
            block.values[prefix + "On"] = 1.0f;
            block.values[prefix + "Vel"] = velocity;
            block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.55f);
            block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
            block.values[prefix + "Div"] = blockValue (block, prefix + "Div", 1.0f);
            drumGridPaintState = true;
        }
        else
        {
            if (startGesture)
                drumGridPaintState = ! wasOn;
            block.values[prefix + "On"] = drumGridPaintState ? 1.0f : 0.0f;
            if (drumGridPaintState)
            {
                block.values[prefix + "Vel"] = velocity;
                block.values[prefix + "Gate"] = blockValue (block, prefix + "Gate", 0.55f);
                block.values[prefix + "Prob"] = blockValue (block, prefix + "Prob", 1.0f);
                block.values[prefix + "Div"] = blockValue (block, prefix + "Div", 1.0f);
            }
        }

        graph.userConfigured = true;
        owner.getProject().markDirty();
        layoutChangedDuringDrag = true;
        lastDrumGridTrack = track;
        lastDrumGridStep = step;
        repaint (r.expanded (8));
        return true;
    }

    bool CanvasEditor::arpLaneStepAt (const LayoutElement& element, juce::Rectangle<int> r,
                                      juce::Point<int> p, int& lane, int& step,
                                      float& velocity) const
    {
        const auto* block = findArpBlock (owner.getProject().getDspGraph());
        const auto layout = ArpLaneUi::layout (r, element, block, false);
        if (! ArpLaneUi::hitTestStep (layout, element, p, lane, step))
            return false;

        velocity = ArpLaneUi::storedStepVelocity (block, lane, step);
        return true;
    }

    bool CanvasEditor::editArpLaneStepAt (const LayoutElement& element, juce::Rectangle<int> r,
                                          juce::Point<int> p, bool startGesture)
    {
        const auto* block = findArpBlock (owner.getProject().getDspGraph());
        const auto layout = ArpLaneUi::layout (r, element, block, false);

        if (! startGesture && layout.stepsMinusBtn.contains (p))
        {
            const int lane = juce::jlimit (0, 15, element.arpLaneIndex);
            auto& graph = owner.getProject().getDspGraph();
            auto& blockMut = ensureArpBlock (graph);
            setArpLaneValue (blockMut, lane, "arpSteps", (float) juce::jmax (1, layout.steps - 1));
            owner.getProject().markDirty();
            owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
            repaint (r.expanded (8));
            return true;
        }

        if (! startGesture && layout.stepsPlusBtn.contains (p))
        {
            const int lane = juce::jlimit (0, 15, element.arpLaneIndex);
            auto& graph = owner.getProject().getDspGraph();
            auto& blockMut = ensureArpBlock (graph);
            setArpLaneValue (blockMut, lane, "arpSteps", (float) juce::jmin (128, layout.steps + 1));
            owner.getProject().markDirty();
            owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
            repaint (r.expanded (8));
            return true;
        }

        int lane = -1;
        int step = -1;
        if (! startGesture && lastArpLane >= 0 && lastArpStep >= 0)
        {
            lane = lastArpLane;
            step = lastArpStep;
        }
        else if (! ArpLaneUi::hitTestStep (layout, element, p, lane, step))
        {
            return false;
        }

        auto& graph = owner.getProject().getDspGraph();
        auto& blockMut = ensureArpBlock (graph);
        const auto suffix = juce::String (step);
        const bool wasActive = ArpLaneUi::storedStepActive (&blockMut, lane, step, false);

        if (startGesture)
        {
            setArpLaneValue (blockMut, lane, "mpStep" + suffix + "On", wasActive ? 0.0f : 1.0f);
            setArpLaneValue (blockMut, lane, "mpVelocity" + suffix,
                             ArpLaneUi::storedStepVelocity (&blockMut, lane, step));
        }
        else
        {
            const float velocity = ArpLaneUi::velocityFromVerticalDrag (layout.content, p.y);
            setArpLaneValue (blockMut, lane, "mpStep" + suffix + "On", 1.0f);
            setArpLaneValue (blockMut, lane, "mpVelocity" + suffix, juce::jlimit (0.05f, 1.0f, velocity));
        }

        setArpLaneValue (blockMut, lane, "mpGate" + suffix, arpLaneValue (blockMut, lane, "mpGate" + suffix, 0.72f));
        setArpLaneValue (blockMut, lane, "mpStepProb" + suffix, arpLaneValue (blockMut, lane, "mpStepProb" + suffix, 1.0f));
        setArpLaneValue (blockMut, lane, "mpStepDiv" + suffix, arpLaneValue (blockMut, lane, "mpStepDiv" + suffix, 1.0f));
        if (step < 16)
            owner.getProject().getLiveValues().setValue ("arpLaneStep" + juce::String (step + 1),
                juce::jlimit (0.05f, 1.0f, arpLaneValue (blockMut, lane, "mpVelocity" + suffix, 0.72f)));

        graph.userConfigured = true;
        owner.getProject().markDirty();
        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        layoutChangedDuringDrag = true;
        lastArpLane = lane;
        lastArpStep = step;
        repaint (r.expanded (8));
        return true;
    }

    void CanvasEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus();
        layoutChangedDuringDrag = false;
        dragLayoutBefore.clear();
        dragActionName.clear();
        if (e.mods.isPopupMenu())
        {
            showContextMenu (e.getPosition(), e.mods.isCtrlDown() || e.mods.isCommandDown());
            return;
        }

        if (juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
        {
            dragStart = e.getPosition();
            panDragOrigin = canvasPanOffset;
            mode = DragMode::Pan;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
        const auto& elements = owner.getProject().getLayout().getAll();

        // 0. TabPanel tabs: normal click swaps the current page. Holding Shift
        // bypasses navigation so the tab strip itself can be selected/moved.
        if (! e.mods.isShiftDown())
        {
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type != ElementType::TabPanel) continue;
                if (! isElementOnCurrentTab (*it)) continue;
                auto r = elementScreenRect (*it);
                const int tabIdx = hitTabIndex (*it, r, e.getPosition());
                if (tabIdx >= 0 && tabIdx < it->tabs.size())
                {
                    const auto targetGroup = scopedTabGroupId (*it, it->tabs[tabIdx]);
                    if (it->id == "tabs")
                        setCurrentTabGroup (targetGroup);
                    else
                    {
                        activeTabGroupsByPanel[it->id] = targetGroup;
                        repaint();
                    }
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);
                    return;
                }
            }

            ensureManualContainerDefaults();
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type != ElementType::Button && it->type != ElementType::Toggle
                    && it->type != ElementType::Label && it->type != ElementType::Image)
                    continue;
                if (! isElementOnCurrentTab (*it)) continue;
                if (! elementScreenRect (*it).contains (e.getPosition())) continue;
                if (triggerManualContainer (*it))
                {
                    owner.setSelectedElementId (it->id);
                    return;
                }
            }
        }

        // 1. Resize handles on the selection group first, then on a single selected element.
        juce::Rectangle<int> multiBounds;
        if (getMultiSelectionScreenBounds (multiBounds))
        {
            activeResizeHandle = resizeHandleAt (e.getPosition(), multiBounds);
            if (activeResizeHandle != ResizeHandle::None)
            {
                dragStart = e.getPosition();
                dragLayoutBefore = owner.getProject().getLayout().getAll();
                dragActionName = "Scale selection";
                mode = DragMode::Resize;
                return;
            }
        }

        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            activeResizeHandle = resizeHandleAt (e.getPosition(), r);
            if (activeResizeHandle != ResizeHandle::None)
            {
                dragStart    = e.getPosition();
                dragOriginal = *sel;
                dragLayoutBefore = owner.getProject().getLayout().getAll();
                dragActionName = owner.getSelectedElementIds().size() > 1 ? "Scale selection" : "Resize element";
                mode = DragMode::Resize;
                return;
            }

            if (sel->type == ElementType::Group && r.contains (e.getPosition()))
            {
                dragStart    = e.getPosition();
                dragOriginal = *sel;
                dragLayoutBefore = owner.getProject().getLayout().getAll();
                dragActionName = "Move group";
                captureMoveOriginsForSelection();
                mode = DragMode::Move;
                return;
            }
        }

        // 2/3. Hit test in reverse z-order (front to back).
        for (auto it = elements.rbegin(); it != elements.rend(); ++it)
        {
            if (! it->visible) continue;
            if (it->type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementScreenRect (*it);
            if (! r.contains (e.getPosition())) continue;

            const auto canvas = owner.getProject().getCanvasSize();
            const bool passiveLockedBackground =
                it->locked
                && (it->id == "background"
                    || (it->type == ElementType::Image
                        && it->x <= 0
                        && it->y <= 0
                        && it->width >= canvas.width - 4
                        && it->height >= canvas.height - 4));
            if (passiveLockedBackground && ! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
                continue;

            // Locked elements (e.g. background artwork): selectable so the
            // inspector's Asset/Browse field is accessible, but no move/resize.
            if (it->locked)
            {
                if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                    owner.toggleSelectedElementId (it->id);
                else
                    owner.setSelectedElementId (it->id);
                mode = DragMode::None;
                return;
            }

            // Design is also an audition surface. Normal clicks play pads and
            // keys through the same Pack runtime used by Brand/Test/exports;
            // Shift keeps the existing move/resize authoring gesture.
            if (! e.mods.isShiftDown()
                && (it->type == ElementType::Keyboard
                    || it->type == ElementType::DrumPad
                    || it->type == ElementType::PadGrid))
            {
                int note = -1;
                if (it->type == ElementType::Keyboard)
                {
                    note = canvasKeyboardNoteAt (r, e.getPosition());
                }
                else
                {
                    const int rows = it->type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, it->padRows);
                    const int cols = it->type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, it->padCols);
                    const int gap = it->type == ElementType::DrumPad ? 0 : kPadGridCellGapPx;
                    const auto inner = r.reduced (it->type == ElementType::PadGrid ? 4 : 0);
                    const bool squarePads = it->type == ElementType::PadGrid;
                    const auto metrics = computePadGridMetrics (inner.toFloat(), rows, cols, gap, squarePads);
                    for (int row = 0; row < rows && note < 0; ++row)
                        for (int col = 0; col < cols && note < 0; ++col)
                            if (padCellRect (metrics, row, col, gap, squarePads,
                                             (float) inner.getX(), (float) inner.getY()).contains (e.getPosition().toFloat()))
                            {
                                const int padIndex = row * cols + col;
                                if (const auto* zone = canvasSampleZoneForPad (owner.getProject().getSampleMap(), padIndex))
                                    note = juce::jlimit (0, 127, zone->rootNote);
                                else
                                    note = juce::jlimit (0, 127, it->padBaseNote + padIndex);
                            }
                }

                if (note >= 0)
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

                    if (auditionNote >= 0)
                        if (auto* runtime = owner.getPackRuntime())
                            runtime->previewNoteOff (auditionNote);

                    auditionNote = note;
                    auditionElementId = it->id;
                    mode = DragMode::AuditionNote;
                    if (auto* runtime = owner.getPackRuntime())
                        runtime->previewNoteOn (note, 0.9f);
                    repaint (r.expanded (4));
                    return;
                }
            }

            if (it->type == ElementType::DrumGrid
                && owner.isElementSelected (it->id)
                && (e.mods.isAltDown() || e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown())
                && editDrumGridCellAt (*it, r, e.getPosition(), e.mods, true))
            {
                drumGridEditElementId = it->id;
                dragStart = e.getPosition();
                mode = DragMode::DrumGridEdit;
                owner.refreshAllPanels();
                return;
            }

            if (it->type == ElementType::ArpLane
                && owner.isElementSelected (it->id)
                && editArpLaneStepAt (*it, r, e.getPosition(), true))
            {
                arpLaneEditElementId = it->id;
                dragStart = e.getPosition();
                mode = DragMode::ArpLaneEdit;
                owner.refreshAllPanels();
                return;
            }

            const bool isInteractiveControl = it->type == ElementType::Knob || it->type == ElementType::Slider
                                           || it->type == ElementType::Toggle || it->type == ElementType::Dropdown
                                           || it->type == ElementType::ValueDisplay
                                           || it->type == ElementType::MacroControl;
            const bool isVisualOnlyFilmstripControl = it->parameterId.isEmpty()
                                                   && it->filmstripAsset.isNotEmpty()
                                                   && (it->type == ElementType::Knob || it->type == ElementType::Slider);
            if (! e.mods.isShiftDown() && owner.getProject().getManifest().playerShowParameterGuidance && isInteractiveControl)
            {
                const bool attemptedControlUse = (it->type == ElementType::Knob || it->type == ElementType::Slider
                                               || it->type == ElementType::MacroControl)
                    ? hitTestControlBody (*it, r, e.getPosition())
                    : r.contains (e.getPosition());
                if (attemptedControlUse && ! isVisualOnlyFilmstripControl)
                {
                    const auto guidance = canvasControlGuidance (owner.getProject(), *it);
                    if (guidance.isNotEmpty())
                    {
                        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                            owner.toggleSelectedElementId (it->id);
                        else
                            owner.setSelectedElementId (it->id);
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                            "Control is not connected", guidance);
                        mode = DragMode::None;
                        return;
                    }
                }
            }

            if (! e.mods.isShiftDown()
                && e.getNumberOfClicks() > 1
                && isInteractiveControl
                && it->type != ElementType::Dropdown
                && it->parameterId.isNotEmpty()
                && hitTestControlBody (*it, r, e.getPosition()))
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

                    owner.getProject().getLiveValues().setValue (it->parameterId, def->defaultValue);
                    applyArpLaneParameterToGraph (owner.getProject(), it->parameterId);
                    if (it->parameterId.startsWith ("arpLane"))
                        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                    else
                        owner.getProject().markDirty();
                    repaint();
                    return;
                }
            }

            if (! e.mods.isShiftDown() && it->type == ElementType::Toggle && it->parameterId.isNotEmpty())
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

                    const auto current = owner.getProject().getLiveValues().getValue (it->parameterId, def->defaultValue);
                    owner.getProject().getLiveValues().setValue (it->parameterId, current >= 0.5f ? def->min : def->max);
                    applyArpLaneParameterToGraph (owner.getProject(), it->parameterId);
                    if (it->parameterId.startsWith ("arpLane"))
                        owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                    repaint();
                    return;
                }
            }

            if (! e.mods.isShiftDown()
                && it->type == ElementType::Dropdown
                && it->parameterId.isNotEmpty()
                && (e.getNumberOfClicks() > 1 || e.mods.isAltDown()))
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);

                    juce::PopupMenu menu;
                    std::vector<float> values;
                    if (def->id == "arpLaneMode")
                    {
                        values = { 0.0f, 1.0f };
                        menu.addItem (1, "Bank");
                        menu.addItem (2, "Performance");
                    }
                    else if (def->id == "arpLaneTarget")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                        menu.addItem (1, "Notes");
                        menu.addItem (2, "Drums");
                        menu.addItem (3, "One Shots");
                        menu.addItem (4, "Loops");
                        menu.addItem (5, "Samples");
                    }
                    else if (def->id == "arpLaneDirection")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f };
                        menu.addItem (1, "Forward");
                        menu.addItem (2, "Reverse");
                        menu.addItem (3, "Bounce");
                        menu.addItem (4, "Random");
                    }
                    else if (def->id == "arpLaneControlBank")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                        for (int bank = 0; bank < 5; ++bank)
                            menu.addItem (bank + 1, "Lane " + juce::String (bank + 1));
                    }
                    else if (def->id == "arpLaneGroup")
                    {
                        for (int groupIndex = 0; groupIndex < 8; ++groupIndex)
                        {
                            values.push_back ((float) groupIndex);
                            menu.addItem (groupIndex + 1, "Group " + juce::String (groupIndex + 1));
                        }
                    }
                    else if (def->id == "arpLaneSliderRole")
                    {
                        values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
                        menu.addItem (1, "Velocity");
                        menu.addItem (2, "Gate");
                        menu.addItem (3, "Probability");
                        menu.addItem (4, "Ratchet");
                        menu.addItem (5, "Mute / Active");
                        menu.addItem (6, "Delay");
                        menu.addItem (7, "Sample Slice");
                        menu.addItem (8, "Transpose");
                        menu.addItem (9, "Filter");
                        menu.addItem (10, "Pan");
                        menu.addItem (11, "FX Send");
                    }
                    else if (def->id == "arpLaneSound")
                    {
                        for (int sound = 0; sound < 16; ++sound)
                        {
                            values.push_back ((float) sound);
                            menu.addItem (sound + 1, orbitLaneSoundName (sound));
                        }
                    }
                    else if (def->id == "arpLaneFxTarget")
                    {
                        static const char* fxTargets[] = { "Delay", "Reverb", "Chorus", "Phaser", "Drive", "Resonance", "Width", "Tape" };
                        for (int target = 0; target < 8; ++target)
                        {
                            values.push_back ((float) target);
                            menu.addItem (target + 1, fxTargets[target]);
                        }
                    }
                    else if (def->id == "arpLanePatternLaunch")
                    {
                        for (int preset = 0; preset < 8; ++preset)
                        {
                            values.push_back ((float) preset);
                            menu.addItem (preset + 1, circleSeqPatternName (preset));
                        }
                    }
                    else if (def->step >= 1.0f && def->max - def->min <= 32.0f)
                    {
                        for (int value = (int) def->min; value <= (int) def->max; value += (int) juce::jmax (1.0f, def->step))
                        {
                            values.push_back ((float) value);
                            menu.addItem ((int) values.size(), juce::String (value) + (def->unit.isNotEmpty() ? " " + def->unit : ""));
                        }
                    }
                    else
                    {
                        values = { def->min, def->defaultValue, def->max };
                        menu.addItem (1, "Min  " + juce::String (def->min, 2));
                        menu.addItem (2, "Default  " + juce::String (def->defaultValue, 2));
                        menu.addItem (3, "Max  " + juce::String (def->max, 2));
                    }

                    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [this, parameterId = it->parameterId, values] (int result)
                        {
                            if (result <= 0 || result > (int) values.size()) return;
                            owner.getProject().getLiveValues().setValue (parameterId, values[(size_t) result - 1]);
                            applyArpLaneParameterToGraph (owner.getProject(), parameterId);
                            if (parameterId.startsWith ("arpLane"))
                                owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
                            repaint();
                        });
                    return;
                }
            }

            // Value drag if the click is inside the control body.
            if (! e.mods.isShiftDown() && hitTestControlBody (*it, r, e.getPosition()))
            {
                if (auto* def = owner.getProject().getParameters().find (it->parameterId);
                    def != nullptr && canvasParameterIsEnabled (owner.getProject(), *def))
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);
                    dragStart        = e.getPosition();
                    dragParameterId  = it->parameterId;
                    dragValueElementId = it->id;
                    dragValueStart   = owner.getProject().getLiveValues()
                                            .getValue (it->parameterId, def->defaultValue);
                    dragValueIsLocalPreview = false;
                    mode = DragMode::ValueDrag;
                    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
                    return;
                }

                if (isVisualOnlyFilmstripControl)
                {
                    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                        owner.toggleSelectedElementId (it->id);
                    else
                        owner.setSelectedElementId (it->id);
                    dragStart = e.getPosition();
                    dragParameterId.clear();
                    dragValueElementId = it->id;
                    dragValueStart = juce::jlimit (0.0f, 1.0f, it->controlPreviewValue);
                    dragValueIsLocalPreview = true;
                    mode = DragMode::ValueDrag;
                    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
                    return;
                }
            }

            // Move drag.
            if (e.mods.isCommandDown() || e.mods.isCtrlDown())
                owner.toggleSelectedElementId (it->id);
            else if (! owner.isElementSelected (it->id))
                owner.setSelectedElementId (it->id);
            dragStart    = e.getPosition();
            dragOriginal = *it;
            dragLayoutBefore = owner.getProject().getLayout().getAll();
            dragActionName = "Move selection";
            captureMoveOriginsForSelection();
            mode = DragMode::Move;
            return;
        }

        if (! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
            owner.clearSelection();
        dragStart = e.getPosition();
        marqueeRect = { e.x, e.y, 0, 0 };
        mode = DragMode::Marquee;
    }

    void CanvasEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (mode == DragMode::None) return;

        if (mode == DragMode::AuditionNote)
        {
            const auto* element = owner.getProject().getLayout().find (auditionElementId);
            if (element == nullptr)
                return;

            const auto bounds = elementScreenRect (*element);
            if (! bounds.contains (e.getPosition()))
                return;

            int nextNote = -1;
            if (element->type == ElementType::Keyboard)
            {
                nextNote = canvasKeyboardNoteAt (bounds, e.getPosition());
            }
            else
            {
                const int rows = element->type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, element->padRows);
                const int cols = element->type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, element->padCols);
                const int gap = element->type == ElementType::DrumPad ? 0 : kPadGridCellGapPx;
                const auto inner = bounds.reduced (element->type == ElementType::PadGrid ? 4 : 0);
                const bool squarePads = element->type == ElementType::PadGrid;
                const auto metrics = computePadGridMetrics (inner.toFloat(), rows, cols, gap, squarePads);
                for (int row = 0; row < rows && nextNote < 0; ++row)
                    for (int col = 0; col < cols && nextNote < 0; ++col)
                        if (padCellRect (metrics, row, col, gap, squarePads,
                                         (float) inner.getX(), (float) inner.getY()).contains (e.getPosition().toFloat()))
                            nextNote = juce::jlimit (0, 127, element->padBaseNote + row * cols + col);
            }

            if (nextNote >= 0 && nextNote != auditionNote)
            {
                if (auto* runtime = owner.getPackRuntime())
                {
                    runtime->previewNoteOff (auditionNote);
                    runtime->previewNoteOn (nextNote, 0.9f);
                }
                auditionNote = nextNote;
                repaint (bounds.expanded (4));
            }
            return;
        }

        const bool disableSnap = e.mods.isCtrlDown() || e.mods.isCommandDown();
        auto snap = [this, disableSnap] (int v) -> int
        {
            if (disableSnap || ! snapEnabled || snapGrid <= 1) return v;
            return juce::roundToInt ((float) v / snapGrid) * snapGrid;
        };

        if (mode == DragMode::ValueDrag)
        {
            if (dragValueIsLocalPreview)
            {
                const int dyPixels = dragStart.y - e.getPosition().y;
                const float fineMult = e.mods.isShiftDown() ? 0.2f : 1.0f;
                const float deltaNorm = (float) dyPixels / 220.0f * fineMult;
                const float nextValue = juce::jlimit (0.0f, 1.0f, dragValueStart + deltaNorm);
                if (auto* dragged = owner.getProject().getLayout().find (dragValueElementId))
                {
                    dragged->controlPreviewValue = nextValue;
                    repaint (elementScreenRect (*dragged).expanded (8));
                }
                else
                {
                    repaint();
                }
                layoutChangedDuringDrag = true;
                return;
            }

            auto* def = owner.getProject().getParameters().find (dragParameterId);
            if (def == nullptr) return;
            const int dyPixels = dragStart.y - e.getPosition().y;     // up = increase
            const float fineMult = e.mods.isShiftDown() ? 0.2f : 1.0f;
            const float pixelsPerFullRange = 220.0f;
            const float deltaNorm = (float) dyPixels / pixelsPerFullRange * fineMult;
            const float range = def->max - def->min;
            float v = dragValueStart + deltaNorm * range;
            v = juce::jlimit (def->min, def->max, v);
            owner.getProject().getLiveValues().setValue (dragParameterId, v);
            applyArpLaneParameterToGraph (owner.getProject(), dragParameterId);
            if (dragParameterId.startsWith ("arpLane"))
                owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
            if (auto* dragged = owner.getProject().getLayout().find (dragValueElementId))
                repaint (elementScreenRect (*dragged).expanded (8));
            else
                repaint();
            return;
        }

        if (mode == DragMode::DrumGridEdit)
        {
            if (auto* grid = owner.getProject().getLayout().find (drumGridEditElementId);
                grid != nullptr && grid->type == ElementType::DrumGrid)
                editDrumGridCellAt (*grid, elementScreenRect (*grid), e.getPosition(), e.mods, false);
            return;
        }

        if (mode == DragMode::ArpLaneEdit)
        {
            if (auto* arp = owner.getProject().getLayout().find (arpLaneEditElementId);
                arp != nullptr && arp->type == ElementType::ArpLane)
                editArpLaneStepAt (*arp, elementScreenRect (*arp), e.getPosition(), false);
            return;
        }

        if (mode == DragMode::Pan)
        {
            canvasPanOffset = panDragOrigin + (e.getPosition() - dragStart);
            repaint();
            return;
        }

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());
        juce::Rectangle<int> dirtyBefore;
        bool hasDirtyBefore = false;
        if (mode == DragMode::Move || mode == DragMode::Resize)
        {
            hasDirtyBefore = getMultiSelectionScreenBounds (dirtyBefore);
            if (! hasDirtyBefore && el != nullptr)
            {
                dirtyBefore = elementScreenRect (*el);
                hasDirtyBefore = true;
            }
        }

        const auto deltaCanvas = juce::Point<float> (
            (e.getPosition().x - dragStart.x) / zoom,
            (e.getPosition().y - dragStart.y) / zoom);

        if (mode == DragMode::Move)
        {
            for (auto& kv : multiDragOrigins)
                if (auto* selected = owner.getProject().getLayout().find (kv.first); selected != nullptr && ! selected->locked)
                {
                    selected->x = snap (kv.second.x + (int) deltaCanvas.x);
                    selected->y = snap (kv.second.y + (int) deltaCanvas.y);
                    owner.propagateLinkedElementChange (selected->id);
                }
            layoutChangedDuringDrag = true;
            owner.getProject().markDirty();
        }
        else if (mode == DragMode::Resize)
        {
            if (el == nullptr) return;
            const auto selectedIds = owner.getSelectedElementIds();
            juce::StringArray scalableIds;
            auto addWithChildren = [&] (auto& self, const juce::String& id) -> void
            {
                if (id.isEmpty() || scalableIds.contains (id))
                    return;

                const auto original = std::find_if (dragLayoutBefore.begin(), dragLayoutBefore.end(),
                    [&] (const LayoutElement& candidate) { return candidate.id == id; });
                if (original == dragLayoutBefore.end() || original->locked || original->type == ElementType::Group)
                    return;

                scalableIds.add (id);
                for (const auto& child : dragLayoutBefore)
                    if (child.containerId == id)
                        self (self, child.id);
            };

            for (const auto& id : selectedIds)
                addWithChildren (addWithChildren, id);

            if (scalableIds.size() > 1 && ! dragLayoutBefore.empty())
            {
                juce::Rectangle<int> originalBounds;
                bool hasBounds = false;
                for (const auto& original : dragLayoutBefore)
                {
                    if (! scalableIds.contains (original.id) || original.locked || original.type == ElementType::Group)
                        continue;
                    const juce::Rectangle<int> itemBounds (original.x, original.y,
                                                           juce::jmax (1, original.width),
                                                           juce::jmax (1, original.height));
                    originalBounds = hasBounds ? originalBounds.getUnion (itemBounds) : itemBounds;
                    hasBounds = true;
                }

                if (hasBounds && originalBounds.getWidth() > 0 && originalBounds.getHeight() > 0)
                {
                    int newLeft = originalBounds.getX();
                    int newTop = originalBounds.getY();
                    int newRight = originalBounds.getRight();
                    int newBottom = originalBounds.getBottom();
                    const int dx = (int) deltaCanvas.x;
                    const int dy = (int) deltaCanvas.y;
                    switch (activeResizeHandle)
                    {
                        case ResizeHandle::TopLeft: newLeft += dx; newTop += dy; break;
                        case ResizeHandle::Top: newTop += dy; break;
                        case ResizeHandle::TopRight: newRight += dx; newTop += dy; break;
                        case ResizeHandle::Right: newRight += dx; break;
                        case ResizeHandle::BottomRight: newRight += dx; newBottom += dy; break;
                        case ResizeHandle::Bottom: newBottom += dy; break;
                        case ResizeHandle::BottomLeft: newLeft += dx; newBottom += dy; break;
                        case ResizeHandle::Left: newLeft += dx; break;
                        default: break;
                    }
                    if (newRight - newLeft < 8) newRight = newLeft + 8;
                    if (newBottom - newTop < 8) newBottom = newTop + 8;
                    const float scaleX = juce::jmax (0.05f, (float) (newRight - newLeft) / (float) originalBounds.getWidth());
                    const float scaleY = juce::jmax (0.05f, (float) (newBottom - newTop) / (float) originalBounds.getHeight());

                    for (const auto& original : dragLayoutBefore)
                    {
                        if (! scalableIds.contains (original.id) || original.locked || original.type == ElementType::Group)
                            continue;
                        if (auto* selected = owner.getProject().getLayout().find (original.id))
                        {
                            selected->x = snap (newLeft
                                + juce::roundToInt ((float) (original.x - originalBounds.getX()) * scaleX));
                            selected->y = snap (newTop
                                + juce::roundToInt ((float) (original.y - originalBounds.getY()) * scaleY));
                            selected->width = juce::jmax (8, snap (juce::roundToInt ((float) original.width * scaleX)));
                            selected->height = juce::jmax (8, snap (juce::roundToInt ((float) original.height * scaleY)));
                            scaleElementTypographyFromOriginal (*selected, original, scaleX, scaleY);
                            owner.propagateLinkedElementChange (selected->id);
                        }
                    }
                }
            }
            else
            {
                int newX = dragOriginal.x;
                int newY = dragOriginal.y;
                int newW = dragOriginal.width;
                int newH = dragOriginal.height;
                const int dx = (int) deltaCanvas.x;
                const int dy = (int) deltaCanvas.y;
                switch (activeResizeHandle)
                {
                    case ResizeHandle::TopLeft: newX += dx; newY += dy; newW -= dx; newH -= dy; break;
                    case ResizeHandle::Top: newY += dy; newH -= dy; break;
                    case ResizeHandle::TopRight: newY += dy; newW += dx; newH -= dy; break;
                    case ResizeHandle::Right: newW += dx; break;
                    case ResizeHandle::BottomRight: newW += dx; newH += dy; break;
                    case ResizeHandle::Bottom: newH += dy; break;
                    case ResizeHandle::BottomLeft: newX += dx; newW -= dx; newH += dy; break;
                    case ResizeHandle::Left: newX += dx; newW -= dx; break;
                    default: break;
                }
                if (newW < 8) { newX -= (8 - newW); newW = 8; }
                if (newH < 8) { newY -= (8 - newH); newH = 8; }
                el->x = snap (newX);
                el->y = snap (newY);
                el->width  = juce::jmax (8, snap (newW));
                el->height = juce::jmax (8, snap (newH));
                const float scaleX = (float) el->width / (float) juce::jmax (1, dragOriginal.width);
                const float scaleY = (float) el->height / (float) juce::jmax (1, dragOriginal.height);
                scaleElementTypographyFromOriginal (*el, dragOriginal, scaleX, scaleY);
                owner.propagateLinkedElementChange (el->id);
            }
            layoutChangedDuringDrag = true;
            owner.getProject().markDirty();
        }
        else if (mode == DragMode::Marquee)
        {
            marqueeRect = juce::Rectangle<int>::leftTopRightBottom (
                juce::jmin (dragStart.x, e.getPosition().x),
                juce::jmin (dragStart.y, e.getPosition().y),
                juce::jmax (dragStart.x, e.getPosition().x),
                juce::jmax (dragStart.y, e.getPosition().y));
            juce::StringArray ids;
            for (const auto& item : owner.getProject().getLayout().getAll())
                if (item.visible && ! item.locked && item.type != ElementType::Group && isElementOnCurrentTab (item)
                    && marqueeRect.intersects (elementScreenRect (item)))
                    ids.add (item.id);
            owner.setSelectedElementIds (ids);
            repaint();
            return;
        }

        if (hasDirtyBefore)
        {
            juce::Rectangle<int> dirtyAfter;
            bool hasDirtyAfter = getMultiSelectionScreenBounds (dirtyAfter);
            if (! hasDirtyAfter)
            {
                if (auto* selected = owner.getProject().getLayout().find (owner.getSelectedElementId()))
                {
                    dirtyAfter = elementScreenRect (*selected);
                    hasDirtyAfter = true;
                }
            }

            repaint (hasDirtyAfter ? dirtyBefore.getUnion (dirtyAfter).expanded (28)
                                   : dirtyBefore.expanded (28));
            return;
        }

        repaint();
    }

    void CanvasEditor::mouseUp (const juce::MouseEvent&)
    {
        if (auditionNote >= 0)
            if (auto* runtime = owner.getPackRuntime())
                runtime->previewNoteOff (auditionNote);
        auditionNote = -1;
        auditionElementId.clear();

        const bool wasMarquee = mode == DragMode::Marquee;
        const auto previousMarquee = marqueeRect;
        const bool shouldNotify = layoutChangedDuringDrag;
        const bool shouldCommitLayoutUndo = layoutChangedDuringDrag
                                         && ! dragLayoutBefore.empty()
                                         && (mode == DragMode::Move || mode == DragMode::Resize);
        const auto afterLayout = shouldCommitLayoutUndo
            ? owner.getProject().getLayout().getAll()
            : std::vector<LayoutElement>();
        const auto beforeLayout = dragLayoutBefore;
        const auto actionName = dragActionName.isNotEmpty() ? dragActionName : juce::String ("Edit layout");
        mode = DragMode::None;
        dragParameterId.clear();
        dragValueElementId.clear();
        dragValueIsLocalPreview = false;
        drumGridEditElementId.clear();
        arpLaneEditElementId.clear();
        panDragOrigin = {};
        lastDrumGridTrack = -1;
        lastDrumGridStep = -1;
        lastArpLane = -1;
        lastArpStep = -1;
        marqueeRect = {};
        multiDragOrigins.clear();
        dragLayoutBefore.clear();
        dragActionName.clear();
        activeResizeHandle = ResizeHandle::None;
        layoutChangedDuringDrag = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (shouldCommitLayoutUndo)
        {
            owner.getProject().getLayout().getAll() = beforeLayout;
            owner.getProject().performLayoutEdit (actionName,
                [afterLayout] (LayoutModel& m)
                {
                    m.getAll() = afterLayout;
                }, false); // structuralChange = false
        }
        else if (shouldNotify)
            owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::layout);

        if (wasMarquee)
            repaint (previousMarquee.expanded (3));
    }

    void CanvasEditor::mouseMove (const juce::MouseEvent& e)
    {
        const auto previousHover = hoverGuidanceBounds.expanded (4);
        const auto previousText = hoverGuidance;
        hoverGuidance.clear();
        hoverGuidanceBounds = {};

        if (owner.getProject().getManifest().playerShowParameterGuidance)
        {
            const auto& elements = owner.getProject().getLayout().getAll();
            for (auto it = elements.rbegin(); it != elements.rend(); ++it)
            {
                if (! it->visible) continue;
                if (it->type == ElementType::Group) continue;
                if (! isElementOnCurrentTab (*it)) continue;

                const bool isInteractiveControl = it->type == ElementType::Knob || it->type == ElementType::Slider
                                               || it->type == ElementType::Button || it->type == ElementType::Toggle
                                               || it->type == ElementType::Dropdown
                                               || it->type == ElementType::ValueDisplay
                                               || it->type == ElementType::MacroControl;
                if (! isInteractiveControl)
                    continue;

                auto r = elementScreenRect (*it);
                if (! r.contains (e.getPosition()))
                    continue;

                const bool overControl = (it->type == ElementType::Knob || it->type == ElementType::Slider
                                       || it->type == ElementType::MacroControl)
                    ? hitTestControlBody (*it, r, e.getPosition())
                    : true;
                if (! overControl)
                    break;

                hoverGuidance = canvasControlGuidance (owner.getProject(), *it);
                if (hoverGuidance.isNotEmpty())
                {
                    constexpr int tipW = 330;
                    constexpr int tipH = 76;
                    int x = e.x + 16;
                    int y = e.y + 18;
                    if (x + tipW > getWidth() - 6)
                        x = e.x - tipW - 16;
                    if (y + tipH > getHeight() - 6)
                        y = e.y - tipH - 14;
                    hoverGuidanceBounds = { juce::jmax (6, x), juce::jmax (6, y), tipW, tipH };
                }
                break;
            }
        }

        if (previousText != hoverGuidance || previousHover != hoverGuidanceBounds.expanded (4))
        {
            if (! previousHover.isEmpty())
                repaint (previousHover);
            if (! hoverGuidanceBounds.isEmpty())
                repaint (hoverGuidanceBounds.expanded (4));
        }

        // BR-corner resize cursor on selected group or selected element
        juce::Rectangle<int> multiBounds;
        if (getMultiSelectionScreenBounds (multiBounds))
        {
            if (resizeHandleAt (e.getPosition(), multiBounds) != ResizeHandle::None)
            {
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }

        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            if (resizeHandleAt (e.getPosition(), r) != ResizeHandle::None)
            {
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }

        if (sel != nullptr && sel->type == ElementType::DrumGrid && owner.isElementSelected (sel->id)
            && (e.mods.isAltDown() || e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
        {
            int pattern = 0, track = 0, step = 0;
            float velocity = 0.0f;
            if (drumCellAt (*sel, elementScreenRect (*sel), e.getPosition(), pattern, track, step, velocity))
            {
                setMouseCursor (juce::MouseCursor::CrosshairCursor);
                return;
            }
        }

        // Up/down cursor over a parameter-bound control body
        const auto& elements = owner.getProject().getLayout().getAll();
        for (auto it = elements.rbegin(); it != elements.rend(); ++it)
        {
            if (! it->visible) continue;
            if (it->type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementScreenRect (*it);
            if (! r.contains (e.getPosition())) continue;
            if (hitTestControlBody (*it, r, e.getPosition()))
            {
                setMouseCursor (canvasControlGuidance (owner.getProject(), *it).isEmpty()
                    ? juce::MouseCursor::UpDownResizeCursor
                    : juce::MouseCursor::NormalCursor);
                return;
            }
            break;
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void CanvasEditor::mouseExit (const juce::MouseEvent&)
    {
        if (hoverGuidance.isEmpty() && hoverGuidanceBounds.isEmpty())
            return;

        const auto previousHover = hoverGuidanceBounds.expanded (4);
        hoverGuidance.clear();
        hoverGuidanceBounds = {};
        repaint (previousHover);
    }

    void CanvasEditor::selectionChanged()
    {
        repaint();
    }

    // -------------------------------------------------------------------------
    // Tab panel rendering + interaction
    // -------------------------------------------------------------------------
    int CanvasEditor::hitTabIndex (const LayoutElement& tabPanel,
                                   juce::Rectangle<int> r,
                                   juce::Point<int> pos) const
    {
        if (! r.contains (pos)) return -1;
        const int n = tabPanel.tabs.size();
        if (n <= 0) return -1;
        const float tabW = (float) r.getWidth() / (float) n;
        const int idx = juce::jlimit (0, n - 1,
                                      (int) ((pos.x - r.getX()) / tabW));
        return idx;
    }

    void CanvasEditor::drawTabPanel (juce::Graphics& g, const LayoutElement& e,
                                     juce::Rectangle<int> r, bool selected) const
    {
        const int n = juce::jmax (1, e.tabs.size());
        const float tabW = (float) r.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            const auto label = i < e.tabs.size() ? e.tabs[i] : juce::String ("Tab");
            const auto groupId = scopedTabGroupId (e, label);
            const auto found = activeTabGroupsByPanel.find (e.id);
            const auto activeGroup = found != activeTabGroupsByPanel.end()
                ? found->second
                : (e.id == "tabs" ? currentTabGroup
                                   : (e.tabs.isEmpty() ? juce::String() : scopedTabGroupId (e, e.tabs[0])));
            const bool active = (groupId == activeGroup);

            const float x = r.getX() + i * tabW;
            juce::Rectangle<float> tabRect (x, (float) r.getY(), tabW, (float) r.getHeight());

            // Active = bright accent; inactive = mid-tone (clearly readable, not
            // textDim() which the user reported as too dark).
            g.setColour (active ? PatchCraftLookAndFeel::textBright()
                                : juce::Colour (0xffb8bcc4));
            g.setFont (juce::Font (juce::jmax (10.0f, tabRect.getHeight() * 0.42f),
                                   juce::Font::bold));
            g.drawText (label.toUpperCase(), tabRect.toNearestInt(),
                        juce::Justification::centred);

            if (active)
            {
                g.setColour (PatchCraftLookAndFeel::accent());
                g.fillRect (tabRect.removeFromBottom (2.0f).toNearestInt());
            }
        }

        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (r.expanded (1), 1);
        }
    }

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------
    bool CanvasEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key.isKeyCode (juce::KeyPress::tabKey))
        {
            struct Candidate
            {
                juce::String id;
                int x = 0;
                int y = 0;
                int row = 0;
            };

            std::vector<Candidate> candidates;
            for (const auto& element : owner.getProject().getLayout().getAll())
            {
                if (! element.visible || element.locked || element.id == "background")
                    continue;
                if (! isElementOnCurrentTab (element))
                    continue;

                Candidate candidate;
                candidate.id = element.id;
                candidate.x = element.x;
                candidate.y = element.y;
                candidate.row = juce::roundToInt ((float) element.y / 24.0f);
                candidates.push_back (candidate);
            }

            if (candidates.empty())
                return false;

            std::sort (candidates.begin(), candidates.end(), [] (const Candidate& a, const Candidate& b)
            {
                if (a.row != b.row) return a.row < b.row;
                if (a.y != b.y) return a.y < b.y;
                if (a.x != b.x) return a.x < b.x;
                return a.id < b.id;
            });

            int index = -1;
            for (int i = 0; i < (int) candidates.size(); ++i)
                if (candidates[(size_t) i].id == owner.getSelectedElementId())
                {
                    index = i;
                    break;
                }

            if (index < 0)
                index = key.getModifiers().isShiftDown() ? 0 : -1;

            index = key.getModifiers().isShiftDown()
                ? (index + (int) candidates.size() - 1) % (int) candidates.size()
                : (index + 1) % (int) candidates.size();

            owner.setSelectedElementId (candidates[(size_t) index].id);
            repaint();
            return true;
        }

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            const auto id = owner.getSelectedElementId();
            if (id.isEmpty()) return false;
            // Don't allow deleting the locked background.
            auto* el = owner.getProject().getLayout().find (id);
            if (el != nullptr && el->locked) return true;
            owner.deleteSelected();
            return true;
        }
        if (key.getKeyCode() == 'D' && key.getModifiers().isCommandDown())
        {
            owner.duplicateSelected();
            return true;
        }
        if (key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown()
            && ! key.getModifiers().isShiftDown())
        {
            owner.undo();
            return true;
        }
        if ((key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown()
             && key.getModifiers().isShiftDown())
            || (key.getKeyCode() == 'Y' && key.getModifiers().isCommandDown()))
        {
            owner.redo();
            return true;
        }
        // Arrow keys nudge selected element by 1px (or 10px with shift).
        if (key.isKeyCode (juce::KeyPress::leftKey)
            || key.isKeyCode (juce::KeyPress::rightKey)
            || key.isKeyCode (juce::KeyPress::upKey)
            || key.isKeyCode (juce::KeyPress::downKey))
        {
            const int step = key.getModifiers().isShiftDown() ? 10 : 1;
            const auto ids = owner.getSelectedElementIds();
            bool hasMovable = false;
            for (const auto& id : ids)
                if (auto* el = owner.getProject().getLayout().find (id); el != nullptr && ! el->locked)
                {
                    hasMovable = true;
                    break;
                }
            if (! hasMovable) return false;

            const bool left = key.isKeyCode (juce::KeyPress::leftKey);
            const bool right = key.isKeyCode (juce::KeyPress::rightKey);
            const bool up = key.isKeyCode (juce::KeyPress::upKey);
            const bool down = key.isKeyCode (juce::KeyPress::downKey);
            owner.getProject().performLayoutEdit ("Nudge selection",
                [ids, step, left, right, up, down] (LayoutModel& m)
                {
                    for (const auto& id : ids)
                        if (auto* el = m.find (id); el != nullptr && ! el->locked)
                        {
                            if (left)  el->x -= step;
                            if (right) el->x += step;
                            if (up)    el->y -= step;
                            if (down)  el->y += step;
                        }
                });
            repaint();
            return true;
        }
        return false;
    }

