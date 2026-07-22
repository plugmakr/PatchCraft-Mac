#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Left sidebar 'Elements' tab content. Drag rows onto the canvas to add.
    */
    class ElementPalette : public juce::Component
    {
    public:
        explicit ElementPalette (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        StudioMainComponent& owner;

        juce::TextEditor searchBox;
        juce::Viewport viewport;
        juce::Component scrollContent;

        enum class PaletteTab
        {
            Controls,
            Modules,
            Starters
        };

        PaletteTab currentTab = PaletteTab::Controls;

        juce::TextButton controlsTabBtn { "CONTROLS" };
        juce::TextButton modulesTabBtn  { "MODULES" };
        juce::TextButton startersTabBtn  { "STARTERS" };

        void applySearchFilter();
        void setAllSectionsOpen (bool shouldOpen);
        void toggleAllSections();

        struct Row : public juce::Component
        {
            Row (juce::String text, juce::String iconKey, juce::var dragDescription, juce::String subtitle = {});
            void paint (juce::Graphics&) override;
            void mouseEnter (const juce::MouseEvent&) override   { hover = true;  repaint(); }
            void mouseExit  (const juce::MouseEvent&) override   { hover = false; repaint(); }
            void mouseDrag  (const juce::MouseEvent&) override;
            int  getRowHeight() const { return subtitle.isNotEmpty() ? 38 : 28; }

            juce::String text;
            juce::String iconKey;
            juce::var dragDescription;
            juce::String subtitle;
            bool hover = false;
            bool matchesFilter = true;
        };

        struct Section : public juce::Component
        {
            Section (juce::String title);
            void paint (juce::Graphics&) override;
            void resized() override;
            void mouseUp (const juce::MouseEvent&) override;
            void addRow (std::unique_ptr<Row>);
            int  getNeededHeight() const;
            int  applyFilter (const juce::String& query);

            juce::String title;
            juce::OwnedArray<Row> rows;
            std::function<void()> onToggle;
            bool open = true;
            juce::String activeFilter;
        };

        Section controlSection { "Sound + Control Elements" };
        Section analysisSection { "EQ + Analysis" };
        Section uiSection { "Visual UI Elements" };
        Section motionSection { "Reactive Animation" };
        Section performanceSection { "MIDI + Performance" };
        Section containerSection { "Containers + Layout" };
        Section productStarterSection { "Plugin Starters" };
        Section synthModuleSection { "Synth / OSC Modules" };
        Section samplerModuleSection { "Sampler / Scratch Modules" };
        Section drumModuleSection { "Drum Modules" };
        Section midiModuleSection { "MIDI / ARP Modules" };
        Section eqDynamicsModuleSection { "EQ + Dynamics Modules" };
        Section fxModuleSection { "Creative FX Modules" };
        Section outputModuleSection { "Output / Master Modules" };

        juce::TextButton trashBtn  { "Del" };
        juce::TextButton copyBtn   { "Dup" };
        juce::TextButton folderBtn { "..." };

        static juce::var makeElementDrag (ElementType type, const juce::String& label,
                                          const juce::String& parameterId = {});
        static juce::var makeModuleDrag (const juce::String& moduleType, const juce::String& label);
        static juce::var makeActionDrag (const juce::String& action, const juce::String& label);
    };

} // namespace patchcraft
