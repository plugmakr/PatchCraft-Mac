#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Full-width top toolbar that mirrors the reference image:

        [LOGO + PATCHCRAFT / INSTRUMENT BUILDER]
        [New] [Open] [Save] | [Import Samples] [Import BG] [AI Assist when enabled]
                                                   [Preview] [Export Pack]
        [Project name + Unsaved Changes] [Settings]
    */
    class TopToolbar : public juce::Component
    {
    public:
        explicit TopToolbar (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

        void setProjectName (juce::String name, bool dirty);
        void setPreviewActive (bool active);

    private:
        StudioMainComponent& owner;

        // Toolbar buttons - drawn with stacked icon + label.
        class IconLabelButton : public juce::Button
        {
        public:
            IconLabelButton (juce::String label, juce::String iconKey);
            void paintButton (juce::Graphics&, bool over, bool down) override;
            juce::String label;
            juce::String iconKey;
            bool accent = false;
        };

        std::unique_ptr<IconLabelButton> btnNew, btnOpen, btnSave;
        std::unique_ptr<IconLabelButton> btnImportSamples, btnImportBg, btnPacks, btnDashboard, btnProjects, btnAiAssist;
        std::unique_ptr<IconLabelButton> btnPreview, btnExport;

        juce::Label projectNameLabel;
        juce::Label projectStatusLabel;
        juce::ComboBox presetBox;
        juce::TextButton settingsBtn;

        bool previewActive = false;
    };

} // namespace patchcraft
