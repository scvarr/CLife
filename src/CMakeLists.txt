add_library(clife_core STATIC
    core/cell.cpp
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
