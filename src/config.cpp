// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SomaHT {

namespace {

namespace cfg = cameraunlock::config;
using cfg::DroppedValue;
using cfg::DropRule;
using cfg::ImportResult;
using cfg::LegacyFollowsDefaultsIni;
using cfg::LegacyInput;
using cfg::LegacyPoseShaping;
using cfg::PoseShapingValue;
using cfg::schema::Concept;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// 0, the game's own field of view, or a chosen one from kMinFieldOfView to kMaxFieldOfView, the
// values the dev build could run on.
class FieldOfViewCodec {
public:
    using Value = float;
    cfg::CodecParseResult<float> Parse(std::string_view text) const {
        cfg::CodecParseResult<float> read = inner_.Parse(text);
        if (!read.ok() || (read.value != 0.0f && read.value < kMinFieldOfView)) {
            return {0.0f, "0, or a number from 30 to 120"};
        }
        return read;
    }
    std::string Render(float value) const { return inner_.Render(value); }
    bool Equal(float a, float b) const { return inner_.Equal(a, b); }

private:
    cfg::FloatCodec inner_{0.0f, kMaxFieldOfView};
};

// A legacy hotkey code and the Ctrl+Shift chord the dev build always registered beside it, as one
// key list: the code's binding (none for a code no hotkey can hold, N1 and N3), then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    const std::string code = cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    const std::string chord = cameraunlock::input::FormatKeyBindings(
        std::vector<KeyBinding>{{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // The dev build opened the file by the ANSI path of the game's folder, and where the ANSI code
    // page could not name that folder it read no file and ran on its defaults.
    legacy::Config c;
    const legacy::ReadStatus read =
        input.ansi_lossy ? legacy::ReadStatus::Absent : legacy::Read(input.ansi_path.c_str(), c);
    const legacy::Config shipped;

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    // The dev build refused a port outside 1024-65535, so every value it ran on fits the row.
    out.udp_port = static_cast<uint16_t>(c.udpPort);
    out.enable_on_startup = c.autoEnable;

    // [Position] Enabled=false parked the session in rotation only and kept the mode hotkey off
    // the position modes. It imports as the rotation-only mode, which the hotkey can now leave.
    out.rotation_enabled = true;
    out.position_enabled = true;
    cfg::LegacyPositionSwitch(c.positionEnabled, "Position", "Enabled", out.rotation_enabled, out.position_enabled,
                              dropped);

    // The legacy reader already held both to [0, 1] and replaced a non-finite value.
    out.local_smoothing = c.localSmoothing;
    out.remote_smoothing = c.remoteSmoothing;

    // The legacy reader held each limit to [0, 10] and replaced a non-finite one. The old file had
    // one vertical limit, which the old runtime applied both ways.
    out.limit_x = c.limitX;
    out.limit_y = c.limitY;
    out.limit_y_down = c.limitY;
    out.limit_z = c.limitZ;
    out.limit_z_back = c.limitZBack;

    out.world_space_yaw = c.worldSpaceYaw;
    out.suppress_eye_tracking = c.suppressEyeTracking;
    // The legacy reader already clamped a field between 0 and 30 up to 30.
    out.field_of_view = c.fieldOfView;

    // Every sensitivity and inversion shipped at identity, so nothing folds and a value the player
    // changed is dropped.
    LegacyPoseShaping(c.yawSens, shipped.yawSens, "Sensitivity", "YawMultiplier", shaping, dropped);
    LegacyPoseShaping(c.pitchSens, shipped.pitchSens, "Sensitivity", "PitchMultiplier", shaping, dropped);
    LegacyPoseShaping(c.rollSens, shipped.rollSens, "Sensitivity", "RollMultiplier", shaping, dropped);
    LegacyPoseShaping(c.invertYaw, shipped.invertYaw, "Sensitivity", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invertPitch, shipped.invertPitch, "Sensitivity", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invertRoll, shipped.invertRoll, "Sensitivity", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.posSensX, shipped.posSensX, "Position", "SensitivityX", shaping, dropped);
    LegacyPoseShaping(c.posSensY, shipped.posSensY, "Position", "SensitivityY", shaping, dropped);
    LegacyPoseShaping(c.posSensZ, shipped.posSensZ, "Position", "SensitivityZ", shaping, dropped);

    // The crosshair always follows the aim now.
    if (!c.crosshairCompensation) dropped.push_back({DropRule::Reticle, "Crosshair", "Compensate", "false"});

    out.toggle_key_name = KeyList(c.toggleKey, 'Y', "ToggleKey", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.trackingModeKey, 'G', "TrackingModeKey", dropped);
    out.yaw_mode_key_name = KeyList(c.yawModeKey, 'H', "YawModeKey", dropped);

    // A row still at what the dev build ran on with no file is no player's choice, so it follows
    // Defaults.ini. The chords were fixed in code, so each hotkey's code decides alone.
    LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.udpPort, shipped.udpPort);
    follows.Setting(Concept::EnableOnStartup, c.autoEnable, shipped.autoEnable);
    follows.TrackingMode(c.positionEnabled, shipped.positionEnabled);
    follows.Setting(Concept::LocalSmoothing, c.localSmoothing, shipped.localSmoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remoteSmoothing, shipped.remoteSmoothing);
    follows.Setting(Concept::PositionLimitX, c.limitX, shipped.limitX);
    follows.Setting(Concept::PositionLimitY, c.limitY, shipped.limitY);
    follows.Setting(Concept::PositionLimitYDown, c.limitY, shipped.limitY);
    follows.Setting(Concept::PositionLimitZ, c.limitZ, shipped.limitZ);
    follows.Setting(Concept::PositionLimitZBack, c.limitZBack, shipped.limitZBack);
    follows.Setting(Concept::WorldSpaceYaw, c.worldSpaceYaw, shipped.worldSpaceYaw);
    follows.Setting(Concept::ToggleKey, c.toggleKey, shipped.toggleKey);
    follows.Setting(Concept::CycleTrackingModeKey, c.trackingModeKey, shipped.trackingModeKey);
    follows.Setting(Concept::YawModeKey, c.yawModeKey, shipped.yawModeKey);

    return read == legacy::ReadStatus::Absent
               ? ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
}

}  // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    cfg::ConfigTable<Config> table{Config{}};
    table.Concept<Concept::UdpPort>(&Config::udp_port)
        .Concept<Concept::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<Concept::RotationEnabled>(&Config::rotation_enabled)
        .Writable()
        .Concept<Concept::WorldSpaceYaw>(&Config::world_space_yaw)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::local_smoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remote_smoothing)
        .Concept<Concept::PositionEnabled>(&Config::position_enabled)
        .Writable()
        .Concept<Concept::PositionLimitX>(&Config::limit_x)
        .Concept<Concept::PositionLimitY>(&Config::limit_y)
        .Concept<Concept::PositionLimitYDown>(&Config::limit_y_down)
        .Concept<Concept::PositionLimitZ>(&Config::limit_z)
        .Concept<Concept::PositionLimitZBack>(&Config::limit_z_back)
        .Concept<Concept::ToggleKey>(&Config::toggle_key_name)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key_name)
        .Concept<Concept::YawModeKey>(&Config::yaw_mode_key_name);
    table.Local("Camera", "SuppressEyeTracking", &Config::suppress_eye_tracking, cfg::BoolCodec(),
                "true: while head tracking is on, SOMA is told its Tobii eye tracker is not\n"
                "tracking, so Extended View and the gaze crosshair do not fight the head pose.\n"
                "Turning head tracking off hands them back.");
    table.Local("Camera", "FieldOfView", &Config::field_of_view, FieldOfViewCodec(),
                "Vertical field of view in degrees. SOMA has no setting of its own. 0 leaves\n"
                "the game's own alone. 30 to 120 can be set, and the game's scripted zooms still\n"
                "narrow and widen the view from it.");
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace SomaHT
