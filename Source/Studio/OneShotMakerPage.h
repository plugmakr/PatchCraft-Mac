#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PatchCraftTypes.h"

#include <atomic>
#include <vector>

namespace patchcraft
{
    class StudioMainComponent;
    class SampleWaveformViewer;
    class FloatingPanelWindow;

    /**
        One Shot Maker renders playable WAV one-shots from an imported VST3.

        This is the authoring bridge between third-party instruments and
        PatchCraft sample instruments: load a plugin, choose a musical capture
        template, render notes to disk, then send the results into Sample Mapper.
    */
    class OneShotMakerPage : public juce::Component,
                             public juce::AudioIODeviceCallback,
                             public juce::MidiInputCallback,
                             private juce::Timer,
                             private juce::ListBoxModel
    {
    public:
        explicit OneShotMakerPage (StudioMainComponent& owner);
        ~OneShotMakerPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const*, int, int,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    private:
        struct RenderNote
        {
            int midiNote = 60;
            int velocity = 110;
            int velocityLayerIndex = 0;
            int velocityLayerCount = 1;
            int roundRobinIndex = 0;
            int roundRobinCount = 1;
            juce::String label;
        };

        struct RenderedSample
        {
            int midiNote = 60;
            int velocity = 110;
            int velocityLayerIndex = 0;
            int velocityLayerCount = 1;
            int roundRobinIndex = 0;
            int roundRobinCount = 1;
            juce::String label;
            juce::File file;
        };

        struct RenderSettings
        {
            juce::String packName;
            juce::String creatorName;
            juce::String categoryName;
            juce::String namingScheme;
            juce::String templateName;
            juce::String sourcePluginName;
            juce::String sourcePluginPath;
            juce::String artworkPath;
            juce::File packFolder;
            int templateId = 1;
            double sampleRate = 48000.0;
            int blockSize = 512;
            double bpm = 120.0;
            double bars = 1.0;
            int velocity = 110;
            int tailMs = 750;
            int fadeInMs = 0;
            int fadeOutMs = 25;
            int velocityLayerCount = 1;
            int roundRobinCount = 1;
            bool normalize = true;
            bool trimStartSilence = true;
            bool hardwareCaptureMode = false;
            juce::String hardwareMidiOutputId;
            juce::String hardwareMidiOutputName;
        };

        struct PackLibraryEntry
        {
            juce::File folder;
            juce::File artworkFile;
            juce::String name;
            juce::String creator;
            juce::String category;
            juce::String sourcePlugin;
            juce::String templateName;
            int sampleCount = 0;
            juce::Time modified;
        };

        enum class ViewMode
        {
            Builder,
            Library
        };

        StudioMainComponent& owner;

        juce::AudioPluginFormatManager pluginFormatManager;
        juce::KnownPluginList knownPlugins;
        std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
        std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
        std::unique_ptr<FloatingPanelWindow> floatingPluginWindow;
        juce::Viewport pluginEditorViewport;
        juce::Component pluginEditorHost;
        juce::PluginDescription pluginDescription;
        juce::File pluginFile;
        juce::File artworkFile;
        juce::File outputBaseFolder;
        juce::File lastPackFolder;
        std::vector<RenderedSample> renderedSamples;
        std::vector<SampleZoneDef> renderedZones;
        std::vector<PackLibraryEntry> packLibraryEntries;
        int selectedRenderedIndex = -1;
        int selectedPackLibraryIndex = -1;
        ViewMode viewMode = ViewMode::Builder;

        juce::AudioFormatManager audioFormatManager;
        juce::SpinLock pluginProcessLock;
        juce::AudioBuffer<float> pluginProcessBuffer;
        juce::MidiBuffer liveMidiBuffer;
        juce::CriticalSection liveMidiLock;
        double livePluginSampleRate = 48000.0;
        int livePluginBlockSize = 512;
        int livePluginChannels = 2;
        bool audioCallbackRegistered = false;
        bool livePluginCallbackActive = false;
        bool hardwareCaptureCallbackActive = false;
        std::atomic<bool> livePluginEnabled { false };
        std::atomic<bool> livePluginPrepared { false };
        std::atomic<float> livePluginPeak { 0.0f };
        bool resumeLivePluginAfterRender = false;
        juce::CriticalSection hardwareCaptureLock;
        juce::AudioBuffer<float> hardwareCaptureBuffer;
        std::atomic<bool> hardwareCaptureActive { false };
        std::atomic<int> hardwareCaptureWritePosition { 0 };
        double hardwareCaptureSampleRate = 48000.0;
        juce::AudioBuffer<float> reviewBuffer;
        double reviewSampleRate = 48000.0;
        double reviewOutputSampleRate = 48000.0;
        double reviewReadPosition = 0.0;
        int reviewPlayStart = 0;
        int reviewPlayEnd = 0;
        SampleZoneDef reviewZone;
        juce::SpinLock reviewLock;
        std::atomic<bool> reviewPreviewPlaying { false };
        std::atomic<bool> reviewPreviewFinished { false };
        bool reviewCallbackActive = false;

        std::atomic<bool> rendering { false };
        std::atomic<float> renderProgress { 0.0f };

        juce::Label title;
        juce::Label subtitle;
        juce::Label pluginSectionTitle;
        juce::Label settingsSectionTitle;
        juce::Label exportSectionTitle;
        juce::Label planSectionTitle;
        juce::Label pluginEditorTitle;
        juce::Label pluginEditorStatusLabel;
        juce::Label sampleEditorTitle;
        juce::Label sampleEditorStatusLabel;
        juce::Label pluginPathLabel;
        juce::Label pluginStatusLabel;
        juce::Label outputPathLabel;
        juce::Label planLabel;
        juce::Label statusLabel;

        juce::TextButton builderViewButton { "Builder" };
        juce::TextButton libraryViewButton { "Pack Library" };
        juce::TextButton refreshLibraryButton { "Refresh Library" };
        juce::TextButton openLibraryPackButton { "Open Folder" };
        juce::TextButton loadLibraryPackButton { "Load Pack" };
        juce::TextButton previewLibraryPackButton { "Preview / Edit" };
        juce::TextButton publishLibraryPackButton { "Publish Pack" };
        juce::ListBox packLibraryList { "One Shot Pack Library", this };

        juce::TextButton importPluginButton { "Import VST3..." };
        juce::TextButton chooseOutputButton { "Choose Output..." };
        juce::TextButton chooseArtworkButton { "Artwork..." };
        juce::TextButton renderButton { "Render One Shot Pack" };
        juce::TextButton buildBundleButton { "Build Commercial Bundle" };
        juce::TextButton sendToMapperButton { "Send to Sample Mapper" };
        juce::TextButton createPatchButton { "Create Patch From Pack" };
        juce::TextButton publishPackButton { "Publish" };
        juce::TextButton refreshPluginEditorButton { "Open VST UI" };
        juce::TextButton auditionPluginButton { "Audition Current Sound" };
        juce::TextButton floatPluginEditorButton { "Float Editor" };
        juce::TextButton closePluginEditorButton { "Close Editor" };
        juce::ToggleButton livePluginToggle { "Live Monitor" };
        juce::ToggleButton hardwareCaptureToggle { "Hardware Input" };
        juce::Label hardwareMidiOutputLabel;
        juce::ComboBox hardwareMidiOutputBox;
        juce::TextButton previewSampleButton { "Preview One Shot" };
        juce::TextButton stopSampleButton { "Stop" };

        juce::Label packNameLabel;
        juce::TextEditor packNameEditor;
        juce::Label creatorLabel;
        juce::TextEditor creatorEditor;
        juce::Label categoryLabel;
        juce::ComboBox categoryBox;
        juce::Label namingLabel;
        juce::ComboBox namingSchemeBox;
        juce::Label templateLabel;
        juce::ComboBox templateBox;
        juce::Label barLengthLabel;
        juce::ComboBox barLengthBox;
        juce::Label sampleRateLabel;
        juce::ComboBox sampleRateBox;
        juce::Label bpmLabel;
        juce::Slider bpmSlider;
        juce::Label velocityLabel;
        juce::Label velocityLayersLabel;
        juce::Label roundRobinLabel;
        juce::Slider velocitySlider;
        juce::ComboBox velocityLayersBox;
        juce::ComboBox roundRobinBox;
        juce::Label tailLabel;
        juce::Slider tailSlider;
        juce::ToggleButton normalizeToggle { "Normalize WAVs" };
        juce::ToggleButton trimStartToggle { "Trim leading silence" };
        juce::ToggleButton replaceMapperToggle { "Replace existing sample map" };
        juce::Label renderedSampleLabel;
        juce::ComboBox renderedSampleBox;
        juce::Label editGainLabel;
        juce::Slider editGainSlider;
        juce::Label fadeInLabel;
        juce::Slider fadeInSlider;
        juce::Label fadeOutLabel;
        juce::Slider fadeOutSlider;
        juce::ToggleButton editReverseToggle { "Reverse" };
        juce::TextEditor renderLog;
        std::unique_ptr<SampleWaveformViewer> sampleWaveform;

        juce::Rectangle<int> pluginCard;
        juce::Rectangle<int> settingsCard;
        juce::Rectangle<int> exportCard;
        juce::Rectangle<int> logCard;
        juce::Rectangle<int> pluginEditorCard;
        juce::Rectangle<int> sampleEditorCard;
        juce::Rectangle<int> libraryCard;
        juce::Rectangle<int> progressBounds;

        void setupLabel (juce::Label&, const juce::String& text, float size, bool bold, juce::Colour colour);
        void setupFieldLabel (juce::Label&, const juce::String& text);
        void setupSlider (juce::Slider&, double min, double max, double interval, double value, const juce::String& suffix);
        void drawCard (juce::Graphics&, juce::Rectangle<int>, juce::Colour accent) const;
        void setViewMode (ViewMode);
        void setBuilderControlsVisible (bool);

        void choosePlugin();
        void chooseOutputFolder();
        void chooseArtwork();
        void loadPlugin (const juce::File&);
        void rebuildPluginEditor();
        void destroyPluginEditor();
        void floatPluginEditor();
        void redockPluginEditor();
        void closePluginEditor();
        void syncPluginEditorHostSize();
        void startLivePluginHost();
        void stopLivePluginHost (bool resetPlugin);
        void ensureSharedAudioCallback();
        void releaseSharedAudioCallbackIfIdle();
        void prepareLivePluginForDevice (juce::AudioIODevice*);
        void populateHardwareMidiOutputs();
        void auditionCurrentPluginSound();
        void updateRenderPlan();
        void setControlsEnabledForRenderState();

        std::vector<RenderNote> buildRenderPlan() const;
        RenderSettings currentRenderSettings() const;
        juce::File defaultOutputFolder() const;
        juce::String selectedPackName() const;
        juce::String selectedCreatorName() const;
        juce::String selectedCategoryName() const;
        juce::String selectedTemplateName() const;
        juce::String selectedNamingScheme() const;
        double selectedBarLength() const;
        double selectedSampleRate() const;
        juce::File fileForRenderedNote (const RenderSettings&, const RenderNote&, int index) const;
        juce::String fileStemForRenderedNote (const RenderSettings&, const RenderNote&, int index) const;

        void renderPack();
        bool renderPackToFolder (const RenderSettings&, const std::vector<RenderNote>&,
                                 std::vector<RenderedSample>& rendered, juce::String& error);
        bool renderSingleNote (const RenderSettings&, const RenderNote&, const juce::File&, juce::String& error);
        bool renderPreparedSingleNote (const RenderSettings&, const RenderNote&, const juce::File&,
                                       bool resetPluginState, juce::String& error);
        bool renderHardwareSingleNote (const RenderSettings&, const RenderNote&, const juce::File&,
                                       juce::String& error);
        bool writeWavFile (const juce::File&, juce::AudioBuffer<float>&, int numSamples,
                           double sampleRate, juce::String& error) const;
        bool writeMetadata (const RenderSettings&, const std::vector<RenderedSample>&, juce::String& error) const;
        bool writeCurrentPackMetadata (juce::String& error) const;
        bool copyArtworkToPack (const juce::File& packFolder, const juce::File& sourceArtwork,
                                juce::String& relativePath, juce::String& error) const;
        void buildCommercialBundle();
        bool mapRenderedSamplesToProject (bool switchToSampleMapper, juce::String& error);
        void createPatchFromOneShotPack();
        void publishOneShotPackToPluginClub();
        void refreshPackLibrary();
        void scanOneShotPackFolder (const juce::File& folder);
        bool loadPackLibraryEntry (int index, bool switchToBuilder = true);
        void openSelectedPackFolder();
        void loadSelectedLibraryPackForAudition();
        void previewSelectedLibraryPack();
        void publishSelectedLibraryPack();
        bool saveLoadedPackLibraryMetadata (const juce::String& name,
                                            const juce::String& creator,
                                            const juce::String& category,
                                            const juce::String& description,
                                            const juce::File& artworkSource,
                                            juce::String& error);
        void previewLibraryKeyboardNote (int midiNote);
        void finishRender (bool ok, std::vector<RenderedSample>, juce::File packFolder, juce::String error);
        void sendToSampleMapper();
        SampleZoneDef makeZoneForRenderedSample (const RenderedSample&, int index) const;
        void populateRenderedSampleBox();
        void loadRenderedSampleForReview (int index);
        void loadReviewFile (const juce::File&, int midiNote, const juce::String& label, bool renderedPackSample);
        bool readAudioFile (const juce::File&, juce::AudioBuffer<float>& buffer, double& sampleRate, juce::String& error);
        void updateReviewZoneFromWaveform();
        void syncReviewControlsFromZone (const SampleZoneDef&);
        void startReviewPreview();
        void stopReviewPreview();
        void appendLog (const juce::String&);
        void showMessage (const juce::String& titleText, const juce::String& message, juce::MessageBoxIconType icon);

        void timerCallback() override;
        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged (int lastRowSelected) override;

        static juce::String noteName (int midiNote);
        static int midiCForOctave (int octave);
        static juce::String legalStem (juce::String text);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OneShotMakerPage)
    };
}
