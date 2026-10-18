#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <optional>

/// <summary>
/// Suppresses the player's shouting. Call once, after the game/player is loaded.
/// </summary>
void InitShoutSuppression();

/// <summary>
/// Gives the player their shout input back and clears any hotkey window. Used while the mod is dormant.
/// </summary>
void ReleaseShoutSuppression();

/// <summary>
/// Auto re-disables shouting once the window from the last hotkey press expires.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickShoutSuppression(float dt);

/// <summary>
/// Enables/disables shouts on hotkey press.
/// </summary>
void ToggleShouts();

/// <summary>
/// Whether shouting is currently enabled. If true, data collection is suspended.
/// </summary>
bool AreShoutsEnabled();

/// <summary>
/// Registers the hotkey listener that calls ToggleShouts(). Called once at plugin init.
/// </summary>
void RegisterShoutHotkey();

/// <summary>
/// Sets the shout toggle hotkey and how long shouting stays enabled after it. Called with the values from TacticSettings.json.
/// </summary>
/// <param name="toggleKey">DirectInput scan code of the hotkey. Keeps the current one (F10 by default) if empty.</param>
/// <param name="enabledWindowSeconds">Seconds shouting stays enabled after the hotkey. Keeps the current one (10 by default) if empty.</param>
void ConfigureShouts(std::optional<std::uint32_t> toggleKey, std::optional<float> enabledWindowSeconds);
