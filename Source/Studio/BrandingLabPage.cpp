#include "BrandingLabPage.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "TestPage.h"

namespace patchcraft
{
    namespace
    {
        static juce::String colourToHex (juce::Colour c)
        {
            return "#" + juce::String::toHexString ((int) c.getARGB()).paddedLeft ('0', 8).toUpperCase();
        }
    }

    BrandingLabPage::BrandingLabPage (StudioMainComponent& o) : owner (o)
    {
        setOpaque (true);

        header.setText ("Branding Lab", juce::dontSendNotification);
        header.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        header.setFont (juce::Font (16.0f, juce::Font::bold));
        addAndMakeVisible (header);

        subtitle.setText ("Brand the Player chrome. Fields here drive what end-users see in the loaded plugin: display name, tagline, logo, and the player’s skin colours.",
                          juce::dontSendNotification);
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        subtitle.setFont (juce::Font (11.5f));
        subtitle.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (subtitle);

        // Field labels + editors.
        struct EditorWiring { juce::Label* label; const char* text; juce::TextEditor* editor; const char* placeholder; };
        const EditorWiring editors[] = {
            { &displayNameLabel, "Display Name",  &displayNameEdit,  "e.g. Cinematic Evolve Pad" },
            { &taglineLabel,     "Tagline",       &taglineEdit,      "Short marketing line" },
            { &creatorLabel,     "Creator",       &creatorEdit,      "Brand or studio name" },
            { &versionLabel,     "Version",       &versionEdit,      "1.0" },
            { &websiteLabel,     "Website",       &websiteEdit,      "https://..." },
            { &logoLabel,        "Logo Image",    &logoPathEdit,     "assets/logo.png" }
        };
        for (const auto& w : editors)
        {
            w.label->setText (w.text, juce::dontSendNotification);
            w.label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            w.label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*w.label);

            w.editor->setMultiLine (false);
            w.editor->setTextToShowWhenEmpty (w.placeholder, PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
            w.editor->onTextChange = [this] { if (! syncingFromManifest) writeToManifest(); };
            addAndMakeVisible (*w.editor);
        }

        browseLogoBtn.setTooltip ("Pick a PNG/JPG that becomes the player’s logo image.");
        browseLogoBtn.onClick = [this] { chooseLogo(); };
        addAndMakeVisible (browseLogoBtn);

        clearLogoBtn.setTooltip ("Remove the logo image so the Player falls back to the title text.");
        clearLogoBtn.onClick = [this]
        {
            logoPathEdit.setText (juce::String(), juce::sendNotificationSync);
            writeToManifest();
        };
        addAndMakeVisible (clearLogoBtn);

        struct SwatchWiring { juce::Label* label; const char* text; juce::TextButton* swatch; const char* fieldId; };
        const SwatchWiring swatches[] = {
            { &accentLabel, "Accent",     &accentSwatch, "accent" },
            { &panelLabel,  "Panel",      &panelSwatch,  "panel" },
            { &bgLabel,     "Background", &bgSwatch,     "bg" },
            { &textLabel,   "Text",       &textSwatch,   "text" }
        };
        for (const auto& s : swatches)
        {
            s.label->setText (s.text, juce::dontSendNotification);
            s.label->setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            s.label->setFont (juce::Font (11.5f, juce::Font::bold));
            addAndMakeVisible (*s.label);

            const juce::String fieldId = s.fieldId;
            s.swatch->setTooltip ("Click to pick a new colour for the " + juce::String (s.text) + " role.");
            s.swatch->onClick = [this, fieldId]
            {
                const auto& mfst = owner.getProject().getManifest();
                const auto current = fieldId == "accent" ? mfst.playerAccentColour
                                   : fieldId == "panel"  ? mfst.playerPanelColour
                                   : fieldId == "bg"     ? mfst.playerBackgroundColour
                                                          : mfst.playerTextColour;
                chooseColour (fieldId, current);
            };
            addAndMakeVisible (*s.swatch);
        }

        resetColoursBtn.setTooltip ("Restore the default Player skin colours.");
        resetColoursBtn.onClick = [this]
        {
            auto& m = owner.getProject().getManifest();
            m.playerAccentColour     = juce::Colour (0xfff5a623);
            m.playerPanelColour      = juce::Colour (0xff15171b);
            m.playerBackgroundColour = juce::Colour (0xff0b0d10);
            m.playerTextColour       = juce::Colour (0xffe6e6e6);
            m.playerTextDimColour    = juce::Colour (0xff8b9098);
            m.playerBorderColour     = juce::Colour (0xff2a2a2a);
            owner.getProject().notifyChanged();
            refresh();
        };
        addAndMakeVisible (resetColoursBtn);

        // Live instrument inside a Player-shaped frame.
        testPage = std::make_unique<TestPage> (owner);
        addAndMakeVisible (*testPage);

        showFormToggle.setToggleState (true, juce::dontSendNotification);
        showFormToggle.setTooltip ("Toggle the branding form column. Hide it to preview the player at full width.");
        showFormToggle.onClick = [this]
        {
            const bool show = showFormToggle.getToggleState();
            displayNameLabel.setVisible (show); displayNameEdit.setVisible (show);
            taglineLabel.setVisible (show);     taglineEdit.setVisible (show);
            creatorLabel.setVisible (show);     creatorEdit.setVisible (show);
            websiteLabel.setVisible (show);     websiteEdit.setVisible (show);
            versionLabel.setVisible (show);     versionEdit.setVisible (show);
            logoLabel.setVisible (show);        logoPathEdit.setVisible (show);
            browseLogoBtn.setVisible (show);    clearLogoBtn.setVisible (show);
            accentLabel.setVisible (show);      accentSwatch.setVisible (show);
            panelLabel.setVisible (show);       panelSwatch.setVisible (show);
            bgLabel.setVisible (show);          bgSwatch.setVisible (show);
            textLabel.setVisible (show);        textSwatch.setVisible (show);
            resetColoursBtn.setVisible (show);
            resized();
            repaint();
        };
        addAndMakeVisible (showFormToggle);

        startTimerHz (4);
        refresh();
    }

