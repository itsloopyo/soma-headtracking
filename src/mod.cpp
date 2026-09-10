// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "windows_lean.h"

#include <string>

#include "mod.h"
#include "aim_projection.h"
#include "camera_fov.h"
#include "camera_hook.h"
#include "camera_pose.h"
#include "crosshair_hook.h"
#include "exe_paths.h"
#include "eye_tracking.h"
#include "gameplay_gate.h"
#include "gui_input_hook.h"
#include "hpl_math.h"
#include "tracking_settings.h"

#include "cameraunlock/input/chord_hotkeys.h"
#include "cameraunlock/logging/file_log.h"

namespace SomaHT {
namespace {

// How often the hotkey thread samples GetAsyncKeyState. 16ms is one 60Hz frame,
// which is short enough that a tap between two frames is not missed.
constexpr unsigned kHotkeyPollIntervalMs = 16;

// The first frame has no previous timestamp to subtract, so it is charged one
// 60Hz interval. The bounds either side cover a frame the mod did not see the
// start of: a level load or an alt-tab leaves a gap of seconds, and a gap
// pushed through the smoothing exponent would snap the pose to the tracker in
// one frame.
constexpr float kDefaultFrameSeconds = 0.016f;
constexpr float kMinFrameSeconds = 0.0001f;
constexpr float kMaxFrameSeconds = 0.1f;

int64_t NowTicks() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

// Seconds between two counter readings, bounded as above.
//
// It is the DELTA that is converted, never a single reading: the counter runs
// from boot, and a raw count multiplied by a million overflows a signed 64-bit
// after about ten days of uptime. Past that every frame's interval would be
// garbage, which reads in game as smoothing that will not settle.
double TicksPerSecond() {
    static const double ticks = [] {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return static_cast<double>(freq.QuadPart);
    }();
    return ticks;
}

float FrameSeconds(int64_t fromTicks, int64_t toTicks) {
    const double seconds = static_cast<double>(toTicks - fromTicks) / TicksPerSecond();
    if (seconds > kMaxFrameSeconds) return kMaxFrameSeconds;
    if (seconds < kMinFrameSeconds) return kMinFrameSeconds;
    return static_cast<float>(seconds);
}

// The interaction ray's freshness window, in counter ticks. Resolved once:
// aim_ray.h works in whatever monotonic unit the caller counts in, and this is
// where that unit is fixed to QPC.
int64_t MaxRayAgeTicks() {
    static const int64_t ticks =
        static_cast<int64_t>(TicksPerSecond() * aim::kMaxRayAgeSeconds);
    return ticks;
}

const char* ModeName(cameraunlock::TrackingMode mode) {
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition: return "6DOF";
        case cameraunlock::TrackingMode::RotationOnly:        return "Rotation only";
        case cameraunlock::TrackingMode::PositionOnly:        return "Position only";
    }
    return "Unknown";
}

}  // namespace

// Deliberately leaked. A function-local static would register ~Mod in the
// module's onexit table, and the CRT runs that table from DLL_PROCESS_DETACH -
// unconditionally, whether or not the process is exiting - under the loader
// lock. By then Windows has terminated the receiver and hotkey threads at
// whatever instruction each was on, so joining them or freeing what they were
// holding deadlocks the game on the way out. The OS reclaims all of it anyway.
Mod& Mod::Instance() {
    static Mod* instance = new Mod();
    return *instance;
}

void Mod::LoadConfig() {
    const std::string path = paths::NextToHostExe("HeadTracking.ini");
    if (m_config.Load(path.c_str())) {
        cameraunlock::logging::Line("Config loaded from %s", path.c_str());
    } else {
        cameraunlock::logging::Line("No config at %s - using defaults.", path.c_str());
    }
    m_worldSpaceYaw.store(m_config.worldSpaceYaw);
}

