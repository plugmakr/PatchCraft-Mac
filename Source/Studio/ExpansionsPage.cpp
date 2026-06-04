#include "ExpansionsPage.h"
#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"
#include "VstExportModule.h"

namespace patchcraft
{
    namespace
    {
        static juce::Colour bgTop()      { return juce::Colour (0xff070a0f); }
        static juce::Colour bgBottom()   { return juce::Colour (0xff101722); }
        static juce::Colour cardBg()     { return juce::Colour (0xff111923); }
        static juce::Colour cardBorder() { return juce::Colour (0xff28364a); }

        static void styleActionButton (juce::TextButton& button, bool primary)
        {
            button.getProperties().set ("fontSize", 12.0);
            button.getProperties().set ("bold", true);
            button.getProperties().set ("corner", 6.0);
            button.setColour (juce::TextButton::buttonColourId,
                              primary ? juce::Colour (0xff25d3c3) : juce::Colour (0xff151d29));
            button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff25d3c3).brighter (0.18f));
            button.setColour (juce::TextButton::textColourOffId,
                              primary ? juce::Colour (0xff061012) : PatchCraftLookAndFeel::text());
            button.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff061012));
        }
    }

    ExpansionsPage::ExpansionsPage (StudioMainComponent& o) : owner (o)
    {
        rebuildAddons();

        styleActionButton (backButton, false);
        styleActionButton (vstDetailsButton, false);
        styleActionButton (vstCheckoutButton, true);
        styleActionButton (pluginClubButton, true);
        styleActionButton (buildPackageButton, false);

        backButton.onClick = [this] { setView (View::Catalog); };
        vstDetailsButton.onClick = [this] { setView (View::VstExpansion); };
        vstCheckoutButton.onClick = [this] { setView (View::Checkout); };
        pluginClubButton.onClick = [this] { openPluginClub(); };
        buildPackageButton.onClick = [this]
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "VST Expansion Package",
                                                    "The addon package is built from the PatchCraftVstExpansionPackage target.\n\n"
                                                    "For the June 1 beta, checkout is mocked here and the installer is produced by the local build pipeline.");
        };

        addAndMakeVisible (backButton);
        addAndMakeVisible (vstDetailsButton);
        addAndMakeVisible (vstCheckoutButton);
        addAndMakeVisible (pluginClubButton);
        addAndMakeVisible (buildPackageButton);

        setView (View::Catalog);
    }

    void ExpansionsPage::refresh()
    {
        rebuildAddons();
        repaint();
    }

    void ExpansionsPage::setView (View view)
    {
        currentView = view;
        backButton.setVisible (view != View::Catalog);
        vstDetailsButton.setVisible (view == View::Catalog);
        vstCheckoutButton.setVisible (view == View::Catalog || view == View::VstExpansion);
        pluginClubButton.setVisible (view == View::Checkout);
        buildPackageButton.setVisible (view == View::VstExpansion);
        resized();
        repaint();
    }

    void ExpansionsPage::rebuildAddons()
    {
        const bool installed = VstExportModule::isVstExpansionInstalled();
        addons = {
            { "vst-expansion",
              "PatchCraft VST Expansion",
              "Commercial plugin export",
              "Build white-label standalone VST3 instruments and FX plugins from PatchCraft projects.",
              "$79 beta",
              installed ? "Installed" : "Available",
              juce::Colour (0xff25d3c3),
              true },
            { "ai-expansion",
              "AI Studio Expansion",
              "Prompt-assisted creation",
              "Generate sound recipes, artwork direction, product copy, and guided builder plans.",
              "Coming soon",
              "Waitlist",
              juce::Colour (0xff9b6dff),
              false },
            { "premium-factory",
              "Premium Factory Library",
              "Sounds, skins, and templates",
              "Curated synth, sample, drum, FX, branding, and animation packs for faster releases.",
              "Coming soon",
              "Planned",
              juce::Colour (0xffffb443),
              false }
        };
    }

    void ExpansionsPage::paint (juce::Graphics& g)
    {
        juce::ColourGradient background (bgTop(), 0.0f, 0.0f, bgBottom(), 0.0f, (float) getHeight(), false);
        g.setGradientFill (background);
        g.fillAll();

        paintHero (g);

        if (currentView == View::Catalog)
            paintCatalog (g);
        else if (currentView == View::VstExpansion)
            paintVstExpansion (g);
        else
            paintCheckout (g);
    }

    void ExpansionsPage::paintHero (juce::Graphics& g)
    {
        auto r = heroBounds.toFloat();
        g.setColour (juce::Colour (0xff0c131d).withAlpha (0.96f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (juce::Colour (0xff25d3c3).withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.2f);
        g.setColour (juce::Colour (0xff25d3c3));
        g.fillRoundedRectangle (r.withHeight (4.0f), 2.0f);

        auto text = heroBounds.reduced (24, 18);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.drawText ("PATCHCRAFT EXPANSIONS", text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (30.0f).withStyle ("bold"));
        g.drawText ("Add capabilities when a creator is ready to sell", text.removeFromTop (42), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("Expansion add-ons stay separate from the base Studio installer. Plugin.club handles purchase, license ownership, installer delivery, and future import/update flows.",
                          text.removeFromTop (46), juce::Justification::centredLeft, 2);

        auto pills = text.removeFromTop (30);
        paintPill (g, pills.removeFromLeft (170), "Plugin.club checkout", juce::Colour (0xff25d3c3));
        pills.removeFromLeft (8);
        paintPill (g, pills.removeFromLeft (142), "License-gated", juce::Colour (0xff9b6dff));
        pills.removeFromLeft (8);
        paintPill (g, pills.removeFromLeft (168), "Separate installers", juce::Colour (0xffffb443));
    }

    void ExpansionsPage::paintCatalog (juce::Graphics& g)
    {
        auto titleArea = catalogBounds;
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        g.drawText ("Available Add-ons", titleArea.removeFromTop (26), juce::Justification::centredLeft, true);

        for (size_t i = 0; i < addons.size() && i < addonCardBounds.size(); ++i)
            paintAddonCard (g, addons[i], addonCardBounds[i], i == 0);
    }

    void ExpansionsPage::paintAddonCard (juce::Graphics& g, const Addon& addon, juce::Rectangle<int> bounds, bool featured)
    {
        auto r = bounds.toFloat();
        g.setColour (featured ? cardBg().brighter (0.04f) : cardBg());
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (addon.accent.withAlpha (featured ? 0.78f : 0.42f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, featured ? 1.6f : 1.0f);
        g.fillRoundedRectangle (r.withHeight (4.0f), 2.0f);

        auto content = bounds.reduced (18, 14);
        g.setColour (addon.accent);
        g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
        g.drawText (addon.eyebrow.toUpperCase(), content.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        g.drawText (addon.name, content.removeFromTop (28), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (addon.shortDescription, content.removeFromTop (54), juce::Justification::topLeft, 3);

        auto bottom = bounds.reduced (18, 14).removeFromBottom (30);
        paintPill (g, bottom.removeFromLeft (96), addon.price, addon.accent);
        bottom.removeFromLeft (8);
        paintPill (g, bottom.removeFromLeft (110), addon.status, addon.available ? juce::Colour (0xff7bd88f) : juce::Colour (0xff98a6ba));
    }

    void ExpansionsPage::paintVstExpansion (juce::Graphics& g)
    {
        auto r = detailBounds;
        auto left = r.removeFromLeft (juce::jmax (360, r.getWidth() * 55 / 100)).reduced (0, 0);
        r.removeFromLeft (18);
        auto right = r;

        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (28.0f).withStyle ("bold"));
        g.drawText ("PatchCraft VST Expansion", left.removeFromTop (42), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("Turn a PatchCraft instrument, FX design, or sample product into a branded VST3 package. The base Studio stays simple; the paid expansion unlocks commercial plugin export templates, installer metadata, and Plugin.club delivery.",
                          left.removeFromTop (78), juce::Justification::topLeft, 4);

        left.removeFromTop (12);
        paintFeatureRow (g, left.removeFromTop (52), "Standalone VST3 Export", "Export instrument and FX plugins with embedded packs and white-label metadata.", juce::Colour (0xff25d3c3));
        paintFeatureRow (g, left.removeFromTop (52), "Installer Delivery", "Ships as a separate addon installer so the base app is not overloaded.", juce::Colour (0xffffb443));
        paintFeatureRow (g, left.removeFromTop (52), "Plugin.club Licensing", "Purchase, entitlement, installer access, and future updates route through Plugin.club.", juce::Colour (0xff9b6dff));
        paintFeatureRow (g, left.removeFromTop (52), "Release QA Hooks", "Export smoke tests validate branded bundle metadata, class IDs, embedded packs, and license behavior.", juce::Colour (0xff72d4ff));

        auto panel = right.toFloat();
        g.setColour (cardBg());
        g.fillRoundedRectangle (panel, 12.0f);
        g.setColour (juce::Colour (0xff25d3c3).withAlpha (0.7f));
        g.drawRoundedRectangle (panel.reduced (0.5f), 12.0f, 1.2f);

        auto card = right.reduced (22, 18);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText ("BETA ADDON", card.removeFromTop (18), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (34.0f).withStyle ("bold"));
        g.drawText ("$79", card.removeFromTop (44), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Beta tester launch price", card.removeFromTop (22), juce::Justification::centredLeft, true);
        card.removeFromTop (16);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (12.8f));
        g.drawFittedText ("Includes the VST3 export templates, installer package, Plugin.club entitlement hook, and commercial delivery workflow preview.",
                          card.removeFromTop (84), juce::Justification::topLeft, 4);
        card.removeFromTop (8);
        paintPill (g, card.removeFromTop (30).removeFromLeft (170),
                   VstExportModule::isVstExpansionInstalled() ? "Installed locally" : "Not installed locally",
                   VstExportModule::isVstExpansionInstalled() ? juce::Colour (0xff7bd88f) : juce::Colour (0xffffb443));
    }

    void ExpansionsPage::paintCheckout (juce::Graphics& g)
    {
        auto r = checkoutBounds;
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (28.0f).withStyle ("bold"));
        g.drawText ("Plugin.club Checkout Preview", r.removeFromTop (42), juce::Justification::centredLeft, true);

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (13.5f));
        g.drawFittedText ("This is the intended purchase flow. Studio shows the preview; the real transaction, license, and installer access are completed on Plugin.club.",
                          r.removeFromTop (48), juce::Justification::topLeft, 2);

        r.removeFromTop (18);
        const int stepH = 66;
        paintCheckoutStep (g, r.removeFromTop (stepH), 1, "Sign in to Plugin.club", "Buyer signs in or creates an account so the addon can attach to their library.", true);
        r.removeFromTop (10);
        paintCheckoutStep (g, r.removeFromTop (stepH), 2, "Review addon and license", "PatchCraft VST Expansion, beta price, license terms, and supported platforms.", true);
        r.removeFromTop (10);
        paintCheckoutStep (g, r.removeFromTop (stepH), 3, "Secure payment", "Plugin.club handles payment, receipt, taxes, and fraud checks.", false);
        r.removeFromTop (10);
        paintCheckoutStep (g, r.removeFromTop (stepH), 4, "Download installer", "Buyer downloads PatchCraftVstExpansion-Setup and install notes from their Plugin.club library.", false);
        r.removeFromTop (10);
        paintCheckoutStep (g, r.removeFromTop (stepH), 5, "Activate inside PatchCraft", "Studio detects the installed addon and unlocks standalone VST3 export.", false);
    }

    void ExpansionsPage::paintPill (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text, juce::Colour accent) const
    {
        g.setColour (accent.withAlpha (0.14f));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (accent.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
        g.drawText (text, bounds.reduced (10, 0), juce::Justification::centred, true);
    }

    void ExpansionsPage::paintFeatureRow (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, const juce::String& body, juce::Colour accent) const
    {
        g.setColour (cardBg().withAlpha (0.80f));
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (accent.withAlpha (0.68f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.fillRoundedRectangle (bounds.removeFromLeft (4).toFloat(), 2.0f);

        auto text = bounds.reduced (14, 7);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        g.drawText (title, text.removeFromTop (18), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (body, text, juce::Justification::centredLeft, true);
    }

    void ExpansionsPage::paintCheckoutStep (juce::Graphics& g, juce::Rectangle<int> bounds, int number, const juce::String& title, const juce::String& body, bool active) const
    {
        const auto accent = active ? juce::Colour (0xff25d3c3) : juce::Colour (0xff6f7d91);
        g.setColour (cardBg().withAlpha (active ? 0.95f : 0.72f));
        g.fillRoundedRectangle (bounds.toFloat(), 10.0f);
        g.setColour (accent.withAlpha (0.62f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 10.0f, 1.0f);

        auto bubble = bounds.removeFromLeft (54).reduced (12, 14);
        g.setColour (accent.withAlpha (0.18f));
        g.fillEllipse (bubble.toFloat());
        g.setColour (accent);
        g.drawEllipse (bubble.toFloat(), 1.3f);
        g.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
        g.drawText (juce::String (number), bubble, juce::Justification::centred, true);

        auto text = bounds.reduced (10, 10);
        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::FontOptions (13.5f).withStyle ("bold"));
        g.drawText (title, text.removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (body, text, juce::Justification::centredLeft, true);
    }

    void ExpansionsPage::openPluginClub()
    {
        juce::URL ("https://plugin.club").launchInDefaultBrowser();
    }

    void ExpansionsPage::resized()
    {
        auto r = getLocalBounds().reduced (24);
        heroBounds = r.removeFromTop (158);
        r.removeFromTop (18);

        auto actions = heroBounds.reduced (24, 18).removeFromRight (430).removeFromBottom (38);
        backButton.setBounds (actions.removeFromLeft (82));
        actions.removeFromLeft (8);
        vstDetailsButton.setBounds (actions.removeFromLeft (150));
        actions.removeFromLeft (8);
        vstCheckoutButton.setBounds (actions.removeFromLeft (146));
        pluginClubButton.setBounds (actions.withWidth (178));
        buildPackageButton.setBounds (heroBounds.reduced (24, 18).removeFromRight (192).removeFromTop (34));

        catalogBounds = r;
        detailBounds = r.reduced (4, 0);
        checkoutBounds = r.reduced (4, 0);

        addonCardBounds.clear();
        auto cardArea = catalogBounds.withTrimmedTop (36);
        const int gap = 16;
        const int columns = getWidth() > 1280 ? 3 : 1;
        const int cardW = columns == 3 ? (cardArea.getWidth() - gap * 2) / 3 : cardArea.getWidth();
        const int cardH = columns == 3 ? 220 : 150;
        for (int i = 0; i < (int) addons.size(); ++i)
        {
            const int col = columns == 3 ? i % columns : 0;
            const int row = columns == 3 ? i / columns : i;
            addonCardBounds.push_back ({ cardArea.getX() + col * (cardW + gap),
                                         cardArea.getY() + row * (cardH + gap),
                                         cardW, cardH });
        }
    }
}
