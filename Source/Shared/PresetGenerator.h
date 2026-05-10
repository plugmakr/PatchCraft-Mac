#pragma once

#include "ParameterModel.h"
#include "PatchCraftTypes.h"
#include "LiveValueStore.h"

namespace patchcraft
{
    struct PresetGenerationOptions
    {
        juce::String theme { "Motion" };
        int count = 16;
        juce::uint32 seed = 0;
        bool includeCurrentAsAnchor = true;
    };

    class PresetGenerator
    {
    public:
        static juce::StringArray themes();
        static std::vector<Preset> generate (const ParameterModel& parameters,
                                             const LiveValueStore& liveValues,
                                             const juce::String& engineId,
                                             const PresetGenerationOptions& options);
    };
}
