#include "TopToolbar.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "../Shared/ControlNodeAuthoring.h"

namespace patchcraft
{
    // ---- Icon drawing helpers ---------------------------------------------
    static void drawIcon (juce::Graphics& g, juce::Rectangle<float> r,
                          const juce::String& key, juce::Colour col)
    {
        g.setColour (col);
        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.85f;
        juce::Rectangle<float> box (cx - s * 0.5f, cy - s * 0.5f, s, s);
        const float t = juce::jmax (1.5f, s * 0.085f);

        if (key == "new")
        {
            // page icon
            juce::Path p;
            const float w = s * 0.7f, h = s * 0.85f;
            p.startNewSubPath (cx - w * 0.5f, cy - h * 0.5f);
            p.lineTo (cx + w * 0.3f, cy - h * 0.5f);
            p.lineTo (cx + w * 0.5f, cy - h * 0.5f + w * 0.2f);
            p.lineTo (cx + w * 0.5f, cy + h * 0.5f);
            p.lineTo (cx - w * 0.5f, cy + h * 0.5f);
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (t));
        }
        else if (key == "open")
        {
            juce::Path p;
            p.addRoundedRectangle (cx - s * 0.45f, cy - s * 0.30f,
                                   s * 0.9f,        s * 0.6f, 2.0f);
            p.startNewSubPath (cx - s * 0.40f, cy - s * 0.30f);
            p.lineTo (cx - s * 0.20f, cy - s * 0.40f);
            p.lineTo (cx + s * 0.05f, cy - s * 0.40f);
            p.lineTo (cx + s * 0.10f, cy - s * 0.30f);
            g.strokePath (p, juce::PathStrokeType (t));
        }
        else if (key == "save")
        {
            juce::Path p;
            p.addRoundedRectangle (box, 2.0f);
            g.strokePath (p, juce::PathStrokeType (t));
            g.fillRect (cx - s * 0.25f, cy + s * 0.08f, s * 0.5f, s * 0.20f);
            g.fillRect (cx - s * 0.18f, cy - s * 0.40f, s * 0.36f, s * 0.18f);
        }
        else if (key == "importSamples")
        {
            juce::Path p;
            p.addRoundedRectangle (box, 2.0f);
            g.strokePath (p, juce::PathStrokeType (t));
            // arrow down
            juce::Path arr;
            arr.startNewSubPath (cx, cy - s * 0.30f);
            arr.lineTo (cx, cy + s * 0.10f);
            arr.startNewSubPath (cx - s * 0.18f, cy - s * 0.05f);
            arr.lineTo (cx, cy + s * 0.15f);
            arr.lineTo (cx + s * 0.18f, cy - s * 0.05f);
            g.strokePath (arr, juce::PathStrokeType (t));
        }
        else if (key == "importBg")
        {
            juce::Path p;
            p.addRoundedRectangle (box, 2.0f);
            g.strokePath (p, juce::PathStrokeType (t));
            // mountain & sun
            g.fillEllipse (cx + s * 0.10f, cy - s * 0.25f, s * 0.18f, s * 0.18f);
            juce::Path m;
            m.startNewSubPath (cx - s * 0.40f, cy + s * 0.25f);
            m.lineTo (cx - s * 0.10f, cy - s * 0.05f);
            m.lineTo (cx + s * 0.10f, cy + s * 0.10f);
            m.lineTo (cx + s * 0.30f, cy - s * 0.10f);
            m.lineTo (cx + s * 0.45f, cy + s * 0.25f);
            g.strokePath (m, juce::PathStrokeType (t));
        }
        else if (key == "aiAssist")
        {
            // sparkle / sun ray icon
            juce::Path p;
            const float r1 = s * 0.18f, r2 = s * 0.45f;
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * i / 8.0f;
                p.startNewSubPath (cx + std::cos (a) * r1, cy + std::sin (a) * r1);
                p.lineTo (cx + std::cos (a) * r2, cy + std::sin (a) * r2);
            }
            g.strokePath (p, juce::PathStrokeType (t));
            g.fillEllipse (cx - s * 0.10f, cy - s * 0.10f, s * 0.20f, s * 0.20f);
        }
        else if (key == "preview")
        {
            // play triangle (in a circle)
            g.drawEllipse (box, t);
            juce::Path tri;
            tri.addTriangle (cx - s * 0.10f, cy - s * 0.18f,
                             cx - s * 0.10f, cy + s * 0.18f,
                             cx + s * 0.20f, cy);
            g.fillPath (tri);
        }
        else if (key == "export")
        {
            // tray with up arrow
            juce::Path tray;
            tray.startNewSubPath (cx - s * 0.4f, cy + s * 0.10f);
            tray.lineTo (cx - s * 0.4f, cy + s * 0.30f);
            tray.lineTo (cx + s * 0.4f, cy + s * 0.30f);
            tray.lineTo (cx + s * 0.4f, cy + s * 0.10f);
            g.strokePath (tray, juce::PathStrokeType (t));
            juce::Path arr;
            arr.startNewSubPath (cx, cy - s * 0.30f);
            arr.lineTo (cx, cy + s * 0.10f);
            arr.startNewSubPath (cx - s * 0.18f, cy - s * 0.10f);
            arr.lineTo (cx, cy - s * 0.30f);
            arr.lineTo (cx + s * 0.18f, cy - s * 0.10f);
            g.strokePath (arr, juce::PathStrokeType (t));
        }
    }

    // ----------------------------------------------------------------------
    TopToolbar::IconLabelButton::IconLabelButton (juce::String l, juce::String k)
        : juce::Button (l), label (std::move (l)), iconKey (std::move (k))
    {
        setSize (84, 56);
    }

    void TopToolbar::IconLabelButton::paintButton (juce::Graphics& g, bool over, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);

        if (accent)
        {
            auto col = PatchCraftLookAndFeel::accent();
            if (down) col = col.darker (0.15f);
            else if (over) col = col.brighter (0.05f);
            juce::ColourGradient grad (col.brighter (0.08f), r.getX(), r.getY(),
                                       col.darker (0.18f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colour (0xff050608).withAlpha (0.35f));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        else
        {
            auto base = PatchCraftLookAndFeel::raised().withAlpha (over || down ? 0.96f : 0.38f);
            if (down) base = base.brighter (0.08f);
            else if (over) base = base.brighter (0.12f);
            g.setColour (base);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (over ? PatchCraftLookAndFeel::accent().withAlpha (0.38f)
                              : PatchCraftLookAndFeel::border().withAlpha (0.45f));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }

        const auto col = accent
            ? juce::Colour (0xff15110b)
            : (over ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());

        if (getWidth() < 60)
        {
            auto iconArea = r.reduced (8.0f);
            drawIcon (g, iconArea, iconKey, col);
        }
        else
        {
            auto iconArea = r.withTrimmedBottom (16.0f).reduced (10.0f);
            drawIcon (g, iconArea, iconKey, col);

            g.setColour (col);
            g.setFont (juce::Font (11.0f, juce::Font::plain));
            g.drawText (label, getLocalBounds().removeFromBottom (18),
                        juce::Justification::centred);
        }
    }

    // ----------------------------------------------------------------------
    TopToolbar::TopToolbar (StudioMainComponent& o) : owner (o)
    {
        auto addBtn = [this] (std::unique_ptr<IconLabelButton>& b,
                              const juce::String& label, const juce::String& iconKey,
                              std::function<void()> action,
                              bool accentColour = false)
        {
            b = std::make_unique<IconLabelButton> (label, iconKey);
            b->accent = accentColour;
            b->onClick = std::move (action);
            addAndMakeVisible (*b);
        };

        addBtn (btnNew,           "New",            "new",
            [this]
            {
                owner.getProject().getLayout().clear();
                owner.getProject().getManifest().backgroundImage = juce::String();
                owner.getProject().notifyChanged (PatchCraftProject::ChangeScope::layout);
            });
        addBtn (btnImportSamples, "Import Samples", "importSamples",  [this] { owner.importSamples(); });
        addBtn (btnImportBg,      "Import BG",      "importBg",       [this] { owner.importBackground(); });
        addBtn (btnPacks,         "Packs",          "importSamples",  [this] { owner.togglePacksPanel(); });
        addBtn (btnDashboard,     "Dashboard",      "preview",        [this] { owner.setBottomTab (BottomPanel::Page::Dashboard); });
        addBtn (btnProjects,      "Projects",       "open",           [this] { owner.setBottomTab (BottomPanel::Page::ProjectBrowser); });
#if PATCHCRAFT_ENABLE_AI_STUDIO
        addBtn (btnAiAssist,      "AI Assist",      "aiAssist",       [this] { owner.aiAssist(); });
#endif
        addBtn (btnPreview,       "Preview", "preview", [this] { owner.togglePreview(); }, true);
        addBtn (btnExport,        "Export Pack",    "export",
            [this]
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Export Pack...");
                menu.addItem (2, "Send to Expansion Pack...");
                menu.addSeparator();
                menu.addItem (3, "Export VST3 Plugin...");
                menu.addItem (4, "Publish Draft to Plugin.club...");
                menu.showMenuAsync (juce::PopupMenu::Options(),
                    [this] (int result)
                    {
                        if (result == 1) owner.exportPack();
                        if (result == 2) owner.sendToExpansionPack();
                        if (result == 3) owner.exportVstPlugin();
                        if (result == 4) owner.publishToPluginClub();
                    });
            }, true);

        btnExport->setTooltip ("Export a playable .patchcraft pack, or send it into an expansion-pack folder.");

        projectNameLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        projectNameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        projectNameLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (projectNameLabel);

        projectStatusLabel.setFont (juce::Font (11.0f));
        projectStatusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        projectStatusLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (projectStatusLabel);

        presetBox.addItemList (ControlNodeAuthoring::getPresetNames(), 1);
        presetBox.setTextWhenNothingSelected ("Graph Templates...");
        presetBox.onChange = [this]
        {
            if (presetBox.getSelectedId() == 0 || ! presetBox.isEnabled())
                return;

            const auto templateName = presetBox.getText();
            presetBox.setEnabled (false);

            juce::Component::SafePointer<TopToolbar> safeThis (this);
            juce::MessageManager::callAsync ([safeThis, templateName]
            {
                if (auto* toolbar = safeThis.getComponent())
                {
                    juce::String message;
                    const bool ok = ControlNodeAuthoring::applyPreset (toolbar->owner.getProject(), templateName, message);
                    toolbar->presetBox.setSelectedId (0, juce::dontSendNotification);
                    toolbar->presetBox.setTextWhenNothingSelected ("Graph Templates...");
                    toolbar->presetBox.setEnabled (true);
                    if (ok)
                        toolbar->owner.getProject().notifyChanged();
                    toolbar->resized();
                    juce::ignoreUnused (message);
                }
            });
        };
        addAndMakeVisible (presetBox);

        settingsBtn.setButtonText ("Settings");
        settingsBtn.getProperties().set ("fontSize", 11.0);
        settingsBtn.setTooltip ("Audio + MIDI device settings");
        settingsBtn.onClick = [this] { owner.openSettings(); };
        addAndMakeVisible (settingsBtn);

        setProjectName ("Cinematic Evolve Pad", true);
    }

    void TopToolbar::setProjectName (juce::String name, bool dirty)
    {
        projectNameLabel.setText (name, juce::dontSendNotification);
        projectStatusLabel.setText (dirty ? "Unsaved Changes" : "Saved",
                                    juce::dontSendNotification);
        projectStatusLabel.setColour (juce::Label::textColourId,
            dirty ? PatchCraftLookAndFeel::accent()
                  : PatchCraftLookAndFeel::textDim());
    }

    void TopToolbar::setPreviewActive (bool active, const juce::String& idleLabel)
    {
        previewActive = active;
        if (active)
            btnPreview->label = idleLabel == "Listen" ? "Stop Listen" : "Exit Preview";
        else
            btnPreview->label = idleLabel;
        repaint();
    }

    void TopToolbar::paint (juce::Graphics& g)
    {
        // Background gradient bar
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient grad (juce::Colour (0xff11161d), 0.0f, 0.0f,
                                   juce::Colour (0xff07090d), 0.0f, r.getHeight(), false);
        g.setGradientFill (grad);
        g.fillAll();

        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.07f));
        const float logoW = getWidth() < 1500 ? 248.0f : 280.0f;
        const float menuX = logoW + 12.0f;
        const float rightClusterX = btnPreview != nullptr ? (float) btnPreview->getX() - 10.0f : r.getWidth() - 520.0f;
        g.fillRoundedRectangle (juce::Rectangle<float> (menuX, 7.0f,
                                                        juce::jmax (120.0f, rightClusterX - menuX - 8.0f),
                                                        r.getHeight() - 14.0f),
                                10.0f);
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.55f));
        g.drawRoundedRectangle (juce::Rectangle<float> (menuX + 0.5f, 7.5f,
                                                        juce::jmax (120.0f, rightClusterX - menuX - 9.0f),
                                                        r.getHeight() - 15.0f),
                                10.0f, 1.0f);

        // Bottom border line
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        // PATCHCRAFT logo block on the far left
        auto logoArea = juce::Rectangle<int> (0, 0, (int) logoW, getHeight()).toFloat();
        juce::ColourGradient logoGrad (juce::Colour (0xff171c22), logoArea.getX(), logoArea.getY(),
                                       juce::Colour (0xff0d1015), logoArea.getX(), logoArea.getBottom(), false);
        g.setGradientFill (logoGrad);
        g.fillRect (logoArea);
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (logoArea.removeFromRight (1.0f));

        // hex glyph
        auto hexBox = juce::Rectangle<float> (16, 14, 44, 44);
        PatchCraftLookAndFeel::drawHexLogo (g, hexBox);

        // Wordmark
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (22.0f, juce::Font::bold));
        g.drawText ("PATCHCRAFT", 72, 12, (int) logoW - 84, 26, juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f, juce::Font::plain));
        g.drawText ("INSTRUMENT BUILDER", 72, 38, (int) logoW - 84, 16, juce::Justification::centredLeft);

        // Subtle separators between button groups
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.5f));
        if (btnImportSamples && btnDashboard)
        {
            int sep = btnDashboard->getRight() + 8;
            g.fillRect (sep, 12, 1, getHeight() - 24);
        }

        if (btnPreview != nullptr && settingsBtn.isVisible())
        {
            auto cluster = btnPreview->getBounds()
                .getUnion (btnExport->getBounds())
                .getUnion (presetBox.getBounds())
                .getUnion (projectNameLabel.getBounds())
                .getUnion (projectStatusLabel.getBounds())
                .getUnion (settingsBtn.getBounds());

            auto clusterF = cluster.expanded (8, 7).toFloat();

            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.58f));
            g.fillRoundedRectangle (clusterF, 9.0f);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.7f));
            g.drawRoundedRectangle (clusterF.reduced (0.5f), 9.0f, 1.0f);

            auto drawSep = [&] (int x)
            {
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.62f));
                g.fillRect (x, 13, 1, getHeight() - 26);
            };
            drawSep ((btnExport->getRight() + presetBox.getX()) / 2);
            drawSep ((presetBox.getRight() + projectNameLabel.getX()) / 2);
            drawSep ((projectNameLabel.getRight() + settingsBtn.getX()) / 2);
        }
    }

    void TopToolbar::resized()
    {
        const bool compact = getWidth() < 1300;
        const bool ultraCompact = getWidth() < 1100;
        
        const int logoW = compact ? 248 : 280;
        const int btnW = ultraCompact ? 44 : (compact ? 70 : 84);
        const int top  = 6;
        const int height = getHeight() - 12;

        // Right cluster calculation
        const int settingsW = ultraCompact ? 40 : (compact ? 64 : 70);
        const int projectW = ultraCompact ? 100 : (compact ? 130 : 220);
        const int presetW = ultraCompact ? 80 : (compact ? 100 : 160);
        const int exportW = ultraCompact ? 60 : (compact ? 92 : btnW + 20);
        const int previewW = ultraCompact ? 44 : (compact ? 76 : btnW);

        int rightX = getWidth() - 8;
        
        settingsBtn.setBounds (rightX - settingsW, (getHeight() - 26) / 2, settingsW, 26);
        settingsBtn.setButtonText (ultraCompact ? "" : "Settings");
        rightX = settingsBtn.getX() - 10;

        projectNameLabel.setBounds (rightX - projectW, 8, projectW, 22);
        projectStatusLabel.setBounds (rightX - projectW, 30, projectW, 18);
        rightX = projectNameLabel.getX() - 10;

        presetBox.setBounds (rightX - presetW, (getHeight() - 24) / 2, presetW, 24);
        rightX = presetBox.getX() - 12;

        btnExport->setBounds  (rightX - exportW, top, exportW, height);
        btnPreview->setBounds (btnExport->getX() - previewW - 6, top, previewW, height);

        // Left cluster
        int x = logoW + 16;

        auto setMenuButton = [&] (IconLabelButton* b, int width)
        {
            if (b == nullptr) return;
            b->setBounds (x, top, width, height);
            x += width + 4;
        };

        setMenuButton (btnNew.get(),           ultraCompact ? 44 : (compact ? 64 : btnW));
        setMenuButton (btnImportSamples.get(), ultraCompact ? 44 : (compact ? 88 : btnW + 16));
        setMenuButton (btnImportBg.get(),      ultraCompact ? 44 : (compact ? 82 : btnW + 16));
        setMenuButton (btnPacks.get(),         ultraCompact ? 44 : (compact ? 72 : btnW + 16));
        setMenuButton (btnDashboard.get(),     ultraCompact ? 44 : (compact ? 90 : btnW + 16));
        setMenuButton (btnProjects.get(),      ultraCompact ? 44 : (compact ? 78 : btnW + 12));
#if PATCHCRAFT_ENABLE_AI_STUDIO
        setMenuButton (btnAiAssist.get(),      ultraCompact ? 44 : (compact ? 82 : btnW + 16));
#endif
    }

} // namespace patchcraft
