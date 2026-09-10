// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once
#include <cstdint>

// The HPL3 structures and functions this mod has to interoperate with.
//
// Member names follow Frictional Games' own naming, which SOMA publishes
// itself: the shipped `hps_api.hps` declares the engine's AngelScript API, and
// the shipped `config/game.cfg` the engine values quoted below. The numbers
// here describe where the player's own executable keeps things; each carries a
// comment saying what established it.
//
// Nothing here is engine source, engine logic or game content. What it is, and
// the only thing it can be used for, is a set of identifiers: byte offsets into
// structures the engine lays out, and short byte strings read from the opening
// few dozen bytes of a function, which name it the way a fingerprint names a
// person. They are recorded solely so this mod can find those functions in the
// copy of the game the player already owns, and they reconstruct no part of it.
// See THIRD-PARTY-NOTICES.md.
//
// Functions are located by byte signature at runtime rather than by fixed RVA,
// so one Layout covers both shipped executables - Soma.exe and Soma_NoSteam.exe
// are the same code linked at different addresses. Which Layout is in force is
// decided by the running executable's PE fingerprint; see build_profile.h. An
// unrecognised build matches no profile and the mod installs nothing.

namespace SomaHT::offsets {

// Everything one generation of shipped builds pins to. A patch that moves a
// member gets a NEW Layout and a new build profile beside the old one, never an
// edit here: a user who has not taken the patch keeps matching the layout their
// executable was built with.
struct Layout {
    // ------------------------------------------------------------ cCamera --
    // Byte offsets from the cCamera*. Read out of the class's own accessors:
    // GetFOV returns +0x28, GetAspect +0x2c, GetPosition returns this+0x10, and
    // cCamera::UpdateView writes the view matrix at +0x74 from the three angles.
    uint32_t camPosX;          // cVector3f mvPosition (metres)
    uint32_t camPosY;
    uint32_t camPosZ;
    uint32_t camFov;           // float mfFOV - VERTICAL, radians
    uint32_t camAspect;        // float mfAspect
    uint32_t camPitch;         // float mfPitch (radians)
    uint32_t camYaw;           // float mfYaw
    uint32_t camRoll;          // float mfRoll
    uint32_t camViewMatrix;    // cMatrixf m_mtxView (16 floats, row-major)

    // The engine's lazy-rebuild bytes, one per cached matrix. The projection is
    // not rebuilt per frame: cCamera::GetProjectionMatrix returns a matrix
    // cached inside the camera and only rebuilds it when camProjDirty is set,
    // and cCamera::SetFOV is three stores - the field, then that byte and both
    // frustum ones. A write to mfFOV that skips them leaves the engine drawing
    // through the previous frame's projection.
    uint32_t camViewDirty;     // char - set to rebuild m_mtxView
    uint32_t camProjDirty;     // char - set to rebuild the projection
    uint32_t camRotationDirty; // char - set to rebuild m_mtxRotation
    uint32_t camFrustumDirty;  // char - set to rebuild the frustum
    uint32_t camExtFrustDirty; // char - set to rebuild the extended frustum

    // ---------------------------------------------------------- cViewport --
    uint32_t viewportCamera;   // cCamera* - what cViewport::GetCamera returns

    // ------------------------------------------------------------ cGuiSet --
    // cVector2f mvVirtualSize - what cGuiSet::GetVirtualSize returns, and the
    // space every position handed to cGuiSet::DrawGfx is in. Read off the set
    // itself rather than off cLuxBase's copy of the HUD metrics: the set is what
    // the draw is scaled by, and the script's own gaze maths divides the screen
    // size by exactly this.
    uint32_t guiSetVirtualSize;

