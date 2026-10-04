/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverPCTalker.h
 *  Description:    Driver for the PC-Talker screen reader.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_PC_TALKER_H_
#define _SCREEN_READER_DRIVER_PC_TALKER_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

class PcTalkerApi;
class PcTalkerBraille;

class ScreenReaderDriverPCTalker : public ScreenReaderDriver {
public:
  ScreenReaderDriverPCTalker();
  ~ScreenReaderDriverPCTalker() override;

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *str) override;
  bool IsSpeaking() override;
  bool Silence() override;
  bool IsActive() override;

private:
  void Load();
  void Unload();

private:
  PcTalkerApi *api;
  PcTalkerBraille *braille;
  unsigned long long brailleContext;
};

#endif // _SCREEN_READER_DRIVER_PC_TALKER_H_
