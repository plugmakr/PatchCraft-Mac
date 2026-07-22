#include "ControlNodeAuthoring.h"

#include "ArpStepSequencerTemplate.h"

#include <algorithm>
#include <functional>
#include <set>

namespace patchcraft
{
    namespace
    {
        static bool supportsEngine (const ControlNodeDefinition& definition, const juce::String& engineId)
        {
            return definition.engines.isEmpty() || definition.engines.contains (engineId, true);
        }

        static bool ensureParameter (PatchCraftProject& project, const juce::String& parameterId)
        {
            auto& parameters = project.getParameters();
            if (parameters.find (parameterId) != nullptr)
                return true;

            juce::StringArray engines { project.getEngineType(), "synth", "sample", "fx" };
            engines.removeDuplicates (true);
            for (const auto& engine : engines)
            {
                ParameterDef definition;
                if (! ParameterModel::getRegistryDefinition (parameterId, engine, definition))
                    continue;

                parameters.add (definition);
                project.getLiveValues().getOrAddRaw (definition.id, definition.defaultValue);
                return true;
            }
            return false;
        }

        static void addSafeDefaults (DspBlock& block, const ControlNodeDefinition& definition)
        {
            if (definition.id == "lfo")
            {
                block.values["rate"] = 2.0f;
                block.values["sync"] = 0.0f;
                block.values["amount"] = 0.25f;
            }
            else if (definition.id == "arp")
            {
                block.values["rate"] = 1.0f;
                block.values["sync"] = 1.0f;
                block.values["arpSteps"] = 16.0f;
                block.values["arpGate"] = 0.58f;
                block.values["arpLaneSwing"] = 0.0f;
                block.values["mpRatchet"] = 1.0f;
                block.values["arpLaneRatchet"] = 1.0f;
                for (int step = 0; step < 16; ++step)
                {
                    block.values["mpStep" + juce::String (step) + "On"] = (step % 2 == 0) ? 1.0f : 0.0f;
                    block.values["mpStepProb" + juce::String (step)] = 1.0f;
                    block.values["mpGate" + juce::String (step)] = 0.58f;
                    block.values["mpStepDiv" + juce::String (step)] = 1.0f;
                }
            }
            else if (definition.id == "delay")
            {
                block.values["delayTime"] = 0.1875f;
                block.values["delayFeedback"] = 0.28f;
                block.values["delayMix"] = 0.12f;
                block.values["sync"] = 1.0f;
            }
            else if (definition.id == "reverb")
            {
                block.values["reverbMix"] = 0.12f;
            }
            else if (definition.id == "output")
            {
                block.values["outputLimiter"] = 1.0f;
                block.values["outputCeilingDb"] = -0.8f;
                block.values["outputGainDb"] = 0.0f;
            }
        }
    }

    const std::vector<ControlNodeDefinition>& ControlNodeAuthoring::definitions()
    {
        static const std::vector<ControlNodeDefinition> result {
            { "oscillator", "OSC", "source", "oscillator", "synth", "source", "stereo",
              { "oscType", "oscBlend", "detune", "octave", "volume" }, { "synth" }, false },
            { "sample", "SAMPLE", "source", "samplePlayer", "sampler", "source", "stereo",
              { "sampleStart", "sampleLength", "samplePitch", "sampleSlice", "sampleReverse" }, { "sample" }, false },
            { "input", "INPUT", "source", "liveInput", "studio", "source", "stereo",
              { "drive", "mix", "filterCutoff", "volume" }, { "fx" }, false },
            { "filter", "FILTER", "filter", "stateVariable", "studio", "tone", "stereo",
              { "filterCutoff", "filterResonance" }, {}, false },
            { "envelope", "ENVELOPE", "amp", "adsr", "synth", "dynamics", "stereo",
              { "attack", "decay", "sustain", "release" }, { "synth", "sample" }, false },
            { "lfo", "LFO", "mod", "lfo", "motion", "modulation", "modulation",
              { "lfoRate", "lfoAmount", "vibratoRate", "vibratoDepth" }, {}, true },
            { "arp", "ARP", "mod", "midiPlayground", "midi", "sequencer", "event",
              { "arpLaneRate", "arpLaneGate", "arpLaneSwing", "arpLaneProbability", "arpLaneRatchet" }, {}, true },
            { "delay", "DELAY", "fx", "delay", "creative", "space", "stereo",
              { "delayTime", "delayFeedback", "delayMix" }, {}, false },
            { "reverb", "REVERB", "fx", "reverb", "creative", "space", "stereo",
              { "reverbMix" }, {}, false },
            { "output", "OUTPUT", "out", "limiter", "studio", "output", "stereo",
              { "volume", "pan", "stereoWidth", "outputGainDb", "outputLimiter" }, {}, false }
        };
        return result;
    }

    std::vector<const ControlNodeDefinition*> ControlNodeAuthoring::definitionsForEngine (const juce::String& engineId)
    {
        std::vector<const ControlNodeDefinition*> result;
        for (const auto& definition : definitions())
            if (supportsEngine (definition, engineId))
                result.push_back (&definition);
        return result;
    }

