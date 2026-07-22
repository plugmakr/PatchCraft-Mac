#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /** Maximum chop pads supported by the sample engine (Serato-style grid). */
    constexpr int kMaxChopPads = 32;

    struct SampleSliceBounds
    {
        int sliceStart = 0;
        int sliceEnd = 0;
        int sliceCount = 1;
    };

    /** Resolve playback region for a slice index (cuePoints or equal divisions). */
    SampleSliceBounds resolveSampleSliceBounds (const SampleZoneDef& zone,
                                                int bufferLength,
                                                int sliceIndex,
                                                int requestedSliceCount);

    /** Normalise cue-point boundaries inside the zone's sampleStart/sampleEnd. */
    std::vector<int> normaliseCuePointBoundaries (const SampleZoneDef& zone,
                                                  int bufferLength,
                                                  const std::vector<int>& boundaries);

    /** Slice count implied by cue points, or requestedSliceCount when unset. */
    int effectiveSliceCount (const SampleZoneDef& zone, int requestedSliceCount);

} // namespace patchcraft
