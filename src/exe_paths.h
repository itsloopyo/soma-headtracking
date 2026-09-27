// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

#include "cameraunlock/os/module_paths.h"

// HeadTracking.log sits next to the running game executable, resolved from the
// EXE rather than from this DLL, because the ASI loader may live somewhere else
// entirely. CameraUnlock.ini sits there too; Mod::LoadConfig resolves it from
// the same core call.
//
// The resolution is core's rather than a local GetModuleFileName pair. Core's
// grows its buffer instead of truncating at MAX_PATH - a game installed under a
// long path otherwise resolves to a cut-off path that names nothing - and it
// refuses the substr(0, npos) case where a path with no separator becomes its
// own directory.
namespace SomaHT::paths {

// The directory could not be resolved. The log lands beside the process
// instead of beside the EXE, which is where it was before core resolved it.
constexpr const wchar_t* kUnresolvedDirWide = L".";

inline std::wstring NextToHostExe(const wchar_t* name) {
    std::wstring dir = cameraunlock::os::HostExeDirectory();
    if (dir.empty()) dir = kUnresolvedDirWide;
    return dir + L"\\" + name;
}

}  // namespace SomaHT::paths
