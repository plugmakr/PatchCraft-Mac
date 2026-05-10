#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class PatchCraftProject;

    /**
        Component for editing library metadata for instruments.
        Allows customization of thumbnail, tags, version, website, and other
        manifest fields for the library browser display.
    */
    class LibraryMetadataComponent : public juce::Component
    {
    public:
        explicit LibraryMetadataComponent (PatchCraftProject& proj);
        ~LibraryMetadataComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();
        void applyChanges();

    private:
        PatchCraftProject& project;

        // Basic info fields
        juce::Label nameLabel;
        juce::TextEditor nameEditor;
        juce::Label creatorLabel;
        juce::TextEditor creatorEditor;
        juce::Label categoryLabel;
        juce::TextEditor categoryEditor;
        juce::Label versionLabel;
        juce::TextEditor versionEditor;
        juce::Label websiteLabel;
        juce::TextEditor websiteEditor;

        // Description
        juce::Label descriptionLabel;
        juce::TextEditor descriptionEditor;

        // Thumbnail path
        juce::Label thumbnailLabel;
        juce::TextEditor thumbnailEditor;
        juce::TextButton browseThumbnailBtn { "Browse..." };

        // Tags
        juce::Label tagsLabel;
        juce::TextEditor tagsEditor;
        juce::Label tagsHint;

        // Player customization
        juce::Label playerHeaderLabel;
        juce::Label playerNameLabel;
        juce::TextEditor playerNameEditor;
        juce::Label playerTaglineLabel;
        juce::TextEditor playerTaglineEditor;
        juce::Label playerAccentLabel;
        juce::TextEditor playerAccentEditor;
        juce::Label playerBgLabel;
        juce::TextEditor playerBgEditor;
        juce::ToggleButton showPackMenuToggle { "Show Pack Menu" };
        juce::ToggleButton allowPackLoadingToggle { "Allow Pack Loading" };
        juce::ToggleButton showLibraryToggle { "Show Library Browser" };
        juce::ToggleButton allowMidiLearnToggle { "Allow MIDI Learn" };
        juce::ToggleButton showAboutToggle { "Show About" };
        juce::ToggleButton showParameterGuidanceToggle { "Show Help Tooltips" };

        // Save button
        juce::TextButton saveBtn { "Save Metadata" };

        void browseThumbnail();
        void saveMetadata();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryMetadataComponent)
    };

} // namespace patchcraft
