/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSAPI.h
 *  Description:    Driver for the Microsoft Speech API (SAPI) - Optimized for Windows 10/11
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#ifndef _SCREEN_READER_DRIVER_SAPI_H_
#define _SCREEN_READER_DRIVER_SAPI_H_

#include <sapi.h>
#include <windows.h>
#include "ScreenReaderDriver.h"

class ScreenReaderDriverSAPI : public ScreenReaderDriver {
public:
    ScreenReaderDriverSAPI();
    ~ScreenReaderDriverSAPI() override;

public:
    bool Speak(const wchar_t *str, bool interrupt) override;
    bool Braille(const wchar_t *) override { return false; }
    bool IsSpeaking() override;
    bool Silence() override;
    bool IsActive() override { return (!!controller); }
    bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
    void Initialize();
    void Finalize();
    bool Recover();  // Auto-recovery mechanism

private:
    ISpVoice *controller;
    SRWLOCK srwLock;          // Performance: SRWLock instead of CRITICAL_SECTION
    volatile LONG isSpeaking; // Atomic flag for fast speaking detection
    volatile LONG recoverCount; // Recovery counter to prevent infinite loops
};

#endif // _SCREEN_READER_DRIVER_SAPI_H_