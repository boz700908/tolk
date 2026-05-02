/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZT.h
 *  Description:    Driver for the ZoomText screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_ZT_H_
#define _SCREEN_READER_DRIVER_ZT_H_

#include "zt.h"
#include "ScreenReaderDriver.h"

class ScreenReaderDriverZT : public ScreenReaderDriver {
public:
  ScreenReaderDriverZT();
  ~ScreenReaderDriverZT();

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override;
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  void Initialize();
  void Finalize();
  bool IsRunning() { return (!!FindWindow(L"ZXSPEECHWNDCLASS", L"ZoomText Speech Processor")); }

private:
  IZoomText2 *controller;
  ISpeech2 *speech;
};

#endif // _SCREEN_READER_DRIVER_ZT_H_
