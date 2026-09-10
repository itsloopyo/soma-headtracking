// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace SomaHT {

// Locates cLuxMapHelper::GetClosestEntity, cLux_GetClosestBody and the
// two-argument cGuiSet::DrawGfx by byte signature and installs the MinHook
// detours that move the crosshair onto the point the interaction ray lands on.
// Returns false if any is missing; head tracking still runs without it.
bool InstallCrosshairHook();
void RemoveCrosshairHook();

}  // namespace SomaHT
