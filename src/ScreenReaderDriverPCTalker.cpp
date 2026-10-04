/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverPCTalker.cpp
 *  Description:    Driver for the PC-Talker screen reader.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverPCTalker.h"
#include "TolkDebug.h"
#include <functional>

// Values from the PC-Talker vendor header.
#define PCTK_PRIORITY_LOW       1
#define PCTK_PRIORITY_OVERRIDE  3
#define PCTK_PIN_MODE_DEFAULT   0x00000000u

// The PC-Talker API. PC-Talker installs PCTKUSR.dll system-wide; the exports
// are resolved dynamically so that Tolk does not depend on the vendor import
// library (and so that a missing DLL is not a load-time failure).
class PcTalkerApi {
public:
  HMODULE library;
  BOOL (__stdcall *Status)(void);
  BOOL (__stdcall *Read)(const wchar_t *, int, BOOL);
  void (__stdcall *Reset)(void);
  BOOL (__stdcall *GetVStatus)(void);
  BOOL (__stdcall *PinStatus)(void);
  BOOL (__stdcall *PinFocus)(LONG_PTR, const wchar_t *, DWORD, const wchar_t *, LONG_PTR);
  BOOL (__stdcall *PinIsFocus)(LONG_PTR);
  BOOL (__stdcall *PinWrite)(const wchar_t *, int, int);

  PcTalkerApi() :
    library(nullptr), Status(nullptr), Read(nullptr), Reset(nullptr), GetVStatus(nullptr),
    PinStatus(nullptr), PinFocus(nullptr), PinIsFocus(nullptr), PinWrite(nullptr)
  {}

  ~PcTalkerApi() {
    if (library) FreeLibrary(library);
  }

  bool Complete() const {
    return Status && Read && Reset && GetVStatus && PinStatus && PinFocus && PinIsFocus && PinWrite;
  }
};

// The vendor's braille API must be driven from one consistent thread, so all
// PCTKPin* calls are marshalled onto a dedicated worker thread. Calls are
// synchronous: the caller blocks until the worker has executed the call.
class PcTalkerBraille {
public:
  PcTalkerBraille() :
    api(nullptr), request(nullptr), done(nullptr), thread(nullptr),
    lockInitialized(false), quit(0)
  {}

  ~PcTalkerBraille() { Stop(); }

  bool Start(PcTalkerApi *library) {
    Stop();
    api = library;
    quit = 0;
    request = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!request || !done) {
      Stop();
      return false;
    }
    InitializeCriticalSection(&lock);
    lockInitialized = true;
    thread = CreateThread(nullptr, 0, &PcTalkerBraille::ThreadProc, this, 0, nullptr);
    if (!thread) {
      Stop();
      return false;
    }
    return true;
  }

  void Stop() {
    if (!thread && !request && !done && !lockInitialized) return;
    InterlockedExchange(&quit, 1);
    if (request) SetEvent(request);
    if (thread) {
      // Vendor calls must return before their events and lock are destroyed.
      WaitForSingleObject(thread, INFINITE);
      CloseHandle(thread);
      thread = nullptr;
    }
    work = nullptr;
    if (request) {
      CloseHandle(request);
      request = nullptr;
    }
    if (done) {
      CloseHandle(done);
      done = nullptr;
    }
    if (lockInitialized) {
      DeleteCriticalSection(&lock);
      lockInitialized = false;
    }
    api = nullptr;
  }

  bool Call(const std::function<bool()> &fn) {
    if (!thread) return false;
    bool result = false;
    EnterCriticalSection(&lock);
    work = [fn, &result]() -> bool { result = fn(); return result; };
    SetEvent(request);
    WaitForSingleObject(done, INFINITE);
    work = nullptr;
    LeaveCriticalSection(&lock);
    return result;
  }

private:
  static DWORD WINAPI ThreadProc(void *self) {
    return static_cast<PcTalkerBraille *>(self)->Run();
  }

  DWORD Run() {
    api->PinStatus();
    for (;;) {
      if (WaitForSingleObject(request, INFINITE) != WAIT_OBJECT_0) return 0;
      if (InterlockedCompareExchange(&quit, 0, 0) != 0) {
        SetEvent(done);
        return 0;
      }
      if (work) work();
      SetEvent(done);
    }
  }

  PcTalkerApi *api;
  HANDLE request;
  HANDLE done;
  HANDLE thread;
  CRITICAL_SECTION lock;
  bool lockInitialized;
  volatile LONG quit;
  std::function<bool()> work;
};

