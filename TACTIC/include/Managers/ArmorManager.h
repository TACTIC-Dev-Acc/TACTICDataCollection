#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Classifies an actor's currently worn body armor. Ignores other body part armor types due to relevant survey results.
/// </summary>
/// <param name="actor">Actor to check.</param>
/// <returns>String identifier of the actor's body armor type.</returns>
std::string_view GetBodyArmorType(RE::Actor* actor);
