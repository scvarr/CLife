add_executable(clife_headless
    calculator_main.cpp
)

target_link_libraries(clife_headless PRIVATE clife::presets)
target_compile_features(clife_headless PRIVATE cxx_std_20)
clife_enable_warnings(clife_headless)

set_target_properties(clife_headless PROPERTIES
    FOLDER "CLife/Apps"
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
