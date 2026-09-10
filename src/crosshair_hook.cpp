// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "crosshair_hook.h"

#include "aim_projection.h"

#include "engine_memory.h"
#include "game_offsets.h"
#include "gameplay_gate.h"
#include "hook_install.h"
#include "hpl_math.h"
#include "mod.h"

#include "cameraunlock/logging/file_log.h"

// SOMA draws its crosshair at a fixed point in the HUD set, so it marks what the
// interaction ray is on only while the rendered view IS the aim. Once the head
// turns or leans, the view and the ray are different and the crosshair points at
// nothing.
//
// Two detours and one plain call:
//   cLuxMapHelper::GetClosestEntity - the worker behind the script-facing
//       cLux_GetClosestEntity, which is what cUtility_PickBasics casts along the
//       player camera every update tick. Taking its result rather than casting
//       again gives the live depth, measured against exactly the bodies the game
//       accepts, on the tick the render then consumes. Its DIRECTION and depth
//       are taken and its origin is not - the pick steps that origin around a
//       2cm cross, one entry per tick, and aim_ray.h says what that does to a
//       crosshair built on it.
//   cLux_GetClosestBody - CALLED, not hooked, and only when the pick came back
//       empty. The pick only reaches cLuxMap::GetMaxInteractDistance, so past
//       arm's length it says nothing about the surface the crosshair sits over,
//       and a crosshair placed from a no-hit is placed at infinity. The call is
//       made from inside the pick detour so the cast runs in the update tick, in
//       the physics state the game's own cast just ran in.
//   cGuiSet::DrawGfx, the two-argument overload - moves it.
namespace SomaHT {
namespace {

// The prefix every line this file writes to the log carries.
constexpr const char* kCategory = "Crosshair";

struct Vec3f { float x, y, z; };

// The prologue bytes in game_offsets.h say how many arguments arrive and in
// which register class, which is not enough to name them; these two lists are
// the names, and getting one wrong is a call through a mismatched ABI.
//
// cLuxMapHelper::GetClosestEntity(apCallback, avStart, avDir, afRayLength,
// alInteractType, abCheckLineOfSight, afOutDistance, apOutBody, apOutEntity).
// The callback object is the one the AngelScript thunk passes out of gpBase; it
// seeds its own closest distance with kNoHitDistance and that distance is copied
// back out through the seventh argument. The return is whether an entity, rather
// than any body, was found.
using GetClosestEntity_t = char(__fastcall*)(void* callback, const Vec3f* start, const Vec3f* dir,
                                             float rayLength, int interactType,
                                             char checkLineOfSight, float* outDistance,
                                             void** outBody, void** outEntity);

// The closest-body ray (avStart, avDir, afRayLength, afDistance,
// avSurfaceNormal), returning the body it hit or null. The float arrives in
// xmm2 and the surface normal on the stack, which is what the thunk's own
// register moves say - so it is not the callback-driven GetClosestBody
// hps_api.hps declares, whose float is fourth. See game_offsets.h.
using GetClosestBody_t = void*(__fastcall*)(const Vec3f* start, const Vec3f* dir, float rayLength,
                                            float* outDistance, Vec3f* outNormal);

// cGuiSet::DrawGfx(apGfx, avPos). AngelScript passes the object last, so the set
// arrives third.
using DrawGfx_t = void(__fastcall*)(void* gfx, const Vec3f* pos, void* set);

GetClosestEntity_t oGetClosestEntity = nullptr;
DrawGfx_t oDrawGfx = nullptr;

// Not a trampoline. This one is called rather than detoured, so it holds the
// game's own entry point.
GetClosestBody_t g_getClosestBody = nullptr;

void* g_getClosestEntity = nullptr;
void* g_drawGfx = nullptr;

// The player's pick starts at the camera and casts along its forward axis.
// Both halves are needed: the eye tracker's line-of-sight probe starts at the
// same point and aims at whatever entity it is checking, so the origin alone
// does not tell the two apart.
bool IsPlayerRay(const Vec3f& start, const Vec3f& dir) {
    void* cam = gate::PlayerCamera();
    if (cam == nullptr) return false;

    const offsets::Layout& l = offsets::Active();
    const float dx = start.x - engine::ReadFloat(cam, l.camPosX);
    const float dy = start.y - engine::ReadFloat(cam, l.camPosY);
    const float dz = start.z - engine::ReadFloat(cam, l.camPosZ);
    const float tolerance = offsets::kPlayerRayOriginTolerance;
    if (dx * dx + dy * dy + dz * dz > tolerance * tolerance) return false;

    float forward[3];
    math::CameraForward(engine::ReadFloat(cam, l.camYaw), engine::ReadFloat(cam, l.camPitch),
                        forward);
    return dir.x * forward[0] + dir.y * forward[1] + dir.z * forward[2] >=
           offsets::kPlayerRayForwardDot;
}

char __fastcall DetourGetClosestEntity(void* callback, const Vec3f* start, const Vec3f* dir,
                                       float rayLength, int interactType, char checkLineOfSight,
                                       float* outDistance, void** outBody, void** outEntity) {
    const char result = oGetClosestEntity(callback, start, dir, rayLength, interactType,
                                          checkLineOfSight, outDistance, outBody, outEntity);

    if (start == nullptr || dir == nullptr || outDistance == nullptr) return result;
    if (!Mod::Instance().WantsAimRay()) return result;

    if (!IsPlayerRay(*start, *dir)) return result;

    // cLuxMapHelper::GetClosestEntity returns before it seeds or copies out the
    // distance when there is no current map, so on that path the float below is
    // whatever the caller happened to leave there rather than a measurement -
    // and any leftover between zero and the no-hit seed would be taken for a
    // live depth and place the crosshair at a range nothing was measured at.
    // The gate's first test is that same current map, so it is exactly this
    // path it excludes.
    if (!gate::IsGameplay()) return result;

    // kNoHitDistance is the seed the worker writes before casting and leaves in
    // place when the ray reaches nothing, so a distance at it is a miss rather
    // than a surface a kilometre away. A distance at or behind the eye is not a
    // measurement either.
    float distance = *outDistance;
    bool hit = distance > 0.0f && distance < offsets::kNoHitDistance;

    if (!hit) {
        float bodyDistance = 0.0f;
        Vec3f normal{};
        void* body = g_getClosestBody(start, dir, offsets::kAimRayLength, &bodyDistance, &normal);
        if (body != nullptr && bodyDistance > 0.0f) {
            distance = bodyDistance;
            hit = true;
        }
    }

    Mod::Instance().OnAimRay(&dir->x, distance, hit);
    return result;
}

// The set Player.hps draws the crosshair into. Null before gpBase is resolved
// and before cLuxBase has built one, and a null matches no set the game passes
// in - which is how a draw that arrives that early is left alone.
const void* GameHudSet() {
    const char* base = gate::Base();
    return base == nullptr ? nullptr
                           : engine::ReadPointer(base, offsets::Active().baseGameHudSet);
}

void __fastcall DetourDrawGfx(void* gfx, const Vec3f* pos, void* set) {
    float virtualSize[2] = {0.0f, 0.0f};
    CrosshairPlacement placement = CrosshairPlacement::Unchanged;
    float ndcX = 0.0f, ndcY = 0.0f;

    if (pos != nullptr && set != nullptr && set == GameHudSet()) {
        engine::ReadVec2(set, offsets::Active().guiSetVirtualSize, virtualSize);
        if (virtualSize[0] > 0.0f && virtualSize[1] > 0.0f) {
            placement = Mod::Instance().GetCrosshairPlacement(ndcX, ndcY);
        }
    }

    if (placement == CrosshairPlacement::Hide) {
        // The head has turned past the aim. There is no screen position that
        // marks it, and a crosshair clamped to an edge would claim there is.
        return;
    }
    if (placement == CrosshairPlacement::Unchanged) {
        oDrawGfx(gfx, pos, set);
        return;
    }

    Vec3f moved = *pos;
    aim::OffsetGuiPositionByNdc(ndcX, ndcY, virtualSize, moved.x, moved.y);

    oDrawGfx(gfx, &moved, set);
}

}  // namespace

bool InstallCrosshairHook() {
    void* getClosestEntity = detour::Find(kCategory, "cLuxMapHelper::GetClosestEntity",
                                          offsets::Active().getClosestEntityPattern);
    void* getClosestBody = detour::Find(kCategory, "cLux_GetClosestBody",
                                        offsets::Active().getClosestBodyPattern);
    void* drawGfx =
        detour::Find(kCategory, "cGuiSet::DrawGfx", offsets::Active().drawGfxPattern);
    if (getClosestEntity == nullptr || getClosestBody == nullptr || drawGfx == nullptr) {
        return false;
    }

    // Assigned before the pick detour can run, because that detour calls it for
    // every ray the interaction pick did not reach.
    g_getClosestBody = reinterpret_cast<GetClosestBody_t>(getClosestBody);

    g_getClosestEntity = detour::Install(kCategory, "cLuxMapHelper::GetClosestEntity",
                                         getClosestEntity,
                                         reinterpret_cast<void*>(&DetourGetClosestEntity),
                                         reinterpret_cast<void**>(&oGetClosestEntity));
    g_drawGfx = detour::Install(kCategory, "cGuiSet::DrawGfx", drawGfx,
                                reinterpret_cast<void*>(&DetourDrawGfx),
                                reinterpret_cast<void**>(&oDrawGfx));
    if (g_getClosestEntity == nullptr || g_drawGfx == nullptr) {
        RemoveCrosshairHook();
        return false;
    }

    cameraunlock::logging::Line("%s: hooks installed.", kCategory);
    return true;
}

void RemoveCrosshairHook() {
    detour::Remove(g_drawGfx);
    detour::Remove(g_getClosestEntity);
    g_getClosestBody = nullptr;
}

}  // namespace SomaHT
