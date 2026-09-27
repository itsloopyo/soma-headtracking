// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read three ways:
//
//   oracle     the reader of the dev pre-release (7d8e2c8), the only published build, with the
//              core sources it compiled at its pin bd22895 (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//   migration  the config owner in a folder holding only HeadTracking.ini, the legacy file,
//              importing it into a new CameraUnlock.ini, then the canonical reader and table on
//              that file
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Comparison 2, import against migration, is the proof for the migration: the settings the mod
// starts on are the import's, apart from the approved changes, each of which the import must
// record as dropped. A sensitivity or inversion the player set away from its shipped identity is
// dropped (pose_shaping), [Crosshair] Compensate=false is dropped because the crosshair always
// follows the aim (reticle), and [Position] Enabled=false imports as the rotation-only mode, which
// the mode hotkey can now leave (position_switch_off). The dev build's reader already refused a
// port outside 1024-65535, a number outside its range or not finite, and a hotkey code outside
// 0x01-0xFE or on a modifier key, so every file it read migrates and the hotkeys fire exactly as
// they did.
//
// A row the player never changed from what the dev build ran on with no file follows Defaults.ini:
// the import lists it in follows_defaults_ini and the migration writes it default, the tracking
// mode pair as one unit. The test derives that list from what the import read and holds the
// import's list to it on every input; the documented file and the empty file list every row and
// migrate to the committed file byte for byte.
//
// Comparison 2 runs twice, once over a Defaults.ini at the built-in values, where the session runs
// as the import read, and once over one a player changed, where a row the player never changed
// takes Defaults.ini's value and a changed row keeps the player's. After every load
// HeadTracking.ini keeps its bytes, its write time and its attributes, Defaults.ini is never
// written, and the folder holds the legacy file and CameraUnlock.ini and nothing else. The next
// load reads CameraUnlock.ini, imports nothing and writes nothing, and a read-only legacy file
// imports as a writable one does.
//
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// Inputs: no file, an empty file, the HeadTracking.ini the dev build's README documented (the
// build shipped no config, seeded none and wrote none, so a player's file is one they wrote,
// most likely from that example), a hotkey on each of Ctrl, Shift and Alt, a non-finite number,
// and core's corpus over the documented file.

#include "config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace SomaHT;

namespace {

// The dev build and the frozen import read the file with the same source (src/config.cpp did not
// change between the dev tag and the commit that froze it) and the same core IniReader and value
// guards (unchanged since bd22895), so comparison 1 has no differences to record.
const char* const kComparison1Differences[] = {
    "none",
};

constexpr const char* kFileName = "HeadTracking.ini";

int g_failures = 0;
int g_checks = 0;

void Check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        if (g_failures < 200) std::printf("  FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("could not write " + path.string());
}

void SetReadOnly(const fs::path& path, bool readOnly) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("no attributes for " + path.string());
    const DWORD next = readOnly ? (attrs | FILE_ATTRIBUTE_READONLY) : (attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!SetFileAttributesW(path.c_str(), next)) throw std::runtime_error("could not set attributes on " + path.string());
}

using Listing = std::vector<std::pair<std::string, std::string>>;

Listing List(const fs::path& dir) {
    Listing l;
    for (const auto& e : fs::directory_iterator(dir)) {
        l.emplace_back(e.path().filename().string(), ReadBytes(e.path()));
    }
    std::sort(l.begin(), l.end());
    return l;
}

struct Input {
    std::string name;
    std::optional<std::string> bytes;  // nullopt: no file
};

// Startup state as the dev build's Mod::Initialize derives it from the config: enabled from
// AutoEnable (and the camera hook, which is no config's), RotationOnly when [Position] Enabled is
// false and the session's RotationAndPosition otherwise, and the yaw mode from WorldSpaceYaw.
struct Startup {
    bool enabled;
    int mode;  // cameraunlock::TrackingMode: 0 rotation and position, 1 rotation only
    bool worldSpaceYaw;
    bool operator==(const Startup& o) const {
        return enabled == o.enabled && mode == o.mode && worldSpaceYaw == o.worldSpaceYaw;
    }
};

Startup StartupOf(const soma_oracle_view::OracleConfig& c) {
    return {c.autoEnable, c.positionEnabled ? 0 : 1, c.worldSpaceYaw};
}

Startup StartupOf(const legacy::Config& c) { return {c.autoEnable, c.positionEnabled ? 0 : 1, c.worldSpaceYaw}; }

// The first press whose fired actions differ, for the failure message.
std::string FirstFireDifference(const soma_oracle_view::FireTable& expected, const soma_oracle_view::FireTable& got) {
    for (std::size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        if (expected[i] != got[i]) {
            char text[160];
            std::snprintf(text, sizeof text, "key 0x%02X held %d fires %d/%d/%d, not %d/%d/%d",
                          static_cast<int>(i / soma_oracle_view::kHeldStates) + soma_oracle_view::kFirstKey,
                          static_cast<int>(i % soma_oracle_view::kHeldStates), got[i][0], got[i][1], got[i][2],
                          expected[i][0], expected[i][1], expected[i][2]);
            return text;
        }
    }
    return expected.size() == got.size() ? "none" : "the tables differ in size";
}

