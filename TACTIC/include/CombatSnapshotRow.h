#pragma once

#include <array>
#include <string>
#include <string_view>

/// <summary>
/// Gates specific enemy features based on whether the enemy is currently visible to the player.
/// </summary>
/// <param name="observable">True if the actor is visible to the player. False otherwise.</param>
/// <param name="value">Value of the bool feature.</param>
/// <returns>"Unknown" if the actor is not visible. The current value of the given bool feature otherwise.</returns>
inline std::string_view BoolOrUnknown(bool observable, bool value)
{
    if (not observable) return "Unknown";
    return value ? "1" : "0";
}

// One dataset row. Each enemy produces a separate row, but they all share the same snapshot ID.
struct CombatSnapshotRow
{
    // ---- Identifiers ----

    // Unique per player for the lifetime of the game process.
    uint64_t sessionID;

    // Unique per combat encounter within this session.
    uint64_t combatID;

    // Unique per written snapshot within the current combat. Resets whenever a new combat begins.
    uint64_t snapshotID;

    // Local system time the snapshot was written. Used only for debugging purposes.
    std::string timestamp;

    // Unpaused game time since the session started, in milliseconds, taken when the snapshot was captured.
    uint64_t gameTimeMs;

    // ---- Trigger metadata ----

    // What kind of event produced this snapshot: Combat, Inventory, Movement, TargetSwitch or Heartbeat.
    std::string_view snapshotTrigger;

    // Where the movement frame's target comes from: "Hud" (the enemy the player is targeting, EnemyIsSelfTarget) or "NearestVisible"
    // (no HUD target, so the nearest visible enemy is the reference. It isn't the player's choice and none of its health/strength is known).
    std::string_view targetSource;

    // The combat decision that triggered data collection.
    std::string_view combatActionType;

    // The inventory-related decision that triggered data collection.
    std::string_view inventoryActionType;

    // The category of spell the actor switched to. Relevant only for SwitchToSpell* inventory actions.
    std::string_view spellCategory;

    // The player's current movement state. Idle, run or sprint. Walk gets converted to run since it provides limited, if none, combat strategy.
    std::string_view movementActionType;

    // Which restorative type(s) were consumed in the burst that triggered data collection.
    bool consumedHealthRestorative;
    bool consumedStaminaRestorative;
    bool consumedMagickaRestorative;

    // ---- Player state ----

    // Resource levels
    std::string_view healthLevel;
    std::string_view staminaLevel;
    std::string_view magickaLevel;

    // Hand equipment
    std::string_view leftType;
    std::string_view rightType;

    // Combat-related
    std::string_view selfAttackPhase;
    bool selfIsBlocking;
    bool selfIsStaggered;
    bool selfIsCasting;
    bool selfHasFleshSpellActive;

    // Movement-related
    bool selfIsRunning;
    bool selfIsSprinting;
    bool selfIsIdle;
    bool selfIsJumping;
    std::string_view movementDirection;

    // The same movement relative to the current target, "forward" meaning toward it (the NPC's movement frame), or to the player's heading without a target
    std::string_view movementDirectionToTarget;

    // Free space around the player (blocked/near/open) in 8 directions, "forward" pointing at the current target, or along the player's heading without one.
    // "Unknown" where the ray's hit point (or end point, if nothing was hit) is outside the camera's view
    std::array<std::string_view, 8> selfClearance;

    // The ground about 4 m out (level/step/drop/blocked) in the same 8 directions.
    // "Unknown" where the spot the floor should be is outside the camera's view; an edge whose bottom is out of view counts as "drop"
    std::array<std::string_view, 8> selfDrop;

    // Incoming projectile
    bool selfHasIncomingProjectileThreat;
    std::string_view selfIncomingProjectileDistance;

    // Spell knowledge
    bool selfKnowsADamageSpell;
    bool selfKnowsAnArmorSpell;
    bool selfKnowsAHealingSpell;
    bool selfKnowsAWardSpell;
    bool selfKnowsAConjurationFollowerSpell;
    bool selfKnowsAConjurationWeaponSpell;

