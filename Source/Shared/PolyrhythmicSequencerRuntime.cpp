#include "PolyrhythmicSequencerRuntime.h"

namespace patchcraft
{
    float PolyrhythmicSequencerRuntime::valueForKey (const DspBlock& block,
                                                     const juce::String& key,
                                                     float fallback)
    {
        if (auto it = block.values.find (key); it != block.values.end())
            return it->second;
        return fallback;
    }

    juce::String PolyrhythmicSequencerRuntime::stringForKey (const DspBlock& block,
                                                             const juce::String& key,
                                                             juce::String fallback)
    {
        if (auto it = block.metadata.find (key); it != block.metadata.end())
            return it->second;
        return fallback;
    }

    void PolyrhythmicSequencerRuntime::LaneState::advance (const RenderContext& context, bool sync)
    {
        if (! active || steps <= 0)
            return;

        const double bpm = RenderContext::sanitiseBpm (context.bpm);
        const double cyclesPerSecond = sync ? (bpm / 240.0) * rate : rate;
        
        phase += cyclesPerSecond * context.secondsPerBlock();
        
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            if (direction == "forward")
            {
                currentStep = (currentStep + 1) % steps;
            }
            else if (direction == "reverse")
            {
                currentStep = currentStep - 1;
                if (currentStep < 0) currentStep = steps - 1;
            }
            else if (direction == "pingpong")
            {
                // Basic ping-pong, can be improved to hold direction state
                static bool pinging = true;
                if (pinging)
                {
                    currentStep++;
                    if (currentStep >= steps) { currentStep = steps - 2; pinging = false; }
                }
                else
                {
                    currentStep--;
                    if (currentStep < 0) { currentStep = 1; pinging = true; }
                }
                currentStep = juce::jlimit(0, steps - 1, currentStep);
            }
            else if (direction == "random")
            {
                currentStep = juce::Random::getSystemRandom().nextInt (steps);
            }
            else
            {
                currentStep = (currentStep + 1) % steps;
            }
        }
        
        heldValue = values[(size_t) juce::jlimit (0, steps - 1, currentStep)];
    }

    void PolyrhythmicSequencerRuntime::bind (const DspGraph& graph)
    {
        enabled = false;
        
        for (const auto& block : graph.blocks)
        {
            if (! block.enabled || ! block.type.containsIgnoreCase ("polyrhythmicSequencer"))
                continue;

            enabled = true;
            for (int i = 0; i < kMaxLanes; ++i)
            {
                auto prefix = "seqLane" + juce::String (i);
                auto& lane = lanes[(size_t) i];
                lane.active = valueForKey (block, prefix + "Active", 0.0f) >= 0.5f;
                
                if (lane.active)
                {
                    lane.type = stringForKey (block, prefix + "Type", "gate");
                    lane.target = stringForKey (block, prefix + "Target", "");
                    lane.steps = juce::jlimit (1, kMaxSteps, juce::roundToInt (valueForKey (block, prefix + "Steps", 16.0f)));
                    lane.direction = stringForKey (block, prefix + "Direction", "forward");
                    lane.rate = juce::jlimit (0.0625f, 32.0f, valueForKey (block, prefix + "Rate", 1.0f));
                    
                    for (int s = 0; s < lane.steps; ++s)
                    {
                        lane.values[(size_t) s] = valueForKey (block, prefix + "Step" + juce::String(s), 0.5f);
                    }
                    
                    if (lane.currentStep < 0)
                    {
                        lane.currentStep = 0;
                        lane.phase = 0.0;
                    }
                }
            }
            break; // Only bind the first polyrhythmic sequencer block
        }
    }

    void PolyrhythmicSequencerRuntime::reset()
    {
        for (auto& lane : lanes)
        {
            lane.currentStep = -1;
            lane.phase = 0.0;
        }
        heldNotes.clear();
        heldVelocities.clear();
        activeNotes.fill (0);
        activeNoteCount = 0;
    }

    void PolyrhythmicSequencerRuntime::allNotesOff (IInstrumentEngine& engine)
    {
        for (int note = 0; note < 128; ++note)
        {
            if (activeNotes[(size_t) note] > 0)
            {
                engine.noteOff (note);
                activeNotes[(size_t) note] = 0;
            }
        }
        activeNoteCount = 0;
    }

    bool PolyrhythmicSequencerRuntime::handleNoteOn (IInstrumentEngine& engine, int midiNote, float velocity)
    {
        if (! enabled) return false;
        heldNotes.push_back (midiNote);
        heldVelocities.push_back (velocity);
        return true; // We consume the note and process it later
    }

    bool PolyrhythmicSequencerRuntime::handleNoteOff (IInstrumentEngine& engine, int midiNote)
    {
        if (! enabled) return false;
        auto it = std::find (heldNotes.begin(), heldNotes.end(), midiNote);
        if (it != heldNotes.end())
        {
            auto index = std::distance (heldNotes.begin(), it);
            heldNotes.erase (it);
            heldVelocities.erase (heldVelocities.begin() + index);
        }
        
        if (heldNotes.empty())
        {
            allNotesOff (engine);
            reset();
        }
        return true;
    }

    void PolyrhythmicSequencerRuntime::process (IInstrumentEngine& engine, const RenderContext& context)
    {
        if (! enabled || heldNotes.empty())
            return;

        bool sync = true; // For now default to true
        for (auto& lane : lanes)
        {
            if (lane.active)
                lane.advance (context, sync);
        }
        
        // Example implementation for Pitch and Gate lanes
        int pitchOffset = 0;
        float gateMultiplier = 1.0f;
        float chanceMultiplier = 1.0f;
        
        for (const auto& lane : lanes)
        {
            if (! lane.active) continue;
            
            if (lane.type == "pitch")
            {
                // Map 0.0 - 1.0 to -24 - +24 semitones
                pitchOffset += juce::roundToInt ((lane.heldValue * 48.0f) - 24.0f);
            }
            else if (lane.type == "gate")
            {
                // Simple gating mechanism
                gateMultiplier *= lane.heldValue;
            }
            else if (lane.type == "chance")
            {
                chanceMultiplier *= lane.heldValue;
            }
        }
        
        // Output notes
        // This is a simplified arpeggiator/gate logic for demonstration
        if (gateMultiplier > 0.05f)
        {
            if (activeNoteCount == 0)
            {
                // Play held notes transposed
                for (size_t i = 0; i < heldNotes.size(); ++i)
                {
                    int note = juce::jlimit (0, 127, heldNotes[i] + pitchOffset);
                    engine.noteOn (note, heldVelocities[i]);
                    activeNotes[(size_t) note] = 1;
                    activeNoteCount++;
                }
            }
        }
        else
        {
            allNotesOff (engine);
        }
    }
}
