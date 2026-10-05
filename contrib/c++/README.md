# Tolk for C and C++

The native Tolk API. `Tolk.dll` exports a plain C interface, so C and C++ need
no wrapper: include the header and link the matching import library.

## Contents

A release package ships this folder as `dist/wrappers/c++`:

| Path                    | Description                                         |
|-------------------------|-----------------------------------------------------|
| `include/Tolk.h`        | The public header, identical for every architecture  |
| `include/TolkVersion.h` | Version macros                                       |
| `lib/x86/Tolk.lib`      | Import library for the 32-bit x86 build              |
| `lib/x64/Tolk.lib`      | Import library for the 64-bit x64 build              |
| `lib/arm64/Tolk.lib`    | Import library for the ARM64 build                   |
| `lib/arm64ec/Tolk.lib`  | Import library for the ARM64EC build                 |

Take the `lib/<arch>/Tolk.lib` that matches the process, exactly like the
runtime set in `dist/<arch>/<config>`. The header is shared by all of them.

## Runtime files

The import library only covers linking. To run, copy the whole matching
`dist/<arch>/<config>` folder (`Tolk.dll` plus the screen reader client
modules) next to the application; the cross-architecture bridge helper is
embedded in `Tolk.dll` and needs no extra file. See the repository README for
the full runtime layout.

## Linking

Command line (MSVC):

```cmd
cl /nologo /EHsc /I dist\wrappers\c++\include app.cpp dist\wrappers\c++\lib\x64\Tolk.lib
```

CMake:

```cmake
add_executable(app app.cpp)
target_include_directories(app PRIVATE "${TOLK_DIR}/wrappers/c++/include")
target_link_libraries(app PRIVATE "${TOLK_DIR}/wrappers/c++/lib/x64/Tolk.lib")
```

In Visual Studio, add `include` to *Additional Include Directories* and
`lib/<arch>/Tolk.lib` to *Additional Dependencies*.

## Example

```cpp
#include "Tolk.h"

int main() {
  Tolk_Load();
  Tolk_Output(L"Hello from Tolk!");
  Tolk_Unload();
  return 0;
}
```

`Tolk_Output` and the other output functions are asynchronous, so keep the
process alive while the text is spoken. The `examples/c++` folder in a source
checkout shows a complete console application.

See `Tolk.h` for the documented API, and
https://github.com/boz700908/tolk/blob/master/docs/README.md for the full
manual.