    const ControlNodeDefinition* ControlNodeAuthoring::findDefinition (const juce::String& definitionId)
    {
        for (const auto& definition : definitions())
            if (definition.id == definitionId)
                return &definition;
        return nullptr;
    }

    const ControlNodeDefinition* ControlNodeAuthoring::definitionForBlock (const DspBlock& block)
    {
        const auto type = block.type.toLowerCase();
        const auto section = block.section.toLowerCase();
        if (section == "out") return findDefinition ("output");
        if (section == "amp" || type.contains ("adsr") || type.contains ("envelope")) return findDefinition ("envelope");
        if (section == "filter"
            && (type.contains ("state") || type == "filter" || type.contains ("morphfilter"))) return findDefinition ("filter");
        if (section == "mod" || section == "motion")
        {
            if (type.contains ("lfo")) return findDefinition ("lfo");
            if (type.contains ("arp") || type.contains ("midi") || type.contains ("sequencer")) return findDefinition ("arp");
        }
        if (section == "fx")
        {
            if (type.contains ("delay")) return findDefinition ("delay");
            if (type.contains ("reverb") || type.contains ("space")) return findDefinition ("reverb");
        }
        if (section == "source")
        {
            if (type.contains ("input") || type.contains ("drive")) return findDefinition ("input");
            if (type.contains ("sample") || type.contains ("granular") || type.contains ("slice") || type.contains ("chop")) return findDefinition ("sample");
            if (type.contains ("osc") || type.contains ("wavetable") || type.contains ("serum")) return findDefinition ("oscillator");
        }
        return nullptr;
    }

    DspBlock* ControlNodeAuthoring::ensureNode (PatchCraftProject& project, const juce::String& definitionId,
                                                juce::String& status)
    {
        const auto* definition = findDefinition (definitionId);
        if (definition == nullptr)
        {
            status = "Unknown node type.";
            return nullptr;
        }
        if (! supportsEngine (*definition, project.getEngineType()))
        {
            status = definition->name + " is not available for the " + project.getEngineType() + " engine.";
            return nullptr;
        }

        for (const auto& parameterId : definition->parameterIds)
            ensureParameter (project, parameterId);

        auto& graph = project.getDspGraph();
        for (auto& block : graph.blocks)
        {
            if (definitionForBlock (block) != definition)
                continue;
            block.enabled = true;
            block.metadata["family"] = definition->family;
            block.metadata["role"] = definition->role;
            block.metadata["ioMode"] = definition->ioMode;
            graph.userConfigured = true;
            project.markDirty();
            status = definition->name + " node ready.";
            return &block;
        }

        return createNode (project, definitionId, status);
    }

    DspBlock* ControlNodeAuthoring::createNode (PatchCraftProject& project, const juce::String& definitionId,
                                                juce::String& status)
    {
        const auto* definition = findDefinition (definitionId);
        if (definition == nullptr)
        {
            status = "Unknown node type.";
            return nullptr;
        }
        if (! supportsEngine (*definition, project.getEngineType()))
        {
            status = definition->name + " is not available for the " + project.getEngineType() + " engine.";
            return nullptr;
        }

        for (const auto& parameterId : definition->parameterIds)
            ensureParameter (project, parameterId);

        auto& graph = project.getDspGraph();
        DspBlock block;
        block.id = "control_node_" + definition->id;
        int suffix = 2;
        while (findBlock (project, block.id) != nullptr)
            block.id = "control_node_" + definition->id + "_" + juce::String (suffix++);
        block.section = definition->section;
        block.type = definition->type;
        block.name = definition->name;
        block.targetId = definition->parameterIds.isEmpty() ? juce::String() : definition->parameterIds[0];
        block.metadata["family"] = definition->family;
        block.metadata["role"] = definition->role;
        block.metadata["ioMode"] = definition->ioMode;
        addSafeDefaults (block, *definition);
        graph.blocks.push_back (std::move (block));
        graph.userConfigured = true;
        project.markDirty();
        project.notifyChanged();
        status = definition->name + " node added to the real DSP graph.";
        return &graph.blocks.back();
    }

    DspBlock* ControlNodeAuthoring::createNodeAt (PatchCraftProject& project, const juce::String& definitionId,
                                                  juce::Point<int> uiPosition, juce::String& status)
    {
        if (auto* block = createNode (project, definitionId, status))
        {
            block->metadata["uiX"] = juce::String (uiPosition.x);
            block->metadata["uiY"] = juce::String (uiPosition.y);
            return block;
        }
        return nullptr;
    }

    juce::String ControlNodeAuthoring::sectionCategoryLabel (const juce::String& section)
    {
        if (section == "source") return "Sources";
        if (section == "filter" || section == "amp") return "Shape";
        if (section == "mod" || section == "motion") return "Motion";
        if (section == "fx") return "FX";
        if (section == "out") return "Output";
        return "Other";
    }

    int ControlNodeAuthoring::defaultColumnForSection (const juce::String& section)
    {
        if (section == "source") return 40;
        if (section == "filter" || section == "amp") return 340;
        if (section == "mod" || section == "motion") return 640;
        if (section == "fx") return 940;
        if (section == "out") return 1240;
        return 640;
    }