    // Consumable-related
    bool selfHasHealthRestorative;
    bool selfHasStaminaRestorative;
    bool selfHasMagickaRestorative;

    // Inventory weapon-related
    bool selfHasOneHandedMeleeInInventory;
    bool selfHasTwoHandedMeleeInInventory;
    bool selfHasOneHandedRangedInInventory;
    bool selfHasTwoHandedRangedInInventory;
    bool selfHasShieldInInventory;
    std::string_view selfArrowCount;
    std::string_view selfBoltCount;
    std::string_view selfArmorType;
    bool selfIsAimingRangedWeapon;

    // ---- Player build ----

    std::string_view selfLevel;

    // Heavy armor
    std::string_view selfHeavyArmorLevel;
    bool selfHasFistsOfSteelPerk;

    // Light armor
    std::string_view selfLightArmorLevel;

    // Block
    std::string_view selfBlockLevel;
    bool selfHasElementalProtectionPerk;
    bool selfHasBlockRunnerPerk;
    bool selfHasShieldChargePerk;
    bool selfHasDisarmingBashPerk;

    // Two-handed
    std::string_view selfTwoHandedLevel;
    bool selfHasGreatCriticalChargePerk;

    // One-handed
    std::string_view selfOneHandedLevel;
    bool selfHasCriticalChargePerk;
    bool selfHasDualSavageryPerk;

    // Archery
    std::string_view selfArcheryLevel;

    // Illusion
    std::string_view selfIllusionLevel;
    bool selfHasIllusionDualCastingPerk;

    // Conjuration
    std::string_view selfConjurationLevel;
    bool selfHasConjurationDualCastingPerk;
    bool selfHasMysticBindingPerk;
    bool selfHasTwinSoulsPerk;

    // Destruction
    std::string_view selfDestructionLevel;
    bool selfHasDestructionDualCastingPerk;
    bool selfHasImpactPerk;

    // Restoration
    std::string_view selfRestorationLevel;
    bool selfHasRestorationDualCastingPerk;

    // Alteration
    std::string_view selfAlterationLevel;
    bool selfHasAlterationDualCastingPerk;

    // ---- Combatant counts ----

    int enemyCount;
    int allyCount;

    // ---- Per-enemy ----

    RE::FormID enemyFormID;
    std::string_view enemyName; // Only used for debugging

    std::string_view enemyStrengthLevel;

    // Summon-related
    bool enemyIsSummoned;
    RE::FormID enemySummonerFormID;
    bool enemySummonerKnown = true;

    bool wasLastHitEnemy;
    std::string_view enemyAngle;
    bool enemyIsVisible;

    // Every NPC state flag below that depends on the player being able to see the NPC is a BoolOrUnknown ("1"/"0"/"Unknown").
    std::string_view enemyIsBlocking;
    std::string_view enemyAttackPhase;
    std::string_view enemyIsBleedingOut;
    std::string_view enemyIsPowerAttackingSelf;
    std::string_view enemyIsNormalAttackingSelf;
    std::string_view enemyIsAimingAtSelf;
    bool enemyIsSelfTarget;

    // The enemy the movement frame points at: the same enemy as EnemyIsSelfTarget when TargetSource is "Hud", the fallback enemy when it's "NearestVisible". Exactly one per snapshot
    bool enemyIsFrameTarget;
    std::string_view enemyHealthLevel;
    std::string_view enemyLeftType;
    std::string_view enemyRightType;
    std::string_view enemyArmorType;
    std::string_view enemyIsStaggered;
    std::string_view enemyIsCasting;
    std::string_view enemyIsGrounded;
    std::string_view enemyIsRunning;
    std::string_view enemyIsSprinting;
    std::string_view enemyIsIdle;
    std::string_view enemyIsJumping;
    std::string_view enemyDistance;
    std::string_view enemyHeight;  // above / same_level / below the player (within about 1 m), "Unknown" when the distance is
    std::string_view enemyIsFacingSelf;
};
