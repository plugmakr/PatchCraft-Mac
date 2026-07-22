#include "PlayerLookAndFeel.h"

namespace patchcraft
{
    PlayerLookAndFeel::PlayerLookAndFeel()
    {
        // Custom color initialization for premium look
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff06070a));
        setColour (juce::DocumentWindow::backgroundColourId, juce::Colour (0xff06070a));
        
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff14161d));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8e6cd6));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffa6acb5));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff11131a));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffe2e5e9));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff202530));
        setColour (juce::ComboBox::buttonColourId, juce::Colour (0xff161a24));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff8e6cd6));
    }

    void PlayerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                              float sliderPos, float startAngle, float endAngle,
                                              juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto rad = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);

        // Get the custom color assigned to this slider, or default to a cool purple
        const auto activeColour = juce::Colour ((juce::uint32) (int) slider.getProperties().getWithDefault ("accentColor", (int) 0xff8e6cd6));

        // 1. Draw track
        const float trackThickness = 3.5f;
        juce::Path trackPath;
        trackPath.addCentredArc (cx, cy, rad - trackThickness * 0.5f, rad - trackThickness * 0.5f,
                                 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff15171d));
        g.strokePath (trackPath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Draw glow arc (soft duplicate)
        if (sliderPos > 0.0f)
        {
            juce::Path activePath;
            activePath.addCentredArc (cx, cy, rad - trackThickness * 0.5f, rad - trackThickness * 0.5f,
                                     0.0f, startAngle, angle, true);
            
            g.setColour (activeColour.withAlpha (0.16f));
            g.strokePath (activePath, juce::PathStrokeType (trackThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour (activeColour);
            g.strokePath (activePath, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Draw central knob body
        const float bodyRadius = rad - trackThickness - 3.5f;
        juce::ColourGradient knobGrad (juce::Colour (0xff1b1d24), cx, cy - bodyRadius,
                                       juce::Colour (0xff0d0e12), cx, cy + bodyRadius, false);
        g.setGradientFill (knobGrad);
        g.fillEllipse (cx - bodyRadius, cy - bodyRadius, bodyRadius * 2, bodyRadius * 2);

        // Knob border
        g.setColour (juce::Colour (0xff20242e));
        g.drawEllipse (cx - bodyRadius, cy - bodyRadius, bodyRadius * 2, bodyRadius * 2, 1.0f);

        // 4. Indicator (small glowing dot near outer edge of body)
        const float dotDist = bodyRadius * 0.70f;
        const float dotRad = 2.2f;
        const float dotX = cx + std::sin (angle) * dotDist;
        const float dotY = cy - std::cos (angle) * dotDist;
        
        g.setColour (activeColour.brighter (0.15f));
        g.fillEllipse (dotX - dotRad, dotY - dotRad, dotRad * 2, dotRad * 2);
        
        // Soft outer glow for dot
        g.setColour (activeColour.withAlpha (0.42f));
        g.drawEllipse (dotX - dotRad - 1.0f, dotY - dotRad - 1.0f, (dotRad + 1.0f) * 2, (dotRad + 1.0f) * 2, 1.0f);
    }

    void PlayerLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                  const juce::Colour& backgroundColour,
                                                  bool shouldDrawButtonAsHighlighted,
                                                  bool shouldDrawButtonAsDown)
    {
        auto bounds = b.getLocalBounds().toFloat();
        const float cornerSize = b.getProperties().getWithDefault ("corner", 5.0f);
        
        const bool isDown = shouldDrawButtonAsDown;
        const bool isHover = shouldDrawButtonAsHighlighted;
        const bool isToggleOn = b.getToggleState();

        juce::Colour baseColour = backgroundColour;
        if (isToggleOn)
            baseColour = b.findColour (juce::TextButton::buttonOnColourId);
        else if (isDown)
            baseColour = baseColour.darker (0.15f);
        else if (isHover)
            baseColour = baseColour.brighter (0.06f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        // Thin glowy border for active/hover states
        if (isToggleOn || isHover)
        {
            g.setColour (baseColour.brighter (0.18f).withAlpha (0.4f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff202532).withAlpha (0.75f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
        }
    }

    void PlayerLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                          int buttonX, int buttonY, int buttonW, int buttonH,
                                          juce::ComboBox& box)
    {
        auto bounds = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        // Draw down arrow icon
        const float arrowX = (float) (width - 20);
        const float arrowY = (float) (height / 2 - 3);
        juce::Path arrow;
        arrow.startNewSubPath (arrowX, arrowY);
        arrow.lineTo (arrowX + 8, arrowY);
        arrow.lineTo (arrowX + 4, arrowY + 6);
        arrow.closeSubPath();

        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }
}