    namespace
    {
        static bool sectionsAllowAudio (const juce::String& fromSection, const juce::String& toSection)
        {
            if (fromSection == "source")
                return toSection == "filter" || toSection == "amp" || toSection == "fx" || toSection == "out";
            if (fromSection == "filter" || fromSection == "amp")
                return toSection == "filter" || toSection == "amp" || toSection == "fx" || toSection == "out";
            if (fromSection == "fx")
                return toSection == "fx" || toSection == "out";
            return false;
        }

        static bool definitionCouldAudioConnect (const ControlNodeDefinition& from,
                                                 const ControlNodeDefinition& to)
        {
            if (from.modulationSource || to.modulationSource)
                return false;
            if (from.section == "out")
                return false;
            if (to.section == "source")
                return false;
            return sectionsAllowAudio (from.section, to.section);
        }

        static bool definitionCouldModConnect (const ControlNodeDefinition& from,
                                               const ControlNodeDefinition& to)
        {
            if (! from.modulationSource || to.section == "out")
                return false;
            if (from.id == "arp" && to.section == "source")
                return true;
            return to.section == "filter" || to.section == "amp" || to.section == "fx"
                || to.section == "source" || to.section == "mod" || to.section == "motion";
        }

        static bool definitionCouldConnectToAnchor (const PatchCraftProject& project,
                                                    const ControlNodeDefinition& candidate,
                                                    const DspBlock& anchor)
        {
            const auto* anchorDefinition = ControlNodeAuthoring::definitionForBlock (anchor);
            if (anchorDefinition == nullptr)
                return false;

            if (definitionCouldAudioConnect (candidate, *anchorDefinition)
                || definitionCouldModConnect (candidate, *anchorDefinition))
                return true;

            if (definitionCouldAudioConnect (*anchorDefinition, candidate)
                || definitionCouldModConnect (*anchorDefinition, candidate))
                return true;

            juce::ignoreUnused (project);
            return false;
        }

        static bool definitionUsefulOnEmptyCanvas (const ControlNodeDefinition& candidate,
                                                   const juce::String& engineId)
        {
            const bool engineOk = candidate.engines.isEmpty() || candidate.engines.contains (engineId, true);
            return engineOk && (candidate.section == "source" || candidate.section == "out");
        }
    }

    std::vector<const ControlNodeDefinition*> ControlNodeAuthoring::connectableDefinitions (
        const PatchCraftProject& project, const juce::String& anchorBlockId)
    {
        std::vector<const ControlNodeDefinition*> result;
        const auto engineId = project.getEngineType();
        const auto& blocks = project.getDspGraph().blocks;

        for (const auto& definition : definitions())
        {
            const bool engineOk = definition.engines.isEmpty() || definition.engines.contains (engineId, true);
            if (! engineOk)
                continue;

            bool include = false;

            if (anchorBlockId.isNotEmpty())
            {
                if (const auto* anchor = findBlock (project, anchorBlockId))
                    include = definitionCouldConnectToAnchor (project, definition, *anchor);
            }
            else if (blocks.empty())
            {
                include = definitionUsefulOnEmptyCanvas (definition, engineId);
            }
            else
            {
                for (const auto& block : blocks)
                {
                    if (definitionCouldConnectToAnchor (project, definition, block))
                    {
                        include = true;
                        break;
                    }
                }
            }

            if (include)
                result.push_back (&definition);
        }

        return result;
    }

    bool ControlNodeAuthoring::bindControl (PatchCraftProject& project, const juce::String& elementId,
                                            const juce::String& parameterId, juce::String& status)
    {
        if (elementId.isEmpty() || project.getLayout().find (elementId) == nullptr)
        {
            status = "The selected control no longer exists.";
            return false;
        }
        if (! ensureParameter (project, parameterId))
        {
            status = "The parameter is not available in the runtime registry.";
            return false;
        }

        const auto* definition = project.getParameters().find (parameterId);
        const auto label = definition != nullptr ? definition->name : parameterId;
        project.performLayoutEdit ("Bind control to sound node", [elementId, parameterId, label] (LayoutModel& layout)
        {
            if (auto* element = layout.find (elementId))
            {
                element->parameterId = parameterId;
                if (element->label.isEmpty())
                    element->label = label;
            }
        });
        status = "Control now drives " + label + ".";
        return true;
    }

    juce::String ControlNodeAuthoring::storageKeyForParameter (const DspBlock& block,
                                                               const juce::String& parameterId)
    {
        if (parameterId == "filterCutoff") return "cutoff";
        if (parameterId == "filterResonance") return "resonance";
        if (parameterId == "lfoRate") return "rate";
        if (parameterId == "lfoAmount") return "amount";
        if (parameterId == "arpLaneRate") return "rate";
        if (parameterId == "arpLaneGate") return "arpGate";
        if (parameterId == "arpLaneSwing") return "arpLaneSwing";
        if (parameterId == "arpLaneRatchet") return "arpLaneRatchet";
        if (parameterId == "arpLaneProbability") return "arpLaneProbability";
        juce::ignoreUnused (block);
        return parameterId;
    }

