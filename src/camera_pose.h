// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstring>

#include "engine_memory.h"
#include "game_offsets.h"
#include "hpl_math.h"

// cCamera's own angle and position fields: the six floats the head pose is
// written into for the duration of one viewport render and taken straight back
// out of afterwards.
//
// Writing the fields rather than post-multiplying the view matrix is what keeps
// the frame self-consistent. cCamera derives the view matrix, its inverse, the
// rotation matrix and both frustums from these fields, and the renderer uses
// all of them - the inverse view matrix reconstructs world positions from depth
// for the deferred lighting, so a view matrix patched on its own would light
// the scene from a camera that is no longer there.
namespace SomaHT::camera {

// cCamera rebuilds its view matrix, its rotation matrix and both frustums
// lazily, each behind its own byte. Writing an angle without these leaves the
// render using the matrix the previous tick built - the clean one - and the mod
// appears to do nothing at all.
//
// The projection byte is deliberately not here. The field of view is the only
// thing that invalidates the projection, and camera_fov.h sets it when it
// writes one.
inline void MarkPoseDirty(void* cam) {
    const offsets::Layout& l = offsets::Active();
    engine::MarkDirty(cam, l.camViewDirty);
    engine::MarkDirty(cam, l.camRotationDirty);
    engine::MarkDirty(cam, l.camFrustumDirty);
    engine::MarkDirty(cam, l.camExtFrustDirty);
}

inline math::CameraPose ReadPose(const void* cam) {
    const offsets::Layout& l = offsets::Active();
    math::CameraPose pose;
    pose.yaw = engine::ReadFloat(cam, l.camYaw);
    pose.pitch = engine::ReadFloat(cam, l.camPitch);
    pose.roll = engine::ReadFloat(cam, l.camRoll);
    pose.pos[0] = engine::ReadFloat(cam, l.camPosX);
    pose.pos[1] = engine::ReadFloat(cam, l.camPosY);
    pose.pos[2] = engine::ReadFloat(cam, l.camPosZ);
    return pose;
}

inline void WritePose(void* cam, const math::CameraPose& pose) {
    const offsets::Layout& l = offsets::Active();
    engine::WriteFloat(cam, l.camYaw, pose.yaw);
    engine::WriteFloat(cam, l.camPitch, pose.pitch);
    engine::WriteFloat(cam, l.camRoll, pose.roll);
    engine::WriteFloat(cam, l.camPosX, pose.pos[0]);
    engine::WriteFloat(cam, l.camPosY, pose.pos[1]);
    engine::WriteFloat(cam, l.camPosZ, pose.pos[2]);
    MarkPoseDirty(cam);
}

// The view matrix the engine built from those fields. cScene::RenderViewport
// asks the camera for its frustum before it renders anything, which rebuilds
// the matrix, so by the end of that call it is the engine's own answer for
// where the eye is and which way it faces - not a second composition by the mod
// that could disagree with the first.
//
// Read it there and nowhere later. SOMA's HUD is drawn after the viewport
// render returns, by which point the clean pose has gone back into the fields;
// the crosshair works from the basis EndViewport captured, not from a fresh
// read.
inline void ReadViewMatrix(const void* cam, float out[16]) {
    std::memcpy(out, static_cast<const char*>(cam) + offsets::Active().camViewMatrix, sizeof(float) * 16);
}

}  // namespace SomaHT::camera
