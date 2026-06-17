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
// ZDSR API state constants (per official documentation: https://www.zdsr.com/docs/api/zdsr-api/)
enum ZDSR_State {
    ZDSR_STATE_VERSION_MISMATCH = 1,     // Version mismatch
    ZDSR_STATE_NOT_RUNNING = 2,          // ZDSR not running or not authorized
    ZDSR_STATE_SPEAKING = 3,             // Currently speaking
    ZDSR_STATE_IDLE = 4,                 // Not speaking (ZDSR running but idle)
    ZDSR_STATE_COMMAND_FAILED = 8,       // Command not delivered successfully
    ZDSR_STATE_CHANNEL_NOT_READY = 9     // Independent channel not ready
};
class ScreenReaderDriverZDSR : public ScreenReaderDriver {
public:
  ScreenReaderDriverZDSR();
  ~ScreenReaderDriverZDSR() override;
public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *str) override;
  bool IsSpeaking() override;
  bool Silence() override;
  bool IsActive() override;
private:
  typedef int (WINAPI *ZDSR_InitTTS)(int type, const WCHAR* channelName, BOOL bKeyDownInterrupt);
  typedef int (WINAPI *ZDSR_GetSpeakState)(void);
  typedef int (WINAPI *ZDSR_Speak)(const WCHAR* text, BOOL bInterrupt);
  typedef void (WINAPI *ZDSR_StopSpeak)(void);
  typedef int (WINAPI *ZDSR_Braille)(const WCHAR* text, BOOL bFlashMessage);
private:
  HINSTANCE controller;
  ZDSR_InitTTS     zdsrInitTTS;
  ZDSR_GetSpeakState zdsrGetSpeakState;
  ZDSR_Speak       zdsrSpeak;
  ZDSR_StopSpeak   zdsrStopSpeak;
  ZDSR_Braille     zdsrBraille;
};
#endif // _SCREEN_READER_DRIVER_ZDSR_H_
