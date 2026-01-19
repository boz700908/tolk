/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.cpp
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

// The BOY Project provides a header and libraries,
// but we don't use these in order to support running even if the DLL is missing.

#include "ScreenReaderDriverBOY.h"

ScreenReaderDriverBOY::ScreenReaderDriverBOY() :
  ScreenReaderDriver(L"BoyPCReader", true, false),
  #ifdef _WIN64
  controller(LoadLibrary(L"BoyCtrl-x64.dll")),
  #else
  controller(LoadLibrary(L"BoyCtrl.dll")),
  #endif
  BoySpeak(NULL),
  BoyStopSpeak(NULL),
  BoyInit(NULL),
  BoyUninit(NULL),
  BoyIsRunning(NULL)
{
  if (controller) {
    BoyInit = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
    BoyUninit = (BoyCtrlUninitialize)GetProcAddress(controller, "BoyCtrlUninitialize");
    BoyIsRunning = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
    BoySpeak = (BoyCtrlSpeak2)GetProcAddress(controller, "BoyCtrlSpeak2");
    BoyStopSpeak = (BoyCtrlStopSpeaking2)GetProcAddress(controller, "BoyCtrlStopSpeaking2");
        BoyInit(NULL);
  }
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY() {
  if (controller) {
BoyUninit();
FreeLibrary(controller);
}
}

bool ScreenReaderDriverBOY::Speak(const wchar_t *str, bool interrupt) {
  if (BoySpeak) return (BoySpeak(str) == 0);
  return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t *str) {
  return false;
}

bool ScreenReaderDriverBOY::Silence() {
  if (BoyStopSpeak) {
    BoyStopSpeak();
    return true;
  }

bool ScreenReaderDriverBOY::IsActive() {
  if (BoyIsRunning) return BoyIsRunning();
  return false;
}

bool ScreenReaderDriverBOY::Output(const wchar_t *str, bool interrupt) {
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}