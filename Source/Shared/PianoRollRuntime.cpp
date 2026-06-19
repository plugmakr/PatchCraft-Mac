#include "PianoRollRuntime.h"

#include <cmath>
#include <map>

namespace patchcraft
{
    bool PianoRollRuntime::isPianoRollBlock (const DspBlock& block)
    {
        const auto type = block.type.trim().toLowerCase().removeCharacters (" _-");
        return type == "pianoroll" || type == "pianorollclip" || type == "midiclip";
    }

    float PianoRollRuntime::valueFor (const DspBlock& block, const juce::String& key, float fallback)
    {
        if (const auto found = block.values.find (key); found != block.values.end())
            return found->second;
        return fallback;
    }

    juce::String PianoRollRuntime::encodeNotes (const std::vector<Note>& notes)
    {
        juce::StringArray parts;
        for (const auto& note : notes)
        {
            parts.add (juce::String (note.startStep)
                       + "," + juce::String (note.lengthSteps)
                       + "," + juce::String (note.pitch)
                       + "," + juce::String (juce::jlimit (0.0f, 1.0f, note.velocity), 3));
        }
        return parts.joinIntoString (";");
    }

    std::vector<PianoRollRuntime::Note> PianoRollRuntime::decodeNotes (const juce::String& encoded)
    {
        std::vector<Note> result;
        if (encoded.trim().isEmpty())
            return result;

        juce::StringArray entries;
        entries.addTokens (encoded, ";", "");
        for (auto& entry : entries)
        {
            const auto trimmed = entry.trim();
            if (trimmed.isEmpty())
                continue;

            juce::StringArray fields;
            fields.addTokens (trimmed, ",", "");
            if (fields.size() < 3)
                continue;

            Note note;
            note.startStep = juce::jmax (0, fields[0].getIntValue());
            note.lengthSteps = juce::jmax (1, fields[1].getIntValue());
            note.pitch = juce::jlimit (0, 127, fields[2].getIntValue());
            note.velocity = fields.size() >= 4
                ? juce::jlimit (0.0f, 1.0f, fields[3].getFloatValue())
                : 0.8f;
            result.push_back (note);
        }
        return result;
    }

