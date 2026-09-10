// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "gui_input_hook.h"

#include "game_offsets.h"
#include "hook_install.h"
#include "mod.h"

#include "cameraunlock/logging/file_log.h"

namespace SomaHT {
namespace {

// The prefix every line this file writes to the log carries.
constexpr const char* kCategory = "Gui input";
constexpr const char* kFunctionName = "cLuxGuiHandler::UpdateInput";

// cLuxGuiHandler::UpdateInput(this). Called once per input tick from
// cLuxInputHandler's update, and again the moment a screen takes focus.
using UpdateGuiInput_t = void(__fastcall*)(void* guiHandler);

UpdateGuiInput_t oUpdateGuiInput = nullptr;
void* g_updateGuiInput = nullptr;

void __fastcall DetourUpdateGuiInput(void* guiHandler) {
    void* camera = Mod::Instance().BeginGuiInput();
    oUpdateGuiInput(guiHandler);
    if (camera != nullptr) Mod::Instance().EndGuiInput(camera);
}

// Two spellings, one function, and the reason this is a pair of silent scans
// rather than one detour::Find: everywhere else the two shipped executables are
// the same code linked at different addresses, but this one is built with a
// stack cookie in Soma.exe and without in Soma_NoSteam.exe, so their prologues
// do not agree. Each pattern matches exactly once in its own executable and not
// at all in the other, so a miss on the first is the ordinary case and has no
// business in the log.
void* FindUpdateInput() {
    const offsets::Layout& l = offsets::Active();
    void* target = detour::Scan(l.guiInputPatternSteam);
    if (target == nullptr) target = detour::Scan(l.guiInputPatternNoSteam);

    if (target == nullptr) {
        cameraunlock::logging::Line("%s: %s is not where this build puts it.", kCategory,
                                    kFunctionName);
        return nullptr;
    }
    cameraunlock::logging::Line("%s: %s found at %p", kCategory, kFunctionName, target);
    return target;
}

}  // namespace

bool InstallGuiInputHook() {
    void* target = FindUpdateInput();
    if (target == nullptr) return false;

    g_updateGuiInput = detour::Install(kCategory, kFunctionName, target,
                                       reinterpret_cast<void*>(&DetourUpdateGuiInput),
                                       reinterpret_cast<void**>(&oUpdateGuiInput));
    if (g_updateGuiInput == nullptr) return false;

    cameraunlock::logging::Line("%s: hook installed.", kCategory);
    return true;
}

void RemoveGuiInputHook() { detour::Remove(g_updateGuiInput); }

}  // namespace SomaHT
