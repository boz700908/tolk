/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverZDCloud.h
 *  Description:    Driver for the ZDCloud (之多云) screen reader.
 *  License:        LGPLv3
 */
#ifndef _SCREEN_READER_DRIVER_ZDCLOUD_H_
#define _SCREEN_READER_DRIVER_ZDCLOUD_H_
#include <windows.h>
#include <string>
#include "ScreenReaderDriver.h"
// ZDCloud (之多云) is a screen reader; its speech reads the content of its own
// features. It has no public header, so the exports are resolved dynamically
// to keep Tolk usable when it is not present.
class ScreenReaderDriverZDCloud : public ScreenReaderDriver {
public:
  ScreenReaderDriverZDCloud();
  ~ScreenReaderDriverZDCloud() override;
public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override { return false; }
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }
private:
  typedef void (__stdcall *ZDCloud_Callback)(int);
  typedef int (__stdcall *ZDCloud_Initial)(const wchar_t *, const wchar_t *, ZDCloud_Callback);
  typedef int (__stdcall *ZDCloud_Speak)(const wchar_t *, int);
  typedef void (__stdcall *ZDCloud_Void)();
  static std::wstring DecodeSecret(const unsigned char *bytes, size_t size, unsigned char key);
  bool Initialize();
  bool ClientRunning();
private:
  HMODULE controller;
  ZDCloud_Initial initial;
  ZDCloud_Speak speakAsync;
  ZDCloud_Speak speakInsert;
  ZDCloud_Speak speakTry;
  ZDCloud_Void stopSpeak;
  ZDCloud_Void uninitial;
  bool initialized;
};
#endif // _SCREEN_READER_DRIVER_ZDCLOUD_H_
