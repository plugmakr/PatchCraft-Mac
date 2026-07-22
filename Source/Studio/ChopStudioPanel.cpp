#include "ChopStudioPanel.h"

#include "SampleMap.h"
#include "SampleSynthEngine.h"
#include "PatchCraftLookAndFeel.h"
#include "RenderContext.h"

#include <algorithm>

namespace patchcraft
{
    class ChopStudioPanel::ChopWaveformView : public juce::Component
        {
        public:
            void setSource (const juce::AudioBuffer<float>* b, double rate, int start, int end)
            {
                buffer = b;
                sampleRate = rate;
                regionStart = start;
                regionEnd = juce::jmax (start + 1, end);
                rebuildPeaks();
                repaint();
            }

            std::vector<int>* slices = nullptr;
            std::function<void()> onSlicesChanged;
            std::function<void (int sliceIndex)> onAuditionSlice;
            int playingSlice = -1;
            double playheadSample = -1.0;

            std::vector<int> fullBoundaries() const
            {
                std::vector<int> b;
                b.push_back (regionStart);
                if (slices != nullptr)
                    for (int s : *slices)
                        if (s > regionStart && s < regionEnd)
                            b.push_back (s);
                b.push_back (regionEnd);
                std::sort (b.begin(), b.end());
                b.erase (std::unique (b.begin(), b.end()), b.end());
                return b;
            }

            void paint (juce::Graphics& g) override
            {
                auto area = getLocalBounds();
                g.setColour (PatchCraftLookAndFeel::panel());
                g.fillRoundedRectangle (area.toFloat(), 6.0f);

                if (buffer == nullptr || peaks.empty())
                {
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.drawText ("Drop or import a sample in Sound, then open Chop.", area, juce::Justification::centred);
                    return;
                }

                const auto bounds = area.reduced (2);
                const float midY = (float) bounds.getCentreY();
                const float halfH = (float) bounds.getHeight() * 0.46f;

                if (playingSlice >= 0)
                {
                    const auto b = fullBoundaries();
                    if (playingSlice + 1 < (int) b.size())
                    {
                        const int x0 = sampleToX (b[(size_t) playingSlice]);
                        const int x1 = sampleToX (b[(size_t) playingSlice + 1]);
                        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.18f));
                        g.fillRect (juce::Rectangle<int> (x0, bounds.getY(), juce::jmax (1, x1 - x0), bounds.getHeight()));
                    }
                }

