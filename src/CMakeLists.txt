add_library(clife_core STATIC
    core/calculator.cpp
    core/simulation.cpp
)

add_library(clife::core ALIAS clife_core)

target_include_directories(clife_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(clife_core PUBLIC cxx_std_20)
clife_enable_warnings(clife_core)

set_target_properties(clife_core PROPERTIES
    FOLDER "CLife"
)

add_library(clife_world STATIC
    world/definition.cpp
    world/runtime.cpp
)

add_library(clife::world ALIAS clife_world)

target_link_libraries(clife_world PUBLIC clife::core)
target_include_directories(clife_world
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_compile_features(clife_world PUBLIC cxx_std_20)
clife_enable_warnings(clife_world)

set_target_properties(clife_world PROPERTIES
    FOLDER "CLife"
)
