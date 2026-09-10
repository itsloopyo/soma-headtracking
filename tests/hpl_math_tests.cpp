// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for hpl_math.h.
//
// Two surfaces, and both fail silently. The first is what the mod reads out of
// the engine's own view matrix: which rows are the camera basis, which way
// forward points, and where the eye is. Get any of them wrong and the crosshair
// projection is fed a basis that does not match the frame, which is exactly the
// failure the projection is written to avoid.
//
// The second is ApplyHeadTracking, where the tracker's axis conventions meet the
// engine's. Those signs are INHERITED from the HPL2 ancestor rather than derived,
// and nothing in the type system defends them: a flipped one still compiles,
// still renders, and surfaces as the view leaning the wrong way in a bug report.
// They are pinned here as literals so that changing one has to be deliberate.

#include "hpl_math.h"
#include "test_support.h"

#include <cmath>

namespace {

using SomaHT::math::ApplyHeadTracking;
using SomaHT::math::CameraForward;
using SomaHT::math::CameraPose;
using SomaHT::math::ComposeCameraRotation;
using SomaHT::math::DecomposeCameraRotation;
using SomaHT::math::DecomposeView;
using SomaHT::math::Rot3;
using SomaHT::math::RotationBasis;
using SomaHT::math::ViewBasis;
using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;

constexpr float kDegToRad = SomaHT::math::kDegToRad;
constexpr float kQuarterTurn = 1.5707963f;

bool Vec3NearEqual(const float a[3], float x, float y, float z) {
    return NearEqual(a[0], x) && NearEqual(a[1], y) && NearEqual(a[2], z);
}

// The view matrix HPL3 builds from a camera orientation and position: the
// rotation part is the transpose, with the eye folded into the translation
// column. Row-major storage, to match cCamera's own.
void MakeViewMatrix(const Rot3& r, const float eye[3], float view[16]) {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) view[row * 4 + col] = r.m[col * 3 + row];
        view[row * 4 + 3] = -(r.m[0 * 3 + row] * eye[0] + r.m[1 * 3 + row] * eye[1] +
                              r.m[2 * 3 + row] * eye[2]);
    }
    view[12] = 0.0f;
    view[13] = 0.0f;
    view[14] = 0.0f;
    view[15] = 1.0f;
}

}  // namespace

