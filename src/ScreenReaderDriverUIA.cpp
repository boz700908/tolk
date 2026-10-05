/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverUIA.cpp
 *  Description:    Driver that speaks through UI Automation notification
 *                  events, which any listening UIA client (such as Narrator)
 *                  announces.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverUIA.h"
#include "TolkDebug.h"
#include <UIAutomation.h>
#include <UIAutomationCoreApi.h>
#include <cwchar>
#include <deque>
#include <mutex>
#include <string>
#include <tlhelp32.h>
#include <cstring>

namespace {

// Messages handled by the notification window on its own thread.
const UINT WM_TOLK_UIA_EXECUTE = WM_APP + 1;
const UINT WM_TOLK_UIA_SHUTDOWN = WM_APP + 2;

struct UiaCommand {
  std::wstring text;
  bool interrupt;
};

// uiautomationcore.dll entry points, resolved at run time so the driver
// degrades gracefully where UIA notifications are not available (the
// notification API arrived in Windows 10 1709).
struct UiaApi {
  HMODULE module;
  BOOL (WINAPI *ClientsAreListening)(void);
  LRESULT (WINAPI *ReturnRawElementProvider)(HWND, WPARAM, LPARAM, IRawElementProviderSimple *);
  HRESULT (WINAPI *HostProviderFromHwnd)(HWND, IRawElementProviderSimple **);
  HRESULT (WINAPI *RaiseNotificationEvent)(IRawElementProviderSimple *, NotificationKind, NotificationProcessing, BSTR, BSTR);
  void (WINAPI *DisconnectProvider)(IRawElementProviderSimple *);

  UiaApi() :
    module(nullptr), ClientsAreListening(nullptr), ReturnRawElementProvider(nullptr),
    HostProviderFromHwnd(nullptr), RaiseNotificationEvent(nullptr), DisconnectProvider(nullptr)
  {}

