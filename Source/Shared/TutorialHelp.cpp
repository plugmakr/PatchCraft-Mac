#include "TutorialHelp.h"

namespace patchcraft
{
    void TutorialHelp::attach (juce::Component& component, const juce::String& tutorialId)
    {
        component.getProperties().set ("tutorialId", tutorialId);
    }

    void TutorialHelp::attach (juce::Component& component, const juce::String& title, const juce::String& body)
    {
        component.getProperties().set ("tutorialTitle", title);
        component.getProperties().set ("tutorialBody", body);
    }

    TutorialHelp::Resolved TutorialHelp::resolve (const juce::Component* target)
    {
        Resolved resolved;
        if (target == nullptr)
            return resolved;

        for (auto* component = target; component != nullptr; component = component->getParentComponent())
        {
            if (auto title = component->getProperties()["tutorialTitle"]; title.isString())
                if (auto body = component->getProperties()["tutorialBody"]; body.isString() && body.toString().isNotEmpty())
                {
                    Resolved out;
                    out.title = title.toString();
                    out.body = body.toString();
                    return out;
                }

            if (auto id = component->getProperties()["tutorialId"]; id.isString())
                if (const auto* entry = TutorialHelpRegistry::lookup (id.toString()))
                {
                    Resolved out;
                    out.title = entry->title;
                    out.body = entry->body;
                    return out;
                }

            if (auto help = component->getProperties()["tutorialHelp"]; help.isString() && help.toString().isNotEmpty())
            {
                const auto text = help.toString();
                const int breakIndex = text.indexOfChar ('\n');
                Resolved out;
                if (breakIndex > 0)
                {
                    out.title = text.substring (0, breakIndex).trim();
                    out.body = text.substring (breakIndex + 1).trim();
                }
                else
                {
                    out.title = component->getName().isNotEmpty() ? component->getName() : "Help";
                    out.body = text;
                }
                return out;
            }

            if (auto* tooltipClient = dynamic_cast<juce::SettableTooltipClient*> (const_cast<juce::Component*> (component)))
            {
                const auto tip = tooltipClient->getTooltip().trim();
                if (tip.isNotEmpty())
                {
                    const int breakIndex = tip.indexOfChar ('\n');
                    if (breakIndex > 0)
                    {
                        Resolved out;
                        out.title = tip.substring (0, breakIndex).trim();
                        out.body = tip.substring (breakIndex + 1).trim();
                        return out;
                    }

                    if (auto* label = dynamic_cast<const juce::Label*> (component))
                    {
                        Resolved out;
                        out.title = label->getText().trim();
                        out.body = tip;
                        return out;
                    }

                    if (auto* button = dynamic_cast<const juce::TextButton*> (component))
                    {
                        Resolved out;
                        out.title = button->getButtonText().trim();
                        out.body = tip;
                        return out;
                    }

                    Resolved out;
                    out.title = component->getName().isNotEmpty() ? component->getName() : "Help";
                    out.body = tip;
                    return out;
                }
            }

            if (auto* label = dynamic_cast<const juce::Label*> (component))
                if (const auto* entry = TutorialHelpRegistry::lookupForLabel (label->getText()))
                {
                    Resolved out;
                    out.title = entry->title;
                    out.body = entry->body;
                    return out;
                }

            if (auto* button = dynamic_cast<const juce::TextButton*> (component))
                if (const auto* entry = TutorialHelpRegistry::lookupForButton (button->getButtonText()))
                {
                    Resolved out;
                    out.title = entry->title;
                    out.body = entry->body;
                    return out;
                }
        }

        return resolved;
    }

    bool TutorialHelp::isWithinStudioUi (const juce::Component* target, const juce::Component& studioRoot)
    {
        for (auto* component = target; component != nullptr; component = component->getParentComponent())
            if (component == &studioRoot)
                return true;
        return false;
    }
}
