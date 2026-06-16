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
#include "TolkDebug.h"
ScreenReaderDriverZDSR::ScreenReaderDriverZDSR() :
  ScreenReaderDriver(L"ZDSR", true, true),
  controller(nullptr),
  zdsrInitTTS(nullptr),
  zdsrGetSpeakState(nullptr),
  zdsrSpeak(nullptr),
  zdsrStopSpeak(nullptr),
  zdsrBraille(nullptr)
{
#ifdef _WIN64
  TOLK_LOG_INFO("ZDSR: Loading 64-bit ZDSRAPI_x64.dll");
  controller = LoadLibrary(L"ZDSRAPI_x64.dll");
#else
  TOLK_LOG_INFO("ZDSR: Loading 32-bit ZDSRAPI.dll");
  controller = LoadLibrary(L"ZDSRAPI.dll");
#endif
  if (!controller) {
    TOLK_LOG_WARN("ZDSR: DLL not found, driver disabled");
    return;
  }
  TOLK_LOG_INFO("ZDSR: DLL loaded successfully");
  zdsrInitTTS      = (ZDSR_InitTTS)GetProcAddress(controller, "InitTTS");
  zdsrGetSpeakState= (ZDSR_GetSpeakState)GetProcAddress(controller, "GetSpeakState");
  zdsrSpeak        = (ZDSR_Speak)GetProcAddress(controller, "Speak");
  zdsrStopSpeak    = (ZDSR_StopSpeak)GetProcAddress(controller, "StopSpeak");
  zdsrBraille      = (ZDSR_Braille)GetProcAddress(controller, "Braille");
  int loadedCount = (zdsrInitTTS?1:0) + (zdsrGetSpeakState?1:0) + (zdsrSpeak?1:0) +
                    (zdsrStopSpeak?1:0) + (zdsrBraille?1:0);
  TOLK_LOG_INFO("ZDSR: Loaded %d/5 API functions", loadedCount);
  if (zdsrInitTTS) {
    TOLK_LOG_INFO("ZDSR: Calling InitTTS");
    zdsrInitTTS(0, nullptr, TRUE);
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
  // State 3 = ZDSR_STATE_SPEAKING (actively speaking)
  // Per official docs: 3 = speaking, 4 = not speaking
  if (zdsrGetSpeakState) return (zdsrGetSpeakState() == ZDSR_STATE_SPEAKING);
  return false;
}
bool ScreenReaderDriverZDSR::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < 100) {
    return cachedIsActive;
  }

  // Per official documentation:
  // State 3 = ZDSR_STATE_SPEAKING (speaking)
  // State 4 = ZDSR_STATE_IDLE (not speaking but ZDSR is running)
  // Both 3 and 4 indicate ZDSR is loaded and operational
  // States 1, 2, 8, 9 = error / not running
  if (zdsrGetSpeakState) {
    int state = zdsrGetSpeakState();
    cachedIsActive = (state == ZDSR_STATE_SPEAKING || state == ZDSR_STATE_IDLE);
  } else {
    cachedIsActive = false;
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
bool ScreenReaderDriverZDSR::Output(const wchar_t *str, bool interrupt) {
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
