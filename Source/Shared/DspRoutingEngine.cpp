#include "DspRoutingEngine.h"
#include "LiveValueStore.h"

#include <cmath>

namespace patchcraft
{
    namespace
    {
        static float onePoleCoefficient (float milliseconds, double sampleRate, int numSamples)
        {
            const auto seconds = juce::jlimit (0.001f, 2.0f, milliseconds * 0.001f);
            return (float) std::exp (-((double) juce::jmax (1, numSamples)
                                      / (RenderContext::sanitiseSampleRate (sampleRate) * seconds)));
        }

        static float gainToFollower01 (float gain, float thresholdDb, float sensitivity)
        {
            const auto db = juce::Decibels::gainToDecibels (juce::jmax (gain, 0.000001f), -120.0f);
            const auto width = juce::jlimit (6.0f, 72.0f, 48.0f / juce::jlimit (0.25f, 4.0f, sensitivity));
            return juce::jlimit (0.0f, 1.0f, (db - thresholdDb) / width);
        }

        static bool isAudioReactiveType (const juce::String& type)
        {
            return type.containsIgnoreCase ("follower")
                || type.containsIgnoreCase ("transient")
                || type.containsIgnoreCase ("centroid")
                || type.containsIgnoreCase ("bandEnergy")
                || type.containsIgnoreCase ("gateTrigger");
        }
    }

    void DspRoutingEngine::prepare (double sr)
    {
        sampleRate = juce::jmax (8000.0, sr);
    }

    void DspRoutingEngine::prepare (const RenderContext& context)
    {
        prepare (context.sampleRate);
    }

    void DspRoutingEngine::reset()
    {
        for (auto& block : blocks)
            block.phase = 0.0;
        for (auto& lane : automation)
            lane.phase = 0.0;
        audioAnalysis = {};
    }

    void DspRoutingEngine::bind (const DspGraph& graph, const ParameterModel& parameters)
    {
        params.clear();
        blocks.clear();
        typedNodes = graph.buildTypedNodes();
        edges = graph.buildAudioEdges();
        macros = graph.macros;
        modulation = graph.modulation;
        automation.clear();

        for (const auto& def : parameters.getAll())
        {
            ParamSlot slot;
            slot.id = def.id;
            slot.min = def.min;
            slot.max = def.max;
            slot.defaultValue = def.defaultValue;
            slot.current = def.defaultValue;
            slot.routed = def.defaultValue;
            params.push_back (slot);
        }

        for (const auto& block : graph.blocks)
            blocks.push_back ({ block, 0.0, 0.0f });

        for (const auto& lane : graph.automation)
            automation.push_back ({ lane, 0.0 });
    }

    void DspRoutingEngine::setParameterValue (const juce::String& parameterId, float value)
    {
        if (auto* param = findParam (parameterId))
            param->current = juce::jlimit (param->min, param->max, value);
    }

    bool DspRoutingEngine::setFxBlockParameterValue (const juce::String& parameterId, float value)
    {
        bool changed = false;
        for (auto& block : blocks)
        {
            auto found = block.block.values.find (parameterId);
            if (found == block.block.values.end())
                continue;

            found->second = value;
            changed = true;
        }
        return changed;
    }

    void DspRoutingEngine::syncFromLiveValues (const LiveValueStore& liveValues)
    {
        for (auto& param : params)
            param.current = liveValues.getValue (param.id, param.defaultValue);
    }

    DspRoutingEngine::ParamSlot* DspRoutingEngine::findParam (const juce::String& id)
    {
        for (auto& param : params)
            if (param.id == id)
                return &param;
        return nullptr;
    }

    const DspRoutingEngine::ParamSlot* DspRoutingEngine::findParam (const juce::String& id) const
    {
        for (const auto& param : params)
            if (param.id == id)
                return &param;
        return nullptr;
    }

    DspRoutingEngine::BlockSlot* DspRoutingEngine::findBlock (const juce::String& id)
    {
        for (auto& block : blocks)
            if (block.block.id == id)
                return &block;
        return nullptr;
    }

    const DspRoutingEngine::BlockSlot* DspRoutingEngine::findBlock (const juce::String& id) const
    {
        for (const auto& block : blocks)
            if (block.block.id == id)
                return &block;
        return nullptr;
    }

    float DspRoutingEngine::valueForKey (const DspBlock& block, const juce::String& key, float fallback)
    {
        const auto it = block.values.find (key);
        return it == block.values.end() ? fallback : it->second;
    }

    bool DspRoutingEngine::blockSignalIsUnipolar (const DspBlock& block)
    {
        if (isAudioReactiveType (block.type))
            return valueForKey (block, "bipolar", 0.0f) < 0.5f;

        return ! block.type.containsIgnoreCase ("lfo")
            && ! block.type.containsIgnoreCase ("random");
    }

