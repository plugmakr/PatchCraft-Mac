#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "ProductRecipes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /** "What are you building?" — shown when creating a new Quick Build project. */
    class ProjectWizardDialog : public juce::Component
    {
    public:
        explicit ProjectWizardDialog (std::function<void (ProductKind)> onChosen);

        void paint (juce::Graphics&) override;
        void resized() override;

        static void show (juce::Component* parent, std::function<void (ProductKind)> onChosen);

    private:
        std::function<void (ProductKind)> callback;
        juce::Label title;
        juce::Label subtitle;

        struct ChoiceButton : juce::TextButton
        {
            ChoiceButton (const juce::String& name, const juce::String& blurb);
            juce::String blurb;
        };

        std::unique_ptr<ChoiceButton> synthBtn;
        std::unique_ptr<ChoiceButton> sampleBtn;
        std::unique_ptr<ChoiceButton> drumBtn;
        std::unique_ptr<ChoiceButton> loopBtn;
        std::unique_ptr<ChoiceButton> hybridBtn;
        std::unique_ptr<ChoiceButton> fxBtn;
    };

} // namespace patchcraft
