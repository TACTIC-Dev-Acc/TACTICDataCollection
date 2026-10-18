#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Checks whether any hostile projectile currently in flight is heading toward the player.
/// </summary>
/// <param name="player">Player to check against.</param>
/// <returns>True if at least one hostile projectile is currently on a path toward the player.</returns>
bool IsProjectileIncomingAtSelf(RE::PlayerCharacter* self);

/// <summary>
/// Gets the distance between an actor and an incoming projectile as a string representation.
/// </summary>
/// <param name="player">Player to check against.</param>
/// <returns>String representation of the distance.</returns>
std::string_view GetIncomingProjectileDistanceRepresentation(RE::PlayerCharacter* self);
