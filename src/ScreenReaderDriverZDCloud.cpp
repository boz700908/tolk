/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZDCloud.cpp
 *  Description:    Driver for the ZDCloud cloud speech backend.
 *  License:        LGPLv3
 */
// ZDCloud ships as the 32-bit ZDCloudAPI.dll. A 32-bit build loads it directly.
// A 64-bit build cannot load a 32-bit DLL, so it runs an embedded 32-bit bridge
// (see src/zdcloud_bridge32) and talks to it over a named pipe. The bridge and
// ZDCloudAPI.dll are embedded into the 64-bit Tolk.dll as resources and
// extracted to a per-user cache directory at run time, so no extra files ship.
#include "ScreenReaderDriverZDCloud.h"
#include "TolkDebug.h"

#if !defined(_WIN64)
// Obfuscated application credentials (XOR encoded, decoded at runtime) so that
// the published binary does not contain the keys as plain text.
static const unsigned char ZDCLOUD_APP_KEY[] = { 0x0A, 0x5E, 0x5C, 0x5F, 0x12, 0x1F, 0x09, 0x1F, 0x59, 0x5E };
static const unsigned char ZDCLOUD_SEC_KEY[] = { 0x4E, 0x48, 0x00, 0x0D, 0x00, 0x45, 0x44, 0x0D, 0x5A, 0x58 };
#endif

#if defined(_M_X64)
namespace {
// Keep in sync with src/zdcloud_bridge32/main.cpp and the resource ids used in
// src/CMakeLists.txt.
const unsigned long kCommandSpeak = 1;
const unsigned long kCommandStop = 2;
const unsigned long kCommandQuit = 3;
const int kBridgeResourceId = 101;
const int kApiResourceId = 102;
const wchar_t kBridgeExeName[] = L"TolkZDCloudBridge32.exe";
const wchar_t kApiDllName[] = L"ZDCloudAPI.dll";
struct BridgeHeader {
  unsigned long command;
  unsigned long argument;
  unsigned long length;
};
struct BridgeReply {
  int result;
};
bool WriteAll(HANDLE pipe, const void *buffer, DWORD size) {
  const unsigned char *bytes = static_cast<const unsigned char *>(buffer);
  DWORD remaining = size;
  while (remaining > 0) {
    DWORD written = 0;
    if (!WriteFile(pipe, bytes, remaining, &written, nullptr) || written == 0) return false;
    bytes += written;
    remaining -= written;
  }
  return true;
}
bool ReadAll(HANDLE pipe, void *buffer, DWORD size) {
  unsigned char *bytes = static_cast<unsigned char *>(buffer);
  DWORD remaining = size;
  while (remaining > 0) {
    DWORD read = 0;
    if (!ReadFile(pipe, bytes, remaining, &read, nullptr) || read == 0) return false;
    bytes += read;
    remaining -= read;
  }
  return true;
}
HMODULE TolkModule() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&WriteAll), &module);
  return module;
}
std::wstring ModuleDirectory(HMODULE module) {
  wchar_t path[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring value(path, length);
  const size_t slash = value.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring(L".") : value.substr(0, slash);
}
bool EnsureDirectory(const std::wstring &path) {
  if (CreateDirectoryW(path.c_str(), nullptr)) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}
bool ExtractResource(HMODULE module, int id, const std::wstring &target) {
  HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!found) return false;
  const DWORD size = SizeofResource(module, found);
  HGLOBAL loaded = LoadResource(module, found);
  if (!loaded) return false;
  const void *data = LockResource(loaded);
  if (!data) return false;
  WIN32_FILE_ATTRIBUTE_DATA attributes = {};
  if (GetFileAttributesExW(target.c_str(), GetFileExInfoStandard, &attributes) &&
      attributes.nFileSizeHigh == 0 && attributes.nFileSizeLow == size) {
    return true;
  }
  HANDLE file = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  const unsigned char *bytes = static_cast<const unsigned char *>(data);
  DWORD remaining = size;
  while (remaining > 0) {
    DWORD written = 0;
    if (!WriteFile(file, bytes, remaining, &written, nullptr) || written == 0) break;
    bytes += written;
    remaining -= written;
  }
  CloseHandle(file);
  return remaining == 0;
}
std::wstring BridgeDirectory() {
  wchar_t buffer[MAX_PATH] = {};
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) length = GetTempPathW(MAX_PATH, buffer);
  std::wstring base(buffer, length);
  while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
  const std::wstring root = base + L"\\Tolk";
  EnsureDirectory(root);
  const std::wstring target = root + L"\\ZdCloud32";
  EnsureDirectory(target);
  return target;
}
}  // namespace
#endif

