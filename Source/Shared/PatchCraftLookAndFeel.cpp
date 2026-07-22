#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        juce::Image loadOptimizedFilmstripForControl (const juce::String& path, int frames, bool vertical)
        {
            if (path.isEmpty())
                return {};

            static juce::HashMap<juce::String, juce::Image> cache;
            const int maxFrameSize = 192;
            const auto key = path + "|" + juce::String (frames) + "|" + (vertical ? "v" : "h");
            if (cache.contains (key))
                return cache[key];

            auto img = juce::ImageFileFormat::loadFrom (juce::File (path));
            if (! img.isValid())
                return {};

            int safeFrames = juce::jmax (1, frames);
            if (frames <= 1)
            {
                const int w = juce::jmax (1, img.getWidth());
                const int h = juce::jmax (1, img.getHeight());
                const int inferred = vertical ? juce::roundToInt ((double) h / (double) w)
                                              : juce::roundToInt ((double) w / (double) h);
                if (inferred > safeFrames)
                    safeFrames = inferred;
            }

            const int frameW = vertical ? img.getWidth() : juce::jmax (1, img.getWidth() / safeFrames);
            const int frameH = vertical ? juce::jmax (1, img.getHeight() / safeFrames) : img.getHeight();
            const int largestFrameEdge = juce::jmax (frameW, frameH);
            if (largestFrameEdge > maxFrameSize)
            {
                const double scale = (double) maxFrameSize / (double) largestFrameEdge;
                const int targetFrameW = juce::jmax (1, juce::roundToInt ((double) frameW * scale));
                const int targetFrameH = juce::jmax (1, juce::roundToInt ((double) frameH * scale));
                img = img.rescaled (vertical ? targetFrameW : targetFrameW * safeFrames,
                                    vertical ? targetFrameH * safeFrames : targetFrameH,
                                    juce::Graphics::highResamplingQuality);
            }

            cache.set (key, img);
            return img;
        }
    }

    PatchCraftLookAndFeel::PatchCraftLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg());
        setColour (juce::DocumentWindow::backgroundColourId, bg());

        setColour (juce::Label::textColourId,                text());
        setColour (juce::Label::backgroundColourId,          juce::Colours::transparentBlack);
        setColour (juce::Label::outlineColourId,             juce::Colours::transparentBlack);

        setColour (juce::TextButton::buttonColourId,         raised());
        setColour (juce::TextButton::buttonOnColourId,       accent());
        setColour (juce::TextButton::textColourOffId,        text());
        setColour (juce::TextButton::textColourOnId,         juce::Colour (0xff111111));

        setColour (juce::ToggleButton::textColourId,         text());
        setColour (juce::ToggleButton::tickColourId,         accent());
        setColour (juce::ToggleButton::tickDisabledColourId, textDim());

        setColour (juce::ComboBox::backgroundColourId,       panelAlt());
        setColour (juce::ComboBox::textColourId,             text());
        setColour (juce::ComboBox::outlineColourId,          border());
        setColour (juce::ComboBox::buttonColourId,           raised());
        setColour (juce::ComboBox::arrowColourId,            text());

        setColour (juce::PopupMenu::backgroundColourId,        panel());
        setColour (juce::PopupMenu::textColourId,              text());
        setColour (juce::PopupMenu::headerTextColourId,        accent());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId,   accent());

        setColour (juce::AlertWindow::backgroundColourId,      panelAlt());
        setColour (juce::AlertWindow::textColourId,            text());
        setColour (juce::AlertWindow::outlineColourId,         border().brighter (0.18f));

        setColour (juce::Slider::backgroundColourId,         raised());
        setColour (juce::Slider::trackColourId,              border());
        setColour (juce::Slider::thumbColourId,              accent());
        setColour (juce::Slider::rotarySliderFillColourId,   accent());
        setColour (juce::Slider::rotarySliderOutlineColourId,border());
        setColour (juce::Slider::textBoxTextColourId,        text());
        setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);

        setColour (juce::TextEditor::backgroundColourId,     panelAlt());
        setColour (juce::TextEditor::textColourId,           text());
        setColour (juce::TextEditor::highlightColourId,      accent().withAlpha (0.35f));
        setColour (juce::TextEditor::outlineColourId,        border());
        setColour (juce::TextEditor::focusedOutlineColourId, accent());

        setColour (juce::ScrollBar::backgroundColourId,      panel());
        setColour (juce::ScrollBar::thumbColourId,           border());
        setColour (juce::ScrollBar::trackColourId,           panel());

        setColour (juce::TabbedComponent::backgroundColourId, panel());
        setColour (juce::TabbedComponent::outlineColourId,    border());
        setColour (juce::TabbedButtonBar::tabOutlineColourId, border());
        setColour (juce::TabbedButtonBar::frontOutlineColourId, accent());
    }

    // -------------------------------------------------------------------------
    // Convenience drawers
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                           float corner, bool drawBorder)
    {
        auto rect = r.toFloat();
        juce::ColourGradient grad (raised().withAlpha (0.98f), rect.getX(), rect.getY(),
                                   panel().darker (0.28f), rect.getX(), rect.getBottom(), false);
        grad.addColour (0.52, panelAlt());
        g.setGradientFill (grad);
        g.fillRoundedRectangle (rect, corner);

        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawRoundedRectangle (rect.reduced (1.0f), juce::jmax (1.0f, corner - 1.0f), 1.0f);
        g.setColour (accent().withAlpha (0.075f));
        g.fillRoundedRectangle (rect.withHeight (2.0f).reduced (corner * 0.45f, 0.0f), 1.0f);

        if (drawBorder)
        {
            g.setColour (border().withAlpha (0.92f));
            g.drawRoundedRectangle (rect.reduced (0.5f), corner, 1.15f);
        }
    }

    void PatchCraftLookAndFeel::drawDarkPanel (juce::Graphics& g,
                                               juce::Rectangle<int> r, float corner)
    {
        auto rect = r.toFloat();
        juce::ColourGradient grad (panel().darker (0.05f), rect.getX(), rect.getY(),
                                   bg().darker (0.10f), rect.getX(), rect.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (rect, corner);
        g.setColour (borderSoft().brighter (0.22f));
        g.drawRoundedRectangle (rect.reduced (0.5f), corner, 1.0f);
    }

    void PatchCraftLookAndFeel::drawAccentUnderline (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (accent());
        g.fillRect (r.removeFromBottom (2));
    }

    int PatchCraftLookAndFeel::detectFilmstripFrames (const juce::Image& strip, bool vertical)
    {
        if (! strip.isValid()) return 0;
        const int w = strip.getWidth(), h = strip.getHeight();
        if (w <= 0 || h <= 0) return 0;
        if (vertical)
            return juce::jmax (1, juce::roundToInt ((double) h / (double) w));
        return juce::jmax (1, juce::roundToInt ((double) w / (double) h));
    }

    void PatchCraftLookAndFeel::drawFilmstripFrame (juce::Graphics& g,
                                                    juce::Rectangle<int> dest,
                                                    const juce::Image& strip,
                                                    int totalFrames, float position,
                                                    bool vertical)
    {
        if (! strip.isValid() || totalFrames <= 0) return;
        const float pos = juce::jlimit (0.0f, 1.0f, position);
        const int frame = juce::jlimit (0, totalFrames - 1,
                                        (int) std::floor (pos * (float) totalFrames));
        if (vertical)
        {
            const int fh = strip.getHeight() / totalFrames;
            g.drawImage (strip,
                         dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                         0, frame * fh, strip.getWidth(), fh);
        }
        else
        {
            const int fw = strip.getWidth() / totalFrames;
            g.drawImage (strip,
                         dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                         frame * fw, 0, fw, strip.getHeight());
        }
    }

    void PatchCraftLookAndFeel::drawHexLogo (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // hexagon shield
        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY();
        const auto rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.45f;

        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const auto a = juce::MathConstants<float>::pi * 2.0f * (i / 6.0f) - juce::MathConstants<float>::halfPi;
            const auto x = cx + std::cos (a) * rad;
            const auto y = cy + std::sin (a) * rad;
            if (i == 0) hex.startNewSubPath (x, y);
            else        hex.lineTo (x, y);
        }
        hex.closeSubPath();

        juce::ColourGradient grad (accent().brighter (0.2f), cx, cy - rad,
                                   accent().darker (0.4f),  cx, cy + rad, false);
        g.setGradientFill (grad);
        g.fillPath (hex);

        g.setColour (juce::Colour (0xff2a1d08));
        g.strokePath (hex, juce::PathStrokeType (1.5f));

        // inner P glyph
        g.setColour (juce::Colour (0xff15110b));
        g.setFont (juce::Font (rad * 1.4f, juce::Font::bold));
        g.drawText ("P", r, juce::Justification::centred);
    }

    // -------------------------------------------------------------------------
    // Rotary
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                                  float sliderPos, float startAngle, float endAngle,
                                                  juce::Slider& slider)
    {
        // ---- Filmstrip override --------------------------------------------
        // If the slider has a "filmstripPath" property set, render via PNG
        // strip instead of procedural drawing.
        const auto stripPath = slider.getProperties().getWithDefault ("filmstripPath", "").toString();
        if (stripPath.isNotEmpty())
        {
            int frames = (int) slider.getProperties().getWithDefault ("filmstripFrames", 0);
            const bool vertical = (bool) slider.getProperties().getWithDefault ("filmstripVertical", true);
            auto img = loadOptimizedFilmstripForControl (stripPath, frames, vertical);
            if (img.isValid())
            {
                if (frames <= 0)
                    frames = detectFilmstripFrames (img, vertical);
                drawFilmstripFrame (g, juce::Rectangle<int> (x, y, w, h),
                                    img, frames, sliderPos, vertical);
                return;
            }
        }

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h)
                                .reduced (4.0f);
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto rad = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);

        // Outer ring track
        const float ringW = juce::jmax (3.0f, rad * 0.10f);
        juce::Path track;
        track.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                             0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff202227));
        g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // Active ring
        juce::Path active;
        active.addCentredArc (cx, cy, rad - ringW * 0.5f, rad - ringW * 0.5f,
                              0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (active, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Knob body (gradient)
        const float bodyR = rad - ringW - 4.0f;
        juce::ColourGradient grad (juce::Colour (0xff2a2d33), cx, cy - bodyR,
                                   juce::Colour (0xff111317), cx, cy + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2, bodyR * 2);

        g.setColour (juce::Colour (0xff070809));
        g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2, bodyR * 2, 1.0f);

        // Indicator
        const float indL = bodyR * 0.85f;
        const float indW = juce::jmax (2.5f, bodyR * 0.12f);
        juce::Path ind;
        ind.addRoundedRectangle (-indW * 0.5f, -indL, indW, bodyR * 0.55f, indW * 0.5f);
        ind.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillPath (ind);
    }

    // -------------------------------------------------------------------------
    // Linear slider (used for vertical performance sliders)
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                                  float sliderPos, float, float,
                                                  juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        const auto stripPath = slider.getProperties().getWithDefault ("filmstripPath", "").toString();
        if (stripPath.isNotEmpty())
        {
            int frames = (int) slider.getProperties().getWithDefault ("filmstripFrames", 0);
            const bool vertical = (bool) slider.getProperties().getWithDefault ("filmstripVertical", true);
            auto img = loadOptimizedFilmstripForControl (stripPath, frames, vertical);
            if (img.isValid())
            {
                if (frames <= 0)
                    frames = detectFilmstripFrames (img, vertical);

                const auto min = slider.getMinimum();
                const auto max = slider.getMaximum();
                const float pos = max > min
                    ? (float) ((slider.getValue() - min) / (max - min))
                    : 0.0f;
                drawFilmstripFrame (g, juce::Rectangle<int> (x, y, w, h),
                                    img, frames, juce::jlimit (0.0f, 1.0f, pos), vertical);
                return;
            }
        }

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

        if (style == juce::Slider::LinearVertical)
        {
            const float trackW = juce::jmax (4.0f, w * 0.18f);
            auto track = bounds.withSizeKeepingCentre (trackW, bounds.getHeight());
            g.setColour (juce::Colour (0xff202227));
            g.fillRoundedRectangle (track, trackW * 0.5f);

            const float thumbY = sliderPos;
            auto fill = juce::Rectangle<float> (track.getX(), thumbY,
                                                track.getWidth(),
                                                track.getBottom() - thumbY);
            g.setColour (accent().withAlpha (0.65f));
            g.fillRoundedRectangle (fill, trackW * 0.5f);

            // Thumb
            const float thumbW = juce::jmin (w - 2.0f, 22.0f);
            const float thumbH = 12.0f;
            auto thumb = juce::Rectangle<float> (bounds.getCentreX() - thumbW * 0.5f,
                                                 thumbY - thumbH * 0.5f,
                                                 thumbW, thumbH);
            juce::ColourGradient tg (juce::Colour (0xff2a2d33), thumb.getX(), thumb.getY(),
                                     juce::Colour (0xff101216), thumb.getX(), thumb.getBottom(), false);
            g.setGradientFill (tg);
            g.fillRoundedRectangle (thumb, 3.0f);
            g.setColour (juce::Colour (0xff050608));
            g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, sliderPos, sliderPos, style, slider);
        }
    }

    // -------------------------------------------------------------------------
    // Buttons
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                      const juce::Colour& bgCol,
                                                      bool over, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const auto isAccent  = b.getProperties().getWithDefault ("accent", false);
        const auto isFlatTab = b.getProperties().getWithDefault ("flatTab", false);
        const auto isWorkflowStep = b.getProperties().getWithDefault ("workflowStep", false);
        const auto isWorkflowProduct = b.getProperties().getWithDefault ("workflowProduct", false);
        const auto isPrimaryAction = b.getProperties().getWithDefault ("primaryAction", false);
        const auto isToolbarIcon = b.getProperties().getWithDefault ("toolbarIcon", false);
        const float corner = (float) b.getProperties().getWithDefault ("corner", 6.0);

        if (isFlatTab)
        {
            // Section tab: no fill, accent underline when toggled.
            if (over && ! b.getToggleState())
            {
                g.setColour (juce::Colour (0xff202227));
                g.fillRect (r);
            }
            if (b.getToggleState())
            {
                g.setColour (accent());
                g.fillRect (r.removeFromBottom (2.0f));
            }
            return;
        }

        if (isWorkflowStep || isWorkflowProduct)
        {
            auto top = raised().brighter (over ? 0.14f : 0.04f);
            auto bottom = panelAlt().darker (down ? 0.05f : 0.0f);
            juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);

            g.setColour (over ? accent().withAlpha (0.48f) : border().withAlpha (0.9f));
            g.drawRoundedRectangle (r, corner, over ? 1.4f : 1.0f);

            const auto rail = r.withWidth (3.0f).reduced (0.0f, 9.0f);
            g.setColour ((isWorkflowProduct ? juce::Colour (0xff58b7ff) : accent()).withAlpha (over ? 0.95f : 0.7f));
            g.fillRoundedRectangle (rail, 1.5f);

            if (over)
            {
                g.setColour (accent().withAlpha (0.08f));
                g.fillRoundedRectangle (r.reduced (1.0f), corner - 1.0f);
            }
            return;
        }

        if (isToolbarIcon)
        {
            auto base = panelAlt().brighter (down ? 0.09f : (over ? 0.06f : 0.0f));
            juce::ColourGradient grad (base.brighter (0.03f), r.getX(), r.getY(),
                                       base.darker (0.12f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);

            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.34f));
            g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 1.0f);
            g.setColour (over || b.getToggleState() ? accent().withAlpha (0.72f)
                                                     : border().withAlpha (0.9f));
            g.drawRoundedRectangle (r, corner, over ? 1.2f : 0.8f);

            if (b.getToggleState())
            {
                g.setColour (accent().withAlpha (0.20f));
                g.fillRoundedRectangle (r.reduced (2.0f), corner - 2.0f);
            }
            return;
        }

        if (isPrimaryAction)
        {
            auto col = accent();
            if (down) col = col.darker (0.18f);
            else if (over) col = col.brighter (0.08f);

            juce::ColourGradient grad (col.brighter (0.08f), r.getX(), r.getY(),
                                       col.darker (0.18f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRoundedRectangle (r.reduced (2.0f).withHeight (1.2f), 0.6f);
            g.setColour (juce::Colour (0xff050608).withAlpha (0.35f));
            g.drawRoundedRectangle (r, corner, 1.0f);
            return;
        }

        if (isAccent)
        {
            auto col = accent();
            if (down) col = col.darker (0.2f);
            else if (over) col = col.brighter (0.05f);
            g.setColour (col);
            g.fillRoundedRectangle (r, corner);
        }
        else
        {
            auto base = bgCol.getAlpha() == 0 ? raised() : bgCol;
            if (down)      base = base.brighter (0.07f);
            else if (over) base = base.brighter (0.12f);
            juce::ColourGradient grad (base.brighter (0.03f), r.getX(), r.getY(),
                                       base.darker (0.16f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);

            g.setColour (over ? accent().withAlpha (0.4f) : border());
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    void PatchCraftLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                                bool over, bool /*down*/)
    {
        const auto isFlatTab = b.getProperties().getWithDefault ("flatTab", false);
        const auto isWorkflowStep = b.getProperties().getWithDefault ("workflowStep", false);
        const auto isWorkflowProduct = b.getProperties().getWithDefault ("workflowProduct", false);
        const auto isPrimaryAction = b.getProperties().getWithDefault ("primaryAction", false);
        const auto isToolbarIcon = b.getProperties().getWithDefault ("toolbarIcon", false);

        if (isWorkflowStep || isWorkflowProduct)
        {
            auto bounds = b.getLocalBounds().reduced (16, 8).withTrimmedLeft (4);
            const auto buttonText = b.getButtonText();
            const auto headline = buttonText.upToFirstOccurrenceOf ("\n", false, false);
            const auto detail = buttonText.fromFirstOccurrenceOf ("\n", false, false);

            g.setColour (over ? textBright() : text());
            g.setFont (juce::Font ((float) b.getProperties().getWithDefault ("headlineSize", isWorkflowProduct ? 12.4 : 13.2),
                                   juce::Font::bold));
            g.drawFittedText (headline, bounds.removeFromTop (22),
                              juce::Justification::centredLeft, 1);

            if (detail.isNotEmpty())
            {
                g.setColour (over ? text().withAlpha (0.88f) : textDim());
                g.setFont (juce::Font ((float) b.getProperties().getWithDefault ("detailSize", 11.0)));
                g.drawFittedText (detail, bounds, juce::Justification::topLeft, 2);
            }
            return;
        }

        if (isPrimaryAction)
        {
            g.setColour (juce::Colour (0xff111111));
            g.setFont (getTextButtonFont (b, b.getHeight()));
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6),
                              juce::Justification::centred, 2);
            return;
        }

        if (isToolbarIcon)
        {
            g.setColour (over || b.getToggleState() ? accent() : text().withAlpha (0.88f));
            g.setFont (getTextButtonFont (b, b.getHeight()));
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (5, 3),
                              juce::Justification::centred, 1);
            return;
        }

        juce::Colour col;
        if (isFlatTab)
        {
            col = b.getToggleState() ? textBright() : juce::Colour (0xffb8bcc4);
            if (over && ! b.getToggleState()) col = accent();
        }
        else
        {
            col = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId
                                                   : juce::TextButton::textColourOffId);
            if (over && ! b.getToggleState()) col = accent();
        }

        g.setColour (col);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4),
                          juce::Justification::centred, 2);
    }

    juce::Font PatchCraftLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
    {
        const float sz = (float) b.getProperties().getWithDefault ("fontSize", 13.0);
        const auto bold = (bool) b.getProperties().getWithDefault ("bold", false);
        return juce::Font (sz, bold ? juce::Font::bold : juce::Font::plain);
    }

    // -------------------------------------------------------------------------
    // Combo box
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool /*down*/,
                                              int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                                              juce::ComboBox& cb)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        juce::ColourGradient grad (raised().brighter (0.08f), r.getX(), r.getY(),
                                   panel().darker (0.05f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
        g.setColour (cb.isMouseOver() || cb.hasKeyboardFocus (false) ? accent().withAlpha (0.52f) : border());
        g.drawRoundedRectangle (r, 5.0f, cb.hasKeyboardFocus (false) ? 1.3f : 1.0f);

        // arrow
        juce::Path arrow;
        const float ax = w - 16.0f, ay = h * 0.5f;
        arrow.addTriangle (ax, ay - 3.0f, ax + 8.0f, ay - 3.0f, ax + 4.0f, ay + 3.0f);
        g.setColour (cb.isMouseOver() ? accent() : textDim());
        g.fillPath (arrow);
    }

    juce::Font PatchCraftLookAndFeel::getComboBoxFont (juce::ComboBox&)
    { return juce::Font (13.0f); }

    juce::Font PatchCraftLookAndFeel::getLabelFont (juce::Label& l)
    {
        return l.getFont();
    }

    // -------------------------------------------------------------------------
    // Text editor
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
    {
        const bool focused = te.hasKeyboardFocus (true);
        g.setColour (focused ? accent().withAlpha (0.85f) : border().withAlpha (0.95f));
        g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, w - 1.0f, h - 1.0f),
                                5.0f, focused ? 1.35f : 1.0f);
    }

    void PatchCraftLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        juce::ColourGradient grad (raised().withAlpha (0.98f), r.getX(), r.getY(),
                                   panel().darker (0.04f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.0f);
    }

    // -------------------------------------------------------------------------
    // Tabs
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawTabButton (juce::TabBarButton& tab, juce::Graphics& g,
                                               bool over, bool /*down*/)
    {
        auto r = tab.getLocalBounds();
        const bool front = tab.isFrontTab();

        if (front || over)
        {
            auto pill = r.reduced (5, 4).toFloat();
            g.setColour (front ? raised().withAlpha (0.62f) : raised().withAlpha (0.28f));
            g.fillRoundedRectangle (pill, 6.0f);
            g.setColour (front ? accent().withAlpha (0.38f) : borderSoft().withAlpha (0.5f));
            g.drawRoundedRectangle (pill, 6.0f, 0.8f);
        }

        g.setColour (front ? textBright() : (over ? text() : textDim()));
        g.setFont (juce::Font (12.5f, juce::Font::bold));
        g.drawText (tab.getButtonText().toUpperCase(), r, juce::Justification::centred);

        if (front)
        {
            auto underline = r.removeFromBottom (3).reduced (12, 0).toFloat();
            juce::ColourGradient grad (accent().withAlpha (0.0f), underline.getX(), underline.getY(),
                                       accent(), underline.getCentreX(), underline.getY(), false);
            grad.addColour (1.0, accent().withAlpha (0.0f));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (underline, 1.5f);
        }
    }

    int PatchCraftLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& tab, int)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (12.5f, juce::Font::bold),
                                                          tab.getButtonText().toUpperCase()) + 28;
    }

    // -------------------------------------------------------------------------
    // Scrollbar
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&,
                                               int x, int y, int width, int height,
                                               bool vertical, int thumbStart, int thumbSize,
                                               bool over, bool /*down*/)
    {
        juce::Rectangle<int> thumb;
        if (vertical) thumb = { x + 2, thumbStart, juce::jmax (4, width - 4), thumbSize };
        else          thumb = { thumbStart, y + 2, thumbSize, juce::jmax (4, height - 4) };

        g.setColour (over ? accent().withAlpha (0.55f) : border().withAlpha (0.75f));
        g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
    }

    void PatchCraftLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        juce::ColourGradient grad (raised().darker (0.06f), r.getX(), r.getY(),
                                   panel().darker (0.15f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (r);
        g.setColour (accent().withAlpha (0.72f));
        g.fillRect (0, 0, w, 2);
        g.setColour (border().brighter (0.25f));
        g.drawRect (0, 0, w, h, 1);
    }

    void PatchCraftLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                   bool isSeparator, bool isActive, bool isHighlighted,
                                                   bool isTicked, bool hasSubMenu, const juce::String& text,
                                                   const juce::String& shortcutKeyText,
                                                   const juce::Drawable* icon, const juce::Colour* textColourToUse)
    {
        if (isSeparator)
        {
            g.setColour (border().withAlpha (0.75f));
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        auto row = area.reduced (4, 2).toFloat();
        if (isHighlighted && isActive)
        {
            g.setColour (accent().withAlpha (0.20f));
            g.fillRoundedRectangle (row, 5.0f);
            g.setColour (accent().withAlpha (0.70f));
            g.drawRoundedRectangle (row, 5.0f, 1.0f);
        }

        auto textArea = area.reduced (12, 0);
        if (isTicked)
        {
            g.setColour (accent());
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawText ("✓", textArea.removeFromLeft (18), juce::Justification::centred);
        }
        else
        {
            textArea.removeFromLeft (18);
        }

        if (icon != nullptr)
        {
            auto iconArea = textArea.removeFromLeft (22).reduced (2);
            icon->drawWithin (g, iconArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);
        }

        auto colour = textColourToUse != nullptr ? *textColourToUse : PatchCraftLookAndFeel::text();
        if (! isActive)
            colour = textDim().withAlpha (0.65f);
        else if (isHighlighted)
            colour = textBright();

        g.setColour (colour);
        g.setFont (getPopupMenuFont());
        g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (textDim());
            g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
        }

        if (hasSubMenu)
        {
            juce::Path arrow;
            const auto x = (float) area.getRight() - 14.0f;
            const auto y = (float) area.getCentreY();
            arrow.addTriangle (x, y - 4.0f, x, y + 4.0f, x + 5.0f, y);
            g.setColour (isHighlighted ? accent() : textDim());
            g.fillPath (arrow);
        }
    }

    juce::Font PatchCraftLookAndFeel::getPopupMenuFont()
    {
        return juce::Font (13.0f);
    }

    void PatchCraftLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert,
                                              const juce::Rectangle<int>& textArea,
                                              juce::TextLayout& textLayout)
    {
        auto bounds = alert.getLocalBounds().toFloat().reduced (1.0f);
        juce::ColourGradient grad (raised().brighter (0.03f), bounds.getX(), bounds.getY(),
                                   bg().brighter (0.03f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds.reduced (2.0f), 10.0f, 1.0f);
        g.setColour (border().brighter (0.32f));
        g.drawRoundedRectangle (bounds, 10.0f, 1.25f);
        g.setColour (accent());
        g.fillRoundedRectangle (bounds.withHeight (4.0f), 2.0f);

        g.setColour (text());
        auto messageArea = textArea.reduced (8, 0).withTrimmedRight (14);
        textLayout.draw (g, messageArea.toFloat());
    }

    juce::Font PatchCraftLookAndFeel::getAlertWindowTitleFont()
    {
        return juce::Font (20.0f, juce::Font::bold);
    }

    juce::Font PatchCraftLookAndFeel::getAlertWindowMessageFont()
    {
        return juce::Font (15.0f);
    }

    juce::Font PatchCraftLookAndFeel::getAlertWindowFont()
    {
        return juce::Font (13.0f);
    }

    void PatchCraftLookAndFeel::drawMenuBarBackground (juce::Graphics& g, int /*width*/, int /*height*/,
                                                       bool /*isMouseOverBar*/,
                                                       juce::MenuBarComponent& /*menuBar*/)
    {
        g.fillAll (juce::Colour (0xff11161d));
    }

    void PatchCraftLookAndFeel::drawMenuBarItem (juce::Graphics& g, int width, int height,
                                                 int /*itemIndex*/,
                                                 const juce::String& itemText,
                                                 bool isMouseOverItem,
                                                 bool isMenuOpen,
                                                 bool /*isMouseOverBar*/,
                                                 juce::MenuBarComponent& /*menuBar*/)
    {
        if (isMouseOverItem || isMenuOpen)
        {
            g.setColour (accent().withAlpha (0.15f));
            g.fillRect (0, 0, width, height);
        }

        g.setColour (isMouseOverItem || isMenuOpen ? juce::Colours::white : textDim());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (itemText, 0, 0, width, height, juce::Justification::centred);
    }

} // namespace patchcraft