static PcTalkerApi *LoadPcTalker() {
  PcTalkerApi *api = new PcTalkerApi();
  api->library = LoadLibraryW(L"PCTKUSR.dll");
  if (!api->library) {
    TOLK_LOG_WARN("PCTalker: PCTKUSR.dll not found, driver disabled");
    delete api;
    return nullptr;
  }
  api->Status = reinterpret_cast<BOOL (__stdcall *)(void)>(GetProcAddress(api->library, "PCTKStatus"));
  api->Read = reinterpret_cast<BOOL (__stdcall *)(const wchar_t *, int, BOOL)>(GetProcAddress(api->library, "PCTKPReadW"));
  api->Reset = reinterpret_cast<void (__stdcall *)(void)>(GetProcAddress(api->library, "PCTKVReset"));
  api->GetVStatus = reinterpret_cast<BOOL (__stdcall *)(void)>(GetProcAddress(api->library, "PCTKGetVStatus"));
  api->PinStatus = reinterpret_cast<BOOL (__stdcall *)(void)>(GetProcAddress(api->library, "PCTKPinStatus"));
  api->PinFocus = reinterpret_cast<BOOL (__stdcall *)(LONG_PTR, const wchar_t *, DWORD, const wchar_t *, LONG_PTR)>(GetProcAddress(api->library, "PCTKPinFocusW"));
  api->PinIsFocus = reinterpret_cast<BOOL (__stdcall *)(LONG_PTR)>(GetProcAddress(api->library, "PCTKPinIsFocus"));
  api->PinWrite = reinterpret_cast<BOOL (__stdcall *)(const wchar_t *, int, int)>(GetProcAddress(api->library, "PCTKPinWriteW"));
  if (!api->Complete()) {
    TOLK_LOG_WARN("PCTalker: PCTKUSR.dll is missing a required export, driver disabled");
    delete api;
    return nullptr;
  }
  TOLK_LOG_INFO("PCTalker: PCTKUSR.dll loaded");
  return api;
}

ScreenReaderDriverPCTalker::ScreenReaderDriverPCTalker() :
  ScreenReaderDriver(L"PC-Talker", true, true),
  api(nullptr),
  braille(nullptr),
  brailleContext(0)
{
  TOLK_LOG_INFO("PCTalker: Initializing driver");
  Load();
}

ScreenReaderDriverPCTalker::~ScreenReaderDriverPCTalker() {
  TOLK_LOG_INFO("PCTalker: Finalizing driver");
  Unload();
}

void ScreenReaderDriverPCTalker::Load() {
  api = LoadPcTalker();
  if (!api) return;
  braille = new PcTalkerBraille();
  if (!braille->Start(api)) {
    TOLK_LOG_WARN("PCTalker: could not start the braille marshaller");
    delete braille;
    braille = nullptr;
  }
}

void ScreenReaderDriverPCTalker::Unload() {
  delete braille;
  braille = nullptr;
  delete api;
  api = nullptr;
}

bool ScreenReaderDriverPCTalker::Speak(const wchar_t *str, bool interrupt) {
  if (!api) return false;
  return api->Read(str, interrupt ? PCTK_PRIORITY_OVERRIDE : PCTK_PRIORITY_LOW, TRUE) != 0;
}

bool ScreenReaderDriverPCTalker::Braille(const wchar_t *str) {
  if (!api || !braille) return false;
  PcTalkerApi *a = api;
  if (!braille->Call([a]() { return a->PinStatus() != 0; })) return false;
  if (brailleContext != 0 && a->PinIsFocus(static_cast<LONG_PTR>(brailleContext))) {
    return braille->Call([a, str]() { return a->PinWrite(str, 0, 0) != 0; });
  }
  QueryUnbiasedInterruptTime(&brailleContext);
  const LONG_PTR context = static_cast<LONG_PTR>(brailleContext);
  return braille->Call([a, context, str]() {
    return a->PinFocus(context, str, PCTK_PIN_MODE_DEFAULT, nullptr, 0) != 0;
  });
}

bool ScreenReaderDriverPCTalker::IsSpeaking() {
  return api && api->GetVStatus() != 0;
}

bool ScreenReaderDriverPCTalker::Silence() {
  if (!api) return false;
  api->Reset();
  return true;
}

bool ScreenReaderDriverPCTalker::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
    return cachedIsActive;
  }
  cachedIsActive = (api != nullptr && api->Status() != 0);
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
