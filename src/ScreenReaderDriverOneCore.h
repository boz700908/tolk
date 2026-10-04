/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverOneCore.h
 *  Description:    Driver for the Windows OneCore speech engine
 *                  (Windows.Media.SpeechSynthesis).
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_ONE_CORE_H_
#define _SCREEN_READER_DRIVER_ONE_CORE_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

class ScreenReaderDriverOneCore : public ScreenReaderDriver {
public:
  ScreenReaderDriverOneCore();
  ~ScreenReaderDriverOneCore() override;

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override;
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  struct Impl;
  Impl *impl;
  bool disabled;
};

#endif // _SCREEN_READER_DRIVER_ONE_CORE_H_
