#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Canonical "Sound Stack" used by factory demos and new projects.

        Source → Tone (shape) → Space (fx) — optional Motion blocks added separately.
        See docs/DSP_SIMPLE_STACK.md.
    */
    namespace SoundStack
    {
        enum class MotionKind
        {
            Arp,
            DrumMachine,
            CircleSequencer
        };

        /** Factory-style 3-block graph: no edges, macros, or mod matrix. */
        void resetSimpleGraph (DspGraph& graph, const juce::String& engineId);

        /** Legacy expanded graph (multi-block, edges, macros, LFO, automation). */
        void resetExpandedGraph (DspGraph& graph, const juce::String& engineId);

        bool isMotionBlock (const DspBlock& block);
        bool hasMotionBlock (const DspGraph& graph);
        bool usesAdvancedGraphFeatures (const DspGraph& graph);

        /** Inserts a motion block if one of that kind is not already present. */
        bool addMotionBlock (DspGraph& graph, MotionKind kind, juce::String& error);
    };
}
