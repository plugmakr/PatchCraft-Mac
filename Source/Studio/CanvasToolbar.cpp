#include "CanvasToolbar.h"
#include "StudioMainComponent.h"
#include "CanvasEditor.h"
#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    CanvasToolbar::CanvasToolbar (StudioMainComponent& o, CanvasEditor& c)
        : owner (o), canvas (c)
    {
        canvasLabel.setText ("Canvas", juce::dontSendNotification);
        canvasLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        canvasLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (canvasLabel);

        engineLabel.setText ("Engine", juce::dontSendNotification);
        engineLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        engineLabel.setFont (juce::Font (11.0f));
        addAndMakeVisible (engineLabel);

        engineBox.addItem ("Sampler",       1);
        engineBox.addItem ("Synth",         2);
        engineBox.addItem ("Effect",        3);
        engineBox.addItem ("Drum Machine",  4);
        engineBox.onChange = [this] { applySelectedEngine(); };
        addAndMakeVisible (engineBox);

        // Section tab strip - styled as flat toggle buttons. Each tab maps
        // explicitly to its BottomPanel::Page so removing/reordering tabs
        // doesn't break the wiring.
        struct TabSpec { juce::TextButton* btn; int pageIndex; };
        const TabSpec specs[] = {
            { &tabDesign,   (int) BottomPanel::Page::Design },
            { &tabMapper,   (int) BottomPanel::Page::Samples },
            { &tabOneShot,  (int) BottomPanel::Page::OneShotMaker },
            { &tabMidi,     (int) BottomPanel::Page::MidiPlayground },
            { &tabArp,      (int) BottomPanel::Page::ArpStudio },
            { &tabDSP,      (int) BottomPanel::Page::DSP },
            { &tabBuild,    (int) BottomPanel::Page::Widgets },
            { &tabAnimation,(int) BottomPanel::Page::Animation },
            { &tabBranding, (int) BottomPanel::Page::Branding },
            { &tabLaunch,   (int) BottomPanel::Page::Export }
        };
        for (const auto& s : specs)
        {
            auto* b = s.btn;
            b->setClickingTogglesState (true);
            b->setRadioGroupId (8801);
            b->getProperties().set ("flatTab", true);
            b->getProperties().set ("fontSize", 12.0);
            b->getProperties().set ("bold", true);
            const int captured = s.pageIndex;
            b->onClick = [this, captured] { onSectionTabClick (captured); };
            addAndMakeVisible (*b);
        }
        tabDesign.setToggleState (true, juce::dontSendNotification);

        patchSeparator.setText ("|", juce::dontSendNotification);
        patchSeparator.setJustificationType (juce::Justification::centred);
        patchSeparator.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::border());
        addAndMakeVisible (patchSeparator);

        for (auto* button : { &savePatchBtn, &savePatchAsBtn })
        {
            button->getProperties().set ("smallButton", true);
            button->getProperties().set ("fontSize", 11.0);
            addAndMakeVisible (*button);
        }
        savePatchBtn.onClick = [this] { owner.saveCurrentDspPatch(); };
        savePatchAsBtn.onClick = [this] { owner.saveCurrentDspPatchAs(); };

        // Approved instrument sizes. All packs in this Studio's projects
        // should pick one of these so backgrounds line up.
        sizePresets = {
            { 1280, 800,  "1280 x 800  -  Standard" },
            { 1024, 768,  "1024 x 768  -  Compact" },
            { 1280, 720,  "1280 x 720  -  HD" },
            { 1366, 768,  "1366 x 768" },
            { 1440, 900,  "1440 x 900" },
            { 1600, 900,  "1600 x 900" },
            { 1920, 1080, "1920 x 1080  -  Full HD" }
        };
        for (size_t i = 0; i < sizePresets.size(); ++i)
            sizeBox.addItem (sizePresets[i].label, (int) i + 1);
        sizeBox.onChange = [this] { applySelectedSize(); };
        addAndMakeVisible (sizeBox);

        snapLabel.setText ("Snap", juce::dontSendNotification);
        snapLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        snapLabel.setFont (juce::Font (11.0f));
        addAndMakeVisible (snapLabel);

        const int snaps[] = { 1, 2, 4, 8, 16, 32 };
        int id = 1;
        for (int s : snaps) snapBox.addItem (juce::String (s), id++);
        snapBox.setSelectedId (4, juce::dontSendNotification); // = 8
        snapBox.onChange = [this] { applySelectedSnap(); };
        addAndMakeVisible (snapBox);

        const int zooms[] = { 10, 25, 33, 50, 67, 75, 90, 100, 110, 125, 150, 175, 200, 300, 400 };
        int zid = 1;
        for (int z : zooms) zoomBox.addItem (juce::String (z) + " %", zid++);
        zoomBox.setSelectedId (8, juce::dontSendNotification); // = 100% (we'll fit on first show)
        zoomBox.onChange = [this] { applySelectedZoom(); };
        addAndMakeVisible (zoomBox);

        canvasPropsBtn.getProperties().set ("smallButton", true);
        canvasPropsBtn.getProperties().set ("fontSize", 11.0);
        canvasPropsBtn.setTooltip ("Canvas properties: grid, snap, colours, zoom, and custom size.");
        canvasPropsBtn.onClick = [this] { showCanvasProperties(); };
        addAndMakeVisible (canvasPropsBtn);

        fitBtn.onClick = [this]
        {
            canvas.fit();
            refresh();
            owner.refreshCanvasToolbar();
        };
        addAndMakeVisible (fitBtn);

        // Alignment cluster: 6 align glyphs + 2 distribute + 1 order popup.
        struct AlignSpec { const char* glyph; const char* tip; const char* command; bool distribute; };
        const AlignSpec alignSpecs[] = {
            { "L",  "Align Left",                "left",    false },
            { "C",  "Align Horizontal Centers",  "hcenter", false },
            { "R",  "Align Right",               "right",   false },
            { "T",  "Align Top",                 "top",     false },
            { "M",  "Align Vertical Middles",    "vcenter", false },
            { "B",  "Align Bottom",              "bottom",  false },
            { "DH", "Distribute Horizontally",   "h",       true  },
            { "DV", "Distribute Vertically",     "v",       true  }
        };
        for (const auto& s : alignSpecs)
        {
            const juce::String cmd = s.command;
            const bool dist = s.distribute;
            auto btn = std::make_unique<AlignButton> (s.glyph, s.tip, [this, cmd, dist]
            {
                if (dist) owner.distributeSelected (cmd == "h");
                else      owner.alignSelected (cmd);
            });
            addAndMakeVisible (*btn);
            alignButtons.push_back (std::move (btn));
        }

        alignSeparator.setText ("|", juce::dontSendNotification);
        alignSeparator.setJustificationType (juce::Justification::centred);
        alignSeparator.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::border());
        addAndMakeVisible (alignSeparator);

        orderBtn = std::make_unique<juce::TextButton> ("Order");
        orderBtn->getProperties().set ("smallButton", true);
        orderBtn->getProperties().set ("fontSize", 11.0);
        orderBtn->setTooltip ("Bring to front / send to back");
        orderBtn->onClick = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "Bring to Front");
            m.addItem (2, "Bring Forward");
            m.addItem (3, "Send Backward");
            m.addItem (4, "Send to Back");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (orderBtn.get()),
                [this] (int r)
                {
                    if (r == 1) owner.orderSelected ("front");
                    else if (r == 2) owner.orderSelected ("forward");
                    else if (r == 3) owner.orderSelected ("backward");
                    else if (r == 4) owner.orderSelected ("back");
                });
        };
        addAndMakeVisible (*orderBtn);
    }

    CanvasToolbar::AlignButton::AlignButton (const juce::String& g, const juce::String& tt,
                                             std::function<void()> on)
        : juce::Button (tt), glyph (g), onAction (std::move (on))
    {
        setTooltip (tt);
        onClick = [this] { if (onAction) onAction(); };
    }

    void CanvasToolbar::AlignButton::paintButton (juce::Graphics& g, bool over, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const auto enabled = isEnabled();
        if (enabled && (down || over))
        {
            g.setColour (PatchCraftLookAndFeel::raised().brighter (0.1f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (enabled ? (over ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text())
                              : PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (glyph, getLocalBounds(), juce::Justification::centred);
    }

    int CanvasToolbar::presetIndexFor (int w, int h) const
    {
        for (size_t i = 0; i < sizePresets.size(); ++i)
            if (sizePresets[i].w == w && sizePresets[i].h == h)
                return (int) i;
        return -1;
    }

    void CanvasToolbar::refresh()
    {
        // Engine selection. The "Drum Machine" choice is a sampler with a
        // PadGrid layout, so we detect it by inspecting the current layout.
        const auto eng = owner.getProject().getEngineType();
        bool hasPadGrid = false;
        for (const auto& el : owner.getProject().getLayout().getAll())
            if (el.type == ElementType::PadGrid) { hasPadGrid = true; break; }

        if      (hasPadGrid && eng == "sample") engineBox.setSelectedId (4, juce::dontSendNotification);
        else if (eng == "synth")                engineBox.setSelectedId (2, juce::dontSendNotification);
        else if (eng == "fx")                   engineBox.setSelectedId (3, juce::dontSendNotification);
        else                                    engineBox.setSelectedId (1, juce::dontSendNotification);

        const auto& cs = owner.getProject().getCanvasSize();
        const int idx = presetIndexFor (cs.width, cs.height);
        if (idx >= 0)
            sizeBox.setSelectedId (idx + 1, juce::dontSendNotification);
        else
        {
            // Custom size - add a transient entry.
            const auto label = juce::String (cs.width) + " x " + juce::String (cs.height) + "  -  Custom";
            const int customId = (int) sizePresets.size() + 1;
            if (sizeBox.getItemId (sizeBox.getNumItems() - 1) != customId)
                sizeBox.addItem (label, customId);
            sizeBox.setSelectedId (customId, juce::dontSendNotification);
        }

        const bool dspActive = owner.getBottomTab() == BottomPanel::Page::DSP;
        savePatchBtn.setButtonText ("Save Patch");
        savePatchAsBtn.setButtonText ("Save Patch As");
        savePatchBtn.setVisible (dspActive);
        savePatchAsBtn.setVisible (dspActive);
        patchSeparator.setVisible (dspActive);
        savePatchBtn.setEnabled (dspActive);
        savePatchAsBtn.setEnabled (dspActive);

        // Alignment cluster: only meaningful on the Design page, and the
        // distribute / order ops need 2+ or 3+ selected to do anything.
        const bool designActive = owner.getBottomTab() == BottomPanel::Page::Design;
        const int selectionCount = owner.getSelectedElementIds().size();

        if (canvas.isAutoFitEnabled())
        {
            zoomBox.setText ("Fit", juce::dontSendNotification);
        }
        else
        {
            const int pct = juce::roundToInt (canvas.getZoom() * 100.0f);
            int selectedZoomId = 0;
            for (int i = 0; i < zoomBox.getNumItems(); ++i)
                if (zoomBox.getItemText (i).startsWith (juce::String (pct)))
                    selectedZoomId = zoomBox.getItemId (i);
            if (selectedZoomId > 0)
                zoomBox.setSelectedId (selectedZoomId, juce::dontSendNotification);
            else
                zoomBox.setText (juce::String (pct) + " %", juce::dontSendNotification);
        }
        snapBox.setEnabled (canvas.isSnapEnabled());
        alignmentVisible = designActive;
        for (auto& b : alignButtons)
        {
            b->setVisible (designActive);
            const auto label = b->glyph;
            const bool needsThree = (label == "DH" || label == "DV");
            b->setEnabled (needsThree ? selectionCount >= 3 : selectionCount >= 2);
        }
        alignSeparator.setVisible (designActive);
        if (orderBtn != nullptr)
        {
            orderBtn->setVisible (designActive);
            orderBtn->setEnabled (selectionCount >= 1);
        }
        resized();
        repaint();
    }

    void CanvasToolbar::onSectionTabClick (int index)
    {
        owner.setBottomTab (static_cast<BottomPanel::Page> (index));
    }

    void CanvasToolbar::syncSectionTabFromOwner()
    {
        const auto p = owner.getBottomTab();

        tabDesign.setToggleState   (p == BottomPanel::Page::Design,         juce::dontSendNotification);
        tabMapper.setToggleState   (p == BottomPanel::Page::Samples,   juce::dontSendNotification);
        tabOneShot.setToggleState  (p == BottomPanel::Page::OneShotMaker,   juce::dontSendNotification);
        tabMidi  .setToggleState   (p == BottomPanel::Page::MidiPlayground, juce::dontSendNotification);
        tabArp   .setToggleState   (p == BottomPanel::Page::ArpStudio,      juce::dontSendNotification);
        tabDSP   .setToggleState   (p == BottomPanel::Page::DSP,            juce::dontSendNotification);
        tabBuild .setToggleState   (p == BottomPanel::Page::Widgets,          juce::dontSendNotification);
        tabAnimation.setToggleState (p == BottomPanel::Page::Animation,      juce::dontSendNotification);
        tabBranding.setToggleState (p == BottomPanel::Page::Branding
                                  || p == BottomPanel::Page::Test,         juce::dontSendNotification);
        tabLaunch.setToggleState   (p == BottomPanel::Page::Export,         juce::dontSendNotification);
        refresh();
    }

    void CanvasToolbar::applySelectedEngine()
    {
        const int idx = engineBox.getSelectedId();
        juce::String e;
        if      (idx == 2) e = "synth";
        else if (idx == 3) e = "fx";
        else if (idx == 4) e = "drum";
        else               e = "sample";

        // Detect current "drum mode" by layout content: drum mode keeps the
        // manifest engine as "sample" but adds a PadGrid element. Skip the
        // early-out only when the dropdown choice differs from the visible
        // selection.
        const auto currentEng = owner.getProject().getEngineType();
        bool currentHasPadGrid = false;
        for (const auto& el : owner.getProject().getLayout().getAll())
            if (el.type == ElementType::PadGrid) { currentHasPadGrid = true; break; }
        const juce::String currentVisible = currentHasPadGrid && currentEng == "sample"
                                          ? juce::String ("drum") : currentEng;
        if (e == currentVisible) return;

        // Confirm with user before wiping the layout.
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("Switch engine")
                .withMessage ("Switching engine type will replace the parameter "
                              "list and the layout with the " + e + " template. "
                              "Continue?")
                .withButton ("Switch")
                .withButton ("Cancel")
                .withIconType (juce::MessageBoxIconType::QuestionIcon),
            [this, e] (int result)
            {
                if (result == 1)
                    owner.getProject().setEngineType (e);
                else
                    refresh();   // revert dropdown selection
            });
    }

    void CanvasToolbar::applySelectedSize()
    {
        const int idx = sizeBox.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) sizePresets.size()) return;
        auto& cs = owner.getProject().getCanvasSize();
        const auto preset = sizePresets[(size_t) idx];
        if (cs.width == preset.w && cs.height == preset.h) return;
        cs.width  = preset.w;
        cs.height = preset.h;

        // Resize the canvas-wide "background" image element to match.
        if (auto* bg = owner.getProject().getLayout().find ("background"))
        {
            bg->x = 0; bg->y = 0;
            bg->width  = preset.w;
            bg->height = preset.h;
        }
        owner.getProject().notifyChanged();
        canvas.setZoom (canvas.getZoom());
    }

    void CanvasToolbar::applySelectedZoom()
    {
        const int idx = zoomBox.getSelectedId() - 1;
        const int zooms[] = { 10, 25, 33, 50, 67, 75, 90, 100, 110, 125, 150, 175, 200, 300, 400 };
        if (idx < 0 || idx >= (int) (sizeof (zooms) / sizeof (zooms[0]))) return;
        canvas.setZoom (zooms[idx] / 100.0f);
    }

    void CanvasToolbar::applySelectedSnap()
    {
        const int snaps[] = { 1, 2, 4, 8, 16, 32 };
        const int idx = snapBox.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) (sizeof (snaps) / sizeof (snaps[0]))) return;
        canvas.setSnap (snaps[idx]);
    }

    void CanvasToolbar::showCanvasProperties()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Grid Visible", true, canvas.isGridVisible());
        menu.addItem (2, "Snap Enabled", true, canvas.isSnapEnabled());
        menu.addSeparator();
        menu.addItem (3, "Grid Colour...");
        menu.addItem (4, "Snap Colour...");
        menu.addSeparator();
        menu.addItem (5, "Zoom In  (Shift + Mouse Wheel)");
        menu.addItem (6, "Zoom Out  (Shift + Mouse Wheel)");
        menu.addItem (7, "Fit Canvas");
        menu.addSeparator();
        menu.addItem (8, "Custom Canvas Size...");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&canvasPropsBtn),
            [this] (int result)
            {
                if (result == 1)      canvas.setGridVisible (! canvas.isGridVisible());
                else if (result == 2) canvas.setSnapEnabled (! canvas.isSnapEnabled());
                else if (result == 3) showCanvasColourMenu (true);
                else if (result == 4) showCanvasColourMenu (false);
                else if (result == 5) canvas.setZoom (canvas.getZoom() * 1.15f);
                else if (result == 6) canvas.setZoom (canvas.getZoom() * 0.85f);
                else if (result == 7) canvas.fit();
                else if (result == 8) showCanvasSizeDialog();

                if (result > 0)
                    refresh();
            });
    }

    void CanvasToolbar::showCanvasColourMenu (bool chooseGridColour)
    {
        struct Swatch { int id; const char* name; juce::Colour colour; };
        const Swatch swatches[] = {
            { 1, "Studio Blue",  juce::Colour (0xff253449) },
            { 2, "Warm Amber",   juce::Colour (0xff3b2a13) },
            { 3, "Soft Grey",    juce::Colour (0xff252932) },
            { 4, "Deep Purple",  juce::Colour (0xff2a2440) },
            { 5, "Muted Green",  juce::Colour (0xff20372d) },
            { 6, "Low Contrast", juce::Colour (0xff15181e) }
        };

        juce::PopupMenu menu;
        for (const auto& swatch : swatches)
            menu.addItem (swatch.id, swatch.name);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&canvasPropsBtn),
            [this, chooseGridColour, swatches] (int result)
            {
                for (const auto& swatch : swatches)
                {
                    if (swatch.id != result)
                        continue;
                    if (chooseGridColour) canvas.setGridColour (swatch.colour);
                    else                  canvas.setSnapColour (swatch.colour.brighter (0.35f));
                    break;
                }
            });
    }

    void CanvasToolbar::showCanvasSizeDialog()
    {
        const auto& cs = owner.getProject().getCanvasSize();
        auto window = std::make_unique<juce::AlertWindow> ("Custom Canvas Size",
                                                           "Set the canvas pixel size. Existing elements keep their positions and dimensions.",
                                                           juce::AlertWindow::NoIcon);
        auto* raw = window.get();
        raw->addTextEditor ("width", juce::String (cs.width), "Width");
        raw->addTextEditor ("height", juce::String (cs.height), "Height");
        raw->addButton ("Apply", 1, juce::KeyPress (juce::KeyPress::returnKey));
        raw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        raw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, raw] (int result)
            {
                std::unique_ptr<juce::AlertWindow> cleanup (raw);
                if (result != 1)
                    return;

                const int w = juce::jlimit (320, 4096, cleanup->getTextEditorContents ("width").getIntValue());
                const int h = juce::jlimit (240, 4096, cleanup->getTextEditorContents ("height").getIntValue());
                auto& csRef = owner.getProject().getCanvasSize();
                if (csRef.width == w && csRef.height == h)
                    return;

                csRef.width = w;
                csRef.height = h;
                if (auto* bg = owner.getProject().getLayout().find ("background"))
                {
                    bg->x = 0;
                    bg->y = 0;
                    bg->width = w;
                    bg->height = h;
                }
                owner.getProject().notifyChanged();
                canvas.setZoom (canvas.getZoom());
                refresh();
            }),
            false);
        raw->setVisible (true);
        window.release();
    }

    void CanvasToolbar::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panelAlt());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void CanvasToolbar::resized()
    {
        auto r = getLocalBounds().reduced (10, 4);

        canvasLabel.setBounds (r.removeFromLeft (54));
        sizeBox.setBounds     (r.removeFromLeft (180));
        r.removeFromLeft (8);
        engineLabel.setBounds (r.removeFromLeft (42));
        engineBox.setBounds   (r.removeFromLeft (88));
        r.removeFromLeft (12);

        // Section tabs (left of right cluster)
        tabDesign.setBounds (r.removeFromLeft (66));
        tabMapper.setBounds (r.removeFromLeft (62));
        tabOneShot.setBounds (r.removeFromLeft (70));
        tabDSP   .setBounds (r.removeFromLeft (42));
        tabMidi  .setBounds (r.removeFromLeft (44));
        tabArp   .setBounds (r.removeFromLeft (44));
        tabBuild .setBounds (r.removeFromLeft (58));
        tabAnimation.setBounds (r.removeFromLeft (48));
        tabBranding.setBounds (r.removeFromLeft (62));
        tabLaunch.setBounds (r.removeFromLeft (52));
        if (owner.getBottomTab() == BottomPanel::Page::DSP)
        {
            patchSeparator.setBounds (r.removeFromLeft (14));
            savePatchBtn.setBounds (r.removeFromLeft (130).reduced (2));
            savePatchAsBtn.setBounds (r.removeFromLeft (150).reduced (2));
        }
        r.removeFromLeft (12);

        // Right cluster
        fitBtn.setBounds   (r.removeFromRight (60));
        r.removeFromRight (4);
        canvasPropsBtn.setBounds (r.removeFromRight (70));
        r.removeFromRight (4);
        zoomBox.setBounds  (r.removeFromRight (90));
        r.removeFromRight (12);
        snapBox.setBounds  (r.removeFromRight (60));
        r.removeFromRight (4);
        snapLabel.setBounds (r.removeFromRight (40));

        // Alignment cluster (Design page only). Sit between section tabs and
        // the right cluster. Hidden tabs aren't part of the row, so we lay
        // out left-to-right within whatever space is left between them.
        if (alignmentVisible && ! alignButtons.empty())
        {
            r.removeFromLeft (8);
            const int btnW = 26;
            for (size_t i = 0; i < alignButtons.size(); ++i)
            {
                // Group separator after the first 6 align buttons.
                if (i == 6) r.removeFromLeft (4);
                alignButtons[i]->setBounds (r.removeFromLeft (btnW).reduced (0, 1));
            }
            alignSeparator.setBounds (r.removeFromLeft (10));
            if (orderBtn != nullptr)
                orderBtn->setBounds (r.removeFromLeft (66).reduced (1, 2));
        }
    }

} // namespace patchcraft
