#include "StudioApplication.h"
#include "DebugLog.h"

#ifdef _WIN32
#include <windows.h>
static LONG WINAPI pcCrashHandler(EXCEPTION_POINTERS* ep)
{
    const char* name = "Unknown";
    switch (ep->ExceptionRecord->ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:    name = "ACCESS_VIOLATION"; break;
        case EXCEPTION_STACK_OVERFLOW:      name = "STACK_OVERFLOW"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  name = "DIVIDE_BY_ZERO"; break;
        case EXCEPTION_PRIV_INSTRUCTION:    name = "PRIV_INSTRUCTION"; break;
    }
    PC_DBG("=== CRASH: %s (code=0x%08X) at addr=0x%p ===",
           name, (unsigned)ep->ExceptionRecord->ExceptionCode,
           ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

namespace patchcraft
{
    StudioApplication::StudioWindow::StudioWindow (juce::String name, PatchCraftLookAndFeel& laf)
        : juce::DocumentWindow (name,
                                PatchCraftLookAndFeel::bg(),
                                juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setLookAndFeel (&laf);
        setResizable (true, true);
        setResizeLimits (1024, 700, 4096, 2400);
        setContentOwned (new StudioMainComponent(), false);

        auto displayArea = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
            ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
            : juce::Rectangle<int> (0, 0, 1440, 900);
        displayArea.reduce (8, 8);

        const int preferredW = juce::jmin (1600, displayArea.getWidth());
        const int preferredH = juce::jmin (1000, displayArea.getHeight());
        setBounds (displayArea.withSizeKeepingCentre (preferredW, preferredH)
                              .constrainedWithin (displayArea));
        setVisible (true);
    }

    bool StudioApplication::StudioWindow::keyPressed (const juce::KeyPress& key)
    {
        const auto mods = key.getModifiers();
        const bool command = mods.isCommandDown() || mods.isCtrlDown();
        const int code = key.getKeyCode();

        if (command && ! mods.isAltDown() && (code == 'z' || code == 'Z'))
        {
            if (auto* studio = dynamic_cast<StudioMainComponent*> (getContentComponent()))
            {
                if (mods.isShiftDown())
                    studio->redo();
                else
                    studio->undo();
                return true;
            }
        }

        if (command && ! mods.isAltDown() && (code == 'y' || code == 'Y'))
        {
            if (auto* studio = dynamic_cast<StudioMainComponent*> (getContentComponent()))
            {
                studio->redo();
                return true;
            }
        }

        return juce::DocumentWindow::keyPressed (key);
    }

    namespace
    {
        juce::File screenshotOutputFolderFromCommandLine (const juce::String& commandLine)
        {
            constexpr const char* key = "--capture-tutorial-screenshots=";
            if (! commandLine.contains (key))
                return {};

            auto value = commandLine.fromFirstOccurrenceOf (key, false, false).trim();
            if (value.startsWithChar ('"'))
                value = value.fromFirstOccurrenceOf ("\"", false, false)
                             .upToFirstOccurrenceOf ("\"", false, false);
            else if (value.containsChar (' '))
                value = value.upToFirstOccurrenceOf (" ", false, false);

            return value.isNotEmpty() ? juce::File (value) : juce::File();
        }
    }

    void StudioApplication::initialise (const juce::String& commandLine)
    {
        #ifdef _WIN32
        SetUnhandledExceptionFilter(pcCrashHandler);
        #endif
        PC_DBG("StudioApplication::initialise - app starting");
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        mainWindow = std::make_unique<StudioWindow> (getApplicationName(), laf);

        const auto screenshotFolder = screenshotOutputFolderFromCommandLine (commandLine);
        if (screenshotFolder.getFullPathName().isNotEmpty())
        {
            juce::Timer::callAfterDelay (750, [this, screenshotFolder]
            {
                juce::String error;
                if (mainWindow != nullptr)
                {
                    if (auto* studio = dynamic_cast<StudioMainComponent*> (mainWindow->getContentComponent()))
                    {
                        if (! studio->captureTutorialScreenshots (screenshotFolder, error))
                            PC_DBG("Tutorial screenshot capture failed: %s", error.toRawUTF8());
                    }
                }

                quit();
            });
        }
    }

    void StudioApplication::shutdown()
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

} // namespace patchcraft
