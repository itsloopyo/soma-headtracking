// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace SomaHT {

// Locates cScene::RenderViewport by byte signature, resolves gpBase for the
// gameplay gate, and installs the MinHook detour that injects the head pose for
// the duration of one viewport render. Returns false - and leaves the game
// vanilla - if either fails.
bool InstallCameraHook();
void RemoveCameraHook();

}  // namespace SomaHT
