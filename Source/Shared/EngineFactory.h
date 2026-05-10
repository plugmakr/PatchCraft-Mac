#pragma once

#include "IInstrumentEngine.h"
#include <memory>

namespace patchcraft
{
    enum class EngineType { Sample, Synth, Effect, Multi };

    EngineType  engineTypeFromString (const juce::String&);
    juce::String engineTypeToString  (EngineType);
    juce::String engineTypeDisplayName (EngineType);

    std::unique_ptr<IInstrumentEngine> createEngine (EngineType);
    std::unique_ptr<IInstrumentEngine> createEngineFromManifest (const juce::String& engineString);

} // namespace patchcraft
