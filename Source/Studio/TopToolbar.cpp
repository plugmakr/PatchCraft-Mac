#include "TopToolbar.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

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
            g.setColour (col);
            g.fillRoundedRectangle (r, 6.0f);
        }
        else if (over || down)
        {
            g.setColour (PatchCraftLookAndFeel::raised().brighter (0.1f));
            g.fillRoundedRectangle (r, 6.0f);
        }

        const auto col = accent
            ? juce::Colour (0xff15110b)
            : (over ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());

        auto iconArea = r.withTrimmedBottom (16.0f).reduced (10.0f);
        drawIcon (g, iconArea, iconKey, col);

        g.setColour (col);
        g.setFont (juce::Font (11.0f, juce::Font::plain));
        g.drawText (label, getLocalBounds().removeFromBottom (18),
                    juce::Justification::centred);
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

        addBtn (btnNew,           "New",            "new",            [this] { owner.newProject(); });
        addBtn (btnOpen,          "Open",           "open",           [this] { owner.openProject(); });
        addBtn (btnSave,          "Save",           "save",
            [this]
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Save Project");
                menu.addItem (2, "Save Project As...");
                menu.showMenuAsync (juce::PopupMenu::Options(),
                    [this] (int result)
                    {
                        if (result == 1) owner.saveProject();
                        if (result == 2) owner.saveProjectAs();
                    });
            });
        addBtn (btnImportSamples, "Import Samples", "importSamples",  [this] { owner.importSamples(); });
        addBtn (btnImportBg,      "Import BG",      "importBg",       [this] { owner.importBackground(); });
        addBtn (btnAiAssist,      "AI Assist",      "aiAssist",       [this] { owner.aiAssist(); });
        addBtn (btnPreview,       "Preview",        "preview",        [this] { owner.togglePreview(); }, true);
        addBtn (btnExport,        "Export Pack",    "export",
            [this]
            {
                juce::PopupMenu menu;
                menu.addItem (1, "Export Pack...");
                menu.addItem (2, "Send to Expansion Pack...");
                menu.showMenuAsync (juce::PopupMenu::Options(),
                    [this] (int result)
                    {
                        if (result == 1) owner.exportPack();
                        if (result == 2) owner.sendToExpansionPack();
                    });
            }, true);

        btnSave->setTooltip ("Save the project folder, or choose Save Project As to fork/name a new editable instrument.");
        btnExport->setTooltip ("Export a playable .patchcraft pack, or send it into an expansion-pack folder.");

        projectNameLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        projectNameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        projectNameLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (projectNameLabel);

        projectStatusLabel.setFont (juce::Font (11.0f));
        projectStatusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        projectStatusLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (projectStatusLabel);

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

    void TopToolbar::setPreviewActive (bool active)
    {
        previewActive = active;
        btnPreview->label = active ? "Stop" : "Preview";
        repaint();
    }

    void TopToolbar::paint (juce::Graphics& g)
    {
        // Background gradient bar
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient grad (juce::Colour (0xff141618), 0.0f, 0.0f,
                                   juce::Colour (0xff0c0d10), 0.0f, r.getHeight(), false);
        g.setGradientFill (grad);
        g.fillAll();

        // Bottom border line
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        // PATCHCRAFT logo block on the far left
        const int logoW = 280;
        auto logoArea = juce::Rectangle<int> (0, 0, logoW, getHeight()).toFloat();
        g.setColour (juce::Colour (0xff181a1d));
        g.fillRect (logoArea);
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (logoArea.removeFromRight (1.0f));

        // hex glyph
        auto hexBox = juce::Rectangle<float> (16, 14, 44, 44);
        PatchCraftLookAndFeel::drawHexLogo (g, hexBox);

        // Wordmark
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (22.0f, juce::Font::bold));
        g.drawText ("PATCHCRAFT", 72, 12, 200, 26, juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f, juce::Font::plain));
        g.drawText ("INSTRUMENT BUILDER", 72, 38, 200, 16, juce::Justification::centredLeft);

        // Subtle separators between button groups
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.5f));
        if (btnSave && btnImportSamples)
        {
            int sep = (btnSave->getRight() + btnImportSamples->getX()) / 2;
            g.fillRect (sep, 12, 1, getHeight() - 24);
        }
    }

    void TopToolbar::resized()
    {
        const int btnW = 84;
        const int top  = 6;
        const int height = getHeight() - 12;

        int x = 296; // after logo block
        for (auto* b : { btnNew.get(), btnOpen.get(), btnSave.get() })
        {
            b->setBounds (x, top, btnW, height);
            x += btnW + 4;
        }
        x += 12; // separator
        for (auto* b : { btnImportSamples.get(), btnImportBg.get(), btnAiAssist.get() })
        {
            b->setBounds (x, top, btnW + 16, height);
            x += btnW + 20;
        }

        // right cluster
        int rightX = getWidth() - 8;
        settingsBtn.setBounds (rightX - 70, (getHeight() - 26) / 2, 70, 26);
        rightX = settingsBtn.getX() - 12;

        projectNameLabel.setBounds (rightX - 240, 8, 240, 22);
        projectStatusLabel.setBounds (rightX - 240, 30, 240, 18);

        rightX = projectNameLabel.getX() - 16;
        btnExport->setBounds  (rightX - btnW - 20, top, btnW + 20, height);
        btnPreview->setBounds (btnExport->getX() - btnW - 8, top, btnW, height);
    }

} // namespace patchcraft
