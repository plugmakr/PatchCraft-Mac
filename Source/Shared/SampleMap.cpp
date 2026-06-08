#include "SampleMap.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace patchcraft
{
    namespace
    {
        static bool isSeparator (juce::juce_wchar c)
        {
            return c == ' ' || c == '_' || c == '-' || c == '.' || c == ','
                || c == '(' || c == ')' || c == '[' || c == ']' || c == '{'
                || c == '}' || c == '@';
        }

        static juce::StringArray tokenizeName (juce::String text)
        {
            for (int i = 0; i < text.length(); ++i)
                if (isSeparator (text[i]))
                    text = text.replaceSection (i, 1, " ");

            juce::StringArray tokens;
            tokens.addTokens (text, " ", {});
            tokens.removeEmptyStrings();
            tokens.trim();
            return tokens;
        }

        static bool parseIntSuffix (const juce::String& token,
                                    const juce::String& prefix,
                                    int& value)
        {
            if (! token.startsWithIgnoreCase (prefix))
                return false;

            const auto suffix = token.substring (prefix.length()).retainCharacters ("0123456789");
            if (suffix.isEmpty())
                return false;

            value = suffix.getIntValue();
            return true;
        }

        static bool parseVelocityRange (const juce::String& rawName, int& low, int& high)
        {
            auto normalised = rawName.toLowerCase();
            normalised = normalised.replace ("velocity", " vel");
            normalised = normalised.replace ("vel", " vel");
            normalised = normalised.replace ("v", " v");

            for (int i = 0; i < normalised.length(); ++i)
                if (! juce::CharacterFunctions::isDigit (normalised[i])
                    && normalised[i] != '-'
                    && normalised[i] != 'v'
                    && normalised[i] != 'e'
                    && normalised[i] != 'l')
                    normalised = normalised.replaceSection (i, 1, " ");

            juce::StringArray tokens;
            tokens.addTokens (normalised, " ", {});
            tokens.removeEmptyStrings();

            auto parseNumber = [] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789");
            };

            auto applyRange = [&] (const juce::String& first, const juce::String& second) -> bool
            {
                if (first.isEmpty())
                    return false;

                low = juce::jlimit (1, 127, first.getIntValue());
                high = second.isNotEmpty() ? juce::jlimit (1, 127, second.getIntValue()) : low;
                if (high < low)
                    std::swap (low, high);
                return true;
            };

            for (int i = 0; i < tokens.size(); ++i)
            {
                auto t = tokens[i].trim();
                if (t == "v" || t == "vel")
                {
                    if (i + 1 >= tokens.size())
                        continue;

                    auto next = tokens[i + 1].trim();
                    if (next.containsChar ('-'))
                    {
                        const auto a = next.upToFirstOccurrenceOf ("-", false, true).retainCharacters ("0123456789");
                        const auto b = next.fromFirstOccurrenceOf ("-", false, true).retainCharacters ("0123456789");
                        if (applyRange (a, b))
                            return true;
                    }

                    const auto a = parseNumber (next);
                    const auto b = i + 2 < tokens.size() ? parseNumber (tokens[i + 2]) : juce::String();
                    if (applyRange (a, b))
                        return true;
                }
            }

            for (const auto& token : tokens)
            {
                auto t = token.trim();
                if (! (t.startsWith ("v") || t.startsWith ("vel")))
                    continue;

                const bool explicitVelocityToken = t.startsWith ("vel") || t.containsChar ('-');
                if (! explicitVelocityToken)
                    continue;

                t = t.fromFirstOccurrenceOf ("v", false, true);
                if (t.startsWith ("el"))
                    t = t.substring (2);

                if (t.containsChar ('-'))
                {
                    const auto a = t.upToFirstOccurrenceOf ("-", false, true).retainCharacters ("0123456789");
                    const auto b = t.fromFirstOccurrenceOf ("-", false, true).retainCharacters ("0123456789");
                    if (a.isNotEmpty() && b.isNotEmpty())
                    {
                        low  = juce::jlimit (1, 127, a.getIntValue());
                        high = juce::jlimit (1, 127, b.getIntValue());
                        if (high < low)
                            std::swap (low, high);
                        return true;
                    }
                }

                const auto number = t.retainCharacters ("0123456789");
                if (number.isNotEmpty())
                {
                    const auto centre = juce::jlimit (1, 127, number.getIntValue());
                    low = high = centre;
                    return true;
                }
            }

            return false;
        }

        static bool parseRootNoteFromName (const juce::String& baseName, int& rootNote)
        {
            bool found = false;
            for (const auto& token : tokenizeName (baseName))
            {
                if (const int note = SampleMap::noteNumberFromName (token); note >= 0)
                {
                    rootNote = note;
                    found = true;
                }
            }
            return found;
        }

        static float medianAbsAmplitude (const std::vector<float>& samples)
        {
            if (samples.empty())
                return 0.0f;

            std::vector<float> values;
            values.reserve (samples.size());
            for (auto sample : samples)
                values.push_back (std::abs (sample));
            std::nth_element (values.begin(), values.begin() + (int) values.size() / 2, values.end());
            return values[values.size() / 2];
        }

    }

    void SampleMap::removeAt (int index)
    {
        if (index < 0 || index >= (int) zones.size()) return;
        zones.erase (zones.begin() + index);
    }

    const SampleZoneDef* SampleMap::findZoneFor (int note, int velocity) const
    {
        for (auto& z : zones)
        {
            if (note >= z.lowNote && note <= z.highNote
                && velocity >= z.lowVelocity && velocity <= z.highVelocity)
                return &z;
        }
        return nullptr;
    }

    void SampleMap::autoMapAcrossKeyboard()
    {
        const int n = (int) zones.size();
        if (n <= 0) return;
        const int low = 24, high = 108;
        const int span = juce::jmax (1, (high - low + 1) / n);

        for (int i = 0; i < n; ++i)
        {
            auto& z = zones[(size_t) i];
            z.lowNote  = juce::jmin (high, low + i * span);
            z.highNote = (i == n - 1) ? high : juce::jmin (high, z.lowNote + span - 1);
            z.rootNote = juce::jlimit (0, 127, z.lowNote + span / 2);
            z.lowVelocity  = 1;
            z.highVelocity = 127;
        }
    }

    void SampleMap::autoMapByRootNotes()
    {
        if (zones.empty())
            return;

        std::vector<int> roots;
        roots.reserve (zones.size());
        for (const auto& zone : zones)
            roots.push_back (juce::jlimit (0, 127, zone.rootNote));

        std::sort (roots.begin(), roots.end());
        roots.erase (std::unique (roots.begin(), roots.end()), roots.end());

        if (roots.size() <= 1)
        {
            const bool multipleZones = zones.size() > 1;
            for (auto& zone : zones)
            {
                const int root = juce::jlimit (0, 127, zone.rootNote);
                zone.lowNote = multipleZones ? root : 0;
                zone.highNote = multipleZones ? root : 127;
            }
            return;
        }

        for (auto& zone : zones)
        {
            const int root = juce::jlimit (0, 127, zone.rootNote);
            const auto it = std::lower_bound (roots.begin(), roots.end(), root);
            const int index = (int) std::distance (roots.begin(), it);

            const int previousRoot = index > 0 ? roots[(size_t) index - 1] : root;
            const int nextRoot = index + 1 < (int) roots.size() ? roots[(size_t) index + 1] : root;

            zone.lowNote = index == 0 ? 0 : juce::jlimit (0, 127, ((previousRoot + root) / 2) + 1);
            zone.highNote = index == (int) roots.size() - 1 ? 127 : juce::jlimit (0, 127, (root + nextRoot) / 2);
        }
    }

    void SampleMap::autoMapDrumPads (int startNote, int padCount, bool stackSamples)
    {
        if (zones.empty())
            return;

        startNote = juce::jlimit (0, 127, startNote);
        padCount = juce::jlimit (1, 16, padCount);

        std::array<int, 128> noteCounts {};

        for (int i = 0; i < (int) zones.size(); ++i)
        {
            auto& zone = zones[(size_t) i];
            const bool assignToPad = stackSamples || i < padCount;
            const int pad = stackSamples ? i % padCount : juce::jlimit (0, padCount - 1, i);
            const int note = juce::jlimit (0, 127,
                                           assignToPad ? startNote + pad
                                                       : startNote + padCount + (i - padCount));

            zone.rootNote = note;
            zone.lowNote = note;
            zone.highNote = note;
            zone.lowVelocity = juce::jlimit (1, 127, zone.lowVelocity);
            zone.highVelocity = juce::jlimit (zone.lowVelocity, 127, zone.highVelocity);
            zone.loopEnabled = false;
            zone.padIndex = assignToPad ? juce::jlimit (0, 15, pad) : -1;
            zone.padLabel = juce::File (zone.samplePath).getFileNameWithoutExtension();
            if (zone.padLabel.isEmpty())
                zone.padLabel = assignToPad ? "Pad " + juce::String (zone.padIndex + 1) : "Unassigned";
            zone.chokeGroup = 0;
            zone.oneShot = true;
            zone.triggerProbability = juce::jlimit (0, 100, zone.triggerProbability);
            zone.group = "Drum Pads";
            zone.roundRobinGroup = 0;
            zone.roundRobinIndex = 0;

            if (assignToPad)
                ++noteCounts[(size_t) note];
        }

        std::map<int, int> roundRobinIndexByNote;
        for (auto& zone : zones)
        {
            const int note = juce::jlimit (0, 127, zone.rootNote);
            if (stackSamples
                && noteCounts[(size_t) note] > 1
                && zone.lowVelocity == 1
                && zone.highVelocity == 127)
            {
                zone.roundRobinGroup = juce::jlimit (1, 127, note + 1);
                zone.roundRobinIndex = ++roundRobinIndexByNote[note];
            }
        }
    }

    int SampleMap::noteNumberFromName (const juce::String& noteName)
    {
        auto token = noteName.trim().toUpperCase();
        if (token.isEmpty())
            return -1;

        const juce::StringArray prefixes { "KEY", "ROOT", "NOTE", "MIDI" };
        bool hadPrefix = false;
        for (const auto& prefix : prefixes)
        {
            if (token.startsWith (prefix))
            {
                token = token.substring (prefix.length());
                hadPrefix = true;
            }
        }

        if (hadPrefix && token.isNotEmpty() && token == token.retainCharacters ("0123456789"))
        {
            const int note = token.getIntValue();
            if (note >= 0 && note <= 127)
                return note;
        }

        if (token.length() < 2)
            return -1;

        const auto letter = token[0];
        int semitone = -1;
        if      (letter == 'C') semitone = 0;
        else if (letter == 'D') semitone = 2;
        else if (letter == 'E') semitone = 4;
        else if (letter == 'F') semitone = 5;
        else if (letter == 'G') semitone = 7;
        else if (letter == 'A') semitone = 9;
        else if (letter == 'B') semitone = 11;
        else return -1;

        int octaveStart = 1;
        if (token.length() > 2 && (token[1] == '#' || token[1] == 'B'))
        {
            semitone += token[1] == '#' ? 1 : -1;
            octaveStart = 2;
        }

        const auto octaveRaw = token.substring (octaveStart);
        const auto octaveText = octaveRaw.retainCharacters ("-0123456789");
        if (octaveText.isEmpty())
            return -1;
        if (octaveRaw != octaveText)
            return -1;

        const int octave = octaveText.getIntValue();
        if (octave < -2 || octave > 9)
            return -1;
        return juce::jlimit (0, 127, (octave + 2) * 12 + semitone);
    }

    SampleZoneDef SampleMap::inferZoneFromFile (const juce::File& file,
                                                int fallbackRootNote,
                                                int fallbackLowNote,
                                                int fallbackHighNote,
                                                bool* usedVelocityRange)
    {
        if (usedVelocityRange != nullptr)
            *usedVelocityRange = false;

        SampleZoneDef zone;
        zone.samplePath = file.getFullPathName();
        zone.rootNote = juce::jlimit (0, 127, fallbackRootNote);
        zone.lowNote = juce::jlimit (0, 127, fallbackLowNote);
        zone.highNote = juce::jlimit (0, 127, fallbackHighNote);
        zone.lowVelocity = 1;
        zone.highVelocity = 127;

        const auto baseName = file.getFileNameWithoutExtension();
        int parsedRoot = zone.rootNote;
        if (parseRootNoteFromName (baseName, parsedRoot))
            zone.rootNote = parsedRoot;

        for (const auto& token : tokenizeName (baseName))
        {
            int parsed = 0;
            if (parseIntSuffix (token, "rrg", parsed) || parseIntSuffix (token, "rrgroup", parsed))
                zone.roundRobinGroup = juce::jlimit (0, 127, parsed);
            else if (parseIntSuffix (token, "rr", parsed))
            {
                zone.roundRobinGroup = zone.roundRobinGroup > 0 ? zone.roundRobinGroup : 1;
                zone.roundRobinIndex = juce::jlimit (0, 127, parsed);
            }
        }

        const bool parsedVelocity = parseVelocityRange (baseName, zone.lowVelocity, zone.highVelocity);
        if (usedVelocityRange != nullptr)
            *usedVelocityRange = parsedVelocity;
        return zone;
    }

    SampleZoneDef SampleMap::inferZoneFromFileWithAudio (const juce::File& file,
                                                         int fallbackRootNote,
                                                         int fallbackLowNote,
                                                         int fallbackHighNote,
                                                         bool* usedNamePitch,
                                                         bool* usedAudioPitch,
                                                         bool* usedVelocityRange)
    {
        if (usedNamePitch != nullptr)
            *usedNamePitch = false;
        if (usedAudioPitch != nullptr)
            *usedAudioPitch = false;

        auto zone = inferZoneFromFile (file, fallbackRootNote, fallbackLowNote, fallbackHighNote, usedVelocityRange);
        int parsedRoot = -1;
        const bool hasNamePitch = parseRootNoteFromName (file.getFileNameWithoutExtension(), parsedRoot);
        if (usedNamePitch != nullptr)
            *usedNamePitch = hasNamePitch;

        if (! hasNamePitch)
        {
            const int audioRoot = detectRootNoteFromAudioFile (file);
            if (audioRoot >= 0)
            {
                zone.rootNote = audioRoot;
                zone.lowNote = audioRoot;
                zone.highNote = audioRoot;
                if (usedAudioPitch != nullptr)
                    *usedAudioPitch = true;
            }
        }

        return zone;
    }

    int SampleMap::detectRootNoteFromAudioFile (const juce::File& file)
    {
        if (! file.existsAsFile())
            return -1;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            return -1;

        for (const auto& key : reader->metadataValues.getAllKeys())
        {
            const auto lowerKey = key.toLowerCase();
            if (! (lowerKey.contains ("root")
                   || lowerKey.contains ("unity")
                   || lowerKey.contains ("midi")
                   || lowerKey.contains ("note")))
                continue;

            const auto value = reader->metadataValues.getValue (key, {}).trim();
            int metadataNote = SampleMap::noteNumberFromName (value);
            if (metadataNote < 0 && value.isNotEmpty() && value == value.retainCharacters ("0123456789"))
                metadataNote = value.getIntValue();

            if (metadataNote >= 0 && metadataNote <= 127)
                return metadataNote;
        }

        const int samplesToRead = juce::jlimit (2048, 65536, (int) juce::jmin ((juce::int64) 65536, reader->lengthInSamples));
        const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
        juce::AudioBuffer<float> buffer (channels, samplesToRead);
        if (! reader->read (&buffer, 0, samplesToRead, 0, true, true))
            return -1;

        std::vector<float> mono;
        mono.resize ((size_t) samplesToRead);
        for (int sample = 0; sample < samplesToRead; ++sample)
        {
            float value = 0.0f;
            for (int channel = 0; channel < channels; ++channel)
                value += buffer.getSample (channel, sample);
            mono[(size_t) sample] = value / (float) channels;
        }

        const float noiseFloor = juce::jmax (0.0008f, medianAbsAmplitude (mono) * 2.0f);
        int start = 0;
        while (start < samplesToRead && std::abs (mono[(size_t) start]) < noiseFloor)
            ++start;

        if (start >= samplesToRead - 2048)
            return -1;

        const int window = juce::jmin (32768, samplesToRead - start);
        const double sampleRate = reader->sampleRate;
        const int minLag = juce::jmax (2, (int) std::floor (sampleRate / 2000.0));
        const int maxLag = juce::jmin (window / 2, (int) std::ceil (sampleRate / 20.0));
        if (maxLag <= minLag)
            return -1;

        double bestScore = 0.0;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double corr = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;
            const int count = window - lag;
            for (int i = 0; i < count; ++i)
            {
                const auto a = (double) mono[(size_t) (start + i)];
                const auto b = (double) mono[(size_t) (start + i + lag)];
                corr += a * b;
                energyA += a * a;
                energyB += b * b;
            }

            if (energyA <= 1.0e-9 || energyB <= 1.0e-9)
                continue;

            const double score = corr / std::sqrt (energyA * energyB);
            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        if (bestLag <= 0 || bestScore < 0.35)
            return -1;

        double frequency = sampleRate / (double) bestLag;
        if (frequency < 20.0 || frequency > 2000.0)
            return -1;

        while (frequency < 16.35)
            frequency *= 2.0;
        while (frequency > 7902.13)
            frequency *= 0.5;

        const int midi = juce::roundToInt (69.0 + 12.0 * std::log2 (frequency / 440.0));
        return juce::jlimit (0, 127, midi);
    }

    bool SampleMapHealthStatus::blocksExport() const
    {
        const bool sampleMapRelevant = engineIsSample || totalZones > 0;
        return sampleMapRelevant && ! exportReady;
    }

    juce::String SampleMapHealthStatus::exportMessage() const
    {
        juce::StringArray lines;
        lines.add ("Sample Map export validation failed:");

        if (primaryIssue.isNotEmpty())
            lines.add ("- " + primaryIssue);

        for (const auto& issue : issues)
            if (! lines.contains ("- " + issue))
                lines.add ("- " + issue);

        return lines.joinIntoString ("\n");
    }

    juce::File SampleMap::resolveSamplePath (const juce::File& projectFolder,
                                             const juce::String& samplePath)
    {
        if (samplePath.isEmpty())
            return {};

        if (juce::File::isAbsolutePath (samplePath))
            return juce::File (samplePath);

        return projectFolder.isDirectory()
            ? projectFolder.getChildFile (samplePath)
            : juce::File (samplePath);
    }

    SampleMapHealthStatus SampleMap::evaluateHealth (const SampleMap& map,
                                                     const juce::File& projectFolder,
                                                     const juce::String& engineId)
    {
        SampleMapHealthStatus status;
        const auto& zones = map.getZones();
        status.totalZones = (int) zones.size();
        status.engineIsSample = engineId == "sample";

        std::array<bool, 128> covered {};
        for (const auto& zone : zones)
        {
            const bool validRange = zone.lowNote >= 0 && zone.highNote <= 127 && zone.lowNote <= zone.highNote
                                 && zone.lowVelocity >= 1 && zone.highVelocity <= 127
                                 && zone.lowVelocity <= zone.highVelocity;
            if (! validRange)
            {
                ++status.invalidRanges;
                continue;
            }

            if (zone.rootNote < zone.lowNote || zone.rootNote > zone.highNote)
                ++status.rootOutsideRange;

            if (! resolveSamplePath (projectFolder, zone.samplePath).existsAsFile())
            {
                ++status.missingFiles;
                continue;
            }

            if (zone.midiPath.isNotEmpty()
                && ! resolveSamplePath (projectFolder, zone.midiPath).existsAsFile())
                ++status.missingMidiFiles;

            ++status.playableZones;
            for (int note = juce::jlimit (0, 127, zone.lowNote); note <= juce::jlimit (0, 127, zone.highNote); ++note)
                covered[(size_t) note] = true;
        }

        for (int note = 0; note < 128; ++note)
        {
            if (! covered[(size_t) note])
                continue;

            ++status.coveredNotes;
            if (status.firstCoveredNote < 0)
                status.firstCoveredNote = note;
            status.lastCoveredNote = note;
        }

        if (status.totalZones == 0)
            status.primaryIssue = "Import at least one sample zone.";
        else if (! status.engineIsSample)
            status.primaryIssue = "Project engine is not Sampler.";
        else if (status.invalidRanges > 0)
            status.primaryIssue = "Fix invalid key or velocity ranges.";
        else if (status.missingFiles > 0)
            status.primaryIssue = "Resolve missing sample files.";
        else if (status.missingMidiFiles > 0)
            status.primaryIssue = "Resolve missing zone MIDI files.";
        else if (status.playableZones == 0)
            status.primaryIssue = "No playable zones are available.";
        else if (status.rootOutsideRange > 0)
            status.primaryIssue = "Some root notes are outside their key ranges.";
        else
            status.primaryIssue = "Ready for Test and Export.";

        if (status.totalZones == 0 && status.engineIsSample)
            status.issues.add ("Sampler engine needs at least one mapped sample zone.");
        if (status.totalZones > 0 && ! status.engineIsSample)
            status.issues.add ("Sample zones exist, but project engine is not Sampler.");
        if (status.invalidRanges > 0)
            status.issues.add ("Invalid key/velocity ranges: " + juce::String (status.invalidRanges) + ".");
        if (status.missingFiles > 0)
            status.issues.add ("Missing sample files: " + juce::String (status.missingFiles) + ".");
        if (status.missingMidiFiles > 0)
            status.issues.add ("Missing zone MIDI files: " + juce::String (status.missingMidiFiles) + ".");
        if (status.rootOutsideRange > 0)
            status.issues.add ("Root notes outside key range: " + juce::String (status.rootOutsideRange) + ".");
        if (status.playableZones == 0 && status.totalZones > 0)
            status.issues.add ("No playable sample zones are available.");

        status.exportReady = status.engineIsSample
                          && status.totalZones > 0
                          && status.playableZones == status.totalZones
                          && status.invalidRanges == 0
                          && status.missingFiles == 0
                          && status.missingMidiFiles == 0
                          && status.rootOutsideRange == 0;
        return status;
    }

    juce::var SampleMap::toVar() const
    {
        juce::Array<juce::var> arr;
        for (auto& z : zones) arr.add (z.toVar());
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("zones", arr);
        return juce::var (obj);
    }

    void SampleMap::fromVar (const juce::var& v)
    {
        zones.clear();
        if (auto* o = v.getDynamicObject())
            if (auto* a = o->getProperty ("zones").getArray())
                for (auto& item : *a)
                    zones.push_back (SampleZoneDef::fromVar (item));
    }

} // namespace patchcraft