// Every field the import reads, against the oracle's field of the same name.
std::vector<std::string> FieldDifferences(const soma_oracle_view::OracleConfig& o, const legacy::Config& i) {
    std::vector<std::string> d;
    auto b = [&d](const char* n, bool x, bool y) { if (x != y) d.push_back(n); };
    auto f = [&d](const char* n, float x, float y) { if (!SameBits(x, y)) d.push_back(n); };
    auto n = [&d](const char* name, long long x, long long y) { if (x != y) d.push_back(name); };
    n("udpPort", o.udpPort, i.udpPort);
    f("yawSens", o.yawSens, i.yawSens);
    f("pitchSens", o.pitchSens, i.pitchSens);
    f("rollSens", o.rollSens, i.rollSens);
    b("invertYaw", o.invertYaw, i.invertYaw);
    b("invertPitch", o.invertPitch, i.invertPitch);
    b("invertRoll", o.invertRoll, i.invertRoll);
    f("localSmoothing", o.localSmoothing, i.localSmoothing);
    f("remoteSmoothing", o.remoteSmoothing, i.remoteSmoothing);
    b("positionEnabled", o.positionEnabled, i.positionEnabled);
    f("posSensX", o.posSensX, i.posSensX);
    f("posSensY", o.posSensY, i.posSensY);
    f("posSensZ", o.posSensZ, i.posSensZ);
    f("limitX", o.limitX, i.limitX);
    f("limitY", o.limitY, i.limitY);
    f("limitZ", o.limitZ, i.limitZ);
    f("limitZBack", o.limitZBack, i.limitZBack);
    b("crosshairCompensation", o.crosshairCompensation, i.crosshairCompensation);
    b("suppressEyeTracking", o.suppressEyeTracking, i.suppressEyeTracking);
    f("fieldOfView", o.fieldOfView, i.fieldOfView);
    b("worldSpaceYaw", o.worldSpaceYaw, i.worldSpaceYaw);
    n("toggleKey", o.toggleKey, i.toggleKey);
    n("trackingModeKey", o.trackingModeKey, i.trackingModeKey);
    n("yawModeKey", o.yawModeKey, i.yawModeKey);
    b("autoEnable", o.autoEnable, i.autoEnable);
    return d;
}

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// The corpus descriptor of every key the frozen reader reads.
std::vector<cameraunlock::config::testing::MutationKey> MutationKeys() {
    using cameraunlock::config::testing::MutationKey;
    auto plain = [](const char* s, const char* k, const char* alt, std::vector<std::string> oor = {}) {
        MutationKey m;
        m.section = s;
        m.key = k;
        m.alternate = alt;
        m.out_of_range = std::move(oor);
        return m;
    };
    auto hotkey = [](const char* k, const char* alt) {
        MutationKey m;
        m.section = "Hotkeys";
        m.key = k;
        m.alternate = alt;
        m.out_of_range = {"0x100"};
        m.hotkey = true;
        return m;
    };
    return {
        plain("Network", "UDPPort", "4243", {"1023", "65536"}),
        plain("Sensitivity", "YawMultiplier", "0.5", {"101"}),
        plain("Sensitivity", "PitchMultiplier", "0.5", {"101"}),
        plain("Sensitivity", "RollMultiplier", "0.5", {"101"}),
        plain("Sensitivity", "InvertYaw", "true"),
        plain("Sensitivity", "InvertPitch", "true"),
        plain("Sensitivity", "InvertRoll", "true"),
        plain("Sensitivity", "LocalSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Sensitivity", "RemoteSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Position", "Enabled", "false"),
        plain("Position", "SensitivityX", "0.5", {"101"}),
        plain("Position", "SensitivityY", "0.5", {"101"}),
        plain("Position", "SensitivityZ", "0.5", {"101"}),
        plain("Position", "LimitX", "0.5", {"-0.3", "11"}),
        plain("Position", "LimitY", "0.5", {"-0.2", "11"}),
        plain("Position", "LimitZ", "0.5", {"-0.4", "11"}),
        plain("Position", "LimitZBack", "0.2", {"-0.1", "11"}),
        plain("Crosshair", "Compensate", "false"),
        plain("Camera", "SuppressEyeTracking", "false"),
        plain("Camera", "FieldOfView", "75", {"-1", "121"}),
        hotkey("ToggleKey", "0x70"),
        hotkey("TrackingModeKey", "0x71"),
        hotkey("YawModeKey", "0x72"),
        plain("General", "WorldSpaceYaw", "false"),
        plain("General", "AutoEnable", "false"),
    };
}