  bool Load() {
    if (module) return Available();
    module = LoadLibraryExW(L"uiautomationcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return false;
    ClientsAreListening = reinterpret_cast<BOOL (WINAPI *)(void)>(
      GetProcAddress(module, "UiaClientsAreListening"));
    ReturnRawElementProvider = reinterpret_cast<LRESULT (WINAPI *)(HWND, WPARAM, LPARAM, IRawElementProviderSimple *)>(
      GetProcAddress(module, "UiaReturnRawElementProvider"));
    HostProviderFromHwnd = reinterpret_cast<HRESULT (WINAPI *)(HWND, IRawElementProviderSimple **)>(
      GetProcAddress(module, "UiaHostProviderFromHwnd"));
    RaiseNotificationEvent = reinterpret_cast<HRESULT (WINAPI *)(IRawElementProviderSimple *, NotificationKind, NotificationProcessing, BSTR, BSTR)>(
      GetProcAddress(module, "UiaRaiseNotificationEvent"));
    DisconnectProvider = reinterpret_cast<void (WINAPI *)(IRawElementProviderSimple *)>(
      GetProcAddress(module, "UiaDisconnectProvider"));
    return Available();
  }

  bool Available() const {
    return ClientsAreListening && ReturnRawElementProvider && HostProviderFromHwnd && RaiseNotificationEvent;
  }
};

UiaApi &Api() {
  static UiaApi api;
  return api;
}

// Minimal server-side UI Automation provider for the notification window.
class UiaProvider : public IRawElementProviderSimple {
public:
  explicit UiaProvider(HWND window) : references(1), window(window) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **result) override {
    if (!result) return E_POINTER;
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IRawElementProviderSimple)) {
      *result = static_cast<IRawElementProviderSimple *>(this);
      AddRef();
      return S_OK;
    }
    *result = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&references));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG remaining = InterlockedDecrement(&references);
    if (remaining == 0) delete this;
    return static_cast<ULONG>(remaining);
  }

  HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *result) override {
    if (!result) return E_POINTER;
    *result = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID, IUnknown **result) override {
    if (!result) return E_POINTER;
    *result = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT *result) override {
    if (!result) return E_POINTER;
    VariantInit(result);
    switch (propertyId) {
      case UIA_ControlTypePropertyId:
        result->vt = VT_I4;
        result->lVal = UIA_CustomControlTypeId;
        break;
      case UIA_IsContentElementPropertyId:
      case UIA_IsControlElementPropertyId:
        result->vt = VT_BOOL;
        result->boolVal = VARIANT_TRUE;
        break;
      case UIA_NamePropertyId:
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(L"Tolk Speech");
        if (!result->bstrVal) return E_OUTOFMEMORY;
        break;
      case UIA_LiveSettingPropertyId:
        result->vt = VT_I4;
        result->lVal = Assertive;
        break;
      case UIA_IsKeyboardFocusablePropertyId:
        result->vt = VT_BOOL;
        result->boolVal = VARIANT_FALSE;
        break;
      case UIA_AutomationIdPropertyId:
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(L"TolkNotification");
        if (!result->bstrVal) return E_OUTOFMEMORY;
        break;
      case UIA_ClassNamePropertyId:
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(L"TolkUIAProvider");
        if (!result->bstrVal) return E_OUTOFMEMORY;
        break;
      case UIA_NativeWindowHandlePropertyId:
        result->vt = VT_I4;
        result->lVal = static_cast<LONG>(reinterpret_cast<INT_PTR>(window));
        break;
      default:
        break;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple **result) override {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (!Api().HostProviderFromHwnd) return S_OK;
    return Api().HostProviderFromHwnd(window, result);
  }

  void Notify(const std::wstring &text, bool interrupt) {
    if (!Api().RaiseNotificationEvent) return;
    BSTR content = SysAllocString(text.c_str());
    BSTR activity = SysAllocString(L"Tolk");
    if (!content || !activity) {
      if (content) SysFreeString(content);
      if (activity) SysFreeString(activity);
      return;
    }
    // The important variants are the ones screen readers act on: NVDA reports
    // ImportantAll/ImportantMostRecent notifications and ignores the plain
    // variants, while Narrator reports all of them.
    Api().RaiseNotificationEvent(this, NotificationKind_ActionCompleted,
      interrupt ? NotificationProcessing_ImportantMostRecent : NotificationProcessing_ImportantAll,
      content, activity);
    SysFreeString(content);
    SysFreeString(activity);
  }

private:
  ~UiaProvider() = default;

  volatile LONG references;
  HWND window;
};

// UIA only delivers notifications from a provider that is owned by a top-level
// window of the calling process. A parentless popup created off-screen is not
// associated with the application and its notifications are dropped, which is
// why a game could never speak through this driver. Pick a host the same way
// Prism does: the foreground window if it is ours, otherwise our active window,
// otherwise the first visible window we own.
bool IsUsableHostWindow(HWND window) {
  if (!window || !IsWindow(window) || !IsWindowVisible(window) || IsIconic(window)) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  if (process != GetCurrentProcessId()) return false;
  if (GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return false;
  if (window == GetConsoleWindow()) return false;
  wchar_t className[256] = {};
  if (GetClassNameW(window, className, ARRAYSIZE(className)) == 0) return false;
  return std::wcscmp(className, L"ConsoleWindowClass") != 0 &&
    std::wcscmp(className, L"CASCADIA_HOSTING_WINDOW_CLASS") != 0;
}

HWND RootOwnerOf(HWND window) {
  const HWND root = GetAncestor(window, GA_ROOTOWNER);
  return root ? root : window;
}

HWND FindHostWindow() {
  const HWND foreground = GetForegroundWindow();
  if (IsUsableHostWindow(foreground)) return RootOwnerOf(foreground);
  const HWND active = GetActiveWindow();
  if (IsUsableHostWindow(active)) return RootOwnerOf(active);
  HWND found = nullptr;
  EnumWindows([](HWND window, LPARAM param) -> BOOL {
    if (IsUsableHostWindow(window)) {
      *reinterpret_cast<HWND *>(param) = window;
      return FALSE;
    }
    return TRUE;
  }, reinterpret_cast<LPARAM>(&found));
  return found ? RootOwnerOf(found) : nullptr;
}

bool ProcessExists(const wchar_t *name) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, name) == 0) {
        found = true;
        break;
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
}

