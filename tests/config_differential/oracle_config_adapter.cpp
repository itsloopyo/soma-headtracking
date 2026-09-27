// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Compiled into the config oracle library only, with `cameraunlock` and `SomaHT` renamed, so
// "config.h" here is the dev build's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

namespace soma_oracle_view {

OracleConfig RunOracle(const std::string& path, bool& loaded) {
    SomaHT::Config c;
    loaded = c.Load(path.c_str());
    OracleConfig o{};
    o.udpPort = c.udpPort;
    o.yawSens = c.yawSens;
    o.pitchSens = c.pitchSens;
    o.rollSens = c.rollSens;
    o.invertYaw = c.invertYaw;
    o.invertPitch = c.invertPitch;
    o.invertRoll = c.invertRoll;
    o.localSmoothing = c.localSmoothing;
    o.remoteSmoothing = c.remoteSmoothing;
    o.positionEnabled = c.positionEnabled;
    o.posSensX = c.posSensX;
    o.posSensY = c.posSensY;
    o.posSensZ = c.posSensZ;
    o.limitX = c.limitX;
    o.limitY = c.limitY;
    o.limitZ = c.limitZ;
    o.limitZBack = c.limitZBack;
    o.crosshairCompensation = c.crosshairCompensation;
    o.suppressEyeTracking = c.suppressEyeTracking;
    o.fieldOfView = c.fieldOfView;
    o.worldSpaceYaw = c.worldSpaceYaw;
    o.toggleKey = c.toggleKey;
    o.trackingModeKey = c.trackingModeKey;
    o.yawModeKey = c.yawModeKey;
    o.autoEnable = c.autoEnable;
    return o;
}

}  // namespace soma_oracle_view
