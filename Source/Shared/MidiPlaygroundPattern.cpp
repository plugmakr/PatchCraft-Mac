#include "MidiPlaygroundPattern.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static float valueFor (const DspBlock& block, const juce::String& key, float fallback)
        {
            if (auto it = block.values.find (key); it != block.values.end())
                return it->second;
            return fallback;
        }

        static juce::String bankPrefix (int bank)
        {
            return "mpBank" + juce::String (juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, bank) + 1) + "_";
        }

        static juce::String bankKey (int bank, const juce::String& key)
        {
            return bankPrefix (bank) + key;
        }

        static juce::String stepOnKey (int step)
        {
            return "mpStep" + juce::String (step) + "On";
        }

        static constexpr std::array<const char*, 31> kPhraseKeys {{
            "arpSteps", "arpPattern", "arpGate", "arpOctaves", "arpSwing",
            "mpScaleRoot", "mpScaleType", "mpChordMode", "mpChordSize", "mpChordSpread",
            "mpProbability", "mpHumanize", "mpMutation", "mpRatchet", "mpVelocityCurve",
            "mpOctaveFold", "mpSampleControl", "mpSampleSliceCount", "mpLatch",
            "mpProgressionPreset", "mpStrum", "mpFlam", "mpEuclideanPulses", "mpEuclideanRotate",
            "mpPolymeterSteps", "mpEchoRepeats", "mpEchoDelay", "mpEchoDecay",
            "mpPatternMorph", "mpKeySwitchEnabled", "mpKeySwitchBase"
        }};

        static constexpr std::array<const char*, 5> kStepValuePrefixes {{
            "arpNote", "mpVelocity", "mpGate", "mpStepProb", "mpSampleSlice"
        }};

        static int positiveMod (int value, int modulus)
        {
            const int result = value % modulus;
            return result < 0 ? result + modulus : result;
        }

        static bool scaleContainsPitchClass (int scaleType, int pitchClass)
        {
            if (scaleType <= 0)
                return true;

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

            const auto& scale = scales[(size_t) juce::jlimit (1, 9, scaleType) - 1];
            return std::find (scale.begin(), scale.end(), positiveMod (pitchClass, 12)) != scale.end();
        }

        static int quantizeToScale (int midiNote, int scaleRoot, int scaleType)
        {
            if (scaleType <= 0)
                return juce::jlimit (0, 127, midiNote);

            const int root = positiveMod (scaleRoot, 12);
            auto inScale = [&] (int note)
            {
                return scaleContainsPitchClass (scaleType, positiveMod (note - root, 12));
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

        static int chordIntervalFor (int noteIndex, int chordMode)
        {
            if (chordMode <= 0)
                return noteIndex == 0 ? 0 : 12 * noteIndex;

            const auto index = (size_t) juce::jlimit (0, 7, noteIndex);
            static constexpr std::array<int, 8> stackedThirds {{ 0, 4, 7, 11, 14, 17, 21, 24 }};
            static constexpr std::array<int, 8> scaleSeventh {{ 0, 4, 7, 11, 14, 17, 21, 24 }};
            static constexpr std::array<int, 8> fifths {{ 0, 7, 12, 19, 24, 31, 36, 43 }};
            static constexpr std::array<int, 8> suspended {{ 0, 5, 7, 12, 17, 19, 24, 29 }};
            static constexpr std::array<int, 8> addTwoSix {{ 0, 2, 5, 9, 12, 14, 17, 21 }};
            static constexpr std::array<int, 8> wideStack {{ 0, 7, 12, 17, 24, 31, 36, 41 }};

            static constexpr std::array<std::array<int, 8>, 23> exact {{
                {{ 0, 4, 7, 12, 16, 19, 24, 28 }},      // major
                {{ 0, 3, 7, 12, 15, 19, 24, 27 }},      // minor
                {{ 0, 3, 6, 12, 15, 18, 24, 27 }},      // diminished
                {{ 0, 4, 8, 12, 16, 20, 24, 28 }},      // augmented
                {{ 0, 2, 7, 12, 14, 19, 24, 26 }},      // sus2
                {{ 0, 5, 7, 12, 17, 19, 24, 29 }},      // sus4
                {{ 0, 4, 7, 9, 12, 16, 19, 21 }},       // major6
                {{ 0, 3, 7, 9, 12, 15, 19, 21 }},       // minor6
                {{ 0, 4, 7, 10, 12, 16, 19, 22 }},      // dominant7
                {{ 0, 4, 7, 11, 12, 16, 19, 23 }},      // major7
                {{ 0, 3, 7, 10, 12, 15, 19, 22 }},      // minor7
                {{ 0, 3, 6, 10, 12, 15, 18, 22 }},      // half-dim
                {{ 0, 3, 6, 9, 12, 15, 18, 21 }},       // diminished7
                {{ 0, 3, 7, 11, 12, 15, 19, 23 }},      // minMaj7
                {{ 0, 4, 7, 14, 16, 19, 24, 26 }},      // add9
                {{ 0, 3, 7, 14, 15, 19, 24, 26 }},      // minAdd9
                {{ 0, 4, 7, 10, 14, 16, 19, 22 }},      // dominant9
                {{ 0, 3, 7, 10, 14, 15, 19, 22 }},      // minor9
                {{ 0, 4, 7, 10, 14, 17, 19, 22 }},      // 11
                {{ 0, 4, 7, 10, 14, 21, 24, 28 }},      // 13
                {{ 0, 5, 7, 10, 12, 17, 19, 22 }},      // 7sus4
                {{ 0, 5, 10, 15, 19, 24, 29, 34 }},     // quartal
                {{ 0, 7, 12, 19, 24, 31, 36, 43 }}      // power
            }};

            if (chordMode == 2) return scaleSeventh[index];
            if (chordMode == 3) return fifths[index];
            if (chordMode == 4) return suspended[index];
            if (chordMode == 5) return addTwoSix[index];
            if (chordMode == 6) return wideStack[index];
            if (chordMode >= 7)
                return exact[(size_t) juce::jlimit (0, (int) exact.size() - 1, chordMode - 7)][index];
            return stackedThirds[index];
        }

        static int scaleDegreeToSemitone (int scaleType, int degree)
        {
            if (scaleType <= 0)
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

            const auto& scale = scales[(size_t) juce::jlimit (1, 9, scaleType) - 1];
            const int octave = degree >= 0 ? degree / 7 : -((std::abs (degree) + 6) / 7);
            return octave * 12 + scale[(size_t) positiveMod (degree, 7)];
        }

        static int chordIntervalForExport (int noteIndex, int chordMode, int scaleType)
        {
            if (chordMode == 1)
            {
                static constexpr std::array<int, 8> degrees {{ 0, 2, 4, 7, 9, 11, 14, 16 }};
                return scaleDegreeToSemitone (scaleType, degrees[(size_t) juce::jlimit (0, 7, noteIndex)]);
            }
            if (chordMode == 2)
            {
                static constexpr std::array<int, 8> degrees {{ 0, 2, 4, 6, 9, 11, 13, 16 }};
                return scaleDegreeToSemitone (scaleType, degrees[(size_t) juce::jlimit (0, 7, noteIndex)]);
            }
            if (chordMode == 6)
            {
                static constexpr std::array<int, 8> degrees {{ 0, 4, 7, 11, 14, 18, 21, 25 }};
                return scaleDegreeToSemitone (scaleType, degrees[(size_t) juce::jlimit (0, 7, noteIndex)]);
            }
            return chordIntervalFor (noteIndex, chordMode);
        }

        static bool stepPassesEuclideanMask (const DspBlock& block, int step)
        {
            const int polymeterSteps = juce::roundToInt (valueFor (block, "mpPolymeterSteps", 0.0f));
            const int steps = juce::jlimit (1, MidiPlaygroundPattern::kStepCount,
                                            polymeterSteps > 0 ? polymeterSteps
                                                               : juce::roundToInt (valueFor (block, "arpSteps", 8.0f)));
            const int pulses = juce::jlimit (0, steps, juce::roundToInt (valueFor (block, "mpEuclideanPulses", 0.0f)));
            if (pulses <= 0 || pulses >= steps)
                return true;

            const int rotated = positiveMod (step + juce::roundToInt (valueFor (block, "mpEuclideanRotate", 0.0f)), steps);
            return (rotated * pulses) % steps < pulses;
        }

        static bool isDrumMachineBlock (const DspBlock& block)
        {
            return block.type.containsIgnoreCase ("drum")
                || block.values.find ("dmTracks") != block.values.end()
                || block.values.find ("dmSteps") != block.values.end();
        }

        static juce::String drumPrefixForPattern (int pattern, int track, int step)
        {
            pattern = juce::jlimit (0, 7, pattern);
            return "dmP" + juce::String (pattern)
                + "T" + juce::String (juce::jlimit (0, 15, track))
                + "S" + juce::String (juce::jlimit (0, 63, step));
        }

        static int defaultDrumNote (int track)
        {
            static constexpr std::array<int, 16> notes {{
                36, 38, 42, 46, 41, 45, 49, 51, 37, 39, 44, 48, 50, 47, 52, 53
            }};
            return notes[(size_t) juce::jlimit (0, 15, track)];
        }

        static bool writeMidiFile (juce::MidiMessageSequence& sequence, const juce::File& targetFile,
                                   double bpm, int ticksPerQuarter, juce::String& error)
        {
            sequence.updateMatchedPairs();

            juce::MidiMessageSequence tempoTrack;
            const auto tempoMicroseconds = juce::roundToInt (60000000.0 / juce::jlimit (20.0, 300.0, bpm));
            tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (tempoMicroseconds), 0.0);

            juce::MidiFile midiFile;
            midiFile.setTicksPerQuarterNote (ticksPerQuarter);
            midiFile.addTrack (tempoTrack);
            midiFile.addTrack (sequence);

            auto outputFile = targetFile.hasFileExtension (".mid;.midi")
                ? targetFile : targetFile.withFileExtension (".mid");
            outputFile.deleteFile();
            auto stream = outputFile.createOutputStream();
            if (stream == nullptr || ! stream->openedOk())
            {
                error = "Could not open MIDI file for writing: " + outputFile.getFullPathName();
                return false;
            }

            if (! midiFile.writeTo (*stream))
            {
                error = "JUCE failed to write the MIDI file.";
                return false;
            }

            error.clear();
            return true;
        }

        static bool writeDrumMidiClip (const DspBlock& block, const juce::File& targetFile,
                                       double bpm, juce::String& error)
        {
            const int ticksPerQuarter = 960;
            const int stepTicks = ticksPerQuarter / 4;
            const int tracks = juce::jlimit (1, 16, juce::roundToInt (valueFor (block, "dmTracks", 8.0f)));
            const int steps = juce::jlimit (1, 64, juce::roundToInt (valueFor (block, "dmSteps", 16.0f)));
            const bool songMode = valueFor (block, "dmSongMode", 0.0f) >= 0.5f;
            const int chainLength = songMode
                ? juce::jlimit (1, 8, juce::roundToInt (valueFor (block, "dmChainLength", 1.0f)))
                : 1;

            juce::MidiMessageSequence sequence;
            int eventCount = 0;
            for (int chainIndex = 0; chainIndex < chainLength; ++chainIndex)
            {
                const int pattern = songMode
                    ? juce::jlimit (0, 7, juce::roundToInt (valueFor (block, "dmChain" + juce::String (chainIndex), (float) chainIndex)))
                    : juce::jlimit (0, 7, juce::roundToInt (valueFor (block, "dmPattern", 0.0f)));
                const int patternOffsetTicks = chainIndex * steps * stepTicks;

                for (int step = 0; step < steps; ++step)
                    for (int track = 0; track < tracks; ++track)
                    {
                        const auto prefix = drumPrefixForPattern (pattern, track, step);
                        const auto directPrefix = "dmT" + juce::String (track) + "S" + juce::String (step);
                        if (valueFor (block, prefix + "On", valueFor (block, directPrefix + "On", 0.0f)) < 0.5f)
                            continue;
                        if (valueFor (block, prefix + "Prob", valueFor (block, directPrefix + "Prob", 1.0f)) <= 0.0f)
                            continue;

                        const int note = juce::jlimit (0, 127,
                            juce::roundToInt (valueFor (block, "dmTrack" + juce::String (track) + "Note", (float) defaultDrumNote (track))));
                        const float velocity = juce::jlimit (0.01f, 1.0f,
                            valueFor (block, prefix + "Vel", valueFor (block, directPrefix + "Vel", track == 0 ? 1.0f : 0.75f)));
                        const float gate = juce::jlimit (0.05f, 1.0f,
                            valueFor (block, prefix + "Gate", valueFor (block, directPrefix + "Gate", track == 2 ? 0.16f : 0.36f)));
                        const int divisions = juce::jlimit (1, 4, juce::roundToInt (
                            valueFor (block, prefix + "Div", valueFor (block, directPrefix + "Div", 1.0f))));

                        for (int division = 0; division < divisions; ++division)
                        {
                            const auto startTick = (double) (patternOffsetTicks + step * stepTicks)
                                                 + (double) division * (double) stepTicks / (double) divisions;
                            const auto durationTicks = (double) juce::jmax (1,
                                juce::roundToInt ((float) stepTicks * gate / (float) divisions));
                            const float divisionVelocity = juce::jlimit (0.01f, 1.0f,
                                velocity * (division == 0 ? 1.0f : juce::jmax (0.40f, 1.0f - 0.12f * (float) division)));
                            sequence.addEvent (juce::MidiMessage::noteOn (10, note, divisionVelocity), startTick);
                            sequence.addEvent (juce::MidiMessage::noteOff (10, note), startTick + durationTicks);
                            ++eventCount;
                        }
                    }
            }

            if (eventCount == 0)
            {
                error = "The drum pattern has no enabled hits to export.";
                return false;
            }

            return writeMidiFile (sequence, targetFile, bpm, ticksPerQuarter, error);
        }

        struct ProgressionPreset
        {
            const char* name;
            int scaleType;
            int chordMode;
            int chordSize;
            float chordSpread;
            std::array<float, 4> roots;
            std::array<float, 4> colourTones;
            int patternStyle = 0;
            float gate = 0.72f;
            float swing = 0.0f;
            float strum = 0.08f;
            float humanize = 0.02f;
        };

        static const std::vector<ProgressionPreset>& progressionPresets()
        {
            static const std::vector<ProgressionPreset> presets {{
                { "I - V - vi - IV", 1, 1, 3, 0.28f, {{ 0.0f, 7.0f, 9.0f, 5.0f }}, {{ 4.0f, 11.0f, 12.0f, 9.0f }} },
                { "i - VI - III - VII", 2, 1, 3, 0.32f, {{ 0.0f, 8.0f, 3.0f, 10.0f }}, {{ 3.0f, 12.0f, 7.0f, 14.0f }} },
                { "ii - V - I - I", 1, 1, 4, 0.38f, {{ 2.0f, 7.0f, 0.0f, 0.0f }}, {{ 5.0f, 11.0f, 4.0f, 7.0f }} },
                { "I - vi - IV - V", 1, 1, 3, 0.24f, {{ 0.0f, 9.0f, 5.0f, 7.0f }}, {{ 4.0f, 12.0f, 9.0f, 11.0f }} },
                { "i - iv - VII - III", 2, 1, 3, 0.30f, {{ 0.0f, 5.0f, 10.0f, 3.0f }}, {{ 3.0f, 8.0f, 14.0f, 7.0f }} },
                { "Dorian Pedal Lift", 3, 2, 3, 0.34f, {{ 0.0f, 2.0f, 5.0f, 7.0f }}, {{ 7.0f, 9.0f, 12.0f, 14.0f }} },
                { "Phrygian Dark Steps", 4, 3, 3, 0.36f, {{ 0.0f, 1.0f, 5.0f, 8.0f }}, {{ 7.0f, 6.0f, 10.0f, 13.0f }} },
                { "Cinematic Lift", 1, 16, 4, 0.62f, {{ 0.0f, 5.0f, 9.0f, 7.0f }}, {{ 11.0f, 12.0f, 16.0f, 14.0f }}, 1, 0.58f, 0.04f, 0.16f, 0.03f },
                { "Neo Soul Glow", 1, 21, 4, 0.68f, {{ 0.0f, 4.0f, 9.0f, 2.0f }}, {{ 14.0f, 16.0f, 21.0f, 17.0f }}, 2, 0.64f, 0.12f, 0.20f, 0.05f },
                { "Ambient Glass Drift", 1, 11, 4, 0.72f, {{ 0.0f, 2.0f, 7.0f, 9.0f }}, {{ 14.0f, 16.0f, 19.0f, 21.0f }}, 3, 0.96f, 0.00f, 0.28f, 0.04f },
                { "Dark Trailer Resolve", 2, 17, 4, 0.58f, {{ 0.0f, 10.0f, 8.0f, 7.0f }}, {{ 12.0f, 15.0f, 13.0f, 14.0f }}, 6, 0.52f, 0.03f, 0.10f, 0.02f },
                { "Future Bass Anthem", 1, 23, 5, 0.74f, {{ 0.0f, 7.0f, 9.0f, 5.0f }}, {{ 14.0f, 18.0f, 21.0f, 17.0f }}, 4, 0.44f, 0.08f, 0.06f, 0.03f },
                { "Dream Pop Suspended", 1, 12, 4, 0.70f, {{ 0.0f, 5.0f, 2.0f, 7.0f }}, {{ 14.0f, 17.0f, 16.0f, 19.0f }}, 3, 0.92f, 0.02f, 0.22f, 0.05f },
                { "Lo-Fi Late Night", 2, 22, 4, 0.48f, {{ 0.0f, 3.0f, 8.0f, 10.0f }}, {{ 14.0f, 15.0f, 20.0f, 22.0f }}, 2, 0.54f, 0.18f, 0.14f, 0.08f },
                { "Melodic Techno Pulse", 3, 1, 3, 0.40f, {{ 0.0f, 2.0f, 7.0f, 5.0f }}, {{ 7.0f, 9.0f, 14.0f, 12.0f }}, 5, 0.36f, 0.04f, 0.00f, 0.01f },
                { "Gospel Passing Clouds", 1, 25, 6, 0.76f, {{ 0.0f, 4.0f, 5.0f, 7.0f }}, {{ 14.0f, 16.0f, 17.0f, 21.0f }}, 2, 0.82f, 0.10f, 0.26f, 0.05f },
                { "Cyberpunk Tension", 4, 28, 4, 0.52f, {{ 0.0f, 1.0f, 6.0f, 8.0f }}, {{ 10.0f, 13.0f, 18.0f, 20.0f }}, 6, 0.34f, 0.06f, 0.02f, 0.02f },
                { "Nordic Minor Bloom", 2, 20, 4, 0.66f, {{ 0.0f, 7.0f, 3.0f, 10.0f }}, {{ 14.0f, 19.0f, 15.0f, 22.0f }}, 1, 0.68f, 0.02f, 0.18f, 0.04f },
                { "Minimal Glass Arp", 1, 0, 1, 0.44f, {{ 0.0f, 7.0f, 2.0f, 9.0f }}, {{ 12.0f, 19.0f, 14.0f, 21.0f }}, 5, 0.42f, 0.00f, 0.00f, 0.03f }
            }};
            return presets;
        }
    }

    int MidiPlaygroundPattern::getActiveBank (const DspBlock& block)
    {
        return juce::jlimit (0, kPhraseBankCount - 1, juce::roundToInt (valueFor (block, "mpActiveBank", 0.0f)));
    }

    void MidiPlaygroundPattern::setActiveBank (DspBlock& block, int bank)
    {
        block.values["mpActiveBank"] = (float) juce::jlimit (0, kPhraseBankCount - 1, bank);
    }

    bool MidiPlaygroundPattern::bankHasData (const DspBlock& block, int bank)
    {
        const auto prefix = bankPrefix (bank);
        for (const auto& value : block.values)
            if (value.first.startsWith (prefix))
                return true;
        return false;
    }

    void MidiPlaygroundPattern::storeActiveBank (DspBlock& block, int bank)
    {
        bank = juce::jlimit (0, kPhraseBankCount - 1, bank);

        for (auto* key : kPhraseKeys)
            block.values[bankKey (bank, key)] = valueFor (block, key, 0.0f);

        for (int step = 0; step < kStepCount; ++step)
        {
            for (auto* prefix : kStepValuePrefixes)
            {
                const auto key = juce::String (prefix) + juce::String (step);
                const auto fallback = juce::String (prefix) == "mpSampleSlice" ? -1.0f : 0.0f;
                block.values[bankKey (bank, key)] = valueFor (block, key, fallback);
            }
            const auto activeKey = stepOnKey (step);
            block.values[bankKey (bank, activeKey)] = valueFor (block, activeKey, 1.0f);
        }

        setActiveBank (block, bank);
    }

    void MidiPlaygroundPattern::loadBank (DspBlock& block, int bank, bool seedFromActiveIfEmpty)
    {
        bank = juce::jlimit (0, kPhraseBankCount - 1, bank);
        if (! bankHasData (block, bank))
        {
            if (seedFromActiveIfEmpty)
                storeActiveBank (block, bank);
            else
                setActiveBank (block, bank);
            return;
        }

        for (auto* key : kPhraseKeys)
            if (auto it = block.values.find (bankKey (bank, key)); it != block.values.end())
                block.values[key] = it->second;

        for (int step = 0; step < kStepCount; ++step)
        {
            for (auto* prefix : kStepValuePrefixes)
            {
                const auto key = juce::String (prefix) + juce::String (step);
                if (auto it = block.values.find (bankKey (bank, key)); it != block.values.end())
                    block.values[key] = it->second;
            }

            const auto activeKey = stepOnKey (step);
            if (auto it = block.values.find (bankKey (bank, activeKey)); it != block.values.end())
                block.values[activeKey] = it->second;
        }

        setActiveBank (block, bank);
    }

    void MidiPlaygroundPattern::copyBank (DspBlock& block, int sourceBank, int destinationBank)
    {
        sourceBank = juce::jlimit (0, kPhraseBankCount - 1, sourceBank);
        destinationBank = juce::jlimit (0, kPhraseBankCount - 1, destinationBank);
        if (! bankHasData (block, sourceBank))
            storeActiveBank (block, sourceBank);

        const auto sourcePrefix = bankPrefix (sourceBank);
        const auto destinationPrefix = bankPrefix (destinationBank);
        for (const auto& value : std::vector<std::pair<juce::String, float>> (block.values.begin(), block.values.end()))
            if (value.first.startsWith (sourcePrefix))
                block.values[destinationPrefix + value.first.substring (sourcePrefix.length())] = value.second;
    }

    juce::StringArray MidiPlaygroundPattern::getProgressionNames()
    {
        juce::StringArray names;
        for (const auto& preset : progressionPresets())
            names.add (preset.name);
        return names;
    }

    void MidiPlaygroundPattern::applyProgressionPreset (DspBlock& block, int progressionIndex, int bank)
    {
        const auto& presets = progressionPresets();
        progressionIndex = juce::jlimit (0, (int) presets.size() - 1, progressionIndex);
        bank = juce::jlimit (0, kPhraseBankCount - 1, bank);
        const auto& preset = presets[(size_t) progressionIndex];

        block.type = "midiPlayground";
        block.section = "mod";
        block.name = juce::String (preset.name) + " Progression";
        if (block.targetId.isEmpty())
            block.targetId = "filterCutoff";
        block.enabled = true;

        block.values["arpSteps"] = 16.0f;
        block.values["arpPattern"] = 0.0f;
        block.values["arpGate"] = preset.gate;
        block.values["arpOctaves"] = 1.0f;
        block.values["arpSwing"] = preset.swing;
        block.values["mpScaleType"] = (float) preset.scaleType;
        block.values["mpChordMode"] = (float) preset.chordMode;
        block.values["mpChordSize"] = (float) preset.chordSize;
        block.values["mpChordSpread"] = preset.chordSpread;
        block.values["mpProbability"] = 1.0f;
        block.values["mpHumanize"] = preset.humanize;
        block.values["mpMutation"] = 0.0f;
        block.values["mpRatchet"] = 1.0f;
        block.values["mpVelocityCurve"] = -0.05f;
        block.values["mpOctaveFold"] = 1.0f;
        block.values["mpSampleControl"] = 0.0f;
        block.values["mpProgressionPreset"] = (float) progressionIndex;
        block.values["mpStrum"] = preset.strum;
        block.values["mpFlam"] = 0.0f;
        block.values["mpEuclideanPulses"] = 0.0f;
        block.values["mpEuclideanRotate"] = 0.0f;

        for (int step = 0; step < kStepCount; ++step)
        {
            const int chord = juce::jlimit (0, 3, step / 4);
            const int localStep = step % 4;
            const bool chordStart = localStep == 0;
            const auto suffix = juce::String (step);

            const auto root = preset.roots[(size_t) chord];
            const auto colour = preset.colourTones[(size_t) chord];
            float note = root;
            float velocity = chordStart ? 0.94f : 0.58f;
            float gate = chordStart ? 0.92f : 0.35f;
            float probability = chordStart ? 1.0f : 0.0f;
            bool active = chordStart;

            switch (preset.patternStyle)
            {
                case 1:
                {
                    static constexpr std::array<float, 4> offsets {{ 0.0f, 7.0f, 12.0f, 19.0f }};
                    note = root + offsets[(size_t) localStep];
                    active = true;
                    velocity = localStep == 0 ? 0.96f : (localStep == 2 ? 0.82f : 0.70f);
                    gate = localStep == 3 ? 0.46f : 0.58f;
                    probability = localStep == 3 ? 0.92f : 1.0f;
                    break;
                }
                case 2:
                {
                    static constexpr std::array<float, 4> offsets {{ 0.0f, 14.0f, 7.0f, 12.0f }};
                    note = (localStep == 1 || localStep == 3) ? colour : root + offsets[(size_t) localStep];
                    active = localStep != 2 || (step % 8) == 6;
                    velocity = localStep == 0 ? 0.90f : (localStep == 1 ? 0.66f : 0.74f);
                    gate = localStep == 0 ? 0.72f : 0.48f;
                    probability = localStep == 3 ? 0.88f : 1.0f;
                    break;
                }
                case 3:
                {
                    note = localStep == 0 ? root : (localStep == 2 ? colour + 7.0f : colour);
                    active = localStep == 0 || localStep == 2;
                    velocity = localStep == 0 ? 0.86f : 0.64f;
                    gate = localStep == 0 ? 0.96f : 0.78f;
                    probability = localStep == 2 ? 0.86f : 1.0f;
                    break;
                }
                case 4:
                {
                    static constexpr std::array<float, 4> offsets {{ 0.0f, 12.0f, 19.0f, 24.0f }};
                    note = localStep == 2 ? colour + 7.0f : root + offsets[(size_t) localStep];
                    active = localStep != 3 || (chord % 2) == 0;
                    velocity = localStep == 0 ? 0.98f : 0.76f;
                    gate = localStep == 0 ? 0.44f : 0.32f;
                    probability = localStep == 3 ? 0.78f : 1.0f;
                    break;
                }
                case 5:
                {
                    static constexpr std::array<float, 4> offsets {{ 0.0f, 12.0f, 7.0f, 19.0f }};
                    note = preset.chordMode == 0 ? root + offsets[(size_t) localStep]
                                                 : (localStep == 2 ? colour : root + offsets[(size_t) localStep]);
                    active = true;
                    velocity = (step % 4) == 0 ? 0.92f : ((step % 2) == 0 ? 0.72f : 0.56f);
                    gate = 0.34f;
                    probability = (step % 8) == 7 ? 0.80f : 1.0f;
                    break;
                }
                case 6:
                {
                    note = localStep == 0 ? root : (localStep == 1 ? colour : (localStep == 2 ? root + 12.0f : colour + 1.0f));
                    active = localStep != 1 || (chord % 2) == 0;
                    velocity = localStep == 0 ? 0.98f : (localStep == 3 ? 0.62f : 0.78f);
                    gate = localStep == 3 ? 0.26f : 0.40f;
                    probability = localStep == 3 ? 0.76f : 1.0f;
                    break;
                }
                default:
                {
                    if (localStep == 1)
                        note = colour;
                    else if (localStep == 2)
                        note = root + 12.0f;
                    else if (localStep == 3)
                        note = colour + 7.0f;
                    break;
                }
            }

            block.values["arpNote" + suffix] = note;
            block.values["mpStep" + suffix + "On"] = active ? 1.0f : 0.0f;
            block.values["mpVelocity" + suffix] = juce::jlimit (0.01f, 1.0f, velocity);
            block.values["mpGate" + suffix] = juce::jlimit (0.05f, 1.0f, gate);
            block.values["mpStepProb" + suffix] = active ? juce::jlimit (0.0f, 1.0f, probability) : 0.0f;
            block.values["mpSampleSlice" + suffix] = -1.0f;
        }

        setActiveBank (block, bank);
        storeActiveBank (block, bank);
    }

    bool MidiPlaygroundPattern::writeMidiClip (const DspBlock& block, const juce::File& targetFile,
                                               double bpm, int rootNote, juce::String& error)
    {
        if (targetFile == juce::File())
        {
            error = "No target file selected.";
            return false;
        }

        if (isDrumMachineBlock (block))
            return writeDrumMidiClip (block, targetFile, bpm, error);

        const int ticksPerQuarter = 960;
        const int stepTicks = ticksPerQuarter / 4;
        const int polymeterSteps = juce::roundToInt (valueFor (block, "mpPolymeterSteps", 0.0f));
        const int steps = juce::jlimit (1, kStepCount,
            polymeterSteps > 0 ? polymeterSteps : juce::roundToInt (valueFor (block, "arpSteps", 8.0f)));
        const int scaleRoot = positiveMod (juce::roundToInt (valueFor (block, "mpScaleRoot", 0.0f)), 12);
        const int scaleType = juce::jlimit (0, 9, juce::roundToInt (valueFor (block, "mpScaleType", 1.0f)));
        const int chordMode = juce::jlimit (0, 32, juce::roundToInt (valueFor (block, "mpChordMode", 0.0f)));
        const int chordSize = juce::jlimit (1, 8, juce::roundToInt (valueFor (block, "mpChordSize", chordMode > 0 ? 3.0f : 1.0f)));
        const bool exactSemitoneChord = chordMode >= 7;
        const int midiRoot = juce::jlimit (0, 127, rootNote + scaleRoot);

        juce::MidiMessageSequence sequence;
        int eventCount = 0;
        for (int step = 0; step < steps; ++step)
        {
            if (valueFor (block, stepOnKey (step), 1.0f) < 0.5f)
                continue;
            if (! stepPassesEuclideanMask (block, step))
                continue;
            if (valueFor (block, "mpStepProb" + juce::String (step), 1.0f) <= 0.0f)
                continue;

            const auto startTick = (double) (step * stepTicks);
            const auto durationTicks = (double) juce::jmax (1, juce::roundToInt ((float) stepTicks
                * juce::jlimit (0.05f, 1.0f, valueFor (block, "mpGate" + juce::String (step), valueFor (block, "arpGate", 0.55f)))));
            const int rawBaseNote = midiRoot + juce::roundToInt (valueFor (block, "arpNote" + juce::String (step), 0.0f));
            const int baseNote = exactSemitoneChord ? juce::jlimit (0, 127, rawBaseNote)
                                                    : quantizeToScale (rawBaseNote, scaleRoot, scaleType);
            const float velocity = juce::jlimit (0.01f, 1.0f, valueFor (block, "mpVelocity" + juce::String (step), 1.0f));
            const auto strumTicks = juce::jlimit (0.0, durationTicks * 0.80,
                                                  (double) valueFor (block, "mpStrum", 0.0f) * (double) stepTicks * 0.75);
            const auto flamTicks = valueFor (block, "mpFlam", 0.0f) > 0.0f
                ? juce::jlimit (1.0, durationTicks * 0.75,
                                24.0 + (double) valueFor (block, "mpFlam", 0.0f) * (double) stepTicks * 0.30)
                : 0.0;

            std::set<int> emitted;
            int rootChordNote = -1;
            for (int chordIndex = 0; chordIndex < chordSize; ++chordIndex)
            {
                const int chordNote = baseNote + chordIntervalForExport (chordIndex, chordMode, scaleType);
                const int note = exactSemitoneChord ? juce::jlimit (0, 127, chordNote)
                                                    : quantizeToScale (chordNote, scaleRoot, scaleType);
                if (note < 0 || note > 127 || emitted.count (note) != 0)
                    continue;

                emitted.insert (note);
                if (rootChordNote < 0)
                    rootChordNote = note;
                const auto noteStart = startTick + (chordSize <= 1 ? 0.0 : strumTicks * (double) chordIndex / (double) (chordSize - 1));
                sequence.addEvent (juce::MidiMessage::noteOn (1, note, velocity), noteStart);
                sequence.addEvent (juce::MidiMessage::noteOff (1, note), noteStart + durationTicks);
                ++eventCount;
            }

            if (rootChordNote >= 0 && flamTicks > 0.0)
            {
                const auto flamStart = startTick + flamTicks;
                sequence.addEvent (juce::MidiMessage::noteOn (1, rootChordNote, velocity * 0.82f), flamStart);
                sequence.addEvent (juce::MidiMessage::noteOff (1, rootChordNote), flamStart + durationTicks * 0.55);
                ++eventCount;
            }
        }

        if (eventCount == 0)
        {
            error = "The active phrase has no enabled MIDI notes to export.";
            return false;
        }

        return writeMidiFile (sequence, targetFile, bpm, ticksPerQuarter, error);
    }
}