    void DspRoutingEngine::captureAudioAnalysis (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        const int channels = buffer.getNumChannels();
        if (channels <= 0 || numSamples <= 0)
            return;

        const int start = juce::jlimit (0, buffer.getNumSamples(), startSample);
        const int count = juce::jlimit (0, buffer.getNumSamples() - start, numSamples);
        if (count <= 0)
            return;

        const float lowAlpha = juce::jlimit (0.001f, 0.50f, 90.0f / (float) RenderContext::sanitiseSampleRate (sampleRate));
        const float midAlpha = juce::jlimit (0.001f, 0.90f, 1400.0f / (float) RenderContext::sanitiseSampleRate (sampleRate));
        float peak = 0.0f;
        double rmsSum = 0.0;
        double lowSum = 0.0;
        double midSum = 0.0;
        double highSum = 0.0;
        double diffSum = 0.0;
        float previousMono = 0.0f;
        auto lowState = audioAnalysis.lowState;
        auto midState = audioAnalysis.midState;

        for (int i = 0; i < count; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                mono += buffer.getSample (ch, start + i);
            mono /= (float) channels;

            const auto absMono = std::abs (mono);
            peak = juce::jmax (peak, absMono);
            rmsSum += (double) mono * (double) mono;

            lowState += lowAlpha * (mono - lowState);
            midState += midAlpha * (mono - midState);
            const auto mid = midState - lowState;
            const auto high = mono - midState;
            lowSum += (double) lowState * (double) lowState;
            midSum += (double) mid * (double) mid;
            highSum += (double) high * (double) high;
            diffSum += std::abs (mono - previousMono);
            previousMono = mono;
        }

        audioAnalysis.lowState = lowState;
        audioAnalysis.midState = midState;
        audioAnalysis.peak = peak;
        audioAnalysis.rms = std::sqrt ((float) (rmsSum / (double) count));
        audioAnalysis.lowEnergy = juce::jlimit (0.0f, 1.0f, std::sqrt ((float) (lowSum / (double) count)) * 2.0f);
        audioAnalysis.midEnergy = juce::jlimit (0.0f, 1.0f, std::sqrt ((float) (midSum / (double) count)) * 2.0f);
        audioAnalysis.highEnergy = juce::jlimit (0.0f, 1.0f, std::sqrt ((float) (highSum / (double) count)) * 2.0f);
        audioAnalysis.spectralCentroid = juce::jlimit (0.0f, 1.0f,
            (float) (diffSum / (double) count) / juce::jmax (audioAnalysis.rms * 4.0f, 0.0001f));

        const auto previousEnvelope = audioAnalysis.envelope;
        const bool rising = audioAnalysis.rms > previousEnvelope;
        const auto coeff = onePoleCoefficient (rising ? 12.0f : 180.0f, sampleRate, count);
        audioAnalysis.envelope = audioAnalysis.rms + (previousEnvelope - audioAnalysis.rms) * coeff;
        audioAnalysis.transient = juce::jlimit (0.0f, 1.0f, (audioAnalysis.envelope - previousEnvelope) * 18.0f);
    }

    float DspRoutingEngine::blockSignal (BlockSlot& block, const RenderContext& context)
    {
        if (isAudioReactiveType (block.block.type))
        {
            const auto threshold = juce::jlimit (-90.0f, 0.0f, valueForKey (block.block, "thresholdDb", -36.0f));
            const auto sensitivity = juce::jlimit (0.25f, 4.0f, valueForKey (block.block, "sensitivity", 1.0f));
            float value = 0.0f;

            if (block.block.type.containsIgnoreCase ("peak"))
                value = gainToFollower01 (audioAnalysis.peak, threshold, sensitivity);
            else if (block.block.type.containsIgnoreCase ("rms"))
                value = gainToFollower01 (audioAnalysis.rms, threshold, sensitivity);
            else if (block.block.type.containsIgnoreCase ("transient"))
                value = audioAnalysis.transient;
            else if (block.block.type.containsIgnoreCase ("centroid"))
                value = audioAnalysis.spectralCentroid;
            else if (block.block.type.containsIgnoreCase ("bandEnergy"))
            {
                const int band = juce::jlimit (0, 2, juce::roundToInt (valueForKey (block.block, "band", 0.0f)));
                const float energy = band == 0 ? audioAnalysis.lowEnergy : band == 1 ? audioAnalysis.midEnergy : audioAnalysis.highEnergy;
                value = gainToFollower01 (energy, threshold, sensitivity);
            }
            else if (block.block.type.containsIgnoreCase ("gateTrigger"))
            {
                const auto db = juce::Decibels::gainToDecibels (juce::jmax (audioAnalysis.envelope, 0.000001f), -120.0f);
                value = db >= threshold ? 1.0f : 0.0f;
            }
            else
            {
                value = gainToFollower01 (audioAnalysis.envelope, threshold, sensitivity);
            }

            const auto smoothingMs = juce::jlimit (1.0f, 500.0f, valueForKey (block.block, "smoothingMs", 80.0f));
            const auto coeff = onePoleCoefficient (smoothingMs, sampleRate, context.blockSize);
            block.heldValue = value + (block.heldValue - value) * coeff;

            return valueForKey (block.block, "bipolar", 0.0f) >= 0.5f
                ? block.heldValue * 2.0f - 1.0f
                : block.heldValue;
        }

        if (block.block.type.containsIgnoreCase ("lfo"))
        {
            const float fallbackRate = findParam ("lfoRate") != nullptr ? findParam ("lfoRate")->current : 1.0f;
            const float rate = juce::jlimit (0.01f, 40.0f, valueForKey (block.block, "rate", fallbackRate));
            const auto* globalSync = findParam ("bpmSync");
            const bool tempoSync = valueForKey (block.block, "sync", 0.0f) >= 0.5f
                && (globalSync == nullptr || globalSync->current >= 0.5f);
            const double cyclesPerSecond = tempoSync ? (context.bpm / 240.0) * rate : rate;
            const auto value = (float) std::sin (block.phase);
            block.phase += juce::MathConstants<double>::twoPi
                         * cyclesPerSecond * context.secondsPerBlock();
            if (block.phase > juce::MathConstants<double>::twoPi)
                block.phase = std::fmod (block.phase, juce::MathConstants<double>::twoPi);
            return value;
        }

        if (block.block.type.containsIgnoreCase ("random"))
        {
            const float fallbackRate = findParam ("lfoRate") != nullptr ? findParam ("lfoRate")->current : 4.0f;
            const float rate = juce::jlimit (0.01f, 40.0f, valueForKey (block.block, "rate", fallbackRate));
            block.phase -= context.secondsPerBlock();
            if (block.phase <= 0.0)
            {
                block.heldValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                block.phase += 1.0 / (double) rate;
            }
            return block.heldValue;
        }

        if (block.block.type.containsIgnoreCase ("midiPlayground")
            || block.block.type.containsIgnoreCase ("phrase generator")
            || block.block.type.containsIgnoreCase ("midi generator")
            || block.block.values.count ("mpStep0On") != 0)
        {
            const float rate = juce::jlimit (0.0625f, 32.0f, valueForKey (block.block, "rate", 1.0f));
            const auto* globalSync = findParam ("bpmSync");
            const bool tempoSync = valueForKey (block.block, "sync", 1.0f) >= 0.5f
                && (globalSync == nullptr || globalSync->current >= 0.5f);
            const int polymeterSteps = juce::roundToInt (valueForKey (block.block, "mpPolymeterSteps", 0.0f));
            const int steps = juce::jlimit (1, 128,
                polymeterSteps > 0 ? polymeterSteps : juce::roundToInt (valueForKey (block.block, "arpSteps", 8.0f)));
            const double phase01 = block.phase - std::floor (block.phase);
            const double scaled = phase01 * (double) steps;
            const int step = juce::jlimit (0, steps - 1, (int) std::floor (scaled));
            const double stepPhase = scaled - std::floor (scaled);
            const bool enabled = valueForKey (block.block, "mpStep" + juce::String (step) + "On", 1.0f) >= 0.5f;
            const float probability = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "mpStepProb" + juce::String (step), 1.0f));
            const float gate = juce::jlimit (0.05f, 1.0f,
                valueForKey (block.block, "mpGate" + juce::String (step), valueForKey (block.block, "arpGate", 0.55f)));

