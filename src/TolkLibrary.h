/**
 *  Product:        Tolk
 *  File:           TolkLibrary.h
 *  Description:    Loader for the screen reader client modules.
 *  License:        LGPLv3
 */

#ifndef _TOLK_LIBRARY_H_
#define _TOLK_LIBRARY_H_

#include <windows.h>

// Loads a module by file name. The directory that contains Tolk.dll is tried
// first, so the client modules shipped next to Tolk.dll are found even when the
// host application keeps its native plugins in a sub-directory (Unity loads
// Tolk.dll from <Game>_Data/Plugins/x86_64, which is not part of the process
// DLL search path). When the module is not there the standard search order is
// used, which preserves loading from the application directory.
HMODULE TolkLoadLibrary(const wchar_t *name);

#endif // _TOLK_LIBRARY_H_
