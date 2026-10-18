#include "Managers/PerkManager.h"

#include <unordered_map>
#include <utility>

namespace {
    // FormIDs for specific perks
    constexpr RE::FormID kFistsOfSteel = 0x058F6E;
    constexpr RE::FormID kElementalProtection = 0x058F69;
    constexpr RE::FormID kBlockRunner = 0x106253;
    constexpr RE::FormID kShieldCharge = 0x058F6A;
    constexpr RE::FormID kDisarmingBash = 0x058F66;
    constexpr RE::FormID kGreatCriticalCharge = 0x0CB407;
    constexpr RE::FormID kCriticalCharge = 0x0CB406;
    constexpr RE::FormID kDualSavagery = 0x106258;
    constexpr RE::FormID kIllusionDualCasting = 0x0153D0;
    constexpr RE::FormID kConjurationDualCasting = 0x0153CE;
    constexpr RE::FormID kMysticBinding = 0x0640B3;
    constexpr RE::FormID kTwinSouls = 0x0D5F1C;
    constexpr RE::FormID kDestructionDualCasting = 0x0153CF;
    constexpr RE::FormID kImpact = 0x0153D2;
    constexpr RE::FormID kRestorationDualCasting = 0x0153D1;
    constexpr RE::FormID kAlterationDualCasting = 0x0153CD;

    RE::BGSPerk* LookupSkyrimPerk(RE::FormID localFormID) 
    {
        static std::unordered_map<RE::FormID, RE::BGSPerk*> cache;

        if (auto it = cache.find(localFormID); it != cache.end()) return it->second;

        auto* perk = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSPerk>(localFormID, "Skyrim.esm");
        cache[localFormID] = perk;
        return perk;
    }

    bool ActorHasPerk(RE::Actor* actor, RE::FormID localFormID) 
    {
        if (not actor) return false;
        auto* perk = LookupSkyrimPerk(localFormID);
        return perk and actor->HasPerk(perk);
    }
}

// Check for specific perks
bool HasFistsOfSteelPerk(RE::Actor* actor) { return ActorHasPerk(actor, kFistsOfSteel); }
bool HasElementalProtectionPerk(RE::Actor* actor) { return ActorHasPerk(actor, kElementalProtection); }
bool HasBlockRunnerPerk(RE::Actor* actor) { return ActorHasPerk(actor, kBlockRunner); }
bool HasShieldChargePerk(RE::Actor* actor) { return ActorHasPerk(actor, kShieldCharge); }
bool HasDisarmingBashPerk(RE::Actor* actor) { return ActorHasPerk(actor, kDisarmingBash); }
bool HasGreatCriticalChargePerk(RE::Actor* actor) { return ActorHasPerk(actor, kGreatCriticalCharge); }
bool HasCriticalChargePerk(RE::Actor* actor) { return ActorHasPerk(actor, kCriticalCharge); }
bool HasDualSavageryPerk(RE::Actor* actor) { return ActorHasPerk(actor, kDualSavagery); }
bool HasIllusionDualCastingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kIllusionDualCasting); }
bool HasConjurationDualCastingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kConjurationDualCasting); }
bool HasMysticBindingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kMysticBinding); }
bool HasTwinSoulsPerk(RE::Actor* actor) { return ActorHasPerk(actor, kTwinSouls); }
bool HasDestructionDualCastingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kDestructionDualCasting); }
bool HasImpactPerk(RE::Actor* actor) { return ActorHasPerk(actor, kImpact); }
bool HasRestorationDualCastingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kRestorationDualCasting); }
bool HasAlterationDualCastingPerk(RE::Actor* actor) { return ActorHasPerk(actor, kAlterationDualCasting); }
