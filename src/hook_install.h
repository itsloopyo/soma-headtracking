// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "windows_lean.h"

#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/memory/pattern_scanner.h"

// Finding a function in the running executable by byte signature and putting a
// detour on it, with the log lines each hook was writing for itself.
//
// @p category is the prefix the mod's log lines carry for that hook - "Camera",
// "Crosshair", "Gui input", "Eye tracking" - and @p name the engine function.
// Between them a failure names both, which is what a user's log has to say for
// a missing signature to be actionable.
namespace SomaHT::detour {

// The running executable, which is what every signature in this mod is scanned
// against: the game is the host process and the mod is a DLL inside it.
inline void* HostModule() { return GetModuleHandleW(nullptr); }

// A silent scan, for the two callers that cannot use Find: one tries a second
// signature when the first misses, the other logs an address the pattern only
// leads to.
inline void* Scan(const char* pattern) {
    return cameraunlock::memory::ScanPattern(HostModule(), pattern);
}

// Scans and says what it found either way. Null means the signature is not in
// this executable, which is a build the mod does not know.
inline void* Find(const char* category, const char* name, const char* pattern) {
    void* address = Scan(pattern);
    if (address == nullptr) {
        cameraunlock::logging::Line("%s: %s is not where this build puts it.", category, name);
        return nullptr;
    }
    cameraunlock::logging::Line("%s: %s found at %p", category, name, address);
    return address;
}

// Creates and enables one detour, and hands back the target as the caller's
// record of it - the value Remove takes to put the code back.
//
// A hook that was created but could not be enabled is rolled back here rather
// than left behind: MinHook holds a trampoline and a patched page for it, and
// nothing outside this call knows it exists.
inline void* Install(const char* category, const char* name, void* target, void* detourFn,
                     void** original) {
    using namespace cameraunlock::hooks;
    auto& hm = HookManager::Instance();

    HookStatus s = hm.CreateHook(target, detourFn, original);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("%s: CreateHook(%s) failed (%s)", category, name,
                                    HookStatusToString(s));
        return nullptr;
    }
    s = hm.EnableHook(target);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("%s: EnableHook(%s) failed (%s)", category, name,
                                    HookStatusToString(s));
        hm.RemoveHook(target);
        return nullptr;
    }
    return target;
}

// Takes one back out and clears the caller's record of it, so a second removal
// - Shutdown after a failed install, or two of them - does nothing.
inline void Remove(void*& installed) {
    if (installed == nullptr) return;
    cameraunlock::hooks::HookManager::Instance().RemoveHook(installed);
    installed = nullptr;
}

}  // namespace SomaHT::detour
