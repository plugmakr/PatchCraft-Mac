#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class AnimationLabPage : public juce::Component,
                             private juce::Timer
    {
    public:
        explicit AnimationLabPage (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        StudioMainComponent& owner;

        juce::Label title;
        juce::Label subtitle;
        juce::Label nonProHeader;
        juce::Label nonProBody;
        juce::Label proHeader;
        juce::Label proBody;
        juce::Label workflowHeader;
        juce::Label workflowBody;
        juce::Label stepsHeader;
        juce::Label stepsBody;
        juce::Label proofHeader;
        juce::Label proofBody;
        juce::Label bindHeader;
        juce::Label bindBody;
        juce::Label targetLabel;
        juce::Label sourceLabel;
        juce::Label actionLabel;
        juce::Label previewLabel;
        juce::ComboBox targetBox;
        juce::ComboBox sourceBox;
        juce::ComboBox actionBox;

        juce::TextButton addVisualKitButton { "Add Complete Visual Kit" };
        juce::TextButton addReactiveButton { "Reactive Image" };
        juce::TextButton addSpriteButton { "Sprite Animator" };
        juce::TextButton addFxButton { "Visual FX Layer" };
        juce::TextButton addBoundVisualButton { "Add Bound Visual" };
        juce::TextButton previewMotionButton { "Preview Motion" };
        juce::TextButton addAiPromptButton { "Pro AI Visual Brief" };
        juce::TextButton generateAiAssetButton { "Generate AI Asset" };
        juce::TextButton openDesignButton { "Open Design" };
        juce::TextButton openBrandButton { "Preview in Brand Lab" };

        void styleLabel (juce::Label&, const juce::String& text, float size, bool bold, juce::Colour colour);
        void styleButton (juce::TextButton&, bool primary = false);
        void addCanvasElement (int elementTypeIndex);
        void addBoundVisualToCanvas();
        void refreshParameterChoices();
        void timerCallback() override;
        static juce::String reactiveModeForSource (const juce::String& source);
        static juce::String fxPresetForAction (const juce::String& action);
        void drawPreviewEffect (juce::Graphics&, juce::Rectangle<float> area);

        bool previewActive = false;
        float previewPhase = 0.0f;
    };
}