    float ControlNodeAuthoring::graphValueForParameter (const ParameterDef& definition,
                                                        const juce::String& parameterId, float value)
    {
        const bool normalisedGraphValue = parameterId == "oscType" || parameterId == "osc2Type"
            || parameterId == "oscBlend" || parameterId == "octave" || parameterId == "detune"
            || parameterId == "osc2Detune" || parameterId == "subBlend" || parameterId == "noiseBlend"
            || parameterId == "volume" || parameterId == "pan" || parameterId == "filterCutoff"
            || parameterId == "filterResonance" || parameterId == "attack" || parameterId == "decay"
            || parameterId == "sustain" || parameterId == "release" || parameterId == "delayTime"
            || parameterId == "delayFeedback" || parameterId == "delayMix" || parameterId == "reverbMix"
            || parameterId == "drive" || parameterId == "mix";
        if (! normalisedGraphValue || std::abs (definition.max - definition.min) < 0.000001f)
            return value;
        return juce::jlimit (0.0f, 1.0f, (value - definition.min) / (definition.max - definition.min));
    }

    bool ControlNodeAuthoring::setNodeParameter (PatchCraftProject& project, const juce::String& blockId,
                                                 const juce::String& parameterId, float value, bool notify)
    {
        auto* block = findBlock (project, blockId);
        auto* definition = project.getParameters().find (parameterId);
        if (block == nullptr || definition == nullptr)
            return false;

        const auto limited = juce::jlimit (definition->min, definition->max, value);
        project.getLiveValues().setValue (parameterId, limited);
        const auto storageKey = storageKeyForParameter (*block, parameterId);
        block->values[storageKey] = graphValueForParameter (*definition, parameterId, limited);
        if (parameterId == "arpLaneProbability")
            for (int step = 0; step < 128; ++step)
                block->values["mpStepProb" + juce::String (step)] = limited;
        if (parameterId == "arpLaneRatchet")
        {
            const int ratchet = juce::jlimit (1, 8, juce::roundToInt (limited));
            block->values["mpRatchet"] = (float) ratchet;
            for (int step = 0; step < 128; ++step)
                block->values["mpStepDiv" + juce::String (step)] = (float) ratchet;
        }
        block->targetId = parameterId;
        project.getDspGraph().userConfigured = true;
        project.markDirty();
        if (notify)
            project.notifyChanged (PatchCraftProject::ChangeScope::dspRealtime);
        return true;
    }

    bool ControlNodeAuthoring::hasModulationRoute (const PatchCraftProject& project,
                                                   const juce::String& sourceBlockId,
                                                   const juce::String& targetParameterId)
    {
        for (const auto& route : project.getDspGraph().modulation)
            if (route.sourceId == sourceBlockId && route.targetId == targetParameterId && route.enabled)
                return true;
        return false;
    }

    bool ControlNodeAuthoring::setModulationRoute (PatchCraftProject& project,
                                                   const juce::String& sourceBlockId,
                                                   const juce::String& targetParameterId,
                                                   bool enabled, float amount, juce::String& status)
    {
        auto* block = findBlock (project, sourceBlockId);
        auto* parameter = project.getParameters().find (targetParameterId);
        if (block == nullptr || parameter == nullptr || ! parameter->modulatable)
        {
            status = "Choose a modulatable target control first.";
            return false;
        }

        auto& routes = project.getDspGraph().modulation;
        auto found = std::find_if (routes.begin(), routes.end(), [&] (const ModRoute& route)
        {
            return route.sourceId == sourceBlockId && route.targetId == targetParameterId;
        });
        if (! enabled)
        {
            if (found != routes.end())
                routes.erase (found);
            status = block->name + " modulation disconnected.";
        }
        else
        {
            if (found == routes.end())
            {
                ModRoute route;
                route.id = sourceBlockId + "_to_" + targetParameterId;
                route.sourceId = sourceBlockId;
                route.targetId = targetParameterId;
                route.amount = juce::jlimit (-1.0f, 1.0f, amount);
                routes.push_back (std::move (route));
            }
            else
            {
                found->enabled = true;
                found->amount = juce::jlimit (-1.0f, 1.0f, amount);
            }
            block->targetId = targetParameterId;
            status = block->name + " now modulates " + parameter->name + ".";
        }

        project.getDspGraph().userConfigured = true;
        project.markDirty();
        project.notifyChanged();
        return true;
    }