void Mod::ApplyTrackingSettings() {
    m_session.GetProcessor().SetSensitivity(SensitivityFrom(m_config));

    static_assert(cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>::kHasRemoteConnection,
                  "receiver must classify connection locality, or smoothing "
                  "silently stays on the local parameter forever");
    m_session.SetLocalSmoothing(m_config.localSmoothing);
    m_session.SetRemoteSmoothing(m_config.remoteSmoothing);
    // Before the two smoothing setters or after them, either way:
    // SetPositionSettings recomposes the session's own two values onto the
    // struct, which is why PositionFrom does not carry them.
    m_session.SetPositionSettings(PositionFrom(m_config));
    if (!m_config.positionEnabled) {
        m_session.SetMode(cameraunlock::TrackingMode::RotationOnly);
    }
}

// The gameplay gate goes with the camera hook rather than being optional:
// without gpBase the mod cannot tell the player's viewport from a security
// monitor's render target, or from the world still drawing behind a menu, and it
// would swing all of them around.
//
// The crosshair hooks ARE optional. Without them head tracking still works; the
// crosshair just stops marking what the interaction ray is on once the head
// leaves centre. So is the eye tracking one: without it a player with a Tobii
// has SOMA's Extended View steering the view alongside the head pose.
bool Mod::InstallHooks() {
    if (!InstallCameraHook()) {
        cameraunlock::logging::Line("Camera hook not installed - head tracking disabled.");
        return false;
    }
    if (!InstallCrosshairHook()) {
        cameraunlock::logging::Line(
            "Crosshair compensation not installed - the crosshair will stay at screen centre.");
    }
    if (!InstallGuiInputHook()) {
        cameraunlock::logging::Line(
            "Terminal mouse ray not compensated - clicking a computer screen will be offset "
            "while the head is off centre.");
    }
    if (m_config.suppressEyeTracking && !eyetrack::InstallSuppression()) {
        cameraunlock::logging::Line(
            "Eye tracking: not held off - SOMA's own Extended View will move the camera too.");
    }
    return true;
}

// A failed bind is not fatal and must not disable the mod: UdpReceiver::Start
// spawns a supervisor thread whether or not the first bind succeeded and
// re-attempts, so a port still held by a game that is closing is reclaimed
// within a tick of that process exiting.
void Mod::StartReceiver() {
    m_receiver.SetLog(
        [](const std::string& s) { cameraunlock::logging::Line("UDP: %s", s.c_str()); });
    if (m_receiver.Start(static_cast<uint16_t>(m_config.udpPort))) {
        cameraunlock::logging::Line("UDP receiver listening on port %d.", m_config.udpPort);
    } else {
        cameraunlock::logging::Line("UDP receiver retrying on port %d...", m_config.udpPort);
    }
}

void Mod::RegisterHotkeys() {
    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;

    // Nav-cluster keys, suppressed while the chord modifier is held so the
    // chord path is the sole trigger for Ctrl+Shift combinations.
    m_hotkeys.AddHotkey(m_config.toggleKey,       NavGuarded([] { Instance().Toggle(); }));
    m_hotkeys.AddHotkey(m_config.trackingModeKey, NavGuarded([] { Instance().CycleMode(); }));
    m_hotkeys.AddHotkey(m_config.yawModeKey,      NavGuarded([] { Instance().ToggleYawMode(); }));

    m_hotkeys.AddHotkey('Y', ChordGuarded([] { Instance().Toggle(); }));
    m_hotkeys.AddHotkey('G', ChordGuarded([] { Instance().CycleMode(); }));
    m_hotkeys.AddHotkey('H', ChordGuarded([] { Instance().ToggleYawMode(); }));

    m_hotkeys.Start(kHotkeyPollIntervalMs);
    cameraunlock::logging::Line(
        "Hotkeys: 0x%X=Toggle 0x%X=CycleMode 0x%X=YawMode (+ Ctrl+Shift+Y/G/H).",
        m_config.toggleKey, m_config.trackingModeKey, m_config.yawModeKey);
}

