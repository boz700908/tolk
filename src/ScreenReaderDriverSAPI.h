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
    bool Recover();  // 自动恢复机制

private:
    ISpVoice *controller;
    SRWLOCK srwLock;          // 性能优化：SRWLock代替CRITICAL_SECTION
    volatile LONG isSpeaking; // 原子变量，快速检测语音状态
    volatile LONG recoverCount; // 恢复计数，防止无限恢复

    // SAPI 5.4 优化：语音参数缓存
    LONG defaultRate;
    LONG defaultVolume;
};

#endif // _SCREEN_READER_DRIVER_SAPI_H_