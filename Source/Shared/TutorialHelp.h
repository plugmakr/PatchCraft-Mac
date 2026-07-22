#pragma once

#include "TutorialHelpRegistry.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace patchcraft
{
    /** Resolves tutorial copy for any Studio UI component. */
    class TutorialHelp
    {
    public:
        struct Resolved
        {
            juce::String title;
            juce::String body;
            bool isValid() const noexcept { return body.isNotEmpty(); }
        };

        static void attach (juce::Component& component, const juce::String& tutorialId);
        static void attach (juce::Component& component, const juce::String& title, const juce::String& body);

        static Resolved resolve (const juce::Component* target);
        static bool isWithinStudioUi (const juce::Component* target, const juce::Component& studioRoot);
    };
}
