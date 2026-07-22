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

        static juce::Colour alphaColour (juce::Colour colour, float alpha)
        {
            return colour.withAlpha (juce::jlimit (0.0f, 1.0f, alpha));
        }

        static void drawPlayerTitleTheme (juce::Graphics& g,
                                          juce::Rectangle<int> chrome,
                                          const Manifest& manifest,
                                          juce::Colour panel,
                                          juce::Colour bg,
                                          juce::Colour accent,
                                          juce::Colour border)
        {
            const auto theme = manifest.playerTitleBarTheme;
            if (theme.equalsIgnoreCase ("no-chrome") || chrome.isEmpty())
                return;

            auto rect = chrome.toFloat();
            const auto darkPanel = panel.darker (0.18f);
            const auto glow = accent.withAlpha (0.18f);

            if (theme.equalsIgnoreCase ("minimal"))
            {
                g.setColour (accent.withAlpha (0.65f));
                g.fillRoundedRectangle (rect.withHeight (2.0f), 1.0f);
                g.setColour (border.withAlpha (0.42f));
                g.fillRect (chrome.withHeight (1).withY (chrome.getBottom() - 1));
                return;
            }

            if (theme.equalsIgnoreCase ("glass"))
            {
                juce::ColourGradient gradient (panel.withAlpha (0.62f), rect.getX(), rect.getY(),
                                               bg.withAlpha (0.20f), rect.getRight(), rect.getBottom(), false);
                g.setGradientFill (gradient);
                g.fillRoundedRectangle (rect, 13.0f);
                g.setColour (juce::Colours::white.withAlpha (0.13f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 13.0f, 1.2f);
                g.setColour (accent.withAlpha (0.42f));
                g.fillRoundedRectangle (rect.reduced (10.0f, 6.0f).withHeight (1.5f), 0.75f);
                return;
            }

            if (theme.equalsIgnoreCase ("neon-strip"))
            {
                g.setColour (bg.withAlpha (0.35f));
                g.fillRoundedRectangle (rect, 9.0f);
                g.setColour (accent.withAlpha (0.88f));
                g.fillRoundedRectangle (rect.withHeight (3.0f), 1.5f);
                g.setColour (accent.contrasting (0.30f).withAlpha (0.50f));
                g.fillRoundedRectangle (rect.withTrimmedTop (rect.getHeight() - 3.0f), 1.5f);
                g.setColour (accent.withAlpha (0.55f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 9.0f, 1.0f);
                return;
            }

            if (theme.equalsIgnoreCase ("split-brand"))
            {
                g.setColour (darkPanel.withAlpha (0.95f));
                g.fillRoundedRectangle (rect, 10.0f);
                auto brand = rect.withWidth (juce::jmin (rect.getWidth() * 0.34f, 380.0f));
                g.setColour (accent.withAlpha (0.25f));
                g.fillRoundedRectangle (brand, 10.0f);
                g.setColour (accent.withAlpha (0.95f));
                g.fillRoundedRectangle (brand.withWidth (4.0f), 2.0f);
                g.setColour (border.withAlpha (0.72f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 10.0f, 1.0f);
                return;
            }

            if (theme.equalsIgnoreCase ("logo-rail"))
            {
                g.setColour (darkPanel.withAlpha (0.96f));
                g.fillRoundedRectangle (rect, 8.0f);
                g.setColour (accent.withAlpha (0.90f));
                g.fillRoundedRectangle (rect.withWidth (70.0f), 8.0f);
                g.setColour (border.withAlpha (0.65f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 8.0f, 1.0f);
                return;
            }

            if (theme.equalsIgnoreCase ("compact-daw") || theme.equalsIgnoreCase ("dark-utility"))
            {
                g.setColour (panel.withAlpha (0.98f));
                g.fillRoundedRectangle (rect, 5.0f);
                g.setColour (border.withAlpha (0.78f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 5.0f, 1.0f);
                g.setColour (accent.withAlpha (0.72f));
                g.fillRect (chrome.reduced (8, 0).withHeight (2));
                return;
            }

            if (theme.equalsIgnoreCase ("clean-pro"))
            {
                g.setColour (panel.withAlpha (0.94f));
                g.fillRoundedRectangle (rect, 12.0f);
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.fillRoundedRectangle (rect.reduced (1.0f).withHeight (rect.getHeight() * 0.48f), 11.0f);
                g.setColour (border.withAlpha (0.82f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 12.0f, 1.0f);
                return;
            }

            if (theme.equalsIgnoreCase ("artist-card") || theme.equalsIgnoreCase ("banner") || theme.equalsIgnoreCase ("wide-banner"))
            {
                juce::ColourGradient gradient (panel.withAlpha (0.96f), rect.getX(), rect.getY(),
                                               accent.withAlpha (0.22f), rect.getRight(), rect.getBottom(), false);
                g.setGradientFill (gradient);
                g.fillRoundedRectangle (rect, 14.0f);
                g.setColour (accent.withAlpha (0.80f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 14.0f, 1.2f);
                g.setColour (glow);
                g.fillRoundedRectangle (rect.reduced (12.0f, 9.0f).removeFromRight (rect.getWidth() * 0.34f), 8.0f);
                return;
            }

            if (theme.equalsIgnoreCase ("bottom-tools"))
            {
                g.setColour (darkPanel.withAlpha (0.98f));
                g.fillRoundedRectangle (rect, 10.0f);
                g.setColour (accent.withAlpha (0.74f));
                g.fillRoundedRectangle (rect.withY (rect.getBottom() - 4.0f).withHeight (3.0f), 1.5f);
                g.setColour (border.withAlpha (0.72f));
                g.drawRoundedRectangle (rect.reduced (0.5f), 10.0f, 1.0f);
                return;
            }

            juce::ColourGradient gradient (panel.withAlpha (0.96f), rect.getX(), rect.getY(),
                                           bg.withAlpha (0.78f), rect.getRight(), rect.getBottom(), false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (rect, 10.0f);
            g.setColour (border.withAlpha (0.72f));
            g.drawRoundedRectangle (rect.reduced (0.5f), 10.0f, 1.0f);
            g.setColour (accent.withAlpha (0.78f));
            g.fillRoundedRectangle (rect.withHeight (2.0f), 1.0f);
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

    // =========================================================================
    // PREMIUM UNIFIED PLAYER COMPONENTS
    // =========================================================================



    class PlayerLeftSidebar : public juce::Component,
                              public juce::TextEditor::Listener,
                              private juce::ListBoxModel
    {
    public:
        explicit PlayerLeftSidebar (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            libTab.setButtonText ("LIBRARY");
            libTab.setClickingTogglesState (true);
            libTab.setToggleState (true, juce::dontSendNotification);
            libTab.onClick = [this] {
                libTab.setToggleState (true, juce::dontSendNotification);
                favTab.setToggleState (false, juce::dontSendNotification);
                showFavorites = false;
                refreshList();
            };
            addAndMakeVisible (libTab);

            favTab.setButtonText ("FAVORITES");
            favTab.setClickingTogglesState (true);
            favTab.onClick = [this] {
                favTab.setToggleState (true, juce::dontSendNotification);
                libTab.setToggleState (false, juce::dontSendNotification);
                showFavorites = true;
                refreshList();
            };
            addAndMakeVisible (favTab);

            searchEditor.setTextToShowWhenEmpty ("Search presets...", juce::Colour (0xff505866));
            searchEditor.addListener (this);
            addAndMakeVisible (searchEditor);

            categories = { "All", "Pads", "Keys", "Leads", "Basses", "Plucks", "Strings", "Percussion" };
            for (int i = 0; i < categories.size(); ++i)
            {
                auto* btn = new juce::TextButton (categories[i]);
                btn->setClickingTogglesState (true);
                if (i == 0) btn->setToggleState (true, juce::dontSendNotification);
                btn->onClick = [this, btn, i] {
                    for (auto* cb : catButtons) cb->setToggleState (false, juce::dontSendNotification);
                    btn->setToggleState (true, juce::dontSendNotification);
                    activeCategory = categories[i];
                    refreshList();
                };
                catButtons.add (btn);
                addAndMakeVisible (btn);
            }

            presetListBox.setModel (this);
            addAndMakeVisible (presetListBox);

            presetCardName.setFont (juce::Font (13.0f, juce::Font::bold));
            presetCardName.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (presetCardName);

            presetCardDetails.setText ("Size: 19 MB", juce::dontSendNotification);
            presetCardDetails.setFont (juce::Font (10.0f));
            presetCardDetails.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (presetCardDetails);

            refreshList();
        }

        ~PlayerLeftSidebar() override
        {
            catButtons.clear();
        }

        void selectCategory (const juce::String& name)
        {
            for (int i = 0; i < categories.size(); ++i)
            {
                if (categories[i].equalsIgnoreCase (name))
                {
                    for (auto* cb : catButtons) cb->setToggleState (false, juce::dontSendNotification);
                    catButtons[i]->setToggleState (true, juce::dontSendNotification);
                    activeCategory = categories[i];
                    refreshList();
                    break;
                }
            }
        }

        void textEditorTextChanged (juce::TextEditor&) override
        {
            refreshList();
        }

        void refreshList()
        {
            filteredPresets.clear();
            const auto allPresets = editor.proc.getPresetNames();
            const auto searchString = searchEditor.getText();

            int selectedRow = -1;
            const auto currentIdx = editor.proc.getCurrentPresetIndex();
            const auto currentName = currentIdx >= 0 ? editor.proc.getPresetName (currentIdx) : "";

            for (int i = 0; i < allPresets.size(); ++i)
            {
                const auto name = allPresets[i];
                if (showFavorites && editor.favoritePresetNames.count (name.toStdString()) == 0)
                    continue;

                if (searchString.isNotEmpty() && ! name.containsIgnoreCase (searchString))
                    continue;

                if (activeCategory != "All")
                {
                    if (activeCategory == "Pads" && ! name.containsIgnoreCase ("pad") && ! name.containsIgnoreCase ("shimmer"))
                        continue;
                    if (activeCategory == "Keys" && ! name.containsIgnoreCase ("key") && ! name.containsIgnoreCase ("piano"))
                        continue;
                    if (activeCategory == "Leads" && ! name.containsIgnoreCase ("lead"))
                        continue;
                    if (activeCategory == "Basses" && ! name.containsIgnoreCase ("bass"))
                        continue;
                }

                if (name == currentName)
                    selectedRow = filteredPresets.size();

                filteredPresets.add (name);
            }

            presetListBox.updateContent();
            if (selectedRow >= 0)
                presetListBox.selectRow (selectedRow, false, false);
            else
                presetListBox.deselectAllRows();

            presetListBox.repaint();

            const auto idx = editor.proc.getCurrentPresetIndex();
            if (idx >= 0)
                presetCardName.setText (editor.proc.getPresetName (idx), juce::dontSendNotification);
            else
                presetCardName.setText ("--", juce::dontSendNotification);
        }

        int getNumRows() override { return filteredPresets.size(); }

        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= filteredPresets.size())
                return;

            if (rowIsSelected)
            {
                g.setColour (editor.getAccentColor().withAlpha (0.22f));
                g.fillRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f);
                g.setColour (editor.getAccentColor());
                g.drawRoundedRectangle (2.5f, 2.5f, (float) width - 5.0f, (float) height - 5.0f, 4.0f, 1.0f);
            }
            else
            {
                g.setColour (editor.getPanelColor().withAlpha (0.35f));
                g.fillRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f);
            }

            g.setFont (juce::Font (12.0f));
            g.setColour (rowIsSelected ? editor.getTextColor() : editor.getTextDimColor());
            g.drawText (filteredPresets[rowNumber], 8, 0, width - 16, height, juce::Justification::centredLeft, true);
        }

        void selectedRowsChanged (int lastRowSelected) override
        {
            if (lastRowSelected >= 0 && lastRowSelected < filteredPresets.size())
            {
                const auto name = filteredPresets[lastRowSelected];
                juce::MessageManager::callAsync ([this, name]
                {
                    const auto allPresets = editor.proc.getPresetNames();
                    const int realIdx = allPresets.indexOf (name);
                    if (realIdx >= 0 && realIdx != editor.proc.getCurrentPresetIndex())
                    {
                        editor.proc.applyPresetByIndex (realIdx);
                        editor.packChanged();
                    }
                });
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (editor.getBgColor());
            
            g.setColour (editor.getBorderColor());
            g.fillRect (getWidth() - 1, 0, 1, getHeight());

            auto cardArea = getLocalBounds().removeFromBottom (60).reduced (10, 4);
            g.setColour (editor.getPanelColor());
            g.fillRoundedRectangle (cardArea.toFloat(), 6.0f);
            g.setColour (editor.getBorderColor());
            g.drawRoundedRectangle (cardArea.toFloat(), 6.0f, 1.0f);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            
            auto tabLine = bounds.removeFromTop (36).reduced (10, 4);
            libTab.setBounds (tabLine.removeFromLeft (tabLine.getWidth() / 2 - 2));
            favTab.setBounds (tabLine.removeFromRight (tabLine.getWidth()));

            searchEditor.setBounds (bounds.removeFromTop (28).reduced (10, 2));

            auto catArea = bounds.removeFromTop (72).reduced (10, 4);
            const int cellW = catArea.getWidth() / 4;
            const int cellH = catArea.getHeight() / 2;
            for (int i = 0; i < catButtons.size(); ++i)
            {
                const int row = i / 4;
                const int col = i % 4;
                catButtons[i]->setBounds (catArea.getX() + col * cellW + 1, catArea.getY() + row * cellH + 1, cellW - 2, cellH - 2);
            }

            bounds.removeFromBottom (60);
            presetListBox.setBounds (bounds.reduced (10, 4));

            auto cardArea = getLocalBounds().removeFromBottom (60).reduced (10, 4);
            presetCardName.setBounds (cardArea.getX() + 10, cardArea.getY() + 10, cardArea.getWidth() - 20, 18);
            presetCardDetails.setBounds (cardArea.getX() + 10, cardArea.getY() + 28, cardArea.getWidth() - 20, 14);
        }

    private:
        PlayerEditor& editor;
        juce::TextButton libTab;
        juce::TextButton favTab;
        juce::TextEditor searchEditor;
        juce::ListBox presetListBox;
        
        juce::Label presetCardName;
        juce::Label presetCardDetails;

        juce::StringArray categories;
        juce::OwnedArray<juce::TextButton> catButtons;
        juce::StringArray filteredPresets;
        juce::String activeCategory = "All";
        bool showFavorites = false;
    };

    class PlayerTopBar : public juce::Component,
                         private juce::Timer
    {
    public:
        explicit PlayerTopBar (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            // logo
            logoLabel.setText ("PATCHCRAFT", juce::dontSendNotification);
            logoLabel.setFont (juce::Font (16.0f, juce::Font::bold));
            logoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
            addAndMakeVisible (logoLabel);

            // preset name & bank info
            presetNameLabel.setFont (juce::Font (14.0f, juce::Font::bold));
            presetNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
            addAndMakeVisible (presetNameLabel);

            bankNameLabel.setFont (juce::Font (10.0f));
            bankNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (bankNameLabel);

            // browse and save buttons
            browseBtn.setButtonText ("Browse");
            browseBtn.onClick = [this] { editor.toggleLibrary(); };
            addAndMakeVisible (browseBtn);

            saveBtn.setButtonText ("Save");
            saveBtn.onClick = [this] {
                juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
                    "Save Preset",
                    "Do you want to save the current settings as a new preset?",
                    "Save", "Cancel", this,
                    juce::ModalCallbackFunction::create ([this] (int result) {
                        if (result != 0) {
                            const auto name = "User Preset " + juce::String (editor.proc.getPresetCount() + 1);
                            editor.proc.saveUserSnapshot (name, false);
                            editor.packChanged();
                        }
                    }));
            };
            addAndMakeVisible (saveBtn);

            // settings gear
            settingsBtn.setButtonText ("S");
            settingsBtn.setTooltip ("Global Settings");
            settingsBtn.onClick = [this] { editor.showAboutDialog(); };
            addAndMakeVisible (settingsBtn);

            // category dropdown
            categoryBox.addItem ("All", 1);
            categoryBox.addItem ("Pads", 2);
            categoryBox.addItem ("Keys", 3);
            categoryBox.addItem ("Leads", 4);
            categoryBox.setSelectedId (1);
            categoryBox.onChange = [this] {
                if (editor.leftSidebar != nullptr)
                    editor.leftSidebar->selectCategory (categoryBox.getText());
            };
            addAndMakeVisible (categoryBox);

            // prev / next preset buttons
            prevBtn.setButtonText ("<");
            prevBtn.onClick = [this] {
                editor.proc.applyPresetOffset (-1);
                editor.packChanged();
            };
            addAndMakeVisible (prevBtn);

            nextBtn.setButtonText (">");
            nextBtn.onClick = [this] {
                editor.proc.applyPresetOffset (1);
                editor.packChanged();
            };
            addAndMakeVisible (nextBtn);

            // favorite star
            favBtn.setButtonText ("*");
            favBtn.setClickingTogglesState (true);
            favBtn.onClick = [this] {
                const auto name = editor.proc.getPresetName (editor.proc.getCurrentPresetIndex());
                if (favBtn.getToggleState())
                    editor.favoritePresetNames.insert (name.toStdString());
                else
                    editor.favoritePresetNames.erase (name.toStdString());
                if (editor.leftSidebar != nullptr)
                    editor.leftSidebar->refreshList();
            };
            addAndMakeVisible (favBtn);

            // volume & output meters
            volSlider.setSliderStyle (juce::Slider::LinearBar);
            volSlider.setRange (0.0, 1.25, 0.01);
            volSlider.setValue (1.0);
            volSlider.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("volume", (float) volSlider.getValue());
            };
            addAndMakeVisible (volSlider);

            volLabel.setText ("VOL", juce::dontSendNotification);
            volLabel.setFont (juce::Font (10.0f));
            volLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (volLabel);

            startTimerHz (24);
            updatePresetCard();
        }

        ~PlayerTopBar() override
        {
            stopTimer();
        }

        void updatePresetCard()
        {
            presetNameLabel.setColour (juce::Label::textColourId, editor.getTextColor());
            bankNameLabel.setColour (juce::Label::textColourId, editor.getTextDimColor());
            logoLabel.setColour (juce::Label::textColourId, editor.getTextColor());

            const auto* pack = editor.proc.getPack();
            const auto* manifest = pack != nullptr ? &pack->manifest : nullptr;
            const bool hideTitleText = manifest != nullptr
                                    && manifest->playerTitleTextPlacement.equalsIgnoreCase ("hidden");
            logoLabel.setVisible (! hideTitleText);
            bankNameLabel.setVisible (! hideTitleText);

            if (manifest != nullptr)
            {
                logoLabel.setText (playerInstrumentName (pack), juce::dontSendNotification);
                const auto subtitle = manifest->playerTagline.isNotEmpty()
                    ? manifest->playerTagline
                    : (manifest->creator.isNotEmpty() ? manifest->creator : manifest->instrumentName);
                bankNameLabel.setText (subtitle, juce::dontSendNotification);
                logoLabel.setFont (playerChromeFont (manifest->playerTitleFontFamily, 16.0f, true));
                bankNameLabel.setFont (playerChromeFont (manifest->playerTitleFontFamily, 10.0f, false));
            }
            else
            {
                logoLabel.setText ("PATCHCRAFT", juce::dontSendNotification);
                bankNameLabel.setText ("PatchCraft", juce::dontSendNotification);
            }

            const auto currentIdx = editor.proc.getCurrentPresetIndex();
            if (currentIdx >= 0)
            {
                presetNameLabel.setText (editor.proc.getPresetName (currentIdx), juce::dontSendNotification);
                const auto name = editor.proc.getPresetName (currentIdx).toStdString();
                favBtn.setToggleState (editor.favoritePresetNames.count (name) > 0, juce::dontSendNotification);
            }
            else
            {
                presetNameLabel.setText ("No Preset Loaded", juce::dontSendNotification);
                bankNameLabel.setText ("--", juce::dontSendNotification);
            }
        }

        void updateChromeVisibility()
        {
            const auto* pack = editor.proc.getPack();
            const auto* manifest = pack != nullptr ? &pack->manifest : nullptr;

            const bool showBrowse = manifest == nullptr || (manifest->playerTopShowBrowse
                                && manifest->playerShowLibraryBrowser
                                && manifest->playerShowPackMenu
                                && manifest->playerAllowPackLoading);
            const bool showSave = manifest == nullptr || manifest->playerTopShowSave;
            const bool showSettings = manifest == nullptr || (manifest->playerTopShowSettings && manifest->playerShowAbout);
            const bool showCategory = manifest == nullptr || manifest->playerTopShowCategory;
            const bool showFavorite = manifest == nullptr || manifest->playerTopShowFavorite;
            const bool showPresetNav = manifest == nullptr || manifest->playerTopShowPresetNav;
            const bool showMaster = manifest == nullptr || manifest->playerTopShowMasterVolume;
            showOutputMeter = manifest == nullptr || manifest->playerTopShowOutputMeter;

            browseBtn.setVisible (showBrowse);
            saveBtn.setVisible (showSave);
            settingsBtn.setVisible (showSettings);
            categoryBox.setVisible (showCategory);
            favBtn.setVisible (showFavorite);
            prevBtn.setVisible (showPresetNav);
            nextBtn.setVisible (showPresetNav);
            volSlider.setVisible (showMaster);
            volLabel.setVisible (showMaster);

            const auto buttonStyle = manifest != nullptr ? manifest->playerTitleButtonStyle : juce::String ("outlined");
            auto applyButtonStyle = [&] (juce::TextButton& button)
            {
                const auto accent = editor.getAccentColor();
                const auto panel = editor.getPanelColor();
                const auto text = editor.getTextColor();
                if (buttonStyle.equalsIgnoreCase ("filled"))
                {
                    button.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.86f));
                    button.setColour (juce::TextButton::buttonOnColourId, accent.brighter (0.12f));
                    button.setColour (juce::TextButton::textColourOffId, editor.getBgColor().contrasting (0.82f));
                    button.setColour (juce::TextButton::textColourOnId, editor.getBgColor().contrasting (0.95f));
                }
                else if (buttonStyle.equalsIgnoreCase ("minimal"))
                {
                    button.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
                    button.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.16f));
                    button.setColour (juce::TextButton::textColourOffId, text);
                    button.setColour (juce::TextButton::textColourOnId, accent.brighter (0.20f));
                }
                else
                {
                    button.setColour (juce::TextButton::buttonColourId, panel.withAlpha (0.82f));
                    button.setColour (juce::TextButton::buttonOnColourId, panel.brighter (0.12f));
                    button.setColour (juce::TextButton::textColourOffId, text);
                    button.setColour (juce::TextButton::textColourOnId, text.brighter (0.18f));
                }
            };

            for (auto* button : { &browseBtn, &saveBtn, &settingsBtn, &prevBtn, &nextBtn, &favBtn })
                applyButtonStyle (*button);
            categoryBox.setColour (juce::ComboBox::backgroundColourId, editor.getPanelColor().withAlpha (0.82f));
            categoryBox.setColour (juce::ComboBox::textColourId, editor.getTextColor());
            categoryBox.setColour (juce::ComboBox::outlineColourId, editor.getBorderColor());
            categoryBox.setColour (juce::ComboBox::arrowColourId, editor.getAccentColor());

            resized();
            repaint();
        }

        void timerCallback() override
        {
            const float peak = editor.proc.getOutputPeak();
            leftPeak = juce::jlimit (0.0f, 1.0f, peak);
            rightPeak = juce::jlimit (0.0f, 1.0f, peak * 0.95f + 0.05f * (float) std::rand() / RAND_MAX);
            if (showOutputMeter)
                repaint (meterArea);
        }

        void paint (juce::Graphics& g) override
        {
            const auto* pack = editor.proc.getPack();
            const auto* manifest = pack != nullptr ? &pack->manifest : nullptr;
            const auto chrome = playerChromeBoundsFor (getLocalBounds(), pack).reduced (10, 8);
            if (manifest != nullptr)
            {
                const auto theme = manifest->playerTitleBarTheme;
                const bool noChrome = theme.equalsIgnoreCase ("no-chrome");
                bool drewBanner = false;
                if (! noChrome && titleThemeUsesBannerArtwork (theme) && manifest->playerTitleBannerImage.isNotEmpty())
                {
                    const auto file = juce::File::isAbsolutePath (manifest->playerTitleBannerImage)
                        ? juce::File (manifest->playerTitleBannerImage)
                        : pack->rootFolder.getChildFile (manifest->playerTitleBannerImage);
                    if (const auto banner = editor.assets.loadImage (file); banner.isValid())
                    {
                        g.drawImage (banner, chrome.toFloat());
                        drewBanner = true;
                    }
                }

                if (! drewBanner)
                    drawPlayerTitleTheme (g, chrome, *manifest, editor.getPanelColor(), editor.getBgColor(),
                                          editor.getAccentColor(), editor.getBorderColor());
            }

            auto logoRect = juce::Rectangle<int> (12, 13, kPlayerTitleBarArtworkSize, kPlayerTitleBarArtworkSize).reduced (5);
            bool drewLogoImage = false;
            if (manifest != nullptr && manifest->playerLogoImage.isNotEmpty())
            {
                const auto file = juce::File::isAbsolutePath (manifest->playerLogoImage)
                    ? juce::File (manifest->playerLogoImage)
                    : pack->rootFolder.getChildFile (manifest->playerLogoImage);
                if (const auto logo = editor.assets.loadImage (file); logo.isValid())
                {
                    g.drawImageWithin (logo, logoRect.getX(), logoRect.getY(), logoRect.getWidth(), logoRect.getHeight(),
                                       juce::RectanglePlacement::centred);
                    drewLogoImage = true;
                }
            }

            if (! drewLogoImage)
            {
                const auto diamondRect = logoRect.toFloat().reduced (4.0f);
                g.setColour (editor.getAccentColor());
                juce::Path diamond;
                diamond.startNewSubPath (diamondRect.getCentreX(), diamondRect.getY());
                diamond.lineTo (diamondRect.getRight(), diamondRect.getCentreY());
                diamond.lineTo (diamondRect.getCentreX(), diamondRect.getBottom());
                diamond.lineTo (diamondRect.getX(), diamondRect.getCentreY());
                diamond.closeSubPath();
                g.fillPath (diamond);
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.strokePath (diamond, juce::PathStrokeType (1.5f));
            }

            if (showOutputMeter)
            {
                g.setColour (editor.getBgColor());
                g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);

                const int totalBars = 10;
                const int barH = 3;
                const int gapY = 2;
                const int xL = meterArea.getX() + 4;
                const int xR = meterArea.getRight() - 10;
                const int barW = 6;

                auto drawLedBar = [&] (int x, float val) {
                    const int activeCount = juce::roundToInt (val * totalBars);
                    for (int i = 0; i < totalBars; ++i) {
                        const int y = meterArea.getBottom() - 4 - (i + 1) * (barH + gapY);
                        if (i < activeCount) {
                            if (i < 6) g.setColour (juce::Colour (0xff5fb37b));
                            else if (i < 8) g.setColour (juce::Colour (0xffe6a13f));
                            else g.setColour (juce::Colour (0xffe94560));
                        } else {
                            g.setColour (editor.getBorderColor().brighter (0.05f));
                        }
                        g.fillRect (x, y, barW, barH);
                    }
                };

                drawLedBar (xL, leftPeak);
                drawLedBar (xR, rightPeak);
            }
        }

        void resized() override
        {
            const auto* pack = editor.proc.getPack();
            const auto* manifest = pack != nullptr ? &pack->manifest : nullptr;
            const auto chrome = playerChromeBoundsFor (getLocalBounds(), pack).reduced (12, 8);
            const auto placement = manifest != nullptr ? manifest->playerTitleTextPlacement : juce::String ("left");
            const auto theme = manifest != nullptr ? manifest->playerTitleBarTheme : juce::String ("classic");
            const int brandW = playerTitleBrandWidth (theme, chrome.getWidth());
            int brandX = chrome.getX() + 44;
            if (placement.equalsIgnoreCase ("center"))
                brandX = chrome.getCentreX() - brandW / 2;
            else if (placement.equalsIgnoreCase ("right"))
                brandX = chrome.getRight() - brandW - 12;
            const int minBrandX = chrome.getX() + 44;
            const int maxBrandX = juce::jmax (minBrandX, chrome.getRight() - juce::jmax (80, brandW));
            brandX = juce::jlimit (minBrandX, maxBrandX, brandX);

            logoLabel.setBounds (brandX, chrome.getY() + 8, juce::jmax (110, brandW), 22);
            bankNameLabel.setBounds (brandX, chrome.getY() + 31, juce::jmax (110, brandW), 16);

            const int cardX = juce::jlimit (brandX + 160, chrome.getRight() - 260, brandX + brandW + 16);
            presetNameLabel.setBounds (cardX, chrome.getY() + 7, 220, 22);

            const int rightBoundary = getWidth() - 10;
            int rightReserved = 10;
            if (showOutputMeter)
            {
                meterArea = juce::Rectangle<int> (rightBoundary - 30, 8, 20, 56);
                rightReserved += 42;
            }
            else
            {
                meterArea = {};
            }

            if (volSlider.isVisible())
            {
                volSlider.setBounds (rightBoundary - rightReserved - 90, 26, 80, 20);
                volLabel.setBounds (rightBoundary - rightReserved - 125, 26, 30, 20);
                rightReserved += 128;
            }

            int x = cardX + 225;
            auto placeButton = [&x] (juce::Component& c, int width)
            {
                if (! c.isVisible())
                    return;
                c.setBounds (x, 26, width, 20);
                x += width + 5;
            };

            placeButton (favBtn, 20);
            placeButton (prevBtn, 20);
            placeButton (nextBtn, 20);

            x = juce::jmax (x + 14, 500);
            const int maxRight = rightBoundary - rightReserved - 8;
            auto placeWide = [&x, maxRight] (juce::Component& c, int width)
            {
                if (! c.isVisible())
                    return;
                if (x + width > maxRight)
                {
                    c.setBounds (0, 0, 0, 0);
                    return;
                }
                c.setBounds (x, 24, width, 24);
                x += width + 8;
            };

            placeWide (categoryBox, 100);
            placeWide (browseBtn, 84);
            placeWide (saveBtn, 64);
            placeWide (settingsBtn, 32);
        }

    private:
        PlayerEditor& editor;
        juce::Label logoLabel;
        juce::Label presetNameLabel;
        juce::Label bankNameLabel;
        juce::TextButton browseBtn;
        juce::TextButton saveBtn;
        juce::TextButton settingsBtn;
        juce::TextButton prevBtn;
        juce::TextButton nextBtn;
        juce::TextButton favBtn;
        juce::ComboBox categoryBox;
        juce::Slider volSlider;
        juce::Label volLabel;

        juce::Rectangle<int> meterArea;
        float leftPeak = 0.0f;
        float rightPeak = 0.0f;
        bool showOutputMeter = true;
    };

    class PlayerCenterPanel : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit PlayerCenterPanel (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            struct KnobSetup { juce::Slider* slider; juce::Label* label; juce::String name; juce::uint32 color; };
            const KnobSetup setups[] = {
                { &attackSlider, &attackLabel, "ATTACK", 0xff2bbdc7 },
                { &releaseSlider, &releaseLabel, "RELEASE", 0xff2b8ac7 },
                { &filterSlider, &filterLabel, "FILTER", 0xfff5a623 },
                { &spaceSlider, &spaceLabel, "SPACE", 0xff6c7ed6 },
                { &motionSlider, &motionLabel, "MOTION", 0xff8e6cd6 },
                { &textureSlider, &textureLabel, "TEXTURE", 0xffd66cbd },
                { &volumeSlider, &volumeLabel, "VOLUME", 0xff5fb37b },
                { &panSlider, &panLabel, "PAN", 0xff9da5b3 }
            };

            for (const auto& s : setups)
            {
                s.slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s.slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s.slider->getProperties().set ("accentColor", (int) s.color);
                s.slider->setRange (0.0, 1.0, 0.001);
                s.slider->setValue (0.5);
                addAndMakeVisible (s.slider);

                s.label->setText (s.name, juce::dontSendNotification);
                s.label->setFont (juce::Font (10.0f, juce::Font::bold));
                s.label->setJustificationType (juce::Justification::centred);
                s.label->setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
                addAndMakeVisible (s.label);
            }

            attackSlider.onValueChange  = [this] { setParam ("attack", attackSlider.getValue()); };
            releaseSlider.onValueChange = [this] { setParam ("release", releaseSlider.getValue()); };
            filterSlider.onValueChange  = [this] { setParam ("filterCutoff", filterSlider.getValue()); };
            spaceSlider.onValueChange   = [this] { setParam ("reverbMix", spaceSlider.getValue()); };
            motionSlider.onValueChange  = [this] { setParam ("chorusMix", motionSlider.getValue()); };
            textureSlider.onValueChange = [this] { setParam ("lofiMix", textureSlider.getValue()); };
            volumeSlider.onValueChange  = [this] { setParam ("volume", volumeSlider.getValue()); };
            panSlider.onValueChange     = [this] { setParam ("pan", panSlider.getValue()); };

            const juce::String tabNames[] = { "MAIN", "LAYERS", "FX", "MOD", "ARP" };
            for (int i = 0; i < 5; ++i)
            {
                auto* btn = new juce::TextButton (tabNames[i]);
                btn->setClickingTogglesState (true);
                if (i == 0) btn->setToggleState (true, juce::dontSendNotification);
                btn->onClick = [this, btn, i] {
                    for (auto* tb : tabButtons) tb->setToggleState (false, juce::dontSendNotification);
                    btn->setToggleState (true, juce::dontSendNotification);
                    activeTabIdx = i;
                    updateVisibility();
                };
                tabButtons.add (btn);
                addAndMakeVisible (btn);
            }

            pitchWheel.setSliderStyle (juce::Slider::LinearVertical);
            pitchWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            pitchWheel.setRange (-1.0, 1.0, 0.01);
            pitchWheel.setValue (0.0);
            pitchWheel.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("pitchWheel", (float) pitchWheel.getValue());
            };
            addAndMakeVisible (pitchWheel);

            modWheel.setSliderStyle (juce::Slider::LinearVertical);
            modWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            modWheel.setRange (0.0, 1.0, 0.01);
            modWheel.setValue (0.0);
            modWheel.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("modWheel", (float) modWheel.getValue());
            };
            addAndMakeVisible (modWheel);

            pitchLabel.setText ("PITCH", juce::dontSendNotification);
            pitchLabel.setFont (juce::Font (8.0f));
            pitchLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (pitchLabel);

            modLabel.setText ("MOD", juce::dontSendNotification);
            modLabel.setFont (juce::Font (8.0f));
            modLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (modLabel);

            startTimerHz (30);
            updateKnobsFromProcessor();
            updateColours();
        }

        void updateColours()
        {
            auto txtDim = editor.getTextDimColor();

            attackLabel.setColour (juce::Label::textColourId, txtDim);
            releaseLabel.setColour (juce::Label::textColourId, txtDim);
            filterLabel.setColour (juce::Label::textColourId, txtDim);
            spaceLabel.setColour (juce::Label::textColourId, txtDim);
            motionLabel.setColour (juce::Label::textColourId, txtDim);
            textureLabel.setColour (juce::Label::textColourId, txtDim);
            volumeLabel.setColour (juce::Label::textColourId, txtDim);
            panLabel.setColour (juce::Label::textColourId, txtDim);

            pitchLabel.setColour (juce::Label::textColourId, txtDim);
            modLabel.setColour (juce::Label::textColourId, txtDim);

            repaint();
        }

        ~PlayerCenterPanel() override
        {
            stopTimer();
            tabButtons.clear();
        }

        void updateKnobsFromProcessor()
        {
            attackSlider.setValue (getParam ("attack"), juce::dontSendNotification);
            releaseSlider.setValue (getParam ("release"), juce::dontSendNotification);
            filterSlider.setValue (getParam ("filterCutoff"), juce::dontSendNotification);
            spaceSlider.setValue (getParam ("reverbMix"), juce::dontSendNotification);
            motionSlider.setValue (getParam ("chorusMix"), juce::dontSendNotification);
            textureSlider.setValue (getParam ("lofiMix"), juce::dontSendNotification);
            volumeSlider.setValue (getParam ("volume"), juce::dontSendNotification);
            panSlider.setValue (getParam ("pan"), juce::dontSendNotification);
        }

        void setParam (const juce::String& keyword, float value)
        {
            const auto id = findParameterIdLike (keyword);
            if (id.isNotEmpty())
                editor.proc.setPackParameterFromUi (id, value);
        }

        float getParam (const juce::String& keyword)
        {
            const auto id = findParameterIdLike (keyword);
            if (id.isNotEmpty())
                return editor.proc.getPackParameterValue (id);
            return 0.5f;
        }

        juce::String findParameterIdLike (const juce::String& keyword) const
        {
            if (const auto* pack = editor.proc.getPack())
            {
                for (const auto& param : pack->parameters.getAll())
                    if (param.id.equalsIgnoreCase (keyword) || param.name.equalsIgnoreCase (keyword))
                        return param.id;
                for (const auto& param : pack->parameters.getAll())
                    if (param.id.containsIgnoreCase (keyword) || param.name.containsIgnoreCase (keyword))
                        return param.id;
            }
            return {};
        }

        void timerCallback() override
        {
            if (editor.proc.decayNoteHighlightLevels())
                repaint (keyboardArea);
            
            repaint (heroArea);
        }

        void updateVisibility()
        {
            const bool showKnobs = (activeTabIdx == 0);
            
            juce::Slider* sliders[] = { &attackSlider, &releaseSlider, &filterSlider, &spaceSlider,
                                        &motionSlider, &textureSlider, &volumeSlider, &panSlider };
            juce::Label* labels[] = { &attackLabel, &releaseLabel, &filterLabel, &spaceLabel,
                                      &motionLabel, &textureLabel, &volumeLabel, &panLabel };
            for (int i = 0; i < 8; ++i)
            {
                sliders[i]->setVisible (showKnobs);
                labels[i]->setVisible (showKnobs);
            }
        }

        int getNoteAtPosition (juce::Point<int> pos) const
        {
            if (! keyboardArea.contains (pos))
                return -1;

            const int startNote = 24;
            const int endNote = 96;
            const int numKeys = 43;
            const float keyW = (float) (keyboardArea.getWidth() - 10) / numKeys;

            int whiteIdx = 0;
            for (int note = startNote; note <= endNote; ++note)
            {
                const bool isBlack = (note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10);
                if (isBlack)
                {
                    const float x = keyboardArea.getX() + 5 + whiteIdx * keyW - (keyW * 0.3f);
                    const auto keyRect = juce::Rectangle<float> (x, (float) keyboardArea.getY(), keyW * 0.6f, (float) keyboardArea.getHeight() * 0.62f);
                    if (keyRect.contains (pos.toFloat()))
                        return note;
                }
                else
                {
                    whiteIdx++;
                }
            }

            whiteIdx = 0;
            for (int note = startNote; note <= endNote; ++note)
            {
                const bool isBlack = (note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10);
                if (! isBlack)
                {
                    const float x = keyboardArea.getX() + 5 + whiteIdx * keyW;
                    const auto keyRect = juce::Rectangle<float> (x, (float) keyboardArea.getY(), keyW - 1.0f, (float) keyboardArea.getHeight());
                    if (keyRect.contains (pos.toFloat()))
                        return note;
                    whiteIdx++;
                }
            }

            return -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int note = getNoteAtPosition (e.getPosition());
            if (note >= 0)
            {
                currentPlayingNote = note;
                editor.proc.handleNoteOn (note, 0.8f);
                repaint (keyboardArea);
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int note = getNoteAtPosition (e.getPosition());
            if (note >= 0 && note != currentPlayingNote)
            {
                if (currentPlayingNote >= 0)
                    editor.proc.handleNoteOff (currentPlayingNote);
                
                currentPlayingNote = note;
                editor.proc.handleNoteOn (note, 0.8f);
                repaint (keyboardArea);
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (currentPlayingNote >= 0)
            {
                editor.proc.handleNoteOff (currentPlayingNote);
                currentPlayingNote = -1;
                repaint (keyboardArea);
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (editor.getBgColor());

            g.setColour (editor.getPanelColor());
            g.fillRect (heroArea);

            juce::Image heroImg = AssetManager::renderDefaultHeroImage (heroArea.getWidth(), heroArea.getHeight());
            if (heroImg.isValid())
            {
                g.drawImage (heroImg, heroArea.toFloat(), juce::RectanglePlacement::fillDestination);
                g.setColour (editor.getBgColor().withAlpha (0.67f));
                g.fillRect (heroArea);
            }

            const float cx = heroArea.getCentreX();
            const float cy = heroArea.getCentreY();
            const float radius = 52.0f;
            
            g.setColour (editor.getAccentColor().withAlpha (0.12f));
            g.fillEllipse (cx - radius - 8, cy - radius - 8, (radius + 8) * 2, (radius + 8) * 2);

            g.setColour (editor.getAccentColor());
            g.drawEllipse (cx - radius, cy - radius, radius * 2, radius * 2, 1.8f);

            const float low = editor.proc.getAudioLowBand() * 20.0f;
            const float mid = editor.proc.getAudioMidBand() * 15.0f;
            const float high = editor.proc.getAudioHighBand() * 10.0f;

            juce::Path wave;
            const int numPoints = 64;
            for (int i = 0; i <= numPoints; ++i)
            {
                const float angle = juce::MathConstants<float>::pi * 2.0f * (i / (float) numPoints);
                const float mod = (i % 2 == 0 ? low : (i % 3 == 0 ? mid : high)) * (0.3f + 0.7f * (float) std::rand() / RAND_MAX);
                const float r = radius + mod;
                const float x = cx + std::cos (angle) * r;
                const float y = cy + std::sin (angle) * r;
                if (i == 0) wave.startNewSubPath (x, y);
                else wave.lineTo (x, y);
            }
            wave.closeSubPath();
            g.setColour (editor.getAccentColor().withAlpha (0.4f));
            g.strokePath (wave, juce::PathStrokeType (1.5f));

            g.setColour (editor.getPanelColor());
            g.fillRoundedRectangle (keyboardArea.toFloat(), 4.0f);

            const int startNote = 24;
            const int endNote = 96;
            const int numKeys = 43;
            const float keyW = (float) (keyboardArea.getWidth() - 10) / numKeys;
            const float keyH = (float) keyboardArea.getHeight();

            int whiteIdx = 0;
            for (int note = startNote; note <= endNote; ++note)
            {
                const bool isBlack = (note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10);
                if (! isBlack)
                {
                    const float x = keyboardArea.getX() + 5 + whiteIdx * keyW;
                    const auto keyRect = juce::Rectangle<float> (x, (float) keyboardArea.getY(), keyW - 1.0f, keyH);
                    
                    const float activeLevel = editor.proc.getNoteHighlightLevel (note);
                    if (activeLevel > 0.0f)
                    {
                        g.setColour (editor.getAccentColor().interpolatedWith (juce::Colours::white, 0.4f).withAlpha (activeLevel));
                        g.fillRoundedRectangle (keyRect, 2.0f);
                    }
                    else
                    {
                        g.setColour (juce::Colour (0xfff0f2f5));
                        g.fillRoundedRectangle (keyRect, 2.0f);
                    }
                    g.setColour (editor.getBgColor().darker (0.05f));
                    g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);

                    whiteIdx++;
                }
            }

            whiteIdx = 0;
            for (int note = startNote; note <= endNote; ++note)
            {
                const bool isBlack = (note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10);
                if (isBlack)
                {
                    const float x = keyboardArea.getX() + 5 + whiteIdx * keyW - (keyW * 0.3f);
                    const auto keyRect = juce::Rectangle<float> (x, (float) keyboardArea.getY(), keyW * 0.6f, keyH * 0.62f);

                    const float activeLevel = editor.proc.getNoteHighlightLevel (note);
                    if (activeLevel > 0.0f)
                    {
                        g.setColour (editor.getAccentColor().brighter (0.2f).withAlpha (activeLevel));
                        g.fillRoundedRectangle (keyRect, 2.0f);
                    }
                    else
                    {
                        g.setColour (editor.getPanelColor().brighter (0.04f));
                        g.fillRoundedRectangle (keyRect, 2.0f);
                    }
                    g.setColour (editor.getBgColor().darker (0.05f));
                    g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);
                }
                else
                {
                    whiteIdx++;
                }
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            heroArea = bounds.removeFromTop (220);

            keyboardArea = bounds.removeFromBottom (108).reduced (46, 6);
            
            const int wheelY = keyboardArea.getY();
            const int wheelH = keyboardArea.getHeight();
            constexpr int labelHeight = 16;
            constexpr int wheelWidth = 20;
            constexpr int wheelGap = 8;
            const int wheelBodyHeight = juce::jmax (24, wheelH - labelHeight - 4);
            pitchWheel.setBounds (6, wheelY, wheelWidth, wheelBodyHeight);
            pitchLabel.setBounds (2, wheelY + wheelBodyHeight + 2, wheelWidth + 8, labelHeight);

            modWheel.setBounds (6 + wheelWidth + wheelGap, wheelY, wheelWidth, wheelBodyHeight);
            modLabel.setBounds (2 + wheelWidth + wheelGap, wheelY + wheelBodyHeight + 2, wheelWidth + 8, labelHeight);

            auto tabsArea = bounds.removeFromBottom (32).reduced (10, 2);
            const int tabW = tabsArea.getWidth() / 5;
            for (int i = 0; i < tabButtons.size(); ++i)
                tabButtons[i]->setBounds (tabsArea.getX() + i * tabW, tabsArea.getY(), tabW - 4, tabsArea.getHeight());

            auto knobsArea = bounds.reduced (30, 8);
            const int cellW = knobsArea.getWidth() / 4;
            const int cellH = knobsArea.getHeight() / 2;

            juce::Slider* sliders[] = { &attackSlider, &releaseSlider, &filterSlider, &spaceSlider,
                                        &motionSlider, &textureSlider, &volumeSlider, &panSlider };
            juce::Label* labels[] = { &attackLabel, &releaseLabel, &filterLabel, &spaceLabel,
                                      &motionLabel, &textureLabel, &volumeLabel, &panLabel };

            for (int i = 0; i < 8; ++i)
            {
                const int row = i / 4;
                const int col = i % 4;
                const int x = knobsArea.getX() + col * cellW;
                const int y = knobsArea.getY() + row * cellH;
                sliders[i]->setBounds (x + cellW / 2 - 24, y + 2, 48, 48);
                labels[i]->setBounds (x + 2, y + 50, cellW - 4, 14);
            }
        }

    private:
        PlayerEditor& editor;
        juce::Slider attackSlider, releaseSlider, filterSlider, spaceSlider,
                     motionSlider, textureSlider, volumeSlider, panSlider;
        juce::Label attackLabel, releaseLabel, filterLabel, spaceLabel,
                    motionLabel, textureLabel, volumeLabel, panLabel;

        juce::OwnedArray<juce::TextButton> tabButtons;
        juce::Slider pitchWheel, modWheel;
        juce::Label pitchLabel, modLabel;

        juce::Rectangle<int> heroArea;
        juce::Rectangle<int> keyboardArea;
        int activeTabIdx = 0;
        int currentPlayingNote = -1;
    };

    class VelocityCurveComponent : public juce::Component,
                                   public juce::TooltipClient
    {
    public:
        VelocityCurveComponent()
        {
        }

        juce::String getTooltip() override
        {
            return "Adjust MIDI Velocity response curve.";
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff11131a));
            g.fillRoundedRectangle (bounds, 4.0f);

            g.setColour (juce::Colour (0xff1c212b));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

            juce::Path p;
            p.startNewSubPath (0.0f, bounds.getHeight());
            
            const int steps = 24;
            for (int i = 1; i <= steps; ++i)
            {
                const float x = (i / (float) steps) * bounds.getWidth();
                const float t = i / (float) steps;
                const float y = (1.0f - std::pow (t, curvePower)) * bounds.getHeight();
                p.lineTo (x, y);
            }

            g.setColour (juce::Colour (0xff8e6cd6));
            g.strokePath (p, juce::PathStrokeType (2.0f));
            
            p.lineTo (bounds.getWidth(), bounds.getHeight());
            p.closeSubPath();
            juce::ColourGradient grad (juce::Colour (0xff8e6cd6).withAlpha (0.24f), 0.0f, 0.0f,
                                       juce::Colours::transparentBlack, 0.0f, bounds.getHeight(), false);
            g.setGradientFill (grad);
            g.fillPath (p);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float dy = (float) e.y / getHeight();
            curvePower = juce::jlimit (0.2f, 4.0f, 1.0f + (0.5f - dy) * 3.0f);
            repaint();
        }

    private:
        float curvePower = 1.0f;
    };

    class PlayerRightPanel : public juce::Component
    {
    public:
        explicit PlayerRightPanel (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            const juce::String macroNames[] = { "TONE", "WASH", "DRIVE", "WIDTH", "MOVEMENT", "PHASER", "DELAY", "REVERB" };
            for (int i = 0; i < 8; ++i)
            {
                auto* s = new juce::Slider();
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s->getProperties().set ("accentColor", (int) 0xff8e6cd6);
                s->setRange (0.0, 1.0, 0.01);
                s->setValue (0.4);
                s->onValueChange = [this, s, i] {
                    editor.proc.setPackParameterFromUi ("macro" + juce::String (i + 1), (float) s->getValue());
                };
                macroSliders.add (s);
                addAndMakeVisible (s);

                auto* l = new juce::Label();
                l->setText (macroNames[i], juce::dontSendNotification);
                l->setFont (juce::Font (8.0f, juce::Font::bold));
                l->setJustificationType (juce::Justification::centred);
                l->setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
                macroLabels.add (l);
                addAndMakeVisible (l);
            }

            struct FxSetup { juce::ToggleButton* toggle; juce::ComboBox* combo; juce::String name; juce::StringArray options; };
            const FxSetup fxSetups[] = {
                { &delayToggle, &delayCombo, "DELAY", { "Tape Delay", "Analog Delay", "Digital Delay" } },
                { &reverbToggle, &reverbCombo, "REVERB", { "Shimmer Hall", "Large Room", "Spring Reverb" } },
                { &chorusToggle, &chorusCombo, "CHORUS", { "Dimension Chorus", "Ensemble Chorus", "Classic Chorus" } }
            };

            for (const auto& f : fxSetups)
            {
                f.toggle->setButtonText (f.name);
                f.toggle->setToggleState (true, juce::dontSendNotification);
                f.toggle->onClick = [this] { updateFxState(); };
                addAndMakeVisible (f.toggle);

                for (int idx = 0; idx < f.options.size(); ++idx)
                    f.combo->addItem (f.options[idx], idx + 1);
                f.combo->setSelectedId (1);
                f.combo->onChange = [this] { updateFxState(); };
                addAndMakeVisible (f.combo);
            }

            const juce::String sendNames[] = { "DELAY", "REVERB", "CHORUS", "MASTER" };
            for (int i = 0; i < 4; ++i)
            {
                auto* s = new juce::Slider();
                s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s->getProperties().set ("accentColor", (int) 0xff8e6cd6);
                s->setRange (0.0, 1.0, 0.01);
                s->setValue (0.5);
                s->onValueChange = [this, s, i] {
                    const juce::String paramKeys[] = { "delayMix", "reverbMix", "chorusMix", "volume" };
                    editor.proc.setPackParameterFromUi (paramKeys[i], (float) s->getValue());
                };
                sendSliders.add (s);
                addAndMakeVisible (s);

                auto* l = new juce::Label();
                l->setText (sendNames[i], juce::dontSendNotification);
                l->setFont (juce::Font (8.0f, juce::Font::bold));
                l->setJustificationType (juce::Justification::centred);
                l->setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
                sendLabels.add (l);
                addAndMakeVisible (l);
            }

            addAndMakeVisible (velCurve);
            velCurveLabel.setText ("VELOCITY CURVE", juce::dontSendNotification);
            velCurveLabel.setFont (juce::Font (8.0f, juce::Font::bold));
            velCurveLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (velCurveLabel);

            glideSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            glideSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            glideSlider.getProperties().set ("accentColor", (int) 0xff8e6cd6);
            glideSlider.setRange (0.0, 1000.0, 1.0);
            glideSlider.setValue (120.0);
            glideSlider.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("glideTime", (float) glideSlider.getValue());
                glideValLabel.setText (juce::String ((int) glideSlider.getValue()) + " ms", juce::dontSendNotification);
            };
            addAndMakeVisible (glideSlider);

            glideLabel.setText ("GLIDE", juce::dontSendNotification);
            glideLabel.setFont (juce::Font (8.0f, juce::Font::bold));
            glideLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (glideLabel);

            glideValLabel.setText ("120 ms", juce::dontSendNotification);
            glideValLabel.setFont (juce::Font (8.0f));
            glideValLabel.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (glideValLabel);

            voicesBox.addItem ("Mono", 1);
            voicesBox.addItem ("4 Voices", 2);
            voicesBox.addItem ("8 Voices", 3);
            voicesBox.addItem ("16 Voices", 4);
            voicesBox.setSelectedId (4);
            voicesBox.onChange = [this] {
                const int counts[] = { 1, 4, 8, 16 };
                editor.proc.setPackParameterFromUi ("polyphony", (float) counts[voicesBox.getSelectedId() - 1]);
            };
            addAndMakeVisible (voicesBox);

            voicesLabel.setText ("VOICES", juce::dontSendNotification);
            voicesLabel.setFont (juce::Font (8.0f, juce::Font::bold));
            voicesLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (voicesLabel);

            cpuLabel.setText ("CPU: 12%", juce::dontSendNotification);
            cpuLabel.setFont (juce::Font (9.0f));
            cpuLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d96a3));
            addAndMakeVisible (cpuLabel);

            updateFxState();
            updateColours();
        }

        void updateColours()
        {
            auto txtDim = editor.getTextDimColor();
            auto txt = editor.getTextColor();
            auto accent = editor.getAccentColor();

            if (const auto* pack = editor.proc.getPack())
            {
                for (int i = 0; i < 8; ++i)
                {
                    if (i < pack->manifest.rightPanelMacroNames.size() && i < macroLabels.size())
                    {
                        const auto name = pack->manifest.rightPanelMacroNames[i];
                        if (name.isNotEmpty())
                            macroLabels[i]->setText (name, juce::dontSendNotification);
                    }
                }
            }

            for (auto* s : macroSliders)
                s->getProperties().set ("accentColor", (int) accent.getARGB());
            for (auto* l : macroLabels)
                l->setColour (juce::Label::textColourId, txtDim);

            for (auto* s : sendSliders)
                s->getProperties().set ("accentColor", (int) accent.getARGB());
            for (auto* l : sendLabels)
                l->setColour (juce::Label::textColourId, txtDim);

            velCurveLabel.setColour (juce::Label::textColourId, txtDim);
            glideLabel.setColour (juce::Label::textColourId, txtDim);
            glideValLabel.setColour (juce::Label::textColourId, txt);
            glideSlider.getProperties().set ("accentColor", (int) accent.getARGB());
            voicesLabel.setColour (juce::Label::textColourId, txtDim);
            cpuLabel.setColour (juce::Label::textColourId, txtDim);

            repaint();
        }

        ~PlayerRightPanel() override
        {
            macroSliders.clear();
            macroLabels.clear();
            sendSliders.clear();
            sendLabels.clear();
        }

        void updateFxState()
        {
            editor.proc.setPackParameterFromUi ("delayEnabled", delayToggle.getToggleState() ? 1.0f : 0.0f);
            editor.proc.setPackParameterFromUi ("reverbEnabled", reverbToggle.getToggleState() ? 1.0f : 0.0f);
            editor.proc.setPackParameterFromUi ("chorusEnabled", chorusToggle.getToggleState() ? 1.0f : 0.0f);
            
            editor.proc.setPackParameterFromUi ("delayType", (float) delayCombo.getSelectedId());
            editor.proc.setPackParameterFromUi ("reverbType", (float) reverbCombo.getSelectedId());
            editor.proc.setPackParameterFromUi ("chorusType", (float) chorusCombo.getSelectedId());
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (editor.getBgColor());
            
            g.setColour (editor.getBorderColor());
            g.fillRect (0, 0, 1, getHeight());

            g.setColour (editor.getAccentColor());
            g.setFont (juce::Font (11.0f, juce::Font::bold));

            const auto* pack = editor.proc.getPack();
            const bool showMacros = pack == nullptr || pack->manifest.rightPanelShowMacros;
            const bool showEffects = pack == nullptr || pack->manifest.rightPanelShowEffects;
            const bool showSends = pack == nullptr || pack->manifest.rightPanelShowSends;

            int currentY = 8;

            if (showMacros)
            {
                g.drawText ("MACROS", 16, currentY, 120, 16, juce::Justification::centredLeft);
                const int mKnobW = 42;
                const int gapY = 6;
                currentY = currentY + 20 + 2 * (mKnobW + 20) + gapY + 12;
            }

            if (showEffects)
            {
                g.drawText ("EFFECTS", 16, currentY, 120, 16, juce::Justification::centredLeft);
                currentY = currentY + 20 + 60 + 22 + 16;
            }

            if (showSends)
            {
                g.drawText ("SEND LEVELS", 16, currentY, 120, 16, juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            const auto* pack = editor.proc.getPack();
            const bool showMacros = pack == nullptr || pack->manifest.rightPanelShowMacros;
            const bool showEffects = pack == nullptr || pack->manifest.rightPanelShowEffects;
            const bool showSends = pack == nullptr || pack->manifest.rightPanelShowSends;
            const bool showUtility = pack == nullptr || pack->manifest.rightPanelShowUtility;

            // Visibility setting
            for (auto* s : macroSliders) s->setVisible (showMacros);
            for (auto* l : macroLabels) l->setVisible (showMacros);

            delayToggle.setVisible (showEffects);
            delayCombo.setVisible (showEffects);
            reverbToggle.setVisible (showEffects);
            reverbCombo.setVisible (showEffects);
            chorusToggle.setVisible (showEffects);
            chorusCombo.setVisible (showEffects);

            for (auto* s : sendSliders) s->setVisible (showSends);
            for (auto* l : sendLabels) l->setVisible (showSends);

            velCurveLabel.setVisible (showUtility);
            velCurve.setVisible (showUtility);
            glideLabel.setVisible (showUtility);
            glideSlider.setVisible (showUtility);
            glideValLabel.setVisible (showUtility);
            voicesLabel.setVisible (showUtility);
            voicesBox.setVisible (showUtility);
            cpuLabel.setVisible (showUtility);

            int currentY = 8;

            if (showMacros)
            {
                const int mKnobW = 42;
                const int gapX = 14;
                const int gapY = 6;
                const int startY = currentY + 20;
                for (int i = 0; i < 8; ++i)
                {
                    const int row = i / 4;
                    const int col = i % 4;
                    const int x = 16 + col * (mKnobW + gapX);
                    const int y = startY + row * (mKnobW + 20 + gapY);
                    macroSliders[i]->setBounds (x, y, mKnobW, mKnobW);
                    macroLabels[i]->setBounds (x - 6, y + mKnobW + 2, mKnobW + 12, 12);
                }
                currentY = startY + 2 * (mKnobW + 20) + gapY + 12;
            }

            if (showEffects)
            {
                const int startY = currentY + 20;
                delayToggle.setBounds (16, startY, 78, 22);
                delayCombo.setBounds (100, startY, 180, 22);

                reverbToggle.setBounds (16, startY + 30, 78, 22);
                reverbCombo.setBounds (100, startY + 30, 180, 22);

                chorusToggle.setBounds (16, startY + 60, 78, 22);
                chorusCombo.setBounds (100, startY + 60, 180, 22);

                currentY = startY + 60 + 22 + 16;
            }

            if (showSends)
            {
                const int startY = currentY + 20;
                const int sKnobW = 44;
                const int sGapX = 18;
                for (int i = 0; i < 4; ++i)
                {
                    const int x = 16 + i * (sKnobW + sGapX);
                    sendSliders[i]->setBounds (x, startY, sKnobW, sKnobW);
                    sendLabels[i]->setBounds (x - 6, startY + sKnobW + 2, sKnobW + 12, 12);
                }
                currentY = startY + sKnobW + 14 + 16;
            }

            if (showUtility)
            {
                const int startY = currentY + 4;
                velCurveLabel.setBounds (16, startY, 120, 12);
                velCurve.setBounds (16, startY + 16, 120, 72);

                glideLabel.setBounds (152, startY + 2, 40, 12);
                glideSlider.setBounds (152, startY + 16, 40, 40);
                glideValLabel.setBounds (152, startY + 58, 40, 12);

                voicesLabel.setBounds (206, startY + 2, 50, 12);
                voicesBox.setBounds (206, startY + 16, 78, 22);
                cpuLabel.setBounds (206, startY + 48, 78, 22);
            }
        }

    private:
        PlayerEditor& editor;
        juce::OwnedArray<juce::Slider> macroSliders;
        juce::OwnedArray<juce::Label> macroLabels;

        juce::ToggleButton delayToggle, reverbToggle, chorusToggle;
        juce::ComboBox delayCombo, reverbCombo, chorusCombo;

        juce::OwnedArray<juce::Slider> sendSliders;
        juce::OwnedArray<juce::Label> sendLabels;

        VelocityCurveComponent velCurve;
        juce::Label velCurveLabel;

        juce::Slider glideSlider;
        juce::Label glideLabel, glideValLabel;
        juce::ComboBox voicesBox;
        juce::Label voicesLabel, cpuLabel;
    };

    class PlayerFooter : public juce::Component
    {
    public:
        explicit PlayerFooter (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            verLabel.setText ("v1.0.0", juce::dontSendNotification);
            verLabel.setFont (juce::Font (10.0f));
            verLabel.setColour (juce::Label::textColourId, juce::Colour (0xff505866));
            addAndMakeVisible (verLabel);

            statusLabel.setText ("Ready", juce::dontSendNotification);
            statusLabel.setFont (juce::Font (10.0f));
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa6acb5));
            statusLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (statusLabel);

            licLabel.setText ("License not required", juce::dontSendNotification);
            licLabel.setFont (juce::Font (10.0f));
            licLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5fb37b));
            licLabel.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (licLabel);

            updateColours();
            refreshLicense();
        }

        void updateColours()
        {
            auto txtDim = editor.getTextDimColor();
            auto txt = editor.getTextColor();
            auto accent = editor.getAccentColor();

            verLabel.setColour (juce::Label::textColourId, txtDim);
            statusLabel.setColour (juce::Label::textColourId, txt);
            licLabel.setColour (juce::Label::textColourId, accent);
        }

        void setStatus (const juce::String& msg)
        {
            statusLabel.setText (msg, juce::dontSendNotification);
        }

        void refreshLicense()
        {
            const auto text = editor.proc.getLicenseStatusText();
            licLabel.setText (text, juce::dontSendNotification);
            licLabel.setTooltip (text + ". Open Settings / About to activate or review support details.");
            licLabel.setColour (juce::Label::textColourId,
                                editor.proc.isLicenseAuthorized()
                                    ? editor.getAccentColor()
                                    : juce::Colour (0xffffa62b));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (editor.getBgColor());
            
            g.setColour (editor.getBorderColor());
            g.fillRect (0, 0, getWidth(), 1);
        }

        void resized() override
        {
            verLabel.setBounds (10, 4, 80, 20);
            statusLabel.setBounds (100, 4, getWidth() - 200, 20);
            licLabel.setBounds (getWidth() - 170, 4, 160, 20);
        }

    private:
        PlayerEditor& editor;
        juce::Label verLabel, statusLabel, licLabel;
    };

    /**
        Bottom performance strip: octave shift, pitch/mod wheels and a playable
        keyboard. Mirrors the exported Player blueprint frame.
    */
    class PlayerKeyboardStrip : public juce::Component,
                                private juce::Timer
    {
    public:
        explicit PlayerKeyboardStrip (PlayerEditor& editorToUse)
            : editor (editorToUse)
        {
            octUpBtn.setButtonText ("+");
            octUpBtn.setTooltip ("Shift on-screen keyboard up one octave.");
            octUpBtn.onClick = [this] { setOctaveShift (octaveShift + 1); };
            addAndMakeVisible (octUpBtn);

            octDownBtn.setButtonText ("-");
            octDownBtn.setTooltip ("Shift on-screen keyboard down one octave.");
            octDownBtn.onClick = [this] { setOctaveShift (octaveShift - 1); };
            addAndMakeVisible (octDownBtn);

            octLabel.setText ("0", juce::dontSendNotification);
            octLabel.setJustificationType (juce::Justification::centred);
            octLabel.setFont (juce::Font (10.0f, juce::Font::bold));
            addAndMakeVisible (octLabel);

            pitchWheel.setSliderStyle (juce::Slider::LinearVertical);
            pitchWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            pitchWheel.setRange (-1.0, 1.0, 0.01);
            pitchWheel.setValue (0.0);
            pitchWheel.setDoubleClickReturnValue (true, 0.0);
            pitchWheel.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("pitchWheel", (float) pitchWheel.getValue());
            };
            pitchWheel.onDragEnd = [this] {
                pitchWheel.setValue (0.0, juce::sendNotification);
            };
            addAndMakeVisible (pitchWheel);

            modWheel.setSliderStyle (juce::Slider::LinearVertical);
            modWheel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            modWheel.setRange (0.0, 1.0, 0.01);
            modWheel.setValue (0.0);
            modWheel.onValueChange = [this] {
                editor.proc.setPackParameterFromUi ("modWheel", (float) modWheel.getValue());
            };
            addAndMakeVisible (modWheel);

            pitchLabel.setText ("PITCH", juce::dontSendNotification);
            pitchLabel.setJustificationType (juce::Justification::centred);
            pitchLabel.setFont (juce::Font (8.0f, juce::Font::bold));
            addAndMakeVisible (pitchLabel);

            modLabel.setText ("MOD", juce::dontSendNotification);
            modLabel.setJustificationType (juce::Justification::centred);
            modLabel.setFont (juce::Font (8.0f, juce::Font::bold));
            addAndMakeVisible (modLabel);

            updateColours();
            startTimerHz (24);
        }

        ~PlayerKeyboardStrip() override
        {
            stopTimer();
        }

        void updateColours()
        {
            const auto txtDim = editor.getTextDimColor();
            octLabel.setColour (juce::Label::textColourId, txtDim);
            pitchLabel.setColour (juce::Label::textColourId, txtDim);
            modLabel.setColour (juce::Label::textColourId, txtDim);
            repaint();
        }

        void setOctaveShift (int newShift)
        {
            releaseHeldNote();
            octaveShift = juce::jlimit (-3, 3, newShift);
            octLabel.setText ((octaveShift > 0 ? "+" : "") + juce::String (octaveShift), juce::dontSendNotification);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (editor.getBgColor());
            g.setColour (editor.getBorderColor());
            g.fillRect (0, 0, getWidth(), 1);

            g.setColour (editor.getPanelColor());
            g.fillRoundedRectangle (keyboardArea.toFloat(), 4.0f);

            const float keyW = whiteKeyWidth();
            const float keyH = (float) keyboardArea.getHeight() - 8.0f;
            const float keyY = (float) keyboardArea.getY() + 4.0f;

            int whiteIdx = 0;
            for (int note = kFirstNote; note <= kLastNote; ++note)
            {
                if (! isBlackKey (note))
                {
                    const float x = (float) keyboardArea.getX() + 5.0f + whiteIdx * keyW;
                    const auto keyRect = juce::Rectangle<float> (x, keyY, keyW - 1.0f, keyH);

                    const float activeLevel = editor.proc.getNoteHighlightLevel (note + octaveShift * 12);
                    if (activeLevel > 0.0f)
                        g.setColour (editor.getAccentColor().interpolatedWith (juce::Colours::white, 0.4f).withAlpha (juce::jmax (0.35f, activeLevel)));
                    else
                        g.setColour (juce::Colour (0xfff0f2f5));
                    g.fillRoundedRectangle (keyRect, 2.0f);
                    g.setColour (editor.getBgColor().darker (0.05f));
                    g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);

                    if (note % 12 == 0)
                    {
                        g.setColour (juce::Colour (0xff70768a));
                        g.setFont (juce::Font (8.0f));
                        g.drawText ("C" + juce::String (note / 12 - 1),
                                    juce::roundToInt (x), juce::roundToInt (keyY + keyH - 12.0f),
                                    juce::roundToInt (keyW), 10, juce::Justification::centred);
                    }

                    ++whiteIdx;
                }
            }

            whiteIdx = 0;
            for (int note = kFirstNote; note <= kLastNote; ++note)
            {
                if (isBlackKey (note))
                {
                    const float x = (float) keyboardArea.getX() + 5.0f + whiteIdx * keyW - keyW * 0.3f;
                    const auto keyRect = juce::Rectangle<float> (x, keyY, keyW * 0.6f, keyH * 0.62f);

                    const float activeLevel = editor.proc.getNoteHighlightLevel (note + octaveShift * 12);
                    if (activeLevel > 0.0f)
                        g.setColour (editor.getAccentColor().brighter (0.2f).withAlpha (juce::jmax (0.35f, activeLevel)));
                    else
                        g.setColour (editor.getPanelColor().brighter (0.04f));
                    g.fillRoundedRectangle (keyRect, 2.0f);
                    g.setColour (editor.getBgColor().darker (0.05f));
                    g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);
                }
                else
                {
                    ++whiteIdx;
                }
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (8, 6);

            auto leftCluster = bounds.removeFromLeft (96);
            auto octColumn = leftCluster.removeFromLeft (30);
            octUpBtn.setBounds (octColumn.removeFromTop (22));
            octLabel.setBounds (octColumn.removeFromTop (16));
            octDownBtn.setBounds (octColumn.removeFromTop (22));

            leftCluster.removeFromLeft (6);
            constexpr int labelHeight = 12;
            auto pitchColumn = leftCluster.removeFromLeft (26);
            pitchLabel.setBounds (pitchColumn.removeFromBottom (labelHeight));
            pitchWheel.setBounds (pitchColumn);

            leftCluster.removeFromLeft (4);
            auto modColumn = leftCluster.removeFromLeft (26);
            modLabel.setBounds (modColumn.removeFromBottom (labelHeight));
            modWheel.setBounds (modColumn);

            bounds.removeFromLeft (6);
            keyboardArea = bounds;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            triggerNoteAt (e.getPosition());
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int note = noteAtPosition (e.getPosition());
            if (note >= 0 && note != heldNote)
            {
                releaseHeldNote();
                heldNote = note;
                editor.proc.handleNoteOn (note, 0.8f);
                repaint (keyboardArea);
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            releaseHeldNote();
        }

    private:
        static constexpr int kFirstNote = 24; // C1
        static constexpr int kLastNote = 96;  // C7
        static constexpr int kNumWhiteKeys = 43;

        static bool isBlackKey (int note)
        {
            const int n = note % 12;
            return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
        }

        float whiteKeyWidth() const
        {
            return (float) (keyboardArea.getWidth() - 10) / (float) kNumWhiteKeys;
        }

        int noteAtPosition (juce::Point<int> pos) const
        {
            if (! keyboardArea.contains (pos))
                return -1;

            const float keyW = whiteKeyWidth();
            const float keyH = (float) keyboardArea.getHeight() - 8.0f;
            const float keyY = (float) keyboardArea.getY() + 4.0f;

            int whiteIdx = 0;
            for (int note = kFirstNote; note <= kLastNote; ++note)
            {
                if (isBlackKey (note))
                {
                    const float x = (float) keyboardArea.getX() + 5.0f + whiteIdx * keyW - keyW * 0.3f;
                    if (juce::Rectangle<float> (x, keyY, keyW * 0.6f, keyH * 0.62f).contains (pos.toFloat()))
                        return juce::jlimit (0, 127, note + octaveShift * 12);
                }
                else
                {
                    ++whiteIdx;
                }
            }

            whiteIdx = 0;
            for (int note = kFirstNote; note <= kLastNote; ++note)
            {
                if (! isBlackKey (note))
                {
                    const float x = (float) keyboardArea.getX() + 5.0f + whiteIdx * keyW;
                    if (juce::Rectangle<float> (x, keyY, keyW - 1.0f, keyH).contains (pos.toFloat()))
                        return juce::jlimit (0, 127, note + octaveShift * 12);
                    ++whiteIdx;
                }
            }

            return -1;
        }

        void triggerNoteAt (juce::Point<int> pos)
        {
            const int note = noteAtPosition (pos);
            if (note >= 0)
            {
                releaseHeldNote();
                heldNote = note;
                editor.proc.handleNoteOn (note, 0.8f);
                repaint (keyboardArea);
            }
        }

        void releaseHeldNote()
        {
            if (heldNote >= 0)
            {
                editor.proc.handleNoteOff (heldNote);
                heldNote = -1;
                repaint (keyboardArea);
            }
        }

        void timerCallback() override
        {
            repaint (keyboardArea);
        }

        PlayerEditor& editor;
        juce::TextButton octUpBtn, octDownBtn;
        juce::Label octLabel;
        juce::Slider pitchWheel, modWheel;
        juce::Label pitchLabel, modLabel;
        juce::Rectangle<int> keyboardArea;
        int octaveShift = 0;
        int heldNote = -1;
    };

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
            drawField (graphics, area, "License", processor.getLicenseStatusText());
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

        topBar = std::make_unique<PlayerTopBar> (*this);
        addAndMakeVisible (*topBar);

        leftSidebar = std::make_unique<PlayerLeftSidebar> (*this);
        addAndMakeVisible (*leftSidebar);

        centerPanel = std::make_unique<PlayerCenterPanel> (*this);
        addChildComponent (*centerPanel);
        centerPanel->setVisible (false);

        rightPanel = std::make_unique<PlayerRightPanel> (*this);
        addAndMakeVisible (*rightPanel);

        footer = std::make_unique<PlayerFooter> (*this);
        addAndMakeVisible (*footer);

        keyboardStrip = std::make_unique<PlayerKeyboardStrip> (*this);
        addAndMakeVisible (*keyboardStrip);

        renderer = std::make_unique<PlayerGuiRenderer> (proc, assets);
        addAndMakeVisible (*renderer);

        libraryBrowser = std::make_unique<LibraryBrowser> (proc.getLibraryScanner(), LibraryBrowser::PackFilter::Any);
        libraryBrowser->onPackSelected = [this] (const juce::File& folder) {
            juce::String err;
            if (proc.loadPack (folder, err))
            {
                libraryVisible = false;
                libraryBrowser->setVisible (false);
                packChanged();
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
            resized();
        };
        libraryBrowser->setVisible (false);
        addAndMakeVisible (*libraryBrowser);

        presetLoadingLabel.setJustificationType (juce::Justification::centred);
        presetLoadingLabel.setInterceptsMouseClicks (false, false);
        presetLoadingLabel.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        presetLoadingLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        presetLoadingLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xee07090c));
        presetLoadingLabel.setColour (juce::Label::outlineColourId, PatchCraftLookAndFeel::accent());
        presetLoadingLabel.setVisible (false);
        addAndMakeVisible (presetLoadingLabel);

        setSize (1280, 720);
        setResizable (true, true);
        setResizeLimits (1024, 600, 2400, 1800);

        proc.addEditorListener (this);
        packChanged();
    }

    PlayerEditor::~PlayerEditor()
    {
        proc.removeEditorListener (this);
        setLookAndFeel (nullptr);
    }

    juce::Colour PlayerEditor::getBgColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerBackgroundColour : juce::Colour (0xff07080a);
    }

    juce::Colour PlayerEditor::getPanelColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerPanelColour : juce::Colour (0xff0a0b12);
    }

    juce::Colour PlayerEditor::getAccentColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerAccentColour : juce::Colour (0xff8e6cd6);
    }

    juce::Colour PlayerEditor::getTextColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerTextColour : juce::Colours::white;
    }

    juce::Colour PlayerEditor::getTextDimColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerTextDimColour : juce::Colour (0xff8d96a3);
    }

    juce::Colour PlayerEditor::getBorderColor() const
    {
        const auto* pack = proc.getPack();
        return pack != nullptr ? pack->manifest.playerBorderColour : juce::Colour (0xff1c212c);
    }

    void PlayerEditor::paint (juce::Graphics& g)
    {
        g.fillAll (getBgColor());
    }

    void PlayerEditor::resized()
    {
        auto bounds = getLocalBounds();

        if (libraryVisible)
        {
            if (libraryBrowser != nullptr)
            {
                libraryBrowser->setBounds (bounds);
                libraryBrowser->setVisible (true);
            }
            if (topBar != nullptr) topBar->setVisible (false);
            if (leftSidebar != nullptr) leftSidebar->setVisible (false);
            if (centerPanel != nullptr) centerPanel->setVisible (false);
            if (rightPanel != nullptr) rightPanel->setVisible (false);
            if (footer != nullptr) footer->setVisible (false);
            if (keyboardStrip != nullptr) keyboardStrip->setVisible (false);
            if (renderer != nullptr) renderer->setVisible (false);
            return;
        }

        if (libraryBrowser != nullptr)
            libraryBrowser->setVisible (false);

        if (topBar != nullptr)
        {
            const auto* pack = proc.getPack();
            const bool showTopBar = pack == nullptr || pack->manifest.playerShowTopBar;
            topBar->setVisible (showTopBar);
            if (showTopBar)
                topBar->setBounds (bounds.removeFromTop (72));
            else
                topBar->setBounds ({});
        }

        if (footer != nullptr)
        {
            const auto* pack = proc.getPack();
            const bool showFooter = pack == nullptr || pack->manifest.playerShowFooter;
            footer->setVisible (showFooter);
            if (showFooter)
                footer->setBounds (bounds.removeFromBottom (28));
        }

        if (leftSidebar != nullptr)
        {
            const auto* pack = proc.getPack();
            const bool showLeft = pack == nullptr || pack->manifest.playerShowLeftSidebar;
            leftSidebar->setVisible (showLeft);
            if (showLeft)
                leftSidebar->setBounds (bounds.removeFromLeft (260));
        }

        if (rightPanel != nullptr)
        {
            const auto* pack = proc.getPack();
            const bool anySection = pack == nullptr
                                 || pack->manifest.rightPanelShowMacros
                                 || pack->manifest.rightPanelShowEffects
                                 || pack->manifest.rightPanelShowSends
                                 || pack->manifest.rightPanelShowUtility;
            const bool showRight = (pack == nullptr || pack->manifest.playerShowRightPanel) && anySection;
            rightPanel->setVisible (showRight);
            if (showRight)
                rightPanel->setBounds (bounds.removeFromRight (300));
            else
                rightPanel->setBounds ({});
        }

        if (keyboardStrip != nullptr)
        {
            const auto* pack = proc.getPack();
            const bool showKeyboard = pack == nullptr || pack->manifest.playerShowKeyboard;
            keyboardStrip->setVisible (showKeyboard);
            if (showKeyboard)
                keyboardStrip->setBounds (bounds.removeFromBottom (96));
            else
                keyboardStrip->setBounds ({});
        }

        if (centerPanel != nullptr)
        {
            centerPanel->setVisible (false);
            centerPanel->setBounds ({});
        }

        if (renderer != nullptr)
        {
            renderer->setBounds (bounds);
            renderer->setVisible (true);
        }

        const int overlayW = juce::jmin (540, juce::jmax (280, getWidth() - 80));
        const int overlayH = 72;
        presetLoadingLabel.setBounds (juce::Rectangle<int> (overlayW, overlayH).withCentre (getLocalBounds().getCentre()));
        if (presetLoadingLabel.isVisible())
            presetLoadingLabel.toFront (false);
    }

    void PlayerEditor::resizeToCurrentPackCanvas()
    {
        if (getWidth() != 1280 || getHeight() != 720)
            setSize (1280, 720);
    }

    void PlayerEditor::packChanged()
    {
        const auto* pack = proc.getPack();
        if (pack != nullptr)
        {
            const auto& m = pack->manifest;
            laf.setColour (juce::ResizableWindow::backgroundColourId, m.playerBackgroundColour);
            laf.setColour (juce::DocumentWindow::backgroundColourId, m.playerBackgroundColour);
            laf.setColour (juce::TextButton::buttonColourId, m.playerPanelColour.brighter (0.04f));
            laf.setColour (juce::TextButton::buttonOnColourId, m.playerAccentColour);
            laf.setColour (juce::TextButton::textColourOffId, m.playerTextDimColour);
            laf.setColour (juce::TextButton::textColourOnId, m.playerTextColour);
            laf.setColour (juce::ComboBox::backgroundColourId, m.playerPanelColour);
            laf.setColour (juce::ComboBox::textColourId, m.playerTextColour);
            laf.setColour (juce::ComboBox::outlineColourId, m.playerBorderColour);
            laf.setColour (juce::ComboBox::buttonColourId, m.playerPanelColour.darker (0.1f));
            laf.setColour (juce::ComboBox::arrowColourId, m.playerAccentColour);
            laf.setColour (juce::Slider::rotarySliderFillColourId, m.playerAccentColour);
            laf.setColour (juce::Slider::rotarySliderOutlineColourId, m.playerBorderColour);
            laf.setColour (juce::Slider::thumbColourId, m.playerAccentColour);
            laf.setColour (juce::Slider::trackColourId, m.playerAccentColour.withAlpha (0.5f));
            laf.setColour (juce::Slider::backgroundColourId, m.playerPanelColour);
            laf.setColour (juce::Label::textColourId, m.playerTextColour);
            laf.setColour (juce::TextEditor::backgroundColourId, m.playerPanelColour);
            laf.setColour (juce::TextEditor::textColourId, m.playerTextColour);
            laf.setColour (juce::TextEditor::outlineColourId, m.playerBorderColour);
            laf.setColour (juce::ListBox::backgroundColourId, m.playerBackgroundColour);
            laf.setColour (juce::ListBox::textColourId, m.playerTextDimColour);
        }

        if (topBar != nullptr)
        {
            topBar->updatePresetCard();
            topBar->updateChromeVisibility();
        }

        if (leftSidebar != nullptr)
            leftSidebar->refreshList();

        if (centerPanel != nullptr)
        {
            centerPanel->updateKnobsFromProcessor();
            centerPanel->updateColours();
        }

        if (renderer != nullptr)
        {
            renderer->rebuild();
            renderer->setVisible (true);
        }

        if (rightPanel != nullptr)
        {
            rightPanel->updateColours();
            rightPanel->resized(); // section visibility follows the manifest
        }

        if (keyboardStrip != nullptr)
            keyboardStrip->updateColours();

        if (footer != nullptr)
        {
            footer->updateColours();
            footer->refreshLicense();
        }

        presetLoadingLabel.setVisible (false);
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
            AboutPanel (Manifest manifestIn,
                        juce::String licenseStatusIn,
                        std::function<void()> activateActionIn)
                : manifest (std::move (manifestIn)),
                  licenseStatus (std::move (licenseStatusIn)),
                  activateAction (std::move (activateActionIn))
            {
                setSize (660, 460);

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
                activateButton.setButtonText ("Activate License");
                activateButton.setTooltip ("Validate a buyer license key with the publisher's activation server.");
                activateButton.setVisible (manifest.licenseRequired);
                activateButton.onClick = [this]
                {
                    if (activateAction)
                        activateAction();
                };
                addAndMakeVisible (activateButton);
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
                drawRow ("License", licenseStatus);
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
                if (activateButton.isVisible())
                    activateButton.setBounds (buttons.removeFromLeft (132));
            }

            Manifest manifest;
            juce::String licenseStatus;
            std::function<void()> activateAction;
            juce::TextButton supportButton { "Support" };
            juce::TextButton manualButton { "Manual" };
            juce::TextButton storeButton { "Store" };
            juce::TextButton activateButton { "Activate License" };
            juce::TextButton closeButton { "Close" };
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "About / Support";
        options.dialogBackgroundColour = pack->manifest.playerBackgroundColour;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.componentToCentreAround = this;
        juce::Component::SafePointer<PlayerEditor> safeThis (this);
        options.content.setOwned (new AboutPanel (pack->manifest,
                                                  proc.getLicenseStatusText(),
                                                  [safeThis]
                                                  {
                                                      if (auto* editor = safeThis.getComponent())
                                                          editor->showLicenseActivationDialog();
                                                  }));
        options.launchAsync();
    }

    void PlayerEditor::showLicenseActivationDialog()
    {
        const auto* pack = proc.getPack();
        if (pack == nullptr || ! pack->manifest.licenseRequired)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("License")
                    .withMessage ("This product does not require activation.")
                    .withButton ("OK"),
                nullptr);
            return;
        }

        if (pack->manifest.licenseProductId.trim().isEmpty()
            || pack->manifest.licenseServerUrl.trim().isEmpty())
        {
            const auto support = pack->manifest.playerSupportEmail.isNotEmpty()
                ? pack->manifest.playerSupportEmail
                : juce::String ("the publisher's support address");
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("License Setup Error")
                    .withMessage ("This product is missing its license product ID or activation URL. Contact " + support + ".")
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withButton ("OK"),
                nullptr);
            return;
        }

        auto* dialog = new juce::AlertWindow ("Activate " + playerInstrumentName (pack),
                                              "Enter the license key supplied by the publisher. Activation is tied to this product"
                                                  + juce::String (pack->manifest.licenseBindToMachine ? " and this machine." : "."),
                                              juce::MessageBoxIconType::QuestionIcon,
                                              this);
        dialog->addTextEditor ("licenseKey", {}, "License key:");
        dialog->addButton ("Activate", 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PlayerEditor> safeThis (this);
        const auto info = proc.getLicenseInfo();
        dialog->enterModalState (true,
            juce::ModalCallbackFunction::create ([dialog, safeThis, info] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (dialog);
                if (result != 1)
                    return;

                const auto key = owned->getTextEditorContents ("licenseKey").trim();
                if (key.isEmpty())
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Activation")
                            .withMessage ("Enter a license key before activating.")
                            .withIconType (juce::MessageBoxIconType::WarningIcon)
                            .withButton ("OK"),
                        nullptr);
                    return;
                }

                if (auto* editor = safeThis.getComponent())
                    if (editor->footer != nullptr)
                        editor->footer->setStatus ("Activating license...");

                juce::Thread::launch ([safeThis, info, key]
                {
                    juce::String diagnostic;
                    auto status = LicenseValidator::activateOnline (info, key, 20000, diagnostic);
                    juce::MessageManager::callAsync ([safeThis, status = std::move (status)] () mutable
                    {
                        auto* editor = safeThis.getComponent();
                        if (editor == nullptr)
                            return;

                        editor->proc.applyLicenseActivationStatus (status);
                        if (editor->footer != nullptr)
                        {
                            editor->footer->setStatus (status.authorized ? "Ready" : "Activation required");
                            editor->footer->refreshLicense();
                        }
                        editor->repaint();

                        juce::AlertWindow::showAsync (
                            juce::MessageBoxOptions()
                                .withTitle (status.authorized ? "Activation Complete" : "Activation Failed")
                                .withMessage (status.authorized
                                    ? (status.ownerName.isNotEmpty()
                                        ? "Licensed to " + status.ownerName + "."
                                        : juce::String ("This product is now activated on this machine."))
                                    : (status.message.isNotEmpty()
                                        ? status.message
                                        : juce::String ("The license server did not authorize this product.")))
                                .withIconType (status.authorized
                                    ? juce::MessageBoxIconType::InfoIcon
                                    : juce::MessageBoxIconType::WarningIcon)
                                .withButton ("OK"),
                            nullptr);
                    });
                });
            }));
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
