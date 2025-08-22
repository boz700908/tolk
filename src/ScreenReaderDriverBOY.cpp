/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>

typedef void(__stdcall *BoyCtrlSetAnyKeyStopSpeakingFunc)(bool);

// Global variable to store the reason value
// Reason: 1 = speaking completed, 2 = interrupted by new speaking, 3 = interrupted by stopped call
static int g_speakCompleteReason = -1;

void __stdcall SpeakCompleteCallback(int reason)
{
    g_speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false)
#ifdef _WIN64
    , controller(LoadLibrary(L"BoyCtrl-x64.dll"))
#else
    , controller(LoadLibrary(L"BoyCtrl.dll"))
#endif
    , BoyInit(nullptr)
    , BoyUninit(nullptr)
    , BoyIsRunning(nullptr)
    , BoySpeak(nullptr)
    , BoyStopSpeak(nullptr)
{
    if (!controller) {
        return;
    }

    BoyInit      = reinterpret_cast<BoyCtrlInitialize>(   GetProcAddress(controller, "BoyCtrlInitialize"));
    BoyUninit    = reinterpret_cast<BoyCtrlUninitialize>( GetProcAddress(controller, "BoyCtrlUninitialize"));
    BoyIsRunning = reinterpret_cast<BoyCtrlIsReaderRunning>(GetProcAddress(controller, "BoyCtrlIsReaderRunning"));
    BoySpeak     = reinterpret_cast<BoyCtrlSpeak>(        GetProcAddress(controller, "BoyCtrlSpeak"));
    BoyStopSpeak = reinterpret_cast<BoyCtrlStopSpeaking>(GetProcAddress(controller, "BoyCtrlStopSpeaking"));

    if (BoyInit) {
        BoyInit(nullptr);
    }

    auto pAnyKeyStop = reinterpret_cast<BoyCtrlSetAnyKeyStopSpeakingFunc>(
        GetProcAddress(controller, "BoyCtrlSetAnyKeyStopSpeaking"));
    if (pAnyKeyStop) {
        pAnyKeyStop(false);
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    if (!controller) {
        return;
    }

    if (BoyUninit) {
        BoyUninit();
    }

    FreeLibrary(controller);
}

bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool /*interrupt*/)
{
    if (BoySpeak) {
        return (BoySpeak(str, false, true, true, SpeakCompleteCallback) == 0);
    }
    return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t* /*str*/)
{
    return false;
}

bool ScreenReaderDriverBOY::Silence()
{
    if (BoyStopSpeak) {
        BoyStopSpeak(false);
        g_speakCompleteReason = 3;
        return true;
    }
    return false;
}

bool ScreenReaderDriverBOY::IsSpeaking()
{
    return (g_speakCompleteReason != 1);
}

bool ScreenReaderDriverBOY::IsActive()
{
    if (BoyIsRunning) {
        return (BoyIsRunning() == 2);
    }
    return false;
}

bool ScreenReaderDriverBOY::Output(const wchar_t* str, bool interrupt)
{
    bool spoke    = Speak(str, interrupt);
    bool brailled = Braille(str);
    return (spoke || brailled);
}
