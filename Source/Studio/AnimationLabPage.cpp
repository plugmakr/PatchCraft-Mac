#include "AnimationLabPage.h"
#include "StudioMainComponent.h"
#include "CanvasEditor.h"
#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    AnimationLabPage::AnimationLabPage (StudioMainComponent& o) : owner (o)
    {
        styleLabel (title, "Animation Lab", 18.0f, true, PatchCraftLookAndFeel::textBright());
        styleLabel (subtitle, "Build visual appeal for instruments without forcing AI: imported art, sprite sheets, procedural FX, and Pro visual generation all land back on the same Design canvas.", 12.0f, false, PatchCraftLookAndFeel::textDim());
        styleLabel (nonProHeader, "NON-PRO / NO-AI VISUALS", 12.0f, true, PatchCraftLookAndFeel::accent());
        styleLabel (nonProBody, "Use native PatchCraft visual elements: reactive artwork slots, sprite animators, glow rings, particles, meters, waveform/spectrum strips, and BPM-synced motion. These export with the Player and do not require network services.", 12.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (proHeader, "PRO / AI VISUALS", 12.0f, true, juce::Colour (0xffc9a4ff));
        styleLabel (proBody, "Use the same visual layer, then generate source assets: title banners, library thumbnails, reactive masks, sprite sheets, and matching artwork families. The AI output becomes regular project artwork so Brand Lab and export can verify it.", 12.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (workflowHeader, "DESIGN -> ANIMATION LAB -> BRAND LAB", 12.0f, true, PatchCraftLookAndFeel::accent());
        styleLabel (workflowBody, "Add visuals here, place and bind them in Design, then prove motion, audio reaction, low-power fallback, and layout in Brand Lab before shipping.", 12.0f, false, PatchCraftLookAndFeel::textDim());
        styleLabel (stepsHeader, "HOW TO USE ANIMATION LAB", 12.0f, true, PatchCraftLookAndFeel::accent());
        styleLabel (stepsBody,
                    "1. Add a complete visual kit for a fast starting point, or add only the element you need.\n"
                    "2. Reactive Image is for logos, backgrounds, masks, or hero art that pulses from audio, MIDI, BPM, or macro values.\n"
                    "3. Sprite Animator is for frame strips: meters, characters, turntables, LEDs, waveform loops, or note-triggered motion.\n"
                    "4. Visual FX Layer is native JUCE motion: particles, rings, pulses, spectrum strips, glow, and low-CPU fallbacks.\n"
                    "5. Pro AI Visual Brief stores the art direction; Generate AI Asset creates source artwork that still becomes normal project art.\n"
                    "6. Open Design to place, size, layer, bind, and theme the visual elements. Open Brand Lab to prove runtime motion.",
                    11.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (proofHeader, "SHIP CHECK", 12.0f, true, juce::Colour (0xffc9a4ff));
        styleLabel (proofBody,
                    "Before export: confirm every visual has a fallback, motion is not distracting while playing, CPU stays reasonable, resized plugin windows keep the artwork framed, and the Player still works with no network or AI service available.",
                    11.0f, false, PatchCraftLookAndFeel::textDim());

        for (auto* label : { &title, &subtitle, &nonProHeader, &nonProBody, &proHeader, &proBody,
                              &workflowHeader, &workflowBody, &stepsHeader, &stepsBody, &proofHeader, &proofBody })
            addAndMakeVisible (*label);

        for (auto* button : { &addVisualKitButton, &addReactiveButton, &addSpriteButton, &addFxButton,
                              &addAiPromptButton, &generateAiAssetButton, &openDesignButton, &openBrandButton })
        {
            styleButton (*button, button == &addVisualKitButton || button == &generateAiAssetButton);
            addAndMakeVisible (*button);
        }

        addVisualKitButton.onClick = [this]
        {
            owner.setBottomTab (BottomPanel::Page::Design);
            if (auto* canvas = owner.getCanvasEditor())
                canvas->addVisualReactivityControlLayout ({ 90, 90 });
        };
        addReactiveButton.onClick = [this] { addCanvasElement ((int) ElementType::ReactiveImage); };
        addSpriteButton.onClick = [this] { addCanvasElement ((int) ElementType::SpriteAnimator); };
        addFxButton.onClick = [this] { addCanvasElement ((int) ElementType::VisualFxLayer); };
        addAiPromptButton.onClick = [this] { addCanvasElement ((int) ElementType::AiVisualPrompt); };
        generateAiAssetButton.onClick = [this] { owner.generateAiImageAsset(); };
        openDesignButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Design); };
        openBrandButton.onClick = [this] { owner.setBottomTab (BottomPanel::Page::Branding); };
    }

    void AnimationLabPage::styleLabel (juce::Label& label, const juce::String& text, float size, bool bold, juce::Colour colour)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, colour);
        label.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
        label.setJustificationType (juce::Justification::topLeft);
    }

    void AnimationLabPage::styleButton (juce::TextButton& button, bool primary)
    {
        button.getProperties().set ("fontSize", 12.0);
        button.getProperties().set ("bold", true);
        if (primary)
            button.getProperties().set ("primaryAction", true);
    }

    void AnimationLabPage::addCanvasElement (int elementTypeIndex)
    {
        owner.setBottomTab (BottomPanel::Page::Design);
        owner.addElementToCanvas ((ElementType) elementTypeIndex);
    }

    void AnimationLabPage::refresh()
    {
        repaint();
    }

    void AnimationLabPage::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        auto r = getLocalBounds().reduced (18);
        auto drawCard = [&] (juce::Rectangle<int> bounds, juce::Colour accent)
        {
            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.86f));
            g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
            g.setColour (accent.withAlpha (0.55f));
            g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
        };
        auto top = r.removeFromTop (82);
        g.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.35f));
        g.fillRoundedRectangle (top.toFloat(), 8.0f);
        r.removeFromTop (12);
        drawCard (r.removeFromLeft (r.getWidth() / 2 - 6), PatchCraftLookAndFeel::accent());
        r.removeFromLeft (12);
        drawCard (r, juce::Colour (0xffb98cff));
    }

    void AnimationLabPage::resized()
    {
        auto r = getLocalBounds().reduced (24, 18);
        auto hero = r.removeFromTop (76);
        title.setBounds (hero.removeFromTop (28));
        subtitle.setBounds (hero);
        r.removeFromTop (18);

        auto left = r.removeFromLeft (r.getWidth() / 2 - 8).reduced (16);
        r.removeFromLeft (16);
        auto right = r.reduced (16);

        nonProHeader.setBounds (left.removeFromTop (24));
        nonProBody.setBounds (left.removeFromTop (86));
        left.removeFromTop (10);
        addVisualKitButton.setBounds (left.removeFromTop (34));
        left.removeFromTop (8);
        addReactiveButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (6);
        addSpriteButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (6);
        addFxButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (14);
        workflowHeader.setBounds (left.removeFromTop (24));
        workflowBody.setBounds (left.removeFromTop (58));
        left.removeFromTop (8);
        openDesignButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (6);
        openBrandButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (12);
        stepsHeader.setBounds (left.removeFromTop (24));
        stepsBody.setBounds (left);

        proHeader.setBounds (right.removeFromTop (24));
        proBody.setBounds (right.removeFromTop (116));
        right.removeFromTop (10);
        addAiPromptButton.setBounds (right.removeFromTop (34));
        right.removeFromTop (8);
        generateAiAssetButton.setBounds (right.removeFromTop (34));
        right.removeFromTop (14);
        proofHeader.setBounds (right.removeFromTop (24));
        proofBody.setBounds (right.removeFromTop (112));
    }
}
