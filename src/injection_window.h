// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "camera_fov.h"
#include "camera_pose.h"
#include "hpl_math.h"

// The camera state one injection has to put back, and the putting back of it.
//
// The mod opens two of these windows and they are not nested: the render one
// spans cScene::RenderViewport, the gui input one spans
// cLuxGuiHandler::UpdateInput, which runs in the update tick outside any
// render. Each holds its own saved state, because a window that restored the
// other's would leave the game aiming with a pose it never set.
//
// The saved value and the flag that says it is worth writing back belong to the
// same object for a plain reason: whoever restores has to answer "was anything
// written" for the pose and for the field of view separately, and two loose
// bools per window is two chances to answer with the other window's.
namespace SomaHT::camera {

struct InjectionWindow {
    // The camera as the game left it, read on the way in.
    math::CameraPose clean{};

    // The field of view the game left on the camera, and whether this window
    // actually overrode it. fov::Apply assigns the saved value even when it
    // refuses to write, so the flag - not the value - is what says there is
    // something to hand back.
    float savedFov = 0.0f;
    bool  fovOverridden = false;

    // Whether the head pose reached the camera's own angle and position fields.
    // The render window writes one only when tracking is running and a pose has
    // arrived; the field of view is applied either way.
    bool  poseWritten = false;

    // Field of view first, then the pose, matching the order they were written.
    // Both clear their flag, so a second call - a hook that fires twice, a
    // shutdown after a restore - writes nothing.
    void Restore(void* cam) {
        if (fovOverridden) {
            fov::Restore(cam, savedFov);
            fovOverridden = false;
        }
        if (poseWritten) {
            // Back to the pose the game set, with every cache marked stale, so
            // the update tick that follows reads the camera it aimed with.
            // Every gameplay query in SOMA - the interaction ray, the sound
            // listener, enemy sight - goes through these fields.
            WritePose(cam, clean);
            poseWritten = false;
        }
    }
};

}  // namespace SomaHT::camera
