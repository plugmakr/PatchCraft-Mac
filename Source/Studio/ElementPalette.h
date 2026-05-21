#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Left sidebar 'Elements' tab content. Lists Add Element + Components.
    */
    class ElementPalette : public juce::Component
    {
    public:
        explicit ElementPalette (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        StudioMainComponent& owner;

        juce::Viewport viewport;
        juce::Component scrollContent;

        struct Row : public juce::Component
        {
            Row (juce::String text, juce::String iconKey,
                 std::function<void()> onClick);
            void paint (juce::Graphics&) override;
            void mouseEnter (const juce::MouseEvent&) override   { hover = true;  repaint(); }
            void mouseExit  (const juce::MouseEvent&) override   { hover = false; repaint(); }
            void mouseUp    (const juce::MouseEvent&) override;

            juce::String text;
            juce::String iconKey;
            std::function<void()> onClick;
            bool hover = false;
        };

        struct Section : public juce::Component
        {
            Section (juce::String title);
            void paint (juce::Graphics&) override;
            void resized() override;
            void mouseUp (const juce::MouseEvent&) override;
            void addRow (std::unique_ptr<Row>);
            int  getNeededHeight() const;

            juce::String title;
            juce::OwnedArray<Row> rows;
            std::function<void()> onToggle;
            bool open = true;
        };

        Section controlSection { "Sound + Control Elements" };
        Section analysisSection { "EQ + Analysis" };
        Section uiSection { "Visual UI Elements" };
        Section performanceSection { "MIDI + Performance" };
        Section containerSection { "Containers + Layout" };

        juce::TextButton trashBtn  { "Del" };
        juce::TextButton copyBtn   { "Dup" };
        juce::TextButton folderBtn { "..." };

        void addElementOfType (ElementType);
    };

} // namespace patchcraft
