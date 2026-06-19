#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>
#include <vector>

namespace patchcraft
{
    class StudioMainComponent;

    class LaunchCenterPage final : public juce::Component
    {
    public:
        enum class Severity { Pass, Warning, Error, Info };

        explicit LaunchCenterPage (StudioMainComponent& owner);
        ~LaunchCenterPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void refresh();

    private:
        struct CheckItem
        {
            Severity severity = Severity::Info;
            juce::String title;
            juce::String detail;
            juce::String actionLabel;
            std::function<void()> action;
        };

        class CheckRow;
        class DemoTile;

        StudioMainComponent& owner;

        juce::Label title;
        juce::Label subtitle;
        juce::Label statusBadge;
        juce::Label summaryLabel;
        juce::Label exportShipLabel;
        juce::Label exportToolsLabel;

        juce::TextButton refreshButton { "Run Launch Doctor" };
        juce::TextButton outputFolderButton { "Output Folder" };
        juce::TextButton bundleButton { "Create Launch Bundle" };
        juce::TextButton customerWizardButton { "Customer Kit" };
        juce::TextButton productPageButton { "Product Page" };
        juce::TextButton testButton { "Test Runtime" };
        juce::TextButton exportPackButton { "Export Pack" };
        juce::TextButton exportVstButton { "Export VST3" };
        juce::TextButton publishButton { "Publish Draft" };
        juce::Label outputFolderLabel;

        juce::Label creatorTitle;
        juce::Label creatorBody;
        juce::TextEditor recipePrompt;
        juce::ComboBox recipeTypeBox;
        juce::TextButton createFromPromptButton { "Create Plugin" };
        juce::TextButton blankProjectButton { "Blank Canvas" };
        juce::TextButton synthStarterButton { "Synth" };
        juce::TextButton sampleStarterButton { "Sampler" };
        juce::TextButton drumStarterButton { "Drums" };
        juce::TextButton fxStarterButton { "FX" };

        juce::Label demoTitle;
        juce::Label demoBody;
        juce::Viewport demoViewport;
        juce::Component demoContent;
        std::vector<std::unique_ptr<DemoTile>> demoTiles;

        enum class ContentTab { Overview, Create, Demos, Doctor };
        ContentTab activeTab = ContentTab::Overview;
        juce::TextButton tabOverview { "Overview" };
        juce::TextButton tabCreate { "Create" };
        juce::TextButton tabDemos { "Demos" };
        juce::TextButton tabDoctor { "Launch Doctor" };
        juce::Label doctorTitle;
        juce::Label doctorBody;

        juce::Viewport checksViewport;
        juce::Component checksContent;
        std::vector<std::unique_ptr<CheckRow>> checkRows;
        std::unique_ptr<juce::FileChooser> outputFolderChooser;
        juce::File selectedLaunchRoot;

        juce::String launchSummary;
        int errorCount = 0;
        int warningCount = 0;
        int passCount = 0;

        std::vector<CheckItem> buildChecks();
        void rebuildRows();
        void rebuildDemoTiles();
        void createSimplePluginFromPrompt (juce::String forcedType = {});
        void createStarterPlugin (const juce::String& engineId,
                                  const juce::String& productName,
                                  const juce::String& description);
        juce::String inferEngineFromPrompt (const juce::String& prompt,
                                            const juce::String& forcedType) const;
        juce::String productNameFromPrompt (const juce::String& prompt,
                                            const juce::String& engineId) const;
        void chooseOutputFolder();
        void createLaunchBundle();
        void showCustomerPackageWizard();
        void showProductPagePreview();
        void showSalesPageBuilder();
        void writeProductPageOnly();
        void writeSalesPageOnly();

        juce::String buildReadinessMarkdown();
        juce::String buildProductPageMarkdown() const;
        juce::String buildSalesPageMarkdown() const;
        juce::String buildSalesPageHtml() const;
        juce::String buildTestPlanMarkdown() const;
        juce::String buildInstallerChecklistMarkdown() const;
        juce::String buildClientDeliveryMarkdown() const;
        juce::String buildCustomerPackageWizardMarkdown() const;
        juce::String buildSellerLaunchPlaybookMarkdown() const;
        juce::String buildBuyerQuickStartMarkdown() const;
        juce::String buildMarketplaceAssetChecklistMarkdown() const;
        juce::String buildInstallerReadmeMarkdown() const;
        juce::String buildWindowsInstallerScript() const;
        juce::String buildMacInstallerNotes() const;
        juce::String buildActivationFlowMarkdown() const;
        juce::var buildLaunchArtifactManifest (const juce::File& launchFolder) const;
        juce::var buildPluginClubMetadataPreview() const;
        juce::var buildWhiteLabelProductManifest() const;
        juce::var buildReleaseManifest() const;

        juce::File defaultLaunchRoot() const;
        juce::File launchRoot() const;
        juce::File createLaunchFolder (juce::String& error) const;
        void updateOutputFolderLabel();
        bool writeTextFile (const juce::File& file, const juce::String& text, juce::String& error) const;
        void showMessage (const juce::String& titleText,
                          const juce::String& message,
                          juce::MessageBoxIconType icon) const;
        void styleActionButton (juce::TextButton& button, bool primary);
        void setActiveTab (ContentTab tab);
        void updateTabBar();
        void applyTabVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaunchCenterPage)
    };
}
