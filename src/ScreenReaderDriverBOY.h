/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBOY.h
 *  Description:    Driver for the BOY screen reader.
 *  Copyright:      (c) 2024, qt06<qt06.com@gmail.com>
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_BOY_H_
#define _SCREEN_READER_DRIVER_BOY_H_

#include <windows.h>
#include "ScreenReaderDriver.h"

// Global variable to store the reason value
//Reason: Reason for callback, 1=speaking completed, 2=Interrupted by new speaking, 3=Interrupted by stopped call

typedef void (__stdcall*BoyCtrlSpeakCompleteFunc)(int reason);

class ScreenReaderDriverBOY : public ScreenReaderDriver {
public:
  ScreenReaderDriverBOY();
  ~ScreenReaderDriverBOY();

public:
  bool Speak(const wchar_t *str, bool interrupt);
  bool Braille(const wchar_t *str);
  bool IsSpeaking();
  bool Silence();
  bool IsActive();
  bool Output(const wchar_t *str, bool interrupt);

private:
  typedef int (__stdcall *BoyCtrlInitializeU8)(const wchar_t* pathName);
  typedef void (__stdcall *BoyCtrlUninitialize)();
  typedef bool (__stdcall *BoyCtrlIsReaderRunning)();
  typedef int (__stdcall *BoyCtrlSpeakU8)(const wchar_t* text, bool withSlave, bool append, bool allowBreak, BoyCtrlSpeakCompleteFunc onCompletion);
  typedef int (__stdcall *BoyCtrlStopSpeaking)(bool withSlave);


private:
  HINSTANCE controller;
  BoyCtrlInitializeU8 BoyInit;
  BoyCtrlUninitialize BoyUninit;
  BoyCtrlIsReaderRunning BoyIsRunning;
  BoyCtrlSpeakU8 BoySpeak;
  BoyCtrlStopSpeaking BoyStopSpeak;
};

#endif // _SCREEN_READER_DRIVER_BOY_H_
