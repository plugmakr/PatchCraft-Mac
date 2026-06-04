#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class BuiltAssetLibraryComponent : public juce::Component,
                                       private juce::ListBoxModel,
                                       public juce::FileDragAndDropTarget,
                                       public juce::DragAndDropTarget
    {
    public:
        explicit BuiltAssetLibraryComponent (StudioMainComponent& owner);
        ~BuiltAssetLibraryComponent() override;

        static juce::File getAssetLibraryRoot();
        static juce::File getCategoryFolder (const juce::String& category);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();
        void showSoundsLibrary();

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

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragMove (const juce::StringArray& files, int x, int y) override;
        void fileDragExit (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

        bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
        void itemDragMove (const SourceDetails& dragSourceDetails) override;
        void itemDragExit (const SourceDetails& dragSourceDetails) override;
        void itemDropped (const SourceDetails& dragSourceDetails) override;

        void addSelectedToCanvas();
        void setMode (LibraryMode);
        void scanBackgrounds();
        void scanTemplates();
        void scanBuiltAssets();
        void scanBrandingAssets();
        void scanSounds();
        void addFolderEntriesForRoot (const juce::File& root, const juce::String& category, bool skipPatchcraftTemplates);
        void selectEntryForFile (const juce::File& file);
        void showSelectedPreview();
        void importAssets();
        void deleteSelectedEntry();
        void createFolderForMode();
        juce::File getWritableModeRoot() const;
        juce::File getTargetFolderForImport (juce::Point<int> localPosition = {}) const;
        int rowAtLocalPosition (juce::Point<int> localPosition) const;
        bool entryCanBeDroppedIntoCurrentMode (const juce::File& file) const;
        void copyExternalFilesIntoFolder (const juce::StringArray& paths, const juce::File& targetFolder);
        void moveOrCopyLibraryEntryToFolder (const juce::File& source, const juce::File& targetFolder);
        juce::Array<juce::File> selectedEntriesForCurrentDrag() const;
        void auditionSoundFile (const juce::File& file);
        void stopSoundPreview();
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
        juce::ToggleButton autoAuditionToggle { "Auto" };
        std::unique_ptr<juce::FileChooser> importChooser;
        juce::AudioFormatManager previewFormatManager;
        juce::AudioSourcePlayer previewPlayer;
        juce::AudioTransportSource previewTransport;
        std::unique_ptr<juce::AudioFormatReaderSource> previewReaderSource;
        bool previewCallbackActive = false;
        int selectedRow = -1;
        int dropTargetRow = -1;
        juce::File activeFolder;
    };
}
