/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverUIA.h
 *  Description:    Driver that speaks through UI Automation notification
 *                  events, which any listening UIA client (such as Narrator)
 *                  announces.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_UIA_H_
#define _SCREEN_READER_DRIVER_UIA_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

struct TolkUiaSession;

class ScreenReaderDriverUIA : public ScreenReaderDriver {
public:
  ScreenReaderDriverUIA();
  ~ScreenReaderDriverUIA() override;

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override { return false; }
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  static unsigned long __stdcall ThreadProc(void *self);
  void RunSession();
  void StopSession();
  bool EnsureSession();
  bool Post(const wchar_t *str, bool interrupt);

private:
  TolkUiaSession *session;
  bool disabled;
};

#endif // _SCREEN_READER_DRIVER_UIA_H_
