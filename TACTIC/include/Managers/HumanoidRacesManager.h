#pragma once

#include <string>

/// <summary>
/// Loads all humanoid races (races with valid weapon/hand-slot and weapon-drawn animation data) from a JSON file.
/// </summary>
/// <param name="path">Path to JSON file.</param>
/// <returns>True if the file was succesfully opened. False otherwise.</returns>
bool LoadHumanoidRaces(const std::string& path);

/// <summary>
/// Checks if a given Race is humanoid.
/// </summary>
/// <param name="raceEditorID">FormEditorID of a race.</param>
/// <returns>True if the given race is humanoid. False otherwise.</returns>
bool IsHumanoidRace(const std::string& raceEditorID);
