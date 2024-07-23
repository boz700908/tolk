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

// Global variable to store the reason value
//Reason: Reason for callback, 1=speaking completed, 2=Interrupted by new speaking, 3=Interrupted by stopped call
static int g_speakCompleteReason = -1; // -1 indicates no speech activity

// Callback function to be called when speaking is complete
void __stdcall SpeakCompleteCallback(int reason) {
  g_speakCompleteReason = reason;
}

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
    BoySpeak = (BoyCtrlSpeak)GetProcAddress(controller, "BoyCtrlSpeak");
    BoyStopSpeak = (BoyCtrlStopSpeaking)GetProcAddress(controller, "BoyCtrlStopSpeaking");
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
  g_speakCompleteReason = -1; // Reset the reason to indicate speaking has started
  return (BoySpeak(str, true, !interrupt, true, SpeakCompleteCallback) == 0);
}

bool ScreenReaderDriverBOY::Braille(const wchar_t *str) {
  return false;
}

bool ScreenReaderDriverBOY::Silence() {
  BoyStopSpeak(true);
  g_speakCompleteReason = 3;
  return true;
}

bool ScreenReaderDriverBOY::IsSpeaking() {
  return (g_speakCompleteReason == -1);
}

bool ScreenReaderDriverBOY::IsActive() {
  return BoyIsRunning();
}

bool ScreenReaderDriverBOY::Output(const wchar_t *str, bool interrupt) {
  // Beware short-circuiting.
  const bool speak = Speak(str, interrupt);
  const bool braille = Braille(str);
  return (speak || braille);
}
