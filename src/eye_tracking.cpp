// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "eye_tracking.h"

#include "game_offsets.h"
#include "hook_install.h"
#include "mod.h"

#include "cameraunlock/logging/file_log.h"

// One detour, on the smallest function that decides the whole feature.
//
// EyeTrackingHandler.hps recomputes mbActive in its VariableUpdate from three
// conditions taken together - a tracker device exists, the handler's own enable
// flag is set, and the device reports it is tracking - so a no there is
// re-asked every frame and takes the module with it: PostUpdate stops calling
// UpdateExtendedView, GetExtendedViewRotation and GetExtendedViewCrosshairOffset
// return zero to their callers, and Player.hps's own VariableUpdate writes
// SetExtendedPitch(0) / SetExtendedYaw(0) into the camera on the next tick. That
// is the state the game is in for the many players with no eye tracker plugged
// in, which is why it is the answer to give rather than zeroing the camera's
// extended angles: the crosshair offset is computed in script from the script's
// own copy of the rotation, so a camera-side fix leaves the crosshair riding the
// gaze with nothing under it.
//
// Suppressing it takes everything else the eye tracker drives with it - the
// larger gaze crosshair, gaze flashlight control, the reactive environment and
// AI. There is no narrower lever: they all read mbActive.
namespace SomaHT {
namespace {

// The prefix every line this file writes to the log carries.
constexpr const char* kCategory = "Eye tracking";
constexpr const char* kFunctionName = "cEyeTrackerTobii::IsTracking";

// cEyeTrackerTobii::IsTracking. Two instructions, returning the byte at +0x92
// zero-extended into eax, so the return is taken as int rather than char - the
// trampoline's value then reaches the caller with its upper bits exactly as the
// game left them.
using IsTracking_t = int(__fastcall*)(void* self);

IsTracking_t oIsTracking = nullptr;
void* g_isTracking = nullptr;

int __fastcall DetourIsTracking(void* self) {
    if (Mod::Instance().SuppressEyeTracking()) return 0;
    return oIsTracking(self);
}

}  // namespace

namespace eyetrack {

bool InstallSuppression() {
    // A silent scan rather than detour::Find: the signature matches at
    // IsUserPresent and the address worth logging is the one
    // kIsTrackingPatternOffset past it.
    void* match = detour::Scan(offsets::Active().isTrackingPattern);
    if (match == nullptr) {
        cameraunlock::logging::Line(
            "%s: could not be held off - %s is not where this build puts it.", kCategory,
            kFunctionName);
        return false;
    }

    void* isTracking = static_cast<char*>(match) + offsets::kIsTrackingPatternOffset;
    cameraunlock::logging::Line("%s: %s found at %p", kCategory, kFunctionName, isTracking);

    g_isTracking = detour::Install(kCategory, kFunctionName, isTracking,
                                   reinterpret_cast<void*>(&DetourIsTracking),
                                   reinterpret_cast<void**>(&oIsTracking));
    if (g_isTracking == nullptr) return false;

    cameraunlock::logging::Line(
        "%s: SOMA's own eye tracking is held off while head tracking is on.", kCategory);
    return true;
}

void RemoveSuppression() { detour::Remove(g_isTracking); }

}  // namespace eyetrack
}  // namespace SomaHT
