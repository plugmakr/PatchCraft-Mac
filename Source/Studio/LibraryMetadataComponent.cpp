#include "LibraryMetadataComponent.h"
#include "PatchCraftProject.h"
#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    namespace
    {
        static juce::Colour parseManifestColour (juce::String text, juce::Colour fallback)
        {
            text = text.trim().removeCharacters ("#");
            if (text.length() == 6)
                text = "ff" + text;
            return text.length() == 8 ? juce::Colour::fromString (text) : fallback;
        }
    }

    LibraryMetadataComponent::LibraryMetadataComponent (PatchCraftProject& proj)
        : project (proj)
    {
        // Basic info labels
        nameLabel.setText ("Instrument Name:", juce::dontSendNotification);
        creatorLabel.setText ("Creator:", juce::dontSendNotification);
        categoryLabel.setText ("Category:", juce::dontSendNotification);
        versionLabel.setText ("Version:", juce::dontSendNotification);
        websiteLabel.setText ("Website:", juce::dontSendNotification);

        // Description label
        descriptionLabel.setText ("Description:", juce::dontSendNotification);

        // Thumbnail label
        thumbnailLabel.setText ("Library Thumbnail:", juce::dontSendNotification);

        // Tags label
        tagsLabel.setText ("Tags (comma-separated):", juce::dontSendNotification);
        tagsHint.setText ("e.g. synth, bass, pad, cinematic", juce::dontSendNotification);
        tagsHint.setFont (juce::FontOptions (11.0f));
        tagsHint.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        playerHeaderLabel.setText ("Player Branding / Runtime", juce::dontSendNotification);
        playerHeaderLabel.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        playerHeaderLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        playerNameLabel.setText ("Player Name:", juce::dontSendNotification);
        playerTaglineLabel.setText ("Tagline:", juce::dontSendNotification);
        playerAccentLabel.setText ("Accent Hex:", juce::dontSendNotification);
        playerBgLabel.setText ("Background Hex:", juce::dontSendNotification);

        // Configure editors
        nameEditor.setMultiLine (false);
        creatorEditor.setMultiLine (false);
        categoryEditor.setMultiLine (false);
        versionEditor.setMultiLine (false);
        websiteEditor.setMultiLine (false);
        descriptionEditor.setMultiLine (true);
        descriptionEditor.setReturnKeyStartsNewLine (true);
        thumbnailEditor.setMultiLine (false);
        thumbnailEditor.setReadOnly (true);
        tagsEditor.setMultiLine (false);
        playerNameEditor.setMultiLine (false);
        playerTaglineEditor.setMultiLine (false);
        playerAccentEditor.setMultiLine (false);
        playerBgEditor.setMultiLine (false);
        playerAccentEditor.setTooltip ("Runtime Player accent colour, e.g. FFF5A623 or #F5A623.");
        playerBgEditor.setTooltip ("Runtime Player fallback background colour, e.g. FF0B0D10 or #0B0D10.");
        showPackMenuToggle.setTooltip ("When off, hides the Player's top-right Pack menu for white-label/sold instruments.");
        allowPackLoadingToggle.setTooltip ("When off, disables Load Pack and Reset Demo actions in the Player menu.");
        showLibraryToggle.setTooltip ("When off, disables the Player Library Browser menu item.");
        allowMidiLearnToggle.setTooltip ("When off, exported Player controls do not offer MIDI Learn.");
        showAboutToggle.setTooltip ("When off, hides About/product metadata in the Player menu.");
        showParameterGuidanceToggle.setTooltip ("When on, disconnected or disabled controls explain why they do not move or affect sound, and how to connect them.");

        // Browse button
        browseThumbnailBtn.onClick = [this] { browseThumbnail(); };

        // Save button
        saveBtn.getProperties().set ("accent", true);
        saveBtn.onClick = [this] { saveMetadata(); };

        // Add all components
        addAndMakeVisible (nameLabel);
        addAndMakeVisible (nameEditor);
        addAndMakeVisible (creatorLabel);
        addAndMakeVisible (creatorEditor);
        addAndMakeVisible (categoryLabel);
        addAndMakeVisible (categoryEditor);
        addAndMakeVisible (versionLabel);
        addAndMakeVisible (versionEditor);
        addAndMakeVisible (websiteLabel);
        addAndMakeVisible (websiteEditor);
        addAndMakeVisible (descriptionLabel);
        addAndMakeVisible (descriptionEditor);
        addAndMakeVisible (thumbnailLabel);
        addAndMakeVisible (thumbnailEditor);
        addAndMakeVisible (browseThumbnailBtn);
        addAndMakeVisible (tagsLabel);
        addAndMakeVisible (tagsEditor);
        addAndMakeVisible (tagsHint);
        addAndMakeVisible (playerHeaderLabel);
        addAndMakeVisible (playerNameLabel);
        addAndMakeVisible (playerNameEditor);
        addAndMakeVisible (playerTaglineLabel);
        addAndMakeVisible (playerTaglineEditor);
        addAndMakeVisible (playerAccentLabel);
        addAndMakeVisible (playerAccentEditor);
        addAndMakeVisible (playerBgLabel);
        addAndMakeVisible (playerBgEditor);
        addAndMakeVisible (showPackMenuToggle);
        addAndMakeVisible (allowPackLoadingToggle);
        addAndMakeVisible (showLibraryToggle);
        addAndMakeVisible (allowMidiLearnToggle);
        addAndMakeVisible (showAboutToggle);
        addAndMakeVisible (showParameterGuidanceToggle);
        addAndMakeVisible (saveBtn);

        refresh();
    }

    LibraryMetadataComponent::~LibraryMetadataComponent() = default;

    void LibraryMetadataComponent::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());
    }

    void LibraryMetadataComponent::resized()
    {
        auto bounds = getLocalBounds().reduced (20);
        const int rowHeight = 30;
        const int labelWidth = 120;
        int y = 10;

        // Name
        nameLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        nameEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;

        // Creator
        creatorLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        creatorEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;

        // Category
        categoryLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        categoryEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;

        // Version
        versionLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        versionEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;

        // Website
        websiteLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        websiteEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;

        // Description (taller)
        descriptionLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        descriptionEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, 80);
        y += 80 + 10;

        // Thumbnail
        thumbnailLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        thumbnailEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10 - 100, rowHeight);
        browseThumbnailBtn.setBounds (bounds.getRight() - 100, y, 90, rowHeight);
        y += rowHeight + 10;

        // Tags
        tagsLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        tagsEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 5;
        tagsHint.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, 20);
        y += 30;

        // Save button
        playerHeaderLabel.setBounds (bounds.getX(), y, bounds.getWidth(), 24);
        y += 30;
        playerNameLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        playerNameEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;
        playerTaglineLabel.setBounds (bounds.getX(), y, labelWidth, rowHeight);
        playerTaglineEditor.setBounds (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, rowHeight);
        y += rowHeight + 10;
        auto colourRow = juce::Rectangle<int> (bounds.getX(), y, bounds.getWidth(), rowHeight);
        playerAccentLabel.setBounds (colourRow.removeFromLeft (labelWidth));
        playerAccentEditor.setBounds (colourRow.removeFromLeft ((bounds.getWidth() - labelWidth) / 2 - 10).reduced (10, 0));
        playerBgLabel.setBounds (colourRow.removeFromLeft (labelWidth));
        playerBgEditor.setBounds (colourRow.reduced (10, 0));
        y += rowHeight + 10;
        auto toggles = juce::Rectangle<int> (bounds.getX() + labelWidth + 10, y, bounds.getWidth() - labelWidth - 10, 54);
        showPackMenuToggle.setBounds (toggles.removeFromTop (26).removeFromLeft (160));
        allowPackLoadingToggle.setBounds (toggles.removeFromLeft (170));
        showLibraryToggle.setBounds (toggles.removeFromLeft (180));
        toggles = juce::Rectangle<int> (bounds.getX() + labelWidth + 10, y + 26, bounds.getWidth() - labelWidth - 10, 26);
        allowMidiLearnToggle.setBounds (toggles.removeFromLeft (160));
        showAboutToggle.setBounds (toggles.removeFromLeft (140));
        showParameterGuidanceToggle.setBounds (toggles.removeFromLeft (220));
        y += 64;

        // Save button
        saveBtn.setBounds (bounds.getX(), y, 120, rowHeight);
    }

    void LibraryMetadataComponent::refresh()
    {
        auto& manifest = project.getManifest();

        nameEditor.setText (manifest.instrumentName, juce::dontSendNotification);
        creatorEditor.setText (manifest.creator, juce::dontSendNotification);
        categoryEditor.setText (manifest.category, juce::dontSendNotification);
        versionEditor.setText (manifest.version, juce::dontSendNotification);
        websiteEditor.setText (manifest.website, juce::dontSendNotification);
        descriptionEditor.setText (manifest.description, juce::dontSendNotification);
        thumbnailEditor.setText (manifest.libraryThumbnail, juce::dontSendNotification);
        playerNameEditor.setText (manifest.playerDisplayName.isNotEmpty() ? manifest.playerDisplayName
                                                                          : manifest.instrumentName,
                                  juce::dontSendNotification);
        playerTaglineEditor.setText (manifest.playerTagline, juce::dontSendNotification);
        playerAccentEditor.setText (manifest.playerAccentColour.toString(), juce::dontSendNotification);
        playerBgEditor.setText (manifest.playerBackgroundColour.toString(), juce::dontSendNotification);
        showPackMenuToggle.setToggleState (manifest.playerShowPackMenu, juce::dontSendNotification);
        allowPackLoadingToggle.setToggleState (manifest.playerAllowPackLoading, juce::dontSendNotification);
        showLibraryToggle.setToggleState (manifest.playerShowLibraryBrowser, juce::dontSendNotification);
        allowMidiLearnToggle.setToggleState (manifest.playerAllowMidiLearn, juce::dontSendNotification);
        showAboutToggle.setToggleState (manifest.playerShowAbout, juce::dontSendNotification);
        showParameterGuidanceToggle.setToggleState (manifest.playerShowParameterGuidance, juce::dontSendNotification);

        // Convert tags array to comma-separated string
        juce::String tagsStr;
        for (int i = 0; i < manifest.tags.size(); ++i)
        {
            if (i > 0) tagsStr += ", ";
            tagsStr += manifest.tags[i];
        }
        tagsEditor.setText (tagsStr, juce::dontSendNotification);
    }

    void LibraryMetadataComponent::applyChanges()
    {
        saveMetadata();
    }

    void LibraryMetadataComponent::browseThumbnail()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Select Library Thumbnail",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.png;*.jpg;*.jpeg");

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result != juce::File())
                {
                    // Store as relative path if inside project assets, otherwise absolute
                    auto assetsFolder = project.getProjectFolder().getChildFile ("assets");
                    if (result.isAChildOf (assetsFolder))
                    {
                        thumbnailEditor.setText (result.getRelativePathFrom (assetsFolder), juce::dontSendNotification);
                    }
                    else
                    {
                        thumbnailEditor.setText (result.getFullPathName(), juce::dontSendNotification);
                    }
                }
            });
    }

    void LibraryMetadataComponent::saveMetadata()
    {
        auto& manifest = project.getManifest();

        manifest.instrumentName = nameEditor.getText().trim();
        manifest.creator = creatorEditor.getText().trim();
        manifest.category = categoryEditor.getText().trim();
        manifest.version = versionEditor.getText().trim();
        manifest.website = websiteEditor.getText().trim();
        manifest.description = descriptionEditor.getText().trim();
        manifest.libraryThumbnail = thumbnailEditor.getText().trim();
        manifest.playerDisplayName = playerNameEditor.getText().trim();
        manifest.playerTagline = playerTaglineEditor.getText().trim();
        manifest.playerAccentColour = parseManifestColour (playerAccentEditor.getText(), manifest.playerAccentColour);
        manifest.playerBackgroundColour = parseManifestColour (playerBgEditor.getText(), manifest.playerBackgroundColour);
        manifest.playerShowPackMenu = showPackMenuToggle.getToggleState();
        manifest.playerAllowPackLoading = allowPackLoadingToggle.getToggleState();
        manifest.playerShowLibraryBrowser = showLibraryToggle.getToggleState();
        manifest.playerAllowMidiLearn = allowMidiLearnToggle.getToggleState();
        manifest.playerShowAbout = showAboutToggle.getToggleState();
        manifest.playerShowParameterGuidance = showParameterGuidanceToggle.getToggleState();

        // Parse tags from comma-separated string
        manifest.tags.clear();
        auto tagsStr = tagsEditor.getText().trim();
        if (tagsStr.isNotEmpty())
        {
            auto tokens = juce::StringArray::fromTokens (tagsStr, ",", "\"");
            for (auto& token : tokens)
            {
                auto trimmed = token.trim();
                if (trimmed.isNotEmpty())
                    manifest.tags.add (trimmed);
            }
        }

        project.notifyChanged();
    }

} // namespace patchcraft
