/**
 *  Product:        Tolk
 *  File:           tolk_wrappers_test.c
 *  Description:    Minimal smoke test for the wrapper bindings. Only built
 *                  when TOLK_BUILD_WRAPPER_TESTS is enabled, so it is never
 *                  part of a normal or shipped build.
 *  License:        LGPLv3
 */
#include <stdio.h>

__declspec(dllimport) double __cdecl TolkGml_Load(void);
__declspec(dllimport) double __cdecl TolkGml_IsLoaded(void);
__declspec(dllimport) double __cdecl TolkGml_Unload(void);

int main(void)
{
  if (TolkGml_Load() != 1.0)
  {
    printf("FAIL: TolkGml_Load() did not report a loaded library\n");
    return 1;
  }
  if (TolkGml_IsLoaded() != 1.0)
  {
    printf("FAIL: TolkGml_IsLoaded() returned false after TolkGml_Load()\n");
    return 1;
  }
  TolkGml_Unload();
  printf("PASS: the GameMaker wrapper shim built, linked and ran\n");
  return 0;
}