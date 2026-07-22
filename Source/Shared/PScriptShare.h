#pragma once

#include <juce_core/juce_core.h>

namespace patchcraft
{
    /** Helpers for sharing, binding, and merging portable pScript files. */
    namespace PScriptShare
    {
        bool isPscriptFile (const juce::File& file);

        /** Rewrite knob event headers so the script listens to the given control parameter id. */
        juce::String bindToKnobParameter (juce::String script,
                                          const juce::String& parameterId,
                                          const juce::String& knobLabel = {});

        juce::String mergeSources (const juce::StringArray& sections);
    };
}