    BrandingLabPage::~BrandingLabPage() = default;

    void BrandingLabPage::activateTest()    { if (testPage) testPage->activate(); }
    void BrandingLabPage::deactivateTest()  { if (testPage) testPage->deactivate(); }
    bool BrandingLabPage::isTestActive() const { return testPage && testPage->isAudioRunning(); }

    void BrandingLabPage::timerCallback()
    {
        // Cheap polling — the project doesn't broadcast manifest mutations
        // through any specific channel, but it does increment a version on
        // notifyChanged. Just rerun the read each tick if we aren't typing.
        if (! displayNameEdit.hasKeyboardFocus (true)
            && ! taglineEdit.hasKeyboardFocus (true)
            && ! creatorEdit.hasKeyboardFocus (true)
            && ! websiteEdit.hasKeyboardFocus (true)
            && ! versionEdit.hasKeyboardFocus (true)
            && ! logoPathEdit.hasKeyboardFocus (true))
        {
            readFromManifest();
        }
        repaint();
    }

    void BrandingLabPage::refresh()
    {
        readFromManifest();
        repaint();
    }

    void BrandingLabPage::readFromManifest()
    {
        const auto& m = owner.getProject().getManifest();
        syncingFromManifest = true;
        displayNameEdit.setText (m.playerDisplayName, juce::dontSendNotification);
        taglineEdit.setText (m.playerTagline, juce::dontSendNotification);
        creatorEdit.setText (m.creator, juce::dontSendNotification);
        websiteEdit.setText (m.website, juce::dontSendNotification);
        versionEdit.setText (m.version, juce::dontSendNotification);
        logoPathEdit.setText (m.playerLogoImage, juce::dontSendNotification);
        syncingFromManifest = false;
    }

    void BrandingLabPage::writeToManifest()
    {
        auto& m = owner.getProject().getManifest();
        m.playerDisplayName = displayNameEdit.getText().trim();
        m.playerTagline     = taglineEdit.getText().trim();
        m.creator           = creatorEdit.getText().trim();
        m.website           = websiteEdit.getText().trim();
        m.version           = versionEdit.getText().trim();
        m.playerLogoImage   = logoPathEdit.getText().trim();
        owner.getProject().notifyChanged();
        repaint();
    }

