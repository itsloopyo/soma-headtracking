# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

# The mod's own tests. No game needed: the header-only surfaces run against
# synthetic buffers, and config.cpp against INI files written to a temp dir.
add_executable(soma_headtracking_tests
    test_main.cpp
    aim_projection_tests.cpp
    aim_ray_tests.cpp
    build_profile_tests.cpp
    camera_fov_tests.cpp
    camera_pose_tests.cpp
    config_tests.cpp
    hpl_math_tests.cpp
    injection_window_tests.cpp
    reticle_litmus_tests.cpp
    tracking_settings_tests.cpp
    ../src/config.cpp
    # For kKnownProfiles. build_profile_tests.cpp reads the registry's shape;
    # MatchRunningBuild itself needs a running SOMA and is not called.
    ../src/steam_offsets.cpp
)

target_include_directories(soma_headtracking_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(soma_headtracking_tests PRIVATE cameraunlock)

add_test(NAME soma_headtracking_tests COMMAND soma_headtracking_tests)

# Compile-only, and STRICT. Every header the mod ships in one TU at the warning
# level the release build uses, plus config.cpp so the mod's own translation
# unit gets the same treatment. /WX is carried here rather than on the mod
# target, so a future compiler's new warning cannot fail a user's build of the
# .asi.
add_library(soma_headtracking_headers_strict OBJECT headers_strict.cpp ../src/config.cpp)
target_include_directories(soma_headtracking_headers_strict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(soma_headtracking_headers_strict PRIVATE cameraunlock)
if(MSVC)
    target_compile_options(soma_headtracking_headers_strict PRIVATE /W4 /WX)
else()
    target_compile_options(soma_headtracking_headers_strict PRIVATE -Wall -Wextra -Werror)
endif()
