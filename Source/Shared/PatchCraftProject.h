#pragma once

#include "PatchCraftTypes.h"
#include "ParameterModel.h"
#include "LayoutModel.h"
#include "SampleMap.h"
#include "LiveValueStore.h"

#include <juce_data_structures/juce_data_structures.h>

namespace patchcraft
{
    /**
        In-memory representation of a PatchCraft authoring project.

        Owned by the Studio app. Studio saves to a folder
        (MyInstrument.patchcraftproject/) and exports a runtime pack
        (MyInstrument.patchcraft/) using PatchCraftPackWriter.
    */
    class PatchCraftProject
    {
    public:
        enum class ChangeScope
        {
            structural,
            dspRealtime
        };

        PatchCraftProject();

        // ---- Project I/O ------------------------------------------------------
        bool save (const juce::File& projectFolder, juce::String& error);
        bool load (const juce::File& projectFolder, juce::String& error);

        juce::File getProjectFolder() const                      { return projectFolder; }
        void       setProjectFolder (const juce::File& f)        { projectFolder = f; }

        bool hasUnsavedChanges() const                           { return dirty; }
        void markClean()                                         { dirty = false; }
        void markDirty()                                         { dirty = true; }

        // ---- Data accessors ---------------------------------------------------
        Manifest&        getManifest()                  { return manifest; }
        CanvasSize&      getCanvasSize()                { return canvasSize; }
        ParameterModel&  getParameters()                { return parameters; }
        LayoutModel&     getLayout()                    { return layout; }
        SampleMap&       getSampleMap()                 { return sampleMap; }
        std::vector<Preset>& getPresets()               { return presets; }
        std::vector<InstrumentPatch>& getPatches()       { return patches; }
        std::vector<SectionPreset>& getSectionPresets()  { return sectionPresets; }
        std::vector<ExpansionMetadata>& getExpansions()  { return expansions; }
        std::vector<MidiMapping>& getMidiMappings()      { return midiMappings; }
        DspGraph&        getDspGraph()                  { return dspGraph; }
        LiveValueStore&  getLiveValues()                { return liveValues; }

        const Manifest&        getManifest()      const { return manifest; }
        const CanvasSize&      getCanvasSize()    const { return canvasSize; }
        const ParameterModel&  getParameters()    const { return parameters; }
        const LayoutModel&     getLayout()        const { return layout; }
        const SampleMap&       getSampleMap()     const { return sampleMap; }
        const std::vector<Preset>& getPresets()   const { return presets; }
        const std::vector<InstrumentPatch>& getPatches() const { return patches; }
        const std::vector<SectionPreset>& getSectionPresets() const { return sectionPresets; }
        const std::vector<ExpansionMetadata>& getExpansions() const { return expansions; }
        const std::vector<MidiMapping>& getMidiMappings() const { return midiMappings; }
        const DspGraph&        getDspGraph()      const { return dspGraph; }
        const LiveValueStore&  getLiveValues()    const { return liveValues; }

        // Initialise live values from parameter defaults.
        void resetLiveValuesToDefaults();
        InstrumentPatch captureCurrentPatch (const juce::String& name) const;
        bool applyPatch (const InstrumentPatch& patch);
        bool applyPreset (const Preset& preset);
        SectionPreset captureSectionPreset (const juce::String& section,
                                             const juce::String& name) const;
        bool applySectionPreset (const SectionPreset& preset,
                                 bool replaceCurrentSection,
                                 juce::String& error);
        ExpansionMetadata& ensureExpansion (const juce::String& idOrName);

        // ---- Undo / redo (coarse snapshots) -------------------------------
        juce::UndoManager& getUndoManager()              { return undoManager; }

        // Snapshot the current layout, run mutator, push action onto undo stack.
        // Action is named so the UI can show "Undo: <name>".
        void performLayoutEdit (const juce::String& actionName,
                                std::function<void (LayoutModel&)> mutator);
        void performSampleMapEdit (const juce::String& actionName,
                                   std::function<void (SampleMap&)> mutator);

        bool canUndo() const { return undoManager.canUndo(); }
        bool canRedo() const { return undoManager.canRedo(); }
        void undo();
        void redo();

        // ---- Helpers ----------------------------------------------------------
        void resetToDefaultInstrument();          // sampler default
        // Switch the project's engine. Replaces parameter palette and layout
        // with that engine's template. Destructive - undo via UndoManager.
        void setEngineType (const juce::String& engineId);
        bool loadRuntimePackAsProject (const juce::File& packFolder, juce::String& error);
        juce::String getEngineType() const                  { return manifest.engine; }

        juce::File getAssetsFolder() const;
        juce::File getSamplesFolder() const;
        juce::File getKnobAssetsFolder() const;

        // Background image (relative path inside the project assets folder).
        juce::String backgroundImageRelative;

        // Listener interface --------------------------------------------------
        struct Listener
        {
            virtual ~Listener() = default;
            virtual void projectChanged() {}
            virtual void projectChanged (ChangeScope) { projectChanged(); }
        };
        void addListener (Listener* l)                  { listeners.add (l); }
        void removeListener (Listener* l)               { listeners.remove (l); }
        void notifyChanged (ChangeScope scope = ChangeScope::structural);

    private:
        juce::File   projectFolder;
        bool         dirty = false;

        Manifest        manifest;
        CanvasSize      canvasSize;
        ParameterModel  parameters;
        LayoutModel     layout;
        SampleMap       sampleMap;
        std::vector<Preset> presets;
        std::vector<InstrumentPatch> patches;
        std::vector<SectionPreset> sectionPresets;
        std::vector<ExpansionMetadata> expansions;
        std::vector<MidiMapping> midiMappings;
        DspGraph        dspGraph;
        LiveValueStore  liveValues;
        juce::UndoManager undoManager;

        juce::ListenerList<Listener> listeners;
    };

} // namespace patchcraft
