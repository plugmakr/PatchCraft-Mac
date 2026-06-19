#pragma once
#include "DspModule.h"

namespace patchcraft
{
    class DelayModule : public AudioModule
    {
    public:
        DelayModule (const juce::String& moduleId) : id (moduleId) {}

        juce::String getId() const override { return id; }
        juce::String getType() const override { return "delay"; }
        bool isEnabled() const override { return enabled; }
        void setEnabled (bool e) override { enabled = e; }

        void prepare (const juce::dsp::ProcessSpec& spec) override
        {
            sampleRate = spec.sampleRate;
            delayL.prepare (spec);
            delayR.prepare (spec);
            delayL.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
            delayR.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
        }

        void process (const juce::dsp::ProcessContextReplacing<float>& context) override
        {
            if (! enabled) return;

            auto& inputBlock = context.getInputBlock();
            auto& outputBlock = context.getOutputBlock();
            const int numSamples = (int) inputBlock.getNumSamples();
            const int numChannels = (int) inputBlock.getNumChannels();

            if (numSamples <= 0 || numChannels <= 0) return;

            auto* inputL = inputBlock.getChannelPointer (0);
            auto* inputR = numChannels > 1 ? inputBlock.getChannelPointer (1) : inputL;

            auto* outputL = outputBlock.getChannelPointer (0);
            auto* outputR = numChannels > 1 ? outputBlock.getChannelPointer (1) : outputL;

            const float dTime = juce::jlimit (0.0f, 2.0f, delayTime);
            const float dFb   = juce::jlimit (0.0f, 0.95f, feedback);
            const float dMix  = juce::jlimit (0.0f, 1.0f, mix);
            const int dSamps  = juce::jmax (1, (int) (dTime * sampleRate));

            delayL.setDelay ((float) dSamps);
            delayR.setDelay ((float) dSamps);

            for (int i = 0; i < numSamples; ++i)
            {
                const float dl = delayL.popSample (0);
                const float dr = delayR.popSample (0);
                delayL.pushSample (0, inputL[i] + dr * dFb);
                delayR.pushSample (0, inputR[i] + dl * dFb);
                outputL[i] = inputL[i] * (1.0f - dMix * 0.5f) + dl * dMix;
                if (numChannels > 1)
                    outputR[i] = inputR[i] * (1.0f - dMix * 0.5f) + dr * dMix;
            }
        }

        void reset() override
        {
            delayL.reset();
            delayR.reset();
        }

        void setParameter (const juce::String& paramId, float value) override
        {
            if (paramId == "time") delayTime = value;
            else if (paramId == "feedback") feedback = value;
            else if (paramId == "mix") mix = value;
        }

        float getParameter (const juce::String& paramId) const override
        {
            if (paramId == "time") return delayTime;
            if (paramId == "feedback") return feedback;
            if (paramId == "mix") return mix;
            return 0.0f;
        }

        juce::StringArray getParameterIds() const override
        {
            return { "time", "feedback", "mix" };
        }

    private:
        juce::String id;
        bool enabled = true;
        float delayTime = 0.3f;
        float feedback = 0.4f;
        float mix = 0.0f;
        double sampleRate = 44100.0;

        juce::dsp::DelayLine<float> delayL { 96000 };
        juce::dsp::DelayLine<float> delayR { 96000 };
    };
}
