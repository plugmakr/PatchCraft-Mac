#include "AiAssistService.h"

#include "SampleMap.h"

#include <map>
#include <memory>

namespace patchcraft
{
    namespace
    {
        static juce::String plural (int count, const juce::String& singular, const juce::String& pluralText)
        {
            return juce::String (count) + " " + (count == 1 ? singular : pluralText);
        }

        static juce::String firstLines (const juce::StringArray& lines, int maxLines)
        {
            juce::StringArray clipped;
            for (int i = 0; i < lines.size() && i < maxLines; ++i)
                clipped.add (lines[i]);
            return clipped.joinIntoString ("\n");
        }

        static juce::String typeCountsText (const std::map<juce::String, int>& counts)
        {
            juce::StringArray parts;
            for (const auto& item : counts)
                parts.add (item.first + " " + juce::String (item.second));
            return parts.isEmpty() ? juce::String ("none") : parts.joinIntoString (", ");
        }

        static juce::String parameterFamilies (const ParameterModel& parameters)
        {
            std::map<juce::String, int> bySection;
            int host = 0;
            int midi = 0;
            int hidden = 0;
            for (const auto& def : parameters.getAll())
            {
                ++bySection[def.section.isNotEmpty() ? def.section : juce::String ("global")];
                if (def.hostAutomatable) ++host;
                if (def.midiLearnable) ++midi;
                if (! def.visible) ++hidden;
            }

            return plural ((int) parameters.getAll().size(), "parameter", "parameters")
                + ", " + plural (host, "host slot candidate", "host slot candidates")
                + ", " + plural (midi, "MIDI-learnable target", "MIDI-learnable targets")
                + ", " + plural (hidden, "hidden/internal target", "hidden/internal targets")
                + "\nSections: " + typeCountsText (bySection);
        }

        static juce::String displayTaskGoal (AiAssistService::TaskType task)
        {
            switch (task)
            {
                case AiAssistService::TaskType::BackgroundPrompt:           return "Create production-ready art direction.";
                case AiAssistService::TaskType::SuggestLayout:              return "Improve the instrument layout and interaction hierarchy.";
                case AiAssistService::TaskType::SuggestControls:            return "Recommend visible controls mapped to real parameters.";
                case AiAssistService::TaskType::SuggestMacroAssignments:    return "Plan expressive macro controls.";
                case AiAssistService::TaskType::GeneratePresetNames:        return "Generate sellable preset names and tags.";
                case AiAssistService::TaskType::GenerateProductDescription: return "Draft product copy for the instrument/expansion.";
                case AiAssistService::TaskType::DesignCritique:             return "Audit UI structure, readability, and runtime parity.";
                case AiAssistService::TaskType::SoundRecipe:                return "Create a sound-design recipe from the current graph.";
                case AiAssistService::TaskType::WavetableRecipe:            return "Design wavetable movement and modulation ideas.";
                case AiAssistService::TaskType::EqChain:                    return "Plan advanced EQ bands and dynamic EQ moves.";
                case AiAssistService::TaskType::ModulationPlan:             return "Plan LFO, macro, automation, and performance modulation.";
                case AiAssistService::TaskType::BuildAssetGuidance:         return "Guide knob/slider/meter asset building.";
                case AiAssistService::TaskType::ExportChecklist:            return "Check sellable pack readiness.";
            }
            return {};
        }

        static juce::String taskPromptName (AiAssistService::TaskType task)
        {
            return AiAssistService::displayName (task).replace (":", " -");
        }

        static juce::String buildLocalLlmPrompt (AiAssistService::TaskType task,
                                                 const AiAssistService::ProjectContextPack& context,
                                                 const juce::String& builtInDraft,
                                                 bool includeContext)
        {
            juce::String prompt;
            prompt << "Action: " << taskPromptName (task) << "\n\n";
            prompt << "PatchCraft is a premium instrument/effect authoring system. "
                   << "Return practical, concise, product-quality guidance for a developer building sellable instruments. "
                   << "Do not claim you applied changes. Do not ask for API keys. Keep output preview-first.\n\n";
            if (includeContext)
                prompt << "Project context:\n" << context.toSummaryText() << "\n\n";
            prompt << "Baseline local guidance to improve or specialize:\n" << builtInDraft << "\n\n";
            prompt << "Output format:\n"
                   << "- Start with a 1-line recommendation.\n"
                   << "- Then provide 4-8 concrete bullets.\n"
                   << "- Include any validation or runtime caveats.\n"
                   << "- If this is a sound-design task, mention specific PatchCraft parameters/modules where useful.\n";
            return prompt;
        }

