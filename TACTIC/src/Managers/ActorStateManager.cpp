#include "Managers/ActorStateManager.h"

#undef GetObject

// Most NPCs can't transform - do not collect data when the player is transformed
bool IsTransformed(RE::Actor* actor)
{
    if (!actor) return false;

    auto* defaultObjects = RE::BGSDefaultObjectManager::GetSingleton();
    if (!defaultObjects) return false;

    RE::TESRace* actorRace = actor->GetRace();
    if (!actorRace) return false;

    static RE::TESRace* werewolfRace = defaultObjects->GetObject<RE::TESRace>(RE::DEFAULT_OBJECT::kWerewolfRace);
    static RE::TESRace* vampireLordRace = defaultObjects->GetObject<RE::TESRace>(RE::DEFAULT_OBJECT::kVampireRace);

    return actorRace == werewolfRace or actorRace == vampireLordRace;
}

// Combines different phases to broader categories to easier training
AttackPhase GetAttackPhase(RE::Actor* actor)
{
    if (!actor) return AttackPhase::kNone;

    using S = RE::ATTACK_STATE_ENUM;
    switch (actor->AsActorState()->GetAttackState()) 
    {
        // Windup
        case S::kDraw:
        case S::kBowDraw:
        case S::kBowAttached:
        case S::kBowDrawn:
            return AttackPhase::kWindup;

        // Release
        case S::kSwing:
        case S::kBowReleasing:
        case S::kBowReleased:
        case S::kFire:
        case S::kFiring:
            return AttackPhase::kRelease;

        // Recovery
        case S::kHit:
        case S::kNextAttack:
        case S::kFollowThrough:
        case S::kBowNextAttack:
        case S::kBowFollowThrough:
        case S::kFired:
            return AttackPhase::kRecovery;

        default:
            return AttackPhase::kNone;
    }
}

std::string_view AttackPhaseToString(AttackPhase phase)
{
    switch (phase)
    {
        case AttackPhase::kWindup: return "Windup";
        case AttackPhase::kRelease: return "Release";
        case AttackPhase::kRecovery: return "Recovery";
        default: return "None";
    }
}

bool IsAimingRangedWeapon(RE::Actor* actor)
{
    if (!actor) return false;

    using S = RE::ATTACK_STATE_ENUM;
    switch (actor->AsActorState()->GetAttackState())
    {
        case S::kBowDraw:
        case S::kBowAttached:
        case S::kBowDrawn:
            return true;

        default:
            return false;
    }
}

bool HasFleshSpellActive(RE::Actor* actor)
{
    if (!actor) return false;

    static RE::BGSKeyword* fleshSpellKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicArmorSpell");
    if (!fleshSpellKeyword) return false;

    return actor->HasMagicEffectWithKeyword(fleshSpellKeyword);
}

bool IsActorCasting(RE::Actor* actor)
{
    if (!actor) return false;

    using Source = RE::MagicSystem::CastingSource;
    for (auto src : {Source::kLeftHand, Source::kRightHand, Source::kInstant, Source::kOther}) 
    {
        if (auto* caster = actor->GetMagicCaster(src)) 
        {
            const auto state = caster->state;
            if (state != RE::MagicCaster::State::kNone) return true;
        }
    }
    return false;
}