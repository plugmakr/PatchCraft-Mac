#include "StudioMainComponent.h"
#include "LayoutBindingHelper.h"
#include "PatchCraftLookAndFeel.h"
#include "TopToolbar.h"
#include "ElementPalette.h"
#include "BuiltAssetLibraryComponent.h"
#include "ExpansionLibraryPanel.h"
#include "FloatingPanelWindow.h"
#include "LayersPanel.h"
#include "CanvasEditor.h"
#include "InspectorPanel.h"
#include "ProjectWizardDialog.h"
#include "ProductRecipes.h"
#include "CanvasToolbar.h"
#include "SettingsDialog.h"
#include "AiImageService.h"
#include "LicenseValidator.h"
#include "PatchCraftPackWriter.h"
#include "PluginClubPublisher.h"
#include "PresetsComponent.h"
#include "LaunchReadiness.h"
#include "SampleMap.h"
#include "SampleMapEditor.h"
#include "BottomPanel.h"
#include "VstExportModule.h"
#include "ScriptEditorComponent.h"
#include "ControlNodeEditor.h"
#include "TutorialModeOverlay.h"
#include "PackRuntimeHost.h"
#include "CustomerPreviewOverlay.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <thread>
#include <vector>

namespace patchcraft
{
    namespace
    {
        static bool isScriptableControlElement (const LayoutElement& element)
        {
            return isRuntimeControlElement (element.type)
                || element.type == ElementType::ValueDisplay
                || element.type == ElementType::SampleDropZone;
        }

        static bool isSupportedRuntimeMidiFile (const juce::File& file)
        {
            const auto ext = file.getFileExtension().toLowerCase();
            return file.existsAsFile() && (ext == ".mid" || ext == ".midi");
        }

        static bool isScriptUnit (const juce::String& unit)
        {
            return unit == "%"
                || unit.equalsIgnoreCase ("Hz")
                || unit.equalsIgnoreCase ("dB")
                || unit.equalsIgnoreCase ("st")
                || unit.equalsIgnoreCase ("ms");
        }

        static juce::String formatScriptNumber (float value)
        {
            if (std::abs (value - std::round (value)) < 0.0001f)
                return juce::String (juce::roundToInt (value));

            auto text = juce::String (value, 4);
            while (text.containsChar ('.') && text.endsWithChar ('0'))
                text = text.dropLastCharacters (1);
            if (text.endsWithChar ('.'))
                text = text.dropLastCharacters (1);
            return text;
        }

        static juce::String formatScriptValue (float value, const juce::String& unit)
        {
            auto text = formatScriptNumber (value);
            return isScriptUnit (unit) ? text + " " + unit : text;
        }

        static juce::String formatScriptRange (const ParameterDef& parameter)
        {
            return formatScriptValue (parameter.min, parameter.unit)
                + ".."
                + formatScriptValue (parameter.max, parameter.unit);
        }

        static const ParameterDef* findFirstExistingParameter (const ParameterModel& parameters,
                                                              std::initializer_list<const char*> ids,
                                                              const juce::String& exceptId)
        {
            for (auto* id : ids)
            {
                if (exceptId == id)
                    continue;
                if (auto* def = parameters.find (id))
                    return def;
            }
            return nullptr;
        }

        static const ParameterDef* choosePscriptMacroTarget (const ParameterModel& parameters,
                                                            const juce::String& sourceParameterId)
        {
            if (auto* fxTarget = findFirstExistingParameter (parameters,
                    { "delayMix", "reverbMix", "chorusMix", "phaserMix", "tapeMix", "stereoWidth", "volume" },
                    sourceParameterId))
                return fxTarget;

            for (const auto& def : parameters.getAll())
                if (def.id != sourceParameterId && def.visible && def.displayMode != "toggle")
                    return &def;

            return nullptr;
        }

        static std::pair<float, float> conservativePscriptTargetRange (const ParameterDef& target)
        {
            auto clamp = [&target] (float value)
            {
                return juce::jlimit (target.min, target.max, value);
            };

            if (target.id == "stereoWidth")
                return { clamp (1.0f), clamp (1.25f) };
            if (target.id == "volume")
                return { clamp (0.65f), clamp (1.0f) };
            if (target.unit.equalsIgnoreCase ("dB"))
                return { clamp (target.defaultValue), clamp (target.defaultValue + 3.0f) };
            if (target.id.containsIgnoreCase ("mix"))
                return { clamp (0.0f), clamp (0.35f) };
            if (target.max <= 1.0f && target.min >= 0.0f)
                return { clamp (target.defaultValue), clamp (target.defaultValue + 0.35f) };

            const auto width = target.max - target.min;
            return { clamp (target.defaultValue), clamp (target.defaultValue + width * 0.25f) };
        }

        static juce::String pscriptEventNameForElement (const LayoutElement& element,
                                                        const ParameterDef& parameter)
        {
            (void) element;
            return parameter.id;
        }

        static juce::String buildPscriptHandlerSnippet (const LayoutElement& element,
                                                       const ParameterDef& source,
                                                       const ParameterDef* target)
        {
            const auto displayName = element.label.isNotEmpty() ? element.label : source.name;
            juce::String snippet;
            snippet << "# Generated for " << elementTypeDisplayName (element.type)
                    << " \"" << displayName << "\".\n";
            snippet << "# This control still moves " << source.id
                    << "; the script makes it drive one more musical parameter.\n";
            snippet << "when knob \"" << pscriptEventNameForElement (element, source) << "\" moves:\n";
            snippet << "    let amount = value mapped " << formatScriptRange (source)
                    << " -> 0.0..1.0\n";

            if (target != nullptr)
            {
                const auto targetRange = conservativePscriptTargetRange (*target);
                snippet << "    set " << target->id << " to amount mapped 0.0..1.0 -> "
                        << formatScriptValue (targetRange.first, target->unit)
                        << ".."
                        << formatScriptValue (targetRange.second, target->unit)
                        << "\n";
            }
            else
            {
                snippet << "    print amount\n";
            }

            return snippet;
        }

        static bool tryAddRegistryParameterForExport (ParameterModel& parameters,
                                                      const juce::String& parameterId,
                                                      const juce::String& engineId)
        {
            if (parameterId.isEmpty() || parameters.contains (parameterId))
                return true;

            if (parameters.addFromRegistry (parameterId, engineId))
                return true;

            for (const auto& fallbackEngine : { juce::String ("synth"),
                                                juce::String ("sample"),
                                                juce::String ("fx") })
            {
                if (fallbackEngine == engineId)
                    continue;
                if (parameters.addFromRegistry (parameterId, fallbackEngine))
                    return true;
            }

            return false;
        }

        static int removeStalePresetValuesForExport (PatchCraftProject& project)
        {
            auto& parameters = project.getParameters();
            const auto engineId = project.getManifest().engine.isNotEmpty()
                ? project.getManifest().engine
                : project.getEngineType();
            parameters.ensureRegistryMetadata (engineId);

            int removed = 0;
            for (auto& preset : project.getPresets())
            {
                for (auto it = preset.values.begin(); it != preset.values.end();)
                {
                    if (tryAddRegistryParameterForExport (parameters, it->first, engineId)
                        || parameters.find (it->first) != nullptr)
                    {
                        ++it;
                    }
                    else
                    {
                        it = preset.values.erase (it);
                        ++removed;
                    }
                }
            }

            if (removed > 0)
                project.notifyChanged();

            return removed;
        }

        static juce::String validationWarningSummary (PatchCraftProject& project)
        {
            project.getDspGraph().ensureAuthoredOutput();

            const auto issues = project.getParameters().validateReferences (
                project.getLayout().getAll(), project.getDspGraph(), project.getPresets());

            juce::StringArray warnings;
            for (const auto& issue : issues)
            {
                if (issue.severity != "error")
                    warnings.add (issue.toString());
                if (warnings.size() >= 8)
                    break;
            }
            for (const auto& issue : project.getDspGraph().validateTypedGraph (project.getManifest().engine))
            {
                if (issue.severity != "error")
                    warnings.add (issue.toString());
                if (warnings.size() >= 8)
                    break;
            }

            return warnings.isEmpty()
                ? juce::String()
                : ("\n\nWarnings:\n" + warnings.joinIntoString ("\n"));
        }

        static juce::String safePublishFileStem (juce::String text)
        {
            text = text.trim();
            juce::String out;
            for (auto c : text)
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_')
                    out << c;
                else if (c == ' ' || c == '.')
                    out << '_';
            }
            return out.isNotEmpty() ? out : juce::String ("PatchCraftProduct");
        }

        static juce::String publishChoiceLabel (int choice)
        {
            switch (choice)
            {
                case 1: return "PatchCraft Instrument Pack";
                case 2: return "Standalone VST3 Plugin";
                case 3: return "Pack + Standalone VST3";
                default: return "PatchCraft Instrument Pack";
            }
        }

