// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// The interaction ray the game cast along the CLEAN camera during the update
// tick, and the aim the crosshair projection is handed.
//
// Reusing the game's own cast is what keeps the crosshair on the live impact
// point instead of a fixed or smoothed depth. The cost of reuse is that a cast
// can go stale: a player state that stops casting leaves the last ray sitting
// in these fields, and a stale depth is the classic way a crosshair ends up
// agreeing with the shot at one distance and drifting either side of it. Hence
// the timestamp and IsFresh - the staleness rule is a named, testable property
// here rather than an inline expression at the one place that reads it.
//
// The age is WALL-CLOCK rather than a count of rendered frames, because the two
// clocks are not the same one. `PlayerState_Normal.hps` casts the pick from
// `Update(float afTimeStep)`, HPL3's fixed-rate callback, while `Player.hps`
// draws the crosshair from `OnDraw`, which runs once per rendered frame - and
// the render rate is a player setting (SOMA exposes v-sync and refresh rate).
// A window counted in frames is therefore a window whose real duration depends
// on the player's display, and past two renders per update it starts refusing
// casts that are still current. The fall-through costs the parallax term: a
// direction projects to the same screen position from either eye, so it is
// exactly the depth the compensation exists to supply.
//
// Whether any particular machine reaches that ratio has not been measured here;
// what is measured is that a duration is the unit the game's own timing makes
// meaningful.
namespace SomaHT::aim {

struct Ray {
    float   dir[3] = {0.0f, 0.0f, 0.0f};
    float   distance = 0.0f;
    bool    hit = false;
    bool    valid = false;
    int64_t stamp = 0;

    void Record(const float rayDir[3], float rayDistance, bool rayHit, int64_t nowTicks) {
        for (int i = 0; i < 3; ++i) dir[i] = rayDir[i];
        distance = rayDistance;
        hit = rayHit;
        valid = true;
        stamp = nowTicks;
    }

    // Ticks are whatever monotonic unit the caller counts in; only the delta is
    // used, so nothing here needs to know the counter's frequency.
    bool IsFresh(int64_t nowTicks, int64_t maxAgeTicks) const {
        return valid && nowTicks - stamp <= maxAgeTicks;
    }
};

// How long a cast stays the answer. It has to exceed one engine update interval
// or a render landing between two ticks is handed no depth, and it has to stay
// short enough that a player state which has stopped casting drops out within a
// few frames rather than pinning the crosshair to a wall that is no longer
// there. 0.1s is a fifth of the 500ms the tracker pose itself is held fresh
// for, and at 60fps it is about six frames - so a cast that stops coming slides
// the crosshair off rather than snapping it to centre.
constexpr float kMaxRayAgeSeconds = 0.1f;

// What the crosshair projection is asked to place: either a world point the ray
// landed on, or a bare direction.
//
// The distinction matters because only a point carries parallax. With the head
// leaned, the frame is drawn from an eye the shot did not leave from, and a
// direction projects to the same screen position from either - which is exactly
// the drift the compensation exists to remove.
struct Target {
    float v[3] = {0.0f, 0.0f, 0.0f};
    bool  isPoint = false;
};

// @p cleanEye is the position of the camera the frame is drawn from, before the
// head pose moves it, and @p cleanForward its forward axis - the latter used
// only when there is no cast to take a direction from at all.
//
// THE POINT IS MEASURED FROM THE EYE, NOT FROM THE RAY'S OWN ORIGIN, and that
// is the whole reason the eye is a parameter. cUtility_PickBasics does not cast
// down the middle: Utility_PickBasics.hps steps a five-entry offset table -
// (0,0), (+1,0), (-1,0), (0,+1), (0,-1) at gfPickOffsetSize = 0.02m along the
// camera's right and up axes - one entry per update tick, so consecutive ticks
// hand back hit points that sit 2cm apart in a cross. Projected from the
// rendered eye that is a few pixels, and it arrives as the crosshair flicking
// between five positions in a plus, every frame, with the head and the camera
// both perfectly still. Rebuilding the point from the eye along the ray's own
// direction drops the offset - it is perpendicular to the ray - and leaves only
// the range difference between the offset cast and a centred one, which is
// radial and moves the crosshair by well under a pixel.
inline Target ResolveTarget(const Ray& ray, int64_t nowTicks, int64_t maxAgeTicks,
                            const float cleanEye[3], const float cleanForward[3]) {
    const bool fresh = ray.IsFresh(nowTicks, maxAgeTicks);

    Target target;
    if (fresh && ray.hit) {
        for (int i = 0; i < 3; ++i) target.v[i] = cleanEye[i] + ray.dir[i] * ray.distance;
        target.isPoint = true;
        return target;
    }

    // Nothing cast this frame, or the cast reached nothing at all along a
    // kilometre of level. The aim has no depth, so the honest thing to project
    // is the direction, which carries no parallax.
    for (int i = 0; i < 3; ++i) target.v[i] = fresh ? ray.dir[i] : cleanForward[i];
    return target;
}

}  // namespace SomaHT::aim
