// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// The config reader of the dev pre-release (7d8e2c8), the only published build that read
// HeadTracking.ini, frozen so a player updating from it is converted exactly as that build read
// the file. Nothing in this folder is ever edited. Three things differ from the reader it was
// taken from: it fills this frozen copy of that build's Config and defaults rather than the
// runtime type, it reports an absent file apart from one it read, and it runs as a free function.
// The build never wrote the file, so there was no write to strip. The core default values the old
// Config took from PositionSettings and smoothing_utils.h are written out here as numbers, and the
// port check it took from core's port_utils.h is copied in, so a later core cannot move what an
// old file converts to.

#include "cameraunlock/config/legacy_import.h"

#include <vector>

namespace SomaHT::legacy {

enum class ReadStatus {
    Read,
    // No file at the path, a path that is not absolute, or a file the old reader could not open.
    // Config holds the defaults.
    Absent,
};

struct Config {
    int   udpPort = 4242;

    float yawSens = 1.0f, pitchSens = 1.0f, rollSens = 1.0f;
    bool  invertYaw = false, invertPitch = false, invertRoll = false;
    float localSmoothing = 0.0f;
    float remoteSmoothing = 0.15f;

    bool  positionEnabled = true;
    float posSensX = 1.0f, posSensY = 1.0f, posSensZ = 1.0f;
    float limitX     = 0.30f;
    float limitY     = 0.20f;
    float limitZ     = 0.40f;
    float limitZBack = 0.10f;

    bool  crosshairCompensation = true;
    bool  suppressEyeTracking = true;
    float fieldOfView = 0.0f;
    bool  worldSpaceYaw = true;

    int   toggleKey       = 0x23;  // End
    int   trackingModeKey = 0x21;  // Page Up
    int   yawModeKey      = 0x22;  // Page Down

    bool  autoEnable = true;
};

// Reads the file at `path`, the ANSI path the dev build opened it by, into a default-constructed
// `c`.
ReadStatus Read(const char* path, Config& c);

// Every section and key Read reads, in the order it reads them.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace SomaHT::legacy
