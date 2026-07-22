#pragma once

#include "IInstrumentEngine.h"
#include "PatchCraftTypes.h"
#include "RenderContext.h"

#include <array>
#include <cstdint>
#include <vector>

namespace patchcraft
{
    class MidiPlaygroundRuntime
    {
    public:
        void bind (const DspGraph&);
        bool isEnabled() const noexcept { return enabled; }

        void reset();
        void allNotesOff (IInstrumentEngine&);

        bool handleNoteOn (IInstrumentEngine&, int midiNote, float velocity);
        bool handleNoteOff (IInstrumentEngine&, int midiNote);
        void process (IInstrumentEngine&, const RenderContext&);
        void process (IInstrumentEngine&, const RenderContext&, IInstrumentEngine* sampleEngine);
        int getCurrentStep() const noexcept { return currentStep; }
        double getPlaybackPosition01 (int stepsPerCycle) const noexcept
        {
            const int steps = juce::jlimit (1, kMaxSteps, stepsPerCycle);
            if (currentStep < 0)
                return -1.0;

            return juce::jlimit (0.0, 0.999999,
                ((double) juce::jlimit (0, steps - 1, currentStep) + (phase - std::floor (phase))) / (double) steps);
        }

    private:
        static constexpr int kMaxSteps = 128;
        static constexpr int kMaxChordNotes = 16;
        static constexpr int kMaxPhraseBanks = 5;
        static constexpr int kMaxDrumPatterns = 8;
        static constexpr int kMaxDrumTracks = 16;
        static constexpr int kMaxDrumSteps = 64;
        static constexpr int kDrumFxTargetCount = 9;

