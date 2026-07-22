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

            const int type = juce::roundToInt (reverbType);
            if (type == 1) // Shimmer
            {
                rp.roomSize = 0.92f;
                rp.damping  = 0.15f;
                rp.width    = 1.0f;
            }
            else if (type == 3) // Spring
            {
                rp.roomSize = 0.35f;
                rp.damping  = 0.75f;
                rp.width    = 0.5f;
            }
            else // Large Room (default)
            {
                rp.roomSize = 0.65f;
                rp.damping  = 0.45f;
                rp.width    = 0.85f;
            }

            rp.wetLevel = rvMix * 0.6f;
            rp.dryLevel = 1.0f - rvMix * 0.5f;
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
            else if (paramId == "type") reverbType = value;
        }

        float getParameter (const juce::String& paramId) const override
        {
            if (paramId == "mix") return mix;
            if (paramId == "type") return reverbType;
            return 0.0f;
        }

        juce::StringArray getParameterIds() const override
        {
            return { "mix", "type" };
        }

    private:
        juce::String id;
        bool enabled = true;
        float mix = 0.0f;
        float reverbType = 2.0f;

        juce::dsp::Reverb reverb;
    };
}
