#include <Managers/ResourceManager.h>

std::string_view CalculateResourceRepresentation(float current, float max) {
    // Prevent division by zero
    if (max == 0) return "Unknown";

    // Calculate percentage
    float percentage = (current / max);

    // Clamp percentage because drinking potions makes Skyrim think that I have more of a resource than the max
    percentage = std::clamp(percentage, 0.0f, 1.0f);
 
    if (percentage <= 0.25f) return "critical";

    else if (percentage <= 0.5f) return "low";

    else if (percentage <= 0.75f) return "medium";

    else if (percentage <= 0.99f) return "high";

    return "full";
}

std::string_view CalculateResourceLevel(RE::Actor* actor, RE::ActorValue resourceType) {
    // Get current and max resource levels
    float currentResource = actor->AsActorValueOwner()->GetActorValue(resourceType);
    float maxResource = actor->AsActorValueOwner()->GetPermanentActorValue(resourceType);

    // Return current resource level strin representation
    return CalculateResourceRepresentation(currentResource, maxResource);
}