    bool ControlNodeAuthoring::canConnectNodes (const PatchCraftProject& project,
                                                const juce::String& sourceBlockId,
                                                const juce::String& targetBlockId,
                                                juce::String* reason)
    {
        const auto* source = findBlock (project, sourceBlockId);
        const auto* target = findBlock (project, targetBlockId);
        if (source == nullptr || target == nullptr || source == target)
        {
            if (reason != nullptr)
                *reason = "Drop the cable on another node's input port.";
            return false;
        }

        const auto* sourceDefinition = definitionForBlock (*source);
        const auto* targetDefinition = definitionForBlock (*target);
        if (sourceDefinition == nullptr || targetDefinition == nullptr)
        {
            if (reason != nullptr)
                *reason = "One of these nodes cannot be routed yet.";
            return false;
        }

        if (sourceDefinition->modulationSource)
        {
            if (definitionCouldModConnect (*sourceDefinition, *targetDefinition))
            {
                if (reason != nullptr)
                {
                    if (sourceDefinition->id == "arp" && targetDefinition->section == "source")
                        *reason = "Event: " + source->name + " → " + target->name;
                    else
                        *reason = "Modulation: " + source->name + " → " + target->name;
                }
                return true;
            }

            if (reason != nullptr)
                *reason = "Modulation sources connect to Shape, FX, Sources, or Motion — not Output.";
            return false;
        }

        if (definitionCouldAudioConnect (*sourceDefinition, *targetDefinition))
        {
            if (reason != nullptr)
                *reason = "Audio: " + source->name + " → " + target->name;
            return true;
        }

        if (reason != nullptr)
            *reason = "Audio cables run from a source or processor into Shape, FX, or Output.";
        return false;
    }

    juce::StringArray ControlNodeAuthoring::connectableTargetNames (const PatchCraftProject& project,
                                                                    const juce::String& sourceBlockId)
    {
        juce::StringArray names;
        for (const auto& block : project.getDspGraph().blocks)
        {
            if (block.id == sourceBlockId)
                continue;
            if (canConnectNodes (project, sourceBlockId, block.id))
                names.add (block.name.isNotEmpty() ? block.name : block.id);
        }
        return names;
    }

    bool ControlNodeAuthoring::connectNodes (PatchCraftProject& project,
                                             const juce::String& sourceBlockId,
                                             const juce::String& targetBlockId,
                                             juce::String& status)
    {
        auto* source = findBlock (project, sourceBlockId);
        auto* target = findBlock (project, targetBlockId);
        if (source == nullptr || target == nullptr || source == target)
        {
            status = "Drop the cable on another node's input port.";
            return false;
        }

        const auto* sourceDefinition = definitionForBlock (*source);
        const auto* targetDefinition = definitionForBlock (*target);
        if (sourceDefinition == nullptr || targetDefinition == nullptr)
        {
            status = "One of these nodes cannot be routed yet.";
            return false;
        }

        if (sourceDefinition->modulationSource)
        {
            if (sourceDefinition->id == "arp" && targetDefinition->section == "source")
            {
                auto& edges = project.getDspGraph().edges;
                const auto existing = std::find_if (edges.begin(), edges.end(), [&] (const DspGraphEdge& edge)
                {
                    return edge.enabled && edge.sourceNodeId == sourceBlockId
                        && edge.targetNodeId == targetBlockId && edge.signalType == DspSignalType::event;
                });
                if (existing == edges.end())
                {
                    DspGraphEdge edge;
                    edge.id = sourceBlockId + "_event_to_" + targetBlockId;
                    edge.sourceNodeId = sourceBlockId;
                    edge.sourcePortId = "eventOut";
                    edge.targetNodeId = targetBlockId;
                    edge.targetPortId = "eventIn";
                    edge.signalType = DspSignalType::event;
                    edges.push_back (std::move (edge));
                }
                source->metadata["eventTargetId"] = targetBlockId;
                status = source->name + " now triggers " + target->name + ".";
            }
            else
            {
                juce::String targetParameter = target->targetId;
                auto* parameter = project.getParameters().find (targetParameter);
                if (parameter == nullptr || ! parameter->modulatable)
                {
                    targetParameter.clear();
                    for (const auto& parameterId : targetDefinition->parameterIds)
                    {
                        if (auto* candidate = project.getParameters().find (parameterId); candidate != nullptr && candidate->modulatable)
                        {
                            targetParameter = parameterId;
                            break;
                        }
                    }
                }

                if (targetParameter.isEmpty())
                {
                    status = target->name + " has no modulatable input.";
                    return false;
                }
                return setModulationRoute (project, sourceBlockId, targetParameter, true, 0.25f, status);
            }
        }
        else
        {
            if (sourceDefinition->section == "out" || targetDefinition->section == "source"
                || targetDefinition->modulationSource)
            {
                status = "Audio cables run from a source or processor into Shape, FX, or Output.";
                return false;
            }

            auto& edges = project.getDspGraph().edges;
            const auto existing = std::find_if (edges.begin(), edges.end(), [&] (const DspGraphEdge& edge)
            {
                return edge.enabled && edge.sourceNodeId == sourceBlockId
                    && edge.targetNodeId == targetBlockId && edge.signalType == DspSignalType::audio;
            });
            if (existing != edges.end())
            {
                status = source->name + " is already connected to " + target->name + ".";
                return true;
            }

            std::vector<juce::String> pending { targetBlockId };
            std::set<juce::String> visited;
            while (! pending.empty())
            {
                const auto current = pending.back();
                pending.pop_back();
                if (! visited.insert (current).second)
                    continue;
                if (current == sourceBlockId)
                {
                    status = "That cable would create an audio feedback loop.";
                    return false;
                }
                for (const auto& edge : edges)
                    if (edge.enabled && edge.signalType == DspSignalType::audio && edge.sourceNodeId == current)
                        pending.push_back (edge.targetNodeId);
            }

            DspGraphEdge edge;
            edge.id = sourceBlockId + "_audio_to_" + targetBlockId;
            edge.sourceNodeId = sourceBlockId;
            edge.targetNodeId = targetBlockId;
            edges.push_back (std::move (edge));
            status = source->name + " audio now feeds " + target->name + ".";
        }

        project.getDspGraph().userConfigured = true;
        project.markDirty();
        project.notifyChanged();
        return true;
    }

