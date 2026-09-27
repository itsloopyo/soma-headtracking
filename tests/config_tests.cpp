// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// CameraUnlock.ini: the committed file is the table's fresh render, the defaults the dev build ran
// on map to the defaults, a first start creates the committed file, the mode and yaw hotkeys'
// saves change only their own lines and leave Defaults.ini and HeadTracking.ini alone, End's row
// cannot be saved, the removed settings are dropped and logged, the field of view keeps the
// values the dev build could run on, and a folder the ANSI code page cannot name imports as the
// dev build read it.
//
// `--render-config <path>` writes the committed file instead (pixi run render-config).

#include "config.h"

#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace SomaHT;

namespace {

namespace cfg = cameraunlock::config;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// The file a first start creates.
std::string Rendered() {
    return cfg::RenderCanonicalFresh(MakeConfigTable(), {kConfigDisplayName});
}

std::string Committed() {
    return ReadBytes(fs::path(SOMA_COMMITTED_CONFIG));
}

void TestCommittedConfigIsRendered() {
    Check(Committed() == Rendered(), "config/CameraUnlock.ini is the table's fresh render (pixi run render-config)");
}

// A scratch game folder, and a Defaults.ini of its own beside it that the first load creates.
struct Scratch {
    fs::path root;
    fs::path game;
    fs::path defaults;