        static juce::String extractChatContent (const juce::var& parsed)
        {
            if (auto* object = parsed.getDynamicObject())
            {
                if (auto* choices = object->getProperty ("choices").getArray())
                {
                    if (choices->isEmpty())
                        return {};

                    if (auto* choice = choices->getReference (0).getDynamicObject())
                    {
                        if (auto* message = choice->getProperty ("message").getDynamicObject())
                            return message->getProperty ("content").toString();

                        if (choice->hasProperty ("text"))
                            return choice->getProperty ("text").toString();
                    }
                }

                if (object->hasProperty ("content"))
                    return object->getProperty ("content").toString();
            }

            return {};
        }

        static bool runLocalLlamaRequest (const AiAssistService::LocalLlmConfig& config,
                                          AiAssistService::TaskType task,
                                          const AiAssistService::ProjectContextPack& context,
                                          const juce::String& builtInDraft,
                                          juce::String& output,
                                          juce::String& error)
        {
            if (config.endpoint.trim().isEmpty())
            {
                error = "Local AI endpoint is empty.";
                return false;
            }

            auto* root = new juce::DynamicObject();
            root->setProperty ("model", config.model.trim().isNotEmpty() ? config.model.trim()
                                                                          : juce::String ("local-model"));
            root->setProperty ("temperature", config.temperature);
            root->setProperty ("max_tokens", config.maxTokens);
            root->setProperty ("stream", false);

            juce::Array<juce::var> messages;
            {
                auto* system = new juce::DynamicObject();
                system->setProperty ("role", "system");
                system->setProperty ("content",
                    "You are PatchCraft Copilot: a focused local assistant for music software developers, sound designers, and instrument builders. "
                    "You know synthesizers, sample instruments, effects, EQ, wavetables, modulation, MIDI, preset packs, and plugin UX. "
                    "Be specific, avoid fluff, and never say you changed the project unless an explicit apply action is provided.");
                messages.add (juce::var (system));
            }
            {
                auto* user = new juce::DynamicObject();
                user->setProperty ("role", "user");
                user->setProperty ("content", buildLocalLlmPrompt (task, context, builtInDraft, config.includeProjectContext));
                messages.add (juce::var (user));
            }
            root->setProperty ("messages", messages);

            const auto body = juce::JSON::toString (juce::var (root), false);
            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto stream = juce::URL (config.endpoint)
                .withPOSTData (body)
                .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (juce::jlimit (1000, 30000, config.timeoutMs))
                    .withExtraHeaders ("Content-Type: application/json\r\n")
                    .withResponseHeaders (&responseHeaders)
                    .withStatusCode (&statusCode)
                    .withHttpRequestCmd ("POST")
                    .withNumRedirectsToFollow (2));

            if (stream == nullptr)
            {
                error = "Could not connect to local llama.cpp server at " + config.endpoint + ".";
                return false;
            }

            const auto response = stream->readEntireStreamAsString();
            if (statusCode < 200 || statusCode >= 300)
            {
                error = "Local AI server returned HTTP " + juce::String (statusCode)
                    + (response.isNotEmpty() ? (": " + response.substring (0, 240)) : juce::String());
                return false;
            }

            auto parsed = juce::JSON::parse (response);
            output = extractChatContent (parsed).trim();
            if (output.isEmpty())
            {
                error = "Local AI response did not contain assistant text.";
                return false;
            }

