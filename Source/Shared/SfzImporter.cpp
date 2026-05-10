#include "SfzImporter.h"
#include "DebugLog.h"

namespace patchcraft
{
    static juce::String trim (juce::String s)
    {
        return s.trim();
    }

    static bool isNoteName (const juce::String& s)
    {
        if (s.isEmpty()) return false;
        const char first = std::toupper (s[0]);
        return first >= 'A' && first <= 'G';
    }

    int SfzImporter::noteFromString (const juce::String& s)
    {
        if (s.isEmpty()) return -1;
        // Direct number
        if (s.containsOnly ("0123456789-"))
            return s.getIntValue();
        // Note name like c4, f#3, bb-1
        juce::String name = s.toLowerCase();
        int pos = 0;
        char noteChar = name[pos++];
        int semitone = -1;
        switch (noteChar)
        {
            case 'c': semitone = 0; break;
            case 'd': semitone = 2; break;
            case 'e': semitone = 4; break;
            case 'f': semitone = 5; break;
            case 'g': semitone = 7; break;
            case 'a': semitone = 9; break;
            case 'b': semitone = 11; break;
            default: return -1;
        }
        if (pos < name.length() && (name[pos] == '#' || name[pos] == 'b'))
        {
            if (name[pos] == '#') { semitone++; pos++; }
            else { semitone--; pos++; }
        }
        int octave = name.substring (pos).getIntValue();
        return (octave + 1) * 12 + semitone;
    }

    void SfzImporter::applyOpcode (RegionState& state, const juce::String& opcode,
                                   const juce::String& value)
    {
        const auto v = value.trim();
        if      (opcode == "sample")              state.samplePath = v;
        else if (opcode == "key")               { state.key = noteFromString (v); }
        else if (opcode == "lokey")             { state.lokey = noteFromString (v); if (state.lokey < 0) state.lokey = v.getIntValue(); }
        else if (opcode == "hikey")             { state.hikey = noteFromString (v); if (state.hikey < 0) state.hikey = v.getIntValue(); }
        else if (opcode == "lovel")             state.lovel = juce::jlimit (1, 127, v.getIntValue());
        else if (opcode == "hivel")             state.hivel = juce::jlimit (1, 127, v.getIntValue());
        else if (opcode == "pitch_keycenter")   { state.pitch_keycenter = noteFromString (v); if (state.pitch_keycenter < 0) state.pitch_keycenter = v.getIntValue(); }
        else if (opcode == "volume")            state.volume = (float) v.getDoubleValue();
        else if (opcode == "pan")               state.pan = (float) juce::jlimit (-100.0, 100.0, v.getDoubleValue()) / 100.0f;
        else if (opcode == "offset")            state.offset = juce::jmax (0, v.getIntValue());
        else if (opcode == "end")               state.end = juce::jmax (0, v.getIntValue());
        else if (opcode == "loop_mode" && (v == "loop_continuous" || v == "loop_sustain"))
            state.loop_mode = true;
        else if (opcode == "loop_start")        state.loop_start = juce::jmax (0, v.getIntValue());
        else if (opcode == "loop_end")          state.loop_end = juce::jmax (0, v.getIntValue());
        else if (opcode == "group")             state.group = juce::jmax (0, v.getIntValue());
        else if (opcode == "seq_position")     state.seq_position = juce::jmax (1, v.getIntValue());
        else if (opcode == "seq_length")       state.seq_length = juce::jmax (1, v.getIntValue());
        else if (opcode == "trigger" && v == "release") state.triggerRelease = true;
        else if (opcode == "loop_mode" && v == "one_shot") state.oneShot = true;
    }

