/**
 *  Product:        Tolk
 *  File:           tolk_gml.c
 *  Description:    GameMaker shim. GameMaker passes UTF-8 strings while Tolk
 *                  speaks UTF-16, so this small bridge does the conversion and
 *                  forwards everything to Tolk.dll.
 *  License:        LGPLv3
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "Tolk.h"

#define TOLK_GML_EXPORT __declspec(dllexport)

static wchar_t *TolkGml_Wide(const char *text)
{
  if (text == NULL)
  {
    text = "";
  }
  int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
  if (length <= 0)
  {
    return _wcsdup(L"");
  }
  wchar_t *buffer = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
  if (buffer == NULL)
  {
    return NULL;
  }
  MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, length);
  return buffer;
}

/* GameMaker keeps the pointer only until it copies the string, so a static
   buffer is enough and avoids leaking. */
static char *TolkGml_Utf8(const wchar_t *text, char *buffer, int bufferSize)
{
  buffer[0] = '\0';
  if (text != NULL && bufferSize > 0)
  {
    WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, bufferSize, NULL, NULL);
  }
  return buffer;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Load(void)
{
  Tolk_Load();
  return Tolk_IsLoaded() ? 1.0 : 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Unload(void)
{
  Tolk_Unload();
  return 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_IsLoaded(void)
{
  return Tolk_IsLoaded() ? 1.0 : 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_TrySAPI(double enabled)
{
  Tolk_TrySAPI(enabled != 0.0);
  return 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_PreferSAPI(double preferred)
{
  Tolk_PreferSAPI(preferred != 0.0);
  return 0.0;
}

TOLK_GML_EXPORT const char *__cdecl TolkGml_DetectScreenReader(void)
{
  static char buffer[256];
  return TolkGml_Utf8(Tolk_DetectScreenReader(), buffer, (int)sizeof(buffer));
}

TOLK_GML_EXPORT double __cdecl TolkGml_HasSpeech(void)
{
  return Tolk_HasSpeech() ? 1.0 : 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_HasBraille(void)
{
  return Tolk_HasBraille() ? 1.0 : 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Output(const char *text, double interrupt)
{
  wchar_t *wide = TolkGml_Wide(text);
  double result = Tolk_Output(wide, interrupt != 0.0) ? 1.0 : 0.0;
  free(wide);
  return result;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Speak(const char *text, double interrupt)
{
  wchar_t *wide = TolkGml_Wide(text);
  double result = Tolk_Speak(wide, interrupt != 0.0) ? 1.0 : 0.0;
  free(wide);
  return result;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Braille(const char *text)
{
  wchar_t *wide = TolkGml_Wide(text);
  double result = Tolk_Braille(wide) ? 1.0 : 0.0;
  free(wide);
  return result;
}

TOLK_GML_EXPORT double __cdecl TolkGml_IsSpeaking(void)
{
  return Tolk_IsSpeaking() ? 1.0 : 0.0;
}

TOLK_GML_EXPORT double __cdecl TolkGml_Silence(void)
{
  return Tolk_Silence() ? 1.0 : 0.0;
}