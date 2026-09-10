// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_hook.h"

#include "engine_memory.h"
#include "game_offsets.h"
#include "gameplay_gate.h"
#include "hook_install.h"
#include "mod.h"

#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"

namespace SomaHT {
namespace {

// The prefix every line this file writes to the log carries.
constexpr const char* kCategory = "Camera";

// One hook, and it is a window rather than a value substitution.
//
// cScene::RenderViewport IS the render phase for one viewport: it reads the
// camera off the viewport, asks it for a frustum - which is what rebuilds the
// cached view matrix - draws the world through it and then draws the GUI over
// the top. Everything the game asks the camera OUTSIDE this call is gameplay:
// the interaction ray cUtility_PickBasics casts along GetPosition() and
// GetForward(), the sound listener, enemy sight, every script that reads the
// camera in its update. So the head pose is written into cCamera's own angle
// and position fields on the way in and taken straight back out on the way out,
// and nothing but this one render ever sees it. Why the fields rather than the
// view matrix is camera_pose.h's to explain.
using RenderViewport_t = void(__fastcall*)(void* scene, void* viewport, float frameTime,
                                           unsigned int flags);

RenderViewport_t oRenderViewport = nullptr;
void* g_renderViewport = nullptr;

void __fastcall DetourRenderViewport(void* scene, void* viewport, float frameTime,
                                     unsigned int flags) {
    void* camera =
        viewport == nullptr
            ? nullptr
            : const_cast<char*>(engine::ReadPointer(viewport, offsets::Active().viewportCamera));

    const bool tracked = camera != nullptr && Mod::Instance().BeginViewport(viewport, camera);
    oRenderViewport(scene, viewport, frameTime, flags);
    if (tracked) Mod::Instance().EndViewport(camera);
}

// MinHook, brought up by whichever hook installs first. This is the one that
// does: InstallHooks stops here when it fails, so no other detour is reached.
bool InitializeHookEngine() {
    using namespace cameraunlock::hooks;
    const HookStatus s = HookManager::Instance().Initialize();
    if (s == HookStatus::Ok || s == HookStatus::ErrorAlreadyInitialized) return true;
    cameraunlock::logging::Line("%s: MinHook init failed (%s)", kCategory,
                                HookStatusToString(s));
    return false;
}

}  // namespace

bool InstallCameraHook() {
    // gpBase first: without it the mod cannot tell the player's viewport from a
    // security monitor's, and a hook that cannot make that distinction is worse
    // than no hook at all.
    void* getClosestEntity = detour::Find(kCategory, "cLuxMapHelper::GetClosestEntity",
                                          offsets::Active().getClosestEntityPattern);
    if (getClosestEntity == nullptr) return false;
    if (!gate::Initialize(getClosestEntity)) return false;

    void* renderViewport = detour::Find(kCategory, "cScene::RenderViewport",
                                        offsets::Active().renderViewportPattern);
    if (renderViewport == nullptr) return false;

    if (!InitializeHookEngine()) return false;

    g_renderViewport = detour::Install(kCategory, "cScene::RenderViewport", renderViewport,
                                       reinterpret_cast<void*>(&DetourRenderViewport),
                                       reinterpret_cast<void**>(&oRenderViewport));
    if (g_renderViewport == nullptr) return false;

    cameraunlock::logging::Line("%s: hook installed (render-phase injection).", kCategory);
    return true;
}

void RemoveCameraHook() { detour::Remove(g_renderViewport); }

}  // namespace SomaHT
