#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;
    class ChopStudioPanel;

    /** Dedicated Serato-style chop workspace (full-page Chop Studio). */
    class ChopLabPage : public juce::Component
    {
    public:
        explicit ChopLabPage (StudioMainComponent& owner);
        ~ChopLabPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        void reloadChopSource();
        void applySlices (const std::vector<int>& boundaries, double bpm);
        void importSample();
        void openSoundMapper();

        StudioMainComponent& owner;
        std::unique_ptr<ChopStudioPanel> chopPanel;

        juce::Label header;
        juce::Label subtitle;
        juce::TextButton importBtn { "Import Sample" };
        juce::TextButton soundBtn { "Open Sound Mapper" };
        juce::TextButton layoutBtn { "Open Layout" };
        juce::ToggleButton keyLockToggle { "Key Lock" };
        juce::ToggleButton bpmSyncToggle { "BPM Sync" };
        juce::Label statusLabel;
    };

} // namespace patchcraft
