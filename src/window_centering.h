// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace SomaHT {

// Waits for the game to bring its window up and hold it still, then centres it
// on the work area of the monitor it is already on. A window that already fills
// the work area, or that the game centred itself, is left alone - so this only
// ever moves a windowed-mode game.
//
// It acts once, or not at all. The decision is made as soon as the window has
// held still, and this returns - so a window the player moves after that stays
// where they put it. Before that it can still act: the search for a window runs
// for up to a minute to cover a cold start, and the settle wait for eight
// seconds after one appears, so on a slow load the placement can happen well
// over a minute into the session.
//
// Blocks the calling thread while it polls, so call it after everything else
// the init thread has to do, and call Cancel below before waiting on that
// thread.
void CenterWindowWhenReady();

// Makes CenterWindowWhenReady return at its next poll. DLL_PROCESS_DETACH waits
// a bounded time for the init thread and then gives up on unhooking, so without
// this a detach landing during the poll would always hit that timeout and unmap
// the module with its detours still installed.
void CancelWindowCentering();

}  // namespace SomaHT