// A running UIA consumer that actually turns notification events into speech
// has to exist for this backend to be useful. Nothing in the UIA API tells us
// whether a listener consumes events, and both the Windows screen-reader flag
// and UiaClientsAreListening can stay set after the reader that raised them has
// exited, so the flag alone would latch the driver on forever. Narrator is the
// one mainstream reader that uses UIA notifications and has no dedicated Tolk
// driver; NVDA and the others have their own, so they win detection first
// anyway. TOLK_UIA_ALWAYS=1 bypasses the process check for testing with another
// notification consumer.
bool UiaConsumerRunning() {
  static DWORD lastCheck = 0;
  static bool cached = false;
  const DWORD now = GetTickCount();
  if (lastCheck != 0 && (now - lastCheck) < 1000) return cached;
  cached = ProcessExists(L"Narrator.exe");
  lastCheck = now;
  return cached;
}

bool UiaAlwaysEnabled() {
  static const bool forced = []() {
    wchar_t value[8] = {};
    const DWORD length = GetEnvironmentVariableW(L"TOLK_UIA_ALWAYS", value, ARRAYSIZE(value));
    return length > 0 && value[0] != L'0';
  }();
  return forced;
}

} // namespace

struct TolkUiaSession {
  HWND host;
  HWND window;
  UiaProvider *provider;
  HANDLE thread;
  HANDLE ready;
  std::mutex lock;
  std::deque<UiaCommand> queue;

  TolkUiaSession() : host(nullptr), window(nullptr), provider(nullptr), thread(nullptr), ready(nullptr) {}
};

namespace {

void DrainSession(TolkUiaSession *session) {
  if (!session) return;
  for (;;) {
    UiaCommand command;
    {
      std::lock_guard<std::mutex> guard(session->lock);
      if (session->queue.empty()) return;
      command = session->queue.front();
      session->queue.pop_front();
    }
    if (session->provider) session->provider->Notify(command.text, command.interrupt);
  }
}

LRESULT CALLBACK UiaWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto *session = reinterpret_cast<TolkUiaSession *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  switch (message) {
    case WM_GETOBJECT:
      if (static_cast<LONG>(lParam) == UiaRootObjectId && session && session->provider) {
        return Api().ReturnRawElementProvider(window, wParam, lParam, session->provider);
      }
      break;
    case WM_TOLK_UIA_EXECUTE:
      DrainSession(session);
      return 0;
    case WM_TOLK_UIA_SHUTDOWN:
      PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      return 0;
    default:
      break;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

ScreenReaderDriverUIA::ScreenReaderDriverUIA() :
  ScreenReaderDriver(L"UIA", true, false),
  session(nullptr),
  disabled(false)
{
  TOLK_LOG_INFO("UIA: Initializing driver");
  if (!Api().Load()) {
    TOLK_LOG_WARN("UIA: UI Automation notification API unavailable, driver disabled");
    disabled = true;
  }
}

ScreenReaderDriverUIA::~ScreenReaderDriverUIA() {
  TOLK_LOG_INFO("UIA: Finalizing driver");
  StopSession();
}

unsigned long __stdcall ScreenReaderDriverUIA::ThreadProc(void *self) {
  static_cast<ScreenReaderDriverUIA *>(self)->RunSession();
  return 0;
}

void ScreenReaderDriverUIA::RunSession() {
  TolkUiaSession *current = session;
  const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
  const bool uninitCom = SUCCEEDED(comResult);
  if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
    if (current->ready) SetEvent(current->ready);
    return;
  }
  const std::wstring className = L"TolkUIAProviderWindow_" +
    std::to_wstring(static_cast<unsigned long long>(reinterpret_cast<unsigned long long>(this)));
  HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = UiaWindowProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className.c_str();
  // The notification provider is created as a popup owned by one of the
  // application's own top-level windows so UIA clients associate it with the
  // application (and with the game window in a game).
  current->host = FindHostWindow();
  if (current->host && RegisterClassExW(&windowClass)) {
    current->window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      className.c_str(), L"Tolk UIA Notification", WS_POPUP, 0, 0, 0, 0,
      current->host, nullptr, instance, nullptr);
  }
  if (current->window) {
    SetWindowLongPtrW(current->window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(current));
    current->provider = new UiaProvider(current->window);
  }
  if (current->ready) SetEvent(current->ready);
  if (current->window) {
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (IsWindow(current->window)) DestroyWindow(current->window);
    current->window = nullptr;
    if (current->provider) {
      if (Api().DisconnectProvider) Api().DisconnectProvider(current->provider);
      current->provider->Release();
      current->provider = nullptr;
    }
    UnregisterClassW(className.c_str(), instance);
  }
  if (uninitCom) CoUninitialize();
}

