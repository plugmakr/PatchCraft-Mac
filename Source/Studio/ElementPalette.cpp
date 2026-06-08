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
    }

    // ---------------------------------------------------------------------
    ElementPalette::Row::Row (juce::String t, juce::String k,
                              std::function<void()> oc)
        : text (std::move (t)), iconKey (std::move (k)), onClick (std::move (oc))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
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
        drawPaletteIcon (g, iconArea, iconKey, col);

        g.setColour (col);
        g.setFont (juce::Font (13.0f));
        g.drawText (text, r.toNearestInt(), juce::Justification::centredLeft);
    }

    void ElementPalette::Row::mouseUp (const juce::MouseEvent& e)
    {
        if (! contains (e.getPosition())) return;
        if (onClick) onClick();
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
        int y = 24;
        for (auto* row : rows)
        {
            row->setVisible (open);
            row->setBounds (4, y, getWidth() - 8, 28);
            y += 30;
        }
    }

    void ElementPalette::Section::mouseUp (const juce::MouseEvent& e)
    {
        if (e.y > 26)
            return;

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
        return 24 + (open ? (int) rows.size() * 30 : 0) + 4;
    }

    // ---------------------------------------------------------------------
    ElementPalette::ElementPalette (StudioMainComponent& o) : owner (o)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&scrollContent, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.setWantsKeyboardFocus (false);

        scrollContent.addAndMakeVisible (controlSection);
        scrollContent.addAndMakeVisible (analysisSection);
        scrollContent.addAndMakeVisible (uiSection);
        scrollContent.addAndMakeVisible (motionSection);
        scrollContent.addAndMakeVisible (proVisualSection);
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

        for (auto* section : { &controlSection, &analysisSection, &uiSection, &motionSection, &proVisualSection, &performanceSection, &containerSection,
                               &productStarterSection,
                               &synthModuleSection, &samplerModuleSection, &drumModuleSection, &midiModuleSection,
                               &eqDynamicsModuleSection, &fxModuleSection, &outputModuleSection })
            section->onToggle = [this] { resized(); repaint(); };

        struct Entry { ElementType t; juce::String label; juce::String icon; };
        const Entry controls[] = {
            { ElementType::Knob,         "Knob",         "knob" },
            { ElementType::Slider,       "Slider",       "slider" },
            { ElementType::Toggle,       "Toggle",       "toggle" },
            { ElementType::XYPad,        "XY Pad",       "xy" },
            { ElementType::MacroControl, "Macro Control","knob" },
            { ElementType::ModMatrix,    "Mod Matrix",   "grid" },
            { ElementType::Mixer,        "Mixer",        "mixer" },
            { ElementType::ValueDisplay, "Value Display","value" },
            { ElementType::Meter,        "Meter",        "meter" },
            { ElementType::Waveform,     "Waveform",     "waveform" },
            { ElementType::Keyboard,     "Keyboard",     "keyboard" },
            { ElementType::GranularField,"Granular Field","granular" }
        };
        const Entry visuals[] = {
            { ElementType::Label,        "Text Label",   "label" },
            { ElementType::Button,       "Button",       "button" },
            { ElementType::Dropdown,     "Dropdown",     "dropdown" },
            { ElementType::Panel,        "Panel",        "panel" },
            { ElementType::Shape,        "Shape",        "shape" },
            { ElementType::Image,        "Image",        "image" }
        };
        const Entry analysis[] = {
            { ElementType::EqCurve,          "EQ Curve",          "eq" },
            { ElementType::SpectrumAnalyzer, "Spectrum Analyzer", "spectrum" }
        };
        const Entry motion[] = {
            { ElementType::ReactiveImage, "Reactive Image", "reactive" },
            { ElementType::SpriteAnimator, "Sprite Animator", "sprite" },
            { ElementType::VisualFxLayer, "Visual FX Layer", "fx" }
        };
        const Entry proVisuals[] = {
            { ElementType::AiVisualPrompt, "AI Visual Prompt", "aiVisual" }
        };
        const Entry performance[] = {
            { ElementType::DrumPad,      "Drum Pad",     "drum" },
            { ElementType::PadGrid,      "Pad Grid",     "grid" },
            { ElementType::DrumGrid,     "Drum Grid",    "grid" }
        };
        const Entry containers[] = {
            { ElementType::TabPanel,    "Tab Panel",    "tabs" },
            { ElementType::ScrollPanel, "Scroll Panel", "scroll" },
            { ElementType::Group,       "Group",        "group" },
            { ElementType::Separator,   "Separator",    "separator" }
        };

        for (auto& e : controls)
        {
            auto type = e.t;
            controlSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : visuals)
        {
            auto type = e.t;
            uiSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : analysis)
        {
            auto type = e.t;
            analysisSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : motion)
        {
            auto type = e.t;
            motionSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : proVisuals)
        {
            auto type = e.t;
            proVisualSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : performance)
        {
            auto type = e.t;
            performanceSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }
        for (auto& e : containers)
        {
            auto type = e.t;
            containerSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }

        productStarterSection.addRow (std::make_unique<Row> ("Synth Plugin Starter", "knob",
            [this] { owner.addModuleToCanvas ("StarterSynthPlugin"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Sampler Instrument Starter", "waveform",
            [this] { owner.addModuleToCanvas ("StarterSamplerInstrument"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Easy Sampler Workstation", "waveform",
            [this] { owner.addModuleToCanvas ("StarterEasySamplerWorkstation"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Vocal Chop Instrument Starter", "grid",
            [this] { owner.addModuleToCanvas ("StarterVocalChopInstrument"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Beat Editing Sampler Starter", "grid",
            [this] { owner.addModuleToCanvas ("StarterBeatEditingSampler"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Drum Machine Starter", "drum",
            [this] { owner.addModuleToCanvas ("StarterDrumMachine"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Scratch / Slice Starter", "xy",
            [this] { owner.addModuleToCanvas ("StarterScratchSlice"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Hip Hop Sampler Starter", "drum",
            [this] { owner.addModuleToCanvas ("StarterHipHopSampler"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Chop Lab Starter", "grid",
            [this] { owner.addModuleToCanvas ("StarterChopLab"); }));
        productStarterSection.addRow (std::make_unique<Row> ("MPC Pad Instrument Starter", "drum",
            [this] { owner.addModuleToCanvas ("StarterMpcPads"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Loop Remix FX Starter", "fx",
            [this] { owner.addModuleToCanvas ("StarterLoopRemixFx"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Chord Progression Plugin Starter", "grid",
            [this] { owner.addModuleToCanvas ("StarterChordProgressionPlugin"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Delay FX Starter", "fx",
            [this] { owner.addModuleToCanvas ("StarterDelayFx"); }));
        productStarterSection.addRow (std::make_unique<Row> ("Vocal / Master FX Starter", "meter",
            [this] { owner.addModuleToCanvas ("StarterVocalMasterFx"); }));

        synthModuleSection.addRow (std::make_unique<Row> ("OSC Stack", "knob",
            [this] { owner.addModuleToCanvas ("OscStack"); }));
        synthModuleSection.addRow (std::make_unique<Row> ("Wavetable Source", "waveform",
            [this] { owner.addModuleToCanvas ("Wavetable"); }));
        synthModuleSection.addRow (std::make_unique<Row> ("Serum-Style Table", "waveform",
            [this] { owner.addModuleToCanvas ("SerumTable"); }));
        synthModuleSection.addRow (std::make_unique<Row> ("Filter Module", "eq",
            [this] { owner.addModuleToCanvas ("Filter"); }));
        synthModuleSection.addRow (std::make_unique<Row> ("ADSR Envelope", "slider",
            [this] { owner.addModuleToCanvas ("ADSR"); }));

        samplerModuleSection.addRow (std::make_unique<Row> ("Sample Player", "waveform",
            [this] { owner.addModuleToCanvas ("SamplePlayer"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Record / Drop Zone", "waveform",
            [this] { owner.addModuleToCanvas ("RecordDropZone"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Slice / Chop", "grid",
            [this] { owner.addModuleToCanvas ("SliceChop"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Vocal Chop Pad", "grid",
            [this] { owner.addModuleToCanvas ("VocalChopPad"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Beat Slice Editor", "grid",
            [this] { owner.addModuleToCanvas ("BeatSliceEditor"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Scratch Deck", "xy",
            [this] { owner.addModuleToCanvas ("ScratchDeck"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Granular Sampler", "granular",
            [this] { owner.addModuleToCanvas ("GranularSampler"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Multisample Keymap", "keyboard",
            [this] { owner.addModuleToCanvas ("MultisampleKeymap"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Chop Grid", "grid",
            [this] { owner.addModuleToCanvas ("ChopGrid"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Loop Slicer", "grid",
            [this] { owner.addModuleToCanvas ("LoopSlicer"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Loop Time Stretch", "waveform",
            [this] { owner.addModuleToCanvas ("LoopTimeStretch"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("MIDI Loop Player", "grid",
            [this] { owner.addModuleToCanvas ("MidiLoopPlayer"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Sample FX Strip", "fx",
            [this] { owner.addModuleToCanvas ("SampleFxStrip"); }));
        samplerModuleSection.addRow (std::make_unique<Row> ("Vinyl Texture Sampler", "waveform",
            [this] { owner.addModuleToCanvas ("VinylTextureSampler"); }));

        drumModuleSection.addRow (std::make_unique<Row> ("Drum Rack", "drum",
            [this] { owner.addModuleToCanvas ("DrumRack"); }));
        drumModuleSection.addRow (std::make_unique<Row> ("Drum Sequencer", "grid",
            [this] { owner.addModuleToCanvas ("DrumSequencer"); }));
        drumModuleSection.addRow (std::make_unique<Row> ("Drum Mixer", "mixer",
            [this] { owner.addModuleToCanvas ("DrumMixer"); }));
        drumModuleSection.addRow (std::make_unique<Row> ("808 Kit Builder", "drum",
            [this] { owner.addModuleToCanvas ("EightOhEightKit"); }));
        drumModuleSection.addRow (std::make_unique<Row> ("Boom Bap Pad Bank", "drum",
            [this] { owner.addModuleToCanvas ("BoomBapPadBank"); }));
        drumModuleSection.addRow (std::make_unique<Row> ("Quick Drum Kit", "drum",
            [this] { owner.addModuleToCanvas ("QuickDrumKit"); }));

        midiModuleSection.addRow (std::make_unique<Row> ("Arp Lane", "grid",
            [this] { owner.addModuleToCanvas ("ArpLaneModule"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Chord Progression Builder", "grid",
            [this] { owner.addModuleToCanvas ("ChordProgressionBuilder"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Scale + Chord Assistant", "keyboard",
            [this] { owner.addModuleToCanvas ("ScaleChordAssistant"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Chord Pad Bank", "drum",
            [this] { owner.addModuleToCanvas ("ChordPadBank"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Voicing + Humanize", "knob",
            [this] { owner.addModuleToCanvas ("VoicingHumanize"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("LFO Module", "reactive",
            [this] { owner.addModuleToCanvas ("LFO"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Step LFO", "grid",
            [this] { owner.addModuleToCanvas ("StepLFO"); }));
        midiModuleSection.addRow (std::make_unique<Row> ("Macro Bank", "knob",
            [this] { owner.addModuleToCanvas ("MacroBank"); }));

        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Surgical EQ", "eq",
            [this] { owner.addModuleToCanvas ("SurgicalEQ"); }));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Dynamic EQ", "eq",
            [this] { owner.addModuleToCanvas ("DynamicEQ"); }));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Compressor", "meter",
            [this] { owner.addModuleToCanvas ("Dynamics"); }));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Limiter", "meter",
            [this] { owner.addModuleToCanvas ("Limiter"); }));
        eqDynamicsModuleSection.addRow (std::make_unique<Row> ("Transient Shaper", "waveform",
            [this] { owner.addModuleToCanvas ("Transient"); }));

        fxModuleSection.addRow (std::make_unique<Row> ("Delay Module", "fx",
            [this] { owner.addModuleToCanvas ("Delay"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("MultiTap Delay", "fx",
            [this] { owner.addModuleToCanvas ("MultiTapDelay"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Reverb Module", "fx",
            [this] { owner.addModuleToCanvas ("Reverb"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Chorus Module", "fx",
            [this] { owner.addModuleToCanvas ("Chorus"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Phaser Module", "fx",
            [this] { owner.addModuleToCanvas ("Phaser"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Flanger Module", "fx",
            [this] { owner.addModuleToCanvas ("Flanger"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Tape Module", "fx",
            [this] { owner.addModuleToCanvas ("Tape"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Lo-Fi Module", "fx",
            [this] { owner.addModuleToCanvas ("LoFi"); }));
        fxModuleSection.addRow (std::make_unique<Row> ("Vocal FX", "fx",
            [this] { owner.addModuleToCanvas ("VocalFX"); }));

        outputModuleSection.addRow (std::make_unique<Row> ("Stereo Module", "mixer",
            [this] { owner.addModuleToCanvas ("Stereo"); }));
        outputModuleSection.addRow (std::make_unique<Row> ("Master Bus", "meter",
            [this] { owner.addModuleToCanvas ("MasterBus"); }));

        performanceSection.addRow (std::make_unique<Row> ("BPM Sync", "toggle",
            [this] { owner.addElementToCanvas (ElementType::Toggle, "bpmSync"); }));
        performanceSection.addRow (std::make_unique<Row> ("Project BPM", "knob",
            [this] { owner.addElementToCanvas (ElementType::Knob, "projectBpm"); }));
        performanceSection.addRow (std::make_unique<Row> ("Retrigger", "toggle",
            [this] { owner.addElementToCanvas (ElementType::Toggle, "retrigger"); }));
        performanceSection.addRow (std::make_unique<Row> ("Mixer Channel", "mixer",
            [this] { owner.addMixerChannelToCanvas(); }));
        performanceSection.addRow (std::make_unique<Row> ("Drum Machine Surface", "grid",
            [this] { owner.addDrumMachineControlsToCanvas(); }));
        performanceSection.addRow (std::make_unique<Row> ("Runtime Sample Library", "library",
            [this] { owner.addElementToCanvas (ElementType::RuntimeSampleLibrary); }));

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
    }

    void ElementPalette::addElementOfType (ElementType type)
    {
        juce::String parameterId;
        if (type == ElementType::Knob)         parameterId = "filterCutoff";
        else if (type == ElementType::Slider)       parameterId = "volume";
        else if (type == ElementType::Toggle)       parameterId = "mix";
        else if (type == ElementType::Dropdown)     parameterId = "oscType";
        else if (type == ElementType::ValueDisplay) parameterId = "filterCutoff";
        else if (type == ElementType::Meter)        parameterId = "volume";
        else if (type == ElementType::Button)      parameterId = "trigger";
        else if (type == ElementType::Label)       parameterId = {};
        else if (type == ElementType::Waveform)    parameterId = {};
        else if (type == ElementType::Keyboard)    parameterId = {};
        else if (type == ElementType::Image)       parameterId = {};
        else if (type == ElementType::XYPad)       parameterId = "xyPosition";
        else if (type == ElementType::GranularField) parameterId = "sampleStart";
        else if (type == ElementType::DrumPad)    parameterId = "drumTrigger";
        else if (type == ElementType::PadGrid)    parameterId = "padGrid";
        else if (type == ElementType::DrumGrid)   parameterId = {};
        else if (type == ElementType::TabPanel)   parameterId = {};
        else if (type == ElementType::MacroControl) parameterId = "filterCutoff";
        else if (type == ElementType::ModMatrix)    parameterId = {};
        else if (type == ElementType::ScrollPanel) parameterId = {};
        else if (type == ElementType::Group)       parameterId = {};
        else if (type == ElementType::Separator)   parameterId = {};
        else if (type == ElementType::Shape)       parameterId = {};
        else if (type == ElementType::ReactiveImage) parameterId = {};
        else if (type == ElementType::SpriteAnimator) parameterId = {};
        else if (type == ElementType::VisualFxLayer) parameterId = {};
        else if (type == ElementType::AiVisualPrompt) parameterId = {};

        owner.addElementToCanvas (type, parameterId);
    }

    void ElementPalette::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void ElementPalette::resized()
    {
        auto outer = getLocalBounds();
        auto bottom = outer.removeFromBottom (36).reduced (8, 4);
        viewport.setBounds (outer);

        auto r = juce::Rectangle<int> (0, 0, juce::jmax (1, viewport.getWidth() - 10), 1).reduced (8, 8);
        const int contentWidth = juce::jmax (1, viewport.getWidth() - 10);
        int contentHeight = 16;

        const int controlH = controlSection.getNeededHeight();
        contentHeight += controlH + 8;
        const int analysisH = analysisSection.getNeededHeight();
        contentHeight += analysisH + 8;
        const int uiH = uiSection.getNeededHeight();
        contentHeight += uiH + 8;
        const int motionH = motionSection.getNeededHeight();
        contentHeight += motionH + 8;
        const int proVisualH = proVisualSection.getNeededHeight();
        contentHeight += proVisualH + 8;
        const int perfH = performanceSection.getNeededHeight();
        contentHeight += perfH + 8;
        const int containerH = containerSection.getNeededHeight();
        contentHeight += containerH + 8;
        const int starterH = productStarterSection.getNeededHeight();
        contentHeight += starterH + 8;
        const int synthModulesH = synthModuleSection.getNeededHeight();
        contentHeight += synthModulesH + 8;
        const int samplerModulesH = samplerModuleSection.getNeededHeight();
        contentHeight += samplerModulesH + 8;
        const int drumModulesH = drumModuleSection.getNeededHeight();
        contentHeight += drumModulesH + 8;
        const int midiModulesH = midiModuleSection.getNeededHeight();
        contentHeight += midiModulesH + 8;
        const int eqDynamicsModulesH = eqDynamicsModuleSection.getNeededHeight();
        contentHeight += eqDynamicsModulesH + 8;
        const int fxModulesH = fxModuleSection.getNeededHeight();
        contentHeight += fxModulesH + 8;
        const int outputModulesH = outputModuleSection.getNeededHeight();
        contentHeight += outputModulesH + 16;

        scrollContent.setBounds (0, 0, contentWidth, juce::jmax (viewport.getHeight(), contentHeight));

        r = scrollContent.getLocalBounds().reduced (8);
        controlSection.setBounds (r.removeFromTop (controlH));
        r.removeFromTop (8);
        analysisSection.setBounds (r.removeFromTop (analysisH));
        r.removeFromTop (8);
        uiSection.setBounds (r.removeFromTop (uiH));
        r.removeFromTop (8);
        motionSection.setBounds (r.removeFromTop (motionH));
        r.removeFromTop (8);
        proVisualSection.setBounds (r.removeFromTop (proVisualH));
        r.removeFromTop (8);
        performanceSection.setBounds (r.removeFromTop (perfH));
        r.removeFromTop (8);
        containerSection.setBounds (r.removeFromTop (containerH));
        r.removeFromTop (8);
        productStarterSection.setBounds (r.removeFromTop (starterH));
        r.removeFromTop (8);
        synthModuleSection.setBounds (r.removeFromTop (synthModulesH));
        r.removeFromTop (8);
        samplerModuleSection.setBounds (r.removeFromTop (samplerModulesH));
        r.removeFromTop (8);
        drumModuleSection.setBounds (r.removeFromTop (drumModulesH));
        r.removeFromTop (8);
        midiModuleSection.setBounds (r.removeFromTop (midiModulesH));
        r.removeFromTop (8);
        eqDynamicsModuleSection.setBounds (r.removeFromTop (eqDynamicsModulesH));
        r.removeFromTop (8);
        fxModuleSection.setBounds (r.removeFromTop (fxModulesH));
        r.removeFromTop (8);
        outputModuleSection.setBounds (r.removeFromTop (outputModulesH));

        // bottom action icons
        const int iw = bottom.getHeight();
        trashBtn.setBounds  (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        copyBtn.setBounds   (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        folderBtn.setBounds (bottom.removeFromLeft (iw));
    }

} // namespace patchcraft
