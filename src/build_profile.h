// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "game_offsets.h"

#include "cameraunlock/memory/pe_fingerprint.h"

// One shipped SOMA executable: its PE fingerprint and the structure layout it
// was built with.
//
// Append-only. A patch that moves a member gets a NEW profile added to the top
// of kKnownProfiles with its own Layout, never an in-place edit, so a user who
// has not taken the patch keeps matching the profile their executable was built
// with. The top entry is the diagnostic primary: when nothing matches, the
// running fingerprint is compared against it to say whether the game is newer,
// older, or repacked.
//
// Byte signatures locate the functions, so a fingerprint match is not what finds
// them - it is what says the structures around them are laid out the way this
// mod believes. Without it a signature that still matches after a patch would be
// enough to start writing floats into a cCamera whose members have moved.
namespace SomaHT {

struct BuildProfile {
    const char* name;
    cameraunlock::memory::PeFingerprint fingerprint;
    const offsets::Layout* layout;
};

// Most recent build first.
extern const BuildProfile kKnownProfiles[];
extern const int kKnownProfileCount;

// Fingerprints the running executable, logs the result either way, and points
// offsets::Active() at the matching profile's layout. Returns false when nothing
// matches, in which case the caller installs no hooks and the game runs vanilla.
bool MatchRunningBuild();

}  // namespace SomaHT
