#pragma once
#include "DspModule.h"

namespace patchcraft
{
    class FilterModule : public AudioModule
    {
    public:
        FilterModule (const juce::String& moduleId) : id (moduleId) {}

        juce::String getId() const override { return id; }
        juce::String getType() const override { return "filter"; }
        bool isEnabled() const override { return enabled; }
        void setEnabled (bool e) override { enabled = e; }

        void prepare (const juce::dsp::ProcessSpec& spec) override
        {
            filter.prepare (spec);
            filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }

        void process (const juce::dsp::ProcessContextReplacing<float>& context) override
        {
            if (! enabled) return;

            const float res = juce::jlimit (0.05f, 1.5f, resonance * 1.5f + 0.05f);
            filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, cutoff));
            filter.setResonance (res);

            filter.process (context);
        }

        void reset() override
        {
            filter.reset();
        }

        void setParameter (const juce::String& paramId, float value) override
        {
            if (paramId == "cutoff") cutoff = value;
            else if (paramId == "resonance") resonance = value;
        }

        float getParameter (const juce::String& paramId) const override
        {
            if (paramId == "cutoff") return cutoff;
            if (paramId == "resonance") return resonance;
            return 0.0f;
        }

        juce::StringArray getParameterIds() const override
        {
            return { "cutoff", "resonance" };
        }

    private:
        juce::String id;
        bool enabled = true;
        float cutoff = 4200.0f;
        float resonance = 0.20f;

        juce::dsp::StateVariableTPTFilter<float> filter;
    };
}
