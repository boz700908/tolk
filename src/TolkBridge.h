/**
 *  Product:        Tolk
 *  File:           TolkBridge.h
 *  Description:    Cross-architecture bridge for screen reader backends that
 *                  do not ship a module for the current architecture.
 *  License:        LGPLv3
 */
#ifndef _TOLK_BRIDGE_H_
#define _TOLK_BRIDGE_H_

#include <windows.h>

// ---------------------------------------------------------------------------
// Architecture
//
// Tolk can be built as x86, x64, ARM64 and ARM64EC. Some backends only ship a
// DLL for one architecture (SuperNova and ZDCloud are 32-bit only, System
// Access, ZDSR and BoyPCReader ship 32-bit and 64-bit builds but no ARM64
// build). A build that cannot load such a module in-process drives it through
// a small helper process of the matching architecture (see src/bridge) and
// talks to it over a named pipe. The helper image is embedded into Tolk.dll
// as a resource and extracted at run time, so no extra file is shipped next
// to Tolk.dll.
// ---------------------------------------------------------------------------
#if defined(_M_ARM64EC)
  #define TOLK_HOST_ARCH_ARM64EC 1
#elif defined(_M_ARM64)
  #define TOLK_HOST_ARCH_ARM64 1
#elif defined(_M_X64)
  #define TOLK_HOST_ARCH_X64 1
#elif defined(_M_IX86)
  #define TOLK_HOST_ARCH_X86 1
#else
  #error "Tolk: unsupported target architecture"
#endif

// Which module architectures the current process can load in-process.
// ARM64EC is a hybrid ABI: it runs x64 modules, but not pure ARM64 ones.
#if defined(TOLK_HOST_ARCH_X86)
  #define TOLK_CAN_LOAD_X86 1
  #define TOLK_CAN_LOAD_X64 0
  #define TOLK_CAN_LOAD_ARM64 0
#elif defined(TOLK_HOST_ARCH_X64)
  #define TOLK_CAN_LOAD_X86 0
  #define TOLK_CAN_LOAD_X64 1
  #define TOLK_CAN_LOAD_ARM64 0
#elif defined(TOLK_HOST_ARCH_ARM64EC)
  #define TOLK_CAN_LOAD_X86 0
  #define TOLK_CAN_LOAD_X64 1
  #define TOLK_CAN_LOAD_ARM64 0
#else
  #define TOLK_CAN_LOAD_X86 0
  #define TOLK_CAN_LOAD_X64 0
  #define TOLK_CAN_LOAD_ARM64 1
#endif

// Helper process architectures, matching the embedded bridge images.
enum TolkBridgeArch {
  TolkBridgeArchX86 = 0,
  TolkBridgeArchX64 = 1
};

// Backend identifiers. Keep in sync with src/bridge/main.cpp.
enum TolkBridgeBackend {
  TolkBridgeBackendNVDA = 1,
  TolkBridgeBackendJAWS = 2,
  TolkBridgeBackendWE = 3,
  TolkBridgeBackendSA = 4,
  TolkBridgeBackendSNova = 5,
  TolkBridgeBackendZDSR = 6,
  TolkBridgeBackendBOY = 7,
  TolkBridgeBackendZDCloud = 8,
  TolkBridgeBackendZT = 9
};

// Pipe protocol commands. Keep in sync with src/bridge/main.cpp.
enum TolkBridgeCommand {
  TolkBridgeCommandPing = 0,
  TolkBridgeCommandIsActive = 1,
  TolkBridgeCommandSpeak = 2,
  TolkBridgeCommandBraille = 3,
  TolkBridgeCommandSilence = 4,
  TolkBridgeCommandIsSpeaking = 5,
  TolkBridgeCommandQuit = 6
};

// The helper architecture that hosts `backend` when the current build cannot
// load its module in-process.
TolkBridgeArch TolkBridgeArchForBackend(TolkBridgeBackend backend);

// Host-side client. Every client shares the helper process and pipe of its
// architecture, so several bridged backends cost one process per architecture.
class TolkBridgeClient {
public:
  explicit TolkBridgeClient(TolkBridgeBackend backend);
  ~TolkBridgeClient();
  TolkBridgeClient(const TolkBridgeClient &) = delete;
  TolkBridgeClient &operator=(const TolkBridgeClient &) = delete;
public:
  bool Speak(const wchar_t *text, bool interrupt);
  bool Braille(const wchar_t *text);
  bool Silence();
  bool IsSpeaking();
  bool IsActive();
private:
  bool Call(int command, unsigned long argument, const wchar_t *text, int *result);
private:
  TolkBridgeBackend backend;
  void *channel;
  bool cachedActive;
  unsigned long cachedActiveTime;
  bool disabled;
};

#endif // _TOLK_BRIDGE_H_
