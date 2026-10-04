// Tolk GDExtension - module registration.
#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "tolk_singleton.h"

using namespace godot;

static Tolk *tolk_singleton = nullptr;

void initialize_tolk_module(ModuleInitializationLevel p_level)
{
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
  {
    return;
  }
  ClassDB::register_class<Tolk>();
  tolk_singleton = memnew(Tolk);
  Engine::get_singleton()->register_singleton("Tolk", tolk_singleton);
}

void uninitialize_tolk_module(ModuleInitializationLevel p_level)
{
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
  {
    return;
  }
  Engine::get_singleton()->unregister_singleton("Tolk");
  memdelete(tolk_singleton);
  tolk_singleton = nullptr;
}

extern "C"
{
  GDExtensionBool GDE_EXPORT tolk_library_init(
      GDExtensionInterfaceGetProcAddress p_get_proc_address,
      const GDExtensionClassLibraryPtr p_library,
      GDExtensionInitialization *r_initialization)
  {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_tolk_module);
    init_obj.register_terminator(uninitialize_tolk_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
  }
}