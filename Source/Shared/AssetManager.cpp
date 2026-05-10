#include "AssetManager.h"

namespace patchcraft
{
    juce::Image AssetManager::loadImage (const juce::File& path)
    {
        if (! path.existsAsFile()) return {};

        auto key = path.getFullPathName();
        if (imageCache.contains (key))
            return imageCache[key];

        auto img = juce::ImageFileFormat::loadFrom (path);
        if (img.isValid())
            imageCache.set (key, img);
        return img;
    }

    void AssetManager::clear()
    {
        imageCache.clear();
    }

    juce::Image AssetManager::renderDefaultHeroImage (int width, int height)
    {
        const int W = juce::jmax (4, width);
        const int H = juce::jmax (4, height);
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);

        // ---- Dark cyberpunk gradient (deep purple -> cyan -> amber) ------
        juce::ColourGradient bg (juce::Colour (0xff0a0b12), 0.0f, 0.0f,
                                 juce::Colour (0xff1a1a2e), 0.0f, (float) H, false);
        bg.addColour (0.35, juce::Colour (0xff16213e));
        bg.addColour (0.60, juce::Colour (0xff0f3460));
        bg.addColour (0.80, juce::Colour (0xff533483));
        bg.addColour (1.0,  juce::Colour (0xffe94560));
        g.setGradientFill (bg);
        g.fillAll();

        // ---- Grid lines (synth aesthetic) ---------------------------------
        {
            const float gridSpacing = 40.0f;
            g.setColour (juce::Colour (0x22ffffff));
            for (float x = 0; x < W; x += gridSpacing)
                g.drawVerticalLine ((int) x, 0, H);
            for (float y = 0; y < H; y += gridSpacing)
                g.drawHorizontalLine ((int) y, 0, W);
        }

        // ---- Glowing hexagon logo in center -------------------------------
        const float cx = W * 0.5f, cy = H * 0.5f;
        const float hexSize = juce::jmin (W, H) * 0.25f;

        // Outer glow
        juce::ColourGradient glow (juce::Colour (0xfff5a623).withAlpha (0.4f), cx, cy,
                                   juce::Colours::transparentBlack, cx + hexSize * 1.5f, cy, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - hexSize * 1.5f, cy - hexSize * 1.5f, hexSize * 3, hexSize * 3);

        // Hexagon shape
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float angle = juce::MathConstants<float>::pi * 2.0f * (i / 6.0f) - juce::MathConstants<float>::halfPi;
            const float x = cx + std::cos (angle) * hexSize;
            const float y = cy + std::sin (angle) * hexSize;
            if (i == 0) hex.startNewSubPath (x, y);
            else hex.lineTo (x, y);
        }
        hex.closeSubPath();

        // Hexagon fill with gradient
        juce::ColourGradient hexGrad (juce::Colour (0xff2a2d35), cx, cy - hexSize,
                                       juce::Colour (0xff141418), cx, cy + hexSize, false);
        g.setGradientFill (hexGrad);
        g.fillPath (hex);

        // Hexagon border
        g.setColour (juce::Colour (0xfff5a623));
        g.strokePath (hex, juce::PathStrokeType (2.5f));

        // ---- "PC" text inside hexagon ------------------------------------
        g.setColour (juce::Colour (0xfff5a623));
        g.setFont (juce::FontOptions (hexSize * 0.7f).withStyle ("bold"));
        g.drawText ("PC", juce::Rectangle<float> (cx - hexSize, cy - hexSize, hexSize * 2, hexSize * 2),
                    juce::Justification::centred);

        // ---- Circuit board traces -----------------------------------------
        juce::Random rng (42);
        for (int i = 0; i < 8; ++i)
        {
            const float startX = rng.nextFloat() * W;
            const float startY = rng.nextFloat() * H;
            const float endX = rng.nextFloat() * W;
            const float endY = rng.nextFloat() * H;

            juce::Path trace;
            trace.startNewSubPath (startX, startY);
            const float midX = (startX + endX) * 0.5f;
            trace.cubicTo (midX, startY, midX, endY, endX, endY);

            g.setColour (juce::Colour (0x33f5a623).withAlpha (0.3f + rng.nextFloat() * 0.4f));
            g.strokePath (trace, juce::PathStrokeType (1.0f + rng.nextFloat() * 2.0f));
        }

        // ---- Scanlines ----------------------------------------------------
        g.setColour (juce::Colour (0x11000000));
        for (float y = 0; y < H; y += 4.0f)
            g.drawHorizontalLine ((int) y, 0, W);

        // ---- Bottom fade to blend with controls section ------------------
        juce::ColourGradient bottomFade (juce::Colours::transparentBlack,
                                         0.0f, (float) H * 0.7f,
                                         juce::Colour (0xff0a0b12),
                                         0.0f, (float) H, false);
        g.setGradientFill (bottomFade);
        g.fillRect (0, (int) (H * 0.7f), W, (int) (H * 0.3f));

        // ---- Vignette ----------------------------------------------------
        juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                       (float) W * 0.5f, (float) H * 0.5f,
                                       juce::Colour (0xaa000000),
                                       0.0f, (float) H, true);
        g.setGradientFill (vignette);
        g.fillAll();

        return img;
    }

} // namespace patchcraft
