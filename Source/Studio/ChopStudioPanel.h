#pragma once



#include <juce_audio_devices/juce_audio_devices.h>

#include <juce_gui_extra/juce_gui_extra.h>



#include "PatchCraftTypes.h"

#include "SampleSliceUtils.h"



namespace patchcraft

{

    class SampleSynthEngine;



    /**

        Serato-style chop editor: waveform slice markers + pad bank audition.

    */

    class ChopStudioPanel : public juce::Component,

                            public juce::AudioIODeviceCallback,

                            private juce::Timer

    {

    public:

        ChopStudioPanel (juce::AudioDeviceManager& deviceManager,

                         std::function<bool (juce::String& errorOut)> ensureAudioFn);

        ~ChopStudioPanel() override;



        void setSource (const juce::AudioBuffer<float>& buffer, double sampleRate,

                        const juce::File& projectFolder, const SampleZoneDef& baseZone);

        void clearSource();



        std::function<void (const std::vector<int>& boundaries, double bpm)> onApply;



        juce::TextButton detectBtn { "Detect BPM & Key" };

        juce::TextButton sliceBeatBtn { "Slice to Beat" };

        juce::TextButton sliceBtn { "Slice to Grid" };

        juce::TextButton transientBtn { "Slice Transients" };

        juce::TextButton playBtn { "Play" };

        juce::TextButton stopBtn { "Stop" };

        juce::TextButton clearBtn { "Clear Slices" };

        juce::TextButton unloadBtn { "Unload Sample" };

        juce::TextButton applyBtn { "Apply Slices" };

        juce::TextButton closeBtn { "Close" };

        juce::ComboBox gridBox;

        juce::Label bpmLabel { {}, "BPM" };

        juce::TextEditor bpmField;



        void resized() override;

        void paint (juce::Graphics&) override;



        void audioDeviceAboutToStart (juce::AudioIODevice*) override;

        void audioDeviceStopped() override;

        void audioDeviceIOCallbackWithContext (const float* const*, int,

                                               float* const*, int, int,

                                               const juce::AudioIODeviceCallbackContext&) override;



    private:

        class ChopWaveformView;

        class ChopPadGrid;



        void timerCallback() override;

        void refreshFromSlices();

        void updateInfo();

        void doDetect();

        void doSliceBeat();

        void doSliceGrid();

        void doTransient();

        void rebuildAuditionEngine();

        void ensureAuditionOpen();

        void stopAudition();

        void playFromPlayhead();

        void auditionSlice (int sliceIndex);

        double currentBpm() const;



        std::unique_ptr<ChopWaveformView> waveform;

        std::unique_ptr<ChopPadGrid> pads;



        juce::Label title;

        juce::Label info;



        juce::AudioBuffer<float> sourceBuffer;

        double sourceRate = 44100.0;

        juce::File projectFolder;

        SampleZoneDef baseZone;

        int regionStart = 0;

        int regionEnd = 1;

        bool hasSource = false;

        std::vector<int> slices;

        double detectedBpm = 0.0;

        juce::String detectedKey;



        int auditionNote = -1;

        int auditionSliceIndex = -1;

        double manualPlayhead = -1.0;



        juce::AudioDeviceManager& deviceManager;

        std::function<bool (juce::String&)> ensureAudio;

        std::unique_ptr<SampleSynthEngine> engine;

        juce::SpinLock engineLock;

        bool callbackActive = false;

        double auditionRate = 44100.0;

        int auditionBlock = 512;

        int auditionChannels = 2;

    };



} // namespace patchcraft

