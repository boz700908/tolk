/**
 *  Product:        Tolk
 *  File:           TolkLibrary.cpp
 *  Description:    Loader for the screen reader client modules.
 *  License:        LGPLv3
 */

#include "TolkLibrary.h"

#include <string>

namespace {

// The module this code lives in, resolved through a function address so the
// loader does not need a module handle of its own.
HMODULE TolkModule() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&TolkLoadLibrary), &module);
  return module;
}

} // namespace

HMODULE TolkLoadLibrary(const wchar_t *name) {
  if (!name || !*name) return nullptr;
  HMODULE module = TolkModule();
  if (module) {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      std::wstring full(path, length);
      const std::wstring::size_type slash = full.find_last_of(L"\\/");
      if (slash != std::wstring::npos) {
        full.resize(slash + 1);
        full += name;
        // LOAD_WITH_ALTERED_SEARCH_PATH also resolves the dependencies of the
        // loaded module against its own directory, so client modules that need
        // sibling files keep working.
        HMODULE loaded = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (loaded) return loaded;
      }
    }
  }
  return LoadLibraryW(name);
}
