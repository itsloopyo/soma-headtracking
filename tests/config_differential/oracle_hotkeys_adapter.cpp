// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Compiled into the hotkey oracle library only, with `cameraunlock` renamed, so the chord guards
// here are the pin's (oracle/core/include/cameraunlock/input/chord_hotkeys.h) and read
// oracle_fake's keyboard.
#include "fake_keyboard.h"

#include "cameraunlock/input/chord_hotkeys.h"
#include "oracle_adapter.h"

#include <functional>
#include <utility>
#include <vector>

namespace soma_oracle_view {

FireTable OracleFires(int toggleKey, int trackingModeKey, int yawModeKey) {
    namespace input = cameraunlock::input;
    std::array<int, kActions> fired{};
    auto toggle = [&fired] { ++fired[0]; };
    auto cycle = [&fired] { ++fired[1]; };
    auto yaw = [&fired] { ++fired[2]; };

    // The six registrations of the dev build's Mod::RegisterHotkeys (src/mod.cpp at 7d8e2c8), in
    // its order. Its poller ran a callback when that callback's nonzero key went down.
    std::vector<std::pair<int, std::function<void()>>> registered;
    registered.emplace_back(toggleKey, input::NavGuarded(toggle));
    registered.emplace_back(trackingModeKey, input::NavGuarded(cycle));
    registered.emplace_back(yawModeKey, input::NavGuarded(yaw));
    registered.emplace_back('Y', input::ChordGuarded(toggle));
    registered.emplace_back('G', input::ChordGuarded(cycle));
    registered.emplace_back('H', input::ChordGuarded(yaw));

    FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            input::FakeHeld() = held;
            for (const auto& r : registered) {
                if (r.first == vk && r.first != 0) r.second();
            }
            table.push_back(fired);
        }
    }
    input::FakeHeld() = 0;
    return table;
}

}  // namespace soma_oracle_view
