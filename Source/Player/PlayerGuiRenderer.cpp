#include "PlayerGuiRenderer.h"
#include "PluginProcessor.h"
#include "PatchCraftLookAndFeel.h"
#include "LicenseValidator.h"

#include <cmath>

namespace patchcraft
{
    static juce::String slotId (int index)
    {
        return "p" + juce::String (index);
    }

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

    static juce::String playerControlGuidance (const Manifest& manifest,
                                               const LayoutElement& element,
                                               const ParameterDef* parameter)
    {
        if (! manifest.playerShowParameterGuidance)
            return {};

        if (element.parameterId.isEmpty())
            return "This control is not connected to a parameter. The instrument developer must assign it in PatchCraft Studio before export.";

        if (parameter == nullptr)
            return "This control points to missing parameter '" + element.parameterId + "'. The exported layout and parameter registry are out of sync.";

        if (parameter->enabledBy.isNotEmpty())
            return parameter->name + " (" + parameter->id + ")\nIf this appears inactive, enable or raise " + parameter->enabledBy + " first.";

        return parameter->name + " (" + parameter->id + ")";
    }

    const Manifest* PlayerGuiRenderer::manifest() const
    {
        if (const auto* pack = proc.getPack())
            return &pack->manifest;
        return nullptr;
    }

    juce::Colour PlayerGuiRenderer::playerBg() const
    {
        return manifest() != nullptr ? manifest()->playerBackgroundColour : PatchCraftLookAndFeel::bg();
    }

    juce::Colour PlayerGuiRenderer::playerPanel() const
    {
        return manifest() != nullptr ? manifest()->playerPanelColour : juce::Colour (0xff15171b);
    }

    juce::Colour PlayerGuiRenderer::playerAccent() const
    {
        return manifest() != nullptr ? manifest()->playerAccentColour : PatchCraftLookAndFeel::accent();
    }

    juce::Colour PlayerGuiRenderer::playerText() const
    {
        return manifest() != nullptr ? manifest()->playerTextColour : PatchCraftLookAndFeel::text();
    }

    juce::Colour PlayerGuiRenderer::playerTextDim() const
    {
        return manifest() != nullptr ? manifest()->playerTextDimColour : PatchCraftLookAndFeel::textDim();
    }

    juce::Colour PlayerGuiRenderer::playerBorder() const
    {
        return manifest() != nullptr ? manifest()->playerBorderColour : PatchCraftLookAndFeel::border();
    }

    PlayerGuiRenderer::PlayerGuiRenderer (PlayerProcessor& p, AssetManager& a)
        : proc (p), assets (a)
    {
        setOpaque (true);
        startTimerHz (30);
    }

    PlayerGuiRenderer::~PlayerGuiRenderer() = default;

    bool PlayerGuiRenderer::isElementOnCurrentTab (const LayoutElement& e) const
    {
        if (e.groupId.isEmpty()) return true;
        for (const auto& parent : elementsCopy)
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

    int PlayerGuiRenderer::parameterIndexForId (const juce::String& parameterId) const
    {
        return proc.getHostParameterSlotIndex (parameterId);
    }

    const ParameterDef* PlayerGuiRenderer::parameterForId (const juce::String& parameterId) const
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || parameterId.isEmpty())
            return nullptr;

