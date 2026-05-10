#include "ArpeggiatorRuntime.h"

#include <algorithm>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        constexpr std::array<float, 16> kDefaultNotes {
            0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 10.0f, 14.0f,
            12.0f, 7.0f, 4.0f, 0.0f, 5.0f, 9.0f, 12.0f, 16.0f
        };
    }

    bool ArpeggiatorRuntime::isArpBlock (const DspBlock& block)
    {
        const auto type = block.type.trim().toLowerCase();
        return type == "arp" || type == "arpeggiator" || type == "arpsequencer"
            || type == "arpstepsequencer" || type == "arp step sequencer";
    }

    float ArpeggiatorRuntime::valueForKey (const DspBlock& block,
                                           const juce::String& key,
                                           float fallback)
    {
        if (auto it = block.values.find (key); it != block.values.end())
            return it->second;
        return fallback;
    }

    void ArpeggiatorRuntime::bind (const DspGraph& graph)
    {
        reset();
        enabled = false;
        settings = {};
        settings.notes = kDefaultNotes;

        for (const auto& block : graph.blocks)
        {
            if (! block.enabled || ! isArpBlock (block))
                continue;

            settings.rate = juce::jlimit (0.0625f, 16.0f, valueForKey (block, "rate", 1.0f));
            settings.sync = valueForKey (block, "sync", 1.0f) >= 0.5f;
            settings.steps = juce::jlimit (1, 16, juce::roundToInt (valueForKey (block, "arpSteps", 8.0f)));
            settings.pattern = juce::jlimit (0, 5, juce::roundToInt (valueForKey (block, "arpPattern", 0.0f)));
            settings.gate = juce::jlimit (0.05f, 1.0f, valueForKey (block, "arpGate", 0.55f));
            settings.octaves = juce::jlimit (1, 4, juce::roundToInt (valueForKey (block, "arpOctaves", 2.0f)));

            for (int step = 0; step < (int) settings.notes.size(); ++step)
                settings.notes[(size_t) step] = valueForKey (block, "arpNote" + juce::String (step), kDefaultNotes[(size_t) step]);

            enabled = true;
            break;
        }
    }

    void ArpeggiatorRuntime::reset()
    {
        heldNotes.clear();
        heldVelocities.fill (0.0f);
        phase = 0.0;
        currentStep = -1;
        activeNote = -1;
        activeVelocity = 0.0f;
        gateOpen = false;
    }

    void ArpeggiatorRuntime::allNotesOff (IInstrumentEngine& engine)
    {
        stopActive (engine);
        reset();
    }

    bool ArpeggiatorRuntime::handleNoteOn (IInstrumentEngine&, int midiNote, float velocity)
    {
        if (! enabled)
            return false;

        const int note = juce::jlimit (0, 127, midiNote);
        heldVelocities[(size_t) note] = juce::jlimit (0.0f, 1.0f, velocity);
        if (std::find (heldNotes.begin(), heldNotes.end(), note) == heldNotes.end())
        {
            heldNotes.push_back (note);
            std::sort (heldNotes.begin(), heldNotes.end());
        }

        if (heldNotes.size() == 1)
        {
            phase = 0.0;
            currentStep = -1;
            gateOpen = false;
        }

        return true;
    }

    bool ArpeggiatorRuntime::handleNoteOff (IInstrumentEngine& engine, int midiNote)
    {
        if (! enabled)
            return false;

        const int note = juce::jlimit (0, 127, midiNote);
        heldVelocities[(size_t) note] = 0.0f;
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());

        if (heldNotes.empty())
        {
            stopActive (engine);
            currentStep = -1;
            phase = 0.0;
            gateOpen = false;
        }
        else if (activeNote == note)
        {
            stopActive (engine);
            currentStep = -1;
            gateOpen = false;
        }

        return true;
    }

    int ArpeggiatorRuntime::sequenceIndexForStep (int step) const
    {
        const int steps = juce::jlimit (1, 16, settings.steps);
        step = juce::jlimit (0, steps - 1, step);

        int index = step;
        if (settings.pattern == 1)
            index = steps - 1 - step;
        else if (settings.pattern == 2)
            index = step <= steps / 2 ? step : juce::jmax (0, steps - 1 - step);
        else if (settings.pattern == 4)
            index = (step * 2 + 1) % steps;
        else if (settings.pattern == 5)
            index = (step * 2) % steps;

        if (settings.pattern == 3)
            index = (step % 4) * 2;

        return juce::jlimit (0, 15, index);
    }

    int ArpeggiatorRuntime::noteForSequenceIndex (int index) const
    {
        if (heldNotes.empty())
            return -1;

        const int offset = juce::roundToInt (settings.notes[(size_t) juce::jlimit (0, 15, index)]);

        if (heldNotes.size() == 1)
            return juce::jlimit (0, 127, heldNotes.front() + offset);

        const int noteIndex = juce::jlimit (0, (int) heldNotes.size() - 1, index % (int) heldNotes.size());
        const int octave = (index / juce::jmax (1, (int) heldNotes.size())) % juce::jmax (1, settings.octaves);
        return juce::jlimit (0, 127, heldNotes[(size_t) noteIndex] + octave * 12);
    }

    float ArpeggiatorRuntime::currentVelocity() const
    {
        float velocity = activeVelocity > 0.0f ? activeVelocity : 0.75f;
        for (const auto note : heldNotes)
            velocity = juce::jmax (velocity, heldVelocities[(size_t) juce::jlimit (0, 127, note)]);
        return juce::jlimit (0.01f, 1.0f, velocity);
    }

    void ArpeggiatorRuntime::stopActive (IInstrumentEngine& engine)
    {
        if (activeNote >= 0)
            engine.noteOff (activeNote);

        activeNote = -1;
        activeVelocity = 0.0f;
        gateOpen = false;
    }

    void ArpeggiatorRuntime::startStep (IInstrumentEngine& engine, int step)
    {
        const int note = noteForSequenceIndex (sequenceIndexForStep (step));
        if (note < 0)
            return;

        stopActive (engine);
        activeNote = note;
        activeVelocity = currentVelocity();
        gateOpen = true;
        engine.noteOn (activeNote, activeVelocity);
    }

    void ArpeggiatorRuntime::process (IInstrumentEngine& engine, const RenderContext& context)
    {
        if (! enabled)
            return;

        if (heldNotes.empty())
        {
            stopActive (engine);
            currentStep = -1;
            phase = 0.0;
            return;
        }

        const int steps = juce::jlimit (1, 16, settings.steps);
        const double phase01 = phase - std::floor (phase);
        const double scaled = phase01 * (double) steps;
        const int step = juce::jlimit (0, steps - 1, (int) std::floor (scaled));
        const double stepPhase = scaled - std::floor (scaled);
        const bool shouldGateOpen = stepPhase <= (double) settings.gate;

        if (step != currentStep)
        {
            currentStep = step;
            stopActive (engine);
            if (shouldGateOpen)
                startStep (engine, step);
        }
        else if (shouldGateOpen && ! gateOpen)
        {
            startStep (engine, step);
        }
        else if (! shouldGateOpen && gateOpen)
        {
            stopActive (engine);
        }

        const double cyclesPerSecond = settings.sync
            ? (RenderContext::sanitiseBpm (context.bpm) / 240.0) * (double) settings.rate
            : (double) settings.rate;
        phase += cyclesPerSecond * context.secondsPerBlock();
        phase -= std::floor (phase);
    }
}
