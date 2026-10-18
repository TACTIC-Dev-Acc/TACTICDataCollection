#include "Managers/EnemyStrengthManager.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace 
{
    constexpr const char* kEnemyStrengthTiersPath = "Data\\SKSE\\Plugins\\TACTIC\\EnemyStrengthTiers.json";

    std::unordered_map<std::string, std::string> g_strengthTiers;

    // Rank tiers
    constexpr std::string_view kTierOrder[] = {"very low", "low", "medium", "high", "very high"};

    int TierIndex(std::string_view tier)
    {
        for (int i = 0; i < static_cast<int>(std::size(kTierOrder)); ++i) 
        {
            if (kTierOrder[i] == tier) return i;
        }
        return -1;
    }

    // Shift tiers based on player level. 
    // Low-level players find enemies more difficult, while high-level players find them easier.
    int ShiftForSelfLevel(std::string_view selfLevel) 
    {
        if (selfLevel == "very low") return 2;
        if (selfLevel == "low") return 1;
        if (selfLevel == "high") return -1;
        if (selfLevel == "very high") return -1;
        return 0;  // "medium" (and anything unrecognized) — no adjustment
    }
}

void LoadEnemyStrengthTiers() 
{
    std::ifstream file(kEnemyStrengthTiersPath);
    if (!file.is_open()) 
    {
        SKSE::log::warn("[EnemyStrength] could not open {} — EnemyStrengthLevel will always report "
                         "\"Unknown\" until this is fixed",
            kEnemyStrengthTiersPath);
        return;
    }

    nlohmann::json parsed;
    try 
    {
        file >> parsed;
    } 
    catch (const std::exception& e)
    {
        SKSE::log::warn("[EnemyStrength] failed to parse {}: {}", kEnemyStrengthTiersPath, e.what());
        return;
    }

    g_strengthTiers.clear();
    for (auto& [name, tier] : parsed.items()) 
    {
        if (tier.is_string()) g_strengthTiers.emplace(name, tier.get<std::string>());
    }

    SKSE::log::info("[EnemyStrength] loaded {} entries from {}", g_strengthTiers.size(), kEnemyStrengthTiersPath);
}

std::string_view GetEnemyStrengthLevel(RE::Actor* enemyActor, std::string_view selfLevel) 
{
    if (!enemyActor) return "Unknown";

    const auto it = g_strengthTiers.find(std::string(enemyActor->GetName()));
    if (it == g_strengthTiers.end()) return "Unknown";

    const int baseIndex = TierIndex(it->second);
    if (baseIndex < 0) return "Unknown";

    constexpr int kMaxIndex = static_cast<int>(std::size(kTierOrder)) - 1;
    const int shifted = std::clamp(baseIndex + ShiftForSelfLevel(selfLevel), 0, kMaxIndex);
    return kTierOrder[shifted];
}