        static bool showSampleExportValidationIfBlocked (StudioMainComponent& owner,
                                                         const PatchCraftProject& project,
                                                         const juce::String& title)
        {
            const auto health = SampleMap::evaluateHealth (project.getSampleMap(),
                                                           project.getProjectFolder(),
                                                           project.getEngineType());
            if (! health.blocksExport())
                return false;

            juce::Component::SafePointer<StudioMainComponent> safeOwner (&owner);
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (health.exportMessage())
                    .withButton ("Open Sample Mapper")
                    .withButton ("Cancel Export")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                [safeOwner] (int result)
                {
                    if (result == 1)
                        if (auto* component = safeOwner.getComponent())
                            component->setBottomTab (BottomPanel::Page::Samples);
            });
            return true;
        }

        static bool showLaunchReadinessIfBlocked (StudioMainComponent& owner,
                                                  const PatchCraftProject& project,
                                                  const juce::String& title,
                                                  LaunchReadiness::Scope scope = LaunchReadiness::Scope::ExportPack)
        {
            const auto report = LaunchReadiness::evaluate (project, scope);
            if (report.errorCount == 0)
                return false;

            juce::StringArray lines;
            for (const auto& line : report.blockingErrors)
                lines.add ("- " + line);
            const auto message = "Launch Doctor found blocking issues:\n\n"
                + lines.joinIntoString ("\n")
                + "\n\nOpen Ship to review and fix them before exporting.";

            juce::Component::SafePointer<StudioMainComponent> safeOwner (&owner);
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (message)
                    .withButton ("Open Ship")
                    .withButton ("Cancel")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                [safeOwner] (int result)
                {
                    if (result == 1)
                        if (auto* component = safeOwner.getComponent())
                            component->setBottomTab (BottomPanel::Page::Export);
                });
            return true;
        }

        static juce::String studioScopedTabGroupId (const LayoutElement& tabPanel,
                                                    const juce::String& label)
        {
            if (tabPanel.id == "tabs")
                return LayoutElement::tabLabelToGroupId (label);
            return tabPanel.id + "__tab__" + LayoutElement::tabLabelToGroupId (label);
        }

        static juce::File studioPreferencesFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("PatchCraft")
                .getChildFile ("studio-preferences.json");
        }

        static bool readStudioTutorialPreference (bool fallback)
        {
            const auto file = studioPreferencesFile();
            if (! file.existsAsFile())
                return fallback;

            auto parsed = juce::JSON::parse (file);
            if (auto* object = parsed.getDynamicObject())
                if (object->hasProperty ("studioShowTutorials"))
                    return (bool) object->getProperty ("studioShowTutorials");

            return fallback;
        }

        static juce::DynamicObject::Ptr readStudioPreferencesObject()
        {
            const auto file = studioPreferencesFile();
            if (file.existsAsFile())
            {
                auto parsed = juce::JSON::parse (file);
                if (auto* object = parsed.getDynamicObject())
                    return object;
            }

            return new juce::DynamicObject();
        }

        static void writeStudioPreferencesObject (juce::DynamicObject::Ptr object)
        {
            auto file = studioPreferencesFile();
            if (object == nullptr || ! file.getParentDirectory().createDirectory())
                return;

            file.replaceWithText (juce::JSON::toString (juce::var (object.get()), true));
        }

        static juce::StringArray readRecentProjectPreference()
        {
            juce::StringArray paths;
            auto object = readStudioPreferencesObject();
            if (object == nullptr)
                return paths;

            if (auto* recent = object->getProperty ("recentProjects").getArray())
                for (const auto& item : *recent)
                    if (auto path = item.toString().trim(); path.isNotEmpty())
                        paths.addIfNotAlreadyThere (path);

            return paths;
        }

        static void writeRecentProjectPreference (const juce::StringArray& paths)
        {
            auto object = readStudioPreferencesObject();
            if (object == nullptr)
                object = new juce::DynamicObject();

            juce::Array<juce::var> recent;
            for (const auto& path : paths)
                if (path.isNotEmpty())
                    recent.add (path);

            object->setProperty ("recentProjects", juce::var (recent));
            writeStudioPreferencesObject (object);
        }

        static juce::String copilotTaskDescription (AiAssistService::TaskType task)
        {
            switch (task)
            {
                case AiAssistService::TaskType::BackgroundPrompt:           return "Art direction for backgrounds and product visuals";
                case AiAssistService::TaskType::SuggestLayout:              return "Improve the Player layout and interaction hierarchy";
                case AiAssistService::TaskType::SuggestControls:            return "Recommend useful controls mapped to real parameters";
                case AiAssistService::TaskType::SuggestMacroAssignments:    return "Plan expressive knobs, macros, and performance links";
                case AiAssistService::TaskType::GeneratePresetNames:        return "Create sellable preset names, categories, and tags";
                case AiAssistService::TaskType::GenerateProductDescription: return "Draft marketplace copy for the instrument or expansion";
                case AiAssistService::TaskType::DesignCritique:             return "Audit UI clarity, spacing, runtime parity, and flow";
                case AiAssistService::TaskType::SoundRecipe:                return "Create a sound-design recipe from the current patch";
                case AiAssistService::TaskType::WavetableRecipe:            return "Suggest wavetable movement and modulation ideas";
                case AiAssistService::TaskType::EqChain:                    return "Plan advanced EQ and dynamic tone-shaping moves";
                case AiAssistService::TaskType::ModulationPlan:             return "Plan LFOs, automation, macros, and motion";
                case AiAssistService::TaskType::BuildAssetGuidance:         return "Guide knob, slider, meter, and UI asset building";
                case AiAssistService::TaskType::ExportChecklist:            return "Check pack, licensing, runtime, and export readiness";
            }
            return "Run a PatchCraft Copilot action";
        }

        class CopilotDialogContent final : public juce::Component
        {
        public:
            CopilotDialogContent()
            {
                setSize (780, 640);

                title.setText ("PatchCraft Copilot", juce::dontSendNotification);
                title.setFont (juce::Font (24.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                subtitle.setText ("Choose a focused assistant action. Results are preview-first and do not mutate the project unless you explicitly use another Studio action.",
                                  juce::dontSendNotification);
                subtitle.setFont (juce::Font (12.5f));
                subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (subtitle);

                status.setText (AiAssistService::providerStatusText(), juce::dontSendNotification);
                status.setFont (juce::Font (12.0f));
                status.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                status.setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (status);

                auto setupButton = [] (juce::TextButton& button, bool primary)
                {
                    button.getProperties().set ("fontSize", primary ? 12.5 : 12.0);
                    button.getProperties().set ("bold", primary);
                    button.getProperties().set ("corner", 8.0);
                    if (primary)
                        button.getProperties().set ("primaryAction", true);
                };

                setupButton (settingsButton, true);
                settingsButton.setButtonText ("Open AI Settings");
                settingsButton.setTooltip ("Configure built-in templates, local llama.cpp/OpenAI-compatible endpoints, image APIs, and cloud integrations.");
                settingsButton.onClick = [this] { runAfterClose (onSettings); };
                addAndMakeVisible (settingsButton);

                setupButton (closeButton, false);
                closeButton.setButtonText ("Close");
                closeButton.onClick = [this] { runAfterClose (onClose); };
                addAndMakeVisible (closeButton);

                for (auto* button : { &backgroundButton, &assetButton, &publishButton })
                {
                    setupButton (*button, false);
                    button->getProperties().set ("workflowStep", true);
                    addAndMakeVisible (*button);
                }
                backgroundButton.setButtonText ("Generate Background\nCreate artwork from the current instrument context");
                assetButton.setButtonText ("Generate Image Asset\nCreate an editable visual asset for the canvas");
                publishButton.setButtonText ("Publish Draft\nStage and push the current pack to Plugin.club");

                backgroundButton.onClick = [this] { runAfterClose (onGenerateBackground); };
                assetButton.onClick = [this] { runAfterClose (onGenerateAsset); };
                publishButton.onClick = [this] { runAfterClose (onPublish); };

                taskViewport.setViewedComponent (&taskContent, false);
                taskViewport.setScrollBarsShown (true, false);
                taskViewport.setScrollBarThickness (8);
                addAndMakeVisible (taskViewport);

                for (auto task : AiAssistService::defaultTasks())
                {
                    auto button = std::make_unique<juce::TextButton>();
                    button->setButtonText (AiAssistService::displayName (task) + "\n" + copilotTaskDescription (task));
                    button->getProperties().set ("workflowStep", true);
                    button->getProperties().set ("headlineSize", 12.2);
                    button->getProperties().set ("detailSize", 10.2);
                    button->setTooltip (copilotTaskDescription (task));
                    button->onClick = [this, task]
                    {
                        auto callback = onTask;
                        runAfterClose ([callback, task]
                        {
                            if (callback)
                                callback (task);
                        });
                    };
                    taskContent.addAndMakeVisible (*button);
                    taskButtons.push_back (std::move (button));
                }
            }

            void paint (juce::Graphics& g) override
            {
                auto bounds = getLocalBounds().toFloat();
                juce::ColourGradient grad (juce::Colour (0xff10151d), bounds.getX(), bounds.getY(),
                                           juce::Colour (0xff080a0e), bounds.getRight(), bounds.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (bounds.reduced (1.0f), 12.0f);

                g.setColour (PatchCraftLookAndFeel::accent());
                g.fillRoundedRectangle (bounds.withHeight (4.0f), 2.0f);

                auto body = getLocalBounds().reduced (18);
                body.removeFromTop (124);
                auto quick = body.removeFromTop (78);
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.65f));
                g.drawRoundedRectangle (quick.toFloat(), 10.0f, 1.0f);

                body.removeFromTop (14);
                g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.78f));
                g.fillRoundedRectangle (body.toFloat(), 10.0f);
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.7f));
                g.drawRoundedRectangle (body.toFloat(), 10.0f, 1.0f);
            }

            void resized() override
            {
                auto bounds = getLocalBounds().reduced (18);
                auto header = bounds.removeFromTop (112);
                auto headerRight = header.removeFromRight (240);
                closeButton.setBounds (headerRight.removeFromTop (32).removeFromRight (82));
                headerRight.removeFromTop (12);
                settingsButton.setBounds (headerRight.removeFromTop (38));

                title.setBounds (header.removeFromTop (34));
                subtitle.setBounds (header.removeFromTop (36));
                status.setBounds (header.removeFromTop (26));

                auto quick = bounds.removeFromTop (78).reduced (12, 10);
                const int quickGap = 10;
                const int quickW = (quick.getWidth() - quickGap * 2) / 3;
                backgroundButton.setBounds (quick.removeFromLeft (quickW));
                quick.removeFromLeft (quickGap);
                assetButton.setBounds (quick.removeFromLeft (quickW));
                quick.removeFromLeft (quickGap);
                publishButton.setBounds (quick);

                bounds.removeFromTop (14);
                taskViewport.setBounds (bounds.reduced (10));
                layoutTaskButtons (taskViewport.getWidth() - 10);
            }

            std::function<void (AiAssistService::TaskType)> onTask;
            std::function<void()> onSettings;
            std::function<void()> onGenerateBackground;
            std::function<void()> onGenerateAsset;
            std::function<void()> onPublish;
            std::function<void()> onClose;

        private:
            void runAfterClose (std::function<void()> callback)
            {
                if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                    dialog->exitModalState (0);

                if (callback)
                    juce::MessageManager::callAsync (std::move (callback));
            }

            void layoutTaskButtons (int width)
            {
                width = juce::jmax (320, width);
                const int columns = width >= 640 ? 2 : 1;
                const int gap = 10;
                const int buttonH = 62;
                const int cellW = columns == 2 ? (width - gap) / 2 : width;

                int index = 0;
                for (auto& button : taskButtons)
                {
                    const int column = columns == 2 ? index % 2 : 0;
                    const int row = columns == 2 ? index / 2 : index;
                    button->setBounds (column * (cellW + gap),
                                       row * (buttonH + gap),
                                       cellW,
                                       buttonH);
                    ++index;
                }

                const int rows = columns == 2
                    ? (int) std::ceil ((double) taskButtons.size() / 2.0)
                    : (int) taskButtons.size();
                taskContent.setSize (width, juce::jmax (taskViewport.getHeight(),
                                                        rows * (buttonH + gap) + 8));
            }

            juce::Label title;
            juce::Label subtitle;
            juce::Label status;
            juce::TextButton settingsButton { "Open AI Settings" };
            juce::TextButton closeButton { "Close" };
            juce::TextButton backgroundButton;
            juce::TextButton assetButton;
            juce::TextButton publishButton;
            juce::Viewport taskViewport;
            juce::Component taskContent;
            std::vector<std::unique_ptr<juce::TextButton>> taskButtons;
        };

        static void writeStudioTutorialPreference (bool enabled)
        {
            auto object = readStudioPreferencesObject();
            if (object == nullptr)
                object = new juce::DynamicObject();
            object->setProperty ("studioShowTutorials", enabled);
            writeStudioPreferencesObject (object);
        }

        static bool readStudioTutorialModePreference (bool fallback)
        {
            const auto file = studioPreferencesFile();
            if (! file.existsAsFile())
                return fallback;

            auto parsed = juce::JSON::parse (file);
            if (auto* object = parsed.getDynamicObject())
                if (object->hasProperty ("studioTutorialMode"))
                    return (bool) object->getProperty ("studioTutorialMode");

            return fallback;
        }

        static void writeStudioTutorialModePreference (bool enabled)
        {
            auto object = readStudioPreferencesObject();
            if (object == nullptr)
                object = new juce::DynamicObject();
            object->setProperty ("studioTutorialMode", enabled);
            writeStudioPreferencesObject (object);
        }

        static juce::String defaultParameterForLibraryAsset (const PatchCraftProject& project,
                                                             ElementType type)
        {
            auto choose = [&] (std::initializer_list<const char*> ids) -> juce::String
            {
                for (auto* id : ids)
                    if (project.getParameters().find (id) != nullptr)
                        return id;
                return {};
            };

            if (type == ElementType::Knob)
                if (auto id = choose ({ "filterCutoff", "volume", "oscBlend", "delayMix", "reverbMix" }); id.isNotEmpty())
                    return id;

            if (type == ElementType::Slider)
                if (auto id = choose ({ "volume", "filterCutoff", "delayMix", "reverbMix" }); id.isNotEmpty())
                    return id;

            if (type == ElementType::Meter)
                if (auto id = choose ({ "volume" }); id.isNotEmpty())
                    return id;

            for (const auto& def : project.getParameters().getAll())
            {
                if (! def.visible || ! def.hostAutomatable)
                    continue;
                if (def.displayMode == "toggle" || def.displayMode == "stepped")
                    continue;
                return def.id;
            }

            return {};
        }
    }

    bool StudioMainComponent::isPreviewActive() const
    {
        return customerPreviewActive
            || graphAudioListen
            || (bottomPanel != nullptr && bottomPanel->isPreviewActive());
    }

    const SampleZoneDef* StudioMainComponent::getSelectedSampleZone() const
    {
        return bottomPanel != nullptr ? bottomPanel->getSelectedSampleZone() : nullptr;
    }

    void StudioMainComponent::rehomePackRuntimeToStudio()
    {
        if (packRuntime == nullptr)
            return;

        if (packRuntime->getParentComponent() != this)
        {
            if (auto* parent = packRuntime->getParentComponent())
                parent->removeChildComponent (packRuntime.get());
            addChildComponent (*packRuntime);
        }
    }

    void StudioMainComponent::exitCustomerPreviewIfActive()
    {
        if (! customerPreviewActive)
            return;

        graphAudioListen = false;
        customerPreviewActive = false;

        if (customerPreviewOverlay != nullptr)
            customerPreviewOverlay->exitPreview();

        if (packRuntime != nullptr)
        {
            packRuntime->deactivate();
            packRuntime->setVisible (false);
            rehomePackRuntimeToStudio();
        }

        if (topToolbar != nullptr)
            topToolbar->setPreviewActive (false, "Preview");
        if (canvasToolbar != nullptr)
            canvasToolbar->syncSectionTabFromOwner();

        refreshPreviewUiState();
    }

    void StudioMainComponent::refreshPreviewUiState()
    {
        if (topToolbar == nullptr)
            return;

        const bool onStack = bottomTab == BottomPanel::Page::DSP
                          || (bottomTab == BottomPanel::Page::Build
                              && bottomPanel != nullptr
                              && bottomPanel->getBuildSubPage() == BottomPanel::BuildSubPage::Stack);
        const bool active = customerPreviewActive || graphAudioListen;
        const juce::String idle = (onStack && ! customerPreviewActive) ? "Listen" : "Preview";
        topToolbar->setPreviewActive (active, idle);
    }

    void StudioMainComponent::setBottomTab (BottomPanel::Page p)
    {
        BottomPanel::BuildSubPage buildSub = bottomPanel != nullptr
            ? bottomPanel->getBuildSubPage()
            : BottomPanel::BuildSubPage::ImportSounds;

        if (project.getManifest().quickBuildMode && ! advancedBuildUnlocked)
        {
            // Preview lives in Brand; keep Test off the primary spine.
            if (p == BottomPanel::Page::Test)
                p = BottomPanel::Page::Branding;

            // Advanced-only destinations stay behind Advanced Build Mode.
            if (p == BottomPanel::Page::Dashboard
                || p == BottomPanel::Page::Widgets
                || p == BottomPanel::Page::Animation
                || p == BottomPanel::Page::OneShotMaker
                || p == BottomPanel::Page::ProjectBrowser)
            {
                p = BottomPanel::Page::Build;
                buildSub = BottomPanel::BuildSubPage::ImportSounds;
            }

            if (p == BottomPanel::Page::Samples)
            {
                buildSub = BottomPanel::BuildSubPage::ImportSounds;
                p = BottomPanel::Page::Build;
            }
            else if (p == BottomPanel::Page::Chop)
            {
                buildSub = BottomPanel::BuildSubPage::Chop;
                p = BottomPanel::Page::Build;
            }
            else if (p == BottomPanel::Page::DSP)
            {
                buildSub = BottomPanel::BuildSubPage::Stack;
                p = BottomPanel::Page::Build;
            }
            else if (p == BottomPanel::Page::MidiPlayground || p == BottomPanel::Page::ArpStudio)
            {
                buildSub = BottomPanel::BuildSubPage::Perform;
                p = BottomPanel::Page::Build;
            }
        }

        const bool leavingPreview = customerPreviewActive;
        exitCustomerPreviewIfActive();

        if (p != BottomPanel::Page::Design && showScriptEditorInsteadOfElements)
            closePscriptPanel();

        if (bottomTab == p && ! leavingPreview
            && (p != BottomPanel::Page::Build
                || bottomPanel == nullptr
                || bottomPanel->getBuildSubPage() == buildSub))
            return;

        if (p != BottomPanel::Page::DSP && graphAudioListen)
        {
            const bool stackBuild = p == BottomPanel::Page::Build
                                 && buildSub == BottomPanel::BuildSubPage::Stack;
            if (! stackBuild)
                setGraphAudioListen (false);
        }

        bottomTab = p;
        if (bottomPanel != nullptr)
        {
            if (p == BottomPanel::Page::Build)
                bottomPanel->setBuildSubPage (buildSub);
            else
                bottomPanel->setPage (p);
        }
        if (canvasToolbar) canvasToolbar->syncSectionTabFromOwner();
        if (p == BottomPanel::Page::Design
            && ! dspTutorialShownThisSession
            && getStudioTutorialsEnabled())
        {
            dspTutorialShownThisSession = true;
            juce::Component::SafePointer<StudioMainComponent> self (this);
            juce::MessageManager::callAsync ([self]
            {
                if (auto* component = self.getComponent())
                    component->showDspBuilderTutorial();
            });
        }
        // Hide/show sidebar/canvas/inspector based on the new tab.
        resized();
        refreshPreviewUiState();
        repaint();
    }

    void StudioMainComponent::setAdvancedBuildUnlocked (bool enabled)
    {
        if (advancedBuildUnlocked == enabled)
            return;

        advancedBuildUnlocked = enabled;
        if (canvasToolbar != nullptr)
            canvasToolbar->refresh();
        if (bottomPanel != nullptr)
            bottomPanel->refresh();
        repaint();
    }

    void StudioMainComponent::toggleSampleLibraryDrawerForSamples()
    {
        showSampleLibraryDrawer (! sampleLibraryDrawerOpen);
    }

    void StudioMainComponent::layoutSampleLibraryInEditor (juce::Rectangle<int> areaInEditor)
    {
        if (assetLibraryPanel == nullptr || ! sampleLibraryDrawerOpen)
            return;

        assetLibraryPanel->setBounds (areaInEditor);
        assetLibraryPanel->setVisible (true);
        assetLibraryPanel->toFront (false);
    }

    void StudioMainComponent::showSampleLibraryDrawer (bool shouldShow)
    {
        sampleLibraryDrawerOpen = shouldShow;
        auto* mapper = bottomPanel != nullptr ? bottomPanel->getSampleMapper() : nullptr;

        if (sampleLibraryDrawerOpen)
        {
            // Host the Sound Library inside the Sample Editor (works for both
            // the docked Samples tab and the floating Full mapper window).
            if (assetLibraryPanel != nullptr && isPanelFloating (assetLibraryPanel.get()))
                togglePanelFloat (assetLibraryPanel.get(), {});

            if (! isPanelFloating (bottomPanel.get())
                && bottomTab != BottomPanel::Page::Samples)
            {
                if (project.getManifest().quickBuildMode && ! advancedBuildUnlocked)
                    setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Samples);
                mapper = bottomPanel != nullptr ? bottomPanel->getSampleMapper() : nullptr;
            }

            if (assetLibraryPanel != nullptr)
            {
                assetLibraryPanel->showSoundsLibrary();
                assetLibraryPanel->refresh();

                if (mapper != nullptr)
                {
                    mapper->addAndMakeVisible (*assetLibraryPanel);
                    mapper->setLibraryDrawerOpen (true);
                }
                else
                {
                    addAndMakeVisible (*assetLibraryPanel);
                    assetLibraryPanel->toFront (false);
                }
            }
        }
        else
        {
            if (mapper != nullptr)
                mapper->setLibraryDrawerOpen (false);

            if (assetLibraryPanel != nullptr)
            {
                addChildComponent (*assetLibraryPanel);
                assetLibraryPanel->setVisible (false);
            }
        }

        resized();
        if (mapper != nullptr)
            mapper->resized();
        repaint();
    }

    bool StudioMainComponent::captureTutorialScreenshots (const juce::File& outputFolder, juce::String& error)
    {
        if (! outputFolder.createDirectory())
        {
            error = "Could not create screenshot folder: " + outputFolder.getFullPathName();
            return false;
        }

        struct Shot
        {
            BottomPanel::Page page;
            const char* fileName;
        };

        const Shot shots[] = {
            { BottomPanel::Page::Dashboard,       "studio-workflow.png" },
            { BottomPanel::Page::Design,         "studio-design.png" },
            { BottomPanel::Page::Samples,   "studio-samples.png" },
            { BottomPanel::Page::OneShotMaker,   "studio-one-shot.png" },
            { BottomPanel::Page::MidiPlayground, "studio-midi.png" },
            { BottomPanel::Page::ArpStudio,      "studio-arp-studio.png" },
            { BottomPanel::Page::Widgets,          "studio-build.png" },
            { BottomPanel::Page::Animation,      "studio-animation-lab.png" },
            { BottomPanel::Page::Branding,       "studio-brand-lab.png" },
            { BottomPanel::Page::Test,           "studio-test.png" },
            { BottomPanel::Page::Export,         "studio-launch.png" }
        };

        dspTutorialShownThisSession = true;
        juce::PNGImageFormat png;

        for (const auto& shot : shots)
        {
            setBottomTab (shot.page);
            resized();
            repaint();

            auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            if (! image.isValid())
            {
                error = "Could not render screenshot: " + juce::String (shot.fileName);
                return false;
            }

            const auto file = outputFolder.getChildFile (shot.fileName);
            std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());
            if (out == nullptr || ! out->openedOk() || ! png.writeImageToStream (image, *out))
            {
                error = "Could not write screenshot: " + file.getFullPathName();
                return false;
            }
        }

        error.clear();
        return true;
    }

    // SettingsWindowHolder is just a unique_ptr<SettingsWindow> with a deleter
    // that runs on the message thread (done implicitly by the host caller).
    class StudioMainComponent::SettingsWindowHolder
    {
    public:
        std::unique_ptr<SettingsWindow> w;
    };

    void StudioMainComponent::openSettings()
    {
        if (settingsWindow != nullptr && settingsWindow->w != nullptr)
        {
            settingsWindow->w->toFront (true);
            return;
        }
        settingsWindow = std::make_unique<SettingsWindowHolder>();
        juce::Component::SafePointer<StudioMainComponent> self (this);
        settingsWindow->w = std::make_unique<SettingsWindow> (audioService, ai, project,
            [self]
            {
                juce::MessageManager::callAsync ([self]
                {
                    if (auto* p = self.getComponent()) p->settingsWindow.reset();
                });
            });
    }

    StudioMainComponent::StudioMainComponent()
    {
        setOpaque (true);
        refreshTooltipWindowState();
        menuBar.setModel (this);

        topToolbar      = std::make_unique<TopToolbar> (*this);
        elementPalette  = std::make_unique<ElementPalette> (*this);
        assetLibraryPanel = std::make_unique<BuiltAssetLibraryComponent> (*this);
        expansionLibraryPanel = std::make_unique<ExpansionLibraryPanel> (*this);
        layersPanel     = std::make_unique<LayersPanel> (*this);
        scriptEditor    = std::make_unique<ScriptEditorComponent> (*this);
        scriptEditor->onPopOut = [this] { togglePanelFloat (scriptEditor.get(), "pScript"); };
        scriptEditor->onClose = [this]
        {
            closePscriptPanel();
        };
        canvasEditor    = std::make_unique<CanvasEditor> (*this);
        canvasToolbar   = std::make_unique<CanvasToolbar> (*this, *canvasEditor);
        inspectorPanel  = std::make_unique<InspectorPanel> (*this);
        inspectorViewport = std::make_unique<juce::Viewport> ("inspectorViewport");
        presetsPanel    = std::make_unique<PresetsComponent> (*this);
        bottomPanel     = std::make_unique<BottomPanel> (*this);
        packRuntime     = std::make_unique<PackRuntimeHost> (*this);
        addChildComponent (*packRuntime);
        packRuntime->setVisible (false);

        customerPreviewOverlay = std::make_unique<CustomerPreviewOverlay> (*this);
        customerPreviewOverlay->onExit = [this] { toggleCustomerPreview(); };
        addChildComponent (*customerPreviewOverlay);

        inspectorViewport->setViewedComponent (inspectorPanel.get(), false);
        inspectorViewport->setScrollBarsShown (true, false);
        inspectorViewport->setScrollBarThickness (8);
        inspectorViewport->setWantsKeyboardFocus (false);

        addAndMakeVisible (menuBar);
        addAndMakeVisible (*topToolbar);
        addAndMakeVisible (*elementPalette);
        addChildComponent (*assetLibraryPanel);
        addChildComponent (*expansionLibraryPanel);
        addChildComponent (*layersPanel);
        addChildComponent (*scriptEditor);
        addAndMakeVisible (*canvasToolbar);
        addAndMakeVisible (*canvasEditor);
        addAndMakeVisible (*inspectorViewport);
        addChildComponent (*presetsPanel);
        addAndMakeVisible (*bottomPanel);
        if (canvasToolbar != nullptr)
            canvasToolbar->syncSectionTabFromOwner();

        tutorialOverlay = std::make_unique<TutorialModeOverlay> (*this);
        tutorialOverlay->getProperties().set ("tutorialIgnore", true);
        const bool tutorialModeOn = readStudioTutorialModePreference (false);
        if (tutorialModeOn)
            addAndMakeVisible (*tutorialOverlay);
        else
            addChildComponent (*tutorialOverlay);
        tutorialOverlay->setActive (tutorialModeOn);

        // Left tabs expose the complete authoring browser at startup.
        leftTabs.addTab ("Elements",   PatchCraftLookAndFeel::panel(), -1);
        leftTabs.addTab ("Layers",     PatchCraftLookAndFeel::panel(), -1);
        leftTabs.addTab ("Library",    PatchCraftLookAndFeel::panel(), -1);
        leftTabs.addTab ("pScript",    PatchCraftLookAndFeel::panel(), -1);
        leftTabs.setCurrentTabIndex (0, juce::dontSendNotification);
        addAndMakeVisible (leftTabs);
        leftCollapseButton.getProperties().set ("smallButton", true);
        leftCollapseButton.setTooltip ("Collapse or expand the Elements/Layers panel");
        leftCollapseButton.onClick = [this]
        {
            leftPanelCollapsed = ! leftPanelCollapsed;
            leftCollapseButton.setButtonText (leftPanelCollapsed ? ">" : "<");
            resized();
            repaint();
        };
        addAndMakeVisible (leftCollapseButton);

        leftPopButton.getProperties().set ("smallButton", true);
        leftPopButton.getProperties().set ("fontSize", 10.0);
        leftPopButton.setTooltip ("Pop out the active left panel.");
        leftPopButton.onClick = [this]
        {
            if (showLibraryInsteadOfElements)
                togglePanelFloat (assetLibraryPanel.get(), "Library");
            else if (showLayersInsteadOfElements)
                togglePanelFloat (layersPanel.get(), "Layers");
            else if (showScriptEditorInsteadOfElements)
                togglePanelFloat (scriptEditor.get(), "pScript");
            else
                togglePanelFloat (elementPalette.get(), "Elements");
        };
        addAndMakeVisible (leftPopButton);

        rightCollapseButton.getProperties().set ("smallButton", true);
        rightCollapseButton.setTooltip ("Collapse or expand the Inspector panel");
        rightCollapseButton.onClick = [this]
        {
            rightPanelCollapsed = ! rightPanelCollapsed;
            rightCollapseButton.setButtonText (rightPanelCollapsed ? "<" : ">");
            resized();
            repaint();
        };
        addAndMakeVisible (rightCollapseButton);

        rightPopButton.getProperties().set ("smallButton", true);
        rightPopButton.getProperties().set ("fontSize", 10.0);
        rightPopButton.setTooltip ("Pop out the active right properties panel.");
        rightPopButton.onClick = [this]
        {
            if (rightTabIndex == 1)
                togglePanelFloat (presetsPanel.get(), "Presets");
            else
                togglePanelFloat (inspectorViewport.get(), "Inspector");
        };
        addAndMakeVisible (rightPopButton);

        // Right panel tabs: Inspector (default), Presets.
        rightTabs.addTab ("Inspector", PatchCraftLookAndFeel::panel(), -1);
        rightTabs.addTab ("Presets",   PatchCraftLookAndFeel::panel(), -1);
        rightTabs.setCurrentTabIndex (0, juce::dontSendNotification);
        addAndMakeVisible (rightTabs);
        for (int i = 0; i < rightTabs.getNumTabs(); ++i)
        {
            auto* tb = rightTabs.getTabButton (i);
            tb->onClick = [this, i]
            {
                rightTabs.setCurrentTabIndex (i);
                rightTabIndex = i;
                inspectorViewport->setVisible (i == 0 || isPanelFloating (inspectorViewport.get()));
                presetsPanel ->setVisible (i == 1);
                if (i == 1) presetsPanel->refresh();
                resized();
            };
        }

        for (int i = 0; i < leftTabs.getNumTabs(); ++i)
        {
            auto* tb = leftTabs.getTabButton (i);
            tb->onClick = [this, i] {
                leftTabs.setCurrentTabIndex (i);
                showLayersInsteadOfElements      = (i == 1);
                showLibraryInsteadOfElements     = (i == 2);
                if (i != 3 && showScriptEditorInsteadOfElements)
                    closePscriptPanel();
                showScriptEditorInsteadOfElements = (i == 3);
                elementPalette->setVisible ((i == 0 || i == 3) && ! leftPanelCollapsed);
                layersPanel->setVisible (showLayersInsteadOfElements);
                assetLibraryPanel->setVisible (showLibraryInsteadOfElements);
                if (showLayersInsteadOfElements)
                    layersPanel->refresh();
                if (showScriptEditorInsteadOfElements)
                    focusPscriptPanel();
                else if (canvasToolbar != nullptr)
                    canvasToolbar->syncSectionTabFromOwner();
                resized();
            };
        }

        project.addListener (this);
        project.getLiveValues().addListener (this);
        refreshPreviewUiState();
    }

    StudioMainComponent::~StudioMainComponent()
    {
        menuBar.setModel (nullptr);
        settingsWindow.reset();
        project.getLiveValues().removeListener (this);
        project.removeListener (this);
    }

    void StudioMainComponent::liveValueChanged (const juce::String&, float)
    {
        // Canvas and test controls derive directly from LiveValueStore.
        // Avoid full panel rebuilds while dragging controls; that stalls audio/UI response.
        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        if (nowMs - lastLiveValueUiRepaintMs < 33.0)
            return;

        lastLiveValueUiRepaintMs = nowMs;
        if (canvasEditor != nullptr)
            canvasEditor->repaint();
        if (bottomPanel != nullptr)
            bottomPanel->repaint();
        if (inspectorPanel != nullptr)
            inspectorPanel->repaint();
    }

    void StudioMainComponent::projectChanged()
    {
        refreshTooltipWindowState();
        refreshAllPanels();
    }

    void StudioMainComponent::projectChanged (PatchCraftProject::ChangeScope scope)
    {
        if (scope == PatchCraftProject::ChangeScope::dspRealtime)
        {
            refreshTooltipWindowState();
            topToolbar->setProjectName (project.getManifest().instrumentName,
                                        project.hasUnsavedChanges());
            if (bottomPanel != nullptr)
                bottomPanel->repaint();
            if (canvasEditor != nullptr)
                canvasEditor->repaint();
            return;
        }
        else if (scope == PatchCraftProject::ChangeScope::layout)
        {
            refreshTooltipWindowState();
            topToolbar->setProjectName (project.getManifest().instrumentName,
                                        project.hasUnsavedChanges());
            if (inspectorPanel != nullptr)
                inspectorPanel->refresh();
            if (layersPanel != nullptr)
                layersPanel->repaint();
            if (bottomPanel != nullptr)
                bottomPanel->repaint();
            if (canvasEditor != nullptr)
                canvasEditor->repaint();
            return;
        }

        projectChanged();
    }

    void StudioMainComponent::refreshTooltipWindowState()
    {
        studioTooltipWindow.setMillisecondsBeforeTipAppears (
            project.getManifest().playerShowParameterGuidance ? 650 : std::numeric_limits<int>::max() / 4);
    }

    void StudioMainComponent::refreshAllPanels()
    {
        topToolbar->setProjectName (project.getManifest().instrumentName,
                                    project.hasUnsavedChanges());
        bottomPanel->refresh();
        // Debounced reload only — never force a synchronous pack rebuild from a
        // general UI refresh (that made Brand / tab switches multi-second).
        if (packRuntime != nullptr && ! customerPreviewActive)
            packRuntime->requestReload();
        inspectorPanel->refresh();
        layersPanel->refresh();
        if (presetsPanel) presetsPanel->refresh();
        assetLibraryPanel->refresh();
        if (canvasToolbar)
        {
            canvasToolbar->refresh();
            canvasToolbar->syncSectionTabFromOwner();
        }
        canvasEditor->repaint();
    }

    void StudioMainComponent::refreshCanvasToolbar()
    {
        if (canvasToolbar)
        {
            canvasToolbar->refresh();
            canvasToolbar->syncSectionTabFromOwner();
        }
    }

    juce::String StudioMainComponent::getCurrentDspPatchSectionLabel() const
    {
        return bottomPanel != nullptr ? bottomPanel->getDspPatchSectionLabel() : juce::String ("Source");
    }

    void StudioMainComponent::addElementToCanvas (ElementType type, juce::String parameterId)
    {
        if (canvasEditor == nullptr)
            return;

        setBottomTab (BottomPanel::Page::Design);
        const auto& canvas = project.getCanvasSize();
        canvasEditor->addElementAt (type,
            { canvas.width / 2 - 48, canvas.height / 2 - 48 },
            std::move (parameterId));
    }

    void StudioMainComponent::addMixerChannelToCanvas()
    {
        if (canvasEditor == nullptr)
            return;

        const auto& canvas = project.getCanvasSize();
        canvasEditor->addMixerChannelAt ({ canvas.width / 2 - 58, canvas.height / 2 - 130 });
    }

    void StudioMainComponent::addDrumMachineControlsToCanvas()
    {
        if (canvasEditor == nullptr)
            return;

        const auto& canvas = project.getCanvasSize();
        canvasEditor->addDrumMachineControlLayout ({ juce::jmax (24, canvas.width / 2 - 540),
                                                     juce::jmax (24, canvas.height / 2 - 210) });
    }

    void StudioMainComponent::addModuleToCanvas (const juce::String& moduleType)
    {
        if (canvasEditor == nullptr)
            return;

        setBottomTab (BottomPanel::Page::Design);
        const auto& canvas = project.getCanvasSize();
        canvasEditor->addModuleLayout (moduleType, { canvas.width / 2 - 140, canvas.height / 2 - 60 });
    }

    void StudioMainComponent::addModuleToCanvasAt (const juce::String& moduleType, juce::Point<int> canvasPosition)
    {
        if (canvasEditor == nullptr)
            return;

        setBottomTab (BottomPanel::Page::Design);
        canvasEditor->addModuleLayout (moduleType, canvasPosition);
    }

    void StudioMainComponent::addElementToCanvasAt (ElementType type, juce::Point<int> canvasPosition, juce::String parameterId)
    {
        if (canvasEditor == nullptr)
            return;

        setBottomTab (BottomPanel::Page::Design);
        canvasEditor->addElementAt (type, canvasPosition, std::move (parameterId));
    }

    void StudioMainComponent::enterDawPreviewMode()
    {
        setBottomTab (BottomPanel::Page::Branding);
        if (! customerPreviewActive)
            toggleCustomerPreview();
    }

    void StudioMainComponent::addLibraryAssetToCanvas (const juce::String& category, const juce::File& file,
                                                       int frames, bool vertical,
                                                       juce::Point<int> canvasPosition)
    {
        auto addBackgroundLayer = [this] (const juce::File& imageFile)
        {
            if (! imageFile.existsAsFile())
                return;

            project.performLayoutEdit ("Add background layer", [&] (LayoutModel& layout)
            {
                const auto& canvas = project.getCanvasSize();
                auto* background = layout.find ("background");
                if (background == nullptr)
                {
                    LayoutElement layer;
                    layer.type = ElementType::Image;
                    layer.id = "background";
                    layer.label = "Background";
                    layer.x = 0;
                    layer.y = 0;
                    layer.width = juce::jmax (1, canvas.width);
                    layer.height = juce::jmax (1, canvas.height);
                    layer.locked = true;
                    layer.asset = imageFile.getFullPathName();
                    layout.add (layer);
                }
                else
                {
                    background->type = ElementType::Image;
                    background->label = background->label.isNotEmpty() ? background->label : "Background";
                    background->x = 0;
                    background->y = 0;
                    background->width = juce::jmax (1, canvas.width);
                    background->height = juce::jmax (1, canvas.height);
                    background->locked = true;
                    background->visible = true;
                    background->asset = imageFile.getFullPathName();
                }
            });
        };

        if (category.equalsIgnoreCase ("templates"))
        {
            juce::Component::SafePointer<StudioMainComponent> safe (this);
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Load Template")
                    .withMessage ("Loading a template replaces the current instrument design, controls, DSP graph, presets, samples, and artwork.\n\n"
                                  "It does not merge with the existing template. Save first if you need to keep the current work.")
                    .withButton ("Load Template")
                    .withButton ("Cancel")
                    .withIconType (juce::MessageBoxIconType::QuestionIcon),
                [safe, file] (int result)
                {
                    if (result != 1)
                        return;
                    if (auto* component = safe.getComponent())
                        component->loadFactoryDemo (file);
                });
            return;
        }

        if (category.equalsIgnoreCase ("backgrounds"))
        {
            if (! file.existsAsFile())
                return;

            project.backgroundImageRelative = file.getFullPathName();
            project.getManifest().backgroundImage = project.backgroundImageRelative;
            addBackgroundLayer (file);
            assets.clear();
            project.notifyChanged();
            return;
        }

        if (category.equalsIgnoreCase ("sounds"))
        {
            if (! file.existsAsFile())
                return;

            juce::Array<juce::File> files;
            files.add (file);
            importSampleFiles (files);
            setBottomTab (BottomPanel::Page::Samples);
            return;
        }

        auto type = ElementType::Image;
        if (category.startsWithIgnoreCase ("knob"))
            type = ElementType::Knob;
        else if (category.startsWithIgnoreCase ("slider"))
            type = ElementType::Slider;
        else if (category.startsWithIgnoreCase ("meter"))
            type = ElementType::Meter;

        auto isRenderImage = [] (const juce::File& candidate)
        {
            const auto extension = candidate.getFileExtension().toLowerCase();
            return extension == ".png" || extension == ".jpg" || extension == ".jpeg"
                || extension == ".gif" || extension == ".webp";
        };
        auto sourceSidecarRenderFile = [] (const juce::File& candidate)
        {
            const auto name = candidate.getFileName();
            for (const auto& suffix : { juce::String (".patchcraft-knob.json"),
                                        juce::String (".patchcraft-slider.json"),
                                        juce::String (".patchcraft-meter.json") })
            {
                if (name.endsWithIgnoreCase (suffix))
                {
                    const auto stem = name.dropLastCharacters (suffix.length());
                    return candidate.getParentDirectory().getChildFile (stem + ".png");
                }
            }
            return juce::File();
        };

        auto renderFile = file;
        if (type != ElementType::Image)
        {
            const auto sourcePng = sourceSidecarRenderFile (file);
            if (sourcePng.existsAsFile())
                renderFile = sourcePng;
            else if (! isRenderImage (renderFile))
            {
                const auto siblingPng = file.withFileExtension ("png");
                if (siblingPng.existsAsFile())
                    renderFile = siblingPng;
            }
        }

        auto& canvas = project.getCanvasSize();
        int assetWidth = type == ElementType::Slider ? 52 : (type == ElementType::Image ? 240 : 112);
        int assetHeight = type == ElementType::Slider ? 220 : (type == ElementType::Image ? 160 : 112);
        float contentPadding = 0.0f;
        auto inferFramesFromImage = [] (const juce::Image& image, int requestedFrames, bool isVertical)
        {
            int detectedFrames = juce::jmax (0, requestedFrames);
            if (image.isValid() && detectedFrames <= 1)
            {
                const int w = juce::jmax (1, image.getWidth());
                const int h = juce::jmax (1, image.getHeight());
                const int inferred = isVertical ? juce::roundToInt ((double) h / (double) w)
                                                : juce::roundToInt ((double) w / (double) h);
                if (inferred > detectedFrames)
                    detectedFrames = inferred;
            }
            return juce::jmax (1, detectedFrames);
        };
        auto opaqueBoundsForFrame = [] (const juce::Image& image, int frameIndex, int frameCount, bool isVertical) -> juce::Rectangle<int>
        {
            if (! image.isValid())
                return {};

            const int safeFrames = juce::jmax (1, frameCount);
            const int frameWidth = isVertical ? image.getWidth() : juce::jmax (1, image.getWidth() / safeFrames);
            const int frameHeight = isVertical ? juce::jmax (1, image.getHeight() / safeFrames) : image.getHeight();
            const int frameX = isVertical ? 0 : frameIndex * frameWidth;
            const int frameY = isVertical ? frameIndex * frameHeight : 0;

            int left = frameWidth, top = frameHeight, right = -1, bottom = -1;
            for (int y = 0; y < frameHeight; ++y)
                for (int x = 0; x < frameWidth; ++x)
                    if (image.getPixelAt (frameX + x, frameY + y).getAlpha() > 8)
                    {
                        left = juce::jmin (left, x);
                        top = juce::jmin (top, y);
                        right = juce::jmax (right, x);
                        bottom = juce::jmax (bottom, y);
                    }

            if (right < left || bottom < top)
                return {};
            return { left, top, right - left + 1, bottom - top + 1 };
        };

        int detectedFrames = juce::jmax (0, frames);
        bool renderImageValid = false;
        if (auto image = type == ElementType::Image
                ? assets.loadImage (renderFile)
                : assets.loadControlFilmstrip (renderFile, frames, vertical);
            image.isValid())
        {
            renderImageValid = true;
            detectedFrames = inferFramesFromImage (image, frames, vertical);
            const int safeFrames = juce::jmax (1, detectedFrames);
            const int frameWidth = vertical ? image.getWidth()
                                            : juce::jmax (1, image.getWidth() / safeFrames);
            const int frameHeight = vertical ? juce::jmax (1, image.getHeight() / safeFrames)
                                             : image.getHeight();
            assetWidth = juce::jlimit (24, 320, frameWidth);
            assetHeight = juce::jlimit (24, 520, frameHeight);

            if (type != ElementType::Image)
            {
                if (const auto opaque = opaqueBoundsForFrame (image, 0, safeFrames, vertical); ! opaque.isEmpty())
                {
                    assetWidth = juce::jlimit (24, 320, opaque.getWidth());
                    assetHeight = juce::jlimit (24, 520, opaque.getHeight());
                    const int marginX = juce::jmin (opaque.getX(), frameWidth - opaque.getRight());
                    const int marginY = juce::jmin (opaque.getY(), frameHeight - opaque.getBottom());
                    contentPadding = -(float) juce::jmin (marginX, marginY);
                }
            }
        }
        project.performLayoutEdit ("Add library asset", [&] (LayoutModel& layout)
        {
            const int maxX = juce::jmax (0, canvas.width - assetWidth);
            const int maxY = juce::jmax (0, canvas.height - assetHeight);
            const bool hasDropPosition = canvasPosition.x >= 0 && canvasPosition.y >= 0;

            LayoutElement element;
            element.type = type;
            element.id = layout.generateUniqueId (elementTypeToString (type) + "_");
            element.label = file.getFileNameWithoutExtension();
            element.x = hasDropPosition
                ? juce::jlimit (0, maxX, canvasPosition.x - assetWidth / 2)
                : canvas.width / 2 - assetWidth / 2;
            element.y = hasDropPosition
                ? juce::jlimit (0, maxY, canvasPosition.y - assetHeight / 2)
                : canvas.height / 2 - assetHeight / 2;
            element.width = assetWidth;
            element.height = assetHeight;
            if (type == ElementType::Image)
            {
                element.asset = renderFile.getFullPathName();
                element.parameterId = {};
            }
            else
            {
                if (renderImageValid && renderFile.existsAsFile())
                    element.filmstripAsset = renderFile.getFullPathName();
                element.filmstripFrames = detectedFrames;
                element.filmstripVertical = vertical;
                element.parameterId = {};
                element.controlPreviewValue = 0.5f;
                element.labelPosition = "hidden";
                element.labelSpacing = 0.0f;
                element.labelOffsetX = 0.0f;
                element.labelOffsetY = 0.0f;
                element.contentPadding = contentPadding;
            }
            layout.add (element);
        });

        setSelectedElementId (project.getLayout().getAll().back().id);
    }

    bool StudioMainComponent::applyBrandingAsset (const juce::String& category, const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        const auto extension = file.getFileExtension().toLowerCase();
        if (extension != ".png" && extension != ".jpg" && extension != ".jpeg"
            && extension != ".gif" && extension != ".webp")
            return false;

        auto& manifest = project.getManifest();
        auto target = category.toLowerCase();
        bool sidecarProvidedTitleTheme = false;
        const auto sidecar = file.withFileExtension ("patchcraft-branding.json");
        if (sidecar.existsAsFile())
        {
            auto parsed = juce::JSON::parse (sidecar);
            if (auto* obj = parsed.getDynamicObject())
            {
                const auto sidecarTarget = obj->getProperty ("target").toString().toLowerCase();
                if (sidecarTarget.isNotEmpty())
                    target = sidecarTarget;
                if (obj->hasProperty ("playerTitleBarTheme"))
                {
                    manifest.playerTitleBarTheme = obj->getProperty ("playerTitleBarTheme").toString();
                    sidecarProvidedTitleTheme = manifest.playerTitleBarTheme.isNotEmpty();
                }
                if (obj->hasProperty ("playerTitleTextPlacement"))
                    manifest.playerTitleTextPlacement = obj->getProperty ("playerTitleTextPlacement").toString();
                if (obj->hasProperty ("playerTitleButtonStyle"))
                    manifest.playerTitleButtonStyle = obj->getProperty ("playerTitleButtonStyle").toString();
                if (obj->hasProperty ("playerTitleFontFamily"))
                    manifest.playerTitleFontFamily = obj->getProperty ("playerTitleFontFamily").toString();
            }
        }

        const auto path = file.getFullPathName();
        if (target.contains ("font"))
        {
            auto name = file.getFileNameWithoutExtension().fromFirstOccurrenceOf ("font_", false, true)
                .replaceCharacter ('_', ' ')
                .trim();
            if (name.isEmpty())
                name = "Default";
            if (name.equalsIgnoreCase ("segoe ui")) name = "Segoe UI";
            else if (name.equalsIgnoreCase ("arial")) name = "Arial";
            else if (name.equalsIgnoreCase ("verdana")) name = "Verdana";
            else if (name.equalsIgnoreCase ("georgia")) name = "Georgia";
            else if (name.equalsIgnoreCase ("consolas")) name = "Consolas";
            else if (name.equalsIgnoreCase ("default")) name = "Default";
            manifest.playerTitleFontFamily = name;
            project.notifyChanged();
            refreshAllPanels();
            return true;
        }

        const bool titleTarget = target.contains ("title")
                              || target.contains ("banner")
                              || target.contains ("texture")
                              || target.contains ("template");
        const bool logoTarget = target.contains ("logo")
                             || target.contains ("icon")
                             || target.contains ("badge");

        if (titleTarget || ! logoTarget)
        {
            manifest.playerTitleBannerImage = path;
            if (! sidecarProvidedTitleTheme)
                manifest.playerTitleBarTheme = "custom";
            project.performLayoutEdit ("Use Player chrome title banner", [&] (LayoutModel& layout)
            {
                juce::StringArray idsToRemove;
                for (const auto& element : layout.getAll())
                    if (element.id == "player_titlebar_artwork"
                        || element.id == "player_titlebar_mock"
                        || element.semanticRole == "playerTitleBarArtwork"
                        || element.semanticRole == "playerTitleBar")
                        idsToRemove.addIfNotAlreadyThere (element.id);

                for (const auto& id : idsToRemove)
                    layout.remove (id);
            });
        }
        if (logoTarget)
            manifest.playerLogoImage = path;

        project.notifyChanged();
        assets.clear();
        refreshAllPanels();
        return true;
    }

    void StudioMainComponent::setSelectedElementId (juce::String id)
    {
        if (selectedElementId == id && selectedElementIds.size() == (id.isEmpty() ? 0 : 1)) return;
        selectedElementId = std::move (id);
        selectedElementIds.clear();
        if (selectedElementId.isNotEmpty())
            selectedElementIds.add (selectedElementId);
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (bottomPanel != nullptr) bottomPanel->refreshDesignSelection();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    bool StudioMainComponent::isElementSelected (const juce::String& id) const
    {
        return selectedElementIds.contains (id);
    }

    void StudioMainComponent::setSelectedElementIds (juce::StringArray ids)
    {
        ids.removeEmptyStrings();
        selectedElementIds = std::move (ids);
        selectedElementId = selectedElementIds.isEmpty() ? juce::String() : selectedElementIds[0];
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (bottomPanel != nullptr) bottomPanel->refreshDesignSelection();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    void StudioMainComponent::toggleSelectedElementId (const juce::String& id)
    {
        if (id.isEmpty()) return;
        if (selectedElementIds.contains (id))
            selectedElementIds.removeString (id);
        else
            selectedElementIds.add (id);
        selectedElementId = selectedElementIds.isEmpty() ? juce::String() : selectedElementIds[0];
        canvasEditor->selectionChanged();
        inspectorPanel->selectionChanged();
        layersPanel->refresh();
        if (bottomPanel != nullptr) bottomPanel->refreshDesignSelection();
        if (canvasToolbar != nullptr) canvasToolbar->refresh();
    }

    void StudioMainComponent::clearSelection()
    {
        setSelectedElementIds ({});
    }

    void StudioMainComponent::openControlNodeEditor (const juce::String& requestedElementId)
    {
        const auto elementId = requestedElementId.isNotEmpty() ? requestedElementId : selectedElementId;
        const auto* element = project.getLayout().find (elementId);
        const bool bindable = element != nullptr
            && (element->type == ElementType::Knob || element->type == ElementType::Slider
                || element->type == ElementType::Button || element->type == ElementType::Toggle
                || element->type == ElementType::Dropdown || element->type == ElementType::ValueDisplay
                || element->type == ElementType::MacroControl || element->type == ElementType::SampleDropZone);
        if (! bindable)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Control Node Editor",
                                                    "Select a knob, slider, button, dropdown, value display, macro, or drop zone first.");
            return;
        }

        setSelectedElementId (elementId);
        auto* content = new ControlNodeEditor (*this, elementId);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Sound Connection - " + (element->label.isNotEmpty() ? element->label : element->id);
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.useBottomRightCornerResizer = true;
        options.componentToCentreAround = this;
        options.content.setOwned (content);
        if (auto* window = options.launchAsync())
            window->setResizeLimits (760, 560, 1600, 1100);
    }

    void StudioMainComponent::selectAllElements()
    {
        juce::StringArray ids;
        for (const auto& element : project.getLayout().getAll())
            ids.addIfNotAlreadyThere (element.id);
        setSelectedElementIds (ids);
    }

    // -------------------------------------------------------------------------
    // Painting & layout
    // -------------------------------------------------------------------------
    void StudioMainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        // Status bar text along the bottom.
        const int sbH = 24;
        auto sb = getLocalBounds().removeFromBottom (sbH);
        g.setColour (juce::Colour (0xff0d0e11));
        g.fillRect (sb);
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (sb.removeFromTop (1));

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (11.0f));
        const auto cpu    = juce::String::formatted ("CPU: %.1f%%", statusCpu.load());
        const auto voices = juce::String::formatted ("Voices: %d", statusVoices.load());
        const auto sr     = juce::String ("Sample Rate: 48.0 kHz");

        auto sbContent = sb.reduced (12, 0);
        g.drawText (cpu + "      " + voices + "      " + sr, sbContent,
                    juce::Justification::centredLeft);

        const auto right = "Project: " + project.getManifest().instrumentName
            + "      Last saved: " + (project.hasUnsavedChanges() ? "Just now" : "Saved");
        g.drawText (right, sbContent, juce::Justification::centredRight);

        if (bottomTab == BottomPanel::Page::Design)
        {
            g.setColour (PatchCraftLookAndFeel::border());
            g.fillRect (leftResizeHandle);
            g.fillRect (rightResizeHandle);
        }
    }

    void StudioMainComponent::resized()
    {
        auto r = getLocalBounds();

        menuBar.setBounds (r.removeFromTop (24));

        // Top toolbar
        topToolbar->setBounds (r.removeFromTop (72));

        // Status bar at bottom
        r.removeFromBottom (24);

        // Canvas toolbar (always visible - hosts the section tabs).
        const int canvasToolbarH = 32;
        const auto toolbarStrip = r.removeFromTop (canvasToolbarH);
        canvasToolbar->setBounds (toolbarStrip);

        const bool designTab = (bottomTab == BottomPanel::Page::Design);
        const bool elementsFloating = isPanelFloating (elementPalette.get());
        const bool libraryFloating = isPanelFloating (assetLibraryPanel.get());
        const bool packsFloating = isPanelFloating (expansionLibraryPanel.get());
        const bool inspectorFloating = isPanelFloating (inspectorViewport.get());
        const bool layersFloating = isPanelFloating (layersPanel.get());
        const bool presetsFloating = isPanelFloating (presetsPanel.get());
        const bool brandLabTab = (bottomTab == BottomPanel::Page::Branding);
        const bool sampleMapperTab = (bottomTab == BottomPanel::Page::Samples);
        const bool brandLibraryDocked = brandLabTab && ! libraryFloating;
        // Sample library docks inside SampleMapEditor, not the main chrome.
        const bool libraryHostedInMapper = sampleLibraryDrawerOpen
            && assetLibraryPanel != nullptr
            && assetLibraryPanel->getParentComponent() != this;
        const bool sampleLibraryDocked = sampleMapperTab && sampleLibraryDrawerOpen
            && ! libraryFloating && ! libraryHostedInMapper;
        const bool leftLayersDocked = designTab && ! leftPanelCollapsed && showLayersInsteadOfElements;
        const bool leftScriptEditorDocked = designTab && showScriptEditorInsteadOfElements;

        // Visibility: only Design shows sidebar / canvas / inspector.
        leftCollapseButton.setVisible (designTab);
        rightCollapseButton.setVisible (designTab);
        leftPopButton.setVisible (designTab && ! leftPanelCollapsed);
        rightPopButton.setVisible (designTab && ! rightPanelCollapsed);
        
        leftTabs.setVisible       (designTab && ! leftPanelCollapsed);
        leftCollapseButton.setVisible (designTab);
        leftPopButton.setVisible  (designTab && ! leftPanelCollapsed);

        // pScript docks code below the canvas; Elements palette stays available on the left.
        elementPalette->setVisible (elementsFloating || (designTab && ! leftPanelCollapsed && ! showLibraryInsteadOfElements && ! showLayersInsteadOfElements));
        assetLibraryPanel->setVisible (libraryFloating || brandLibraryDocked || sampleLibraryDocked || (designTab && ! leftPanelCollapsed && showLibraryInsteadOfElements));
        expansionLibraryPanel->setVisible (packsFloating);
        if (scriptEditor != nullptr)
        {
            const bool scriptEditorFloating = isPanelFloating (scriptEditor.get());
            scriptEditor->setVisible (scriptEditorFloating || leftScriptEditorDocked);
        }

        // Right column (Inspector / Layers / Presets).
        const bool rightVisible = designTab && ! rightPanelCollapsed;
        rightTabs.setVisible (rightVisible);
        layersPanel  ->setVisible (layersFloating || leftLayersDocked);
        presetsPanel ->setVisible (presetsFloating || (rightVisible && rightTabIndex == 1));
        canvasEditor ->setVisible (designTab);
        inspectorViewport->setVisible (inspectorFloating || (rightVisible && rightTabIndex == 0));

        if (designTab)
        {
            const bool scriptDockOpen = showScriptEditorInsteadOfElements
                                     && scriptEditor != nullptr
                                     && ! isPanelFloating (scriptEditor.get());
            juce::Rectangle<int> scriptDockArea;
            if (scriptDockOpen)
            {
                const int normalBottomH = juce::jmax (210, r.getHeight() / 4);
                const int dockH = juce::jmax (normalBottomH,
                                              juce::roundToInt ((float) r.getHeight() * 0.42f));
                scriptDockArea = r.removeFromBottom (dockH);
                bottomPanel->setVisible (false);
            }
            else
            {
                const int bottomH = juce::jmax (210, r.getHeight() / 4);
                bottomPanel->setVisible (true);
                bottomPanel->setBounds (r.removeFromBottom (bottomH));
            }

            leftPanelWidth = juce::jlimit (180, juce::jmax (180, getWidth() / 2), leftPanelWidth);
            inspectorPanelWidth = juce::jlimit (220, juce::jmax (220, getWidth() / 2), inspectorPanelWidth);

            if (leftPanelCollapsed)
            {
                auto collapsed = r.removeFromLeft (30);
                leftCollapseButton.setBounds (collapsed.removeFromTop (30).reduced (3));
                leftResizeHandle = {};
                r.removeFromLeft (4);
            }
            else
            {
                auto leftCol = r.removeFromLeft (leftPanelWidth);
                auto leftHeader = leftCol.removeFromTop (32);
                leftCollapseButton.setBounds (leftHeader.removeFromRight (28).reduced (3));
                leftPopButton.setBounds (leftHeader.removeFromRight (40).reduced (3));
                leftTabs.setBounds (leftHeader);
                if (! elementsFloating)
                    elementPalette->setBounds (leftCol);
                if (! libraryFloating)
                    assetLibraryPanel->setBounds (leftCol);
                if (! packsFloating)
                    expansionLibraryPanel->setBounds (leftCol);
                if (! layersFloating)
                    layersPanel->setBounds (leftCol);
                leftResizeHandle = r.removeFromLeft (5);
            }

            if (rightPanelCollapsed)
            {
                auto collapsed = r.removeFromRight (30);
                rightCollapseButton.setBounds (collapsed.removeFromTop (30).reduced (3));
                rightResizeHandle = {};
                r.removeFromRight (4);
            }
            else
            {
                auto rightCol = r.removeFromRight (inspectorPanelWidth);
                auto rightHeader = rightCol.removeFromTop (32);
                rightCollapseButton.setBounds (rightHeader.removeFromLeft (28).reduced (3));
                rightPopButton.setBounds (rightHeader.removeFromLeft (40).reduced (3));
                rightTabs.setBounds (rightHeader);
                // Inspector, Layers and Presets share the body - only the
                // active one is visible (handled by the rightTabs onClick
                // and the visibility logic above).
                if (! inspectorFloating)
                {
                    inspectorViewport->setBounds (rightCol);
                    inspectorPanel->setSize (juce::jmax (1, rightCol.getWidth() - 10),
                                             juce::jmax (rightCol.getHeight(), 900));
                }
                if (! layersFloating && ! leftLayersDocked)
                    layersPanel->setBounds (rightCol);
                if (! presetsFloating)
                    presetsPanel->setBounds (rightCol);
                rightResizeHandle = r.removeFromRight (5);
            }

            // pScript docks under the Design canvas — keep editing the layout,
            // not a live Player takeover of the workspace.
            if (scriptEditor != nullptr && leftScriptEditorDocked && ! isPanelFloating (scriptEditor.get()))
            {
                scriptEditor->setBounds (scriptDockArea);
                scriptEditor->toFront (false);
            }

            canvasEditor->setVisible (designTab);
            canvasEditor->setBounds (r);
            canvasEditor->refreshZoomForBounds();
            if (packRuntime != nullptr && ! customerPreviewActive && ! graphAudioListen)
                packRuntime->setVisible (false);
        }
        else
        {
            leftResizeHandle = {};
            rightResizeHandle = {};
            if (brandLibraryDocked || sampleLibraryDocked)
            {
                auto libraryCol = r.removeFromRight (juce::jlimit (280, 380, getWidth() / 5));
                r.removeFromRight (8);
                assetLibraryPanel->setBounds (libraryCol);
            }
            // Non-Design tabs: bottom panel grows to fill the entire workspace.
            bottomPanel->setVisible (true);
            bottomPanel->setBounds (r);

            if (packRuntime != nullptr && ! customerPreviewActive && ! graphAudioListen
                && bottomTab != BottomPanel::Page::Branding)
            {
                packRuntime->setVisible (false);
                rehomePackRuntimeToStudio();
            }
        }

        if (tutorialOverlay != nullptr)
        {
            tutorialOverlay->setBounds (getLocalBounds());
            if (tutorialOverlay->isActive())
                tutorialOverlay->toFront (false);
        }

        if (customerPreviewOverlay != nullptr)
        {
            customerPreviewOverlay->setBounds (getLocalBounds());
            customerPreviewOverlay->setVisible (customerPreviewActive);
            if (customerPreviewActive)
            {
                customerPreviewOverlay->toFront (true);
                customerPreviewOverlay->grabKeyboardFocus();
            }
        }

        canvasToolbar->syncSectionTabFromOwner();
    }

    void StudioMainComponent::setGraphAudioListen (bool active)
    {
        if (graphAudioListen == active)
            return;

        graphAudioListen = active;

        if (active)
        {
            if (customerPreviewActive)
                toggleCustomerPreview();
            if (bottomPanel != nullptr)
                bottomPanel->setPreviewActive (false);
            if (packRuntime != nullptr)
                packRuntime->activate();
        }
        else if (! customerPreviewActive && packRuntime != nullptr)
        {
            if (bottomTab != BottomPanel::Page::Branding)
            {
                packRuntime->deactivate();
                packRuntime->setVisible (false);
                rehomePackRuntimeToStudio();
            }
        }

        if (bottomPanel != nullptr && bottomTab == BottomPanel::Page::DSP)
            bottomPanel->refresh();

        resized();
        refreshPreviewUiState();
        repaint();
    }

    juce::StringArray StudioMainComponent::getMenuBarNames()
    {
        return { "File", "Design", "Tools", "Window", "Store", "Help" };
    }

    juce::PopupMenu StudioMainComponent::getMenuForIndex (int, const juce::String& menuName)
    {
        juce::PopupMenu menu;
        if (menuName == "File")
        {
            menu.addItem (1001, "New Project");
            menu.addItem (1013, "New Sample Chopper Project...");
            menu.addItem (1002, "Open Project...");
            menu.addItem (1024, "Project Browser...");

            juce::PopupMenu recentMenu;
            const auto recentPaths = getRecentProjectPaths();
            if (recentPaths.isEmpty())
            {
                recentMenu.addItem (1199, "No recent projects", false);
            }
            else
            {
                for (int i = 0; i < juce::jmin (recentPaths.size(), 12); ++i)
                {
                    const auto file = juce::File (recentPaths[i]);
                    recentMenu.addItem (1100 + i,
                                        file.getFileName().isNotEmpty() ? file.getFileName() : recentPaths[i],
                                        file.isDirectory());
                }
            }
            menu.addSubMenu ("Load Recent", recentMenu, ! recentPaths.isEmpty());
            menu.addSeparator();
            menu.addItem (1003, "Save Project");
            menu.addItem (1004, "Save Project As...");
            menu.addSeparator();
            menu.addItem (1005, "Import Samples...");
            menu.addItem (1006, "Import Background...");
#if PATCHCRAFT_ENABLE_AI_STUDIO
            menu.addItem (1022, "Generate AI Background...");
#endif
            menu.addSeparator();
            menu.addItem (1023, "Launch Center...");
            menu.addSeparator();
            menu.addItem (1007, "Export Pack...");
            menu.addItem (1008, "Send to Expansion Pack...");
            menu.addItem (1020, "Export VST3 Plugin...");
            menu.addItem (1021, "Publish Draft to Plugin.club...");
            menu.addSeparator();
            menu.addItem (1009, "Settings...");
            menu.addItem (1010, "Quit");
            menu.addSeparator();
            menu.addItem (1012, "Set Current Sound as Default Preset");
            menu.addItem (1011, "Restore Current Sound to Defaults");
        }
        else if (menuName == "Design")
        {
            const auto selectedCount = selectedElementIds.size();
            const bool hasSelection = selectedCount > 0;
            const bool canAlign = selectedCount >= 2;
            const bool canDistribute = selectedCount >= 3;
            bool canDetachLabels = false;
            bool canCreatePscriptHandler = false;
            bool canAttachPscriptFile = false;
            bool canDetachPscript = false;
            for (const auto& id : selectedElementIds)
                if (auto* el = project.getLayout().find (id))
                {
                    if (isRuntimeControlElement (el->type)
                        && el->labelPosition != "hidden"
                        && (el->label.isNotEmpty() || el->parameterId.isNotEmpty()))
                    {
                        canDetachLabels = true;
                    }
                    if (isScriptableControlElement (*el)
                        && el->parameterId.isNotEmpty()
                        && project.getParameters().find (el->parameterId) != nullptr)
                    {
                        canCreatePscriptHandler = true;
                        canAttachPscriptFile = true;
                    }
                    if (el->pscriptFile.isNotEmpty())
                        canDetachPscript = true;
                }

            menu.addItem (2998, "Undo", project.canUndo());
            menu.addItem (2999, "Redo", project.canRedo());
            menu.addSeparator();

            juce::PopupMenu scriptMenu;
            scriptMenu.addItem (3101, "Open pScript Editor", true);
            scriptMenu.addItem (3100, "Create pScript Handler For Selected Control", canCreatePscriptHandler);
            scriptMenu.addItem (3102, "Attach pScript File...", canAttachPscriptFile);
            scriptMenu.addItem (3103, "Detach pScript From Selection", canDetachPscript);
            menu.addSubMenu ("pScript", scriptMenu, canCreatePscriptHandler || canAttachPscriptFile || canDetachPscript || hasSelection);
            menu.addSeparator();

            menu.addItem (3003, "Copy Selection", hasSelection);
            menu.addItem (3004, "Copy Selection Without Parameters", hasSelection);
            menu.addItem (3005, "Paste Elements", hasCopiedElements());
            menu.addItem (3006, "Copy Selection To All Tabs", hasSelection);
            menu.addSeparator();
            menu.addItem (3007, "Select All Layers", ! project.getLayout().getAll().empty());
            menu.addSeparator();
            menu.addItem (3001, "Duplicate Selection", hasSelection);
            menu.addItem (3002, "Delete Selection", hasSelection);
            menu.addSeparator();

            juce::PopupMenu groupMenu;
            groupMenu.addItem (3010, "Group Selection", hasSelection);
            groupMenu.addItem (3011, "Ungroup Selection", hasSelection);
            groupMenu.addItem (3012, "Remove From Container", hasSelection);
            menu.addSubMenu ("Containers / Groups", groupMenu);

            juce::PopupMenu layerMenu;
            layerMenu.addItem (3050, "Show Selection", hasSelection);
            layerMenu.addItem (3051, "Hide Selection", hasSelection);
            layerMenu.addSeparator();
            layerMenu.addItem (3052, "Lock Selection", hasSelection);
            layerMenu.addItem (3053, "Unlock Selection", hasSelection);
            menu.addSubMenu ("Layer State", layerMenu);

            juce::PopupMenu alignMenu;
            alignMenu.addItem (3020, "Align Left", canAlign);
            alignMenu.addItem (3021, "Align Horizontal Center", canAlign);
            alignMenu.addItem (3022, "Align Right", canAlign);
            alignMenu.addSeparator();
            alignMenu.addItem (3023, "Align Top", canAlign);
            alignMenu.addItem (3024, "Align Vertical Center", canAlign);
            alignMenu.addItem (3025, "Align Bottom", canAlign);
            alignMenu.addSeparator();
            alignMenu.addItem (3026, "Distribute Horizontally", canDistribute);
            alignMenu.addItem (3027, "Distribute Vertically", canDistribute);
            menu.addSubMenu ("Align / Distribute", alignMenu);

            juce::PopupMenu canvasAlignMenu;
            canvasAlignMenu.addItem (3060, "Left Edge", hasSelection);
            canvasAlignMenu.addItem (3061, "Horizontal Center", hasSelection);
            canvasAlignMenu.addItem (3062, "Right Edge", hasSelection);
            canvasAlignMenu.addSeparator();
            canvasAlignMenu.addItem (3063, "Top Edge", hasSelection);
            canvasAlignMenu.addItem (3064, "Vertical Center", hasSelection);
            canvasAlignMenu.addItem (3065, "Bottom Edge", hasSelection);
            canvasAlignMenu.addSeparator();
            canvasAlignMenu.addItem (3066, "Center Both Axes", hasSelection);
            menu.addSubMenu ("Align To Canvas", canvasAlignMenu);

            juce::PopupMenu precisionMenu;
            precisionMenu.addItem (3070, "Match Width", canAlign);
            precisionMenu.addItem (3071, "Match Height", canAlign);
            precisionMenu.addItem (3072, "Match Width + Height", canAlign);
            precisionMenu.addSeparator();
            precisionMenu.addItem (3073, "Snap Selection To Grid", hasSelection);
            menu.addSubMenu ("Transform Precision", precisionMenu);

            juce::PopupMenu labelMenu;
            labelMenu.addItem (3090, "Detach Labels From Selection", canDetachLabels);
            labelMenu.addItem (3091, "Hide Labels For Selection", hasSelection);
            labelMenu.addItem (3092, "Show Labels For Selection", hasSelection);
            menu.addSubMenu ("Labels", labelMenu);

            juce::PopupMenu styleMenu;
            styleMenu.addItem (3080, "Copy Style From Primary Selection", hasSelection);
            styleMenu.addItem (3081, "Paste Style To Selection", hasSelection && hasCopiedDesignStyle);
            styleMenu.addSeparator();
            styleMenu.addItem (3082, "Apply: Glass Panel", hasSelection);
            styleMenu.addItem (3083, "Apply: Gold Hardware", hasSelection);
            styleMenu.addItem (3084, "Apply: Minimal Flat", hasSelection);
            styleMenu.addItem (3085, "Apply: Neon Reactive", hasSelection);
            menu.addSubMenu ("Reusable Styles", styleMenu);

            juce::PopupMenu orderMenu;
            orderMenu.addItem (3030, "Bring to Front", hasSelection);
            orderMenu.addItem (3031, "Bring Forward", hasSelection);
            orderMenu.addItem (3032, "Send Backward", hasSelection);
            orderMenu.addItem (3033, "Send to Back", hasSelection);
            menu.addSubMenu ("Arrange Order", orderMenu);

            menu.addSeparator();
            menu.addItem (3040, "Show Grid", true, canvasEditor != nullptr && canvasEditor->isGridVisible());
            menu.addItem (3041, "Show Rulers", true, canvasEditor != nullptr && canvasEditor->areRulersVisible());
        }
        else if (menuName == "Tools")
        {
            const auto* selected = project.getLayout().find (selectedElementId);
            const bool bindable = selected != nullptr
                && (selected->type == ElementType::Knob || selected->type == ElementType::Slider
                    || selected->type == ElementType::Button || selected->type == ElementType::Toggle
                    || selected->type == ElementType::Dropdown || selected->type == ElementType::ValueDisplay
                    || selected->type == ElementType::MacroControl || selected->type == ElementType::SampleDropZone);
            menu.addItem (6000, "Open Control Node Editor...", bindable);
            menu.addItem (6001, "Add Sound Control + Open Node Editor");
            menu.addSeparator();
            menu.addItem (4013, "Full Screen Sample Mapper Zones");
            menu.addItem (4010, "Animation Lab");
        }
        else if (menuName == "Window")
        {
            juce::PopupMenu zoomMenu;
            zoomMenu.addItem (4100, "Fit Canvas To Window", canvasEditor != nullptr, canvasEditor != nullptr && canvasEditor->isAutoFitEnabled());
            zoomMenu.addSeparator();
            const int zooms[] = { 10, 25, 33, 50, 67, 75, 90, 100, 110, 125, 150, 175, 200, 300, 400 };
            for (int z : zooms)
            {
                const bool selected = canvasEditor != nullptr
                    && ! canvasEditor->isAutoFitEnabled()
                    && juce::roundToInt (canvasEditor->getZoom() * 100.0f) == z;
                zoomMenu.addItem (4100 + z, juce::String (z) + " %", canvasEditor != nullptr, selected);
            }
            menu.addSubMenu ("Canvas Zoom", zoomMenu);
            menu.addSeparator();
            menu.addItem (4012, "Reset Canvas To Blank", true);
            menu.addSeparator();
            menu.addItem (4020,
                          bottomTab == BottomPanel::Page::DSP ? "Listen" : "Preview",
                          true,
                          isPreviewActive());
            menu.addSeparator();
            menu.addItem (4015, "Advanced Build Mode", true, advancedBuildUnlocked);
            menu.addSeparator();
            juce::PopupMenu advancedMenu;
            advancedMenu.addItem (4021, "One-Shot Maker");
            advancedMenu.addItem (4022, "Widget Builder");
            advancedMenu.addItem (4023, "Animation Lab");
            advancedMenu.addItem (4029, "Graph / Sound Stack");
            advancedMenu.addItem (4013, "Full Sample Mapper");
            advancedMenu.addItem (4030, "Keyboard Test Page");
            advancedMenu.addItem (4024, "Project Dashboard");
            advancedMenu.addItem (4025, "Project Browser");
            menu.addSubMenu ("Advanced Tools", advancedMenu);
            menu.addSeparator();
            menu.addItem (4001, "Hide All Windows");
            menu.addItem (4002, "Show Main Windows");
            menu.addItem (4003, "Dock All Floating Windows", ! floatingPanels.empty());
            menu.addSeparator();
            menu.addItem (4004, "Toggle Left Panel", bottomTab == BottomPanel::Page::Design);
            menu.addItem (4005, "Toggle Right Panel", bottomTab == BottomPanel::Page::Design);
            menu.addSeparator();
            menu.addItem (4028, "Go To Sounds");
            menu.addItem (4007, "Go To Design");
            menu.addItem (4027, "Go To Brand");
            menu.addItem (4011, "Go To Ship");
        }
        else if (menuName == "Store")
        {
            menu.addItem (5000, "PatchCraft Expansions...");
            menu.addSeparator();
            menu.addItem (5002, "Browse Marketplace Packs...", false);
            menu.addItem (5003, "Import Purchased Pack...", false);
        }
        else if (menuName == "Help")
        {
            menu.addItem (2001, "DSP Builder Tutorial...");
            menu.addItem (2003, "Auto-Show Tutorials", true, getStudioTutorialsEnabled());
            menu.addItem (2005, "Tutorial Mode", true, getTutorialModeEnabled());
            menu.addSeparator();
            menu.addItem (2002, "Show Help Tooltips", true, project.getManifest().playerShowParameterGuidance);
        }
        return menu;
    }

    void StudioMainComponent::menuItemSelected (int menuItemID, int)
    {
        switch (menuItemID)
        {
            case 1001: newProject(); break;
            case 1013: newSampleChopperProject(); break;
            case 1002: openProject(); break;
            case 1003: saveProject(); break;
            case 1004: saveProjectAs(); break;
            case 1005: importSamples(); break;
            case 1006: importBackground(); break;
#if PATCHCRAFT_ENABLE_AI_STUDIO
            case 1022: generateAiBackground(); break;
#endif
            case 1023: setBottomTab (BottomPanel::Page::Export); break;
            case 1024: setBottomTab (BottomPanel::Page::ProjectBrowser); break;
            case 1007: exportPack(); break;
            case 1008: sendToExpansionPack(); break;
            case 1020: exportVstPlugin(); break;
            case 1021: publishToPluginClub(); break;
            case 1009: openSettings(); break;
            case 1010: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 5000: setBottomTab (BottomPanel::Page::Expansions); break;
            case 5001: juce::URL ("https://plugin.club").launchInDefaultBrowser(); break;
            case 6000: openControlNodeEditor(); break;
            case 6001:
            {
                addElementToCanvas (ElementType::Knob);
                juce::Component::SafePointer<StudioMainComponent> safe (this);
                juce::MessageManager::callAsync ([safe]
                {
                    if (auto* component = safe.getComponent())
                        component->openControlNodeEditor();
                });
                break;
            }
            case 2998: undo(); break;
            case 2999: redo(); break;
            case 3003: copySelectedElements (true); break;
            case 3004: copySelectedElements (false); break;
            case 3005: pasteCopiedElements(); break;
            case 3006: copySelectedToAllTabs(); break;
            case 3007: selectAllElements(); break;
            case 3001: duplicateSelected(); break;
            case 3002: deleteSelected(); break;
            case 3010: groupSelectedElements(); break;
            case 3011: ungroupSelectedElements(); break;
            case 3012: removeSelectedFromContainer(); break;
            case 3020: alignSelected ("left"); break;
            case 3021: alignSelected ("hcenter"); break;
            case 3022: alignSelected ("right"); break;
            case 3023: alignSelected ("top"); break;
            case 3024: alignSelected ("vcenter"); break;
            case 3025: alignSelected ("bottom"); break;
            case 3026: distributeSelected (true); break;
            case 3027: distributeSelected (false); break;
            case 3030: orderSelected ("front"); break;
            case 3031: orderSelected ("forward"); break;
            case 3032: orderSelected ("backward"); break;
            case 3033: orderSelected ("back"); break;
            case 3050: setSelectedVisibility (true); break;
            case 3051: setSelectedVisibility (false); break;
            case 3052: setSelectedLocked (true); break;
            case 3053: setSelectedLocked (false); break;
            case 3060: alignSelectedToCanvas ("left"); break;
            case 3061: alignSelectedToCanvas ("hcenter"); break;
            case 3062: alignSelectedToCanvas ("right"); break;
            case 3063: alignSelectedToCanvas ("top"); break;
            case 3064: alignSelectedToCanvas ("vcenter"); break;
            case 3065: alignSelectedToCanvas ("bottom"); break;
            case 3066: alignSelectedToCanvas ("center"); break;
            case 3070: matchSelectedSize ("width"); break;
            case 3071: matchSelectedSize ("height"); break;
            case 3072: matchSelectedSize ("both"); break;
            case 3073: snapSelectedToGrid(); break;
            case 3090: detachLabelsFromSelectedControls(); break;
            case 3091: setSelectedLabelVisibility (false); break;
            case 3092: setSelectedLabelVisibility (true); break;
            case 3100: createPscriptHandlerForSelectedControl(); break;
            case 3101: focusPscriptPanel(); break;
            case 3102: attachPscriptFileToSelectedControl(); break;
            case 3103: detachPscriptFromSelectedControl(); break;
            case 3080: copySelectedDesignStyle(); break;
            case 3081: pasteDesignStyle(); break;
            case 3082: applyDesignStylePreset ("glass"); break;
            case 3083: applyDesignStylePreset ("gold"); break;
            case 3084: applyDesignStylePreset ("minimal"); break;
            case 3085: applyDesignStylePreset ("neon"); break;
            case 3040: toggleCanvasGrid(); break;
            case 3041: toggleCanvasRulers(); break;
            case 4001: hideAllWindows(); break;
            case 4002: showMainWindows(); break;
            case 4003: dockAllFloatingPanels(); break;
            case 4004:
                leftPanelCollapsed = ! leftPanelCollapsed;
                leftCollapseButton.setButtonText (leftPanelCollapsed ? ">" : "<");
                resized();
                repaint();
                break;
            case 4005:
                rightPanelCollapsed = ! rightPanelCollapsed;
                rightCollapseButton.setButtonText (rightPanelCollapsed ? "<" : ">");
                resized();
                repaint();
                break;
            case 4020: togglePreview(); break;
            case 4015:
                setAdvancedBuildUnlocked (! advancedBuildUnlocked);
                break;
            case 4021:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::OneShotMaker);
                break;
            case 4022:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Widgets);
                break;
            case 4023:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Animation);
                break;
            case 4024:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Dashboard);
                break;
            case 4025:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::ProjectBrowser);
                break;
            case 4026:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Samples);
                break;
            case 4027: setBottomTab (BottomPanel::Page::Branding); break;
            case 4028: setBottomTab (BottomPanel::Page::Build); break;
            case 4029:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::DSP);
                break;
            case 4030:
                setAdvancedBuildUnlocked (true);
                setBottomTab (BottomPanel::Page::Test);
                break;
            case 4006: setBottomTab (BottomPanel::Page::Dashboard); break;
            case 4007: setBottomTab (BottomPanel::Page::Design); break;
            case 4008: setBottomTab (BottomPanel::Page::MidiPlayground); break;
            case 4009: setBottomTab (BottomPanel::Page::DSP); break;
            case 4010: setBottomTab (BottomPanel::Page::Animation); break;
            case 4011: setBottomTab (BottomPanel::Page::Export); break;
            case 4014: setBottomTab (BottomPanel::Page::Expansions); break;
            case 4013:
                setAdvancedBuildUnlocked (true);
                openSampleMapperZoneManager();
                break;
            case 4012:
                project.resetCanvasToBlank();
                clearSelection();
                setBottomTab (BottomPanel::Page::Design);
                break;
            case 4100: fitCanvasToWindow(); break;
            case 2001: showDspBuilderTutorial(); break;
            case 2002: toggleHelpTooltips(); break;
            case 2003:
                setStudioTutorialsEnabled (! getStudioTutorialsEnabled());
                break;
            case 2005:
                setTutorialModeEnabled (! getTutorialModeEnabled());
                break;
            case 1011: restoreAllPresets(); break;
            case 1012: setDefaultPreset(); break;
            default:
                if (menuItemID >= 1100 && menuItemID < 1116)
                {
                    const auto recent = getRecentProjectPaths();
                    const int index = menuItemID - 1100;
                    if (index >= 0 && index < recent.size())
                        openProjectFolder (juce::File (recent[index]));
                    break;
                }
                if (menuItemID > 4100 && menuItemID <= 4500)
                    setCanvasZoom ((float) (menuItemID - 4100) / 100.0f);
                break;
        }
    }

    void StudioMainComponent::toggleHelpTooltips()
    {
        auto& manifest = project.getManifest();
        manifest.playerShowParameterGuidance = ! manifest.playerShowParameterGuidance;
        project.notifyChanged();
        menuBar.repaint();
    }

    void StudioMainComponent::showDspBuilderTutorial()
    {
        if (bottomTab != BottomPanel::Page::Design)
            setBottomTab (BottomPanel::Page::Design);

        if (bottomPanel != nullptr)
            bottomPanel->showDspBuilderTutorial();
    }

    bool StudioMainComponent::getStudioTutorialsEnabled() const
    {
        return readStudioTutorialPreference (project.getManifest().studioShowTutorials);
    }

    void StudioMainComponent::setStudioTutorialsEnabled (bool enabled)
    {
        project.getManifest().studioShowTutorials = enabled;
        writeStudioTutorialPreference (enabled);
        project.notifyChanged();
        menuBar.repaint();
    }

    bool StudioMainComponent::getTutorialModeEnabled() const
    {
        return tutorialOverlay != nullptr && tutorialOverlay->isActive();
    }

    void StudioMainComponent::setTutorialModeEnabled (bool enabled)
    {
        writeStudioTutorialModePreference (enabled);
        if (tutorialOverlay == nullptr)
            return;

        if (enabled)
            addAndMakeVisible (*tutorialOverlay);
        else
        {
            tutorialOverlay->setActive (false);
            removeChildComponent (tutorialOverlay.get());
            addChildComponent (*tutorialOverlay);
            menuBar.repaint();
            return;
        }

        tutorialOverlay->setActive (enabled);
        tutorialOverlay->setBounds (getLocalBounds());
        tutorialOverlay->toFront (false);
        menuBar.repaint();
    }

    void StudioMainComponent::mouseDown (const juce::MouseEvent& e)
    {
        panelDragMode = PanelDragMode::None;
        if (bottomTab != BottomPanel::Page::Design)
            return;

        if (! leftPanelCollapsed && leftResizeHandle.expanded (3, 0).contains (e.getPosition()))
            panelDragMode = PanelDragMode::LeftPanel;
        else if (! rightPanelCollapsed && rightResizeHandle.expanded (3, 0).contains (e.getPosition()))
            panelDragMode = PanelDragMode::Inspector;
    }

    void StudioMainComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (panelDragMode == PanelDragMode::LeftPanel)
        {
            leftPanelWidth = juce::jlimit (180, getWidth() / 2, e.x);
            resized();
        }
        else if (panelDragMode == PanelDragMode::Inspector)
        {
            inspectorPanelWidth = juce::jlimit (220, getWidth() / 2, getWidth() - e.x);
            resized();
        }
    }

    void StudioMainComponent::mouseUp (const juce::MouseEvent&)
    {
        panelDragMode = PanelDragMode::None;
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void StudioMainComponent::mouseMove (const juce::MouseEvent& e)
    {
        if (bottomTab == BottomPanel::Page::Design
            && ((! leftPanelCollapsed && leftResizeHandle.expanded (3, 0).contains (e.getPosition()))
                || (! rightPanelCollapsed && rightResizeHandle.expanded (3, 0).contains (e.getPosition()))))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    // -------------------------------------------------------------------------
    // Top-level actions
    // -------------------------------------------------------------------------
    void StudioMainComponent::newProject()
    {
        juce::Component::SafePointer<StudioMainComponent> self (this);
        ProjectWizardDialog::show (this, [self] (ProductKind kind)
        {
            if (auto* component = self.getComponent())
                component->createProductProject (kind);
        });
    }

    void StudioMainComponent::createProductProject (ProductKind kind)
    {
        createProductFromTemplate (defaultTemplateForKind (kind));
    }

    void StudioMainComponent::createProductFromTemplate (const juce::String& templateId)
    {
        const auto spec = templateSpecFor (templateId);
        applyProductTemplate (project, templateId);

        selectedElementId.clear();
        selectedElementIds.clear();

        if (canvasEditor != nullptr && spec.layoutModuleId.isNotEmpty())
        {
            project.resetCanvasToBlank();
            const auto& canvas = project.getCanvasSize();
            canvasEditor->addModuleLayout (spec.layoutModuleId,
                                           { canvas.width / 2 - 430, canvas.height / 2 - 235 });
        }

        applyTemplateLiveDefaults (project, templateId);
        project.notifyChanged();
        refreshAllPanels();
        syncExportPreview();

        const auto sub = spec.showChopStep
            ? BottomPanel::BuildSubPage::Chop
            : BottomPanel::BuildSubPage::ImportSounds;
        if (bottomPanel != nullptr)
            bottomPanel->setBuildSubPage (sub);

        bottomTab = BottomPanel::Page::Build;
        if (canvasToolbar != nullptr)
            canvasToolbar->syncSectionTabFromOwner();
        resized();
        refreshPreviewUiState();
    }

    void StudioMainComponent::showMultiLayerSetupWizard()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Save Project First",
                                                    "Save this project before creating a multi-layer rack so PatchCraft can write layer map files beside the project.");
            return;
        }

        if (project.getSampleMap().getZones().empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Import Samples First",
                                                    "Multi-layer export needs at least one mapped sample zone. Import or map samples, then run Layer Rack again.");
            return;
        }

        auto* alert = new juce::AlertWindow ("Multi-Layer Instrument",
                                             "Create a two-layer Player rack from the current sample map. You can edit the generated layer JSON files later for separate maps.",
                                             juce::MessageBoxIconType::QuestionIcon);
        alert->addTextEditor ("layer1", "Layer 1", "Layer 1 name:");
        alert->addTextEditor ("layer2", "Layer 2", "Layer 2 name:");
        alert->addTextEditor ("vol1", "1.0", "Layer 1 volume:");
        alert->addTextEditor ("vol2", "0.8", "Layer 2 volume:");
        alert->addTextEditor ("pan1", "-0.10", "Layer 1 pan:");
        alert->addTextEditor ("pan2", "0.10", "Layer 2 pan:");
        alert->addButton ("Create Rack", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<StudioMainComponent> safeThis (this);
        alert->enterModalState (true,
            juce::ModalCallbackFunction::create ([safeThis, alert] (int result)
            {
                const auto layer1Name = alert->getTextEditorContents ("layer1").trim();
                const auto layer2Name = alert->getTextEditorContents ("layer2").trim();
                const auto layer1Vol = alert->getTextEditorContents ("vol1").getFloatValue();
                const auto layer2Vol = alert->getTextEditorContents ("vol2").getFloatValue();
                const auto layer1Pan = alert->getTextEditorContents ("pan1").getFloatValue();
                const auto layer2Pan = alert->getTextEditorContents ("pan2").getFloatValue();
                std::unique_ptr<juce::AlertWindow> owned (alert);

                if (result != 1)
                    return;

                if (auto* self = safeThis.getComponent())
                {
                    auto& project = self->project;
                    const auto layerFolder = project.getProjectFolder().getChildFile ("layers");
                    if (! layerFolder.createDirectory())
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                "Layer Rack Not Created",
                                                                "Could not create:\n" + layerFolder.getFullPathName());
                        return;
                    }

                    const auto layerMapJson = juce::JSON::toString (project.getSampleMap().toVar(), true);
                    const auto layer1File = layerFolder.getChildFile ("layer_1.json");
                    const auto layer2File = layerFolder.getChildFile ("layer_2.json");
                    if (! layer1File.replaceWithText (layerMapJson) || ! layer2File.replaceWithText (layerMapJson))
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                "Layer Rack Not Created",
                                                                "Could not write layer map files in:\n" + layerFolder.getFullPathName());
                        return;
                    }

                    auto& manifest = project.getManifest();
                    manifest.engine = "multi";
                    manifest.multiInstrumentMode = true;
                    manifest.category = "Multi-Layer Instrument";
                    manifest.productRecipeId = "multi_layer_rack";
                    manifest.productKindLabel = "Multi-Layer Instrument";
                    manifest.instrumentIds.clear();
                    manifest.instrumentNames.clear();
                    manifest.instrumentFiles.clear();
                    manifest.instrumentVolumes.clear();
                    manifest.instrumentPans.clear();
                    manifest.instrumentMidiChannels.clear();
                    manifest.instrumentOutputRoutes.clear();
                    manifest.instrumentTransposeSemitones.clear();
                    manifest.instrumentEnabled.clear();
                    manifest.instrumentAutoPlay.clear();
                    manifest.instrumentAutoPlayNotes.clear();
                    manifest.instrumentAutoPlayVelocities.clear();

                    manifest.instrumentIds.add ("layer_1");
                    manifest.instrumentIds.add ("layer_2");
                    manifest.instrumentNames.add (layer1Name.isNotEmpty() ? layer1Name : "Layer 1");
                    manifest.instrumentNames.add (layer2Name.isNotEmpty() ? layer2Name : "Layer 2");
                    manifest.instrumentFiles.add ("layers/layer_1.json");
                    manifest.instrumentFiles.add ("layers/layer_2.json");
                    manifest.instrumentVolumes.add (juce::jlimit (0.0f, 2.0f, layer1Vol));
                    manifest.instrumentVolumes.add (juce::jlimit (0.0f, 2.0f, layer2Vol));
                    manifest.instrumentPans.add (juce::jlimit (-1.0f, 1.0f, layer1Pan));
                    manifest.instrumentPans.add (juce::jlimit (-1.0f, 1.0f, layer2Pan));
                    manifest.instrumentMidiChannels.add (0);
                    manifest.instrumentMidiChannels.add (0);
                    manifest.instrumentOutputRoutes.add (0);
                    manifest.instrumentOutputRoutes.add (0);
                    manifest.instrumentTransposeSemitones.add (0);
                    manifest.instrumentTransposeSemitones.add (0);
                    manifest.instrumentEnabled.add (1);
                    manifest.instrumentEnabled.add (1);
                    manifest.instrumentAutoPlay.add (0);
                    manifest.instrumentAutoPlay.add (0);
                    manifest.instrumentAutoPlayNotes.add (60);
                    manifest.instrumentAutoPlayNotes.add (60);
                    manifest.instrumentAutoPlayVelocities.add (1.0f);
                    manifest.instrumentAutoPlayVelocities.add (1.0f);
                    manifest.tags.addIfNotAlreadyThere ("multi-layer");

                    project.notifyChanged();
                    self->refreshAllPanels();
                    self->syncExportPreview();

                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                            "Layer Rack Created",
                                                            "This project is now set up as a two-layer multi-instrument pack.");
                }
            }), true);
    }

    void StudioMainComponent::syncExportPreview()
    {
        if (packRuntime != nullptr)
            packRuntime->requestReloadImmediate();
        refreshAllPanels();
    }

    void StudioMainComponent::loadArpStepSequencerTemplate()
    {
        project.resetToArpStepSequencerTemplate();
        selectedElementId.clear();
        selectedElementIds.clear();
        refreshAllPanels();
        setBottomTab (BottomPanel::Page::Design);
    }

    void StudioMainComponent::newSampleChopperProject()
    {
        createProductProject (ProductKind::LoopChopInstrument);
    }

    void StudioMainComponent::openProject()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Open PatchCraft project or demo",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.patchcraftproject;*.patchcraft");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;
                openProjectFolder (folder);
            });
    }

    void StudioMainComponent::openProjectFolder (const juce::File& folder)
    {
        if (! folder.isDirectory())
            return;

        juce::String err;
        const bool runtimePack = folder.getChildFile ("manifest.json").existsAsFile();
        const bool authoringProject = folder.getChildFile ("project.json").existsAsFile();

        if (runtimePack)
        {
            if (! project.loadRuntimePackAsProject (folder, err))
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Open demo/template")
                        .withMessage (err.isEmpty() ? "Failed to open PatchCraft demo/template." : err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
                return;
            }
        }
        else if (authoringProject)
        {
            if (! project.load (folder, err))
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Open project")
                        .withMessage (err.isEmpty() ? "Failed to open project." : err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
                return;
            }
        }
        else
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Open project")
                    .withMessage ("Choose a folder that contains project.json or manifest.json.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
            return;
        }

        selectedElementId.clear();
        selectedElementIds.clear();
        assets.clear();
        sanitiseLayoutParameterReferences (project);
        syncDspGraphValuesToLiveStore (project);
        addRecentProject (folder);
        project.notifyChanged();
        refreshAllPanels();
        setBottomTab (BottomPanel::Page::Design);
    }

    juce::StringArray StudioMainComponent::getRecentProjectPaths() const
    {
        return readRecentProjectPreference();
    }

    void StudioMainComponent::addRecentProject (const juce::File& folder)
    {
        if (! folder.isDirectory())
            return;

        const auto fullPath = folder.getFullPathName();
        auto paths = readRecentProjectPreference();

        for (int i = paths.size(); --i >= 0;)
            if (juce::File (paths[i]) == folder || paths[i].equalsIgnoreCase (fullPath))
                paths.remove (i);

        paths.insert (0, fullPath);
        while (paths.size() > 16)
            paths.remove (paths.size() - 1);

        writeRecentProjectPreference (paths);
        menuBar.repaint();
    }

    void StudioMainComponent::loadFactoryDemo (const juce::File& demoPackFolder)
    {
        juce::String err;
        if (! project.loadRuntimePackAsProject (demoPackFolder, err))
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("Load factory demo")
                    .withMessage (err.isEmpty() ? "Failed to load factory demo." : err)
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
            return;
        }

        selectedElementId.clear();
        selectedElementIds.clear();
        assets.clear();
        addRecentProject (demoPackFolder);
        refreshAllPanels();
        setBottomTab (BottomPanel::Page::Design);
    }

    void StudioMainComponent::saveProject()
    {
        if (project.getProjectFolder().isDirectory())
        {
            juce::String err;
            if (! project.save (project.getProjectFolder(), err))
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Save project")
                        .withMessage (err.isEmpty() ? "Failed to save project." : err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
                return;
            }
            addRecentProject (project.getProjectFolder());
            refreshAllPanels();
            return;
        }

        saveProjectAs();
    }

    void StudioMainComponent::saveProjectAs()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Save PatchCraft project as...",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (project.getManifest().instrumentName + ".patchcraftproject"));
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;
                juce::String err;
                if (! project.save (folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Save project")
                            .withMessage (err.isEmpty() ? "Failed to save project." : err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }
                addRecentProject (folder);
                refreshAllPanels();
            });
    }

    namespace
    {
        static juce::String sanitiseFileName (juce::String name)
        {
            name = name.trim();
            if (name.isEmpty())
                name = "Untitled";
            return name.replaceCharacters ("\\/:*?\"<>|", "_________");
        }

        static bool writeDspPatchJson (const PatchCraftProject& project,
                                       const juce::File& file,
                                       const juce::String& sectionId,
                                       const juce::String& patchName,
                                       juce::String& error)
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("format", "PatchCraft DSP Patch");
            root->setProperty ("formatVersion", 1);
            root->setProperty ("name", patchName);
            root->setProperty ("engine", project.getEngineType());
            root->setProperty ("section", sectionId);
            root->setProperty ("dspGraph", project.getDspGraph().toVar());

            if (! file.getParentDirectory().createDirectory())
            {
                error = "Could not create patch folder.";
                return false;
            }

            if (! file.replaceWithText (juce::JSON::toString (juce::var (root), true)))
            {
                error = "Could not write patch file.";
                return false;
            }

            return true;
        }

        static Preset makePlayablePresetFromCurrentPatch (PatchCraftProject& project,
                                                          const juce::String& name)
        {
            return project.captureCurrentPatch (name).toPreset();
        }

        static int findPresetIndexByName (PatchCraftProject& project, const juce::String& name)
        {
            auto& presets = project.getPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].name == name)
                    return i;
            return -1;
        }

        static int findPatchIndexById (PatchCraftProject& project, const juce::String& id)
        {
            auto& patches = project.getPatches();
            for (int i = 0; i < (int) patches.size(); ++i)
                if (patches[(size_t) i].id == id)
                    return i;
            return -1;
        }

        // Wire a saved patch into the active expansion (pack) so the patch
        // shows up in the pack's preset list automatically. Mirrors what
        // DspPage::addPatchPresetToExpansion does for Easy Mode preset
        // creation, but kept local here so the toolbar's Save Patch button
        // doesn't need to reach across into the DSP page.
        static void registerPatchWithExpansion (PatchCraftProject& project,
                                                  InstrumentPatch& patch,
                                                  Preset& preset)
        {
            // Pick (or create) a default expansion. Use the first existing
            // one if any; otherwise create one named after the instrument so
            // saving a patch into a fresh project still lands somewhere
            // sensible — that becomes the default pack on export.
            ExpansionMetadata* expansion = nullptr;
            if (! project.getExpansions().empty())
                expansion = &project.getExpansions().front();
            else
                expansion = &project.ensureExpansion (
                    project.getManifest().instrumentName.isNotEmpty()
                        ? project.getManifest().instrumentName + " Pack"
                        : juce::String ("Pack"));

            if (expansion == nullptr) return;

            patch.expansionId = expansion->id;
            patch.packId = expansion->id;
            preset.expansionId = expansion->id;
            preset.packId = expansion->id;

            const auto category = patch.category.isNotEmpty()
                ? patch.category : project.getManifest().category;
            if (category.isNotEmpty())
            {
                expansion->folders.addIfNotAlreadyThere (category);
                if (expansion->category.isEmpty())
                    expansion->category = category;
            }
            expansion->includedPatchIds.addIfNotAlreadyThere (patch.id);
            expansion->includedPresetNames.addIfNotAlreadyThere (preset.name);
            for (const auto& asset : patch.includedAssets)
                expansion->includedAssets.addIfNotAlreadyThere (asset);
        }

        static void showSaveError (const juce::String& title, const juce::String& message)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (message)
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
        }
    }

    void StudioMainComponent::saveCurrentDspPatch()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Patch", "Save the project first so the playable patch and layout changes can be written to disk.");
            return;
        }

        auto patchName = project.getManifest().defaultPreset;
        if (patchName.isEmpty())
            patchName = project.getManifest().instrumentName + " Patch";

        auto patch = project.captureCurrentPatch (patchName);
        patch.isDefault = true;
        auto preset = patch.toPreset();
        preset.isDefault = true;
        for (auto& existing : project.getPatches())
            existing.isDefault = false;
        for (auto& existing : project.getPresets())
            existing.isDefault = false;

        // Register the patch with the active expansion so it ships in the
        // pack on the next export, without the user needing to manually
        // attach it via the DSP page.
        registerPatchWithExpansion (project, patch, preset);

        const int existingPatchIndex = findPatchIndexById (project, patch.id);
        if (existingPatchIndex >= 0)
            project.getPatches()[(size_t) existingPatchIndex] = patch;
        else
            project.getPatches().push_back (patch);

        const int existingIndex = findPresetIndexByName (project, preset.name);
        if (existingIndex >= 0)
            project.getPresets()[(size_t) existingIndex] = preset;
        else
            project.getPresets().push_back (preset);

        project.getManifest().defaultPreset = preset.name;
        project.notifyChanged();

        juce::String error;
        if (! project.save (project.getProjectFolder(), error))
        {
            showSaveError ("Save Patch", error);
            return;
        }

        refreshAllPanels();
    }

    void StudioMainComponent::saveCurrentDspPatchAs()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Patch As", "Save the project first so the playable patch and layout changes can be written to disk.");
            return;
        }

        auto* window = new juce::AlertWindow ("Save Patch As",
            "Name this playable patch. It stores the full current instrument sound across Source, Filter, Amp, Mod, FX, and Output.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", project.getManifest().instrumentName + " Patch", "Patch Name:");
        window->addButton ("Save Patch", 1);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                const auto name = window->getTextEditorContents ("name").trim();
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result != 1 || name.isEmpty())
                    return;

                auto patch = project.captureCurrentPatch (name);
                patch.isDefault = true;
                auto preset = patch.toPreset();
                preset.isDefault = true;
                for (auto& existing : project.getPatches())
                    existing.isDefault = false;
                for (auto& existing : project.getPresets())
                    existing.isDefault = false;

                registerPatchWithExpansion (project, patch, preset);

                const int existingPatchIndex = findPatchIndexById (project, patch.id);
                if (existingPatchIndex >= 0)
                    project.getPatches()[(size_t) existingPatchIndex] = patch;
                else
                    project.getPatches().push_back (patch);

                const int existingIndex = findPresetIndexByName (project, preset.name);
                if (existingIndex >= 0)
                    project.getPresets()[(size_t) existingIndex] = preset;
                else
                    project.getPresets().push_back (preset);
                project.getManifest().defaultPreset = preset.name;
                project.notifyChanged();

                juce::String error;
                if (! project.save (project.getProjectFolder(), error))
                {
                    showSaveError ("Save Patch As", error);
                    return;
                }

                refreshAllPanels();
            }), true);
    }

    void StudioMainComponent::saveCurrentSectionPreset()
    {
        if (! project.getProjectFolder().isDirectory())
        {
            showSaveError ("Save Section Preset", "Save the project first so section presets can be written to disk.");
            return;
        }

        const auto sectionId = bottomPanel != nullptr ? bottomPanel->getDspPatchSectionId() : juce::String ("source");
        const auto sectionLabel = bottomPanel != nullptr ? bottomPanel->getDspPatchSectionLabel() : juce::String ("Source");
        auto* window = new juce::AlertWindow ("Save Section Preset",
            "Name this reusable " + sectionLabel + " section. It stores only this section's blocks, routes, automation, and parameter values.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", sectionLabel + " Preset", "Section Preset Name:");
        window->addButton ("Save Section", 1);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window, sectionId] (int result)
            {
                const auto name = window->getTextEditorContents ("name").trim();
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result != 1 || name.isEmpty())
                    return;

                auto preset = project.captureSectionPreset (sectionId, name);
                auto& presets = project.getSectionPresets();
                bool replaced = false;
                for (auto& existing : presets)
                {
                    if (existing.id == preset.id)
                    {
                        existing = preset;
                        replaced = true;
                        break;
                    }
                }
                if (! replaced)
                    presets.push_back (std::move (preset));
                project.notifyChanged();

                juce::String error;
                if (! project.save (project.getProjectFolder(), error))
                {
                    showSaveError ("Save Section Preset", error);
                    return;
                }
                refreshAllPanels();
            }), true);
    }

    void StudioMainComponent::sendToExpansionPack()
    {
        if (showSampleExportValidationIfBlocked (*this, project, "Send to Expansion Pack"))
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Choose Expansion Pack folder",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto expansionFolder = fc.getResult();
                if (expansionFolder == juce::File()) return;

                const auto safeName = project.getManifest().instrumentName
                    .replaceCharacters ("\\/:*?\"<>|", "_________");
                auto packFolder = expansionFolder.getChildFile (safeName + ".patchcraft");
                auto& expansion = project.ensureExpansion (expansionFolder.getFileName().isNotEmpty()
                    ? expansionFolder.getFileName()
                    : project.getManifest().instrumentName + " Expansion");
                auto patch = project.captureCurrentPatch (project.getManifest().defaultPreset.isNotEmpty()
                    ? project.getManifest().defaultPreset
                    : project.getManifest().instrumentName + " Patch");
                patch.expansionId = expansion.id;
                patch.isDefault = true;
                for (auto& existing : project.getPatches())
                    existing.isDefault = false;
                const int existingPatchIndex = findPatchIndexById (project, patch.id);
                if (existingPatchIndex >= 0)
                    project.getPatches()[(size_t) existingPatchIndex] = patch;
                else
                    project.getPatches().push_back (patch);

                auto preset = patch.toPreset();
                preset.expansionId = expansion.id;
                preset.isDefault = true;
                for (auto& existing : project.getPresets())
                    existing.isDefault = false;
                const int existingPresetIndex = findPresetIndexByName (project, preset.name);
                if (existingPresetIndex >= 0)
                    project.getPresets()[(size_t) existingPresetIndex] = preset;
                else
                    project.getPresets().push_back (preset);

                expansion.includedPatchIds.addIfNotAlreadyThere (patch.id);
                expansion.includedPresetNames.addIfNotAlreadyThere (preset.name);
                for (const auto& asset : patch.includedAssets)
                    expansion.includedAssets.addIfNotAlreadyThere (asset);
                project.notifyChanged();

                PatchCraftPackWriter writer;
                juce::String err;
                const auto warnings = validationWarningSummary (project);
                if (! writer.write (project, packFolder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Send to Expansion Pack")
                            .withMessage ("Send failed: " + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }

                auto* expansionRoot = new juce::DynamicObject();
                expansionRoot->setProperty ("format", "PatchCraft Expansion");
                expansionRoot->setProperty ("formatVersion", 1);
                expansionRoot->setProperty ("metadata", expansion.toVar());
                juce::Array<juce::var> packs;
                packs.add (packFolder.getFileName());
                expansionRoot->setProperty ("includedPacks", packs);
                expansionFolder.createDirectory();
                expansionFolder.getChildFile ("expansion.json")
                    .replaceWithText (juce::JSON::toString (juce::var (expansionRoot), true));

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Send to Expansion Pack")
                        .withMessage ("Added instrument pack:\n" + packFolder.getFullPathName() + warnings)
                        .withButton ("OK")
                        .withIconType (warnings.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon
                                                             : juce::MessageBoxIconType::InfoIcon),
                    nullptr);
            });
    }

    void StudioMainComponent::importSamples (std::function<void (bool imported)> onComplete)
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import samples", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        auto completion = std::make_shared<std::function<void (bool imported)>> (std::move (onComplete));
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectMultipleItems
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser, completion] (const juce::FileChooser& fc)
            {
                const auto results = fc.getResults();
                if (results.isEmpty())
                {
                    if (completion != nullptr && *completion)
                        (*completion) (false);
                    return;
                }

                importSampleFiles (results, false, true, "keyboard", 36, 0);
                if (packRuntime != nullptr && graphAudioListen)
                    packRuntime->requestReloadImmediate();

                if (completion != nullptr && *completion)
                    (*completion) (true);
            });
    }

    void StudioMainComponent::openSoundMapperForChopZone (int zoneIndex)
    {
        setBottomTab (BottomPanel::Page::Samples);
        if (bottomPanel != nullptr && zoneIndex >= 0)
            bottomPanel->selectSampleZone (zoneIndex);
    }

    void StudioMainComponent::importSampleFiles (const juce::Array<juce::File>& files,
                                                 bool switchToMapper,
                                                 bool spanMappedRoots,
                                                 juce::String sampleMappingMode,
                                                 int targetNote,
                                                 int targetPadIndex)
    {
        sampleMappingMode = sampleMappingMode.trim().toLowerCase();
        targetNote = juce::jlimit (-1, 127, targetNote);
        targetPadIndex = juce::jlimit (-1, 63, targetPadIndex);
        const bool requestedPadMode = sampleMappingMode == "pads" || targetPadIndex >= 0;
        if (requestedPadMode)
        {
            if (targetPadIndex < 0 && targetNote >= 0)
                targetPadIndex = juce::jlimit (0, 63, targetNote - 36);

            if (targetNote < 0 && targetPadIndex >= 0)
                targetNote = juce::jlimit (0, 127, 36 + targetPadIndex);

            if (targetNote < 0 && targetPadIndex < 0)
            {
                int nextPadIndex = 0;
                for (const auto& zone : project.getSampleMap().getZones())
                    if (zone.padIndex >= 0)
                        nextPadIndex = juce::jmax (nextPadIndex, zone.padIndex + 1);

                targetPadIndex = juce::jlimit (0, 63, nextPadIndex);
                targetNote = juce::jlimit (0, 127, 36 + targetPadIndex);
            }
        }

        const bool hasTargetNote = targetNote >= 0;
        const bool hasTargetPad = targetPadIndex >= 0;
        const bool padMode = requestedPadMode || hasTargetPad;
        const bool keyboardMode = sampleMappingMode == "keyboard";
        const bool zoneMode = sampleMappingMode == "zone";

        int baseNote = 24; // C0 fallback when filenames/audio do not expose pitch.
        constexpr int zoneSize = 1;
        bool anyParsedRoot = false;
        bool anyNamePitch = false;
        bool anyAudioPitch = false;
        std::vector<SampleZoneDef> importedZones;
        importedZones.reserve ((size_t) files.size());
        for (auto& f : files)
        {
            if (! f.existsAsFile())
                continue;

            const int fallbackRoot = juce::jlimit (0, 127, baseNote);
            bool usedNamePitch = false;
            bool usedAudioPitch = false;
            auto z = SampleMap::inferZoneFromFileWithAudio (f,
                                                            fallbackRoot,
                                                            baseNote,
                                                            juce::jmin (127, baseNote + zoneSize - 1),
                                                            &usedNamePitch,
                                                            &usedAudioPitch);
            anyParsedRoot = anyParsedRoot || usedNamePitch || usedAudioPitch;
            anyNamePitch = anyNamePitch || usedNamePitch;
            anyAudioPitch = anyAudioPitch || usedAudioPitch;
            importedZones.push_back (z);
            baseNote += zoneSize;
            if (baseNote > 108) baseNote = 24;
        }
        std::map<int, int> initialRootCounts;
        for (const auto& zone : importedZones)
            ++initialRootCounts[zone.rootNote];

        if (! anyNamePitch
            && anyAudioPitch
            && initialRootCounts.size() <= 1
            && importedZones.size() > 1)
        {
            int fallbackNote = 24;
            for (auto& zone : importedZones)
            {
                zone.rootNote = fallbackNote;
                zone.lowNote = fallbackNote;
                zone.highNote = fallbackNote;
                if (++fallbackNote > 108)
                    fallbackNote = 24;
            }
            anyParsedRoot = false;
        }

        std::map<int, int> rootCounts;
        for (const auto& zone : importedZones)
            ++rootCounts[zone.rootNote];
        std::map<int, int> rootRoundRobinIndex;
        for (auto& zone : importedZones)
        {
            const bool stackedRoot = rootCounts[zone.rootNote] > 1;
            const bool noVelocityLayer = zone.lowVelocity == 1 && zone.highVelocity == 127;
            const bool noRoundRobin = zone.roundRobinGroup == 0 && zone.roundRobinIndex == 0;
            if (stackedRoot && noVelocityLayer && noRoundRobin)
            {
                zone.roundRobinGroup = 1;
                zone.roundRobinIndex = ++rootRoundRobinIndex[zone.rootNote];
            }
        }

        if (hasTargetNote)
        {
            for (int i = 0; i < (int) importedZones.size(); ++i)
            {
                auto& zone = importedZones[(size_t) i];
                const int pad = hasTargetPad ? juce::jlimit (0, 63, targetPadIndex + i) : -1;
                const int note = hasTargetPad
                    ? juce::jlimit (0, 127, targetNote + i)
                    : juce::jlimit (0, 127, targetNote + (keyboardMode ? i : 0));

                zone.rootNote = note;
                zone.lowNote = keyboardMode && ! zoneMode ? 0 : note;
                zone.highNote = keyboardMode && ! zoneMode ? 127 : note;
                zone.lowVelocity = 1;
                zone.highVelocity = 127;
                zone.oneShot = padMode || zoneMode;
                zone.loopEnabled = false;

                if (padMode)
                {
                    zone.padIndex = pad;
                    zone.padLabel = zone.padLabel.isNotEmpty()
                        ? zone.padLabel
                        : juce::File (zone.samplePath).getFileNameWithoutExtension();
                    zone.group = "Drum Pads";
                    zone.roundRobinGroup = 0;
                    zone.roundRobinIndex = 0;
                }
                else
                {
                    zone.padIndex = -1;
                    if (zone.group.isEmpty())
                        zone.group = zoneMode ? "Drop Zones" : "Keyboard";
                }
            }
            anyParsedRoot = false;
            spanMappedRoots = false;
        }

        if (! importedZones.empty())
        {
            project.performSampleMapEdit ("Import samples",
                [zonesToAdd = importedZones,
                 shouldAutoMap = anyParsedRoot && spanMappedRoots,
                 hasTargetNote,
                 hasTargetPad,
                 padMode] (SampleMap& map)
                {
                    if (hasTargetNote)
                    {
                        auto& existing = map.getZones();
                        juce::Array<int> targetNotes;
                        juce::Array<int> targetPads;
                        for (const auto& zone : zonesToAdd)
                        {
                            targetNotes.addIfNotAlreadyThere (zone.rootNote);
                            if (padMode && zone.padIndex >= 0)
                                targetPads.addIfNotAlreadyThere (zone.padIndex);
                        }

                        existing.erase (std::remove_if (existing.begin(), existing.end(),
                            [&] (const SampleZoneDef& zone)
                            {
                                if (hasTargetPad && targetPads.contains (zone.padIndex))
                                    return true;
                                for (int note : targetNotes)
                                    if (note == zone.rootNote || (note >= zone.lowNote && note <= zone.highNote))
                                        return true;
                                return false;
                            }), existing.end());
                    }

                    for (const auto& zone : zonesToAdd)
                        map.add (zone);
                    if (shouldAutoMap)
                        map.autoMapByRootNotes();
                });
        }
        if (! importedZones.empty() && project.getEngineType() != "sample")
            project.setEngineType ("sample");
        else if (! importedZones.empty())
            project.notifyChanged();

        if (! importedZones.empty() && switchToMapper)
            setBottomTab (BottomPanel::Page::Samples);
    }

    bool StudioMainComponent::assignMidiFilesToSampleMap (const juce::Array<juce::File>& files,
                                                          juce::String& report,
                                                          int targetNote,
                                                          int targetPadIndex)
    {
        juce::Array<juce::File> midiFiles;
        for (const auto& file : files)
            if (isSupportedRuntimeMidiFile (file))
                midiFiles.add (file);

        if (midiFiles.isEmpty())
        {
            report = "No MIDI files assigned. Drop .mid or .midi files.";
            return false;
        }

        targetNote = juce::jlimit (-1, 127, targetNote);
        targetPadIndex = juce::jlimit (-1, 63, targetPadIndex);

        const auto& currentZones = project.getSampleMap().getZones();
        if (targetNote < 0 && currentZones.size() == 1)
            targetNote = currentZones.front().rootNote;

        if (targetNote < 0 && targetPadIndex < 0)
        {
            report = "Drop MIDI on a pad, key, or sample drop zone so PatchCraft knows which sample zone should play it.";
            return false;
        }

        int assigned = 0;
        int missed = 0;
        juce::StringArray assignedTargets;

        project.performSampleMapEdit ("Assign MIDI to sample zones",
            [&] (SampleMap& map)
            {
                auto& zones = map.getZones();
                for (int i = 0; i < midiFiles.size(); ++i)
                {
                    const auto& file = midiFiles.getReference (i);
                    const int pad = targetPadIndex >= 0 ? juce::jlimit (0, 63, targetPadIndex + i) : -1;
                    const int note = targetNote >= 0 ? juce::jlimit (0, 127, targetNote + i) : -1;

                    auto matchZone = [&] () -> SampleZoneDef*
                    {
                        for (auto& zone : zones)
                        {
                            if (pad >= 0 && zone.padIndex == pad)
                                return &zone;
                            if (note >= 0 && note >= zone.lowNote && note <= zone.highNote)
                                return &zone;
                            if (note >= 0 && zone.rootNote == note)
                                return &zone;
                        }
                        return nullptr;
                    };

                    if (auto* zone = matchZone())
                    {
                        zone->midiPath = file.getFullPathName();
                        zone->midiPlaybackMode = pad >= 0 ? "drum" : "trigger";
                        zone->midiHostSync = true;
                        zone->midiTranspose = 0;
                        zone->midiVelocityAmount = 1.0f;
                        ++assigned;
                        const int labelNote = note >= 0 ? note : zone->rootNote;
                        assignedTargets.add (juce::MidiMessage::getMidiNoteName (labelNote, true, true, 4));
                    }
                    else
                    {
                        ++missed;
                    }
                }
            });

        if (assigned <= 0)
        {
            report = "MIDI was not assigned because no sample zone exists at that target. Drop a sample on the pad/key first.";
            return false;
        }

        if (project.getEngineType() != "sample")
            project.setEngineType ("sample");
        else
            project.notifyChanged();

        report = "Assigned " + juce::String (assigned) + " MIDI file"
               + (assigned == 1 ? "" : "s")
               + " to " + assignedTargets.joinIntoString (", ") + ".";
        if (missed > 0)
            report += " " + juce::String (missed) + " file"
                   + (missed == 1 ? "" : "s")
                   + " had no matching sample zone.";
        return true;
    }

    void StudioMainComponent::importBackground()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Import background image", juce::File(), "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;

                if (project.getProjectFolder().isDirectory())
                {
                    auto dst = project.getAssetsFolder().getChildFile ("background.png");
                    project.getAssetsFolder().createDirectory();
                    f.copyFileTo (dst);
                    project.backgroundImageRelative = "assets/background.png";
                    project.getManifest().backgroundImage = project.backgroundImageRelative;
                }
                else
                {
                    project.backgroundImageRelative = f.getFullPathName();
                    project.getManifest().backgroundImage = project.backgroundImageRelative;
                }
                auto imageFile = juce::File::isAbsolutePath (project.backgroundImageRelative)
                    ? juce::File (project.backgroundImageRelative)
                    : project.getProjectFolder().getChildFile (project.backgroundImageRelative);
                project.performLayoutEdit ("Add background layer", [&] (LayoutModel& layout)
                {
                    const auto& canvas = project.getCanvasSize();
                    auto* bg = layout.find ("background");
                    if (bg == nullptr)
                    {
                        LayoutElement layer;
                        layer.type = ElementType::Image;
                        layer.id = "background";
                        layer.label = "Background";
                        layer.x = 0;
                        layer.y = 0;
                        layer.width = juce::jmax (1, canvas.width);
                        layer.height = juce::jmax (1, canvas.height);
                        layer.locked = true;
                        layer.asset = imageFile.getFullPathName();
                        layout.add (layer);
                    }
                    else
                    {
                        bg->type = ElementType::Image;
                        bg->label = bg->label.isNotEmpty() ? bg->label : "Background";
                        bg->x = 0;
                        bg->y = 0;
                        bg->width = juce::jmax (1, canvas.width);
                        bg->height = juce::jmax (1, canvas.height);
                        bg->locked = true;
                        bg->visible = true;
                        bg->asset = imageFile.getFullPathName();
                    }
                });
                assets.clear();
                project.notifyChanged();
            });
    }

    void StudioMainComponent::generateAiBackground()
    {
        auto* aw = new juce::AlertWindow ("Generate Background",
                                          "Create background artwork for the current instrument. No baked-in knobs or text.",
                                          juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("prompt", "", "Direction:");
        aw->addButton ("Generate", 1);
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int action)
            {
                std::unique_ptr<juce::AlertWindow> own (aw);
                if (action != 1)
                    return;

                auto direction = aw->getTextEditorContents ("prompt").trim();
                const auto config = AiAssistService::loadCloudIntegrationConfig();
                const auto& canvas = project.getCanvasSize();
                auto output = project.getProjectFolder().isDirectory()
                    ? project.getAssetsFolder().getChildFile ("ai-background.png")
                    : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("PatchCraft")
                        .getChildFile ("Generated")
                        .getChildFile ("ai-background-" + juce::String (juce::Time::getMillisecondCounter()) + ".png");

                AiImageService::Request request;
                request.kind = AiImageService::ImageKind::Background;
                request.width = canvas.width;
                request.height = canvas.height;
                request.outputFile = output;
                request.prompt = AiImageService::buildPrompt (request.kind, project, direction);

                juce::Component::SafePointer<StudioMainComponent> safeThis (this);
                std::thread ([safeThis, request, config]
                {
                    const auto result = AiImageService::generate (request, config);
                    juce::MessageManager::callAsync ([safeThis, result]
                    {
                        if (safeThis == nullptr)
                            return;

                        if (result.success)
                        {
                            auto& project = safeThis->project;
                            if (project.getProjectFolder().isDirectory()
                                && result.outputFile.isAChildOf (project.getProjectFolder()))
                                project.backgroundImageRelative = result.outputFile.getRelativePathFrom (project.getProjectFolder()).replaceCharacter ('\\', '/');
                            else
                                project.backgroundImageRelative = result.outputFile.getFullPathName();

                            project.getManifest().backgroundImage = project.backgroundImageRelative;
                            const auto imageFile = result.outputFile;
                            project.performLayoutEdit ("Add generated background layer", [&] (LayoutModel& layout)
                            {
                                const auto& canvas = project.getCanvasSize();
                                auto* background = layout.find ("background");
                                if (background == nullptr)
                                {
                                    LayoutElement layer;
                                    layer.type = ElementType::Image;
                                    layer.id = "background";
                                    layer.label = "Background";
                                    layer.x = 0;
                                    layer.y = 0;
                                    layer.width = juce::jmax (1, canvas.width);
                                    layer.height = juce::jmax (1, canvas.height);
                                    layer.locked = true;
                                    layer.asset = imageFile.getFullPathName();
                                    layout.add (layer);
                                }
                                else
                                {
                                    background->type = ElementType::Image;
                                    background->label = background->label.isNotEmpty() ? background->label : "Background";
                                    background->x = 0;
                                    background->y = 0;
                                    background->width = juce::jmax (1, canvas.width);
                                    background->height = juce::jmax (1, canvas.height);
                                    background->locked = true;
                                    background->visible = true;
                                    background->asset = imageFile.getFullPathName();
                                }
                            });
                            safeThis->assets.clear();
                            project.markDirty();
                            project.notifyChanged();
                        }

                        juce::AlertWindow::showAsync (
                            juce::MessageBoxOptions()
                                .withTitle (result.success ? "Background Generated" : "Background Generation Failed")
                                .withMessage (result.message + (result.outputFile != juce::File()
                                    ? "\n\n" + result.outputFile.getFullPathName() : juce::String()))
                                .withButton ("OK")
                                .withIconType (result.success ? juce::MessageBoxIconType::InfoIcon
                                                              : juce::MessageBoxIconType::WarningIcon),
                            nullptr);
                    });
                }).detach();
            }), true);
    }

    void StudioMainComponent::generateAiImageAsset()
    {
        auto* aw = new juce::AlertWindow ("Generate Image Asset",
                                          "Create a reusable transparent image asset and place it on the Design canvas.",
                                          juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("prompt", "", "Direction:");
        aw->addButton ("Generate", 1);
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, aw] (int action)
            {
                std::unique_ptr<juce::AlertWindow> own (aw);
                if (action != 1)
                    return;

                const auto direction = aw->getTextEditorContents ("prompt").trim();
                const auto config = AiAssistService::loadCloudIntegrationConfig();
                auto output = project.getProjectFolder().isDirectory()
                    ? project.getAssetsFolder().getChildFile ("images").getChildFile ("ai-asset-" + juce::String (juce::Time::getMillisecondCounter()) + ".png")
                    : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("PatchCraft")
                        .getChildFile ("Generated")
                        .getChildFile ("ai-asset-" + juce::String (juce::Time::getMillisecondCounter()) + ".png");

                AiImageService::Request request;
                request.kind = AiImageService::ImageKind::Asset;
                request.width = 512;
                request.height = 512;
                request.transparent = true;
                request.outputFile = output;
                request.prompt = AiImageService::buildPrompt (request.kind, project, direction);

                juce::Component::SafePointer<StudioMainComponent> safeThis (this);
                std::thread ([safeThis, request, config]
                {
                    const auto result = AiImageService::generate (request, config);
                    juce::MessageManager::callAsync ([safeThis, result]
                    {
                        if (safeThis == nullptr)
                            return;

                        if (result.success)
                        {
                            auto& project = safeThis->project;
                            const auto& canvas = project.getCanvasSize();
                            project.performLayoutEdit ("Add generated image asset", [&] (LayoutModel& layout)
                            {
                                LayoutElement element;
                                element.type = ElementType::Image;
                                element.id = layout.generateUniqueId ("image_");
                                element.label = result.outputFile.getFileNameWithoutExtension();
                                element.asset = project.getProjectFolder().isDirectory()
                                    && result.outputFile.isAChildOf (project.getProjectFolder())
                                        ? result.outputFile.getRelativePathFrom (project.getProjectFolder()).replaceCharacter ('\\', '/')
                                        : result.outputFile.getFullPathName();
                                element.x = canvas.width / 2 - 128;
                                element.y = canvas.height / 2 - 128;
                                element.width = 256;
                                element.height = 256;
                                layout.add (element);
                            });
                            if (! project.getLayout().getAll().empty())
                                safeThis->setSelectedElementId (project.getLayout().getAll().back().id);
                            safeThis->assets.clear();
                            project.notifyChanged();
                        }

                        juce::AlertWindow::showAsync (
                            juce::MessageBoxOptions()
                                .withTitle (result.success ? "Image Asset Generated" : "Image Generation Failed")
                                .withMessage (result.message + (result.outputFile != juce::File()
                                    ? "\n\n" + result.outputFile.getFullPathName() : juce::String()))
                                .withButton ("OK")
                                .withIconType (result.success ? juce::MessageBoxIconType::InfoIcon
                                                              : juce::MessageBoxIconType::WarningIcon),
                            nullptr);
                    });
                }).detach();
            }), true);
    }

    void StudioMainComponent::aiAssist()
    {
        juce::Component::SafePointer<StudioMainComponent> safeThis (this);
        auto* content = new CopilotDialogContent();

        content->onSettings = [safeThis]
        {
            if (auto* component = safeThis.getComponent())
                component->openSettings();
        };

        content->onGenerateBackground = [safeThis]
        {
            if (auto* component = safeThis.getComponent())
                component->generateAiBackground();
        };

        content->onGenerateAsset = [safeThis]
        {
            if (auto* component = safeThis.getComponent())
                component->generateAiImageAsset();
        };

        content->onPublish = [safeThis]
        {
            if (auto* component = safeThis.getComponent())
                component->publishToPluginClub();
        };

        content->onTask = [safeThis] (AiAssistService::TaskType task)
        {
            if (auto* component = safeThis.getComponent())
            {
                const auto proposal = component->ai.run (task, component->project);

                auto* result = new juce::AlertWindow (
                    proposal.title,
                    juce::String(), juce::MessageBoxIconType::NoIcon);
                result->addTextBlock (proposal.summary + "\n\nContext pack:\n" + proposal.contextSummary);
                result->addTextEditor ("preview", proposal.details, "Preview", false);
                if (auto* editor = result->getTextEditor ("preview"))
                {
                    editor->setMultiLine (true);
                    editor->setReadOnly (true);
                    editor->setScrollbarsShown (true);
                    editor->setCaretVisible (false);
                    editor->setSize (760, 360);
                }
                result->addButton ("Copy Preview", 1);
                result->addButton ("Close", 0);
                result->enterModalState (true,
                    juce::ModalCallbackFunction::create ([result, text = proposal.details] (int action)
                    {
                        if (action == 1)
                            juce::SystemClipboard::copyTextToClipboard (text);
                        std::unique_ptr<juce::AlertWindow> r (result);
                    }), true);
            }
        };

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "PatchCraft Copilot";
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.useBottomRightCornerResizer = true;
        options.componentToCentreAround = this;
        options.content.setOwned (content);
        if (auto* window = options.launchAsync())
            window->setResizeLimits (640, 500, 1180, 900);
    }

    void StudioMainComponent::deleteSelected()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Delete selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    juce::StringArray tabGroups;
                    if (el->type == ElementType::TabPanel)
                        for (const auto& tab : el->tabs)
                            tabGroups.addIfNotAlreadyThere (studioScopedTabGroupId (*el, tab));

                    for (auto& candidate : m.getAll())
                    {
                        if (candidate.id == id)
                            continue;
                        if (candidate.containerId == id)
                            candidate.containerId.clear();
                        if (candidate.groupId == id || tabGroups.contains (candidate.groupId))
                            candidate.groupId.clear();
                    }
                    m.remove (id);
                }
        });
        clearSelection();
    }

    void StudioMainComponent::duplicateSelected()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        juce::StringArray expandedIds;
        auto addWithChildren = [&] (auto& self, const juce::String& id) -> void
        {
            if (id.isEmpty() || expandedIds.contains (id))
                return;

            expandedIds.add (id);
            for (const auto& child : project.getLayout().getAll())
                if (child.containerId == id)
                    self (self, child.id);
        };

        for (const auto& id : ids)
            addWithChildren (addWithChildren, id);

        juce::Rectangle<int> sourceBounds;
        bool hasBounds = false;
        for (const auto& id : expandedIds)
        {
            if (auto* el = project.getLayout().find (id))
            {
                const juce::Rectangle<int> itemBounds (el->x, el->y,
                                                       juce::jmax (1, el->width),
                                                       juce::jmax (1, el->height));
                sourceBounds = hasBounds ? sourceBounds.getUnion (itemBounds) : itemBounds;
                hasBounds = true;
            }
        }

        const int duplicateOffsetX = hasBounds ? sourceBounds.getWidth() + 24 : 120;
        juce::StringArray newIds;
        std::map<juce::String, juce::String> idRemap;
        project.performLayoutEdit ("Duplicate selection", [&] (LayoutModel& m)
        {
            // First pass: copy every selected element and record old->new id.
            for (const auto& id : expandedIds)
            {
                if (auto* el = m.find (id))
                {
                    LayoutElement copy = *el;
                    const auto oldId = copy.id;
                    copy.id.clear();          // forces add() to mint a unique id
                    copy.x += duplicateOffsetX;
                    auto& added = m.add (copy);
                    idRemap[oldId] = added.id;
                    if (ids.contains (oldId))
                        newIds.add (added.id);
                }
            }

            // Second pass: rewrite container/group references that point to
            // siblings inside the duplicated set, so a duplicated Group keeps
            // owning its (now duplicated) children. References that point
            // outside the set are left alone.
            for (const auto& newId : newIds)
            {
                if (auto* el = m.find (newId))
                {
                    if (auto it = idRemap.find (el->containerId); it != idRemap.end())
                        el->containerId = it->second;
                    if (auto it = idRemap.find (el->groupId); it != idRemap.end())
                        el->groupId = it->second;
                }
            }
        });
        if (! newIds.isEmpty())
            setSelectedElementIds (newIds);
    }

    void StudioMainComponent::copySelectedElements (bool includeParameters)
    {
        copiedLayoutElements.clear();
        copiedLayoutIncludesParameters = includeParameters;

        for (const auto& id : selectedElementIds)
        {
            if (auto* el = project.getLayout().find (id))
            {
                auto copy = *el;
                if (! includeParameters)
                {
                    copy.parameterId.clear();
                    copy.valueFormat = "Auto";
                    copy.mixerVolumeParams.clear();
                    copy.mixerPanParams.clear();
                    copy.mixerMuteParams.clear();
                    copy.mixerSoloParams.clear();
                }
                copiedLayoutElements.push_back (std::move (copy));
            }
        }

        if (inspectorPanel != nullptr)
            inspectorPanel->refresh();
    }

    void StudioMainComponent::pasteCopiedElements()
    {
        if (copiedLayoutElements.empty())
            return;

        juce::StringArray newIds;
        project.performLayoutEdit ("Paste elements", [&] (LayoutModel& m)
        {
            for (auto copy : copiedLayoutElements)
            {
                copy.id.clear();
                copy.x += 16;
                copy.y += 16;
                auto& added = m.add (copy);
                newIds.add (added.id);
            }
        });

        if (! newIds.isEmpty())
            setSelectedElementIds (newIds);
        refreshAllPanels();
    }

    void StudioMainComponent::copySelectedToAllTabs()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty())
            return;

        struct Fanout
        {
            juce::String sourceId;
            LayoutElement source;
            juce::StringArray targetGroups;
        };

        std::vector<Fanout> fanouts;
        const auto& elements = project.getLayout().getAll();
        for (const auto& id : ids)
        {
            const auto* source = project.getLayout().find (id);
            if (source == nullptr || source->locked)
                continue;

            for (const auto& candidate : elements)
            {
                if (candidate.type != ElementType::TabPanel || candidate.tabs.isEmpty())
                    continue;

                juce::StringArray groups;
                for (const auto& tab : candidate.tabs)
                    groups.addIfNotAlreadyThere (studioScopedTabGroupId (candidate, tab));

                if (! groups.contains (source->groupId))
                    continue;

                Fanout fanout;
                fanout.sourceId = id;
                fanout.source = *source;
                for (const auto& group : groups)
                    if (group != source->groupId)
                        fanout.targetGroups.addIfNotAlreadyThere (group);

                if (! fanout.targetGroups.isEmpty())
                    fanouts.push_back (std::move (fanout));
                break;
            }
        }

        if (fanouts.empty())
            return;

        juce::StringArray newIds;
        project.performLayoutEdit ("Copy selection to all tabs", [&] (LayoutModel& m)
        {
            for (const auto& fanout : fanouts)
            {
                juce::String linkedGroupId;
                if (copySelectionToTabsAsReference)
                {
                    linkedGroupId = fanout.source.linkedCopyGroupId;
                    if (linkedGroupId.isEmpty())
                        linkedGroupId = "linked_tabs_" + juce::Uuid().toString();

                    if (auto* source = m.find (fanout.sourceId))
                        source->linkedCopyGroupId = linkedGroupId;
                }

                for (const auto& group : fanout.targetGroups)
                {
                    auto copy = fanout.source;
                    copy.id.clear();
                    copy.groupId = group;
                    copy.linkedCopyGroupId = linkedGroupId;

                    if (copy.containerId.isNotEmpty())
                    {
                        if (auto* parent = m.find (copy.containerId))
                        {
                            if (parent->groupId.isNotEmpty() && parent->groupId != group)
                                copy.containerId.clear();
                        }
                    }

                    auto& added = m.add (copy);
                    newIds.add (added.id);
                }
            }
        });

        if (! newIds.isEmpty())
            setSelectedElementIds (newIds);
        refreshAllPanels();
    }

    void StudioMainComponent::propagateLinkedElementChange (const juce::String& sourceId)
    {
        auto* source = project.getLayout().find (sourceId);
        if (source == nullptr || source->linkedCopyGroupId.isEmpty())
            return;

        const auto linkedGroupId = source->linkedCopyGroupId;
        auto& elements = project.getLayout().getAll();
        for (auto& target : elements)
        {
            if (target.id == source->id || target.linkedCopyGroupId != linkedGroupId)
                continue;

            const auto keepId = target.id;
            const auto keepGroupId = target.groupId;
            const auto keepContainerId = target.containerId;
            target = *source;
            target.id = keepId;
            target.groupId = keepGroupId;
            target.containerId = keepContainerId;
            target.linkedCopyGroupId = linkedGroupId;
        }
    }

    void StudioMainComponent::groupSelectedElements()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        juce::String groupId;
        project.performLayoutEdit ("Group selection", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            juce::String commonContainer;
            juce::String commonGroup;
            bool first = true;

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && el->type != ElementType::Group)
                {
                    const auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                    bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                    if (first)
                    {
                        commonContainer = el->containerId;
                        commonGroup = el->groupId;
                        first = false;
                    }
                    else
                    {
                        if (commonContainer != el->containerId) commonContainer.clear();
                        if (commonGroup != el->groupId) commonGroup.clear();
                    }
                }

            LayoutElement group;
            group.type = ElementType::Group;
            group.id = m.generateUniqueId ("group_");
            group.label = "New Group";
            group.x = bounds.isEmpty() ? 40 : bounds.getX();
            group.y = bounds.isEmpty() ? 40 : bounds.getY();
            group.width = bounds.isEmpty() ? 240 : juce::jmax (32, bounds.getWidth());
            group.height = bounds.isEmpty() ? 160 : juce::jmax (32, bounds.getHeight());
            group.containerId = commonContainer;
            group.groupId = commonGroup;
            groupId = group.id;
            m.add (group);

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && el->id != groupId && ! el->locked)
                    el->containerId = groupId;
        });

        if (groupId.isNotEmpty())
            setSelectedElementId (groupId);
        refreshAllPanels();
    }

    void StudioMainComponent::ungroupSelectedElements()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Ungroup selection", [&] (LayoutModel& m)
        {
            juce::StringArray groupsToRemove;
            for (const auto& id : ids)
            {
                if (auto* el = m.find (id))
                {
                    if (el->type == ElementType::Group)
                    {
                        for (auto& child : m.getAll())
                            if (child.containerId == id)
                                child.containerId = el->containerId;
                        groupsToRemove.addIfNotAlreadyThere (id);
                    }
                    else
                    {
                        el->containerId.clear();
                    }
                }
            }

            for (const auto& id : groupsToRemove)
                m.remove (id);
        });

        clearSelection();
        refreshAllPanels();
    }

    void StudioMainComponent::removeSelectedFromContainer()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Remove from container", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    el->containerId.clear();
        });
        refreshAllPanels();
    }

    void StudioMainComponent::toggleCanvasGrid()
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->setGridVisible (! canvasEditor->isGridVisible());
        menuBar.repaint();
    }

    void StudioMainComponent::toggleCanvasRulers()
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->setRulersVisible (! canvasEditor->areRulersVisible());
        resized();
        menuBar.repaint();
    }

    void StudioMainComponent::setCanvasZoom (float zoomFactor)
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->setZoom (zoomFactor);
        if (canvasToolbar != nullptr)
            canvasToolbar->refresh();
        menuBar.repaint();
    }

    void StudioMainComponent::fitCanvasToWindow()
    {
        if (canvasEditor == nullptr) return;
        canvasEditor->fit();
        if (canvasToolbar != nullptr)
            canvasToolbar->refresh();
        menuBar.repaint();
    }

    void StudioMainComponent::alignSelected (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 2) return;

        project.performLayoutEdit ("Align selection", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                    bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                }

            if (bounds.isEmpty()) return;

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    if (command == "left")    el->x = bounds.getX();
                    if (command == "hcenter") el->x = bounds.getCentreX() - el->width / 2;
                    if (command == "right")   el->x = bounds.getRight() - el->width;
                    if (command == "top")     el->y = bounds.getY();
                    if (command == "vcenter") el->y = bounds.getCentreY() - el->height / 2;
                    if (command == "bottom")  el->y = bounds.getBottom() - el->height;
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::alignSelectedToCanvas (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        const auto canvas = project.getCanvasSize();
        project.performLayoutEdit ("Align selection to canvas", [&] (LayoutModel& m)
        {
            juce::Rectangle<int> bounds;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    const auto r = juce::Rectangle<int> (el->x, el->y, el->width, el->height);
                    bounds = bounds.isEmpty() ? r : bounds.getUnion (r);
                }

            if (bounds.isEmpty()) return;

            int dx = 0;
            int dy = 0;
            if (command == "left")         dx = -bounds.getX();
            else if (command == "hcenter") dx = canvas.width / 2 - bounds.getCentreX();
            else if (command == "right")   dx = canvas.width - bounds.getRight();
            else if (command == "top")     dy = -bounds.getY();
            else if (command == "vcenter") dy = canvas.height / 2 - bounds.getCentreY();
            else if (command == "bottom")  dy = canvas.height - bounds.getBottom();
            else if (command == "center")
            {
                dx = canvas.width / 2 - bounds.getCentreX();
                dy = canvas.height / 2 - bounds.getCentreY();
            }

            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    el->x += dx;
                    el->y += dy;
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::distributeSelected (bool horizontal)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 3) return;

        project.performLayoutEdit ("Distribute selection", [&] (LayoutModel& m)
        {
            std::vector<LayoutElement*> elements;
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    elements.push_back (el);

            if (elements.size() < 3) return;

            std::sort (elements.begin(), elements.end(), [horizontal] (const LayoutElement* a, const LayoutElement* b)
            {
                return horizontal ? a->x < b->x : a->y < b->y;
            });

            const auto firstStart = horizontal ? elements.front()->x : elements.front()->y;
            const auto lastEnd = horizontal ? elements.back()->x + elements.back()->width
                                            : elements.back()->y + elements.back()->height;
            int totalSize = 0;
            for (const auto* el : elements)
                totalSize += horizontal ? el->width : el->height;

            const int gap = (lastEnd - firstStart - totalSize) / juce::jmax (1, (int) elements.size() - 1);
            int cursor = firstStart;
            for (auto* el : elements)
            {
                if (horizontal)
                {
                    el->x = cursor;
                    cursor += el->width + gap;
                }
                else
                {
                    el->y = cursor;
                    cursor += el->height + gap;
                }
            }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::matchSelectedSize (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.size() < 2) return;

        LayoutElement reference;
        bool hasReference = false;
        if (auto* selected = project.getLayout().find (selectedElementId);
            selected != nullptr && ! selected->locked)
        {
            reference = *selected;
            hasReference = true;
        }
        if (! hasReference)
            return;

        project.performLayoutEdit ("Match selection size", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (id != reference.id)
                    if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    {
                        if (command == "width" || command == "both")
                            el->width = reference.width;
                        if (command == "height" || command == "both")
                            el->height = reference.height;
                    }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::snapSelectedToGrid()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty() || canvasEditor == nullptr) return;
        const int grid = juce::jmax (1, canvasEditor->getSnapGrid());

        auto snap = [grid] (int value)
        {
            return juce::roundToInt ((float) value / (float) grid) * grid;
        };

        project.performLayoutEdit ("Snap selection to grid", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    el->x = snap (el->x);
                    el->y = snap (el->y);
                    el->width = juce::jmax (1, snap (el->width));
                    el->height = juce::jmax (1, snap (el->height));
                }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::setSelectedVisibility (bool visible)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit (visible ? "Show selection" : "Hide selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr)
                    el->visible = visible;
        });
        refreshAllPanels();
    }

    void StudioMainComponent::setSelectedLocked (bool locked)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit (locked ? "Lock selection" : "Unlock selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr)
                    el->locked = locked;
        });
        refreshAllPanels();
    }

    void StudioMainComponent::scaleSelectedElements (float scaleX, float scaleY)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty())
            return;

        scaleX = juce::jlimit (0.05f, 20.0f, scaleX);
        scaleY = juce::jlimit (0.05f, 20.0f, scaleY);

        juce::StringArray expandedIds;
        auto addWithChildren = [&] (auto& self, const juce::String& id) -> void
        {
            if (id.isEmpty() || expandedIds.contains (id))
                return;

            if (auto* el = project.getLayout().find (id); el != nullptr && ! el->locked && el->type != ElementType::Group)
            {
                expandedIds.add (id);
                for (const auto& child : project.getLayout().getAll())
                    if (child.containerId == id)
                        self (self, child.id);
            }
        };

        for (const auto& id : ids)
            addWithChildren (addWithChildren, id);

        if (expandedIds.isEmpty())
            return;

        int left = std::numeric_limits<int>::max();
        int top = std::numeric_limits<int>::max();
        int right = std::numeric_limits<int>::min();
        int bottom = std::numeric_limits<int>::min();
        bool found = false;

        for (const auto& id : expandedIds)
        {
            if (auto* el = project.getLayout().find (id); el != nullptr && ! el->locked)
            {
                left = juce::jmin (left, el->x);
                top = juce::jmin (top, el->y);
                right = juce::jmax (right, el->x + el->width);
                bottom = juce::jmax (bottom, el->y + el->height);
                found = true;
            }
        }

        if (! found)
            return;

        const float centreX = ((float) left + (float) right) * 0.5f;
        const float centreY = ((float) top + (float) bottom) * 0.5f;

        project.performLayoutEdit ("Scale selection", [&] (LayoutModel& m)
        {
            for (const auto& id : expandedIds)
            {
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                {
                    const float elementCentreX = (float) el->x + (float) el->width * 0.5f;
                    const float elementCentreY = (float) el->y + (float) el->height * 0.5f;
                    const float scaledCentreX = centreX + (elementCentreX - centreX) * scaleX;
                    const float scaledCentreY = centreY + (elementCentreY - centreY) * scaleY;
                    const float scaledWidth = juce::jmax (1.0f, (float) el->width * scaleX);
                    const float scaledHeight = juce::jmax (1.0f, (float) el->height * scaleY);

                    el->width = juce::jmax (1, juce::roundToInt (scaledWidth));
                    el->height = juce::jmax (1, juce::roundToInt (scaledHeight));
                    el->x = juce::roundToInt (scaledCentreX - (float) el->width * 0.5f);
                    el->y = juce::roundToInt (scaledCentreY - (float) el->height * 0.5f);

                    if (el->labelSize > 0.0f)
                        el->labelSize = juce::jmax (6.0f, el->labelSize * scaleY);
                    el->labelSpacing *= scaleY;
                    el->labelOffsetX *= scaleX;
                    el->labelOffsetY *= scaleY;
                    el->contentPadding *= juce::jmax (scaleX, scaleY);
                    el->cornerRadius = juce::jmax (0.0f, el->cornerRadius * juce::jmax (scaleX, scaleY));
                    el->strokeWidth = juce::jmax (0.0f, el->strokeWidth * juce::jmax (scaleX, scaleY));
                }
            }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::setSelectedLabelVisibility (bool visible, const juce::String& defaultPosition)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty())
            return;

        project.performLayoutEdit (visible ? "Show labels for selection" : "Hide labels for selection", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
            {
                if (auto* el = m.find (id); el != nullptr && isRuntimeControlElement (el->type))
                {
                    if (visible)
                    {
                        if (el->labelPosition == "hidden")
                            el->labelPosition = defaultPosition.isNotEmpty() ? defaultPosition : "bottom";
                    }
                    else
                    {
                        el->labelPosition = "hidden";
                    }
                }
            }
        });
        refreshAllPanels();
    }

    void StudioMainComponent::detachLabelsFromSelectedControls()
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        juce::StringArray newLabelIds;
        project.performLayoutEdit ("Detach control labels", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
            {
                auto* el = m.find (id);
                if (el == nullptr || el->locked || ! isRuntimeControlElement (el->type))
                    continue;

                const auto text = el->label.isNotEmpty() ? el->label : el->parameterId;
                if (text.isEmpty() || el->labelPosition == "hidden")
                    continue;

                const float fontSize = el->labelSize > 0.0f
                    ? el->labelSize
                    : juce::jmax (10.0f, (float) el->height * 0.13f);
                const int labelHeight = juce::jmax (18, juce::roundToInt (fontSize + 8.0f));
                const int labelWidth = juce::jmax (el->width, juce::roundToInt (text.length() * fontSize * 0.72f) + 24);

                int labelX = el->x + el->width / 2 - labelWidth / 2 + juce::roundToInt (el->labelOffsetX);
                int labelY = el->y + el->height + 4 + juce::roundToInt (el->labelSpacing + el->labelOffsetY);

                if (el->labelPosition == "top")
                {
                    labelY = el->y - labelHeight - 4 + juce::roundToInt (el->labelOffsetY);
                }
                else if (el->labelPosition == "left")
                {
                    labelX = el->x - labelWidth - 8 + juce::roundToInt (el->labelOffsetX - el->labelSpacing);
                    labelY = el->y + el->height / 2 - labelHeight / 2 + juce::roundToInt (el->labelOffsetY);
                }
                else if (el->labelPosition == "right")
                {
                    labelX = el->x + el->width + 8 + juce::roundToInt (el->labelOffsetX + el->labelSpacing);
                    labelY = el->y + el->height / 2 - labelHeight / 2 + juce::roundToInt (el->labelOffsetY);
                }
                else
                {
                    labelY = el->y + juce::roundToInt ((float) el->height * 0.65f)
                           + juce::roundToInt (el->labelSpacing + el->labelOffsetY);
                }

                LayoutElement label;
                label.type = ElementType::Label;
                label.id.clear();
                label.x = labelX;
                label.y = labelY;
                label.width = labelWidth;
                label.height = labelHeight;
                label.label = text;
                label.style = el->style;
                label.textColour = el->textColour;
                label.accentColour = el->accentColour;
                label.backgroundColour = juce::Colours::transparentBlack;
                label.borderColour = juce::Colours::transparentBlack;
                label.labelSize = fontSize;
                label.groupId = el->groupId;
                label.containerId = el->containerId;
                label.visible = el->visible;
                label.locked = false;

                auto& added = m.add (label);
                newLabelIds.add (added.id);

                el->labelPosition = "hidden";
                el->labelOffsetX = 0.0f;
                el->labelOffsetY = 0.0f;
                el->labelSpacing = 0.0f;
            }
        });

        if (! newLabelIds.isEmpty())
            setSelectedElementIds (newLabelIds);
        refreshAllPanels();
    }

    void StudioMainComponent::focusPscriptPanel()
    {
        exitCustomerPreviewIfActive();

        if (bottomTab != BottomPanel::Page::Design)
            setBottomTab (BottomPanel::Page::Design);

        showLayersInsteadOfElements = false;
        showLibraryInsteadOfElements = false;
        showScriptEditorInsteadOfElements = true;
        leftTabs.setCurrentTabIndex (3, juce::dontSendNotification);

        if (elementPalette != nullptr)
            elementPalette->setVisible (! leftPanelCollapsed);
        if (layersPanel != nullptr)
            layersPanel->setVisible (false);
        if (assetLibraryPanel != nullptr)
            assetLibraryPanel->setVisible (false);
        if (scriptEditor != nullptr)
        {
            scriptEditor->setVisible (true);
            scriptEditor->refresh();
        }
        if (inspectorPanel != nullptr)
        {
            inspectorPanel->refreshPscriptHelpers();
            inspectorPanel->refresh();
        }

        if (canvasToolbar != nullptr)
            canvasToolbar->syncSectionTabFromOwner();

        resized();
        repaint();
    }

    void StudioMainComponent::closePscriptPanel()
    {
        if (! showScriptEditorInsteadOfElements)
            return;

        showScriptEditorInsteadOfElements = false;

        if (leftTabs.getCurrentTabIndex() == 3)
            leftTabs.setCurrentTabIndex (0, juce::dontSendNotification);

        if (scriptEditor != nullptr)
            scriptEditor->setVisible (false);

        if (elementPalette != nullptr)
            elementPalette->setVisible (! leftPanelCollapsed);

        if (packRuntime != nullptr && ! customerPreviewActive && ! graphAudioListen)
        {
            packRuntime->setVisible (false);
            rehomePackRuntimeToStudio();
        }

        if (inspectorPanel != nullptr)
            inspectorPanel->refresh();

        if (canvasToolbar != nullptr)
            canvasToolbar->syncSectionTabFromOwner();

        resized();
        repaint();
    }

    bool StudioMainComponent::keyPressed (const juce::KeyPress& key)
    {
        if (customerPreviewActive && key == juce::KeyPress::escapeKey)
        {
            toggleCustomerPreview();
            return true;
        }
        return juce::Component::keyPressed (key);
    }

    void StudioMainComponent::createPscriptHandlerForSelectedControl()
    {
        // Gather every selected control that maps to a real parameter so a single
        // action can attach handlers to one OR many assets at once (bulk attach).
        struct Attachable { const LayoutElement* element; const ParameterDef* source; };
        std::vector<Attachable> attachables;

        for (const auto& id : selectedElementIds)
        {
            if (auto* element = project.getLayout().find (id);
                element != nullptr
                && isScriptableControlElement (*element)
                && element->parameterId.isNotEmpty())
            {
                if (auto* parameter = project.getParameters().find (element->parameterId))
                    attachables.push_back ({ element, parameter });
            }
        }

        if (attachables.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "pScript needs a mapped control",
                "Select one or more knobs, sliders, buttons, dropdowns, value displays, or sample drop zones assigned to real parameters, then run this action again.");
            return;
        }

        juce::String combined;
        for (const auto& a : attachables)
        {
            const auto* target = choosePscriptMacroTarget (project.getParameters(), a.source->id);
            if (combined.isNotEmpty())
                combined << "\n";
            combined << buildPscriptHandlerSnippet (*a.element, *a.source, target);
        }

        focusPscriptPanel();
        if (scriptEditor != nullptr)
            scriptEditor->insertSnippetAndCompile (combined);
    }

    bool StudioMainComponent::attachPscriptFileToElement (const juce::String& elementId, const juce::File& file)
    {
        juce::String error;
        if (! project.attachPscriptFileToElement (elementId, file, error))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Could not attach pScript",
                error);
            return false;
        }

        setSelectedElementId (elementId);
        focusPscriptPanel();
        if (scriptEditor != nullptr)
            scriptEditor->refresh();
        if (inspectorPanel != nullptr)
            inspectorPanel->refresh();

        if (auto* element = project.getLayout().find (elementId))
        {
            const auto label = element->label.isNotEmpty() ? element->label : element->parameterId;
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::InfoIcon,
                "pScript attached",
                "Attached \"" + file.getFileName() + "\" to " + label + ".\n"
                "Export the pack to share this behaviour with other PatchCraft users.");
        }
        return true;
    }

    bool StudioMainComponent::attachPscriptFileAt (const juce::File& file, juce::Point<int> canvasLocalPos)
    {
        if (canvasEditor == nullptr)
            return false;

        const auto* control = canvasEditor->scriptableControlAt (canvasLocalPos);
        if (control == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Drop pScript on a control",
                "Drop the .pscript file onto a knob, slider, button, dropdown, or other control that is assigned to a real parameter.");
            return false;
        }

        return attachPscriptFileToElement (control->id, file);
    }

    void StudioMainComponent::attachPscriptFileToSelectedControl()
    {
        if (selectedElementId.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Select a control",
                "Select a mapped knob, slider, button, or dropdown, then attach a pScript file.");
            return;
        }

        auto* element = project.getLayout().find (selectedElementId);
        if (element == nullptr || element->parameterId.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Control needs a parameter",
                "Assign this control to a parameter before attaching pScript.");
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser> (
            "Attach pScript file",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.pscript;*.psc;*.txt");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser, elementId = selectedElementId] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File())
                    return;
                attachPscriptFileToElement (elementId, file);
            });
    }

    void StudioMainComponent::detachPscriptFromSelectedControl()
    {
        if (selectedElementId.isEmpty())
            return;

        juce::String error;
        if (! project.detachPscriptFromElement (selectedElementId, error))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Could not detach pScript",
                error);
            return;
        }

        if (scriptEditor != nullptr)
            scriptEditor->refresh();
        if (inspectorPanel != nullptr)
            inspectorPanel->refresh();
    }

    namespace
    {
        static void copyDesignStyleFields (LayoutElement& target, const LayoutElement& source)
        {
            target.style = source.style;
            target.knobStyle = source.knobStyle;
            target.shapeKind = source.shapeKind;
            target.opacity = source.opacity;
            target.cornerRadius = source.cornerRadius;
            target.strokeWidth = source.strokeWidth;
            target.shadowAmount = source.shadowAmount;
            target.glowAmount = source.glowAmount;
            target.blurAmount = source.blurAmount;
            target.textColour = source.textColour;
            target.accentColour = source.accentColour;
            target.borderColour = source.borderColour;
            target.backgroundColour = source.backgroundColour;
            target.labelPosition = source.labelPosition;
            target.labelOffsetX = source.labelOffsetX;
            target.labelOffsetY = source.labelOffsetY;
            target.labelSpacing = source.labelSpacing;
            target.labelSize = source.labelSize;
            target.audioReactive = source.audioReactive;
            target.audioReactiveMode = source.audioReactiveMode;
            target.audioReactiveAmount = source.audioReactiveAmount;
            target.animationMode = source.animationMode;
            target.animationRate = source.animationRate;
        }

        static void applyBuiltInDesignStyle (LayoutElement& target, const juce::String& presetId)
        {
            if (presetId == "glass")
            {
                target.style = "Glass";
                target.opacity = 0.86f;
                target.cornerRadius = 18.0f;
                target.strokeWidth = 1.5f;
                target.shadowAmount = 0.28f;
                target.glowAmount = 0.18f;
                target.blurAmount = 0.20f;
                target.backgroundColour = juce::Colour (0x66242a34);
                target.borderColour = juce::Colour (0x99c6d4ff);
                target.accentColour = juce::Colour (0xff8fb6ff);
            }
            else if (presetId == "gold")
            {
                target.style = "Vintage Gold";
                target.opacity = 1.0f;
                target.cornerRadius = 14.0f;
                target.strokeWidth = 2.0f;
                target.shadowAmount = 0.34f;
                target.glowAmount = 0.20f;
                target.blurAmount = 0.0f;
                target.backgroundColour = juce::Colour (0xff18120a);
                target.borderColour = juce::Colour (0xffb9872d);
                target.accentColour = juce::Colour (0xfff5a623);
            }
            else if (presetId == "minimal")
            {
                target.style = "Minimal Flat";
                target.opacity = 1.0f;
                target.cornerRadius = 8.0f;
                target.strokeWidth = 1.0f;
                target.shadowAmount = 0.0f;
                target.glowAmount = 0.0f;
                target.blurAmount = 0.0f;
                target.backgroundColour = juce::Colour (0xff15171b);
                target.borderColour = juce::Colour (0xff30343b);
                target.accentColour = juce::Colour (0xffc8ccd4);
            }
            else if (presetId == "neon")
            {
                target.style = "Space Blue";
                target.opacity = 0.94f;
                target.cornerRadius = 16.0f;
                target.strokeWidth = 2.0f;
                target.shadowAmount = 0.18f;
                target.glowAmount = 0.62f;
                target.blurAmount = 0.10f;
                target.backgroundColour = juce::Colour (0xff07111e);
                target.borderColour = juce::Colour (0xff186fba);
                target.accentColour = juce::Colour (0xff2bd6ff);
                target.audioReactive = true;
                target.audioReactiveMode = "level";
                target.audioReactiveAmount = 0.35f;
                target.animationMode = "glow";
                target.animationRate = 0.45f;
            }
        }
    }

    void StudioMainComponent::copySelectedDesignStyle()
    {
        if (auto* el = project.getLayout().find (selectedElementId))
        {
            copiedDesignStyle = *el;
            hasCopiedDesignStyle = true;
            menuBar.repaint();
        }
    }

    void StudioMainComponent::pasteDesignStyle()
    {
        if (! hasCopiedDesignStyle || selectedElementIds.isEmpty())
            return;

        const auto ids = selectedElementIds;
        project.performLayoutEdit ("Paste design style", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    copyDesignStyleFields (*el, copiedDesignStyle);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::applyDesignStylePreset (const juce::String& presetId)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Apply design style", [&] (LayoutModel& m)
        {
            for (const auto& id : ids)
                if (auto* el = m.find (id); el != nullptr && ! el->locked)
                    applyBuiltInDesignStyle (*el, presetId);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::orderSelected (const juce::String& command)
    {
        const auto ids = selectedElementIds;
        if (ids.isEmpty()) return;

        project.performLayoutEdit ("Reorder selection", [&] (LayoutModel& m)
        {
            auto elementBounds = [] (const LayoutElement& e)
            {
                return juce::Rectangle<int> (e.x, e.y, e.width, e.height);
            };

            auto visuallyRelated = [&] (const LayoutElement& a, const LayoutElement& b)
            {
                if (a.id == b.id || ! a.visible || ! b.visible)
                    return false;
                if (a.containerId != b.containerId)
                    return false;
                if (a.groupId != b.groupId)
                    return false;
                return elementBounds (a).intersects (elementBounds (b));
            };

            auto moveBackwardBehindOverlap = [&] (const juce::String& id)
            {
                auto idx = m.findIndex (id);
                auto& all = m.getAll();
                if (idx <= 0 || idx >= (int) all.size())
                    return;

                int target = -1;
                for (int i = idx - 1; i >= 0; --i)
                    if (visuallyRelated (all[(size_t) idx], all[(size_t) i]))
                    {
                        target = i;
                        break;
                    }

                if (target < 0)
                    m.sendBackward (id);
                else
                    while (idx > target)
                    {
                        std::swap (all[(size_t) idx], all[(size_t) idx - 1]);
                        --idx;
                    }
            };

            auto moveForwardInFrontOfOverlap = [&] (const juce::String& id)
            {
                auto idx = m.findIndex (id);
                auto& all = m.getAll();
                if (idx < 0 || idx >= (int) all.size() - 1)
                    return;

                int target = -1;
                for (int i = idx + 1; i < (int) all.size(); ++i)
                    if (visuallyRelated (all[(size_t) idx], all[(size_t) i]))
                    {
                        target = i;
                        break;
                    }

                if (target < 0)
                    m.bringForward (id);
                else
                    while (idx < target)
                    {
                        std::swap (all[(size_t) idx], all[(size_t) idx + 1]);
                        ++idx;
                    }
            };

            if (command == "forward")
                for (const auto& id : ids) moveForwardInFrontOfOverlap (id);
            else if (command == "backward")
                for (int i = ids.size(); --i >= 0;) moveBackwardBehindOverlap (ids[i]);
            else if (command == "front")
                for (const auto& id : ids)
                    for (int guard = 0; guard < (int) m.getAll().size(); ++guard) m.bringForward (id);
            else if (command == "back")
                for (int i = ids.size(); --i >= 0;)
                    for (int guard = 0; guard < (int) m.getAll().size(); ++guard) m.sendBackward (ids[i]);
        });
        refreshAllPanels();
    }

    void StudioMainComponent::undo() { project.undo(); }
    void StudioMainComponent::redo() { project.redo(); }

    bool StudioMainComponent::isPanelFloating (juce::Component* panel) const
    {
        for (const auto& f : floatingPanels)
            if (f.panel == panel) return true;
        return false;
    }

    void StudioMainComponent::dockAllFloatingPanels()
    {
        for (auto& entry : floatingPanels)
        {
            if (entry.panel != nullptr && entry.originalParent != nullptr)
            {
                entry.originalParent->addAndMakeVisible (entry.panel);
                entry.originalParent->resized();
            }
            entry.window.reset();
        }

        floatingPanels.clear();
        resized();
        repaint();
        menuBar.repaint();
    }

    void StudioMainComponent::hideAllWindows()
    {
        dockAllFloatingPanels();
        leftPanelCollapsed = true;
        rightPanelCollapsed = true;
        leftCollapseButton.setButtonText (">");
        rightCollapseButton.setButtonText ("<");
        resized();
        repaint();
        menuBar.repaint();
    }

    void StudioMainComponent::showMainWindows()
    {
        leftPanelCollapsed = false;
        rightPanelCollapsed = false;
        leftCollapseButton.setButtonText ("<");
        rightCollapseButton.setButtonText (">");
        resized();
        repaint();
        menuBar.repaint();
    }

    void StudioMainComponent::togglePacksPanel()
    {
        togglePanelFloat (expansionLibraryPanel.get(), "Packs");
    }

    void StudioMainComponent::openSampleMapperZoneManager()
    {
        setBottomTab (BottomPanel::Page::Samples);
        if (bottomPanel != nullptr && ! isPanelFloating (bottomPanel.get()))
            bottomPanel->setSize (juce::jmax (1080, getWidth() - 120),
                                  juce::jmax (700, getHeight() - 120));
        togglePanelFloat (bottomPanel.get(), "Sample Mapper Zones");
    }

    void StudioMainComponent::togglePanelFloat (juce::Component* panel, juce::String title)
    {
        if (panel == nullptr) return;

        // Already floating? Bring focus + close — restoring it to the host.
        for (auto it = floatingPanels.begin(); it != floatingPanels.end(); ++it)
        {
            if (it->panel == panel)
            {
                auto* originalParent = it->originalParent;
                if (originalParent != nullptr)
                {
                    originalParent->addAndMakeVisible (panel);
                    originalParent->resized();
                }
                // Tear down the window after we've reparented.
                it->window.reset();
                floatingPanels.erase (it);
                resized();
                repaint();
                return;
            }
        }

        // Not floating: stash original parent, open a window with the panel
        // as content. Closing the window re-docks via the lambda above.
        panel->setVisible (true);
        auto* originalParent = panel->getParentComponent();
        FloatingEntry entry;
        entry.panel = panel;
        entry.originalParent = originalParent;
        entry.window = std::make_unique<FloatingPanelWindow> (
            title, panel,
            [this, panel]
            {
                // Defer so we never destroy the window from inside its own
                // closeButtonPressed callback.
                juce::MessageManager::callAsync ([this, panel]
                {
                    togglePanelFloat (panel, juce::String());
                });
            });
        floatingPanels.push_back (std::move (entry));

        // Host now has a hole where the panel was — re-flow.
        if (originalParent != nullptr)
            originalParent->resized();
        panel->setVisible (true);
        panel->resized();
        resized();
        repaint();
    }

    void StudioMainComponent::toggleCustomerPreview()
    {
        if (packRuntime == nullptr || customerPreviewOverlay == nullptr)
            return;

        if (customerPreviewActive)
        {
            exitCustomerPreviewIfActive();
            if (bottomPanel != nullptr)
                bottomPanel->refresh();
            resized();
            refreshPreviewUiState();
            repaint();
            return;
        }

        closePscriptPanel();

        if (bottomPanel != nullptr)
            bottomPanel->setPreviewActive (false);

        graphAudioListen = false;
        customerPreviewActive = true;
        customerPreviewOverlay->enterPreview();
        refreshCanvasToolbar();
        resized();
        refreshPreviewUiState();
        repaint();
    }

    void StudioMainComponent::attachPackRuntimePreview (juce::Component* parent, juce::Rectangle<int> area)
    {
        if (packRuntime == nullptr || customerPreviewActive || parent == nullptr)
            return;

        if (area.isEmpty())
            return;

        packRuntime->setPlayerChromeVisible (true);
        packRuntime->attachToParent (parent, area);
        packRuntime->setVisible (true);
        packRuntime->toFront (false);

        // Layout passes (including maximize) must not sync-rebuild the pack.
        // Soft-activate on the next message tick if audio isn't running yet.
        if (bottomTab == BottomPanel::Page::Branding && ! packRuntime->isAudioRunning())
        {
            juce::Component::SafePointer<PackRuntimeHost> runtime (packRuntime.get());
            juce::MessageManager::callAsync ([runtime]
            {
                if (runtime != nullptr)
                    runtime->activate();
            });
        }
    }

    void StudioMainComponent::togglePreview()
    {
        if (bottomTab == BottomPanel::Page::DSP)
        {
            setGraphAudioListen (! graphAudioListen);
            return;
        }

        toggleCustomerPreview();
    }

    void StudioMainComponent::beginMidiLearn (juce::String parameterId)
    {
        auto* def = project.getParameters().find (parameterId);
        if (def == nullptr || ! def->midiLearnable)
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("MIDI Learn")
                    .withMessage ("Choose a MIDI-learnable parameter first.")
                    .withButton ("OK")
                    .withIconType (juce::MessageBoxIconType::WarningIcon),
                nullptr);
            return;
        }

        pendingMidiLearnParameter = parameterId;
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("MIDI Learn")
                .withMessage ("Move a hardware knob, wheel, aftertouch, or expression control to map it to:\n"
                              + def->name + " (" + def->id + ")")
                .withButton ("OK")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            nullptr);
    }

    bool StudioMainComponent::captureMidiLearnMessage (const juce::MidiMessage& message)
    {
        if (pendingMidiLearnParameter.isEmpty())
            return false;

        auto* def = project.getParameters().find (pendingMidiLearnParameter);
        if (def == nullptr || ! def->midiLearnable)
        {
            pendingMidiLearnParameter.clear();
            return false;
        }

        MidiMapping mapping;
        mapping.id = "midi_" + def->id;
        mapping.parameterId = def->id;
        mapping.channel = message.getChannel();
        mapping.targetMin = def->min;
        mapping.targetMax = def->max;

        if (message.isController())
        {
            mapping.sourceType = "cc";
            mapping.controller = message.getControllerNumber();
        }
        else if (message.isPitchWheel())
        {
            mapping.sourceType = "pitchWheel";
            mapping.bipolar = true;
        }
        else if (message.isAftertouch())
        {
            mapping.sourceType = "aftertouch";
        }
        else if (message.isChannelPressure())
        {
            mapping.sourceType = "channelPressure";
        }
        else
        {
            return false;
        }

        auto& mappings = project.getMidiMappings();
        mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
            [&] (const MidiMapping& existing)
            {
                return existing.parameterId == mapping.parameterId
                    || (existing.sourceType == mapping.sourceType
                        && existing.channel == mapping.channel
                        && existing.controller == mapping.controller);
            }), mappings.end());
        mappings.push_back (mapping);
        pendingMidiLearnParameter.clear();
        project.notifyChanged();
        refreshAllPanels();

        juce::String sourceLabel = mapping.sourceType;
        if (mapping.sourceType == "cc")
            sourceLabel += " " + juce::String (mapping.controller);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle ("MIDI Learn")
                .withMessage ("Mapped " + sourceLabel + " on channel "
                              + juce::String (mapping.channel) + " to "
                              + def->name + " (" + def->id + ").")
                .withButton ("OK")
                .withIconType (juce::MessageBoxIconType::InfoIcon),
            nullptr);
        return true;
    }

    void StudioMainComponent::exportPack()
    {
        if (showLaunchReadinessIfBlocked (*this, project, "Export Pack"))
            return;

        if (showSampleExportValidationIfBlocked (*this, project, "Export Pack"))
            return;

        sanitiseLayoutParameterReferences (project);

        const int repairedPresetValues = removeStalePresetValuesForExport (project);

        auto chooser = std::make_shared<juce::FileChooser> (
            "Export PatchCraft pack",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (project.getManifest().instrumentName + ".patchcraft"),
            "");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser, repairedPresetValues] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File()) return;

                PatchCraftPackWriter writer;
                juce::String err;
                const auto warnings = validationWarningSummary (project);
                const auto repairNote = repairedPresetValues > 0
                    ? ("\n\nCleaned up " + juce::String (repairedPresetValues)
                       + " stale preset value" + (repairedPresetValues == 1 ? "" : "s")
                       + " that referenced removed parameters.")
                    : juce::String();
                if (! writer.write (project, folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Export Pack")
                            .withMessage ("Export failed: " + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                    return;
                }

                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle (warnings.isNotEmpty() ? "Export Pack Complete - Review Warnings"
                                                          : "Export Pack Complete")
                        .withMessage ("Exported successfully to:\n" + folder.getFullPathName() + repairNote + warnings)
                        .withButton ("OK")
                        .withIconType (warnings.isNotEmpty() ? juce::MessageBoxIconType::WarningIcon
                                                             : juce::MessageBoxIconType::InfoIcon),
                    nullptr);
            });
    }

    void StudioMainComponent::addArpBlock()
    {
        addMotionBlock (SoundStack::MotionKind::Arp);
    }

    void StudioMainComponent::addMotionBlock (SoundStack::MotionKind kind)
    {
        if (bottomPanel)
            bottomPanel->addMotionBlock (kind);
        setBottomTab (BottomPanel::Page::DSP);
        if (canvasToolbar)
            canvasToolbar->syncSectionTabFromOwner();
    }

    bool StudioMainComponent::isAdvancedGraphMode() const
    {
        return advancedGraphModeExplicit
            || SoundStack::usesAdvancedGraphFeatures (project.getDspGraph());
    }

    void StudioMainComponent::setAdvancedGraphMode (bool enabled)
    {
        advancedGraphModeExplicit = enabled;
        if (bottomPanel != nullptr)
            bottomPanel->refresh();
    }

    void StudioMainComponent::exportVstPlugin()
    {
        if (showLaunchReadinessIfBlocked (*this, project, "Export VST3 Plugin"))
            return;

        // Sample-health gating only applies to sampler projects. Synth and
        // FX engines export fine without any zones - their sound comes
        // from the DSP graph, not from samples - so we run the validator
        // only when the project's engine is the sampler.
        if (project.getEngineType() == "sample"
            && showSampleExportValidationIfBlocked (*this, project, "Export VST3 Plugin"))
            return;

        VstExportModule::showExportDialog (this, project);
    }

    void StudioMainComponent::publishToPluginClub()
    {
        if (showLaunchReadinessIfBlocked (*this, project, "Publish Draft to Plugin.club",
                                          LaunchReadiness::Scope::Publish))
            return;

        if (showSampleExportValidationIfBlocked (*this, project, "Publish Draft to Plugin.club"))
            return;

        const auto config = AiAssistService::loadCloudIntegrationConfig();
        const auto endpoint = PluginClubPublisher::normaliseSellerImportEndpoint (
            config.pluginClubEndpoint.trim().isNotEmpty()
                ? config.pluginClubEndpoint
                : juce::String ("https://plugin.club/functions/v1/sellerImport"));
        const bool hasApiKey = PluginClubPublisher::optionsFromCloudConfig (config).apiKey.isNotEmpty();
        const bool canPushOnline = endpoint.isNotEmpty() && hasApiKey;
        const bool vstExpansionInstalled = VstExportModule::isVstExpansionInstalled();

        auto* window = new juce::AlertWindow ("Publish Draft to Plugin.club",
            "Choose the artifact to stage. If the seller API key is configured, PatchCraft will also push it as a Plugin.club draft.",
            juce::MessageBoxIconType::NoIcon);
        window->addTextBlock ("Endpoint: " + (endpoint.isNotEmpty() ? endpoint : juce::String ("not configured"))
            + "\nSeller API key: " + (hasApiKey ? "ready" : "missing - package will be prepared locally")
            + "\nLicense: " + (project.getManifest().licenseRequired ? "required" : "not required")
            + "\nMode: " + (canPushOnline ? "stage + publish draft" : "stage local package only")
            + "\nVST Expansion: " + (vstExpansionInstalled ? "installed" : "not installed - VST3 publishing locked"));

        juce::StringArray artifactChoices { "PatchCraft Instrument Pack" };
        if (vstExpansionInstalled)
            artifactChoices.addArray ({ "Standalone VST3 Plugin", "Pack + Standalone VST3" });
        window->addComboBox ("artifact", artifactChoices,
                             "Artifact:");
        if (auto* box = window->getComboBoxComponent ("artifact"))
            box->setSelectedItemIndex (0, juce::dontSendNotification);
        window->addButton (canPushOnline ? "Publish Draft" : "Prepare Package", 1);
        window->addButton ("Settings", 2);
        window->addButton ("Cancel", 0);
        window->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, window] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (window);
                if (result == 2)
                {
                    openSettings();
                    return;
                }
                if (result != 1)
                    return;

                int choice = 1;
                if (auto* box = owned->getComboBoxComponent ("artifact"))
                    choice = box->getSelectedItemIndex() + 1;
                publishToPluginClubWithArtifactChoice (choice);
            }), true);
    }

    void StudioMainComponent::publishToPluginClubWithArtifactChoice (int artifactChoice)
    {
        if (artifactChoice != 1 && ! VstExportModule::isVstExpansionInstalled())
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withTitle ("VST Expansion Required")
                    .withMessage (VstExportModule::vstExpansionInstallMessage())
                    .withIconType (juce::MessageBoxIconType::InfoIcon)
                    .withButton ("OK"),
                nullptr);
            return;
        }

        const auto config = AiAssistService::loadCloudIntegrationConfig();
        auto options = PluginClubPublisher::optionsFromCloudConfig (config);

        if (project.getManifest().licenseRequired || config.licenseEndpoint.isNotEmpty())
        {
            auto& manifest = project.getManifest();
            if (manifest.licenseServerUrl.isEmpty())
                manifest.licenseServerUrl = config.licenseEndpoint;
            if (manifest.licensePublicKey.isEmpty())
                manifest.licensePublicKey = config.licensePublicKey;
            if (manifest.licenseProductId.isEmpty())
                manifest.licenseProductId = LicenseValidator::hashInstrumentId (manifest.instrumentName,
                                                                                manifest.creator);
            project.markDirty();
        }

        juce::Component::SafePointer<StudioMainComponent> safeThis (this);
        std::thread ([safeThis, options, artifactChoice]
        {
            if (safeThis == nullptr)
                return;

            std::vector<PluginClubPublisher::PublishResult> results;

            auto publishPack = [&]
            {
                results.push_back (PluginClubPublisher::publishDraft (safeThis->project, options));
            };

            auto publishVst3 = [&]
            {
                const auto& manifest = safeThis->project.getManifest();
                const auto pluginName = manifest.instrumentName.trim().isNotEmpty()
                    ? manifest.instrumentName.trim()
                    : juce::String ("PatchCraft Plugin");

                VstExportModule::ExportOptions exportOptions;
                exportOptions.pluginName = pluginName;
                exportOptions.fileSafeName = safePublishFileStem (pluginName);
                exportOptions.manufacturerName = manifest.creator.trim().isNotEmpty()
                    ? manifest.creator.trim()
                    : juce::String ("PatchCraft");
                exportOptions.version = manifest.version.trim().isNotEmpty()
                    ? manifest.version.trim()
                    : juce::String ("1.0.0");
                exportOptions.outputFolder = options.stagingRoot
                    .getChildFile ("VST3Exports")
                    .getChildFile (exportOptions.fileSafeName + "-" + juce::String (juce::Time::getCurrentTime().toMilliseconds()));
                exportOptions.installToSystemVst3 = false;

                const auto exportResult = VstExportModule::exportPlugin (safeThis->project, exportOptions);
                if (! exportResult.success)
                {
                    PluginClubPublisher::PublishResult failed;
                    failed.artifactKind = PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin;
                    failed.message = "VST3 export failed before Plugin.club upload:\n" + exportResult.message;
                    results.push_back (failed);
                    return;
                }

                PluginClubPublisher::PublishArtifact artifact;
                artifact.kind = PluginClubPublisher::ArtifactKind::StandaloneVst3Plugin;
                artifact.title = pluginName;
                artifact.description = manifest.description.trim().isNotEmpty()
                    ? manifest.description.trim()
                    : juce::String ("Standalone VST3 plugin exported from PatchCraft Studio.");
                artifact.creator = exportOptions.manufacturerName;
                artifact.category = manifest.category;
                artifact.version = exportOptions.version;
                artifact.status = "draft";
                artifact.tags = manifest.tags;
                artifact.tags.addIfNotAlreadyThere ("VST3");
                artifact.tags.addIfNotAlreadyThere ("PatchCraft");
                artifact.formats.add ("VST3");
               #if JUCE_WINDOWS
                artifact.operatingSystems.add ("Windows");
               #elif JUCE_MAC
                artifact.operatingSystems.add ("macOS");
               #endif
                artifact.sourcePath = exportResult.bundlePath;

                auto* extra = new juce::DynamicObject();
                extra->setProperty ("embeddedPack", true);
                extra->setProperty ("sourceProject", manifest.instrumentName);
                extra->setProperty ("bundlePath", exportResult.bundlePath.getFullPathName());
                artifact.extraMetadata = juce::var (extra);
                results.push_back (PluginClubPublisher::publishArtifact (artifact, options));
            };

            if (artifactChoice == 1)
                publishPack();
            else if (artifactChoice == 2)
                publishVst3();
            else
            {
                publishPack();
                publishVst3();
            }

            juce::MessageManager::callAsync ([safeThis, results, artifactChoice]
            {
                if (safeThis == nullptr)
                    return;

                bool anyUploaded = false;
                bool allSucceeded = ! results.empty();
                juce::String message = "Artifact: " + publishChoiceLabel (artifactChoice) + "\n";
                juce::String firstEditUrl;
                for (const auto& result : results)
                {
                    anyUploaded = anyUploaded || result.uploaded;
                    allSucceeded = allSucceeded && result.success;
                    if (firstEditUrl.isEmpty() && result.editUrl.isNotEmpty())
                        firstEditUrl = result.editUrl;
                    message += "\n[" + PluginClubPublisher::artifactKindToString (result.artifactKind) + "]\n"
                             + result.message
                             + (result.endpointUsed.isNotEmpty() ? "\nEndpoint: " + result.endpointUsed : juce::String())
                             + "\nArchive: " + result.archiveFile.getFullPathName()
                             + "\nPayload: " + result.payloadFile.getFullPathName()
                             + "\n";
                }

                const auto title = allSucceeded
                    ? (anyUploaded ? "Plugin.club Draft Published" : "Plugin.club Package Prepared")
                    : "Plugin.club Publish Failed";

                auto options = juce::MessageBoxOptions()
                    .withTitle (title)
                    .withMessage (message)
                    .withIconType (allSucceeded ? juce::MessageBoxIconType::InfoIcon
                                                : juce::MessageBoxIconType::WarningIcon);

                if (firstEditUrl.isNotEmpty())
                    options = options.withButton ("Open Draft")
                                     .withButton ("Copy Report")
                                     .withButton ("OK");
                else
                    options = options.withButton ("Copy Report")
                                     .withButton ("OK");

                juce::AlertWindow::showAsync (options,
                    [firstEditUrl, message] (int result)
                    {
                        if (firstEditUrl.isNotEmpty())
                        {
                            if (result == 1)
                                juce::URL (firstEditUrl).launchInDefaultBrowser();
                            else if (result == 2)
                                juce::SystemClipboard::copyTextToClipboard (message);
                        }
                        else if (result == 1)
                        {
                            juce::SystemClipboard::copyTextToClipboard (message);
                        }
                    });
            });
        }).detach();
    }

    void StudioMainComponent::restoreAllPresets()
    {
        project.resetLiveValuesToDefaults();
        project.notifyChanged();
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Restore Current Sound",
            "The current sound has been restored to the parameter defaults.",
            "OK",
            nullptr, nullptr);
    }

    void StudioMainComponent::setDefaultPreset()
    {
        auto patchName = project.getManifest().defaultPreset.trim();
        if (patchName.isEmpty())
            patchName = project.getManifest().instrumentName.trim().isNotEmpty()
                ? project.getManifest().instrumentName.trim() + " Default"
                : juce::String ("Default Preset");

        auto patch = project.captureCurrentPatch (patchName);
        patch.isDefault = true;

        int patchIndex = -1;
        auto& patches = project.getPatches();
        for (int i = 0; i < (int) patches.size(); ++i)
        {
            if (patches[(size_t) i].isDefault
                || patches[(size_t) i].id == patch.id
                || patches[(size_t) i].name == patch.name)
            {
                patchIndex = i;
                break;
            }
        }
        if (patchIndex >= 0)
            patches[(size_t) patchIndex] = patch;
        else
        {
            patches.push_back (patch);
            patchIndex = (int) patches.size() - 1;
        }
        for (int i = 0; i < (int) patches.size(); ++i)
            patches[(size_t) i].isDefault = i == patchIndex;

        auto preset = patch.toPreset();
        preset.isDefault = true;

        int presetIndex = -1;
        auto& presets = project.getPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
        {
            if (presets[(size_t) i].isDefault
                || presets[(size_t) i].patchId == patch.id
                || presets[(size_t) i].name == preset.name)
            {
                presetIndex = i;
                break;
            }
        }
        if (presetIndex >= 0)
            presets[(size_t) presetIndex] = preset;
        else
        {
            presets.push_back (preset);
            presetIndex = (int) presets.size() - 1;
        }
        for (int i = 0; i < (int) presets.size(); ++i)
            presets[(size_t) i].isDefault = i == presetIndex;

        project.getManifest().defaultPreset = preset.name;
        project.notifyChanged();
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Set Default Preset",
            "Current sound has been captured as the default preset and will be restored when this project is reopened or exported.",
            "OK",
            nullptr, nullptr);
    }

    } // namespace patchcraft
