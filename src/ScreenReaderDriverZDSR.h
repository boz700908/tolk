/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZDSR.h
 *  Description:    Driver for the ZDSR screen reader.
 *  Copyright:      (c) 2022, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_ZDSR_H_
#define _SCREEN_READER_DRIVER_ZDSR_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

class ScreenReaderDriverZDSR : public ScreenReaderDriver
{
public:
    ScreenReaderDriverZDSR();
    ~ScreenReaderDriverZDSR() override;

public:
    bool Speak(const wchar_t *str, bool interrupt) override;
    bool Braille(const wchar_t *str) override;
    bool IsSpeaking() override;
    bool Silence() override;
    bool IsActive() override;
    bool Output(const wchar_t *str, bool interrupt) override;

private:
    typedef int (WINAPI *InitTTS)(int type, const WCHAR* channelName, BOOL bKeyDownInterrupt);
    typedef int (WINAPI *GetSpeakState)(void);
    typedef int (WINAPI *Speak)(const WCHAR* text, BOOL bInterrupt);
    typedef void (WINAPI *StopSpeak)(void);
    typedef int (WINAPI *Braille)(const WCHAR* text, BOOL bFlashMessage);

private:
    HINSTANCE controller;
    InitTTS zdsrInitTTS;
    GetSpeakState zdsrGetSpeakState;
    Speak zdsrSpeak;
    StopSpeak zdsrStopSpeak;
    Braille zdsrBraille;
};

#endif // _SCREEN_READER_DRIVER_ZDSR_H_

