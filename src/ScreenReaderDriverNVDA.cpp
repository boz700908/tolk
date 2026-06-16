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
  nvdaController_testIfRunning(nullptr)
{
#ifdef _M_ARM64
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
  int loadedCount = (nvdaController_speakText?1:0) + (nvdaController_brailleMessage?1:0) +
                    (nvdaController_cancelSpeech?1:0) + (nvdaController_testIfRunning?1:0);
  TOLK_LOG_INFO("NVDA: Loaded %d/4 API functions", loadedCount);
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
bool ScreenReaderDriverNVDA::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < 100) {
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
bool ScreenReaderDriverNVDA::Output(const wchar_t *str, bool interrupt) {
  // Beware short-circuiting.
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