    bool ControlNodeAuthoring::disconnectNodes (PatchCraftProject& project,
                                                const juce::String& sourceBlockId,
                                                const juce::String& targetBlockId,
                                                juce::String& status)
    {
        auto& edges = project.getDspGraph().edges;
        bool disconnected = false;
        
        for (auto it = edges.begin(); it != edges.end(); )
        {
            if (it->sourceNodeId == sourceBlockId && it->targetNodeId == targetBlockId)
            {
                it = edges.erase (it);
                disconnected = true;
            }
            else
            {
                ++it;
            }
        }
        
        auto& mods = project.getDspGraph().modulation;
        for (auto it = mods.begin(); it != mods.end(); )
        {
            if (it->sourceId == sourceBlockId)
            {
                if (auto* target = findBlock (project, targetBlockId))
                {
                    if (it->targetId == target->targetId)
                    {
                        it = mods.erase (it);
                        disconnected = true;
                        continue;
                    }
                }
            }
            ++it;
        }

        if (disconnected)
        {
            status = "Disconnected.";
            project.markDirty();
            project.notifyChanged();
            return true;
        }

        status = "No connection found to disconnect.";
        return false;
    }

    bool ControlNodeAuthoring::deleteNode (PatchCraftProject& project,
                                           const juce::String& blockId,
                                           juce::String& status)
    {
        auto& graph = project.getDspGraph();
        const auto* block = findBlock (project, blockId);
        if (block == nullptr)
        {
            status = "Node not found.";
            return false;
        }

        const auto blockName = block->name;

        graph.edges.erase (std::remove_if (graph.edges.begin(), graph.edges.end(),
            [&] (const DspGraphEdge& edge)
            {
                return edge.sourceNodeId == blockId || edge.targetNodeId == blockId;
            }), graph.edges.end());

        graph.modulation.erase (std::remove_if (graph.modulation.begin(), graph.modulation.end(),
            [&] (const ModRoute& route)
            {
                return route.sourceId == blockId;
            }), graph.modulation.end());

        graph.blocks.erase (std::remove_if (graph.blocks.begin(), graph.blocks.end(),
            [&] (const DspBlock& candidate)
            {
                return candidate.id == blockId;
            }), graph.blocks.end());

        graph.userConfigured = true;
        project.markDirty();
        project.notifyChanged();
        status = "Deleted " + blockName + ".";
        return true;
    }

    DspBlock* ControlNodeAuthoring::findBlock (PatchCraftProject& project, const juce::String& blockId)
    {
        for (auto& block : project.getDspGraph().blocks)
            if (block.id == blockId)
                return &block;
        return nullptr;
    }

    const DspBlock* ControlNodeAuthoring::findBlock (const PatchCraftProject& project, const juce::String& blockId)
    {
        for (const auto& block : project.getDspGraph().blocks)
            if (block.id == blockId)
                return &block;
        return nullptr;
    }

    namespace
    {
        static void clearAuthoringGraph (PatchCraftProject& project)
        {
            auto& graph = project.getDspGraph();
            graph.blocks.clear();
            graph.edges.clear();
            graph.modulation.clear();
            graph.userConfigured = true;
        }

        static DspBlock* addTemplateNode (PatchCraftProject& project, const juce::String& blockId,
                                          const juce::String& definitionId, int uiX, int uiY,
                                          std::function<void (DspBlock&)> tune = {})
        {
            const auto* definition = ControlNodeAuthoring::findDefinition (definitionId);
            if (definition == nullptr)
                return nullptr;

            for (const auto& parameterId : definition->parameterIds)
                ensureParameter (project, parameterId);

            DspBlock block;
            block.id = blockId;
            block.section = definition->section;
            block.type = definition->type;
            block.name = definition->name;
            block.targetId = definition->parameterIds.isEmpty() ? juce::String() : definition->parameterIds[0];
            block.metadata["family"] = definition->family;
            block.metadata["role"] = definition->role;
            block.metadata["ioMode"] = definition->ioMode;
            block.metadata["uiX"] = juce::String (uiX);
            block.metadata["uiY"] = juce::String (uiY);
            addSafeDefaults (block, *definition);
            if (tune)
                tune (block);

            auto& graph = project.getDspGraph();
            graph.blocks.push_back (std::move (block));
            return &graph.blocks.back();
        }

