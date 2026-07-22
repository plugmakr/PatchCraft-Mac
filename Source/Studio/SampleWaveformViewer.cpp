#include "SampleWaveformViewer.h"
#include "PatchCraftLookAndFeel.h"

#include <cmath>

namespace patchcraft
{
    SampleWaveformViewer::SampleWaveformViewer()
    {
        setOpaque (true);
        addAndMakeVisible (horizontalScrollbar);
        horizontalScrollbar.setAutoHide (false);
        horizontalScrollbar.addListener (this);
    }

    SampleWaveformViewer::~SampleWaveformViewer() = default;

    void SampleWaveformViewer::setSampleData (const juce::AudioBuffer<float>& buffer, double sr)
    {
        sampleBuffer = buffer;
        sampleRate = sr;
        viewOffset = 0;
        zoomLevel = 1.0;
        waveformNeedsUpdate = true;
        syncScrollbar();
        repaint();
    }

    void SampleWaveformViewer::clearSampleData()
    {
        sampleBuffer.setSize (0, 0);
        cachedWaveform = {};
        currentZone = {};
        viewOffset = 0;
        zoomLevel = 1.0;
        waveformNeedsUpdate = true;
        syncScrollbar();
        repaint();
    }

    void SampleWaveformViewer::setZone (const SampleZoneDef& zone)
    {
        const bool reverseChanged = (currentZone.reverse != zone.reverse);
        currentZone = zone;
        if (reverseChanged) waveformNeedsUpdate = true;
        repaint();
    }

    void SampleWaveformViewer::setZoomLevel (double zoom)
    {
        zoomLevel = juce::jlimit (1.0, 200.0, zoom);
        waveformNeedsUpdate = true;
        syncScrollbar();
        repaint();
    }

    void SampleWaveformViewer::setViewOffset (int offset)
    {
        viewOffset = juce::jmax (0, offset);
        waveformNeedsUpdate = true;
        syncScrollbar();
        repaint();
    }

    void SampleWaveformViewer::setBeatSnapEnabled (bool enabled)
    {
        beatSnapEnabled = enabled;
        repaint();
    }

    void SampleWaveformViewer::setBeatGrid (double bpm, int divisionsPerBeat)
    {
        beatSnapBpm = juce::jlimit (20.0, 300.0, bpm);
        beatSnapDivisionsPerBeat = juce::jlimit (1, 16, divisionsPerBeat);
        repaint();
    }

