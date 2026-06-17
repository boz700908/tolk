/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZT.cpp
 *  Description:    Driver for the ZoomText screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverZT.h"
#include "TolkDebug.h"
ScreenReaderDriverZT::ScreenReaderDriverZT() :
  ScreenReaderDriver(L"ZoomText", true, false),
  controller(nullptr),
  speech(nullptr)
{
  TOLK_LOG_INFO("ZT: Initializing driver");
  if (IsRunning()) {
    TOLK_LOG_INFO("ZT: ZoomText is running, creating COM instance");
    Initialize();
  }
  else {
    TOLK_LOG_WARN("ZT: ZoomText not running, driver disabled");
  }
}
ScreenReaderDriverZT::~ScreenReaderDriverZT() {
  TOLK_LOG_INFO("ZT: Finalizing driver");
  Finalize();
}
bool ScreenReaderDriverZT::Speak(const wchar_t *str, bool interrupt) {
  if (!controller || !speech) return false;
  IVoice *voice;
  if (FAILED(speech->get_CurrentVoice(&voice))) return false;
  if (interrupt && FAILED(voice->put_AllowInterrupt(VARIANT_TRUE))) {
    voice->Release();
    return false;
  }
  const BSTR bstr = SysAllocString(str);
  if (!bstr) {
    voice->Release();
    return false;
  }
  const bool succeeded = SUCCEEDED(voice->Speak(bstr));
  SysFreeString(bstr);
  if (interrupt && FAILED(voice->put_AllowInterrupt(VARIANT_FALSE))) {
    voice->Release();
    return false;
  }
  voice->Release();
  return succeeded;
}
bool ScreenReaderDriverZT::IsSpeaking() {
  if (!controller || !speech) return false;
  IVoice *voice;
  if (FAILED(speech->get_CurrentVoice(&voice))) return false;
  VARIANT_BOOL result = VARIANT_FALSE;
  const bool succeeded = SUCCEEDED(voice->get_Speaking(&result));
  voice->Release();
  return (succeeded && result == VARIANT_TRUE);
}
bool ScreenReaderDriverZT::Silence() {
  if (!controller || !speech) return false;
  IVoice *voice;
  if (FAILED(speech->get_CurrentVoice(&voice))) return false;
  const bool succeeded = SUCCEEDED(voice->Stop());
  voice->Release();
  return succeeded;
}
bool ScreenReaderDriverZT::IsActive() {
  // Performance: Check cache first (100ms timeout)
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
void ScreenReaderDriverZT::Initialize() {
  if (controller || FAILED(CoCreateInstance(CLSID_ZoomText, nullptr, CLSCTX_LOCAL_SERVER, IID_IZoomText2, (void **)&controller))) {
    TOLK_LOG_WARN("ZT: CoCreateInstance failed");
    return;
  }
  TOLK_LOG_INFO("ZT: COM instance created successfully");
  if (FAILED(controller->get_Speech(&speech))) {
    TOLK_LOG_ERROR("ZT: Failed to get Speech interface");
    Finalize();
  }
  else {
    TOLK_LOG_INFO("ZT: Speech interface obtained");
  }
}
void ScreenReaderDriverZT::Finalize() {
  TOLK_LOG_INFO("ZT: Releasing COM interfaces");
  if (speech) {
    speech->Release();
    speech = nullptr;
  }
  if (controller) {
    controller->Release();
    controller = nullptr;
  }
}
