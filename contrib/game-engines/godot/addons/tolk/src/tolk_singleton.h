// Tolk GDExtension - the "Tolk" singleton.
#ifndef TOLK_SINGLETON_H
#define TOLK_SINGLETON_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot
{

  class Tolk : public Object
  {
    GDCLASS(Tolk, Object)

  protected:
    static void _bind_methods();

  public:
    Tolk();
    ~Tolk() override;

    void load();
    void unload();
    bool is_loaded() const;

    void try_sapi(bool p_try);
    void prefer_sapi(bool p_prefer);

    String detect_screen_reader() const;
    bool has_speech() const;
    bool has_braille() const;

    bool output(const String &p_text, bool p_interrupt = false);
    bool speak(const String &p_text, bool p_interrupt = false);
    bool braille(const String &p_text);
    bool is_speaking() const;
    bool silence();

  private:
    bool loaded;
  };

} // namespace godot

#endif // TOLK_SINGLETON_H