/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.h
 *  Description:    Driver for the Boy screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */
#ifndef _SCREEN_READER_DRIVER_BOY_H_
#define _SCREEN_READER_DRIVER_BOY_H_
#include <windows.h>
#include "ScreenReaderDriver.h"
// BoyCtrl Error Codes (per official documentation)
enum BoyCtrlError {
    e_bcerr_success = 0,        // Operation successful
    e_bcerr_fail = 1,           // Operation failed
    e_bcerr_arg = 2,            // Invalid argument
    e_bcerr_unavailable = 3     // Service unavailable (reader not running)
};
// BoyCtrl Speak Complete Callback Reason (per official documentation)
enum BoyCtrlCompleteReason {
    e_bccr_normal = 1,          // Completed normally
    e_bccr_interrupted = 2,     // Interrupted by new speak task
    e_bccr_stopped = 3          // Stopped by StopSpeaking API
};
// BoyCtrl Function Types
typedef int  (__stdcall *BoyCtrlInitialize)(const wchar_t* logPath);
typedef void (__stdcall *BoyCtrlUninitialize)();
typedef bool (__stdcall *BoyCtrlIsReaderRunning)();
typedef int  (__stdcall *BoyCtrlGetReaderState)();
typedef void (__stdcall *BoyCtrlSpeakCompleteFunc)(int reason);
typedef int  (__stdcall *BoyCtrlSpeak)(const wchar_t* text, bool append, BoyCtrlSpeakCompleteFunc onCompletion);
typedef int  (__stdcall *BoyCtrlStopSpeaking)();
class ScreenReaderDriverBOY : public ScreenReaderDriver
{
public:
    ScreenReaderDriverBOY();
    ~ScreenReaderDriverBOY() override;
    bool Speak(const wchar_t *str, bool interrupt) override;
    bool Braille(const wchar_t *str) override;
    bool IsSpeaking() override;
    bool Silence() override;
    bool IsActive() override;
    bool Output(const wchar_t *str, bool interrupt) override;
    static void __stdcall SpeakCompleteCallback(int reason);
private:
    HINSTANCE controller;
    SRWLOCK srwLock;  // Performance: SRWLock instead of CRITICAL_SECTION
    volatile LONG isSpeaking;
    volatile LONG speakCompleteReason;
    // Performance: IsActive result cache (100ms timeout, avoids frequent cross-process calls)
    static const DWORD CACHE_TIMEOUT_MS = 100;
    DWORD lastIsActiveTime;
    bool cachedIsActive;
    BoyCtrlInitialize BoyInit;
    BoyCtrlUninitialize BoyUninit;
    BoyCtrlIsReaderRunning BoyIsRunning;
    BoyCtrlGetReaderState BoyGetState;
    BoyCtrlSpeak BoySpeak;
    BoyCtrlStopSpeaking BoyStopSpeak;
    static ScreenReaderDriverBOY* g_instance;
};
#endif
