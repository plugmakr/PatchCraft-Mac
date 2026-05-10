#include "AdvancedEqProcessor.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        constexpr float kMinFreq = 20.0f;
        constexpr float kMaxFreq = 20000.0f;

        static AdvancedEqProcessor::Coefficients normalise (double b0, double b1, double b2,
                                                            double a0, double a1, double a2)
        {
            if (! std::isfinite (a0) || std::abs (a0) < 1.0e-12)
                return {};

            AdvancedEqProcessor::Coefficients c;
            c.b0 = (float) (b0 / a0);
            c.b1 = (float) (b1 / a0);
            c.b2 = (float) (b2 / a0);
            c.a1 = (float) (a1 / a0);
            c.a2 = (float) (a2 / a0);
            if (! std::isfinite (c.b0) || ! std::isfinite (c.b1) || ! std::isfinite (c.b2)
                || ! std::isfinite (c.a1) || ! std::isfinite (c.a2))
                return {};
            return c;
        }

        static double clampFrequency (float freq, double sampleRate)
        {
            return juce::jlimit ((double) kMinFreq,
                                 juce::jmin ((double) kMaxFreq, sampleRate * 0.45),
                                 (double) freq);
        }

        static float detectorLevelForMode (const juce::AudioBuffer<float>& buffer,
                                           int startSample,
                                           int numSamples,
                                           int numChannels,
                                           int mode)
        {
            if (numSamples <= 0 || numChannels <= 0)
                return 0.0f;

            double sumSquares = 0.0;
            int count = 0;

            if (numChannels == 1)
            {
                const auto* samples = buffer.getReadPointer (0, startSample);
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const double value = samples[sample];
                    sumSquares += value * value;
                }
                count = numSamples;
            }
            else
            {
                const auto* left = buffer.getReadPointer (0, startSample);
                const auto* right = buffer.getReadPointer (1, startSample);
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    double value = 0.0;
                    switch (juce::jlimit (0, 4, mode))
                    {
                        case 1:  value = left[sample]; break;
                        case 2:  value = right[sample]; break;
                        case 3:  value = (left[sample] + right[sample]) * 0.5; break;
                        case 4:  value = (left[sample] - right[sample]) * 0.5; break;
                        default:
                        {
                            const double l = left[sample];
                            const double r = right[sample];
                            sumSquares += l * l + r * r;
                            count += 2;
                            continue;
                        }
                    }
                    sumSquares += value * value;
                    ++count;
                }
            }

            return count > 0 ? std::sqrt ((float) (sumSquares / (double) count)) : 0.0f;
        }

        static float smoothingCoefficient (float milliseconds, double sampleRate, int numSamples)
        {
            const double seconds = juce::jmax (0.0001, (double) milliseconds * 0.001);
            return (float) std::exp (-((double) numSamples) / (sampleRate * seconds));
        }

        static float dynamicGainForMode (int dynMode, float envelopeDb, float thresholdDb, float rangeDb)
        {
            const float range = std::abs (juce::jlimit (-24.0f, 24.0f, rangeDb));

            switch (juce::jlimit (0, 3, dynMode))
            {
                case 1:
                {
                    const float above = juce::jmax (0.0f, envelopeDb - thresholdDb);
                    return -range * juce::jlimit (0.0f, 1.0f, above / 24.0f);
                }
                case 2:
                {
                    const float above = juce::jmax (0.0f, envelopeDb - thresholdDb);
                    return range * juce::jlimit (0.0f, 1.0f, above / 24.0f);
                }
                case 3:
                {
                    const float below = juce::jmax (0.0f, thresholdDb - envelopeDb);
                    return -range * juce::jlimit (0.0f, 1.0f, below / 24.0f);
                }
                default:
                    return 0.0f;
            }
        }
    }

    void AdvancedEqProcessor::prepare (double sr, int, int numChannels)
    {
        sampleRate = juce::jmax (8000.0, sr);
        channelCount = juce::jmax (1, numChannels);
        states.assign ((size_t) channelCount * (size_t) kMaxBands, {});
    }

    void AdvancedEqProcessor::reset()
    {
        for (auto& state : states)
            state = {};
        detectorEnvelope.fill (0.0f);
    }

    bool AdvancedEqProcessor::setParameter (const juce::String& id, float value)
    {
        if (id == "eqEnabled")      { enabled = value; return true; }
        if (id == "eqMix")          { mix = value; return true; }
        if (id == "eqOutputTrimDb") { outputTrimDb = value; return true; }

        for (int i = 0; i < kMaxBands; ++i)
        {
            const auto prefix = "eqBand" + juce::String (i + 1);
            if (! id.startsWith (prefix))
                continue;

            auto& band = bands[(size_t) i];
            const auto suffix = id.substring (prefix.length());
            if (suffix == "On")     { band.enabled = value; return true; }
            if (suffix == "Type")   { band.type = value; return true; }
            if (suffix == "Mode")   { band.mode = value; return true; }
            if (suffix == "Freq")   { band.freq = value; return true; }
            if (suffix == "GainDb") { band.gainDb = value; return true; }
            if (suffix == "Q")      { band.q = value; return true; }
            if (suffix == "Solo")   { band.solo = value; return true; }
            if (suffix == "DynMode")        { band.dynMode = value; return true; }
            if (suffix == "DynThresholdDb") { band.dynThresholdDb = value; return true; }
            if (suffix == "DynRangeDb")     { band.dynRangeDb = value; return true; }
            if (suffix == "DynAttackMs")    { band.dynAttackMs = value; return true; }
            if (suffix == "DynReleaseMs")   { band.dynReleaseMs = value; return true; }
        }

        return false;
    }

    AdvancedEqProcessor::State& AdvancedEqProcessor::stateFor (int bandIndex, int channel)
    {
        const auto index = (size_t) bandIndex * (size_t) channelCount
                         + (size_t) juce::jlimit (0, channelCount - 1, channel);
        return states[index];
    }

    AdvancedEqProcessor::Coefficients AdvancedEqProcessor::makeCoefficients (int type, float freq, float gainDb,
                                                                             float q, double sr)
    {
        const double f = clampFrequency (freq, sr);
        const double omega = juce::MathConstants<double>::twoPi * f / sr;
        const double sinw = std::sin (omega);
        const double cosw = std::cos (omega);
        const double qSafe = juce::jlimit (0.10, 18.0, (double) q);
        const double alpha = sinw / (2.0 * qSafe);
        const double a = std::pow (10.0, (double) gainDb / 40.0);

        switch (juce::jlimit (0, 5, type))
        {
            case 1:
            {
                const double shelfSlope = juce::jlimit (0.10, 2.0, qSafe);
                const double shelfAlpha = sinw * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / shelfSlope - 1.0) + 2.0);
                const double rootA = std::sqrt (a);
                return normalise (
                    a * ((a + 1.0) - (a - 1.0) * cosw + 2.0 * rootA * shelfAlpha),
                    2.0 * a * ((a - 1.0) - (a + 1.0) * cosw),
                    a * ((a + 1.0) - (a - 1.0) * cosw - 2.0 * rootA * shelfAlpha),
                    (a + 1.0) + (a - 1.0) * cosw + 2.0 * rootA * shelfAlpha,
                    -2.0 * ((a - 1.0) + (a + 1.0) * cosw),
                    (a + 1.0) + (a - 1.0) * cosw - 2.0 * rootA * shelfAlpha);
            }

            case 2:
            {
                const double shelfSlope = juce::jlimit (0.10, 2.0, qSafe);
                const double shelfAlpha = sinw * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / shelfSlope - 1.0) + 2.0);
                const double rootA = std::sqrt (a);
                return normalise (
                    a * ((a + 1.0) + (a - 1.0) * cosw + 2.0 * rootA * shelfAlpha),
                    -2.0 * a * ((a - 1.0) + (a + 1.0) * cosw),
                    a * ((a + 1.0) + (a - 1.0) * cosw - 2.0 * rootA * shelfAlpha),
                    (a + 1.0) - (a - 1.0) * cosw + 2.0 * rootA * shelfAlpha,
                    2.0 * ((a - 1.0) - (a + 1.0) * cosw),
                    (a + 1.0) - (a - 1.0) * cosw - 2.0 * rootA * shelfAlpha);
            }

            case 3:
                return normalise ((1.0 + cosw) * 0.5, -(1.0 + cosw), (1.0 + cosw) * 0.5,
                                  1.0 + alpha, -2.0 * cosw, 1.0 - alpha);

            case 4:
                return normalise ((1.0 - cosw) * 0.5, 1.0 - cosw, (1.0 - cosw) * 0.5,
                                  1.0 + alpha, -2.0 * cosw, 1.0 - alpha);

            case 5:
                return normalise (1.0, -2.0 * cosw, 1.0,
                                  1.0 + alpha, -2.0 * cosw, 1.0 - alpha);

            default:
                return normalise (1.0 + alpha * a, -2.0 * cosw, 1.0 - alpha * a,
                                  1.0 + alpha / a, -2.0 * cosw, 1.0 - alpha / a);
        }
    }

    void AdvancedEqProcessor::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (numSamples <= 0 || enabled.load() < 0.5f)
            return;

        const int numChannels = juce::jmin (channelCount, buffer.getNumChannels());
        if (numChannels <= 0 || states.size() < (size_t) numChannels * (size_t) kMaxBands)
            return;

        juce::ScopedNoDenormals noDenormals;
        const float wetMix = juce::jlimit (0.0f, 1.0f, mix.load());
        const float trim = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 24.0f, outputTrimDb.load()));
        std::array<Coefficients, kMaxBands> coefficients;
        std::array<bool, kMaxBands> activeBands {};
        std::array<int, kMaxBands> bandModes {};
        bool anyBandActive = false;
        bool anySolo = false;
        for (const auto& band : bands)
            anySolo = anySolo || band.solo.load() >= 0.5f;

        for (int bandIndex = 0; bandIndex < kMaxBands; ++bandIndex)
        {
            const auto& band = bands[(size_t) bandIndex];
            activeBands[(size_t) bandIndex] = band.enabled.load() >= 0.5f
                                           && (! anySolo || band.solo.load() >= 0.5f);
            if (! activeBands[(size_t) bandIndex])
                continue;

            const int mode = juce::jlimit (0, 4, juce::roundToInt (band.mode.load()));
            float effectiveGainDb = band.gainDb.load();
            const int dynMode = juce::jlimit (0, 3, juce::roundToInt (band.dynMode.load()));
            if (dynMode > 0 && std::abs (band.dynRangeDb.load()) > 0.001f)
            {
                const float detected = detectorLevelForMode (buffer, startSample, numSamples, numChannels, mode);
                const float previous = detectorEnvelope[(size_t) bandIndex];
                const float coefficient = smoothingCoefficient (detected > previous ? band.dynAttackMs.load()
                                                                                    : band.dynReleaseMs.load(),
                                                                sampleRate,
                                                                numSamples);
                const float smoothed = detected + (previous - detected) * coefficient;
                detectorEnvelope[(size_t) bandIndex] = smoothed;

                const float envelopeDb = juce::Decibels::gainToDecibels (juce::jmax (smoothed, 0.000001f), -120.0f);
                effectiveGainDb += dynamicGainForMode (dynMode,
                                                       envelopeDb,
                                                       juce::jlimit (-80.0f, 12.0f, band.dynThresholdDb.load()),
                                                       band.dynRangeDb.load());
            }

            coefficients[(size_t) bandIndex] = makeCoefficients (juce::roundToInt (band.type.load()),
                                                                  band.freq.load(),
                                                                  effectiveGainDb,
                                                                  band.q.load(),
                                                                  sampleRate);
            bandModes[(size_t) bandIndex] = mode;
            anyBandActive = true;
        }

        if (! anyBandActive && std::abs (trim - 1.0f) < 0.00001f)
            return;

        auto processBandSample = [this, &coefficients] (int bandIndex, int channel, float input)
        {
            const auto& coefficient = coefficients[(size_t) bandIndex];
            auto& state = stateFor (bandIndex, channel);
            const float y = coefficient.b0 * input + state.z1;
            state.z1 = coefficient.b1 * input - coefficient.a1 * y + state.z2;
            state.z2 = coefficient.b2 * input - coefficient.a2 * y;
            return y;
        };

        if (numChannels == 1)
        {
            auto* samples = buffer.getWritePointer (0, startSample);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float dry = samples[sample];
                float wet = dry;

                for (int bandIndex = 0; bandIndex < kMaxBands; ++bandIndex)
                {
                    if (! activeBands[(size_t) bandIndex])
                        continue;

                    const int mode = bandModes[(size_t) bandIndex];
                    if (mode == 2 || mode == 4)
                        continue;

                    wet = processBandSample (bandIndex, 0, wet);
                }

                samples[sample] = (dry + (wet - dry) * wetMix) * trim;
            }
            return;
        }

        auto* left = buffer.getWritePointer (0, startSample);
        auto* right = buffer.getWritePointer (1, startSample);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dryL = left[sample];
            const float dryR = right[sample];
            float wetL = dryL;
            float wetR = dryR;

            for (int bandIndex = 0; bandIndex < kMaxBands; ++bandIndex)
            {
                if (! activeBands[(size_t) bandIndex])
                    continue;

                switch (bandModes[(size_t) bandIndex])
                {
                    case 1:
                        wetL = processBandSample (bandIndex, 0, wetL);
                        break;
                    case 2:
                        wetR = processBandSample (bandIndex, 1, wetR);
                        break;
                    case 3:
                    {
                        float mid = (wetL + wetR) * 0.5f;
                        const float side = (wetL - wetR) * 0.5f;
                        mid = processBandSample (bandIndex, 0, mid);
                        wetL = mid + side;
                        wetR = mid - side;
                        break;
                    }
                    case 4:
                    {
                        const float mid = (wetL + wetR) * 0.5f;
                        float side = (wetL - wetR) * 0.5f;
                        side = processBandSample (bandIndex, 1, side);
                        wetL = mid + side;
                        wetR = mid - side;
                        break;
                    }
                    default:
                        wetL = processBandSample (bandIndex, 0, wetL);
                        wetR = processBandSample (bandIndex, 1, wetR);
                        break;
                }
            }

            left[sample] = (dryL + (wetL - dryL) * wetMix) * trim;
            right[sample] = (dryR + (wetR - dryR) * wetMix) * trim;
        }

        for (int channel = 2; channel < numChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel, startSample);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float dry = samples[sample];
                float wet = dry;

                for (int bandIndex = 0; bandIndex < kMaxBands; ++bandIndex)
                {
                    if (! activeBands[(size_t) bandIndex] || bandModes[(size_t) bandIndex] != 0)
                        continue;

                    wet = processBandSample (bandIndex, channel, wet);
                }

                samples[sample] = (dry + (wet - dry) * wetMix) * trim;
            }
        }
    }
}