ScreenReaderDriverZDCloud::ScreenReaderDriverZDCloud() :
  ScreenReaderDriver(L"ZDCloud", true, false),
  controller(nullptr),
  initial(nullptr),
  speakAsync(nullptr),
  speakInsert(nullptr),
  stopSpeak(nullptr),
  uninitial(nullptr),
  initialized(false)
#if defined(_M_X64)
  , bridgePipe(INVALID_HANDLE_VALUE),
  bridgeProcess(nullptr),
  bridgeState(0)
#endif
{
#if defined(_M_X64)
  TOLK_LOG_INFO("ZDCloud: 64-bit build, using the embedded 32-bit bridge");
#elif defined(_WIN64)
  TOLK_LOG_INFO("ZDCloud: unavailable on this architecture");
#else
  Initialize();
#endif
}

ScreenReaderDriverZDCloud::~ScreenReaderDriverZDCloud() {
#if defined(_M_X64)
  StopBridge();
#else
  if (uninitial) uninitial();
  if (controller) FreeLibrary(controller);
#endif
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
  return false;
#else
  if (!controller) {
    TOLK_LOG_INFO("ZDCloud: Loading 32-bit ZDCloudAPI.dll");
    controller = LoadLibrary(L"ZDCloudAPI.dll");
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

#if defined(_M_X64)
bool ScreenReaderDriverZDCloud::StartBridge() {
  if (bridgePipe != INVALID_HANDLE_VALUE) return true;
  HMODULE module = TolkModule();
  if (!module) return false;
  const std::wstring directory = BridgeDirectory();
  const std::wstring exePath = directory + L"\\" + kBridgeExeName;
  const std::wstring dllPath = directory + L"\\" + kApiDllName;
  if (!ExtractResource(module, kBridgeResourceId, exePath)) {
    TOLK_LOG_WARN("ZDCloud: Failed to extract the 32-bit bridge, driver disabled");
    return false;
  }
  if (!ExtractResource(module, kApiResourceId, dllPath)) {
    TOLK_LOG_WARN("ZDCloud: Failed to extract ZDCloudAPI.dll, driver disabled");
    return false;
  }
  static unsigned long counter = 0;
  const std::wstring pipeName = L"\\\\.\\pipe\\TolkZdCloud_" +
      std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++counter);
  std::wstring commandLine = L"\"" + exePath + L"\" \"" + pipeName + L"\"";
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exePath.c_str(), &commandLine[0], nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) {
    TOLK_LOG_WARN("ZDCloud: Failed to start the 32-bit bridge (%lu)", GetLastError());
    return false;
  }
  CloseHandle(process.hThread);
  // Wait for the bridge to publish its pipe, but fail fast if it exits first.
  // WaitNamedPipe returns immediately when the pipe does not exist yet, so a
  // short sleep between attempts is required.
  bool available = false;
  for (int attempt = 0; attempt < 200 && !available; ++attempt) {
    if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) break;
    if (WaitNamedPipeW(pipeName.c_str(), 50)) available = true;
    else Sleep(25);
  }
  HANDLE pipe = available
      ? CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr)
      : INVALID_HANDLE_VALUE;
  if (pipe == INVALID_HANDLE_VALUE) {
    DWORD exitCode = 0;
    if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
      TOLK_LOG_WARN("ZDCloud: Bridge exited with code %lu", exitCode);
    TOLK_LOG_WARN("ZDCloud: Bridge did not become ready (%lu)", GetLastError());
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    return false;
  }
  DWORD mode = PIPE_READMODE_BYTE;
  SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
  TOLK_LOG_INFO("ZDCloud: 32-bit bridge started (pid %lu)", process.dwProcessId);
  bridgePipe = pipe;
  bridgeProcess = process.hProcess;
  return true;
}