int RunHplMathTests() {
    std::cout << "\nHPL view basis tests\n";
    Report r;

    // The identity view matrix is a camera at the origin looking down -z: the
    // engine stores the backwards axis, which is why GetForward negates it.
    {
        const float view[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        ViewBasis b{};
        DecomposeView(view, b);
        r.Check(Vec3NearEqual(b.right, 1.0f, 0.0f, 0.0f), "identity view: right is +x");
        r.Check(Vec3NearEqual(b.up, 0.0f, 1.0f, 0.0f), "identity view: up is +y");
        r.Check(Vec3NearEqual(b.fwd, 0.0f, 0.0f, -1.0f), "identity view: forward is -z");
        r.Check(Vec3NearEqual(b.eye, 0.0f, 0.0f, 0.0f), "identity view: eye is the origin");
    }

    // A camera at (3,4,5) yawed a quarter turn to look down +x. Rows 0/1 are the
    // right and up axes, row 2 is the negated forward, and column 3 is -R*eye,
    // so the eye has to come back through the transpose.
    {
        const float view[16] = {
             0.0f, 0.0f, -1.0f,  5.0f,
             0.0f, 1.0f,  0.0f, -4.0f,
            -1.0f, 0.0f,  0.0f,  3.0f,
             0.0f, 0.0f,  0.0f,  1.0f,
        };
        ViewBasis b{};
        DecomposeView(view, b);
        r.Check(Vec3NearEqual(b.right, 0.0f, 0.0f, -1.0f), "yawed view: right axis");
        r.Check(Vec3NearEqual(b.up, 0.0f, 1.0f, 0.0f), "yawed view: up axis");
        r.Check(Vec3NearEqual(b.fwd, 1.0f, 0.0f, 0.0f), "yawed view: forward axis");
        r.Check(Vec3NearEqual(b.eye, 3.0f, 4.0f, 5.0f), "yawed view: eye recovered through the transpose");
    }

    // Translation alone: the eye is the negated column, not the column.
    {
        const float view[16] = {
            1.0f, 0.0f, 0.0f, -2.0f,
            0.0f, 1.0f, 0.0f, -7.0f,
            0.0f, 0.0f, 1.0f,  1.5f,
            0.0f, 0.0f, 0.0f,  1.0f,
        };
        ViewBasis b{};
        DecomposeView(view, b);
        r.Check(Vec3NearEqual(b.eye, 2.0f, 7.0f, -1.5f), "translated view: eye is the negated translation");
    }

    // The two halves have to agree: the basis pulled back out of the matrix the
    // engine built must be the basis the composed rotation describes. This is
    // the join the crosshair projection stands on, and nothing else checks it.
    {
        const float eye[3] = {1.5f, -2.0f, 3.25f};
        const Rot3 rot = ComposeCameraRotation(0.4f, -0.25f, 0.15f);

        float right[3], up[3], fwd[3];
        RotationBasis(rot, right, up, fwd);

        float view[16];
        MakeViewMatrix(rot, eye, view);
        ViewBasis b{};
        DecomposeView(view, b);

        r.Check(Vec3NearEqual(b.right, right[0], right[1], right[2]),
                "the decomposed right axis is the composed one");
        r.Check(Vec3NearEqual(b.up, up[0], up[1], up[2]),
                "the decomposed up axis is the composed one");
        r.Check(Vec3NearEqual(b.fwd, fwd[0], fwd[1], fwd[2]),
                "the decomposed forward axis is the composed one");
        r.Check(Vec3NearEqual(b.eye, eye[0], eye[1], eye[2]),
                "the decomposed eye is the camera position");
    }

    // Composition and its inverse. The camera-local yaw mode is the only caller
    // of the decomposition, and a wrong branch there reads as the view rolling
    // rather than turning.
    {
        float right[3], up[3], fwd[3];
        RotationBasis(ComposeCameraRotation(0.0f, 0.0f, 0.0f), right, up, fwd);
        r.Check(Vec3NearEqual(right, 1.0f, 0.0f, 0.0f), "an unrotated camera has right at +x");
        r.Check(Vec3NearEqual(up, 0.0f, 1.0f, 0.0f), "an unrotated camera has up at +y");
        r.Check(Vec3NearEqual(fwd, 0.0f, 0.0f, -1.0f), "an unrotated camera looks down -z");

        float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
        DecomposeCameraRotation(ComposeCameraRotation(0.7f, -0.3f, 0.2f), yaw, pitch, roll);
        r.Check(NearEqual(yaw, 0.7f) && NearEqual(pitch, -0.3f) && NearEqual(roll, 0.2f),
                "decomposition inverts composition");
    }

    // CameraForward skips the rotation and drops roll, on the claim that roll is
    // applied about the very axis being asked for. Nothing else would catch it
    // diverging: the crosshair's no-hit fallback is the only caller, and a wrong
    // forward there reads as the crosshair sitting slightly off a wall it was
    // never told the distance to.
    {
        const float yaw = 0.7f, pitch = -0.3f;
        float direct[3];
        CameraForward(yaw, pitch, direct);

        float right[3], up[3], fwd[3];
        RotationBasis(ComposeCameraRotation(yaw, pitch, 0.45f), right, up, fwd);
        r.Check(Vec3NearEqual(direct, fwd[0], fwd[1], fwd[2]),
                "CameraForward is the composed forward axis, and roll does not move it");
    }

    // Straight up, where the yaw and roll axes coincide: the whole rotation is
    // attributed to yaw, which leaves the view where it was rather than rolling
    // it.
    {
        float yaw = -99.0f, pitch = -99.0f, roll = -99.0f;
        DecomposeCameraRotation(ComposeCameraRotation(0.5f, kQuarterTurn, 0.0f), yaw, pitch, roll);
        r.Check(NearEqual(pitch, kQuarterTurn, 1e-3f), "a quarter-turn pitch decomposes as itself");
        r.Check(NearEqual(roll, 0.0f), "a degenerate pitch puts no rotation into roll");
        // The half that matters, and the half a `yaw = 0` in the degenerate
        // branch would pass without: the heading survives. Losing it snaps the
        // camera-local view to north the moment the player looks straight up.
        r.Check(NearEqual(yaw, 0.5f, 1e-3f), "a degenerate pitch keeps the heading in yaw");
    }

    // And the same looking straight DOWN, which is the other arm of that branch
    // and has its own sign.
    {
        float yaw = -99.0f, pitch = -99.0f, roll = -99.0f;
        DecomposeCameraRotation(ComposeCameraRotation(0.5f, -kQuarterTurn, 0.0f), yaw, pitch, roll);
        r.Check(NearEqual(pitch, -kQuarterTurn, 1e-3f),
                "a downward quarter-turn pitch decomposes as itself");
        r.Check(NearEqual(roll, 0.0f), "a downward degenerate pitch puts no rotation into roll");
        r.Check(NearEqual(yaw, 0.5f, 1e-3f),
                "a downward degenerate pitch keeps the same heading, not its mirror");
    }

    return r.Failures();
}

int RunHeadTrackingTests() {
    std::cout << "\nHead tracking application tests\n";
    Report r;

    const CameraPose clean;  // level, at the origin

    // A centred head must leave the camera exactly where the game put it, in
    // both yaw modes: this is every frame between packets, and every frame with
    // tracking toggled off.
    {
        const CameraPose world = ApplyHeadTracking(clean, 0, 0, 0, 0, 0, 0, true);
        const CameraPose local = ApplyHeadTracking(clean, 0, 0, 0, 0, 0, 0, false);
        r.Check(NearEqual(world.yaw, 0.0f) && NearEqual(world.pitch, 0.0f) &&
                    NearEqual(world.roll, 0.0f) && Vec3NearEqual(world.pos, 0.0f, 0.0f, 0.0f),
                "a centred head leaves the world-yaw pose untouched");
        r.Check(NearEqual(local.yaw, 0.0f) && NearEqual(local.pitch, 0.0f) &&
                    NearEqual(local.roll, 0.0f),
                "a centred head leaves the camera-local pose untouched");
    }

    // THE SIGNS, inherited from the HPL2 ancestor where they were confirmed in a
    // running game: yaw is negated at the boundary, pitch and roll are not.
    {
        const CameraPose out = ApplyHeadTracking(clean, 10.0f, 0, 0, 0, 0, 0, true);
        r.Check(NearEqual(out.yaw, -10.0f * kDegToRad), "head yaw is NEGATED into the camera");
    }
    {
        const CameraPose out = ApplyHeadTracking(clean, 0, 10.0f, 0, 0, 0, 0, true);
        r.Check(NearEqual(out.pitch, 10.0f * kDegToRad), "head pitch is NOT negated");
    }
    {
        const CameraPose out = ApplyHeadTracking(clean, 0, 0, 10.0f, 0, 0, 0, true);
        r.Check(NearEqual(out.roll, 10.0f * kDegToRad), "head roll is NOT negated");
    }

    // World-space yaw adds to the game's own angles, so the horizon stays level
    // however far the mouse has pitched the camera.
    {
        CameraPose pitched;
        pitched.yaw = 0.3f;
        pitched.pitch = 0.5f;
        const CameraPose out = ApplyHeadTracking(pitched, 10.0f, 0, 0, 0, 0, 0, true);
        r.Check(NearEqual(out.yaw, 0.3f - 10.0f * kDegToRad) && NearEqual(out.pitch, 0.5f) &&
                    NearEqual(out.roll, 0.0f),
                "world-space yaw is a per-axis sum and adds no roll");
    }

    // Camera-local yaw goes round the camera's own up axis, which leans the view
    // once the camera is pitched away from level. That lean is the mode's
    // defining behaviour, not a fault - it is why the other mode exists.
    {
        CameraPose pitched;
        pitched.pitch = 0.5f;
        const CameraPose out = ApplyHeadTracking(pitched, 10.0f, 0, 0, 0, 0, 0, false);
        r.Check(std::fabs(out.roll) > 1e-3f, "camera-local yaw leans a pitched view");
    }

    // The game clamps its own pitch; the head adds to that, so the sum needs its
    // own bound before it passes straight up and the view turns over.
    {
        CameraPose steep;
        steep.pitch = 1.5f;
        const CameraPose up = ApplyHeadTracking(steep, 0, 30.0f, 0, 0, 0, 0, true);
        r.Check(NearEqual(up.pitch, SomaHT::math::kMaxPitchRadians),
                "pitch is clamped short of straight up");

        steep.pitch = -1.5f;
        const CameraPose down = ApplyHeadTracking(steep, 0, -30.0f, 0, 0, 0, 0, true);
        r.Check(NearEqual(down.pitch, -SomaHT::math::kMaxPitchRadians),
                "pitch is clamped short of straight down");
    }

    // Position, on a level camera whose right/up/forward are +x/+y/-z: x and z
    // are negated with the offset, y is not.
    {
        const CameraPose out = ApplyHeadTracking(clean, 0, 0, 0, 0.3f, 0.2f, 0.4f, true);
        r.Check(Vec3NearEqual(out.pos, -0.3f, 0.2f, 0.4f),
                "x and z are negated into the camera, y is not");
    }

    // The processor's NEGATIVE z is the forward lean, so it has to move the eye
    // along the camera's forward axis, which is -z for a level camera. A
    // mirrored sign here is the one that leaves 0.10m of travel for leaning in
    // and 0.40m for pulling back.
    {
        const CameraPose out = ApplyHeadTracking(clean, 0, 0, 0, 0, 0, -0.4f, true);
        r.Check(out.pos[2] < 0.0f, "a forward lean moves the eye along the view");
    }

    // The offset is applied along the CLEAN camera axes - the ones the game
    // aimed with - so a lean follows the body's orientation rather than the
    // head-turned view. Yawed a quarter turn the camera looks down +x with its
    // right axis at +z, so the same x offset that moved the eye along -x on a
    // level camera now moves it along -z and leaves x alone.
    {
        CameraPose yawed;
        yawed.yaw = -kQuarterTurn;
        const CameraPose out = ApplyHeadTracking(yawed, 0, 0, 0, 0.3f, 0, 0, true);
        r.Check(NearEqual(out.pos[0], 0.0f, 1e-3f) && NearEqual(out.pos[2], -0.3f, 1e-3f),
                "the offset is applied along the clean camera axes");
    }

    // Past vertical the composed rotation folds: a Euler triple read back out
    // of it reports an elevation inside a quarter turn and puts the rest into
    // yaw and roll, so a clamp on the reported pitch never fires. Before the
    // head's contribution was bounded, a 40 degree head tilt on a camera
    // already pitched 68 degrees turned the view upside down and half a turn
    // round in one frame.
    {
        CameraPose steep;
        steep.pitch = 1.2f;
        const CameraPose out = ApplyHeadTracking(steep, 0, 40.0f, 0, 0, 0, 0, false);
        r.Check(std::fabs(out.roll) < 1e-3f && std::fabs(out.yaw) < 1e-3f,
                "camera-local pitch past vertical does not flip the view");
        r.Check(NearEqual(out.pitch, SomaHT::math::kMaxPitchRadians),
                "camera-local pitch past vertical stops at the bound");
    }

    // The offset basis is HORIZON-LOCKED: the body's facing, world up, and the
    // flat forward. On the full clean orientation instead, leaning in while
    // looking down at 60 degrees would spend 0.346m of the 0.40m forward budget
    // going vertically - past the 0.20m the vertical limit claims to enforce -
    // and a rolled camera would tip a sideways lean.
    {
        CameraPose pitched;
        pitched.pitch = -1.0471976f;  // 60 degrees down
        const CameraPose out = ApplyHeadTracking(pitched, 0, 0, 0, 0, 0, -0.4f, true);
        r.Check(NearEqual(out.pos[1], 0.0f, 1e-4f) && NearEqual(out.pos[2], -0.4f, 1e-4f),
                "a forward lean stays level however far the camera is pitched");
    }
    {
        CameraPose rolled;
        rolled.roll = 0.5235988f;  // 30 degrees
        const CameraPose out = ApplyHeadTracking(rolled, 0, 0, 0, 0.3f, 0, 0, true);
        r.Check(NearEqual(out.pos[0], -0.3f, 1e-4f) && NearEqual(out.pos[1], 0.0f, 1e-4f),
                "a sideways lean stays level however far the camera is rolled");
    }

    // Rotation must not move the eye, and position must not turn the view.
    {
        const CameraPose rotated = ApplyHeadTracking(clean, 20.0f, 15.0f, 5.0f, 0, 0, 0, true);
        r.Check(Vec3NearEqual(rotated.pos, 0.0f, 0.0f, 0.0f),
                "head rotation alone does not move the eye");

        const CameraPose moved = ApplyHeadTracking(clean, 0, 0, 0, 0.3f, 0.2f, 0.4f, true);
        r.Check(NearEqual(moved.yaw, 0.0f) && NearEqual(moved.pitch, 0.0f) &&
                    NearEqual(moved.roll, 0.0f),
                "head position alone does not turn the view");
    }

    return r.Failures();
}
