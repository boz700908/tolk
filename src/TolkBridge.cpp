/**
 *  Product:        Tolk
 *  File:           TolkBridge.cpp
 *  Description:    Cross-architecture bridge for screen reader backends.
 *  License:        LGPLv3
 */
#include "TolkBridge.h"
#include "TolkDebug.h"
#include <map>
#include <mutex>
#include <string>

namespace {

struct BridgeRequest {
  unsigned long command;
  unsigned long backend;
  unsigned long argument;
  unsigned long length;
};

struct BridgeReply {
  int result;
};

struct PayloadEntry {
  int resourceId;
  const wchar_t *fileName;
};

// Resource ids embedded by src/CMakeLists.txt. Keep the two in sync.
const int kBridgeImageResource[2] = { 101, 102 };

// Modules the 32-bit helper needs: the 32-bit only backends.
const PayloadEntry kX86Payloads[] = {
  { 250, L"dolapi32.dll" },     // SuperNova
  { 280, L"ZDCloudAPI.dll" }    // ZDCloud
};

// Modules the 64-bit helper needs: the 64-bit backends used from ARM64.
const PayloadEntry kX64Payloads[] = {
  { 241, L"SAAPI64.dll" },      // System Access
  { 261, L"ZDSRAPI_x64.dll" },  // ZDSR
  { 266, L"ZDSRAPI.ini" },
  { 271, L"byctrl-x64.dll" },   // BoyPCReader
  { 276, L"byctrl.conf" }
};

const wchar_t kBridgeExeName[] = L"TolkBridge.exe";
const DWORD kBridgeWaitStepMs = 25;
const int kBridgeWaitAttempts = 200;
const unsigned long kMaximumTextLength = 1u << 20;
const unsigned long kIsActiveCacheMs = 100;

const wchar_t *ArchName(TolkBridgeArch arch) {
  return arch == TolkBridgeArchX86 ? L"x86" : L"x64";
}

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

std::wstring ArchDirectory(TolkBridgeArch arch) {
  wchar_t buffer[MAX_PATH] = {};
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) length = GetTempPathW(MAX_PATH, buffer);
  std::wstring base(buffer, length);
  while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
  const std::wstring root = base + L"\\Tolk\\Bridge";
  EnsureDirectory(base + L"\\Tolk");
  EnsureDirectory(root);
  const std::wstring target = root + (arch == TolkBridgeArchX86 ? L"\\x86" : L"\\x64");
  EnsureDirectory(target);
  return target;
}

// One helper process per architecture, shared by every bridged backend.
struct BridgeChannel {
  TolkBridgeArch arch;
  HANDLE pipe;
  HANDLE process;
  int references;
  std::mutex io;
};

std::mutex g_registryMutex;
std::map<int, BridgeChannel *> g_channels;

