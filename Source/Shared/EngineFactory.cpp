#include "EngineFactory.h"
#include "SampleSynthEngine.h"
#include "SynthEngine.h"
#include "EffectEngine.h"
#include "MultiInstrumentEngine.h"

namespace patchcraft
{
    EngineType engineTypeFromString (const juce::String& s)
    {
        if (s == "synth") return EngineType::Synth;
        if (s == "fx" || s == "effect") return EngineType::Effect;
        return EngineType::Sample;
    }

    juce::String engineTypeToString (EngineType t)
    {
        switch (t) {
            case EngineType::Synth:  return "synth";
            case EngineType::Effect: return "fx";
            case EngineType::Sample: default: return "sample";
        }
    }

    juce::String engineTypeDisplayName (EngineType t)
    {
        switch (t) {
            case EngineType::Synth:  return "Synth";
            case EngineType::Effect: return "Effect";
            case EngineType::Sample: default: return "Sampler";
        }
    }

    std::unique_ptr<IInstrumentEngine> createEngine (EngineType t)
    {
        switch (t) {
            case EngineType::Synth:  return std::make_unique<SynthEngine>();
            case EngineType::Effect: return std::make_unique<EffectEngine>();
            case EngineType::Sample: return std::make_unique<SampleSynthEngine>();
            case EngineType::Multi:  return std::make_unique<MultiInstrumentEngine>();
            default: return std::make_unique<SampleSynthEngine>();
        }
    }

    std::unique_ptr<IInstrumentEngine> createEngineFromManifest (const juce::String& s)
    {
        return createEngine (engineTypeFromString (s));
    }

} // namespace patchcraft
