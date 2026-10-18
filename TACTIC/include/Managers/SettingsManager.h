#pragma once

#include <string>

/// <summary>
/// Loads TacticSettings.json, which holds every setting the player can change: the shout hotkey and its window, and whether scrolls are allowed.
/// A missing file, a missing key, or a file that can't be read leaves the defaults in place and never throws, since players edit this file by hand.
/// Every call starts again from the defaults, so a key removed from the file stops applying the next time it's loaded.
/// </summary>
/// <param name="path">Path to the JSON file.</param>
void LoadTacticSettings(const std::string& path);

/// <summary>
/// The "scrolls_enabled" setting. False by default, in which case scrolls are removed from the player's inventory as soon as they enter it.
/// </summary>
/// <returns>True if the player allowed scrolls.</returns>
bool AreScrollsEnabled();
