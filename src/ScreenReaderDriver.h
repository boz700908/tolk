/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriver.h
 *  Description:    Generic screen reader driver.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
#ifndef _SCREEN_READER_DRIVER_H_
#define _SCREEN_READER_DRIVER_H_
class ScreenReaderDriver {
public:
  // Performance: IsActive() result cache timeout (milliseconds)
  static const unsigned long long CACHE_TIMEOUT_MS = 100;
protected:
  ScreenReaderDriver(const wchar_t *screenReaderName, bool speech, bool braille) :
    name(screenReaderName),
    hasSpeech(speech),
    hasBraille(braille),
    cachedIsActive(false),
    lastIsActiveTime(0)
    {}
  ScreenReaderDriver(const ScreenReaderDriver&) = delete;
  ScreenReaderDriver& operator=(const ScreenReaderDriver&) = delete;
protected:
  // Performance: IsActive() result cache (CACHE_TIMEOUT_MS)
  mutable bool cachedIsActive;
  mutable unsigned long long lastIsActiveTime;
public:
  virtual ~ScreenReaderDriver() {}
public:
  virtual bool Speak(const wchar_t *str, bool interrupt) = 0;
  virtual bool Braille(const wchar_t *str) = 0;
  virtual bool IsSpeaking() = 0;
  virtual bool Silence() = 0;
  virtual bool IsActive() = 0;
  virtual bool Output(const wchar_t *str, bool interrupt) {
    const bool speak = Speak(str, interrupt);
    const bool braille = Braille(str);
    return (speak || braille);
  }
public:
  const wchar_t * GetName() const { return name; }
  bool HasSpeech() const { return hasSpeech; }
  bool HasBraille() const { return hasBraille; }
private:
  const wchar_t *name;
  const bool hasSpeech;
  const bool hasBraille;
};
#endif // _SCREEN_READER_DRIVER_H_