bool StartBridgeChannel(BridgeChannel &channel) {
  const wchar_t *name = ArchName(channel.arch);
  HMODULE module = TolkModule();
  if (!module) return false;
  const std::wstring directory = ArchDirectory(channel.arch);
  const std::wstring exePath = directory + L"\\" + kBridgeExeName;
  if (!ExtractResource(module, kBridgeImageResource[channel.arch], exePath)) {
    TOLK_LOG_WARN("TolkBridge: no embedded %ls helper image", name);
    return false;
  }
  const PayloadEntry *payloads = channel.arch == TolkBridgeArchX86 ? kX86Payloads : kX64Payloads;
  const size_t payloadCount = channel.arch == TolkBridgeArchX86
      ? sizeof(kX86Payloads) / sizeof(kX86Payloads[0])
      : sizeof(kX64Payloads) / sizeof(kX64Payloads[0]);
  for (size_t index = 0; index < payloadCount; ++index) {
    if (!ExtractResource(module, payloads[index].resourceId, directory + L"\\" + payloads[index].fileName)) {
      TOLK_LOG_WARN("TolkBridge: failed to extract %ls for the %ls helper", payloads[index].fileName, name);
    }
  }
  static unsigned long counter = 0;
  const std::wstring pipeName = L"\\\\.\\pipe\\TolkBridge_" +
      std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(static_cast<int>(channel.arch)) +
      L"_" + std::to_wstring(++counter);
  std::wstring commandLine = L"\"" + exePath + L"\" \"" + pipeName + L"\"";
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exePath.c_str(), &commandLine[0], nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process)) {
    TOLK_LOG_WARN("TolkBridge: failed to start the %ls helper (%lu)", name, GetLastError());
    return false;
  }
  CloseHandle(process.hThread);
  // WaitNamedPipe returns immediately when the pipe does not exist yet, so a
  // short sleep between attempts is required.
  bool available = false;
  for (int attempt = 0; attempt < kBridgeWaitAttempts && !available; ++attempt) {
    if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) break;
    if (WaitNamedPipeW(pipeName.c_str(), 50)) available = true;
    else Sleep(kBridgeWaitStepMs);
  }
  HANDLE pipe = available
      ? CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr)
      : INVALID_HANDLE_VALUE;
  if (pipe == INVALID_HANDLE_VALUE) {
    DWORD exitCode = 0;
    if (GetExitCodeProcess(process.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
      TOLK_LOG_WARN("TolkBridge: %ls helper exited with code %lu", name, exitCode);
    else
      TOLK_LOG_WARN("TolkBridge: %ls helper did not become ready", name);
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    return false;
  }
  DWORD mode = PIPE_READMODE_BYTE;
  SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
  TOLK_LOG_INFO("TolkBridge: %ls helper started (pid %lu)", name, process.dwProcessId);
  channel.pipe = pipe;
  channel.process = process.hProcess;
  return true;
}

BridgeChannel *AcquireChannel(TolkBridgeArch arch) {
  std::lock_guard<std::mutex> lock(g_registryMutex);
  std::map<int, BridgeChannel *>::iterator found = g_channels.find(static_cast<int>(arch));
  if (found != g_channels.end()) {
    ++found->second->references;
    return found->second;
  }
  BridgeChannel *channel = new BridgeChannel();
  channel->arch = arch;
  channel->pipe = INVALID_HANDLE_VALUE;
  channel->process = nullptr;
  channel->references = 1;
  if (!StartBridgeChannel(*channel)) {
    delete channel;
    return nullptr;
  }
  g_channels[static_cast<int>(arch)] = channel;
  return channel;
}

void ReleaseChannel(BridgeChannel *channel) {
  if (!channel) return;
  std::lock_guard<std::mutex> lock(g_registryMutex);
  if (--channel->references > 0) return;
  g_channels.erase(static_cast<int>(channel->arch));
  std::lock_guard<std::mutex> io(channel->io);
  if (channel->pipe != INVALID_HANDLE_VALUE) {
    const BridgeRequest quit = { static_cast<unsigned long>(TolkBridgeCommandQuit), 0, 0, 0 };
    WriteAll(channel->pipe, &quit, sizeof(quit));
    CloseHandle(channel->pipe);
    channel->pipe = INVALID_HANDLE_VALUE;
  }
  if (channel->process) {
    if (WaitForSingleObject(channel->process, 1000) == WAIT_TIMEOUT) TerminateProcess(channel->process, 0);
    CloseHandle(channel->process);
    channel->process = nullptr;
  }
  delete channel;
}

bool CallChannel(BridgeChannel *channel, int command, TolkBridgeBackend backend,
                 unsigned long argument, const wchar_t *text, int *result) {
  if (!channel) return false;
  const unsigned long length = text ? static_cast<unsigned long>(wcslen(text)) : 0;
  if (length > kMaximumTextLength) return false;
  std::lock_guard<std::mutex> lock(channel->io);
  if (channel->pipe == INVALID_HANDLE_VALUE) return false;
  if (channel->process && WaitForSingleObject(channel->process, 0) == WAIT_OBJECT_0) return false;
  const BridgeRequest request = { static_cast<unsigned long>(command), static_cast<unsigned long>(backend), argument, length };
  BridgeReply reply = { 0 };
  if (!WriteAll(channel->pipe, &request, sizeof(request))) return false;
  if (length > 0 && !WriteAll(channel->pipe, text, length * sizeof(wchar_t))) return false;
  if (!ReadAll(channel->pipe, &reply, sizeof(reply))) return false;
  if (result) *result = reply.result;
  return true;
}

}  // namespace

TolkBridgeArch TolkBridgeArchForBackend(TolkBridgeBackend backend) {
  switch (backend) {
    case TolkBridgeBackendSNova:
    case TolkBridgeBackendZDCloud:
      return TolkBridgeArchX86;
    default:
      return TolkBridgeArchX64;
  }
}

TolkBridgeClient::TolkBridgeClient(TolkBridgeBackend backend) :
  backend(backend),
  channel(nullptr),
  cachedActive(false),
  cachedActiveTime(0),
  disabled(false)
{
}

TolkBridgeClient::~TolkBridgeClient() {
  ReleaseChannel(static_cast<BridgeChannel *>(channel));
}

bool TolkBridgeClient::Call(int command, unsigned long argument, const wchar_t *text, int *result) {
  if (disabled) return false;
  if (!channel) {
    channel = AcquireChannel(TolkBridgeArchForBackend(backend));
    if (!channel) {
      disabled = true;
      return false;
    }
  }
  if (!CallChannel(static_cast<BridgeChannel *>(channel), command, backend, argument, text, result)) {
    TOLK_LOG_WARN("TolkBridge: communication failed, backend %d disabled", static_cast<int>(backend));
    ReleaseChannel(static_cast<BridgeChannel *>(channel));
    channel = nullptr;
    disabled = true;
    return false;
  }
  return true;
}

bool TolkBridgeClient::Speak(const wchar_t *text, bool interrupt) {
  if (!text || !*text) return false;
  int result = 0;
  return Call(TolkBridgeCommandSpeak, interrupt ? 1ul : 0ul, text, &result) && result != 0;
}

bool TolkBridgeClient::Braille(const wchar_t *text) {
  if (!text || !*text) return false;
  int result = 0;
  return Call(TolkBridgeCommandBraille, 0, text, &result) && result != 0;
}

bool TolkBridgeClient::Silence() {
  int result = 0;
  return Call(TolkBridgeCommandSilence, 0, nullptr, &result) && result != 0;
}

bool TolkBridgeClient::IsSpeaking() {
  int result = 0;
  return Call(TolkBridgeCommandIsSpeaking, 0, nullptr, &result) && result != 0;
}

bool TolkBridgeClient::IsActive() {
  const unsigned long now = GetTickCount();
  if (cachedActiveTime != 0 && (now - cachedActiveTime) < kIsActiveCacheMs) return cachedActive;
  int result = 0;
  cachedActive = Call(TolkBridgeCommandIsActive, 0, nullptr, &result) && result != 0;
  cachedActiveTime = now;
  return cachedActive;
}
