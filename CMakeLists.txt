cmake_minimum_required(VERSION 3.16)

project(m3sp
    VERSION 0.1.0
    DESCRIPTION "Multi-period, Multi-rate and Multi-channel with variable Bandwidth Scheduling Problem"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The old src/Makefile always built with -O2; keep that as the default so a bare
# `cmake ..` does not silently produce an unoptimised (and much slower) binary.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(M3SP_BUILD_TESTS     "Build the unit tests"                         ON)
option(M3SP_ENABLE_WARNINGS "Compile the project with a strict warning set" OFF)
option(M3SP_ENABLE_PROFILING "Also build m3sp_prof, instrumented for gprof" OFF)

find_package(OpenMP REQUIRED)

add_subdirectory(extern)

# ----------------------------------------------------------------------------
# Core library: everything except the entry point, so the tests can link it.
# ----------------------------------------------------------------------------
set(M3SP_CORE_SOURCES
    src/common.cpp
    src/decoder.cpp
    src/decoder/base.cpp
    src/decoder/mic.cpp
    src/decoder/dp.cpp
    src/decoder/gcsid.cpp
    src/decoder/gcsid_plus.cpp
    src/decoder/gcsid_greedy.cpp
    src/decoder/local_search.cpp)

add_library(m3sp_core ${M3SP_CORE_SOURCES})
add_library(m3sp::core ALIAS m3sp_core)

target_include_directories(m3sp_core
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# mtrand is PUBLIC because include/m3sp/common.h exposes `extern MTRand rng;`.
target_link_libraries(m3sp_core PUBLIC mtrand OpenMP::OpenMP_CXX)
m3sp_set_warnings(m3sp_core)

# ----------------------------------------------------------------------------
# Application
# ----------------------------------------------------------------------------
add_executable(m3sp src/main.cpp)
target_link_libraries(m3sp PRIVATE m3sp_core brkga)
m3sp_set_warnings(m3sp)

# main.cpp resolves "../instances" and "../output" against the working directory,
# so keep the binary at the top of the build tree: `cd build && ./m3sp mic`
# behaves exactly like the old `cd src && ./code 0`.
set_target_properties(m3sp PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# ----------------------------------------------------------------------------
# Profiling build (replaces the `code_prof` target of the old src/Makefile)
# ----------------------------------------------------------------------------
if(M3SP_ENABLE_PROFILING)
    add_executable(m3sp_prof src/main.cpp ${M3SP_CORE_SOURCES})
    target_include_directories(m3sp_prof
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(m3sp_prof PRIVATE mtrand brkga OpenMP::OpenMP_CXX)
    target_compile_options(m3sp_prof PRIVATE -pg -O3)
    target_link_options(m3sp_prof PRIVATE -pg)
    set_target_properties(m3sp_prof PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    # Run ./m3sp_prof <type> first; it leaves gmon.out in the build tree.
    add_custom_target(profile-report
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/profile"
        COMMAND gprof "${CMAKE_BINARY_DIR}/m3sp_prof" "${CMAKE_BINARY_DIR}/gmon.out"
                > "${PROJECT_SOURCE_DIR}/profile/gprof.txt"
        COMMAND python3 plot_profile.py profile/gprof.txt
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Generating profile/gprof.txt and the profile PNGs"
        VERBATIM)
endif()

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------
if(M3SP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
