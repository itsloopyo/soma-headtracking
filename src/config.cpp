// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "legacy_config/legacy_config.h"

namespace SomaHT {

bool Config::Load(const char* path) {
    legacy::Config c;
    if (legacy::Read(path, c) == legacy::ReadStatus::Absent) return false;

    udpPort = c.udpPort;
    yawSens = c.yawSens;
    pitchSens = c.pitchSens;
    rollSens = c.rollSens;
    invertYaw = c.invertYaw;
    invertPitch = c.invertPitch;
    invertRoll = c.invertRoll;
    localSmoothing = c.localSmoothing;
    remoteSmoothing = c.remoteSmoothing;
    positionEnabled = c.positionEnabled;
    posSensX = c.posSensX;
    posSensY = c.posSensY;
    posSensZ = c.posSensZ;
    limitX = c.limitX;
    limitY = c.limitY;
    limitZ = c.limitZ;
    limitZBack = c.limitZBack;
    crosshairCompensation = c.crosshairCompensation;
    suppressEyeTracking = c.suppressEyeTracking;
    fieldOfView = c.fieldOfView;
    toggleKey = c.toggleKey;
    trackingModeKey = c.trackingModeKey;
    yawModeKey = c.yawModeKey;
    worldSpaceYaw = c.worldSpaceYaw;
    autoEnable = c.autoEnable;
    return true;
}

}  // namespace SomaHT
