/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSAPI.h
 *  Description:    Driver for the Microsoft Speech API (SAPI).
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_SAPI_H_
#define _SCREEN_READER_DRIVER_SAPI_H_

#include <sapi.h>
#include "ScreenReaderDriver.h"

class ScreenReaderDriverSAPI : public ScreenReaderDriver {
public:
  ScreenReaderDriverSAPI();
  ~ScreenReaderDriverSAPI();

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override;
  bool Silence() override;
  bool IsActive() override { return (!!controller); }
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  void Initialize();
  void Finalize();

private:
  ISpVoice *controller;
};

#endif // _SCREEN_READER_DRIVER_SAPI_H_
