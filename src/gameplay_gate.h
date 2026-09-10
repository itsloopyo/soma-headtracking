// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// Reads the engine's cLuxBase global (gpBase) and answers the one question the
// camera hook asks each frame: is this viewport the player looking at the world,
// or is it a menu, a loading screen or a security-monitor render target.
namespace SomaHT::gate {

// Resolves gpBase from the RIP-relative operand that opens
// cLuxMapHelper::GetClosestEntity. Returns false when the displacement points
// outside the executable's image, in which case every query below reports "not
// gameplay" and the mod stays out of the way.
bool Initialize(const void* getClosestEntityFn);

// The cLuxBase global itself. Null until Initialize succeeds.
const char* Base();

// The camera cLuxPlayer is looking through, or null. This is the camera a ray
// cast by the player's own pick check starts at, which is how a ray cast by
// something else - an agent's line of sight, a script's own probe - is told
// apart from the aim.
void* PlayerCamera();

// True when @p viewport is the game viewport, a map is loaded, the player is
// active and not paused, and @p camera is the player's own camera. Anything
// else - the main menu, a loading screen, a cGuiCameraTexture render, the
// pause menu - reports false and head tracking stays off.
bool IsGameplay(const void* viewport, const void* camera);

// The same test for a caller outside a viewport render, which has neither
// pointer to hand: the game viewport comes off the map handler and the camera
// off cLuxPlayer.
bool IsGameplay();

// The HUD sizes, each a cVector2f in {x, y} order.
//
// guiSetVirtualSize is the one that matters: it is read off the game HUD set
// itself and is the space every position handed to cGuiSet::DrawGfx is in, so a
// delta of half of it is one NDC unit. The three cLuxBase copies are logged
// once beside it because they are what the scripts quote, and a disagreement
// between them and the set is the first thing to look at if the crosshair moves
// by the wrong amount.
struct HudMetrics {
    float guiSetVirtualSize[2];
    float centerSize[2];
    float centerScreenSize[2];
    float virtualSize[2];
};

// HUD geometry, for turning a screen-space NDC offset into the units
// cGuiSet::DrawGfx takes. Returns false before gpBase is resolved or before
// cLuxBase has built its HUD set, leaving @p out untouched.
bool GetHudMetrics(HudMetrics& out);

}  // namespace SomaHT::gate
