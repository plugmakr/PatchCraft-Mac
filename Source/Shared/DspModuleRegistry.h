#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Central registry of supported DSP block types.

        Replaces ad-hoc string-token sniffing in graph validation with an
        explicit, queryable catalogue of first-class modules. Every block type
        that should pass export validation without a "generic Player routing"
        warning must be registered here.
    */
    struct DspModuleDescriptor
    {
        juce::String typeId;
        juce::String displayName;
        DspNodeKind  kind = DspNodeKind::unknown;
        juce::String defaultSection;
        juce::StringArray aliases;
        juce::StringArray engines;
        bool hasPlayerRuntime = true;
    };

    class DspModuleRegistry
    {
    public:
        static const std::vector<DspModuleDescriptor>& all();

        /** Exact (case-insensitive) match on typeId or aliases. */
        static const DspModuleDescriptor* findByType (const juce::String& type);

        /** True when the block type is registered and matches the node kind. */
        static bool isBlockSupported (const TypedDspNode& node);

        /** Resolve node kind from registry, falling back to section heuristics. */
        static DspNodeKind classifyBlockKind (const DspBlock& block);

        static juce::StringArray allTypeIdsForKind (DspNodeKind kind);
    };

} // namespace patchcraft