                if (playheadSample >= (double) regionStart && playheadSample <= (double) regionEnd)
                {
                    const int px = sampleToX ((int) std::round (playheadSample));
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.fillRect (px - 1, bounds.getY(), 2, bounds.getHeight());
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.35f));
                    g.drawVerticalLine (px, (float) bounds.getY(), (float) bounds.getBottom());
                }

                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
                for (int x = 0; x < (int) peaks.size(); ++x)
                {
                    const float y0 = midY - peaks[(size_t) x].second * halfH;
                    const float y1 = midY - peaks[(size_t) x].first * halfH;
                    g.drawVerticalLine (bounds.getX() + x, y0, y1);
                }

                const auto b = fullBoundaries();
                for (int i = 0; i < (int) b.size(); ++i)
                {
                    const int x = sampleToX (b[(size_t) i]);
                    const bool edge = (i == 0 || i == (int) b.size() - 1);
                    g.setColour (edge ? PatchCraftLookAndFeel::textDim() : PatchCraftLookAndFeel::textBright());
                    g.fillRect (juce::Rectangle<int> (x - (edge ? 0 : 1), bounds.getY(), edge ? 1 : 2, bounds.getHeight()));
                    if (i < (int) b.size() - 1 && i < kMaxChopPads)
                    {
                        const int x2 = sampleToX (b[(size_t) i + 1]);
                        g.setColour (PatchCraftLookAndFeel::textDim());
                        g.setFont (juce::FontOptions (11.0f));
                        g.drawText (juce::String (i + 1),
                                    juce::Rectangle<int> (x + 3, bounds.getY() + 2, juce::jmax (10, x2 - x - 6), 14),
                                    juce::Justification::topLeft);
                    }
                }
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (buffer == nullptr || slices == nullptr)
                    return;

                draggingMarker = markerIndexNear (e.x);
                if (draggingMarker < 0)
                {
                    playheadSample = (double) xToSample (e.x);
                    const int sample = (int) playheadSample;
                    const auto b = fullBoundaries();
                    for (int i = 0; i + 1 < (int) b.size(); ++i)
                        if (sample >= b[(size_t) i] && sample < b[(size_t) i + 1])
                        {
                            if (onAuditionSlice) onAuditionSlice (i);
                            break;
                        }
                    repaint();
                }
            }

            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (draggingMarker < 0 || slices == nullptr)
                    return;

                const int idx = draggingMarker;
                if (idx >= (int) slices->size())
                    return;

                const int lower = idx > 0 ? (*slices)[(size_t) idx - 1] : regionStart;
                const int upper = idx + 1 < (int) slices->size() ? (*slices)[(size_t) idx + 1] : regionEnd;
                const int minGap = juce::jmax (16, (int) (sampleRate * 0.001));
                (*slices)[(size_t) idx] = juce::jlimit (lower + minGap, upper - minGap, xToSample (e.x));
                if (onSlicesChanged) onSlicesChanged();
                repaint();
            }

            void mouseUp (const juce::MouseEvent&) override { draggingMarker = -1; }

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                if (buffer == nullptr || slices == nullptr)
                    return;

                const int near = markerIndexNear (e.x);
                if (near >= 0 && near < (int) slices->size())
                {
                    slices->erase (slices->begin() + near);
                }
                else
                {
                    const int sample = xToSample (e.x);
                    const int minGap = juce::jmax (16, (int) (sampleRate * 0.001));
                    if (sample > regionStart + minGap && sample < regionEnd - minGap)
                    {
                        slices->push_back (sample);
                        std::sort (slices->begin(), slices->end());
                    }
                }
                if (onSlicesChanged) onSlicesChanged();
                repaint();
            }

            void resized() override { rebuildPeaks(); }

        private:
            int sampleToX (int sample) const
            {
                const auto w = juce::jmax (1, getWidth() - 4);
                const double t = (double) (sample - regionStart) / (double) juce::jmax (1, regionEnd - regionStart);
                return 2 + (int) std::round (t * w);
            }

            int xToSample (int x) const
            {
                const auto w = juce::jmax (1, getWidth() - 4);
                const double t = juce::jlimit (0.0, 1.0, (double) (x - 2) / (double) w);
                return regionStart + (int) std::round (t * (regionEnd - regionStart));
            }

            int markerIndexNear (int x) const
            {
                if (slices == nullptr)
                    return -1;
                for (int i = 0; i < (int) slices->size(); ++i)
                    if (std::abs (sampleToX ((*slices)[(size_t) i]) - x) <= 6)
                        return i;
                return -1;
            }

            void rebuildPeaks()
            {
                peaks.clear();
                if (buffer == nullptr || buffer->getNumSamples() <= 0)
                    return;

                const int w = juce::jmax (1, getWidth() - 4);
                peaks.resize ((size_t) w, { 0.0f, 0.0f });
                const int ch = juce::jmax (1, buffer->getNumChannels());
                const int regionLen = juce::jmax (1, regionEnd - regionStart);

                for (int x = 0; x < w; ++x)
                {
                    const int s0 = regionStart + (int) ((juce::int64) x * regionLen / w);
                    const int s1 = regionStart + (int) ((juce::int64) (x + 1) * regionLen / w);
                    float mn = 0.0f, mx = 0.0f;
                    for (int s = s0; s < s1 && s < buffer->getNumSamples(); ++s)
                    {
                        float v = 0.0f;
                        for (int c = 0; c < ch; ++c)
                            v += buffer->getSample (c, s);
                        v /= (float) ch;
                        mn = juce::jmin (mn, v);
                        mx = juce::jmax (mx, v);
                    }
                    peaks[(size_t) x] = { mn, mx };
                }
            }

            const juce::AudioBuffer<float>* buffer = nullptr;
            double sampleRate = 44100.0;
            int regionStart = 0;
            int regionEnd = 1;
            int draggingMarker = -1;
            std::vector<std::pair<float, float>> peaks;
    };

    class ChopStudioPanel::ChopPadGrid : public juce::Component
        {
        public:
            int padCount = 0;
            int playingPad = -1;
            std::function<void (int)> onPad;

            void paint (juce::Graphics& g) override
            {
                auto area = getLocalBounds();
                const int cols = 8, rows = 4;
                const int gap = 5;
                const int cw = (area.getWidth() - gap * (cols - 1)) / cols;
                const int ch = (area.getHeight() - gap * (rows - 1)) / rows;

                for (int i = 0; i < kMaxChopPads; ++i)
                {
                    const int r = i / cols, c = i % cols;
                    juce::Rectangle<int> pad (area.getX() + c * (cw + gap),
                                              area.getY() + r * (ch + gap), cw, ch);
                    const bool active = i < padCount;
                    juce::Colour fill = active ? PatchCraftLookAndFeel::accent().withAlpha (0.30f)
                                               : PatchCraftLookAndFeel::panel();
                    if (i == playingPad)
                        fill = PatchCraftLookAndFeel::accent();
                    g.setColour (fill);
                    g.fillRoundedRectangle (pad.toFloat(), 4.0f);
                    g.setColour (active ? PatchCraftLookAndFeel::textBright()
                                        : PatchCraftLookAndFeel::textDim().withAlpha (0.4f));
                    g.drawRoundedRectangle (pad.toFloat(), 4.0f, 1.0f);
                    g.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
                    g.drawText (juce::String (i + 1), pad, juce::Justification::centred);
                }
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                const int cols = 8, rows = 4, gap = 5;
                auto area = getLocalBounds();
                const int cw = (area.getWidth() - gap * (cols - 1)) / cols;
                const int ch = (area.getHeight() - gap * (rows - 1)) / rows;
                for (int i = 0; i < kMaxChopPads; ++i)
                {
                    const int r = i / cols, c = i % cols;
                    juce::Rectangle<int> pad (area.getX() + c * (cw + gap),
                                              area.getY() + r * (ch + gap), cw, ch);
                    if (pad.contains (e.getPosition()) && i < padCount && onPad)
                    {
                        onPad (i);
                        break;
                    }
                }
            }
    };

    ChopStudioPanel::ChopStudioPanel (juce::AudioDeviceManager& dm,
                                      std::function<bool (juce::String&)> ensureAudioFn)
        : deviceManager (dm), ensureAudio (std::move (ensureAudioFn))
    {
        waveform = std::make_unique<ChopWaveformView>();
        pads = std::make_unique<ChopPadGrid>();

        title.setText ("Sample Chopper", juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (title);

        info.setFont (juce::FontOptions (12.0f));
        info.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        info.setText ("Double-click to add slices. Drag markers. Click pads to audition.",
                      juce::dontSendNotification);
        addAndMakeVisible (info);

        gridBox.addItem ("1/4", 1);
        gridBox.addItem ("1/8", 2);
        gridBox.addItem ("1/16", 3);
        gridBox.addItem ("1/32", 4);
        gridBox.setSelectedId (3, juce::dontSendNotification);
        addAndMakeVisible (gridBox);

        bpmField.setInputRestrictions (6, "0123456789.");
        bpmField.setText ("120", juce::dontSendNotification);
        bpmField.setTooltip ("Detected or manual BPM used for beat slicing.");
        addAndMakeVisible (bpmLabel);
        addAndMakeVisible (bpmField);

        for (auto* b : { &detectBtn, &sliceBeatBtn, &sliceBtn, &transientBtn, &playBtn, &stopBtn,
                         &clearBtn, &unloadBtn, &applyBtn, &closeBtn })
        {
            b->getProperties().set ("smallButton", true);
            addAndMakeVisible (*b);
        }
        applyBtn.getProperties().set ("primaryAction", true);
        closeBtn.setVisible (false);

        addAndMakeVisible (*waveform);
        addAndMakeVisible (*pads);

        waveform->slices = &slices;
        waveform->onSlicesChanged = [this] { refreshFromSlices(); };
        waveform->onAuditionSlice = [this] (int s) { auditionSlice (s); };
        pads->onPad = [this] (int p) { auditionSlice (p); };

        detectBtn.onClick = [this] { doDetect(); };
        sliceBeatBtn.onClick = [this] { doSliceBeat(); };
        sliceBtn.onClick = [this] { doSliceGrid(); };
        transientBtn.onClick = [this] { doTransient(); };
        playBtn.onClick = [this] { playFromPlayhead(); };
        stopBtn.onClick = [this] { stopAudition(); };
        clearBtn.onClick = [this] { slices.clear(); refreshFromSlices(); waveform->repaint(); };
        unloadBtn.onClick = [this] { clearSource(); };
        applyBtn.onClick = [this]
        {
            if (onApply)
                onApply (waveform->fullBoundaries(), currentBpm());
        };
    }

    ChopStudioPanel::~ChopStudioPanel()
    {
        stopTimer();
        stopAudition();
    }

    void ChopStudioPanel::clearSource()
    {
        stopAudition();
        hasSource = false;
        sourceBuffer.setSize (0, 0);
        sourceRate = 44100.0;
        projectFolder = {};
        baseZone = {};
        regionStart = 0;
        regionEnd = 1;
        slices.clear();
        detectedBpm = 0.0;
        detectedKey.clear();
        bpmField.setText ("120", juce::dontSendNotification);
        manualPlayhead = -1.0;
        waveform->setSource (nullptr, 44100.0, 0, 1);
        waveform->playheadSample = -1.0;
        waveform->playingSlice = -1;
        pads->padCount = 0;
        pads->playingPad = -1;
        pads->repaint();
        waveform->repaint();
        updateInfo();
        info.setText ("Sample unloaded. Import a WAV or open Sound Mapper.", juce::dontSendNotification);
    }

    void ChopStudioPanel::setSource (const juce::AudioBuffer<float>& buffer, double rate,
                                     const juce::File& folder, const SampleZoneDef& base)
    {
        hasSource = buffer.getNumSamples() > 0;
        sourceBuffer.makeCopyOf (buffer);
        sourceRate = rate > 0.0 ? rate : 44100.0;
        projectFolder = folder;
        baseZone = base;

        const int len = sourceBuffer.getNumSamples();
        regionStart = juce::jlimit (0, juce::jmax (0, len - 1), base.sampleStart);
        regionEnd = base.sampleEnd > regionStart ? juce::jmin (len, base.sampleEnd) : len;

        slices.clear();
        if (base.cuePoints.size() >= 2)
        {
            for (size_t i = 1; i + 1 < base.cuePoints.size(); ++i)
                slices.push_back (base.cuePoints[i]);
        }

        detectedBpm = base.bpm > 0.0f ? (double) base.bpm : 0.0;
        if (detectedBpm <= 0.0)
        {
            const auto analysis = SampleMap::analyseClipBuffer (sourceBuffer, sourceRate, regionStart, regionEnd);
            detectedBpm = analysis.bpm;
            detectedKey = analysis.keyName();
            if (detectedBpm > 0.0)
                baseZone.bpm = (float) detectedBpm;
        }

        if (detectedBpm > 0.0)
            bpmField.setText (juce::String (detectedBpm, 1), juce::dontSendNotification);

        waveform->setSource (&sourceBuffer, sourceRate, regionStart, regionEnd);
        waveform->playheadSample = (double) regionStart;
        refreshFromSlices();
        updateInfo();
    }

    void ChopStudioPanel::resized()
    {
        auto r = getLocalBounds().reduced (10);
        auto header = r.removeFromTop (26);
        title.setBounds (header.removeFromLeft (220));
        info.setBounds (header);
        r.removeFromTop (6);

        auto toolbar = r.removeFromTop (32);
        auto place = [&] (juce::Component& c, int w) { c.setBounds (toolbar.removeFromLeft (w).reduced (2, 2)); toolbar.removeFromLeft (4); };
        place (detectBtn, 130);
        place (bpmLabel, 34);
        place (bpmField, 52);
        place (sliceBeatBtn, 108);
        place (gridBox, 70);
        place (sliceBtn, 100);
        place (transientBtn, 118);
        place (playBtn, 64);
        place (stopBtn, 64);
        place (clearBtn, 96);
        place (unloadBtn, 108);
        applyBtn.setBounds (toolbar.removeFromRight (118).reduced (2, 2));

        r.removeFromTop (8);
        auto padArea = r.removeFromBottom (juce::jmin (200, r.getHeight() / 2));
        pads->setBounds (padArea.reduced (2));
        r.removeFromBottom (6);
        waveform->setBounds (r);
    }

    void ChopStudioPanel::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());
    }

    void ChopStudioPanel::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        auditionRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
        auditionBlock = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
        auditionChannels = device != nullptr
            ? juce::jmax (1, device->getActiveOutputChannels().countNumberOfSetBits()) : 2;
        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine != nullptr)
            engine->prepare (auditionRate, auditionBlock, auditionChannels);
    }

    void ChopStudioPanel::audioDeviceStopped()
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        if (engine != nullptr)
            engine->reset();
    }

    void ChopStudioPanel::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                            float* const* outputChannelData, int numOutputChannels,
                                                            int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        juce::AudioBuffer<float> output (outputChannelData, numOutputChannels, numSamples);
        output.clear();
        const juce::SpinLock::ScopedTryLockType lock (engineLock);
        if (lock.isLocked() && engine != nullptr)
        {
            engine->setRenderContext (RenderContext::forBlock (auditionRate, numSamples, auditionBlock,
                                                               0, numOutputChannels, 120.0));
            engine->process (output, 0, numSamples);
        }
    }

    void ChopStudioPanel::refreshFromSlices()
    {
        pads->padCount = juce::jmin (kMaxChopPads, (int) waveform->fullBoundaries().size() - 1);
        pads->repaint();
        rebuildAuditionEngine();
    }

    void ChopStudioPanel::updateInfo()
    {
        juce::String txt = baseZone.samplePath;
        if (detectedBpm > 0.0)
            txt += "   |   " + juce::String (detectedBpm, 1) + " BPM";
        if (detectedKey.isNotEmpty())
            txt += "   |   key " + detectedKey;
        info.setText (txt.isNotEmpty() ? txt
                      : juce::String ("Double-click waveform to add a slice. Drag to move, double-click marker to delete."),
                      juce::dontSendNotification);
    }

    void ChopStudioPanel::doDetect()
    {
        if (! hasSource)
            return;

        const auto analysis = SampleMap::analyseClipBuffer (sourceBuffer, sourceRate, regionStart, regionEnd);
        detectedBpm = analysis.bpm;
        detectedKey = analysis.keyName();
        if (detectedBpm > 0.0)
        {
            baseZone.bpm = (float) detectedBpm;
            bpmField.setText (juce::String (detectedBpm, 1), juce::dontSendNotification);
        }
        updateInfo();
    }

    double ChopStudioPanel::currentBpm() const
    {
        const double manual = bpmField.getText().getDoubleValue();
        if (manual > 20.0)
            return manual;
        if (detectedBpm > 0.0)
            return detectedBpm;
        if (baseZone.bpm > 0.0f)
            return (double) baseZone.bpm;
        return 120.0;
    }

    void ChopStudioPanel::doSliceBeat()
    {
        if (! hasSource)
            return;

        double bpm = currentBpm();
        if (detectedBpm <= 0.0 && baseZone.bpm <= 0.0f)
        {
            doDetect();
            bpm = currentBpm();
        }

        const auto b = SampleMap::sliceByBeatGrid (regionStart, regionEnd, sourceRate, bpm, 1);
        slices.clear();
        for (int s : b)
            if (s > regionStart && s < regionEnd)
                slices.push_back (s);
        refreshFromSlices();
    }

    void ChopStudioPanel::doSliceGrid()
    {
        if (! hasSource)
            return;

        double bpm = currentBpm();
        if (detectedBpm <= 0.0 && baseZone.bpm <= 0.0f)
        {
            doDetect();
            bpm = currentBpm();
        }

        const int per = juce::jlimit (1, 4, gridBox.getSelectedId());
        const int slicesPerBeat = per == 1 ? 1 : per == 2 ? 2 : per == 3 ? 4 : 8;
        const auto b = SampleMap::sliceByBeatGrid (regionStart, regionEnd, sourceRate, bpm, slicesPerBeat);

        slices.clear();
        for (int s : b)
            if (s > regionStart && s < regionEnd)
                slices.push_back (s);
        refreshFromSlices();
    }

    void ChopStudioPanel::doTransient()
    {
        if (! hasSource)
            return;

        const auto onsets = SampleMap::detectOnsets (sourceBuffer, sourceRate, regionStart, regionEnd, kMaxChopPads);
        slices.clear();
        for (int s : onsets)
            if (s > regionStart && s < regionEnd)
                slices.push_back (s);
        std::sort (slices.begin(), slices.end());
        refreshFromSlices();
    }

    void ChopStudioPanel::rebuildAuditionEngine()
    {
        if (! hasSource)
            return;

        const auto boundaries = waveform->fullBoundaries();
        const int count = juce::jmin (kMaxChopPads, (int) boundaries.size() - 1);
        if (count <= 0)
            return;

        auto newEngine = std::make_unique<SampleSynthEngine>();
        if (auditionRate > 0.0)
            newEngine->prepare (auditionRate, auditionBlock, auditionChannels);

        auto z = baseZone;
        z.cuePoints = boundaries;
        z.sampleStart = regionStart;
        z.sampleEnd = regionEnd;
        z.lowNote = 36;
        z.highNote = juce::jlimit (36, 127, 36 + count - 1);
        z.rootNote = 36;
        z.playMode = 1;
        z.oneShot = true;
        z.loopEnabled = false;

        newEngine->loadSingleZoneFromBuffer (z, sourceBuffer, sourceRate);
        newEngine->setParameter ("sampleSliceCount", (float) count);
        newEngine->setParameter ("retrigger", 1.0f);
        newEngine->setParameter ("volume", 1.0f);

        const juce::SpinLock::ScopedLockType lock (engineLock);
        engine = std::move (newEngine);
    }

    void ChopStudioPanel::ensureAuditionOpen()
    {
        if (callbackActive)
            return;

        juce::String error;
        if (! ensureAudio (error))
        {
            info.setText ("Audio unavailable: " + error, juce::dontSendNotification);
            return;
        }

        deviceManager.addAudioCallback (this);
        callbackActive = true;
        rebuildAuditionEngine();
    }

    void ChopStudioPanel::stopAudition()
    {
        stopTimer();
        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            if (engine != nullptr)
            {
                engine->allNotesOff();
                engine->reset();
            }
        }

        auditionNote = -1;
        auditionSliceIndex = -1;
        waveform->playingSlice = -1;
        pads->playingPad = -1;
        waveform->repaint();
        pads->repaint();

        if (callbackActive)
        {
            deviceManager.removeAudioCallback (this);
            callbackActive = false;
        }
    }

    void ChopStudioPanel::playFromPlayhead()
    {
        if (! hasSource)
            return;

        ensureAuditionOpen();
        if (! callbackActive)
            return;

        if (manualPlayhead < (double) regionStart || manualPlayhead >= (double) regionEnd)
            manualPlayhead = waveform->playheadSample >= (double) regionStart
                ? waveform->playheadSample : (double) regionStart;

        auto z = baseZone;
        z.cuePoints.clear();
        z.sampleStart = (int) std::round (manualPlayhead);
        z.sampleEnd = regionEnd;
        z.lowNote = z.highNote = z.rootNote = 36;
        z.playMode = 1;
        z.oneShot = true;

        auto newEngine = std::make_unique<SampleSynthEngine>();
        if (auditionRate > 0.0)
            newEngine->prepare (auditionRate, auditionBlock, auditionChannels);
        newEngine->loadSingleZoneFromBuffer (z, sourceBuffer, sourceRate);
        newEngine->setParameter ("sampleStart", 0.0f);
        newEngine->setParameter ("sampleLength", 1.0f);
        newEngine->setParameter ("retrigger", 1.0f);
        newEngine->setParameter ("volume", 1.0f);

        const int note = 36;
        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            engine = std::move (newEngine);
            engine->noteOn (note, 0.92f);
        }

        auditionNote = note;
        auditionSliceIndex = -1;
        startTimerHz (30);
    }

    void ChopStudioPanel::timerCallback()
    {
        if (auditionNote < 0)
        {
            stopTimer();
            return;
        }

        double playhead = manualPlayhead;
        {
            const juce::SpinLock::ScopedTryLockType lock (engineLock);
            if (lock.isLocked() && engine != nullptr)
            {
                const double voicePos = engine->getActivePlayheadSample (auditionNote);
                if (voicePos >= 0.0)
                    playhead = voicePos;
            }
        }

        if (playhead >= 0.0)
        {
            waveform->playheadSample = playhead;
            manualPlayhead = playhead;
            waveform->repaint();
        }

        bool stillActive = false;
        {
            const juce::SpinLock::ScopedTryLockType lock (engineLock);
            if (lock.isLocked() && engine != nullptr)
                stillActive = engine->getActivePlayheadSample (auditionNote) >= 0.0;
        }

        if (! stillActive)
        {
            waveform->playingSlice = -1;
            pads->playingPad = -1;
            pads->repaint();
            stopTimer();
        }
    }

    void ChopStudioPanel::auditionSlice (int sliceIndex)
    {
        if (! hasSource)
            return;

        ensureAuditionOpen();
        if (! callbackActive)
            return;

        rebuildAuditionEngine();

        const int note = juce::jlimit (0, 127, 36 + sliceIndex);
        {
            const juce::SpinLock::ScopedLockType lock (engineLock);
            if (engine != nullptr)
            {
                engine->setParameter ("sampleStart", 0.0f);
                engine->setParameter ("sampleLength", 1.0f);
                engine->allNotesOff();
                engine->noteOn (note, 0.92f);
            }
        }

        auditionNote = note;
        auditionSliceIndex = sliceIndex;
        waveform->playingSlice = sliceIndex;
        pads->playingPad = sliceIndex;
        waveform->repaint();
        pads->repaint();
        startTimerHz (30);
    }

} // namespace patchcraft