    void SampleWaveformViewer::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        if (sampleBuffer.getNumSamples() == 0)
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (14.0f));
            g.drawText ("No sample loaded", getLocalBounds(), juce::Justification::centred);
            return;
        }

        if (waveformNeedsUpdate)
            updateWaveformCache();

        const auto rulerArea = juce::Rectangle<int> (0, 0, getWidth(), 16);
        drawTimeRuler (g, rulerArea);
        drawWaveform (g, waveformArea);
        drawBeatGrid (g, waveformArea);
        drawLoopRegion (g, waveformArea);
        drawFadeRegions (g, waveformArea);
        drawSampleBounds (g, waveformArea);
    }

    void SampleWaveformViewer::resized()
    {
        // Top: 16px time ruler. Bottom: 14px scrollbar. Middle: waveform.
        auto bounds = getLocalBounds();
        bounds.removeFromTop (16);
        const int scrollH = 14;
        horizontalScrollbar.setBounds (bounds.removeFromBottom (scrollH));
        waveformArea = bounds;
        waveformNeedsUpdate = true;
        syncScrollbar();
    }

    void SampleWaveformViewer::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        waveformNeedsUpdate = true;
        repaint();
    }

    void SampleWaveformViewer::updateWaveformCache()
    {
        if (sampleBuffer.getNumSamples() == 0)
            return;

        const auto w = waveformArea.getWidth();
        const auto h = waveformArea.getHeight();
        if (w <= 0 || h <= 0)
            return;

        cachedWaveform = juce::Image (juce::Image::RGB, w, h, true);
        juce::Graphics g (cachedWaveform);
        g.fillAll (PatchCraftLookAndFeel::bg());

        const int numChannels = sampleBuffer.getNumChannels();
        const int totalSamples = sampleBuffer.getNumSamples();
        const double samplesPerPixel = juce::jmax (1.0,
            (double) totalSamples / juce::jmax (1.0, w * zoomLevel));

        for (int x = 0; x < w; ++x)
        {
            // The reverse flag flips which sample range maps to each column,
            // so the viewer matches what the engine plays when reverse=true.
            const int srcCol = currentZone.reverse ? (w - 1 - x) : x;
            const int startSample = viewOffset + (int) (srcCol * samplesPerPixel);
            const int endSample = juce::jmin (totalSamples, startSample + (int) samplesPerPixel);

            if (startSample >= totalSamples)
                break;

            float minVal = 1.0f;
            float maxVal = -1.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = sampleBuffer.getReadPointer (ch);
                for (int i = startSample; i < endSample; ++i)
                {
                    minVal = juce::jmin (minVal, data[i]);
                    maxVal = juce::jmax (maxVal, data[i]);
                }
            }

            const int y1 = juce::jlimit (0, h, (int) ((1.0f - maxVal) * h * 0.5f));
            const int y2 = juce::jlimit (0, h, (int) ((1.0f - minVal) * h * 0.5f));

            g.setColour (currentZone.reverse
                ? PatchCraftLookAndFeel::accent().interpolatedWith (juce::Colour (0xff8a4fff), 0.5f)
                : PatchCraftLookAndFeel::accent());
            g.drawLine ((float) x, (float) y1, (float) x, (float) y2);
        }

        // Centre line.
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.5f));
        g.drawHorizontalLine (h / 2, 0.0f, (float) w);

        waveformNeedsUpdate = false;
    }

    void SampleWaveformViewer::drawTimeRuler (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (sampleBuffer.getNumSamples() == 0 || r.isEmpty())
            return;

        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (r);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX(), (float) r.getRight());

        const auto sr = juce::jmax (1.0, sampleRate);
        const double totalSec = sampleBuffer.getNumSamples() / sr;
        const double visibleSec = totalSec / juce::jmax (0.001, zoomLevel);
        // Pick a tick step that yields ~6-12 ticks across the visible range.
        const double targets[] = { 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0 };
        double step = 1.0;
        for (double t : targets) { step = t; if (visibleSec / t < 12.0) break; }

        g.setFont (juce::Font (9.5f));
        g.setColour (PatchCraftLookAndFeel::textDim());
        const double startSec = viewOffset / sr;
        const double endSec   = startSec + visibleSec;
        const double firstTick = std::ceil (startSec / step) * step;
        for (double t = firstTick; t <= endSec; t += step)
        {
            const int sampleIdx = (int) (t * sr);
            const int x = sampleToX (sampleIdx);
            if (x < r.getX() || x > r.getRight()) continue;
            g.drawVerticalLine (x, (float) r.getY() + 8.0f, (float) r.getBottom());
            g.drawText (juce::String (t, t < 1.0 ? 3 : 2) + "s",
                        x + 2, r.getY(), 60, r.getHeight() - 2,
                        juce::Justification::centredLeft);
        }
    }

    void SampleWaveformViewer::drawWaveform (juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (cachedWaveform.isValid())
            g.drawImageAt (cachedWaveform, area.getX(), area.getY());
    }

    void SampleWaveformViewer::drawLoopRegion (juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (! currentZone.loopEnabled)
            return;

        const int loopStartX = sampleToX (currentZone.loopStart);
        const int loopEndX   = sampleToX (currentZone.loopEnd);

        if (loopEndX > area.getX() && loopStartX < area.getRight() && loopStartX < loopEndX)
        {
            const juce::Colour loopColour (0xff4CAF50);
            const int clippedStart = juce::jlimit (area.getX(), area.getRight(), loopStartX);
            const int clippedEnd   = juce::jlimit (area.getX(), area.getRight(), loopEndX);
            g.setColour (loopColour.withAlpha (0.2f));
            g.fillRect (clippedStart, area.getY(), clippedEnd - clippedStart, area.getHeight());

            g.setColour (loopColour);
            g.drawVerticalLine (loopStartX, (float) area.getY(), (float) area.getBottom());
            g.drawVerticalLine (loopEndX,   (float) area.getY(), (float) area.getBottom());

            const int handleY = area.getY() + area.getHeight() / 2 - 10;
            const bool hoverStart = hoverHandle == DragMode::LoopStart;
            const bool hoverEnd   = hoverHandle == DragMode::LoopEnd;
            g.setColour (loopColour.brighter (hoverStart ? 0.3f : 0.0f));
            g.fillRect (loopStartX - 4, handleY, 8, 20);
            g.setColour (loopColour.brighter (hoverEnd ? 0.3f : 0.0f));
            g.fillRect (loopEndX - 4,   handleY, 8, 20);
        }
    }

    void SampleWaveformViewer::drawFadeRegions (juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Fade in
        if (currentZone.fadeInLength > 0)
        {
            const int fadeStart = currentZone.fadeInStart > 0 ? currentZone.fadeInStart : currentZone.sampleStart;
            const int fadeInStartX = sampleToX (fadeStart);
            const int fadeInEndX   = sampleToX (fadeStart + currentZone.fadeInLength);

            if (fadeInEndX > area.getX() && fadeInStartX < area.getRight())
            {
                const int clippedStart = juce::jlimit (area.getX(), area.getRight(), fadeInStartX);
                const int clippedEnd   = juce::jlimit (area.getX(), area.getRight(), fadeInEndX);
                g.setColour (juce::Colour (0xff2196F3).withAlpha (0.3f));
                g.fillRect (clippedStart, area.getY(), clippedEnd - clippedStart, area.getHeight());
                g.setColour (juce::Colour (0xff2196F3));
                g.drawVerticalLine (fadeInStartX, (float) area.getY(), (float) area.getBottom());
                g.drawVerticalLine (fadeInEndX,   (float) area.getY(), (float) area.getBottom());
            }
        }

        // Fade out
        if (currentZone.fadeOutLength > 0)
        {
            const int playEnd = currentZone.sampleEnd > 0 ? currentZone.sampleEnd : sampleBuffer.getNumSamples();
            const int fadeStart = currentZone.fadeOutStart > 0
                ? currentZone.fadeOutStart
                : juce::jmax (currentZone.sampleStart, playEnd - currentZone.fadeOutLength);
            const int fadeOutStartX = sampleToX (fadeStart);
            const int fadeOutEndX   = sampleToX (fadeStart + currentZone.fadeOutLength);

            if (fadeOutEndX > area.getX() && fadeOutStartX < area.getRight())
            {
                const int clippedStart = juce::jlimit (area.getX(), area.getRight(), fadeOutStartX);
                const int clippedEnd   = juce::jlimit (area.getX(), area.getRight(), fadeOutEndX);
                g.setColour (juce::Colour (0xffFF9800).withAlpha (0.3f));
                g.fillRect (clippedStart, area.getY(), clippedEnd - clippedStart, area.getHeight());
                g.setColour (juce::Colour (0xffFF9800));
                g.drawVerticalLine (fadeOutStartX, (float) area.getY(), (float) area.getBottom());
                g.drawVerticalLine (fadeOutEndX,   (float) area.getY(), (float) area.getBottom());
            }
        }
    }

    void SampleWaveformViewer::drawBeatGrid (juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (! beatSnapEnabled || sampleBuffer.getNumSamples() <= 0 || area.isEmpty())
            return;

        const double samplesPerBeat = sampleRate * 60.0 / juce::jmax (20.0, beatSnapBpm);
        const double stepSamples = samplesPerBeat / (double) juce::jmax (1, beatSnapDivisionsPerBeat);
        if (stepSamples < 1.0)
            return;

        const int start = juce::jmax (0, viewOffset);
        const int end = juce::jmin (sampleBuffer.getNumSamples(), xToSample (area.getRight()));
        const int firstDivision = juce::jmax (0, (int) std::floor ((double) start / stepSamples));

        for (int division = firstDivision; ; ++division)
        {
            const int sample = juce::roundToInt ((double) division * stepSamples);
            if (sample > end)
                break;

            const int x = sampleToX (sample);
            if (x < area.getX() || x > area.getRight())
                continue;

            const bool beat = division % beatSnapDivisionsPerBeat == 0;
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (beat ? 0.42f : 0.18f));
            g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
        }
    }

    void SampleWaveformViewer::drawSampleBounds (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const int sampleStartX = sampleToX (currentZone.sampleStart);
        const int sampleEndX   = sampleToX (currentZone.sampleEnd > 0 ? currentZone.sampleEnd
                                                                       : sampleBuffer.getNumSamples());

        const juce::Colour boundsColour (0xff9C27B0);
        g.setColour (boundsColour);
        if (sampleStartX >= area.getX() && sampleStartX <= area.getRight())
            g.drawVerticalLine (sampleStartX, (float) area.getY(), (float) area.getBottom());
        if (sampleEndX >= area.getX() && sampleEndX <= area.getRight())
            g.drawVerticalLine (sampleEndX, (float) area.getY(), (float) area.getBottom());

        // Draw handles at the bottom of the waveform area.
        const int handleY = area.getBottom() - 20;
        const bool hoverStart = hoverHandle == DragMode::SampleStart;
        const bool hoverEnd   = hoverHandle == DragMode::SampleEnd;
        if (sampleStartX >= area.getX() && sampleStartX <= area.getRight())
        {
            g.setColour (boundsColour.brighter (hoverStart ? 0.3f : 0.0f));
            g.fillRect (sampleStartX - 4, handleY, 8, 20);
        }
        if (sampleEndX >= area.getX() && sampleEndX <= area.getRight())
        {
            g.setColour (boundsColour.brighter (hoverEnd ? 0.3f : 0.0f));
            g.fillRect (sampleEndX - 4, handleY, 8, 20);
        }

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::FontOptions (10.0f));
        const auto duration = sampleBuffer.getNumSamples() / juce::jmax (1.0, sampleRate);
        g.drawText (juce::String (sampleBuffer.getNumSamples()) + " samples  |  "
                    + juce::String (duration, 2) + "s"
                    + (currentZone.reverse ? "  |  REVERSE" : ""),
                    area.reduced (8), juce::Justification::topRight);
    }

    int SampleWaveformViewer::sampleToX (int sample) const
    {
        const int w = waveformArea.getWidth();
        if (sampleBuffer.getNumSamples() == 0 || w <= 0)
            return waveformArea.getX();
        const double samplesPerPixel = juce::jmax (1.0,
            (double) sampleBuffer.getNumSamples() / juce::jmax (1.0, w * zoomLevel));
        const int srcCol = (int) ((sample - viewOffset) / samplesPerPixel);
        const int col = currentZone.reverse ? (w - 1 - srcCol) : srcCol;
        return waveformArea.getX() + col;
    }

    int SampleWaveformViewer::xToSample (int x) const
    {
        const int w = waveformArea.getWidth();
        if (sampleBuffer.getNumSamples() == 0 || w <= 0)
            return 0;
        const double samplesPerPixel = juce::jmax (1.0,
            (double) sampleBuffer.getNumSamples() / juce::jmax (1.0, w * zoomLevel));
        const int colInArea = x - waveformArea.getX();
        const int srcCol = currentZone.reverse ? (w - 1 - colInArea) : colInArea;
        return juce::jlimit (0, sampleBuffer.getNumSamples(),
                             (int) (viewOffset + srcCol * samplesPerPixel));
    }

    SampleWaveformViewer::DragMode SampleWaveformViewer::handleAt (juce::Point<int> pos) const
    {
        if (sampleBuffer.getNumSamples() <= 0)
            return DragMode::None;
        if (! waveformArea.contains (pos))
            return DragMode::None;

        const int x = pos.x;
        const int threshold = 8;

        const int loopStartX = sampleToX (currentZone.loopStart);
        const int loopEndX   = sampleToX (currentZone.loopEnd);
        const int sampleStartX = sampleToX (currentZone.sampleStart);
        const int sampleEndX   = sampleToX (currentZone.sampleEnd > 0 ? currentZone.sampleEnd
                                                                        : sampleBuffer.getNumSamples());
        const int fadeInStart  = currentZone.fadeInStart > 0 ? currentZone.fadeInStart : currentZone.sampleStart;
        const int playEnd      = currentZone.sampleEnd > 0 ? currentZone.sampleEnd : sampleBuffer.getNumSamples();
        const int fadeOutStart = currentZone.fadeOutStart > 0
            ? currentZone.fadeOutStart
            : juce::jmax (currentZone.sampleStart, playEnd - currentZone.fadeOutLength);
        const int fadeInStartX  = sampleToX (fadeInStart);
        const int fadeInEndX    = sampleToX (fadeInStart + currentZone.fadeInLength);
        const int fadeOutStartX = sampleToX (fadeOutStart);
        const int fadeOutEndX   = sampleToX (fadeOutStart + currentZone.fadeOutLength);

        if (currentZone.loopEnabled && std::abs (x - loopStartX) < threshold) return DragMode::LoopStart;
        if (currentZone.loopEnabled && std::abs (x - loopEndX)   < threshold) return DragMode::LoopEnd;
        if (std::abs (x - sampleStartX) < threshold)                          return DragMode::SampleStart;
        if (std::abs (x - sampleEndX)   < threshold)                          return DragMode::SampleEnd;
        if (currentZone.fadeInLength > 0 && std::abs (x - fadeInStartX) < threshold)  return DragMode::FadeInStart;
        if (currentZone.fadeInLength > 0 && std::abs (x - fadeInEndX)   < threshold)  return DragMode::FadeInLength;
        if (currentZone.fadeOutLength > 0 && std::abs (x - fadeOutStartX) < threshold) return DragMode::FadeOutStart;
        if (currentZone.fadeOutLength > 0 && std::abs (x - fadeOutEndX)   < threshold) return DragMode::FadeOutLength;
        return DragMode::None;
    }

    void SampleWaveformViewer::mouseDown (const juce::MouseEvent& e)
    {
        if (sampleBuffer.getNumSamples() <= 0)
            return;

        const auto h = handleAt (e.getPosition());
        if (h != DragMode::None)
        {
            dragMode = h;
            switch (h)
            {
                case DragMode::LoopStart:     dragStartValue = currentZone.loopStart; break;
                case DragMode::LoopEnd:       dragStartValue = currentZone.loopEnd; break;
                case DragMode::SampleStart:   dragStartValue = currentZone.sampleStart; break;
                case DragMode::SampleEnd:     dragStartValue = currentZone.sampleEnd > 0
                                                  ? currentZone.sampleEnd : sampleBuffer.getNumSamples(); break;
                case DragMode::FadeInStart:   dragStartValue = currentZone.fadeInStart > 0
                                                  ? currentZone.fadeInStart : currentZone.sampleStart; break;
                case DragMode::FadeInLength:  dragStartValue = currentZone.fadeInLength; break;
                case DragMode::FadeOutStart:  dragStartValue = currentZone.fadeOutStart; break;
                case DragMode::FadeOutLength: dragStartValue = currentZone.fadeOutLength; break;
                default: break;
            }
        }
        else
        {
            dragMode = DragMode::View;
        }

        dragStartX = e.getPosition().x;
    }

    void SampleWaveformViewer::mouseDrag (const juce::MouseEvent& e)
    {
        int deltaX = e.getPosition().x - dragStartX;
        int sampleDelta = xToSample (deltaX) - xToSample (0);
        const int length = sampleBuffer.getNumSamples();
        const int playEnd = currentZone.sampleEnd > 0 ? currentZone.sampleEnd : length;

        switch (dragMode)
        {
            case DragMode::LoopStart:
                currentZone.loopStart = juce::jlimit (currentZone.sampleStart, juce::jmax (currentZone.sampleStart + 1, currentZone.loopEnd - 1), dragStartValue + sampleDelta);
                break;
            case DragMode::LoopEnd:
                currentZone.loopEnd = juce::jlimit (currentZone.loopStart + 1, length, dragStartValue + sampleDelta);
                break;
            case DragMode::SampleStart:
                currentZone.sampleStart = juce::jlimit (0, juce::jmax (1, playEnd - 1), dragStartValue + sampleDelta);
                break;
            case DragMode::SampleEnd:
                currentZone.sampleEnd = juce::jlimit (currentZone.sampleStart + 1, length, dragStartValue + sampleDelta);
                break;
            case DragMode::FadeInStart:
                currentZone.fadeInStart = juce::jlimit (currentZone.sampleStart, playEnd, dragStartValue + sampleDelta);
                break;
            case DragMode::FadeInLength:
                currentZone.fadeInLength = juce::jlimit (0, playEnd - currentZone.sampleStart, dragStartValue + sampleDelta);
                break;
            case DragMode::FadeOutStart:
                currentZone.fadeOutStart = juce::jlimit (currentZone.sampleStart, playEnd, dragStartValue + sampleDelta);
                break;
            case DragMode::FadeOutLength:
                currentZone.fadeOutLength = juce::jlimit (0, playEnd - currentZone.sampleStart, dragStartValue + sampleDelta);
                break;
            case DragMode::View:
                viewOffset = juce::jlimit (0, length, viewOffset - sampleDelta);
                waveformNeedsUpdate = true;
                syncScrollbar();
                break;
            default:
                break;
        }

        repaint();

        if (onZoneChanged)
            onZoneChanged();
    }

    void SampleWaveformViewer::mouseUp (const juce::MouseEvent& e)
    {
        // Snap handles on release. Beat snap is explicit; otherwise we snap
        // start/end/loop handles to nearby zero crossings. Hold Shift to keep
        // the exact position the user dragged to.
        if (! e.mods.isShiftDown() && sampleBuffer.getNumSamples() > 0
            && waveformArea.getWidth() > 0)
        {
            const double samplesPerPixel = juce::jmax (1.0,
                (double) sampleBuffer.getNumSamples()
                / juce::jmax (1.0, waveformArea.getWidth() * zoomLevel));
            const int radius = juce::jmax (8, (int) (samplesPerPixel * 6.0));

            auto snap = [&] (int& dst)
            {
                dst = beatSnapEnabled ? snapToBeatGrid (dst)
                                      : snapToZeroCrossing (dst, radius);
            };
            switch (dragMode)
            {
                case DragMode::LoopStart:    snap (currentZone.loopStart); break;
                case DragMode::LoopEnd:      snap (currentZone.loopEnd); break;
                case DragMode::SampleStart:  snap (currentZone.sampleStart); break;
                case DragMode::SampleEnd:    snap (currentZone.sampleEnd); break;
                default: break;
            }
            if (dragMode == DragMode::LoopStart || dragMode == DragMode::LoopEnd
                || dragMode == DragMode::SampleStart || dragMode == DragMode::SampleEnd)
            {
                if (onZoneChanged) onZoneChanged();
                repaint();
            }
        }

        dragMode = DragMode::None;
    }

    void SampleWaveformViewer::mouseMove (const juce::MouseEvent& e)
    {
        const auto h = handleAt (e.getPosition());
        if (h != hoverHandle)
        {
            hoverHandle = h;
            repaint();
        }
        setMouseCursor (h != DragMode::None
            ? juce::MouseCursor::LeftRightResizeCursor
            : juce::MouseCursor::NormalCursor);
    }

    void SampleWaveformViewer::mouseExit (const juce::MouseEvent&)
    {
        if (hoverHandle != DragMode::None)
        {
            hoverHandle = DragMode::None;
            repaint();
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void SampleWaveformViewer::mouseWheelMove (const juce::MouseEvent& e,
                                                const juce::MouseWheelDetails& wheel)
    {
        // Ctrl/Cmd+wheel = zoom (focus on cursor sample). Plain wheel = scroll.
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            if (wheel.deltaY == 0.0f) return;
            const int focusSample = xToSample (e.getPosition().x);
            const double newZoom = juce::jlimit (1.0, 200.0,
                zoomLevel * (wheel.deltaY > 0 ? 1.15 : 1.0 / 1.15));
            zoomLevel = newZoom;
            // Re-anchor viewOffset so focusSample stays under the cursor.
            const double samplesPerPixel = juce::jmax (1.0,
                (double) sampleBuffer.getNumSamples()
                / juce::jmax (1.0, waveformArea.getWidth() * zoomLevel));
            const int colInArea = e.getPosition().x - waveformArea.getX();
            const int srcCol = currentZone.reverse ? (waveformArea.getWidth() - 1 - colInArea) : colInArea;
            viewOffset = juce::jmax (0, focusSample - (int) (srcCol * samplesPerPixel));
            waveformNeedsUpdate = true;
            syncScrollbar();
            repaint();
        }
        else
        {
            if (wheel.deltaY == 0.0f && wheel.deltaX == 0.0f) return;
            const double samplesPerPixel = juce::jmax (1.0,
                (double) sampleBuffer.getNumSamples()
                / juce::jmax (1.0, waveformArea.getWidth() * zoomLevel));
            const int delta = (int) (samplesPerPixel * (wheel.deltaY != 0.0f
                ? -wheel.deltaY * 80.0f : -wheel.deltaX * 80.0f));
            viewOffset = juce::jlimit (0, juce::jmax (0,
                sampleBuffer.getNumSamples() - (int) (waveformArea.getWidth() * samplesPerPixel)),
                viewOffset + delta);
            waveformNeedsUpdate = true;
            syncScrollbar();
            repaint();
        }
    }

    int SampleWaveformViewer::snapToZeroCrossing (int sampleIndex, int searchRadius) const
    {
        const int total = sampleBuffer.getNumSamples();
        if (total < 2) return sampleIndex;
        const int target = juce::jlimit (0, total - 1, sampleIndex);
        const int lo = juce::jmax (1, target - searchRadius);
        const int hi = juce::jmin (total - 1, target + searchRadius);

        int best = target;
        int bestDist = std::numeric_limits<int>::max();
        const int numCh = sampleBuffer.getNumChannels();
        for (int i = lo; i <= hi; ++i)
        {
            float prev = 0.0f, curr = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                prev += sampleBuffer.getReadPointer (ch)[i - 1];
                curr += sampleBuffer.getReadPointer (ch)[i];
            }
            // Sign change between consecutive samples → zero crossing.
            if ((prev <= 0.0f && curr > 0.0f) || (prev >= 0.0f && curr < 0.0f))
            {
                const int d = std::abs (i - target);
                if (d < bestDist) { bestDist = d; best = i; }
            }
        }
        return best;
    }

    int SampleWaveformViewer::snapToBeatGrid (int sampleIndex) const
    {
        const int total = sampleBuffer.getNumSamples();
        if (total <= 0)
            return sampleIndex;

        const double samplesPerBeat = sampleRate * 60.0 / juce::jmax (20.0, beatSnapBpm);
        const double stepSamples = samplesPerBeat / (double) juce::jmax (1, beatSnapDivisionsPerBeat);
        if (stepSamples < 1.0)
            return juce::jlimit (0, total, sampleIndex);

        return juce::jlimit (0, total,
                             juce::roundToInt (std::round ((double) sampleIndex / stepSamples) * stepSamples));
    }

    void SampleWaveformViewer::syncScrollbar()
    {
        if (sampleBuffer.getNumSamples() <= 0 || waveformArea.getWidth() <= 0)
        {
            suppressScrollbarCallback = true;
            horizontalScrollbar.setRangeLimits (0.0, 1.0, juce::dontSendNotification);
            horizontalScrollbar.setCurrentRange (0.0, 1.0, juce::dontSendNotification);
            suppressScrollbarCallback = false;
            return;
        }
        const double samplesPerPixel = juce::jmax (1.0,
            (double) sampleBuffer.getNumSamples()
            / juce::jmax (1.0, waveformArea.getWidth() * zoomLevel));
        const double visible = juce::jmin ((double) sampleBuffer.getNumSamples(),
                                            waveformArea.getWidth() * samplesPerPixel);
        suppressScrollbarCallback = true;
        horizontalScrollbar.setRangeLimits (0.0, (double) sampleBuffer.getNumSamples(),
                                             juce::dontSendNotification);
        horizontalScrollbar.setCurrentRange ((double) viewOffset, visible,
                                              juce::dontSendNotification);
        suppressScrollbarCallback = false;
    }

    void SampleWaveformViewer::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
    {
        if (suppressScrollbarCallback) return;
        viewOffset = juce::jmax (0, (int) newRangeStart);
        waveformNeedsUpdate = true;
        repaint();
    }

} // namespace patchcraft
