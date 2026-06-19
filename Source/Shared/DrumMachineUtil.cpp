#include "DrumMachineUtil.h"

namespace patchcraft
{
    namespace DrumMachineUtil
    {
        juce::String defaultTrackLabel (int track)
        {
            static const char* labels[] =
            {
                "Kick", "Snare", "Closed Hat", "Open Hat",
                "Clap", "Low Tom", "Perc", "Crash",
                "Ride", "Rim", "Shaker", "FX",
                "Lane 13", "Lane 14", "Lane 15", "Lane 16"
            };
            return track >= 0 && track < 16 ? juce::String (labels[track])
                                            : "Track " + juce::String (track + 1);
        }

        int defaultTrackNote (int track)
        {
            static const int notes[] =
            {
                36, 38, 42, 46, 39, 45, 48, 49,
                51, 37, 44, 52, 53, 54, 55, 56
            };
            return track >= 0 && track < 16 ? notes[track] : 36 + track;
        }

        juce::String cellPrefix (int pattern, int track, int step)
        {
            return "dmP" + juce::String (pattern)
                 + "T" + juce::String (track)
                 + "S" + juce::String (step);
        }

        void setCell (DspBlock& block,
                      int pattern,
                      int track,
                      int step,
                      bool active,
                      float velocity,
                      float gate,
                      float probability,
                      int divisions)
        {
            const auto prefix = cellPrefix (pattern, track, step);
            block.values[prefix + "On"] = active ? 1.0f : 0.0f;
            block.values[prefix + "Vel"] = juce::jlimit (0.01f, 1.0f, velocity);
            block.values[prefix + "Gate"] = juce::jlimit (0.05f, 1.0f, gate);
            block.values[prefix + "Prob"] = juce::jlimit (0.0f, 1.0f, probability);
            block.values[prefix + "Div"] = (float) juce::jlimit (1, 4, divisions);
        }

        void clearPattern (DspBlock& block, int pattern, int tracks, int steps)
        {
            pattern = juce::jlimit (0, 7, pattern);
            tracks = juce::jlimit (1, 16, tracks);
            steps = juce::jlimit (1, 64, steps);
            for (int track = 0; track < tracks; ++track)
                for (int step = 0; step < steps; ++step)
                    setCell (block, pattern, track, step, false, 0.75f, 0.34f);
        }

        void ensureBlockDefaults (DspBlock& block)
        {
            block.type = "drumMachine";
            block.section = "mod";
            block.enabled = true;
            if (block.targetId.isEmpty())
                block.targetId = "sample";
            if (block.name.isEmpty())
                block.name = "Drum Machine";

            block.values["rate"] = block.values.count ("rate") != 0 ? block.values.at ("rate") : 1.0f;
            block.values["sync"] = block.values.count ("sync") != 0 ? block.values.at ("sync") : 1.0f;
            block.values["dmTracks"] = block.values.count ("dmTracks") != 0 ? block.values.at ("dmTracks") : 8.0f;
            block.values["dmSteps"] = block.values.count ("dmSteps") != 0 ? block.values.at ("dmSteps") : 16.0f;
            block.values["dmPattern"] = block.values.count ("dmPattern") != 0 ? block.values.at ("dmPattern") : 0.0f;
            block.values["dmTransport"] = block.values.count ("dmTransport") != 0 ? block.values.at ("dmTransport") : 1.0f;
            block.values["dmTriggerPadSlots"] = block.values.count ("dmTriggerPadSlots") != 0
                ? block.values.at ("dmTriggerPadSlots") : 1.0f;
            block.values["dmSwing"] = block.values.count ("dmSwing") != 0 ? block.values.at ("dmSwing") : 0.0f;
            block.values["dmProbability"] = block.values.count ("dmProbability") != 0
                ? block.values.at ("dmProbability") : 1.0f;
            block.values["dmSongMode"] = block.values.count ("dmSongMode") != 0 ? block.values.at ("dmSongMode") : 0.0f;
            block.values["dmChainLength"] = block.values.count ("dmChainLength") != 0
                ? block.values.at ("dmChainLength") : 4.0f;
            block.values["dmSeed"] = block.values.count ("dmSeed") != 0 ? block.values.at ("dmSeed") : 16001.0f;

            for (int chain = 0; chain < 8; ++chain)
            {
                const auto key = "dmChain" + juce::String (chain);
                if (block.values.find (key) == block.values.end())
                    block.values[key] = (float) juce::jlimit (0, 7, chain);
            }

            for (int track = 0; track < 16; ++track)
            {
                const auto noteKey = "dmTrack" + juce::String (track) + "Note";
                const auto labelKey = "dmTrack" + juce::String (track) + "Label";
                if (block.values.find (noteKey) == block.values.end())
                    block.values[noteKey] = (float) defaultTrackNote (track);
                if (block.metadata.find (labelKey) == block.metadata.end())
                    block.metadata[labelKey] = defaultTrackLabel (track);
            }
        }

        void seedEmptyPatterns (DspBlock& block)
        {
            ensureBlockDefaults (block);
            for (int pattern = 0; pattern < 8; ++pattern)
                clearPattern (block, pattern, 16, 64);
        }

        void seedFactoryPatterns (DspBlock& block)
        {
            seedEmptyPatterns (block);

            for (int step : { 0, 4, 8, 12 }) setCell (block, 0, 0, step, true, 1.0f, 0.42f);
            for (int step : { 4, 12 }) setCell (block, 0, 1, step, true, 0.88f, 0.34f);
            for (int step = 0; step < 16; step += 2)
                setCell (block, 0, 2, step, true, step % 4 == 0 ? 0.72f : 0.58f, 0.18f);
            for (int step : { 7, 15 }) setCell (block, 0, 3, step, true, 0.64f, 0.30f);

            for (int step : { 0, 3, 10, 14 }) setCell (block, 1, 0, step, true, step == 0 ? 1.0f : 0.82f, 0.38f);
            for (int step : { 8 }) setCell (block, 1, 1, step, true, 0.92f, 0.34f);
            for (int step = 0; step < 16; ++step)
                setCell (block, 1, 2, step, true, step % 2 == 0 ? 0.52f : 0.38f, 0.12f,
                         step % 4 == 3 ? 0.70f : 1.0f, step % 4 == 3 ? 2 : 1);
            for (int step : { 11 }) setCell (block, 1, 3, step, true, 0.68f, 0.24f);

            for (int step : { 0, 6, 10 }) setCell (block, 2, 0, step, true, 0.94f, 0.40f);
            for (int step : { 4, 12 }) setCell (block, 2, 1, step, true, 0.90f, 0.34f);
            for (int step : { 2, 5, 7, 9, 11, 13, 15 })
                setCell (block, 2, 2, step, true, 0.60f, 0.16f, 1.0f, (step == 7 || step == 15) ? 3 : 1);
            setCell (block, 2, 6, 0, true, 0.78f, 0.48f);

            for (int step : { 0, 4, 8, 12 }) setCell (block, 3, 0, step, true, 1.0f, 0.38f);
            for (int step : { 4, 12 }) setCell (block, 3, 1, step, true, 0.70f, 0.28f);
            for (int step : { 2, 6, 10, 14 }) setCell (block, 3, 3, step, true, 0.68f, 0.28f);
            for (int step : { 0, 8 }) setCell (block, 3, 7, step, true, 0.44f, 0.50f);
        }
    }
}
