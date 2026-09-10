// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for injection_window.h - putting the camera back.
//
// The head pose lives on the game's own cCamera for the length of one render
// and one gui input call, and every gameplay query in SOMA runs outside both.
// So a restore that misses a field leaves the player aiming through a camera
// they did not point, and a restore that fires when nothing was written puts a
// stale pose onto a camera the game has since moved. Both are silent in a
// screenshot, which is why they are pinned here.

#include "injection_window.h"
#include "test_support.h"

#include <array>
#include <cstring>
#include <iostream>

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

SomaHT::math::CameraPose Pose(float yaw, float pitch, float roll, float x, float y, float z) {
    SomaHT::math::CameraPose pose;
    pose.yaw = yaw;
    pose.pitch = pitch;
    pose.roll = roll;
    pose.pos[0] = x;
    pose.pos[1] = y;
    pose.pos[2] = z;
    return pose;
}

}  // namespace

int RunInjectionWindowTests() {
    std::cout << "\nInjection window tests\n";
    Report r;

    // Nothing written, nothing put back. The render window opens on every
    // gameplay viewport whether or not tracking is running, so a restore that
    // writes unconditionally would push the previous frame's pose onto a camera
    // the game has moved since.
    {
        FakeCamera cam{};
        WriteFloat(cam, offsets::Active().camYaw, 7.0f);
        WriteFloat(cam, offsets::Active().camFov, 1.2f);

        camera::InjectionWindow window;
        window.clean = Pose(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f);
        window.savedFov = 0.5f;
        window.Restore(cam.data());

        r.Check(NearEqual(ReadFloat(cam, offsets::Active().camYaw), 7.0f) &&
                    NearEqual(ReadFloat(cam, offsets::Active().camFov), 1.2f),
                "a window that wrote nothing leaves the camera alone");
        r.Check(cam[offsets::Active().camViewDirty] == 0,
                "a window that wrote nothing marks nothing stale");
    }

    // The pose half: all six fields, and the rebuild bytes that decide whether
    // the engine notices them.
    {
        FakeCamera cam{};
        camera::InjectionWindow window;
        window.clean = Pose(0.25f, -0.5f, 0.125f, 1.0f, 2.0f, 3.0f);
        window.poseWritten = true;

        camera::WritePose(cam.data(), Pose(9.0f, 9.0f, 9.0f, 9.0f, 9.0f, 9.0f));
        window.Restore(cam.data());

        const SomaHT::math::CameraPose back = camera::ReadPose(cam.data());
        r.Check(NearEqual(back.yaw, 0.25f) && NearEqual(back.pitch, -0.5f) &&
                    NearEqual(back.roll, 0.125f),
                "the three clean angles go back");
        r.Check(NearEqual(back.pos[0], 1.0f) && NearEqual(back.pos[1], 2.0f) &&
                    NearEqual(back.pos[2], 3.0f),
                "the clean position goes back");
        r.Check(cam[offsets::Active().camViewDirty] == 1 &&
                    cam[offsets::Active().camRotationDirty] == 1,
                "the restore marks the cached matrices stale");
    }

    // The field of view half. cLuxPlayer only calls SetFOV while a script fade
    // is in flight, so a field left overridden stays overridden for the rest of
    // the session.
    {
        FakeCamera cam{};
        camera::InjectionWindow window;
        window.savedFov = 1.221731f;  // the 70 vertical degrees config/game.cfg declares
        window.fovOverridden = true;

        WriteFloat(cam, offsets::Active().camFov, 0.9f);
        window.Restore(cam.data());

        r.Check(NearEqual(ReadFloat(cam, offsets::Active().camFov), 1.221731f),
                "the field of view the game set goes back");
        r.Check(cam[offsets::Active().camProjDirty] == 1,
                "the restored field marks the cached projection stale");
    }

    // Both flags clear, so a second restore is a no-op. The gui input hook can
    // be reached twice for one saved state - once from the input tick and once
    // from a screen taking focus - and the second would otherwise write a pose
    // the game has moved on from.
    {
        FakeCamera cam{};
        camera::InjectionWindow window;
        window.clean = Pose(0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        window.poseWritten = true;
        window.fovOverridden = true;
        window.savedFov = 1.0f;

        window.Restore(cam.data());
        r.Check(!window.poseWritten && !window.fovOverridden,
                "a restore clears both flags");

        WriteFloat(cam, offsets::Active().camYaw, 4.0f);
        WriteFloat(cam, offsets::Active().camFov, 0.4f);
        window.Restore(cam.data());
        r.Check(NearEqual(ReadFloat(cam, offsets::Active().camYaw), 4.0f) &&
                    NearEqual(ReadFloat(cam, offsets::Active().camFov), 0.4f),
                "a second restore writes nothing");
    }

    return r.Failures();
}
