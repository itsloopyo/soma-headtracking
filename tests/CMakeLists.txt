# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

# The mod's own tests. No game needed: the header-only surfaces run against
# synthetic buffers.
add_executable(soma_headtracking_tests
    test_main.cpp
    aim_projection_tests.cpp
    aim_ray_tests.cpp
    build_profile_tests.cpp
    camera_fov_tests.cpp
    camera_pose_tests.cpp
    hpl_math_tests.cpp
    injection_window_tests.cpp
    reticle_litmus_tests.cpp
    tracking_settings_tests.cpp
    ../src/config.cpp
    ../src/legacy_config/legacy_config.cpp
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
add_library(soma_headtracking_headers_strict OBJECT headers_strict.cpp ../src/config.cpp
    ../src/legacy_config/legacy_config.cpp)
target_include_directories(soma_headtracking_headers_strict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(soma_headtracking_headers_strict PRIVATE cameraunlock)
if(MSVC)
    target_compile_options(soma_headtracking_headers_strict PRIVATE /W4 /WX)
else()
    target_compile_options(soma_headtracking_headers_strict PRIVATE -Wall -Wextra -Werror)
endif()

# The config differential test. The oracle is the dev pre-release's reader with the core sources
# it compiled, copied verbatim into config_differential/oracle, and that build's hotkey
# registration over the chord guards of its pin. It is two libraries, each renaming the namespaces
# its own way, so neither collides with the other or with the current core.
set(SOMA_DIFF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config_differential)
set(SOMA_ORACLE_DIR ${SOMA_DIFF_DIR}/oracle)
add_library(soma_config_oracle STATIC
    ${SOMA_ORACLE_DIR}/src/config.cpp
    ${SOMA_ORACLE_DIR}/core/src/config/ini_reader.cpp
    ${SOMA_ORACLE_DIR}/core/src/config/value_guards.cpp
    ${SOMA_ORACLE_DIR}/core/src/logging/file_log.cpp
    ${SOMA_DIFF_DIR}/oracle_config_adapter.cpp
)
target_include_directories(soma_config_oracle PRIVATE
    ${SOMA_ORACLE_DIR}/src
    ${SOMA_ORACLE_DIR}/core/include
    ${SOMA_DIFF_DIR})
target_compile_definitions(soma_config_oracle PRIVATE
    cameraunlock=soma_oracle_core
    SomaHT=soma_oracle)

add_library(soma_hotkeys_oracle STATIC
    ${SOMA_DIFF_DIR}/oracle_hotkeys_adapter.cpp
)
target_include_directories(soma_hotkeys_oracle PRIVATE
    ${SOMA_DIFF_DIR}/oracle_fake
    ${SOMA_ORACLE_DIR}/core/include
    ${SOMA_DIFF_DIR})
target_compile_definitions(soma_hotkeys_oracle PRIVATE
    cameraunlock=soma_oracle_hk_core)

add_executable(config_differential_tests
    ${SOMA_DIFF_DIR}/differential_tests.cpp
    ../src/config.cpp
    ../src/legacy_config/legacy_config.cpp
)
target_include_directories(config_differential_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${SOMA_DIFF_DIR})
target_compile_definitions(config_differential_tests PRIVATE
    SOMA_DIFFERENTIAL_DATA="${SOMA_DIFF_DIR}/data"
    SOMA_COMMITTED_CONFIG="${CMAKE_SOURCE_DIR}/config/CameraUnlock.ini")
target_link_libraries(config_differential_tests PRIVATE
    cameraunlock soma_config_oracle soma_hotkeys_oracle)
if(MSVC)
    target_compile_options(config_differential_tests PRIVATE /W4 /permissive-)
endif()
add_test(NAME config_differential_tests COMMAND config_differential_tests)

# Core's canonical config lint over the committed file and every file the differential test
# migrated, which it writes beside itself under migrated.
add_test(NAME config_lint COMMAND node
    ${SOMA_DIFF_DIR}/lint-migrated.mjs
    $<TARGET_FILE_DIR:config_differential_tests>/migrated)
set_tests_properties(config_lint PROPERTIES DEPENDS config_differential_tests)
set_tests_properties(config_differential_tests config_lint PROPERTIES LABELS differential)

# The committed config against the table, a first start, the hotkeys' saves and the legacy file's
# ANSI path. `--render-config <path>` makes it write the committed file instead
# (pixi run render-config).
add_executable(config_tests
    config_tests.cpp
    ../src/config.cpp
    ../src/legacy_config/legacy_config.cpp
)
target_include_directories(config_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(config_tests PRIVATE
    SOMA_COMMITTED_CONFIG="${CMAKE_SOURCE_DIR}/config/CameraUnlock.ini")
target_link_libraries(config_tests PRIVATE cameraunlock)
if(MSVC)
    target_compile_options(config_tests PRIVATE /W4 /permissive-)
endif()
add_test(NAME config_tests COMMAND config_tests)