    explicit Scratch(const std::wstring& gameFolder) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        root = fs::path(temp) / (L"soma-config-tests-" + std::to_wstring(GetCurrentProcessId()));
        game = root / gameFolder;
        fs::remove_all(game);
        fs::create_directories(game);
        fs::create_directories(root / L"user");
        defaults = root / L"user" / L"CameraUnlock" / L"Defaults.ini";
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    cfg::ConfigOwnerOptions<Config> Options() const {
        return MakeConfigOwnerOptions(game.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
    }
    fs::path ConfigPath() const { return game / kConfigFileName; }
    fs::path LegacyPath() const { return game / kLegacyConfigFileName; }
};

std::vector<std::string> Listing(const fs::path& dir) {
    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

// With neither file there, the first start creates CameraUnlock.ini as the committed file and
// Defaults.ini with the built-in values, and no HeadTracking.ini.
void TestFirstStartCreatesTheCommittedFile() {
    const Scratch s(L"first-start");
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Created, "a first start with no file is Created");
    Check(ReadBytes(s.ConfigPath()) == Committed(), "a first start creates config/CameraUnlock.ini's bytes");
    Check(Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini"}, "a first start creates CameraUnlock.ini and nothing else");
    Check(fs::exists(s.defaults), "a first start creates Defaults.ini");
    Check(AllValues(loaded.config) == AllValues(MakeConfigTable().defaults()), "a first start runs on the built-in values");
}

cfg::ImportResult MapLegacy(const std::string& bytes, Config& mapped) {
    const Scratch s(L"map");
    WriteBytes(s.LegacyPath(), bytes);
    mapped = MakeConfigTable().defaults();
    return MakeLegacyImport().run(cfg::LegacyInput{s.LegacyPath().wstring(), s.LegacyPath().string(), false}, mapped);
}

bool Dropped(const cfg::ImportResult& result, cfg::DropRule rule, const char* section, const char* key) {
    for (const cfg::DroppedValue& d : result.dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return true;
    }
    return false;
}

// A fresh install and an upgrade from the dev build's defaults start the same: the map of the
// frozen defaults holds every row at the table's default, leaves every concept row to
// Defaults.ini, drops nothing, and every pose-shaping value is the shipped identity, which the
// code now applies.
void TestLegacyDefaultsMapToTheDefaults() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const fs::path missing = fs::path(temp) / L"soma-no-such-folder" / kLegacyConfigFileName;
    const auto table = MakeConfigTable();
    Config mapped = table.defaults();
    const cfg::ImportResult result =
        MakeLegacyImport().run(cfg::LegacyInput{missing.wstring(), missing.string(), false}, mapped);
    Check(result.status == cfg::ImportStatus::Absent, "no file imports as Absent");
    Check(result.dropped.empty(), "the dev build's defaults drop nothing");
    Check(result.pose_shaping.size() == 9, "every sensitivity and inversion is recorded");
    Check(result.follows_defaults_ini.size() == 15, "every one of the 15 concept rows follows Defaults.ini");
    for (const cfg::PoseShapingValue& value : result.pose_shaping) {
        Check(value.folded, "[" + value.section + "] " + value.key + " at its shipped value is folded");
    }
    Check(AllValues(mapped) == AllValues(table.defaults()), "the dev build's defaults map to the defaults");
    Check(mapped.toggle_key_name == "End, Ctrl+Shift+Y" && mapped.cycle_tracking_mode_key_name == "PageUp, Ctrl+Shift+G" &&
              mapped.yaw_mode_key_name == "PageDown, Ctrl+Shift+H",
          "the old hotkeys and their chords become the fleet's key lists");
}

// The settings the canonical format has no row for are dropped and recorded; the values that
// remain settings are carried.
void TestRemovedSettingsAreDropped() {
    Config mapped;
    const cfg::ImportResult result = MapLegacy(
        "[Position]\r\nEnabled=false\r\nSensitivityZ=2.0\r\n[Crosshair]\r\nCompensate=false\r\n"
        "[Sensitivity]\r\nInvertPitch=true\r\n[Camera]\r\nSuppressEyeTracking=false\r\nFieldOfView=90\r\n"
        "[General]\r\nWorldSpaceYaw=false\r\n[Hotkeys]\r\nYawModeKey=0x77\r\n",
        mapped);
    Check(result.status == cfg::ImportStatus::Imported, "the file imports");
    Check(mapped.rotation_enabled && !mapped.position_enabled, "[Position] Enabled=false imports as rotation only");
    Check(Dropped(result, cfg::DropRule::PositionSwitchOff, "Position", "Enabled"), "the position switch is recorded");
    Check(Dropped(result, cfg::DropRule::Reticle, "Crosshair", "Compensate"), "Compensate=false is dropped as a reticle setting");
    Check(Dropped(result, cfg::DropRule::PoseShaping, "Position", "SensitivityZ"), "a changed position sensitivity is dropped");
    Check(Dropped(result, cfg::DropRule::PoseShaping, "Sensitivity", "InvertPitch"), "a changed inversion is dropped");
    Check(result.dropped.size() == 4, "nothing else is dropped");
    Check(!mapped.suppress_eye_tracking && mapped.field_of_view == 90.0f && !mapped.world_space_yaw,
          "the settings that remain are carried");
    Check(mapped.yaw_mode_key_name == "F8, Ctrl+Shift+H", "a changed hotkey keeps its chord");
}

// The field of view reads 0 and 30 to 120, the values the dev build could run on, and nothing
// else.
void TestFieldOfViewRange() {
    for (const auto& [text, ok] : std::vector<std::pair<const char*, bool>>{
             {"0", true}, {"30", true}, {"75.5", true}, {"120", true}, {"29.9", false}, {"-1", false}, {"121", false}}) {
        const Scratch s(L"fov");
        std::string bytes = Committed();
        const std::string from = "\r\nFieldOfView=0.0\r\n";
        const size_t at = bytes.find(from);
        if (at == std::string::npos) throw std::runtime_error("the committed file has no FieldOfView=0.0 line");
        bytes.replace(at, from.size(), std::string("\r\nFieldOfView=") + text + "\r\n");
        WriteBytes(s.ConfigPath(), bytes);
        const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
        Check(loaded.diagnostics.empty() == ok, std::string("FieldOfView=") + text + (ok ? " reads" : " is refused"));
        if (!ok) Check(loaded.config.field_of_view == 0.0f, std::string("a refused FieldOfView=") + text + " runs on the game's own field");
    }
}

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < bytes.size()) {
        const size_t end = bytes.find("\r\n", start);
        lines.push_back(bytes.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

// The lines of `after` that differ from `before`, which must have as many lines.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before);
    const std::vector<std::string> b = Lines(after);
    if (a.size() != b.size()) return {"a line was added or removed"};
    std::vector<std::string> changed;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A save changes the lines of its rows and no other byte, writes a value over default, and
// touches neither Defaults.ini nor HeadTracking.ini; the tracking mode and the yaw mode persist,
// and End's row cannot be saved at all.
void TestTogglesSave() {
    const Scratch s(L"save");
    const std::string committed = Committed();
    const std::string legacyBytes = "[General]\r\nAutoEnable=false\r\n";
    WriteBytes(s.ConfigPath(), committed);
    WriteBytes(s.LegacyPath(), legacyBytes);

    {
        cfg::ConfigOwner<Config> owner(s.Options());
        const auto loaded = owner.Load();
        Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
        Check(loaded.config.enable_on_startup, "HeadTracking.ini is not read while CameraUnlock.ini exists");
        Check(Contains(loaded.log, "is left as it was and is not read"),
              "the log says HeadTracking.ini is not read while CameraUnlock.ini exists");
        const std::string defaultsBefore = ReadBytes(s.defaults);

        const auto rotationOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::RotationOnly);
        const cfg::ConfigSaveResult first = owner.Save([rotationOnly](Config& c) {
            c.rotation_enabled = rotationOnly.rotation_enabled;
            c.position_enabled = rotationOnly.position_enabled;
        });
        Check(first.status == cfg::ConfigSaveStatus::Saved, "the tracking mode saves");
        Check(Contains(first.log, "no longer follows Defaults.ini"),
              "the mode save says the pair stopped following Defaults.ini");
        const std::string afterRotationOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(committed, afterRotationOnly) == std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"},
              "saving rotation only writes the mode pair over default and changes nothing else");

        const auto positionOnly = cameraunlock::EncodeTrackingMode(cameraunlock::TrackingMode::PositionOnly);
        Check(owner.Save([positionOnly](Config& c) {
                  c.rotation_enabled = positionOnly.rotation_enabled;
                  c.position_enabled = positionOnly.position_enabled;
              }).status == cfg::ConfigSaveStatus::Saved,
              "the third tracking mode saves");
        const std::string afterPositionOnly = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterRotationOnly, afterPositionOnly) ==
                  std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"},
              "saving position only changes the mode pair and nothing else");

