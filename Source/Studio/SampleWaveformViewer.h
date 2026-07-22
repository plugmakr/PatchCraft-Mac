#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Waveform viewer component for sample editing.
        Displays the sample waveform with loop points, fade regions,
        and sample start/end markers. Supports zooming and navigation.
    */
    class SampleWaveformViewer : public juce::Component,
                                   public juce::ChangeListener,
                                   private juce::ScrollBar::Listener
    {
    public:
        SampleWaveformViewer();
        ~SampleWaveformViewer() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

        void setSampleData (const juce::AudioBuffer<float>& buffer, double sampleRate);
        void clearSampleData();
        void setZone (const SampleZoneDef& zone);
        SampleZoneDef getZone() const { return currentZone; }

        void setLoopEnabled (bool enabled) { currentZone.loopEnabled = enabled; repaint(); }
        void setLoopStart (int start) { currentZone.loopStart = start; repaint(); }
        void setLoopEnd (int end) { currentZone.loopEnd = end; repaint(); }
        void setSampleStart (int start) { currentZone.sampleStart = start; repaint(); }
        void setSampleEnd (int end) { currentZone.sampleEnd = end; repaint(); }
        void setFadeInStart (int start) { currentZone.fadeInStart = start; repaint(); }
        void setFadeInLength (int length) { currentZone.fadeInLength = length; repaint(); }
        void setFadeOutStart (int start) { currentZone.fadeOutStart = start; repaint(); }
        void setFadeOutLength (int length) { currentZone.fadeOutLength = length; repaint(); }

        void setZoomLevel (double zoom);
        double getZoomLevel() const { return zoomLevel; }
        void setViewOffset (int offset);
        int getViewOffset() const { return viewOffset; }
        int getSampleLength() const { return sampleBuffer.getNumSamples(); }
        double getSampleRate() const { return sampleRate; }
        void setBeatSnapEnabled (bool enabled);
        void setBeatGrid (double bpm, int divisionsPerBeat);

        std::function<void()> onZoneChanged;

    private:
        juce::AudioBuffer<float> sampleBuffer;
        double sampleRate = 44100.0;
        SampleZoneDef currentZone;
        double zoomLevel = 1.0;
        int viewOffset = 0;

        juce::Image cachedWaveform;
        bool waveformNeedsUpdate = true;
        bool beatSnapEnabled = false;
        double beatSnapBpm = 120.0;
        int beatSnapDivisionsPerBeat = 4;

        enum class DragMode { None, LoopStart, LoopEnd, SampleStart, SampleEnd,
                            FadeInStart, FadeInLength, FadeOutStart, FadeOutLength, View };
        DragMode dragMode = DragMode::None;
        int dragStartX = 0;
        int dragStartValue = 0;

        // Bottom strip carries the horizontal scrollbar so the user can pan
        // long samples that overflow the viewport at high zoom.
        juce::ScrollBar horizontalScrollbar { false };
        bool suppressScrollbarCallback = false;
        DragMode hoverHandle = DragMode::None;
        juce::Rectangle<int> waveformArea;

        void updateWaveformCache();
        void drawWaveform (juce::Graphics& g, juce::Rectangle<int> area);
        void drawLoopRegion (juce::Graphics& g, juce::Rectangle<int> area);
        void drawFadeRegions (juce::Graphics& g, juce::Rectangle<int> area);
        void drawSampleBounds (juce::Graphics& g, juce::Rectangle<int> area);
        void drawBeatGrid (juce::Graphics& g, juce::Rectangle<int> area);

        int sampleToX (int sample) const;
        int xToSample (int x) const;

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        DragMode handleAt (juce::Point<int> pos) const;
        int snapToZeroCrossing (int sampleIndex, int searchRadius) const;
        int snapToBeatGrid (int sampleIndex) const;
        void syncScrollbar();
        void drawTimeRuler (juce::Graphics& g, juce::Rectangle<int> rulerArea);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleWaveformViewer)
    };

} // namespace patchcraft
