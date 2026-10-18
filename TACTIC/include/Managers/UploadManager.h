#pragma once

#include <string>

/// <summary>
/// Loads the upload endpoint URL and write-only upload key from a local JSON config.
/// </summary>
/// <param name="path">Path to the upload settings JSON file.</param>
void LoadUploadSettings(const std::string& path);

/// <summary>
/// Closes the current CombatData CSV and, if it has any rows, sets it aside (a fast rename) and reopens a fresh one so collection never stops. 
/// The set-aside file(s) are uploaded on a background thread.
/// </summary>
void TryFlushAndUploadCombatData();

/// <summary>
/// Call once per frame with the frame's delta time. Triggers TryFlushAndUploadCombatData every 30 minutes of playtime
/// </summary>
void TickPeriodicUpload(float dt);
