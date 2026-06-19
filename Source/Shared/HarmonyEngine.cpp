#include "HarmonyEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace patchcraft
{
    namespace
    {
        int positiveMod (int value, int modulus)
        {
            const int result = value % modulus;
            return result < 0 ? result + modulus : result;
        }

        std::vector<int> pitchClassSet (const std::vector<int>& notes)
        {
            std::set<int> unique;
            for (const auto note : notes)
                if (note >= 0 && note <= 127)
                    unique.insert (positiveMod (note, 12));
            return { unique.begin(), unique.end() };
        }

        std::vector<int> relativePitchClasses (const std::vector<int>& intervals)
        {
            std::set<int> unique;
            for (const auto interval : intervals)
                unique.insert (positiveMod (interval, 12));
            return { unique.begin(), unique.end() };
        }

        juce::String romanForDegree (int degree, const juce::String& chordId)
        {
            static const std::array<const char*, 7> upper {{ "I", "II", "III", "IV", "V", "VI", "VII" }};
            auto roman = juce::String (upper[(size_t) positiveMod (degree, 7)]);
            const auto lower = chordId.containsIgnoreCase ("minor")
                            || chordId == "diminished"
                            || chordId == "half_diminished"
                            || chordId == "diminished7";
            if (lower)
                roman = roman.toLowerCase();
            if (chordId == "diminished" || chordId == "diminished7")
                roman += "dim";
            else if (chordId == "half_diminished")
                roman += "half-dim";
            else if (chordId.contains ("7"))
                roman += "7";
            return roman;
        }

        int nearestChordIndex (const std::vector<int>& relativeIntervals)
        {
            const auto target = relativePitchClasses (relativeIntervals);
            int bestIndex = 0;
            int bestScore = std::numeric_limits<int>::min();
            for (int index = 0; index < (int) HarmonyEngine::chords().size(); ++index)
            {
                const auto candidate = relativePitchClasses (HarmonyEngine::chordAt (index).intervals);
                int matches = 0;
                for (const auto pitchClass : target)
                    if (std::find (candidate.begin(), candidate.end(), pitchClass) != candidate.end())
                        ++matches;
                const int extras = (int) candidate.size() - matches;
                const int missing = (int) target.size() - matches;
                const int score = matches * 8 - extras * 2 - missing * 5;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = index;
                }
            }
            return bestIndex;
        }
    }

    const std::vector<HarmonyEngine::ScaleDefinition>& HarmonyEngine::scales()
    {
        static const std::vector<ScaleDefinition> definitions {
            { "chromatic",             "Chromatic",             { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
            { "major",                 "Major / Ionian",        { 0, 2, 4, 5, 7, 9, 11 } },
            { "natural_minor",         "Natural Minor",         { 0, 2, 3, 5, 7, 8, 10 } },
            { "dorian",                "Dorian",                { 0, 2, 3, 5, 7, 9, 10 } },
            { "phrygian",              "Phrygian",              { 0, 1, 3, 5, 7, 8, 10 } },
            { "lydian",                "Lydian",                { 0, 2, 4, 6, 7, 9, 11 } },
            { "mixolydian",            "Mixolydian",            { 0, 2, 4, 5, 7, 9, 10 } },
            { "locrian",               "Locrian",               { 0, 1, 3, 5, 6, 8, 10 } },
            { "harmonic_minor",        "Harmonic Minor",        { 0, 2, 3, 5, 7, 8, 11 } },
            { "melodic_minor",         "Melodic Minor",         { 0, 2, 3, 5, 7, 9, 11 } },
            { "major_pentatonic",      "Major Pentatonic",      { 0, 2, 4, 7, 9 } },
            { "minor_pentatonic",      "Minor Pentatonic",      { 0, 3, 5, 7, 10 } },
            { "blues",                 "Blues",                 { 0, 3, 5, 6, 7, 10 } },
            { "whole_tone",            "Whole Tone",            { 0, 2, 4, 6, 8, 10 } },
            { "diminished_hw",         "Diminished Half-Whole", { 0, 1, 3, 4, 6, 7, 9, 10 } },
            { "diminished_wh",         "Diminished Whole-Half", { 0, 2, 3, 5, 6, 8, 9, 11 } },
            { "phrygian_dominant",     "Phrygian Dominant",     { 0, 1, 4, 5, 7, 8, 10 } },
            { "double_harmonic",       "Double Harmonic",       { 0, 1, 4, 5, 7, 8, 11 } },
            { "hungarian_minor",       "Hungarian Minor",       { 0, 2, 3, 6, 7, 8, 11 } },
            { "lydian_dominant",       "Lydian Dominant",       { 0, 2, 4, 6, 7, 9, 10 } },
            { "altered",               "Altered",               { 0, 1, 3, 4, 6, 8, 10 } },
            { "hirajoshi",             "Hirajoshi",             { 0, 2, 3, 7, 8 } },
            { "in_sen",                "In Sen",                { 0, 1, 5, 7, 10 } },
            { "enigmatic",             "Enigmatic",             { 0, 1, 4, 6, 8, 10, 11 } }
        };
        return definitions;
    }

    const std::vector<HarmonyEngine::ChordDefinition>& HarmonyEngine::chords()
    {
        static const std::vector<ChordDefinition> definitions {
            { "major",          "",       "Major",             { 0, 4, 7 } },
            { "minor",          "m",      "Minor",             { 0, 3, 7 } },
            { "diminished",     "dim",    "Diminished",        { 0, 3, 6 } },
            { "augmented",      "aug",    "Augmented",         { 0, 4, 8 } },
            { "sus2",           "sus2",   "Suspended 2",       { 0, 2, 7 } },
            { "sus4",           "sus4",   "Suspended 4",       { 0, 5, 7 } },
            { "major6",         "6",      "Major 6",           { 0, 4, 7, 9 } },
            { "minor6",         "m6",     "Minor 6",           { 0, 3, 7, 9 } },
            { "dominant7",      "7",      "Dominant 7",        { 0, 4, 7, 10 } },
            { "major7",         "maj7",   "Major 7",           { 0, 4, 7, 11 } },
            { "minor7",         "m7",     "Minor 7",           { 0, 3, 7, 10 } },
            { "half_diminished", "m7b5",  "Half Diminished",   { 0, 3, 6, 10 } },
            { "diminished7",    "dim7",   "Diminished 7",      { 0, 3, 6, 9 } },
            { "minor_major7",   "mMaj7",  "Minor Major 7",     { 0, 3, 7, 11 } },
            { "add9",           "add9",   "Add 9",             { 0, 4, 7, 14 } },
            { "minor_add9",     "madd9",  "Minor Add 9",       { 0, 3, 7, 14 } },
            { "dominant9",      "9",      "Dominant 9",        { 0, 4, 7, 10, 14 } },
            { "major9",         "maj9",   "Major 9",           { 0, 4, 7, 11, 14 } },
            { "minor9",         "m9",     "Minor 9",           { 0, 3, 7, 10, 14 } },
            { "dominant11",     "11",     "Dominant 11",       { 0, 4, 7, 10, 14, 17 } },
            { "minor11",        "m11",    "Minor 11",          { 0, 3, 7, 10, 14, 17 } },
            { "dominant13",     "13",     "Dominant 13",       { 0, 4, 7, 10, 14, 21 } },
            { "major13",        "maj13",  "Major 13",          { 0, 4, 7, 11, 14, 21 } },
            { "minor13",        "m13",    "Minor 13",          { 0, 3, 7, 10, 14, 21 } },
            { "7sus4",          "7sus4",  "Dominant 7 Sus 4",  { 0, 5, 7, 10 } },
            { "six_nine",       "6/9",    "Major 6/9",         { 0, 4, 7, 9, 14 } },
            { "minor_six_nine", "m6/9",   "Minor 6/9",         { 0, 3, 7, 9, 14 } },
            { "quartal",        "quartal", "Quartal",          { 0, 5, 10, 15 } },
            { "power",          "5",      "Power Chord",       { 0, 7, 12 } },
            { "major7_sharp11", "maj7#11", "Major 7 Sharp 11", { 0, 4, 7, 11, 18 } },
            { "dominant7_flat9", "7b9",   "Dominant 7 Flat 9", { 0, 4, 7, 10, 13 } },
            { "dominant7_sharp9", "7#9",  "Dominant 7 Sharp 9",{ 0, 4, 7, 10, 15 } }
        };
        return definitions;
    }

    const HarmonyEngine::ScaleDefinition& HarmonyEngine::scaleAt (int index)
    {
        return scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, index)];
    }

    const HarmonyEngine::ChordDefinition& HarmonyEngine::chordAt (int index)
    {
        return chords()[(size_t) juce::jlimit (0, (int) chords().size() - 1, index)];
    }

    int HarmonyEngine::scaleIndexForId (const juce::String& id)
    {
        for (int index = 0; index < (int) scales().size(); ++index)
            if (scales()[(size_t) index].id.equalsIgnoreCase (id))
                return index;
        return 0;
    }

    int HarmonyEngine::chordIndexForId (const juce::String& id)
    {
        for (int index = 0; index < (int) chords().size(); ++index)
            if (chords()[(size_t) index].id.equalsIgnoreCase (id))
                return index;
        return 0;
    }

    juce::String HarmonyEngine::pitchClassName (int pitchClass, bool preferFlats)
    {
        static const std::array<const char*, 12> sharps {{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }};
        static const std::array<const char*, 12> flats  {{ "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" }};
        return preferFlats ? flats[(size_t) positiveMod (pitchClass, 12)]
                           : sharps[(size_t) positiveMod (pitchClass, 12)];
    }

    int HarmonyEngine::quantizeToScale (int midiNote, int rootPitchClass, int scaleIndex)
    {
        midiNote = juce::jlimit (0, 127, midiNote);
        const auto& scale = scaleAt (scaleIndex);
        auto contains = [&] (int note)
        {
            const int relative = positiveMod (note - rootPitchClass, 12);
            return std::find_if (scale.intervals.begin(), scale.intervals.end(),
                                 [relative] (int interval) { return positiveMod (interval, 12) == relative; })
                != scale.intervals.end();
        };

        if (contains (midiNote))
            return midiNote;

        for (int distance = 1; distance <= 11; ++distance)
        {
            if (midiNote + distance <= 127 && contains (midiNote + distance))
                return midiNote + distance;
            if (midiNote - distance >= 0 && contains (midiNote - distance))
                return midiNote - distance;
        }
        return midiNote;
    }

    std::vector<HarmonyEngine::ChordMatch> HarmonyEngine::detectChords (const std::vector<int>& midiNotes,
                                                                         int maximumResults)
    {
        std::vector<ChordMatch> results;
        const auto input = pitchClassSet (midiNotes);
        if (input.empty())
            return results;

        const int bass = positiveMod (*std::min_element (midiNotes.begin(), midiNotes.end()), 12);
        for (int root = 0; root < 12; ++root)
        {
            for (int chordIndex = 0; chordIndex < (int) chords().size(); ++chordIndex)
            {
                const auto& chord = chordAt (chordIndex);
                auto relative = relativePitchClasses (chord.intervals);
                std::vector<int> target;
                for (const auto interval : relative)
                    target.push_back (positiveMod (root + interval, 12));

                int matches = 0;
                for (const auto pitchClass : input)
                    if (std::find (target.begin(), target.end(), pitchClass) != target.end())
                        ++matches;
                const int missing = (int) target.size() - matches;
                const int extras = (int) input.size() - matches;
                if (matches < juce::jmin (2, (int) input.size()))
                    continue;

                const bool exact = missing == 0 && extras == 0;
                float score = (float) matches / (float) juce::jmax (1, (int) target.size());
                score -= 0.13f * (float) missing + 0.08f * (float) extras;
                if (root == bass) score += 0.08f;
                if (exact) score += 0.18f;

                int inversion = 0;
                for (int noteIndex = 0; noteIndex < (int) chord.intervals.size(); ++noteIndex)
                    if (positiveMod (root + chord.intervals[(size_t) noteIndex], 12) == bass)
                    {
                        inversion = noteIndex;
                        break;
                    }

                ChordMatch match;
                match.rootPitchClass = root;
                match.bassPitchClass = bass;
                match.inversion = inversion;
                match.chordIndex = chordIndex;
                match.chordId = chord.id;
                match.displayName = pitchClassName (root) + chord.symbol;
                if (inversion > 0)
                    match.displayName += "/" + pitchClassName (bass);
                match.confidence = juce::jlimit (0.0f, 1.0f, score);
                match.exact = exact;
                results.push_back (std::move (match));
            }
        }

        std::sort (results.begin(), results.end(), [] (const auto& a, const auto& b)
        {
            if (a.exact != b.exact) return a.exact > b.exact;
            return a.confidence > b.confidence;
        });
        if ((int) results.size() > maximumResults)
            results.resize ((size_t) juce::jmax (0, maximumResults));
        return results;
    }

    std::vector<HarmonyEngine::ScaleMatch> HarmonyEngine::detectScales (const std::vector<int>& midiNotes,
                                                                         int maximumResults)
    {
        std::vector<ScaleMatch> results;
        const auto input = pitchClassSet (midiNotes);
        if (input.empty())
            return results;

        for (int root = 0; root < 12; ++root)
        {
            for (int scaleIndex = 1; scaleIndex < (int) scales().size(); ++scaleIndex)
            {
                const auto& scale = scaleAt (scaleIndex);
                int matches = 0;
                for (const auto pitchClass : input)
                {
                    const int relative = positiveMod (pitchClass - root, 12);
                    if (std::find_if (scale.intervals.begin(), scale.intervals.end(),
                                     [relative] (int interval) { return positiveMod (interval, 12) == relative; })
                        != scale.intervals.end())
                        ++matches;
                }
                const int outside = (int) input.size() - matches;
                float score = (float) matches / (float) input.size() - 0.18f * (float) outside;
                if (std::find (input.begin(), input.end(), root) != input.end())
                    score += 0.06f;

                ScaleMatch match;
                match.rootPitchClass = root;
                match.scaleIndex = scaleIndex;
                match.scaleId = scale.id;
                match.displayName = pitchClassName (root) + " " + scale.name;
                match.confidence = juce::jlimit (0.0f, 1.0f, score);
                results.push_back (std::move (match));
            }
        }

        std::sort (results.begin(), results.end(), [] (const auto& a, const auto& b)
        {
            return a.confidence > b.confidence;
        });
        if ((int) results.size() > maximumResults)
            results.resize ((size_t) juce::jmax (0, maximumResults));
        return results;
    }

    HarmonyEngine::ChordSuggestion HarmonyEngine::buildDiatonicChord (int scaleRootPitchClass,
                                                                       int scaleIndex,
                                                                       int degree,
                                                                       int chordSize)
    {
        const auto& scale = scaleAt (scaleIndex);
        const int scaleSize = juce::jmax (1, (int) scale.intervals.size());
        degree = positiveMod (degree, scaleSize);
        chordSize = juce::jlimit (2, 8, chordSize);

        std::vector<int> intervals;
        const int rootInterval = scale.intervals[(size_t) degree];
        for (int voice = 0; voice < chordSize; ++voice)
        {
            const int scaleDegree = degree + voice * 2;
            const int octave = scaleDegree / scaleSize;
            const int interval = scale.intervals[(size_t) positiveMod (scaleDegree, scaleSize)] + octave * 12 - rootInterval;
            intervals.push_back (interval);
        }

        const int chordIndex = nearestChordIndex (intervals);
        const auto& chord = chordAt (chordIndex);
        ChordSuggestion result;
        result.degree = degree;
        result.rootPitchClass = positiveMod (scaleRootPitchClass + rootInterval, 12);
        result.chordIndex = chordIndex;
        result.chordId = chord.id;
        result.romanNumeral = romanForDegree (degree, chord.id);
        result.displayName = pitchClassName (result.rootPitchClass) + chord.symbol;
        result.reason = "Diatonic " + result.romanNumeral + " in "
                      + pitchClassName (scaleRootPitchClass) + " " + scale.name;
        result.score = 1.0f;
        return result;
    }

    std::vector<HarmonyEngine::ChordSuggestion> HarmonyEngine::diatonicChords (int scaleRootPitchClass,
                                                                                int scaleIndex,
                                                                                int chordSize)
    {
        std::vector<ChordSuggestion> result;
        const int count = juce::jmin (7, (int) scaleAt (scaleIndex).intervals.size());
        for (int degree = 0; degree < count; ++degree)
            result.push_back (buildDiatonicChord (scaleRootPitchClass, scaleIndex, degree, chordSize));
        return result;
    }

    std::vector<HarmonyEngine::ChordSuggestion> HarmonyEngine::suggestNextChords (int scaleRootPitchClass,
                                                                                   int scaleIndex,
                                                                                   int currentDegree,
                                                                                   int maximumResults)
    {
        static const std::array<std::array<int, 6>, 7> transitions {{
            {{ 4, 3, 5, 1, 2, 6 }},
            {{ 4, 6, 0, 3, 5, 2 }},
            {{ 5, 3, 1, 4, 0, 6 }},
            {{ 4, 0, 1, 5, 2, 6 }},
            {{ 0, 5, 3, 1, 2, 6 }},
            {{ 1, 3, 4, 2, 0, 6 }},
            {{ 0, 2, 4, 5, 1, 3 }}
        }};

        std::vector<ChordSuggestion> suggestions;
        const auto& order = transitions[(size_t) positiveMod (currentDegree, 7)];
        for (int rank = 0; rank < (int) order.size() && rank < maximumResults; ++rank)
        {
            auto suggestion = buildDiatonicChord (scaleRootPitchClass, scaleIndex, order[(size_t) rank], 4);
            suggestion.score = 1.0f - 0.11f * (float) rank;
            suggestion.reason = rank == 0 ? "Strong functional resolution" : "Smooth diatonic continuation";
            suggestions.push_back (std::move (suggestion));
        }
        return suggestions;
    }

    std::vector<int> HarmonyEngine::voiceChord (int rootPitchClass,
                                                 int chordIndex,
                                                 const VoicingOptions& options,
                                                 const std::vector<int>& previousVoicing)
    {
        const auto& chord = chordAt (chordIndex);
        const int voiceCount = juce::jlimit (1, 8, options.voices);
        const int low = juce::jlimit (0, 127, options.lowNote);
        const int high = juce::jlimit (low, 127, options.highNote);
        const int preferredCenter = juce::jlimit (low, high, options.preferredCenter);
        const int requestedInversion = options.inversion;

        std::vector<int> best;
        double bestScore = std::numeric_limits<double>::max();
        const int inversionStart = requestedInversion >= 0 ? requestedInversion : 0;
        const int inversionEnd = requestedInversion >= 0 ? requestedInversion : juce::jmin (voiceCount - 1, (int) chord.intervals.size() - 1);

        for (int inversion = inversionStart; inversion <= inversionEnd; ++inversion)
        {
            for (int octave = 1; octave <= 7; ++octave)
            {
                std::vector<int> candidate;
                const int base = (octave + 1) * 12 + positiveMod (rootPitchClass, 12);
                for (int voice = 0; voice < voiceCount; ++voice)
                {
                    const int sourceIndex = (voice + inversion) % (int) chord.intervals.size();
                    int note = base + chord.intervals[(size_t) sourceIndex];
                    if (voice + inversion >= (int) chord.intervals.size())
                        note += 12;
                    if (sourceIndex < inversion)
                        note += 12;
                    if (options.spread > 0.50f && voice >= 2)
                        note += 12;
                    while (note < low) note += 12;
                    while (note > high) note -= 12;
                    candidate.push_back (note);
                }
                std::sort (candidate.begin(), candidate.end());
                candidate.erase (std::unique (candidate.begin(), candidate.end()), candidate.end());
                if (candidate.empty() || candidate.front() < low || candidate.back() > high)
                    continue;

                double score = std::abs ((candidate.front() + candidate.back()) * 0.5 - preferredCenter);
                if (! previousVoicing.empty())
                {
                    for (int voice = 0; voice < (int) candidate.size(); ++voice)
                    {
                        const int previousIndex = juce::jmin (voice, (int) previousVoicing.size() - 1);
                        score += std::abs (candidate[(size_t) voice] - previousVoicing[(size_t) previousIndex]) * 1.75;
                    }
                }
                score += std::abs ((candidate.back() - candidate.front()) - options.spread * 36.0) * 0.15;
                if (score < bestScore)
                {
                    bestScore = score;
                    best = std::move (candidate);
                }
            }
        }

        return best;
    }
}
