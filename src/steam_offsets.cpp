// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "windows_lean.h"

#include "build_profile.h"

#include "cameraunlock/logging/file_log.h"

// The Steam depot's two executables. Both were linked on 2021-09-27 and carry
// the same structure layout - they are the same code, linked at different
// addresses, which is why one Layout serves both and why every function is
// found by byte signature rather than by RVA.
//
// The fingerprints were read out of the PE headers of the shipped files:
// TimeDateStamp, SizeOfImage, CheckSum.
namespace SomaHT {
namespace {

const BuildProfile kSteamProfile_20210927 = {
    "steam-win64-20210927",
    {0x6151D1CAu, 0x008FB000u, 0x008DCFE4u},
    &offsets::kLayout_20210927,
};

const BuildProfile kSteamNoSteamProfile_20210927 = {
    "steam-nosteam-win64-20210927",
    {0x6151CA9Cu, 0x0087B000u, 0x0085480Au},
    &offsets::kLayout_20210927,
};

}  // namespace

const BuildProfile kKnownProfiles[] = {
    kSteamProfile_20210927,
    kSteamNoSteamProfile_20210927,
};
const int kKnownProfileCount = static_cast<int>(sizeof(kKnownProfiles) / sizeof(kKnownProfiles[0]));

bool MatchRunningBuild() {
    cameraunlock::memory::PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(GetModuleHandleW(nullptr), running)) {
        cameraunlock::logging::Line(
            "Build: could not read the executable's PE fingerprint - mod dormant.");
        return false;
    }

    // The running fingerprint goes in the log whatever happens next. A user on a
    // build this mod does not know sends this file and nothing else, and without
    // these three numbers the new profile cannot be written from it.
    cameraunlock::logging::Line(
        "Build: EXE fingerprint TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
        running.TimeDateStamp, running.SizeOfImage, running.CheckSum);

    for (int i = 0; i < kKnownProfileCount; ++i) {
        const BuildProfile& p = kKnownProfiles[i];
        if (running.Matches(p.fingerprint)) {
            cameraunlock::logging::Line("Build: matched profile %s", p.name);
            offsets::SetActiveLayout(*p.layout);
            return true;
        }
    }

    using cameraunlock::memory::ClassifyMismatch;
    using cameraunlock::memory::FingerprintMismatch;
    const BuildProfile& primary = kKnownProfiles[0];
    switch (ClassifyMismatch(running, primary.fingerprint)) {
        case FingerprintMismatch::Newer:
            cameraunlock::logging::Line(
                "Build: this SOMA is newer than any build the mod knows about (newest is %s) - "
                "mod dormant. Check the releases page for an updated mod.",
                primary.name);
            break;
        case FingerprintMismatch::Older:
            cameraunlock::logging::Line(
                "Build: this SOMA is older than the newest build the mod knows about (%s) "
                "and matches no other - mod dormant. Let the store finish updating.",
                primary.name);
            break;
        case FingerprintMismatch::Differs:
            cameraunlock::logging::Line(
                "Build: this executable is repacked or modified - mod dormant. It will not "
                "engage on a binary it cannot identify.");
            break;
    }
    return false;
}

}  // namespace SomaHT
