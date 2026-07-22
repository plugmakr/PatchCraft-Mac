#pragma once

#include <juce_core/juce_core.h>

namespace patchcraft
{
    struct TutorialHelpEntry
    {
        juce::String title;
        juce::String body;
    };

    /** Static help copy keyed by tutorial id or normalized label text. */
    class TutorialHelpRegistry
    {
    public:
        static const TutorialHelpEntry* lookup (const juce::String& key);
        static const TutorialHelpEntry* lookupForLabel (const juce::String& labelText);
        static const TutorialHelpEntry* lookupForButton (const juce::String& buttonText);
    };
}
