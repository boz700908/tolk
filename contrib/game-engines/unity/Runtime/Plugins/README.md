# Native plugins

Copy the contents of the matching Tolk build into this folder before building
the game:

| Target                | Source folder                | Unity folder      |
|-----------------------|------------------------------|-------------------|
| Windows 32-bit        | `dist/x86/Release`           | `Plugins/x86`     |
| Windows 64-bit        | `dist/x64/Release`           | `Plugins/x86_64`  |
| Windows on ARM64/EC   | `dist/arm64/Release`         | `Plugins/ARM64`   |
| Debug builds          | `dist/<arch>/Debug`          | same folders      |

Copy the whole folder, not just `Tolk.dll`: the screen reader client modules
(`nvdaControllerClient*.dll`, `SAAPI64.dll`, `ZDSRAPI_x64.dll`, `PCTKUSR.dll`
and so on) have to sit next to `Tolk.dll`. The cross-architecture bridge helper
is embedded in `Tolk.dll` and needs no extra file.

Unity must be set to the matching CPU architecture in *Project Settings >
Player > Configuration*. A 32-bit `Tolk.dll` in a 64-bit player fails to load;
the wrapper detects this and reports `Tolk.IsAvailable == false`.