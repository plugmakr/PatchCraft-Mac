#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class BuiltAssetLibraryComponent : public juce::Component,
                                       private juce::ListBoxModel
    {
    public:
        explicit BuiltAssetLibraryComponent (StudioMainComponent& owner);

        static juce::File getAssetLibraryRoot();
        static juce::File getCategoryFolder (const juce::String& category);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        enum class LibraryMode
        {
            Backgrounds,
            Templates,
            Assets,
            Sounds
        };

        struct Entry
        {
            juce::String category;
            juce::File file;
            juce::String title;
            juce::String subtitle;
            juce::String folderPath;
            int frames = 1;
            bool vertical = true;
            bool isFolder = false;
        };

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRowSelected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        juce::var getDragSourceDescription (const juce::SparseSet<int>& selectedRows) override;

        void addSelectedToCanvas();
        void setMode (LibraryMode);
        void scanBackgrounds();
        void scanTemplates();
        void scanBuiltAssets();
        void scanSounds();
        void addFolderEntriesForRoot (const juce::File& root, const juce::String& category, bool skipPatchcraftTemplates);
        void selectEntryForFile (const juce::File& file);
        void showSelectedPreview();
        void importAssets();
        void deleteSelectedEntry();
        void createFolderForMode();
        juce::File getWritableModeRoot() const;
        static bool isUserLibraryFile (const juce::File& file);
        static bool isSupportedImageFile (const juce::File& file);
        static bool isSupportedAssetFile (const juce::File& file);
        static bool isSupportedSoundFile (const juce::File& file);
        static juce::String folderLabelFor (const juce::File& root, const juce::File& file);
        static juce::File makeUniqueChildFile (const juce::File& folder, const juce::String& fileName);
        static void copyAssetWithSidecars (const juce::File& source, const juce::File& destinationFolder, juce::StringArray& errors);
        static juce::Array<juce::File> getTemplateRoots();
        static juce::Array<juce::File> getBackgroundRoots();
        static juce::Array<juce::File> getSoundRoots();
        static Entry inspectAssetFile (juce::File file, juce::String category);

        StudioMainComponent& owner;
        std::vector<Entry> entries;
        LibraryMode mode = LibraryMode::Backgrounds;
        juce::ListBox list { "builtAssetLibrary", this };
        juce::Label title;
        juce::TextButton backgroundsButton { "Backgrounds" };
        juce::TextButton templatesButton { "Templates" };
        juce::TextButton assetsButton { "Assets" };
        juce::TextButton soundsButton { "Sounds" };
        juce::TextButton refreshButton { "Refresh" };
        juce::TextButton importButton { "Import" };
        juce::TextButton folderButton { "Folder" };
        juce::TextButton deleteButton { "Delete" };
        juce::TextButton addButton { "Add" };
        juce::TextButton previewButton { "Preview" };
        std::unique_ptr<juce::FileChooser> importChooser;
        int selectedRow = -1;
    };
}
