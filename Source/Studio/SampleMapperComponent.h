#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"
#include "SampleWaveformViewer.h"

namespace patchcraft
{
    class StudioMainComponent;

    /**
        Sample mapper: list of imported samples on the left, zone properties
        across the top, mini coloured keyboard at the bottom showing current
        zone ranges, and Auto Map / Add / Remove action buttons.
    */
    class SampleMapperComponent : public juce::Component,
                                  private juce::ListBoxModel
    {
    public:
        explicit SampleMapperComponent (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();
        int getSelectedZoneIndex() const                   { return selectedZone; }
        const SampleZoneDef* getSelectedZone() const;

    private:
        StudioMainComponent& owner;
        int selectedZone = 0;

        juce::Label samplesHeader;
        juce::ListBox samplesList { "samples", this };
        juce::TextButton addSampleBtn { "+ Add Sample" };

        // Zone fields
        juce::Label  rootLbl, lowLbl, highLbl, lowVelLbl, highVelLbl, gainLbl, panLbl,
                     loopLbl, startLbl, endLbl, bpmLbl;
        juce::ComboBox rootBox, lowBox, highBox;
        juce::TextEditor lowVelEdit, highVelEdit, gainEdit, panEdit, startEdit, endEdit, bpmEdit;
        juce::ToggleButton loopToggle { "" };

        // HISE-style advanced fields
        juce::Label rrGroupLbl, rrIndexLbl, sampleStartLbl, sampleEndLbl;
        juce::Label fadeInStartLbl, fadeInLenLbl, fadeOutStartLbl, fadeOutLenLbl;
        juce::Label pitchOffsetLbl, velXFadeLowerLbl, velXFadeUpperLbl;
        juce::Label priorityLbl, groupLbl;
        juce::TextEditor rrGroupEdit, rrIndexEdit, sampleStartEdit, sampleEndEdit;
        juce::TextEditor fadeInStartEdit, fadeInLenEdit, fadeOutStartEdit, fadeOutLenEdit;
        juce::TextEditor pitchOffsetEdit, velXFadeLowerEdit, velXFadeUpperEdit;
        juce::TextEditor priorityEdit, groupEdit;
        juce::ToggleButton reverseToggle { "Reverse" };

        juce::TextButton importSfzBtn { "Import SFZ" };
        juce::TextButton autoMapBtn   { "Auto Map" };
        juce::TextButton addZoneBtn   { "Add Zone" };
        juce::TextButton removeZoneBtn{ "Remove Zone" };

        // Waveform viewer for sample editing
        std::unique_ptr<SampleWaveformViewer> waveformViewer;

        // Mini keyboard area drawn directly in paint().

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

        void noteToCombo (juce::ComboBox&);
        static juce::String noteName (int midi);

        void writeFromUi();
        void importSample();
        void importSfz();
        void autoMap();
        void addZone();
        void removeZone();

        void drawZoneKeyboard (juce::Graphics& g, juce::Rectangle<int> r) const;
    };

} // namespace patchcraft
