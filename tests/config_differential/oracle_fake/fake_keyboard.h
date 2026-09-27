// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// Included ahead of the pin's chord_hotkeys.h in the hotkey oracle library only, so its guards'
// unqualified GetAsyncKeyState calls find this one, declared in their own namespace, before
// ::GetAsyncKeyState. That is how the test holds modifiers without a keyboard. Compiled with that
// library's renaming, so it cannot collide with the current core.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace cameraunlock::input {

// The modifiers the test holds: 1 Ctrl, 2 Shift, 4 Alt.
inline int& FakeHeld() {
    static int held = 0;
    return held;
}

inline SHORT GetAsyncKeyState(int vk) {
    const int held = FakeHeld();
    const bool down = (vk == VK_CONTROL && (held & 1) != 0) || (vk == VK_SHIFT && (held & 2) != 0) ||
                      (vk == VK_MENU && (held & 4) != 0);
    return down ? static_cast<SHORT>(0x8000) : static_cast<SHORT>(0);
}

}  // namespace cameraunlock::input
