#include "ComposerRuntime.h"

#include <algorithm>
#include <cmath>

namespace patchcraft
{
    namespace
    {
        int positiveMod (int value, int modulus)
        {
            const int result = value % modulus;
            return result < 0 ? result + modulus : result;
        }
    }

    bool ComposerRuntime::isComposerBlock (const DspBlock& block)
    {
        const auto type = block.type.trim().toLowerCase().removeCharacters (" _-");
        return type == "harmonycomposer"
            || type == "chordcomposer"
            || type == "composer";
    }

    float ComposerRuntime::valueFor (const DspBlock& block, const juce::String& key, float fallback)
    {
        if (const auto found = block.values.find (key); found != block.values.end())
            return found->second;
        return fallback;
    }

    void ComposerRuntime::bind (const DspGraph& graph)
    {
        enabled = false;
        for (const auto& block : graph.blocks)
        {
            if (! block.enabled || ! isComposerBlock (block))
                continue;
            loadBlock (block);
            enabled = true;
            break;
        }
        reset();
    }

    void ComposerRuntime::loadBlock (const DspBlock& block)
    {
        settings.rootPitchClass = juce::jlimit (0, 11, juce::roundToInt (valueFor (block, "composerRoot", 0.0f)));
        settings.scaleIndex = juce::jlimit (0, (int) HarmonyEngine::scales().size() - 1,
                                            juce::roundToInt (valueFor (block, "composerScale", 1.0f)));
        settings.chordCount = juce::jlimit (1, kMaxChords, juce::roundToInt (valueFor (block, "composerChordCount", 4.0f)));
        settings.beatsPerChord = juce::jlimit (0.125f, 16.0f, valueFor (block, "composerRate", 1.0f));
        settings.gate = juce::jlimit (0.05f, 1.0f, valueFor (block, "composerGate", 0.82f));
        settings.velocity = juce::jlimit (0.01f, 1.0f, valueFor (block, "composerVelocity", 0.82f));
        settings.voices = juce::jlimit (1, 8, juce::roundToInt (valueFor (block, "composerVoices", 4.0f)));
        settings.octave = juce::jlimit (1, 7, juce::roundToInt (valueFor (block, "composerOctave", 4.0f)));
        settings.outputChannel = juce::jlimit (1, 16, juce::roundToInt (valueFor (block, "composerOutputChannel", 1.0f)));
        settings.spread = juce::jlimit (0.0f, 1.0f, valueFor (block, "composerSpread", 0.38f));
        settings.midiThru = valueFor (block, "composerMidiThru", 1.0f) >= 0.5f;

        for (int chord = 0; chord < kMaxChords; ++chord)
        {
            const auto suffix = juce::String (chord + 1);
            const int fallbackDegree = chord == 0 ? 0 : (chord == 1 ? 5 : (chord == 2 ? 3 : (chord == 3 ? 4 : chord % 7)));
            settings.degrees[(size_t) chord] = juce::jlimit (0, 6,
                juce::roundToInt (valueFor (block, "composerDegree" + suffix, (float) fallbackDegree)));
            settings.inversions[(size_t) chord] = juce::jlimit (-1, 7,
                juce::roundToInt (valueFor (block, "composerInversion" + suffix, -1.0f)));
            settings.active[(size_t) chord] = valueFor (block, "composerChord" + suffix + "On", chord < settings.chordCount ? 1.0f : 0.0f) >= 0.5f;
        }
    }

    void ComposerRuntime::reset()
    {
        activeNotes.clear();
        previousVoicing.clear();
        currentChordIndex = -1;
        playbackPosition01 = -1.0;
        previousPpq = -1.0;
        gateOpen = false;
    }

