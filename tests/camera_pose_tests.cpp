// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for camera_pose.h.
//
// These lock the write half of the render-phase injection: the six fields the
// head pose goes into, and the four lazy-rebuild bytes that decide whether the
// engine notices. A write that lands in the fields but misses a dirty byte
// renders through the matrix the previous tick built, which looks exactly like
// a mod that is not running at all - the failure this file exists to catch.

#include "camera_pose.h"
#include "test_support.h"

#include <array>
#include <cstring>

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;
namespace camera = SomaHT::camera;
namespace offsets = SomaHT::offsets;

// Big enough to hold every cCamera offset the mod touches, dirty bytes included.
constexpr std::size_t kCameraSize = 0x800;
using FakeCamera = std::array<char, kCameraSize>;

float ReadFloat(const FakeCamera& cam, uint32_t offset) {
    float value = 0.0f;
    std::memcpy(&value, cam.data() + offset, sizeof(value));
    return value;
}

void WriteFloat(FakeCamera& cam, uint32_t offset, float value) {
    std::memcpy(cam.data() + offset, &value, sizeof(value));
}

// The offsets themselves, restated here as literals.
//
// Everything below writes through a Layout member and reads back through the
// same one, so it would pass just as happily with two of them transposed. These
// are the second statement of the numbers, and the derivation is in
// game_offsets.h: GetFOV returns +0x28, GetPosition returns this+0x10,
// cCamera::UpdateView writes the matrix at +0x74 from the three angles at
// +0x44/+0x48/+0x4c, and cCamera::SetFOV's three stores name the projection and
// frustum bytes.
static_assert(offsets::kLayout_20210927.camPosX == 0x10, "cCamera::mvPosition");
static_assert(offsets::kLayout_20210927.camPosY == 0x14, "cCamera::mvPosition.y");
static_assert(offsets::kLayout_20210927.camPosZ == 0x18, "cCamera::mvPosition.z");
static_assert(offsets::kLayout_20210927.camFov == 0x28, "cCamera::mfFOV");
static_assert(offsets::kLayout_20210927.camAspect == 0x2c, "cCamera::mfAspect");
static_assert(offsets::kLayout_20210927.camPitch == 0x44, "cCamera::mfPitch");
static_assert(offsets::kLayout_20210927.camYaw == 0x48, "cCamera::mfYaw");
static_assert(offsets::kLayout_20210927.camRoll == 0x4c, "cCamera::mfRoll");
static_assert(offsets::kLayout_20210927.camViewMatrix == 0x74, "cCamera::m_mtxView");
static_assert(offsets::kLayout_20210927.camViewDirty == 0x709, "view rebuild byte");
static_assert(offsets::kLayout_20210927.camProjDirty == 0x70a, "projection rebuild byte");
static_assert(offsets::kLayout_20210927.camRotationDirty == 0x70b, "rotation rebuild byte");
static_assert(offsets::kLayout_20210927.camFrustumDirty == 0x70c, "frustum rebuild byte");
static_assert(offsets::kLayout_20210927.camExtFrustDirty == 0x70d, "extended frustum byte");

}  // namespace

int RunCameraPoseTests() {
    std::cout << "\nCamera pose tests\n";
    Report r;

    // Read: the six fields, each from its own offset. A transposed pair here
    // would send the head's pitch into the camera's yaw.
    {
        FakeCamera cam{};
        WriteFloat(cam, offsets::Active().camYaw, 0.25f);
        WriteFloat(cam, offsets::Active().camPitch, -0.5f);
        WriteFloat(cam, offsets::Active().camRoll, 0.125f);
        WriteFloat(cam, offsets::Active().camPosX, 1.0f);
        WriteFloat(cam, offsets::Active().camPosY, 2.0f);
        WriteFloat(cam, offsets::Active().camPosZ, 3.0f);

        const SomaHT::math::CameraPose pose = camera::ReadPose(cam.data());
        r.Check(NearEqual(pose.yaw, 0.25f) && NearEqual(pose.pitch, -0.5f) &&
                    NearEqual(pose.roll, 0.125f),
                "the three angles are read from their own offsets");
        r.Check(NearEqual(pose.pos[0], 1.0f) && NearEqual(pose.pos[1], 2.0f) &&
                    NearEqual(pose.pos[2], 3.0f),
                "the position is read from its own offsets");
    }

    // Write, and back again. EndViewport hands the clean pose back through this
    // same path, so a round trip that does not land on the original is the mod
    // leaving the game aiming somewhere it did not choose.
    {
        FakeCamera cam{};
        SomaHT::math::CameraPose pose;
        pose.yaw = -1.25f;
        pose.pitch = 0.75f;
        pose.roll = -0.0625f;
        pose.pos[0] = -4.5f;
        pose.pos[1] = 0.5f;
        pose.pos[2] = 9.75f;

        camera::WritePose(cam.data(), pose);
        const SomaHT::math::CameraPose back = camera::ReadPose(cam.data());
        r.Check(NearEqual(back.yaw, pose.yaw) && NearEqual(back.pitch, pose.pitch) &&
                    NearEqual(back.roll, pose.roll),
                "the angles survive a write and read back");
        r.Check(NearEqual(back.pos[0], pose.pos[0]) && NearEqual(back.pos[1], pose.pos[1]) &&
                    NearEqual(back.pos[2], pose.pos[2]),
                "the position survives a write and read back");
    }

    // The four caches cCamera rebuilds from these fields. Missing one leaves the
    // render using the matrix the previous tick built.
    {
        FakeCamera cam{};
        camera::WritePose(cam.data(), SomaHT::math::CameraPose{});
        r.Check(cam[offsets::Active().camViewDirty] == 1, "a pose write marks the view matrix stale");
        r.Check(cam[offsets::Active().camRotationDirty] == 1, "a pose write marks the rotation matrix stale");
        r.Check(cam[offsets::Active().camFrustumDirty] == 1, "a pose write marks the frustum stale");
        r.Check(cam[offsets::Active().camExtFrustDirty] == 1, "a pose write marks the extended frustum stale");

        // The field of view is the only thing that invalidates the projection,
        // and camera_fov.h owns that byte. A pose write that dirtied it too
        // would rebuild the projection every frame for nothing.
        r.Check(cam[offsets::Active().camProjDirty] == 0, "a pose write leaves the projection alone");
    }

    // The view matrix comes back as the engine's own sixteen floats, in the
    // engine's own order. This is the matrix the crosshair projection is
    // decomposed from, rather than a second composition by the mod.
    {
        FakeCamera cam{};
        float source[16];
        for (int i = 0; i < 16; ++i) source[i] = static_cast<float>(i) + 0.5f;
        std::memcpy(cam.data() + offsets::Active().camViewMatrix, source, sizeof(source));

        float view[16] = {};
        camera::ReadViewMatrix(cam.data(), view);
        bool same = true;
        for (int i = 0; i < 16; ++i) same = same && NearEqual(view[i], source[i]);
        r.Check(same, "the view matrix is read whole and in order");
    }

    return r.Failures();
}
