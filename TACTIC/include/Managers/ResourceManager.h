#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Calculates the resource level as a percentage and returns a string representation of it.
/// </summary>
/// <param name="current">Current resource value.</param>
/// <param name="max">Maximum resource value.</param>
/// <returns>String representation of the current resource level.</returns>
std::string_view CalculateResourceRepresentation(float current, float max);

/// <summary>
/// Gets the current and maximum resource levels for the specified resource type and calculates the resource level representation.
/// </summary>
/// <param name="actor">Current actor.</param>
/// <param name="resoureType">Resource type to get the levels of.</param>
/// <returns>String representation of the current resource level.</returns>
std::string_view CalculateResourceLevel(RE::Actor* actor, RE::ActorValue resourceType);