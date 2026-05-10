#include "CanvasEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <vector>

namespace patchcraft
{
    static constexpr int kRulerSize = 22;

    static juce::String scopedTabGroupId (const LayoutElement& tabPanel, const juce::String& label)
    {
        if (tabPanel.id == "tabs")
            return LayoutElement::tabLabelToGroupId (label);
        return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
    }

    static bool isScopedTabGroupId (const juce::String& groupId)
    {
        return groupId.contains ("__tab__");
    }

    static bool canvasParameterIsEnabled (const PatchCraftProject& project, const ParameterDef& parameter)
    {
        if (parameter.enabledBy.isEmpty())
            return true;

        const auto* gate = project.getParameters().find (parameter.enabledBy);
        const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
        const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
        return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
    }

    static juce::String canvasControlGuidance (const PatchCraftProject& project, const LayoutElement& element)
    {
        if (element.parameterId.isEmpty())
            return "This control is not assigned to any parameter.\nSelect it, then use Inspector > DSP Assignment/Parameter or drag a DSP Quick Edit parameter onto the canvas.";

        const auto* parameter = project.getParameters().find (element.parameterId);
        if (parameter == nullptr)
            return "This control points to missing parameter '" + element.parameterId + "'.\nReconnect it in the Inspector or replace it by dragging a valid parameter onto the canvas.";

        if (! canvasParameterIsEnabled (project, *parameter))
            return "This control is disabled: "
                + (parameter->enableHint.isNotEmpty() ? parameter->enableHint : ("enable " + parameter->enabledBy + " first."))
                + "\nAfter enabling the source parameter, this knob will move and affect sound.";

        return {};
    }

