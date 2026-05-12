/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>

int ScreenReaderDriverBOY::g_speakCompleteReason = -1;

void __stdcall ScreenReaderDriverBOY::SpeakCompleteCallback(int reason)
{
    g_speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false),
      controller(nullptr),
      BoyInit(nullptr), BoyUninit(nullptr),
      BoyIsRunning(nullptr), BoySpeak(nullptr),
      BoyStopSpeak(nullptr)
{
#ifdef _WIN64
    controller = LoadLibraryW(L"byctrl-x64.dll");
#else
    controller = LoadLibraryW(L"byctrl.dll");
#endif
    if (!controller) {
        return;
    }
    BoyInit       = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
    BoyUninit     = (BoyCtrlUninitialize)GetProcUninitialize");
    BoyIsRunning  = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
    BoySpeak      = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
    BoyStopSpeak  = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");
    if (BoyInit) {
        int err = BoyInit(nullptr);
        if (err != e_bcerr_success) controller = nullptr;
        }
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    if (controller) {
        if (BoyUninit)
            BoyUninit();
        FreeLibrary(controller);
        controller = nullptr;
    }
}

bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool /*interrupt*/)
{
    g_speakCompleteReason = -1;
    if (!controller || !BoySpeak || !str || !str[0])
        return false;
    int err = BoySpeak(str, false, SpeakCompleteCallback);
    if (err == e_bcerr_success)
        return true;
    return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t* /*str*/)
{
    return false;
}

bool ScreenReaderDriverBOY::IsSpeaking()
{
    return (g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::Silence()
{
    if (!controller || !BoyStopSpeak)
        return false;
    int err = BoyStopSpeak();
    if (err == e_bcerr_success) {
        g_speakCompleteReason = 3;
        return true;
    }
    return false;
}

bool ScreenReaderDriverBOY::IsActive()
{
    if (!controller || !BoyIsRunning)
        return false;
    return BoyIsRunning() != 0;
}

bool ScreenReaderDriverBOY::Output(const wchar_t* str, bool interrupt)
{
    bool spoke    = Speak(str, interrupt);
    bool brailled = Braille(str);
    return (spoke || brailled);
}