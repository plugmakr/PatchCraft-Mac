#include "EffectEngine.h"

namespace patchcraft
{
    EffectEngine::EffectEngine() = default;

    void EffectEngine::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = sr;
        blockSize  = maxBlockSize;
        preparedChannels = juce::jmax (1, numChannels);
        preparedMaxSamples = maxBlockSize;

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize,
                                      (juce::uint32) preparedChannels };
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        eq.prepare (sr, maxBlockSize, numChannels);
        advancedFx.prepare (sr, maxBlockSize, numChannels);

        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) (sr * 2.0));

        reverb.prepare (spec);

        dryBuffer.setSize (preparedChannels, maxBlockSize, false, false, true);
        delayScratch.resize ((size_t) preparedChannels, 0.0f);
    }

    void EffectEngine::setRenderContext (const RenderContext& context)
    {
        renderContext = context;
        sampleRate = RenderContext::sanitiseSampleRate (context.sampleRate);
    }

    void EffectEngine::reset()
    {
        filter.reset();
        eq.reset();
        advancedFx.reset();
        delay.reset();
        reverb.reset();
    }

    void EffectEngine::setParameter (const juce::String& id, float v)
    {
        if      (id == "drive")           atomics.drive          = v;
        else if (id == "mix")             atomics.mix            = v;
        else if (id == "filterCutoff")    atomics.cutoff         = v;
        else if (id == "filterResonance") atomics.resonance      = v;
        else if (id == "delayTime")       atomics.delayTime      = v;
        else if (id == "delayFeedback")   atomics.delayFeedback  = v;
        else if (id == "delayMix")        atomics.delayMix       = v;
        else if (id == "reverbMix")       atomics.reverbMix      = v;
        else if (id == "volume")          atomics.volume         = v;
        else if (id == "expression")      atomics.expression     = v;
        else if (id == "pan")             atomics.pan            = v;
        else if (id.startsWithIgnoreCase ("eq")) eq.setParameter (id, v);
        else if (advancedFx.setParameter (id, v)) {}
        else utility.setParameter (id, v);
    }

    static inline float softClip (float x)
    {
        // Tanh-like soft clip without the cost of std::tanh
        return x / (1.0f + std::abs (x));
    }

    void EffectEngine::process (juce::AudioBuffer<float>& buffer,
                                int startSample, int numSamples)
    {
        if (numSamples <= 0) return;
        const int numChans = buffer.getNumChannels();
        if (numChans <= 0) return;

        // Defensive: host should never exceed prepared bounds, but guard against it.
        if (numChans > preparedChannels || numSamples > preparedMaxSamples)
            return;

        // Copy input -> dryBuffer for wet/dry blend at the end.
        utility.processInput (buffer, startSample, numSamples);
        for (int ch = 0; ch < numChans; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, startSample, numSamples);

        // ---- Drive (pre-gain + soft clip) ----------------------------------
        const float drive = juce::jlimit (0.0f, 1.0f, atomics.drive.load());
        const float driveGain = juce::Decibels::decibelsToGain (drive * 24.0f);
        for (int ch = 0; ch < numChans; ++ch)
        {
            auto* d = buffer.getWritePointer (ch, startSample);
            for (int i = 0; i < numSamples; ++i)
                d[i] = softClip (d[i] * driveGain);
        }

        // ---- Filter --------------------------------------------------------
        filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, atomics.cutoff.load()));
        filter.setResonance      (juce::jlimit (0.05f, 1.5f, atomics.resonance.load() * 1.5f + 0.05f));
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            (size_t) numChans,
                                            (size_t) startSample,
                                            (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
        eq.process (buffer, startSample, numSamples);
        advancedFx.process (buffer, startSample, numSamples);

        // ---- Delay ---------------------------------------------------------
        const float dTime = juce::jlimit (0.0f, 2.0f, atomics.delayTime.load());
        const float dFb   = juce::jlimit (0.0f, 0.95f, atomics.delayFeedback.load());
        const float dMix  = juce::jlimit (0.0f, 1.0f, atomics.delayMix.load());
        const int dSamps  = juce::jmax (1, (int) (dTime * sampleRate));
        delay.setDelay ((float) dSamps);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChans; ++ch)
                delayScratch[(size_t) ch] = delay.popSample (ch);

            for (int ch = 0; ch < numChans; ++ch)
            {
                auto* channel = buffer.getWritePointer (ch, startSample);
                const int feedbackChannel = numChans == 1 ? 0
                    : ((ch % 2 == 0 && ch + 1 < numChans) ? ch + 1 : ch - 1);
                delay.pushSample (ch, channel[i] + delayScratch[(size_t) feedbackChannel] * dFb);
                channel[i] = channel[i] * (1.0f - dMix * 0.5f) + delayScratch[(size_t) ch] * dMix;
            }
        }

        // ---- Reverb --------------------------------------------------------
        const float rvMix = juce::jlimit (0.0f, 1.0f, atomics.reverbMix.load());
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.6f; rp.damping = 0.4f;
        rp.wetLevel = rvMix * 0.6f;
        rp.dryLevel = 1.0f - rvMix * 0.5f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);

        // ---- Master vol/pan + wet/dry blend --------------------------------
        const float vol = juce::jlimit (0.0f, 2.0f, atomics.volume.load())
                        * juce::jlimit (0.0f, 1.0f, atomics.expression.load());
        const float pan = juce::jlimit (-1.0f, 1.0f, atomics.pan.load());
        const float lG = vol * std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        const float rG = vol * std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        const float mix = juce::jlimit (0.0f, 1.0f, atomics.mix.load());

        for (int ch = 0; ch < numChans; ++ch)
        {
            auto* dry = dryBuffer.getReadPointer (ch);
            auto* wet = buffer.getWritePointer (ch, startSample);
            const float chGain = numChans == 1 ? vol : (ch == 0 ? lG : ch == 1 ? rG : vol);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = (wet[i] * mix * chGain) + (dry[i] * (1.0f - mix));
        }
        utility.processOutput (buffer, startSample, numSamples);
    }

} // namespace patchcraft
