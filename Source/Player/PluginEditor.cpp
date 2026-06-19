#include "PluginEditor.h"
#include "HarmonyEngine.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static constexpr int kPlayerMenuBarHeight = 108;
        static constexpr int kPlayerTitleAreaHeight = 66;
        static constexpr int kPlayerTitleBarArtworkSize = 46;

        static juce::Rectangle<int> playerChromeBoundsFor (juce::Rectangle<int> topBar,
                                                           const PatchCraftPack* pack)
        {
            int width = topBar.getWidth();
            if (pack != nullptr && pack->canvasSize.width > 0)
                width = juce::jlimit (juce::jmin (topBar.getWidth(), 640),
                                      topBar.getWidth(),
                                      pack->canvasSize.width);

            return juce::Rectangle<int> (width, topBar.getHeight()).withCentre (topBar.getCentre());
        }

        static juce::String playerInstrumentName (const PatchCraftPack* pack)
        {
            if (pack == nullptr)
                return "No Instrument";

            return pack->manifest.playerDisplayName.isNotEmpty()
                ? pack->manifest.playerDisplayName
                : pack->manifest.instrumentName;
        }

        static int playerTitleBrandWidth (const juce::String& theme, int barWidth)
        {
            const bool compact = barWidth < 980;
            if (theme == "no-chrome")
                return compact ? 0 : 12;
            if (theme == "custom")
                return compact ? 240 : 390;
            if (theme == "wide-banner")
                return compact ? 240 : 390;
            if (theme == "artist-card")
                return compact ? 226 : 340;
            if (theme == "banner")
                return compact ? 206 : 300;
            if (theme == "split-brand")
                return compact ? 192 : 280;
            if (theme == "logo-rail")
                return compact ? 90 : 126;
            if (theme == "minimal")
                return compact ? 118 : 170;
            if (theme == "compact-daw")
                return 146;
            return compact ? 146 : 218;
        }

        static bool titleThemeUsesBannerArtwork (const juce::String& theme)
        {
            return theme == "banner"
                || theme == "custom"
                || theme == "wide-banner"
                || theme == "artist-card";
        }

        static juce::Font playerChromeFont (const juce::String& family, float size, bool bold)
        {
            juce::Font font (size, bold ? juce::Font::bold : juce::Font::plain);
            if (family.isNotEmpty() && family != "Default")
                font.setTypefaceName (family);
            return font;
        }

        static juce::String cleanedSectionName (juce::String section)
        {
            section = section.trim();
            if (section.isEmpty())
                return "Global";

            if (section.equalsIgnoreCase ("fx"))
                return "FX";

            if (section.length() == 1)
                return section.toUpperCase();

            return section.substring (0, 1).toUpperCase()
                 + section.substring (1).toLowerCase();
        }

        static juce::String formatParameterValue (const ParameterDef& parameter, float value)
        {
            if (parameter.displayMode == "toggle")
                return value >= (parameter.min + parameter.max) * 0.5f ? "On" : "Off";

            auto choiceText = [&] (const juce::StringArray& choices)
            {
                const int index = juce::jlimit (0, choices.size() - 1, juce::roundToInt (value));
                return choices[index];
            };

            if (parameter.id == "oscType" || parameter.id == "osc2Type")
                return choiceText ({ "Sine", "Saw", "Square", "Triangle", "Noise" });

            if (parameter.id == "wtTable")
                return choiceText ({ "Analog", "Glass", "PWM", "Formant", "Razor", "Organ", "Aggro", "Hybrid", "Custom" });

            if (parameter.id.endsWithIgnoreCase ("Type") && parameter.id.startsWithIgnoreCase ("eqBand"))
                return choiceText ({ "Bell", "Low Shelf", "High Shelf", "High Pass", "Low Pass", "Notch" });

            if (parameter.id == "wtPhaseMode")
                return choiceText ({ "Reset", "Random", "Spread" });

            if (parameter.id == "granularDirection")
                return choiceText ({ "Forward", "Reverse", "Ping-Pong", "Multi" });

            if (parameter.id == "granularWindow")
                return choiceText ({ "Hann", "Triangle", "Blackman", "Plateau" });

            if (parameter.id == "composerRoot")
                return HarmonyEngine::pitchClassName (juce::roundToInt (value));

            if (parameter.id == "composerScale")
                return HarmonyEngine::scaleAt (juce::roundToInt (value)).name;

            if (parameter.id.startsWith ("composerDegree"))
                return choiceText ({ "I", "ii", "iii", "IV", "V", "vi", "vii" });

            if (parameter.displayMode == "stepped" || parameter.step >= 1.0f)
                return juce::String (juce::roundToInt (value)) + parameter.unit;

            if (parameter.unit == "Hz" && value >= 1000.0f)
                return juce::String (value / 1000.0f, 2) + " kHz";

            if (parameter.unit == "s")
                return value < 1.0f ? juce::String (value * 1000.0f, 0) + " ms"
                                    : juce::String (value, 2) + " s";

            if (parameter.unit.isNotEmpty())
                return juce::String (value, 2) + " " + parameter.unit;

            return juce::String (value, 3);
        }

        static bool parameterIsUnlocked (const PlayerProcessor& processor, const ParameterDef& parameter)
        {
            if (parameter.enabledBy.isEmpty())
                return true;

            return processor.getPackParameterValue (parameter.enabledBy) > 0.0001f;
        }

        static juce::String parameterTooltip (const PlayerProcessor& processor, const ParameterDef& parameter)
        {
            juce::String tooltip = parameter.name + " (" + parameter.id + ")";
            if (parameter.category.isNotEmpty())
                tooltip += "\nCategory: " + parameter.category;
            if (parameter.enabledBy.isNotEmpty() && ! parameterIsUnlocked (processor, parameter))
            {
                tooltip += "\nDisabled: " + (parameter.enableHint.isNotEmpty()
                    ? parameter.enableHint
                    : "Enable " + parameter.enabledBy + " before this control affects sound.");
            }
            if (processor.isParameterMidiLearnable (parameter.id))
                tooltip += "\nRight-click the instrument control or use Learn here to assign MIDI.";
            return tooltip;
        }

        static bool stringArrayContains (const juce::StringArray& values, const juce::String& target)
        {
            for (const auto& value : values)
                if (value == target)
                    return true;
            return false;
        }

        static bool isRuntimeImportPath (const juce::String& path)
        {
            const juce::File file (path);
            if (file.isDirectory())
                return true;

            const auto extension = file.getFileExtension().toLowerCase();
            return extension == ".wav"
                || extension == ".aif"
                || extension == ".aiff"
                || extension == ".flac"
                || extension == ".mp3"
                || extension == ".ogg"
                || extension == ".mid"
                || extension == ".midi";
        }

        static bool isSupportedRuntimeImportFile (const juce::File& file)
        {
            if (! file.existsAsFile())
                return false;

            const auto extension = file.getFileExtension().toLowerCase();
            return extension == ".wav"
                || extension == ".aif"
                || extension == ".aiff"
                || extension == ".flac"
                || extension == ".mp3"
                || extension == ".ogg"
                || extension == ".mid"
                || extension == ".midi";
        }

        static juce::StringArray collectRuntimeImportFiles (const juce::StringArray& sources,
                                                            int maxFiles = 512)
        {
            juce::StringArray paths;

            for (const auto& sourcePath : sources)
            {
                if (paths.size() >= maxFiles)
                    break;

                const juce::File source (sourcePath);
                if (isSupportedRuntimeImportFile (source))
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
                    if (isSupportedRuntimeImportFile (child))
                        paths.addIfNotAlreadyThere (child.getFullPathName());
                }
            }

            return paths;
        }

        static bool layoutReferencesParameter (const PatchCraftPack& pack, const juce::String& parameterId)
        {
            for (const auto& element : pack.layout.getAll())
            {
                if (element.parameterId == parameterId)
                    return true;

                if (stringArrayContains (element.mixerVolumeParams, parameterId)
                    || stringArrayContains (element.mixerPanParams, parameterId)
                    || stringArrayContains (element.mixerMuteParams, parameterId)
                    || stringArrayContains (element.mixerSoloParams, parameterId))
                    return true;
            }
            return false;
        }

        static bool graphReferencesParameter (const DspGraph& graph, const juce::String& parameterId)
        {
            for (const auto& block : graph.blocks)
            {
                if (block.targetId == parameterId || block.values.find (parameterId) != block.values.end())
                    return true;
            }

            for (const auto& macro : graph.macros)
                if (macro.macroId == parameterId || macro.targetId == parameterId)
                    return true;

            for (const auto& mod : graph.modulation)
                if (mod.sourceId == parameterId || mod.targetId == parameterId)
                    return true;

            for (const auto& lane : graph.automation)
                if (lane.targetId == parameterId)
                    return true;

            for (const auto& item : graph.quickEditControls)
                if (stringArrayContains (item.second, parameterId))
                    return true;

            return false;
        }

        static bool isCoreRuntimeParameter (const PatchCraftPack& pack, const juce::String& parameterId)
        {
            static const juce::StringArray coreIds {
                "volume", "pan", "expression", "modWheel", "pitchBend",
                "bpmSync", "retrigger", "attack", "decay", "sustain", "release"
            };

            if (stringArrayContains (coreIds, parameterId))
                return true;

            static const juce::StringArray sampleIds {
                "sampleStart", "sampleLength", "sampleSlice", "sampleSliceCount",
                "samplePitch", "sampleReverse", "sampleGlitch", "sampleGlitchGrid",
                "granularOn", "granularDensity", "granularSizeMs", "granularSizeRandom",
                "granularSpread", "granularScan", "granularPitchSpread", "granularPanSpread",
                "granularReverse", "granularTexture", "granularMaxGrains", "granularDirection",
                "granularWindow", "granularFreeze"
            };

            return ! pack.sampleMap.getZones().empty() && stringArrayContains (sampleIds, parameterId);
        }

        static bool parameterBelongsToLoadedInstrument (const PatchCraftPack& pack, const ParameterDef& parameter)
        {
            if (! parameter.visible)
                return false;

            if (layoutReferencesParameter (pack, parameter.id))
                return true;

            if (graphReferencesParameter (pack.dspGraph, parameter.id))
                return true;

            if (isCoreRuntimeParameter (pack, parameter.id))
                return true;

            for (const auto& mapping : pack.midiMappings)
                if (mapping.parameterId == parameter.id)
                    return true;

            return false;
        }

        static juce::StringArray choicesForParameter (const ParameterDef& parameter)
        {
            if (parameter.id == "granularDirection")
                return { "Forward", "Reverse", "Ping-Pong", "Multi" };
            if (parameter.id == "granularWindow")
                return { "Hann", "Triangle", "Blackman", "Plateau" };
            if (parameter.id == "oscType" || parameter.id == "osc2Type")
                return { "Sine", "Saw", "Square", "Triangle", "Noise" };
            if (parameter.id == "wtTable")
                return { "Analog", "Glass", "PWM", "Formant", "Razor", "Organ", "Aggro", "Hybrid", "Custom" };
            if (parameter.id == "wtPhaseMode")
                return { "Reset", "Random", "Spread" };
            if (parameter.id.endsWithIgnoreCase ("Type") && parameter.id.startsWithIgnoreCase ("eqBand"))
                return { "Bell", "Low Shelf", "High Shelf", "High Pass", "Low Pass", "Notch" };

            if ((parameter.displayMode == "stepped" || parameter.step >= 1.0f)
                && parameter.max - parameter.min <= 16.0f)
            {
                juce::StringArray choices;
                for (int value = (int) parameter.min; value <= (int) parameter.max; value += (int) juce::jmax (1.0f, parameter.step))
                    choices.add (juce::String (value) + (parameter.unit.isNotEmpty() ? " " + parameter.unit : ""));
                return choices;
            }

            return {};
        }

        static bool parameterIsBypassCandidate (const ParameterDef& parameter)
        {
            return parameter.displayMode == "toggle"
                || parameter.id.endsWithIgnoreCase ("Mix")
                || parameter.id.endsWithIgnoreCase ("Amount")
                || parameter.id.endsWithIgnoreCase ("Depth")
                || parameter.id.endsWithIgnoreCase ("Level")
                || parameter.id.containsIgnoreCase ("enabled")
                || parameter.name.containsIgnoreCase ("mix")
                || parameter.name.containsIgnoreCase ("bypass");
        }
    }

    class PlayerPerformancePanel : public juce::Component,
                                   private juce::Timer
    {
    public:
        explicit PlayerPerformancePanel (PlayerProcessor& processorToUse)
            : processor (processorToUse)
        {
            titleLabel.setText ("Sound Controls", juce::dontSendNotification);
            titleLabel.setJustificationType (juce::Justification::centredLeft);
            titleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
            addAndMakeVisible (titleLabel);

            subtitleLabel.setText ("Dedicated runtime panel for the loaded instrument: source, pads, macros, FX, MIDI Learn, reset, and bypass controls.",
                                   juce::dontSendNotification);
            subtitleLabel.setJustificationType (juce::Justification::centredLeft);
            subtitleLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            addAndMakeVisible (subtitleLabel);

            statusLabel.setJustificationType (juce::Justification::centredLeft);
            statusLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            addAndMakeVisible (statusLabel);

            closeButton.setButtonText ("Close");
            closeButton.onClick = [this]
            {
                if (onClose)
                    onClose();
            };
            addAndMakeVisible (closeButton);

            floatButton.setButtonText ("Dock");
            floatButton.setTooltip ("Sound Controls is now a dedicated workspace.");
            floatButton.onClick = [this]
            {
                floating = ! floating;
                floatButton.setButtonText (floating ? "Dock" : "Float");
                if (onToggleFloat)
                    onToggleFloat (floating);
            };
            addAndMakeVisible (floatButton);
            floatButton.setVisible (false);

            randomizeButton.setButtonText ("Randomize");
            randomizeButton.setTooltip ("Randomize this patch's exposed synth/sample/FX controls.");
            randomizeButton.onClick = [this]
            {
                processor.randomizeCurrentPreset();
                syncRows();
            };
            addAndMakeVisible (randomizeButton);

            restoreButton.setButtonText ("Restore");
            restoreButton.setTooltip ("Restore values to the current patch defaults.");
            restoreButton.onClick = [this]
            {
                processor.restoreAllPresets();
                syncRows();
            };
            addAndMakeVisible (restoreButton);

            defaultButton.setButtonText ("Set Default");
            defaultButton.setTooltip ("Use the current values as the recall default for this Player session.");
            defaultButton.onClick = [this] { processor.setDefaultPreset(); };
            addAndMakeVisible (defaultButton);

            sectionBox.setTextWhenNothingSelected ("Section");
            sectionBox.onChange = [this] { rebuildRows(); };
            addAndMakeVisible (sectionBox);

            viewport.setViewedComponent (&rowContainer, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (8);
            addAndMakeVisible (viewport);

            startTimerHz (4);
        }

        ~PlayerPerformancePanel() override
        {
            viewport.setViewedComponent (nullptr, false);
        }

        std::function<void()> onClose;
        std::function<void(bool)> onToggleFloat;

        void setFloating (bool shouldFloat)
        {
            floating = shouldFloat;
            floatButton.setButtonText (floating ? "Dock" : "Float");
            floatButton.setVisible (false);
        }

        void rebuild()
        {
            rebuildSectionFilter();
            rebuildRows();
            updateStatus();
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();
            graphics.setColour (juce::Colour (0xee0b0d10));
            graphics.fillRoundedRectangle (bounds.reduced (2.0f), 14.0f);

            graphics.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
            graphics.drawRoundedRectangle (bounds.reduced (2.5f), 14.0f, 1.6f);

            auto header = getLocalBounds().removeFromTop (86).reduced (14, 10).toFloat();
            graphics.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.88f));
            graphics.fillRoundedRectangle (header, 10.0f);
            graphics.setColour (PatchCraftLookAndFeel::borderSoft());
            graphics.drawRoundedRectangle (header, 10.0f, 1.0f);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (14);
            auto header = bounds.removeFromTop (76);

            auto topLine = header.removeFromTop (28);
            titleLabel.setBounds (topLine.removeFromLeft (260));
            closeButton.setBounds (topLine.removeFromRight (64));
            floatButton.setBounds (0, 0, 0, 0);

            subtitleLabel.setBounds (header.removeFromTop (22));
            statusLabel.setBounds (header.removeFromTop (22));

            bounds.removeFromTop (10);
            auto controls = bounds.removeFromTop (30);
            sectionBox.setBounds (controls.removeFromLeft (180));
            controls.removeFromLeft (8);
            randomizeButton.setBounds (controls.removeFromLeft (86));
            controls.removeFromLeft (8);
            restoreButton.setBounds (controls.removeFromLeft (74));
            controls.removeFromLeft (8);
            defaultButton.setBounds (controls.removeFromLeft (90));

            bounds.removeFromTop (10);
            viewport.setBounds (bounds);
            layoutRows();
        }

    private:
        struct ParameterRow : public juce::Component,
                              public juce::SettableTooltipClient
        {
            ParameterRow (PlayerProcessor& processorToUse, ParameterDef parameterToUse)
                : processor (processorToUse), parameter (std::move (parameterToUse))
            {
                choices = choicesForParameter (parameter);
                usesToggle = parameter.displayMode == "toggle"
                          || (parameter.step >= 1.0f && parameter.min == 0.0f && parameter.max == 1.0f);
                usesChoice = ! usesToggle && ! choices.isEmpty();

                nameLabel.setText (parameter.name, juce::dontSendNotification);
                nameLabel.setJustificationType (juce::Justification::centredLeft);
                nameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
                addAndMakeVisible (nameLabel);

                categoryLabel.setText (cleanedSectionName (parameter.section) + " / " + parameter.category,
                                       juce::dontSendNotification);
                categoryLabel.setJustificationType (juce::Justification::centredLeft);
                categoryLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (categoryLabel);

                valueLabel.setJustificationType (juce::Justification::centredRight);
                valueLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                addAndMakeVisible (valueLabel);

                slider.setSliderStyle (juce::Slider::LinearHorizontal);
                slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                slider.setRange (parameter.min, parameter.max,
                                 parameter.step > 0.0f ? parameter.step
                                                       : 0.0);
                slider.setDoubleClickReturnValue (true, parameter.defaultValue);
                slider.onValueChange = [this]
                {
                    if (syncing)
                        return;

                    processor.setPackParameterFromUi (parameter.id, (float) slider.getValue());
                    syncFromProcessor();
                };
                addAndMakeVisible (slider);

                toggle.setButtonText ("On");
                toggle.setTooltip ("Enable or bypass this parameter.");
                toggle.onClick = [this]
                {
                    if (syncing)
                        return;

                    processor.setPackParameterFromUi (parameter.id, toggle.getToggleState() ? parameter.max : parameter.min);
                    syncFromProcessor();
                };
                addAndMakeVisible (toggle);

                choiceBox.setTextWhenNothingSelected ("Choose");
                for (int index = 0; index < choices.size(); ++index)
                    choiceBox.addItem (choices[index], index + 1);
                choiceBox.onChange = [this]
                {
                    if (syncing || choiceBox.getSelectedId() <= 0)
                        return;

                    const float stepped = parameter.min
                        + (float) (choiceBox.getSelectedId() - 1) * juce::jmax (1.0f, parameter.step > 0.0f ? parameter.step : 1.0f);
                    processor.setPackParameterFromUi (parameter.id, juce::jlimit (parameter.min, parameter.max, stepped));
                    syncFromProcessor();
                };
                addAndMakeVisible (choiceBox);

                bypassButton.setButtonText ("Bypass");
                bypassButton.setTooltip ("Set this control to its bypass/minimum value.");
                bypassButton.onClick = [this]
                {
                    if (syncing)
                        return;
                    processor.setPackParameterFromUi (parameter.id, parameter.min);
                    syncFromProcessor();
                };
                addAndMakeVisible (bypassButton);

                resetButton.setButtonText ("Reset");
                resetButton.setTooltip ("Reset this control to its instrument default.");
                resetButton.onClick = [this]
                {
                    if (syncing)
                        return;
                    processor.setPackParameterFromUi (parameter.id, parameter.defaultValue);
                    syncFromProcessor();
                };
                addAndMakeVisible (resetButton);

                learnButton.setButtonText ("Learn");
                learnButton.setTooltip ("Assign a hardware MIDI CC to this parameter.");
                learnButton.onClick = [this]
                {
                    if (processor.isParameterMidiLearnable (parameter.id))
                        processor.beginMidiLearn (parameter.id);
                };
                addAndMakeVisible (learnButton);

                setTooltip (parameterTooltip (processor, parameter));
                syncFromProcessor();
            }

            void paint (juce::Graphics& graphics) override
            {
                const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
                graphics.setColour (enabled ? PatchCraftLookAndFeel::panelAlt().withAlpha (0.82f)
                                            : PatchCraftLookAndFeel::panel().withAlpha (0.54f));
                graphics.fillRoundedRectangle (bounds, 8.0f);
                graphics.setColour (enabled ? PatchCraftLookAndFeel::border()
                                            : PatchCraftLookAndFeel::borderSoft().withAlpha (0.7f));
                graphics.drawRoundedRectangle (bounds, 8.0f, 1.0f);

                if (! enabled)
                {
                    graphics.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.22f));
                    graphics.fillRoundedRectangle (bounds.reduced (6.0f), 6.0f);
                }
            }

            void resized() override
            {
                auto bounds = getLocalBounds().reduced (10, 6);
                auto left = bounds.removeFromLeft (juce::jlimit (145, 240, getWidth() / 4));
                nameLabel.setBounds (left.removeFromTop (22));
                categoryLabel.setBounds (left);

                learnButton.setBounds (bounds.removeFromRight (54).reduced (0, 5));
                resetButton.setBounds (bounds.removeFromRight (54).reduced (2, 5));
                if (parameterIsBypassCandidate (parameter))
                    bypassButton.setBounds (bounds.removeFromRight (64).reduced (2, 5));
                else
                    bypassButton.setBounds (bounds.withWidth (0));
                valueLabel.setBounds (bounds.removeFromRight (76).reduced (4, 0));

                auto editor = bounds.reduced (8, 7);
                if (usesToggle)
                    toggle.setBounds (editor.removeFromLeft (72));
                else
                    toggle.setBounds (editor.withWidth (0));

                choiceBox.setBounds (editor);
                slider.setBounds (editor);
            }

            void syncFromProcessor()
            {
                syncing = true;
                const auto value = processor.getPackParameterValue (parameter.id);
                const bool unlocked = parameterIsUnlocked (processor, parameter);
                enabled = unlocked;
                slider.setEnabled (unlocked);
                toggle.setEnabled (unlocked);
                choiceBox.setEnabled (unlocked);
                learnButton.setEnabled (unlocked && processor.isParameterMidiLearnable (parameter.id));
                bypassButton.setEnabled (unlocked && parameterIsBypassCandidate (parameter));
                resetButton.setEnabled (unlocked);

                slider.setVisible (! usesToggle && ! usesChoice);
                toggle.setVisible (usesToggle);
                choiceBox.setVisible (usesChoice);
                bypassButton.setVisible (parameterIsBypassCandidate (parameter));

                slider.setValue (value, juce::dontSendNotification);
                toggle.setToggleState (value >= (parameter.min + parameter.max) * 0.5f, juce::dontSendNotification);
                if (usesChoice)
                {
                    const int choiceIndex = juce::jlimit (0, choices.size() - 1,
                        juce::roundToInt ((value - parameter.min) / juce::jmax (1.0f, parameter.step > 0.0f ? parameter.step : 1.0f)));
                    choiceBox.setSelectedId (choiceIndex + 1, juce::dontSendNotification);
                }

                valueLabel.setText (formatParameterValue (parameter, value), juce::dontSendNotification);
                setTooltip (parameterTooltip (processor, parameter));
                syncing = false;
                repaint();
            }

            PlayerProcessor& processor;
            ParameterDef parameter;
            juce::Label nameLabel;
            juce::Label categoryLabel;
            juce::Label valueLabel;
            juce::Slider slider;
            juce::ToggleButton toggle;
            juce::ComboBox choiceBox;
            juce::TextButton bypassButton { "Bypass" };
            juce::TextButton resetButton { "Reset" };
            juce::TextButton learnButton { "Learn" };
            juce::StringArray choices;
            bool syncing = false;
            bool enabled = true;
            bool usesToggle = false;
            bool usesChoice = false;
        };

        void timerCallback() override
        {
            if (! isShowing())
                return;

            syncRows();
            updateStatus();
        }

        void rebuildSectionFilter()
        {
            const auto previousText = sectionBox.getText();
            sectionBox.clear (juce::dontSendNotification);
            sectionBox.addItem ("All", 1);

            juce::StringArray sectionNames;
            if (const auto* pack = processor.getPack())
            {
                for (const auto& parameter : pack->parameters.getAll())
                {
                    if (parameterBelongsToLoadedInstrument (*pack, parameter))
                        sectionNames.addIfNotAlreadyThere (cleanedSectionName (parameter.section));
                }
            }

            sectionNames.sort (true);
            for (int index = 0; index < sectionNames.size(); ++index)
                sectionBox.addItem (sectionNames[index], index + 2);

            const auto restoredIndex = sectionNames.indexOf (previousText);
            if (previousText == "All")
                sectionBox.setSelectedId (1, juce::dontSendNotification);
            else if (restoredIndex >= 0)
                sectionBox.setSelectedId (restoredIndex + 2, juce::dontSendNotification);
            else
                sectionBox.setSelectedId (sectionNames.contains ("Source") ? sectionNames.indexOf ("Source") + 2 : 1,
                                          juce::dontSendNotification);
        }

        void rebuildRows()
        {
            rows.clear();

            const auto selectedSection = sectionBox.getText();
            if (const auto* pack = processor.getPack())
            {
                std::vector<ParameterDef> parameters = pack->parameters.getAll();
                std::stable_sort (parameters.begin(), parameters.end(),
                    [] (const ParameterDef& first, const ParameterDef& second)
                    {
                        if (first.section != second.section)
                            return first.section < second.section;
                        if (first.category != second.category)
                            return first.category < second.category;
                        return first.name < second.name;
                    });

                for (const auto& parameter : parameters)
                {
                    if (! parameterBelongsToLoadedInstrument (*pack, parameter))
                        continue;
                    if (selectedSection != "All"
                        && cleanedSectionName (parameter.section) != selectedSection)
                        continue;
                    if (! parameter.hostAutomatable && ! parameter.midiLearnable && ! parameter.modulatable)
                        continue;

                    auto row = std::make_unique<ParameterRow> (processor, parameter);
                    rowContainer.addAndMakeVisible (*row);
                    rows.push_back (std::move (row));
                }
            }

            layoutRows();
        }

        void layoutRows()
        {
            const int rowHeight = 58;
            const int rowGap = 7;
            const int rowWidth = juce::jmax (240, viewport.getWidth() - 10);
            int top = 0;

            for (auto& row : rows)
            {
                row->setBounds (0, top, rowWidth, rowHeight);
                top += rowHeight + rowGap;
            }

            rowContainer.setSize (rowWidth, juce::jmax (viewport.getHeight(), top + 8));
        }

        void syncRows()
        {
            for (auto& row : rows)
                row->syncFromProcessor();
        }

        void updateStatus()
        {
            const auto* pack = processor.getPack();
            const auto instrumentName = pack != nullptr
                ? (pack->manifest.playerDisplayName.isNotEmpty()
                    ? pack->manifest.playerDisplayName
                    : pack->manifest.instrumentName)
                : juce::String ("No pack");

            juce::String status = instrumentName;
            if (pack != nullptr)
                status += "  |  " + cleanedSectionName (pack->manifest.engine)
                       + "  |  " + juce::String (processor.getActiveVoiceCount()) + " voices"
                       + "  |  " + juce::String (processor.getLoadedSampleCount()) + " samples";

            const auto diagnostic = processor.getEngineDiagnosticStatus();
            if (diagnostic.isNotEmpty())
                status += "  |  " + diagnostic;

            statusLabel.setText (status, juce::dontSendNotification);
        }

        PlayerProcessor& processor;
        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::Label statusLabel;
        juce::TextButton closeButton { "Close" };
        juce::TextButton floatButton { "Dock" };
        juce::TextButton randomizeButton { "Randomize" };
        juce::TextButton restoreButton { "Restore" };
        juce::TextButton defaultButton { "Set Default" };
        juce::ComboBox sectionBox;
        juce::Viewport viewport;
        juce::Component rowContainer;
        std::vector<std::unique_ptr<ParameterRow>> rows;
        bool floating = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerPerformancePanel)
    };

    class PlayerControlCenter : public juce::Component,
                                private juce::Timer
    {
    public:
        explicit PlayerControlCenter (PlayerProcessor& processorToUse)
            : processor (processorToUse)
        {
            closeButton.setButtonText ("Close");
            closeButton.onClick = [this]
            {
                if (onClose)
                    onClose();
            };
            addAndMakeVisible (closeButton);

            infoButton.setButtonText ("Info");
            rackButton.setButtonText ("Rack");
            mixButton.setButtonText ("Mix");
            routingButton.setButtonText ("Routing");
            midiButton.setButtonText ("MIDI");
            snapshotsButton.setButtonText ("Snapshots");
            dnaButton.setButtonText ("Sound DNA");
            for (auto* button : { &infoButton, &mixButton })
            {
                button->onClick = [this, button]
                {
                    if (button == &infoButton) currentTab = Tab::Info;
                    else if (button == &mixButton) currentTab = Tab::Mix;
                    updateTabState();
                    resized();
                    repaint();
                };
                addAndMakeVisible (*button);
            }

            libraryButton.setButtonText ("Open Library");
            libraryButton.setTooltip ("Open this instrument's library.");
            libraryButton.onClick = [this]
            {
                if (onOpenLibrary)
                    onOpenLibrary();
            };
            addAndMakeVisible (libraryButton);

            engineButton.setButtonText ("Sound Controls");
            engineButton.setTooltip ("Open the dedicated runtime control workspace.");
            engineButton.onClick = [this]
            {
                if (onOpenEngine)
                    onOpenEngine();
            };
            addAndMakeVisible (engineButton);

            clearMidiButton.setButtonText ("Cancel Learn");
            clearMidiButton.setTooltip ("Cancel the pending MIDI Learn capture.");
            clearMidiButton.onClick = [this] { processor.clearMidiLearn(); repaint(); };
            addAndMakeVisible (clearMidiButton);

            saveSnapshotButton.setButtonText ("Save Snapshot");
            recallSnapshotButton.setButtonText ("Recall");
            favoriteSnapshotButton.setButtonText ("Favorite");
            deleteSnapshotButton.setButtonText ("Delete");
            saveSnapshotButton.setTooltip ("Capture the current parameter state as a snapshot saved with this DAW session.");
            recallSnapshotButton.setTooltip ("Recall the selected user snapshot.");
            favoriteSnapshotButton.setTooltip ("Mark or unmark the selected snapshot as a favorite.");
            deleteSnapshotButton.setTooltip ("Delete the selected user snapshot from this DAW session.");
            saveSnapshotButton.onClick = [this]
            {
                auto name = processor.getPresetName (processor.getCurrentPresetIndex());
                if (name.isEmpty())
                    name = "Snapshot";
                name << " " << juce::String ((int) processor.getUserSnapshots().size() + 1);
                if (processor.saveUserSnapshot (name, false))
                {
                    selectedSnapshotIndex = (int) processor.getUserSnapshots().size() - 1;
                    repaint();
                }
            };
            recallSnapshotButton.onClick = [this]
            {
                const auto snapshots = processor.getUserSnapshots();
                if (juce::isPositiveAndBelow (selectedSnapshotIndex, (int) snapshots.size()))
                    processor.applyUserSnapshot (snapshots[(size_t) selectedSnapshotIndex].id);
                repaint();
            };
            favoriteSnapshotButton.onClick = [this]
            {
                const auto snapshots = processor.getUserSnapshots();
                if (juce::isPositiveAndBelow (selectedSnapshotIndex, (int) snapshots.size()))
                    processor.toggleUserSnapshotFavorite (snapshots[(size_t) selectedSnapshotIndex].id);
                repaint();
            };
            deleteSnapshotButton.onClick = [this]
            {
                const auto snapshots = processor.getUserSnapshots();
                if (juce::isPositiveAndBelow (selectedSnapshotIndex, (int) snapshots.size()))
                    processor.deleteUserSnapshot (snapshots[(size_t) selectedSnapshotIndex].id);
                selectedSnapshotIndex = juce::jlimit (0, juce::jmax (0, (int) processor.getUserSnapshots().size() - 1), selectedSnapshotIndex);
                repaint();
            };
            for (auto* button : { &saveSnapshotButton, &recallSnapshotButton, &favoriteSnapshotButton, &deleteSnapshotButton })
                addAndMakeVisible (*button);

            startTimerHz (4);
            updateTabState();
        }

        std::function<void()> onClose;
        std::function<void()> onOpenLibrary;
        std::function<void()> onOpenEngine;

        void rebuild()
        {
            rebuildRackRows();
            rebuildMixRows();
            repaint();
        }

        void showRack()
        {
            currentTab = Tab::Rack;
            rebuild();
            updateTabState();
            resized();
        }

        void showMix()
        {
            currentTab = Tab::Mix;
            rebuild();
            updateTabState();
            resized();
        }

        void showRouting()
        {
            currentTab = Tab::Routing;
            rebuild();
            updateTabState();
            resized();
        }

        void showMidi()
        {
            currentTab = Tab::Midi;
            rebuild();
            updateTabState();
            resized();
        }

        void showSnapshots()
        {
            currentTab = Tab::Snapshots;
            rebuild();
            updateTabState();
            resized();
        }

        void showSoundDna()
        {
            currentTab = Tab::SoundDna;
            rebuild();
            updateTabState();
            resized();
        }

        void paint (juce::Graphics& graphics) override
        {
            graphics.fillAll (juce::Colour (0xbb05070a));

            const auto modal = modalBounds().toFloat();
            graphics.setColour (juce::Colour (0xf40d1016));
            graphics.fillRoundedRectangle (modal, 16.0f);
            graphics.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.90f));
            graphics.drawRoundedRectangle (modal.reduced (0.5f), 16.0f, 1.7f);

            auto header = modalBounds().reduced (18).removeFromTop (54);
            graphics.setColour (PatchCraftLookAndFeel::textBright());
            graphics.setFont (juce::FontOptions (20.0f).withStyle ("bold"));
            graphics.drawText (tabTitle(), header.removeFromTop (26),
                               juce::Justification::centredLeft, true);

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText (tabSubtitle(),
                               header, juce::Justification::centredLeft, true);

            auto content = contentBounds();
            graphics.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.50f));
            graphics.fillRoundedRectangle (content.toFloat(), 10.0f);
            graphics.setColour (PatchCraftLookAndFeel::borderSoft());
            graphics.drawRoundedRectangle (content.toFloat().reduced (0.5f), 10.0f, 1.0f);

            switch (currentTab)
            {
                case Tab::Info:    drawInfo (graphics, content.reduced (16)); break;
                case Tab::Rack:    drawRack (graphics, content.reduced (16)); break;
                case Tab::Mix:     drawMix (graphics, content.reduced (16)); break;
                case Tab::Routing: drawRouting (graphics, content.reduced (16)); break;
                case Tab::Midi:    drawMidi (graphics, content.reduced (16)); break;
                case Tab::Snapshots: drawSnapshots (graphics, content.reduced (16)); break;
                case Tab::SoundDna:  drawSoundDna (graphics, content.reduced (16)); break;
            }
        }

        void resized() override
        {
            const auto modal = modalBounds();
            auto area = modal.reduced (18);
            auto top = area.removeFromTop (34);
            closeButton.setBounds (top.removeFromRight (74));
            area.removeFromTop (34);

            auto tabs = area.removeFromTop (34);
            infoButton.setBounds (tabs.removeFromLeft (78).reduced (2));
            mixButton.setBounds (tabs.removeFromLeft (78).reduced (2));

            auto actions = area.removeFromBottom (34);
            libraryButton.setBounds (actions.removeFromLeft (120).reduced (2));
            engineButton.setBounds (actions.removeFromLeft (148).reduced (2));

            layoutRackRows();
            layoutMixRows();
            updateChildVisibility();
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (currentTab == Tab::Snapshots)
            {
                for (int index = 0; index < (int) snapshotRowBounds.size(); ++index)
                {
                    if (snapshotRowBounds[(size_t) index].contains (event.getPosition()))
                    {
                        selectedSnapshotIndex = index;
                        repaint();
                        return;
                    }
                }
            }

            if (! modalBounds().contains (event.getPosition()) && onClose)
                onClose();
        }

    private:
        enum class Tab
        {
            Info,
            Rack,
            Mix,
            Routing,
            Midi,
            Snapshots,
            SoundDna
        };

        struct MixRow : public juce::Component
        {
            MixRow (PlayerProcessor& processorToUse, int layerIndexToUse, juce::String labelToUse)
                : processor (processorToUse), layerIndex (layerIndexToUse), label (std::move (labelToUse))
            {
                nameLabel.setText (label, juce::dontSendNotification);
                nameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
                nameLabel.setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (nameLabel);

                for (auto* slider : { &volumeSlider, &panSlider })
                {
                    slider->setSliderStyle (juce::Slider::LinearHorizontal);
                    slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
                    addAndMakeVisible (*slider);
                }

                volumeSlider.setRange (0.0, layerIndex >= 0 ? 1.0 : 1.5, 0.001);
                panSlider.setRange (-1.0, 1.0, 0.001);
                volumeSlider.setDoubleClickReturnValue (true, layerIndex >= 0 ? 1.0 : 0.85);
                panSlider.setDoubleClickReturnValue (true, 0.0);

                volumeSlider.onValueChange = [this]
                {
                    if (syncing) return;
                    if (layerIndex >= 0)
                        processor.setMultiLayerVolume (layerIndex, (float) volumeSlider.getValue());
                    else
                        processor.setPackParameterFromUi ("volume", (float) volumeSlider.getValue());
                };

                panSlider.onValueChange = [this]
                {
                    if (syncing) return;
                    if (layerIndex >= 0)
                        processor.setMultiLayerPan (layerIndex, (float) panSlider.getValue());
                    else
                        processor.setPackParameterFromUi ("pan", (float) panSlider.getValue());
                };

                muteButton.setButtonText ("M");
                soloButton.setButtonText ("S");
                muteButton.setTooltip ("Mute this layer.");
                soloButton.setTooltip ("Solo this layer.");
                muteButton.onClick = [this]
                {
                    if (layerIndex >= 0)
                        processor.setMultiLayerMuted (layerIndex, ! processor.getMultiLayerMuted (layerIndex));
                    syncFromProcessor();
                };
                soloButton.onClick = [this]
                {
                    if (layerIndex >= 0)
                        processor.setMultiLayerSoloed (layerIndex, ! processor.getMultiLayerSoloed (layerIndex));
                    syncFromProcessor();
                };
                addAndMakeVisible (muteButton);
                addAndMakeVisible (soloButton);
                syncFromProcessor();
            }

            void paint (juce::Graphics& graphics) override
            {
                const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
                graphics.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.72f));
                graphics.fillRoundedRectangle (bounds, 8.0f);
                graphics.setColour (PatchCraftLookAndFeel::borderSoft());
                graphics.drawRoundedRectangle (bounds, 8.0f, 1.0f);
            }

            void resized() override
            {
                auto row = getLocalBounds().reduced (10, 6);
                nameLabel.setBounds (row.removeFromLeft (150));
                if (layerIndex >= 0)
                {
                    soloButton.setBounds (row.removeFromRight (30).reduced (2));
                    muteButton.setBounds (row.removeFromRight (30).reduced (2));
                    row.removeFromRight (8);
                }
                else
                {
                    muteButton.setVisible (false);
                    soloButton.setVisible (false);
                }

                auto volume = row.removeFromLeft (juce::jmax (120, row.getWidth() / 2 - 8));
                volumeSlider.setBounds (volume.reduced (4, 3));
                panSlider.setBounds (row.reduced (4, 3));
            }

            void syncFromProcessor()
            {
                syncing = true;
                if (layerIndex >= 0)
                {
                    volumeSlider.setValue (processor.getMultiLayerVolume (layerIndex), juce::dontSendNotification);
                    panSlider.setValue (processor.getMultiLayerPan (layerIndex), juce::dontSendNotification);
                    muteButton.setToggleState (processor.getMultiLayerMuted (layerIndex), juce::dontSendNotification);
                    soloButton.setToggleState (processor.getMultiLayerSoloed (layerIndex), juce::dontSendNotification);
                }
                else
                {
                    volumeSlider.setValue (processor.getPackParameterValue ("volume"), juce::dontSendNotification);
                    panSlider.setValue (processor.getPackParameterValue ("pan"), juce::dontSendNotification);
                    const auto* pack = processor.getPack();
                    volumeSlider.setEnabled (pack != nullptr && pack->parameters.find ("volume") != nullptr);
                    panSlider.setEnabled (pack != nullptr && pack->parameters.find ("pan") != nullptr);
                }
                syncing = false;
                repaint();
            }

            PlayerProcessor& processor;
            int layerIndex = -1;
            juce::String label;
            juce::Label nameLabel;
            juce::Slider volumeSlider;
            juce::Slider panSlider;
            juce::TextButton muteButton { "M" };
            juce::TextButton soloButton { "S" };
            bool syncing = false;
        };

        struct RackRow : public juce::Component
        {
            RackRow (PlayerProcessor& processorToUse, int layerIndexToUse)
                : processor (processorToUse), layerIndex (layerIndexToUse)
            {
                nameLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
                nameLabel.setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (nameLabel);

                onButton.setButtonText ("On");
                onButton.setClickingTogglesState (true);
                onButton.setTooltip ("Enable or disable this instrument layer without removing it.");
                onButton.onClick = [this]
                {
                    if (syncing) return;
                    processor.setMultiLayerEnabled (layerIndex, onButton.getToggleState());
                    syncFromProcessor();
                };
                addAndMakeVisible (onButton);

                muteButton.setButtonText ("M");
                soloButton.setButtonText ("S");
                muteButton.setTooltip ("Mute this instrument layer.");
                soloButton.setTooltip ("Solo this instrument layer.");
                muteButton.onClick = [this]
                {
                    if (syncing) return;
                    processor.setMultiLayerMuted (layerIndex, ! processor.getMultiLayerMuted (layerIndex));
                    syncFromProcessor();
                };
                soloButton.onClick = [this]
                {
                    if (syncing) return;
                    processor.setMultiLayerSoloed (layerIndex, ! processor.getMultiLayerSoloed (layerIndex));
                    syncFromProcessor();
                };
                addAndMakeVisible (muteButton);
                addAndMakeVisible (soloButton);

                midiBox.addItem ("Omni", 1);
                for (int channel = 1; channel <= 16; ++channel)
                    midiBox.addItem ("Ch " + juce::String (channel), channel + 1);
                midiBox.setTooltip ("Choose which MIDI channel triggers this instrument. Omni plays from any channel.");
                midiBox.onChange = [this]
                {
                    if (syncing) return;
                    processor.setMultiLayerMidiChannel (layerIndex, juce::jmax (0, midiBox.getSelectedId() - 1));
                };
                addAndMakeVisible (midiBox);

                routeBox.addItem ("Main", 1);
                for (int route = 1; route <= 4; ++route)
                    routeBox.addItem ("Aux " + juce::String (route), route + 1);
                routeBox.setTooltip ("Assign this layer to Main or an optional stereo Aux output. If the host has not enabled aux buses, the layer falls back to Main.");
                routeBox.onChange = [this]
                {
                    if (syncing) return;
                    processor.setMultiLayerOutputRoute (layerIndex, juce::jmax (0, routeBox.getSelectedId() - 1));
                };
                addAndMakeVisible (routeBox);

                for (auto* slider : { &volumeSlider, &panSlider, &tuneSlider })
                {
                    slider->setSliderStyle (juce::Slider::LinearHorizontal);
                    slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
                    addAndMakeVisible (*slider);
                }

                volumeSlider.setRange (0.0, 1.5, 0.001);
                volumeSlider.setDoubleClickReturnValue (true, 1.0);
                volumeSlider.setTooltip ("Layer volume.");
                volumeSlider.onValueChange = [this]
                {
                    if (! syncing)
                        processor.setMultiLayerVolume (layerIndex, (float) volumeSlider.getValue());
                };

                panSlider.setRange (-1.0, 1.0, 0.001);
                panSlider.setDoubleClickReturnValue (true, 0.0);
                panSlider.setTooltip ("Layer stereo pan.");
                panSlider.onValueChange = [this]
                {
                    if (! syncing)
                        processor.setMultiLayerPan (layerIndex, (float) panSlider.getValue());
                };

                tuneSlider.setRange (-24.0, 24.0, 1.0);
                tuneSlider.setDoubleClickReturnValue (true, 0.0);
                tuneSlider.setTooltip ("Transpose this layer before it reaches the sound engine.");
                tuneSlider.onValueChange = [this]
                {
                    if (! syncing)
                        processor.setMultiLayerTransposeSemitones (layerIndex, juce::roundToInt (tuneSlider.getValue()));
                };

                syncFromProcessor();
            }

            void paint (juce::Graphics& graphics) override
            {
                const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
                graphics.setColour (PatchCraftLookAndFeel::panel().withAlpha (processor.getMultiLayerEnabled (layerIndex) ? 0.78f : 0.42f));
                graphics.fillRoundedRectangle (bounds, 8.0f);
                graphics.setColour (PatchCraftLookAndFeel::borderSoft());
                graphics.drawRoundedRectangle (bounds, 8.0f, 1.0f);
            }

            void resized() override
            {
                auto row = getLocalBounds().reduced (10, 6);
                nameLabel.setBounds (row.removeFromLeft (132));
                row.removeFromLeft (6);
                onButton.setBounds (row.removeFromLeft (42).reduced (1, 5));
                muteButton.setBounds (row.removeFromLeft (30).reduced (2, 5));
                soloButton.setBounds (row.removeFromLeft (30).reduced (2, 5));
                row.removeFromLeft (6);
                midiBox.setBounds (row.removeFromLeft (72).reduced (2, 5));
                routeBox.setBounds (row.removeFromLeft (84).reduced (2, 5));
                tuneSlider.setBounds (row.removeFromLeft (112).reduced (4, 5));
                volumeSlider.setBounds (row.removeFromLeft (juce::jmax (120, row.getWidth() / 2)).reduced (4, 5));
                panSlider.setBounds (row.reduced (4, 5));
            }

            void syncFromProcessor()
            {
                syncing = true;
                nameLabel.setText (processor.getMultiLayerName (layerIndex), juce::dontSendNotification);
                onButton.setToggleState (processor.getMultiLayerEnabled (layerIndex), juce::dontSendNotification);
                muteButton.setToggleState (processor.getMultiLayerMuted (layerIndex), juce::dontSendNotification);
                soloButton.setToggleState (processor.getMultiLayerSoloed (layerIndex), juce::dontSendNotification);
                midiBox.setSelectedId (processor.getMultiLayerMidiChannel (layerIndex) + 1, juce::dontSendNotification);
                routeBox.setSelectedId (processor.getMultiLayerOutputRoute (layerIndex) + 1, juce::dontSendNotification);
                volumeSlider.setValue (processor.getMultiLayerVolume (layerIndex), juce::dontSendNotification);
                panSlider.setValue (processor.getMultiLayerPan (layerIndex), juce::dontSendNotification);
                tuneSlider.setValue (processor.getMultiLayerTransposeSemitones (layerIndex), juce::dontSendNotification);
                syncing = false;
                repaint();
            }

            PlayerProcessor& processor;
            int layerIndex = -1;
            juce::Label nameLabel;
            juce::TextButton onButton { "On" };
            juce::TextButton muteButton { "M" };
            juce::TextButton soloButton { "S" };
            juce::ComboBox midiBox;
            juce::ComboBox routeBox;
            juce::Slider volumeSlider;
            juce::Slider panSlider;
            juce::Slider tuneSlider;
            bool syncing = false;
        };

        void timerCallback() override
        {
            if (! isShowing())
                return;

            for (auto& row : rackRows)
                row->syncFromProcessor();
            for (auto& row : mixRows)
                row->syncFromProcessor();
            repaint();
        }

        juce::Rectangle<int> modalBounds() const
        {
            auto bounds = getLocalBounds().reduced (24);
            const int width = juce::jmin (820, bounds.getWidth());
            const int height = juce::jmin (560, bounds.getHeight());
            return juce::Rectangle<int> (width, height).withCentre (bounds.getCentre());
        }

        juce::Rectangle<int> contentBounds() const
        {
            auto bounds = modalBounds().reduced (18);
            bounds.removeFromTop (92);
            bounds.removeFromBottom (44);
            return bounds;
        }

        void updateTabState()
        {
            infoButton.getProperties().set ("accent", currentTab == Tab::Info);
            rackButton.getProperties().set ("accent", currentTab == Tab::Rack);
            mixButton.getProperties().set ("accent", currentTab == Tab::Mix);
            routingButton.getProperties().set ("accent", currentTab == Tab::Routing);
            midiButton.getProperties().set ("accent", currentTab == Tab::Midi);
            snapshotsButton.getProperties().set ("accent", currentTab == Tab::Snapshots);
            dnaButton.getProperties().set ("accent", currentTab == Tab::SoundDna);
            updateChildVisibility();
        }

        juce::String tabTitle() const
        {
            switch (currentTab)
            {
                case Tab::Info:    return "Player Control Center";
                case Tab::Rack:    return "Instrument Rack";
                case Tab::Mix:     return "Player Mixer";
                case Tab::Routing: return "Routing Matrix";
                case Tab::Midi:    return "MIDI Control";
                case Tab::Snapshots: return "Snapshots + Favorites";
                case Tab::SoundDna:  return "Sound DNA";
            }
            return "Player Control Center";
        }

        juce::String tabSubtitle() const
        {
            switch (currentTab)
            {
                case Tab::Info:    return "Instrument identity, license status, and support details.";
                case Tab::Rack:    return "Layer stack, MIDI channel splits, output routes, tuning, mute, solo, and bypass.";
                case Tab::Mix:     return "Global and per-instrument mixing controls that affect the loaded patch.";
                case Tab::Routing: return "Audio/MIDI routing for this plugin.";
                case Tab::Midi:    return "MIDI learn, hardware mappings, and performance inputs.";
                case Tab::Snapshots: return "Save favorite sound states and recall them without changing factory presets.";
                case Tab::SoundDna:  return "Readable signal-chain formula: blocks, modulation, samples, mappings, and live parameter values.";
            }
            return {};
        }

        void updateChildVisibility()
        {
            const bool showRackRows = currentTab == Tab::Rack;
            for (auto& row : rackRows)
                row->setVisible (showRackRows);

            const bool showMixRows = currentTab == Tab::Mix;
            for (auto& row : mixRows)
                row->setVisible (showMixRows);

            libraryButton.setVisible (currentTab == Tab::Info || currentTab == Tab::Routing);
            engineButton.setVisible (currentTab == Tab::Info || currentTab == Tab::Routing || currentTab == Tab::Mix);
            clearMidiButton.setVisible (currentTab == Tab::Midi);
            const bool showSnapshotActions = currentTab == Tab::Snapshots;
            saveSnapshotButton.setVisible (showSnapshotActions);
            recallSnapshotButton.setVisible (showSnapshotActions);
            favoriteSnapshotButton.setVisible (showSnapshotActions);
            deleteSnapshotButton.setVisible (showSnapshotActions);
        }

        void rebuildRackRows()
        {
            rackRows.clear();
            if (processor.isMultiInstrumentPack())
            {
                const int layerCount = processor.getMultiLayerCount();
                for (int index = 0; index < layerCount; ++index)
                {
                    auto row = std::make_unique<RackRow> (processor, index);
                    addAndMakeVisible (*row);
                    rackRows.push_back (std::move (row));
                }
            }
            layoutRackRows();
            updateChildVisibility();
        }

        void rebuildMixRows()
        {
            mixRows.clear();
            const int layerCount = processor.getMultiLayerCount();
            if (processor.isMultiInstrumentPack() && layerCount > 1)
            {
                auto globalRow = std::make_unique<MixRow> (processor, -1, "Global Output");
                addAndMakeVisible (*globalRow);
                mixRows.push_back (std::move (globalRow));
                for (int index = 0; index < layerCount; ++index)
                {
                    auto row = std::make_unique<MixRow> (processor, index, processor.getMultiLayerName (index));
                    addAndMakeVisible (*row);
                    mixRows.push_back (std::move (row));
                }
            }
            else
            {
                auto row = std::make_unique<MixRow> (processor, -1, "Main Output");
                addAndMakeVisible (*row);
                mixRows.push_back (std::move (row));
            }
            layoutMixRows();
            updateChildVisibility();
        }

        void layoutRackRows()
        {
            auto area = contentBounds().reduced (16);
            area.removeFromTop (76);
            for (auto& row : rackRows)
            {
                row->setBounds (area.removeFromTop (56).reduced (0, 3));
                area.removeFromTop (4);
            }
        }

        void layoutMixRows()
        {
            auto area = contentBounds().reduced (16);
            area.removeFromTop (54);
            for (auto& row : mixRows)
            {
                row->setBounds (area.removeFromTop (46).reduced (0, 3));
                area.removeFromTop (4);
            }
        }

        void drawField (juce::Graphics& graphics, juce::Rectangle<int>& area,
                        juce::String label, juce::String value) const
        {
            auto row = area.removeFromTop (28);
            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
            graphics.drawText (label, row.removeFromLeft (150), juce::Justification::centredLeft, true);
            graphics.setColour (PatchCraftLookAndFeel::text());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText (value, row, juce::Justification::centredLeft, true);
        }

        void drawInfo (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            const auto* pack = processor.getPack();
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("Instrument", area.removeFromTop (26), juce::Justification::centredLeft, true);

            drawField (graphics, area, "Name", playerInstrumentName (pack));
            drawField (graphics, area, "Creator", pack != nullptr ? pack->manifest.creator : "-");
            drawField (graphics, area, "Client", pack != nullptr && pack->manifest.playerClientName.isNotEmpty()
                ? pack->manifest.playerClientName : "-");
            drawField (graphics, area, "Engine", pack != nullptr ? cleanedSectionName (pack->manifest.engine) : "-");
            drawField (graphics, area, "Preset", processor.getCurrentPresetIndex() >= 0
                ? processor.getPresetName (processor.getCurrentPresetIndex()) : "-");
            drawField (graphics, area, "Presets", juce::String (processor.getPresetCount()));
            drawField (graphics, area, "Runtime", juce::String (processor.getActiveVoiceCount()) + " voices, "
                + juce::String (processor.getLoadedSampleCount()) + " samples");
            drawField (graphics, area, "License", pack != nullptr && pack->manifest.licenseRequired
                ? "Required" + (pack->manifest.trialDays > 0 ? " / " + juce::String (pack->manifest.trialDays) + " day trial" : juce::String())
                : "Not required");
            drawField (graphics, area, "Support", pack != nullptr && pack->manifest.playerSupportUrl.isNotEmpty()
                ? pack->manifest.playerSupportUrl
                : (pack != nullptr && pack->manifest.playerSupportEmail.isNotEmpty()
                    ? pack->manifest.playerSupportEmail
                    : "-"));
        }

        void drawRack (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("Instrument Rack", area.removeFromTop (26), juce::Justification::centredLeft, true);

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText ("Stack instruments, split by MIDI channel, choose internal routes, tune layers, and mix them together.",
                               area.removeFromTop (24), juce::Justification::centredLeft, true);

            if (! processor.isMultiInstrumentPack())
            {
                area.removeFromTop (12);
                graphics.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.72f));
                graphics.fillRoundedRectangle (area.removeFromTop (74).toFloat(), 8.0f);
                graphics.setColour (PatchCraftLookAndFeel::text());
                graphics.setFont (juce::FontOptions (12.0f));
                graphics.drawText ("This patch is a single instrument. Load or export a multi-instrument pack to use layer toggles, channel splits, and per-instrument routing.",
                                   area.translated (12, -68).withTrimmedRight (24),
                                   juce::Justification::centredLeft, true);
                return;
            }

            auto columns = area.removeFromTop (22);
            graphics.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.85f));
            graphics.setFont (juce::FontOptions (10.0f).withStyle ("bold"));
            graphics.drawText ("Layer", columns.removeFromLeft (138), juce::Justification::centredLeft, true);
            columns.removeFromLeft (6 + 42 + 30 + 30 + 6);
            graphics.drawText ("MIDI", columns.removeFromLeft (72), juce::Justification::centred, true);
            graphics.drawText ("Route", columns.removeFromLeft (84), juce::Justification::centred, true);
            graphics.drawText ("Tune", columns.removeFromLeft (112), juce::Justification::centred, true);
            graphics.drawText ("Volume", columns.removeFromLeft (juce::jmax (120, columns.getWidth() / 2)), juce::Justification::centred, true);
            graphics.drawText ("Pan", columns, juce::Justification::centred, true);
        }

        void drawMix (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("Mixer", area.removeFromTop (26), juce::Justification::centredLeft, true);
            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText ("Mix layers, master volume/pan, and mapped instrument buses without leaving the Player.",
                               area.removeFromTop (24), juce::Justification::centredLeft, true);
            graphics.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.80f));
            graphics.setFont (juce::FontOptions (10.5f));
            graphics.drawText ("Rows below are live controls. Drag sliders while notes or host audio are playing.",
                               area.removeFromTop (20), juce::Justification::centredLeft, true);
        }

        void drawRouting (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            const auto* pack = processor.getPack();
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("Routing", area.removeFromTop (26), juce::Justification::centredLeft, true);
            drawField (graphics, area, "Audio Path", pack != nullptr && pack->manifest.engine.equalsIgnoreCase ("fx")
                ? "Host Input -> FX -> Host Output"
                : "MIDI -> Instrument -> Host Output");
            drawField (graphics, area, "Main Output", "Stereo host bus");
            drawField (graphics, area, "Layer Routes", "Main + optional Aux 1-4 stereo buses");
            drawField (graphics, area, "Automation", juce::String (kPatchCraftHostParameterSlots) + " exported host slots");
            drawField (graphics, area, "Layers", processor.isMultiInstrumentPack()
                ? juce::String (processor.getMultiLayerCount()) + " synced instruments"
                : "Single instrument");
            drawField (graphics, area, "Diagnostics", processor.getEngineDiagnosticStatus().upToFirstOccurrenceOf ("\n", false, false));
           #if PATCHCRAFT_PLAYER_FX
            drawField (graphics, area, "FX Insert", pack != nullptr && pack->manifest.engine.equalsIgnoreCase ("fx")
                ? "Processing incoming host audio"
                : "Instrument pack loaded; host audio passes through, sound controls respond to MIDI");
           #endif
        }

        void drawMidi (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("MIDI", area.removeFromTop (26), juce::Justification::centredLeft, true);
            drawField (graphics, area, "Learn State", processor.getPendingMidiLearnParameter().isNotEmpty()
                ? "Waiting for hardware control: " + processor.getPendingMidiLearnParameter()
                : "Idle");
            drawField (graphics, area, "Learn Workflow", "Right-click a control, choose MIDI Learn, move hardware.");
            drawField (graphics, area, "Performance", "Mod wheel, expression, pitch bend, sustain, CC mappings.");
            drawField (graphics, area, "Mappings", "Saved in the host plugin state.");
        }

        void drawSnapshots (juce::Graphics& graphics, juce::Rectangle<int> area)
        {
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("User Snapshots", area.removeFromTop (26), juce::Justification::centredLeft, true);

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText ("Capture live tweaks as snapshots. Favorites appear first and survive DAW session reloads.",
                               area.removeFromTop (24), juce::Justification::centredLeft, true);

            snapshotRowBounds.clear();
            const auto snapshots = processor.getUserSnapshots();
            if (snapshots.empty())
            {
                area.removeFromTop (12);
                graphics.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.72f));
                graphics.fillRoundedRectangle (area.removeFromTop (92).toFloat(), 10.0f);
                graphics.setColour (PatchCraftLookAndFeel::text());
                graphics.setFont (juce::FontOptions (12.0f));
                graphics.drawText ("No user snapshots yet. Click Save Snapshot after moving macros, mixer, or sound controls.",
                                   area.translated (14, -84).withTrimmedRight (24),
                                   juce::Justification::centredLeft, true);
                return;
            }

            selectedSnapshotIndex = juce::jlimit (0, (int) snapshots.size() - 1, selectedSnapshotIndex);
            area.removeFromTop (8);
            for (int index = 0; index < (int) snapshots.size() && area.getHeight() > 34; ++index)
            {
                const auto row = area.removeFromTop (38).reduced (0, 2);
                snapshotRowBounds.push_back (row);
                const bool selected = index == selectedSnapshotIndex;
                const auto& snapshot = snapshots[(size_t) index];
                graphics.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.22f)
                                             : PatchCraftLookAndFeel::panel().withAlpha (0.70f));
                graphics.fillRoundedRectangle (row.toFloat(), 8.0f);
                graphics.setColour (selected ? PatchCraftLookAndFeel::accent()
                                             : PatchCraftLookAndFeel::borderSoft());
                graphics.drawRoundedRectangle (row.toFloat().reduced (0.5f), 8.0f, selected ? 1.7f : 1.0f);

                auto text = row.reduced (12, 3);
                graphics.setColour (snapshot.favorite ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textBright());
                graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                graphics.drawText ((snapshot.favorite ? "★ " : "") + snapshot.name,
                                   text.removeFromLeft (juce::jmax (180, text.getWidth() / 2)),
                                   juce::Justification::centredLeft, true);
                graphics.setColour (PatchCraftLookAndFeel::textDim());
                graphics.setFont (juce::FontOptions (10.5f));
                graphics.drawText (juce::String (snapshot.parameterCount) + " parameters"
                                   + (snapshot.sourcePresetIndex >= 0 ? "  •  from factory preset" : juce::String()),
                                   text, juce::Justification::centredLeft, true);
            }
        }

        void drawSoundDna (juce::Graphics& graphics, juce::Rectangle<int> area) const
        {
            const auto* pack = processor.getPack();
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (14.0f).withStyle ("bold"));
            graphics.drawText ("Sound DNA", area.removeFromTop (26), juce::Justification::centredLeft, true);

            if (pack == nullptr)
                return;

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawText ("A readable formula for what is making sound and what is safe to perform live.",
                               area.removeFromTop (24), juce::Justification::centredLeft, true);
            area.removeFromTop (8);

            std::map<juce::String, int> sectionCounts;
            for (const auto& block : pack->dspGraph.blocks)
                if (block.enabled)
                    ++sectionCounts[block.section.toLowerCase()];

            auto card = [&] (juce::String title, juce::String value, juce::Colour colour)
            {
                auto box = area.removeFromLeft (juce::jmax (110, area.getWidth() / 6)).reduced (3);
                graphics.setColour (colour.withAlpha (0.16f));
                graphics.fillRoundedRectangle (box.toFloat(), 9.0f);
                graphics.setColour (colour.withAlpha (0.82f));
                graphics.drawRoundedRectangle (box.toFloat().reduced (0.5f), 9.0f, 1.2f);
                graphics.setColour (PatchCraftLookAndFeel::textDim());
                graphics.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
                graphics.drawText (title.toUpperCase(), box.reduced (8, 4).removeFromTop (14), juce::Justification::centredLeft, true);
                graphics.setColour (PatchCraftLookAndFeel::textBright());
                graphics.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
                graphics.drawText (value, box.reduced (8, 4), juce::Justification::centredLeft, true);
            };

            auto cards = area.removeFromTop (58);
            auto oldArea = area;
            area = cards;
            card ("Source", juce::String (sectionCounts["source"]), PatchCraftLookAndFeel::accent());
            card ("Filter", juce::String (sectionCounts["filter"]), juce::Colour (0xff64d88a));
            card ("Amp", juce::String (sectionCounts["amp"]), juce::Colour (0xff7aa6ff));
            card ("Mod", juce::String (sectionCounts["mod"]), juce::Colour (0xffb678ff));
            card ("FX", juce::String (sectionCounts["fx"]), juce::Colour (0xffff6f61));
            card ("Out", juce::String (sectionCounts["out"]), juce::Colour (0xfff5d36c));
            area = oldArea;
            area.removeFromTop (10);

            auto left = area.removeFromLeft (area.getWidth() / 2).reduced (0, 0);
            auto right = area.reduced (12, 0);

            graphics.setColour (PatchCraftLookAndFeel::textBright());
            graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
            graphics.drawText ("Signal Formula", left.removeFromTop (22), juce::Justification::centredLeft, true);
            graphics.drawText ("Live Parameters", right.removeFromTop (22), juce::Justification::centredLeft, true);

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (10.5f));
            const juce::String formula = juce::String (pack->dspGraph.blocks.size()) + " blocks  •  "
                + juce::String (pack->dspGraph.modulation.size()) + " mod routes  •  "
                + juce::String (pack->dspGraph.macros.size()) + " macros  •  "
                + juce::String (pack->dspGraph.automation.size()) + " automations  •  "
                + juce::String ((int) pack->sampleMap.getZones().size()) + " samples";
            graphics.drawText (formula, left.removeFromTop (22), juce::Justification::centredLeft, true);

            int blockRows = 0;
            for (const auto& block : pack->dspGraph.blocks)
            {
                if (blockRows++ >= 8 || left.getHeight() < 22)
                    break;
                const auto label = (block.name.isNotEmpty() ? block.name : block.type)
                    + "  [" + block.section.toUpperCase() + "]";
                graphics.drawText ((block.enabled ? "● " : "○ ") + label,
                                   left.removeFromTop (20), juce::Justification::centredLeft, true);
            }

            int paramRows = 0;
            for (const auto& def : pack->parameters.getAll())
            {
                if (! def.visible || paramRows++ >= 10 || right.getHeight() < 20)
                    continue;
                const float value = processor.getPackParameterValue (def.id);
                graphics.drawText (def.name + "  =  " + juce::String (value, 2) + (def.unit.isNotEmpty() ? " " + def.unit : juce::String()),
                                   right.removeFromTop (20), juce::Justification::centredLeft, true);
            }
        }

        PlayerProcessor& processor;
        Tab currentTab = Tab::Rack;
        juce::TextButton closeButton { "Close" };
        juce::TextButton infoButton { "Info" };
        juce::TextButton rackButton { "Rack" };
        juce::TextButton mixButton { "Mix" };
        juce::TextButton routingButton { "Routing" };
        juce::TextButton midiButton { "MIDI" };
        juce::TextButton snapshotsButton { "Snapshots" };
        juce::TextButton dnaButton { "Sound DNA" };
        juce::TextButton libraryButton { "Open Library" };
        juce::TextButton engineButton { "Open Engine" };
        juce::TextButton clearMidiButton { "Cancel Learn" };
        juce::TextButton saveSnapshotButton { "Save Snapshot" };
        juce::TextButton recallSnapshotButton { "Recall" };
        juce::TextButton favoriteSnapshotButton { "Favorite" };
        juce::TextButton deleteSnapshotButton { "Delete" };
        std::vector<std::unique_ptr<RackRow>> rackRows;
        std::vector<std::unique_ptr<MixRow>> mixRows;
        std::vector<juce::Rectangle<int>> snapshotRowBounds;
        int selectedSnapshotIndex = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerControlCenter)
    };

    class PlayerUserImportPanel : public juce::Component,
                                  public juce::FileDragAndDropTarget,
                                  private juce::ListBoxModel,
                                  private juce::Timer
    {
    public:
        explicit PlayerUserImportPanel (PlayerProcessor& processorToUse)
            : processor (processorToUse), keyboard (processorToUse)
        {
            title.setText ("Samples + MIDI", juce::dontSendNotification);
            title.setJustificationType (juce::Justification::centredLeft);
            title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
            addAndMakeVisible (title);

            subtitle.setText ("Add samples or MIDI loops to this instrument session.",
                              juce::dontSendNotification);
            subtitle.setJustificationType (juce::Justification::centredLeft);
            subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            addAndMakeVisible (subtitle);

            status.setJustificationType (juce::Justification::centredLeft);
            status.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
            addAndMakeVisible (status);

            closeButton.setButtonText ("Close");
            closeButton.setTooltip ("Close the sample and MIDI library.");
            closeButton.onClick = [this]
            {
                if (onClose)
                    onClose();
            };
            addAndMakeVisible (closeButton);

            sampleMode.addItem ("Pads / one-shots", 1);
            sampleMode.addItem ("Keyboard / pitched", 2);
            sampleMode.setSelectedId (1, juce::dontSendNotification);
            sampleMode.setTooltip ("Choose how imported samples are mapped: one-shot pads or a playable pitched keyboard zone.");
            addAndMakeVisible (sampleMode);

            midiImportMode.addItem ("Replace piano roll", 1);
            midiImportMode.addItem ("Stack on piano roll", 2);
            midiImportMode.setSelectedId (1, juce::dontSendNotification);
            midiImportMode.setTooltip ("Replace clears the current piano-roll pattern. Stack keeps existing notes and adds the imported MIDI.");
            addAndMakeVisible (midiImportMode);

            importSamplesButton.setButtonText ("Import Samples");
            importSamplesButton.setTooltip ("Import WAV, AIFF, or FLAC files into this instrument session.");
            importSamplesButton.onClick = [this] { chooseFiles (false); };
            addAndMakeVisible (importSamplesButton);

            importMidiButton.setButtonText ("Import MIDI");
            importMidiButton.setTooltip ("Import MIDI files and convert them into a playable pattern.");
            importMidiButton.onClick = [this] { chooseFiles (true); };
            addAndMakeVisible (importMidiButton);

            auditionButton.setButtonText ("Audition");
            auditionButton.setTooltip ("Play the selected imported sample from its mapped note.");
            auditionButton.onClick = [this] { auditionSelected(); };
            addAndMakeVisible (auditionButton);

            applyMidiButton.setButtonText ("Apply MIDI");
            applyMidiButton.setTooltip ("Apply the selected MIDI file to the playable pattern.");
            applyMidiButton.onClick = [this] { applySelectedMidi(); };
            addAndMakeVisible (applyMidiButton);

            dragMidiButton.setButtonText ("Drag MIDI Out");
            dragMidiButton.setTooltip ("Drag the selected MIDI loop preset out to a DAW or folder.");
            dragMidiButton.onDrag = [this] (const juce::MouseEvent&) { dragSelectedMidiOut(); };
            addAndMakeVisible (dragMidiButton);

            tuneLabel.setText ("Sample tune", juce::dontSendNotification);
            tuneLabel.setJustificationType (juce::Justification::centredLeft);
            tuneLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            addAndMakeVisible (tuneLabel);

            tuneSlider.setRange (-12.0, 12.0, 1.0);
            tuneSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
            tuneSlider.setDoubleClickReturnValue (true, 0.0);
            tuneSlider.setTooltip ("Tune the selected imported sample by up to one octave of semitones.");
            tuneSlider.onValueChange = [this]
            {
                if (updatingTuneSlider)
                    return;

                const auto selected = getSelectedItem();
                if (selected.kind != "sample")
                    return;

                const int tune = juce::roundToInt (tuneSlider.getValue());
                if (processor.setUserContentTuneSemitones (selected.id, tune))
                {
                    lastReport = "Sample tuned " + juce::String (tune > 0 ? "+" : "") + juce::String (tune) + " semitones.";
                    refresh();
                    if (onContentChanged)
                        onContentChanged();
                }
            };
            addAndMakeVisible (tuneSlider);

            clearButton.setButtonText ("Clear Imports");
            clearButton.setTooltip ("Remove imported samples and MIDI from this session.");
            clearButton.onClick = [this] { clearImports(); };
            addAndMakeVisible (clearButton);

            contentList.setModel (this);
            contentList.setRowHeight (46);
            contentList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            contentList.setMultipleSelectionEnabled (false);
            contentList.setTooltip ("Imported samples and MIDI saved with this host session.");
            addAndMakeVisible (contentList);

            keyboard.setTooltip ("Audition imported samples. Hardware MIDI also triggers mapped imported samples.");
            addAndMakeVisible (keyboard);

            refresh();
        }

        ~PlayerUserImportPanel() override
        {
            stopAudition();
            contentList.setModel (nullptr);
        }

        std::function<void()> onClose;
        std::function<void()> onContentChanged;

        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (const auto& file : files)
                if (isRuntimeImportPath (file))
                    return true;
            return false;
        }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            setDragActive (false);
            importDroppedFiles (files);
        }

        void setLastReport (juce::String report)
        {
            lastReport = std::move (report);
            updateStatus();
        }

        void setDragActive (bool shouldBeActive)
        {
            if (dragActive == shouldBeActive)
                return;

            dragActive = shouldBeActive;
            repaint();
        }

        void fileDragEnter (const juce::StringArray& files, int, int) override
        {
            if (isInterestedInFileDrag (files))
                setDragActive (true);
        }

        void fileDragMove (const juce::StringArray& files, int, int) override
        {
            if (isInterestedInFileDrag (files))
                setDragActive (true);
        }

        void fileDragExit (const juce::StringArray&) override
        {
            setDragActive (false);
        }

        void importDroppedFiles (const juce::StringArray& files)
        {
            auto paths = collectRuntimeImportFiles (files);

            if (paths.isEmpty())
            {
                lastReport = "No supported files found. Drop WAV, AIFF, FLAC, MID, MIDI, or folders containing them.";
                updateStatus();
                return;
            }

            juce::String report;
            const auto mode = sampleMode.getSelectedId() == 1 ? "pads" : "keyboard";
            const bool midiStack = midiImportMode.getSelectedId() == 2;
            processor.importUserContentFiles (paths, mode, report, -1, -1, midiStack);
            lastReport = report;
            refresh();
            selectNewestItem();
            if (onContentChanged)
                onContentChanged();
        }

        void refresh()
        {
            items = processor.getUserContentSnapshot();
            contentList.updateContent();
            if (contentList.getSelectedRow() >= (int) items.size())
                contentList.deselectAllRows();
            updateButtonState();
            updateTuneFromSelection();
            updateStatus();
            repaint();
        }

        void paint (juce::Graphics& graphics) override
        {
            auto bounds = getLocalBounds().toFloat();
            graphics.setColour (juce::Colour (0xf40a0d12));
            graphics.fillRect (bounds);

            auto panel = getLocalBounds().reduced (24).toFloat();
            graphics.setColour (juce::Colour (0xff11161e));
            graphics.fillRoundedRectangle (panel, 16.0f);
            graphics.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.9f));
            graphics.drawRoundedRectangle (panel, 16.0f, 1.8f);

            auto workflow = workflowBounds.reduced (0, 3);
            if (! workflow.isEmpty())
            {
                const int gap = 8;
                const int cardWidth = juce::jmax (120, (workflow.getWidth() - gap * 3) / 4);
                const char* headings[] = { "1  DROP / IMPORT", "2  MAP", "3  AUDITION", "4  PLAY" };
                const char* bodies[] =
                {
                    "Drag files/folders here or use the import buttons.",
                    "Choose pads for drums or keyboard for pitched sounds.",
                    "Select a row, then audition on the mini keyboard.",
                    "MIDI files are applied to the performance pattern."
                };

                for (int index = 0; index < 4; ++index)
                {
                    auto card = workflow.removeFromLeft (cardWidth);
                    workflow.removeFromLeft (gap);
                    graphics.setColour (index == 0 && dragActive
                        ? PatchCraftLookAndFeel::accent().withAlpha (0.22f)
                        : PatchCraftLookAndFeel::raised().withAlpha (0.78f));
                    graphics.fillRoundedRectangle (card.toFloat(), 10.0f);
                    graphics.setColour (index == 0 && dragActive
                        ? PatchCraftLookAndFeel::accent()
                        : PatchCraftLookAndFeel::borderSoft());
                    graphics.drawRoundedRectangle (card.toFloat(), 10.0f, index == 0 && dragActive ? 1.8f : 1.0f);

                    auto textArea = card.reduced (12, 9);
                    graphics.setColour (PatchCraftLookAndFeel::textBright());
                    graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                    graphics.drawText (headings[index], textArea.removeFromTop (17), juce::Justification::centredLeft, true);
                    graphics.setColour (PatchCraftLookAndFeel::textDim());
                    graphics.setFont (juce::FontOptions (11.0f));
                    graphics.drawFittedText (bodies[index], textArea, juce::Justification::topLeft, 2);
                }
            }

            auto help = helpBounds.toFloat();
            graphics.setColour (PatchCraftLookAndFeel::raised().withAlpha (0.85f));
            graphics.fillRoundedRectangle (help, 10.0f);
            graphics.setColour (PatchCraftLookAndFeel::borderSoft());
            graphics.drawRoundedRectangle (help, 10.0f, 1.0f);

            auto text = helpBounds.reduced (14, 10);
            graphics.setColour (PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
            graphics.drawText ("Session library", text.removeFromTop (18), juce::Justification::centredLeft, true);
            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (12.0f));
            graphics.drawFittedText ("Samples are copied into this instrument session. MIDI files become playable patterns. "
                                     "The host recalls these imports with the session.",
                                     text, juce::Justification::topLeft, 3);

            auto drop = dropBounds.toFloat();
            graphics.setColour (PatchCraftLookAndFeel::accent().withAlpha (dragActive ? 0.22f : 0.08f));
            graphics.fillRoundedRectangle (drop, 10.0f);
            graphics.setColour (PatchCraftLookAndFeel::accent().withAlpha (dragActive ? 1.0f : 0.72f));
            graphics.drawRoundedRectangle (drop, 10.0f, dragActive ? 2.0f : 1.2f);
            graphics.setColour (PatchCraftLookAndFeel::textBright());
            graphics.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
            graphics.drawText (dragActive ? "RELEASE TO IMPORT" : "DROP SAMPLES, MIDI, OR FOLDERS HERE",
                               dropBounds.reduced (12, 6).removeFromTop (18),
                               juce::Justification::centredLeft, true);
            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (11.5f));
            graphics.drawFittedText ("WAV/AIFF/FLAC become pads or keyboard zones. MID/MIDI becomes the playable pattern. "
                                     "Your original instrument sounds stay intact.",
                                     dropBounds.reduced (12, 26),
                                     juce::Justification::topLeft, 2);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (24);
            auto panel = bounds.reduced (18);

            auto header = panel.removeFromTop (68);
            auto headerTop = header.removeFromTop (30);
            title.setBounds (headerTop.removeFromLeft (220));
            closeButton.setBounds (headerTop.removeFromRight (74));
            subtitle.setBounds (header);

            workflowBounds = panel.removeFromTop (74);
            panel.removeFromTop (8);

            auto actions = panel.removeFromTop (38);
            sampleMode.setBounds (actions.removeFromLeft (170));
            actions.removeFromLeft (8);
            midiImportMode.setBounds (actions.removeFromLeft (170));
            actions.removeFromLeft (8);
            importSamplesButton.setBounds (actions.removeFromLeft (128));
            actions.removeFromLeft (8);
            importMidiButton.setBounds (actions.removeFromLeft (102));
            actions.removeFromLeft (8);
            auditionButton.setBounds (actions.removeFromLeft (82));
            actions.removeFromLeft (8);
            applyMidiButton.setBounds (actions.removeFromLeft (92));
            actions.removeFromLeft (8);
            dragMidiButton.setBounds (actions.removeFromLeft (112));
            actions.removeFromLeft (8);
            clearButton.setBounds (actions.removeFromLeft (102));

            panel.removeFromTop (8);
            auto tools = panel.removeFromTop (28);
            tuneLabel.setBounds (tools.removeFromLeft (88));
            tuneSlider.setBounds (tools.removeFromLeft (170));

            panel.removeFromTop (8);
            status.setBounds (panel.removeFromTop (22));
            panel.removeFromTop (8);

            helpBounds = panel.removeFromRight (juce::jlimit (260, 390, panel.getWidth() / 3)).reduced (10, 0);
            dropBounds = helpBounds.removeFromBottom (90);
            helpBounds.removeFromBottom (8);
            keyboard.setBounds (panel.removeFromBottom (72).reduced (0, 10));
            contentList.setBounds (panel.reduced (0, 0));
        }

    private:
        class MiniKeyboard : public juce::Component,
                             public juce::SettableTooltipClient
        {
        public:
            explicit MiniKeyboard (PlayerProcessor& processorToUse) : processor (processorToUse) {}

            ~MiniKeyboard() override
            {
                stopNote();
            }

            void paint (juce::Graphics& graphics) override
            {
                auto area = getLocalBounds().reduced (2);
                graphics.setColour (juce::Colour (0xff07090c));
                graphics.fillRoundedRectangle (area.toFloat(), 8.0f);
                graphics.setColour (PatchCraftLookAndFeel::borderSoft());
                graphics.drawRoundedRectangle (area.toFloat(), 8.0f, 1.0f);

                const int keys = 24;
                const float keyWidth = area.getWidth() / (float) keys;
                for (int index = 0; index < keys; ++index)
                {
                    auto key = juce::Rectangle<float> (area.getX() + keyWidth * index + 1.0f,
                                                       area.getY() + 6.0f,
                                                       keyWidth - 2.0f,
                                                       area.getHeight() - 12.0f);
                    const int note = firstNote + index;
                    graphics.setColour (note == activeNote ? PatchCraftLookAndFeel::accent()
                                                           : juce::Colour (0xffe8d9b5));
                    graphics.fillRoundedRectangle (key, 3.0f);
                    graphics.setColour (juce::Colour (0xff15171b));
                    graphics.drawRoundedRectangle (key, 3.0f, 0.8f);
                }

                graphics.setColour (PatchCraftLookAndFeel::textDim());
                graphics.setFont (juce::FontOptions (10.0f));
                graphics.drawText ("C2", area.reduced (6, 0).removeFromLeft (40), juce::Justification::bottomLeft, true);
                graphics.drawText ("B3", area.reduced (6, 0).removeFromRight (40), juce::Justification::bottomRight, true);
            }

            void mouseDown (const juce::MouseEvent& event) override
            {
                playNoteAt (event.position.x);
            }

            void mouseDrag (const juce::MouseEvent& event) override
            {
                playNoteAt (event.position.x);
            }

            void mouseUp (const juce::MouseEvent&) override
            {
                stopNote();
            }

        private:
            void playNoteAt (float x)
            {
                const int keys = 24;
                const int index = juce::jlimit (0, keys - 1,
                    (int) ((x - 2.0f) / juce::jmax (1.0f, (float) (getWidth() - 4) / keys)));
                const int note = firstNote + index;
                if (note == activeNote)
                    return;
                stopNote();
                activeNote = note;
                processor.handleNoteOn (activeNote, 0.9f);
                repaint();
            }

            void stopNote()
            {
                if (activeNote >= 0)
                    processor.handleNoteOff (activeNote);
                activeNote = -1;
                repaint();
            }

            PlayerProcessor& processor;
            static constexpr int firstNote = 36;
            int activeNote = -1;
        };

        int getNumRows() override
        {
            return (int) items.size();
        }

        void paintListBoxItem (int rowNumber, juce::Graphics& graphics,
                               int width, int height, bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= (int) items.size())
                return;

            const auto& item = items[(size_t) rowNumber];
            auto area = juce::Rectangle<int> (0, 0, width, height).reduced (4, 3);

            graphics.setColour (rowIsSelected ? PatchCraftLookAndFeel::accent().withAlpha (0.28f)
                                               : PatchCraftLookAndFeel::raised().withAlpha (0.72f));
            graphics.fillRoundedRectangle (area.toFloat(), 7.0f);
            graphics.setColour (rowIsSelected ? PatchCraftLookAndFeel::accent()
                                               : PatchCraftLookAndFeel::borderSoft());
            graphics.drawRoundedRectangle (area.toFloat(), 7.0f, 1.0f);

            auto text = area.reduced (10, 6);
            auto badge = text.removeFromRight (96);
            graphics.setColour (item.kind == "midi" ? juce::Colour (0xff7fd3ff)
                                                     : PatchCraftLookAndFeel::accent());
            graphics.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
            graphics.drawText (item.kind.toUpperCase() + " / " + item.role.toUpperCase(),
                               badge, juce::Justification::centredRight, true);

            graphics.setColour (PatchCraftLookAndFeel::textBright());
            graphics.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
            graphics.drawText (item.name, text.removeFromTop (18), juce::Justification::centredLeft, true);

            graphics.setColour (PatchCraftLookAndFeel::textDim());
            graphics.setFont (juce::FontOptions (11.0f));
            auto summary = item.summary.isNotEmpty() ? item.summary : juce::File (item.filePath).getFileName();
            if (item.kind == "sample" && item.tuneSemitones != 0 && ! summary.containsIgnoreCase ("tune"))
                summary += " / tune " + juce::String (item.tuneSemitones > 0 ? "+" : "") + juce::String (item.tuneSemitones) + " st";
            graphics.drawText (summary,
                               text, juce::Justification::centredLeft, true);
        }

        void selectedRowsChanged (int) override
        {
            updateButtonState();
            updateTuneFromSelection();
        }

        PlayerProcessor::UserContentItem getSelectedItem() const
        {
            const int selected = contentList.getSelectedRow();
            if (selected >= 0 && selected < (int) items.size())
                return items[(size_t) selected];
            return {};
        }

        void updateButtonState()
        {
            const auto selected = getSelectedItem();
            auditionButton.setEnabled (selected.kind == "sample");
            applyMidiButton.setEnabled (selected.kind == "midi");
            dragMidiButton.setEnabled (selected.kind == "midi");
            tuneLabel.setEnabled (selected.kind == "sample");
            tuneSlider.setEnabled (selected.kind == "sample");
            clearButton.setEnabled (! items.empty());
        }

        void selectNewestItem()
        {
            if (items.empty())
                return;

            contentList.selectRow ((int) items.size() - 1);
            updateButtonState();
            updateTuneFromSelection();
        }

        void updateStatus()
        {
            int samples = 0;
            int midi = 0;
            for (const auto& item : items)
            {
                if (item.kind == "sample") ++samples;
                if (item.kind == "midi") ++midi;
            }

            const auto root = processor.getUserContentRoot().getFullPathName();
            status.setText ("Runtime imports: " + juce::String (samples) + " samples, "
                            + juce::String (midi) + " MIDI files  |  " + lastReport
                            + (lastReport.isNotEmpty() ? "  |  " : "")
                            + root,
                            juce::dontSendNotification);
        }

        void chooseFiles (bool midi)
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                midi ? "Import MIDI Files" : "Import Sample Files",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                midi ? "*.mid;*.midi" : "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

            juce::Component::SafePointer<PlayerUserImportPanel> safeThis (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                [safeThis, chooser, midi] (const juce::FileChooser& fileChooser)
                {
                    if (safeThis == nullptr)
                        return;

                    juce::StringArray chosen;
                    for (const auto& file : fileChooser.getResults())
                        chosen.add (file.getFullPathName());

                    auto paths = collectRuntimeImportFiles (chosen);

                    if (paths.isEmpty())
                    {
                        safeThis->lastReport = "No supported files found in that selection.";
                        safeThis->updateStatus();
                        return;
                    }

                    juce::String report;
                    const auto mode = safeThis->sampleMode.getSelectedId() == 1 ? "pads" : "keyboard";
                    const bool midiStack = midi && safeThis->midiImportMode.getSelectedId() == 2;
                    safeThis->processor.importUserContentFiles (paths, mode, report, -1, -1, midiStack);
                    safeThis->lastReport = report;
                    safeThis->refresh();
                    safeThis->selectNewestItem();
                    if (safeThis->onContentChanged)
                        safeThis->onContentChanged();
                });
        }

        void auditionSelected()
        {
            const auto selected = getSelectedItem();
            if (selected.kind != "sample")
                return;

            stopAudition();
            auditionNote = juce::jlimit (0, 127, selected.rootNote);
            processor.handleNoteOn (auditionNote, 0.95f);
            startTimer (450);
        }

        void applySelectedMidi()
        {
            const auto selected = getSelectedItem();
            if (selected.kind != "midi")
                return;

            lastReport = processor.applyUserMidiToPlayground (selected.id,
                                                              midiImportMode.getSelectedId() == 2)
                ? "Applied MIDI pattern: " + selected.name
                : "Could not apply selected MIDI file.";
            refresh();
            if (onContentChanged)
                onContentChanged();
        }

        void dragSelectedMidiOut()
        {
            const auto selected = getSelectedItem();
            if (selected.kind != "midi")
                return;

            const juce::File file (selected.filePath);
            if (! file.existsAsFile())
            {
                lastReport = "MIDI loop file is missing on disk.";
                updateStatus();
                return;
            }

            juce::StringArray files;
            files.add (file.getFullPathName());
            const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
            lastReport = started ? "Drag the MIDI loop into your DAW."
                                 : "This host did not accept external MIDI drag.";
            updateStatus();
        }

        void clearImports()
        {
            stopAudition();
            lastReport = processor.clearUserContent() ? "Cleared runtime imports." : "No runtime imports to clear.";
            refresh();
            if (onContentChanged)
                onContentChanged();
        }

        void timerCallback() override
        {
            stopAudition();
        }

        void stopAudition()
        {
            stopTimer();
            if (auditionNote >= 0)
                processor.handleNoteOff (auditionNote);
            auditionNote = -1;
        }

        PlayerProcessor& processor;
        juce::Label title;
        juce::Label subtitle;
        juce::Label status;
        juce::TextButton closeButton { "Close" };
        juce::ComboBox sampleMode;
        juce::ComboBox midiImportMode;
        juce::TextButton importSamplesButton { "Import Samples" };
        juce::TextButton importMidiButton { "Import MIDI" };
        juce::TextButton auditionButton { "Audition" };
        juce::TextButton applyMidiButton { "Apply MIDI" };

        class DragButton : public juce::TextButton
        {
        public:
            DragButton() = default;
            std::function<void(const juce::MouseEvent&)> onDrag;
            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (onDrag && ! e.mouseWasClicked())
                    onDrag (e);
            }
        };

        DragButton dragMidiButton;
        juce::Label tuneLabel;
        juce::Slider tuneSlider;
        juce::TextButton clearButton { "Clear Imports" };
        juce::ListBox contentList { "Runtime imports", this };
        MiniKeyboard keyboard;
        juce::Rectangle<int> workflowBounds;
        juce::Rectangle<int> helpBounds;
        juce::Rectangle<int> dropBounds;
        std::vector<PlayerProcessor::UserContentItem> items;
        juce::String lastReport;
        int auditionNote = -1;
        bool dragActive = false;
        bool updatingTuneSlider = false;

        void updateTuneFromSelection()
        {
            const auto selected = getSelectedItem();
            const juce::ScopedValueSetter<bool> guard (updatingTuneSlider, true);
            tuneSlider.setValue (selected.kind == "sample" ? selected.tuneSemitones : 0,
                                 juce::dontSendNotification);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerUserImportPanel)
    };

    PlayerEditor::PlayerEditor (PlayerProcessor& p)
        : juce::AudioProcessorEditor (&p), proc (p)
    {
        setLookAndFeel (&laf);

        // Build the renderer + buttons BEFORE calling setSize/setResizable -
        // setSize triggers resized() synchronously, and resized() dereferences
        // renderer->setBounds(...). If we set size first, that's a null deref.
        renderer = std::make_unique<PlayerGuiRenderer> (proc, assets);
        renderer->onPresetBrowserRequested = [this]
        {
            showPresetMenu();
        };
        renderer->onRuntimeImportReport = [this] (const juce::String& report)
        {
            userImportVisible = true;
            if (userImportPanel != nullptr)
            {
                userImportPanel->setLastReport (report);
                userImportPanel->refresh();
            }
            refreshPresetControls();
            if (performancePanel != nullptr)
                performancePanel->rebuild();
            if (controlCenter != nullptr)
                controlCenter->rebuild();
            resized();
            repaint();
        };
        addAndMakeVisible (*renderer);

        // Library browser (hidden by default)
        libraryBrowser = std::make_unique<LibraryBrowser> (proc.getLibraryScanner(),
            LibraryBrowser::PackFilter::Any
        );
        libraryBrowser->onPackSelected = [this] (const juce::File& folder) {
            juce::String err;
            if (proc.loadPack (folder, err))
            {
                libraryVisible = false;
                controlCenterVisible = false;
                libraryBrowser->setVisible (false);
                renderer->setVisible (true);
                resized();
            }
            else
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Load Failed")
                        .withMessage ("Could not load pack:\n" + err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
            }
        };
        libraryBrowser->onClose = [this] {
            libraryVisible = false;
            libraryBrowser->setVisible (false);
            renderer->setVisible (true);
            resized();
        };
        libraryBrowser->setVisible (false);
        addAndMakeVisible (*libraryBrowser);

        performancePanel = std::make_unique<PlayerPerformancePanel> (proc);
        performancePanel->onClose = [this]
        {
            performanceVisible = false;
            resized();
        };
        performancePanel->onToggleFloat = [this] (bool shouldFloat)
        {
            performanceFloating = shouldFloat;
            resized();
        };
        performancePanel->setVisible (false);
        addAndMakeVisible (*performancePanel);

        controlCenter = std::make_unique<PlayerControlCenter> (proc);
        controlCenter->onClose = [this]
        {
            controlCenterVisible = false;
            resized();
        };
        controlCenter->onOpenLibrary = [this]
        {
            controlCenterVisible = false;
            toggleLibrary();
        };
        controlCenter->onOpenEngine = [this]
        {
            controlCenterVisible = false;
            if (! performanceVisible)
                togglePerformancePanel();
            else
                resized();
        };
        controlCenter->setVisible (false);
        addAndMakeVisible (*controlCenter);

        userImportPanel = std::make_unique<PlayerUserImportPanel> (proc);
        userImportPanel->onClose = [this]
        {
            userImportVisible = false;
            resized();
        };
        userImportPanel->onContentChanged = [this]
        {
            refreshPresetControls();
            if (performancePanel != nullptr)
                performancePanel->rebuild();
            if (controlCenter != nullptr)
                controlCenter->rebuild();
            if (renderer != nullptr)
                renderer->rebuild();
        };
        userImportPanel->setVisible (false);
        addAndMakeVisible (*userImportPanel);

        presetLoadingLabel.setJustificationType (juce::Justification::centred);
        presetLoadingLabel.setInterceptsMouseClicks (false, false);
        presetLoadingLabel.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        presetLoadingLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        presetLoadingLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xee07090c));
        presetLoadingLabel.setColour (juce::Label::outlineColourId, PatchCraftLookAndFeel::accent());
        presetLoadingLabel.setVisible (false);
        addAndMakeVisible (presetLoadingLabel);

        addAndMakeVisible (loadBtn);
        loadBtn.getProperties().set ("accent", true);
        loadBtn.onClick = [this] { showLoadDialog(); };

        libraryBtn.setButtonText ("LIB");
        performanceBtn.setButtonText ("SOUND");
        controlBtn.setButtonText ("CTRL");

        addAndMakeVisible (libraryBtn);
        libraryBtn.getProperties().set ("accent", true);
       #if PATCHCRAFT_PLAYER_FX
        libraryBtn.setTooltip ("Open the instrument library. FX Player passes host audio through when previewing instrument packs.");
       #else
        libraryBtn.setTooltip ("Open the instrument library.");
       #endif
        libraryBtn.onClick = [this] { toggleLibrary(); };

        prevPresetBtn.setTooltip ("Load previous preset.");
        prevPresetBtn.onClick = [this]
        {
            const int count = proc.getPresetCount();
            int current = proc.getCurrentPresetIndex();
            if (count <= 0)
                return;
            if (current < 0)
                current = 0;
            const int next = (current - 1 + count) % count;
            showPresetLoading (proc.getPresetName (next));
            if (proc.applyPresetByIndex (next))
            {
                refreshPresetControls();
                if (performancePanel != nullptr)
                    performancePanel->rebuild();
                renderer->repaint();
            }
        };
        addAndMakeVisible (prevPresetBtn);

        presetBtn.setTooltip ("Open this instrument's preset list.");
        presetBtn.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (presetBtn);

        nextPresetBtn.setTooltip ("Load next preset.");
        nextPresetBtn.onClick = [this]
        {
            const int count = proc.getPresetCount();
            int current = proc.getCurrentPresetIndex();
            if (count <= 0)
                return;
            if (current < 0)
                current = 0;
            const int next = (current + 1) % count;
            showPresetLoading (proc.getPresetName (next));
            if (proc.applyPresetByIndex (next))
            {
                refreshPresetControls();
                if (performancePanel != nullptr)
                    performancePanel->rebuild();
                renderer->repaint();
            }
        };
        addAndMakeVisible (nextPresetBtn);

        addAndMakeVisible (performanceBtn);
        performanceBtn.setTooltip ("Open live sound controls for this instrument.");
        performanceBtn.onClick = [this] { togglePerformancePanel(); };

        controlBtn.setTooltip ("Open instrument info, mix, routing, and MIDI controls.");
        controlBtn.onClick = [this] { toggleControlCenter(); };
        addAndMakeVisible (controlBtn);

        setSize (1280, 800 + kPlayerMenuBarHeight);
        setResizable (true, true);
        setResizeLimits (720, 520, 2400, 1800);

        proc.addEditorListener (this);
        packChanged();
    }

    PlayerEditor::~PlayerEditor()
    {
        proc.removeEditorListener (this);
        setLookAndFeel (nullptr);
    }

    void PlayerEditor::paint (juce::Graphics& g)
    {
        if (const auto* pack = proc.getPack())
            g.fillAll (pack->manifest.playerBackgroundColour);
        else
            g.fillAll (PatchCraftLookAndFeel::bg());

        const auto* pack = proc.getPack();
        auto fullTopBar = getLocalBounds().removeFromTop (kPlayerMenuBarHeight);
        auto bar = playerChromeBoundsFor (fullTopBar, pack);
        auto titleArea = bar.removeFromTop (kPlayerTitleAreaHeight);
        auto toolbarArea = bar;
        const auto titleTheme = pack != nullptr ? pack->manifest.playerTitleBarTheme : juce::String ("classic");
        const auto titlePlacement = pack != nullptr ? pack->manifest.playerTitleTextPlacement : juce::String ("left");
        const auto titleFontFamily = pack != nullptr ? pack->manifest.playerTitleFontFamily : juce::String ("Default");
        const auto accent = pack != nullptr ? pack->manifest.playerAccentColour : PatchCraftLookAndFeel::accent();
        const auto panel = pack != nullptr ? pack->manifest.playerPanelColour : juce::Colour (0xff0b0d11);
        const auto bg = pack != nullptr ? pack->manifest.playerBackgroundColour : juce::Colour (0xff07090c);

        auto themeTitleTop = titleTheme == "clean-pro" ? panel.brighter (0.22f)
                           : titleTheme == "dark-utility" ? panel.darker (0.25f)
                           : titleTheme == "glass" ? panel.withAlpha (0.65f)
                           : panel.brighter (0.06f);
        auto themeTitleBottom = titleTheme == "aurora" ? accent.interpolatedWith (bg, 0.62f)
                              : titleTheme == "minimal" ? bg.darker (0.15f)
                              : titleTheme == "compact-daw" ? panel.darker (0.12f)
                              : bg.darker (0.08f);
        juce::ColourGradient gradient (themeTitleTop,
                                       (float) titleArea.getX(), (float) titleArea.getY(),
                                       themeTitleBottom,
                                       (float) titleArea.getRight(), (float) titleArea.getBottom(),
                                       false);
        g.setGradientFill (gradient);
        g.fillRect (titleArea);
        g.setColour (panel.withAlpha (0.94f));
        g.fillRect (toolbarArea);
        const bool useFullTitleBackground = pack != nullptr
                                         && pack->manifest.playerTitleBannerImage.isNotEmpty()
                                         && titleThemeUsesBannerArtwork (titleTheme);
        if (useFullTitleBackground)
        {
            const auto bannerFile = juce::File::isAbsolutePath (pack->manifest.playerTitleBannerImage)
                ? juce::File (pack->manifest.playerTitleBannerImage)
                : pack->rootFolder.getChildFile (pack->manifest.playerTitleBannerImage);
            if (auto banner = assets.loadImage (bannerFile); banner.isValid())
            {
                g.drawImage (banner, titleArea.toFloat(), juce::RectanglePlacement::fillDestination);
                g.setColour (juce::Colour (0x8805070a));
                g.fillRect (titleArea);
            }
        }
        if ((titleTheme == "minimal" || titleTheme == "no-chrome") && ! useFullTitleBackground)
        {
            g.setColour (bg);
            g.fillRect (titleArea);
        }
        if (titleTheme == "glass" && ! useFullTitleBackground)
        {
            g.setColour (accent.withAlpha (0.08f));
            g.fillRect (titleArea.reduced (12, 8));
        }
        if (titleTheme == "dark-utility")
        {
            g.setColour (panel.darker (0.42f));
            g.fillRect (titleArea);
            g.setColour (accent.withAlpha (0.38f));
            g.fillRect (titleArea.withBottom (titleArea.getBottom()).withHeight (2));
        }
        if (titleTheme == "compact-daw")
        {
            g.setColour (panel.brighter (0.05f));
            g.fillRect (titleArea.reduced (0, 8));
        }
        if (titleTheme == "neon-strip" || titleTheme == "aurora")
        {
            g.setColour (accent.withAlpha (0.82f));
            g.fillRoundedRectangle (titleArea.withHeight (3).toFloat(), 2.0f);
        }
        if (titleTheme == "split-brand")
        {
            g.setColour (accent.withAlpha (0.88f));
            g.fillRect (titleArea.withWidth (5));
        }
        if (titleTheme == "logo-rail")
        {
            g.setColour (accent.withAlpha (0.14f));
            g.fillRect (titleArea.withWidth (96));
            g.setColour (accent.withAlpha (0.72f));
            g.drawVerticalLine (96, 4.0f, (float) titleArea.getBottom() - 5.0f);
        }

        if (titleTheme != "no-chrome")
        {
            g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.82f));
            g.fillRect (toolbarArea.withHeight (1));
            g.setColour (bg.withAlpha (0.42f));
            g.fillRect (toolbarArea.withTrimmedTop (1));
            auto toolWell = toolbarArea.reduced (10, 5);
            const auto corner = titleTheme == "compact-daw" || titleTheme == "dark-utility" ? 2.0f
                              : 6.0f;
            g.setColour ((titleTheme == "glass" ? panel.withAlpha (0.58f)
                                                : panel.withAlpha (0.96f)));
            if (titleTheme == "minimal")
                g.fillRect (toolWell.withHeight (1));
            else
                g.fillRoundedRectangle (toolWell.toFloat(), corner);
            g.setColour ((titleTheme == "neon-strip" ? accent : PatchCraftLookAndFeel::borderSoft()).withAlpha (0.72f));
            if (titleTheme != "minimal")
                g.drawRoundedRectangle (toolWell.toFloat().reduced (0.5f), corner, 1.0f);
        }

        const int bannerWidth = playerTitleBrandWidth (titleTheme, titleArea.getWidth());
        const auto brandFrame = juce::Rectangle<int> (titleArea.getX() + 10,
                                                      titleArea.getY() + 10,
                                                      bannerWidth,
                                                      52);
        const auto artworkBounds = brandFrame.withWidth (kPlayerTitleBarArtworkSize)
                                             .withHeight (kPlayerTitleBarArtworkSize)
                                             .withY (titleArea.getY() + (titleArea.getHeight() - kPlayerTitleBarArtworkSize) / 2);
        auto brand = (titleTheme == "logo-rail" || titleTheme == "no-chrome")
            ? brandFrame.reduced (0, 1)
            : brandFrame.withTrimmedLeft (kPlayerTitleBarArtworkSize + 8).reduced (0, 1);
        bool drewLogo = false;
        bool drewTitleBanner = false;
        if (pack != nullptr && bannerWidth > 0)
        {
            if (pack->manifest.playerTitleBannerImage.isNotEmpty()
                && ! useFullTitleBackground)
            {
                const auto bannerFile = juce::File::isAbsolutePath (pack->manifest.playerTitleBannerImage)
                    ? juce::File (pack->manifest.playerTitleBannerImage)
                    : pack->rootFolder.getChildFile (pack->manifest.playerTitleBannerImage);
                if (auto banner = assets.loadImage (bannerFile); banner.isValid())
                {
                    g.saveState();
                    juce::Path clip;
                    clip.addRoundedRectangle (brandFrame.toFloat(), 7.0f);
                    g.reduceClipRegion (clip);
                    g.drawImage (banner, brandFrame.toFloat(), juce::RectanglePlacement::fillDestination);
                    g.setColour (juce::Colour (0xaa05070a));
                    g.fillRoundedRectangle (brandFrame.toFloat(), 7.0f);
                    g.restoreState();
                    g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.86f));
                    g.drawRoundedRectangle (brandFrame.toFloat().reduced (0.5f), 7.0f, 1.0f);
                    drewTitleBanner = true;
                    brand = brandFrame.reduced (11, 3);
                }
            }

            auto artworkPath = pack->manifest.playerLogoImage;
            if (artworkPath.isEmpty())
                artworkPath = pack->manifest.libraryThumbnail;

            if (! drewTitleBanner && artworkPath.isNotEmpty() && titleTheme != "no-chrome")
            {
                const auto logoFile = juce::File::isAbsolutePath (artworkPath)
                    ? juce::File (artworkPath)
                    : pack->rootFolder.getChildFile (artworkPath);
                if (auto logo = assets.loadImage (logoFile); logo.isValid())
                {
                    g.saveState();
                    juce::Path clip;
                    clip.addRoundedRectangle (artworkBounds.toFloat(), 5.0f);
                    g.reduceClipRegion (clip);
                    g.drawImage (logo, artworkBounds.toFloat(), juce::RectanglePlacement::fillDestination);
                    g.restoreState();
                    g.setColour (PatchCraftLookAndFeel::borderSoft().withAlpha (0.75f));
                    g.drawRoundedRectangle (artworkBounds.toFloat(), 5.0f, 1.0f);
                    drewLogo = true;
                }
            }
        }
        if (! drewTitleBanner && ! drewLogo && bannerWidth > 0 && titleTheme != "no-chrome")
        {
            const auto fallbackName = pack != nullptr ? playerInstrumentName (pack) : juce::String ("Instrument");
            const auto initial = fallbackName.isNotEmpty() ? fallbackName.substring (0, 1).toUpperCase() : juce::String ("P");
            g.setColour ((pack != nullptr ? pack->manifest.playerAccentColour : PatchCraftLookAndFeel::accent()).withAlpha (0.22f));
            g.fillRoundedRectangle (artworkBounds.toFloat().reduced (1.0f), 5.0f);
            g.setColour (pack != nullptr ? pack->manifest.playerAccentColour : PatchCraftLookAndFeel::accent());
            g.setFont (playerChromeFont (titleFontFamily, (float) artworkBounds.getHeight() * 0.52f, true));
            g.drawText (initial, artworkBounds, juce::Justification::centred, true);
        }

        const auto brandName = pack != nullptr
            ? playerInstrumentName (pack)
            : juce::String ("PATCHCRAFT");
        const auto tagline = pack != nullptr && pack->manifest.playerTagline.isNotEmpty()
            ? pack->manifest.playerTagline
            : juce::String();
        if (titlePlacement != "hidden")
        {
            const auto justify = titlePlacement == "center" ? juce::Justification::centred
                               : titlePlacement == "right"  ? juce::Justification::centredRight
                                                            : juce::Justification::centredLeft;
            if (titlePlacement == "center")
                brand = juce::Rectangle<int> (titleArea.getX() + titleArea.getWidth() / 2 - 190,
                                              brand.getY(), 380, brand.getHeight());
            else if (titlePlacement == "right")
                brand = juce::Rectangle<int> (titleArea.getRight() - 390, brand.getY(), 240, brand.getHeight());

            g.setColour (PatchCraftLookAndFeel::textBright());
            g.setFont (playerChromeFont (titleFontFamily, titleTheme == "artist-card" ? 16.5f : 15.0f, true));
            g.drawText (brandName, brand.removeFromTop (drewTitleBanner ? 17 : 19), justify, true);
            if (tagline.isNotEmpty())
            {
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (playerChromeFont (titleFontFamily, 10.0f, true));
                g.drawText (tagline.toUpperCase(), brand, justify, true);
            }
        }
    }

    void PlayerEditor::resized()
    {
        if (renderer == nullptr) return;

        auto r = getLocalBounds();
        auto topBar = r.removeFromTop (kPlayerMenuBarHeight);
        auto contentBounds = r;
        const auto* pack = proc.getPack();
        const bool showPackMenu = pack == nullptr || pack->manifest.playerShowPackMenu;
        const bool showLibrary = pack == nullptr || pack->manifest.playerShowLibraryBrowser;
        const auto titleButtonStyle = pack != nullptr && pack->manifest.playerTitleButtonStyle.isNotEmpty()
            ? pack->manifest.playerTitleButtonStyle
            : juce::String ("outlined");
        for (auto* button : { &libraryBtn, &performanceBtn, &controlBtn,
                              &prevPresetBtn, &presetBtn, &nextPresetBtn })
        {
            button->getProperties().set ("corner", titleButtonStyle == "square" ? 2.0 : (titleButtonStyle == "pill" ? 12.0 : 5.0));
            if (pack != nullptr)
            {
                button->setColour (juce::TextButton::buttonColourId,
                                   titleButtonStyle == "minimal" ? juce::Colours::transparentBlack
                                   : titleButtonStyle == "filled" ? pack->manifest.playerAccentColour.withAlpha (0.28f)
                                                                  : pack->manifest.playerPanelColour.brighter (0.04f));
                button->setColour (juce::TextButton::textColourOffId, pack->manifest.playerTextColour);
            }
        }

        if (libraryVisible)
        {
            libraryBrowser->setBounds (contentBounds);
            renderer->setVisible (false);
            libraryBrowser->setVisible (true);
            if (performancePanel != nullptr)
                performancePanel->setVisible (false);
            if (controlCenter != nullptr)
                controlCenter->setVisible (false);
            if (userImportPanel != nullptr)
                userImportPanel->setVisible (false);
        }
        else
        {
            auto renderBounds = contentBounds;
            const bool showPerformance = performanceVisible && pack != nullptr && performancePanel != nullptr;
            if (showPerformance)
            {
                performancePanel->setFloating (false);
                performancePanel->setBounds (contentBounds.reduced (10));
                performancePanel->setVisible (true);
            }
            else if (performancePanel != nullptr)
            {
                performancePanel->setVisible (false);
            }

            renderer->setBounds (renderBounds);
            renderer->setVisible (true);
            libraryBrowser->setVisible (false);
            if (performancePanel != nullptr && performancePanel->isVisible())
                performancePanel->toFront (false);

            if (controlCenter != nullptr)
            {
                controlCenter->setBounds (contentBounds);
                controlCenter->setVisible (controlCenterVisible && pack != nullptr);
                if (controlCenter->isVisible())
                    controlCenter->toFront (false);
            }

            if (userImportPanel != nullptr)
            {
                userImportPanel->setBounds (contentBounds);
                userImportPanel->setVisible (userImportVisible && pack != nullptr);
                if (userImportPanel->isVisible())
                    userImportPanel->toFront (false);
            }
        }

        const auto chromeBar = playerChromeBoundsFor (topBar, pack);
        auto toolbarArea = chromeBar.withTop (chromeBar.getY() + kPlayerTitleAreaHeight);
        auto toolWell = toolbarArea.reduced (14, 5);
        const int buttonH = juce::jlimit (26, 32, toolWell.getHeight() - 2);
        const int buttonY = toolWell.getY() + (toolWell.getHeight() - buttonH) / 2;
        const bool runtimeVisible = pack != nullptr && ! libraryVisible;
        const bool compact = chromeBar.getWidth() < 980;
        const bool narrow = chromeBar.getWidth() < 760;
        const int gap = narrow ? 6 : compact ? 9 : 12;
        int leftX = toolWell.getX();

        auto placeLeft = [&] (juce::TextButton& button, bool visible, int width)
        {
            button.setVisible (visible);
            if (visible)
            {
                button.setBounds (leftX, buttonY, width, buttonH);
                leftX += width + gap;
            }
            else
            {
                button.setBounds ({});
            }
        };

        placeLeft (libraryBtn, showLibrary, narrow ? 58 : compact ? 64 : 72);

        int rightX = toolWell.getRight();
        auto placeRight = [&] (juce::TextButton& button, bool visible, int width)
        {
            button.setVisible (visible);
            if (visible)
            {
                rightX -= width;
                button.setBounds (rightX, buttonY, width, buttonH);
                rightX -= gap;
            }
            else
            {
                button.setBounds ({});
            }
        };

        placeRight (controlBtn, runtimeVisible, narrow ? 64 : compact ? 72 : 82);
        placeRight (performanceBtn, runtimeVisible, narrow ? 72 : compact ? 82 : 92);

        const bool hasPresets = pack != nullptr && proc.getPresetCount() > 0;
        const int presetAreaX = leftX + gap;
        const int presetAreaWidth = juce::jmax (0, rightX - leftX - gap * 2);
        const int presetWidth = juce::jlimit (96, 240, presetAreaWidth - 62);
        const int presetX = presetAreaX + juce::jmax (0, (presetAreaWidth - presetWidth - 62) / 2);
        prevPresetBtn.setBounds (presetX, buttonY, 26, buttonH);
        presetBtn.setBounds (presetX + 31, buttonY, presetWidth, buttonH);
        nextPresetBtn.setBounds (presetX + 36 + presetWidth, buttonY, 26, buttonH);

        prevPresetBtn.setVisible (hasPresets && ! libraryVisible && presetAreaWidth >= 170);
        presetBtn.setVisible (hasPresets && ! libraryVisible && presetAreaWidth >= 170);
        nextPresetBtn.setVisible (hasPresets && ! libraryVisible && presetAreaWidth >= 170);
        loadBtn.setVisible (false);

        const int overlayW = juce::jmin (540, juce::jmax (280, contentBounds.getWidth() - 80));
        const int overlayH = 72;
        presetLoadingLabel.setBounds (juce::Rectangle<int> (overlayW, overlayH).withCentre (contentBounds.getCentre()));
        if (presetLoadingLabel.isVisible())
            presetLoadingLabel.toFront (false);
    }

    void PlayerEditor::resizeToCurrentPackCanvas()
    {
        int width = 1280;
        int height = 800 + kPlayerMenuBarHeight;

        if (const auto* loadedPack = proc.getPack())
        {
            width = juce::jlimit (640, 1920, loadedPack->canvasSize.width);
            height = juce::jlimit (420, 1200, loadedPack->canvasSize.height + kPlayerMenuBarHeight);
        }

        const int minWidth = juce::jlimit (640, 1200, width / 2);
        const int minHeight = juce::jlimit (420, 900, height / 2);
        const int maxWidth = juce::jlimit (width, 2600, width * 2);
        const int maxHeight = juce::jlimit (height, 1900, height * 2);

        setResizable (true, true);
        setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
        if (getWidth() != width || getHeight() != height)
            setSize (width, height);
    }

    void PlayerEditor::packChanged()
    {
        refreshTooltipWindowState();
        resizeToCurrentPackCanvas();
        renderer->rebuild();
        presetLoadingLabel.setVisible (false);
        loadBtn.setVisible (false);
        libraryBtn.setVisible (proc.getPack() == nullptr || proc.getPack()->manifest.playerShowLibraryBrowser);
        performanceBtn.setVisible (proc.getPack() != nullptr);
        controlBtn.setVisible (false);
        if (performancePanel != nullptr)
            performancePanel->rebuild();
        if (controlCenter != nullptr)
            controlCenter->rebuild();
        if (userImportPanel != nullptr)
            userImportPanel->refresh();
        refreshPresetControls();
        resized();
        repaint();
    }

    bool PlayerEditor::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (auto& f : files)
        {
            if (proc.getPack() != nullptr && isRuntimeImportPath (f))
                return true;

            if (proc.allowsExternalPackLoading()
                && (juce::File (f).isDirectory() || f.endsWithIgnoreCase (".patchcraft")))
                return true;
        }
        return false;
    }

    void PlayerEditor::fileDragEnter (const juce::StringArray& files, int x, int y)
    {
        if (proc.getPack() == nullptr || ! isInterestedInFileDrag (files))
            return;

        if (renderer != nullptr
            && renderer->isVisible()
            && renderer->getBounds().contains (x, y)
            && renderer->isInterestedInFileDrag (files))
            return;

        if (userImportPanel != nullptr)
        {
            userImportPanel->setDragActive (true);
            userImportPanel->setLastReport ("Drop samples, MIDI, or a folder to add them to this session.");
        }
        if (userImportVisible)
            resized();
    }

    void PlayerEditor::refreshTooltipWindowState()
    {
        const auto* pack = proc.getPack();
        const bool enabled = pack == nullptr || pack->manifest.playerShowParameterGuidance;
        tooltipWindow.setMillisecondsBeforeTipAppears (enabled ? 650 : std::numeric_limits<int>::max() / 4);
    }

    void PlayerEditor::fileDragMove (const juce::StringArray& files, int x, int y)
    {
        if (proc.getPack() == nullptr || userImportPanel == nullptr || ! isInterestedInFileDrag (files))
            return;

        if (renderer != nullptr
            && renderer->isVisible()
            && renderer->getBounds().contains (x, y)
            && renderer->isInterestedInFileDrag (files))
        {
            userImportPanel->setDragActive (false);
            return;
        }

        userImportPanel->setDragActive (true);
    }

    void PlayerEditor::fileDragExit (const juce::StringArray&)
    {
        if (userImportPanel != nullptr)
            userImportPanel->setDragActive (false);
    }

    void PlayerEditor::filesDropped (const juce::StringArray& files, int x, int y)
    {
        if (userImportPanel != nullptr)
            userImportPanel->setDragActive (false);

        auto runtimeFiles = collectRuntimeImportFiles (files);

        if (proc.getPack() != nullptr && ! runtimeFiles.isEmpty())
        {
            if (renderer != nullptr
                && renderer->isVisible()
                && renderer->getBounds().contains (x, y)
                && renderer->isInterestedInFileDrag (files))
            {
                const auto rendererBounds = renderer->getBounds();
                renderer->filesDropped (files, x - rendererBounds.getX(), y - rendererBounds.getY());
                userImportVisible = true;
                resized();
                return;
            }

            juce::String report;
            proc.importUserContentFiles (runtimeFiles, "pads", report);
            userImportVisible = true;
            if (userImportPanel != nullptr)
            {
                userImportPanel->setLastReport (report);
                userImportPanel->refresh();
            }
            if (renderer != nullptr)
                renderer->rebuild();
            resized();
            return;
        }

        if (! proc.allowsExternalPackLoading())
            return;

        for (auto& f : files)
        {
            juce::File folder (f);
            if (! folder.exists()) continue;
            if (! folder.isDirectory() && f.endsWithIgnoreCase (".patchcraft"))
            {
                // .patchcraft files could be unpacked to a temp folder in future
                continue;
            }
            juce::String err;
            if (proc.loadPack (folder, err))
                break;
        }
    }

    void PlayerEditor::showLoadDialog()
    {
        if (! proc.allowsExternalPackLoading())
            return;

        // Let user select the instrument folder directly
        auto chooser = std::make_shared<juce::FileChooser> (
            "Select Instrument Folder",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File() || !folder.isDirectory()) return;

                juce::String err;
                if (! proc.loadPack (folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Load Failed")
                            .withMessage ("Could not load instrument from:\n" + folder.getFullPathName() + "\n\n" + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                }
            });
    }



    void PlayerEditor::toggleLibrary()
    {
        if (! proc.allowsExternalPackLoading())
            return;

        libraryVisible = !libraryVisible;
        if (libraryVisible)
        {
            controlCenterVisible = false;
            performanceVisible = false;
            userImportVisible = false;
            // Scan library when first opened
            proc.getLibraryScanner().scanLibrary();
        }
        resized();
    }

    void PlayerEditor::togglePerformancePanel()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        controlCenterVisible = false;
        userImportVisible = false;
        performanceFloating = false;
        performanceVisible = ! performanceVisible;
        if (performancePanel != nullptr)
            performancePanel->rebuild();
        resized();
    }

    void PlayerEditor::toggleControlCenter()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        performanceVisible = false;
        userImportVisible = false;
        controlCenterVisible = ! controlCenterVisible;
        if (controlCenterVisible)
        {
            if (controlCenter != nullptr)
                controlCenter->rebuild();
        }
        resized();
    }

    void PlayerEditor::toggleUserImportPanel()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        performanceVisible = false;
        controlCenterVisible = false;
        userImportVisible = ! userImportVisible;
        if (userImportPanel != nullptr)
            userImportPanel->refresh();
        resized();
    }

    void PlayerEditor::showRackPanel()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        performanceVisible = false;
        userImportVisible = false;
        controlCenterVisible = true;
        if (controlCenter != nullptr)
            controlCenter->showRack();
        resized();
    }

    void PlayerEditor::showSnapshotsPanel()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        performanceVisible = false;
        userImportVisible = false;
        controlCenterVisible = true;
        if (controlCenter != nullptr)
            controlCenter->showSnapshots();
        resized();
    }

    void PlayerEditor::showSoundDnaPanel()
    {
        if (proc.getPack() == nullptr)
            return;

        libraryVisible = false;
        performanceVisible = false;
        userImportVisible = false;
        controlCenterVisible = true;
        if (controlCenter != nullptr)
            controlCenter->showSoundDna();
        resized();
    }

    void PlayerEditor::showPresetMenu()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || pack->presets.empty())
            return;

        struct PresetBrowser final : public juce::Component,
                                     private juce::ListBoxModel
        {
            PresetBrowser (PlayerProcessor& processorIn,
                           std::set<std::string>& favoritesIn,
                           bool auditionIn,
                           bool closeAfterLoadIn,
                           std::function<void (int)> applyIn,
                           std::function<void (bool, bool)> savePrefsIn)
                : processor (processorIn),
                  favorites (favoritesIn),
                  applyPreset (std::move (applyIn)),
                  savePrefs (std::move (savePrefsIn))
            {
                setSize (760, 520);

                search.setTextToShowWhenEmpty ("Search presets", PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (search);
                search.onTextChange = [this] { rebuildRows(); };

                tagBox.addItem ("All Tags", 1);
                tagBox.addItem ("Favorites", 2);
                addAndMakeVisible (tagBox);
                tagBox.onChange = [this] { rebuildRows(); };

                auditionToggle.setButtonText ("Audition on select");
                auditionToggle.setToggleState (auditionIn, juce::dontSendNotification);
                addAndMakeVisible (auditionToggle);

                closeAfterLoadToggle.setButtonText ("Close after load");
                closeAfterLoadToggle.setToggleState (closeAfterLoadIn, juce::dontSendNotification);
                addAndMakeVisible (closeAfterLoadToggle);

                favButton.setButtonText ("Favorite");
                favButton.onClick = [this] { toggleFavorite(); };
                addAndMakeVisible (favButton);

                loadButton.setButtonText ("Load");
                loadButton.getProperties().set ("accent", true);
                loadButton.onClick = [this] { loadSelected (true); };
                addAndMakeVisible (loadButton);

                closeButton.setButtonText ("Close");
                closeButton.onClick = [this]
                {
                    savePrefs (auditionToggle.getToggleState(), closeAfterLoadToggle.getToggleState());
                    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                        window->exitModalState (0);
                };
                addAndMakeVisible (closeButton);

                list.setModel (this);
                list.setRowHeight (54);
                list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0x00000000));
                addAndMakeVisible (list);

                const auto* pack = processor.getPack();
                if (pack != nullptr)
                {
                    for (const auto& preset : pack->presets)
                    {
                        for (const auto& tag : preset.tags)
                            if (tag.trim().isNotEmpty())
                                allTags.addIfNotAlreadyThere (tag.trim());
                        if (preset.theme.trim().isNotEmpty())
                            allTags.addIfNotAlreadyThere (preset.theme.trim());
                    }
                }
                allTags.sort (true);
                int id = 10;
                for (const auto& tag : allTags)
                    tagBox.addItem (tag, id++);

                rebuildRows();
                const int current = processor.getCurrentPresetIndex();
                if (juce::isPositiveAndBelow (current, rows.size()))
                    list.selectRow (rows.indexOf (current), juce::dontSendNotification);
                updateDetails();
            }

            ~PresetBrowser() override
            {
                savePrefs (auditionToggle.getToggleState(), closeAfterLoadToggle.getToggleState());
            }

            int getNumRows() override { return rows.size(); }

            void paintListBoxItem (int row, juce::Graphics& graphics, int width, int height, bool selected) override
            {
                if (! juce::isPositiveAndBelow (row, rows.size()))
                    return;
                const auto* pack = processor.getPack();
                if (pack == nullptr)
                    return;

                const int presetIndex = rows[row];
                if (! juce::isPositiveAndBelow (presetIndex, (int) pack->presets.size()))
                    return;
                const auto& preset = pack->presets[(size_t) presetIndex];

                auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (6, 4);
                const bool current = presetIndex == processor.getCurrentPresetIndex();
                graphics.setColour (selected ? PatchCraftLookAndFeel::accent().withAlpha (0.22f)
                                             : PatchCraftLookAndFeel::panel().withAlpha (0.76f));
                graphics.fillRoundedRectangle (bounds.toFloat(), 8.0f);
                graphics.setColour (current ? PatchCraftLookAndFeel::accent()
                                            : PatchCraftLookAndFeel::borderSoft());
                graphics.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, current ? 1.6f : 1.0f);

                auto text = bounds.reduced (12, 5);
                const bool fav = favorites.count (preset.name.toStdString()) != 0;
                graphics.setColour (fav ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::textBright());
                graphics.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
                graphics.drawText ((fav ? "* " : "") + preset.name,
                                   text.removeFromTop (18),
                                   juce::Justification::centredLeft, true);

                juce::String tagLine = preset.theme;
                for (const auto& tag : preset.tags)
                    if (tagLine.length() < 72 && ! tagLine.containsIgnoreCase (tag))
                        tagLine << (tagLine.isNotEmpty() ? "  /  " : "") << tag;
                graphics.setColour (PatchCraftLookAndFeel::textDim());
                graphics.setFont (juce::FontOptions (10.5f));
                graphics.drawText (tagLine, text, juce::Justification::centredLeft, true);
            }

            void selectedRowsChanged (int) override
            {
                updateDetails();
                if (auditionToggle.getToggleState())
                    loadSelected (false);
            }

            void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override
            {
                loadSelected (true);
            }

            void paint (juce::Graphics& graphics) override
            {
                graphics.fillAll (PatchCraftLookAndFeel::bg());
                auto area = getLocalBounds().reduced (18);

                graphics.setColour (PatchCraftLookAndFeel::panel());
                graphics.fillRoundedRectangle (area.toFloat(), 14.0f);
                graphics.setColour (PatchCraftLookAndFeel::border());
                graphics.drawRoundedRectangle (area.toFloat().reduced (0.5f), 14.0f, 1.2f);

                auto body = area.reduced (18);
                graphics.setColour (PatchCraftLookAndFeel::accent());
                graphics.setFont (juce::FontOptions (20.0f).withStyle ("bold"));
                graphics.drawText ("Preset Browser", body.removeFromTop (28), juce::Justification::centredLeft, true);

                if (selectedDescription.isNotEmpty())
                {
                    auto detail = getLocalBounds().reduced (36).removeFromBottom (88);
                    detail.removeFromRight (196);
                    graphics.setColour (PatchCraftLookAndFeel::panel().darker (0.35f).withAlpha (0.90f));
                    graphics.fillRoundedRectangle (detail.toFloat(), 10.0f);
                    graphics.setColour (PatchCraftLookAndFeel::text());
                    graphics.setFont (juce::FontOptions (12.0f));
                    graphics.drawFittedText (selectedDescription, detail.reduced (12, 8), juce::Justification::topLeft, 3);
                }
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (36);
                area.removeFromTop (38);
                auto filters = area.removeFromTop (32);
                search.setBounds (filters.removeFromLeft (260));
                filters.removeFromLeft (10);
                tagBox.setBounds (filters.removeFromLeft (170));
                filters.removeFromLeft (14);
                auditionToggle.setBounds (filters.removeFromLeft (150));
                closeAfterLoadToggle.setBounds (filters.removeFromLeft (138));

                area.removeFromTop (12);
                auto bottom = area.removeFromBottom (94);
                auto buttons = bottom.removeFromRight (184);
                favButton.setBounds (buttons.removeFromTop (28));
                buttons.removeFromTop (8);
                loadButton.setBounds (buttons.removeFromTop (28));
                buttons.removeFromTop (8);
                closeButton.setBounds (buttons.removeFromTop (28));
                list.setBounds (area);
            }

            void rebuildRows()
            {
                rows.clear();
                const auto* pack = processor.getPack();
                if (pack == nullptr)
                {
                    list.updateContent();
                    return;
                }

                const auto query = search.getText().trim().toLowerCase();
                const bool favoritesOnly = tagBox.getSelectedId() == 2;
                const juce::String tagFilter = tagBox.getSelectedId() >= 10
                    ? tagBox.getText().trim()
                    : juce::String();

                for (int index = 0; index < (int) pack->presets.size(); ++index)
                {
                    const auto& preset = pack->presets[(size_t) index];
                    const bool fav = favorites.count (preset.name.toStdString()) != 0;
                    if (favoritesOnly && ! fav)
                        continue;

                    if (tagFilter.isNotEmpty())
                    {
                        bool hasTag = preset.theme.equalsIgnoreCase (tagFilter);
                        for (const auto& tag : preset.tags)
                            hasTag = hasTag || tag.equalsIgnoreCase (tagFilter);
                        if (! hasTag)
                            continue;
                    }

                    if (query.isNotEmpty())
                    {
                        juce::String haystack = preset.name + " " + preset.description + " " + preset.theme + " " + preset.tags.joinIntoString (" ");
                        if (! haystack.toLowerCase().contains (query))
                            continue;
                    }

                    rows.add (index);
                }

                list.updateContent();
                if (! rows.isEmpty() && list.getSelectedRow() < 0)
                    list.selectRow (0, juce::dontSendNotification);
                repaint();
            }

            void updateDetails()
            {
                selectedDescription.clear();
                const auto* pack = processor.getPack();
                const int row = list.getSelectedRow();
                if (pack != nullptr && juce::isPositiveAndBelow (row, rows.size()))
                {
                    const int presetIndex = rows[row];
                    if (juce::isPositiveAndBelow (presetIndex, (int) pack->presets.size()))
                    {
                        const auto& preset = pack->presets[(size_t) presetIndex];
                        selectedDescription = preset.description;
                        favButton.setButtonText (favorites.count (preset.name.toStdString()) != 0 ? "Unfavorite" : "Favorite");
                    }
                }
                repaint();
            }

            void toggleFavorite()
            {
                const auto* pack = processor.getPack();
                const int row = list.getSelectedRow();
                if (pack == nullptr || ! juce::isPositiveAndBelow (row, rows.size()))
                    return;
                const int presetIndex = rows[row];
                if (! juce::isPositiveAndBelow (presetIndex, (int) pack->presets.size()))
                    return;
                const auto key = pack->presets[(size_t) presetIndex].name.toStdString();
                if (favorites.count (key) != 0)
                    favorites.erase (key);
                else
                    favorites.insert (key);
                updateDetails();
                list.repaint();
            }

            void loadSelected (bool explicitLoad)
            {
                const int row = list.getSelectedRow();
                if (! juce::isPositiveAndBelow (row, rows.size()))
                    return;
                const int presetIndex = rows[row];
                applyPreset (presetIndex);
                if ((explicitLoad || closeAfterLoadToggle.getToggleState()) && closeAfterLoadToggle.getToggleState())
                    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                        window->exitModalState (1);
            }

            PlayerProcessor& processor;
            std::set<std::string>& favorites;
            std::function<void (int)> applyPreset;
            std::function<void (bool, bool)> savePrefs;
            juce::TextEditor search;
            juce::ComboBox tagBox;
            juce::ToggleButton auditionToggle;
            juce::ToggleButton closeAfterLoadToggle;
            juce::TextButton favButton { "Favorite" };
            juce::TextButton loadButton { "Load" };
            juce::TextButton closeButton { "Close" };
            juce::ListBox list { "PresetList", this };
            juce::Array<int> rows;
            juce::StringArray allTags;
            juce::String selectedDescription;
        };

        auto applyPreset = [this] (int presetIndex)
        {
            const auto name = proc.getPresetName (presetIndex);
            showPresetLoading (name);
            if (proc.applyPresetByIndex (presetIndex))
            {
                refreshPresetControls();
                if (performancePanel != nullptr)
                    performancePanel->rebuild();
                if (controlCenter != nullptr)
                    controlCenter->rebuild();
                if (renderer != nullptr)
                    renderer->repaint();
            }
        };

        auto savePrefs = [this] (bool audition, bool closeAfterLoad)
        {
            presetAuditionOnSelect = audition;
            presetCloseAfterLoad = closeAfterLoad;
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Preset Browser";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.componentToCentreAround = this;
        options.content.setOwned (new PresetBrowser (proc, favoritePresetNames,
                                                     presetAuditionOnSelect,
                                                     presetCloseAfterLoad,
                                                     applyPreset,
                                                     savePrefs));
        options.launchAsync();
    }

    void PlayerEditor::showPresetLoading (const juce::String& presetName)
    {
        juce::String message = "LOADING PRESET";
        if (presetName.isNotEmpty())
            message << "  \u2022  " << presetName;

        presetLoadingLabel.setText (message, juce::dontSendNotification);
        presetLoadingLabel.setVisible (true);
        presetLoadingLabel.toFront (false);
        repaint (presetLoadingLabel.getBounds());

        juce::Component::SafePointer<PlayerEditor> safeThis (this);
        juce::Timer::callAfterDelay (900, [safeThis]
        {
            if (auto* editor = safeThis.getComponent())
            {
                editor->presetLoadingLabel.setVisible (false);
                editor->repaint();
            }
        });
    }

    void PlayerEditor::showAboutDialog()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr)
            return;

        struct AboutPanel final : public juce::Component
        {
            explicit AboutPanel (Manifest manifestIn)
                : manifest (std::move (manifestIn))
            {
                setSize (620, 440);

                auto wireUrlButton = [this] (juce::TextButton& button, const juce::String& label, const juce::String& url)
                {
                    button.setButtonText (label);
                    button.setVisible (url.isNotEmpty());
                    button.onClick = [url]
                    {
                        if (url.isNotEmpty())
                            juce::URL (url).launchInDefaultBrowser();
                    };
                    addAndMakeVisible (button);
                };

                wireUrlButton (supportButton, "Support", manifest.playerSupportUrl);
                wireUrlButton (manualButton, "Manual", manifest.playerManualUrl);
                wireUrlButton (storeButton, manifest.salesCtaText.isNotEmpty() ? manifest.salesCtaText : juce::String ("Store"),
                               manifest.salesCheckoutUrl.isNotEmpty()
                                    ? manifest.salesCheckoutUrl
                                    : (manifest.playerStoreUrl.isNotEmpty() ? manifest.playerStoreUrl : manifest.website));
                closeButton.setButtonText ("Close");
                closeButton.onClick = [this]
                {
                    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                        dialog->exitModalState (0);
                };
                addAndMakeVisible (closeButton);
            }

            void paint (juce::Graphics& graphics) override
            {
                graphics.fillAll (manifest.playerBackgroundColour);
                auto area = getLocalBounds().reduced (24);

                graphics.setColour (manifest.playerPanelColour);
                graphics.fillRoundedRectangle (area.toFloat(), 14.0f);
                graphics.setColour (manifest.playerAccentColour);
                graphics.drawRoundedRectangle (area.toFloat().reduced (0.5f), 14.0f, 1.6f);

                auto body = area.reduced (22);
                graphics.setColour (manifest.playerAccentColour);
                graphics.setFont (juce::FontOptions (22.0f).withStyle ("bold"));
                const auto title = manifest.playerDisplayName.isNotEmpty()
                    ? manifest.playerDisplayName
                    : manifest.instrumentName;
                graphics.drawText (title, body.removeFromTop (32), juce::Justification::centredLeft, true);

                graphics.setColour (manifest.playerTextDimColour);
                graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                juce::String subtitle;
                if (manifest.playerTagline.isNotEmpty())
                    subtitle << manifest.playerTagline;
                if (manifest.version.isNotEmpty())
                    subtitle << (subtitle.isNotEmpty() ? "  •  " : "") << "Version " << manifest.version;
                graphics.drawText (subtitle, body.removeFromTop (22), juce::Justification::centredLeft, true);
                body.removeFromTop (12);

                auto drawRow = [&] (const juce::String& label, const juce::String& value)
                {
                    if (value.isEmpty())
                        return;
                    auto row = body.removeFromTop (26);
                    graphics.setColour (manifest.playerTextDimColour);
                    graphics.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                    graphics.drawText (label, row.removeFromLeft (126), juce::Justification::centredLeft, true);
                    graphics.setColour (manifest.playerTextColour);
                    graphics.setFont (juce::FontOptions (12.0f));
                    graphics.drawText (value, row, juce::Justification::centredLeft, true);
                };

                drawRow ("Creator", manifest.creator);
                drawRow ("Built For", manifest.playerClientName);
                drawRow ("Website", manifest.website);
                drawRow ("Support Email", manifest.playerSupportEmail);
                drawRow ("License", manifest.licenseRequired
                    ? "License required" + (manifest.trialDays > 0 ? " / trial available" : juce::String())
                    : "No license required");
                body.removeFromTop (8);

                if (manifest.description.isNotEmpty() || manifest.playerLegalText.isNotEmpty())
                {
                    graphics.setColour (manifest.playerTextColour);
                    graphics.setFont (juce::FontOptions (12.0f));
                    graphics.drawFittedText (manifest.playerLegalText.isNotEmpty()
                                                ? manifest.playerLegalText
                                                : manifest.description,
                                             body.removeFromTop (78),
                                             juce::Justification::topLeft, 4);
                }

                if (manifest.playerCopyright.isNotEmpty())
                {
                    graphics.setColour (manifest.playerTextDimColour);
                    graphics.setFont (juce::FontOptions (10.5f));
                    graphics.drawText (manifest.playerCopyright,
                                       area.reduced (22).removeFromBottom (22),
                                       juce::Justification::centredLeft, true);
                }

                if (manifest.playerShowPatchCraftBranding
                    && manifest.whiteLabelPublisher.isNotEmpty()
                    && ! manifest.whiteLabelPublisher.equalsIgnoreCase ("PatchCraft"))
                {
                    graphics.setColour (manifest.playerTextDimColour.withAlpha (0.7f));
                    graphics.setFont (juce::FontOptions (10.5f));
                    graphics.drawText ("Powered by " + manifest.whiteLabelPublisher,
                                       area.reduced (22).removeFromBottom (22),
                                       juce::Justification::centredRight, true);
                }
            }

            void resized() override
            {
                auto buttons = getLocalBounds().reduced (46, 28).removeFromBottom (34);
                closeButton.setBounds (buttons.removeFromRight (86));
                buttons.removeFromRight (8);
                storeButton.setBounds (buttons.removeFromRight (86));
                buttons.removeFromRight (8);
                manualButton.setBounds (buttons.removeFromRight (86));
                buttons.removeFromRight (8);
                supportButton.setBounds (buttons.removeFromRight (86));
            }

            Manifest manifest;
            juce::TextButton supportButton { "Support" };
            juce::TextButton manualButton { "Manual" };
            juce::TextButton storeButton { "Store" };
            juce::TextButton closeButton { "Close" };
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "About / Support";
        options.dialogBackgroundColour = pack->manifest.playerBackgroundColour;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.componentToCentreAround = this;
        options.content.setOwned (new AboutPanel (pack->manifest));
        options.launchAsync();
    }

    void PlayerEditor::refreshPresetControls()
    {
        const int count = proc.getPresetCount();
        const int current = proc.getCurrentPresetIndex();
        const bool hasPresets = count > 0 && current >= 0;

        prevPresetBtn.setEnabled (hasPresets && count > 1);
        nextPresetBtn.setEnabled (hasPresets && count > 1);
        presetBtn.setEnabled (hasPresets);

        juce::String label = hasPresets ? proc.getPresetName (current) : "No Presets";
        if (label.isEmpty())
            label = "Preset " + juce::String (current + 1);
        if (label.length() > 28)
            label = label.substring (0, 25) + "...";
        presetBtn.setButtonText (label);
    }

} // namespace patchcraft