            return true;
        }
    }

    juce::String AiAssistService::displayName (TaskType t)
    {
        switch (t)
        {
            case TaskType::BackgroundPrompt:           return "Generate Background Prompt";
            case TaskType::SuggestLayout:              return "Suggest Layout";
            case TaskType::SuggestControls:            return "Suggest Controls";
            case TaskType::SuggestMacroAssignments:    return "Suggest Macro Assignments";
            case TaskType::GeneratePresetNames:        return "Generate Preset Names";
            case TaskType::GenerateProductDescription: return "Generate Product Description";
            case TaskType::DesignCritique:             return "Design Copilot: Critique";
            case TaskType::SoundRecipe:                return "Sound Copilot: Recipe";
            case TaskType::WavetableRecipe:            return "Sound Copilot: Wavetable";
            case TaskType::EqChain:                    return "Sound Copilot: EQ Chain";
            case TaskType::ModulationPlan:             return "Sound Copilot: Modulation";
            case TaskType::BuildAssetGuidance:         return "Build Copilot: Assets";
            case TaskType::ExportChecklist:            return "Export Copilot: Checklist";
        }
        return "AI Assist";
    }

    std::vector<AiAssistService::TaskType> AiAssistService::defaultTasks()
    {
        return {
            TaskType::DesignCritique,
            TaskType::SuggestLayout,
            TaskType::SuggestControls,
            TaskType::SoundRecipe,
            TaskType::WavetableRecipe,
            TaskType::EqChain,
            TaskType::ModulationPlan,
            TaskType::SuggestMacroAssignments,
            TaskType::GeneratePresetNames,
            TaskType::GenerateProductDescription,
            TaskType::BuildAssetGuidance,
            TaskType::ExportChecklist,
            TaskType::BackgroundPrompt
        };
    }

    juce::String AiAssistService::providerModeToString (ProviderMode provider)
    {
        switch (provider)
        {
            case ProviderMode::BuiltInTemplates: return "builtInTemplates";
            case ProviderMode::LocalLlamaServer: return "localLlamaServer";
        }
        return "builtInTemplates";
    }

    AiAssistService::ProviderMode AiAssistService::providerModeFromString (const juce::String& value)
    {
        if (value == "localLlamaServer")
            return ProviderMode::LocalLlamaServer;
        return ProviderMode::BuiltInTemplates;
    }

    juce::var AiAssistService::LocalLlmConfig::toVar() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("provider", providerModeToString (provider));
        object->setProperty ("endpoint", endpoint);
        object->setProperty ("model", model);
        object->setProperty ("timeoutMs", timeoutMs);
        object->setProperty ("maxTokens", maxTokens);
        object->setProperty ("temperature", temperature);
        object->setProperty ("includeProjectContext", includeProjectContext);
        return juce::var (object);
    }

    AiAssistService::LocalLlmConfig AiAssistService::LocalLlmConfig::fromVar (const juce::var& value)
    {
        LocalLlmConfig config;
        if (auto* object = value.getDynamicObject())
        {
            config.provider = providerModeFromString (object->getProperty ("provider").toString());
            if (object->hasProperty ("endpoint")) config.endpoint = object->getProperty ("endpoint").toString();
            if (object->hasProperty ("model")) config.model = object->getProperty ("model").toString();
            if (object->hasProperty ("timeoutMs")) config.timeoutMs = juce::jlimit (1000, 30000, (int) object->getProperty ("timeoutMs"));
            if (object->hasProperty ("maxTokens")) config.maxTokens = juce::jlimit (128, 4096, (int) object->getProperty ("maxTokens"));
            if (object->hasProperty ("temperature")) config.temperature = juce::jlimit (0.0f, 1.5f, (float) object->getProperty ("temperature"));
            if (object->hasProperty ("includeProjectContext")) config.includeProjectContext = (bool) object->getProperty ("includeProjectContext");
        }
        return config;
    }

    juce::File AiAssistService::localLlmConfigFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("ai-copilot.json");
    }

    AiAssistService::LocalLlmConfig AiAssistService::loadLocalLlmConfig()
    {
        const auto file = localLlmConfigFile();
        if (! file.existsAsFile())
            return {};

        return LocalLlmConfig::fromVar (juce::JSON::parse (file));
    }

    void AiAssistService::saveLocalLlmConfig (const LocalLlmConfig& config)
    {
        auto file = localLlmConfigFile();
        if (! file.getParentDirectory().createDirectory())
            return;
        file.replaceWithText (juce::JSON::toString (config.toVar(), true));
    }

    juce::String AiAssistService::providerStatusText()
    {
        const auto config = loadLocalLlmConfig();
        if (config.provider == ProviderMode::LocalLlamaServer)
            return "Local AI: llama.cpp server -> " + config.model + " @ " + config.endpoint;
        return "Local AI: built-in templates only. Enable llama.cpp in Settings for model-backed suggestions.";
    }

    juce::String AiAssistService::ProjectContextPack::toSummaryText() const
    {
        juce::StringArray lines;
        lines.add ("Instrument: " + (instrumentName.isNotEmpty() ? instrumentName : juce::String ("Untitled Instrument")));
        lines.add ("Creator: " + (creator.isNotEmpty() ? creator : juce::String ("PatchCraft User")));
        lines.add ("Engine: " + engine + " | Category: " + category);
        lines.add ("Canvas: " + canvasSummary);
        lines.add ("Layout: " + layoutSummary);
        lines.add ("Parameters: " + parameterSummary);
        lines.add ("DSP: " + dspSummary);
        lines.add ("Samples: " + sampleSummary);
        lines.add ("Content: " + contentSummary);
        if (validationSummary.isNotEmpty())
            lines.add ("Validation:\n" + validationSummary);
        return lines.joinIntoString ("\n");
    }

    AiAssistService::ProjectContextPack AiAssistService::buildContextPack (const PatchCraftProject& project)
    {
        ProjectContextPack context;
        const auto& manifest = project.getManifest();
        context.instrumentName = manifest.instrumentName;
        context.creator = manifest.creator;
        context.engine = project.getEngineType();
        context.category = manifest.category;
        context.description = manifest.description;
        context.canvasSummary = juce::String (project.getCanvasSize().width) + " x "
            + juce::String (project.getCanvasSize().height);

        std::map<juce::String, int> layoutTypes;
        int visible = 0;
        int mappedControls = 0;
        int animated = 0;
        int audioReactive = 0;
        for (const auto& element : project.getLayout().getAll())
        {
            ++layoutTypes[elementTypeDisplayName (element.type)];
            if (element.visible) ++visible;
            if (element.parameterId.isNotEmpty()) ++mappedControls;
            if (element.animationMode.isNotEmpty() && element.animationMode != "none") ++animated;
            if (element.audioReactive) ++audioReactive;
        }

        context.layoutSummary = plural ((int) project.getLayout().getAll().size(), "element", "elements")
            + ", " + plural (visible, "visible", "visible")
            + ", " + plural (mappedControls, "mapped control", "mapped controls")
            + ", " + plural (animated, "animated element", "animated elements")
            + ", " + plural (audioReactive, "audio-reactive element", "audio-reactive elements")
            + "\nTypes: " + typeCountsText (layoutTypes);

        context.parameterSummary = parameterFamilies (project.getParameters());

        const auto& graph = project.getDspGraph();
        std::map<juce::String, int> blockSections;
        std::map<juce::String, int> blockTypes;
        for (const auto& block : graph.blocks)
        {
            ++blockSections[block.section.isNotEmpty() ? block.section : juce::String ("unknown")];
            ++blockTypes[block.type.isNotEmpty() ? block.type : juce::String ("unknown")];
        }

        context.dspSummary = plural ((int) graph.blocks.size(), "block", "blocks")
            + ", " + plural ((int) graph.edges.size(), "edge", "edges")
            + ", " + plural ((int) graph.macros.size(), "macro", "macros")
            + ", " + plural ((int) graph.modulation.size(), "mod route", "mod routes")
            + ", " + plural ((int) graph.automation.size(), "automation lane", "automation lanes")
            + "\nSections: " + typeCountsText (blockSections)
            + "\nTypes: " + typeCountsText (blockTypes);

        const auto health = SampleMap::evaluateHealth (project.getSampleMap(),
                                                       project.getProjectFolder(),
                                                       project.getEngineType());
        context.sampleSummary = plural (health.totalZones, "zone", "zones")
            + ", " + plural (health.playableZones, "playable", "playable")
            + ", " + plural (health.missingFiles, "missing file", "missing files")
            + ", coverage "
            + (health.coveredNotes > 0 ? (juce::String (health.firstCoveredNote) + "-" + juce::String (health.lastCoveredNote))
                                       : juce::String ("none"));

        context.contentSummary = plural ((int) project.getPresets().size(), "preset", "presets")
            + ", " + plural ((int) project.getPatches().size(), "patch", "patches")
            + ", " + plural ((int) project.getSectionPresets().size(), "section preset", "section presets")
            + ", " + plural ((int) project.getExpansions().size(), "expansion", "expansions");

        juce::StringArray validation;
        for (const auto& issue : project.getParameters().validateReferences (
                 project.getLayout().getAll(), graph, project.getPresets()))
            validation.add (issue.toString());
        for (const auto& issue : graph.validateTypedGraph (project.getEngineType()))
            validation.add (issue.toString());
        for (const auto& issue : health.issues)
            validation.add (issue);

        context.validationSummary = validation.isEmpty()
            ? juce::String ("No immediate validation issues found by local checks.")
            : firstLines (validation, 8);

        return context;
    }

    juce::String AiAssistService::run (TaskType t, const juce::String& name) const
    {
        ProjectContextPack context;
        context.instrumentName = name.isEmpty() ? juce::String ("Untitled Instrument") : name;
        context.creator = "PatchCraft User";
        context.engine = "sample";
        context.category = "Instrument";
        context.canvasSummary = "1280 x 800";
        context.layoutSummary = "No project context supplied.";
        context.parameterSummary = "No parameter context supplied.";
        context.dspSummary = "No DSP context supplied.";
        context.sampleSummary = "No sample context supplied.";
        context.contentSummary = "No preset/expansion context supplied.";
        return run (t, context).details;
    }

    AiAssistService::Suggestion AiAssistService::run (TaskType t, const PatchCraftProject& project) const
    {
        return run (t, buildContextPack (project));
    }

    AiAssistService::Suggestion AiAssistService::run (TaskType t, const ProjectContextPack& context) const
    {
        const auto inst = context.instrumentName.isEmpty() ? juce::String ("Untitled Instrument") : context.instrumentName;
        Suggestion result;
        result.task = t;
        result.title = displayName (t);
        result.summary = displayTaskGoal (t);
        result.contextSummary = context.toSummaryText();

        switch (t)
        {
            case TaskType::BackgroundPrompt:
                result.details =
                    "Preview-only art prompt:\n\n"
                    "Create a " + context.canvasSummary + " premium audio plugin background for an instrument named '"
                    + inst + "'. Use a dark cinematic base, warm amber accents, and clear empty zones for controls. "
                    "Leave space for preset browsing at the top, performance controls in the center/bottom, and a focused hero/artwork area. "
                    "No text, no baked-in knobs, no baked-in meters. The exported UI controls will be layered in PatchCraft.\n\n"
                    "Project-aware notes:\n"
                    "- Match engine/category: " + context.engine + " / " + context.category + "\n"
                    "- Current layout: " + context.layoutSummary + "\n"
                    "- Keep contrast high enough for readable labels and parameter guidance.";
                break;

            case TaskType::SuggestLayout:
                result.details =
                    "Layout plan for '" + inst + "':\n"
                    "- Put preset/panic/settings controls in a consistent top strip; keep them outside tab containers.\n"
                    "- Use one primary performance row: 6-8 macro knobs with visible labels and value displays.\n"
                    "- Group sound-shaping controls into tabs: Main, Tone, Motion, FX, Output.\n"
                    "- Keep instrument-level performance modules visible: ARP/phrase, Motion, Glitch, XY/Morph.\n"
                    "- Reserve a small output meter and master volume/pan area on the right edge.\n"
                    "- Keep sample/wavetable visuals decorative unless they are tied to real runtime parameters.\n\n"
                    "Current layout context:\n" + context.layoutSummary;
                break;

            case TaskType::SuggestControls:
                result.details =
                    "Recommended controls that should map to registry parameters:\n"
                    "- Core macros: Volume, Cutoff, Resonance, Attack, Release, Reverb, Delay, Motion.\n"
                    "- Sound identity: Osc Blend/Wavetable Position/Sample Start depending on engine.\n"
                    "- Performance: Mod Wheel amount, Expression, ARP on/off, Retrigger on/off, Glide/Legato where available.\n"
                    "- FX: Delay Mix, Delay Feedback, Reverb Mix, Drive or Dynamics Mix.\n"
                    "- Output: Stereo Width, Output Gain, Limiter On, Output Meter.\n\n"
                    "Parameter context:\n" + context.parameterSummary;
                break;

            case TaskType::SuggestMacroAssignments:
                result.details =
                    "Macro assignment preview:\n"
                    "- Macro 1 'Tone': filterCutoff up, filterResonance slight up, spectral tilt darker/brighter.\n"
                    "- Macro 2 'Space': delayMix, delayFeedback, reverbMix, shimmer/smear amount if present.\n"
                    "- Macro 3 'Motion': lfoAmount, lfoRate, wavetable/frame position, chorus/phaser mix.\n"
                    "- Macro 4 'Shape': attack, decay, sustain, release for pluck-to-pad morphing.\n"
                    "- Macro 5 'Damage': drive, waveshaper, bit/glitch/stutter controls.\n"
                    "- Macro 6 'Width': stereo width, unison spread, chorus width, output safety trim.\n\n"
                    "DSP context:\n" + context.dspSummary;
                break;

            case TaskType::GeneratePresetNames:
                result.details =
                    "Preset name bank for '" + inst + "':\n"
                    "Deep Horizon\nWarm Motion\nGlass Pulse\nShimmer Engine\nVoltage Choir\n"
                    "Dust Circuit\nOrbit Pluck\nVelvet Scanner\nBroken Cathedral\nNeon Drift\n"
                    "Pressure Bloom\nMagnetic Rain\nAfterimage Keys\nTorn Halo\nCarbon Arp\n"
                    "Slow Reactor\n\nSuggested tags: " + context.engine + ", " + context.category
                    + ", motion, playable, expansion-ready";
                break;

            case TaskType::GenerateProductDescription:
                result.details =
                    inst + " is a playable PatchCraft instrument built for producers who want fast inspiration and deep control. "
                    "The instrument combines a polished custom interface with mapped performance controls, expansion-ready presets, "
                    "and a sound engine designed for expressive movement while playing inside a DAW.\n\n"
                    "Short copy:\n"
                    "A premium " + context.category + " for PatchCraft Player with performance macros, curated presets, and real-time sound shaping.\n\n"
                    "Expansion copy should mention preset count, required Player version, author, license, and included sample/library assets.";
                break;

            case TaskType::DesignCritique:
                result.details =
                    "Design critique preview:\n"
                    "- Verify every interactive element has a valid `parameterId`; unmapped controls should be labels/art only.\n"
                    "- Put related controls inside clear containers/tabs and avoid duplicate controls visible across tab states.\n"
                    "- Keep knob labels outside filmstrip artwork so imported controls remain reusable.\n"
                    "- Use audio-reactive visuals sparingly: one hero movement and one output/meter behavior is clearer than many moving parts.\n"
                    "- Treat Designer as source of truth; Test and Player should mirror tab/container states exactly.\n\n"
                    "Current layout:\n" + context.layoutSummary + "\n\nValidation:\n" + context.validationSummary;
                break;

            case TaskType::SoundRecipe:
                result.details =
                    "Sound recipe preview:\n"
                    "1. Source: build a stable primary source, then add one contrast layer such as noise, wavetable, or sample transient.\n"
                    "2. Filter/EQ: set broad tone first, then add surgical/dynamic EQ bands only where they audibly change the patch.\n"
                    "3. Amp: define the instrument class: pluck, pad, key, drone, hit, or texture.\n"
                    "4. Motion: add one tempo-synced motion lane and one performance macro; avoid stacking invisible modulation with no control.\n"
                    "5. FX: create one spatial identity and one optional throw/damage effect.\n"
                    "6. Output: normalize gain, limiter ceiling, width, and bypass-safe volume.\n\n"
                    "Current DSP:\n" + context.dspSummary;
                break;

            case TaskType::WavetableRecipe:
                result.details =
                    "Wavetable recipe preview:\n"
                    "- Use four custom frames: clean fundamental, harmonic-rich frame, asymmetric/warped frame, and noisy/aggressive frame.\n"
                    "- Map WT Position to a main macro or XY X-axis.\n"
                    "- Map WT Warp/Fold to velocity or mod wheel for playable intensity.\n"
                    "- Use slow frame scan for pads/motion; use stepped frame scan for arps/glitch.\n"
                    "- Add unison spread only after gain staging; wide unison should reduce output trim.\n\n"
                    "Relevant context:\n" + context.parameterSummary;
                break;

            case TaskType::EqChain:
                result.details =
                    "Advanced EQ chain preview:\n"
                    "- Band 1: high-pass safety, 20-40 Hz for most instruments, higher for plucks/arps.\n"
                    "- Band 2: low-mid cleanup, dynamic cut around 180-350 Hz when RMS rises.\n"
                    "- Band 3: body boost/cut, broad bell around 500-900 Hz.\n"
                    "- Band 4: presence control, dynamic bell around 2-5 kHz tied to velocity/mod macro.\n"
                    "- Band 5: air shelf, 8-14 kHz with macro amount.\n"
                    "- Band 6: resonant notch, narrow Q for harsh source peaks.\n"
                    "- Add solo/audition controls for surgical work and export the EQ macro labels clearly.\n\n"
                    "Validation context:\n" + context.validationSummary;
                break;

            case TaskType::ModulationPlan:
                result.details =
                    "Modulation plan preview:\n"
                    "- Global Motion macro: controls LFO amount/rate and wavetable position.\n"
                    "- Mod wheel: opens filter, raises motion depth, and adds stereo width or shimmer.\n"
                    "- Velocity: controls attack brightness, sample start/position, and transient emphasis.\n"
                    "- Aftertouch: temporary performance effect such as glitch, resonator throw, or pitch/formant bend.\n"
                    "- Tempo-sync lanes: one rhythmic lane for ARP/gate, one slow lane for pad/texture evolution.\n"
                    "- Audio-reactive: envelope follower to FX send or visual amount; keep it optional and labeled.\n\n"
                    "Current modulation context:\n" + context.dspSummary;
                break;

            case TaskType::BuildAssetGuidance:
                result.details =
                    "Build asset guidance preview:\n"
                    "- Build knobs at a consistent rendered frame size; avoid oversized knobs unless the layout specifically needs a hero control.\n"
                    "- Export editable source sidecars with each filmstrip so controls can be revised later.\n"
                    "- Tag assets by type, theme, color, size, and intended control use: macro, output, EQ, ARP, FX.\n"
                    "- Test imported assets on the Design canvas and in Test/Player runtime before adding to a sellable template.\n"
                    "- Keep labels separate from filmstrips so developers can localize, reposition, and resize text.\n\n"
                    "Current layout/control mix:\n" + context.layoutSummary;
                break;

            case TaskType::ExportChecklist:
                result.details =
                    "Sellable export checklist preview:\n"
                    "- Save a full Patch for each playable preset; section presets alone are not enough.\n"
                    "- Confirm all visible controls are mapped to host/MIDI-learnable parameters.\n"
                    "- Confirm all samples, artwork, filmstrips, library assets, and expansion metadata resolve locally.\n"
                    "- Run Sample Mapper Health and DSP graph validation before writing a pack.\n"
                    "- Verify Test page, Player standalone, and VST/FX variants all reproduce the same sound and UI state.\n"
                    "- Add preset categories, tags, author, license, version, and compatibility notes.\n\n"
                    "Current content:\n" + context.contentSummary + "\n\nValidation:\n" + context.validationSummary;
                break;
        }

        if (result.details.isEmpty())
            result.details = "No local copilot recipe exists for this action yet.";

        const auto config = loadLocalLlmConfig();
        if (config.provider == ProviderMode::LocalLlamaServer)
        {
            juce::String generated;
            juce::String error;
            if (runLocalLlamaRequest (config, t, context, result.details, generated, error))
            {
                result.summary += "\nProvider: Local llama.cpp model (" + config.model + ")";
                result.details = generated;
            }
            else
            {
                result.summary += "\nProvider: Built-in fallback. Local model unavailable: " + error;
                result.details = "Local AI unavailable: " + error
                    + "\n\nBuilt-in PatchCraft guidance:\n\n" + result.details;
            }
        }
        else
        {
            result.summary += "\nProvider: Built-in local templates.";
        }

        return result;
    }

} // namespace patchcraft