        Check(owner.Save([](Config& c) { c.world_space_yaw = false; }).status == cfg::ConfigSaveStatus::Saved,
              "the yaw mode saves");
        const std::string afterYaw = ReadBytes(s.ConfigPath());
        Check(ChangedLines(afterPositionOnly, afterYaw) == std::vector<std::string>{"WorldSpaceYaw=false"},
              "saving the yaw mode writes WorldSpaceYaw over default and changes nothing else");

        Check(owner.Save([](Config&) {}).status == cfg::ConfigSaveStatus::Saved, "an empty save succeeds");
        Check(ReadBytes(s.ConfigPath()) == afterYaw, "an empty save writes nothing");

        bool refused = false;
        try {
            owner.Save([](Config& c) { c.enable_on_startup = false; });
        } catch (const std::logic_error&) {
            refused = true;
        }
        Check(refused, "EnableOnStartup is not Writable, so the End toggle cannot persist");
        Check(ReadBytes(s.ConfigPath()) == afterYaw, "a refused save writes nothing");

        Check(ReadBytes(s.defaults) == defaultsBefore, "saving leaves Defaults.ini as it was");
        Check(ReadBytes(s.LegacyPath()) == legacyBytes, "saving leaves HeadTracking.ini as it was");
    }

    const auto again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical && again.diagnostics.empty() &&
              !again.config.rotation_enabled && again.config.position_enabled && !again.config.world_space_yaw &&
              again.config.enable_on_startup,
          "the saved tracking mode and yaw mode come back at the next start");
    Check((Listing(s.game) == std::vector<std::string>{"CameraUnlock.ini", kLegacyConfigFileName}),
          "the game folder holds CameraUnlock.ini and HeadTracking.ini and nothing else");
}

// The dev build built its path from the ANSI form of the game's folder, and in a folder the ANSI
// code page cannot name it read no file and ran on its defaults. The import does the same, so such
// a player migrates to the defaults, and HeadTracking.ini stays as it was.
void TestAFolderTheCodepageCannotNameImportsAsTheDevBuildReadIt() {
    const Scratch s(L"soma-\x4E2D");
    const std::string legacyBytes = "[Network]\r\nUDPPort=5000\r\n";
    WriteBytes(s.LegacyPath(), legacyBytes);
    const auto loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    if (GetACP() == CP_UTF8) {
        std::printf("note: the ANSI code page is UTF-8 here, so the folder has an ANSI name\n");
        Check(loaded.status == cfg::ConfigLoadStatus::Migrated && loaded.config.udp_port == 5000,
              "with a UTF-8 code page the file is read");
        return;
    }
    Check(loaded.status == cfg::ConfigLoadStatus::Migrated, "a file the dev build could not find migrates to its defaults");
    Check(loaded.config.udp_port == 4242, "the port is the dev build's default, as that build ran");
    Check(ReadBytes(s.LegacyPath()) == legacyBytes, "HeadTracking.ini stays as it was");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
            WriteBytes(argv[2], Rendered());
            return 0;
        }
        if (argc != 1) {
            std::printf("usage: %s [--render-config <path>]\n", argv[0]);
            return 2;
        }

        TestCommittedConfigIsRendered();
        TestLegacyDefaultsMapToTheDefaults();
        TestRemovedSettingsAreDropped();
        TestFieldOfViewRange();
        TestFirstStartCreatesTheCommittedFile();
        TestTogglesSave();
        TestAFolderTheCodepageCannotNameImportsAsTheDevBuildReadIt();
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }

    if (g_failures == 0) {
        std::printf("config tests: all passed\n");
        return 0;
    }
    std::printf("config tests: %d failure(s)\n", g_failures);
    return 1;
}
