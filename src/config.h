// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

#include <cstdint>
#include <string>

namespace SomaHT {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file the dev pre-release read, beside kConfigFileName. Imported once while kConfigFileName
// is absent, and never written.
constexpr const char* kLegacyConfigFileName = "HeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "SOMA";

// The vertical field of view a player may ask for, in degrees, when not 0. Below 30 the view is
// unplayable, and at or past a half turn the crosshair projection has no tangent to project
// through.
constexpr float kMinFieldOfView = 30.0f;
constexpr float kMaxFieldOfView = 120.0f;

struct Config {
    uint16_t udp_port = 4242;
    bool enable_on_startup = true;
    // The startup tracking mode. The mode hotkey saves both.
    bool rotation_enabled = true;
    bool position_enabled = true;

    // Picked per connection from the packet source address: a tracker on this machine uses
    // local_smoothing, a remote network device remote_smoothing. Both cover rotation and position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // Travel limits in metres. The sensitivities and inversions stay at identity: the tracker
    // shapes the pose.
    float limit_x = cameraunlock::PositionSettings{}.limit_x;
    float limit_y = cameraunlock::PositionSettings{}.limit_y;
    float limit_y_down = cameraunlock::PositionSettings{}.limit_y_down;
    float limit_z = cameraunlock::PositionSettings{}.limit_z;
    float limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;

    // Yaw about the world up axis, so up stays the horizon however far the mouse has pitched the
    // camera. False yaws about the camera's own up axis. The yaw mode hotkey saves it.
    bool world_space_yaw = true;

    std::string toggle_key_name = "End, Ctrl+Shift+Y";
    std::string cycle_tracking_mode_key_name = "PageUp, Ctrl+Shift+G";
    std::string yaw_mode_key_name = "PageDown, Ctrl+Shift+H";

    // SOMA drives its own camera from a Tobii's gaze - Extended View - and offsets its crosshair to
    // match, both of which fight the head pose. While this is set, the game is told its eye
    // tracker is not tracking for as long as head tracking is enabled, which is the state a player
    // without one is in. Nothing is written to the game or the device, so disabling head tracking
    // hands all of it straight back.
    bool suppress_eye_tracking = true;

    // Vertical field of view in degrees, or 0 to leave the game's own field alone. SOMA exposes no
    // field of view control at all, so this is the only one a player has - see camera_fov.h. The
    // value is the field for normal play; the game's script-driven zooms still scale away from it.
    float field_of_view = 0.0f;
};

// The rows of CameraUnlock.ini. The tracking mode pair and WorldSpaceYaw are Writable: the mode
// and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// HeadTracking.ini as the dev build read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from HeadTracking.ini. The mod passes DefaultsFile::PerUser()
// and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace SomaHT
