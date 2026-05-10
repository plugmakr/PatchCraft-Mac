#include "PluginEditor.h"

namespace patchcraft
{
    PlayerEditor::PlayerEditor (PlayerProcessor& p)
        : juce::AudioProcessorEditor (&p), proc (p)
    {
        setLookAndFeel (&laf);

        // Build the renderer + buttons BEFORE calling setSize/setResizable -
        // setSize triggers resized() synchronously, and resized() dereferences
        // renderer->setBounds(...). If we set size first, that's a null deref.
        renderer = std::make_unique<PlayerGuiRenderer> (proc, assets);
        addAndMakeVisible (*renderer);

        // Library browser (hidden by default)
        libraryBrowser = std::make_unique<LibraryBrowser> (proc.getLibraryScanner());
        libraryBrowser->onPackSelected = [this] (const juce::File& folder) {
            juce::String err;
            if (proc.loadPack (folder, err))
            {
                libraryVisible = false;
                libraryBrowser->setVisible (false);
                renderer->setVisible (true);
                resized();
            }
            else
            {
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle ("Load Failed")
                        .withMessage ("Could not load pack:\n" + err)
                        .withButton ("OK")
                        .withIconType (juce::MessageBoxIconType::WarningIcon),
                    nullptr);
            }
        };
        libraryBrowser->onClose = [this] {
            libraryVisible = false;
            libraryBrowser->setVisible (false);
            renderer->setVisible (true);
            resized();
        };
        libraryBrowser->setVisible (false);
        addAndMakeVisible (*libraryBrowser);

        addAndMakeVisible (loadBtn);
        loadBtn.getProperties().set ("accent", true);
        loadBtn.onClick = [this] { showLoadDialog(); };

        addAndMakeVisible (menuBtn);
        menuBtn.onClick = [this] { showPackMenu(); };

        randomizeBtn.onClick = [this] { proc.randomizeCurrentPreset(); };
        addAndMakeVisible (randomizeBtn);

