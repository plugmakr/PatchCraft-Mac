#include "ArpeggiatorRuntime.h"
#include "DspRoutingEngine.h"
#include "EffectEngine.h"
#include "EngineFactory.h"
#include "AiAssistService.h"
#include "AiImageService.h"
#include "LicenseValidator.h"
#include "LibraryScanner.h"
#include "MidiPlaygroundRuntime.h"
#include "MidiPlaygroundPattern.h"
#include "MultiInstrumentEngine.h"
#include "ParameterModel.h"
#include "PatchCraftPackReader.h"
#include "PatchCraftPackWriter.h"
#include "PcexpManager.h"
#include "PluginClubPublisher.h"
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

    bool isVisibleFactoryDemoRuntimeElement (const patchcraft::LayoutElement& element)
    {
        if (! element.visible)
            return false;
        if (! (element.groupId.isEmpty() || element.groupId == "main"))
            return false;

        return patchcraft::isRuntimeControlElement (element.type)
            || element.type == patchcraft::ElementType::Keyboard
            || element.type == patchcraft::ElementType::DrumPad
            || element.type == patchcraft::ElementType::PadGrid
            || element.type == patchcraft::ElementType::DrumGrid
            || element.type == patchcraft::ElementType::ArpLane
            || element.type == patchcraft::ElementType::XYPad
            || element.type == patchcraft::ElementType::SampleDropZone
            || element.type == patchcraft::ElementType::RuntimeSampleLibrary;
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

    void smokeSamplerGranularVoiceEngine()
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

        patchcraft::SampleSynthEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.loadFromMap (file.getParentDirectory(), map);
        engine.setParameter ("attack", 0.001f);
        engine.setParameter ("release", 0.2f);
        engine.setParameter ("sampleStart", 0.55f);
        engine.setParameter ("sampleLength", 0.40f);
        engine.setParameter ("granularOn", 1.0f);
        engine.setParameter ("granularDensity", 42.0f);
        engine.setParameter ("granularSizeMs", 70.0f);
        engine.setParameter ("granularSpread", 0.22f);
        engine.setParameter ("granularPitchSpread", 5.0f);
        engine.setParameter ("granularPanSpread", 1.0f);
        engine.setParameter ("granularReverse", 0.45f);
        engine.setParameter ("granularDirection", 3.0f);
        engine.setParameter ("granularWindow", 2.0f);

        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        engine.noteOn (60, 1.0f);
        float peak = 0.0f;
        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();
            if (block == 4)
            {
                engine.setParameter ("granularScan", 0.55f);
                engine.setParameter ("granularTexture", 0.75f);
            }
            engine.process (buffer, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, peakAbs (buffer));
        }
        require (peak > 0.001f, "granular voice engine produced silence");
        require (engine.getActiveVoiceCount() > 0, "granular voice did not report an active voice");
        engine.noteOff (60);
        pass ("sample granular voice engine");
    }

    void smokeSamplerBpmSyncPlayback()
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
        zone.bpm = 240.0f;
        map.add (zone);

        auto renderPeak = [&] (bool sync, int blocks)
        {
            patchcraft::SampleSynthEngine engine;
            engine.prepare (kSampleRate, kBlockSize, kChannels);
            engine.setRenderContext (patchcraft::RenderContext::forBlock (kSampleRate, kBlockSize,
                                                                           kBlockSize, kChannels,
                                                                           kChannels, 120.0));
            engine.loadFromMap (file.getParentDirectory(), map);
            engine.setParameter ("attack", 0.001f);
            engine.setParameter ("bpmSync", sync ? 1.0f : 0.0f);
            juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
            engine.noteOn (60, 1.0f);
            float peak = 0.0f;
            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                engine.process (buffer, 0, buffer.getNumSamples());
                peak = juce::jmax (peak, peakAbs (buffer));
            }
            return peak;
        };

        require (renderPeak (false, 50) > 0.01f,
                 "unsynced sample did not reach the audible segment at normal speed");
        require (renderPeak (true, 50) < 0.0001f,
                 "BPM sync did not slow sample playback using zone BPM metadata");
        require (renderPeak (true, 95) > 0.01f,
                 "BPM-synced sample never reached the audible segment");
        pass ("sample BPM-synced playback");
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
                 "first dropped sample did not map to C1 pad");
        require (zones[0].oneShot && zones[0].padIndex == 0 && zones[0].padLabel == "Deep Kick",
                 "first pad metadata was not assigned from the sample file");
        require (zones[1].rootNote == 37 && zones[1].padIndex == 1,
                 "second dropped sample did not map to the next pad slot");
        require (zones[2].chokeGroup == 0 && zones[3].chokeGroup == 0,
                 "drum pad auto-map should not infer choke groups from filenames");
        require (zones[4].oneShot && zones[4].group == "Drum Pads",
                 "fallback drum pad did not become a one-shot drum zone");
        map.getZones()[2].bpm = 92.5f;
        map.getZones()[2].midiPath = "midi/hat-loop.mid";
        map.getZones()[2].midiPlaybackMode = "slice";
        map.getZones()[2].midiHostSync = true;
        map.getZones()[2].midiTranspose = -12;
        map.getZones()[2].midiVelocityAmount = 0.65f;

        auto roundTrip = patchcraft::SampleZoneDef::fromVar (zones[2].toVar());
        require (roundTrip.padIndex == zones[2].padIndex
                 && roundTrip.padLabel == zones[2].padLabel
                 && roundTrip.chokeGroup == zones[2].chokeGroup
                 && roundTrip.oneShot == zones[2].oneShot
                 && roundTrip.triggerProbability == zones[2].triggerProbability
                 && std::abs (roundTrip.bpm - zones[2].bpm) < 0.001f
                 && roundTrip.midiPath == zones[2].midiPath
                 && roundTrip.midiPlaybackMode == zones[2].midiPlaybackMode
                 && roundTrip.midiHostSync == zones[2].midiHostSync
                 && roundTrip.midiTranspose == zones[2].midiTranspose
                 && std::abs (roundTrip.midiVelocityAmount - zones[2].midiVelocityAmount) < 0.001f,
                 "sample performance metadata did not serialize");

        patchcraft::SampleMap stackedMap;
        for (const auto& name : { "Kick A.wav", "Kick B.wav", "Kick C.wav" })
        {
            patchcraft::SampleZoneDef zone;
            zone.samplePath = name;
            stackedMap.add (zone);
        }

        stackedMap.autoMapDrumPads (36, 1, true);
        const auto& stacked = stackedMap.getZones();
        require (stacked[0].rootNote == 36 && stacked[1].rootNote == 36 && stacked[2].rootNote == 36,
                 "stacked pad map did not keep all layers on the target pad");
        require (stacked[0].roundRobinGroup > 0 && stacked[1].roundRobinIndex == 2 && stacked[2].roundRobinIndex == 3,
                 "stacked pad map did not assign round-robin layer metadata");

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
            { "dmTriggerPadSlots", 0.0f },
            { "dmTrack0Note", 36.0f },
            { "dmTrack0FxTarget", 4.0f },
            { "dmTrack0FxAmount", 0.70f },
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
        block.values["dmP0T0S0FxTarget"] = 4.0f;
        block.values["dmP0T0S0FxAmount"] = 0.85f;
        block.values["dmP0T2S0Div"] = 4.0f;

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "MIDI Playground drum machine did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (! runtime.handleNoteOn (engine, 36, 1.0f),
                 "drum machine should not consume live drum-pad note input");
        require (engine.parameters["delayMix"] > 0.60f,
                 "drum pad note input did not trigger assigned FX amount");
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
        require (std::count (engine.noteOns.begin(), engine.noteOns.end(), 42) >= 4,
                 "drum machine cell divisions did not create repeated hat hits");
        require (engine.noteOffCount >= 1, "drum machine did not gate triggered notes");
        require (engine.parameters["delayMix"] > 0.01f,
                 "drum grid cell did not trigger assigned FX amount");

        context.isPlaying = false;
        runtime.process (engine, context);

        patchcraft::DspBlock padSlotBlock;
        padSlotBlock.id = "midi_playground_pad_slots";
        padSlotBlock.section = "mod";
        padSlotBlock.type = "drumMachine";
        padSlotBlock.enabled = true;
        padSlotBlock.values = {
            { "rate", 4.0f },
            { "sync", 0.0f },
            { "dmTracks", 2.0f },
            { "dmSteps", 4.0f },
            { "dmPattern", 0.0f },
            { "dmTransport", 1.0f },
            { "dmTrack1Note", 38.0f },
            { "dmP0T1S0On", 1.0f },
            { "dmP0T1S0Vel", 0.85f },
            { "dmP0T1S0Gate", 0.35f }
        };
        patchcraft::DspGraph padSlotGraph;
        padSlotGraph.blocks.push_back (padSlotBlock);
        patchcraft::MidiPlaygroundRuntime padSlotRuntime;
        padSlotRuntime.bind (padSlotGraph);
        CountingEngine padSlotEngine;
        auto padSlotContext = makeContext (0);
        for (int blockIndex = 0; blockIndex < 30; ++blockIndex)
        {
            padSlotRuntime.process (padSlotEngine, padSlotContext);
            advanceContext (padSlotContext);
        }
        require (std::find (padSlotEngine.noteOns.begin(), padSlotEngine.noteOns.end(), 37) != padSlotEngine.noteOns.end(),
                 "drum machine default did not trigger pad slot note for row 2");
        require (std::find (padSlotEngine.noteOns.begin(), padSlotEngine.noteOns.end(), 38) == padSlotEngine.noteOns.end(),
                 "drum machine default still used custom track note instead of pad slot");

        patchcraft::DspBlock divisionBlock;
        divisionBlock.id = "midi_playground_drum_divisions";
        divisionBlock.section = "mod";
        divisionBlock.type = "drumMachine";
        divisionBlock.enabled = true;
        divisionBlock.values = {
            { "rate", 2.0f },
            { "sync", 0.0f },
            { "dmTracks", 1.0f },
            { "dmSteps", 4.0f },
            { "dmPattern", 0.0f },
            { "dmTransport", 1.0f },
            { "dmTriggerPadSlots", 0.0f },
            { "dmTrack0Note", 42.0f },
            { "dmP0T0S0On", 1.0f },
            { "dmP0T0S0Vel", 0.80f },
            { "dmP0T0S0Gate", 0.20f },
            { "dmP0T0S0Div", 4.0f }
        };
        patchcraft::DspGraph divisionGraph;
        divisionGraph.blocks.push_back (divisionBlock);
        patchcraft::MidiPlaygroundRuntime divisionRuntime;
        divisionRuntime.bind (divisionGraph);
        CountingEngine divisionEngine;
        auto divisionContext = makeContext (0);
        for (int blockIndex = 0; blockIndex < 30; ++blockIndex)
        {
            divisionRuntime.process (divisionEngine, divisionContext);
            advanceContext (divisionContext);
        }
        require (std::count (divisionEngine.noteOns.begin(), divisionEngine.noteOns.end(), 42) >= 4,
                 "drum machine per-cell divisions did not retrigger inside one step");

        pass ("MIDI Playground drum machine runtime");
    }

    void smokeMidiPlaygroundSampleOverlayRuntime()
    {
        patchcraft::DspBlock block;
        block.id = "midi_playground_sample_overlay";
        block.section = "mod";
        block.type = "drumMachine";
        block.enabled = true;
        block.values = {
            { "rate", 4.0f },
            { "sync", 0.0f },
            { "dmTracks", 1.0f },
            { "dmSteps", 4.0f },
            { "dmPattern", 0.0f },
            { "dmTransport", 1.0f },
            { "dmTriggerPadSlots", 0.0f },
            { "sampleSliceCount", 4.0f },
            { "sampleStart", 0.10f },
            { "sampleLength", 0.35f },
            { "samplePitch", -3.0f },
            { "dmTrack0Note", 62.0f },
            { "dmP0T0S0On", 1.0f },
            { "dmP0T0S0Vel", 0.90f },
            { "dmP0T0S0Gate", 0.20f },
            { "dmP0T0S0SampleSlice", 2.0f }
        };

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);
        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);

        CountingEngine mainEngine;
        CountingEngine sampleOverlayEngine;
        auto context = makeContext (0);
        runtime.process (mainEngine, context, &sampleOverlayEngine);
        require (std::find (mainEngine.noteOns.begin(), mainEngine.noteOns.end(), 62) != mainEngine.noteOns.end(),
                 "drum machine did not trigger custom MIDI note on main engine");
        require (std::find (sampleOverlayEngine.noteOns.begin(), sampleOverlayEngine.noteOns.end(), 62) != sampleOverlayEngine.noteOns.end(),
                 "drum machine did not trigger runtime sample overlay");
        require (std::abs (sampleOverlayEngine.parameters["sampleSliceCount"] - 4.0f) < 0.001f
              && std::abs (sampleOverlayEngine.parameters["sampleSlice"] - 2.0f) < 0.001f
              && std::abs (sampleOverlayEngine.parameters["sampleStart"] - 0.10f) < 0.001f
              && std::abs (sampleOverlayEngine.parameters["sampleLength"] - 0.35f) < 0.001f
              && std::abs (sampleOverlayEngine.parameters["samplePitch"] + 3.0f) < 0.001f,
                 "drum machine did not apply per-cell sample slice controls before triggering overlay");

        context.isPlaying = false;
        runtime.process (mainEngine, context, &sampleOverlayEngine);
        require (std::find (sampleOverlayEngine.noteOffs.begin(), sampleOverlayEngine.noteOffs.end(), 62) != sampleOverlayEngine.noteOffs.end(),
                 "drum machine did not stop runtime sample overlay on transport stop");
        pass ("MIDI Playground sample overlay runtime");
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
        const auto progressionNames = patchcraft::MidiPlaygroundPattern::getProgressionNames();
        require (progressionNames.size() >= 18,
                 "MIDI Playground curated progression library lost its premium preset count");
        require (progressionNames.contains ("Cinematic Lift")
                 && progressionNames.contains ("Neo Soul Glow")
                 && progressionNames.contains ("Future Bass Anthem"),
                 "MIDI Playground curated progression library is missing flagship presets");
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

        patchcraft::DspBlock cinematicBlock;
        cinematicBlock.id = "midi_playground_cinematic_preset";
        cinematicBlock.section = "mod";
        cinematicBlock.type = "midiPlayground";
        cinematicBlock.targetId = "filterCutoff";
        cinematicBlock.values["mpScaleRoot"] = 0.0f;
        const auto cinematicIndex = progressionNames.indexOf ("Cinematic Lift");
        require (cinematicIndex >= 0, "MIDI Playground missing Cinematic Lift preset");
        patchcraft::MidiPlaygroundPattern::applyProgressionPreset (cinematicBlock, cinematicIndex, 0);
        require (cinematicBlock.values["mpStep1On"] > 0.5f
                 && cinematicBlock.values["mpStep2On"] > 0.5f,
                 "Cinematic MIDI preset did not create a playable multi-step phrase");
        require (cinematicBlock.values["arpGate"] < 0.9f
                 && cinematicBlock.values["mpGate1"] < 0.9f,
                 "Cinematic MIDI preset did not apply its musical timing profile");

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

        patchcraft::DspBlock longArp;
        longArp.id = "midi_playground_advanced_arp_64";
        longArp.section = "mod";
        longArp.type = "midiPlayground";
        longArp.name = "Advanced Arp Instrument";
        longArp.enabled = true;
        longArp.values = {
            { "rate", 64.0f },
            { "sync", 0.0f },
            { "arpSteps", 64.0f },
            { "arpGate", 0.90f },
            { "arpPattern", 0.0f },
            { "mpScaleType", 0.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpRatchet", 1.0f }
        };
        for (int step = 0; step < 64; ++step)
        {
            const auto suffix = juce::String (step);
            longArp.values["arpNote" + suffix] = (float) (step % 12);
            longArp.values["mpStep" + suffix + "On"] = 1.0f;
            longArp.values["mpVelocity" + suffix] = 1.0f;
            longArp.values["mpGate" + suffix] = 0.80f;
            longArp.values["mpStepProb" + suffix] = 1.0f;
            longArp.values["mpStepDiv" + suffix] = step == 0 ? 4.0f : 1.0f;
            longArp.values["mpStepTranspose" + suffix] = step == 32 ? 12.0f : 0.0f;
            longArp.values["mpStepChordMode" + suffix] = step == 16 ? 15.0f : -1.0f;
            longArp.values["mpStepChordSize" + suffix] = step == 16 ? 4.0f : -1.0f;
            longArp.values["mpStepTie" + suffix] = step == 8 ? 1.0f : 0.0f;
        }

        patchcraft::DspGraph longGraph;
        longGraph.blocks.push_back (longArp);
        patchcraft::MidiPlaygroundRuntime longRuntime;
        longRuntime.bind (longGraph);
        require (longRuntime.isEnabled(), "advanced 64-step arp runtime did not bind");
        CountingEngine longEngine;
        auto longContext = makeContext (0);
        require (longRuntime.handleNoteOn (longEngine, 60, 1.0f), "advanced 64-step arp did not consume note-on");
        for (int blockIndex = 0; blockIndex < 220; ++blockIndex)
        {
            longRuntime.process (longEngine, longContext);
            advanceContext (longContext);
        }
        require (longEngine.noteOnCount >= 68, "advanced 64-step arp did not emit extended sequence and per-step ratchets");
        require (std::find (longEngine.noteOns.begin(), longEngine.noteOns.end(), 80) != longEngine.noteOns.end(),
                 "advanced 64-step arp did not apply per-step transpose or octave motion");

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
            { "dmTriggerPadSlots", 0.0f },
            { "dmTrack0Note", 36.0f },
            { "dmTrack1Note", 38.0f }
        };
        drum.values["dmP0T0S0On"] = 1.0f;
        drum.values["dmP0T0S0Vel"] = 1.0f;
        drum.values["dmP0T0S0Gate"] = 0.40f;
        drum.values["dmP0T0S0Div"] = 3.0f;
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
        int kickEvents = 0;
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
                            if (message.getNoteNumber() == 36)
                                ++kickEvents;
                        }
                    }
        require (hasKick && hasSnare, "drum machine MIDI export did not include mapped drum notes");
        require (kickEvents >= 3, "drum machine MIDI export did not include cell divisions");

        pass ("MIDI Playground advanced runtime and MIDI export");
    }

    void smokeMidiPlaygroundActiveBankIsolation()
    {
        patchcraft::DspBlock block;
        block.id = "circle_seq_bank_isolation";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "CircleSEQ Bank Isolation";
        block.enabled = true;
        block.values = {
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpPattern", 0.0f },
            { "mpActiveBank", 1.0f },
            { "mpScaleType", 0.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpRatchet", 1.0f }
        };

        for (int bank = 0; bank < 2; ++bank)
        {
            const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
            block.values[prefix + "arpSteps"] = 1.0f;
            block.values[prefix + "arpNote0"] = bank == 0 ? 0.0f : 12.0f;
            block.values[prefix + "mpStep0On"] = 1.0f;
            block.values[prefix + "mpVelocity0"] = 1.0f;
            block.values[prefix + "mpGate0"] = 1.0f;
            block.values[prefix + "mpStepProb0"] = 1.0f;
            block.values[prefix + "mpSampleSlice0"] = -1.0f;
            block.values[prefix + "mpStepDiv0"] = 1.0f;
        }

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "CircleSEQ bank isolation runtime did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 60, 1.0f),
                 "CircleSEQ bank isolation runtime did not consume note-on");

        for (int blockIndex = 0; blockIndex < 12; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 72) != engine.noteOns.end(),
                 "CircleSEQ active bank did not play its selected lane note");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 60) == engine.noteOns.end(),
                 "CircleSEQ runtime layered an inactive lane instead of isolating the selected bank");
        require (runtime.handleNoteOff (engine, 60), "CircleSEQ bank isolation runtime did not consume note-off");

        pass ("MIDI Playground active bank isolation");
    }

    void smokeMidiPlaygroundMultiLanePlayback()
    {
        patchcraft::DspBlock block;
        block.id = "circle_seq_multi_lane";
        block.section = "mod";
        block.type = "midiPlayground";
        block.name = "CircleSEQ Multi Lane Playback";
        block.enabled = true;
        block.values = {
            { "rate", 1.0f },
            { "sync", 0.0f },
            { "arpSteps", 1.0f },
            { "arpGate", 1.0f },
            { "arpPattern", 0.0f },
            { "mpActiveBank", 0.0f },
            { "mpMultiLane", 1.0f },
            { "mpScaleType", 0.0f },
            { "mpChordMode", 0.0f },
            { "mpChordSize", 1.0f },
            { "mpProbability", 1.0f },
            { "mpRatchet", 1.0f }
        };

        for (int bank = 0; bank < 3; ++bank)
        {
            const auto prefix = "mpBank" + juce::String (bank + 1) + "_";
            block.values[prefix + "arpSteps"] = 1.0f;
            block.values[prefix + "arpNote0"] = bank == 0 ? 0.0f : bank == 1 ? 12.0f : 19.0f;
            block.values[prefix + "mpStep0On"] = 1.0f;
            block.values[prefix + "mpVelocity0"] = 1.0f;
            block.values[prefix + "mpGate0"] = 1.0f;
            block.values[prefix + "mpStepProb0"] = 1.0f;
            block.values[prefix + "mpSampleSlice0"] = -1.0f;
            block.values[prefix + "mpStepDiv0"] = 1.0f;
        }
        block.values["mpBank2_mpAutoFxSend0"] = 0.75f;
        block.values["mpBank3_mpLaneMute"] = 1.0f;

        patchcraft::DspGraph graph;
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime runtime;
        runtime.bind (graph);
        require (runtime.isEnabled(), "CircleSEQ multi-lane runtime did not bind");

        CountingEngine engine;
        auto context = makeContext (0);
        require (runtime.handleNoteOn (engine, 60, 1.0f),
                 "CircleSEQ multi-lane runtime did not consume note-on");

        for (int blockIndex = 0; blockIndex < 12; ++blockIndex)
        {
            runtime.process (engine, context);
            advanceContext (context);
        }

        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 60) != engine.noteOns.end(),
                 "CircleSEQ multi-lane playback did not trigger lane 1");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 72) != engine.noteOns.end(),
                 "CircleSEQ multi-lane playback did not layer lane 2");
        require (std::find (engine.noteOns.begin(), engine.noteOns.end(), 79) == engine.noteOns.end(),
                 "CircleSEQ multi-lane playback ignored lane mute");
        require (engine.parameters.count ("delayMix") != 0 && engine.parameters["delayMix"] > 0.70f,
                 "CircleSEQ multi-lane FX send did not reach the instrument engine");

        block.values["mpBank2_mpLaneSolo"] = 1.0f;
        graph.blocks.clear();
        graph.blocks.push_back (block);

        patchcraft::MidiPlaygroundRuntime soloRuntime;
        soloRuntime.bind (graph);
        CountingEngine soloEngine;
        context = makeContext (0);
        require (soloRuntime.handleNoteOn (soloEngine, 60, 1.0f),
                 "CircleSEQ solo-lane runtime did not consume note-on");
        for (int blockIndex = 0; blockIndex < 12; ++blockIndex)
        {
            soloRuntime.process (soloEngine, context);
            advanceContext (context);
        }

        require (std::find (soloEngine.noteOns.begin(), soloEngine.noteOns.end(), 72) != soloEngine.noteOns.end(),
                 "CircleSEQ lane solo did not trigger the soloed lane");
        require (std::find (soloEngine.noteOns.begin(), soloEngine.noteOns.end(), 60) == soloEngine.noteOns.end(),
                 "CircleSEQ lane solo allowed an unsoloed lane to play");

        pass ("MIDI Playground multi-lane playback");
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

    void smokePlayerFxGraphControlsStayLive()
    {
        patchcraft::DspGraph graph;
        graph.resetForEngine ("synth");

        auto parameters = parametersForEngine ("synth");
        patchcraft::DspRoutingEngine router;
        auto context = makeContext (0);
        router.prepare (context);
        router.bind (graph, parameters);

        router.setParameterValue ("delayMix", 0.82f);
        require (router.setFxBlockParameterValue ("delayMix", 0.82f),
                 "Player FX graph control did not find the delayMix block value");

        CountingEngine engine;
        router.processToEngine (engine, context);
        require (engine.parameters.count ("delayMix") != 0,
                 "Player FX graph control did not reach the audio engine");
        require (engine.parameters["delayMix"] > 0.75f,
                 "Player FX graph routing overwrote the live delayMix control");

        pass ("Player FX graph controls stay live");
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

    void smokeMultiInstrumentFactoryAndRouting()
    {
        patchcraft::Manifest manifest;
        manifest.engine = "multi";
        manifest.multiInstrumentMode = true;
        manifest.instrumentIds.add ("a");
        manifest.instrumentIds.add ("b");
        manifest.instrumentNames.add ("Layer A");
        manifest.instrumentNames.add ("Layer B");
        manifest.instrumentFiles.add ("instruments/a.json");
        manifest.instrumentFiles.add ("instruments/b.json");
        manifest.instrumentVolumes.add (0.7f);
        manifest.instrumentVolumes.add (0.4f);
        manifest.instrumentPans.add (-0.25f);
        manifest.instrumentPans.add (0.25f);
        manifest.instrumentAutoPlay.add (1);
        manifest.instrumentAutoPlay.add (0);
        manifest.instrumentAutoPlayNotes.add (60);
        manifest.instrumentAutoPlayNotes.add (60);
        manifest.instrumentAutoPlayVelocities.add (0.8f);
        manifest.instrumentAutoPlayVelocities.add (0.8f);
        const auto restoredManifest = patchcraft::Manifest::fromVar (manifest.toVar());
        require (restoredManifest.multiInstrumentMode
                 && restoredManifest.instrumentIds.size() == 2
                 && restoredManifest.instrumentNames[1] == "Layer B"
                 && restoredManifest.instrumentFiles[0] == "instruments/a.json"
                 && restoredManifest.instrumentVolumes.size() == 2
                 && std::abs (restoredManifest.instrumentVolumes[0] - 0.7f) < 0.0001f
                 && restoredManifest.instrumentAutoPlay[0] == 1,
                 "multi instrument manifest metadata did not serialize");

        require (patchcraft::engineTypeFromString ("multi") == patchcraft::EngineType::Multi,
                 "engine factory did not parse multi engine type");
        require (patchcraft::engineTypeToString (patchcraft::EngineType::Multi) == "multi",
                 "engine factory did not serialize multi engine type");

        auto factoryEngine = patchcraft::createEngineFromManifest ("multi");
        require (factoryEngine != nullptr && juce::String (factoryEngine->engineId()) == "multi",
                 "Player factory did not create a multi instrument engine");

        const auto sourceSample = createSmokeWav();
        auto packDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftMultiInstrumentPackSmoke");
        packDirectory.deleteRecursively();
        require (packDirectory.createDirectory(), "failed to create multi instrument pack directory");

        require (juce::JSON::toString (manifest.toVar(), true).isNotEmpty()
                 && packDirectory.getChildFile ("manifest.json").replaceWithText (juce::JSON::toString (manifest.toVar(), true)),
                 "failed to write multi instrument manifest");

        const auto samplesDirectory = packDirectory.getChildFile ("samples");
        require (samplesDirectory.createDirectory(), "failed to create multi instrument sample directory");
        require (sourceSample.copyFileTo (samplesDirectory.getChildFile ("sample.wav")),
                 "failed to copy shared multi instrument sample");

        const auto instrumentsDirectory = packDirectory.getChildFile ("instruments");
        require (instrumentsDirectory.createDirectory(), "failed to create multi instrument definitions directory");

        for (const auto& layerId : { juce::String ("a"), juce::String ("b") })
        {
            patchcraft::SampleMap layerMap;
            patchcraft::SampleZoneDef zone;
            zone.samplePath = "samples/sample.wav";
            zone.rootNote = 60;
            zone.lowNote = 0;
            zone.highNote = 127;
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            layerMap.add (zone);
            require (instrumentsDirectory.getChildFile (layerId + ".json")
                         .replaceWithText (juce::JSON::toString (layerMap.toVar(), true)),
                     "failed to write manifest-referenced layer sample map");
        }

        patchcraft::MultiInstrumentEngine loadedEngine;
        loadedEngine.prepare (kSampleRate, kBlockSize, kChannels);
        loadedEngine.loadFromPack (packDirectory, {});
        require (loadedEngine.getLayerCount() == 2, "multi pack did not load both layers");
        require (loadedEngine.getLoadedSampleCount() == 2, "multi pack did not load layer samples");
        juce::AudioBuffer<float> loadedBuffer (kChannels, kBlockSize);
        loadedBuffer.clear();
        loadedEngine.noteOn (60, 0.8f);
        loadedEngine.process (loadedBuffer, 0, loadedBuffer.getNumSamples());
        require (peakAbs (loadedBuffer) > 0.0001f, "loaded multi pack produced silence");
        loadedEngine.allNotesOff();
        loadedEngine.reset();

        auto transportContext = makeContext (0);
        transportContext.isPlaying = true;
        loadedEngine.setRenderContext (transportContext);
        loadedBuffer.clear();
        loadedEngine.process (loadedBuffer, 0, loadedBuffer.getNumSamples());
        require (peakAbs (loadedBuffer) > 0.0001f,
                 "transport auto-play layer did not produce sound without a MIDI note");
        transportContext.isPlaying = false;
        loadedEngine.setRenderContext (transportContext);
        loadedEngine.process (loadedBuffer, 0, loadedBuffer.getNumSamples());

        auto writerRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftMultiInstrumentWriterSmoke");
        writerRoot.deleteRecursively();
        auto projectDirectory = writerRoot.getChildFile ("Project");
        auto writerPackDirectory = writerRoot.getChildFile ("WriterExport.patchcraft");
        require (projectDirectory.createDirectory(), "failed to create multi writer project directory");
        auto layerSourceDirectory = projectDirectory.getChildFile ("layers");
        require (layerSourceDirectory.createDirectory(), "failed to create multi writer layer directory");
        require (sourceSample.copyFileTo (layerSourceDirectory.getChildFile ("a.wav")),
                 "failed to create multi writer layer A sample");
        require (sourceSample.copyFileTo (layerSourceDirectory.getChildFile ("b.wav")),
                 "failed to create multi writer layer B sample");

        auto writeLayerMap = [&] (const juce::String& id, const juce::String& sampleName)
        {
            patchcraft::SampleMap layerMap;
            patchcraft::SampleZoneDef zone;
            zone.samplePath = sampleName;
            zone.rootNote = 60;
            zone.lowNote = 0;
            zone.highNote = 127;
            zone.lowVelocity = 1;
            zone.highVelocity = 127;
            layerMap.add (zone);
            return layerSourceDirectory.getChildFile (id + ".json")
                .replaceWithText (juce::JSON::toString (layerMap.toVar(), true));
        };
        require (writeLayerMap ("a", "a.wav"), "failed to write multi writer layer A map");
        require (writeLayerMap ("b", "b.wav"), "failed to write multi writer layer B map");

        patchcraft::PatchCraftProject writerProject;
        writerProject.setProjectFolder (projectDirectory);
        writerProject.setEngineType ("multi");
        writerProject.getLayout().clear();
        writerProject.backgroundImageRelative.clear();
        writerProject.getManifest().backgroundImage.clear();
        writerProject.getManifest().libraryThumbnail.clear();
        writerProject.getManifest().playerLogoImage.clear();
        writerProject.getManifest().instrumentName = "Layered Writer Smoke";
        writerProject.getManifest().creator = "PatchCraft QA";
        writerProject.getManifest().multiInstrumentMode = true;
        writerProject.getManifest().instrumentIds.add ("a");
        writerProject.getManifest().instrumentIds.add ("b");
        writerProject.getManifest().instrumentNames.add ("Writer Layer A");
        writerProject.getManifest().instrumentNames.add ("Writer Layer B");
        writerProject.getManifest().instrumentFiles.add ("layers/a.json");
        writerProject.getManifest().instrumentFiles.add ("layers/b.json");
        writerProject.getPatches().clear();
        writerProject.getPresets().clear();
        writerProject.getExpansions().clear();

        patchcraft::PatchCraftPackWriter writer;
        juce::String writerError;
        require (writer.write (writerProject, writerPackDirectory, writerError),
                 "multi writer export did not copy layer mappings and samples");

        patchcraft::PatchCraftPack writerPack;
        patchcraft::PatchCraftPackReader writerReader;
        require (writerReader.read (writerPackDirectory, writerPack, writerError),
                 "multi writer exported pack did not read back");
        require (writerPack.manifest.multiInstrumentMode
                 && writerPack.manifest.instrumentFiles.size() == 2
                 && writerPack.manifest.instrumentFiles[0] == "instruments/a.json",
                 "multi writer export did not normalize layer file references");
        require (writerPackDirectory.getChildFile ("instruments").getChildFile ("a.json").existsAsFile(),
                 "multi writer export is missing layer A mapping");
        require (writerPackDirectory.getChildFile ("instruments").getChildFile ("b.json").existsAsFile(),
                 "multi writer export is missing layer B mapping");

        patchcraft::MultiInstrumentEngine writerLoadedEngine;
        writerLoadedEngine.prepare (kSampleRate, kBlockSize, kChannels);
        writerLoadedEngine.loadFromPack (writerPackDirectory, {});
        require (writerLoadedEngine.getLayerCount() == 2,
                 "multi writer exported pack did not load two layers");
        require (writerLoadedEngine.getLoadedSampleCount() == 2,
                 "multi writer exported pack did not load copied layer samples");
        loadedBuffer.clear();
        writerLoadedEngine.noteOn (60, 0.8f);
        writerLoadedEngine.process (loadedBuffer, 0, loadedBuffer.getNumSamples());
        require (peakAbs (loadedBuffer) > 0.0001f,
                 "multi writer exported pack produced silence");
        writerLoadedEngine.allNotesOff();

        patchcraft::MultiInstrumentEngine engine;
        engine.prepare (kSampleRate, kBlockSize, kChannels);
        engine.addInstrumentLayer ("a", "Layer A");
        engine.addInstrumentLayer ("b", "Layer B");
        require (engine.getLayerCount() == 2, "multi engine did not add layers");

        juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
        buffer.clear();
        engine.noteOn (60, 0.8f);
        require (engine.getActiveVoiceCount() == 2, "multi engine did not trigger both layers");
        engine.process (buffer, 0, buffer.getNumSamples());
        require (peakAbs (buffer) > 0.0001f, "multi engine layer render produced silence");
        engine.reset();

        engine.setLayerSolo ("a", true);
        engine.noteOn (64, 0.8f);
        require (engine.getActiveVoiceCount() == 1, "multi engine solo did not isolate one layer");
        buffer.clear();
        engine.process (buffer, 32, 96);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int sample = 0; sample < 32; ++sample)
                require (std::abs (buffer.getSample (channel, sample)) < 0.000001f,
                         "multi engine wrote before requested start sample");
        require (peakAbs (buffer) > 0.0001f, "multi engine offset render produced silence");
        engine.allNotesOff();

        pass ("multi instrument factory and routing");
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
        engine.setParameter ("tapeMix", 0.25f);
        engine.setParameter ("tapeDrive", 0.35f);
        engine.setParameter ("vinylMix", 0.18f);
        engine.setParameter ("vinylDust", 0.12f);
        engine.setParameter ("lofiMix", 0.12f);
        engine.setParameter ("lofiBits", 10.0f);
        engine.setParameter ("vocalMix", 0.16f);
        engine.setParameter ("multiTapMix", 0.24f);
        engine.setParameter ("multiTapFeedback", 0.42f);

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

    void smokePlayerLibraryScannerFindsFactoryDemos()
    {
        auto demoRoot = juce::File::getCurrentWorkingDirectory().getChildFile ("FactoryDemos");
        require (demoRoot.isDirectory(), "FactoryDemos folder is missing");

        patchcraft::LibraryScanner scanner;
        for (const auto& path : scanner.getSearchPaths())
            scanner.removeSearchPath (path);
        scanner.addSearchPath (demoRoot);
        scanner.scanLibrary();

        const auto entries = scanner.getEntries();
        require (entries.size() >= 6, "Player library scanner did not find the approved factory demo packs");
        require (scanner.search ("EchoCraft").size() > 0, "Player library search cannot find EchoCraft demo");
        require (scanner.search ("CircleSEQ").size() > 0, "Player library search cannot find CircleSEQ demo");
        require (scanner.search ("Analog House").size() > 0, "Player library search cannot find Analog House Drums demo");
        require (scanner.getEntriesByCategory ("synth").size() >= 1, "Player library scanner is missing the ship synth demo");
        require (scanner.getEntriesByCategory ("sample").size() >= 1, "Player library scanner is missing the ship sample demo");
        require (scanner.getEntriesByCategory ("fx").size() >= 1, "Player library scanner is missing the ship FX demo");

        bool hasThumbnail = false;
        for (const auto& entry : entries)
        {
            require (entry.folder.isDirectory(), "Player library entry points at a missing folder");
            require (entry.instrumentName.isNotEmpty(), "Player library entry is missing an instrument name");
            require (entry.engineId.isNotEmpty(), "Player library entry is missing an engine id");
            hasThumbnail = hasThumbnail || entry.thumbnailImage.isValid();
        }
        require (hasThumbnail, "Player library entries did not load or generate thumbnails");
        pass ("Player library scanner finds factory demos");
    }

    void smokeFactoryDemoPacks()
    {
        auto demoRoot = juce::File::getCurrentWorkingDirectory().getChildFile ("FactoryDemos");
        require (demoRoot.isDirectory(), "FactoryDemos folder is missing");

        auto demoFolders = demoRoot.findChildFiles (juce::File::findDirectories, false, "*.patchcraft");
        require (demoFolders.size() >= 6, "factory demo library should ship the approved six-demo RC set");

        int audibleInstrumentCount = 0;
        juce::StringArray defaultPresetSignatures;
        for (const auto& folder : demoFolders)
        {
            patchcraft::PatchCraftPack pack;
            patchcraft::PatchCraftPackReader reader;
            juce::String error;
            require (reader.read (folder, pack, error),
                     ("factory demo failed to read: " + folder.getFileName()).toRawUTF8());
            require (folder.getChildFile (pack.manifest.backgroundImage).existsAsFile(),
                      "factory demo background image is missing");
            require (folder.getChildFile (pack.manifest.libraryThumbnail).existsAsFile(),
                      "factory demo thumbnail image is missing");
            require (pack.manifest.playerTitleBannerImage.isNotEmpty(),
                     "factory demo is missing a Player title banner image reference");
            require (folder.getChildFile (pack.manifest.playerTitleBannerImage).existsAsFile(),
                     "factory demo Player title banner image is missing");
            require (folder.getChildFile ("assets/library-artwork.png").existsAsFile(),
                     "factory demo library artwork image is missing");
            require (folder.getChildFile ("assets/player-library-modal.png").existsAsFile(),
                     "factory demo Player library modal artwork is missing");
            require (pack.layout.getAll().size() >= 12,
                      "factory demo does not contain a real player layout");
            require (pack.presets.size() >= 5,
                     "factory demo does not contain enough curated presets");
            require (pack.manifest.playerDisplayName.isNotEmpty(),
                     "factory demo is missing Player display branding");
            require (pack.manifest.playerTagline.isNotEmpty(),
                     "factory demo is missing Player tagline branding");

            const auto referenceIssues = pack.parameters.validateReferences (
                pack.layout.getAll(), pack.dspGraph, pack.presets);
            for (const auto& issue : referenceIssues)
            {
                const auto issueText = issue.toString();
                require (! issueText.containsIgnoreCase ("missing parameter"),
                         ("factory demo contains a missing parameter reference: " + issueText).toRawUTF8());
            }

            int visibleRuntimeElementCount = 0;
            for (const auto& element : pack.layout.getAll())
            {
                require (patchcraft::isPlayerRuntimeElementSupported (element.type),
                         ("factory demo uses unsupported Player runtime element: "
                          + element.id + " / " + patchcraft::elementTypeDisplayName (element.type)).toRawUTF8());

                if (isVisibleFactoryDemoRuntimeElement (element))
                    ++visibleRuntimeElementCount;

                if (element.type == patchcraft::ElementType::Knob)
                    require (element.width <= 180 && element.height <= 180,
                             "factory demo contains an oversized knob that will not match Studio/Player scale");

                if (element.filmstripAsset.isNotEmpty())
                {
                    const auto filmstrip = juce::File::isAbsolutePath (element.filmstripAsset)
                        ? juce::File (element.filmstripAsset)
                        : folder.getChildFile (element.filmstripAsset);
                    require (filmstrip.existsAsFile(),
                             ("factory demo references a missing filmstrip asset: "
                              + element.filmstripAsset).toRawUTF8());
                }

                if (patchcraft::isRuntimeControlElement (element.type))
                {
                    if (element.type == patchcraft::ElementType::Dropdown && element.id == "presets")
                        continue;
                    if (element.action.isNotEmpty())
                        continue;

                    require (element.parameterId.isNotEmpty(),
                             ("factory demo runtime control is not mapped to a parameter: "
                              + element.id).toRawUTF8());
                    require (pack.parameters.find (element.parameterId) != nullptr,
                             ("factory demo runtime control maps to a missing parameter: "
                              + element.id + " -> " + element.parameterId).toRawUTF8());
                    require (element.label.isNotEmpty() || element.parameterId.isNotEmpty(),
                             "factory demo runtime control has no visible label or parameter fallback");
                }
            }
            require (visibleRuntimeElementCount >= 24,
                     "factory demo does not expose enough visible runtime controls in the active surface");

            const patchcraft::LayoutElement* tabPanel = nullptr;
            for (const auto& element : pack.layout.getAll())
                if (element.type == patchcraft::ElementType::TabPanel && element.id == "tabs")
                {
                    tabPanel = &element;
                    break;
                }

            {
                int runtimeControlCount = 0;
                for (const auto& element : pack.layout.getAll())
                    if (patchcraft::isRuntimeControlElement (element.type))
                        ++runtimeControlCount;
                require (tabPanel == nullptr,
                         "factory demo still uses the shared tab strip instead of a custom surface");
                require (runtimeControlCount >= 24,
                         "custom-surface factory demo does not expose enough runtime controls");
                for (const auto& element : pack.layout.getAll())
                {
                    require (element.groupId.isEmpty() || element.groupId == "main",
                             "custom-surface factory demo stores a non-main group id that can hide controls without tabs");
                }
            }

            const auto* defaultPreset = pack.findDefaultPreset();
            require (defaultPreset != nullptr, "factory demo is missing a default preset");
            for (const auto& preset : pack.presets)
            {
                if (auto it = preset.values.find ("oscType"); it != preset.values.end())
                    require (it->second <= 3.0f, "factory demo preset still resolves oscType to unsupported noise waveform");
                if (auto it = preset.values.find ("osc2Type"); it != preset.values.end())
                    require (it->second <= 3.0f, "factory demo preset still resolves osc2Type to unsupported noise waveform");
                if (auto it = preset.values.find ("noiseBlend"); it != preset.values.end())
                    require (it->second <= 0.001f, "factory demo preset still enables broadband noise by default");
            }

            for (const auto& block : pack.dspGraph.blocks)
            {
                if (auto it = block.values.find ("oscType"); it != block.values.end())
                    require (it->second <= 1.0f, "factory demo graph stores an out-of-range oscType source value");
                if (auto it = block.values.find ("osc2Type"); it != block.values.end())
                    require (it->second <= 1.0f, "factory demo graph stores an out-of-range osc2Type source value");
                if (auto it = block.values.find ("noiseBlend"); it != block.values.end())
                    require (it->second <= 0.001f, "factory demo graph enables broadband noise in the source stack by default");
            }

            juce::String signature = folder.getFileNameWithoutExtension() + ":";
            for (const auto& id : { juce::String ("oscType"), juce::String ("wtEnabled"),
                                    juce::String ("sampleLength"), juce::String ("sampleSliceCount"),
                                    juce::String ("drive"), juce::String ("eqEnabled"),
                                    juce::String ("attack"), juce::String ("release"),
                                    juce::String ("filterCutoff"), juce::String ("delayMix"),
                                    juce::String ("reverbMix"), juce::String ("volume") })
            {
                if (auto it = defaultPreset->values.find (id); it != defaultPreset->values.end())
                    signature << id << "=" << juce::String (it->second, 4) << ";";
            }
            defaultPresetSignatures.add (signature);

            patchcraft::PatchCraftProject project;
            require (project.loadRuntimePackAsProject (folder, error),
                     "factory demo cannot be loaded into Studio as a project");
            require (project.getManifest().instrumentName == pack.manifest.instrumentName,
                     "factory demo project load changed the instrument identity");

            auto engine = patchcraft::createEngineFromManifest (pack.manifest.engine);
            require (engine != nullptr, "factory demo could not create runtime engine");
            engine->prepare (kSampleRate, kBlockSize, kChannels);
            engine->loadFromPack (folder, pack.sampleMap);
            auto context = makeContext (engine->needsAudioInput() ? kChannels : 0);
            engine->setRenderContext (context);

            const auto* preset = pack.findDefaultPreset();
            if (preset != nullptr)
                for (const auto& value : preset->values)
                    engine->setParameter (value.first, value.second);

            juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
            float peak = 0.0f;
            if (engine->needsAudioInput())
            {
                for (int block = 0; block < 8; ++block)
                {
                    fillSineInput (buffer, 220.0, block);
                    engine->setRenderContext (context);
                    engine->process (buffer, 0, buffer.getNumSamples());
                    peak = juce::jmax (peak, peakAbs (buffer));
                    advanceContext (context);
                }
                require (peak > 0.0001f, "factory FX demo produced silence with input");
            }
            else
            {
                const bool drumDemo = pack.manifest.category.containsIgnoreCase ("drum");
                engine->noteOn (drumDemo ? 36 : 60, 0.9f);
                for (int block = 0; block < 12; ++block)
                {
                    buffer.clear();
                    engine->setRenderContext (context);
                    engine->process (buffer, 0, buffer.getNumSamples());
                    peak = juce::jmax (peak, peakAbs (buffer));
                    advanceContext (context);
                }
                engine->noteOff (drumDemo ? 36 : 60);
                require (peak > 0.0001f, "factory instrument demo produced silence");
                ++audibleInstrumentCount;
            }
        }

        require (audibleInstrumentCount >= 4, "factory demos need the approved playable instrument set");
        defaultPresetSignatures.removeDuplicates (false);
        require (defaultPresetSignatures.size() == demoFolders.size(),
                 "factory demo default presets must be unique per shipped demo");
        pass ("factory demo packs load and render");
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
        const std::array<patchcraft::ElementType, 23> elementTypes {
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
            patchcraft::ElementType::GranularField,
            patchcraft::ElementType::TabPanel,
            patchcraft::ElementType::ScrollPanel,
            patchcraft::ElementType::Group,
            patchcraft::ElementType::Separator,
            patchcraft::ElementType::DrumPad,
            patchcraft::ElementType::PadGrid,
            patchcraft::ElementType::DrumGrid,
            patchcraft::ElementType::Mixer
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

    void smokeAiCloudLicensingAndPublishScaffolds()
    {
        patchcraft::AiAssistService::CloudIntegrationConfig cloud;
        cloud.imageProvider = patchcraft::AiAssistService::ImageProviderMode::OpenAIImages;
        cloud.imageApiKey = "test-key";
        cloud.imageModel = "gpt-image-1";
        cloud.murekaApiKey = "mureka-test";
        cloud.pluginClubEndpoint = "";
        cloud.licenseEndpoint = "https://license.test/activate";
        const auto restoredCloud = patchcraft::AiAssistService::CloudIntegrationConfig::fromVar (cloud.toVar());
        require (restoredCloud.imageProvider == patchcraft::AiAssistService::ImageProviderMode::OpenAIImages,
                 "cloud image provider did not round-trip");
        require (restoredCloud.murekaApiKey == "mureka-test", "Mureka API setting did not round-trip");
        require (restoredCloud.licenseEndpoint.contains ("license.test"), "license endpoint did not round-trip");

        patchcraft::Manifest manifest;
        manifest.instrumentName = "Protected Smoke";
        manifest.creator = "PatchCraft";
        manifest.licenseRequired = true;
        manifest.licenseProductId = "PROTECTED-SMOKE";
        manifest.licenseServerUrl = cloud.licenseEndpoint;
        manifest.licensePublicKey = "public-key";
        manifest.licensePolicy = "online-or-offline-grace";
        manifest.licenseOfflineGraceDays = 21;
        const auto restoredManifest = patchcraft::Manifest::fromVar (manifest.toVar());
        require (restoredManifest.licenseProductId == manifest.licenseProductId,
                 "manifest license product id did not round-trip");
        require (restoredManifest.licenseOfflineGraceDays == 21,
                 "manifest offline license grace did not round-trip");

        patchcraft::LicenseValidator::LicenseInfo info;
        info.instrumentName = manifest.instrumentName;
        info.creator = manifest.creator;
        info.productId = manifest.licenseProductId;
        info.licenseServerUrl = manifest.licenseServerUrl;
        info.policy = manifest.licensePolicy;
        info.offlineGraceDays = manifest.licenseOfflineGraceDays;
        const auto activation = patchcraft::LicenseValidator::buildActivationRequest (info, "TEST-MACHINE");
        require (activation.getDynamicObject() != nullptr, "activation request was not an object");
        require (activation.getDynamicObject()->getProperty ("machineId").toString() == "TEST-MACHINE",
                 "activation request did not include explicit machine id");

        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftAiCloudSmoke");
        root.deleteRecursively();
        require (root.createDirectory(), "failed to create AI cloud smoke folder");

        patchcraft::PatchCraftProject project;
        project.setEngineType ("synth");
        project.setProjectFolder (root.getChildFile ("Project"));
        require (project.getProjectFolder().createDirectory(), "failed to create AI cloud smoke project folder");
        project.getManifest().instrumentName = "Protected Smoke";
        project.getManifest().creator = "PatchCraft";
        project.getManifest().licenseRequired = true;
        project.getManifest().licenseServerUrl = cloud.licenseEndpoint;
        project.getManifest().licenseProductId = "PROTECTED-SMOKE";
        project.getManifest().libraryThumbnail.clear();
        project.getManifest().playerLogoImage.clear();
        project.backgroundImageRelative.clear();
        project.getManifest().backgroundImage.clear();
        project.getPatches().clear();
        project.getPresets().clear();
        project.getExpansions().clear();
        auto patch = project.captureCurrentPatch ("Protected Smoke Patch");
        patch.includedAssets.clear();
        patch.isDefault = true;
        auto preset = patch.toPreset();
        preset.isDefault = true;
        preset.libraryReferences.clear();
        project.getPatches().push_back (patch);
        project.getPresets().push_back (preset);

        patchcraft::AiImageService::Request request;
        request.kind = patchcraft::AiImageService::ImageKind::Asset;
        request.width = 128;
        request.height = 128;
        request.transparent = true;
        request.outputFile = root.getChildFile ("fallback-asset.png");
        request.prompt = patchcraft::AiImageService::buildPrompt (request.kind, project, "gold geometry");
        patchcraft::AiAssistService::CloudIntegrationConfig fallbackCloud;
        const auto imageResult = patchcraft::AiImageService::generate (request, fallbackCloud);
        require (imageResult.success, "fallback AI image generation failed");
        require (imageResult.outputFile.existsAsFile(), "fallback AI image was not written");

        auto options = patchcraft::PluginClubPublisher::optionsFromCloudConfig (fallbackCloud);
        require (patchcraft::PluginClubPublisher::normaliseSellerImportEndpoint ("https://plugin.club")
                    == "https://plugin.club/functions/sellerImport",
                 "Plugin.club root endpoint did not normalize to /functions/sellerImport");
        require (patchcraft::PluginClubPublisher::normaliseSellerImportEndpoint ("https://plugin.club/sellerImport")
                    == "https://plugin.club/functions/sellerImport",
                 "Plugin.club endpoint missing /functions was not repaired");
        require (patchcraft::PluginClubPublisher::normaliseSellerImportEndpoint ("https://plugin.club/functions/romplurSellerImport")
                    == "https://plugin.club/functions/sellerImport",
                 "legacy Plugin.club Romplur seller endpoint was not migrated");
        options.endpoint.clear();
        options.stagingRoot = root.getChildFile ("Publish");
        const auto publish = patchcraft::PluginClubPublisher::publishDraft (project, options);
        if (! publish.success)
            std::cerr << publish.message << std::endl;
        require (publish.success, "Plugin.club draft staging failed");
        require (! publish.uploaded, "Plugin.club draft unexpectedly uploaded with no endpoint");
        require (publish.packFolder.getChildFile ("manifest.json").existsAsFile(),
                 "Plugin.club staged pack is missing manifest");
        require (publish.packFolder.getChildFile ("license.json").existsAsFile(),
                 "Plugin.club staged protected pack is missing license.json");
        require (publish.payloadFile.existsAsFile(), "Plugin.club publish payload was not written");
        require (publish.metadataFile.existsAsFile(), "Plugin.club publish metadata was not written");
        require (publish.archiveFile.existsAsFile() && publish.archiveFile.getSize() > 0,
                 "Plugin.club publish archive was not written");
        auto parsedPublishMetadata = juce::JSON::parse (publish.metadataFile);
        if (auto* metadata = parsedPublishMetadata.getDynamicObject())
        {
            require (metadata->getProperty ("artifact_kind").toString() == "patchcraft_instrument_pack",
                     "Plugin.club instrument metadata has wrong artifact_kind");
            require (metadata->getProperty ("product_type").toString() == "instrument",
                     "Plugin.club instrument metadata has wrong product_type");
            require (juce::JSON::toString (metadata->getProperty ("plugin_format"), false).contains ("PatchCraft"),
                     "Plugin.club instrument metadata does not advertise PatchCraft format");
            require (metadata->hasProperty ("license_config"),
                     "Plugin.club protected instrument metadata is missing license_config");
        }
        else
        {
            require (false, "Plugin.club metadata file is not valid JSON");
        }

        const auto oneShotFolder = root.getChildFile ("OneShotPublishSource");
        require (oneShotFolder.createDirectory(), "could not create one-shot publish source");
        require (oneShotFolder.getChildFile ("Kick_C1.wav").replaceWithText ("fake wav bytes"),
                 "could not write one-shot publish source file");
        require (oneShotFolder.getChildFile ("oneshot_pack.json").replaceWithText ("{\"format\":\"PatchCraftOneShotPack\"}"),
                 "could not write one-shot publish metadata");

        patchcraft::PluginClubPublisher::PublishArtifact oneShotArtifact;
        oneShotArtifact.kind = patchcraft::PluginClubPublisher::ArtifactKind::OneShotPack;
        oneShotArtifact.title = "Smoke One Shots";
        oneShotArtifact.creator = "PatchCraft";
        oneShotArtifact.category = "Drums";
        oneShotArtifact.formats.add ("WAV");
        oneShotArtifact.sourcePath = oneShotFolder;

        const auto oneShotPublish = patchcraft::PluginClubPublisher::publishArtifact (oneShotArtifact, options);
        if (! oneShotPublish.success)
            std::cerr << oneShotPublish.message << std::endl;
        require (oneShotPublish.success, "Plugin.club one-shot artifact staging failed");
        require (! oneShotPublish.uploaded, "Plugin.club one-shot artifact unexpectedly uploaded with no endpoint");
        require (oneShotPublish.metadataFile.existsAsFile(), "Plugin.club one-shot metadata was not written");
        require (oneShotPublish.payloadFile.existsAsFile(), "Plugin.club one-shot payload was not written");
        require (oneShotPublish.archiveFile.existsAsFile() && oneShotPublish.archiveFile.getSize() > 0,
                 "Plugin.club one-shot archive was not written");

        pass ("AI cloud, licensing, and Plugin.club scaffolds");
    }

    void smokePcexpExpansionSystem()
    {
        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraftPcexpSmoke");
        root.deleteRecursively();
        require (root.createDirectory(), "failed to create pcexp smoke root");

        patchcraft::PcexpManager manager (root.getChildFile ("UserExpansions"),
                                          root.getChildFile ("BundledExpansions"));
        require (manager.hasCapability ("script.pscript"), "built-in pScript capability missing");
        require (manager.findInstalled ("com.patchcraft.pscript.core") != nullptr,
                 "built-in pScript manifest missing");

        const auto package = root.getChildFile ("JavaScriptRuntime.pcexp");
        require (package.createDirectory(), "failed to create pcexp package folder");
        const char* manifestJson = R"json({
  "format": "PatchCraft Extension",
  "formatVersion": 1,
  "id": "com.patchcraft.javascript-runtime",
  "name": "JavaScript Runtime",
  "version": "1.0.0",
  "kind": "script-runtime",
  "author": "PatchCraft",
  "description": "Optional JavaScript bridge for Studio-side scripts.",
  "minPatchCraftVersion": "0.1.0",
  "capabilities": ["script.runtime.javascript"],
  "dependencies": ["com.patchcraft.pscript.core>=1.0.0"],
  "license": { "mode": "none" },
  "tags": ["script", "javascript"]
})json";
        require (package.getChildFile ("manifest.json").replaceWithText (manifestJson),
                 "failed to write pcexp manifest");

        const auto validation = manager.validatePackage (package);
        require (validation.valid, "pcexp package validation failed");
        require (validation.manifest.id == "com.patchcraft.javascript-runtime",
                 "pcexp manifest id did not parse");

        const auto install = manager.installPackage (package, true);
        if (install.failed())
            std::cerr << install.getErrorMessage() << std::endl;
        require (install.wasOk(), "pcexp package install failed");
        require (manager.hasCapability ("script.runtime.javascript"),
                 "installed pcexp capability missing");

        const auto disable = manager.setEnabled ("com.patchcraft.javascript-runtime", false);
        require (disable.wasOk(), "failed to disable pcexp");
        require (! manager.hasCapability ("script.runtime.javascript"),
                 "disabled pcexp capability still active");

        const auto enable = manager.setEnabled ("com.patchcraft.javascript-runtime", true);
        require (enable.wasOk(), "failed to re-enable pcexp");
        require (manager.hasCapability ("script.runtime.javascript"),
                 "re-enabled pcexp capability missing");

        pass ("PatchCraft .pcexp extension system");
    }

    void smokePScriptInterpreterAndEvents()
    {
        patchcraft::LiveValueStore store;
        patchcraft::PScriptEngine engine;
        engine.bindStore (&store);

        juce::String script = 
            "when note starts:\n"
            "    set volume to velocity mapped 0..127 -> 0.0..1.0\n"
            "    if velocity > 100: set filterCutoff to 12000\n"
            "    else: set filterCutoff to 800\n"
            "when note ends:\n"
            "    set volume to 0.0\n"
            "when modwheel moves:\n"
            "    set pan to modwheel mapped 0..127 -> -1.0..1.0\n"
            "when knob \"Cutoff\" moves:\n"
            "    set delayMix to value mapped 20 Hz..20000 Hz -> 0.1..0.9\n";

        juce::String err = engine.compile (script);
        require (err.isEmpty(), ("pScript compilation failed: " + err).toRawUTF8());
        require (engine.isCompiled(), "pScript isCompiled should be true");

        // Trigger note starts with high velocity
        engine.triggerEvent ("note starts", {{"velocity", 120.0f}});
        require (std::abs (store.getValue ("volume") - (120.0f / 127.0f)) < 0.0001f, "volume was not mapped correctly");
        require (std::abs (store.getValue ("filterCutoff") - 12000.0f) < 0.0001f, "filterCutoff was not set under velocity condition");

        // Trigger note starts with low velocity
        engine.triggerEvent ("note starts", {{"velocity", 50.0f}});
        require (std::abs (store.getValue ("volume") - (50.0f / 127.0f)) < 0.0001f, "volume mapping mismatch");
        require (std::abs (store.getValue ("filterCutoff") - 800.0f) < 0.0001f, "filterCutoff else branch mismatch");

        // Trigger note ends
        engine.triggerEvent ("note ends", {});
        require (std::abs (store.getValue ("volume") - 0.0f) < 0.0001f, "volume was not set to 0.0 on note ends");

        // Trigger modwheel with a negative mapped destination
        engine.triggerEvent ("modwheel moves", {{"modwheel", 0.0f}});
        require (std::abs (store.getValue ("pan") - -1.0f) < 0.0001f, "pan was not mapped to negative destination range");

        // Trigger knob moves for Cutoff
        engine.triggerEvent ("knob moves", {{"value", 5015.0f}}, "Cutoff");
        require (std::abs (store.getValue ("delayMix") - 0.3f) < 0.0001f, "delayMix was not mapped from knob value on Cutoff move");

        // Trigger knob moves for Resonance (should not trigger Cutoff event)
        store.setValue ("delayMix", 0.2f);
        engine.triggerEvent ("knob moves", {{"value", 0.8f}}, "Resonance");
        require (std::abs (store.getValue ("delayMix") - 0.2f) < 0.0001f, "delayMix was incorrectly modified by other knob");

        pass ("pScript runtime, compilation, events, and interpreter execution");
    }

    void smokeCanvasModuleTemplates()
    {
        patchcraft::PatchCraftProject project;
        auto& graph = project.getDspGraph();
        auto& pm = project.getParameters();
        auto& liveValues = project.getLiveValues();

        juce::StringArray paramIds { "chorusRate", "chorusDepth", "chorusFeedback", "chorusMix" };
        for (const auto& paramId : paramIds)
        {
            if (pm.find (paramId) == nullptr)
            {
                patchcraft::ParameterDef def;
                if (patchcraft::ParameterModel::getRegistryDefinition (paramId, "fx", def))
                {
                    pm.add (def);
                    liveValues.getOrAddRaw (def.id, def.defaultValue);
                }
            }
        }

        patchcraft::DspBlock chorusBlock;
        chorusBlock.id = "chorus_module";
        chorusBlock.section = "fx";
        chorusBlock.type = "chorus";
        chorusBlock.name = "Chorus Block";
        chorusBlock.targetId = "chorusMix";
        chorusBlock.enabled = true;
        chorusBlock.values["chorusRate"] = 0.35f;
        chorusBlock.values["chorusDepth"] = 0.35f;
        chorusBlock.values["chorusFeedback"] = 0.0f;
        chorusBlock.values["chorusMix"] = 0.5f;
        graph.blocks.push_back (chorusBlock);

        require (pm.find ("chorusRate") != nullptr, "chorusRate parameter not registered");
        require (pm.find ("chorusDepth") != nullptr, "chorusDepth parameter not registered");
        
        bool foundChorusBlock = false;
        for (const auto& b : graph.blocks)
        {
            if (b.id == "chorus_module" && b.type == "chorus")
            {
                foundChorusBlock = true;
                break;
            }
        }
        require (foundChorusBlock, "Chorus DSP block was not created or has wrong type");

        pass ("canvas module templates system");
    }

    void smokePScriptExpandedFeatures()
    {
        patchcraft::LiveValueStore store;
        patchcraft::PScriptEngine engine;
        engine.bindStore (&store);

        juce::String script =
            "when note starts:\n"
            "    let x = 10.0\n"
            "    let y = x * 2.0 + (5.0 - 1.0) / 2.0\n" // y = 10 * 2 + 4 / 2 = 22
            "    set volume to y / 22.0\n" // volume = 1.0
            "    let loopCount = 5.0\n"
            "    repeat loopCount:\n"
            "        set filterCutoff to filterCutoff + 100.0\n" // filterCutoff starts at 0, should become 500
            "    print \"Hello pScript\"\n"
            "    print y\n";

        store.setValue ("filterCutoff", 0.0f);

        juce::String err = engine.compile (script);
        require (err.isEmpty(), ("Expanded pScript compilation failed: " + err).toRawUTF8());
        require (engine.isCompiled(), "PScript Engine isCompiled should be true");

        engine.triggerEvent ("note starts", {});

        require (std::abs (store.getValue ("volume") - 1.0f) < 0.0001f, "volume calculation using let and arithmetic failed");
        require (std::abs (store.getValue ("filterCutoff") - 500.0f) < 0.0001f, "repeat loop execution on parameters failed");

        pass ("pScript expanded variables, arithmetic, print, and repeat loops");
    }

    void smokeTimerEvents()
    {
        patchcraft::LiveValueStore store;
        patchcraft::PScriptEngine engine;
        engine.bindStore (&store);

        juce::String script =
            "when timer 50 ms:\n"
            "    set delayMix to delayMix + 0.1\n";

        store.setValue ("delayMix", 0.0f);

        juce::String err = engine.compile (script);
        require (err.isEmpty(), ("Timer pScript compilation failed: " + err).toRawUTF8());

        // Wait a short time to let the timer fire (e.g. 150 ms)
        juce::Time start = juce::Time::getCurrentTime();
        while (juce::Time::getCurrentTime() - start < juce::RelativeTime::milliseconds (180))
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        }

        require (store.getValue ("delayMix") > 0.05f, "timer event did not fire and modify delayMix");

        pass ("pScript timer event compilation and scheduled firing");
    }

    void smokeExpandedCanvasModules()
    {
        patchcraft::PatchCraftProject project;
        auto& graph = project.getDspGraph();
        auto& pm = project.getParameters();
        auto& liveValues = project.getLiveValues();

        // 1. Reverb Module
        juce::StringArray reverbParams { "reverbMix" };
        for (const auto& paramId : reverbParams)
        {
            if (pm.find (paramId) == nullptr)
            {
                patchcraft::ParameterDef def;
                if (patchcraft::ParameterModel::getRegistryDefinition (paramId, "fx", def))
                {
                    pm.add (def);
                    liveValues.getOrAddRaw (def.id, def.defaultValue);
                }
            }
        }
        patchcraft::DspBlock reverbBlock;
        reverbBlock.id = "reverb_module";
        reverbBlock.section = "fx";
        reverbBlock.type = "reverb";
        reverbBlock.name = "Reverb Block";
        reverbBlock.targetId = "reverbMix";
        reverbBlock.enabled = true;
        reverbBlock.values["reverbMix"] = 0.35f;
        graph.blocks.push_back (reverbBlock);

        // 2. Phaser Module
        juce::StringArray phaserParams { "phaserRate", "phaserDepth", "phaserFeedback", "phaserMix" };
        for (const auto& paramId : phaserParams)
        {
            if (pm.find (paramId) == nullptr)
            {
                patchcraft::ParameterDef def;
                if (patchcraft::ParameterModel::getRegistryDefinition (paramId, "fx", def))
                {
                    pm.add (def);
                    liveValues.getOrAddRaw (def.id, def.defaultValue);
                }
            }
        }
        patchcraft::DspBlock phaserBlock;
        phaserBlock.id = "phaser_module";
        phaserBlock.section = "fx";
        phaserBlock.type = "phaser";
        phaserBlock.name = "Phaser Block";
        phaserBlock.targetId = "phaserMix";
        phaserBlock.enabled = true;
        phaserBlock.values["phaserRate"] = 0.25f;
        phaserBlock.values["phaserDepth"] = 0.45f;
        phaserBlock.values["phaserMix"] = 0.5f;
        graph.blocks.push_back (phaserBlock);

        // 3. Stereo Module
        juce::StringArray stereoParams { "stereoWidth", "monoMaker" };
        for (const auto& paramId : stereoParams)
        {
            if (pm.find (paramId) == nullptr)
            {
                patchcraft::ParameterDef def;
                if (patchcraft::ParameterModel::getRegistryDefinition (paramId, "fx", def))
                {
                    pm.add (def);
                    liveValues.getOrAddRaw (def.id, def.defaultValue);
                }
            }
        }
        patchcraft::DspBlock utilityBlock;
        utilityBlock.id = "stereo_module";
        utilityBlock.section = "out";
        utilityBlock.type = "utility";
        utilityBlock.name = "Stereo Utility Block";
        utilityBlock.targetId = "stereoWidth";
        utilityBlock.enabled = true;
        utilityBlock.values["stereoWidth"] = 1.0f;
        graph.blocks.push_back (utilityBlock);

        require (pm.find ("reverbMix") != nullptr, "reverbMix parameter not registered");
        require (pm.find ("phaserRate") != nullptr, "phaserRate parameter not registered");
        require (pm.find ("stereoWidth") != nullptr, "stereoWidth parameter not registered");

        bool foundReverb = false;
        bool foundPhaser = false;
        bool foundUtility = false;
        for (const auto& b : graph.blocks)
        {
            if (b.type == "reverb") foundReverb = true;
            if (b.type == "phaser") foundPhaser = true;
            if (b.type == "utility") foundUtility = true;
        }

        require (foundReverb, "Reverb DSP block missing");
        require (foundPhaser, "Phaser DSP block missing");
        require (foundUtility, "Utility DSP block missing");

        patchcraft::ParameterDef oscTypeDef;
        require (patchcraft::ParameterModel::getRegistryDefinition ("oscType", "synth", oscTypeDef)
                 && oscTypeDef.max <= 3.0f,
                 "oscType registry still exposes the unsafe noise waveform slot");
        patchcraft::ParameterDef osc2TypeDef;
        require (patchcraft::ParameterModel::getRegistryDefinition ("osc2Type", "synth", osc2TypeDef)
                 && osc2TypeDef.max <= 3.0f,
                 "osc2Type registry still exposes the unsafe noise waveform slot");

        struct ModuleBlockCase
        {
            const char* id;
            const char* section;
            const char* type;
            const char* name;
            const char* target;
            const char* family;
            const char* role;
            const char* ioMode;
        };

        const ModuleBlockCase premiumModules[] = {
            { "osc_stack_module_test",       "source", "oscStack",        "OSC Stack",          "oscBlend",       "synth",   "source",     "stereo" },
            { "serum_table_module_test",     "source", "serumWavetable",  "Serum Table",        "wtPosition",     "synth",   "source",     "stereo" },
            { "sample_player_module_test",   "source", "samplePlayer",    "Sample Player",      "sampleStart",    "sampler", "source",     "stereo" },
            { "slice_chop_module_test",      "source", "sliceChop",       "Slice Chop",         "sampleSlice",    "sampler", "source",     "stereo" },
            { "scratch_deck_module_test",    "source", "scratchDeck",     "Scratch Deck",       "sampleStart",    "sampler", "source",     "stereo" },
            { "granular_sampler_module_test","source", "granularSampler", "Granular Sampler",   "granularDensity","sampler", "source",     "stereo" },
            { "drum_rack_module_test",       "source", "drumRack",        "Drum Rack",          "pad1Volume",     "drums",   "source",     "stereo" },
            { "drum_seq_module_test",        "mod",    "drumSequencer",   "Drum Sequencer",     "arpLaneRate",    "midi",    "sequencer",  "event" },
            { "arp_lane_module_test",        "mod",    "arp",             "Arp Lane",           "arpLaneRate",    "midi",    "arp",        "event" },
            { "step_lfo_module_test",        "mod",    "stepLfo",         "Step LFO",           "filterCutoff",   "midi",    "modulation", "modulation" },
            { "dynamic_eq_module_test",      "filter", "dynamicEq",       "Dynamic EQ",         "eqMix",          "studio",  "tone",       "stereo" },
            { "limiter_module_test",         "out",    "limiter",         "Limiter",            "outputCeilingDb", "studio", "dynamics",   "stereo" },
            { "transient_module_test",       "fx",     "transientShaper", "Transient Shaper",   "dynMix",         "studio",  "dynamics",   "stereo" },
            { "flanger_module_test",         "fx",     "flanger",         "Flanger",            "chorusMix",      "creative","modulation", "stereo" },
            { "multitap_module_test",        "fx",     "multiTapDelay",   "MultiTap Delay",     "multiTapMix",    "creative","space",      "stereo" },
            { "vocal_fx_module_test",        "fx",     "vocalFormant",    "Vocal FX",           "vocalMix",       "creative","tone",       "stereo" },
            { "master_bus_module_test",      "out",    "masterBus",       "Master Bus",         "outputCeilingDb", "studio", "dynamics",   "stereo" }
        };

        for (const auto& module : premiumModules)
        {
            patchcraft::DspGraph moduleGraph;
            if (juce::String (module.section) != "source")
            {
                patchcraft::DspBlock source;
                source.id = "test_source";
                source.section = "source";
                source.type = "samplePlayer";
                source.name = "Test Source";
                source.targetId = "sampleStart";
                source.enabled = true;
                source.values["volume"] = 0.65f;
                moduleGraph.blocks.push_back (source);
            }

            patchcraft::DspBlock block;
            block.id = module.id;
            block.section = module.section;
            block.type = module.type;
            block.name = module.name;
            block.targetId = module.target;
            block.enabled = true;
            block.metadata["family"] = module.family;
            block.metadata["role"] = module.role;
            block.metadata["ioMode"] = module.ioMode;
            block.values[module.target] = juce::String (module.id) == "limiter_module_test" ? -0.8f : 0.5f;
            if (juce::String (module.section) == "out")
                block.values["outputLimiter"] = 1.0f;
            moduleGraph.blocks.push_back (block);

            if (juce::String (module.section) != "out")
            {
                patchcraft::DspBlock output;
                output.id = "test_output";
                output.section = "out";
                output.type = "limiter";
                output.name = "Test Output";
                output.targetId = "outputCeilingDb";
                output.enabled = true;
                output.values["outputLimiter"] = 1.0f;
                output.values["outputCeilingDb"] = -0.8f;
                moduleGraph.blocks.push_back (output);
            }

            for (const auto& issue : moduleGraph.validateTypedGraph ("sample"))
            {
                require (issue.severity != "error", ("premium DSP module graph validation error: " + issue.toString()).toRawUTF8());
                if (issue.ownerId == module.id)
                    require (! issue.message.containsIgnoreCase ("generic Player routing"),
                             ("premium DSP module is not first-class in graph validation: " + issue.toString()).toRawUTF8());
            }
        }

        pass ("expanded canvas module templates");
    }
}

