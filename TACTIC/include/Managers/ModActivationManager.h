#pragma once

#include "PCH.h"

/// <summary>
/// Checks whether the mod does anything yet.
/// </summary>
/// <returns>True if the mod is active.</returns>
bool IsModActive();

/// <summary>
/// Re-checks the quest state a few times a second and switches the mod on or off when it changed.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickModActivation(float dt);

/// <summary>
/// Call after a save was loaded or a new game started: re-checks right away and, if the mod is active, sets its per-save state up again.
/// </summary>
void NotifyGameLoaded();
