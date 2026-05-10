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

    void StudioApplication::initialise (const juce::String&)
    {
        #ifdef _WIN32
        SetUnhandledExceptionFilter(pcCrashHandler);
        #endif
        PC_DBG("StudioApplication::initialise - app starting");
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        mainWindow = std::make_unique<StudioWindow> (getApplicationName(), laf);
    }

    void StudioApplication::shutdown()
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

} // namespace patchcraft
