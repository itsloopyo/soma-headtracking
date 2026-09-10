// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The build registry's structural rules.
//
// None of this is arithmetic - it is the shape of the table, and the shape is
// the whole failsafe. A profile appended to the BOTTOM of kKnownProfiles still
// matches its own executable, so nothing looks broken; what breaks is the one
// diagnostic a dormant mod emits, because ClassifyMismatch compares an
// unrecognised build against kKnownProfiles[0] and would then be comparing it
// against a build years older than the newest one the mod knows. A user on a
// freshly patched game is told their game is newer than a 2021 profile while a
// 2027 profile sits two rows down, and the line they send back names the wrong
// build.
//
// So: newest first, no duplicates, and every profile pointing at a layout.

#include "build_profile.h"
#include "test_support.h"

namespace {

using SomaHT::testing::Report;

}  // namespace

int RunBuildProfileTests() {
    std::cout << "\nBuild profile registry tests\n";
    Report r;

    r.Check(SomaHT::kKnownProfileCount > 0, "the registry names at least one shipped build");

    {
        bool ok = true;
        for (int i = 0; i < SomaHT::kKnownProfileCount; ++i) {
            const SomaHT::BuildProfile& p = SomaHT::kKnownProfiles[i];
            ok = ok && p.name != nullptr && p.name[0] != '\0' && p.layout != nullptr;
        }
        r.Check(ok, "every profile carries a name and a layout");
    }

    // Three fields, so a repacked executable that kept one of them fails the
    // match instead of silently routing to a layout it was not built with.
    {
        bool ok = true;
        for (int i = 0; i < SomaHT::kKnownProfileCount; ++i) {
            for (int j = i + 1; j < SomaHT::kKnownProfileCount; ++j) {
                ok = ok && !SomaHT::kKnownProfiles[i].fingerprint.Matches(
                               SomaHT::kKnownProfiles[j].fingerprint);
            }
        }
        r.Check(ok, "no two profiles share a fingerprint");
    }

    // The diagnostic primary. kKnownProfiles[0] is what an unrecognised build is
    // measured against, so it has to be the most recently linked entry.
    {
        bool ok = true;
        for (int i = 1; i < SomaHT::kKnownProfileCount; ++i) {
            ok = ok && SomaHT::kKnownProfiles[0].fingerprint.TimeDateStamp >=
                           SomaHT::kKnownProfiles[i].fingerprint.TimeDateStamp;
        }
        r.Check(ok, "the first profile is the newest, so it is the right diagnostic primary");
    }

    return r.Failures();
}