        for (const auto& def : pack->parameters.getAll())
            if (def.id == parameterId)
                return &def;
        return nullptr;
    }

    bool PlayerGuiRenderer::parameterIsEnabled (const ParameterDef& def) const
    {
        if (def.enabledBy.isEmpty())
            return true;

        const auto* gate = parameterForId (def.enabledBy);
        if (gate == nullptr)
            return false;

        const auto value = proc.getPackParameterValue (def.enabledBy);
        return gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
    }

    void PlayerGuiRenderer::refreshControlEnablement()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || controls.isEmpty())
            return;

        for (int i = 0; i < controls.size() && i < (int) elementsCopy.size(); ++i)
        {
            auto* control = controls[i];
            if (control == nullptr)
                continue;

            const auto& element = elementsCopy[(size_t) i];
            const bool isSliderControl = element.type == ElementType::Knob || element.type == ElementType::Slider;
            if (! isSliderControl)
                continue;

            const auto* def = parameterForId (element.parameterId);
            const int slotIndex = parameterIndexForId (element.parameterId);
            const bool mapped = def != nullptr && slotIndex >= 0 && slotIndex < kPatchCraftHostParameterSlots;
            const bool enabled = mapped && parameterIsEnabled (*def);
            control->setEnabled (enabled);
            control->setAlpha (enabled ? juce::jlimit (0.0f, 1.0f, element.opacity)
                                       : juce::jlimit (0.10f, 0.38f, element.opacity));
        }
    }

    float PlayerGuiRenderer::parameterValueForElement (const LayoutElement& element, float fallback) const
    {
        const int index = parameterIndexForId (element.parameterId);
        const auto* def = parameterForId (element.parameterId);
        if (index >= 0 && index < kPatchCraftHostParameterSlots && def != nullptr)
        {
            if (auto* value = proc.getApvts().getRawParameterValue (slotId (index)))
                return juce::jmap (juce::jlimit (0.0f, 1.0f, value->load()), 0.0f, 1.0f, def->min, def->max);
        }
        return def != nullptr ? proc.getPackParameterValue (element.parameterId) : fallback;
    }

    juce::String PlayerGuiRenderer::formattedParameterValue (const LayoutElement& element) const
    {
        const auto* def = parameterForId (element.parameterId);
        if (def == nullptr)
            return element.label.isNotEmpty() ? element.label : element.parameterId;

        const auto value = parameterValueForElement (element, def->defaultValue);
        if (def->displayMode == "toggle" || def->step >= 1.0f)
            return juce::String (juce::roundToInt (value)) + (def->unit.isNotEmpty() ? " " + def->unit : "");
        return juce::String (value, def->unit == "Hz" || def->unit == "ms" ? 1 : 2)
            + (def->unit.isNotEmpty() ? " " + def->unit : "");
    }

    juce::Rectangle<int> PlayerGuiRenderer::animatedElementRect (const LayoutElement& element,
                                                                 juce::Rectangle<int> rect) const
    {
        float amount = 0.0f;
        if (element.audioReactive)
            amount += proc.getOutputPeak() * juce::jmax (0.10f, element.audioReactiveAmount);
        if (element.animationMode != "none")
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi
                                                  * juce::jmax (0.05f, element.animationRate)) * 0.5) + 0.5);
            if (element.animationMode == "shake")
            {
                rect.translate (juce::roundToInt ((wave - 0.5f) * 10.0f), 0);
                return rect;
            }
            amount += wave * (element.animationMode == "breathe" ? 0.05f : 0.08f);
        }

        if (amount <= 0.0001f)
            return rect;

        const int grow = juce::roundToInt (juce::jlimit (0.0f, 18.0f, amount * 14.0f));
        return rect.expanded (grow, grow);
    }

    void PlayerGuiRenderer::rebuild()
    {
        controls.clear();
        attachments.clear();
        elementsCopy.clear();

        const auto* pack = proc.getPack();
        if (pack == nullptr) { background = {}; repaint(); return; }

        elementsCopy = pack->layout.getAll();

        // Cache whether the layout has any element whose paint depends on the
        // live output peak. If not, the timer doesn't need to repaint at all
        // when audio is playing — only when MIDI-learn / control enablement
        // state changes.
        hasMeterOrReactiveElement = false;
        for (const auto& e : elementsCopy)
        {
            if (e.audioReactive
                || e.type == ElementType::Meter
                || e.type == ElementType::Waveform)
            {
                hasMeterOrReactiveElement = true;
                break;
            }
        }

        // Default tab = first one defined in the first TabPanel, or "main".
        currentTabGroup = "main";
        activeTabGroupsByPanel.clear();
        for (auto& e : elementsCopy)
            if (e.type == ElementType::TabPanel && ! e.tabs.isEmpty())
            {
                if (e.id == "tabs")
                    currentTabGroup = LayoutElement::tabLabelToGroupId (e.tabs[0]);
                else
                    activeTabGroupsByPanel[e.id] = scopedTabGroupId (e, e.tabs[0]);
            }

        background = juce::Image();
        heroImage = juce::Image();
        if (pack->backgroundImageRelative.isNotEmpty())
        {
            auto f = pack->rootFolder.getChildFile (pack->backgroundImageRelative);
            background = assets.loadImage (f);
        }
        if (! background.isValid())
            background = AssetManager::renderDefaultHeroImage (
                pack->canvasSize.width, pack->canvasSize.height);

        // Generate hero image for the hero artwork area (1200x360 in template)
        heroImage = AssetManager::renderDefaultHeroImage (1200, 360);

        // Map parameter id -> slot index for APVTS attachments.
        std::map<juce::String, int> paramIndex;
        for (auto& def : pack->parameters.getAll())
        {
            const auto slot = proc.getHostParameterSlotIndex (def.id);
            if (slot >= 0)
                paramIndex[def.id] = slot;
        }

        for (auto& e : elementsCopy)
        {
            if (! e.visible) { controls.add (new juce::Component()); continue; }
            if (e.type == ElementType::Knob || e.type == ElementType::Slider)
            {
                auto slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle (e.type == ElementType::Knob
                    ? juce::Slider::RotaryHorizontalVerticalDrag
                    : juce::Slider::LinearVertical);
                slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
                slider->setColour (juce::Slider::rotarySliderFillColourId, e.accentColour);
                slider->setAlpha (juce::jlimit (0.0f, 1.0f, e.opacity));

                if (e.filmstripAsset.isNotEmpty())
                {
                    juce::File f = juce::File::isAbsolutePath (e.filmstripAsset)
                        ? juce::File (e.filmstripAsset)
                        : pack->rootFolder.getChildFile (e.filmstripAsset);
                    slider->getProperties().set ("filmstripPath",   f.getFullPathName());
                    slider->getProperties().set ("filmstripFrames", e.filmstripFrames);
                }

                slider->addMouseListener (this, false);
                auto it = paramIndex.find (e.parameterId);
                const auto* parameter = [&]() -> const ParameterDef*
                {
                    for (const auto& def : pack->parameters.getAll())
                        if (def.id == e.parameterId)
                            return &def;
                    return nullptr;
                }();
                slider->setTooltip (playerControlGuidance (pack->manifest, e, parameter));
                if (it != paramIndex.end() && it->second < kPatchCraftHostParameterSlots)
                {
                    auto* att = new juce::AudioProcessorValueTreeState::SliderAttachment (
                        proc.getApvts(), slotId (it->second), *slider);
                    attachments.add (att);
                }
                else
                {
                    slider->setEnabled (false);
                    slider->setAlpha (juce::jlimit (0.10f, 0.45f, e.opacity));
                    if (parameter != nullptr)
                        slider->setTooltip (parameter->name + " is not exposed to a Player host automation slot. Assign a host-automatable parameter or reduce exported host parameters.");
                }
                addAndMakeVisible (*slider);
                controls.add (slider.release());
            }
            else
            {
                // Inert placeholder so indexing into controls[] tracks elements 1:1;
                // we paint these types directly.
                auto comp = std::make_unique<juce::Component>();
                comp->setInterceptsMouseClicks (false, false);
                addAndMakeVisible (*comp);
                controls.add (comp.release());
            }
        }
        refreshControlEnablement();
        resized();
        repaint();
    }

    PlayerGuiRenderer::CanvasMetrics PlayerGuiRenderer::metrics() const
    {
        CanvasMetrics m;
        const auto* pack = proc.getPack();
        if (pack == nullptr) { m.scale = 1.0f; m.canvas = {}; return m; }
        const float sx = (float) getWidth()  / (float) pack->canvasSize.width;
        const float sy = (float) getHeight() / (float) pack->canvasSize.height;
        m.scale = juce::jmin (sx, sy);
        const int rw = (int) (pack->canvasSize.width  * m.scale);
        const int rh = (int) (pack->canvasSize.height * m.scale);
        m.canvas = juce::Rectangle<int> ((getWidth()  - rw) / 2,
                                         (getHeight() - rh) / 2, rw, rh);
        return m;
    }

    juce::Rectangle<int> PlayerGuiRenderer::elementRect (const LayoutElement& e,
                                                         const CanvasMetrics& m) const
    {
        return juce::Rectangle<int> (
            m.canvas.getX() + (int) (e.x * m.scale),
            m.canvas.getY() + (int) (e.y * m.scale),
            (int) (e.width  * m.scale),
            (int) (e.height * m.scale));
    }

    // ---- Drawing helpers -----------------------------------------------------
    void PlayerGuiRenderer::drawHeroPlaceholder (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        g.setColour (playerPanel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
    }

    void PlayerGuiRenderer::drawMeter (juce::Graphics& g, juce::Rectangle<int> r,
                                       const LayoutElement& element) const
    {
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        const bool vertical = r.getHeight() > r.getWidth() * 1.2f;
        const int segs = vertical ? 16 : 24;
        const auto inner = r.reduced (4);
        float level = proc.getOutputPeak();
        if (element.parameterId.isNotEmpty())
        {
            if (const auto* def = parameterForId (element.parameterId))
            {
                const auto value = parameterValueForElement (element, def->defaultValue);
                level = juce::jlimit (0.0f, 1.0f, (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
            }
        }
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            juce::Colour c = (t < 0.6f) ? juce::Colour (0xff5fb37b)
                          : (t < 0.85f ? juce::Colour (0xffe8b840)
                                       : juce::Colour (0xffe6504a));
            const float alpha = (t < 0.7f) ? 1.0f : 0.85f;
            const bool lit = t <= level;
            if (vertical)
            {
                const int sh = juce::jmax (2, inner.getHeight() / segs - 1);
                const int sy = inner.getBottom() - (i + 1) * (sh + 1);
                g.setColour (c.withAlpha (lit ? alpha : 0.16f));
                g.fillRect (inner.getX(), sy, inner.getWidth(), sh);
            }
            else
            {
                const int sw = juce::jmax (2, inner.getWidth() / segs - 1);
                const int sx = inner.getX() + i * (sw + 1);
                g.setColour (c.withAlpha (lit ? alpha : 0.16f));
                g.fillRect (sx, inner.getY(), sw, inner.getHeight());
            }
        }
    }

    void PlayerGuiRenderer::drawPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                       const juce::String& label) const
    {
        g.setColour (playerPanel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
        if (label.isNotEmpty())
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (label, r.reduced (8, 4), juce::Justification::topLeft);
        }
    }

    void PlayerGuiRenderer::drawButton (juce::Graphics& g, juce::Rectangle<int> r,
                                        const LayoutElement& e) const
    {
        const bool active = activeMomentaryParameter.isNotEmpty()
            && e.parameterId == activeMomentaryParameter;
        g.setColour ((e.backgroundColour.isTransparent() ? playerPanel().brighter (0.06f) : e.backgroundColour)
                     .withAlpha (active ? 0.92f : 0.78f));
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour ((e.accentColour.isTransparent() ? playerAccent() : e.accentColour).withAlpha (active ? 1.0f : 0.72f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), active ? 2.0f : 1.0f);
        g.setColour (e.textColour.isTransparent() ? playerText() : e.textColour);
        g.setFont (juce::FontOptions (juce::jmax (11.0f, r.getHeight() * 0.32f)).withStyle ("bold"));
        g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId, r.reduced (8, 0), juce::Justification::centred, true);
    }

    void PlayerGuiRenderer::drawValueDisplay (juce::Graphics& g, juce::Rectangle<int> r,
                                              const LayoutElement& e) const
    {
        g.setColour (playerPanel().brighter (0.04f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                    r.withHeight (juce::jmin (18, r.getHeight())).reduced (6, 0),
                    juce::Justification::centredLeft, true);
        g.setColour (playerAccent());
        g.setFont (juce::FontOptions (juce::jmax (12.0f, r.getHeight() * 0.36f)).withStyle ("bold"));
        g.drawText (formattedParameterValue (e), r.reduced (8, 0), juce::Justification::centredRight, true);
    }

    void PlayerGuiRenderer::drawDropdown (juce::Graphics& g, juce::Rectangle<int> r,
                                          const juce::String& display) const
    {
        g.setColour (playerPanel().brighter (0.04f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
        g.setColour (playerText());
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.drawText (display, r.reduced (28, 0), juce::Justification::centred);

        // Arrows
        g.setColour (playerTextDim());
        const float cy = r.getCentreY();
        juce::Path l;  l.addTriangle ((float) r.getX() + 14.0f, cy - 5.0f,
                                       (float) r.getX() + 14.0f, cy + 5.0f,
                                       (float) r.getX() + 8.0f,  cy);
        juce::Path rt; rt.addTriangle ((float) r.getRight() - 14.0f, cy - 5.0f,
                                       (float) r.getRight() - 14.0f, cy + 5.0f,
                                       (float) r.getRight() - 8.0f,  cy);
        g.fillPath (l); g.fillPath (rt);
    }

    void PlayerGuiRenderer::drawKeyboard (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        const int totalKeys = 52;
        const float kw = (float) (r.getWidth() - 8) / (float) totalKeys;
        const float keyTop = (float) r.getY() + 4.0f;
        const float keyH   = (float) r.getHeight() - 8.0f;

        for (int i = 0; i < totalKeys; ++i)
        {
            juce::Rectangle<float> key (r.getX() + 4 + i * kw, keyTop, kw - 1.0f, keyH);
            g.setColour (juce::Colour (0xffe9d8b8));
            g.fillRoundedRectangle (key, 1.5f);
            g.setColour (juce::Colour (0xff8a7958));
            g.drawRoundedRectangle (key, 1.5f, 0.5f);
        }
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

    void PlayerGuiRenderer::drawTabPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e) const
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
            g.setColour (active ? playerText().brighter (0.25f)
                                : juce::Colour (0xffb8bcc4));
            g.setFont (juce::FontOptions (juce::jmax (10.0f, tabRect.getHeight() * 0.42f)).withStyle ("bold"));
            g.drawText (label.toUpperCase(), tabRect.toNearestInt(),
                        juce::Justification::centred);
            if (active)
            {
                g.setColour (playerAccent());
                g.fillRect (tabRect.removeFromBottom (2.0f).toNearestInt());
            }
        }
    }

    void PlayerGuiRenderer::drawXYPad (juce::Graphics& g, juce::Rectangle<int> r,
                                       const LayoutElement& e) const
    {
        const auto backgroundColour = e.backgroundColour.isTransparent()
            ? playerPanel().darker (0.08f)
            : e.backgroundColour;
        const auto borderColour = e.borderColour.isTransparent()
            ? playerBorder()
            : e.borderColour;
        const auto accentColour = e.accentColour.isTransparent()
            ? playerAccent()
            : e.accentColour;

        g.setColour (backgroundColour);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour (borderColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);

        const auto inner = r.reduced (8);
        if (inner.isEmpty())
            return;

        g.setColour (borderColour.withAlpha (0.35f));
        g.drawLine ((float) inner.getCentreX(), (float) inner.getY(), (float) inner.getCentreX(), (float) inner.getBottom(), 1.0f);
        g.drawLine ((float) inner.getX(), (float) inner.getCentreY(), (float) inner.getRight(), (float) inner.getCentreY(), 1.0f);

        float normalised = 0.5f;
        if (const auto* def = parameterForId (e.parameterId))
        {
            const auto range = juce::jmax (0.0001f, def->max - def->min);
            normalised = juce::jlimit (0.0f, 1.0f, (parameterValueForElement (e, def->defaultValue) - def->min) / range);
        }

        const float dotX = juce::jmap (normalised, 0.0f, 1.0f, (float) inner.getX(), (float) inner.getRight());
        const float dotY = (float) inner.getCentreY();
        g.setColour (accentColour.withAlpha (0.22f));
        g.fillEllipse (dotX - 13.0f, dotY - 13.0f, 26.0f, 26.0f);
        g.setColour (accentColour);
        g.fillEllipse (dotX - 5.5f, dotY - 5.5f, 11.0f, 11.0f);

        if (e.label.isNotEmpty() || e.parameterId.isNotEmpty())
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                        r.reduced (8, 4).withHeight (16),
                        juce::Justification::topLeft, true);
        }
    }

    void PlayerGuiRenderer::drawPadGrid (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e) const
    {
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty()) return;

        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.08f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                            (float) inner.getY() + row * (padH + gap),
                                            padW, padH);
                const int padIdx = row * cols + col;
                const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                const bool active = (note == activePadNote);

                g.setColour (active ? accent.withAlpha (0.85f) : bg.brighter (0.04f));
                g.fillRoundedRectangle (pad, juce::jmax (3.0f, e.cornerRadius * 0.6f));
                g.setColour (active ? accent : borderC.withAlpha (0.6f));
                g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), 1.0f);

                g.setColour (active ? juce::Colour (0xff0a0c10) : playerText().withAlpha (0.85f));
                g.setFont (juce::FontOptions (juce::jmin (12.0f, padH * 0.28f)).withStyle ("bold"));
                const juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                    ? e.label : juce::String (padIdx + 1);
                g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f).toNearestInt(),
                            juce::Justification::centred);
                g.setColour (active ? juce::Colour (0xaa0a0c10) : playerTextDim());
                g.setFont (juce::FontOptions (juce::jmin (10.0f, padH * 0.22f)));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                            pad.reduced (4.0f).removeFromBottom (padH * 0.35f).toNearestInt(),
                            juce::Justification::centred);
            }
        }
    }

    int PlayerGuiRenderer::padNoteAt (const LayoutElement& e,
                                      juce::Rectangle<int> r,
                                      juce::Point<int> pos) const
    {
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty() || ! inner.contains (pos)) return -1;

        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
        if (padW <= 0.0f || padH <= 0.0f) return -1;
        const int col = juce::jlimit (0, cols - 1, (int) ((pos.x - inner.getX()) / (padW + gap)));
        const int row = juce::jlimit (0, rows - 1, (int) ((pos.y - inner.getY()) / (padH + gap)));
        return juce::jlimit (0, 127, e.padBaseNote + row * cols + col);
    }

    bool PlayerGuiRenderer::handlePadClick (const juce::MouseEvent& event)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr) return false;

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible) continue;
            if (e.type != ElementType::DrumPad && e.type != ElementType::PadGrid) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (! r.contains (pos)) continue;

            const int note = padNoteAt (e, r, pos);
            if (note < 0) return true;

            // Velocity from vertical click position inside the pad: top = soft,
            // bottom = full. Mirrors how MPC pads respond to pressure.
            const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
            const float yNorm = juce::jlimit (0.0f, 1.0f,
                (float) (pos.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));
            const float velocity = juce::jlimit (0.2f, 1.0f, 0.4f + yNorm * 0.6f);

            if (activePadNote >= 0 && activePadNote != note)
                proc.handleNoteOff (activePadNote);
            activePadNote = note;
            proc.handleNoteOn (note, velocity);
            repaint (r);
            return true;
        }
        return false;
    }

    int PlayerGuiRenderer::hitTabIndex (const LayoutElement& tabPanel,
                                        juce::Rectangle<int> r,
                                        juce::Point<int> pos) const
    {
        if (! r.contains (pos)) return -1;
        const int n = tabPanel.tabs.size();
        if (n <= 0) return -1;
        const float tabW = (float) r.getWidth() / (float) n;
        return juce::jlimit (0, n - 1, (int) ((pos.x - r.getX()) / tabW));
    }

    bool PlayerGuiRenderer::handleXYPadGesture (const juce::MouseEvent& event)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto position = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::XYPad || ! isElementOnCurrentTab (e))
                continue;

            const auto r = elementRect (e, m);
            if (! r.contains (position))
                continue;

            const auto* def = parameterForId (e.parameterId);
            if (def == nullptr || ! parameterIsEnabled (*def))
                return true;

            const auto inner = r.reduced (8);
            const auto width = juce::jmax (1, inner.getWidth());
            const float normalised = juce::jlimit (0.0f, 1.0f,
                (float) (position.x - inner.getX()) / (float) width);
            proc.setPackParameterFromUi (e.parameterId, juce::jmap (normalised, 0.0f, 1.0f, def->min, def->max));
            repaint (r.expanded (2));
            return true;
        }

        return false;
    }

    // ---- Paint --------------------------------------------------------------
    void PlayerGuiRenderer::paint (juce::Graphics& g)
    {
        g.fillAll (playerBg());
        const auto* pack = proc.getPack();
        if (pack == nullptr) return;

        const auto m = metrics();
        if (background.isValid())
            g.drawImage (background, m.canvas.toFloat());

        // Find current preset name for the dropdown display.
        juce::String currentPresetName = pack->manifest.defaultPreset;
        if (currentPresetName.isEmpty() && ! pack->presets.empty())
            currentPresetName = pack->presets.front().name;

        // Draw non-control element types in z-order.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (! isElementOnCurrentTab (e)) continue;

            auto r = animatedElementRect (e, elementRect (e, m));
            juce::Graphics::ScopedSaveState opacityState (g);
            const float reactiveAlpha = e.audioReactive
                ? juce::jlimit (0.0f, 0.28f, proc.getOutputPeak() * juce::jmax (0.1f, e.audioReactiveAmount))
                : 0.0f;
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity + reactiveAlpha));

            // Draw labels for knobs/sliders (they paint themselves via JUCE widgets)
            if (e.type == ElementType::Knob || e.type == ElementType::Slider)
            {
                if (e.label.isNotEmpty())
                {
                    g.setColour (playerText().brighter (0.25f));
                    g.setFont (juce::FontOptions (11.0f));
                    g.drawText (e.label, r.withHeight (20).withBottomY (r.getBottom()),
                               juce::Justification::centred);
                }
                continue;
            }

            switch (e.type)
            {
                case ElementType::Image:
                    if (e.id == "background")
                        break;  // Already painted as full-canvas above

                    {
                        // Branding override: an Image element with id="logo"
                        // pulls from manifest.playerLogoImage if set, so the
                        // same layout works for both the bare PatchCraft
                        // Player and a white-label developer build.
                        juce::String assetPath = e.asset;
                        if (e.id == "logo" && manifest() != nullptr
                            && manifest()->playerLogoImage.isNotEmpty())
                            assetPath = manifest()->playerLogoImage;

                        if (assetPath.isNotEmpty())
                        {
                            auto f = juce::File::isAbsolutePath (assetPath)
                                        ? juce::File (assetPath)
                                        : pack->rootFolder.getChildFile (assetPath);
                            if (auto img = assets.loadImage (f); img.isValid())
                                g.drawImage (img, r.toFloat());
                            else if (heroImage.isValid())
                                g.drawImage (heroImage, r.toFloat());
                        }
                        else if (heroImage.isValid())
                        {
                            // Use hero image for hero element
                            g.drawImage (heroImage, r.toFloat());
                        }
                    }
                    break;

                case ElementType::Label:
                {
                    // Substitute branding tokens for well-known label ids so a
                    // white-label Player can override the visible name without
                    // shipping a re-saved layout. Falls back to the static
                    // label.text from the layout if no branding override.
                    juce::String text = e.label;
                    if (const auto* m = manifest())
                    {
                        if (e.id == "title" && m->playerDisplayName.isNotEmpty())
                            text = m->playerDisplayName;
                        else if (e.id == "tagline" && m->playerTagline.isNotEmpty())
                            text = m->playerTagline;
                    }
                    g.setColour (e.textColour);
                    g.setFont (juce::FontOptions (juce::jmax (12.0f, r.getHeight() * 0.5f)).withStyle ("bold"));
                    g.drawText (text, r, juce::Justification::centredLeft);
                    break;
                }

                case ElementType::Meter:    drawMeter (g, r, e); break;
                case ElementType::Panel:
                case ElementType::ScrollPanel:
                case ElementType::Group:
                    drawPanel (g, r, e.label);
                    break;
                case ElementType::Button:
                    drawButton (g, r, e);
                    break;
                case ElementType::Toggle:
                {
                    const auto* def = parameterForId (e.parameterId);
                    float value = def != nullptr ? parameterValueForElement (e, def->defaultValue) : 0.0f;
                    if (def != nullptr && def->max > def->min)
                        value = (value - def->min) / (def->max - def->min);

                    const bool on = value >= 0.5f;
                    auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 92), juce::jmin (r.getHeight(), 38)).toFloat();
                    g.setColour (on ? e.accentColour.withAlpha (0.85f) : juce::Colour (0xff202329));
                    g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.drawRoundedRectangle (toggle, toggle.getHeight() * 0.5f, 1.0f);
                    const float knobSize = toggle.getHeight() - 8.0f;
                    const float knobX = on ? toggle.getRight() - knobSize - 4.0f : toggle.getX() + 4.0f;
                    g.setColour (playerText().brighter (0.25f));
                    g.fillEllipse (knobX, toggle.getY() + 4.0f, knobSize, knobSize);
                    g.setColour (playerText().brighter (0.25f));
                    g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
                    g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                                r.withTrimmedTop (r.getHeight() / 2), juce::Justification::centred, true);
                    break;
                }
                case ElementType::Shape:
                {
                    auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 1.0f);
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
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));
                    break;
                }
                case ElementType::ValueDisplay:
                    drawValueDisplay (g, r, e);
                    break;
                case ElementType::Separator:
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.drawLine ((float) r.getX(), (float) r.getCentreY(), (float) r.getRight(), (float) r.getCentreY(),
                                juce::jmax (1.0f, e.strokeWidth));
                    break;
                case ElementType::Waveform:
                {
                    g.setColour (playerPanel().darker (0.25f));
                    g.fillRoundedRectangle (r.toFloat(), 4.0f);
                    g.setColour (playerAccent().withAlpha (0.8f));
                    juce::Path wave;
                    const auto bounds = r.reduced (6).toFloat();
                    const auto level = juce::jlimit (0.05f, 1.0f, proc.getOutputPeak() + 0.08f);
                    for (int i = 0; i < 64; ++i)
                    {
                        const float x = bounds.getX() + (float) i / 63.0f * bounds.getWidth();
                        const float y = bounds.getCentreY()
                            + std::sin ((float) i * 0.48f) * bounds.getHeight() * 0.42f * level;
                        if (i == 0) wave.startNewSubPath (x, y);
                        else        wave.lineTo (x, y);
                    }
                    g.strokePath (wave, juce::PathStrokeType (1.5f));
                    break;
                }
                case ElementType::Keyboard: drawKeyboard (g, r); break;
                case ElementType::TabPanel: drawTabPanel (g, r, e); break;
                case ElementType::XYPad:    drawXYPad (g, r, e); break;
                case ElementType::DrumPad:
                case ElementType::PadGrid:
                    drawPadGrid (g, r, e);
                    break;
                case ElementType::Dropdown:
                    drawDropdown (g, r, e.id == "presets" ? currentPresetName
                                     : e.parameterId.isNotEmpty() ? formattedParameterValue (e)
                                                                  : e.label);
                    break;

                default: break;
            }
        }

        if (const auto pending = proc.getPendingMidiLearnParameter(); pending.isNotEmpty())
        {
            auto banner = m.canvas.withHeight (34).reduced (12, 0).translated (0, 10);
            g.setColour (juce::Colour (0xee11151b));
            g.fillRoundedRectangle (banner.toFloat(), 6.0f);
            g.setColour (playerAccent());
            g.drawRoundedRectangle (banner.toFloat(), 6.0f, 1.0f);
            g.setColour (playerText().brighter (0.25f));
            g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
            g.drawText ("MIDI Learn: move a hardware control for " + pending,
                        banner.reduced (12, 0), juce::Justification::centredLeft);
        }

        // License / trial watermark
        if (pack != nullptr)
        {
            LicenseValidator::LicenseInfo info;
            info.licenseKey = pack->manifest.licenseKey;
            info.isTrial = pack->manifest.isTrial;
            info.trialDays = pack->manifest.trialDays;
            info.expiryDate = pack->manifest.trialExpiryDate;
            const auto watermark = LicenseValidator::generateWatermarkText (info);
            if (watermark.isNotEmpty())
            {
                g.setColour (juce::Colours::red.withAlpha (0.6f));
                g.setFont (juce::FontOptions (16.0f).withStyle ("bold"));
                g.drawText (watermark, m.canvas.withHeight (28).withBottomY (m.canvas.getBottom() - 6),
                            juce::Justification::centred, true);
            }
        }
    }

    // ---- Layout -------------------------------------------------------------
    void PlayerGuiRenderer::resized()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || controls.isEmpty()) return;

        const auto m = metrics();

        // Positions controls 1:1 with elementsCopy. Hide controls whose element
        // is not on the active tab (they still exist behind the scenes - this
        // way the SliderAttachments stay alive across tab switches).
        int idx = 0;
        for (auto& e : elementsCopy)
        {
            if (idx >= controls.size()) break;
            auto* c = controls[idx];
            ++idx;

            const bool show = e.visible && isElementOnCurrentTab (e);
            c->setVisible (show);
            if (show) c->setBounds (elementRect (e, m));
        }
    }

    // ---- Mouse --------------------------------------------------------------
    void PlayerGuiRenderer::mouseDown (const juce::MouseEvent& evt)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr) return;
        const auto event = evt.getEventRelativeTo (this);
        if (event.mods.isPopupMenu())
        {
            if (const auto* element = findBindableElementAt (event.getPosition()))
                showControlContextMenu (*element, event.getScreenPosition());
            return;
        }

        if (handleXYPadGesture (event))
            return;

        if (handlePadClick (event))
            return;

        const auto m = metrics();

        // Keyboard click - send MIDI note
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Keyboard) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (r.contains (event.getPosition()))
            {
                handleKeyboardClick (r, event.getPosition());
                return;
            }
        }

        // Tab strip click switches the active group.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::TabPanel) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            const int tabIdx = hitTabIndex (e, r, event.getPosition());
            if (tabIdx >= 0 && tabIdx < e.tabs.size())
            {
                const auto targetGroup = scopedTabGroupId (e, e.tabs[tabIdx]);
                if (e.id == "tabs")
                    currentTabGroup = targetGroup;
                else
                    activeTabGroupsByPanel[e.id] = targetGroup;
                resized();
                repaint();
                return;
            }
        }

        // Dropdown clicks show preset or parameter menus.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Dropdown) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (r.contains (event.getPosition()))
            {
                if (e.id == "presets")
                    showPresetMenu (event.getScreenPosition());
                else
                    showParameterMenu (e, event.getScreenPosition());
                return;
            }
        }

        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Toggle && e.type != ElementType::Button) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (! r.contains (event.getPosition())) continue;

            const auto* def = parameterForId (e.parameterId);
            if (def == nullptr)
                return;
            if (! parameterIsEnabled (*def))
                return;

            const auto current = parameterValueForElement (e, def->defaultValue);
            if (e.type == ElementType::Toggle)
                proc.setPackParameterFromUi (e.parameterId, current >= (def->min + def->max) * 0.5f ? def->min : def->max);
            else
            {
                activeMomentaryParameter = e.parameterId;
                proc.setPackParameterFromUi (e.parameterId, def->max);
            }
            repaint();
            return;
        }
    }

    void PlayerGuiRenderer::mouseDrag (const juce::MouseEvent& evt)
    {
        const auto event = evt.getEventRelativeTo (this);
        if (handleXYPadGesture (event))
            return;
        if (activePadNote >= 0)
            handlePadClick (event);
    }

    void PlayerGuiRenderer::mouseUp (const juce::MouseEvent& evt)
    {
        // Release any held keyboard notes
        if (lastPlayedNote >= 0)
        {
            proc.handleNoteOff (lastPlayedNote);
            lastPlayedNote = -1;
        }
        if (activePadNote >= 0)
        {
            // Drum pads honour `oneShot` in the engine, so the note-off here
            // is harmless for one-shots and correctly releases sustained pads.
            proc.handleNoteOff (activePadNote);
            activePadNote = -1;
            repaint();
        }
        if (activeMomentaryParameter.isNotEmpty())
        {
            if (const auto* def = parameterForId (activeMomentaryParameter))
                proc.setPackParameterFromUi (activeMomentaryParameter, def->min);
            activeMomentaryParameter.clear();
            repaint();
        }
    }

    const LayoutElement* PlayerGuiRenderer::findBindableElementAt (juce::Point<int> position) const
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return nullptr;

        const auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.parameterId.isEmpty())
                continue;
            if (e.type != ElementType::Knob && e.type != ElementType::Slider
                && e.type != ElementType::Toggle && e.type != ElementType::Button
                && e.type != ElementType::Dropdown && e.type != ElementType::ValueDisplay
                && e.type != ElementType::XYPad)
                continue;
            if (! isElementOnCurrentTab (e))
                continue;
            if (elementRect (e, m).contains (position))
                return &e;
        }
        return nullptr;
    }

    void PlayerGuiRenderer::showControlContextMenu (const LayoutElement& element,
                                                    const juce::Point<int>& screenPos)
    {
        if (element.parameterId.isEmpty())
            return;

        juce::PopupMenu menu;
        const auto mapping = proc.getMidiMappingSummary (element.parameterId);
        const bool learnable = proc.isParameterMidiLearnable (element.parameterId);
        const auto* parameter = parameterForId (element.parameterId);
        juce::String prerequisiteId;
        juce::String prerequisiteName;
        float prerequisiteValue = 1.0f;
        if (parameter != nullptr && parameter->enabledBy.isNotEmpty())
        {
            if (const auto* prerequisite = parameterForId (parameter->enabledBy))
            {
                prerequisiteId = prerequisite->id;
                prerequisiteName = prerequisite->name.isNotEmpty() ? prerequisite->name : prerequisite->id;
                prerequisiteValue = prerequisite->displayMode == "toggle"
                    ? prerequisite->max : juce::jmax (prerequisite->defaultValue, prerequisite->min + (prerequisite->max - prerequisite->min) * 0.5f);
            }
        }
        menu.addItem (1, "MIDI Learn", learnable);
        menu.addItem (2, mapping.isNotEmpty() ? "Clear MIDI Mapping (" + mapping + ")" : "Clear MIDI Mapping",
                      mapping.isNotEmpty());
        if (prerequisiteId.isNotEmpty())
        {
            menu.addSeparator();
            menu.addItem (4, "Enable prerequisite: " + prerequisiteName, true);
        }
        menu.addSeparator();
        menu.addItem (3, "Cancel Learn", true);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (220)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, parameterId = element.parameterId, prerequisiteId, prerequisiteValue] (int result)
            {
                if (result == 1)
                {
                    proc.beginMidiLearn (parameterId);
                    repaint();
                }
                else if (result == 2)
                {
                    proc.removeMidiMappingForParameter (parameterId);
                    repaint();
                }
                else if (result == 3)
                {
                    proc.clearMidiLearn();
                    repaint();
                }
                else if (result == 4 && prerequisiteId.isNotEmpty())
                {
                    proc.setPackParameterFromUi (prerequisiteId, prerequisiteValue);
                    refreshControlEnablement();
                    repaint();
                }
            });
    }

    void PlayerGuiRenderer::timerCallback()
    {
        // Only repaint when something meaningful changed. Previously this
        // forced a full-canvas repaint at 30 Hz, which serialised behind
        // every knob/slider drag and caused noticeable lag.
        bool needsRepaint = false;

        const auto pending = proc.getPendingMidiLearnParameter();
        if (pending != lastPendingMidiLearn)
        {
            lastPendingMidiLearn = pending;
            needsRepaint = true;
        }

        refreshControlEnablement();   // updates control enabled state if any

        // If the layout has metering / audio-reactive elements, repaint when
        // the output peak shifted enough to be visible. Otherwise the static
        // controls don't need a tick-driven repaint at all.
        if (hasMeterOrReactiveElement)
        {
            const float peak = proc.getOutputPeak();
            if (std::abs (peak - lastOutputPeak) > 0.01f)
            {
                lastOutputPeak = peak;
                needsRepaint = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    void PlayerGuiRenderer::showParameterMenu (const LayoutElement& element,
                                               const juce::Point<int>& screenPos)
    {
        const auto* def = parameterForId (element.parameterId);
        if (def == nullptr)
            return;

        juce::PopupMenu menu;
        std::vector<float> values;
        if (def->displayMode == "toggle")
        {
            values = { def->min, def->max };
            menu.addItem (1, "Off");
            menu.addItem (2, "On");
        }
        else if (def->step >= 1.0f && def->max - def->min <= 64.0f)
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

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (200)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, parameterId = element.parameterId, values] (int result)
            {
                if (result <= 0 || result > (int) values.size()) return;
                proc.setPackParameterFromUi (parameterId, values[(size_t) result - 1]);
                repaint();
            });
    }

    void PlayerGuiRenderer::handleKeyboardClick (juce::Rectangle<int> r, juce::Point<int> pos)
    {
        const int totalKeys = 52;
        const float kw = (float) (r.getWidth() - 8) / (float) totalKeys;
        const float keyTop = (float) r.getY() + 4.0f;
        const float keyH   = (float) r.getHeight() - 8.0f;
        const float bkH = keyH * 0.62f;
        const float bkW = kw * 0.62f;

        // Check black keys first (they're on top)
        for (int oct = 0; oct < 8; ++oct)
        {
            const int base = oct * 7;
            const int blackOffsets[5] = { 0, 1, 3, 4, 5 };
            for (int b : blackOffsets)
            {
                const float x = r.getX() + 4 + (base + b + 1) * kw - bkW * 0.5f;
                if (x + bkW > r.getRight() - 4) continue;
                juce::Rectangle<float> key (x, keyTop, bkW, bkH);
                if (key.contains (pos.toFloat()))
                {
                    // Black key hit - calculate MIDI note
                    const int whiteKey = base + b + 1;
                    const int midiNote = 36 + oct * 12 + whiteKey;  // Starting from C2
                    proc.handleNoteOn (midiNote);
                    lastPlayedNote = midiNote;
                    return;
                }
            }
        }

        // Check white keys
        for (int i = 0; i < totalKeys; ++i)
        {
            juce::Rectangle<float> key (r.getX() + 4 + i * kw, keyTop, kw - 1.0f, keyH);
            if (key.contains (pos.toFloat()))
            {
                // White key hit - calculate MIDI note
                const int midiNote = 36 + i;  // Starting from C2
                proc.handleNoteOn (midiNote);
                lastPlayedNote = midiNote;
                return;
            }
        }
    }

    void PlayerGuiRenderer::showPresetMenu (const juce::Point<int>& screenPos)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || pack->presets.empty()) return;

        juce::PopupMenu menu;
        for (size_t i = 0; i < pack->presets.size(); ++i)
            menu.addItem ((int) i + 1, pack->presets[i].name);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (200)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this] (int r)
            {
                if (r <= 0) return;
                const auto* p = proc.getPack();
                if (p == nullptr) return;
                const int idx = r - 1;
                if (idx < 0 || idx >= (int) p->presets.size()) return;

                proc.applyPresetByIndex (idx);
                refreshControlEnablement();
                repaint();
            });
    }

} // namespace patchcraft
