# Tolk for Unity

A Unity package that wraps `Tolk.dll`. It exposes the same API as the other
Tolk wrappers and degrades gracefully: if the native library is missing, every
call becomes a safe no-op.

## Installing

Copy the `unity` folder into your project's `Packages` folder and rename it to
`com.davykager.tolk`, or add it through *Package Manager > Add package from
disk*. Then copy the native library as described in
`Runtime/Plugins/README.md`.

```csharp
using DavyKager;
using UnityEngine;

public sealed class Announcer : MonoBehaviour
{
  private void Awake() => Tolk.Initialize();

  public void Say(string text) => Tolk.Output(text);

  private void OnDestroy() => Tolk.Shutdown();
}
```

Add the optional `TolkBehaviour` component to a GameObject to load Tolk
automatically, or call `Tolk.Initialize()` yourself.

## API

`Tolk` mirrors the native library, with every call guarded by
`Tolk.IsAvailable`:

* `Initialize()` / `Shutdown()` / `IsInitialized()`
* `TrySAPI(bool)` / `PreferSAPI(bool)` - control the fallback speech engines,
  the Windows OneCore engine and SAPI. OneCore is tried before SAPI.
* `DetectScreenReader()` - name of the active screen reader, or `null`
* `HasSpeech()` / `HasBraille()`
* `Output(string, bool interrupt = false)` - speech and/or braille
* `Speak(string, bool interrupt = false)` / `Braille(string)`
* `IsSpeaking()` / `Silence()`

## Notes

* Unity's IL2CPP and Mono backends both support the `DllImport` calls used
  here; no managed plugin configuration is required.
* On Windows on ARM the ARM64 (or ARM64EC) build of Tolk works, but the screen
  readers that only ship x86 or x64 modules still need Windows' emulation.
* The package assembly only compiles for the editor and Windows standalone
  players. Remove the `includePlatforms` entries from
  `Runtime/DavyKager.Tolk.asmdef` if you need it elsewhere.