#include "PackRuntimeHost.h"

#include "StudioMainComponent.h"
#include "../Player/PluginEditor.h"
#include "PatchCraftLookAndFeel.h"
#include "PatchCraftPackWriter.h"

namespace patchcraft
{
    PackRuntimeHost::PackRuntimeHost (StudioMainComponent& o)
        : owner (o)
    {
        processor = std::make_unique<PlayerProcessor> (true);
        playerEditor.reset (processor->createEditor());
        if (playerEditor != nullptr)
            addAndMakeVisible (*playerEditor);

        owner.getProject().getLiveValues().addListener (this);

        statusLabel.setJustificationType (juce::Justification::centred);
        statusLabel.setFont (juce::FontOptions (13.0f));
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addChildComponent (statusLabel);

        requestReload();
        startTimerHz (30);
    }

    PackRuntimeHost::~PackRuntimeHost()
    {
        stopTimer();
        owner.getProject().getLiveValues().removeListener (this);
        deactivate();
        playerEditor.reset();
        processor.reset();
    }

    void PackRuntimeHost::attachToParent (juce::Component* parent, juce::Rectangle<int> area)
    {
        if (parent == nullptr)
            return;

        if (getParentComponent() != parent)
        {
            if (auto* current = getParentComponent())
                current->removeChildComponent (this);

            parent->addAndMakeVisible (this);
        }

        setBounds (area);
        toFront (false);
        resized();
    }

    void PackRuntimeHost::requestReload()
    {
        reloadPending = true;
        reloadDebounceMs = 200;
    }

    void PackRuntimeHost::requestReloadImmediate()
    {
        reloadPending = true;
        reloadDebounceMs = 0;
    }

    void PackRuntimeHost::ensurePlaybackReady()
    {
        // Only block for a sync rebuild when there is no pack to play yet.
        if (reloadPending && (processor == nullptr || processor->getPack() == nullptr))
            reloadPack();

        if (! audioRunning)
            activate();
    }

    void PackRuntimeHost::previewNoteOn (int note, float velocity)
    {
        ensurePlaybackReady();
        if (processor != nullptr)
            processor->handleNoteOn (note, velocity);
    }

    void PackRuntimeHost::previewNoteOff (int note)
    {
        if (processor != nullptr)
            processor->handleNoteOff (note);
    }

    void PackRuntimeHost::setStudioExitHandler (std::function<void()> /*handler*/)
    {
    }



    void PackRuntimeHost::syncLiveValuesToProcessor()
    {
        if (processor == nullptr || ! processor->isPackLoaded())
            return;

        for (const auto& def : owner.getProject().getParameters().getAll())
        {
            const auto value = owner.getProject().getLiveValues().getValue (def.id, def.defaultValue);
            processor->setPackParameterFromUi (def.id, value);
        }
    }

    void PackRuntimeHost::liveValueChanged (const juce::String& parameterId, float newValue)
    {
        owner.getProject().syncDspGraphFromLiveValues();
        if (processor != nullptr)
            processor->setPackParameterFromUi (parameterId, newValue);
    }

    void PackRuntimeHost::timerCallback()
    {
        if (reloadPending && reloadDebounceMs > 0)
        {
            reloadDebounceMs -= juce::roundToInt (1000.0f / 30.0f);
            if (reloadDebounceMs <= 0)
            {
                reloadPending = false;
                reloadPack();
            }
        }
    }

    void PackRuntimeHost::paint (juce::Graphics& g)
    {
        g.fillAll (owner.getProject().getManifest().playerBackgroundColour);
    }

    void PackRuntimeHost::setStatus (const juce::String& message, bool warning)
    {
        juce::String display = message.trim();
        if (display.isNotEmpty())
        {
            const auto lines = juce::StringArray::fromLines (display);
            if (lines.size() > 3)
                display = lines.joinIntoString ("\n", 0, 3)
                        + "\n... (" + juce::String (lines.size() - 3) + " more — fix in Ship before export)";
        }

        statusLabel.setText (display, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId,
                               warning ? juce::Colour (0xffe6504a) : PatchCraftLookAndFeel::textDim());
        statusLabel.setVisible (display.isNotEmpty());
        if (display.isNotEmpty())
            statusLabel.toFront (false);
        resized();
    }

