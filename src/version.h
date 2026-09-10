// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace SomaHT {

// Single source of truth for the mod version. scripts/Version.psm1 parses
// VERSION_MAJOR/MINOR/PATCH out of this file for every script that needs the
// number, and packaging refuses to build a ZIP unless VERSION_STRING,
// CMakeLists.txt's project() version and install.cmd's MOD_VERSION all agree
// with it.
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

constexpr const char* VERSION_STRING = "0.0.0";

}  // namespace SomaHT
