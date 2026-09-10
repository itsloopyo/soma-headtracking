// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// SOMA's own Tobii support, held off while head tracking is running.
//
// The game moves the camera from the player's gaze by itself - Extended View -
// and offsets its crosshair to match. That is a second thing steering the view
// on top of the head pose, and the two do not compose: the mod's crosshair
// compensation projects the head's contribution, so the game's gaze offset
// lands on the crosshair a second time.
namespace SomaHT::eyetrack {

// Answers the game's own "is the eye tracker tracking" with a no while the mod
// wants it quiet. Nothing in the game or the device is written, so there is no
// state to put back: the moment Mod::SuppressEyeTracking() goes false the
// question is answered by the game again, and the eye tracking the player has
// configured returns on the next update tick.
bool InstallSuppression();
void RemoveSuppression();

}  // namespace SomaHT::eyetrack
