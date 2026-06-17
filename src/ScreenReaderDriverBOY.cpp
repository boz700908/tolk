/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the Boy screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */
// The Boy Project provides a header and libraries,
// but we don't use these in order to support running even if the DLL is missing.
#include "ScreenReaderDriverBOY.h"
#include "TolkDebug.h"
#include <windows.h>
ScreenReaderDriverBOY* ScreenReaderDriverBOY::g_instance = nullptr;
void __stdcall ScreenReaderDriverBOY::SpeakCompleteCallback(int reason)
{
    // Per official documentation:
    // reason = 1 (e_bccr_normal): Completed normally
    // reason = 2 (e_bccr_interrupted): Interrupted by new speak task
    // reason = 3 (e_bccr_stopped): Stopped by StopSpeaking API
    if (g_instance) {
        InterlockedExchange(&g_instance->speakCompleteReason, reason);
        InterlockedExchange(&g_instance->isSpeaking, 0);
    }
}
ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false),
      controller(nullptr),
      srwLock(SRWLOCK_INIT),  // SRWLock static initialization
      isSpeaking(0),
      speakCompleteReason(0),
      BoyInit(nullptr), BoyUninit(nullptr),
      BoyIsRunning(nullptr), BoyGetState(nullptr),
      BoySpeak(nullptr), BoyStopSpeak(nullptr)
{
    g_instance = this;
#ifdef _WIN64
    TOLK_LOG_INFO("BOY: Loading 64-bit byctrl-x64.dll");
    controller = LoadLibraryW(L"byctrl-x64.dll");
#else
    TOLK_LOG_INFO("BOY: Loading 32-bit byctrl.dll");
    controller = LoadLibraryW(L"byctrl.dll");
#endif
    if (!controller) {
        TOLK_LOG_WARN("BOY: DLL not found, driver disabled");
        return;
    }
    TOLK_LOG_INFO("BOY: DLL loaded successfully");
    // Load all BoyCtrl API functions per official documentation
    BoyInit       = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
    BoyUninit     = (BoyCtrlUninitialize)GetProcAddress(controller, "BoyCtrlUninitialize");
    BoyIsRunning  = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
    BoyGetState   = (BoyCtrlGetReaderState)GetProcAddress(controller, "BoyCtrlGetReaderState");
    BoySpeak      = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
    BoyStopSpeak  = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");
    int loadedCount = (BoyInit?1:0) + (BoyUninit?1:0) + (BoyIsRunning?1:0) +
                      (BoyGetState?1:0) + (BoySpeak?1:0) + (BoyStopSpeak?1:0);
    TOLK_LOG_INFO("BOY: Loaded %d/6 API functions", loadedCount);
    // Per official docs: Must call BoyCtrlInitialize before using any other API
    if (BoyInit)
    {
        // Pass NULL for logPath to disable logging
        int err = BoyInit(nullptr);
        if (err != e_bcerr_success)
        {
            TOLK_LOG_ERROR("BOY: BoyCtrlInitialize failed, err=%d", err);
            // Initialization failed, cleanup
            FreeLibrary(controller);
            controller = nullptr;
            return;
        }
        TOLK_LOG_INFO("BOY: BoyCtrlInitialize succeeded");
    }
    else {
        TOLK_LOG_WARN("BOY: BoyCtrlInitialize not found (old DLL version?)");
    }
}
ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    AcquireSRWLockExclusive(&srwLock);
    g_instance = nullptr;
    if (controller)
    {
        // Per official docs: Must call BoyCtrlUninitialize on exit
        if (BoyUninit)
            BoyUninit();
        FreeLibrary(controller);
        controller = nullptr;
    }
    ReleaseSRWLockExclusive(&srwLock);
    // SRWLock does not need destruction
}
bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool interrupt)
{
    AcquireSRWLockExclusive(&srwLock);
    if (!controller || !BoySpeak || !str || str[0] == L'\0') {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }
    // Per official documentation:
    // append = true: Queue speech (do NOT interrupt)
    // append = false: Interrupt current speech
    // So: interrupt -> !interrupt = append
    bool append = !interrupt;
    // If interrupt requested, stop current speech first
    if (interrupt && BoyStopSpeak) {
        BoyStopSpeak();
    }
    InterlockedExchange(&speakCompleteReason, 0);
    InterlockedExchange(&isSpeaking, 1);
    int err = BoySpeak(str, append, SpeakCompleteCallback);
    ReleaseSRWLockExclusive(&srwLock);
    return (err == e_bcerr_success);
}
bool ScreenReaderDriverBOY::Braille(const wchar_t* /*str*/)
{
    return false;
}
bool ScreenReaderDriverBOY::IsSpeaking()
{
    // Only return true if speech was explicitly started and not yet completed
    return (InterlockedCompareExchange(&isSpeaking, 0, 0) == 1);
}
bool ScreenReaderDriverBOY::Silence()
{
    AcquireSRWLockExclusive(&srwLock);
    if (!controller || !BoyStopSpeak) {
        ReleaseSRWLockExclusive(&srwLock);
        return false;
    }
    int err = BoyStopSpeak();
    if (err == e_bcerr_success)
    {
        InterlockedExchange(&speakCompleteReason, e_bccr_stopped);
        InterlockedExchange(&isSpeaking, 0);
        ReleaseSRWLockExclusive(&srwLock);
        return true;
    }
    ReleaseSRWLockExclusive(&srwLock);
    return false;
}
bool ScreenReaderDriverBOY::IsActive()
{
    // Performance: Check cache first (100ms timeout, no lock needed: read-only)
    DWORD currentTime = GetTickCount();
    if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
        return cachedIsActive;
    }

    AcquireSRWLockShared(&srwLock);
    if (!controller || !BoyIsRunning) {
        cachedIsActive = false;
        lastIsActiveTime = currentTime;
        ReleaseSRWLockShared(&srwLock);
        return false;
    }

    cachedIsActive = BoyIsRunning();
    lastIsActiveTime = currentTime;
    ReleaseSRWLockShared(&srwLock);
    return cachedIsActive;
}
