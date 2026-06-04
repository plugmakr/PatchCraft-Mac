#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace patchcraft
{
    class StudioMainComponent;

    class ExpansionsPage final : public juce::Component
    {
    public:
        explicit ExpansionsPage (StudioMainComponent& owner);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        enum class View
        {
            Catalog,
            VstExpansion,
            Checkout
        };

        struct Addon
        {
            juce::String id;
            juce::String name;
            juce::String eyebrow;
            juce::String shortDescription;
            juce::String price;
            juce::String status;
            juce::Colour accent;
            bool available = false;
        };

        StudioMainComponent& owner;
        View currentView = View::Catalog;

        std::vector<Addon> addons;
        juce::Rectangle<int> heroBounds;
        juce::Rectangle<int> catalogBounds;
        juce::Rectangle<int> detailBounds;
        juce::Rectangle<int> checkoutBounds;
        std::vector<juce::Rectangle<int>> addonCardBounds;

        juce::TextButton backButton { "Back" };
        juce::TextButton vstDetailsButton { "View VST Expansion" };
        juce::TextButton vstCheckoutButton { "Checkout Preview" };
        juce::TextButton pluginClubButton { "Continue on Plugin.club" };
        juce::TextButton buildPackageButton { "Build Addon Package" };

        void setView (View view);
        void rebuildAddons();
        void paintHero (juce::Graphics&);
        void paintCatalog (juce::Graphics&);
        void paintAddonCard (juce::Graphics&, const Addon&, juce::Rectangle<int>, bool featured);
        void paintVstExpansion (juce::Graphics&);
        void paintCheckout (juce::Graphics&);
        void paintPill (juce::Graphics&, juce::Rectangle<int>, const juce::String&, juce::Colour) const;
        void paintFeatureRow (juce::Graphics&, juce::Rectangle<int>, const juce::String&, const juce::String&, juce::Colour) const;
        void paintCheckoutStep (juce::Graphics&, juce::Rectangle<int>, int, const juce::String&, const juce::String&, bool active) const;
        void openPluginClub();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpansionsPage)
    };
}
