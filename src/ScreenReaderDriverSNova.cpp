/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSNova.cpp
 *  Description:    Driver for the SuperNova screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverSNova.h"
#include "TolkDebug.h"
#define DOLACCESS_NONE 0
#define DOLACCESS_HAL 1
#define DOLACCESS_SUPERNOVA 4
#define DOLACCESS_LUNARPLUS 8
#define DOLACCESS_SUCCESS 0
#define DOLACCESS_FAIL_NO_SERVER 1
#define DOLACCESS_FAIL_INVALID_ID 2
#define DOLACCESS_FAIL_BAD_LEN 3
#define DOLACCESS_FAIL_INVALID_ACTION 4
#define DOLACCESS_FAIL_NOT_SUPPORTED_BY_SERVER 5
#define DOLACCESS_FAIL_BAD_STRING 6
#define DOLACCESS_FAIL_TIMED_OUT 7
#define DOLACCESS_FAIL_READ_ONLY 8
#define DOLACCESS_FAIL_SERVER_BUSY 9
#define DOLAPI_COMMAND_SPEAK 1
#define CMD_mute_ 141
ScreenReaderDriverSNova::ScreenReaderDriverSNova() :
  ScreenReaderDriver(L"SuperNova", true, false),
  controller(nullptr),
  dolAccess_GetSystem(nullptr),
  dolAccess_Action(nullptr),
  dolAccess_Command(nullptr)
{
#ifndef _WIN64
  TOLK_LOG_INFO("SNova: Loading 32-bit dolapi32.dll");
  controller = LoadLibrary(L"dolapi32.dll");
  if (!controller) {
    TOLK_LOG_WARN("SNova: DLL not found, driver disabled");
    return;
  }
  TOLK_LOG_INFO("SNova: DLL loaded successfully");
  dolAccess_GetSystem = (DolAccess_GetSystem)GetProcAddress(controller, "_DolAccess_GetSystem@0");
  dolAccess_Action = (DolAccess_Action)GetProcAddress(controller, "_DolAccess_Action@4");
  dolAccess_Command = (DolAccess_Command)GetProcAddress(controller, "_DolAccess_Command@12");
  int loadedCount = (dolAccess_GetSystem?1:0) + (dolAccess_Action?1:0) + (dolAccess_Command?1:0);
  TOLK_LOG_INFO("SNova: Loaded %d/3 API functions", loadedCount);
#else
  TOLK_LOG_INFO("SNova: Skipped - 64-bit not supported by Dolphin SuperNova");
#endif
}
ScreenReaderDriverSNova::~ScreenReaderDriverSNova() {
  if (controller) {
    TOLK_LOG_INFO("SNova: Unloading DLL");
    FreeLibrary(controller);
  }
}
bool ScreenReaderDriverSNova::Speak(const wchar_t* str, bool interrupt) {
  if (interrupt && !Silence()) return false;
  if (dolAccess_Command)
    return (dolAccess_Command(str, (int)(wcslen(str) + 1) * sizeof(wchar_t), DOLAPI_COMMAND_SPEAK) == DOLACCESS_SUCCESS);
  return false;
}
bool ScreenReaderDriverSNova::Silence() {
  if (dolAccess_Action)
    return (dolAccess_Action(CMD_mute_) == DOLACCESS_SUCCESS);
  return false;
}
bool ScreenReaderDriverSNova::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < 100) {
    return cachedIsActive;
  }

  if (dolAccess_GetSystem) {
    const int result = dolAccess_GetSystem();
    cachedIsActive = (result == DOLACCESS_HAL || result == DOLACCESS_SUPERNOVA || result == DOLACCESS_LUNARPLUS);
  } else {
    cachedIsActive = false;
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
