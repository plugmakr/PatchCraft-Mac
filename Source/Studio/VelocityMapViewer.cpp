#include "VelocityMapViewer.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    VelocityMapViewer::VelocityMapViewer()
    {
        setOpaque (true);
    }

    VelocityMapViewer::~VelocityMapViewer() = default;

    void VelocityMapViewer::setZones (const std::vector<SampleZoneDef>& zones)
    {
        currentZones = zones;
        repaint();
    }

    void VelocityMapViewer::setSelectedZone (int index)
    {
        selectedZoneIndex = index;
        repaint();
    }

    juce::Colour VelocityMapViewer::zoneColour (int idx)
    {
        const juce::uint32 cols[] = {
            (juce::uint32) PatchCraftLookAndFeel::kZoneA,
            (juce::uint32) PatchCraftLookAndFeel::kZoneB,
            (juce::uint32) PatchCraftLookAndFeel::kZoneC,
            (juce::uint32) PatchCraftLookAndFeel::kZoneD
        };
        return juce::Colour (cols[idx % 4]);
    }

    juce::String VelocityMapViewer::noteName (int midi) const
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int oct = midi / 12 - 1;
        return juce::String (names[midi % 12]) + juce::String (oct);
    }

    juce::Rectangle<int> VelocityMapViewer::plotBounds() const
    {
        return getLocalBounds().reduced (36, 18).withTrimmedBottom (12);
    }

    int VelocityMapViewer::noteAtX (int x) const
    {
        auto r = plotBounds();
        if (r.getWidth() <= 0)
            return 0;
        const float normalised = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / (float) r.getWidth());
        return juce::jlimit (0, 127, juce::roundToInt (normalised * 127.0f));
    }

    int VelocityMapViewer::velocityAtY (int y) const
    {
        auto r = plotBounds();
        if (r.getHeight() <= 0)
            return 1;
        const float normalised = juce::jlimit (0.0f, 1.0f, (y - r.getY()) / (float) r.getHeight());
        return juce::jlimit (1, 127, juce::roundToInt (127.0f - normalised * 126.0f));
    }

    juce::Rectangle<float> VelocityMapViewer::zoneRectFor (const SampleZoneDef& z) const
    {
        auto r = plotBounds().toFloat();
        const float x1 = r.getX() + (z.lowNote / 128.0f) * r.getWidth();
        const float x2 = r.getX() + ((z.highNote + 1) / 128.0f) * r.getWidth();
        const float y1 = r.getY() + ((127.0f - (float) z.highVelocity) / 126.0f) * r.getHeight();
        const float y2 = r.getY() + ((128.0f - (float) z.lowVelocity) / 127.0f) * r.getHeight();
        return { x1, y1, juce::jmax (4.0f, x2 - x1), juce::jmax (6.0f, y2 - y1) };
    }

    void VelocityMapViewer::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());

        auto bounds = plotBounds();
        if (bounds.isEmpty()) return;

        const int numNotes = 128;
        const int numVelocities = 128;

        // Draw grid
        g.setColour (PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (bounds.toFloat(), 5.0f);
        const float noteWidth = bounds.getWidth() / (float) numNotes;
        const float velHeight = bounds.getHeight() / (float) numVelocities;

        // Draw note grid lines (every octave)
        g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.3f));
        for (int i = 0; i <= numNotes; i += 12)
        {
            int x = bounds.getX() + juce::roundToInt (i * noteWidth);
            g.drawVerticalLine (x, (float) bounds.getY(), (float) bounds.getBottom());
        }

        // Draw velocity grid lines
        for (int velocity : { 127, 112, 96, 80, 64, 48, 32, 16, 1 })
        {
            int y = bounds.getY() + juce::roundToInt (((127.0f - velocity) / 126.0f) * bounds.getHeight());
            g.drawHorizontalLine (y, (float) bounds.getX(), (float) bounds.getRight());
        }

        // Draw zones
        for (size_t i = 0; i < currentZones.size(); ++i)
        {
            const auto& z = currentZones[i];
            auto zoneRect = zoneRectFor (z);

            // Fill zone
            g.setColour (zoneColour ((int) i).withAlpha (0.4f));
            g.fillRect (zoneRect);

            // Highlight selected zone
            if ((int) i == selectedZoneIndex)
            {
                g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.6f));
                g.drawRoundedRectangle (zoneRect, 3.0f, 2.0f);
            }
            else
            {
                g.setColour (zoneColour ((int) i));
                g.drawRoundedRectangle (zoneRect, 3.0f, 1.0f);
            }

            // Draw root note marker
            int rootX = bounds.getX() + juce::roundToInt (z.rootNote * noteWidth);
            g.setColour (juce::Colours::white);
            g.drawVerticalLine (rootX, zoneRect.getY(), zoneRect.getBottom());

            if (zoneRect.getWidth() > 44.0f)
            {
                g.setColour (PatchCraftLookAndFeel::textBright());
                g.setFont (juce::FontOptions (10.0f));
                g.drawText (juce::File (z.samplePath).getFileNameWithoutExtension(),
                            zoneRect.toNearestInt().reduced (4), juce::Justification::topLeft, true);
            }
        }

        // Draw axis labels
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (juce::Font (9.0f));

        // Note labels (every octave)
        for (int i = 0; i < numNotes; i += 12)
        {
            int x = bounds.getX() + juce::roundToInt (i * noteWidth);
            g.drawText (noteName (i), x + 2, bounds.getBottom() + 2, 40, 10,
                      juce::Justification::centredLeft);
        }

        // Velocity labels
        g.drawText ("Velocity", 8, bounds.getY(), 58, 16, juce::Justification::centredLeft);
        for (int velocity : { 127, 96, 64, 32, 1 })
        {
            int y = bounds.getY() + juce::roundToInt (((127.0f - velocity) / 126.0f) * bounds.getHeight());
            g.drawText (juce::String (velocity), 8, y - 7, 24, 14, juce::Justification::centredRight);
        }

        g.setColour (PatchCraftLookAndFeel::textDim());
        g.drawText ("Drag zones to move note/velocity ranges. Drag edges to resize.",
                    bounds.withY (getHeight() - 16).withHeight (14),
                    juce::Justification::centredRight);
    }

    void VelocityMapViewer::resized()
    {
        repaint();
    }

    void VelocityMapViewer::mouseDown (const juce::MouseEvent& e)
    {
        auto bounds = plotBounds();
        if (! bounds.contains (e.getPosition()))
            return;

        int clickedNote = noteAtX (e.x);
        int clickedVel = velocityAtY (e.y);
        const float handle = 8.0f;
        dragMode = DragMode::none;
        dragZoneIndex = -1;

        // Find which zone was clicked
        for (int i = (int) currentZones.size() - 1; i >= 0; --i)
        {
            const auto& z = currentZones[(size_t) i];
            auto rect = zoneRectFor (z);
            if (rect.expanded (4.0f).contains ((float) e.x, (float) e.y))
            {
                selectedZoneIndex = i;
                dragZoneIndex = i;
                dragStartX = e.x;
                dragStartY = e.y;
                dragStartNote = clickedNote;
                dragStartVelocity = clickedVel;
                dragStartZone = z;

                if (std::abs ((float) e.x - rect.getX()) <= handle)
                    dragMode = DragMode::lowNote;
                else if (std::abs ((float) e.x - rect.getRight()) <= handle)
                    dragMode = DragMode::highNote;
                else if (std::abs ((float) e.y - rect.getY()) <= handle)
                    dragMode = DragMode::highVelocity;
                else if (std::abs ((float) e.y - rect.getBottom()) <= handle)
                    dragMode = DragMode::lowVelocity;
                else
                    dragMode = DragMode::move;

                if (onZoneClicked)
                    onZoneClicked (i);
                repaint();
                break;
            }
        }
    }

    void VelocityMapViewer::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragMode == DragMode::none || dragZoneIndex < 0 || dragZoneIndex >= (int) currentZones.size())
            return;

        auto& z = currentZones[(size_t) dragZoneIndex];
        const int currentNote = noteAtX (e.x);
        const int currentVelocity = velocityAtY (e.y);
        const int noteDelta = currentNote - dragStartNote;
        const int velocityDelta = currentVelocity - dragStartVelocity;

        if (dragMode == DragMode::lowNote)
            z.lowNote = juce::jlimit (0, z.highNote, currentNote);
        else if (dragMode == DragMode::highNote)
            z.highNote = juce::jlimit (z.lowNote, 127, currentNote);
        else if (dragMode == DragMode::lowVelocity)
            z.lowVelocity = juce::jlimit (1, z.highVelocity, currentVelocity);
        else if (dragMode == DragMode::highVelocity)
            z.highVelocity = juce::jlimit (z.lowVelocity, 127, currentVelocity);
        else if (dragMode == DragMode::move)
        {
            const int noteSpan = dragStartZone.highNote - dragStartZone.lowNote;
            const int velSpan = dragStartZone.highVelocity - dragStartZone.lowVelocity;
            z.lowNote = juce::jlimit (0, 127 - noteSpan, dragStartZone.lowNote + noteDelta);
            z.highNote = juce::jlimit (z.lowNote, 127, z.lowNote + noteSpan);
            z.rootNote = juce::jlimit (z.lowNote, z.highNote, dragStartZone.rootNote + noteDelta);
            z.lowVelocity = juce::jlimit (1, 127 - velSpan, dragStartZone.lowVelocity + velocityDelta);
            z.highVelocity = juce::jlimit (z.lowVelocity, 127, z.lowVelocity + velSpan);
        }

        if (onZoneEdited)
            onZoneEdited (dragZoneIndex, z);
        repaint();
    }

    void VelocityMapViewer::mouseUp (const juce::MouseEvent&)
    {
        dragMode = DragMode::none;
        dragZoneIndex = -1;
    }

} // namespace patchcraft
