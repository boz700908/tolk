# Tolk for GameMaker

GameMaker strings are UTF-8 while Tolk speaks UTF-16, so a tiny shim,
`TolkGml.dll`, does the conversion and forwards everything to `Tolk.dll`.

## Getting the shim

A Tolk release package already contains the prebuilt shim for every
architecture under `bin/<arch>/TolkGml.dll`, so you can go straight to step 3
below and copy the one that matches your game. The shim's C source and its
`build.bat` are not part of the package; to build it for an architecture the
package does not cover, use a Tolk source checkout:

1. Build Tolk for the architecture your game uses (`build.bat release --x64`
   or `--x86`).
2. Open a matching *Native Tools Command Prompt* and run `build.bat x64` (or
   `build.bat x86`) in the gamemaker folder of the checkout.
3. Copy `TolkGml.dll`, `Tolk.dll` and every screen reader client module from
   `dist/<arch>/<config>` into the game's working directory, or ship them as
   *Included Files*. `TolkGml.dll` and `Tolk.dll` must have the same
   architecture (bitness), otherwise loading fails.

## Usage

Add `scripts/tolk/tolk.gml` to the project, then:

```gml
// Create event of the first object
tolk_init();
show_debug_message("Screen reader: " + tolk_detect_screen_reader());

// Anywhere
tolk_output("Game started");
tolk_speak("Only speech", true);
tolk_braille("Brailled only");
tolk_silence();
```

`tolk_init()` is optional: every other function calls it on first use and it
is safe to call repeatedly.

## API

| Function | Purpose |
|----------|---------|
| `tolk_init()` | Define the DLL functions and load Tolk |
| `tolk_load()` / `tolk_unload()` / `tolk_is_loaded()` | Manage the Tolk lifetime |
| `tolk_try_sapi(enabled)` | Enable or disable the fallback speech engines (OneCore and SAPI) |
| `tolk_prefer_sapi(preferred)` | Put the fallback engines first or last |
| `tolk_detect_screen_reader()` | Name of the active screen reader, or `""` |
| `tolk_has_speech()` / `tolk_has_braille()` | What the active driver supports |
| `tolk_output(text, interrupt)` | Speak and/or braille (preferred) |
| `tolk_speak(text, interrupt)` / `tolk_braille(text)` | Speech only / braille only |
| `tolk_is_speaking()` / `tolk_silence()` | Speech state and cancellation |

## Notes

* Only the shim's `const char *` return value is copied by GameMaker; it is a
  fixed internal buffer, so do not keep the returned pointer around.
* GameMaker loads the DLL lazily; if `TolkGml.dll` is missing, `tolk_init()`
  reports an error and the wrapper functions return `false`.