    void ComposerRuntime::setParameter (const juce::String& parameterId, float value)
    {
        if (parameterId == "composerRoot") settings.rootPitchClass = juce::jlimit (0, 11, juce::roundToInt (value));
        else if (parameterId == "composerScale") settings.scaleIndex = juce::jlimit (0, (int) HarmonyEngine::scales().size() - 1, juce::roundToInt (value));
        else if (parameterId == "composerChordCount") settings.chordCount = juce::jlimit (1, kMaxChords, juce::roundToInt (value));
        else if (parameterId == "composerRate") settings.beatsPerChord = juce::jlimit (0.125f, 16.0f, value);
        else if (parameterId == "composerGate") settings.gate = juce::jlimit (0.05f, 1.0f, value);
        else if (parameterId == "composerVelocity") settings.velocity = juce::jlimit (0.01f, 1.0f, value);
        else if (parameterId == "composerVoices") settings.voices = juce::jlimit (1, 8, juce::roundToInt (value));
        else if (parameterId == "composerOctave") settings.octave = juce::jlimit (1, 7, juce::roundToInt (value));
        else if (parameterId == "composerOutputChannel") settings.outputChannel = juce::jlimit (1, 16, juce::roundToInt (value));
        else if (parameterId == "composerSpread") settings.spread = juce::jlimit (0.0f, 1.0f, value);
        else if (parameterId == "composerMidiThru") settings.midiThru = value >= 0.5f;
        else
        {
            for (int chord = 0; chord < kMaxChords; ++chord)
            {
                const auto suffix = juce::String (chord + 1);
                if (parameterId == "composerDegree" + suffix)
                    settings.degrees[(size_t) chord] = juce::jlimit (0, 6, juce::roundToInt (value));
                else if (parameterId == "composerInversion" + suffix)
                    settings.inversions[(size_t) chord] = juce::jlimit (-1, 7, juce::roundToInt (value));
                else if (parameterId == "composerChord" + suffix + "On")
                    settings.active[(size_t) chord] = value >= 0.5f;
            }
        }
    }

    void ComposerRuntime::releaseAll (juce::MidiBuffer& output, int sampleOffset)
    {
        for (const auto note : activeNotes)
            output.addEvent (juce::MidiMessage::noteOff (settings.outputChannel, note), sampleOffset);
        activeNotes.clear();
        gateOpen = false;
    }

    void ComposerRuntime::startChord (int chordIndex, juce::MidiBuffer& output, int sampleOffset)
    {
        releaseAll (output, sampleOffset);
        chordIndex = juce::jlimit (0, settings.chordCount - 1, chordIndex);
        if (! settings.active[(size_t) chordIndex])
            return;

        const auto suggestion = HarmonyEngine::buildDiatonicChord (settings.rootPitchClass,
                                                                    settings.scaleIndex,
                                                                    settings.degrees[(size_t) chordIndex],
                                                                    settings.voices);
        HarmonyEngine::VoicingOptions options;
        options.lowNote = 24;
        options.highNote = 108;
        options.preferredCenter = (settings.octave + 1) * 12;
        options.voices = settings.voices;
        options.inversion = settings.inversions[(size_t) chordIndex];
        options.spread = settings.spread;
        auto notes = HarmonyEngine::voiceChord (suggestion.rootPitchClass,
                                                suggestion.chordIndex,
                                                options,
                                                previousVoicing);
        if (notes.empty())
            return;

        const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (settings.velocity * 127.0f));
        for (const auto note : notes)
            output.addEvent (juce::MidiMessage::noteOn (settings.outputChannel, note, velocity), sampleOffset);
        activeNotes = notes;
        previousVoicing = std::move (notes);
        gateOpen = true;
    }

    void ComposerRuntime::process (const RenderContext& context, juce::MidiBuffer& output)
    {
        if (! enabled || ! context.isPlaying)
        {
            if (! activeNotes.empty())
                releaseAll (output);
            currentChordIndex = -1;
            playbackPosition01 = -1.0;
            previousPpq = context.ppqPosition;
            return;
        }

        const double beatsPerChord = juce::jmax (0.125, (double) settings.beatsPerChord);
        const double cycleBeats = beatsPerChord * (double) settings.chordCount;
        const double cyclePosition = std::fmod (juce::jmax (0.0, context.ppqPosition), cycleBeats);
        const int chordIndex = juce::jlimit (0, settings.chordCount - 1,
                                             (int) std::floor (cyclePosition / beatsPerChord));
        const double chordPosition = cyclePosition - (double) chordIndex * beatsPerChord;
        const bool shouldGateBeOpen = chordPosition < beatsPerChord * settings.gate;
        const bool transportJumped = previousPpq >= 0.0
                                  && (context.ppqPosition + 0.0001 < previousPpq
                                      || context.ppqPosition - previousPpq > context.beatsPerBlock() * 4.0 + 0.01);

        playbackPosition01 = cycleBeats > 0.0 ? cyclePosition / cycleBeats : 0.0;
        if (transportJumped || chordIndex != currentChordIndex)
        {
            currentChordIndex = chordIndex;
            if (shouldGateBeOpen)
                startChord (chordIndex, output, 0);
            else
                releaseAll (output, 0);
        }
        else if (shouldGateBeOpen && ! gateOpen)
        {
            startChord (chordIndex, output, 0);
        }
        else if (! shouldGateBeOpen && gateOpen)
        {
            releaseAll (output, 0);
        }

        previousPpq = context.ppqPosition;
    }
}
