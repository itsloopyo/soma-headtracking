// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

#include "cameraunlock/camera/zoom_compensation.h"

#include "engine_memory.h"
#include "game_offsets.h"

// The field of view the frame is drawn with, and the one setting SOMA never
// gave the player.
//
// HPL3 keeps the VERTICAL field on the camera (cCamera::mfFOV, radians) and
// derives the horizontal one from the viewport aspect, so a wider monitor gets
// a wider picture rather than a shorter one. cMath::MatrixPerspectiveProjection
// takes tan(mfFOV/2) as the top of the near plane and multiplies by mfAspect
// for the sides, which is where the two half-field tangents below come from.
//
// Both numbers were read back out of the running game rather than deduced:
// mfFOV came back as 1.221731 rad, the 70 degrees config/game.cfg declares in
// its <Player FOV="70"> line, and mfAspect matched the display.
//
// SOMA HAS NO FIELD OF VIEW CONTROL. The engine does load one - the default is
// that <Player FOV> line and a <Gameplay FOV> attribute in the player's
// main_settings.cfg overrides it, which the game then writes back out on exit -
// but nothing in the game ever sets it. The only code references to that "FOV"
// key are the settings load, the settings save and an unrelated spotlight
// property in the map loader; the "HORIZONTAL FOV" entry in the language files
// has no reference at all, and its label is wrong as well. So the only route a
// player has is hand-editing an undocumented attribute, which is why the mod
// carries the setting instead.

namespace SomaHT::fov {

// Half a turn is where the tangent stops existing, and a crosshair divided by
// it is a NaN on its way to a vertex buffer.
constexpr float kMaxFieldRadians = 3.14159265f;

// What the render will use, and the half-field tangents that go to
// ProjectAimToNdc. These have to be the tangents of the field the frame is
// actually drawn with: a reticle projected through the field the game asked for
// and drawn into a frame rendered with a different one misses by the ratio
// between them, which is exactly what changing the field of view does.
struct Field {
    float fovY   = 0.0f;  // vertical, radians
    float aspect = 0.0f;
    float tanX   = 0.0f;
    float tanY   = 0.0f;
};

// The field the game itself would use, in degrees, off the cLuxBase global.
// The global is null until the gate resolves it, and its settings object is
// null for the moment before cLuxBase builds one; both report no base field,
// which ScaleFor below turns into leaving the game's own alone.
inline float BaseDegrees(const void* gpBase) {
    if (gpBase == nullptr) return 0.0f;

    const char* config = engine::ReadPointer(gpBase, offsets::Active().baseUserConfig);
    if (config == nullptr) return 0.0f;

    return engine::ReadFloat(config, offsets::Active().userConfigFovDegrees);
}

// The multiplier that turns whatever field the engine has into the configured
// one. It is a ratio and not an absolute so that the script-driven zooms keep
// working: Area_Zoom fades the field to its own target and the anglerfish
// chases with a multiplier, and both stay proportional through this.
//
// Returns 1 when the mod is leaving the field alone, so the caller has one path
// through Apply and not two.
inline float ScaleFor(float configuredDegrees, float baseDegrees) {
    if (!(configuredDegrees > 0.0f) || !(baseDegrees > 0.0f)) return 1.0f;
    return configuredDegrees / baseDegrees;
}

// The field ORDINARY PLAY is drawn with, in radians, which is the reference the
// zoom factor below is measured against.
//
// It is the configured field when the mod owns the setting and the game's own
// when it does not, because the player's normal is whatever they normally look
// at. Measuring against the game's 70 while the player has asked for 85 would
// put a constant 1.23 on the head pose for every frame of the session, which is
// the same fault as pairing a vertical field with a horizontal one and has the
// same symptom - head tracking that feels wrong everywhere rather than anywhere.
//
// VERTICAL, to match the field Apply reports. Both numbers this converts are
// vertical: cLuxUserConfig +0xd0 is the degrees cLuxPlayer runs through
// cMath::ToRad on its way to cCamera::SetFOV, and the mod's own setting is
// documented and clamped in the same units.
// The configured field is only the reference when the mod actually installed
// it, and ScaleFor declines to when the game reported no field of its own. The
// two have to agree about that: answering with a field the frame is not being
// drawn with puts a constant on the head pose for the whole session, which is
// the fault this function exists to avoid rather than cause.
inline float NormalPlayRadians(float configuredDegrees, float baseDegrees) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const bool modOwnsField = configuredDegrees > 0.0f && baseDegrees > 0.0f;
    const float degrees = modOwnsField ? configuredDegrees : baseDegrees;
    return degrees * kDegToRad;
}

// How much of the head pose to apply, so a zoom does not magnify head tracking
// along with everything else in the frame.
//
// SOMA's field moves under script: FadeCameraFOVTo and FadeCameraFOVMulTo are
// both on the Player interface and cLuxPlayer::UpdateCameraFOV fades mfFOV
// towards its goal. A narrower field means the same head angle sweeps further
// across the screen, and the player reads that as the mod's sensitivity jumping
// the moment the game pulls in.
//
// Returns 1 when there is no reference to measure against - an unresolved
// cLuxBase, or a settings object the game has not filled in yet - so the pose
// passes through unscaled rather than through a guessed factor.
inline float ZoomFactor(const Field& drawn, float normalPlayRadians) {
    if (!(normalPlayRadians > 0.0f) || !(normalPlayRadians < kMaxFieldRadians)) return 1.0f;
    const float tanNormal = std::tan(normalPlayRadians * 0.5f);
    if (!(tanNormal > 0.0f)) return 1.0f;
    return cameraunlock::camera::FovZoomFactor(drawn.tanY, tanNormal);
}

