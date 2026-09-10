// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

// The camera basis the renderer was handed, pulled back out of the matrix the
// engine itself built, and the rotation algebra needed to write a head pose
// into the engine's own angle fields.
//
// This mod does NOT compose its own view matrix. It writes the head pose into
// cCamera's angle and position fields for the duration of one viewport render
// and lets cCamera::UpdateView build the matrix, which is why there is no view
// matrix maths here to disagree with the engine's. HPL3 builds
//
//     V = Rz(-roll) * Rx(-pitch) * Ry(-yaw) * T(-position)
//
// (row-major storage, column-vector maths, angles in radians), so the rotation
// rows of the result are the camera's world-space basis and the camera's own
// world orientation is
//
//     R = Ry(yaw) * Rx(pitch) * Rz(roll).
//
// Both were read off cCamera::UpdateView and the three cMath rotation builders
// it calls, not assumed: the builders are the textbook RotateX / RotateY /
// RotateZ, the multiply is a*b, and the translate is applied first.
//
// Matching a convention is not deriving from an implementation: no engine code
// is copied or adapted here. See THIRD-PARTY-NOTICES.md.

namespace SomaHT::math {

struct ViewBasis {
    float eye[3];
    float right[3];
    float up[3];
    float fwd[3];
};

// Row 2 of the rotation part points backwards along the view, which is why the
// engine's own GetForward negates it. The eye is -R^T * translation.
inline void DecomposeView(const float view[16], ViewBasis& out) {
    out.right[0] = view[0];  out.right[1] = view[1];  out.right[2] = view[2];
    out.up[0]    = view[4];  out.up[1]    = view[5];  out.up[2]    = view[6];
    out.fwd[0]   = -view[8]; out.fwd[1]   = -view[9]; out.fwd[2]   = -view[10];

    const float t[3] = { view[3], view[7], view[11] };
    out.eye[0] = -(view[0] * t[0] + view[4] * t[1] + view[8]  * t[2]);
    out.eye[1] = -(view[1] * t[0] + view[5] * t[1] + view[9]  * t[2]);
    out.eye[2] = -(view[2] * t[0] + view[6] * t[1] + view[10] * t[2]);
}

// The camera's world orientation, row-major, m[row * 3 + col]. Columns are the
// camera's world axes: column 0 right, column 1 up, column 2 backwards.
struct Rot3 {
    float m[9];
};

inline Rot3 MulRot(const Rot3& a, const Rot3& b) {
    Rot3 r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            r.m[row * 3 + col] = a.m[row * 3 + 0] * b.m[0 * 3 + col] +
                                 a.m[row * 3 + 1] * b.m[1 * 3 + col] +
                                 a.m[row * 3 + 2] * b.m[2 * 3 + col];
        }
    }
    return r;
}

// R = Ry(yaw) * Rx(pitch) * Rz(roll), the orientation cCamera::UpdateView
// inverts into the view matrix. Angles in radians.
inline Rot3 ComposeCameraRotation(float yaw, float pitch, float roll) {
    const float cy = std::cos(yaw),   sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll),  sr = std::sin(roll);

    Rot3 r{};
    r.m[0] = cy * cr + sy * sp * sr;
    r.m[1] = -cy * sr + sy * sp * cr;
    r.m[2] = sy * cp;
    r.m[3] = cp * sr;
    r.m[4] = cp * cr;
    r.m[5] = -sp;
    r.m[6] = -sy * cr + cy * sp * sr;
    r.m[7] = sy * sr + cy * sp * cr;
    r.m[8] = cy * cp;
    return r;
}

