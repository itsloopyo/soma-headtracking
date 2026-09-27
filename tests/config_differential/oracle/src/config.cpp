// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"
#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/protocol/port_utils.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace SomaHT {
namespace {

namespace guards = cameraunlock::config;

// Every number in the file is read through guards::ReadFloatChecked rather than
// IniReader::ReadFloat. ReadFloat is a strtod PREFIX parse, so
// "LocalSmoothing=0,15" - a European decimal comma, which is the expected user
// error - yields 0.0, sits inside the valid range and passes every check with
// nothing in the log. ReadFloatChecked strips the inline comment, requires the
// whole token to parse, refuses NaN and Inf before they reach exp() or the view
// matrix, and reports whatever it had to correct.
//
// The bounds are the guards' own, which bound what would arrive at the camera
// as garbage rather than what is a sensible setting: every value a player might
// plausibly type is kept as typed. Validation only, never a floor - a
// configured 0.0 smoothing stays 0.0.
float ReadFloat(const cameraunlock::IniReader& ini, const char* section, const char* key,
                float fallback, float lo, float hi) {
    return guards::ReadFloatChecked(ini, section, key, fallback, lo, hi,
                                    &cameraunlock::logging::Line);
}

// Sensitivity: a negative multiplier is a legitimate way to invert an axis
// without touching the Invert flags, so only the magnitude is bounded.
constexpr float kMaxSens = guards::kMaxSensitivity;

// Travel limits are metres, and the floor is 0 rather than -kMaxPositionLimit:
// a negative limit inverts the clamp in PositionProcessor - Clamp(v, -limit,
// limit) with limit < 0 returns the lower bound for every input - which pins
// the lean at a fixed offset instead of freeing it.
constexpr float kMaxLimit = guards::kMaxPositionLimit;

// Vertical degrees. Zero is not a bad value here, it is the off switch, so the
// read admits [0, kMaxFov] and the playable floor is applied after it. The
// upper bound is the projection's: the field is multiplied into the camera and
// divided back out as tan(fov/2) by the crosshair projection, and a field at or
// past a half turn has no tangent to project through.
constexpr float kMinFov = 30.0f;
constexpr float kMaxFov = 120.0f;

// IniReader::ReadBool matches the WHOLE value against a fixed list and hands
// back the default on anything else, silently. That is the same hazard the
// floats above are read through ReadFloatChecked for, and it bites harder here
// because the mod seeds no HeadTracking.ini: every user's file is hand-written,
// so "WorldSpaceYaw=true ; horizon locked" - a comment where the docs put one
// for a numeric key - reverts the setting to its default with nothing in the
// log the README tells the player to read. Same treatment as the numbers:
// strip the inline comment, accept the value whatever its casing, and report
// one that was present but not understood.
bool ReadBool(const cameraunlock::IniReader& ini, const char* section, const char* key,
              bool fallback) {
    const std::string raw = guards::ReadRawValue(ini, section, key);
    if (raw.empty()) return fallback;

    std::string lowered = raw;
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") return true;
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") return false;

    cameraunlock::logging::Line(
        "config: [%s] %s=%s is not true or false, so the default %s is used instead.",
        section, key, raw.c_str(), fallback ? "true" : "false");
    return fallback;
}

// A virtual key GetAsyncKeyState can never report is a hotkey that silently
// does nothing: ToggleKey=0x230 registers and is polled forever without ever
// firing, and the user cannot tell that from a broken mod.
//
// The token is parsed whole, for the same reason every number above is.
// IniReader::ReadHex is a strtol PREFIX parse, and a key NAME is the expected
// user error here: "ToggleKey=Delete" reads as 0xDE and binds the apostrophe
// key, "ToggleKey=End" reads as 0x0E and binds a code GetAsyncKeyState never
// reports - which is verbatim the case this function exists to catch. Neither
// writes anything to the log.
int ReadHotkey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const std::string raw = guards::ReadRawValue(ini, "Hotkeys", key);
    if (raw.empty()) return fallback;

