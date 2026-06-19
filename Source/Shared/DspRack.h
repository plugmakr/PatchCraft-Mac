#pragma once
#include "DspModule.h"
#include "DelayModule.h"
#include "ReverbModule.h"
#include "FilterModule.h"
#include <vector>
#include <memory>

namespace patchcraft
{
    class LfoModulator : public ModulatorModule
    {
    public:
        LfoModulator (const juce::String& moduleId) : id (moduleId) {}

        juce::String getId() const override { return id; }
        juce::String getType() const override { return "lfo"; }
        bool isEnabled() const override { return enabled; }
        void setEnabled (bool e) override { enabled = e; }

        void prepare (double sr) override
        {
            sampleRate = sr;
            phase = 0.0;
        }

        void trigger() override
        {
            phase = 0.0;
        }

        float getNextValue() override
        {
            if (! enabled) return 0.0f;

            const double rateHz = juce::jmax (0.05f, rate);
            const double phaseInc = juce::MathConstants<double>::twoPi * rateHz / sampleRate;
            
            float value = (float) std::sin (phase) * amount;
            
            phase += phaseInc;
            if (phase > juce::MathConstants<double>::twoPi)
                phase = std::fmod (phase, juce::MathConstants<double>::twoPi);
                
            return value;
        }

        void setParameter (const juce::String& paramId, float value) override
        {
            if (paramId == "rate") rate = value;
            else if (paramId == "amount") amount = value;
        }

        float getParameter (const juce::String& paramId) const override
        {
            if (paramId == "rate") return rate;
            if (paramId == "amount") return amount;
            return 0.0f;
        }

        juce::StringArray getParameterIds() const override
        {
            return { "rate", "amount" };
        }

    private:
        juce::String id;
        bool enabled = true;
        float rate = 4.0f;
        float amount = 1.0f;
        double phase = 0.0;
        double sampleRate = 44100.0;
    };

    struct ModulationRoute
    {
        juce::String sourceModulatorId;
        juce::String targetModuleId;
        juce::String targetParamId;
        float depth = 1.0f;
        bool enabled = true;
    };

    class DspRack
    {
    public:
        DspRack() = default;

        void addAudioModule (std::unique_ptr<AudioModule> module)
        {
            audioChain.push_back (std::move (module));
        }

        void addModulator (std::unique_ptr<ModulatorModule> modulator)
        {
            modulators.push_back (std::move (modulator));
        }

        void addModulationRoute (const ModulationRoute& route)
        {
            routes.push_back (route);
        }

        void clear()
        {
            audioChain.clear();
            modulators.clear();
            routes.clear();
        }

        const std::vector<std::unique_ptr<AudioModule>>& getAudioChain() const { return audioChain; }
        const std::vector<std::unique_ptr<ModulatorModule>>& getModulators() const { return modulators; }
        const std::vector<ModulationRoute>& getModulationRoutes() const { return routes; }

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            for (auto& mod : modulators)
                mod->prepare (spec.sampleRate);

            for (auto& fx : audioChain)
                fx->prepare (spec);
        }

        void reset()
        {
            for (auto& mod : modulators)
                mod->trigger();

            for (auto& fx : audioChain)
                fx->reset();
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numSamples = buffer.getNumSamples();
            if (numSamples <= 0) return;

            // 1. Process modulation: store parameters before modulation to prevent drift
            std::vector<float> baseValues;
            baseValues.reserve (routes.size());

            for (const auto& route : routes)
            {
                if (! route.enabled)
                {
                    baseValues.push_back (0.0f);
                    continue;
                }

                auto* dest = findModule (route.targetModuleId);
                if (dest != nullptr)
                {
                    baseValues.push_back (dest->getParameter (route.targetParamId));
                }
                else
                {
                    baseValues.push_back (0.0f);
                }
            }

            for (size_t i = 0; i < routes.size(); ++i)
            {
                const auto& route = routes[i];
                if (! route.enabled) continue;

                auto* src = findModulator (route.sourceModulatorId);
                auto* dest = findModule (route.targetModuleId);
                if (src != nullptr && dest != nullptr)
                {
                    float modVal = src->getNextValue();
                    float baseVal = baseValues[i];
                    dest->setParameter (route.targetParamId, baseVal + (modVal * route.depth));
                }
            }

            // 2. Process audio serially through the FX chain
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> context (block);

            for (auto& fx : audioChain)
            {
                if (fx->isEnabled())
                {
                    fx->process (context);
                }
            }

            // 3. Restore base parameter values
            for (size_t i = 0; i < routes.size(); ++i)
            {
                const auto& route = routes[i];
                if (! route.enabled) continue;

                auto* dest = findModule (route.targetModuleId);
                if (dest != nullptr)
                {
                    dest->setParameter (route.targetParamId, baseValues[i]);
                }
            }
        }

    private:
        DspModule* findModule (const juce::String& id)
        {
            for (auto& fx : audioChain)
                if (fx->getId() == id) return fx.get();
            for (auto& mod : modulators)
                if (mod->getId() == id) return mod.get();
            return nullptr;
        }

        ModulatorModule* findModulator (const juce::String& id)
        {
            for (auto& mod : modulators)
                if (mod->getId() == id) return mod.get();
            return nullptr;
        }

        std::vector<std::unique_ptr<AudioModule>> audioChain;
        std::vector<std::unique_ptr<ModulatorModule>> modulators;
        std::vector<ModulationRoute> routes;
    };
}
