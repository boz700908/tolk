# Tolk: Screen Reader Abstraction Library

## Introduction

Tolk is an application extension (DLL) that allows Windows applications to output text through screen reader software (assistive technology for the blind and visually impaired). It is an abstraction layer on top of the vendor-specific APIs that auto-detects the active screen reader, allowing for clean and simple client code. Speech and braille output are supported in 32-bit and 64-bit environments. See `Supported screen readers` for more details. In addition to screen readers, the Windows OneCore speech engine and Microsoft Speech API (SAPI) are supported as fallback speech engines. The name Tolk is a Dutch word meaning interpreter.

There are APIs for the following languages:

* C/C++
* Java
* Microsoft .NET (C# and VB)
* Python
* AutoIt
* PureBasic

## License

Tolk is distributed under the GNU Lesser General Public License version 3 (LGPLv3).

Client libraries and headers are distributed under their own license.

## Design

The key components of Tolk are the screen reader drivers. They wrap a specific screen reader API into an abstract interface which is then used by Tolk's auto-detection mechanism. The fallback speech engines, Windows OneCore and SAPI, albeit not screen readers, also have their own drivers. To keep things simple and secure, these screen reader drivers are not exposed to client code.

Functions that output text or that silence speech are asynchronous. That is, they return immediately once the appropriate commands have been queued for processing by the active screen reader. All other functions are synchronous. That is, they return only when their work is done.

Finally, a few words on multi-threaded applications. Tolk is not thread-safe. Also, some of the screen reader drivers use COM. You have two options for initializing COM:

1. Call `Tolk_Load` on every thread that uses Tolk. Match every call by a call to `Tolk_Unload`. Tolk will initialize COM if required.
2. Initialize and uninitialize COM yourself and call `Tolk_Load` only once in your application. You still need a matching call to `Tolk_Unload`. This is also what you should do in languages that automatically deal with COM, e.g. .NET.

## Usage

Tolk has functions for (un)initialization, querying and using the active screen reader, and for working with the fallback speech engines, Windows OneCore and Microsoft SAPI. To use Tolk, import the appropriate version of `Tolk.dll` into your application. In C/C++ this is usually done by including `Tolk.h` and linking with the appropriate import library `Tolk.lib`. You could also use the Windows API functions `LoadLibrary` and `FreeLibrary`. Other languages are also supported, see `Wrappers`. If you're working in an unsupported language, use its specific facilities to call into the DLL.

### Required files

The `lib` directory contains the required screen reader API DLLs. Tolk expects these DLLs to be found either in the current working directory or somewhere in the `PATH`. If a DLL for a screen reader is not found, that screen reader will be unavailable. Note that some screen readers use COM and therefore don't need API DLLs.

### Loading

Before using Tolk you need to initialize it with `Tolk_Load`, and uninitialize it with `Tolk_Unload` when you are done. You can then use all the other functions.

### Outputting text

The most important way to send text to the active screen reader is using `Tolk_Output`. The first parameter to this function is the Unicode string of text, the second parameter indicates whether or not previously queued speech should be interrupted (or canceled, flushed, etc). In languages that support this feature, the second parameter is optional and defaults to `false`. The advantage of using `Tolk_Output` is that it tries both speech and braille. If you need something more specialized, use `Tolk_Speak` for speech, `Tolk_Braille` for braille and `Tolk_Silence` to interrupt previously queued speech. All these functions return `true` on success and `false` otherwise, but because of the auto-detection mechanism it is recommended (and safe) to discard this return value and simply insert the required calls wherever you need screen reader output. This keeps your code clean and straight-forward.

### Querying status

There are functions to find out more about the active screen reader driver. You can get the name of the currently active screen reader through `Tolk_DetectScreenReader`. This returns the name as Unicode string or `NULL` if none of the supported screen readers is active. As the name implies, this function tries auto-detection if required. Internally, Tolk's other functions use this, so it is not necessary to call this yourself unless you actually need the common name.

If a screen reader is active, you can use `Tolk_HasSpeech` and `Tolk_HasBraille` to find out whether the driver supports speech or braille, respectively.

For synchronization, `Tolk_IsSpeaking` returns whether or not the active screen reader is speaking text at the time of the call, assuming the driver supports this query. Note that not many drivers implement this functionality because of limitations in screen reader APIs. See the `Status` column of the `Supported screen readers` table for details. There is no such function for braille, since braille is instantaneous.

### Using the fallback speech engines (OneCore and SAPI)

Tolk can output text through the Windows speech stack even when no screen reader is running. Two drivers provide this: OneCore, which uses the modern Windows speech engine (`Windows.Media.SpeechSynthesis`, the engine behind Narrator), and SAPI, which uses SAPI 5.4. This is mostly meant as a fallback mechanism, so the functionality is limited to what screen reader drivers provide. Applications that need more control should use the speech engine directly. Both engines are controlled explicitly via the Tolk_TrySAPI() and Tolk_PreferSAPI() functions.

By default, support for both fallback engines is enabled. To change this, use `Tolk_TrySAPI`, passing `true` to enable them or `false` to disable them. The required drivers will automatically be (un)loaded.

The fallback engines are initially put at the end of the auto-detection chain. This is good for using them as a fallback option when none of the supported screen readers is running. It is also possible to have Tolk prefer them over the other screen reader drivers. This is good for basic speech output where screen readers are only tried if the engines fail. To change the preference, use `Tolk_PreferSAPI`. This also takes a boolean parameter, `true` to prefer the fallback engines or `false` to prefer the traditional screen readers. OneCore, the newer engine, is always tried before SAPI.

The most efficient way of enabling the fallback engines is to set them up before calling `Tolk_Load`. However, you can also call these functions after Tolk has already been loaded. This will trigger the screen reader detection process and is therefore slightly less efficient.

### Using ZDCloud

Tolk also has a driver for ZDCloud (之多云). ZDCloud is a screen reader rather than a general-purpose speech backend: its speech output reads the content of its own features. The driver is enabled by default and placed after the other screen reader drivers and before the fallback speech engines in the auto-detection chain, so it is used when none of the other supported screen readers is active.

ZDCloud only ships a 32-bit module. On x64, ARM64 and ARM64EC builds Tolk therefore hosts it in a small embedded 32-bit helper process and talks to it over a named pipe (see `Cross-architecture support`).

### Cross-architecture support

Tolk is built for x86, x64, ARM64 and ARM64EC. Every backend uses the module of the first architecture that has one, in this order:

1. the native architecture of the build,
2. x64,
3. x86.

A module that the process cannot load itself is loaded by a helper process of that module's architecture, so the same rule also decides how a backend is bridged. In practice:

* x86 builds load every backend directly and embed no helper.
* Every architecture uses a native NVDA client (NVDA 2026.3 or later ships a native ARM64EC client next to the ARM64 one), so NVDA never needs a helper.
* The backends that have no ARM64 build use x64 on ARM64 and ARM64EC: an x64 module where one exists (System Access, ZDSR, BoyPCReader, PC-Talker) or the x64 COM view for the COM-only drivers (JAWS, Window-Eyes, ZoomText, Sense Reader). An ARM64EC process uses them in-process; on ARM64 they run in the embedded 64-bit helper.
* The backends without an x64 module at all (SuperNova and ZDCloud, which are 32-bit only) fall back to their x86 module and run in the embedded 32-bit helper.
* UIA and OneCore are Windows components, so every architecture uses its native version and no helper.
* Windows 11 on ARM runs the helpers through its x86 and x64 emulation.

The helper image is the only piece that is not shipped as-is: it is stored as a resource inside `Tolk.dll` and extracted to `%LOCALAPPDATA%\Tolk\Bridge\<arch>` on first use. The extracted image is removed again when Tolk unloads; if the host exits without unloading Tolk, the next run removes the leftovers. The helper processes themselves are stopped when the host process ends. The backend modules are exposed next to `Tolk.dll` exactly like a normal distribution - a non-x86 build carries the 64-bit module set plus the 32-bit modules that have no 64-bit build (the 32-bit-only backends, which the helper loads) - and no module is shipped twice in both widths.

Client modules are resolved from the directory that contains `Tolk.dll` first, and only then through the standard DLL search order. This matters for engines such as Unity, which load native plugins from a sub-directory (`<Game>_Data/Plugins/x86_64`) that is not part of the process search path: a bare module name would not be found there, and the affected backends would look unavailable even though their files are shipped next to `Tolk.dll`.

### Wrappers

Wrappers around `Tolk.dll` have been added for some languages to make things easier:

* **.NET (C#/VB.NET)**: Provides managed access to Tolk functions for .NET applications.
* **Java**: Enables Java applications to call Tolk functionality via JNI.
* **Python**: Offers a Pythonic interface for screen reader output.
* **AutoIt**: Allows AutoIt scripts to use Tolk for accessibility.
* **PureBasic**: Provides native bindings for PureBasic projects.

The wrappers cover all functions and use the language's native types where possible.

Game engines are covered by bindings that are not compiled with Tolk itself; see `contrib/game-engines/README.md`:

* **Unity**: A UPM package with a C# binding that degrades to no-ops when `Tolk.dll` is missing.
* **Unreal Engine**: A runtime plugin that exposes Tolk to Blueprints and C++.
* **Godot**: A GDExtension that registers a `Tolk` singleton for GDScript and C#.
* **GameMaker**: GML scripts over a small string-conversion shim.
* **LOVE**: The LuaJIT binding in `contrib/lua` works unchanged.

Every one of these bindings, including the game engine packages and the source-only layers, is collected in `dist/wrappers` of a release build; see `contrib/README.md`.

## Examples

Take a look at the `examples` directory to get started. This directory contains console applications in the supported languages that demonstrate the basic usage. Note that the fallback speech engines (OneCore and SAPI) will stop speaking when your application closes, which means that they will not work with these console applications because they return immediately after queueing text. Add a short delay (sleep) to work around this if you want to try them.

## Supported screen readers

The following table lists the supported screen readers in the order in which they are auto-detected. The architecture columns show how each backend actually runs on that architecture:

* `native` - the driver runs inside the `Tolk.dll` process using the API or module for that architecture.
* `x64` - the backend has no ARM64 build, so x64 is used: an ARM64EC process loads the x64 code in-process (under Windows' x64 emulation), an ARM64 process runs it in the embedded 64-bit helper.
* `x86` - the backend has no 64-bit build, so its 32-bit module runs in the embedded 32-bit helper.
* `Status` - the driver implements the `Tolk_IsSpeaking` speech-state query.

| Screen Reader | Speech | Braille | Status | x86 | x64 | ARM64 | ARM64EC |
|---------------|--------|---------|--------|-----|-----|-------|---------|
| NVDA          | Yes    | Yes     | Yes    | native | native | native | native |
| JAWS          | Yes    | Yes     | No     | native | native | x64 | x64 |
| Window-Eyes   | Yes    | Yes     | No     | native | native | x64 | x64 |
| System Access | Yes    | Yes     | No     | native | native | x64 | x64 |
| SuperNova     | Yes    | No      | No     | native | x86 | x86 | x86 |
| ZoomText      | Yes    | No      | Yes    | native | native | x64 | x64 |
| ZDSR          | Yes    | Yes     | Yes    | native | native | x64 | x64 |
| BoyPCReader   | Yes    | No      | Yes    | native | native | x64 | x64 |
| ZDCloud (之多云) | Yes    | No      | Yes    | native | x86 | x86 | x86 |
| PC-Talker     | Yes    | Yes     | Yes    | native | native | x64 | x64 |
| Sense Reader  | Yes    | No      | No     | native | native | x64 | x64 |
| UIA           | Yes    | No      | No     | native | native | native | native |
| OneCore       | Yes    | No      | Yes    | native | native | native | native |
| SAPI          | Yes    | No      | Yes    | native | native | partial* | partial* |

### Notes

* A backend that cannot run in the current process (no module or COM server for its architecture) runs through an embedded helper process; see `Cross-architecture support`. This is transparent to the caller.
* On ARM64, Windows 11 x86 and x64 emulation is required for the backends that have no ARM64 module.
* PC-Talker (Japanese) and Sense Reader (Korean) are regional screen readers. PC-Talker speaks and drives braille displays through `PCTKUSR.dll`; Sense Reader speaks through the COM server of `xvsrd.exe`. Neither ships an ARM64 build, so ARM64 drives them through the embedded 64-bit helper exactly like the other local readers.
* Sense Reader does not expose a speech-state query, so `Tolk_IsSpeaking` returns `false` while it is active.
* UIA speaks through UI Automation notification events, which a listening UIA client announces. It is only offered while a screen reader has set the Windows screen-reader flag, a UIA client is listening, a reader that consumes notification events is actually running and the calling application owns a visible top-level window to host the notification provider on; a process without such a window (for example a console tool) simply falls through to the fallback speech engines instead of going silent. The screen-reader flag and `UiaClientsAreListening` can stay set after the reader that raised them has exited, so those two signals alone would latch the driver on; Narrator is the mainstream reader that consumes UIA notifications and has no dedicated driver, so its process is required as well. Set `TOLK_UIA_ALWAYS=1` to skip that process check when another notification consumer is under test. It is tried after every named screen reader and before the fallback speech engines, and it is re-evaluated on every call instead of being latched onto, so a named screen reader that starts later takes over immediately. Because it is provided by Windows, every architecture uses its native component.
* OneCore speaks through the Windows speech engine behind Narrator (`Windows.Media.SpeechSynthesis`). Like SAPI it is an ordinary speech engine, not a screen reader that can be detected on its own: it does not depend on the Windows screen-reader flag, it is enabled and disabled together with SAPI through `Tolk_TrySAPI`, moved with `Tolk_PreferSAPI`, and always tried before SAPI. Because it is provided by Windows, every architecture uses its native component.
* The UIA and OneCore drivers require Windows 10 (1709 for UIA notifications). On older systems they report themselves unavailable and Tolk falls back as usual.
* The UIA, OneCore, PC-Talker and Sense Reader drivers were ported from the Prism project (https://github.com/ethindp/prism, MPL-2.0). Prism's D-Bus backends (Orca, speech-dispatcher, Spiel) are not ported: they depend on GLib/GIO and D-Bus and target Linux (or Wine) rather than a native Windows screen reader.
* ZDCloud (之多云) is a screen reader driver, not a general-purpose speech backend: its speech reads the content of its own features. It is tried after the other screen reader drivers and before the fallback speech engines.
* NVDA speech-state queries (`Tolk_IsSpeaking`) require NVDA 2026.3 or later, which introduced `nvdaController_isSpeaking`. On older versions `Tolk_IsSpeaking` returns `false`.
* SuperNova and ZDCloud only provide a 32-bit API. On 64-bit builds they are driven through the embedded 32-bit helper; see `Cross-architecture support`.
* SuperNova has support for braille, but the API does not let you use it.
* SuperNova can speak even if the user turned the voice off, but in that state interrupts will not work.
* Some screen readers (notably Window-Eyes and ZoomText) support many more functions, but there are no plans to implement any of them.
* SAPI is marked `Partial*` on ARM64 and ARM64EC because it is provided by Windows itself: it works there, but only voices installed for the process architecture can be used.
* The driver for Microsoft SAPI explicitly disables XML handling because there is no way to be sure SAPI is being used and other drivers don't support this.
* Window-Eyes is obsolete, but support has not yet been removed.

## Compiling

If you want to compile Tolk yourself, here's what you need to build the whole thing:

* Microsoft Visual C++
* Windows Software Development Kit (SDK)
* Java Development Kit (JDK)
* Microsoft .NET Framework
* Python
* Pandoc

The root directory and `examples` directories contain various batch files as a starting point. They assume the required tools are in your `PATH` and that the JDK include directory is in `INCLUDE`. For the examples you will also need to copy over any dependency files.

### Build Script Usage

```cmd
build.bat                        # Build both Debug and Release (all architectures)
build.bat debug                  # Build Debug only (all architectures)
build.bat release --x64          # Build Release only, for x64
build.bat both --x86 --x64       # Build Debug and Release for x86 and x64
build.bat release --arm64ec      # Build Release only, for ARM64EC
build.bat release --clean        # Remove build-*/ and dist/ before building
build.bat release --x64 --wrapper-tests   # Also compile the wrapper bindings (test only)
```

`build.bat` is also the entry point used by CI. It runs in non-interactive CI mode automatically when `GITHUB_ACTIONS`, `APPVEYOR`, `TF_BUILD` or `CI` is set: tool installation is skipped and the build always starts clean. On a local machine, missing build tools (CMake, Visual Studio Build Tools, and optionally .NET SDK, Java and Pandoc) are installed through Chocolatey, which requires Administrator privileges; pass `--no-bootstrap` to disable that. Requests for an architecture whose toolchain is not installed fail the build, except when ARM64 or ARM64EC is part of the default set, in which case it is skipped with a warning. The script returns a non-zero exit code if any requested build fails.

`--wrapper-tests` is a test-only extra: it compiles the wrapper bindings that do not need a game engine SDK (the GameMaker shim is linked against the real `Tolk` target and run, and the Unity C# binding is compiled with the .NET SDK), and it fails if a binding calls a `Tolk_*` entry point that `Tolk.h` no longer declares. The test targets are never installed and never part of a release package.

### Output Directory Structure

```
dist/
├── x86/
│   ├── Debug/      # 32-bit x86 Debug build (Tolk.dll, drivers, PDB symbols)
│   └── Release/    # 32-bit x86 Release build
├── x64/
│   ├── Debug/      # 64-bit x64 Debug build
│   └── Release/    # 64-bit x64 Release build
├── arm64/
│   ├── Debug/      # ARM64 Debug build (NVDA native, other backends via the bridge)
│   └── Release/    # ARM64 Release build
├── arm64ec/
│   ├── Debug/      # ARM64EC Debug build (x64 modules in-process, 32-bit backends via the bridge)
│   └── Release/    # ARM64EC Release build
├── wrappers/       # Shared language wrappers (all architectures)
│   ├── README.html # Wrapper overview
│   ├── dotnet/     # .NET wrapper (TolkDotNet.dll)
│   ├── java/       # Java wrapper (Tolk.jar)
│   ├── python/     # Python wrapper (Tolk.py)
│   ├── autoit/     # AutoIt wrapper (Tolk.au3)
│   ├── purebasic/  # PureBasic wrapper (Tolk.pb)
│   ├── lua/        # Lua / LuaJIT wrapper (Tolk.lua)
│   ├── java-iodine/  # Iodine Java wrapper
│   ├── oxygene/    # Oxygene wrapper
│   ├── swift-silver/ # Silver Swift wrapper
│   └── game-engines/ # Unity, Unreal, Godot and GameMaker packages
│       └── gamemaker/bin/<arch>/TolkGml.dll  # prebuilt shim per architecture
├── dotnet/         # Legacy .NET wrapper (backward compatibility)
├── java/           # Legacy Java wrapper (backward compatibility)
├── docs/           # Documentation
├── DEBUG_FEATURES.txt
└── LICENSE files
```

Every architecture folder also carries the prebuilt GameMaker shim `TolkGml.dll` next to `Tolk.dll`, and `dist/wrappers` repeats it per architecture under `game-engines/gamemaker/bin/<arch>` so that binding works without a compiler. Wrapper documentation ships as HTML only, exactly like this README, and the GameMaker shim ships only as the compiled `TolkGml.dll`; their Markdown and C sources stay in the repository.

## Debugging and Logging

Tolk includes a unified logging system that helps with debugging and troubleshooting.

### Debug vs Release Builds

| Feature | Debug Build | Release Build |
|---------|-------------|---------------|
| Optimization | Disabled (`/Od`) | Full Optimization (`/O2`) |
| Debug Symbols | Full PDB | None |
| Runtime Checks | Enabled (RTC1 + RTCsu) | Disabled |
| Log File Output | ✅ `Tolk_Debug.log` | ❌ None |
| Console Output (ERR/WRN) | ✅ Colored | ❌ None |
| OutputDebugString | ✅ All logs | ✅ All logs |

### Log File Location

When using the **Debug build**, Tolk automatically creates a log file in the **calling process working directory**:

```
YourApplication.exe
Tolk.dll           (Debug version)
Tolk_Debug.log     ← Auto-generated log file
```

### Log Format

All log entries include a millisecond-precision timestamp:

```
[2024-06-15 14:30:25.123][Tolk][INF] Tolk.cpp(123): Tolk_Load() called
[2024-06-15 14:30:25.125][Tolk][INF] ScreenReaderDriverNVDA.cpp(45): Loading nvdaControllerClient.dll
[2024-06-15 14:30:25.128][Tolk][ERR] ScreenReaderDriverBOY.cpp(78): Failed to load byctrl.dll
[2024-06-15 14:30:25.130][Tolk][WRN] ScreenReaderDriverZDSR.cpp(56): ZDSR not running
```

### Log Levels

* `[ERR]` - Errors (red in console)
* `[WRN]` - Warnings (yellow in console)
* `[INF]` - Information
* `[DBG]` - Debug details

### Viewing Logs

1. **Console**: Debug builds automatically print `[ERR]` and `[WRN]` messages to the console with color coding
2. **Log File**: `Tolk_Debug.log` contains all log entries with timestamps (UTF-8 encoded)
3. **DebugView**: All logs are sent to Windows `OutputDebugString` - use [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) to view them in real-time

### Disabling Logging

To completely disable logging at compile time, define `TOLK_DISABLE_LOGGING` before including `TolkDebug.h`.

## Contributors

* Davy Kager
* Leonard de Ruijter
* Axel Vugts
* QuentinC, who has developed [Universal Speech](https://github.com/QuentinC-Github/UniversalSpeech), another great screen reader library
