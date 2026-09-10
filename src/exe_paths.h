// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

#include "cameraunlock/os/module_paths.h"

// Files that sit next to the running game executable: HeadTracking.log and
// HeadTracking.ini. Both are resolved from the EXE rather than from this DLL,
// because the ASI loader may live somewhere else entirely.
//
// The resolution is core's rather than a local GetModuleFileName pair. Core's
// grows its buffer instead of truncating at MAX_PATH - a game installed under a
// long path otherwise resolves to a cut-off path that names nothing - and it
// refuses the substr(0, npos) case where a path with no separator becomes its
// own directory.
namespace SomaHT::paths {

// The directory could not be resolved, or - for the ANSI form - has no
// representation in the process code page. Both come back as the working
// directory, which is the state these paths were already in before core
// resolved them: the log lands beside the process instead of beside the EXE,
// and Config::Load refuses the INI outright because it is not fully qualified.
// Neither may silently read a DIFFERENT file, which is the failure core's
// NarrowToAnsi exists to prevent.
constexpr const char* kUnresolvedDirNarrow = ".";
constexpr const wchar_t* kUnresolvedDirWide = L".";

inline std::wstring NextToHostExe(const wchar_t* name) {
    std::wstring dir = cameraunlock::os::HostExeDirectory();
    if (dir.empty()) dir = kUnresolvedDirWide;
    return dir + L"\\" + name;
}

inline std::string NextToHostExe(const char* name) {
    std::string dir = cameraunlock::os::HostExeDirectoryNarrow();
    if (dir.empty()) dir = kUnresolvedDirNarrow;
    return dir + "\\" + name;
}

}  // namespace SomaHT::paths
