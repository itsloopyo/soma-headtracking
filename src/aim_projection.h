// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

namespace SomaHT::aim {

// Not just "behind the camera": as depth goes to zero the projection goes to
// infinity, and a crosshair at 1e30 is a NaN on its way to a vertex buffer.
//
// It bounds two different quantities, because the aim vector is two different
// things. For a point it is metres from the eye, so this is a centimetre. For
// the direction fallback the vector is unit length and the depth is the cosine
// of the angle to the view axis, so this is 89.4 degrees off centre. Both are
// the bound wanted at that end.
constexpr float kMinProjectableDepth = 0.01f;

// Where the interaction ray is pointing, in the picture the head is looking at.
//
// SOMA draws its crosshair at a fixed screen position, so it marks what the
// player will grab, push, read or climb only while the rendered view IS the aim.
// Once the head turns or leans, the view and the ray are different and the
// crosshair is a lie - which is the whole reason this exists.
//
// The projection takes a vector FROM THE EYE THE FRAME IS DRAWN FROM TO THE
// POINT THE RAY LANDS ON, and the basis the frame is actually rendered with,
// and asks where the first lands in the second. With the head centred that
// vector is just the clean camera's forward; with a positional lean it swings
// by the parallax, which is why it has to be a vector to a point and not a
// direction.
//
// It is deliberately basis-to-basis rather than a formula in yaw / pitch /
// roll: the camera hook hands over the vectors it read back out of the matrix
// the engine built, so there is no second derivation of the composition to
// disagree with the first. A per-axis tangent formula would agree with it on
// single-axis poses and drift on combined ones, which is precisely the bug that
// survives testing.
//
// `tanX` / `tanY` are the drawn half-field tangents. HPL3's mfFOV is the
// VERTICAL field in radians, so tanY = tan(fov/2) and tanX = aspect * tanY.
//
// Returns false when the aim points at or behind the rendered view, where there
// is no screen position to draw at. NDC is x right, y up, both -1..1 across the
// frame.
inline bool ProjectAimToNdc(const float aim[3],
                            const float fwd[3], const float right[3], const float up[3],
                            float tanX, float tanY, float& ndcX, float& ndcY) {
    const float z = aim[0] * fwd[0] + aim[1] * fwd[1] + aim[2] * fwd[2];
    if (!(z > kMinProjectableDepth) || !(tanX > 0.0f) || !(tanY > 0.0f)) return false;

    const float x = aim[0] * right[0] + aim[1] * right[1] + aim[2] * right[2];
    const float y = aim[0] * up[0] + aim[1] * up[1] + aim[2] * up[2];
    ndcX = (x / z) / tanX;
    ndcY = (y / z) / tanY;
    return std::isfinite(ndcX) && std::isfinite(ndcY);
}

// A screen-space NDC offset, in the units cGuiSet::DrawGfx takes, added to the
// position the game asked to draw at.
//
// Half the set's virtual size is one NDC unit on each axis and GUI y runs
// downwards. Player.hps draws the crosshair at cLux_GetHudVirtualCenterSize()/2,
// the centre of a set whose virtual rect is @p virtualSize wide, which puts
// exactly half that rect between the centre and each edge - per axis, and
// independently of the camera's aspect. Do not derive the horizontal scale from
// the camera aspect instead: the HUD virtual size is not the screen aspect (it
// read 3299.56x768 in one session and 1479.11x768 in another on the same
// 5120x1440 display), and the game maps the two axes by independent ratios.
inline void OffsetGuiPositionByNdc(float ndcX, float ndcY, const float virtualSize[2],
                                   float& guiX, float& guiY) {
    guiX += ndcX * virtualSize[0] * 0.5f;
    guiY -= ndcY * virtualSize[1] * 0.5f;
}

}  // namespace SomaHT::aim
