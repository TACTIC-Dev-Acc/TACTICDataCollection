#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Sinks the player's attack/cast/block input to drive combat-action data collection.
/// </summary>
void RegisterCombatInputHandler();

/// <summary>
/// Ticks for pending projectile spawn once the player releases a drawn bow/crossbow.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickProjectileFiringCollection(float dt);

/// <summary>
/// Returns which hand the player's most recent favorites-menu equip input targeted. 
/// Used to tell a deliberate equip apart from an incidental one when a two-handed weapon breaks up into two independently-held items
/// </summary>
/// <param name="outWasLeftHand">Set to true if the left hand was targeted, false for right.</param>
/// <returns>True if there was a recent menu-equip input to report.</returns>
bool ConsumeRecentMenuEquipSide(bool& outWasLeftHand);

/// <summary>
/// Samples the player's live melee attack/block/bash state once per frame and resolves a pending melee press into the action it started.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickAttackStateTracking(float dt);

/// <summary>
/// Whether a captured melee-press/load/release/cast snapshot is still waiting for its outcome before being written.
/// </summary>
bool HasPendingDeferredRows();
