#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Converts an angle in degrees (-180 - 180) to a string representation.
/// </summary>
/// <param name="angle">Angle value.</param>
/// <returns>String representation of the given angle.</returns>
std::string_view AngleToString(float angle);

/// <summary>
/// Calculates the angle between a source actor and a target actor.
/// </summary>
/// <param name="camera">Player camera.</param>
/// <param name="sourceActor">Source actor.</param>
/// <param name="targetActor">Target actor.</param>
float GetAngle(RE::Actor* sourceActor, RE::Actor* targetActor);

/// <summary>
/// Gets the angle between a source actor and a target actor as a string representation.
/// </summary>
/// <param name="camera">Player camera.</param>
/// <param name="sourceActor">Source actor.</param>
/// <param name="targetActor">Target actor.</param>
/// <returns>String representation of the angle between the 2 actors.</returns>
std::string_view GetAngleRepresentation(RE::Actor* sourceActor, RE::Actor* targetActor);

/// <summary>
/// Camera-relative heading (degrees) from the player's camera to a world position.
/// </summary>
float GetCameraRelativeAngle(const RE::NiPoint3& a_targetPos);