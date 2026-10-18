#pragma once

#include <limits>
#include <string>

#include "Managers/ActorStateManager.h"
#include "Managers/ResourceRestorationManager.h"

struct ActorContext
{
    // FormID of the actor that was hit last
    RE::FormID lastHitActor = 0;

    // FormID of the NPC most recently shown under the player's crosshair — i.e. whose health bar the HUD last displayed
    RE::FormID lastCrosshairTarget = 0;

    // Whether the player has ever seen this actor
    bool everSeen = false;

    // Armor type the actor is wearing. Only looks at body armor due to relevant survey result.
    std::string_view armorType = "Unknown";

    // Equipment held in the left and right hands of the actor.
    std::string_view observedLeftType = "Unknown";
    std::string_view observedRightType = "Unknown";

    // Name-based strength tier, remembered from the last time the actor was shown in the HUD (the only time the player sees their exact name).
    std::string_view observedStrengthLevel = "Unknown";

    // Whether the NPC is a summon/reanimation and who commands it.
    bool observedIsSummoned = false;
    RE::FormID observedSummonerFormID = 0;

    // Restorative-consumption burst tracking.
    bool pendingHealthRestorative = false;
    bool pendingStaminaRestorative = false;
    bool pendingMagickaRestorative = false;
    float restorativeCollectionCooldown = 0.0f;
    bool burstInMenu = false;
    SelfResourceSample burstPreState;

    // Player's resource state from the last two frames.
    SelfResourceSample sampleNow;
    SelfResourceSample samplePrev;

    // Player's attack/block state from the last two frames.
    // By the time the input sink sees an attack/block press, the game has already started it, so the previous frame is the pre-press state.
    SelfCombatStateSample combatSampleNow;
    SelfCombatStateSample combatSamplePrev;

    // Per-hand equip collection. Used to decide whether that hand actually changed.
    std::string lastLeftHandLabel;
    std::string lastRightHandLabel;

    // What each hand held.
    std::string_view prevLeftContent = "Unknown";
    std::string_view prevRightContent = "Unknown";
    std::string_view burstPreLeftContent = "Unknown";
    std::string_view burstPreRightContent = "Unknown";
    double lastEquipEventTime = -1000.0;

    // Used to correctly determien the left and right hand equipment when switching from a 2-handed weapon to a 1-handed weapon/spell.
    // Important because Skyrim auto-equips the other hand with something, which is not chosen by the player.
    bool inTwoHandedBreakup = false;
    bool suppressNextLeftHandChange = false;
    bool suppressNextRightHandChange = false;

    // Sticky memory for movement-change collection.
    std::string_view lastMovementType;
    std::string_view lastMovementDirection;

    // The player's actual world-space body heading (degrees) as of the last collected movement snapshot, frozen while idle.
    float lastWorldHeadingDeg = std::numeric_limits<float>::quiet_NaN();

    // Store information about restorative items.
    RestoreItems restoratives;

    // Sticky per-combat flag.
    bool inCombat = false;
};

/// <summary>
/// Getter for ActorContext
/// </summary>
/// <param name="actor">Actor to get the context for,</param>
/// <returns>Actor's ActorContext.</returns>
ActorContext& GetActorContext(RE::Actor* actor);

/// <summary>
/// Whether the player has ever seen the actor with this FormID.
/// </summary>
bool HasActorBeenSeen(RE::FormID formID);

/// <summary>
/// Restetrs combat state.
/// </summary>
void ResetCombatState();

/// <summary>\
/// Clears the list of ActorContexts.
/// </summary>
void ClearActorContexts();