    void BrandingLabPage::chooseColour (const juce::String& field, juce::Colour current)
    {
        auto* selector = new juce::ColourSelector (juce::ColourSelector::showColourAtTop
                                                   | juce::ColourSelector::showSliders
                                                   | juce::ColourSelector::showColourspace);
        selector->setSize (380, 360);
        selector->setCurrentColour (current);

        struct Listener : juce::ChangeListener
        {
            BrandingLabPage* page;
            juce::String field;
            juce::ColourSelector* sel;
            void changeListenerCallback (juce::ChangeBroadcaster*) override
            {
                auto& m = page->owner.getProject().getManifest();
                const auto c = sel->getCurrentColour();
                if (field == "accent") m.playerAccentColour = c;
                else if (field == "panel") m.playerPanelColour = c;
                else if (field == "bg") m.playerBackgroundColour = c;
                else if (field == "text") m.playerTextColour = c;
                page->owner.getProject().notifyChanged();
                page->repaint();
            }
        };
        auto* l = new Listener();
        l->page = this; l->field = field; l->sel = selector;
        selector->addChangeListener (l);

        juce::CallOutBox::launchAsynchronously (
            std::unique_ptr<juce::Component> (selector),
            [&]
            {
                auto* swatch = field == "accent" ? &accentSwatch
                            : field == "panel"  ? &panelSwatch
                            : field == "bg"     ? &bgSwatch : &textSwatch;
                return swatch->getScreenBounds();
            }(),
            nullptr);
    }

