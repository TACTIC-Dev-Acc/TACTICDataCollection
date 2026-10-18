#pragma once

#include <string_view>
#include <vector>

/// <summary>
/// Enumeration for restorative consumable types.
/// </summary>
enum class RestoreType : uint8_t { kNone, kHealth, kStamina, kMagicka };

/// <summary>
/// Defines a restorative item.
/// </summary>
struct RestorativeItem 
{
    RE::AlchemyItem* item = nullptr;
    float magnitude = 0.0f;
    float duration = 0.0f;
    bool isFood = false;
    uint32_t count = 0;

    /// <summary>
    /// Checks if the restorative item is instant.
    /// </summary>
    /// <returns>True if instant. False otherwise.</returns>
    bool IsInstant() const { return duration == 0.0f; }

    /// <summary>
    /// Gets the amount that the item restores.
    /// </summary>
    /// <param name="window">Restoration window.</param>
    /// <returns>Amount that the item restores.</returns>
    float EffectiveRestore(float window = 10.0f) const 
    {
        return IsInstant() ? magnitude : magnitude * std::min(duration, window);
    }
};

/// <summary>
/// Holds informationm about all the restorative items an actor owns.
/// </summary>
struct RestoreItems 
{
    std::vector<RestorativeItem> health;
    std::vector<RestorativeItem> stamina;
    std::vector<RestorativeItem> magicka;
    bool initialized = false;

    std::vector<RestorativeItem>& For(RestoreType t) 
    {
        switch (t) 
        {
            case RestoreType::kStamina:
                return stamina;
            case RestoreType::kMagicka:
                return magicka;
            default:
                return health;
        }
    }
};

// The player's resource levels at the moment they decide to consume oen or more restorative items.
struct SelfResourceSample
{
    std::string_view healthLevel = "Unknown";
    std::string_view staminaLevel = "Unknown";
    std::string_view magickaLevel = "Unknown";
    bool hasHealthRestorative = false;
    bool hasStaminaRestorative = false;
    bool hasMagickaRestorative = false;
};

/// <summary>
/// Classifies a restorative item into one of the RestoreType categories.
/// </summary>
/// <param name="item">Item to check.</param>
/// <param name="outMagnitude">Magnitude of effect.</param>
/// <param name="outDuration">Duration of effect.</param>
/// <returns>RestireType category.</returns>
RestoreType ClassifyRestorative(RE::AlchemyItem* item, float& outMagnitude, float& outDuration);

/// <summary>
/// Initializes an actor's owned restorative items.
/// </summary>
/// <param name="actor">Actor to initialize for.</param>
void InitRestoratives(RE::Actor* actor);

/// <summary>
/// Checks if an actor owns any restorative items of a specific category.
/// </summary
/// <param name="actor">Actor to check for.</param>
/// <param name="type">Restorative category to check for.</param>
/// <returns>True if the actor owns at least one item of the desired category. False otherwise.</returns>
bool HasRestorative(RE::Actor* actor, RestoreType type);