// One folder per reading under a root of this process's own, emptied before each input so the
// test never holds more than one input's files.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("soma-config-differential-" + std::to_string(GetCurrentProcessId()));
        Remove(root_);
        fs::create_directories(root_);
    }
    ~Scratch() { Remove(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    // root/leaf, created empty.
    fs::path Clean(const std::string& leaf) {
        const fs::path dir = root_ / leaf;
        Remove(dir);
        fs::create_directories(dir);
        return dir;
    }

private:
    // Read-only files included, which remove_all will not delete.
    static void Remove(const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(dir, ec);
        if (ec) throw std::runtime_error("could not empty " + dir.string() + ": " + ec.message());
    }

    fs::path root_;
};

fs::path Place(const fs::path& dir, const Input& input) {
    const fs::path file = dir / kFileName;
    if (input.bytes) WriteBytes(file, *input.bytes);
    return file;
}

struct ImportRun {
    legacy::Config config;
    legacy::ReadStatus status = legacy::ReadStatus::Read;
};

// The import on a read-only copy of the input, which must leave its folder as it found it.
ImportRun RunImport(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.status = legacy::Read(file.string().c_str(), run.config);
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("oracle");
    const fs::path file = Place(dir, input);
    bool oracleLoaded = false;
    const soma_oracle_view::OracleConfig oracle = soma_oracle_view::RunOracle(file.string(), oracleLoaded);
    const ImportRun import = RunImport(scratch, input);

    Check(oracleLoaded == (import.status == legacy::ReadStatus::Read),
          input.name + ": the import's status is not the oracle's");
    Check(input.bytes.has_value() == (import.status == legacy::ReadStatus::Read),
          input.name + ": the import's status does not say whether there was a file");
    const std::vector<std::string> fields = FieldDifferences(oracle, import.config);
    Check(fields.empty(), input.name + ": fields differ: " + Join(fields));
    Check(StartupOf(oracle) == StartupOf(import.config), input.name + ": startup state differs");
    const soma_oracle_view::FireTable oracleFires =
        soma_oracle_view::OracleFires(oracle.toggleKey, oracle.trackingModeKey, oracle.yawModeKey);
    const soma_oracle_view::FireTable importFires = soma_oracle_view::OracleFires(
        import.config.toggleKey, import.config.trackingModeKey, import.config.yawModeKey);
    Check(oracleFires == importFires,
          input.name + ": hotkeys fire differently: " + FirstFireDifference(oracleFires, importFires));
    return import;
}

// ---------------------------------------------------------------------------
// Comparison 2
// ---------------------------------------------------------------------------

namespace cfg = cameraunlock::config;
using cfg::ConfigLoadStatus;
using cfg::DropRule;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::ImportStatus;
using cfg::schema::Concept;

cameraunlock::input::KeyModifiers g_currentHeld = cameraunlock::input::KeyModifiers::kNone;

cameraunlock::input::KeyModifiers CurrentHeld() { return g_currentHeld; }

cameraunlock::input::KeyModifiers ModifiersOf(int held) {
    using cameraunlock::input::KeyModifiers;
    KeyModifiers m = KeyModifiers::kNone;
    if ((held & 1) != 0) m = m | KeyModifiers::kCtrl;
    if ((held & 2) != 0) m = m | KeyModifiers::kShift;
    if ((held & 4) != 0) m = m | KeyModifiers::kAlt;
    return m;
}

// OracleFires' table for the current build. Mod::RegisterHotkeys parses each key list and hands it
// to RegisterKeyBindings, which puts one detail::GuardKey callback per distinct key on the poller,
// holding that key's bindings in list order. The same callbacks are built here with the held
// modifiers read from the test rather than the keyboard, since the poller keeps its callbacks to
// itself. Actions in the order toggle, cycle, yaw mode, as OracleFires counts them.
soma_oracle_view::FireTable CurrentFires(const Config& m) {
    using soma_oracle_view::kActions;
    using soma_oracle_view::kFirstKey;
    using soma_oracle_view::kHeldStates;
    using soma_oracle_view::kLastKey;
    std::array<int, kActions> fired{};
    std::vector<std::pair<int, std::function<void()>>> registered;
    const std::string* lists[kActions] = {&m.toggle_key_name, &m.cycle_tracking_mode_key_name, &m.yaw_mode_key_name};
    for (int action = 0; action < kActions; ++action) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*lists[action]);
        if (!parsed.ok()) throw std::logic_error("migrated hotkey list '" + *lists[action] + "' does not parse");
        std::vector<int> keys;
        std::vector<std::vector<cameraunlock::input::KeyModifiers>> modifiers;
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            const auto at = std::find(keys.begin(), keys.end(), b.vk);
            if (at == keys.end()) {
                keys.push_back(b.vk);
                modifiers.push_back({b.modifiers});
            } else {
                modifiers[static_cast<std::size_t>(at - keys.begin())].push_back(b.modifiers);
            }
        }
        for (std::size_t i = 0; i < keys.size(); ++i) {
            registered.emplace_back(keys[i], cameraunlock::input::detail::GuardKey(
                                                 std::move(modifiers[i]), [&fired, action] { ++fired[action]; },
                                                 &CurrentHeld));
        }
    }

    soma_oracle_view::FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            g_currentHeld = ModifiersOf(held);
            for (const auto& r : registered) {
                if (r.first == vk) r.second();
            }
            table.push_back(fired);
        }
    }
    g_currentHeld = cameraunlock::input::KeyModifiers::kNone;
    return table;
}

// A file as the test holds it to: its bytes, its last write time and its attributes.
struct FileStamp {
    std::string bytes;
    FILETIME written{};
    DWORD attributes = 0;

