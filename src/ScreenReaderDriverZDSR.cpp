/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZDSR.cpp
 *  Description:    Driver for the ZDSR screen reader.
 *  Copyright:      (c) 2022, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

// The ZDSR Project provides a header and libraries,
// but we don't use these in order to support running even if the DLL is missing.

#include "ScreenReaderDriverZDSR.h"

ScreenReaderDriverZDSR::ScreenReaderDriverZDSR() :
  ScreenReaderDriver(L"ZDSR", true, true),
  #ifdef _WIN64
  controller(LoadLibrary(L"ZDSRAPI_x64.dll")),
  #else
  controller(LoadLibrary(L"ZDSRAPI.dll")),
  #endif
  zdsrInitTTS(nullptr),
  zdsrGetSpeakState(nullptr),
  zdsrSpeak(nullptr),
  zdsrStopSpeak(nullptr),
  zdsrBraille(nullptr)
{
  if (controller) {
    zdsrInitTTS      = (ZDSR_InitTTS)GetProcAddress(controller, "InitTTS");
    zdsrGetSpeakState= (ZDSR_GetSpeakState)GetProcAddress(controller, "GetSpeakState");
    zdsrSpeak        = (ZDSR_Speak)GetProcAddress(controller, "Speak");
    zdsrStopSpeak    = (ZDSR_StopSpeak)GetProcAddress(controller, "StopSpeak");
    zdsrBraille      = (ZDSR_Braille)GetProcAddress(controller, "Braille");

    if (zdsrInitTTS) zdsrInitTTS(0, nullptr, TRUE);
  }
}

ScreenReaderDriverZDSR::~ScreenReaderDriverZDSR() {
  if (controller) FreeLibrary(controller);
}

bool ScreenReaderDriverZDSR::Speak(const wchar_t *str, bool interrupt) {
  if (zdsrSpeak) return (zdsrSpeak(str, interrupt) == 0);
  return false;
}

bool ScreenReaderDriverZDSR::Braille(const wchar_t *str) {
  if (zdsrBraille) return (zdsrBraille(str, FALSE) == 0);
  return false;
}

bool ScreenReaderDriverZDSR::Silence() {
  if (zdsrStopSpeak) {
    zdsrStopSpeak();
    return true;
  }
  return false;
}

bool ScreenReaderDriverZDSR::IsSpeaking() {
  if (zdsrGetSpeakState) return (zdsrGetSpeakState() == 3);
  return false;
}

bool ScreenReaderDriverZDSR::IsActive() {
  if (zdsrGetSpeakState) {
    int state = zdsrGetSpeakState();
    return (state != 1 && state != 2);
  }
  return false;
}

bool ScreenReaderDriverZDSR::Output(const wchar_t *str, bool interrupt) {
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}