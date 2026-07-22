#include "CustomerPreviewOverlay.h"

#include "StudioMainComponent.h"
#include "PackRuntimeHost.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    CustomerPreviewOverlay::CustomerPreviewOverlay (StudioMainComponent& o)
        : owner (o)
    {
        setVisible (false);

        exitButton.setButtonText ("x");
        exitButton.getProperties().set ("primaryAction", true);
        exitButton.getProperties().set ("fontSize", 12.0);
        exitButton.getProperties().set ("bold", true);
        exitButton.setTooltip ("Exit Player preview (Esc).");
        exitButton.onClick = [this]
        {
            if (onExit)
                onExit();
        };
        addAndMakeVisible (exitButton);
        setWantsKeyboardFocus (true);
    }

    bool CustomerPreviewOverlay::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onExit)
                onExit();
            return true;
        }
        return juce::Component::keyPressed (key);
    }

    void CustomerPreviewOverlay::enterPreview()
    {
        active = true;
        setVisible (true);
        exitButton.setVisible (true);
        grabKeyboardFocus();

        if (auto* runtime = owner.getPackRuntime())
        {
            runtime->setPlayerChromeVisible (true);
            runtime->setStudioExitHandler ({});
            runtime->requestReloadImmediate();
            runtime->ensurePlaybackReady();
        }

        resized();
        toFront (true);
        exitButton.toFront (true);
        repaint();
    }

    void CustomerPreviewOverlay::exitPreview()
    {
        active = false;
        setVisible (false);
        exitButton.setVisible (false);

        if (auto* runtime = owner.getPackRuntime())
            runtime->setStudioExitHandler ({});
    }

    void CustomerPreviewOverlay::paint (juce::Graphics& g)
    {
        g.fillAll (owner.getProject().getManifest().playerBackgroundColour);
    }

    void CustomerPreviewOverlay::resized()
    {
        exitButton.setBounds (getLocalBounds().removeFromTop (42)
                                  .removeFromRight (42).reduced (6));

        if (auto* runtime = owner.getPackRuntime())
            runtime->attachToParent (this, getLocalBounds());

        exitButton.toFront (true);
    }

} // namespace patchcraft
