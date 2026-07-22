#include "TutorialModeOverlay.h"

#include "StudioMainComponent.h"
#include "InspectorPanel.h"
#include "PatchCraftLookAndFeel.h"
#include "TutorialHelp.h"

namespace patchcraft
{
    namespace
    {
        constexpr int kHoverDelayMs = 380;
        constexpr int kCalloutWidth = 360;
        constexpr int kMinCalloutHeight = 96;
        constexpr int kMaxCalloutHeight = 220;
    }

    TutorialModeOverlay::TutorialModeOverlay (StudioMainComponent& owner) : studio (owner)
    {
        setInterceptsMouseClicks (false, false);
        setAlwaysOnTop (true);
    }

    void TutorialModeOverlay::setActive (bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;

        active = shouldBeActive;
        hoverComponent = nullptr;
        hoverStartMs = 0;
        lastHelpSignature.clear();
        hideCallout();

        if (active)
            startTimerHz (20);
        else
            stopTimer();
    }

    void TutorialModeOverlay::applyInspectorLabelHelp()
    {
        // Inspector field names are labels; ensure they participate in hit-testing for hover help.
        if (auto* inspector = studio.findChildWithID ("inspectorPanel"))
        {
            for (int i = 0; i < inspector->getNumChildComponents(); ++i)
            {
                if (auto* label = dynamic_cast<juce::Label*> (inspector->getChildComponent (i)))
                {
                    if (const auto* entry = TutorialHelpRegistry::lookupForLabel (label->getText()))
                    {
                        label->setInterceptsMouseClicks (true, false);
                        TutorialHelp::attach (*label, entry->title, entry->body);
                    }
                }
            }
        }
    }

    void TutorialModeOverlay::paint (juce::Graphics& g)
    {
        if (! calloutVisible || calloutBounds.isEmpty())
            return;

        auto bounds = calloutBounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.expanded (1.5f), 10.0f);

        g.setColour (juce::Colour (0xff121820));
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.5f);

        auto content = calloutBounds.reduced (14, 12);
        g.setColour (PatchCraftLookAndFeel::accent());
        g.setFont (juce::FontOptions (13.5f).withStyle ("bold"));
        g.drawFittedText (calloutTitle, content.removeFromTop (22), juce::Justification::centredLeft, 1);

        content.removeFromTop (4);
        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (calloutBody, content, juce::Justification::topLeft, 12);

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("Tutorial Mode", calloutBounds.removeFromBottom (16).reduced (14, 0),
                    juce::Justification::centredRight);
    }

    void TutorialModeOverlay::resized()
    {
        setBounds (studio.getLocalBounds());
    }

    void TutorialModeOverlay::hideCallout()
    {
        if (! calloutVisible)
            return;

        calloutVisible = false;
        calloutTitle.clear();
        calloutBody.clear();
        calloutBounds = {};
        repaint();
    }

    void TutorialModeOverlay::showCallout (const juce::String& title, const juce::String& body,
                                           juce::Point<int> screenAnchor)
    {
        calloutTitle = title.isNotEmpty() ? title : "Help";
        calloutBody = body;
        calloutVisible = true;

        const int bodyHeight = juce::jlimit (48, kMaxCalloutHeight - 52,
                                           (int) std::ceil (calloutBody.length() / 42.0) * 18 + 24);
        const int height = juce::jlimit (kMinCalloutHeight, kMaxCalloutHeight, 52 + bodyHeight);

        auto localAnchor = studio.getLocalPoint (nullptr, screenAnchor);
        int x = localAnchor.x + 18;
        int y = localAnchor.y + 20;
        if (x + kCalloutWidth > getWidth() - 8)
            x = localAnchor.x - kCalloutWidth - 18;
        if (y + height > getHeight() - 8)
            y = localAnchor.y - height - 16;

        calloutBounds = { juce::jmax (8, x), juce::jmax (8, y), kCalloutWidth, height };
        toFront (false);
        repaint (calloutBounds.expanded (4));
    }

    void TutorialModeOverlay::timerCallback()
    {
        if (! active)
        {
            hideCallout();
            return;
        }

        const auto screenPos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        if (! studio.getScreenBounds().contains (screenPos))
        {
            hoverComponent = nullptr;
            hoverStartMs = 0;
            hideCallout();
            return;
        }

        const auto localPos = studio.getLocalPoint (nullptr, screenPos);
        auto* target = studio.getComponentAt (localPos);
        while (target != nullptr && (target == this || (bool) target->getProperties()["tutorialIgnore"]))
            target = target->getParentComponent();

        if (target == nullptr || target == this || ! TutorialHelp::isWithinStudioUi (target, studio))
        {
            hoverComponent = nullptr;
            hoverStartMs = 0;
            hideCallout();
            return;
        }

        const auto help = TutorialHelp::resolve (target);
        if (! help.isValid())
        {
            hoverComponent = nullptr;
            hoverStartMs = 0;
            lastHelpSignature.clear();
            hideCallout();
            return;
        }

        const auto signature = help.title + "\n" + help.body;
        const auto now = juce::Time::getMillisecondCounter();

        if (target != hoverComponent || signature != lastHelpSignature)
        {
            hoverComponent = target;
            lastHelpSignature = signature;
            hoverStartMs = now;
            hideCallout();
            return;
        }

        if (! calloutVisible && now - hoverStartMs >= (juce::uint32) kHoverDelayMs)
            showCallout (help.title, help.body, screenPos);
    }
}
