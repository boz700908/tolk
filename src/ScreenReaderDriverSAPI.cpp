/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSAPI.cpp
 *  Description:    Driver for the Microsoft Speech API (SAPI).
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverSAPI.h"
#include "TolkDebug.h"
ScreenReaderDriverSAPI::ScreenReaderDriverSAPI() :
  ScreenReaderDriver(L"SAPI", true, false),
  controller(nullptr)
{
  TOLK_LOG_INFO("SAPI: Initializing fallback driver");
  Initialize();
}
ScreenReaderDriverSAPI::~ScreenReaderDriverSAPI() {
  TOLK_LOG_INFO("SAPI: Finalizing driver");
  Finalize();
}
bool ScreenReaderDriverSAPI::Speak(const wchar_t *str, bool interrupt) {
  if (!controller) return false;
  DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML;
  if (interrupt) flags |= SPF_PURGEBEFORESPEAK;
  return SUCCEEDED(controller->Speak(str, flags, nullptr));
}
bool ScreenReaderDriverSAPI::IsSpeaking() {
  if (!controller) return false;
  SPVOICESTATUS status;
  // The second parameter to GetStatus() can be NULL,
  // suppress warning when compiling with /analyze.
#pragma warning(suppress:6387)
  if (FAILED(controller->GetStatus(&status, nullptr))) return false;
  return (status.dwRunningState == SPRS_IS_SPEAKING);
}
bool ScreenReaderDriverSAPI::Silence() {
  if (!controller) return false;
  const DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML | SPF_PURGEBEFORESPEAK;
  return SUCCEEDED(controller->Speak(nullptr, flags, nullptr));
}
void ScreenReaderDriverSAPI::Initialize() {
  if (controller || FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER, IID_ISpVoice, (void **)&controller))) {
    TOLK_LOG_WARN("SAPI: CoCreateInstance failed");
    return;
  }
  TOLK_LOG_INFO("SAPI: COM instance created successfully");
}
void ScreenReaderDriverSAPI::Finalize() {
  if (controller) {
    TOLK_LOG_INFO("SAPI: Releasing COM instance");
    controller->Release();
    controller = nullptr;
  }
}
