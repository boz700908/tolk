/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSNova.h
 *  Description:    Driver for the SuperNova screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_SNOVA_H_
#define _SCREEN_READER_DRIVER_SNOVA_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

class ScreenReaderDriverSNova : public ScreenReaderDriver {
public:
  ScreenReaderDriverSNova();
  ~ScreenReaderDriverSNova();

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override { return false; }
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  typedef DWORD (__stdcall *DolAccess_GetSystem)();
  typedef DWORD (__stdcall *DolAccess_Action)(int);
  typedef DWORD (__stdcall *DolAccess_Command)(const wchar_t *, int, int);

private:
  HINSTANCE controller;
  DolAccess_GetSystem dolAccess_GetSystem;
  DolAccess_Action dolAccess_Action;
  DolAccess_Command dolAccess_Command;
};

#endif // _SCREEN_READER_DRIVER_SNOVA_H_
