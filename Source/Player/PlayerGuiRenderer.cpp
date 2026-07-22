#include "PlayerGuiRenderer.h"
#include "PluginProcessor.h"
#include "PatchCraftLookAndFeel.h"
#include "LicenseValidator.h"
#include "MidiPlaygroundPattern.h"
#include "HarmonyEngine.h"
#include "ArpLaneUi.h"
#include "LayoutCustomerView.h"
#include "DrumMachineUtil.h"
#include "ParameterModel.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <map>
#include <vector>

namespace patchcraft
{
    static const SampleZoneDef* sampleZoneForPad (const PatchCraftPack* pack, int padIndex)
    {
        if (pack == nullptr || padIndex < 0 || padIndex >= 16)
            return nullptr;

        const auto& zones = pack->sampleMap.getZones();
        for (const auto& zone : zones)
            if (zone.padIndex == padIndex)
                return &zone;

        std::array<bool, 16> occupied {};
        std::map<int, int> padByRoot;
        for (const auto& zone : zones)
        {
            if (zone.padIndex < 0 || zone.padIndex >= (int) occupied.size())
                continue;
            occupied[(size_t) zone.padIndex] = true;
            padByRoot.emplace (zone.rootNote, zone.padIndex);
        }

        int nextPad = 0;
        for (const auto& zone : zones)
        {
            if (zone.padIndex >= 0)
                continue;
            if (const auto found = padByRoot.find (zone.rootNote); found != padByRoot.end())
            {
                if (found->second == padIndex)
                    return &zone;
                continue;
            }
            while (nextPad < (int) occupied.size() && occupied[(size_t) nextPad])
                ++nextPad;
            if (nextPad >= (int) occupied.size())
                break;
            occupied[(size_t) nextPad] = true;
            padByRoot[zone.rootNote] = nextPad;
            if (nextPad == padIndex)
                return &zone;
            ++nextPad;
        }
        return nullptr;
    }

    static juce::String steppedParameterChoiceLabel (const ParameterDef& def, float value)
    {
        const int index = juce::jlimit (0, 127, juce::roundToInt (value));
        auto pick = [&index] (const juce::StringArray& choices) -> juce::String
        {
            if (choices.isEmpty())
                return {};
            return choices[juce::jlimit (0, choices.size() - 1, index)];
        };

        if (def.id == "oscType" || def.id == "osc2Type")
            return pick ({ "Sine", "Saw", "Square", "Triangle" });
        if (def.id == "wtTable")
            return pick ({ "Analog", "Glass", "PWM", "Formant", "Razor", "Organ", "Aggro", "Hybrid", "Custom" });
        if (def.id == "wtPhaseMode")
            return pick ({ "Reset", "Random", "Unison Spread" });
        if (def.id == "wtSyncRatio")
            return juce::String (juce::jlimit (1, 8, index)) + "x";

        if (def.displayMode == "stepped" || def.step >= 1.0f)
            return juce::String (index) + (def.unit.isNotEmpty() ? " " + def.unit : "");
        return {};
    }

    static bool populateSteppedParameterMenu (const ParameterDef& def,
                                              juce::PopupMenu& menu,
                                              std::vector<float>& values)
    {
        auto addChoices = [&menu, &values] (const juce::StringArray& choices)
        {
            for (int i = 0; i < choices.size(); ++i)
            {
                values.push_back ((float) i);
                menu.addItem (values.size(), choices[i]);
            }
        };

        if (def.id == "oscType" || def.id == "osc2Type")
        {
            addChoices ({ "Sine", "Saw", "Square", "Triangle" });
            return true;
        }
        if (def.id == "wtTable")
        {
            addChoices ({ "Analog", "Glass", "PWM", "Formant", "Razor", "Organ", "Aggro", "Hybrid", "Custom" });
            return true;
        }
        if (def.id == "wtPhaseMode")
        {
            addChoices ({ "Reset", "Random", "Unison Spread" });
            return true;
        }
        if (def.id == "wtSyncRatio")
        {
            for (int ratio = 1; ratio <= 8; ++ratio)
            {
                values.push_back ((float) ratio);
                menu.addItem (values.size(), juce::String (ratio) + "x");
            }
            return true;
        }
        return false;
    }

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

    static juce::String orbitLaneSoundName (int sound)
    {
        return "DSP Slot " + juce::String (juce::jlimit (0, 15, sound) + 1);
    }

    static juce::String circleSeqPatternName (int preset)
    {
        static const char* names[] =
        {
            "Pentatonic Pulse", "Bass Anchor", "Melody Answer", "Bell Topline",
            "Soft Syncopation", "Arp Climb", "Open Fifths", "Reset Empty"
        };
        return names[(size_t) juce::jlimit (0, 7, preset)];
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
            instrumentName = "Instrument";
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
            || ext == ".mp3" || ext == ".ogg" || ext == ".mid" || ext == ".midi";
    }

