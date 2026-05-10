#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace patchcraft
{
    class StudioMainComponent;
    class TestPage;

    /**
        Branding Lab — focused workspace for white-labeling the Player.
        Lets the developer edit the manifest's player-* fields (display name,
        tagline, logo image, accent / panel / text colours) and see a live
        mock of the Player chrome update underneath. The Design tab shapes
        the layout; this tab shapes the Player's identity.
    */
    class BrandingLabPage : public juce::Component,
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

    private:
        StudioMainComponent& owner;

        juce::Label header;
        juce::Label subtitle;

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

        juce::Label  accentLabel;
        juce::TextButton accentSwatch;
        juce::Label  panelLabel;
        juce::TextButton panelSwatch;
        juce::Label  bgLabel;
        juce::TextButton bgSwatch;
        juce::Label  textLabel;
        juce::TextButton textSwatch;

        juce::TextButton resetColoursBtn { "Reset to Default Colours" };

        juce::Rectangle<int> previewArea;
        juce::Rectangle<int> playerHeaderArea;
        bool syncingFromManifest = false;
        std::unique_ptr<juce::FileChooser> logoChooser;
        std::unique_ptr<TestPage> testPage;
        juce::ToggleButton showFormToggle { "Show Branding Form" };

        void timerCallback() override;
        void readFromManifest();
        void writeToManifest();
        void chooseColour (const juce::String& field, juce::Colour current);
        void chooseLogo();
        void paintPlayerPreview (juce::Graphics&, juce::Rectangle<int> r);
    };
}
