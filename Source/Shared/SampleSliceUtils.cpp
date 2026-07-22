#include "SampleSliceUtils.h"

#include <algorithm>

namespace patchcraft
{
    static int clampRegionEnd (const SampleZoneDef& zone, int bufferLength)
    {
        const int zoneStart = juce::jlimit (0, juce::jmax (0, bufferLength - 1), zone.sampleStart);
        return zone.sampleEnd > zoneStart
            ? juce::jlimit (zoneStart + 1, bufferLength, zone.sampleEnd)
            : bufferLength;
    }

    std::vector<int> normaliseCuePointBoundaries (const SampleZoneDef& zone,
                                                  int bufferLength,
                                                  const std::vector<int>& boundaries)
    {
        if (boundaries.size() < 2 || bufferLength <= 0)
            return {};

        const int zoneStart = juce::jlimit (0, juce::jmax (0, bufferLength - 1), zone.sampleStart);
        const int zoneEnd = clampRegionEnd (zone, bufferLength);

        std::vector<int> out;
        out.reserve (boundaries.size());
        for (int b : boundaries)
        {
            const int clamped = juce::jlimit (zoneStart, zoneEnd, b);
            if (out.empty() || out.back() != clamped)
                out.push_back (clamped);
        }

        if (out.front() != zoneStart)
            out.insert (out.begin(), zoneStart);
        if (out.back() != zoneEnd)
            out.push_back (zoneEnd);

        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end()), out.end());
        return out.size() >= 2 ? out : std::vector<int>();
    }

    int effectiveSliceCount (const SampleZoneDef& zone, int requestedSliceCount)
    {
        if (zone.cuePoints.size() >= 2)
            return juce::jlimit (1, kMaxChopPads, (int) zone.cuePoints.size() - 1);

        return juce::jlimit (1, 128, requestedSliceCount);
    }

    SampleSliceBounds resolveSampleSliceBounds (const SampleZoneDef& zone,
                                                int bufferLength,
                                                int sliceIndex,
                                                int requestedSliceCount)
    {
        SampleSliceBounds result;
        if (bufferLength <= 0)
            return result;

        const int zoneStart = juce::jlimit (0, juce::jmax (0, bufferLength - 1), zone.sampleStart);
        const int zoneEnd = clampRegionEnd (zone, bufferLength);

        if (zone.cuePoints.size() >= 2)
        {
            auto bounds = zone.cuePoints;
            std::sort (bounds.begin(), bounds.end());
            bounds.erase (std::unique (bounds.begin(), bounds.end()), bounds.end());

            if (bounds.front() > zoneStart)
                bounds.insert (bounds.begin(), zoneStart);
            if (bounds.back() < zoneEnd)
                bounds.push_back (zoneEnd);

            result.sliceCount = juce::jlimit (1, kMaxChopPads, (int) bounds.size() - 1);
            const int idx = juce::jlimit (0, result.sliceCount - 1, sliceIndex);
            result.sliceStart = juce::jlimit (zoneStart, zoneEnd - 1, bounds[(size_t) idx]);
            result.sliceEnd = juce::jlimit (result.sliceStart + 1, zoneEnd, bounds[(size_t) idx + 1]);
            return result;
        }

        const int sliceCount = juce::jlimit (1, 128, requestedSliceCount);
        const int idx = juce::jlimit (0, sliceCount - 1, sliceIndex);
        const int zoneLength = juce::jmax (1, zoneEnd - zoneStart);
        const int rawSliceStart = zoneStart + (zoneLength * idx) / sliceCount;
        const int rawSliceEnd = zoneStart + (zoneLength * (idx + 1)) / sliceCount;
        result.sliceCount = sliceCount;
        result.sliceStart = juce::jlimit (zoneStart, zoneEnd - 1, rawSliceStart);
        result.sliceEnd = juce::jlimit (result.sliceStart + 1, zoneEnd, juce::jmax (result.sliceStart + 1, rawSliceEnd));
        return result;
    }

} // namespace patchcraft
