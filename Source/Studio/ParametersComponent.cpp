#include "ParametersComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    ParametersComponent::ParametersComponent (StudioMainComponent& o) : owner (o)
    {
        addAndMakeVisible (list);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);

        addAndMakeVisible (addBtn);
        addAndMakeVisible (menuBtn);
        addBtn.onClick = [this]
        {
            ParameterDef p;
            p.id = "param" + juce::String ((int) owner.getProject().getParameters().getAll().size() + 1);
            p.name = p.id;
            owner.getProject().getParameters().add (p);
            owner.getProject().notifyChanged();
            refresh();
        };
    }

    juce::String ParametersComponent::formatValue (const ParameterDef& p)
    {
        if (p.unit == "Hz")
        {
            if (p.defaultValue >= 1000.0f)
                return juce::String (p.defaultValue / 1000.0f, 1) + " kHz";
            return juce::String (p.defaultValue, 0) + " Hz";
        }
        if (p.unit == "%")
            return juce::String (juce::roundToInt (p.defaultValue * 100.0f)) + " %";
        if (p.unit == "s")
            return juce::String (p.defaultValue, 2) + " s";
        if (p.unit.isNotEmpty())
            return juce::String (p.defaultValue, 2) + " " + p.unit;
        return juce::String (p.defaultValue, 2);
    }

    void ParametersComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
    }

    void ParametersComponent::resized()
    {
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom (28).reduced (4, 2);
        addBtn.setBounds (bottom.removeFromLeft (28));
        bottom.removeFromLeft (4);
        menuBtn.setBounds (bottom.removeFromLeft (28));

        list.setBounds (r.reduced (4, 2));
    }

    void ParametersComponent::refresh()
    {
        list.updateContent();
        repaint();
    }

    int ParametersComponent::getNumRows()
    {
        return (int) owner.getProject().getParameters().getAll().size();
    }

    void ParametersComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
    {
        auto& params = owner.getProject().getParameters().getAll();
        if (row < 0 || row >= (int) params.size()) return;
        const auto& p = params[(size_t) row];

        if (selected)
        {
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.20f));
            g.fillRect (0, 0, w, h);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.fillRect (0, 0, 2, h);
        }

        // Bullet circle
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.fillEllipse (8.0f, h * 0.5f - 3.0f, 6.0f, 6.0f);

        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.0f));
        g.drawText (p.id, 22, 0, w - 90, h, juce::Justification::centredLeft);

        g.setColour (PatchCraftLookAndFeel::text());
        g.drawText (formatValue (p), w - 80, 0, 70, h, juce::Justification::centredRight);
    }

    void ParametersComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
    {
        auto& params = owner.getProject().getParameters().getAll();
        if (row < 0 || row >= (int) params.size()) return;
        // Select the layout element bound to this parameter id, if any.
        for (auto& el : owner.getProject().getLayout().getAll())
        {
            if (el.parameterId == params[(size_t) row].id)
            {
                owner.setSelectedElementId (el.id);
                return;
            }
        }
    }

} // namespace patchcraft
