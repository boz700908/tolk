# Tolk wrappers

Every wrapper layer for `Tolk.dll`, in one place. This folder is copied to
`dist/wrappers` when Tolk is packaged, so the release archive contains all of
them:

| Folder            | Language / engine | Form                                   |
|-------------------|-------------------|----------------------------------------|
| `dotnet`          | C# / VB.NET       | `TolkDotNet.dll` (netstandard2.0, net40) |
| `java`            | Java              | `Tolk.jar` (JNI)                       |
| `python`          | Python            | `Tolk.py` (+ bytecode)                 |
| `autoit`          | AutoIt            | `Tolk.au3`                             |
| `purebasic`       | PureBasic         | `Tolk.pb`                              |
| `lua`             | Lua / LuaJIT      | `Tolk.lua` (FFI; works in LOVE)        |
| `java-iodine`     | Java (Iodine)     | `Tolk.java` + sample                   |
| `oxygene`         | Oxygene           | `Tolk.pas` + sample                    |
| `swift-silver`    | Swift (Silver)    | `Tolk.swift` + sample                  |
| `game-engines/unity`   | Unity        | UPM package (C#)                       |
| `game-engines/unreal`  | Unreal Engine | Runtime plugin (Blueprints + C++)     |
| `game-engines/godot`   | Godot 4      | GDExtension singleton                  |
| `game-engines/gamemaker` | GameMaker  | GML scripts + conversion shim          |

The `dotnet`, `java` and `python` folders are built by `build.bat`; the others
are source-only and are shipped as-is, except for the GameMaker shim which is
compiled per architecture (see below). See `game-engines/README.md` for the
engine bindings and the repository README for the native API.

## In a release package

A release build copies this layout into `dist/wrappers`, shipping the
documentation as HTML, and adds things that are not in the repository:

* every Markdown document here is rendered to `.html` and the Markdown source
  is dropped, so the package never carries the same text twice;
* `game-engines/gamemaker/bin/<arch>/TolkGml.dll` holds the prebuilt
  conversion shim for each architecture, so the GameMaker binding can be used
  without a compiler. The shim source and `game-engines/gamemaker/build.bat`
  are not shipped; take them from a source checkout if you need to build the
  shim for an architecture the package does not cover.

## Common requirements

* Every wrapper ultimately calls the same `Tolk.dll`, so the matching native
  files must be next to the application: copy the whole
  `dist/<arch>/<config>` folder (`Tolk.dll` plus the screen reader client
  modules). The cross-architecture bridge helper is embedded in `Tolk.dll` and
  needs no extra file.
* Pick the architecture that matches the process: `x86` for 32-bit, `x64` for
  64-bit, `arm64`/`arm64ec` on Windows on ARM.
* The native API is identical everywhere: load, detect, output, speak, braille,
  query speech state, silence, and the `Tolk_TrySAPI` / `Tolk_PreferSAPI`
  switches that control the fallback speech engines (Windows OneCore and SAPI,
  with OneCore tried first).
