#pragma once

#include "IInstrumentEngine.h"
#include "PatchCraftTypes.h"
#include "RenderContext.h"
#include <array>
#include <vector>

namespace patchcraft
{
    class PolyrhythmicSequencerRuntime
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
        static constexpr int kMaxLanes = 16;
        static constexpr int kMaxSteps = 128;

        struct LaneState
        {
            bool active = false;
            juce::String type { "gate" };
            juce::String target { "" };
            int steps = 16;
            juce::String direction { "forward" };
            float rate = 1.0f;
            
            std::array<float, kMaxSteps> values {};
            
            double phase = 0.0;
            int currentStep = -1;
            float heldValue = 0.0f;
            
            void advance (const RenderContext& context, bool sync);
        };

        bool enabled = false;
        std::array<LaneState, kMaxLanes> lanes {};
        
        std::vector<int> heldNotes;
        std::vector<float> heldVelocities;
        std::array<int, 128> activeNotes {};
        int activeNoteCount = 0;
        
        static float valueForKey (const DspBlock& block, const juce::String& key, float fallback = 0.0f);
        static juce::String stringForKey (const DspBlock& block, const juce::String& key, juce::String fallback = "");
    };
}
