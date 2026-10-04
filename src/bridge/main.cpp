/**
 *  Product:        Tolk
 *  File:           bridge/main.cpp
 *  Description:    Architecture helper that hosts the screen reader drivers a
 *                  Tolk.dll cannot load in-process. The image is embedded into
 *                  Tolk.dll and extracted at run time, so no extra file is
 *                  shipped next to Tolk.dll.
 *  License:        LGPLv3
 */
#include <windows.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "../ScreenReaderDriver.h"
#include "../ScreenReaderDriverBOY.h"
#include "../ScreenReaderDriverJAWS.h"
#include "../ScreenReaderDriverNVDA.h"
#include "../ScreenReaderDriverSA.h"
#include "../ScreenReaderDriverSNova.h"
#include "../ScreenReaderDriverWE.h"
#include "../ScreenReaderDriverZDCloud.h"
#include "../ScreenReaderDriverZDSR.h"
#include "../ScreenReaderDriverZT.h"

namespace {

// Pipe protocol, shared with src/TolkBridge.cpp.
const unsigned long kCommandPing = 0;
const unsigned long kCommandIsActive = 1;
const unsigned long kCommandSpeak = 2;
const unsigned long kCommandBraille = 3;
const unsigned long kCommandSilence = 4;
const unsigned long kCommandIsSpeaking = 5;
const unsigned long kCommandQuit = 6;

// Backend identifiers, shared with src/TolkBridge.h.
const unsigned long kBackendNVDA = 1;
const unsigned long kBackendJAWS = 2;
const unsigned long kBackendWE = 3;
const unsigned long kBackendSA = 4;
const unsigned long kBackendSNova = 5;
const unsigned long kBackendZDSR = 6;
const unsigned long kBackendBOY = 7;
const unsigned long kBackendZDCloud = 8;
const unsigned long kBackendZT = 9;

const unsigned long kMaximumTextLength = 1u << 20;

struct BridgeRequest {
  unsigned long command;
  unsigned long backend;
  unsigned long argument;
  unsigned long length;
};

struct BridgeReply {
  int result;
};

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

std::wstring ModuleDirectory() {
  wchar_t path[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring value(path, length);
  const size_t slash = value.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring(L".") : value.substr(0, slash);
}

std::unique_ptr<ScreenReaderDriver> CreateDriver(unsigned long backend) {
  switch (backend) {
    case kBackendNVDA:   return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverNVDA());
    case kBackendJAWS:   return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverJAWS());
    case kBackendWE:     return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverWE());
    case kBackendSA:     return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverSA());
    case kBackendSNova:  return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverSNova());
    case kBackendZDSR:   return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverZDSR());
    case kBackendBOY:    return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverBOY());
    case kBackendZDCloud:return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverZDCloud());
    case kBackendZT:     return std::unique_ptr<ScreenReaderDriver>(new ScreenReaderDriverZT());
    default: return std::unique_ptr<ScreenReaderDriver>();
  }
}

}  // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc < 2) return 1;
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  // Load the backend modules from Tolk.dll's directory, where the distribution
  // exposes them, and from this helper's own directory.
  const std::wstring directory = ModuleDirectory();
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
  AddDllDirectory(directory.c_str());
  if (argc >= 3 && argv[2] && *argv[2]) AddDllDirectory(argv[2]);
  // Publish the pipe immediately so the host can connect as soon as the
  // process starts instead of waiting for a backend to initialize.
  HANDLE pipe = CreateNamedPipeW(argv[1], PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1 << 16, 1 << 16, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    CoUninitialize();
    return 5;
  }
  if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
    CloseHandle(pipe);
    CoUninitialize();
    return 6;
  }
  std::map<unsigned long, std::unique_ptr<ScreenReaderDriver> > drivers;
  for (;;) {
    BridgeRequest request = {};
    if (!ReadAll(pipe, &request, sizeof(request))) break;
    if (request.command == kCommandQuit) break;
    if (request.length > kMaximumTextLength) break;
    std::wstring text;
    if (request.command == kCommandSpeak || request.command == kCommandBraille) {
      text.resize(request.length, L'\0');
      if (request.length > 0 && !ReadAll(pipe, &text[0], request.length * sizeof(wchar_t))) break;
    }
    ScreenReaderDriver *driver = nullptr;
    std::map<unsigned long, std::unique_ptr<ScreenReaderDriver> >::iterator found = drivers.find(request.backend);
    if (found == drivers.end()) {
      std::unique_ptr<ScreenReaderDriver> created;
      try {
        created = CreateDriver(request.backend);
      }
      catch (...) {
        created.reset();
      }
      found = drivers.insert(std::make_pair(request.backend, std::move(created))).first;
    }
    if (found->second) driver = found->second.get();
    BridgeReply reply = { 0 };
    if (driver) {
      switch (request.command) {
        case kCommandPing:       reply.result = 1; break;
        case kCommandIsActive:   reply.result = driver->IsActive() ? 1 : 0; break;
        case kCommandSpeak:      reply.result = driver->Speak(text.c_str(), (request.argument & 1) != 0) ? 1 : 0; break;
        case kCommandBraille:    reply.result = driver->Braille(text.c_str()) ? 1 : 0; break;
        case kCommandSilence:    reply.result = driver->Silence() ? 1 : 0; break;
        case kCommandIsSpeaking: reply.result = driver->IsSpeaking() ? 1 : 0; break;
        default: break;
      }
    }
    if (!WriteAll(pipe, &reply, sizeof(reply))) break;
  }
  drivers.clear();
  CloseHandle(pipe);
  CoUninitialize();
  return 0;
}
