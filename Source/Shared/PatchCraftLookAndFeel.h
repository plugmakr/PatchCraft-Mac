#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace patchcraft
{
    /**
        Shared visual language for PatchCraft Studio (and Player). Matches
        the dark-charcoal + amber/gold reference design.
    */
    class PatchCraftLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        PatchCraftLookAndFeel();

        // ---- Palette ---------------------------------------------------------
        static constexpr juce::uint32 kBgDarkest   = 0xff06080b;
        static constexpr juce::uint32 kBgPanel     = 0xff0d1117;
        static constexpr juce::uint32 kBgPanelAlt  = 0xff131923;
        static constexpr juce::uint32 kBgRaised    = 0xff1b2430;
        static constexpr juce::uint32 kBorder      = 0xff344252;
        static constexpr juce::uint32 kBorderSoft  = 0xff202a36;
        static constexpr juce::uint32 kTextDim     = 0xff8d96a3;
        static constexpr juce::uint32 kText        = 0xffe3e8ef;
        static constexpr juce::uint32 kTextBright  = 0xffffffff;
        static constexpr juce::uint32 kAccent      = 0xfff5a623;   // amber/gold
        static constexpr juce::uint32 kAccentDim   = 0xff9d6817;
        static constexpr juce::uint32 kSecondary   = 0xff58b7ff;
        static constexpr juce::uint32 kZoneA       = 0xff8e6cd6;   // purple sample zone
        static constexpr juce::uint32 kZoneB       = 0xff5fb37b;   // green
        static constexpr juce::uint32 kZoneC       = 0xffe6a13f;   // gold
        static constexpr juce::uint32 kZoneD       = 0xff70747a;   // gray

        static juce::Colour bg()           { return juce::Colour (kBgDarkest); }
        static juce::Colour panel()        { return juce::Colour (kBgPanel); }
        static juce::Colour panelAlt()     { return juce::Colour (kBgPanelAlt); }
        static juce::Colour raised()       { return juce::Colour (kBgRaised); }
        static juce::Colour border()       { return juce::Colour (kBorder); }
        static juce::Colour borderSoft()   { return juce::Colour (kBorderSoft); }
        static juce::Colour text()         { return juce::Colour (kText); }
        static juce::Colour textDim()      { return juce::Colour (kTextDim); }
        static juce::Colour textBright()   { return juce::Colour (kTextBright); }
        static juce::Colour accent()       { return juce::Colour (kAccent); }
        static juce::Colour accentDim()    { return juce::Colour (kAccentDim); }

        // Convenience drawers ------------------------------------------------
        static void drawPanel (juce::Graphics&, juce::Rectangle<int>,
                               float corner = 8.0f, bool drawBorder = true);
        static void drawDarkPanel (juce::Graphics&, juce::Rectangle<int>,
                                   float corner = 8.0f);
        static void drawAccentUnderline (juce::Graphics&, juce::Rectangle<int>);
        static void drawHexLogo (juce::Graphics&, juce::Rectangle<float>);

        // Draw one frame of a filmstrip image stretched into 'dest'. position is 0..1.
        static void drawFilmstripFrame (juce::Graphics&, juce::Rectangle<int> dest,
                                        const juce::Image& strip,
                                        int totalFrames, float position,
                                        bool vertical = true);

        // Auto-detect frame count from a strip image (square frames assumed).
        static int detectFilmstripFrames (const juce::Image& strip,
                                          bool vertical = true);

        // LookAndFeel overrides ---------------------------------------------
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float rotaryStart, float rotaryEnd,
                               juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawButtonText (juce::Graphics&, juce::TextButton&,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getLabelFont    (juce::Label&)    override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        void drawTextEditorOutline (juce::Graphics&, int width, int height,
                                    juce::TextEditor&) override;
        void fillTextEditorBackground (juce::Graphics&, int width, int height,
                                       juce::TextEditor&) override;

        void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                            bool isMouseOver, bool isMouseDown) override;
        int  getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;

        void drawScrollbar (juce::Graphics&, juce::ScrollBar&,
                            int x, int y, int width, int height,
                            bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                            bool isMouseOver, bool isMouseDown) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                                bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText,
                                const juce::Drawable* icon, const juce::Colour* textColourToUse) override;
        juce::Font getPopupMenuFont() override;

        void drawAlertBox (juce::Graphics&, juce::AlertWindow&,
                           const juce::Rectangle<int>& textArea,
                           juce::TextLayout&) override;
        juce::Font getAlertWindowTitleFont() override;
        juce::Font getAlertWindowMessageFont() override;
        juce::Font getAlertWindowFont() override;
    };

} // namespace patchcraft