    SampleZoneDef SfzImporter::stateToZone (const RegionState& s, const juce::File& sfzDir)
    {
        SampleZoneDef z;
        z.samplePath = s.samplePath;
        if (z.samplePath.startsWithChar ('/') || z.samplePath.contains (":"))
            z.samplePath = s.samplePath; // absolute
        else
            z.samplePath = sfzDir.getChildFile (s.samplePath).getRelativePathFrom (sfzDir.getParentDirectory());

        if (s.key >= 0)
        {
            z.rootNote = s.key;
            z.lowNote = s.key;
            z.highNote = s.key;
        }
        else
        {
            z.lowNote = juce::jlimit (0, 127, s.lokey);
            z.highNote = juce::jlimit (0, 127, s.hikey);
            z.rootNote = s.pitch_keycenter >= 0 ? s.pitch_keycenter : (z.lowNote + z.highNote) / 2;
        }
        z.lowVelocity = juce::jlimit (1, 127, s.lovel);
        z.highVelocity = juce::jlimit (1, 127, s.hivel);
        z.gainDb = s.volume;
        z.pan = s.pan;
        z.sampleStart = s.offset;
        z.sampleEnd = s.end;
        z.loopEnabled = s.loop_mode;
        z.loopStart = s.loop_start;
        z.loopEnd = s.loop_end;
        z.roundRobinGroup = s.group;
        if (s.seq_length > 0 && s.seq_position > 0)
        {
            z.roundRobinGroup = s.group > 0 ? s.group : 1;
            z.roundRobinIndex = s.seq_position - 1;
        }
        z.oneShot = s.oneShot;
        return z;
    }

    SfzImporter::Result SfzImporter::parseFile (const juce::File& sfzFile)
    {
        Result result;
        if (! sfzFile.existsAsFile())
        {
            result.warnings.add ("SFZ file not found.");
            return result;
        }

        juce::String text = sfzFile.loadFileAsString();
        if (text.isEmpty())
        {
            result.warnings.add ("SFZ file is empty.");
            return result;
        }

        const auto sfzDir = sfzFile.getParentDirectory();
        RegionState globalState;
        RegionState groupState;
        int regionCount = 0;

        auto tokens = juce::StringArray::fromTokens (text, "<", {});
        for (auto& token : tokens)
        {
            juce::String tag = token.upToFirstOccurrenceOf (">", false, false).toLowerCase().trim();
            juce::String body = token.fromFirstOccurrenceOf (">", false, false).trim();

            if (tag == "global")
            {
                globalState = RegionState();
                groupState = RegionState();
            }
            else if (tag == "group")
            {
                groupState = globalState;
            }
            else if (tag == "region" || tag == "sample")
            {
                RegionState region = groupState;
                regionCount++;

                // Parse opcodes in the body
                juce::String remaining = body;
                while (remaining.isNotEmpty())
                {
                    // Skip whitespace
                    int idx = 0;
                    while (idx < remaining.length() && remaining[idx] == ' ') idx++;
                    remaining = remaining.substring (idx);
                    if (remaining.isEmpty()) break;

                    // Find opcode=value pair
                    int eq = remaining.indexOfChar ('=');
                    if (eq < 0) break;
                    juce::String opcode = remaining.substring (0, eq).trim();
                    juce::String value;
                    int valStart = eq + 1;
                    int valEnd = valStart;
                    bool inQuotes = false;
                    while (valEnd < remaining.length())
                    {
                        if (remaining[valEnd] == '"') inQuotes = !inQuotes;
                        else if (! inQuotes && remaining[valEnd] == ' ') break;
                        valEnd++;
                    }
                    value = remaining.substring (valStart, valEnd).trim();
                    applyOpcode (region, opcode, value);
                    remaining = remaining.substring (valEnd);
                }

                if (region.samplePath.isNotEmpty())
                {
                    result.zones.add (stateToZone (region, sfzDir));
                }
                else
                {
                    result.warnings.add ("Region " + juce::String (regionCount) + " has no sample path.");
                }
            }
            else if (tag.isNotEmpty())
            {
                // Unknown tag - skip gracefully
            }
        }

        result.success = result.zones.size() > 0;
        if (! result.success && result.warnings.isEmpty())
            result.warnings.add ("No regions found in SFZ file.");
        return result;
    }

} // namespace patchcraft