    std::vector<PianoRollRuntime::Note> PianoRollRuntime::notesFromMidiFile (const juce::MidiFile& midiFile,
                                                                             int steps,
                                                                             int defaultLengthSteps)
    {
        steps = juce::jlimit (1, 256, steps);
        defaultLengthSteps = juce::jmax (1, defaultLengthSteps);

        struct TimedNote { double start = 0.0; double end = 0.0; int pitch = 60; float velocity = 0.8f; };
        std::vector<TimedNote> timed;
        std::map<std::pair<int, int>, std::vector<std::pair<double, float>>> activeOns;

        for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
        {
            const auto* track = midiFile.getTrack (trackIndex);
            if (track == nullptr)
                continue;

            for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
            {
                const auto message = track->getEventPointer (eventIndex)->message;
                const double time = message.getTimeStamp();
                const int channel = message.getChannel() > 0 ? message.getChannel() - 1 : 0;

                if (message.isNoteOn())
                {
                    activeOns[{ channel, message.getNoteNumber() }].push_back ({ time, message.getFloatVelocity() });
                }
                else if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0))
                {
                    const auto key = std::make_pair (channel, message.getNoteNumber());
                    auto found = activeOns.find (key);
                    if (found == activeOns.end() || found->second.empty())
                        continue;

                    const auto on = found->second.back();
                    found->second.pop_back();
                    TimedNote note;
                    note.start = on.first;
                    note.end = juce::jmax (on.first + 0.001, time);
                    note.pitch = juce::jlimit (0, 127, message.getNoteNumber());
                    note.velocity = juce::jlimit (0.05f, 1.0f, on.second);
                    timed.push_back (note);
                }
            }
        }

        for (auto& entry : activeOns)
        {
            auto& stack = entry.second;
            while (! stack.empty())
            {
                const auto on = stack.back();
                stack.pop_back();
                TimedNote note;
                note.start = on.first;
                note.end = on.first + 1.0;
                note.pitch = juce::jlimit (0, 127, entry.first.second);
                note.velocity = juce::jlimit (0.05f, 1.0f, on.second);
                timed.push_back (note);
            }
        }

        if (timed.empty())
            return {};

        std::stable_sort (timed.begin(), timed.end(),
            [] (const TimedNote& a, const TimedNote& b) { return a.start < b.start; });

        const double firstTime = timed.front().start;
        double lastTime = firstTime + 1.0;
        for (const auto& note : timed)
            lastTime = juce::jmax (lastTime, note.end);
        const double duration = juce::jmax (0.001, lastTime - firstTime);

        std::vector<Note> result;
        result.reserve (timed.size());
        for (const auto& note : timed)
        {
            Note roll;
            roll.startStep = juce::jlimit (0, steps - 1,
                juce::roundToInt (((note.start - firstTime) / duration) * (double) (steps - 1)));
            roll.lengthSteps = juce::jmax (defaultLengthSteps,
                juce::roundToInt (((note.end - note.start) / duration) * (double) (steps - 1)));
            roll.lengthSteps = juce::jlimit (1, steps - roll.startStep, roll.lengthSteps);
            roll.pitch = note.pitch;
            roll.velocity = note.velocity;
            result.push_back (roll);
        }
        return result;
    }

    void PianoRollRuntime::bind (const DspGraph& graph)
    {
        enabled = false;
        notes.clear();
        steps = 16;
        perBeat = 4;
        rate = 1.0f;
        gate = 0.9f;
        velocityScale = 1.0f;
        sync = true;
        loop = true;

        for (const auto& block : graph.blocks)
        {
            if (! block.enabled || ! isPianoRollBlock (block))
                continue;

            steps = juce::jlimit (1, 256, juce::roundToInt (valueFor (block, "prSteps", 16.0f)));
            perBeat = juce::jlimit (1, 16, juce::roundToInt (valueFor (block, "prStepsPerBeat", 4.0f)));
            rate = juce::jlimit (0.125f, 8.0f, valueFor (block, "prRate", 1.0f));
            gate = juce::jlimit (0.05f, 1.0f, valueFor (block, "prGate", 0.9f));
            velocityScale = juce::jlimit (0.05f, 1.0f, valueFor (block, "prVelocity", 1.0f));
            sync = valueFor (block, "prSync", 1.0f) >= 0.5f;
            loop = valueFor (block, "prLoop", 1.0f) >= 0.5f;

            const auto found = block.metadata.find ("notes");
            if (found != block.metadata.end())
                notes = decodeNotes (found->second);

            enabled = true;
            break;
        }

        reset();
    }

    void PianoRollRuntime::reset()
    {
        sounding.clear();
        lastAbsStep = -1.0;
        playback01 = -1.0;
        wasPlaying = false;
    }

    void PianoRollRuntime::stopAll (IInstrumentEngine& engine)
    {
        for (const auto& note : sounding)
            engine.noteOff (note.pitch);
        sounding.clear();
    }

    void PianoRollRuntime::allNotesOff (IInstrumentEngine& engine)
    {
        stopAll (engine);
        reset();
    }

    void PianoRollRuntime::process (IInstrumentEngine& engine, const RenderContext& context)
    {
        if (! enabled || notes.empty() || steps <= 0)
        {
            if (! sounding.empty())
                stopAll (engine);
            playback01 = -1.0;
            wasPlaying = context.isPlaying;
            return;
        }

        if (! context.isPlaying)
        {
            if (! sounding.empty())
                stopAll (engine);
            playback01 = -1.0;
            lastAbsStep = -1.0;
            wasPlaying = false;
            return;
        }

        // Convert host position to an absolute, monotonic step counter.
        const double stepsPerBeat = juce::jmax (1.0, (double) perBeat) / juce::jmax (0.125, (double) rate);
        const double absStep = juce::jmax (0.0, context.ppqPosition) * stepsPerBeat;
        const double blockSteps = juce::jmax (0.0, context.beatsPerBlock() * stepsPerBeat);

        const double loopLength = (double) steps;
        playback01 = loopLength > 0.0
            ? juce::jlimit (0.0, 0.999999, std::fmod (absStep, loopLength) / loopLength)
            : -1.0;

        // Detect transport (re)start or backward jump: re-arm cleanly.
        const bool jumped = ! wasPlaying
                         || lastAbsStep < 0.0
                         || absStep + 1.0e-6 < lastAbsStep
                         || absStep - lastAbsStep > blockSteps * 4.0 + 1.0;
        if (jumped)
        {
            stopAll (engine);
            lastAbsStep = absStep;
        }
        wasPlaying = true;

        const double windowStart = lastAbsStep;
        const double windowEnd = absStep + blockSteps;

        // Close any sounding notes whose end falls within this window.
        for (int i = (int) sounding.size(); --i >= 0;)
        {
            if (sounding[(size_t) i].endAbsStep <= windowEnd)
            {
                engine.noteOff (sounding[(size_t) i].pitch);
                sounding.erase (sounding.begin() + i);
            }
        }

        // Trigger any note whose start boundary falls in this window. We scan
        // the integer step boundaries in the half-open range [windowStart,
        // windowEnd) so a note at the exact transport/loop start still fires
        // and seam boundaries are never double-triggered.
        constexpr double eps = 1.0e-9;
        for (long boundary = (long) std::ceil (windowStart - eps);
             (double) boundary <= windowEnd - eps;
             ++boundary)
        {
            if (boundary < 0)
                continue;
            if (! loop && boundary >= steps)
                break;

            const int loopStep = ((int) (boundary % steps) + steps) % steps;
            for (const auto& note : notes)
            {
                if (note.startStep != loopStep)
                    continue;

                const float velocity = juce::jlimit (0.02f, 1.0f, note.velocity * velocityScale);
                engine.noteOn (note.pitch, velocity);

                const double lengthSteps = juce::jmax (0.1, (double) note.lengthSteps * (double) gate);
                sounding.push_back ({ note.pitch, (double) boundary + lengthSteps });
            }
        }

        lastAbsStep = absStep + blockSteps;
    }
}
