#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Loads JSON that maps NPCs to strength tiesrs at startup.
/// </summary>
void LoadEnemyStrengthTiers();

/// <summary>
/// Gets the strenght tier of an actor and adjusts it based on player level.
/// </summary>
std::string_view GetEnemyStrengthLevel(RE::Actor* enemyActor, std::string_view selfLevel);
