# Tolk for game engines

Bindings that make `Tolk.dll` available to the major game engines. They are
source-only and are not compiled with Tolk; each folder has its own README with
build and install instructions.

| Engine           | Folder       | Technology                                   |
|------------------|--------------|----------------------------------------------|
| Unity            | `unity`      | UPM package with a guarded C# binding         |
| Unreal Engine    | `unreal`     | Runtime plugin (Blueprints + C++)             |
| Godot            | `godot`      | GDExtension registering a `Tolk` singleton    |
| GameMaker        | `gamemaker`  | GML scripts plus a UTF-8/UTF-16 shim, prebuilt per architecture in a release package |

Engines that already have a language binding in the main tree can use it
directly:

* **LOVE** and any other LuaJIT host: `contrib/lua/Tolk.lua`.
* **Ren'Py** and any Python host: `src/python/Tolk.py` (built into
  `dist/wrappers/python`).
* Plain C/C++ engines (Cocos2d-x, OGRE, custom engines): include `Tolk.h` and
  link `Tolk.lib` from `dist/<arch>/<config>`.

## The one rule that matters

Every binding is a thin layer over the same native library, so the same two
things apply everywhere:

1. `Tolk.dll` and its screen reader client modules must be next to the game
   executable (or otherwise on the DLL search path). Copy the whole matching
   `dist/<arch>/<config>` folder; the cross-architecture bridge helper is
   embedded in `Tolk.dll` and needs no extra file.
2. Pick the architecture that matches the player. Use `x64` for 64-bit builds,
   `x86` for 32-bit builds, and `arm64`/`arm64ec` on Windows on ARM. A module
   of the wrong bitness fails to load; the bindings either report that or
   degrade to no-ops.

## Shared API

All bindings expose the same calls, named after the engine's conventions:

* initialize/release: `load` / `unload` / `is_loaded`
* fallback speech engines: `try_sapi(enabled)` / `prefer_sapi(preferred)` -
  these control OneCore and SAPI together, and OneCore is tried first
* detection and capability: `detect_screen_reader()`, `has_speech()`,
  `has_braille()`
* output: `output(text, interrupt)`, `speak(text, interrupt)`,
  `braille(text)`, `is_speaking()`, `silence()`
