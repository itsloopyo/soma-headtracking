// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// SOMA head tracking - entry point.
//
// Loaded as an .asi by Ultimate ASI Loader dropped in as version.dll, which
// SDL2.dll imports and Windows resolves out of the exe directory. DllMain runs
// under the loader lock, so everything that scans, hooks or binds happens on an
// init thread instead.
//
// The log is opened here and nowhere else. cameraunlock::logging::Open renames
// the outgoing HeadTracking.log to HeadTracking.prev.log and opens the live
// file fresh, so one previous session survives and nothing accumulates; see
// exe_paths.h for why the path is resolved from the running EXE rather than
// from this DLL.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>

#include <exception>

#include "build_profile.h"
#include "exe_paths.h"
#include "mod.h"
#include "version.h"
#include "window_centering.h"

#include "cameraunlock/logging/file_log.h"

namespace {

HANDLE g_initThread = nullptr;

// The pattern scan reads the whole image, and HPL3 is still mapping and
// relocating for a moment after the first DLL attaches.
constexpr DWORD kInitDelayMs = 1500;

// DLL_PROCESS_DETACH runs under the loader lock, which the init thread needs in
// order to finish, so the wait is bounded.
constexpr DWORD kInitThreadJoinMs = 2000;

void Startup() {
    cameraunlock::logging::Open(SomaHT::paths::NextToHostExe(L"HeadTracking.log"));
    cameraunlock::logging::Line("SOMA Head Tracking v%s attached.", SomaHT::VERSION_STRING);

    // Which build this is. Soma.exe and Soma_NoSteam.exe are the same code
    // linked at different addresses, so both are known profiles; anything else
    // leaves the mod dormant rather than writing floats into structures whose
    // layout it cannot vouch for.
    if (!SomaHT::MatchRunningBuild()) return;

    SomaHT::Mod::Instance().Initialize();

    // Last, because it polls for the game window and blocks this thread while
    // it does. Only once the mod is actually live, too: a mod that went dormant
    // has promised to leave the game exactly as it found it, and a window that
    // jumps is not that.
    if (SomaHT::Mod::Instance().IsInitialized()) SomaHT::CenterWindowWhenReady();
}

// Nothing may escape this thread. An unhandled C++ exception on a thread with
// no handler calls std::terminate, which takes the game down - and there are
// three places inside that throw by design: std::thread construction in the
// hotkey poller and in the receiver, and the allocations either side of them.
// A mod that cannot start has to leave the game running.
unsigned __stdcall InitThread(void*) {
    Sleep(kInitDelayMs);
    try {
        Startup();
    } catch (const std::exception& e) {
        cameraunlock::logging::Line("Init failed (%s) - mod dormant.", e.what());
    } catch (...) {
        cameraunlock::logging::Line("Init failed - mod dormant.");
    }
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_initThread = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr));
            // The log is not open yet, so there is nowhere to say this. Nothing
            // else runs either: without the init thread the mod never touches
            // the process and the game is simply unmodded.
            break;
        case DLL_PROCESS_DETACH:
            // A non-null @p reserved means the PROCESS is exiting rather than
            // this DLL being unloaded, and none of the cleanup below is legal
            // there. Windows has already terminated the init, hotkey and
            // receiver threads, at whatever instruction each was on: one
            // stopped inside logging::Line holds the log mutex forever, so
            // Close() blocks on it under the loader lock and the game never
            // finishes quitting. Unhooking is worse - MinHook takes a toolhelp
            // snapshot and suspends every thread, both of which need the loader
            // lock this callback is holding.
            //
            // Skipping it costs nothing: the OS closes the log handle, reclaims
            // the threads and unmaps the patched code either way, and the log
            // is written with WriteFile rather than the CRT, so the page cache
            // already has every line.
            //
            // The FreeLibrary path (reserved == nullptr) still cleans up in
            // full - the process lives on, so the hooks have to come out.
            if (reserved != nullptr) break;
            // No init thread means nothing was ever installed to take out.
            if (g_initThread == nullptr) break;
            // Before the wait, not after it. The init thread's last act polls
            // for the game window for up to a minute, and the join below gives
            // up long before that - so without this the timeout is certain and
            // the module unmaps with its detours still in the game's code.
            SomaHT::CancelWindowCentering();
            // Only once it has actually finished. It takes the loader lock this
            // callback is holding, so the wait can run out; tearing down state
            // it is still using, or closing a handle it is still running on, is
            // worse than leaving the hooks in.
            if (WaitForSingleObject(g_initThread, kInitThreadJoinMs) != WAIT_OBJECT_0) break;
            CloseHandle(g_initThread);
            g_initThread = nullptr;
            SomaHT::Mod::Instance().Shutdown();
            cameraunlock::logging::Close();
            break;
        default:
            break;
    }
    return TRUE;
}
