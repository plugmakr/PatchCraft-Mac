#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Holds an editable list of ParameterDef entries.
    */
    class ParameterModel
    {
    public:
        ParameterModel();

        void clear()                                 { params.clear(); }
        void addDefaultPalette();              // legacy = sampler defaults
        void loadSamplerPalette();
        void loadSynthPalette();
        void loadEffectPalette();

        std::vector<ParameterDef>&       getAll()       { return params; }
        const std::vector<ParameterDef>& getAll() const { return params; }

        ParameterDef* find (const juce::String& id);
        const ParameterDef* find (const juce::String& id) const;
        bool contains (const juce::String& id) const { return find (id) != nullptr; }

        void add (const ParameterDef& p);
        ParameterDef& addOrUpdate (const ParameterDef& p);
        bool addFromRegistry (const juce::String& id, const juce::String& engineId = {});
        bool remove (const juce::String& id);
        void ensureRegistryMetadata (const juce::String& engineId);

        struct ValidationIssue
        {
            juce::String severity { "error" };
            juce::String ownerId;
            juce::String parameterId;
            juce::String message;

            juce::String toString() const;
        };

        std::vector<ValidationIssue> validateReferences (const std::vector<LayoutElement>& layout,
                                                         const DspGraph& graph,
                                                         const std::vector<Preset>& presets) const;

        static bool getRegistryDefinition (const juce::String& id,
                                           const juce::String& engineId,
                                           ParameterDef& out);
        static juce::StringArray getRegistryIdsForEngine (const juce::String& engineId);

        std::vector<HostParameterSlot> buildHostParameterSlots (
            int maxSlots = kPatchCraftHostParameterSlots) const;

        juce::var toVar() const;
        void fromVar (const juce::var&);

    private:
        std::vector<ParameterDef> params;
    };

} // namespace patchcraft
