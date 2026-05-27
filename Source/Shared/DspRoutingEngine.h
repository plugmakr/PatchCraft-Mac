#pragma once

#include "IInstrumentEngine.h"
#include "ParameterModel.h"

namespace patchcraft
{
    class DspRoutingEngine
    {
    public:
        void prepare (double sampleRate);
        void prepare (const RenderContext& context);
        void reset();

        void bind (const DspGraph& graph, const ParameterModel& parameters);
        void setParameterValue (const juce::String& parameterId, float value);
        bool setFxBlockParameterValue (const juce::String& parameterId, float value);
        void syncFromLiveValues (const class LiveValueStore& liveValues);

        void processToEngine (IInstrumentEngine& engine, int numSamples, double bpm);
        void processToEngine (IInstrumentEngine& engine, const RenderContext& context);
        void captureAudioAnalysis (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    private:
        struct ParamSlot
        {
            juce::String id;
            float min = 0.0f;
            float max = 1.0f;
            float defaultValue = 0.0f;
            float current = 0.0f;
            float routed = 0.0f;
        };

        struct BlockSlot
        {
            DspBlock block;
            double phase = 0.0;
            float heldValue = 0.0f;
        };

        struct LaneSlot
        {
            AutomationLane lane;
            double phase = 0.0;
        };

        struct AudioAnalysisState
        {
            float peak = 0.0f;
            float rms = 0.0f;
            float envelope = 0.0f;
            float transient = 0.0f;
            float spectralCentroid = 0.0f;
            float lowEnergy = 0.0f;
            float midEnergy = 0.0f;
            float highEnergy = 0.0f;
            float lowState = 0.0f;
            float midState = 0.0f;
        };

        std::vector<ParamSlot> params;
        std::vector<BlockSlot> blocks;
        std::vector<TypedDspNode> typedNodes;
        std::vector<DspGraphEdge> edges;
        std::vector<MacroAssignment> macros;
        std::vector<ModRoute> modulation;
        std::vector<LaneSlot> automation;
        AudioAnalysisState audioAnalysis;

        double sampleRate = 44100.0;

        ParamSlot* findParam (const juce::String& id);
        const ParamSlot* findParam (const juce::String& id) const;
        BlockSlot* findBlock (const juce::String& id);
        const BlockSlot* findBlock (const juce::String& id) const;

        float sourceValue (const juce::String& id, const RenderContext& context);
        float blockSignal (BlockSlot& block, const RenderContext& context);
        float laneValue (LaneSlot& lane, const RenderContext& context) const;
        static bool blockSignalIsUnipolar (const DspBlock& block);
        static float valueForKey (const DspBlock& block, const juce::String& key, float fallback);
    };
}
