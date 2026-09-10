// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Characterization tests for aim_projection.h.
//
// These lock the crosshair projection: where the interaction ray lands in the
// picture the head is looking at. A drift here is invisible in a screenshot and
// only shows up as the crosshair sitting beside what the player actually grabs,
// so the numbers below are hand-computed rather than snapshotted.

#include "aim_projection.h"
#include "test_support.h"

namespace {

using SomaHT::testing::NearEqual;
using SomaHT::testing::Report;

// Camera looking down +z with x right and y up.
constexpr float kFwd[3]   = {0.0f, 0.0f, 1.0f};
constexpr float kRight[3] = {1.0f, 0.0f, 0.0f};
constexpr float kUp[3]    = {0.0f, 1.0f, 0.0f};

}  // namespace

int RunAimProjectionTests() {
    std::cout << "\nAim projection tests\n";
    Report r;

    float ndcX = -99.0f;
    float ndcY = -99.0f;

    // The head is centred: the ray is the view, so the crosshair is the centre.
    {
        const float aim[3] = {0.0f, 0.0f, 5.0f};
        const bool ok = SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY);
        r.Check(ok && NearEqual(ndcX, 0.0f) && NearEqual(ndcY, 0.0f), "centred aim projects to screen centre");
    }

    // Off-centre: x/z divided by the half-field tangent, independently per axis.
    {
        const float aim[3] = {0.3f, 0.2f, 2.0f};
        const bool ok = SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY);
        r.Check(ok && NearEqual(ndcX, 0.15f) && NearEqual(ndcY, 0.2f), "offset aim divides by the half-field tangents");
    }

    // The frame edge is exactly one half-field tangent away.
    {
        const float aim[3] = {2.0f, 1.0f, 2.0f};
        const bool ok = SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY);
        r.Check(ok && NearEqual(ndcX, 1.0f) && NearEqual(ndcY, 1.0f), "aim at the half-field lands on the frame edge");
    }

    // The basis is the rendered one, not the world: a yawed camera projects the
    // same world-space aim somewhere else.
    {
        const float fwd[3]   = {1.0f, 0.0f, 0.0f};
        const float right[3] = {0.0f, 0.0f, -1.0f};
        const float up[3]    = {0.0f, 1.0f, 0.0f};
        const float aim[3]   = {2.0f, 0.0f, 0.3f};
        const bool ok = SomaHT::aim::ProjectAimToNdc(aim, fwd, right, up, 1.0f, 0.5f, ndcX, ndcY);
        r.Check(ok && NearEqual(ndcX, -0.15f) && NearEqual(ndcY, 0.0f), "projection follows the rendered basis");
    }

    // Behind the rendered view there is no screen position to draw at.
    {
        const float aim[3] = {0.0f, 0.0f, -5.0f};
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY),
                "aim behind the view is rejected");
    }

    // The guard is a minimum depth, not merely a sign test: the projection goes
    // to infinity as depth goes to zero.
    {
        const float aim[3] = {0.0f, 0.0f, 0.01f};
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY),
                "aim at the minimum depth is rejected");
    }
    {
        const float aim[3] = {0.0f, 0.0f, 0.011f};
        r.Check(SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY),
                "aim just past the minimum depth is accepted");
    }

    // A degenerate field has no tangent to divide by.
    {
        const float aim[3] = {0.0f, 0.0f, 5.0f};
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 0.0f, 0.5f, ndcX, ndcY),
                "zero horizontal half-field is rejected");
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.0f, ndcX, ndcY),
                "zero vertical half-field is rejected");
    }

    // A NaN must not reach a vertex buffer. This one is caught by the depth
    // test - NaN fails `z > kMinProjectableDepth` - so it says nothing about
    // the finite check on the way out; the case below is what does.
    {
        const float nan = std::nanf("");
        const float aim[3] = {nan, 0.0f, 5.0f};
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0f, 0.5f, ndcX, ndcY),
                "a non-finite aim is rejected");
    }

    // A finite aim with a projectable depth whose QUOTIENT overflows. Nothing
    // upstream bounds the aim point - it is the game's own interaction hit,
    // divided by a half-field tangent that a script zoom can pull very small -
    // so the only thing between an infinity here and the vertex buffer is the
    // isfinite check on the returned NDC.
    {
        const float aim[3] = {3.0e38f, 0.0f, 0.02f};
        r.Check(!SomaHT::aim::ProjectAimToNdc(aim, kFwd, kRight, kUp, 1.0e-6f, 0.5f, ndcX, ndcY),
                "an aim that projects to a non-finite NDC is rejected");
    }

    return r.Failures();
}
