#include "BuiltAssetLibraryComponent.h"
#include "StudioMainComponent.h"
#include "PatchCraftLookAndFeel.h"

#include <algorithm>

namespace patchcraft
{
    BuiltAssetLibraryComponent::BuiltAssetLibraryComponent (StudioMainComponent& o) : owner (o)
    {
        title.setText ("Built Asset Library", juce::dontSendNotification);
        title.setFont (juce::Font (13.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        addAndMakeVisible (title);

        refreshButton.getProperties().set ("smallButton", true);
        addButton.getProperties().set ("smallButton", true);
        refreshButton.onClick = [this] { refresh(); };
        addButton.onClick = [this] { addSelectedToCanvas(); };
        addAndMakeVisible (refreshButton);
        addAndMakeVisible (addButton);

        list.setRowHeight (54);
        list.setOutlineThickness (0);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (list);
        refresh();
    }

    juce::File BuiltAssetLibraryComponent::getAssetLibraryRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("PatchCraft")
            .getChildFile ("AssetLibrary");
    }

    juce::File BuiltAssetLibraryComponent::getCategoryFolder (const juce::String& category)
    {
        return getAssetLibraryRoot().getChildFile (category.toLowerCase());
    }

    void BuiltAssetLibraryComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::panel());
        g.setColour (PatchCraftLookAndFeel::border());
        g.fillRect (0, 34, getWidth(), 1);

        if (entries.empty())
        {
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (11.5f);
            g.drawFittedText ("No built assets yet.\nUse Build > Add To Library to add knobs, sliders, and meters.",
                              getLocalBounds().reduced (14).withTrimmedTop (46),
                              juce::Justification::centred, 4);
        }
    }

    void BuiltAssetLibraryComponent::resized()
    {
        auto r = getLocalBounds().reduced (5);
        auto top = r.removeFromTop (30);
        title.setBounds (top.removeFromLeft (juce::jmax (80, top.getWidth() - 118)));
        refreshButton.setBounds (top.removeFromLeft (62).reduced (2));
        addButton.setBounds (top.removeFromLeft (50).reduced (2));
        r.removeFromTop (5);
        list.setBounds (r);
    }

    void BuiltAssetLibraryComponent::refresh()
    {
        entries.clear();
        for (const auto& category : { juce::String ("knobs"), juce::String ("sliders"), juce::String ("meters") })
        {
            auto folder = getCategoryFolder (category);
            folder.createDirectory();
            for (auto file : folder.findChildFiles (juce::File::findFiles, false, "*.png"))
                entries.push_back (inspectAssetFile (file, category));
        }

        std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
        {
            if (a.category != b.category)
                return a.category < b.category;
            return a.file.getFileName().compareIgnoreCase (b.file.getFileName()) < 0;
        });

        list.updateContent();
        list.repaint();
        repaint();
    }

    BuiltAssetLibraryComponent::Entry BuiltAssetLibraryComponent::inspectAssetFile (juce::File file,
                                                                                   juce::String category)
    {
        Entry entry;
        entry.file = std::move (file);
        entry.category = std::move (category);

        auto sidecarForCategory = [&]
        {
            if (entry.category.startsWithIgnoreCase ("slider"))
                return entry.file.withFileExtension ("patchcraft-slider.json");
            if (entry.category.startsWithIgnoreCase ("meter"))
                return entry.file.withFileExtension ("patchcraft-meter.json");
            return entry.file.withFileExtension ("patchcraft-knob.json");
        };

        auto sidecar = sidecarForCategory();
        if (sidecar.existsAsFile())
        {
            auto parsed = juce::JSON::parse (sidecar);
            if (auto* root = parsed.getDynamicObject())
            {
                const int frames = (int) root->getProperty ("frames");
                if (frames > 0)
                    entry.frames = frames;
                if (root->hasProperty ("stripVertical"))
                    entry.vertical = (bool) root->getProperty ("stripVertical");
                if (auto* style = root->getProperty ("style").getDynamicObject())
                {
                    const int styleFrames = (int) style->getProperty ("frames");
                    if (styleFrames > 0 && entry.frames <= 1)
                        entry.frames = styleFrames;
                    if (style->hasProperty ("outputMode"))
                        entry.vertical = ! style->getProperty ("outputMode").toString().containsIgnoreCase ("horizontal");
                }
            }
        }

        auto image = juce::ImageFileFormat::loadFrom (entry.file);
        if (image.isValid())
        {
            const int width = image.getWidth();
            const int height = image.getHeight();
            if (entry.frames > 1)
            {
                if (entry.vertical && height / entry.frames <= 0)
                    entry.frames = 1;
                if (! entry.vertical && width / entry.frames <= 0)
                    entry.frames = 1;
            }
            else if (height >= width && width > 0 && height % width == 0)
            {
                entry.vertical = true;
                entry.frames = juce::jmax (1, height / width);
            }
            else if (width >= height && height > 0 && width % height == 0)
            {
                entry.vertical = false;
                entry.frames = juce::jmax (1, width / height);
            }
        }
        return entry;
    }

    int BuiltAssetLibraryComponent::getNumRows()
    {
        return (int) entries.size();
    }

    void BuiltAssetLibraryComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= (int) entries.size())
            return;

        const auto& entry = entries[(size_t) row];
        auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (4, 3);
        g.setColour (selected ? PatchCraftLookAndFeel::raised() : PatchCraftLookAndFeel::panelAlt());
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);
        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::border());
        g.drawRoundedRectangle (bounds.toFloat(), 6.0f, 1.0f);

        auto thumb = bounds.removeFromLeft (46).reduced (5);
        auto image = juce::ImageFileFormat::loadFrom (entry.file);
        if (image.isValid())
        {
            const int frameW = entry.vertical ? image.getWidth()
                                              : juce::jmax (1, image.getWidth() / juce::jmax (1, entry.frames));
            const int frameH = entry.vertical ? juce::jmax (1, image.getHeight() / juce::jmax (1, entry.frames))
                                              : image.getHeight();
            auto frame = image.getClippedImage ({ 0, 0, frameW, frameH });
            g.drawImageWithin (frame, thumb.getX(), thumb.getY(), thumb.getWidth(), thumb.getHeight(),
                               juce::RectanglePlacement::centred);
        }

        g.setColour (selected ? PatchCraftLookAndFeel::accent() : PatchCraftLookAndFeel::text());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (entry.file.getFileNameWithoutExtension(), bounds.removeFromTop (22), juce::Justification::centredLeft);
        g.setColour (PatchCraftLookAndFeel::textDim());
        g.setFont (10.5f);
        g.drawText (entry.category + "  |  " + juce::String (entry.frames) + " frames  |  "
                    + (entry.vertical ? "vertical" : "horizontal"),
                    bounds, juce::Justification::centredLeft);
    }

    void BuiltAssetLibraryComponent::selectedRowsChanged (int lastRowSelected)
    {
        selectedRow = lastRowSelected;
    }

    void BuiltAssetLibraryComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        selectedRow = row;
        addSelectedToCanvas();
    }

    void BuiltAssetLibraryComponent::addSelectedToCanvas()
    {
        if (selectedRow < 0 || selectedRow >= (int) entries.size())
            return;

        const auto& entry = entries[(size_t) selectedRow];
        owner.addLibraryAssetToCanvas (entry.category, entry.file, entry.frames, entry.vertical);
    }
}
