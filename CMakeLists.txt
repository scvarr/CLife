cmake_minimum_required(VERSION 3.25)

project(
    CLife
    VERSION 0.1.0
    DESCRIPTION "Portable artificial-life simulation core"
    LANGUAGES CXX
)

option(CLIFE_BUILD_TESTS "Build CLife tests" ON)

set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/CLifeWarnings.cmake)

add_subdirectory(src)
add_subdirectory(apps/headless)

if(CLIFE_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
