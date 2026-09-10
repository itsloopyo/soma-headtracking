// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for camera_fov.h.
//
// The field of view is the one setting SOMA never gave the player, so this is
// the mod's own surface rather than a passthrough. Two properties matter enough
// to lock: the projection dirty bytes cCamera::SetFOV writes (a bare mfFOV
// store draws the frame through the previous projection and moves nothing but
// the culling), and that a field the frame cannot be projected through is
// refused rather than written.

#include "camera_fov.h"
#include "test_support.h"

#include <array>
#include <cstring>
#include <limits>

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;
namespace fov = SomaHT::fov;
namespace offsets = SomaHT::offsets;

// Big enough to hold every cCamera offset the mod touches, dirty bytes included.
constexpr std::size_t kCameraSize = 0x800;
using FakeCamera = std::array<char, kCameraSize>;

FakeCamera MakeCamera(float fovRadians, float aspect) {
    FakeCamera cam{};
    std::memcpy(cam.data() + offsets::Active().camFov, &fovRadians, sizeof(fovRadians));
    std::memcpy(cam.data() + offsets::Active().camAspect, &aspect, sizeof(aspect));
    return cam;
}

float ReadFov(const FakeCamera& cam) {
    float value = 0.0f;
    std::memcpy(&value, cam.data() + offsets::Active().camFov, sizeof(value));
    return value;
}

bool DirtyBytesSet(const FakeCamera& cam) {
    return cam[offsets::Active().camProjDirty] == 1 &&
           cam[offsets::Active().camFrustumDirty] == 1 &&
           cam[offsets::Active().camExtFrustDirty] == 1;
}

// The game's own value: <Player FOV="70"> out of config/game.cfg.
constexpr float kGameFovRadians = 1.221731f;
constexpr float kGameAspect = 1.777778f;

}  // namespace

