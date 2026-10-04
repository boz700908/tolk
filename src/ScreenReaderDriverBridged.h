/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverBridged.h
 *  Description:    Driver proxy for a backend that has no module for the
 *                  current architecture. Calls are forwarded to the embedded
 *                  helper process of the matching architecture.
 *  License:        LGPLv3
 */
#ifndef _SCREEN_READER_DRIVER_BRIDGED_H_
#define _SCREEN_READER_DRIVER_BRIDGED_H_

#include "ScreenReaderDriver.h"
#include "TolkBridge.h"

class ScreenReaderDriverBridged : public ScreenReaderDriver {
public:
  ScreenReaderDriverBridged(const wchar_t *name, bool speech, bool braille, TolkBridgeBackend backend) :
    ScreenReaderDriver(name, speech, braille),
    brailleSupported(braille),
    client(backend)
  {}
public:
  bool Speak(const wchar_t *str, bool interrupt) override { return client.Speak(str, interrupt); }
  bool Braille(const wchar_t *str) override { return brailleSupported ? client.Braille(str) : false; }
  bool IsSpeaking() override { return client.IsSpeaking(); }
  bool Silence() override { return client.Silence(); }
  bool IsActive() override { return client.IsActive(); }
  bool Output(const wchar_t *str, bool interrupt) override {
    if (!brailleSupported) return Speak(str, interrupt);
    return ScreenReaderDriver::Output(str, interrupt);
  }
private:
  const bool brailleSupported;
  TolkBridgeClient client;
};

#endif // _SCREEN_READER_DRIVER_BRIDGED_H_
