// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "windows_lean.h"

#include <atomic>
#include <cstdlib>

#include "window_centering.h"

#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/os/game_window.h"

namespace SomaHT {

namespace {

namespace os = cameraunlock::os;

constexpr int kPollIntervalMs = 250;

// How long to wait for a window to exist at all. A cold start off a hard disk
// can take most of this.
constexpr int kFindAttempts = 240;  // 60s

// And how long, once one exists, to wait for it to stop moving. The engine is
// still sizing and placing the window for a moment after it first appears, so
// the trigger is an unchanged rect rather than a fixed delay - but the budget
// is short and it is measured from the FIRST sighting, which is what keeps this
// out of the player's way. A window that is still being moved eight seconds
// after it appeared is being moved by someone, and a mod that centres it then
// is a mod that undoes a drag.
constexpr int kSettleAttempts = 32;  // 8s
constexpr int kSettlePolls = 12;     // 3s of an unchanged rect

std::atomic<bool> g_cancelled{false};

int CenteredOrigin(int areaStart, int areaExtent, int windowExtent) {
    return areaStart + (areaExtent - windowExtent) / 2;
}

bool IsCenteredOn(const RECT& window, const RECT& area) {
    // A game that centres its own window rounds the odd half-pixel up where the
    // integer maths here rounds it down, so an exact comparison would move the
    // window one pixel and report that as a fix.
    constexpr int kTolerance = 2;
    const int dx = window.left -
                   CenteredOrigin(area.left, area.right - area.left, window.right - window.left);
    const int dy = window.top -
                   CenteredOrigin(area.top, area.bottom - area.top, window.bottom - window.top);
    return std::abs(dx) <= kTolerance && std::abs(dy) <= kTolerance;
}

void CenterUnlessAlready(HWND window, const RECT& rect) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) {
        cameraunlock::logging::Line("WARNING: window: GetMonitorInfoW failed: %lu",
                                    GetLastError());
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    // Either reading counts as centred. A game centres on the monitor, this mod
    // centres on the work area, and the two differ by half the taskbar. Moving a
    // window that is already centred trades a visible jump for nothing, and a
    // fullscreen or borderless window is centred by definition, which is how it
    // is left alone here.
    if (IsCenteredOn(rect, info.rcWork) || IsCenteredOn(rect, info.rcMonitor)) {
        cameraunlock::logging::Line(
            "window: %dx%d at (%d, %d) is already centred, leaving it alone", width, height,
            static_cast<int>(rect.left), static_cast<int>(rect.top));
        return;
    }

    // The work area, not the monitor bounds: centring against the full monitor
    // puts the title bar behind a top-docked taskbar, and the window cannot then
    // be dragged back.
    const int workWidth = info.rcWork.right - info.rcWork.left;
    const int workHeight = info.rcWork.bottom - info.rcWork.top;
    if (width >= workWidth || height >= workHeight) {
        cameraunlock::logging::Line(
            "window: %dx%d fills the work area %dx%d, leaving it in place", width, height,
            workWidth, workHeight);
        return;
    }

    const int x = CenteredOrigin(info.rcWork.left, workWidth, width);
    const int y = CenteredOrigin(info.rcWork.top, workHeight, height);

    // On the window handle this function was handed, not one re-found here: the
    // rect above was measured off that handle, and a second lookup can answer
    // with a different window.
    //
    // SWP_ASYNCWINDOWPOS because the window belongs to the game's own thread.
    // Without it SetWindowPos posts WM_WINDOWPOSCHANGING across threads and
    // blocks, with no timeout, until that thread pumps - and the moment this
    // fires is the moment the window has been still for three seconds, which is
    // most likely to be a synchronous map load with the pump stopped.
    if (!SetWindowPos(window, nullptr, x, y, 0, 0,
                      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)) {
        cameraunlock::logging::Line("WARNING: window: SetWindowPos failed: %lu", GetLastError());
        return;
    }
    // Asked, not did: SWP_ASYNCWINDOWPOS returns once the request is posted to
    // the window's own thread, which has to pump before anything moves.
    cameraunlock::logging::Line(
        "window: asked Windows to centre the %dx%d window at (%d, %d) on the %dx%d work area",
        width, height, x, y, workWidth, workHeight);
}

// Sleeps one poll interval. False when the mod is being torn down and this
// should stop where it is.
bool PollWait() {
    Sleep(kPollIntervalMs);
    return !g_cancelled.load(std::memory_order_acquire);
}

HWND WaitForWindow() {
    for (int attempt = 0; attempt < kFindAttempts; ++attempt) {
        if (!PollWait()) return nullptr;
        HWND window = os::FindGameWindow();
        if (window != nullptr) return window;
    }
    return nullptr;
}

// Why WaitForSettledRect gave up, so the caller does not report one as another.
enum class SettleResult { Settled, Restless, WindowGone, Cancelled };

// Waits for @p window's rect to hold still for kSettlePolls, writing it to
// @p settledRect on Settled.
SettleResult WaitForSettledRect(HWND window, RECT& settledRect) {
    RECT previous{};
    bool havePrevious = false;
    int stablePolls = 0;

    for (int attempt = 0; attempt < kSettleAttempts; ++attempt) {
        if (g_cancelled.load(std::memory_order_acquire)) return SettleResult::Cancelled;

        RECT current{};
        if (!GetWindowRect(window, &current)) return SettleResult::WindowGone;

        if (havePrevious && EqualRect(&previous, &current)) {
            if (++stablePolls >= kSettlePolls) {
                settledRect = current;
                return SettleResult::Settled;
            }
        } else {
            previous = current;
            havePrevious = true;
            stablePolls = 0;
        }

        if (!PollWait()) return SettleResult::Cancelled;
    }
    return SettleResult::Restless;
}

}  // namespace

void CancelWindowCentering() { g_cancelled.store(true, std::memory_order_release); }

void CenterWindowWhenReady() {
    HWND window = WaitForWindow();
    if (window == nullptr) {
        // Silent on cancellation - the mod is being torn down and there is
        // nothing to report about a window it never looked at again.
        if (!g_cancelled.load(std::memory_order_acquire)) {
            cameraunlock::logging::Line(
                "window: no game window appeared within %ds, leaving placement alone",
                kFindAttempts * kPollIntervalMs / 1000);
        }
        return;
    }

    RECT rect{};
    switch (WaitForSettledRect(window, rect)) {
        case SettleResult::Settled:
            CenterUnlessAlready(window, rect);
            return;
        case SettleResult::Restless:
            cameraunlock::logging::Line(
                "window: the game window did not hold still within %ds of appearing, leaving "
                "its placement alone",
                kSettleAttempts * kPollIntervalMs / 1000);
            return;
        case SettleResult::WindowGone:
            cameraunlock::logging::Line(
                "window: the game window went away while it was being measured, leaving "
                "placement alone");
            return;
        case SettleResult::Cancelled:
            return;
    }
}

}  // namespace SomaHT