    void PackRuntimeHost::resized()
    {
        auto area = getLocalBounds();

        if (playerEditor != nullptr)
            playerEditor->setBounds (area);

        if (statusLabel.isVisible())
        {
            auto statusArea = area.removeFromBottom (juce::jmin (96, juce::jmax (48, getHeight() / 6)));
            statusLabel.setBounds (statusArea.reduced (12, 4));
            statusLabel.setJustificationType (juce::Justification::topLeft);
        }
    }

    bool PackRuntimeHost::exportPreviewPack (juce::String& errorOut)
    {
        const auto& project = owner.getProject();
        previewPackFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("pack-runtime-preview")
            .getChildFile (juce::String::toHexString (project.getProjectFolder().getFullPathName().hashCode64()));

        if (previewPackFolder.exists())
            previewPackFolder.deleteRecursively();

        previewPackFolder.createDirectory();
        PatchCraftPackWriter writer;
        PackWriteOptions options;
        options.strictReferenceValidation = false;
        options.exportForPreview = true;
        return writer.write (project, previewPackFolder, errorOut, options);
    }

    bool PackRuntimeHost::loadProjectPreviewPack()
    {
        if (processor == nullptr)
            return false;

        juce::String error;
        if (! exportPreviewPack (error))
        {
            setStatus ("Preview export failed: " + error, true);
            return false;
        }

        if (! processor->loadPack (previewPackFolder, error))
        {
            setStatus ("Audio engine could not load pack: " + error, true);
            return false;
        }

        syncLiveValuesToProcessor();
        if (playerEditor != nullptr)
            playerEditor->repaint();
        setStatus ({});
        return true;
    }

    void PackRuntimeHost::attachAudioAndMidi()
    {
        auto& deviceManager = owner.getAudio().getDeviceManager();

        if (auto* device = deviceManager.getCurrentAudioDevice())
            processor->prepareToPlay (device->getCurrentSampleRate(),
                                      device->getCurrentBufferSizeSamples());

        audioPlayer.setProcessor (processor.get());
        deviceManager.addAudioCallback (&audioPlayer);
        deviceManager.addMidiInputDeviceCallback ({}, &audioPlayer);
    }

    void PackRuntimeHost::detachAudioAndMidi()
    {
        auto& deviceManager = owner.getAudio().getDeviceManager();
        deviceManager.removeMidiInputDeviceCallback ({}, &audioPlayer);
        deviceManager.removeAudioCallback (&audioPlayer);
        audioPlayer.setProcessor (nullptr);
    }

    void PackRuntimeHost::reloadPack()
    {
        reloadPending = false;
        reloadDebounceMs = 0;

        if (processor == nullptr)
            return;

        const bool wasRunning = audioRunning;
        if (wasRunning)
            detachAudioAndMidi();

        loadProjectPreviewPack();

        if (wasRunning && processor->getPack() != nullptr)
            attachAudioAndMidi();

        resized();
        repaint();
    }

    void PackRuntimeHost::activate()
    {
        if (processor == nullptr)
            return;

        // Never stall tab switches on a full preview export when a pack is already
        // loaded — the timer debounce will apply pending edits shortly.
        if (processor->getPack() == nullptr)
        {
            reloadPack();
            if (processor->getPack() == nullptr)
            {
                setStatus ("Save your project and ensure Sound + Layout are set up before previewing audio.", true);
                return;
            }
        }
        else
        {
            syncLiveValuesToProcessor();
        }

        if (audioRunning)
            return;

        juce::String error;
        if (! owner.getAudio().ensureOpen (error))
        {
            setStatus ("Audio unavailable: " + error, true);
            return;
        }

        auto& deviceManager = owner.getAudio().getDeviceManager();
        for (const auto& input : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

        processor->setNonRealtime (false);
        attachAudioAndMidi();
        audioRunning = true;
        setStatus ({});
        resized();
    }

    void PackRuntimeHost::deactivate()
    {
        if (! audioRunning)
            return;

        detachAudioAndMidi();

        if (processor != nullptr)
            processor->releaseResources();

        audioRunning = false;
    }

    bool PackRuntimeHost::importUserContent (const juce::StringArray& paths, juce::String& report)
    {
        if (processor == nullptr)
            return false;

        return processor->importUserContentFiles (paths, {}, report);
    }

} // namespace patchcraft