// The inverse of ComposeCameraRotation: the yaw / pitch / roll triple that
// makes the engine build @p r. Needed only for the camera-local yaw mode, where
// the head rotation is applied on the right of the game's own orientation and
// the product is no longer a per-axis sum.
//
// At a pitch of a quarter turn the yaw and roll axes coincide and the split
// between them is arbitrary; the whole rotation is then attributed to yaw,
// which is the choice that leaves the view where it was rather than rolling it.
inline void DecomposeCameraRotation(const Rot3& r, float& yaw, float& pitch, float& roll) {
    float sp = -r.m[5];
    if (sp > 1.0f) sp = 1.0f;
    if (sp < -1.0f) sp = -1.0f;
    pitch = std::asin(sp);

    const float cp = std::sqrt(1.0f - sp * sp);
    if (cp > 1e-4f) {
        roll = std::atan2(r.m[3], r.m[4]);
        yaw  = std::atan2(r.m[2], r.m[8]);
    } else {
        roll = 0.0f;
        yaw = (sp > 0.0f) ? std::atan2(r.m[1], r.m[0]) : std::atan2(-r.m[1], r.m[0]);
    }
}

// The camera axes a rotation describes, in world space. Column 2 is the
// BACKWARDS axis, which is why forward is its negation.
inline void RotationForward(const Rot3& r, float fwd[3]) {
    fwd[0] = -r.m[2]; fwd[1] = -r.m[5]; fwd[2] = -r.m[8];
}

inline void RotationBasis(const Rot3& r, float right[3], float up[3], float fwd[3]) {
    right[0] = r.m[0]; right[1] = r.m[3]; right[2] = r.m[6];
    up[0]    = r.m[1]; up[1]    = r.m[4]; up[2]    = r.m[7];
    RotationForward(r, fwd);
}

// Where a camera at these angles looks, without building the rotation.
//
// Roll is absent because it cancels: it is applied about the view axis, which
// is the axis being asked for. Callers that want only the aim direction - the
// crosshair's no-hit fallback is the one here - go through this rather than
// composing a whole orientation and reading one column back out of it.
inline void CameraForward(float yaw, float pitch, float fwd[3]) {
    const float cy = std::cos(yaw),   sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    fwd[0] = -sy * cp;
    fwd[1] = sp;
    fwd[2] = -cy * cp;
}

// The clean camera state a viewport render starts from, and the tracked state
// it is rendered with. Angles in radians, matching cCamera's own fields.
struct CameraPose {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float pos[3] = {0.0f, 0.0f, 0.0f};
};

// The basis a camera at this pose renders with.
//
// This is the same answer DecomposeView gives for the matrix cCamera::UpdateView
// builds from the pose - the litmus tests hold the two together - and it is the
// one the crosshair projects through, because the engine's own matrix carries a
// rotation the pose does not.
//
// SOMA has native Tobii support, and its Extended View lives in cCamera's
// mfExtendedYaw / mfExtendedPitch rather than in the angle fields this mod
// writes. The engine folds it into the view matrix, and Player.hps offsets the
// crosshair by its own estimate of it every frame. So a crosshair delta taken
// from the engine's matrix would carry the extended rotation a second time, on
// top of the offset the game already applied.
inline void PoseBasis(const CameraPose& pose, ViewBasis& out) {
    RotationBasis(ComposeCameraRotation(pose.yaw, pose.pitch, pose.roll), out.right, out.up,
                  out.fwd);
    for (int i = 0; i < 3; ++i) out.eye[i] = pose.pos[i];
}

// Beyond this the view is upside down and the yaw/roll split of a camera-local
// composition stops being well conditioned. The game clamps its own pitch
// through cCamera::SetPitch; the head adds to that, so the sum needs its own
// bound.
//
// The bound is applied to the head's CONTRIBUTION, not to the sum read back
// out of a composed rotation. A Euler triple recovered from a rotation always
// reports an elevation inside a quarter turn, because it is asin of the
// forward vector's world y: past vertical the frame comes back the right way
// up in pitch and half a turn out in yaw AND roll, so a clamp on the reported
// pitch never fires and the camera-local mode flips the view instead of
// stopping at vertical.
constexpr float kMaxPitchRadians = 1.5620697f;  // 89.5 degrees

constexpr float kDegToRad = 0.01745329252f;

