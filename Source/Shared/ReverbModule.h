#pragma once
#include "DspModule.h"

namespace patchcraft
{
    class ReverbModule : public AudioModule
    {
    public:
        ReverbModule (const juce::String& moduleId) : id (moduleId) {}

        juce::String getId() const override { return id; }
        juce::String getType() const override { return "reverb"; }
        bool isEnabled() const override { return enabled; }
        void setEnabled (bool e) override { enabled = e; }

        void prepare (const juce::dsp::ProcessSpec& spec) override
        {
            reverb.prepare (spec);
        }

        void process (const juce::dsp::ProcessContextReplacing<float>& context) override
        {
            if (! enabled) return;

            const float rvMix = juce::jlimit (0.0f, 1.0f, mix);
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.6f;
            rp.damping  = 0.4f;
            rp.wetLevel = rvMix * 0.6f;
            rp.dryLevel = 1.0f - rvMix * 0.5f;
            rp.width    = 1.0f;
            reverb.setParameters (rp);

            reverb.process (context);
        }

        void reset() override
        {
            reverb.reset();
        }

        void setParameter (const juce::String& paramId, float value) override
        {
            if (paramId == "mix") mix = value;
        }

        float getParameter (const juce::String& paramId) const override
        {
            if (paramId == "mix") return mix;
            return 0.0f;
        }

        juce::StringArray getParameterIds() const override
        {
            return { "mix" };
        }

    private:
        juce::String id;
        bool enabled = true;
        float mix = 0.0f;

        juce::dsp::Reverb reverb;
    };
}
