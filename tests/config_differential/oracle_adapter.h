// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// The oracle: the config reader and startup code of the dev pre-release (7d8e2c8), the only
// published build, compiled from oracle/ with the core sources they included at its pin
// (bd22895). Two libraries build it, each with its namespaces renamed at compile time so it links
// beside the current core: the reader, and the hotkey registration of that build's
// Mod::RegisterHotkeys against the chord guards of its pin. This header names no core type, so
// the test includes it without the renaming.

#include <array>
#include <string>
#include <vector>

namespace soma_oracle_view {

struct OracleConfig {
    int udpPort;
    float yawSens, pitchSens, rollSens;
    bool invertYaw, invertPitch, invertRoll;
    float localSmoothing, remoteSmoothing;
    bool positionEnabled;
    float posSensX, posSensY, posSensZ;
    float limitX, limitY, limitZ, limitZBack;
    bool crosshairCompensation;
    bool suppressEyeTracking;
    float fieldOfView;
    bool worldSpaceYaw;
    int toggleKey, trackingModeKey, yawModeKey;
    bool autoEnable;
};

// Config::Load on `path` into a default-constructed Config, as the dev build's Mod::LoadConfig
// ran it. `loaded` says whether it found a file; without one the build kept the defaults.
OracleConfig RunOracle(const std::string& path, bool& loaded);

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every set
// of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle, cycle and yaw
// mode actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
constexpr int kActions = 3;
using FireTable = std::vector<std::array<int, kActions>>;

// The dev build's Mod::RegisterHotkeys run on the three codes, pressing each key under each held
// set.
FireTable OracleFires(int toggleKey, int trackingModeKey, int yawModeKey);

}  // namespace soma_oracle_view
