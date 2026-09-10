# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

cmake_minimum_required(VERSION 3.20)
project(SomaHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOMA_BUILD_TESTS "Build the mod's test executable" ON)

# SOMA (HPL3 engine) is a 64-bit OpenGL game. The .asi is loaded by Ultimate
# ASI Loader dropped in as version.dll, which SDL2.dll imports and Windows
# resolves out of the exe directory. Configure with -A x64.

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    # Static runtime so the .asi has no MSVC redist dependency.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- cameraunlock-core (submodule) ---
# Brings its own vendored MinHook in through CAMERAUNLOCK_BUILD_HOOKS.
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/main.cpp
    src/mod.cpp
    src/config.cpp
    src/camera_hook.cpp
    src/crosshair_hook.cpp
    src/eye_tracking.cpp
    src/gameplay_gate.cpp
    src/gui_input_hook.cpp
    src/steam_offsets.cpp
    src/window_centering.cpp
)

add_library(SomaHeadTracking SHARED ${MOD_SOURCES})

target_include_directories(SomaHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SomaHeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
)

if(WIN32)
    target_link_libraries(SomaHeadTracking PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(SomaHeadTracking PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()

set_target_properties(SomaHeadTracking PROPERTIES
    PREFIX ""
    OUTPUT_NAME "SomaHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

if(SOMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "SOMA Head Tracking:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Output:       bin/<config>/SomaHeadTracking.asi")
message(STATUS "")
