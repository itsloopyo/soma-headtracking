// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "windows_lean.h"

#include <atomic>
#include <cstring>

#include "gameplay_gate.h"
#include "engine_memory.h"
#include "game_offsets.h"

#include "cameraunlock/logging/file_log.h"

namespace SomaHT::gate {
namespace {

// gpBase is not exported and has no signature of its own, so it is read out of
// the `mov rax, [rip+gpBase]` that opens cLuxMapHelper::GetClosestEntity.
//
// Written on the init thread and read on the render thread, hence the atomic.
// It is written once and never changes, and MinHook's own barriers order the
// write before the hook that reads it becomes reachable, so this costs nothing
// and removes the last unsynchronised handover between the two.
std::atomic<const char* const*> g_gpBase{nullptr};

const char* PlayerOf(const char* base) {
    return engine::ReadPointer(base, offsets::Active().basePlayer);
}

// cLux_GetGamePaused(). An unresolvable handler reports PAUSED, so a layout the
// mod cannot read leaves the game alone rather than swinging the camera behind
// a menu. Every other indirection in this file fails the same way.
bool GamePaused(const char* base) {
    const char* gameHandler = engine::ReadPointer(base, offsets::Active().baseGameHandler);
    if (gameHandler == nullptr) return true;
    return engine::ReadByte(gameHandler, offsets::Active().gameHandlerPaused) != 0;
}

// The RIP-relative displacement is signed and comes out of bytes the pattern
// wildcarded, so a scan that matched the wrong place produces an arbitrary
// address. Dereferencing that on the render thread is a crash before the main
// menu draws, so the target has to land inside the executable's own image.
bool InsideHostImage(const void* address) {
    const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if (base == nullptr) return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    const auto* candidate = static_cast<const unsigned char*>(address);
    return candidate >= base &&
           candidate + sizeof(void*) <= base + nt->OptionalHeader.SizeOfImage;
}

}  // namespace

bool Initialize(const void* getClosestEntityFn) {
    const char* insn =
        static_cast<const char*>(getClosestEntityFn) + offsets::kGpBaseInsnOffset;
    int32_t disp = 0;
    std::memcpy(&disp, insn + offsets::kGpBaseDispOffset, sizeof(disp));
    const char* const* candidate =
        reinterpret_cast<const char* const*>(insn + offsets::kGpBaseInsnLength + disp);

    if (!InsideHostImage(candidate)) {
        cameraunlock::logging::Line(
            "Gate: gpBase resolved to %p, which is outside the executable's image - "
            "refusing it.",
            static_cast<const void*>(candidate));
        return false;
    }

    g_gpBase.store(candidate, std::memory_order_release);
    cameraunlock::logging::Line("Gate: gpBase at %p", static_cast<const void*>(candidate));
    return true;
}

const char* Base() {
    const char* const* slot = g_gpBase.load(std::memory_order_acquire);
    return slot ? *slot : nullptr;
}

void* PlayerCamera() {
    const char* base = Base();
    if (base == nullptr) return nullptr;
    const char* player = PlayerOf(base);
    if (player == nullptr) return nullptr;
    return const_cast<char*>(engine::ReadPointer(player, offsets::Active().playerCamera));
}

// cScene::Render walks every active viewport, so the render hook alone says
// nothing about whether the player is playing. SOMA renders the world behind its
// menus, renders it into a security monitor's texture through a second viewport
// with its own camera, and renders it during scripted sequences that take the
// camera away from the player entirely.
//
// The test below is the game's own. Player.hps draws its crosshair under exactly
// these conditions - a map is loaded, the player is active, the game is not
// paused, and the camera this viewport is rendering is the player's own - so
// anything that fails them is by the game's definition not the player looking at
// the world. The viewport check is the same statement as the camera one for the
// stock game and is kept because a script may hand a second viewport the
// player's camera.
bool IsGameplay(const void* viewport, const void* camera) {
    // Every indirection here is the game's own lifecycle, not an impossible
    // case: the mod initialises from DllMain, long before cLuxBase exists, and
    // the handlers and the map hang off it later still.
    const char* base = Base();
    if (base == nullptr) return false;

    const offsets::Layout& l = offsets::Active();
    const char* mapHandler = engine::ReadPointer(base, l.baseMapHandler);
    if (mapHandler == nullptr) return false;
    if (engine::ReadPointer(mapHandler, l.mapHandlerCurrentMap) == nullptr) return false;
    if (engine::ReadPointer(mapHandler, l.mapHandlerViewport) != viewport) return false;

    const char* player = PlayerOf(base);
    if (player == nullptr) return false;
    if (engine::ReadByte(player, l.playerActive) == 0) return false;
    if (engine::ReadPointer(player, l.playerCamera) != camera) return false;

    return !GamePaused(base);
}

bool IsGameplay() {
    const char* base = Base();
    if (base == nullptr) return false;
    const char* mapHandler = engine::ReadPointer(base, offsets::Active().baseMapHandler);
    if (mapHandler == nullptr) return false;

    const void* viewport = engine::ReadPointer(mapHandler, offsets::Active().mapHandlerViewport);
    const void* camera = PlayerCamera();
    if (viewport == nullptr || camera == nullptr) return false;
    return IsGameplay(viewport, camera);
}

bool GetHudMetrics(HudMetrics& out) {
    const char* base = Base();
    if (base == nullptr) return false;
    const offsets::Layout& l = offsets::Active();
    const char* guiSet = engine::ReadPointer(base, l.baseGameHudSet);
    if (guiSet == nullptr) return false;

    engine::ReadVec2(guiSet, l.guiSetVirtualSize, out.guiSetVirtualSize);
    engine::ReadVec2(base, l.baseHudVirtualCenterSize, out.centerSize);
    engine::ReadVec2(base, l.baseHudCenterScreenSize, out.centerScreenSize);
    engine::ReadVec2(base, l.baseHudVirtualSize, out.virtualSize);
    return true;
}

}  // namespace SomaHT::gate
