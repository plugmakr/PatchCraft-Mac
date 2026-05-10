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
    }

    void AdvancedFxProcessor::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = RenderContext::sanitiseSampleRate (sr);
        blockSize = juce::jmax (1, maxBlockSize);
        channels = juce::jmax (1, numChannels);
        maxCombDelaySamples = juce::jmax (64, (int) sampleRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) channels };
        chorus.prepare (spec);
        phaser.prepare (spec);
        resonator.prepare (spec);
        resonator.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

        scratchBuffer.setSize (channels, blockSize, false, false, true);
        dynamicsEnvelope.assign ((size_t) channels, 0.0f);
        spectralLowState.assign ((size_t) channels, 0.0f);
        combLines.assign ((size_t) channels, std::vector<float> ((size_t) maxCombDelaySamples, 0.0f));
        combWriteIndices.assign ((size_t) channels, 0);
        convolutionState.assign ((size_t) channels, {});
        convolutionWriteIndices.assign ((size_t) channels, 0);
    }

    void AdvancedFxProcessor::reset()
    {
        chorus.reset();
        phaser.reset();
        resonator.reset();
        std::fill (dynamicsEnvelope.begin(), dynamicsEnvelope.end(), 0.0f);
        std::fill (spectralLowState.begin(), spectralLowState.end(), 0.0f);
        for (auto& line : combLines)
            std::fill (line.begin(), line.end(), 0.0f);
        std::fill (combWriteIndices.begin(), combWriteIndices.end(), 0);
        for (auto& state : convolutionState)
            state.fill (0.0f);
        std::fill (convolutionWriteIndices.begin(), convolutionWriteIndices.end(), 0);
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
        if ((int) dynamicsEnvelope.size() < numChans)
        {
            dynamicsEnvelope.resize ((size_t) numChans, 0.0f);
            spectralLowState.resize ((size_t) numChans, 0.0f);
            combLines.resize ((size_t) numChans);
            combWriteIndices.resize ((size_t) numChans, 0);
            convolutionState.resize ((size_t) numChans);
            convolutionWriteIndices.resize ((size_t) numChans, 0);
            for (auto& line : combLines)
                if ((int) line.size() < maxCombDelaySamples)
                    line.assign ((size_t) maxCombDelaySamples, 0.0f);
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
    }
}
