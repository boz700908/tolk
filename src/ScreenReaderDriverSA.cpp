/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSA.cpp
 *  Description:    Driver for the System Access screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverSA.h"
#include "TolkDebug.h"
ScreenReaderDriverSA::ScreenReaderDriverSA() :
  ScreenReaderDriver(L"System Access", true, true),
  controller(nullptr),
  lastIsActiveTime(0),
  cachedIsActive(false),
  sa_SayW(nullptr),
  sa_BrlShowTextW(nullptr),
  sa_StopAudio(nullptr),
  sa_IsRunning(nullptr)
{
#ifdef _WIN64
  TOLK_LOG_INFO("SA: Loading 64-bit SAAPI64.dll");
  controller = LoadLibrary(L"SAAPI64.dll");
#else
  TOLK_LOG_INFO("SA: Loading 32-bit SAAPI32.dll");
  controller = LoadLibrary(L"SAAPI32.dll");
#endif
  if (!controller) {
    TOLK_LOG_WARN("SA: DLL not found, driver disabled");
    return;
  }
  TOLK_LOG_INFO("SA: DLL loaded successfully");
  sa_SayW = (SA_SayW)GetProcAddress(controller, "SA_SayW");
  sa_BrlShowTextW = (SA_BrlShowTextW)GetProcAddress(controller, "SA_BrlShowTextW");
  sa_StopAudio = (SA_StopAudio)GetProcAddress(controller, "SA_StopAudio");
  sa_IsRunning = (SA_IsRunning)GetProcAddress(controller, "SA_IsRunning");
  int loadedCount = (sa_SayW?1:0) + (sa_BrlShowTextW?1:0) +
                    (sa_StopAudio?1:0) + (sa_IsRunning?1:0);
  TOLK_LOG_INFO("SA: Loaded %d/4 API functions", loadedCount);
}
ScreenReaderDriverSA::~ScreenReaderDriverSA() {
  if (controller) {
    TOLK_LOG_INFO("SA: Unloading DLL");
    FreeLibrary(controller);
  }
}
bool ScreenReaderDriverSA::Speak(const wchar_t *str, bool interrupt) {
  if (interrupt && !Silence()) return false;
  if (sa_SayW) return sa_SayW(str);
  return false;
}
bool ScreenReaderDriverSA::Braille(const wchar_t *str) {
  if (sa_BrlShowTextW) return sa_BrlShowTextW(str);
  return false;
}
bool ScreenReaderDriverSA::Silence() {
  if (sa_StopAudio) return sa_StopAudio();
  return false;
}
bool ScreenReaderDriverSA::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < 100) {
    return cachedIsActive;
  }

  if (sa_IsRunning) {
    cachedIsActive = sa_IsRunning();
  } else {
    cachedIsActive = false;
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
bool ScreenReaderDriverSA::Output(const wchar_t *str, bool interrupt) {
  // Beware short-circuiting.
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