    void BrandingLabPage::chooseLogo()
    {
        logoChooser = std::make_unique<juce::FileChooser> (
            "Pick logo image", juce::File(), "*.png;*.jpg;*.jpeg;*.svg");
        logoChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File()) return;
                logoPathEdit.setText (f.getFullPathName(), juce::sendNotificationSync);
                writeToManifest();
            });
    }

    void BrandingLabPage::resized()
    {
        auto r = getLocalBounds().reduced (16);

        // Header row: title + form-toggle in the top-right.
        auto headerRow = r.removeFromTop (28);
        showFormToggle.setBounds (headerRow.removeFromRight (180));
        header.setBounds (headerRow);

        subtitle.setBounds (r.removeFromTop (38));
        r.removeFromTop (8);

        // Two-column layout: form on left (collapsible), Player-style frame
        // around the live test page on the right.
        const bool showForm = showFormToggle.getToggleState();
        const int formW = showForm ? juce::jmin (420, r.getWidth() * 4 / 10) : 0;
        auto form = r.removeFromLeft (formW);
        if (showForm) r.removeFromLeft (16);
        previewArea = r;

        // Reserve top strip of preview area for the branded Player header.
        const int headerH = 56;
        playerHeaderArea = previewArea.removeFromTop (headerH);
        previewArea.removeFromTop (4);

        // The TestPage fills what's left so the user can play their instrument.
        if (testPage) testPage->setBounds (previewArea);

        if (! showForm) return;

        auto row = [&form] (int height)
        {
            auto out = form.removeFromTop (height);
            form.removeFromTop (6);
            return out;
        };

        auto pairRow = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::Component& edit)
        {
            lbl.setBounds (r.removeFromLeft (130));
            edit.setBounds (r);
        };

        pairRow (row (28), displayNameLabel, displayNameEdit);
        pairRow (row (28), taglineLabel,     taglineEdit);
        pairRow (row (28), creatorLabel,     creatorEdit);
        pairRow (row (28), versionLabel,     versionEdit);
        pairRow (row (28), websiteLabel,     websiteEdit);

        // Logo row uses three controls (label, edit, browse, clear).
        auto logoRow = row (28);
        logoLabel.setBounds (logoRow.removeFromLeft (130));
        clearLogoBtn.setBounds (logoRow.removeFromRight (60));
        logoRow.removeFromRight (4);
        browseLogoBtn.setBounds (logoRow.removeFromRight (74));
        logoRow.removeFromRight (6);
        logoPathEdit.setBounds (logoRow);

        form.removeFromTop (8);

        // Colour swatches in a grid.
        auto colourRow = [&form] (int rows)
        {
            auto out = form.removeFromTop (32 * rows + 6 * (rows - 1));
            form.removeFromTop (10);
            return out;
        };

        auto swatchPair = [] (juce::Rectangle<int> r, juce::Label& lbl, juce::TextButton& swatch)
        {
            lbl.setBounds (r.removeFromLeft (90));
            swatch.setBounds (r.removeFromLeft (60).reduced (0, 4));
        };

        auto cRow = colourRow (1);
        swatchPair (cRow.removeFromLeft (160), accentLabel, accentSwatch);
        cRow.removeFromLeft (16);
        swatchPair (cRow.removeFromLeft (160), panelLabel, panelSwatch);

        auto cRow2 = colourRow (1);
        swatchPair (cRow2.removeFromLeft (160), bgLabel, bgSwatch);
        cRow2.removeFromLeft (16);
        swatchPair (cRow2.removeFromLeft (160), textLabel, textSwatch);

        resetColoursBtn.setBounds (form.removeFromTop (28));
    }

    void BrandingLabPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        // Recolour the swatch buttons to reflect manifest state.
        const auto& m = owner.getProject().getManifest();
        struct SwatchPaint { juce::TextButton* btn; juce::Colour c; };
        const SwatchPaint paints[] = {
            { &accentSwatch, m.playerAccentColour },
            { &panelSwatch,  m.playerPanelColour },
            { &bgSwatch,     m.playerBackgroundColour },
            { &textSwatch,   m.playerTextColour }
        };
        for (const auto& s : paints)
        {
            s.btn->setColour (juce::TextButton::buttonColourId,   s.c);
            s.btn->setColour (juce::TextButton::buttonOnColourId, s.c.brighter (0.10f));
            s.btn->setButtonText (colourToHex (s.c));
            s.btn->setColour (juce::TextButton::textColourOnId,
                              s.c.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
            s.btn->setColour (juce::TextButton::textColourOffId,
                              s.c.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white);
        }

        // Live Player chrome around the embedded TestPage (the actual
        // playable instrument). The header strip carries the branded
        // logo + display name + tagline; the colour stripes mirror the
        // accent / background palette so the developer sees their skin.
        if (! playerHeaderArea.isEmpty())
            paintPlayerPreview (g, playerHeaderArea);
    }

    void BrandingLabPage::paintPlayerPreview (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto& m = owner.getProject().getManifest();

        // Branded Player header strip — what the end user sees in their DAW
        // above the instrument layout. The TestPage rendered below is the
        // actual playable instrument; this strip is the chrome around it.
        g.setColour (m.playerBackgroundColour);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (m.playerAccentColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.4f);

        auto inner = r.reduced (12);

        // Logo plate on the left.
        const auto logoBox = inner.removeFromLeft (juce::jmin (44, inner.getHeight()));
        inner.removeFromLeft (10);
        const auto logoFile = juce::File::isAbsolutePath (m.playerLogoImage)
            ? juce::File (m.playerLogoImage)
            : owner.getProject().getProjectFolder().getChildFile (m.playerLogoImage);
        juce::Image logo;
        if (m.playerLogoImage.isNotEmpty() && logoFile.existsAsFile())
            logo = juce::ImageFileFormat::loadFrom (logoFile);
        if (logo.isValid())
        {
            g.drawImage (logo, logoBox.toFloat(),
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour (m.playerAccentColour.withAlpha (0.20f));
            g.fillRoundedRectangle (logoBox.toFloat(), 4.0f);
            g.setColour (m.playerAccentColour);
            g.setFont (juce::Font ((float) logoBox.getHeight() * 0.55f, juce::Font::bold));
            const auto initial = m.playerDisplayName.isNotEmpty()
                ? m.playerDisplayName.substring (0, 1)
                : (m.instrumentName.isNotEmpty() ? m.instrumentName.substring (0, 1) : juce::String ("P"));
            g.drawText (initial.toUpperCase(), logoBox, juce::Justification::centred);
        }

        // Title + tagline stacked vertically.
        const auto title = m.playerDisplayName.isNotEmpty() ? m.playerDisplayName : m.instrumentName;
        auto titleArea = inner.removeFromTop (inner.getHeight() / 2);
        g.setColour (m.playerTextColour);
        g.setFont (juce::Font (juce::jmax (16.0f, (float) titleArea.getHeight() * 0.85f), juce::Font::bold));
        g.drawText (title, titleArea, juce::Justification::centredLeft);

        if (m.playerTagline.isNotEmpty())
        {
            g.setColour (m.playerTextDimColour);
            g.setFont (juce::Font (juce::jmax (10.0f, (float) inner.getHeight() * 0.7f)));
            g.drawText (m.playerTagline, inner, juce::Justification::centredLeft);
        }

        // Right-side meta: creator + version.
        auto rightBlock = juce::Rectangle<int> (r.getRight() - 220, r.getY() + 6, 200, r.getHeight() - 12);
        g.setColour (m.playerTextDimColour);
        g.setFont (juce::Font (10.0f));
        const auto creator = m.creator.isNotEmpty() ? m.creator : juce::String ("PatchCraft");
        g.drawText (creator + (m.version.isNotEmpty() ? "  v" + m.version : juce::String()),
                    rightBlock, juce::Justification::centredRight);
    }
}
