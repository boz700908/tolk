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

// Global variable to store the reason value.
// Reason: 0=no speech activity, -1=speaking, 1=speaking completed,
// 2=interrupted by new speaking, 3=interrupted by stopped call.
static int g_speakCompleteReason = 0;

// Callback function to be called when speaking is complete
void __stdcall SpeakCompleteCallback(int reason) {
  g_speakCompleteReason = reason;
}

ScreenReaderDriverBOY::ScreenReaderDriverBOY() :
  ScreenReaderDriver(L"BoyPCReader", true, false),
  #ifdef _WIN64
  controller(LoadLibrary(L"byctrl-x64.dll")),
  #else
  controller(LoadLibrary(L"byctrl.dll")),
  #endif
  initialized(false),
  BoySpeak(NULL),
  BoyStopSpeak(NULL),
  BoyInit(NULL),
  BoyUninit(NULL),
  BoyIsRunning(NULL)
{
  if (!controller) return;

  BoyInit = (BoyCtrlInitialize)GetProcAddress(controller, "BoyCtrlInitialize");
  BoyUninit = (BoyCtrlUninitialize)GetProcAddress(controller, "BoyCtrlUninitialize");
  BoyIsRunning = (BoyCtrlIsReaderRunning)GetProcAddress(controller, "BoyCtrlIsReaderRunning");
  BoySpeak = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
  BoyStopSpeak = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");

  if (!BoyInit || !BoyUninit || !BoyIsRunning || !BoySpeak || !BoyStopSpeak) {
    FreeLibrary(controller);
    controller = NULL;
    BoyInit = NULL;
    BoyUninit = NULL;
    BoyIsRunning = NULL;
    BoySpeak = NULL;
    BoyStopSpeak = NULL;
    return;
  }

  if (BoyInit(NULL) != 0) {
    FreeLibrary(controller);
    controller = NULL;
    BoyInit = NULL;
    BoyUninit = NULL;
    BoyIsRunning = NULL;
    BoySpeak = NULL;
    BoyStopSpeak = NULL;
    return;
  }

  initialized = true;
}

ScreenReaderDriverBOY::~ScreenReaderDriverBOY() {
  if (initialized && BoyUninit) {
    BoyUninit();
    initialized = false;
  }
  if (controller) {
    FreeLibrary(controller);
    controller = NULL;
  }
}

bool ScreenReaderDriverBOY::Speak(const wchar_t *str, bool interrupt) {
  if (!initialized || !BoySpeak) return false;
  g_speakCompleteReason = -1;
  if (BoySpeak(str, !interrupt, SpeakCompleteCallback) == 0) return true;
  g_speakCompleteReason = 0;
  return false;
}

bool ScreenReaderDriverBOY::Braille(const wchar_t *str) {
  (void)str;
  return false;
}

bool ScreenReaderDriverBOY::Silence() {
  if (initialized && BoyStopSpeak && BoyStopSpeak() == 0) {
    g_speakCompleteReason = 3;
    return true;
  }
  return false;
}

bool ScreenReaderDriverBOY::IsSpeaking() {
  return (initialized && g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::IsActive() {
  if (initialized && BoyIsRunning) return BoyIsRunning();
  return false;
}

bool ScreenReaderDriverBOY::Output(const wchar_t *str, bool interrupt) {
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
