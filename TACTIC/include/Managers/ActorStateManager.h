#pragma once

#include "PCH.h"

/// <summary>
/// Enumerates attack phases.
/// </summary>
enum class AttackPhase : int { kNone, kWindup, kRelease, kRecovery };

/// <summary>
/// The player's own attack/block/aim state at one moment.
/// </summary>
struct SelfCombatStateSample
{
    AttackPhase attackPhase = AttackPhase::kNone;
    bool isBlocking = false;
    bool isAimingRanged = false;
};

/// <summary>
/// Getter for the current attack's current phase.
/// </summary>
/// <param name="actor">Actor to return the attack phase for.</param>
/// <returns>Actor's current attack phase.</returns>
AttackPhase GetAttackPhase(RE::Actor* actor);

/// <summary>
/// Converts attack phase to string.
/// </summary>
/// <param name="phase">Current attack phase.</param>
/// <returns>String representation of the current attack phase.</returns>
std::string_view AttackPhaseToString(AttackPhase phase);

/// <summary>
/// Checks if an actor is casting.
/// </summary>
/// <param name="actor">Actor to check.</param>
/// <returns>True if the actor is currently casting. False otherwise.</returns>
bool IsActorCasting(RE::Actor* actor);

/// <summary>
/// Checks if an actor is currently aiming with a ranged weapon.
/// </summary>
bool IsAimingRangedWeapon(RE::Actor* actor);

/// <summary>
/// Checks if an actor currently has a flesh-type Alteration armor spell active.
/// </summary>
bool HasFleshSpellActive(RE::Actor* actor);

/// <summary>
/// Checks if an actor is currently transformed into a Werewolf or Vampire Lord.
/// </summary>
bool IsTransformed(RE::Actor* actor);