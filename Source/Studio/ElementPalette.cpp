#include "ElementPalette.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    static void drawPaletteIcon (juce::Graphics& g, juce::Rectangle<float> r,
                                 const juce::String& key, juce::Colour col)
    {
        g.setColour (col);
        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY();
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.85f;
        const float t = juce::jmax (1.2f, s * 0.10f);

        if (key == "knob")
        {
            g.drawEllipse (cx - s * 0.45f, cy - s * 0.45f, s * 0.9f, s * 0.9f, t);
            g.drawLine (cx, cy, cx, cy - s * 0.35f, t);
        }
        else if (key == "slider")
        {
            g.drawLine (cx - s * 0.45f, cy + s * 0.30f, cx + s * 0.45f, cy + s * 0.30f, t);
            g.drawLine (cx - s * 0.10f, cy - s * 0.40f, cx - s * 0.10f, cy + s * 0.30f, t);
            g.drawLine (cx + s * 0.10f, cy - s * 0.40f, cx + s * 0.10f, cy + s * 0.30f, t);
        }
        else if (key == "button")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.20f, s * 0.9f, s * 0.40f, 3.0f, t);
        }
        else if (key == "toggle")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.18f, s * 0.9f, s * 0.36f, s * 0.18f, t);
            g.fillEllipse (cx + s * 0.05f, cy - s * 0.16f, s * 0.30f, s * 0.32f);
        }
        else if (key == "dropdown")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.20f, s * 0.9f, s * 0.40f, 3.0f, t);
            juce::Path tri; tri.addTriangle (cx + s * 0.20f, cy - s * 0.05f,
                                             cx + s * 0.35f, cy - s * 0.05f,
                                             cx + s * 0.275f, cy + s * 0.10f);
            g.fillPath (tri);
        }
        else if (key == "label")
        {
            g.setFont (juce::Font (s * 0.7f, juce::Font::bold));
            g.drawText ("T", r, juce::Justification::centred);
        }
        else if (key == "value")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.25f, s * 0.9f, s * 0.5f, 3.0f, t);
            g.setFont (juce::Font (s * 0.4f, juce::Font::bold));
            g.drawText ("123", juce::Rectangle<float> (cx - s * 0.45f, cy - s * 0.25f, s * 0.9f, s * 0.5f),
                        juce::Justification::centred);
        }
        else if (key == "meter")
        {
            for (int i = 0; i < 4; ++i)
            {
                g.drawRect (cx - s * 0.40f + i * (s * 0.22f), cy - s * 0.30f, s * 0.16f, s * 0.60f, t);
            }
        }
        else if (key == "waveform")
        {
            juce::Path p;
            p.startNewSubPath (cx - s * 0.45f, cy);
            for (int i = 0; i < 16; ++i)
            {
                const float x = cx - s * 0.45f + i * (s * 0.9f / 15.0f);
                const float y = cy + std::sin (i * 0.9f) * s * 0.30f;
                p.lineTo (x, y);
            }
            g.strokePath (p, juce::PathStrokeType (t));
        }
        else if (key == "eq")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, 3.0f, t);
            juce::Path p;
            p.startNewSubPath (cx - s * 0.38f, cy + s * 0.12f);
            p.cubicTo (cx - s * 0.16f, cy + s * 0.10f,
                       cx - s * 0.12f, cy - s * 0.30f,
                       cx + s * 0.04f, cy - s * 0.06f);
            p.cubicTo (cx + s * 0.18f, cy + s * 0.16f,
                       cx + s * 0.28f, cy - s * 0.22f,
                       cx + s * 0.40f, cy - s * 0.02f);
            g.strokePath (p, juce::PathStrokeType (t));
            g.fillEllipse (cx - s * 0.06f, cy - s * 0.12f, s * 0.12f, s * 0.12f);
        }
        else if (key == "spectrum")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, 3.0f, t);
            for (int i = 0; i < 8; ++i)
            {
                const float h = s * (0.14f + 0.34f * std::abs (std::sin ((float) i * 0.9f)));
                g.fillRect (cx - s * 0.36f + (float) i * s * 0.10f,
                            cy + s * 0.24f - h,
                            s * 0.055f,
                            h);
            }
        }
        else if (key == "adsr")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, 3.0f, t);
            juce::Path p;
            p.startNewSubPath (cx - s * 0.38f, cy + s * 0.22f);
            p.lineTo (cx - s * 0.22f, cy - s * 0.22f); // attack
            p.lineTo (cx - s * 0.08f, cy + s * 0.02f); // decay
            p.lineTo (cx + s * 0.12f, cy + s * 0.02f); // sustain
            p.lineTo (cx + s * 0.36f, cy + s * 0.22f); // release
            g.strokePath (p, juce::PathStrokeType (t));
        }
        else if (key == "keyboard")
        {
            g.drawRect (cx - s * 0.45f, cy - s * 0.25f, s * 0.9f, s * 0.5f, t);
            for (int i = 1; i < 6; ++i)
                g.drawLine (cx - s * 0.45f + i * (s * 0.15f), cy - s * 0.25f,
                            cx - s * 0.45f + i * (s * 0.15f), cy + s * 0.25f, t);
        }
        else if (key == "panel")
        {
            g.drawRoundedRectangle (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, 3.0f, t);
        }
        else if (key == "shape")
        {
            juce::Path p;
            p.addRoundedRectangle (cx - s * 0.40f, cy - s * 0.28f, s * 0.8f, s * 0.56f, 6.0f);
            g.strokePath (p, juce::PathStrokeType (t));
            g.drawLine (cx - s * 0.30f, cy + s * 0.18f, cx + s * 0.30f, cy - s * 0.18f, t);
        }
        else if (key == "image")
        {
            g.drawRect (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, t);
            g.fillEllipse (cx - s * 0.20f, cy - s * 0.20f, s * 0.16f, s * 0.16f);
            juce::Path p;
            p.startNewSubPath (cx - s * 0.40f, cy + s * 0.20f);
            p.lineTo (cx - s * 0.10f, cy - s * 0.10f);
            p.lineTo (cx + s * 0.20f, cy + s * 0.20f);
            g.strokePath (p, juce::PathStrokeType (t));
        }
        else if (key == "reactive")
        {
            g.drawEllipse (cx - s * 0.36f, cy - s * 0.36f, s * 0.72f, s * 0.72f, t);
            g.drawEllipse (cx - s * 0.22f, cy - s * 0.22f, s * 0.44f, s * 0.44f, t);
            g.fillEllipse (cx - s * 0.06f, cy - s * 0.06f, s * 0.12f, s * 0.12f);
        }
        else if (key == "sprite")
        {
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 3; ++x)
                    g.drawRoundedRectangle (cx - s * 0.42f + (float) x * s * 0.29f,
                                            cy - s * 0.24f + (float) y * s * 0.27f,
                                            s * 0.22f, s * 0.20f, 2.0f, t);
            g.fillRoundedRectangle (cx - s * 0.40f, cy - s * 0.22f, s * 0.18f, s * 0.16f, 2.0f);
        }
        else if (key == "fx")
        {
            for (int i = 0; i < 4; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 4.0f;
                g.drawLine (cx, cy, cx + std::cos (a) * s * 0.42f, cy + std::sin (a) * s * 0.42f, t);
                g.fillEllipse (cx + std::cos (a + 0.45f) * s * 0.27f - s * 0.035f,
                               cy + std::sin (a + 0.45f) * s * 0.27f - s * 0.035f,
                               s * 0.07f, s * 0.07f);
            }
            g.drawEllipse (cx - s * 0.22f, cy - s * 0.22f, s * 0.44f, s * 0.44f, t);
        }
        else if (key == "aiVisual")
        {
            g.drawRoundedRectangle (cx - s * 0.42f, cy - s * 0.28f, s * 0.84f, s * 0.56f, 3.0f, t);
            juce::Path sparkle;
            sparkle.startNewSubPath (cx, cy - s * 0.22f);
            sparkle.lineTo (cx, cy + s * 0.22f);
            sparkle.startNewSubPath (cx - s * 0.22f, cy);
            sparkle.lineTo (cx + s * 0.22f, cy);
            sparkle.startNewSubPath (cx - s * 0.15f, cy - s * 0.15f);
            sparkle.lineTo (cx + s * 0.15f, cy + s * 0.15f);
            sparkle.startNewSubPath (cx + s * 0.15f, cy - s * 0.15f);
            sparkle.lineTo (cx - s * 0.15f, cy + s * 0.15f);
            g.strokePath (sparkle, juce::PathStrokeType (t));
        }
        else if (key == "xy")
        {
            g.drawRect (cx - s * 0.40f, cy - s * 0.40f, s * 0.8f, s * 0.8f, t);
            g.fillEllipse (cx - s * 0.10f, cy - s * 0.10f, s * 0.20f, s * 0.20f);
        }
        else if (key == "granular")
        {
            g.drawRoundedRectangle (cx - s * 0.44f, cy - s * 0.30f, s * 0.88f, s * 0.60f, 4.0f, t);
            juce::Path stream;
            stream.startNewSubPath (cx - s * 0.34f, cy + s * 0.06f);
            stream.cubicTo (cx - s * 0.12f, cy - s * 0.28f,
                            cx + s * 0.18f, cy + s * 0.30f,
                            cx + s * 0.36f, cy - s * 0.05f);
            g.strokePath (stream, juce::PathStrokeType (t));
            for (int i = 0; i < 6; ++i)
            {
                const float px = cx - s * 0.30f + (float) i * s * 0.12f;
                const float py = cy + std::sin ((float) i * 1.7f) * s * 0.20f;
                g.fillEllipse (px, py, s * 0.07f, s * 0.07f);
            }
        }
        else if (key == "tabs")
        {
            g.drawRect (cx - s * 0.45f, cy - s * 0.10f, s * 0.9f, s * 0.40f, t);
            g.drawRect (cx - s * 0.40f, cy - s * 0.30f, s * 0.30f, s * 0.20f, t);
            g.drawRect (cx - s * 0.05f, cy - s * 0.30f, s * 0.30f, s * 0.20f, t);
        }
        else if (key == "scroll")
        {
            g.drawRect (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, t);
            g.fillRect (cx + s * 0.30f, cy - s * 0.20f, s * 0.10f, s * 0.30f);
        }
        else if (key == "group")
        {
            g.drawRect (cx - s * 0.45f, cy - s * 0.30f, s * 0.9f, s * 0.6f, t);
            g.drawRect (cx - s * 0.30f, cy - s * 0.15f, s * 0.30f, s * 0.30f, t);
            g.drawRect (cx + s * 0.05f, cy - s * 0.15f, s * 0.30f, s * 0.30f, t);
        }
        else if (key == "separator")
        {
            g.drawLine (cx - s * 0.45f, cy, cx + s * 0.45f, cy, t);
        }
        else if (key == "drum")
        {
            g.drawRect (cx - s * 0.4f, cy - s * 0.3f, s * 0.8f, s * 0.6f, s * 0.15f);
            g.fillEllipse (cx, cy - s * 0.1f, s * 0.2f, s * 0.2f);
        }
        else if (key == "grid")
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g.drawRect (cx - s * 0.35f + j * (s * 0.18f), 
               cy - s * 0.25f + i * (s * 0.13f), 
               s * 0.15f, s * 0.1f, s * 0.05f);
        }
        else if (key == "mixer")
        {
            for (int i = 0; i < 4; ++i)
            {
                const float x = cx - s * 0.38f + (float) i * s * 0.25f;
                g.drawLine (x, cy - s * 0.34f, x, cy + s * 0.32f, t);
                g.fillRoundedRectangle (x - s * 0.045f,
                                        cy + ((i % 2 == 0) ? -s * 0.06f : s * 0.10f),
                                        s * 0.09f, s * 0.18f, 2.0f);
            }
        }
        else if (key == "library")
        {
            g.drawRoundedRectangle (cx - s * 0.40f, cy - s * 0.28f, s * 0.80f, s * 0.56f, 4.0f, t);
            for (int i = 0; i < 3; ++i)
                g.drawLine (cx - s * 0.28f + (float) i * s * 0.14f, cy - s * 0.16f,
                            cx - s * 0.28f + (float) i * s * 0.14f, cy + s * 0.16f, t);
        }
    }

    juce::var ElementPalette::makeElementDrag (ElementType type, const juce::String& label,
                                               const juce::String& parameterId)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("patchcraftDragType", "paletteElement");
        object->setProperty ("elementType", (int) type);
        object->setProperty ("parameterId", parameterId);
        object->setProperty ("label", label);
        return juce::var (object);
    }

    juce::var ElementPalette::makeModuleDrag (const juce::String& moduleType, const juce::String& label)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("patchcraftDragType", "paletteModule");
        object->setProperty ("moduleType", moduleType);
        object->setProperty ("label", label);
        return juce::var (object);
    }

    juce::var ElementPalette::makeActionDrag (const juce::String& action, const juce::String& label)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("patchcraftDragType", "paletteAction");
        object->setProperty ("action", action);
        object->setProperty ("label", label);
        return juce::var (object);
    }

    // ---------------------------------------------------------------------
    ElementPalette::Row::Row (juce::String t, juce::String k, juce::var drag, juce::String sub)
        : text (std::move (t)), iconKey (std::move (k)), dragDescription (std::move (drag)), subtitle (std::move (sub))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void ElementPalette::Row::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f, 2.0f);
        if (hover)
        {
            g.setColour (PatchCraftLookAndFeel::raised().brighter (0.10f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.4f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }

        const auto col = hover ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text();
        auto iconArea = r.removeFromLeft (28.0f);
        // center the icon vertically relative to row height
        const float iconH = 24.0f;
        const float iconY = r.getY() + (r.getHeight() - iconH) * 0.5f;
        drawPaletteIcon (g, juce::Rectangle<float> (iconArea.getX(), iconY, iconArea.getWidth(), iconH), iconKey, col);

        if (subtitle.isNotEmpty())
        {
            g.setColour (col);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (text, r.toNearestInt().removeFromTop (18), juce::Justification::bottomLeft, true);

            g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.75f));
            g.setFont (juce::Font (10.0f));
            g.drawText (subtitle, r.toNearestInt(), juce::Justification::topLeft, true);
        }
        else
        {
            g.setColour (col);
            g.setFont (juce::Font (13.0f));
            g.drawText (text, r.toNearestInt(), juce::Justification::centredLeft, true);
        }

        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.75f));
        g.setFont (juce::Font (10.0f));
        g.drawText ("drag", r.toNearestInt().removeFromRight (34), juce::Justification::centredRight);
    }

    void ElementPalette::Row::mouseDrag (const juce::MouseEvent& e)
    {
        if (e.getDistanceFromDragStart() < 6 || dragDescription.isVoid())
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            container->startDragging (dragDescription, this, juce::Image(), true, nullptr, &e.source);
    }

    // ---------------------------------------------------------------------
    ElementPalette::Section::Section (juce::String t) : title (std::move (t)) {}

    void ElementPalette::Section::paint (juce::Graphics& g)
    {
        auto header = getLocalBounds().removeFromTop (24).toFloat().reduced (2.0f, 2.0f);
        g.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.72f));
        g.fillRoundedRectangle (header, 5.0f);
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.85f));
        g.drawRoundedRectangle (header, 5.0f, 1.0f);

        g.setColour (open ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (open ? "v" : ">", 8, 0, 18, 24, juce::Justification::centred);
        g.setColour (open ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim());
        g.drawText (title.toUpperCase(), 28, 0, getWidth() - 36, 24, juce::Justification::centredLeft);
    }

    void ElementPalette::Section::resized()
    {
        const bool filtering = activeFilter.isNotEmpty();
        int y = 24;
        for (auto* row : rows)
        {
            const bool show = filtering ? row->matchesFilter : open;
            row->setVisible (show);
            if (show)
            {
                const int rh = row->getRowHeight();
                row->setBounds (4, y, getWidth() - 8, rh);
                y += rh + 2;
            }
        }
    }

    int ElementPalette::Section::applyFilter (const juce::String& query)
    {
        activeFilter = query;
        int visible = 0;
        for (auto* row : rows)
        {
            row->matchesFilter = query.isEmpty()
                              || row->text.containsIgnoreCase (query)
                              || title.containsIgnoreCase (query);
            if (row->matchesFilter)
                ++visible;
        }
        resized();
        return visible;
    }

    void ElementPalette::Section::mouseUp (const juce::MouseEvent& e)
    {
        if (e.y > 26)
            return;

        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            if (auto* palette = findParentComponentOfClass<ElementPalette>())
                palette->toggleAllSections();
            return;
        }

        open = ! open;
        resized();
        repaint();
        if (onToggle)
            onToggle();
    }

    void ElementPalette::Section::addRow (std::unique_ptr<Row> r)
    {
        addAndMakeVisible (*r);
        rows.add (r.release());
    }

    int ElementPalette::Section::getNeededHeight() const
    {
        if (activeFilter.isNotEmpty())
        {
            int total = 24;
            int visible = 0;
            for (auto* row : rows)
            {
                if (row->matchesFilter)
                {
                    total += row->getRowHeight() + 2;
                    ++visible;
                }
            }
            return visible > 0 ? total + 2 : 0;
        }
        if (! open)
            return 24 + 4;
        int total = 24;
        for (auto* row : rows)
            total += row->getRowHeight() + 2;
        return total + 4;
    }

    // ---------------------------------------------------------------------
    ElementPalette::ElementPalette (StudioMainComponent& o) : owner (o)
    {
        searchBox.setTextToShowWhenEmpty ("Search elements & modules...",
                                          PatchCraftLookAndFeel::textDim());
        searchBox.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::raised());
        searchBox.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
        searchBox.setColour (juce::TextEditor::focusedOutlineColourId, PatchCraftLookAndFeel::accent());
        searchBox.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
        searchBox.setFont (juce::Font (13.0f));
        searchBox.setJustification (juce::Justification::centredLeft);
        searchBox.onTextChange = [this] { applySearchFilter(); };
        searchBox.onEscapeKey   = [this] { searchBox.clear(); applySearchFilter(); };
        addAndMakeVisible (searchBox);

        // Tab Selector Setup — flatTab underline (not accent fill) so selected text stays readable
        for (auto* tab : { &controlsTabBtn, &modulesTabBtn, &startersTabBtn })
        {
            tab->getProperties().set ("flatTab", true);
            tab->getProperties().set ("fontSize", 11.5);
            tab->getProperties().set ("bold", true);
            tab->setClickingTogglesState (false);
            tab->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            tab->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        }

        auto updateTabButtons = [this]
        {
            controlsTabBtn.setToggleState (currentTab == PaletteTab::Controls, juce::dontSendNotification);
            modulesTabBtn.setToggleState (currentTab == PaletteTab::Modules, juce::dontSendNotification);
            startersTabBtn.setToggleState (currentTab == PaletteTab::Starters, juce::dontSendNotification);

            resized();
            repaint();
        };

        controlsTabBtn.onClick = [this, updateTabButtons] { currentTab = PaletteTab::Controls; updateTabButtons(); };
        modulesTabBtn.onClick  = [this, updateTabButtons] { currentTab = PaletteTab::Modules;  updateTabButtons(); };
        startersTabBtn.onClick = [this, updateTabButtons] { currentTab = PaletteTab::Starters; updateTabButtons(); };

        updateTabButtons();

        addAndMakeVisible (controlsTabBtn);
        addAndMakeVisible (modulesTabBtn);
        addAndMakeVisible (startersTabBtn);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&scrollContent, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.setWantsKeyboardFocus (false);

        scrollContent.addAndMakeVisible (controlSection);
        scrollContent.addAndMakeVisible (analysisSection);
        scrollContent.addAndMakeVisible (uiSection);
        scrollContent.addAndMakeVisible (motionSection);
        scrollContent.addAndMakeVisible (performanceSection);
        scrollContent.addAndMakeVisible (containerSection);
        scrollContent.addAndMakeVisible (productStarterSection);
        scrollContent.addAndMakeVisible (synthModuleSection);
        scrollContent.addAndMakeVisible (samplerModuleSection);
        scrollContent.addAndMakeVisible (drumModuleSection);
        scrollContent.addAndMakeVisible (midiModuleSection);
        scrollContent.addAndMakeVisible (eqDynamicsModuleSection);
        scrollContent.addAndMakeVisible (fxModuleSection);
        scrollContent.addAndMakeVisible (outputModuleSection);

        for (auto* section : { &controlSection, &analysisSection, &uiSection, &motionSection, &performanceSection, &containerSection,
                               &productStarterSection,
                               &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
                               &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection })
            section->onToggle = [this] { resized(); repaint(); };

        struct Entry { ElementType t; juce::String label; juce::String icon; juce::String parameterId; };
        const Entry controls[] = {
            { ElementType::Knob,         "Knob",              "knob",     "filterCutoff" },
            { ElementType::Slider,       "Slider",            "slider",   "volume" },
            { ElementType::Toggle,       "Toggle",            "toggle",   "mix" },
            { ElementType::XYPad,        "XY Pad",            "xy",       "xyPosition" },
            { ElementType::MacroControl, "Macro Knob",        "knob",     "macro1" },
            { ElementType::ModMatrix,    "Mod Matrix",        "grid",     {} },
            { ElementType::Mixer,        "Mixer Strip Bank",  "mixer",    {} },
            { ElementType::ValueDisplay, "Live Value Readout","value",    "filterCutoff" },
            { ElementType::Meter,        "Level Meter",       "meter",    "volume" },
            { ElementType::Waveform,     "Sample Waveform",   "waveform", {} },
            { ElementType::Keyboard,     "Virtual Keyboard",  "keyboard", {} },
            { ElementType::GranularField,"Granular Field",   "granular", "sampleStart" }
        };
        const Entry visuals[] = {
            { ElementType::Label,    "Text Label", "label", {} },
            { ElementType::Button,   "Button",     "button", "trigger" },
            { ElementType::Dropdown, "Dropdown",   "dropdown", "oscType" },
            { ElementType::Panel,    "Panel",      "panel", {} },
            { ElementType::Shape,    "Shape",      "shape", {} },
            { ElementType::Image,    "Image",      "image", {} }
        };
        const Entry analysis[] = {
            { ElementType::EqCurve,          "EQ Curve",          "eq", {} },
            { ElementType::SpectrumAnalyzer, "Spectrum Analyzer", "spectrum", {} },
            { ElementType::AdsrCurve,        "ADSR Envelope",     "adsr", {} }
        };
        const Entry motion[] = {
            { ElementType::VisualFxLayer,  "Visual FX Layer", "fx", {} }
        };
        const Entry performance[] = {
            { ElementType::DrumPad,       "Single Drum Pad",     "drum", {} },
            { ElementType::PadGrid,       "MPC Pad Grid",        "grid", {} },
            { ElementType::DrumGrid,      "Drum Pattern Grid",   "grid", {} },
            { ElementType::PianoRoll,     "Piano Roll",          "grid", {} },
            { ElementType::ArpLane,       "Arp Lane View",       "grid", {} },
            { ElementType::SequencerLane, "Step Sequencer Lane","grid", {} }
        };
        const Entry containers[] = {
            { ElementType::TabPanel,    "Tab Panel",    "tabs", {} },
            { ElementType::Separator,   "Separator",    "separator", {} }
        };

        for (const auto& e : controls)
            controlSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));
        for (const auto& e : visuals)
            uiSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));
        for (const auto& e : analysis)
            analysisSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));
        for (const auto& e : motion)
            motionSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));
        for (const auto& e : performance)
            performanceSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));
        for (const auto& e : containers)
            containerSection.addRow (std::make_unique<Row> (e.label, e.icon, makeElementDrag (e.t, e.label, e.parameterId)));

        productStarterSection.addRow (std::make_unique<Row> ("Synth Plugin Starter", "knob",
            makeModuleDrag ("StarterSynthPlugin", "Synth Plugin Starter"), "Standard Subtractive Synth template"));
        productStarterSection.addRow (std::make_unique<Row> ("Sampler Instrument Starter", "waveform",
            makeModuleDrag ("StarterSamplerInstrument", "Sampler Instrument Starter"), "Multi-mic and pitch-mapped sample engine"));
        productStarterSection.addRow (std::make_unique<Row> ("Easy Sampler Workstation", "waveform",
            makeModuleDrag ("StarterEasySamplerWorkstation", "Easy Sampler Workstation"), "Single drop zone with automatic mapping"));
        productStarterSection.addRow (std::make_unique<Row> ("Vocal Chop Instrument Starter", "grid",
            makeModuleDrag ("StarterVocalChopInstrument", "Vocal Chop Instrument Starter"), "Slice playback & chord triggering"));
        productStarterSection.addRow (std::make_unique<Row> ("Beat Editing Sampler Starter", "grid",
            makeModuleDrag ("StarterBeatEditingSampler", "Beat Editing Sampler Starter"), "Granular beat slicer and loop tempo sync"));
        productStarterSection.addRow (std::make_unique<Row> ("Drum Machine Starter", "drum",
            makeModuleDrag ("StarterDrumMachine", "Drum Machine Starter"), "MPC-style 16 pad kit layout"));
        productStarterSection.addRow (std::make_unique<Row> ("Scratch / Slice Starter", "xy",
            makeModuleDrag ("StarterScratchSlice", "Scratch / Slice Starter"), "Realtime vinyl scratch deck controls"));
        productStarterSection.addRow (std::make_unique<Row> ("Hip Hop Sampler Starter", "drum",
            makeModuleDrag ("StarterHipHopSampler", "Hip Hop Sampler Starter"), "Warm saturation, filter & transient shaper"));
        productStarterSection.addRow (std::make_unique<Row> ("Chop Lab Starter", "grid",
            makeModuleDrag ("StarterChopLab", "Chop Lab Starter"), "Creative loop chopper layout"));
        productStarterSection.addRow (std::make_unique<Row> ("MPC Pad Instrument Starter", "drum",
            makeModuleDrag ("StarterMpcPads", "MPC Pad Instrument Starter"), "16 pad bank linked to sample triggers"));
        productStarterSection.addRow (std::make_unique<Row> ("Loop Remix FX Starter", "fx",
            makeModuleDrag ("StarterLoopRemixFx", "Loop Remix FX Starter"), "Multi-band delay, chorus and tape delay"));
        productStarterSection.addRow (std::make_unique<Row> ("Chord Progression Plugin Starter", "grid",
            makeModuleDrag ("StarterChordProgressionPlugin", "Chord Progression Plugin Starter"), "MIDI Chord progression pads & arpeggiator"));
        productStarterSection.addRow (std::make_unique<Row> ("Delay FX Starter", "fx",
            makeModuleDrag ("StarterDelayFx", "Delay FX Starter"), "Creative multi-tap echo layout"));
        productStarterSection.addRow (std::make_unique<Row> ("Vocal / Master FX Starter", "meter",
            makeModuleDrag ("StarterVocalMasterFx", "Vocal / Master FX Starter"), "Surgical EQ, glue compressor and limiter"));

        synthModuleSection.addRow (std::make_unique<Row> ("OSC Stack", "knob",
            makeModuleDrag ("OscStack", "OSC Stack"), "Multi-oscillator generator with sub & noise"));
        synthModuleSection.addRow (std::make_unique<Row> ("Wavetable Source", "waveform",
            makeModuleDrag ("Wavetable", "Wavetable Source"), "Wavetable oscillator engine"));
        synthModuleSection.addRow (std::make_unique<Row> ("Serum-Style Table", "waveform",
            makeModuleDrag ("SerumTable", "Serum-Style Table"), "Morphing 2D/3D wavetable display"));
        synthModuleSection.addRow (std::make_unique<Row> ("Filter Module", "eq",
            makeModuleDrag ("Filter", "Filter Module"), "Resonant morphing low/high pass filter"));
        synthModuleSection.addRow (std::make_unique<Row> ("ADSR Envelope", "slider",
            makeModuleDrag ("ADSR", "ADSR Envelope"), "Amp ADSR controls with curve visualizer"));

        samplerModuleSection.addRow (std::make_unique<Row> ("Sample Player", "waveform",
            makeModuleDrag ("SamplePlayer", "Sample Player"), "WAV playback engine with start/loop settings"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Record / Drop Zone", "waveform",
            makeModuleDrag ("RecordDropZone", "Record / Drop Zone"), "Drag & drop sample panel with level meter"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Slice / Chop", "grid",
            makeModuleDrag ("SliceChop", "Slice / Chop"), "Grid-based sampler slicing view"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Vocal Chop Pad", "grid",
            makeModuleDrag ("VocalChopPad", "Vocal Chop Pad"), "Interactive pads mapping vocal chops"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Beat Slice Editor", "grid",
            makeModuleDrag ("BeatSliceEditor", "Beat Slice Editor"), "Waveform visualizer with beat grid transient cues"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Scratch Deck", "xy",
            makeModuleDrag ("ScratchDeck", "Scratch Deck"), "XY vinyl deck scratching pad"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Granular Sampler", "granular",
            makeModuleDrag ("GranularSampler", "Granular Sampler"), "Real-time cloud particle synthesis"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Multisample Keymap", "keyboard",
            makeModuleDrag ("MultisampleKeymap", "Multisample Keymap"), "Multisample editor layout"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Chop Grid", "grid",
            makeModuleDrag ("ChopGrid", "Chop Grid"), "MPC pads for sample chop triggering"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Loop Slicer", "grid",
            makeModuleDrag ("LoopSlicer", "Loop Slicer"), "Automated transient sample splitter"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Loop Time Stretch", "waveform",
            makeModuleDrag ("LoopTimeStretch", "Loop Time Stretch"), "BPM sync time stretching engine"));
        samplerModuleSection.addRow (std::make_unique<Row> ("MIDI Loop Player", "grid",
            makeModuleDrag ("MidiLoopPlayer", "MIDI Loop Player"), "MIDI clip playback & export trigger"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Sample FX Strip", "fx",
            makeModuleDrag ("SampleFxStrip", "Sample FX Strip"), "Channel insert effect strip panel"));
        samplerModuleSection.addRow (std::make_unique<Row> ("Vinyl Texture Sampler", "waveform",
            makeModuleDrag ("VinylTextureSampler", "Vinyl Texture Sampler"), "Analog crackle, hiss & wow-flutter simulation"));

        drumModuleSection.addRow (std::make_unique<Row> ("Drum Rack", "drum",
            makeModuleDrag ("DrumRack", "Drum Rack"), "8 pad drum triggers with individual controls"));
        drumModuleSection.addRow (std::make_unique<Row> ("Drum Sequencer", "grid",
            makeModuleDrag ("DrumSequencer", "Drum Sequencer"), "8-step grid pattern step sequencer"));
        drumModuleSection.addRow (std::make_unique<Row> ("Drum Mixer", "mixer",
            makeModuleDrag ("DrumMixer", "Drum Mixer"), "Custom sub-mixer for drum channels"));
        drumModuleSection.addRow (std::make_unique<Row> ("808 Kit Builder", "drum",
            makeModuleDrag ("EightOhEightKit", "808 Kit Builder"), "Pre-patched sub bass and drum kit"));
        drumModuleSection.addRow (std::make_unique<Row> ("Boom Bap Pad Bank", "drum",
            makeModuleDrag ("BoomBapPadBank", "Boom Bap Pad Bank"), "12 pad bank with vinyl crunch filter"));
        drumModuleSection.addRow (std::make_unique<Row> ("Quick Drum Kit", "drum",
            makeModuleDrag ("QuickDrumKit", "Quick Drum Kit"), "Pre-routed drum kit template"));

        midiModuleSection.addRow (std::make_unique<Row> ("Arp Lane Module Rack", "grid",
            makeModuleDrag ("ArpLaneModule", "Arp Lane Module Rack"), "Polyphonic arpeggiator pattern editor"));
        midiModuleSection.addRow (std::make_unique<Row> ("Step Sequencer Module", "grid",
            makeModuleDrag ("SeqSequencerModule", "Step Sequencer Module"), "Step sequencer layout"));
        midiModuleSection.addRow (std::make_unique<Row> ("Harmony Composer", "keyboard",
            makeModuleDrag ("HarmonyComposer", "Harmony Composer"), "Real-time chord voicing & chord progression"));
        midiModuleSection.addRow (std::make_unique<Row> ("Chord Progression Builder", "grid",
            makeModuleDrag ("ChordProgressionBuilder", "Chord Progression Builder"), "Chord bank selector for quick progressions"));
        midiModuleSection.addRow (std::make_unique<Row> ("Scale + Chord Assistant", "keyboard",
            makeModuleDrag ("ScaleChordAssistant", "Scale + Chord Assistant"), "Force MIDI input to active scale"));
        midiModuleSection.addRow (std::make_unique<Row> ("Chord Pad Bank", "drum",
            makeModuleDrag ("ChordPadBank", "Chord Pad Bank"), "16 velocity pads playing rich chords"));
        midiModuleSection.addRow (std::make_unique<Row> ("Voicing + Humanize", "knob",
            makeModuleDrag ("VoicingHumanize", "Voicing + Humanize"), "MIDI velocity, timing drift & humanizing control"));
        midiModuleSection.addRow (std::make_unique<Row> ("LFO Module", "reactive",
            makeModuleDrag ("LFO", "LFO Module"), "LFO modulator panel with rate & depth"));
        midiModuleSection.addRow (std::make_unique<Row> ("Step LFO", "grid",
            makeModuleDrag ("StepLFO", "Step LFO"), "Custom multi-step modulation sequencer"));
        midiModuleSection.addRow (std::make_unique<Row> ("Macro Bank", "knob",
            makeModuleDrag ("MacroBank", "Macro Bank"), "8 global assignable VST macros"));

        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Surgical EQ", "eq",
            makeModuleDrag ("SurgicalEQ", "Surgical EQ"), "4-band surgical EQ with graphic EQ curve"));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Dynamic EQ", "eq",
            makeModuleDrag ("DynamicEQ", "Dynamic EQ"), "Dynamic threshold-activated EQ bands"));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Compressor", "meter",
            makeModuleDrag ("Dynamics", "Compressor"), "Peak / RMS channel compressor"));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Limiter", "meter",
            makeModuleDrag ("Limiter", "Limiter"), "Brickwall output limiter with gain & ceiling"));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Transient Shaper", "waveform",
            makeModuleDrag ("Transient", "Transient Shaper"), "Attack & release transient shaper"));

        fxModuleSection.addRow (std::make_unique<Row> ("Delay Module", "fx",
            makeModuleDrag ("Delay", "Delay Module"), "Sleek stereo feedback delay"));
        fxModuleSection.addRow (std::make_unique<Row> ("MultiTap Delay", "fx",
            makeModuleDrag ("MultiTapDelay", "MultiTap Delay"), "4-tap delay with width and feedback spread"));
        fxModuleSection.addRow (std::make_unique<Row> ("Reverb Module", "fx",
            makeModuleDrag ("Reverb", "Reverb Module"), "Algorithmic space reverb simulator"));
        fxModuleSection.addRow (std::make_unique<Row> ("Chorus Module", "fx",
            makeModuleDrag ("Chorus", "Chorus Module"), "Stereo bucket brigade chorus modulator"));
        fxModuleSection.addRow (std::make_unique<Row> ("Phaser Module", "fx",
            makeModuleDrag ("Phaser", "Phaser Module"), "Classic multi-stage phase shifter"));
        fxModuleSection.addRow (std::make_unique<Row> ("Flanger Module", "fx",
            makeModuleDrag ("Flanger", "Flanger Module"), "Comb filtering chorus flanger effect"));
        synthModuleSection.addRow (std::make_unique<Row> ("Tape Module", "fx",
            makeModuleDrag ("Tape", "Tape Module"), "Tape speed, bias & saturation emulator"));
        fxModuleSection.addRow (std::make_unique<Row> ("Lo-Fi Module", "fx",
            makeModuleDrag ("LoFi", "Lo-Fi Module"), "Bitcrusher, downsampler and analog noise"));
        fxModuleSection.addRow (std::make_unique<Row> ("Vocal FX", "fx",
            makeModuleDrag ("VocalFX", "Vocal FX"), "Pitch correction, formant shifter and voice space"));

        outputModuleSection.addRow (std::make_unique<Row> ("Stereo Module", "mixer",
            makeModuleDrag ("Stereo", "Stereo Module"), "Stereo width, panning and mono-maker control"));
        outputModuleSection.addRow (std::make_unique<Row> ("Master Bus", "meter",
            makeModuleDrag ("MasterBus", "Master Bus"), "Master EQ, compression and limiter stack"));

        performanceSection.addRow (std::make_unique<Row> ("BPM Sync Toggle", "toggle",
            makeElementDrag (ElementType::Toggle, "BPM Sync Toggle", "bpmSync")));
        performanceSection.addRow (std::make_unique<Row> ("Project BPM Knob", "knob",
            makeElementDrag (ElementType::Knob, "Project BPM Knob", "projectBpm")));
        performanceSection.addRow (std::make_unique<Row> ("Retrigger Toggle", "toggle",
            makeElementDrag (ElementType::Toggle, "Retrigger Toggle", "retrigger")));
        performanceSection.addRow (std::make_unique<Row> ("Add Mixer Channel", "mixer",
            makeActionDrag ("mixerChannel", "Add Mixer Channel")));
        performanceSection.addRow (std::make_unique<Row> ("Drum Machine Layout Kit", "grid",
            makeActionDrag ("drumMachineLayout", "Drum Machine Layout Kit")));
        performanceSection.addRow (std::make_unique<Row> ("Runtime Sample Library", "library",
            makeElementDrag (ElementType::RuntimeSampleLibrary, "Runtime Sample Library", {})));

        for (auto* b : { &trashBtn, &copyBtn, &folderBtn })
        {
            b->getProperties().set ("fontSize", 10.5);
            addAndMakeVisible (*b);
        }
        trashBtn.setTooltip ("Delete selected elements");
        copyBtn.setTooltip ("Duplicate selected elements");
        folderBtn.setTooltip ("Create a group from the current selection");
        trashBtn.onClick = [this] { owner.deleteSelected(); };
        copyBtn.onClick = [this] { owner.duplicateSelected(); };
        folderBtn.onClick = [this]
        {
            const auto ids = owner.getSelectedElementIds();
            if (ids.isEmpty())
                return;

            LayoutElement group;
            group.type = ElementType::Group;
            group.label = "New Group";
            group.id = owner.getProject().getLayout().generateUniqueId ("group_");
            group.x = 80;
            group.y = 80;
            group.width = 320;
            group.height = 180;
            group.backgroundColour = juce::Colour (0x33141822);
            group.borderColour = PatchCraftLookAndFeel::accent();

            auto& added = owner.getProject().getLayout().add (group);
            for (const auto& id : ids)
                if (auto* el = owner.getProject().getLayout().find (id))
                    el->containerId = added.id;

            owner.setSelectedElementId (added.id);
            owner.getProject().notifyChanged();
        };

        updateTabButtons();
    }

    void ElementPalette::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        
        // draw a nice bottom border line under the tab buttons
        auto r = getLocalBounds().removeFromTop (34 + 28);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) getWidth());
    }

    void ElementPalette::applySearchFilter()
    {
        const auto query = searchBox.getText().trim();
        for (auto* section : { &controlSection, &analysisSection, &uiSection, &motionSection,
                               &performanceSection, &containerSection, &productStarterSection,
                               &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
                               &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection })
            section->applyFilter (query);

        resized();
        repaint();
    }

    void ElementPalette::setAllSectionsOpen (bool shouldOpen)
    {
        for (auto* section : { &controlSection, &analysisSection, &uiSection, &motionSection,
                               &performanceSection, &containerSection, &productStarterSection,
                               &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
                               &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection })
            section->open = shouldOpen;

        resized();
        repaint();
    }

    void ElementPalette::toggleAllSections()
    {
        bool anyOpen = false;
        for (auto* section : { &controlSection, &analysisSection, &uiSection, &motionSection,
                                 &performanceSection, &containerSection, &productStarterSection,
                                 &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
                                 &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection })
            anyOpen = anyOpen || section->open;

        setAllSectionsOpen (! anyOpen);
    }

    void ElementPalette::resized()
    {
        auto outer = getLocalBounds();
        auto bottom = outer.removeFromBottom (36).reduced (8, 4);
        searchBox.setBounds (outer.removeFromTop (34).reduced (8, 6));

        // tab row
        auto tabArea = outer.removeFromTop (28).reduced (8, 0);
        const int buttonWidth = tabArea.getWidth() / 3;
        controlsTabBtn.setBounds (tabArea.removeFromLeft (buttonWidth).reduced (1, 2));
        modulesTabBtn.setBounds (tabArea.removeFromLeft (buttonWidth).reduced (1, 2));
        startersTabBtn.setBounds (tabArea.reduced (1, 2));

        viewport.setBounds (outer);

        const int contentWidth = juce::jmax (1, viewport.getWidth() - 10);

        Section* ordered[] = {
            &controlSection, &analysisSection, &uiSection, &motionSection,
            &performanceSection, &containerSection, &productStarterSection,
            &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
            &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection
        };

        const auto getTabForSection = [this] (Section* s) -> PaletteTab
        {
            if (s == &productStarterSection)
                return PaletteTab::Starters;
                
            if (s == &synthModuleSection || s == &samplerModuleSection || s == &drumModuleSection
                || s == &midiModuleSection || s == &eqDynamicsModuleSection || s == &fxModuleSection
                || s == &outputModuleSection)
                return PaletteTab::Modules;
                
            return PaletteTab::Controls;
        };

        int contentHeight = 16;
        for (auto* section : ordered)
        {
            if (getTabForSection (section) != currentTab)
            {
                section->setVisible (false);
                continue;
            }
            const int h = section->getNeededHeight();
            if (h > 0)
                contentHeight += h + 8;
        }
        contentHeight += 8;

        scrollContent.setBounds (0, 0, contentWidth, juce::jmax (viewport.getHeight(), contentHeight));

        auto r = scrollContent.getLocalBounds().reduced (8);
        for (auto* section : ordered)
        {
            if (getTabForSection (section) != currentTab)
            {
                section->setVisible (false);
                continue;
            }
            const int h = section->getNeededHeight();
            const bool show = h > 0;
            section->setVisible (show);
            if (! show)
                continue;
            section->setBounds (r.removeFromTop (h));
            r.removeFromTop (8);
        }

        // bottom action icons
        const int iw = bottom.getHeight();
        trashBtn.setBounds  (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        copyBtn.setBounds   (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        folderBtn.setBounds (bottom.removeFromLeft (iw));
    }

} // namespace patchcraft
