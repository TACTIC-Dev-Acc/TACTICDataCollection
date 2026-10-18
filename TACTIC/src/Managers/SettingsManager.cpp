#include "Managers/SettingsManager.h"

#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>

#include "Managers/ShoutSuppressionManager.h"

namespace {
    constexpr bool kDefaultScrollsEnabled = false;

    bool g_scrollsEnabled = kDefaultScrollsEnabled;

    // The value under `key` as a T, or nothing if it's absent or has the wrong type (a typo in a hand-edited file shouldn't discard the other settings)
    template <class T>
    std::optional<T> ReadSetting(const nlohmann::json& json, const char* key)
    {
        if (not json.contains(key)) return std::nullopt;

        try
        {
            return json[key].get<T>();
        }
        catch (const nlohmann::json::exception& e)
        {
            SKSE::log::warn("[Settings] ignoring '{}': {}", key, e.what());
            return std::nullopt;
        }
    }
}

bool AreScrollsEnabled()
{
    return g_scrollsEnabled;
}

void LoadTacticSettings(const std::string& path)
{
    g_scrollsEnabled = kDefaultScrollsEnabled;

    std::ifstream file(path);
    if (not file.is_open())
    {
        SKSE::log::info("[Settings] {} not found, using the defaults", path);
        return;
    }

    nlohmann::json json;
    try
    {
        file >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        SKSE::log::warn("[Settings] couldn't read {}, using the defaults: {}", path, e.what());
        return;
    }

    ConfigureShouts(
        ReadSetting<std::uint32_t>(json, "shout_toggle_key"),
        ReadSetting<float>(json, "shout_enabled_window_seconds"));

    if (auto scrollsEnabled = ReadSetting<bool>(json, "scrolls_enabled")) g_scrollsEnabled = *scrollsEnabled;
    SKSE::log::info("[Settings] scrolls_enabled={}", g_scrollsEnabled);
}