void Mod::Initialize() {
    if (m_initialized.load()) return;
    cameraunlock::logging::Line("Initializing mod...");

    LoadConfig();
    ApplyTrackingSettings();
    if (!InstallHooks()) {
        // Dormant means dormant: no socket bound, no hotkeys registered, no
        // state a player could toggle. The game runs vanilla and the log says
        // why one line above this.
        cameraunlock::logging::Line("Mod dormant - the game is running unmodified.");
        return;
    }
    m_cameraHooked.store(true);
    StartReceiver();
    RegisterHotkeys();

    m_enabled.store(m_config.autoEnable);
    m_initialized.store(true);
    cameraunlock::logging::Line("Mod initialized (tracking %s, %s yaw).",
                                m_enabled.load() ? "ON" : "OFF",
                                m_worldSpaceYaw.load() ? "world-space" : "camera-local");
}

void Mod::Shutdown() {
    if (!m_initialized.load()) return;
    m_hotkeys.Stop();
    m_receiver.Stop();
    eyetrack::RemoveSuppression();
    RemoveGuiInputHook();
    RemoveCrosshairHook();
    RemoveCameraHook();
    m_initialized.store(false);
    cameraunlock::logging::Line("Mod shut down.");
}

void Mod::Toggle() {
    if (!m_cameraHooked.load()) return;
    const bool now = !m_enabled.load();
    m_enabled.store(now);
    cameraunlock::logging::Line("Head tracking %s", now ? "ENABLED" : "DISABLED");
}

// Requested here, run in ComputeFrame. SetMode resets the interpolator and the
// position processor's smoothing state, and the render thread is inside
// Update() reading exactly those fields; doing it from the hotkey thread is a
// data race, not a benign one.
void Mod::CycleMode() {
    // The setting is expressed by parking the session in RotationOnly, and the
    // cycle would walk straight back out of it - two presses and a player who
    // turned positional tracking off in the ini is in full 6DOF again, with
    // nothing saying so and no way back but to press until they land on the
    // state they configured. There is no positional half to cycle through here,
    // so the key does nothing and says why.
    if (!m_config.positionEnabled) {
        cameraunlock::logging::Line(
            "Tracking mode: rotation only - [Position] Enabled is false in the config, so there "
            "is no positional half to cycle to.");
        return;
    }
    m_cycleModeRequest.Request();
}

void Mod::ToggleYawMode() {
    const bool now = !m_worldSpaceYaw.load();
    m_worldSpaceYaw.store(now);
    cameraunlock::logging::Line("Yaw mode: %s", now ? "world-space" : "camera-local");
}

void Mod::ComputeFrame() {
    if (m_cycleModeRequest.Consume()) {
        cameraunlock::logging::Line("Tracking mode: %s", ModeName(m_session.CycleMode()));
    }

    const int64_t now = NowTicks();
    const float dt =
        m_lastFrameTicks == 0 ? kDefaultFrameSeconds : FrameSeconds(m_lastFrameTicks, now);
    m_lastFrameTicks = now;

    // The session re-reads the receiver's locality every Update, so a tracker
    // swap (local OpenTrack to a phone on WiFi, or back) picks up the other
    // smoothing parameter without a restart. This only reports the change, and
    // only once a packet has parsed: the flag starts at false, so reporting it
    // earlier reads as proof a local tracker connected when nothing arrived.
    const bool fresh = m_session.Update(dt);
    if (!fresh) {
        m_rotValid = false;
        m_posValid = false;
        return;
    }

    m_diag.ConnectionLocality(m_session.IsRemoteConnection(), m_config.localSmoothing,
                              m_config.remoteSmoothing);
    m_diag.ConnectionState(m_receiver.IsReceiving());
    m_rotValid = m_session.GetRotation(m_yaw, m_pitch, m_roll);
    m_posValid = m_session.GetPositionOffset(m_offsetX, m_offsetY, m_offsetZ);
    m_diag.FirstPose(m_yaw, m_pitch, m_roll);
}

void Mod::OnAimRay(const float dir[3], float distance, bool hit) {
    m_ray.Record(dir, distance, hit, NowTicks());
}

