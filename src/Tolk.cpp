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
#include "ScreenReaderDriverBridged.h"
#include "ScreenReaderDriverJAWS.h"
#include "ScreenReaderDriverNVDA.h"
#include "ScreenReaderDriverSA.h"
#include "ScreenReaderDriverSAPI.h"
#include "ScreenReaderDriverSNova.h"
#include "ScreenReaderDriverWE.h"
#include "ScreenReaderDriverZDCloud.h"
#include "ScreenReaderDriverZDSR.h"
#include "ScreenReaderDriverZT.h"
#include "ScreenReaderDriverOneCore.h"
#include "ScreenReaderDriverPCTalker.h"
#include "ScreenReaderDriverSenseReader.h"
#include "ScreenReaderDriverUIA.h"

// Performance: SRWLock instead of CRITICAL_SECTION (lighter, supports read/write separation)
static SRWLOCK g_srwLock = SRWLOCK_INIT;

static bool g_comInitializedByUs = false;
static bool g_isLoaded = false;
static volatile LONG g_lastError = 0;  // Internal error code for debugging
static std::vector<std::unique_ptr<ScreenReaderDriver>> g_screenReaderDrivers;
static std::unique_ptr<ScreenReaderDriverSAPI> g_sapi;
static std::unique_ptr<ScreenReaderDriverOneCore> g_oneCore;
static ScreenReaderDriver *g_currentScreenReaderDriver = nullptr;
// The UIA driver, kept separately so detection can tell it apart from the
// named screen readers.
static ScreenReaderDriver *g_uiaDriver = nullptr;
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

  const bool currentIsFallback = g_currentScreenReaderDriver != nullptr &&
    (g_currentScreenReaderDriver == g_sapi.get() || g_currentScreenReaderDriver == g_oneCore.get());
  // UIA reports itself active whenever the Windows screen-reader flag is set
  // and any UIA client is listening. That can also describe a screen reader
  // that has its own driver, and those signals can outlive the reader, so do
  // not latch onto UIA: re-scan every call to let a named screen reader that
  // appears later take over and to fall through once UIA really stops.
  const bool currentIsUia = g_currentScreenReaderDriver != nullptr && g_currentScreenReaderDriver == g_uiaDriver;
  if (g_currentScreenReaderDriver && !currentIsUia && (g_preferSAPI || !currentIsFallback) && g_currentScreenReaderDriver->IsActive()) {
    g_cachedName = g_currentScreenReaderDriver->GetName();
    g_lastDetectTime = currentTime;
    return g_cachedName;
  }
  // OneCore and SAPI form the fallback speech tier. They are enabled together
  // through Tolk_TrySAPI and moved together through Tolk_PreferSAPI, and
  // OneCore, the newer engine, is always tried before SAPI.
  const auto selectFallback = [&]() -> bool {
    if (!g_trySAPI) return false;
    if (g_oneCore && g_oneCore->IsActive()) {
      g_currentScreenReaderDriver = g_oneCore.get();
    }
    else if (g_sapi && g_sapi->IsActive()) {
      g_currentScreenReaderDriver = g_sapi.get();
    }
    else {
      return false;
    }
    g_cachedName = g_currentScreenReaderDriver->GetName();
    g_lastDetectTime = currentTime;
    return true;
  };
  if (g_preferSAPI && selectFallback()) {
    return g_cachedName;
  }
  // The current driver is deliberately not skipped: when it is UIA the scan
  // must be able to select it again after no named driver matched.
  for (const auto &driver : g_screenReaderDrivers) {
    if (driver->IsActive()) {
      g_currentScreenReaderDriver = driver.get();
      g_cachedName = g_currentScreenReaderDriver->GetName();
      g_lastDetectTime = currentTime;
      return g_cachedName;
    }
  }
  if (!g_preferSAPI && selectFallback()) {
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
#if TOLK_CAN_LOAD_X86 || TOLK_CAN_LOAD_X64
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverJAWS>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverWE>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverSA>());
#else
    // These backends have no ARM64 build, so a 64-bit helper process hosts
    // them on ARM64.
    TOLK_LOG_INFO("JAWS/Window-Eyes/System Access: using the 64-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"JAWS", true, true, TolkBridgeBackendJAWS));
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"Window-Eyes", true, true, TolkBridgeBackendWE));
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"System Access", true, true, TolkBridgeBackendSA));
#endif
#if TOLK_CAN_LOAD_X86
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverSNova>());
#else
    TOLK_LOG_INFO("SuperNova: using the 32-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"SuperNova", true, false, TolkBridgeBackendSNova));
#endif
#if TOLK_CAN_LOAD_X86 || TOLK_CAN_LOAD_X64
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverZT>());
#else
    TOLK_LOG_INFO("ZoomText: using the 64-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"ZoomText", true, false, TolkBridgeBackendZT));
#endif
    // Chinese screen readers (regional market)
#if TOLK_CAN_LOAD_X86 || TOLK_CAN_LOAD_X64
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverZDSR>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBOY>());
#else
    TOLK_LOG_INFO("ZDSR/BoyPCReader: using the 64-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"ZDSR", true, true, TolkBridgeBackendZDSR));
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"BoyPCReader", true, false, TolkBridgeBackendBOY));
#endif
#if TOLK_CAN_LOAD_X86
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverZDCloud>());
#else
    TOLK_LOG_INFO("ZDCloud: using the 32-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"ZDCloud", true, false, TolkBridgeBackendZDCloud));
#endif
    // Japanese and Korean screen readers (regional market). They ship 64-bit
    // code but no ARM64 build, so an ARM64 build drives them through the
    // 64-bit helper like the other local readers.
#if TOLK_CAN_LOAD_X86 || TOLK_CAN_LOAD_X64
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverPCTalker>());
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverSenseReader>());
#else
    TOLK_LOG_INFO("PC-Talker/Sense Reader: using the 64-bit bridge helper");
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"PC-Talker", true, true, TolkBridgeBackendPCTalker));
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverBridged>(L"Sense Reader", true, false, TolkBridgeBackendSenseReader));
#endif
    // Generic Windows backend: UIA notifications. It activates while a screen
    // reader has set the Windows screen-reader flag, a UIA client is listening,
    // a UIA consumer is actually running (Narrator) and the application owns a
    // top-level window to host the provider on. It ranks below the named screen
    // readers and above the fallback speech engines, and it is never latched
    // onto (see DetectCurrentScreenReader) because the flag and the listening
    // bit can outlive the reader that raised them.
    g_screenReaderDrivers.push_back(std::make_unique<ScreenReaderDriverUIA>());
    g_uiaDriver = g_screenReaderDrivers.back().get();
    // Fallback speech engines. Like SAPI, OneCore does not depend on the
    // Windows screen-reader flag; both are enabled by Tolk_TrySAPI and moved
    // by Tolk_PreferSAPI, and OneCore is always tried before SAPI.
    if (g_trySAPI) {
      TOLK_LOG_INFO("Initializing OneCore and SAPI fallback drivers");
      g_oneCore = std::make_unique<ScreenReaderDriverOneCore>();
      g_sapi = std::make_unique<ScreenReaderDriverSAPI>();
    }
    TOLK_LOG_INFO("All drivers initialized successfully, total=%d", (int)g_screenReaderDrivers.size());
  }
  catch (...) {
    TOLK_LOG_ERROR("EXCEPTION during driver initialization!");
    InterlockedExchange(&g_lastError, TOLK_ERR_LOAD_EXCEPTION);
    g_oneCore.reset();
    g_sapi.reset();
    g_screenReaderDrivers.clear();
    g_uiaDriver = nullptr;
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
    g_oneCore.reset();
    g_sapi.reset();
    g_screenReaderDrivers.clear();
    g_uiaDriver = nullptr;
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
    if (g_trySAPI) {
      if (!g_oneCore)
        g_oneCore = std::make_unique<ScreenReaderDriverOneCore>();
      if (!g_sapi)
        g_sapi = std::make_unique<ScreenReaderDriverSAPI>();
    }
    else {
      g_oneCore.reset();
      g_sapi.reset();
    }
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
  if (Tolk_IsLoaded() && g_trySAPI && (g_oneCore || g_sapi)) {
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
