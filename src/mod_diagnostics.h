// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "camera_fov.h"
#include "gameplay_gate.h"
#include "hpl_math.h"

#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/math/smoothing_utils.h"

// Everything the mod says about itself in HeadTracking.log while it runs, and
// the latches that keep it to one line each.
//
// It is one object rather than a method per line on Mod for two reasons. The
// log is a side effect and the pose maths is not, so keeping them apart is what
// lets the per-frame path be read without nine "have I said this yet" flags in
// the way. And the latching is the whole difficulty: every one of these lines
// sits on a path that runs every frame, so a line written unlatched is tens of
// thousands of copies of one fact, and a latch that is checked but never set is
// the same thing.
//
// Nothing here reads the mod's state. Each call is handed what it reports, so a
// line cannot disagree with the frame that produced it.
namespace SomaHT {

class ModDiagnostics {
public:
    // Latched ahead of every other check in the render path: an installed hook
    // that never runs and a hook that runs with tracking off read the same in
    // the log otherwise.
    void HookRunning() {
        if (!m_hookRan.Fire()) return;
        cameraunlock::logging::Line("Camera: RenderViewport hook is running.");
    }

    // The latched "the player is playing" line and the HUD geometry beside it.
    void GameplayEntry() {
        if (!m_gameplay.Fire()) return;
        cameraunlock::logging::Line("Game state: in gameplay.");

        gate::HudMetrics hud{};
        if (!gate::GetHudMetrics(hud)) return;
        cameraunlock::logging::Line(
            "HUD: gui set virtual %gx%g, cLuxBase virtual %gx%g, centre %gx%g, centre screen "
            "%gx%g",
            hud.guiSetVirtualSize[0], hud.guiSetVirtualSize[1],
            hud.virtualSize[0], hud.virtualSize[1],
            hud.centerSize[0], hud.centerSize[1],
            hud.centerScreenSize[0], hud.centerScreenSize[1]);
    }

    // Latched, because "did any tracker data reach the mod" is the one question
    // the startup lines cannot answer: the UDP bind succeeds whether or not a
    // tracker ever sends.
    void FirstPose(float yaw, float pitch, float roll) {
        if (!m_firstPose.Fire()) return;
        cameraunlock::logging::Line("Tracker data received: yaw=%.2f pitch=%.2f roll=%.2f", yaw,
                                    pitch, roll);
    }

    // Which of the two smoothing parameters the session is now using, which
    // follows the receiver's source-address classification. Reported on every
    // change rather than once, because a tracker swap picks the other parameter
    // up without a restart.
    void ConnectionLocality(bool isRemote, float localSmoothing, float remoteSmoothing) {
        if (m_localityKnown && isRemote == m_isRemote) return;
        m_isRemote = isRemote;
        m_localityKnown = true;

        const double effective = cameraunlock::math::GetEffectiveSmoothing(
            localSmoothing, remoteSmoothing, isRemote);
        cameraunlock::logging::Line("Tracker connection is %s; smoothing=%.2f",
                                    isRemote ? "remote" : "local", effective);
    }

    // The tracker going quiet and coming back, which no other line in the log
    // covers - the pose is held either way.
    //
    // The receiver's has-data flag is sticky, so nothing else in the mod can
    // tell a tracker that stopped sending from one held perfectly still. These
    // two lines are the only record that the packets stopped, which is what a
    // report of "it was working and then it stopped" needs to be answerable
    // from the log alone.
    void ConnectionState(bool receiving) {
        if (m_receivingKnown && receiving == m_receiving) return;
        const bool first = !m_receivingKnown;
        m_receiving = receiving;
        m_receivingKnown = true;
        // The first observation is not a change: whether a tracker connected at
        // all is what FirstPose reports.
        if (first) return;

        ++m_connectionChanges;
        if (m_connectionChanges > kMaxConnectionChangesLogged) return;
        if (m_connectionChanges == kMaxConnectionChangesLogged) {
            cameraunlock::logging::Line(
                "Tracker connection keeps dropping and returning; further changes are not "
                "logged.");
            return;
        }
        cameraunlock::logging::Line(receiving
            ? "Tracker data resumed."
            : "Tracker stopped sending; holding the last pose.");
    }

    // Latched, once each. Whether the crosshair is being placed with a measured
    // depth or with a bare direction is the difference between it sitting on the
    // surface the ray reached and it drifting under a lean, and the two look the
    // same in a screenshot.
    //
    // @p rayEverCast is whether any cast has been recorded at all: before the
    // first one there is nothing to have gone stale, which is the crosshair
    // hook not being installed, and that said so at startup.
    void AimDepth(bool isPoint, bool rayEverCast) {
        if (isPoint) {
            if (!m_aimDepth.Fire()) return;
            cameraunlock::logging::Line(
                "Crosshair: placing on the interaction ray's own depth.");
            return;
        }
        if (!rayEverCast || !m_rayStale.Fire()) return;
        cameraunlock::logging::Line(
            "Crosshair: no ray was cast for this frame, so it is placed along the aim direction "
            "and carries no depth.");
    }

