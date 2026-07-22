#include "SliderBuilderComponent.h"
#include "BuiltAssetLibraryComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <array>
#include <memory>

namespace patchcraft
{
    static void styleSliderBuilderLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        label.setFont (juce::Font (10.5f, juce::Font::bold));
    }

    SliderBuilderComponent::SliderBuilderComponent (StudioMainComponent& o) : owner (o)
    {
        orientationBox.addItem ("Vertical", 1);
        orientationBox.addItem ("Horizontal", 2);
        orientationBox.setSelectedId (1, juce::dontSendNotification);
        orientationBox.onChange = [this] { repaint(); };
        addAndMakeVisible (orientationBox);

        capBox.addItem ("Round Caps", 1);
        capBox.addItem ("Square Caps", 2);
        capBox.addItem ("Inset Rail", 3);
        capBox.setSelectedId (1, juce::dontSendNotification);
        capBox.onChange = [this] { repaint(); };
        addAndMakeVisible (capBox);

        fillModeBox.addItem ("Unipolar Fill", 1);
        fillModeBox.addItem ("Bipolar Fill", 2);
        fillModeBox.addItem ("LED Segments", 3);
        fillModeBox.setSelectedId (1, juce::dontSendNotification);
        fillModeBox.onChange = [this] { repaint(); };
        addAndMakeVisible (fillModeBox);

        configureSlider (widthSlider, 16, 180, 1, 42, " px");
        configureSlider (heightSlider, 64, 420, 1, 250, " px");
        configureSlider (trackSlider, 2, 32, 1, 10, " px");
        configureSlider (thumbSlider, 6, 48, 1, 24, " px");
        configureSlider (valueSlider, 0.0, 1.0, 0.001, 0.68);
        configureSlider (framesSlider, 8, 128, 1, 64);
        for (auto* slider : { &widthSlider, &heightSlider, &trackSlider, &thumbSlider, &valueSlider, &framesSlider })
        {
            slider->onValueChange = [this] { repaint(); };
            addAndMakeVisible (*slider);
        }

        for (auto* toggle : { &scaleToggle, &bipolarToggle, &labelToggle })
        {
            toggle->onClick = [this] { repaint(); };
            addAndMakeVisible (*toggle);
        }

        trackColourBtn.onClick = [this] { cycleColour (trackColour, trackColourBtn); };
        fillColourBtn.onClick = [this] { cycleColour (fillColour, fillColourBtn); };
        thumbColourBtn.onClick = [this] { cycleColour (thumbColour, thumbColourBtn); };
        for (auto* button : { &trackColourBtn, &fillColourBtn, &thumbColourBtn, &exportBtn, &exportJsonBtn, &addToProjectBtn })
            addAndMakeVisible (*button);
        exportBtn.getProperties().set ("accent", true);
        exportBtn.onClick = [this] { exportSliderFilmstrip(); };
        exportJsonBtn.onClick = [this] { exportSliderSourceJson(); };
        addToProjectBtn.onClick = [this] { addSliderToLibrary(); };

        importThumbBtn.onClick = [this] { importImage (0); };
        importTrackBtn.onClick = [this] { importImage (1); };
        importStripBtn.onClick = [this] { importImage (2); };
        clearImageBtn.onClick = [this]
        {
            importedThumb = importedTrack = importedStrip = {};
            importedSource = {};
            repaint();
        };
        for (auto* button : { &importThumbBtn, &importTrackBtn, &importStripBtn, &clearImageBtn })
            addAndMakeVisible (*button);

        styleSliderBuilderLabel (assetLbl, "SLIDER ASSET");
        styleSliderBuilderLabel (geometryLbl, "GEOMETRY");
        styleSliderBuilderLabel (paintLbl, "PAINT");
        styleSliderBuilderLabel (behaviorLbl, "OUTPUT / BEHAVIOR");
        styleSliderBuilderLabel (imageLbl, "IMPORT IMAGE -> WORKING ASSET");
        for (auto* label : { &assetLbl, &geometryLbl, &paintLbl, &behaviorLbl, &imageLbl })
            addAndMakeVisible (*label);

        cycleColour (trackColour, trackColourBtn);
        cycleColour (fillColour, fillColourBtn);
        cycleColour (thumbColour, thumbColourBtn);
    }

    void SliderBuilderComponent::configureSlider (juce::Slider& slider, double min, double max, double step,
                                                  double value, juce::String suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 58, 22);
        slider.setTextValueSuffix (suffix);
    }

    juce::StringArray SliderBuilderComponent::galleryPresetNames()
    {
        return {
            "Slim Studio Fader",
            "Wide Macro Slider",
            "LED Step Rail",
            "Dark Mixer Throw",
            "Glass Horizontal",
            "Tape Speed Control",
            "Master Bus Trim",
            "Neon Send Strip",
            "Minimal Utility",
            "Chunky Sampler Level"
        };
    }

    void SliderBuilderComponent::applyGalleryPreset (int index)
    {
        importedThumb = importedTrack = importedStrip = {};
        importedSource = {};

        auto setColours = [this] (juce::uint32 track, juce::uint32 fill, juce::uint32 thumb)
        {
            trackColour = juce::Colour (track);
            fillColour = juce::Colour (fill);
            thumbColour = juce::Colour (thumb);
            trackColourBtn.setColour (juce::TextButton::buttonColourId, trackColour);
            fillColourBtn.setColour (juce::TextButton::buttonColourId, fillColour);
            thumbColourBtn.setColour (juce::TextButton::buttonColourId, thumbColour);
        };

        auto setValues = [this] (int orientation, int cap, int fillMode, double width, double height,
                                 double track, double thumb, double value, bool scale,
                                 bool bipolar, bool label)
        {
            orientationBox.setSelectedId (orientation, juce::dontSendNotification);
            capBox.setSelectedId (cap, juce::dontSendNotification);
            fillModeBox.setSelectedId (fillMode, juce::dontSendNotification);
            widthSlider.setValue (width, juce::dontSendNotification);
            heightSlider.setValue (height, juce::dontSendNotification);
            trackSlider.setValue (track, juce::dontSendNotification);
            thumbSlider.setValue (thumb, juce::dontSendNotification);
            valueSlider.setValue (value, juce::dontSendNotification);
            framesSlider.setValue (64, juce::dontSendNotification);
            scaleToggle.setToggleState (scale, juce::dontSendNotification);
            bipolarToggle.setToggleState (bipolar, juce::dontSendNotification);
            labelToggle.setToggleState (label, juce::dontSendNotification);
        };

        switch (juce::jlimit (0, 9, index))
        {
            case 0:
                setValues (1, 1, 1, 38, 270, 10, 24, 0.68, true, false, false);
                setColours (0xff20242a, 0xfff1b34b, 0xffe7edf2);
                break;
            case 1:
                setValues (2, 1, 1, 34, 330, 12, 30, 0.54, false, false, true);
                setColours (0xff151b22, 0xff48c7e8, 0xfff4f8fb);
                break;
            case 2:
                setValues (1, 2, 3, 44, 250, 16, 18, 0.77, true, false, false);
                setColours (0xff101316, 0xff8cff67, 0xffe8ffe1);
                break;
            case 3:
                setValues (1, 3, 1, 54, 300, 18, 32, 0.61, false, false, false);
                setColours (0xff0d1014, 0xffff6b4a, 0xffd7dde4);
                break;
            case 4:
                setValues (2, 1, 1, 42, 300, 11, 26, 0.42, true, false, false);
                setColours (0xff0b1721, 0xff6de5ff, 0xffc7f5ff);
                break;
            case 5:
                setValues (2, 2, 2, 32, 280, 8, 22, 0.36, true, true, true);
                setColours (0xff2b2117, 0xffe0a45a, 0xffffd49b);
                break;
            case 6:
                setValues (1, 2, 2, 30, 220, 7, 20, 0.50, false, true, false);
                setColours (0xff24282d, 0xffb9c1ca, 0xfff5f7f9);
                break;
            case 7:
                setValues (1, 1, 1, 40, 280, 9, 26, 0.72, false, false, false);
                setColours (0xff10151d, 0xfff4408a, 0xffffb7d3);
                break;
            case 8:
                setValues (2, 2, 1, 24, 240, 6, 18, 0.33, false, false, false);
                setColours (0xffd9dee2, 0xff242a30, 0xff171b20);
                break;
            case 9:
                setValues (1, 1, 1, 64, 210, 18, 42, 0.84, false, false, true);
                setColours (0xff17181c, 0xffffcc4f, 0xffefeff2);
                break;
            default:
                break;
        }

        repaint();
    }

    void SliderBuilderComponent::paint (juce::Graphics& g)
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
        g.drawText ("Slider Parts", left.removeFromTop (24), juce::Justification::centredLeft);

        const juce::StringArray parts { "Thumb", "Fill", "Center Rail", "Scale Ticks", "Value Text", "Hit Area" };
        int y = left.getY() + 4;
        for (int i = 0; i < parts.size(); ++i)
        {
            auto row = juce::Rectangle<int> (left.getX(), y, left.getWidth(), 25);
            g.setColour (i == 0 ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
            g.fillRoundedRectangle (row.toFloat(), 5.0f);
            g.setColour (i == 0 ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
            g.drawRoundedRectangle (row.toFloat(), 5.0f, 1.0f);
            g.setColour (PatchCraftLookAndFeel::text());
            g.setFont (juce::Font (11.0f));
            g.drawText (parts[i], row.reduced (8, 0), juce::Justification::centredLeft);
            y += 29;
        }

        auto header = centre.removeFromTop (28);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("Live Slider Preview", header, juce::Justification::centredLeft);
        drawSliderPreview (g, centre.toFloat().reduced (28.0f), (float) valueSlider.getValue());
    }

    void SliderBuilderComponent::drawSliderPreview (juce::Graphics& g, juce::Rectangle<float> area, float value)
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const float width = (float) widthSlider.getValue();
        const float height = (float) heightSlider.getValue();
        auto preview = horizontal ? area.withSizeKeepingCentre (height, width)
                                  : area.withSizeKeepingCentre (width, height);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (preview.translated (0.0f, 4.0f), 10.0f);

        auto rail = horizontal ? preview.withHeight ((float) trackSlider.getValue()).withCentre ({ preview.getCentreX(), preview.getCentreY() })
                               : preview.withWidth ((float) trackSlider.getValue()).withCentre ({ preview.getCentreX(), preview.getCentreY() });
        const float corner = capBox.getSelectedId() == 2 ? 1.0f : rail.getHeight() * 0.5f;
        if (importedTrack.isValid())
        {
            g.drawImage (importedTrack, preview, juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            g.setColour (trackColour);
            g.fillRoundedRectangle (rail, corner);

            auto fill = rail;
            if (horizontal)
                fill.setRight (rail.getX() + rail.getWidth() * value);
            else
                fill.setTop (rail.getBottom() - rail.getHeight() * value);
            g.setColour (fillColour);
            g.fillRoundedRectangle (fill, corner);
        }

        if (scaleToggle.getToggleState())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            for (int i = 0; i <= 10; ++i)
            {
                const float p = (float) i / 10.0f;
                if (horizontal)
                {
                    const float x = preview.getX() + preview.getWidth() * p;
                    g.drawLine (x, preview.getBottom() + 6.0f, x, preview.getBottom() + (i % 5 == 0 ? 14.0f : 10.0f));
                }
                else
                {
                    const float y = preview.getBottom() - preview.getHeight() * p;
                    g.drawLine (preview.getRight() + 6.0f, y, preview.getRight() + (i % 5 == 0 ? 14.0f : 10.0f), y);
                }
            }
        }

        const float thumb = (float) thumbSlider.getValue();
        const auto thumbCentre = horizontal
            ? juce::Point<float> (rail.getX() + rail.getWidth() * value, rail.getCentreY())
            : juce::Point<float> (rail.getCentreX(), rail.getBottom() - rail.getHeight() * value);
        if (importedThumb.isValid())
        {
            const float tw = thumb * 1.6f;
            const float ar = (float) importedThumb.getHeight() / juce::jmax (1.0f, (float) importedThumb.getWidth());
            const float th = tw * ar;
            g.drawImage (importedThumb,
                         juce::Rectangle<float> (thumbCentre.x - tw * 0.5f, thumbCentre.y - th * 0.5f, tw, th),
                         juce::RectanglePlacement::centred);
        }
        else
        {
            g.setColour (thumbColour);
            g.fillEllipse (thumbCentre.x - thumb * 0.5f, thumbCentre.y - thumb * 0.5f, thumb, thumb);
            g.setColour (PatchCraftLookAndFeel::border());
            g.drawEllipse (thumbCentre.x - thumb * 0.5f, thumbCentre.y - thumb * 0.5f, thumb, thumb, 1.2f);
        }
    }

    void SliderBuilderComponent::resized()
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
        row (capBox);
        row (fillModeBox);
        header (geometryLbl);
        row (widthSlider);
        row (heightSlider);
        row (trackSlider);
        row (thumbSlider);
        row (valueSlider);
        header (paintLbl);
        auto colours = right.removeFromTop (30);
        trackColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        fillColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        thumbColourBtn.setBounds (colours.removeFromLeft (92).reduced (2));
        right.removeFromTop (6);
        header (imageLbl);
        auto imgRow = right.removeFromTop (28);
        importThumbBtn.setBounds (imgRow.removeFromLeft (68).reduced (2));
        importTrackBtn.setBounds (imgRow.removeFromLeft (68).reduced (2));
        importStripBtn.setBounds (imgRow.removeFromLeft (68).reduced (2));
        clearImageBtn.setBounds (imgRow.reduced (2));
        right.removeFromTop (6);
        header (behaviorLbl);
        scaleToggle.setBounds (right.removeFromTop (24));
        bipolarToggle.setBounds (right.removeFromTop (24));
        labelToggle.setBounds (right.removeFromTop (24));
        row (framesSlider);
        auto actions = right.removeFromTop (30);
        exportBtn.setBounds (actions.removeFromLeft (92).reduced (2));
        exportJsonBtn.setBounds (actions.removeFromLeft (94).reduced (2));
        addToProjectBtn.setBounds (actions.reduced (2));
    }

    void SliderBuilderComponent::cycleColour (juce::Colour& colour, juce::TextButton& button)
    {
        const std::array<juce::Colour, 7> palette
        {
            juce::Colour (0xff202227), juce::Colour (0xfff5a623), juce::Colour (0xff4fc3f7),
            juce::Colour (0xff8e6cd6), juce::Colour (0xff5fb37b), juce::Colour (0xffd9dde2),
            juce::Colour (0xff080a0d)
        };
        int next = 0;
        for (int i = 0; i < (int) palette.size(); ++i)
            if (palette[(size_t) i] == colour)
                next = (i + 1) % (int) palette.size();
        colour = palette[(size_t) next];
        button.setColour (juce::TextButton::buttonColourId, colour);
        repaint();
    }

    juce::Image SliderBuilderComponent::renderSliderFrame (int index, int totalFrames)
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const int narrow = juce::jmax (16, juce::roundToInt (widthSlider.getValue()));
        const int longSide = juce::jmax (64, juce::roundToInt (heightSlider.getValue()));
        const int frameW = horizontal ? longSide + 28 : narrow + 28;
        const int frameH = horizontal ? narrow + 28 : longSide + 28;
        juce::Image image (juce::Image::ARGB, frameW, frameH, true);
        juce::Graphics g (image);
        const float value = totalFrames > 1 ? (float) index / (float) (totalFrames - 1)
                                            : (float) valueSlider.getValue();
        drawSliderPreview (g, image.getBounds().toFloat().reduced (8.0f), value);
        return image;
    }

    juce::Image SliderBuilderComponent::renderSliderFilmstrip (bool verticalStrip)
    {
        // A ready-made imported filmstrip is the working asset as-is.
        if (importedStrip.isValid())
            return importedStrip;

        const int total = juce::jlimit (1, 256, juce::roundToInt (framesSlider.getValue()));
        auto first = renderSliderFrame (0, total);
        juce::Image strip (juce::Image::ARGB,
                           verticalStrip ? first.getWidth() : first.getWidth() * total,
                           verticalStrip ? first.getHeight() * total : first.getHeight(),
                           true);
        juce::Graphics g (strip);
        g.drawImageAt (first, 0, 0);
        for (int i = 1; i < total; ++i)
            g.drawImageAt (renderSliderFrame (i, total),
                           verticalStrip ? 0 : i * first.getWidth(),
                           verticalStrip ? i * first.getHeight() : 0);
        return strip;
    }

    juce::var SliderBuilderComponent::buildSliderSourceVar (bool verticalStrip) const
    {
        const bool horizontal = orientationBox.getSelectedId() == 2;
        const int narrow = juce::jmax (16, juce::roundToInt (widthSlider.getValue()));
        const int longSide = juce::jmax (64, juce::roundToInt (heightSlider.getValue()));
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "PatchCraft Builder Asset");
        root->setProperty ("formatVersion", 1);
        root->setProperty ("assetType", "slider");
        root->setProperty ("frames", juce::jlimit (1, 256, juce::roundToInt (framesSlider.getValue())));
        root->setProperty ("stripVertical", verticalStrip);
        root->setProperty ("frameWidth", horizontal ? longSide + 28 : narrow + 28);
        root->setProperty ("frameHeight", horizontal ? narrow + 28 : longSide + 28);

        auto* style = new juce::DynamicObject();
        style->setProperty ("orientation", orientationBox.getText());
        style->setProperty ("cap", capBox.getText());
        style->setProperty ("fillMode", fillModeBox.getText());
        style->setProperty ("width", narrow);
        style->setProperty ("height", longSide);
        style->setProperty ("track", (int) trackSlider.getValue());
        style->setProperty ("thumb", (int) thumbSlider.getValue());
        style->setProperty ("previewValue", valueSlider.getValue());
        style->setProperty ("scaleMarks", scaleToggle.getToggleState());
        style->setProperty ("bipolar", bipolarToggle.getToggleState());
        style->setProperty ("label", labelToggle.getToggleState());
        style->setProperty ("trackColour", trackColour.toString());
        style->setProperty ("fillColour", fillColour.toString());
        style->setProperty ("thumbColour", thumbColour.toString());
        root->setProperty ("style", juce::var (style));
        return juce::var (root);
    }

    bool SliderBuilderComponent::writeSliderSourceJson (const juce::File& destination,
                                                        bool verticalStrip,
                                                        juce::String& error) const
    {
        if (! destination.getParentDirectory().createDirectory())
        {
            error = "Could not create destination folder.";
            return false;
        }

        if (! destination.replaceWithText (juce::JSON::toString (buildSliderSourceVar (verticalStrip), true)))
        {
            error = "Could not write slider builder source JSON.";
            return false;
        }

        return true;
    }

    void SliderBuilderComponent::exportSliderFilmstrip()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Slider Filmstrip",
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
                    png.writeImageToStream (renderSliderFilmstrip (verticalStrip), *out);

                juce::String error;
                writeSliderSourceJson (destination.withFileExtension ("patchcraft-slider.json"), verticalStrip, error);
            });
    }

    void SliderBuilderComponent::exportSliderSourceJson()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export Editable Slider Source",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.patchcraft-slider.json;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto destination = fc.getResult();
                if (destination == juce::File())
                    return;
                if (destination.getFileExtension().isEmpty())
                    destination = destination.withFileExtension ("patchcraft-slider.json");

                juce::String error;
                if (! writeSliderSourceJson (destination, true, error))
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Export Slider Source")
                            .withMessage (error)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
            });
    }

    void SliderBuilderComponent::importImage (int slot)
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            slot == 2 ? "Import Slider Filmstrip" : (slot == 0 ? "Import Thumb Image" : "Import Track Image"),
            juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
            "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser, slot] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                auto image = juce::ImageFileFormat::loadFrom (file);
                if (! image.isValid())
                    return;

                importedSource = file;
                if (slot == 0)      importedThumb = image;
                else if (slot == 1) importedTrack = image;
                else
                {
                    importedStrip = image;
                    // Best-effort frame count: assume square frames along the long axis.
                    const int w = image.getWidth(), h = image.getHeight();
                    const int inferred = h >= w ? (w > 0 ? h / w : 1) : (h > 0 ? w / h : 1);
                    if (inferred > 1)
                        framesSlider.setValue (juce::jlimit (1, 256, inferred), juce::sendNotificationSync);
                }
                repaint();
            });
    }

    void SliderBuilderComponent::addSliderToLibrary()
    {
        auto folder = BuiltAssetLibraryComponent::getCategoryFolder ("sliders");
        folder.createDirectory();

        auto safeName = "slider_" + orientationBox.getText().toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
        safeName = safeName.replaceCharacter (' ', '_');
        auto destination = folder.getChildFile (safeName + ".png");
        for (int i = 2; destination.existsAsFile(); ++i)
            destination = folder.getChildFile (safeName + "_" + juce::String (i) + ".png");

        constexpr bool verticalStrip = true;
        juce::PNGImageFormat png;
        if (auto out = std::unique_ptr<juce::FileOutputStream> (destination.createOutputStream()))
            png.writeImageToStream (renderSliderFilmstrip (verticalStrip), *out);

        juce::String error;
        writeSliderSourceJson (destination.withFileExtension ("patchcraft-slider.json"), verticalStrip, error);
        owner.refreshAllPanels();
    }
}
