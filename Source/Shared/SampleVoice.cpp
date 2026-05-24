#include "SampleVoice.h"
#include "DebugLog.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        static float cubicSample (const float* data, int length, double position) noexcept
        {
            const int i1 = juce::jlimit (0, length - 1, (int) position);
            const int i0 = juce::jmax (0, i1 - 1);
            const int i2 = juce::jmin (length - 1, i1 + 1);
            const int i3 = juce::jmin (length - 1, i1 + 2);
            const float frac = (float) (position - (double) i1);

            const float y0 = data[i0];
            const float y1 = data[i1];
            const float y2 = data[i2];
            const float y3 = data[i3];

            const float a0 = y3 - y2 - y0 + y1;
            const float a1 = y0 - y1 - a0;
            const float a2 = y2 - y0;
            const float a3 = y1;
            return ((a0 * frac + a1) * frac + a2) * frac + a3;
        }
    }

    void SampleVoice::prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        env.setSampleRate (sampleRate);
    }

    void SampleVoice::start (const LoadedSamplePtr& s, int midiNote, float velocity,
                             const juce::ADSR::Parameters& adsr, bool /*legato*/,
                             float sampleStart01, float sampleLength01,
                             int sampleSlice, int sampleSliceCount,
                             float pitchOffset, bool reverseOverride,
                             float tempoRatio,
                             float padGain,
                             float padPanOffset)
    {
        PC_DBG("[SampleVoice::start] note=%d vel=%.2f", midiNote, velocity);

        sample      = s;
        note        = midiNote;
        oneShot     = s != nullptr && s->zone.oneShot;
        chokeGroup  = s != nullptr ? juce::jlimit (0, 127, s->zone.chokeGroup) : 0;
        envParams   = adsr;
        env.setParameters (envParams);
        env.reset();
        env.noteOn();

        velocityGain = juce::jlimit (0.05f, 1.0f, velocity);

        if (s != nullptr && s->buffer.getNumSamples() > 0 && s->buffer.getNumChannels() > 0)
        {
            const int length = s->buffer.getNumSamples();
            const int zoneStart = juce::jlimit (0, length - 1, s->zone.sampleStart);
            const int zoneEnd = s->zone.sampleEnd > zoneStart
                ? juce::jlimit (zoneStart + 1, length, s->zone.sampleEnd)
                : length;
            const int sliceCount = juce::jlimit (1, 64, sampleSliceCount);
            const int sliceIndex = juce::jlimit (0, sliceCount - 1, sampleSlice);
            const int zoneLength = juce::jmax (1, zoneEnd - zoneStart);
            const int rawSliceStart = zoneStart + (zoneLength * sliceIndex) / sliceCount;
            const int rawSliceEnd = zoneStart + (zoneLength * (sliceIndex + 1)) / sliceCount;
            const int sliceStart = juce::jlimit (zoneStart, zoneEnd - 1, rawSliceStart);
            const int sliceEnd = juce::jlimit (sliceStart + 1, zoneEnd, juce::jmax (sliceStart + 1, rawSliceEnd));
            const int sliceLength = juce::jmax (1, sliceEnd - sliceStart);
            const int startOffset = juce::roundToInt (juce::jlimit (0.0f, 1.0f, sampleStart01)
                                                    * (float) juce::jmax (0, sliceLength - 1));
            playStart = juce::jlimit (sliceStart, sliceEnd - 1, sliceStart + startOffset);
            playEnd = juce::jlimit (playStart + 1, sliceEnd,
                                    playStart + juce::roundToInt ((float) sliceLength
                                                                * juce::jlimit (0.01f, 1.0f, sampleLength01)));
            reversePlayback = s->zone.reverse || reverseOverride;
            position = reversePlayback ? (double) (playEnd - 1) : (double) playStart;

            active.store (true, std::memory_order_release);

            const auto trackedSemitones = (double) (midiNote - s->zone.rootNote)
                                        * (double) juce::jlimit (0.0f, 2.0f, s->zone.keyTracking);
            const auto rootRatio = std::pow (2.0, (trackedSemitones + s->zone.pitchOffset + pitchOffset) / 12.0);
            pitchRatio = rootRatio
                       * (double) juce::jlimit (0.25f, 4.0f, tempoRatio)
                       * (s->sourceSampleRate / currentSampleRate);
            if (reversePlayback)
                pitchRatio = -pitchRatio;

            const float gainLin = juce::Decibels::decibelsToGain (s->zone.gainDb)
                                * juce::jlimit (0.0f, 2.0f, padGain);
            const float pan     = juce::jlimit (-1.0f, 1.0f, s->zone.pan + padPanOffset);
            leftGain  = gainLin * std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            rightGain = gainLin * std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        }
        else
        {
            active.store (false, std::memory_order_release);
            PC_DBG("[SampleVoice::start] sample invalid, not activating");
        }
    }

    void SampleVoice::release()
    {
        if (active.load (std::memory_order_acquire)) env.noteOff();
    }

    void SampleVoice::kill()
    {
        active.store (false, std::memory_order_release);
        env.reset();
        sample = nullptr;
        oneShot = false;
        chokeGroup = 0;
    }

    void SampleVoice::render (juce::AudioBuffer<float>& dest, int startSample, int numSamples)
    {
        if (! active.load (std::memory_order_acquire) || ! sample)
            return;

        const auto* s = sample.get();
        if (! s) return;

        const auto& buf = s->buffer;
        const int numChans = buf.getNumChannels();
        const int length   = buf.getNumSamples();
        if (length <= 0 || numChans <= 0) { kill(); return; }
        if (playEnd <= playStart || playStart < 0 || playEnd > length)
        {
            playStart = juce::jlimit (0, length - 1, s->zone.sampleStart);
            playEnd = s->zone.sampleEnd > playStart
                ? juce::jlimit (playStart + 1, length, s->zone.sampleEnd)
                : length;
        }

        const auto* l = buf.getReadPointer (0);
        const auto* r = numChans > 1 ? buf.getReadPointer (1) : l;
        if (! l) { kill(); return; }

        auto* outL = dest.getWritePointer (0, startSample);
        auto* outR = dest.getNumChannels() > 1 ? dest.getWritePointer (1, startSample) : nullptr;
        if (! outL) { kill(); return; }

        for (int i = 0; i < numSamples; ++i)
        {
            if ((! reversePlayback && position >= (double) (playEnd - 1))
                || (reversePlayback && position <= (double) playStart))
            {
                const int loopStart = juce::jlimit (playStart, playEnd - 1, s->zone.loopStart);
                const int loopEnd = s->zone.loopEnd > loopStart
                    ? juce::jlimit (loopStart + 1, playEnd, s->zone.loopEnd)
                    : playEnd;

                if (s->zone.loopEnabled && loopEnd > loopStart + 1)
                {
                    const double loopLen = loopEnd - loopStart;
                    if (reversePlayback)
                        while (position <= loopStart)
                            position += loopLen;
                    else
                        while (position >= loopEnd)
                            position -= loopLen;
                }
                else
                {
                    kill();
                    break;
                }
            }

            const int idx0 = juce::jlimit (0, length - 1, (int) position);
            const float sL = cubicSample (l, length, position);
            const float sR = cubicSample (r, length, position);

            const float ev = env.getNextSample();
            if (! env.isActive())
            {
                kill();
                break;
            }

            const float g = ev * velocityGain;
            float fadeGain = 1.0f;
            if (s->zone.fadeInLength > 0)
            {
                const int fadeStart = s->zone.fadeInStart > 0
                    ? juce::jlimit (playStart, playEnd, s->zone.fadeInStart)
                    : playStart;
                if (idx0 >= fadeStart && idx0 < fadeStart + s->zone.fadeInLength)
                    fadeGain *= juce::jlimit (0.0f, 1.0f, (idx0 - fadeStart) / (float) s->zone.fadeInLength);
            }
            if (s->zone.fadeOutLength > 0)
            {
                const int fadeOutStart = s->zone.fadeOutStart > 0
                    ? juce::jlimit (playStart, playEnd, s->zone.fadeOutStart)
                    : juce::jmax (playStart, playEnd - s->zone.fadeOutLength);
                if (idx0 >= fadeOutStart)
                    fadeGain *= juce::jlimit (0.0f, 1.0f, (playEnd - idx0) / (float) s->zone.fadeOutLength);
            }

            outL[i] += sL * g * fadeGain * leftGain;
            if (outR != nullptr)
                outR[i] += sR * g * fadeGain * rightGain;

            position += pitchRatio;
        }
    }

    void GranularVoice::prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        env.setSampleRate (sampleRate);
    }

    void GranularVoice::start (const LoadedSamplePtr& s, int midiNote, float velocity,
                               const juce::ADSR::Parameters& adsr,
                               const Params& params,
                               juce::uint32 seed)
    {
        sample = s;
        note = midiNote;
        chokeGroup = s != nullptr ? juce::jlimit (0, 127, s->zone.chokeGroup) : 0;
        velocityGain = juce::jlimit (0.05f, 1.0f, velocity);
        envParams = adsr;
        env.setParameters (envParams);
        env.reset();
        env.noteOn();
        rngState = seed != 0 ? seed : 0x12345678u;
        samplesUntilNextGrain = 0.0;
        scanPosition = 0.0;
        alternatingDirection = 1;
        for (auto& grain : grains)
            grain.active = false;

        active.store (s != nullptr && s->buffer.getNumSamples() > 8 && s->buffer.getNumChannels() > 0,
                      std::memory_order_release);
    }

    void GranularVoice::release()
    {
        if (active.load (std::memory_order_acquire))
            env.noteOff();
    }

    void GranularVoice::kill()
    {
        active.store (false, std::memory_order_release);
        env.reset();
        sample = nullptr;
        chokeGroup = 0;
        for (auto& grain : grains)
            grain.active = false;
    }

    juce::uint32 GranularVoice::nextRandom() noexcept
    {
        rngState = rngState * 1664525u + 1013904223u;
        return rngState;
    }

    float GranularVoice::random01() noexcept
    {
        return (float) ((nextRandom() >> 8) & 0x00ffffffu) / (float) 0x01000000u;
    }

    float GranularVoice::randomSigned() noexcept
    {
        return random01() * 2.0f - 1.0f;
    }

    int GranularVoice::activeGrainCount() const noexcept
    {
        int count = 0;
        for (const auto& grain : grains)
            if (grain.active)
                ++count;
        return count;
    }

    float GranularVoice::grainWindow (const Grain& grain, int shape) const noexcept
    {
        const float phase = juce::jlimit (0.0f, 1.0f, (float) grain.age / (float) juce::jmax (1, grain.length));
        switch (juce::jlimit (0, 3, shape))
        {
            case 1:
                return 1.0f - std::abs (phase * 2.0f - 1.0f);
            case 2:
                return 0.42f - 0.50f * std::cos (juce::MathConstants<float>::twoPi * phase)
                             + 0.08f * std::cos (juce::MathConstants<float>::twoPi * 2.0f * phase);
            case 3:
            {
                constexpr float edge = 0.18f;
                if (phase < edge)
                    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * phase / edge);
                if (phase > 1.0f - edge)
                    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - phase) / edge);
                return 1.0f;
            }
            default:
                return std::sin (juce::MathConstants<float>::pi * phase);
        }
    }

    void GranularVoice::spawnGrain (const Params& params, float envelopeValue)
    {
        if (sample == nullptr || envelopeValue <= 0.00001f)
            return;

        const auto& buffer = sample->buffer;
        const int sourceLength = buffer.getNumSamples();
        if (sourceLength <= 8 || buffer.getNumChannels() <= 0)
            return;

        const int maxGrains = juce::jlimit (1, kMaxGrains, params.maxGrains);
        if (activeGrainCount() >= maxGrains)
            return;

        Grain* target = nullptr;
        for (auto& grain : grains)
        {
            if (! grain.active)
            {
                target = &grain;
                break;
            }
        }

        if (target == nullptr)
            return;

        const int zoneStart = juce::jlimit (0, sourceLength - 1, sample->zone.sampleStart);
        const int zoneEnd = sample->zone.sampleEnd > zoneStart
            ? juce::jlimit (zoneStart + 1, sourceLength, sample->zone.sampleEnd)
            : sourceLength;
        const int zoneLength = juce::jmax (1, zoneEnd - zoneStart);
        const int sliceCount = juce::jlimit (1, 128, params.sampleSliceCount);
        const int sliceIndex = juce::jlimit (0, sliceCount - 1, params.sampleSlice);
        const int sliceStart = zoneStart + (zoneLength * sliceIndex) / sliceCount;
        const int sliceEnd = zoneStart + (zoneLength * (sliceIndex + 1)) / sliceCount;
        const int availableLength = juce::jmax (8, sliceEnd - sliceStart);
        const float regionLength01 = juce::jlimit (0.01f, 1.0f, params.sampleLength);
        const int regionLength = juce::jlimit (8, availableLength, juce::roundToInt ((float) availableLength * regionLength01));

        const float basePosition = params.freeze
            ? juce::jlimit (0.0f, 1.0f, params.sampleStart)
            : juce::jlimit (0.0f, 1.0f, (float) (params.sampleStart + scanPosition));
        const float spread = juce::jlimit (0.0f, 1.0f, params.positionSpread);
        const float texture = juce::jlimit (0.0f, 1.0f, params.texture);
        const float randomOffset = randomSigned() * spread * (0.20f + texture * 0.80f);
        const int centre = sliceStart + juce::roundToInt (juce::jlimit (0.0f, 1.0f, basePosition + randomOffset)
                                                        * (float) juce::jmax (0, availableLength - 1));

        const float sizeRandom = 1.0f + randomSigned() * juce::jlimit (0.0f, 1.0f, params.sizeRandom) * 0.85f;
        const int grainLength = juce::jlimit (8, regionLength,
            juce::roundToInt (juce::jmax (2.0, currentSampleRate * (double) params.sizeMs / 1000.0) * sizeRandom));
        const int half = grainLength / 2;
        const int start = juce::jlimit (sliceStart, juce::jmax (sliceStart, sliceEnd - 2), centre - half);

        int direction = 1;
        const int directionMode = juce::jlimit (0, 3, params.directionMode);
        if (directionMode == 1)
            direction = -1;
        else if (directionMode == 2)
            direction = alternatingDirection;
        else if (directionMode == 3)
            direction = random01() < juce::jlimit (0.0f, 1.0f, params.reverseProbability) ? -1 : 1;

        const float pitchRandom = randomSigned() * juce::jlimit (0.0f, 36.0f, params.pitchSpread);
        const double trackedSemitones = (double) (note - sample->zone.rootNote)
                                      * (double) juce::jlimit (0.0f, 2.0f, sample->zone.keyTracking);
        const double pitchRatio = std::pow (2.0, (trackedSemitones
                                                + sample->zone.pitchOffset
                                                + params.pitchOffset
                                                + pitchRandom) / 12.0)
                                * (sample->sourceSampleRate / currentSampleRate);

        const float zoneGain = juce::Decibels::decibelsToGain (sample->zone.gainDb)
                             * juce::jlimit (0.0f, 2.0f, params.padGain);
        const float pan = juce::jlimit (-1.0f, 1.0f,
            sample->zone.pan + params.padPanOffset + randomSigned() * juce::jlimit (0.0f, 1.0f, params.panSpread));
        const float grainGain = zoneGain * velocityGain * 1.35f
                              / std::sqrt ((float) juce::jmax (1, maxGrains));

        target->active = true;
        target->position = direction >= 0 ? (double) start : (double) juce::jmin (sliceEnd - 2, start + grainLength);
        target->step = pitchRatio * (double) direction;
        target->age = 0;
        target->length = grainLength;
        target->leftGain = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        target->rightGain = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        target->gain = grainGain;
    }

    void GranularVoice::render (juce::AudioBuffer<float>& dest, int startSample, int numSamples, const Params& params)
    {
        if (! active.load (std::memory_order_acquire) || sample == nullptr)
            return;

        const auto& buffer = sample->buffer;
        const int sourceLength = buffer.getNumSamples();
        const int numSourceChannels = buffer.getNumChannels();
        if (sourceLength <= 8 || numSourceChannels <= 0)
        {
            kill();
            return;
        }

        auto* outL = dest.getWritePointer (0, startSample);
        auto* outR = dest.getNumChannels() > 1 ? dest.getWritePointer (1, startSample) : nullptr;
        if (outL == nullptr)
            return;

        const auto* srcL = buffer.getReadPointer (0);
        const auto* srcR = numSourceChannels > 1 ? buffer.getReadPointer (1) : srcL;
        const float density = juce::jlimit (0.5f, 220.0f, params.density);
        const double baseInterval = currentSampleRate / (double) density;
        const float texture = juce::jlimit (0.0f, 1.0f, params.texture);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            const float envValue = env.getNextSample();
            const bool envelopeActive = env.isActive();
            bool anyGrainsActive = false;

            if (envelopeActive)
            {
                if (! params.freeze)
                {
                    scanPosition += (double) params.scanRate / juce::jmax (1.0, currentSampleRate);
                    if (params.directionMode == 2)
                    {
                        if (scanPosition >= 1.0)
                        {
                            scanPosition = 1.0;
                            alternatingDirection = -1;
                        }
                        else if (scanPosition <= 0.0)
                        {
                            scanPosition = 0.0;
                            alternatingDirection = 1;
                        }
                    }
                    else
                    {
                        while (scanPosition > 1.0)
                            scanPosition -= 1.0;
                        while (scanPosition < 0.0)
                            scanPosition += 1.0;
                    }
                }

                samplesUntilNextGrain -= 1.0;
                while (samplesUntilNextGrain <= 0.0)
                {
                    spawnGrain (params, envValue);
                    const double jitter = 1.0 + (double) randomSigned() * 0.75 * (double) texture;
                    samplesUntilNextGrain += juce::jmax (4.0, baseInterval * juce::jlimit (0.20, 2.50, jitter));
                }
            }

            float accumL = 0.0f;
            float accumR = 0.0f;
            for (auto& grain : grains)
            {
                if (! grain.active)
                    continue;

                const int idx0 = (int) grain.position;
                if (grain.age >= grain.length || idx0 < 0 || idx0 >= sourceLength - 1)
                {
                    grain.active = false;
                    continue;
                }

                const float sL = cubicSample (srcL, sourceLength, grain.position);
                const float sR = cubicSample (srcR, sourceLength, grain.position);
                const float window = grainWindow (grain, params.windowShape);
                const float gain = window * grain.gain * envValue;
                accumL += sL * gain * grain.leftGain;
                accumR += sR * gain * grain.rightGain;

                grain.position += grain.step;
                ++grain.age;
                anyGrainsActive = true;
            }

            outL[sampleIndex] += accumL;
            if (outR != nullptr)
                outR[sampleIndex] += accumR;

            if (! envelopeActive && ! anyGrainsActive)
            {
                kill();
                break;
            }
        }
    }

} // namespace patchcraft
