#include "Managers/DistanceManager.h"
#undef GetObject

bool IsInMeleeRange(float distance, float weaponRange) 
{
    if (weaponRange == 0) return distance <= 130.0f; // Unarmed 

    else if (weaponRange < 1.0f) return distance <= 170.0f;  // Daggers
        
    else if (weaponRange < 1.3f) return distance <= 180.0f;  // One-handed
        
    else return distance <= 220.0f;  // Two-handed
}

float GetMaximumActorRange(RE::Actor* actor) 
{
    // Get equipped items for left and right hands
    auto leftEntry = actor->GetEquippedEntryData(true);
    auto rightEntry = actor->GetEquippedEntryData(false);

    // Define max reach
    float maxReach = 0;

    // Get left-hand weapon reach
    if (leftEntry) 
    {
        RE::TESObjectWEAP* weapon = leftEntry->GetObject()->As<RE::TESObjectWEAP>();

        if (weapon) maxReach = std::max(maxReach, weapon->weaponData.reach);
    }

    // Get right-hand weapon reach
    if (rightEntry) 
    {
        RE::TESObjectWEAP* weapon = rightEntry->GetObject()->As<RE::TESObjectWEAP>();

        if (weapon) maxReach = std::max(maxReach, weapon->weaponData.reach);
    }

    return maxReach;
}

float GetDistance(RE::Actor* sourceActor, RE::Actor* targetActor) 
{
    // Get actor positions
    RE::NiPoint3 selfPos = sourceActor->GetPosition();
    RE::NiPoint3 actorPos = targetActor->GetPosition();

    // Calculate distance between actors
    return selfPos.GetDistance(actorPos);
}

std::string_view GetDistanceRepresentation(float distance, float weaponRange)
{
    if (IsInMeleeRange(distance, weaponRange)) return "melee";
    else if (distance <= 500.0f) return "close";
    else if (distance <= 1000.0f) return "mid";
    return "far";
}

std::string_view GetDistanceRepresentation(RE::Actor* from, float distance)
{
    if (!from) return "Unknown";

    return GetDistanceRepresentation(distance, GetMaximumActorRange(from));
}

std::string_view GetDistanceRepresentation(RE::Actor* from, RE::Actor* to)
{
    if (!from || !to) return "Unknown";

    return GetDistanceRepresentation(from, GetDistance(from, to));
}