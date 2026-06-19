#include "SynthEngine.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        static double wrapTwoPi (double phase)
        {
            const auto twoPi = juce::MathConstants<double>::twoPi;
            phase = std::fmod (phase, twoPi);
            return phase < 0.0 ? phase + twoPi : phase;
        }

        static float bipolarFold (float sample, float amount)
        {
            amount = juce::jlimit (0.0f, 1.0f, amount);
            if (amount <= 0.0001f)
                return sample;

            sample *= 1.0f + amount * 5.0f;
            for (int i = 0; i < 4; ++i)
            {
                if (sample > 1.0f)  sample = 2.0f - sample;
                if (sample < -1.0f) sample = -2.0f - sample;
            }
            return juce::jlimit (-1.0f, 1.0f, sample);
        }

        static float additiveSaw (double phase, int harmonics, float tilt)
        {
            float out = 0.0f;
            float norm = 0.0f;
            tilt = juce::jlimit (0.4f, 2.8f, tilt);
            for (int h = 1; h <= harmonics; ++h)
            {
                const float amp = 1.0f / std::pow ((float) h, tilt);
                out += std::sin ((float) phase * (float) h) * amp * ((h & 1) != 0 ? 1.0f : -1.0f);
                norm += amp;
            }
            return norm > 0.0f ? out / norm : 0.0f;
        }

        static float additiveSquare (double phase, int harmonics, float tilt)
        {
            float out = 0.0f;
            float norm = 0.0f;
            tilt = juce::jlimit (0.4f, 2.8f, tilt);
            for (int h = 1; h <= harmonics; h += 2)
            {
                const float amp = 1.0f / std::pow ((float) h, tilt);
                out += std::sin ((float) phase * (float) h) * amp;
                norm += amp;
            }
            return norm > 0.0f ? out / norm : 0.0f;
        }

        static float spectralTiltFor (float baseTilt, float spectralTilt)
        {
            return juce::jlimit (0.35f, 3.2f, baseTilt + juce::jlimit (-1.0f, 1.0f, spectralTilt) * 0.85f);
        }

        static float frameSample (int family, int frame, double phase, float position,
                                  float morph, int harmonics, float spectralTilt)
        {
            const float p = (float) (wrapTwoPi (phase) / juce::MathConstants<double>::twoPi);
            const float width = juce::jlimit (0.08f, 0.92f, 0.5f + (position - 0.5f) * 0.72f);

            switch (juce::jlimit (0, 7, family))
            {
                case 1:
                    if (frame == 0) return std::sin ((float) phase);
                    if (frame == 1) return additiveSaw (phase, harmonics, spectralTiltFor (0.70f, spectralTilt));
                    if (frame == 2) return std::sin ((float) phase + morph * 7.0f * std::sin ((float) phase * 2.0f));
                    return std::sin ((float) phase * 3.0f) * 0.55f + std::sin ((float) phase * 7.0f) * 0.25f;
                case 2:
                    if (frame == 0) return additiveSquare (phase, harmonics, spectralTiltFor (0.85f, spectralTilt));
                    if (frame == 1) return p < width ? 1.0f : -1.0f;
                    if (frame == 2) return std::sin ((float) phase) * std::sin ((float) phase * 3.0f + morph * 2.0f);
                    return std::sin ((float) phase * 5.0f + std::sin ((float) phase) * 2.5f);
                case 3:
                    if (frame == 0) return std::sin ((float) phase) * 0.75f + std::sin ((float) phase * 2.0f) * 0.25f;
                    if (frame == 1) return std::sin ((float) phase * 2.0f) * 0.6f + std::sin ((float) phase * 5.0f) * 0.35f;
                    if (frame == 2) return std::sin ((float) phase + std::sin ((float) phase * 4.0f) * 1.6f);
                    return additiveSaw (phase, juce::jmin (harmonics, 18), spectralTiltFor (1.4f, spectralTilt));
                case 4:
                    if (frame == 0) return std::sin ((float) phase + std::sin ((float) phase) * 2.0f);
                    if (frame == 1) return std::sin ((float) phase + std::sin ((float) phase * 2.0f) * 4.0f);
                    if (frame == 2) return std::sin ((float) phase * (2.0f + morph * 5.0f)) * 0.55f + additiveSaw (phase, harmonics, spectralTiltFor (1.1f, spectralTilt)) * 0.45f;
                    return bipolarFold (std::sin ((float) phase) + std::sin ((float) phase * 3.0f) * 0.45f, 0.55f);
                case 5:
                    if (frame == 0) return additiveSquare (phase, harmonics, spectralTiltFor (1.2f, spectralTilt));
                    if (frame == 1) return std::sin ((float) phase) + std::sin ((float) phase * 3.0f) * 0.33f + std::sin ((float) phase * 5.0f) * 0.2f;
                    if (frame == 2) return std::sin ((float) phase * 2.0f) * 0.5f + std::sin ((float) phase * 4.0f) * 0.25f + std::sin ((float) phase * 8.0f) * 0.125f;
                    return additiveSaw (phase, harmonics, spectralTiltFor (1.8f, spectralTilt));
                case 6:
                    if (frame == 0) return additiveSaw (phase, harmonics, spectralTiltFor (0.55f, spectralTilt));
                    if (frame == 1) return additiveSquare (phase, harmonics, spectralTiltFor (0.65f, spectralTilt));
                    if (frame == 2) return std::sin ((float) phase * 9.0f + std::sin ((float) phase) * 1.5f) * 0.65f;
                    return bipolarFold (additiveSaw (phase, harmonics, spectralTiltFor (0.85f, spectralTilt)), 0.75f);
                case 7:
                    if (frame == 0) return std::sin ((float) phase);
                    if (frame == 1) return additiveSaw (phase, harmonics, spectralTiltFor (1.9f, spectralTilt));
                    if (frame == 2) return additiveSquare (phase, harmonics, spectralTiltFor (1.7f, spectralTilt));
                    return std::sin ((float) phase + std::sin ((float) phase * 6.0f) * 2.2f);
                default:
                    if (frame == 0) return std::sin ((float) phase);
                    if (frame == 1) return additiveSaw (phase, harmonics, spectralTiltFor (1.0f, spectralTilt));
                    if (frame == 2) return additiveSquare (phase, harmonics, spectralTiltFor (1.0f, spectralTilt));
                    return 4.0f * std::abs (p - 0.5f) - 1.0f;
            }
        }

        static double warpWavetablePhase (double phase, float bend, int syncRatio)
        {
            float cycle = (float) (wrapTwoPi (phase) / juce::MathConstants<double>::twoPi);
            const int ratio = juce::jlimit (1, 8, syncRatio);
            if (ratio > 1)
                cycle = std::fmod (cycle * (float) ratio, 1.0f);

            bend = juce::jlimit (-1.0f, 1.0f, bend);
            if (std::abs (bend) > 0.0001f)
            {
                const float curve = 1.0f + std::abs (bend) * 4.0f;
                cycle = bend > 0.0f ? std::pow (cycle, curve)
                                     : 1.0f - std::pow (1.0f - cycle, curve);
            }

            return (double) juce::jlimit (0.0f, 1.0f, cycle) * juce::MathConstants<double>::twoPi;
        }
    }

    SynthEngine::SynthEngine()
    {
        for (int i = 0; i < kCustomWavetablePoints; ++i)
        {
            const float phase = (float) i / (float) kCustomWavetablePoints * juce::MathConstants<float>::twoPi;
            atomics.wtShape[(size_t) i] = std::sin (phase);
            for (int frame = 0; frame < kCustomWavetableFrames; ++frame)
            {
                const float framePhase = phase * (float) (frame + 1);
                atomics.wtFrameShape[(size_t) frame][(size_t) i] = std::sin (framePhase);
            }
        }

        // Initialize dynamic DspRack with default chain
        dspRack.addAudioModule (std::make_unique<FilterModule> ("filter1"));
        dspRack.addAudioModule (std::make_unique<DelayModule> ("delay1"));
        dspRack.addAudioModule (std::make_unique<ReverbModule> ("reverb1"));
        
        dspRack.addModulator (std::make_unique<LfoModulator> ("lfo1"));
        
        // Connect LFO1 to modulate Filter1's cutoff
        dspRack.addModulationRoute (ModulationRoute { "lfo1", "filter1", "cutoff", 4000.0f, true });
    }

    void SynthEngine::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = sr;
        blockSize  = maxBlockSize;
        preparedChannels = juce::jmax (1, numChannels);
        preparedMaxSamples = maxBlockSize;

        for (auto& v : voices) v.env.setSampleRate (sr);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize,
                                      (juce::uint32) juce::jmax (1, numChannels) };
        filter.prepare (spec);
        dspRack.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        eq.prepare (sr, maxBlockSize, numChannels);
        advancedFx.prepare (sr, maxBlockSize, numChannels);

        delayL.prepare (spec);
        delayR.prepare (spec);
        delayL.setMaximumDelayInSamples ((int) (sr * 2.0));
        delayR.setMaximumDelayInSamples ((int) (sr * 2.0));

        reverb.prepare (spec);

        tempBuffer.setSize (juce::jmax (2, numChannels), maxBlockSize, false, false, true);
    }

    void SynthEngine::setRenderContext (const RenderContext& context)
    {
        renderContext = context;
        sampleRate = RenderContext::sanitiseSampleRate (context.sampleRate);
    }

    void SynthEngine::reset()
    {
        for (auto& v : voices) { v.active = false; v.env.reset(); }
        filter.reset();
        eq.reset();
        advancedFx.reset();
        delayL.reset();
        delayR.reset();
        reverb.reset();
        dspRack.reset();
        lfoPhase = 0.0;
    }

    juce::ADSR::Parameters SynthEngine::currentAdsr() const
    {
        juce::ADSR::Parameters p;
        p.attack  = juce::jmax (0.001f, atomics.attack.load());
        p.decay   = juce::jmax (0.001f, atomics.decay.load());
        p.sustain = juce::jlimit (0.0f, 1.0f, atomics.sustain.load());
        p.release = juce::jmax (0.001f, atomics.release.load());
        return p;
    }

    double SynthEngine::frequencyForVoice (int note, float extraCents, float extraOctave) const
    {
        const float octave = atomics.octave.load();
        const float cents  = atomics.detuneCents.load();
        const double semis = (note - 69) + ((octave + extraOctave) * 12.0) + ((cents + extraCents) / 100.0);
        return 440.0 * std::pow (2.0, semis / 12.0);
    }

    SynthEngine::Voice* SynthEngine::findFreeVoice()
    {
        Voice* oldest = nullptr;
        for (auto& v : voices)
        {
            if (! v.active) return &v;
            if (oldest == nullptr || v.age < oldest->age)
                oldest = &v;
        }
        return oldest != nullptr ? oldest : &voices[0];
    }

    void SynthEngine::noteOn (int note, float velocity)
    {
        if (atomics.retrigger.load() < 0.5f)
        {
            for (auto& existing : voices)
            {
                if (existing.active && existing.note == note)
                {
                    existing.velocity = juce::jlimit (0.05f, 1.0f, velocity);
                    return;
                }
            }
        }

        if (auto* v = findFreeVoice())
        {
            v->active   = true;
            v->note     = note;
            v->velocity = juce::jlimit (0.05f, 1.0f, velocity);
            v->age      = voiceAgeCounter++;
            v->phase    = 0.0;
            v->phase2   = 0.0;
            v->subPhase = 0.0;
            const int wtPhaseMode = juce::jlimit (0, 2, juce::roundToInt (atomics.wtPhaseMode.load()));
            const double wtBasePhase = wtPhaseMode == 1 ? rng.nextDouble() * juce::MathConstants<double>::twoPi : 0.0;
            for (int i = 0; i < (int) v->wavetablePhase.size(); ++i)
            {
                const double spreadPhase = wtPhaseMode == 2
                    ? juce::MathConstants<double>::twoPi * ((double) i / (double) v->wavetablePhase.size())
                    : 0.0;
                v->wavetablePhase[(size_t) i] = wrapTwoPi (wtBasePhase + spreadPhase);
                v->wavetablePhaseInc[(size_t) i] = 0.0;
            }
            const double freq = frequencyForVoice (note);
            v->phaseInc = juce::MathConstants<double>::twoPi * freq / sampleRate;
            v->phaseInc2 = juce::MathConstants<double>::twoPi
                         * frequencyForVoice (note, atomics.osc2DetuneCents.load()) / sampleRate;
            v->subPhaseInc = juce::MathConstants<double>::twoPi
                           * frequencyForVoice (note, 0.0f, -1.0f) / sampleRate;
            v->env.setParameters (currentAdsr());
            v->env.reset();
            v->env.noteOn();
        }
    }

    void SynthEngine::noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note)
                v.env.noteOff();
    }

    void SynthEngine::allNotesOff()
    {
        for (auto& v : voices) if (v.active) v.env.noteOff();
    }

    void SynthEngine::setParameter (const juce::String& id, float v)
    {
        if      (id == "oscType")         atomics.oscType        = juce::jlimit (0.0f, 3.0f, v);
        else if (id == "osc2Type")        atomics.osc2Type       = juce::jlimit (0.0f, 3.0f, v);
        else if (id == "oscBlend")        atomics.oscBlend       = v;
        else if (id == "octave")          atomics.octave         = v;
        else if (id == "detune")          atomics.detuneCents    = v;
        else if (id == "osc2Detune")      atomics.osc2DetuneCents = v;
        else if (id == "subBlend")        atomics.subBlend       = v;
        else if (id == "noiseBlend")      atomics.noiseBlend     = v;
        else if (id == "wtEnabled")       atomics.wtEnabled      = v;
        else if (id == "wtTable")         atomics.wtTable        = v;
        else if (id == "wtPosition")      atomics.wtPosition     = v;
        else if (id == "wtMorph")         atomics.wtMorph        = v;
        else if (id == "wtWarp")          atomics.wtWarp         = v;
        else if (id == "wtFold")          atomics.wtFold         = v;
        else if (id == "wtUnison")        atomics.wtUnison       = v;
        else if (id == "wtDetune")        atomics.wtDetune       = v;
        else if (id == "wtSpread")        atomics.wtSpread       = v;
        else if (id == "wtLevel")         atomics.wtLevel        = v;
        else if (id == "wtBend")          atomics.wtBend         = v;
        else if (id == "wtSyncRatio")     atomics.wtSyncRatio    = v;
        else if (id == "wtSpectralTilt")  atomics.wtSpectralTilt = v;
        else if (id == "wtPhaseMode")     atomics.wtPhaseMode    = v;
        else if (id == "wtFramePosition") atomics.wtFramePosition = v;
        else if (id == "wtFrameCount")    atomics.wtFrameCount    = v;
        else if (id.startsWithIgnoreCase ("wtFrame") && id.containsIgnoreCase ("Shape"))
        {
            const auto afterFrame = id.fromFirstOccurrenceOf ("wtFrame", false, true);
            const int frame = afterFrame.upToFirstOccurrenceOf ("Shape", false, true).getIntValue();
            const int index = afterFrame.fromFirstOccurrenceOf ("Shape", false, true).getIntValue();
            if (frame >= 0 && frame < kCustomWavetableFrames && index >= 0 && index < kCustomWavetablePoints)
                atomics.wtFrameShape[(size_t) frame][(size_t) index] = juce::jlimit (-1.0f, 1.0f, v);
        }
        else if (id.startsWithIgnoreCase ("wtShape"))
        {
            const int index = id.fromFirstOccurrenceOf ("wtShape", false, true).getIntValue();
            if (index >= 0 && index < kCustomWavetablePoints)
            {
                atomics.wtShape[(size_t) index] = juce::jlimit (-1.0f, 1.0f, v);
                atomics.wtFrameShape[0][(size_t) index] = juce::jlimit (-1.0f, 1.0f, v);
            }
        }
        else if (id == "attack")          atomics.attack         = v;
        else if (id == "decay")           atomics.decay          = v;
        else if (id == "sustain")         atomics.sustain        = v;
        else if (id == "release")         atomics.release        = v;
        else if (id == "filterCutoff")
        {
            atomics.cutoff = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "filter") fx->setParameter ("cutoff", v);
        }
        else if (id == "filterResonance")
        {
            atomics.resonance = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "filter") fx->setParameter ("resonance", v);
        }
        else if (id == "lfoRate")
        {
            atomics.lfoRate = v;
            for (auto& m : dspRack.getModulators())
                if (m->getType() == "lfo") m->setParameter ("rate", v);
        }
        else if (id == "lfoAmount")
        {
            atomics.lfoAmount = v;
            for (auto& m : dspRack.getModulators())
                if (m->getType() == "lfo") m->setParameter ("amount", v);
        }
        else if (id == "delayTime")
        {
            atomics.delayTime = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "delay") fx->setParameter ("time", v);
        }
        else if (id == "delayFeedback")
        {
            atomics.delayFeedback = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "delay") fx->setParameter ("feedback", v);
        }
        else if (id == "delayMix")
        {
            atomics.delayMix = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "delay") fx->setParameter ("mix", v);
        }
        else if (id == "reverbMix")
        {
            atomics.reverbMix = v;
            for (auto& fx : dspRack.getAudioChain())
                if (fx->getType() == "reverb") fx->setParameter ("mix", v);
        }
        else if (id == "volume")          atomics.volume         = v;
        else if (id == "expression")      atomics.expression     = v;
        else if (id == "pan")             atomics.pan            = v;
        else if (id == "retrigger")       atomics.retrigger      = v;
        else if (id.startsWithIgnoreCase ("eq")) eq.setParameter (id, v);
        else if (advancedFx.setParameter (id, v)) {}
        else utility.setParameter (id, v);
    }

    int SynthEngine::getActiveVoiceCount() const noexcept
    {
        int c = 0;
        for (auto& v : voices) if (v.active) ++c;
        return c;
    }

    float SynthEngine::oscSample (int t, double phase, juce::Random& r) const
    {
        const float p = (float) (phase / juce::MathConstants<double>::twoPi);   // 0..1
        switch (t)
        {
            case 0: return std::sin ((float) phase);                // sine
            case 1: return 2.0f * p - 1.0f;                          // saw
            case 2: return p < 0.5f ? 1.0f : -1.0f;                  // square
            case 3: return 4.0f * std::abs (p - 0.5f) - 1.0f;        // triangle
            case 4: return r.nextFloat() * 2.0f - 1.0f;              // noise
        }
        return 0.0f;
    }

    float SynthEngine::wavetableSample (int table, double phase, float position, float morph,
                                        float warp, float fold, float bend, int syncRatio,
                                        float spectralTilt, double frequency) const
    {
        position = juce::jlimit (0.0f, 1.0f, position);
        morph = juce::jlimit (0.0f, 1.0f, morph);
        warp = juce::jlimit (0.0f, 1.0f, warp);
        fold = juce::jlimit (0.0f, 1.0f, fold);
        bend = juce::jlimit (-1.0f, 1.0f, bend);
        spectralTilt = juce::jlimit (-1.0f, 1.0f, spectralTilt);
        phase = warpWavetablePhase (phase, bend, syncRatio);

        const float warpedPosition = juce::jlimit (0.0f, 1.0f,
            position + (std::sin ((float) phase * 2.0f) * 0.5f + 0.5f - position) * warp * 0.65f);
        const float frame = warpedPosition * 3.0f;
        const int frameA = juce::jlimit (0, 3, (int) std::floor (frame));
        const int frameB = juce::jlimit (0, 3, frameA + 1);
        const float frameBlend = frame - (float) frameA;
        const int maxHarmonic = juce::jlimit (1, 48, (int) std::floor ((sampleRate * 0.45) / juce::jmax (20.0, frequency)));

        if (table >= 8)
        {
            const float point = (float) (wrapTwoPi (phase) / juce::MathConstants<double>::twoPi)
                              * (float) kCustomWavetablePoints;
            const int indexA = ((int) std::floor (point)) % kCustomWavetablePoints;
            const int indexB = (indexA + 1) % kCustomWavetablePoints;
            const int frameCount = juce::jlimit (1, kCustomWavetableFrames, juce::roundToInt (atomics.wtFrameCount.load()));
            const float framePosition = juce::jlimit (0.0f, 1.0f, atomics.wtFramePosition.load());
            const float customFrame = framePosition * (float) juce::jmax (0, frameCount - 1);
            const int customFrameA = juce::jlimit (0, frameCount - 1, (int) std::floor (customFrame));
            const int customFrameB = juce::jlimit (0, frameCount - 1, customFrameA + 1);
            const float blend = customFrame - (float) customFrameA;
            auto frameSample = [&] (int frameIndex)
            {
                const float a = atomics.wtFrameShape[(size_t) frameIndex][(size_t) indexA].load();
                const float b = atomics.wtFrameShape[(size_t) frameIndex][(size_t) indexB].load();
                return a + (b - a) * (point - (float) std::floor (point));
            };
            float sample = frameSample (customFrameA);
            sample += (frameSample (customFrameB) - sample) * blend;
            const float harmonic = std::sin ((float) phase * (2.0f + morph * 6.0f));
            sample = sample + (harmonic - sample) * warp * 0.35f;
            const float tiltDrive = spectralTilt < 0.0f ? 1.0f + std::abs (spectralTilt) * 0.75f
                                                        : 1.0f - spectralTilt * 0.35f;
            return bipolarFold (std::tanh (sample * tiltDrive * (1.0f + morph * 1.5f)), fold);
        }

        const float a = frameSample (table, frameA, phase, warpedPosition, morph, maxHarmonic, spectralTilt);
        const float b = frameSample (table, frameB, phase, warpedPosition, morph, maxHarmonic, spectralTilt);
        float sample = a + (b - a) * frameBlend;

        if (morph > 0.0001f)
        {
            const float alternate = frameSample ((table + 1) & 7, frameB, phase, 1.0f - warpedPosition,
                                                morph, maxHarmonic, spectralTilt);
            sample = sample + (alternate - sample) * morph;
        }

        sample = std::tanh (sample * (1.0f + warp * 1.8f));
        return bipolarFold (sample, fold);
    }

    void SynthEngine::process (juce::AudioBuffer<float>& buffer,
                               int startSample, int numSamples)
    {
        if (numSamples <= 0) return;
        const int numChans = juce::jmin (2, buffer.getNumChannels());

        if (numChans > preparedChannels || numSamples > preparedMaxSamples)
            return;
        tempBuffer.clear (0, numSamples);

        const int oscT = juce::jlimit (0, 3, (int) std::round (atomics.oscType.load()));
        const int osc2T = juce::jlimit (0, 3, (int) std::round (atomics.osc2Type.load()));
        const float oscBlend = juce::jlimit (0.0f, 1.0f, atomics.oscBlend.load());
        const float subBlend = juce::jlimit (0.0f, 1.0f, atomics.subBlend.load());
        const float noiseBlend = juce::jlimit (0.0f, 1.0f, atomics.noiseBlend.load());
        const bool wtEnabled = atomics.wtEnabled.load() >= 0.5f || atomics.wtLevel.load() > 0.0001f;
        const int wtTable = juce::jlimit (0, 8, juce::roundToInt (atomics.wtTable.load()));
        const float wtPosition = juce::jlimit (0.0f, 1.0f, atomics.wtPosition.load());
        const float wtMorph = juce::jlimit (0.0f, 1.0f, atomics.wtMorph.load());
        const float wtWarp = juce::jlimit (0.0f, 1.0f, atomics.wtWarp.load());
        const float wtFold = juce::jlimit (0.0f, 1.0f, atomics.wtFold.load());
        const float wtBend = juce::jlimit (-1.0f, 1.0f, atomics.wtBend.load());
        const int wtSyncRatio = juce::jlimit (1, 8, juce::roundToInt (atomics.wtSyncRatio.load()));
        const float wtSpectralTilt = juce::jlimit (-1.0f, 1.0f, atomics.wtSpectralTilt.load());
        const int wtUnison = wtEnabled ? juce::jlimit (1, 8, juce::roundToInt (atomics.wtUnison.load())) : 1;
        const float wtDetune = juce::jlimit (0.0f, 80.0f, atomics.wtDetune.load());
        const float wtSpread = juce::jlimit (0.0f, 1.0f, atomics.wtSpread.load());
        const float wtLevel = juce::jlimit (0.0f, 1.5f, atomics.wtLevel.load());

        auto* L = tempBuffer.getWritePointer (0);
        auto* R = tempBuffer.getWritePointer (juce::jmin (1, tempBuffer.getNumChannels() - 1));

        for (auto& v : voices)
        {
            if (! v.active) continue;

            // Re-apply pitch parameters every block in case octave/detune changed
            v.phaseInc = juce::MathConstants<double>::twoPi
                         * frequencyForVoice (v.note) / sampleRate;
            v.phaseInc2 = juce::MathConstants<double>::twoPi
                         * frequencyForVoice (v.note, atomics.osc2DetuneCents.load()) / sampleRate;
            v.subPhaseInc = juce::MathConstants<double>::twoPi
                         * frequencyForVoice (v.note, 0.0f, -1.0f) / sampleRate;
            double wtFrequencies[8] {};
            for (int u = 0; u < wtUnison; ++u)
            {
                const float centre = wtUnison == 1 ? 0.0f
                    : ((float) u / (float) (wtUnison - 1) - 0.5f) * 2.0f;
                const float cents = centre * wtDetune;
                wtFrequencies[u] = frequencyForVoice (v.note, cents);
                v.wavetablePhaseInc[(size_t) u] = juce::MathConstants<double>::twoPi * wtFrequencies[u] / sampleRate;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                const float ev = v.env.getNextSample();
                if (! v.env.isActive()) { v.active = false; break; }
                const float primary = oscSample (oscT, v.phase, rng);
                const float secondary = oscSample (osc2T, v.phase2, rng);
                const float sub = oscSample (2, v.subPhase, rng);
                const float noise = rng.nextFloat() * 2.0f - 1.0f;
                const float blendedOsc = primary * (1.0f - oscBlend) + secondary * oscBlend;
                float stackL = blendedOsc + sub * subBlend + noise * noiseBlend;
                float stackR = stackL;
                float stackGain = 1.0f + subBlend + noiseBlend;

                if (wtEnabled && wtLevel > 0.0001f)
                {
                    float wtL = 0.0f;
                    float wtR = 0.0f;
                    float wtNorm = 0.0f;
                    for (int u = 0; u < wtUnison; ++u)
                    {
                        const float centre = wtUnison == 1 ? 0.0f
                            : ((float) u / (float) (wtUnison - 1) - 0.5f) * 2.0f;
                        const float pan = centre * wtSpread;
                        const float sample = wavetableSample (wtTable,
                                                              v.wavetablePhase[(size_t) u],
                                                              wtPosition, wtMorph, wtWarp, wtFold,
                                                              wtBend, wtSyncRatio, wtSpectralTilt,
                                                              wtFrequencies[u]);
                        wtL += sample * std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                        wtR += sample * std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                        wtNorm += 0.7071f;
                        v.wavetablePhase[(size_t) u] += v.wavetablePhaseInc[(size_t) u];
                        if (v.wavetablePhase[(size_t) u] > juce::MathConstants<double>::twoPi)
                            v.wavetablePhase[(size_t) u] = std::fmod (v.wavetablePhase[(size_t) u],
                                                                       juce::MathConstants<double>::twoPi);
                    }
                    if (wtNorm > 0.0f)
                    {
                        wtL /= wtNorm;
                        wtR /= wtNorm;
                    }
                    stackL += wtL * wtLevel;
                    stackR += wtR * wtLevel;
                    stackGain += wtLevel;
                }

                const float sL = stackL / stackGain * v.velocity * ev * 0.25f;
                const float sR = stackR / stackGain * v.velocity * ev * 0.25f;
                L[i] += sL;
                if (R != L) R[i] += sR;
                v.phase += v.phaseInc;
                v.phase2 += v.phaseInc2;
                v.subPhase += v.subPhaseInc;
                if (v.phase > juce::MathConstants<double>::twoPi)
                    v.phase -= juce::MathConstants<double>::twoPi;
                if (v.phase2 > juce::MathConstants<double>::twoPi)
                    v.phase2 -= juce::MathConstants<double>::twoPi;
                if (v.subPhase > juce::MathConstants<double>::twoPi)
                    v.subPhase -= juce::MathConstants<double>::twoPi;
            }
        }

        // Run EQ and Advanced FX
        eq.process (tempBuffer, 0, numSamples);
        advancedFx.process (tempBuffer, 0, numSamples);

        // Run the dynamic modular DSP rack
        dspRack.process (tempBuffer);
        utility.processOutput (tempBuffer, 0, numSamples);

        // Master volume + pan, mix into destination
        const float vol = juce::jlimit (0.0f, 2.0f, atomics.volume.load())
                        * juce::jlimit (0.0f, 1.0f, atomics.expression.load());
        const float pan = juce::jlimit (-1.0f, 1.0f, atomics.pan.load());
        const float lG = vol * std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        const float rG = vol * std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.addSample (0, startSample + i, L[i] * lG);
            if (buffer.getNumChannels() > 1)
                buffer.addSample (1, startSample + i, R[i] * rG);
        }
    }

} // namespace patchcraft
