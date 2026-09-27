// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace SomaHT {

struct Config {
    int   udpPort = 4242;

    float yawSens = 1.0f, pitchSens = 1.0f, rollSens = 1.0f;
    bool  invertYaw = false, invertPitch = false, invertRoll = false;
    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses localSmoothing, a remote network
    // device uses remoteSmoothing. Both cover rotation and position.
    float localSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    bool  positionEnabled = true;
    float posSensX = 1.0f, posSensY = 1.0f, posSensZ = 1.0f;
    float limitX     = cameraunlock::PositionSettings{}.limit_x;
    float limitY     = cameraunlock::PositionSettings{}.limit_y;
    float limitZ     = cameraunlock::PositionSettings{}.limit_z;
    float limitZBack = cameraunlock::PositionSettings{}.limit_z_back;

    bool  crosshairCompensation = true;

    // SOMA drives its own camera from a Tobii's gaze - Extended View - and
    // offsets its crosshair to match, both of which fight the head pose. While
    // this is set, the game is told its eye tracker is not tracking for as long
    // as head tracking is enabled, which is the state a player without one is
    // in: no Extended View, and no gaze crosshair, flashlight control, reactive
    // environment or reactive AI either. Nothing is written to the game or the
    // device, so disabling head tracking hands all of it straight back.
    bool  suppressEyeTracking = true;

    // Vertical field of view in degrees, or 0 to leave the game's own field
    // alone. SOMA exposes no field of view control at all, so this is the only
    // one a player has - see camera_fov.h. The value is the field for normal
    // play; the game's script-driven zooms still scale away from it.
    float fieldOfView = 0.0f;

    // Yaw about the world up axis, so up stays the horizon however far the
    // mouse has pitched the camera. False yaws about the camera's own up axis
    // instead, which leans the view once the camera is pitched away from level.
    bool  worldSpaceYaw = true;

    int   toggleKey       = 0x23;  // End
    int   trackingModeKey = 0x21;  // Page Up
    int   yawModeKey      = 0x22;  // Page Down

    bool  autoEnable = true;

    // Loads from an INI file. Returns false if the file does not exist (caller
    // then keeps these defaults).
    bool Load(const char* path);
};

}  // namespace SomaHT
