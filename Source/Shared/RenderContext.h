#pragma once

#include <cmath>
#include <cstdint>

namespace patchcraft
{
    struct RenderContext
    {
        enum class OversamplingMode
        {
            none = 1,
            x2 = 2,
            x4 = 4,
            x8 = 8
        };

        double sampleRate = 44100.0;
        int blockSize = 512;
        int maxBlockSize = 512;
        int inputChannels = 0;
        int outputChannels = 2;

        double bpm = 120.0;
        bool isPlaying = false;
        bool isRecording = false;
        double ppqPosition = 0.0;
        double ppqPositionOfLastBarStart = 0.0;
        double timeInSeconds = 0.0;
        std::int64_t timeInSamples = 0;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;

        OversamplingMode oversamplingMode = OversamplingMode::none;

        static RenderContext forBlock (double sampleRate,
                                       int blockSize,
                                       int maxBlockSize,
                                       int inputChannels,
                                       int outputChannels,
                                       double bpm) noexcept
        {
            RenderContext context;
            context.sampleRate = sanitiseSampleRate (sampleRate);
            context.blockSize = positiveOr (blockSize, 512);
            context.maxBlockSize = positiveOr (maxBlockSize, context.blockSize);
            context.inputChannels = nonNegative (inputChannels);
            context.outputChannels = positiveOr (outputChannels, 1);
            context.bpm = sanitiseBpm (bpm);
            return context;
        }

        double secondsPerBlock() const noexcept
        {
            return (double) blockSize / sanitiseSampleRate (sampleRate);
        }

        double beatsPerBlock() const noexcept
        {
            return secondsPerBlock() * sanitiseBpm (bpm) / 60.0;
        }

        double barLengthInBeats() const noexcept
        {
            return (double) positiveOr (timeSigNumerator, 4) * 4.0
                 / (double) positiveOr (timeSigDenominator, 4);
        }

        int oversamplingFactor() const noexcept
        {
            return (int) oversamplingMode;
        }

        static double sanitiseSampleRate (double value) noexcept
        {
            return std::isfinite (value) && value >= 8000.0 ? value : 44100.0;
        }

        static double sanitiseBpm (double value) noexcept
        {
            if (! std::isfinite (value))
                return 120.0;
            return value < 20.0 ? 20.0 : (value > 999.0 ? 999.0 : value);
        }

        static int positiveOr (int value, int fallback) noexcept
        {
            return value > 0 ? value : fallback;
        }

        static int nonNegative (int value) noexcept
        {
            return value > 0 ? value : 0;
        }
    };
}