int main()
{
    try
    {
        smokeSynthWavetable();
        smokeSamplerWavLoad();
        smokeSamplerMidiSampleControls();
        smokeSamplerGranularVoiceEngine();
        smokeSamplerBpmSyncPlayback();
        smokeSampleDrumPadsAndPerformanceMetadata();
        smokeSamplerDrumPadRuntime();
        smokeSampleImportNameParsing();
        smokeSampleAutoMapSingleRootDoesNotStretchStack();
        smokeFxSamplePreview();
        smokeFxLiveInput();
        smokeArpeggiatorRuntime();
        smokeMidiPlaygroundRuntime();
        smokeMidiPlaygroundDrumMachineRuntime();
        smokeMidiPlaygroundSampleOverlayRuntime();
        smokeMidiPlaygroundChordPresetRuntime();
        smokeMidiPlaygroundTransformers();
        smokeMidiPlaygroundTimingTransformers();
        smokeMidiPlaygroundPhraseBanksAndExport();
        smokeMidiPlaygroundAdvancedRuntimeAndExport();
        smokeMidiPlaygroundActiveBankIsolation();
        smokeMidiPlaygroundMultiLanePlayback();
        smokeMidiPlaygroundDspModulationRouting();
        smokePlayerFxGraphControlsStayLive();
        smokePlayerInstrumentFactory();
        smokePlayerFxFactory();
        smokeMultiInstrumentFactoryAndRouting();
        smokeTypedGraphEdges();
        smokeAdvancedFxProcessors();
        smokeStudioPreviewProjectRender();
        smokePlayerLibraryScannerFindsFactoryDemos();
        smokeFactoryDemoPacks();
        smokeProjectSaveRestoresCurrentSound();
        smokePresetAppliesLinkedPatchState();
        smokePatchExpansionSerialization();
        smokePatchExpansionPackExport();
        smokeLayoutRuntimeParityAndMalformedPacks();
        smokePcexpExpansionSystem();
        smokeAiCloudLicensingAndPublishScaffolds();
        smokePhysicalMidiAndModWheelCrashRepros();
        smokePScriptInterpreterAndEvents();
        smokeCanvasModuleTemplates();
        smokePScriptExpandedFeatures();
        smokeTimerEvents();
        smokeExpandedCanvasModules();
        std::cout << "PatchCraft audio smoke tests passed." << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] " << e.what() << std::endl;
        return 1;
    }
}
