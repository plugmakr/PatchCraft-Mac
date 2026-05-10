#include "MidiPlaygroundRuntime.h"

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

        int positiveMod (int value, int modulus)
        {
            const int result = value % modulus;
            return result < 0 ? result + modulus : result;
        }

        bool scaleContainsPitchClass (int scaleType, int pitchClass)
        {
            if (scaleType <= 0)
                return true;

            static constexpr std::array<std::array<int, 7>, 9> scales {{
                {{ 0, 2, 4, 5, 7, 9, 11 }},  // major
                {{ 0, 2, 3, 5, 7, 8, 10 }},  // natural minor
                {{ 0, 2, 3, 5, 7, 9, 10 }},  // dorian
                {{ 0, 1, 3, 5, 7, 8, 10 }},  // phrygian
                {{ 0, 2, 4, 6, 7, 9, 11 }},  // lydian
                {{ 0, 2, 4, 5, 7, 9, 10 }},  // mixolydian
                {{ 0, 2, 3, 5, 7, 8, 11 }},  // harmonic minor
                {{ 0, 2, 4, 7, 9, 12, 12 }}, // major pentatonic
                {{ 0, 3, 5, 6, 7, 10, 12 }}  // blues
            }};

            const auto& scale = scales[(size_t) juce::jlimit (1, 9, scaleType) - 1];
            for (const auto interval : scale)
            {
                if (positiveMod (interval, 12) == pitchClass)
                    return true;
            }
            return false;
        }
    }

    bool MidiPlaygroundRuntime::isMidiPlaygroundBlock (const DspBlock& block)
    {
        const auto type = block.type.trim().toLowerCase();
        return type == "arp" || type == "arpeggiator" || type == "arpsequencer"
            || type == "arpstepsequencer" || type == "arp step sequencer"
            || type == "midiplayground" || type == "midi playground"
            || type == "phrasegenerator" || type == "phrase generator"
            || type == "chordsequencer" || type == "chord sequencer"
            || type == "midigenerator" || type == "midi generator"
            || type == "drummachine" || type == "drum machine"
            || type == "drumpattern" || type == "drum pattern"
            || type == "stepsequencer" || type == "step sequencer";
    }

    float MidiPlaygroundRuntime::valueForKey (const DspBlock& block,
                                              const juce::String& key,
                                              float fallback)
    {
        if (auto it = block.values.find (key); it != block.values.end())
            return it->second;
        return fallback;
    }

    int MidiPlaygroundRuntime::drumIndex (int track, int step)
    {
        track = juce::jlimit (0, kMaxDrumTracks - 1, track);
        step = juce::jlimit (0, kMaxDrumSteps - 1, step);
        return track * kMaxDrumSteps + step;
    }

    int MidiPlaygroundRuntime::drumPatternIndex (int pattern, int track, int step)
    {
        pattern = juce::jlimit (0, kMaxDrumPatterns - 1, pattern);
        track = juce::jlimit (0, kMaxDrumTracks - 1, track);
        step = juce::jlimit (0, kMaxDrumSteps - 1, step);
        return pattern * kMaxDrumTracks * kMaxDrumSteps + track * kMaxDrumSteps + step;
    }

    int MidiPlaygroundRuntime::bankStepIndex (int bank, int step)
    {
        bank = juce::jlimit (0, kMaxPhraseBanks - 1, bank);
        step = juce::jlimit (0, kMaxSteps - 1, step);
        return bank * kMaxSteps + step;
    }

    uint32_t MidiPlaygroundRuntime::hash (uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    void MidiPlaygroundRuntime::bind (const DspGraph& graph)
    {
        reset();
        enabled = false;
        settings = {};
        settings.notes = kDefaultNotes;
        settings.velocities.fill (1.0f);
        settings.gates.fill (0.55f);
        settings.active.fill (1.0f);
        settings.probabilities.fill (1.0f);
        settings.sampleSlices.fill (-1.0f);
        settings.bankHasData.fill (0.0f);
        settings.bankNotes.fill (0.0f);
        settings.bankVelocities.fill (1.0f);
        settings.bankGates.fill (0.55f);
        settings.bankActive.fill (1.0f);
        settings.bankProbabilities.fill (1.0f);
        settings.bankSampleSlices.fill (-1.0f);
        settings.drumNotes.fill (36);
        settings.drumChain.fill (0);
        settings.drumActive.fill (0.0f);
        settings.drumVelocities.fill (1.0f);
        settings.drumGates.fill (0.40f);
        settings.drumProbabilities.fill (1.0f);

        for (const auto& block : graph.blocks)
        {
            if (! block.enabled || ! isMidiPlaygroundBlock (block))
                continue;

            settings.drumMachine = block.type.containsIgnoreCase ("drum");
            settings.rate = juce::jlimit (0.0625f, 16.0f, valueForKey (block, "rate", 1.0f));
            settings.sync = valueForKey (block, "sync", 1.0f) >= 0.5f;
            if (settings.drumMachine)
            {
                settings.drumTracks = juce::jlimit (1, kMaxDrumTracks, juce::roundToInt (valueForKey (block, "dmTracks", 8.0f)));
                settings.drumSteps = juce::jlimit (1, kMaxDrumSteps, juce::roundToInt (valueForKey (block, "dmSteps", 16.0f)));
                settings.drumPattern = juce::jlimit (0, 7, juce::roundToInt (valueForKey (block, "dmPattern", 0.0f)));
                settings.drumTransport = valueForKey (block, "dmTransport", 1.0f) >= 0.5f;
                settings.drumSongMode = valueForKey (block, "dmSongMode", 0.0f) >= 0.5f;
                settings.drumChainLength = juce::jlimit (1, kMaxDrumPatterns, juce::roundToInt (valueForKey (block, "dmChainLength", 1.0f)));
                for (int index = 0; index < kMaxDrumPatterns; ++index)
                    settings.drumChain[(size_t) index] = juce::jlimit (0, kMaxDrumPatterns - 1,
                        juce::roundToInt (valueForKey (block, "dmChain" + juce::String (index), (float) index)));
                settings.swing = juce::jlimit (0.0f, 0.75f, valueForKey (block, "dmSwing", valueForKey (block, "arpSwing", 0.0f)));
                settings.probability = juce::jlimit (0.0f, 1.0f, valueForKey (block, "dmProbability", valueForKey (block, "mpProbability", 1.0f)));
                settings.seed = (uint32_t) juce::roundToInt (valueForKey (block, "dmSeed", valueForKey (block, "mpSeed", 12001.0f)));

                static constexpr std::array<int, kMaxDrumTracks> defaultNotes {{
                    36, 38, 42, 46, 41, 45, 49, 51, 37, 39, 44, 48, 50, 47, 52, 53
                }};

                for (int track = 0; track < kMaxDrumTracks; ++track)
                {
                    settings.drumNotes[(size_t) track] = juce::jlimit (0, 127,
                        juce::roundToInt (valueForKey (block, "dmTrack" + juce::String (track) + "Note",
                                                       (float) defaultNotes[(size_t) track])));

                    for (int pattern = 0; pattern < kMaxDrumPatterns; ++pattern)
                        for (int step = 0; step < kMaxDrumSteps; ++step)
                        {
                            const auto patternPrefix = "dmP" + juce::String (pattern)
                                                    + "T" + juce::String (track)
                                                    + "S" + juce::String (step);
                            const auto directPrefix = "dmT" + juce::String (track) + "S" + juce::String (step);
                            const auto index = (size_t) drumPatternIndex (pattern, track, step);
                            settings.drumActive[index] = valueForKey (block, patternPrefix + "On",
                                valueForKey (block, directPrefix + "On", 0.0f)) >= 0.5f ? 1.0f : 0.0f;
                            settings.drumVelocities[index] = juce::jlimit (0.0f, 1.0f, valueForKey (block, patternPrefix + "Vel",
                                valueForKey (block, directPrefix + "Vel", 0.88f)));
                            settings.drumGates[index] = juce::jlimit (0.05f, 1.0f, valueForKey (block, patternPrefix + "Gate",
                                valueForKey (block, directPrefix + "Gate", 0.36f)));
                            settings.drumProbabilities[index] = juce::jlimit (0.0f, 1.0f, valueForKey (block, patternPrefix + "Prob",
                                valueForKey (block, directPrefix + "Prob", 1.0f)));
                        }
                }

                enabled = true;
                break;
            }

            settings.steps = juce::jlimit (1, kMaxSteps, juce::roundToInt (valueForKey (block, "arpSteps", 8.0f)));
            settings.activeBank = juce::jlimit (0, kMaxPhraseBanks - 1, juce::roundToInt (valueForKey (block, "mpActiveBank", 0.0f)));
            settings.polymeterSteps = juce::jlimit (0, kMaxSteps, juce::roundToInt (valueForKey (block, "mpPolymeterSteps", 0.0f)));
            settings.pattern = juce::jlimit (0, 7, juce::roundToInt (valueForKey (block, "arpPattern", 0.0f)));
            settings.gate = juce::jlimit (0.05f, 1.0f, valueForKey (block, "arpGate", 0.55f));
            settings.octaves = juce::jlimit (1, 4, juce::roundToInt (valueForKey (block, "arpOctaves", 2.0f)));
            settings.swing = juce::jlimit (0.0f, 0.75f, valueForKey (block, "arpSwing", valueForKey (block, "mpSwing", 0.0f)));
            settings.probability = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpProbability", 1.0f));
            settings.humanize = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpHumanize", 0.0f));
            settings.mutation = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpMutation", 0.0f));
            settings.velocityCurve = juce::jlimit (-1.0f, 1.0f, valueForKey (block, "mpVelocityCurve", 0.0f));
            settings.octaveFold = valueForKey (block, "mpOctaveFold", 0.0f) >= 0.5f;
            settings.ratchet = juce::jlimit (1, 4, juce::roundToInt (valueForKey (block, "mpRatchet", 1.0f)));
            settings.strum = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpStrum", 0.0f));
            settings.flam = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpFlam", 0.0f));
            settings.echoRepeats = juce::jlimit (0, 4, juce::roundToInt (valueForKey (block, "mpEchoRepeats", 0.0f)));
            settings.echoDelay = juce::jlimit (0.01f, 0.95f, valueForKey (block, "mpEchoDelay", 0.18f));
            settings.echoDecay = juce::jlimit (0.05f, 1.0f, valueForKey (block, "mpEchoDecay", 0.55f));
            settings.patternMorph = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpPatternMorph", 0.0f));
            settings.euclideanPulses = juce::jlimit (0, kMaxSteps, juce::roundToInt (valueForKey (block, "mpEuclideanPulses", 0.0f)));
            settings.euclideanRotate = positiveMod (juce::roundToInt (valueForKey (block, "mpEuclideanRotate", 0.0f)), kMaxSteps);
            settings.keySwitchEnabled = valueForKey (block, "mpKeySwitchEnabled", 0.0f) >= 0.5f;
            settings.keySwitchBase = juce::jlimit (0, 124, juce::roundToInt (valueForKey (block, "mpKeySwitchBase", 24.0f)));
            settings.scaleRoot = positiveMod (juce::roundToInt (valueForKey (block, "mpScaleRoot", valueForKey (block, "scaleRoot", 0.0f))), 12);
            settings.scaleType = juce::jlimit (0, 9, juce::roundToInt (valueForKey (block, "mpScaleType", valueForKey (block, "scaleType", 0.0f))));
            settings.chordMode = juce::jlimit (0, 32, juce::roundToInt (valueForKey (block, "mpChordMode", valueForKey (block, "chordMode", 0.0f))));
            settings.chordSize = juce::jlimit (1, kMaxChordNotes, juce::roundToInt (valueForKey (block, "mpChordSize", valueForKey (block, "chordSize", settings.chordMode == 0 ? 1.0f : 3.0f))));
            settings.chordSpread = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpChordSpread", valueForKey (block, "chordSpread", 0.0f)));
            settings.latch = valueForKey (block, "mpLatch", 0.0f) >= 0.5f;
            settings.sampleControl = valueForKey (block, "mpSampleControl", 0.0f) >= 0.5f;
            settings.sampleSliceCount = juce::jlimit (1, 64, juce::roundToInt (valueForKey (block, "sampleSliceCount", valueForKey (block, "mpSampleSliceCount", 1.0f))));
            settings.sampleStart = juce::jlimit (0.0f, 1.0f, valueForKey (block, "sampleStart", valueForKey (block, "mpSampleStart", 0.0f)));
            settings.sampleLength = juce::jlimit (0.01f, 1.0f, valueForKey (block, "sampleLength", valueForKey (block, "mpSampleLength", 1.0f)));
            settings.samplePitch = juce::jlimit (-48.0f, 48.0f, valueForKey (block, "samplePitch", valueForKey (block, "mpSamplePitch", 0.0f)));
            settings.seed = (uint32_t) juce::roundToInt (valueForKey (block, "mpSeed", 12001.0f));

            for (int step = 0; step < kMaxSteps; ++step)
            {
                settings.notes[(size_t) step] = valueForKey (block, "arpNote" + juce::String (step), kDefaultNotes[(size_t) step]);
                settings.velocities[(size_t) step] = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpVelocity" + juce::String (step), 1.0f));
                settings.gates[(size_t) step] = juce::jlimit (0.05f, 1.0f, valueForKey (block, "mpGate" + juce::String (step), settings.gate));
                settings.active[(size_t) step] = valueForKey (block, "mpStep" + juce::String (step) + "On", 1.0f) >= 0.5f ? 1.0f : 0.0f;
                settings.probabilities[(size_t) step] = juce::jlimit (0.0f, 1.0f, valueForKey (block, "mpStepProb" + juce::String (step), 1.0f));
                settings.sampleSlices[(size_t) step] = valueForKey (block, "mpSampleSlice" + juce::String (step), -1.0f);
            }

            for (int bank = 0; bank < kMaxPhraseBanks; ++bank)
            {
                const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
                bool bankHasData = false;
                for (const auto& value : block.values)
                {
                    if (value.first.startsWith (prefix))
                    {
                        bankHasData = true;
                        break;
                    }
                }

                settings.bankHasData[(size_t) bank] = bankHasData ? 1.0f : 0.0f;
                for (int step = 0; step < kMaxSteps; ++step)
                {
                    const auto suffix = juce::String (step);
                    const auto index = (size_t) bankStepIndex (bank, step);
                    settings.bankNotes[index] = valueForKey (block, prefix + "arpNote" + suffix, settings.notes[(size_t) step]);
                    settings.bankVelocities[index] = juce::jlimit (0.0f, 1.0f,
                        valueForKey (block, prefix + "mpVelocity" + suffix, settings.velocities[(size_t) step]));
                    settings.bankGates[index] = juce::jlimit (0.05f, 1.0f,
                        valueForKey (block, prefix + "mpGate" + suffix, settings.gates[(size_t) step]));
                    settings.bankActive[index] = valueForKey (block, prefix + "mpStep" + suffix + "On",
                        settings.active[(size_t) step]) >= 0.5f ? 1.0f : 0.0f;
                    settings.bankProbabilities[index] = juce::jlimit (0.0f, 1.0f,
                        valueForKey (block, prefix + "mpStepProb" + suffix, settings.probabilities[(size_t) step]));
                    settings.bankSampleSlices[index] = valueForKey (block, prefix + "mpSampleSlice" + suffix,
                        settings.sampleSlices[(size_t) step]);
                }
            }

            loadRuntimeBank (settings.activeBank);

            enabled = true;
            break;
        }
    }

    void MidiPlaygroundRuntime::loadRuntimeBank (int bank)
    {
        bank = juce::jlimit (0, kMaxPhraseBanks - 1, bank);
        if (settings.bankHasData[(size_t) bank] < 0.5f)
            return;

        settings.activeBank = bank;
        for (int step = 0; step < kMaxSteps; ++step)
        {
            const auto index = (size_t) bankStepIndex (bank, step);
            settings.notes[(size_t) step] = settings.bankNotes[index];
            settings.velocities[(size_t) step] = settings.bankVelocities[index];
            settings.gates[(size_t) step] = settings.bankGates[index];
            settings.active[(size_t) step] = settings.bankActive[index];
            settings.probabilities[(size_t) step] = settings.bankProbabilities[index];
            settings.sampleSlices[(size_t) step] = settings.bankSampleSlices[index];
        }
    }

    int MidiPlaygroundRuntime::activeStepCount() const
    {
        return juce::jlimit (1, kMaxSteps,
            settings.polymeterSteps > 0 ? settings.polymeterSteps : settings.steps);
    }

    int MidiPlaygroundRuntime::activeDrumPattern() const
    {
        if (! settings.drumSongMode)
            return juce::jlimit (0, kMaxDrumPatterns - 1, settings.drumPattern);

        const int chainIndex = (int) (cycleCounter % (uint32_t) juce::jmax (1, settings.drumChainLength));
        return juce::jlimit (0, kMaxDrumPatterns - 1, settings.drumChain[(size_t) chainIndex]);
    }

    void MidiPlaygroundRuntime::reset()
    {
        heldNotes.clear();
        heldVelocities.fill (0.0f);
        activeNotes.fill (-1);
        activeNoteCount = 0;
        activeVelocity = 0.0f;
        phase = 0.0;
        currentStep = -1;
        currentRatchetSlot = -1;
        gateOpen = false;
        cycleCounter = 0;
        pendingNotes.fill (-1);
        pendingPhases.fill (0.0);
        pendingVelocities.fill (0.0f);
        pendingNoteCount = 0;
        pendingNoteIndex = 0;
        pendingStep = -1;
        activeDrumNotes.fill (-1);
        activeDrumGateEnds.fill (0.0);
    }

    void MidiPlaygroundRuntime::allNotesOff (IInstrumentEngine& engine)
    {
        stopActive (engine);
        stopActiveDrums (engine);
        reset();
    }

    bool MidiPlaygroundRuntime::handleNoteOn (IInstrumentEngine&, int midiNote, float velocity)
    {
        if (! enabled)
            return false;
        if (settings.drumMachine)
            return false;

        const int note = juce::jlimit (0, 127, midiNote);
        if (settings.keySwitchEnabled
            && note >= settings.keySwitchBase
            && note < settings.keySwitchBase + kMaxPhraseBanks)
        {
            loadRuntimeBank (note - settings.keySwitchBase);
            phase = 0.0;
            currentStep = -1;
            currentRatchetSlot = -1;
            gateOpen = false;
            pendingStep = -1;
            return true;
        }

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
            cycleCounter = 0;
        }

        return true;
    }

    bool MidiPlaygroundRuntime::handleNoteOff (IInstrumentEngine& engine, int midiNote)
    {
        if (! enabled)
            return false;
        if (settings.drumMachine)
            return false;

        if (settings.latch)
            return true;

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

        return true;
    }

    int MidiPlaygroundRuntime::sequenceIndexForStep (int step) const
    {
        const int steps = activeStepCount();
        step = juce::jlimit (0, steps - 1, step);

        int index = step;
        if (settings.pattern == 1)
            index = steps - 1 - step;
        else if (settings.pattern == 2)
            index = step <= steps / 2 ? step : juce::jmax (0, steps - 1 - step);
        else if (settings.pattern == 3)
            index = (step % 4) * 2;
        else if (settings.pattern == 4)
            index = (step * 2 + 1) % steps;
        else if (settings.pattern == 5)
            index = (step * 2) % steps;
        else if (settings.pattern == 6)
            index = (step * 5) % steps;
        else if (settings.pattern == 7)
            index = (int) (hash (settings.seed ^ (uint32_t) step ^ (cycleCounter * 7919u)) % (uint32_t) steps);

        return juce::jlimit (0, kMaxSteps - 1, index);
    }

    int MidiPlaygroundRuntime::baseNoteForSequenceIndex (int index) const
    {
        if (heldNotes.empty())
            return -1;

        index = juce::jlimit (0, kMaxSteps - 1, index);
        float offsetValue = settings.notes[(size_t) index];
        if (settings.patternMorph > 0.0f)
        {
            const int nextBank = (settings.activeBank + 1) % kMaxPhraseBanks;
            if (settings.bankHasData[(size_t) nextBank] >= 0.5f)
            {
                const auto bankIndex = (size_t) bankStepIndex (nextBank, index);
                offsetValue += (settings.bankNotes[bankIndex] - offsetValue) * settings.patternMorph;
            }
        }

        const int offset = juce::roundToInt (offsetValue);

        int note = 0;
        const int foldRoot = heldNotes.front();
        if (heldNotes.size() == 1)
        {
            note = heldNotes.front() + offset;
        }
        else
        {
            const int noteIndex = juce::jlimit (0, (int) heldNotes.size() - 1, index % (int) heldNotes.size());
            const int octave = (index / juce::jmax (1, (int) heldNotes.size())) % juce::jmax (1, settings.octaves);
            note = heldNotes[(size_t) noteIndex] + octave * 12 + offset;
        }

        if (settings.mutation > 0.0f)
        {
            static constexpr std::array<int, 8> mutationOffsets { -12, -7, -5, -2, 2, 5, 7, 12 };
            const auto h = hash (settings.seed ^ (uint32_t) index * 0x9e3779b9u ^ cycleCounter * 0x85ebca6bu);
            const float chance = (float) (h & 0x00ffffffu) / (float) 0x01000000u;
            if (chance < settings.mutation)
                note += mutationOffsets[(size_t) ((h >> 24) % mutationOffsets.size())];
        }

        if (settings.octaveFold)
        {
            while (note < foldRoot - 12)
                note += 12;
            while (note > foldRoot + 12)
                note -= 12;
        }

        return quantizeToScale (juce::jlimit (0, 127, note));
    }

    int MidiPlaygroundRuntime::quantizeToScale (int midiNote) const
    {
        if (settings.scaleType <= 0)
            return juce::jlimit (0, 127, midiNote);

        const int root = positiveMod (settings.scaleRoot, 12);
        const auto inScale = [&] (int note)
        {
            return scaleContainsPitchClass (settings.scaleType, positiveMod (note - root, 12));
        };

        if (inScale (midiNote))
            return juce::jlimit (0, 127, midiNote);

        int best = midiNote;
        int bestDistance = 128;
        for (int delta = -12; delta <= 12; ++delta)
        {
            const int candidate = midiNote + delta;
            if (candidate < 0 || candidate > 127 || ! inScale (candidate))
                continue;

            const int distance = std::abs (delta);
            if (distance < bestDistance || (distance == bestDistance && candidate > best))
            {
                best = candidate;
                bestDistance = distance;
            }
        }

        return juce::jlimit (0, 127, best);
    }

    int MidiPlaygroundRuntime::scaleDegreeToSemitone (int degree) const
    {
        if (settings.scaleType <= 0)
            return degree;

        static constexpr std::array<std::array<int, 7>, 9> scales {{
            {{ 0, 2, 4, 5, 7, 9, 11 }},
            {{ 0, 2, 3, 5, 7, 8, 10 }},
            {{ 0, 2, 3, 5, 7, 9, 10 }},
            {{ 0, 1, 3, 5, 7, 8, 10 }},
            {{ 0, 2, 4, 6, 7, 9, 11 }},
            {{ 0, 2, 4, 5, 7, 9, 10 }},
            {{ 0, 2, 3, 5, 7, 8, 11 }},
            {{ 0, 2, 4, 7, 9, 12, 12 }},
            {{ 0, 3, 5, 6, 7, 10, 12 }}
        }};

        const auto& scale = scales[(size_t) juce::jlimit (1, 9, settings.scaleType) - 1];
        const int octave = degree >= 0 ? degree / 7 : -((std::abs (degree) + 6) / 7);
        const int index = positiveMod (degree, 7);
        return octave * 12 + scale[(size_t) index];
    }

    bool MidiPlaygroundRuntime::stepIsEnabled (int step) const
    {
        return settings.active[(size_t) juce::jlimit (0, kMaxSteps - 1, step)] >= 0.5f
            && stepPassesEuclideanMask (step);
    }

    bool MidiPlaygroundRuntime::stepPassesEuclideanMask (int step) const
    {
        const int steps = activeStepCount();
        const int pulses = juce::jlimit (0, steps, settings.euclideanPulses);
        if (pulses <= 0 || pulses >= steps)
            return true;

        const int rotated = positiveMod (step + settings.euclideanRotate, steps);
        return (rotated * pulses) % steps < pulses;
    }

    bool MidiPlaygroundRuntime::stepPassesProbability (int step) const
    {
        const float stepProbability = juce::jlimit (0.0f, 1.0f,
            settings.probability * settings.probabilities[(size_t) juce::jlimit (0, kMaxSteps - 1, step)]);
        if (stepProbability >= 0.999f)
            return true;
        if (stepProbability <= 0.001f)
            return false;

        const auto h = hash (settings.seed ^ (uint32_t) step * 0x9e3779b9u ^ cycleCounter * 0x85ebca6bu);
        const float value = (float) (h & 0x00ffffffu) / (float) 0x01000000u;
        return value <= stepProbability;
    }

    float MidiPlaygroundRuntime::velocityForStep (int step) const
    {
        float velocity = activeVelocity > 0.0f ? activeVelocity : 0.75f;
        for (const auto note : heldNotes)
            velocity = juce::jmax (velocity, heldVelocities[(size_t) juce::jlimit (0, 127, note)]);

        step = juce::jlimit (0, kMaxSteps - 1, step);
        float stepVelocity = settings.velocities[(size_t) step];
        if (settings.patternMorph > 0.0f)
        {
            const int nextBank = (settings.activeBank + 1) % kMaxPhraseBanks;
            if (settings.bankHasData[(size_t) nextBank] >= 0.5f)
            {
                const auto bankIndex = (size_t) bankStepIndex (nextBank, step);
                stepVelocity += (settings.bankVelocities[bankIndex] - stepVelocity) * settings.patternMorph;
            }
        }

        velocity *= stepVelocity;

        if (settings.velocityCurve > 0.001f)
            velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.0f + settings.velocityCurve * 2.0f);
        else if (settings.velocityCurve < -0.001f)
            velocity = 1.0f - std::pow (1.0f - juce::jlimit (0.0f, 1.0f, velocity), 1.0f + -settings.velocityCurve * 2.0f);

        if (settings.humanize > 0.0f)
        {
            const auto h = hash (settings.seed ^ (uint32_t) step * 2654435761u ^ cycleCounter * 2246822519u);
            const float bipolar = ((float) (h & 0x00ffffffu) / (float) 0x00800000u) - 1.0f;
            velocity *= 1.0f + bipolar * settings.humanize * 0.25f;
        }

        return juce::jlimit (0.01f, 1.0f, velocity);
    }

    void MidiPlaygroundRuntime::buildStepNotes (int step, std::array<int, kMaxChordNotes>& notes, int& count) const
    {
        notes.fill (-1);
        count = 0;

        if (! stepIsEnabled (step) || ! stepPassesProbability (step))
            return;

        const int root = baseNoteForSequenceIndex (sequenceIndexForStep (step));
        if (root < 0)
            return;

        const bool exactSemitoneChord = settings.chordMode >= 7;
        auto addNote = [&] (int note)
        {
            note = juce::jlimit (0, 127, exactSemitoneChord ? note : quantizeToScale (note));
            for (int i = 0; i < count; ++i)
                if (notes[(size_t) i] == note)
                    return;
            if (count < kMaxChordNotes)
                notes[(size_t) count++] = note;
        };

        if (settings.chordMode <= 0 || settings.chordSize <= 1)
        {
            addNote (root);
            return;
        }

        std::array<int, kMaxChordNotes> intervals {};
        int intervalCount = 0;

        auto addDegree = [&] (int degree)
        {
            if (intervalCount < kMaxChordNotes)
                intervals[(size_t) intervalCount++] = scaleDegreeToSemitone (degree);
        };
        auto addSemitone = [&] (int semitone)
        {
            if (intervalCount < kMaxChordNotes)
                intervals[(size_t) intervalCount++] = semitone;
        };

        if (settings.chordMode == 1)
        {
            addDegree (0); addDegree (2); addDegree (4);
        }
        else if (settings.chordMode == 2)
        {
            addDegree (0); addDegree (2); addDegree (4); addDegree (6);
        }
        else if (settings.chordMode == 3)
        {
            addSemitone (0); addSemitone (7); addSemitone (12);
        }
        else if (settings.chordMode == 4)
        {
            addSemitone (0); addSemitone (5); addSemitone (7); addSemitone (12);
        }
        else if (settings.chordMode == 5)
        {
            addSemitone (0); addSemitone (2); addSemitone (5); addSemitone (9);
        }
        else if (settings.chordMode == 6)
        {
            addDegree (0); addDegree (4); addDegree (7); addDegree (11);
        }
        else if (settings.chordMode == 7)
        {
            addSemitone (0); addSemitone (4); addSemitone (7);
        }
        else if (settings.chordMode == 8)
        {
            addSemitone (0); addSemitone (3); addSemitone (7);
        }
        else if (settings.chordMode == 9)
        {
            addSemitone (0); addSemitone (3); addSemitone (6);
        }
        else if (settings.chordMode == 10)
        {
            addSemitone (0); addSemitone (4); addSemitone (8);
        }
        else if (settings.chordMode == 11)
        {
            addSemitone (0); addSemitone (2); addSemitone (7);
        }
        else if (settings.chordMode == 12)
        {
            addSemitone (0); addSemitone (5); addSemitone (7);
        }
        else if (settings.chordMode == 13)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (9);
        }
        else if (settings.chordMode == 14)
        {
            addSemitone (0); addSemitone (3); addSemitone (7); addSemitone (9);
        }
        else if (settings.chordMode == 15)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (10);
        }
        else if (settings.chordMode == 16)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (11);
        }
        else if (settings.chordMode == 17)
        {
            addSemitone (0); addSemitone (3); addSemitone (7); addSemitone (10);
        }
        else if (settings.chordMode == 18)
        {
            addSemitone (0); addSemitone (3); addSemitone (6); addSemitone (10);
        }
        else if (settings.chordMode == 19)
        {
            addSemitone (0); addSemitone (3); addSemitone (6); addSemitone (9);
        }
        else if (settings.chordMode == 20)
        {
            addSemitone (0); addSemitone (3); addSemitone (7); addSemitone (11);
        }
        else if (settings.chordMode == 21)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (14);
        }
        else if (settings.chordMode == 22)
        {
            addSemitone (0); addSemitone (3); addSemitone (7); addSemitone (14);
        }
        else if (settings.chordMode == 23)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (10); addSemitone (14);
        }
        else if (settings.chordMode == 24)
        {
            addSemitone (0); addSemitone (3); addSemitone (7); addSemitone (10); addSemitone (14);
        }
        else if (settings.chordMode == 25)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (10); addSemitone (14); addSemitone (17);
        }
        else if (settings.chordMode == 26)
        {
            addSemitone (0); addSemitone (4); addSemitone (7); addSemitone (10); addSemitone (14); addSemitone (21);
        }
        else if (settings.chordMode == 27)
        {
            addSemitone (0); addSemitone (5); addSemitone (7); addSemitone (10);
        }
        else if (settings.chordMode == 28)
        {
            addSemitone (0); addSemitone (5); addSemitone (10); addSemitone (15);
        }
        else
        {
            addSemitone (0); addSemitone (7); addSemitone (12);
        }

        const int wanted = juce::jlimit (1, kMaxChordNotes, juce::jmin (settings.chordSize, intervalCount));
        for (int i = 0; i < wanted; ++i)
        {
            const int spreadOctave = settings.chordSpread > 0.5f && i >= 2 ? 12 : 0;
            addNote (root + intervals[(size_t) i] + spreadOctave);
        }
    }

    void MidiPlaygroundRuntime::applySampleControl (IInstrumentEngine& engine, int step) const
    {
        if (! settings.sampleControl)
            return;

        const int sliceCount = juce::jlimit (1, 64, settings.sampleSliceCount);
        step = juce::jlimit (0, kMaxSteps - 1, step);
        auto storedSlice = settings.sampleSlices[(size_t) step];
        if (settings.patternMorph > 0.5f)
        {
            const int nextBank = (settings.activeBank + 1) % kMaxPhraseBanks;
            if (settings.bankHasData[(size_t) nextBank] >= 0.5f)
                storedSlice = settings.bankSampleSlices[(size_t) bankStepIndex (nextBank, step)];
        }
        const int slice = storedSlice >= 0.0f
            ? juce::jlimit (0, sliceCount - 1, juce::roundToInt (storedSlice))
            : (step % sliceCount);

        engine.setParameter ("sampleSliceCount", (float) sliceCount);
        engine.setParameter ("sampleSlice", (float) slice);
        engine.setParameter ("sampleStart", settings.sampleStart);
        engine.setParameter ("sampleLength", settings.sampleLength);
        engine.setParameter ("samplePitch", settings.samplePitch);
    }

    void MidiPlaygroundRuntime::stopActive (IInstrumentEngine& engine)
    {
        for (int i = 0; i < activeNoteCount; ++i)
        {
            const int note = activeNotes[(size_t) i];
            if (note >= 0)
                engine.noteOff (note);
        }

        activeNotes.fill (-1);
        activeNoteCount = 0;
        activeVelocity = 0.0f;
        gateOpen = false;
        pendingNotes.fill (-1);
        pendingPhases.fill (0.0);
        pendingVelocities.fill (0.0f);
        pendingNoteCount = 0;
        pendingNoteIndex = 0;
        pendingStep = -1;
    }

    void MidiPlaygroundRuntime::stopActiveDrums (IInstrumentEngine& engine)
    {
        for (auto& note : activeDrumNotes)
        {
            if (note >= 0)
                engine.noteOff (note);
            note = -1;
        }

        activeDrumGateEnds.fill (0.0);
    }

    void MidiPlaygroundRuntime::processDrumMachine (IInstrumentEngine& engine,
                                                    const RenderContext& context)
    {
        if (settings.drumTransport && ! context.isPlaying)
        {
            stopActiveDrums (engine);
            currentStep = -1;
            phase = 0.0;
            return;
        }

        const int steps = juce::jlimit (1, kMaxDrumSteps, settings.drumSteps);
        const int tracks = juce::jlimit (1, kMaxDrumTracks, settings.drumTracks);
        const double phase01 = phase - std::floor (phase);
        const double scaled = phase01 * (double) steps;
        const int step = juce::jlimit (0, steps - 1, (int) std::floor (scaled));
        const double stepPhase = scaled - std::floor (scaled);

        if (currentStep >= 0 && step < currentStep)
            ++cycleCounter;

        if (step != currentStep)
        {
            stopActiveDrums (engine);
            currentStep = step;
            const int pattern = activeDrumPattern();

            for (int track = 0; track < tracks; ++track)
            {
                const auto index = (size_t) drumPatternIndex (pattern, track, step);
                if (settings.drumActive[index] < 0.5f)
                    continue;

                const float probability = juce::jlimit (0.0f, 1.0f,
                    settings.probability * settings.drumProbabilities[index]);
                if (probability <= 0.001f)
                    continue;
                if (probability < 0.999f)
                {
                    const auto h = hash (settings.seed
                                       ^ (uint32_t) (pattern + 1) * 0x9e3779b9u
                                       ^ (uint32_t) track * 0x85ebca6bu
                                       ^ (uint32_t) step * 0xc2b2ae35u
                                       ^ cycleCounter * 7919u);
                    const float roll = (float) (h & 0x00ffffffu) / (float) 0x01000000u;
                    if (roll > probability)
                        continue;
                }

                const int note = juce::jlimit (0, 127, settings.drumNotes[(size_t) track]);
                const float velocity = juce::jlimit (0.01f, 1.0f, settings.drumVelocities[index]);
                const float gate = juce::jlimit (0.05f, 1.0f, settings.drumGates[index]);
                activeDrumNotes[(size_t) track] = note;
                activeDrumGateEnds[(size_t) track] = gate;
                engine.noteOn (note, velocity);
            }
        }
        else
        {
            for (int track = 0; track < tracks; ++track)
            {
                auto& note = activeDrumNotes[(size_t) track];
                if (note >= 0 && stepPhase >= activeDrumGateEnds[(size_t) track])
                {
                    engine.noteOff (note);
                    note = -1;
                }
            }
        }

        const double cyclesPerSecond = settings.sync
            ? (RenderContext::sanitiseBpm (context.bpm) / 240.0) * (double) settings.rate
            : (double) settings.rate;
        phase += cyclesPerSecond * context.secondsPerBlock();
        phase -= std::floor (phase);
    }

    void MidiPlaygroundRuntime::triggerPendingNotes (IInstrumentEngine& engine, int step, double stepPhase)
    {
        if (pendingStep != step)
            return;

        while (pendingNoteIndex < pendingNoteCount
               && stepPhase + 0.000001 >= pendingPhases[(size_t) pendingNoteIndex])
        {
            const int note = pendingNotes[(size_t) pendingNoteIndex++];
            const float velocity = pendingVelocities[(size_t) (pendingNoteIndex - 1)];
            if (note < 0)
                continue;

            for (int i = 0; i < activeNoteCount; ++i)
                if (activeNotes[(size_t) i] == note)
                {
                    engine.noteOff (note);
                    break;
                }

            if (activeNoteCount < kMaxChordNotes)
                activeNotes[(size_t) activeNoteCount++] = note;
            engine.noteOn (note, juce::jlimit (0.01f, 1.0f, velocity > 0.0f ? velocity : activeVelocity));
        }
    }

    void MidiPlaygroundRuntime::startStep (IInstrumentEngine& engine, int step,
                                           double stepPhase, double swingDelay, double stepGate)
    {
        std::array<int, kMaxChordNotes> notes {};
        int count = 0;
        buildStepNotes (step, notes, count);
        if (count <= 0)
            return;

        stopActive (engine);
        applySampleControl (engine, step);
        activeVelocity = velocityForStep (step);

        const double gateEnd = juce::jmin (1.0, swingDelay + stepGate);
        const double strumWindow = juce::jlimit (0.0, juce::jmax (0.0, gateEnd - stepPhase),
                                                 (double) settings.strum * juce::jmin (0.85, stepGate * 0.80));
        const double flamOffset = settings.flam > 0.0f
            ? juce::jlimit (0.0, juce::jmax (0.0, gateEnd - stepPhase),
                            0.025 + (double) settings.flam * juce::jmin (0.24, stepGate * 0.45))
            : 0.0;

        pendingNotes.fill (-1);
        pendingPhases.fill (0.0);
        pendingVelocities.fill (0.0f);
        pendingNoteCount = 0;
        pendingNoteIndex = 0;
        pendingStep = step;

        const int noteCount = juce::jlimit (0, kMaxChordNotes, count);
        for (int i = 0; i < noteCount && pendingNoteCount < kMaxChordNotes; ++i)
        {
            const double noteOffset = noteCount <= 1 || strumWindow <= 0.0
                ? 0.0 : strumWindow * (double) i / (double) (noteCount - 1);
            pendingNotes[(size_t) pendingNoteCount] = notes[(size_t) i];
            pendingPhases[(size_t) pendingNoteCount] = juce::jmin (gateEnd, stepPhase + noteOffset);
            pendingVelocities[(size_t) pendingNoteCount] = activeVelocity;
            ++pendingNoteCount;
        }

        if (settings.flam > 0.0f && noteCount > 0 && pendingNoteCount < kMaxChordNotes)
        {
            pendingNotes[(size_t) pendingNoteCount] = notes[0];
            pendingPhases[(size_t) pendingNoteCount] = juce::jmin (gateEnd, stepPhase + flamOffset);
            pendingVelocities[(size_t) pendingNoteCount] = activeVelocity * 0.82f;
            ++pendingNoteCount;
        }

        for (int repeat = 1; repeat <= settings.echoRepeats; ++repeat)
        {
            const double echoPhase = stepPhase + (double) settings.echoDelay * (double) repeat;
            if (echoPhase >= gateEnd)
                break;

            const float echoVelocity = activeVelocity * std::pow (settings.echoDecay, (float) repeat);
            for (int i = 0; i < noteCount && pendingNoteCount < kMaxChordNotes; ++i)
            {
                pendingNotes[(size_t) pendingNoteCount] = notes[(size_t) i];
                pendingPhases[(size_t) pendingNoteCount] = echoPhase;
                pendingVelocities[(size_t) pendingNoteCount] = echoVelocity;
                ++pendingNoteCount;
            }
        }

        for (int i = 1; i < pendingNoteCount; ++i)
            for (int j = i; j > 0 && pendingPhases[(size_t) j] < pendingPhases[(size_t) (j - 1)]; --j)
            {
                std::swap (pendingPhases[(size_t) j], pendingPhases[(size_t) (j - 1)]);
                std::swap (pendingNotes[(size_t) j], pendingNotes[(size_t) (j - 1)]);
                std::swap (pendingVelocities[(size_t) j], pendingVelocities[(size_t) (j - 1)]);
            }

        triggerPendingNotes (engine, step, stepPhase);
        gateOpen = true;
    }

    void MidiPlaygroundRuntime::process (IInstrumentEngine& engine, const RenderContext& context)
    {
        if (! enabled)
            return;

        if (settings.drumMachine)
        {
            processDrumMachine (engine, context);
            return;
        }

        if (heldNotes.empty())
        {
            stopActive (engine);
            currentStep = -1;
            currentRatchetSlot = -1;
            phase = 0.0;
            return;
        }

        const int steps = activeStepCount();
        const double phase01 = phase - std::floor (phase);
        const double scaled = phase01 * (double) steps;
        const int step = juce::jlimit (0, steps - 1, (int) std::floor (scaled));
        const double stepPhase = scaled - std::floor (scaled);
        const double swingDelay = (step % 2 == 1) ? (double) settings.swing * 0.5 : 0.0;
        const double stepGate = (double) settings.gates[(size_t) step] * (1.0 - swingDelay);
        const bool shouldGateOpen = stepPhase >= swingDelay && stepPhase <= juce::jmin (1.0, swingDelay + stepGate);
        const int ratchetSlot = shouldGateOpen
            ? juce::jlimit (0, settings.ratchet - 1,
                            (int) std::floor (juce::jlimit (0.0, 0.999999, (stepPhase - swingDelay) / juce::jmax (0.0001, stepGate))
                                              * (double) settings.ratchet))
            : -1;

        if (currentStep >= 0 && step < currentStep)
            ++cycleCounter;

        if (step != currentStep)
        {
            currentStep = step;
            currentRatchetSlot = -1;
            stopActive (engine);
            if (shouldGateOpen)
            {
                currentRatchetSlot = ratchetSlot;
                startStep (engine, step, stepPhase, swingDelay, stepGate);
            }
        }
        else if (shouldGateOpen && (! gateOpen || ratchetSlot != currentRatchetSlot))
        {
            currentRatchetSlot = ratchetSlot;
            startStep (engine, step, stepPhase, swingDelay, stepGate);
        }
        else if (shouldGateOpen && gateOpen)
        {
            triggerPendingNotes (engine, step, stepPhase);
        }
        else if (! shouldGateOpen && gateOpen)
        {
            currentRatchetSlot = -1;
            stopActive (engine);
        }

        const double cyclesPerSecond = settings.sync
            ? (RenderContext::sanitiseBpm (context.bpm) / 240.0) * (double) settings.rate
            : (double) settings.rate;
        phase += cyclesPerSecond * context.secondsPerBlock();
        phase -= std::floor (phase);
    }
}
