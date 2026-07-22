#pragma once

#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    /**
        Player runtime visual style. Inherits Studio palette but tweaked for
        the in-DAW plugin window.
    */
    class PlayerLookAndFeel : public PatchCraftLookAndFeel
    {
    public:
        PlayerLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
    };

} // namespace patchcraft
