# Tolk for Godot (GDExtension)

A Godot 4 extension that registers a `Tolk` singleton, so GDScript, C# and
GDExtension code can all speak and braille through the active screen reader.

```gdscript
func _ready() -> void:
    Tolk.load()
    print("Screen reader: ", Tolk.detect_screen_reader())
    Tolk.output("Game started")

func _exit_tree() -> void:
    Tolk.unload()
```

## Building

The extension links against Tolk, so build Tolk first (see the repository
README). Then:

1. Get [godot-cpp](https://github.com/godotengine/godot-cpp) for Godot 4 and
   place it in this folder as `godot-cpp`, or set `GODOT_CPP_ROOT`.
2. Build:

   ```
   scons platform=windows target=template_release arch=x86_64
   scons platform=windows target=template_debug arch=x86_64
   ```

   `TOLK_ROOT` defaults to the repository root and `TOLK_LIB_DIR` to
   `dist/<arch>/Release`; override them if your build lives elsewhere.
3. Copy `Tolk.dll` and every screen reader client module from
   `dist/<arch>/<config>` into `addons/tolk/bin/`, next to the built
   `tolk.windows.<target>.<arch>.dll`.

## API

All methods live on the `Tolk` singleton:

* `load()` / `unload()` / `is_loaded()`
* `try_sapi(enabled)` / `prefer_sapi(preferred)` - the fallback speech engines
  (Windows OneCore and SAPI); OneCore is tried before SAPI
* `detect_screen_reader()` - name of the active screen reader, `""` if none
* `has_speech()` / `has_braille()`
* `output(text, interrupt = false)` - speech and/or braille
* `speak(text, interrupt = false)` / `braille(text)`
* `is_speaking()` / `silence()`

## Notes

* This is a Windows-only extension. Tolk.dll and the screen reader modules must
  be discoverable next to the extension when Godot starts.
* The `windows.arm64` entry needs a Godot build with Windows on ARM support;
  remove entries you do not build for.