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
                    "1. Pick a DRIVER: output level, a frequency band (bass / mids / highs), "
                    "transients, RMS, BPM sync, MIDI, or a parameter.\n"
                    "2. Pick an EFFECT: spectrum bars, waveform scope, orbit, sweep, particle burst, glow, or sprite.\n"
                    "3. Click Add Bound Visual - it lands on the Design canvas already wired to that driver.\n"
                    "4. In Inspector, fine-tune reactivity amount and speed.\n"
                    "5. The effect runs live in the Player, reacting to real audio.",
                    11.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (proofHeader, "SHIP CHECK", 12.0f, true, juce::Colour (0xffc9a4ff));
        styleLabel (proofBody,
                    "Before export: confirm every visual has a fallback, motion is not distracting while playing, CPU stays reasonable, resized plugin windows keep the artwork framed, and the Player still works with no network or AI service available.",
                    11.0f, false, PatchCraftLookAndFeel::textDim());
        styleLabel (bindHeader, "ADD ANIMATION", 12.0f, true, PatchCraftLookAndFeel::accent());
        styleLabel (bindBody, "Build a visual layer that is already connected to audio, BPM, MIDI, or a parameter. The element lands on the Design canvas with the binding saved.", 12.0f, false, PatchCraftLookAndFeel::text());
        styleLabel (targetLabel, "PARAMETER", 11.0f, true, PatchCraftLookAndFeel::textDim());
        styleLabel (sourceLabel, "DRIVER", 11.0f, true, PatchCraftLookAndFeel::textDim());
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

        sourceBox.addItem ("Output Level", 1);
        sourceBox.addItem ("Bass (Low Band)", 2);
        sourceBox.addItem ("Mids (Mid Band)", 3);
        sourceBox.addItem ("Highs (High Band)", 4);
        sourceBox.addItem ("Transient / Attack", 5);
        sourceBox.addItem ("RMS Loudness", 6);
        sourceBox.addItem ("BPM Sync", 7);
        sourceBox.addItem ("MIDI Notes", 8);
        sourceBox.addItem ("Selected Parameter", 9);
        sourceBox.setSelectedId (1, juce::dontSendNotification);
        sourceBox.onChange = [this] { repaint(); };
        addAndMakeVisible (sourceBox);

        actionBox.addItem ("Spectrum Bars", 1);
        actionBox.addItem ("Waveform Scope", 2);
        actionBox.addItem ("Pulse Glow", 3);
        actionBox.addItem ("Orbit", 4);
        actionBox.addItem ("Sweep", 5);
        actionBox.addItem ("Particle Burst", 6);
        actionBox.addItem ("Sprite Frame", 7);
        actionBox.addItem ("Scale", 8);
        actionBox.addItem ("Shake", 9);
        actionBox.setSelectedId (1, juce::dontSendNotification);
        actionBox.onChange = [this] { repaint(); };
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
        juce::String currentParamId;
        const auto currentComboId = targetBox.getSelectedId();
        if (currentComboId > 0)
            currentParamId = targetBox.getProperties()["param_" + juce::String (currentComboId)].toString();

        targetBox.clear (juce::dontSendNotification);
        targetBox.getProperties().clear();

        // Group visible parameters by category
        std::map<juce::String, std::vector<ParameterDef>> grouped;
        for (const auto& param : owner.getProject().getParameters().getAll())
        {
            if (! param.visible)
                continue;
            juce::String cat = param.category.isNotEmpty() ? param.category : "General";
            grouped[cat].push_back (param);
        }

        int pid = 1;
        int matchId = 1;
        for (const auto& pair : grouped)
        {
            targetBox.addSectionHeading (pair.first);
            for (const auto& param : pair.second)
            {
                targetBox.addItem (param.name.isNotEmpty() ? param.name + "  (" + param.id + ")" : param.id, pid);
                targetBox.getProperties().set ("param_" + juce::String (pid), param.id);
                if (param.id == currentParamId)
                    matchId = pid;
                pid++;
            }
        }

        if (targetBox.getNumItems() > 0)
            targetBox.setSelectedId (matchId, juce::dontSendNotification);
    }

    juce::String AnimationLabPage::reactiveModeForSource (const juce::String& source)
    {
        if (source.startsWithIgnoreCase ("Bass"))      return "lowBand";
        if (source.startsWithIgnoreCase ("Mids"))      return "midBand";
        if (source.startsWithIgnoreCase ("Highs"))     return "highBand";
        if (source.startsWithIgnoreCase ("Transient")) return "transient";
        if (source.startsWithIgnoreCase ("RMS"))       return "rms";
        return "level";
    }

    juce::String AnimationLabPage::fxPresetForAction (const juce::String& action)
    {
        if (action.equalsIgnoreCase ("Spectrum Bars"))   return "spectrumBars";
        if (action.equalsIgnoreCase ("Waveform Scope"))  return "waveform";
        if (action.equalsIgnoreCase ("Orbit"))           return "orbit";
        if (action.equalsIgnoreCase ("Sweep"))           return "sweep";
        if (action.equalsIgnoreCase ("Particle Burst"))  return "particles";
        return "glow";
    }

    void AnimationLabPage::addBoundVisualToCanvas()
    {
        auto source = sourceBox.getText().trim();
        auto action = actionBox.getText().trim();
        auto parameterId = targetBox.getProperties()["param_" + juce::String (targetBox.getSelectedId())].toString();
        if (parameterId.isEmpty())
            parameterId = "macro1";

        const bool isBpm   = source.startsWithIgnoreCase ("BPM");
        const bool isParam = source.startsWithIgnoreCase ("Selected");
        const bool isMidi  = source.startsWithIgnoreCase ("MIDI");
        const bool isAudio = ! (isBpm || isParam || isMidi);
        const auto fxPreset = fxPresetForAction (action);

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
            visual.audioReactive = isAudio;
            visual.audioReactiveMode = reactiveModeForSource (source);
            visual.audioReactiveAmount = isAudio ? 0.7f : 0.3f;
            visual.animationMode = isBpm ? "bpmPulse" : (isParam ? "parameter" : "none");
            visual.animationRate = isBpm ? 1.0f : (fxPreset == "spectrumBars" ? 2.0f : 0.6f);
            visual.visualSource = isParam ? parameterId
                                : isMidi  ? "midiNotes"
                                : isBpm   ? "bpm"
                                : reactiveModeForSource (source);
            visual.visualAction = fxPreset;
            visual.visualPreset = fxPreset;
            visual.visualLowPowerFallback = true;
            visual.opacity = 0.9f;
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
        auto p = preview.toFloat();
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (p, 12.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.5f));
        g.drawRoundedRectangle (p.reduced (1.0f), 12.0f, 1.2f);

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (preview.reduced (3));
            drawPreviewEffect (g, p.reduced (10));
        }

        g.setColour (PatchCraftLookAndFeel::textBright());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawFittedText (sourceBox.getText() + "  ->  " + actionBox.getText(),
                          preview.reduced (14).removeFromBottom (22), juce::Justification::centred, 1);
    }

    void AnimationLabPage::drawPreviewEffect (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto accent = PatchCraftLookAndFeel::accent();
        const float t = previewPhase;                       // 0..1 loop
        const float seconds = previewPhase * 6.2831853f;
        // Synthetic "audio" so the preview moves even without playback.
        const float level = previewActive
            ? juce::jlimit (0.0f, 1.0f, 0.45f + 0.45f * std::sin (seconds * 1.7f))
            : 0.5f;
        const auto fx = fxPresetForAction (actionBox.getText().trim());
        const auto centre = area.getCentre();

        if (fx == "spectrumBars")
        {
            const int bars = 22;
            const float bw = area.getWidth() / (float) bars;
            for (int i = 0; i < bars; ++i)
            {
                const float ph = seconds * 2.0f + (float) i * 0.5f;
                const float h = juce::jlimit (0.06f, 1.0f, 0.2f + 0.8f * std::abs (std::sin (ph)) * (0.4f + level)) * area.getHeight();
                g.setColour (accent.withRotatedHue ((float) i / (float) bars * 0.12f - 0.06f).withAlpha (0.85f));
                g.fillRoundedRectangle (area.getX() + (float) i * bw + 1.0f, area.getBottom() - h, bw - 2.0f, h, 1.5f);
            }
            return;
        }
        if (fx == "waveform")
        {
            juce::Path path;
            const int n = 80;
            for (int i = 0; i < n; ++i)
            {
                const float x = area.getX() + area.getWidth() * ((float) i / (float) (n - 1));
                const float y = centre.y - std::sin ((float) i * 0.35f + seconds * 3.0f) * area.getHeight() * 0.4f * (0.4f + level);
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.95f));
            g.strokePath (path, juce::PathStrokeType (2.0f));
            return;
        }
        if (fx == "orbit")
        {
            for (int i = 0; i < 24; ++i)
            {
                const float ph = seconds * 1.6f + (float) i * juce::MathConstants<float>::twoPi / 24.0f;
                const float rad = area.getHeight() * 0.18f * (1.0f + level * 1.3f) + (float) (i % 4) * 4.0f;
                const float sz = 3.0f + level * 5.0f;
                g.setColour (accent.withRotatedHue ((float) i / 24.0f * 0.1f).withAlpha (0.7f));
                g.fillEllipse (centre.x + std::cos (ph) * rad - sz * 0.5f, centre.y + std::sin (ph) * rad * 0.78f - sz * 0.5f, sz, sz);
            }
            return;
        }
        if (fx == "sweep")
        {
            const float x = area.getX() + area.getWidth() * t;
            const float w = juce::jmax (16.0f, area.getWidth() * (0.12f + level * 0.18f));
            juce::ColourGradient grad (accent.withAlpha (0.0f), x - w, centre.y, accent.withAlpha (0.0f), x + w, centre.y, false);
            grad.addColour (0.5, accent.withAlpha (0.8f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x - w, area.getY(), w * 2.0f, area.getHeight()));
            return;
        }
        if (fx == "particles")
        {
            for (int i = 0; i < 30; ++i)
            {
                const float ph = (float) i * 0.61f;
                const float tt = std::fmod (t + (float) i * 0.13f, 1.0f);
                const float rad = tt * area.getHeight() * 0.5f * (0.6f + level);
                const float sz = (1.0f - tt) * (3.0f + level * 6.0f);
                if (sz <= 0.3f) continue;
                g.setColour (accent.withRotatedHue ((float) i / 30.0f * 0.15f).withAlpha ((1.0f - tt) * 0.8f));
                g.fillEllipse (centre.x + std::cos (ph) * rad - sz * 0.5f, centre.y + std::sin (ph) * rad - sz * 0.5f, sz, sz);
            }
            return;
        }
        // glow
        for (int ring = 4; ring >= 0; --ring)
        {
            const float pulse = 0.5f + 0.5f * std::sin (seconds * 2.0f);
            const float rr = area.getHeight() * 0.5f * (0.35f + 0.16f * (float) ring) * (0.8f + level * 0.6f + pulse * 0.1f);
            g.setColour (accent.withAlpha ((0.10f + level * 0.30f) * (1.0f - (float) ring * 0.16f)));
            g.fillEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f);
        }
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
        bindBody.setBounds (left.removeFromTop (62));
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
