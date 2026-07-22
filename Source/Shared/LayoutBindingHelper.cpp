#include "LayoutBindingHelper.h"

#include "ParameterModel.h"

namespace patchcraft
{
    namespace
    {
        bool parameterExists (const PatchCraftProject& project, const juce::String& id)
        {
            return id.isNotEmpty() && project.getParameters().find (id) != nullptr;
        }

        bool parameterUsedOnLayout (const PatchCraftProject& project, const juce::String& id)
        {
            for (const auto& element : project.getLayout().getAll())
                if (element.parameterId == id)
                    return true;
            return false;
        }

        juce::String aliasParameter (const juce::String& hint, const PatchCraftProject& project)
        {
            const auto lower = hint.toLowerCase();
            struct Alias { const char* key; const char* param; };
            static const Alias aliases[] = {
                { "pos", "wtPosition" }, { "position", "wtPosition" }, { "morph", "wtMorph" },
                { "warp", "wtWarp" }, { "fold", "wtFold" }, { "unison", "wtUnison" },
                { "detune", "wtDetune" }, { "cutoff", "filterCutoff" }, { "filter", "filterCutoff" },
                { "resonance", "filterResonance" }, { "res", "filterResonance" },
                { "delay", "delayMix" }, { "reverb", "reverbMix" }, { "verb", "reverbMix" },
                { "level", "volume" }, { "vol", "volume" }, { "pitch", "samplePitch" },
                { "start", "sampleStart" }, { "length", "sampleLength" }
            };

            for (const auto& alias : aliases)
                if (lower.contains (alias.key) && parameterExists (project, alias.param))
                    return alias.param;

            return {};
        }

        juce::StringArray defaultParametersForEngine (const juce::String& engine)
        {
            if (engine.equalsIgnoreCase ("synth"))
                return { "filterCutoff", "filterResonance", "wtPosition", "wtMorph", "wtWarp",
                         "delayMix", "reverbMix", "volume", "attack", "release" };
            if (engine.equalsIgnoreCase ("fx"))
                return { "filterCutoff", "drive", "mix", "delayMix", "reverbMix", "volume" };
            return { "filterCutoff", "filterResonance", "samplePitch", "delayMix", "reverbMix",
                     "volume", "pan", "sampleStart", "sampleLength", "attack", "release" };
        }

        juce::String firstAvailableDefault (const PatchCraftProject& project)
        {
            for (const auto& id : defaultParametersForEngine (project.getEngineType()))
                if (parameterExists (project, id) && ! parameterUsedOnLayout (project, id))
                    return id;

            for (const auto& id : defaultParametersForEngine (project.getEngineType()))
                if (parameterExists (project, id))
                    return id;

            return {};
        }
    }

    bool layoutElementRequiresParameter (ElementType type)
    {
        return isRuntimeControlElement (type)
            || type == ElementType::XYPad
            || type == ElementType::Meter
            || type == ElementType::GranularField
            || type == ElementType::Waveform;
    }

    bool layoutElementUsesSemanticParameterId (ElementType type)
    {
        return type == ElementType::PadGrid
            || type == ElementType::DrumPad
            || type == ElementType::DrumGrid
            || type == ElementType::PianoRoll
            || type == ElementType::Keyboard
            || type == ElementType::ArpLane
            || type == ElementType::SequencerLane
            || type == ElementType::RuntimeSampleLibrary
            || type == ElementType::Panel
            || type == ElementType::Group
            || type == ElementType::TabPanel
            || type == ElementType::ScrollPanel
            || type == ElementType::Separator
            || type == ElementType::Label
            || type == ElementType::Image
            || type == ElementType::Shape;
    }

    juce::String resolveControlParameter (ElementType type,
                                          juce::String hint,
                                          const PatchCraftProject& project)
    {
        if (layoutElementUsesSemanticParameterId (type))
            return {};

        if (! layoutElementRequiresParameter (type))
            return hint;

        hint = hint.trim();
        if (parameterExists (project, hint))
            return hint;

        if (const auto aliased = aliasParameter (hint, project); aliased.isNotEmpty())
            return aliased;

        if (type == ElementType::Knob || type == ElementType::Slider
            || type == ElementType::Toggle || type == ElementType::Button)
            return firstAvailableDefault (project);

        return hint;
    }