            float output = 0.0f;
            if (enabled && probability > 0.0f && stepPhase <= (double) gate)
            {
                const int lane = juce::jlimit (0, 7, juce::roundToInt (valueForKey (block.block, "mpModLane", 0.0f)));
                if (lane == 1)
                    output = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "mpVelocity" + juce::String (step), 1.0f));
                else if (lane == 2)
                    output = gate;
                else if (lane == 3)
                    output = probability;
                else if (lane == 4)
                    output = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "mpSampleSlice" + juce::String (step), (float) step)
                        / juce::jmax (1.0f, valueForKey (block.block, "mpSampleSliceCount", 16.0f) - 1.0f));
                else if (lane == 5)
                    output = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "mpAutoFilter" + juce::String (step), 0.5f));
                else if (lane == 6)
                    output = juce::jlimit (0.0f, 1.0f, (valueForKey (block.block, "mpAutoPan" + juce::String (step), 0.0f) + 1.0f) * 0.5f);
                else if (lane == 7)
                    output = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "mpAutoFxSend" + juce::String (step), 0.0f));
                else
                    output = juce::jlimit (0.0f, 1.0f,
                        (valueForKey (block.block, "arpNote" + juce::String (step), 0.0f) + 24.0f) / 48.0f);
            }

            block.heldValue = output;
            const double cyclesPerSecond = tempoSync ? (context.bpm / 240.0) * rate : rate;
            block.phase += cyclesPerSecond * context.secondsPerBlock();
            block.phase -= std::floor (block.phase);
            return output;
        }

        if (block.block.type.containsIgnoreCase ("step") || block.block.type.containsIgnoreCase ("sequencer"))
        {
            const float rate = juce::jlimit (0.0625f, 32.0f, valueForKey (block.block, "rate", 1.0f));
            const auto* globalSync = findParam ("bpmSync");
            const bool tempoSync = valueForKey (block.block, "sync", 1.0f) >= 0.5f
                && (globalSync == nullptr || globalSync->current >= 0.5f);
            const double cyclesPerSecond = tempoSync ? (context.bpm / 240.0) * rate : rate;
            const int steps = juce::jlimit (1, 16, juce::roundToInt (valueForKey (block.block, "steps", 8.0f)));
            const int step = juce::jlimit (0, steps - 1, (int) std::floor ((block.phase - std::floor (block.phase)) * (double) steps));
            block.heldValue = juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "step" + juce::String (step), step % 2 == 0 ? 1.0f : 0.0f));
            block.phase += cyclesPerSecond * context.secondsPerBlock();
            block.phase -= std::floor (block.phase);
            return valueForKey (block.block, "bipolar", 0.0f) >= 0.5f ? block.heldValue * 2.0f - 1.0f : block.heldValue;
        }

        if (block.block.type.containsIgnoreCase ("arp"))
        {
            const float rate = juce::jlimit (0.0625f, 16.0f, valueForKey (block.block, "rate", 1.0f));
            const auto* globalSync = findParam ("bpmSync");
            const bool tempoSync = valueForKey (block.block, "sync", 1.0f) >= 0.5f
                && (globalSync == nullptr || globalSync->current >= 0.5f);
            const double cyclesPerSecond = tempoSync ? (context.bpm / 240.0) * rate : rate;
            const int steps = juce::jlimit (1, 128, juce::roundToInt (valueForKey (block.block, "arpSteps", 8.0f)));
            const int pattern = juce::jlimit (0, 5, juce::roundToInt (valueForKey (block.block, "arpPattern", 0.0f)));
            const float gate = juce::jlimit (0.05f, 1.0f, valueForKey (block.block, "arpGate", 0.55f));
            const double phase01 = block.phase - std::floor (block.phase);
            const double scaled = phase01 * (double) steps;
            const int step = juce::jlimit (0, steps - 1, (int) std::floor (scaled));
            const double stepPhase = scaled - std::floor (scaled);
            const bool active = stepPhase <= (double) gate;
            const float defaultNotes[] { 0.0f, 4.0f, 7.0f, 12.0f, 7.0f, 4.0f, 10.0f, 14.0f,
                                         12.0f, 7.0f, 4.0f, 0.0f, 5.0f, 9.0f, 12.0f, 16.0f };
            int sequenceIndex = step;
            if (pattern == 1)
                sequenceIndex = steps - 1 - step;
            else if (pattern == 2)
                sequenceIndex = step <= steps / 2 ? step : juce::jmax (0, steps - 1 - step);
            else if (pattern == 4)
                sequenceIndex = (step * 2 + 1) % steps;
            else if (pattern == 5)
                sequenceIndex = (step * 2) % steps;

            sequenceIndex = juce::jlimit (0, 15, sequenceIndex);
            float note = valueForKey (block.block, "arpNote" + juce::String (sequenceIndex), defaultNotes[sequenceIndex]);
            if (pattern == 3)
                note = defaultNotes[(step % 4) * 2];

            const float octaveSpan = juce::jlimit (1.0f, 4.0f, valueForKey (block.block, "arpOctaves", 2.0f)) * 12.0f;
            block.heldValue = juce::jlimit (0.0f, 1.0f, note / juce::jmax (1.0f, octaveSpan));

            block.phase += cyclesPerSecond * context.secondsPerBlock();
            block.phase -= std::floor (block.phase);
            return pattern == 3 && ! active ? 0.0f : block.heldValue;
        }

        if (block.block.type.containsIgnoreCase ("velocity")
            || block.block.type.containsIgnoreCase ("keytrack")
            || block.block.type.containsIgnoreCase ("midi")
            || block.block.type.containsIgnoreCase ("cc")
            || block.block.type.containsIgnoreCase ("macro"))
            return juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, "value", 0.5f));

        return valueForKey (block.block, "value", 0.5f);
    }

    float DspRoutingEngine::sourceValue (const juce::String& id, const RenderContext& context)
    {
        if (auto* param = findParam (id))
        {
            const float norm = juce::jmap (param->current, param->min, param->max, 0.0f, 1.0f);
            return norm * 2.0f - 1.0f;
        }

        if (auto* block = findBlock (id))
            return blockSignal (*block, context);

        return 0.0f;
    }

    float DspRoutingEngine::laneValue (LaneSlot& lane, const RenderContext& context) const
    {
        if (lane.lane.points.empty())
            return 0.0f;

        const auto& points = lane.lane.points;
        const auto scaled = lane.phase * (double) points.size();
        const auto index = (size_t) juce::jlimit (0, (int) points.size() - 1, (int) scaled);
        const auto next = (index + 1) % points.size();
        const float frac = (float) (scaled - std::floor (scaled));
        const float value = points[index] + (points[next] - points[index]) * frac;

        const auto* globalSync = findParam ("bpmSync");
        const bool tempoSync = lane.lane.syncToTempo
            && (globalSync == nullptr || globalSync->current >= 0.5f);
        const double cyclesPerSecond = tempoSync
            ? (context.bpm / 240.0) * juce::jmax (0.01f, lane.lane.rate)
            : juce::jmax (0.01f, lane.lane.rate);
        lane.phase += cyclesPerSecond * context.secondsPerBlock();
        lane.phase -= std::floor (lane.phase);
        return juce::jlimit (0.0f, 1.0f, value);
    }

    void DspRoutingEngine::processToEngine (IInstrumentEngine& engine, int numSamples, double bpm)
    {
        processToEngine (engine, RenderContext::forBlock (sampleRate, numSamples, numSamples, 0, 2, bpm));
    }

    void DspRoutingEngine::processToEngine (IInstrumentEngine& engine, const RenderContext& context)
    {
        if (params.empty())
            return;

        sampleRate = RenderContext::sanitiseSampleRate (context.sampleRate);
        engine.setRenderContext (context);

        for (auto& param : params)
            param.routed = param.current;

        for (auto& lane : automation)
            if (auto* block = findBlock (lane.lane.targetId))
                block->block.values["value"] = laneValue (lane, context);

        auto setNorm = [this] (const juce::String& id, float value01)
        {
            if (auto* target = findParam (id))
                target->routed = juce::jlimit (target->min, target->max,
                    juce::jmap (juce::jlimit (0.0f, 1.0f, value01), 0.0f, 1.0f, target->min, target->max));
        };
        auto setValue = [this] (const juce::String& id, float value)
        {
            if (auto* target = findParam (id))
                target->routed = juce::jlimit (target->min, target->max, value);
        };
        auto setExactOrNorm = [&] (const juce::String& id, const DspBlock& block, const juce::String& key)
        {
            if (auto* target = findParam (id))
            {
                const auto normalised = valueForKey (block, key, valueForKey (block, "value", 0.5f));
                target->routed = juce::jlimit (target->min, target->max,
                    juce::jmap (juce::jlimit (0.0f, 1.0f, normalised), 0.0f, 1.0f, target->min, target->max));
            }
        };

        auto applyUtilityValues = [&] (const DspBlock& block)
        {
            if (block.values.count ("inputTrimDb") != 0)     setValue ("inputTrimDb", juce::jlimit (-48.0f, 24.0f, valueForKey (block, "inputTrimDb", 0.0f)));
            if (block.values.count ("phaseInvert") != 0)     setValue ("phaseInvert", valueForKey (block, "phaseInvert", 0.0f) >= 0.5f ? 1.0f : 0.0f);
            if (block.values.count ("stereoWidth") != 0)     setValue ("stereoWidth", juce::jlimit (0.0f, 2.0f, valueForKey (block, "stereoWidth", 1.0f)));
            if (block.values.count ("monoMaker") != 0)       setValue ("monoMaker", juce::jlimit (0.0f, 1.0f, valueForKey (block, "monoMaker", 0.0f)));
            if (block.values.count ("outputGainDb") != 0)    setValue ("outputGainDb", juce::jlimit (-48.0f, 24.0f, valueForKey (block, "outputGainDb", 0.0f)));
            if (block.values.count ("outputLimiter") != 0)   setValue ("outputLimiter", valueForKey (block, "outputLimiter", 1.0f) >= 0.5f ? 1.0f : 0.0f);
            if (block.values.count ("outputCeilingDb") != 0) setValue ("outputCeilingDb", juce::jlimit (-24.0f, 0.0f, valueForKey (block, "outputCeilingDb", -0.5f)));
        };
        auto isHandledOutputTarget = [] (const juce::String& id)
        {
            return id == "inputTrimDb"
                || id == "phaseInvert"
                || id == "stereoWidth"
                || id == "monoMaker"
                || id == "outputGainDb"
                || id == "outputLimiter"
                || id == "outputCeilingDb"
                || id == "volume"
                || id == "pan"
                || id == "bpmSync"
                || id == "retrigger";
        };

        auto applyTypedNode = [&] (BlockSlot& block, DspNodeKind nodeKind)
        {
            if (! block.block.enabled)
                return;

            if (nodeKind == DspNodeKind::utility)
            {
                applyUtilityValues (block.block);
            }
            else if (nodeKind == DspNodeKind::processor && block.block.section == "filter")
            {
                if (block.block.type.containsIgnoreCase ("eq"))
                {
                    const int band = juce::jlimit (1, 8, juce::roundToInt (valueForKey (block.block, "eqBand", 1.0f)));
                    const auto prefix = "eqBand" + juce::String (band);
                    setValue ("eqEnabled", 1.0f);
                    setValue ("eqMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "eqMix", 1.0f)));
                    setValue (prefix + "On", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "eqOn", 1.0f)));
                    setValue (prefix + "Type", juce::jlimit (0.0f, 5.0f, valueForKey (block.block, "eqType", 0.0f)));
                    setValue (prefix + "Mode", juce::jlimit (0.0f, 4.0f, valueForKey (block.block, "eqMode", 0.0f)));
                    setValue (prefix + "Freq", juce::jlimit (20.0f, 20000.0f, valueForKey (block.block, "eqFreq", 1000.0f)));
                    setValue (prefix + "GainDb", juce::jlimit (-24.0f, 24.0f, valueForKey (block.block, "eqGainDb", 0.0f)));
                    setValue (prefix + "Q", juce::jlimit (0.10f, 18.0f, valueForKey (block.block, "eqQ", 1.0f)));
                    if (block.block.values.count ("eqSolo") != 0)           setValue (prefix + "Solo", valueForKey (block.block, "eqSolo", 0.0f) >= 0.5f ? 1.0f : 0.0f);
                    if (block.block.values.count ("eqDynMode") != 0)        setValue (prefix + "DynMode", juce::jlimit (0.0f, 3.0f, valueForKey (block.block, "eqDynMode", 0.0f)));
                    if (block.block.values.count ("eqDynThresholdDb") != 0) setValue (prefix + "DynThresholdDb", juce::jlimit (-80.0f, 12.0f, valueForKey (block.block, "eqDynThresholdDb", -24.0f)));
                    if (block.block.values.count ("eqDynRangeDb") != 0)     setValue (prefix + "DynRangeDb", juce::jlimit (-24.0f, 24.0f, valueForKey (block.block, "eqDynRangeDb", 0.0f)));
                    if (block.block.values.count ("eqDynAttackMs") != 0)    setValue (prefix + "DynAttackMs", juce::jlimit (0.1f, 250.0f, valueForKey (block.block, "eqDynAttackMs", 10.0f)));
                    if (block.block.values.count ("eqDynReleaseMs") != 0)   setValue (prefix + "DynReleaseMs", juce::jlimit (5.0f, 1000.0f, valueForKey (block.block, "eqDynReleaseMs", 120.0f)));
                }
                else
                {
                    if (block.block.values.count ("cutoff") != 0) setNorm ("filterCutoff", valueForKey (block.block, "cutoff", 0.5f));
                    if (block.block.values.count ("resonance") != 0) setNorm ("filterResonance", valueForKey (block.block, "resonance", 0.2f));
                    if (block.block.values.count ("lfoAmount") != 0) setNorm ("lfoAmount", valueForKey (block.block, "lfoAmount", 0.0f));
                    if (block.block.values.count ("rate") != 0) setValue ("lfoRate", juce::jlimit (0.1f, 20.0f, valueForKey (block.block, "rate", 1.0f)));
                }
            }
            else if (nodeKind == DspNodeKind::processor && block.block.section == "amp")
            {
                if (block.block.values.count ("attack") != 0) setNorm ("attack", valueForKey (block.block, "attack", 0.01f));
                if (block.block.values.count ("decay") != 0) setNorm ("decay", valueForKey (block.block, "decay", 0.2f));
                if (block.block.values.count ("sustain") != 0) setNorm ("sustain", valueForKey (block.block, "sustain", 0.8f));
                if (block.block.values.count ("release") != 0) setNorm ("release", valueForKey (block.block, "release", 0.4f));
            }
            else if (nodeKind == DspNodeKind::processor && block.block.section == "fx")
            {
                if (block.block.values.count ("delayMix") != 0) setNorm ("delayMix", valueForKey (block.block, "delayMix", 0.0f));
                if (block.block.values.count ("delayFeedback") != 0) setNorm ("delayFeedback", valueForKey (block.block, "delayFeedback", 0.4f));
                if (block.block.values.count ("delayTime") != 0)
                {
                    const auto* globalSync = findParam ("bpmSync");
                    const bool tempoSync = valueForKey (block.block, "sync", 0.0f) >= 0.5f
                        && (globalSync == nullptr || globalSync->current >= 0.5f);
                    if (tempoSync)
                    {
                        const float beats = juce::jlimit (0.0625f, 8.0f, valueForKey (block.block, "rate", 1.0f));
                        setValue ("delayTime", (float) ((60.0 / juce::jlimit (20.0, 300.0, context.bpm)) * beats));
                    }
                    else
                    {
                        setNorm ("delayTime", valueForKey (block.block, "delayTime", 0.25f));
                    }
                }
                if (block.block.values.count ("reverbMix") != 0) setNorm ("reverbMix", valueForKey (block.block, "reverbMix", 0.0f));
                if (block.block.values.count ("drive") != 0) setNorm ("drive", valueForKey (block.block, "drive", 0.0f));
                if (block.block.values.count ("mix") != 0) setNorm ("mix", valueForKey (block.block, "mix", 1.0f));
                if (block.block.values.count ("dynThresholdDb") != 0) setValue ("dynThresholdDb", juce::jlimit (-80.0f, 12.0f, valueForKey (block.block, "dynThresholdDb", -18.0f)));
                if (block.block.values.count ("dynRatio") != 0) setValue ("dynRatio", juce::jlimit (1.0f, 40.0f, valueForKey (block.block, "dynRatio", 2.0f)));
                if (block.block.values.count ("dynAttackMs") != 0) setValue ("dynAttackMs", juce::jlimit (0.1f, 250.0f, valueForKey (block.block, "dynAttackMs", 10.0f)));
                if (block.block.values.count ("dynReleaseMs") != 0) setValue ("dynReleaseMs", juce::jlimit (5.0f, 1000.0f, valueForKey (block.block, "dynReleaseMs", 120.0f)));
                if (block.block.values.count ("dynMakeupDb") != 0) setValue ("dynMakeupDb", juce::jlimit (-24.0f, 24.0f, valueForKey (block.block, "dynMakeupDb", 0.0f)));
                if (block.block.values.count ("dynMix") != 0) setValue ("dynMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "dynMix", 0.0f)));
                if (block.block.values.count ("chorusRate") != 0) setValue ("chorusRate", juce::jlimit (0.01f, 20.0f, valueForKey (block.block, "chorusRate", 0.35f)));
                if (block.block.values.count ("chorusDepth") != 0) setValue ("chorusDepth", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "chorusDepth", 0.35f)));
                if (block.block.values.count ("chorusFeedback") != 0) setValue ("chorusFeedback", juce::jlimit (-0.95f, 0.95f, valueForKey (block.block, "chorusFeedback", 0.0f)));
                if (block.block.values.count ("chorusMix") != 0) setValue ("chorusMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "chorusMix", 0.0f)));
                if (block.block.values.count ("phaserRate") != 0) setValue ("phaserRate", juce::jlimit (0.01f, 20.0f, valueForKey (block.block, "phaserRate", 0.25f)));
                if (block.block.values.count ("phaserDepth") != 0) setValue ("phaserDepth", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "phaserDepth", 0.45f)));
                if (block.block.values.count ("phaserFeedback") != 0) setValue ("phaserFeedback", juce::jlimit (-0.95f, 0.95f, valueForKey (block.block, "phaserFeedback", 0.0f)));
                if (block.block.values.count ("phaserMix") != 0) setValue ("phaserMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "phaserMix", 0.0f)));
                if (block.block.values.count ("combFreq") != 0) setValue ("combFreq", juce::jlimit (20.0f, 8000.0f, valueForKey (block.block, "combFreq", 220.0f)));
                if (block.block.values.count ("combFeedback") != 0) setValue ("combFeedback", juce::jlimit (-0.95f, 0.95f, valueForKey (block.block, "combFeedback", 0.35f)));
                if (block.block.values.count ("combMix") != 0) setValue ("combMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "combMix", 0.0f)));
                if (block.block.values.count ("resonatorFreq") != 0) setValue ("resonatorFreq", juce::jlimit (20.0f, 16000.0f, valueForKey (block.block, "resonatorFreq", 440.0f)));
                if (block.block.values.count ("resonatorQ") != 0) setValue ("resonatorQ", juce::jlimit (0.05f, 18.0f, valueForKey (block.block, "resonatorQ", 4.0f)));
                if (block.block.values.count ("resonatorMix") != 0) setValue ("resonatorMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "resonatorMix", 0.0f)));
                if (block.block.values.count ("convolutionSize") != 0) setValue ("convolutionSize", juce::jlimit (1.0f, 8.0f, valueForKey (block.block, "convolutionSize", 3.0f)));
                if (block.block.values.count ("convolutionMix") != 0) setValue ("convolutionMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "convolutionMix", 0.0f)));
                if (block.block.values.count ("spectralTilt") != 0) setValue ("spectralTilt", juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, "spectralTilt", 0.0f)));
                if (block.block.values.count ("spectralMix") != 0) setValue ("spectralMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "spectralMix", 0.0f)));
                if (block.block.values.count ("tapeDrive") != 0) setValue ("tapeDrive", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "tapeDrive", 0.25f)));
                if (block.block.values.count ("tapeTone") != 0) setValue ("tapeTone", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "tapeTone", 0.55f)));
                if (block.block.values.count ("tapeFlutter") != 0) setValue ("tapeFlutter", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "tapeFlutter", 0.12f)));
                if (block.block.values.count ("tapeMix") != 0) setValue ("tapeMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "tapeMix", 0.0f)));
                if (block.block.values.count ("vinylAge") != 0) setValue ("vinylAge", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vinylAge", 0.35f)));
                if (block.block.values.count ("vinylDust") != 0) setValue ("vinylDust", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vinylDust", 0.08f)));
                if (block.block.values.count ("vinylWarp") != 0) setValue ("vinylWarp", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vinylWarp", 0.12f)));
                if (block.block.values.count ("vinylMix") != 0) setValue ("vinylMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vinylMix", 0.0f)));
                if (block.block.values.count ("lofiBits") != 0) setValue ("lofiBits", juce::jlimit (4.0f, 16.0f, valueForKey (block.block, "lofiBits", 12.0f)));
                if (block.block.values.count ("lofiRate") != 0) setValue ("lofiRate", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "lofiRate", 0.20f)));
                if (block.block.values.count ("lofiMix") != 0) setValue ("lofiMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "lofiMix", 0.0f)));
                if (block.block.values.count ("vocalFormant") != 0) setValue ("vocalFormant", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vocalFormant", 0.40f)));
                if (block.block.values.count ("vocalBody") != 0) setValue ("vocalBody", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vocalBody", 0.35f)));
                if (block.block.values.count ("vocalMix") != 0) setValue ("vocalMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "vocalMix", 0.0f)));
                if (block.block.values.count ("multiTapTime") != 0) setValue ("multiTapTime", juce::jlimit (0.02f, 2.0f, valueForKey (block.block, "multiTapTime", 0.375f)));
                if (block.block.values.count ("multiTapFeedback") != 0) setValue ("multiTapFeedback", juce::jlimit (0.0f, 0.92f, valueForKey (block.block, "multiTapFeedback", 0.35f)));
                if (block.block.values.count ("multiTapSpread") != 0) setValue ("multiTapSpread", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "multiTapSpread", 0.45f)));
                if (block.block.values.count ("multiTapMix") != 0) setValue ("multiTapMix", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "multiTapMix", 0.0f)));
            }
            else if (nodeKind == DspNodeKind::source)
            {
                if (block.block.type.containsIgnoreCase ("wavetable") || block.block.values.count ("wtLevel") != 0)
                {
                    setValue ("wtEnabled", 1.0f);
                    if (block.block.values.count ("wtTable") != 0)    setValue ("wtTable", juce::jlimit (0.0f, 8.0f, valueForKey (block.block, "wtTable", 0.0f)));
                    if (block.block.values.count ("wtPosition") != 0) setValue ("wtPosition", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtPosition", 0.0f)));
                    if (block.block.values.count ("wtMorph") != 0)    setValue ("wtMorph", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtMorph", 0.0f)));
                    if (block.block.values.count ("wtWarp") != 0)     setValue ("wtWarp", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtWarp", 0.0f)));
                    if (block.block.values.count ("wtFold") != 0)     setValue ("wtFold", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtFold", 0.0f)));
                    if (block.block.values.count ("wtUnison") != 0)   setValue ("wtUnison", juce::jlimit (1.0f, 8.0f, valueForKey (block.block, "wtUnison", 1.0f)));
                    if (block.block.values.count ("wtDetune") != 0)   setValue ("wtDetune", juce::jlimit (0.0f, 80.0f, valueForKey (block.block, "wtDetune", 12.0f)));
                    if (block.block.values.count ("wtSpread") != 0)   setValue ("wtSpread", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtSpread", 0.0f)));
                    if (block.block.values.count ("wtLevel") != 0)    setValue ("wtLevel", juce::jlimit (0.0f, 1.5f, valueForKey (block.block, "wtLevel", 1.0f)));
                    if (block.block.values.count ("wtBend") != 0)     setValue ("wtBend", juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, "wtBend", 0.0f)));
                    if (block.block.values.count ("wtSyncRatio") != 0) setValue ("wtSyncRatio", juce::jlimit (1.0f, 8.0f, valueForKey (block.block, "wtSyncRatio", 1.0f)));
                    if (block.block.values.count ("wtSpectralTilt") != 0) setValue ("wtSpectralTilt", juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, "wtSpectralTilt", 0.0f)));
                    if (block.block.values.count ("wtPhaseMode") != 0) setValue ("wtPhaseMode", juce::jlimit (0.0f, 2.0f, valueForKey (block.block, "wtPhaseMode", 0.0f)));
                    if (block.block.values.count ("wtFramePosition") != 0) setValue ("wtFramePosition", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "wtFramePosition", 0.0f)));
                    if (block.block.values.count ("wtFrameCount") != 0) setValue ("wtFrameCount", juce::jlimit (1.0f, 4.0f, valueForKey (block.block, "wtFrameCount", 1.0f)));
                    for (int point = 0; point < 32; ++point)
                    {
                        const auto shapeId = "wtShape" + juce::String (point);
                        if (block.block.values.count (shapeId) != 0)
                            setValue (shapeId, juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, shapeId, 0.0f)));
                    }
                    for (int frame = 0; frame < 4; ++frame)
                    {
                        for (int point = 0; point < 32; ++point)
                        {
                            const auto shapeId = "wtFrame" + juce::String (frame) + "Shape" + juce::String (point);
                            if (block.block.values.count (shapeId) != 0)
                                setValue (shapeId, juce::jlimit (-1.0f, 1.0f, valueForKey (block.block, shapeId, 0.0f)));
                        }
                    }
                }
                if (block.block.values.count ("drive") != 0) setNorm ("drive", valueForKey (block.block, "drive", 0.0f));
                if (block.block.values.count ("mix") != 0) setNorm ("mix", valueForKey (block.block, "mix", 1.0f));
                if (block.block.values.count ("oscType") != 0) setNorm ("oscType", valueForKey (block.block, "oscType", 0.0f));
                if (block.block.values.count ("osc2Type") != 0) setNorm ("osc2Type", valueForKey (block.block, "osc2Type", 0.75f));
                if (block.block.values.count ("oscBlend") != 0) setNorm ("oscBlend", valueForKey (block.block, "oscBlend", 0.0f));
                if (block.block.values.count ("sampleStart") != 0) setValue ("sampleStart", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "sampleStart", 0.0f)));
                if (block.block.values.count ("sampleLength") != 0) setValue ("sampleLength", juce::jlimit (0.01f, 1.0f, valueForKey (block.block, "sampleLength", 1.0f)));
                if (block.block.values.count ("sampleSlice") != 0) setValue ("sampleSlice", juce::jlimit (0.0f, 63.0f, valueForKey (block.block, "sampleSlice", 0.0f)));
                if (block.block.values.count ("sampleSliceCount") != 0) setValue ("sampleSliceCount", juce::jlimit (1.0f, 64.0f, valueForKey (block.block, "sampleSliceCount", 1.0f)));
                if (block.block.values.count ("samplePitch") != 0) setValue ("samplePitch", juce::jlimit (-48.0f, 48.0f, valueForKey (block.block, "samplePitch", 0.0f)));
                if (block.block.values.count ("sampleReverse") != 0) setValue ("sampleReverse", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "sampleReverse", 0.0f)));
                if (block.block.values.count ("granularOn") != 0) setValue ("granularOn", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularOn", 0.0f)));
                if (block.block.values.count ("granularDensity") != 0) setValue ("granularDensity", juce::jlimit (0.5f, 220.0f, valueForKey (block.block, "granularDensity", 24.0f)));
                if (block.block.values.count ("granularSizeMs") != 0) setValue ("granularSizeMs", juce::jlimit (2.0f, 1000.0f, valueForKey (block.block, "granularSizeMs", 90.0f)));
                if (block.block.values.count ("granularSizeRandom") != 0) setValue ("granularSizeRandom", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularSizeRandom", 0.25f)));
                if (block.block.values.count ("granularSpread") != 0) setValue ("granularSpread", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularSpread", 0.18f)));
                if (block.block.values.count ("granularScan") != 0) setValue ("granularScan", juce::jlimit (-3.0f, 3.0f, valueForKey (block.block, "granularScan", 0.0f)));
                if (block.block.values.count ("granularPitchSpread") != 0) setValue ("granularPitchSpread", juce::jlimit (0.0f, 36.0f, valueForKey (block.block, "granularPitchSpread", 0.0f)));
                if (block.block.values.count ("granularPanSpread") != 0) setValue ("granularPanSpread", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularPanSpread", 0.45f)));
                if (block.block.values.count ("granularReverse") != 0) setValue ("granularReverse", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularReverse", 0.0f)));
                if (block.block.values.count ("granularTexture") != 0) setValue ("granularTexture", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularTexture", 0.20f)));
                if (block.block.values.count ("granularMaxGrains") != 0) setValue ("granularMaxGrains", juce::jlimit (1.0f, 32.0f, valueForKey (block.block, "granularMaxGrains", 16.0f)));
                if (block.block.values.count ("granularDirection") != 0) setValue ("granularDirection", juce::jlimit (0.0f, 3.0f, valueForKey (block.block, "granularDirection", 3.0f)));
                if (block.block.values.count ("granularWindow") != 0) setValue ("granularWindow", juce::jlimit (0.0f, 3.0f, valueForKey (block.block, "granularWindow", 0.0f)));
                if (block.block.values.count ("granularFreeze") != 0) setValue ("granularFreeze", juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "granularFreeze", 0.0f)));
                if (block.block.values.count ("octave") != 0) setNorm ("octave", valueForKey (block.block, "octave", 0.5f));
                if (block.block.values.count ("detune") != 0) setNorm ("detune", valueForKey (block.block, "detune", 0.5f));
                if (block.block.values.count ("osc2Detune") != 0) setNorm ("osc2Detune", valueForKey (block.block, "osc2Detune", 0.535f));
                if (block.block.values.count ("subBlend") != 0) setNorm ("subBlend", valueForKey (block.block, "subBlend", 0.0f));
                if (block.block.values.count ("noiseBlend") != 0) setNorm ("noiseBlend", valueForKey (block.block, "noiseBlend", 0.0f));
                if (block.block.values.count ("volume") != 0 && ! block.block.type.containsIgnoreCase ("noise"))
                    setNorm ("volume", valueForKey (block.block, "volume", 0.75f));
                if (block.block.values.count ("pan") != 0)
                    if (auto* target = findParam ("pan"))
                        target->routed = juce::jmap (juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "pan", 0.5f)),
                                                     0.0f, 1.0f, target->min, target->max);
            }
            else if (nodeKind == DspNodeKind::output)
            {
                applyUtilityValues (block.block);
                if (block.block.values.count ("volume") != 0) setNorm ("volume", valueForKey (block.block, "volume", 0.75f));
                if (block.block.values.count ("bpmSync") != 0) setNorm ("bpmSync", valueForKey (block.block, "bpmSync", 1.0f));
                if (block.block.values.count ("retrigger") != 0) setNorm ("retrigger", valueForKey (block.block, "retrigger", 1.0f));
                if (block.block.values.count ("pan") != 0)
                    if (auto* target = findParam ("pan"))
                        target->routed = juce::jmap (juce::jlimit (0.0f, 1.0f, valueForKey (block.block, "pan", 0.5f)),
                                                     0.0f, 1.0f, target->min, target->max);
            }

            const bool utilityTargetAlreadyApplied = block.block.targetId.isNotEmpty()
                && ((nodeKind == DspNodeKind::utility)
                    || (nodeKind == DspNodeKind::output && isHandledOutputTarget (block.block.targetId)));
            if (! utilityTargetAlreadyApplied
                && block.block.targetId.isNotEmpty()
                && block.block.values.count (block.block.targetId) != 0)
                setExactOrNorm (block.block.targetId, block.block, block.block.targetId);

            if (nodeKind == DspNodeKind::modulation && block.block.targetId.isNotEmpty())
            {
                if (auto* target = findParam (block.block.targetId))
                {
                    const float signal = blockSignal (block, context);
                    const float amount = valueForKey (block.block, "amount", 0.15f);
                    const float range = target->max - target->min;
                    target->routed = juce::jlimit (target->min, target->max,
                        target->routed + signal * amount * range);
                }
            }
        };

        juce::StringArray appliedNodes;
        auto applyNodeById = [&] (const juce::String& nodeId)
        {
            if (nodeId.isEmpty() || appliedNodes.contains (nodeId, false))
                return;
            for (const auto& node : typedNodes)
            {
                if (node.id != nodeId || ! node.enabled || node.sourceBlockId.isEmpty())
                    continue;
                if (auto* block = findBlock (node.sourceBlockId))
                    applyTypedNode (*block, node.kind);
                appliedNodes.add (nodeId);
                return;
            }
        };

        if (! edges.empty())
        {
            for (const auto& edge : edges)
            {
                if (! edge.enabled || edge.signalType != DspSignalType::audio)
                    continue;
                applyNodeById (edge.sourceNodeId);
                applyNodeById (edge.targetNodeId);
            }
            for (const auto& node : typedNodes)
                applyNodeById (node.id);
        }
        else
        {
            for (const auto& node : typedNodes)
            {
                if (! node.enabled || node.sourceBlockId.isEmpty())
                    continue;

                if (auto* block = findBlock (node.sourceBlockId))
                    applyTypedNode (*block, node.kind);
            }
        }

        if (typedNodes.empty())
        {
            for (auto& block : blocks)
                applyTypedNode (block, block.block.section == "mod" ? DspNodeKind::modulation
                                               : block.block.section == "source" ? DspNodeKind::source
                                               : block.block.section == "out" ? DspNodeKind::output
                                               : DspNodeKind::processor);
        }

        for (const auto& macro : macros)
        {
            auto* target = findParam (macro.targetId);
            auto* targetBlock = findBlock (macro.targetId);
            if (target == nullptr && targetBlock == nullptr)
                continue;

            float source = 0.5f;
            if (auto* sourceParam = findParam (macro.macroId))
                source = juce::jmap (sourceParam->current, sourceParam->min, sourceParam->max, 0.0f, 1.0f);
            else if (auto* sourceBlock = findBlock (macro.macroId))
            {
                const auto signal = blockSignal (*sourceBlock, context);
                source = juce::jlimit (0.0f, 1.0f, blockSignalIsUnipolar (sourceBlock->block)
                    ? signal
                    : signal * 0.5f + 0.5f);
            }

            const float sourceNorm = juce::jlimit (0.0f, 1.0f,
                (source - macro.sourceMin) / juce::jmax (0.0001f, macro.sourceMax - macro.sourceMin));
            const float curved = std::pow (sourceNorm, juce::jmax (0.05f, macro.curve));
            if (target != nullptr)
                target->routed = juce::jlimit (target->min, target->max,
                    juce::jmap (curved, 0.0f, 1.0f, macro.targetMin, macro.targetMax));
            else
                targetBlock->block.values["value"] = juce::jlimit (0.0f, 1.0f, curved);
        }

        for (auto& lane : automation)
        {
            auto* target = findParam (lane.lane.targetId);
            if (target == nullptr)
                continue;
            target->routed = juce::jlimit (target->min, target->max,
                juce::jmap (laneValue (lane, context), 0.0f, 1.0f, target->min, target->max));
        }

        for (const auto& route : modulation)
        {
            if (! route.enabled)
                continue;

            auto* target = findParam (route.targetId);
            auto* targetBlock = findBlock (route.targetId);
            if (target == nullptr && targetBlock == nullptr)
                continue;

            const float signal = sourceValue (route.sourceId, context);
            if (target != nullptr)
            {
                const float range = target->max - target->min;
                target->routed = juce::jlimit (target->min, target->max,
                    target->routed + signal * route.amount * range);
            }
            else
            {
                targetBlock->block.values["value"] = juce::jlimit (0.0f, 1.0f,
                    valueForKey (targetBlock->block, "value", 0.5f) + signal * route.amount);
            }
        }

        for (const auto& param : params)
            engine.setParameter (param.id, param.routed);
    }
}
