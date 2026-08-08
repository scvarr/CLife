include(FetchContent)

set(GODOTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GODOTCPP_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    godot_cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(godot_cpp)

# godot-cpp uses the static release CRT and iterator ABI for every configuration.
# Match only the opt-in adapter build; the normal CLife presets retain their defaults.
if(MSVC)
    # Keep the adapter warning policy while isolating one warning emitted by pinned generated godot-cpp code.
    target_compile_options(godot-cpp PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/wd4146>")
    foreach(clife_target IN ITEMS clife_core clife_world clife_presets)
        set_property(TARGET ${clife_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
        target_compile_definitions(${clife_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    endforeach()
endif()

add_library(clife_godot SHARED
    src/clife_world_editor.cpp
    src/register_types.cpp
)

target_include_directories(clife_godot PRIVATE src)
target_include_directories(clife_godot SYSTEM PRIVATE
    "${godot_cpp_SOURCE_DIR}/include"
    "${godot_cpp_BINARY_DIR}/gen/include"
    "${godot_cpp_SOURCE_DIR}/gdextension"
)
target_link_libraries(clife_godot PRIVATE clife::presets godot-cpp)
target_compile_features(clife_godot PRIVATE cxx_std_20)
clife_enable_warnings(clife_godot)

set_target_properties(clife_godot PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    FOLDER "CLife/Adapters"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/apps/godot/bin"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/apps/godot/bin"
)
