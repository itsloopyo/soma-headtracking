// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for tracking_settings.h - the one place a Config field
// becomes a core settings field.
//
// A field that is not carried across is a setting the INI documents, the reader
// validates, the log reports, and nothing acts on. Nothing else in the tree
// would notice: the mod builds, the game runs, the value is simply never asked
// for. Every field of both structs is checked here for that reason, with a
// distinct value in each so a transposed pair fails rather than passing on two
// defaults that happen to agree.

#include "tracking_settings.h"
#include "test_support.h"

#include <cmath>
#include <iostream>

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;

// A config with nothing left at its default, so every assertion below is about
// the value that was set rather than about a shared 1.0.
SomaHT::Config DistinctConfig() {
    SomaHT::Config config;
    config.yawSens = 1.5f;
    config.pitchSens = 0.5f;
    config.rollSens = 2.25f;
    config.invertYaw = true;
    config.invertPitch = false;
    config.invertRoll = true;

    config.posSensX = 1.25f;
    config.posSensY = 0.75f;
    config.posSensZ = 2.5f;
    config.limitX = 0.11f;
    config.limitY = 0.13f;
    config.limitZ = 0.17f;
    config.limitZBack = 0.19f;
    return config;
}

}  // namespace

int RunTrackingSettingsTests() {
    std::cout << "\nTracking settings tests\n";
    Report r;

    const SomaHT::Config config = DistinctConfig();

    {
        const cameraunlock::SensitivitySettings sens = SomaHT::SensitivityFrom(config);
        r.Check(NearEqual(sens.yaw, 1.5f) && NearEqual(sens.pitch, 0.5f) &&
                    NearEqual(sens.roll, 2.25f),
                "each rotation sensitivity reaches its own axis");
        r.Check(sens.invert_yaw && !sens.invert_pitch && sens.invert_roll,
                "each rotation inversion reaches its own axis");
    }

    {
        const cameraunlock::PositionSettings pos = SomaHT::PositionFrom(config);
        r.Check(NearEqual(pos.sensitivity_x, 1.25f) && NearEqual(pos.sensitivity_y, 0.75f) &&
                    NearEqual(pos.sensitivity_z, 2.5f),
                "each position sensitivity reaches its own axis");
        r.Check(NearEqual(pos.limit_x, 0.11f) && NearEqual(pos.limit_z, 0.17f) &&
                    NearEqual(pos.limit_z_back, 0.19f),
                "the x and both z limits reach their own fields");
        // Doctrine, not a mapping: there is no [Position] InvertX/Y/Z key and
        // there must not be one. The axis conversion happens once, at the
        // engine boundary, and a second user-reachable place to flip a sign
        // puts the asymmetric z limits the wrong way round.
        r.Check(!pos.invert_x && !pos.invert_y && !pos.invert_z,
                "each position inversion reaches its own axis");

        // The clamp is [-limit_y_down, +limit_y]. The mod carries one vertical
        // limit, so it has to land in both: left at its own default, raising
        // LimitY would widen the upward budget alone and downward travel would
        // stay pinned at core's 0.20m.
        r.Check(NearEqual(pos.limit_y, 0.13f) && NearEqual(pos.limit_y_down, 0.13f),
                "the one configured vertical limit is mirrored into limit_y_down");
    }

    {
        // Smoothing is deliberately not carried: the session recomposes its own
        // two connection-selected values onto the struct, and a value copied in
        // here would be one the session then overwrites - or worse, would not.
        const cameraunlock::PositionSettings pos = SomaHT::PositionFrom(config);
        const cameraunlock::PositionSettings defaults;
        r.Check(NearEqual(pos.local_smoothing, defaults.local_smoothing) &&
                    NearEqual(pos.remote_smoothing, defaults.remote_smoothing),
                "the mapping leaves both smoothing values for the session to set");
    }

    {
        // A default config maps to core's own defaults, which is what makes
        // "no HeadTracking.ini" and "an INI with nothing in it" the same game.
        const SomaHT::Config defaults;
        const cameraunlock::PositionSettings pos = SomaHT::PositionFrom(defaults);
        const cameraunlock::PositionSettings core;
        r.Check(NearEqual(pos.limit_x, core.limit_x) && NearEqual(pos.limit_y, core.limit_y) &&
                    NearEqual(pos.limit_z, core.limit_z) &&
                    NearEqual(pos.limit_z_back, core.limit_z_back),
                "an unedited config maps to core's own position limits");
    }

    // The zoom split. AGENTS.md states it as a hard rule and camera_fov.h tests
    // ScaleAngleForZoom on its own, but which axes are handed to it is decided
    // here - and every wrong answer still compiles, still renders, and reads in
    // game as the mod's sensitivity misbehaving rather than as a bug.
    {
        SomaHT::Config c;
        SomaHT::HeadPose head;
        head.yaw = 12.0f;
        head.pitch = -8.0f;
        head.roll = 6.0f;
        head.rotValid = true;
        head.offsetX = 0.05f;
        head.offsetY = 0.03f;
        head.offsetZ = 0.04f;
        head.posValid = true;

        const SomaHT::math::CameraPose clean;
        const SomaHT::math::CameraPose rest =
            SomaHT::ComposeTrackedPose(c, clean, head, 1.0f, true);
        const SomaHT::math::CameraPose zoomed =
            SomaHT::ComposeTrackedPose(c, clean, head, 0.5f, true);

        // Yaw and pitch translate the picture across the frame, so a zoom that
        // magnifies the frame has to shrink them BY THE FACTOR, not merely
        // downwards - a factor wrong by a constant renders exactly like one
        // that is right, and a direction-only check passes a pose scaled by the
        // square of it.
        //
        // Pinned on the TANGENT, which is where the property actually lives:
        // the tangent of the view angle is the picture's displacement per unit
        // depth, so "the head moves the picture as far as it would have at the
        // un-zoomed field" is exactly tan(scaled) == zoom * tan(unscaled). The
        // angles themselves do not scale linearly and asserting that they do is
        // wrong by 0.07 degrees at 12. tan is odd, so the engine's negated yaw
        // cancels out of both sides.
        r.Check(NearEqual(std::tan(zoomed.yaw), 0.5f * std::tan(rest.yaw), 1e-5f) &&
                    NearEqual(std::tan(zoomed.pitch), 0.5f * std::tan(rest.pitch), 1e-5f),
                "a zoom scales yaw and pitch by the zoom factor, measured on screen");

        // Roll does NOT. It rotates the picture about the view axis by the same
        // angle at every field of view there is, so scaling it flattens a tilt
        // the player is holding and buys nothing.
        r.Check(zoomed.roll == rest.roll, "and leaves roll exactly alone");

        // The lean translates the picture linearly, so it scales linearly, and
        // by the same factor on all three axes.
        bool leanScaled = true;
        for (int i = 0; i < 3; ++i) {
            leanScaled = leanScaled && NearEqual(zoomed.pos[i], rest.pos[i] * 0.5f, 1e-5f);
        }
        r.Check(leanScaled, "and scales the lean by the same factor on all three axes");

        // A half that never arrived is composed as zero, not held.
        SomaHT::HeadPose rotOnly = head;
        rotOnly.posValid = false;
        const SomaHT::math::CameraPose noLean =
            SomaHT::ComposeTrackedPose(c, clean, rotOnly, 1.0f, true);
        r.Check(NearEqual(noLean.pos[0], clean.pos[0]) &&
                    NearEqual(noLean.pos[1], clean.pos[1]) &&
                    NearEqual(noLean.pos[2], clean.pos[2]),
                "a position half that did not arrive leaves the camera where the game put it");
    }

    return r.Failures();
}
