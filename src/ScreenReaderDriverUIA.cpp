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
#include <deque>
#include <mutex>
#include <string>

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

  UiaApi() :
    module(nullptr), ClientsAreListening(nullptr), ReturnRawElementProvider(nullptr),
    HostProviderFromHwnd(nullptr), RaiseNotificationEvent(nullptr)
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
    Api().RaiseNotificationEvent(this, NotificationKind_ActionCompleted,
      interrupt ? NotificationProcessing_ImportantMostRecent : NotificationProcessing_All,
      content, activity);
    SysFreeString(content);
    SysFreeString(activity);
  }

private:
  ~UiaProvider() = default;

  volatile LONG references;
  HWND window;
};

} // namespace

struct TolkUiaSession {
  HWND window;
  UiaProvider *provider;
  HANDLE thread;
  HANDLE ready;
  std::mutex lock;
  std::deque<UiaCommand> queue;

  TolkUiaSession() : window(nullptr), provider(nullptr), thread(nullptr), ready(nullptr) {}
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
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const std::wstring className = L"TolkUIAProviderWindow_" +
    std::to_wstring(static_cast<unsigned long long>(reinterpret_cast<unsigned long long>(this)));
  HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = UiaWindowProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className.c_str();
  if (RegisterClassExW(&windowClass)) {
    current->window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      className.c_str(), L"Tolk UIA Notification", WS_POPUP, 0, 0, 0, 0,
      nullptr, nullptr, instance, nullptr);
  }
  if (current->window) {
    SetWindowLongPtrW(current->window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(current));
    current->provider = new UiaProvider(current->window);
  }
  if (current->ready) SetEvent(current->ready);
  if (current->window) {
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (IsWindow(current->window)) DestroyWindow(current->window);
    current->window = nullptr;
    if (current->provider) {
      current->provider->Release();
      current->provider = nullptr;
    }
    UnregisterClassW(className.c_str(), instance);
  }
  CoUninitialize();
}

bool ScreenReaderDriverUIA::EnsureSession() {
  if (session && session->window) return true;
  if (disabled) return false;
  session = new TolkUiaSession();
  session->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (session->ready) {
    session->thread = CreateThread(nullptr, 0, &ScreenReaderDriverUIA::ThreadProc, this, 0, nullptr);
  }
  if (!session->ready || !session->thread) {
    StopSession();
    disabled = true;
    return false;
  }
  WaitForSingleObject(session->ready, 5000);
  if (!session->window) {
    TOLK_LOG_WARN("UIA: could not create the notification window, driver disabled");
    StopSession();
    disabled = true;
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
        Api().ClientsAreListening()) {
      cachedIsActive = EnsureSession();
    }
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
