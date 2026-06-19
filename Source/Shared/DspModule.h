#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace patchcraft
{
    /** Base class for all DSP modules (both audio processors and modulators) */
    class DspModule
    {
    public:
        virtual ~DspModule() = default;

        virtual juce::String getId() const = 0;
        virtual juce::String getType() const = 0;
        virtual bool isEnabled() const = 0;
        virtual void setEnabled (bool shouldBeEnabled) = 0;

        // Parameter interface for automation and UI mapping
        virtual void setParameter (const juce::String& paramId, float value) = 0;
        virtual float getParameter (const juce::String& paramId) const = 0;
        virtual juce::StringArray getParameterIds() const = 0;
    };

    /** Modules that process or generate audio signals in the serial chain */
    class AudioModule : public DspModule
    {
    public:
        virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
        virtual void process (const juce::dsp::ProcessContextReplacing<float>& context) = 0;
        virtual void reset() = 0;
    };

    /** Modules that output modulation values (LFOs, Envelopes) */
    class ModulatorModule : public DspModule
    {
    public:
        virtual void prepare (double sampleRate) = 0;
        virtual void trigger() = 0;
        virtual float getNextValue() = 0;
    };
}
