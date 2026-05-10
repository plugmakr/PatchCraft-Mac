#include "ArpeggiatorRuntime.h"
#include "DspRoutingEngine.h"
#include "EffectEngine.h"
#include "EngineFactory.h"
#include "MidiPlaygroundRuntime.h"
#include "MidiPlaygroundPattern.h"
#include "ParameterModel.h"
#include "PatchCraftPackReader.h"
#include "PatchCraftPackWriter.h"
#include "PatchCraftProject.h"
#include "SampleMap.h"
#include "SampleSynthEngine.h"
#include "SynthEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <map>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 256;
    constexpr int kChannels = 2;

    void require (bool condition, const char* message)
    {
        if (! condition)
            throw std::runtime_error (message);
    }

    float peakAbs (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float value = data[sample];
                require (std::isfinite (value), "audio buffer contains a non-finite sample");
                peak = juce::jmax (peak, std::abs (value));
            }
        }
        return peak;
    }

    void fillSineInput (juce::AudioBuffer<float>& buffer, double frequency, int blockIndex)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto absoluteSample = blockIndex * buffer.getNumSamples() + sample;
                const auto phase = juce::MathConstants<double>::twoPi * frequency * (double) absoluteSample / kSampleRate;
                data[sample] = (float) (0.35 * std::sin (phase));
            }
        }
    }

    patchcraft::RenderContext makeContext (int inputChannels)
    {
        auto context = patchcraft::RenderContext::forBlock (kSampleRate, kBlockSize, kBlockSize,
                                                            inputChannels, kChannels, 120.0);
        context.isPlaying = true;
        return context;
    }

    void advanceContext (patchcraft::RenderContext& context)
    {
        context.timeInSamples += kBlockSize;
        context.timeInSeconds = (double) context.timeInSamples / context.sampleRate;
        context.ppqPosition += context.beatsPerBlock();
    }

    patchcraft::ParameterModel parametersForEngine (const juce::String& engineId)
    {
        patchcraft::ParameterModel parameters;
        if (engineId == "synth")
            parameters.loadSynthPalette();
        else if (engineId == "fx")
            parameters.loadEffectPalette();
        else
            parameters.loadSamplerPalette();
        return parameters;
    }

    class CountingEngine final : public patchcraft::IInstrumentEngine
    {
    public:
        const char* engineId() const override { return "counting"; }
        bool needsAudioInput() const override { return false; }
        void prepare (double, int, int) override {}
        void reset() override { activeNote = -1; }
        void noteOn (int midiNote, float velocity) override
        {
            activeNote = midiNote;
            noteOns.push_back (midiNote);
            noteVelocities.push_back (velocity);
            ++noteOnCount;
        }
        void noteOff (int midiNote) override
        {
            lastNoteOff = midiNote;
            noteOffs.push_back (midiNote);
            activeNote = -1;
            ++noteOffCount;
        }
        void allNotesOff() override { activeNote = -1; ++allNotesOffCount; }
        void setParameter (const juce::String& id, float value) override { parameters[id] = value; }
        void loadFromPack (const juce::File&, const patchcraft::SampleMap&) override {}
        void process (juce::AudioBuffer<float>&, int, int) override {}
        int getActiveVoiceCount() const noexcept override { return activeNote >= 0 ? 1 : 0; }

        int noteOnCount = 0;
        int noteOffCount = 0;
        int allNotesOffCount = 0;
        int activeNote = -1;
        int lastNoteOff = -1;
        std::vector<int> noteOns;
        std::vector<int> noteOffs;
        std::vector<float> noteVelocities;
        std::map<juce::String, float> parameters;
    };

    float renderRoutedEngine (patchcraft::IInstrumentEngine& engine,
                              const juce::String& engineId,
                              bool feedInput,
                              bool triggerNote)
    {
        auto parameters = parametersForEngine (engineId);
        patchcraft::DspGraph graph;
        graph.resetForEngine (engineId);

        patchcraft::DspRoutingEngine router;
        auto context = makeContext (feedInput ? kChannels : 0);
        router.prepare (context);
        router.bind (graph, parameters);

        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.setRenderContext (context);
        if (triggerNote)
            engine.noteOn (60, 0.85f);

        float peak = 0.0f;
        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        for (int block = 0; block < 10; ++block)
        {
            if (feedInput)
                fillSineInput (buffer, 220.0, block);
            else
                buffer.clear();

            router.processToEngine (engine, context);
            engine.setRenderContext (context);
            engine.process (buffer, 0, buffer.getNumSamples());
            router.captureAudioAnalysis (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
            advanceContext (context);
        }

        if (triggerNote)
            engine.noteOff (60);

        return peak;
    }

    juce::File createSmokeWav()
    {
        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftAudioSmokeTests");
        directory.createDirectory();

        auto file = directory.getChildFile ("smoke_sample.wav");
        file.deleteFile();

        juce::AudioBuffer<float> sampleBuffer (1, (int) (kSampleRate * 0.25));
        for (int sample = 0; sample < sampleBuffer.getNumSamples(); ++sample)
        {
            const auto phase = juce::MathConstants<double>::twoPi * 330.0 * (double) sample / kSampleRate;
            const auto envelope = 1.0 - (double) sample / (double) sampleBuffer.getNumSamples();
            sampleBuffer.setSample (0, sample, (float) (0.6 * std::sin (phase) * envelope));
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> output (file.createOutputStream());
        require (output != nullptr && output->openedOk(), "failed to create smoke-test WAV output stream");

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (output.get(), kSampleRate, 1, 16, {}, 0));
        require (writer != nullptr, "failed to create smoke-test WAV writer");
        output.release();
        require (writer->writeFromAudioSampleBuffer (sampleBuffer, 0, sampleBuffer.getNumSamples()),
                 "failed to write smoke-test WAV data");
        writer.reset();

        require (file.existsAsFile(), "smoke-test WAV was not written");
        return file;
    }

    juce::File createSegmentedSmokeWav()
    {
        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftAudioSmokeTests");
        directory.createDirectory();

        auto file = directory.getChildFile ("segmented_sample.wav");
        file.deleteFile();

        juce::AudioBuffer<float> sampleBuffer (1, (int) (kSampleRate * 0.50));
        sampleBuffer.clear();
        const int half = sampleBuffer.getNumSamples() / 2;
        for (int sample = half; sample < sampleBuffer.getNumSamples(); ++sample)
        {
            const auto phase = juce::MathConstants<double>::twoPi * 440.0 * (double) (sample - half) / kSampleRate;
            sampleBuffer.setSample (0, sample, (float) (0.7 * std::sin (phase)));
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> output (file.createOutputStream());
        require (output != nullptr && output->openedOk(), "failed to create segmented WAV output stream");
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (output.get(), kSampleRate, 1, 16, {}, 0));
        require (writer != nullptr, "failed to create segmented WAV writer");
        output.release();
        require (writer->writeFromAudioSampleBuffer (sampleBuffer, 0, sampleBuffer.getNumSamples()),
                 "failed to write segmented WAV data");
        return file;
    }

    void pass (const char* name)
    {
        std::cout << "[PASS] " << name << std::endl;
    }

    void smokeSynthWavetable()
    {
        patchcraft::SynthEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.setParameter ("wtEnabled", 1.0f);
        engine.setParameter ("wtTable", 8.0f);
        engine.setParameter ("wtLevel", 1.0f);
        engine.setParameter ("wtFrameCount", 4.0f);
        engine.setParameter ("wtFramePosition", 0.65f);

        for (int frame = 0; frame < 4; ++frame)
        {
            for (int point = 0; point < 32; ++point)
            {
                const auto phase = juce::MathConstants<double>::twoPi * (double) point / 32.0;
                const auto harmonic = (double) frame + 1.0;
                engine.setParameter ("wtFrame" + juce::String (frame) + "Shape" + juce::String (point),
                                     (float) std::sin (phase * harmonic));
            }
        }

        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        buffer.clear();
        engine.noteOn (60, 0.85f);
        for (int block = 0; block < 10; ++block)
        {
            buffer.clear();
            engine.process (buffer, 0, buffer.getNumSamples());
        }
        require (peakAbs (buffer) > 0.0001f, "synth wavetable smoke test produced silence");
        engine.noteOff (60);
        pass ("synth wavetable render");
    }

    void smokeSamplerWavLoad()
    {
        const auto file = createSmokeWav();

        patchcraft::SampleMap map;
        patchcraft::SampleZoneDef zone;
        zone.samplePath = file.getFileName();
        zone.rootNote = 60;
        zone.lowNote = 0;
        zone.highNote = 127;
        zone.lowVelocity = 1;
        zone.highVelocity = 127;
        map.add (zone);

        patchcraft::SampleSynthEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.loadFromMap (file.getParentDirectory(), map);
        require (engine.getLoadedSampleCount() == 1, "sampler did not load the smoke-test WAV");

        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        engine.noteOn (60, 0.9f);
        float peak = 0.0f;
        for (int block = 0; block < 10; ++block)
        {
            buffer.clear();
            engine.process (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
        }
        require (peak > 0.0001f, "sampler smoke test produced silence");
        engine.noteOff (60);
        pass ("sample engine WAV playback");
    }

    void smokeSamplerMidiSampleControls()
    {
        const auto file = createSegmentedSmokeWav();

        patchcraft::SampleMap map;
        patchcraft::SampleZoneDef zone;
        zone.samplePath = file.getFileName();
        zone.rootNote = 60;
        zone.lowNote = 0;
        zone.highNote = 127;
        zone.lowVelocity = 1;
        zone.highVelocity = 127;
        map.add (zone);

        auto renderPeak = [&] (float sampleStart, float sampleLength, float slice, float sliceCount)
        {
            patchcraft::SampleSynthEngine engine;
            engine.prepare (kSampleRate, kBlockSize, kChannels);
            engine.loadFromMap (file.getParentDirectory(), map);
            engine.setParameter ("attack", 0.001f);
            engine.setParameter ("sampleStart", sampleStart);
            engine.setParameter ("sampleLength", sampleLength);
            engine.setParameter ("sampleSlice", slice);
            engine.setParameter ("sampleSliceCount", sliceCount);
            juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
            buffer.clear();
            engine.noteOn (60, 1.0f);
            engine.process (buffer, 0, buffer.getNumSamples());
            return peakAbs (buffer);
        };

        const auto defaultPeak = renderPeak (0.0f, 1.0f, 0.0f, 1.0f);
        const auto shiftedPeak = renderPeak (0.55f, 0.40f, 0.0f, 1.0f);
        const auto slicePeak = renderPeak (0.0f, 1.0f, 2.0f, 4.0f);
        require (defaultPeak < 0.0001f, "default sample start unexpectedly skipped the silent segment");
        require (shiftedPeak > 0.01f, "sampleStart did not move playback into the audible segment");
        require (slicePeak > 0.01f, "sampleSlice/sampleSliceCount did not trigger an audible segment");
        pass ("sample MIDI start/slice controls");
    }

    void smokeSampleDrumPadsAndPerformanceMetadata()
    {
        patchcraft::SampleMap map;
        for (const auto& name : { "Deep Kick.wav", "Snare Tight.wav", "Closed Hat.wav", "Open Hat.wav", "Texture Hit.wav" })
        {
            patchcraft::SampleZoneDef zone;
            zone.samplePath = name;
            map.add (zone);
        }

        map.autoMapDrumPads (36, 16);
        const auto& zones = map.getZones();
        require (zones.size() == 5, "drum pad auto map changed zone count");
        require (zones[0].rootNote == 36 && zones[0].lowNote == 36 && zones[0].highNote == 36,
                 "kick did not map to C1 pad");
        require (zones[0].oneShot && zones[0].padIndex == 0 && zones[0].padLabel == "Kick",
                 "kick pad metadata was not assigned");
        require (zones[1].rootNote == 38 && zones[1].padIndex == 2,
                 "snare did not map to the expected drum pad");
        require (zones[2].chokeGroup == 1 && zones[3].chokeGroup == 1,
                 "hat samples did not share a choke group");
        require (zones[4].oneShot && zones[4].group == "Drum Pads",
                 "fallback drum pad did not become a one-shot drum zone");

        auto roundTrip = patchcraft::SampleZoneDef::fromVar (zones[2].toVar());
        require (roundTrip.padIndex == zones[2].padIndex
                 && roundTrip.padLabel == zones[2].padLabel
                 && roundTrip.chokeGroup == zones[2].chokeGroup
                 && roundTrip.oneShot == zones[2].oneShot
                 && roundTrip.triggerProbability == zones[2].triggerProbability,
                 "sample performance metadata did not serialize");

        pass ("sample drum pad metadata");
    }

    void smokeSamplerDrumPadRuntime()
    {
        const auto file = createSmokeWav();

        patchcraft::SampleMap map;
        patchcraft::SampleZoneDef first;
        first.samplePath = file.getFileName();
        first.rootNote = first.lowNote = first.highNote = 36;
        first.lowVelocity = 1;
        first.highVelocity = 127;
        first.oneShot = true;
        first.chokeGroup = 1;
        first.padIndex = 0;
        first.padLabel = "A";
        map.add (first);

        auto second = first;
        second.rootNote = second.lowNote = second.highNote = 37;
        second.padIndex = 1;
        second.padLabel = "B";
        map.add (second);

        patchcraft::SampleSynthEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.loadFromMap (file.getParentDirectory(), map);
        engine.noteOn (36, 1.0f);
        require (engine.getActiveVoiceCount() == 1, "drum pad note-on did not start a voice");
        engine.noteOn (37, 1.0f);
        require (engine.getActiveVoiceCount() == 1, "drum pad choke group did not stop the previous voice");
        engine.noteOff (37);
        require (engine.getActiveVoiceCount() == 1, "one-shot drum pad stopped on note-off");
        engine.allNotesOff();

        patchcraft::SampleMap probabilityMap;
        auto muted = first;
        muted.chokeGroup = 0;
        muted.triggerProbability = 0;
        probabilityMap.add (muted);

        engine.reset();
        engine.loadFromMap (file.getParentDirectory(), probabilityMap);
        engine.noteOn (36, 1.0f);
        require (engine.getActiveVoiceCount() == 0, "zero-percent trigger probability still started a voice");

        engine.reset();
        engine.loadFromMap (file.getParentDirectory(), map);
        engine.setParameter ("sampleGlitch", 1.0f);
        engine.setParameter ("sampleGlitchGrid", 8.0f);
        engine.noteOn (36, 1.0f);
        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        buffer.clear();
        engine.process (buffer, 0, buffer.getNumSamples());
        require (peakAbs (buffer) > 0.0001f, "glitch-enabled drum pad playback produced silence");

        pass ("sample drum pad runtime");
    }

    void smokeSampleImportNameParsing()
    {
        {
            const auto zone = patchcraft::SampleMap::inferZoneFromFile (
                juce::File ("808 Boom key60 vel 1 80 rr2.wav"), 48, 48, 53);
            require (zone.rootNote == 60, "sample import did not parse key60");
            require (zone.lowVelocity == 1 && zone.highVelocity == 80,
                     "sample import did not parse spaced velocity range");
            require (zone.roundRobinGroup == 1 && zone.roundRobinIndex == 2,
                     "sample import did not parse rr2");
        }

        {
            const auto zone = patchcraft::SampleMap::inferZoneFromFile (
                juce::File ("Pad Layer C#3 velocity 81-127 bpm120.wav"), 48, 48, 53);
            require (zone.rootNote == 61, "sample import did not parse C#3");
            require (zone.lowVelocity == 81 && zone.highVelocity == 127,
                     "sample import did not parse hyphen velocity range");
        }

        {
            const auto zone = patchcraft::SampleMap::inferZoneFromFile (
                juce::File ("Texture rootC2 v 1 64.wav"), 48, 48, 53);
            require (zone.rootNote == 48, "sample import did not parse rootC2");
            require (zone.lowVelocity == 1 && zone.highVelocity == 64,
                     "sample import did not parse v low high tokens");
        }

        {
            bool usedVelocityRange = false;
            const auto zone = patchcraft::SampleMap::inferZoneFromFile (
                juce::File ("Texture Long Name v2 no velocity label.wav"), 48, 48, 53, &usedVelocityRange);
            require (! usedVelocityRange, "sample import incorrectly parsed version marker v2 as velocity");
            require (zone.lowVelocity == 1 && zone.highVelocity == 127,
                     "sample import should keep full velocity when no explicit velocity tag exists");
        }

        {
            bool usedNamePitch = false;
            bool usedAudioPitch = false;
            const auto zone = patchcraft::SampleMap::inferZoneFromFileWithAudio (
                createSmokeWav(), 24, 24, 24, &usedNamePitch, &usedAudioPitch);
            require (! usedNamePitch, "unlabelled sample import incorrectly used filename pitch");
            require (usedAudioPitch, "sample import did not audio-detect unlabelled pitch");
            require (zone.rootNote >= 63 && zone.rootNote <= 65,
                     "audio pitch detection placed root unexpectedly");
        }

        pass ("sample import pitch parsing");
    }

    void smokeSampleAutoMapSingleRootDoesNotStretchStack()
    {
        patchcraft::SampleMap stackedMap;
        for (int i = 0; i < 4; ++i)
        {
            patchcraft::SampleZoneDef zone;
            zone.samplePath = juce::String ("stacked_") + juce::String (i) + ".wav";
            zone.rootNote = 24;
            zone.lowNote = 24;
            zone.highNote = 24;
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            stackedMap.add (zone);
        }

        stackedMap.autoMapByRootNotes();
        for (const auto& zone : stackedMap.getZones())
            require (zone.lowNote == zone.rootNote && zone.highNote == zone.rootNote,
                     "same-root multi-zone auto map stretched every zone across the keyboard");

        patchcraft::SampleMap singleZoneMap;
        patchcraft::SampleZoneDef zone;
        zone.samplePath = "single.wav";
        zone.rootNote = 60;
        zone.lowNote = 60;
        zone.highNote = 60;
        singleZoneMap.add (zone);
        singleZoneMap.autoMapByRootNotes();
        require (singleZoneMap.getZones().front().lowNote == 0
                 && singleZoneMap.getZones().front().highNote == 127,
                 "single-zone auto map should still stretch one sample for quick audition");

        pass ("sample single-root auto-map range handling");
    }

    void smokeFxSamplePreview()
    {
        patchcraft::EffectEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.setParameter ("drive", 0.35f);
        engine.setParameter ("delayMix", 0.15f);
        engine.setParameter ("mix", 1.0f);

        float peak = 0.0f;
        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        for (int block = 0; block < 10; ++block)
        {
            fillSineInput (buffer, 440.0, block);
            engine.process (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
        }
        require (peak > 0.0001f, "FX sample preview smoke test produced silence");
        pass ("FX sample preview processing");
    }

    void smokeFxLiveInput()
    {
        patchcraft::EffectEngine engine;
        const auto peak = renderRoutedEngine (engine, "fx", true, false);
        require (peak > 0.0001f, "FX live input smoke test produced silence");
        pass ("FX live input routing");
    }

    void smokeArpeggiatorRuntime()
    {
        patchcraft::DspBlock arpBlock;
        arpBlock.id = "arp_smoke";
        arpBlock.section = "mod";
        arpBlock.type = "arp";
        arpBlock.name = "Smoke ARP";
        arpBlock.enabled = true;
        arpBlock.values = {
            { "rate", 8.0f },
            { "sync", 0.0f },
            { "arpSteps", 4.0f },
            { "arpGate", 0.45f },
            { "arpPattern", 0.0f },
            { "arpOctaves", 1.0f },
            { "arpNote0", 0.0f },
            { "arpNote1", 4.0f },
            { "arpNote2", 7.0f },
            { "arpNote3", 12.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (arpBlock);

        patchcraft::ArpeggiatorRuntime arpeggiator;
        arpeggiator.bind (graph);
        require (arpeggiator.isEnabled(), "ARP runtime did not bind an enabled arp block");

        CountingEngine engine;
        auto context = makeContext (0);
        require (arpeggiator.handleNoteOn (engine, 60, 0.8f), "ARP did not consume note-on input");
        for (int block = 0; block < 80; ++block)
        {
            arpeggiator.process (engine, context);
            advanceContext (context);
        }

        require (engine.noteOnCount >= 2, "ARP did not emit sequenced note-on events");
        require (engine.noteOffCount >= 1, "ARP did not gate sequenced notes");
        require (arpeggiator.handleNoteOff (engine, 60), "ARP did not consume note-off input");
        require (engine.getActiveVoiceCount() == 0, "ARP left a note active after note-off");
        pass ("ARP note sequencing runtime");
    }

    void smokeMidiPlaygroundRuntime()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_smoke";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "MIDI Playground Smoke";
        block.enabled = true;
        block.values = {
            { "rate", 16.0f },
            { "sync", 0.0f },
            { "arpSteps", 4.0f },
            { "arpGate", 0.60f },
            { "arpPattern", 0.0f },
            { "arpOctaves", 1.0f },
            { "arpNote0", 0.0f },
            { "arpNote1", 2.0f },
            { "arpNote2", 4.0f },
            { "arpNote3", 7.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 1.0f },
            { "mpChordSize", 3.0f },
            { "mpProbability", 1.0f },
            { "mpSampleControl", 1.0f },
            { "sampleSliceCount", 8.0f },
            { "sampleLength", 0.25f },
            { "mpVelocity0", 0.50f },
            { "mpVelocity1", 0.75f },
            { "mpSampleSlice0", 3.0f },
            { "mpStep2On", 0.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "MIDI Playground runtime did not bind an enabled generator block");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 60, 0.8f), "MIDI Playground did not consume note-on input");
        runtime.process (engine, context);
        require (engine.parameters["sampleSliceCount"] == 8.0f && engine.parameters["sampleSlice"] == 3.0f,
                 "MIDI Playground did not push sample slice controls into the engine");
        advanceContext (context);
        for (int blockIndex = 1; blockIndex < 80; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (engine.noteOnCount >= 6, "MIDI Playground did not emit chord/phrase note events");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 60) != engine.noteOns.end(),
                 "MIDI Playground did not emit the root chord note");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 64) != engine.noteOns.end(),
                 "MIDI Playground did not emit a major third from the scale engine");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 67) != engine.noteOns.end(),
                 "MIDI Playground did not emit a fifth from the chord engine");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 66) == engine.noteOns.end(),
                 "MIDI Playground emitted a note outside the selected major scale");
        require (runtime.handleNoteOff (engine, 60), "MIDI Playground did not consume note-off input");
        require (engine.getActiveVoiceCount() == 0, "MIDI Playground left a note active after note-off");
        pass ("MIDI Playground chord/scale phrase runtime");
    }

    void smokeMidiPlaygroundDrumMachineRuntime()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_drum_machine";
        block.section = "mod";
        block.type = "drumMachine";
        block.name = "Drum Machine Smoke";
        block.enabled = true;
        block.values = {
            { "rate", 4.0f },
            { "sync", 0.0f },
            { "dmTracks", 4.0f },
            { "dmSteps", 16.0f },
            { "dmPattern", 0.0f },
            { "dmTransport", 1.0f },
            { "dmProbability", 1.0f },
            { "dmTrack0Note", 36.0f },
            { "dmTrack1Note", 38.0f },
            { "dmTrack2Note", 42.0f },
            { "dmTrack3Note", 46.0f }
        };

        auto setCell = [&] (int track, int step, float velocity)
        {
            const auto prefix = "dmP0T" + juce::String (track) + "S" + juce::String (step);
            block.values[prefix + "On"] = 1.0f;
            block.values[prefix + "Vel"] = velocity;
            block.values[prefix + "Gate"] = 0.35f;
            block.values[prefix + "Prob"] = 1.0f;
        };

        for (int step : { 0, 4, 8, 12 }) setCell (0, step, 1.0f);
        for (int step : { 4, 12 }) setCell (1, step, 0.85f);
        for (int step = 0; step < 16; step += 2) setCell (2, step, 0.60f);

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "MIDI Playground drum machine did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (! runtime.handleNoteOn (engine, 36, 1.0f),
                 "drum machine should not consume live drum-pad note input");
        for (int blockIndex = 0; blockIndex < 120; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (engine.noteOnCount >= 8, "drum machine did not emit pattern note events");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 36) != engine.noteOns.end(),
                 "drum machine did not trigger kick note");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 38) != engine.noteOns.end(),
                 "drum machine did not trigger snare note");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 42) != engine.noteOns.end(),
                 "drum machine did not trigger hat note");
        require (engine.noteOffCount >= 1, "drum machine did not gate triggered notes");

        context.isPlaying = false;
        runtime.process (engine, context);
        pass ("MIDI Playground drum machine runtime");
    }

    void smokeMidiPlaygroundChordPresetRuntime()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_chord_preset";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "Dominant 7 Chord Preset";
        block.enabled = true;
        block.values = {
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpPattern", 0.0f },
            { "arpNote0", 0.0f },
            { "mpStep0On", 1.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 15.0f },
            { "mpChordSize", 4.0f },
            { "mpProbability", 1.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "MIDI chord preset runtime did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 60, 1.0f), "MIDI chord preset did not consume note-on");
        runtime.process (engine, context);

        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 60) != engine.noteOns.end(),
                 "dominant 7 preset did not emit root");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 64) != engine.noteOns.end(),
                 "dominant 7 preset did not emit major third");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 67) != engine.noteOns.end(),
                 "dominant 7 preset did not emit fifth");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 70) != engine.noteOns.end(),
                 "dominant 7 preset did not emit flat seventh");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 71) == engine.noteOns.end(),
                 "exact dominant 7 preset was incorrectly scale-quantized to major seventh");

        pass ("MIDI Playground exact chord preset runtime");
    }

    void smokeMidiPlaygroundTransformers()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_transformers";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "MIDI Playground Transformers";
        block.enabled = true;
        block.values = {
            { "rate", 16.0f },
            { "sync", 0.0f },
            { "arpSteps", 2.0f },
            { "arpGate", 0.90f },
            { "arpPattern", 0.0f },
            { "arpOctaves", 1.0f },
            { "arpNote0", 0.0f },
            { "arpNote1", 24.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpStepProb0", 0.0f },
            { "mpStepProb1", 1.0f },
            { "mpVelocity1", 0.40f },
            { "mpVelocityCurve", 1.0f },
            { "mpRatchet", 3.0f },
            { "mpMutation", 1.0f },
            { "mpOctaveFold", 1.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "MIDI Playground transformer runtime did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 60, 1.0f), "MIDI Playground transformers did not consume note-on");
        for (int blockIndex = 0; blockIndex < 80; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (engine.noteOnCount >= 3, "MIDI ratchet transformer did not emit repeated note events");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 60) == engine.noteOns.end(),
                 "per-step probability did not suppress the disabled first step");
        for (const auto note : engine.noteOns)
            require (note >= 48 && note <= 72, "MIDI octave fold allowed transformed notes outside the playable octave window");
        for (const auto velocity : engine.noteVelocities)
            require (velocity < 0.20f, "MIDI velocity curve did not reshape step velocity");

        pass ("MIDI Playground transformer runtime");
    }

    void smokeMidiPlaygroundTimingTransformers()
    {
        patchcraft::DspBlock strumBlock;
        strumBlock.id = "midi_playground_strum";
        strumBlock.section = "mod";
        strumBlock.type = "midiPlayground";
        strumBlock.name = "MIDI Playground Strum";
        strumBlock.enabled = true;
        strumBlock.values = {
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpPattern", 0.0f },
            { "arpNote0", 0.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 1.0f },
            { "mpChordSize", 3.0f },
            { "mpProbability", 1.0f },
            { "mpStrum", 0.50f },
            { "mpFlam", 0.0f }
        };

        patchcraft::DspGraph strumGraph;
        strumGraph.blocks.push_back (strumBlock);

        patchcraft::MidiPlaygroundRuntime strumRuntime;
        strumRuntime.bind (strumGraph);
        CountingEngine strumEngine;
        auto strumContext = makeContext (0);
        require (strumRuntime.handleNoteOn (strumEngine, 60, 1.0f), "MIDI strum transformer did not consume note-on");
        strumRuntime.process (strumEngine, strumContext);
        require (strumEngine.noteOnCount == 1, "MIDI strum emitted the whole chord immediately");
        for (int blockIndex = 0; blockIndex < 90; ++blockIndex)
        {
            advanceContext (strumContext);
            strumRuntime.process (strumEngine, strumContext);
        }
        require (strumEngine.noteOnCount >= 3, "MIDI strum did not stagger the remaining chord notes");
        require (std::find (strumEngine.noteOns.begin(), strumEngine.noteOns.end(), 64) != strumEngine.noteOns.end()
                 && std::find (strumEngine.noteOns.begin(), strumEngine.noteOns.end(), 67) != strumEngine.noteOns.end(),
                 "MIDI strum did not emit the expected chord tones");

        patchcraft::DspBlock euclideanBlock;
        euclideanBlock.id = "midi_playground_euclidean";
        euclideanBlock.section = "mod";
        euclideanBlock.type = "midiPlayground";
        euclideanBlock.name = "MIDI Playground Euclidean";
        euclideanBlock.enabled = true;
        euclideanBlock.values = {
            { "rate", 8.0f },
            { "sync", 0.0f },
            { "arpSteps", 8.0f },
            { "arpGate", 0.45f },
            { "arpPattern", 0.0f },
            { "mpScaleType", 0.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpEuclideanPulses", 2.0f },
            { "mpEuclideanRotate", 0.0f }
        };
        for (int step = 0; step < 8; ++step)
            euclideanBlock.values["arpNote" + juce::String (step)] = (float) step;

        patchcraft::DspGraph euclideanGraph;
        euclideanGraph.blocks.push_back (euclideanBlock);

        patchcraft::MidiPlaygroundRuntime euclideanRuntime;
        euclideanRuntime.bind (euclideanGraph);
        CountingEngine euclideanEngine;
        auto euclideanContext = makeContext (0);
        require (euclideanRuntime.handleNoteOn (euclideanEngine, 60, 1.0f), "MIDI Euclidean transformer did not consume note-on");
        for (int blockIndex = 0; blockIndex < 90; ++blockIndex)
        {
            euclideanRuntime.process (euclideanEngine, euclideanContext);
            advanceContext (euclideanContext);
        }

        require (euclideanEngine.noteOnCount >= 2, "MIDI Euclidean mask emitted no active pulses");
        for (const auto note : euclideanEngine.noteOns)
            require (note == 60 || note == 64, "MIDI Euclidean mask allowed a non-pulse step through");

        pass ("MIDI Playground strum/flam and Euclidean timing");
    }

    void smokeMidiPlaygroundPhraseBanksAndExport()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_phrase_bank";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "Phrase Bank Export";
        block.enabled = true;
        block.values = {
            { "arpSteps", 4.0f },
            { "arpGate", 0.60f },
            { "arpPattern", 0.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 1.0f },
            { "mpChordSize", 3.0f },
            { "arpNote0", 0.0f },
            { "arpNote1", 2.0f },
            { "arpNote2", 4.0f },
            { "arpNote3", 7.0f },
            { "mpVelocity0", 0.80f },
            { "mpVelocity1", 0.70f },
            { "mpVelocity2", 0.90f },
            { "mpVelocity3", 0.75f }
        };

        for (int step = 0; step < 16; ++step)
        {
            block.values["mpStep" + juce::String (step) + "On"] = step < 4 ? 1.0f : 0.0f;
            block.values["mpGate" + juce::String (step)] = 0.50f;
            block.values["mpStepProb" + juce::String (step)] = 1.0f;
            block.values["mpSampleSlice" + juce::String (step)] = -1.0f;
        }

        patchcraft::MidiPlaygroundPattern::storeActiveBank (block, 0);
        block.values["arpNote0"] = 12.0f;
        block.values["mpVelocity0"] = 0.45f;
        patchcraft::MidiPlaygroundPattern::storeActiveBank (block, 1);
        patchcraft::MidiPlaygroundPattern::loadBank (block, 0, false);
        require (std::abs (block.values["arpNote0"] - 0.0f) < 0.0001f,
                 "MIDI phrase bank did not restore stored step notes");
        patchcraft::MidiPlaygroundPattern::copyBank (block, 1, 2);
        patchcraft::MidiPlaygroundPattern::loadBank (block, 2, false);
        require (std::abs (block.values["arpNote0"] - 12.0f) < 0.0001f,
                 "MIDI phrase bank copy did not restore duplicated step notes");

        patchcraft::DspBlock progressionBlock;
        progressionBlock.id = "midi_playground_progression";
        progressionBlock.section = "mod";
        progressionBlock.type = "midiPlayground";
        progressionBlock.targetId = "filterCutoff";
        progressionBlock.values["mpScaleRoot"] = 0.0f;
        patchcraft::MidiPlaygroundPattern::applyProgressionPreset (progressionBlock, 0, 0);
        require (progressionBlock.values["arpSteps"] == 16.0f,
                 "MIDI progression helper did not create a 16-step phrase");
        require (progressionBlock.values["mpChordSize"] >= 3.0f,
                 "MIDI progression helper did not enable chord expansion");
        require (progressionBlock.values["mpStep0On"] > 0.5f
                 && progressionBlock.values["mpStep4On"] > 0.5f
                 && progressionBlock.values["mpStep8On"] > 0.5f
                 && progressionBlock.values["mpStep12On"] > 0.5f,
                 "MIDI progression helper did not enable chord change steps");
        require (progressionBlock.values["mpStep1On"] < 0.5f,
                 "MIDI progression helper left non-change filler steps active");

        auto midiFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftMidiPlaygroundExport.mid");
        midiFile.deleteFile();

        juce::String error;
        require (patchcraft::MidiPlaygroundPattern::writeMidiClip (progressionBlock, midiFile, 120.0, 60, error),
                 "MIDI Playground clip export failed");
        require (midiFile.existsAsFile() && midiFile.getSize() > 0,
                 "MIDI Playground clip export did not write a file");

        juce::FileInputStream input (midiFile);
        juce::MidiFile parsed;
        require (input.openedOk() && parsed.readFrom (input),
                 "MIDI Playground exported clip could not be read back");
        require (parsed.getNumTracks() >= 2, "MIDI Playground export did not include tempo and note tracks");

        pass ("MIDI Playground phrase banks and MIDI export");
    }

    void smokeMidiPlaygroundAdvancedRuntimeAndExport()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_advanced";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "Advanced MIDI Playground";
        block.enabled = true;
        block.values = {
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpPattern", 0.0f },
            { "mpScaleType", 0.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpKeySwitchEnabled", 1.0f },
            { "mpKeySwitchBase", 24.0f },
            { "mpEchoRepeats", 2.0f },
            { "mpEchoDelay", 0.05f },
            { "mpEchoDecay", 0.50f }
        };

        for (int bank = 0; bank < 2; ++bank)
        {
            const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
            block.values[prefix + "arpNote0"] = bank == 0 ? 0.0f : 12.0f;
            block.values[prefix + "mpStep0On"] = 1.0f;
            block.values[prefix + "mpVelocity0"] = 1.0f;
            block.values[prefix + "mpGate0"] = 1.0f;
            block.values[prefix + "mpStepProb0"] = 1.0f;
            block.values[prefix + "mpSampleSlice0"] = -1.0f;
        }

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "advanced MIDI Playground runtime did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 25, 1.0f), "MIDI key switch did not consume bank select note");
        require (runtime.handleNoteOn (engine, 60, 1.0f), "MIDI Playground did not consume played note after key switch");
        for (int blockIndex = 0; blockIndex < 40; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 72) != engine.noteOns.end(),
                 "MIDI key switch did not switch to the selected phrase bank");
        require (engine.noteOnCount >= 2, "MIDI echo performer did not retrigger notes inside the step");

        patchcraft::DspBlock exactChord;
        exactChord.id = "midi_playground_export_exact";
        exactChord.section = "mod";
        exactChord.type = "midiPlayground";
        exactChord.enabled = true;
        exactChord.values = {
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpNote0", 0.0f },
            { "mpStep0On", 1.0f },
            { "mpScaleRoot", 0.0f },
            { "mpScaleType", 1.0f },
            { "mpChordMode", 15.0f },
            { "mpChordSize", 4.0f },
            { "mpVelocity0", 1.0f },
            { "mpGate0", 1.0f },
            { "mpStepProb0", 1.0f }
        };

        auto chordFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftMidiPlaygroundExactChordExport.mid");
        chordFile.deleteFile();
        juce::String error;
        require (patchcraft::MidiPlaygroundPattern::writeMidiClip (exactChord, chordFile, 120.0, 60, error),
                 "exact MIDI chord export failed");

        juce::FileInputStream chordInput (chordFile);
        juce::MidiFile parsedChord;
        require (chordInput.openedOk() && parsedChord.readFrom (chordInput),
                 "exact MIDI chord export could not be read back");
        bool hasFlatSeven = false;
        bool hasMajorSeven = false;
        for (int track = 0; track < parsedChord.getNumTracks(); ++track)
            if (const auto* sequence = parsedChord.getTrack (track))
                for (int event = 0; event < sequence->getNumEvents(); ++event)
                    if (const auto* holder = sequence->getEventPointer (event))
                    {
                        const auto message = holder->message;
                        if (message.isNoteOn())
                        {
                            hasFlatSeven = hasFlatSeven || message.getNoteNumber() == 70;
                            hasMajorSeven = hasMajorSeven || message.getNoteNumber() == 71;
                        }
                    }
        require (hasFlatSeven && ! hasMajorSeven, "exact chord MIDI export quantized a dominant 7 to major 7");

        patchcraft::DspBlock drum;
        drum.id = "midi_playground_drum_export";
        drum.section = "mod";
        drum.type = "drumMachine";
        drum.enabled = true;
        drum.values = {
            { "dmTracks", 2.0f },
            { "dmSteps", 4.0f },
            { "dmPattern", 0.0f },
            { "dmTrack0Note", 36.0f },
            { "dmTrack1Note", 38.0f }
        };
        drum.values["dmP0T0S0On"] = 1.0f;
        drum.values["dmP0T0S0Vel"] = 1.0f;
        drum.values["dmP0T0S0Gate"] = 0.40f;
        drum.values["dmP0T1S2On"] = 1.0f;
        drum.values["dmP0T1S2Vel"] = 0.85f;
        drum.values["dmP0T1S2Gate"] = 0.35f;

        auto drumFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftMidiPlaygroundDrumExport.mid");
        drumFile.deleteFile();
        require (patchcraft::MidiPlaygroundPattern::writeMidiClip (drum, drumFile, 120.0, 60, error),
                 "drum machine MIDI export failed");

        juce::FileInputStream drumInput (drumFile);
        juce::MidiFile parsedDrum;
        require (drumInput.openedOk() && parsedDrum.readFrom (drumInput),
                 "drum machine MIDI export could not be read back");
        bool hasKick = false;
        bool hasSnare = false;
        for (int track = 0; track < parsedDrum.getNumTracks(); ++track)
            if (const auto* sequence = parsedDrum.getTrack (track))
                for (int event = 0; event < sequence->getNumEvents(); ++event)
                    if (const auto* holder = sequence->getEventPointer (event))
                    {
                        const auto message = holder->message;
                        if (message.isNoteOn())
                        {
                            hasKick = hasKick || message.getNoteNumber() == 36;
                            hasSnare = hasSnare || message.getNoteNumber() == 38;
                        }
                    }
        require (hasKick && hasSnare, "drum machine MIDI export did not include mapped drum notes");

        pass ("MIDI Playground advanced runtime and MIDI export");
    }

    void smokeMidiPlaygroundDspModulationRouting()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_dsp_mod";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "MIDI DSP Modulation";
        block.targetId = "filterCutoff";
        block.enabled = true;
        block.values = {
            { "amount", 0.75f },
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpNote0", 24.0f },
            { "mpStep0On", 1.0f },
            { "mpVelocity0", 1.0f },
            { "mpGate0", 1.0f },
            { "mpStepProb0", 1.0f },
            { "mpModLane", 0.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        auto parameters = parametersForEngine ("synth");
        patchcraft::DspRoutingEngine router;
        auto context = makeContext (0);
        router.prepare (context);
        router.bind (graph, parameters);
        router.setParameterValue ("filterCutoff", 1000.0f);

        CountingEngine engine;
        router.processToEngine (engine, context);
        require (engine.parameters.count ("filterCutoff") != 0,
                 "MIDI Playground modulation did not write the target parameter");
        require (engine.parameters["filterCutoff"] > 1000.0f,
                 "MIDI Playground output did not modulate the DSP target parameter");

        pass ("MIDI Playground DSP modulation routing");
    }

    void smokePlayerInstrumentFactory()
    {
        auto engine = patchcraft::createEngineFromManifest ("synth");
        require (engine != nullptr && juce::String (engine->engineId()) == "synth",
                 "Player instrument factory did not create a synth engine");
        const auto peak = renderRoutedEngine (*engine, "synth", false, true);
        require (peak > 0.0001f, "Player instrument smoke test produced silence");
        pass ("Player instrument factory render");
    }

    void smokePlayerFxFactory()
    {
        auto engine = patchcraft::createEngineFromManifest ("fx");
        require (engine != nullptr && juce::String (engine->engineId()) == "fx",
                 "Player FX factory did not create an FX engine");
        const auto peak = renderRoutedEngine (*engine, "fx", true, false);
        require (peak > 0.0001f, "Player FX smoke test produced silence");
        pass ("Player FX factory render");
    }

    void smokeTypedGraphEdges()
    {
        patchcraft::DspGraph graph;
        graph.resetForEngine ("synth");
        const auto edges = graph.buildAudioEdges ("synth");
        require (! edges.empty(), "typed graph did not build audio edges");

        bool hasOutputEdge = false;
        for (const auto& edge : edges)
            if (edge.targetNodeId == "output_utility")
                hasOutputEdge = true;
        require (hasOutputEdge, "typed graph has no route to the output utility");

        for (const auto& issue : graph.validateTypedGraph ("synth"))
            require (issue.severity != "error", "default synth graph validation produced an error");

        patchcraft::DspGraph restored;
        restored.fromVar (graph.toVar());
        require (! restored.buildAudioEdges ("synth").empty(), "typed graph edges were not serialized/restored");

        patchcraft::DspGraphEdge bad;
        bad.id = "unsafe_loop";
        bad.sourceNodeId = "filter_1";
        bad.targetNodeId = "filter_1";
        restored.edges.push_back (bad);
        bool foundSelfRoute = false;
        for (const auto& issue : restored.validateTypedGraph ("synth"))
            if (issue.severity == "error" && issue.ownerId == "unsafe_loop")
                foundSelfRoute = true;
        require (foundSelfRoute, "typed graph validation did not catch self-routing edge");
        pass ("typed graph edge validation");
    }

    void smokeAdvancedFxProcessors()
    {
        patchcraft::EffectEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.setParameter ("mix", 1.0f);
        engine.setParameter ("dynMix", 0.35f);
        engine.setParameter ("dynThresholdDb", -30.0f);
        engine.setParameter ("chorusMix", 0.25f);
        engine.setParameter ("phaserMix", 0.20f);
        engine.setParameter ("combMix", 0.20f);
        engine.setParameter ("resonatorMix", 0.20f);
        engine.setParameter ("convolutionMix", 0.15f);
        engine.setParameter ("spectralMix", 0.25f);
        engine.setParameter ("spectralTilt", 0.4f);

        float peak = 0.0f;
        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        for (int block = 0; block < 10; ++block)
        {
            fillSineInput (buffer, 330.0, block);
            engine.process (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
        }
        require (peak > 0.0001f, "advanced FX processor smoke test produced silence");
        pass ("advanced FX processor render");
    }

    void smokeStudioPreviewProjectRender()
    {
        patchcraft::PatchCraftProject project;
        auto engine = patchcraft::createEngineFromManifest (project.getEngineType());
        require (engine != nullptr, "Studio preview could not create an engine for the current project");

        patchcraft::DspRoutingEngine router;
        auto context = makeContext (0);
        router.prepare (context);
        router.bind (project.getDspGraph(), project.getParameters());
        router.syncFromLiveValues (project.getLiveValues());

        engine->prepare (kSampleRate, kBlockSize, kChannels);
        engine->setRenderContext (context);
        for (const auto& def : project.getParameters().getAll())
        {
            const auto value = project.getLiveValues().getValue (def.id, def.defaultValue);
            engine->setParameter (def.id, value);
            router.setParameterValue (def.id, value);
        }

        engine->noteOn (60, 0.85f);
        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        float peak = 0.0f;
        for (int block = 0; block < 240; ++block)
        {
            buffer.clear();
            router.processToEngine (*engine, context);
            engine->process (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
            advanceContext (context);
        }

        engine->noteOff (60);
        if (peak <= 0.05f)
        {
            std::cerr << "[DEBUG] preview engine=" << project.getEngineType()
                      << " blocks=" << project.getDspGraph().blocks.size()
                      << " params=" << project.getParameters().getAll().size()
                      << " peak=" << peak
                      << " volume=" << project.getLiveValues().getValue ("volume", -1.0f)
                      << " attack=" << project.getLiveValues().getValue ("attack", -1.0f)
                      << " cutoff=" << project.getLiveValues().getValue ("filterCutoff", -1.0f)
                      << " status=" << engine->getDiagnosticStatus()
                      << std::endl;
        }
        require (peak > 0.05f, "Studio preview project render is too quiet");
        pass ("Studio preview project render");
    }

    void smokeProjectSaveRestoresCurrentSound()
    {
        patchcraft::PatchCraftProject project;
        project.getLiveValues().setValue ("attack", 0.05f);
        project.getLiveValues().setValue ("volume", 1.05f);
        project.getLiveValues().setValue ("filterCutoff", 5200.0f);

        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftCurrentSoundSaveSmoke");
        directory.deleteRecursively();

        juce::String error;
        require (project.save (directory, error), "current sound project save failed");

        patchcraft::PatchCraftProject restored;
        require (restored.load (directory, error), "current sound project load failed");
        require (std::abs (restored.getLiveValues().getValue ("attack", -1.0f) - 0.05f) < 0.0001f,
                 "project load did not restore saved attack value");
        require (std::abs (restored.getLiveValues().getValue ("volume", -1.0f) - 1.05f) < 0.0001f,
                 "project load did not restore saved volume value");
        require (! restored.getPatches().empty() && restored.getPatches().front().parameterValues.count ("attack") != 0,
                 "project save did not capture the active playable patch");

        pass ("project save restores current sound");
    }

    void smokePresetAppliesLinkedPatchState()
    {
        patchcraft::PatchCraftProject project;
        project.getPatches().clear();
        project.getPresets().clear();

        auto patch = project.captureCurrentPatch ("Linked Easy MIDI Patch");
        patch.dspGraph.blocks.clear();
        patch.dspGraph.edges.clear();
        patch.dspGraph.macros.clear();
        patch.dspGraph.modulation.clear();
        patch.dspGraph.automation.clear();
        patch.dspGraph.userConfigured = true;

        patchcraft::DspBlock midiBlock;
        midiBlock.id = "linked_easy_midi";
        midiBlock.section = "mod";
        midiBlock.type = "midiPlayground";
        midiBlock.name = "Linked Easy MIDI";
        midiBlock.targetId = "filterCutoff";
        midiBlock.enabled = true;
        midiBlock.values["arpSteps"] = 16.0f;
        midiBlock.values["arpNote0"] = 7.0f;
        midiBlock.values["mpStepProb0"] = 0.75f;
        patch.dspGraph.blocks.push_back (midiBlock);
        patch.parameterValues["volume"] = 0.42f;
        patch.parameterValues["filterCutoff"] = 1234.0f;

        auto preset = patch.toPreset();
        project.getPatches().push_back (patch);
        project.getPresets().push_back (preset);

        project.getDspGraph().blocks.clear();
        project.getLiveValues().setValue ("volume", 0.99f);

        require (project.applyPreset (preset), "linked preset could not be applied");
        require (project.getDspGraph().blocks.size() == 1, "linked preset did not restore its DSP graph");
        require (project.getDspGraph().blocks.front().type == "midiPlayground",
                 "linked preset did not restore MIDI Playground block");
        require (std::abs (project.getDspGraph().blocks.front().values["arpNote0"] - 7.0f) < 0.0001f,
                 "linked preset lost MIDI note data");
        require (std::abs (project.getDspGraph().blocks.front().values["mpStepProb0"] - 0.75f) < 0.0001f,
                 "linked preset lost MIDI probability data");
        require (std::abs (project.getLiveValues().getValue ("volume", -1.0f) - 0.42f) < 0.0001f,
                 "linked preset did not restore patch parameter values");

        pass ("preset applies linked full patch state");
    }

    void smokePatchExpansionSerialization()
    {
        patchcraft::PatchCraftProject project;
        project.getPatches().clear();
        project.getSectionPresets().clear();
        project.getExpansions().clear();
        project.getPresets().clear();

        auto patch = project.captureCurrentPatch ("Phase 4 Playable Patch");
        patch.isDefault = true;
        require (patch.id.isNotEmpty(), "captured patch has no id");
        require (! patch.dspGraph.blocks.empty(), "captured patch has no DSP graph");

        auto preset = patch.toPreset();
        preset.isDefault = true;
        require (preset.patchId == patch.id, "patch preset did not retain patch id");

        auto sectionPreset = project.captureSectionPreset ("source", "Phase 4 Source Fragment");
        require (sectionPreset.id.isNotEmpty(), "section preset has no id");
        require (sectionPreset.section == "source", "section preset has wrong section");
        require (! sectionPreset.blocks.empty(), "section preset did not capture section blocks");

        auto& expansion = project.ensureExpansion ("Phase 4 Expansion");
        expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
        expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
        expansion.includedSectionPresetIds.addIfNotAlreadyThere (sectionPreset.id);

        project.getPatches().push_back (patch);
        project.getPresets().push_back (preset);
        project.getSectionPresets().push_back (sectionPreset);

        juce::String applyError;
        require (project.applySectionPreset (sectionPreset, true, applyError),
                 "section preset could not be applied back to project");

        auto patchRoundTrip = patchcraft::InstrumentPatch::fromVar (patch.toVar());
        auto sectionRoundTrip = patchcraft::SectionPreset::fromVar (sectionPreset.toVar());
        auto expansionRoundTrip = patchcraft::ExpansionMetadata::fromVar (expansion.toVar());
        require (patchRoundTrip.id == patch.id && patchRoundTrip.parameterValues.size() == patch.parameterValues.size(),
                 "patch var serialization lost state");
        require (sectionRoundTrip.id == sectionPreset.id && sectionRoundTrip.blocks.size() == sectionPreset.blocks.size(),
                 "section preset var serialization lost state");
        require (expansionRoundTrip.includedPatchIds.contains (patch.id, false),
                 "expansion var serialization lost patch membership");

        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftPhase4ProjectSmoke");
        directory.deleteRecursively();

        juce::String error;
        require (project.save (directory, error), "phase 4 project save failed");

        patchcraft::PatchCraftProject restored;
        require (restored.load (directory, error), "phase 4 project load failed");
        require (! restored.getPatches().empty(), "saved project lost playable patches");
        require (! restored.getSectionPresets().empty(), "saved project lost section presets");
        require (! restored.getExpansions().empty(), "saved project lost expansions");
        require (restored.getPresets().front().patchId.isNotEmpty(), "saved preset lost patch link");

        pass ("patch and expansion serialization");
    }

    void smokePatchExpansionPackExport()
    {
        patchcraft::PatchCraftProject project;

        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftPhase4PackSmoke");
        root.deleteRecursively();
        auto projectDirectory = root.getChildFile ("Project");
        auto packDirectory = root.getChildFile ("Exported.patchcraft");
        require (projectDirectory.createDirectory(), "failed to create phase 4 export project directory");

        project.setProjectFolder (projectDirectory);
        project.getLayout().clear();
        project.backgroundImageRelative = "assets/background.png";
        project.getManifest().backgroundImage = "assets/background.png";
        project.getManifest().libraryThumbnail.clear();
        project.getManifest().playerLogoImage.clear();

        auto assetsDirectory = projectDirectory.getChildFile ("assets");
        require (assetsDirectory.createDirectory(), "failed to create phase 4 export asset directory");
        require (assetsDirectory.getChildFile ("background.png").replaceWithText ("phase4-background"),
                 "failed to create phase 4 export background placeholder");

        project.getPatches().clear();
        project.getSectionPresets().clear();
        project.getExpansions().clear();
        project.getPresets().clear();

        auto patch = project.captureCurrentPatch ("Phase 4 Export Patch");
        patch.isDefault = true;
        auto preset = patch.toPreset();
        preset.isDefault = true;
        auto sectionPreset = project.captureSectionPreset ("source", "Phase 4 Export Source");
        auto& expansion = project.ensureExpansion ("Phase 4 Export Expansion");
        expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
        expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
        expansion.includedSectionPresetIds.addIfNotAlreadyThere (sectionPreset.id);
        for (const auto& asset : patch.includedAssets)
            expansion.includedAssets.addIfNotAlreadyThere (asset);

        project.getPatches().push_back (patch);
        project.getPresets().push_back (preset);
        project.getSectionPresets().push_back (sectionPreset);

        juce::String error;
        patchcraft::PatchCraftPackWriter writer;
        require (writer.write (project, packDirectory, error), "phase 4 pack export failed");
        require (packDirectory.getChildFile ("patches.json").existsAsFile(), "exported pack is missing patches.json");
        require (packDirectory.getChildFile ("sectionPresets.json").existsAsFile(), "exported pack is missing sectionPresets.json");
        require (packDirectory.getChildFile ("expansions.json").existsAsFile(), "exported pack is missing expansions.json");

        patchcraft::PatchCraftPack restoredPack;
        patchcraft::PatchCraftPackReader reader;
        require (reader.read (packDirectory, restoredPack, error), "phase 4 exported pack read failed");
        require (! restoredPack.patches.empty(), "exported pack lost patches");
        require (! restoredPack.sectionPresets.empty(), "exported pack lost section presets");
        require (! restoredPack.expansions.empty(), "exported pack lost expansions");
        require (restoredPack.findPatchForPreset (restoredPack.presets.front()) != nullptr,
                 "exported preset did not resolve to a linked patch");

        pass ("patch and expansion pack export");
    }

    void smokeLayoutRuntimeParityAndMalformedPacks()
    {
        const std::array<patchcraft::ElementType, 18> elementTypes {
            patchcraft::ElementType::Image,
            patchcraft::ElementType::Knob,
            patchcraft::ElementType::Slider,
            patchcraft::ElementType::Button,
            patchcraft::ElementType::Toggle,
            patchcraft::ElementType::Dropdown,
            patchcraft::ElementType::Label,
            patchcraft::ElementType::ValueDisplay,
            patchcraft::ElementType::Meter,
            patchcraft::ElementType::Waveform,
            patchcraft::ElementType::Keyboard,
            patchcraft::ElementType::Panel,
            patchcraft::ElementType::Shape,
            patchcraft::ElementType::XYPad,
            patchcraft::ElementType::TabPanel,
            patchcraft::ElementType::ScrollPanel,
            patchcraft::ElementType::Group,
            patchcraft::ElementType::Separator
        };

        patchcraft::CanvasSize canvas;
        patchcraft::LayoutModel layout;
        int index = 0;
        for (const auto type : elementTypes)
        {
            require (patchcraft::isPlayerRuntimeElementSupported (type),
                     "Studio element type has no Player runtime support flag");

            patchcraft::LayoutElement element;
            element.id = "phase8_" + juce::String (index++);
            element.type = type;
            element.label = patchcraft::elementTypeDisplayName (type);
            element.x = 20 * index;
            element.y = 12 * index;
            element.width = 96;
            element.height = 72;
            if (patchcraft::isRuntimeControlElement (type))
                element.parameterId = "volume";
            if (type == patchcraft::ElementType::XYPad)
                element.parameterId = "cutoff";
            if (type == patchcraft::ElementType::TabPanel)
            {
                element.tabs.add ("Main");
                element.tabs.add ("Motion");
            }

            const auto restored = patchcraft::LayoutElement::fromVar (element.toVar());
            require (restored.type == element.type, "layout element type did not round-trip");
            require (restored.id == element.id, "layout element id did not round-trip");
            layout.add (element);
        }

        patchcraft::LayoutModel restoredLayout;
        restoredLayout.fromVar (layout.toVar (canvas), canvas);
        require (restoredLayout.getAll().size() == layout.getAll().size(),
                 "layout model lost elements during serialization");

        patchcraft::PatchCraftProject project;
        project.setEngineType ("synth");
        project.backgroundImageRelative.clear();
        project.getManifest().backgroundImage.clear();
        project.getManifest().libraryThumbnail.clear();
        project.getManifest().playerLogoImage.clear();
        project.getPatches().clear();
        project.getPresets().clear();
        project.getExpansions().clear();
        const auto hostSlots = project.getParameters().buildHostParameterSlots();
        require (! hostSlots.empty(), "synth parameter registry produced no host slots");
        const auto controlParameterId = hostSlots.front().parameterId;

        project.getLayout().clear();
        for (auto element : layout.getAll())
        {
            if (element.parameterId.isNotEmpty())
                element.parameterId = controlParameterId;
            project.getLayout().add (element);
        }

        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftPhase8ParityPackSmoke");
        root.deleteRecursively();
        root.createDirectory();
        auto packDirectory = root.getChildFile ("Parity.patchcraft");

        juce::String error;
        patchcraft::PatchCraftPackWriter writer;
        const bool wrotePack = writer.write (project, packDirectory, error);
        if (! wrotePack)
            std::cerr << error << std::endl;
        require (wrotePack, "runtime parity pack export rejected a supported Studio element");

        patchcraft::PatchCraftPack restoredPack;
        patchcraft::PatchCraftPackReader reader;
        require (reader.read (packDirectory, restoredPack, error),
                 "runtime parity pack read failed");
        require (restoredPack.layout.getAll().size() == layout.getAll().size(),
                 "runtime parity pack lost layout elements");

        auto malformed = root.getChildFile ("Malformed.patchcraft");
        malformed.createDirectory();
        malformed.getChildFile ("manifest.json").replaceWithText ("{ bad manifest json");
        require (! reader.read (malformed, restoredPack, error),
                 "malformed pack unexpectedly loaded");
        require (error.containsIgnoreCase ("manifest.json"),
                 "malformed pack error did not identify the broken manifest");

        pass ("layout runtime parity and malformed pack rejection");
    }

    void smokePhysicalMidiAndModWheelCrashRepros()
    {
        patchcraft::MidiMapping modWheel;
        modWheel.parameterId = "modWheel";
        modWheel.sourceType = "cc";
        modWheel.controller = 1;
        require (modWheel.matches (juce::MidiMessage::controllerEvent (1, 1, 127)),
                 "mod wheel CC did not match MIDI mapping");
        require (modWheel.normalisedValueFromMessage (juce::MidiMessage::controllerEvent (1, 1, 127)) > 0.99f,
                 "mod wheel CC did not normalize to full scale");

        patchcraft::MidiMapping pitchWheel;
        pitchWheel.parameterId = "pitchWheel";
        pitchWheel.sourceType = "pitchWheel";
        pitchWheel.bipolar = true;
        require (pitchWheel.matches (juce::MidiMessage::pitchWheel (1, 16383)),
                 "pitch wheel did not match MIDI mapping");
        require (pitchWheel.normalisedValueFromMessage (juce::MidiMessage::pitchWheel (1, 0)) <= -0.99f,
                 "pitch wheel did not normalize as bipolar");

        patchcraft::MidiMapping aftertouch;
        aftertouch.parameterId = "aftertouch";
        aftertouch.sourceType = "aftertouch";
        require (aftertouch.matches (juce::MidiMessage::aftertouchChange (1, 60, 96)),
                 "poly aftertouch did not match MIDI mapping");

        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        patchcraft::SynthEngine synth;
        synth.prepare (kSampleRate, kBlockSize, kChannels);
        synth.setParameter ("volume", 1.0f);
        synth.setParameter ("expression", 1.0f);
        synth.setParameter ("modWheel", 1.0f);
        synth.setParameter ("pitchWheel", -1.0f);
        synth.setParameter ("aftertouch", 0.75f);
        synth.noteOn (0, 0.8f);
        synth.noteOn (127, 0.7f);
        for (int block = 0; block < 6; ++block)
        {
            buffer.clear();
            synth.process (buffer, 0, buffer.getNumSamples());
            peakAbs (buffer);
        }
        synth.noteOff (0);
        synth.noteOff (127);

        patchcraft::SampleSynthEngine sampler;
        sampler.prepare (kSampleRate, kBlockSize, kChannels);
        sampler.setParameter ("expression", 1.0f);
        sampler.setParameter ("modWheel", 1.0f);
        sampler.setParameter ("pitchWheel", 1.0f);
        sampler.setParameter ("aftertouch", 1.0f);
        sampler.noteOn (0, 0.8f);
        buffer.clear();
        sampler.process (buffer, 0, buffer.getNumSamples());
        peakAbs (buffer);
        sampler.noteOff (0);

        pass ("physical MIDI and mod wheel crash repros");
    }
}

int main()
{
    try
    {
        smokeSynthWavetable();
        smokeSamplerWavLoad();
        smokeSamplerMidiSampleControls();
        smokeSampleDrumPadsAndPerformanceMetadata();
        smokeSamplerDrumPadRuntime();
        smokeSampleImportNameParsing();
        smokeSampleAutoMapSingleRootDoesNotStretchStack();
        smokeFxSamplePreview();
        smokeFxLiveInput();
        smokeArpeggiatorRuntime();
        smokeMidiPlaygroundRuntime();
        smokeMidiPlaygroundDrumMachineRuntime();
        smokeMidiPlaygroundChordPresetRuntime();
        smokeMidiPlaygroundTransformers();
        smokeMidiPlaygroundTimingTransformers();
        smokeMidiPlaygroundPhraseBanksAndExport();
        smokeMidiPlaygroundAdvancedRuntimeAndExport();
        smokeMidiPlaygroundDspModulationRouting();
        smokePlayerInstrumentFactory();
        smokePlayerFxFactory();
        smokeTypedGraphEdges();
        smokeAdvancedFxProcessors();
        smokeStudioPreviewProjectRender();
        smokeProjectSaveRestoresCurrentSound();
        smokePresetAppliesLinkedPatchState();
        smokePatchExpansionSerialization();
        smokePatchExpansionPackExport();
        smokeLayoutRuntimeParityAndMalformedPacks();
        smokePhysicalMidiAndModWheelCrashRepros();
        std::cout << "PatchCraft audio smoke tests passed." << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] " << e.what() << std::endl;
        return 1;
    }
}