    const char* text = raw.c_str();
    if (raw.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;

    char* end = nullptr;
    const long vk = std::strtol(text, &end, 16);
    const bool wholeToken = end != nullptr && *end == '\0' && end != text;
    if (wholeToken && vk >= 0 && vk <= 0xFF &&
        guards::IsBindableVirtualKey(static_cast<int>(vk))) {
        return static_cast<int>(vk);
    }

    cameraunlock::logging::Line(
        "config: [Hotkeys] %s=%s is not a virtual key code that can be polled (write it as "
        "hex, GetAsyncKeyState defines 0x01-0xFE, and Ctrl/Shift/Alt are reserved for the "
        "chord bindings), using 0x%X",
        key, raw.c_str(), fallback);
    return fallback;
}

// IniReader::Open existence-checks the path with GetFileAttributesA, which
// resolves a relative path against the working directory - but every read that
// follows goes through GetPrivateProfileStringA, which searches the WINDOWS
// directory for anything that is not fully qualified. A relative path therefore
// opens one file and reads another: Load reports success and every key comes
// back as its default, or worse, comes back out of a C:\Windows\HeadTracking.ini
// that anything on the machine could have put there. Neither is distinguishable
// from a config that was simply never edited.
//
// The null check is the same boundary and not a courtesy: Open takes a
// std::string by reference, so a null path is constructed into one and takes
// the game down with it.
bool IsAbsolutePath(const char* path) {
    if (path == nullptr) return false;
    const std::string p(path);
#ifdef _WIN32
    // Drive-qualified, or UNC. A rooted-but-driveless "\\HeadTracking.ini" is
    // not fully qualified as far as the profile API is concerned.
    if (p.size() >= 3 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':' &&
        (p[2] == '\\' || p[2] == '/')) {
        return true;
    }
    return p.size() >= 2 && (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/');
#else
    return !p.empty() && p[0] == '/';
#endif
}

}  // namespace

bool Config::Load(const char* path) {
    if (!IsAbsolutePath(path)) {
        cameraunlock::logging::Line(
            "config: '%s' is not an absolute path, so the file that was found would not "
            "be the file that is read. Using defaults.",
            path == nullptr ? "(null)" : path);
        return false;
    }

    cameraunlock::IniReader ini;
    if (!ini.Open(path)) return false;

    // ReadInt is the one reader that does not honour its default on a
    // present-but-unparseable value - it hands back 0 - and a receiver bound to
    // port 0 takes an ephemeral port no tracker is sending to.
    const int rawPort = ini.ReadInt("Network", "UDPPort", udpPort);
    bool portValid = false;
    const uint16_t port =
        cameraunlock::NormalizeUdpPort(rawPort, static_cast<uint16_t>(udpPort), portValid);
    if (!portValid) {
        cameraunlock::logging::Line(
            "config: [Network] UDPPort=%d is outside 1024-65535, using %u", rawPort, port);
    }
    udpPort = port;

    yawSens   = ReadFloat(ini, "Sensitivity", "YawMultiplier",   yawSens,   -kMaxSens, kMaxSens);
    pitchSens = ReadFloat(ini, "Sensitivity", "PitchMultiplier", pitchSens, -kMaxSens, kMaxSens);
    rollSens  = ReadFloat(ini, "Sensitivity", "RollMultiplier",  rollSens,  -kMaxSens, kMaxSens);
    invertYaw   = ReadBool(ini, "Sensitivity", "InvertYaw", invertYaw);
    invertPitch = ReadBool(ini, "Sensitivity", "InvertPitch", invertPitch);
    invertRoll  = ReadBool(ini, "Sensitivity", "InvertRoll", invertRoll);
    // Each key falls back to its own default (local 0.0, remote 0.15), never to
    // a shared one: a bad RemoteSmoothing dropping to the local default would
    // leave a phone's network jitter entirely unsmoothed.
    localSmoothing  = ReadFloat(ini, "Sensitivity", "LocalSmoothing",  localSmoothing,  0.0f, 1.0f);
    remoteSmoothing = ReadFloat(ini, "Sensitivity", "RemoteSmoothing", remoteSmoothing, 0.0f, 1.0f);

    positionEnabled = ReadBool(ini, "Position", "Enabled", positionEnabled);
    posSensX = ReadFloat(ini, "Position", "SensitivityX", posSensX, -kMaxSens, kMaxSens);
    posSensY = ReadFloat(ini, "Position", "SensitivityY", posSensY, -kMaxSens, kMaxSens);
    posSensZ = ReadFloat(ini, "Position", "SensitivityZ", posSensZ, -kMaxSens, kMaxSens);
    limitX     = ReadFloat(ini, "Position", "LimitX",     limitX,     0.0f, kMaxLimit);
    limitY     = ReadFloat(ini, "Position", "LimitY",     limitY,     0.0f, kMaxLimit);
    limitZ     = ReadFloat(ini, "Position", "LimitZ",     limitZ,     0.0f, kMaxLimit);
    limitZBack = ReadFloat(ini, "Position", "LimitZBack", limitZBack, 0.0f, kMaxLimit);

    crosshairCompensation = ReadBool(ini, "Crosshair", "Compensate", crosshairCompensation);

    suppressEyeTracking = ReadBool(ini, "Camera", "SuppressEyeTracking", suppressEyeTracking);

    fieldOfView = ReadFloat(ini, "Camera", "FieldOfView", fieldOfView, 0.0f, kMaxFov);
    if (fieldOfView > 0.0f && fieldOfView < kMinFov) {
        cameraunlock::logging::Line(
            "config: [Camera] FieldOfView=%g is below %.0f vertical degrees, clamped to %.0f",
            static_cast<double>(fieldOfView), kMinFov, kMinFov);
        fieldOfView = kMinFov;
    }

    toggleKey       = ReadHotkey(ini, "ToggleKey", toggleKey);
    trackingModeKey = ReadHotkey(ini, "TrackingModeKey", trackingModeKey);
    yawModeKey      = ReadHotkey(ini, "YawModeKey", yawModeKey);

    worldSpaceYaw = ReadBool(ini, "General", "WorldSpaceYaw", worldSpaceYaw);
    autoEnable    = ReadBool(ini, "General", "AutoEnable", autoEnable);

    return true;
}

}  // namespace SomaHT
