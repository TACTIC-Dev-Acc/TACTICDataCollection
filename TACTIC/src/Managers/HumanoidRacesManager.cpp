#include "Managers/HumanoidRacesManager.h"

#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

static std::unordered_set<std::string> g_humanoidRaces;

bool LoadHumanoidRaces(const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open())
    {
        // Without this list every enemy counts as a creature, and no fight is ever collected
        SKSE::log::error("[Races] couldn't open {}, no fight will be collected until it's there", path);
        return false;
    }

    std::unordered_set<std::string> races;
    try
    {
        nlohmann::json json;
        file >> json;

        for (const auto& [raceName, allowed] : json.items())
        {
            if (allowed.get<bool>()) races.insert(raceName);
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        SKSE::log::error("[Races] couldn't read {}, no fight will be collected until it's fixed: {}", path, e.what());
        return false;
    }

    g_humanoidRaces = std::move(races);
    SKSE::log::info("[Races] loaded {} humanoid race(s) from {}", g_humanoidRaces.size(), path);
    return true;
}

bool IsHumanoidRace(const std::string& raceEditorID)
{
    return g_humanoidRaces.contains(raceEditorID);
}
