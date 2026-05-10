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
        else if (key == "xy")
        {
            g.drawRect (cx - s * 0.40f, cy - s * 0.40f, s * 0.8f, s * 0.8f, t);
            g.fillEllipse (cx - s * 0.10f, cy - s * 0.10f, s * 0.20f, s * 0.20f);
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
            g.drawRoundedRectangle (cx - s * 0.4f, cy - s * 0.3f, s * 0.8f, s * 0.6f, s * 0.15f);
            g.fillEllipse (cx, cy - s * 0.1f, s * 0.2f, s * 0.2f);
        }
        else if (key == "grid")
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g.drawRoundedRectangle (cx - s * 0.35f + j * (s * 0.18f), 
                                            cy - s * 0.25f + i * (s * 0.13f), 
                                            s * 0.15f, s * 0.1f, s * 0.05f);
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
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (title, 6, 0, getWidth() - 12, 22, juce::Justification::centredLeft);
    }

    void ElementPalette::Section::resized()
    {
        int y = 24;
        for (auto* row : rows)
        {
            row->setBounds (4, y, getWidth() - 8, 28);
            y += 30;
        }
    }

    void ElementPalette::Section::addRow (std::unique_ptr<Row> r)
    {
        addAndMakeVisible (*r);
        rows.add (r.release());
    }

    int ElementPalette::Section::getNeededHeight() const
    {
        return 24 + (int) rows.size() * 30 + 4;
    }

    // ---------------------------------------------------------------------
    ElementPalette::ElementPalette (StudioMainComponent& o) : owner (o)
    {
        addAndMakeVisible (addSection);
        addAndMakeVisible (performanceSection);
        addAndMakeVisible (componentsSection);

        struct Entry { ElementType t; juce::String label; juce::String icon; };
        const Entry add[] = {
            { ElementType::Knob,         "Knob",         "knob" },
            { ElementType::Slider,       "Slider",       "slider" },
            { ElementType::Button,       "Button",       "button" },
            { ElementType::Toggle,       "Toggle",       "toggle" },
            { ElementType::Dropdown,     "Dropdown",     "dropdown" },
            { ElementType::Label,        "Label",        "label" },
            { ElementType::ValueDisplay, "Value Display","value" },
            { ElementType::Meter,        "Meter",        "meter" },
            { ElementType::Waveform,     "Waveform",     "waveform" },
            { ElementType::Keyboard,     "Keyboard",     "keyboard" },
            { ElementType::Panel,        "Panel",        "panel" },
            { ElementType::Shape,        "Shape",        "shape" },
            { ElementType::Image,        "Image",        "image" },
            { ElementType::XYPad,        "XY Pad",       "xy" },
            { ElementType::DrumPad,      "Drum Pad",     "drum" },
            { ElementType::PadGrid,      "Pad Grid",     "grid" },
            { ElementType::TabPanel,    "Tab Panel",    "tabs" },
            { ElementType::ScrollPanel, "Scroll Panel", "scroll" },
            { ElementType::Group,       "Group",        "group" },
            { ElementType::Separator,   "Separator",    "separator" }
        };

        for (auto& e : add)
        {
            auto type = e.t;
            addSection.addRow (std::make_unique<Row> (e.label, e.icon,
                [this, type] { addElementOfType (type); }));
        }

        performanceSection.addRow (std::make_unique<Row> ("BPM Sync", "toggle",
            [this] { owner.addElementToCanvas (ElementType::Toggle, "bpmSync"); }));
        performanceSection.addRow (std::make_unique<Row> ("Retrigger", "toggle",
            [this] { owner.addElementToCanvas (ElementType::Toggle, "retrigger"); }));

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
        else if (type == ElementType::DrumPad)    parameterId = "drumTrigger";
        else if (type == ElementType::PadGrid)    parameterId = "padGrid";
        else if (type == ElementType::TabPanel)   parameterId = {};
        else if (type == ElementType::ScrollPanel) parameterId = {};
        else if (type == ElementType::Group)       parameterId = {};
        else if (type == ElementType::Separator)   parameterId = {};
        else if (type == ElementType::Shape)       parameterId = {};

        owner.addElementToCanvas (type, parameterId);
    }

    void ElementPalette::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void ElementPalette::resized()
    {
        auto r = getLocalBounds().reduced (8);
        const int addH = addSection.getNeededHeight();
        addSection.setBounds (r.removeFromTop (addH));
        r.removeFromTop (8);
        const int perfH = performanceSection.getNeededHeight();
        performanceSection.setBounds (r.removeFromTop (perfH));
        r.removeFromTop (8);
        const int compH = componentsSection.getNeededHeight();
        componentsSection.setBounds (r.removeFromTop (compH));

        // bottom action icons
        auto bottom = getLocalBounds().removeFromBottom (32).reduced (8, 4);
        const int iw = bottom.getHeight();
        trashBtn.setBounds  (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        copyBtn.setBounds   (bottom.removeFromLeft (iw));   bottom.removeFromLeft (4);
        folderBtn.setBounds (bottom.removeFromLeft (iw));
    }

} // namespace patchcraft
