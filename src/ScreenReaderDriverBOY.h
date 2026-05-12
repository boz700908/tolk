/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.h
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_BOY_H_
#define _SCREEN_READER_DRIVER_BOY_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

typedef void (__stdcall*BoyCtrlSpeakCompleteFunc)(int reason);

enum BoyCtrlError {
    e_bcerr_success = 0,
    e_bcerr_fail = 1,
    e_bcerr_arg = 2,
    e_bcerr_unavailable = 3
};

class ScreenReaderDriverBOY : public ScreenReaderDriver {
public:
    ScreenReaderDriverBOY();
    ~ScreenReaderDriverBOY();
    bool Speak(const wchar_t *str, bool interrupt) override;
    bool Braille(const wchar_t *str) override;
    bool IsSpeaking() override;
    bool Silence() override;
    bool IsActive() override;
    bool Output(const wchar_t *str, bool interrupt) override;
    static int g_speakCompleteReason;
    static void __stdcall SpeakCompleteCallback(int reason);
private:
 typedef int  (__stdcall *BoyCtrlInitialize)(const wchar_t* pathName);
    typedef void (__stdcall *BoyCtrlUninitialize)();
    typedef bool (__stdcall *BoyCtrlIsReaderRunning)();
    typedef int  (__stdcall *BoyCtrlSpeak)(const wchar_t* text, bool append, BoyCtrlSpeakCompleteFunc onCompletion);
    typedef int  (__stdcall *BoyCtrlStopSpeaking)();
    BoyCtrlInitialize BoyInit;
    BoyCtrlUninitialize BoyUninit;
    BoyCtrlIsReaderRunning BoyIsRunning;
    BoyCtrlSpeak BoySpeak;
    BoyCtrlStopSpeaking BoyStopSpeak;
};

#endif