// The field of view is applied whether or not tracking is running: it is the
// mod's other setting, not part of the head pose. fov::Apply reports the
// half-field tangents of the field the frame will actually be drawn with, which
// is what the crosshair has to be projected through - reading them before the
// override would project through a field the frame does not use.
//
// The same read gives the zoom factor, which is why this runs ahead of the pose
// composition at both injection points rather than after it.
bool Mod::ApplyFieldOfView(void* camera, camera::InjectionWindow& window) {
    const float baseDegrees = fov::BaseDegrees(gate::Base());
    const float scale = fov::ScaleFor(m_config.fieldOfView, baseDegrees);
    fov::Field field{};
    const bool fieldOk = fov::Apply(camera, scale, field, window.savedFov);
    window.fovOverridden = fieldOk && scale != 1.0f;
    if (!fieldOk) {
        // Nothing was written and nothing can be projected, so the pose is left
        // at 1:1 rather than scaled against a field that was not read.
        m_zoom = 1.0f;
        return false;
    }

    m_tanX = field.tanX;
    m_tanY = field.tanY;
    m_zoom = fov::ZoomFactor(field, fov::NormalPlayRadians(m_config.fieldOfView, baseDegrees));
    m_diag.FieldBasis(field, baseDegrees, m_config.fieldOfView, m_zoom);
    return true;
}

// The composition itself is in tracking_settings.h, where it has a test. This
// is the packing of the frame's own state into it.
math::CameraPose Mod::ComposeTrackedPose(const math::CameraPose& clean) const {
    HeadPose head;
    head.yaw = m_yaw;
    head.pitch = m_pitch;
    head.roll = m_roll;
    head.rotValid = m_rotValid;
    head.offsetX = m_offsetX;
    head.offsetY = m_offsetY;
    head.offsetZ = m_offsetZ;
    head.posValid = m_posValid;
    return SomaHT::ComposeTrackedPose(m_config, clean, head, m_zoom, m_worldSpaceYaw.load());
}

bool Mod::BeginViewport(const void* viewport, void* camera) {
    m_diag.HookRunning();

    if (!gate::IsGameplay(viewport, camera)) return false;
    m_diag.GameplayEntry();

    // The frame the crosshair will be drawn over starts here, so whatever the
    // previous one captured stops being an answer. Cleared behind the gameplay
    // gate rather than at the top: a security monitor's viewport renders in the
    // same frame as the player's and must not wipe the player's basis.
    m_renderViewValid = false;

    ComputeFrame();

    m_render.clean = camera::ReadPose(camera);

    // Ahead of the composition, not after it: the pose is scaled by the field
    // this call reports, so composing first would scale by the previous frame's.
    const bool fieldOk = ApplyFieldOfView(camera, m_render);

    const bool track = m_enabled.load() && (m_rotValid || m_posValid);
    if (track) {
        m_trackedPose = ComposeTrackedPose(m_render.clean);
        camera::WritePose(camera, m_trackedPose);
    }

    m_render.poseWritten = track;

    // The crosshair can only be placed through a field the frame can be
    // projected through, so a camera fov::Apply refused is a frame drawn with
    // the crosshair left where the game put it. Everything in the block below
    // exists to place it, so it all hangs off the same answer.
    m_injecting = track && fieldOk;
    if (m_injecting) {
        float cleanForward[3];
        math::CameraForward(m_render.clean.yaw, m_render.clean.pitch, cleanForward);
        m_aim = aim::ResolveTarget(m_ray, NowTicks(), MaxRayAgeTicks(), m_render.clean.pos,
                                   cleanForward);
        m_diag.AimDepth(m_aim.isPoint, m_ray.valid);

        // Composed here, and kept: the crosshair draw that consumes it happens
        // after this render has returned and the clean pose has gone back.
        math::PoseBasis(m_trackedPose, m_renderView);
        m_renderViewValid = true;
    }

    return m_render.poseWritten || m_render.fovOverridden;
}

