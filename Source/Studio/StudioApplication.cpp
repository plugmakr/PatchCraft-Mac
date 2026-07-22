#include "StudioApplication.h"
#include "DebugLog.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

static void pcWriteMiniDump (EXCEPTION_POINTERS* ep)
{
    char path[MAX_PATH] = {};
    const char* tmp = std::getenv ("TEMP");
    if (tmp == nullptr) tmp = ".";
    SYSTEMTIME st {};
    GetLocalTime (&st);
    std::snprintf (path, sizeof (path),
                   "%s\\PatchCraftStudio_%04d%02d%02d_%02d%02d%02d.dmp",
                   tmp, (int) st.wYear, (int) st.wMonth, (int) st.wDay,
                   (int) st.wHour, (int) st.wMinute, (int) st.wSecond);

    HANDLE file = CreateFileA (path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        PC_DBG ("=== CRASH DUMP FAILED: could not create %s ===", path);
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION info {};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = ep;
    info.ClientPointers = FALSE;

    const BOOL ok = MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), file,
                                       MiniDumpWithIndirectlyReferencedMemory,
                                       ep != nullptr ? &info : nullptr,
                                       nullptr, nullptr);
    CloseHandle (file);
    PC_DBG ("=== CRASH DUMP %s: %s ===", ok ? "OK" : "FAILED", path);
}

static void pcLogLoadedModules()
{
    HMODULE modules[256] = {};
    DWORD needed = 0;
    if (! EnumProcessModules (GetCurrentProcess(), modules, sizeof (modules), &needed))
        return;

    const unsigned count = needed / sizeof (HMODULE);
    for (unsigned i = 0; i < count && i < 64; ++i)
    {
        MODULEINFO mi {};
        char name[MAX_PATH] = {};
        if (! GetModuleInformation (GetCurrentProcess(), modules[i], &mi, sizeof (mi)))
            continue;
        GetModuleFileNameA (modules[i], name, MAX_PATH);
        PC_DBG ("MODULE base=0x%p size=0x%X %s",
                mi.lpBaseOfDll, (unsigned) mi.SizeOfImage, name);
    }
}

static LONG WINAPI pcCrashHandler (EXCEPTION_POINTERS* ep)
{
    const char* name = "Unknown";
    switch (ep->ExceptionRecord->ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:    name = "ACCESS_VIOLATION"; break;
        case EXCEPTION_STACK_OVERFLOW:      name = "STACK_OVERFLOW"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  name = "DIVIDE_BY_ZERO"; break;
        case EXCEPTION_PRIV_INSTRUCTION:    name = "PRIV_INSTRUCTION"; break;
    }

    void* stack[64] = {};
    const USHORT frames = CaptureStackBackTrace (0, 64, stack, nullptr);
    PC_DBG ("=== CRASH: %s (code=0x%08X) at addr=0x%p tid=%u frames=%u ===",
            name, (unsigned) ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress,
            (unsigned) GetCurrentThreadId(), (unsigned) frames);
    for (USHORT i = 0; i < frames; ++i)
        PC_DBG ("  #%02u 0x%p", (unsigned) i, stack[i]);

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && ep->ExceptionRecord->NumberParameters >= 2)
    {
        PC_DBG ("  AV op=%s target=0x%p",
                ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "read"
                : ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "exec",
                (void*) ep->ExceptionRecord->ExceptionInformation[1]);
    }

    pcLogLoadedModules();
    pcWriteMiniDump (ep);
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
