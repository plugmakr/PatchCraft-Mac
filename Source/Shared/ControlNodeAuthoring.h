#pragma once

#include "PatchCraftProject.h"

namespace patchcraft
{
    struct ControlNodeDefinition
    {
        juce::String id;
        juce::String name;
        juce::String section;
        juce::String type;
        juce::String family;
        juce::String role;
        juce::String ioMode;
        juce::StringArray parameterIds;
        juce::StringArray engines;
        bool modulationSource = false;
    };

    /**
        Target-focused authoring helpers used by the control node editor.
        The editor remains a thin UI over the same DspGraph, parameter model,
        live values, and layout bindings used by exported Players.
    */
    class ControlNodeAuthoring
    {
    public:
        static const std::vector<ControlNodeDefinition>& definitions();
        static std::vector<const ControlNodeDefinition*> definitionsForEngine (const juce::String& engineId);
        static const ControlNodeDefinition* findDefinition (const juce::String& definitionId);
        static const ControlNodeDefinition* definitionForBlock (const DspBlock& block);

        static DspBlock* ensureNode (PatchCraftProject& project, const juce::String& definitionId,
                                     juce::String& status);
        static DspBlock* createNode (PatchCraftProject& project, const juce::String& definitionId,
                                     juce::String& status);
        static bool bindControl (PatchCraftProject& project, const juce::String& elementId,
                                 const juce::String& parameterId, juce::String& status);
        static bool setNodeParameter (PatchCraftProject& project, const juce::String& blockId,
                                      const juce::String& parameterId, float value,
                                      bool notify = true);

        static bool hasModulationRoute (const PatchCraftProject& project,
                                        const juce::String& sourceBlockId,
                                        const juce::String& targetParameterId);
        static bool setModulationRoute (PatchCraftProject& project,
                                        const juce::String& sourceBlockId,
                                        const juce::String& targetParameterId,
                                        bool enabled, float amount, juce::String& status);
        static bool connectNodes (PatchCraftProject& project,
                                  const juce::String& sourceBlockId,
                                  const juce::String& targetBlockId,
                                  juce::String& status);
        static bool disconnectNodes (PatchCraftProject& project,
                                     const juce::String& sourceBlockId,
                                     const juce::String& targetBlockId,
                                     juce::String& status);
        static bool deleteNode (PatchCraftProject& project,
                                const juce::String& blockId,
                                juce::String& status);
        static DspBlock* createNodeAt (PatchCraftProject& project, const juce::String& definitionId,
                                       juce::Point<int> uiPosition, juce::String& status);
        static std::vector<const ControlNodeDefinition*> connectableDefinitions (
            const PatchCraftProject& project, const juce::String& anchorBlockId = {});
        static juce::String sectionCategoryLabel (const juce::String& section);
        static int defaultColumnForSection (const juce::String& section);

        static DspBlock* findBlock (PatchCraftProject& project, const juce::String& blockId);
        static const DspBlock* findBlock (const PatchCraftProject& project, const juce::String& blockId);
        static juce::String storageKeyForParameter (const DspBlock& block, const juce::String& parameterId);
        static float graphValueForParameter (const ParameterDef& definition, const juce::String& parameterId,
                                             float value);

        static juce::StringArray getGraphTemplateNames();
        static bool applyGraphTemplate (PatchCraftProject& project, const juce::String& templateName,
                                        juce::String& status);
        static juce::StringArray getPresetNames();
        static bool applyPreset (PatchCraftProject& project, const juce::String& presetName,
                                 juce::String& status);
        static void tidyGraphLayout (PatchCraftProject& project);
        static std::vector<DspGraphValidationIssue> validateNodeGraph (const PatchCraftProject& project);
        static void syncTemplateLiveValues (PatchCraftProject& project);
    };
}