        struct Settings
        {
            bool drumMachine = false;
            float rate = 1.0f;
            bool sync = true;
            int steps = 8;
            int activeBank = 0;
            bool multiLane = false;
            int pattern = 0;
            int polymeterSteps = 0;
            float gate = 0.55f;
            int octaves = 2;
            float swing = 0.0f;
            float probability = 1.0f;
            bool laneMuted = false;
            float humanize = 0.0f;
            float mutation = 0.0f;
            float velocityCurve = 0.0f;
            bool octaveFold = false;
            int ratchet = 1;
            float strum = 0.0f;
            float flam = 0.0f;
            int echoRepeats = 0;
            float echoDelay = 0.18f;
            float echoDecay = 0.55f;
            float patternMorph = 0.0f;
            bool retrigger = true;
            int euclideanPulses = 0;
            int euclideanRotate = 0;
            bool keySwitchEnabled = false;
            int keySwitchBase = 24;
            int scaleRoot = 0;
            int scaleType = 0;
            int chordMode = 0;
            int chordSize = 1;
            float chordSpread = 0.0f;
            bool latch = false;
            bool sampleControl = false;
            int sampleSliceCount = 1;
            float sampleStart = 0.0f;
            float sampleLength = 1.0f;
            float samplePitch = 0.0f;
            uint32_t seed = 0x50434d49u;
            std::array<float, kMaxSteps> notes {};
            std::array<float, kMaxSteps> velocities {};
            std::array<float, kMaxSteps> gates {};
            std::array<float, kMaxSteps> active {};
            std::array<float, kMaxSteps> probabilities {};
            std::array<float, kMaxSteps> sampleSlices {};
            std::array<float, kMaxSteps> stepDivisions {};
            std::array<float, kMaxSteps> stepDelays {};
            std::array<float, kMaxSteps> stepTransposes {};
            std::array<float, kMaxSteps> stepChordModes {};
            std::array<float, kMaxSteps> stepChordSizes {};
            std::array<float, kMaxSteps> stepTies {};
            std::array<float, kMaxPhraseBanks> bankHasData {};
            std::array<float, kMaxPhraseBanks> bankMuted {};
            std::array<float, kMaxPhraseBanks> bankSolo {};
            std::array<float, kMaxPhraseBanks> bankStepCounts {};
            std::array<float, kMaxPhraseBanks> bankRates {};
            std::array<float, kMaxPhraseBanks> bankSampleControl {};
            std::array<float, kMaxPhraseBanks> bankSampleSliceCounts {};
            std::array<float, kMaxPhraseBanks> bankRetrigger {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankNotes {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankVelocities {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankGates {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankActive {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankProbabilities {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankSampleSlices {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepDivisions {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepDelays {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepTransposes {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepChordModes {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepChordSizes {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankStepTies {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankAutoFxSends {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankAutoFilters {};
            std::array<float, kMaxPhraseBanks * kMaxSteps> bankAutoPans {};
            std::array<float, kMaxPhraseBanks> bankAutoFxTargets {};
            std::array<float, kMaxPhraseBanks> bankLaneTargets {}; // 0=notes, 1=filter, 2=pan, 3=fx, 4=slice
            int drumTracks = 8;
            int drumSteps = 16;
            int drumPattern = 0;
            bool drumTransport = true;
            bool drumSongMode = false;
            int drumChainLength = 1;
            std::array<int, kMaxDrumPatterns> drumChain {};
            std::array<int, kMaxDrumTracks> drumNotes {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumActive {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumVelocities {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumGates {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumProbabilities {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumDivisions {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumFxTargets {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumFxAmounts {};
            std::array<float, kMaxDrumPatterns * kMaxDrumTracks * kMaxDrumSteps> drumSampleSlices {};
            std::array<float, kMaxDrumTracks> drumTrackFxTargets {};
            std::array<float, kMaxDrumTracks> drumTrackFxAmounts {};
            std::array<float, kMaxDrumTracks> drumTrackMuted {};
            std::array<float, kMaxDrumTracks> drumTrackSolo {};
        };

        bool enabled = false;
        Settings settings;
        std::vector<int> heldNotes;
        std::array<float, 128> heldVelocities {};
        std::array<int, kMaxChordNotes> activeNotes {};
        int activeNoteCount = 0;
        float activeVelocity = 0.0f;
        double phase = 0.0;
        int currentStep = -1;
        int currentRatchetSlot = -1;
        bool gateOpen = false;
        uint32_t cycleCounter = 0;
        std::array<int, kMaxChordNotes> pendingNotes {};
        std::array<double, kMaxChordNotes> pendingPhases {};
        std::array<float, kMaxChordNotes> pendingVelocities {};
        int pendingNoteCount = 0;
        int pendingNoteIndex = 0;
        int pendingStep = -1;
        std::array<int, kMaxDrumTracks> activeDrumNotes {};
        std::array<double, kMaxDrumTracks> activeDrumGateEnds {};
        std::array<int, kMaxDrumTracks> activeDrumSubSlots {};
        std::array<float, kDrumFxTargetCount> drumFxState {};
        std::array<std::array<int, kMaxChordNotes>, kMaxPhraseBanks> activeBankNotes {};
        std::array<int, kMaxPhraseBanks> activeBankNoteCounts {};
        std::array<float, kMaxPhraseBanks> activeBankVelocities {};
        std::array<bool, kMaxPhraseBanks> bankGateOpen {};
        std::array<double, kMaxPhraseBanks> bankPhases {};
        std::array<int, kMaxPhraseBanks> currentBankSteps {};
        std::array<int, kMaxPhraseBanks> currentBankRatchetSlots {};

        static bool isMidiPlaygroundBlock (const DspBlock&);
        static float valueForKey (const DspBlock&, const juce::String&, float fallback);
        static uint32_t hash (uint32_t);
        static int drumIndex (int track, int step);
        static int drumPatternIndex (int pattern, int track, int step);
        static int bankStepIndex (int bank, int step);

        void loadRuntimeBank (int bank);
        int activeStepCount() const;
        int activeDrumPattern() const;
        int sequenceIndexForStep (int step) const;
        int baseNoteForSequenceIndex (int index) const;
        int quantizeToScale (int midiNote) const;
        int scaleDegreeToSemitone (int degree) const;
        bool bankIsAudible (int bank) const;
        bool stepIsEnabled (int step) const;
        bool bankStepIsEnabled (int bank, int step) const;
        bool stepPassesEuclideanMask (int step) const;
        bool stepPassesProbability (int step) const;
        bool bankStepPassesProbability (int bank, int step) const;
        float velocityForStep (int step) const;
        float velocityForBankStep (int bank, int step);
        void buildStepNotes (int step, std::array<int, kMaxChordNotes>& notes, int& count) const;
        void buildBankStepNotes (int bank, int step, std::array<int, kMaxChordNotes>& notes, int& count);
        void applySampleControl (IInstrumentEngine&, int step) const;
        void applyBankSampleControl (IInstrumentEngine&, int bank, int step) const;
        void applyDrumSampleControl (IInstrumentEngine&, size_t index) const;
        void applyBankStepFx (IInstrumentEngine&, int bank, int step, float velocity);
        void triggerPendingNotes (IInstrumentEngine&, int step, double stepPhase);
        void stopActive (IInstrumentEngine&);
        IInstrumentEngine& engineForBank (IInstrumentEngine&, IInstrumentEngine* sampleEngine, int bank) const;
        void stopActiveBank (IInstrumentEngine&, int bank, IInstrumentEngine* sampleEngine = nullptr);
        void stopActiveBanks (IInstrumentEngine&, IInstrumentEngine* sampleEngine = nullptr);
        void stopActiveDrums (IInstrumentEngine&, IInstrumentEngine* sampleEngine = nullptr);
        void processDrumMachine (IInstrumentEngine&, const RenderContext&, IInstrumentEngine* sampleEngine = nullptr);
        void processMultiLane (IInstrumentEngine&, const RenderContext&, IInstrumentEngine* sampleEngine = nullptr);
        void triggerDrumFx (IInstrumentEngine&, int target, float amount, float velocity);
        void applyDrumFxState (IInstrumentEngine&);
        void startStep (IInstrumentEngine&, int step, double stepPhase, double swingDelay, double stepGate);
        void startBankStep (IInstrumentEngine&, int bank, int step, IInstrumentEngine* sampleEngine = nullptr);
    };
}