// The furthest a head angle may be from centre before the zoom scaling stops
// being defined on it.
//
// ScaleAngleForZoom is atan(tan(angle) * factor), and the tangent wraps at a
// quarter turn: 120 degrees comes back as -60, so an angle past that point
// turns the view the OTHER WAY rather than further. Core states the
// precondition on its own function - "angle_deg must be within +/-90" - and
// meeting it is this boundary's job, because what arrives here is the tracker's
// pose after the mod's own YawMultiplier / PitchMultiplier: a 35 degree head
// turn at a multiplier of 3 is already past the wrap.
//
// The value matches math::kMaxPitchRadians, which is the elevation the composed
// pose is held inside anyway.
constexpr float kMaxScalableAngleDegrees = 89.5f;

// A tracked angle, scaled so its displacement on screen is what it would have
// been at the field ordinary play is drawn with.
inline float ScalePoseAngle(float degrees, float zoom) {
    float bounded = degrees;
    if (bounded > kMaxScalableAngleDegrees) bounded = kMaxScalableAngleDegrees;
    if (bounded < -kMaxScalableAngleDegrees) bounded = -kMaxScalableAngleDegrees;
    return cameraunlock::camera::ScaleAngleForZoom(bounded, zoom);
}

// A tracked lean, scaled for the same reason, and put back inside the travel
// limit it had already been clamped to.
//
// PositionProcessor applies LimitX / LimitY / LimitZ / LimitZBack and the mod
// then multiplies by the zoom factor, so without this the two run in the wrong
// order and the limit is not the last word. A field WIDER than ordinary play
// scales by more than 1 and carries the eye straight back out through it: a
// script fade to 100 degrees against a 70 degree normal is a factor of 1.70,
// which turns the default 0.30m sideways budget into 0.51m. Those limits are
// what keep the rendered eye inside the player's body - LimitZBack's 0.10m
// against LimitZ's 0.40m exists to stop the camera reversing through the player
// model - so they have to hold whatever the game does with its field. A zoom
// IN scales by less than 1 and never reaches the clamp.
//
// @p lo and @p hi are the processor's own bounds for this axis, in the
// processor's sign convention: negative z is the forward lean, so z arrives as
// [-LimitZ, +LimitZBack].
inline float ScalePoseOffset(float metres, float zoom, float lo, float hi) {
    const float scaled = metres * zoom;
    if (scaled > hi) return hi;
    if (scaled < lo) return lo;
    return scaled;
}

// cCamera::SetFOV's own three stores: the field, then the projection rebuild
// byte and both frustum ones. The projection matrix is cached inside the camera
// and only rebuilt when that byte is set, so a bare write to mfFOV draws the
// frame through the previous projection and moves nothing but the culling.
inline void Write(void* camera, float fovY) {
    const offsets::Layout& l = offsets::Active();
    engine::WriteFloat(camera, l.camFov, fovY);
    engine::MarkDirty(camera, l.camProjDirty);
    engine::MarkDirty(camera, l.camFrustumDirty);
    engine::MarkDirty(camera, l.camExtFrustDirty);
}

// Scales the camera's field by @p scale for the render that follows and reports
// what that render will use. The tangents come back from the same call because
// they have to be read AFTER the override, not before it.
//
// @p saved receives the value to hand to Restore once the render is done.
// Returns false when the camera is not describing a frame that can be projected
// through - a field of zero, which a script zoom multiplier of zero produces on
// its way through - and in that case nothing was written.
inline bool Apply(void* camera, float scale, Field& out, float& saved) {
    const float aspect = engine::ReadFloat(camera, offsets::Active().camAspect);
    saved = engine::ReadFloat(camera, offsets::Active().camFov);

    const float fov = saved * scale;
    if (!(fov > 0.0f) || !(fov < kMaxFieldRadians) || !(aspect > 0.0f)) return false;

    if (scale != 1.0f) Write(camera, fov);

    out.fovY   = fov;
    out.aspect = aspect;
    out.tanY   = std::tan(fov * 0.5f);
    out.tanX   = aspect * out.tanY;
    return true;
}

// Hands the engine's own field back at the end of the render.
//
// This is not optional housekeeping. cLuxPlayer only calls SetFOV while a
// script fade is in flight, so nothing corrects the field on the next tick: a
// field left overridden stays overridden, and the next fade would then start
// from the mod's value and lerp its own target away from it.
//
// Only a field the render could have been drawn with is handed back. Apply
// assigns @p saved before it validates, so a refused Apply leaves the caller
// holding the very value Apply rejected - a zero from a script zoom multiplier,
// or a NaN out of a camera the engine has not finished building - and writing
// that back would put it into mfFOV and mark the cached projection stale. The
// engine does not recover from that: cLuxPlayer calls SetFOV only while a
// script fade is in flight, so a NaN projection matrix is drawn through for the
// rest of the session.
inline void Restore(void* camera, float savedFovY) {
    if (!(savedFovY > 0.0f) || !(savedFovY < kMaxFieldRadians)) return;
    if (engine::ReadFloat(camera, offsets::Active().camFov) == savedFovY) return;

    Write(camera, savedFovY);
}

}  // namespace SomaHT::fov
