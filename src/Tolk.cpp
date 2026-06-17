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

// Performance: SRWLock instead of CRITICAL_SECTION (lighter, supports read/write separation)
static SRWLOCK g_srwLock = SRWLOCK_INIT;

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

// Cached screen reader name for Tolk_DetectScreenReader().
// Using a const wchar_t* (points to driver's static string) avoids UAF:
// the driver's name string outlives any cached pointer because it's a
// compile-time constant stored in the driver's data segment.
static const wchar_t *g_cachedName = nullptr;
static DWORD g_lastDetectTime = 0;
static const DWORD CACHE_TIMEOUT_MS = 100;

// Internal: detect active screen reader. Caller MUST hold g_srwLock.
// Returns the driver name, or nullptr if none detected.
// Updates g_currentScreenReaderDriver and the name cache.
static const wchar_t * DetectCurrentScreenReader() {
  DWORD currentTime = GetTickCount();
  // Check name cache (valid for CACHE_TIMEOUT_MS)
  if (g_cachedName && (currentTime - g_lastDetectTime) < CACHE_TIMEOUT_MS) {
    return g_cachedName;
  }

  if (g_currentScreenReaderDriver && (g_preferSAPI || g_currentScreenReaderDriver != g_sapi.get()) && g_currentScreenReaderDriver->IsActive()) {
    g_cachedName = g_currentScreenReaderDriver->GetName();
    g_lastDetectTime = currentTime;
    return g_cachedName;
  }
  if (g_trySAPI && g_preferSAPI && g_sapi && g_sapi->IsActive()) {
    g_currentScreenReaderDriver = g_sapi.get();
    g_cachedName = g_currentScreenReaderDriver->GetName();
    g_lastDetectTime = currentTime;
    return g_cachedName;
  }
  for (const auto &driver : g_screenReaderDrivers) {
    if (driver.get() != g_currentScreenReaderDriver && driver->IsActive()) {
      g_currentScreenReaderDriver = driver.get();
      g_cachedName = g_currentScreenReaderDriver->GetName();
      g_lastDetectTime = currentTime;
      return g_cachedName;
    }
  }
  if (g_trySAPI && !g_preferSAPI && g_sapi && g_sapi->IsActive()) {
    g_currentScreenReaderDriver = g_sapi.get();
    g_cachedName = g_currentScreenReaderDriver->GetName();
    g_lastDetectTime = currentTime;
    return g_cachedName;
  }
  g_currentScreenReaderDriver = nullptr;
  g_cachedName = nullptr;
  g_lastDetectTime = currentTime;
  return nullptr;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
  // SRWLock is statically initialized, no need to init/destroy in DllMain
  (void)reason;
  return TRUE;
}
extern "C" {
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Load() {
  AcquireSRWLockExclusive(&g_srwLock);
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
    ReleaseSRWLockExclusive(&g_srwLock);
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
    InterlockedExchange(&g_lastError, TOLK_ERR_LOAD_EXCEPTION);
    g_sapi.reset();
    g_screenReaderDrivers.clear();
    ReleaseSRWLockExclusive(&g_srwLock);
    return;
  }
  g_isLoaded = true;
  g_lastDetectTime = 0;
  g_cachedName = nullptr;
  TOLK_LOG_INFO("Tolk loaded successfully");
  ReleaseSRWLockExclusive(&g_srwLock);
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsLoaded() {
  return g_isLoaded;
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Unload() {
  AcquireSRWLockExclusive(&g_srwLock);
  TOLK_LOG_INFO("Tolk_Unload() called");
  if (Tolk_IsLoaded()) {
    TOLK_LOG_INFO("Unloading all drivers");
    g_isLoaded = false;
    g_currentScreenReaderDriver = nullptr;
    g_sapi.reset();
    g_screenReaderDrivers.clear();
    g_lastDetectTime = 0;
    g_cachedName = nullptr;
  }
  if (g_comInitializedByUs) {
    TOLK_LOG_INFO("Uninitializing COM");
    CoUninitialize();
    g_comInitializedByUs = false;
  }
  TOLK_LOG_INFO("Tolk unloaded successfully");
  ReleaseSRWLockExclusive(&g_srwLock);
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {
  AcquireSRWLockExclusive(&g_srwLock);
  if (g_trySAPI == trySAPI) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return;
  }
  g_trySAPI = trySAPI;
  if (Tolk_IsLoaded()) {
    if (g_trySAPI && !g_sapi)
      g_sapi = std::make_unique<ScreenReaderDriverSAPI>();
    else if (!g_trySAPI && g_sapi)
      g_sapi.reset();
    g_currentScreenReaderDriver = nullptr;
    g_lastDetectTime = 0;
    g_cachedName = nullptr;
  }
  ReleaseSRWLockExclusive(&g_srwLock);
}
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
  AcquireSRWLockExclusive(&g_srwLock);
  if (g_preferSAPI == preferSAPI) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return;
  }
  g_preferSAPI = preferSAPI;
  if (Tolk_IsLoaded() && g_trySAPI && g_sapi) {
    g_currentScreenReaderDriver = nullptr;
    g_lastDetectTime = 0;
    g_cachedName = nullptr;
  }
  ReleaseSRWLockExclusive(&g_srwLock);
}
TOLK_DLL_DECLSPEC const wchar_t * TOLK_CALL Tolk_DetectScreenReader() {
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return nullptr;
  }
  const wchar_t *name = DetectCurrentScreenReader();
  ReleaseSRWLockExclusive(&g_srwLock);
  return name;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasSpeech() {
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->HasSpeech();
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasBraille() {
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->HasBraille();
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt) {
  if (!str) return false;
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->Output(str, interrupt);
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt) {
  if (!str) return false;
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->Speak(str, interrupt);
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Braille(const wchar_t *str) {
  if (!str) return false;
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->Braille(str);
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsSpeaking() {
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->IsSpeaking();
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Silence() {
  AcquireSRWLockExclusive(&g_srwLock);
  if (!Tolk_IsLoaded() || !DetectCurrentScreenReader()) {
    ReleaseSRWLockExclusive(&g_srwLock);
    return false;
  }
  bool result = g_currentScreenReaderDriver->Silence();
  ReleaseSRWLockExclusive(&g_srwLock);
  return result;
}
} // extern "C"
