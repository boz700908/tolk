# Tolk for Unreal Engine

A runtime plugin that exposes Tolk to Blueprints and C++. Tolk is loaded when
the module starts and unloaded when the engine shuts down.

## Installing

1. Copy the `Tolk` folder into your project's `Plugins` directory (create it
   if necessary).
2. Fill in `ThirdParty/Tolk` as described in `ThirdParty/Tolk/README.md`.
3. Enable the plugin in *Edit > Plugins* and restart the editor.

## Blueprint usage

All functions are in the **Tolk** category:

| Node                  | Purpose                                                        |
|-----------------------|----------------------------------------------------------------|
| `Load` / `Unload`     | Initialize or release Tolk (done automatically at startup)      |
| `Detect Screen Reader`| Name of the active screen reader, empty when none is active     |
| `Output`              | Speak and/or braille text (preferred call)                      |
| `Speak` / `Braille`   | Speech only / braille only                                     |
| `Is Speaking`         | Whether the driver is speaking right now                        |
| `Silence`             | Cancel speech                                                  |
| `Has Speech` / `Has Braille` | What the active driver supports                         |
| `Try SAPI` / `Prefer SAPI` | Control the fallback speech engines (OneCore and SAPI)    |

## C++ usage

```cpp
#include "TolkBlueprintLibrary.h"

UTolkBlueprintLibrary::Output(TEXT("Game started"));
```

or call the native API directly (`#include "Tolk.h"`).

## Notes

* Unreal's game thread is not the thread Tolk is initialized on if you call
  the API from a background thread; call `Load`/`Output` from the game thread.
* `Tolk.dll` and the screen reader modules must be present next to the game
  executable. They are declared as runtime dependencies so the editor and the
  packager copy them automatically.
* On Windows on ARM, use the `arm64` (or `arm64ec`) build in
  `ThirdParty/Tolk/runtime/arm64`.