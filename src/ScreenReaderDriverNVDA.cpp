/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverNVDA.cpp
 *  Description:    Driver for the NVDA screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
// The NVDA Project provides a header and libraries,
// but we don't use these in order to support running even if the DLL is missing.
#include "ScreenReaderDriverNVDA.h"
#include "TolkDebug.h"
ScreenReaderDriverNVDA::ScreenReaderDriverNVDA() :
  ScreenReaderDriver(L"NVDA", true, true),
  controller(nullptr),
  nvdaController_speakText(nullptr),
  nvdaController_brailleMessage(nullptr),
  nvdaController_cancelSpeech(nullptr),
  nvdaController_testIfRunning(nullptr),
  nvdaController_isSpeaking(nullptr)
{
// Every architecture uses its own native client. NVDA 2026.3 ships a native
// ARM64EC client next to the ARM64 one, so no emulated client is needed.
#if defined(_M_ARM64EC)
  TOLK_LOG_INFO("NVDA: Loading ARM64EC native nvdaControllerClientARM64EC.dll");
  controller = LoadLibrary(L"nvdaControllerClientARM64EC.dll");
#elif defined(_M_ARM64)
  TOLK_LOG_INFO("NVDA: Loading ARM64 native nvdaControllerClientARM64.dll");
  controller = LoadLibrary(L"nvdaControllerClientARM64.dll");
#elif defined(_WIN64)
  TOLK_LOG_INFO("NVDA: Loading 64-bit nvdaControllerClient64.dll");
  controller = LoadLibrary(L"nvdaControllerClient64.dll");
#else
  TOLK_LOG_INFO("NVDA: Loading 32-bit nvdaControllerClient32.dll");
  controller = LoadLibrary(L"nvdaControllerClient32.dll");
#endif
  if (!controller) {
    TOLK_LOG_WARN("NVDA: DLL not found, driver disabled");
    return;
  }
  TOLK_LOG_INFO("NVDA: DLL loaded successfully");
  nvdaController_speakText = (NVDAController_speakText)GetProcAddress(controller, "nvdaController_speakText");
  nvdaController_brailleMessage = (NVDAController_brailleMessage)GetProcAddress(controller, "nvdaController_brailleMessage");
  nvdaController_cancelSpeech = (NVDAController_cancelSpeech)GetProcAddress(controller, "nvdaController_cancelSpeech");
  nvdaController_testIfRunning = (NVDAController_testIfRunning)GetProcAddress(controller, "nvdaController_testIfRunning");
  // Added in NVDA controller client 3.0 (NVDA 2026.3+); optional for older clients.
  nvdaController_isSpeaking = (NVDAController_isSpeaking)GetProcAddress(controller, "nvdaController_isSpeaking");
  int loadedCount = (nvdaController_speakText?1:0) + (nvdaController_brailleMessage?1:0) +
                    (nvdaController_cancelSpeech?1:0) + (nvdaController_testIfRunning?1:0) +
                    (nvdaController_isSpeaking?1:0);
  TOLK_LOG_INFO("NVDA: Loaded %d/5 API functions (isSpeaking %s)",
                loadedCount, nvdaController_isSpeaking ? "available" : "unavailable");
}
ScreenReaderDriverNVDA::~ScreenReaderDriverNVDA() {
  if (controller) {
    TOLK_LOG_INFO("NVDA: Unloading DLL");
    FreeLibrary(controller);
  }
}
bool ScreenReaderDriverNVDA::Speak(const wchar_t *str, bool interrupt) {
  if (interrupt && !Silence()) return false;
  if (nvdaController_speakText) return (nvdaController_speakText(str) == 0);
  return false;
}
bool ScreenReaderDriverNVDA::Braille(const wchar_t *str) {
  if (nvdaController_brailleMessage) return (nvdaController_brailleMessage(str) == 0);
  return false;
}
bool ScreenReaderDriverNVDA::Silence() {
  if (nvdaController_cancelSpeech) return (nvdaController_cancelSpeech() == 0);
  return false;
}
bool ScreenReaderDriverNVDA::IsSpeaking() {
  if (!nvdaController_isSpeaking) return false;
  boolean speaking = FALSE;
  // Older NVDA versions (< 2026.3) answer RPC_S_UNKNOWN_IF, so this fails safely.
  if (nvdaController_isSpeaking(&speaking) != 0) return false;
  return speaking != FALSE;
}
bool ScreenReaderDriverNVDA::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
    return cachedIsActive;
  }

  // This needs an extra check because System Access pretends to be NVDA.
  if (nvdaController_testIfRunning) {
    cachedIsActive = (!!FindWindow(L"wxWindowClassNR", L"NVDA") && nvdaController_testIfRunning() == 0);
  } else {
    cachedIsActive = false;
  }
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