        abBtn.onClick = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "Save A", true, false);
            m.addItem (2, "Save B", true, false);
            m.addSeparator();
            m.addItem (3, "Recall A  (" + proc.getAbSnapshotName (0) + ")", true, false);
            m.addItem (4, "Recall B  (" + proc.getAbSnapshotName (1) + ")", true, false);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&abBtn),
                [this] (int r)
                {
                    if      (r == 1) proc.saveAbSnapshot (0);
                    else if (r == 2) proc.saveAbSnapshot (1);
                    else if (r == 3) proc.recallAbSnapshot (0);
                    else if (r == 4) proc.recallAbSnapshot (1);
                });
        };
        addAndMakeVisible (abBtn);

        // Now safe to size and add the corner resizer.
        setSize (960, 600);
        setResizable (true, true);
        setResizeLimits (640, 400, 2400, 1500);

        proc.addEditorListener (this);
        packChanged();
    }

    PlayerEditor::~PlayerEditor()
    {
        proc.removeEditorListener (this);
        setLookAndFeel (nullptr);
    }

    void PlayerEditor::paint (juce::Graphics& g)
    {
        // Background drawn entirely by the renderer; this is just the fallback.
        if (const auto* pack = proc.getPack())
            g.fillAll (pack->manifest.playerBackgroundColour);
        else
            g.fillAll (PatchCraftLookAndFeel::bg());
    }

    void PlayerEditor::resized()
    {
        if (renderer == nullptr) return;

        auto r = getLocalBounds();
        const auto* pack = proc.getPack();
        const bool showPackMenu = pack == nullptr || pack->manifest.playerShowPackMenu;

        if (libraryVisible)
        {
            libraryBrowser->setBounds (r);
            renderer->setVisible (false);
            libraryBrowser->setVisible (true);
        }
        else
        {
            renderer->setBounds (r);
            renderer->setVisible (true);
            libraryBrowser->setVisible (false);
        }

        // Pack-menu button stays small in the top-right; load button is hidden
        // because the demo pack is always loaded by default.
        int xOff = r.getRight() - 70;
        menuBtn.setBounds (xOff, 6, 60, 22);
        menuBtn.setVisible (showPackMenu);
        xOff -= 78;
        randomizeBtn.setBounds (xOff, 6, 70, 22);
        randomizeBtn.setVisible (pack != nullptr);
        xOff -= 52;
        abBtn.setBounds (xOff, 6, 44, 22);
        abBtn.setVisible (pack != nullptr);
        loadBtn.setVisible (false);
    }

    void PlayerEditor::packChanged()
    {
        renderer->rebuild();
        loadBtn.setVisible (false);
        menuBtn.setVisible (proc.getPack() == nullptr || proc.getPack()->manifest.playerShowPackMenu);
        randomizeBtn.setVisible (proc.getPack() != nullptr);
        abBtn.setVisible (proc.getPack() != nullptr);
        resized();
        repaint();
    }

    bool PlayerEditor::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (auto& f : files)
            if (juce::File (f).isDirectory() || f.endsWithIgnoreCase (".patchcraft"))
                return true;
        return false;
    }

    void PlayerEditor::filesDropped (const juce::StringArray& files, int, int)
    {
        for (auto& f : files)
        {
            juce::File folder (f);
            if (! folder.exists()) continue;
            if (! folder.isDirectory() && f.endsWithIgnoreCase (".patchcraft"))
            {
                // .patchcraft files could be unpacked to a temp folder in future
                continue;
            }
            juce::String err;
            if (proc.loadPack (folder, err))
                break;
        }
    }

    void PlayerEditor::showLoadDialog()
    {
        // Let user select the instrument folder directly
        auto chooser = std::make_shared<juce::FileChooser> (
            "Select Instrument Folder",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser] (const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder == juce::File() || !folder.isDirectory()) return;

                juce::String err;
                if (! proc.loadPack (folder, err))
                {
                    juce::AlertWindow::showAsync (
                        juce::MessageBoxOptions()
                            .withTitle ("Load Failed")
                            .withMessage ("Could not load instrument from:\n" + folder.getFullPathName() + "\n\n" + err)
                            .withButton ("OK")
                            .withIconType (juce::MessageBoxIconType::WarningIcon),
                        nullptr);
                }
            });
    }

    void PlayerEditor::showPackMenu()
    {
        juce::PopupMenu m;
        const auto* pack = proc.getPack();
        const bool allowLoad = pack == nullptr || pack->manifest.playerAllowPackLoading;
        const bool showLibrary = pack == nullptr || pack->manifest.playerShowLibraryBrowser;
        const bool showAbout = pack == nullptr || pack->manifest.playerShowAbout;
        m.addItem (1, "Load Pack...", allowLoad);
        m.addItem (2, "Library Browser", showLibrary);
        if (pack != nullptr)
        {
            m.addSeparator();
            m.addItem (5, "Randomize Preset", true);
            m.addItem (6, "Save A/B Snapshots", true);
        }
        if (allowLoad || showLibrary)
            m.addSeparator();
        m.addItem (3, "Reset to Demo Instrument", allowLoad);
        if (showAbout)
        {
            m.addSeparator();
            juce::String about = "About: ";
            if (pack != nullptr)
            {
                about += pack->manifest.playerDisplayName.isNotEmpty()
                    ? pack->manifest.playerDisplayName
                    : pack->manifest.instrumentName;
                if (pack->manifest.version.isNotEmpty())
                    about += " v" + pack->manifest.version;
                if (pack->manifest.creator.isNotEmpty())
                    about += " by " + pack->manifest.creator;
            }
            else
            {
                about += "(none)";
            }
            m.addItem (4, about, false);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn),
            [this] (int r)
            {
                if (r == 1) showLoadDialog();
                else if (r == 2) toggleLibrary();
                else if (r == 3) proc.unloadPack();
                else if (r == 5) proc.randomizeCurrentPreset();
                else if (r == 6)
                {
                    proc.saveAbSnapshot (0);
                    proc.saveAbSnapshot (1);
                }
            });
    }

    void PlayerEditor::toggleLibrary()
    {
        libraryVisible = !libraryVisible;
        if (libraryVisible)
        {
            // Scan library when first opened
            proc.getLibraryScanner().scanLibrary();
        }
        resized();
    }

} // namespace patchcraft
