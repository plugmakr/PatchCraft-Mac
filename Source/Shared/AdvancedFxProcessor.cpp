#include "AdvancedFxProcessor.h"
#include "RenderContext.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        static float coefficientForMs (float milliseconds, double sampleRate)
        {
            const auto seconds = juce::jlimit (0.0001f, 2.0f, milliseconds * 0.001f);
            return (float) std::exp (-1.0 / (RenderContext::sanitiseSampleRate (sampleRate) * (double) seconds));
        }

        static float softLimit (float x)
        {
            return juce::jlimit (-4.0f, 4.0f, x) / (1.0f + std::abs (x) * 0.15f);
        }

        static float nextSignedNoise (std::uint32_t& state)
        {
            state = state * 1664525u + 1013904223u;
            return ((float) ((state >> 8) & 0x00ffffffu) / 8388607.5f) - 1.0f;
        }

        static float onePoleAlpha (float frequency, double sampleRate)
        {
            const auto sr = RenderContext::sanitiseSampleRate (sampleRate);
            return juce::jlimit (0.001f, 0.99f,
                                 1.0f - (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                                          * (double) juce::jlimit (20.0f, 20000.0f, frequency) / sr));
        }
    }

    void AdvancedFxProcessor::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = RenderContext::sanitiseSampleRate (sr);
        blockSize = juce::jmax (1, maxBlockSize);
        channels = juce::jmax (1, numChannels);
        maxCombDelaySamples = juce::jmax (64, (int) sampleRate);
        maxMultiTapDelaySamples = juce::jmax (512, (int) (sampleRate * 2.0));

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) channels };
        chorus.prepare (spec);
        phaser.prepare (spec);
        resonator.prepare (spec);
        resonator.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        vocalFormantA.prepare (spec);
        vocalFormantB.prepare (spec);
        vocalFormantA.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        vocalFormantB.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

        scratchBuffer.setSize (channels, blockSize, false, false, true);
        formantScratchBuffer.setSize (channels, blockSize, false, false, true);
        dynamicsEnvelope.assign ((size_t) channels, 0.0f);
        spectralLowState.assign ((size_t) channels, 0.0f);
        tapeToneState.assign ((size_t) channels, 0.0f);
        vinylToneState.assign ((size_t) channels, 0.0f);
        lofiHeld.assign ((size_t) channels, 0.0f);
        lofiCounters.assign ((size_t) channels, 0);
        combLines.assign ((size_t) channels, std::vector<float> ((size_t) maxCombDelaySamples, 0.0f));
        combWriteIndices.assign ((size_t) channels, 0);
        multiTapLines.assign ((size_t) channels, std::vector<float> ((size_t) maxMultiTapDelaySamples, 0.0f));
        multiTapWriteIndices.assign ((size_t) channels, 0);
        convolutionState.assign ((size_t) channels, {});
        convolutionWriteIndices.assign ((size_t) channels, 0);
        modulationPhase = 0.0;
    }

    void AdvancedFxProcessor::reset()
    {
        chorus.reset();
        phaser.reset();
        resonator.reset();
        vocalFormantA.reset();
        vocalFormantB.reset();
        std::fill (dynamicsEnvelope.begin(), dynamicsEnvelope.end(), 0.0f);
        std::fill (spectralLowState.begin(), spectralLowState.end(), 0.0f);
        std::fill (tapeToneState.begin(), tapeToneState.end(), 0.0f);
        std::fill (vinylToneState.begin(), vinylToneState.end(), 0.0f);
        std::fill (lofiHeld.begin(), lofiHeld.end(), 0.0f);
        std::fill (lofiCounters.begin(), lofiCounters.end(), 0);
        for (auto& line : combLines)
            std::fill (line.begin(), line.end(), 0.0f);
        std::fill (combWriteIndices.begin(), combWriteIndices.end(), 0);
        for (auto& line : multiTapLines)
            std::fill (line.begin(), line.end(), 0.0f);
        std::fill (multiTapWriteIndices.begin(), multiTapWriteIndices.end(), 0);
        for (auto& state : convolutionState)
            state.fill (0.0f);
        std::fill (convolutionWriteIndices.begin(), convolutionWriteIndices.end(), 0);
        modulationPhase = 0.0;
    }

    bool AdvancedFxProcessor::setParameter (const juce::String& id, float value)
    {
        if      (id == "dynThresholdDb") atomics.dynamicsThresholdDb = value;
        else if (id == "dynRatio")       atomics.dynamicsRatio       = value;
        else if (id == "dynAttackMs")    atomics.dynamicsAttackMs    = value;
        else if (id == "dynReleaseMs")   atomics.dynamicsReleaseMs   = value;
        else if (id == "dynMakeupDb")    atomics.dynamicsMakeupDb    = value;
        else if (id == "dynMix")         atomics.dynamicsMix         = value;
        else if (id == "chorusRate")     atomics.chorusRate          = value;
        else if (id == "chorusDepth")    atomics.chorusDepth         = value;
        else if (id == "chorusFeedback") atomics.chorusFeedback      = value;
        else if (id == "chorusMix")      atomics.chorusMix           = value;
        else if (id == "phaserRate")     atomics.phaserRate          = value;
        else if (id == "phaserDepth")    atomics.phaserDepth         = value;
        else if (id == "phaserFeedback") atomics.phaserFeedback      = value;
        else if (id == "phaserMix")      atomics.phaserMix           = value;
        else if (id == "combFreq")       atomics.combFreq            = value;
        else if (id == "combFeedback")   atomics.combFeedback        = value;
        else if (id == "combMix")        atomics.combMix             = value;
        else if (id == "resonatorFreq")  atomics.resonatorFreq       = value;
        else if (id == "resonatorQ")     atomics.resonatorQ          = value;
        else if (id == "resonatorMix")   atomics.resonatorMix        = value;
        else if (id == "convolutionSize") atomics.convolutionSize    = value;
        else if (id == "convolutionMix") atomics.convolutionMix      = value;
        else if (id == "spectralTilt")   atomics.spectralTilt        = value;
        else if (id == "spectralMix")    atomics.spectralMix         = value;
        else if (id == "tapeDrive")      atomics.tapeDrive           = value;
        else if (id == "tapeTone")       atomics.tapeTone            = value;
        else if (id == "tapeFlutter")    atomics.tapeFlutter         = value;
        else if (id == "tapeMix")        atomics.tapeMix             = value;
        else if (id == "vinylAge")       atomics.vinylAge            = value;
        else if (id == "vinylDust")      atomics.vinylDust           = value;
        else if (id == "vinylWarp")      atomics.vinylWarp           = value;
        else if (id == "vinylMix")       atomics.vinylMix            = value;
        else if (id == "lofiBits")       atomics.lofiBits            = value;
        else if (id == "lofiRate")       atomics.lofiRate            = value;
        else if (id == "lofiMix")        atomics.lofiMix             = value;
        else if (id == "vocalFormant")   atomics.vocalFormant        = value;
        else if (id == "vocalBody")      atomics.vocalBody           = value;
        else if (id == "vocalMix")       atomics.vocalMix            = value;
        else if (id == "multiTapTime")   atomics.multiTapTime        = value;
        else if (id == "multiTapFeedback") atomics.multiTapFeedback  = value;
        else if (id == "multiTapSpread") atomics.multiTapSpread      = value;
        else if (id == "multiTapMix")    atomics.multiTapMix         = value;
        else return false;

        return true;
    }

    void AdvancedFxProcessor::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        const int numChans = buffer.getNumChannels();
        if (scratchBuffer.getNumChannels() < numChans || scratchBuffer.getNumSamples() < numSamples)
            scratchBuffer.setSize (numChans, juce::jmax (numSamples, blockSize), false, false, true);
        if (formantScratchBuffer.getNumChannels() < numChans || formantScratchBuffer.getNumSamples() < numSamples)
            formantScratchBuffer.setSize (numChans, juce::jmax (numSamples, blockSize), false, false, true);
        if ((int) dynamicsEnvelope.size() < numChans)
        {
            dynamicsEnvelope.resize ((size_t) numChans, 0.0f);
            spectralLowState.resize ((size_t) numChans, 0.0f);
            tapeToneState.resize ((size_t) numChans, 0.0f);
            vinylToneState.resize ((size_t) numChans, 0.0f);
            lofiHeld.resize ((size_t) numChans, 0.0f);
            lofiCounters.resize ((size_t) numChans, 0);
            combLines.resize ((size_t) numChans);
            combWriteIndices.resize ((size_t) numChans, 0);
            multiTapLines.resize ((size_t) numChans);
            multiTapWriteIndices.resize ((size_t) numChans, 0);
            convolutionState.resize ((size_t) numChans);
            convolutionWriteIndices.resize ((size_t) numChans, 0);
            for (auto& line : combLines)
                if ((int) line.size() < maxCombDelaySamples)
                    line.assign ((size_t) maxCombDelaySamples, 0.0f);
            for (auto& line : multiTapLines)
                if ((int) line.size() < maxMultiTapDelaySamples)
                    line.assign ((size_t) maxMultiTapDelaySamples, 0.0f);
        }

        const float dynMix = juce::jlimit (0.0f, 1.0f, atomics.dynamicsMix.load());
        if (dynMix > 0.0001f)
        {
            const float thresholdDb = juce::jlimit (-80.0f, 12.0f, atomics.dynamicsThresholdDb.load());
            const float ratio = juce::jlimit (1.0f, 40.0f, atomics.dynamicsRatio.load());
            const float attack = coefficientForMs (atomics.dynamicsAttackMs.load(), sampleRate);
            const float release = coefficientForMs (atomics.dynamicsReleaseMs.load(), sampleRate);
            const float makeup = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 24.0f, atomics.dynamicsMakeupDb.load()));

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                auto& env = dynamicsEnvelope[(size_t) ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    const float detector = std::abs (input);
                    const float coeff = detector > env ? attack : release;
                    env = detector + (env - detector) * coeff;
                    const float db = juce::Decibels::gainToDecibels (juce::jmax (env, 0.000001f), -120.0f);
                    const float overDb = juce::jmax (0.0f, db - thresholdDb);
                    const float gainReductionDb = -(overDb - overDb / ratio);
                    const float wet = input * juce::Decibels::decibelsToGain (gainReductionDb) * makeup;
                    data[i] = input * (1.0f - dynMix) + wet * dynMix;
                }
            }
        }

        const float tapeMix = juce::jlimit (0.0f, 1.0f, atomics.tapeMix.load());
        if (tapeMix > 0.0001f)
        {
            const float drive = 1.0f + juce::jlimit (0.0f, 1.0f, atomics.tapeDrive.load()) * 7.0f;
            const float tone = juce::jlimit (0.0f, 1.0f, atomics.tapeTone.load());
            const float alpha = onePoleAlpha (900.0f + tone * 15000.0f, sampleRate);
            const float flutterDepth = juce::jlimit (0.0f, 1.0f, atomics.tapeFlutter.load()) * 0.018f;
            const double phaseDelta = 2.0 * juce::MathConstants<double>::pi * 0.43 / sampleRate;
            const float norm = juce::jmax (0.2f, std::tanh (drive));

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                auto& toneState = tapeToneState[(size_t) ch];
                double localPhase = modulationPhase + (double) ch * 0.37;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    const float flutter = 1.0f + flutterDepth * ((float) std::sin (localPhase)
                                                                 + 0.45f * (float) std::sin (localPhase * 2.31 + 1.7));
                    const float saturated = std::tanh (input * drive * flutter) / norm;
                    toneState += alpha * (saturated - toneState);
                    const float wet = toneState + (saturated - toneState) * (0.30f + tone * 0.70f);
                    data[i] = input * (1.0f - tapeMix) + wet * tapeMix;
                    localPhase += phaseDelta;
                    if (localPhase > juce::MathConstants<double>::twoPi)
                        localPhase -= juce::MathConstants<double>::twoPi;
                }
            }
            modulationPhase += phaseDelta * (double) numSamples;
            while (modulationPhase > juce::MathConstants<double>::twoPi)
                modulationPhase -= juce::MathConstants<double>::twoPi;
        }

        const float vinylMix = juce::jlimit (0.0f, 1.0f, atomics.vinylMix.load());
        if (vinylMix > 0.0001f)
        {
            const float age = juce::jlimit (0.0f, 1.0f, atomics.vinylAge.load());
            const float dust = juce::jlimit (0.0f, 1.0f, atomics.vinylDust.load());
            const float warp = juce::jlimit (0.0f, 1.0f, atomics.vinylWarp.load());
            const float alpha = onePoleAlpha (18000.0f - age * 14500.0f, sampleRate);
            const double phaseDelta = 2.0 * juce::MathConstants<double>::pi * (0.18 + warp * 0.55) / sampleRate;

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                auto& toneState = vinylToneState[(size_t) ch];
                double localPhase = modulationPhase + (double) ch * 0.19;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    toneState += alpha * (input - toneState);
                    float crackle = nextSignedNoise (randomState) * dust * 0.006f;
                    const float impulseGate = (nextSignedNoise (randomState) + 1.0f) * 0.5f;
                    if (impulseGate > 1.0f - dust * dust * 0.020f)
                        crackle += nextSignedNoise (randomState) * dust * 0.12f;
                    const float wobble = 1.0f + warp * 0.045f * (float) std::sin (localPhase);
                    const float wet = softLimit (toneState * wobble * (1.0f - age * 0.10f) + crackle);
                    data[i] = input * (1.0f - vinylMix) + wet * vinylMix;
                    localPhase += phaseDelta;
                    if (localPhase > juce::MathConstants<double>::twoPi)
                        localPhase -= juce::MathConstants<double>::twoPi;
                }
            }
            modulationPhase += phaseDelta * (double) numSamples;
            while (modulationPhase > juce::MathConstants<double>::twoPi)
                modulationPhase -= juce::MathConstants<double>::twoPi;
        }

        const float lofiMix = juce::jlimit (0.0f, 1.0f, atomics.lofiMix.load());
        if (lofiMix > 0.0001f)
        {
            const float bits = juce::jlimit (4.0f, 16.0f, atomics.lofiBits.load());
            const int holdSamples = juce::jlimit (1, 128, 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, atomics.lofiRate.load()) * 96.0f));
            const float levels = std::pow (2.0f, bits - 1.0f);
            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                auto& held = lofiHeld[(size_t) ch];
                auto& counter = lofiCounters[(size_t) ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    if (counter <= 0)
                    {
                        held = std::round (juce::jlimit (-1.5f, 1.5f, input) * levels) / levels;
                        counter = holdSamples;
                    }
                    --counter;
                    data[i] = input * (1.0f - lofiMix) + held * lofiMix;
                }
            }
        }

        const float chorusMix = juce::jlimit (0.0f, 1.0f, atomics.chorusMix.load());
        if (chorusMix > 0.0001f)
        {
            chorus.setRate (juce::jlimit (0.01f, 20.0f, atomics.chorusRate.load()));
            chorus.setDepth (juce::jlimit (0.0f, 1.0f, atomics.chorusDepth.load()));
            chorus.setFeedback (juce::jlimit (-0.95f, 0.95f, atomics.chorusFeedback.load()));
            chorus.setMix (chorusMix);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChans,
                                                (size_t) startSample, (size_t) numSamples);
            chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        const float phaserMix = juce::jlimit (0.0f, 1.0f, atomics.phaserMix.load());
        if (phaserMix > 0.0001f)
        {
            phaser.setRate (juce::jlimit (0.01f, 20.0f, atomics.phaserRate.load()));
            phaser.setDepth (juce::jlimit (0.0f, 1.0f, atomics.phaserDepth.load()));
            phaser.setFeedback (juce::jlimit (-0.95f, 0.95f, atomics.phaserFeedback.load()));
            phaser.setMix (phaserMix);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChans,
                                                (size_t) startSample, (size_t) numSamples);
            phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        const float multiTapMix = juce::jlimit (0.0f, 1.0f, atomics.multiTapMix.load());
        if (multiTapMix > 0.0001f)
        {
            const int baseDelay = juce::jlimit (1, maxMultiTapDelaySamples - 1,
                juce::roundToInt (juce::jlimit (0.02f, 2.0f, atomics.multiTapTime.load()) * (float) sampleRate));
            const float feedback = juce::jlimit (0.0f, 0.92f, atomics.multiTapFeedback.load());
            const float spread = juce::jlimit (0.0f, 1.0f, atomics.multiTapSpread.load());
            const int tapA = juce::jlimit (1, maxMultiTapDelaySamples - 1, baseDelay);
            const int tapB = juce::jlimit (1, maxMultiTapDelaySamples - 1, juce::roundToInt ((float) baseDelay * (0.50f + spread * 0.45f)));
            const int tapC = juce::jlimit (1, maxMultiTapDelaySamples - 1, juce::roundToInt ((float) baseDelay * (1.25f + spread * 0.70f)));

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto& line = multiTapLines[(size_t) ch];
                auto& writeIndex = multiTapWriteIndices[(size_t) ch];
                auto* data = buffer.getWritePointer (ch, startSample);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    const int readA = (writeIndex - tapA + maxMultiTapDelaySamples) % maxMultiTapDelaySamples;
                    const int readB = (writeIndex - tapB + maxMultiTapDelaySamples) % maxMultiTapDelaySamples;
                    const int readC = (writeIndex - tapC + maxMultiTapDelaySamples) % maxMultiTapDelaySamples;
                    const float wet = line[(size_t) readA] * 0.55f
                                    + line[(size_t) readB] * (0.35f + spread * 0.20f)
                                    + line[(size_t) readC] * 0.25f;
                    line[(size_t) writeIndex] = softLimit (input + wet * feedback);
                    writeIndex = (writeIndex + 1) % maxMultiTapDelaySamples;
                    data[i] = input * (1.0f - multiTapMix) + softLimit (wet) * multiTapMix;
                }
            }
        }

        const float combMix = juce::jlimit (0.0f, 1.0f, atomics.combMix.load());
        if (combMix > 0.0001f)
        {
            const int delaySamples = juce::jlimit (1, maxCombDelaySamples - 1,
                (int) (sampleRate / juce::jlimit (20.0f, 8000.0f, atomics.combFreq.load())));
            const float feedback = juce::jlimit (-0.95f, 0.95f, atomics.combFeedback.load());
            for (int ch = 0; ch < numChans; ++ch)
            {
                auto& line = combLines[(size_t) ch];
                auto& writeIndex = combWriteIndices[(size_t) ch];
                auto* data = buffer.getWritePointer (ch, startSample);
                for (int i = 0; i < numSamples; ++i)
                {
                    const int readIndex = (writeIndex - delaySamples + maxCombDelaySamples) % maxCombDelaySamples;
                    const float delayed = line[(size_t) readIndex];
                    const float wet = softLimit (data[i] + delayed * feedback);
                    line[(size_t) writeIndex] = data[i] + wet * feedback;
                    writeIndex = (writeIndex + 1) % maxCombDelaySamples;
                    data[i] = data[i] * (1.0f - combMix) + wet * combMix;
                }
            }
        }

        const float resonatorMix = juce::jlimit (0.0f, 1.0f, atomics.resonatorMix.load());
        if (resonatorMix > 0.0001f)
        {
            for (int ch = 0; ch < numChans; ++ch)
                scratchBuffer.copyFrom (ch, 0, buffer, ch, startSample, numSamples);

            resonator.setCutoffFrequency (juce::jlimit (20.0f, 16000.0f, atomics.resonatorFreq.load()));
            resonator.setResonance (juce::jlimit (0.05f, 18.0f, atomics.resonatorQ.load()));
            juce::dsp::AudioBlock<float> block (scratchBuffer.getArrayOfWritePointers(), (size_t) numChans, 0, (size_t) numSamples);
            resonator.process (juce::dsp::ProcessContextReplacing<float> (block));

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                const auto* wet = scratchBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = data[i] * (1.0f - resonatorMix) + softLimit (wet[i] * 2.0f) * resonatorMix;
            }
        }

        const float convolutionMix = juce::jlimit (0.0f, 1.0f, atomics.convolutionMix.load());
        if (convolutionMix > 0.0001f)
        {
            static constexpr std::array<float, 8> taps { 0.62f, -0.20f, 0.14f, 0.10f, -0.08f, 0.055f, 0.035f, -0.025f };
            const int size = juce::jlimit (1, 8, juce::roundToInt (atomics.convolutionSize.load()));
            for (int ch = 0; ch < numChans; ++ch)
            {
                auto& state = convolutionState[(size_t) ch];
                auto& writeIndex = convolutionWriteIndices[(size_t) ch];
                auto* data = buffer.getWritePointer (ch, startSample);
                for (int i = 0; i < numSamples; ++i)
                {
                    state[(size_t) writeIndex] = data[i];
                    float wet = 0.0f;
                    for (int tap = 0; tap < size; ++tap)
                    {
                        const int readIndex = (writeIndex - tap + 8) % 8;
                        wet += state[(size_t) readIndex] * taps[(size_t) tap];
                    }
                    writeIndex = (writeIndex + 1) % 8;
                    data[i] = data[i] * (1.0f - convolutionMix) + wet * convolutionMix;
                }
            }
        }

        const float spectralMix = juce::jlimit (0.0f, 1.0f, atomics.spectralMix.load());
        if (spectralMix > 0.0001f)
        {
            const float tilt = juce::jlimit (-1.0f, 1.0f, atomics.spectralTilt.load());
            const float alpha = juce::jlimit (0.001f, 0.5f, 1200.0f / (float) sampleRate);
            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                auto& low = spectralLowState[(size_t) ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    low += alpha * (data[i] - low);
                    const float high = data[i] - low;
                    const float tilted = tilt >= 0.0f
                        ? data[i] + high * tilt - low * tilt * 0.25f
                        : data[i] + low * (-tilt) - high * (-tilt) * 0.25f;
                    data[i] = data[i] * (1.0f - spectralMix) + softLimit (tilted) * spectralMix;
                }
            }
        }

        const float vocalMix = juce::jlimit (0.0f, 1.0f, atomics.vocalMix.load());
        if (vocalMix > 0.0001f)
        {
            for (int ch = 0; ch < numChans; ++ch)
            {
                scratchBuffer.copyFrom (ch, 0, buffer, ch, startSample, numSamples);
                formantScratchBuffer.copyFrom (ch, 0, buffer, ch, startSample, numSamples);
            }

            const float formant = juce::jlimit (0.0f, 1.0f, atomics.vocalFormant.load());
            const float body = juce::jlimit (0.0f, 1.0f, atomics.vocalBody.load());
            vocalFormantA.setCutoffFrequency (juce::jlimit (180.0f, 2600.0f, 420.0f + formant * 1850.0f));
            vocalFormantB.setCutoffFrequency (juce::jlimit (600.0f, 6000.0f, 1100.0f + formant * 4100.0f));
            vocalFormantA.setResonance (juce::jlimit (0.20f, 10.0f, 2.0f + body * 7.0f));
            vocalFormantB.setResonance (juce::jlimit (0.20f, 10.0f, 1.4f + body * 5.0f));

            juce::dsp::AudioBlock<float> blockA (scratchBuffer.getArrayOfWritePointers(), (size_t) numChans, 0, (size_t) numSamples);
            juce::dsp::AudioBlock<float> blockB (formantScratchBuffer.getArrayOfWritePointers(), (size_t) numChans, 0, (size_t) numSamples);
            vocalFormantA.process (juce::dsp::ProcessContextReplacing<float> (blockA));
            vocalFormantB.process (juce::dsp::ProcessContextReplacing<float> (blockB));

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* data = buffer.getWritePointer (ch, startSample);
                const auto* vowelA = scratchBuffer.getReadPointer (ch);
                const auto* vowelB = formantScratchBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float input = data[i];
                    const float wet = softLimit ((vowelA[i] * (0.95f + body) + vowelB[i] * (0.65f + formant * 0.35f)) * 1.8f);
                    data[i] = input * (1.0f - vocalMix) + wet * vocalMix;
                }
            }
        }
    }
}
