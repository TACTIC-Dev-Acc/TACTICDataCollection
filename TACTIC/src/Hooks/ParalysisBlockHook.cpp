#include "Hooks/ParalysisBlockHook.h"

#include <atomic>
#include <fstream>
#include <mutex>
#include <set>

#include <nlohmann/json.hpp>

#include "Logging.h"
#include "Managers/ModActivationManager.h"

namespace {
    // Every blocked application is counted, but only the first one of each distinct effect is written to the log, so a spell that fires every frame can't flood it
    std::atomic<std::uint64_t> g_blockedTotal{ 0 };
    std::mutex g_loggedEffectsMutex;
    std::set<RE::FormID> g_loggedEffects;
}

bool ParalysisBlockHook::LoadAllowlist(const std::string& path)
{
    _allowedEffects.clear();

    std::ifstream file(path);
    if (!file.is_open())
    {
        SKSE::log::warn("[Paralysis] {} not found, every Paralysis effect will be blocked", path);
        return false;
    }

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (not dataHandler)
    {
        SKSE::log::error("[Paralysis] game data isn't available yet, the allowlist wasn't loaded and every Paralysis effect will be blocked");
        return false;
    }

    nlohmann::json json;
    try
    {
        file >> json;
    }
    catch (const nlohmann::json::exception& e)
    {
        SKSE::log::warn("[Paralysis] couldn't read {}, every Paralysis effect will be blocked: {}", path, e.what());
        return false;
    }

    int notFound = 0;
    int malformed = 0;

    // Keys are "Plugin.esm|0xLocalFormID", values are just a description of the effect
    for (const auto& [key, description] : json.items())
    {
        const auto separator = key.find('|');
        if (separator == std::string::npos)
        {
            SKSE::log::warn("[Paralysis] allowlist entry '{}' isn't \"Plugin.esm|0xFormID\", ignoring it", key);
            malformed++;
            continue;
        }

        RE::FormID localFormID = 0;
        try
        {
            localFormID = static_cast<RE::FormID>(std::stoul(key.substr(separator + 1), nullptr, 16));
        }
        catch (const std::exception&)
        {
            SKSE::log::warn("[Paralysis] allowlist entry '{}' has an invalid FormID, ignoring it", key);
            malformed++;
            continue;
        }

        // A plugin that isn't loaded just has nothing to allow
        const RE::FormID formID = dataHandler->LookupFormID(localFormID, key.substr(0, separator));
        if (formID) _allowedEffects.insert(formID);
        else notFound++;
    }

    SKSE::log::info("[Paralysis] allowlist loaded: {} effect(s) allowed, {} entr{} not in the load order, {} malformed",
        _allowedEffects.size(), notFound, notFound == 1 ? "y" : "ies", malformed);
    return true;
}

void ParalysisBlockHook::Install()
{
    // MagicTarget is the 5th base class of Actor (after TESForm, BSHandleRefObject, BSTEventSink<BSAnimationGraphEvent>, IAnimationGraphManagerHolder)
    // AddTarget is its vfunc 0x01
    REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[4] };
    REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE_PlayerCharacter[4] };

    SKSE::log::info("[Hooks] installing ParalysisBlockHook on MagicTarget::AddTarget (Character vtable {}, PlayerCharacter vtable {})",
        Logging::GameOffset(characterVtbl.address()), Logging::GameOffset(playerVtbl.address()));

    _characterAddTarget = characterVtbl.write_vfunc(0x1, CharacterAddTarget);
    _playerAddTarget = playerVtbl.write_vfunc(0x1, PlayerAddTarget);

    SKSE::log::info("[Hooks] ParalysisBlockHook installed");
    RE::ConsoleLog::GetSingleton()->Print("ParalysisBlockHook Installed!");
}

bool ParalysisBlockHook::ShouldBlock(RE::MagicTarget* a_this, const RE::MagicTarget::AddTargetData& a_data)
{
    // Nothing is blocked while the mod is dormant
    if (not IsModActive()) return false;

    const RE::EffectSetting* baseEffect = a_data.effect ? a_data.effect->baseEffect : nullptr;
    if (not baseEffect or not baseEffect->HasArchetype(RE::EffectSetting::Archetype::kParalysis)) return false;

    if (_allowedEffects.contains(baseEffect->GetFormID())) return false;

    const auto total = ++g_blockedTotal;

    {
        std::lock_guard lock(g_loggedEffectsMutex);
        if (not g_loggedEffects.insert(baseEffect->GetFormID()).second) return true;
    }

    // The first time each effect is blocked. If a quest or scripted scene stops working, this line names the effect to add to the allowlist
    const auto* file = baseEffect->GetFile(0);
    const auto* target = a_this->GetTargetStatsObject();
    SKSE::log::info("[Paralysis] blocked effect {:06X} from {} ('{}', via '{}') cast by '{}' on '{}' (block #{} this session)",
        baseEffect->GetLocalFormID(), file ? file->GetFilename() : "?", baseEffect->GetName(),
        a_data.magicItem ? a_data.magicItem->GetName() : "?",
        a_data.caster ? a_data.caster->GetName() : "?", target ? target->GetName() : "?", total);

    return true;
}

std::uint64_t ParalysisBlockHook::GetBlockedCount()
{
    return g_blockedTotal;
}

bool ParalysisBlockHook::CharacterAddTarget(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data)
{
    if (ShouldBlock(a_this, a_data)) return false;
    return _characterAddTarget(a_this, a_data);
}

bool ParalysisBlockHook::PlayerAddTarget(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data)
{
    if (ShouldBlock(a_this, a_data)) return false;
    return _playerAddTarget(a_this, a_data);
}
