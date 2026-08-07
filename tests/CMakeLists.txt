add_executable(clife_tests
    calculator_tests.cpp
)

target_link_libraries(clife_tests PRIVATE clife::core)
target_compile_features(clife_tests PRIVATE cxx_std_20)
clife_enable_warnings(clife_tests)

set_target_properties(clife_tests PROPERTIES
    FOLDER "CLife/Tests"
)

add_test(NAME clife.calculator.core COMMAND clife_tests)
