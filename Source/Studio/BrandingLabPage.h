#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <set>

namespace patchcraft
{
    class StudioMainComponent;
    class TestPage;

    /**
        Branding Lab — focused workspace for white-labeling the Player.
        Lets the developer edit the manifest's player-* fields (display name,
        tagline, logo image, accent / panel / text colours) and see a
        live preview of the Player chrome underneath. The Design tab shapes
        the layout; this tab shapes the Player's identity.
    */
    class BrandingLabPage : public juce::Component,
                            public juce::DragAndDropTarget,
                            private juce::Timer
    {
    public:
        explicit BrandingLabPage (StudioMainComponent& owner);
        ~BrandingLabPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        // The Brand Lab now hosts the live test environment too: the user
        // sees their instrument in a Player-style frame and can play it.
        // BottomPanel forwards page activation here.
        void activateTest();
        void deactivateTest();
        bool isTestActive() const;
        TestPage* getTestPage() const noexcept { return testPage.get(); }
        bool isInterestedInDragSource (const SourceDetails& details) override;
        void itemDropped (const SourceDetails& details) override;

    private:
        StudioMainComponent& owner;

        juce::Label header;
        juce::Label subtitle;
        juce::Viewport formViewport;
        juce::Component formContent;

        // Form fields.
        juce::Label  displayNameLabel;
        juce::TextEditor displayNameEdit;
        juce::Label  taglineLabel;
        juce::TextEditor taglineEdit;
        juce::Label  creatorLabel;
        juce::TextEditor creatorEdit;
        juce::Label  websiteLabel;
        juce::TextEditor websiteEdit;
        juce::Label  versionLabel;
        juce::TextEditor versionEdit;

        juce::Label  logoLabel;
        juce::TextEditor logoPathEdit;
        juce::TextButton browseLogoBtn { "Browse..." };
        juce::TextButton clearLogoBtn  { "Clear" };
        juce::Label  titleBannerImageLabel;
        juce::TextEditor titleBannerImageEdit;
        juce::TextButton browseTitleBannerBtn { "Browse..." };
        juce::TextButton clearTitleBannerBtn  { "Clear" };
        juce::Label  titleBarThemeLabel;
        juce::ComboBox titleBarThemeBox;
        juce::Label  titleTextPlacementLabel;
        juce::ComboBox titleTextPlacementBox;
        juce::Label  titleButtonStyleLabel;
        juce::ComboBox titleButtonStyleBox;
        juce::Label  titleFontLabel;
        juce::ComboBox titleFontBox;
        juce::Label  playerBackgroundImageLabel;
        juce::TextEditor playerBackgroundImageEdit;
        juce::TextButton browsePlayerBackgroundBtn { "Browse..." };
        juce::TextButton clearPlayerBackgroundBtn  { "Clear" };
        juce::Label  thumbnailImageLabel;
        juce::TextEditor thumbnailImageEdit;
        juce::TextButton browseThumbnailBtn { "Browse..." };
        juce::TextButton clearThumbnailBtn  { "Clear" };

        juce::Label  accentLabel;
        juce::TextButton accentSwatch;
        juce::Label  panelLabel;
        juce::TextButton panelSwatch;
        juce::Label  bgLabel;
        juce::TextButton bgSwatch;
        juce::Label  textLabel;
        juce::TextButton textSwatch;
        juce::Label  dimTextLabel;
        juce::TextButton dimTextSwatch;
        juce::Label  borderLabel;
        juce::TextButton borderSwatch;

        juce::TextButton resetColoursBtn { "Reset to Default Colours" };
        juce::TextButton identitySectionBtn { "Identity" };
        juce::TextButton skinSectionBtn { "Skin" };
        juce::TextButton runtimeSectionBtn { "Player Features" };
        juce::TextButton clientSectionBtn { "Client Package" };
        juce::TextButton licensingSectionBtn { "Licensing" };
        juce::Label runtimeSectionLabel;
        juce::ToggleButton showPackMenuToggle { "Show Pack Menu" };
        juce::ToggleButton allowPackLoadingToggle { "Allow External Pack Loading" };
        juce::ToggleButton showLibraryToggle { "Show Library Browser" };
        juce::ToggleButton allowMidiLearnToggle { "Allow MIDI Learn" };
        juce::ToggleButton showAboutToggle { "Show About Panel" };
        juce::ToggleButton showGuidanceToggle { "Show Parameter Guidance" };
        juce::ToggleButton showPatchCraftBrandingToggle { "Show PatchCraft Credit" };

        juce::Label clientSectionLabel;
        juce::Label clientNameLabel;
        juce::TextEditor clientNameEdit;
        juce::Label supportEmailLabel;
        juce::TextEditor supportEmailEdit;
        juce::Label supportUrlLabel;
        juce::TextEditor supportUrlEdit;
        juce::Label manualUrlLabel;
        juce::TextEditor manualUrlEdit;
        juce::Label storeUrlLabel;
        juce::TextEditor storeUrlEdit;
        juce::Label copyrightLabel;
        juce::TextEditor copyrightEdit;
        juce::Label legalTextLabel;
        juce::TextEditor legalTextEdit;
        juce::Label packageNameLabel;
        juce::TextEditor packageNameEdit;
        juce::Label publisherLabel;
        juce::TextEditor publisherEdit;
        juce::Label productCodeLabel;
        juce::TextEditor productCodeEdit;
        juce::Label bundleIdLabel;
        juce::TextEditor bundleIdEdit;
        juce::Label windowsVst3PathLabel;
        juce::TextEditor windowsVst3PathEdit;
        juce::Label macVst3PathLabel;
        juce::TextEditor macVst3PathEdit;
        juce::Label privacyUrlLabel;
        juce::TextEditor privacyUrlEdit;
        juce::Label installNotesLabel;
        juce::TextEditor installNotesEdit;
        juce::ToggleButton includeVst3Toggle { "Include VST3" };
        juce::ToggleButton includeStandaloneToggle { "Include Standalone" };
        juce::ToggleButton requireLicenseFirstRunToggle { "Require License On First Run" };

        juce::Label commerceSectionLabel;
        juce::ToggleButton licenseRequiredToggle { "Require License" };
        juce::ToggleButton bindMachineToggle { "Bind License To Machine" };
        juce::Label productIdLabel;
        juce::TextEditor productIdEdit;
        juce::Label licenseUrlLabel;
        juce::TextEditor licenseUrlEdit;
        juce::Label trialDaysLabel;
        juce::Slider trialDaysSlider;
        juce::Label offlineGraceLabel;
        juce::Slider offlineGraceSlider;
        juce::TextButton applyWhiteLabelPresetBtn { "Apply White-Label Player Defaults" };

        juce::Rectangle<int> previewArea;
        juce::Rectangle<int> playerHeaderArea;
        bool syncingFromManifest = false;
        bool pendingProjectNotify = false;
        int  ticksSinceLastEdit = 0;
        std::unique_ptr<juce::FileChooser> logoChooser;
        std::unique_ptr<TestPage> testPage;
        juce::ToggleButton showFormToggle { "Show Branding Form" };
        juce::TextButton playerFileBtn { "File" };
        juce::TextButton playerLibraryBtn { "Library" };
        juce::TextButton playerViewBtn { "View" };
        juce::TextButton playerToolsBtn { "Tools" };
        juce::TextButton playerSoundBtn { "Sound" };
        juce::TextButton playerRackBtn { "Rack" };
        juce::TextButton playerControlBtn { "Control" };
        juce::TextButton playerMidiClipBtn { "MIDI Clip" };
        juce::TextButton playerPlayBtn { "Play" };
        juce::TextButton playerStopBtn { "Stop" };
        juce::Label playerBpmLabel;
        juce::Slider playerBpmSlider;
        juce::TextButton playerPrevPresetBtn { "<" };
        juce::TextButton playerPresetBtn { "Current Patch" };
        juce::TextButton playerNextPresetBtn { ">" };
        bool identitySectionOpen = true;
        bool skinSectionOpen = true;
        bool runtimeSectionOpen = true;
        bool clientSectionOpen = false;
        bool licensingSectionOpen = true;
        std::set<std::string> favoritePresetNames;
        bool presetAuditionOnSelect = true;
        bool presetCloseAfterLoad = false;

        void timerCallback() override;
        void readFromManifest();
        void writeToManifest();
        void scheduleProjectNotify();
        void chooseColour (const juce::String& field, juce::Colour current);
        void chooseLogo();
        void chooseImagePath (juce::TextEditor& targetEditor, const juce::String& title);
        void showPlayerPreviewPanel (const juce::String& title, const juce::String& body);
        void showPlayerSoundPanel();
        void showPlayerRackPanel();
        void showPlayerControlPanel();
        void showRuntimeRoutingPanel();
        void showPlayerLibraryPanel();
        void showPlayerFileMenu();
        void showPlayerToolsMenu();
        void showPlayerPresetMenu();
        void paintPlayerPreview (juce::Graphics&, juce::Rectangle<int> r);
    };
}
