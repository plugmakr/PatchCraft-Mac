#include "KnobBuilderComponent.h"
#include "StudioMainComponent.h"
#include "BuiltAssetLibraryComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "PluginClubPublisher.h"

#include <array>
#include <cstring>
#include <cmath>
#include <thread>
#include <tuple>

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
        configureSlider (imageScaleSlider, 0.25, 2.0, 0.01, style.imageScale, " x");
        configureSlider (imageRotationSlider, -180.0, 180.0, 1.0, style.imageRotation, "Â°");
        configureSlider (animationDepthSlider, 0.0, 1.0, 0.01, style.animationDepth);
        configureSlider (surfaceTextureSlider, 0.0, 1.0, 0.01, style.surfaceTexture);
        configureSlider (lightAngleSlider, -180.0, 180.0, 1.0, style.lightAngle, " deg");
        configureSlider (ringInsetSlider, 0.0, 1.0, 0.01, style.ringInset);
        configureSlider (pointerLengthSlider, 0.20, 1.25, 0.01, style.pointerLength);
        configureSlider (motionCurveSlider, 0.0, 1.0, 0.01, style.motionCurve);
        sizeSlider.setTooltip ("Output frame size for each knob frame.");
        valueSlider.setTooltip ("Scrubs the live preview and filmstrip frame position.");
        startSlider.setTooltip ("Minimum angle for the knob throw.");
        endSlider.setTooltip ("Maximum angle for the knob throw.");
        ringWidthSlider.setTooltip ("Thickness of the value arc.");
        pointerWidthSlider.setTooltip ("Width of the pointer, needle, or dot indicator.");
        bevelSlider.setTooltip ("Face depth and concave shading amount.");
        glowSlider.setTooltip ("Accent glow intensity.");
        framesSlider.setTooltip ("Number of animation frames rendered to the filmstrip.");
        importedBaseOpacitySlider.setTooltip ("Opacity of imported PNG or KnobMan artwork.");
        overlayOpacitySlider.setTooltip ("Opacity of PatchCraft-generated edits over imported art.");
        imageScaleSlider.setTooltip ("Scale imported art before rendering it into the knob.");
        imageRotationSlider.setTooltip ("Rotate imported art before animation is applied.");
        animationDepthSlider.setTooltip ("Amount of pulse, glow, or spin applied across the rendered frames.");
        surfaceTextureSlider.setTooltip ("Procedural surface texture amount applied to the generated face.");
        lightAngleSlider.setTooltip ("Virtual light direction for the knob face, bevel, and highlight.");
        ringInsetSlider.setTooltip ("Moves the value ring inward for deeper 3D knob shapes.");
        pointerLengthSlider.setTooltip ("Length of the pointer, needle, or indicator line.");
        motionCurveSlider.setTooltip ("Non-linear frame/value response for animated filmstrips.");

        for (auto* slider : { &sizeSlider, &valueSlider, &startSlider, &endSlider, &ringWidthSlider,
                              &pointerWidthSlider, &bevelSlider, &glowSlider, &framesSlider,
                              &importedBaseOpacitySlider, &overlayOpacitySlider, &imageScaleSlider,
                              &imageRotationSlider, &animationDepthSlider, &surfaceTextureSlider,
                              &lightAngleSlider, &ringInsetSlider, &pointerLengthSlider,
                              &motionCurveSlider })
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
        styleBox.setSelectedId (5, juce::dontSendNotification);
        styleBox.onChange = [this] { repaint(); };
        addAndMakeVisible (styleBox);

        indicatorBox.addItem ("Pointer Line", 1);
        indicatorBox.addItem ("Dot", 2);
        indicatorBox.addItem ("Needle", 3);
        indicatorBox.addItem ("Arc Only", 4);
        indicatorBox.setSelectedId (3, juce::dontSendNotification);
        indicatorBox.onChange = [this] { repaint(); };
        addAndMakeVisible (indicatorBox);

        imageRoleBox.addItem ("Full Imported Base", 1);
        imageRoleBox.addItem ("Face Texture", 2);
        imageRoleBox.addItem ("Rotating Indicator", 3);
        imageRoleBox.addItem ("Halo Overlay", 4);
        imageRoleBox.setSelectedId (1, juce::dontSendNotification);
        imageRoleBox.onChange = [this] { repaint(); };
        imageRoleBox.setTooltip ("Choose how an imported PNG filmstrip or image participates in the knob render.");
        addAndMakeVisible (imageRoleBox);

        imageFitBox.addItem ("Fit", 1);
        imageFitBox.addItem ("Fill", 2);
        imageFitBox.addItem ("Stretch", 3);
        imageFitBox.setSelectedId (1, juce::dontSendNotification);
        imageFitBox.onChange = [this] { repaint(); };
        imageFitBox.setTooltip ("Controls how imported art is fitted inside the knob frame.");
        addAndMakeVisible (imageFitBox);

        animationBox.addItem ("Static", 1);
        animationBox.addItem ("Value Sweep", 2);
        animationBox.addItem ("Pulse Ring", 3);
        animationBox.addItem ("Breathing Glow", 4);
        animationBox.addItem ("Image Spin", 5);
        animationBox.setSelectedId (3, juce::dontSendNotification);
        animationBox.onChange = [this] { repaint(); };
        animationBox.setTooltip ("Preview/export animation behavior for generated filmstrip frames.");
        addAndMakeVisible (animationBox);

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
        importBtn.setTooltip ("Import a PNG/JPG filmstrip, PatchCraft knob source JSON, or best-effort KnobMan .knob file.");
        clearImportBtn.onClick = [this] { clearImportedKnob(); };
        clearImportBtn.setTooltip ("Remove imported artwork and return to generated PatchCraft layers.");
        clearImportBtn.setEnabled (false);
        proDemoBtn.onClick = [this] { loadProDemoKnob(); };
        proDemoBtn.setTooltip ("Load a stronger animated demo knob preset so the builder starts from a professional visual target.");
        addAndMakeVisible (importBtn);
        addAndMakeVisible (clearImportBtn);
        addAndMakeVisible (proDemoBtn);

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
        publishBtn.setTooltip ("Package this knob filmstrip/source and push it to Plugin.club as a draft control asset.");
        publishBtn.onClick = [this] { publishKnobToPluginClub(); };
        for (auto* button : { &exportBtn, &exportJsonBtn, &addToProjectBtn, &publishBtn })
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
            { "Text Label", false }
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
        slider.setScrollWheelEnabled (false);
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
        style.imageScale = (float) imageScaleSlider.getValue();
        style.imageRotation = (float) imageRotationSlider.getValue();
        style.animationDepth = (float) animationDepthSlider.getValue();
        style.surfaceTexture = (float) surfaceTextureSlider.getValue();
        style.lightAngle = (float) lightAngleSlider.getValue();
        style.ringInset = (float) ringInsetSlider.getValue();
        style.pointerLength = (float) pointerLengthSlider.getValue();
        style.motionCurve = (float) motionCurveSlider.getValue();
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

        workbenchCards.clear();

        auto previewHeader = centre.removeFromTop (28);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText ("Knob Workbench", previewHeader, juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f));
        const int frameCount = style.frames;
        const auto frameText = "Frame " + juce::String ((int) (style.previewValue * (float) juce::jmax (0, frameCount - 1)) + 1)
                             + " / " + juce::String (juce::jmax (1, frameCount));
        g.drawText (hasImportedKnob() ? importedSourceFile.getFileName() + "  |  " + frameText : frameText,
                    previewHeader, juce::Justification::centredRight);

        auto previewColumn = centre.removeFromLeft (juce::jlimit (300, 390, centre.getWidth() / 2)).reduced (8);
        auto bench = centre.reduced (8);

        PatchCraftLookAndFeel::drawPanel (g, previewColumn, 9.0f);
        auto previewTitle = previewColumn.removeFromTop (22).reduced (10, 0);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText ("LIVE ASSET PREVIEW", previewTitle, juce::Justification::centredLeft);
        auto previewArea = previewColumn.removeFromTop (juce::jmax (220, previewColumn.getHeight() - 134)).reduced (12, 4);
        previewKnobBounds = previewArea.reduced (12);
        drawKnob (g, previewKnobBounds.toFloat(), style.previewValue, false);

        auto hint = previewColumn.removeFromTop (34).reduced (10, 2);
        g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.62f));
        g.fillRoundedRectangle (hint.toFloat(), 7.0f);
        g.setColour (PatchCraftLookAndFeel::borderSoft());
        g.drawRoundedRectangle (hint.toFloat(), 7.0f, 1.0f);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.5f, juce::Font::bold));
        g.drawText ("DRAG TO SCRUB  |  TEXT LABELS ARE OPTIONAL EXPORT LAYERS",
                    hint.reduced (8, 0), juce::Justification::centred);

        auto filmstrip = previewColumn.reduced (10, 2);
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

        PatchCraftLookAndFeel::drawPanel (g, bench, 9.0f);
        bench = bench.reduced (12, 10);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("PARTS + RECIPES", bench.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (10.5f));
        g.drawFittedText ("Combine bezels, faces, rings, indicators, material treatments, and motion recipes. Cards below write real builder parameters and export into the PNG/JSON asset.",
                          bench.removeFromTop (34), juce::Justification::topLeft, 2);
        bench.removeFromTop (6);

        auto drawSectionTitle = [&] (const juce::String& title)
        {
            auto titleArea = bench.removeFromTop (18);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawText (title, titleArea, juce::Justification::centredLeft);
            bench.removeFromTop (4);
        };

        auto drawGrid = [&] (std::initializer_list<std::tuple<const char*, const char*, const char*, juce::Colour>> cards)
        {
            const int columns = bench.getWidth() > 520 ? 3 : 2;
            const int gap = 8;
            const int cardW = (bench.getWidth() - gap * (columns - 1)) / columns;
            const int cardH = 64;
            int index = 0;
            auto row = bench.removeFromTop (((int) cards.size() + columns - 1) / columns * (cardH + gap) - gap);
            for (const auto& card : cards)
            {
                const int col = index % columns;
                const int rowIndex = index / columns;
                auto area = juce::Rectangle<int> (row.getX() + col * (cardW + gap),
                                                  row.getY() + rowIndex * (cardH + gap),
                                                  cardW, cardH);
                drawWorkbenchCard (g, area, std::get<0> (card), std::get<1> (card),
                                   std::get<2> (card), std::get<3> (card));
                ++index;
            }
            bench.removeFromTop (10);
        };

        drawSectionTitle ("GEOMETRY PARTS");
        drawGrid ({
            { "Deep Bezel", "3D chrome rim + inset ring", "deepBezel", juce::Colour (0xff8aa4ff) },
            { "Glass Face", "smooth translucent highlight", "glassFace", juce::Colour (0xff59d7ff) },
            { "Console Ring", "tight pro encoder arc", "consoleRing", juce::Colour (0xffffa51d) },
            { "Needle", "long hardware pointer", "needleIndicator", juce::Colour (0xffffcf6a) },
            { "Dot", "minimal position dot", "dotIndicator", juce::Colour (0xff7cf2b2) },
            { "Flat UI", "clean modern control", "flatMinimal", juce::Colour (0xffc2c7d0) }
        });

        drawSectionTitle ("MATERIALS");
        drawGrid ({
            { "Obsidian", "dark stone + amber light", "materialObsidian", juce::Colour (0xffffa51d) },
            { "Neon Blue", "cyan glow performer skin", "materialNeon", juce::Colour (0xff20d6ff) },
            { "Brushed Metal", "visible texture + ticks", "materialMetal", juce::Colour (0xffc8d0d8) }
        });

        drawSectionTitle ("ANIMATION RECIPES");
        drawGrid ({
            { "Pulse", "ring breathes across frames", "animPulse", juce::Colour (0xffffa51d) },
            { "Glow", "value-reactive halo", "animGlow", juce::Colour (0xff8a6cff) },
            { "Spin Layer", "imported art rotates", "animSpin", juce::Colour (0xff54d7ff) }
        });

        for (int i = 0; i < SectionCount; ++i)
        {
            auto header = sectionHeaderBounds[(size_t) i];
            if (header.isEmpty())
                continue;
            if (! rightPanelViewportBounds.intersects (header))
                continue;
            g.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.35f));
            g.fillRoundedRectangle (header.toFloat(), 4.0f);
            g.setColour (PatchCraftLookAndFeel::borderSoft());
            g.drawRoundedRectangle (header.toFloat(), 4.0f, 1.0f);
        }

        for (const auto& item : sliderLabelRects)
        {
            if (! rightPanelViewportBounds.intersects (item.first))
                continue;

            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (9.8f, juce::Font::bold));
            g.drawFittedText (item.second, item.first, juce::Justification::centredLeft, 1);
        }

        if (rightPanelMaxScroll > 0 && ! rightPanelViewportBounds.isEmpty())
        {
            auto scrollArea = rightPanelViewportBounds;
            auto track = scrollArea.removeFromRight (5).reduced (0, 6);
            g.setColour (PatchCraftLookAndFeel::bg().withAlpha (0.8f));
            g.fillRoundedRectangle (track.toFloat(), 2.5f);
            const auto proportion = (float) rightPanelViewportBounds.getHeight() / (float) juce::jmax (rightPanelViewportBounds.getHeight(), rightPanelContentHeight);
            const int thumbH = juce::jlimit (28, track.getHeight(), (int) ((float) track.getHeight() * proportion));
            const int thumbY = track.getY() + (int) ((float) (track.getHeight() - thumbH) * ((float) rightPanelScrollOffset / (float) rightPanelMaxScroll));
            auto thumb = juce::Rectangle<int> (track.getX(), thumbY, track.getWidth(), thumbH);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
            g.fillRoundedRectangle (thumb.toFloat(), 2.5f);
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
        const float curvedPosition = std::pow (juce::jlimit (0.0f, 1.0f, position),
                                               juce::jmap (juce::jlimit (0.0f, 1.0f, style.motionCurve),
                                                           0.0f, 1.0f, 0.72f, 1.65f));
        const float angle = startA + (endA - startA) * curvedPosition;
        const int importedRole = imageRoleBox.getSelectedId() == 0 ? 1 : imageRoleBox.getSelectedId();
        const int animationMode = animationBox.getSelectedId() == 0 ? 2 : animationBox.getSelectedId();
        const float phase = std::sin (juce::MathConstants<float>::twoPi * curvedPosition);
        const float animationDepth = juce::jlimit (0.0f, 1.0f, style.animationDepth);
        const float animatedRingBoost = animationMode == 3 ? (1.0f + (0.18f * animationDepth * (phase * 0.5f + 0.5f))) : 1.0f;
        const float animatedGlow = animationMode == 4 ? juce::jlimit (0.0f, 1.0f, style.glow + animationDepth * (phase * 0.5f + 0.5f)) : style.glow;
        const float ringInsetPixels = juce::jlimit (0.0f, 1.0f, style.ringInset) * radius * 0.18f;
        const float ringRadius = radius - ringW * 0.5f - ringInsetPixels;
        const bool drawingImportedBase = hasImportedKnob()
                                      && importedBaseToggle.getToggleState()
                                      && importedRole == 1
                                      && isBuildLayerVisible ("Imported Filmstrip Base");
        auto drawImportedImage = [&] (juce::Rectangle<float> target, float opacity, float rotationDegrees)
        {
            auto importedFrame = getImportedFrame (position);
            if (! importedFrame.isValid())
                return;

            const auto scale = juce::jlimit (0.25f, 2.0f, style.imageScale);
            auto imageBounds = target.withSizeKeepingCentre (target.getWidth() * scale, target.getHeight() * scale);
            auto placement = juce::RectanglePlacement::centred;
            if (imageFitBox.getSelectedId() == 2)
                placement = juce::RectanglePlacement::fillDestination;
            else if (imageFitBox.getSelectedId() == 3)
                placement = juce::RectanglePlacement::stretchToFit;

            juce::Graphics::ScopedSaveState saved (g);
            g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rotationDegrees),
                                                             imageBounds.getCentreX(),
                                                             imageBounds.getCentreY()));
            g.setOpacity (juce::jlimit (0.0f, 1.0f, opacity));
            g.drawImageWithin (importedFrame,
                               (int) imageBounds.getX(), (int) imageBounds.getY(),
                               (int) imageBounds.getWidth(), (int) imageBounds.getHeight(),
                               placement);
        };

        if (style.shadow && ! compact && ! drawingImportedBase && isBuildLayerVisible ("Shadow"))
        {
            g.setColour (juce::Colours::black.withAlpha (0.42f));
            g.fillEllipse (knob.translated (0.0f, radius * 0.08f).reduced (radius * 0.1f));
        }

        if (drawingImportedBase)
        {
            const float spin = animationMode == 5 ? 360.0f * position * animationDepth : 0.0f;
            drawImportedImage (knob, style.importedBaseOpacity, style.imageRotation + spin);
            g.setOpacity (1.0f);

            if (! overlayToggle.getToggleState())
                return;

            g.setOpacity (style.overlayOpacity);
        }

        if (style.ring && isBuildLayerVisible ("Value Arc"))
        {
            juce::Path track;
            track.addCentredArc (cx, cy, ringRadius, ringRadius, 0.0f, startA, endA, true);
            g.setColour (PatchCraftLookAndFeel::borderSoft());
            g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path active;
            active.addCentredArc (cx, cy, ringRadius, ringRadius, 0.0f, startA, angle, true);
            g.setColour (style.ringColour.withAlpha (0.65f + animatedGlow * 0.35f));
            g.strokePath (active, juce::PathStrokeType (ringW * animatedRingBoost, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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

        const float faceRadius = radius - (style.ring ? ringW + 5.0f + ringInsetPixels * 0.45f : 4.0f);
        if (! drawingImportedBase && isBuildLayerVisible ("Face Gradient"))
        {
            const float lightRadians = juce::degreesToRadians (style.lightAngle);
            const float lx = std::cos (lightRadians);
            const float ly = std::sin (lightRadians);
            juce::ColourGradient face (style.backgroundColour.brighter (style.bevel),
                                       cx + lx * faceRadius * 0.72f, cy + ly * faceRadius * 0.72f,
                                       style.backgroundColour.darker (0.55f),
                                       cx - lx * faceRadius * 0.55f, cy - ly * faceRadius * 0.55f, false);
            g.setGradientFill (face);
            g.fillEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

            const float texture = juce::jlimit (0.0f, 1.0f, style.surfaceTexture);
            if (texture > 0.01f)
            {
                g.setColour (style.tickColour.withAlpha (0.04f + texture * 0.12f));
                const int lines = compact ? 4 : 9;
                for (int i = 0; i < lines; ++i)
                {
                    const float y01 = ((float) i + 0.5f) / (float) lines;
                    const float y = cy - faceRadius + y01 * faceRadius * 2.0f;
                    const float half = std::sqrt (juce::jmax (0.0f, faceRadius * faceRadius - (y - cy) * (y - cy)));
                    g.drawLine (cx - half * 0.82f, y, cx + half * 0.82f, y + texture * 1.5f, 0.6f);
                }
            }
        }

        if (! drawingImportedBase
            && importedRole == 2
            && hasImportedKnob()
            && importedBaseToggle.getToggleState()
            && isBuildLayerVisible ("Imported Filmstrip Base"))
        {
            const float spin = animationMode == 5 ? 180.0f * position * animationDepth : 0.0f;
            drawImportedImage (juce::Rectangle<float> (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f),
                               style.importedBaseOpacity * 0.9f,
                               style.imageRotation + spin);
            g.setOpacity (1.0f);
        }

        if (! drawingImportedBase && isBuildLayerVisible ("Outer Bezel"))
        {
            g.setColour (style.borderColour);
            g.drawEllipse (cx - faceRadius, cy - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f, compact ? 1.0f : 1.8f);
        }

        if (animatedGlow > 0.01f && ! drawingImportedBase && isBuildLayerVisible ("Specular Highlight"))
        {
            g.setColour (style.ringColour.withAlpha (0.10f * animatedGlow));
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

        if (importedRole == 3
            && hasImportedKnob()
            && importedBaseToggle.getToggleState()
            && isBuildLayerVisible ("Indicator"))
        {
            const auto indicatorArea = juce::Rectangle<float> (cx - faceRadius * 0.42f,
                                                               cy - faceRadius * 0.42f,
                                                               faceRadius * 0.84f,
                                                               faceRadius * 0.84f);
            drawImportedImage (indicatorArea,
                               style.importedBaseOpacity,
                               style.imageRotation + juce::radiansToDegrees (angle) + 90.0f);
            g.setOpacity (1.0f);
        }
        else if (indicatorBox.getSelectedId() != 4 && isBuildLayerVisible ("Indicator"))
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
                const float pointerLength = faceRadius * juce::jlimit (0.20f, 1.25f, style.pointerLength)
                                          * (indicatorBox.getSelectedId() == 3 ? 1.0f : 0.72f);
                pointer.addRoundedRectangle (-style.pointerWidth * 0.5f, -pointerLength,
                                             style.pointerWidth, pointerLength * 0.72f, style.pointerWidth * 0.5f);
                pointer.applyTransform (juce::AffineTransform::rotation (angle + juce::MathConstants<float>::halfPi).translated (cx, cy));
                g.fillPath (pointer);
            }
        }

        if (style.label && ! compact && isBuildLayerVisible ("Text Label"))
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (10.0f, juce::Font::bold));
            g.drawFittedText (style.name,
                              area.withHeight (18.0f).withY (area.getBottom() - 20.0f).toNearestInt(),
                              juce::Justification::centred, 1);
        }

        if (! drawingImportedBase
            && importedRole == 4
            && hasImportedKnob()
            && importedBaseToggle.getToggleState()
            && isBuildLayerVisible ("Imported Filmstrip Base"))
        {
            const float spin = animationMode == 5 ? 360.0f * position * animationDepth : 0.0f;
            drawImportedImage (knob.expanded (side * 0.06f), style.importedBaseOpacity * 0.75f, style.imageRotation + spin);
            g.setOpacity (1.0f);
        }

        if (drawingImportedBase)
            g.setOpacity (1.0f);
    }

    void KnobBuilderComponent::drawWorkbenchCard (juce::Graphics& g, juce::Rectangle<int> area,
                                                  const juce::String& title,
                                                  const juce::String& body,
                                                  const juce::String& actionId,
                                                  juce::Colour accent)
    {
        workbenchCards.push_back ({ area, actionId });
        g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.92f));
        g.fillRoundedRectangle (area.toFloat(), 7.0f);
        g.setColour (accent.withAlpha (0.70f));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 7.0f, 1.1f);
        auto text = area.reduced (9, 7);
        g.setColour (accent);
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.drawFittedText (title, text.removeFromTop (16), juce::Justification::centredLeft, 1);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.3f));
        g.drawFittedText (body, text, juce::Justification::topLeft, 2);
    }

    void KnobBuilderComponent::applyWorkbenchAction (const juce::String& actionId)
    {
        if (actionId == "deepBezel")
        {
            styleBox.setSelectedId (5, juce::dontSendNotification);
            style.bevel = 0.82f; style.ringInset = 0.36f; style.ringThickness = 12.0f;
            style.borderColour = juce::Colour (0xff05070a);
        }
        else if (actionId == "glassFace")
        {
            styleBox.setSelectedId (4, juce::dontSendNotification);
            style.bevel = 0.62f; style.glow = 0.62f; style.surfaceTexture = 0.04f;
            style.backgroundColour = juce::Colour (0xff1b2430);
        }
        else if (actionId == "consoleRing")
        {
            style.ring = true; style.ringInset = 0.12f; style.ringThickness = 8.0f;
            style.ringColour = juce::Colour (0xffffa51d);
        }
        else if (actionId == "needleIndicator")
        {
            indicatorBox.setSelectedId (3, juce::dontSendNotification);
            style.pointerWidth = 5.0f; style.pointerLength = 0.96f;
        }
        else if (actionId == "dotIndicator")
        {
            indicatorBox.setSelectedId (2, juce::dontSendNotification);
            style.pointerWidth = 6.0f; style.pointerLength = 0.55f;
        }
        else if (actionId == "flatMinimal")
        {
            styleBox.setSelectedId (3, juce::dontSendNotification);
            style.bevel = 0.08f; style.glow = 0.0f; style.surfaceTexture = 0.0f;
            style.shadow = false; style.ticks = false; style.ringInset = 0.0f;
        }
        else if (actionId == "materialObsidian")
        {
            style.backgroundColour = juce::Colour (0xff11151b);
            style.borderColour = juce::Colour (0xff020305);
            style.indicator = juce::Colour (0xffffa51d);
            style.ringColour = juce::Colour (0xffffa51d);
            style.tickColour = juce::Colour (0xff47d7ff);
            style.surfaceTexture = 0.34f;
            style.lightAngle = -48.0f;
        }
        else if (actionId == "materialNeon")
        {
            style.backgroundColour = juce::Colour (0xff07131d);
            style.indicator = juce::Colour (0xff20d6ff);
            style.ringColour = juce::Colour (0xff20d6ff);
            style.tickColour = juce::Colour (0xff8a6cff);
            style.glow = 0.82f;
            style.surfaceTexture = 0.12f;
        }
        else if (actionId == "materialMetal")
        {
            style.backgroundColour = juce::Colour (0xff242932);
            style.borderColour = juce::Colour (0xff07090c);
            style.tickColour = juce::Colour (0xffc8d0d8);
            style.surfaceTexture = 0.72f;
            style.bevel = 0.72f;
            style.ticks = true;
        }
        else if (actionId == "animPulse")
        {
            animationBox.setSelectedId (3, juce::dontSendNotification);
            style.animationDepth = 0.72f; style.motionCurve = 0.50f; style.frames = juce::jmax (96, style.frames);
        }
        else if (actionId == "animGlow")
        {
            animationBox.setSelectedId (4, juce::dontSendNotification);
            style.animationDepth = 0.85f; style.glow = juce::jmax (0.60f, style.glow); style.frames = juce::jmax (96, style.frames);
        }
        else if (actionId == "animSpin")
        {
            animationBox.setSelectedId (5, juce::dontSendNotification);
            imageRoleBox.setSelectedId (3, juce::dontSendNotification);
            style.animationDepth = 0.70f; style.frames = juce::jmax (128, style.frames);
        }

        syncControlsFromStyle();
        owner.getProject().markDirty();
        repaint();
    }

    void KnobBuilderComponent::resized()
    {
        juce::Component* editableControls[] {
            &nameEdit, &styleBox, &proDemoBtn, &importBtn, &clearImportBtn,
            &importedBaseToggle, &overlayToggle, &importedBaseOpacitySlider, &overlayOpacitySlider,
            &imageRoleBox, &imageFitBox, &imageScaleSlider, &imageRotationSlider,
            &sizeSlider, &valueSlider, &startSlider, &endSlider, &ringWidthSlider,
            &pointerWidthSlider, &indicatorBox, &indicatorColourBtn, &ringColourBtn,
            &backgroundColourBtn, &borderColourBtn, &tickColourBtn, &bevelSlider, &glowSlider,
            &ringToggle, &ticksToggle, &shadowToggle, &labelToggle, &framesSlider,
            &animationBox, &animationDepthSlider, &surfaceTextureSlider, &lightAngleSlider,
            &ringInsetSlider, &pointerLengthSlider, &motionCurveSlider, &outputBox,
            &exportBtn, &exportJsonBtn, &addToProjectBtn, &publishBtn
        };

        for (auto* component : editableControls)
            component->setVisible (false);

        for (auto& header : sectionHeaderBounds)
            header = {};
        sliderLabelRects.clear();

        auto bounds = getLocalBounds().reduced (2);
        bounds.removeFromLeft (190);
        rightPanelViewportBounds = bounds.removeFromRight (290).reduced (6);
        const auto rightInner = rightPanelViewportBounds.reduced (10, 8);
        rightPanelScrollOffset = juce::jlimit (0, juce::jmax (0, rightPanelMaxScroll), rightPanelScrollOffset);
        auto right = rightInner.translated (0, -rightPanelScrollOffset);

        auto isRowVisible = [this] (juce::Rectangle<int> row)
        {
            return rightPanelViewportBounds.expanded (0, 18).intersects (row);
        };

        auto putHeader = [&] (juce::Label& label, SectionIndex index)
        {
            auto header = right.removeFromTop (22);
            sectionHeaderBounds[(size_t) index] = header;
            const juce::String titles[] { "ASSET", "IMPORT / EDIT EXISTING", "GEOMETRY", "PAINT / LAYERS", "BEHAVIOR", "OUTPUT" };
            label.setText ((sectionOpen[(size_t) index] ? "v  " : ">  ") + titles[(int) index], juce::dontSendNotification);
            label.setVisible (isRowVisible (header));
            label.setBounds (header);
            right.removeFromTop (4);
        };
        auto putRow = [&] (juce::Component& component)
        {
            auto row = right.removeFromTop (25);
            component.setVisible (isRowVisible (row));
            component.setBounds (row);
            right.removeFromTop (5);
        };
        auto putSliderRow = [&] (const juce::String& label, juce::Slider& slider)
        {
            auto row = right.removeFromTop (28);
            auto labelBounds = row.removeFromLeft (96);
            sliderLabelRects.push_back ({ labelBounds, label });
            slider.setVisible (isRowVisible (row));
            slider.setBounds (row);
            right.removeFromTop (5);
        };
        auto putPairRow = [&] (juce::Component& first, juce::Component& second)
        {
            auto row = right.removeFromTop (30);
            auto leftCell = row.removeFromLeft (row.getWidth() / 2);
            first.setVisible (isRowVisible (leftCell));
            second.setVisible (isRowVisible (row));
            first.setBounds (leftCell.reduced (2));
            second.setBounds (row.reduced (2));
            right.removeFromTop (5);
        };

        putHeader (assetLbl, AssetSection);
        if (sectionOpen[(size_t) AssetSection])
        {
            putRow (nameEdit);
            putRow (styleBox);
            putRow (proDemoBtn);
        }

        right.removeFromTop (4);
        putHeader (importLbl, ImportSection);
        if (sectionOpen[(size_t) ImportSection])
        {
            putPairRow (importBtn, clearImportBtn);
            putRow (imageRoleBox);
            putRow (imageFitBox);
            putSliderRow ("Image Scale", imageScaleSlider);
            putSliderRow ("Image Rotate", imageRotationSlider);
            importedBaseToggle.setVisible (isRowVisible (right.withHeight (24)));
            importedBaseToggle.setBounds (right.removeFromTop (24));
            overlayToggle.setVisible (isRowVisible (right.withHeight (24)));
            overlayToggle.setBounds (right.removeFromTop (24));
            right.removeFromTop (4);
            putSliderRow ("Base Opacity", importedBaseOpacitySlider);
            putSliderRow ("Overlay Mix", overlayOpacitySlider);
        }

        right.removeFromTop (4);
        putHeader (geometryLbl, GeometrySection);
        if (sectionOpen[(size_t) GeometrySection])
        {
            putSliderRow ("Frame Size", sizeSlider);
            putSliderRow ("Preview", valueSlider);
            putSliderRow ("Start Angle", startSlider);
            putSliderRow ("End Angle", endSlider);
            putSliderRow ("Ring Width", ringWidthSlider);
            putSliderRow ("Ring Inset", ringInsetSlider);
            putSliderRow ("Pointer W", pointerWidthSlider);
            putSliderRow ("Pointer Len", pointerLengthSlider);
        }

        right.removeFromTop (6);
        putHeader (paintLbl, PaintSection);
        if (sectionOpen[(size_t) PaintSection])
        {
            putRow (indicatorBox);
            auto colourRow = right.removeFromTop (62);
            for (auto* button : { &indicatorColourBtn, &ringColourBtn, &backgroundColourBtn, &borderColourBtn, &tickColourBtn })
                button->setVisible (isRowVisible (colourRow));
            auto topColours = colourRow.removeFromTop (30);
            indicatorColourBtn.setBounds (topColours.removeFromLeft (topColours.getWidth() / 2).reduced (2));
            ringColourBtn.setBounds (topColours.reduced (2));
            backgroundColourBtn.setBounds (colourRow.removeFromLeft (colourRow.getWidth() / 3).reduced (2));
            borderColourBtn.setBounds (colourRow.removeFromLeft (colourRow.getWidth() / 2).reduced (2));
            tickColourBtn.setBounds (colourRow.reduced (2));
            right.removeFromTop (4);
            putSliderRow ("3D Depth", bevelSlider);
            putSliderRow ("Light Angle", lightAngleSlider);
            putSliderRow ("Texture", surfaceTextureSlider);
            putSliderRow ("Glow", glowSlider);
        }

        right.removeFromTop (6);
        putHeader (behaviorLbl, BehaviorSection);
        if (sectionOpen[(size_t) BehaviorSection])
        {
            for (auto* toggle : { &ringToggle, &ticksToggle, &shadowToggle, &labelToggle })
                toggle->setVisible (isRowVisible (right.withHeight (56)));
            auto toggles = right.removeFromTop (58);
            auto toggleRowA = toggles.removeFromTop (28);
            ringToggle.setBounds (toggleRowA.removeFromLeft (toggleRowA.getWidth() / 2).reduced (2));
            ticksToggle.setBounds (toggleRowA.reduced (2));
            shadowToggle.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2).reduced (2));
            labelToggle.setBounds (toggles.reduced (2));
            putRow (animationBox);
            putSliderRow ("Anim Depth", animationDepthSlider);
            putSliderRow ("Motion Curve", motionCurveSlider);
            putSliderRow ("Frames", framesSlider);
        }

        right.removeFromTop (6);
        putHeader (exportLbl, OutputSection);
        if (sectionOpen[(size_t) OutputSection])
        {
            putRow (outputBox);
            auto actions = right.removeFromTop (30);
            for (auto* button : { &exportBtn, &exportJsonBtn, &addToProjectBtn, &publishBtn })
                button->setVisible (isRowVisible (actions));
            const int buttonW = juce::jmax (62, actions.getWidth() / 4);
            exportBtn.setBounds (actions.removeFromLeft (buttonW).reduced (2));
            exportJsonBtn.setBounds (actions.removeFromLeft (buttonW).reduced (2));
            addToProjectBtn.setBounds (actions.removeFromLeft (buttonW).reduced (2));
            publishBtn.setBounds (actions.reduced (2));
        }

        rightPanelContentHeight = juce::jmax (0, right.getY() + rightPanelScrollOffset - rightInner.getY());
        rightPanelMaxScroll = juce::jmax (0, rightPanelContentHeight - rightInner.getHeight());
        const auto clamped = juce::jlimit (0, rightPanelMaxScroll, rightPanelScrollOffset);
        if (clamped != rightPanelScrollOffset)
        {
            rightPanelScrollOffset = clamped;
            resized();
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
        for (const auto& card : workbenchCards)
        {
            if (card.bounds.contains (e.getPosition()))
            {
                applyWorkbenchAction (card.actionId);
                return;
            }
        }

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

    void KnobBuilderComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (! rightPanelViewportBounds.contains (e.getPosition()) || rightPanelMaxScroll <= 0)
            return;

        const int delta = (int) std::round (wheel.deltaY * -120.0f);
        rightPanelScrollOffset = juce::jlimit (0, rightPanelMaxScroll, rightPanelScrollOffset + delta);
        resized();
        repaint();
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
        imageScaleSlider.setValue (style.imageScale, juce::dontSendNotification);
        imageRotationSlider.setValue (style.imageRotation, juce::dontSendNotification);
        animationDepthSlider.setValue (style.animationDepth, juce::dontSendNotification);
        surfaceTextureSlider.setValue (style.surfaceTexture, juce::dontSendNotification);
        lightAngleSlider.setValue (style.lightAngle, juce::dontSendNotification);
        ringInsetSlider.setValue (style.ringInset, juce::dontSendNotification);
        pointerLengthSlider.setValue (style.pointerLength, juce::dontSendNotification);
        motionCurveSlider.setValue (style.motionCurve, juce::dontSendNotification);
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
        imageRoleBox.setEnabled (hasImportedKnob());
        imageFitBox.setEnabled (hasImportedKnob());
        imageScaleSlider.setEnabled (hasImportedKnob());
        imageRotationSlider.setEnabled (hasImportedKnob());
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
            style.imageScale = juce::jlimit (0.25f, 2.0f, readFloat ("imageScale", style.imageScale));
            style.imageRotation = juce::jlimit (-180.0f, 180.0f, readFloat ("imageRotation", style.imageRotation));
            style.animationDepth = juce::jlimit (0.0f, 1.0f, readFloat ("animationDepth", style.animationDepth));
            style.surfaceTexture = juce::jlimit (0.0f, 1.0f, readFloat ("surfaceTexture", style.surfaceTexture));
            style.lightAngle = juce::jlimit (-180.0f, 180.0f, readFloat ("lightAngle", style.lightAngle));
            style.ringInset = juce::jlimit (0.0f, 1.0f, readFloat ("ringInset", style.ringInset));
            style.pointerLength = juce::jlimit (0.20f, 1.25f, readFloat ("pointerLength", style.pointerLength));
            style.motionCurve = juce::jlimit (0.0f, 1.0f, readFloat ("motionCurve", style.motionCurve));
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
            selectComboText (imageRoleBox, styleObject->getProperty ("imageRole").toString());
            selectComboText (imageFitBox, styleObject->getProperty ("imageFit").toString());
            selectComboText (animationBox, styleObject->getProperty ("animationMode").toString());
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

    bool KnobBuilderComponent::loadKnobManFile (const juce::File& source, juce::String& error)
    {
        const auto text = source.loadFileAsString();
        if (text.trim().isEmpty())
        {
            error = "The .knob file could not be read as text. Export a PNG filmstrip from KnobMan, or save a text/XML .knob file.";
            return false;
        }

        auto readNumber = [&text] (std::initializer_list<const char*> keys, float fallback)
        {
            for (auto* key : keys)
            {
                const int pos = text.indexOfIgnoreCase (key);
                if (pos < 0)
                    continue;

                auto tail = text.substring (pos + (int) std::strlen (key), pos + (int) std::strlen (key) + 64);
                int start = 0;
                while (start < tail.length()
                       && ! juce::CharacterFunctions::isDigit (tail[start])
                       && tail[start] != '-'
                       && tail[start] != '.')
                    ++start;

                int end = start;
                while (end < tail.length()
                       && (juce::CharacterFunctions::isDigit (tail[end])
                           || tail[end] == '-'
                           || tail[end] == '.'))
                    ++end;

                if (end > start)
                    return tail.substring (start, end).getFloatValue();
            }
            return fallback;
        };

        style.name = source.getFileNameWithoutExtension();
        style.frames = juce::jlimit (1, 256, juce::roundToInt (readNumber ({ "FrameNum", "Frames", "FrameCount", "frame_count" }, (float) style.frames)));
        style.size = juce::jlimit (32, 4096, juce::roundToInt (readNumber ({ "Width", "ImageWidth", "size", "Size" }, (float) style.size)));
        style.ringThickness = juce::jlimit (2.0f, 22.0f, readNumber ({ "RingWidth", "RingThickness", "LineWidth" }, style.ringThickness));
        style.pointerWidth = juce::jlimit (1.0f, 12.0f, readNumber ({ "PointerWidth", "NeedleWidth" }, style.pointerWidth));
        style.startAngle = juce::jlimit (-180.0f, 0.0f, readNumber ({ "StartAngle", "start_angle" }, style.startAngle));
        style.endAngle = juce::jlimit (0.0f, 180.0f, readNumber ({ "EndAngle", "end_angle" }, style.endAngle));

        clearImportedKnob();
        importedSourceFile = source;
        style.name = source.getFileNameWithoutExtension();
        syncControlsFromStyle();
        outputBox.setSelectedId (1, juce::dontSendNotification);
        return true;
    }

    void KnobBuilderComponent::importExistingKnob()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import Existing Knob Filmstrip or Source",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.png;*.jpg;*.jpeg;*.knob;*.patchcraft-knob.json;*.json");

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

                if (file.getFileExtension().equalsIgnoreCase (".knob"))
                {
                    juce::String error;
                    if (! loadKnobManFile (file, error))
                    {
                        juce::AlertWindow::showAsync (
                            juce::MessageBoxOptions()
                                .withTitle ("Import KnobMan File")
                                .withMessage (error)
                                .withButton ("OK")
                                .withIconType (juce::MessageBoxIconType::WarningIcon),
                            nullptr);
                    }
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
                    imageRoleBox.setEnabled (true);
                    imageFitBox.setEnabled (true);
                    imageScaleSlider.setEnabled (true);
                    imageRotationSlider.setEnabled (true);
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
        importedSourceFile = juce::File();
        importedFrameCount = 0;
        importedFrameSize = 0;
        importedStripVertical = true;
        importedBaseToggle.setToggleState (false, juce::dontSendNotification);
        overlayToggle.setToggleState (true, juce::dontSendNotification);
        importedBaseToggle.setEnabled (false);
        overlayToggle.setEnabled (false);
        importedBaseOpacitySlider.setEnabled (false);
        overlayOpacitySlider.setEnabled (false);
        imageRoleBox.setEnabled (false);
        imageFitBox.setEnabled (false);
        imageScaleSlider.setEnabled (false);
        imageRotationSlider.setEnabled (false);
        clearImportBtn.setEnabled (false);
        repaint();
    }

    void KnobBuilderComponent::loadProDemoKnob()
    {
        clearImportedKnob();

        style.name = "Obsidian Motion Encoder";
        style.size = 148;
        style.frames = 96;
        style.previewValue = 0.64f;
        style.startAngle = -145.0f;
        style.endAngle = 145.0f;
        style.ringThickness = 10.0f;
        style.pointerWidth = 5.0f;
        style.bevel = 0.58f;
        style.glow = 0.48f;
        style.imageScale = 1.0f;
        style.imageRotation = 0.0f;
        style.animationDepth = 0.62f;
        style.surfaceTexture = 0.32f;
        style.lightAngle = -52.0f;
        style.ringInset = 0.18f;
        style.pointerLength = 0.88f;
        style.motionCurve = 0.58f;
        style.ring = true;
        style.ticks = true;
        style.shadow = true;
        style.label = false;
        style.indicator = juce::Colour (0xffffb02e);
        style.ringColour = juce::Colour (0xffffa51d);
        style.backgroundColour = juce::Colour (0xff171b22);
        style.borderColour = juce::Colour (0xff05070a);
        style.tickColour = juce::Colour (0xff54d7ff);

        styleBox.setSelectedId (5, juce::dontSendNotification);
        indicatorBox.setSelectedId (3, juce::dontSendNotification);
        imageRoleBox.setSelectedId (1, juce::dontSendNotification);
        imageFitBox.setSelectedId (1, juce::dontSendNotification);
        animationBox.setSelectedId (3, juce::dontSendNotification);
        outputBox.setSelectedId (1, juce::dontSendNotification);

        for (auto& layer : buildLayers)
            layer.visible = true;

        sectionOpen = {{ true, false, true, true, true, true }};
        syncControlsFromStyle();
        resized();
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
        styleObj->setProperty ("imageScale", (double) style.imageScale);
        styleObj->setProperty ("imageRotation", (double) style.imageRotation);
        styleObj->setProperty ("animationDepth", (double) style.animationDepth);
        styleObj->setProperty ("surfaceTexture", (double) style.surfaceTexture);
        styleObj->setProperty ("lightAngle", (double) style.lightAngle);
        styleObj->setProperty ("ringInset", (double) style.ringInset);
        styleObj->setProperty ("pointerLength", (double) style.pointerLength);
        styleObj->setProperty ("motionCurve", (double) style.motionCurve);
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
        styleObj->setProperty ("imageRole", imageRoleBox.getText());
        styleObj->setProperty ("imageFit", imageFitBox.getText());
        styleObj->setProperty ("animationMode", animationBox.getText());
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

    bool KnobBuilderComponent::writeKnobAssetPackage (const juce::File& folder,
                                                      juce::File& renderedPng,
                                                      juce::String& error)
    {
        if (! folder.createDirectory())
        {
            error = "Could not create knob package folder: " + folder.getFullPathName();
            return false;
        }

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

        auto safeName = style.name.trim().isNotEmpty() ? style.name.trim() : juce::String ("patchcraft_knob");
        safeName = safeName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        safeName = safeName.replaceCharacter (' ', '_').toLowerCase();
        if (safeName.isEmpty())
            safeName = "patchcraft_knob";

        renderedPng = folder.getChildFile (safeName + ".png");
        juce::PNGImageFormat png;
        if (auto out = std::unique_ptr<juce::FileOutputStream> (renderedPng.createOutputStream()))
            png.writeImageToStream (strip, *out);

        if (! renderedPng.existsAsFile())
        {
            error = "Could not write knob filmstrip PNG.";
            return false;
        }

        if (! writeKnobSourceJson (folder.getChildFile (safeName + ".patchcraft-knob.json"), error))
            return false;

        auto* metadata = new juce::DynamicObject();
        metadata->setProperty ("asset_type", "knob");
        metadata->setProperty ("title", style.name);
        metadata->setProperty ("frames", total);
        metadata->setProperty ("frame_size", frameSize);
        metadata->setProperty ("filmstrip_orientation", vertical ? "vertical" : "horizontal");
        metadata->setProperty ("image_role", imageRoleBox.getText());
        metadata->setProperty ("animation_mode", animationBox.getText());
        metadata->setProperty ("source_import", importedSourceFile.exists() ? importedSourceFile.getFullPathName() : juce::String());
        metadata->setProperty ("created_at", juce::Time::getCurrentTime().toISO8601 (true));
        folder.getChildFile ("patchcraft-control-asset.json")
            .replaceWithText (juce::JSON::toString (juce::var (metadata), true));

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

    void KnobBuilderComponent::publishKnobToPluginClub()
    {
        auto safeName = style.name.trim().isNotEmpty() ? style.name.trim() : juce::String ("PatchCraft Knob");
        auto slug = safeName.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
        slug = slug.replaceCharacter (' ', '_');
        if (slug.isEmpty())
            slug = "patchcraft_knob";

        const auto packageFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("ControlAssetPublish")
            .getChildFile (slug + "-" + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

        juce::File renderedPng;
        juce::String error;
        if (! writeKnobAssetPackage (packageFolder, renderedPng, error))
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Publish Knob Asset")
                    .withMessage (error)
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
            return;
        }

        PluginClubPublisher::PublishArtifact artifact;
        artifact.kind = PluginClubPublisher::ArtifactKind::ControlAssetPack;
        artifact.title = safeName;
        artifact.description = "PatchCraft control asset: animated knob filmstrip with editable source.";
        artifact.creator = owner.getProject().getManifest().creator;
        artifact.category = "Controls / Knobs";
        artifact.version = "1.0.0";
        artifact.status = "draft";
        artifact.tags.add ("PatchCraft");
        artifact.tags.add ("Knob");
        artifact.tags.add ("Filmstrip");
        artifact.formats.add ("PNG Filmstrip");
        artifact.formats.add ("PatchCraft Builder JSON");
        artifact.sourcePath = packageFolder;

        auto* extra = new juce::DynamicObject();
        extra->setProperty ("asset_type", "knob");
        extra->setProperty ("frames", style.frames);
        extra->setProperty ("frame_size", style.size);
        artifact.extraMetadata = juce::var (extra);

        auto options = PluginClubPublisher::optionsFromCloudConfig (AiAssistService::loadCloudIntegrationConfig());
        juce::Component::SafePointer<KnobBuilderComponent> safeThis (this);
        std::thread ([safeThis, artifact, options]
        {
            const auto result = PluginClubPublisher::publishArtifact (artifact, options);
            juce::MessageManager::callAsync ([safeThis, result]
            {
                if (safeThis == nullptr)
                    return;

                auto message = result.message
                    + "\nArchive: " + result.archiveFile.getFullPathName()
                    + "\nPayload: " + result.payloadFile.getFullPathName();
                if (result.editUrl.isNotEmpty())
                    message += "\nEdit URL: " + result.editUrl;

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle (result.success ? "Knob Asset Published" : "Knob Asset Publish Failed")
                        .withMessage (message)
                        .withButton ("OK")
                        .withIconType (result.success ? juce::MessageBoxIconType::InfoIcon
                                                      : juce::MessageBoxIconType::WarningIcon),
                    nullptr);
            });
        }).detach();
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
