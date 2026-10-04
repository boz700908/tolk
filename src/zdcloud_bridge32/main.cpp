/**
 *  Product:        Tolk
 *  File:           zdcloud_bridge32/main.cpp
 *  Description:    32-bit bridge that exposes the ZDCloud speech backend to
 *                  the 64-bit Tolk.dll. A 64-bit process cannot load the
 *                  32-bit ZDCloudAPI.dll, so this helper is built as a
 *                  standalone 32-bit executable. On 64-bit builds its image
 *                  (together with ZDCloudAPI.dll) is embedded into Tolk.dll
 *                  and extracted at run time, so no extra files are shipped.
 *  License:        LGPLv3
 */
#include <windows.h>
#include <string>

namespace {

typedef void (__stdcall *ZDCloud_Callback)(int);
typedef int (__stdcall *ZDCloud_Initial)(const wchar_t *, const wchar_t *, ZDCloud_Callback);
typedef int (__stdcall *ZDCloud_Speak)(const wchar_t *, int);
typedef void (__stdcall *ZDCloud_Void)();

// Obfuscated application credentials (XOR encoded, decoded at runtime).
const unsigned char kAppKey[] = { 0x0A, 0x5E, 0x5C, 0x5F, 0x12, 0x1F, 0x09, 0x1F, 0x59, 0x5E };
const unsigned char kSecKey[] = { 0x4E, 0x48, 0x00, 0x0D, 0x00, 0x45, 0x44, 0x0D, 0x5A, 0x58 };

// Keep in sync with the constants in ScreenReaderDriverZDCloud.cpp.
const unsigned long kCommandSpeak = 1;
const unsigned long kCommandStop = 2;
const unsigned long kCommandQuit = 3;

struct BridgeHeader {
  unsigned long command;
  unsigned long argument;
  unsigned long length;
};

struct BridgeReply {
  int result;
};

std::wstring DecodeSecret(const unsigned char *bytes, size_t size, unsigned char key) {
  std::wstring value;
  value.reserve(size);
  for (size_t index = 0; index < size; ++index) {
    value.push_back(static_cast<wchar_t>(bytes[index] ^ key));
  }
  return value;
}

std::wstring ModuleDirectory() {
  wchar_t path[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring value(path, length);
  const size_t slash = value.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring(L".") : value.substr(0, slash);
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

}  // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc < 2) return 1;
  // Publish the pipe immediately so the 64-bit side can connect as soon as the
  // process starts instead of waiting for the backend to initialize.
  HANDLE pipe = CreateNamedPipeW(argv[1], PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1 << 16, 1 << 16, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) return 5;
  const std::wstring directory = ModuleDirectory();
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
  AddDllDirectory(directory.c_str());
  const std::wstring library = directory + L"\\ZDCloudAPI.dll";
  HMODULE module = LoadLibraryExW(library.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
  if (!module) {
    CloseHandle(pipe);
    return 2;
  }
  ZDCloud_Initial initial = (ZDCloud_Initial)GetProcAddress(module, "Initial");
  ZDCloud_Speak speakAsync = (ZDCloud_Speak)GetProcAddress(module, "SpeakAsync");
  ZDCloud_Speak speakInsert = (ZDCloud_Speak)GetProcAddress(module, "SpeakInsert");
  ZDCloud_Void stopSpeak = (ZDCloud_Void)GetProcAddress(module, "StopSpeak");
  ZDCloud_Void uninitial = (ZDCloud_Void)GetProcAddress(module, "UnInitial");
  if (!initial || !speakAsync || !speakInsert) {
    CloseHandle(pipe);
    FreeLibrary(module);
    return 3;
  }
  const std::wstring appKey = DecodeSecret(kAppKey, sizeof(kAppKey), 0x6E);
  const std::wstring secKey = DecodeSecret(kSecKey, sizeof(kSecKey), 0x72);
  int result = initial(appKey.c_str(), secKey.c_str(), nullptr);
  if (result != 0) result = initial(appKey.c_str(), secKey.c_str(), nullptr);
  if (result != 0) {
    CloseHandle(pipe);
    if (uninitial) uninitial();
    FreeLibrary(module);
    return 4;
  }
  if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
    CloseHandle(pipe);
    if (uninitial) uninitial();
    FreeLibrary(module);
    return 6;
  }

  for (;;) {
    BridgeHeader header = {};
    if (!ReadAll(pipe, &header, sizeof(header))) break;
    if (header.command == kCommandQuit) break;
    BridgeReply reply = { 0 };
    if (header.command == kCommandSpeak) {
      std::wstring text(header.length, L'\0');
      if (header.length > 0 && !ReadAll(pipe, &text[0], header.length * sizeof(wchar_t))) break;
      if ((header.argument & 1) != 0) {
        if (stopSpeak) stopSpeak();
        reply.result = speakAsync(text.c_str(), 1);
      } else {
        reply.result = speakInsert(text.c_str(), 1);
      }
    } else if (header.command == kCommandStop) {
      if (stopSpeak) stopSpeak();
      reply.result = 0;
    }
    if (!WriteAll(pipe, &reply, sizeof(reply))) break;
  }

  CloseHandle(pipe);
  if (uninitial) uninitial();
  FreeLibrary(module);
  return 0;
}