bool ScreenReaderDriverUIA::EnsureSession() {
  if (session) {
    if (session->window && IsWindow(session->host)) return true;
    // The owning window is gone or the session never came up; rebuild it.
    StopSession();
  }
  if (disabled) return false;
  // Without one of our own top-level windows there is nothing UIA can attach
  // the notification provider to, so stay inactive and let Tolk fall through.
  if (!FindHostWindow()) return false;
  session = new TolkUiaSession();
  session->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (session->ready) {
    session->thread = CreateThread(nullptr, 0, &ScreenReaderDriverUIA::ThreadProc, this, 0, nullptr);
  }
  if (!session->ready || !session->thread) {
    StopSession();
    return false;
  }
  WaitForSingleObject(session->ready, 5000);
  if (!session->window) {
    // The host window may not exist yet (a game that is still starting up), so
    // stay enabled and try again on the next detection round.
    TOLK_LOG_WARN("UIA: could not create the notification window, retrying later");
    StopSession();
    return false;
  }
  TOLK_LOG_INFO("UIA: notification provider window created");
  return true;
}

void ScreenReaderDriverUIA::StopSession() {
  TolkUiaSession *current = session;
  if (!current) return;
  if (current->thread) {
    if (current->window) PostMessageW(current->window, WM_TOLK_UIA_SHUTDOWN, 0, 0);
    WaitForSingleObject(current->thread, 5000);
    CloseHandle(current->thread);
  }
  if (current->ready) CloseHandle(current->ready);
  delete current;
  session = nullptr;
}

bool ScreenReaderDriverUIA::Post(const wchar_t *str, bool interrupt) {
  TolkUiaSession *current = session;
  if (!current || !current->window) return false;
  {
    std::lock_guard<std::mutex> guard(current->lock);
    current->queue.push_back(UiaCommand{ str ? std::wstring(str) : std::wstring(), interrupt });
  }
  return PostMessageW(current->window, WM_TOLK_UIA_EXECUTE, 0, 0) != 0;
}

bool ScreenReaderDriverUIA::Speak(const wchar_t *str, bool interrupt) {
  if (!EnsureSession()) return false;
  return Post(str, interrupt);
}

bool ScreenReaderDriverUIA::Silence() {
  if (!EnsureSession()) return false;
  return Post(L"", true);
}

bool ScreenReaderDriverUIA::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
    return cachedIsActive;
  }
  cachedIsActive = false;
  if (!disabled) {
    BOOL screenReader = FALSE;
    if (SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0) && screenReader != FALSE &&
        Api().ClientsAreListening() && (UiaAlwaysEnabled() || UiaConsumerRunning())) {
      cachedIsActive = EnsureSession();
    }
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