        static float liveValueFromGraph (const ParameterDef& definition, const juce::String& parameterId,
                                         float graphValue)
        {
            const bool normalisedGraphValue = parameterId == "oscType" || parameterId == "osc2Type"
                || parameterId == "oscBlend" || parameterId == "octave" || parameterId == "detune"
                || parameterId == "osc2Detune" || parameterId == "subBlend" || parameterId == "noiseBlend"
                || parameterId == "volume" || parameterId == "pan" || parameterId == "filterCutoff"
                || parameterId == "filterResonance" || parameterId == "attack" || parameterId == "decay"
                || parameterId == "sustain" || parameterId == "release" || parameterId == "delayTime"
                || parameterId == "delayFeedback" || parameterId == "delayMix" || parameterId == "reverbMix"
                || parameterId == "drive" || parameterId == "mix";
            if (! normalisedGraphValue || std::abs (definition.max - definition.min) < 0.000001f)
                return graphValue;
            return definition.min + graphValue * (definition.max - definition.min);
        }
    }

    juce::StringArray ControlNodeAuthoring::getGraphTemplateNames()
    {
        return { "Arp Step Sequencer", "Pluck Arp", "Warm Pad", "Drum Bus", "Init Synth" };
    }

    juce::StringArray ControlNodeAuthoring::getPresetNames()
    {
        return getGraphTemplateNames();
    }

    bool ControlNodeAuthoring::applyGraphTemplate (PatchCraftProject& project,
                                                   const juce::String& templateName,
                                                   juce::String& status)
    {
        clearAuthoringGraph (project);

        if (templateName == "Init Synth")
        {
            addTemplateNode (project, "tpl_osc", "oscillator", 40, 80);
            addTemplateNode (project, "tpl_amp", "envelope", 340, 80);
            addTemplateNode (project, "tpl_out", "output", 1240, 80);
            connectNodes (project, "tpl_osc", "tpl_amp", status);
            connectNodes (project, "tpl_amp", "tpl_out", status);
            status = "Loaded Init Synth graph template.";
        }
        else if (templateName == "Pluck Arp")
        {
            addTemplateNode (project, "tpl_osc", "oscillator", 40, 80);
            addTemplateNode (project, "tpl_filter", "filter", 340, 80, [] (DspBlock& block)
            {
                block.values["cutoff"] = 0.68f;
                block.values["resonance"] = 0.24f;
            });
            addTemplateNode (project, "tpl_amp", "envelope", 340, 240, [] (DspBlock& block)
            {
                block.values["decay"] = 0.2f;
                block.values["sustain"] = 0.0f;
                block.values["release"] = 0.18f;
            });
            addTemplateNode (project, "tpl_arp", "arp", 640, 80, [] (DspBlock& block) { seedArpStepPattern (block, 0); });
            addTemplateNode (project, "tpl_delay", "delay", 940, 80);
            addTemplateNode (project, "tpl_out", "output", 1240, 80);
            connectNodes (project, "tpl_osc", "tpl_filter", status);
            connectNodes (project, "tpl_filter", "tpl_amp", status);
            connectNodes (project, "tpl_amp", "tpl_delay", status);
            connectNodes (project, "tpl_delay", "tpl_out", status);
            connectNodes (project, "tpl_arp", "tpl_osc", status);
            status = "Loaded Pluck Arp graph template.";
        }
        else if (templateName == "Warm Pad")
        {
            addTemplateNode (project, "tpl_osc", "oscillator", 40, 80, [] (DspBlock& block)
            {
                block.values["oscBlend"] = 0.42f;
                block.values["detune"] = 0.14f;
                block.values["octave"] = 0.5f;
            });
            addTemplateNode (project, "tpl_filter", "filter", 340, 80, [] (DspBlock& block)
            {
                block.values["cutoff"] = 0.48f;
                block.values["resonance"] = 0.12f;
            });
            addTemplateNode (project, "tpl_amp", "envelope", 340, 240, [] (DspBlock& block)
            {
                block.values["attack"] = 0.42f;
                block.values["decay"] = 0.55f;
                block.values["sustain"] = 0.72f;
                block.values["release"] = 0.68f;
            });
            addTemplateNode (project, "tpl_lfo", "lfo", 640, 80);
            addTemplateNode (project, "tpl_reverb", "reverb", 940, 80, [] (DspBlock& block)
            {
                block.values["reverbMix"] = 0.28f;
            });
            addTemplateNode (project, "tpl_out", "output", 1240, 80);
            connectNodes (project, "tpl_osc", "tpl_filter", status);
            connectNodes (project, "tpl_filter", "tpl_amp", status);
            connectNodes (project, "tpl_amp", "tpl_reverb", status);
            connectNodes (project, "tpl_reverb", "tpl_out", status);
            connectNodes (project, "tpl_lfo", "tpl_filter", status);
            status = "Loaded Warm Pad graph template.";
        }
        else if (templateName == "Drum Bus")
        {
            const auto sourceDef = project.getEngineType() == "sample" ? "sample" : "oscillator";
            addTemplateNode (project, "tpl_source", sourceDef, 40, 80, [] (DspBlock& block)
            {
                block.values["octave"] = 0.2f;
                block.values["oscBlend"] = 0.0f;
            });
            addTemplateNode (project, "tpl_filter", "filter", 340, 80, [] (DspBlock& block)
            {
                block.values["cutoff"] = 0.58f;
                block.values["resonance"] = 0.18f;
            });
            addTemplateNode (project, "tpl_amp", "envelope", 340, 240, [] (DspBlock& block)
            {
                block.values["attack"] = 0.0f;
                block.values["decay"] = 0.22f;
                block.values["sustain"] = 0.0f;
                block.values["release"] = 0.12f;
            });
            addTemplateNode (project, "tpl_delay", "delay", 940, 80, [] (DspBlock& block)
            {
                block.values["delayMix"] = 0.08f;
                block.values["delayFeedback"] = 0.18f;
            });
            addTemplateNode (project, "tpl_out", "output", 1240, 80);
            connectNodes (project, "tpl_source", "tpl_filter", status);
            connectNodes (project, "tpl_filter", "tpl_amp", status);
            connectNodes (project, "tpl_amp", "tpl_delay", status);
            connectNodes (project, "tpl_delay", "tpl_out", status);
            status = "Loaded Drum Bus graph template.";
        }
        else if (templateName == "Arp Step Sequencer")
        {
            addTemplateNode (project, "seq_source", "oscillator", 40, 80, [] (DspBlock& block)
            {
                block.name = "SEQ OSC";
                block.values["oscBlend"] = 0.12f;
                block.values["octave"] = 0.5f;
                block.values["detune"] = 0.08f;
            });
            addTemplateNode (project, "seq_filter", "filter", 340, 80, [] (DspBlock& block)
            {
                block.name = "SEQ FILTER";
                block.values["cutoff"] = 0.62f;
                block.values["resonance"] = 0.22f;
            });
            addTemplateNode (project, "seq_amp", "envelope", 340, 240, [] (DspBlock& block)
            {
                block.name = "SEQ AMP";
                block.values["attack"] = 0.01f;
                block.values["decay"] = 0.22f;
                block.values["sustain"] = 0.18f;
                block.values["release"] = 0.24f;
            });
            addTemplateNode (project, "seq_arp", "arp", 640, 80, [] (DspBlock& block)
            {
                block.name = "STEP ARP";
                seedArpStepPattern (block, 0);
            });
            addTemplateNode (project, "seq_delay", "delay", 940, 80, [] (DspBlock& block)
            {
                block.name = "SEQ DELAY";
                block.values["delayTime"] = 0.1875f;
                block.values["delayFeedback"] = 0.28f;
                block.values["delayMix"] = 0.14f;
                block.values["sync"] = 1.0f;
            });
            addTemplateNode (project, "seq_output", "output", 1240, 80, [] (DspBlock& block)
            {
                block.name = "OUTPUT";
            });
            connectNodes (project, "seq_source", "seq_filter", status);
            connectNodes (project, "seq_filter", "seq_amp", status);
            connectNodes (project, "seq_amp", "seq_delay", status);
            connectNodes (project, "seq_delay", "seq_output", status);
            connectNodes (project, "seq_arp", "seq_source", status);
            status = "Loaded Arp Step Sequencer graph template.";
        }
        else
        {
            status = "Unknown graph template.";
            return false;
        }

        syncTemplateLiveValues (project);
        project.markDirty();
        project.notifyChanged();
        return true;
    }

