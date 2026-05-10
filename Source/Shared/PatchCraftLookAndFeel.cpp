#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
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

        setColour (juce::ComboBox::backgroundColourId,       raised());
        setColour (juce::ComboBox::textColourId,             text());
        setColour (juce::ComboBox::outlineColourId,          border());
        setColour (juce::ComboBox::buttonColourId,           accent());
        setColour (juce::ComboBox::arrowColourId,            text());

        setColour (juce::PopupMenu::backgroundColourId,        panel());
        setColour (juce::PopupMenu::textColourId,              text());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId,   accent());

        setColour (juce::Slider::backgroundColourId,         raised());
        setColour (juce::Slider::trackColourId,              border());
        setColour (juce::Slider::thumbColourId,              accent());
        setColour (juce::Slider::rotarySliderFillColourId,   accent());
        setColour (juce::Slider::rotarySliderOutlineColourId,border());
        setColour (juce::Slider::textBoxTextColourId,        text());
        setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);

        setColour (juce::TextEditor::backgroundColourId,     raised());
        setColour (juce::TextEditor::textColourId,           text());
        setColour (juce::TextEditor::highlightColourId,      accent().withAlpha (0.35f));
        setColour (juce::TextEditor::outlineColourId,        border());
        setColour (juce::TextEditor::focusedOutlineColourId, accent());

        setColour (juce::ScrollBar::backgroundColourId,      panel());
        setColour (juce::ScrollBar::thumbColourId,           juce::Colour (0xff343840));
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
        g.setColour (panel());
        g.fillRoundedRectangle (r.toFloat(), corner);
        if (drawBorder)
        {
            g.setColour (border());
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), corner, 1.0f);
        }
    }

    void PatchCraftLookAndFeel::drawDarkPanel (juce::Graphics& g,
                                               juce::Rectangle<int> r, float corner)
    {
        g.setColour (juce::Colour (0xff080a0d));
        g.fillRoundedRectangle (r.toFloat(), corner);
        g.setColour (border());
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), corner, 1.0f);
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
            auto src = strip.getClippedImage (juce::Rectangle<int> (0, frame * fh,
                                                                    strip.getWidth(), fh));
            g.drawImage (src, dest.toFloat(), juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            const int fw = strip.getWidth() / totalFrames;
            auto src = strip.getClippedImage (juce::Rectangle<int> (frame * fw, 0,
                                                                    fw, strip.getHeight()));
            g.drawImage (src, dest.toFloat(), juce::RectanglePlacement::stretchToFit);
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
            auto img = juce::ImageCache::getFromFile (juce::File (stripPath));
            if (img.isValid())
            {
                int frames = (int) slider.getProperties().getWithDefault ("filmstripFrames", 0);
                if (frames <= 0)
                    frames = detectFilmstripFrames (img, true);
                drawFilmstripFrame (g, juce::Rectangle<int> (x, y, w, h),
                                    img, frames, sliderPos, true);
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
            g.setColour (base);
            g.fillRoundedRectangle (r, corner);

            g.setColour (over ? accent().withAlpha (0.4f) : border());
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    void PatchCraftLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                                bool over, bool /*down*/)
    {
        const auto isFlatTab = b.getProperties().getWithDefault ("flatTab", false);
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
        g.setColour (raised());
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cb.isMouseOver() ? accent().withAlpha (0.4f) : border());
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        // arrow
        juce::Path arrow;
        const float ax = w - 16.0f, ay = h * 0.5f;
        arrow.addTriangle (ax, ay - 3.0f, ax + 8.0f, ay - 3.0f, ax + 4.0f, ay + 3.0f);
        g.setColour (textDim());
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
        g.setColour (te.hasKeyboardFocus (true) ? accent() : border());
        g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, w - 1.0f, h - 1.0f), 4.0f, 1.0f);
    }

    void PatchCraftLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
    {
        g.setColour (raised());
        g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), 4.0f);
    }

    // -------------------------------------------------------------------------
    // Tabs
    // -------------------------------------------------------------------------
    void PatchCraftLookAndFeel::drawTabButton (juce::TabBarButton& tab, juce::Graphics& g,
                                               bool over, bool /*down*/)
    {
        auto r = tab.getLocalBounds();
        const bool front = tab.isFrontTab();

        g.setColour (front ? text() : (over ? text() : textDim()));
        g.setFont (juce::Font (12.5f, juce::Font::bold));
        g.drawText (tab.getButtonText().toUpperCase(), r, juce::Justification::centred);

        if (front)
        {
            g.setColour (accent());
            g.fillRect (r.removeFromBottom (2));
        }
    }

    int PatchCraftLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& tab, int)
    {
        return juce::Font (12.5f, juce::Font::bold)
                   .getStringWidth (tab.getButtonText().toUpperCase()) + 28;
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

        g.setColour (over ? juce::Colour (0xff494d56) : juce::Colour (0xff2f323a));
        g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
    }

    void PatchCraftLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        g.fillAll (panel());
        g.setColour (border());
        g.drawRect (0, 0, w, h, 1);
    }

} // namespace patchcraft
