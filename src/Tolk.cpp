/**
 *  Product:        Tolk
 *  File:           Tolk.cpp
 *  Description:    C-style DLL exports.
 *  Copyright:      (c) 2014-2016, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include <windows.h>
#include <vector>
#include <memory>
#include "Tolk.h"
#include "TolkDebug.h"
#include "ScreenReaderDriverBOY.h"
#include "ScreenReaderDriverJAWS.h"
#include "ScreenReaderDriverNVDA.h"
#include "ScreenReaderDriverSA.h"
#include "ScreenReaderDriverSAPI.h"
#include "ScreenReaderDriverSNova.h"
#include "ScreenReaderDriverWE.h"
#include "ScreenReaderDriverZDSR.h"
#include "ScreenReaderDriverZT.h"
static CRITICAL_SECTION g_cs;
static bool g_comInitializedByUs = false;
static bool g_isLoaded = false;
static volatile LONG g_lastError = 0;  // Internal error code for debugging
static std::vector<std::unique_ptr<ScreenReaderDriver>> g_screenReaderDrivers;
static std::unique_ptr<ScreenReaderDriverSAPI> g_sapi;
static ScreenReaderDriver *g_currentScreenReaderDriver = nullptr;
static bool g_trySAPI = true;
static bool g_preferSAPI = false;

// Internal error codes (for debugging only, not exposed in public API)
enum TolkInternalError {
    TOLK_ERR_NONE = 0,
    TOLK_ERR_LOAD_EXCEPTION = 1,    // Exception during driver initialization
    TOLK_ERR_COM_INIT_FAILED = 2    // COM initialization failed
};
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    InitializeCriticalSection(&g_cs);
    break;
  case DLL_PROCESS_DETACH:
    DeleteCriticalSection(&g_cs);
    break;
  }
  return TRUE;
}
extern "C" {
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Load() {
  EnterCriticalSection(&g_cs);
  TOLK_LOG_INFO("Tolk_Load() called");
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (hr == S_OK) {
    g_comInitializedByUs = true;
    TOLK_LOG_INFO("COM initialized successfully");
  }
  else if (hr == S_FALSE) {
    // COM was already initialized on this thread, undo our extra reference.
    CoUninitialize();
    TOLK_LOG_INFO("COM already initialized, extra reference released");
  }
  else {
    TOLK_LOG_ERROR("CoInitializeEx failed, hr=0x%08X", hr);
    InterlockedExchange(&g_lastError, TOLK_ERR_COM_INIT_FAILED);
  }
  if (Tolk_IsLoaded()) {
    TOLK_LOG_INFO("Tolk already loaded, skipping initialization");
    LeaveCriticalSection(&g_cs);
    return;
  }
  try {
    TOLK_LOG_INFO("Initializing screen reader drivers...");
    // Priority order: most popular screen readers first (global market share)
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverNVDA>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverJAWS>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverWE>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverSA>());
#ifndef _WIN64
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverSNova>());
#else
    TOLK_LOG_INFO("SuperNova driver skipped (64-bit not supported)");
#endif
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverZT>());
    // Chinese screen readers (regional market)
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverZDSR>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBOY>());
    if (g_trySAPI) {
      TOLK_LOG_INFO("Initializing SAPI fallback driver");
      g_sapi = std::make_unique<ScreenReaderDriverSAPI>();
    }
    TOLK_LOG_INFO("All drivers initialized successfully, total=%d", (int)g_screenReaderDrivers.size());
  }
  catch (...) {
    TOLK_LOG_ERROR("EXCEPTION during driver initialization!");
    // Record error for debugging (can be inspected via debugger)
    InterlockedExchange(&g_lastError, TOLK_ERR_LOAD_EXCEPTION);
    g_sapi.reset();
    g_screenReaderDrivers.clear();
    LeaveCriticalSection(&g_cs);
    return;
  }
  g_isLoaded = true;
  TOLK_LOG_INFO("Tolk loaded successfully");
  LeaveCriticalSection(&g_cs);
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsLoaded() {
  return g_isLoaded;
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Unload() {
  EnterCriticalSection(&g_cs);
  TOLK_LOG_INFO("Tolk_Unload() called");
  if (Tolk_IsLoaded()) {
    TOLK_LOG_INFO("Unloading all drivers");
    g_isLoaded = false;
    g_currentScreenReaderDriver = nullptr;
    g_sapi.reset();
    g_screenReaderDrivers.clear();
  }
  if (g_comInitializedByUs) {
    TOLK_LOG_INFO("Uninitializing COM");
    CoUninitialize();
    g_comInitializedByUs = false;
  }
  TOLK_LOG_INFO("Tolk unloaded successfully");
  LeaveCriticalSection(&g_cs);
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {
  EnterCriticalSection(&g_cs);
  if (g_trySAPI == trySAPI) {
    LeaveCriticalSection(&g_cs);
    return;
  }
  g_trySAPI = trySAPI;
  if (Tolk_IsLoaded()) {
    if (g_trySAPI && !g_sapi)
      g_sapi = std::make_unique<ScreenReaderDriverSAPI>();
    else if (!g_trySAPI && g_sapi)
      g_sapi.reset();
    g_currentScreenReaderDriver = nullptr;
  }
  LeaveCriticalSection(&g_cs);
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
  EnterCriticalSection(&g_cs);
  if (g_preferSAPI == preferSAPI) {
    LeaveCriticalSection(&g_cs);
    return;
  }
  g_preferSAPI = preferSAPI;
  if (Tolk_IsLoaded() && g_trySAPI && g_sapi)
    g_currentScreenReaderDriver = nullptr;
  LeaveCriticalSection(&g_cs);
}
TOLK_DLL_DECLSPEC const wchar_t * TOLK_CALL Tolk_DetectScreenReader() {
  EnterCriticalSection(&g_cs);
  if (!Tolk_IsLoaded()) {
    LeaveCriticalSection(&g_cs);
    return nullptr;
  }
  if (g_currentScreenReaderDriver && (g_preferSAPI || g_currentScreenReaderDriver != g_sapi.get()) && g_currentScreenReaderDriver->IsActive()) {
    const wchar_t *name = g_currentScreenReaderDriver->GetName();
    LeaveCriticalSection(&g_cs);
    return name;
  }
  if (g_trySAPI && g_preferSAPI && g_sapi && g_sapi->IsActive()) {
    g_currentScreenReaderDriver = g_sapi.get();
    const wchar_t *name = g_currentScreenReaderDriver->GetName();
    LeaveCriticalSection(&g_cs);
    return name;
  }
  for (const auto &driver : g_screenReaderDrivers) {
    if (driver.get() != g_currentScreenReaderDriver && driver->IsActive()) {
      g_currentScreenReaderDriver = driver.get();
      const wchar_t *name = g_currentScreenReaderDriver->GetName();
      LeaveCriticalSection(&g_cs);
      return name;
    }
  }
  if (g_trySAPI && !g_preferSAPI && g_sapi && g_sapi->IsActive()) {
    g_currentScreenReaderDriver = g_sapi.get();
    const wchar_t *name = g_currentScreenReaderDriver->GetName();
    LeaveCriticalSection(&g_cs);
    return name;
  }
  g_currentScreenReaderDriver = nullptr;
  LeaveCriticalSection(&g_cs);
  return nullptr;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasSpeech() {
  EnterCriticalSection(&g_cs);
  if (Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->HasSpeech();
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasBraille() {
  EnterCriticalSection(&g_cs);
  if (Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->HasBraille();
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt) {
  EnterCriticalSection(&g_cs);
  if (str && Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->Output(str, interrupt);
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt) {
  EnterCriticalSection(&g_cs);
  if (str && Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->Speak(str, interrupt);
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Braille(const wchar_t *str) {
  EnterCriticalSection(&g_cs);
  if (str && Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->Braille(str);
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsSpeaking() {
  EnterCriticalSection(&g_cs);
  if (Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->IsSpeaking();
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Silence() {
  EnterCriticalSection(&g_cs);
  if (Tolk_DetectScreenReader()) {
    bool result = g_currentScreenReaderDriver->Silence();
    LeaveCriticalSection(&g_cs);
    return result;
  }
  LeaveCriticalSection(&g_cs);
  return false;
}
} // extern "C"
