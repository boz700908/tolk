/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverBOY.h"
#include <windows.h>

int ScreenReaderDriverBOY::speakCompleteReason = -1;

void __stdcall ScreenReaderDriverBOY::SpeakCompleteCallback(int reason)
{
    speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY()
    : ScreenReaderDriver(L"BoyPCReader", true, false),
      controller(nullptr),
      boyCtrlInitialize(nullptr), boyCtrlUninitialize(nullptr),
      boyCtrlIsReaderRunning(nullptr), boyCtrlSpeak(nullptr),
      boyCtrlStopSpeaking(nullptr)
{
#ifdef _WIN64
    controller = LoadLibraryW(L"byctrl-x64.dll");
#else
    controller = LoadLibraryW(L"byctrl.dll");
#endif
    if (!controller) {
        return;
    }
    boyCtrlInitialize       = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
    boyCtrlUninitialize     = (BoyCtrlUninitialize)GetProcAddress(controller, "BoyCtrlUninitialize");
    boyCtrlIsReaderRunning  = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
    boyCtrlSpeak            = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
    boyCtrlStopSpeaking     = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");
    if (boyCtrlInitialize) {
        int err = boyCtrlInitialize(nullptr);
        if (err != e_bcerr_success) {
            FreeLibrary(controller);
            controller = nullptr;
        }
    }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY()
{
    if (controller) {
        if (boyCtrlUninitialize)
            boyCtrlUninitialize();
        FreeLibrary(controller);
        controller = nullptr;
    }
}

bool ScreenReaderDriverBOY::Speak(const wchar_t* str, bool /*interrupt*/)
{
    speakCompleteReason = -1;
    if (!controller || !boyCtrlSpeak || !str || !str[0])
        return false;
    int err = boyCtrlSpeak(str, false, SpeakCompleteCallback);
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
    return (speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::Silence()
{
    if (!controller || !boyCtrlStopSpeaking)
        return false;
    int err = boyCtrlStopSpeaking();
    if (err == e_bcerr_success) {
        speakCompleteReason = 3;
        return true;
    }
    return false;
}

bool ScreenReaderDriverBOY::IsActive()
{
    if (!controller || !boyCtrlIsReaderRunning)
        return false;
    return boyCtrlIsReaderRunning() != 0;
}

bool ScreenReaderDriverBOY::Output(const wchar_t* str, bool interrupt)
{
    bool spoke    = Speak(str, interrupt);
    bool brailled = Braille(str);
    return (spoke || brailled);
}