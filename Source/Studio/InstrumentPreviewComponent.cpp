#include "InstrumentPreviewComponent.h"
#include "PatchCraftLookAndFeel.h"
#include "EngineFactory.h"
#include "DebugLog.h"

namespace patchcraft
{
    InstrumentPreviewComponent::InstrumentPreviewComponent (PatchCraftProject& p,
                                                            AssetManager& a,
                                                            juce::AudioDeviceManager& dm)
        : project (p), assets (a), deviceManager (dm)
    {
        keyboard = std::make_unique<juce::MidiKeyboardComponent> (
            keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
        keyboard->setAvailableRange (24, 108);
        keyboard->setLowestVisibleKey (36);
        keyboard->setKeyWidth (16.0f);
        keyboard->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9d8b8));
        keyboard->setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff141413));
        keyboard->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                             PatchCraftLookAndFeel::accent().withAlpha (0.5f));
        keyboard->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,
                             juce::Colour (0xff8a7958));
        addAndMakeVisible (*keyboard);

        headerLabel.setText ("PREVIEW", juce::dontSendNotification);
        headerLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        headerLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (headerLabel);

        statusLabel.setText ("Stopped", juce::dontSendNotification);
        statusLabel.setFont (juce::Font (11.0f));
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (statusLabel);

        project.getLiveValues().addListener (this);
        project.addListener (this);
    }

    InstrumentPreviewComponent::~InstrumentPreviewComponent()
    {
        stopTimer();
        project.removeListener (this);
        project.getLiveValues().removeListener (this);
        stop();
    }

    void InstrumentPreviewComponent::projectChanged()
    {
        projectChanged (PatchCraftProject::ChangeScope::structural);
    }

    void InstrumentPreviewComponent::projectChanged (PatchCraftProject::ChangeScope scope)
    {
        // Auto-rebuild engine if the project's engine type changed.
        if (active && engine != nullptr && project.getEngineType() != engineId)
        {
            {
                const juce::SpinLock::ScopedLockType lk (engineLock);
                arpeggiator.allNotesOff (*engine);
            }
            ensureEngineMatches();
            syncAllValuesToEngine();
            refreshPlaybackStatus();
            return;
        }

        // Reload samples if preview is active and engine exists
        // loadFromPack is now lock-free, safe to call from message thread
        if (active && engine != nullptr)
        {
            {
                const juce::SpinLock::ScopedLockType lk (engineLock);
                syncRoutingFromProject (scope == PatchCraftProject::ChangeScope::dspRealtime);
            }
            if (scope != PatchCraftProject::ChangeScope::dspRealtime)
                engine->loadFromPack (project.getProjectFolder(), project.getSampleMap());
            refreshPlaybackStatus();
        }
    }

    void InstrumentPreviewComponent::ensureEngineMatches()
    {
        const auto wanted = project.getEngineType();
        if (engine != nullptr && wanted == engineId)
        {
            // Engine type matches, but reload samples in case they changed
            // loadFromPack is now lock-free
            engine->loadFromPack (project.getProjectFolder(), project.getSampleMap());
            return;
        }

        auto newEngine = createEngineFromManifest (wanted);
        newEngine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
        newEngine->setRenderContext (RenderContext::forBlock (currentSampleRate,
                                                              currentBlockSize,
                                                              currentBlockSize,
                                                              0,
                                                              currentNumChans,
                                                              120.0));
        newEngine->loadFromPack (project.getProjectFolder(), project.getSampleMap());

        const juce::SpinLock::ScopedLockType lk (engineLock);
        engine = std::move (newEngine);
        engineId = wanted;
        syncRoutingFromProject();
    }

    void InstrumentPreviewComponent::start()
    {
        if (active) return;

        ensureEngineMatches();
        syncRoutingFromProject();
        syncAllValuesToEngine();

        // Make sure SOMETHING is open. The shared service tries to use the
        // user's saved choice on app start; only init defaults as a fallback.
        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            const auto err = deviceManager.initialiseWithDefaultDevices (0, 2);
            if (err.isNotEmpty()) { DBG ("Preview start: " << err); }
        }

        deviceManager.addAudioCallback (this);

        for (auto& src : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (src.identifier, true);
        deviceManager.addMidiInputDeviceCallback ({}, this);

        active = true;
        startTimerHz (4);

        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            refreshPlaybackStatus();
        }
        else
        {
            statusLabel.setText ("No audio device - open Settings to choose one",
                                 juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6504a));
        }
    }

    void InstrumentPreviewComponent::stop()
    {
        if (! active) return;
        deviceManager.removeMidiInputDeviceCallback ({}, this);
        deviceManager.removeAudioCallback (this);
        stopTimer();
        {
            const juce::ScopedLock guard (hardwareMidiLock);
            hardwareMidiBuffer.clear();
        }
        keyboardState.allNotesOff (0);
        // Don't close the device - keep it warm for the next preview, and the
        // settings dialog needs it open to display the current configuration.
        if (engine)
        {
            arpeggiator.allNotesOff (*engine);
            engine->allNotesOff();
        }
        active = false;
        statusLabel.setText ("Stopped", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
    }

    void InstrumentPreviewComponent::syncAllValuesToEngine()
    {
        if (engine == nullptr) return;
        for (auto& def : project.getParameters().getAll())
        {
            const float v = project.getLiveValues().getValue (def.id, def.defaultValue);
            engine->setParameter (def.id, v);
            routingEngine.setParameterValue (def.id, v);
            routingEngine.setFxBlockParameterValue (def.id, v);
        }
    }

    void InstrumentPreviewComponent::syncRoutingFromProject (bool preserveActiveNotes)
    {
        if (! preserveActiveNotes && engine != nullptr)
            arpeggiator.allNotesOff (*engine);
        arpeggiator.bind (project.getDspGraph());
        routingEngine.bind (project.getDspGraph(), project.getParameters());
        routingEngine.prepare (RenderContext::forBlock (currentSampleRate,
                                                        currentBlockSize,
                                                        currentBlockSize,
                                                        0,
                                                        currentNumChans,
                                                        120.0));
        routingEngine.syncFromLiveValues (project.getLiveValues());
    }

    juce::String InstrumentPreviewComponent::makePlaybackStatusText()
    {
        juce::String prefix = "Playing";
        if (auto* dev = deviceManager.getCurrentAudioDevice())
            prefix << " through " << dev->getName()
                   << juce::String::formatted ("   %.0f Hz / %d samples",
                                               dev->getCurrentSampleRate(),
                                               dev->getCurrentBufferSizeSamples());

        const auto projectEngine = project.getEngineType();
        const auto zoneCount = (int) project.getSampleMap().getZones().size();
        if (zoneCount > 0 && projectEngine != "sample")
            return prefix + " | Engine mismatch: Sample Mapper has "
                 + juce::String (zoneCount) + " zone(s), but project engine is "
                 + projectEngine + ". Switch engine to Sampler.";

        juce::String diagnostic;
        {
            const juce::SpinLock::ScopedTryLockType lk (engineLock);
            if (lk.isLocked() && engine != nullptr)
                diagnostic = engine->getDiagnosticStatus();
        }

        if (diagnostic.isNotEmpty())
            return prefix + " | " + diagnostic;

        return prefix + " | Engine: " + (projectEngine.isNotEmpty() ? projectEngine : "synth");
    }

    void InstrumentPreviewComponent::refreshPlaybackStatus()
    {
        if (! active)
        {
            statusLabel.setText ("Stopped", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            return;
        }

        const auto text = makePlaybackStatusText();
        statusLabel.setText (text, juce::dontSendNotification);
        const bool warning = text.containsIgnoreCase ("mismatch")
                          || text.containsIgnoreCase ("missing")
                          || text.containsIgnoreCase ("failed")
                          || text.containsIgnoreCase ("no zone")
                          || text.containsIgnoreCase ("0/");
        statusLabel.setColour (juce::Label::textColourId,
                               warning ? juce::Colour (0xffffc857)
                                       : PatchCraftLookAndFeel::accent());
    }

    void InstrumentPreviewComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());

        // Top header strip
        auto top = getLocalBounds().removeFromTop (32);
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRect (top);
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (top.removeFromBottom (1));
    }

    void InstrumentPreviewComponent::resized()
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (32).reduced (10, 6);
        headerLabel.setBounds (top.removeFromLeft (80));
        statusLabel.setBounds (top);

        keyboard->setBounds (r.reduced (4));
    }

    void InstrumentPreviewComponent::audioDeviceAboutToStart (juce::AudioIODevice* d)
    {
        currentSampleRate = d->getCurrentSampleRate();
        currentBlockSize  = d->getCurrentBufferSizeSamples();
        currentNumChans   = 2;
        if (engine != nullptr)
        {
            engine->prepare (currentSampleRate, currentBlockSize, currentNumChans);
            engine->setRenderContext (RenderContext::forBlock (currentSampleRate,
                                                               currentBlockSize,
                                                               currentBlockSize,
                                                               d->getActiveInputChannels().countNumberOfSetBits(),
                                                               currentNumChans,
                                                               120.0));
        }
        routingEngine.prepare (RenderContext::forBlock (currentSampleRate,
                                                        currentBlockSize,
                                                        currentBlockSize,
                                                        d->getActiveInputChannels().countNumberOfSetBits(),
                                                        currentNumChans,
                                                        120.0));
    }

    void InstrumentPreviewComponent::audioDeviceStopped()
    {
        if (engine)
        {
            arpeggiator.allNotesOff (*engine);
            engine->reset();
        }
    }

    void InstrumentPreviewComponent::timerCallback()
    {
        // Auto-enable MIDI inputs that appeared after the preview started so a
        // keyboard connected mid-session begins triggering without a restart.
        if (active)
        {
            for (auto& src : juce::MidiInput::getAvailableDevices())
                if (! deviceManager.isMidiInputDeviceEnabled (src.identifier))
                    deviceManager.setMidiInputDeviceEnabled (src.identifier, true);
        }

        refreshPlaybackStatus();
    }

    void InstrumentPreviewComponent::audioDeviceIOCallbackWithContext (
        const float* const* inputs, int numInputs,
        float* const* outputs, int numOutputs, int numSamples,
        const juce::AudioIODeviceCallbackContext&)
    {
        if (outputs == nullptr || numOutputs <= 0) return;
        juce::AudioBuffer<float> out (outputs, numOutputs, numSamples);
        out.clear();

        // For FX engines, copy any audio inputs into the output buffer first
        // so the engine can process them in-place.
        const juce::SpinLock::ScopedTryLockType lk (engineLock);
        if (! lk.isLocked() || engine == nullptr) return;

        if (engine->needsAudioInput() && inputs != nullptr)
        {
            for (int ch = 0; ch < juce::jmin (numOutputs, numInputs); ++ch)
                if (inputs[ch] != nullptr)
                    juce::FloatVectorOperations::copy (
                        out.getWritePointer (ch), inputs[ch], numSamples);
        }

        // Drain MIDI events from the on-screen keyboard / external MIDI input.
        juce::MidiBuffer midi;
        keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
        {
            // Merge hardware MIDI staged on the MIDI thread (see
            // handleIncomingMidiMessage) so external keyboards trigger the engine.
            const juce::ScopedLock guard (hardwareMidiLock);
            midi.addEvents (hardwareMidiBuffer, 0, numSamples, 0);
            hardwareMidiBuffer.clear();
        }
        int midiCount = 0;
        for (auto md : midi)
        {
            midiCount++;
            const auto m = md.getMessage();
            if (m.isNoteOn())
            {
                PC_DBG("[AUDIO] NoteOn from keyboardState: note=%d vel=%.2f", m.getNoteNumber(), m.getFloatVelocity());
                if (! arpeggiator.handleNoteOn (*engine, m.getNoteNumber(), m.getFloatVelocity()))
                    engine->noteOn  (m.getNoteNumber(), m.getFloatVelocity());
            }
            if (m.isNoteOff())
            {
                PC_DBG("[AUDIO] NoteOff from keyboardState: note=%d", m.getNoteNumber());
                if (! arpeggiator.handleNoteOff (*engine, m.getNoteNumber()))
                    engine->noteOff (m.getNoteNumber());
            }
            if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                arpeggiator.allNotesOff (*engine);
                engine->allNotesOff();
            }
        }

        auto context = RenderContext::forBlock (currentSampleRate,
                                               numSamples,
                                               currentBlockSize,
                                               numInputs,
                                               numOutputs,
                                               120.0);
        routingEngine.processToEngine (*engine, context);
        arpeggiator.process (*engine, context);
        engine->process (out, 0, numSamples);
    }

    void InstrumentPreviewComponent::handleIncomingMidiMessage (juce::MidiInput* source,
                                                                const juce::MidiMessage& m)
    {
        PC_DBG("[MIDI IN] from %s: %s", 
               source ? source->getName().toStdString().c_str() : "unknown",
               m.getDescription().toStdString().c_str());

        // Update the on-screen keyboard's visual state (reflects held notes).
        keyboardState.processNextMidiEvent (m);

        // Stage note events for the audio thread. processNextMidiEvent does NOT
        // forward these to processNextMidiBuffer, so without this the engine
        // never hears the hardware keyboard.
        if (m.isNoteOn() || m.isNoteOff() || m.isAllNotesOff() || m.isAllSoundOff())
        {
            const juce::ScopedLock guard (hardwareMidiLock);
            hardwareMidiBuffer.addEvent (m, 0);
        }

        refreshPlaybackStatus();
    }

    void InstrumentPreviewComponent::liveValueChanged (const juce::String& id, float v)
    {
        project.syncDspGraphFromLiveValues();
        const juce::SpinLock::ScopedLockType lk (engineLock);
        routingEngine.setParameterValue (id, v);
        routingEngine.setFxBlockParameterValue (id, v);
        if (engine) engine->setParameter (id, v);
    }

} // namespace patchcraft
