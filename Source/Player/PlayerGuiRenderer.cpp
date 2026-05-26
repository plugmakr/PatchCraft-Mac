#include "PlayerGuiRenderer.h"
#include "PluginProcessor.h"
#include "PatchCraftLookAndFeel.h"
#include "LicenseValidator.h"
#include "MidiPlaygroundPattern.h"

#include <cmath>
#include <algorithm>
#include <vector>

namespace patchcraft
{
    static juce::String slotId (int index)
    {
        return "p" + juce::String (index);
    }

    static juce::String scopedTabGroupId (const LayoutElement& tabPanel, const juce::String& label)
    {
        if (tabPanel.id == "tabs")
            return LayoutElement::tabLabelToGroupId (label);
        return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
    }

    static bool isScopedTabGroupId (const juce::String& groupId)
    {
        return groupId.contains ("__tab__");
    }

    static float blockValue (const DspBlock& block, const juce::String& key, float fallback)
    {
        const auto it = block.values.find (key);
        return it != block.values.end() ? it->second : fallback;
    }

    static float eqFrequencyToX01 (float frequency)
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        return juce::jlimit (0.0f, 1.0f,
            std::log (f / 20.0f) / std::log (20000.0f / 20.0f));
    }

    static float eqGainToY01 (float gainDb)
    {
        return juce::jlimit (0.0f, 1.0f, juce::jmap (gainDb, 24.0f, -24.0f, 0.0f, 1.0f));
    }

    static const DspBlock* findDrumMachineBlock (const DspGraph& graph)
    {
        for (const auto& block : graph.blocks)
            if (block.type.containsIgnoreCase ("drum") || block.values.find ("dmTracks") != block.values.end())
                return &block;
        return nullptr;
    }

    static const DspBlock* findArpBlock (const DspGraph& graph)
    {
        for (const auto& block : graph.blocks)
            if (block.type.containsIgnoreCase ("arp")
                || block.type.containsIgnoreCase ("midi")
                || block.values.find ("arpSteps") != block.values.end())
                return &block;
        return nullptr;
    }

    static float arpLaneValue (const DspBlock& block, int bank, const juce::String& key, float fallback)
    {
        const auto bankPrefix = "mpBank" + juce::String (juce::jlimit (0, 15, bank) + 1) + "_";
        if (bank > 0)
        {
            const auto bankIt = block.values.find (bankPrefix + key);
            if (bankIt != block.values.end())
                return bankIt->second;
        }

        const auto directIt = block.values.find (key);
        if (directIt != block.values.end())
            return directIt->second;

        const auto bankOneIt = block.values.find (bankPrefix + key);
        return bankOneIt != block.values.end() ? bankOneIt->second : fallback;
    }

    static juce::String arpLaneMidiFileName (juce::String instrumentName, juce::String laneName, int lane)
    {
        if (instrumentName.trim().isEmpty())
            instrumentName = "PatchCraft";
        if (laneName.trim().isEmpty())
            laneName = "ArpLane" + juce::String (lane + 1);

        auto name = juce::File::createLegalFileName (instrumentName.trim()
            + "_" + laneName.trim() + "_midi");
        return name.replaceCharacter (' ', '_') + ".mid";
    }

    static int defaultDrumTrackNote (int track)
    {
        static constexpr int notes[] = { 36, 38, 42, 46, 41, 45, 49, 51,
                                         37, 39, 44, 48, 50, 47, 52, 53 };
        return notes[(size_t) juce::jlimit (0, 15, track)];
    }

    static bool isRuntimePerformanceParameter (const juce::String& parameterId)
    {
        return parameterId == "pitchWheel"
            || parameterId == "modWheel"
            || parameterId == "expression"
            || parameterId == "aftertouch"
            || parameterId == "sustainPedal";
    }

    constexpr int kPianoFirstMidiNote = 21;  // A0
    constexpr int kPianoLastMidiNote  = 108; // C8

    static bool isBlackPianoKey (int midiNote)
    {
        const int semitone = ((midiNote % 12) + 12) % 12;
        return semitone == 1 || semitone == 3 || semitone == 6 || semitone == 8 || semitone == 10;
    }

    static std::vector<int> pianoWhiteNotes()
    {
        std::vector<int> notes;
        notes.reserve (52);
        for (int note = kPianoFirstMidiNote; note <= kPianoLastMidiNote; ++note)
            if (! isBlackPianoKey (note))
                notes.push_back (note);
        return notes;
    }

    static int whiteNotesBefore (int midiNote)
    {
        int count = 0;
        for (int note = kPianoFirstMidiNote; note < midiNote; ++note)
            if (! isBlackPianoKey (note))
                ++count;
        return count;
    }

    static bool isRuntimeImportPathForPlayerCanvas (const juce::String& path)
    {
        const auto file = juce::File (path);
        if (file.isDirectory())
            return true;

        if (! file.existsAsFile())
            return false;

        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"
            || ext == ".mid" || ext == ".midi";
    }

    static bool isRuntimeImportFileForPlayerCanvas (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"
            || ext == ".mid" || ext == ".midi";
    }

    static juce::String manualContainerTargetForElement (const LayoutElement& element, bool& toggleMode)
    {
        const auto action = element.action.isNotEmpty() ? element.action.trim() : element.parameterId.trim();
        toggleMode = false;
        if (action.startsWithIgnoreCase ("showContainer:"))
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        if (action.startsWithIgnoreCase ("toggleContainer:"))
        {
            toggleMode = true;
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        }
        if (action.startsWithIgnoreCase ("showGroup:"))
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        if (action.startsWithIgnoreCase ("toggleGroup:"))
        {
            toggleMode = true;
            return action.fromFirstOccurrenceOf (":", false, false).trim();
        }
        return {};
    }

    static juce::StringArray collectRuntimeImportFilesForPlayerCanvas (const juce::StringArray& sources,
                                                                       int maxFiles = 512)
    {
        juce::StringArray paths;

        for (const auto& sourcePath : sources)
        {
            if (paths.size() >= maxFiles)
                break;

            const juce::File source (sourcePath);
            if (isRuntimeImportFileForPlayerCanvas (source))
            {
                paths.addIfNotAlreadyThere (source.getFullPathName());
                continue;
            }

            if (! source.isDirectory())
                continue;

            juce::DirectoryIterator iterator (source, true, "*", juce::File::findFiles);
            while (iterator.next() && paths.size() < maxFiles)
            {
                const auto child = iterator.getFile();
                if (isRuntimeImportFileForPlayerCanvas (child))
                    paths.addIfNotAlreadyThere (child.getFullPathName());
            }
        }

        return paths;
    }

    static juce::String stringAtOr (const juce::StringArray& values,
                                    int index,
                                    const juce::String& fallback)
    {
        return (index >= 0 && index < values.size()) ? values[index] : fallback;
    }

    static juce::String playerControlGuidance (const Manifest& manifest,
                                               const LayoutElement& element,
                                               const ParameterDef* parameter)
    {
        if (! manifest.playerShowParameterGuidance)
            return {};

        if (element.parameterId.isEmpty())
            return "This control is not connected to a parameter. The instrument developer must assign it in PatchCraft Studio before export.";

        if (parameter == nullptr)
            return "This control points to missing parameter '" + element.parameterId + "'. The exported layout and parameter registry are out of sync.";

        if (parameter->enabledBy.isNotEmpty())
            return parameter->name + " (" + parameter->id + ")\nIf this appears inactive, enable or raise " + parameter->enabledBy + " first.";

        return parameter->name + " (" + parameter->id + ")";
    }

    const Manifest* PlayerGuiRenderer::manifest() const
    {
        if (const auto* pack = proc.getPack())
            return &pack->manifest;
        return nullptr;
    }

    juce::Colour PlayerGuiRenderer::playerBg() const
    {
        return manifest() != nullptr ? manifest()->playerBackgroundColour : PatchCraftLookAndFeel::bg();
    }

    juce::Colour PlayerGuiRenderer::playerPanel() const
    {
        return manifest() != nullptr ? manifest()->playerPanelColour : juce::Colour (0xff15171b);
    }

    juce::Colour PlayerGuiRenderer::playerAccent() const
    {
        return manifest() != nullptr ? manifest()->playerAccentColour : PatchCraftLookAndFeel::accent();
    }

    juce::Colour PlayerGuiRenderer::playerText() const
    {
        return manifest() != nullptr ? manifest()->playerTextColour : PatchCraftLookAndFeel::text();
    }

    juce::Colour PlayerGuiRenderer::playerTextDim() const
    {
        return manifest() != nullptr ? manifest()->playerTextDimColour : PatchCraftLookAndFeel::textDim();
    }

    juce::Colour PlayerGuiRenderer::playerBorder() const
    {
        return manifest() != nullptr ? manifest()->playerBorderColour : PatchCraftLookAndFeel::border();
    }

    PlayerGuiRenderer::PlayerGuiRenderer (PlayerProcessor& p, AssetManager& a)
        : proc (p), assets (a)
    {
        setOpaque (true);
        startTimerHz (30);
    }

    PlayerGuiRenderer::~PlayerGuiRenderer() = default;

    bool PlayerGuiRenderer::isInterestedInFileDrag (const juce::StringArray& files)
    {
        if (proc.getPack() == nullptr)
            return false;

        for (const auto& file : files)
            if (isRuntimeImportPathForPlayerCanvas (file))
                return true;

        return false;
    }

    void PlayerGuiRenderer::filesDropped (const juce::StringArray& files, int, int)
    {
        if (proc.getPack() == nullptr)
            return;

        auto runtimeFiles = collectRuntimeImportFilesForPlayerCanvas (files);

        if (runtimeFiles.isEmpty())
            return;

        juce::String report;
        proc.importUserContentFiles (runtimeFiles, "pads", report);
        if (onRuntimeImportReport)
            onRuntimeImportReport (report);
        rebuild();
        repaint();
    }

    juce::Rectangle<int> PlayerGuiRenderer::arpLaneMidiDragHandleBounds (juce::Rectangle<int> elementBounds) const
    {
        auto header = elementBounds.reduced (10).removeFromTop (26);
        return header.removeFromRight (72).reduced (2, 3);
    }

    juce::Rectangle<int> PlayerGuiRenderer::arpLanePlayButtonBounds (juce::Rectangle<int> elementBounds) const
    {
        auto header = elementBounds.reduced (10).removeFromTop (26);
        header.removeFromRight (72);
        return header.removeFromRight (48).reduced (2, 3);
    }

    juce::String PlayerGuiRenderer::getTooltip()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return {};

        const auto m = metrics();
        const auto pos = getMouseXYRelative();

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || ! isElementOnCurrentTab (e))
                continue;

            const auto r = elementRect (e, m);
            if (! r.contains (pos))
                continue;

            if (e.type == ElementType::PadGrid || e.type == ElementType::DrumPad)
                return "Playable drum pads. Click pads, use hardware pads/keys, or drag WAV/AIFF/FLAC files onto the Player to add runtime samples. Highlight color follows this element's Accent Color in Studio.";

            if (e.type == ElementType::DrumGrid)
                return "MIDI drum pattern grid. Click cells to add/remove hits, Ctrl-click cells for x2/x3/x4 divisions, then press DAW Play or the Player PLAY button to audition.";

            if (e.type == ElementType::ArpLane)
            {
                if (arpLaneMidiDragHandleBounds (r).contains (pos))
                    return "Drag this handle to a DAW track to export this Arp Studio lane as a MIDI clip.";

                return "Circular Arp Studio lane. Click it to select that MIDI Playground bank; velocity spokes control MIDI note velocity.";
            }

            if (e.type == ElementType::Keyboard)
                return "Software keyboard. Click keys to audition this instrument and imported runtime samples.";

            if (e.type == ElementType::Waveform)
                return "Playback display. The vertical playhead follows the DAW transport or the Player PLAY audition button.";

            if (e.type == ElementType::EqCurve)
                return "EQ curve display. Shows the active surgical EQ blocks in this patch so users can see what is shaping the tone.";

            if (e.type == ElementType::SpectrumAnalyzer)
                return "Spectrum analyzer. Shows live output energy so users can see how the current sound is moving.";

            if (e.type == ElementType::Mixer)
                return "Runtime mixer. Drag faders/pan areas when mapped, or assign mixer channels in Studio.";

            if (e.type == ElementType::GranularField)
                return "Granular performance field. Drag inside the field to move sample position and texture controls.";

            if (e.type == ElementType::TabPanel)
                return "Instrument page tabs. Click a tab to switch the visible controls for this Player UI.";

            if (e.type == ElementType::Dropdown)
                return e.id == "presets"
                    ? "Preset selector for this instrument."
                    : "Parameter menu. Assign or choose a parameter in Studio to make this dropdown functional.";

            if (e.type == ElementType::Button || e.type == ElementType::Toggle)
            {
                if (e.parameterId.isEmpty())
                    return "UI button without a parameter assignment. Assign a parameter/action in Studio if it should control sound.";
                if (const auto* parameter = parameterForId (e.parameterId))
                    return parameter->name + " (" + parameter->id + ")";
                return "This button points to missing parameter '" + e.parameterId + "'.";
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider
                || e.type == ElementType::ValueDisplay || e.type == ElementType::XYPad
                || e.type == ElementType::MacroControl)
            {
                if (const auto* parameter = parameterForId (e.parameterId))
                    return playerControlGuidance (pack->manifest, e, parameter);
                if (e.parameterId.isNotEmpty())
                    return "This control points to missing parameter '" + e.parameterId + "'.";
            }
        }

        return "Drop WAV/AIFF/FLAC samples or MID/MIDI files here to import them into this Player session.";
    }

    bool PlayerGuiRenderer::isElementOnCurrentTab (const LayoutElement& e) const
    {
        if (e.groupId.isEmpty()) return true;

        if (isManualContainerGroup (e.groupId))
        {
            const auto state = manualContainerOpen.find (e.groupId);
            return state != manualContainerOpen.end() && state->second;
        }

        for (const auto& parent : elementsCopy)
        {
            if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                && parent.id == e.groupId)
                return true;
            if (parent.type == ElementType::TabPanel)
            {
                for (const auto& tab : parent.tabs)
                {
                    const auto group = tabTargetGroup (parent, tab);
                    if (group == e.groupId)
                    {
                        const auto active = activeTabGroupsByPanel.find (parent.id);
                        const auto activeGroup = active != activeTabGroupsByPanel.end()
                            ? active->second
                            : (parent.tabs.isEmpty() ? juce::String() : tabTargetGroup (parent, parent.tabs[0]));
                        return activeGroup == e.groupId && isElementOnCurrentTab (parent);
                    }
                }
            }
        }
        if (isScopedTabGroupId (e.groupId))
            return false;
        return e.groupId == currentTabGroup;
    }

    bool PlayerGuiRenderer::isManualContainerGroup (const juce::String& groupId) const
    {
        return manualContainerOrder.contains (groupId);
    }

    void PlayerGuiRenderer::initialiseManualContainers()
    {
        manualContainerOrder.clear();
        manualContainerOpen.clear();

        for (const auto& element : elementsCopy)
        {
            bool toggleMode = false;
            const auto target = manualContainerTargetForElement (element, toggleMode);
            juce::ignoreUnused (toggleMode);
            if (target.isNotEmpty())
                manualContainerOrder.addIfNotAlreadyThere (target);
        }

        for (int i = 0; i < manualContainerOrder.size(); ++i)
            manualContainerOpen[manualContainerOrder[i]] = (i == 0);
    }

    bool PlayerGuiRenderer::triggerManualContainer (const LayoutElement& element)
    {
        bool toggleMode = false;
        const auto target = manualContainerTargetForElement (element, toggleMode);
        if (target.isEmpty())
            return false;

        if (! manualContainerOrder.contains (target))
            manualContainerOrder.add (target);

        if (toggleMode)
        {
            const bool nowOpen = ! manualContainerOpen[target];
            manualContainerOpen[target] = nowOpen;
        }
        else
        {
            for (const auto& group : manualContainerOrder)
                manualContainerOpen[group] = false;
            manualContainerOpen[target] = true;
        }

        refreshControlEnablement();
        resized();
        repaint();
        return true;
    }

    int PlayerGuiRenderer::parameterIndexForId (const juce::String& parameterId) const
    {
        return proc.getHostParameterSlotIndex (parameterId);
    }

    const ParameterDef* PlayerGuiRenderer::parameterForId (const juce::String& parameterId) const
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || parameterId.isEmpty())
            return nullptr;

        for (const auto& def : pack->parameters.getAll())
            if (def.id == parameterId)
                return &def;
        return nullptr;
    }

    bool PlayerGuiRenderer::parameterIsEnabled (const ParameterDef& def) const
    {
        if (def.enabledBy.isEmpty())
            return true;

        const auto* gate = parameterForId (def.enabledBy);
        if (gate == nullptr)
            return false;

        const auto value = proc.getPackParameterValue (def.enabledBy);
        return gate->displayMode == "toggle" ? value >= 0.5f : value > 0.0001f;
    }

    void PlayerGuiRenderer::refreshControlEnablement()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || controls.isEmpty())
            return;

        for (int i = 0; i < controls.size() && i < (int) elementsCopy.size(); ++i)
        {
            auto* control = controls[i];
            if (control == nullptr)
                continue;

            const auto& element = elementsCopy[(size_t) i];
            const bool isSliderControl = element.type == ElementType::Knob
                                      || element.type == ElementType::Slider
                                      || element.type == ElementType::MacroControl;
            if (! isSliderControl)
                continue;

            const auto* def = parameterForId (element.parameterId);
            const int slotIndex = parameterIndexForId (element.parameterId);
            const bool mapped = def != nullptr && slotIndex >= 0 && slotIndex < kPatchCraftHostParameterSlots;
            const bool internalRuntimeControl = def != nullptr && isRuntimePerformanceParameter (element.parameterId);
            const bool enabled = def != nullptr && (mapped || internalRuntimeControl || ! element.parameterId.isEmpty())
                              && parameterIsEnabled (*def);
            control->setEnabled (enabled);
            control->setAlpha (0.01f);
        }
    }

    float PlayerGuiRenderer::parameterValueForElement (const LayoutElement& element, float fallback) const
    {
        const int index = parameterIndexForId (element.parameterId);
        const auto* def = parameterForId (element.parameterId);
        if (index >= 0 && index < kPatchCraftHostParameterSlots && def != nullptr)
        {
            if (auto* value = proc.getApvts().getRawParameterValue (slotId (index)))
                return juce::jmap (juce::jlimit (0.0f, 1.0f, value->load()), 0.0f, 1.0f, def->min, def->max);
        }
        return def != nullptr ? proc.getPackParameterValue (element.parameterId) : fallback;
    }

    juce::String PlayerGuiRenderer::formattedParameterValue (const LayoutElement& element) const
    {
        const auto* def = parameterForId (element.parameterId);
        if (def == nullptr)
            return element.label.isNotEmpty() ? element.label : element.parameterId;

        const auto value = parameterValueForElement (element, def->defaultValue);
        auto choiceText = [&] (const juce::StringArray& choices)
        {
            const int index = juce::jlimit (0, choices.size() - 1, juce::roundToInt (value));
            return choices[index];
        };

        if (def->id == "granularDirection")
            return choiceText ({ "Forward", "Reverse", "Ping-Pong", "Multi" });

        if (def->id == "granularWindow")
            return choiceText ({ "Hann", "Triangle", "Blackman", "Plateau" });

        if (def->displayMode == "toggle" || def->step >= 1.0f)
            return juce::String (juce::roundToInt (value)) + (def->unit.isNotEmpty() ? " " + def->unit : "");
        return juce::String (value, def->unit == "Hz" || def->unit == "ms" ? 1 : 2)
            + (def->unit.isNotEmpty() ? " " + def->unit : "");
    }

    juce::Rectangle<int> PlayerGuiRenderer::animatedElementRect (const LayoutElement& element,
                                                                 juce::Rectangle<int> rect) const
    {
        float amount = 0.0f;
        if (element.audioReactive)
            amount += proc.getOutputPeak() * juce::jmax (0.10f, element.audioReactiveAmount);
        if (element.animationMode != "none")
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi
                                                  * juce::jmax (0.05f, element.animationRate)) * 0.5) + 0.5);
            if (element.animationMode == "shake")
            {
                rect.translate (juce::roundToInt ((wave - 0.5f) * 10.0f), 0);
                return rect;
            }
            amount += wave * (element.animationMode == "breathe" ? 0.05f : 0.08f);
        }

        if (amount <= 0.0001f)
            return rect;

        const int grow = juce::roundToInt (juce::jlimit (0.0f, 18.0f, amount * 14.0f));
        return rect.expanded (grow, grow);
    }

    juce::Rectangle<int> PlayerGuiRenderer::tabBounds (juce::Rectangle<int> bounds,
                                                       int tabIndex,
                                                       int tabCount) const
    {
        tabCount = juce::jmax (1, tabCount);
        tabIndex = juce::jlimit (0, tabCount - 1, tabIndex);
        const int left = bounds.getX() + (bounds.getWidth() * tabIndex) / tabCount;
        const int right = bounds.getX() + (bounds.getWidth() * (tabIndex + 1)) / tabCount;
        return { left, bounds.getY(), juce::jmax (1, right - left), bounds.getHeight() };
    }

    void PlayerGuiRenderer::drawLabelElement (juce::Graphics& g,
                                              const LayoutElement& e,
                                              juce::Rectangle<int> r) const
    {
        juce::String text = e.label;
        if (const auto* m = manifest())
        {
            if (e.id == "title" && m->playerDisplayName.isNotEmpty())
                text = m->playerDisplayName;
            else if (e.id == "tagline" && m->playerTagline.isNotEmpty())
                text = m->playerTagline;
        }

        if (text.isEmpty())
            return;

        g.setColour (e.textColour.isTransparent() ? playerText().brighter (0.25f) : e.textColour);
        g.setFont (juce::FontOptions (juce::jmax (12.0f, (float) r.getHeight() * 0.5f)).withStyle ("bold"));
        g.drawText (text, r, juce::Justification::centredLeft, true);
    }

    void PlayerGuiRenderer::drawControlLabelOverlay (juce::Graphics& g,
                                                     const LayoutElement& e,
                                                     juce::Rectangle<int> r) const
    {
        if (e.labelPosition == "hidden")
            return;

        juce::Rectangle<int> labelArea;
        const int defaultH = juce::jmax (20, r.getHeight() / 4);
        if (e.labelPosition == "top")
            labelArea = r.removeFromTop (defaultH).translated (juce::roundToInt (e.labelOffsetX),
                                                               juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "left")
            labelArea = { r.getX() - r.getWidth() / 2 - juce::roundToInt (e.labelSpacing),
                          r.getCentreY() - 18,
                          r.getWidth() / 2,
                          36 };
        else if (e.labelPosition == "right")
            labelArea = { r.getRight() + juce::roundToInt (e.labelSpacing),
                          r.getCentreY() - 18,
                          r.getWidth() / 2,
                          36 };
        else
            labelArea = r.removeFromBottom (defaultH);

        if (e.labelPosition != "top")
            labelArea.translate (juce::roundToInt (e.labelOffsetX),
                                 juce::roundToInt (e.labelOffsetY + e.labelSpacing));

        if (labelArea.isEmpty())
            return;

        const auto labelText = e.label.isNotEmpty() ? e.label : e.parameterId;
        if (labelText.isEmpty())
            return;

        const auto colour = e.textColour.isTransparent() ? playerText().brighter (0.25f) : e.textColour;
        const float fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, labelArea.getHeight() * 0.42f);
        g.setColour (colour);
        g.setFont (juce::FontOptions (fontSize).withStyle ("bold"));
        g.drawText (labelText.toUpperCase(),
                    labelArea.removeFromTop (labelArea.getHeight() / 2),
                    juce::Justification::centred, true);

        juce::String valueText;
        if (parameterForId (e.parameterId) != nullptr)
            valueText = formattedParameterValue (e);
        else if (e.parameterId.isNotEmpty())
            valueText = "missing";

        if (valueText.isNotEmpty())
        {
            g.setColour (e.accentColour.isTransparent() ? playerAccent() : e.accentColour);
            g.setFont (juce::FontOptions (juce::jmax (8.0f, fontSize * 0.85f)));
            g.drawText (valueText, labelArea, juce::Justification::centred, true);
        }
    }

    juce::Rectangle<int> PlayerGuiRenderer::controlBodyRect (const LayoutElement& e,
                                                             juce::Rectangle<int> r) const
    {
        if (e.labelPosition != "hidden"
            && (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl))
            r.removeFromBottom (juce::jmax (20, r.getHeight() / 4));
        return r.reduced (2);
    }

    void PlayerGuiRenderer::drawRuntimeControl (juce::Graphics& g,
                                                juce::Rectangle<int> r,
                                                const LayoutElement& e) const
    {
        const auto* def = parameterForId (e.parameterId);
        const auto minValue = def != nullptr ? def->min : 0.0f;
        const auto maxValue = def != nullptr ? def->max : 1.0f;
        const auto fallback = def != nullptr ? def->defaultValue : 0.5f;
        const auto value = parameterValueForElement (e, fallback);
        const auto norm = juce::jlimit (0.0f, 1.0f, (value - minValue) / juce::jmax (0.0001f, maxValue - minValue));
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
        const auto border = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto fill = e.backgroundColour.isTransparent() ? playerPanel().brighter (0.04f) : e.backgroundColour;
        const bool connected = def != nullptr
                            && (parameterIndexForId (e.parameterId) >= 0
                                || isRuntimePerformanceParameter (e.parameterId)
                                || ! e.parameterId.isEmpty())
                            && parameterIsEnabled (*def);
        const float disabledAlpha = connected ? 1.0f : 0.38f;

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (disabledAlpha);

        if (e.type == ElementType::Slider)
        {
            auto track = r.reduced (r.getWidth() / 3, 6).toFloat();
            g.setColour (fill);
            g.fillRoundedRectangle (track, 4.0f);
            g.setColour (accent.withAlpha (0.85f));
            auto active = track;
            active.setY (juce::jmap (norm, track.getBottom(), track.getY()));
            g.fillRoundedRectangle (active, 4.0f);
            g.setColour (border);
            g.drawRoundedRectangle (track, 4.0f, 1.0f);
            return;
        }

        if (e.type == ElementType::MacroControl)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
            g.setColour (border);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
            r = r.reduced (8, 6);
            auto title = r.removeFromTop (18);
            g.setColour (accent);
            g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
            g.drawText ((e.label.isNotEmpty() ? e.label : "Macro").toUpperCase(),
                        title, juce::Justification::centredLeft, true);
            r.removeFromTop (2);
        }

        juce::Rectangle<int> macroLanes;
        if (e.type == ElementType::MacroControl && r.getWidth() > 118)
            macroLanes = r.removeFromRight (juce::jmax (58, r.getWidth() / 2)).reduced (4, 2);

        auto dial = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                             juce::jmin (r.getWidth(), r.getHeight())).toFloat().reduced (3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (dial.translated (0.0f, 2.0f));
        g.setColour (fill);
        g.fillEllipse (dial);
        g.setColour (border);
        g.drawEllipse (dial, 1.0f);

        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end = juce::MathConstants<float>::pi * 2.75f;
        juce::Path arc;
        arc.addCentredArc (dial.getCentreX(), dial.getCentreY(),
                           dial.getWidth() * 0.43f, dial.getHeight() * 0.43f,
                           0.0f, start, juce::jmap (norm, start, end), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.0f, dial.getWidth() * 0.08f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        const auto angle = juce::jmap (norm, start, end);
        const auto radius = dial.getWidth() * 0.32f;
        const auto centre = dial.getCentre();
        g.drawLine (centre.x, centre.y,
                    centre.x + std::cos (angle) * radius,
                    centre.y + std::sin (angle) * radius,
                    juce::jmax (1.0f, dial.getWidth() * 0.04f));

        if (e.type == ElementType::MacroControl && ! macroLanes.isEmpty())
        {
            juce::StringArray targets;
            if (const auto* pack = proc.getPack())
                for (const auto& macro : pack->dspGraph.macros)
                    if (macro.macroId == e.parameterId)
                        targets.add (macro.targetId);

            if (targets.isEmpty())
                targets.addArray ({ "No routes", "Open Studio", "Add target", "Apply" });

            for (int i = 0; i < juce::jmin (4, targets.size()); ++i)
            {
                auto row = macroLanes.removeFromTop (juce::jmax (14, macroLanes.getHeight() / (4 - i))).reduced (0, 2);
                g.setColour (playerBg().withAlpha (0.72f));
                g.fillRoundedRectangle (row.toFloat(), 3.0f);
                g.setColour (targets[i] == "No routes" ? border.withAlpha (0.35f) : accent.withAlpha (0.62f));
                g.fillRoundedRectangle (row.withWidth (juce::roundToInt ((float) row.getWidth() * norm)).toFloat(), 3.0f);
                g.setColour (playerTextDim());
                g.setFont (juce::FontOptions (8.0f).withStyle ("bold"));
                g.drawText (targets[i], row.reduced (4, 0), juce::Justification::centredLeft, true);
            }
        }
    }

    juce::String PlayerGuiRenderer::tabTargetGroup (const LayoutElement& panel,
                                                    const juce::String& label) const
    {
        const auto global = LayoutElement::tabLabelToGroupId (label);
        if (panel.id == "tabs")
            return global;

        if (panel.groupId.isEmpty())
        {
            for (const auto& element : elementsCopy)
                if (element.groupId == global)
                    return global;
        }

        const auto scoped = scopedTabGroupId (panel, label);
        for (const auto& element : elementsCopy)
            if (element.groupId == scoped)
                return scoped;

        for (const auto& element : elementsCopy)
            if (element.groupId == global)
                return global;

        return scoped;
    }

    bool PlayerGuiRenderer::tabTargetIsGlobal (const LayoutElement& panel,
                                               const juce::String& targetGroup) const
    {
        if (panel.id == "tabs")
            return true;

        if (panel.groupId.isEmpty() && ! isScopedTabGroupId (targetGroup))
            return true;

        return false;
    }

    void PlayerGuiRenderer::rebuild()
    {
        controls.clear();
        attachments.clear();
        elementsCopy.clear();

        const auto* pack = proc.getPack();
        if (pack == nullptr) { background = {}; repaint(); return; }

        elementsCopy = pack->layout.getAll();

        // Cache whether the layout has any element whose paint depends on the
        // live output peak. If not, the timer doesn't need to repaint at all
        // when audio is playing — only when MIDI-learn / control enablement
        // state changes.
        hasMeterOrReactiveElement = false;
        for (const auto& e : elementsCopy)
        {
            if (e.audioReactive
                || (e.animationMode.isNotEmpty() && e.animationMode != "none")
                || e.type == ElementType::Meter
                || e.type == ElementType::Waveform
                || e.type == ElementType::SpectrumAnalyzer
                || e.type == ElementType::GranularField
                || e.type == ElementType::ArpLane
                || e.type == ElementType::Mixer)
            {
                hasMeterOrReactiveElement = true;
                break;
            }
        }

        // Default tab = first one defined in the first TabPanel, or "main".
        currentTabGroup = "main";
        activeTabGroupsByPanel.clear();
        for (auto& e : elementsCopy)
            if (e.type == ElementType::TabPanel && ! e.tabs.isEmpty())
            {
                const auto target = tabTargetGroup (e, e.tabs[0]);
                activeTabGroupsByPanel[e.id] = target;
                if (tabTargetIsGlobal (e, target))
                    currentTabGroup = target;
            }
        initialiseManualContainers();

        background = juce::Image();
        heroImage = juce::Image();
        if (pack->backgroundImageRelative.isNotEmpty())
        {
            auto f = pack->rootFolder.getChildFile (pack->backgroundImageRelative);
            background = assets.loadImage (f);
        }
        if (! background.isValid())
            background = AssetManager::renderDefaultHeroImage (
                pack->canvasSize.width, pack->canvasSize.height);

        // Generate hero image for the hero artwork area (1200x360 in template)
        heroImage = AssetManager::renderDefaultHeroImage (1200, 360);

        // Map parameter id -> slot index for APVTS attachments.
        std::map<juce::String, int> paramIndex;
        for (auto& def : pack->parameters.getAll())
        {
            const auto slot = proc.getHostParameterSlotIndex (def.id);
            if (slot >= 0)
                paramIndex[def.id] = slot;
        }

        for (auto& e : elementsCopy)
        {
            if (! e.visible) { controls.add (new juce::Component()); continue; }
            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
            {
                auto slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle (e.type == ElementType::Slider
                    ? juce::Slider::LinearVertical
                    : juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
                slider->setColour (juce::Slider::rotarySliderFillColourId, e.accentColour);
                slider->setAlpha (0.01f);

                if (e.filmstripAsset.isNotEmpty())
                {
                    juce::File f = juce::File::isAbsolutePath (e.filmstripAsset)
                        ? juce::File (e.filmstripAsset)
                        : pack->rootFolder.getChildFile (e.filmstripAsset);
                    slider->getProperties().set ("filmstripPath",   f.getFullPathName());
                    slider->getProperties().set ("filmstripFrames", e.filmstripFrames);
                    slider->getProperties().set ("filmstripVertical", e.filmstripVertical);
                }

                slider->addMouseListener (this, false);
                auto it = paramIndex.find (e.parameterId);
                const auto* parameter = [&]() -> const ParameterDef*
                {
                    for (const auto& def : pack->parameters.getAll())
                        if (def.id == e.parameterId)
                            return &def;
                    return nullptr;
                }();
                slider->setTooltip (playerControlGuidance (pack->manifest, e, parameter));
                if (it != paramIndex.end() && it->second < kPatchCraftHostParameterSlots)
                {
                    auto* att = new juce::AudioProcessorValueTreeState::SliderAttachment (
                        proc.getApvts(), slotId (it->second), *slider);
                    attachments.add (att);
                }
                else if (parameter != nullptr)
                {
                    const double interval = parameter->step > 0.0f ? (double) parameter->step : 0.0;
                    auto* sliderPtr = slider.get();
                    sliderPtr->setRange ((double) parameter->min, (double) parameter->max, interval);
                    sliderPtr->setValue ((double) proc.getPackParameterValue (e.parameterId),
                                         juce::dontSendNotification);
                    sliderPtr->onValueChange = [this, sliderPtr, parameterId = e.parameterId]
                    {
                        proc.setPackParameterFromUi (parameterId, (float) sliderPtr->getValue());
                        repaint();
                    };
                    slider->setTooltip (playerControlGuidance (pack->manifest, e, parameter)
                        + "\nInternal Player control: it changes sound in real time but is not exposed as host automation.");
                }
                else
                {
                    slider->setEnabled (false);
                    slider->setAlpha (0.01f);
                    slider->setTooltip ("This control is not connected to a PatchCraft parameter. Assign a parameter in Studio before export.");
                }
                addAndMakeVisible (*slider);
                controls.add (slider.release());
            }
            else
            {
                // Lightweight component keeps controls[] aligned; these element types
                // are painted directly instead of using child widgets.
                auto comp = std::make_unique<juce::Component>();
                comp->setInterceptsMouseClicks (false, false);
                addAndMakeVisible (*comp);
                controls.add (comp.release());
            }
        }
        refreshControlEnablement();
        resized();
        repaint();
    }

    PlayerGuiRenderer::CanvasMetrics PlayerGuiRenderer::metrics() const
    {
        CanvasMetrics m;
        const auto* pack = proc.getPack();
        if (pack == nullptr) { m.scale = 1.0f; m.canvas = {}; return m; }
        const float sx = (float) getWidth()  / (float) pack->canvasSize.width;
        const float sy = (float) getHeight() / (float) pack->canvasSize.height;
        m.scale = juce::jmin (sx, sy);
        const int rw = (int) (pack->canvasSize.width  * m.scale);
        const int rh = (int) (pack->canvasSize.height * m.scale);
        m.canvas = juce::Rectangle<int> ((getWidth()  - rw) / 2,
                                         (getHeight() - rh) / 2, rw, rh);
        return m;
    }

    juce::Rectangle<int> PlayerGuiRenderer::elementRect (const LayoutElement& e,
                                                         const CanvasMetrics& m) const
    {
        return juce::Rectangle<int> (
            m.canvas.getX() + (int) (e.x * m.scale),
            m.canvas.getY() + (int) (e.y * m.scale),
            (int) (e.width  * m.scale),
            (int) (e.height * m.scale));
    }

    // ---- Drawing helpers -----------------------------------------------------
    void PlayerGuiRenderer::drawHeroPlaceholder (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        g.setColour (playerPanel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
    }

    void PlayerGuiRenderer::drawMeter (juce::Graphics& g, juce::Rectangle<int> r,
                                       const LayoutElement& element) const
    {
        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        const bool vertical = r.getHeight() > r.getWidth() * 1.2f;
        const int segs = vertical ? 16 : 24;
        const auto inner = r.reduced (4);
        float level = proc.getOutputPeak();
        if (element.parameterId.isNotEmpty())
        {
            if (const auto* def = parameterForId (element.parameterId))
            {
                const auto value = parameterValueForElement (element, def->defaultValue);
                level = juce::jlimit (0.0f, 1.0f, (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
            }
        }
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            juce::Colour c = (t < 0.6f) ? juce::Colour (0xff5fb37b)
                          : (t < 0.85f ? juce::Colour (0xffe8b840)
                                       : juce::Colour (0xffe6504a));
            const float alpha = (t < 0.7f) ? 1.0f : 0.85f;
            const bool lit = t <= level;
            if (vertical)
            {
                const int sh = juce::jmax (2, inner.getHeight() / segs - 1);
                const int sy = inner.getBottom() - (i + 1) * (sh + 1);
                g.setColour (c.withAlpha (lit ? alpha : 0.16f));
                g.fillRect (inner.getX(), sy, inner.getWidth(), sh);
            }
            else
            {
                const int sw = juce::jmax (2, inner.getWidth() / segs - 1);
                const int sx = inner.getX() + i * (sw + 1);
                g.setColour (c.withAlpha (lit ? alpha : 0.16f));
                g.fillRect (sx, inner.getY(), sw, inner.getHeight());
            }
        }
    }

    void PlayerGuiRenderer::drawEqCurve (juce::Graphics& g, juce::Rectangle<int> r,
                                         const LayoutElement& element) const
    {
        const auto bg = element.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.78f) : element.backgroundColour;
        const auto border = element.borderColour.isTransparent() ? playerBorder() : element.borderColour;
        const auto accent = element.accentColour.isTransparent() ? playerAccent() : element.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, element.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, element.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "EQ CURVE",
                    header.removeFromLeft (130), juce::Justification::centredLeft, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("live patch EQ", header, juce::Justification::centredRight, true);

        auto graphArea = area.reduced (2, 4);
        g.setColour (playerBg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);
        g.setColour (border.withAlpha (0.22f));
        for (int i = 1; i < 5; ++i)
        {
            const int x = graphArea.getX() + (graphArea.getWidth() * i) / 5;
            const int y = graphArea.getY() + (graphArea.getHeight() * i) / 5;
            g.drawVerticalLine (x, (float) graphArea.getY(), (float) graphArea.getBottom());
            g.drawHorizontalLine (y, (float) graphArea.getX(), (float) graphArea.getRight());
        }

        const auto* pack = proc.getPack();
        juce::Path curve;
        for (int i = 0; i < 96; ++i)
        {
            const float x01 = (float) i / 95.0f;
            float y = (float) graphArea.getCentreY();
            if (pack != nullptr)
            {
                for (const auto& block : pack->dspGraph.blocks)
                {
                    if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                        continue;
                    const float freqX = eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f));
                    const float gain = blockValue (block, "eqGainDb", 0.0f);
                    const float q = juce::jlimit (0.15f, 18.0f, blockValue (block, "eqQ", 1.0f));
                    const float width = juce::jlimit (0.025f, 0.22f, 0.11f / std::sqrt (q));
                    const float influence = std::exp (-std::pow ((x01 - freqX) / width, 2.0f));
                    y -= (gain / 24.0f) * influence * (float) graphArea.getHeight() * 0.44f;
                }
            }
            const float x = (float) graphArea.getX() + x01 * (float) graphArea.getWidth();
            y = juce::jlimit ((float) graphArea.getY(), (float) graphArea.getBottom(), y);
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }
        g.setColour (accent.withAlpha (0.92f));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        if (pack != nullptr)
        {
            for (const auto& block : pack->dspGraph.blocks)
            {
                if (block.section != "filter" || ! block.type.containsIgnoreCase ("eq") || ! block.enabled)
                    continue;
                const float x = (float) graphArea.getX() + eqFrequencyToX01 (blockValue (block, "eqFreq", 1000.0f)) * (float) graphArea.getWidth();
                const float y = (float) graphArea.getY() + eqGainToY01 (blockValue (block, "eqGainDb", 0.0f)) * (float) graphArea.getHeight();
                g.setColour (accent);
                g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            }
        }
    }

    void PlayerGuiRenderer::drawSpectrumAnalyzer (juce::Graphics& g, juce::Rectangle<int> r,
                                                  const LayoutElement& element) const
    {
        const auto bg = element.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.78f) : element.backgroundColour;
        const auto border = element.borderColour.isTransparent() ? playerBorder() : element.borderColour;
        const auto accent = element.accentColour.isTransparent() ? juce::Colour (0xff20d6ff) : element.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, element.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, element.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (20);
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "SPECTRUM",
                    header.removeFromLeft (130), juce::Justification::centredLeft, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("runtime analyzer", header, juce::Justification::centredRight, true);

        auto graphArea = area.reduced (2, 4);
        g.setColour (playerBg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);
        const float level = juce::jlimit (0.08f, 1.0f, proc.getOutputPeak() + 0.14f);
        constexpr int bars = 36;
        for (int i = 0; i < bars; ++i)
        {
            const float x01 = (float) i / (float) (bars - 1);
            const float h01 = level * (0.18f + 0.72f * std::abs (std::sin (x01 * 9.0f + (float) i * 0.37f)));
            const int barW = juce::jmax (2, graphArea.getWidth() / bars - 2);
            const int x = graphArea.getX() + i * graphArea.getWidth() / bars;
            const int h = juce::roundToInt (h01 * (float) graphArea.getHeight());
            g.setColour (accent.interpolatedWith (playerAccent(), x01).withAlpha (0.78f));
            g.fillRoundedRectangle ((float) x, (float) graphArea.getBottom() - (float) h,
                                    (float) barW, (float) h, 2.0f);
        }
    }

    void PlayerGuiRenderer::drawPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                       const juce::String& label) const
    {
        g.setColour (playerPanel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
        if (label.isNotEmpty())
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (label, r.reduced (8, 4), juce::Justification::topLeft);
        }
    }

    void PlayerGuiRenderer::drawButton (juce::Graphics& g, juce::Rectangle<int> r,
                                        const LayoutElement& e) const
    {
        const bool active = activeMomentaryParameter.isNotEmpty()
            && e.parameterId == activeMomentaryParameter;
        g.setColour ((e.backgroundColour.isTransparent() ? playerPanel().brighter (0.06f) : e.backgroundColour)
                     .withAlpha (active ? 0.92f : 0.78f));
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour ((e.accentColour.isTransparent() ? playerAccent() : e.accentColour).withAlpha (active ? 1.0f : 0.72f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), active ? 2.0f : 1.0f);
        g.setColour (e.textColour.isTransparent() ? playerText() : e.textColour);
        g.setFont (juce::FontOptions (juce::jmax (11.0f, r.getHeight() * 0.32f)).withStyle ("bold"));
        g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId, r.reduced (8, 0), juce::Justification::centred, true);
    }

    void PlayerGuiRenderer::drawValueDisplay (juce::Graphics& g, juce::Rectangle<int> r,
                                              const LayoutElement& e) const
    {
        g.setColour (playerPanel().brighter (0.04f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                    r.withHeight (juce::jmin (18, r.getHeight())).reduced (6, 0),
                    juce::Justification::centredLeft, true);
        g.setColour (playerAccent());
        g.setFont (juce::FontOptions (juce::jmax (12.0f, r.getHeight() * 0.36f)).withStyle ("bold"));
        g.drawText (formattedParameterValue (e), r.reduced (8, 0), juce::Justification::centredRight, true);
    }

    void PlayerGuiRenderer::drawDropdown (juce::Graphics& g, juce::Rectangle<int> r,
                                          const juce::String& display) const
    {
        g.setColour (playerPanel().brighter (0.04f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
        g.setColour (playerText());
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.drawText (display, r.reduced (28, 0), juce::Justification::centred);

        // Arrows
        g.setColour (playerTextDim());
        const float cy = r.getCentreY();
        juce::Path l;  l.addTriangle ((float) r.getX() + 14.0f, cy - 5.0f,
                                       (float) r.getX() + 14.0f, cy + 5.0f,
                                       (float) r.getX() + 8.0f,  cy);
        juce::Path rt; rt.addTriangle ((float) r.getRight() - 14.0f, cy - 5.0f,
                                       (float) r.getRight() - 14.0f, cy + 5.0f,
                                       (float) r.getRight() - 8.0f,  cy);
        g.fillPath (l); g.fillPath (rt);
    }

    void PlayerGuiRenderer::drawKeyboard (juce::Graphics& g, juce::Rectangle<int> r) const
    {
        auto body = r.toFloat();
        g.setColour (juce::Colour (0xff05060a).withAlpha (0.94f));
        g.fillRoundedRectangle (body, 8.0f);
        g.setColour (playerBorder().withAlpha (0.78f));
        g.drawRoundedRectangle (body.reduced (0.5f), 8.0f, 1.0f);

        const auto whiteNotes = pianoWhiteNotes();
        const float kw = (float) (r.getWidth() - 12) / (float) whiteNotes.size();
        const float keyTop = (float) r.getY() + 6.0f;
        const float keyH   = (float) r.getHeight() - 12.0f;

        for (int i = 0; i < (int) whiteNotes.size(); ++i)
        {
            const int midiNote = whiteNotes[(size_t) i];
            const float active = proc.getNoteHighlightLevel (midiNote);
            juce::Rectangle<float> key (r.getX() + 6 + i * kw, keyTop, kw - 1.0f, keyH);
            auto white = juce::Colour (0xfff2ead9).interpolatedWith (playerAccent().brighter (0.25f),
                                                                     juce::jlimit (0.0f, 1.0f, active));
            g.setGradientFill (juce::ColourGradient (white.brighter (0.08f), key.getX(), key.getY(),
                                                     white.darker (0.12f), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.5f);
            g.setColour (active > 0.01f ? playerAccent().withAlpha (0.92f) : juce::Colour (0xff887b63));
            g.drawRoundedRectangle (key, 1.5f, active > 0.01f ? 1.2f : 0.5f);

            if (midiNote % 12 == 0 && key.getWidth() > 13.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.58f));
                g.setFont (juce::FontOptions (8.5f));
                g.drawText ("C" + juce::String (midiNote / 12 - 1),
                            key.withTrimmedTop (keyH - 13.0f).toNearestInt(),
                            juce::Justification::centred, true);
            }
        }
        const float bkH = keyH * 0.62f;
        const float bkW = kw * 0.62f;
        for (int midiNote = kPianoFirstMidiNote; midiNote <= kPianoLastMidiNote; ++midiNote)
        {
            if (! isBlackPianoKey (midiNote))
                continue;

            const int before = whiteNotesBefore (midiNote);
            const float x = r.getX() + 6 + before * kw - bkW * 0.5f;
            if (x < r.getX() + 4 || x + bkW > r.getRight() - 4)
                continue;

            const float active = proc.getNoteHighlightLevel (midiNote);
            juce::Rectangle<float> key (x, keyTop, bkW, bkH);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff202226).interpolatedWith (playerAccent(), active * 0.45f),
                                                     key.getX(), key.getY(),
                                                     juce::Colour (0xff050507), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.5f);
            g.setColour (active > 0.01f ? playerAccent().withAlpha (0.95f) : juce::Colour (0xff050505));
            g.drawRoundedRectangle (key, 1.5f, active > 0.01f ? 1.2f : 0.5f);
        }
    }

    void PlayerGuiRenderer::drawTabPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e) const
    {
        const int n = juce::jmax (1, e.tabs.size());

        for (int i = 0; i < n; ++i)
        {
            const auto label = i < e.tabs.size() ? e.tabs[i] : juce::String ("Tab");
            const auto groupId = tabTargetGroup (e, label);
            const auto found = activeTabGroupsByPanel.find (e.id);
            const auto activeGroup = found != activeTabGroupsByPanel.end()
                ? found->second
                : (tabTargetIsGlobal (e, groupId) ? currentTabGroup
                                                   : (e.tabs.isEmpty() ? juce::String() : tabTargetGroup (e, e.tabs[0])));
            const bool active = (groupId == activeGroup);
            auto tabRect = tabBounds (r, i, n);
            g.setColour (active ? playerText().brighter (0.25f)
                                : juce::Colour (0xffb8bcc4));
            g.setFont (juce::FontOptions (juce::jmax (10.0f, (float) tabRect.getHeight() * 0.42f)).withStyle ("bold"));
            g.drawText (label.toUpperCase(), tabRect, juce::Justification::centred, true);
            if (active)
            {
                g.setColour (playerAccent());
                g.fillRect (tabRect.removeFromBottom (2));
            }
        }
    }

    void PlayerGuiRenderer::drawXYPad (juce::Graphics& g, juce::Rectangle<int> r,
                                       const LayoutElement& e) const
    {
        const auto backgroundColour = e.backgroundColour.isTransparent()
            ? playerPanel().darker (0.08f)
            : e.backgroundColour;
        const auto borderColour = e.borderColour.isTransparent()
            ? playerBorder()
            : e.borderColour;
        const auto accentColour = e.accentColour.isTransparent()
            ? playerAccent()
            : e.accentColour;

        g.setColour (backgroundColour);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour (borderColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);

        const auto inner = r.reduced (8);
        if (inner.isEmpty())
            return;

        g.setColour (borderColour.withAlpha (0.35f));
        g.drawLine ((float) inner.getCentreX(), (float) inner.getY(), (float) inner.getCentreX(), (float) inner.getBottom(), 1.0f);
        g.drawLine ((float) inner.getX(), (float) inner.getCentreY(), (float) inner.getRight(), (float) inner.getCentreY(), 1.0f);

        float normalised = 0.5f;
        if (const auto* def = parameterForId (e.parameterId))
        {
            const auto range = juce::jmax (0.0001f, def->max - def->min);
            normalised = juce::jlimit (0.0f, 1.0f, (parameterValueForElement (e, def->defaultValue) - def->min) / range);
        }

        const float dotX = juce::jmap (normalised, 0.0f, 1.0f, (float) inner.getX(), (float) inner.getRight());
        const float dotY = (float) inner.getCentreY();
        g.setColour (accentColour.withAlpha (0.22f));
        g.fillEllipse (dotX - 13.0f, dotY - 13.0f, 26.0f, 26.0f);
        g.setColour (accentColour);
        g.fillEllipse (dotX - 5.5f, dotY - 5.5f, 11.0f, 11.0f);

        if (e.label.isNotEmpty() || e.parameterId.isNotEmpty())
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                        r.reduced (8, 4).withHeight (16),
                        juce::Justification::topLeft, true);
        }
    }

    void PlayerGuiRenderer::drawGranularField (juce::Graphics& g,
                                               juce::Rectangle<int> r,
                                               const LayoutElement& e) const
    {
        const auto backgroundColour = e.backgroundColour.isTransparent()
            ? playerPanel().darker (0.14f)
            : e.backgroundColour;
        const auto borderColour = e.borderColour.isTransparent()
            ? playerBorder()
            : e.borderColour;
        const auto accentColour = e.accentColour.isTransparent()
            ? playerAccent()
            : e.accentColour;

        auto normalisedParameter = [this] (const juce::String& parameterId, float fallback)
        {
            const auto* def = parameterForId (parameterId);
            if (def == nullptr)
                return fallback;

            const float range = juce::jmax (0.0001f, def->max - def->min);
            return juce::jlimit (0.0f, 1.0f,
                (proc.getPackParameterValue (parameterId) - def->min) / range);
        };

        auto rawParameter = [this] (const juce::String& parameterId, float fallback)
        {
            const auto* def = parameterForId (parameterId);
            return def != nullptr ? proc.getPackParameterValue (parameterId) : fallback;
        };

        auto directionName = [] (int direction)
        {
            switch (juce::jlimit (0, 3, direction))
            {
                case 0:  return juce::String ("Forward");
                case 1:  return juce::String ("Reverse");
                case 2:  return juce::String ("Ping-Pong");
                default: return juce::String ("Multi");
            }
        };

        const float position    = normalisedParameter ("sampleStart", 0.0f);
        const float length      = normalisedParameter ("sampleLength", 1.0f);
        const float slice       = normalisedParameter ("sampleSlice", 0.0f);
        const float glitch      = normalisedParameter ("sampleGlitch", 0.0f);
        const float density     = rawParameter ("granularDensity", 24.0f);
        const float grainSizeMs = rawParameter ("granularSizeMs", 90.0f);
        const float spread      = normalisedParameter ("granularSpread", 0.18f);
        const float scan        = rawParameter ("granularScan", 0.0f);
        const float pitchSpray  = normalisedParameter ("granularPitchSpread", 0.0f);
        const float panSpray    = normalisedParameter ("granularPanSpread", 0.45f);
        const float reverse     = normalisedParameter ("granularReverse", 0.0f);
        const float texture     = normalisedParameter ("granularTexture", 0.20f);
        const int maxGrains     = juce::roundToInt (rawParameter ("granularMaxGrains", 16.0f));
        const int direction     = juce::roundToInt (rawParameter ("granularDirection", 3.0f));
        const bool frozen       = rawParameter ("granularFreeze", 0.0f) >= 0.5f;
        const bool granularOn   = rawParameter ("granularOn", 0.0f) >= 0.5f;
        const bool hasSampleControls = parameterForId ("sampleStart") != nullptr
                                    && parameterForId ("sampleLength") != nullptr;

        g.setColour (backgroundColour);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (borderColour);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (12, 10);
        auto header = area.removeFromTop (22);
        g.setColour (accentColour);
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        if (e.label.isNotEmpty())
            g.drawText (e.label.toUpperCase(),
                        header.removeFromLeft (juce::jmin (190, header.getWidth())),
                        juce::Justification::centredLeft, true);

        area.removeFromTop (4);
        if (area.getHeight() < 60 || area.getWidth() < 160)
            return;

        auto chipArea = area.removeFromBottom (24);
        area.removeFromBottom (4);

        g.setColour (borderColour.withAlpha (0.20f));
        for (int i = 1; i < 6; ++i)
        {
            const float x = (float) area.getX() + (float) area.getWidth() * (float) i / 6.0f;
            g.drawVerticalLine (juce::roundToInt (x), (float) area.getY(), (float) area.getBottom());
        }
        for (int i = 1; i < 4; ++i)
        {
            const float y = (float) area.getY() + (float) area.getHeight() * (float) i / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), (float) area.getX(), (float) area.getRight());
        }

        const double seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        float visualPosition = position;
        if (granularOn && ! frozen)
        {
            visualPosition += (float) seconds * scan * 0.055f;
            visualPosition -= std::floor (visualPosition);
        }
        const float playheadX = juce::jmap (visualPosition, 0.0f, 1.0f, (float) area.getX(), (float) area.getRight());
        const float grainSpan = juce::jmap (length, 0.0f, 1.0f, 14.0f, (float) area.getWidth() * 0.54f);
        const float level = juce::jlimit (0.04f, 0.65f, proc.getOutputPeak() + 0.10f);

        juce::ColourGradient glow (accentColour.withAlpha (0.26f), playheadX, (float) area.getCentreY(),
                                   accentColour.withAlpha (0.00f), playheadX + grainSpan, (float) area.getCentreY(),
                                   true);
        g.setGradientFill (glow);
        g.fillEllipse (playheadX - grainSpan * 0.42f,
                       (float) area.getCentreY() - (float) area.getHeight() * 0.34f,
                       grainSpan * 0.84f,
                       (float) area.getHeight() * 0.68f);

        g.setColour (accentColour.withAlpha (0.92f));
        g.drawLine (playheadX, (float) area.getY(), playheadX, (float) area.getBottom(), 1.4f);

        juce::Path cloud;
        const int grainDots = juce::jlimit (18, 72, juce::roundToInt (juce::jmap (density, 0.5f, 220.0f, 18.0f, 72.0f)));
        const float motionPhase = granularOn && ! frozen ? (float) seconds * (0.50f + std::abs (scan) * 0.72f) : 0.0f;
        for (int i = 0; i < grainDots; ++i)
        {
            const float phase = (float) i * 0.49f + slice * 5.0f + motionPhase;
            const float spray = 0.16f + glitch * 0.34f + spread * 0.42f + texture * 0.16f;
            const float directionSign = direction == 1 ? -1.0f
                                      : direction == 2 ? std::sin (motionPhase * 0.75f) >= 0.0f ? 1.0f : -1.0f
                                      : scan < 0.0f ? -1.0f : 1.0f;
            const float x = playheadX
                + std::sin (phase * 1.31f) * grainSpan * (0.18f + spray)
                + directionSign * std::cos (phase * 0.67f) * grainSpan * (0.10f + reverse * 0.16f);
            const float y = (float) area.getCentreY()
                + std::cos (phase * 1.17f + panSpray * 2.0f) * (float) area.getHeight() * (0.10f + spray * 0.32f);
            const float grain = juce::jmap ((float) ((i * 17) % 31) / 30.0f, 1.4f, 3.8f + level * 4.0f + pitchSpray * 1.8f);
            g.setColour (accentColour.withAlpha (juce::jlimit (0.18f, 0.84f, 0.24f + level + (float) (i % 5) * 0.06f)));
            g.fillEllipse (x - grain * 0.5f, y - grain * 0.5f, grain, grain);

            if (i == 0) cloud.startNewSubPath (x, y);
            else        cloud.lineTo (x, y);
        }

        g.setColour (accentColour.withAlpha (0.32f));
        g.strokePath (cloud, juce::PathStrokeType (1.0f));

        juce::Path arrow;
        const float arrowY = (float) area.getBottom() - 13.0f;
        const float arrowW = juce::jmin (70.0f, (float) area.getWidth() * 0.16f);
        const float arrowStart = direction == 1 ? playheadX + arrowW * 0.5f : playheadX - arrowW * 0.5f;
        const float arrowEnd   = direction == 1 ? playheadX - arrowW * 0.5f : playheadX + arrowW * 0.5f;
        g.setColour (accentColour.withAlpha (granularOn ? 0.88f : 0.38f));
        g.drawArrow ({ arrowStart, arrowY, arrowEnd, arrowY },
                     1.8f, 9.0f, 7.0f);
        if (direction == 2 || direction == 3)
            g.drawArrow ({ arrowEnd, arrowY - 9.0f, arrowStart, arrowY - 9.0f },
                         1.4f, 8.0f, 6.0f);

        const juce::StringArray chips { "FWD", "REV", "PING", "MULTI", frozen ? "UNFREEZE" : "FREEZE" };
        const int chipGap = 5;
        const int chipW = juce::jmax (48, (chipArea.getWidth() - chipGap * (chips.size() - 1)) / chips.size());
        for (int i = 0; i < chips.size(); ++i)
        {
            auto chip = juce::Rectangle<int> (chipArea.getX() + i * (chipW + chipGap),
                                              chipArea.getY(), chipW, chipArea.getHeight()).reduced (1);
            const bool active = (i < 4 && i == juce::jlimit (0, 3, direction)) || (i == 4 && frozen);
            g.setColour (active ? accentColour.withAlpha (0.85f) : backgroundColour.brighter (0.08f));
            g.fillRoundedRectangle (chip.toFloat(), 5.0f);
            g.setColour (active ? accentColour : borderColour.withAlpha (0.74f));
            g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), 5.0f, active ? 1.4f : 1.0f);
            g.setColour (active ? juce::Colour (0xff080a0f) : playerTextDim());
            g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
            g.drawText (chips[i], chip, juce::Justification::centred, true);
        }

        juce::ignoreUnused (hasSampleControls, directionName, grainSizeMs, maxGrains);
    }

    void PlayerGuiRenderer::drawPadGrid (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e) const
    {
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty()) return;

        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.08f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                juce::Rectangle<float> pad ((float) inner.getX() + col * (padW + gap),
                                            (float) inner.getY() + row * (padH + gap),
                                            padW, padH);
                const int padIdx = row * cols + col;
                const int note = juce::jlimit (0, 127, e.padBaseNote + padIdx);
                const float triggerLevel = juce::jlimit (0.0f, 1.0f, proc.getNoteHighlightLevel (note));
                const bool active = (note == activePadNote) || triggerLevel > 0.02f;
                const float activeAlpha = juce::jlimit (0.40f, 0.92f, 0.48f + triggerLevel * 0.44f);

                g.setColour (active ? accent.withAlpha (activeAlpha) : bg.brighter (0.04f));
                g.fillRoundedRectangle (pad, juce::jmax (3.0f, e.cornerRadius * 0.6f));
                g.setColour (active ? accent : borderC.withAlpha (0.6f));
                g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), active ? 1.8f : 1.0f);

                g.setColour (active ? juce::Colour (0xff0a0c10) : playerText().withAlpha (0.85f));
                g.setFont (juce::FontOptions (juce::jmin (12.0f, padH * 0.28f)).withStyle ("bold"));
                const juce::String label = e.type == ElementType::DrumPad && e.label.isNotEmpty()
                    ? e.label : juce::String (padIdx + 1);
                g.drawText (label, pad.reduced (4.0f).removeFromTop (padH * 0.55f).toNearestInt(),
                            juce::Justification::centred);
                g.setColour (active ? juce::Colour (0xaa0a0c10) : playerTextDim());
                g.setFont (juce::FontOptions (juce::jmin (10.0f, padH * 0.22f)));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                            pad.reduced (4.0f).removeFromBottom (padH * 0.35f).toNearestInt(),
                            juce::Justification::centred);
            }
        }
    }

    void PlayerGuiRenderer::drawDrumGrid (juce::Graphics& g,
                                          juce::Rectangle<int> r,
                                          const LayoutElement& e) const
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findDrumMachineBlock (pack->dspGraph) : nullptr;
        const int tracks = block != nullptr
            ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) e.drumTracks)))
            : juce::jlimit (1, 16, e.drumTracks);
        const int steps = block != nullptr
            ? juce::jlimit (1, 64, juce::roundToInt (blockValue (*block, "dmSteps", (float) e.drumSteps)))
            : juce::jlimit (1, 64, e.drumSteps);
        const int pattern = block != nullptr
            ? juce::jlimit (0, 7, juce::roundToInt (blockValue (*block, "dmPattern", (float) e.drumPattern)))
            : juce::jlimit (0, 7, e.drumPattern);

        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.06f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour (borderC);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (8);
        auto header = area.removeFromTop (28);
        auto playButton = header.removeFromRight (58).reduced (2);
        auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText ((e.label.isNotEmpty() ? e.label : "DRUM GRID")
                        + "  P" + juce::String (pattern + 1),
                    header, juce::Justification::centredLeft, true);

        const bool playing = proc.isAnyTransportPlaying();
        g.setColour (playing ? accent : playerPanel().brighter (0.08f));
        g.fillRoundedRectangle (playButton.toFloat(), 5.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerBorder());
        g.drawRoundedRectangle (playButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerText().withAlpha (0.92f));
        g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
        g.drawText (playing ? "STOP" : "PLAY", playButton, juce::Justification::centred, true);

        const int bankCount = 8;
        const int bankGap = 3;
        const int bankW = juce::jmax (16, (bankStrip.getWidth() - bankGap * (bankCount - 1)) / bankCount);
        for (int bank = 0; bank < bankCount; ++bank)
        {
            auto chip = juce::Rectangle<int> (bankStrip.getX() + bank * (bankW + bankGap),
                                              bankStrip.getY(),
                                              bankW,
                                              bankStrip.getHeight()).reduced (0, 2);
            const bool activeBank = bank == pattern;
            g.setColour (activeBank ? accent.withAlpha (0.92f) : playerPanel().brighter (0.05f));
            g.fillRoundedRectangle (chip.toFloat(), 4.0f);
            g.setColour (activeBank ? juce::Colour (0xff071014) : playerTextDim());
            g.setFont (juce::FontOptions (8.5f).withStyle ("bold"));
            g.drawText (juce::String (bank + 1), chip, juce::Justification::centred, true);
        }

        area.removeFromTop (4);
        if (area.isEmpty())
            return;

        const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
        auto grid = area.withTrimmedLeft (labelW);
        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        const double playback01 = proc.getSequencerPlaybackPosition01 (steps);
        const int playbackStep = playback01 >= 0.0
            ? juce::jlimit (0, steps - 1, (int) std::floor (playback01 * (double) steps))
            : -1;
        static const char* names[] = { "Kick", "Snare", "Hat", "Clap", "Tom", "Perc", "Ride", "Crash" };

        for (int track = 0; track < tracks; ++track)
        {
            const int y = area.getY() + juce::roundToInt ((float) track * cellH);
            const int h = juce::roundToInt (cellH);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (juce::jlimit (8.0f, 10.5f, cellH * 0.40f)).withStyle ("bold"));
            const juce::String trackLabel = track < 8 ? juce::String (names[track]) : "T" + juce::String (track + 1);
            g.drawText (trackLabel, area.getX(), y, labelW - 5, h, juce::Justification::centredLeft, true);

            for (int step = 0; step < steps; ++step)
            {
                const int x = grid.getX() + juce::roundToInt ((float) step * cellW);
                const auto cell = juce::Rectangle<float> ((float) x + 1.0f,
                                                          (float) y + 1.0f,
                                                          juce::jmax (1.0f, cellW - 2.0f),
                                                          juce::jmax (1.0f, cellH - 2.0f));
                if (step == playbackStep)
                {
                    g.setColour (accent.withAlpha (0.13f));
                    g.fillRoundedRectangle (cell.expanded (0.5f, 0.0f), 2.5f);
                }
                const auto prefix = "dmP" + juce::String (pattern)
                                  + "T" + juce::String (track)
                                  + "S" + juce::String (step);
                const bool active = block != nullptr && blockValue (*block, prefix + "On", 0.0f) >= 0.5f;
                const float velocity = block != nullptr
                    ? juce::jlimit (0.1f, 1.0f, blockValue (*block, prefix + "Vel", 0.8f))
                    : 0.75f;
                const int divisions = block != nullptr
                    ? juce::jlimit (1, 4, juce::roundToInt (blockValue (*block, prefix + "Div", 1.0f)))
                    : 1;

                g.setColour ((step % 4 == 0 ? playerPanel().brighter (0.06f) : playerPanel()).withAlpha (0.90f));
                g.fillRoundedRectangle (cell, 2.5f);
                if (divisions > 1)
                {
                    g.setColour (playerBorder().brighter (0.25f).withAlpha (0.70f));
                    for (int division = 1; division < divisions; ++division)
                    {
                        const float x = cell.getX() + cell.getWidth() * (float) division / (float) divisions;
                        g.drawVerticalLine (juce::roundToInt (x), cell.getY() + 2.0f, cell.getBottom() - 2.0f);
                    }
                }
                if (active)
                {
                    auto hit = cell.reduced (2.0f);
                    hit.removeFromTop (hit.getHeight() * (1.0f - velocity));
                    g.setColour (accent.withAlpha (0.68f + velocity * 0.28f));
                    g.fillRoundedRectangle (hit, 2.0f);
                    if (divisions > 1 && cell.getWidth() >= 14.0f && cell.getHeight() >= 12.0f)
                    {
                        g.setColour (juce::Colour (0xff0a0c10));
                        g.setFont (juce::FontOptions (juce::jmin (9.0f, cell.getHeight() * 0.48f)).withStyle ("bold"));
                        g.drawText ("x" + juce::String (divisions), cell.toNearestInt().reduced (1),
                                    juce::Justification::centred, true);
                    }
                }
            }
        }

        if (playback01 >= 0.0)
        {
            const float playheadX = (float) grid.getX()
                + (float) playback01 * (float) grid.getWidth();
            g.setColour (accent.withAlpha (0.95f));
            g.drawLine (playheadX, (float) grid.getY(), playheadX, (float) grid.getBottom(), 2.0f);
            g.setColour (accent.withAlpha (0.28f));
            g.fillRoundedRectangle (playheadX - 4.0f, (float) header.getY(), 8.0f,
                                    (float) (grid.getBottom() - header.getY()), 4.0f);
        }
    }

    void PlayerGuiRenderer::drawArpLane (juce::Graphics& g,
                                         juce::Rectangle<int> r,
                                         const LayoutElement& e) const
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findArpBlock (pack->dspGraph) : nullptr;
        const int lane = juce::jlimit (0, 15, e.arpLaneIndex);
        const int activeLane = block != nullptr
            ? juce::jlimit (0, 15, juce::roundToInt (blockValue (*block, "mpActiveBank", 0.0f)))
            : 0;
        const bool laneSelected = lane == activeLane;
        const bool orbitMultiRing = e.arpLaneMode.equalsIgnoreCase ("multiRing")
                                 || e.arpLaneMode.equalsIgnoreCase ("orbit")
                                 || e.arpLaneMode.equalsIgnoreCase ("orbitMulti");
        const int steps = block != nullptr
            ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) e.arpLaneSteps)))
            : juce::jlimit (1, 128, e.arpLaneSteps);

        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.08f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
        g.setColour (laneSelected ? accent : borderC);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), laneSelected ? 2.0f : 1.0f);

        auto area = r.reduced (10);
        auto header = area.removeFromTop (26);
        const auto dragHandle = arpLaneMidiDragHandleBounds (r);
        const auto playButton = arpLanePlayButtonBounds (r);
        header.removeFromRight (120);
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.setColour (accent);
        g.drawText (juce::String (lane + 1), header.removeFromLeft (24), juce::Justification::centredLeft, true);
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "ARP LANE",
                    header.removeFromLeft (120), juce::Justification::centredLeft, true);
        g.setColour (laneSelected ? accent : playerTextDim());
        g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
        g.drawText (laneSelected ? "ACTIVE" : "CLICK TO SELECT", header, juce::Justification::centredRight, true);
        g.setColour (playerPanel().brighter (0.10f).withAlpha (0.94f));
        g.fillRoundedRectangle (dragHandle.toFloat(), 6.0f);
        g.setColour (accent.withAlpha (0.82f));
        g.drawRoundedRectangle (dragHandle.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (playerText());
        g.setFont (juce::FontOptions (7.8f).withStyle ("bold"));
        g.drawText ("DRAG MIDI", dragHandle, juce::Justification::centred, true);
        g.setColour (proc.isAnyTransportPlaying() ? accent.withAlpha (0.95f) : playerPanel().brighter (0.10f).withAlpha (0.94f));
        g.fillRoundedRectangle (playButton.toFloat(), 6.0f);
        g.setColour (accent.withAlpha (0.82f));
        g.drawRoundedRectangle (playButton.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (proc.isAnyTransportPlaying() ? playerBg() : playerText());
        g.setFont (juce::FontOptions (7.8f).withStyle ("bold"));
        g.drawText (proc.isAnyTransportPlaying() ? "STOP" : "PLAY", playButton, juce::Justification::centred, true);

        if (orbitMultiRing)
        {
            const int laneCount = 5;
            const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
            if (size <= 24.0f)
                return;

            const juce::Point<float> centre ((float) area.getCentreX(),
                                             (float) area.getY() + size * 0.52f);
            const float radius = size * 0.42f;
            const float innerRadius = radius * 0.25f;
            const float outerRadius = radius * 0.94f;
            const float band = (outerRadius - innerRadius) / (float) laneCount;
            const float laneAlpha = proc.isAnyTransportPlaying() ? 0.88f : 0.66f;

            g.setColour (borderC.withAlpha (0.22f));
            for (int spoke = 0; spoke < 16; ++spoke)
            {
                const float angle = -juce::MathConstants<float>::halfPi
                    + juce::MathConstants<float>::twoPi * (float) spoke / 16.0f;
                const auto start = centre + juce::Point<float> (std::cos (angle) * innerRadius,
                                                                std::sin (angle) * innerRadius);
                const auto end = centre + juce::Point<float> (std::cos (angle) * outerRadius,
                                                              std::sin (angle) * outerRadius);
                g.drawLine (start.x, start.y, end.x, end.y, spoke % 4 == 0 ? 1.0f : 0.55f);
            }

            for (int ringLane = 0; ringLane < laneCount; ++ringLane)
            {
                const int laneToDraw = ringLane;
                const int laneSteps = block != nullptr
                    ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, laneToDraw, "arpSteps", (float) e.arpLaneSteps)))
                    : juce::jlimit (1, 128, e.arpLaneSteps);
                const int drawSteps = juce::jmin (laneSteps, 64);
                const float ringRadius = innerRadius + band * ((float) ringLane + 0.5f);
                const bool activeRing = laneToDraw == activeLane;
                const auto ringColour = activeRing ? accent : accent.interpolatedWith (borderC, 0.45f + 0.08f * (float) ringLane);
                const double playback01 = activeRing ? proc.getSequencerPlaybackPosition01 (laneSteps) : -1.0;
                const int playbackStep = playback01 >= 0.0
                    ? juce::jlimit (0, drawSteps - 1, (int) std::floor (playback01 * (double) drawSteps))
                    : -1;

                g.setColour ((activeRing ? ringColour : borderC).withAlpha (activeRing ? 0.82f : 0.28f));
                g.drawEllipse (centre.x - ringRadius, centre.y - ringRadius,
                               ringRadius * 2.0f, ringRadius * 2.0f, activeRing ? 2.0f : 0.9f);

                for (int stepIndex = 0; stepIndex < drawSteps; ++stepIndex)
                {
                    const float active = block != nullptr
                        ? arpLaneValue (*block, laneToDraw, "mpStep" + juce::String (stepIndex) + "On", stepIndex % 2 == 0 ? 1.0f : 0.0f)
                        : (stepIndex % 3 == 0 ? 1.0f : 0.0f);
                    const float velocity = block != nullptr
                        ? juce::jlimit (0.05f, 1.0f, arpLaneValue (*block, laneToDraw, "mpVelocity" + juce::String (stepIndex), 0.74f))
                        : 0.72f;
                    const float angle = -juce::MathConstants<float>::halfPi
                        + juce::MathConstants<float>::twoPi * (float) stepIndex / (float) drawSteps;
                    const float rOffset = juce::jmap (velocity, 0.0f, 1.0f, -band * 0.34f, band * 0.34f);
                    const auto p = centre + juce::Point<float> (std::cos (angle) * (ringRadius + rOffset),
                                                                std::sin (angle) * (ringRadius + rOffset));
                    const float dot = (stepIndex == playbackStep ? 4.4f : 2.5f) + velocity * (activeRing ? 2.4f : 1.2f);
                    if (active >= 0.5f)
                    {
                        g.setColour (ringColour.withAlpha ((activeRing ? laneAlpha : 0.48f) * (0.5f + velocity * 0.5f)));
                        g.fillEllipse (p.x - dot, p.y - dot, dot * 2.0f, dot * 2.0f);
                    }
                    else if (activeRing)
                    {
                        g.setColour (borderC.withAlpha (0.18f));
                        g.drawEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f, 0.7f);
                    }
                }
            }

            g.setColour (playerText());
            g.setFont (juce::FontOptions (21.0f).withStyle ("bold"));
            g.drawText ("ORBIT", juce::Rectangle<int> ((int) centre.x - 45, (int) centre.y - 22, 90, 24),
                        juce::Justification::centred, true);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (8.5f).withStyle ("bold"));
            g.drawText ("LANE " + juce::String (activeLane + 1) + " ACTIVE",
                        juce::Rectangle<int> ((int) centre.x - 48, (int) centre.y + 3, 96, 18),
                        juce::Justification::centred, true);

            auto footer = r.reduced (10).removeFromBottom (30);
            const juce::String footerLabels[] = { "PITCH", "FILTER", "PAN", "FX", "SLICE" };
            for (int i = 0; i < 5; ++i)
            {
                const int cellW = footer.getWidth() / (5 - i);
                auto cell = footer.removeFromLeft (cellW).reduced (2, 1);
                g.setColour (i == activeLane ? accent : playerTextDim());
                g.setFont (juce::FontOptions (7.3f).withStyle ("bold"));
                g.drawText (footerLabels[i], cell, juce::Justification::centred, true);
            }
            return;
        }

        const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 34);
        const juce::Point<float> centre ((float) area.getCentreX(),
                                         (float) area.getY() + size * 0.52f);
        const float radius = size * 0.40f;
        const float innerRadius = radius * 0.70f;
        const float noteRadius = radius * 1.09f;
        const int maxDrawSteps = juce::jmin (steps, 64);
        const int slotCount = juce::jlimit (1, 12, e.arpLaneSampleSlots);
        const bool multiRing = slotCount > 1 && e.arpLaneTarget != "notes";

        g.setColour (borderC.withAlpha (0.75f));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        g.setColour (borderC.withAlpha (0.20f));
        g.drawEllipse (centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);
        if (multiRing)
        {
            for (int slot = 0; slot < slotCount; ++slot)
            {
                const float rr = juce::jmap ((float) slot, 0.0f, (float) juce::jmax (1, slotCount - 1),
                                             radius * 0.32f, radius * 0.93f);
                g.setColour (borderC.withAlpha (slot == 0 ? 0.28f : 0.16f));
                g.drawEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f, 0.7f);
            }
        }

        static const char* noteLabels[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setFont (juce::FontOptions (8.0f));
        for (int note = 0; note < 12; ++note)
        {
            const float angle = -juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::twoPi * (float) note / 12.0f;
            const auto p = centre + juce::Point<float> (std::cos (angle) * noteRadius,
                                                        std::sin (angle) * noteRadius);
            g.setColour (playerTextDim());
            g.drawText (noteLabels[note], juce::Rectangle<int> ((int) p.x - 12, (int) p.y - 6, 24, 12),
                        juce::Justification::centred, true);
        }

        const double playback01 = laneSelected ? proc.getSequencerPlaybackPosition01 (steps) : -1.0;
        const int playbackStep = playback01 >= 0.0
            ? juce::jlimit (0, maxDrawSteps - 1, (int) std::floor (playback01 * (double) maxDrawSteps))
            : -1;

        for (int step = 0; step < maxDrawSteps; ++step)
        {
            const float active = block != nullptr
                ? arpLaneValue (*block, lane, "mpStep" + juce::String (step) + "On", step % 2 == 0 ? 1.0f : 0.0f)
                : (step % 3 == 0 ? 1.0f : 0.0f);
            const float velocity = block != nullptr
                ? juce::jlimit (0.15f, 1.0f, arpLaneValue (*block, lane, "mpVelocity" + juce::String (step), 0.74f))
                : 0.72f;
            const int divisions = block != nullptr
                ? juce::jlimit (1, 8, juce::roundToInt (arpLaneValue (*block, lane, "mpStepDiv" + juce::String (step), 1.0f)))
                : 1;
            const int slot = multiRing
                ? juce::jlimit (0, slotCount - 1,
                    block != nullptr && e.arpLaneTarget == "loops"
                        ? juce::roundToInt (arpLaneValue (*block, lane, "mpSampleSlice" + juce::String (step), (float) (step % slotCount)))
                        : (step + e.arpLaneRotate) % slotCount)
                : 0;
            const float angle = -juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::twoPi * (float) step / (float) maxDrawSteps;
            const float activeRadius = multiRing
                ? juce::jmap ((float) slot, 0.0f, (float) juce::jmax (1, slotCount - 1), radius * 0.32f, radius * 0.93f)
                : juce::jmap (velocity, 0.0f, 1.0f, radius * 0.25f, radius * 0.93f);
            const auto outer = centre + juce::Point<float> (std::cos (angle) * radius,
                                                            std::sin (angle) * radius);
            const auto gridStart = centre + juce::Point<float> (std::cos (angle) * radius * 0.18f,
                                                                std::sin (angle) * radius * 0.18f);
            const auto velocityEnd = centre + juce::Point<float> (std::cos (angle) * activeRadius,
                                                                  std::sin (angle) * activeRadius);
            g.setColour (borderC.withAlpha (0.20f));
            g.drawLine (gridStart.x, gridStart.y, outer.x, outer.y, 0.7f);
            if (active >= 0.5f)
            {
                const float dotSize = (step == playbackStep ? 6.0f : 3.0f) + velocity * 4.0f;
                g.setColour (accent.withAlpha (step == playbackStep ? 0.88f : 0.58f));
                g.drawLine (centre.x, centre.y, velocityEnd.x, velocityEnd.y, step == playbackStep ? 2.3f : 1.4f);
                g.setColour (accent.withAlpha (step == playbackStep ? 0.42f : 0.24f));
                g.fillEllipse (velocityEnd.x - dotSize, velocityEnd.y - dotSize, dotSize * 2.0f, dotSize * 2.0f);
                g.setColour (accent);
                g.fillEllipse (velocityEnd.x - dotSize * 0.42f, velocityEnd.y - dotSize * 0.42f, dotSize * 0.84f, dotSize * 0.84f);
                if (divisions > 1)
                {
                    const auto badge = centre + juce::Point<float> (std::cos (angle) * radius * 1.06f,
                                                                    std::sin (angle) * radius * 1.06f);
                    g.setColour (playerBg().withAlpha (0.92f));
                    g.fillEllipse (badge.x - 6.5f, badge.y - 6.5f, 13.0f, 13.0f);
                    g.setColour (accent);
                    g.drawEllipse (badge.x - 6.5f, badge.y - 6.5f, 13.0f, 13.0f, 1.2f);
                    g.setFont (juce::FontOptions (7.2f).withStyle ("bold"));
                    g.drawText (juce::String (divisions),
                                juce::Rectangle<int> ((int) badge.x - 7, (int) badge.y - 7, 14, 14),
                                juce::Justification::centred, true);
                }
            }
        }

        const auto targetLabel = e.arpLaneTarget == "drums" ? "DRUMS"
                              : e.arpLaneTarget == "oneShots" ? "ONE SHOTS"
                              : e.arpLaneTarget == "loops" ? "LOOP SLICES"
                              : e.arpLaneTarget == "samples" ? "SAMPLES" : "NOTES";

        g.setColour (playerText());
        g.setFont (juce::FontOptions (24.0f));
        g.drawText (juce::String (steps), juce::Rectangle<int> ((int) centre.x - 44, (int) centre.y - 22, 88, 30),
                    juce::Justification::centred, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
        g.drawText (targetLabel, juce::Rectangle<int> ((int) centre.x - 44, (int) centre.y + 7, 88, 18),
                    juce::Justification::centred, true);

        auto footer = r.reduced (10).removeFromBottom (30);
        const juce::String footerLabels[] =
        {
            e.arpLaneDirection.toUpperCase().substring (0, 4),
            "PUL " + juce::String (e.arpLaneEuclideanPulses),
            "RAT " + juce::String (e.arpLaneRatchet),
            "FIL " + juce::String (e.arpLaneFillPulses)
        };
        for (int i = 0; i < 4; ++i)
        {
            const int cellW = footer.getWidth() / (4 - i);
            auto cell = footer.removeFromLeft (cellW).reduced (3, 1);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (7.6f).withStyle ("bold"));
            g.drawText (footerLabels[i], cell.removeFromTop (11), juce::Justification::centred, true);
            g.setColour (accent.withAlpha (0.88f));
            const auto bar = cell.withHeight (4).withCentre (juce::Point<int> (cell.getCentreX(), cell.getCentreY()));
            g.fillRoundedRectangle (bar.toFloat(), 2.0f);
        }
    }

    void PlayerGuiRenderer::drawMixer (juce::Graphics& g,
                                       juce::Rectangle<int> r,
                                       const LayoutElement& e) const
    {
        const bool wantsLayerMode = (e.mixerMode == "layers")
            || (e.mixerMode == "auto" && proc.isMultiInstrumentPack() && proc.getMultiLayerCount() > 1);
        const bool layerMode = wantsLayerMode && proc.getMultiLayerCount() > 0;
        const int channelCount = juce::jlimit (1, 16, layerMode ? proc.getMultiLayerCount()
                                                                : e.mixerChannels);
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.06f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        g.setColour (bg.withAlpha (0.94f));
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (6.0f, e.cornerRadius));
        g.setColour (borderC.withAlpha (0.82f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (6.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        if (e.labelPosition != "hidden")
        {
            auto header = area.removeFromTop (22);
            g.setColour (accent);
            g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
            g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MIXER",
                        header.removeFromLeft (juce::jlimit (90, 190, header.getWidth() / 3)),
                        juce::Justification::centredLeft, true);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (layerMode ? "Layer mixer - volume, pan, mute, solo"
                                  : "Output / parameter mixer",
                        header, juce::Justification::centredRight, true);
            area.removeFromTop (6);
        }
        if (area.isEmpty())
            return;

        const int stripW = juce::jmax (38, area.getWidth() / channelCount);
        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto strip = juce::Rectangle<int> (area.getX() + channel * stripW,
                                               area.getY(),
                                               channel == channelCount - 1
                                                    ? area.getRight() - (area.getX() + channel * stripW)
                                                    : stripW,
                                               area.getHeight()).reduced (3, 0);
            if (strip.getWidth() <= 12)
                continue;

            const juce::String volumeParam = layerMode ? juce::String()
                : stringAtOr (e.mixerVolumeParams, channel, channel == 0 ? "volume" : juce::String());
            const juce::String panParam = layerMode ? juce::String()
                : stringAtOr (e.mixerPanParams, channel, channel == 0 ? "pan" : juce::String());
            const juce::String muteParam = layerMode ? juce::String()
                : stringAtOr (e.mixerMuteParams, channel, {});
            const juce::String soloParam = layerMode ? juce::String()
                : stringAtOr (e.mixerSoloParams, channel, {});
            const auto* volumeDef = parameterForId (volumeParam);
            const auto* panDef = parameterForId (panParam);
            const bool assigned = layerMode || volumeDef != nullptr || panDef != nullptr;

            const juce::String label = layerMode ? proc.getMultiLayerName (channel)
                : stringAtOr (e.mixerChannelLabels, channel, channel == 0 ? "Main" : "Assign");
            const float volume = layerMode
                ? juce::jlimit (0.0f, 1.0f, proc.getMultiLayerVolume (channel))
                : [&]() -> float
                  {
                      if (const auto* def = volumeDef)
                      {
                          const float range = juce::jmax (0.0001f, def->max - def->min);
                          return juce::jlimit (0.0f, 1.0f, (proc.getPackParameterValue (volumeParam) - def->min) / range);
                      }
                      return 0.0f;
                  }();
            const float pan = layerMode
                ? juce::jlimit (-1.0f, 1.0f, proc.getMultiLayerPan (channel))
                : [&]() -> float
                  {
                      if (const auto* def = panDef)
                      {
                          const float range = juce::jmax (0.0001f, def->max - def->min);
                          return juce::jlimit (-1.0f, 1.0f,
                              juce::jmap (proc.getPackParameterValue (panParam), def->min, def->max, -1.0f, 1.0f));
                      }
                      return 0.0f;
                  }();
            const bool muted = layerMode ? proc.getMultiLayerMuted (channel)
                : (parameterForId (muteParam) != nullptr && proc.getPackParameterValue (muteParam) >= 0.5f);
            const bool soloed = layerMode ? proc.getMultiLayerSoloed (channel)
                : (parameterForId (soloParam) != nullptr && proc.getPackParameterValue (soloParam) >= 0.5f);

            g.setColour ((assigned ? playerPanel() : playerPanel().darker (0.18f)).withAlpha (0.72f));
            g.fillRoundedRectangle (strip.toFloat(), 5.0f);
            g.setColour ((assigned ? borderC : borderC.withAlpha (0.35f)));
            g.drawRoundedRectangle (strip.toFloat().reduced (0.5f), 5.0f, 1.0f);

            auto nameArea = strip.removeFromTop (30);
            g.setColour (assigned ? playerText().withAlpha (0.92f) : playerTextDim().withAlpha (0.55f));
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (label, nameArea.removeFromTop (15).reduced (3, 0), juce::Justification::centred, true);
            g.setColour (assigned ? playerTextDim().withAlpha (0.82f) : playerTextDim().withAlpha (0.42f));
            g.setFont (juce::FontOptions (8.5f));
            const auto mappedName = layerMode ? juce::String ("Layer")
                : volumeDef != nullptr ? (volumeDef->name.isNotEmpty() ? volumeDef->name : volumeDef->id)
                : panDef != nullptr ? (panDef->name.isNotEmpty() ? panDef->name : panDef->id)
                : (volumeParam.isNotEmpty() ? "Missing: " + volumeParam
                                            : panParam.isNotEmpty() ? "Missing: " + panParam
                                                                    : "Unassigned");
            g.drawText (mappedName, nameArea.reduced (3, 0), juce::Justification::centred, true);

            auto buttons = strip.removeFromBottom (18).reduced (2, 1);
            auto ledArea = strip.removeFromBottom (16).reduced (5, 2);
            auto panArea = strip.removeFromBottom (18).reduced (5, 3);
            auto valueArea = strip.removeFromBottom (14);
            auto faderArea = strip.reduced (4, 3);
            const int faderCentre = faderArea.getCentreX();
            const int trackWidth = juce::jlimit (10, 18, faderArea.getWidth() / 5);
            const auto faderTrack = juce::Rectangle<int> (faderCentre - trackWidth / 2,
                                                          faderArea.getY() + 1,
                                                          trackWidth,
                                                          juce::jmax (32, faderArea.getHeight() - 2));
            const int thumbY = juce::jmap (volume, 0.0f, 1.0f,
                                           (float) faderTrack.getBottom(),
                                           (float) faderTrack.getY());
            float meterValue = layerMode
                ? juce::jlimit (0.0f, 1.0f, (float) proc.getMultiLayerActiveVoiceCount (channel) / 8.0f)
                : proc.getOutputPeak();
            meterValue = juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jlimit (0.0f, 1.0f, meterValue)) * volume);
            if (muted)
                meterValue = 0.0f;

            auto meter = faderTrack.translated (-trackWidth - 7, 0).withWidth (5);
            g.setColour (playerBg().withAlpha (0.72f));
            g.fillRoundedRectangle (faderTrack.toFloat(), 4.0f);
            g.setColour (accent.withAlpha (assigned ? 0.72f : 0.22f));
            g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (
                                        faderTrack.getX(),
                                        juce::jlimit (faderTrack.getY(), faderTrack.getBottom(),
                                                      thumbY),
                                        faderTrack.getRight(),
                                        faderTrack.getBottom()).toFloat(), 4.0f);
            g.setColour (playerBorder().withAlpha (0.55f));
            g.drawRoundedRectangle (faderTrack.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (assigned ? playerText().brighter (0.15f) : playerTextDim().withAlpha (0.42f));
            const float thumbWidth = juce::jmin (30.0f, (float) faderArea.getWidth() - 8.0f);
            g.fillRoundedRectangle (juce::Rectangle<float> ((float) faderCentre - thumbWidth * 0.5f,
                                                            (float) thumbY - 5.0f,
                                                            thumbWidth, 10.0f), 4.0f);

            g.setColour (playerBg().brighter (0.08f));
            g.fillRoundedRectangle (meter.toFloat(), 2.0f);
            g.setColour (accent.withAlpha (0.70f));
            g.fillRoundedRectangle (meter.withTrimmedTop (
                                        juce::roundToInt ((1.0f - meterValue) * (float) meter.getHeight())).toFloat(), 2.0f);

            if (! ledArea.isEmpty())
            {
                g.setColour (playerBg().brighter (0.04f).withAlpha (0.86f));
                g.fillRoundedRectangle (ledArea.toFloat(), 3.0f);

                constexpr int segmentCount = 10;
                const int gap = 2;
                const int available = ledArea.getWidth() - gap * (segmentCount - 1);
                const int segmentW = juce::jmax (2, available / segmentCount);
                int x = ledArea.getX();
                for (int segment = 0; segment < segmentCount; ++segment)
                {
                    const float threshold = (float) (segment + 1) / (float) segmentCount;
                    auto segmentBounds = juce::Rectangle<int> (x, ledArea.getY(), segmentW, ledArea.getHeight()).reduced (0, 2);
                    auto segmentColour = segment >= 8 ? juce::Colour (0xffff4a48)
                                        : segment >= 6 ? juce::Colour (0xffffb22a)
                                                       : juce::Colour (0xff52e58c);
                    g.setColour (segmentColour.withAlpha (assigned && meterValue >= threshold ? 0.95f : 0.16f));
                    g.fillRoundedRectangle (segmentBounds.toFloat(), 1.5f);
                    x += segmentW + gap;
                }

                g.setColour (playerBorder().withAlpha (0.45f));
                g.drawRoundedRectangle (ledArea.toFloat().reduced (0.5f), 3.0f, 1.0f);
            }

            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (juce::String (juce::roundToInt (volume * 100.0f)),
                        valueArea, juce::Justification::centred, true);

            if (! panArea.isEmpty())
            {
                g.setColour (playerBg().withAlpha (0.72f));
                g.fillRoundedRectangle (panArea.toFloat(), 3.0f);
                const int centreX = panArea.getCentreX();
                const int panX = panArea.getX() + juce::roundToInt ((pan + 1.0f) * 0.5f * (float) panArea.getWidth());
                g.setColour (accent.withAlpha (assigned ? 0.65f : 0.20f));
                g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (juce::jmin (centreX, panX),
                                                                                  panArea.getY(),
                                                                                  juce::jmax (centreX, panX),
                                                                                  panArea.getBottom()).toFloat(), 3.0f);
                g.setColour (playerTextDim());
                g.drawVerticalLine (centreX, (float) panArea.getY(), (float) panArea.getBottom());
            }

            auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0);
            auto solo = buttons.reduced (1, 0);
            auto drawSwitch = [&] (juce::Rectangle<int> bounds, juce::String text, bool on, bool enabled)
            {
                g.setColour (on ? accent : playerBg().brighter (0.08f));
                if (! enabled)
                    g.setColour (playerBg().brighter (0.02f));
                g.fillRoundedRectangle (bounds.toFloat(), 3.0f);
                g.setColour (on ? juce::Colour (0xff0a0c10) : playerTextDim().withAlpha (enabled ? 0.90f : 0.35f));
                g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
                g.drawText (text, bounds, juce::Justification::centred, true);
            };
            drawSwitch (mute, "M", muted, layerMode || muteParam.isNotEmpty());
            drawSwitch (solo, "S", soloed, layerMode || soloParam.isNotEmpty());
        }
    }

    void PlayerGuiRenderer::drawMultiLayerDock (juce::Graphics& g, juce::Rectangle<int> canvas)
    {
        multiLayerDockBounds = {};
        multiLayerToggleBounds = {};
        multiLayerMuteBounds.clear();
        multiLayerSoloBounds.clear();
        multiLayerVolumeBounds.clear();
        multiLayerPanBounds.clear();
        multiLayerCollapseBounds.clear();

        const int layerCount = proc.getMultiLayerCount();
        if (! proc.isMultiInstrumentPack() || layerCount <= 1 || canvas.isEmpty())
            return;

        if ((int) multiLayerRowCollapsed.size() != layerCount)
            multiLayerRowCollapsed.assign ((size_t) layerCount, false);

        multiLayerMuteBounds.resize ((size_t) layerCount);
        multiLayerSoloBounds.resize ((size_t) layerCount);
        multiLayerVolumeBounds.resize ((size_t) layerCount);
        multiLayerPanBounds.resize ((size_t) layerCount);
        multiLayerCollapseBounds.resize ((size_t) layerCount);

        int visibleRows = 0;
        int dockHeight = 38;
        if (! multiLayerDockCollapsed)
        {
            const int maxHeight = juce::jlimit (122, juce::jmax (122, canvas.getHeight() - 42),
                                                juce::roundToInt ((float) canvas.getHeight() * 0.42f));
            dockHeight = 44;
            for (int i = 0; i < layerCount; ++i)
            {
                const int rowHeight = multiLayerRowCollapsed[(size_t) i] ? 28 : 58;
                if (visibleRows > 0 && dockHeight + rowHeight > maxHeight)
                    break;
                dockHeight += rowHeight;
                ++visibleRows;
            }
        }
        multiLayerDockBounds = canvas.reduced (14).withHeight (dockHeight).withY (canvas.getY() + 12);

        g.setColour (playerPanel().withAlpha (0.88f));
        g.fillRoundedRectangle (multiLayerDockBounds.toFloat(), 10.0f);
        g.setColour (playerBorder().withAlpha (0.80f));
        g.drawRoundedRectangle (multiLayerDockBounds.toFloat().reduced (0.5f), 10.0f, 1.0f);

        auto area = multiLayerDockBounds.reduced (12, 7);
        auto header = area.removeFromTop (24);
        multiLayerToggleBounds = header.removeFromRight (78).reduced (2, 1);

        g.setColour (multiLayerDockCollapsed ? playerAccent().withAlpha (0.85f)
                                             : playerBorder().brighter (0.10f));
        g.fillRoundedRectangle (multiLayerToggleBounds.toFloat(), 6.0f);
        g.setColour (playerText().brighter (0.2f));
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText (multiLayerDockCollapsed ? "Layers" : "Hide",
                    multiLayerToggleBounds, juce::Justification::centred, true);

        g.setColour (playerAccent());
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.drawText ("Multi Instrument", header.removeFromLeft (132), juce::Justification::centredLeft, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (layerCount) + " synced layers"
                    + (multiLayerDockCollapsed ? " - open rack to mix, mute, solo, and compact layers" : ""),
                    header, juce::Justification::centredLeft, true);

        if (multiLayerDockCollapsed)
            return;

        area.removeFromTop (4);
        for (int i = 0; i < visibleRows; ++i)
        {
            const bool rowCollapsed = multiLayerRowCollapsed[(size_t) i];
            auto row = area.removeFromTop (rowCollapsed ? 26 : 56).reduced (0, 1);
            g.setColour (playerBg().withAlpha (0.62f));
            g.fillRoundedRectangle (row.toFloat(), 5.0f);

            auto collapse = row.removeFromLeft (42).reduced (3, rowCollapsed ? 4 : 16);
            auto solo = row.removeFromRight (30).reduced (2, 3);
            auto mute = row.removeFromRight (30).reduced (2, 3);
            row.removeFromRight (8);
            auto stats = row.removeFromRight (juce::jlimit (156, 240, row.getWidth() / 2));
            row.removeFromRight (8);

            multiLayerCollapseBounds[(size_t) i] = collapse;
            multiLayerMuteBounds[(size_t) i] = mute;
            multiLayerSoloBounds[(size_t) i] = solo;

            const bool muted = proc.getMultiLayerMuted (i);
            const bool soloed = proc.getMultiLayerSoloed (i);
            const float volume = juce::jlimit (0.0f, 1.0f, proc.getMultiLayerVolume (i));
            const float pan = juce::jlimit (-1.0f, 1.0f, proc.getMultiLayerPan (i));

            g.setColour (playerText().brighter (0.1f));
            g.setFont (juce::FontOptions (11.5f).withStyle ("bold"));
            const auto nameLine = row.withHeight (rowCollapsed ? row.getHeight() : 24);
            g.drawText (proc.getMultiLayerName (i), nameLine.reduced (8, 0), juce::Justification::centredLeft, true);

            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (10.5f));

            if (rowCollapsed)
            {
                multiLayerVolumeBounds[(size_t) i] = {};
                multiLayerPanBounds[(size_t) i] = {};
                g.setColour (playerTextDim());
                g.drawText ("VOL " + juce::String (juce::roundToInt (volume * 100.0f))
                            + "   PAN " + juce::String (juce::roundToInt (pan * 100.0f)),
                            stats, juce::Justification::centredLeft, true);
            }
            else
            {
                auto statsLine = stats.removeFromTop (24);
                g.setColour (playerTextDim());
                g.drawText (juce::String (proc.getMultiLayerActiveVoiceCount (i)) + " voices  |  "
                            + juce::String (proc.getMultiLayerLoadedSampleCount (i)) + " samples",
                            statsLine, juce::Justification::centredLeft, true);
            }

            auto drawBar = [&] (juce::Rectangle<int> bounds, juce::String label,
                                float normalised, bool bipolar)
            {
                if (bounds.isEmpty())
                    return;

                g.setColour (playerPanel().brighter (0.08f));
                g.fillRoundedRectangle (bounds.toFloat(), 3.0f);
                if (bipolar)
                {
                    const int centre = bounds.getCentreX();
                    const int x = bounds.getX() + juce::roundToInt (normalised * (float) bounds.getWidth());
                    g.setColour (playerAccent().withAlpha (0.78f));
                    g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (juce::jmin (centre, x),
                                                                                      bounds.getY(),
                                                                                      juce::jmax (centre, x),
                                                                                      bounds.getBottom()).toFloat(),
                                            3.0f);
                }
                else
                {
                    g.setColour (playerAccent().withAlpha (0.78f));
                    g.fillRoundedRectangle (bounds.withWidth (juce::roundToInt (normalised * (float) bounds.getWidth())).toFloat(), 3.0f);
                }

                g.setColour (playerBorder().withAlpha (0.8f));
                g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 3.0f, 1.0f);
                g.setColour (playerTextDim());
                g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
                g.drawText (label, bounds, juce::Justification::centred, true);
            };

            if (! rowCollapsed)
            {
                auto barLine = stats.reduced (0, 3);
                auto volumeBar = barLine.removeFromLeft (juce::jmax (64, barLine.getWidth() / 2 - 4)).reduced (2, 4);
                auto panBar = barLine.reduced (2, 4);
                multiLayerVolumeBounds[(size_t) i] = volumeBar;
                multiLayerPanBounds[(size_t) i] = panBar;
                drawBar (volumeBar, "VOL " + juce::String (juce::roundToInt (volume * 100.0f)), volume, false);
                drawBar (panBar, "PAN " + juce::String (juce::roundToInt (pan * 100.0f)), (pan + 1.0f) * 0.5f, true);
            }

            auto drawSwitch = [&] (juce::Rectangle<int> bounds, juce::String text, bool active)
            {
                g.setColour (active ? playerAccent() : playerPanel().brighter (0.10f));
                g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
                g.setColour (active ? juce::Colour (0xff0a0c10) : playerTextDim());
                g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
                g.drawText (text, bounds, juce::Justification::centred, true);
            };
            drawSwitch (collapse, rowCollapsed ? "EDIT" : "MIN", false);
            drawSwitch (mute, "M", muted);
            drawSwitch (solo, "S", soloed);
        }

        if (visibleRows < layerCount)
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ("+" + juce::String (layerCount - visibleRows) + " more layers",
                        area.removeFromTop (18), juce::Justification::centredLeft, true);
        }
    }

    int PlayerGuiRenderer::padNoteAt (const LayoutElement& e,
                                      juce::Rectangle<int> r,
                                      juce::Point<int> pos) const
    {
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : 4;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty() || ! inner.contains (pos)) return -1;

        const float padW = (float) (inner.getWidth()  - gap * (cols - 1)) / (float) cols;
        const float padH = (float) (inner.getHeight() - gap * (rows - 1)) / (float) rows;
        if (padW <= 0.0f || padH <= 0.0f) return -1;
        const int col = juce::jlimit (0, cols - 1, (int) ((pos.x - inner.getX()) / (padW + gap)));
        const int row = juce::jlimit (0, rows - 1, (int) ((pos.y - inner.getY()) / (padH + gap)));
        return juce::jlimit (0, 127, e.padBaseNote + row * cols + col);
    }

    bool PlayerGuiRenderer::drumCellAt (const LayoutElement& e,
                                        juce::Rectangle<int> r,
                                        juce::Point<int> pos,
                                        int& pattern,
                                        int& track,
                                        int& step,
                                        float& velocity,
                                        float& gate,
                                        float& probability,
                                        bool& active,
                                        int& note,
                                        int& divisions) const
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findDrumMachineBlock (pack->dspGraph) : nullptr;
        const int tracks = block != nullptr
            ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) e.drumTracks)))
            : juce::jlimit (1, 16, e.drumTracks);
        const int steps = block != nullptr
            ? juce::jlimit (1, 64, juce::roundToInt (blockValue (*block, "dmSteps", (float) e.drumSteps)))
            : juce::jlimit (1, 64, e.drumSteps);
        pattern = block != nullptr
            ? juce::jlimit (0, 7, juce::roundToInt (blockValue (*block, "dmPattern", (float) e.drumPattern)))
            : juce::jlimit (0, 7, e.drumPattern);

        auto area = r.reduced (8);
        area.removeFromTop (28);
        area.removeFromTop (4);
        if (area.isEmpty())
            return false;

        const int labelW = juce::jlimit (42, 86, area.getWidth() / 5);
        auto grid = area.withTrimmedLeft (labelW);
        if (grid.isEmpty() || ! grid.contains (pos))
            return false;

        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        if (cellW <= 0.0f || cellH <= 0.0f)
            return false;

        step = juce::jlimit (0, steps - 1, (int) ((pos.x - grid.getX()) / cellW));
        track = juce::jlimit (0, tracks - 1, (int) ((pos.y - area.getY()) / cellH));

        const auto prefix = "dmP" + juce::String (pattern)
                          + "T" + juce::String (track)
                          + "S" + juce::String (step);
        active = block != nullptr && blockValue (*block, prefix + "On", 0.0f) >= 0.5f;
        gate = block != nullptr ? juce::jlimit (0.05f, 1.0f, blockValue (*block, prefix + "Gate", 0.35f)) : 0.35f;
        probability = block != nullptr ? juce::jlimit (0.0f, 1.0f, blockValue (*block, prefix + "Prob", 1.0f)) : 1.0f;
        divisions = block != nullptr
            ? juce::jlimit (1, 4, juce::roundToInt (blockValue (*block, prefix + "Div", 1.0f)))
            : 1;

        const auto trackTop = (float) area.getY() + (float) track * cellH;
        const float localY = juce::jlimit (0.0f, 1.0f, ((float) pos.y - trackTop) / cellH);
        velocity = juce::jlimit (0.15f, 1.0f, 1.0f - localY * 0.85f);

        note = block != nullptr
            ? juce::jlimit (0, 127, juce::roundToInt (blockValue (*block,
                "dmTrack" + juce::String (track) + "Note",
                (float) defaultDrumTrackNote (track))))
            : defaultDrumTrackNote (track);
        return true;
    }

    bool PlayerGuiRenderer::handleMultiLayerDockClick (juce::Point<int> pos)
    {
        if (! multiLayerDockBounds.contains (pos))
            return false;

        if (multiLayerToggleBounds.contains (pos))
        {
            multiLayerDockCollapsed = ! multiLayerDockCollapsed;
            repaint (multiLayerDockBounds.expanded (2));
            return true;
        }

        if (multiLayerDockCollapsed)
            return true;

        for (int i = 0; i < (int) multiLayerMuteBounds.size(); ++i)
        {
            if (i < (int) multiLayerCollapseBounds.size()
                && multiLayerCollapseBounds[(size_t) i].contains (pos))
            {
                if (i >= (int) multiLayerRowCollapsed.size())
                    multiLayerRowCollapsed.resize ((size_t) (i + 1), false);
                multiLayerRowCollapsed[(size_t) i] = ! multiLayerRowCollapsed[(size_t) i];
                repaint (multiLayerDockBounds.expanded (2));
                return true;
            }

            if (multiLayerMuteBounds[(size_t) i].contains (pos))
            {
                proc.setMultiLayerMuted (i, ! proc.getMultiLayerMuted (i));
                repaint (multiLayerDockBounds.expanded (2));
                return true;
            }

            if (multiLayerSoloBounds[(size_t) i].contains (pos))
            {
                proc.setMultiLayerSoloed (i, ! proc.getMultiLayerSoloed (i));
                repaint (multiLayerDockBounds.expanded (2));
                return true;
            }

            if (multiLayerVolumeBounds[(size_t) i].contains (pos))
            {
                const auto bounds = multiLayerVolumeBounds[(size_t) i];
                const float value = (float) (pos.x - bounds.getX()) / (float) juce::jmax (1, bounds.getWidth());
                proc.setMultiLayerVolume (i, juce::jlimit (0.0f, 1.0f, value));
                repaint (multiLayerDockBounds.expanded (2));
                return true;
            }

            if (multiLayerPanBounds[(size_t) i].contains (pos))
            {
                const auto bounds = multiLayerPanBounds[(size_t) i];
                const float value = (float) (pos.x - bounds.getX()) / (float) juce::jmax (1, bounds.getWidth());
                proc.setMultiLayerPan (i, juce::jlimit (-1.0f, 1.0f, value * 2.0f - 1.0f));
                repaint (multiLayerDockBounds.expanded (2));
                return true;
            }
        }

        return true;
    }

    bool PlayerGuiRenderer::handleMixerGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto pos = event.getPosition();

        auto setNormalisedParameter = [this] (const juce::String& parameterId, float normalised)
        {
            if (parameterId.isEmpty())
                return false;
            const auto* def = parameterForId (parameterId);
            if (def == nullptr || ! parameterIsEnabled (*def))
                return false;
            const float value = juce::jmap (juce::jlimit (0.0f, 1.0f, normalised),
                                            0.0f, 1.0f, def->min, def->max);
            return proc.setPackParameterFromUi (parameterId, value);
        };

        auto toggleParameter = [this] (const juce::String& parameterId)
        {
            if (parameterId.isEmpty())
                return false;
            const auto* def = parameterForId (parameterId);
            if (def == nullptr || ! parameterIsEnabled (*def))
                return false;
            const auto current = proc.getPackParameterValue (parameterId);
            return proc.setPackParameterFromUi (parameterId,
                current >= (def->min + def->max) * 0.5f ? def->min : def->max);
        };

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::Mixer || ! isElementOnCurrentTab (e))
                continue;
            if (drag && mixerDragActive && mixerDragElementId != e.id)
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos) && ! (drag && mixerDragActive && mixerDragElementId == e.id))
                continue;

            const bool wantsLayerMode = (e.mixerMode == "layers")
                || (e.mixerMode == "auto" && proc.isMultiInstrumentPack() && proc.getMultiLayerCount() > 1);
            const bool layerMode = wantsLayerMode && proc.getMultiLayerCount() > 0;
            const int channelCount = juce::jlimit (1, 16, layerMode ? proc.getMultiLayerCount()
                                                                    : e.mixerChannels);

            auto area = r.reduced (10, 8);
            if (e.labelPosition != "hidden")
            {
                area.removeFromTop (22);
                area.removeFromTop (6);
            }
            if (area.isEmpty())
                return true;

            const int stripW = juce::jmax (38, area.getWidth() / channelCount);
            for (int channel = 0; channel < channelCount; ++channel)
            {
                auto strip = juce::Rectangle<int> (area.getX() + channel * stripW,
                                                   area.getY(),
                                                   channel == channelCount - 1
                                                        ? area.getRight() - (area.getX() + channel * stripW)
                                                        : stripW,
                                                   area.getHeight()).reduced (3, 0);
                if (strip.getWidth() <= 12)
                    continue;

                strip.removeFromTop (30);
                auto buttons = strip.removeFromBottom (18).reduced (2, 1);
                auto panArea = strip.removeFromBottom (18).reduced (5, 3);
                strip.removeFromBottom (14);
                auto faderArea = strip.reduced (4, 3);
                const int faderCentre = faderArea.getCentreX();
                const int trackWidth = juce::jlimit (10, 18, faderArea.getWidth() / 5);
                const auto faderTrack = juce::Rectangle<int> (faderCentre - trackWidth / 2,
                                                              faderArea.getY() + 1,
                                                              trackWidth,
                                                              juce::jmax (32, faderArea.getHeight() - 2));
                auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0);
                auto solo = buttons.reduced (1, 0);

                juce::String hitKind;
                if ((! drag || mixerDragActive) && faderTrack.expanded (8, 2).contains (pos))
                    hitKind = "volume";
                else if ((! drag || mixerDragActive) && panArea.expanded (2, 4).contains (pos))
                    hitKind = "pan";
                else if (! drag && mute.contains (pos))
                    hitKind = "mute";
                else if (! drag && solo.contains (pos))
                    hitKind = "solo";

                if (drag && mixerDragActive && channel == mixerDragChannel && mixerDragKind.isNotEmpty())
                    hitKind = mixerDragKind;

                if (hitKind.isEmpty())
                    continue;

                const juce::String volumeParam = layerMode ? juce::String()
                    : stringAtOr (e.mixerVolumeParams, channel, channel == 0 ? "volume" : juce::String());
                const juce::String panParam = layerMode ? juce::String()
                    : stringAtOr (e.mixerPanParams, channel, channel == 0 ? "pan" : juce::String());
                const juce::String muteParam = layerMode ? juce::String()
                    : stringAtOr (e.mixerMuteParams, channel, {});
                const juce::String soloParam = layerMode ? juce::String()
                    : stringAtOr (e.mixerSoloParams, channel, {});

                if (hitKind == "volume")
                {
                    const float normalised = 1.0f - ((float) pos.y - (float) faderTrack.getY())
                        / (float) juce::jmax (1, faderTrack.getHeight());
                    if (layerMode)
                        proc.setMultiLayerVolume (channel, juce::jlimit (0.0f, 1.0f, normalised));
                    else
                        setNormalisedParameter (volumeParam, normalised);
                    mixerDragActive = true;
                    mixerDragChannel = channel;
                    mixerDragElementId = e.id;
                    mixerDragKind = hitKind;
                    repaint (r.expanded (2));
                    return true;
                }

                if (hitKind == "pan")
                {
                    const float normalised = ((float) pos.x - (float) panArea.getX())
                        / (float) juce::jmax (1, panArea.getWidth());
                    if (layerMode)
                        proc.setMultiLayerPan (channel, juce::jlimit (-1.0f, 1.0f, normalised * 2.0f - 1.0f));
                    else
                        setNormalisedParameter (panParam, normalised);
                    mixerDragActive = true;
                    mixerDragChannel = channel;
                    mixerDragElementId = e.id;
                    mixerDragKind = hitKind;
                    repaint (r.expanded (2));
                    return true;
                }

                if (hitKind == "mute")
                {
                    if (layerMode)
                        proc.setMultiLayerMuted (channel, ! proc.getMultiLayerMuted (channel));
                    else
                        toggleParameter (muteParam);
                    repaint (r.expanded (2));
                    return true;
                }

                if (hitKind == "solo")
                {
                    if (layerMode)
                        proc.setMultiLayerSoloed (channel, ! proc.getMultiLayerSoloed (channel));
                    else
                        toggleParameter (soloParam);
                    repaint (r.expanded (2));
                    return true;
                }
            }

            return true;
        }

        return false;
    }

    bool PlayerGuiRenderer::handleDrumGridGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::DrumGrid || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos))
                continue;

            auto area = r.reduced (8);
            auto header = area.removeFromTop (28);
            auto playButton = header.removeFromRight (58).reduced (2);
            auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);

            if (! drag && playButton.contains (pos))
            {
                proc.toggleInternalTransport();
                repaint (r);
                return true;
            }

            if (! drag && bankStrip.contains (pos))
            {
                constexpr int bankCount = 8;
                constexpr int bankGap = 3;
                const int bankW = juce::jmax (16, (bankStrip.getWidth() - bankGap * (bankCount - 1)) / bankCount);
                for (int bank = 0; bank < bankCount; ++bank)
                {
                    const auto chip = juce::Rectangle<int> (bankStrip.getX() + bank * (bankW + bankGap),
                                                            bankStrip.getY(),
                                                            bankW,
                                                            bankStrip.getHeight()).reduced (0, 2);
                    if (chip.contains (pos))
                    {
                        proc.setDrumActivePatternFromUi (bank);
                        repaint (r);
                        return true;
                    }
                }
            }

            int pattern = -1;
            int track = -1;
            int step = -1;
            int note = -1;
            int divisions = 1;
            float velocity = 0.8f;
            float gate = 0.35f;
            float probability = 1.0f;
            bool active = false;
            if (! drumCellAt (e, r, pos, pattern, track, step, velocity, gate, probability, active, note, divisions))
                return true;

            const bool cycleDivisions = event.mods.isCtrlDown() || event.mods.isCommandDown();
            if (drag && ! drumGridDragActive)
                return true;
            if (drag
                && pattern == lastDrumGridPattern
                && track == lastDrumGridTrack
                && step == lastDrumGridStep)
                return true;

            const bool newValue = cycleDivisions ? true : (drag ? drumGridPaintValue : ! active);
            const int newDivisions = cycleDivisions ? (active ? (divisions >= 4 ? 1 : divisions + 1) : 2) : divisions;
            drumGridDragActive = true;
            drumGridPaintValue = newValue;
            lastDrumGridPattern = pattern;
            lastDrumGridTrack = track;
            lastDrumGridStep = step;

            if (proc.setDrumPatternCellFromUi (pattern, track, step, newValue, velocity,
                                               gate, probability, newDivisions))
            {
                if (newValue && note >= 0)
                {
                    proc.handleNoteOn (note, velocity);
                    juce::Timer::callAfterDelay (90,
                        [safe = juce::Component::SafePointer<PlayerGuiRenderer> (this), note]
                        {
                            if (auto* self = safe.getComponent())
                                self->proc.handleNoteOff (note);
                        });
                }
                repaint (r);
            }

            return true;
        }

        return false;
    }

    bool PlayerGuiRenderer::arpLaneStepAt (const LayoutElement& element,
                                           juce::Rectangle<int> r,
                                           juce::Point<int> pos,
                                           int& lane,
                                           int& step,
                                           float& velocity) const
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findArpBlock (pack->dspGraph) : nullptr;
        lane = juce::jlimit (0, 15, element.arpLaneIndex);
        const bool orbitMultiRing = element.arpLaneMode.equalsIgnoreCase ("multiRing")
                                 || element.arpLaneMode.equalsIgnoreCase ("orbit")
                                 || element.arpLaneMode.equalsIgnoreCase ("orbitMulti");
        int steps = block != nullptr
            ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) element.arpLaneSteps)))
            : juce::jlimit (1, 128, element.arpLaneSteps);

        auto area = r.reduced (10);
        area.removeFromTop (26);
        const float size = (float) juce::jmin (area.getWidth(), area.getHeight() - 42);
        if (size <= 24.0f)
            return false;

        const juce::Point<float> centre ((float) area.getCentreX(), (float) area.getY() + size * 0.52f);
        const float radius = size * 0.40f;
        const auto delta = pos.toFloat() - centre;
        const float distance = delta.getDistanceFromOrigin();
        if (distance < radius * 0.16f || distance > radius * 1.22f)
            return false;

        if (orbitMultiRing)
        {
            const int laneCount = 5;
            const float multiRadius = size * 0.42f;
            const float innerRadius = multiRadius * 0.25f;
            const float outerRadius = multiRadius * 0.94f;
            if (distance < innerRadius - 4.0f || distance > outerRadius + 8.0f)
                return false;

            const float lanePos = juce::jmap (distance, innerRadius, outerRadius, 0.0f, (float) laneCount);
            lane = juce::jlimit (0, laneCount - 1, (int) std::floor (lanePos));
            steps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, lane, "arpSteps", (float) element.arpLaneSteps)))
                : juce::jlimit (1, 128, element.arpLaneSteps);
            const float laneCentre = innerRadius + ((outerRadius - innerRadius) / (float) laneCount) * ((float) lane + 0.5f);
            velocity = juce::jlimit (0.05f, 1.0f,
                juce::jmap (distance - laneCentre, -(outerRadius - innerRadius) / (float) laneCount * 0.45f,
                            (outerRadius - innerRadius) / (float) laneCount * 0.45f, 0.05f, 1.0f));
        }

        float angle01 = (std::atan2 (delta.y, delta.x) + juce::MathConstants<float>::halfPi)
            / juce::MathConstants<float>::twoPi;
        while (angle01 < 0.0f) angle01 += 1.0f;
        while (angle01 >= 1.0f) angle01 -= 1.0f;

        const int maxDrawSteps = juce::jmin (steps, 64);
        step = juce::jlimit (0, maxDrawSteps - 1, juce::roundToInt (angle01 * (float) maxDrawSteps) % maxDrawSteps);
        if (! orbitMultiRing)
            velocity = juce::jlimit (0.05f, 1.0f,
                juce::jmap (distance, radius * 0.25f, radius * 0.93f, 0.05f, 1.0f));
        return true;
    }

    bool PlayerGuiRenderer::handleArpLaneGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::ArpLane || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos))
                continue;

            if (! drag && arpLanePlayButtonBounds (r).contains (pos))
            {
                proc.toggleInternalTransport();
                repaint (r);
                return true;
            }

            if (! drag && arpLaneMidiDragHandleBounds (r).contains (pos))
            {
                arpMidiDragArmed = true;
                arpMidiDragStart = pos;
                arpMidiDragElementId = e.id;
                return true;
            }

            if (drag && ! arpLaneDragActive)
                return true;

            int lane = -1;
            int step = -1;
            float velocity = 0.8f;
            if (arpLaneStepAt (e, r, pos, lane, step, velocity))
            {
                arpLaneDragActive = true;
                if (proc.setArpLaneStepFromUi (lane, step, velocity, true))
                    repaint (r);
                return true;
            }

            if (drag)
                return true;

            if (proc.setMidiPlaygroundActiveBankFromUi (e.arpLaneIndex))
                repaint();
            return true;
        }

        return false;
    }

    bool PlayerGuiRenderer::startArpLaneMidiDrag (const LayoutElement& element)
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findArpBlock (pack->dspGraph) : nullptr;
        if (block == nullptr)
        {
            if (onRuntimeImportReport)
                onRuntimeImportReport ("MIDI drag failed: this Player has no Arp Studio or MIDI Playground block.");
            return false;
        }

        DspBlock exportBlock = *block;
        const int lane = juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, element.arpLaneIndex);
        MidiPlaygroundPattern::loadBank (exportBlock, lane, false);

        auto outputFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("MidiDrag");
        if (! outputFolder.createDirectory())
        {
            if (onRuntimeImportReport)
                onRuntimeImportReport ("MIDI drag failed: could not create " + outputFolder.getFullPathName());
            return false;
        }

        auto target = outputFolder.getChildFile (arpLaneMidiFileName (pack->manifest.instrumentName, element.label, lane));
        int duplicateIndex = 2;
        while (target.existsAsFile())
            target = outputFolder.getChildFile (arpLaneMidiFileName (pack->manifest.instrumentName,
                                                                     element.label + "_" + juce::String (duplicateIndex++),
                                                                     lane));

        juce::String error;
        if (! MidiPlaygroundPattern::writeMidiClip (exportBlock, target, 120.0, 60, error))
        {
            if (onRuntimeImportReport)
                onRuntimeImportReport ("MIDI drag failed: " + error);
            return false;
        }

        juce::StringArray files;
        files.add (target.getFullPathName());
        const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
        if (! started && onRuntimeImportReport)
            onRuntimeImportReport ("MIDI drag failed: the operating system did not start an external file drag.");
        return started;
    }

    bool PlayerGuiRenderer::handlePadClick (const juce::MouseEvent& event)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr) return false;

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible) continue;
            if (e.type != ElementType::DrumPad && e.type != ElementType::PadGrid) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (! r.contains (pos)) continue;

            const int note = padNoteAt (e, r, pos);
            if (note < 0) return true;

            // Velocity from vertical click position inside the pad: top = soft,
            // bottom = full. Mirrors how MPC pads respond to pressure.
            const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
            const float yNorm = juce::jlimit (0.0f, 1.0f,
                (float) (pos.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));
            const float velocity = juce::jlimit (0.2f, 1.0f, 0.4f + yNorm * 0.6f);

            if (activePadNote >= 0 && activePadNote != note)
                proc.handleNoteOff (activePadNote);
            activePadNote = note;
            proc.handleNoteOn (note, velocity);
            repaint (r);
            return true;
        }
        return false;
    }

    int PlayerGuiRenderer::hitTabIndex (const LayoutElement& tabPanel,
                                        juce::Rectangle<int> r,
                                        juce::Point<int> pos) const
    {
        if (! r.contains (pos)) return -1;
        const int n = tabPanel.tabs.size();
        if (n <= 0) return -1;
        for (int i = 0; i < n; ++i)
            if (tabBounds (r, i, n).contains (pos))
                return i;
        return -1;
    }

    bool PlayerGuiRenderer::handleXYPadGesture (const juce::MouseEvent& event)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto position = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::XYPad || ! isElementOnCurrentTab (e))
                continue;

            const auto r = elementRect (e, m);
            if (! r.contains (position))
                continue;

            const auto* def = parameterForId (e.parameterId);
            if (def == nullptr || ! parameterIsEnabled (*def))
                return true;

            const auto inner = r.reduced (8);
            const auto width = juce::jmax (1, inner.getWidth());
            const float normalised = juce::jlimit (0.0f, 1.0f,
                (float) (position.x - inner.getX()) / (float) width);
            proc.setPackParameterFromUi (e.parameterId, juce::jmap (normalised, 0.0f, 1.0f, def->min, def->max));
            repaint (r.expanded (2));
            return true;
        }

        return false;
    }

    bool PlayerGuiRenderer::handleGranularGesture (const juce::MouseEvent& event)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto position = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::GranularField || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (position))
                continue;

            auto setParameter = [this] (const juce::String& parameterId, float value)
            {
                const auto* def = parameterForId (parameterId);
                if (def == nullptr)
                    return false;

                if (! parameterIsEnabled (*def))
                {
                    if (def->enabledBy == "granularOn")
                    {
                        if (const auto* gate = parameterForId ("granularOn"))
                            proc.setPackParameterFromUi ("granularOn", gate->max);
                    }
                    else
                        return false;
                }

                proc.setPackParameterFromUi (parameterId, juce::jlimit (def->min, def->max, value));
                return true;
            };

            bool changed = false;
            if (parameterForId ("granularOn") != nullptr)
                changed = setParameter ("granularOn", 1.0f) || changed;

            auto controlArea = r.reduced (12, 10);
            controlArea.removeFromTop (22);
            controlArea.removeFromTop (4);
            if (controlArea.getHeight() < 60 || controlArea.getWidth() < 160)
                return true;

            auto chipArea = controlArea.removeFromBottom (24);
            controlArea.removeFromBottom (4);
            const juce::StringArray chips { "FWD", "REV", "PING", "MULTI", "FREEZE" };
            if (chipArea.contains (position))
            {
                const int chipGap = 5;
                const int chipW = juce::jmax (48, (chipArea.getWidth() - chipGap * (chips.size() - 1)) / chips.size());
                for (int chipIndex = 0; chipIndex < chips.size(); ++chipIndex)
                {
                    auto chip = juce::Rectangle<int> (chipArea.getX() + chipIndex * (chipW + chipGap),
                                                      chipArea.getY(), chipW, chipArea.getHeight()).reduced (1);
                    if (! chip.contains (position))
                        continue;

                    if (chipIndex < 4)
                        changed = setParameter ("granularDirection", (float) chipIndex) || changed;
                    else
                    {
                        const float current = proc.getPackParameterValue ("granularFreeze");
                        changed = setParameter ("granularFreeze", current >= 0.5f ? 0.0f : 1.0f) || changed;
                    }

                    if (changed)
                        repaint (r.expanded (2));
                    return true;
                }
            }

            const auto inner = controlArea;
            const float x = juce::jlimit (0.0f, 1.0f,
                (float) (position.x - inner.getX()) / (float) juce::jmax (1, inner.getWidth()));
            const float y = juce::jlimit (0.0f, 1.0f,
                (float) (position.y - inner.getY()) / (float) juce::jmax (1, inner.getHeight()));

            if (event.mods.isShiftDown())
            {
                changed = setParameter ("granularScan", juce::jmap (x, 0.0f, 1.0f, -3.0f, 3.0f)) || changed;
                changed = setParameter ("granularSpread", 1.0f - y) || changed;
            }
            else if (event.mods.isCommandDown() || event.mods.isCtrlDown())
            {
                changed = setParameter ("granularDirection", (float) juce::jlimit (0, 3, (int) std::floor (x * 4.0f))) || changed;
                changed = setParameter ("granularReverse", 1.0f - y) || changed;
            }
            else if (event.mods.isAltDown())
            {
                changed = setParameter ("granularPitchSpread", juce::jmap (x, 0.0f, 1.0f, 0.0f, 36.0f)) || changed;
                changed = setParameter ("granularPanSpread", 1.0f - y) || changed;
            }
            else
            {
                changed = setParameter ("sampleStart", x) || changed;
                changed = setParameter ("sampleLength", juce::jmap (1.0f - y, 0.0f, 1.0f, 0.01f, 1.0f)) || changed;
            }

            if (changed)
                repaint (r.expanded (2));
            return true;
        }

        return false;
    }

    bool PlayerGuiRenderer::advanceGranularFields()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (lastGranularAdvanceSeconds <= 0.0)
        {
            lastGranularAdvanceSeconds = now;
            return false;
        }

        const float dt = juce::jlimit (0.0f, 0.10f, (float) (now - lastGranularAdvanceSeconds));
        lastGranularAdvanceSeconds = now;
        if (dt <= 0.0f)
            return false;

        bool changed = false;
        for (const auto& e : elementsCopy)
        {
            if (! e.visible || e.type != ElementType::GranularField || ! isElementOnCurrentTab (e))
                continue;

            const float on = proc.getPackParameterValue ("granularOn");
            const float freeze = proc.getPackParameterValue ("granularFreeze");
            const float scan = proc.getPackParameterValue ("granularScan");
            const auto* startDef = parameterForId ("sampleStart");
            if (startDef == nullptr || on < 0.5f || freeze >= 0.5f || std::abs (scan) < 0.0001f)
                continue;

            float start = proc.getPackParameterValue ("sampleStart");
            float next = start + scan * dt * 0.055f;
            next -= std::floor (next);
            proc.setPackParameterFromUi ("sampleStart", juce::jlimit (startDef->min, startDef->max, next));
            changed = true;
        }
        return changed;
    }

    // ---- Paint --------------------------------------------------------------
    void PlayerGuiRenderer::paint (juce::Graphics& g)
    {
        g.fillAll (playerBg());
        const auto* pack = proc.getPack();
        if (pack == nullptr) return;

        const auto m = metrics();
        if (background.isValid())
            g.drawImage (background, m.canvas.toFloat());

        // Find current preset name for the dropdown display.
        juce::String currentPresetName = pack->manifest.defaultPreset;
        if (currentPresetName.isEmpty() && ! pack->presets.empty())
            currentPresetName = pack->presets.front().name;

        // Draw non-control element types in z-order.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (e)) continue;

            auto r = animatedElementRect (e, elementRect (e, m));
            juce::Graphics::ScopedSaveState opacityState (g);
            const float reactiveAlpha = e.audioReactive
                ? juce::jlimit (0.0f, 0.28f, proc.getOutputPeak() * juce::jmax (0.1f, e.audioReactiveAmount))
                : 0.0f;
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity + reactiveAlpha));

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
            {
                drawRuntimeControl (g, controlBodyRect (e, r), e);
                continue;
            }

            switch (e.type)
            {
                case ElementType::Image:
                    if (e.id == "background")
                        break;  // Already painted as full-canvas above

                    {
                        // Branding override: an Image element with id="logo"
                        // pulls from manifest.playerLogoImage if set, so the
                        // same layout works for both the bare PatchCraft
                        // Player and a white-label developer build.
                        juce::String assetPath = e.asset;
                        if (e.id == "logo" && manifest() != nullptr
                            && manifest()->playerLogoImage.isNotEmpty())
                            assetPath = manifest()->playerLogoImage;

                        if (assetPath.isNotEmpty())
                        {
                            auto f = juce::File::isAbsolutePath (assetPath)
                                        ? juce::File (assetPath)
                                        : pack->rootFolder.getChildFile (assetPath);
                            if (auto img = assets.loadImage (f); img.isValid())
                                g.drawImage (img, r.toFloat());
                            else if (heroImage.isValid())
                                g.drawImage (heroImage, r.toFloat());
                        }
                        else if (heroImage.isValid())
                        {
                            // Use hero image for hero element
                            g.drawImage (heroImage, r.toFloat());
                        }
                    }
                    break;

                case ElementType::Label:
                {
                    const auto fill = e.backgroundColour;
                    if (! fill.isTransparent())
                    {
                        g.setColour (fill.withMultipliedAlpha (e.opacity));
                        g.fillRoundedRectangle (r.toFloat(), juce::jmax (0.0f, e.cornerRadius));
                    }

                    if (! e.borderColour.isTransparent() && e.strokeWidth > 0.0f)
                    {
                        g.setColour (e.borderColour.withMultipliedAlpha (e.opacity));
                        g.drawRoundedRectangle (r.toFloat().reduced (0.5f),
                                                juce::jmax (0.0f, e.cornerRadius),
                                                juce::jmax (0.5f, e.strokeWidth));
                    }

                    const auto text = e.label.isNotEmpty() ? e.label : e.parameterId;
                    if (text.isNotEmpty())
                    {
                        const float fontSize = e.labelSize > 0.0f
                            ? e.labelSize
                            : juce::jlimit (10.0f, 22.0f, (float) r.getHeight() * 0.42f);
                        const bool bold = e.style.containsIgnoreCase ("bold")
                                       || e.style.containsIgnoreCase ("title")
                                       || fontSize >= 14.0f;
                        g.setColour ((e.textColour.isTransparent() ? playerText() : e.textColour)
                                     .withMultipliedAlpha (e.opacity));
                        g.setFont (juce::FontOptions (fontSize).withStyle (bold ? "bold" : "plain"));
                        const int lines = juce::jmax (1, r.getHeight() / juce::jmax (1, juce::roundToInt (fontSize + 3.0f)));
                        g.drawFittedText (text, r.reduced (4, 1),
                                          lines > 1 ? juce::Justification::topLeft
                                                    : juce::Justification::centredLeft,
                                          lines);
                    }
                    break;
                }

                case ElementType::Meter:    drawMeter (g, r, e); break;
                case ElementType::EqCurve:  drawEqCurve (g, r, e); break;
                case ElementType::SpectrumAnalyzer: drawSpectrumAnalyzer (g, r, e); break;
                case ElementType::ReactiveImage:
                {
                    juce::String assetPath = e.asset;
                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                                    ? juce::File (assetPath)
                                    : pack->rootFolder.getChildFile (assetPath);
                        if (auto img = assets.loadImage (f); img.isValid())
                            g.drawImage (img, r.toFloat());
                    }
                    const float level = juce::jlimit (0.0f, 1.0f, proc.getOutputPeak() * juce::jmax (0.1f, e.audioReactiveAmount));
                    const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
                    auto halo = r.reduced (8).toFloat();
                    for (int ring = 0; ring < 4; ++ring)
                    {
                        const float grow = (float) ring * 9.0f + level * 16.0f;
                        g.setColour (accent.withAlpha (0.22f - (float) ring * 0.035f));
                        g.drawRoundedRectangle (halo.expanded (grow), juce::jmax (4.0f, e.cornerRadius + grow), 1.0f + level * 2.0f);
                    }
                    if (assetPath.isEmpty())
                    {
                        g.setColour (playerPanel().withAlpha (0.70f));
                        g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                        g.setColour (accent.withAlpha (0.75f));
                        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    }
                    break;
                }
                case ElementType::SpriteAnimator:
                {
                    const auto assetPath = e.asset.isNotEmpty() ? e.asset : e.filmstripAsset;
                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                                    ? juce::File (assetPath)
                                    : pack->rootFolder.getChildFile (assetPath);
                        if (auto img = assets.loadImage (f); img.isValid())
                        {
                            const int frames = juce::jmax (1, e.filmstripFrames > 0 ? e.filmstripFrames : 8);
                            const int frame = ((int) std::floor (juce::Time::getMillisecondCounterHiRes() * 0.001
                                                                 * juce::jmax (0.05f, e.animationRate))) % frames;
                            if (e.filmstripVertical)
                            {
                                const int h = juce::jmax (1, img.getHeight() / frames);
                                g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                                             0, frame * h, img.getWidth(), h);
                            }
                            else
                            {
                                const int w = juce::jmax (1, img.getWidth() / frames);
                                g.drawImage (img, r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                                             frame * w, 0, w, img.getHeight());
                            }
                            break;
                        }
                    }
                    g.setColour (playerPanel().withAlpha (0.65f));
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour ((e.accentColour.isTransparent() ? playerAccent() : e.accentColour).withAlpha (0.65f));
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    break;
                }
                case ElementType::VisualFxLayer:
                {
                    const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
                    const float level = juce::jlimit (0.0f, 1.0f, proc.getOutputPeak() * juce::jmax (0.1f, e.audioReactiveAmount));
                    const float seconds = (float) (juce::Time::getMillisecondCounterHiRes() * 0.001);
                    const auto bounds = r.reduced (8).toFloat();
                    const auto centre = bounds.getCentre();
                    const float base = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.18f;
                    for (int i = 0; i < 32; ++i)
                    {
                        const float phase = seconds * juce::jmax (0.05f, e.animationRate) + (float) i * 0.43f;
                        const float radius = base + std::fmod ((float) i * 11.0f + seconds * 24.0f, base * (1.6f + level));
                        g.setColour (accent.withAlpha (0.12f + level * 0.24f));
                        g.fillEllipse (centre.x + std::cos (phase) * radius - 2.0f,
                                       centre.y + std::sin (phase * 0.8f) * radius * 0.74f - 2.0f,
                                       4.0f + level * 4.0f,
                                       4.0f + level * 4.0f);
                    }
                    break;
                }
                case ElementType::AiVisualPrompt:
                {
                    g.setColour (e.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.55f) : e.backgroundColour);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour ((e.accentColour.isTransparent() ? juce::Colour (0xffb98cff) : e.accentColour).withAlpha (0.7f));
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    g.setColour (playerTextDim());
                    g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
                    g.drawFittedText (e.visualAiGenerated ? "AI VISUAL ASSET" : "AI VISUAL PLACEHOLDER",
                                      r.reduced (8), juce::Justification::centred, 2);
                    break;
                }
                case ElementType::Panel:
                case ElementType::ScrollPanel:
                    drawPanel (g, r, e.label);
                    break;
                case ElementType::Button:
                    drawButton (g, r, e);
                    break;
                case ElementType::Toggle:
                {
                    const auto* def = parameterForId (e.parameterId);
                    float value = def != nullptr ? parameterValueForElement (e, def->defaultValue) : 0.0f;
                    if (def != nullptr && def->max > def->min)
                        value = (value - def->min) / (def->max - def->min);

                    const bool on = value >= 0.5f;
                    auto toggle = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 92), juce::jmin (r.getHeight(), 38)).toFloat();
                    g.setColour (on ? e.accentColour.withAlpha (0.85f) : juce::Colour (0xff202329));
                    g.fillRoundedRectangle (toggle, toggle.getHeight() * 0.5f);
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.drawRoundedRectangle (toggle, toggle.getHeight() * 0.5f, 1.0f);
                    const float knobSize = toggle.getHeight() - 8.0f;
                    const float knobX = on ? toggle.getRight() - knobSize - 4.0f : toggle.getX() + 4.0f;
                    g.setColour (playerText().brighter (0.25f));
                    g.fillEllipse (knobX, toggle.getY() + 4.0f, knobSize, knobSize);
                    g.setColour (playerText().brighter (0.25f));
                    g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
                    g.drawText (e.label.isNotEmpty() ? e.label : e.parameterId,
                                r.withTrimmedTop (r.getHeight() / 2), juce::Justification::centred, true);
                    break;
                }
                case ElementType::Shape:
                {
                    auto shapeBounds = r.toFloat().reduced (e.strokeWidth * 0.5f + 1.0f);
                    juce::Path path;
                    if (e.shapeKind == "ellipse")
                        path.addEllipse (shapeBounds);
                    else if (e.shapeKind == "triangle")
                        path.addTriangle (shapeBounds.getCentreX(), shapeBounds.getY(),
                                          shapeBounds.getRight(), shapeBounds.getBottom(),
                                          shapeBounds.getX(), shapeBounds.getBottom());
                    else if (e.shapeKind == "diamond")
                    {
                        path.startNewSubPath (shapeBounds.getCentreX(), shapeBounds.getY());
                        path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
                        path.lineTo (shapeBounds.getCentreX(), shapeBounds.getBottom());
                        path.lineTo (shapeBounds.getX(), shapeBounds.getCentreY());
                        path.closeSubPath();
                    }
                    else if (e.shapeKind == "line")
                    {
                        path.startNewSubPath (shapeBounds.getX(), shapeBounds.getCentreY());
                        path.lineTo (shapeBounds.getRight(), shapeBounds.getCentreY());
                    }
                    else
                    {
                        path.addRoundedRectangle (shapeBounds, juce::jmax (0.0f, e.cornerRadius));
                    }

                    if (e.shapeKind != "line")
                    {
                        g.setColour (e.backgroundColour.isTransparent() ? juce::Colour (0x33141822) : e.backgroundColour);
                        g.fillPath (path);
                    }
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.strokePath (path, juce::PathStrokeType (juce::jmax (0.5f, e.strokeWidth)));
                    break;
                }
                case ElementType::ValueDisplay:
                    drawValueDisplay (g, r, e);
                    break;
                case ElementType::Separator:
                    g.setColour (e.borderColour.isTransparent() ? playerBorder() : e.borderColour);
                    g.drawLine ((float) r.getX(), (float) r.getCentreY(), (float) r.getRight(), (float) r.getCentreY(),
                                juce::jmax (1.0f, e.strokeWidth));
                    break;
                case ElementType::Waveform:
                {
                    g.setColour (playerPanel().darker (0.25f));
                    g.fillRoundedRectangle (r.toFloat(), 4.0f);
                    g.setColour (playerAccent().withAlpha (0.8f));
                    juce::Path wave;
                    const auto bounds = r.reduced (6).toFloat();
                    const auto level = juce::jlimit (0.05f, 1.0f, proc.getOutputPeak() + 0.08f);
                    for (int i = 0; i < 64; ++i)
                    {
                        const float x = bounds.getX() + (float) i / 63.0f * bounds.getWidth();
                        const float y = bounds.getCentreY()
                            + std::sin ((float) i * 0.48f) * bounds.getHeight() * 0.42f * level;
                        if (i == 0) wave.startNewSubPath (x, y);
                        else        wave.lineTo (x, y);
                    }
                    g.strokePath (wave, juce::PathStrokeType (1.5f));
                    const double playback01 = proc.getSequencerPlaybackPosition01();
                    if (playback01 >= 0.0)
                    {
                        const float playheadX = bounds.getX() + (float) playback01 * bounds.getWidth();
                        g.setColour (playerAccent().withAlpha (0.95f));
                        g.drawLine (playheadX, bounds.getY(), playheadX, bounds.getBottom(), 1.6f);
                    }
                    break;
                }
                case ElementType::Keyboard: drawKeyboard (g, r); break;
                case ElementType::TabPanel: drawTabPanel (g, r, e); break;
                case ElementType::XYPad:    drawXYPad (g, r, e); break;
                case ElementType::GranularField:
                    drawGranularField (g, r, e);
                    break;
                case ElementType::DrumPad:
                case ElementType::PadGrid:
                    drawPadGrid (g, r, e);
                    break;
                case ElementType::DrumGrid:
                    drawDrumGrid (g, r, e);
                    break;
                case ElementType::ArpLane:
                    drawArpLane (g, r, e);
                    break;
                case ElementType::Mixer:
                    drawMixer (g, r, e);
                    break;
                case ElementType::ModMatrix:
                {
                    const auto bg = e.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.78f) : e.backgroundColour;
                    const auto border = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
                    const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
                    g.setColour (bg);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour (border);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    auto area = r.reduced (10, 8);
                    auto header = area.removeFromTop (20);
                    g.setColour (accent);
                    g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
                    g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : "MOD MATRIX",
                                header.removeFromLeft (130), juce::Justification::centredLeft, true);
                    g.setColour (playerTextDim());
                    g.setFont (juce::FontOptions (9.0f));
                    g.drawText ("performable routing", header, juce::Justification::centredRight, true);
                    auto list = area.reduced (2, 6);
                    const auto* pack = proc.getPack();
                    const auto routeCount = pack != nullptr ? (int) pack->dspGraph.modulation.size() : 0;
                    if (routeCount <= 0)
                    {
                        g.setColour (playerTextDim());
                        g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
                        g.drawFittedText ("No modulation routes in this patch.",
                                          list, juce::Justification::centred, 3);
                        break;
                    }
                    const int maxRows = juce::jmin (6, routeCount);
                    for (int i = 0; i < maxRows; ++i)
                    {
                        const auto& route = pack->dspGraph.modulation[(size_t) i];
                        auto row = list.removeFromTop (juce::jmax (18, list.getHeight() / (maxRows - i))).reduced (0, 2);
                        g.setColour (route.enabled ? accent.withAlpha (0.16f) : playerBg().withAlpha (0.72f));
                        g.fillRoundedRectangle (row.toFloat(), 4.0f);
                        g.setColour (route.enabled ? accent.withAlpha (0.78f) : playerTextDim());
                        g.setFont (juce::FontOptions (8.8f).withStyle ("bold"));
                        g.drawText (route.sourceId + " -> " + route.targetId,
                                    row.removeFromLeft (juce::jmax (80, row.getWidth() - 52)).reduced (6, 0),
                                    juce::Justification::centredLeft, true);
                        g.drawText (juce::String (route.amount, 2), row.reduced (4, 0), juce::Justification::centredRight, true);
                    }
                    break;
                }
                case ElementType::Dropdown:
                    drawDropdown (g, r, e.id == "presets" ? currentPresetName
                                     : e.parameterId.isNotEmpty() ? formattedParameterValue (e)
                                                                  : e.label);
                    break;

                default: break;
            }
        }

        drawMultiLayerDock (g, m.canvas);

        if (const auto pending = proc.getPendingMidiLearnParameter(); pending.isNotEmpty())
        {
            auto banner = m.canvas.withHeight (34).reduced (12, 0).translated (0, 10);
            g.setColour (juce::Colour (0xee11151b));
            g.fillRoundedRectangle (banner.toFloat(), 6.0f);
            g.setColour (playerAccent());
            g.drawRoundedRectangle (banner.toFloat(), 6.0f, 1.0f);
            g.setColour (playerText().brighter (0.25f));
            g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
            g.drawText ("MIDI Learn: move a hardware control for " + pending,
                        banner.reduced (12, 0), juce::Justification::centredLeft);
        }

        // License / trial watermark
        if (pack != nullptr)
        {
            LicenseValidator::LicenseInfo info;
            info.licenseKey = pack->manifest.licenseKey;
            info.instrumentName = pack->manifest.instrumentName;
            info.creator = pack->manifest.creator;
            info.productId = pack->manifest.licenseProductId;
            info.licenseServerUrl = pack->manifest.licenseServerUrl;
            info.policy = pack->manifest.licensePolicy;
            info.isTrial = pack->manifest.isTrial;
            info.trialDays = pack->manifest.trialDays;
            info.expiryDate = pack->manifest.trialExpiryDate;
            info.offlineGraceDays = pack->manifest.licenseOfflineGraceDays;
            info.bindToMachine = pack->manifest.licenseBindToMachine;
            const auto watermark = LicenseValidator::generateWatermarkText (info);
            if (watermark.isNotEmpty())
            {
                g.setColour (juce::Colours::red.withAlpha (0.6f));
                g.setFont (juce::FontOptions (16.0f).withStyle ("bold"));
                g.drawText (watermark, m.canvas.withHeight (28).withBottomY (m.canvas.getBottom() - 6),
                            juce::Justification::centred, true);
            }
        }
    }

    void PlayerGuiRenderer::paintOverChildren (juce::Graphics& g)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return;

        const auto m = metrics();
        for (const auto& e : elementsCopy)
        {
            if (! e.visible || ! isElementOnCurrentTab (e))
                continue;
            if (e.type == ElementType::Group)
                continue;

            juce::Graphics::ScopedSaveState opacityState (g);
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity));
            const auto r = animatedElementRect (e, elementRect (e, m));

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
                drawControlLabelOverlay (g, e, r);
        }
    }

    // ---- Layout -------------------------------------------------------------
    void PlayerGuiRenderer::resized()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || controls.isEmpty()) return;

        const auto m = metrics();

        // Positions controls 1:1 with elementsCopy. Hide controls whose element
        // is not on the active tab (they still exist behind the scenes - this
        // way the SliderAttachments stay alive across tab switches).
        int idx = 0;
        for (auto& e : elementsCopy)
        {
            if (idx >= controls.size()) break;
            auto* c = controls[idx];
            ++idx;

            const bool show = e.visible && e.type != ElementType::Group && isElementOnCurrentTab (e);
            c->setVisible (show);
            if (show)
            {
                auto bounds = elementRect (e, m);
                if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl)
                    bounds = controlBodyRect (e, bounds);
                bounds = animatedElementRect (e, bounds);
                c->setBounds (bounds);
            }
        }
    }

    // ---- Mouse --------------------------------------------------------------
    void PlayerGuiRenderer::mouseDown (const juce::MouseEvent& evt)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr) return;
        const auto event = evt.getEventRelativeTo (this);

        if (handleMultiLayerDockClick (event.getPosition()))
            return;

        if (event.mods.isPopupMenu())
        {
            if (const auto* element = findBindableElementAt (event.getPosition()))
                showControlContextMenu (*element, event.getScreenPosition());
            return;
        }

        const auto m = metrics();

        // Tab strips are navigation, not editable controls. Handle them before
        // any performance gestures so overlays, mixers, or invisible control
        // peers can never swallow the page switch.
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::TabPanel)
                continue;

            const bool rootTabs = e.id == "tabs";
            if (! rootTabs && ! isElementOnCurrentTab (e))
                continue;

            const auto r = elementRect (e, m);
            if (! r.contains (event.getPosition()))
                continue;

            const int tabIdx = hitTabIndex (e, r, event.getPosition());
            if (tabIdx < 0 || tabIdx >= e.tabs.size())
                continue;

            const auto targetGroup = tabTargetGroup (e, e.tabs[tabIdx]);
            activeTabGroupsByPanel[e.id] = targetGroup;
            if (tabTargetIsGlobal (e, targetGroup))
                currentTabGroup = targetGroup;

            refreshControlEnablement();
            resized();
            repaint();
            return;
        }

        if (handleGranularGesture (event))
            return;

        if (handleXYPadGesture (event))
            return;

        if (handleArpLaneGesture (event, false))
            return;

        if (handleDrumGridGesture (event, false))
            return;

        if (handleMixerGesture (event, false))
            return;

        if (handlePadClick (event))
            return;

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible) continue;
            if (e.type != ElementType::Button && e.type != ElementType::Toggle
                && e.type != ElementType::Label && e.type != ElementType::Image)
                continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (! r.contains (event.getPosition())) continue;
            if (triggerManualContainer (e))
                return;
        }

        // Keyboard click - send MIDI note
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Keyboard) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (r.contains (event.getPosition()))
            {
                handleKeyboardClick (r, event.getPosition());
                return;
            }
        }

        // Dropdown clicks show preset or parameter menus.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Dropdown) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (r.contains (event.getPosition()))
            {
                if (e.id == "presets")
                    showPresetMenu (event.getScreenPosition());
                else
                    showParameterMenu (e, event.getScreenPosition());
                return;
            }
        }

        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (e.type != ElementType::Toggle && e.type != ElementType::Button) continue;
            if (! isElementOnCurrentTab (e)) continue;
            const auto r = elementRect (e, m);
            if (! r.contains (event.getPosition())) continue;

            const auto* def = parameterForId (e.parameterId);
            if (def == nullptr)
                return;
            if (! parameterIsEnabled (*def))
                return;

            const auto current = parameterValueForElement (e, def->defaultValue);
            if (e.type == ElementType::Toggle)
                proc.setPackParameterFromUi (e.parameterId, current >= (def->min + def->max) * 0.5f ? def->min : def->max);
            else
            {
                activeMomentaryParameter = e.parameterId;
                proc.setPackParameterFromUi (e.parameterId, def->max);
            }
            repaint();
            return;
        }
    }

    void PlayerGuiRenderer::mouseDrag (const juce::MouseEvent& evt)
    {
        const auto event = evt.getEventRelativeTo (this);
        const auto m = metrics();
        if (arpMidiDragArmed && arpMidiDragElementId.isNotEmpty())
        {
            const auto delta = event.getPosition() - arpMidiDragStart;
            if (std::abs (delta.x) + std::abs (delta.y) >= 8)
            {
                for (const auto& element : elementsCopy)
                {
                    if (element.id == arpMidiDragElementId)
                    {
                        startArpLaneMidiDrag (element);
                        break;
                    }
                }
                arpMidiDragArmed = false;
                arpMidiDragElementId.clear();
            }
            return;
        }

        if (lastPlayedNote >= 0)
        {
            for (auto& e : elementsCopy)
            {
                if (! e.visible || e.type != ElementType::Keyboard || ! isElementOnCurrentTab (e))
                    continue;
                const auto r = elementRect (e, m);
                if (! r.contains (event.getPosition()))
                    continue;
                const int note = noteForKeyboardPosition (r, event.getPosition());
                if (note >= 0 && note != lastPlayedNote)
                {
                    proc.handleNoteOff (lastPlayedNote);
                    proc.handleNoteOn (note);
                    lastPlayedNote = note;
                    repaint (r.expanded (4));
                }
                return;
            }
        }
        if (handleMultiLayerDockClick (event.getPosition()))
            return;
        if (handleGranularGesture (event))
            return;

        if (handleXYPadGesture (event))
            return;
        if (handleArpLaneGesture (event, true))
            return;
        if (handleDrumGridGesture (event, true))
            return;
        if (handleMixerGesture (event, true))
            return;
        if (activePadNote >= 0)
            handlePadClick (event);
    }

    void PlayerGuiRenderer::mouseUp (const juce::MouseEvent& evt)
    {
        // Release any held keyboard notes
        if (lastPlayedNote >= 0)
        {
            proc.handleNoteOff (lastPlayedNote);
            lastPlayedNote = -1;
        }
        if (activePadNote >= 0)
        {
            // Drum pads honour `oneShot` in the engine, so the note-off here
            // is harmless for one-shots and correctly releases sustained pads.
            proc.handleNoteOff (activePadNote);
            activePadNote = -1;
            repaint();
        }
        if (activeMomentaryParameter.isNotEmpty())
        {
            if (const auto* def = parameterForId (activeMomentaryParameter))
                proc.setPackParameterFromUi (activeMomentaryParameter, def->min);
            activeMomentaryParameter.clear();
            repaint();
        }
        drumGridDragActive = false;
        arpLaneDragActive = false;
        lastDrumGridPattern = -1;
        lastDrumGridTrack = -1;
        lastDrumGridStep = -1;
        mixerDragActive = false;
        mixerDragChannel = -1;
        mixerDragElementId.clear();
        mixerDragKind.clear();
        arpMidiDragArmed = false;
        arpMidiDragElementId.clear();
    }

    const LayoutElement* PlayerGuiRenderer::findBindableElementAt (juce::Point<int> position) const
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return nullptr;

        const auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible)
                continue;
            if (e.type == ElementType::Group)
                continue;
            if (! isElementOnCurrentTab (e))
                continue;
            if (elementRect (e, m).contains (position))
                return &e;
        }
        return nullptr;
    }

    void PlayerGuiRenderer::showControlContextMenu (const LayoutElement& element,
                                                    const juce::Point<int>& screenPos)
    {
        juce::PopupMenu menu;
        const auto mapping = element.parameterId.isNotEmpty() ? proc.getMidiMappingSummary (element.parameterId) : juce::String();
        const bool learnable = element.parameterId.isNotEmpty() && proc.isParameterMidiLearnable (element.parameterId);
        const auto* parameter = element.parameterId.isNotEmpty() ? parameterForId (element.parameterId) : nullptr;
        juce::String prerequisiteId;
        juce::String prerequisiteName;
        float prerequisiteValue = 1.0f;
        if (parameter != nullptr && parameter->enabledBy.isNotEmpty())
        {
            if (const auto* prerequisite = parameterForId (parameter->enabledBy))
            {
                prerequisiteId = prerequisite->id;
                prerequisiteName = prerequisite->name.isNotEmpty() ? prerequisite->name : prerequisite->id;
                prerequisiteValue = prerequisite->displayMode == "toggle"
                    ? prerequisite->max : juce::jmax (prerequisite->defaultValue, prerequisite->min + (prerequisite->max - prerequisite->min) * 0.5f);
            }
        }
        if (element.parameterId.isNotEmpty())
        {
            menu.addItem (1, "MIDI Learn", learnable);
            menu.addItem (2, mapping.isNotEmpty() ? "Clear MIDI Mapping (" + mapping + ")" : "Clear MIDI Mapping",
                          mapping.isNotEmpty());
            if (prerequisiteId.isNotEmpty())
            {
                menu.addSeparator();
                menu.addItem (4, "Enable prerequisite: " + prerequisiteName, true);
            }
            menu.addSeparator();
            menu.addItem (3, "Cancel Learn", true);
            menu.addSeparator();
        }
        juce::PopupMenu animationMenu;
        animationMenu.addItem (101, "None");
        animationMenu.addItem (102, "Breathe");
        animationMenu.addItem (103, "Pulse");
        animationMenu.addItem (104, "Glow");
        animationMenu.addItem (105, "Shake");
        animationMenu.addItem (106, "Audio Reactive Glow");
        menu.addSubMenu ("Runtime Animation", animationMenu);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (220)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, parameterId = element.parameterId, elementId = element.id, prerequisiteId, prerequisiteValue] (int result)
            {
                if (result == 1)
                {
                    proc.beginMidiLearn (parameterId);
                    repaint();
                }
                else if (result == 2)
                {
                    proc.removeMidiMappingForParameter (parameterId);
                    repaint();
                }
                else if (result == 3)
                {
                    proc.clearMidiLearn();
                    repaint();
                }
                else if (result == 4 && prerequisiteId.isNotEmpty())
                {
                    proc.setPackParameterFromUi (prerequisiteId, prerequisiteValue);
                    refreshControlEnablement();
                    repaint();
                }
                else if (result >= 101 && result <= 106)
                {
                    for (auto& e : elementsCopy)
                    {
                        if (e.id != elementId)
                            continue;

                        e.animationRate = result == 105 ? 2.0f
                                        : result == 102 ? 0.55f
                                        : 1.0f;
                        e.audioReactive = result == 106;
                        e.audioReactiveMode = "level";
                        e.audioReactiveAmount = result == 106 ? 0.85f : 0.35f;
                        e.animationMode = result == 101 ? "none"
                                        : result == 102 ? "breathe"
                                        : result == 103 ? "pulse"
                                        : result == 104 ? "glow"
                                        : result == 105 ? "shake"
                                        : "glow";
                        hasMeterOrReactiveElement = true;
                        resized();
                        repaint();
                        break;
                    }
                }
            });
    }

    void PlayerGuiRenderer::timerCallback()
    {
        // Only repaint when something meaningful changed. Previously this
        // forced a full-canvas repaint at 30 Hz, which serialised behind
        // every knob/slider drag and caused noticeable lag.
        bool needsRepaint = false;

        const auto pending = proc.getPendingMidiLearnParameter();
        if (pending != lastPendingMidiLearn)
        {
            lastPendingMidiLearn = pending;
            needsRepaint = true;
        }

        refreshControlEnablement();   // updates control enabled state if any
        if (advanceGranularFields())
            needsRepaint = true;

        if (proc.decayNoteHighlightLevels())
            needsRepaint = true;

        if (proc.isAnyTransportPlaying())
            needsRepaint = true;

        for (const auto& e : elementsCopy)
        {
            if (! e.visible || ! isElementOnCurrentTab (e))
                continue;
            if (e.animationMode.isNotEmpty() && e.animationMode != "none")
            {
                resized();
                needsRepaint = true;
                break;
            }
        }

        // If the layout has metering / audio-reactive elements, repaint when
        // the output peak shifted enough to be visible. Otherwise the static
        // controls don't need a tick-driven repaint at all.
        if (hasMeterOrReactiveElement)
        {
            const float peak = proc.getOutputPeak();
            if (std::abs (peak - lastOutputPeak) > 0.01f)
            {
                lastOutputPeak = peak;
                needsRepaint = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    void PlayerGuiRenderer::showParameterMenu (const LayoutElement& element,
                                               const juce::Point<int>& screenPos)
    {
        const auto* def = parameterForId (element.parameterId);
        if (def == nullptr)
            return;

        juce::PopupMenu menu;
        std::vector<float> values;
        if (def->id == "arpLaneMode")
        {
            values = { 0.0f, 1.0f };
            menu.addItem (1, "Bank");
            menu.addItem (2, "Performance");
        }
        else if (def->id == "arpLaneTarget")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
            menu.addItem (1, "Notes");
            menu.addItem (2, "Drums");
            menu.addItem (3, "One Shots");
            menu.addItem (4, "Loops");
            menu.addItem (5, "Samples");
        }
        else if (def->id == "arpLaneDirection")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f };
            menu.addItem (1, "Forward");
            menu.addItem (2, "Reverse");
            menu.addItem (3, "Bounce");
            menu.addItem (4, "Random");
        }
        else if (def->id == "arpLaneControlBank")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
            for (int bank = 0; bank < 5; ++bank)
                menu.addItem (bank + 1, "Lane " + juce::String (bank + 1));
        }
        else if (def->id == "arpLaneSliderRole")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
            menu.addItem (1, "Velocity");
            menu.addItem (2, "Gate");
            menu.addItem (3, "Probability");
            menu.addItem (4, "Ratchet");
            menu.addItem (5, "Mute / Active");
            menu.addItem (6, "Delay");
            menu.addItem (7, "Sample Slice");
            menu.addItem (8, "Transpose");
            menu.addItem (9, "Filter");
            menu.addItem (10, "Pan");
            menu.addItem (11, "FX Send");
        }
        else if (def->id == "granularDirection")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f };
            menu.addItem (1, "Forward");
            menu.addItem (2, "Reverse");
            menu.addItem (3, "Ping-Pong");
            menu.addItem (4, "Multi Direction");
        }
        else if (def->id == "granularWindow")
        {
            values = { 0.0f, 1.0f, 2.0f, 3.0f };
            menu.addItem (1, "Hann");
            menu.addItem (2, "Triangle");
            menu.addItem (3, "Blackman");
            menu.addItem (4, "Smooth Plateau");
        }
        else if (def->displayMode == "toggle")
        {
            values = { def->min, def->max };
            menu.addItem (1, "Off");
            menu.addItem (2, "On");
        }
        else if (def->step >= 1.0f && def->max - def->min <= 64.0f)
        {
            for (int value = (int) def->min; value <= (int) def->max; value += (int) juce::jmax (1.0f, def->step))
            {
                values.push_back ((float) value);
                menu.addItem ((int) values.size(), juce::String (value) + (def->unit.isNotEmpty() ? " " + def->unit : ""));
            }
        }
        else
        {
            values = { def->min, def->defaultValue, def->max };
            menu.addItem (1, "Min  " + juce::String (def->min, 2));
            menu.addItem (2, "Default  " + juce::String (def->defaultValue, 2));
            menu.addItem (3, "Max  " + juce::String (def->max, 2));
        }

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (200)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, parameterId = element.parameterId, values] (int result)
            {
                if (result <= 0 || result > (int) values.size()) return;
                proc.setPackParameterFromUi (parameterId, values[(size_t) result - 1]);
                repaint();
            });
    }

    int PlayerGuiRenderer::noteForKeyboardPosition (juce::Rectangle<int> r, juce::Point<int> pos) const
    {
        const auto whiteNotes = pianoWhiteNotes();
        const float kw = (float) (r.getWidth() - 12) / (float) whiteNotes.size();
        const float keyTop = (float) r.getY() + 6.0f;
        const float keyH   = (float) r.getHeight() - 12.0f;
        const float bkH = keyH * 0.62f;
        const float bkW = kw * 0.62f;

        // Check black keys first because they are painted above white keys.
        for (int midiNote = kPianoFirstMidiNote; midiNote <= kPianoLastMidiNote; ++midiNote)
        {
            if (! isBlackPianoKey (midiNote))
                continue;

            const int before = whiteNotesBefore (midiNote);
            const float x = r.getX() + 6 + before * kw - bkW * 0.5f;
            if (x < r.getX() + 4 || x + bkW > r.getRight() - 4)
                continue;

            juce::Rectangle<float> key (x, keyTop, bkW, bkH);
            if (key.contains (pos.toFloat()))
                return midiNote;
        }

        for (int i = 0; i < (int) whiteNotes.size(); ++i)
        {
            juce::Rectangle<float> key (r.getX() + 6 + i * kw, keyTop, kw - 1.0f, keyH);
            if (key.contains (pos.toFloat()))
                return whiteNotes[(size_t) i];
        }

        return -1;
    }

    void PlayerGuiRenderer::handleKeyboardClick (juce::Rectangle<int> r, juce::Point<int> pos)
    {
        const int midiNote = noteForKeyboardPosition (r, pos);
        if (midiNote < 0)
            return;

        if (lastPlayedNote >= 0 && lastPlayedNote != midiNote)
            proc.handleNoteOff (lastPlayedNote);

        proc.handleNoteOn (midiNote);
        lastPlayedNote = midiNote;
        repaint (r.expanded (4));
    }

    void PlayerGuiRenderer::showPresetMenu (const juce::Point<int>& screenPos)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || pack->presets.empty()) return;

        juce::PopupMenu menu;
        for (size_t i = 0; i < pack->presets.size(); ++i)
            menu.addItem ((int) i + 1, pack->presets[i].name);

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (200)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this] (int r)
            {
                if (r <= 0) return;
                const auto* p = proc.getPack();
                if (p == nullptr) return;
                const int idx = r - 1;
                if (idx < 0 || idx >= (int) p->presets.size()) return;

                proc.applyPresetByIndex (idx);
                refreshControlEnablement();
                repaint();
            });
    }

} // namespace patchcraft
