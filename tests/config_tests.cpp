// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Config::Load is the one place text a player typed becomes a number that
// reaches the camera, the UDP bind and the hotkey poller, and until this file
// existed nothing in the tree compiled it - headers_strict.cpp includes
// config.h, which is the declarations only.
//
// Each case below is a value that used to be accepted, or discarded, silently.
// Every one of them is what a hand-written INI actually contains: the mod seeds
// no HeadTracking.ini, so there is no correct file for a player to copy.

#include "config.h"
#include "test_support.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;

// GetPrivateProfileStringA resolves a relative path against the Windows
// directory, so every fixture is written to an absolute path.
class Fixture {
public:
    explicit Fixture(const char* body) {
        path_ = (std::filesystem::temp_directory_path() /
                 ("soma_ht_config_test_" + std::to_string(++counter_) + ".ini")).string();
        std::FILE* f = std::fopen(path_.c_str(), "wb");
        if (f != nullptr) {
            std::fwrite(body, 1, std::char_traits<char>::length(body), f);
            std::fclose(f);
        }
    }

    ~Fixture() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    const char* Path() const { return path_.c_str(); }

private:
    std::string path_;
    static int counter_;
};

int Fixture::counter_ = 0;

SomaHT::Config Load(const char* body, bool& loaded) {
    Fixture ini(body);
    SomaHT::Config config;
    loaded = config.Load(ini.Path());
    return config;
}

}  // namespace

