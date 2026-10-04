/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverSenseReader.h
 *  Description:    Driver for the Sense Reader screen reader.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 */

#ifndef _SCREEN_READER_DRIVER_SENSE_READER_H_
#define _SCREEN_READER_DRIVER_SENSE_READER_H_

#include <windows.h>
#include <oaidl.h>
#include <oleauto.h>
#include "ScreenReaderDriver.h"

// Sense Reader (Xvision) exposes an out-of-process COM server (xvsrd.exe) whose
// default interface is IXVApplication. The vtable below mirrors the vendor's
// generated type library header, which is the only contract that matters for
// early binding; only Speak and StopSpeaking are used.
struct IXVApplication : public IDispatch {
  virtual HRESULT __stdcall get_SpeechControlers(IUnknown **pControlers) = 0;
  virtual HRESULT __stdcall get_SpeechControler(IUnknown **pControler) = 0;
  virtual HRESULT __stdcall Speak(BSTR text) = 0;
  virtual HRESULT __stdcall StopSpeaking() = 0;
  virtual HRESULT __stdcall RegisterAutoReadExceptionWindow(long wnd, long option, VARIANT_BOOL applyChildren) = 0;
  virtual HRESULT __stdcall RemoveAutoReadExceptionWindow(long wnd) = 0;
  virtual HRESULT __stdcall get_DemoMode(VARIANT_BOOL *pVal) = 0;
};

class ScreenReaderDriverSenseReader : public ScreenReaderDriver {
public:
  ScreenReaderDriverSenseReader();
  ~ScreenReaderDriverSenseReader() override;

public:
  bool Speak(const wchar_t *str, bool interrupt) override;
  bool Braille(const wchar_t *) override { return false; }
  bool IsSpeaking() override { return false; }
  bool Silence() override;
  bool IsActive() override;
  bool Output(const wchar_t *str, bool interrupt) override { return Speak(str, interrupt); }

private:
  void Initialize();
  void Finalize();
  static bool IsRunning();

private:
  IXVApplication *application;
};

#endif // _SCREEN_READER_DRIVER_SENSE_READER_H_
