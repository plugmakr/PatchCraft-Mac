#include "PlayerPreviewChrome.h"

#include "../Player/PluginProcessor.h"
#include "../Shared/PatchCraftProject.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        static juce::String displayName (const Manifest& m)
        {
            return m.playerDisplayName.isNotEmpty() ? m.playerDisplayName : m.instrumentName;
        }

        static constexpr int kPlayerTitleBarArtworkSize = 46;

        static juce::Font playerChromeFont (const juce::String& family, float size, bool bold)
        {
            juce::Font font (size, bold ? juce::Font::bold : juce::Font::plain);
            if (family.isNotEmpty() && family != "Default")
                font.setTypefaceName (family);
            return font;
        }

        static int playerTitleBrandWidth (const juce::String& theme, int barWidth)
        {
            const bool compact = barWidth < 980;
            if (theme == "no-chrome")
                return compact ? 0 : 12;
            if (theme == "custom")
                return compact ? 240 : 390;
            if (theme == "wide-banner")
                return compact ? 240 : 390;
            if (theme == "artist-card")
                return compact ? 226 : 340;
            if (theme == "banner")
                return compact ? 206 : 300;
            if (theme == "split-brand")
                return compact ? 192 : 280;
            if (theme == "logo-rail")
                return compact ? 90 : 126;
            if (theme == "minimal")
                return compact ? 118 : 170;
            if (theme == "compact-daw")
                return 146;
            return compact ? 146 : 218;
        }

        static bool titleThemeUsesBannerArtwork (const juce::String& theme)
        {
            return theme == "banner"
                || theme == "custom"
                || theme == "wide-banner"
                || theme == "artist-card";
        }
    }

    PlayerPreviewChrome::PlayerPreviewChrome (PatchCraftProject& proj, PlayerProcessor& proc)
        : project (proj), processor (proc)
    {
        for (auto* b : { &libraryBtn, &soundBtn, &prevPresetBtn, &presetBtn, &nextPresetBtn, &playBtn })
        {
            b->getProperties().set ("smallButton", true);
            b->getProperties().set ("fontSize", 11.0);
            addAndMakeVisible (*b);
        }

        libraryBtn.setTooltip ("Pack library (hidden in shipped products when disabled in Brand).");
        soundBtn.setTooltip ("Sound DNA / performance panel (Player runtime).");
        playBtn.setTooltip ("Internal transport for drum grid and sequencer patterns.");

        libraryBtn.onClick = [] {};
        soundBtn.onClick = [] {};

        prevPresetBtn.onClick = [this]
        {
            processor.applyPresetOffset (-1);
            refreshPresetLabel();
        };
        nextPresetBtn.onClick = [this]
        {
            processor.applyPresetOffset (1);
            refreshPresetLabel();
        };
        presetBtn.onClick = [this] { showPresetMenu(); };
        playBtn.onClick = [this]
        {
            processor.toggleInternalTransport();
            playBtn.setButtonText (processor.isInternalTransportPlaying() ? "Stop" : "Play");
        };

        studioBackBtn.getProperties().set ("primaryAction", true);
        studioBackBtn.getProperties().set ("fontSize", 11.0);
        studioBackBtn.getProperties().set ("bold", true);
        studioBackBtn.setVisible (false);
        studioBackBtn.setTooltip ("Return to Studio authoring.");
        studioBackBtn.onClick = [this]
        {
            if (onStudioExit)
                onStudioExit();
        };
        addChildComponent (studioBackBtn);

        refresh();
        startTimerHz (8);
    }

    PlayerPreviewChrome::~PlayerPreviewChrome()
    {
        stopTimer();
    }

    void PlayerPreviewChrome::refresh()
    {
        const auto& m = project.getManifest();
        libraryBtn.setVisible (m.playerShowLibraryBrowser);
        soundBtn.setVisible (processor.getPack() != nullptr);

        const bool hasPresets = processor.getPresetCount() > 0;
        prevPresetBtn.setVisible (hasPresets);
        presetBtn.setVisible (hasPresets);
        nextPresetBtn.setVisible (hasPresets);
        playBtn.setVisible (processor.getPack() != nullptr);

        const auto titleButtonStyle = m.playerTitleButtonStyle.isNotEmpty()
            ? m.playerTitleButtonStyle
            : juce::String ("outlined");

        for (auto* button : { &libraryBtn, &soundBtn, &prevPresetBtn, &presetBtn, &nextPresetBtn, &playBtn })
        {
            button->getProperties().set ("corner", titleButtonStyle == "square" ? 2.0 : (titleButtonStyle == "pill" ? 12.0 : 5.0));
            button->setColour (juce::TextButton::buttonColourId,
                               titleButtonStyle == "minimal" ? juce::Colours::transparentBlack
                               : titleButtonStyle == "filled" ? m.playerAccentColour.withAlpha (0.28f)
                                                              : m.playerPanelColour.brighter (0.04f));
            button->setColour (juce::TextButton::textColourOffId, m.playerTextColour);
        }

        refreshPresetLabel();
        playBtn.setButtonText (processor.isInternalTransportPlaying() ? "Stop" : "Play");
        resized();
        repaint();
    }

    void PlayerPreviewChrome::setStudioExitHandler (std::function<void()> handler)
    {
        onStudioExit = std::move (handler);
        studioBackBtn.setVisible (onStudioExit != nullptr);
        resized();
        repaint();
    }

    void PlayerPreviewChrome::refreshPresetLabel()
    {
        const int idx = processor.getCurrentPresetIndex();
        presetBtn.setButtonText (idx >= 0 ? processor.getPresetName (idx) : "Preset");
    }

    void PlayerPreviewChrome::showPresetMenu()
    {
        juce::PopupMenu menu;
        const int count = processor.getPresetCount();
        for (int i = 0; i < count; ++i)
            menu.addItem (i + 1, processor.getPresetName (i), true, i == processor.getCurrentPresetIndex());

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
            [this] (int result)
            {
                if (result > 0)
                {
                    processor.applyPresetByIndex (result - 1);
                    refreshPresetLabel();
                }
            });
    }

    void PlayerPreviewChrome::timerCallback()
    {
        const bool playing = processor.isInternalTransportPlaying();
        const auto label = playing ? juce::String ("Stop") : juce::String ("Play");
        if (playBtn.getButtonText() != label)
            playBtn.setButtonText (label);
    }

    void PlayerPreviewChrome::paint (juce::Graphics& g)
    {
        const auto& m = project.getManifest();
        auto bounds = getLocalBounds();
        auto titleArea = bounds.removeFromTop (kTitleHeight);
        auto toolbarArea = bounds;

        const auto titleTheme = m.playerTitleBarTheme.isNotEmpty() ? m.playerTitleBarTheme : juce::String ("classic");
        const auto titlePlacement = m.playerTitleTextPlacement.isNotEmpty() ? m.playerTitleTextPlacement : juce::String ("left");
        const auto titleFontFamily = m.playerTitleFontFamily.isNotEmpty() ? m.playerTitleFontFamily : juce::String ("Default");
        const auto accent = m.playerAccentColour;
        const auto panel = m.playerPanelColour;
        const auto bg = m.playerBackgroundColour;

        auto themeTitleTop = titleTheme == "clean-pro" ? panel.brighter (0.22f)
                           : titleTheme == "dark-utility" ? panel.darker (0.25f)
                           : titleTheme == "glass" ? panel.withAlpha (0.65f)
                           : panel.brighter (0.06f);
        auto themeTitleBottom = titleTheme == "aurora" ? accent.interpolatedWith (bg, 0.62f)
                              : titleTheme == "minimal" ? bg.darker (0.15f)
                              : titleTheme == "compact-daw" ? panel.darker (0.12f)
                              : bg.darker (0.08f);
        juce::ColourGradient gradient (themeTitleTop,
                                       (float) titleArea.getX(), (float) titleArea.getY(),
                                       themeTitleBottom,
                                       (float) titleArea.getRight(), (float) titleArea.getBottom(),
                                       false);
        g.setGradientFill (gradient);
        g.fillRect (titleArea);

        g.setColour (panel.withAlpha (0.94f));
        g.fillRect (toolbarArea);

        const bool useFullTitleBackground = m.playerTitleBannerImage.isNotEmpty()
                                         && titleThemeUsesBannerArtwork (titleTheme);
        if (useFullTitleBackground)
        {
            const auto bannerFile = juce::File::isAbsolutePath (m.playerTitleBannerImage)
                ? juce::File (m.playerTitleBannerImage)
                : project.getProjectFolder().getChildFile (m.playerTitleBannerImage);
            if (auto banner = assets.loadImage (bannerFile); banner.isValid())
            {
                g.drawImage (banner, titleArea.toFloat(), juce::RectanglePlacement::fillDestination);
                g.setColour (juce::Colour (0x8805070a));
                g.fillRect (titleArea);
            }
        }
        if ((titleTheme == "minimal" || titleTheme == "no-chrome") && ! useFullTitleBackground)
        {
            g.setColour (bg);
            g.fillRect (titleArea);
        }
        if (titleTheme == "glass" && ! useFullTitleBackground)
        {
            g.setColour (accent.withAlpha (0.08f));
            g.fillRect (titleArea.reduced (12, 8));
        }
        if (titleTheme == "dark-utility")
        {
            g.setColour (panel.darker (0.42f));
            g.fillRect (titleArea);
            g.setColour (accent.withAlpha (0.38f));
            g.fillRect (titleArea.withBottom (titleArea.getBottom()).withHeight (2));
        }
        if (titleTheme == "compact-daw")
        {
            g.setColour (panel.brighter (0.05f));
            g.fillRect (titleArea.reduced (0, 8));
        }
        if (titleTheme == "neon-strip" || titleTheme == "aurora")
        {
            g.setColour (accent.withAlpha (0.82f));
            g.fillRoundedRectangle (titleArea.withHeight (3).toFloat(), 2.0f);
        }
        if (titleTheme == "split-brand")
        {
            g.setColour (accent.withAlpha (0.88f));
            g.fillRect (titleArea.withWidth (5));
        }
        if (titleTheme == "logo-rail")
        {
            g.setColour (accent.withAlpha (0.14f));
            g.fillRect (titleArea.withWidth (96));
            g.setColour (accent.withAlpha (0.72f));
            g.drawVerticalLine (96, 4.0f, (float) titleArea.getBottom() - 5.0f);
        }

        if (titleTheme != "no-chrome")
        {
            g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.82f));
            g.fillRect (toolbarArea.withHeight (1));
            g.setColour (bg.withAlpha (0.42f));
            g.fillRect (toolbarArea.withTrimmedTop (1));
            auto toolWell = toolbarArea.reduced (10, 5);
            const auto corner = titleTheme == "compact-daw" || titleTheme == "dark-utility" ? 2.0f
                              : 6.0f;
            g.setColour ((titleTheme == "glass" ? panel.withAlpha (0.58f)
                                                : panel.withAlpha (0.96f)));
            if (titleTheme == "minimal")
                g.fillRect (toolWell.withHeight (1));
            else
                g.fillRoundedRectangle (toolWell.toFloat(), corner);
            g.setColour ((titleTheme == "neon-strip" ? accent : PatchCraftLookAndFeel::borderSoft()).withAlpha (0.72f));
            if (titleTheme != "minimal")
                g.drawRoundedRectangle (toolWell.toFloat().reduced (0.5f), corner, 1.0f);
        }

        const int bannerWidth = playerTitleBrandWidth (titleTheme, titleArea.getWidth());
        const auto brandFrame = juce::Rectangle<int> (titleArea.getX() + 10,
                                                      titleArea.getY() + 10,
                                                      bannerWidth,
                                                      52);
        const auto artworkBounds = brandFrame.withWidth (kPlayerTitleBarArtworkSize)
                                             .withHeight (kPlayerTitleBarArtworkSize)
                                             .withY (titleArea.getY() + (titleArea.getHeight() - kPlayerTitleBarArtworkSize) / 2);
        auto brand = (titleTheme == "logo-rail" || titleTheme == "no-chrome")
            ? brandFrame.reduced (0, 1)
            : brandFrame.withTrimmedLeft (kPlayerTitleBarArtworkSize + 8).reduced (0, 1);
        bool drewLogo = false;
        bool drewTitleBanner = false;

        if (bannerWidth > 0)
        {
            if (m.playerTitleBannerImage.isNotEmpty()
                && ! useFullTitleBackground)
            {
                const auto bannerFile = juce::File::isAbsolutePath (m.playerTitleBannerImage)
                    ? juce::File (m.playerTitleBannerImage)
                    : project.getProjectFolder().getChildFile (m.playerTitleBannerImage);
                if (auto banner = assets.loadImage (bannerFile); banner.isValid())
                {
                    g.saveState();
                    juce::Path clip;
                    clip.addRoundedRectangle (brandFrame.toFloat(), 7.0f);
                    g.reduceClipRegion (clip);
                    g.drawImage (banner, brandFrame.toFloat(), juce::RectanglePlacement::fillDestination);
                    g.setColour (juce::Colour (0xaa05070a));
                    g.fillRoundedRectangle (brandFrame.toFloat(), 7.0f);
                    g.restoreState();
                    g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.86f));
                    g.drawRoundedRectangle (brandFrame.toFloat().reduced (0.5f), 7.0f, 1.0f);
                    drewTitleBanner = true;
                    brand = brandFrame.reduced (11, 3);
                }
            }

            auto artworkPath = m.playerLogoImage;
            if (artworkPath.isEmpty())
                artworkPath = m.libraryThumbnail;

            if (! drewTitleBanner && artworkPath.isNotEmpty() && titleTheme != "no-chrome")
            {
                const auto logoFile = juce::File::isAbsolutePath (artworkPath)
                    ? juce::File (artworkPath)
                    : project.getProjectFolder().getChildFile (artworkPath);
                if (auto logo = assets.loadImage (logoFile); logo.isValid())
                {
                    g.saveState();
                    juce::Path clip;
                    clip.addRoundedRectangle (artworkBounds.toFloat(), 5.0f);
                    g.reduceClipRegion (clip);
                    g.drawImage (logo, artworkBounds.toFloat(), juce::RectanglePlacement::fillDestination);
                    g.restoreState();
                    g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.75f));
                    g.drawRoundedRectangle (artworkBounds.toFloat(), 5.0f, 1.0f);
                    drewLogo = true;
                }
            }
        }
        if (! drewTitleBanner && ! drewLogo && bannerWidth > 0 && titleTheme != "no-chrome")
        {
            const auto fallbackName = displayName (m);
            const auto initial = fallbackName.isNotEmpty() ? fallbackName.substring (0, 1).toUpperCase() : juce::String ("P");
            g.setColour (accent.withAlpha (0.22f));
            g.fillRoundedRectangle (artworkBounds.toFloat().reduced (1.0f), 5.0f);
            g.setColour (accent);
            g.setFont (playerChromeFont (titleFontFamily, (float) artworkBounds.getHeight() * 0.52f, true));
            g.drawText (initial, artworkBounds, juce::Justification::centred, true);
        }

        const auto brandName = displayName (m);
        const auto tagline = m.playerTagline;
        if (titlePlacement != "hidden")
        {
            const auto justify = titlePlacement == "center" ? juce::Justification::centred
                               : titlePlacement == "right"  ? juce::Justification::centredRight
                                                            : juce::Justification::centredLeft;
            if (titlePlacement == "center")
                brand = juce::Rectangle<int> (titleArea.getX() + titleArea.getWidth() / 2 - 190,
                                              brand.getY(), 380, brand.getHeight());
            else if (titlePlacement == "right")
                brand = juce::Rectangle<int> (titleArea.getRight() - 390, brand.getY(), 240, brand.getHeight());

            g.setColour (m.playerTextColour);
            g.setFont (playerChromeFont (titleFontFamily, titleTheme == "artist-card" ? 16.5f : 15.0f, true));
            g.drawText (brandName, brand.removeFromTop (drewTitleBanner ? 17 : 19), justify, true);
            if (tagline.isNotEmpty())
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (playerChromeFont (titleFontFamily, 10.0f, true));
                g.drawText (tagline.toUpperCase(), brand, justify, true);
            }
        }
    }

    void PlayerPreviewChrome::resized()
    {
        auto toolbar = getLocalBounds().removeFromBottom (getHeight() - kTitleHeight).reduced (10, 5);
        const int buttonH = juce::jlimit (24, 30, toolbar.getHeight() - 2);
        const int y = toolbar.getY() + (toolbar.getHeight() - buttonH) / 2;
        int x = toolbar.getX();

        auto place = [&] (juce::TextButton& b, int w)
        {
            if (! b.isVisible())
                return;
            b.setBounds (x, y, w, buttonH);
            x += w + 8;
        };

        if (studioBackBtn.isVisible())
        {
            studioBackBtn.setBounds (toolbar.getX(), y, 118, buttonH);
            x = toolbar.getX() + 126;
        }

        place (libraryBtn, 68);
        place (playBtn, 52);

        const bool hasPresets = presetBtn.isVisible();
        if (hasPresets)
        {
            const int presetW = juce::jlimit (96, 200, toolbar.getRight() - x - 120);
            const int presetX = x + juce::jmax (0, (toolbar.getRight() - x - presetW - 62) / 2);
            prevPresetBtn.setBounds (presetX, y, 26, buttonH);
            presetBtn.setBounds (presetX + 30, y, presetW, buttonH);
            nextPresetBtn.setBounds (presetX + 34 + presetW, y, 26, buttonH);
        }

        soundBtn.setBounds (toolbar.getRight() - 72, y, 72, buttonH);
    }

} // namespace patchcraft
