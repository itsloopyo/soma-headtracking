// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Compile-only, and STRICT. Every header the mod ships in one TU at the warning
// level the release build uses.
//
// Two things this catches that nothing else does: a header no test file
// includes (gameplay_gate.h and the two hook declarations are consumed only by
// the mod library, which cannot be built without a game-side source that is not
// written yet), and a problem that appears only when two headers meet, which is
// invisible to a build that includes them separately.

#include "aim_projection.h"
#include "aim_ray.h"
#include "build_profile.h"
#include "camera_fov.h"
#include "camera_hook.h"
#include "camera_pose.h"
#include "config.h"
#include "crosshair_hook.h"
#include "engine_memory.h"
#include "exe_paths.h"
#include "eye_tracking.h"
#include "game_offsets.h"
#include "gameplay_gate.h"
#include "gui_input_hook.h"
#include "hook_install.h"
#include "hpl_math.h"
#include "injection_window.h"
#include "mod.h"
#include "mod_diagnostics.h"
#include "tracking_settings.h"
#include "version.h"
#include "window_centering.h"
