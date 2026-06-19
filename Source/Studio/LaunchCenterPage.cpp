#include "LaunchCenterPage.h"

#include "BottomPanel.h"
#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"

#include "AiAssistService.h"
#include "LicenseValidator.h"
#include "ParameterModel.h"
#include "PatchCraftPackWriter.h"
#include "PluginClubPublisher.h"
#include "SampleMap.h"
#include "VstExportModule.h"

#include <algorithm>
#include <map>
#include <juce_cryptography/juce_cryptography.h>

namespace patchcraft
{
    namespace
    {
        constexpr int kApprovedFactoryDemoCount = 10;

        static juce::String safeSlug (juce::String text)
        {
            text = text.trim();
            juce::String out;
            for (auto c : text)
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_')
                    out << c;
                else if (c == ' ' || c == '.' || c == '#')
                    out << '_';
            }
            return out.isNotEmpty() ? out : juce::String ("PatchCraftLaunch");
        }

        static juce::String engineDisplayName (juce::String engine)
        {
            engine = engine.toLowerCase();
            if (engine == "synth") return "Synth Instrument";
            if (engine == "fx") return "FX Plugin";
            if (engine == "drum") return "Drum Machine";
            if (engine == "sample") return "Sample Instrument";
            return engine.isNotEmpty() ? engine : "Unknown";
        }

        static juce::String plural (int value, const juce::String& singular, const juce::String& pluralWord)
        {
            return juce::String (value) + " " + (value == 1 ? singular : pluralWord);
        }

        static bool isDrumProject (const PatchCraftProject& project)
        {
            if (project.getEngineType().equalsIgnoreCase ("drum"))
                return true;

            for (const auto& element : project.getLayout().getAll())
                if (element.type == ElementType::PadGrid || element.type == ElementType::DrumGrid)
                    return true;

            for (const auto& block : project.getDspGraph().blocks)
                if (block.type.containsIgnoreCase ("drum"))
                    return true;

            return false;
        }

        static int countRuntimeControls (const PatchCraftProject& project, bool bound)
        {
            int count = 0;
            for (const auto& element : project.getLayout().getAll())
            {
                if (! isRuntimeControlElement (element.type))
                    continue;
                if (element.type == ElementType::Dropdown)
                    continue;
                if (element.parameterId.isNotEmpty() == bound)
                    ++count;
            }
            return count;
        }

        static int countElementType (const PatchCraftProject& project, ElementType type)
        {
            int count = 0;
            for (const auto& element : project.getLayout().getAll())
                if (element.type == type)
                    ++count;
            return count;
        }

        static int countBlocksInSection (const PatchCraftProject& project, const juce::String& section)
        {
            int count = 0;
            for (const auto& block : project.getDspGraph().blocks)
                if (block.section.equalsIgnoreCase (section))
                    ++count;
            return count;
        }

        static int countAnimatedOrReactiveElements (const PatchCraftProject& project)
        {
            int count = 0;
            for (const auto& element : project.getLayout().getAll())
                if (element.audioReactive
                    || (element.animationMode.isNotEmpty() && element.animationMode != "none"))
                    ++count;
            return count;
        }

        static int premiumInteractionScore (const PatchCraftProject& project)
        {
            int score = 0;
            score += juce::jmin (2, countElementType (project, ElementType::MacroControl));
            score += juce::jmin (2, countElementType (project, ElementType::ModMatrix));
            score += juce::jmin (2, countElementType (project, ElementType::GranularField));
            score += juce::jmin (2, countElementType (project, ElementType::Mixer));
            score += juce::jmin (2, countElementType (project, ElementType::DrumGrid));
            score += juce::jmin (2, countAnimatedOrReactiveElements (project));
            score += project.getDspGraph().macros.empty() ? 0 : 2;
            score += project.getDspGraph().modulation.empty() ? 0 : 2;
            score += project.getDspGraph().automation.empty() ? 0 : 2;
            return score;
        }

        static juce::StringArray tagsFromManifest (const Manifest& manifest)
        {
            auto tags = manifest.tags;
            tags.addIfNotAlreadyThere ("PatchCraft");
            tags.addIfNotAlreadyThere (manifest.engine);
            tags.addIfNotAlreadyThere (manifest.category);
            tags.removeEmptyStrings();
            tags.removeDuplicates (true);
            return tags;
        }

        static juce::var stringArrayToVar (const juce::StringArray& values)
        {
            juce::Array<juce::var> out;
            for (const auto& value : values)
                if (value.trim().isNotEmpty())
                    out.add (value.trim());
            return juce::var (out);
        }

        static juce::String severityName (LaunchCenterPage::Severity severity)
        {
            switch (severity)
            {
                case LaunchCenterPage::Severity::Pass:    return "PASS";
                case LaunchCenterPage::Severity::Warning: return "WARN";
                case LaunchCenterPage::Severity::Error:   return "FIX";
                case LaunchCenterPage::Severity::Info:    return "INFO";
            }
            return "INFO";
        }

        static juce::Colour severityColour (LaunchCenterPage::Severity severity)
        {
            switch (severity)
            {
                case LaunchCenterPage::Severity::Pass:    return juce::Colour (0xff64d88a);
                case LaunchCenterPage::Severity::Warning: return PatchCraftLookAndFeel::accent();
                case LaunchCenterPage::Severity::Error:   return juce::Colour (0xffff5f5f);
                case LaunchCenterPage::Severity::Info:    return juce::Colour (0xff58b7ff);
            }
            return PatchCraftLookAndFeel::textDim();
        }

        static juce::File resolveAssetPath (const PatchCraftProject& project, juce::String path)
        {
            path = path.trim();
            if (path.isEmpty())
                return {};

            juce::File file (path);
            if (juce::File::isAbsolutePath (path))
                return file;

            if (project.getProjectFolder().isDirectory())
                return project.getProjectFolder().getChildFile (path);

            return {};
        }

        static bool assetExists (const PatchCraftProject& project, const juce::String& path)
        {
            const auto file = resolveAssetPath (project, path);
            return file != juce::File() && file.existsAsFile();
        }

        static juce::File runtimeFolder()
        {
            const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            return app.isDirectory() ? app : app.getParentDirectory();
        }

        static int countRuntimeFactoryDemos()
        {
            const auto demos = runtimeFolder().getChildFile ("FactoryDemos");
            if (! demos.isDirectory())
                return 0;

            int count = 0;
            for (auto& entry : juce::RangedDirectoryIterator (demos, false, "*.patchcraft", juce::File::findDirectories))
                if (entry.getFile().getChildFile ("manifest.json").existsAsFile())
                    ++count;
            return count;
        }

        static juce::StringArray missingRuntimeDistributionItems()
        {
            const auto appDir = runtimeFolder();
            juce::StringArray missing;

            if (! appDir.getChildFile ("PatchCraftStudio.exe").existsAsFile()
                && ! appDir.getChildFile ("PatchCraft Studio.exe").existsAsFile()
                && ! appDir.getChildFile ("PatchCraftStudio").existsAsFile())
                missing.add ("PatchCraft Studio executable");
            if (countRuntimeFactoryDemos() < kApprovedFactoryDemoCount)
                missing.add ("FactoryDemos with the approved factory demo set");
            if (! appDir.getChildFile ("Library").isDirectory())
                missing.add ("Library folder");
            if (! appDir.getChildFile ("Library").getChildFile ("Assets").isDirectory())
                missing.add ("Library/Assets");
            if (! appDir.getChildFile ("PlayerPlugins").getChildFile ("PatchCraft Player.vst3").exists())
                missing.add ("PlayerPlugins/PatchCraft Player.vst3");
            if (! appDir.getChildFile ("PlayerPlugins").getChildFile ("PatchCraft Player FX.vst3").exists())
                missing.add ("PlayerPlugins/PatchCraft Player FX.vst3");

            return missing;
        }

        static juce::String markdownEscape (juce::String text)
        {
            return text.replace ("\r", "").trim();
        }

        static juce::String htmlEscape (juce::String text)
        {
            return text.replace ("&", "&amp;")
                       .replace ("<", "&lt;")
                       .replace (">", "&gt;")
                       .replace ("\"", "&quot;")
                       .replace ("'", "&#39;");
        }

        static juce::StringArray splitLinesClean (juce::String text)
        {
            juce::StringArray lines;
            lines.addLines (text.replace ("\r", "\n"));
            lines.trim();
            lines.removeEmptyStrings();
            return lines;
        }

        static juce::String joinLinesClean (const juce::StringArray& lines)
        {
            juce::StringArray copy = lines;
            copy.trim();
            copy.removeEmptyStrings();
            return copy.joinIntoString ("\n");
        }

        static juce::String priceText (const Manifest& manifest)
        {
            if (manifest.salesPrice <= 0.0)
                return "Free / invite-only";

            return manifest.salesCurrency + " " + juce::String (manifest.salesPrice, 2);
        }

        static juce::String salesHeadline (const Manifest& manifest)
        {
            return manifest.salesHeadline.trim().isNotEmpty()
                ? manifest.salesHeadline.trim()
                : "A custom " + manifest.instrumentName + " instrument built for serious music creators.";
        }

        static juce::String salesSubheadline (const Manifest& manifest)
        {
            return manifest.salesSubheadline.trim().isNotEmpty()
                ? manifest.salesSubheadline.trim()
                : "A premium PatchCraft Player instrument with branded visuals, playable presets, performance controls, and seller-ready delivery.";
        }

        static juce::String whiteLabelProductName (const Manifest& manifest)
        {
            if (manifest.whiteLabelPackageName.trim().isNotEmpty())
                return manifest.whiteLabelPackageName.trim();
            if (manifest.playerDisplayName.trim().isNotEmpty())
                return manifest.playerDisplayName.trim();
            return manifest.instrumentName.trim().isNotEmpty() ? manifest.instrumentName.trim() : juce::String ("PatchCraft Player Product");
        }

        static juce::String whiteLabelPublisher (const Manifest& manifest)
        {
            if (manifest.whiteLabelPublisher.trim().isNotEmpty())
                return manifest.whiteLabelPublisher.trim();
            if (manifest.playerClientName.trim().isNotEmpty())
                return manifest.playerClientName.trim();
            return manifest.creator.trim().isNotEmpty() ? manifest.creator.trim() : juce::String ("PatchCraft Producer");
        }

        static juce::String reverseDnsPart (juce::String text)
        {
            text = text.trim().toLowerCase();
            juce::String out;
            bool lastWasDash = false;
            for (auto c : text)
            {
                if (juce::CharacterFunctions::isLetterOrDigit (c))
                {
                    out << c;
                    lastWasDash = false;
                }
                else if (! lastWasDash)
                {
                    out << "-";
                    lastWasDash = true;
                }
            }
            out = out.trimCharactersAtStart ("-").trimCharactersAtEnd ("-");
            return out.isNotEmpty() ? out : juce::String ("patchcraft");
        }

        static juce::String whiteLabelBundleId (const Manifest& manifest)
        {
            if (manifest.whiteLabelBundleIdentifier.trim().isNotEmpty())
                return manifest.whiteLabelBundleIdentifier.trim();

            return "com." + reverseDnsPart (whiteLabelPublisher (manifest)).replace ("-", "")
                + "." + reverseDnsPart (whiteLabelProductName (manifest)).replace ("-", "");
        }

        static juce::String whiteLabelProductCode (const Manifest& manifest)
        {
            if (manifest.whiteLabelProductCode.trim().isNotEmpty())
                return manifest.whiteLabelProductCode.trim();
            return safeSlug (whiteLabelProductName (manifest)).toUpperCase();
        }

        static juce::String whiteLabelInstallerId (const Manifest& manifest)
        {
            const auto configured = manifest.whiteLabelInstallerId.trim();
            const auto seed = configured.isNotEmpty()
                ? configured
                : whiteLabelBundleId (manifest) + "|" + whiteLabelProductCode (manifest);

            const auto hash = juce::SHA256 (seed.toUTF8()).toHexString().toUpperCase();
            return "{" + hash.substring (0, 8) + "-" + hash.substring (8, 12)
                 + "-" + hash.substring (12, 16) + "-" + hash.substring (16, 20)
                 + "-" + hash.substring (20, 32) + "}";
        }

        static juce::String innoString (juce::String text)
        {
            return text.replace ("\"", "\"\"");
        }

        static juce::String sha256ForFile (const juce::File& file)
        {
            if (! file.existsAsFile())
                return {};

            return juce::SHA256 (file).toHexString().toLowerCase();
        }

