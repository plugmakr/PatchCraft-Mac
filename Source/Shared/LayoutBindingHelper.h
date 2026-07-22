#pragma once

#include "PatchCraftProject.h"

namespace patchcraft
{
    /** True when a layout element needs a real ParameterModel entry (knobs, sliders, etc.). */
    bool layoutElementRequiresParameter (ElementType type);

    /** Performance surfaces (PadGrid, Keyboard, …) use semantic ids — not registry params. */
    bool layoutElementUsesSemanticParameterId (ElementType type);

    /** Resolve or suggest a parameter id when dropping a control on the canvas. */
    juce::String resolveControlParameter (ElementType type,
                                          juce::String hint,
                                          const PatchCraftProject& project);

    /** Clear invalid semantic parameter ids (padGrid, drumTrigger, …). */
    void sanitiseLayoutParameterReferences (PatchCraftProject& project);

    /** Push DSP block default/current values into LiveValueStore so Preview/Export hear them. */
    void syncDspGraphValuesToLiveStore (PatchCraftProject& project);

    /** Ensure a registry parameter exists (creates from engine defaults when missing). */
    bool ensureProjectParameter (PatchCraftProject& project,
                                 const juce::String& paramId,
                                 const juce::String& engineHint = {});

    /** Attach a UI control parameter to an existing Sound Stack / DSP block when possible. */
    void attachControlToExistingRouting (PatchCraftProject& project, const juce::String& paramId);

} // namespace patchcraft