    bool ControlNodeAuthoring::applyPreset (PatchCraftProject& project, const juce::String& presetName,
                                            juce::String& status)
    {
        return applyGraphTemplate (project, presetName, status);
    }

    void ControlNodeAuthoring::tidyGraphLayout (PatchCraftProject& project)
    {
        std::map<juce::String, int> sectionCounts;
        for (auto& block : project.getDspGraph().blocks)
        {
            const int x = defaultColumnForSection (block.section);
            const int y = 38 + sectionCounts[block.section]++ * 168;
            block.metadata["uiX"] = juce::String (x);
            block.metadata["uiY"] = juce::String (y);
        }
        project.markDirty();
    }

    std::vector<DspGraphValidationIssue> ControlNodeAuthoring::validateNodeGraph (const PatchCraftProject& project)
    {
        return project.getDspGraph().validateTypedGraph (project.getEngineType());
    }

    void ControlNodeAuthoring::syncTemplateLiveValues (PatchCraftProject& project)
    {
        for (const auto& block : project.getDspGraph().blocks)
        {
            const auto* definition = definitionForBlock (block);
            if (definition == nullptr)
                continue;

            juce::StringArray parameterIds = definition->parameterIds;
            if (block.targetId.isNotEmpty() && ! parameterIds.contains (block.targetId, false))
                parameterIds.add (block.targetId);

            for (const auto& parameterId : parameterIds)
            {
                const auto* parameter = project.getParameters().find (parameterId);
                if (parameter == nullptr)
                    continue;

                const auto storageKey = storageKeyForParameter (block, parameterId);
                const auto it = block.values.find (storageKey);
                if (it == block.values.end())
                    continue;

                const auto liveValue = liveValueFromGraph (*parameter, parameterId, it->second);
                project.getLiveValues().setValue (parameterId,
                                                  juce::jlimit (parameter->min, parameter->max, liveValue));
            }
        }
        project.markDirty();
    }
}
