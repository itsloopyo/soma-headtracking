// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <atomic>
#include <cstdint>

#include "aim_ray.h"
#include "config.h"
#include "hpl_math.h"
#include "injection_window.h"
#include "mod_diagnostics.h"
#include "cameraunlock/input/deferred_actions.h"
#include "cameraunlock/input/hotkey_poller.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"

namespace SomaHT {

// Where the crosshair draw should put the sprite this frame.
enum class CrosshairPlacement {
    Unchanged,  // tracking is off, or this draw is not inside a tracked render
    Offset,     // draw at the game's position plus the returned NDC offset
    Hide,       // the aim is at or behind the rendered view; there is nowhere to draw
};

class Mod {
public:
    static Mod& Instance();

    void Initialize();
    void Shutdown();

    // False when Initialize left the mod dormant: no hooks installed, no
    // socket bound, nothing of the game touched.
    bool IsInitialized() const { return m_initialized.load(); }

    void Toggle();
    void CycleMode();
    void ToggleYawMode();

    // Called from the cScene::RenderViewport hook for every viewport the frame
    // renders. Returns true when @p camera now carries the head pose and
    // EndViewport must be called once the render is done.
    bool BeginViewport(const void* viewport, void* camera);
    void EndViewport(void* camera);

    // Called from the interaction-ray hook with the direction the game cast
    // along in the update tick and the distance that came back. Reusing the
    // game's own cast is what keeps the crosshair on the live impact point
    // instead of a fixed or smoothed depth.
    //
    // The cast's ORIGIN is deliberately not taken: the game steps it around a
    // 2cm cross from tick to tick, and the crosshair has to be placed from the
    // eye - see aim_ray.h.
    void OnAimRay(const float dir[3], float distance, bool hit);

    // Called from the crosshair hook. SOMA draws its HUD after every viewport
    // render rather than inside one, so this runs with the injection window
    // already closed and answers from the basis EndViewport captured.
    CrosshairPlacement GetCrosshairPlacement(float& ndcX, float& ndcY);

    // Called from the gui input hook, which runs in the update tick with the
    // injection window closed. Puts the head pose back on the player camera for
    // the length of that call so the engine casts a focused screen's mouse ray
    // through the camera the frame was drawn with, and returns the camera to
    // hand to EndGuiInput - null when nothing was written.
    void* BeginGuiInput();
    void EndGuiInput(void* camera);

    // Called from the interaction-ray hook before it does any work of its own.
    // Nothing consumes a recorded ray unless the crosshair is being placed, so
    // a no here spares the game's update tick the mod's origin test and the
    // thousand-metre fallback cast behind it. The ray recorded on the tick
    // after this turns true is the first one used; the frame in between places
    // the crosshair along the aim direction, which is what a stale ray does
    // anyway.
    bool WantsAimRay() const {
        return m_config.crosshairCompensation && m_enabled.load();
    }

    // Called from the eye tracking hook, on whatever thread asked the game
    // whether its tracker is tracking. Reports nothing but the mod's own state:
    // the setting is fixed after Initialize and the enable flag is atomic, so a
    // toggle takes effect on the next question and hands the feature back.
    bool SuppressEyeTracking() const {
        return m_config.suppressEyeTracking && m_enabled.load();
    }

    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;

private:
    Mod() : m_session(m_receiver) {}

    // No destructor, and the instance is deliberately never destroyed - see
    // Instance(). It owns two threads and a socket, and the CRT runs static
    // destructors from DLL_PROCESS_DETACH under the loader lock, after Windows
    // has already terminated those threads wherever each happened to be.
    ~Mod() = delete;

    // Initialize, in the order the log reports them.
    void LoadConfig();
    void ApplyTrackingSettings();
    bool InstallHooks();
    void StartReceiver();
    void RegisterHotkeys();

    void ComputeFrame();
    // The clean pose with this frame's head pose composed onto it, in whichever
    // yaw mode is selected. Both injection points write the same composition.
    math::CameraPose ComposeTrackedPose(const math::CameraPose& clean) const;

    // Scales the camera's field by the configured one for the render that
    // follows, caching the half-field tangents the crosshair projects through
    // and the zoom factor the head pose is scaled by. Returns whether the frame
    // can be projected through at all.
    //
    // Both injection points share it, and both have to call it BEFORE they
    // compose a pose: the factor is read off the field this call reports. The
    // window it is handed receives what Restore has to put back.
    bool ApplyFieldOfView(void* camera, camera::InjectionWindow& window);

    Config m_config;
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    cameraunlock::input::HotkeyPoller m_hotkeys;
    ModDiagnostics m_diag;

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_cameraHooked{false};
    std::atomic<bool> m_worldSpaceYaw{true};

    // Requested on the hotkey thread, run on the render thread. The mode
    // change itself is atomic, but it resets the interpolator and the position
    // processor's smoothing state, which the render thread is reading.
    cameraunlock::input::DeferredAction m_cycleModeRequest;

    int64_t m_lastFrameTicks = 0;
    float m_yaw = 0.0f, m_pitch = 0.0f, m_roll = 0.0f;
    bool  m_rotValid = false;
    float m_offsetX = 0.0f, m_offsetY = 0.0f, m_offsetZ = 0.0f;
    bool  m_posValid = false;

    // The interaction ray, as the game cast it during the update tick.
    aim::Ray m_ray;

    // What the game viewport's render has to have put back by the time
    // EndViewport returns. Live only between BeginViewport and EndViewport.
    camera::InjectionWindow m_render;

    // The same for one cLuxGuiHandler::UpdateInput call, which is outside the
    // render window and so has its own saved state rather than sharing that
    // one's.
    camera::InjectionWindow m_gui;

    // The pose the game viewport is being rendered through. Kept beside the
    // window because the crosshair and the view matrix check both read it after
    // the pose itself has gone back.
    math::CameraPose m_trackedPose{};

    // m_render.poseWritten says the camera fields have to be restored;
    // m_injecting says the frame is being drawn through a head pose the
    // crosshair can be projected into. They differ when the field of view was
    // overridden and the pose was not, and when a camera the field could not be
    // read from left no tangents to project through.
    bool  m_injecting = false;
    float m_tanX = 0.0f, m_tanY = 0.0f;

    // What the head pose is multiplied by so its displacement on screen is the
    // same at any field of view. 1 in ordinary play, below 1 while a script
    // zoom has pulled the field in, and 1 again whenever the field cannot be
    // read - never a guessed value. Yaw, pitch and the lean take it; roll does
    // not, because a head tilt rolls the picture by the same angle at every
    // field there is.
    float m_zoom = 1.0f;

    // The clean aim, resolved at injection time: a world point when the ray hit
    // something, a direction when it did not.
    aim::Target m_aim;

    // The basis the head pose is rendered through, composed when that pose is
    // written and kept until the next game viewport starts.
    //
    // It has to be kept rather than read on demand. SOMA's crosshair is drawn
    // from the player script's OnDraw, which runs after cScene::RenderViewport
    // has returned - measured in game, every single crosshair draw lands
    // outside the window. By then the camera carries the clean pose again, so a
    // crosshair placed from a live read is placed against the aim and sits at
    // screen centre however far the head has turned.
    math::ViewBasis m_renderView{};
    bool  m_renderViewValid = false;
};

}  // namespace SomaHT
