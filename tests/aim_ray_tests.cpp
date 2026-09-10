// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for aim_ray.h.
//
// Two rules live here.
//
// The aim point is measured from the EYE along the cast's direction, never from
// the cast's own origin: cUtility_PickBasics steps that origin around a 2cm
// cross from tick to tick, and a crosshair built on it flicks between five
// positions in a plus with the head and the camera both still.
//
// And the staleness rule. A crosshair placed from a cast the game has stopped
// making keeps the depth it had when the casting stopped, and a fixed depth
// agrees with the shot at exactly one distance and drifts either side of it -
// the classic reticle fault, and one that looks correct in a screenshot taken
// at the wrong range.
//
// The age is measured in ticks of a monotonic counter, not in rendered frames.
// The game casts the pick from its fixed-rate update and draws the crosshair
// once per rendered frame, so the two rates are independent and a window
// counted in frames rejects live rays on any machine rendering faster than the
// engine updates. The cases below use a counter of 1000 ticks per second so the
// numbers read as milliseconds.

#include "aim_ray.h"
#include "test_support.h"

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;
namespace aim = SomaHT::aim;

constexpr float kForward[3] = {0.0f, 0.0f, -1.0f};
constexpr float kEye[3] = {1.0f, 2.0f, 3.0f};

// 1000 ticks to the second, so a tick is a millisecond.
constexpr int64_t kTicksPerSecond = 1000;
constexpr int64_t kMaxAge = static_cast<int64_t>(aim::kMaxRayAgeSeconds * kTicksPerSecond);

aim::Ray MakeRay(int64_t stamp, bool hit) {
    aim::Ray ray;
    const float dir[3] = {1.0f, 0.0f, 0.0f};
    ray.Record(dir, 4.0f, hit, stamp);
    return ray;
}

}  // namespace

int RunAimRayTests() {
    std::cout << "\nAim ray tests\n";
    Report r;

    // The window has to be wide enough to bridge one engine update interval, or
    // a render landing between two ticks is handed no depth. 100ms covers an
    // update rate as low as 10Hz.
    r.Check(kMaxAge >= 50, "the freshness window spans at least one update interval");

    // A ray that has never been recorded is not a ray at zero: with no cast at
    // all the aim has no depth and no direction of its own.
    {
        const aim::Ray ray;
        r.Check(!ray.IsFresh(0, kMaxAge), "an unrecorded ray is never fresh");
        r.Check(!ray.IsFresh(kMaxAge, kMaxAge), "an unrecorded ray is never fresh later either");

        const aim::Target target = aim::ResolveTarget(ray, 1, kMaxAge, kEye, kForward);
        r.Check(!target.isPoint, "no cast resolves to a direction");
        r.Check(NearEqual(target.v[0], kForward[0]) && NearEqual(target.v[1], kForward[1]) &&
                    NearEqual(target.v[2], kForward[2]),
                "no cast falls back to the clean camera forward");
    }

    // Fresh right up to the window and stale one tick past it. The second of
    // these is the case a frame counter got wrong: a render 16ms after the tick
    // that cast the ray - every second frame at 120Hz against a 60Hz update -
    // still has a live measurement to place the crosshair with.
    {
        const aim::Ray ray = MakeRay(1000, true);
        r.Check(ray.IsFresh(1000, kMaxAge), "a ray cast this instant is fresh");
        r.Check(ray.IsFresh(1016, kMaxAge), "a ray one 60Hz render old is fresh");
        r.Check(ray.IsFresh(1000 + kMaxAge, kMaxAge), "a ray at the window's edge is fresh");
        r.Check(!ray.IsFresh(1001 + kMaxAge, kMaxAge), "a ray one tick past the window is stale");
    }

    // A fresh hit is a world point: the EYE advanced along the ray by its
    // distance. Only a point carries parallax, which is what puts the crosshair
    // on the impact under a positional lean.
    {
        const aim::Ray ray = MakeRay(3000, true);
        const aim::Target target = aim::ResolveTarget(ray, 3000, kMaxAge, kEye, kForward);
        r.Check(target.isPoint, "a fresh hit resolves to a point");
        r.Check(NearEqual(target.v[0], 5.0f) && NearEqual(target.v[1], 2.0f) &&
                    NearEqual(target.v[2], 3.0f),
                "the point is the eye advanced along the ray by its distance");
    }

    // A fresh cast that reached nothing has a direction but no depth, so the
    // direction is what gets projected - never the seeded no-hit distance.
    {
        const aim::Ray ray = MakeRay(3000, false);
        const aim::Target target = aim::ResolveTarget(ray, 3000, kMaxAge, kEye, kForward);
        r.Check(!target.isPoint, "a fresh miss resolves to a direction");
        r.Check(NearEqual(target.v[0], 1.0f) && NearEqual(target.v[1], 0.0f) &&
                    NearEqual(target.v[2], 0.0f),
                "the direction is the ray's own, not the camera's");
    }

    // The case this header exists for: a hit the game has stopped refreshing is
    // refused rather than reused at its last depth.
    {
        const aim::Ray ray = MakeRay(3000, true);
        const aim::Target target =
            aim::ResolveTarget(ray, 3000 + kMaxAge * 10, kMaxAge, kEye, kForward);
        r.Check(!target.isPoint, "a stale hit is not reused as a point");
        r.Check(NearEqual(target.v[2], kForward[2]),
                "a stale ray falls back to the clean camera forward, not its own direction");
    }

    // Recording again re-arms it: a player state that resumes casting is fresh
    // from that tick on.
    {
        aim::Ray ray = MakeRay(3000, true);
        const float dir[3] = {0.0f, 1.0f, 0.0f};
        ray.Record(dir, 2.0f, true, 20000);
        r.Check(ray.IsFresh(20000, kMaxAge), "a re-recorded ray is fresh again");

        const aim::Target target = aim::ResolveTarget(ray, 20000, kMaxAge, kEye, kForward);
        r.Check(target.isPoint && NearEqual(target.v[1], 4.0f),
                "the re-recorded cast replaces the previous one entirely");
    }

    return r.Failures();
}