    CanvasEditor::CanvasEditor (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);
        setWantsKeyboardFocus (true);
        startTimerHz (24);
    }

    void CanvasEditor::setCurrentTabGroup (juce::String groupId)
    {
        currentTabGroup = std::move (groupId);
        activeTabGroupsByPanel["tabs"] = currentTabGroup;
        repaint();
    }

    void CanvasEditor::setGridVisible (bool shouldShow)
    {
        if (showGrid == shouldShow) return;
        showGrid = shouldShow;
        repaint();
    }

    void CanvasEditor::setRulersVisible (bool shouldShow)
    {
        if (showRulers == shouldShow) return;
        showRulers = shouldShow;
        resized();
        repaint();
    }

    bool CanvasEditor::isElementOnCurrentTab (const LayoutElement& e) const
    {
        if (e.groupId.isEmpty()) return true;
        for (const auto& parent : owner.getProject().getLayout().getAll())
        {
            if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                && parent.id == e.groupId)
                return true;
            if (parent.type == ElementType::TabPanel)
            {
                for (const auto& tab : parent.tabs)
                {
                    const auto group = scopedTabGroupId (parent, tab);
                    if (group == e.groupId)
                    {
                        const auto active = activeTabGroupsByPanel.find (parent.id);
                        const auto activeGroup = active != activeTabGroupsByPanel.end()
                            ? active->second
                            : (parent.tabs.isEmpty() ? juce::String() : scopedTabGroupId (parent, parent.tabs[0]));
                        return activeGroup == e.groupId && isElementOnCurrentTab (parent);
                    }
                }
            }
        }
        if (isScopedTabGroupId (e.groupId))
            return false;
        return e.groupId == currentTabGroup;
    }

    juce::Rectangle<int> CanvasEditor::canvasScreenRect() const
    {
        const auto& cs = owner.getProject().getCanvasSize();
        const int w = juce::roundToInt (cs.width  * zoom);
        const int h = juce::roundToInt (cs.height * zoom);
        const int x = (getWidth()  - w) / 2 + (showRulers ? kRulerSize / 2 : 0);
        const int y = (getHeight() - h) / 2 + (showRulers ? kRulerSize / 2 : 0);
        return { x, y, w, h };
    }

    juce::Rectangle<int> CanvasEditor::elementScreenRect (const LayoutElement& e) const
    {
        auto c = canvasScreenRect();
        return juce::Rectangle<int> (
            c.getX() + juce::roundToInt (e.x * zoom),
            c.getY() + juce::roundToInt (e.y * zoom),
            juce::roundToInt (e.width  * zoom),
            juce::roundToInt (e.height * zoom));
    }

    juce::Point<int> CanvasEditor::screenToCanvas (juce::Point<int> p) const
    {
        auto c = canvasScreenRect();
        return { juce::roundToInt ((p.x - c.getX()) / zoom),
                 juce::roundToInt ((p.y - c.getY()) / zoom) };
    }

    void CanvasEditor::fit()
    {
        const auto& cs = owner.getProject().getCanvasSize();
        if (cs.width <= 0 || cs.height <= 0) return;
        const int avail = juce::jmax (200, getWidth() - kRulerSize * 2 - 60);
        const int availV = juce::jmax (200, getHeight() - kRulerSize * 2 - 60);
        zoom = juce::jmin ((float) avail / (float) cs.width,
                           (float) availV / (float) cs.height);
        zoom = juce::jlimit (0.15f, 2.0f, zoom);
        resized();
        repaint();
    }

    // -------------------------------------------------------------------------
    void CanvasEditor::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        if (showRulers) drawRulers (g);

        auto canvas = canvasScreenRect();
        // outer drop shadow
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.6f), 24, {});
        ds.drawForRectangle (g, canvas);

        drawCanvasBackground (g, canvas);

        // Draw elements in z-order (back-to-front).
        const auto& elements = owner.getProject().getLayout().getAll();
        for (auto& e : elements)
        {
            if (! e.visible) continue;
            if (e.type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (e)) continue;
            drawElement (g, e, elementScreenRect (e), owner.isElementSelected (e.id));
        }

        if (mode == DragMode::Marquee && ! marqueeRect.isEmpty())
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.12f));
            g.fillRect (marqueeRect);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (marqueeRect, 1);
        }

        drawSelectionGuides (g);

        if (hoverGuidance.isNotEmpty() && ! hoverGuidanceBounds.isEmpty())
        {
            auto bubble = hoverGuidanceBounds.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.88f));
            g.fillRoundedRectangle (bubble, 7.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.75f));
            g.drawRoundedRectangle (bubble, 7.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.setFont (juce::Font (11.0f));
            g.drawFittedText (hoverGuidance, hoverGuidanceBounds.reduced (10, 7),
                              juce::Justification::centredLeft, 4);
        }
    }

    void CanvasEditor::resized()
    {
    }

    void CanvasEditor::timerCallback()
    {
        // Only repaint when there's actually something animated on the
        // current tab. Skipping the full-canvas repaint when nothing is
        // moving avoids burning a frame's worth of work behind every
        // knob/slider drag event.
        if (mode != DragMode::None)
            return;

        for (const auto& item : owner.getProject().getLayout().getAll())
        {
            if (! item.visible || ! isElementOnCurrentTab (item))
                continue;
            if ((item.animationMode.isNotEmpty() && item.animationMode != "none")
                || item.audioReactive)
            {
                repaint();
                return;
            }
        }
    }

    bool CanvasEditor::isInterestedInDragSource (const SourceDetails& details)
    {
        return details.description.toString().startsWith ("param:");
    }

    void CanvasEditor::itemDropped (const SourceDetails& details)
    {
        const auto descriptor = details.description.toString();
        if (! descriptor.startsWith ("param:"))
            return;

        const auto parameterId = descriptor.fromFirstOccurrenceOf ("param:", false, false).trim();
        if (parameterId.isEmpty())
            return;

        addElementAt (ElementType::Knob, screenToCanvas (details.localPosition), parameterId);
    }

    // ---- Rulers --------------------------------------------------------------
    void CanvasEditor::drawRulers (juce::Graphics& g) const
    {
        auto canvas = canvasScreenRect();

        // Top ruler bar
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRect (0, 0, getWidth(), kRulerSize);
        g.fillRect (0, 0, kRulerSize, getHeight());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, kRulerSize, getWidth(), 1);
        g.fillRect (kRulerSize, 0, 1, getHeight());

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.0f));

        const auto& cs = owner.getProject().getCanvasSize();
        const int step = 100;
        for (int x = 0; x <= cs.width; x += step)
        {
            const int sx = canvas.getX() + juce::roundToInt (x * zoom);
            g.drawLine ((float) sx, (float) kRulerSize - 6,
                        (float) sx, (float) kRulerSize, 1.0f);
            g.drawText (juce::String (x), sx - 18, 2, 36, kRulerSize - 6,
                        juce::Justification::centred);
        }
        for (int y = 0; y <= cs.height; y += step)
        {
            const int sy = canvas.getY() + juce::roundToInt (y * zoom);
            g.drawLine ((float) kRulerSize - 6, (float) sy,
                        (float) kRulerSize, (float) sy, 1.0f);
            g.drawText (juce::String (y), 0, sy - 9, kRulerSize - 8, 18,
                        juce::Justification::centredRight);
        }
    }

    void CanvasEditor::drawCanvasBackground (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        // Outer canvas surface
        g.setColour (juce::Colour (0xff0c0e12));
        g.fillRect (r);

        // Grid
        if (showGrid)
        {
            g.setColour (juce::Colour (0xff15181e));
            const auto step = (float) snapGrid * zoom * 5.0f;
            if (step >= 6.0f)
            {
                for (float x = (float) r.getX(); x < (float) r.getRight(); x += step)
                    g.drawVerticalLine ((int) x, (float) r.getY(), (float) r.getBottom());
                for (float y = (float) r.getY(); y < (float) r.getBottom(); y += step)
                    g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
            }
        }

        // Background image / hero placeholder is drawn by the 'background' layer
        // element. Frame the canvas with a subtle border.
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRect (r, 1);
    }

    // ---- Element rendering --------------------------------------------------
    static void drawHeroArtwork (juce::Graphics& g, juce::Rectangle<int> r)
    {
        // Cinematic mountain hero with EVOLVE wordmark
        juce::ColourGradient grad (juce::Colour (0xff0a0d12), 0.0f, (float) r.getY(),
                                   juce::Colour (0xff1a1a18), 0.0f, (float) r.getBottom(), false);
        grad.addColour (0.45, juce::Colour (0xff5a3a1a));
        grad.addColour (0.60, juce::Colour (0xffc88a3a));
        grad.addColour (0.75, juce::Colour (0xff1a1612));
        g.setGradientFill (grad);
        g.fillRect (r);

        // Sun glow
        const float cx = r.getCentreX();
        const float cy = r.getY() + r.getHeight() * 0.62f;
        juce::ColourGradient glow (juce::Colour (0xfff5d089), cx, cy,
                                   juce::Colours::transparentBlack,
                                   cx, cy - r.getHeight() * 0.3f, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - r.getHeight() * 0.5f, cy - r.getHeight() * 0.5f,
                       r.getHeight() * 1.0f, r.getHeight() * 1.0f);

        // Mountains
        juce::Path m;
        const float baseY = r.getBottom() - r.getHeight() * 0.18f;
        m.startNewSubPath ((float) r.getX(), (float) r.getBottom());
        m.lineTo ((float) r.getX(), baseY);
        const int peaks = juce::jmax (5, r.getWidth() / 100);
        juce::Random rnd (4242);
        for (int i = 0; i <= peaks; ++i)
        {
            float x = juce::jmap ((float) i, 0.0f, (float) peaks,
                                  (float) r.getX(), (float) r.getRight());
            float y = baseY - rnd.nextFloat() * r.getHeight() * 0.18f;
            m.lineTo (x, y);
        }
        m.lineTo ((float) r.getRight(), baseY);
        m.lineTo ((float) r.getRight(), (float) r.getBottom());
        m.closeSubPath();
        g.setColour (juce::Colour (0xff1a1714).withAlpha (0.85f));
        g.fillPath (m);

        // Foreground silhouette
        juce::Path fg;
        const float fgY = r.getBottom() - r.getHeight() * 0.10f;
        fg.startNewSubPath ((float) r.getX(), (float) r.getBottom());
        fg.lineTo ((float) r.getX(), fgY);
        for (int i = 0; i <= peaks * 2; ++i)
        {
            float x = juce::jmap ((float) i, 0.0f, (float) (peaks * 2),
                                  (float) r.getX(), (float) r.getRight());
            float y = fgY + rnd.nextFloat() * r.getHeight() * 0.06f;
            fg.lineTo (x, y);
        }
        fg.lineTo ((float) r.getRight(), (float) r.getBottom());
        fg.closeSubPath();
        g.setColour (juce::Colour (0xff05060a).withAlpha (0.95f));
        g.fillPath (fg);

        // EVOLVE wordmark
        auto title = r.withSizeKeepingCentre (r.getWidth(),
                                              juce::jmax (40, (int) (r.getHeight() * 0.3f)));
        title = title.withY (r.getY() + (int) (r.getHeight() * 0.18f));
        g.setColour (juce::Colour (0xfff7d28d));
        g.setFont (juce::Font (juce::jmax (32.0f, (float) r.getHeight() * 0.18f),
                               juce::Font::bold));
        g.drawText ("EVOLVE", title, juce::Justification::centredTop);

        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (juce::jmax (10.0f, r.getHeight() * 0.038f), juce::Font::plain));
        auto sub = title.translated (0, juce::roundToInt (r.getHeight() * 0.21f));
        g.drawText ("CINEMATIC PAD", sub, juce::Justification::centredTop);
    }

    static void drawControlLabel (juce::Graphics& g, juce::Rectangle<int> r,
                                  const LayoutElement& e, juce::String valueText)
    {
        if (e.labelPosition == "hidden")
            return;

        const auto label = e.label.isNotEmpty() ? e.label : e.parameterId;
        if (label.isEmpty() && valueText.isEmpty())
            return;

        juce::Rectangle<int> labelArea;
        if (e.labelPosition == "top")
            labelArea = r.withY (r.getY() + juce::roundToInt (e.labelOffsetY)).withHeight (juce::jmax (20, r.getHeight() / 5));
        else if (e.labelPosition == "left")
            labelArea = r.withX (r.getX() - juce::roundToInt (48 + e.labelSpacing - e.labelOffsetX)).withWidth (juce::jmax (42, r.getWidth() / 2));
        else if (e.labelPosition == "right")
            labelArea = r.withX (r.getRight() + juce::roundToInt (e.labelSpacing + e.labelOffsetX)).withWidth (juce::jmax (42, r.getWidth() / 2));
        else
            labelArea = r.withTrimmedTop ((int) (r.getHeight() * 0.65f) + juce::roundToInt (e.labelSpacing + e.labelOffsetY))
                         .withHeight ((int) (r.getHeight() * 0.20f));

        labelArea.translate (juce::roundToInt (e.labelOffsetX), 0);
        const auto fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, r.getHeight() * 0.13f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (fontSize, juce::Font::bold));
        g.drawText (label.toUpperCase(), labelArea, juce::Justification::centred, true);

        if (valueText.isNotEmpty())
        {
            auto valueArea = labelArea.translated (0, juce::roundToInt (fontSize + 2.0f));
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (juce::jmax (8.5f, fontSize * 0.85f)));
            g.drawText (valueText, valueArea, juce::Justification::centred, true);
        }
    }

    static void drawCanvasKnob (juce::Graphics& g, juce::Rectangle<int> r,
                                const LayoutElement& e, juce::String valueText,
                                juce::Colour accent, float pos01)
    {
        const float cx = r.getCentreX();
        const float cy = r.getCentreY() - r.getHeight() * 0.12f;
        const float rad = juce::jmin (r.getWidth(), (int) (r.getHeight() * 0.7f)) * 0.5f;

        // outer ring
        const float ringW = juce::jmax (3.0f, rad * 0.13f);
        const float startA = juce::degreesToRadians (-135.0f);
        const float endA   = juce::degreesToRadians ( 135.0f);
        const float pos    = juce::jlimit (0.0f, 1.0f, pos01);

        juce::Path track;
        track.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                             0.0f, startA, endA, true);
        g.setColour (juce::Colour (0xff202227));
        g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        juce::Path active;
        active.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                              0.0f, startA, startA + (endA - startA) * pos, true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // body
        const float br = rad - ringW - 3.0f;
        juce::ColourGradient grad (juce::Colour (0xff262830), cx, cy - br,
                                   juce::Colour (0xff0c0d11), cx, cy + br, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
        g.setColour (juce::Colour (0xff050607));
        g.drawEllipse (cx - br, cy - br, br * 2, br * 2, 1.0f);

        // indicator line
        const float ang = startA + (endA - startA) * pos;
        juce::Path ind;
        ind.addRoundedRectangle (-1.5f, -br * 0.95f, 3.0f, br * 0.55f, 1.5f);
        ind.applyTransform (juce::AffineTransform::rotation (ang).translated (cx, cy));
        g.setColour (accent);
        g.fillPath (ind);

        drawControlLabel (g, r, e, valueText);
    }

    static void drawCanvasVerticalSlider (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e)
    {
        // label on top
        if (e.labelPosition != "hidden")
        {
            auto labelRect = r.removeFromTop (16 + juce::roundToInt (e.labelSpacing));
            labelRect.translate (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (e.labelSize > 0.0f ? e.labelSize : 10.0f, juce::Font::bold));
            g.drawText ((e.label.isNotEmpty() ? e.label : e.parameterId).toUpperCase(), labelRect, juce::Justification::centred);
        }

        const float trackW = juce::jmax (4.0f, r.getWidth() * 0.18f);
        auto track = r.toFloat().withSizeKeepingCentre (trackW, (float) r.getHeight());
        g.setColour (juce::Colour (0xff202227));
        g.fillRoundedRectangle (track, trackW * 0.5f);

        const float thumbY = track.getY() + track.getHeight() * 0.45f;
        auto fill = juce::Rectangle<float> (track.getX(), thumbY,
                                            track.getWidth(),
                                            track.getBottom() - thumbY);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.55f));
        g.fillRoundedRectangle (fill, trackW * 0.5f);

        const float tw = juce::jmin ((float) r.getWidth() - 2, 22.0f);
        const float th = 14.0f;
        auto thumb = juce::Rectangle<float> (r.getCentreX() - tw * 0.5f,
                                             thumbY - th * 0.5f, tw, th);
        juce::ColourGradient grad (juce::Colour (0xff2a2d33), thumb.getX(), thumb.getY(),
                                   juce::Colour (0xff101216), thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (juce::Colour (0xff050608));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
    }

    static void drawCanvasMeter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const bool vertical = r.getHeight() > r.getWidth() * 1.2f;
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        const int segs = vertical ? 16 : 24;
        const auto inner = r.reduced (4);

        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            juce::Colour c = (t < 0.6f) ? juce::Colour (0xff5fb37b)
                            : (t < 0.85f ? juce::Colour (0xffe8b840)
                                         : juce::Colour (0xffe6504a));
            const float alpha = (t < 0.7f) ? 1.0f : 0.85f;

            if (vertical)
            {
                const int sh = juce::jmax (2, inner.getHeight() / segs - 1);
                const int sy = inner.getBottom() - (i + 1) * (sh + 1);
                g.setColour (c.withAlpha (alpha));
                g.fillRect (inner.getX(), sy, inner.getWidth(), sh);
            }
            else
            {
                const int sw = juce::jmax (2, inner.getWidth() / segs - 1);
                const int sx = inner.getX() + i * (sw + 1);
                g.setColour (c.withAlpha (alpha));
                g.fillRect (sx, inner.getY(), sw, inner.getHeight());
            }
        }
    }

    static void drawCanvasKeyboard (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        const int totalKeys = 52; // 8 octaves of white keys
        const float kw = (float) (r.getWidth() - 8) / (float) totalKeys;
        const float keyTop = (float) r.getY() + 4.0f;
        const float keyH   = (float) r.getHeight() - 8.0f;

        // White keys
        for (int i = 0; i < totalKeys; ++i)
        {
            juce::Rectangle<float> key (r.getX() + 4 + i * kw, keyTop, kw - 1.0f, keyH);
            g.setColour (juce::Colour (0xffe9d8b8));
            g.fillRoundedRectangle (key, 1.5f);
            g.setColour (juce::Colour (0xff8a7958));
            g.drawRoundedRectangle (key, 1.5f, 0.5f);
        }
        // Black keys (rough pattern based on octave)
        const float bkH = keyH * 0.62f;
        const float bkW = kw * 0.62f;
        for (int oct = 0; oct < 8; ++oct)
        {
            const int base = oct * 7;
            const int blackOffsets[5] = { 0, 1, 3, 4, 5 };
            for (int b : blackOffsets)
            {
                const float x = r.getX() + 4 + (base + b + 1) * kw - bkW * 0.5f;
                if (x + bkW > r.getRight() - 4) continue;
                juce::Rectangle<float> key (x, keyTop, bkW, bkH);
                g.setColour (juce::Colour (0xff141413));
                g.fillRoundedRectangle (key, 1.5f);
                g.setColour (juce::Colour (0xff050505));
                g.drawRoundedRectangle (key, 1.5f, 0.5f);
            }
        }
    }

    void CanvasEditor::drawSelectionGuides (juce::Graphics& g) const
    {
        const auto& selectedIds = owner.getSelectedElementIds();
        if (selectedIds.size() < 2) return;

        struct Item { juce::Rectangle<int> screen; juce::Rectangle<int> canvas; };
        std::vector<Item> items;
        items.reserve ((size_t) selectedIds.size());
        for (const auto& id : selectedIds)
        {
            if (auto* el = owner.getProject().getLayout().find (id))
                items.push_back ({ elementScreenRect (*el),
                                   { el->x, el->y, el->width, el->height } });
        }
        if (items.size() < 2) return;

        // Selection bounding rect in screen space, used to extend the guide
        // lines past the selection.
        auto bounds = items.front().screen;
        for (auto& it : items) bounds = bounds.getUnion (it.screen);

        const auto guide = PatchCraftLookAndFeel::accent().withAlpha (0.45f);
        const auto guideStrong = PatchCraftLookAndFeel::accent().withAlpha (0.8f);
        const float dashes[] = { 4.0f, 4.0f };

        auto dashedH = [&] (int y)
        {
            g.setColour (guide);
            g.drawDashedLine (juce::Line<float> ((float) bounds.getX() - 24.0f, (float) y,
                                                 (float) bounds.getRight() + 24.0f, (float) y),
                              dashes, 2, 1.0f);
        };
        auto dashedV = [&] (int x)
        {
            g.setColour (guide);
            g.drawDashedLine (juce::Line<float> ((float) x, (float) bounds.getY() - 24.0f,
                                                 (float) x, (float) bounds.getBottom() + 24.0f),
                              dashes, 2, 1.0f);
        };

        // Horizontal alignment guides: top of topmost element, common middle,
        // bottom of bottommost element. These mirror the Align Top / Vertical
        // Middle / Align Bottom buttons.
        int topMin = items.front().screen.getY();
        int botMax = items.front().screen.getBottom();
        for (auto& it : items)
        {
            topMin = juce::jmin (topMin, it.screen.getY());
            botMax = juce::jmax (botMax, it.screen.getBottom());
        }
        const int midY = (topMin + botMax) / 2;
        dashedH (topMin);
        dashedH (midY);
        dashedH (botMax);

        // Vertical alignment guides: left, centre, right edges of selection.
        int leftMin = items.front().screen.getX();
        int rightMax = items.front().screen.getRight();
        for (auto& it : items)
        {
            leftMin = juce::jmin (leftMin, it.screen.getX());
            rightMax = juce::jmax (rightMax, it.screen.getRight());
        }
        const int midX = (leftMin + rightMax) / 2;
        dashedV (leftMin);
        dashedV (midX);
        dashedV (rightMax);

        // Pairwise distance labels showing pixel gap between adjacent
        // elements. Distances use the canvas-space size (the value the user
        // sees in the inspector) — not the on-screen scaled size.
        g.setFont (juce::Font (10.0f, juce::Font::bold));

        // Vertical gaps (sorted top-to-bottom).
        auto sortedY = items;
        std::sort (sortedY.begin(), sortedY.end(),
                   [] (const Item& a, const Item& b) { return a.screen.getY() < b.screen.getY(); });
        for (size_t i = 1; i < sortedY.size(); ++i)
        {
            const int prevBottomScreen = sortedY[i - 1].screen.getBottom();
            const int currTopScreen    = sortedY[i].screen.getY();
            if (currTopScreen <= prevBottomScreen) continue;

            const int prevBottomCanvas = sortedY[i - 1].canvas.getBottom();
            const int currTopCanvas    = sortedY[i].canvas.getY();
            const int gap = currTopCanvas - prevBottomCanvas;

            const int x = juce::jmax (sortedY[i - 1].screen.getRight(),
                                      sortedY[i].screen.getRight()) + 14;
            g.setColour (guideStrong);
            g.drawLine ((float) x, (float) prevBottomScreen, (float) x, (float) currTopScreen, 1.0f);
            g.drawLine ((float) x - 3.0f, (float) prevBottomScreen, (float) x + 3.0f, (float) prevBottomScreen, 1.0f);
            g.drawLine ((float) x - 3.0f, (float) currTopScreen,    (float) x + 3.0f, (float) currTopScreen,    1.0f);
            g.drawText (juce::String (gap) + " px", x + 6,
                        (prevBottomScreen + currTopScreen) / 2 - 8, 70, 16,
                        juce::Justification::centredLeft);
        }

        // Horizontal gaps (sorted left-to-right).
        auto sortedX = items;
        std::sort (sortedX.begin(), sortedX.end(),
                   [] (const Item& a, const Item& b) { return a.screen.getX() < b.screen.getX(); });
        for (size_t i = 1; i < sortedX.size(); ++i)
        {
            const int prevRightScreen = sortedX[i - 1].screen.getRight();
            const int currLeftScreen  = sortedX[i].screen.getX();
            if (currLeftScreen <= prevRightScreen) continue;

            const int prevRightCanvas = sortedX[i - 1].canvas.getRight();
            const int currLeftCanvas  = sortedX[i].canvas.getX();
            const int gap = currLeftCanvas - prevRightCanvas;

            const int y = juce::jmin (sortedX[i - 1].screen.getY(),
                                      sortedX[i].screen.getY()) - 14;
            g.setColour (guideStrong);
            g.drawLine ((float) prevRightScreen, (float) y, (float) currLeftScreen, (float) y, 1.0f);
            g.drawLine ((float) prevRightScreen, (float) y - 3.0f, (float) prevRightScreen, (float) y + 3.0f, 1.0f);
            g.drawLine ((float) currLeftScreen,  (float) y - 3.0f, (float) currLeftScreen,  (float) y + 3.0f, 1.0f);
            g.drawText (juce::String (gap) + " px",
                        (prevRightScreen + currLeftScreen) / 2 - 20, y - 18, 60, 16,
                        juce::Justification::centred);
        }
    }

    void CanvasEditor::drawElement (juce::Graphics& g, const LayoutElement& e,
                                    juce::Rectangle<int> r, bool selected) const
    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity));

        if ((e.animationMode.isNotEmpty() && e.animationMode != "none") || e.audioReactive)
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float rate = juce::jmax (0.05f, e.animationRate);
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi * rate) * 0.5) + 0.5);
            const float animationAmount = e.animationMode == "none" ? 0.0f : wave;
            const float reactiveAmount = e.audioReactive ? juce::jmax (0.08f, e.audioReactiveAmount) * 0.55f : 0.0f;
            const float combined = juce::jlimit (0.0f, 1.0f, animationAmount * 0.8f + reactiveAmount);

            if (e.animationMode == "shake")
            {
                const int dx = juce::roundToInt ((wave - 0.5f) * 10.0f * juce::jmax (0.25f, e.audioReactiveAmount));
                r.translate (dx, 0);
            }
            else if (combined > 0.001f)
            {
                const int grow = juce::roundToInt (combined * 8.0f);
                r = r.expanded (grow, grow);
            }

            if (e.animationMode == "glow" || e.audioReactive)
            {
                auto halo = r.expanded (juce::roundToInt (6.0f + combined * 16.0f)).toFloat();
                g.setColour ((e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour)
                             .withAlpha (juce::jlimit (0.05f, 0.28f, 0.08f + combined * 0.20f)));
                g.fillRoundedRectangle (halo, juce::jmax (4.0f, e.cornerRadius + 8.0f));
            }
        }

        if (e.blurAmount > 0.001f && e.type != ElementType::Image)
        {
            const int blurPx = juce::roundToInt (e.blurAmount * 14.0f);
            for (int i = 3; i >= 1; --i)
            {
                const float alpha = e.blurAmount * 0.035f * (float) i;
                g.setColour ((e.backgroundColour.isTransparent() ? e.accentColour : e.backgroundColour).withAlpha (alpha));
                g.fillRoundedRectangle (r.expanded (blurPx * i / 3).toFloat(),
                                        juce::jmax (4.0f, e.cornerRadius + (float) blurPx));
            }
        }

        // ---- Image element ------------------------------------------------
        // 'background' (id == "background") falls back to the procedural hero
        //   artwork when no asset is set.
        // 'hero' or any other Image with an empty asset draws an "Artwork"
        //   placeholder so the user knows where to drop a PNG.
        // Any Image with an asset path loads + draws the file.
        if (e.type == ElementType::Image)
        {
            // Resolve the asset path: explicit asset wins; for the special
            // 'background' element, fall back to project.backgroundImageRelative.
            juce::String relPath = e.asset;
            if (relPath.isEmpty() && e.id == "background")
                relPath = owner.getProject().backgroundImageRelative;

            juce::Image img;
            if (relPath.isNotEmpty())
            {
                juce::File f = juce::File::isAbsolutePath (relPath)
                    ? juce::File (relPath)
                    : owner.getProject().getProjectFolder().getChildFile (relPath);
                if (f.existsAsFile())
                    img = owner.getAssets().loadImage (f);
            }

            if (img.isValid())
            {
                g.drawImage (img, r.toFloat());
            }
            else if (e.id == "background")
            {
                drawHeroArtwork (g, r);
            }
            else
            {
                // "Drop artwork here" placeholder
                g.setColour (juce::Colour (0xff141618));
                g.fillRoundedRectangle (r.toFloat(), 6.0f);
                g.setColour (PatchCraftLookAndFeel::border());
                g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);

                // Frame icon
                const float cx = r.getCentreX(), cy = r.getCentreY() - r.getHeight() * 0.06f;
                const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.35f;
                g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.6f));
                g.drawRoundedRectangle (cx - s * 0.5f, cy - s * 0.5f, s, s, 4.0f, 1.5f);
                g.fillEllipse (cx - s * 0.18f, cy - s * 0.20f, s * 0.16f, s * 0.16f);
                juce::Path mountains;
                mountains.startNewSubPath (cx - s * 0.45f, cy + s * 0.40f);
                mountains.lineTo (cx - s * 0.10f, cy);
                mountains.lineTo (cx + s * 0.10f, cy + s * 0.20f);
                mountains.lineTo (cx + s * 0.30f, cy - s * 0.10f);
                mountains.lineTo (cx + s * 0.50f, cy + s * 0.40f);
                g.strokePath (mountains, juce::PathStrokeType (1.5f));

                // Caption
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (juce::jmax (12.0f, r.getHeight() * 0.06f),
                                       juce::Font::bold));
                const auto label = e.label.isNotEmpty() ? e.label : juce::String ("Artwork");
                g.drawText (label.toUpperCase(),
                            r.withTrimmedTop ((int) (r.getHeight() * 0.55f))
                             .withHeight ((int) (r.getHeight() * 0.10f)),
                            juce::Justification::centred);
                g.setFont (juce::Font (juce::jmax (10.0f, r.getHeight() * 0.045f)));
                g.drawText ("Click to select, then use Inspector -> Asset to load a PNG",
                            r.withTrimmedTop ((int) (r.getHeight() * 0.66f))
                             .withHeight ((int) (r.getHeight() * 0.10f)),
                            juce::Justification::centred);
            }
        }
        else if (e.type == ElementType::Knob)
        {
            const auto* p = owner.getProject().getParameters().find (e.parameterId);
            juce::String value = "-";
            float pos01 = 0.5f;
            if (p != nullptr)
            {
                const float live = owner.getProject().getLiveValues()
                                        .getValue (p->id, p->defaultValue);
                pos01 = (p->max > p->min) ? (live - p->min) / (p->max - p->min) : 0.0f;

                if (p->unit == "Hz" && live >= 1000.0f)
                    value = juce::String (live / 1000.0f, 1) + " kHz";
                else if (p->unit == "Hz")
                    value = juce::String (live, 0) + " Hz";
                else if (p->unit == "%")
                    value = juce::String (juce::roundToInt (live * 100.0f)) + " %";
                else if (p->unit == "s")
                    value = juce::String (live, 2) + " s";
                else if (p->unit.isNotEmpty())
                    value = juce::String (live, 2) + " " + p->unit;
                else
                    value = juce::String (live, 2);
            }

            // Filmstrip override - load PNG and draw frame.
            if (e.filmstripAsset.isNotEmpty())
            {
                juce::File f (juce::File::isAbsolutePath (e.filmstripAsset)
                                ? e.filmstripAsset
                                : owner.getProject().getProjectFolder()
                                       .getChildFile (e.filmstripAsset).getFullPathName());
                if (auto img = owner.getAssets().loadImage (f); img.isValid())
                {
                    int frames = e.filmstripFrames;
                    if (frames <= 0)
                        frames = PatchCraftLookAndFeel::detectFilmstripFrames (img, e.filmstripVertical);
                    auto stripRect = r.withTrimmedBottom (juce::roundToInt (r.getHeight() * 0.30f));
                    PatchCraftLookAndFeel::drawFilmstripFrame (
                        g, stripRect, img, frames, pos01, e.filmstripVertical);

                    drawControlLabel (g, r, e, value);
                }
                else
                {
                    drawCanvasKnob (g, r, e, value, e.accentColour, pos01);
                }
            }
            else
            {
                drawCanvasKnob (g, r, e, value, e.accentColour, pos01);
            }
        }
        else if (e.type == ElementType::Slider)
        {
            drawCanvasVerticalSlider (g, r, e);
        }
        else if (e.type == ElementType::Toggle)
        {
            const auto* def = owner.getProject().getParameters().find (e.parameterId);
            const auto value = owner.getProject().getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
            const bool on = value >= 0.5f;
            auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 92), juce::jmin (r.getHeight(), 38)).toFloat();
            g.setColour (on ? e.accentColour.withAlpha (0.85f) : juce::Colour (0xff202329));
            g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
            g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
            g.drawRoundedRectangle (toggle, toggle.getHeight() * 0.5f, 1.0f);

            const float knobSize = toggle.getHeight() - 8.0f;
            const float knobX = on ? toggle.getRight() - knobSize - 4.0f : toggle.getX() + 4.0f;
            g.setColour (PatchCraftLookAndFeel::textBright());
            g.fillEllipse (knobX, toggle.getY() + 4.0f, knobSize, knobSize);
            drawControlLabel (g, r, e, on ? "ON" : "OFF");
        }
        else if (e.type == ElementType::Meter)
        {
            drawCanvasMeter (g, r);
        }
        else if (e.type == ElementType::Keyboard)
        {
            drawCanvasKeyboard (g, r);
        }
        else if (e.type == ElementType::Label)
        {
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (juce::jmax (12.0f, (float) r.getHeight() * 0.5f),
                                   juce::Font::bold));
            g.drawText (e.label, r, juce::Justification::centredLeft);
        }
        else if (e.type == ElementType::Dropdown)
        {
            g.setColour (juce::Colour (0xff181a1e));
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            auto rr = r.reduced (28, 0);
            g.drawText ("Deep Horizon", rr, juce::Justification::centred);

            // arrows
            g.setColour (PatchCraftLookAndFeel::textDim());
            const float cy = r.getCentreY();
            juce::Path l; l.addTriangle ((float) r.getX() + 14.0f, cy - 5.0f,
                                          (float) r.getX() + 14.0f, cy + 5.0f,
                                          (float) r.getX() + 8.0f, cy);
            juce::Path rArr; rArr.addTriangle ((float) r.getRight() - 14.0f, cy - 5.0f,
                                               (float) r.getRight() - 14.0f, cy + 5.0f,
                                               (float) r.getRight() - 8.0f, cy);
            g.fillPath (l); g.fillPath (rArr);
        }
        else if (e.type == ElementType::Shape)
        {
            auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 2.0f);
            if (e.shadowAmount > 0.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                g.fillRoundedRectangle (shapeBounds.translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount),
                                        juce::jmax (0.0f, e.cornerRadius));
            }
            if (e.glowAmount > 0.0f)
            {
                g.setColour (e.accentColour.withAlpha (0.18f * e.glowAmount));
                g.fillRoundedRectangle (shapeBounds.expanded (8.0f * e.glowAmount),
                                        juce::jmax (0.0f, e.cornerRadius + 8.0f * e.glowAmount));
            }

            juce::Path path;
            if (e.shapeKind == "ellipse")
                path.addEllipse (shapeBounds);
            else if (e.shapeKind == "triangle")
                path.addTriangle (shapeBounds.getCentreX(), shapeBounds.getY(),
                                  shapeBounds.getRight(), shapeBounds.getBottom(),
                                  shapeBounds.getX(), shapeBounds.getBottom());
            else if (e.shapeKind == "diamond")
            {
                path.startNewSubPath (shapeBounds.getCentreX(), shapeBounds.getY());
                path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
                path.lineTo (shapeBounds.getCentreX(), shapeBounds.getBottom());
                path.lineTo (shapeBounds.getX(), shapeBounds.getCentreY());
                path.closeSubPath();
            }
            else if (e.shapeKind == "line")
            {
                path.startNewSubPath (shapeBounds.getX(), shapeBounds.getCentreY());
                path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
            }
            else
            {
                path.addRoundedRectangle (shapeBounds, juce::jmax (0.0f, e.cornerRadius));
            }

            if (e.shapeKind != "line")
            {
                g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0x33141822) : e.backgroundColour);
                g.fillPath (path);
            }
            g.setColour (e.borderColour);
            g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));

            if (e.audioReactive)
            {
                g.setColour (e.accentColour.withAlpha (0.70f));
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText ("AUDIO", r.reduced (6), juce::Justification::bottomRight);
            }
        }
        else if (e.type == ElementType::Panel)
        {
            const auto radius = juce::jmax (0.0f, e.cornerRadius);
            if (e.shadowAmount > 0.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                g.fillRoundedRectangle (r.toFloat().reduced (1.0f).translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount), radius);
            }
            g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour);
            g.fillRoundedRectangle (r.toFloat(), radius);
            g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
            g.drawRoundedRectangle (r.toFloat(), radius, juce::jmax (0.5f, e.strokeWidth));
            if (e.label.isNotEmpty())
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (11.0f));
                g.drawText (e.label, r.reduced (8, 4), juce::Justification::topLeft);
            }
        }
        else if (e.type == ElementType::Button)
        {
            // Static label-style button; useful as a chrome decoration. Real
            // tab strips use ElementType::TabPanel.
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.toUpperCase(), r, juce::Justification::centred);
        }
        else if (e.type == ElementType::TabPanel)
        {
            drawTabPanel (g, e, r, selected);
        }
        else if (e.type == ElementType::DrumPad || e.type == ElementType::PadGrid)
        {
            const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
            const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
            const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
            const auto inner = r.reduced (e.type == ElementType::PadGrid ? 4 : 0);
            const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
            const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
            const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff1a1d23) : e.backgroundColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            for (int row = 0; row < rows; ++row)
            {
                for (int col = 0; col < cols; ++col)
                {
                    juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                                (float) inner.getY() + row * (padH + gap),
                                                padW, padH);
                    g.setColour (bg.brighter (0.05f));
                    g.fillRoundedRectangle (pad, 4.0f);
                    g.setColour (accent.withAlpha (0.55f));
                    g.drawRoundedRectangle (pad.reduced (0.5f), 4.0f, 1.0f);

                    g.setColour (PatchCraftLookAndFeel::text().withAlpha (0.85f));
                    g.setFont (juce::Font (juce::jmin (12.0f, padH * 0.28f), juce::Font::bold));
                    const int padIdx = row * cols + col;
                    const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                    juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                        ? e.label : juce::String (padIdx + 1);
                    g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f),
                                juce::Justification::centred);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::Font (juce::jmin (10.0f, padH * 0.22f)));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                                pad.reduced (4.0f).removeFromBottom (padH * 0.35f),
                                juce::Justification::centred);
                }
            }
        }
        else
        {
            g.setColour (juce::Colour (0xff15171b));
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (11.0f));
            g.drawText (elementTypeDisplayName (e.type),
                        r, juce::Justification::centred);
        }

        // Selection outline
        g.setOpacity (1.0f);
        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (r.expanded (1), 1);
            // resize handles
            const int hs = 6;
            for (auto p : { r.getTopLeft(), r.getTopRight(),
                            r.getBottomLeft(), r.getBottomRight() })
            {
                g.fillRect (p.x - hs / 2, p.y - hs / 2, hs, hs);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Mouse interaction
    //
    // Click priority:
    //   1. BR corner of *selected* element → resize.
    //   2. Inside the control body of a knob/slider bound to a parameter →
    //      value drag (vertical drag changes the live parameter value).
    //      This works whether the element is currently selected or not, so
    //      the canvas behaves like a real plug-in.
    //   3. Otherwise click selects + starts a move drag.
    // -------------------------------------------------------------------------
    bool CanvasEditor::hitTestControlBody (const LayoutElement& el,
                                           juce::Rectangle<int> r,
                                           juce::Point<int> p) const
    {
        if (el.parameterId.isEmpty()) return false;
        if (el.type == ElementType::Knob)
        {
            // Use a circle inside r (matching how the knob is drawn).
            const float cx = r.getCentreX();
            const float cy = r.getCentreY() - r.getHeight() * 0.12f;
            const float rad = juce::jmin (r.getWidth(), (int) (r.getHeight() * 0.7f)) * 0.5f;
            const float dx = p.x - cx, dy = p.y - cy;
            return dx * dx + dy * dy <= rad * rad;
        }
        if (el.type == ElementType::Slider)
            return r.contains (p);
        return false;
    }

    void CanvasEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus();
        layoutChangedDuringDrag = false;
        if (e.mods.isPopupMenu())
        {
            showContextMenu (e.getPosition());
            return;
        }
        const auto& elements = owner.getProject().getLayout().getAll();

        // 0. TabPanel tabs: clicking a tab swaps the current page. Done before
        // selection so users don't have to fiddle with edit mode to switch tabs.
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

        // 1. BR-resize on selected element first.
        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            const int hs = 8;
            if (juce::Rectangle<int> (r.getRight() - hs, r.getBottom() - hs, hs * 2, hs * 2)
                .contains (e.getPosition()))
            {
                dragStart    = e.getPosition();
                dragOriginal = *sel;
                mode = DragMode::ResizeBR;
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

            const bool isInteractiveControl = it->type == ElementType::Knob || it->type == ElementType::Slider
                                           || it->type == ElementType::Toggle || it->type == ElementType::Dropdown
                                           || it->type == ElementType::ValueDisplay;
            if (! e.mods.isShiftDown() && owner.getProject().getManifest().playerShowParameterGuidance && isInteractiveControl)
            {
                const bool attemptedControlUse = (it->type == ElementType::Knob || it->type == ElementType::Slider)
                    ? hitTestControlBody (*it, r, e.getPosition())
                    : r.contains (e.getPosition());
                if (attemptedControlUse)
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
                    repaint();
                    return;
                }
            }

            if (! e.mods.isShiftDown() && it->type == ElementType::Dropdown && it->parameterId.isNotEmpty())
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
                    if (def->step >= 1.0f && def->max - def->min <= 32.0f)
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
            multiDragOrigins.clear();
            for (const auto& id : owner.getSelectedElementIds())
                if (auto* selected = owner.getProject().getLayout().find (id))
                    multiDragOrigins[id] = { selected->x, selected->y };
            mode = DragMode::Move;
            return;
        }

        if (! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
            owner.clearSelection();
        dragStart = e.getPosition();
        marqueeRect = { e.x, e.y, 0, 0 };
        mode = DragMode::Marquee;
    }

    void CanvasEditor::addElementAt (ElementType type, juce::Point<int> canvasPos, juce::String parameterId)
    {
        LayoutElement el;
        el.type = type;
        el.x = canvasPos.x;
        el.y = canvasPos.y;
        el.width = (type == ElementType::Panel) ? 320
                 : type == ElementType::Shape ? 180
                 : type == ElementType::TabPanel ? 420
                 : type == ElementType::PadGrid ? 360
                 : type == ElementType::DrumPad ? 80
                 : type == ElementType::Toggle ? 128 : 96;
        el.height = (type == ElementType::Panel) ? 180
                  : type == ElementType::Shape ? 120
                  : type == ElementType::TabPanel ? 44
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

        auto& added = owner.getProject().getLayout().add (el);
        owner.setSelectedElementId (added.id);
        owner.getProject().notifyChanged();
    }

    void CanvasEditor::showContextMenu (juce::Point<int> screenPos)
    {
        const auto canvasPos = screenToCanvas (screenPos);
        juce::PopupMenu menu;
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
        menu.addSectionHeader ("Add DSP Control");

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
        for (const auto& cat : categoryOrder)
        {
            const auto& defs = byCategory[cat];
            juce::PopupMenu sub;
            for (const auto* def : defs)
            {
                sub.addItem (itemId, def->name + "  (" + def->id + ")");
                paramByItem[itemId++] = def->id;
            }
            menu.addSubMenu (cat, sub);
        }

        std::map<int, juce::String> assignParamByItem;
        if (! owner.getSelectedElementIds().isEmpty())
        {
            juce::PopupMenu assignMenu;
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
        menu.addItem (4, "Create Group From Selection", ! owner.getSelectedElementIds().isEmpty());
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
            [this, canvasPos, paramByItem, assignParamByItem, prerequisiteId, prerequisiteValue] (int result)
            {
                if (result == 1) addElementAt (ElementType::Panel, canvasPos);
                else if (result == 2) addElementAt (ElementType::Group, canvasPos);
                else if (result == 3) addElementAt (ElementType::TabPanel, canvasPos);
                else if (result >= 5 && result <= 9)
                {
                    addElementAt (ElementType::Shape, canvasPos);
                    if (auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId()))
                    {
                        if (result == 6) el->shapeKind = "ellipse";
                        if (result == 7) el->shapeKind = "triangle";
                        if (result == 8) el->shapeKind = "diamond";
                        if (result == 9) el->shapeKind = "line";
                        owner.getProject().notifyChanged();
                    }
                }
                else if (result == 4)
                {
                    LayoutElement group;
                    group.type = ElementType::Group;
                    group.label = "New Group";
                    group.id = owner.getProject().getLayout().generateUniqueId ("group_");
                    auto& added = owner.getProject().getLayout().add (group);
                    for (const auto& id : owner.getSelectedElementIds())
                        if (auto* el = owner.getProject().getLayout().find (id))
                            el->containerId = added.id;
                    owner.setSelectedElementId (added.id);
                    owner.getProject().notifyChanged();
                }
                else if (result == 10 && prerequisiteId.isNotEmpty())
                {
                    owner.getProject().getLiveValues().setValue (prerequisiteId, prerequisiteValue);
                    owner.getProject().notifyChanged();
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
                    const auto* def = owner.getProject().getParameters().find (it->second);
                    for (const auto& id : owner.getSelectedElementIds())
                    {
                        if (auto* el = owner.getProject().getLayout().find (id))
                        {
                            const bool assignable = el->type == ElementType::Knob
                                || el->type == ElementType::Slider
                                || el->type == ElementType::Button
                                || el->type == ElementType::Toggle
                                || el->type == ElementType::Dropdown
                                || el->type == ElementType::ValueDisplay;
                            if (! assignable)
                                continue;
                            el->parameterId = it->second;
                            if (el->label.isEmpty() && def != nullptr)
                                el->label = def->name;
                        }
                    }
                    owner.getProject().notifyChanged();
                    repaint();
                }
                else if (auto it = paramByItem.find (result); it != paramByItem.end())
                    addElementAt (ElementType::Knob, canvasPos, it->second);
            });
    }

    void CanvasEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (mode == DragMode::None) return;

        auto snap = [this] (int v) -> int
        {
            if (snapGrid <= 1) return v;
            return juce::roundToInt ((float) v / snapGrid) * snapGrid;
        };

        if (mode == DragMode::ValueDrag)
        {
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
            if (auto* dragged = owner.getProject().getLayout().find (dragValueElementId))
                repaint (elementScreenRect (*dragged).expanded (8));
            else
                repaint();
            return;
        }

        auto* el = owner.getProject().getLayout().find (owner.getSelectedElementId());

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
                }
            layoutChangedDuringDrag = true;
            owner.getProject().markDirty();
        }
        else if (mode == DragMode::ResizeBR)
        {
            if (el == nullptr) return;
            el->width  = juce::jmax (8, snap (dragOriginal.width  + (int) deltaCanvas.x));
            el->height = juce::jmax (8, snap (dragOriginal.height + (int) deltaCanvas.y));
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
                if (item.visible && item.type != ElementType::Group && isElementOnCurrentTab (item)
                    && marqueeRect.intersects (elementScreenRect (item)))
                    ids.add (item.id);
            owner.setSelectedElementIds (ids);
            repaint();
            return;
        }
        repaint();
    }

    void CanvasEditor::mouseUp (const juce::MouseEvent&)
    {
        const bool shouldNotify = layoutChangedDuringDrag;
        mode = DragMode::None;
        dragParameterId.clear();
        dragValueElementId.clear();
        marqueeRect = {};
        multiDragOrigins.clear();
        layoutChangedDuringDrag = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (shouldNotify)
            owner.getProject().notifyChanged();
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
                                               || it->type == ElementType::ValueDisplay;
                if (! isInteractiveControl)
                    continue;

                auto r = elementScreenRect (*it);
                if (! r.contains (e.getPosition()))
                    continue;

                const bool overControl = (it->type == ElementType::Knob || it->type == ElementType::Slider)
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

        // BR-corner resize cursor on selected element
        auto* sel = owner.getProject().getLayout().find (owner.getSelectedElementId());
        if (sel != nullptr && ! sel->locked)
        {
            auto r = elementScreenRect (*sel);
            const int hs = 8;
            if (juce::Rectangle<int> (r.getRight() - hs, r.getBottom() - hs, hs * 2, hs * 2)
                .contains (e.getPosition()))
            {
                setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
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
            bool moved = false;
            for (const auto& id : owner.getSelectedElementIds())
                if (auto* el = owner.getProject().getLayout().find (id); el != nullptr && ! el->locked)
                {
                    if (key.isKeyCode (juce::KeyPress::leftKey))  el->x -= step;
                    if (key.isKeyCode (juce::KeyPress::rightKey)) el->x += step;
                    if (key.isKeyCode (juce::KeyPress::upKey))    el->y -= step;
                    if (key.isKeyCode (juce::KeyPress::downKey))  el->y += step;
                    moved = true;
                }
            if (! moved) return false;
            owner.getProject().notifyChanged();
            repaint();
            return true;
        }
        return false;
    }

} // namespace patchcraft