bool ScreenReaderDriverZDCloud::CallBridge(unsigned long command, unsigned long argument, const wchar_t *text, int *result) {
  if (bridgePipe == INVALID_HANDLE_VALUE) {
    if (!StartBridge()) {
      bridgeState = -1;
      return false;
    }
    bridgeState = 1;
  }
  if (bridgeProcess && WaitForSingleObject(bridgeProcess, 0) == WAIT_OBJECT_0) {
    StopBridge();
    bridgeState = -1;
    return false;
  }
  const unsigned long length = text ? static_cast<unsigned long>(wcslen(text)) : 0;
  const BridgeHeader header = { command, argument, length };
  BridgeReply reply = { 0 };
  const bool sent = WriteAll(bridgePipe, &header, sizeof(header)) &&
      (length == 0 || WriteAll(bridgePipe, text, length * sizeof(wchar_t))) &&
      ReadAll(bridgePipe, &reply, sizeof(reply));
  if (!sent) {
    TOLK_LOG_WARN("ZDCloud: Bridge communication failed, driver disabled");
    StopBridge();
    bridgeState = -1;
    return false;
  }
  if (result) *result = reply.result;
  return true;
}

void ScreenReaderDriverZDCloud::StopBridge() {
  if (bridgePipe != INVALID_HANDLE_VALUE) {
    if (bridgeState == 1) {
      const BridgeHeader quit = { kCommandQuit, 0, 0 };
      WriteAll(bridgePipe, &quit, sizeof(quit));
    }
    CloseHandle(bridgePipe);
    bridgePipe = INVALID_HANDLE_VALUE;
  }
  if (bridgeProcess) {
    if (WaitForSingleObject(bridgeProcess, 1000) == WAIT_TIMEOUT) TerminateProcess(bridgeProcess, 0);
    CloseHandle(bridgeProcess);
    bridgeProcess = nullptr;
  }
  if (bridgeState == 1) bridgeState = 0;
}
#endif

bool ScreenReaderDriverZDCloud::Speak(const wchar_t *str, bool interrupt) {
  if (!str || !*str) return false;
#if defined(_M_X64)
  int result = 0;
  if (!CallBridge(kCommandSpeak, interrupt ? 1ul : 0ul, str, &result)) return false;
  return result == 0;
#elif defined(_WIN64)
  return false;
#else
  if (!initialized && !Initialize()) return false;
  if (interrupt) {
    if (stopSpeak) stopSpeak();
    return speakAsync ? (speakAsync(str, 1) == 0) : false;
  }
  return speakInsert ? (speakInsert(str, 1) == 0) : false;
#endif
}

bool ScreenReaderDriverZDCloud::Silence() {
#if defined(_M_X64)
  int result = 0;
  if (!CallBridge(kCommandStop, 0, nullptr, &result)) return false;
  return result == 0;
#elif defined(_WIN64)
  return false;
#else
  if (!initialized && !Initialize()) return false;
  if (!stopSpeak) return false;
  stopSpeak();
  return true;
#endif
}

bool ScreenReaderDriverZDCloud::IsActive() {
#if defined(_M_X64)
  if (bridgeState == 0) bridgeState = StartBridge() ? 1 : -1;
  return bridgeState == 1;
#elif defined(_WIN64)
  return false;
#else
  return initialized;
#endif
}