// Where the camera has to be for this frame to show what the head is looking
// at, with the game's own aim left untouched.
//
// THE SIGNS ARE INHERITED, NOT DERIVED. They come from
// amnesia-the-dark-descent-headtracking, the HPL2 ancestor this mod is a port
// of, where they were confirmed in a running game: head yaw is negated at the
// boundary, pitch and roll are not, and x and z are negated with the offset.
// The tracker protocol states no positive directions, so re-deriving them from
// the engine's handedness alone would be a coin flip per axis - and this pair
// of games shares an engine lineage, a camera convention and a hook shape, so
// the ancestor's answer is the verified one.
//
// The ancestor writes its head pose into the VIEW MATRIX rather than into the
// camera's angle fields, so the two files do not diff line for line. They were
// diffed by working the ancestor's expression back to a camera orientation.
// It builds V' = H * T(offX, -offY, -offZ) * V with
// H = Rz(-roll) * Rx(-pitch) * Ry(+yaw); a view matrix carries the camera's
// world rotation transposed, so that is the camera rotation
// R * Ry(-yaw) * Rx(+pitch) * Rz(+roll) - the same three signs applied on the
// right of the game's own orientation, which is what the camera-local branch
// below composes. Its translation, read as an eye movement in the clean
// camera's own axes, is -offX*right + offY*up - offZ*fwd, which is the
// expression below.
//
// @p worldSpaceYaw sends head yaw round the world up axis, so the horizon stays
// level however far the mouse has pitched the camera. HPL3 already applies the
// camera's own yaw outermost, which makes that mode a per-axis sum rather than
// a composition. The camera-local mode yaws about the camera's own up axis and
// has to go through the matrix.
//
// The offset is applied along a HORIZON-LOCKED basis built from the clean
// camera's yaw alone: the body's facing, world up, and the flat forward. Using
// the full clean orientation instead would send a forward lean down into the
// floor whenever the player is looking down - at a 60 degree clean pitch, the
// 0.40m forward budget spends 0.346m of it going vertically, which is past the
// 0.20m the vertical limit claims to enforce - and would tip a sideways lean
// whenever the camera rolls.
inline CameraPose ApplyHeadTracking(const CameraPose& clean,
                                    float yawDeg, float pitchDeg, float rollDeg,
                                    float offX, float offY, float offZ,
                                    bool worldSpaceYaw) {
    const float headYaw = -yawDeg * kDegToRad;
    const float headRoll = rollDeg * kDegToRad;

    // Bounded here rather than by clamping the sum, because past vertical the
    // sum is not what comes back - see kMaxPitchRadians. Rotating by the head's
    // pitch moves the view's elevation by at most that angle whatever the head
    // yaw is doing, so bounding the contribution against the camera's own
    // elevation keeps the composed frame the right way up in both modes.
    float headPitch = pitchDeg * kDegToRad;
    const float headPitchMax = kMaxPitchRadians - clean.pitch;
    const float headPitchMin = -kMaxPitchRadians - clean.pitch;
    if (headPitch > headPitchMax) headPitch = headPitchMax;
    if (headPitch < headPitchMin) headPitch = headPitchMin;

    CameraPose out = clean;
    if (worldSpaceYaw) {
        out.yaw = clean.yaw + headYaw;
        out.pitch = clean.pitch + headPitch;
        out.roll = clean.roll + headRoll;
    } else {
        const Rot3 cleanRot = ComposeCameraRotation(clean.yaw, clean.pitch, clean.roll);
        const Rot3 composed =
            MulRot(cleanRot, ComposeCameraRotation(headYaw, headPitch, headRoll));
        DecomposeCameraRotation(composed, out.yaw, out.pitch, out.roll);
    }

    // The game's own pitch can already sit outside the bound, so the sum still
    // gets clamped after the fact.
    if (out.pitch > kMaxPitchRadians) out.pitch = kMaxPitchRadians;
    if (out.pitch < -kMaxPitchRadians) out.pitch = -kMaxPitchRadians;

    float right[3], up[3], fwd[3];
    RotationBasis(ComposeCameraRotation(clean.yaw, 0.0f, 0.0f), right, up, fwd);
    for (int i = 0; i < 3; ++i) {
        out.pos[i] = clean.pos[i] - offX * right[i] + offY * up[i] - offZ * fwd[i];
    }
    return out;
}

}  // namespace SomaHT::math
