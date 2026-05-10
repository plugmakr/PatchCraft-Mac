#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Velocity map viewer component.
        Displays a visual representation of velocity layers across the keyboard,
        showing how zones are mapped in note/velocity space. Similar to HISE's
        velocity map editor.
    */
    class VelocityMapViewer : public juce::Component
    {
    public:
        VelocityMapViewer();
        ~VelocityMapViewer() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void setZones (const std::vector<SampleZoneDef>& zones);
        void setSelectedZone (int index);

        std::function<void(int)> onZoneClicked;
        std::function<void(int, const SampleZoneDef&)> onZoneEdited;

    private:
        std::vector<SampleZoneDef> currentZones;
        int selectedZoneIndex = -1;

        static juce::Colour zoneColour (int idx);
        juce::String noteName (int midi) const;
        juce::Rectangle<int> plotBounds() const;
        juce::Rectangle<float> zoneRectFor (const SampleZoneDef& zone) const;
        int noteAtX (int x) const;
        int velocityAtY (int y) const;

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

        enum class DragMode { none, move, lowNote, highNote, lowVelocity, highVelocity };
        DragMode dragMode = DragMode::none;
        int dragZoneIndex = -1;
        int dragStartX = 0;
        int dragStartY = 0;
        int dragStartNote = 0;
        int dragStartVelocity = 1;
        SampleZoneDef dragStartZone;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelocityMapViewer)
    };

} // namespace patchcraft
