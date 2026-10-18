# pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Checks if the player is within melee range of the target actor.
/// </summary>
/// <param name="distance">Distance between player and target.</param>
/// <param name="weaponRange">Maximum weeapon range that the player has.</param>
/// <returns>True if the player is in melee range with the target. False otherwise.</returns>
bool IsInMeleeRange(float distance, float weaponRange);

/// <summary>
/// Gets the maximum range of an actor.
/// </summary>
/// <param name="actor">Current actor.</param>
/// <returns>Maximum range of the current actor.</returns>
float GetMaximumActorRange(RE::Actor* actor);

/// <summary>
/// Gets the distance between two actors.
/// </summary>
/// <param name="actor1">Source actor (player).</param>
/// <param name="actor2">Target actor.</param>
/// <returns>Distance between the 2 actors.</returns>
float GetDistance(RE::Actor* sourceActor, RE::Actor* targetActor);

/// <summary>
/// Gets a string representation of the distance between two actors.
/// </summary>
/// <param name="from">Source actor.</param>
/// <param name="to">Target actor</param>
/// <returns></returns>
std::string_view GetDistanceRepresentation(RE::Actor* sourceActor, RE::Actor* targetActor);

/// <summary>
/// Gets a string representation of the distance between an actor and a non-actor object (e.g. projectiles).
/// </summary>
/// <param name="from">Actor the distance is measured from.</param>
/// <param name="distance">Pre-computed distance from that actor.</param>
/// <returns>String representation of the given distance.</returns>
std::string_view GetDistanceRepresentation(RE::Actor* from, float distance);

/// <summary>
/// Used in order to reduce Skyrim engine calls.
/// </summary>
/// <param name="distance">Pre-computed distance from the "from" actor.</param>
/// <param name="weaponRange">Pre-computed GetMaximumActorRange(from).</param>
/// <returns>String representation of the given distance.</returns>
std::string_view GetDistanceRepresentation(float distance, float weaponRange);