#pragma once

#include "RE/H/HUDMarkerManager.h"
#include "PCH.h"

#include "REL/ID.h"

#include <unordered_map>

class EnemyMarkerHook
{
public:
    /// <summary>
    /// Installs hook.
    /// </summary>
    static void InstallHook();

    /// <summary>
    /// Checks if an enemy is shown as a red marker in the compass
    /// </summary>
    static bool IsShownAsEnemyMarker(RE::FormID a_formID);

    /// <summary>
    /// Whether the compass is showing ANY red enemy dot right now.
    /// </summary>
    static bool AnyEnemyMarkerShown();

private:
    static bool Thunk(
        const RE::HUDMarkerManager* a_hudMarkerManager, 
        RE::HUDMarker::ScaleformData* a_markerData,
        RE::NiPoint3* a_pos, 
        const RE::RefHandle& a_refHandle, 
        std::uint32_t a_markerGotoFrame
    );

    static inline REL::Relocation<decltype(Thunk)> _AddMarker;

    static constexpr float kFreshnessSeconds = 0.25f;

    struct LastMarkerInfo
    {
        float time = 0.0f;
    };

    static inline std::unordered_map<RE::FormID, LastMarkerInfo> _lastMarker;
};
