#include "KeyzonesComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        class KeyzoneTextCell final : public juce::TextEditor
        {
        public:
            std::function<void()> commit;

            void focusLost (FocusChangeType cause) override
            {
                juce::TextEditor::focusLost (cause);
                if (commit)
                    commit();
            }

            bool keyPressed (const juce::KeyPress& key) override
            {
                if (key == juce::KeyPress::returnKey)
                {
                    if (commit)
                        commit();
                    return true;
                }

                return juce::TextEditor::keyPressed (key);
            }
        };

        class KeyzoneToggleCell final : public juce::ToggleButton
        {
        public:
            std::function<void (bool)> commit;
        };

        bool parseBool (juce::String text)
        {
            text = text.trim().toLowerCase();
            return text == "1" || text == "true" || text == "yes" || text == "on";
        }
    }

    KeyzonesComponent::KeyzonesComponent (StudioMainComponent& o) : owner (o)
    {
        addAndMakeVisible (table);
        table.setColour (juce::ListBox::backgroundColourId, PatchCraftLookAndFeel::panelAlt());
        auto& header = table.getHeader();
        header.addColumn ("Sample",    1, 200);
        header.addColumn ("Root",      2, 58);
        header.addColumn ("Low Key",   3, 64);
        header.addColumn ("High Key",  4, 64);
        header.addColumn ("Low Vel",   5, 64);
        header.addColumn ("High Vel",  6, 70);
        header.addColumn ("Gain dB",   7, 68);
        header.addColumn ("Pan",       8, 58);
        header.addColumn ("Loop",      9, 58);
        header.addColumn ("Loop In",  10, 74);
        header.addColumn ("Loop Out", 11, 74);
        header.addColumn ("Start",    12, 74);
        header.addColumn ("End",      13, 74);
        header.addColumn ("RR Group", 14, 74);
        header.addColumn ("RR Index", 15, 72);
        header.addColumn ("Pitch",    16, 64);
        header.addColumn ("Reverse",  17, 68);
        header.addColumn ("Group",    18, 100);
        table.setHeaderHeight (24);
    }

    void KeyzonesComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void KeyzonesComponent::resized()
    {
        table.setBounds (getLocalBounds().reduced (4));
    }

    void KeyzonesComponent::refresh()
    {
        table.updateContent();
        table.repaint();
        repaint();
    }

    int KeyzonesComponent::getNumRows()
    {
        return (int) owner.getProject().getSampleMap().getZones().size();
    }

    void KeyzonesComponent::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool sel)
    {
        if (sel)
            g.fillAll (PatchCraftLookAndFeel::accent().withAlpha (0.18f));
        else if (row % 2)
            g.fillAll (juce::Colour (0xff141519));
    }

    void KeyzonesComponent::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (row < 0 || row >= (int) zones.size()) return;
        const auto& z = zones[(size_t) row];

        g.setColour (PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.0f));
        const auto s = textForCell (z, col);
        g.drawText (s, 6, 0, w - 12, h, juce::Justification::centredLeft);
    }

    juce::Component* KeyzonesComponent::refreshComponentForCell (int row, int col, bool,
                                                                 juce::Component* existing)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (row < 0 || row >= (int) zones.size() || ! isEditableColumn (col))
        {
            delete existing;
            return nullptr;
        }

        const auto& zone = zones[(size_t) row];
        const bool isToggleColumn = col == 9 || col == 17;

        if (isToggleColumn)
        {
            if (dynamic_cast<KeyzoneToggleCell*> (existing) == nullptr)
            {
                delete existing;
                existing = new KeyzoneToggleCell();
            }

            auto* toggle = static_cast<KeyzoneToggleCell*> (existing);
            toggle->setButtonText ({});
            toggle->setToggleState (col == 9 ? zone.loopEnabled : zone.reverse, juce::dontSendNotification);
            toggle->commit = [safeThis = juce::Component::SafePointer<KeyzonesComponent> (this), row, col] (bool state)
            {
                if (safeThis != nullptr)
                    safeThis->commitCellEdit (row, col, state ? "1" : "0");
            };
            toggle->onClick = [toggle]
            {
                if (toggle->commit)
                    toggle->commit (toggle->getToggleState());
            };
            return toggle;
        }

        if (dynamic_cast<KeyzoneTextCell*> (existing) == nullptr)
        {
            delete existing;
            existing = new KeyzoneTextCell();
        }

        auto* editor = static_cast<KeyzoneTextCell*> (existing);
        editor->setText (textForCell (zone, col), juce::dontSendNotification);
        editor->setSelectAllWhenFocused (true);
        editor->setJustification (juce::Justification::centredLeft);
        editor->setFont (juce::Font (12.0f));
        editor->setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::panelAlt().withAlpha (0.35f));
        editor->setColour (juce::TextEditor::focusedOutlineColourId, PatchCraftLookAndFeel::accent());
        editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor->setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
        editor->commit = [safeThis = juce::Component::SafePointer<KeyzonesComponent> (this), row, col, editor]
        {
            if (safeThis != nullptr)
                safeThis->commitCellEdit (row, col, editor->getText());
        };
        return editor;
    }

    juce::String KeyzonesComponent::textForCell (const SampleZoneDef& z, int col) const
    {
        switch (col)
        {
            case 1:  return juce::File (z.samplePath).getFileName();
            case 2:  return juce::String (z.rootNote);
            case 3:  return juce::String (z.lowNote);
            case 4:  return juce::String (z.highNote);
            case 5:  return juce::String (z.lowVelocity);
            case 6:  return juce::String (z.highVelocity);
            case 7:  return juce::String (z.gainDb, 1);
            case 8:  return juce::String (z.pan, 2);
            case 9:  return z.loopEnabled ? "ON" : "OFF";
            case 10: return juce::String (z.loopStart);
            case 11: return juce::String (z.loopEnd);
            case 12: return juce::String (z.sampleStart);
            case 13: return juce::String (z.sampleEnd);
            case 14: return juce::String (z.roundRobinGroup);
            case 15: return juce::String (z.roundRobinIndex);
            case 16: return juce::String (z.pitchOffset, 2);
            case 17: return z.reverse ? "ON" : "OFF";
            case 18: return z.group;
            default: return {};
        }
    }

    bool KeyzonesComponent::isEditableColumn (int col) const
    {
        return col >= 2 && col <= 18;
    }

    void KeyzonesComponent::commitCellEdit (int row, int col, const juce::String& text)
    {
        auto& zones = owner.getProject().getSampleMap().getZones();
        if (row < 0 || row >= (int) zones.size())
            return;

        auto& z = zones[(size_t) row];
        const auto trimmed = text.trim();
        switch (col)
        {
            case 2:
                z.rootNote = juce::jlimit (0, 127, trimmed.getIntValue());
                break;
            case 3:
                z.lowNote = juce::jlimit (0, z.highNote, trimmed.getIntValue());
                break;
            case 4:
                z.highNote = juce::jlimit (z.lowNote, 127, trimmed.getIntValue());
                break;
            case 5:
                z.lowVelocity = juce::jlimit (1, z.highVelocity, trimmed.getIntValue());
                break;
            case 6:
                z.highVelocity = juce::jlimit (z.lowVelocity, 127, trimmed.getIntValue());
                break;
            case 7:
                z.gainDb = juce::jlimit (-96.0f, 24.0f, (float) trimmed.getDoubleValue());
                break;
            case 8:
                z.pan = juce::jlimit (-1.0f, 1.0f, (float) trimmed.getDoubleValue());
                break;
            case 9:
                z.loopEnabled = parseBool (trimmed);
                break;
            case 10:
                z.loopStart = juce::jmax (0, trimmed.getIntValue());
                if (z.loopEnd > 0 && z.loopEnd < z.loopStart)
                    z.loopEnd = z.loopStart;
                break;
            case 11:
                z.loopEnd = juce::jmax (z.loopStart, trimmed.getIntValue());
                break;
            case 12:
                z.sampleStart = juce::jmax (0, trimmed.getIntValue());
                if (z.sampleEnd > 0 && z.sampleEnd < z.sampleStart)
                    z.sampleEnd = z.sampleStart + 1;
                break;
            case 13:
                z.sampleEnd = juce::jmax (0, trimmed.getIntValue());
                if (z.sampleEnd > 0 && z.sampleEnd <= z.sampleStart)
                    z.sampleEnd = z.sampleStart + 1;
                break;
            case 14:
                z.roundRobinGroup = juce::jlimit (0, 127, trimmed.getIntValue());
                break;
            case 15:
                z.roundRobinIndex = juce::jlimit (0, 127, trimmed.getIntValue());
                break;
            case 16:
                z.pitchOffset = juce::jlimit (-48.0f, 48.0f, (float) trimmed.getDoubleValue());
                break;
            case 17:
                z.reverse = parseBool (trimmed);
                break;
            case 18:
                z.group = trimmed;
                break;
            default:
                return;
        }

        owner.getProject().notifyChanged();
        table.repaint();
    }

} // namespace patchcraft
