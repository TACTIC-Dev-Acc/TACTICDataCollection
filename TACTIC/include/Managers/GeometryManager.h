#pragma once

#include "PCH.h"

#include <array>
#include <string_view>

/// <summary>
/// The 8 directions around an actor, clockwise from "forward". Same names and order as the MovementDirection labels.
/// </summary>
inline constexpr std::array<std::string_view, 8> kRelativeDirections =
{
    "forward", "forward_right", "right", "back_right", "back", "back_left", "left", "forward_left"
};

/// <summary>
/// Whether a world point can currently be seen: whether it is inside a viewer's field of view.
/// </summary>
using IsPointObservableFn = bool (*)(const RE::NiPoint3& a_point);

/// <summary>
/// Whether a world point is inside the player's camera frustum. The same test EnemyIsVisible uses.
/// </summary>
bool IsInPlayerCameraView(const RE::NiPoint3& a_point);

/// <summary>
/// What an actor can see of its surroundings in each of the 8 directions, in kRelativeDirections order.
/// </summary>
struct Surroundings
{
    // Free space, from a horizontal ray against walls, terrain and statics (not characters):
    // "blocked" (under 1 m), "near" (1 to 3 m), "open" (over 3 m).
    // "Unknown" if the hit point (or the ray's end point, if nothing was hit) is out of view.
    std::array<std::string_view, 8> clearance;

    // The ground about 4 m out (where the floor first comes into a first-person view), from a downward ray: "level" (within 0.5 m of the feet),
    // "step" (a drop shorter than the fall-damage height), "drop" (a drop at least that high, no ground at all, or an edge whose bottom is out of view),
    // "blocked" (a wall within about 4.5 m comes first).
    // "Unknown" if the spot where the floor should be is out of view.
    std::array<std::string_view, 8> drop;
};

/// <summary>
/// Raycasts an actor's surroundings in 8 directions. "forward" points at the target, or along the actor's heading when it has no target.
/// </summary>
/// <param name="actor">Actor to cast from. Its own collision is ignored.</param>
/// <param name="target">Actor that defines "forward". Can be null.</param>
/// <param name="isObservable">Whether the viewer can see a point. Null means everything is observable.</param>
/// <returns>Clearance and drops per direction. Everything is "Unknown" without a physics world.</returns>
Surroundings GetSurroundings(RE::Actor* actor, RE::Actor* target, IsPointObservableFn isObservable = nullptr);