    // ------------------------------------------------------------- gpBase --
    // The single cLuxBase global. Its address is read out of the RIP-relative
    // operand that opens cLuxMapHelper::GetClosestEntity. Every offset below is
    // the one the matching cLux_* script accessor reads.
    uint32_t baseGameHudSet;           // cGuiSet*
    uint32_t baseHudVirtualCenterSize; // cVector2f
    uint32_t baseHudVirtualSize;       // cVector2f
    uint32_t baseHudCenterScreenSize;  // cVector2f
    uint32_t baseUserConfig;           // cLuxUserConfig*
    uint32_t baseGameHandler;          // owner of the paused flag
    uint32_t basePlayer;               // cLuxPlayer*
    uint32_t baseMapHandler;           // cLuxMapHandler*
    uint32_t gameHandlerPaused;        // char - cLux_GetGamePaused()
    uint32_t mapHandlerCurrentMap;     // cLuxMap*
    uint32_t mapHandlerViewport;       // cViewport* - the game viewport

    // ------------------------------------------------------ cLuxUserConfig --
    // The settings object cLuxBase creates at startup and writes back out on
    // exit. The field of view is loaded from two documents in one expression:
    // the default is the <Player FOV> line in config/game.cfg and a
    // <Gameplay FOV> attribute in the player's main_settings.cfg overrides it.
    // cLuxPlayer converts it with cMath::ToRad and hands it to cCamera::SetFOV,
    // so it is VERTICAL degrees - the "HORIZONTAL FOV" string in the language
    // files is a label on a widget that does not exist. See camera_fov.h.
    uint32_t userConfigFovDegrees;     // float

    // ---------------------------------------------------------- cLuxPlayer --
    // Confirmed as a pair: SetCharacterBody writes +0x170 and GetCharacterBody
    // reads it, which fixes the alignment of the surrounding accessors.
    uint32_t playerCamera;             // cCamera*
    uint32_t playerActive;             // char

    // -------------------------------------------------------- signatures --
    // Each was checked to match exactly once in each of the two shipped
    // executables.

    // cScene::RenderViewport - called once per visible viewport from
    // cScene::Render. This is the render phase: the camera is read through the
    // viewport here, and every gameplay query (the interaction ray, enemy
    // sight, sound listener) runs in the update tick outside it.
    const char* renderViewportPattern;

    // cLuxMapHelper::GetClosestEntity - the worker behind the script-facing
    // cLux_GetClosestEntity, which cUtility_PickBasics casts along the clean
    // player camera every update. Its distance is the depth the crosshair has to
    // be drawn at. The `mov rax, [rip+gpBase]` seven bytes in is also where the
    // gpBase global address comes from.
    const char* getClosestEntityPattern;

    // A plain closest-solid-surface ray with no interact filtering and no
    // length ceiling of its own, taking (avStart, avDir, afRayLength,
    // afDistance, avSurfaceNormal) - the bytes put the float third, in xmm2.
    // NOT the script-facing GetClosestBody, which hps_api.hps declares as a
    // callback-driven void with its float fourth; this is an engine-side
    // overload or helper behind it.
    //
    // The game's own interaction ray only reaches
    // cLuxMap::GetMaxInteractDistance (2 metres by config/game.cfg), so past
    // arm's length it reports no hit and says nothing about how far away the
    // surface the crosshair is drawn over is. A crosshair placed from a no-hit
    // is placed at infinity, and infinity has no parallax: it agrees with the
    // aim at exactly one distance and drifts either side of it the moment the
    // head leans. This is the cast that gives the far half of the frame a real
    // depth.
    const char* getClosestBodyPattern;

    // cGuiSet::DrawGfx(apGfx, avPos) as AngelScript calls it - the two-argument
    // overload, which has its own entry point separate from the four- and
    // eight-argument ones.
    //
    // This IS the crosshair, and nothing else. Player.hps line 411 is the only
    // call to the two-argument overload in the whole of script/; Effect_Flash,
    // InfectionHandler and PlayerEnergyHandler all draw through the
    // four-argument one, and every other DrawGfx in the tree is ImGui_DrawGfx, a
    // different function entirely. So the overload identifies the draw and no
    // sprite has to be recognised. It is the script-facing thunk, so
    // engine-side GUI drawing does not come through here either.
    //
    // The argument order is AngelScript's object-last: the set arrives in r8,
    // after the element and the position.
    const char* drawGfxPattern;

