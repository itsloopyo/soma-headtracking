// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The six reticle litmus tests, run against the join the mod actually makes.
//
// Every other test file checks one function against a fixture. This one is the
// only place the camera modification and the crosshair projection meet, and
// that join is where reticle bugs live: a projection that agrees with the
// camera on single-axis poses and drifts on combined ones passes every
// per-function test in the tree.
//
// The chain here is the mod's, in order: ApplyHeadTracking writes a pose,
// PoseBasis gives the basis that pose renders through, and ProjectAimToNdc
// places the clean aim point in it. BuildViewMatrix stands in for the engine -
// it is the composition hpl_math.h documents cCamera::UpdateView as using - and
// the first check below is what keeps PoseBasis honest against it.
//
// The property being defended is that the crosshair sits on the point the
// interaction ray reached, whatever the head is doing. It holds today because
// the projection is basis-to-basis rather than a second derivation in Euler
// angles; nothing in the type system keeps it that way.

#include "aim_projection.h"
#include "hpl_math.h"
#include "test_support.h"

#include <cmath>

namespace {

using SomaHT::aim::ProjectAimToNdc;
using SomaHT::math::ApplyHeadTracking;
using SomaHT::math::CameraForward;
using SomaHT::math::CameraPose;
using SomaHT::math::ComposeCameraRotation;
using SomaHT::math::DecomposeView;
using SomaHT::math::PoseBasis;
using SomaHT::math::Rot3;
using SomaHT::math::ViewBasis;
using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;

constexpr float kDegToRad = SomaHT::math::kDegToRad;

// SOMA's own field, read off the running camera: 70 vertical degrees, and the
// horizontal half-field tangent is that scaled by the aspect.
const float kTanY = std::tan(35.0f * kDegToRad);
const float kAspect = 16.0f / 9.0f;
const float kTanX = kAspect * kTanY;

// What cCamera::UpdateView builds from the six fields the mod writes:
// V = R^T * T(-position), row-major storage with the translation in the last
// column. The rotation rows are the camera's world axes, which is what
// DecomposeView reads back.
void BuildViewMatrix(const CameraPose& pose, float view[16]) {
    const Rot3 r = ComposeCameraRotation(pose.yaw, pose.pitch, pose.roll);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            view[row * 4 + col] = r.m[col * 3 + row];
        }
        view[row * 4 + 3] = -(r.m[0 * 3 + row] * pose.pos[0] + r.m[1 * 3 + row] * pose.pos[1] +
                              r.m[2 * 3 + row] * pose.pos[2]);
    }
    view[12] = view[13] = view[14] = 0.0f;
    view[15] = 1.0f;
}

// Where the crosshair lands: the point the clean camera's aim reached at
// @p depth metres, projected into the frame the head pose is rendered with.
bool PlaceCrosshair(const CameraPose& clean, float yawDeg, float pitchDeg, float rollDeg,
                    float offX, float offY, float offZ, bool worldSpaceYaw, float depth,
                    float& ndcX, float& ndcY) {
    const CameraPose tracked =
        ApplyHeadTracking(clean, yawDeg, pitchDeg, rollDeg, offX, offY, offZ, worldSpaceYaw);

    ViewBasis basis{};
    PoseBasis(tracked, basis);

    float cleanForward[3];
    CameraForward(clean.yaw, clean.pitch, cleanForward);

    float aim[3];
    for (int i = 0; i < 3; ++i) {
        aim[i] = clean.pos[i] + cleanForward[i] * depth - basis.eye[i];
    }
    return ProjectAimToNdc(aim, basis.fwd, basis.right, basis.up, kTanX, kTanY, ndcX, ndcY);
}

}  // namespace

