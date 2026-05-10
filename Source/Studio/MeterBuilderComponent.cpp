#include "MeterBuilderComponent.h"
#include "BuiltAssetLibraryComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <array>
#include <memory>

namespace patchcraft
{
    static void styleMeterBuilderLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        label.setFont (juce::Font (10.5f, juce::Font::bold));
    }

    MeterBuilderComponent::MeterBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        orientationBox.addItem ("Vertical", 1);
        orientationBox.addItem ("Horizontal", 2);
        orientationBox.setSelectedId (1, juce::dontSendNotification);
        orientationBox.onChange = [this] { repaint(); };
        addAndMakeVisible (orientationBox);

        styleBox.addItem ("Segmented LED", 1);
        styleBox.addItem ("Smooth Bar", 2);
        styleBox.addItem ("Needle VU", 3);
        styleBox.addItem ("Spectrum Column", 4);
        styleBox.setSelectedId (1, juce::dontSendNotification);
        styleBox.onChange = [this] { repaint(); };
        addAndMakeVisible (styleBox);

        scaleBox.addItem ("-60 to 0 dB", 1);
        scaleBox.addItem ("-36 to +6 dB", 2);
        scaleBox.addItem ("0 to 100%", 3);
        scaleBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (scaleBox);

        configureSlider (widthSlider, 12, 180, 1, 34, " px");
        configureSlider (heightSlider, 64, 420, 1, 260, " px");
        configureSlider (segmentSlider, 4, 48, 1, 22);
        configureSlider (valueSlider, 0.0, 1.0, 0.001, 0.72);
        configureSlider (warningSlider, 0.45, 0.95, 0.01, 0.72);
        configureSlider (peakSlider, 0.0, 1.0, 0.001, 0.88);
        for (auto* slider : { &widthSlider, &heightSlider, &segmentSlider, &valueSlider, &warningSlider, &peakSlider })
        {
            slider->onValueChange = [this] { repaint(); };
            addAndMakeVisible (*slider);
        }

        for (auto* toggle : { &peakHoldToggle, &dbScaleToggle, &stereoToggle })
        {
            toggle->onClick = [this] { repaint(); };
            addAndMakeVisible (*toggle);
        }
        peakHoldToggle.setToggleState (true, juce::dontSendNotification);
        dbScaleToggle.setToggleState (true, juce::dontSendNotification);

        lowColourBtn.onClick = [this] { cycleColour (lowColour, lowColourBtn); };
        midColourBtn.onClick = [this] { cycleColour (midColour, midColourBtn); };
        highColourBtn.onClick = [this] { cycleColour (highColour, highColourBtn); };
        for (auto* button : { &lowColourBtn, &midColourBtn, &highColourBtn, &exportBtn, &exportJsonBtn, &addToProjectBtn })
            addAndMakeVisible (*button);
        exportBtn.getProperties().set ("accent", true);
        exportBtn.onClick = [this] { exportMeterFilmstrip(); };
        exportJsonBtn.onClick = [this] { exportMeterSourceJson(); };
        addToProjectBtn.onClick = [this] { addMeterToLibrary(); };

        styleMeterBuilderLabel (assetLbl, "METER ASSET");
        styleMeterBuilderLabel (geometryLbl, "RANGE / GEOMETRY");
        styleMeterBuilderLabel (behaviorLbl, "BEHAVIOR");
        for (auto* label : { &assetLbl, &geometryLbl, &behaviorLbl })
            addAndMakeVisible (*label);

        lowColourBtn.setColour (juce::TextButton::buttonColourId, lowColour);
        midColourBtn.setColour (juce::TextButton::buttonColourId, midColour);
        highColourBtn.setColour (juce::TextButton::buttonColourId, highColour);
    }

    void MeterBuilderComponent::configureSlider (juce::Slider& slider, double min, double max, double step,
                                                 double value, juce::String suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 58, 22);
        slider.setTextValueSuffix (suffix);
    }

    void MeterBuilderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        auto bounds = getLocalBounds().reduced (2);
        auto left = bounds.removeFromLeft (210).reduced (6);
        auto right = bounds.removeFromRight (300).reduced (6);
        auto centre = bounds.reduced (6);

        PatchCraftLookAndFeel::drawPanel (g, left, 8.0f);
        PatchCraftLookAndFeel::drawPanel (g, right, 8.0f);
        PatchCraftLookAndFeel::drawDarkPanel (g, centre, 10.0f);

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("Meter Components", left.removeFromTop (24), juce::Justification::centredLeft);

        const juce::StringArray rows { "Input Bar", "Peak Marker", "Warning Zone", "Clip Zone", "dB Labels", "Stereo Link" };
        int y = left.getY() + 4;
        for (int i = 0; i < rows.size(); ++i)
        {
            auto row = juce::Rectangle<int> (left.getX(), y, left.getWidth(), 25);
            g.setColour (i == 0 ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
            g.fillRoundedRectangle (row.toFloat(), 5.0f);
            g.setColour (i == 0 ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (row.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (11.0f));
            g.drawText (rows[i], row.reduced (8, 0), juce::Justification::centredLeft);
            y += 29;
        }

        auto header = centre.removeFromTop (28);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("Live Meter Preview", header, juce::Justification::centredLeft);
        drawMeterPreview (g, centre.toFloat().reduced (28.0f));
    }

    void MeterBuilderComponent::drawMeterPreview (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const bool stereo = stereoToggle.getToggleState();
        const float width = (float) widthSlider.getValue();
        const float height = (float) heightSlider.getValue();
        auto meter = horizontal ? area.withSizeKeepingCentre (height, width)
                                : area.withSizeKeepingCentre (width * (stereo ? 2.3f : 1.0f), height);

        const int lanes = stereo ? 2 : 1;
        for (int lane = 0; lane < lanes; ++lane)
        {
            auto laneBounds = meter;
            if (stereo)
                laneBounds = meter.removeFromLeft (meter.getWidth() / (float) (2 - lane)).reduced (lane == 0 ? 4.0f : 0.0f, 0.0f);

            g.setColour (PatchCraftLookAndFeel::bg());
            g.fillRoundedRectangle (laneBounds, 5.0f);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (laneBounds, 5.0f, 1.0f);

            const int segments = juce::jlimit (4, 48, (int) segmentSlider.getValue());
            const float value = juce::jlimit (0.0f, 1.0f, (float) valueSlider.getValue() - lane * 0.08f);
            for (int i = 0; i < segments; ++i)
            {
                const float pos = (float) (i + 1) / (float) segments;
                juce::Colour colour = pos > 0.9f ? highColour : (pos > (float) warningSlider.getValue() ? midColour : lowColour);
                colour = i < (int) (value * segments) ? colour : PatchCraftLookAndFeel::borderSoft();

                if (horizontal)
                {
                    auto seg = laneBounds.reduced (3.0f);
                    const float segW = seg.getWidth() / (float) segments;
                    seg.setX (seg.getX() + i * segW);
                    seg.setWidth (juce::jmax (1.0f, segW - 2.0f));
                    g.setColour (colour);
                    g.fillRoundedRectangle (seg, 2.0f);
                }
                else
                {
                    auto seg = laneBounds.reduced (3.0f);
                    const float segH = seg.getHeight() / (float) segments;
                    seg.setY (seg.getBottom() - (i + 1) * segH);
                    seg.setHeight (juce::jmax (1.0f, segH - 2.0f));
                    g.setColour (colour);
                    g.fillRoundedRectangle (seg, 2.0f);
                }
            }

            if (peakHoldToggle.getToggleState())
            {
                g.setColour (highColour.brighter (0.25f));
                if (horizontal)
                {
                    const float x = laneBounds.getX() + laneBounds.getWidth() * (float) peakSlider.getValue();
                    g.drawLine (x, laneBounds.getY() - 4.0f, x, laneBounds.getBottom() + 4.0f, 2.0f);
                }
                else
                {
                    const float y = laneBounds.getBottom() - laneBounds.getHeight() * (float) peakSlider.getValue();
                    g.drawLine (laneBounds.getX() - 4.0f, y, laneBounds.getRight() + 4.0f, y, 2.0f);
                }
            }
        }
    }

    void MeterBuilderComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (2);
        bounds.removeFromLeft (210);
        auto right = bounds.removeFromRight (300).reduced (16, 12);

        auto header = [&] (juce::Label& label)
        {
            label.setBounds (right.removeFromTop (18));
            right.removeFromTop (4);
        };
        auto row = [&] (juce::Component& component)
        {
            component.setBounds (right.removeFromTop (25));
            right.removeFromTop (5);
        };

        header (assetLbl);
        row (orientationBox);
        row (styleBox);
        row (scaleBox);
        header (geometryLbl);
        row (widthSlider);
        row (heightSlider);
        row (segmentSlider);
        row (valueSlider);
        row (warningSlider);
        row (peakSlider);
        auto colours = right.removeFromTop (30);
        lowColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        midColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        highColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        right.removeFromTop (6);
        header (behaviorLbl);
        peakHoldToggle.setBounds (right.removeFromTop (24));
        dbScaleToggle.setBounds (right.removeFromTop (24));
        stereoToggle.setBounds (right.removeFromTop (24));
        right.removeFromTop (8);
        auto actions = right.removeFromTop (30);
        exportBtn.setBounds (actions.removeFromLeft (92).reduced (2));
        exportJsonBtn.setBounds (actions.removeFromLeft (94).reduced (2));
        addToProjectBtn.setBounds (actions.reduced (2));
    }

    void MeterBuilderComponent::cycleColour (juce::Colour& colour, juce::TextButton& button)
    {
        const std::array<juce::Colour, 7> palette
        {
            juce::Colour (0xff5fb37b), juce::Colour (0xfff5a623), juce::Colour (0xffe24d42),
            juce::Colour (0xff4fc3f7), juce::Colour (0xff8e6cd6), juce::Colour (0xffd9dde2),
            juce::Colour (0xff202227)
        };
        int next = 0;
        for (int i = 0; i < (int) palette.size(); ++i)
            if (palette[(size_t) i] == colour)
                next = (i + 1) % (int) palette.size();
        colour = palette[(size_t) next];
        button.setColour (juce::TextButton::buttonColourId, colour);
        repaint();
    }

    juce::Image MeterBuilderComponent::renderMeterFrame (int index, int totalFrames)
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const bool stereo = stereoToggle.getToggleState();
        const int narrow = juce::jmax (12, juce::roundToInt (widthSlider.getValue()));
        const int longSide = juce::jmax (64, juce::roundToInt (heightSlider.getValue()));
        const int frameW = horizontal ? longSide + 28 : juce::roundToInt ((float) narrow * (stereo ? 2.3f : 1.0f)) + 28;
        const int frameH = horizontal ? narrow + 28 : longSide + 28;
        juce::Image image (juce::Image::ARGB, frameW, frameH, true);
        juce::Graphics g (image);

        const float generatedValue = totalFrames > 1 ? (float) index / (float) (totalFrames - 1)
                                                     : (float) valueSlider.getValue();
        const auto previousValue = valueSlider.getValue();
        valueSlider.setValue (generatedValue, juce::dontSendNotification);
        drawMeterPreview (g, image.getBounds().toFloat().reduced (8.0f));
        valueSlider.setValue (previousValue, juce::dontSendNotification);
        return image;
    }

    juce::Image MeterBuilderComponent::renderMeterFilmstrip (bool verticalStrip)
    {
        constexpr int total = 64;
        auto first = renderMeterFrame (0, total);
        juce::Image strip (juce::Image::ARGB,
                           verticalStrip ? first.getWidth() : first.getWidth() * total,
                           verticalStrip ? first.getHeight() * total : first.getHeight(),
                           true);
        juce::Graphics g (strip);
        g.drawImageAt (first, 0, 0);
        for (int i = 1; i < total; ++i)
            g.drawImageAt (renderMeterFrame (i, total),
                           verticalStrip ? 0 : i * first.getWidth(),
                           verticalStrip ? i * first.getHeight() : 0);
        return strip;
    }

    juce::var MeterBuilderComponent::buildMeterSourceVar (bool verticalStrip) const
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const bool stereo = stereoToggle.getToggleState();
        const int narrow = juce::jmax (12, juce::roundToInt (widthSlider.getValue()));
        const int longSide = juce::jmax (64, juce::roundToInt (heightSlider.getValue()));
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "PatchCraft Builder Asset");
        root->setProperty ("formatVersion", 1);
        root->setProperty ("assetType", "meter");
        root->setProperty ("frames", 64);
        root->setProperty ("stripVertical", verticalStrip);
        root->setProperty ("frameWidth", horizontal ? longSide + 28 : juce::roundToInt ((float) narrow * (stereo ? 2.3f : 1.0f)) + 28);
        root->setProperty ("frameHeight", horizontal ? narrow + 28 : longSide + 28);

        auto* style = new juce::DynamicObject();
        style->setProperty ("orientation", orientationBox.getText());
        style->setProperty ("style", styleBox.getText());
        style->setProperty ("scale", scaleBox.getText());
        style->setProperty ("width", narrow);
        style->setProperty ("height", longSide);
        style->setProperty ("segments", (int) segmentSlider.getValue());
        style->setProperty ("previewValue", valueSlider.getValue());
        style->setProperty ("warning", warningSlider.getValue());
        style->setProperty ("peak", peakSlider.getValue());
        style->setProperty ("peakHold", peakHoldToggle.getToggleState());
        style->setProperty ("dbScale", dbScaleToggle.getToggleState());
        style->setProperty ("stereo", stereo);
        style->setProperty ("lowColour", lowColour.toString());
        style->setProperty ("midColour", midColour.toString());
        style->setProperty ("highColour", highColour.toString());
        root->setProperty ("style", juce::var (style));
        return juce::var (root);
    }

    bool MeterBuilderComponent::writeMeterSourceJson (const juce::File& destination,
                                                      bool verticalStrip,
                                                      juce::String& error) const
    {
        if (! destination.getParentDirectory().createDirectory())
        {
            error = "Could not create destination folder.";
            return false;
        }

        if (! destination.replaceWithText (juce::JSON::toString (buildMeterSourceVar (verticalStrip), true)))
        {
            error = "Could not write meter builder source JSON.";
            return false;
        }

        return true;
    }

    void MeterBuilderComponent::exportMeterFilmstrip()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Meter Filmstrip",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.png");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto destination = fc.getResult();
                if (destination == juce::File())
                    return;
                if (destination.getFileExtension().isEmpty())
                    destination = destination.withFileExtension ("png");

                constexpr bool verticalStrip = true;
                juce::PNGImageFormat png;
                if (auto out = std::unique_ptr<juce::FileOutputStream> (destination.createOutputStream()))
                    png.writeImageToStream (renderMeterFilmstrip (verticalStrip), *out);

                juce::String error;
                writeMeterSourceJson (destination.withFileExtension ("patchcraft-meter.json"), verticalStrip, error);
            });
    }

    void MeterBuilderComponent::exportMeterSourceJson()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Editable Meter Source",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.patchcraft-meter.json;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto destination = fc.getResult();
                if (destination == juce::File())
                    return;
                if (destination.getFileExtension().isEmpty())
                    destination = destination.withFileExtension ("patchcraft-meter.json");

                juce::String error;
                if (! writeMeterSourceJson (destination, true, error))
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Export Meter Source")
                            .withMessage (error)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
            });
    }

    void MeterBuilderComponent::addMeterToLibrary()
    {
        auto folder = BuiltAssetLibraryComponent::getCategoryFolder ("meters");
        folder.createDirectory();

        auto safeName = "meter_" + styleBox.getText().toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
        safeName = safeName.replaceCharacter (' ', '_');
        auto destination = folder.getChildFile (safeName + ".png");
        for (int i = 2; destination.existsAsFile(); ++i)
            destination = folder.getChildFile (safeName + "_" + juce::String (i) + ".png");

        constexpr bool verticalStrip = true;
        juce::PNGImageFormat png;
        if (auto out = std::unique_ptr<juce::FileOutputStream> (destination.createOutputStream()))
            png.writeImageToStream (renderMeterFilmstrip (verticalStrip), *out);

        juce::String error;
        writeMeterSourceJson (destination.withFileExtension ("patchcraft-meter.json"), verticalStrip, error);
        owner.refreshAllPanels();
    }
}
