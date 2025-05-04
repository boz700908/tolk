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

typedef void (__stdcall*BoyCtrlSpeakCompleteFunc)(int reason);

typedef enum
{
	e_bcerr_success = 0,
	e_bcerr_fail = 1,
	e_bcerr_arg = 2,
	e_bcerr_unavailable = 3,
} BoyCtrlError;

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
  typedef int (BoyCtrlError __stdcall *BoyCtrlInitializeu8)(const wchar_t* pathName);
  typedef void (__stdcall *BoyCtrlUninitialize)();
  typedef bool (__stdcall *BoyCtrlIsReaderRunning)();
  typedef int (BoyCtrlError __stdcall *BoyCtrlSpeaku8)(const wchar_t* text, bool withSlave, bool append, bool allowBreak, BoyCtrlSpeakCompleteFunc onCompletion);
  typedef int (BoyCtrlError __stdcall *BoyCtrlStopSpeaking)(bool withSlave);


private:
  HINSTANCE controller;
  BoyCtrlInitialize BoyInit;
  BoyCtrlUninitialize BoyUninit;
  BoyCtrlIsReaderRunning BoyIsRunning;
  BoyCtrlSpeak BoySpeak;
  BoyCtrlStopSpeaking BoyStopSpeak;
};

#endif // _SCREEN_READER_DRIVER_BOY_H_
