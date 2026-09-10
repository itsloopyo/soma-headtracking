// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// The mouse ray a focused screen is picked with - SOMA's computer terminals.
//
// cLuxGuiHandler's input update does not hand the terminal a screen position.
// It casts a ray from the player camera through the mouse's own pixel,
// intersects it with the screen's mesh, and the point it lands on becomes the
// focused cImGui's mouse position - which is where the pointer is drawn and
// what a click is tested against. That cast runs in the update tick, where the
// camera carries the clean pose, while the frame the player is looking at was
// drawn through the head pose. Off centre, the two disagree: the ray lands on
// the part of the screen that would be under the mouse if the head had not
// moved, and the buttons the player can see are somewhere else.
//
// The hook puts the rendered pose back on the camera for the length of that one
// call, so the engine casts the ray through the camera the player is looking
// through, and takes it straight back out.
namespace SomaHT {

bool InstallGuiInputHook();
void RemoveGuiInputHook();

}  // namespace SomaHT