    static bool isRuntimeImportFileForPlayerCanvas (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"
            || ext == ".mp3" || ext == ".ogg" || ext == ".mid" || ext == ".midi";
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

    static int runtimeTargetNoteFromElement (const LayoutElement& element, int fallback)
    {
        auto parse = [] (juce::String text) -> int
        {
            text = text.trim();
            if (text.isEmpty())
                return -1;

            for (const auto& prefix : { juce::String ("sampleNote:"),
                                        juce::String ("targetNote:"),
                                        juce::String ("midiNote:"),
                                        juce::String ("note:") })
            {
                if (text.startsWithIgnoreCase (prefix))
                {
                    const int note = text.fromFirstOccurrenceOf (":", false, false).trim().getIntValue();
                    return juce::isPositiveAndBelow (note, 128) ? note : -1;
                }
            }
            return -1;
        };

        if (const int note = parse (element.semanticRole); note >= 0) return note;
        if (const int note = parse (element.action); note >= 0)       return note;
        return fallback;
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
            return "This control is not available in this instrument.";

        if (parameter == nullptr)
            return "This control points to missing parameter '" + element.parameterId + "'. The exported layout and parameter registry are out of sync.";

        if (parameter->id == "arpLaneSound")
            return "DSP Source Slot (arpLaneSound)\nSelects a DSP-owned pad/sample/slice slot. It does not create a separate ArpLane sound.";

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

    void PlayerGuiRenderer::filesDropped (const juce::StringArray& files, int x, int y)
    {
        if (proc.getPack() == nullptr)
            return;

        auto runtimeFiles = collectRuntimeImportFilesForPlayerCanvas (files);

        if (runtimeFiles.isEmpty())
            return;

        juce::String report;
        const auto target = runtimeDropTargetAt ({ x, y });
        proc.importUserContentFiles (runtimeFiles, target.mappingMode, report, target.note, target.padIndex);
        if (onRuntimeImportReport)
            onRuntimeImportReport (report);
        fileDragActive = false;
        fileDragHighlight = {};
        fileDragLabel = {};
        rebuild();
        repaint();
    }

    void PlayerGuiRenderer::fileDragEnter (const juce::StringArray& files, int x, int y)
    {
        if (! isInterestedInFileDrag (files))
            return;
        fileDragActive = true;
        updateFileDragHighlight ({ x, y }, files);
    }

    void PlayerGuiRenderer::fileDragMove (const juce::StringArray& files, int x, int y)
    {
        if (! fileDragActive)
        {
            if (! isInterestedInFileDrag (files))
                return;
            fileDragActive = true;
        }
        updateFileDragHighlight ({ x, y }, files);
    }

    void PlayerGuiRenderer::fileDragExit (const juce::StringArray&)
    {
        if (! fileDragActive && fileDragHighlight.isEmpty())
            return;
        fileDragActive = false;
        fileDragHighlight = {};
        fileDragLabel = {};
        repaint();
    }

    void PlayerGuiRenderer::updateFileDragHighlight (juce::Point<int> pos,
                                                     const juce::StringArray& files)
    {
        juce::Rectangle<int> newBounds;
        juce::String newLabel;
        bool hasMidi = false;
        bool hasAudio = false;
        for (const auto& path : collectRuntimeImportFilesForPlayerCanvas (files))
        {
            const auto ext = juce::File (path).getFileExtension().toLowerCase();
            hasMidi = hasMidi || ext == ".mid" || ext == ".midi";
            hasAudio = hasAudio || (ext != ".mid" && ext != ".midi");
        }
        const auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& element = *it;
            if (! element.visible || ! isElementOnCurrentTab (element))
                continue;

            const auto bounds = animatedElementRect (element, elementRect (element, m));
            if (! bounds.contains (pos))
                continue;

            if (element.type == ElementType::SampleDropZone)
            {
                newBounds = bounds;
                newLabel = hasMidi && ! hasAudio
                    ? "Drop MIDI to replace the active musical pattern"
                    : "Drop sample to pitch and play across the keyboard";
            }
            else if (element.type == ElementType::RuntimeSampleLibrary)
            {
                newBounds = bounds;
                newLabel = "Drop samples to add";
            }
            else if (element.type == ElementType::Keyboard)
            {
                newBounds = bounds;
                newLabel = "Drop to map across the keyboard";
            }
            else if (element.type == ElementType::DrumPad || element.type == ElementType::PadGrid)
            {
                if (padNoteAt (element, bounds, pos) >= 0)
                {
                    newBounds = bounds;
                    newLabel = "Drop to assign pad";
                }
            }
            else if (hasMidi && element.type == ElementType::PianoRoll)
            {
                newBounds = bounds;
                newLabel = "Drop MIDI to replace this editable piano roll";
            }
            else if (hasMidi && element.type == ElementType::ArpLane)
            {
                newBounds = bounds;
                newLabel = "Drop MIDI to import this sequencer pattern";
            }

            if (! newBounds.isEmpty())
                break;
        }

        if (newBounds != fileDragHighlight || newLabel != fileDragLabel)
        {
            fileDragHighlight = newBounds;
            fileDragLabel = newLabel;
            repaint();
        }
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

    juce::Rectangle<int> PlayerGuiRenderer::arpLaneBypassButtonBounds (juce::Rectangle<int> elementBounds) const
    {
        auto header = elementBounds.reduced (10).removeFromTop (26);
        header.removeFromRight (72);
        header.removeFromRight (48);
        header.removeFromRight (26);
        return header.removeFromRight (26).reduced (2, 3);
    }

    juce::Rectangle<int> PlayerGuiRenderer::arpLaneSoloButtonBounds (juce::Rectangle<int> elementBounds) const
    {
        auto header = elementBounds.reduced (10).removeFromTop (26);
        header.removeFromRight (72);
        header.removeFromRight (48);
        header.removeFromRight (26);
        return header.removeFromRight (26).reduced (2, 3);
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
                return "Playable drum pads. Click pads, use hardware pads/keys, or drag WAV/AIFF/FLAC files here to add session samples.";

            if (e.type == ElementType::DrumGrid)
                return "MIDI drum pattern grid. Click cells to add/remove hits, Ctrl-click cells for x2/x3/x4 divisions, then press DAW Play or the Player PLAY button to audition.";

            if (e.type == ElementType::ArpLane)
            {
                if (arpLaneMidiDragHandleBounds (r).contains (pos))
                    return "Drag this handle to a DAW track to export this lane as a MIDI clip.";

                return "Circular ArpLane performance bank. DSP makes the sound; this lane edits notes, velocity, gates, FX sends, and timing for that DSP patch. Drag dots while playing to hear the change.";
            }

            if (e.type == ElementType::PianoRoll)
                return "Editable piano roll. Click an empty cell to add a note, drag right to lengthen it, click a note to remove it. Press DAW Play or the PLAY button to hear it.";

            if (e.type == ElementType::Keyboard)
                return "Software keyboard. Click keys to audition this instrument and imported runtime samples.";

            if (e.type == ElementType::Waveform)
                return "Playback display. The vertical playhead follows the DAW transport or the Player PLAY audition button.";

            if (e.type == ElementType::EqCurve)
                return "EQ curve display. Shows the active surgical EQ blocks in this patch so users can see what is shaping the tone.";

            if (e.type == ElementType::SpectrumAnalyzer)
                return "Spectrum analyzer. Shows live output energy so users can see how the current sound is moving.";

            if (e.type == ElementType::Mixer)
                return "Runtime mixer. Drag faders and pan areas when this instrument exposes live mix controls.";

            if (e.type == ElementType::GranularField)
                return "Granular performance field. Drag inside the field to move sample position and texture controls.";

            if (e.type == ElementType::TabPanel)
                return "Instrument page tabs. Click a tab to switch the visible controls for this Player UI.";

            if (e.type == ElementType::Dropdown)
                return e.id == "presets"
                    ? "Preset selector for this instrument."
                    : "Parameter menu for this instrument.";

            if (e.type == ElementType::Button || e.type == ElementType::Toggle)
            {
                if (e.parameterId.isEmpty())
                    return "This button is not available in this instrument.";
            }

            if (e.type == ElementType::Keyboard)
                return "Software keyboard. Click keys to audition this instrument and imported runtime samples.";

            if (e.type == ElementType::RuntimeSampleLibrary)
                return "Runtime sample/MIDI library. Drop samples or MIDI here, or drop files directly onto pads and sample zones to target playback.";

            if (e.type == ElementType::Waveform)
                return "Playback display. The vertical playhead follows the DAW transport or the Player PLAY audition button.";

            if (e.type == ElementType::EqCurve)
                return "EQ curve display. Shows the active surgical EQ blocks in this patch so users can see what is shaping the tone.";

            if (e.type == ElementType::SpectrumAnalyzer)
                return "Spectrum analyzer. Shows live output energy so users can see how the current sound is moving.";

            if (e.type == ElementType::Mixer)
                return "Runtime mixer. Drag faders and pan areas when this instrument exposes live mix controls.";

            if (e.type == ElementType::GranularField)
                return "Granular performance field. Drag inside the field to move sample position and texture controls.";

            if (e.type == ElementType::TabPanel)
                return "Instrument page tabs. Click a tab to switch the visible controls for this Player UI.";

            if (e.type == ElementType::Dropdown)
                return e.id == "presets"
                    ? "Preset selector for this instrument."
                    : "Parameter menu for this instrument.";

            if (e.type == ElementType::Button || e.type == ElementType::Toggle)
            {
                if (e.parameterId.isEmpty())
                    return "This button is not available in this instrument.";
                if (const auto* parameter = parameterForId (e.parameterId))
                    return parameter->name + " (" + parameter->id + ")";
                return "This button points to missing parameter '" + e.parameterId + "'.";
            }

            if (e.type == ElementType::Knob || e.type == ElementType::Slider
                || e.type == ElementType::ValueDisplay || e.type == ElementType::XYPad
                || e.type == ElementType::MacroControl
                || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
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

        bool hasTabPanels = false;
        for (const auto& parent : elementsCopy)
        {
            if ((parent.type == ElementType::Group || parent.type == ElementType::Panel)
                && parent.id == e.groupId)
                return true;
            if (parent.type == ElementType::TabPanel)
            {
                hasTabPanels = true;
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
        if (! hasTabPanels && ! isScopedTabGroupId (e.groupId))
            return true;
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
        return proc.getHostParameterSlotIndex (parameterId == "pitchBend" ? juce::String ("pitchWheel") : parameterId);
    }

    const ParameterDef* PlayerGuiRenderer::parameterForId (const juce::String& parameterId) const
    {
        const auto* pack = proc.getPack();
        const auto effectiveParameterId = parameterId == "pitchBend"
            ? juce::String ("pitchWheel")
            : parameterId;
        if (pack == nullptr || effectiveParameterId.isEmpty())
            return nullptr;

        for (const auto& def : pack->parameters.getAll())
            if (def.id == effectiveParameterId)
                return &def;

        ParameterDef registryDef;
        const auto engine = pack->manifest.engine.isNotEmpty() ? pack->manifest.engine : juce::String ("synth");
        if (ParameterModel::getRegistryDefinition (effectiveParameterId, engine, registryDef))
        {
            registryParameterCache[effectiveParameterId] = registryDef;
            return &registryParameterCache[effectiveParameterId];
        }

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
                                      || element.type == ElementType::MacroControl
                                      || element.type == ElementType::PitchWheel
                                      || element.type == ElementType::ModWheel;
            if (! isSliderControl)
                continue;

            const auto* def = parameterForId (element.parameterId);
            const int slotIndex = parameterIndexForId (element.parameterId);
            const bool mapped = def != nullptr && slotIndex >= 0 && slotIndex < kPatchCraftHostParameterSlots;
            const bool internalRuntimeControl = def != nullptr && isRuntimePerformanceParameter (element.parameterId);
            const bool visualOnlyControl = element.parameterId.isEmpty()
                && (element.type == ElementType::Knob || element.type == ElementType::Slider);
            const bool enabled = def != nullptr && (mapped || internalRuntimeControl || ! element.parameterId.isEmpty())
                              && parameterIsEnabled (*def);
            control->setEnabled (enabled || visualOnlyControl);
            control->setAlpha (0.01f);
        }
    }

    float PlayerGuiRenderer::parameterValueForElement (const LayoutElement& element, float fallback) const
    {
        const auto effectiveParameterId = element.parameterId == "pitchBend"
            ? juce::String ("pitchWheel")
            : element.parameterId;
        const int index = parameterIndexForId (effectiveParameterId);
        const auto* def = parameterForId (effectiveParameterId);
        if (index >= 0 && index < kPatchCraftHostParameterSlots && def != nullptr)
        {
            if (auto* value = proc.getApvts().getRawParameterValue (slotId (index)))
                return juce::jmap (juce::jlimit (0.0f, 1.0f, value->load()), 0.0f, 1.0f, def->min, def->max);
        }
        return def != nullptr ? proc.getPackParameterValue (effectiveParameterId) : fallback;
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

        if (def->id == "arpLaneMode")
            return choiceText ({ "Bank", "Performance" });
        if (def->id == "arpLaneTarget")
            return choiceText ({ "Notes", "Drums", "One Shots", "Loops", "Samples" });
        if (def->id == "arpLaneDirection")
            return choiceText ({ "Forward", "Reverse", "Bounce", "Random" });
        if (def->id == "arpLaneControlBank")
            return "Lane " + juce::String (juce::jlimit (0, 15, juce::roundToInt (value)) + 1);
        if (def->id == "arpLaneGroup")
            return "Group " + juce::String (juce::jlimit (0, 7, juce::roundToInt (value)) + 1);
        if (def->id == "arpLaneSliderRole")
            return choiceText ({ "Velocity", "Gate", "Chance", "Ratchet", "On/Off", "Delay", "Slice", "Transpose", "Filter", "Pan", "FX Send" });
        if (def->id == "arpLaneSound")
            return orbitLaneSoundName (juce::roundToInt (value));
        if (def->id == "arpLaneFxTarget")
            return choiceText ({ "Delay", "Reverb", "Chorus", "Phaser", "Drive", "Resonance", "Width", "Tape" });
        if (def->id == "arpLanePatternLaunch")
            return circleSeqPatternName (juce::roundToInt (value));
        if (def->id == "composerRoot")
            return HarmonyEngine::pitchClassName (juce::roundToInt (value));
        if (def->id == "composerScale")
            return HarmonyEngine::scaleAt (juce::roundToInt (value)).name;
        if (def->id.startsWith ("composerDegree"))
            return choiceText ({ "I", "ii", "iii", "IV", "V", "vi", "vii" });

        if (const auto stepped = steppedParameterChoiceLabel (*def, value); stepped.isNotEmpty())
            return stepped;

        if (def->displayMode == "toggle" || def->displayMode == "stepped" || def->step >= 1.0f)
            return juce::String (juce::roundToInt (value)) + (def->unit.isNotEmpty() ? " " + def->unit : "");
        return juce::String (value, def->unit == "Hz" || def->unit == "ms" ? 1 : 2)
            + (def->unit.isNotEmpty() ? " " + def->unit : "");
    }

    juce::Rectangle<int> PlayerGuiRenderer::animatedElementRect (const LayoutElement& element,
                                                                 juce::Rectangle<int> rect) const
    {
        float driveVal = 0.0f;
        if (element.parameterId.isNotEmpty())
        {
            if (const auto* def = parameterForId (element.parameterId))
            {
                const float val = parameterValueForElement (element, def->defaultValue);
                const float range = def->max - def->min;
                driveVal = range > 0.0001f ? juce::jlimit (0.0f, 1.0f, (val - def->min) / range) : 0.0f;
            }
        }

        float amount = 0.0f;
        if (element.audioReactive)
            amount += proc.getAudioReactiveSignal (element.audioReactiveMode) * juce::jmax (0.10f, element.audioReactiveAmount);

        if (element.animationMode != "none")
        {
            const auto seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const float rate = juce::jmax (0.05f, element.animationRate);
            const float wave = (float) ((std::sin (seconds * juce::MathConstants<double>::twoPi * rate) * 0.5) + 0.5);

            float animationAmount = 0.0f;
            if (element.animationMode == "parameter")
            {
                animationAmount = driveVal;
            }
            else if (element.animationMode == "bpmPulse")
            {
                const double bpm = proc.getHostBpm();
                const double bps = bpm / 60.0;
                animationAmount = (float) ((std::sin (seconds * bps * juce::MathConstants<double>::twoPi) * 0.5) + 0.5);
            }
            else
            {
                animationAmount = wave;
            }

            if (element.animationMode == "shake")
            {
                rect.translate (juce::roundToInt ((wave - 0.5f) * 10.0f * juce::jmax (0.25f, element.audioReactiveAmount)), 0);
                return rect;
            }

            const bool isScale = element.visualAction.equalsIgnoreCase ("Scale") || element.animationMode == "breathe" || element.animationMode == "pulse";
            if (isScale)
            {
                amount += animationAmount * 0.8f;
            }
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
            const auto id = e.id.toLowerCase();
            const bool titleLike = id == "title" || id == "instrument_title" || id == "instrument_name"
                                || id == "player_title" || id.contains ("title") || id.contains ("product_name");
            const bool taglineLike = id == "tagline" || id == "subtitle" || id == "sub_title"
                                  || id == "player_tagline" || id.contains ("tagline") || id.contains ("subtitle");
            const bool creatorLike = id == "creator" || id == "artist" || id == "brand_name"
                                  || id.contains ("creator") || id.contains ("artist") || id.contains ("brand");

            if (titleLike && m->playerDisplayName.isNotEmpty())
                text = m->playerDisplayName;
            else if (taglineLike && m->playerTagline.isNotEmpty())
                text = m->playerTagline;
            else if (creatorLike && m->creator.isNotEmpty())
                text = m->creator;
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
        // Wider band + fit-to-box below so full words like CHANCE / CUTOFF render
        // instead of being clipped to "CHA..." in compact knob grids.
        const int defaultH = juce::jmax (24, juce::roundToInt (r.getHeight() * 0.34f));
        if (e.labelPosition == "top")
            labelArea = r.removeFromTop (defaultH).translated (juce::roundToInt (e.labelOffsetX),
                                                               juce::roundToInt (e.labelOffsetY));
        else if (e.labelPosition == "left")
            labelArea = { r.getX() - r.getWidth() - juce::roundToInt (e.labelSpacing),
                          r.getCentreY() - 18,
                          r.getWidth(),
                          36 };
        else if (e.labelPosition == "right")
            labelArea = { r.getRight() + juce::roundToInt (e.labelSpacing),
                          r.getCentreY() - 18,
                          r.getWidth(),
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
        const float fontSize = e.labelSize > 0.0f ? e.labelSize : juce::jmax (9.0f, labelArea.getHeight() * 0.40f);
        g.setColour (colour);
        g.setFont (juce::FontOptions (fontSize).withStyle ("bold"));
        g.drawFittedText (labelText.toUpperCase(),
                          labelArea.removeFromTop (juce::roundToInt (labelArea.getHeight() * 0.60f)),
                          juce::Justification::centred, 2, 0.5f);

        juce::String valueText;
        if (parameterForId (e.parameterId) != nullptr)
            valueText = formattedParameterValue (e);
        else if (e.parameterId.isNotEmpty())
            valueText = "missing";

        if (valueText.isNotEmpty())
        {
            g.setColour (e.accentColour.isTransparent() ? playerAccent() : e.accentColour);
            g.setFont (juce::FontOptions (juce::jmax (8.0f, fontSize * 0.85f)));
            g.drawFittedText (valueText, labelArea, juce::Justification::centred, 1, 0.7f);
        }
    }

    juce::Rectangle<int> PlayerGuiRenderer::controlBodyRect (const LayoutElement& e,
                                                             juce::Rectangle<int> r) const
    {
        if (e.labelPosition != "hidden"
            && (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel))
        {
            const int labelHeight = (e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
                ? juce::jmax (28, r.getHeight() / 3)
                : juce::jmax (20, r.getHeight() / 4);
            r.removeFromBottom (labelHeight);
        }
        return r.reduced (2);
    }

    void PlayerGuiRenderer::drawRuntimeControl (juce::Graphics& g,
                                                juce::Rectangle<int> r,
                                                const LayoutElement& e) const
    {
        const auto* def = parameterForId (e.parameterId);
        const auto minValue = def != nullptr ? def->min : 0.0f;
        const auto maxValue = def != nullptr ? def->max : 1.0f;
        const auto fallback = def != nullptr ? def->defaultValue : juce::jlimit (0.0f, 1.0f, e.controlPreviewValue);
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

        if ((e.type == ElementType::Knob || e.type == ElementType::Slider)
            && e.filmstripAsset.isNotEmpty())
        {
            if (const auto* pack = proc.getPack())
            {
                const auto filmstripFile = juce::File::isAbsolutePath (e.filmstripAsset)
                    ? juce::File (e.filmstripAsset)
                    : pack->rootFolder.getChildFile (e.filmstripAsset);

                if (auto strip = assets.loadControlFilmstrip (filmstripFile, e.filmstripFrames, e.filmstripVertical); strip.isValid())
                {
                    const int detectedFrames = PatchCraftLookAndFeel::detectFilmstripFrames (strip, e.filmstripVertical);
                    const int frames = e.filmstripFrames > 1
                        ? e.filmstripFrames
                        : juce::jmax (1, detectedFrames);
                    PatchCraftLookAndFeel::drawFilmstripFrame (g, r, strip, frames, norm, e.filmstripVertical);
                    return;
                }
            }
        }

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

        if (e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
        {
            const float wheelW = juce::jmax (12.0f, (float) r.getWidth() * 0.4f);
            auto wheel = r.toFloat().withSizeKeepingCentre (wheelW, (float) r.getHeight());
            g.setColour (juce::Colour (0xff080a0d));
            g.fillRoundedRectangle (wheel, 6.0f);
            
            juce::ColourGradient grad (juce::Colour (0xff2c3038), wheel.getX(), wheel.getY(),
                                       juce::Colour (0xff12141a), wheel.getX(), wheel.getBottom(), false);
            grad.addColour (0.5, juce::Colour (0xff3a3e46));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (wheel.reduced (2.0f), 4.0f);
            
            const float indY = juce::jmap (norm, wheel.getBottom() - 10.0f, wheel.getY() + 10.0f);
            g.setColour (accent);
            g.fillRect (wheel.getX() + 4.0f, indY - 2.0f, wheel.getWidth() - 8.0f, 4.0f);
            
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            for (float y = wheel.getY() + 12.0f; y < wheel.getBottom() - 12.0f; y += 8.0f)
                g.drawHorizontalLine (juce::roundToInt(y), wheel.getX() + 6.0f, wheel.getRight() - 6.0f);
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

        const float start = juce::degreesToRadians (-135.0f);
        const float end = juce::degreesToRadians (135.0f);
        juce::Path arc;
        arc.addCentredArc (dial.getCentreX(), dial.getCentreY(),
                           dial.getWidth() * 0.43f, dial.getHeight() * 0.43f,
                           0.0f, start, juce::jmap (norm, start, end), true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (2.0f, dial.getWidth() * 0.08f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        const float angle = juce::jmap (norm, start, end);
        const float radius = dial.getWidth() * 0.32f;
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
                targets.addArray ({ "No routes", "Add target", "Apply" });

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
        // Attachments hold raw pointers into controls; destroy them first.
        attachments.clear();
        controls.clear();
        elementsCopy.clear();
        registryParameterCache.clear();

        const auto* pack = proc.getPack();
        if (pack == nullptr) { background = {}; repaint(); return; }

        elementsCopy = pack->layout.getAll();

        // Cache whether the layout has any element whose paint depends on the
        // live output peak. If not, the timer doesn't need to repaint at all
        // when audio is playing — only when MIDI-learn / control enablement
        // state changes.
        hasMeterOrReactiveElement = false;
        hasContinuousFxElement = false;
        for (const auto& e : elementsCopy)
        {
            // FX layers / sprites / reactive images animate continuously and must
            // tick every frame regardless of audio peak movement.
            if (e.type == ElementType::VisualFxLayer
                || e.type == ElementType::SpriteAnimator
                || e.type == ElementType::ReactiveImage)
                hasContinuousFxElement = true;

            if (e.audioReactive
                || (e.animationMode.isNotEmpty() && e.animationMode != "none")
                || e.type == ElementType::Meter
                || e.type == ElementType::Waveform
                || e.type == ElementType::SpectrumAnalyzer
                || e.type == ElementType::GranularField
                || e.type == ElementType::ArpLane
                || e.type == ElementType::PianoRoll
                || e.type == ElementType::Mixer
                || e.type == ElementType::VisualFxLayer
                || e.type == ElementType::SpriteAnimator
                || e.type == ElementType::ReactiveImage)
            {
                hasMeterOrReactiveElement = true;
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
            auto f = juce::File::isAbsolutePath (pack->backgroundImageRelative)
                ? juce::File (pack->backgroundImageRelative)
                : pack->rootFolder.getChildFile (pack->backgroundImageRelative);
            background = assets.loadImage (f);
        }

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
            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl
                || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
            {
                auto slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle ((e.type == ElementType::Slider || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
                    ? juce::Slider::LinearVertical
                    : juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
                slider->setColour (juce::Slider::rotarySliderFillColourId, e.accentColour);
                slider->setAlpha (0.01f);

                slider->addMouseListener (this, false);
                const auto runtimeParameterId = e.parameterId == "pitchBend"
                    ? juce::String ("pitchWheel")
                    : e.parameterId;
                auto it = paramIndex.find (runtimeParameterId);
                const auto* parameter = parameterForId (runtimeParameterId);
                slider->setTooltip (playerControlGuidance (pack->manifest, e, parameter));
                if (parameter != nullptr)
                    slider->setDoubleClickReturnValue (true, parameter->defaultValue);
                const bool visualOnlyControl = e.parameterId.isEmpty()
                    && (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel);
                const bool performanceWheel = e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel;
                if (performanceWheel && parameter != nullptr)
                {
                    const double interval = parameter->step > 0.0f ? (double) parameter->step : 0.0;
                    auto* sliderPtr = slider.get();
                    sliderPtr->setRange ((double) parameter->min, (double) parameter->max, interval);
                    sliderPtr->setValue ((double) proc.getPackParameterValue (runtimeParameterId),
                                         juce::dontSendNotification);
                    sliderPtr->onValueChange = [this, sliderPtr, runtimeParameterId]
                    {
                        proc.setPackParameterFromUi (runtimeParameterId, (float) sliderPtr->getValue());
                        repaint();
                    };
                }
                else if (it != paramIndex.end() && it->second < kPatchCraftHostParameterSlots)
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
                    sliderPtr->setValue ((double) proc.getPackParameterValue (runtimeParameterId),
                                         juce::dontSendNotification);
                    sliderPtr->onValueChange = [this, sliderPtr, parameterId = runtimeParameterId]
                    {
                        proc.setPackParameterFromUi (parameterId, (float) sliderPtr->getValue());
                        repaint();
                    };
                    slider->setTooltip (playerControlGuidance (pack->manifest, e, parameter)
                        + "\nInternal Player control: it changes sound in real time but is not exposed as host automation.");
                }
                else
                {
                    slider->setRange (0.0, 1.0, 0.01);
                    slider->setValue ((double) juce::jlimit (0.0f, 1.0f, e.controlPreviewValue),
                                      juce::dontSendNotification);
                    slider->setDoubleClickReturnValue (true, (double) juce::jlimit (0.0f, 1.0f, e.controlPreviewValue));
                    slider->setEnabled (visualOnlyControl);
                    slider->setAlpha (0.01f);
                    if (visualOnlyControl)
                    {
                        auto* sliderPtr = slider.get();
                        sliderPtr->onValueChange = [this, sliderPtr, elementId = e.id]
                        {
                            const auto value = juce::jlimit (0.0f, 1.0f, (float) sliderPtr->getValue());
                            for (auto& element : elementsCopy)
                            {
                                if (element.id == elementId)
                                {
                                    element.controlPreviewValue = value;
                                    break;
                                }
                            }
                            repaint();
                        };
                        slider->setTooltip ({});
                    }
                    else
                    {
                        slider->setTooltip ("This control is not available in this instrument.");
                    }
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
        const bool vertical = r.getHeight() > r.getWidth() * 1.2f;
        float level = proc.getOutputPeak();
        if (element.parameterId.isNotEmpty())
        {
            if (const auto* def = parameterForId (element.parameterId))
            {
                const auto value = parameterValueForElement (element, def->defaultValue);
                level = juce::jlimit (0.0f, 1.0f, (value - def->min) / juce::jmax (0.0001f, def->max - def->min));
            }
        }

        // Custom image meter: pick the filmstrip frame for the current level.
        if (element.filmstripAsset.isNotEmpty())
        {
            const auto* pack = proc.getPack();
            auto file = juce::File::isAbsolutePath (element.filmstripAsset)
                            ? juce::File (element.filmstripAsset)
                            : (pack != nullptr ? pack->rootFolder.getChildFile (element.filmstripAsset) : juce::File());
            if (auto strip = assets.loadControlFilmstrip (file, element.filmstripFrames, element.filmstripVertical); strip.isValid())
            {
                const int frames = element.filmstripFrames > 0
                    ? element.filmstripFrames
                    : PatchCraftLookAndFeel::detectFilmstripFrames (strip, element.filmstripVertical);
                PatchCraftLookAndFeel::drawFilmstripFrame (g, r, strip, juce::jmax (1, frames),
                                                           juce::jlimit (0.0f, 1.0f, level), element.filmstripVertical);
                return;
            }
        }

        g.setColour (juce::Colour (0xff05060a));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        const int segs = vertical ? 16 : 24;
        const auto inner = r.reduced (4);
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

    float PlayerGuiRenderer::reactiveLevelFor (const LayoutElement& e) const
    {
        const float raw = e.audioReactive
            ? proc.getAudioReactiveSignal (e.audioReactiveMode)
            : proc.getOutputPeak();
        const float gain = 0.4f + juce::jmax (0.0f, e.audioReactiveAmount) * 1.4f;
        return juce::jlimit (0.0f, 1.0f, raw * gain);
    }

    void PlayerGuiRenderer::drawVisualFx (juce::Graphics& g, juce::Rectangle<int> r,
                                          const LayoutElement& e, float level) const
    {
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
        const auto bounds = r.reduced (4).toFloat();
        if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
            return;

        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float seconds = (float) now;
        const float rate = juce::jmax (0.05f, e.animationRate);
        const float low  = proc.getAudioLowBand();
        const float mid  = proc.getAudioMidBand();
        const float high = proc.getAudioHighBand();

        // Identify the effect. Authoring writes visualPreset (pulseGlow / orbitAura /
        // spectrumSweep) and visualAction; we map both onto a curated effect set.
        juce::String fx = e.visualPreset.trim().toLowerCase();
        const juce::String act = e.visualAction.trim().toLowerCase();
        if (act.contains ("spectrum") || act.contains ("bars")) fx = "spectrumbars";
        else if (act.contains ("scope") || act.contains ("wave")) fx = "waveform";
        else if (act.contains ("orbit")) fx = "orbit";
        else if (act.contains ("sweep")) fx = "sweep";
        else if (act.contains ("burst") || act.contains ("particle")) fx = "particles";
        else if (act.contains ("glow") || act.contains ("pulse")) fx = "glow";

        if (fx == "spectrumsweep") fx = "spectrumbars";
        if (fx == "orbitaura")     fx = "orbit";

        if (fx == "spectrumbars")
        {
            const int bars = juce::jlimit (8, 48, juce::roundToInt (bounds.getWidth() / 10.0f));
            const float bw = bounds.getWidth() / (float) bars;
            for (int i = 0; i < bars; ++i)
            {
                const float t = (float) i / (float) (bars - 1);
                // Blend low->mid->high across the width, animate with a travelling wave.
                const float band = t < 0.5f ? juce::jmap (t, 0.0f, 0.5f, low, mid)
                                            : juce::jmap (t, 0.5f, 1.0f, mid, high);
                const float wobble = 0.15f * (0.5f + 0.5f * std::sin (seconds * rate * 6.0f + (float) i * 0.6f));
                const float h = juce::jlimit (0.02f, 1.0f, band + wobble * level + level * 0.25f) * bounds.getHeight();
                auto bar = juce::Rectangle<float> (bounds.getX() + (float) i * bw + 1.0f,
                                                   bounds.getBottom() - h, bw - 2.0f, h);
                const auto c = accent.withRotatedHue (t * 0.12f - 0.06f).withAlpha (0.55f + 0.4f * band);
                g.setColour (c);
                g.fillRoundedRectangle (bar, 1.5f);
            }
            return;
        }

        if (fx == "waveform")
        {
            juce::Path path;
            const int n = kLevelHistorySize;
            for (int i = 0; i < n; ++i)
            {
                const int idx = (levelHistoryHead + i) % n;
                const float v = levelHistory[(size_t) idx];
                const float x = bounds.getX() + bounds.getWidth() * ((float) i / (float) (n - 1));
                const float y = bounds.getCentreY() - (v - 0.5f * proc.getAudioRms()) * bounds.getHeight() * 0.9f;
                if (i == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }
            g.setColour (accent.withAlpha (0.35f));
            g.strokePath (path, juce::PathStrokeType (3.4f));
            g.setColour (accent.withAlpha (0.95f));
            g.strokePath (path, juce::PathStrokeType (1.6f));
            return;
        }

        if (fx == "orbit")
        {
            const auto centre = bounds.getCentre();
            const float baseR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.16f;
            const int dots = 26;
            for (int i = 0; i < dots; ++i)
            {
                const float ph = seconds * rate * 1.6f + (float) i * juce::MathConstants<float>::twoPi / (float) dots;
                const float rad = baseR * (1.0f + level * 1.4f) + (float) (i % 4) * 4.0f;
                const float sz = 2.0f + level * 6.0f;
                g.setColour (accent.withRotatedHue ((float) i / (float) dots * 0.1f).withAlpha (0.18f + level * 0.5f));
                g.fillEllipse (centre.x + std::cos (ph) * rad - sz * 0.5f,
                               centre.y + std::sin (ph) * rad * 0.78f - sz * 0.5f, sz, sz);
            }
            return;
        }

        if (fx == "sweep")
        {
            const float phase = std::fmod (seconds * rate * 0.5f, 1.0f);
            const float x = bounds.getX() + bounds.getWidth() * phase;
            const float w = juce::jmax (12.0f, bounds.getWidth() * (0.10f + level * 0.18f));
            juce::ColourGradient grad (accent.withAlpha (0.0f), x - w, bounds.getCentreY(),
                                       accent.withAlpha (0.0f), x + w, bounds.getCentreY(), false);
            grad.addColour (0.5, accent.withAlpha (0.30f + level * 0.5f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x - w, bounds.getY(), w * 2.0f, bounds.getHeight()));
            return;
        }

        if (fx == "particles")
        {
            const auto centre = bounds.getCentre();
            const float burst = juce::jmax (level, proc.getAudioTransient());
            const float maxR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const int count = 36;
            for (int i = 0; i < count; ++i)
            {
                const float ph = (float) i * 0.61f;
                const float t = std::fmod (seconds * rate * 0.7f + (float) i * 0.13f, 1.0f);
                const float rad = t * maxR * (0.5f + burst);
                const float sz = (1.0f - t) * (2.0f + burst * 7.0f);
                if (sz <= 0.2f) continue;
                g.setColour (accent.withRotatedHue ((float) i / (float) count * 0.15f)
                                   .withAlpha ((1.0f - t) * (0.25f + burst * 0.6f)));
                g.fillEllipse (centre.x + std::cos (ph) * rad - sz * 0.5f,
                               centre.y + std::sin (ph) * rad - sz * 0.5f, sz, sz);
            }
            return;
        }

        // Default: radial pulse glow.
        {
            const auto centre = bounds.getCentre();
            const float maxR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const float pulse = 0.5f + 0.5f * std::sin (seconds * rate * 2.0f);
            for (int ring = 4; ring >= 0; --ring)
            {
                const float rr = maxR * (0.35f + 0.16f * (float) ring) * (0.8f + level * 0.6f + pulse * 0.1f);
                g.setColour (accent.withAlpha ((0.06f + level * 0.30f) * (1.0f - (float) ring * 0.16f)));
                g.fillEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f);
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

    void PlayerGuiRenderer::drawAdsrCurve (juce::Graphics& g, juce::Rectangle<int> r,
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
        g.drawText (element.label.isNotEmpty() ? element.label.toUpperCase() : "ENVELOPE",
                    header.removeFromLeft (130), juce::Justification::centredLeft, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("amp adsr", header, juce::Justification::centredRight, true);

        auto graphArea = area.reduced (2, 4);
        g.setColour (playerBg().withAlpha (0.55f));
        g.fillRoundedRectangle (graphArea.toFloat(), 5.0f);

        // draw light grid lines
        g.setColour (border.withAlpha (0.15f));
        for (int i = 1; i < 4; ++i)
        {
            const int x = graphArea.getX() + (graphArea.getWidth() * i) / 4;
            const int y = graphArea.getY() + (graphArea.getHeight() * i) / 4;
            g.drawVerticalLine (x, (float) graphArea.getY(), (float) graphArea.getBottom());
            g.drawHorizontalLine (y, (float) graphArea.getX(), (float) graphArea.getRight());
        }

        juce::String prefix = "";
        if (element.parameterId.isNotEmpty() && element.parameterId.containsIgnoreCase ("attack"))
            prefix = element.parameterId.upToFirstOccurrenceOf ("attack", false, false);
        else if (element.parameterId.isNotEmpty() && element.parameterId.containsIgnoreCase ("adsr"))
            prefix = element.parameterId.upToFirstOccurrenceOf ("adsr", false, false);

        const auto getVal = [&] (const juce::String& baseName, float fallback) -> float
        {
            const auto fullId = prefix + baseName;
            const auto* def = parameterForId (fullId);
            if (def != nullptr)
            {
                LayoutElement e;
                e.parameterId = fullId;
                return parameterValueForElement (e, def->defaultValue);
            }
            
            const auto* fallbackDef = parameterForId (baseName);
            if (fallbackDef != nullptr)
            {
                LayoutElement e;
                e.parameterId = baseName;
                return parameterValueForElement (e, fallbackDef->defaultValue);
            }
            
            return fallback;
        };

        const float a = juce::jlimit (0.0f, 1.0f, getVal ("attack", 0.1f) / 4.0f);
        const float d = juce::jlimit (0.0f, 1.0f, getVal ("decay", 0.2f) / 4.0f);
        const float s = juce::jlimit (0.0f, 1.0f, getVal ("sustain", 0.8f));
        const float r_val = juce::jlimit (0.0f, 1.0f, getVal ("release", 0.4f) / 4.0f);

        const float totalW = (float) graphArea.getWidth();
        const float startX = (float) graphArea.getX();
        const float startY = (float) graphArea.getBottom() - 4.0f;
        const float topY = (float) graphArea.getY() + 4.0f;
        const float height = startY - topY;

        const float attW = 4.0f + a * (totalW * 0.22f);
        const float decW = 4.0f + d * (totalW * 0.22f);
        const float susW = totalW * 0.25f;
        const float relW = 4.0f + r_val * (totalW * 0.22f);
        const float susY = startY - s * height;

        juce::Path p;
        p.startNewSubPath (startX, startY);
        p.lineTo (startX + attW, topY);
        p.lineTo (startX + attW + decW, susY);
        p.lineTo (startX + attW + decW + susW, susY);
        p.lineTo (startX + attW + decW + susW + relW, startY);

        juce::Path fillPath = p;
        fillPath.lineTo (startX + attW + decW + susW + relW, startY);
        fillPath.lineTo (startX, startY);
        fillPath.closeSubPath();

        juce::ColourGradient grad (accent.withAlpha (0.24f), startX, topY,
                                   accent.withAlpha (0.01f), startX, startY, false);
        g.setGradientFill (grad);
        g.fillPath (fillPath);

        g.setColour (accent.withAlpha (0.92f));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        g.setColour (accent);
        g.fillEllipse (startX + attW - 3.0f, topY - 3.0f, 6.0f, 6.0f);
        g.fillEllipse (startX + attW + decW - 3.0f, susY - 3.0f, 6.0f, 6.0f);
        g.fillEllipse (startX + attW + decW + susW - 3.0f, susY - 3.0f, 6.0f, 6.0f);
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
        const bool transportButton = e.action.equalsIgnoreCase ("transport.toggle");
        const bool active = transportButton ? proc.isAnyTransportPlaying()
            : activeMomentaryParameter.isNotEmpty() && e.parameterId == activeMomentaryParameter;
        g.setColour ((e.backgroundColour.isTransparent() ? playerPanel().brighter (0.06f) : e.backgroundColour)
                     .withAlpha (active ? 0.92f : 0.78f));
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour ((e.accentColour.isTransparent() ? playerAccent() : e.accentColour).withAlpha (active ? 1.0f : 0.72f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), active ? 2.0f : 1.0f);
        g.setColour (e.textColour.isTransparent() ? playerText() : e.textColour);
        g.setFont (juce::FontOptions (juce::jmax (11.0f, r.getHeight() * 0.32f)).withStyle ("bold"));
        g.drawText (transportButton && active ? "Stop" : (e.label.isNotEmpty() ? e.label : e.parameterId),
                    r.reduced (8, 0), juce::Justification::centred, true);
    }

    void PlayerGuiRenderer::drawValueDisplay (juce::Graphics& g, juce::Rectangle<int> r,
                                              const LayoutElement& e) const
    {
        g.setColour (playerPanel().brighter (0.04f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);

        auto valueArea = r;
        if (e.label.isNotEmpty() || e.parameterId.isNotEmpty())
        {
            const auto labelStr = e.label.isNotEmpty() ? e.label : e.parameterId;
            const int labelH = juce::jlimit (12, 16, r.getHeight() / 2 - 2);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (juce::jmax (9.0f, (float) labelH - 2.0f)).withStyle ("bold"));
            g.drawFittedText (labelStr.toUpperCase(), r.removeFromTop (labelH).reduced (6, 0), juce::Justification::centredLeft, 1, 0.7f);
            valueArea = r;
        }

        g.setColour (playerAccent());
        const float valFontSize = juce::jmax (11.0f, (float) valueArea.getHeight() * 0.75f - 2.0f);
        g.setFont (juce::FontOptions (valFontSize).withStyle ("bold"));
        g.drawFittedText (formattedParameterValue (e), valueArea.reduced (6, 0),
                          e.label.isNotEmpty() || e.parameterId.isNotEmpty() ? juce::Justification::centredRight
                                                                              : juce::Justification::centred,
                          1, 0.7f);
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
        const auto* pack = proc.getPack();
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : kPadGridCellGapPx;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty()) return;

        const bool squarePads = e.type == ElementType::PadGrid;
        const auto metrics = computePadGridMetrics (inner.toFloat(), rows, cols, gap, squarePads);
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.08f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                juce::Rectangle<float> pad = padCellRect (metrics, row, col, gap, squarePads,
                                                          (float) inner.getX(), (float) inner.getY());
                const int padIdx = row * cols + col;
                const auto* zone = sampleZoneForPad (pack, padIdx);
                const int note = zone != nullptr
                    ? juce::jlimit (0, 127, zone->rootNote)
                    : juce::jlimit (0, 127, e.padBaseNote + padIdx);
                const float triggerLevel = juce::jlimit (0.0f, 1.0f, proc.getNoteHighlightLevel (note));
                const bool active = (note == activePadNote) || triggerLevel > 0.02f;
                const float activeAlpha = juce::jlimit (0.40f, 0.92f, 0.48f + triggerLevel * 0.44f);

                g.setColour (active ? accent.withAlpha (activeAlpha) : bg.brighter (0.04f));
                g.fillRoundedRectangle (pad, juce::jmax (3.0f, e.cornerRadius * 0.6f));
                g.setColour (active ? accent : borderC.withAlpha (0.6f));
                g.drawRoundedRectangle (pad.reduced (0.5f), juce::jmax (3.0f, e.cornerRadius * 0.6f), active ? 1.8f : 1.0f);

                g.setColour (active ? juce::Colour (0xff0a0c10) : playerText().withAlpha (0.85f));
                g.setFont (juce::FontOptions (juce::jmin (12.0f, pad.getHeight() * 0.28f)).withStyle ("bold"));
                const juce::String sampleLabel = zone != nullptr
                    ? (zone->padLabel.isNotEmpty()
                        ? zone->padLabel
                        : juce::File (zone->samplePath).getFileNameWithoutExtension())
                    : juce::String();
                const juce::String label = sampleLabel.isNotEmpty()
                    ? sampleLabel
                    : (e.type == ElementType::DrumPad && e.label.isNotEmpty()
                        ? e.label
                        : juce::String (padIdx + 1));
                g.drawText (label, pad.reduced (4.0f).removeFromTop (pad.getHeight() * 0.55f).toNearestInt(),
                            juce::Justification::centred);
                g.setColour (active ? juce::Colour (0xaa0a0c10) : playerTextDim());
                g.setFont (juce::FontOptions (juce::jmin (10.0f, pad.getHeight() * 0.22f)));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                            pad.reduced (4.0f).removeFromBottom (pad.getHeight() * 0.35f).toNearestInt(),
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
        const bool songMode = block != nullptr && blockValue (*block, "dmSongMode", 0.0f) >= 0.5f;

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
        auto songButton = header.removeFromRight (54).reduced (2);
        auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText ((e.label.isNotEmpty() ? e.label : "DRUM GRID")
                        + "  P" + juce::String (pattern + 1)
                        + (songMode ? "  SONG" : ""),
                    header, juce::Justification::centredLeft, true);

        const bool playing = proc.isAnyTransportPlaying();
        g.setColour (playing ? accent : playerPanel().brighter (0.08f));
        g.fillRoundedRectangle (playButton.toFloat(), 5.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerBorder());
        g.drawRoundedRectangle (playButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerText().withAlpha (0.92f));
        g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
        g.drawText (playing ? "STOP" : "PLAY", playButton, juce::Justification::centred, true);

        g.setColour (songMode ? accent.withAlpha (0.92f) : playerPanel().brighter (0.08f));
        g.fillRoundedRectangle (songButton.toFloat(), 5.0f);
        g.setColour (songMode ? juce::Colour (0xff071014) : playerBorder());
        g.drawRoundedRectangle (songButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (songMode ? juce::Colour (0xff071014) : playerText().withAlpha (0.92f));
        g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
        g.drawText ("SONG", songButton, juce::Justification::centred, true);

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

        const int labelW = juce::jlimit (96, 140, area.getWidth() / 4);
        auto grid = area.withTrimmedLeft (labelW);
        const float cellW = (float) grid.getWidth() / (float) steps;
        const float cellH = (float) area.getHeight() / (float) tracks;
        const double playback01 = proc.getSequencerPlaybackPosition01 (steps);
        const int playbackStep = playback01 >= 0.0
            ? juce::jlimit (0, steps - 1, (int) std::floor (playback01 * (double) steps))
            : -1;

        for (int track = 0; track < tracks; ++track)
        {
            const int y = area.getY() + juce::roundToInt ((float) track * cellH);
            const int h = juce::roundToInt (cellH);
            auto lane = juce::Rectangle<int> (area.getX(), y, labelW - 4, h).reduced (0, 1);
            auto muteBtn = lane.removeFromRight (juce::jmin (18, lane.getWidth() / 3)).reduced (1);
            auto soloBtn = lane.removeFromRight (juce::jmin (18, lane.getWidth() / 3)).reduced (1);
            const bool muted = drumTrackMuted (block, track);
            const bool soloed = drumTrackSoloed (block, track);

            g.setColour (muted ? playerBg().darker (0.12f) : playerPanel().brighter (0.06f));
            g.fillRoundedRectangle (lane.toFloat(), 3.0f);
            g.setColour (playerText());
            g.setFont (juce::FontOptions (juce::jlimit (7.5f, 9.5f, cellH * 0.38f)).withStyle ("bold"));
            g.drawText (drumTrackLabel (block, track), lane.reduced (4, 0),
                        juce::Justification::centredLeft, true);

            g.setColour (muted ? accent.withAlpha (0.85f) : playerPanel().brighter (0.04f));
            g.fillRoundedRectangle (muteBtn.toFloat(), 3.0f);
            g.setColour (muted ? juce::Colour (0xff071014) : playerTextDim());
            g.setFont (juce::FontOptions (8.0f).withStyle ("bold"));
            g.drawText ("M", muteBtn, juce::Justification::centred, true);

            g.setColour (soloed ? accent.withAlpha (0.85f) : playerPanel().brighter (0.04f));
            g.fillRoundedRectangle (soloBtn.toFloat(), 3.0f);
            g.setColour (soloed ? juce::Colour (0xff071014) : playerTextDim());
            g.drawText ("S", soloBtn, juce::Justification::centred, true);

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
        const auto bypassButton = arpLaneBypassButtonBounds (r);
        const auto soloButton = arpLaneSoloButtonBounds (r);
        const bool laneMuted = block != nullptr && arpLaneValue (*block, lane, "mpLaneMute", 0.0f) >= 0.5f;
        const bool laneSolo = block != nullptr && arpLaneValue (*block, lane, "mpLaneSolo", 0.0f) >= 0.5f;
        header.removeFromRight (178);
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
        g.setColour (laneMuted ? juce::Colour (0xffe6504a).withAlpha (0.94f) : playerPanel().brighter (0.10f).withAlpha (0.94f));
        g.fillRoundedRectangle (bypassButton.toFloat(), 6.0f);
        g.setColour ((laneMuted ? juce::Colour (0xffe6504a) : accent).withAlpha (0.82f));
        g.drawRoundedRectangle (bypassButton.toFloat().reduced (0.5f), 6.0f, 1.0f);
        auto power = bypassButton.toFloat().reduced (7.0f, 5.0f);
        g.setColour (laneMuted ? playerBg() : playerText());
        juce::Path powerArc;
        powerArc.addArc (power.getX(), power.getY() + 2.0f, power.getWidth(), power.getHeight(),
                         juce::MathConstants<float>::pi * 0.18f,
                         juce::MathConstants<float>::pi * 1.82f,
                         true);
        g.strokePath (powerArc, juce::PathStrokeType (1.4f));
        g.drawLine (power.getCentreX(), power.getY(), power.getCentreX(), power.getCentreY(), 1.4f);
        g.setColour (laneSolo ? juce::Colour (0xff6adf7f).withAlpha (0.92f) : playerPanel().brighter (0.10f).withAlpha (0.94f));
        g.fillRoundedRectangle (soloButton.toFloat(), 6.0f);
        g.setColour ((laneSolo ? juce::Colour (0xff6adf7f) : accent).withAlpha (0.82f));
        g.drawRoundedRectangle (soloButton.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (laneSolo ? playerBg() : playerText());
        g.setFont (juce::FontOptions (7.8f).withStyle ("bold"));
        g.drawText ("S", soloButton, juce::Justification::centred, true);

        const auto layout = ArpLaneUi::layout (r, e, block, true);
        auto drawStepBtn = [&] (juce::Rectangle<int> btn, const juce::String& label)
        {
            g.setColour (playerPanel().brighter (0.12f));
            g.fillRoundedRectangle (btn.toFloat(), 4.0f);
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (btn.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (playerText());
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (label, btn, juce::Justification::centred, true);
        };
        drawStepBtn (layout.stepsMinusBtn, "-");
        drawStepBtn (layout.stepsPlusBtn, "+");
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
        g.drawText (juce::String (steps) + " st", layout.stepsLabel, juce::Justification::centredLeft, true);

        if (ArpLaneUi::isLinearMode (e))
        {
            const int maxDrawSteps = juce::jmin (steps, 64);
            const double playback01 = proc.getSequencerPlaybackPosition01 (maxDrawSteps);
            const int playbackStep = playback01 >= 0.0
                ? juce::jlimit (0, maxDrawSteps - 1, (int) std::floor (playback01 * (double) maxDrawSteps))
                : -1;

            for (int step = 0; step < maxDrawSteps; ++step)
            {
                const bool active = block != nullptr
                    && arpLaneValue (*block, lane, "mpStep" + juce::String (step) + "On", 0.0f) >= 0.5f;
                const float velocity = block != nullptr
                    ? juce::jlimit (0.05f, 1.0f, arpLaneValue (*block, lane, "mpVelocity" + juce::String (step), 0.72f))
                    : 0.72f;
                auto cell = ArpLaneUi::linearStepRect (layout, step);
                g.setColour (active ? accent.withAlpha (0.18f) : playerPanel().darker (0.08f));
                g.fillRoundedRectangle (cell, 3.0f);
                if (active)
                {
                    auto fill = cell.withTrimmedTop (cell.getHeight() * (1.0f - velocity));
                    g.setColour (accent.withAlpha (step == playbackStep ? 0.95f : 0.72f));
                    g.fillRoundedRectangle (fill, 3.0f);
                }
                if (step == playbackStep)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.85f));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.4f);
                }
            }
            return;
        }

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
            const int activeSteps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (arpLaneValue (*block, activeLane, "arpSteps", (float) e.arpLaneSteps)))
                : juce::jlimit (1, 128, e.arpLaneSteps);
            const double sequencerPlayback01 = proc.getSequencerPlaybackPosition01 (activeSteps);
            const double playback01 = sequencerPlayback01 >= 0.0
                ? sequencerPlayback01
                : std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.00010, 1.0);
            const bool realPlayback = sequencerPlayback01 >= 0.0 || proc.isAnyTransportPlaying();

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

                    const float dot = 3.0f + velocity * 3.2f;
                    if (active >= 0.5f)
                    {
                        g.setColour (ringColour.withAlpha ((activeRing ? laneAlpha : laneAlpha * 0.48f) * (0.5f + velocity * 0.5f)));
                        g.drawLine (centre.x, centre.y, p.x, p.y, activeRing ? 1.2f : 0.75f);
                        g.fillEllipse (p.x - dot, p.y - dot, dot * 2.0f, dot * 2.0f);
                    }
                    else
                    {
                        g.setColour (borderC.withAlpha (activeRing ? 0.22f : 0.12f));
                        g.drawEllipse (p.x - 2.4f, p.y - 2.4f, 4.8f, 4.8f, 0.8f);
                    }
                }
            }

            {
                const float playheadAngle = -juce::MathConstants<float>::halfPi
                    + juce::MathConstants<float>::twoPi * (float) std::fmod (playback01, 1.0);
                const auto dot = centre + juce::Point<float> (std::cos (playheadAngle) * (outerRadius + band * 0.46f),
                                                              std::sin (playheadAngle) * (outerRadius + band * 0.46f));
                const auto tick0 = centre + juce::Point<float> (std::cos (playheadAngle) * (innerRadius - band * 0.10f),
                                                                std::sin (playheadAngle) * (innerRadius - band * 0.10f));
                const auto tick1 = centre + juce::Point<float> (std::cos (playheadAngle) * (outerRadius + band * 0.18f),
                                                                std::sin (playheadAngle) * (outerRadius + band * 0.18f));
                g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.70f : 0.38f));
                g.drawLine (tick0.x, tick0.y, tick1.x, tick1.y, realPlayback ? 1.4f : 1.0f);
                g.setColour (accent.withAlpha (realPlayback ? 0.92f : 0.58f));
                g.fillEllipse (dot.x - 5.2f, dot.y - 5.2f, 10.4f, 10.4f);
                g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.82f : 0.50f));
                g.drawEllipse (dot.x - 5.2f, dot.y - 5.2f, 10.4f, 10.4f, 1.1f);
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

        const double sequencerPlayback01 = proc.getSequencerPlaybackPosition01 (steps);
        const double playback01 = sequencerPlayback01 >= 0.0
            ? sequencerPlayback01
            : std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.00010 + (double) lane * 0.071, 1.0);
        const bool realPlayback = sequencerPlayback01 >= 0.0 || proc.isAnyTransportPlaying();
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

        const float playheadAngle = -juce::MathConstants<float>::halfPi
            + juce::MathConstants<float>::twoPi * (float) std::fmod (playback01, 1.0);
        const auto playheadInner = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 0.18f,
                                                                std::sin (playheadAngle) * radius * 0.18f);
        const auto playheadOuter = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 1.04f,
                                                                std::sin (playheadAngle) * radius * 1.04f);
        const auto playheadDot = centre + juce::Point<float> (std::cos (playheadAngle) * radius * 1.10f,
                                                              std::sin (playheadAngle) * radius * 1.10f);
        g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.72f : 0.38f));
        g.drawLine (playheadInner.x, playheadInner.y, playheadOuter.x, playheadOuter.y, realPlayback ? 2.0f : 1.1f);
        g.setColour (accent.withAlpha (realPlayback ? 0.90f : 0.58f));
        g.fillEllipse (playheadDot.x - 5.0f, playheadDot.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (realPlayback ? 0.85f : 0.50f));
        g.drawEllipse (playheadDot.x - 5.0f, playheadDot.y - 5.0f, 10.0f, 10.0f, 1.2f);

        const auto targetLabel = e.arpLaneTarget == "drums" ? "DRUMS"
                              : e.arpLaneTarget == "oneShots" ? "ONE SHOTS"
                              : e.arpLaneTarget == "loops" ? "LOOP SLICES"
                              : e.arpLaneTarget == "effects" ? "FX SENDS"
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

    PlayerGuiRenderer::PianoRollGeometry PlayerGuiRenderer::pianoRollGeometry (const LayoutElement& e,
                                                                              juce::Rectangle<int> r) const
    {
        PianoRollGeometry geo;
        geo.steps = juce::jlimit (1, 256, e.pianoRollSteps);
        geo.stepsPerBeat = juce::jlimit (1, 16, e.pianoRollStepsPerBeat);
        geo.rows = juce::jlimit (4, 88, e.pianoRollRows);
        geo.lowNote = juce::jlimit (0, 120, e.pianoRollLowNote);

        auto area = r.reduced (8);
        geo.header = area.removeFromTop (26);
        geo.expandButton = geo.header.removeFromRight (54).reduced (2);
        geo.playButton = geo.header.removeFromRight (58).reduced (2);
        area.removeFromTop (4);
        if (area.getWidth() < 30 || area.getHeight() < 20)
            return geo;

        const int gutterW = juce::jlimit (26, 52, area.getWidth() / 12);
        geo.gutter = area.removeFromLeft (gutterW);
        geo.grid = area;
        geo.cellW = (float) geo.grid.getWidth() / (float) geo.steps;
        geo.cellH = (float) geo.grid.getHeight() / (float) geo.rows;
        geo.valid = geo.cellW > 0.0f && geo.cellH > 0.0f;
        return geo;
    }

    bool PlayerGuiRenderer::pianoRollCellAt (const LayoutElement& e,
                                             juce::Rectangle<int> r,
                                             juce::Point<int> pos,
                                             int& pitch,
                                             int& step) const
    {
        const auto geo = pianoRollGeometry (e, r);
        if (! geo.valid || ! geo.grid.contains (pos))
            return false;

        step = juce::jlimit (0, geo.steps - 1, (int) (((float) pos.x - (float) geo.grid.getX()) / geo.cellW));
        const int rowFromTop = juce::jlimit (0, geo.rows - 1,
            (int) (((float) pos.y - (float) geo.grid.getY()) / geo.cellH));
        // Top row is the highest pitch.
        pitch = juce::jlimit (0, 127, geo.lowNote + (geo.rows - 1 - rowFromTop));
        return true;
    }

    void PlayerGuiRenderer::drawPianoRoll (juce::Graphics& g,
                                           juce::Rectangle<int> r,
                                           const LayoutElement& e) const
    {
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.10f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;

        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour (borderC);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);

        const auto geo = pianoRollGeometry (e, r);

        // Header.
        const auto notes = PianoRollRuntime::decodeNotes (proc.getPianoRollNotesEncoded());
        g.setColour (accent);
        g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : juce::String ("PIANO ROLL"),
                    geo.header.withTrimmedRight (160), juce::Justification::centredLeft, true);
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (juce::String (notes.size()) + (notes.size() == 1 ? " note  -  click notes to remove, drag to resize"
                                                                     : " notes  -  click add/remove, drag notes to resize"),
                    geo.header.withTrimmedRight (62), juce::Justification::centredRight, true);

        // Play button.
        const bool playing = proc.isAnyTransportPlaying();
        g.setColour (playing ? accent : playerPanel().brighter (0.08f));
        g.fillRoundedRectangle (geo.playButton.toFloat(), 5.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerBorder());
        g.drawRoundedRectangle (geo.playButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (playing ? juce::Colour (0xff071014) : playerText().withAlpha (0.92f));
        g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
        g.drawText (playing ? "STOP" : "PLAY", geo.playButton, juce::Justification::centred, true);

        g.setColour (playerPanel().brighter (0.10f));
        g.fillRoundedRectangle (geo.expandButton.toFloat(), 5.0f);
        g.setColour (playerBorder());
        g.drawRoundedRectangle (geo.expandButton.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (playerText().withAlpha (0.92f));
        g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
        g.drawText ("EDIT", geo.expandButton, juce::Justification::centred, true);

        if (! geo.valid)
            return;

        // Keyboard gutter + horizontal pitch rows.
        for (int row = 0; row < geo.rows; ++row)
        {
            const int pitch = geo.lowNote + (geo.rows - 1 - row);
            const int pc = pitch % 12;
            const bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
            const auto rowRect = juce::Rectangle<float> ((float) geo.grid.getX(),
                                                         (float) geo.grid.getY() + (float) row * geo.cellH,
                                                         (float) geo.grid.getWidth(),
                                                         geo.cellH);
            g.setColour ((isBlack ? playerBg().darker (0.18f) : playerPanel()).withAlpha (0.55f));
            g.fillRect (rowRect);

            const auto keyRect = juce::Rectangle<float> ((float) geo.gutter.getX(),
                                                         rowRect.getY(),
                                                         (float) geo.gutter.getWidth(),
                                                         geo.cellH).reduced (1.0f, 0.5f);
            g.setColour (isBlack ? juce::Colour (0xff141821) : juce::Colour (0xffe9ecf2));
            g.fillRoundedRectangle (keyRect, 2.0f);
            if (pc == 0 && geo.cellH >= 9.0f)
            {
                g.setColour (juce::Colour (0xff2a2f3a));
                g.setFont (juce::FontOptions (juce::jlimit (7.0f, 9.5f, geo.cellH * 0.62f)).withStyle ("bold"));
                g.drawText ("C" + juce::String (pitch / 12 - 1), keyRect.reduced (3.0f, 0.0f),
                            juce::Justification::centredLeft, false);
            }
        }

        // Vertical grid lines (beat + bar emphasis).
        for (int step = 0; step <= geo.steps; ++step)
        {
            const float x = (float) geo.grid.getX() + (float) step * geo.cellW;
            const bool beat = step % geo.stepsPerBeat == 0;
            g.setColour (playerBorder().withAlpha (beat ? 0.65f : 0.28f));
            g.drawVerticalLine (juce::roundToInt (x),
                                (float) geo.grid.getY(), (float) geo.grid.getBottom());
        }
        g.setColour (playerBorder().withAlpha (0.45f));
        g.drawRect (geo.grid.toFloat(), 1.0f);

        // Notes.
        const int highNote = geo.lowNote + geo.rows - 1;
        for (const auto& note : notes)
        {
            if (note.pitch < geo.lowNote || note.pitch > highNote)
                continue;
            const int rowFromTop = highNote - note.pitch;
            const int startStep = juce::jlimit (0, geo.steps - 1, note.startStep);
            const int endStep = juce::jlimit (1, geo.steps, note.startStep + juce::jmax (1, note.lengthSteps));
            const auto noteRect = juce::Rectangle<float> (
                (float) geo.grid.getX() + (float) startStep * geo.cellW + 1.0f,
                (float) geo.grid.getY() + (float) rowFromTop * geo.cellH + 1.0f,
                juce::jmax (2.0f, (float) (endStep - startStep) * geo.cellW - 2.0f),
                juce::jmax (2.0f, geo.cellH - 2.0f));
            const float vel = juce::jlimit (0.1f, 1.0f, note.velocity);
            g.setColour (accent.withAlpha (0.45f + vel * 0.45f));
            g.fillRoundedRectangle (noteRect, 2.5f);
            g.setColour (accent.brighter (0.4f).withAlpha (0.9f));
            g.drawRoundedRectangle (noteRect.reduced (0.5f), 2.5f, 1.0f);
        }

        // Playhead.
        const double playback01 = proc.getPianoRollPlaybackPosition01();
        if (playback01 >= 0.0)
        {
            const float x = (float) geo.grid.getX() + (float) playback01 * (float) geo.grid.getWidth();
            g.setColour (accent.withAlpha (0.95f));
            g.drawLine (x, (float) geo.grid.getY(), x, (float) geo.grid.getBottom(), 2.0f);
        }
    }

    bool PlayerGuiRenderer::handlePianoRollSurface (const LayoutElement& e,
                                                  juce::Rectangle<int> bounds,
                                                  const juce::MouseEvent& event,
                                                  bool drag)
    {
        const auto pos = event.getPosition();
        if (! bounds.contains (pos) && ! (drag && pianoRollDragActive && pianoRollDragElementId == e.id))
            return false;

        const auto geo = pianoRollGeometry (e, bounds);

        if (! drag && geo.expandButton.contains (pos))
        {
            showPianoRollEditorModal (e);
            return true;
        }

        if (! drag && geo.playButton.contains (pos))
        {
            proc.toggleInternalTransport();
            return true;
        }

        auto notes = PianoRollRuntime::decodeNotes (proc.getPianoRollNotesEncoded());

        if (! drag)
        {
            int pitch = -1;
            int step = -1;
            if (! pianoRollCellAt (e, bounds, pos, pitch, step))
                return true;

            int hitIndex = -1;
            for (int i = 0; i < (int) notes.size(); ++i)
            {
                const auto& n = notes[(size_t) i];
                if (n.pitch == pitch
                    && step >= n.startStep
                    && step < n.startStep + juce::jmax (1, n.lengthSteps))
                {
                    hitIndex = i;
                    break;
                }
            }

            if (hitIndex >= 0)
            {
                pianoRollDragActive = true;
                pianoRollDragMode = PianoRollDragMode::ResizeNote;
                pianoRollDragElementId = e.id;
                pianoRollDragPitch = notes[(size_t) hitIndex].pitch;
                pianoRollDragStartStep = notes[(size_t) hitIndex].startStep;
                pianoRollDragPendingDelete = true;
                pianoRollDragDidEdit = false;
                return true;
            }

            PianoRollRuntime::Note note;
            note.startStep = step;
            note.lengthSteps = 1;
            note.pitch = pitch;
            note.velocity = 0.85f;
            notes.push_back (note);
            proc.setPianoRollNotesFromUi (PianoRollRuntime::encodeNotes (notes));

            pianoRollDragActive = true;
            pianoRollDragMode = PianoRollDragMode::NewNote;
            pianoRollDragElementId = e.id;
            pianoRollDragPitch = pitch;
            pianoRollDragStartStep = step;
            pianoRollDragPendingDelete = false;
            pianoRollDragDidEdit = true;

            proc.handleNoteOn (pitch, note.velocity);
            juce::Timer::callAfterDelay (110,
                [safe = juce::Component::SafePointer<PlayerGuiRenderer> (this), pitch]
                {
                    if (auto* self = safe.getComponent())
                        self->proc.handleNoteOff (pitch);
                });
            return true;
        }

        if (! pianoRollDragActive || pianoRollDragElementId != e.id)
            return true;

        int pitch = -1;
        int step = -1;
        if (! pianoRollCellAt (e, bounds, pos, pitch, step))
            return true;

        const int newLength = juce::jlimit (1, geo.steps - pianoRollDragStartStep,
                                            step - pianoRollDragStartStep + 1);
        bool changed = false;
        for (auto& n : notes)
        {
            if (n.pitch == pianoRollDragPitch && n.startStep == pianoRollDragStartStep)
            {
                if (n.lengthSteps != newLength)
                {
                    n.lengthSteps = newLength;
                    changed = true;
                    pianoRollDragDidEdit = true;
                    pianoRollDragPendingDelete = false;
                }
                break;
            }
        }
        if (changed)
            proc.setPianoRollNotesFromUi (PianoRollRuntime::encodeNotes (notes));
        return true;
    }

    void PlayerGuiRenderer::cancelPianoRollDrag()
    {
        if (pianoRollDragActive && pianoRollDragMode == PianoRollDragMode::ResizeNote
            && pianoRollDragPendingDelete && ! pianoRollDragDidEdit)
        {
            auto notes = PianoRollRuntime::decodeNotes (proc.getPianoRollNotesEncoded());
            for (auto it = notes.begin(); it != notes.end(); ++it)
            {
                if (it->pitch == pianoRollDragPitch && it->startStep == pianoRollDragStartStep)
                {
                    notes.erase (it);
                    proc.setPianoRollNotesFromUi (PianoRollRuntime::encodeNotes (notes));
                    break;
                }
            }
        }

        pianoRollDragActive = false;
        pianoRollDragMode = PianoRollDragMode::None;
        pianoRollDragElementId.clear();
        pianoRollDragPitch = -1;
        pianoRollDragStartStep = -1;
        pianoRollDragPendingDelete = false;
        pianoRollDragDidEdit = false;
    }

    bool PlayerGuiRenderer::handlePianoRollGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;

        const auto m = metrics();
        const auto pos = event.getPosition();

        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::PianoRoll || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos) && ! (drag && pianoRollDragActive && pianoRollDragElementId == e.id))
                continue;

            if (handlePianoRollSurface (e, r, event, drag))
            {
                repaint (r);
                return true;
            }
        }

        return false;
    }

    struct PianoRollModalContent final : public juce::Component,
                                         private juce::Timer
    {
        PianoRollModalContent (PlayerGuiRenderer& ownerIn, LayoutElement elementIn)
            : owner (ownerIn), element (std::move (elementIn))
        {
            title.setText ("Piano Roll Editor", juce::dontSendNotification);
            title.setFont (juce::FontOptions (15.0f).withStyle ("bold"));
            title.setColour (juce::Label::textColourId, owner.playerText());
            addAndMakeVisible (title);

            subtitle.setText ("Click empty cells to add notes. Click notes to remove them, or drag existing notes shorter/longer. "
                              "Playback keeps running while this window is open.",
                              juce::dontSendNotification);
            subtitle.setFont (juce::FontOptions (11.0f));
            subtitle.setColour (juce::Label::textColourId, owner.playerTextDim());
            addAndMakeVisible (subtitle);

            closeButton.setButtonText ("Close");
            closeButton.onClick = [this]
            {
                if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                    dialog->exitModalState (0);
            };
            addAndMakeVisible (closeButton);

            setSize (980, 560);
            startTimerHz (30);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (owner.playerBg());
            auto area = getLocalBounds().reduced (14);
            g.setColour (owner.playerBorder());
            g.drawRoundedRectangle (area.toFloat(), 10.0f, 1.0f);
            owner.drawPianoRoll (g, rollBounds(), element);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (18);
            auto header = area.removeFromTop (48);
            title.setBounds (header.removeFromTop (22));
            subtitle.setBounds (header);
            closeButton.setBounds (area.removeFromBottom (34).removeFromRight (88).reduced (2));
        }

        juce::Rectangle<int> rollBounds() const
        {
            auto area = getLocalBounds().reduced (18);
            area.removeFromTop (48);
            area.removeFromBottom (42);
            return area.reduced (4, 0);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (owner.handlePianoRollSurface (element, rollBounds(), e, false))
                repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (owner.handlePianoRollSurface (element, rollBounds(), e, true))
                repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            owner.cancelPianoRollDrag();
            repaint();
        }

        void timerCallback() override
        {
            if (owner.proc.isAnyTransportPlaying())
                repaint (rollBounds());
        }

        PlayerGuiRenderer& owner;
        LayoutElement element;
        juce::Label title;
        juce::Label subtitle;
        juce::TextButton closeButton;
    };

    void PlayerGuiRenderer::showPianoRollEditorModal (const LayoutElement& sourceElement)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Piano Roll Editor";
        options.dialogBackgroundColour = playerBg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (new PianoRollModalContent (*this, sourceElement));
        options.launchAsync();
        repaint();
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

    void PlayerGuiRenderer::drawRuntimeSampleLibrary (juce::Graphics& g,
                                                       juce::Rectangle<int> r,
                                                       const LayoutElement& e) const
    {
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.82f) : e.backgroundColour;
        const auto border = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);

        auto area = r.reduced (10, 8);
        auto header = area.removeFromTop (28);
        g.setColour (accent);
        g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
        g.drawFittedText (e.label.isNotEmpty() ? e.label : "Runtime Samples",
                          header.removeFromLeft (juce::jmax (120, header.getWidth() - 80)),
                          juce::Justification::centredLeft, 1);

        const auto items = proc.getUserContentSnapshot();
        int sampleCount = 0;
        int midiCount = 0;
        for (const auto& item : items)
        {
            if (item.kind == "sample") ++sampleCount;
            else if (item.kind == "midi") ++midiCount;
        }

        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (sampleCount) + " S / " + juce::String (midiCount) + " MIDI",
                    header, juce::Justification::centredRight, true);

        if (items.empty())
        {
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (area.toFloat(), 7.0f);
            g.setColour (playerText());
            g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
            g.drawFittedText ("Drop samples or MIDI here",
                              area.removeFromTop (24), juce::Justification::centred, 1);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (9.5f));
            g.drawFittedText ("Drop onto pads/zones to target playback.",
                              area, juce::Justification::centred, 2);
            return;
        }

        int rowsDrawn = 0;
        for (const auto& item : items)
        {
            if (rowsDrawn >= 5 || area.getHeight() < 22)
                break;

            auto row = area.removeFromTop (24).reduced (0, 2);
            g.setColour (item.kind == "midi" ? accent.withAlpha (0.18f)
                                             : playerBg().withAlpha (0.48f));
            g.fillRoundedRectangle (row.toFloat(), 5.0f);
            g.setColour (item.kind == "midi" ? accent : playerText());
            g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
            g.drawText (item.kind == "midi" ? "M" : "S", row.removeFromLeft (20), juce::Justification::centred, true);
            g.setColour (playerText());
            g.setFont (juce::FontOptions (10.0f));
            g.drawFittedText (item.name, row.removeFromLeft (juce::jmax (80, row.getWidth() - 78)),
                              juce::Justification::centredLeft, 1);
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (9.0f));
            g.drawFittedText (item.summary, row, juce::Justification::centredRight, 1);
            ++rowsDrawn;
        }