int RunConfigTests() {
    std::cout << "\nConfig tests\n";
    Report r;

    const SomaHT::Config defaults;

    // IniReader::Open finds a relative path from the working directory and
    // every read that follows looks for it in the Windows directory instead, so
    // this is the case where the file that was found is not the file that is
    // read: the load used to report success and hand back defaults. The fixture
    // is written into the working directory precisely so the existence check
    // passes.
    {
        const std::filesystem::path previousCwd = std::filesystem::current_path();
        std::filesystem::current_path(std::filesystem::temp_directory_path());
        {
            Fixture ini("[General]\nWorldSpaceYaw=false\n");
            const std::filesystem::path full(ini.Path());
            const std::string name = full.filename().string();

            SomaHT::Config config;
            r.Check(!config.Load(name.c_str()),
                    "a relative config path is refused even when the file is there");
            r.Check(config.worldSpaceYaw == defaults.worldSpaceYaw,
                    "a refused path leaves the defaults alone");

            // The other shape the profile API treats as not fully qualified:
            // "C:name.ini" is drive-RELATIVE, naming the current directory of
            // drive C: rather than its root. Built from the fixture's own drive
            // and filename so the existence check would pass, which is what
            // makes this the same "the file that was found is not the file that
            // is read" case rather than just a missing file.
            const std::string driveRelative = full.root_name().string() + name;
            SomaHT::Config driveRelativeConfig;
            r.Check(!driveRelativeConfig.Load(driveRelative.c_str()),
                    "a drive-relative config path is refused even when the file is there");
            r.Check(driveRelativeConfig.worldSpaceYaw == defaults.worldSpaceYaw,
                    "a refused drive-relative path leaves the defaults alone");
        }
        std::filesystem::current_path(previousCwd);

        SomaHT::Config config;
        r.Check(!config.Load(nullptr), "a null config path is refused");
        r.Check(!config.Load(""), "an empty config path is refused");
    }

    {
        SomaHT::Config config;
        const std::string missing =
            (std::filesystem::temp_directory_path() / "soma_ht_config_absent.ini").string();
        std::error_code ec;
        std::filesystem::remove(missing, ec);
        r.Check(!config.Load(missing.c_str()), "an absent config file reports not loaded");
    }

    // A comment where the docs put one for a numeric key. IniReader::ReadBool
    // matches the whole value, so this used to revert to the default with
    // nothing in the log.
    {
        bool loaded = false;
        const SomaHT::Config c = Load(
            "[General]\nWorldSpaceYaw=false ; camera-local\nAutoEnable=false ; start off\n", loaded);
        r.Check(loaded, "a config with an inline comment loads");
        r.Check(c.worldSpaceYaw == false, "a bool survives a trailing ';' comment");
        r.Check(c.autoEnable == false, "a second bool survives a trailing ';' comment");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[General]\nWorldSpaceYaw=FALSE\nAutoEnable=Off\n", loaded);
        r.Check(loaded && c.worldSpaceYaw == false, "a bool is read whatever its casing");
        r.Check(c.autoEnable == false, "on/off are accepted as bools");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[General]\nWorldSpaceYaw=maybe\n", loaded);
        r.Check(loaded && c.worldSpaceYaw == defaults.worldSpaceYaw,
                "a bool that is not true or false keeps the default");
    }

    {
        bool loaded = false;
        const SomaHT::Config c =
            Load("[Position]\nEnabled=no\nLimitZ=0.25 ; tighter forward lean\n", loaded);
        r.Check(loaded && c.positionEnabled == false, "yes/no are accepted as bools");
        r.Check(NearEqual(c.limitZ, 0.25f), "a limit survives its comment");
    }

    // strtod parses a prefix, so a European decimal comma used to read back as
    // 0.0 - inside the valid range, and silent. RemoteSmoothing rather than
    // LocalSmoothing because its default is not the value the bug produced.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Sensitivity]\nRemoteSmoothing=0,15\n", loaded);
        r.Check(loaded && NearEqual(c.remoteSmoothing, defaults.remoteSmoothing),
                "a decimal comma is refused rather than read as zero");
    }

    {
        bool loaded = false;
        const SomaHT::Config c =
            Load("[Sensitivity]\nRemoteSmoothing=nan\nYawMultiplier=1e400\n", loaded);
        r.Check(loaded && NearEqual(c.remoteSmoothing, defaults.remoteSmoothing),
                "a NaN smoothing falls back to its own default");
        r.Check(NearEqual(c.yawSens, defaults.yawSens),
                "an overflowing sensitivity falls back to the default");
    }

    // Each smoothing key falls back to its own default: a bad RemoteSmoothing
    // dropping to the local 0.0 would leave a phone's network jitter unsmoothed.
    {
        bool loaded = false;
        const SomaHT::Config c =
            Load("[Sensitivity]\nLocalSmoothing=0.0\nRemoteSmoothing=oops\n", loaded);
        r.Check(loaded && NearEqual(c.localSmoothing, 0.0f),
                "a configured zero smoothing stays zero");
        r.Check(NearEqual(c.remoteSmoothing, defaults.remoteSmoothing),
                "a malformed remote smoothing keeps the remote default");
    }

    // ReadInt is the one reader that hands back 0 rather than its default on a
    // present-but-unparseable value, and a receiver bound to port 0 takes an
    // ephemeral port no tracker is sending to.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Network]\nUDPPort=0\n", loaded);
        r.Check(loaded && c.udpPort == defaults.udpPort, "port 0 falls back to the default");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Network]\nUDPPort=70000\n", loaded);
        r.Check(loaded && c.udpPort == defaults.udpPort,
                "a port past 65535 falls back rather than truncating to 4464");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Network]\nUDPPort=5555\n", loaded);
        r.Check(loaded && c.udpPort == 5555, "a port in range is kept as typed");
    }

    // A negative limit inverts PositionProcessor's clamp and pins the lean at a
    // fixed offset instead of freeing it.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Position]\nLimitZ=-1.0\nLimitX=99999\n", loaded);
        r.Check(loaded && c.limitZ >= 0.0f, "a negative travel limit is floored at zero");
        r.Check(c.limitX <= 10.0f, "an absurd travel limit is clamped");
    }

    // A virtual key GetAsyncKeyState cannot report is a binding that is polled
    // forever without ever firing.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Hotkeys]\nToggleKey=0x230\nYawModeKey=0x10\n", loaded);
        r.Check(loaded && c.toggleKey == defaults.toggleKey,
                "a key past 0xFE falls back to the default");
        r.Check(c.yawModeKey == defaults.yawModeKey,
                "Shift is refused - the chord guard owns the modifiers");
    }

    // Insert, not Home: Home was half of the recenter pair before mods stopped
    // keeping a centre, so it stays unbound and must not be enshrined here as a
    // reasonable alternative.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Hotkeys]\nToggleKey=0x2D\n", loaded);
        r.Check(loaded && c.toggleKey == 0x2D, "a bindable key is kept as typed");
    }

    // ReadHex is a strtol PREFIX parse, so a key NAME parses as a plausible
    // virtual key and binds something else entirely with nothing in the log.
    {
        bool loaded2 = false;
        const SomaHT::Config d = Load("[Hotkeys]\nToggleKey=Delete\nYawModeKey=End\n", loaded2);
        r.Check(loaded2 && d.toggleKey == defaults.toggleKey,
                "a key name does not parse as its hex prefix 0xDE");
        r.Check(d.yawModeKey == defaults.yawModeKey,
                "a key name that parses to an unpollable code falls back too");
    }

    // Zero is the off switch rather than a bad value, and the playable floor is
    // applied only to a field the player actually asked for.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=0\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 0.0f),
                "a field of zero stays off rather than being floored");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=10\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 30.0f), "a field below 30 degrees is floored");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=200\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 120.0f), "a field past 120 degrees is clamped");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=85 ; wider\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 85.0f), "a field in range survives its comment");
    }

    // strtod accepts "nan" and "inf" and overflows a literal like 1e400 to
    // +inf, and the field of view is the one config value that reaches the
    // PROJECTION matrix. A non-finite one divided out as tan(fov/2) is a NaN
    // the game never recovers from - cLuxPlayer calls SetFOV only while a
    // script fade is in flight - so it has to land on the off switch, not on
    // the 30-degree playable floor.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=nan\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 0.0f),
                "a NaN field of view leaves the engine's own field alone");
    }

    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nFieldOfView=1e400\n", loaded);
        r.Check(loaded && NearEqual(c.fieldOfView, 0.0f),
                "an overflowing field of view leaves the engine's own field alone");
    }

    // A negative multiplier is a legitimate way to invert an axis without
    // touching the Invert flags, so only the magnitude is bounded. Flooring it
    // at zero would silently pin the axis instead.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Sensitivity]\nYawMultiplier=-1.5\n", loaded);
        r.Check(loaded && NearEqual(c.yawSens, -1.5f),
                "a negative sensitivity is kept as typed rather than floored");
    }

    // A player who wants SOMA's own Extended View back alongside the head pose
    // says so once, and the hook is then never installed.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[Camera]\nSuppressEyeTracking=off\n", loaded);
        r.Check(loaded && !c.suppressEyeTracking,
                "SuppressEyeTracking=off leaves the game's own eye tracking alone");
    }

    // The shipped defaults, as literals. Every other assertion in this file
    // compares a loaded Config against a default-constructed one, so all of them
    // would pass just as happily with the defaults themselves changed. These are
    // the numbers the docs, the hotkey table and the smoothing model promise.
    {
        r.Check(defaults.udpPort == 4242, "the default port is the OpenTrack one");
        r.Check(defaults.toggleKey == 0x23 && defaults.trackingModeKey == 0x21 &&
                    defaults.yawModeKey == 0x22,
                "the default hotkeys are End, Page Up and Page Down");
        r.Check(NearEqual(defaults.localSmoothing, 0.0f) &&
                    NearEqual(defaults.remoteSmoothing, 0.15f),
                "smoothing defaults to none locally and 0.15 remotely");
        r.Check(NearEqual(defaults.yawSens, 1.0f) && NearEqual(defaults.pitchSens, 1.0f) &&
                    NearEqual(defaults.rollSens, 1.0f) && NearEqual(defaults.posSensX, 1.0f) &&
                    NearEqual(defaults.posSensY, 1.0f) && NearEqual(defaults.posSensZ, 1.0f),
                "every sensitivity defaults to 1.0");
        r.Check(!defaults.invertYaw && !defaults.invertPitch && !defaults.invertRoll,
                "no axis is inverted by default");
        r.Check(NearEqual(defaults.limitX, 0.30f) && NearEqual(defaults.limitY, 0.20f) &&
                    NearEqual(defaults.limitZ, 0.40f) && NearEqual(defaults.limitZBack, 0.10f),
                "the travel limits are 0.30 up to 0.40 forward and 0.10 back");
        r.Check(defaults.positionEnabled && defaults.autoEnable && defaults.worldSpaceYaw &&
                    defaults.crosshairCompensation,
                "position, auto-enable, world-space yaw and crosshair compensation are on");
        r.Check(defaults.suppressEyeTracking,
                "SOMA's own eye tracking is held off while head tracking runs");
        r.Check(NearEqual(defaults.fieldOfView, 0.0f),
                "the field of view override is off, leaving the game's own alone");
    }

    // An INI that mentions none of the keys is the shipped state, and must not
    // move a single default.
    {
        bool loaded = false;
        const SomaHT::Config c = Load("[General]\n; nothing set\n", loaded);
        r.Check(loaded, "an empty config still loads");
        r.Check(c.udpPort == defaults.udpPort && c.worldSpaceYaw == defaults.worldSpaceYaw &&
                    NearEqual(c.localSmoothing, defaults.localSmoothing) &&
                    NearEqual(c.remoteSmoothing, defaults.remoteSmoothing) &&
                    c.toggleKey == defaults.toggleKey,
                "an empty config leaves every default where it was");
    }

    return r.Failures();
}
