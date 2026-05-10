#include "KnobBuilderComponent.h"
#include "StudioMainComponent.h"
#include "BuiltAssetLibraryComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <array>
#include <cmath>

namespace patchcraft
{
    class BuilderColourPopup : public juce::Component,
                               private juce::ChangeListener
    {
    public:
        BuilderColourPopup (juce::Colour initialColour, std::function<void (juce::Colour)> colourChanged)
            : onColourChanged (std::move (colourChanged)),
              selector (juce::ColourSelector::showColourAtTop
                        | juce::ColourSelector::showColourspace
                        | juce::ColourSelector::showSliders
                        | juce::ColourSelector::showAlphaChannel)
        {
            selector.setName ("RGBA Colour Picker");
            selector.setCurrentColour (initialColour, juce::dontSendNotification);
            selector.addChangeListener (this);
            addAndMakeVisible (selector);
            setSize (330, 410);
        }

        void resized() override
        {
            selector.setBounds (getLocalBounds().reduced (8));
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            if (onColourChanged)
                onColourChanged (selector.getCurrentColour());
        }

        std::function<void (juce::Colour)> onColourChanged;
        juce::ColourSelector selector;
    };

    static void styleBuilderLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        label.setFont (juce::Font (10.5f, juce::Font::bold));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);
    }

    static void selectComboText (juce::ComboBox& combo, const juce::String& text)
    {
        if (text.isEmpty())
            return;

        for (int i = 0; i < combo.getNumItems(); ++i)
            if (combo.getItemText (i) == text)
            {
                combo.setSelectedId (combo.getItemId (i), juce::dontSendNotification);
                return;
            }
    }

    KnobBuilderComponent::KnobBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        nameEdit.setText (style.name);
        nameEdit.setIndents (8, 4);
        nameEdit.onTextChange = [this] { style.name = nameEdit.getText(); repaint(); };
        addAndMakeVisible (nameEdit);

        configureSlider (sizeSlider, 48, 220, 1, style.size, " px");
        configureSlider (valueSlider, 0.0, 1.0, 0.001, style.previewValue);
        configureSlider (startSlider, -180, 0, 1, style.startAngle, "°");
        configureSlider (endSlider, 0, 180, 1, style.endAngle, "°");
        configureSlider (ringWidthSlider, 2, 22, 0.5, style.ringThickness, " px");
        configureSlider (pointerWidthSlider, 1, 12, 0.5, style.pointerWidth, " px");
        configureSlider (bevelSlider, 0.0, 1.0, 0.01, style.bevel);
        configureSlider (glowSlider, 0.0, 1.0, 0.01, style.glow);
        configureSlider (framesSlider, 1, 256, 1, style.frames);
        configureSlider (importedBaseOpacitySlider, 0.0, 1.0, 0.01, style.importedBaseOpacity);
        configureSlider (overlayOpacitySlider, 0.0, 1.0, 0.01, style.overlayOpacity);

        for (auto* slider : { &sizeSlider, &valueSlider, &startSlider, &endSlider, &ringWidthSlider,
                              &pointerWidthSlider, &bevelSlider, &glowSlider, &framesSlider,
                              &importedBaseOpacitySlider, &overlayOpacitySlider })
        {
            slider->onValueChange = [this] { updateStyleFromControls(); };
            addAndMakeVisible (*slider);
        }
        importedBaseOpacitySlider.setEnabled (false);
        overlayOpacitySlider.setEnabled (false);

        styleBox.addItem ("Modern Concave", 1);
        styleBox.addItem ("Vintage Hardware", 2);
        styleBox.addItem ("Flat Minimal", 3);
        styleBox.addItem ("Glass / Glow", 4);
        styleBox.addItem ("Console Encoder", 5);
        styleBox.setSelectedId (1, juce::dontSendNotification);
        styleBox.onChange = [this] { repaint(); };
        addAndMakeVisible (styleBox);

        indicatorBox.addItem ("Pointer Line", 1);
        indicatorBox.addItem ("Dot", 2);
        indicatorBox.addItem ("Needle", 3);
        indicatorBox.addItem ("Arc Only", 4);
        indicatorBox.setSelectedId (1, juce::dontSendNotification);
        indicatorBox.onChange = [this] { repaint(); };
        addAndMakeVisible (indicatorBox);

        outputBox.addItem ("Vertical PNG Filmstrip", 1);
        outputBox.addItem ("Horizontal PNG Filmstrip", 2);
        outputBox.addItem ("Single SVG Concept", 3);
        outputBox.addItem ("PatchCraft Asset JSON", 4);
        outputBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (outputBox);

        ringToggle.setToggleState (style.ring, juce::dontSendNotification);
        ticksToggle.setToggleState (style.ticks, juce::dontSendNotification);
        shadowToggle.setToggleState (style.shadow, juce::dontSendNotification);
        labelToggle.setToggleState (style.label, juce::dontSendNotification);
        importedBaseToggle.setToggleState (false, juce::dontSendNotification);
        overlayToggle.setToggleState (true, juce::dontSendNotification);
        for (auto* toggle : { &ringToggle, &ticksToggle, &shadowToggle, &labelToggle,
                              &importedBaseToggle, &overlayToggle })
        {
            toggle->onClick = [this] { updateStyleFromControls(); };
            addAndMakeVisible (*toggle);
        }
        importedBaseToggle.setEnabled (false);
        overlayToggle.setEnabled (false);

        importBtn.onClick = [this] { importExistingKnob(); };
        clearImportBtn.onClick = [this] { clearImportedKnob(); };
        clearImportBtn.setEnabled (false);
        addAndMakeVisible (importBtn);
        addAndMakeVisible (clearImportBtn);

        indicatorColourBtn.onClick = [this] { showColourPicker (style.indicator, [this] (juce::Colour c) { style.indicator = c; updateColourButtonText(); repaint(); }); };
        ringColourBtn.onClick = [this] { showColourPicker (style.ringColour, [this] (juce::Colour c) { style.ringColour = c; updateColourButtonText(); repaint(); }); };
        backgroundColourBtn.onClick = [this] { showColourPicker (style.backgroundColour, [this] (juce::Colour c) { style.backgroundColour = c; updateColourButtonText(); repaint(); }); };
        borderColourBtn.onClick = [this] { showColourPicker (style.borderColour, [this] (juce::Colour c) { style.borderColour = c; updateColourButtonText(); repaint(); }); };
        tickColourBtn.onClick = [this] { showColourPicker (style.tickColour, [this] (juce::Colour c) { style.tickColour = c; updateColourButtonText(); repaint(); }); };
        for (auto* button : { &indicatorColourBtn, &ringColourBtn, &backgroundColourBtn, &borderColourBtn, &tickColourBtn })
            addAndMakeVisible (*button);

        exportBtn.onClick = [this] { exportKnobFilmstrip(); };
        exportBtn.getProperties().set ("accent", true);
        exportJsonBtn.setTooltip ("Export an editable PatchCraft knob-builder source JSON.");
        exportJsonBtn.onClick = [this] { exportKnobSourceJson(); };
        addToProjectBtn.setButtonText ("Add To Library");
        addToProjectBtn.setTooltip ("Render this knob into the shared Design page Library.");
        addToProjectBtn.onClick = [this] { addKnobToLibrary(); };
        for (auto* button : { &exportBtn, &exportJsonBtn, &addToProjectBtn })
            addAndMakeVisible (*button);

        styleBuilderLabel (assetLbl, "ASSET");
        styleBuilderLabel (importLbl, "IMPORT / EDIT EXISTING");
        styleBuilderLabel (geometryLbl, "GEOMETRY");
        styleBuilderLabel (paintLbl, "PAINT / LAYERS");
        styleBuilderLabel (behaviorLbl, "BEHAVIOR");
        styleBuilderLabel (exportLbl, "OUTPUT");
        for (auto* label : { &assetLbl, &importLbl, &geometryLbl, &paintLbl, &behaviorLbl, &exportLbl })
            addAndMakeVisible (*label);

        buildLayers = {
            { "Imported Filmstrip Base", true },
            { "Value Arc", true },
            { "Indicator", true },
            { "Tick Marks", true },
            { "Face Gradient", true },
            { "Outer Bezel", true },
            { "Specular Highlight", true },
            { "Shadow", true },
            { "Text Label", true }
        };

        updateColourButtonText();
    }

    void KnobBuilderComponent::configureSlider (juce::Slider& slider, double min, double max, double step,
                                                double value, juce::String suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 58, 22);
        slider.setTextValueSuffix (suffix);
    }

    void KnobBuilderComponent::updateStyleFromControls()
    {
        style.size = (int) sizeSlider.getValue();
        style.previewValue = (float) valueSlider.getValue();
        style.startAngle = (float) startSlider.getValue();
        style.endAngle = (float) endSlider.getValue();
        style.ringThickness = (float) ringWidthSlider.getValue();
        style.pointerWidth = (float) pointerWidthSlider.getValue();
        style.bevel = (float) bevelSlider.getValue();
        style.glow = (float) glowSlider.getValue();
        style.frames = juce::jlimit (1, 256, (int) framesSlider.getValue());
        style.importedBaseOpacity = (float) importedBaseOpacitySlider.getValue();
        style.overlayOpacity = (float) overlayOpacitySlider.getValue();
        style.ring = ringToggle.getToggleState();
        style.ticks = ticksToggle.getToggleState();
        style.shadow = shadowToggle.getToggleState();
        style.label = labelToggle.getToggleState();
        repaint();
    }

    void KnobBuilderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());

        auto bounds = getLocalBounds().reduced (2);
        auto left = bounds.removeFromLeft (190).reduced (6);
        auto right = bounds.removeFromRight (290).reduced (6);
        auto centre = bounds.reduced (6);

        PatchCraftLookAndFeel::drawPanel (g, left, 8.0f);
        PatchCraftLookAndFeel::drawPanel (g, right, 8.0f);
        PatchCraftLookAndFeel::drawDarkPanel (g, centre, 10.0f);

        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText ("Asset Layers", left.removeFromTop (24), juce::Justification::centredLeft);

        buildLayerRows.clear();
        buildLayerRowIndices.clear();
        int y = left.getY() + 4;
        for (int i = 0; i < (int) buildLayers.size(); ++i)
        {
            if (buildLayers[(size_t) i].name == "Imported Filmstrip Base" && ! hasImportedKnob())
                continue;

            auto row = juce::Rectangle<int> (left.getX(), y, left.getWidth(), 24);
            buildLayerRows.push_back (row);
            buildLayerRowIndices.push_back (i);
            const bool selected = i == selectedBuildLayer;
            const bool visible = buildLayers[(size_t) i].visible;
            g.setColour (selected ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
            g.fillRoundedRectangle (row.toFloat(), 5.0f);
            g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (row.toFloat(), 5.0f, 1.0f);
            auto rowText = row.reduced (8, 0);
            g.setColour (visible ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText (visible ? "V" : "H", rowText.removeFromLeft (20), juce::Justification::centred);
            g.setColour (visible ? PatchCraftLookAndFeel::text() : PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (11.0f));
            g.drawText (buildLayers[(size_t) i].name, rowText, juce::Justification::centredLeft);
            y += 28;
        }

        if (selectedBuildLayer >= 0 && selectedBuildLayer < (int) buildLayers.size())
        {
            auto info = juce::Rectangle<int> (left.getX(), left.getBottom() - 54, left.getWidth(), 48);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.82f));
            g.fillRoundedRectangle (info.toFloat(), 6.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (info.toFloat(), 6.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText ("SELECTED LAYER", info.reduced (8).removeFromTop (16), juce::Justification::centredLeft);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (10.5f);
            g.drawText (buildLayers[(size_t) selectedBuildLayer].name + " - click V/H to show or hide",
                        info.reduced (8).withTrimmedTop (16), juce::Justification::centredLeft);
        }

        auto previewHeader = centre.removeFromTop (28);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText ("Live Preview", previewHeader, juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f));
        const int frameCount = style.frames;
        const auto frameText = "Frame " + juce::String ((int) (style.previewValue * (float) juce::jmax (0, frameCount - 1)) + 1)
                             + " / " + juce::String (juce::jmax (1, frameCount));
        g.drawText (hasImportedKnob() ? importedSourceFile.getFileName() + "  |  " + frameText : frameText,
                    previewHeader, juce::Justification::centredRight);

        auto filmstrip = centre.removeFromBottom (76).reduced (4);
        centre.removeFromBottom (8);
        previewKnobBounds = centre.reduced (20);
        drawKnob (g, previewKnobBounds.toFloat(), style.previewValue, false);

        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (filmstrip.toFloat(), 7.0f);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (filmstrip.toFloat(), 7.0f, 1.0f);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.drawText ("FILMSTRIP PREVIEW", filmstrip.removeFromTop (16), juce::Justification::centredLeft);

        const int framesToDraw = 8;
        auto thumbArea = filmstrip.reduced (6, 2);
        const int thumb = juce::jmin (thumbArea.getHeight(), thumbArea.getWidth() / framesToDraw - 4);
        for (int i = 0; i < framesToDraw; ++i)
        {
            auto cell = thumbArea.removeFromLeft (thumb + 8).withSizeKeepingCentre (thumb, thumb);
            drawKnob (g, cell.toFloat(), (float) i / (float) (framesToDraw - 1), true);
        }

        for (int i = 0; i < SectionCount; ++i)
        {
            auto header = sectionHeaderBounds[(size_t) i];
            if (header.isEmpty())
                continue;
            g.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.35f));
            g.fillRoundedRectangle (header.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::borderSoft());
            g.drawRoundedRectangle (header.toFloat(), 4.0f, 1.0f);
        }
    }

    void KnobBuilderComponent::drawKnob (juce::Graphics& g, juce::Rectangle<float> area,
                                         float position, bool compact)
    {
        const float side = juce::jmin (area.getWidth(), area.getHeight(), (float) style.size);
        auto knob = area.withSizeKeepingCentre (side, side);
        const float cx = knob.getCentreX();
        const float cy = knob.getCentreY();
        const float radius = side * 0.5f - 4.0f;
        const float ringW = juce::jlimit (2.0f, radius * 0.25f, style.ringThickness);
        const float startA = juce::degreesToRadians (style.startAngle);
        const float endA = juce::degreesToRadians (style.endAngle);
        const float angle = startA + (endA - startA) * juce::jlimit (0.0f, 1.0f, position);
        const bool drawingImportedBase = hasImportedKnob()
                                      && importedBaseToggle.getToggleState()
                                      && isBuildLayerVisible ("Imported Filmstrip Base");

        if (style.shadow && ! compact && ! drawingImportedBase && isBuildLayerVisible ("Shadow"))
        {
            g.setColour (juce::Colours::black.withAlpha (0.42f));
            g.fillEllipse (knob.translated (0.0f, radius * 0.08f).reduced (radius * 0.1f));
        }

        if (drawingImportedBase)
        {
            auto importedFrame = getImportedFrame (position);
            if (importedFrame.isValid())
            {
                g.setOpacity (style.importedBaseOpacity);
                g.drawImageWithin (importedFrame, (int) knob.getX(), (int) knob.getY(),
                                   (int) knob.getWidth(), (int) knob.getHeight(),
                                   juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
            }

            if (! overlayToggle.getToggleState())
                return;

            g.setOpacity (style.overlayOpacity);
        }

        if (style.ring && isBuildLayerVisible ("Value Arc"))
        {
            juce::Path track;
            track.addCentredArc (cx, cy, radius - ringW * 0.5f, radius - ringW * 0.5f, 0.0f, startA, endA, true);
            g.setColour (PatchCraftLookAndFeel::borderSoft());
            g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path active;
            active.addCentredArc (cx, cy, radius - ringW * 0.5f, radius - ringW * 0.5f, 0.0f, startA, angle, true);
            g.setColour (style.ringColour.withAlpha (0.65f + style.glow * 0.35f));
            g.strokePath (active, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (style.ticks && ! compact && isBuildLayerVisible ("Tick Marks"))
        {
            g.setColour (style.tickColour.withAlpha (0.7f));
            for (int i = 0; i <= 10; ++i)
            {
                const float tickAngle = startA + (endA - startA) * ((float) i / 10.0f);
                const auto inner = juce::Point<float> (cx + std::cos (tickAngle) * (radius + 4.0f),
                                                       cy + std::sin (tickAngle) * (radius + 4.0f));
                const auto outer = juce::Point<float> (cx + std::cos (tickAngle) * (radius + 10.0f),
                                                       cy + std::sin (tickAngle) * (radius + 10.0f));
                g.drawLine ({ inner, outer }, i % 5 == 0 ? 1.4f : 0.8f);
            }
        }

        const float faceRadius = radius - (style.ring ? ringW + 5.0f : 4.0f);
        if (! drawingImportedBase && isBuildLayerVisible ("Face Gradient"))
        {
            juce::ColourGradient face (style.backgroundColour.brighter (style.bevel),
                                       cx - faceRadius * 0.4f, cy - faceRadius,
                                       style.backgroundColour.darker (0.55f),
                                       cx + faceRadius * 0.2f, cy + faceRadius, false);
            g.setGradientFill (face);
            g.fillEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
        }

        if (! drawingImportedBase && isBuildLayerVisible ("Outer Bezel"))
        {
            g.setColour (style.borderColour);
            g.drawEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f, compact ? 1.0f : 1.8f);
        }

        if (style.glow > 0.01f && ! drawingImportedBase && isBuildLayerVisible ("Specular Highlight"))
        {
            g.setColour (style.ringColour.withAlpha (0.10f * style.glow));
            g.fillEllipse (cx - faceRadius * 0.82f, cy - faceRadius * 0.82f, faceRadius * 1.64f, faceRadius * 1.64f);
        }

        if (! compact && ! drawingImportedBase && isBuildLayerVisible ("Specular Highlight"))
        {
            juce::ColourGradient highlight (juce::Colours::white.withAlpha (0.20f),
                                            cx - faceRadius * 0.35f, cy - faceRadius * 0.6f,
                                            juce::Colours::white.withAlpha (0.0f),
                                            cx + faceRadius * 0.3f, cy + faceRadius * 0.45f,
                                            true);
            g.setGradientFill (highlight);
            g.fillEllipse (cx - faceRadius * 0.78f, cy - faceRadius * 0.78f,
                           faceRadius * 1.56f, faceRadius * 1.56f);
        }

        if (indicatorBox.getSelectedId() != 4 && isBuildLayerVisible ("Indicator"))
        {
            g.setColour (style.indicator);
            if (indicatorBox.getSelectedId() == 2)
            {
                const auto dot = juce::Point<float> (cx + std::cos (angle) * faceRadius * 0.62f,
                                                     cy + std::sin (angle) * faceRadius * 0.62f);
                g.fillEllipse (dot.x - style.pointerWidth, dot.y - style.pointerWidth,
                               style.pointerWidth * 2.0f, style.pointerWidth * 2.0f);
            }
            else
            {
                juce::Path pointer;
                const float pointerLength = indicatorBox.getSelectedId() == 3 ? faceRadius * 0.92f : faceRadius * 0.68f;
                pointer.addRoundedRectangle (-style.pointerWidth * 0.5f, -pointerLength,
                                             style.pointerWidth, pointerLength * 0.72f, style.pointerWidth * 0.5f);
                pointer.applyTransform (juce::AffineTransform::rotation (angle + juce::MathConstants<float>::halfPi).translated (cx, cy));
                g.fillPath (pointer);
            }
        }

        if (style.label && ! compact && isBuildLayerVisible ("Text Label"))
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (style.name, knob.translated (0.0f, side * 0.52f).withHeight (18.0f),
                        juce::Justification::centred);
        }

        if (drawingImportedBase)
            g.setOpacity (1.0f);
    }

    void KnobBuilderComponent::resized()
    {
        juce::Component* editableControls[] {
            &nameEdit, &styleBox, &importBtn, &clearImportBtn,
            &importedBaseToggle, &overlayToggle, &importedBaseOpacitySlider, &overlayOpacitySlider,
            &sizeSlider, &valueSlider, &startSlider, &endSlider, &ringWidthSlider,
            &pointerWidthSlider, &indicatorBox, &indicatorColourBtn, &ringColourBtn,
            &backgroundColourBtn, &borderColourBtn, &tickColourBtn, &bevelSlider, &glowSlider,
            &ringToggle, &ticksToggle, &shadowToggle, &labelToggle, &framesSlider, &outputBox,
            &exportBtn, &exportJsonBtn, &addToProjectBtn
        };

        for (auto* component : editableControls)
            component->setVisible (false);

        auto bounds = getLocalBounds().reduced (2);
        bounds.removeFromLeft (190);
        auto right = bounds.removeFromRight (290).reduced (16, 12);

        auto putHeader = [&] (juce::Label& label, SectionIndex index)
        {
            auto header = right.removeFromTop (22);
            sectionHeaderBounds[(size_t) index] = header;
            const juce::String titles[] { "ASSET", "IMPORT / EDIT EXISTING", "GEOMETRY", "PAINT / LAYERS", "BEHAVIOR", "OUTPUT" };
            label.setText ((sectionOpen[(size_t) index] ? "v  " : ">  ") + titles[(int) index], juce::dontSendNotification);
            label.setBounds (header);
            right.removeFromTop (4);
        };
        auto putRow = [&] (juce::Component& component)
        {
            auto row = right.removeFromTop (25);
            component.setVisible (true);
            component.setBounds (row);
            right.removeFromTop (5);
        };

        putHeader (assetLbl, AssetSection);
        if (sectionOpen[(size_t) AssetSection])
        {
            putRow (nameEdit);
            putRow (styleBox);
        }

        right.removeFromTop (4);
        putHeader (importLbl, ImportSection);
        if (sectionOpen[(size_t) ImportSection])
        {
            auto importRow = right.removeFromTop (30);
            importBtn.setVisible (true);
            clearImportBtn.setVisible (true);
            importBtn.setBounds (importRow.removeFromLeft (130).reduced (2));
            clearImportBtn.setBounds (importRow.reduced (2));
            importedBaseToggle.setVisible (true);
            overlayToggle.setVisible (true);
            importedBaseToggle.setBounds (right.removeFromTop (24));
            overlayToggle.setBounds (right.removeFromTop (24));
            putRow (importedBaseOpacitySlider);
            putRow (overlayOpacitySlider);
        }

        right.removeFromTop (4);
        putHeader (geometryLbl, GeometrySection);
        if (sectionOpen[(size_t) GeometrySection])
        {
            putRow (sizeSlider);
            putRow (valueSlider);
            putRow (startSlider);
            putRow (endSlider);
            putRow (ringWidthSlider);
            putRow (pointerWidthSlider);
        }

        right.removeFromTop (6);
        putHeader (paintLbl, PaintSection);
        if (sectionOpen[(size_t) PaintSection])
        {
            putRow (indicatorBox);
            auto colourRow = right.removeFromTop (58);
            for (auto* button : { &indicatorColourBtn, &ringColourBtn, &backgroundColourBtn, &borderColourBtn, &tickColourBtn })
                button->setVisible (true);
            indicatorColourBtn.setBounds (colourRow.removeFromLeft (86).reduced (2));
            ringColourBtn.setBounds (colourRow.removeFromLeft (66).reduced (2));
            backgroundColourBtn.setBounds (colourRow.removeFromLeft (66).reduced (2));
            borderColourBtn.setBounds (colourRow.removeFromLeft (66).reduced (2));
            tickColourBtn.setBounds (right.removeFromTop (26).reduced (2));
            right.removeFromTop (4);
            putRow (bevelSlider);
            putRow (glowSlider);
        }

        right.removeFromTop (6);
        putHeader (behaviorLbl, BehaviorSection);
        if (sectionOpen[(size_t) BehaviorSection])
        {
            for (auto* toggle : { &ringToggle, &ticksToggle, &shadowToggle, &labelToggle })
                toggle->setVisible (true);
            auto toggles = right.removeFromTop (56);
            ringToggle.setBounds (toggles.removeFromTop (26).removeFromLeft (126));
            ticksToggle.setBounds (toggles.removeFromLeft (126));
            shadowToggle.setBounds (toggles.removeFromLeft (126));
            labelToggle.setBounds (right.removeFromTop (26).removeFromLeft (126));
            putRow (framesSlider);
        }

        right.removeFromTop (6);
        putHeader (exportLbl, OutputSection);
        if (sectionOpen[(size_t) OutputSection])
        {
            putRow (outputBox);
            auto actions = right.removeFromTop (30);
            for (auto* button : { &exportBtn, &exportJsonBtn, &addToProjectBtn })
                button->setVisible (true);
            exportBtn.setBounds (actions.removeFromLeft (116).reduced (2));
            exportJsonBtn.setBounds (actions.removeFromLeft (88).reduced (2));
            addToProjectBtn.setBounds (actions.reduced (2));
        }
    }

    void KnobBuilderComponent::cycleColour (juce::Colour& colour, juce::TextButton& button)
    {
        const std::array<juce::Colour, 8> palette
        {
            juce::Colour (0xfff5a623), juce::Colour (0xff4fc3f7), juce::Colour (0xff8e6cd6),
            juce::Colour (0xff5fb37b), juce::Colour (0xffe24d42), juce::Colour (0xffd9dde2),
            juce::Colour (0xff1a1c20), juce::Colour (0xff080a0d)
        };

        int next = 0;
        for (int i = 0; i < (int) palette.size(); ++i)
            if (palette[(size_t) i] == colour)
                next = (i + 1) % (int) palette.size();

        colour = palette[(size_t) next];
        button.setColour (juce::TextButton::buttonColourId, colour);
        updateColourButtonText();
        repaint();
    }

    void KnobBuilderComponent::mouseDown (const juce::MouseEvent& e)
    {
        for (int i = 0; i < SectionCount; ++i)
        {
            if (sectionHeaderBounds[(size_t) i].contains (e.getPosition()))
            {
                sectionOpen[(size_t) i] = ! sectionOpen[(size_t) i];
                resized();
                repaint();
                return;
            }
        }

        for (int i = 0; i < (int) buildLayerRows.size(); ++i)
        {
            if (buildLayerRows[(size_t) i].contains (e.getPosition()))
            {
                const int layerIndex = buildLayerRowIndices[(size_t) i];
                selectedBuildLayer = layerIndex;
                if (e.x < buildLayerRows[(size_t) i].getX() + 30)
                    buildLayers[(size_t) layerIndex].visible = ! buildLayers[(size_t) layerIndex].visible;
                openSectionForLayer (buildLayers[(size_t) layerIndex].name);
                resized();
                repaint();
                return;
            }
        }

        if (previewKnobBounds.contains (e.getPosition()))
        {
            draggingPreviewKnob = true;
            updatePreviewValueFromPoint (e.getPosition());
        }
    }

    void KnobBuilderComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingPreviewKnob)
            updatePreviewValueFromPoint (e.getPosition());
    }

    void KnobBuilderComponent::mouseUp (const juce::MouseEvent&)
    {
        draggingPreviewKnob = false;
    }

    void KnobBuilderComponent::updatePreviewValueFromPoint (juce::Point<int> point)
    {
        if (previewKnobBounds.isEmpty())
            return;

        const auto centre = previewKnobBounds.getCentre().toFloat();
        const auto delta = point.toFloat() - centre;
        const float angleDegrees = juce::radiansToDegrees (std::atan2 (delta.y, delta.x));
        const float range = juce::jmax (1.0f, style.endAngle - style.startAngle);
        const float value = juce::jlimit (0.0f, 1.0f, (angleDegrees - style.startAngle) / range);
        valueSlider.setValue (value, juce::sendNotificationSync);
    }

    void KnobBuilderComponent::openSectionForLayer (const juce::String& layerName)
    {
        if (layerName == "Imported Filmstrip Base")
            sectionOpen[(size_t) ImportSection] = true;
        else if (layerName == "Value Arc" || layerName == "Indicator" || layerName == "Tick Marks"
                 || layerName == "Face Gradient" || layerName == "Outer Bezel" || layerName == "Specular Highlight")
            sectionOpen[(size_t) PaintSection] = true;
        else if (layerName == "Shadow" || layerName == "Text Label")
            sectionOpen[(size_t) BehaviorSection] = true;

        if (layerName == "Value Arc" || layerName == "Indicator")
            sectionOpen[(size_t) GeometrySection] = true;
    }

    bool KnobBuilderComponent::isBuildLayerVisible (const juce::String& layerName) const
    {
        for (const auto& layer : buildLayers)
            if (layer.name == layerName)
                return layer.visible;
        return true;
    }

    void KnobBuilderComponent::updateColourButtonText()
    {
        auto setText = [] (juce::TextButton& button, const juce::String& label, juce::Colour colour)
        {
            button.setButtonText (label + "\n#" + colour.toDisplayString (false).toUpperCase());
            button.setColour (juce::TextButton::buttonColourId, colour);
        };
        setText (indicatorColourBtn, "Indicator", style.indicator);
        setText (ringColourBtn, "Ring", style.ringColour);
        setText (backgroundColourBtn, "Face", style.backgroundColour);
        setText (borderColourBtn, "Border", style.borderColour);
        setText (tickColourBtn, "Ticks", style.tickColour);
    }

    bool KnobBuilderComponent::hasImportedKnob() const noexcept
    {
        return importedStrip.isValid() && importedFrameCount > 0 && importedFrameSize > 0;
    }

    void KnobBuilderComponent::syncControlsFromStyle()
    {
        nameEdit.setText (style.name, juce::dontSendNotification);
        sizeSlider.setValue (style.size, juce::dontSendNotification);
        valueSlider.setValue (style.previewValue, juce::dontSendNotification);
        startSlider.setValue (style.startAngle, juce::dontSendNotification);
        endSlider.setValue (style.endAngle, juce::dontSendNotification);
        ringWidthSlider.setValue (style.ringThickness, juce::dontSendNotification);
        pointerWidthSlider.setValue (style.pointerWidth, juce::dontSendNotification);
        bevelSlider.setValue (style.bevel, juce::dontSendNotification);
        glowSlider.setValue (style.glow, juce::dontSendNotification);
        framesSlider.setValue (style.frames, juce::dontSendNotification);
        importedBaseOpacitySlider.setValue (style.importedBaseOpacity, juce::dontSendNotification);
        overlayOpacitySlider.setValue (style.overlayOpacity, juce::dontSendNotification);
        ringToggle.setToggleState (style.ring, juce::dontSendNotification);
        ticksToggle.setToggleState (style.ticks, juce::dontSendNotification);
        shadowToggle.setToggleState (style.shadow, juce::dontSendNotification);
        labelToggle.setToggleState (style.label, juce::dontSendNotification);
        importedBaseToggle.setToggleState (hasImportedKnob(), juce::dontSendNotification);
        overlayToggle.setToggleState (true, juce::dontSendNotification);
        importedBaseToggle.setEnabled (hasImportedKnob());
        overlayToggle.setEnabled (hasImportedKnob());
        importedBaseOpacitySlider.setEnabled (hasImportedKnob());
        overlayOpacitySlider.setEnabled (hasImportedKnob());
        clearImportBtn.setEnabled (hasImportedKnob());
        updateColourButtonText();
        repaint();
    }

    bool KnobBuilderComponent::loadKnobSourceJson (const juce::File& source, juce::String& error)
    {
        const auto parsed = juce::JSON::parse (source);
        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
        {
            error = "This is not a valid PatchCraft knob source JSON file.";
            return false;
        }

        const auto assetType = root->getProperty ("assetType").toString();
        if (assetType.isNotEmpty() && assetType != "knob")
        {
            error = "This builder source is for '" + assetType + "', not a knob.";
            return false;
        }

        if (root->hasProperty ("name"))
            style.name = root->getProperty ("name").toString();

        if (auto* styleObject = root->getProperty ("style").getDynamicObject())
        {
            auto readFloat = [styleObject] (const char* key, float fallback)
            {
                return styleObject->hasProperty (key) ? (float) (double) styleObject->getProperty (key) : fallback;
            };
            auto readInt = [styleObject] (const char* key, int fallback)
            {
                return styleObject->hasProperty (key) ? (int) styleObject->getProperty (key) : fallback;
            };
            auto readBool = [styleObject] (const char* key, bool fallback)
            {
                return styleObject->hasProperty (key) ? (bool) styleObject->getProperty (key) : fallback;
            };
            auto readColour = [styleObject] (const char* key, juce::Colour fallback)
            {
                return styleObject->hasProperty (key)
                    ? juce::Colour::fromString (styleObject->getProperty (key).toString())
                    : fallback;
            };

            style.size = juce::jlimit (32, 4096, readInt ("size", style.size));
            style.frames = juce::jlimit (1, 256, readInt ("frames", style.frames));
            style.previewValue = juce::jlimit (0.0f, 1.0f, readFloat ("previewValue", style.previewValue));
            style.startAngle = readFloat ("startAngle", style.startAngle);
            style.endAngle = readFloat ("endAngle", style.endAngle);
            style.ringThickness = readFloat ("ringThickness", style.ringThickness);
            style.pointerWidth = readFloat ("pointerWidth", style.pointerWidth);
            style.bevel = juce::jlimit (0.0f, 1.0f, readFloat ("bevel", style.bevel));
            style.glow = juce::jlimit (0.0f, 1.0f, readFloat ("glow", style.glow));
            style.importedBaseOpacity = juce::jlimit (0.0f, 1.0f, readFloat ("importedBaseOpacity", style.importedBaseOpacity));
            style.overlayOpacity = juce::jlimit (0.0f, 1.0f, readFloat ("overlayOpacity", style.overlayOpacity));
            style.ring = readBool ("ring", style.ring);
            style.ticks = readBool ("ticks", style.ticks);
            style.shadow = readBool ("shadow", style.shadow);
            style.label = readBool ("label", style.label);
            style.indicator = readColour ("indicatorColour", style.indicator);
            style.ringColour = readColour ("ringColour", style.ringColour);
            style.backgroundColour = readColour ("backgroundColour", style.backgroundColour);
            style.borderColour = readColour ("borderColour", style.borderColour);
            style.tickColour = readColour ("tickColour", style.tickColour);
            selectComboText (styleBox, styleObject->getProperty ("builderStyle").toString());
            selectComboText (indicatorBox, styleObject->getProperty ("indicatorStyle").toString());
            selectComboText (outputBox, styleObject->getProperty ("outputMode").toString());
        }

        if (auto* layers = root->getProperty ("layers").getArray())
        {
            for (const auto& layerVar : *layers)
                if (auto* layerObject = layerVar.getDynamicObject())
                {
                    const auto layerName = layerObject->getProperty ("name").toString();
                    const bool visible = layerObject->hasProperty ("visible")
                        ? (bool) layerObject->getProperty ("visible")
                        : true;
                    for (auto& layer : buildLayers)
                        if (layer.name == layerName)
                        {
                            layer.visible = visible;
                            break;
                        }
                }
        }

        const auto sourceFile = root->getProperty ("sourceFile").toString();
        if (sourceFile.isNotEmpty())
        {
            juce::File imported = juce::File::isAbsolutePath (sourceFile)
                ? juce::File (sourceFile)
                : source.getParentDirectory().getChildFile (sourceFile);
            if (auto image = juce::ImageFileFormat::loadFrom (imported); image.isValid())
            {
                importedStrip = image;
                importedSourceFile = imported;
                detectImportedFilmstripLayout();
            }
        }

        syncControlsFromStyle();
        return true;
    }

    void KnobBuilderComponent::importExistingKnob()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import Existing Knob Filmstrip or Source",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.png;*.jpg;*.jpeg;*.patchcraft-knob.json;*.json");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File())
                    return;

                if (file.getFileExtension().equalsIgnoreCase (".json"))
                {
                    juce::String error;
                    if (! loadKnobSourceJson (file, error))
                        juce::AlertWindow::showAsync (
                            juce::MessageBoxOptions()
                                .withTitle ("Import Knob Source")
                                .withMessage (error)
                                .withButton ("OK")
                                .withIconType (juce::MessageBoxIconType::WarningIcon),
                            nullptr);
                    return;
                }

                auto image = juce::ImageFileFormat::loadFrom (file);
                if (! image.isValid())
                    return;

                importedStrip = image;
                importedSourceFile = file;
                detectImportedFilmstripLayout();

                if (hasImportedKnob())
                {
                    importedBaseToggle.setEnabled (true);
                    overlayToggle.setEnabled (true);
                    importedBaseOpacitySlider.setEnabled (true);
                    overlayOpacitySlider.setEnabled (true);
                    clearImportBtn.setEnabled (true);
                    importedBaseToggle.setToggleState (true, juce::dontSendNotification);
                    overlayToggle.setToggleState (true, juce::dontSendNotification);
                    outputBox.setSelectedId (importedStripVertical ? 1 : 2, juce::dontSendNotification);
                    sizeSlider.setValue (importedFrameSize, juce::sendNotificationSync);
                    framesSlider.setValue (importedFrameCount, juce::sendNotificationSync);

                    if (nameEdit.getText().trim().isEmpty() || nameEdit.getText() == "PatchCraft Pro Knob")
                        nameEdit.setText (file.getFileNameWithoutExtension(), true);
                }

                repaint();
            });
    }

    void KnobBuilderComponent::clearImportedKnob()
    {
        importedStrip = {};
        importedSourceFile = {};
        importedFrameCount = 0;
        importedFrameSize = 0;
        importedStripVertical = true;
        importedBaseToggle.setToggleState (false, juce::dontSendNotification);
        overlayToggle.setToggleState (true, juce::dontSendNotification);
        importedBaseToggle.setEnabled (false);
        overlayToggle.setEnabled (false);
        importedBaseOpacitySlider.setEnabled (false);
        overlayOpacitySlider.setEnabled (false);
        clearImportBtn.setEnabled (false);
        repaint();
    }

    void KnobBuilderComponent::detectImportedFilmstripLayout()
    {
        importedFrameCount = 0;
        importedFrameSize = 0;
        importedStripVertical = true;

        const int width = importedStrip.getWidth();
        const int height = importedStrip.getHeight();
        if (width <= 0 || height <= 0)
            return;

        const bool verticalCandidate = height >= width && height % width == 0;
        const bool horizontalCandidate = width >= height && width % height == 0;

        if (verticalCandidate && (! horizontalCandidate || height / width >= width / height))
        {
            importedStripVertical = true;
            importedFrameSize = width;
            importedFrameCount = juce::jmax (1, height / width);
        }
        else if (horizontalCandidate)
        {
            importedStripVertical = false;
            importedFrameSize = height;
            importedFrameCount = juce::jmax (1, width / height);
        }
        else
        {
            importedStripVertical = height >= width;
            importedFrameSize = juce::jmin (width, height);
            importedFrameCount = 1;
        }

        importedFrameCount = juce::jlimit (1, 256, importedFrameCount);
        importedFrameSize = juce::jlimit (16, 4096, importedFrameSize);
    }

    juce::Image KnobBuilderComponent::getImportedFrame (float position) const
    {
        if (! hasImportedKnob())
            return {};

        const int frameCount = juce::jlimit (1, 256, style.frames);
        const int frameSize = importedStripVertical
            ? juce::jmax (1, importedStrip.getHeight() / frameCount)
            : juce::jmax (1, importedStrip.getWidth() / frameCount);
        const int index = juce::jlimit (0, frameCount - 1,
                                        (int) std::round (juce::jlimit (0.0f, 1.0f, position)
                                                          * (float) juce::jmax (0, frameCount - 1)));
        const auto source = importedStripVertical
            ? juce::Rectangle<int> (0, index * frameSize, importedStrip.getWidth(), frameSize)
            : juce::Rectangle<int> (index * frameSize, 0, frameSize, importedStrip.getHeight());

        return importedStrip.getClippedImage (source.getIntersection (importedStrip.getBounds()));
    }

    void KnobBuilderComponent::drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String&)
    {
    }

    void KnobBuilderComponent::showColourPicker (juce::Colour initial, std::function<void (juce::Colour)> onChange)
    {
        const auto mouse = juce::Desktop::getInstance().getMousePosition();
        const auto targetArea = juce::Rectangle<int> (mouse.x - 6, mouse.y - 6, 12, 12);

        juce::CallOutBox::launchAsynchronously (
            std::make_unique<BuilderColourPopup> (initial, std::move (onChange)),
            targetArea,
            this);
    }

    juce::Image KnobBuilderComponent::renderKnobFrame (int idx, int total)
    {
        const int frameSize = juce::jmax (32, style.size);
        juce::Image img (juce::Image::ARGB, frameSize, frameSize, true);
        juce::Graphics g (img);
        drawKnob (g, juce::Rectangle<float> (0.0f, 0.0f, (float) frameSize, (float) frameSize),
                  (float) idx / (float) juce::jmax (1, total - 1), true);
        return img;
    }

    void KnobBuilderComponent::exportKnobFilmstrip()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Knob Filmstrip", juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.png");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto dst = fc.getResult();
                if (dst == juce::File())
                    return;
                if (dst.getFileExtension().isEmpty())
                    dst = dst.withFileExtension ("png");

                const int total = juce::jlimit (1, 256, style.frames);
                const int frameSize = juce::jmax (32, style.size);
                const bool vertical = outputBox.getSelectedId() != 2;
                juce::Image strip (juce::Image::ARGB,
                                   vertical ? frameSize : frameSize * total,
                                   vertical ? frameSize * total : frameSize,
                                   true);
                juce::Graphics gAll (strip);
                for (int i = 0; i < total; ++i)
                    gAll.drawImageAt (renderKnobFrame (i, total),
                                      vertical ? 0 : i * frameSize,
                                      vertical ? i * frameSize : 0);

                juce::PNGImageFormat png;
                if (auto out = std::unique_ptr<juce::FileOutputStream> (dst.createOutputStream()))
                    png.writeImageToStream (strip, *out);

                juce::String error;
                writeKnobSourceJson (dst.withFileExtension ("patchcraft-knob.json"), error);
            });
    }

    juce::var KnobBuilderComponent::buildKnobSourceVar() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "PatchCraft Builder Asset");
        root->setProperty ("formatVersion", 1);
        root->setProperty ("assetType", "knob");
        root->setProperty ("name", style.name);
        root->setProperty ("sourceFile", importedSourceFile.existsAsFile() ? importedSourceFile.getFullPathName() : juce::String());

        auto* styleObj = new juce::DynamicObject();
        styleObj->setProperty ("size", style.size);
        styleObj->setProperty ("frames", style.frames);
        styleObj->setProperty ("previewValue", (double) style.previewValue);
        styleObj->setProperty ("startAngle", (double) style.startAngle);
        styleObj->setProperty ("endAngle", (double) style.endAngle);
        styleObj->setProperty ("ringThickness", (double) style.ringThickness);
        styleObj->setProperty ("pointerWidth", (double) style.pointerWidth);
        styleObj->setProperty ("bevel", (double) style.bevel);
        styleObj->setProperty ("glow", (double) style.glow);
        styleObj->setProperty ("importedBaseOpacity", (double) style.importedBaseOpacity);
        styleObj->setProperty ("overlayOpacity", (double) style.overlayOpacity);
        styleObj->setProperty ("ring", style.ring);
        styleObj->setProperty ("ticks", style.ticks);
        styleObj->setProperty ("shadow", style.shadow);
        styleObj->setProperty ("label", style.label);
        styleObj->setProperty ("indicatorColour", style.indicator.toString());
        styleObj->setProperty ("ringColour", style.ringColour.toString());
        styleObj->setProperty ("backgroundColour", style.backgroundColour.toString());
        styleObj->setProperty ("borderColour", style.borderColour.toString());
        styleObj->setProperty ("tickColour", style.tickColour.toString());
        styleObj->setProperty ("builderStyle", styleBox.getText());
        styleObj->setProperty ("indicatorStyle", indicatorBox.getText());
        styleObj->setProperty ("outputMode", outputBox.getText());
        root->setProperty ("style", juce::var (styleObj));

        juce::Array<juce::var> layerArray;
        for (const auto& layer : buildLayers)
        {
            auto* layerObj = new juce::DynamicObject();
            layerObj->setProperty ("name", layer.name);
            layerObj->setProperty ("visible", layer.visible);
            layerArray.add (juce::var (layerObj));
        }
        root->setProperty ("layers", layerArray);

        return juce::var (root);
    }

    bool KnobBuilderComponent::writeKnobSourceJson (const juce::File& destination, juce::String& error) const
    {
        if (! destination.getParentDirectory().createDirectory())
        {
            error = "Could not create destination folder.";
            return false;
        }

        if (! destination.replaceWithText (juce::JSON::toString (buildKnobSourceVar(), true)))
        {
            error = "Could not write builder source JSON.";
            return false;
        }

        return true;
    }

    void KnobBuilderComponent::exportKnobSourceJson()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Editable Knob Source",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.patchcraft-knob.json;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto dst = fc.getResult();
                if (dst == juce::File())
                    return;
                if (dst.getFileExtension().isEmpty())
                    dst = dst.withFileExtension ("patchcraft-knob.json");

                juce::String error;
                if (! writeKnobSourceJson (dst, error))
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Export Knob Source")
                            .withMessage (error)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
            });
    }

    void KnobBuilderComponent::addKnobToLibrary()
    {
        auto folder = BuiltAssetLibraryComponent::getCategoryFolder ("knobs");
        folder.createDirectory();

        auto safeName = style.name.trim().isNotEmpty() ? style.name.trim() : juce::String ("knob");
        safeName = safeName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        safeName = safeName.replaceCharacter (' ', '_').toLowerCase();
        if (safeName.isEmpty())
            safeName = "knob";

        auto destination = folder.getChildFile (safeName + ".png");
        for (int i = 2; destination.existsAsFile(); ++i)
            destination = folder.getChildFile (safeName + "_" + juce::String (i) + ".png");

        const int total = juce::jlimit (1, 256, style.frames);
        const int frameSize = juce::jmax (32, style.size);
        const bool vertical = outputBox.getSelectedId() != 2;
        juce::Image strip (juce::Image::ARGB,
                           vertical ? frameSize : frameSize * total,
                           vertical ? frameSize * total : frameSize,
                           true);
        juce::Graphics gAll (strip);
        for (int i = 0; i < total; ++i)
            gAll.drawImageAt (renderKnobFrame (i, total),
                              vertical ? 0 : i * frameSize,
                              vertical ? i * frameSize : 0);

        juce::PNGImageFormat png;
        if (auto out = std::unique_ptr<juce::FileOutputStream> (destination.createOutputStream()))
            png.writeImageToStream (strip, *out);

        juce::String error;
        writeKnobSourceJson (destination.withFileExtension ("patchcraft-knob.json"), error);

        owner.refreshAllPanels();
    }
}
