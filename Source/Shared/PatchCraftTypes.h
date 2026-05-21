#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace patchcraft
{
    constexpr int kFormatVersion = 1;
    constexpr int kPatchCraftHostParameterSlots = 128;

    // Manifest -----------------------------------------------------------------
    struct Manifest
    {
        int formatVersion       = kFormatVersion;
        juce::String instrumentName  { "Untitled Instrument" };
        juce::String creator         { "PatchCraft User" };
        juce::String description;
        juce::String category        { "Sample Instrument" };
        juce::String engine          { "sample" };
        juce::String backgroundImage { "assets/background.png" };
        juce::String defaultPreset;
        juce::String createdWith     { "PatchCraft Studio" };

        // Multi-instrument support
        juce::StringArray instrumentIds;      // List of instrument IDs in this pack
        juce::StringArray instrumentNames;    // Display names for each instrument
        juce::StringArray instrumentFiles;   // Relative paths to instrument definition files
        juce::Array<float> instrumentVolumes;
        juce::Array<float> instrumentPans;
        juce::Array<int> instrumentMidiChannels;
        juce::Array<int> instrumentOutputRoutes;
        juce::Array<int> instrumentTransposeSemitones;
        juce::Array<int> instrumentEnabled;
        juce::Array<int> instrumentAutoPlay;
        juce::Array<int> instrumentAutoPlayNotes;
        juce::Array<float> instrumentAutoPlayVelocities;
        bool multiInstrumentMode = false;        // true if pack contains multiple instruments

        // Library metadata for browser display
        juce::String libraryThumbnail { "assets/thumbnail.png" };  // Small image for library grid
        juce::StringArray tags;                                     // Tags for filtering/search
        juce::String version         { "1.0" };
        juce::String website;                                       // Author/product website

        // Runtime Player customization for sellable/white-label instruments.
        juce::String playerDisplayName;
        juce::String playerTagline;
        juce::String playerLogoImage;
        juce::String playerClientName;
        juce::String playerSupportEmail;
        juce::String playerSupportUrl;
        juce::String playerManualUrl;
        juce::String playerStoreUrl;
        juce::String playerCopyright;
        juce::String playerLegalText;
        juce::String salesHeadline;
        juce::String salesSubheadline;
        juce::String salesCtaText { "Buy Now" };
        juce::String salesCheckoutUrl;
        juce::String salesDemoVideoUrl;
        juce::String salesAudioDemoUrl;
        juce::String salesCurrency { "USD" };
        double salesPrice = 0.0;
        double salesCompareAtPrice = 0.0;
        juce::StringArray salesHighlights;
        juce::StringArray salesIncludes;
        juce::StringArray salesFaq;
        juce::Colour playerBackgroundColour { 0xff0b0d10 };
        juce::Colour playerPanelColour      { 0xff15171b };
        juce::Colour playerAccentColour     { 0xfff5a623 };
        juce::Colour playerTextColour       { 0xffe6e6e6 };
        juce::Colour playerTextDimColour    { 0xff8b9098 };
        juce::Colour playerBorderColour     { 0xff2a2a2a };
        bool playerShowPackMenu = true;
        bool playerAllowPackLoading = true;
        bool playerShowLibraryBrowser = true;
        bool playerAllowMidiLearn = true;
        bool playerShowAbout = true;
        bool playerShowParameterGuidance = true;
        bool playerShowPatchCraftBranding = true;
        bool studioShowTutorials = true;

        // White-label packaging / installer metadata for client-ready Player products.
        juce::String whiteLabelPackageName;
        juce::String whiteLabelPublisher;
        juce::String whiteLabelProductCode;
        juce::String whiteLabelBundleIdentifier;
        juce::String whiteLabelInstallerId;
        juce::String whiteLabelInstallerIcon;
        juce::String whiteLabelEulaPath;
        juce::String whiteLabelPrivacyUrl;
        juce::String whiteLabelInstallNotes;
        juce::String whiteLabelWindowsVst3Path { R"(CommonFilesFolder\VST3)" };
        juce::String whiteLabelMacVst3Path { "/Library/Audio/Plug-Ins/VST3" };
        bool whiteLabelRequireLicenseOnFirstRun = false;
        bool whiteLabelIncludeStandalone = true;
        bool whiteLabelIncludeVst3 = true;

        // Licensing / copy protection
        bool licenseRequired = false;
        juce::String licenseKey;
        juce::String licenseProductId;
        juce::String licenseServerUrl;
        juce::String licensePublicKey;
        juce::String licensePolicy { "online-or-offline-grace" };
        int  trialDays = 0;               // 0 = no trial, >0 = trial mode
        bool isTrial = false;
        juce::String trialExpiryDate;     // ISO 8601
        juce::String licenseOwner;
        int  licenseOfflineGraceDays = 14;
        bool licenseBindToMachine = true;
        bool licenseAllowTrialConversion = true;

        juce::var toVar() const;
        static Manifest fromVar (const juce::var&);
    };

    // Canvas size --------------------------------------------------------------
    struct CanvasSize
    {
        int width  = 1280;
        int height = 800;
    };

    // Layout element types ----------------------------------------------------
    enum class ElementType
    {
        Image,
        Knob,
        Slider,
        Button,
        Toggle,
        Dropdown,
        Label,
        ValueDisplay,
        Meter,
        Waveform,
        Keyboard,
        Panel,
        Shape,
        XYPad,
        GranularField,
        TabPanel,
        ScrollPanel,
        Group,
        Separator,
        DrumPad,
        PadGrid,
        DrumGrid,
        Mixer,
        MacroControl,
        ModMatrix,
        EqCurve,
        SpectrumAnalyzer
    };

    juce::String elementTypeToString (ElementType);
    ElementType  elementTypeFromString (const juce::String&);
    juce::String elementTypeDisplayName (ElementType);
    bool isRuntimeControlElement (ElementType);
    bool isPlayerRuntimeElementSupported (ElementType);

    struct LayoutElement
    {
        juce::String id;
        ElementType  type = ElementType::Knob;
        juce::String parameterId;
        juce::String label;
        juce::String style { "Modern Dark" };
        juce::String knobStyle { "Vintage 01" };
        juce::String valueFormat { "Auto" };
        juce::String asset;
        juce::String action;
        juce::String shapeKind { "roundedRect" };
        juce::String labelPosition { "bottom" };
        juce::String containerId;
        juce::String linkedCopyGroupId;

        int x = 100, y = 100, width = 90, height = 90;
        bool visible = true;
        bool locked  = false;
        float opacity = 1.0f;
        float cornerRadius = 12.0f;
        float strokeWidth = 1.0f;
        float shadowAmount = 0.0f;
        float glowAmount = 0.0f;
        float blurAmount = 0.0f;
        float labelOffsetX = 0.0f;
        float labelOffsetY = 0.0f;
        float labelSpacing = 0.0f;
        float labelSize = 0.0f;
        bool  audioReactive = false;
        juce::String audioReactiveMode { "level" };
        float audioReactiveAmount = 0.0f;
        juce::String animationMode { "none" };
        float animationRate = 1.0f;

        juce::Colour textColour       { 0xffe6e6e6 };
        juce::Colour accentColour     { 0xfff5a623 };
        juce::Colour borderColour     { 0xff2a2a2a };
        juce::Colour backgroundColour { 0x00000000 };

        // Group / page membership.
        // - An element with empty groupId is always visible (chrome).
        // - An element with a groupId is only visible when that group is the
        //   canvas's current page. Pages are switched by TabPanel elements.
        juce::String groupId;

        // For TabPanel elements only: ordered list of tab labels. Each tab
        // activates the group whose id is the lower-cased label (spaces ->
        // underscores). The first tab is the default page.
        juce::StringArray tabs;

        // Optional PNG filmstrip override for Knob / Slider / Meter elements.
        // Used by both the Studio canvas preview and the Player runtime UI.
        // - filmstripAsset: relative path inside the project (e.g.
        //   "assets/knobs/vintage_03.png") or an absolute path during authoring.
        // - filmstripFrames: 0 means auto-detect (image_height / image_width).
        juce::String filmstripAsset;
        int          filmstripFrames = 0;
        bool         filmstripVertical = true;   // false = horizontal strip

        // Drum-pad authoring (DrumPad / PadGrid only).
        // PadGrid lays out padRows x padCols pads, with the top-left pad firing
        // padBaseNote and subsequent pads ascending chromatically left->right,
        // top->bottom. A single DrumPad uses padBaseNote as its trigger note.
        int  padRows     = 4;
        int  padCols     = 4;
        int  padBaseNote = 36;          // C1 — matches autoMapDrumPads default

        // Drum-machine pattern UI (DrumGrid only).
        // This mirrors the MIDI Playground drum-machine block: rows are tracks,
        // columns are sequencer steps, and drumPattern selects the visible bank.
        int drumTracks = 8;
        int drumSteps = 16;
        int drumPattern = 0;

        // Runtime mixer UI (Mixer only).
        // auto: multi-instrument layers when present, otherwise main output.
        // layers: force multi-layer mixer behaviour.
        // parameters: use explicit channel parameter assignments below.
        int mixerChannels = 4;
        juce::String mixerMode { "auto" };
        juce::StringArray mixerChannelLabels;
        juce::StringArray mixerVolumeParams;
        juce::StringArray mixerPanParams;
        juce::StringArray mixerMuteParams;
        juce::StringArray mixerSoloParams;

        juce::var toVar() const;
        static LayoutElement fromVar (const juce::var&);

        // Convert a tab label to its group id (e.g. "Main" -> "main").
        static juce::String tabLabelToGroupId (const juce::String& label);
    };

    // Parameters ---------------------------------------------------------------
    struct ParameterDef
    {
        juce::String id;
        juce::String name;
        juce::String type   { "float" };
        juce::String unit;
        float min      = 0.0f;
        float max      = 1.0f;
        float defaultValue = 0.0f;
        float step     = 0.0f;
        float smoothing = 0.02f;
        juce::String category { "General" };
        juce::String section { "global" };
        juce::String displayMode { "continuous" };
        juce::String enabledBy;
        juce::String enableHint;
        bool hostAutomatable = true;
        bool midiLearnable = true;
        bool modulatable = true;
        bool visible = true;

        juce::var toVar() const;
        static ParameterDef fromVar (const juce::var&);
    };

    struct HostParameterSlot
    {
        int slotIndex = -1;
        juce::String slotId;
        juce::String parameterId;
        juce::String name;
        juce::String section;
        juce::String category;
        juce::String unit;
        float min = 0.0f;
        float max = 1.0f;
        float defaultValue = 0.0f;
        bool midiLearnable = true;
        bool overflow = false;

        juce::var toVar() const;
        static HostParameterSlot fromVar (const juce::var&);
    };

    // Sample mappings ----------------------------------------------------------
    struct SampleZoneDef
    {
        juce::String samplePath;        // relative path inside pack
        int  rootNote     = 60;
        int  lowNote      = 0;
        int  highNote     = 127;
        int  lowVelocity  = 1;
        int  highVelocity = 127;
        float gainDb      = 0.0f;
        float pan         = 0.0f;
        bool loopEnabled  = false;
        int  loopStart    = 0;
        int  loopEnd      = 0;

        // HISE-style advanced features
        int  roundRobinGroup = 0;       // 0 = no round robin, 1+ = group ID
        int  roundRobinIndex = 0;       // Position within group
        int  sampleStart    = 0;        // Sample start offset (samples)
        int  sampleEnd      = 0;        // Sample end offset (0 = full sample)
        int  fadeInStart    = 0;        // Fade in start (samples)
        int  fadeInLength   = 0;        // Fade in length (samples)
        int  fadeOutStart   = 0;        // Fade out start (samples)
        int  fadeOutLength  = 0;        // Fade out length (samples)
        float pitchOffset   = 0.0f;     // Pitch offset in semitones
        float keyTracking   = 1.0f;     // 0 = fixed pitch, 1 = normal keyboard tracking, 2 = exaggerated tracking
        float velocityLowerVelXFade = 0; // Velocity crossfade lower zone
        float velocityUpperVelXFade = 0; // Velocity crossfade upper zone
        bool reverse        = false;    // Play sample in reverse
        int  priority       = 0;        // Voice stealing priority
        juce::String group;             // Group name for organization

        // Performance / drum-pad authoring
        int  padIndex       = -1;       // -1 = not assigned, 0..15 = drum pad grid slot
        juce::String padLabel;          // Friendly pad label shown in authoring UI
        int  chokeGroup     = 0;        // 0 = no choke, 1+ = mutually exclusive group
        bool oneShot        = false;    // Ignore note-off and play until sample end
        int  triggerProbability = 100;  // Percent chance a note-on will trigger this zone
        float bpm           = 120.0f;   // BPM for sample playback (120 = default)

        juce::var toVar() const;
        static SampleZoneDef fromVar (const juce::var&);
    };

    // Presets ------------------------------------------------------------------
    struct Preset
    {
        juce::String name;
        juce::String description;
        juce::String theme;
        juce::String patchId;
        juce::String expansionId;
        juce::String packId;
        juce::StringArray libraryReferences;
        juce::StringArray tags;
        bool generated = false;
        bool isDefault = false;
        std::map<juce::String, float> values;

        juce::var toVar() const;
        static Preset fromVar (const juce::var&);
    };

    struct MidiMapping
    {
        juce::String id;
        juce::String parameterId;
        juce::String sourceType { "cc" };
        int channel = 0;       // 0 = any channel, 1..16 = specific channel
        int controller = -1;   // CC number for sourceType == "cc"
        float targetMin = 0.0f;
        float targetMax = 1.0f;
        bool enabled = true;
        bool bipolar = false;

        bool matches (const juce::MidiMessage&) const;
        float normalisedValueFromMessage (const juce::MidiMessage&) const;
        juce::var toVar() const;
        static MidiMapping fromVar (const juce::var&);
    };

    struct DspBlock
    {
        juce::String id;
        juce::String section { "source" };
        juce::String type;
        juce::String name;
        juce::String targetId;
        bool enabled = true;
        std::map<juce::String, float> values;
        std::map<juce::String, juce::String> metadata;

        juce::var toVar() const;
        static DspBlock fromVar (const juce::var&);
    };

    struct MacroAssignment
    {
        juce::String id;
        juce::String macroId;
        juce::String targetId;
        float sourceMin = 0.0f;
        float sourceMax = 1.0f;
        float targetMin = 0.0f;
        float targetMax = 1.0f;
        float curve = 1.0f;
        bool bipolar = false;

        juce::var toVar() const;
        static MacroAssignment fromVar (const juce::var&);
    };

    struct ModRoute
    {
        juce::String id;
        juce::String sourceId;
        juce::String targetId;
        float amount = 0.0f;
        float smoothing = 0.02f;
        bool enabled = true;

        juce::var toVar() const;
        static ModRoute fromVar (const juce::var&);
    };

    struct AutomationLane
    {
        juce::String id;
        juce::String targetId;
        juce::String mode { "loop" };
        float rate = 1.0f;
        bool syncToTempo = true;
        std::vector<float> points;

        juce::var toVar() const;
        static AutomationLane fromVar (const juce::var&);
    };

    enum class DspNodeKind
    {
        unknown,
        source,
        processor,
        modulation,
        analysis,
        output,
        utility
    };

    enum class DspSignalType
    {
        audio,
        modulation,
        event,
        parameter
    };

    struct DspNodePort
    {
        juce::String id;
        juce::String name;
        DspSignalType signalType = DspSignalType::audio;
        bool input = false;

        juce::var toVar() const;
        static DspNodePort fromVar (const juce::var&);
    };

    struct TypedDspNode
    {
        juce::String id;
        juce::String sourceBlockId;
        juce::String section;
        juce::String type;
        juce::String name;
        juce::String targetId;
        DspNodeKind kind = DspNodeKind::unknown;
        bool enabled = true;
        std::vector<DspNodePort> inputs;
        std::vector<DspNodePort> outputs;
        std::map<juce::String, float> values;
        std::map<juce::String, juce::String> metadata;

        juce::var toVar() const;
        static TypedDspNode fromVar (const juce::var&);
    };

    struct DspGraphEdge
    {
        juce::String id;
        juce::String sourceNodeId;
        juce::String sourcePortId { "audioOut" };
        juce::String targetNodeId;
        juce::String targetPortId { "audioIn" };
        DspSignalType signalType = DspSignalType::audio;
        float gain = 1.0f;
        bool enabled = true;

        juce::var toVar() const;
        static DspGraphEdge fromVar (const juce::var&);
    };

    struct DspGraphValidationIssue
    {
        juce::String severity { "warning" };
        juce::String ownerId;
        juce::String message;

        juce::String toString() const;
    };

    struct DspGraph
    {
        std::vector<DspBlock> blocks;
        std::vector<DspGraphEdge> edges;
        std::vector<MacroAssignment> macros;
        std::vector<ModRoute> modulation;
        std::vector<AutomationLane> automation;
        std::map<juce::String, juce::StringArray> quickEditControls;
        bool userConfigured = false;

        void resetForEngine (const juce::String& engineId);
        std::vector<TypedDspNode> buildTypedNodes (const juce::String& engineId = {}) const;
        std::vector<DspGraphEdge> buildAudioEdges (const juce::String& engineId = {}) const;
        std::vector<DspGraphValidationIssue> validateTypedGraph (const juce::String& engineId = {}) const;
        juce::var toVar() const;
        void fromVar (const juce::var&);

        static juce::String nodeKindToString (DspNodeKind);
        static DspNodeKind nodeKindFromString (const juce::String&);
        static juce::String signalTypeToString (DspSignalType);
        static DspSignalType signalTypeFromString (const juce::String&);
    };

    struct InstrumentPatch
    {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String engine { "sample" };
        juce::String category;
        juce::String author;
        juce::String version { "1.0" };
        juce::String packId;
        juce::String expansionId;
        juce::StringArray tags;
        juce::StringArray libraryReferences;
        juce::StringArray includedAssets;
        bool generated = false;
        bool isDefault = false;

        DspGraph dspGraph;
        std::vector<SampleZoneDef> sampleZones;
        std::vector<MidiMapping> midiMappings;
        std::map<juce::String, float> parameterValues;

        Preset toPreset() const;
        juce::var toVar() const;
        static InstrumentPatch fromVar (const juce::var&);
    };

    struct SectionPreset
    {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String section;
        juce::String engine { "sample" };
        juce::String category;
        juce::String packId;
        juce::String expansionId;
        juce::StringArray tags;
        juce::StringArray libraryReferences;

        std::vector<DspBlock> blocks;
        std::vector<DspGraphEdge> edges;
        std::vector<MacroAssignment> macros;
        std::vector<ModRoute> modulation;
        std::vector<AutomationLane> automation;
        std::map<juce::String, float> parameterValues;

        juce::var toVar() const;
        static SectionPreset fromVar (const juce::var&);
    };

    struct ExpansionMetadata
    {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String author;
        juce::String brand;
        juce::String artworkPath;
        juce::String licensePath;
        juce::String category;
        juce::String version { "1.0" };
        juce::String compatibility { "PatchCraft 0.1+" };
        juce::StringArray tags;
        juce::StringArray folders;
        juce::StringArray includedPatchIds;
        juce::StringArray includedPresetNames;
        juce::StringArray includedSectionPresetIds;
        juce::StringArray includedAssets;

        juce::var toVar() const;
        static ExpansionMetadata fromVar (const juce::var&);
    };

} // namespace patchcraft
