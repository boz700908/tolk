// Tolk GDExtension - the "Tolk" singleton.
#include "tolk_singleton.h"

#include <string>
#include <vector>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/memory.hpp>

#include <windows.h>

#include "Tolk.h"

using namespace godot;

namespace
{
  // Tolk speaks UTF-16; Godot strings are UTF-8. Convert through Win32 so the
  // mapping is correct for names that are not plain ASCII.
  String FromWide(const wchar_t *p_text)
  {
    if (p_text == nullptr)
    {
      return String();
    }
    int length = WideCharToMultiByte(CP_UTF8, 0, p_text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
    {
      return String();
    }
    std::vector<char> buffer(static_cast<size_t>(length));
    WideCharToMultiByte(CP_UTF8, 0, p_text, -1, buffer.data(), length, nullptr, nullptr);
    return String::utf8(buffer.data(), length - 1);
  }

  std::wstring ToWide(const String &p_text)
  {
    CharString utf8 = p_text.utf8();
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8.get_data(), -1, nullptr, 0);
    if (length <= 1)
    {
      return std::wstring();
    }
    std::wstring buffer(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.get_data(), -1, buffer.data(), length);
    buffer.resize(static_cast<size_t>(length - 1));
    return buffer;
  }
} // namespace

Tolk::Tolk() : loaded(false) {}

Tolk::~Tolk()
{
  if (loaded)
  {
    Tolk_Unload();
    loaded = false;
  }
}

void Tolk::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("load"), &Tolk::load);
  ClassDB::bind_method(D_METHOD("unload"), &Tolk::unload);
  ClassDB::bind_method(D_METHOD("is_loaded"), &Tolk::is_loaded);
  ClassDB::bind_method(D_METHOD("try_sapi", "enabled"), &Tolk::try_sapi);
  ClassDB::bind_method(D_METHOD("prefer_sapi", "preferred"), &Tolk::prefer_sapi);
  ClassDB::bind_method(D_METHOD("detect_screen_reader"), &Tolk::detect_screen_reader);
  ClassDB::bind_method(D_METHOD("has_speech"), &Tolk::has_speech);
  ClassDB::bind_method(D_METHOD("has_braille"), &Tolk::has_braille);
  ClassDB::bind_method(D_METHOD("output", "text", "interrupt"), &Tolk::output, DEFVAL(false));
  ClassDB::bind_method(D_METHOD("speak", "text", "interrupt"), &Tolk::speak, DEFVAL(false));
  ClassDB::bind_method(D_METHOD("braille", "text"), &Tolk::braille);
  ClassDB::bind_method(D_METHOD("is_speaking"), &Tolk::is_speaking);
  ClassDB::bind_method(D_METHOD("silence"), &Tolk::silence);
}

void Tolk::load()
{
  Tolk_Load();
  loaded = Tolk_IsLoaded();
}

void Tolk::unload()
{
  Tolk_Unload();
  loaded = false;
}

bool Tolk::is_loaded() const
{
  return Tolk_IsLoaded();
}

void Tolk::try_sapi(bool p_try)
{
  Tolk_TrySAPI(p_try);
}

void Tolk::prefer_sapi(bool p_prefer)
{
  Tolk_PreferSAPI(p_prefer);
}

String Tolk::detect_screen_reader() const
{
  return FromWide(Tolk_DetectScreenReader());
}

bool Tolk::has_speech() const
{
  return Tolk_HasSpeech();
}

bool Tolk::has_braille() const
{
  return Tolk_HasBraille();
}

bool Tolk::output(const String &p_text, bool p_interrupt)
{
  return Tolk_Output(ToWide(p_text).c_str(), p_interrupt);
}

bool Tolk::speak(const String &p_text, bool p_interrupt)
{
  return Tolk_Speak(ToWide(p_text).c_str(), p_interrupt);
}

bool Tolk::braille(const String &p_text)
{
  return Tolk_Braille(ToWide(p_text).c_str());
}

bool Tolk::is_speaking() const
{
  return Tolk_IsSpeaking();
}

bool Tolk::silence()
{
  return Tolk_Silence();
}