void Mod::EndViewport(void* camera) {
    if (m_injecting) {
        // The render has been through cCamera::GetFrustum by now, which is what
        // rebuilds m_mtxView from the angles written on the way in, so this is
        // the last moment the engine's own answer for the rendered basis can be
        // read - the clean pose goes back a few lines down.
        //
        // It is read only to check that the write took. The crosshair projects
        // through PoseBasis instead; see hpl_math.h for why the matrix is the
        // wrong basis for a crosshair DELTA on this game.
        float view[16];
        camera::ReadViewMatrix(camera, view);
        math::ViewBasis engineBasis{};
        math::DecomposeView(view, engineBasis);
        m_diag.ViewMatrixRebuilt(engineBasis, m_trackedPose, m_render.clean);
    }

    m_render.Restore(camera);
    m_injecting = false;
}

// The mouse position a focused screen is picked with is not a screen position:
// cLuxGuiHandler unprojects the mouse's pixel through the player camera, casts
// the ray at the screen's mesh and hands the point it lands on to that screen's
// cImGui. So the camera this call sees is what decides which widget is under
// the pointer, and it runs in the update tick with the clean pose on the camera
// while the frame the player is looking at was drawn through the head pose.
//
// Writing the pose here is not a hole in the clean-camera rule. It is put back
// before this call returns, so nothing the game does with the camera afterwards
// - the interaction ray, enemy sight, the sound listener - sees it.
void* Mod::BeginGuiInput() {
    // Inside the render window the pose is already on the camera, and writing a
    // second one would restore the wrong pose on the way out. The input update
    // is not called from there, so the one line says the shape of the frame is
    // not what this hook was written against.
    if (m_render.poseWritten) {
        m_diag.GuiInputInsideRender();
        return nullptr;
    }

    // Re-entry would read the pose this window already wrote as its own clean
    // one, restore that on the way out, and leave the head pose on the camera
    // for the rest of the update tick - where every gameplay query in SOMA
    // would then run against a camera the player never pointed. The outer
    // window's Restore has cleared its flags by then and puts nothing back, so
    // the state does not recover until the game is restarted.
    if (m_gui.poseWritten) return nullptr;

    if (!m_enabled.load() || !(m_rotValid || m_posValid)) return nullptr;
    if (!gate::IsGameplay()) return nullptr;

    void* camera = gate::PlayerCamera();
    if (camera == nullptr) return nullptr;

    // The field the frame was drawn with, for the same reason: the ray through
    // the mouse's pixel is only the one the player is looking along if it is
    // unprojected through the projection the frame used. First, because the
    // pose written below is scaled by it.
    ApplyFieldOfView(camera, m_gui);

    m_gui.clean = camera::ReadPose(camera);
    camera::WritePose(camera, ComposeTrackedPose(m_gui.clean));
    m_gui.poseWritten = true;

    m_diag.GuiInputRunning();
    return camera;
}

void Mod::EndGuiInput(void* camera) { m_gui.Restore(camera); }

CrosshairPlacement Mod::GetCrosshairPlacement(float& ndcX, float& ndcY) {
    if (!m_renderViewValid || !m_config.crosshairCompensation) {
        return CrosshairPlacement::Unchanged;
    }

    // The basis is deliberately kept past the render it was composed in - the
    // crosshair is drawn after cScene::RenderViewport returns - but BeginViewport
    // can only clear it on a frame that renders a gameplay viewport, so on a
    // frame that renders none it is last frame's answer. This asks the three
    // questions the no-argument gate can still answer outside a viewport: a map
    // is loaded, the player is active, and the game is not paused. It does not
    // catch a camera swap - the viewport and camera comparisons degenerate to
    // tautologies on this path - and it is not a re-gate on being inside the
    // render window, which would put the crosshair back at screen centre.
    if (!gate::IsGameplay()) return CrosshairPlacement::Unchanged;

    // A point is relative to the eye the frame is drawn from; a direction is
    // already the vector to project.
    float aimVector[3];
    for (int i = 0; i < 3; ++i) {
        aimVector[i] = m_aim.isPoint ? m_aim.v[i] - m_renderView.eye[i] : m_aim.v[i];
    }

    if (!aim::ProjectAimToNdc(aimVector, m_renderView.fwd, m_renderView.right, m_renderView.up,
                              m_tanX, m_tanY, ndcX, ndcY)) {
        return CrosshairPlacement::Hide;
    }
    return CrosshairPlacement::Offset;
}

}  // namespace SomaHT
