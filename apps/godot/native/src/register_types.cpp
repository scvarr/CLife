#include "clife_demo_runtime.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace clife::godot_adapter {

void initialize_clife_module(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        godot::ClassDB::register_class<CLifeDemoRuntime>();
    }
}

void uninitialize_clife_module(godot::ModuleInitializationLevel) {}

} // namespace clife::godot_adapter

extern "C" {

GDExtensionBool GDE_EXPORT clife_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                               const GDExtensionClassLibraryPtr library,
                                               GDExtensionInitialization* initialization)
{
    godot::GDExtensionBinding::InitObject init_object{get_proc_address, library, initialization};
    init_object.register_initializer(clife::godot_adapter::initialize_clife_module);
    init_object.register_terminator(clife::godot_adapter::uninitialize_clife_module);
    init_object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_object.init();
}
}
