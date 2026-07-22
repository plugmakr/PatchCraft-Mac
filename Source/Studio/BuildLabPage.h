#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "ProductRecipes.h"

namespace patchcraft
{
    class StudioMainComponent;

    class BuildLabPage : public juce::Component
    {
    public:
        enum class SubPage { ImportSounds = 0, Chop, Stack, Perform };

        explicit BuildLabPage (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        void setSubPage (SubPage page);
        SubPage getSubPage() const { return activeSubPage; }

        bool chopStepAvailable() const;
        int getHeaderHeight() const { return 110; }

    private:
        StudioMainComponent& owner;

        juce::Label productLabel;
        juce::Label stepHint;
        juce::Label previewContractLabel;
        juce::TextButton importBtn { "1  Import" };
        juce::TextButton chopBtn   { "2  Chop" };
        juce::TextButton stackBtn  { "3  Shape" };
        juce::TextButton performBtn { "4  Perform" };
        juce::TextButton changeTypeBtn { "Change Type" };
        juce::TextButton importSamplesBtn { "Import Samples" };
        juce::TextButton advancedMapBtn { "Advanced Mapping" };
        juce::TextButton layerWizardBtn { "Layer Rack" };
        juce::TextButton previewBtn { "Preview in Player" };
        juce::TextButton shipBtn { "Ship" };

        SubPage activeSubPage = SubPage::ImportSounds;

        void updateStepVisibility();
        void styleStepButton (juce::TextButton& b, int radioId);
    };

} // namespace patchcraft
