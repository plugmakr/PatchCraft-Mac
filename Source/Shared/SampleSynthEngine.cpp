#include "SampleSynthEngine.h"
#include "PatchCraftPackFormat.h"
#include "SampleMap.h"
#include "DebugLog.h"

#include <cstring>
#include <cmath>
#include <limits>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static juce::uint16 readU16LE (juce::InputStream& stream)
        {
            unsigned char data[2] {};
            if (stream.read (data, 2) != 2)
                return 0;
            return (juce::uint16) data[0] | ((juce::uint16) data[1] << 8);
        }

        static juce::uint32 readU32LE (juce::InputStream& stream)
        {
            unsigned char data[4] {};
            if (stream.read (data, 4) != 4)
                return 0;
            return (juce::uint32) data[0]
                 | ((juce::uint32) data[1] << 8)
                 | ((juce::uint32) data[2] << 16)
                 | ((juce::uint32) data[3] << 24);
        }

        static juce::String readFourCC (juce::InputStream& stream)
        {
            char data[4] {};
            if (stream.read (data, 4) != 4)
                return {};
            return juce::String::fromUTF8 (data, 4);
        }

        static int parsePadControlIndex (const juce::String& id, const juce::String& suffix)
        {
            if (! id.startsWithIgnoreCase ("pad") || ! id.endsWithIgnoreCase (suffix))
                return -1;

            const auto numberText = id.substring (3, id.length() - suffix.length());
            const int oneBasedIndex = numberText.getIntValue();
            const int zeroBasedIndex = oneBasedIndex - 1;
            return zeroBasedIndex >= 0 && zeroBasedIndex < 16 ? zeroBasedIndex : -1;
        }

        static float clampSampleValue (double value)
        {
            return juce::jlimit (-1.0f, 1.0f, (float) value);
        }

        static float decodeWavFrameSample (const unsigned char* frame,
                                           int availableBytes,
                                           int offset,
                                           int bytesPerSample,
                                           int bitsPerSample,
                                           int audioFormat)
        {
            if (frame == nullptr || offset < 0 || bytesPerSample <= 0 || offset + bytesPerSample > availableBytes)
                return 0.0f;

            const auto* data = frame + offset;
            if (bitsPerSample == 8 && bytesPerSample >= 1)
                return clampSampleValue (((int) data[0] - 128) / 128.0);
            if (bitsPerSample == 16 && bytesPerSample >= 2)
            {
                const auto value = (short) ((juce::uint16) data[0] | ((juce::uint16) data[1] << 8));
                return clampSampleValue ((double) value / 32768.0);
            }
            if (bitsPerSample == 24 && bytesPerSample >= 3)
            {
                int value = (int) data[0] | ((int) data[1] << 8) | ((int) data[2] << 16);
                if ((value & 0x00800000) != 0)
                    value |= (int) 0xff000000;
                return clampSampleValue ((double) value / 8388608.0);
            }
            if (bitsPerSample == 32 && bytesPerSample >= 4)
            {
                if (audioFormat == 3)
                {
                    float value = 0.0f;
                    std::memcpy (&value, data, sizeof (value));
                    return clampSampleValue ((double) value);
                }

                const int value = (int) ((juce::uint32) data[0]
                                      | ((juce::uint32) data[1] << 8)
                                      | ((juce::uint32) data[2] << 16)
                                      | ((juce::uint32) data[3] << 24));
                return clampSampleValue ((double) value / 2147483648.0);
            }
            return 0.0f;
        }

        static bool loadWavSampleSafely (const juce::File& file,
                                         LoadedSample& out,
                                         juce::String& error)
        {
            auto stream = file.createInputStream();
            if (stream == nullptr || ! stream->openedOk())
            {
                error = "could not open stream";
                return false;
            }

            if (readFourCC (*stream) != "RIFF")
            {
                error = "unsupported container";
                return false;
            }

            (void) readU32LE (*stream);
            if (readFourCC (*stream) != "WAVE")
            {
                error = "not a WAVE file";
                return false;
            }

            int audioFormat = 0;
            int numChannels = 0;
            int bitsPerSample = 0;
            int blockAlign = 0;
            double sourceRate = 44100.0;
            juce::int64 dataStart = -1;
            juce::uint32 dataSize = 0;
            bool haveFmt = false;

            while (! stream->isExhausted())
            {
                const auto chunkId = readFourCC (*stream);
                if (chunkId.length() != 4)
                    break;

                const auto chunkSize = readU32LE (*stream);
                const auto chunkDataStart = stream->getPosition();
                if (chunkId == "fmt ")
                {
                    if (chunkSize < 16)
                    {
                        error = "invalid fmt chunk";
                        return false;
                    }

                    audioFormat = (int) readU16LE (*stream);
                    numChannels = (int) readU16LE (*stream);
                    sourceRate = (double) readU32LE (*stream);
                    (void) readU32LE (*stream);
                    blockAlign = (int) readU16LE (*stream);
                    bitsPerSample = (int) readU16LE (*stream);
                    if (audioFormat == 0xfffe && chunkSize >= 40)
                    {
                        (void) readU16LE (*stream);
                        (void) readU16LE (*stream);
                        (void) readU32LE (*stream);
                        audioFormat = (int) readU16LE (*stream);
                    }
                    haveFmt = true;
                }
                else if (chunkId == "data")
                {
                    dataStart = chunkDataStart;
                    dataSize = chunkSize;
                }

                stream->setPosition (chunkDataStart + (juce::int64) chunkSize + (chunkSize & 1u));
                if (haveFmt && dataStart >= 0)
                    break;
            }

            if (! haveFmt || dataStart < 0)
            {
                error = "missing fmt/data chunk";
                return false;
            }

            if ((audioFormat != 1 && audioFormat != 3) || sourceRate <= 0.0 || numChannels <= 0
                || bitsPerSample <= 0 || blockAlign <= 0)
            {
                error = "unsupported WAV format";
                return false;
            }

            const int bytesPerSample = (bitsPerSample + 7) / 8;
            if (bytesPerSample <= 0 || blockAlign < bytesPerSample * numChannels || blockAlign > 4096)
            {
                error = "unsupported frame layout";
                return false;
            }

            const int channelsToRead = juce::jlimit (1, 2, numChannels);
            const auto fileSize = file.getSize();
            const auto availableDataBytes = dataStart >= 0 && fileSize > dataStart
                ? juce::jmin ((juce::int64) dataSize, fileSize - dataStart)
                : (juce::int64) dataSize;
            const auto totalFrames = availableDataBytes / blockAlign;
            const auto maxFramesByDuration = (juce::int64) (juce::jmax (1.0, sourceRate) * 30.0);
            const auto maxFramesByMemory = (96ll * 1024ll * 1024ll)
                                          / (juce::int64) channelsToRead
                                          / (juce::int64) sizeof (float);
            const auto frames64 = juce::jmin (juce::jmin (totalFrames, maxFramesByDuration), maxFramesByMemory);
            if (frames64 <= 0 || frames64 > (juce::int64) std::numeric_limits<int>::max())
            {
                error = "empty or too large";
                return false;
            }

            const auto bytesToRead64 = frames64 * (juce::int64) blockAlign;
            if (bytesToRead64 <= 0 || bytesToRead64 > (juce::int64) std::numeric_limits<int>::max())
            {
                error = "unsupported byte range";
                return false;
            }

            std::vector<unsigned char> sampleData;
            try
            {
                sampleData.resize ((size_t) bytesToRead64, 0);
            }
            catch (...)
            {
                error = "could not allocate read buffer";
                return false;
            }

            stream->setPosition (dataStart);
            const int bytesRead = stream->read (sampleData.data(), (int) bytesToRead64);
            const int actualFrames = bytesRead > 0 ? bytesRead / blockAlign : 0;
            if (actualFrames <= 0)
            {
                error = "unexpected end of data";
                return false;
            }

            out.buffer.setSize (channelsToRead, actualFrames, false, true, true);
            out.sourceSampleRate = sourceRate;

            for (int ch = 0; ch < channelsToRead; ++ch)
            {
                auto* dest = out.buffer.getWritePointer (ch);
                const int sampleOffset = ch * bytesPerSample;
                for (int i = 0; i < actualFrames; ++i)
                {
                    const auto* frame = sampleData.data() + (size_t) i * (size_t) blockAlign;
                    dest[i] = decodeWavFrameSample (frame, blockAlign, sampleOffset,
                                                    bytesPerSample, bitsPerSample, audioFormat);
                }
            }

            return true;
        }
    }

    SampleSynthEngine::SampleSynthEngine()
    {
        // registerBasicFormats registers every format the build enables: WAV/AIFF plus
        // FLAC, Ogg, and (on Windows/Apple) MP3 via WindowsMedia/CoreAudio decoders.
        formatManager.registerBasicFormats();
        PC_DBG("SampleSynthEngine initialized");
    }

    void SampleSynthEngine::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = sr;
        blockSize  = maxBlockSize;
        preparedChannels = juce::jmax (1, numChannels);
        preparedMaxSamples = maxBlockSize;

        for (auto& v : voices)     v.prepare (sr);
        for (auto& v : granularVoices) v.prepare (sr);
        for (auto& sv : sineVoices) sv.env.setSampleRate (sr);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize,
                                      (juce::uint32) preparedChannels };

        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency (atomics.cutoff.load());
        filter.setResonance (atomics.resonance.load());
        eq.prepare (sr, maxBlockSize, numChannels);
        advancedFx.prepare (sr, maxBlockSize, numChannels);

        delayL.prepare (spec);
        delayR.prepare (spec);
        delayL.setMaximumDelayInSamples ((int) (sr * 2.0));
        delayR.setMaximumDelayInSamples ((int) (sr * 2.0));

        reverb.prepare (spec);

        tempBuffer.setSize (juce::jmax (2, numChannels), maxBlockSize, false, false, true);
    }

    void SampleSynthEngine::setRenderContext (const RenderContext& context)
    {
        renderContext = context;
        sampleRate = RenderContext::sanitiseSampleRate (context.sampleRate);
    }

    void SampleSynthEngine::reset()
    {
        for (auto& v : voices) v.kill();
        for (auto& v : granularVoices) v.kill();
        for (auto& sv : sineVoices) { sv.active = false; sv.env.reset(); }
        filter.reset();
        eq.reset();
        advancedFx.reset();
        triggerCounter.store (0, std::memory_order_relaxed);
        delayL.reset();
        delayR.reset();
        reverb.reset();
    }

    SampleSynthEngine::SineVoice* SampleSynthEngine::findFreeSineVoice()
    {
        for (auto& sv : sineVoices)
            if (! sv.active) return &sv;
        return &sineVoices[0];
    }

    juce::ADSR::Parameters SampleSynthEngine::currentAdsr() const
    {
        juce::ADSR::Parameters p;
        p.attack  = juce::jmax (0.001f, atomics.attack.load());
        p.decay   = juce::jmax (0.001f, atomics.decay.load());
        p.sustain = juce::jlimit (0.0f, 1.0f, atomics.sustain.load());
        p.release = juce::jmax (0.001f, atomics.release.load());
        return p;
    }

    static juce::File resolvePackPath (const juce::File& root, const juce::String& rel)
    {
        return SampleMap::resolveSamplePath (root, rel);
    }

    namespace
    {
        static juce::String midiNoteName (int note)
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            note = juce::jlimit (0, 127, note);
            return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
        }

        static juce::uint32 makeTriggerHash (int note, int velocity, juce::uint32 counter)
        {
            auto hash = counter + 0x9e3779b9u;
            hash ^= (juce::uint32) (note * 0x85ebca6bu);
            hash = (hash << 13) | (hash >> 19);
            hash ^= (juce::uint32) (velocity * 0xc2b2ae35u);
            hash ^= hash >> 16;
            hash *= 0x7feb352du;
            hash ^= hash >> 15;
            hash *= 0x846ca68bu;
            hash ^= hash >> 16;
            return hash;
        }

        static double selectedRegionSeconds (const LoadedSample& sample,
                                             float sampleStart01,
                                             float sampleLength01,
                                             int sampleSlice,
                                             int sampleSliceCount)
        {
            const int length = sample.buffer.getNumSamples();
            if (length <= 0 || sample.sourceSampleRate <= 0.0)
                return 0.0;

            const int zoneStart = juce::jlimit (0, length - 1, sample.zone.sampleStart);
            const int zoneEnd = sample.zone.sampleEnd > zoneStart
                ? juce::jlimit (zoneStart + 1, length, sample.zone.sampleEnd)
                : length;
            const int sliceCount = juce::jlimit (1, 128, sampleSliceCount);
            const int sliceIndex = juce::jlimit (0, sliceCount - 1, sampleSlice);
            const int zoneLength = juce::jmax (1, zoneEnd - zoneStart);
            const int sliceStart = zoneStart + (zoneLength * sliceIndex) / sliceCount;
            const int sliceEnd = zoneStart + (zoneLength * (sliceIndex + 1)) / sliceCount;
            const int sliceLength = juce::jmax (1, sliceEnd - sliceStart);
            const int startOffset = juce::roundToInt (juce::jlimit (0.0f, 1.0f, sampleStart01)
                                                    * (float) juce::jmax (0, sliceLength - 1));
            const int playStart = juce::jlimit (sliceStart, sliceEnd - 1, sliceStart + startOffset);
            const int playEnd = juce::jlimit (playStart + 1, sliceEnd,
                                              playStart + juce::roundToInt ((float) sliceLength
                                                                          * juce::jlimit (0.01f, 1.0f, sampleLength01)));
            return (double) juce::jmax (1, playEnd - playStart) / sample.sourceSampleRate;
        }
    }

    void SampleSynthEngine::loadFromPack (const juce::File& packFolder, const SampleMap& map)
    {
        PC_DBG("[loadFromPack] BEGIN zones=%d", (int)map.getZones().size());
        auto next = std::make_shared<SampleList>();
        int missingCount = 0;
        int failedCount = 0;
        juce::String firstMissing;
        juce::String firstFailed;

        for (auto& z : map.getZones())
        {
            auto file = resolvePackPath (packFolder, z.samplePath);
            if (! file.existsAsFile())
            {
                PC_DBG("[loadFromPack] Missing sample: %s", file.getFullPathName().toRawUTF8());
                ++missingCount;
                if (firstMissing.isEmpty())
                    firstMissing = file.getFileName().isNotEmpty() ? file.getFileName() : z.samplePath;
                continue;
            }

            auto ls = std::make_shared<LoadedSample>();
            ls->path = file.getFullPathName();
            ls->zone = z;

            if (file.getFileExtension().equalsIgnoreCase (".wav"))
            {
                juce::String wavError;
                if (! loadWavSampleSafely (file, *ls, wavError))
                {
                    PC_DBG("[loadFromPack] WAV decode failed for %s: %s",
                           file.getFullPathName().toRawUTF8(),
                           wavError.toRawUTF8());
                    ++failedCount;
                    if (firstFailed.isEmpty())
                        firstFailed = file.getFileName() + " (" + wavError + ")";
                    continue;
                }
            }
            else
            {
                std::unique_ptr<juce::AudioFormatReader> reader;
                try
                {
                    reader.reset (formatManager.createReaderFor (file));
                }
                catch (...)
                {
                    PC_DBG("[loadFromPack] Reader creation threw for %s", file.getFullPathName().toRawUTF8());
                    ++failedCount;
                    if (firstFailed.isEmpty())
                        firstFailed = file.getFileName() + " (reader failed)";
                    continue;
                }

                if (reader == nullptr)
                {
                    PC_DBG("[loadFromPack] No reader for %s", file.getFullPathName().toRawUTF8());
                    ++failedCount;
                    if (firstFailed.isEmpty())
                        firstFailed = file.getFileName() + " (unsupported format)";
                    continue;
                }

                ls->sourceSampleRate = reader->sampleRate;
                const auto samplesToRead = (int) juce::jmin<int64_t> (reader->lengthInSamples,
                                                                      (int64_t) sampleRate * 30);
                ls->buffer.setSize ((int) juce::jlimit (1, 2, (int) reader->numChannels), samplesToRead);
                if (! reader->read (&ls->buffer, 0, ls->buffer.getNumSamples(), 0, true, reader->numChannels > 1))
                {
                    PC_DBG("[loadFromPack] Decode failed for %s", file.getFullPathName().toRawUTF8());
                    ++failedCount;
                    if (firstFailed.isEmpty())
                        firstFailed = file.getFileName() + " (decode failed)";
                    continue;
                }
            }

            PC_DBG("[loadFromPack] Loaded: %d samples, %d chans",
                   ls->buffer.getNumSamples(), ls->buffer.getNumChannels());
            next->push_back (std::move (ls));
        }

        PC_DBG("[loadFromPack] Swapping in %d samples (shared lifetime)", (int)next->size());
        std::shared_ptr<const SampleList> immutableNext = std::move (next);
        std::atomic_store_explicit (&currentSamples, immutableNext, std::memory_order_release);
        requestedZoneCount.store ((int) map.getZones().size(), std::memory_order_release);
        loadedSampleCount.store ((int) immutableNext->size(), std::memory_order_release);
        missingSampleCount.store (missingCount, std::memory_order_release);
        failedSampleCount.store (failedCount, std::memory_order_release);
        lastMissedNote.store (-1, std::memory_order_release);
        lastMissedVelocity.store (0, std::memory_order_release);

        {
            const juce::ScopedLock lock (diagnosticsLock);
            firstMissingSample = firstMissing;
            firstFailedSample = firstFailed;
        }
        PC_DBG("[loadFromPack] DONE");
    }

    void SampleSynthEngine::loadFromMap (const juce::File& projectFolder, const SampleMap& map)
    {
        loadFromPack (projectFolder, map);
    }

    SampleVoice* SampleSynthEngine::findFreeVoice()
    {
        for (auto& v : voices)
            if (! v.isActive()) return &v;

        // No free voice - steal intelligently instead of blind round-robin.
        // Preference order:
        //   1. Lowest priority zone (priority field; higher = protect).
        //   2. Voices already in their release tail (quietest first).
        //   3. Oldest voice (smallest start order).
        // This avoids cutting off a freshly played, high-priority note.
        SampleVoice* best = nullptr;
        float bestScore = std::numeric_limits<float>::max();
        for (auto& v : voices)
        {
            // Lower score == better steal candidate.
            float score = (float) v.getPriority() * 1000.0f;
            if (v.isReleasing())
                score -= 500.0f;
            score += v.getEnvelopeLevel() * 250.0f;
            // Tie-break toward the oldest voice.
            score += (float) (v.getStartOrder() % 100000u) * 0.001f;

            if (score < bestScore)
            {
                bestScore = score;
                best = &v;
            }
        }

        if (best == nullptr)
        {
            nextVoiceIndex = (nextVoiceIndex + 1) % kMaxVoices;
            best = &voices[(size_t) nextVoiceIndex];
        }
        best->kill();
        return best;
    }

    GranularVoice* SampleSynthEngine::findFreeGranularVoice()
    {
        for (auto& v : granularVoices)
            if (! v.isActive()) return &v;
        nextGranularVoiceIndex = (nextGranularVoiceIndex + 1) % kMaxVoices;
        granularVoices[(size_t) nextGranularVoiceIndex].kill();
        return &granularVoices[(size_t) nextGranularVoiceIndex];
    }

    GranularVoice::Params SampleSynthEngine::currentGranularParams (float tempoRatio) const
    {
        GranularVoice::Params params;
        params.sampleStart = juce::jlimit (0.0f, 1.0f, atomics.sampleStart.load());
        params.sampleLength = juce::jlimit (0.01f, 1.0f, atomics.sampleLength.load());
        params.sampleSlice = juce::roundToInt (atomics.sampleSlice.load());
        params.sampleSliceCount = juce::jlimit (1, 128, juce::roundToInt (atomics.sampleSliceCount.load()));
        params.pitchOffset = juce::jlimit (-48.0f, 48.0f, atomics.samplePitch.load());
        params.tempoRatio = juce::jlimit (0.25f, 4.0f, tempoRatio);
        params.density = juce::jlimit (0.5f, 220.0f, atomics.granularDensity.load());
        params.sizeMs = juce::jlimit (2.0f, 1000.0f, atomics.granularSizeMs.load());
        params.sizeRandom = juce::jlimit (0.0f, 1.0f, atomics.granularSizeRandom.load());
        params.positionSpread = juce::jlimit (0.0f, 1.0f, atomics.granularSpread.load());
        params.scanRate = juce::jlimit (-3.0f, 3.0f, atomics.granularScan.load());
        params.pitchSpread = juce::jlimit (0.0f, 36.0f, atomics.granularPitchSpread.load());
        params.panSpread = juce::jlimit (0.0f, 1.0f, atomics.granularPanSpread.load());
        params.reverseProbability = juce::jlimit (0.0f, 1.0f, atomics.granularReverse.load());
        params.texture = juce::jlimit (0.0f, 1.0f, atomics.granularTexture.load());
        params.maxGrains = juce::jlimit (1, 32, juce::roundToInt (atomics.granularMaxGrains.load()));
        params.directionMode = juce::jlimit (0, 3, juce::roundToInt (atomics.granularDirection.load()));
        params.windowShape = juce::jlimit (0, 3, juce::roundToInt (atomics.granularWindow.load()));
        params.freeze = atomics.granularFreeze.load() >= 0.5f;
        return params;
    }

    LoadedSamplePtr SampleSynthEngine::selectSample (int note, int velocity)
    {
        auto list = getSamples();
        if (! list) return nullptr;

        int roundRobinGroup = 0;
        for (auto& s : *list)
        {
            const auto& z = s->zone;
            if (note >= z.lowNote && note <= z.highNote
                && velocity >= z.lowVelocity && velocity <= z.highVelocity
                && z.roundRobinGroup > 0)
            {
                roundRobinGroup = juce::jlimit (1, 127, z.roundRobinGroup);
                break;
            }
        }

        if (roundRobinGroup > 0)
        {
            int matchingCount = 0;
            for (auto& s : *list)
            {
                const auto& z = s->zone;
                if (note >= z.lowNote && note <= z.highNote
                    && velocity >= z.lowVelocity && velocity <= z.highVelocity
                    && juce::jlimit (1, 127, z.roundRobinGroup) == roundRobinGroup)
                    ++matchingCount;
            }

            if (matchingCount > 0)
            {
                const int target = nextRoundRobinIndex[(size_t) roundRobinGroup] % matchingCount;
                nextRoundRobinIndex[(size_t) roundRobinGroup] = (target + 1) % matchingCount;

                int seen = 0;
                for (auto& s : *list)
                {
                    const auto& z = s->zone;
                    if (note >= z.lowNote && note <= z.highNote
                        && velocity >= z.lowVelocity && velocity <= z.highVelocity
                        && juce::jlimit (1, 127, z.roundRobinGroup) == roundRobinGroup)
                    {
                        if (seen == target)
                            return s;
                        ++seen;
                    }
                }
            }
        }

        for (auto& s : *list)
        {
            const auto& z = s->zone;
            if (note >= z.lowNote && note <= z.highNote
                && velocity >= z.lowVelocity && velocity <= z.highVelocity)
                return s;
        }
        return nullptr;
    }

    float SampleSynthEngine::velocityCurveGain (float velocity01) const noexcept
    {
        const float v = juce::jlimit (0.0f, 1.0f, velocity01);
        const float sensitivity = juce::jlimit (0.0f, 1.0f, atomics.velocitySensitivity.load());
        // sensitivity 0.5 -> gamma 1.0 (linear, legacy). 0 -> 0.5 (compress),
        // 1 -> 2.0 (expand). Returned as a multiplier relative to the linear
        // velocity the voice already applies, so the default is exactly 1.0.
        const float gamma = std::pow (4.0f, sensitivity - 0.5f);
        if (std::abs (gamma - 1.0f) < 1.0e-4f || v <= 0.0f)
            return 1.0f;
        return std::pow (v, gamma - 1.0f);
    }

    int SampleSynthEngine::selectSamplesLayered (int note, int velocity, LayeredSample* out, int maxLayers)
    {
        if (out == nullptr || maxLayers <= 0)
            return 0;

        auto list = getSamples();
        if (! list)
            return 0;

        // Equal-power velocity crossfade gain for a zone given its xfade edges.
        auto velocityXFadeGain = [] (const SampleZoneDef& z, int vel) -> float
        {
            float t = 1.0f;
            const float lowerXf = juce::jmax (0.0f, z.velocityLowerVelXFade);
            const float upperXf = juce::jmax (0.0f, z.velocityUpperVelXFade);
            if (lowerXf > 0.0f && vel < z.lowVelocity + (int) lowerXf)
                t = juce::jmin (t, ((float) vel - (float) z.lowVelocity) / lowerXf);
            if (upperXf > 0.0f && vel > z.highVelocity - (int) upperXf)
                t = juce::jmin (t, ((float) z.highVelocity - (float) vel) / upperXf);
            t = juce::jlimit (0.0f, 1.0f, t);
            // Equal-power so two overlapping layers sum to constant power.
            return std::sin (t * 0.5f * juce::MathConstants<float>::pi);
        };

        // Track which round-robin groups we have already resolved so each group
        // contributes a single chosen zone per note.
        std::array<bool, 128> rrGroupResolved {};
        rrGroupResolved.fill (false);

        int count = 0;
        for (auto& s : *list)
        {
            if (count >= maxLayers)
                break;

            const auto& z = s->zone;
            if (note < z.lowNote || note > z.highNote
                || velocity < z.lowVelocity || velocity > z.highVelocity)
                continue;

            const int rrGroup = juce::jlimit (0, 127, z.roundRobinGroup);
            if (rrGroup > 0)
            {
                if (rrGroupResolved[(size_t) rrGroup])
                    continue;
                rrGroupResolved[(size_t) rrGroup] = true;

                auto chosen = selectSample (note, velocity); // advances RR counter
                // selectSample returns the chosen RR member for the first group
                // it finds; only accept it if it belongs to this group.
                if (chosen != nullptr && juce::jlimit (0, 127, chosen->zone.roundRobinGroup) == rrGroup)
                {
                    out[count].sample = chosen;
                    out[count].velocityGain = velocityXFadeGain (chosen->zone, velocity);
                    ++count;
                }
                continue;
            }

            out[count].sample = s;
            out[count].velocityGain = velocityXFadeGain (z, velocity);
            ++count;
        }

        return count;
    }

    void SampleSynthEngine::noteOn (int note, float velocity)
    {
        PC_DBG("[SampleSynthEngine::noteOn] note=%d vel=%.2f hasSamples=%d", note, velocity, hasUsableSamples());
        if (atomics.retrigger.load() < 0.5f)
        {
            for (auto& v : voices)
                if (v.isActive() && v.getNote() == note)
                    return;
            for (auto& v : granularVoices)
                if (v.isActive() && v.getNote() == note)
                    return;
            for (auto& sv : sineVoices)
                if (sv.active && sv.note == note)
                {
                    sv.velocity = juce::jlimit (0.05f, 1.0f, velocity);
                    return;
                }
        }

        if (hasUsableSamples())
        {
            const int velocityInt = juce::jlimit (1, 127, (int) (velocity * 127.0f));

            LayeredSample layers[kMaxLayersPerNote];
            const int layerCount = selectSamplesLayered (note, velocityInt, layers, kMaxLayersPerNote);
            if (layerCount <= 0)
            {
                lastMissedNote.store (juce::jlimit (0, 127, note), std::memory_order_release);
                lastMissedVelocity.store (velocityInt, std::memory_order_release);
                missedNoteCount.fetch_add (1, std::memory_order_relaxed);
                return;
            }

            lastMissedNote.store (-1, std::memory_order_release);

            // Global velocity-sensitivity curve (1.0 == legacy linear).
            const float velocityCurve = velocityCurveGain (velocity);

            // Trigger one zone as either a granular or a standard sample voice.
            // layerGain carries the per-zone velocity-crossfade weight.
            auto triggerLayer = [&] (const LoadedSamplePtr& s, float layerGain)
            {
                if (s == nullptr)
                    return;

                const auto triggerIndex = triggerCounter.fetch_add (1, std::memory_order_relaxed) + 1;
                const auto triggerHash = makeTriggerHash (note, velocityInt, triggerIndex);
                const int triggerChance = juce::jlimit (0, 100, s->zone.triggerProbability);
                if (triggerChance <= 0 || (triggerChance < 100 && (int) (triggerHash % 100u) >= triggerChance))
                    return;

                const int chokeGroup = juce::jlimit (0, 127, s->zone.chokeGroup);
                if (chokeGroup > 0)
                {
                    for (auto& voice : voices)
                        if (voice.isActive() && voice.getChokeGroup() == chokeGroup)
                            voice.kill();
                    for (auto& voice : granularVoices)
                        if (voice.isActive() && voice.getChokeGroup() == chokeGroup)
                            voice.kill();
                }

                float start01 = juce::jlimit (0.0f, 1.0f, atomics.sampleStart.load());
                float length01 = juce::jlimit (0.01f, 1.0f, atomics.sampleLength.load());
                int slice = juce::roundToInt (atomics.sampleSlice.load());
                int sliceCount = juce::roundToInt (atomics.sampleSliceCount.load());
                float pitchOffset = juce::jlimit (-48.0f, 48.0f, atomics.samplePitch.load());
                bool reverse = atomics.sampleReverse.load() >= 0.5f;
                int padIndex = s->zone.padIndex;
                if (padIndex < 0 || padIndex >= 16)
                {
                    const int notePadIndex = note - 36;
                    if (notePadIndex >= 0 && notePadIndex < 16)
                        padIndex = notePadIndex;
                }

                float padGain = 1.0f;
                float padPanOffset = 0.0f;
                if (padIndex >= 0 && padIndex < 16)
                {
                    padGain = juce::jlimit (0.0f, 2.0f, atomics.padVolume[(size_t) padIndex].load());
                    pitchOffset += juce::jlimit (-24.0f, 24.0f, atomics.padPitch[(size_t) padIndex].load());
                    padPanOffset = juce::jlimit (-1.0f, 1.0f, atomics.padPan[(size_t) padIndex].load());
                }

                float tempoRatio = 1.0f;
                if (atomics.bpmSync.load() >= 0.5f && s->zone.bpm > 0.0f)
                {
                    const float hostBpm = (float) RenderContext::sanitiseBpm (renderContext.bpm);
                    const float sampleBpm = (float) RenderContext::sanitiseBpm ((double) s->zone.bpm);
                    tempoRatio = juce::jlimit (0.25f, 4.0f, hostBpm / juce::jmax (1.0f, sampleBpm));
                }

                const float glitchAmount = juce::jlimit (0.0f, 1.0f, atomics.sampleGlitch.load());
                if (glitchAmount > 0.001f)
                {
                    const bool shouldGlitch = (triggerHash % 10000u) < (juce::uint32) juce::roundToInt (glitchAmount * 10000.0f);
                    if (shouldGlitch)
                    {
                        const int glitchGrid = juce::jlimit (2, 64, juce::roundToInt (atomics.sampleGlitchGrid.load()));
                        sliceCount = juce::jmax (sliceCount, glitchGrid);
                        slice = (int) ((triggerHash >> 8) % (juce::uint32) sliceCount);
                        const float minLength = 1.0f / (float) juce::jmax (2, sliceCount);
                        length01 = juce::jmin (length01, juce::jmap (glitchAmount, 0.0f, 1.0f, 0.35f, minLength));
                        start01 = juce::jlimit (0.0f, 1.0f, start01 + ((triggerHash >> 16) % 1000u) / 1000.0f * glitchAmount * 0.08f);
                        if (glitchAmount > 0.72f && ((triggerHash >> 4) & 1u) != 0)
                            reverse = ! reverse;
                    }
                }

                const float extraGain = juce::jlimit (0.0f, 4.0f, velocityCurve * layerGain);

                const double tempoRegionSeconds = selectedRegionSeconds (*s, start01, length01, slice, sliceCount);
                const bool tempoStretchCandidate = atomics.bpmSync.load() >= 0.5f
                                                && s->zone.bpm > 0.0f
                                                && ! s->zone.oneShot
                                                && tempoRegionSeconds >= 0.85
                                                && std::abs (tempoRatio - 1.0f) > 0.015f;
                if (atomics.granularOn.load() >= 0.5f || tempoStretchCandidate)
                {
                    if (auto* v = findFreeGranularVoice())
                    {
                        auto params = currentGranularParams (tempoRatio);
                        params.sampleStart = start01;
                        params.sampleLength = length01;
                        params.sampleSlice = slice;
                        params.sampleSliceCount = sliceCount;
                        params.pitchOffset = pitchOffset;
                        params.padGain = padGain * extraGain;
                        params.padPanOffset = padPanOffset;
                        if (tempoStretchCandidate)
                        {
                            params.density = juce::jmax (params.density, 42.0f);
                            params.sizeMs = juce::jlimit (36.0f, 140.0f, (float) (tempoRegionSeconds * 1000.0 / 18.0));
                            params.sizeRandom = juce::jmin (params.sizeRandom, 0.08f);
                            params.positionSpread = juce::jmin (params.positionSpread, 0.035f);
                            params.pitchSpread = 0.0f;
                            params.panSpread = juce::jmin (params.panSpread, 0.12f);
                            params.texture = juce::jmin (params.texture, 0.18f);
                            params.maxGrains = juce::jmax (params.maxGrains, 20);
                            params.windowShape = 3;
                            params.directionMode = 0;
                            params.scanRate = tempoRegionSeconds > 0.0
                                ? (float) juce::jlimit (0.01, 12.0, (double) tempoRatio / tempoRegionSeconds)
                                : tempoRatio;
                        }
                        v->start (s, note, velocity, currentAdsr(), params, triggerHash);
                    }
                    return;
                }

                if (auto* v = findFreeVoice())
                {
                    const auto order = voiceStartCounter.fetch_add (1, std::memory_order_relaxed) + 1;
                    v->start (s, note, velocity, currentAdsr(), false,
                              start01, length01, slice, sliceCount, pitchOffset, reverse, tempoRatio,
                              padGain, padPanOffset, extraGain, order);
                }
            };

            for (int i = 0; i < layerCount; ++i)
                triggerLayer (layers[i].sample, layers[i].velocityGain);

            return;
        }

        // Fallback voice - keeps preview audible without imported samples.
        // Notes in the GM percussion range synthesize a drum hit so drum grids
        // and pads sound like drums; everything else stays a melodic sine.
        if (auto* sv = findFreeSineVoice())
        {
            sv->active   = true;
            sv->note     = note;
            sv->phase    = 0.0;
            sv->phase2   = 0.0;
            sv->velocity = juce::jlimit (0.05f, 1.0f, velocity);
            sv->drumType = drumTypeForNote (note);
            sv->drumAgeSamples = 0.0;
            sv->prevNoise = 0.0f;
            sv->noiseState = 0x9e3779b9u ^ (juce::uint32) (note * 2654435761u);
            sv->env.setParameters (currentAdsr());
            sv->env.reset();
            sv->env.noteOn();
        }
    }

    int SampleSynthEngine::drumTypeForNote (int note) noexcept
    {
        // Map the General-MIDI percussion range to our synth-drum voices.
        switch (note)
        {
            case 35: case 36:           return 0; // kick
            case 38: case 40:           return 1; // snare
            case 37:                    return 6; // rim
            case 39:                    return 4; // clap
            case 42: case 44:           return 2; // closed hat
            case 46:                    return 3; // open hat
            case 49: case 52: case 55:
            case 57:                    return 7; // crash
            case 51: case 53: case 59:  return 8; // ride
            case 41: case 43: case 45:
            case 47: case 48: case 50:  return 5; // toms
            default:                    return -1; // melodic
        }
    }

    void SampleSynthEngine::noteOff (int note)
    {
        for (auto& v : voices)
            if (v.isActive() && v.getNote() == note)
            {
                if (! v.isOneShot())
                    v.release();
            }
        for (auto& v : granularVoices)
            if (v.isActive() && v.getNote() == note)
                v.release();
        for (auto& sv : sineVoices)
            if (sv.active && sv.note == note)
                sv.env.noteOff();
    }

    void SampleSynthEngine::allNotesOff()
    {
        for (auto& v : voices) v.release();
        for (auto& v : granularVoices) v.release();
        for (auto& sv : sineVoices) if (sv.active) sv.env.noteOff();
    }

    void SampleSynthEngine::setParameter (const juce::String& id, float value)
    {
        if      (id == "attack")        atomics.attack       = value;
        else if (id == "decay")         atomics.decay        = value;
        else if (id == "sustain")       atomics.sustain      = value;
        else if (id == "release")       atomics.release      = value;
        else if (id == "sampleStart")   atomics.sampleStart  = value;
        else if (id == "sampleLength")  atomics.sampleLength = value;
        else if (id == "sampleSlice")   atomics.sampleSlice  = value;
        else if (id == "sampleSliceCount") atomics.sampleSliceCount = value;
        else if (id == "samplePitch")   atomics.samplePitch  = value;
        else if (id == "sampleReverse") atomics.sampleReverse = value;
        else if (id == "sampleGlitch")  atomics.sampleGlitch = value;
        else if (id == "sampleGlitchGrid") atomics.sampleGlitchGrid = value;
        else if (id == "granularOn") atomics.granularOn = value;
        else if (id == "granularDensity") atomics.granularDensity = value;
        else if (id == "granularSizeMs") atomics.granularSizeMs = value;
        else if (id == "granularSizeRandom") atomics.granularSizeRandom = value;
        else if (id == "granularSpread") atomics.granularSpread = value;
        else if (id == "granularScan") atomics.granularScan = value;
        else if (id == "granularPitchSpread") atomics.granularPitchSpread = value;
        else if (id == "granularPanSpread") atomics.granularPanSpread = value;
        else if (id == "granularReverse") atomics.granularReverse = value;
        else if (id == "granularTexture") atomics.granularTexture = value;
        else if (id == "granularMaxGrains") atomics.granularMaxGrains = value;
        else if (id == "granularDirection") atomics.granularDirection = value;
        else if (id == "granularWindow") atomics.granularWindow = value;
        else if (id == "granularFreeze") atomics.granularFreeze = value;
        else if (id == "filterCutoff")  atomics.cutoff       = value;
        else if (id == "filterResonance") atomics.resonance  = value;
        else if (id == "reverbMix")     atomics.reverbMix    = value;
        else if (id == "delayTime")     atomics.delayTime    = value;
        else if (id == "delayFeedback") atomics.delayFb      = value;
        else if (id == "delayMix")      atomics.delayMix     = value;
        else if (id == "volume")        atomics.volume       = value;
        else if (id == "expression")    atomics.expression   = value;
        else if (id == "pan")           atomics.pan          = value;
        else if (id == "retrigger")     atomics.retrigger    = value;
        else if (id == "bpmSync")       atomics.bpmSync      = value;
        else if (id == "velocitySensitivity") atomics.velocitySensitivity = value;
        else
        {
            const int padVolumeIndex = parsePadControlIndex (id, "Volume");
            const int padPitchIndex = parsePadControlIndex (id, "Pitch");
            const int padPanIndex = parsePadControlIndex (id, "Pan");

            if (padVolumeIndex >= 0)
                atomics.padVolume[(size_t) padVolumeIndex].store (juce::jlimit (0.0f, 2.0f, value), std::memory_order_relaxed);
            else if (padPitchIndex >= 0)
                atomics.padPitch[(size_t) padPitchIndex].store (juce::jlimit (-24.0f, 24.0f, value), std::memory_order_relaxed);
            else if (padPanIndex >= 0)
                atomics.padPan[(size_t) padPanIndex].store (juce::jlimit (-1.0f, 1.0f, value), std::memory_order_relaxed);
            else if (id.startsWithIgnoreCase ("eq"))
                eq.setParameter (id, value);
            else if (advancedFx.setParameter (id, value)) {}
            else
                utility.setParameter (id, value);
        }
    }

    int SampleSynthEngine::getActiveVoiceCount() const noexcept
    {
        int count = 0;
        for (auto& v : voices) if (v.isActive()) ++count;
        for (auto& v : granularVoices) if (v.isActive()) ++count;
        for (auto& sv : sineVoices) if (sv.active) ++count;
        return count;
    }

    int SampleSynthEngine::getLoadedSampleCount() const noexcept
    {
        return loadedSampleCount.load (std::memory_order_acquire);
    }

    juce::String SampleSynthEngine::getDiagnosticStatus() const
    {
        const int requested = requestedZoneCount.load (std::memory_order_acquire);
        const int loaded = loadedSampleCount.load (std::memory_order_acquire);
        const int missing = missingSampleCount.load (std::memory_order_acquire);
        const int failed = failedSampleCount.load (std::memory_order_acquire);

        juce::String text;
        if (requested <= 0)
            text = "Sampler: no zones mapped. Import samples in Sample Mapper.";
        else if (loaded <= 0)
            text = "Sampler: 0/" + juce::String (requested) + " samples loaded; fallback tone active.";
        else
            text = "Sampler: " + juce::String (loaded) + "/" + juce::String (requested) + " samples loaded.";

        juce::String missingName;
        juce::String failedName;
        {
            const juce::ScopedLock lock (diagnosticsLock);
            missingName = firstMissingSample;
            failedName = firstFailedSample;
        }

        if (missing > 0)
            text << " Missing " << missing << (missingName.isNotEmpty() ? " (" + missingName + ")" : juce::String());
        if (failed > 0)
            text << " Decode failed " << failed << (failedName.isNotEmpty() ? " (" + failedName + ")" : juce::String());

        const int missedNote = lastMissedNote.load (std::memory_order_acquire);
        if (missedNote >= 0)
            text << " No zone for " << midiNoteName (missedNote)
                 << " / V" << lastMissedVelocity.load (std::memory_order_acquire) << ".";

        return text;
    }

    void SampleSynthEngine::process (juce::AudioBuffer<float>& buffer,
                                     int startSample, int numSamples)
    {
        if (numSamples <= 0) return;
        const int numChans = juce::jmin (2, buffer.getNumChannels());

        // Render voices into temp buffer, then apply global FX.
        if (numChans > preparedChannels || numSamples > preparedMaxSamples)
            return;
        tempBuffer.clear (0, numSamples);

        // Render sample voices (no lock needed - voices are audio-thread-only)
        for (auto& v : voices)
            if (v.isActive())
                v.render (tempBuffer, 0, numSamples);
        const auto granularParams = currentGranularParams();
        for (auto& v : granularVoices)
            if (v.isActive())
                v.render (tempBuffer, 0, numSamples, granularParams);

        // Sine fallback voices
        if (! hasUsableSamples())
        {
            auto* L = tempBuffer.getWritePointer (0);
            auto* R = tempBuffer.getWritePointer (juce::jmin (1, tempBuffer.getNumChannels() - 1));
            const double twoPi = juce::MathConstants<double>::twoPi;
            auto nextNoise = [] (juce::uint32& s) -> float
            {
                s = s * 1664525u + 1013904223u;
                return (float) ((int) (s >> 9) & 0x7fffff) / 4194304.0f - 1.0f;
            };

            for (auto& sv : sineVoices)
            {
                if (! sv.active) continue;

                if (sv.drumType < 0)
                {
                    // Melodic sine preview.
                    const double freq = 440.0 * std::pow (2.0, (sv.note - 69) / 12.0);
                    const double dPhase = twoPi * freq / sampleRate;
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float ev = sv.env.getNextSample();
                        if (! sv.env.isActive()) { sv.active = false; break; }
                        const float s = (float) std::sin (sv.phase) * sv.velocity * ev * 0.18f;
                        L[i] += s;
                        if (R != L) R[i] += s;
                        sv.phase += dPhase;
                        if (sv.phase > twoPi) sv.phase -= twoPi;
                    }
                    continue;
                }

                // Synthesized drum voice (one-shot, self-terminating).
                for (int i = 0; i < numSamples; ++i)
                {
                    const double t = sv.drumAgeSamples / sampleRate;
                    float out = 0.0f;
                    float decayTime = 0.2f;

                    switch (sv.drumType)
                    {
                        case 0: // kick: pitch-swept sine + click
                        {
                            decayTime = 0.26f;
                            const double freq = 45.0 + 95.0 * std::exp (-t / 0.028);
                            sv.phase += twoPi * freq / sampleRate;
                            const float body = (float) std::sin (sv.phase) * std::exp (-(float) t / 0.20f);
                            const float click = t < 0.003 ? nextNoise (sv.noiseState) * 0.5f : 0.0f;
                            out = body + click;
                            break;
                        }
                        case 1: // snare: 185Hz tone + bright noise
                        {
                            decayTime = 0.19f;
                            sv.phase += twoPi * 185.0 / sampleRate;
                            const float tone = (float) std::sin (sv.phase) * std::exp (-(float) t / 0.075f) * 0.5f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise; sv.prevNoise = n;
                            out = tone + hp * std::exp (-(float) t / 0.11f) * 0.9f;
                            break;
                        }
                        case 2: // closed hat: very short high-passed noise
                        {
                            decayTime = 0.05f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise; sv.prevNoise = n;
                            out = hp * std::exp (-(float) t / 0.022f) * 0.7f;
                            break;
                        }
                        case 3: // open hat
                        {
                            decayTime = 0.34f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise; sv.prevNoise = n;
                            out = hp * std::exp (-(float) t / 0.18f) * 0.6f;
                            break;
                        }
                        case 4: // clap: noise with quick repeats
                        {
                            decayTime = 0.18f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise * 0.5f; sv.prevNoise = n;
                            const float shape = (t < 0.01 || (t > 0.02 && t < 0.03)) ? 1.0f : std::exp (-(float) t / 0.09f);
                            out = hp * shape * 0.8f;
                            break;
                        }
                        case 5: // tom: pitched sine by note + slight sweep
                        {
                            decayTime = 0.30f;
                            const double base = 90.0 + (double) (sv.note - 41) * 14.0;
                            const double freq = base * (1.0 + 0.4 * std::exp (-t / 0.05));
                            sv.phase += twoPi * juce::jmax (40.0, freq) / sampleRate;
                            out = (float) std::sin (sv.phase) * std::exp (-(float) t / 0.22f);
                            break;
                        }
                        case 7: // crash: long bright noise
                        {
                            decayTime = 1.1f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise; sv.prevNoise = n;
                            out = hp * std::exp (-(float) t / 0.65f) * 0.5f;
                            break;
                        }
                        case 8: // ride: brighter, medium decay with ping
                        {
                            decayTime = 0.7f;
                            sv.phase += twoPi * 520.0 / sampleRate;
                            const float ping = (float) std::sin (sv.phase) * std::exp (-(float) t / 0.10f) * 0.3f;
                            const float n = nextNoise (sv.noiseState);
                            const float hp = n - sv.prevNoise; sv.prevNoise = n;
                            out = ping + hp * std::exp (-(float) t / 0.45f) * 0.35f;
                            break;
                        }
                        default: // rim (6): short bright tick
                        {
                            decayTime = 0.06f;
                            sv.phase += twoPi * 1700.0 / sampleRate;
                            const float tick = (float) std::sin (sv.phase) * std::exp (-(float) t / 0.012f);
                            const float n = nextNoise (sv.noiseState) * 0.4f;
                            out = (tick + n) * std::exp (-(float) t / 0.03f);
                            break;
                        }
                    }

                    const float ampEnv = std::exp (-(float) t / decayTime);
                    if (t > decayTime * 6.0) { sv.active = false; break; }

                    if (sv.phase > twoPi) sv.phase -= twoPi;
                    const float s = out * ampEnv * sv.velocity * 0.5f;
                    L[i] += s;
                    if (R != L) R[i] += s;
                    sv.drumAgeSamples += 1.0;
                }
            }
        }

        // Filter
        filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, atomics.cutoff.load()));
        filter.setResonance      (juce::jlimit (0.05f, 1.5f, atomics.resonance.load() * 1.5f + 0.05f));

        juce::dsp::AudioBlock<float> block (tempBuffer.getArrayOfWritePointers(),
                                            (size_t) numChans, 0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
        eq.process (tempBuffer, 0, numSamples);
        advancedFx.process (tempBuffer, 0, numSamples);

        // Delay (mono-ish ping-pong)
        const float dTime = juce::jlimit (0.0f, 2.0f, atomics.delayTime.load());
        const float dFb   = juce::jlimit (0.0f, 0.95f, atomics.delayFb.load());
        const float dMix  = juce::jlimit (0.0f, 1.0f, atomics.delayMix.load());
        const int   dSamples = juce::jmax (1, (int) (dTime * sampleRate));
        delayL.setDelay ((float) dSamples);
        delayR.setDelay ((float) dSamples);

        auto* L = tempBuffer.getWritePointer (0);
        auto* R = numChans > 1 ? tempBuffer.getWritePointer (1) : L;
        for (int i = 0; i < numSamples; ++i)
        {
            const float dl = delayL.popSample (0);
            const float dr = delayR.popSample (0);
            delayL.pushSample (0, L[i] + dr * dFb);
            delayR.pushSample (0, R[i] + dl * dFb);
            L[i] = L[i] * (1.0f - dMix * 0.5f) + dl * dMix;
            R[i] = R[i] * (1.0f - dMix * 0.5f) + dr * dMix;
        }

        // Reverb
        const float rvMix = juce::jlimit (0.0f, 1.0f, atomics.reverbMix.load());
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.6f;
        rp.damping  = 0.4f;
        rp.wetLevel = rvMix * 0.6f;
        rp.dryLevel = 1.0f - rvMix * 0.5f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
        utility.processOutput (tempBuffer, 0, numSamples);

        // Master volume + pan
        const float vol = juce::jlimit (0.0f, 2.0f, atomics.volume.load())
                        * juce::jlimit (0.0f, 1.0f, atomics.expression.load());
        const float pan = juce::jlimit (-1.0f, 1.0f, atomics.pan.load());
        const float lG = vol * std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        const float rG = vol * std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = L[i] * lG;
            const float r = R[i] * rG;
            buffer.addSample (0, startSample + i, l);
            if (buffer.getNumChannels() > 1)
                buffer.addSample (1, startSample + i, r);
        }
    }

} // namespace patchcraft
