#pragma once

#include "IInstrumentEngine.h"
#include "PatchCraftTypes.h"
#include "RenderContext.h"

#include <array>
#include <vector>

namespace patchcraft
{
    class ArpeggiatorRuntime
    {
    public:
        void bind (const DspGraph&);
        bool isEnabled() const noexcept { return enabled; }

        void reset();
        void allNotesOff (IInstrumentEngine&);

        bool handleNoteOn (IInstrumentEngine&, int midiNote, float velocity);
        bool handleNoteOff (IInstrumentEngine&, int midiNote);
        void process (IInstrumentEngine&, const RenderContext&);

    private:
        struct Settings
        {
            float rate = 1.0f;
            bool sync = true;
            int steps = 8;
            int pattern = 0;
            float gate = 0.55f;
            int octaves = 2;
            std::array<float, 16> notes {};
        };

        bool enabled = false;
        Settings settings;
        std::vector<int> heldNotes;
        std::array<float, 128> heldVelocities {};
        double phase = 0.0;
        int currentStep = -1;
        int activeNote = -1;
        float activeVelocity = 0.0f;
        bool gateOpen = false;

        static bool isArpBlock (const DspBlock&);
        static float valueForKey (const DspBlock&, const juce::String&, float fallback);

        int sequenceIndexForStep (int step) const;
        int noteForSequenceIndex (int index) const;
        float currentVelocity() const;
        void stopActive (IInstrumentEngine&);
        void startStep (IInstrumentEngine&, int step);
    };
}