    // cLuxGuiHandler::UpdateInput - where the mouse position a focused screen
    // is picked with comes from.
    //
    // A screen the player is using does not get a screen-space mouse position.
    // The handler reads the mouse's own pixel, unprojects it through the player
    // camera, casts the resulting ray at the screen's mesh and hands the point
    // it lands on to that screen's cImGui as its mouse position. So the camera
    // this call sees decides both where the pointer is drawn on the screen and
    // which widget a click reaches, and it runs in the update tick, outside the
    // render - see gui_input_hook.h.
    //
    // Two spellings for the one function: Soma.exe compiles it with a stack
    // cookie and a different register allocation from Soma_NoSteam.exe, so
    // unlike every other signature here one pattern will not match both. Each
    // matches exactly once in its own executable and not at all in the other.
    const char* guiInputPatternSteam;
    const char* guiInputPatternNoSteam;

    // cEyeTrackerTobii::IsTracking - the one thing SOMA's own eye tracking
    // hangs off. EyeTrackingHandler.hps recomputes mbActive every
    // VariableUpdate from three conditions taken together - a tracker device
    // exists, the handler's own enable flag is set, and the device reports it
    // is tracking - and with mbActive false the whole module is
    // dormant: UpdateExtendedView does not run, GetExtendedViewRotation and
    // GetExtendedViewCrosshairOffset both return zero, and Player.hps writes
    // SetExtendedPitch/SetExtendedYaw(0) into the camera on the next tick.
    //
    // The implementation is two instructions - movzx eax, [rcx+0x92]; ret - so
    // it is matched together with IsUserPresent, the identical getter for the
    // byte before it, and the padding between them. Add
    // kIsTrackingPatternOffset to reach IsTracking itself.
    //
    // NOT the AngelScript thunk for it, which is a bare `mov rax,[rcx]; jmp
    // [rax+0x48]` vcall stub the linker folds across every class whose ninth
    // virtual is called from script - hooking that would answer for hundreds of
    // unrelated methods.
    const char* isTrackingPattern;
};

// Both shipped executables, linked 2021-09-27, carry this layout.
inline constexpr Layout kLayout_20210927 = {
    /* camPosX */ 0x10,
    /* camPosY */ 0x14,
    /* camPosZ */ 0x18,
    /* camFov */ 0x28,
    /* camAspect */ 0x2c,
    /* camPitch */ 0x44,
    /* camYaw */ 0x48,
    /* camRoll */ 0x4c,
    /* camViewMatrix */ 0x74,
    /* camViewDirty */ 0x709,
    /* camProjDirty */ 0x70a,
    /* camRotationDirty */ 0x70b,
    /* camFrustumDirty */ 0x70c,
    /* camExtFrustDirty */ 0x70d,
    /* viewportCamera */ 0x18,
    /* guiSetVirtualSize */ 0x100,
    /* baseGameHudSet */ 0x50,
    /* baseHudVirtualCenterSize */ 0x58,
    /* baseHudVirtualSize */ 0x60,
    /* baseHudCenterScreenSize */ 0x7c,
    /* baseUserConfig */ 0xa8,
    /* baseGameHandler */ 0xc8,
    /* basePlayer */ 0x140,
    /* baseMapHandler */ 0x148,
    /* gameHandlerPaused */ 0x2d4,
    /* mapHandlerCurrentMap */ 0x90,
    /* mapHandlerViewport */ 0x210,
    /* userConfigFovDegrees */ 0xd0,
    /* playerCamera */ 0x168,
    /* playerActive */ 0x230,
    /* renderViewportPattern */
    "48 89 5C 24 10 44 89 4C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 "
    "48 8B 42 38 4C 8B 72 30 48 8B FA 0F 29 74 24 40 0F 28 F2 48 89 84 24 90 00 00 00",
    /* getClosestEntityPattern */
    "40 57 48 83 EC 60 48 8B 05 ?? ?? ?? ?? 48 8B F9 4D 8B D0 4C 8B 88 48 01 00 00 "
    "4C 8B C2 49 8B 89 90 00 00 00 48 85 C9 75 ?? 32 C0 48 83 C4 60 5F",
    /* getClosestBodyPattern */
    "48 83 EC 38 48 8B 44 24 60 4C 8B C2 48 8B D1 48 8B 0D ?? ?? ?? ?? 48 89 44 24 28 "
    "0F 28 DA 48 8B 89 C0 00 00 00 4C 89 4C 24 20 E8 ?? ?? ?? ?? 48 83 C4 38 C3",
    /* drawGfxPattern */
    "4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 48 81 EC 28 01 00 00 "
    "F3 0F 10 0D ?? ?? ?? ?? 48 8D 8C 24 C0 00 00 00 E8 ?? ?? ?? ?? 0F 57 C9 "
    "48 8D 8C 24 C8 00 00 00",
    /* guiInputPatternSteam */
    "48 8B C4 55 57 41 54 48 8D 68 A1 48 81 EC E0 00 00 00 48 C7 45 07 FE FF FF FF "
    "48 89 58 10 48 89 70 18 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 37 48 8B F1 "
    "0F B6 89 92 01 00 00 84 C9 74 09 4C 8B A6 60 01 00 00 EB 07 4C 8B A6 70 01 00 00 4D 85 E4",
    /* guiInputPatternNoSteam */
    "40 55 56 41 54 48 8D 6C 24 B9 48 81 EC C0 00 00 00 48 8B F1 "
    "0F B6 89 92 01 00 00 84 C9 74 09 4C 8B A6 60 01 00 00 EB 07 4C 8B A6 70 01 00 00 4D 85 E4",
    /* isTrackingPattern */
    "0F B6 81 91 00 00 00 C3 CC CC CC CC CC CC CC CC 0F B6 81 92 00 00 00 C3",
};

// The layout the running executable's profile selected. Defaults to the layout
// the shipped builds carry, which is what the header-only accessors are
// exercised against in tests; MatchRunningProfile points it at the matched
// profile's layout before any hook exists to read it, and installs nothing when
// no profile matches.
inline const Layout*& ActiveSlot() {
    static const Layout* slot = &kLayout_20210927;
    return slot;
}
inline const Layout& Active() { return *ActiveSlot(); }
inline void SetActiveLayout(const Layout& layout) { ActiveSlot() = &layout; }

// Offsets into the getClosestEntity signature for the `mov rax, [rip+disp32]`
// that loads gpBase. Properties of that byte string rather than of the build,
// so they live with it and not in the Layout.
constexpr int kGpBaseInsnOffset = 6;
constexpr int kGpBaseDispOffset = 3;
constexpr int kGpBaseInsnLength = 7;

// isTrackingPattern matches at IsUserPresent, the getter for the byte before
// the one IsTracking reads. Both are eight bytes with eight of padding between
// them, so IsTracking starts sixteen in.
constexpr int kIsTrackingPatternOffset = 16;

// cLuxClosestEntityCallback seeds the distance with this. A ray that comes back
// with it hit nothing: the aim point is at infinity and the direction is the
// honest thing to project.
constexpr float kNoHitDistance = 9999999.0f;

// How far the mod's own far cast reaches. The camera's far clip is 1000 metres
// by config/game.cfg and no SOMA interior is a tenth of that.
constexpr float kAimRayLength = 1000.0f;

// A ray whose origin is further than this from the player camera was cast by
// something else and is not the player's aim. cUtility_PickBasics offsets its
// origin off the camera by at most gfPickOffsetSize (0.02m) along one axis.
constexpr float kPlayerRayOriginTolerance = 0.25f;

// ...and one that does not run along the camera's forward axis is not the aim
// either. The eye tracker's line-of-sight probe starts at the same point and
// aims at whatever entity it is checking, so the origin alone does not separate
// the two. A quarter of a degree is far wider than the float error in a
// normalised direction and far tighter than any second cast in the scripts.
constexpr float kPlayerRayForwardDot = 0.99999f;

}  // namespace SomaHT::offsets
