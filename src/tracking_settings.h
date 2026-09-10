// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "camera_fov.h"
#include "config.h"
#include "hpl_math.h"

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/data/tracking_pose.h"

// The one place a Config field becomes a core settings field.
//
// It is a pair of pure functions rather than a block inside Mod::Initialize so
// that the mapping can be read - and tested - without a session, a receiver or
// a game. Every value the player typed reaches the processor through here, so a
// field that is silently not carried across is a setting that does nothing.
namespace SomaHT {

inline cameraunlock::SensitivitySettings SensitivityFrom(const Config& config) {
    cameraunlock::SensitivitySettings sens;
    sens.yaw = config.yawSens;     sens.invert_yaw = config.invertYaw;
    sens.pitch = config.pitchSens; sens.invert_pitch = config.invertPitch;
    sens.roll = config.rollSens;   sens.invert_roll = config.invertRoll;
    return sens;
}

// Smoothing is deliberately absent: position takes the same
// connection-selected value as rotation, and the session recomposes its own two
// values onto the struct, so there is no separate position smoothing setting to
// carry.
inline cameraunlock::PositionSettings PositionFrom(const Config& config) {
    cameraunlock::PositionSettings pos;
    pos.sensitivity_x = config.posSensX;
    pos.sensitivity_y = config.posSensY;
    pos.sensitivity_z = config.posSensZ;
    pos.limit_x = config.limitX;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so the one configured vertical limit is mirrored the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY would widen
    // the upward budget only and downward travel would stay pinned at 0.20m.
    pos.limit_y = config.limitY;
    pos.limit_y_down = config.limitY;
    pos.limit_z = config.limitZ;
    pos.limit_z_back = config.limitZBack;
    // invert_x / invert_y / invert_z are left at false and there is no config
    // key for them. The protocol-to-engine axis conversion happens once, at the
    // engine boundary in math::ApplyHeadTracking, where the negations are
    // pinned as literals, and a second place to flip a position sign is how a
    // player ends up with the 0.40m forward budget on the backward lean and
    // 0.10m to lean in with. The rotation Invert flags above are a different
    // case and stay: they are in AGENTS.md's own settings table, and a mirrored
    // yaw or pitch has no asymmetric limit behind it to get the wrong way round.
    return pos;
}

// This frame's head pose, with the two flags that say which halves of it
// arrived. A half that did not is composed as zero rather than held at its last
// value: the tracker has stopped speaking for that axis and the honest camera
// is the one the game set.
struct HeadPose {
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    bool  rotValid = false;
    float offsetX = 0.0f, offsetY = 0.0f, offsetZ = 0.0f;
    bool  posValid = false;
};

// The clean pose with @p head composed onto it, scaled for whatever field the
// frame is being drawn with.
//
// Yaw, pitch and the lean all translate the picture across the frame, so all
// three carry the zoom factor. ROLL DOES NOT: it rotates the picture about the
// view axis by the same angle at every field of view there is, so scaling it
// would flatten a tilt the player is actively holding and buy nothing. See
// camera_fov.h, which also owns the two bounds the scaling is held inside - the
// angle's tangent wraps at a quarter turn, and the lean's own travel limits
// have to survive the multiply.
//
// A free function rather than a block inside Mod so that the split above can be
// read, and pinned, without a game.
inline math::CameraPose ComposeTrackedPose(const Config& config, const math::CameraPose& clean,
                                           const HeadPose& head, float zoom, bool worldSpaceYaw) {
    using fov::ScalePoseAngle;
    using fov::ScalePoseOffset;
    return math::ApplyHeadTracking(
        clean,
        head.rotValid ? ScalePoseAngle(head.yaw, zoom) : 0.0f,
        head.rotValid ? ScalePoseAngle(head.pitch, zoom) : 0.0f,
        head.rotValid ? head.roll : 0.0f,
        head.posValid ? ScalePoseOffset(head.offsetX, zoom, -config.limitX, config.limitX) : 0.0f,
        head.posValid ? ScalePoseOffset(head.offsetY, zoom, -config.limitY, config.limitY) : 0.0f,
        head.posValid ? ScalePoseOffset(head.offsetZ, zoom, -config.limitZ, config.limitZBack)
                      : 0.0f,
        worldSpaceYaw);
}

}  // namespace SomaHT