int RunCameraFovTests() {
    std::cout << "\nCamera field of view tests\n";
    Report r;

    // Zero is the off switch, not a bad value, and a field the engine has not
    // produced yet leaves the scale at 1 so the caller has one path through.
    r.Check(NearEqual(fov::ScaleFor(0.0f, 70.0f), 1.0f), "an unset field leaves the scale at 1");
    r.Check(NearEqual(fov::ScaleFor(-5.0f, 70.0f), 1.0f), "a negative field leaves the scale at 1");
    r.Check(NearEqual(fov::ScaleFor(85.0f, 0.0f), 1.0f), "a zero base field leaves the scale at 1");
    r.Check(NearEqual(fov::ScaleFor(85.0f, 70.0f), 85.0f / 70.0f), "the scale is a ratio, so script zooms stay proportional");

    // Leaving the field alone must not touch the camera at all.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        const bool ok = fov::Apply(cam.data(), 1.0f, field, saved);
        r.Check(ok, "a scale of 1 reports the field");
        r.Check(NearEqual(saved, kGameFovRadians), "a scale of 1 saves the engine's field");
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians), "a scale of 1 writes nothing");
        r.Check(cam[offsets::Active().camProjDirty] == 0, "a scale of 1 leaves the projection clean");
        r.Check(NearEqual(field.tanY, std::tan(kGameFovRadians * 0.5f)), "tanY is the vertical half-field");
        r.Check(NearEqual(field.tanX, kGameAspect * field.tanY), "tanX derives from the aspect");
    }

    // An override scales the field and marks the cached projection stale.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        const float scale = 85.0f / 70.0f;
        const bool ok = fov::Apply(cam.data(), scale, field, saved);
        r.Check(ok, "an override reports the field");
        r.Check(NearEqual(saved, kGameFovRadians), "an override saves the engine's field for the restore");
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians * scale), "an override writes the scaled field");
        r.Check(NearEqual(field.fovY, kGameFovRadians * scale), "the reported field is the drawn one");
        r.Check(NearEqual(field.tanY, std::tan(kGameFovRadians * scale * 0.5f)), "the tangents are read after the override");
        r.Check(DirtyBytesSet(cam), "an override sets the projection and both frustum bytes");
        r.Check(cam[offsets::Active().camViewDirty] == 0, "an override leaves the view dirty byte alone");
    }

    // A field of zero - what a script zoom multiplier of zero produces on its
    // way through - has no tangent to project a crosshair by.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        r.Check(!fov::Apply(cam.data(), 0.0f, field, saved), "a scale of zero is refused");
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians), "a refused field writes nothing");
        r.Check(cam[offsets::Active().camProjDirty] == 0, "a refused field leaves the projection clean");
    }

    // Half a turn is where the tangent stops existing.
    {
        FakeCamera cam = MakeCamera(2.0f, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        r.Check(!fov::Apply(cam.data(), 2.0f, field, saved), "a field past half a turn is refused");
        r.Check(NearEqual(ReadFov(cam), 2.0f), "a field past half a turn writes nothing");
    }

    // No aspect means no horizontal half-field.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, 0.0f);
        fov::Field field{};
        float saved = 0.0f;
        r.Check(!fov::Apply(cam.data(), 1.0f, field, saved), "a zero aspect is refused");
    }

    // The restore is not optional housekeeping: cLuxPlayer only calls SetFOV
    // while a script fade is in flight, so nothing else puts the field back.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        fov::Apply(cam.data(), 85.0f / 70.0f, field, saved);
        cam[offsets::Active().camProjDirty] = 0;
        cam[offsets::Active().camFrustumDirty] = 0;
        cam[offsets::Active().camExtFrustDirty] = 0;

        fov::Restore(cam.data(), saved);
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians), "the restore hands the engine's field back");
        r.Check(DirtyBytesSet(cam), "the restore marks the projection stale again");
    }

    // Restoring a field that is already there must not dirty the projection.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Restore(cam.data(), kGameFovRadians);
        r.Check(cam[offsets::Active().camProjDirty] == 0, "restoring an unchanged field writes nothing");
    }

    // Apply assigns `saved` before it validates, so a refused Apply hands the
    // caller back the value it just rejected. Restoring that would write a NaN
    // or a past-half-turn field into mfFOV and mark the projection stale, and
    // nothing in the game puts it right: cLuxPlayer calls SetFOV only while a
    // script fade is running.
    {
        FakeCamera cam = MakeCamera(kGameFovRadians, kGameAspect);
        fov::Field field{};
        float saved = 0.0f;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        r.Check(!fov::Apply(cam.data(), nan, field, saved), "a NaN scale is refused");

        fov::Restore(cam.data(), nan);
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians), "the restore refuses a NaN field");
        r.Check(cam[offsets::Active().camProjDirty] == 0, "a refused restore leaves the projection clean");

        fov::Restore(cam.data(), 0.0f);
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians), "the restore refuses a zero field");

        fov::Restore(cam.data(), 4.0f);
        r.Check(NearEqual(ReadFov(cam), kGameFovRadians),
                "the restore refuses a field past half a turn");
        r.Check(cam[offsets::Active().camProjDirty] == 0, "none of the three dirtied the projection");
    }

    // The zoom factor. SOMA's field moves under script - FadeCameraFOVTo and
    // FadeCameraFOVMulTo are both on the Player interface - and a narrower field
    // magnifies head tracking along with the rest of the frame.
    {
        // Ordinary play: the game's own field, no override, nothing zooming.
        // This is the release gate. Anything but 1 here is a units fault, and
        // it renders as head tracking that feels weak or strong everywhere.
        fov::Field drawn{};
        drawn.fovY = kGameFovRadians;
        drawn.tanY = std::tan(kGameFovRadians * 0.5f);
        r.Check(NearEqual(fov::ZoomFactor(drawn, fov::NormalPlayRadians(0.0f, 70.0f)), 1.0f),
                "the game's own field at rest scales the pose by exactly 1");

        // And with the mod owning the setting, because the player's normal is
        // then the configured field. Measuring against the game's 70 instead
        // would put a constant on every frame of the session.
        fov::Field wide{};
        wide.fovY = kGameFovRadians * (85.0f / 70.0f);
        wide.tanY = std::tan(wide.fovY * 0.5f);
        r.Check(NearEqual(fov::ZoomFactor(wide, fov::NormalPlayRadians(85.0f, 70.0f)), 1.0f),
                "a configured field at rest also scales the pose by exactly 1");

        // A script zoom to half the field: the pose shrinks by the tangent
        // ratio, so the picture moves the same distance it did unzoomed.
        fov::Field zoomed{};
        zoomed.fovY = kGameFovRadians * 0.5f;
        zoomed.tanY = std::tan(zoomed.fovY * 0.5f);
        const float expected = zoomed.tanY / std::tan(kGameFovRadians * 0.5f);
        r.Check(NearEqual(fov::ZoomFactor(zoomed, fov::NormalPlayRadians(0.0f, 70.0f)), expected),
                "a zoom scales the pose by the ratio of the half-field tangents");
        r.Check(fov::ZoomFactor(zoomed, fov::NormalPlayRadians(0.0f, 70.0f)) < 1.0f,
                "zooming in shrinks the pose");

        // No reference to measure against leaves the pose at 1:1 rather than
        // guessing one.
        r.Check(NearEqual(fov::ZoomFactor(drawn, fov::NormalPlayRadians(0.0f, 0.0f)), 1.0f),
                "an unreadable base field applies no compensation");
        r.Check(NearEqual(fov::ZoomFactor(drawn, 4.0f), 1.0f),
                "a base field past half a turn applies no compensation");

        // Both terms are VERTICAL. NormalPlayRadians converts the same degrees
        // cLuxPlayer runs through cMath::ToRad, so it must land on the field
        // Apply reads back off the camera.
        r.Check(NearEqual(fov::NormalPlayRadians(0.0f, 70.0f), kGameFovRadians),
                "the reference converts the game's degrees to the camera's radians");
        r.Check(NearEqual(fov::NormalPlayRadians(85.0f, 70.0f),
                          fov::NormalPlayRadians(0.0f, 85.0f)),
                "a configured field is the reference in the game's own units");

        // ScaleFor declines to install the configured field when the game
        // reported none, so the reference has to decline with it. Answering
        // with the configured field there would measure every frame of the
        // session against one the game is not drawing with, which puts a
        // constant on the head pose and reads in game as tracking that feels
        // weak everywhere rather than wrong anywhere.
        r.Check(NearEqual(fov::ScaleFor(100.0f, 0.0f), 1.0f),
                "no game field means the mod installs none");
        fov::Field unreferenced{};
        unreferenced.fovY = kGameFovRadians;
        unreferenced.tanY = std::tan(unreferenced.fovY * 0.5f);
        r.Check(NearEqual(fov::ZoomFactor(unreferenced, fov::NormalPlayRadians(100.0f, 0.0f)), 1.0f),
                "and no reference either, so the pose is not scaled against a field "
                "the frame was not drawn with");
    }

    // Roll is not scaled and the other axes are, which is the split the pose
    // composition relies on.
    {
        using cameraunlock::camera::ScaleAngleForZoom;
        const float half = 0.5f;
        r.Check(ScaleAngleForZoom(10.0f, half) < 10.0f, "a zoom shrinks a tracked angle");
        r.Check(NearEqual(ScaleAngleForZoom(10.0f, 1.0f), 10.0f),
                "an unzoomed frame leaves a tracked angle alone");
        r.Check(NearEqual(ScaleAngleForZoom(0.0f, half), 0.0f), "a centred axis stays centred");
    }

    // The scaling is atan(tan(angle) * factor), so it is only defined inside a
    // quarter turn. Past that the tangent wraps and the view turns the OTHER
    // way: 120 degrees of head yaw comes back as -60. The pose reaching here is
    // the tracker's after the mod's own multipliers, so a 35 degree head turn
    // at YawMultiplier=3 is already there.
    {
        using cameraunlock::camera::ScaleAngleForZoom;
        r.Check(ScaleAngleForZoom(120.0f, 1.0f) < 0.0f,
                "the raw scaling wraps a head angle past a quarter turn");

        r.Check(NearEqual(fov::ScalePoseAngle(30.0f, 1.0f), 30.0f),
                "an unzoomed angle inside the domain passes through untouched");
        r.Check(fov::ScalePoseAngle(30.0f, 0.5f) < 30.0f, "a zoom still shrinks it");

        r.Check(NearEqual(fov::ScalePoseAngle(120.0f, 1.0f), fov::kMaxScalableAngleDegrees),
                "an angle past a quarter turn is held at the bound, not wrapped");
        r.Check(NearEqual(fov::ScalePoseAngle(-120.0f, 1.0f), -fov::kMaxScalableAngleDegrees),
                "and the same on the other side");
        r.Check(fov::ScalePoseAngle(100.0f, 0.5f) > 0.0f,
                "a bounded angle keeps its sign through a zoom");
    }

    // The zoom factor multiplies a lean the position processor has already
    // clamped, so without a re-clamp a field WIDER than ordinary play carries
    // the eye back out through LimitX / LimitY / LimitZ. Those limits are what
    // keep the rendered eye inside the player's body.
    {
        constexpr float kLimit = 0.30f;
        r.Check(NearEqual(fov::ScalePoseOffset(0.30f, 1.0f, -kLimit, kLimit), 0.30f),
                "an unzoomed lean passes through untouched");
        r.Check(NearEqual(fov::ScalePoseOffset(0.30f, 0.5f, -kLimit, kLimit), 0.15f),
                "a zoom in shrinks the lean and never reaches the clamp");
        r.Check(NearEqual(fov::ScalePoseOffset(0.30f, 1.7f, -kLimit, kLimit), kLimit),
                "a wide field cannot scale the lean past its limit");
        r.Check(NearEqual(fov::ScalePoseOffset(-0.30f, 1.7f, -kLimit, kLimit), -kLimit),
                "and the same leaning the other way");

        // z is asymmetric in the processor's own convention: negative is the
        // forward lean, and the backward budget is the small one that stops the
        // camera reversing through the player model.
        r.Check(NearEqual(fov::ScalePoseOffset(-0.40f, 3.0f, -0.40f, 0.10f), -0.40f),
                "a scaled forward lean is held at LimitZ");
        r.Check(NearEqual(fov::ScalePoseOffset(0.10f, 3.0f, -0.40f, 0.10f), 0.10f),
                "a scaled backward lean is held at the tighter LimitZBack");
    }

    // The field the game itself would use, off the cLuxBase global.
    {
        std::array<char, 0x200> userConfig{};
        const float degrees = 70.0f;
        std::memcpy(userConfig.data() + offsets::Active().userConfigFovDegrees, &degrees, sizeof(degrees));

        std::array<char, 0x200> base{};
        const void* configPtr = userConfig.data();
        std::memcpy(base.data() + offsets::Active().baseUserConfig, &configPtr, sizeof(configPtr));

        r.Check(NearEqual(fov::BaseDegrees(base.data()), 70.0f), "the base field is read as degrees off cLuxUserConfig");

        std::array<char, 0x200> baseWithoutConfig{};
        r.Check(NearEqual(fov::BaseDegrees(baseWithoutConfig.data()), 0.0f), "a missing user config reports no base field");

        r.Check(NearEqual(fov::BaseDegrees(nullptr), 0.0f), "an unresolved gpBase reports no base field");
    }

    return r.Failures();
}
