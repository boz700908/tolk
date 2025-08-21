/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>

// Global variable to store the reason value
//Reason: Reason for callback, 1=speaking completed, 2=Interrupted by new speaking, 3=Interrupted by stopped call

static int g_speakCompleteReason = -1;

void __stdcall SpeakCompleteCallback(int reason) {
    g_speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false)
#ifdef _WIN64
    , controller(LoadLibrary(L"BoyCtrl-x64.dll"))
#else
    , controller(LoadLibrary(L"BoyCtrl.dll"))
#endif
    , BoySpeak(nullptr)
    , BoyStopSpeak(nullptr)
    , BoyInit(nullptr)
    , BoyUninit(nullptr)
    , BoyIsRunning(nullptr)
{
    if (controller) {
        BoyInit      = reinterpret_cast<BoyCtrlInitialize>(GetProcAddress(controller, "BoyCtrlInitialize"));
        BoyUninit    = reinterpret_cast<BoyCtrlUninitialize>(GetProcAddress(controller, "BoyCtrlUninitialize"));
        BoyIsRunning = reinterpret_cast<BoyCtrlIsReaderRunning>(GetProcAddress(controller, "BoyCtrlIsReaderRunning"));
        BoySpeak     = reinterpret_cast<BoyCtrlSpeak>(GetProcAddress(controller, "BoyCtrlSpeak"));
        BoyStopSpeak = reinterpret_cast<BoyCtrlStopSpeaking>(GetProcAddress(controller, "BoyCtrlStopSpeaking"));

        if (BoyInit) {
            BoyInit(nullptr);
        }
        BoyCtrlSetAnyKeyStopSpeaking(false);
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY() {
    if (controller) {
        if (BoyUninit) {
            BoyUninit();
        }
        FreeLibrary(controller);
    }
}

bool ScreenReaderDriverBOY::Speak(const wchar_t *str, bool interrupt) {
    g_speakCompleteReason = -1;
    if (BoySpeak) {
        return (BoySpeak(str, false, true, true, SpeakCompleteCallback) == 0);
    }
    return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t *) {
    return false;
}

bool ScreenReaderDriverBOY::Silence() {
    if (BoyStopSpeak) {
        BoyStopSpeak(false);
        g_speakCompleteReason = 3;
        return true;
    }
    return false;
}

bool ScreenReaderDriverBOY::IsSpeaking() {
    return (g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::IsActive() {
    if (BoyIsRunning) {
        return BoyIsRunning();
    }
    return false;
}

bool ScreenReaderDriverBOY::Output(const wchar_t *str, bool interrupt) {
    bool speak   = Speak(str, interrupt);
    bool braille = Braille(str);
    return (speak || braille);
}
