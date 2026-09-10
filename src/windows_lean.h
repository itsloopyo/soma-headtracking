// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// <windows.h>, with the two macros that have to be defined before it.
//
// WIN32_LEAN_AND_MEAN drops winsock.h from what windows.h pulls in, and
// cameraunlock-core's socket_types.h includes WinSock2.h - the two declare the
// same names and including both is a wall of redefinition errors rather than a
// warning. NOMINMAX keeps the min and max macros out of the way of the standard
// functions of those names.
//
// The mod target defines both from CMake as well. This header is what makes a
// translation unit compiled outside that target - the tests - read the same
// windows.h the .asi is built against, and it is why no .cpp in this mod
// carries its own copy of the pair.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