        static juce::String relativePackagePath (const juce::File& root, const juce::File& file)
        {
            auto path = file.getRelativePathFrom (root)
                .replaceCharacter ('\\', '/')
                .trimCharactersAtStart ("/");
            return path.isNotEmpty() ? path : file.getFileName();
        }
    }

    class LaunchCenterPage::CheckRow final : public juce::Component
    {
    public:
        explicit CheckRow (CheckItem itemIn) : item (std::move (itemIn))
        {
            status.setText (severityName (item.severity), juce::dontSendNotification);
            status.setJustificationType (juce::Justification::centred);
            status.setColour (juce::Label::textColourId, severityColour (item.severity));
            status.setFont (juce::Font (11.0f, juce::Font::bold));
            addAndMakeVisible (status);

            title.setText (item.title, juce::dontSendNotification);
            title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
            title.setFont (juce::Font (14.0f, juce::Font::bold));
            addAndMakeVisible (title);

            detail.setText (item.detail, juce::dontSendNotification);
            detail.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            detail.setFont (juce::Font (12.0f));
            detail.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (detail);

            actionButton.setButtonText (item.actionLabel);
            actionButton.getProperties().set ("smallButton", true);
            actionButton.getProperties().set ("fontSize", 11.0);
            actionButton.setVisible (item.actionLabel.isNotEmpty() && item.action != nullptr);
            actionButton.onClick = [this]
            {
                if (item.action)
                    item.action();
            };
            addAndMakeVisible (actionButton);
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced (1.0f);
            const auto colour = severityColour (item.severity);
            juce::ColourGradient grad (PatchCraftLookAndFeel::panelAlt().brighter (0.02f),
                                       area.getX(), area.getY(),
                                       PatchCraftLookAndFeel::panel().darker (0.18f),
                                       area.getRight(), area.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (area, 10.0f);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.8f));
            g.drawRoundedRectangle (area, 10.0f, 1.0f);
            g.setColour (colour.withAlpha (0.9f));
            g.fillRoundedRectangle (area.withWidth (4.0f), 2.0f);
            g.setColour (colour.withAlpha (0.08f));
            g.fillRoundedRectangle (area.reduced (8.0f), 8.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12, 10);
            status.setBounds (area.removeFromLeft (54));
            area.removeFromLeft (10);

            auto right = area.removeFromRight (item.actionLabel.isNotEmpty() ? 136 : 0);
            if (! right.isEmpty())
                actionButton.setBounds (right.removeFromTop (34).reduced (4, 0));

            title.setBounds (area.removeFromTop (24));
            detail.setBounds (area);
        }

    private:
        CheckItem item;
        juce::Label status;
        juce::Label title;
        juce::Label detail;
        juce::TextButton actionButton;
    };

    class LaunchCenterPage::DemoTile final : public juce::Component
    {
    public:
        DemoTile (juce::File folderIn,
                  juce::String nameIn,
                  juce::String categoryIn,
                  juce::String engineIn,
                  juce::String descriptionIn,
                  juce::File imageFileIn,
                  std::function<void (juce::File)> loadActionIn)
            : folder (std::move (folderIn)),
              name (std::move (nameIn)),
              category (std::move (categoryIn)),
              engine (std::move (engineIn)),
              description (std::move (descriptionIn)),
              loadAction (std::move (loadActionIn))
        {
            if (imageFileIn.existsAsFile())
                image = juce::ImageCache::getFromFile (imageFileIn);

            loadButton.setButtonText ("Open");
            loadButton.getProperties().set ("smallButton", true);
            loadButton.getProperties().set ("primaryAction", true);
            loadButton.onClick = [this]
            {
                if (loadAction)
                    loadAction (folder);
            };
            addAndMakeVisible (loadButton);

            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            if (! loadButton.getBounds().contains (event.getPosition()) && loadAction)
                loadAction (folder);
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.96f));
            g.fillRoundedRectangle (area, 9.0f);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.9f));
            g.drawRoundedRectangle (area, 9.0f, 1.0f);

            auto imageArea = getLocalBounds().reduced (10).removeFromTop (78).toFloat();
            if (image.isValid())
            {
                juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (imageArea.toNearestInt());
                g.drawImageWithin (image,
                                   (int) imageArea.getX(), (int) imageArea.getY(),
                                   (int) imageArea.getWidth(), (int) imageArea.getHeight(),
                                   juce::RectanglePlacement::fillDestination);
            }
            else
            {
                juce::ColourGradient grad (PatchCraftLookAndFeel::accent().withAlpha (0.22f),
                                           imageArea.getX(), imageArea.getY(),
                                           juce::Colour (0xff101923),
                                           imageArea.getRight(), imageArea.getBottom(),
                                           false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (imageArea, 6.0f);
            }

            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (imageArea, 6.0f);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.85f));
            g.drawRoundedRectangle (imageArea, 6.0f, 1.0f);

            auto text = getLocalBounds().reduced (12);
            text.removeFromTop (86);
            g.setColour (PatchCraftLookAndFeel::textBright());
            g.setFont (juce::Font (13.0f, juce::Font::bold));
            g.drawFittedText (name, text.removeFromTop (20), juce::Justification::centredLeft, 1);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            g.drawFittedText ((engine.isNotEmpty() ? engine.toUpperCase() : "DEMO") + "  " + category,
                              text.removeFromTop (18), juce::Justification::centredLeft, 1);
            g.setColour (PatchCraftLookAndFeel::textDim());
            g.setFont (juce::Font (11.0f));
            g.drawFittedText (description, text.removeFromTop (44), juce::Justification::topLeft, 2);
        }

        void resized() override
        {
            loadButton.setBounds (getLocalBounds().reduced (12).removeFromBottom (28));
        }

    private:
        juce::File folder;
        juce::String name, category, engine, description;
        juce::Image image;
        std::function<void (juce::File)> loadAction;
        juce::TextButton loadButton;
    };

    LaunchCenterPage::LaunchCenterPage (StudioMainComponent& ownerIn) : owner (ownerIn)
    {
        title.setText ("Export Center", juce::dontSendNotification);
        title.setFont (juce::Font (28.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (title);

        subtitle.setText ("Validate your instrument, ship packs and plugins, and prepare launch assets.",
                          juce::dontSendNotification);
        subtitle.setFont (juce::Font (13.0f));
        subtitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (subtitle);

        exportShipLabel.setText ("Ship", juce::dontSendNotification);
        exportShipLabel.setFont (juce::Font (11.0f, juce::Font::bold));
        exportShipLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (exportShipLabel);

        exportToolsLabel.setText ("Launch Tools", juce::dontSendNotification);
        exportToolsLabel.setFont (juce::Font (11.0f, juce::Font::bold));
        exportToolsLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (exportToolsLabel);

        statusBadge.setJustificationType (juce::Justification::centred);
        statusBadge.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (statusBadge);

        summaryLabel.setFont (juce::Font (12.2f));
        summaryLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        summaryLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (summaryLabel);

        outputFolderLabel.setFont (juce::Font (11.5f));
        outputFolderLabel.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        outputFolderLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (outputFolderLabel);

        creatorTitle.setText ("Start With A Simple Prompt", juce::dontSendNotification);
        creatorTitle.setFont (juce::Font (16.0f, juce::Font::bold));
        creatorTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (creatorTitle);

        creatorBody.setText ("Describe the plugin. PatchCraft builds a real starting point with DSP, controls, presets, and a Player layout.",
                             juce::dontSendNotification);
        creatorBody.setFont (juce::Font (12.0f));
        creatorBody.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (creatorBody);

        recipePrompt.setMultiLine (true);
        recipePrompt.setReturnKeyStartsNewLine (true);
        recipePrompt.setTextToShowWhenEmpty ("Example: warm melodic synth with filter, delay, reverb, macros, and a clean neon interface",
                                             PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
        addAndMakeVisible (recipePrompt);

        recipeTypeBox.addItem ("Auto", 1);
        recipeTypeBox.addItem ("Synth", 2);
        recipeTypeBox.addItem ("Sampler", 3);
        recipeTypeBox.addItem ("Drums", 4);
        recipeTypeBox.addItem ("FX", 5);
        recipeTypeBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (recipeTypeBox);

        demoTitle.setText ("Factory Demos", juce::dontSendNotification);
        demoTitle.setFont (juce::Font (16.0f, juce::Font::bold));
        demoTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (demoTitle);

        demoBody.setText ("Open a finished template, hear it, edit it, or export it.",
                          juce::dontSendNotification);
        demoBody.setFont (juce::Font (12.0f));
        demoBody.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (demoBody);

        doctorTitle.setText ("Launch Doctor", juce::dontSendNotification);
        doctorTitle.setFont (juce::Font (16.0f, juce::Font::bold));
        doctorTitle.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
        addAndMakeVisible (doctorTitle);

        doctorBody.setText ("Run checks before you ship. Fix blockers first, then review warnings.",
                            juce::dontSendNotification);
        doctorBody.setFont (juce::Font (12.0f));
        doctorBody.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        addAndMakeVisible (doctorBody);

        for (auto* tab : { &tabOverview, &tabCreate, &tabDemos, &tabDoctor })
        {
            tab->setClickingTogglesState (true);
            tab->setRadioGroupId (88421);
            styleActionButton (*tab, false);
            addAndMakeVisible (*tab);
        }
        tabOverview.onClick = [this] { setActiveTab (ContentTab::Overview); };
        tabCreate.onClick = [this] { setActiveTab (ContentTab::Create); };
        tabDemos.onClick = [this] { setActiveTab (ContentTab::Demos); };
        tabDoctor.onClick = [this] { setActiveTab (ContentTab::Doctor); };

        styleActionButton (refreshButton, true);
        styleActionButton (outputFolderButton, false);
        styleActionButton (bundleButton, true);
        styleActionButton (customerWizardButton, false);
        styleActionButton (productPageButton, false);
        styleActionButton (testButton, false);
        styleActionButton (exportPackButton, false);
        styleActionButton (exportVstButton, false);
        styleActionButton (publishButton, true);
        styleActionButton (createFromPromptButton, true);
        styleActionButton (blankProjectButton, false);
        styleActionButton (synthStarterButton, false);
        styleActionButton (sampleStarterButton, false);
        styleActionButton (drumStarterButton, false);
        styleActionButton (fxStarterButton, false);

        refreshButton.onClick = [this] { refresh(); };
        outputFolderButton.onClick = [this] { chooseOutputFolder(); };
        bundleButton.onClick = [this] { createLaunchBundle(); };
        customerWizardButton.onClick = [this] { showCustomerPackageWizard(); };
        productPageButton.setButtonText ("Sales Page");
        productPageButton.onClick = [this] { showSalesPageBuilder(); };
        testButton.onClick = [this]
        {
            owner.setBottomTab (BottomPanel::Page::Branding);
            if (! owner.isPreviewActive())
                owner.togglePreview();
        };
        exportPackButton.onClick = [this] { owner.exportPack(); };
        exportVstButton.onClick = [this] { owner.exportVstPlugin(); };
        publishButton.onClick = [this] { owner.publishToPluginClub(); };
        createFromPromptButton.onClick = [this] { createSimplePluginFromPrompt(); };
        blankProjectButton.onClick = [this]
        {
            owner.newProject();
            owner.setBottomTab (BottomPanel::Page::Design);
            refresh();
        };
        synthStarterButton.onClick = [this] { createSimplePluginFromPrompt ("synth"); };
        sampleStarterButton.onClick = [this] { createSimplePluginFromPrompt ("sample"); };
        drumStarterButton.onClick = [this] { createSimplePluginFromPrompt ("drum"); };
        fxStarterButton.onClick = [this] { createSimplePluginFromPrompt ("fx"); };

        outputFolderButton.setTooltip ("Choose where Launch bundles and Product Page exports are written.");
        bundleButton.setTooltip ("Create the complete launch folder: pack payload, installer files, sales copy, QA docs, and Plugin.club metadata.");
        customerWizardButton.setTooltip ("Open the white-label customer package wizard for installer, license, first-run, support, and DAW QA handoff.");
        productPageButton.setTooltip ("Open the sales page builder: offer copy, price, CTA, checkout link, demos, HTML export, and Plugin.club metadata.");

        for (auto* button : { &refreshButton, &outputFolderButton, &bundleButton, &customerWizardButton, &productPageButton, &testButton,
                              &exportPackButton, &exportVstButton, &publishButton, &createFromPromptButton, &blankProjectButton,
                              &synthStarterButton, &sampleStarterButton, &drumStarterButton, &fxStarterButton })
            addAndMakeVisible (*button);

        demoViewport.setViewedComponent (&demoContent, false);
        demoViewport.setScrollBarsShown (false, true);
        demoViewport.setScrollBarThickness (10);
        addAndMakeVisible (demoViewport);

        checksViewport.setViewedComponent (&checksContent, false);
        checksViewport.setScrollBarsShown (true, false);
        checksViewport.setScrollBarThickness (10);
        addAndMakeVisible (checksViewport);

        rebuildDemoTiles();
        refresh();
        updateOutputFolderLabel();
        setActiveTab (ContentTab::Overview);
    }

    void LaunchCenterPage::setActiveTab (ContentTab tab)
    {
        activeTab = tab;
        updateTabBar();
        applyTabVisibility();
        resized();
        repaint();
    }

    void LaunchCenterPage::updateTabBar()
    {
        tabOverview.setToggleState (activeTab == ContentTab::Overview, juce::dontSendNotification);
        tabCreate.setToggleState (activeTab == ContentTab::Create, juce::dontSendNotification);
        tabDemos.setToggleState (activeTab == ContentTab::Demos, juce::dontSendNotification);
        tabDoctor.setToggleState (activeTab == ContentTab::Doctor, juce::dontSendNotification);

        const bool primary = true;
        for (auto* tab : { &tabOverview, &tabCreate, &tabDemos, &tabDoctor })
        {
            const bool selected = tab->getToggleState();
            tab->getProperties().set ("primaryAction", selected);
            tab->getProperties().set ("bold", selected);
        }
        juce::ignoreUnused (primary);
    }

    void LaunchCenterPage::applyTabVisibility()
    {
        const bool overview = activeTab == ContentTab::Overview;
        const bool create = activeTab == ContentTab::Create;
        const bool demos = activeTab == ContentTab::Demos;
        const bool doctor = activeTab == ContentTab::Doctor;

        statusBadge.setVisible (overview);
        summaryLabel.setVisible (overview);
        exportShipLabel.setVisible (overview);
        exportToolsLabel.setVisible (overview);
        outputFolderLabel.setVisible (overview || create);

        for (auto* button : { &exportPackButton, &exportVstButton, &publishButton, &bundleButton,
                              &refreshButton, &outputFolderButton, &testButton, &customerWizardButton, &productPageButton })
            button->setVisible (overview);

        creatorTitle.setVisible (create);
        creatorBody.setVisible (create);
        recipePrompt.setVisible (create);
        recipeTypeBox.setVisible (create);
        createFromPromptButton.setVisible (create);
        blankProjectButton.setVisible (create);
        synthStarterButton.setVisible (create);
        sampleStarterButton.setVisible (create);
        drumStarterButton.setVisible (create);
        fxStarterButton.setVisible (create);

        demoTitle.setVisible (demos);
        demoBody.setVisible (demos);
        demoViewport.setVisible (demos);

        doctorTitle.setVisible (doctor);
        doctorBody.setVisible (doctor);
        checksViewport.setVisible (doctor);
    }

    LaunchCenterPage::~LaunchCenterPage() = default;

    void LaunchCenterPage::styleActionButton (juce::TextButton& button, bool primary)
    {
        button.getProperties().set ("fontSize", primary ? 12.5 : 12.0);
        button.getProperties().set ("bold", primary);
        button.getProperties().set ("corner", 8.0);
        if (primary)
            button.getProperties().set ("primaryAction", true);
        else
            button.getProperties().set ("smallButton", true);
    }

    juce::String LaunchCenterPage::inferEngineFromPrompt (const juce::String& prompt,
                                                          const juce::String& forcedType) const
    {
        auto forced = forcedType.trim().toLowerCase();
        if (forced == "synth" || forced == "sample" || forced == "drum" || forced == "fx")
            return forced;

        auto selected = recipeTypeBox.getText().trim().toLowerCase();
        if (selected == "synth") return "synth";
        if (selected == "sampler") return "sample";
        if (selected == "drums") return "drum";
        if (selected == "fx") return "fx";

        auto text = prompt.toLowerCase();
        if (text.contains ("drum") || text.contains ("beat") || text.contains ("808") || text.contains ("kick") || text.contains ("snare"))
            return "drum";
        if (text.contains ("effect") || text.contains ("fx") || text.contains ("delay") || text.contains ("reverb") || text.contains ("distortion") || text.contains ("eq"))
            return "fx";
        if (text.contains ("sample") || text.contains ("sampler") || text.contains ("vocal") || text.contains ("granular") || text.contains ("one shot") || text.contains ("keys"))
            return "sample";

        return "synth";
    }

    juce::String LaunchCenterPage::productNameFromPrompt (const juce::String& prompt,
                                                          const juce::String& engineId) const
    {
        auto text = prompt.trim();
        if (text.isNotEmpty())
        {
            text = text.upToFirstOccurrenceOf (".", false, false)
                       .upToFirstOccurrenceOf (",", false, false)
                       .upToFirstOccurrenceOf (" with ", false, true)
                       .trim();
            if (text.length() > 4)
            {
                juce::StringArray words;
                words.addTokens (text, " \t\r\n-_", "\"'");
                words.trim();
                words.removeEmptyStrings();
                while (words.size() > 4)
                    words.remove (words.size() - 1);
                for (auto& word : words)
                    word = word.substring (0, 1).toUpperCase() + word.substring (1).toLowerCase();
                return words.joinIntoString (" ");
            }
        }

        if (engineId == "drum") return "New Drum Machine";
        if (engineId == "sample") return "New Sample Instrument";
        if (engineId == "fx") return "New Effect Plugin";
        return "New Synth Instrument";
    }

    void LaunchCenterPage::createStarterPlugin (const juce::String& engineId,
                                                const juce::String& productName,
                                                const juce::String& description)
    {
        auto& project = owner.getProject();
        project.setEngineType (engineId);

        auto& manifest = project.getManifest();
        manifest.instrumentName = productName;
        manifest.playerDisplayName = productName;
        manifest.description = description.isNotEmpty()
            ? description
            : ("A playable " + engineDisplayName (engineId).toLowerCase() + " created in PatchCraft.");
        manifest.category = engineDisplayName (engineId);
        manifest.creator = manifest.creator.trim().isNotEmpty() ? manifest.creator : juce::String ("PatchCraft User");
        manifest.tags.clear();
        manifest.tags.add ("starter");
        manifest.tags.add (engineId);
        manifest.tags.add ("export-ready");

        project.performLayoutEdit ("Create simple plugin starter", [&] (LayoutModel& layout)
        {
            const auto& canvas = project.getCanvasSize();
            auto addElementIfMissing = [&layout] (LayoutElement element)
            {
                if (layout.find (element.id) == nullptr)
                    layout.add (element);
            };

            LayoutElement titleLabel;
            titleLabel.id = "starter_title";
            titleLabel.type = ElementType::Label;
            titleLabel.label = productName;
            titleLabel.x = 74;
            titleLabel.y = 62;
            titleLabel.width = 420;
            titleLabel.height = 40;
            titleLabel.labelSize = 24.0f;
            titleLabel.textColour = PatchCraftLookAndFeel::textBright();
            titleLabel.semanticRole = "productTitle";
            addElementIfMissing (titleLabel);

            LayoutElement visual;
            visual.id = "starter_visual_reactor";
            visual.type = ElementType::VisualFxLayer;
            visual.label = "Reactive Visual";
            visual.x = juce::jmax (60, canvas.width / 2 - 220);
            visual.y = 112;
            visual.width = 440;
            visual.height = 220;
            visual.audioReactive = true;
            visual.audioReactiveMode = "level";
            visual.audioReactiveAmount = 0.55f;
            visual.animationMode = "bpmPulse";
            visual.animationRate = 1.0f;
            visual.visualSource = "audioLevel";
            visual.visualAction = "pulseGlow";
            visual.visualPreset = "orbitAura";
            visual.visualLowPowerFallback = true;
            visual.opacity = 0.82f;
            visual.accentColour = PatchCraftLookAndFeel::accent();
            addElementIfMissing (visual);

            if (engineId == "sample" || engineId == "drum")
            {
                LayoutElement drop;
                drop.id = "starter_sample_drop";
                drop.type = ElementType::SampleDropZone;
                drop.label = engineId == "drum" ? "Drop Drum Samples" : "Drop Samples";
                drop.parameterId = "sampleStart";
                drop.x = 74;
                drop.y = 344;
                drop.width = 260;
                drop.height = 104;
                drop.cornerRadius = 10.0f;
                drop.borderColour = PatchCraftLookAndFeel::accent();
                drop.backgroundColour = juce::Colour (0xff101923).withAlpha (0.72f);
                drop.semanticRole = "sampleDrop";
                addElementIfMissing (drop);
            }
        });

        for (auto& preset : project.getPresets())
        {
            preset.tags.addIfNotAlreadyThere ("starter");
            preset.tags.addIfNotAlreadyThere (engineId);
            if (preset.description.trim().isEmpty())
                preset.description = "Playable starter preset for " + productName + ".";
        }

        project.notifyChanged();
        owner.setBottomTab (BottomPanel::Page::Design);
        refresh();
        showMessage ("Plugin Created",
                     productName + " is ready on the Design page. Controls are already bound to real parameters; use Brand Lab to test the Player.",
                     juce::MessageBoxIconType::InfoIcon);
    }

    void LaunchCenterPage::createSimplePluginFromPrompt (juce::String forcedType)
    {
        const auto prompt = recipePrompt.getText().trim();
        const auto engineId = inferEngineFromPrompt (prompt, forcedType);
        const auto name = productNameFromPrompt (prompt, engineId);
        const auto description = prompt.isNotEmpty()
            ? prompt
            : ("A playable " + engineDisplayName (engineId).toLowerCase() + " starter with mapped controls and a Player-ready layout.");
        createStarterPlugin (engineId, name, description);
    }

    void LaunchCenterPage::rebuildDemoTiles()
    {
        demoTiles.clear();
        demoContent.removeAllChildren();

        juce::Array<juce::File> roots;
        roots.add (juce::File::getCurrentWorkingDirectory().getChildFile ("FactoryDemos"));
        roots.add (runtimeFolder().getChildFile ("FactoryDemos"));

        juce::StringArray seen;
        for (const auto& root : roots)
        {
            if (! root.isDirectory())
                continue;

            for (auto& entry : juce::RangedDirectoryIterator (root, false, "*.patchcraft", juce::File::findDirectories))
            {
                const auto folder = entry.getFile();
                const auto path = folder.getFullPathName();
                if (seen.contains (path))
                    continue;
                seen.add (path);

                const auto manifestFile = folder.getChildFile ("manifest.json");
                if (! manifestFile.existsAsFile())
                    continue;

                auto parsed = juce::JSON::parse (manifestFile);
                auto* object = parsed.getDynamicObject();
                if (object == nullptr)
                    continue;

                const auto name = object->getProperty ("instrumentName").toString();
                const auto category = object->getProperty ("category").toString();
                const auto engine = object->getProperty ("engine").toString();
                auto description = object->getProperty ("description").toString();
                if (description.length() > 92)
                    description = description.substring (0, 89).trim() + "...";

                auto imagePath = object->getProperty ("libraryThumbnail").toString();
                if (imagePath.isEmpty())
                    imagePath = "assets/thumbnail.png";
                auto imageFile = juce::File::isAbsolutePath (imagePath)
                    ? juce::File (imagePath)
                    : folder.getChildFile (imagePath);
                if (! imageFile.existsAsFile())
                    imageFile = folder.getChildFile ("assets").getChildFile ("thumbnail.png");
                if (! imageFile.existsAsFile())
                    imageFile = folder.getChildFile ("assets").getChildFile ("library-artwork.png");

                auto tile = std::make_unique<DemoTile> (folder, name, category, engine, description, imageFile,
                    [this] (juce::File demoFolder)
                    {
                        owner.loadFactoryDemo (demoFolder);
                        refresh();
                    });
                demoContent.addAndMakeVisible (*tile);
                demoTiles.push_back (std::move (tile));

                if (demoTiles.size() >= 18)
                    break;
            }

            if (demoTiles.size() >= 18)
                break;
        }

        resized();
    }

    std::vector<LaunchCenterPage::CheckItem> LaunchCenterPage::buildChecks()
    {
        std::vector<CheckItem> checks;
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto engine = project.getEngineType();

        auto add = [&checks] (Severity severity,
                              juce::String titleText,
                              juce::String detailText,
                              juce::String actionLabel = {},
                              std::function<void()> action = {})
        {
            checks.push_back ({ severity, std::move (titleText), std::move (detailText),
                                std::move (actionLabel), std::move (action) });
        };

        const bool nameIsDefault = manifest.instrumentName.trim().isEmpty()
            || manifest.instrumentName.equalsIgnoreCase ("Untitled Instrument");
        const bool creatorIsDefault = manifest.creator.trim().isEmpty()
            || manifest.creator.equalsIgnoreCase ("PatchCraft User");
        const bool hasDescription = manifest.description.trim().length() >= 40;
        if (nameIsDefault || creatorIsDefault || ! hasDescription)
        {
            juce::StringArray missing;
            if (nameIsDefault) missing.add ("real product name");
            if (creatorIsDefault) missing.add ("creator/brand");
            if (! hasDescription) missing.add ("marketplace description");
            add (Severity::Warning,
                 "Product metadata needs launch polish",
                 "Missing " + missing.joinIntoString (", ") + ". This makes Plugin.club drafts and exported Player info feel unfinished.",
                 "Brand Lab",
                 [this] { owner.setBottomTab (BottomPanel::Page::Branding); });
        }
        else
        {
            add (Severity::Pass,
                 "Product metadata is ready",
                 manifest.instrumentName + " by " + manifest.creator + " has launch-ready name, creator, category, version, and description.");
        }

        const bool sampleBased = engine.equalsIgnoreCase ("sample") || engine.equalsIgnoreCase ("drum") || isDrumProject (project);
        if (sampleBased)
        {
            const auto health = SampleMap::evaluateHealth (project.getSampleMap(), project.getProjectFolder(), engine);
            if (health.blocksExport())
            {
                add (Severity::Error,
                     "Sample map blocks export",
                     health.exportMessage(),
                     "Fix Samples",
                     [this] { owner.setBottomTab (BottomPanel::Page::Samples); });
            }
            else if (health.totalZones == 0)
            {
                add (Severity::Error,
                     "No playable sample zones",
                     "Sample and drum instruments need mapped zones/pads before they can be sold or exported.",
                     "Import Samples",
                     [this] { owner.setBottomTab (BottomPanel::Page::Samples); });
            }
            else
            {
                add (health.issues.isEmpty() ? Severity::Pass : Severity::Warning,
                     "Sample map is playable",
                     plural (health.playableZones, "playable zone", "playable zones")
                        + ", keyboard range "
                        + (health.firstCoveredNote >= 0 ? juce::MidiMessage::getMidiNoteName (health.firstCoveredNote, true, true, 3) : juce::String ("none"))
                        + " to "
                        + (health.lastCoveredNote >= 0 ? juce::MidiMessage::getMidiNoteName (health.lastCoveredNote, true, true, 3) : juce::String ("none"))
                        + (health.issues.isEmpty() ? "." : ". Check mapper warnings before final export."),
                     health.issues.isEmpty() ? juce::String() : juce::String ("Review"),
                     health.issues.isEmpty() ? std::function<void()>() : [this] { owner.setBottomTab (BottomPanel::Page::Samples); });
            }
        }

        const int sourceBlocks = countBlocksInSection (project, "source");
        const int filterBlocks = countBlocksInSection (project, "filter");
        const int ampBlocks = countBlocksInSection (project, "amp");
        const int modBlocks = countBlocksInSection (project, "mod");
        const int fxBlocks = countBlocksInSection (project, "fx");
        const int outBlocks = countBlocksInSection (project, "out");
        const auto graphIssues = project.getDspGraph().validateTypedGraph (engine);
        int graphErrors = 0;
        int graphWarnings = 0;
        juce::StringArray graphDetails;
        for (const auto& issue : graphIssues)
        {
            if (issue.severity == "error")
                ++graphErrors;
            else
                ++graphWarnings;
            if (graphDetails.size() < 4)
                graphDetails.add (issue.toString());
        }

        if (graphErrors > 0)
        {
            add (Severity::Error,
                 "DSP graph has blocking errors",
                 graphDetails.joinIntoString ("  |  "),
                 "Graph",
                 [this] { owner.setBottomTab (BottomPanel::Page::DSP); });
        }
        else if (project.getDspGraph().blocks.empty())
        {
            add (Severity::Warning,
                 "DSP graph is using defaults",
                 "No author blocks exist. The sound may play, but it will not feel like a crafted sellable patch.",
                 "Graph",
                 [this] { owner.setBottomTab (BottomPanel::Page::DSP); });
        }
        else
        {
            add (graphWarnings > 0 ? Severity::Warning : Severity::Pass,
                 "DSP graph is routable",
                 "Blocks: Source " + juce::String (sourceBlocks)
                    + ", Filter " + juce::String (filterBlocks)
                    + ", Amp " + juce::String (ampBlocks)
                    + ", Mod " + juce::String (modBlocks)
                    + ", FX " + juce::String (fxBlocks)
                    + ", Out " + juce::String (outBlocks)
                    + (graphWarnings > 0 ? ". Review graph warnings before launch." : "."),
                 graphWarnings > 0 ? juce::String ("Graph") : juce::String(),
                 graphWarnings > 0 ? std::function<void()> ([this] { owner.setBottomTab (BottomPanel::Page::DSP); }) : std::function<void()>());
        }

        const int boundControls = countRuntimeControls (project, true);
        const int unboundControls = countRuntimeControls (project, false);
        const int tabPanels = countElementType (project, ElementType::TabPanel);
        const auto paramIssues = project.getParameters().validateReferences (project.getLayout().getAll(),
                                                                             project.getDspGraph(),
                                                                             project.getPresets());
        int paramErrors = 0;
        int paramWarnings = 0;
        juce::StringArray paramDetails;
        for (const auto& issue : paramIssues)
        {
            if (issue.severity == "error")
                ++paramErrors;
            else
                ++paramWarnings;
            if (paramDetails.size() < 4)
                paramDetails.add (issue.toString());
        }

        if (project.getLayout().getAll().empty())
        {
            add (Severity::Error,
                 "Player UI is empty",
                 "The exported Player needs a real customer-facing interface.",
                 "Design UI",
                 [this] { owner.setBottomTab (BottomPanel::Page::Design); });
        }
        else if (paramErrors > 0)
        {
            add (Severity::Error,
                 "UI bindings contain broken references",
                 paramDetails.joinIntoString ("  |  "),
                 "Fix Bindings",
                 [this] { owner.setBottomTab (BottomPanel::Page::Design); });
        }
        else if (unboundControls > 0)
        {
            add (Severity::Warning,
                 "Some controls are not connected",
                 juce::String (boundControls) + " controls are bound, "
                    + juce::String (unboundControls) + " runtime controls are unbound. Unbound controls confuse customers.",
                 "Assign Controls",
                 [this] { owner.setBottomTab (BottomPanel::Page::Design); });
        }
        else
        {
            add (paramWarnings > 0 ? Severity::Warning : Severity::Pass,
                 "Runtime UI bindings are clean",
                 juce::String (boundControls) + " controls are bound to real parameters"
                    + (tabPanels > 0 ? ", with " + juce::String (tabPanels) + " tabbed container(s)." : "."),
                 paramWarnings > 0 ? juce::String ("Review") : juce::String(),
                 paramWarnings > 0 ? std::function<void()> ([this] { owner.setBottomTab (BottomPanel::Page::Design); }) : std::function<void()>());
        }

        const int orbitElements = countElementType (project, ElementType::ArpLane);
        if (orbitElements > 0)
        {
            int multiRingOrbitElements = 0;
            for (const auto& element : project.getLayout().getAll())
                if (element.type == ElementType::ArpLane
                    && (element.arpLaneMode.equalsIgnoreCase ("multiRing")
                        || element.arpLaneMode.equalsIgnoreCase ("orbit")
                        || element.arpLaneMode.equalsIgnoreCase ("orbitMulti")))
                    ++multiRingOrbitElements;

            const bool hasMidiPlayground = std::any_of (project.getDspGraph().blocks.begin(),
                                                        project.getDspGraph().blocks.end(),
                                                        [] (const DspBlock& block)
                                                        {
                                                            return block.type.containsIgnoreCase ("midiPlayground")
                                                                || block.type.containsIgnoreCase ("phrase generator")
                                                                || block.type.containsIgnoreCase ("midi generator");
                                                        });
            const auto hasLayoutParameter = [&project] (const juce::String& parameterId)
            {
                for (const auto& element : project.getLayout().getAll())
                    if (element.parameterId == parameterId)
                        return true;
                return false;
            };
            const bool hasFillControls = hasLayoutParameter ("arpLaneFillMomentary")
                                      || hasLayoutParameter ("arpLaneFillLatch");
            const bool hasRoleControl = hasLayoutParameter ("arpLaneSliderRole");

            if (! hasMidiPlayground)
            {
                add (Severity::Error,
                     "CircleSEQ surface is not connected to a performance engine",
                     "The Design canvas has CircleSEQ/Arp Lane elements, but no MIDI Playground engine is present. Add the CircleSEQ surface or open Performance Builder.",
                     "Perform",
                     [this] { owner.setBottomTab (BottomPanel::Page::MidiPlayground); });
            }
            else if (multiRingOrbitElements == 0 || ! hasFillControls || ! hasRoleControl)
            {
                add (Severity::Warning,
                     "CircleSEQ workflow is missing performance controls",
                     "For a Patterning-style instrument, use a multi-ring CircleSEQ element plus Role, Fill, and lane controls so players can edit sources, timing, automation, and fills without returning to Studio.",
                     "Perform",
                     [this] { owner.setBottomTab (BottomPanel::Page::MidiPlayground); });
            }
            else
            {
                add (Severity::Pass,
                     "CircleSEQ performance surface is instrument-ready",
                     plural (orbitElements, "CircleSEQ/Arp Lane element", "CircleSEQ/Arp Lane elements")
                        + " with multi-ring editing, fill controls, and automation role selection.");
            }
        }

        const int reactiveImages = countElementType (project, ElementType::ReactiveImage);
        const int spriteAnimators = countElementType (project, ElementType::SpriteAnimator);
        const int visualFxLayers = countElementType (project, ElementType::VisualFxLayer);
        const int aiVisualPrompts = countElementType (project, ElementType::AiVisualPrompt);
        const int visualMotionElements = reactiveImages + spriteAnimators + visualFxLayers + aiVisualPrompts;
        if (visualMotionElements > 0)
        {
            bool hasReactiveBinding = false;
            bool hasMissingRequiredAsset = false;
            bool hasProAiOnlyBrief = false;
            for (const auto& element : project.getLayout().getAll())
            {
                if (element.type != ElementType::ReactiveImage
                    && element.type != ElementType::SpriteAnimator
                    && element.type != ElementType::VisualFxLayer
                    && element.type != ElementType::AiVisualPrompt)
                    continue;

                hasReactiveBinding = hasReactiveBinding
                    || element.audioReactive
                    || (element.animationMode.isNotEmpty() && element.animationMode != "none")
                    || element.visualSource.isNotEmpty();

                if ((element.type == ElementType::ReactiveImage || element.type == ElementType::SpriteAnimator)
                    && element.asset.isEmpty()
                    && element.filmstripAsset.isEmpty())
                    hasMissingRequiredAsset = true;

                if (element.type == ElementType::AiVisualPrompt && ! element.visualAiGenerated)
                    hasProAiOnlyBrief = true;
            }

            if (! hasReactiveBinding)
            {
                add (Severity::Warning,
                     "Visual elements are not bound to motion",
                     "Animation Lab elements exist, but none have audio, BPM, MIDI, or parameter reactivity. Set React Mode or Animation in the Inspector.",
                     "Open Animation Lab",
                     [this] { owner.setBottomTab (BottomPanel::Page::Animation); });
            }
            else if (hasMissingRequiredAsset || hasProAiOnlyBrief)
            {
                add (Severity::Warning,
                     "Visual layer needs final artwork assets",
                     "Reactive image or sprite slots can ship with procedural fallback, but final instruments should import artwork or generate Pro assets before export.",
                     "Open Animation Lab",
                     [this] { owner.setBottomTab (BottomPanel::Page::Animation); });
            }
            else
            {
                add (Severity::Pass,
                     "Reactive visual layer is export-ready",
                     plural (visualMotionElements, "Animation Lab element", "Animation Lab elements")
                        + " with runtime-supported visual motion.");
            }
        }

        const int macroControls = countElementType (project, ElementType::MacroControl);
        const int modMatrices = countElementType (project, ElementType::ModMatrix);
        const int granularFields = countElementType (project, ElementType::GranularField);
        const int mixerElements = countElementType (project, ElementType::Mixer);
        const int animatedReactive = countAnimatedOrReactiveElements (project);
        const int wowScore = premiumInteractionScore (project);
        if (wowScore < 6)
        {
            add (Severity::Warning,
                 "Product needs a stronger wow layer",
                 "Current premium score " + juce::String (wowScore) + "/18. Add performance macros, a Mod Matrix, audio-reactive visuals, granular field, mixer, or automation so the Player feels like a premium instrument instead of a static skin.",
                 "Add Wow",
                 [this]
                 {
                     juce::MessageBoxOptions options = juce::MessageBoxOptions()
                         .withIconType (juce::MessageBoxIconType::NoIcon)
                         .withTitle ("Add Premium Wow")
                         .withMessage ("High-value additions that move this instrument beyond a static skin:\n\n"
                                       "1. Granular Field: sample motion, clouds, freeze, multi-direction grain control.\n"
                                       "2. Macro Control: one performance knob driving several real parameters.\n"
                                       "3. Mod Matrix: visible routing between LFOs, MIDI, macros, and sound targets.\n"
                                       "4. Mixer: layer balance, mute/solo, output routing, and buyer-facing mix control.\n"
                                       "5. Audio-Reactive Motion: UI elements that respond to the actual instrument output.")
                         .withButton ("Open Design")
                         .withButton ("Close");
                     juce::AlertWindow::showAsync (options,
                         [this] (int result)
                         {
                             if (result == 1) owner.setBottomTab (BottomPanel::Page::Design);
                         });
                 });
        }
        else
        {
            add (Severity::Pass,
                 "Premium interaction layer is present",
                 "Wow features: " + plural (macroControls, "macro control", "macro controls")
                    + ", " + plural (modMatrices, "mod matrix", "mod matrices")
                    + ", " + plural (granularFields, "granular field", "granular fields")
                    + ", " + plural (mixerElements, "mixer", "mixers")
                    + ", " + plural (animatedReactive, "animated/reactive element", "animated/reactive elements") + ".");
        }

        const bool hasDefaultPreset = manifest.defaultPreset.trim().isNotEmpty()
            || std::any_of (project.getPresets().begin(), project.getPresets().end(),
                            [] (const Preset& preset) { return preset.isDefault; });
        if (project.getPatches().empty() || project.getPresets().empty())
        {
            add (Severity::Error,
                 "No sellable playable presets yet",
                 "A shipped product needs at least one full Patch-backed preset. Current counts: "
                    + plural ((int) project.getPatches().size(), "patch", "patches") + ", "
                    + plural ((int) project.getPresets().size(), "preset", "presets") + ".",
                 "Save Default",
                 [this] { owner.setDefaultPreset(); refresh(); });
        }
        else if (! hasDefaultPreset)
        {
            add (Severity::Warning,
                 "Default preset is not locked in",
                 "The Player should open to a named, reliable default preset.",
                 "Set Default",
                 [this] { owner.setDefaultPreset(); refresh(); });
        }
        else
        {
            add (Severity::Pass,
                 "Preset system is launch-ready",
                 plural ((int) project.getPresets().size(), "preset", "presets") + ", "
                    + plural ((int) project.getPatches().size(), "full patch", "full patches") + ", "
                    + plural ((int) project.getExpansions().size(), "expansion", "expansions") + ".");
        }

        if (manifest.backgroundImage.trim().isEmpty() && manifest.libraryThumbnail.trim().isEmpty()
            && manifest.playerLogoImage.trim().isEmpty() && manifest.playerTitleBannerImage.trim().isEmpty())
        {
            add (Severity::Warning,
                 "Branding artwork is missing",
                 "Add background artwork, thumbnail art, and optional logo so the product page and Player feel premium.",
                 "Brand Lab",
                 [this] { owner.setBottomTab (BottomPanel::Page::Branding); });
        }
        else if ((manifest.backgroundImage.isNotEmpty() && ! assetExists (project, manifest.backgroundImage))
              || (manifest.libraryThumbnail.isNotEmpty() && ! assetExists (project, manifest.libraryThumbnail))
              || (manifest.playerLogoImage.isNotEmpty() && ! assetExists (project, manifest.playerLogoImage))
              || (manifest.playerTitleBannerImage.isNotEmpty() && ! assetExists (project, manifest.playerTitleBannerImage)))
        {
            add (Severity::Error,
                 "Branding references missing files",
                 "One or more artwork paths are set but the file cannot be found. Export will either fail or generate fallback artwork.",
                 "Fix Artwork",
                 [this] { owner.setBottomTab (BottomPanel::Page::Branding); });
        }
        else
        {
            add (Severity::Pass,
                 "Branding assets are present",
                 "Artwork paths resolve and will be included in launch materials where available.");
        }

        const bool hasSalesOffer = manifest.salesHeadline.trim().isNotEmpty()
                                || manifest.salesSubheadline.trim().isNotEmpty()
                                || manifest.salesPrice > 0.0
                                || manifest.salesCheckoutUrl.trim().isNotEmpty();
        if (! hasSalesOffer)
        {
            add (Severity::Warning,
                 "Sales page offer is not configured",
                 "Set headline, offer copy, price, CTA, checkout URL, demo links, and included content so the customer package has a real purchase path.",
                 "Sales Page",
                 [this] { showSalesPageBuilder(); });
        }
        else if (manifest.salesPrice > 0.0 && manifest.salesCheckoutUrl.trim().isEmpty())
        {
            add (Severity::Warning,
                 "Paid product has no checkout URL",
                 "The sales page can be generated, but the buyer cannot purchase directly until a Plugin.club/product checkout URL is added.",
                 "Sales Page",
                 [this] { showSalesPageBuilder(); });
        }
        else
        {
            add (Severity::Pass,
                 "Sales page offer is configured",
                 salesHeadline (manifest) + "  |  " + priceText (manifest));
        }

        const bool hasClientIdentity = whiteLabelPublisher (manifest).trim().isNotEmpty()
                                    && whiteLabelProductName (manifest).trim().isNotEmpty();
        const bool hasSupportPath = manifest.playerSupportEmail.trim().isNotEmpty()
                                 || manifest.playerSupportUrl.trim().isNotEmpty()
                                 || manifest.playerManualUrl.trim().isNotEmpty();
        const bool hasInstallPayload = true; // Launch Bundle always exports a Player pack plus Player runtime installer payload.
        if (! hasClientIdentity || ! hasSupportPath || ! hasInstallPayload)
        {
            juce::StringArray missing;
            if (! hasClientIdentity) missing.add ("product/client identity");
            if (! hasSupportPath) missing.add ("support or manual link");
            add (Severity::Warning,
                 "White-label delivery package needs setup",
                 "Missing " + missing.joinIntoString (", ") + ". The Launch Bundle will still generate defaults, but client handoff will feel incomplete.",
                 "Brand Lab",
                 [this] { owner.setBottomTab (BottomPanel::Page::Branding); });
        }
        else
        {
            add (Severity::Pass,
                 "White-label product identity is ready",
                 whiteLabelProductName (manifest) + " by " + whiteLabelPublisher (manifest)
                    + " will generate installer manifests, activation copy, and client handoff docs.");
        }

        if (manifest.licenseRequired
            && (manifest.licenseProductId.trim().isEmpty() || manifest.licenseServerUrl.trim().isEmpty()))
        {
            add (Severity::Error,
                 "Licensing is enabled but incomplete",
                 "Protected instruments need a product ID and licensing server URL before publishing.",
                 "Settings",
                 [this] { owner.openSettings(); });
        }
        else if (manifest.licenseRequired)
        {
            add (Severity::Pass,
                 "Licensing metadata is configured",
                 "Product ID and license server URL are present. Verify activation in the exported Player.");
        }
        else
        {
            add (Severity::Info,
                 "Licensing is optional for this product",
                 "Turn on licensing in Brand Lab/Settings when this instrument should require activation.",
                 "Settings",
                 [this] { owner.openSettings(); });
        }

        const auto cloud = AiAssistService::loadCloudIntegrationConfig();
        if (cloud.pluginClubEndpoint.trim().isEmpty() || cloud.pluginClubApiKey.trim().isEmpty())
        {
            add (Severity::Warning,
                 "Plugin.club direct publish is not fully configured",
                 "Add the seller endpoint and seller API key in Settings to publish draft packs directly from Studio.",
                 "Settings",
                 [this] { owner.openSettings(); });
        }
        else if (PluginClubPublisher::normaliseSellerImportEndpoint (cloud.pluginClubEndpoint).isEmpty())
        {
            add (Severity::Error,
                 "Plugin.club endpoint is invalid",
                 "Use https://plugin.club/functions or https://plugin.club/functions/sellerImport in Settings.",
                 "Settings",
                 [this] { owner.openSettings(); });
        }
        else if (PluginClubPublisher::normaliseSellerImportEndpoint (cloud.pluginClubEndpoint)
                    != cloud.pluginClubEndpoint.trim().trimCharactersAtEnd ("/"))
        {
            add (Severity::Warning,
                 "Plugin.club endpoint will be normalized",
                 "Studio will publish to " + PluginClubPublisher::normaliseSellerImportEndpoint (cloud.pluginClubEndpoint)
                    + ". Update Settings to include /functions/sellerImport and avoid Base44 405 errors.",
                 "Settings",
                 [this] { owner.openSettings(); });
        }
        else
        {
            add (Severity::Pass,
                 "Plugin.club direct publish is configured",
                 VstExportModule::isVstExpansionInstalled()
                    ? "Studio can push instrument packs, standalone VST3 plugins, and one-shot packs as seller drafts."
                    : "Studio can push instrument packs and one-shot packs. Standalone VST3 publishing unlocks when the paid VST Expansion is installed.");
        }

        const auto missingDistribution = missingRuntimeDistributionItems();
        if (! missingDistribution.isEmpty())
        {
            add (Severity::Error,
                 "Studio distribution package is incomplete",
                 "Missing: " + missingDistribution.joinIntoString (", ")
                    + ". Build the PatchCraftRcBundle target before creating a release installer.",
                 "Create Bundle",
                 [this] { createLaunchBundle(); });
        }
        else
        {
            add (Severity::Pass,
                 "Studio distribution assets are staged",
                 "Runtime folder contains Studio, " + juce::String (countRuntimeFactoryDemos())
                    + " factory demos, Library assets, and both Player VST3 runtime plugins.");
        }

        add (Severity::Info,
             "Final DAW proof is still required",
             "After export, load the pack/plugin in FL Studio or another DAW and verify sound, tabs, MIDI learn, pad grids, presets, and volume.",
             "Test Runtime",
             [this]
             {
                 owner.setBottomTab (BottomPanel::Page::Branding);
                 if (! owner.isPreviewActive())
                     owner.togglePreview();
             });

        return checks;
    }

    void LaunchCenterPage::rebuildRows()
    {
        checkRows.clear();
        checksContent.removeAllChildren();

        errorCount = 0;
        warningCount = 0;
        passCount = 0;

        for (auto& item : buildChecks())
        {
            if (item.severity == Severity::Error) ++errorCount;
            if (item.severity == Severity::Warning) ++warningCount;
            if (item.severity == Severity::Pass) ++passCount;

            auto row = std::make_unique<CheckRow> (std::move (item));
            checksContent.addAndMakeVisible (*row);
            checkRows.push_back (std::move (row));
        }

        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        launchSummary = manifest.instrumentName + "\n"
            + engineDisplayName (project.getEngineType()) + "  |  "
            + plural ((int) project.getPresets().size(), "preset", "presets") + "  |  "
            + plural ((int) project.getSampleMap().getZones().size(), "sample zone", "sample zones") + "  |  "
            + plural (countRuntimeControls (project, true), "bound control", "bound controls") + "\n"
            + (errorCount == 0
                ? (warningCount == 0 ? "Ready to export/publish after DAW proof." : "Exportable, but review launch warnings.")
                : "Not launch-ready. Fix blocking items before export/publish.");

        summaryLabel.setText (launchSummary, juce::dontSendNotification);

        statusBadge.setText (errorCount > 0
                                ? "NEEDS FIXES"
                                : (warningCount > 0 ? "REVIEW" : "READY"),
                              juce::dontSendNotification);
        statusBadge.setColour (juce::Label::textColourId,
                               errorCount > 0 ? juce::Colour (0xffff5f5f)
                                              : (warningCount > 0 ? PatchCraftLookAndFeel::accent()
                                                                  : juce::Colour (0xff64d88a)));

        const bool canRelease = errorCount == 0;
        const bool vstExpansionInstalled = VstExportModule::isVstExpansionInstalled();
        exportPackButton.setEnabled (canRelease);
        exportVstButton.setEnabled (canRelease && vstExpansionInstalled);
        publishButton.setEnabled (canRelease);
        exportPackButton.setTooltip (canRelease ? "Export the current PatchCraft pack."
                                                : "Fix Launch Doctor blocking items before exporting.");
        exportVstButton.setTooltip (! vstExpansionInstalled ? VstExportModule::vstExpansionInstallMessage()
                                                            : (canRelease ? "Export a standalone VST3 from this product."
                                                                          : "Fix Launch Doctor blocking items before VST3 export."));
        publishButton.setTooltip (canRelease ? "Publish a Plugin.club seller draft."
                                             : "Fix Launch Doctor blocking items before publishing.");

        resized();
        repaint();
    }

    void LaunchCenterPage::refresh()
    {
        rebuildRows();
    }

    juce::File LaunchCenterPage::defaultLaunchRoot() const
    {
        const auto& project = owner.getProject();
        return project.getProjectFolder().isDirectory()
            ? project.getProjectFolder().getChildFile ("Launch")
            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("PatchCraft")
                .getChildFile ("LaunchBundles");
    }

    juce::File LaunchCenterPage::launchRoot() const
    {
        return selectedLaunchRoot.isDirectory() ? selectedLaunchRoot : defaultLaunchRoot();
    }

    void LaunchCenterPage::updateOutputFolderLabel()
    {
        const auto root = launchRoot();
        outputFolderLabel.setText ("Launch output: " + root.getFullPathName(), juce::dontSendNotification);
        outputFolderLabel.setTooltip ("Launch materials will be written under:\n" + root.getFullPathName());
    }

    void LaunchCenterPage::chooseOutputFolder()
    {
        outputFolderChooser = std::make_unique<juce::FileChooser> (
            "Choose PatchCraft Launch Output Folder",
            launchRoot(),
            "*");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectDirectories;

        outputFolderChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
        {
            const auto folder = chooser.getResult();
            if (! folder.isDirectory())
                return;

            selectedLaunchRoot = folder;
            updateOutputFolderLabel();
        });
    }

    juce::File LaunchCenterPage::createLaunchFolder (juce::String& error) const
    {
        const auto& manifest = owner.getProject().getManifest();
        const auto root = launchRoot();
        const auto folder = root.getChildFile (safeSlug (manifest.instrumentName)
            + "-" + juce::String (juce::Time::getCurrentTime().toMilliseconds()));

        if (! folder.createDirectory())
            error = "Could not create launch folder: " + folder.getFullPathName();

        return folder;
    }

    bool LaunchCenterPage::writeTextFile (const juce::File& file, const juce::String& text, juce::String& error) const
    {
        if (! file.getParentDirectory().createDirectory())
        {
            error = "Could not create folder: " + file.getParentDirectory().getFullPathName();
            return false;
        }

        if (! file.replaceWithText (text))
        {
            error = "Could not write file: " + file.getFullPathName();
            return false;
        }

        return true;
    }

    juce::String LaunchCenterPage::buildReadinessMarkdown()
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        juce::StringArray lines;
        lines.add ("# PatchCraft Launch Readiness");
        lines.add ("");
        lines.add ("Product: " + manifest.instrumentName);
        lines.add ("Creator: " + manifest.creator);
        lines.add ("Engine: " + engineDisplayName (project.getEngineType()));
        lines.add ("Generated: " + juce::Time::getCurrentTime().toString (true, true));
        lines.add ("");
        lines.add ("## Summary");
        lines.add ("");
        lines.add ("- Errors: " + juce::String (errorCount));
        lines.add ("- Warnings: " + juce::String (warningCount));
        lines.add ("- Passed checks: " + juce::String (passCount));
        lines.add ("- Presets: " + juce::String ((int) project.getPresets().size()));
        lines.add ("- Patches: " + juce::String ((int) project.getPatches().size()));
        lines.add ("- Sample zones: " + juce::String ((int) project.getSampleMap().getZones().size()));
        lines.add ("- Bound UI controls: " + juce::String (countRuntimeControls (project, true)));
        lines.add ("- Unbound UI controls: " + juce::String (countRuntimeControls (project, false)));
        lines.add ("");
        lines.add ("## Checks");
        lines.add ("");

        for (const auto& item : buildChecks())
        {
            lines.add ("### " + severityName (item.severity) + " - " + item.title);
            lines.add (markdownEscape (item.detail));
            lines.add ("");
        }

        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildProductPageMarkdown() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto tags = tagsFromManifest (manifest);
        const bool drum = isDrumProject (project);

        juce::StringArray lines;
        lines.add ("# " + manifest.instrumentName);
        lines.add ("");
        lines.add ("**By " + manifest.creator + "**");
        lines.add ("");
        if (manifest.description.trim().isNotEmpty())
            lines.add (markdownEscape (manifest.description));
        else
            lines.add ("A PatchCraft-built " + engineDisplayName (project.getEngineType()).toLowerCase()
                       + " with performance-ready controls, presets, and Player integration.");
        lines.add ("");
        lines.add ("## Highlights");
        lines.add ("");
        lines.add ("- Built in PatchCraft Studio with a custom Player interface.");
        lines.add ("- " + plural ((int) project.getPresets().size(), "playable preset", "playable presets")
                   + " backed by full Patch state.");
        lines.add ("- " + plural (countRuntimeControls (project, true), "mapped performance control", "mapped performance controls")
                   + " for real-time sound shaping.");
        if (drum)
            lines.add ("- Drum-machine workflow with pads and pattern-based performance.");
        if (! project.getSampleMap().getZones().empty())
            lines.add ("- " + plural ((int) project.getSampleMap().getZones().size(), "mapped sample zone", "mapped sample zones")
                       + " across key/velocity ranges.");
        if (! project.getDspGraph().blocks.empty())
            lines.add ("- Custom DSP graph with Source, Filter, Amp, Mod, FX, and Out routing.");
        lines.add ("");
        lines.add ("## Compatibility");
        lines.add ("");
        lines.add ("- PatchCraft Player pack");
        lines.add ("- PatchCraft Player runtime path");
        lines.add ("- Optional standalone VST3 path through the paid VST Expansion addon");
        lines.add ("- Tested workflow targets: FL Studio, Ableton Live, Logic Pro, Cubase, Studio One, Reaper, Bitwig");
        lines.add ("");
        lines.add ("## Tags");
        lines.add ("");
        lines.add (tags.isEmpty() ? "PatchCraft" : tags.joinIntoString (", "));
        lines.add ("");
        lines.add ("## Seller QA");
        lines.add ("");
        lines.add ("- Load in the PatchCraft Player.");
        lines.add ("- Verify software and hardware MIDI playback.");
        lines.add ("- Move every UI control while audio is playing.");
        lines.add ("- Switch every preset and container tab.");
        lines.add ("- Re-scan PatchCraft Player in a DAW and confirm product name/artwork.");
        lines.add ("- If VST Expansion is installed, re-scan exported standalone VST3 in a DAW.");

        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildSalesPageMarkdown() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto highlights = manifest.salesHighlights.isEmpty()
            ? juce::StringArray {
                "Custom branded PatchCraft Player instrument.",
                plural ((int) project.getPresets().size(), "playable preset", "playable presets") + " with full patch-state recall.",
                plural (countRuntimeControls (project, true), "performance control", "performance controls") + " mapped to real sound parameters.",
                "Ready for DAW use through PatchCraft Player, with optional standalone VST3 export through the VST Expansion addon."
              }
            : manifest.salesHighlights;
        const auto includes = manifest.salesIncludes.isEmpty()
            ? juce::StringArray {
                "PatchCraft Player instrument pack",
                "Optional standalone VST3 export package when purchased",
                "Preset library",
                "Installation and license instructions",
                "Customer support link"
              }
            : manifest.salesIncludes;

        juce::StringArray lines;
        lines.add ("# " + salesHeadline (manifest));
        lines.add ("");
        lines.add (salesSubheadline (manifest));
        lines.add ("");
        lines.add ("**Product:** " + manifest.instrumentName);
        lines.add ("**Creator:** " + manifest.creator);
        if (manifest.playerClientName.isNotEmpty())
            lines.add ("**Built for:** " + manifest.playerClientName);
        lines.add ("**Price:** " + priceText (manifest));
        if (manifest.salesCompareAtPrice > manifest.salesPrice && manifest.salesCompareAtPrice > 0.0)
            lines.add ("**Compare at:** " + manifest.salesCurrency + " " + juce::String (manifest.salesCompareAtPrice, 2));
        if (manifest.salesCheckoutUrl.isNotEmpty())
            lines.add ("**Checkout:** " + manifest.salesCheckoutUrl);
        lines.add ("");
        lines.add ("## Why Buyers Want It");
        lines.add ("");
        for (const auto& item : highlights)
            lines.add ("- " + markdownEscape (item));
        lines.add ("");
        lines.add ("## What Is Included");
        lines.add ("");
        for (const auto& item : includes)
            lines.add ("- " + markdownEscape (item));
        lines.add ("");
        if (manifest.description.trim().isNotEmpty())
        {
            lines.add ("## Product Story");
            lines.add ("");
            lines.add (markdownEscape (manifest.description));
            lines.add ("");
        }
        lines.add ("## Demos");
        lines.add ("");
        lines.add ("- Audio demo: " + (manifest.salesAudioDemoUrl.isNotEmpty() ? manifest.salesAudioDemoUrl : "Add audio demo URL"));
        lines.add ("- Video demo: " + (manifest.salesDemoVideoUrl.isNotEmpty() ? manifest.salesDemoVideoUrl : "Add video demo URL"));
        lines.add ("");
        lines.add ("## Compatibility");
        lines.add ("");
        lines.add ("- PatchCraft Player compatible instrument pack");
        lines.add ("- PatchCraft Player VST3 path for supported DAWs");
        lines.add ("- Optional standalone VST3 path when the VST Expansion addon is included");
        lines.add ("- Target DAWs: FL Studio, Ableton Live, Logic Pro, Cubase, Studio One, Reaper, Bitwig");
        lines.add ("");
        lines.add ("## FAQ");
        lines.add ("");
        const auto faqs = manifest.salesFaq.isEmpty()
            ? juce::StringArray {
                "Do I need PatchCraft Studio?|No. End buyers use the exported Player/instrument package.",
                "Can I use hardware MIDI?|Yes. The Player supports MIDI input and MIDI learn when enabled.",
                "Can I install it in my DAW?|Yes. Use the delivered Player pack. Dedicated standalone VST3 delivery is available when the VST Expansion addon is included."
              }
            : manifest.salesFaq;
        for (const auto& faq : faqs)
        {
            const auto question = faq.upToFirstOccurrenceOf ("|", false, false).trim();
            const auto answer = faq.fromFirstOccurrenceOf ("|", false, false).trim();
            lines.add ("### " + (question.isNotEmpty() ? question : faq));
            lines.add (answer.isNotEmpty() ? answer : "Add answer.");
            lines.add ("");
        }
        lines.add ("## Call To Action");
        lines.add ("");
        lines.add ((manifest.salesCtaText.isNotEmpty() ? manifest.salesCtaText : "Buy Now")
            + (manifest.salesCheckoutUrl.isNotEmpty() ? ": " + manifest.salesCheckoutUrl : ""));

        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildSalesPageHtml() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto highlights = manifest.salesHighlights.isEmpty()
            ? juce::StringArray {
                "Custom branded PatchCraft Player instrument.",
                plural ((int) project.getPresets().size(), "playable preset", "playable presets") + " with full patch-state recall.",
                plural (countRuntimeControls (project, true), "performance control", "performance controls") + " mapped to real sound parameters.",
                "Ready for DAW use through PatchCraft Player, with optional standalone VST3 export through the VST Expansion addon."
              }
            : manifest.salesHighlights;
        const auto includes = manifest.salesIncludes.isEmpty()
            ? juce::StringArray { "PatchCraft Player instrument pack", "Optional standalone VST3 export package when purchased", "Preset library", "Installation and license instructions" }
            : manifest.salesIncludes;

        auto listHtml = [] (const juce::StringArray& values)
        {
            juce::String out;
            for (const auto& value : values)
                out << "<li>" << htmlEscape (value) << "</li>\n";
            return out;
        };

        const auto accent = manifest.playerAccentColour.toDisplayString (false);
        const auto bg = manifest.playerBackgroundColour.toDisplayString (false);
        const auto panel = manifest.playerPanelColour.toDisplayString (false);
        const auto text = manifest.playerTextColour.toDisplayString (false);
        const auto dim = manifest.playerTextDimColour.toDisplayString (false);
        const auto title = salesHeadline (manifest);
        const auto checkout = manifest.salesCheckoutUrl.trim();
        const auto cta = manifest.salesCtaText.isNotEmpty() ? manifest.salesCtaText : juce::String ("Buy Now");

        juce::String html;
        html << "<!doctype html>\n<html lang=\"en\">\n<head>\n"
             << "<meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
             << "<title>" << htmlEscape (manifest.instrumentName) << "</title>\n"
             << "<style>\n"
             << ":root{--bg:#" << bg << ";--panel:#" << panel << ";--text:#" << text << ";--dim:#" << dim << ";--accent:#" << accent << ";}\n"
             << "body{margin:0;background:radial-gradient(circle at top,#1b2430,var(--bg));color:var(--text);font-family:Inter,Segoe UI,Arial,sans-serif;}\n"
             << ".wrap{max-width:1120px;margin:0 auto;padding:56px 28px 72px;}.hero{border:1px solid color-mix(in srgb,var(--accent),transparent 35%);background:linear-gradient(135deg,color-mix(in srgb,var(--panel),white 6%),rgba(0,0,0,.42));border-radius:28px;padding:44px;box-shadow:0 24px 80px rgba(0,0,0,.35)}\n"
             << "h1{font-size:clamp(38px,6vw,76px);line-height:.94;margin:0 0 18px}.sub{font-size:22px;color:var(--dim);max-width:780px}.meta{display:flex;gap:16px;flex-wrap:wrap;margin:28px 0}.pill{border:1px solid rgba(255,255,255,.12);border-radius:999px;padding:10px 14px;background:rgba(255,255,255,.04)}\n"
             << ".cta{display:inline-block;margin-top:8px;background:var(--accent);color:#05070a;text-decoration:none;font-weight:800;border-radius:14px;padding:16px 24px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:22px;margin-top:28px}.card{background:rgba(255,255,255,.045);border:1px solid rgba(255,255,255,.10);border-radius:20px;padding:24px}.price{font-size:34px;font-weight:900;color:var(--accent)}li{margin:10px 0}.footer{color:var(--dim);margin-top:34px;font-size:13px}@media(max-width:760px){.grid{grid-template-columns:1fr}.hero{padding:28px}}\n"
             << "</style>\n</head>\n<body><main class=\"wrap\"><section class=\"hero\">\n"
             << "<h1>" << htmlEscape (title) << "</h1>\n"
             << "<p class=\"sub\">" << htmlEscape (salesSubheadline (manifest)) << "</p>\n"
             << "<div class=\"meta\"><span class=\"pill\">" << htmlEscape (manifest.creator) << "</span><span class=\"pill\">"
             << htmlEscape (engineDisplayName (project.getEngineType())) << "</span><span class=\"pill\">"
             << htmlEscape (juce::String ((int) project.getPresets().size()) + " presets") << "</span></div>\n"
             << "<div class=\"price\">" << htmlEscape (priceText (manifest)) << "</div>\n";
        if (checkout.isNotEmpty())
            html << "<a class=\"cta\" href=\"" << htmlEscape (checkout) << "\">" << htmlEscape (cta) << "</a>\n";
        html << "</section><section class=\"grid\">\n"
             << "<div class=\"card\"><h2>Why buyers want it</h2><ul>" << listHtml (highlights) << "</ul></div>\n"
             << "<div class=\"card\"><h2>Included</h2><ul>" << listHtml (includes) << "</ul></div>\n"
             << "<div class=\"card\"><h2>Demos</h2><p>Audio: " << htmlEscape (manifest.salesAudioDemoUrl.isNotEmpty() ? manifest.salesAudioDemoUrl : "Add audio demo URL")
             << "</p><p>Video: " << htmlEscape (manifest.salesDemoVideoUrl.isNotEmpty() ? manifest.salesDemoVideoUrl : "Add video demo URL") << "</p></div>\n"
             << "<div class=\"card\"><h2>Compatibility</h2><p>PatchCraft Player pack for FL Studio, Ableton Live, Logic Pro, Cubase, Studio One, Reaper, and Bitwig. Dedicated standalone VST3 delivery is available when the VST Expansion addon is included.</p></div>\n"
             << "</section><p class=\"footer\">" << htmlEscape (manifest.playerCopyright.isNotEmpty() ? manifest.playerCopyright : manifest.creator)
             << (manifest.playerShowPatchCraftBranding ? " · Powered by PatchCraft" : "") << "</p></main></body></html>\n";
        return html;
    }

    juce::String LaunchCenterPage::buildTestPlanMarkdown() const
    {
        juce::StringArray lines;
        lines.add ("# PatchCraft Runtime Test Plan");
        lines.add ("");
        lines.add ("1. Open the exported pack in PatchCraft Player.");
        lines.add ("2. Confirm the default preset loads and produces expected volume.");
        lines.add ("3. Play software keyboard notes and hardware MIDI notes.");
        lines.add ("4. Verify mod wheel, expression, sustain, retrigger, BPM sync, pads, and drum grids where used.");
        lines.add ("5. Move every mapped knob/slider/switch while holding notes.");
        lines.add ("6. Switch all Player tabs/containers and confirm only the correct controls are visible.");
        lines.add ("7. Switch every preset and confirm it changes the sound musically.");
        lines.add ("8. Load PatchCraft Player/Player FX in FL Studio and verify there is no Player-pack crossover unless intentionally enabled.");
        lines.add ("9. If VST Expansion is installed, export standalone VST3 and verify it scans as a dedicated branded plugin.");
        lines.add ("10. Publish a Plugin.club draft, open the edit URL, and confirm metadata, artwork, files, licensing, and price/status.");
        lines.add ("11. Record audio demos/screenshots after DAW verification.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildInstallerChecklistMarkdown() const
    {
        const auto appDir = runtimeFolder();
        const auto missing = missingRuntimeDistributionItems();

        juce::StringArray lines;
        lines.add ("# PatchCraft Installer / Distribution Checklist");
        lines.add ("");
        lines.add ("Generated: " + juce::Time::getCurrentTime().toString (true, true));
        lines.add ("Runtime folder inspected: `" + appDir.getFullPathName() + "`");
        lines.add ("");
        lines.add ("## Required Payload");
        lines.add ("");
        lines.add ("- `PatchCraftStudio.exe`");
        lines.add ("- `FactoryDemos/` with the approved `.patchcraft` factory products");
        lines.add ("- `Library/Backgrounds`, `Library/Templates`, and `Library/Assets`");
        lines.add ("- `PlayerPlugins/PatchCraft Player.vst3`");
        lines.add ("- `PlayerPlugins/PatchCraft Player FX.vst3`");
        lines.add ("- `docs/` and launch handoff documents");
        lines.add ("");
        lines.add ("## Current Staging Status");
        lines.add ("");
        if (missing.isEmpty())
            lines.add ("- PASS: Required Studio distribution assets are staged.");
        else
            for (const auto& item : missing)
                lines.add ("- FIX: Missing " + item);
        lines.add ("- Factory demos found: " + juce::String (countRuntimeFactoryDemos()));
        lines.add ("");
        lines.add ("## Installer Behavior");
        lines.add ("");
        lines.add ("- Install Studio into a writable application folder.");
        lines.add ("- Keep `FactoryDemos`, `Library`, and `PlayerPlugins` beside the executable.");
        lines.add ("- Install Player VST3 and Player FX VST3 to the per-user VST3 folder by default.");
        lines.add ("- Keep paid VST Expansion templates out of the base installer; install `PluginTemplate` only through the VST Expansion package.");
        lines.add ("- Offer system VST3 install only through an elevated installer path.");
        lines.add ("- Preserve user data under Documents/PatchCraft and AppData/PatchCraft.");
        lines.add ("- For white-label products, include `installer/white-label-product.json` beside the branded installer.");
        lines.add ("- Build the client installer from `installer/windows-inno-setup.iss` or the macOS pkgbuild notes.");
        lines.add ("");
        lines.add ("## Required Manual Proof");
        lines.add ("");
        lines.add ("- Run Launch Doctor with no blocking errors.");
        lines.add ("- Export a pack and load it in PatchCraft Player.");
        lines.add ("- Load PatchCraft Player and Player FX in FL Studio.");
        lines.add ("- If VST Expansion is installed, export standalone VST3 and rescan it in FL Studio.");
        lines.add ("- Confirm hardware MIDI, mod wheel, expression, sustain, tabs, labels, pads/drum grids, presets, and volume.");
        lines.add ("- Publish a Plugin.club draft through `https://plugin.club/functions/sellerImport`.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildClientDeliveryMarkdown() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();

        juce::StringArray lines;
        lines.add ("# White-Label Client Delivery Kit");
        lines.add ("");
        lines.add ("Product: " + manifest.instrumentName);
        lines.add ("Client / Artist: " + (manifest.playerClientName.isNotEmpty() ? manifest.playerClientName : manifest.creator));
        lines.add ("Generated: " + juce::Time::getCurrentTime().toString (true, true));
        lines.add ("");
        lines.add ("## What To Deliver");
        lines.add ("");
        lines.add ("- Branded PatchCraft Player instrument pack.");
        lines.add ("- Standalone VST3 export when purchased as a dedicated plugin.");
        lines.add ("- Installer or ZIP package with clear install path.");
        lines.add ("- License/activation instructions and support contact.");
        lines.add ("- Sales page HTML/Markdown and Plugin.club draft metadata.");
        lines.add ("- `installer/white-label-product.json`, Windows installer script, macOS packaging notes, and activation flow.");
        lines.add ("- Audio/video demos and artwork assets.");
        lines.add ("");
        lines.add ("## Buyer-Facing Links");
        lines.add ("");
        lines.add ("- Checkout: " + (manifest.salesCheckoutUrl.isNotEmpty() ? manifest.salesCheckoutUrl : "TODO"));
        lines.add ("- Support: " + (manifest.playerSupportUrl.isNotEmpty() ? manifest.playerSupportUrl : "TODO"));
        lines.add ("- Manual: " + (manifest.playerManualUrl.isNotEmpty() ? manifest.playerManualUrl : "TODO"));
        lines.add ("- Store: " + (manifest.playerStoreUrl.isNotEmpty() ? manifest.playerStoreUrl : "TODO"));
        lines.add ("");
        lines.add ("## Runtime QA Before Hand-Off");
        lines.add ("");
        lines.add ("- Load in PatchCraft Player and exported VST3.");
        lines.add ("- Confirm the Player opens with correct logo, title, client name, support links, and About panel.");
        lines.add ("- Confirm no unwanted PatchCraft branding if `Show PatchCraft Credit` is disabled.");
        lines.add ("- Confirm licensing status and trial/offline behavior.");
        lines.add ("- Confirm hardware MIDI, pads, tab switching, presets, sample import, MIDI import, and mapped controls.");
        lines.add ("- Confirm all audio demos match the shipped preset names.");
        lines.add ("");
        lines.add ("## Commercial Positioning");
        lines.add ("");
        lines.add ("Headline: " + salesHeadline (manifest));
        lines.add ("Price: " + priceText (manifest));
        lines.add ("CTA: " + (manifest.salesCtaText.isNotEmpty() ? manifest.salesCtaText : "Buy Now"));
        lines.add ("Preset count: " + juce::String ((int) project.getPresets().size()));
        lines.add ("Mapped controls: " + juce::String (countRuntimeControls (project, true)));
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildCustomerPackageWizardMarkdown() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto missing = missingRuntimeDistributionItems();

        juce::StringArray lines;
        lines.add ("# White-Label Customer Package Wizard");
        lines.add ("");
        lines.add ("Generated: " + juce::Time::getCurrentTime().toString (true, true));
        lines.add ("Project: " + manifest.instrumentName);
        lines.add ("Engine: " + engineDisplayName (project.getEngineType()));
        lines.add ("Customer product: " + whiteLabelProductName (manifest));
        lines.add ("Publisher: " + whiteLabelPublisher (manifest));
        lines.add ("Product code: " + whiteLabelProductCode (manifest));
        lines.add ("Bundle ID: `" + whiteLabelBundleId (manifest) + "`");
        lines.add ("Launch Doctor: " + juce::String (errorCount) + " errors, " + juce::String (warningCount) + " warnings");
        lines.add ("");
        lines.add ("## 1. Product Identity");
        lines.add ("");
        lines.add ("- Buyer-facing name: " + whiteLabelProductName (manifest));
        lines.add ("- Display name in Player: " + (manifest.playerDisplayName.isNotEmpty() ? manifest.playerDisplayName : manifest.instrumentName));
        lines.add ("- Client / artist: " + (manifest.playerClientName.isNotEmpty() ? manifest.playerClientName : "TODO"));
        lines.add ("- Support email: " + (manifest.playerSupportEmail.isNotEmpty() ? manifest.playerSupportEmail : "TODO"));
        lines.add ("- Support URL: " + (manifest.playerSupportUrl.isNotEmpty() ? manifest.playerSupportUrl : "TODO"));
        lines.add ("- Manual URL: " + (manifest.playerManualUrl.isNotEmpty() ? manifest.playerManualUrl : "TODO"));
        lines.add ("");
        lines.add ("## 2. Installer Payload");
        lines.add ("");
        lines.add ("- Include PatchCraft Player VST3: " + juce::String (manifest.whiteLabelIncludeVst3 ? "yes" : "no"));
        lines.add ("- Include Player FX VST3: yes, when FX routing or Player FX support is part of the product.");
        lines.add ("- Include standalone branded VST3: " + juce::String (manifest.whiteLabelIncludeStandalone ? "yes" : "no") + " (requires VST Expansion addon).");
        lines.add ("- Include generated `.patchcraft` pack under `payload/Packs`.");
        lines.add ("- Include `installer/white-label-product.json` as the canonical installer/runtime manifest.");
        lines.add ("- Windows VST3 path: `" + (manifest.whiteLabelWindowsVst3Path.isNotEmpty() ? manifest.whiteLabelWindowsVst3Path : juce::String ("per-user VST3 folder")) + "`");
        lines.add ("- macOS VST3 path: `" + (manifest.whiteLabelMacVst3Path.isNotEmpty() ? manifest.whiteLabelMacVst3Path : juce::String ("/Library/Audio/Plug-Ins/VST3")) + "`");
        lines.add ("");
        lines.add ("## 3. Licensing And Protection");
        lines.add ("");
        lines.add ("- License required: " + juce::String (manifest.licenseRequired ? "yes" : "no"));
        lines.add ("- Require on first run: " + juce::String (manifest.whiteLabelRequireLicenseOnFirstRun ? "yes" : "no"));
        lines.add ("- License server URL: " + (manifest.licenseServerUrl.isNotEmpty() ? manifest.licenseServerUrl : "TODO"));
        lines.add ("- Product ID: " + (manifest.licenseProductId.isNotEmpty() ? manifest.licenseProductId : "TODO"));
        lines.add ("- Public key configured: " + juce::String (manifest.licensePublicKey.isNotEmpty() ? "yes" : "no"));
        lines.add ("- Trial days: " + juce::String (manifest.trialDays));
        lines.add ("- Offline grace days: " + juce::String (manifest.licenseOfflineGraceDays));
        lines.add ("");
        lines.add ("## 4. Buyer First-Run Flow");
        lines.add ("");
        lines.add ("1. Buyer runs the branded installer.");
        lines.add ("2. Installer places Player/Player FX VST3 files into the selected VST3 folder and installs the pack under the product data folder.");
        lines.add ("3. Buyer opens the DAW, rescans plugins, and loads " + whiteLabelProductName (manifest) + ".");
        lines.add ("4. Player shows branded title, artwork, support/manual links, and activation state.");
        lines.add ("5. If licensing is enabled, buyer activates with the connected license server.");
        lines.add ("6. Default preset loads and produces sound without opening PatchCraft Studio.");
        lines.add ("7. Presets, tabs, MIDI learn, imported samples/MIDI, pads, drum grids, and performance controls behave exactly like Brand Lab.");
        lines.add ("");
        lines.add ("## 5. Required DAW QA");
        lines.add ("");
        lines.add ("- Install on a clean Windows user account.");
        lines.add ("- Rescan FL Studio and at least one second DAW.");
        lines.add ("- Confirm Player and Player FX load only their intended product/pack.");
        lines.add ("- Confirm no UI resize misalignment, no duplicate title/preset bars, and no overlapped menu text.");
        lines.add ("- Confirm tab/container switching, keyboard playback, hardware MIDI, pitch wheel, mod wheel, sustain, pads, and drum pattern playback.");
        lines.add ("- Move every exposed knob/slider/switch while audio is playing and confirm real-time response.");
        lines.add ("- Verify installer uninstall does not delete user presets, imports, or license cache unless explicitly requested.");
        lines.add ("");
        lines.add ("## 6. Sales And Publishing");
        lines.add ("");
        lines.add ("- Sales headline: " + salesHeadline (manifest));
        lines.add ("- Price: " + priceText (manifest));
        lines.add ("- Checkout URL: " + (manifest.salesCheckoutUrl.isNotEmpty() ? manifest.salesCheckoutUrl : "TODO"));
        lines.add ("- Plugin.club endpoint: `https://plugin.club/functions/sellerImport`");
        lines.add ("- Launch licensing endpoint: `https://plugin.club/functions/deviceAuth` until AudiLock becomes the source of truth.");
        lines.add ("- Generated files: `sales-page.md`, `sales-page.html`, `pluginclub-metadata-preview.json`, and `release-manifest.json`.");
        lines.add ("");
        lines.add ("## Current Blocking Items");
        lines.add ("");
        if (errorCount == 0 && missing.isEmpty())
            lines.add ("- No automatic launch blockers found. Manual DAW proof is still required before customer delivery.");
        else
        {
            if (errorCount > 0)
                lines.add ("- Launch Doctor has " + juce::String (errorCount) + " blocking error(s). Run Launch Doctor and fix those rows first.");
            for (const auto& item : missing)
                lines.add ("- Missing distribution item: " + item);
        }

        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildSellerLaunchPlaybookMarkdown() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();

        juce::StringArray lines;
        lines.add ("# Seller Launch Playbook");
        lines.add ("");
        lines.add ("Product: " + whiteLabelProductName (manifest));
        lines.add ("Generated: " + juce::Time::getCurrentTime().toString (true, true));
        lines.add ("");
        lines.add ("## Positioning");
        lines.add ("");
        lines.add ("- Promise: " + salesHeadline (manifest));
        lines.add ("- Buyer: producers, composers, beatmakers, sound designers, and artists who want fast usable sounds.");
        lines.add ("- Proof: " + juce::String ((int) project.getPresets().size()) + " presets, "
                   + juce::String (countRuntimeControls (project, true)) + " mapped controls, branded Player UI, and DAW-ready installer path.");
        lines.add ("- Differentiator: this is a finished branded Player product, not a loose sample folder.");
        lines.add ("");
        lines.add ("## Required Store Assets");
        lines.add ("");
        lines.add ("- Hero product image showing the full Player.");
        lines.add ("- 3-5 cropped screenshots: preset browser, main controls, performance page, rack/mixer, Sound DNA/control center.");
        lines.add ("- 60-90 second audio demo with dry intro, performance automation, and preset switching.");
        lines.add ("- 20-30 second social clip with a clear before/after or preset walkthrough.");
        lines.add ("- Feature bullets copied from `sales-page.md` and Plugin.club metadata.");
        lines.add ("- Install/activation screenshots from the buyer quick-start guide.");
        lines.add ("");
        lines.add ("## Launch Sequence");
        lines.add ("");
        lines.add ("1. Install from the generated installer on a clean machine.");
        lines.add ("2. Open in FL Studio and one second DAW, record QA screenshots.");
        lines.add ("3. Render audio demos from the shipped presets only.");
        lines.add ("4. Publish Plugin.club draft and verify checkout, metadata, artwork, license config, and download file.");
        lines.add ("5. Send beta installers to 3-5 producers and capture short quotes.");
        lines.add ("6. Publish the sales page, upload videos, and schedule social clips.");
        lines.add ("7. Keep support/manual/update links live before accepting paid buyers.");
        lines.add ("");
        lines.add ("## Support Script");
        lines.add ("");
        lines.add ("- If the plugin is missing: rescan VST3 folder and confirm install path.");
        lines.add ("- If there is no sound: check audio device, MIDI input, selected preset, and license state.");
        lines.add ("- If activation fails: verify license key/product ID and online access, then retry after clearing the license cache only if instructed.");
        lines.add ("- If UI appears wrong: confirm the buyer is using the latest installer and did not resize a fixed-layout Player beyond supported bounds.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildBuyerQuickStartMarkdown() const
    {
        const auto& manifest = owner.getProject().getManifest();

        juce::StringArray lines;
        lines.add ("# " + whiteLabelProductName (manifest) + " Buyer Quick Start");
        lines.add ("");
        lines.add ("## Install");
        lines.add ("");
        lines.add ("1. Close your DAW.");
        lines.add ("2. Run the installer.");
        lines.add ("3. Use the default VST3 path unless your DAW scans a custom folder.");
        lines.add ("4. Open the DAW and rescan plugins.");
        lines.add ("5. Load PatchCraft Player or the dedicated branded VST3 if included.");
        lines.add ("");
        lines.add ("## Activate");
        lines.add ("");
        if (manifest.licenseRequired)
        {
            lines.add ("1. Open the Player.");
            lines.add ("2. Enter the license key or account details provided with your purchase.");
            lines.add ("3. Keep the product ID unchanged: `" + (manifest.licenseProductId.isNotEmpty() ? manifest.licenseProductId : juce::String ("configured by seller")) + "`.");
            lines.add ("4. If offline grace is enabled, reconnect before the grace period expires.");
        }
        else
        {
            lines.add ("This product does not require activation.");
        }
        lines.add ("");
        lines.add ("## First Sound Check");
        lines.add ("");
        lines.add ("1. Select the default preset.");
        lines.add ("2. Play C3-C5 on your MIDI keyboard or the on-screen keyboard.");
        lines.add ("3. Open `SND` for performance controls, `RACK` for layers, `CTRL` for info/routing, `SNAP` for saved user states, and `DNA` to see what shapes the sound.");
        lines.add ("4. Save a Snapshot after making a sound you want to keep.");
        lines.add ("");
        lines.add ("## Support");
        lines.add ("");
        lines.add ("- Support: " + (manifest.playerSupportUrl.isNotEmpty() ? manifest.playerSupportUrl : manifest.playerSupportEmail));
        lines.add ("- Manual: " + (manifest.playerManualUrl.isNotEmpty() ? manifest.playerManualUrl : "provided by seller"));
        lines.add ("- Store: " + (manifest.playerStoreUrl.isNotEmpty() ? manifest.playerStoreUrl : manifest.salesCheckoutUrl));
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildMarketplaceAssetChecklistMarkdown() const
    {
        const auto& manifest = owner.getProject().getManifest();

        juce::StringArray lines;
        lines.add ("# Marketplace Asset Checklist");
        lines.add ("");
        lines.add ("Product: " + whiteLabelProductName (manifest));
        lines.add ("");
        lines.add ("## Required");
        lines.add ("");
        lines.add ("- 1920x1080 hero image.");
        lines.add ("- 1000x1000 square cover art.");
        lines.add ("- 6-10 screenshots: full Player, preset menu, Sound page, Rack/Mixer, Snapshots, Sound DNA, About/Support.");
        lines.add ("- Audio demo WAV/MP3.");
        lines.add ("- Short social video.");
        lines.add ("- Installer ZIP and checksum.");
        lines.add ("- EULA, privacy URL, support URL, and manual URL.");
        lines.add ("- Plugin.club title, category, tags, price, compatibility, and license config.");
        lines.add ("");
        lines.add ("## Copy Blocks");
        lines.add ("");
        lines.add ("- Headline: " + salesHeadline (manifest));
        lines.add ("- CTA: " + (manifest.salesCtaText.isNotEmpty() ? manifest.salesCtaText : juce::String ("Buy Now")));
        lines.add ("- Price: " + priceText (manifest));
        lines.add ("- Checkout URL: " + (manifest.salesCheckoutUrl.isNotEmpty() ? manifest.salesCheckoutUrl : juce::String ("TODO")));
        lines.add ("");
        lines.add ("## Final QA Screenshots");
        lines.add ("");
        lines.add ("- Activated Player in DAW.");
        lines.add ("- Preset list open.");
        lines.add ("- Snapshot saved and recalled.");
        lines.add ("- Sound DNA panel showing signal formula.");
        lines.add ("- Library/about/support links visible.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildInstallerReadmeMarkdown() const
    {
        const auto& manifest = owner.getProject().getManifest();

        juce::StringArray lines;
        lines.add ("# " + whiteLabelProductName (manifest) + " Installer Kit");
        lines.add ("");
        lines.add ("This folder is the handoff point for building a customer-facing installer for the branded Player product.");
        lines.add ("");
        lines.add ("## Files Generated");
        lines.add ("");
        lines.add ("- `white-label-product.json`: canonical product, payload, licensing, support, and install metadata.");
        lines.add ("- `license-activation.json`: activation request template for Plugin.club/AudiLock integration.");
        lines.add ("- `windows-inno-setup.iss`: starter Inno Setup script for Windows VST3 delivery.");
        lines.add ("- `macos-pkgbuild-notes.md`: macOS pkgbuild/productbuild packaging notes.");
        lines.add ("- `activation-flow.md`: buyer-facing activation and trial behavior.");
        lines.add ("- `artifact-manifest.json`: SHA256 checksums for the generated payload and handoff files.");
        lines.add ("- `installer-readme.md`: this implementation guide.");
        lines.add ("");
        lines.add ("## Expected Build Input");
        lines.add ("");
        lines.add ("- The Launch Bundle exports the `.patchcraft` pack into `payload/Packs` automatically.");
        lines.add ("- The Launch Bundle copies PatchCraft Player/Player FX into `payload/PlayerPlugins` when they are staged with Studio.");
        lines.add ("- If the customer purchased a dedicated plugin, export the standalone VST3 with the VST Expansion addon and place it in `payload/StandaloneVST3`.");
        lines.add ("- Add EULA, privacy policy, icon, and code-signing material before public release.");
        lines.add ("- Keep product identity in Brand Lab consistent with this generated manifest.");
        lines.add ("- Do not expose PatchCraft authoring metadata to buyers; use white-label product names and support links.");
        lines.add ("");
        lines.add ("## Non-Negotiable QA");
        lines.add ("");
        lines.add ("- Install on a clean machine or VM.");
        lines.add ("- Rescan in FL Studio, Ableton Live, Studio One/Reaper, and at least one DAW the client uses.");
        lines.add ("- Confirm license activation, offline grace, support links, About panel, presets, MIDI learn, tab switching, and audio output.");
        lines.add ("- Confirm uninstall removes plugin files but never deletes user-created presets or imported samples.");
        lines.add ("- Compare shipped files against `artifact-manifest.json` before uploading to Plugin.club or sending beta installers.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildWindowsInstallerScript() const
    {
        const auto& manifest = owner.getProject().getManifest();
        const auto productName = whiteLabelProductName (manifest);
        const auto publisher = whiteLabelPublisher (manifest);
        const auto fileSafe = safeSlug (productName);
        const auto version = manifest.version.isNotEmpty() ? manifest.version : juce::String ("1.0.0");
        const auto vst3Folder = manifest.whiteLabelWindowsVst3Path.isNotEmpty()
            ? manifest.whiteLabelWindowsVst3Path
            : juce::String (R"({localappdata}\Programs\Common\VST3)");
        const auto vst3Dest = vst3Folder.containsIgnoreCase ("CommonFilesFolder")
            ? juce::String (R"({commoncf64}\VST3)")
            : vst3Folder.replace ("%LOCALAPPDATA%", "{localappdata}", true);

        juce::StringArray lines;
        lines.add ("; PatchCraft generated Inno Setup starter script");
        lines.add ("; Review paths, signing, EULA, and support URLs before shipping.");
        lines.add ("#define ProductName \"" + innoString (productName) + "\"");
        lines.add ("#define ProductPublisher \"" + innoString (publisher) + "\"");
        lines.add ("#define ProductVersion \"" + innoString (version) + "\"");
        lines.add ("#define ProductCode \"" + innoString (whiteLabelProductCode (manifest)) + "\"");
        lines.add ("#define SourceDir \"..\\payload\"");
        lines.add ("");
        lines.add ("[Setup]");
        lines.add ("AppId=" + innoString (whiteLabelInstallerId (manifest)));
        lines.add ("AppName={#ProductName}");
        lines.add ("AppVersion={#ProductVersion}");
        lines.add ("AppPublisher={#ProductPublisher}");
        lines.add ("DefaultDirName={localappdata}\\Programs\\{#ProductPublisher}\\{#ProductName}");
        lines.add ("DefaultGroupName={#ProductName}");
        lines.add ("OutputBaseFilename=" + fileSafe + "_Installer");
        lines.add ("Compression=lzma2");
        lines.add ("SolidCompression=yes");
        lines.add ("ArchitecturesAllowed=x64");
        lines.add ("ArchitecturesInstallIn64BitMode=x64");
        lines.add ("PrivilegesRequired=lowest");
        if (manifest.whiteLabelInstallerIcon.isNotEmpty())
            lines.add ("SetupIconFile=\"" + innoString (manifest.whiteLabelInstallerIcon) + "\"");
        if (manifest.whiteLabelEulaPath.isNotEmpty())
            lines.add ("LicenseFile=\"" + innoString (manifest.whiteLabelEulaPath) + "\"");
        lines.add ("");
        lines.add ("[Files]");
        lines.add ("Source: \"{#SourceDir}\\Packs\\" + safeSlug (manifest.instrumentName) + ".patchcraft\\*\"; DestDir: \"{userappdata}\\{#ProductPublisher}\\{#ProductName}\\Packs\\" + safeSlug (manifest.instrumentName) + ".patchcraft\"; Flags: recursesubdirs ignoreversion");
        lines.add ("Source: \"{#SourceDir}\\PlayerPlugins\\PatchCraft Player.vst3\\*\"; DestDir: \"" + vst3Dest + "\\PatchCraft Player.vst3\"; Flags: recursesubdirs ignoreversion skipifsourcedoesntexist");
        lines.add ("Source: \"{#SourceDir}\\PlayerPlugins\\PatchCraft Player FX.vst3\\*\"; DestDir: \"" + vst3Dest + "\\PatchCraft Player FX.vst3\"; Flags: recursesubdirs ignoreversion skipifsourcedoesntexist");
        if (manifest.whiteLabelIncludeVst3)
            lines.add ("Source: \"{#SourceDir}\\StandaloneVST3\\" + fileSafe + ".vst3\\*\"; DestDir: \"" + vst3Dest + "\\" + productName + ".vst3\"; Flags: recursesubdirs ignoreversion skipifsourcedoesntexist");
        if (manifest.whiteLabelIncludeStandalone)
            lines.add ("; Source: \"{#SourceDir}\\" + fileSafe + ".exe\"; DestDir: \"{app}\"; Flags: ignoreversion");
        lines.add ("Source: \"white-label-product.json\"; DestDir: \"{app}\"; Flags: ignoreversion");
        lines.add ("Source: \"license-activation.json\"; DestDir: \"{app}\"; Flags: ignoreversion skipifsourcedoesntexist");
        lines.add ("Source: \"artifact-manifest.json\"; DestDir: \"{app}\"; Flags: ignoreversion skipifsourcedoesntexist");
        lines.add ("");
        lines.add ("[Icons]");
        if (manifest.whiteLabelIncludeStandalone)
            lines.add ("; Name: \"{group}\\{#ProductName}\"; Filename: \"{app}\\" + fileSafe + ".exe\"");
        lines.add ("Name: \"{group}\\Support\"; Filename: \"" + (manifest.playerSupportUrl.isNotEmpty() ? manifest.playerSupportUrl : manifest.website) + "\"");
        lines.add ("");
        lines.add ("[UninstallDelete]");
        lines.add ("; Preserve buyer-created presets, imported samples, MIDI files, license cache, and user recordings.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildMacInstallerNotes() const
    {
        const auto& manifest = owner.getProject().getManifest();

        juce::StringArray lines;
        lines.add ("# macOS Installer Notes");
        lines.add ("");
        lines.add ("Product: " + whiteLabelProductName (manifest));
        lines.add ("Publisher: " + whiteLabelPublisher (manifest));
        lines.add ("Bundle ID: `" + whiteLabelBundleId (manifest) + "`");
        lines.add ("Target VST3 path: `" + (manifest.whiteLabelMacVst3Path.isNotEmpty() ? manifest.whiteLabelMacVst3Path : juce::String ("/Library/Audio/Plug-Ins/VST3")) + "`");
        lines.add ("");
        lines.add ("## Recommended Flow");
        lines.add ("");
        lines.add ("1. Stage PatchCraft Player/Player FX under `payload/Library/Audio/Plug-Ins/VST3/`.");
        lines.add ("2. Stage the generated pack under `payload/Library/Application Support/" + whiteLabelPublisher (manifest) + "/" + whiteLabelProductName (manifest) + "/Packs/`.");
        lines.add ("3. Add `white-label-product.json` under `payload/Library/Application Support/" + whiteLabelPublisher (manifest) + "/" + whiteLabelProductName (manifest) + "/`.");
        lines.add ("4. If this customer purchased a dedicated plugin, export and notarize the standalone VST3 with the VST Expansion addon and stage it under `payload/Library/Audio/Plug-Ins/VST3/" + safeSlug (whiteLabelProductName (manifest)) + ".vst3`.");
        lines.add ("5. Use `pkgbuild --root payload --identifier " + whiteLabelBundleId (manifest) + " --version " + manifest.version + " " + safeSlug (whiteLabelProductName (manifest)) + ".pkg`.");
        lines.add ("6. Wrap with `productbuild` when adding EULA, background image, signing, or distribution choices.");
        lines.add ("7. Notarize with Apple before public delivery.");
        lines.add ("");
        lines.add ("## QA");
        lines.add ("");
        lines.add ("- Fresh install on a clean macOS user account.");
        lines.add ("- DAW rescan confirms the branded plugin name and no PatchCraft template name leakage.");
        lines.add ("- Activation, support links, presets, tab switching, MIDI learn, and audio output work inside the DAW.");
        return lines.joinIntoString ("\n");
    }

    juce::String LaunchCenterPage::buildActivationFlowMarkdown() const
    {
        const auto& manifest = owner.getProject().getManifest();

        juce::StringArray lines;
        lines.add ("# Activation Flow");
        lines.add ("");
        lines.add ("Product: " + whiteLabelProductName (manifest));
        lines.add ("License required: " + juce::String (manifest.licenseRequired ? "yes" : "no"));
        lines.add ("Product ID: " + (manifest.licenseProductId.isNotEmpty() ? manifest.licenseProductId : "TODO"));
        lines.add ("License server: " + (manifest.licenseServerUrl.isNotEmpty() ? manifest.licenseServerUrl : "TODO"));
        lines.add ("Public key: " + juce::String (manifest.licensePublicKey.isNotEmpty() ? "configured" : "TODO"));
        lines.add ("Trial days: " + juce::String (manifest.trialDays));
        lines.add ("Offline grace days: " + juce::String (manifest.licenseOfflineGraceDays));
        lines.add ("Bind to machine: " + juce::String (manifest.licenseBindToMachine ? "yes" : "no"));
        lines.add ("Require license on first run: " + juce::String (manifest.whiteLabelRequireLicenseOnFirstRun ? "yes" : "no"));
        lines.add ("");
        lines.add ("## Buyer Experience");
        lines.add ("");
        if (manifest.licenseRequired)
        {
            lines.add ("1. Buyer installs the plugin and opens it in a DAW.");
            lines.add ("2. Player shows branded activation using the product name, client/publisher, support link, and product ID.");
            lines.add ("3. Buyer enters license key or signs in through the connected licensing endpoint.");
            lines.add ("4. Player stores a local activation token and respects offline grace.");
            lines.add ("5. If activation fails, the Player explains the exact problem and shows support/manual links.");
        }
        else
        {
            lines.add ("This product is configured as unlicensed/perpetual. The installer still includes product metadata so support, updates, and Plugin.club ownership can be attached later.");
        }
        lines.add ("");
        lines.add ("## Required Before Shipping");
        lines.add ("");
        lines.add ("- Verify the licensing endpoint and public key in Settings/Brand Lab.");
        lines.add ("- For launch, licensing is Plugin.club-backed. AudiLock should keep the same product ID, license URL, public key, trial, offline grace, and bind-to-machine fields when it replaces Plugin.club as the source of truth.");
        lines.add ("- Confirm the server returns deterministic product, entitlement, and expiration data.");
        lines.add ("- Test valid key, expired key, revoked key, offline grace, and no-network startup.");
        return lines.joinIntoString ("\n");
    }

    juce::var LaunchCenterPage::buildLaunchArtifactManifest (const juce::File& launchFolder) const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();

        juce::Array<juce::var> fileEntries;
        juce::int64 totalBytes = 0;

        juce::Array<juce::File> files;
        launchFolder.findChildFiles (files, juce::File::findFiles, true);
        for (const auto& file : files)
        {
            if (file.getFileName().equalsIgnoreCase ("artifact-manifest.json"))
                continue;

            auto* entry = new juce::DynamicObject();
            entry->setProperty ("path", relativePackagePath (launchFolder, file));
            entry->setProperty ("bytes", (double) file.getSize());
            entry->setProperty ("sha256", sha256ForFile (file));
            entry->setProperty ("modified", file.getLastModificationTime().toISO8601 (true));
            fileEntries.add (juce::var (entry));
            totalBytes += file.getSize();
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("schema", "patchcraft.launch_artifacts.v1");
        obj->setProperty ("generated_at", juce::Time::getCurrentTime().toISO8601 (true));
        obj->setProperty ("product", whiteLabelProductName (manifest));
        obj->setProperty ("publisher", whiteLabelPublisher (manifest));
        obj->setProperty ("engine", project.getEngineType());
        obj->setProperty ("installer_id", whiteLabelInstallerId (manifest));
        obj->setProperty ("file_count", fileEntries.size());
        obj->setProperty ("total_bytes", (double) totalBytes);
        obj->setProperty ("release_manifest", "release-manifest.json");
        obj->setProperty ("white_label_product_manifest", "installer/white-label-product.json");
        obj->setProperty ("activation_request_template", "installer/license-activation.json");
        obj->setProperty ("files", juce::var (fileEntries));
        return juce::var (obj);
    }

    juce::var LaunchCenterPage::buildWhiteLabelProductManifest() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();

        auto* product = new juce::DynamicObject();
        product->setProperty ("name", whiteLabelProductName (manifest));
        product->setProperty ("display_name", manifest.playerDisplayName.isNotEmpty() ? manifest.playerDisplayName : manifest.instrumentName);
        product->setProperty ("instrument_name", manifest.instrumentName);
        product->setProperty ("publisher", whiteLabelPublisher (manifest));
        product->setProperty ("client", manifest.playerClientName);
        product->setProperty ("creator", manifest.creator);
        product->setProperty ("version", manifest.version);
        product->setProperty ("category", manifest.category);
        product->setProperty ("engine", project.getEngineType());
        product->setProperty ("product_code", whiteLabelProductCode (manifest));
        product->setProperty ("bundle_identifier", whiteLabelBundleId (manifest));
        product->setProperty ("installer_id", whiteLabelInstallerId (manifest));

        auto* payload = new juce::DynamicObject();
        payload->setProperty ("include_patchcraft_player_runtime", true);
        payload->setProperty ("include_vst3", manifest.whiteLabelIncludeVst3);
        payload->setProperty ("include_standalone", manifest.whiteLabelIncludeStandalone);
        payload->setProperty ("player_vst3_bundle_name", "PatchCraft Player.vst3");
        payload->setProperty ("player_fx_vst3_bundle_name", "PatchCraft Player FX.vst3");
        payload->setProperty ("vst3_bundle_name", safeSlug (whiteLabelProductName (manifest)) + ".vst3");
        payload->setProperty ("pack_name", safeSlug (manifest.instrumentName) + ".patchcraft");
        payload->setProperty ("background_image", manifest.backgroundImage);
        payload->setProperty ("thumbnail_image", manifest.libraryThumbnail);
        payload->setProperty ("logo_image", manifest.playerLogoImage);
        payload->setProperty ("installer_icon", manifest.whiteLabelInstallerIcon);
        payload->setProperty ("eula_path", manifest.whiteLabelEulaPath);

        auto* install = new juce::DynamicObject();
        install->setProperty ("windows_vst3_path", manifest.whiteLabelWindowsVst3Path);
        install->setProperty ("mac_vst3_path", manifest.whiteLabelMacVst3Path);
        install->setProperty ("windows_user_data_path", "%APPDATA%\\" + whiteLabelPublisher (manifest) + "\\" + whiteLabelProductName (manifest));
        install->setProperty ("mac_user_data_path", "~/Library/Application Support/" + whiteLabelPublisher (manifest) + "/" + whiteLabelProductName (manifest));
        install->setProperty ("notes", manifest.whiteLabelInstallNotes);

        auto* licensing = new juce::DynamicObject();
        licensing->setProperty ("required", manifest.licenseRequired);
        licensing->setProperty ("require_on_first_run", manifest.whiteLabelRequireLicenseOnFirstRun);
        licensing->setProperty ("product_id", manifest.licenseProductId);
        licensing->setProperty ("instrument_id", LicenseValidator::hashInstrumentId (manifest.instrumentName,
                                                                                      manifest.creator));
        licensing->setProperty ("server_url", manifest.licenseServerUrl);
        licensing->setProperty ("public_key_configured", manifest.licensePublicKey.isNotEmpty());
        licensing->setProperty ("policy", manifest.licensePolicy);
        licensing->setProperty ("trial_days", manifest.trialDays);
        licensing->setProperty ("offline_grace_days", manifest.licenseOfflineGraceDays);
        licensing->setProperty ("bind_to_machine", manifest.licenseBindToMachine);
        licensing->setProperty ("allow_trial_conversion", manifest.licenseAllowTrialConversion);
        licensing->setProperty ("activation_template", "license-activation.json");

        auto* support = new juce::DynamicObject();
        support->setProperty ("email", manifest.playerSupportEmail);
        support->setProperty ("support_url", manifest.playerSupportUrl);
        support->setProperty ("manual_url", manifest.playerManualUrl);
        support->setProperty ("store_url", manifest.playerStoreUrl);
        support->setProperty ("privacy_url", manifest.whiteLabelPrivacyUrl);
        support->setProperty ("website", manifest.website);

        auto* qa = new juce::DynamicObject();
        qa->setProperty ("bound_control_count", countRuntimeControls (project, true));
        qa->setProperty ("preset_count", (int) project.getPresets().size());
        qa->setProperty ("patch_count", (int) project.getPatches().size());
        qa->setProperty ("launch_errors", errorCount);
        qa->setProperty ("launch_warnings", warningCount);
        qa->setProperty ("requires_manual_daw_proof", true);

        auto* root = new juce::DynamicObject();
        root->setProperty ("schema", "patchcraft.white_label_product.v1");
        root->setProperty ("generated_at", juce::Time::getCurrentTime().toISO8601 (true));
        root->setProperty ("product", juce::var (product));
        root->setProperty ("payload", juce::var (payload));
        root->setProperty ("install", juce::var (install));
        root->setProperty ("licensing", juce::var (licensing));
        root->setProperty ("support", juce::var (support));
        root->setProperty ("qa", juce::var (qa));
        return juce::var (root);
    }

    juce::var LaunchCenterPage::buildPluginClubMetadataPreview() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();

        auto* compatibility = new juce::DynamicObject();
        juce::StringArray operatingSystems;
        operatingSystems.add ("Windows");
        operatingSystems.add ("macOS");
        juce::StringArray daws;
        for (const auto& daw : { "FL Studio", "Ableton Live", "Logic Pro", "Cubase", "Studio One", "Reaper", "Bitwig" })
            daws.add (daw);
        compatibility->setProperty ("os", stringArrayToVar (operatingSystems));
        compatibility->setProperty ("daws", stringArrayToVar (daws));

        auto* license = new juce::DynamicObject();
        license->setProperty ("license_type", manifest.licenseRequired ? "licensed" : "perpetual");
        license->setProperty ("product_id", manifest.licenseProductId);
        license->setProperty ("trial_days", manifest.trialDays);
        license->setProperty ("offline_grace_days", manifest.licenseOfflineGraceDays);
        license->setProperty ("bind_to_machine", manifest.licenseBindToMachine);

        auto* patchcraft = new juce::DynamicObject();
        patchcraft->setProperty ("engine", project.getEngineType());
        patchcraft->setProperty ("preset_count", (int) project.getPresets().size());
        patchcraft->setProperty ("patch_count", (int) project.getPatches().size());
        patchcraft->setProperty ("sample_zone_count", (int) project.getSampleMap().getZones().size());
        patchcraft->setProperty ("bound_control_count", countRuntimeControls (project, true));
        patchcraft->setProperty ("launch_errors", errorCount);
        patchcraft->setProperty ("launch_warnings", warningCount);

        auto* sales = new juce::DynamicObject();
        sales->setProperty ("headline", salesHeadline (manifest));
        sales->setProperty ("subheadline", salesSubheadline (manifest));
        sales->setProperty ("cta", manifest.salesCtaText);
        sales->setProperty ("checkout_url", manifest.salesCheckoutUrl);
        sales->setProperty ("audio_demo_url", manifest.salesAudioDemoUrl);
        sales->setProperty ("video_demo_url", manifest.salesDemoVideoUrl);
        sales->setProperty ("currency", manifest.salesCurrency);
        sales->setProperty ("price", manifest.salesPrice);
        sales->setProperty ("compare_at_price", manifest.salesCompareAtPrice);
        sales->setProperty ("highlights", stringArrayToVar (manifest.salesHighlights));
        sales->setProperty ("includes", stringArrayToVar (manifest.salesIncludes));

        auto* whiteLabel = new juce::DynamicObject();
        whiteLabel->setProperty ("product_name", whiteLabelProductName (manifest));
        whiteLabel->setProperty ("publisher", whiteLabelPublisher (manifest));
        whiteLabel->setProperty ("product_code", whiteLabelProductCode (manifest));
        whiteLabel->setProperty ("bundle_identifier", whiteLabelBundleId (manifest));
        whiteLabel->setProperty ("installer_manifest", "installer/white-label-product.json");
        whiteLabel->setProperty ("windows_installer_template", "installer/windows-inno-setup.iss");
        whiteLabel->setProperty ("macos_installer_notes", "installer/macos-pkgbuild-notes.md");

        auto* metadata = new juce::DynamicObject();
        metadata->setProperty ("title", manifest.instrumentName);
        metadata->setProperty ("creator", manifest.creator);
        metadata->setProperty ("product_type", project.getEngineType().equalsIgnoreCase ("fx") ? "plugin" : "instrument");
        metadata->setProperty ("plugin_type", project.getEngineType());
        metadata->setProperty ("category", manifest.category);
        metadata->setProperty ("version", manifest.version);
        metadata->setProperty ("status", "draft");
        metadata->setProperty ("price", manifest.salesPrice);
        metadata->setProperty ("currency", manifest.salesCurrency);
        metadata->setProperty ("checkout_url", manifest.salesCheckoutUrl);
        metadata->setProperty ("short_description", manifest.description.substring (0, 160));
        metadata->setProperty ("description", manifest.description);
        metadata->setProperty ("tags", stringArrayToVar (tagsFromManifest (manifest)));
        metadata->setProperty ("compatibility", juce::var (compatibility));
        metadata->setProperty ("license_config", juce::var (license));
        metadata->setProperty ("patchcraft", juce::var (patchcraft));
        metadata->setProperty ("sales_page", juce::var (sales));
        metadata->setProperty ("white_label", juce::var (whiteLabel));
        return juce::var (metadata);
    }

    juce::var LaunchCenterPage::buildReleaseManifest() const
    {
        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto appDir = runtimeFolder();
        const auto missing = missingRuntimeDistributionItems();

        juce::Array<juce::var> missingItems;
        for (const auto& item : missing)
            missingItems.add (item);

        juce::Array<juce::var> manualQa;
        for (const auto& item : { "Studio preview audio",
                                  "Hardware MIDI note input",
                                  "Mod wheel / expression / sustain",
                                  "Player tab switching and text labels",
                                  "Pack export and Player load",
                                  "PatchCraft Player/Player FX FL Studio scan",
                                  "Standalone VST3 export scan when VST Expansion is installed",
                                  "Plugin.club seller draft publish" })
            manualQa.add (juce::String (item));

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("product", manifest.instrumentName);
        obj->setProperty ("creator", manifest.creator);
        obj->setProperty ("engine", project.getEngineType());
        obj->setProperty ("generated_at", juce::Time::getCurrentTime().toISO8601 (true));
        obj->setProperty ("launch_errors", errorCount);
        obj->setProperty ("launch_warnings", warningCount);
        obj->setProperty ("factory_demo_count", countRuntimeFactoryDemos());
        obj->setProperty ("runtime_folder", appDir.getFullPathName());
        obj->setProperty ("pluginclub_endpoint", "https://plugin.club/functions/sellerImport");
        obj->setProperty ("installer_id", whiteLabelInstallerId (manifest));
        obj->setProperty ("installer_manifest", "installer/white-label-product.json");
        obj->setProperty ("artifact_manifest", "artifact-manifest.json");
        obj->setProperty ("license_activation_template", "installer/license-activation.json");
        obj->setProperty ("white_label_product", buildWhiteLabelProductManifest());
        obj->setProperty ("missing_distribution_items", juce::var (missingItems));
        obj->setProperty ("manual_qa_required", juce::var (manualQa));
        obj->setProperty ("rc_gate_status", errorCount == 0 && missing.isEmpty() ? "pass_pending_manual_daw_proof" : "blocked");
        return juce::var (obj);
    }

    void LaunchCenterPage::createLaunchBundle()
    {
        refresh();

        juce::String error;
        const auto folder = createLaunchFolder (error);
        if (error.isNotEmpty())
        {
            showMessage ("Create Launch Bundle", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto installerFolder = folder.getChildFile ("installer");
        const auto installerCreated = installerFolder.createDirectory();
        if (installerCreated.failed())
        {
            showMessage ("Create Launch Bundle", "Could not create installer folder: " + installerCreated.getErrorMessage(), juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto payloadFolder = folder.getChildFile ("payload");
        payloadFolder.createDirectory();
        const auto packsFolder = payloadFolder.getChildFile ("Packs");
        packsFolder.createDirectory();
        const auto playerPayloadFolder = payloadFolder.getChildFile ("PlayerPlugins");
        const auto appPlayerPlugins = runtimeFolder().getChildFile ("PlayerPlugins");
        if (playerPayloadFolder.exists())
            playerPayloadFolder.deleteRecursively();
        if (appPlayerPlugins.isDirectory())
            appPlayerPlugins.copyDirectoryTo (playerPayloadFolder);

        const auto standalonePlaceholder = payloadFolder.getChildFile ("StandaloneVST3");
        standalonePlaceholder.createDirectory();

        const auto& project = owner.getProject();
        const auto& manifest = project.getManifest();
        const auto packPayloadFolder = packsFolder.getChildFile (safeSlug (manifest.instrumentName) + ".patchcraft");
        if (packPayloadFolder.exists())
            packPayloadFolder.deleteRecursively();

        PatchCraftPackWriter writer;
        if (! writer.write (project, packPayloadFolder, error))
        {
            showMessage ("Create Launch Bundle", "Could not export launch pack payload: " + error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        LicenseValidator::LicenseInfo licenseInfo;
        licenseInfo.licenseKey = manifest.licenseKey;
        licenseInfo.instrumentName = manifest.instrumentName;
        licenseInfo.creator = manifest.creator;
        licenseInfo.instrumentId = LicenseValidator::hashInstrumentId (manifest.instrumentName,
                                                                       manifest.creator);
        licenseInfo.productId = manifest.licenseProductId;
        licenseInfo.licenseServerUrl = manifest.licenseServerUrl;
        licenseInfo.policy = manifest.licensePolicy;
        licenseInfo.trialDays = manifest.trialDays;
        licenseInfo.isTrial = manifest.isTrial || manifest.trialDays > 0;
        licenseInfo.expiryDate = manifest.trialExpiryDate;
        licenseInfo.offlineGraceDays = manifest.licenseOfflineGraceDays;
        licenseInfo.bindToMachine = manifest.licenseBindToMachine;
        const auto activationTemplate = LicenseValidator::buildActivationRequest (
            licenseInfo, "RUNTIME_MACHINE_ID");

        if (! writeTextFile (folder.getChildFile ("launch-readiness.md"), buildReadinessMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("product-page.md"), buildProductPageMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("sales-page.md"), buildSalesPageMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("sales-page.html"), buildSalesPageHtml(), error)
            || ! writeTextFile (folder.getChildFile ("runtime-test-plan.md"), buildTestPlanMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("installer-checklist.md"), buildInstallerChecklistMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("client-delivery.md"), buildClientDeliveryMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("customer-package-wizard.md"), buildCustomerPackageWizardMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("seller-launch-playbook.md"), buildSellerLaunchPlaybookMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("buyer-quick-start.md"), buildBuyerQuickStartMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("marketplace-asset-checklist.md"), buildMarketplaceAssetChecklistMarkdown(), error)
            || ! writeTextFile (installerFolder.getChildFile ("white-label-product.json"),
                                juce::JSON::toString (buildWhiteLabelProductManifest(), true), error)
            || ! writeTextFile (installerFolder.getChildFile ("license-activation.json"),
                                juce::JSON::toString (activationTemplate, true), error)
            || ! writeTextFile (installerFolder.getChildFile ("windows-inno-setup.iss"), buildWindowsInstallerScript(), error)
            || ! writeTextFile (installerFolder.getChildFile ("macos-pkgbuild-notes.md"), buildMacInstallerNotes(), error)
            || ! writeTextFile (installerFolder.getChildFile ("activation-flow.md"), buildActivationFlowMarkdown(), error)
            || ! writeTextFile (installerFolder.getChildFile ("installer-readme.md"), buildInstallerReadmeMarkdown(), error)
            || ! writeTextFile (folder.getChildFile ("pluginclub-metadata-preview.json"),
                                juce::JSON::toString (buildPluginClubMetadataPreview(), true), error)
            || ! writeTextFile (folder.getChildFile ("release-manifest.json"),
                                juce::JSON::toString (buildReleaseManifest(), true), error))
        {
            showMessage ("Create Launch Bundle", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto artworkFolder = folder.getChildFile ("artwork");
        artworkFolder.createDirectory();
        auto copyAsset = [&] (const juce::String& path, const juce::String& stem)
        {
            const auto src = resolveAssetPath (project, path);
            if (src.existsAsFile())
                src.copyFileTo (artworkFolder.getChildFile (stem + src.getFileExtension()));
        };
        copyAsset (manifest.backgroundImage, "background");
        copyAsset (manifest.libraryThumbnail, "thumbnail");
        copyAsset (manifest.playerLogoImage, "logo");
        copyAsset (manifest.playerTitleBannerImage, "title-banner");

        const auto artifactManifestText = juce::JSON::toString (buildLaunchArtifactManifest (folder), true);
        if (! writeTextFile (folder.getChildFile ("artifact-manifest.json"), artifactManifestText, error)
            || ! writeTextFile (installerFolder.getChildFile ("artifact-manifest.json"), artifactManifestText, error))
        {
            showMessage ("Create Launch Bundle", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        showMessage ("Launch Bundle Created",
                     "Created launch materials:\n" + folder.getFullPathName()
                        + "\n\nIncludes readiness report, sales page Markdown/HTML, product copy, runtime test plan, installer checklist, customer package wizard, seller launch playbook, buyer quick-start, marketplace asset checklist, client delivery guide, installer templates, activation flow, release manifest, artifact checksums, Plugin.club metadata preview, and resolved artwork.",
                     juce::MessageBoxIconType::InfoIcon);
    }

    void LaunchCenterPage::showCustomerPackageWizard()
    {
        struct CustomerPackageWizard final : public juce::Component
        {
            CustomerPackageWizard (juce::String guideIn,
                                   juce::String manifestIn,
                                   juce::String outputFolderIn,
                                   std::function<void()> createBundleIn,
                                   std::function<void()> openFolderIn)
                : guideText (std::move (guideIn)),
                  manifestText (std::move (manifestIn)),
                  outputFolderText (std::move (outputFolderIn)),
                  createBundle (std::move (createBundleIn)),
                  openFolder (std::move (openFolderIn))
            {
                setSize (1120, 760);

                title.setText ("White-Label Customer Package Wizard", juce::dontSendNotification);
                title.setFont (juce::Font (24.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                help.setText ("Use this as the customer handoff path: product identity, installer payload, Plugin.club licensing, first-run behavior, DAW QA, and publish prep. AudiLock becomes the licensing authority later.",
                              juce::dontSendNotification);
                help.setFont (juce::Font (12.0f));
                help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (help);

                output.setText ("Output folder: " + outputFolderText, juce::dontSendNotification);
                output.setFont (juce::Font (11.0f));
                output.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                output.setTooltip (outputFolderText);
                addAndMakeVisible (output);

                setupEditor (guide, guideText);
                setupEditor (manifest, manifestText);
                addAndMakeVisible (guide);
                addAndMakeVisible (manifest);

                copyButton.setTooltip ("Copy the white-label customer package guide to the clipboard.");
                createButton.setTooltip ("Create the full launch bundle with pack payload, installer files, customer guide, sales page, and QA docs.");
                openFolderButton.setTooltip ("Open the selected Launch output folder.");
                closeButton.setTooltip ("Close the customer package wizard.");

                copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (guideText); };
                createButton.onClick = [this]
                {
                    if (createBundle)
                        createBundle();
                };
                openFolderButton.onClick = [this]
                {
                    if (openFolder)
                        openFolder();
                };
                closeButton.onClick = [this]
                {
                    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                        window->exitModalState (0);
                };

                for (auto* button : { &copyButton, &createButton, &openFolderButton, &closeButton })
                {
                    button->getProperties().set ("smallButton", true);
                    addAndMakeVisible (*button);
                }
                createButton.getProperties().set ("primaryAction", true);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto area = getLocalBounds().toFloat().reduced (12.0f);
                juce::ColourGradient grad (juce::Colour (0xff101823), area.getX(), area.getY(),
                                           juce::Colour (0xff07090d), area.getRight(), area.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (area, 16.0f);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawRoundedRectangle (area, 16.0f, 1.5f);

                auto cards = getLocalBounds().reduced (28, 24);
                cards.removeFromTop (92);
                drawStepCards (g, cards.removeFromTop (176));
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (28, 24);
                title.setBounds (area.removeFromTop (32));
                help.setBounds (area.removeFromTop (24));
                output.setBounds (area.removeFromTop (22));
                area.removeFromTop (14);
                area.removeFromTop (176);
                area.removeFromTop (12);

                auto buttons = area.removeFromBottom (42);
                closeButton.setBounds (buttons.removeFromRight (92));
                buttons.removeFromRight (8);
                openFolderButton.setBounds (buttons.removeFromRight (136));
                buttons.removeFromRight (8);
                createButton.setBounds (buttons.removeFromRight (164));
                buttons.removeFromRight (8);
                copyButton.setBounds (buttons.removeFromRight (124));

                area.removeFromBottom (10);
                auto left = area.removeFromLeft (area.getWidth() / 2);
                left.removeFromRight (8);
                auto right = area;
                right.removeFromLeft (8);
                guide.setBounds (left);
                manifest.setBounds (right);
            }

            static void setupEditor (juce::TextEditor& editor, const juce::String& text)
            {
                editor.setMultiLine (true);
                editor.setReadOnly (true);
                editor.setScrollbarsShown (true);
                editor.setCaretVisible (false);
                editor.setText (text, false);
                editor.setFont (juce::Font (12.8f));
                editor.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::panel().withAlpha (0.94f));
                editor.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
                editor.setColour (juce::TextEditor::focusedOutlineColourId, PatchCraftLookAndFeel::accent());
                editor.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
            }

            void drawStepCards (juce::Graphics& g, juce::Rectangle<int> area)
            {
                const juce::StringArray stepTitles {
                    "1  Identity",
                    "2  Installer Payload",
                    "3  Licensing",
                    "4  Buyer First Run",
                    "5  DAW QA",
                    "6  Publish"
                };
                const juce::StringArray details {
                    "Product name, publisher, client, support, manual, EULA, artwork.",
                    "Pack, Player VST3, Player FX, optional standalone VST3, installer manifest.",
                    "Plugin.club License URL now; AudiLock-ready product ID, public key, trial, offline grace, activation UX.",
                    "Install, rescan DAW, open Player, activate, load default preset, play.",
                    "Tabs, controls, hardware MIDI, pads, drum grids, presets, resize, audio.",
                    "Sales page, Plugin.club metadata, package archive, customer handoff."
                };

                auto row = area.removeFromTop ((area.getHeight() - 10) / 2);
                auto row2 = area.removeFromBottom (row.getHeight());
                for (int i = 0; i < stepTitles.size(); ++i)
                {
                    auto& currentRow = i < 3 ? row : row2;
                    auto card = currentRow.removeFromLeft ((currentRow.getWidth() - (2 - (i % 3)) * 10) / (3 - (i % 3)));
                    if ((i % 3) != 2)
                        currentRow.removeFromLeft (10);

                    const auto r = card.toFloat();
                    const auto accent = i == 2 ? juce::Colour (0xff64d88a)
                                      : i == 5 ? juce::Colour (0xff58b7ff)
                                               : PatchCraftLookAndFeel::accent();
                    g.setColour (PatchCraftLookAndFeel::panelAlt().withAlpha (0.92f));
                    g.fillRoundedRectangle (r, 10.0f);
                    g.setColour (accent.withAlpha (0.86f));
                    g.drawRoundedRectangle (r, 10.0f, 1.2f);
                    g.fillRoundedRectangle (r.withHeight (3.0f), 2.0f);

                    auto text = card.reduced (14, 10);
                    g.setColour (PatchCraftLookAndFeel::textBright());
                    g.setFont (juce::Font (13.2f, juce::Font::bold));
                    g.drawFittedText (stepTitles[i], text.removeFromTop (22), juce::Justification::centredLeft, 1);
                    g.setColour (PatchCraftLookAndFeel::textDim());
                    g.setFont (juce::Font (11.7f));
                    g.drawFittedText (details[i], text, juce::Justification::topLeft, 3);
                }
            }

            juce::String guideText;
            juce::String manifestText;
            juce::String outputFolderText;
            std::function<void()> createBundle;
            std::function<void()> openFolder;
            juce::Label title, help, output;
            juce::TextEditor guide, manifest;
            juce::TextButton copyButton { "Copy Guide" };
            juce::TextButton createButton { "Create Bundle" };
            juce::TextButton openFolderButton { "Open Folder" };
            juce::TextButton closeButton { "Close" };
        };

        auto* content = new CustomerPackageWizard (
            buildCustomerPackageWizardMarkdown(),
            juce::JSON::toString (buildWhiteLabelProductManifest(), true),
            launchRoot().getFullPathName(),
            [this] { createLaunchBundle(); },
            [this] { launchRoot().revealToUser(); });

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "PatchCraft Customer Package Wizard";
        options.content.setOwned (content);
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();

        if (auto* window = content->findParentComponentOfClass<juce::DialogWindow>())
        {
            window->setResizeLimits (900, 620, 1600, 1100);
            window->centreWithSize (1120, 760);
        }
    }

    void LaunchCenterPage::showProductPagePreview()
    {
        struct ProductPagePreview final : public juce::Component
        {
            ProductPagePreview (juce::String markdownIn,
                                juce::String metadataIn,
                                juce::String outputFolderIn,
                                std::function<void()> exportFnIn)
                : markdownText (std::move (markdownIn)),
                  metadataText (std::move (metadataIn)),
                  outputFolderText (std::move (outputFolderIn)),
                  exportFn (std::move (exportFnIn))
            {
                title.setText ("Product Page Preview", juce::dontSendNotification);
                title.setFont (juce::Font (24.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                help.setText ("Review marketplace copy before export. The Product Page button no longer writes files immediately.",
                              juce::dontSendNotification);
                help.setFont (juce::Font (12.0f));
                help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (help);

                output.setText ("Output folder: " + outputFolderText, juce::dontSendNotification);
                output.setFont (juce::Font (11.0f));
                output.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
                output.setTooltip (outputFolderText);
                addAndMakeVisible (output);

                setupEditor (markdown, markdownText);
                setupEditor (metadata, metadataText);
                addAndMakeVisible (markdown);
                addAndMakeVisible (metadata);

                copyButton.setTooltip ("Copy the product page Markdown to the clipboard.");
                exportButton.setTooltip ("Write product-page.md into the selected Launch output folder.");
                closeButton.setTooltip ("Close this preview.");
                copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (markdownText); };
                exportButton.onClick = [this]
                {
                    if (exportFn)
                        exportFn();
                };
                closeButton.onClick = [this]
                {
                    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                        window->exitModalState (0);
                };

                for (auto* button : { &copyButton, &exportButton, &closeButton })
                {
                    button->getProperties().set ("smallButton", true);
                    addAndMakeVisible (*button);
                }

                exportButton.getProperties().set ("primaryAction", true);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (PatchCraftLookAndFeel::bg());
                auto area = getLocalBounds().toFloat().reduced (12.0f);
                juce::ColourGradient grad (juce::Colour (0xff101722), area.getX(), area.getY(),
                                           juce::Colour (0xff080b10), area.getRight(), area.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (area, 14.0f);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawRoundedRectangle (area, 14.0f, 1.5f);
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (28, 24);
                title.setBounds (area.removeFromTop (32));
                help.setBounds (area.removeFromTop (24));
                output.setBounds (area.removeFromTop (24));
                area.removeFromTop (10);

                auto buttons = area.removeFromBottom (42);
                closeButton.setBounds (buttons.removeFromRight (96));
                buttons.removeFromRight (8);
                exportButton.setBounds (buttons.removeFromRight (150));
                buttons.removeFromRight (8);
                copyButton.setBounds (buttons.removeFromRight (150));

                area.removeFromBottom (10);
                auto left = area.removeFromLeft (area.getWidth() / 2);
                left.removeFromRight (8);
                auto right = area;
                right.removeFromLeft (8);
                markdown.setBounds (left);
                metadata.setBounds (right);
            }

            static void setupEditor (juce::TextEditor& editor, const juce::String& text)
            {
                editor.setMultiLine (true);
                editor.setReadOnly (true);
                editor.setScrollbarsShown (true);
                editor.setCaretVisible (false);
                editor.setText (text, false);
                editor.setFont (juce::Font (13.0f));
                editor.setColour (juce::TextEditor::backgroundColourId, PatchCraftLookAndFeel::panel());
                editor.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
                editor.setColour (juce::TextEditor::focusedOutlineColourId, PatchCraftLookAndFeel::accent());
                editor.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
            }

            juce::String markdownText;
            juce::String metadataText;
            juce::String outputFolderText;
            std::function<void()> exportFn;
            juce::Label title, help, output;
            juce::TextEditor markdown, metadata;
            juce::TextButton copyButton { "Copy Markdown" };
            juce::TextButton exportButton { "Export Page" };
            juce::TextButton closeButton { "Close" };
        };

        auto* content = new ProductPagePreview (
            buildProductPageMarkdown(),
            juce::JSON::toString (buildPluginClubMetadataPreview(), true),
            launchRoot().getFullPathName(),
            [this] { writeProductPageOnly(); });

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "PatchCraft Product Page Preview";
        options.content.setOwned (content);
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();

        if (auto* window = content->findParentComponentOfClass<juce::DialogWindow>())
        {
            window->setResizeLimits (780, 520, 1400, 1000);
            window->centreWithSize (1040, 720);
        }
    }

    void LaunchCenterPage::showSalesPageBuilder()
    {
        struct SalesPageValues
        {
            juce::String headline;
            juce::String subheadline;
            juce::String cta;
            juce::String checkoutUrl;
            juce::String audioDemoUrl;
            juce::String videoDemoUrl;
            juce::String currency;
            double price = 0.0;
            double compareAtPrice = 0.0;
            juce::StringArray highlights;
            juce::StringArray includes;
            juce::StringArray faq;
        };

        struct SalesPageBuilder final : public juce::Component
        {
            SalesPageBuilder (const Manifest& manifest,
                              std::function<void (const SalesPageValues&, int)> actionIn)
                : action (std::move (actionIn))
            {
                setSize (980, 760);
                title.setText ("Sales Page Builder", juce::dontSendNotification);
                title.setFont (juce::Font (24.0f, juce::Font::bold));
                title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textBright());
                addAndMakeVisible (title);

                help.setText ("Create the buyer-facing offer: headline, price, CTA, demo links, bullets, included items, FAQ, and exported sales-page HTML.",
                              juce::dontSendNotification);
                help.setFont (juce::Font (12.0f));
                help.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                addAndMakeVisible (help);

                auto setupSingle = [this] (juce::Label& label, juce::TextEditor& editor, const juce::String& labelText, const juce::String& value, const juce::String& placeholder)
                {
                    label.setText (labelText, juce::dontSendNotification);
                    label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                    label.setFont (juce::Font (11.5f, juce::Font::bold));
                    editor.setMultiLine (false);
                    editor.setText (value, false);
                    editor.setTextToShowWhenEmpty (placeholder, PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
                    addAndMakeVisible (label);
                    addAndMakeVisible (editor);
                };

                auto setupMulti = [this] (juce::Label& label, juce::TextEditor& editor, const juce::String& labelText, const juce::String& value, const juce::String& placeholder)
                {
                    label.setText (labelText, juce::dontSendNotification);
                    label.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
                    label.setFont (juce::Font (11.5f, juce::Font::bold));
                    editor.setMultiLine (true);
                    editor.setScrollbarsShown (true);
                    editor.setText (value, false);
                    editor.setTextToShowWhenEmpty (placeholder, PatchCraftLookAndFeel::textDim().withAlpha (0.65f));
                    addAndMakeVisible (label);
                    addAndMakeVisible (editor);
                };

                setupSingle (headlineLabel, headlineEdit, "Headline", manifest.salesHeadline, "A powerful promise for the buyer");
                setupSingle (subheadlineLabel, subheadlineEdit, "Subheadline", manifest.salesSubheadline, "What this instrument does and why it matters");
                setupSingle (priceLabel, priceEdit, "Price", manifest.salesPrice > 0.0 ? juce::String (manifest.salesPrice, 2) : juce::String(), "49.00");
                setupSingle (compareLabel, compareEdit, "Compare At", manifest.salesCompareAtPrice > 0.0 ? juce::String (manifest.salesCompareAtPrice, 2) : juce::String(), "99.00");
                setupSingle (currencyLabel, currencyEdit, "Currency", manifest.salesCurrency, "USD");
                setupSingle (ctaLabel, ctaEdit, "CTA", manifest.salesCtaText, "Buy Now");
                setupSingle (checkoutLabel, checkoutEdit, "Checkout URL", manifest.salesCheckoutUrl, "https://plugin.club/...");
                setupSingle (audioLabel, audioEdit, "Audio Demo URL", manifest.salesAudioDemoUrl, "https://...");
                setupSingle (videoLabel, videoEdit, "Video Demo URL", manifest.salesDemoVideoUrl, "https://...");
                setupMulti (highlightsLabel, highlightsEdit, "Highlights", joinLinesClean (manifest.salesHighlights), "One sales bullet per line");
                setupMulti (includesLabel, includesEdit, "Includes", joinLinesClean (manifest.salesIncludes), "One included deliverable per line");
                setupMulti (faqLabel, faqEdit, "FAQ", joinLinesClean (manifest.salesFaq), "Question|Answer, one per line");

                for (auto* button : { &saveButton, &copyMarkdownButton, &exportButton, &closeButton })
                {
                    button->getProperties().set ("smallButton", true);
                    addAndMakeVisible (*button);
                }
                saveButton.getProperties().set ("primaryAction", true);
                exportButton.getProperties().set ("primaryAction", true);

                saveButton.onClick = [this] { runAction (1); };
                copyMarkdownButton.onClick = [this] { runAction (2); };
                exportButton.onClick = [this] { runAction (3); };
                closeButton.onClick = [this]
                {
                    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                        window->exitModalState (0);
                };
            }

            void paint (juce::Graphics& graphics) override
            {
                graphics.fillAll (PatchCraftLookAndFeel::bg());
                auto area = getLocalBounds().toFloat().reduced (12.0f);
                graphics.setColour (PatchCraftLookAndFeel::panel());
                graphics.fillRoundedRectangle (area, 16.0f);
                graphics.setColour (PatchCraftLookAndFeel::accent());
                graphics.drawRoundedRectangle (area, 16.0f, 1.4f);
            }

            void resized() override
            {
                auto area = getLocalBounds().reduced (28, 24);
                title.setBounds (area.removeFromTop (32));
                help.setBounds (area.removeFromTop (24));
                area.removeFromTop (12);

                auto buttons = area.removeFromBottom (40);
                closeButton.setBounds (buttons.removeFromRight (88));
                buttons.removeFromRight (8);
                exportButton.setBounds (buttons.removeFromRight (142));
                buttons.removeFromRight (8);
                copyMarkdownButton.setBounds (buttons.removeFromRight (142));
                buttons.removeFromRight (8);
                saveButton.setBounds (buttons.removeFromRight (98));
                area.removeFromBottom (12);

                auto left = area.removeFromLeft (area.getWidth() / 2);
                left.removeFromRight (10);
                auto right = area;
                right.removeFromLeft (10);

                auto row = [] (juce::Rectangle<int>& source, int height)
                {
                    auto out = source.removeFromTop (height);
                    source.removeFromTop (7);
                    return out;
                };
                auto place = [] (juce::Rectangle<int> r, juce::Label& label, juce::Component& field)
                {
                    label.setBounds (r.removeFromTop (18));
                    field.setBounds (r);
                };

                place (row (left, 64), headlineLabel, headlineEdit);
                place (row (left, 76), subheadlineLabel, subheadlineEdit);
                auto priceRow = row (left, 64);
                auto priceA = priceRow.removeFromLeft (priceRow.getWidth() / 3);
                auto priceB = priceRow.removeFromLeft (priceRow.getWidth() / 2);
                place (priceA.reduced (0, 0), priceLabel, priceEdit);
                place (priceB.reduced (6, 0), compareLabel, compareEdit);
                place (priceRow.reduced (6, 0), currencyLabel, currencyEdit);
                place (row (left, 64), ctaLabel, ctaEdit);
                place (row (left, 64), checkoutLabel, checkoutEdit);
                place (row (left, 64), audioLabel, audioEdit);
                place (row (left, 64), videoLabel, videoEdit);

                place (row (right, 132), highlightsLabel, highlightsEdit);
                place (row (right, 132), includesLabel, includesEdit);
                place (row (right, 172), faqLabel, faqEdit);
            }

            SalesPageValues values() const
            {
                SalesPageValues v;
                v.headline = headlineEdit.getText().trim();
                v.subheadline = subheadlineEdit.getText().trim();
                v.price = priceEdit.getText().getDoubleValue();
                v.compareAtPrice = compareEdit.getText().getDoubleValue();
                v.currency = currencyEdit.getText().trim();
                v.cta = ctaEdit.getText().trim();
                v.checkoutUrl = checkoutEdit.getText().trim();
                v.audioDemoUrl = audioEdit.getText().trim();
                v.videoDemoUrl = videoEdit.getText().trim();
                v.highlights = splitLinesClean (highlightsEdit.getText());
                v.includes = splitLinesClean (includesEdit.getText());
                v.faq = splitLinesClean (faqEdit.getText());
                return v;
            }

            void runAction (int actionId)
            {
                if (action)
                    action (values(), actionId);
            }

            std::function<void (const SalesPageValues&, int)> action;
            juce::Label title, help;
            juce::Label headlineLabel, subheadlineLabel, priceLabel, compareLabel, currencyLabel, ctaLabel;
            juce::Label checkoutLabel, audioLabel, videoLabel, highlightsLabel, includesLabel, faqLabel;
            juce::TextEditor headlineEdit, subheadlineEdit, priceEdit, compareEdit, currencyEdit, ctaEdit;
            juce::TextEditor checkoutEdit, audioEdit, videoEdit, highlightsEdit, includesEdit, faqEdit;
            juce::TextButton saveButton { "Save" };
            juce::TextButton copyMarkdownButton { "Copy MD" };
            juce::TextButton exportButton { "Export Page" };
            juce::TextButton closeButton { "Close" };
        };

        auto saveValues = [this] (const SalesPageValues& values, int actionId)
        {
            auto& manifest = owner.getProject().getManifest();
            manifest.salesHeadline = values.headline;
            manifest.salesSubheadline = values.subheadline;
            manifest.salesPrice = juce::jmax (0.0, values.price);
            manifest.salesCompareAtPrice = juce::jmax (0.0, values.compareAtPrice);
            manifest.salesCurrency = values.currency.isNotEmpty() ? values.currency : juce::String ("USD");
            manifest.salesCtaText = values.cta.isNotEmpty() ? values.cta : juce::String ("Buy Now");
            manifest.salesCheckoutUrl = values.checkoutUrl;
            manifest.salesAudioDemoUrl = values.audioDemoUrl;
            manifest.salesDemoVideoUrl = values.videoDemoUrl;
            manifest.salesHighlights = values.highlights;
            manifest.salesIncludes = values.includes;
            manifest.salesFaq = values.faq;
            if (manifest.playerStoreUrl.isEmpty() && manifest.salesCheckoutUrl.isNotEmpty())
                manifest.playerStoreUrl = manifest.salesCheckoutUrl;
            owner.getProject().notifyChanged();
            refresh();

            if (actionId == 1)
            {
                showMessage ("Sales Page", "Sales page settings saved.", juce::MessageBoxIconType::InfoIcon);
            }
            else if (actionId == 2)
            {
                juce::SystemClipboard::copyTextToClipboard (buildSalesPageMarkdown());
                showMessage ("Sales Page", "Sales page Markdown copied to clipboard.", juce::MessageBoxIconType::InfoIcon);
            }
            else if (actionId == 3)
            {
                writeSalesPageOnly();
            }
        };

        auto* content = new SalesPageBuilder (owner.getProject().getManifest(), saveValues);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "PatchCraft Sales Page Builder";
        options.content.setOwned (content);
        options.dialogBackgroundColour = PatchCraftLookAndFeel::bg();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();

        if (auto* window = content->findParentComponentOfClass<juce::DialogWindow>())
        {
            window->setResizeLimits (820, 620, 1400, 1000);
            window->centreWithSize (980, 760);
        }
    }

    void LaunchCenterPage::writeProductPageOnly()
    {
        juce::String error;
        const auto folder = createLaunchFolder (error);
        if (error.isNotEmpty())
        {
            showMessage ("Product Page", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto file = folder.getChildFile ("product-page.md");
        if (! writeTextFile (file, buildProductPageMarkdown(), error))
        {
            showMessage ("Product Page", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        juce::SystemClipboard::copyTextToClipboard (buildProductPageMarkdown());
        showMessage ("Product Page Created",
                     "Wrote and copied product page copy:\n" + file.getFullPathName(),
                     juce::MessageBoxIconType::InfoIcon);
    }

    void LaunchCenterPage::writeSalesPageOnly()
    {
        juce::String error;
        const auto folder = createLaunchFolder (error);
        if (error.isNotEmpty())
        {
            showMessage ("Sales Page", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        const auto markdown = folder.getChildFile ("sales-page.md");
        const auto html = folder.getChildFile ("sales-page.html");
        if (! writeTextFile (markdown, buildSalesPageMarkdown(), error)
            || ! writeTextFile (html, buildSalesPageHtml(), error)
            || ! writeTextFile (folder.getChildFile ("pluginclub-metadata-preview.json"),
                                juce::JSON::toString (buildPluginClubMetadataPreview(), true), error))
        {
            showMessage ("Sales Page", error, juce::MessageBoxIconType::WarningIcon);
            return;
        }

        juce::SystemClipboard::copyTextToClipboard (buildSalesPageMarkdown());
        showMessage ("Sales Page Created",
                     "Wrote sales page files and copied Markdown:\n" + folder.getFullPathName(),
                     juce::MessageBoxIconType::InfoIcon);
    }

    void LaunchCenterPage::showMessage (const juce::String& titleText,
                                        const juce::String& message,
                                        juce::MessageBoxIconType icon) const
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withTitle (titleText)
                .withMessage (message)
                .withButton ("OK")
                .withIconType (icon),
            nullptr);
    }

    void LaunchCenterPage::paint (juce::Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        juce::ColourGradient bg (juce::Colour (0xff080b10), full.getX(), full.getY(),
                                 juce::Colour (0xff111923), full.getRight(), full.getBottom(), false);
        g.setGradientFill (bg);
        g.fillAll();

        g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.025f));
        for (int x = 0; x < getWidth(); x += 72)
            g.drawVerticalLine (x, 0.0f, (float) getHeight());

        auto bounds = getLocalBounds().reduced (24);
        bounds.removeFromTop (34 + 24 + 8 + 36 + 12);

        auto panel = bounds.toFloat();
        g.setColour (PatchCraftLookAndFeel::panel().withAlpha (0.9f));
        g.fillRoundedRectangle (panel, 12.0f);
        g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.9f));
        g.drawRoundedRectangle (panel, 12.0f, 1.0f);

        juce::Colour accent = PatchCraftLookAndFeel::accent();
        if (activeTab == ContentTab::Overview)
            accent = errorCount > 0 ? juce::Colour (0xffff5f5f)
                  : (warningCount > 0 ? PatchCraftLookAndFeel::accent() : juce::Colour (0xff64d88a));
        else if (activeTab == ContentTab::Create)
            accent = PatchCraftLookAndFeel::accent();
        else if (activeTab == ContentTab::Demos)
            accent = juce::Colour (0xff58b7ff);
        else
            accent = juce::Colour (0xff79c267);

        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (panel.withHeight (3.0f), 2.0f);
    }

    void LaunchCenterPage::resized()
    {
        auto bounds = getLocalBounds().reduced (24);
        title.setBounds (bounds.removeFromTop (34));
        subtitle.setBounds (bounds.removeFromTop (24));
        bounds.removeFromTop (8);

        auto tabRow = bounds.removeFromTop (36);
        const int tabGap = 8;
        const int tabW = juce::jmax (108, (tabRow.getWidth() - tabGap * 3) / 4);
        for (auto* tab : { &tabOverview, &tabCreate, &tabDemos, &tabDoctor })
        {
            tab->setBounds (tabRow.removeFromLeft (tabW).reduced (1));
            tabRow.removeFromLeft (tabGap);
        }

        bounds.removeFromTop (12);
        auto content = bounds.reduced (10, 14);

        if (activeTab == ContentTab::Overview)
        {
            auto summaryCard = content.removeFromTop (juce::jmax (120, content.getHeight() / 3)).reduced (4, 2);
            statusBadge.setBounds (summaryCard.removeFromTop (28).reduced (2));
            summaryCard.removeFromTop (6);
            summaryLabel.setBounds (summaryCard);

            content.removeFromTop (14);
            exportShipLabel.setBounds (content.removeFromTop (18));
            content.removeFromTop (4);
            auto shipRow = content.removeFromTop (36);
            const int shipGap = 8;
            const int shipW = juce::jmax (120, (shipRow.getWidth() - shipGap * 3) / 4);
            for (auto* button : { &exportPackButton, &exportVstButton, &publishButton, &bundleButton })
            {
                button->setBounds (shipRow.removeFromLeft (shipW).reduced (1));
                shipRow.removeFromLeft (shipGap);
            }

            content.removeFromTop (12);
            exportToolsLabel.setBounds (content.removeFromTop (18));
            content.removeFromTop (4);
            auto toolsRow = content.removeFromTop (36);
            const int toolGap = 8;
            const int toolW = juce::jmax (108, (toolsRow.getWidth() - toolGap * 4) / 5);
            for (auto* button : { &refreshButton, &outputFolderButton, &testButton, &customerWizardButton, &productPageButton })
            {
                button->setBounds (toolsRow.removeFromLeft (toolW).reduced (1));
                toolsRow.removeFromLeft (toolGap);
            }

            outputFolderLabel.setBounds (content.removeFromBottom (24));
        }
        else if (activeTab == ContentTab::Create)
        {
            creatorTitle.setBounds (content.removeFromTop (24));
            creatorBody.setBounds (content.removeFromTop (38));
            content.removeFromTop (8);

            auto top = content.removeFromTop (32);
            recipeTypeBox.setBounds (top.removeFromLeft (128));
            top.removeFromLeft (8);
            createFromPromptButton.setBounds (top.removeFromLeft (150));
            top.removeFromLeft (8);
            blankProjectButton.setBounds (top.removeFromLeft (128));

            content.removeFromTop (10);
            recipePrompt.setBounds (content.removeFromTop (juce::jmax (120, content.getHeight() / 2)));

            content.removeFromTop (12);
            auto quick = content.removeFromTop (36);
            for (auto* button : { &synthStarterButton, &sampleStarterButton, &drumStarterButton, &fxStarterButton })
            {
                button->setBounds (quick.removeFromLeft (juce::jmax (96, quick.getWidth() / 4 - 8)));
                quick.removeFromLeft (8);
            }

            outputFolderLabel.setBounds (content.removeFromBottom (24));
        }
        else if (activeTab == ContentTab::Demos)
        {
            auto demoHeader = content.removeFromTop (42);
            demoTitle.setBounds (demoHeader.removeFromTop (22));
            demoBody.setBounds (demoHeader);
            demoViewport.setBounds (content);

            const int tileW = 238;
            const int tileH = juce::jmax (132, demoViewport.getHeight() - demoViewport.getScrollBarThickness() - 4);
            const int tileGap = 10;
            int x = 0;
            for (auto& tile : demoTiles)
            {
                tile->setBounds (x, 0, tileW, tileH);
                x += tileW + tileGap;
            }
            demoContent.setSize (juce::jmax (demoViewport.getWidth(), x + 4), tileH);
        }
        else
        {
            doctorTitle.setBounds (content.removeFromTop (24));
            doctorBody.setBounds (content.removeFromTop (34));
            content.removeFromTop (8);
            checksViewport.setBounds (content);

            const int width = juce::jmax (360, checksViewport.getWidth() - checksViewport.getScrollBarThickness() - 6);
            const int rowH = 86;
            const int gapY = 8;
            int y = 0;
            for (auto& row : checkRows)
            {
                row->setBounds (0, y, width, rowH);
                y += rowH + gapY;
            }
            checksContent.setSize (width, juce::jmax (checksViewport.getHeight(), y + 8));
        }
    }
}
