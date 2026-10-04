# ThirdParty/Tolk

This plugin does not vendor the Tolk binaries. Populate the folders before
building the game:

```
ThirdParty/Tolk/
  include/
    Tolk.h                 <- copy from <repo>/src/Tolk.h
  runtime/
    x64/                   <- copy everything from dist/x64/Release
      Tolk.lib
      Tolk.dll
      nvdaControllerClient64.dll, SAAPI64.dll, ... (the screen reader modules)
    arm64/                 <- optional, copy everything from dist/arm64/Release
```

`Tolk.lib` is the import library; the DLLs listed in the runtime folder are
copied next to the game executable by `Tolk.Build.cs`. The cross-architecture
bridge helper needs no extra file: it is embedded inside `Tolk.dll`.