        if ((int) items.size() > rowsDrawn)
        {
            g.setColour (playerTextDim());
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("+" + juce::String ((int) items.size() - rowsDrawn) + " more",
                        area.removeFromTop (18), juce::Justification::centredLeft, true);
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
                                      juce::Point<int> pos,
                                      int* padIndexOut) const
    {
        const int rows = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padRows);
        const int cols = e.type == ElementType::DrumPad ? 1 : juce::jlimit (1, 8, e.padCols);
        const int gap  = e.type == ElementType::DrumPad ? 0 : kPadGridCellGapPx;
        const auto inner = e.type == ElementType::PadGrid ? r.reduced (4) : r;
        if (inner.isEmpty() || ! inner.contains (pos)) return -1;

        const bool squarePads = e.type == ElementType::PadGrid;
        const auto metrics = computePadGridMetrics (inner.toFloat(), rows, cols, gap, squarePads);
        if (metrics.padW <= 0.0f || metrics.padH <= 0.0f) return -1;

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const auto pad = padCellRect (metrics, row, col, gap, squarePads,
                                              (float) inner.getX(), (float) inner.getY());
                if (pad.contains (pos.toFloat()))
                {
                    const int padIndex = row * cols + col;
                    if (padIndexOut != nullptr)
                        *padIndexOut = padIndex;
                    if (const auto* zone = sampleZoneForPad (proc.getPack(), padIndex))
                        return juce::jlimit (0, 127, zone->rootNote);
                    return juce::jlimit (0, 127, e.padBaseNote + padIndex);
                }
            }
        }

        return -1;
    }

    PlayerGuiRenderer::RuntimeDropTarget PlayerGuiRenderer::runtimeDropTargetAt (juce::Point<int> pos) const
    {
        const auto m = metrics();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& element = *it;
            if (! element.visible || ! isElementOnCurrentTab (element))
                continue;

            const auto bounds = animatedElementRect (element, elementRect (element, m));
            if (! bounds.contains (pos))
                continue;

            if (element.type == ElementType::RuntimeSampleLibrary)
                return { "pads", -1, -1, false };

            if (element.type == ElementType::SampleDropZone)
            {
                // Drop a sample on a zone and immediately play it melodically across the
                // whole keyboard, pitched from its root. An authored sampleNote:/note: tag
                // pins the root; otherwise the root is inferred from the file name/audio.
                const int authored = runtimeTargetNoteFromElement (element, -1);
                const int rootNote = (authored >= 0 && authored <= 127) ? authored : -1;
                return { "keyboard", rootNote, -1, rootNote >= 0 };
            }

            if (element.type == ElementType::Keyboard)
            {
                const int note = noteForKeyboardPosition (bounds, pos);
                if (note >= 0)
                    return { "keyboard", note, -1, true };
            }

            if (element.type == ElementType::DrumPad || element.type == ElementType::PadGrid)
            {
                int padIndex = -1;
                const int note = padNoteAt (element, bounds, pos, &padIndex);
                if (note >= 0)
                    return { "pads", note, juce::jmax (0, padIndex), true };
            }
        }

        return {};
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

        const int labelW = juce::jlimit (96, 140, area.getWidth() / 4);
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
        note = drumTrackNote (block, track);
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
        const auto* block = findArpBlock (pack->dspGraph);

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

    juce::String PlayerGuiRenderer::drumTrackLabel (const DspBlock* block, int track) const
    {
        if (block != nullptr)
        {
            const auto key = "dmTrack" + juce::String (track) + "Label";
            if (const auto found = block->metadata.find (key); found != block->metadata.end())
                if (found->second.isNotEmpty())
                    return found->second;
        }
        return DrumMachineUtil::defaultTrackLabel (track);
    }

    int PlayerGuiRenderer::drumTrackNote (const DspBlock* block, int track) const
    {
        if (block == nullptr)
            return DrumMachineUtil::defaultTrackNote (track);
        const bool triggerPadSlots = blockValue (*block, "dmTriggerPadSlots", 1.0f) >= 0.5f;
        if (triggerPadSlots)
            return juce::jlimit (0, 127, 36 + track);
        return juce::jlimit (0, 127, juce::roundToInt (blockValue (*block,
            "dmTrack" + juce::String (track) + "Note",
            (float) DrumMachineUtil::defaultTrackNote (track))));
    }

    bool PlayerGuiRenderer::drumTrackMuted (const DspBlock* block, int track) const
    {
        return block != nullptr
            && blockValue (*block, "dmTrack" + juce::String (track) + "Mute", 0.0f) >= 0.5f;
    }

    bool PlayerGuiRenderer::drumTrackSoloed (const DspBlock* block, int track) const
    {
        return block != nullptr
            && blockValue (*block, "dmTrack" + juce::String (track) + "Solo", 0.0f) >= 0.5f;
    }

    void PlayerGuiRenderer::showDrumLaneMenu (int track, const juce::Point<int>& screenPos)
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Assign Lane Sample");
        struct Choice { int note = 60; juce::String label; };
        static const Choice choices[] =
        {
            { 36, "Kick (C1)" }, { 38, "Snare (D1)" }, { 42, "Closed Hat (F#1)" },
            { 46, "Open Hat (A#1)" }, { 39, "Clap (D#1)" }, { 45, "Low Tom (A1)" },
            { 48, "High Tom (C2)" }, { 49, "Crash (C#2)" }, { 51, "Ride (D#2)" },
            { 37, "Rim (C#1)" }, { 44, "Pedal Hat (G#1)" }, { 52, "China (E2)" }
        };

        std::vector<int> notes;
        for (int i = 0; i < (int) (sizeof (choices) / sizeof (choices[0])); ++i)
        {
            notes.push_back (choices[i].note);
            menu.addItem (i + 1, choices[i].label);
        }

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withMinimumWidth (220)
                                .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
            [this, track, notes] (int result)
            {
                if (result <= 0 || result > (int) notes.size())
                    return;
                proc.setDrumTrackNoteFromUi (track, notes[(size_t) result - 1]);
                repaint();
            });
    }

    bool PlayerGuiRenderer::handleDrumGridGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;
        const auto* block = findDrumMachineBlock (pack->dspGraph);

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
            auto songButton = header.removeFromRight (54).reduced (2);
            auto bankStrip = header.removeFromRight (juce::jmin (224, header.getWidth() / 2)).reduced (2);

            if (! drag && playButton.contains (pos))
            {
                proc.toggleInternalTransport();
                repaint (r);
                return true;
            }

            if (! drag && songButton.contains (pos))
            {
                const bool songMode = block != nullptr && blockValue (*block, "dmSongMode", 0.0f) >= 0.5f;
                proc.setDrumSongModeFromUi (! songMode);
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

            area.removeFromTop (4);
            const int tracks = block != nullptr
                ? juce::jlimit (1, 16, juce::roundToInt (blockValue (*block, "dmTracks", (float) e.drumTracks)))
                : juce::jlimit (1, 16, e.drumTracks);
            const int labelW = juce::jlimit (96, 140, area.getWidth() / 4);
            const float cellH = area.isEmpty() ? 0.0f : (float) area.getHeight() / (float) tracks;

            if (! drag && cellH > 0.0f && pos.x < area.getX() + labelW)
            {
                const int track = juce::jlimit (0, tracks - 1, (int) ((pos.y - area.getY()) / cellH));
                auto lane = juce::Rectangle<int> (area.getX(), area.getY() + juce::roundToInt ((float) track * cellH),
                                                  labelW - 4, juce::roundToInt (cellH)).reduced (0, 1);
                auto muteBtn = lane.removeFromRight (juce::jmin (18, lane.getWidth() / 3)).reduced (1);
                auto soloBtn = lane.removeFromRight (juce::jmin (18, lane.getWidth() / 3)).reduced (1);

                if (muteBtn.contains (pos))
                {
                    proc.setDrumTrackMutedFromUi (track, ! drumTrackMuted (block, track));
                    repaint (r);
                    return true;
                }
                if (soloBtn.contains (pos))
                {
                    proc.setDrumTrackSoloFromUi (track, ! drumTrackSoloed (block, track));
                    repaint (r);
                    return true;
                }
                if (lane.contains (pos))
                {
                    if (event.mods.isPopupMenu())
                        showDrumLaneMenu (track, event.getScreenPosition());
                    else
                    {
                        const int note = drumTrackNote (block, track);
                        proc.handleNoteOn (note, 0.92f);
                        juce::Timer::callAfterDelay (120,
                            [safe = juce::Component::SafePointer<PlayerGuiRenderer> (this), note]
                            {
                                if (auto* self = safe.getComponent())
                                    self->proc.handleNoteOff (note);
                            });
                    }
                    repaint (r);
                    return true;
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
        const auto layout = ArpLaneUi::layout (r, element, block, true);
        if (! ArpLaneUi::hitTestStep (layout, element, pos, lane, step))
            return false;

        velocity = ArpLaneUi::storedStepVelocity (block, lane, step);
        return true;
    }

    bool PlayerGuiRenderer::handleArpLaneGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;
        const auto* block = findArpBlock (pack->dspGraph);

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::ArpLane || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos) && ! (drag && arpLaneDragActive))
                continue;

            const auto layout = ArpLaneUi::layout (r, e, block, true);

            if (! drag && layout.playBtn.contains (pos))
            {
                proc.toggleInternalTransport();
                repaint (r);
                return true;
            }

            if (! drag && arpLaneBypassButtonBounds (r).contains (pos))
            {
                const bool currentlyMuted = block != nullptr
                    && arpLaneValue (*block, e.arpLaneIndex, "mpLaneMute", 0.0f) >= 0.5f;
                if (proc.setArpLaneMutedFromUi (e.arpLaneIndex, ! currentlyMuted))
                    repaint (r);
                return true;
            }

            if (! drag && arpLaneSoloButtonBounds (r).contains (pos))
            {
                const bool currentlySoloed = block != nullptr
                    && arpLaneValue (*block, e.arpLaneIndex, "mpLaneSolo", 0.0f) >= 0.5f;
                if (proc.setArpLaneSoloFromUi (e.arpLaneIndex, ! currentlySoloed))
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

            if (! drag && layout.stepsMinusBtn.contains (pos))
            {
                const int lane = juce::jlimit (0, 15, e.arpLaneIndex);
                const int cur = layout.steps;
                if (proc.setArpLaneStepsFromUi (lane, juce::jmax (1, cur - 1)))
                    repaint (r);
                return true;
            }

            if (! drag && layout.stepsPlusBtn.contains (pos))
            {
                const int lane = juce::jlimit (0, 15, e.arpLaneIndex);
                if (proc.setArpLaneStepsFromUi (lane, juce::jmin (128, layout.steps + 1)))
                    repaint (r);
                return true;
            }

            if (! drag)
            {
                const int footerTab = ArpLaneUi::hitTestOrbitFooterTab (layout, pos);
                if (footerTab >= 0)
                {
                    if (proc.setMidiPlaygroundActiveBankFromUi (footerTab))
                        repaint (r);
                    return true;
                }
            }

            int lane = -1;
            int step = -1;
            if (drag && arpLaneDragActive && lastArpLane >= 0 && lastArpStep >= 0)
            {
                lane = lastArpLane;
                step = lastArpStep;
                const float velocity = layout.orbitMultiRing
                    ? ArpLaneUi::velocityFromOrbitRadius ((pos.toFloat() - layout.centre).getDistanceFromOrigin(),
                                                         layout.ringSize, lane)
                    : ArpLaneUi::velocityFromVerticalDrag (layout.content, pos.y);
                proc.setArpLaneStepFromUi (lane, step, velocity, true);
                repaint (r);
                return true;
            }

            if (! ArpLaneUi::hitTestStep (layout, e, pos, lane, step))
                return true;

            if (! drag)
            {
                proc.setMidiPlaygroundActiveBankFromUi (lane);
                const bool wasActive = ArpLaneUi::storedStepActive (block, lane, step, false);
                const float velocity = ArpLaneUi::storedStepVelocity (block, lane, step);
                proc.setArpLaneStepFromUi (lane, step, velocity, ! wasActive);
                arpLaneDragActive = ! wasActive;
                lastArpLane = lane;
                lastArpStep = step;
                repaint (r);
                return true;
            }

            return true;
        }

        return false;
    }

    void PlayerGuiRenderer::drawSequencerLane (juce::Graphics& g,
                                               juce::Rectangle<int> r,
                                               const LayoutElement& e) const
    {
        const auto* pack = proc.getPack();
        const auto* block = pack != nullptr ? findArpBlock (pack->dspGraph) : nullptr;
        const auto layout = SeqLaneUi::layout (r, e, block);
        const auto bg = e.backgroundColour.isTransparent() ? playerPanel().darker (0.08f) : e.backgroundColour;
        const auto borderC = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
        const auto fg = e.seqLaneColour.isTransparent() ? playerAccent() : e.seqLaneColour;
        const int laneIndex = juce::jlimit (0, 15, e.seqLaneIndex);

        g.setColour (bg);
        g.fillRoundedRectangle (r.toFloat(), juce::jmax (4.0f, e.cornerRadius));
        g.setColour (borderC);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (4.0f, e.cornerRadius), 1.0f);

        auto drawStepBtn = [&] (juce::Rectangle<int> btn, const juce::String& label)
        {
            g.setColour (playerPanel().brighter (0.12f));
            g.fillRoundedRectangle (btn.toFloat(), 4.0f);
            g.setColour (fg.withAlpha (0.75f));
            g.drawRoundedRectangle (btn.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (playerText());
            g.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            g.drawText (label, btn, juce::Justification::centred, true);
        };
        drawStepBtn (layout.stepsMinusBtn, "-");
        drawStepBtn (layout.stepsPlusBtn, "+");
        g.setColour (playerTextDim());
        g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
        g.drawText (e.label.isNotEmpty() ? e.label.toUpperCase() : e.seqLaneType.toUpperCase(),
                    layout.header, juce::Justification::centredLeft, true);

        if (! layout.valid)
            return;

        const double playback01 = proc.getSequencerPlaybackPosition01 (layout.steps);
        const int playbackStep = playback01 >= 0.0
            ? juce::jlimit (0, layout.steps - 1, (int) std::floor (playback01 * (double) layout.steps))
            : -1;
        const float cellW = (float) layout.grid.getWidth() / (float) layout.steps;

        for (int i = 0; i < layout.steps; ++i)
        {
            auto stepRect = juce::Rectangle<float> ((float) layout.grid.getX() + (float) i * cellW + 1.0f,
                                                    (float) layout.grid.getY(),
                                                    juce::jmax (3.0f, cellW - 2.0f),
                                                    (float) layout.grid.getHeight()).reduced (0.0f, 1.0f);
            const float val = SeqLaneUi::readStepValue (block, laneIndex, i, e.seqLaneType, 0.5f);

            g.setColour (bg.brighter (0.08f));
            g.fillRoundedRectangle (stepRect, 2.0f);

            if (e.seqLaneType == "gate")
            {
                if (val >= 0.5f)
                {
                    g.setColour (fg.withAlpha (i == playbackStep ? 0.95f : 0.72f));
                    g.fillRoundedRectangle (stepRect.reduced (stepRect.getWidth() * 0.12f, stepRect.getHeight() * 0.18f), 2.0f);
                }
            }
            else if (e.seqLaneType == "pitch")
            {
                const float norm = juce::jlimit (0.0f, 1.0f, (val + 24.0f) / 48.0f);
                g.setColour (fg.withAlpha (i == playbackStep ? 0.95f : 0.72f));
                g.fillRoundedRectangle (stepRect.withTrimmedTop (stepRect.getHeight() * (1.0f - norm)), 2.0f);
            }
            else if (e.seqLaneType == "chance")
            {
                juce::Path p;
                p.addStar (stepRect.getCentre(), 4, stepRect.getWidth() * 0.16f, stepRect.getWidth() * 0.34f * val);
                g.setColour (fg.withAlpha (i == playbackStep ? 0.95f : 0.55f + val * 0.35f));
                g.fillPath (p);
            }
            else
            {
                g.setColour (fg.withAlpha (i == playbackStep ? 0.95f : 0.72f));
                g.fillRoundedRectangle (stepRect.withTrimmedTop (stepRect.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, val))), 2.0f);
            }

            if (i == playbackStep)
            {
                g.setColour (juce::Colours::white.withAlpha (0.82f));
                g.drawRoundedRectangle (stepRect.reduced (0.5f), 2.0f, 1.2f);
            }
        }
    }

    bool PlayerGuiRenderer::handleSequencerLaneGesture (const juce::MouseEvent& event, bool drag)
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return false;
        const auto* block = findArpBlock (pack->dspGraph);

        const auto m = metrics();
        const auto pos = event.getPosition();
        for (auto it = elementsCopy.rbegin(); it != elementsCopy.rend(); ++it)
        {
            const auto& e = *it;
            if (! e.visible || e.type != ElementType::SequencerLane || ! isElementOnCurrentTab (e))
                continue;

            const auto r = animatedElementRect (e, elementRect (e, m));
            if (! r.contains (pos) && ! (drag && seqLaneDragActive))
                continue;

            const auto layout = SeqLaneUi::layout (r, e, block);
            const int laneIndex = juce::jlimit (0, 15, e.seqLaneIndex);

            if (! drag && layout.stepsMinusBtn.contains (pos))
            {
                if (proc.setArpLaneStepsFromUi (laneIndex, juce::jmax (1, layout.steps - 1)))
                    repaint (r);
                return true;
            }

            if (! drag && layout.stepsPlusBtn.contains (pos))
            {
                if (proc.setArpLaneStepsFromUi (laneIndex, juce::jmin (64, layout.steps + 1)))
                    repaint (r);
                return true;
            }

            if (! layout.valid)
                return true;

            const int step = SeqLaneUi::stepAtX (layout, pos.x);
            if (drag && seqLaneDragActive && lastSeqStep >= 0 && e.id == lastSeqElementId)
            {
                const float value = SeqLaneUi::valueFromY (layout, pos.y, e.seqLaneType);
                const bool active = e.seqLaneType == "gate" ? value >= 0.5f : true;
                proc.setSeqLaneStepFromUi (laneIndex, lastSeqStep, value, active, e.seqLaneType);
                repaint (r);
                return true;
            }

            if (! drag)
            {
                if (e.seqLaneType == "gate")
                {
                    const bool wasActive = SeqLaneUi::readStepValue (block, laneIndex, step, "gate", 0.0f) >= 0.5f;
                    const float velocity = ArpLaneUi::storedStepVelocity (block, laneIndex, step);
                    proc.setSeqLaneStepFromUi (laneIndex, step, velocity, ! wasActive, "gate");
                    seqLaneDragActive = ! wasActive;
                }
                else
                {
                    const float value = SeqLaneUi::valueFromY (layout, pos.y, e.seqLaneType);
                    proc.setSeqLaneStepFromUi (laneIndex, step, value, true, e.seqLaneType);
                    seqLaneDragActive = true;
                }
                lastSeqStep = step;
                lastSeqElementId = e.id;
                repaint (r);
                return true;
            }

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
                onRuntimeImportReport ("MIDI drag failed: this instrument has no MIDI pattern block.");
            return false;
        }

        DspBlock exportBlock = *block;
        const int lane = juce::jlimit (0, MidiPlaygroundPattern::kPhraseBankCount - 1, element.arpLaneIndex);
        MidiPlaygroundPattern::loadBank (exportBlock, lane, false);

        auto outputFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("Player")
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
        if (started && onRuntimeImportReport)
            onRuntimeImportReport ("MIDI exported to " + target.getFileName() + ". Drop it into your DAW.");
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
        const LayoutElement* backgroundElement = nullptr;
        for (const auto& e : elementsCopy)
            if (e.id == "background" && e.type == ElementType::Image && isElementOnCurrentTab (e))
            {
                backgroundElement = &e;
                break;
            }

        if (background.isValid() && (backgroundElement == nullptr || backgroundElement->visible))
        {
            juce::Graphics::ScopedSaveState backgroundOpacity (g);
            if (backgroundElement != nullptr)
                g.setOpacity (juce::jlimit (0.0f, 1.0f, backgroundElement->opacity));
            g.drawImage (background, m.canvas.toFloat());
        }

        // Find current preset name for the dropdown display.
        juce::String currentPresetName = pack->manifest.defaultPreset;
        if (currentPresetName.isEmpty() && ! pack->presets.empty())
            currentPresetName = pack->presets.front().name;

        // Draw non-control element types in z-order.
        for (auto& e : elementsCopy)
        {
            if (! e.visible) continue;
            if (isAuthoringOnlyLayoutElement (e)) continue;
            if (e.type == ElementType::Group) continue;
            if (! isElementOnCurrentTab (e)) continue;

            auto r = animatedElementRect (e, elementRect (e, m));
            juce::Graphics::ScopedSaveState opacityState (g);
            const float reactiveAlpha = e.audioReactive
                ? juce::jlimit (0.0f, 0.28f, proc.getAudioReactiveSignal (e.audioReactiveMode) * juce::jmax (0.1f, e.audioReactiveAmount))
                : 0.0f;
            g.setOpacity (juce::jlimit (0.0f, 1.0f, e.opacity + reactiveAlpha));

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl
                || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
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
                        const auto imageId = e.id.toLowerCase();
                        const bool logoLike = imageId == "logo" || imageId == "brand_logo"
                                           || imageId == "player_logo" || imageId.contains ("logo");
                        if (logoLike && manifest() != nullptr && manifest()->playerLogoImage.isNotEmpty())
                            assetPath = manifest()->playerLogoImage;

                        if (assetPath.isNotEmpty())
                        {
                            auto f = juce::File::isAbsolutePath (assetPath)
                                        ? juce::File (assetPath)
                                        : pack->rootFolder.getChildFile (assetPath);
                            if (auto img = assets.loadImage (f); img.isValid())
                                g.drawImage (img, r.toFloat());
                        }
                        else if (e.id == "hero" && heroImage.isValid())
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
                case ElementType::AdsrCurve: drawAdsrCurve (g, r, e); break;
                case ElementType::SpectrumAnalyzer: drawSpectrumAnalyzer (g, r, e); break;
                case ElementType::ReactiveImage:
                {
                    const float level = reactiveLevelFor (e);
                    juce::String assetPath = e.asset;
                    if (assetPath.isNotEmpty())
                    {
                        auto f = juce::File::isAbsolutePath (assetPath)
                                    ? juce::File (assetPath)
                                    : pack->rootFolder.getChildFile (assetPath);
                        if (auto img = assets.loadImage (f); img.isValid())
                            g.drawImage (img, r.toFloat());
                    }
                    else
                    {
                        const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
                        g.setColour (playerPanel().withAlpha (0.70f));
                        g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                        g.setColour (accent.withAlpha (0.75f));
                        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);
                    }
                    // Overlay the authored audio-reactive effect on top of the artwork.
                    drawVisualFx (g, r, e, level);
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
                            float driveVal = 0.0f;
                            if (e.parameterId.isNotEmpty())
                            {
                                if (const auto* pDef = parameterForId (e.parameterId))
                                {
                                    const float val = parameterValueForElement (e, pDef->defaultValue);
                                    const float range = pDef->max - pDef->min;
                                    driveVal = range > 0.0001f ? juce::jlimit (0.0f, 1.0f, (val - pDef->min) / range) : 0.0f;
                                }
                            }
                            const int frames = juce::jmax (1, e.filmstripFrames > 0 ? e.filmstripFrames : 8);
                            int frame = 0;
                            if (e.animationMode == "parameter")
                            {
                                frame = juce::jlimit (0, frames - 1, juce::roundToInt (driveVal * (float) (frames - 1)));
                            }
                            else
                            {
                                frame = ((int) std::floor (juce::Time::getMillisecondCounterHiRes() * 0.001
                                                           * juce::jmax (0.05f, e.animationRate))) % frames;
                            }
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
                    drawVisualFx (g, r, e, reactiveLevelFor (e));
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
                    const auto& peaks = proc.getUserWaveformPeaks();
                    const bool isStep = e.label.containsIgnoreCase ("Step");
                    if (! peaks.empty() && ! isStep)
                    {
                        // Draw the real waveform of the most recently dropped sample.
                        const auto bounds = r.reduced (6).toFloat();
                        const float midY = bounds.getCentreY();
                        const int n = (int) peaks.size();
                        g.setColour (playerAccent().withAlpha (0.85f));
                        for (int i = 0; i < n; ++i)
                        {
                            const float x = bounds.getX() + (float) i / (float) juce::jmax (1, n - 1) * bounds.getWidth();
                            const float h = juce::jlimit (0.0f, 1.0f, peaks[(size_t) i]) * bounds.getHeight() * 0.46f;
                            g.drawLine (x, midY - h, x, midY + h, 1.0f);
                        }
                        const double playback01 = proc.getSequencerPlaybackPosition01();
                        if (playback01 >= 0.0)
                        {
                            const float playheadX = bounds.getX() + (float) playback01 * bounds.getWidth();
                            g.setColour (playerAccent().withAlpha (0.95f));
                            g.drawLine (playheadX, bounds.getY(), playheadX, bounds.getBottom(), 1.6f);
                        }
                        break;
                    }
                    g.setColour (playerAccent().withAlpha (0.8f));
                    juce::Path wave;
                    const auto bounds = r.reduced (6).toFloat();
                    const auto level = juce::jlimit (0.05f, 1.0f, proc.getOutputPeak() + 0.08f);
                    if (isStep)
                    {
                        const int numSteps = 12;
                        for (int i = 0; i <= numSteps; ++i)
                        {
                            const float x = bounds.getX() + (float) i / (float) numSteps * bounds.getWidth();
                            const int stepIndex = juce::jmin (i, numSteps - 1);
                            const float phase = (float) stepIndex / (float) numSteps * juce::MathConstants<float>::twoPi * 3.0f;
                            const float y = bounds.getCentreY() + std::sin (phase) * bounds.getHeight() * 0.42f * level;
                            if (i == 0)
                            {
                                wave.startNewSubPath (x, y);
                            }
                            else
                            {
                                wave.lineTo (x, wave.getCurrentPosition().y);
                                wave.lineTo (x, y);
                            }
                        }
                    }
                    else
                    {
                        for (int i = 0; i < 64; ++i)
                        {
                            const float x = bounds.getX() + (float) i / 63.0f * bounds.getWidth();
                            const float y = bounds.getCentreY()
                                + std::sin ((float) i * 0.48f) * bounds.getHeight() * 0.42f * level;
                            if (i == 0) wave.startNewSubPath (x, y);
                            else        wave.lineTo (x, y);
                        }
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
                case ElementType::SampleDropZone:
                {
                    const auto bg = e.backgroundColour.isTransparent() ? playerPanel().withAlpha (0.78f) : e.backgroundColour;
                    const auto border = e.borderColour.isTransparent() ? playerBorder() : e.borderColour;
                    const auto accent = e.accentColour.isTransparent() ? playerAccent() : e.accentColour;
                    g.setColour (bg);
                    g.fillRoundedRectangle (r.toFloat(), juce::jmax (5.0f, e.cornerRadius));
                    g.setColour (border);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), juce::jmax (5.0f, e.cornerRadius), 1.0f);

                    auto area = r.reduced (10, 8);
                    const auto& dropPeaks = proc.getUserWaveformPeaks();
                    auto waveBounds = area.removeFromTop (juce::jmax (34, area.getHeight() / 2)).toFloat();
                    g.setColour (accent.withAlpha (0.8f));
                    if (! dropPeaks.empty())
                    {
                        const float midY = waveBounds.getCentreY();
                        const int n = (int) dropPeaks.size();
                        for (int i = 0; i < n; ++i)
                        {
                            const float x = waveBounds.getX() + (float) i / (float) juce::jmax (1, n - 1) * waveBounds.getWidth();
                            const float h = juce::jlimit (0.0f, 1.0f, dropPeaks[(size_t) i]) * waveBounds.getHeight() * 0.46f;
                            g.drawLine (x, midY - h, x, midY + h, 1.0f);
                        }
                    }
                    else
                    {
                        juce::Path wave;
                        for (int i = 0; i < 48; ++i)
                        {
                            const float x = waveBounds.getX() + (float) i / 47.0f * waveBounds.getWidth();
                            const float y = waveBounds.getCentreY()
                                + std::sin ((float) i * 0.54f) * waveBounds.getHeight() * 0.35f;
                            if (i == 0) wave.startNewSubPath (x, y);
                            else        wave.lineTo (x, y);
                        }
                        g.strokePath (wave, juce::PathStrokeType (1.4f));
                    }
                    g.setColour (playerText());
                    g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
                    g.drawFittedText (e.label.isNotEmpty() ? e.label : juce::String ("Drop Sample"),
                                      area.removeFromTop (22), juce::Justification::centred, 1);
                    g.setColour (playerTextDim());
                    g.setFont (juce::FontOptions (9.0f));
                    g.drawFittedText (dropPeaks.empty() ? juce::String ("Drag an audio file here")
                                                        : juce::String ("Playable across the keyboard"),
                                      area, juce::Justification::centred, 2);
                    break;
                }
                case ElementType::RuntimeSampleLibrary:
                    drawRuntimeSampleLibrary (g, r, e);
                    break;
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
                case ElementType::SequencerLane:
                    drawSequencerLane (g, r, e);
                    break;
                case ElementType::PianoRoll:
                    drawPianoRoll (g, r, e);
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
                {
                    drawDropdown (g, r, e.id == "presets" ? currentPresetName
                                     : e.parameterId.isNotEmpty() ? ((e.label.isNotEmpty() ? e.label + ": " : juce::String()) + formattedParameterValue (e))
                                                                  : e.label);
                    if (e.parameterId.startsWith ("composerDegree"))
                    {
                        const int chordCount = juce::jlimit (1, 16, juce::roundToInt (
                            proc.getPackParameterValue ("composerChordCount")));
                        const int chordIndex = e.parameterId.fromFirstOccurrenceOf ("composerDegree", false, false).getIntValue() - 1;
                        const double playback = proc.getSequencerPlaybackPosition01 (chordCount);
                        const int activeChord = playback >= 0.0
                            ? juce::jlimit (0, chordCount - 1, (int) std::floor (playback * chordCount)) : -1;
                        if (chordIndex == activeChord)
                        {
                            g.setColour (e.accentColour.isTransparent() ? playerAccent() : e.accentColour);
                            g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 5.0f, 2.5f);
                        }
                    }
                    break;
                }

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

            if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl
                || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
                drawControlLabelOverlay (g, e, r);
        }

        if (fileDragActive && ! fileDragHighlight.isEmpty())
        {
            const auto accent = playerAccent();
            const auto area = fileDragHighlight.toFloat();
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (area, 9.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (area.reduced (1.0f), 9.0f, 2.4f);
            if (fileDragLabel.isNotEmpty())
            {
                auto badge = juce::Rectangle<int> (fileDragHighlight.getX(), fileDragHighlight.getY() - 26,
                                                   juce::jmax (140, fileDragLabel.length() * 8 + 24), 22)
                                 .constrainedWithin (getLocalBounds());
                g.setColour (juce::Colour (0xee0a0e16));
                g.fillRoundedRectangle (badge.toFloat(), 6.0f);
                g.setColour (accent);
                g.drawRoundedRectangle (badge.toFloat(), 6.0f, 1.0f);
                g.setColour (juce::Colours::white);
                g.setFont (juce::Font (12.0f, juce::Font::bold));
                g.drawFittedText (fileDragLabel, badge.reduced (8, 0), juce::Justification::centredLeft, 1);
            }
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
                if (e.type == ElementType::Knob || e.type == ElementType::Slider || e.type == ElementType::MacroControl
                    || e.type == ElementType::PitchWheel || e.type == ElementType::ModWheel)
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

        if (handleSequencerLaneGesture (event, false))
            return;

        if (handlePianoRollGesture (event, false))
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

            if (e.type == ElementType::Button && e.action.equalsIgnoreCase ("transport.toggle"))
            {
                proc.toggleInternalTransport();
                repaint (r);
                return;
            }

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
            if (std::abs (delta.x) + std::abs (delta.y) >= 6)
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
        if (handleSequencerLaneGesture (event, true))
            return;
        if (handlePianoRollGesture (event, true))
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
        cancelPianoRollDrag();
        arpLaneDragActive = false;
        lastArpLane = -1;
        lastArpStep = -1;
        seqLaneDragActive = false;
        lastSeqStep = -1;
        lastSeqElementId.clear();
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
        // Runtime Animation submenu removed — developer tool, not shown to end-users.

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
            // Roll the level history used by waveform/scope FX.
            levelHistory[(size_t) levelHistoryHead] = peak;
            levelHistoryHead = (levelHistoryHead + 1) % kLevelHistorySize;

            if (std::abs (peak - lastOutputPeak) > 0.01f)
            {
                lastOutputPeak = peak;
                needsRepaint = true;
            }
        }

        // Continuous visual FX (orbit/sweep/particles/scope) must animate even
        // when audio is silent, so tick them every frame.
        if (hasContinuousFxElement)
            needsRepaint = true;

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
        else if (def->id == "arpLaneGroup")
        {
            for (int groupIndex = 0; groupIndex < 8; ++groupIndex)
            {
                values.push_back ((float) groupIndex);
                menu.addItem (groupIndex + 1, "Group " + juce::String (groupIndex + 1));
            }
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
        else if (def->id == "arpLaneSound")
        {
            for (int sound = 0; sound < 16; ++sound)
            {
                values.push_back ((float) sound);
                menu.addItem (sound + 1, orbitLaneSoundName (sound));
            }
        }
        else if (def->id == "arpLaneFxTarget")
        {
            static const char* fxTargets[] = { "Delay", "Reverb", "Chorus", "Phaser", "Drive", "Resonance", "Width", "Tape" };
            for (int target = 0; target < 8; ++target)
            {
                values.push_back ((float) target);
                menu.addItem (target + 1, fxTargets[target]);
            }
        }
        else if (def->id == "arpLanePatternLaunch")
        {
            for (int preset = 0; preset < 8; ++preset)
            {
                values.push_back ((float) preset);
                menu.addItem (preset + 1, circleSeqPatternName (preset));
            }
        }
        else if (def->id == "composerRoot")
        {
            for (int root = 0; root < 12; ++root)
            {
                values.push_back ((float) root);
                menu.addItem (root + 1, HarmonyEngine::pitchClassName (root));
            }
        }
        else if (def->id == "composerScale")
        {
            const auto& scales = HarmonyEngine::scales();
            for (int scale = 0; scale < (int) scales.size(); ++scale)
            {
                values.push_back ((float) scale);
                menu.addItem (scale + 1, scales[(size_t) scale].name);
            }
        }
        else if (def->id.startsWith ("composerDegree"))
        {
            static const char* degrees[] = { "I", "ii", "iii", "IV", "V", "vi", "vii" };
            for (int degree = 0; degree < 7; ++degree)
            {
                values.push_back ((float) degree);
                menu.addItem (degree + 1, degrees[degree]);
            }
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
        else if (populateSteppedParameterMenu (*def, menu, values))
        {
        }
        else if (def->displayMode == "stepped" || (def->step >= 1.0f && def->max - def->min <= 64.0f))
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
        if (onPresetBrowserRequested)
        {
            onPresetBrowserRequested();
            return;
        }

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
