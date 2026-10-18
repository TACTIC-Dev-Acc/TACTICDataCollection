#pragma once

#include "PCH.h"

#include <vector>

#include "CombatSnapshotRow.h"
#include "Managers/ActorStateManager.h"
#include "Managers/ResourceRestorationManager.h"

/// <summary>
/// The hand contents the live ones: what the hands held before an equip change.
/// </summary>
struct SelfHandContents
{
    std::string_view left = "Unknown";
    std::string_view right = "Unknown";
};

/// <summary>
/// Collects data after a relevant combat event was triggered.
/// </summary>
/// <param name="self">Actor from the point of view of which the data is collected.</param>
/// <param name="actionType">Type of action that triggered data collection.</param>
/// <param name="consumedHealthRestorative">Whether a health item was consumed in the burst that triggered this snapshot.</param>
/// <param name="consumedStaminaRestorative">Whether a stamina item was consumed in the burst that triggered this snapshot.</param>
/// <param name="consumedMagickaRestorative">Whether a magicka item was consumed in the burst that triggered this snapshot.</param>
/// <param name="preConsumptionState">State of the actor on the moment they decided to consumbe (a) restorative item(s).</param>
/// <param name="deferredRows">If given, the snapshot's rows are captured into this vector instead of being written. Used for actions whose outcome is not immediately known.</param>
/// <param name="preEquipHands">If given, the hand contents to record as LeftType/RightType instead of the live ones.</param>
/// <param name="prePressCombatState">If given, the attack phase/blocking state to record instead of the live ones (which already reflect the press).</param>
void CollectData
(
    RE::PlayerCharacter* self,
    std::string_view actionType,
    bool consumedHealthRestorative = false,
    bool consumedStaminaRestorative = false,
    bool consumedMagickaRestorative = false,
    const SelfResourceSample* preConsumptionState = nullptr,
    std::vector<CombatSnapshotRow>* deferredRows = nullptr,
    const SelfHandContents* preEquipHands = nullptr,
    const SelfCombatStateSample* prePressCombatState = nullptr
);

/// <summary>
/// Writes rows captured by CollectData's deferredRows mode.
/// </summary>
void CommitDeferredRows(std::vector<CombatSnapshotRow>& rows, std::string_view combatActionType);

/// <summary>
/// Records the player's resource state once per frame, keeping the last two samples.
/// </summary>
void TickResourceHistory();

/// <summary>
/// Records the player's attack/block state once per frame, keeping the last two samples.
/// </summary>
void TickCombatStateHistory();

/// <summary>
/// Notifes that a restorative was consumed.
/// </summary>
/// <param name="actor">Actor that consumed the item.</param>
/// <param name="type">Which resource the consumed item restores.</param>
void NotifyRestorativeConsumed(RE::Actor* actor, RestoreType type);

/// <summary>
/// Ends the current combat once the compass has shown no red enemy dot for a moment so the next row starts a new CombatID. Called once per frame.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickCombatEnd(float dt);

/// <summary>
/// Ticks the debounced restorative-consumption collection armed by NotifyRestorativeConsumed.
/// Call once per frame.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickRestorativeCollection(float dt);

/// <summary>
/// Re-checks both of an actor's hand slots against what was last seen and collects a row, immediately, for whichever hand actually changed.
/// </summary>
/// <param name="actor">Actor whose hands to re-check.</param>
void NotifyHandsChanged(RE::Actor* actor);

/// <summary>
/// Checks whether the player's crosshair is now showing a different NPC than last checked.
/// Call once per frame.
/// </summary>
void TickTargetSwitchCollection();

/// <summary>
/// Checks whether the player's movement type (Jump/Sprint/Run/Idle) or raw WASD movement direction has changed since last checked.
/// Call once per frame.
/// </summary>
void TickMovementCollection();

/// <summary>
/// Seconds without any snapshot before a heartbeat snapshot is collected.
/// Must match the live inference tick rate, so a "None" label covers the same window the model decides over.
/// </summary>
constexpr float kHeartbeatInterval = 0.25f;

/// <summary>
/// Advances the unpaused game clock stamped into every snapshot as GameTimeMs, and the time since the last snapshot.
/// Call once per frame, before any collection tick.
/// </summary>
/// <param name="dt">Time elapsed since the last tick, in seconds.</param>
void TickSnapshotClock(float dt);

/// <summary>
/// Collects a "Heartbeat" snapshot (no combat/inventory action) once kHeartbeatInterval passes without any other snapshot.
/// Call once per frame, after the other collection ticks.
/// </summary>
void TickHeartbeatCollection();

/// <summary>
/// Writes one line to the log with the collection's current state, so a log shows what the plugin was doing around a crash or a report.
/// </summary>
/// <param name="modActive">Whether the mod is currently active.</param>
void LogCollectionStatus(bool modActive);

/// <summary>
/// The current session's unique identifier — stamped once at plugin load, never changes.
/// </summary>
uint64_t GetSessionID();

/// <summary>
/// Whether a combat segment is currently open.
/// </summary>
bool IsCollectingCombat();

/// <summary>
/// Resets combat tracking if a new save file is loaded in order to cater for cases where a save file is loaded while the player was in combat.
/// </summary>
void ResetCombatTracking();