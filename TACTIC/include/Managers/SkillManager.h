#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Converts a character's base skill level to a string representation.
/// </summary>
/// <param name="skillValue">Current skill level.</param>
/// <returns>String representation of the skill level.</returns>
std::string_view CalculateSkillLevel(float skillValue);

/// <summary>
/// Converts a character's level to a string representation.
/// </summary>
/// <param name="level">Character level.</param>
/// <returns>String representation of the character's level.</returns>
std::string_view CalculateCharacterLevel(std::uint16_t level);
