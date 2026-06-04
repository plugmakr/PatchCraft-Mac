#include "AnimationLabPage.h"
#include "StudioMainComponent.h"
#include "CanvasEditor.h"
#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"

#include <cmath>

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
                    "1. Choose a driver and a visual behavior.\n"
                    "2. Add the bound visual to the canvas.\n"
                    "3. Move or resize it in Design.\n"
                    "4. Test runtime motion in Brand Lab before export.",
                    11.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (proofHeader, "SHIP CHECK", 12.0f, true, juce::Colour (0xffc9a4ff));
        styleLabel (proofBody,
                    "Before export: confirm every visual has a fallback, motion is not distracting while playing, CPU stays reasonable, resized plugin windows keep the artwork framed, and the Player still works with no network or AI service available.",
                    11.0f, false, PatchCraftLookAndFeel::textDim());
        styleLabel (bindHeader, "ADD ANIMATION", 12.0f, true, PatchCraftLookAndFeel::accent());
        styleLabel (bindBody, "Build a visual layer that is already connected to audio, BPM, MIDI, or a parameter. The element lands on the Design canvas with the binding saved.", 12.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (targetLabel, "DRIVER", 11.0f, true, PatchCraftLookAndFeel::textDim());
        styleLabel (sourceLabel, "SOURCE", 11.0f, true, PatchCraftLookAndFeel::textDim());
        styleLabel (actionLabel, "MOTION", 11.0f, true, PatchCraftLookAndFeel::textDim());
        styleLabel (previewLabel, "PREVIEW", 11.0f, true, PatchCraftLookAndFeel::textDim());

        for (auto* label : { &title, &subtitle, &nonProHeader, &nonProBody, &proHeader, &proBody,
                              &workflowHeader, &workflowBody, &stepsHeader, &stepsBody, &proofHeader, &proofBody,
                              &bindHeader, &bindBody, &targetLabel, &sourceLabel, &actionLabel, &previewLabel })
            addAndMakeVisible (*label);

        for (auto* button : { &addVisualKitButton, &addReactiveButton, &addSpriteButton, &addFxButton,
                              &addBoundVisualButton, &previewMotionButton, &addAiPromptButton, &generateAiAssetButton,
                              &openDesignButton, &openBrandButton })
        {
            styleButton (*button, button == &addVisualKitButton || button == &addBoundVisualButton || button == &generateAiAssetButton);
            addAndMakeVisible (*button);
        }

        sourceBox.addItem ("Audio Level", 1);
        sourceBox.addItem ("BPM", 2);
        sourceBox.addItem ("MIDI Notes", 3);
        sourceBox.addItem ("Selected Parameter", 4);
        sourceBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (sourceBox);

        actionBox.addItem ("Pulse Glow", 1);
        actionBox.addItem ("Scale", 2);
        actionBox.addItem ("Orbit", 3);
        actionBox.addItem ("Sweep", 4);
        actionBox.addItem ("Sprite Frame", 5);
        actionBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (actionBox);
        addAndMakeVisible (targetBox);
        refreshParameterChoices();

        addVisualKitButton.onClick = [this]
        {
            owner.setBottomTab (BottomPanel::Page::Design);
            if (auto* canvas = owner.getCanvasEditor())
                canvas->addVisualReactivityControlLayout ({ 90, 90 });
        };
        addReactiveButton.onClick = [this] { addCanvasElement ((int) ElementType::ReactiveImage); };
        addSpriteButton.onClick = [this] { addCanvasElement ((int) ElementType::SpriteAnimator); };
        addFxButton.onClick = [this] { addCanvasElement ((int) ElementType::VisualFxLayer); };
        addBoundVisualButton.onClick = [this] { addBoundVisualToCanvas(); };
        previewMotionButton.onClick = [this]
        {
            previewActive = ! previewActive;
            previewMotionButton.setButtonText (previewActive ? "Stop Preview" : "Preview Motion");
            if (previewActive) startTimerHz (30); else stopTimer();
            repaint();
        };
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

    void AnimationLabPage::refreshParameterChoices()
    {
        const auto currentId = targetBox.getSelectedId();
        targetBox.clear (juce::dontSendNotification);
        int id = 1;
        for (const auto& param : owner.getProject().getParameters().getAll())
        {
            if (! param.visible)
                continue;
            targetBox.addItem (param.name.isNotEmpty() ? param.name + "  (" + param.id + ")" : param.id, id++);
            targetBox.getProperties().set ("param_" + juce::String (id - 1), param.id);
        }
        if (targetBox.getNumItems() > 0)
            targetBox.setSelectedId (currentId > 0 ? juce::jmin (currentId, targetBox.getNumItems()) : 1,
                                     juce::dontSendNotification);
    }

    void AnimationLabPage::addBoundVisualToCanvas()
    {
        auto source = sourceBox.getText().trim();
        auto action = actionBox.getText().trim();
        auto parameterId = targetBox.getProperties()["param_" + juce::String (targetBox.getSelectedId())].toString();
        if (parameterId.isEmpty())
            parameterId = "macro1";

        owner.getProject().performLayoutEdit ("Add bound animation visual", [&] (LayoutModel& layout)
        {
            const auto& canvas = owner.getProject().getCanvasSize();
            LayoutElement visual;
            visual.id = "visual_bind_" + juce::String (juce::Time::getMillisecondCounter());
            visual.type = action.equalsIgnoreCase ("Sprite Frame") ? ElementType::SpriteAnimator : ElementType::VisualFxLayer;
            visual.label = action;
            visual.parameterId = parameterId;
            visual.x = juce::jmax (40, canvas.width / 2 - 180);
            visual.y = juce::jmax (40, canvas.height / 2 - 130);
            visual.width = 360;
            visual.height = 220;
            visual.audioReactive = source.equalsIgnoreCase ("Audio Level");
            visual.audioReactiveMode = "level";
            visual.audioReactiveAmount = source.equalsIgnoreCase ("Audio Level") ? 0.65f : 0.25f;
            visual.animationMode = source.equalsIgnoreCase ("BPM") ? "bpmPulse" : "parameter";
            visual.animationRate = source.equalsIgnoreCase ("BPM") ? 1.0f : 0.5f;
            visual.visualSource = source.equalsIgnoreCase ("Selected Parameter") ? parameterId
                                : source.equalsIgnoreCase ("MIDI Notes") ? "midiNotes"
                                : source.equalsIgnoreCase ("BPM") ? "bpm"
                                : "audioLevel";
            visual.visualAction = action.removeCharacters (" ").toLowerCase();
            visual.visualPreset = action.equalsIgnoreCase ("Orbit") ? "orbitAura"
                                : action.equalsIgnoreCase ("Sweep") ? "spectrumSweep"
                                : "pulseGlow";
            visual.visualLowPowerFallback = true;
            visual.opacity = 0.85f;
            visual.cornerRadius = 12.0f;
            visual.accentColour = PatchCraftLookAndFeel::accent();
            layout.add (visual);
        });

        owner.getProject().notifyChanged();
        owner.setBottomTab (BottomPanel::Page::Design);
    }

    void AnimationLabPage::refresh()
    {
        refreshParameterChoices();
        repaint();
    }

    void AnimationLabPage::timerCallback()
    {
        previewPhase += 0.035f;
        if (previewPhase > 1.0f)
            previewPhase -= 1.0f;
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

        auto preview = getLocalBounds().reduced (40);
        preview.removeFromTop (178);
        preview = preview.removeFromRight (preview.getWidth() / 2 - 24).removeFromTop (162);
        const float pulse = previewActive ? (0.35f + 0.65f * std::sin (previewPhase * juce::MathConstants<float>::twoPi) * 0.5f + 0.35f) : 0.45f;
        auto p = preview.toFloat();
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.12f + 0.24f * pulse));
        g.fillRoundedRectangle (p, 12.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.6f + 0.3f * pulse));
        g.drawRoundedRectangle (p.reduced (1.0f), 12.0f, 1.4f + 2.2f * pulse);
        auto centre = p.getCentre();
        const float radius = 34.0f + 26.0f * pulse;
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawFittedText (sourceBox.getText() + " -> " + actionBox.getText(), preview.reduced (14).removeFromBottom (24), juce::Justification::centred, 1);
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
        nonProBody.setBounds (left.removeFromTop (54));
        left.removeFromTop (10);
        addVisualKitButton.setBounds (left.removeFromTop (34));
        left.removeFromTop (8);
        bindHeader.setBounds (left.removeFromTop (24));
        bindBody.setBounds (left.removeFromTop (48));
        auto row = left.removeFromTop (54);
        auto third = row.getWidth() / 3;
        auto targetArea = row.removeFromLeft (third).reduced (0, 0);
        auto sourceArea = row.removeFromLeft (third).reduced (8, 0);
        auto actionArea = row.reduced (8, 0);
        targetLabel.setBounds (targetArea.removeFromTop (18));
        targetBox.setBounds (targetArea);
        sourceLabel.setBounds (sourceArea.removeFromTop (18));
        sourceBox.setBounds (sourceArea);
        actionLabel.setBounds (actionArea.removeFromTop (18));
        actionBox.setBounds (actionArea);
        left.removeFromTop (8);
        addBoundVisualButton.setBounds (left.removeFromTop (34));
        left.removeFromTop (8);
        addReactiveButton.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        addSpriteButton.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        addFxButton.setBounds (left.removeFromTop (30));
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
        proBody.setBounds (right.removeFromTop (86));
        right.removeFromTop (10);
        previewLabel.setBounds (right.removeFromTop (20));
        previewMotionButton.setBounds (right.removeFromTop (34));
        right.removeFromTop (178);
        addAiPromptButton.setBounds (right.removeFromTop (34));
        right.removeFromTop (8);
        generateAiAssetButton.setBounds (right.removeFromTop (34));
        right.removeFromTop (14);
        proofHeader.setBounds (right.removeFromTop (24));
        proofBody.setBounds (right.removeFromTop (112));
    }
}