    void sanitiseLayoutParameterReferences (PatchCraftProject& project)
    {
        project.performLayoutEdit ("Sanitise layout parameter references", [&] (LayoutModel& layout)
        {
            for (auto& element : layout.getAll())
            {
                if (layoutElementUsesSemanticParameterId (element.type))
                {
                    if (element.parameterId.isNotEmpty()
                        && project.getParameters().find (element.parameterId) == nullptr)
                        element.parameterId.clear();
                    continue;
                }

                if (! layoutElementRequiresParameter (element.type))
                    continue;

                if (element.parameterId.isEmpty())
                    continue;

                if (project.getParameters().find (element.parameterId) == nullptr)
                    element.parameterId = resolveControlParameter (element.type, element.parameterId, project);
            }
        });
    }

    void syncDspGraphValuesToLiveStore (PatchCraftProject& project)
    {
        auto& live = project.getLiveValues();
        const auto& params = project.getParameters();

        for (const auto& block : project.getDspGraph().blocks)
        {
            for (const auto& entry : block.values)
            {
                if (params.find (entry.first) == nullptr)
                    continue;

                live.setValue (entry.first, entry.second);
            }
        }
    }

    bool ensureProjectParameter (PatchCraftProject& project,
                                 const juce::String& paramId,
                                 const juce::String& engineHint)
    {
        if (paramId.isEmpty() || project.getParameters().find (paramId) != nullptr)
            return paramId.isNotEmpty();

        juce::StringArray engines;
        if (engineHint.isNotEmpty())
            engines.addIfNotAlreadyThere (engineHint);
        engines.addIfNotAlreadyThere (project.getEngineType());
        engines.addIfNotAlreadyThere ("sample");
        engines.addIfNotAlreadyThere ("synth");
        engines.addIfNotAlreadyThere ("fx");

        ParameterDef def;
        for (const auto& engine : engines)
        {
            if (ParameterModel::getRegistryDefinition (paramId, engine, def))
            {
                project.getParameters().add (def);
                project.getLiveValues().getOrAddRaw (def.id, def.defaultValue);
                return true;
            }
        }

        return false;
    }

    void attachControlToExistingRouting (PatchCraftProject& project, const juce::String& paramId)
    {
        if (paramId.isEmpty())
            return;

        const auto* def = project.getParameters().find (paramId);
        if (def == nullptr)
            return;

        auto& graph = project.getDspGraph();
        auto& live = project.getLiveValues();
        const float current = live.getValue (paramId, def->defaultValue);

        for (auto& block : graph.blocks)
        {
            if (block.targetId == paramId || block.values.find (paramId) != block.values.end())
            {
                block.values[paramId] = current;
                block.targetId = paramId;
                graph.userConfigured = true;
                return;
            }
        }

        auto sectionForParam = [&] (const juce::String& id) -> juce::String
        {
            const auto lower = id.toLowerCase();
            if (lower.startsWith ("wt") || lower.startsWith ("osc"))
                return "source";
            if (lower.contains ("filter") || lower.contains ("attack") || lower.contains ("release")
                || lower.contains ("decay") || lower.contains ("sustain"))
                return juce::String (project.getEngineType()).equalsIgnoreCase ("sample") ? "shape" : "filter";
            if (lower.contains ("delay") || lower.contains ("reverb") || lower.contains ("mix")
                || lower.contains ("drive") || lower.contains ("tape") || lower.contains ("lofi"))
                return "fx";
            return {};
        };

        const auto preferredSection = sectionForParam (paramId);
        DspBlock* targetBlock = nullptr;

        if (preferredSection.isNotEmpty())
        {
            for (auto& block : graph.blocks)
                if (block.section.equalsIgnoreCase (preferredSection))
                {
                    targetBlock = &block;
                    break;
                }
        }

        if (targetBlock == nullptr)
        {
            for (auto& block : graph.blocks)
            {
                if (block.section.equalsIgnoreCase ("source") || block.section.equalsIgnoreCase ("shape")
                    || block.section.equalsIgnoreCase ("filter") || block.section.equalsIgnoreCase ("fx"))
                {
                    targetBlock = &block;
                    break;
                }
            }
        }

        if (targetBlock != nullptr)
        {
            targetBlock->values[paramId] = current;
            targetBlock->targetId = paramId;
            graph.userConfigured = true;
        }
    }

} // namespace patchcraft
