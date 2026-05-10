#include "SampleVoice.h"
#include "DebugLog.h"

namespace patchcraft
{
    void SampleVoice::prepare (double sampleRate)
    {
        currentSampleRate = sampleRate;
        env.setSampleRate (sampleRate);
    }

    void SampleVoice::start (const LoadedSamplePtr& s, int midiNote, float velocity,
                             const juce::ADSR::Parameters& adsr, bool /*legato*/,
                             float sampleStart01, float sampleLength01,
                             int sampleSlice, int sampleSliceCount,
                             float pitchOffset, bool reverseOverride)
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

            const auto rootRatio = std::pow (2.0, (midiNote - s->zone.rootNote + s->zone.pitchOffset + pitchOffset) / 12.0);
            pitchRatio = rootRatio * (s->sourceSampleRate / currentSampleRate);
            if (reversePlayback)
                pitchRatio = -pitchRatio;

            const float gainLin = juce::Decibels::decibelsToGain (s->zone.gainDb);
            const float pan     = juce::jlimit (-1.0f, 1.0f, s->zone.pan);
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

            const int    idx0  = (int) position;
            const int    idx1  = juce::jmin (idx0 + 1, length - 1);
            const float  frac  = (float) (position - idx0);

            const float sL = l[idx0] + frac * (l[idx1] - l[idx0]);
            const float sR = r[idx0] + frac * (r[idx1] - r[idx0]);

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

} // namespace patchcraft