    bool operator==(const FileStamp& o) const {
        return bytes == o.bytes && CompareFileTime(&written, &o.written) == 0 && attributes == o.attributes;
    }
};

FileStamp Stamp(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("cannot stat " + path.string());
    }
    FileStamp s;
    s.bytes = ReadBytes(path);
    s.written = data.ftLastWriteTime;
    s.attributes = data.dwFileAttributes;
    return s;
}

// Where Defaults.ini is for each run of comparison 2: at the built-in values, which the first load
// creates, and with the values a player changed, written from it.
fs::path g_builtinDefaults;
fs::path g_alteredDefaults;

cfg::ConfigOwnerOptions<Config> OwnerOptions(const fs::path& dir, const fs::path& defaults) {
    return MakeConfigOwnerOptions(dir.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
}

// The import with its map, for the values it records.
ImportResult RunMappedImport(Scratch& scratch, const Input& input) {
    const fs::path file = Place(scratch.Clean("mapped"), input);
    Config out = MakeConfigTable().defaults();
    return MakeLegacyImport().run(cfg::LegacyInput{file.wstring(), file.string(), false}, out);
}

const DroppedValue* FindDrop(const std::vector<DroppedValue>& dropped, DropRule rule, const char* section,
                             const char* key) {
    for (const DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

struct Tally {
    std::string committed;
    std::set<std::string> migrated;
    struct Run {
        int created = 0;
        int imported = 0;
        // Migrated files holding at least one default row.
        int with_default_rows = 0;
        // Migrated files that differ from the committed file.
        int with_values = 0;
    } builtin, altered;
    int with_pose_shaping_dropped = 0;
    int with_position_switch_off = 0;
    int with_crosshair_off = 0;
    // Inputs that change a row from the dev build's default, and those among them that change the
    // mode.
    int touched = 0;
    int mode_touched = 0;
};

// Every concept row the table binds, each of which follows Defaults.ini.
const std::set<Concept>& AllRows() {
    static const std::set<Concept> all = {
        Concept::UdpPort,        Concept::EnableOnStartup,    Concept::RotationEnabled,    Concept::PositionEnabled,
        Concept::WorldSpaceYaw,  Concept::LocalSmoothing,     Concept::RemoteSmoothing,    Concept::PositionLimitX,
        Concept::PositionLimitY, Concept::PositionLimitYDown, Concept::PositionLimitZ,     Concept::PositionLimitZBack,
        Concept::ToggleKey,      Concept::CycleTrackingModeKey, Concept::YawModeKey,
    };
    return all;
}

// The rows the player never changed: each reads as the dev build ran on with no file. One LimitY
// gave both vertical rows, and [Position] Enabled gave the mode pair.
std::set<Concept> UntouchedRows(const legacy::Config& l) {
    const legacy::Config d;
    std::set<Concept> u;
    const auto row = [&u](bool same, std::initializer_list<Concept> ids) {
        if (same) u.insert(ids.begin(), ids.end());
    };
    row(l.udpPort == d.udpPort, {Concept::UdpPort});
    row(l.autoEnable == d.autoEnable, {Concept::EnableOnStartup});
    row(l.positionEnabled == d.positionEnabled, {Concept::RotationEnabled, Concept::PositionEnabled});
    row(l.worldSpaceYaw == d.worldSpaceYaw, {Concept::WorldSpaceYaw});
    row(l.localSmoothing == d.localSmoothing, {Concept::LocalSmoothing});
    row(l.remoteSmoothing == d.remoteSmoothing, {Concept::RemoteSmoothing});
    row(l.limitX == d.limitX, {Concept::PositionLimitX});
    row(l.limitY == d.limitY, {Concept::PositionLimitY, Concept::PositionLimitYDown});
    row(l.limitZ == d.limitZ, {Concept::PositionLimitZ});
    row(l.limitZBack == d.limitZBack, {Concept::PositionLimitZBack});
    row(l.toggleKey == d.toggleKey, {Concept::ToggleKey});
    row(l.trackingModeKey == d.trackingModeKey, {Concept::CycleTrackingModeKey});
    row(l.yawModeKey == d.yawModeKey, {Concept::YawModeKey});
    return u;
}

std::string Names(const std::set<Concept>& rows) {
    std::string text;
    for (const Concept row : rows) {
        text += (text.empty() ? "" : ", ") + std::string(cfg::schema::kConcepts[static_cast<std::size_t>(row)].name);
    }
    return text.empty() ? "none" : text;
}

// What the session runs on over the changed Defaults.ini (WriteAlteredDefaults), in the frozen
// reader's terms: the import's values, with each row the import left to Defaults.ini as that file
// gives it.
legacy::Config OverAlteredDefaults(legacy::Config l, const std::set<Concept>& follows) {
    const auto f = [&follows](Concept id) { return follows.count(id) != 0; };
    if (f(Concept::UdpPort)) l.udpPort = 4243;
    if (f(Concept::EnableOnStartup)) l.autoEnable = false;
    if (f(Concept::RotationEnabled)) l.positionEnabled = false;
    if (f(Concept::WorldSpaceYaw)) l.worldSpaceYaw = false;
    if (f(Concept::LocalSmoothing)) l.localSmoothing = 0.3f;
    if (f(Concept::RemoteSmoothing)) l.remoteSmoothing = 0.3f;
    if (f(Concept::PositionLimitX)) l.limitX = 0.5f;
    if (f(Concept::PositionLimitY)) l.limitY = 0.5f;
    if (f(Concept::PositionLimitZ)) l.limitZ = 0.5f;
    if (f(Concept::PositionLimitZBack)) l.limitZBack = 0.2f;
    if (f(Concept::ToggleKey)) l.toggleKey = 0x70;
    if (f(Concept::CycleTrackingModeKey)) l.trackingModeKey = 0x71;
    if (f(Concept::YawModeKey)) l.yawModeKey = 0x72;
    return l;
}

// The documented file and the empty file: neither holds a value the dev build did not run on with
// no file, so every row follows Defaults.ini and the migration gives the committed file.
bool IsUnedited(const std::string& name) { return name == "empty file" || name.rfind("documented-", 0) == 0; }

// Every pose-shaping value the frozen reader read is listed in its place, folded where it holds
// the identity the dev build shipped and dropped as PoseShaping where it does not; a position
// switch that was off is recorded as PositionSwitchOff and a crosshair compensation that was off
// as Reticle; and nothing is dropped by any other rule. The dev build's reader already refused a
// non-finite number, a code outside 0x01-0xFE and a modifier key, so N1, N2 and N3 never apply.
void CheckDrops(const std::string& name, const legacy::Config& l, const ImportResult& imported, Tally& tally) {
    const legacy::Config shipped;
    struct Read {
        const char* section;
        const char* key;
        bool atShipped;
    };
    const Read reads[] = {
        {"Sensitivity", "YawMultiplier", SameBits(l.yawSens, shipped.yawSens)},
        {"Sensitivity", "PitchMultiplier", SameBits(l.pitchSens, shipped.pitchSens)},
        {"Sensitivity", "RollMultiplier", SameBits(l.rollSens, shipped.rollSens)},
        {"Sensitivity", "InvertYaw", l.invertYaw == shipped.invertYaw},
        {"Sensitivity", "InvertPitch", l.invertPitch == shipped.invertPitch},
        {"Sensitivity", "InvertRoll", l.invertRoll == shipped.invertRoll},
        {"Position", "SensitivityX", SameBits(l.posSensX, shipped.posSensX)},
        {"Position", "SensitivityY", SameBits(l.posSensY, shipped.posSensY)},
        {"Position", "SensitivityZ", SameBits(l.posSensZ, shipped.posSensZ)},
    };
    Check(imported.pose_shaping.size() == std::size(reads),
          name + ": the import lists " + std::to_string(imported.pose_shaping.size()) + " pose-shaping values, not 9");
    if (imported.pose_shaping.size() != std::size(reads)) return;
    bool anyDropped = false;
    for (size_t k = 0; k < std::size(reads); ++k) {
        const cfg::PoseShapingValue& v = imported.pose_shaping[k];
        const std::string label = std::string("[") + reads[k].section + "] " + reads[k].key;
        Check(v.section == reads[k].section && v.key == reads[k].key, name + ": " + label + " is not listed in its place");
        Check(v.folded == reads[k].atShipped, name + ": " + label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
        const bool listed = FindDrop(imported.dropped, DropRule::PoseShaping, reads[k].section, reads[k].key) != nullptr;
        Check(listed != reads[k].atShipped,
              name + ": " + label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
        if (!reads[k].atShipped) anyDropped = true;
    }
    if (anyDropped) ++tally.with_pose_shaping_dropped;

    const bool switchOff = FindDrop(imported.dropped, DropRule::PositionSwitchOff, "Position", "Enabled") != nullptr;
    Check(switchOff == !l.positionEnabled, name + ": [Position] Enabled recorded as a position switch does not match its value");
    if (switchOff) ++tally.with_position_switch_off;

    const bool crosshairOff = FindDrop(imported.dropped, DropRule::Reticle, "Crosshair", "Compensate") != nullptr;
    Check(crosshairOff == !l.crosshairCompensation,
          name + ": [Crosshair] Compensate dropped as a reticle setting does not match its value");
    if (crosshairOff) ++tally.with_crosshair_off;

    for (const DroppedValue& d : imported.dropped) {
        Check(d.rule == DropRule::PoseShaping || d.rule == DropRule::PositionSwitchOff || d.rule == DropRule::Reticle,
              name + ": the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
}

// The settings the mod starts on after the migration against the ones the frozen reader's build
// started on, with the approved changes applied: identity pose shaping (CheckDrops holds the import
// to recording every value it leaves out), and the crosshair and the mode cycle no longer switched
// off (recorded as Reticle and PositionSwitchOff). The dev build applied its one LimitY both ways,
// and the hotkeys fire as they did.
std::vector<std::string> StartupDifferences(const legacy::Config& l, const Config& m) {
    std::vector<std::string> d;
    if (m.enable_on_startup != l.autoEnable) d.push_back("EnableOnStartup");
    if (m.udp_port != l.udpPort) d.push_back("UdpPort");
    const auto mode = cameraunlock::DecodeTrackingMode(m.rotation_enabled, m.position_enabled);
    if (!mode || *mode != (l.positionEnabled ? cameraunlock::TrackingMode::RotationAndPosition
                                             : cameraunlock::TrackingMode::RotationOnly)) {
        d.push_back("tracking mode");
    }
    if (m.world_space_yaw != l.worldSpaceYaw) d.push_back("WorldSpaceYaw");
    if (!SameBits(m.local_smoothing, l.localSmoothing)) d.push_back("LocalSmoothing");
    if (!SameBits(m.remote_smoothing, l.remoteSmoothing)) d.push_back("RemoteSmoothing");
    if (!SameBits(m.limit_x, l.limitX)) d.push_back("PositionLimitX");
    if (!SameBits(m.limit_y, l.limitY)) d.push_back("PositionLimitY");
    if (!SameBits(m.limit_y_down, l.limitY)) d.push_back("PositionLimitYDown");
    if (!SameBits(m.limit_z, l.limitZ)) d.push_back("PositionLimitZ");
    if (!SameBits(m.limit_z_back, l.limitZBack)) d.push_back("PositionLimitZBack");
    if (m.suppress_eye_tracking != l.suppressEyeTracking) d.push_back("SuppressEyeTracking");
    if (!SameBits(m.field_of_view, l.fieldOfView)) d.push_back("FieldOfView");

    const soma_oracle_view::FireTable before = soma_oracle_view::OracleFires(l.toggleKey, l.trackingModeKey, l.yawModeKey);
    const soma_oracle_view::FireTable after = CurrentFires(m);
    if (before != after) d.push_back("hotkeys: " + FirstFireDifference(before, after));
    return d;
}

// Every field the table binds, as the canonical renderer writes it, so two Configs compare whole.
std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

void Comparison2(Scratch& scratch, const Input& input, const ImportRun& import, const ImportResult* mapped,
                 const fs::path& defaults, Tally& tally) {
    const bool builtin = defaults == g_builtinDefaults;
    Tally::Run& run = builtin ? tally.builtin : tally.altered;
    const std::string name =
        input.name + (builtin ? " (Defaults.ini at the built-in values)" : " (Defaults.ini changed)");

    const fs::path dir = scratch.Clean("migration");
    const fs::path config = dir / kConfigFileName;
    const fs::path legacyFile = Place(dir, input);
    const FileStamp defaultsBefore = Stamp(defaults);
    FileStamp legacyBefore;
    if (input.bytes) legacyBefore = Stamp(legacyFile);

    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
    const Listing after = List(dir);
    Check(Stamp(defaults) == defaultsBefore, name + ": the load wrote Defaults.ini");
    if (input.bytes) {
        Check(Stamp(legacyFile) == legacyBefore, name + ": HeadTracking.ini did not keep its bytes, write time and attributes");
    }

    if (!input.bytes) {
        // A fresh install, which follows Defaults.ini.
        ++run.created;
        Check(loaded.status == ConfigLoadStatus::Created, name + ": no file is not Created");
        Check(after == Listing{{kConfigFileName, tally.committed}},
              name + ": the folder does not hold CameraUnlock.ini as config/CameraUnlock.ini and nothing else");
        if (builtin) {
            const std::vector<std::string> d = StartupDifferences(import.config, loaded.config);
            Check(d.empty(), name + ": comparison 2: " + Join(d));
        }
        return;
    }

    if (builtin) CheckDrops(name, import.config, *mapped, tally);

    // Over the changed Defaults.ini the rows the import left to it take its values.
    const legacy::Config expected =
        builtin ? import.config
                : OverAlteredDefaults(import.config, std::set<Concept>(mapped->follows_defaults_ini.begin(),
                                                                       mapped->follows_defaults_ini.end()));
    {
        const std::vector<std::string> d = StartupDifferences(expected, loaded.config);
        Check(d.empty(), name + ": comparison 2: " + Join(d));
    }

    // The dev build's reader held every value to a range the canonical rows hold too, so every file
    // it read migrates.
    ++run.imported;
    Check(loaded.status == ConfigLoadStatus::Migrated,
          name + ": the migration is " + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
    if (loaded.status != ConfigLoadStatus::Migrated) return;
    Check(after.size() == 2 && after[0].first == kConfigFileName && after[1].first == kFileName &&
              after[1].second == *input.bytes,
          name + ": the folder does not hold HeadTracking.ini and CameraUnlock.ini and nothing else");
    Check(Contains(loaded.log, "created from"), name + ": the log does not say where CameraUnlock.ini came from");
    const std::string migrated = ReadBytes(config);
    tally.migrated.insert(migrated);
    if (migrated.find("=default\r\n") != std::string::npos) ++run.with_default_rows;
    if (migrated != tally.committed) ++run.with_values;
    for (const Concept row : mapped->follows_defaults_ini) {
        const std::string key = cfg::schema::kConcepts[static_cast<std::size_t>(row)].key;
        Check(migrated.find("\r\n" + key + "=default\r\n") != std::string::npos, name + ": " + key + " is not written default");
    }
    if (builtin && IsUnedited(input.name)) {
        Check(migrated == tally.committed, name + ": does not migrate to the committed file");
    }

    // The next launch reads CameraUnlock.ini over the same Defaults.ini, with nothing to report, to
    // the same settings, does not import, and writes neither file.
    {
        const cfg::ConfigLoadResult<Config> reread = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
        Check(reread.status == ConfigLoadStatus::Canonical && reread.diagnostics.empty(),
              name + ": the next launch does not read CameraUnlock.ini cleanly");
        Check(AllValues(reread.config) == AllValues(loaded.config), name + ": the next launch runs on other settings");
        Check(!Contains(reread.log, "created from"), name + ": the next launch imports again");
        Check(Contains(reread.log, "is left as it was and is not read"),
              name + ": the next launch does not say HeadTracking.ini is not read");
        Check(List(dir) == after && Stamp(legacyFile) == legacyBefore && Stamp(defaults) == defaultsBefore,
              name + ": the next launch changed a file");
    }

    // A read-only HeadTracking.ini imports as a writable one does and keeps its attribute, bytes
    // and write time.
    if (builtin) {
        const fs::path roDir = scratch.Clean("read-only");
        const fs::path roLegacy = Place(roDir, input);
        SetReadOnly(roLegacy, true);
        const FileStamp roBefore = Stamp(roLegacy);
        const cfg::ConfigLoadResult<Config> fromReadOnly = cfg::ConfigOwner<Config>(OwnerOptions(roDir, defaults)).Load();
        Check(fromReadOnly.status == ConfigLoadStatus::Migrated && AllValues(fromReadOnly.config) == AllValues(loaded.config) &&
                  ReadBytes(roDir / kConfigFileName) == migrated,
              name + ": a read-only HeadTracking.ini does not import as a writable one does");
        Check(Stamp(roLegacy) == roBefore && (roBefore.attributes & FILE_ATTRIBUTE_READONLY) != 0,
              name + ": a read-only HeadTracking.ini did not keep its attribute, bytes and write time");
    }
}

// Defaults.ini as a player may have changed it, from the one the owner created: every value this
// game takes from it differs from the built-in one, each set to the corpus's alternate for the
// legacy key it comes from, so a corpus input holding that alternate migrates as default.
void WriteAlteredDefaults() {
    std::string text = ReadBytes(g_builtinDefaults);
    const std::pair<const char*, const char*> changes[] = {
        {"UdpPort=4242", "UdpPort=4243"},
        {"EnableOnStartup=true", "EnableOnStartup=false"},
        {"PositionEnabled=true", "PositionEnabled=false"},
        {"WorldSpaceYaw=true", "WorldSpaceYaw=false"},
        {"LocalSmoothing=0.0", "LocalSmoothing=0.3"},
        {"RemoteSmoothing=0.15", "RemoteSmoothing=0.3"},
        {"PositionLimitX=0.3", "PositionLimitX=0.5"},
        {"PositionLimitY=0.2", "PositionLimitY=0.5"},
        {"PositionLimitYDown=0.2", "PositionLimitYDown=0.5"},
        {"PositionLimitZ=0.4", "PositionLimitZ=0.5"},
        {"PositionLimitZBack=0.1", "PositionLimitZBack=0.2"},
        {"ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F1, Ctrl+Shift+Y"},
        {"CycleTrackingModeKey=PageUp, Ctrl+Shift+G", "CycleTrackingModeKey=F2, Ctrl+Shift+G"},
        {"YawModeKey=PageDown, Ctrl+Shift+H", "YawModeKey=F3, Ctrl+Shift+H"},
    };
    for (const auto& [from, to] : changes) {
        const std::string line = std::string("\r\n") + from + "\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error(std::string("the created Defaults.ini has no line ") + from);
        text.replace(at + 2, std::strlen(from), to);
    }
    fs::create_directories(g_alteredDefaults.parent_path());
    WriteBytes(g_alteredDefaults, text);
}

std::string Data(const char* name) {
    const std::string bytes = ReadBytes(fs::path(SOMA_DIFFERENTIAL_DATA) / name);
    Check(!bytes.empty(), std::string("data/") + name + " is empty");
    return bytes;
}

constexpr const char* kDocumented = "documented-7d8e2c8.ini";

std::vector<Input> Inputs() {
    using cameraunlock::config::testing::GenerateIniMutations;
    std::vector<Input> inputs;
    inputs.push_back({"no file", std::nullopt});
    inputs.push_back({"empty file", std::string()});
    inputs.push_back({kDocumented, Data(kDocumented)});
    // A hotkey on Ctrl, Shift or Alt alone, which the dev build refused for its default.
    for (const char* line : {"ToggleKey=0x10", "TrackingModeKey=0x11", "YawModeKey=0x12", "ToggleKey=0xA0",
                             "TrackingModeKey=0xA3", "YawModeKey=0xA5"}) {
        inputs.push_back({std::string("modifier key: ") + line, std::string("[Hotkeys]\r\n") + line + "\r\n"});
    }
    // A number the dev build read as not finite, which its guards replaced with the default.
    for (const char* line : {"LimitX=nan", "LimitY=inf", "LimitZ=-inf", "LimitZBack=nan"}) {
        inputs.push_back({std::string("non-finite limit: ") + line, std::string("[Position]\r\n") + line + "\r\n"});
    }
    for (auto& m : GenerateIniMutations(Data(kDocumented), legacy::ReadKeys(), MutationKeys())) {
        inputs.push_back({std::string("corpus over ") + kDocumented + ": " + m.name, std::move(m.bytes)});
    }
    return inputs;
}

}  // namespace

int main() {
    try {
        Scratch scratch;
        Tally tally;
        tally.committed = ReadBytes(fs::path(SOMA_COMMITTED_CONFIG));
        Check(!tally.committed.empty(), "config/CameraUnlock.ini is missing");

        // Each Defaults.ini sits outside the game folder, in a user folder of its own whose parent
        // exists, as the owner requires before it creates the file.
        g_builtinDefaults = scratch.Clean("user-builtin") / "CameraUnlock" / "Defaults.ini";
        g_alteredDefaults = scratch.Clean("user-altered") / "CameraUnlock" / "Defaults.ini";
        {
            const fs::path dir = scratch.Clean("first-load");
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Created,
                  "the first load is not Created");
            Check(fs::exists(g_builtinDefaults), "the first load did not create Defaults.ini");
        }
        WriteAlteredDefaults();

        // Fresh equals upgrade: over Defaults.ini at the built-in values, the file the dev build's
        // README documented imports into a CameraUnlock.ini that is the committed file, which is
        // what a fresh install creates. That build shipped, seeded and wrote no file of its own.
        {
            const fs::path dir = scratch.Clean("fresh-equals-upgrade");
            WriteBytes(dir / kFileName, Data(kDocumented));
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Migrated &&
                      ReadBytes(dir / kConfigFileName) == tally.committed,
                  std::string(kDocumented) + " does not import into the committed file");
        }

        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (dev, 7d8e2c8) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) {
            const ImportRun import = Comparison1(scratch, input);
            std::optional<ImportResult> mapped;
            if (input.bytes) {
                mapped = RunMappedImport(scratch, input);
                Check(mapped->status == ImportStatus::Imported, input.name + ": the mapped import is not Imported");
                const std::set<Concept> follows(mapped->follows_defaults_ini.begin(), mapped->follows_defaults_ini.end());
                Check(follows.size() == mapped->follows_defaults_ini.size(),
                      input.name + ": follows_defaults_ini names a row twice");
                const std::set<Concept> untouched = UntouchedRows(import.config);
                Check(follows == untouched, input.name + ": follows Defaults.ini " + Names(follows) +
                                                ", but the rows the player never changed are " + Names(untouched));
                if (untouched != AllRows()) ++tally.touched;
                if (!untouched.count(Concept::RotationEnabled)) ++tally.mode_touched;
                if (IsUnedited(input.name)) Check(untouched == AllRows(), input.name + ": a row is changed");
            }
            for (const fs::path& defaults : {g_builtinDefaults, g_alteredDefaults}) {
                Comparison2(scratch, input, import, mapped ? &*mapped : nullptr, defaults, tally);
            }
        }

        std::printf("comparison 2, the import against the migration, %zu distinct files:\n", tally.migrated.size());
        for (const auto& [over, run] : {std::pair<const char*, const Tally::Run*>{"at the built-in values", &tally.builtin},
                                        std::pair<const char*, const Tally::Run*>{"changed", &tally.altered}}) {
            std::printf("  over Defaults.ini %s: %d created, %d imported (%d holding a default row, %d differing from "
                        "the committed file)\n",
                        over, run->created, run->imported, run->with_default_rows, run->with_values);
            Check(run->with_default_rows > 0, std::string("no import writes default over ") + over);
            Check(run->with_values > 0, std::string("no import writes a value over ") + over);
        }
        std::printf("  %d with a changed sensitivity or inversion dropped (pose_shaping)\n", tally.with_pose_shaping_dropped);
        std::printf("  %d with [Position] Enabled=false imported as rotation only (position_switch_off)\n",
                    tally.with_position_switch_off);
        std::printf("  %d with [Crosshair] Compensate=false dropped (reticle)\n", tally.with_crosshair_off);
        std::printf("  %d inputs changed a row from the dev build's default, %d of them the tracking mode\n", tally.touched,
                    tally.mode_touched);
        Check(tally.with_pose_shaping_dropped > 0, "no input drops a changed pose-shaping value");
        Check(tally.with_position_switch_off > 0, "no input turns the position switch off");
        Check(tally.with_crosshair_off > 0, "no input turns the crosshair compensation off");
        Check(tally.touched > 0 && tally.mode_touched > 0,
              "no input changes a row, the tracking mode among them, which then does not follow Defaults.ini");
        Check(tally.migrated.count(tally.committed) == 1, "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const fs::path lintDir = fs::path(exe).parent_path() / "migrated";
        fs::remove_all(lintDir);
        fs::create_directories(lintDir);
        int n = 0;
        for (const std::string& file : tally.migrated) WriteBytes(lintDir / (std::to_string(n++) + ".ini"), file);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
