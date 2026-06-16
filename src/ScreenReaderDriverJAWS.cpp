/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverJAWS.cpp
 *  Description:    Driver for the JAWS screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include <string>
#include "ScreenReaderDriverJAWS.h"
#include "TolkDebug.h"
ScreenReaderDriverJAWS::ScreenReaderDriverJAWS() :
  ScreenReaderDriver(L"JAWS", true, true),
  controller(nullptr),
  lastIsActiveTime(0),
  cachedIsActive(false)
{
  TOLK_LOG_INFO("JAWS: Initializing driver");
  if (IsRunning()) {
    TOLK_LOG_INFO("JAWS: JAWS is running, creating COM instance");
    Initialize();
  }
  else {
    TOLK_LOG_WARN("JAWS: JAWS not running, driver disabled");
  }
}
ScreenReaderDriverJAWS::~ScreenReaderDriverJAWS() {
  TOLK_LOG_INFO("JAWS: Finalizing driver");
  Finalize();
}
bool ScreenReaderDriverJAWS::Speak(const wchar_t *str, bool interrupt) {
  if (!controller) return false;
  const BSTR bstr = SysAllocString(str);
  if (!bstr) return false;
  VARIANT_BOOL result = VARIANT_FALSE;
  const VARIANT_BOOL flush = interrupt ? VARIANT_TRUE : VARIANT_FALSE;
  const bool succeeded = SUCCEEDED(controller->SayString(bstr, flush, &result));
  SysFreeString(bstr);
  return (succeeded && result == VARIANT_TRUE);
}
bool ScreenReaderDriverJAWS::Braille(const wchar_t *str) {
  if (!controller) return false;
  std::wstring wstr(str);
  std::wstring::size_type i = wstr.find_first_of(L"\"");
  while (i != std::wstring::npos) {
    wstr[i] = L'\'';
    i = wstr.find_first_of(L"\"", i + 1);
  }
  wstr.insert(0, L"BrailleString(\"");
  wstr.append(L"\")");
  const BSTR bstr = SysAllocString(wstr.c_str());
  if (!bstr) return false;
  VARIANT_BOOL result = VARIANT_FALSE;
  const bool succeeded = SUCCEEDED(controller->RunFunction(bstr, &result));
  SysFreeString(bstr);
  return (succeeded && result == VARIANT_TRUE);
}
bool ScreenReaderDriverJAWS::Silence() {
  if (!controller) return false;
  return SUCCEEDED(controller->StopSpeech());
}
bool ScreenReaderDriverJAWS::IsActive() {
  // 性能优化：先检查缓存（100ms超时）
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < 100) {
    return cachedIsActive;
  }

  if (!IsRunning()) {
    Finalize();
    cachedIsActive = false;
    lastIsActiveTime = currentTime;
    return false;
  }
  if (!controller) Initialize();
  cachedIsActive = (!!controller);
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
bool ScreenReaderDriverJAWS::Output(const wchar_t *str, bool interrupt) {
  // Beware short-circuiting.
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
void ScreenReaderDriverJAWS::Initialize() {
  if (controller || FAILED(CoCreateInstance(CLSID_JawsApi, nullptr, CLSCTX_INPROC_SERVER, IID_IJawsApi, (void **)&controller))) {
    TOLK_LOG_WARN("JAWS: CoCreateInstance failed");
    return;
  }
  TOLK_LOG_INFO("JAWS: COM instance created successfully");
}
void ScreenReaderDriverJAWS::Finalize() {
  if (controller) {
    TOLK_LOG_INFO("JAWS: Releasing COM instance");
    controller->Release();
    controller = nullptr;
  }
}
