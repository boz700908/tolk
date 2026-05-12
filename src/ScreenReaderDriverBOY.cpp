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

    if (!controller)
        return;

    BoyInit       = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
    BoyUninit     = (BoyCtrlUninitialize)GetProcAddress(controller, "BoyCtrlUninitialize");
    BoyIsRunning  = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
    BoySpeak      = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
    BoyStopSpeak  = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");

    if (BoyInit)
    {
        int err = BoyInit(nullptr);
        if (err != e_bcerr_success)
        {
            FreeLibrary(controller);
            controller = nullptr;
        }
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    if (controller)
    {
        if (BoyUninit)
            BoyUninit();

        FreeLibrary(controller);
        controller = nullptr;
    }
}

bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool interrupt)
{
    if (!controller || !BoySpeak || !str || str[0] == L'\0')
        return false;

    // 打断旧朗读：true=打断，false=追加
    g_speakCompleteReason = -1;
    int err = BoySpeak(str, !interrupt, SpeakCompleteCallback);

    return (err == e_bcerr_success);
}

bool ScreenReaderDriverBOY::Braille(const wchar_t* /*str*/)
{
    return false;
}

bool ScreenReaderDriverBOY::IsSpeaking()
{
    // 回调未返回 = 正在朗读
    return (g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::Silence()
{
    if (!controller || !BoyStopSpeak)
        return false;

    int err = BoyStopSpeak();
    if (err == e_bcerr_success)
    {
        g_speakCompleteReason = e_bcerr_unavailable;
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
    bool spoke = Speak(str, interrupt);
    bool brailled = Braille(str);
    return spoke || brailled;
}
