#include "StudioInstrumentRenderer.h"
#include "CanvasEditor.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <cmath>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static bool parameterIsEnabledForRenderer (const PatchCraftProject& project, const ParameterDef& parameter)
        {
            if (parameter.enabledBy.isEmpty())
                return true;

            const auto* gate = project.getParameters().find (parameter.enabledBy);
            const float fallback = gate != nullptr ? gate->defaultValue : 0.0f;
            const float value = project.getLiveValues().getValue (parameter.enabledBy, fallback);
            return gate != nullptr && gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
        }

        static juce::String disconnectedControlMessage (const PatchCraftProject& project,
                                                        const LayoutElement& element,
                                                        const ParameterDef* parameter)
        {
            if (element.parameterId.isEmpty())
                return "This control is not connected to a parameter.\nDesign page: select it, open Inspector > DSP Assignment/Parameter, then choose a target parameter.";

            if (parameter == nullptr)
                return "This control points to missing parameter '" + element.parameterId + "'.\nReconnect it in the Inspector or drag a valid DSP Quick Edit parameter onto the canvas.";

            if (! parameterIsEnabledForRenderer (project, *parameter))
                return "This control is currently disabled: "
                    + (parameter->enableHint.isNotEmpty() ? parameter->enableHint
                                                          : ("enable " + parameter->enabledBy + " first."))
                    + "\nConnect or raise the enabling parameter, then this control will move and affect sound.";

            return parameter->name + " (" + parameter->id + ")\nConnected and active.";
        }
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

    static void drawRuntimeLabelText (juce::Graphics& g, juce::Rectangle<int> r,
                                      const LayoutElement& e, const juce::String& valueText)
    {
        if (e.labelPosition == "hidden")
            return;

        auto labelArea = r.removeFromBottom (juce::jmax (20, r.getHeight() / 4));
        labelArea.translate (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY + e.labelSpacing));
        if (e.labelPosition == "top")
            labelArea = r.removeFromTop (juce::jmax (20, r.getHeight() / 4)).translated (juce::roundToInt (e.labelOffsetX),
                                                                                         juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "left")
            labelArea = juce::Rectangle<int> (r.getX() - r.getWidth() / 2 - juce::roundToInt (e.labelSpacing),
                                              r.getCentreY() - 18, r.getWidth() / 2, 36)
                            .translated (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "right")
            labelArea = juce::Rectangle<int> (r.getRight() + juce::roundToInt (e.labelSpacing),
                                              r.getCentreY() - 18, r.getWidth() / 2, 36)
                            .translated (juce::roundToInt (e.labelOffsetX), juce::roundToInt (e.labelOffsetY));

        const auto fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, labelArea.getHeight() * 0.42f);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (fontSize, juce::Font::bold));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : e.parameterId.toUpperCase(),
                    labelArea.removeFromTop (labelArea.getHeight() / 2),
                    juce::Justification::centred, true);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (juce::jmax (8.0f, fontSize * 0.85f)));
        g.drawText (valueText, labelArea, juce::Justification::centred, true);
    }

    StudioInstrumentRenderer::StudioInstrumentRenderer (StudioMainComponent& o)
        : owner (o), project (o.getProject()), assets (o.getAssets())
    {
        setOpaque (true);
        project.addListener (this);
        project.getLiveValues().addListener (this);
        startTimerHz (24);
        rebuild();
    }

    StudioInstrumentRenderer::~StudioInstrumentRenderer()
    {
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void StudioInstrumentRenderer::syncFromDesignerState (const CanvasEditor& canvas)
    {
        const auto nextTabGroup = canvas.getCurrentTabGroup();
        const auto nextPanels = canvas.getActiveTabGroupsByPanel();
        if (currentTabGroup == nextTabGroup && activeTabGroupsByPanel == nextPanels)
        {
            rebuild();
            return;
        }

        currentTabGroup = nextTabGroup;
        activeTabGroupsByPanel = nextPanels;
        rebuild();
    }

    bool StudioInstrumentRenderer::isElementOnCurrentTab (const LayoutElement& e) const
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

    void StudioInstrumentRenderer::rebuild()
    {
        const auto requestedTabGroup = currentTabGroup;
        const auto requestedPanels = activeTabGroupsByPanel;
        activeTabGroupsByPanel.clear();
        knobs.clear();
        knobParamIds.clear();
        knobIndicesByParam.clear();
        elementsCopy.clear();

        elementsCopy = project.getLayout().getAll();

        // Default tab = first one defined in the first TabPanel, or "main",
        // while preserving an already selected tab across rebuilds.
        juce::String defaultTabGroup = "main";
        bool requestedTabStillExists = requestedTabGroup.isNotEmpty() && requestedTabGroup == "main";
        for (auto& e : elementsCopy)
        {
            if (e.type == ElementType::TabPanel && ! e.tabs.isEmpty())
            {
                defaultTabGroup = scopedTabGroupId (e, e.tabs[0]);
                auto active = e.id == "tabs" && requestedTabGroup.isNotEmpty()
                    ? requestedTabGroup
                    : defaultTabGroup;
                if (auto it = requestedPanels.find (e.id);
                    it != requestedPanels.end())
                {
                    for (auto& tab : e.tabs)
                        if (scopedTabGroupId (e, tab) == it->second)
                            active = it->second;
                }
                activeTabGroupsByPanel[e.id] = active;
                for (auto& tab : e.tabs)
                    if (scopedTabGroupId (e, tab) == requestedTabGroup)
                        requestedTabStillExists = true;
            }
        }
        currentTabGroup = requestedTabStillExists ? requestedTabGroup : defaultTabGroup;

        // Load background
        background = juce::Image();
        heroImage = juce::Image();
        auto bgPath = project.getManifest().backgroundImage;
        if (bgPath.isNotEmpty())
        {
            auto f = project.getProjectFolder().getChildFile (bgPath);
            background = assets.loadImage (f);
        }
        if (! background.isValid())
            background = AssetManager::renderDefaultHeroImage (800, 600);

        heroImage = AssetManager::renderDefaultHeroImage (600, 200);

        // Create knobs/sliders for visible elements
        for (auto& e : elementsCopy)
        {
            if (! e.visible || ! isElementOnCurrentTab (e))
            {
                knobs.add (nullptr);
                knobParamIds.push_back ("");
                continue;
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider)
            {
                auto slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle (e.type == ElementType::Knob
                    ? juce::Slider::RotaryHorizontalVerticalDrag
                    : juce::Slider::LinearVertical);
                slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
                slider->setColour (juce::Slider::rotarySliderFillColourId, e.accentColour);
                slider->setColour (juce::Slider::thumbColourId, e.accentColour);
                slider->setAlpha (0.01f);

                const bool showGuidance = project.getManifest().playerShowParameterGuidance;

                // Set range from parameter definition if available
                auto* paramDef = project.getParameters().find (e.parameterId);
                if (paramDef != nullptr)
                {
                    slider->setRange (paramDef->min, paramDef->max, paramDef->step > 0.0f ? paramDef->step : 0.001);
                    slider->setValue (project.getLiveValues().getValue (e.parameterId, paramDef->defaultValue), juce::dontSendNotification);
                    slider->setDoubleClickReturnValue (true, paramDef->defaultValue);
                }
                else
                {
                    slider->setRange (0.0, 1.0, 0.01);
                    slider->setValue (project.getLiveValues().getValue (e.parameterId, 0.5f), juce::dontSendNotification);
                    slider->setDoubleClickReturnValue (true, 0.5);
                }

                const bool connected = e.parameterId.isNotEmpty() && paramDef != nullptr
                    && parameterIsEnabledForRenderer (project, *paramDef);
                slider->setEnabled (connected);
                slider->setTooltip (showGuidance ? disconnectedControlMessage (project, e, paramDef) : juce::String());

                // On change, update LiveValueStore
                slider->onValueChange = [this, paramId = e.parameterId, s = slider.get()]
                {
                    if (paramId.isEmpty() || project.getParameters().find (paramId) == nullptr)
                        return;
                    project.getLiveValues().setValue (paramId, (float) s->getValue());
                };

                addAndMakeVisible (*slider);
                const int knobIndex = knobs.size();
                knobs.add (slider.release());
                knobParamIds.push_back (e.parameterId);
                if (connected)
                    knobIndicesByParam[e.parameterId].push_back (knobIndex);
            }
            else
            {
                knobs.add (nullptr);
                knobParamIds.push_back ("");
            }
        }

        repaint();
        resized();
    }

    void StudioInstrumentRenderer::liveValueChanged (const juce::String& parameterId, float newValue)
    {
        const auto found = knobIndicesByParam.find (parameterId);
        if (found == knobIndicesByParam.end())
            return;

        for (auto i : found->second)
        {
            if (i >= 0 && i < knobs.size() && knobs[i] != nullptr
                && std::abs ((float) knobs[i]->getValue() - newValue) > 0.0001f)
            {
                knobs[i]->setValue (newValue, juce::dontSendNotification);
            }
        }
    }

    void StudioInstrumentRenderer::projectChanged()
    {
        rebuild();
    }

    void StudioInstrumentRenderer::syncKnobsToLiveValues()
    {
        for (int i = 0; i < knobs.size(); ++i)
        {
            if (knobs[i] != nullptr && knobParamIds[i].isNotEmpty())
            {
                float value = project.getLiveValues().getValue (knobParamIds[i], (float) knobs[i]->getValue());
                knobs[i]->setValue (value, juce::dontSendNotification);
            }
        }
    }

    StudioInstrumentRenderer::CanvasMetrics StudioInstrumentRenderer::metrics() const
    {
        CanvasMetrics m;
        auto bounds = getLocalBounds();
        auto canvasW = project.getCanvasSize().width;
        auto canvasH = project.getCanvasSize().height;
        if (canvasW <= 0) canvasW = 1000;
        if (canvasH <= 0) canvasH = 600;

        float scaleX = (float) bounds.getWidth() / (float) canvasW;
        float scaleY = (float) bounds.getHeight() / (float) canvasH;
        m.scale = juce::jmin (scaleX, scaleY);

        int drawW = (int) (canvasW * m.scale);
        int drawH = (int) (canvasH * m.scale);
        m.canvas = bounds.withSizeKeepingCentre (drawW, drawH);
        return m;
    }

    juce::Rectangle<int> StudioInstrumentRenderer::elementRect (const LayoutElement& e, const CanvasMetrics& m) const
    {
        int x = m.canvas.getX() + (int) (e.x * m.scale);
        int y = m.canvas.getY() + (int) (e.y * m.scale);
        int w = (int) (e.width * m.scale);
        int h = (int) (e.height * m.scale);
        return { x, y, w, h };
    }

    juce::Rectangle<int> StudioInstrumentRenderer::animatedElementRect (const LayoutElement& element,
                                                                        juce::Rectangle<int> rect) const
    {
        float amount = element.audioReactive ? juce::jmax (0.08f, element.audioReactiveAmount) * 0.35f : 0.0f;
        if (element.animationMode != "none")
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi
                                                  * juce::jmax (0.05f, element.animationRate)) * 0.5) + 0.5);
            if (element.animationMode == "shake")
            {
                rect.translate (juce::roundToInt ((wave - 0.5f) * 8.0f), 0);
                return rect;
            }
            amount += wave * 0.10f;
        }

        if (amount <= 0.0001f)
            return rect;

        const int grow = juce::roundToInt (juce::jlimit (0.0f, 16.0f, amount * 14.0f));
        return rect.expanded (grow, grow);
    }

    void StudioInstrumentRenderer::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        auto m = metrics();

        // Background
        if (background.isValid())
            g.drawImage (background, m.canvas.toFloat(), juce::RectanglePlacement::stretchToFit);

        // Elements (non-knob types)
        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& e = elementsCopy[i];
            if (! e.visible || ! isElementOnCurrentTab (e)) continue;

            auto r = animatedElementRect (e, elementRect (e, m));
            juce::Graphics::ScopedSaveState opacityState (g);
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity));

            switch (e.type)
            {
                case ElementType::Image:
                {
                    juce::String assetPath = e.asset;
                    if (assetPath.isEmpty() && e.id == "background")
                        assetPath = project.backgroundImageRelative;

                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                            ? juce::File (assetPath)
                            : project.getProjectFolder().getChildFile (assetPath);
                        auto img = assets.loadImage (f);
                        if (img.isValid())
                            g.drawImage (img, r.toFloat(), juce::RectanglePlacement::stretchToFit);
                    }
                    else if (e.id == "background" && background.isValid())
                    {
                        g.drawImage (background, r.toFloat(), juce::RectanglePlacement::stretchToFit);
                    }
                    else
                    {
                        drawHeroPlaceholder (g, r);
                    }
                    break;
                }

                case ElementType::Label:
                    drawLabel (g, r, e.label);
                    break;

                case ElementType::Panel:
                    drawPanel (g, r, e.label);
                    break;

                case ElementType::Shape:
                {
                    auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 1.0f);
                    if (e.shadowAmount > 0.0f)
                    {
                        g.setColour (juce::Colours::black.withAlpha (0.35f * e.shadowAmount));
                        g.fillRoundedRectangle (shapeBounds.translated (5.0f * e.shadowAmount, 7.0f * e.shadowAmount), e.cornerRadius);
                    }
                    if (e.glowAmount > 0.0f)
                    {
                        g.setColour (e.accentColour.withAlpha (0.18f * e.glowAmount));
                        g.fillRoundedRectangle (shapeBounds.expanded (8.0f * e.glowAmount), e.cornerRadius + 8.0f * e.glowAmount);
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
                        path.addRoundedRectangle (shapeBounds, e.cornerRadius);

                    if (e.shapeKind != "line")
                    {
                        g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panel().withAlpha (0.5f) : e.backgroundColour);
                        g.fillPath (path);
                    }
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));
                    break;
                }

                case ElementType::Meter:
                    drawMeter (g, r);
                    break;

                case ElementType::Knob:
                case ElementType::Slider:
                    drawRuntimeControl (g, r.withTrimmedBottom (juce::jmax (20, r.getHeight() / 4)), e);
                    break;

                case ElementType::Button:
                {
                    g.setColour (e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
                    g.setColour (e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.2f);
                    drawLabel (g, r.reduced (6), e.label.isNotEmpty() ? e.label : "Button");
                    break;
                }

                case ElementType::Toggle:
                {
                    const auto* def = project.getParameters().find (e.parameterId);
                    const auto value = project.getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
                    const bool on = value >= 0.5f;
                    auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())).reduced (4);
                    g.setColour (on ? e.accentColour : PatchCraftLookAndFeel::panelAlt());
                    g.fillEllipse (toggle.toFloat());
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.drawEllipse (toggle.toFloat(), 1.0f);
                    drawRuntimeLabelText (g, r, e, on ? "ON" : "OFF");
                    break;
                }

                case ElementType::Dropdown:
                    drawDropdown (g, r, e.parameterId.isEmpty() ? "Select..." : e.parameterId);
                    break;

                case ElementType::ValueDisplay:
                {
                    const auto* def = project.getParameters().find (e.parameterId);
                    const auto value = project.getLiveValues().getValue (e.parameterId, def != nullptr ? def->defaultValue : 0.0f);
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 5.0f);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (10.0f);
                    g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId, r.removeFromTop (16).reduced (5, 0), juce::Justification::centredLeft, true);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.setFont (juce::Font (16.0f, juce::Font::bold));
                    g.drawText (juce::String (value, 2), r.reduced (5, 0), juce::Justification::centredRight, true);
                    break;
                }

                case ElementType::Waveform:
                {
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 4.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    juce::Path wave;
                    wave.startNewSubPath ((float) r.getX(), (float) r.getCentreY());
                    for (int x = 0; x < r.getWidth(); x += 4)
                    {
                        const auto phase = (float) x / juce::jmax (1, r.getWidth()) * juce::MathConstants<float>::twoPi * 3.0f;
                        wave.lineTo ((float) r.getX() + x, (float) r.getCentreY() + std::sin (phase) * (float) r.getHeight() * 0.28f);
                    }
                    g.strokePath (wave, juce::PathStrokeType (1.5f));
                    break;
                }

                case ElementType::Keyboard:
                    drawKeyboard (g, r);
                    break;

                case ElementType::TabPanel:
                    drawTabPanel (g, r, e);
                    break;

                case ElementType::XYPad:
                {
                    float normalised = 0.5f;
                    if (const auto* def = project.getParameters().find (e.parameterId))
                    {
                        const auto value = project.getLiveValues().getValue (def->id, def->defaultValue);
                        normalised = juce::jlimit (0.0f, 1.0f,
                            (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
                    }
                    const auto inner = r.reduced (6);
                    const float dotX = juce::jmap (normalised, 0.0f, 1.0f,
                        (float) inner.getX(), (float) inner.getRight());
                    const float dotY = (float) inner.getCentreY();
                    g.setColour (PatchCraftLookAndFeel::panelAlt());
                    g.fillRoundedRectangle (r.toFloat(), 5.0f);
                    g.setColour (PatchCraftLookAndFeel::border());
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.drawLine ((float) r.getCentreX(), (float) r.getY() + 6.0f, (float) r.getCentreX(), (float) r.getBottom() - 6.0f, 1.0f);
                    g.drawLine ((float) r.getX() + 6.0f, (float) r.getCentreY(), (float) r.getRight() - 6.0f, (float) r.getCentreY(), 1.0f);
                    g.fillEllipse (dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
                    break;
                }

                case ElementType::ScrollPanel:
                    drawPanel (g, r, e.label);
                    break;

                case ElementType::Group:
                    break;

                case ElementType::Separator:
                    g.setColour (e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour);
                    g.drawLine ((float) r.getX(), (float) r.getCentreY(), (float) r.getRight(), (float) r.getCentreY(), juce::jmax (1.0f, e.strokeWidth));
                    break;

                case ElementType::DrumPad:
                case ElementType::PadGrid:
                {
                    const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
                    const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
                    const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
                    const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
                    if (! inner.isEmpty())
                    {
                        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
                        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
                        const auto bg = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;
                        const auto borderC = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
                        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
                        for (int row = 0; row < rows; ++row)
                        {
                            for (int col = 0; col < cols; ++col)
                            {
                                juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                                            (float) inner.getY() + row * (padH + gap),
                                                            padW, padH);
                                g.setColour (bg.brighter (0.04f));
                                g.fillRoundedRectangle (pad, juce::jmax (3.0f, e.cornerRadius * 0.6f));
                                g.setColour (accent.withAlpha (0.55f));
                                g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), 1.0f);
                                const int padIdx = row * cols + col;
                                const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                                g.setColour (PatchCraftLookAndFeel::text().withAlpha (0.85f));
                                g.setFont (juce::Font (juce::jmin (12.0f, padH * 0.28f), juce::Font::bold));
                                const juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                                    ? e.label : juce::String (padIdx + 1);
                                g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f).toNearestInt(),
                                            juce::Justification::centred);
                                g.setColour (PatchCraftLookAndFeel::textDim());
                                g.setFont (juce::jmin (10.0f, padH * 0.22f));
                                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                                            pad.reduced (4.0f).removeFromBottom (padH * 0.35f).toNearestInt(),
                                            juce::Justification::centred);
                                juce::ignoreUnused (borderC);
                            }
                        }
                    }
                    break;
                }

                default:
                    break;
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider)
            {
                const auto* def = project.getParameters().find (e.parameterId);
                juce::String valueText;
                if (def != nullptr)
                {
                    const auto v = project.getLiveValues().getValue (def->id, def->defaultValue);
                    if (def->unit == "Hz" && v >= 1000.0f) valueText = juce::String (v / 1000.0f, 1) + " kHz";
                    else if (def->unit.isNotEmpty())       valueText = juce::String (v, 2) + " " + def->unit;
                    else                                   valueText = juce::String (v, 2);
                }

                drawRuntimeLabelText (g, r, e, valueText);
            }
        }
    }

    void StudioInstrumentRenderer::resized()
    {
        auto m = metrics();

        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& e = elementsCopy[i];
            if (knobs[(int) i] != nullptr && e.visible && isElementOnCurrentTab (e))
            {
                auto bounds = elementRect (e, m);
                if (e.type == ElementType::Knob || e.type == ElementType::Slider)
                    bounds.removeFromBottom (juce::jmax (20, bounds.getHeight() / 4));
                knobs[(int) i]->setBounds (bounds.reduced (2));
            }
        }
    }

    void StudioInstrumentRenderer::mouseDown (const juce::MouseEvent& e)
    {
        if (handleXYPadGesture (e))
            return;

        auto m = metrics();

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            if (! it->visible || it->type != ElementType::TabPanel) continue;
            if (! isElementOnCurrentTab (*it)) continue;
            auto r = elementRect (*it, m);
            if (! r.contains (e.getPosition())) continue;

            const int hit = hitTabIndex (*it, r, e.getPosition());
            if (hit >= 0 && hit < it->tabs.size())
            {
                const auto targetGroup = scopedTabGroupId (*it, it->tabs[hit]);
                if (it->id == "tabs")
                {
                    currentTabGroup = targetGroup;
                    activeTabGroupsByPanel[it->id] = targetGroup;
                }
                else
                    activeTabGroupsByPanel[it->id] = targetGroup;
                rebuild();
                return;
            }
        }

        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& el = elementsCopy[i];
            if (! el.visible || ! isElementOnCurrentTab (el)) continue;

            auto r = elementRect (el, m);
            if (! r.contains (e.getPosition())) continue;

            if (el.type == ElementType::TabPanel)
            {
                int hit = hitTabIndex (el, r, e.getPosition());
                if (hit >= 0 && hit < el.tabs.size())
                {
                    const auto targetGroup = scopedTabGroupId (el, el.tabs[hit]);
                    if (el.id == "tabs")
                    {
                        currentTabGroup = targetGroup;
                        activeTabGroupsByPanel[el.id] = targetGroup;
                    }
                    else
                        activeTabGroupsByPanel[el.id] = targetGroup;
                    rebuild(); // Rebuild to show/hide elements for new tab
                }
            }
            else if (el.type == ElementType::Dropdown && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
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
                        [this, parameterId = el.parameterId, values] (int result)
                        {
                            if (result <= 0 || result > (int) values.size()) return;
                            project.getLiveValues().setValue (parameterId, values[(size_t) result - 1]);
                            repaint();
                        });
                    return;
                }
            }
            else if (el.type == ElementType::Toggle && el.parameterId.isNotEmpty())
            {
                if (auto* def = project.getParameters().find (el.parameterId))
                {
                    const auto current = project.getLiveValues().getValue (el.parameterId, def->defaultValue);
                    project.getLiveValues().setValue (el.parameterId, current >= 0.5f ? def->min : def->max);
                    repaint (r);
                    return;
                }
            }
            else if (el.type == ElementType::Keyboard)
            {
                int note = hitKeyboardNote (r, e.getPosition());
                if (note >= 0 && note != lastPlayedNote)
                {
                    // Release previous note if any
                    if (lastPlayedNote >= 0 && onNoteOff)
                        onNoteOff (lastPlayedNote);

                    lastPlayedNote = note;
                    if (onNoteOn)
                        onNoteOn (note, 0.8f);
                    repaint (r);
                }
            }
        }
    }

    void StudioInstrumentRenderer::mouseDrag (const juce::MouseEvent& e)
    {
        if (handleXYPadGesture (e))
            return;

        if (lastPlayedNote < 0) return;

        auto m = metrics();
        for (size_t i = 0; i < elementsCopy.size(); ++i)
        {
            auto& el = elementsCopy[i];
            if (el.type != ElementType::Keyboard) continue;
            if (! el.visible || ! isElementOnCurrentTab (el)) continue;

            auto r = elementRect (el, m);
            int note = hitKeyboardNote (r, e.getPosition());
            if (note >= 0 && note != lastPlayedNote)
            {
                // Release previous note
                if (onNoteOff)
                    onNoteOff (lastPlayedNote);

                lastPlayedNote = note;
                if (onNoteOn)
                    onNoteOn (note, 0.8f);
                repaint (r);
            }
        }
    }

    void StudioInstrumentRenderer::mouseUp (const juce::MouseEvent&)
    {
        if (lastPlayedNote >= 0 && onNoteOff)
        {
            onNoteOff (lastPlayedNote);
            lastPlayedNote = -1;
            repaint();
        }
    }

    bool StudioInstrumentRenderer::handleXYPadGesture (const juce::MouseEvent& e)
    {
        auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            auto& el = *it;
            if (! el.visible || el.type != ElementType::XYPad || ! isElementOnCurrentTab (el))
                continue;

            auto r = elementRect (el, m);
            if (! r.contains (e.getPosition()))
                continue;

            auto* def = project.getParameters().find (el.parameterId);
            if (def == nullptr)
                return true;

            const auto inner = r.reduced (6);
            const float normalised = juce::jlimit (0.0f, 1.0f,
                (float) (e.getPosition().x - inner.getX()) / (float) juce::jmax (1, inner.getWidth()));
            project.getLiveValues().setValue (def->id, juce::jmap (normalised, 0.0f, 1.0f, def->min, def->max));
            repaint (r.expanded (2));
            return true;
        }
        return false;
    }

    void StudioInstrumentRenderer::timerCallback()
    {
        for (const auto& item : elementsCopy)
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

    int StudioInstrumentRenderer::hitKeyboardNote (juce::Rectangle<int> r, juce::Point<int> p) const
    {
        // Check black keys first (they're on top)
        int numWhiteKeys = 28;
        int whiteKeyW = r.getWidth() / numWhiteKeys;
        int blackKeyW = (int)(whiteKeyW * 0.6f);
        int blackKeyH = (int)(r.getHeight() * 0.65f);

        // Check black keys
        int note = 0;
        for (int i = 0; i < numWhiteKeys; ++i)
        {
            int n = note % 12;
            if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10)
            {
                int x = r.getX() + i * whiteKeyW - blackKeyW / 2;
                juce::Rectangle<int> blackKey (x, r.getY(), blackKeyW, blackKeyH);
                if (blackKey.contains (p))
                    return 48 + note; // Start at C3 (MIDI note 48)
            }
            if (n == 3 || n == 10)
                note += 2;
            else
                note += 1;
        }

        // Check white keys
        int whiteKeyIdx = (p.x - r.getX()) / whiteKeyW;
        if (whiteKeyIdx >= 0 && whiteKeyIdx < numWhiteKeys)
        {
            // Calculate MIDI note from white key index
            // White keys are: C, D, E, F, G, A, B (0, 2, 4, 5, 7, 9, 11 in semitones)
            int octave = whiteKeyIdx / 7;
            int whiteInOctave = whiteKeyIdx % 7;
            int whiteOffsets[] = { 0, 2, 4, 5, 7, 9, 11 };
            return 48 + octave * 12 + whiteOffsets[whiteInOctave];
        }
        return -1;
    }

    //------------------------------------------------------------------------------
    // Drawing helpers

    void StudioInstrumentRenderer::drawHeroPlaceholder (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        if (heroImage.isValid())
            g.drawImage (heroImage, r.toFloat(), juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillRect (r);
            g.setColour (juce::Colours::white);
            g.setFont (14.0f);
            g.drawText ("Hero Art", r, juce::Justification::centred);
        }
    }

    void StudioInstrumentRenderer::drawMeter (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::accent());
        float level = 0.7f; // TODO: Connect to actual audio level
        int fillH = (int) (r.getHeight() * level);
        g.fillRect (r.removeFromBottom (fillH));
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (r, 1);
    }

    void StudioInstrumentRenderer::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label) const
    {
        g.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.5f));
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (r, 1);
        if (label.isNotEmpty())
        {
            g.setFont (12.0f);
            g.drawText (label, r.removeFromTop (20), juce::Justification::left, true);
        }
    }

    void StudioInstrumentRenderer::drawDropdown (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& display) const
    {
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawRect (r, 1);
        g.setFont (12.0f);
        g.drawText (display, r.reduced (4), juce::Justification::centredLeft, true);
        g.drawText ("\u25BC", r.reduced (4), juce::Justification::centredRight, true); // Down arrow
    }

    void StudioInstrumentRenderer::drawKeyboard (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        // Draw simple piano keyboard visualization
        int numWhiteKeys = 28;
        int whiteKeyW = r.getWidth() / numWhiteKeys;
        int blackKeyW = whiteKeyW * 0.6f;
        int blackKeyH = r.getHeight() * 0.65f;

        g.setColour (juce::Colours::white);
        for (int i = 0; i < numWhiteKeys; ++i)
        {
            int x = r.getX() + i * whiteKeyW;
            g.fillRect (x, r.getY(), whiteKeyW - 1, r.getHeight());
        }

        // Black keys
        g.setColour (juce::Colours::black);
        int note = 0;
        for (int i = 0; i < numWhiteKeys; ++i)
        {
            int n = note % 12;
            if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10)
            {
                int x = r.getX() + i * whiteKeyW - blackKeyW / 2;
                g.fillRect (x, r.getY(), blackKeyW, blackKeyH);
            }
            if (n == 3 || n == 10)
                note += 2; // Skip C# and F#
            else
                note += 1;
        }
    }

    void StudioInstrumentRenderer::drawTabPanel (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e) const
    {
        if (e.tabs.isEmpty()) return;

        int tabW = r.getWidth() / (int) e.tabs.size();
        for (int i = 0; i < e.tabs.size(); ++i)
        {
            auto tabR = r.removeFromLeft (tabW);
            const auto group = scopedTabGroupId (e, e.tabs[i]);
            const auto found = activeTabGroupsByPanel.find (e.id);
            const auto activeGroup = found != activeTabGroupsByPanel.end()
                ? found->second
                : (e.id == "tabs" ? currentTabGroup
                                   : scopedTabGroupId (e, e.tabs[0]));
            bool active = (group == activeGroup);
            g.setColour (active ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::panel());
            g.fillRect (tabR);
            g.setColour (PatchCraftLookAndFeel::text());
            g.drawRect (tabR, 1);
            g.setFont (12.0f);
            g.drawText (e.tabs[i], tabR, juce::Justification::centred, true);
        }
    }

    void StudioInstrumentRenderer::drawLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text) const
    {
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (14.0f);
        g.drawText (text, r, juce::Justification::centredLeft, true);
    }

    void StudioInstrumentRenderer::drawRuntimeControl (juce::Graphics& g, juce::Rectangle<int> r, const LayoutElement& e) const
    {
        const auto* def = project.getParameters().find (e.parameterId);
        const auto minValue = def != nullptr ? def->min : 0.0f;
        const auto maxValue = def != nullptr ? def->max : 1.0f;
        const auto fallback = def != nullptr ? def->defaultValue : 0.5f;
        const auto value = project.getLiveValues().getValue (e.parameterId, fallback);
        const auto norm = juce::jlimit (0.0f, 1.0f, (value - minValue) / juce::jmax (0.0001f, maxValue - minValue));
        const auto accent = e.accentColour.isTransparent() ? PatchCraftLookAndFeel::accent() : e.accentColour;
        const auto border = e.borderColour.isTransparent() ? PatchCraftLookAndFeel::border() : e.borderColour;
        const auto fill = e.backgroundColour.isTransparent() ? PatchCraftLookAndFeel::panelAlt() : e.backgroundColour;

        if (e.type == ElementType::Slider)
        {
            auto track = r.reduced (r.getWidth() / 3, 6).toFloat();
            g.setColour (fill);
            g.fillRoundedRectangle (track, 4.0f);
            g.setColour (accent.withAlpha (0.85f));
            auto active = track;
            active.setY (juce::jmap (norm, track.getBottom(), track.getY()));
            g.fillRoundedRectangle (active, 4.0f);
            g.setColour (border);
            g.drawRoundedRectangle (track, 4.0f, 1.0f);
            return;
        }

        auto dial = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                             juce::jmin (r.getWidth(), r.getHeight())).toFloat().reduced (3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (dial.translated (0.0f, 2.0f));
        g.setColour (fill);
        g.fillEllipse (dial);
        g.setColour (border);
        g.drawEllipse (dial, 1.0f);

        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end = juce::MathConstants<float>::pi * 2.75f;
        juce::Path arc;
        arc.addCentredArc (dial.getCentreX(), dial.getCentreY(),
                           dial.getWidth() * 0.43f, dial.getHeight() * 0.43f,
                           0.0f, start, juce::jmap (norm, start, end), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.0f, dial.getWidth() * 0.08f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        const auto angle = juce::jmap (norm, start, end);
        const auto radius = dial.getWidth() * 0.32f;
        const auto centre = dial.getCentre();
        g.drawLine (centre.x, centre.y,
                    centre.x + std::cos (angle) * radius,
                    centre.y + std::sin (angle) * radius,
                    juce::jmax (1.0f, dial.getWidth() * 0.04f));
    }

    int StudioInstrumentRenderer::hitTabIndex (const LayoutElement& e, juce::Rectangle<int> r, juce::Point<int> p) const
    {
        if (e.tabs.isEmpty()) return -1;
        int tabW = r.getWidth() / (int) e.tabs.size();
        int idx = (p.x - r.getX()) / tabW;
        return juce::jlimit (0, (int) e.tabs.size() - 1, idx);
    }

} // namespace patchcraft
