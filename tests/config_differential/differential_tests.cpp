// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read two ways:
//
//   oracle     the reader of the dev pre-release (7d8e2c8), the only published build, with the
//              core sources it compiled at its pin bd22895 (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Inputs: no file, an empty file, the HeadTracking.ini the dev build's README documented (the
// build shipped no config, seeded none and wrote none, so a player's file is one they wrote,
// most likely from that example), a hotkey on each of Ctrl, Shift and Alt, a non-finite number,
// and core's corpus over the documented file.

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
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
        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (dev, 7d8e2c8) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) Comparison1(scratch, input);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