int RunReticleLitmusTests() {
    std::cout << "\nReticle litmus tests\n";
    Report r;

    CameraPose clean;  // level, at the origin

    // 0. PoseBasis is the basis the engine's own matrix decomposes to. The mod
    //    composes it rather than reading the matrix back, because SOMA folds its
    //    Extended View into that matrix without touching the angle fields; this
    //    is what stops the shortcut drifting from the engine's convention.
    {
        bool ok = true;
        const CameraPose poses[] = {
            CameraPose{},
            CameraPose{0.7f, -0.4f, 0.25f, {1.5f, -2.0f, 3.25f}},
            CameraPose{-2.3f, 1.1f, -0.9f, {-4.0f, 0.5f, 12.0f}},
        };
        for (const CameraPose& pose : poses) {
            float view[16];
            BuildViewMatrix(pose, view);
            ViewBasis fromMatrix{};
            DecomposeView(view, fromMatrix);

            ViewBasis composed{};
            PoseBasis(pose, composed);
            for (int i = 0; i < 3; ++i) {
                ok = ok && NearEqual(composed.right[i], fromMatrix.right[i], 1e-5f) &&
                     NearEqual(composed.up[i], fromMatrix.up[i], 1e-5f) &&
                     NearEqual(composed.fwd[i], fromMatrix.fwd[i], 1e-5f) &&
                     NearEqual(composed.eye[i], fromMatrix.eye[i], 1e-4f);
            }
        }
        r.Check(ok, "0. PoseBasis matches the basis the engine's view matrix decomposes to");
    }

    // 1. Pure roll leaves the crosshair at screen centre. Roll turns the frame
    //    about the view axis, and the aim point is on that axis.
    {
        bool ok = true;
        for (float roll : {-30.0f, -10.0f, 10.0f, 30.0f}) {
            float x = 0.0f, y = 0.0f;
            ok = ok && PlaceCrosshair(clean, 0, 0, roll, 0, 0, 0, true, 2.0f, x, y) &&
                 NearEqual(x, 0.0f, 1e-5f) && NearEqual(y, 0.0f, 1e-5f);
        }
        r.Check(ok, "1. pure roll leaves the crosshair at screen centre");
    }

    // 2. Pure pitch moves it straight up or down, and downwards when the head
    //    pitches up - the aim the head left behind is now below the view.
    {
        float x = 0.0f, y = 0.0f;
        const bool ok = PlaceCrosshair(clean, 0, 20.0f, 0, 0, 0, 0, true, 2.0f, x, y);
        r.Check(ok && NearEqual(x, 0.0f, 1e-5f) && y < -0.1f,
                "2. pure pitch moves the crosshair vertically, and the right way");
    }

    // 3. Pitch and roll together rotate the offset rigidly about centre. A
    //    projection that mishandled roll would stretch or wander instead, which
    //    is the drift that survives single-axis testing.
    {
        float baseX = 0.0f, baseY = 0.0f;
        bool ok = PlaceCrosshair(clean, 0, 15.0f, 0, 0, 0, 0, true, 2.0f, baseX, baseY);
        // Measured in tangent space, where the frame is isotropic.
        const float baseLen =
            std::sqrt(baseX * kTanX * baseX * kTanX + baseY * kTanY * baseY * kTanY);

        for (float roll : {10.0f, 20.0f, 30.0f, -20.0f}) {
            float x = 0.0f, y = 0.0f;
            ok = ok && PlaceCrosshair(clean, 0, 15.0f, roll, 0, 0, 0, true, 2.0f, x, y);
            const float len = std::sqrt(x * kTanX * x * kTanX + y * kTanY * y * kTanY);
            ok = ok && NearEqual(len, baseLen, 1e-4f);

            // ...and by exactly the roll angle, IN THE RIGHT DIRECTION. Comparing
            // magnitudes here would pass a projection that rotated the offset
            // the other way, which is the roll-sign fault that ships as a
            // crosshair drifting horizontally whenever roll meets pitch.
            const float turned = std::atan2(y * kTanY, x * kTanX) - std::atan2(baseY * kTanY, baseX * kTanX);
            ok = ok && NearEqual(turned, -roll * kDegToRad, 1e-3f);
        }
        r.Check(ok, "3. pitch with roll rotates the offset rigidly, with no wander");
    }

    // 4. World-space yaw with the camera looking straight down is a spin about
    //    the view axis, so the world turns and the crosshair does not move.
    {
        CameraPose down;
        down.pitch = -SomaHT::math::kMaxPitchRadians;
        bool ok = true;
        for (float yaw : {10.0f, 30.0f, 60.0f}) {
            float x = 0.0f, y = 0.0f;
            ok = ok && PlaceCrosshair(down, yaw, 0, 0, 0, 0, 0, true, 2.0f, x, y) &&
                 std::fabs(x) < 0.02f && std::fabs(y) < 0.02f;
        }
        r.Check(ok, "4. world-space yaw looking down leaves the crosshair at centre");
    }

    // 5. A sideways lean is pure parallax: the offset is the lean over the
    //    depth, at EVERY depth. Agreement at one distance and drift either side
    //    of it is what a fixed or stale depth looks like, and it is the failure
    //    this whole path exists to prevent.
    {
        bool ok = true;
        const float lean = 0.3f;
        for (float depth : {0.5f, 1.0f, 2.0f, 5.0f, 20.0f}) {
            float x = 0.0f, y = 0.0f;
            ok = ok && PlaceCrosshair(clean, 0, 0, 0, lean, 0, 0, true, depth, x, y) &&
                 NearEqual(x, lean / depth / kTanX, 1e-4f) && NearEqual(y, 0.0f, 1e-5f);
        }
        r.Check(ok, "5. a sideways lean projects as 1/depth parallax at every depth");
    }

    // 6. The same for a vertical lean.
    {
        bool ok = true;
        const float lean = 0.2f;
        for (float depth : {0.5f, 1.0f, 2.0f, 5.0f, 20.0f}) {
            float x = 0.0f, y = 0.0f;
            ok = ok && PlaceCrosshair(clean, 0, 0, 0, 0, lean, 0, true, depth, x, y) &&
                 NearEqual(y, -lean / depth / kTanY, 1e-4f) && NearEqual(x, 0.0f, 1e-5f);
        }
        r.Check(ok, "6. a vertical lean projects as 1/depth parallax at every depth");
    }

    // 7. And the last link in the chain: NDC into the units cGuiSet::DrawGfx
    //    takes. Half the set's virtual rect is one NDC unit on each axis, and
    //    GUI y runs DOWNWARDS while NDC y runs up - so the sprite for a pitched
    //    head, which litmus 2 puts at negative NDC y, has to land BELOW the
    //    position the game asked for, meaning a larger GUI y.
    {
        const float virtualSize[2] = {1479.11f, 768.0f};
        const float centre[2] = {512.0f, 384.0f};

        float x = centre[0], y = centre[1];
        SomaHT::aim::OffsetGuiPositionByNdc(0.0f, 0.0f, virtualSize, x, y);
        bool ok = NearEqual(x, centre[0]) && NearEqual(y, centre[1]);

        // A full NDC unit is half the virtual rect, per axis and independently
        // of the camera aspect - the set's rect is not the screen's.
        x = centre[0];
        y = centre[1];
        SomaHT::aim::OffsetGuiPositionByNdc(1.0f, 1.0f, virtualSize, x, y);
        ok = ok && NearEqual(x, centre[0] + virtualSize[0] * 0.5f, 1e-3f) &&
             NearEqual(y, centre[1] - virtualSize[1] * 0.5f, 1e-3f);

        // The whole chain, end to end: a head pitched up puts the aim below the
        // view, so the sprite moves DOWN the screen.
        float ndcX = 0.0f, ndcY = 0.0f;
        ok = ok && PlaceCrosshair(clean, 0, 20.0f, 0, 0, 0, 0, true, 2.0f, ndcX, ndcY);
        x = centre[0];
        y = centre[1];
        SomaHT::aim::OffsetGuiPositionByNdc(ndcX, ndcY, virtualSize, x, y);
        ok = ok && NearEqual(x, centre[0], 1e-3f) && y > centre[1];

        r.Check(ok, "7. an NDC offset maps to GUI units by half the set's rect, y downwards");
    }

    // An aim point behind the rendered view has no screen position, and the
    // crosshair hook hides the sprite rather than clamping it to an edge.
    {
        float x = 0.0f, y = 0.0f;
        r.Check(!PlaceCrosshair(clean, 170.0f, 0, 0, 0, 0, 0, true, 2.0f, x, y),
                "an aim point behind the rendered view is refused");
    }

    return r.Failures();
}
