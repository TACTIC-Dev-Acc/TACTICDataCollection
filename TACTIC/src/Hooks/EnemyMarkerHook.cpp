#include "Hooks/EnemyMarkerHook.h"

#include "Logging.h"
#include "stuff.h"

/// <summary>
/// Install the hook
/// </summary>
void EnemyMarkerHook::InstallHook()
{
    auto& trampoline = SKSE::GetTrampoline();

    const auto callSite = REL::RelocationID(50718, 51612).address() + REL::Relocate(0x15AB, 0x1735);

    // Logged before and after, so a crash while installing shows which hook it was
    SKSE::log::info("[Hooks] installing EnemyMarkerHook (compass marker) at {}", Logging::GameOffset(callSite));
    _AddMarker = trampoline.write_call<5>(callSite, Thunk);
    SKSE::log::info("[Hooks] EnemyMarkerHook installed");

    RE::ConsoleLog::GetSingleton()->Print("EnemyMarkerHook Installed!");
}

bool EnemyMarkerHook::Thunk
(
    const RE::HUDMarkerManager* a_hudMarkerManager, 
    RE::HUDMarker::ScaleformData* a_markerData,
    RE::NiPoint3* a_pos, 
    const RE::RefHandle& a_refHandle, 
    std::uint32_t a_markerGotoFrame
)
{
    // Add enemy as a red dot in the compass
    const bool added = _AddMarker(a_hudMarkerManager, a_markerData, a_pos, a_refHandle, a_markerGotoFrame);

    if (added)
    {
        // Find the FormID of the actor that this marker corresponds to
        if (auto* marker = RE::TESObjectREFR::LookupByHandle(a_refHandle).get())
        {
            _lastMarker[marker->GetFormID()] = LastMarkerInfo{ QTimer() };
        }
    }

    return added;
}

/// <summary>
/// Check whether given FormID corresponds to an actor that is shown in the compass as a red dot
/// </summary>
/// <param name="a_formID">Actor FormID</param>
/// <returns></returns>
bool EnemyMarkerHook::IsShownAsEnemyMarker(RE::FormID a_formID)
{
    const auto it = _lastMarker.find(a_formID);
    if (it == _lastMarker.end()) return false;

    return (QTimer() - it->second.time) < kFreshnessSeconds;
}

bool EnemyMarkerHook::AnyEnemyMarkerShown()
{
    const float now = QTimer();
    for (const auto& [formID, info] : _lastMarker)
    {
        if ((now - info.time) < kFreshnessSeconds) return true;
    }
    return false;
}
