// Tolk GDExtension - module registration.
#ifndef TOLK_REGISTER_TYPES_H
#define TOLK_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_tolk_module(ModuleInitializationLevel p_level);
void uninitialize_tolk_module(ModuleInitializationLevel p_level);

#endif // TOLK_REGISTER_TYPES_H