    // One shot, and only once the head has moved the view far enough for the
    // answer to mean anything. The whole mod rests on cCamera rebuilding its
    // view matrix from the angles written on the way into the render; if it did
    // not, the frame is drawn from the clean camera, the crosshair moves off a
    // view that never turned, and nothing else in the log says so.
    //
    // The question is asked as "closer to which", not "does it match the tracked
    // forward". SOMA's own Extended View adds up to 34 degrees to the rendered
    // view without touching the angle fields, so a match test reports a failure
    // on every frame a Tobii user plays. That rotation is common to both
    // candidates and cancels out of a comparison.
    void ViewMatrixRebuilt(const math::ViewBasis& rendered, const math::CameraPose& tracked,
                           const math::CameraPose& clean) {
        // Checked before the trigonometry, not after: this runs every frame of
        // the render window until it fires, and every frame after it has.
        if (m_viewMatrix.Fired()) return;

        float trackedFwd[3], cleanFwd[3];
        math::CameraForward(tracked.yaw, tracked.pitch, trackedFwd);
        math::CameraForward(clean.yaw, clean.pitch, cleanFwd);

        if (Dot(trackedFwd, cleanFwd) > kMinSeparationCosine) return;

        m_viewMatrix.Fire();
        if (Dot(rendered.fwd, trackedFwd) > Dot(rendered.fwd, cleanFwd)) return;
        cameraunlock::logging::Line(
            "Crosshair: the engine is rendering through a view matrix that does not carry the "
            "head pose, so crosshair compensation is inert.");
    }

    // Latched: every term of the zoom factor, on the first frame the camera
    // updates rather than the first frame a pose arrives. A factor that is wrong
    // by a constant renders exactly like one that is right, so the only check is
    // a human reading 1.0000 off this line in ordinary play.
    void FieldBasis(const fov::Field& drawn, float baseDegrees, float configuredDegrees,
                    float zoom) {
        constexpr float kRadToDeg = 57.2957795f;
        // Two latches, not one. The line below the degenerate branch is the
        // release gate - a human reads 1.0000 off it - and a single latch spent
        // on the degenerate case during a frame where cLuxUserConfig was not
        // filled in yet would mean that line is never written at all.
        if (!(baseDegrees > 0.0f) && !(configuredDegrees > 0.0f)) {
            if (!m_fieldUnmeasurable.Fire()) return;
            cameraunlock::logging::Line(
                "Field of view: rendering %.3f vertical degrees, but cLuxUserConfig reported no "
                "field to measure against, so the head pose is not scaled for zoom.",
                static_cast<double>(drawn.fovY * kRadToDeg));
            return;
        }

        if (!m_fieldBasis.Fire()) return;
        // Through NormalPlayRadians, not a second copy of its rule: the two
        // disagreed once, and a line that reports a basis the code did not use
        // is worse than no line, because reading 1.0000 off it is the gate.
        const float normalDegrees =
            fov::NormalPlayRadians(configuredDegrees, baseDegrees) * kRadToDeg;
        cameraunlock::logging::Line(
            "Field of view: rendering %.3f vertical degrees at aspect %.4f; ordinary play is "
            "%.3f vertical degrees (game %.3f, [Camera] FieldOfView %.3f), so the head pose "
            "scales by %.4f.",
            static_cast<double>(drawn.fovY * kRadToDeg), static_cast<double>(drawn.aspect),
            static_cast<double>(normalDegrees), static_cast<double>(baseDegrees),
            static_cast<double>(configuredDegrees), static_cast<double>(zoom));
    }

    void GuiInputRunning() {
        if (!m_guiInput.Fire()) return;
        cameraunlock::logging::Line(
            "Gui input: hook is running - a focused screen's mouse ray is cast through the "
            "head-tracked camera.");
    }

    // The input update is not called from inside a render, so this line says the
    // shape of the frame is not what the gui input hook was written against.
    void GuiInputInsideRender() {
        if (!m_guiInputInRender.Fire()) return;
        cameraunlock::logging::Line(
            "Gui input: the mouse ray is cast inside the render, where the head pose is already "
            "on the camera.");
    }

private:
    // A line that is written the first time it is reached and never again.
    class Once {
    public:
        // True exactly once: for the caller that gets to write the line.
        bool Fire() {
            if (m_fired) return false;
            m_fired = true;
            return true;
        }

        // For a caller with work to do BEFORE it knows whether the line applies,
        // which must not pay for that work on every frame after it was written.
        bool Fired() const { return m_fired; }

    private:
        bool m_fired = false;
    };

    static float Dot(const float a[3], const float b[3]) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    // A tracker on a poor WiFi link crosses the receiver's 500ms timeout in both
    // directions over and over, and one line per crossing would fill the log
    // with the same fact. Past this many changes the log says so once and goes
    // quiet.
    static constexpr int kMaxConnectionChangesLogged = 20;

    // Two forwards closer together than this - about 2.6 degrees - cannot be
    // told apart by which one the rendered basis is nearer to.
    static constexpr float kMinSeparationCosine = 0.999f;

    Once m_hookRan;
    Once m_gameplay;
    Once m_firstPose;
    Once m_aimDepth;
    Once m_rayStale;
    Once m_viewMatrix;
    Once m_fieldBasis;
    Once m_fieldUnmeasurable;
    Once m_guiInput;
    Once m_guiInputInRender;

    bool m_isRemote = false;
    bool m_localityKnown = false;

    bool m_receiving = false;
    bool m_receivingKnown = false;
    int  m_connectionChanges = 0;
};

}  // namespace SomaHT
