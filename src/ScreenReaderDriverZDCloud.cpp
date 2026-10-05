/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZDCloud.cpp
 *  Description:    Driver for the ZDCloud (之多云) screen reader.
 *  License:        LGPLv3
 */
// The ZDCloud backend ships as the 32-bit ZDCloudAPI.dll. It has no public
// header, so the exports are resolved dynamically, like the other drivers.
#include "ScreenReaderDriverZDCloud.h"
#include "TolkDebug.h"
#include "TolkLibrary.h"
// Obfuscated application credentials (XOR encoded, decoded at runtime) so that
// the published binary does not contain the keys as plain text.
static const unsigned char ZDCLOUD_APP_KEY[] = { 0x0A, 0x5E, 0x5C, 0x5F, 0x12, 0x1F, 0x09, 0x1F, 0x59, 0x5E };
static const unsigned char ZDCLOUD_SEC_KEY[] = { 0x4E, 0x48, 0x00, 0x0D, 0x00, 0x45, 0x44, 0x0D, 0x5A, 0x58 };
ScreenReaderDriverZDCloud::ScreenReaderDriverZDCloud() :
  ScreenReaderDriver(L"ZDCloud", true, false),
  controller(nullptr),
  initial(nullptr),
  speakAsync(nullptr),
  speakInsert(nullptr),
  stopSpeak(nullptr),
  uninitial(nullptr),
  initialized(false)
{
  Initialize();
}
ScreenReaderDriverZDCloud::~ScreenReaderDriverZDCloud() {
  if (uninitial) uninitial();
  if (controller) FreeLibrary(controller);
}
std::wstring ScreenReaderDriverZDCloud::DecodeSecret(const unsigned char *bytes, size_t size, unsigned char key) {
  std::wstring value;
  value.reserve(size);
  for (size_t index = 0; index < size; ++index) {
    value.push_back(static_cast<wchar_t>(bytes[index] ^ key));
  }
  return value;
}
bool ScreenReaderDriverZDCloud::Initialize() {
  if (initialized) return true;
#ifdef _WIN64
  TOLK_LOG_INFO("ZDCloud: 32-bit only, driver unavailable in this build");
  return false;
#else
  if (!controller) {
    TOLK_LOG_INFO("ZDCloud: Loading 32-bit ZDCloudAPI.dll");
    controller = TolkLoadLibrary(L"ZDCloudAPI.dll");
    if (!controller) {
      TOLK_LOG_WARN("ZDCloud: DLL not found, driver disabled");
      return false;
    }
    initial     = (ZDCloud_Initial)GetProcAddress(controller, "Initial");
    speakAsync  = (ZDCloud_Speak)GetProcAddress(controller, "SpeakAsync");
    speakInsert = (ZDCloud_Speak)GetProcAddress(controller, "SpeakInsert");
    stopSpeak   = (ZDCloud_Void)GetProcAddress(controller, "StopSpeak");
    uninitial   = (ZDCloud_Void)GetProcAddress(controller, "UnInitial");
    if (!initial || !speakAsync || !speakInsert) {
      TOLK_LOG_WARN("ZDCloud: DLL is missing required exports, driver disabled");
      initial = nullptr;
      speakAsync = nullptr;
      speakInsert = nullptr;
      stopSpeak = nullptr;
      uninitial = nullptr;
      FreeLibrary(controller);
      controller = nullptr;
      return false;
    }
  }
  auto appKey = DecodeSecret(ZDCLOUD_APP_KEY, sizeof(ZDCLOUD_APP_KEY), 0x6E);
  auto secKey = DecodeSecret(ZDCLOUD_SEC_KEY, sizeof(ZDCLOUD_SEC_KEY), 0x72);
  int result = initial(appKey.c_str(), secKey.c_str(), nullptr);
  if (result != 0) {
    // The backend may report "not ready" on the first call, so retry once.
    result = initial(appKey.c_str(), secKey.c_str(), nullptr);
  }
  if (result != 0) {
    TOLK_LOG_WARN("ZDCloud: Initial failed (code %d), driver disabled", result);
    return false;
  }
  TOLK_LOG_INFO("ZDCloud: Initialized successfully");
  initialized = true;
  return true;
#endif
}
bool ScreenReaderDriverZDCloud::Speak(const wchar_t *str, bool interrupt) {
  if (!str) return false;
  if (!initialized && !Initialize()) return false;
  if (interrupt) {
    if (stopSpeak) stopSpeak();
    return speakAsync ? (speakAsync(str, 1) == 0) : false;
  }
  return speakInsert ? (speakInsert(str, 1) == 0) : false;
}
bool ScreenReaderDriverZDCloud::Silence() {
  if (!initialized && !Initialize()) return false;
  if (!stopSpeak) return false;
  stopSpeak();
  return true;
}
bool ScreenReaderDriverZDCloud::IsActive() {
  return initialized;
}
