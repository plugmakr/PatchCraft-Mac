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

        const auto backgroundPath = owner.getProject().backgroundImageRelative;
        if (backgroundPath.isNotEmpty())
        {
            const auto file = juce::File::isAbsolutePath (backgroundPath)
                ? juce::File (backgroundPath)
                : owner.getProject().getProjectFolder().getChildFile (backgroundPath);

            if (file.existsAsFile())
            {
                const auto image = owner.getAssets().loadImage (file);
                if (image.isValid())
                    g.drawImage (image, r.toFloat(), juce::RectanglePlacement::stretchToFit);
            }
        }

        // Grid
        if (showGrid)
        {
            const auto minorStep = (float) snapGrid * zoom;
            if (minorStep >= 3.0f)
            {
                g.setColour (gridColour.withAlpha (0.80f));
                for (float x = (float) r.getX(); x < (float) r.getRight(); x += minorStep)
                    g.drawVerticalLine ((int) x, (float) r.getY(), (float) r.getBottom());
                for (float y = (float) r.getY(); y < (float) r.getBottom(); y += minorStep)
                    g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
            }

            g.setColour (snapColour.withAlpha (0.95f));
            const auto step = (float) snapGrid * zoom * 5.0f;
            if (step >= 3.0f)
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

    static void drawMacroControlElement (juce::Graphics& g, juce::Rectangle<int> r,
                                         const LayoutElement& e,
                                         const PatchCraftProject& project)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (9, 7);
        auto title = area.removeFromTop (18);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MACRO",
                    title, juce::Justification::centredLeft, true);

        const auto* def = project.getParameters().find (e.parameterId);
        const float raw = def != nullptr ? project.getLiveValues().getValue (def->id, def->defaultValue) : 0.5f;
        const float norm = def != nullptr
            ? juce::jlimit (0.0f, 1.0f, (raw - def->min) / juce::jmax (0.0001f, def->max - def->min))
            : 0.5f;

        auto knob = area.removeFromLeft (juce::jmin (area.getHeight(), 74)).reduced (4);
        LayoutElement knobElement = e;
        knobElement.labelPosition = "hidden";
        drawCanvasKnob (g, knob, knobElement, {}, accent, norm);

        auto lanes = area.reduced (5, 4);
        const juce::StringArray targetLabels { "TONE", "MOTION", "SPACE", "DRIVE" };
        for (int i = 0; i < targetLabels.size(); ++i)
        {
            auto row = lanes.removeFromTop (juce::jmax (14, lanes.getHeight() / (targetLabels.size() - i))).reduced (0, 2);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.70f));
            g.fillRoundedRectangle (row.toFloat(), 3.0f);
            g.setColour (accent.withAlpha (0.35f + 0.10f * (float) i));
            g.fillRoundedRectangle (row.withWidth (juce::roundToInt ((float) row.getWidth() * norm)).toFloat(), 3.0f);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText (targetLabels[i], row.reduced (5, 0), juce::Justification::centredLeft, true);
        }
    }

    static void drawModMatrixElement (juce::Graphics& g, juce::Rectangle<int> r,
                                      const LayoutElement& e,
                                      const PatchCraftProject& project)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MOD MATRIX",
                    header.removeFromLeft (140), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.0f));
        g.drawText (juce::String ((int) project.getDspGraph().modulation.size()) + " routes",
                    header, juce::Justification::centredRight, true);

        auto list = area.reduced (4, 6);
        const auto& routes = project.getDspGraph().modulation;
        if (routes.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawFittedText ("No routes yet. Select this element, then add source -> target rows in the Inspector.",
                              list, juce::Justification::centred, 3);
            return;
        }

        const int maxRows = juce::jmin (6, (int) routes.size());
        for (int i = 0; i < maxRows; ++i)
        {
            const auto& route = routes[(size_t) i];
            auto row = list.removeFromTop (juce::jmax (18, list.getHeight() / (maxRows - i))).reduced (0, 2);
            g.setColour (route.enabled ? accent.withAlpha (0.16f) : PatchCraftLookAndFeel::bg().withAlpha (0.78f));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (route.enabled ? accent.withAlpha (0.78f) : PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (8.8f, juce::Font::bold));
            g.drawText (route.sourceId + " -> " + route.targetId, row.removeFromLeft (juce::jmax (80, row.getWidth() - 52)).reduced (6, 0),
                        juce::Justification::centredLeft, true);
            g.drawText (juce::String (route.amount, 2), row.reduced (4, 0), juce::Justification::centredRight, true);
        }
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

    static float eqFrequencyToX01 (float frequency)
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        return juce::jlimit (0.0f, 1.0f,
            std::log (f / 20.0f) / std::log (20000.0f / 20.0f));
    }

    static float eqGainToY01 (float gainDb)
    {
        return juce::jlimit (0.0f, 1.0f, juce::jmap (gainDb, 24.0f, -24.0f, 0.0f, 1.0f));
    }

    static void drawEqCurveElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e,
                                    const DspGraph& graph)
    {
        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "EQ CURVE", header.removeFromLeft (130),
                    juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (9.0f);
        g.drawText ("live patch EQ", header, juce::Justification::centredRight, true);

        auto graphArea = area.reduced (2, 4);
        g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);
        g.setColour (border.withAlpha (0.22f));
        for (int i = 1; i < 5; ++i)
        {
            const int x = graphArea.getX() + (graphArea.getWidth() * i) / 5;
            const int y = graphArea.getY() + (graphArea.getHeight() * i) / 5;
            g.drawVerticalLine (x, (float) graphArea.getY(), (float) graphArea.getBottom());
            g.drawHorizontalLine (y, (float) graphArea.getX(), (float) graphArea.getRight());
        }

        juce::Path curve;
        for (int i = 0; i < 96; ++i)
        {
            const float x01 = (float) i / 95.0f;
            float y = (float) graphArea.getCentreY();
            for (const auto& block : graph.blocks)
            {
                if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                    continue;
                const float freqX = eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f));
                const float gain = blockValue (block, "eqGainDb", 0.0f);
                const float q = juce::jlimit (0.15f, 18.0f, blockValue (block, "eqQ", 1.0f));
                const float width = juce::jlimit (0.025f, 0.22f, 0.11f / std::sqrt (q));
                const float influence = std::exp (-std::pow ((x01 - freqX) / width, 2.0f));
                y -= (gain / 24.0f) * influence * (float) graphArea.getHeight() * 0.44f;
            }
            const float x = (float) graphArea.getX() + x01 * (float) graphArea.getWidth();
            y = juce::jlimit ((float) graphArea.getY(), (float) graphArea.getBottom(), y);
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }
        g.setColour (accent.withAlpha (0.92f));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        for (const auto& block : graph.blocks)
        {
            if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                continue;
            const float x = (float) graphArea.getX() + eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f)) * (float) graphArea.getWidth();
            const float y = (float) graphArea.getY() + eqGainToY01 (blockValue (block, "eqGainDb", 0.0f)) * (float) graphArea.getHeight();
            g.setColour (accent);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
    }

    static void drawSpectrumElement (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e)
    {
        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? juce::Colour (0xff20d6ff) : e.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "SPECTRUM", header.removeFromLeft (130),
                    juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (9.0f);
        g.drawText ("runtime analyzer", header, juce::Justification::centredRight, true);
        auto graphArea = area.reduced (2, 4);
        g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);

        constexpr int bars = 36;
        for (int i = 0; i < bars; ++i)
        {
            const float x01 = (float) i / (float) (bars - 1);
            const float h01 = 0.18f + 0.72f * std::abs (std::sin (x01 * 9.0f + (float) i * 0.37f));
            const int barW = juce::jmax (2, graphArea.getWidth() / bars - 2);
            const int x = graphArea.getX() + i * graphArea.getWidth() / bars;
            const int h = juce::roundToInt (h01 * (float) graphArea.getHeight());
            g.setColour (accent.interpolatedWith (PatchCraftLookAndFeel::accent(), x01).withAlpha (0.78f));
            g.fillRoundedRectangle ((float) x, (float) graphArea.getBottom() - (float) h,
                                    (float) barW, (float) h, 2.0f);
        }
    }

    static void drawReactiveVisualPlaceholder (juce::Graphics& g, juce::Rectangle<int> r,
                                               const LayoutElement& e, float previewLevel)
    {
        const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0x6610151d) : e.backgroundColour;
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const float amount = juce::jlimit (0.0f, 1.0f, previewLevel + (e.audioReactive ? e.audioReactiveAmount * 0.35f : 0.0f));
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour ((e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour).withAlpha (0.75f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), juce::jmax (0.5f, e.strokeWidth));

        auto bounds = r.reduced (10).toFloat();
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * (0.22f + amount * 0.12f);
        for (int ring = 0; ring < 4; ++ring)
        {
            const float scale = 1.0f + (float) ring * 0.58f;
            g.setColour (accent.withAlpha (0.28f - (float) ring * 0.045f));
            g.drawEllipse (centre.x - radius * scale,
                           centre.y - radius * scale,
                           radius * 2.0f * scale,
                           radius * 2.0f * scale,
                           1.2f + amount * 2.0f);
        }

        g.setColour (accent.withAlpha (0.75f));
        g.fillEllipse (centre.x - radius * 0.32f, centre.y - radius * 0.32f, radius * 0.64f, radius * 0.64f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "REACTIVE IMAGE",
                    r.reduced (10, 8), juce::Justification::topLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.5f));
        g.drawText (e.visualSource + " -> " + e.visualAction,
                    r.reduced (10, 8), juce::Justification::bottomRight, true);
    }

    static void drawSpriteAnimatorPlaceholder (juce::Graphics& g, juce::Rectangle<int> r,
                                               const LayoutElement& e, juce::Image img)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0xaa080b10) : e.backgroundColour);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        if (img.isValid())
        {
            const int frames = juce::jmax (1, e.filmstripFrames > 0 ? e.filmstripFrames : 8);
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const int frame = ((int) std::floor (seconds * juce::jmax (0.05f, e.animationRate))) % frames;
            if (e.filmstripVertical)
            {
                const int h = juce::jmax (1, img.getHeight() / frames);
                g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                             0, frame * h, img.getWidth(), h);
            }
            else
            {
                const int w = juce::jmax (1, img.getWidth() / frames);
                g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                             frame * w, 0, w, img.getHeight());
            }
            return;
        }

        auto area = r.reduced (12, 28);
        const int cols = 4;
        const int rows = 2;
        const int frameW = juce::jmax (1, area.getWidth() / cols);
        const int frameH = juce::jmax (1, area.getHeight() / rows);
        const int active = ((int) (juce::Time::getMillisecondCounterHiRes() * 0.001 * juce::jmax (0.05f, e.animationRate))) % (cols * rows);
        for (int i = 0; i < cols * rows; ++i)
        {
            auto cell = juce::Rectangle<int> (area.getX() + (i % cols) * frameW,
                                              area.getY() + (i / cols) * frameH,
                                              frameW - 6, frameH - 6);
            g.setColour (i == active ? accent.withAlpha (0.62f) : accent.withAlpha (0.14f));
            g.fillRoundedRectangle (cell.toFloat(), 4.0f);
            g.setColour (accent.withAlpha (0.72f));
            g.drawRoundedRectangle (cell.toFloat(), 4.0f, 1.0f);
        }
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "SPRITE ANIMATOR",
                    r.reduced (10, 8), juce::Justification::topLeft, true);
    }

    static void drawVisualFxLayer (juce::Graphics& g, juce::Rectangle<int> r,
                                   const LayoutElement& e, float previewLevel)
    {
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const float seconds = (float) (juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float level = juce::jlimit (0.0f, 1.0f, previewLevel + (e.audioReactive ? e.audioReactiveAmount * 0.4f : 0.0f));
        g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0x22000000) : e.backgroundColour);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));

        auto bounds = r.reduced (10).toFloat();
        if (e.visualPreset == "spectrumBars")
        {
            for (int i = 0; i < 18; ++i)
            {
                const float h = bounds.getHeight() * (0.12f + 0.78f * std::abs (std::sin (seconds * 1.7f + (float) i * 0.47f)) * (0.35f + level));
                g.setColour (accent.withHue (std::fmod (accent.getHue() + (float) i * 0.018f, 1.0f)).withAlpha (0.55f));
                g.fillRoundedRectangle (bounds.getX() + (float) i * bounds.getWidth() / 18.0f,
                                        bounds.getBottom() - h,
                                        bounds.getWidth() / 24.0f,
                                        h,
                                        2.0f);
            }
        }
        else
        {
            const auto centre = bounds.getCentre();
            const float base = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.18f;
            for (int i = 0; i < 36; ++i)
            {
                const float phase = seconds * juce::jmax (0.05f, e.animationRate) + (float) i * 0.42f;
                const float radius = base + std::fmod ((float) i * 13.0f + seconds * 28.0f, base * (1.5f + level));
                const float x = centre.x + std::cos (phase) * radius;
                const float y = centre.y + std::sin (phase * 0.83f) * radius * 0.72f;
                g.setColour (accent.withAlpha (0.16f + level * 0.22f));
                g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f + level * 4.0f, 4.0f + level * 4.0f);
            }
            g.setColour (accent.withAlpha (0.36f + level * 0.28f));
            g.drawEllipse (centre.x - base * 1.6f, centre.y - base * 1.6f, base * 3.2f, base * 3.2f, 1.2f + level * 2.0f);
        }

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "VISUAL FX",
                    r.reduced (8), juce::Justification::topLeft, true);
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

        if (e.type == ElementType::ReactiveImage)
        {
            juce::Image img;
            if (e.asset.isNotEmpty())
            {
                juce::File f = juce::File::isAbsolutePath (e.asset)
                    ? juce::File (e.asset)
                    : owner.getProject().getProjectFolder().getChildFile (e.asset);
                if (f.existsAsFile())
                    img = owner.getAssets().loadImage (f);
            }

            if (img.isValid())
            {
                g.drawImage (img, r.toFloat());
                drawReactiveVisualPlaceholder (g, r, e, 0.25f);
            }
            else
            {
                drawReactiveVisualPlaceholder (g, r, e, 0.25f);
            }
        }
        else if (e.type == ElementType::SpriteAnimator)
        {
            juce::Image img;
            const auto assetPath = e.asset.isNotEmpty() ? e.asset : e.filmstripAsset;
            if (assetPath.isNotEmpty())
            {
                juce::File f = juce::File::isAbsolutePath (assetPath)
                    ? juce::File (assetPath)
                    : owner.getProject().getProjectFolder().getChildFile (assetPath);
                if (f.existsAsFile())
                    img = owner.getAssets().loadImage (f);
            }
            drawSpriteAnimatorPlaceholder (g, r, e, img);
        }
        else if (e.type == ElementType::VisualFxLayer)
        {
            drawVisualFxLayer (g, r, e, 0.30f);
        }
        else if (e.type == ElementType::AiVisualPrompt)
        {
            const auto accent = e.accentColour.isTransparent() ? juce::Colour (0xffb98cff) : e.accentColour;
            g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0xdd121019) : e.backgroundColour);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);
            auto area = r.reduced (12, 10);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("PRO AI VISUAL PROMPT", area.removeFromTop (22), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.5f));
            const auto prompt = e.visualAiPrompt.isNotEmpty() ? e.visualAiPrompt
                : "Generate artwork, sprite sheets, masks, or title banners from this instrument's sound and brand direction.";
            g.drawFittedText (prompt, area, juce::Justification::topLeft, 5);
        }
        // ---- Image element ------------------------------------------------
        // 'background' (id == "background") falls back to the procedural hero
        //   artwork when no asset is set.
        // 'hero' or any other Image with an empty asset draws an "Artwork"
        //   placeholder so the user knows where to drop a PNG.
        // Any Image with an asset path loads + draws the file.
        else if (e.type == ElementType::Image)
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
        else if (e.type == ElementType::SampleDropZone)
        {
            const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xdd10141a) : e.backgroundColour;
            const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

            const bool compactDropZone = r.getHeight() <= 64 || e.semanticRole.startsWith ("circleSeqLaneSample:");
            if (compactDropZone)
            {
                auto pill = r.reduced (5, 4);
                g.setColour (accent.withAlpha (0.18f));
                g.fillRoundedRectangle (pill.toFloat(), juce::jmax (5.0f, e.cornerRadius - 1.0f));
                g.setColour (accent.withAlpha (0.85f));
                g.drawRoundedRectangle (pill.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius - 1.0f), 1.2f);

                auto textArea = pill.reduced (7, 3);
                g.setFont (juce::Font (10.5f, juce::Font::bold));
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.drawFittedText ((e.label.isNotEmpty() ? e.label : "DROP SAMPLES").toUpperCase(),
                                  textArea.removeFromTop (18), juce::Justification::centred, 1);
                g.setFont (juce::Font (8.5f, juce::Font::bold));
                g.setColour (e.asset.isNotEmpty() ? accent : PatchCraftLookAndFeel::textDim());
                g.drawFittedText (e.asset.isNotEmpty() ? juce::File (e.asset).getFileName() : "DROP WAV/AIFF",
                                  textArea, juce::Justification::centred, 1);
                return;
            }

            auto area = r.reduced (12, 10);
            g.setColour (accent.withAlpha (0.26f));
            g.drawRoundedRectangle (area.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius - 2.0f), 1.0f);
            auto header = area.removeFromTop (24);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText ("SAMPLE DROP ZONE", header.removeFromLeft (150), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f));
            g.drawText (e.parameterId.isNotEmpty() ? "target " + e.parameterId : "target sampleStart",
                        header, juce::Justification::centredRight, true);

            area.removeFromTop (8);
            juce::Path wave;
            const auto waveBounds = area.removeFromTop (juce::jmax (36, area.getHeight() / 2)).reduced (6, 0).toFloat();
            for (int i = 0; i < 48; ++i)
            {
                const float x = waveBounds.getX() + (float) i / 47.0f * waveBounds.getWidth();
                const float y = waveBounds.getCentreY()
                    + std::sin ((float) i * 0.62f) * waveBounds.getHeight() * (0.16f + 0.28f * (float) ((i % 7) + 1) / 7.0f);
                if (i == 0) wave.startNewSubPath (x, y);
                else        wave.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.85f));
            g.strokePath (wave, juce::PathStrokeType (1.5f));

            area.removeFromTop (8);
            const auto fileLabel = e.asset.isNotEmpty()
                ? juce::File (e.asset).getFileName()
                : juce::String ("Drop WAV / AIFF / FLAC here");
            g.setColour (e.asset.isNotEmpty() ? PatchCraftLookAndFeel::textBright() : PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawFittedText (fileLabel, area.removeFromTop (24), juce::Justification::centred, 1);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.0f);
            g.drawFittedText (e.semanticRole.isNotEmpty() ? "linked controls: " + e.semanticRole.fromFirstOccurrenceOf (":", false, false)
                                                          : "drop once to create linked Start / Length / Pitch / Level controls",
                              area, juce::Justification::centred, 2);
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
            else if (e.filmstripAsset.isNotEmpty())
            {
                pos01 = juce::jlimit (0.0f, 1.0f, e.controlPreviewValue);
                value = juce::String (juce::roundToInt (pos01 * 100.0f)) + " %";
            }

            // Filmstrip override - load PNG and draw frame.
            if (e.filmstripAsset.isNotEmpty())
            {
                juce::File f (juce::File::isAbsolutePath (e.filmstripAsset)
                                ? e.filmstripAsset
                                : owner.getProject().getProjectFolder()
                                       .getChildFile (e.filmstripAsset).getFullPathName());
                if (auto img = owner.getAssets().loadControlFilmstrip (f, e.filmstripFrames, e.filmstripVertical); img.isValid())
                {
                    int frames = e.filmstripFrames;
                    if (frames <= 1)
                        frames = juce::jmax (frames, PatchCraftLookAndFeel::detectFilmstripFrames (img, e.filmstripVertical));
                    auto stripRect = e.labelPosition == "hidden"
                        ? r.reduced (2)
                        : r.withTrimmedBottom (juce::roundToInt (r.getHeight() * 0.30f));
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
        else if (e.type == ElementType::EqCurve)
        {
            drawEqCurveElement (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::SpectrumAnalyzer)
        {
            drawSpectrumElement (g, r, e);
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
        else if (e.type == ElementType::GranularField)
        {
            const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
            const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (22);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "GRANULAR FIELD",
                        header.removeFromLeft (160), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f));
            g.drawText ("sample position / length / slice / glitch",
                        header, juce::Justification::centredRight, true);
            area.removeFromTop (4);

            g.setColour (border.withAlpha (0.22f));
            for (int i = 1; i < 6; ++i)
            {
                const float x = (float) area.getX() + (float) area.getWidth() * (float) i / 6.0f;
                g.drawVerticalLine (juce::roundToInt (x), (float) area.getY(), (float) area.getBottom());
            }

            const float centreX = (float) area.getX() + (float) area.getWidth() * 0.38f;
            const float span = (float) area.getWidth() * 0.34f;
            g.setColour (accent.withAlpha (0.25f));
            g.fillEllipse (centreX - span * 0.45f, (float) area.getCentreY() - (float) area.getHeight() * 0.28f,
                           span * 0.9f, (float) area.getHeight() * 0.56f);
            g.setColour (accent.withAlpha (0.9f));
            g.drawLine (centreX, (float) area.getY(), centreX, (float) area.getBottom(), 1.4f);
            for (int i = 0; i < 42; ++i)
            {
                const float phase = (float) i * 0.71f;
                const float x = centreX + std::sin (phase * 1.3f) * span * 0.48f;
                const float y = (float) area.getCentreY() + std::cos (phase * 0.9f) * (float) area.getHeight() * 0.23f;
                const float size = 2.0f + (float) (i % 4);
                g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size, size);
            }
        }
        else if (e.type == ElementType::MacroControl)
        {
            drawMacroControlElement (g, r, e, owner.getProject());
        }
        else if (e.type == ElementType::ModMatrix)
        {
            drawModMatrixElement (g, r, e, owner.getProject());
        }
        else if (e.type == ElementType::DrumGrid)
        {
            drawDrumGridPreview (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::ArpLane)
        {
            drawArpLanePreview (g, r, e, owner.getProject().getDspGraph());
        }
        else if (e.type == ElementType::Mixer)
        {
            const auto bg = e.backgroundColour.isTransparent() ? juce::Colour (0xff15171b) : e.backgroundColour;
            const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
            const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
            const int channels = juce::jlimit (1, 16, e.mixerChannels);

            g.setColour (bg);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);

            auto area = r.reduced (10, 8);
            auto header = area.removeFromTop (22);
            g.setColour (accent);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MIXER",
                        header.removeFromLeft (160), juce::Justification::centredLeft, true);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f));
            g.drawText ("Auto layer / parameter mixer",
                        header, juce::Justification::centredRight, true);
            area.removeFromTop (6);

            const int stripW = juce::jmax (38, area.getWidth() / channels);
            for (int channel = 0; channel < channels; ++channel)
            {
                auto strip = juce::Rectangle<int> (area.getX() + channel * stripW,
                                                   area.getY(),
                                                   channel == channels - 1
                                                        ? area.getRight() - (area.getX() + channel * stripW)
                                                        : stripW,
                                                   area.getHeight()).reduced (3, 0);
                if (strip.getWidth() <= 10)
                    continue;

                const auto label = stringAtOr (e.mixerChannelLabels, channel,
                                               channel == 0 ? "Main" : "Bus " + juce::String (channel + 1));
                g.setColour (juce::Colour (0xaa0b0f14));
                g.fillRoundedRectangle (strip.toFloat(), 5.0f);
                g.setColour (border.withAlpha (0.75f));
                g.drawRoundedRectangle (strip.toFloat().reduced (0.5f), 5.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText (label, strip.removeFromTop (20), juce::Justification::centred, true);

                auto buttons = strip.removeFromBottom (22).reduced (2, 2);
                auto pan = strip.removeFromBottom (22).reduced (5, 5);
                auto value = strip.removeFromBottom (14);
                auto fader = strip.reduced (5, 6);
                const int centre = fader.getCentreX();
                const auto track = juce::Rectangle<int> (centre - 4, fader.getY() + 2,
                                                         8, juce::jmax (12, fader.getHeight() - 4));
                const float level = channel == 0 ? 0.78f : 0.55f - (float) channel * 0.06f;
                const int thumbY = juce::roundToInt (juce::jmap (juce::jlimit (0.15f, 0.90f, level),
                                                                  0.0f, 1.0f,
                                                                  (float) track.getBottom(),
                                                                  (float) track.getY()));
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                g.fillRoundedRectangle (track.toFloat(), 4.0f);
                g.setColour (accent.withAlpha (0.72f));
                g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (track.getX(), thumbY,
                                                                                  track.getRight(), track.getBottom()).toFloat(), 4.0f);
                g.setColour (PatchCraftLookAndFeel::text());
                g.fillRoundedRectangle (juce::Rectangle<float> ((float) centre - 11.0f,
                                                                (float) thumbY - 4.0f,
                                                                22.0f, 8.0f), 3.0f);

                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.0f));
                g.drawText (juce::String (juce::roundToInt (level * 100.0f)),
                            value, juce::Justification::centred, true);
                g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.72f));
                g.fillRoundedRectangle (pan.toFloat(), 3.0f);
                g.setColour (accent.withAlpha (0.45f));
                g.drawLine ((float) pan.getX(), (float) pan.getCentreY(),
                            (float) pan.getRight(), (float) pan.getCentreY(), 2.0f);

                auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0);
                auto solo = buttons.reduced (1, 0);
                g.setColour (PatchCraftLookAndFeel::bg().brighter (0.08f));
                g.fillRoundedRectangle (mute.toFloat(), 3.0f);
                g.fillRoundedRectangle (solo.toFloat(), 3.0f);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::Font (9.0f, juce::Font::bold));
                g.drawText ("M", mute, juce::Justification::centred, true);
                g.drawText ("S", solo, juce::Justification::centred, true);
            }
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
