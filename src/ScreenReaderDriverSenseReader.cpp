/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSenseReader.cpp
 *  Description:    Driver for the Sense Reader screen reader.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */
#include "ScreenReaderDriverSenseReader.h"
#include "TolkDebug.h"

// Class and interface ids from the Sense Reader type library.
// {8C94F442-B088-412C-9DBB-042CD8C04A78}
static const GUID TOLK_CLSID_SenseReaderApplication =
  { 0x8c94f442, 0xb088, 0x412c, { 0x9d, 0xbb, 0x04, 0x2c, 0xd8, 0xc0, 0x4a, 0x78 } };
// {7087A040-C069-4784-96FC-FC2E7F166737}
static const GUID TOLK_IID_IXVApplication =
  { 0x7087a040, 0xc069, 0x4784, { 0x96, 0xfc, 0xfc, 0x2e, 0x7f, 0x16, 0x67, 0x37 } };

ScreenReaderDriverSenseReader::ScreenReaderDriverSenseReader() :
  ScreenReaderDriver(L"Sense Reader", true, false),
  application(nullptr)
{
  TOLK_LOG_INFO("SenseReader: Initializing driver");
  if (IsRunning()) {
    Initialize();
  }
  else {
    TOLK_LOG_WARN("SenseReader: Sense Reader is not running, driver disabled");
  }
}

ScreenReaderDriverSenseReader::~ScreenReaderDriverSenseReader() {
  TOLK_LOG_INFO("SenseReader: Finalizing driver");
  Finalize();
}

// Sense Reader owns a window of the class "XVSRD" and only registers its COM
// class factory while it runs.
bool ScreenReaderDriverSenseReader::IsRunning() {
  if (!FindWindowW(L"XVSRD", nullptr)) return false;
  IClassFactory *factory = nullptr;
  const HRESULT hr = CoGetClassObject(TOLK_CLSID_SenseReaderApplication,
    CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr, IID_IClassFactory,
    reinterpret_cast<void **>(&factory));
  if (FAILED(hr) || !factory) return false;
  factory->Release();
  return true;
}

void ScreenReaderDriverSenseReader::Initialize() {
  if (application) return;
  const HRESULT hr = CoCreateInstance(TOLK_CLSID_SenseReaderApplication, nullptr,
    CLSCTX_ALL, TOLK_IID_IXVApplication, reinterpret_cast<void **>(&application));
  if (FAILED(hr) || !application) {
    TOLK_LOG_WARN("SenseReader: CoCreateInstance failed (hr=0x%08X)", static_cast<unsigned int>(hr));
    application = nullptr;
    return;
  }
  TOLK_LOG_INFO("SenseReader: COM instance created");
}

void ScreenReaderDriverSenseReader::Finalize() {
  if (application) {
    TOLK_LOG_INFO("SenseReader: Releasing COM interface");
    application->Release();
    application = nullptr;
  }
}

bool ScreenReaderDriverSenseReader::Speak(const wchar_t *str, bool interrupt) {
  if (!application) return false;
  if (interrupt && FAILED(application->StopSpeaking())) return false;
  const BSTR bstr = SysAllocString(str);
  if (!bstr) return false;
  const bool succeeded = SUCCEEDED(application->Speak(bstr));
  SysFreeString(bstr);
  return succeeded;
}

bool ScreenReaderDriverSenseReader::Silence() {
  if (!application) return false;
  return SUCCEEDED(application->StopSpeaking());
}

bool ScreenReaderDriverSenseReader::IsActive() {
  // Performance: Check cache first (100ms timeout)
  DWORD currentTime = GetTickCount();
  if ((currentTime - lastIsActiveTime) < CACHE_TIMEOUT_MS) {
    return cachedIsActive;
  }
  if (!IsRunning()) {
    Finalize();
  }
  else if (!application) {
    Initialize();
  }
  cachedIsActive = (application != nullptr);
  lastIsActiveTime = currentTime;
  return cachedIsActive;
}
